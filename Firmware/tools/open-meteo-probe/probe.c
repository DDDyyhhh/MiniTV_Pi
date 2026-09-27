/* Temporary, on-device network measurement for issue #22. Not production firmware. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_model.h"
#include "ui_runtime.h"

#define PROBE_BODY_LIMIT 1024
#define PROBE_TIMEOUT_MS 8000
#define PROBE_INTERVAL_MS (30 * 60 * 1000)
#define PROBE_URL "https://api.open-meteo.com/v1/forecast?latitude=39.9042&longitude=116.4074&current=temperature_2m&temperature_unit=celsius&timeformat=unixtime&timezone=GMT%2B8&forecast_days=1"

static const char *TAG = "OM-PROBE";

typedef struct {
    char body[PROBE_BODY_LIMIT + 1];
    size_t body_len;
    unsigned data_events;
    bool connected;
    bool finished;
    bool overflow;
    char content_encoding[32];
    char transfer_encoding[32];
} response_t;

static esp_err_t on_http_event(esp_http_client_event_t *event)
{
    response_t *response = event->user_data;
    switch (event->event_id) {
    case HTTP_EVENT_ON_CONNECTED:
        response->connected = true;
        break;
    case HTTP_EVENT_ON_HEADER:
        if (strcasecmp(event->header_key, "Content-Encoding") == 0) {
            snprintf(response->content_encoding, sizeof(response->content_encoding), "%s", event->header_value);
        } else if (strcasecmp(event->header_key, "Transfer-Encoding") == 0) {
            snprintf(response->transfer_encoding, sizeof(response->transfer_encoding), "%s", event->header_value);
        }
        break;
    case HTTP_EVENT_ON_DATA:
        response->data_events++;
        if (event->data_len < 0 || (size_t)event->data_len > PROBE_BODY_LIMIT - response->body_len) {
            response->overflow = true;
            return ESP_FAIL;
        }
        memcpy(response->body + response->body_len, event->data, (size_t)event->data_len);
        response->body_len += (size_t)event->data_len;
        response->body[response->body_len] = '\0';
        break;
    case HTTP_EVENT_ON_FINISH:
        response->finished = true;
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void run_request(const char *scenario, const char *url, const char *accept_encoding)
{
    response_t response = {0};
    const uint32_t before_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    const uint32_t before_min = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
    const uint32_t before_largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    const uint16_t fps_before = ui_runtime_fps();
    const int64_t start_us = esp_timer_get_time();
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = PROBE_TIMEOUT_MS,
        .event_handler = on_http_event,
        .user_data = &response,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = 512,
        .disable_auto_redirect = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "result scenario=%s init_failed=1", scenario);
        return;
    }
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_header(client, "Accept-Encoding", accept_encoding);
    const esp_err_t result = esp_http_client_perform(client);
    const int status = esp_http_client_get_status_code(client);
    const int64_t content_length = esp_http_client_get_content_length(client);
    const bool chunked = esp_http_client_is_chunked_response(client);
    const int64_t elapsed_ms = (esp_timer_get_time() - start_us) / 1000;
    const uint32_t after_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    const uint32_t after_min = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
    const uint32_t after_largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    const uint16_t fps_after = ui_runtime_fps();
    ESP_LOGI(TAG, "result scenario=%s err=%s status=%d connected=%d finished=%d bytes=%u events=%u overflow=%d content_length=%lld chunked=%d content_encoding=%s transfer_encoding=%s elapsed_ms=%lld free_before=%u free_after=%u min_before=%u min_after=%u largest_before=%u largest_after=%u fps_before=%u fps_after=%u",
             scenario, esp_err_to_name(result), status, response.connected, response.finished,
             (unsigned)response.body_len, response.data_events, response.overflow,
             (long long)content_length, chunked,
             response.content_encoding[0] ? response.content_encoding : "identity",
             response.transfer_encoding[0] ? response.transfer_encoding : "none",
             (long long)elapsed_ms, before_free, after_free, before_min, after_min,
             before_largest, after_largest, fps_before, fps_after);
    if (result == ESP_OK && status == 200 && !response.overflow &&
        strcasecmp(response.content_encoding, "deflate") != 0 &&
        strcasecmp(response.content_encoding, "gzip") != 0) {
        ESP_LOGI(TAG, "body scenario=%s json=%s", scenario, response.body);
    }
    esp_http_client_cleanup(client);
}

static void probe_task(void *unused)
{
    (void)unused;
    app_model_t model;
    bool ready = false;
    for (unsigned i = 0; i < 120; i++) {
        app_model_get(&model);
        if (model.wifi_state == APP_WIFI_CONNECTED && model.time_synced) {
            ready = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGI(TAG, "ready wifi=%d time_synced=%d", model.wifi_state == APP_WIFI_CONNECTED, model.time_synced);
    if (!ready) {
        ESP_LOGE(TAG, "done reason=network_or_time_unavailable");
        vTaskDelete(NULL);
        return;
    }
    run_request("current_1", PROBE_URL, "identity");
    run_request("bad_parameter_400", "https://api.open-meteo.com/v1/forecast?latitude=invalid&longitude=116.4074&current=temperature_2m", "identity");
    run_request("dns_failure", "https://minitv-probe.invalid/v1/forecast", "identity");
    run_request("deflate_offer", PROBE_URL, "deflate");
    ESP_LOGI(TAG, "interval_start seconds=1800");
    vTaskDelay(pdMS_TO_TICKS(PROBE_INTERVAL_MS));
    app_model_get(&model);
    ESP_LOGI(TAG, "interval_end wifi=%d time_synced=%d", model.wifi_state == APP_WIFI_CONNECTED, model.time_synced);
    if (model.wifi_state == APP_WIFI_CONNECTED && model.time_synced) {
        run_request("current_2", PROBE_URL, "identity");
    }
    ESP_LOGI(TAG, "done samples=2");
    vTaskDelete(NULL);
}

void open_meteo_probe_start(void)
{
    BaseType_t created = xTaskCreate(probe_task, "om_probe", 8192, NULL, 3, NULL);
    ESP_LOGI(TAG, "start task_created=%d body_limit=%d timeout_ms=%d interval_ms=%d", created == pdPASS,
             PROBE_BODY_LIMIT, PROBE_TIMEOUT_MS, PROBE_INTERVAL_MS);
}
