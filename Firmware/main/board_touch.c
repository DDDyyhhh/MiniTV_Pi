#include "board_touch.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board_pins.h"

#define FT6336U_ADDRESS 0x38
#define FT6336U_REG_TOUCH_COUNT 0x02
#define FT6336U_REG_P1_X_HIGH 0x03
#define FT6336U_SAMPLE_BYTES 4

static const char *TAG = "board_touch";
static i2c_master_dev_handle_t s_touch_device;
static volatile board_touch_sample_t s_sample;
static TaskHandle_t s_task;

static void IRAM_ATTR touch_interrupt(void *argument)
{
    BaseType_t high_priority_woken = pdFALSE;
    (void)argument;
    vTaskNotifyGiveFromISR(s_task, &high_priority_woken);
    if (high_priority_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static esp_err_t read_register(uint8_t register_address, uint8_t *data, size_t data_length)
{
    return i2c_master_transmit_receive(s_touch_device, &register_address, 1, data, data_length, 50);
}

static void touch_task(void *argument)
{
    (void)argument;
    while (true) {
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
        uint8_t touch_count = 0;
        if (read_register(FT6336U_REG_TOUCH_COUNT, &touch_count, 1) != ESP_OK) {
            continue;
        }
        if ((touch_count & 0x0FU) == 0U) {
            s_sample.pressed = false;
            continue;
        }
        uint8_t data[FT6336U_SAMPLE_BYTES] = {0};
        if (read_register(FT6336U_REG_P1_X_HIGH, data, sizeof(data)) != ESP_OK) {
            continue;
        }
        const uint16_t x = (uint16_t)(((data[0] & 0x0FU) << 8U) | data[1]);
        const uint16_t y = (uint16_t)(((data[2] & 0x0FU) << 8U) | data[3]);
        s_sample.x = x < BOARD_LCD_H_RES ? x : BOARD_LCD_H_RES - 1U;
        s_sample.y = y < BOARD_LCD_V_RES ? y : BOARD_LCD_V_RES - 1U;
        s_sample.pressed = true;
    }
}

esp_err_t board_touch_init(void)
{
    const gpio_config_t reset_config = {
        .pin_bit_mask = 1ULL << BOARD_TOUCH_RST_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&reset_config), TAG, "touch reset GPIO setup failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(BOARD_TOUCH_RST_GPIO, 0), TAG, "touch reset low failed");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(gpio_set_level(BOARD_TOUCH_RST_GPIO, 1), TAG, "touch reset high failed");
    vTaskDelay(pdMS_TO_TICKS(300));

    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = BOARD_TOUCH_SDA_GPIO,
        .scl_io_num = BOARD_TOUCH_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false,
    };
    i2c_master_bus_handle_t bus_handle = NULL;
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &bus_handle), TAG, "I2C bus creation failed");
    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = FT6336U_ADDRESS,
        .scl_speed_hz = 400000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus_handle, &device_config, &s_touch_device), TAG, "FT6336U add device failed");

    if (xTaskCreate(touch_task, "touch", 3072, NULL, 8, &s_task) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    const gpio_config_t interrupt_config = {
        .pin_bit_mask = 1ULL << BOARD_TOUCH_INT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&interrupt_config), TAG, "touch INT GPIO setup failed");
    ESP_RETURN_ON_ERROR(gpio_install_isr_service(0), TAG, "GPIO ISR service setup failed");
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(BOARD_TOUCH_INT_GPIO, touch_interrupt, NULL), TAG, "touch ISR add failed");
    ESP_LOGI(TAG, "FT6336U initialized at 0x%02X", FT6336U_ADDRESS);
    return ESP_OK;
}

void board_touch_get_sample(board_touch_sample_t *out_sample)
{
    if (out_sample != NULL) {
        *out_sample = s_sample;
    }
}
