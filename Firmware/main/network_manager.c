#include "network_manager.h"
#include "esp_check.h"

#include <stdio.h>
#include "esp_check.h"
#include <string.h>
#include "esp_check.h"

#include "esp_event.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_mac.h"
#include "esp_check.h"
#include "esp_netif.h"
#include "esp_check.h"
#include "esp_wifi.h"
#include "esp_check.h"
#include "lwip/inet.h"
#include "esp_check.h"

#include "app_model.h"
#include "esp_check.h"
#include "backend_probe.h"
#include "card1.h"
#include "pc_monitor.h"
#include "esp_check.h"
#include "provisioning_portal.h"
#include "esp_check.h"
#include "time_service.h"
#include "esp_check.h"
#include "ui_runtime.h"
#include "esp_check.h"

static const char *TAG = "network";
static esp_netif_t *s_sta_netif;
static esp_netif_t *s_ap_netif;
static uint8_t s_retry_count;
static bool s_initialized;

static void refresh_ui(void)
{
    ui_runtime_request_refresh();
}

static void wifi_event_handler(void *argument, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)argument;
    (void)event_base;
    (void)event_data;
    if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *disconnected = event_data;
        ESP_LOGW(TAG, "STA disconnected: reason=%d retry=%u", disconnected != NULL ? disconnected->reason : -1, s_retry_count);
        if (s_retry_count < 5U) {
            s_retry_count++;
            app_model_set_wifi(APP_WIFI_CONNECTING, NULL, NULL, -127);
            (void)esp_wifi_connect();
        } else {
            app_model_set_wifi(APP_WIFI_FAILED, NULL, NULL, -127);
            network_manager_start_provisioning();
        }
        refresh_ui();
    }
}

static void ip_event_handler(void *argument, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)argument;
    (void)event_base;
    if (event_id != IP_EVENT_STA_GOT_IP) {
        return;
    }
    const ip_event_got_ip_t *event = event_data;
    char ip[APP_IP_MAX_LEN] = {0};
    (void)esp_ip4addr_ntoa(&event->ip_info.ip, ip, sizeof(ip));
    wifi_ap_record_t access_point = {0};
    const int8_t rssi = esp_wifi_sta_get_ap_info(&access_point) == ESP_OK ? access_point.rssi : -127;
    app_model_t model;
    app_model_get(&model);
    app_model_set_wifi(APP_WIFI_CONNECTED, model.ssid, ip, rssi);
    s_retry_count = 0;
    time_service_start();
    backend_probe_request_now();
    card1_start();
    pc_monitor_start();
    refresh_ui();
}

esp_err_t network_manager_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif init failed");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop init failed");
    s_sta_netif = esp_netif_create_default_wifi_sta();
    s_ap_netif = esp_netif_create_default_wifi_ap();
    const wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&wifi_init_config), TAG, "Wi-Fi init failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL), TAG, "Wi-Fi event register failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, ip_event_handler, NULL), TAG, "IP event register failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "Wi-Fi STA mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Wi-Fi start failed");
    s_initialized = true;
    return ESP_OK;
}

esp_err_t network_manager_connect(const app_config_t *config)
{
    if ((config == NULL) || (config->wifi_ssid[0] == '\0')) {
        return ESP_ERR_INVALID_ARG;
    }
    wifi_config_t wifi_config = {0};
    memcpy(wifi_config.sta.ssid, config->wifi_ssid, strlen(config->wifi_ssid));
    memcpy(wifi_config.sta.password, config->wifi_password, strlen(config->wifi_password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "STA mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG, "STA configuration failed");
    app_model_set_wifi(APP_WIFI_CONNECTING, config->wifi_ssid, NULL, -127);
    app_model_set_backend(config->ha_endpoint[0] == '\0' ? APP_BACKEND_UNCONFIGURED : APP_BACKEND_PROBING, config->ha_endpoint);
    refresh_ui();
    const esp_err_t result = esp_wifi_connect();
    if (result != ESP_OK) {
        app_model_set_wifi(APP_WIFI_FAILED, config->wifi_ssid, NULL, -127);
        refresh_ui();
    }
    return result;
}

void network_manager_start_provisioning(void)
{
    uint8_t mac[6];
    char access_point_name[33];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    (void)snprintf(access_point_name, sizeof(access_point_name), "MiniTV-%02X%02X", mac[4], mac[5]);
    wifi_config_t access_point_config = {0};
    memcpy(access_point_config.ap.ssid, access_point_name, strlen(access_point_name));
    access_point_config.ap.ssid_len = strlen(access_point_name);
    access_point_config.ap.channel = 1;
    access_point_config.ap.max_connection = 4;
    access_point_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    memcpy(access_point_config.ap.password, "minitv-setup", sizeof("minitv-setup"));
    if (esp_wifi_set_mode(WIFI_MODE_APSTA) == ESP_OK) {
        (void)esp_wifi_set_config(WIFI_IF_AP, &access_point_config);
        app_model_set_wifi(APP_WIFI_PROVISIONING, access_point_name, "192.168.4.1", -127);
        provisioning_portal_start();
        refresh_ui();
        ESP_LOGI(TAG, "Provisioning AP %s started at 192.168.4.1", access_point_name);
    }
}
