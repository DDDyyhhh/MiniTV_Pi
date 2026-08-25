#include "ui_runtime.h"

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"

#include "board_display.h"
#include "board_pins.h"
#include "board_touch.h"
#include "ui_control_center.h"
#include "ui_shell.h"

static const char *TAG = "ui_runtime";
static lv_disp_draw_buf_t s_draw_buffer;
static lv_disp_drv_t s_display_driver;
static lv_indev_drv_t s_input_driver;
static lv_color_t *s_buffer_1;
static lv_color_t *s_buffer_2;
static SemaphoreHandle_t s_lvgl_lock;
static uint16_t s_fps;
static uint32_t s_flushes;
static int64_t s_last_fps_us;
static bool s_started;
static TaskHandle_t s_ui_task_handle;
static volatile bool s_toggle_control_center;

static void display_wait(lv_disp_drv_t *display_driver)
{
    (void)display_driver;
    board_display_wait_for_transfer();
    lv_disp_flush_ready(&s_display_driver);
}

static void display_flush(lv_disp_drv_t *display_driver, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel = board_display_panel();
    if (panel == NULL) {
        lv_disp_flush_ready(display_driver);
        return;
    }
    if (esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, color_map) != ESP_OK) {
        lv_disp_flush_ready(display_driver);
    }
}

static void touch_read(lv_indev_drv_t *input_driver, lv_indev_data_t *data)
{
    (void)input_driver;
    board_touch_sample_t sample = {0};
    board_touch_get_sample(&sample);
    data->state = sample.pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
    data->point.x = sample.x;
    data->point.y = sample.y;
}

static void lvgl_tick(void *argument)
{
    (void)argument;
    lv_tick_inc(5);
}

static void ui_task(void *argument)
{
    (void)argument;
    s_ui_task_handle = xTaskGetCurrentTaskHandle();
    s_last_fps_us = esp_timer_get_time();
    while (true) {
        if (xSemaphoreTake(s_lvgl_lock, portMAX_DELAY) == pdTRUE) {
            if (s_toggle_control_center) {
                s_toggle_control_center = false;
                ESP_LOGI(TAG, "[DEBUG-button] applying Control Center toggle");
                ui_control_center_toggle();
            }
            ui_shell_refresh();
            (void)lv_timer_handler();
            xSemaphoreGive(s_lvgl_lock);
        }
        s_flushes++;
        const int64_t now = esp_timer_get_time();
        if ((now - s_last_fps_us) >= 1000000) {
            s_fps = (uint16_t)s_flushes;
            s_flushes = 0;
            s_last_fps_us = now;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

esp_err_t ui_runtime_init(void)
{
    lv_init();
    s_lvgl_lock = xSemaphoreCreateMutex();
    if (s_lvgl_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }

    const size_t buffer_pixels = BOARD_LCD_H_RES * BOARD_LCD_DMA_LINES;
    const size_t buffer_bytes = buffer_pixels * sizeof(lv_color_t);
    s_buffer_1 = heap_caps_malloc(buffer_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    s_buffer_2 = heap_caps_malloc(buffer_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if ((s_buffer_1 == NULL) || (s_buffer_2 == NULL)) {
        return ESP_ERR_NO_MEM;
    }
    lv_disp_draw_buf_init(&s_draw_buffer, s_buffer_1, s_buffer_2, buffer_pixels);

    lv_disp_drv_init(&s_display_driver);
    s_display_driver.hor_res = BOARD_LCD_H_RES;
    s_display_driver.ver_res = BOARD_LCD_V_RES;
    s_display_driver.flush_cb = display_flush;
    s_display_driver.wait_cb = display_wait;
    s_display_driver.draw_buf = &s_draw_buffer;
    if (lv_disp_drv_register(&s_display_driver) == NULL) {
        return ESP_ERR_NO_MEM;
    }

    lv_indev_drv_init(&s_input_driver);
    s_input_driver.type = LV_INDEV_TYPE_POINTER;
    s_input_driver.read_cb = touch_read;
    if (lv_indev_drv_register(&s_input_driver) == NULL) {
        return ESP_ERR_NO_MEM;
    }

    const esp_timer_create_args_t tick_args = {.callback = lvgl_tick, .name = "lvgl_tick"};
    esp_timer_handle_t tick_timer = NULL;
    ESP_RETURN_ON_ERROR(esp_timer_create(&tick_args, &tick_timer), TAG, "LVGL timer create failed");
    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(tick_timer, 5000), TAG, "LVGL timer start failed");
    ESP_LOGI(TAG, "LVGL %d.%d.%d registered: DMA double buffer %u lines, %u bytes", LVGL_VERSION_MAJOR,
             LVGL_VERSION_MINOR, LVGL_VERSION_PATCH, BOARD_LCD_DMA_LINES, (unsigned)(buffer_bytes * 2U));
    return ESP_OK;
}

esp_err_t ui_runtime_start(void)
{
    if (s_started) {
        return ESP_OK;
    }
    if (xTaskCreate(ui_task, "lvgl", 6144, NULL, 6, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    s_started = true;
    return ESP_OK;
}

void ui_runtime_request_refresh(void)
{
    if (s_ui_task_handle != NULL) {
        xTaskNotifyGive(s_ui_task_handle);
    }
}

void ui_runtime_request_control_center_toggle(void)
{
    s_toggle_control_center = true;
    ui_runtime_request_refresh();
}

bool ui_runtime_is_task(void)
{
    return xTaskGetCurrentTaskHandle() == s_ui_task_handle;
}

uint16_t ui_runtime_fps(void)
{
    return s_fps;
}

uint16_t ui_runtime_buffer_lines(void)
{
    return BOARD_LCD_DMA_LINES;
}

uint32_t ui_runtime_buffer_bytes(void)
{
    return BOARD_LCD_H_RES * BOARD_LCD_DMA_LINES * sizeof(lv_color_t) * 2U;
}

bool ui_runtime_lock(uint32_t timeout_ms)
{
    return xSemaphoreTake(s_lvgl_lock, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void ui_runtime_unlock(void)
{
    xSemaphoreGive(s_lvgl_lock);
}
