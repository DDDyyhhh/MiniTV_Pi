#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define CONFIG_SSID_MAX_LEN 32
#define CONFIG_PASSWORD_MAX_LEN 64
#define CONFIG_ENDPOINT_MAX_LEN 127
#define CONFIG_TOKEN_MAX_LEN 255

typedef struct {
    char wifi_ssid[CONFIG_SSID_MAX_LEN + 1];
    char wifi_password[CONFIG_PASSWORD_MAX_LEN + 1];
    char ha_endpoint[CONFIG_ENDPOINT_MAX_LEN + 1];
    char ha_token[CONFIG_TOKEN_MAX_LEN + 1];
    uint8_t brightness_percent;
} app_config_t;

esp_err_t config_store_init(void);
bool config_store_load(app_config_t *out_config);
esp_err_t config_store_save(const app_config_t *config);
esp_err_t config_store_save_brightness(uint8_t brightness_percent);
esp_err_t config_store_clear(void);
bool config_store_endpoint_is_valid(const char *endpoint);
