#include "app_model.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static app_model_t s_model;
static SemaphoreHandle_t s_lock;

static void copy_text(char *destination, size_t destination_size, const char *source)
{
    if (source == NULL) {
        destination[0] = '\0';
        return;
    }
    strncpy(destination, source, destination_size - 1U);
    destination[destination_size - 1U] = '\0';
}

void app_model_init(void)
{
    memset(&s_model, 0, sizeof(s_model));
    s_model.wifi_state = APP_WIFI_UNCONFIGURED;
    s_model.backend_state = APP_BACKEND_UNCONFIGURED;
    s_model.brightness_percent = 55;
    s_model.rssi = -127;
    copy_text(s_model.status_line, sizeof(s_model.status_line), "Starting");
    s_lock = xSemaphoreCreateMutex();
}

void app_model_get(app_model_t *out_model)
{
    if ((out_model == NULL) || (s_lock == NULL)) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out_model = s_model;
    xSemaphoreGive(s_lock);
}

void app_model_set_wifi(app_wifi_state_t state, const char *ssid, const char *ip, int8_t rssi)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_model.wifi_state = state;
    copy_text(s_model.ssid, sizeof(s_model.ssid), ssid);
    copy_text(s_model.ip, sizeof(s_model.ip), ip);
    s_model.rssi = rssi;
    xSemaphoreGive(s_lock);
}

void app_model_set_backend(app_backend_state_t state, const char *endpoint)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_model.backend_state = state;
    copy_text(s_model.ha_endpoint, sizeof(s_model.ha_endpoint), endpoint);
    xSemaphoreGive(s_lock);
}

void app_model_set_time_synced(bool synced)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_model.time_synced = synced;
    xSemaphoreGive(s_lock);
}

void app_model_set_brightness(uint8_t percent)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_model.brightness_percent = percent;
    xSemaphoreGive(s_lock);
}

void app_model_set_status(const char *status)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    copy_text(s_model.status_line, sizeof(s_model.status_line), status);
    xSemaphoreGive(s_lock);
}
