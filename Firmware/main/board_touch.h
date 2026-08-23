#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    bool pressed;
    uint16_t x;
    uint16_t y;
} board_touch_sample_t;

esp_err_t board_touch_init(void);
void board_touch_get_sample(board_touch_sample_t *out_sample);
