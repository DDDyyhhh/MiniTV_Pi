#include <inttypes.h>

#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_model.h"
#include "backend_probe.h"
#include "board_backlight.h"
#include "board_button.h"
#include "board_display.h"
#include "board_touch.h"
#include "config_store.h"
#include "diagnostics.h"
#include "network_manager.h"
#include "ui_shell.h"
#include "ui_runtime.h"

static const char *TAG = "minitv";

static void diagnostics_task(void *argument)
{
    (void)argument;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(60000));
        diagnostics_log_baseline("periodic");
    }
}

void app_main(void)
{
    const esp_app_desc_t *description = esp_app_get_description();
    ESP_LOGI(TAG, "Mini TV Skeleton Milestone %s (%s)", description->version, description->date);
    ESP_LOGI(TAG, "reset_reason=%d free_heap=%u minimum_free_heap=%u largest_free_block=%u", esp_reset_reason(),
             (unsigned)esp_get_free_heap_size(), (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    ESP_ERROR_CHECK(config_store_init());
    app_model_init();
    app_config_t config = {0};
    const bool configured = config_store_load(&config);
    const uint8_t brightness = (configured && (config.brightness_percent >= 10U)) ? config.brightness_percent : 55U;
    app_model_set_brightness(brightness);

    ESP_ERROR_CHECK(board_backlight_init(brightness));
    ESP_ERROR_CHECK(board_display_init());
    ESP_ERROR_CHECK(board_touch_init());
    ESP_ERROR_CHECK(ui_runtime_init());
    ESP_ERROR_CHECK(ui_shell_create());
    ESP_ERROR_CHECK(board_button_init());
    ESP_ERROR_CHECK(backend_probe_init());
    ESP_ERROR_CHECK(network_manager_init());

    if (configured) {
        app_model_set_status("Saved Wi-Fi configuration found");
        ESP_ERROR_CHECK(network_manager_connect(&config));
    } else {
        app_model_set_status("No Wi-Fi configuration; opening portal");
        network_manager_start_provisioning();
    }
    ui_shell_refresh();
    ESP_ERROR_CHECK(ui_runtime_start());
    diagnostics_log_baseline("shell-ready");
    xTaskCreate(diagnostics_task, "diagnostics", 3072, NULL, 2, NULL);
}
