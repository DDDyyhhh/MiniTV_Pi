#pragma once

#include <stdint.h>

#include "esp_err.h"

esp_err_t board_backlight_init(uint8_t initial_percent);
void board_backlight_set(uint8_t percent);
uint8_t board_backlight_get(void);
