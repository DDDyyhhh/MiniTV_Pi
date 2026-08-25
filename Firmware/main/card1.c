#include "card1.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "nvs.h"

#include "app_model.h"
#include "config_store.h"
#include "ui_runtime.h"

#define CARD1_HTTP_CHUNK_BYTES 512
#define CARD1_FORECAST_OBJECT_BYTES 384
#define CARD1_CHART_POINTS 47
#define CARD1_WEATHER_REFRESH_SECONDS (30 * 60)
#define CARD1_CACHE_MAX_AGE_SECONDS (24 * 60 * 60)
#define CARD1_NVS_NAMESPACE "minitv"
#define CARD1_NVS_CACHE_KEY "weather"
#define CARD1_CACHE_VERSION 1U
#define COLOR_BG lv_color_hex(0x121214)
#define COLOR_PANEL lv_color_hex(0x1C1C1E)
#define COLOR_BORDER lv_color_hex(0x3A3A3C)
#define COLOR_PRIMARY lv_color_hex(0xF5F5F7)
#define COLOR_SECONDARY lv_color_hex(0xA1A1A6)
#define COLOR_ACCENT lv_color_hex(0x0A84FF)
#define COLOR_RAIN lv_color_hex(0x63B3ED)

static const char *TAG = "card1";
static lv_obj_t *s_subpages;
static lv_obj_t *s_clock;
static lv_obj_t *s_date;
static lv_obj_t *s_weather_chart;
static lv_obj_t *s_weather_state;
static lv_obj_t *s_weather_extremes;
static lv_obj_t *s_calendar_label;
static lv_obj_t *s_holiday_label;
static lv_chart_series_t *s_temperature_series;
static lv_chart_series_t *s_rain_series;
static TaskHandle_t s_weather_task;
static SemaphoreHandle_t s_weather_lock;
static card1_weather_snapshot_t s_weather;
static uint32_t s_weather_revision;
static uint32_t s_rendered_weather_revision;
static uint8_t s_last_minute = 255;
static int s_last_calendar_day = -1;

typedef struct {
    uint32_t version;
    card1_weather_snapshot_t snapshot;
} persisted_weather_cache_t;

typedef struct { uint16_t year; uint8_t month; uint8_t day; const char *name; } holiday_t;
static const holiday_t s_holidays[] = {
    {2026, 1, 1, "New Year"}, {2026, 2, 17, "Spring Festival"}, {2026, 4, 5, "Qingming"},
    {2026, 5, 1, "Labour Day"}, {2026, 6, 19, "Dragon Boat"}, {2026, 9, 25, "Mid-Autumn"}, {2026, 10, 1, "National Day"},
    {2027, 1, 1, "New Year"}, {2027, 2, 6, "Spring Festival"}, {2027, 4, 5, "Qingming"},
    {2027, 5, 1, "Labour Day"}, {2027, 6, 9, "Dragon Boat"}, {2027, 9, 15, "Mid-Autumn"}, {2027, 10, 1, "National Day"},
};

static void style_panel(lv_obj_t *object)
{
    lv_obj_set_style_bg_color(object, COLOR_PANEL, 0);
    lv_obj_set_style_border_color(object, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(object, 1, 0);
    lv_obj_set_style_radius(object, 16, 0);
    lv_obj_set_style_pad_all(object, 10, 0);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
}

static void set_weather(const card1_weather_snapshot_t *snapshot)
{
    xSemaphoreTake(s_weather_lock, portMAX_DELAY);
    s_weather = *snapshot;
    s_weather_revision++;
    xSemaphoreGive(s_weather_lock);
}

static void load_weather_cache(void)
{
    nvs_handle_t handle;
    if (nvs_open(CARD1_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return;
    persisted_weather_cache_t cache = {0};
    size_t size = sizeof(cache);
    const esp_err_t result = nvs_get_blob(handle, CARD1_NVS_CACHE_KEY, &cache, &size);
    nvs_close(handle);
    if ((result == ESP_OK) && (size == sizeof(cache)) && (cache.version == CARD1_CACHE_VERSION) && cache.snapshot.valid) {
        const time_t age = time(NULL) - cache.snapshot.fetched_at;
        if ((age >= 0) && (age < CARD1_CACHE_MAX_AGE_SECONDS)) {
            set_weather(&cache.snapshot);
        }
    }
}

static void save_weather_cache(const card1_weather_snapshot_t *snapshot)
{
    persisted_weather_cache_t cache = {.version = CARD1_CACHE_VERSION, .snapshot = *snapshot};
    nvs_handle_t handle;
    if (nvs_open(CARD1_NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        (void)nvs_set_blob(handle, CARD1_NVS_CACHE_KEY, &cache, sizeof(cache));
        (void)nvs_commit(handle);
        nvs_close(handle);
    }
}

static lv_obj_t *new_page(uint8_t row, const char *title)
{
    lv_obj_t *page = lv_tileview_add_tile(s_subpages, 0, row, LV_DIR_VER);
    lv_obj_set_style_bg_color(page, COLOR_BG, 0);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *label = lv_label_create(page);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_color(label, COLOR_SECONDARY, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 8, 2);
    return page;
}

static void subpage_changed(lv_event_t *event)
{
    lv_obj_t *dots = lv_event_get_user_data(event);
    const lv_obj_t *active = lv_tileview_get_tile_act(s_subpages);
    for (uint8_t index = 0; index < 3; index++) {
        lv_obj_t *dot = lv_obj_get_child(dots, index);
        const lv_obj_t *page = lv_obj_get_child(s_subpages, index);
        lv_obj_set_style_bg_color(dot, page == active ? COLOR_ACCENT : COLOR_BORDER, 0);
    }
}

static void draw_clock_page(lv_obj_t *page)
{
    lv_obj_t *base = lv_obj_create(page);
    lv_obj_set_size(base, 216, 188);
    lv_obj_align(base, LV_ALIGN_TOP_MID, 0, 28);
    style_panel(base);
    lv_obj_set_style_shadow_color(base, lv_color_black(), 0);
    lv_obj_set_style_shadow_width(base, 10, 0);
    lv_obj_set_style_shadow_opa(base, LV_OPA_30, 0);

    s_clock = lv_label_create(base);
    lv_label_set_text(s_clock, "--:--");
    lv_obj_set_style_text_color(s_clock, COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(s_clock, &lv_font_montserrat_40, 0);
    lv_obj_align(s_clock, LV_ALIGN_CENTER, 0, -14);
    s_date = lv_label_create(base);
    lv_label_set_text(s_date, "Not synced");
    lv_obj_set_style_text_color(s_date, COLOR_SECONDARY, 0);
    lv_obj_set_style_text_font(s_date, &lv_font_montserrat_14, 0);
    lv_obj_align(s_date, LV_ALIGN_CENTER, 0, 38);

    lv_obj_t *hint = lv_label_create(page);
    lv_label_set_text(hint, "Swipe up: Weather  |  Calendar");
    lv_obj_set_style_text_color(hint, COLOR_SECONDARY, 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);
}

static void draw_weather_page(lv_obj_t *page)
{
    s_weather_chart = lv_chart_create(page);
    lv_obj_set_size(s_weather_chart, 220, 158);
    lv_obj_align(s_weather_chart, LV_ALIGN_TOP_MID, 0, 38);
    lv_chart_set_type(s_weather_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s_weather_chart, CARD1_CHART_POINTS);
    lv_chart_set_div_line_count(s_weather_chart, 4, 4);
    lv_obj_set_style_bg_color(s_weather_chart, COLOR_PANEL, 0);
    lv_obj_set_style_border_color(s_weather_chart, COLOR_BORDER, 0);
    lv_obj_set_style_line_color(s_weather_chart, COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_line_width(s_weather_chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_line_rounded(s_weather_chart, true, LV_PART_ITEMS);
    lv_obj_set_style_size(s_weather_chart, 0, LV_PART_INDICATOR);
    s_temperature_series = lv_chart_add_series(s_weather_chart, COLOR_ACCENT, LV_CHART_AXIS_PRIMARY_Y);
    s_rain_series = lv_chart_add_series(s_weather_chart, COLOR_RAIN, LV_CHART_AXIS_SECONDARY_Y);
    lv_chart_set_range(s_weather_chart, LV_CHART_AXIS_PRIMARY_Y, -200, 500);
    lv_chart_set_range(s_weather_chart, LV_CHART_AXIS_SECONDARY_Y, 0, 100);
    lv_chart_set_all_value(s_weather_chart, s_temperature_series, LV_CHART_POINT_NONE);
    lv_chart_set_all_value(s_weather_chart, s_rain_series, LV_CHART_POINT_NONE);

    s_weather_extremes = lv_label_create(page);
    lv_label_set_text(s_weather_extremes, "24h forecast  -- / --");
    lv_obj_set_style_text_color(s_weather_extremes, COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(s_weather_extremes, &lv_font_montserrat_12, 0);
    lv_obj_align(s_weather_extremes, LV_ALIGN_TOP_LEFT, 8, 20);
    s_weather_state = lv_label_create(page);
    lv_label_set_text(s_weather_state, "Weather offline • 24h forecast");
    lv_obj_set_style_text_color(s_weather_state, COLOR_SECONDARY, 0);
    lv_obj_set_style_text_font(s_weather_state, &lv_font_montserrat_12, 0);
    lv_obj_align(s_weather_state, LV_ALIGN_BOTTOM_LEFT, 8, -8);
}

static void draw_calendar_page(lv_obj_t *page)
{
    lv_obj_t *calendar = lv_obj_create(page);
    lv_obj_set_size(calendar, 220, 168);
    lv_obj_align(calendar, LV_ALIGN_TOP_MID, 0, 28);
    style_panel(calendar);
    s_calendar_label = lv_label_create(calendar);
    lv_obj_set_style_text_color(s_calendar_label, COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(s_calendar_label, &lv_font_montserrat_12, 0);
    lv_obj_align(s_calendar_label, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *holiday = lv_obj_create(page);
    lv_obj_set_size(holiday, 220, 72);
    lv_obj_align(holiday, LV_ALIGN_BOTTOM_MID, 0, -8);
    style_panel(holiday);
    s_holiday_label = lv_label_create(holiday);
    lv_obj_set_style_text_color(s_holiday_label, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(s_holiday_label, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(s_holiday_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_holiday_label, 196);
    lv_obj_align(s_holiday_label, LV_ALIGN_CENTER, 0, 0);
}

static void animate_clock_flip(void *object, int32_t progress)
{
    const int32_t clamped = progress < 0 ? 0 : (progress > 255 ? 255 : progress);
    lv_obj_set_style_opa(object, (lv_opa_t)clamped, 0);
    lv_obj_set_style_translate_y(object, (int16_t)(-8 + (8 * clamped / 255)), 0);
}

static void update_clock(const app_model_t *model)
{
    if (!model->time_synced) {
        lv_label_set_text(s_clock, "--:--");
        lv_label_set_text(s_date, "Not synced");
        return;
    }
    time_t now = time(NULL);
    struct tm local = {0};
    if ((now < 1000000000) || (localtime_r(&now, &local) == NULL)) return;
    if (s_last_minute == local.tm_min) return;
    s_last_minute = local.tm_min;
    char time_text[8];
    char date_text[40];
    (void)strftime(time_text, sizeof(time_text), "%H:%M", &local);
    (void)strftime(date_text, sizeof(date_text), "%a  %Y.%m.%d", &local);
    lv_label_set_text(s_clock, time_text);
    lv_label_set_text(s_date, date_text);
    lv_obj_set_style_opa(s_clock, LV_OPA_30, 0);
    lv_obj_set_style_translate_y(s_clock, -8, 0);
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, s_clock);
    lv_anim_set_values(&animation, LV_OPA_30, LV_OPA_COVER);
    lv_anim_set_time(&animation, 220);
    lv_anim_set_exec_cb(&animation, animate_clock_flip);
    lv_anim_start(&animation);
}

static int days_in_month(int year, int month)
{
    static const int lengths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month != 1) return lengths[month];
    return ((year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0))) ? 29 : 28;
}

static void update_calendar(const app_model_t *model)
{
    if (!model->time_synced) {
        lv_label_set_text(s_calendar_label, "Calendar waits for time sync");
        lv_label_set_text(s_holiday_label, "Holiday countdown waits for time sync");
        return;
    }
    time_t now = time(NULL);
    struct tm local = {0};
    if (localtime_r(&now, &local) == NULL || s_last_calendar_day == local.tm_yday) return;
    s_last_calendar_day = local.tm_yday;
    char calendar[256] = {0};
    int written = snprintf(calendar, sizeof(calendar), "%04d.%02d\nSu Mo Tu We Th Fr Sa\n", local.tm_year + 1900, local.tm_mon + 1);
    struct tm first = local;
    first.tm_mday = 1;
    (void)mktime(&first);
    for (int blank = 0; blank < first.tm_wday; blank++) written += snprintf(calendar + written, sizeof(calendar) - written, "   ");
    for (int day = 1; day <= days_in_month(local.tm_year + 1900, local.tm_mon); day++) {
        written += snprintf(calendar + written, sizeof(calendar) - written, "%2d%c", day, day == local.tm_mday ? '*' : ' ');
        if ((first.tm_wday + day) % 7 == 0) written += snprintf(calendar + written, sizeof(calendar) - written, "\n");
    }
    lv_label_set_text(s_calendar_label, calendar);

    const holiday_t *next = NULL;
    int days_until = 0;
    for (size_t i = 0; i < sizeof(s_holidays) / sizeof(s_holidays[0]); i++) {
        struct tm target = {.tm_year = s_holidays[i].year - 1900, .tm_mon = s_holidays[i].month - 1, .tm_mday = s_holidays[i].day, .tm_isdst = -1};
        time_t target_time = mktime(&target);
        if (target_time >= now) { next = &s_holidays[i]; days_until = (int)((target_time - now) / 86400); break; }
    }
    char holiday_text[96];
    if (next == NULL) snprintf(holiday_text, sizeof(holiday_text), "Holiday table needs update");
    else snprintf(holiday_text, sizeof(holiday_text), "%s\n%d days remaining", next->name, days_until);
    lv_label_set_text(s_holiday_label, holiday_text);
}

static bool extract_number(const char *object, const char *key, double *value)
{
    const char *at = strstr(object, key);
    if (at == NULL) return false;
    const char *colon = strchr(at, ':');
    if (colon == NULL) return false;
    char *end = NULL;
    *value = strtod(colon + 1, &end);
    return end != colon + 1;
}

static bool read_forecast_stream(esp_http_client_handle_t client, card1_weather_snapshot_t *snapshot)
{
    card1_weather_snapshot_t result = {0};
    char chunk[CARD1_HTTP_CHUNK_BYTES];
    char key_window[12] = {0};
    char object[CARD1_FORECAST_OBJECT_BYTES] = {0};
    size_t key_length = 0;
    size_t object_length = 0;
    int brace_depth = 0;
    bool forecast_key_found = false;
    bool forecast_array_started = false;
    int received;

    while ((received = esp_http_client_read(client, chunk, sizeof(chunk))) > 0) {
        for (int index = 0; index < received; index++) {
            const char character = chunk[index];
            if (!forecast_key_found) {
                if (key_length < sizeof(key_window) - 1U) key_window[key_length++] = character;
                else {
                    memmove(key_window, key_window + 1, sizeof(key_window) - 2U);
                    key_window[sizeof(key_window) - 2U] = character;
                }
                key_window[key_length] = '\0';
                if (strstr(key_window, "\"forecast\"") != NULL) forecast_key_found = true;
                continue;
            }
            if (!forecast_array_started) {
                if (character == '[') forecast_array_started = true;
                continue;
            }
            if (brace_depth == 0) {
                if (character == '{') {
                    brace_depth = 1;
                    object_length = 0;
                    object[object_length++] = character;
                } else if (character == ']') {
                    goto finished;
                }
                continue;
            }
            if (object_length + 1U >= sizeof(object)) {
                brace_depth = 0;
                object_length = 0;
                continue;
            }
            object[object_length++] = character;
            if (character == '{') brace_depth++;
            if (character != '}') continue;
            brace_depth--;
            if (brace_depth != 0) continue;
            object[object_length] = '\0';
            double temperature = 0;
            double precipitation = 0;
            if (extract_number(object, "\"temperature\"", &temperature)) {
                (void)extract_number(object, "\"precipitation\"", &precipitation);
                const uint8_t point = result.point_count++;
                result.temperature_tenths[point] = (int16_t)(temperature * 10.0);
                result.precipitation_tenths[point] = (int16_t)(precipitation * 10.0);
                if (result.point_count == CARD1_FORECAST_POINTS) goto finished;
            }
        }
    }

finished:
    if (result.point_count != CARD1_FORECAST_POINTS) return false;
    result.valid = true;
    result.fetched_at = time(NULL);
    *snapshot = result;
    return true;
}

static bool fetch_forecast(const app_config_t *config, card1_weather_snapshot_t *snapshot)
{
    char url[CONFIG_ENDPOINT_MAX_LEN + 64];
    char request[160];
    snprintf(url, sizeof(url), "%s/api/services/weather/get_forecasts?return_response", config->ha_endpoint);
    snprintf(request, sizeof(request), "{\"type\":\"hourly\",\"entity_id\":\"%s\"}", config->weather_entity);
    esp_http_client_config_t client_config = {.url = url, .method = HTTP_METHOD_POST, .timeout_ms = 5000, .buffer_size = 1024, .buffer_size_tx = 256};
    esp_http_client_handle_t client = esp_http_client_init(&client_config);
    if (client == NULL) return false;
    char authorization[CONFIG_TOKEN_MAX_LEN + 8] = "Bearer ";
    strncat(authorization, config->ha_token, sizeof(authorization) - strlen(authorization) - 1U);
    esp_http_client_set_header(client, "Authorization", authorization);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, request, strlen(request));
    bool valid = false;
    if ((esp_http_client_open(client, strlen(request)) == ESP_OK) && (esp_http_client_fetch_headers(client) >= 0) &&
        (esp_http_client_get_status_code(client) >= 200) && (esp_http_client_get_status_code(client) < 300)) {
        valid = read_forecast_stream(client, snapshot);
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return valid;
}

static void weather_task(void *argument)
{
    (void)argument;
    while (true) {
        app_config_t config = {0};
        card1_weather_snapshot_t snapshot;
        if (config_store_load(&config) && (config.weather_entity[0] != '\0') && (config.ha_endpoint[0] != '\0') &&
            (config.ha_token[0] != '\0') && fetch_forecast(&config, &snapshot)) {
            set_weather(&snapshot);
            save_weather_cache(&snapshot);
            ESP_LOGI(TAG, "Weather Trend updated: %u hourly points", snapshot.point_count);
            ui_runtime_request_refresh();
        } else {
            xSemaphoreTake(s_weather_lock, portMAX_DELAY);
            const time_t age = time(NULL) - s_weather.fetched_at;
            if (s_weather.valid && (age >= 0) && (age < CARD1_CACHE_MAX_AGE_SECONDS)) {
                s_weather.stale = true;
            } else {
                s_weather.valid = false;
            }
            s_weather_revision++;
            xSemaphoreGive(s_weather_lock);
            ESP_LOGW(TAG, "Weather Trend unavailable; keeping Stale Data if valid");
            ui_runtime_request_refresh();
        }
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(CARD1_WEATHER_REFRESH_SECONDS * 1000U));
    }
}

static void update_weather_chart(void)
{
    card1_weather_snapshot_t snapshot;
    uint32_t revision;
    xSemaphoreTake(s_weather_lock, portMAX_DELAY);
    snapshot = s_weather;
    revision = s_weather_revision;
    xSemaphoreGive(s_weather_lock);
    if (revision == s_rendered_weather_revision) return;
    s_rendered_weather_revision = revision;
    if (!snapshot.valid || snapshot.point_count == 0) {
        lv_chart_set_all_value(s_weather_chart, s_temperature_series, LV_CHART_POINT_NONE);
        lv_chart_set_all_value(s_weather_chart, s_rain_series, LV_CHART_POINT_NONE);
        lv_chart_refresh(s_weather_chart);
        lv_label_set_text(s_weather_extremes, "24h forecast  -- / --");
        lv_label_set_text(s_weather_state, "Weather offline • no cached forecast");
        return;
    }
    int16_t low = snapshot.temperature_tenths[0], high = low;
    int16_t rain_high = 10;
    for (uint8_t point = 0; point < snapshot.point_count; point++) {
        if (snapshot.temperature_tenths[point] < low) low = snapshot.temperature_tenths[point];
        if (snapshot.temperature_tenths[point] > high) high = snapshot.temperature_tenths[point];
        if (snapshot.precipitation_tenths[point] > rain_high) rain_high = snapshot.precipitation_tenths[point];
    }
    low -= 20; high += 20;
    if (high - low < 40) { low -= 20; high += 20; }
    lv_chart_set_range(s_weather_chart, LV_CHART_AXIS_PRIMARY_Y, low, high);
    lv_chart_set_range(s_weather_chart, LV_CHART_AXIS_SECONDARY_Y, 0, rain_high + 10);
    for (uint8_t chart_point = 0; chart_point < CARD1_CHART_POINTS; chart_point++) {
        int32_t temperature = LV_CHART_POINT_NONE;
        int32_t precipitation = LV_CHART_POINT_NONE;
        if (snapshot.point_count == 1U && chart_point == 0U) {
            temperature = snapshot.temperature_tenths[0];
            precipitation = snapshot.precipitation_tenths[0];
        } else if (snapshot.point_count >= 2U) {
            const uint8_t left = chart_point / 2U;
            const uint8_t right = left + (chart_point & 1U);
            if (right < snapshot.point_count) {
                temperature = right == left ? snapshot.temperature_tenths[left] :
                    (snapshot.temperature_tenths[left] + snapshot.temperature_tenths[right]) / 2;
                precipitation = right == left ? snapshot.precipitation_tenths[left] :
                    (snapshot.precipitation_tenths[left] + snapshot.precipitation_tenths[right]) / 2;
            }
        }
        lv_chart_set_value_by_id(s_weather_chart, s_temperature_series, chart_point, temperature);
        lv_chart_set_value_by_id(s_weather_chart, s_rain_series, chart_point, precipitation);
    }
    lv_chart_refresh(s_weather_chart);
    char extremes[64];
    snprintf(extremes, sizeof(extremes), "24h  Low %.1f°  High %.1f°", low / 10.0 + 2.0, high / 10.0 - 2.0);
    lv_label_set_text(s_weather_extremes, extremes);
    char state[96];
    snprintf(state, sizeof(state), "%s  %u hourly points", snapshot.stale ? "Stale Data" : "Updated", snapshot.point_count);
    lv_label_set_text(s_weather_state, state);
}

esp_err_t card1_create(lv_obj_t *parent)
{
    s_weather_lock = xSemaphoreCreateMutex();
    if (s_weather_lock == NULL) return ESP_ERR_NO_MEM;
    load_weather_cache();
    s_weather_revision++;
    s_subpages = lv_tileview_create(parent);
    lv_obj_set_size(s_subpages, 240, 320);
    lv_obj_align(s_subpages, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_scrollbar_mode(s_subpages, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(s_subpages, COLOR_BG, 0);

    lv_obj_t *dots = lv_obj_create(parent);
    lv_obj_set_size(dots, 72, 12);
    lv_obj_align(dots, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_bg_opa(dots, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dots, 0, 0);
    for (uint8_t index = 0; index < 3; index++) {
        lv_obj_t *dot = lv_obj_create(dots);
        lv_obj_set_size(dot, 7, 7);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_set_style_bg_color(dot, index == 0 ? COLOR_ACCENT : COLOR_BORDER, 0);
        lv_obj_align(dot, LV_ALIGN_LEFT_MID, index * 24, 0);
    }
    draw_clock_page(new_page(0, "Flip Clock"));
    draw_weather_page(new_page(1, "Weather Trend • 24h"));
    draw_calendar_page(new_page(2, "Calendar & Holiday"));
    lv_obj_add_event_cb(s_subpages, subpage_changed, LV_EVENT_VALUE_CHANGED, dots);
    return ESP_OK;
}

void card1_refresh(void)
{
    if (s_clock == NULL) return;
    app_model_t model;
    app_model_get(&model);
    update_clock(&model);
    update_calendar(&model);
    update_weather_chart();
}

void card1_start(void)
{
    if (s_weather_task == NULL && xTaskCreate(weather_task, "weather", 8192, NULL, 3, &s_weather_task) == pdPASS) {
        xTaskNotifyGive(s_weather_task);
    }
}

void card1_get_weather(card1_weather_snapshot_t *snapshot)
{
    if (snapshot == NULL || s_weather_lock == NULL) return;
    xSemaphoreTake(s_weather_lock, portMAX_DELAY);
    *snapshot = s_weather;
    xSemaphoreGive(s_weather_lock);
}
