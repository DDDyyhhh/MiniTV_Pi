#include "board_backlight.h"

#include "driver/ledc.h"

#include "board_pins.h"

#define BACKLIGHT_PWM_FREQUENCY_HZ 25000U
#define BACKLIGHT_PWM_MAX_DUTY 1023U

static uint8_t s_percent;

esp_err_t board_backlight_init(uint8_t initial_percent)
{
    const ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = BACKLIGHT_PWM_FREQUENCY_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t result = ledc_timer_config(&timer_config);
    if (result != ESP_OK) {
        return result;
    }

    const ledc_channel_config_t channel_config = {
        .gpio_num = BOARD_LCD_BLK_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
        .flags.output_invert = 0,
    };
    result = ledc_channel_config(&channel_config);
    if (result == ESP_OK) {
        board_backlight_set(initial_percent);
    }
    return result;
}

void board_backlight_set(uint8_t percent)
{
    if (percent < 10U) {
        percent = 10U;
    }
    if (percent > 100U) {
        percent = 100U;
    }
    const uint32_t duty = (BACKLIGHT_PWM_MAX_DUTY * percent) / 100U;
    (void)ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    (void)ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    s_percent = percent;
}

uint8_t board_backlight_get(void)
{
    return s_percent;
}
