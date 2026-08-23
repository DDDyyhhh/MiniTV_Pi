#include "board_button.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board_pins.h"
#include "provisioning_portal.h"
#include "ui_runtime.h"

#define LONG_PRESS_TICKS pdMS_TO_TICKS(3000)

static const char *TAG = "board_button";
static volatile TickType_t s_pressed_at;
static volatile bool s_long_press;
static TaskHandle_t s_task;

static void IRAM_ATTR button_interrupt(void *argument)
{
    (void)argument;
    BaseType_t high_priority_woken = pdFALSE;
    if (gpio_get_level(BOARD_BOOT_GPIO) == 0) {
        s_pressed_at = xTaskGetTickCountFromISR();
        return;
    }
    s_long_press = (xTaskGetTickCountFromISR() - s_pressed_at) >= LONG_PRESS_TICKS;
    vTaskNotifyGiveFromISR(s_task, &high_priority_woken);
    if (high_priority_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void button_task(void *argument)
{
    (void)argument;
    while (true) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        const TickType_t held_ticks = xTaskGetTickCount() - s_pressed_at;
        ESP_LOGI(TAG, "[DEBUG-button] release after %u ms: %s press", (unsigned)(held_ticks * portTICK_PERIOD_MS),
                 s_long_press ? "long" : "short");
        if (s_long_press) {
            provisioning_portal_request_start();
        } else {
            ui_runtime_request_control_center_toggle();
        }
    }
}

esp_err_t board_button_init(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << BOARD_BOOT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&config), TAG, "BOOT GPIO setup failed");
    if (xTaskCreate(button_task, "button", 2048, NULL, 5, &s_task) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(BOARD_BOOT_GPIO, button_interrupt, NULL), TAG, "BOOT ISR add failed");
    ESP_LOGI(TAG, "BOOT short press toggles Control Center; hold 3s starts provisioning");
    return ESP_OK;
}
