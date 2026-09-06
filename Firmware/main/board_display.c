#include "board_display.h"

#include <stdlib.h>

#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_st7789.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "board_pins.h"

#define LCD_SPI_CLOCK_HZ (10 * 1000 * 1000)
#define LCD_CLEAR_COLOR_RGB565 0x0000U

static const char *TAG = "board_display";
static esp_lcd_panel_handle_t s_panel;
static esp_lcd_panel_io_handle_t s_io;
static SemaphoreHandle_t s_transfer_done;

static bool on_color_transfer_done(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *event_data, void *user_context)
{
    (void)panel_io;
    (void)event_data;
    (void)user_context;
    BaseType_t high_priority_woken = pdFALSE;
    xSemaphoreGiveFromISR(s_transfer_done, &high_priority_woken);
    return high_priority_woken == pdTRUE;
}

static esp_err_t clear_panel(void)
{
    const size_t line_count = BOARD_LCD_DMA_LINES;
    const size_t pixel_count = BOARD_LCD_H_RES * line_count;
    uint16_t *pixels = heap_caps_calloc(pixel_count, sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (pixels == NULL) {
        return ESP_ERR_NO_MEM;
    }
    if (LCD_CLEAR_COLOR_RGB565 != 0U) {
        for (size_t index = 0; index < pixel_count; index++) {
            pixels[index] = LCD_CLEAR_COLOR_RGB565;
        }
    }
    esp_err_t result = ESP_OK;
    for (uint16_t y_start = 0; (y_start < BOARD_LCD_V_RES) && (result == ESP_OK); y_start += line_count) {
        const uint16_t y_end = y_start + line_count;
        result = esp_lcd_panel_draw_bitmap(s_panel, 0, y_start, BOARD_LCD_H_RES, y_end, pixels);
        if (result == ESP_OK) {
            board_display_wait_for_transfer();
        }
    }
    free(pixels);
    return result;
}

esp_err_t board_display_init(void)
{
    const spi_bus_config_t bus_config = {
        .sclk_io_num = BOARD_LCD_SCLK_GPIO,
        .mosi_io_num = BOARD_LCD_MOSI_GPIO,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BOARD_LCD_H_RES * BOARD_LCD_DMA_LINES * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO), TAG, "SPI bus initialization failed");
    s_transfer_done = xSemaphoreCreateBinary();
    if (s_transfer_done == NULL) {
        return ESP_ERR_NO_MEM;
    }

    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BOARD_LCD_DC_GPIO,
        .cs_gpio_num = BOARD_LCD_CS_GPIO,
        .pclk_hz = LCD_SPI_CLOCK_HZ,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &s_io), TAG,
                        "ST7789V SPI panel I/O creation failed");
    const esp_lcd_panel_io_callbacks_t callbacks = {
        .on_color_trans_done = on_color_transfer_done,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_register_event_callbacks(s_io, &callbacks, NULL), TAG,
                        "LCD transfer callback registration failed");

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BOARD_LCD_RST_GPIO,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(s_io, &panel_config, &s_panel), TAG, "ST7789V panel creation failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "panel reset failed");
    vTaskDelay(pdMS_TO_TICKS(120));
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "panel init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(s_panel, true), TAG, "panel landscape axis swap failed");
    /* Landscape transform for this panel mount: swap axes and mirror X only. */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(s_panel, true, false), TAG, "panel landscape mirror failed");
    vTaskDelay(pdMS_TO_TICKS(120));
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(s_panel, true), TAG, "panel invert setup failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), TAG, "panel display on failed");
    ESP_RETURN_ON_ERROR(clear_panel(), TAG, "panel clear failed");
    ESP_LOGI(TAG, "ST7789V ready: %dx%d, BGR RGB565, SPI %u Hz, black clear complete", BOARD_LCD_H_RES,
             BOARD_LCD_V_RES, LCD_SPI_CLOCK_HZ);
    return ESP_OK;
}

void board_display_wait_for_transfer(void)
{
    (void)xSemaphoreTake(s_transfer_done, pdMS_TO_TICKS(1000));
}

esp_lcd_panel_handle_t board_display_panel(void)
{
    return s_panel;
}

bool board_display_is_ready(void)
{
    return s_panel != NULL;
}
