#include "config_store.h"
#include "esp_check.h"

#include <string.h>
#include "esp_check.h"

#include "nvs.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "esp_check.h"

#define NVS_NAMESPACE "minitv"
#define NVS_KEY_CONFIG "config"
#define CONFIG_VERSION 1U

typedef struct {
    uint32_t version;
    app_config_t data;
} persisted_config_t;

esp_err_t config_store_init(void)
{
    esp_err_t result = nvs_flash_init();
    if ((result == ESP_ERR_NVS_NO_FREE_PAGES) || (result == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }
    return result;
}

bool config_store_load(app_config_t *out_config)
{
    if (out_config == NULL) {
        return false;
    }
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }
    persisted_config_t stored = {0};
    size_t size = sizeof(stored);
    const esp_err_t result = nvs_get_blob(handle, NVS_KEY_CONFIG, &stored, &size);
    nvs_close(handle);
    if ((result != ESP_OK) || (size != sizeof(stored)) || (stored.version != CONFIG_VERSION) ||
        (stored.data.wifi_ssid[0] == '\0')) {
        return false;
    }
    stored.data.wifi_ssid[CONFIG_SSID_MAX_LEN] = '\0';
    stored.data.wifi_password[CONFIG_PASSWORD_MAX_LEN] = '\0';
    stored.data.ha_endpoint[CONFIG_ENDPOINT_MAX_LEN] = '\0';
    stored.data.ha_token[CONFIG_TOKEN_MAX_LEN] = '\0';
    *out_config = stored.data;
    return true;
}

esp_err_t config_store_save(const app_config_t *config)
{
    if ((config == NULL) || (config->wifi_ssid[0] == '\0')) {
        return ESP_ERR_INVALID_ARG;
    }
    persisted_config_t stored = {.version = CONFIG_VERSION, .data = *config};
    nvs_handle_t handle;
    ESP_RETURN_ON_ERROR(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle), "config", "NVS open failed");
    esp_err_t result = nvs_set_blob(handle, NVS_KEY_CONFIG, &stored, sizeof(stored));
    if (result == ESP_OK) {
        result = nvs_commit(handle);
    }
    nvs_close(handle);
    return result;
}

esp_err_t config_store_save_brightness(uint8_t brightness_percent)
{
    app_config_t config = {0};
    if (!config_store_load(&config)) {
        return ESP_ERR_NOT_FOUND;
    }
    config.brightness_percent = brightness_percent;
    return config_store_save(&config);
}

esp_err_t config_store_clear(void)
{
    nvs_handle_t handle;
    ESP_RETURN_ON_ERROR(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle), "config", "NVS open failed");
    const esp_err_t result = nvs_erase_all(handle);
    if (result == ESP_OK) {
        (void)nvs_commit(handle);
    }
    nvs_close(handle);
    return result;
}

bool config_store_endpoint_is_valid(const char *endpoint)
{
    if ((endpoint == NULL) || (endpoint[0] == '\0')) {
        return true;
    }
    const char *host = NULL;
    if (strncmp(endpoint, "http://", 7) == 0) {
        host = endpoint + 7;
    } else if (strncmp(endpoint, "https://", 8) == 0) {
        host = endpoint + 8;
    }
    return (host != NULL) && (host[0] != '\0') && (strchr(host, '/') == NULL) && (strchr(endpoint, '@') == NULL) &&
           (strchr(endpoint, '?') == NULL) && (strchr(endpoint, '#') == NULL) &&
           (strlen(endpoint) <= CONFIG_ENDPOINT_MAX_LEN);
}
