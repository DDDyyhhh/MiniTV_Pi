#include "pc_monitor.h"

#include <ctype.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "app_model.h"
#include "board_pins.h"
#include "config_store.h"
#include "ui_fonts.h"
#include "ui_runtime.h"

#define PC_HTTP_RESPONSE_BYTES 1536U
#define PC_VISIBLE_REFRESH_US (10LL * 1000LL * 1000LL)
#define PC_HIDDEN_REFRESH_US (60LL * 1000LL * 1000LL)
#define PC_ACTION_CONFIRM_US (3LL * 1000LL * 1000LL)
#define PC_ACTION_RESULT_US (2LL * 1000LL * 1000LL)
#define PC_STALE_AFTER_SECONDS 30

#define COLOR_BG lv_color_hex(0x121214)
#define COLOR_PANEL lv_color_hex(0x1C1C1E)
#define COLOR_BORDER lv_color_hex(0x3A3A3C)
#define COLOR_PRIMARY lv_color_hex(0xF5F5F7)
#define COLOR_SECONDARY lv_color_hex(0xA1A1A6)
#define COLOR_ACCENT lv_color_hex(0x0A84FF)
#define COLOR_SUCCESS lv_color_hex(0x30D158)
#define COLOR_ERROR lv_color_hex(0xFF453A)
#define COLOR_OFFLINE lv_color_hex(0x8E8E93)

static const char *TAG = "pc_monitor";

static const char *const PC_AGENT_ONLINE_ENTITY = "binary_sensor.pc_agent_online";
static const char *const PC_TELEMETRY_UPDATED_ENTITY = "sensor.pc_telemetry_updated";
static const char *const PC_CPU_ENTITY = "sensor.pc_cpu_percent";
static const char *const PC_CPU_TEMP_ENTITY = "sensor.pc_cpu_temp_c";
static const char *const PC_RAM_PERCENT_ENTITY = "sensor.pc_ram_percent";
static const char *const PC_RAM_USED_ENTITY = "sensor.pc_ram_used_gib";
static const char *const PC_GPU_UTIL_ENTITY = "sensor.pc_gpu_util_percent";
static const char *const PC_GPU_TEMP_ENTITY = "sensor.pc_gpu_temp_c";
static const char *const PC_GPU_VRAM_USED_ENTITY = "sensor.pc_gpu_vram_used_mib";
static const char *const PC_GPU_NAME_ENTITY = "sensor.pc_gpu_name";
static const char *const PC_UI_ACTION_STATUS_ENTITY = "sensor.pc_ui_action_status";

typedef enum {
    PC_ACTION_IDLE,
    PC_ACTION_PENDING,
    PC_ACTION_LAUNCHED,
    PC_ACTION_FAILED,
} pc_action_state_t;

typedef struct {
    char data[PC_HTTP_RESPONSE_BYTES];
    size_t length;
    bool overflowed;
} http_response_t;

typedef struct {
    bool agent_known;
    bool agent_online;
    bool telemetry_known;
    bool telemetry_stale;
    bool cpu_available;
    bool cpu_temp_available;
    bool ram_available;
    bool ram_percent_available;
    bool gpu_available;
    bool gpu_temp_available;
    bool gpu_vram_available;
    float cpu_percent;
    float cpu_temp_c;
    float ram_used_gib;
    float ram_percent;
    float ram_total_gib;
    float gpu_percent;
    float gpu_temp_c;
    float gpu_vram_used_mib;
    float gpu_vram_total_mib;
    bool gpu_name_available;
    char gpu_name[48];
    time_t telemetry_updated_at;
    pc_launch_action_t action;
    pc_action_state_t action_state;
    bool action_sent;
    char request_id[37];
    int64_t action_deadline_us;
    int64_t action_result_until_us;
} pc_monitor_state_t;

static SemaphoreHandle_t s_lock;
static TaskHandle_t s_task;
static pc_monitor_state_t s_state;
static volatile bool s_visible;
static int64_t s_last_poll_us;
static lv_obj_t *s_status;
static lv_obj_t *s_agent_dot;
static lv_obj_t *s_cpu_arc;
static lv_obj_t *s_gpu_arc;
static lv_obj_t *s_cpu_value;
static lv_obj_t *s_gpu_value;
static lv_obj_t *s_cpu_detail;
static lv_obj_t *s_gpu_detail;
static lv_obj_t *s_gpu_title;
static lv_obj_t *s_action_buttons[3];

static void set_label_text_if_changed(lv_obj_t *label, const char *text)
{
    if ((label != NULL) && (text != NULL) && (strcmp(lv_label_get_text(label), text) != 0)) {
        lv_label_set_text(label, text);
    }
}

static void style_panel(lv_obj_t *object)
{
    lv_obj_set_style_bg_color(object, COLOR_PANEL, 0);
    lv_obj_set_style_border_color(object, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(object, 1, 0);
    lv_obj_set_style_radius(object, 14, 0);
    lv_obj_set_style_pad_all(object, 8, 0);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
}

static void snapshot_get(pc_monitor_state_t *out_state)
{
    if ((out_state == NULL) || (s_lock == NULL)) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out_state = s_state;
    xSemaphoreGive(s_lock);
}

static void snapshot_set(const pc_monitor_state_t *state)
{
    if ((state == NULL) || (s_lock == NULL)) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_state = *state;
    xSemaphoreGive(s_lock);
    ui_runtime_request_refresh();
}

static esp_err_t http_response_handler(esp_http_client_event_t *event)
{
    if ((event->event_id != HTTP_EVENT_ON_DATA) || (event->user_data == NULL)) {
        return ESP_OK;
    }
    http_response_t *response = event->user_data;
    if ((event->data_len < 0) || ((size_t)event->data_len > (sizeof(response->data) - 1U - response->length))) {
        response->overflowed = true;
        return ESP_FAIL;
    }
    memcpy(response->data + response->length, event->data, (size_t)event->data_len);
    response->length += (size_t)event->data_len;
    response->data[response->length] = '\0';
    return ESP_OK;
}

static bool ha_request(const app_config_t *config, esp_http_client_method_t method, const char *path, const char *body,
                       http_response_t *response)
{
    if ((config == NULL) || (path == NULL) || (response == NULL) || (config->ha_endpoint[0] == '\0') ||
        (config->ha_token[0] == '\0')) {
        return false;
    }
    char url[CONFIG_ENDPOINT_MAX_LEN + 96];
    const int url_length = snprintf(url, sizeof(url), "%s%s", config->ha_endpoint, path);
    if ((url_length < 0) || ((size_t)url_length >= sizeof(url))) {
        return false;
    }
    char authorization[CONFIG_TOKEN_MAX_LEN + 8] = "Bearer ";
    strncat(authorization, config->ha_token, sizeof(authorization) - strlen(authorization) - 1U);
    *response = (http_response_t){0};
    const esp_http_client_config_t client_config = {
        .url = url,
        .method = method,
        .timeout_ms = 5000,
        .buffer_size = 1024,
        .buffer_size_tx = 1024,
        .event_handler = http_response_handler,
        .user_data = response,
    };
    esp_http_client_handle_t client = esp_http_client_init(&client_config);
    if (client == NULL) {
        return false;
    }
    (void)esp_http_client_set_header(client, "Authorization", authorization);
    (void)esp_http_client_set_header(client, "Accept", "application/json");
    if (body != NULL) {
        (void)esp_http_client_set_header(client, "Content-Type", "application/json");
        (void)esp_http_client_set_post_field(client, body, (int)strlen(body));
    }
    const esp_err_t result = esp_http_client_perform(client);
    const int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    return (result == ESP_OK) && !response->overflowed && (status >= 200) && (status < 300);
}

static bool ha_get_state(const app_config_t *config, const char *entity_id, http_response_t *response)
{
    char path[112];
    const int path_length = snprintf(path, sizeof(path), "/api/states/%s", entity_id);
    return (path_length > 0) && ((size_t)path_length < sizeof(path)) && ha_request(config, HTTP_METHOD_GET, path, NULL, response);
}

static const char *skip_json_space(const char *text)
{
    while ((text != NULL) && isspace((unsigned char)*text)) {
        text++;
    }
    return text;
}

static bool json_string_value(const char *json, const char *key, char *out_value, size_t out_size)
{
    if ((json == NULL) || (key == NULL) || (out_value == NULL) || (out_size < 2U)) {
        return false;
    }
    const char *at = strstr(json, key);
    if (at == NULL) {
        return false;
    }
    at = strchr(at + strlen(key), ':');
    if (at == NULL) {
        return false;
    }
    at = skip_json_space(at + 1);
    if (*at != '\"') {
        return false;
    }
    at++;
    size_t written = 0;
    while ((*at != '\0') && (*at != '\"')) {
        if ((*at == '\\') || (written + 1U >= out_size)) {
            return false;
        }
        out_value[written++] = *at++;
    }
    if (*at != '\"') {
        return false;
    }
    out_value[written] = '\0';
    return true;
}

static bool json_number_value(const char *json, const char *key, float *out_value)
{
    char value[32];
    if (!json_string_value(json, key, value, sizeof(value)) || (strcmp(value, "unknown") == 0) ||
        (strcmp(value, "unavailable") == 0)) {
        return false;
    }
    char *end = NULL;
    const float parsed = strtof(value, &end);
    if ((end == value) || (*end != '\0') || !isfinite(parsed)) {
        return false;
    }
    *out_value = parsed;
    return true;
}

static bool json_attribute_number(const char *json, const char *key, float *out_value)
{
    if ((json == NULL) || (key == NULL) || (out_value == NULL)) {
        return false;
    }
    const char *at = strstr(json, key);
    if (at == NULL) {
        return false;
    }
    at = strchr(at + strlen(key), ':');
    if (at == NULL) {
        return false;
    }
    at = skip_json_space(at + 1);
    char *end = NULL;
    const float parsed = strtof(at, &end);
    if ((end == at) || !isfinite(parsed)) {
        return false;
    }
    *out_value = parsed;
    return true;
}

static bool parse_iso8601_utc(const char *text, time_t *out_time)
{
    if ((text == NULL) || (out_time == NULL) || (strlen(text) < 19U)) {
        return false;
    }
    struct tm parsed = {0};
    if (sscanf(text, "%4d-%2d-%2dT%2d:%2d:%2d", &parsed.tm_year, &parsed.tm_mon, &parsed.tm_mday, &parsed.tm_hour,
               &parsed.tm_min, &parsed.tm_sec) != 6) {
        return false;
    }
    parsed.tm_year -= 1900;
    parsed.tm_mon -= 1;
    time_t timestamp = timegm(&parsed);
    const char *zone = text + 19;
    while ((*zone != '\0') && (*zone != 'Z') && (*zone != '+') && (*zone != '-')) {
        zone++;
    }
    if ((*zone == '+') || (*zone == '-')) {
        int hours = 0;
        int minutes = 0;
        if (sscanf(zone + 1, "%2d:%2d", &hours, &minutes) != 2) {
            return false;
        }
        const int offset_seconds = hours * 3600 + minutes * 60;
        timestamp += *zone == '+' ? -offset_seconds : offset_seconds;
    } else if ((*zone != 'Z') && (*zone != '\0')) {
        return false;
    }
    *out_time = timestamp;
    return timestamp > 0;
}

static bool time_is_stale(time_t updated_at)
{
    if (updated_at <= 0) {
        return true;
    }
    const time_t now = time(NULL);
    return (now > 1704067200) && (now > updated_at) && ((now - updated_at) > PC_STALE_AFTER_SECONDS);
}

static bool fetch_metric(const app_config_t *config, const char *entity_id, float *value, bool *available)
{
    http_response_t response;
    if (!ha_get_state(config, entity_id, &response)) {
        return false;
    }
    *available = json_number_value(response.data, "\"state\"", value);
    return true;
}
static bool fetch_text(const app_config_t *config, const char *entity_id, char *value, size_t value_size, bool *available)
{
    http_response_t response;
    if (!ha_get_state(config, entity_id, &response)) {
        return false;
    }
    *available = json_string_value(response.data, "\"state\"", value, value_size) && (strcmp(value, "unknown") != 0);
    return true;
}


static bool fetch_metric_with_total(const app_config_t *config, const char *entity_id, const char *total_key, float *value,
                                    float *total, bool *available)
{
    http_response_t response;
    if (!ha_get_state(config, entity_id, &response)) {
        return false;
    }
    *available = json_number_value(response.data, "\"state\"", value) && json_attribute_number(response.data, total_key, total);
    return true;
}

static void refresh_telemetry(const app_config_t *config)
{
    pc_monitor_state_t next;
    snapshot_get(&next);
    http_response_t response;
    char state[48];
    if (!ha_get_state(config, PC_AGENT_ONLINE_ENTITY, &response) ||
        !json_string_value(response.data, "\"state\"", state, sizeof(state))) {
        next.agent_known = false;
        next.telemetry_stale = next.telemetry_known;
        snapshot_set(&next);
        return;
    }
    next.agent_known = true;
    next.agent_online = strcmp(state, "on") == 0;
    if (!next.agent_online) {
        next.telemetry_stale = next.telemetry_known;
        snapshot_set(&next);
        return;
    }
    if (ha_get_state(config, PC_TELEMETRY_UPDATED_ENTITY, &response) &&
        json_string_value(response.data, "\"state\"", state, sizeof(state))) {
        time_t updated_at = 0;
        if (parse_iso8601_utc(state, &updated_at)) {
            next.telemetry_known = true;
            next.telemetry_updated_at = updated_at;
            next.telemetry_stale = time_is_stale(updated_at);
        } else {
            next.telemetry_known = false;
            next.telemetry_stale = true;
        }
    } else {
        next.telemetry_stale = next.telemetry_known;
    }
    if (!s_visible) {
        snapshot_set(&next);
        return;
    }

    float value = 0;
    if (fetch_metric(config, PC_CPU_ENTITY, &value, &next.cpu_available) && next.cpu_available && (value >= 0.0F) && (value <= 100.0F)) {
        next.cpu_percent = value;
    } else {
        next.cpu_available = false;
    }
    if (fetch_metric(config, PC_CPU_TEMP_ENTITY, &value, &next.cpu_temp_available) && next.cpu_temp_available && (value >= 0.0F)) {
        next.cpu_temp_c = value;
    } else {
        next.cpu_temp_available = false;
    }
    if (fetch_metric(config, PC_RAM_PERCENT_ENTITY, &value, &next.ram_percent_available) && next.ram_percent_available &&
        (value >= 0.0F) && (value <= 100.0F)) {
        next.ram_percent = value;
    } else {
        next.ram_percent_available = false;
    }
    if (fetch_metric_with_total(config, PC_RAM_USED_ENTITY, "\"total_gib\"", &value, &next.ram_total_gib, &next.ram_available) &&
        next.ram_available && (value >= 0.0F) && (next.ram_total_gib > 0.0F)) {
        next.ram_used_gib = value;
    } else {
        next.ram_available = false;
    }
    if (fetch_metric(config, PC_GPU_UTIL_ENTITY, &value, &next.gpu_available) && next.gpu_available && (value >= 0.0F) && (value <= 100.0F)) {
        next.gpu_percent = value;
    } else {
        next.gpu_available = false;
    }
    if (fetch_metric(config, PC_GPU_TEMP_ENTITY, &value, &next.gpu_temp_available) && next.gpu_temp_available && (value >= 0.0F)) {
        next.gpu_temp_c = value;
    } else {
        next.gpu_temp_available = false;
    }
    if (fetch_metric_with_total(config, PC_GPU_VRAM_USED_ENTITY, "\"total_mib\"", &value, &next.gpu_vram_total_mib,
                                &next.gpu_vram_available) &&
        next.gpu_vram_available && (value >= 0.0F) && (next.gpu_vram_total_mib > 0.0F)) {
        next.gpu_vram_used_mib = value;
    } else {
        next.gpu_vram_available = false;
    }
    if (!fetch_text(config, PC_GPU_NAME_ENTITY, next.gpu_name, sizeof(next.gpu_name), &next.gpu_name_available) ||
        !next.gpu_name_available) {
        next.gpu_name_available = false;
        next.gpu_name[0] = '\0';
    }
    snapshot_set(&next);
}

static const char *script_for_action(pc_launch_action_t action)
{
    switch (action) {
    case PC_LAUNCH_ACTION_VSCODE: return "script.pc_open_vscode";
    case PC_LAUNCH_ACTION_BILIBILI: return "script.pc_open_bilibili";
    case PC_LAUNCH_ACTION_DOUYIN: return "script.pc_open_douyin";
    default: return NULL;
    }
}

static const char *action_id_for_action(pc_launch_action_t action)
{
    switch (action) {
    case PC_LAUNCH_ACTION_VSCODE: return "open_vscode";
    case PC_LAUNCH_ACTION_BILIBILI: return "open_bilibili";
    case PC_LAUNCH_ACTION_DOUYIN: return "open_douyin";
    default: return NULL;
    }
}

static bool send_launch_action(const app_config_t *config, const pc_monitor_state_t *state)
{
    const char *script = script_for_action(state->action);
    if (script == NULL) {
        return false;
    }
    char request[128];
    const int request_length = snprintf(request, sizeof(request), "{\"entity_id\":\"%s\",\"variables\":{\"request_id\":\"%s\"}}", script,
                                        state->request_id);
    if ((request_length < 0) || ((size_t)request_length >= sizeof(request))) {
        return false;
    }
    http_response_t response;
    return ha_request(config, HTTP_METHOD_POST, "/api/services/script/turn_on", request, &response);
}

static void set_action_result(pc_monitor_state_t *state, pc_action_state_t result)
{
    state->action_state = result;
    state->action_sent = false;
    state->action_result_until_us = esp_timer_get_time() + PC_ACTION_RESULT_US;
    if (result == PC_ACTION_LAUNCHED) {
        ESP_LOGI(TAG, "Launch Action confirmed: %s", action_id_for_action(state->action));
    } else {
        ESP_LOGW(TAG, "Launch Action failed: %s", action_id_for_action(state->action));
    }
}

static void refresh_pending_action(const app_config_t *config)
{
    pc_monitor_state_t next;
    snapshot_get(&next);
    const int64_t now = esp_timer_get_time();
    if (next.action_state == PC_ACTION_PENDING) {
        if (!next.action_sent) {
            if (send_launch_action(config, &next)) {
                next.action_sent = true;
            } else {
                set_action_result(&next, PC_ACTION_FAILED);
            }
        } else {
            http_response_t response;
            char status[32];
            char request_id[40];
            if (ha_get_state(config, PC_UI_ACTION_STATUS_ENTITY, &response) &&
                json_string_value(response.data, "\"state\"", status, sizeof(status)) &&
                json_string_value(response.data, "\"request_id\"", request_id, sizeof(request_id)) &&
                strcmp(request_id, next.request_id) == 0) {
                if (strcmp(status, "launched") == 0) {
                    set_action_result(&next, PC_ACTION_LAUNCHED);
                } else if ((strcmp(status, "failed") == 0) || (strcmp(status, "rejected") == 0)) {
                    set_action_result(&next, PC_ACTION_FAILED);
                }
            }
            if ((next.action_state == PC_ACTION_PENDING) && (now >= next.action_deadline_us)) {
                set_action_result(&next, PC_ACTION_FAILED);
            }
        }
        snapshot_set(&next);
    } else if ((next.action_state != PC_ACTION_IDLE) && (now >= next.action_result_until_us)) {
        next.action_state = PC_ACTION_IDLE;
        snapshot_set(&next);
    }
}

static bool backend_allows_pc_actions(void)
{
    app_model_t model;
    app_model_get(&model);
    return model.backend_state == APP_BACKEND_ONLINE;
}

static void pc_monitor_task(void *argument)
{
    (void)argument;
    bool refresh_requested = true;
    while (true) {
        app_config_t config = {0};
        const bool configured = config_store_load(&config) && (config.ha_endpoint[0] != '\0') && (config.ha_token[0] != '\0');
        pc_monitor_state_t current;
        snapshot_get(&current);
        const int64_t now = esp_timer_get_time();
        if ((current.action_state == PC_ACTION_PENDING) && configured) {
            refresh_pending_action(&config);
        } else if ((current.action_state == PC_ACTION_PENDING) && !configured) {
            set_action_result(&current, PC_ACTION_FAILED);
            snapshot_set(&current);
        } else if (current.action_state != PC_ACTION_IDLE) {
            refresh_pending_action(&config);
        }

        const int64_t refresh_interval = s_visible ? PC_VISIBLE_REFRESH_US : PC_HIDDEN_REFRESH_US;
        if (configured && (refresh_requested || ((now - s_last_poll_us) >= refresh_interval))) {
            refresh_requested = false;
            s_last_poll_us = now;
            refresh_telemetry(&config);
        }

        current = (pc_monitor_state_t){0};
        snapshot_get(&current);
        const TickType_t wait = current.action_state == PC_ACTION_PENDING ? pdMS_TO_TICKS(300) : pdMS_TO_TICKS(1000);
        refresh_requested = ulTaskNotifyTake(pdTRUE, wait) > 0U;
    }
}

static void animate_arc_value(void *object, int32_t value)
{
    lv_arc_set_value(object, value);
}

static void set_arc_value(lv_obj_t *arc, uint8_t value)
{
    if (arc == NULL) {
        return;
    }
    lv_anim_del(arc, animate_arc_value);
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, arc);
    lv_anim_set_values(&animation, lv_arc_get_value(arc), value);
    lv_anim_set_time(&animation, 180);
    lv_anim_set_exec_cb(&animation, animate_arc_value);
    lv_anim_start(&animation);
}

static lv_obj_t *create_gauge(lv_obj_t *parent, const char *title, lv_obj_t **out_value, lv_obj_t **out_detail,
                              lv_obj_t **out_title)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, 144, 124);
    style_panel(panel);

    lv_obj_t *title_label = lv_label_create(panel);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_color(title_label, COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(title_label, 128);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 0);
    if (out_title != NULL) {
        *out_title = title_label;
    }

    lv_obj_t *arc = lv_arc_create(panel);
    lv_obj_set_size(arc, 70, 70);
    lv_obj_align(arc, LV_ALIGN_TOP_MID, 0, 20);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_value(arc, 0);
    lv_obj_set_style_arc_width(arc, 7, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 7, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);

    *out_value = lv_label_create(arc);
    lv_label_set_text(*out_value, "--");
    lv_obj_set_style_text_color(*out_value, COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(*out_value, &lv_font_montserrat_20, 0);
    lv_obj_center(*out_value);

    *out_detail = lv_label_create(panel);
    lv_label_set_text(*out_detail, "--");
    lv_obj_set_style_text_color(*out_detail, COLOR_SECONDARY, 0);
    lv_obj_set_style_text_font(*out_detail, &lv_font_montserrat_12, 0);
    lv_label_set_long_mode(*out_detail, LV_LABEL_LONG_DOT);
    lv_obj_set_width(*out_detail, 128);
    lv_obj_align(*out_detail, LV_ALIGN_BOTTOM_MID, 0, 0);
    return panel;
}

static void action_button_event(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        const pc_launch_action_t action = (pc_launch_action_t)(uintptr_t)lv_event_get_user_data(event);
        pc_monitor_request_action(action);
    }
}

static lv_obj_t *create_action_button(lv_obj_t *parent, const char *label, pc_launch_action_t action)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_set_size(button, 94, 48);
    lv_obj_set_style_bg_color(button, COLOR_PANEL, 0);
    lv_obj_set_style_border_color(button, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_radius(button, 12, 0);
    lv_obj_add_event_cb(button, action_button_event, LV_EVENT_ALL, (void *)(uintptr_t)action);
    lv_obj_t *text = lv_label_create(button);
    lv_label_set_text(text, label);
    lv_obj_set_style_text_color(text, COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(text, &lv_font_montserrat_12, 0);
    lv_obj_center(text);
    return button;
}

static void update_action_buttons(const pc_monitor_state_t *state, bool actions_allowed)
{
    for (uint8_t index = 0; index < 3U; index++) {
        lv_obj_t *button = s_action_buttons[index];
        if (button == NULL) {
            continue;
        }
        const bool selected = state->action == (pc_launch_action_t)index;
        lv_color_t color = COLOR_PANEL;
        bool disabled = !actions_allowed || (state->action_state == PC_ACTION_PENDING);
        if (selected && (state->action_state == PC_ACTION_PENDING)) {
            color = COLOR_ACCENT;
        } else if (selected && (state->action_state == PC_ACTION_LAUNCHED)) {
            color = COLOR_SUCCESS;
        } else if (selected && (state->action_state == PC_ACTION_FAILED)) {
            color = COLOR_ERROR;
        }
        lv_obj_set_style_bg_color(button, color, 0);
        if (disabled) {
            lv_obj_add_state(button, LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(button, LV_STATE_DISABLED);
        }
    }
}

static void update_ui(const pc_monitor_state_t *state)
{
    app_model_t model;
    app_model_get(&model);
    const bool backend_online = model.backend_state == APP_BACKEND_ONLINE;
    const bool actions_allowed = backend_online && state->agent_known && state->agent_online;
    const bool dim = !backend_online || !state->agent_known || !state->agent_online || state->telemetry_stale;
    const lv_opa_t metric_opa = dim ? LV_OPA_50 : LV_OPA_COVER;

    const char *status = "Backend unavailable";
    lv_color_t dot_color = COLOR_OFFLINE;
    if (model.backend_state == APP_BACKEND_AUTH_FAILED) {
        status = "Backend authentication failed";
    } else if (model.backend_state == APP_BACKEND_UNCONFIGURED) {
        status = "HA not configured";
    } else if (model.backend_state == APP_BACKEND_PROBING) {
        status = "Backend probing";
    } else if (backend_online && !state->agent_known) {
        status = "PC agent unavailable";
    } else if (backend_online && !state->agent_online) {
        status = "PC agent offline";
    } else if (state->telemetry_stale) {
        status = "Stale Data";
        dot_color = COLOR_ERROR;
    } else if (!state->telemetry_known) {
        status = "Telemetry unavailable";
        dot_color = COLOR_ERROR;
    } else {
        status = "PC agent online";
        dot_color = COLOR_SUCCESS;
    }
    set_label_text_if_changed(s_status, status);
    if (s_agent_dot != NULL) {
        lv_obj_set_style_bg_color(s_agent_dot, dot_color, 0);
    }

    char text[48];
    if (state->cpu_available) {
        snprintf(text, sizeof(text), "%d%%", (int)lroundf(state->cpu_percent));
        set_arc_value(s_cpu_arc, (uint8_t)lroundf(state->cpu_percent));
    } else {
        set_label_text_if_changed(s_cpu_value, "--");
        set_arc_value(s_cpu_arc, 0);
    }
    if (state->cpu_available) {
        set_label_text_if_changed(s_cpu_value, text);
    }
    if (state->cpu_temp_available && state->ram_available) {
        snprintf(text, sizeof(text), "%.0fC R%.1f/%.1fG", (double)state->cpu_temp_c, (double)state->ram_used_gib,
                 (double)state->ram_total_gib);
    } else if (state->ram_available) {
        snprintf(text, sizeof(text), "R%.1f/%.1fG", (double)state->ram_used_gib, (double)state->ram_total_gib);
    } else {
        snprintf(text, sizeof(text), "Temp -- RAM --");
    }
    set_label_text_if_changed(s_cpu_detail, text);

    if (state->gpu_available) {
        snprintf(text, sizeof(text), "%d%%", (int)lroundf(state->gpu_percent));
        set_arc_value(s_gpu_arc, (uint8_t)lroundf(state->gpu_percent));
    } else {
        set_label_text_if_changed(s_gpu_value, "--");
        set_arc_value(s_gpu_arc, 0);
    }
    if (state->gpu_available) {
        set_label_text_if_changed(s_gpu_value, text);
    }
    const char *gpu_name = state->gpu_name_available ? state->gpu_name : "GPU";
    if (strncmp(gpu_name, "NVIDIA GeForce ", 15U) == 0) {
        gpu_name += 15;
    }
    set_label_text_if_changed(s_gpu_title, gpu_name);
    if (state->gpu_temp_available && state->gpu_vram_available) {
        snprintf(text, sizeof(text), "%.0fC V%.0f/%.0fM", (double)state->gpu_temp_c, (double)state->gpu_vram_used_mib,
                 (double)state->gpu_vram_total_mib);
    } else if (state->gpu_vram_available) {
        snprintf(text, sizeof(text), "V%.0f/%.0fM", (double)state->gpu_vram_used_mib, (double)state->gpu_vram_total_mib);
    } else {
        snprintf(text, sizeof(text), "Temp -- VRAM --");
    }
    set_label_text_if_changed(s_gpu_detail, text);

    lv_obj_set_style_text_opa(s_cpu_value, metric_opa, 0);
    lv_obj_set_style_text_opa(s_gpu_value, metric_opa, 0);
    lv_obj_set_style_text_opa(s_cpu_detail, metric_opa, 0);
    lv_obj_set_style_text_opa(s_gpu_detail, metric_opa, 0);
    lv_obj_set_style_arc_opa(s_cpu_arc, metric_opa, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(s_gpu_arc, metric_opa, LV_PART_INDICATOR);
    update_action_buttons(state, actions_allowed);
}

esp_err_t pc_monitor_create(lv_obj_t *parent)
{
    if ((parent == NULL) || (s_lock != NULL)) {
        return parent == NULL ? ESP_ERR_INVALID_ARG : ESP_OK;
    }
    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }
    lv_obj_set_style_bg_color(parent, COLOR_BG, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "PC Monitor Card");
    lv_obj_set_style_text_color(title, COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(title, UI_FONT_TITLE, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 12, 8);

    s_agent_dot = lv_obj_create(parent);
    lv_obj_set_size(s_agent_dot, 8, 8);
    lv_obj_set_style_radius(s_agent_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(s_agent_dot, 0, 0);
    lv_obj_set_style_bg_color(s_agent_dot, COLOR_OFFLINE, 0);
    lv_obj_align(s_agent_dot, LV_ALIGN_TOP_RIGHT, -130, 15);

    s_status = lv_label_create(parent);
    lv_label_set_text(s_status, "Telemetry unavailable");
    lv_obj_set_style_text_color(s_status, COLOR_SECONDARY, 0);
    lv_obj_set_style_text_font(s_status, &lv_font_montserrat_12, 0);
    lv_label_set_long_mode(s_status, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_status, 116);
    lv_obj_align(s_status, LV_ALIGN_TOP_RIGHT, -8, 8);

    lv_obj_t *cpu_panel = create_gauge(parent, "CPU", &s_cpu_value, &s_cpu_detail, NULL);
    lv_obj_align(cpu_panel, LV_ALIGN_TOP_LEFT, 12, 38);
    s_cpu_arc = lv_obj_get_child(cpu_panel, 1);
    lv_obj_t *gpu_panel = create_gauge(parent, "GPU", &s_gpu_value, &s_gpu_detail, &s_gpu_title);
    lv_obj_align(gpu_panel, LV_ALIGN_TOP_RIGHT, -12, 38);
    s_gpu_arc = lv_obj_get_child(gpu_panel, 1);

    s_action_buttons[PC_LAUNCH_ACTION_VSCODE] = create_action_button(parent, "VS Code", PC_LAUNCH_ACTION_VSCODE);
    s_action_buttons[PC_LAUNCH_ACTION_BILIBILI] = create_action_button(parent, "Bilibili", PC_LAUNCH_ACTION_BILIBILI);
    s_action_buttons[PC_LAUNCH_ACTION_DOUYIN] = create_action_button(parent, "Douyin", PC_LAUNCH_ACTION_DOUYIN);
    lv_obj_align(s_action_buttons[PC_LAUNCH_ACTION_VSCODE], LV_ALIGN_BOTTOM_LEFT, 12, -18);
    lv_obj_align(s_action_buttons[PC_LAUNCH_ACTION_BILIBILI], LV_ALIGN_BOTTOM_MID, 0, -18);
    lv_obj_align(s_action_buttons[PC_LAUNCH_ACTION_DOUYIN], LV_ALIGN_BOTTOM_RIGHT, -12, -18);
    update_ui(&s_state);
    return ESP_OK;
}

void pc_monitor_refresh(void)
{
    if ((s_lock == NULL) || !ui_runtime_is_task()) {
        return;
    }
    pc_monitor_state_t state;
    snapshot_get(&state);
    update_ui(&state);
}

void pc_monitor_start(void)
{
    if ((s_task == NULL) && (xTaskCreate(pc_monitor_task, "pc_monitor", 8192, NULL, 3, &s_task) == pdPASS)) {
        xTaskNotifyGive(s_task);
    }
}

void pc_monitor_set_visible(bool visible)
{
    s_visible = visible;
    if (visible && (s_task != NULL)) {
        xTaskNotifyGive(s_task);
    }
}

void pc_monitor_request_action(pc_launch_action_t action)
{
    if ((action > PC_LAUNCH_ACTION_DOUYIN) || (s_lock == NULL)) {
        return;
    }
    pc_monitor_state_t next;
    snapshot_get(&next);
    if (next.action_state == PC_ACTION_PENDING) {
        return;
    }
    if (!backend_allows_pc_actions() || !next.agent_known || !next.agent_online) {
        next.action = action;
        set_action_result(&next, PC_ACTION_FAILED);
        snapshot_set(&next);
        return;
    }
    const uint32_t random_a = esp_random();
    const uint32_t random_b = esp_random();
    const uint32_t random_c = esp_random();
    const uint32_t random_d = esp_random();
    snprintf(next.request_id, sizeof(next.request_id), "%08" PRIx32 "-%04" PRIx32 "-4%03" PRIx32 "-%04" PRIx32 "-%08" PRIx32 "%04" PRIx32,
             random_a, random_b & 0xFFFFU, random_b >> 20U, (random_c & 0x0FFFU) | 0x8000U, random_c, random_d & 0xFFFFU);
    next.action = action;
    next.action_state = PC_ACTION_PENDING;
    next.action_sent = false;
    next.action_deadline_us = esp_timer_get_time() + PC_ACTION_CONFIRM_US;
    snapshot_set(&next);
    if (s_task != NULL) {
        xTaskNotifyGive(s_task);
    }
}
