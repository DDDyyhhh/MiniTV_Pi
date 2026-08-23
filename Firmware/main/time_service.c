#include "time_service.h"

#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_model.h"
#include "ui_runtime.h"

static bool s_started;

static void sync_task(void *argument)
{
    (void)argument;
    const esp_err_t result = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(15000));
    app_model_set_time_synced(result == ESP_OK);
    app_model_set_status(result == ESP_OK ? "Time synchronized" : "Time not synchronized");
    ui_runtime_request_refresh();
    ESP_LOGI("time", "SNTP %s", result == ESP_OK ? "synchronized" : "not synchronized");
    vTaskDelete(NULL);
}

void time_service_start(void)
{
    if (!s_started) {
        const esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
        if (esp_netif_sntp_init(&config) != ESP_OK) {
            app_model_set_status("SNTP initialization failed");
            ui_runtime_request_refresh();
            return;
        }
        s_started = true;
    }
    xTaskCreate(sync_task, "sntp", 3072, NULL, 4, NULL);
}
