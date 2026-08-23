#pragma once

#include <stdbool.h>
#include <stdint.h>

#define APP_SSID_MAX_LEN 32
#define APP_IP_MAX_LEN 16
#define APP_HA_ENDPOINT_MAX_LEN 128

typedef enum {
    APP_WIFI_UNCONFIGURED,
    APP_WIFI_PROVISIONING,
    APP_WIFI_CONNECTING,
    APP_WIFI_CONNECTED,
    APP_WIFI_FAILED,
} app_wifi_state_t;

typedef enum {
    APP_BACKEND_UNCONFIGURED,
    APP_BACKEND_PROBING,
    APP_BACKEND_ONLINE,
    APP_BACKEND_OFFLINE,
    APP_BACKEND_AUTH_FAILED,
} app_backend_state_t;

typedef struct {
    app_wifi_state_t wifi_state;
    app_backend_state_t backend_state;
    bool time_synced;
    uint8_t brightness_percent;
    int8_t rssi;
    char ssid[APP_SSID_MAX_LEN + 1];
    char ip[APP_IP_MAX_LEN];
    char ha_endpoint[APP_HA_ENDPOINT_MAX_LEN];
    char status_line[64];
} app_model_t;

void app_model_init(void);
void app_model_get(app_model_t *out_model);
void app_model_set_wifi(app_wifi_state_t state, const char *ssid, const char *ip, int8_t rssi);
void app_model_set_backend(app_backend_state_t state, const char *endpoint);
void app_model_set_time_synced(bool synced);
void app_model_set_brightness(uint8_t percent);
void app_model_set_status(const char *status);
