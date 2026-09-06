#include "backend_probe.h"

#include <string.h>

#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_model.h"
#include "config_store.h"
#include "ui_runtime.h"

static TaskHandle_t s_task;

static void update_state(app_backend_state_t state, const char *message, const char *endpoint)
{
    app_model_set_backend(state, endpoint);
    app_model_set_status(message);
    ui_runtime_request_refresh();
}

static void probe_task(void *argument)
{
    (void)argument;
    while (true) {
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(30000));
        app_config_t config = {0};
        if (!config_store_load(&config) || (config.ha_endpoint[0] == '\0') || (config.ha_token[0] == '\0')) {
            update_state(APP_BACKEND_UNCONFIGURED, "HA not configured", "");
            continue;
        }
        update_state(APP_BACKEND_PROBING, "Probing backend", config.ha_endpoint);
        char url[CONFIG_ENDPOINT_MAX_LEN + 6] = {0};
        (void)snprintf(url, sizeof(url), "%s/api/", config.ha_endpoint);
        const esp_http_client_config_t client_config = {
            .url = url,
            .method = HTTP_METHOD_GET,
            .timeout_ms = 5000,
            .buffer_size = 1024,
            .buffer_size_tx = 1024,
        };
        esp_http_client_handle_t client = esp_http_client_init(&client_config);
        if (client == NULL) {
            update_state(APP_BACKEND_OFFLINE, "Backend unavailable", config.ha_endpoint);
            continue;
        }
        char authorization[CONFIG_TOKEN_MAX_LEN + 8] = "Bearer ";
        strncat(authorization, config.ha_token, sizeof(authorization) - strlen(authorization) - 1U);
        (void)esp_http_client_set_header(client, "Authorization", authorization);
        const esp_err_t result = esp_http_client_perform(client);
        const int status = esp_http_client_get_status_code(client);
        ESP_LOGI("backend", "HA probe result=%s http_status=%d", esp_err_to_name(result), status);
        esp_http_client_cleanup(client);
        if ((result == ESP_OK) && (status >= 200) && (status < 300)) {
            update_state(APP_BACKEND_ONLINE, "Backend online", config.ha_endpoint);
        } else if (status == 401) {
            update_state(APP_BACKEND_AUTH_FAILED, "Backend authentication failed", config.ha_endpoint);
        } else {
            update_state(APP_BACKEND_OFFLINE, "Backend offline", config.ha_endpoint);
        }
    }
}

esp_err_t backend_probe_init(void)
{
    if (s_task != NULL) {
        return ESP_OK;
    }
    if (xTaskCreate(probe_task, "ha_probe", 6144, NULL, 3, &s_task) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void backend_probe_request_now(void)
{
    if (s_task != NULL) {
        xTaskNotifyGive(s_task);
    }
}
