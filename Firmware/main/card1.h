#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "esp_err.h"
#include "lvgl.h"

#define CARD1_FORECAST_POINTS 24

typedef struct {
    bool valid;
    bool stale;
    int16_t temperature_tenths[CARD1_FORECAST_POINTS];
    int16_t precipitation_tenths[CARD1_FORECAST_POINTS];
    uint8_t point_count;
    time_t fetched_at;
} card1_weather_snapshot_t;

esp_err_t card1_create(lv_obj_t *parent);
void card1_refresh(void);
void card1_start(void);
void card1_get_weather(card1_weather_snapshot_t *snapshot);
