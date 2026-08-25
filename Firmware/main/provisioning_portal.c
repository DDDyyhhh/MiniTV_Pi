#include "provisioning_portal.h"
#include "esp_check.h"

#include <ctype.h>
#include "esp_check.h"
#include <string.h>
#include "esp_check.h"

#include "esp_http_server.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_check.h"

#include "app_model.h"
#include "esp_check.h"
#include "config_store.h"
#include "esp_check.h"
#include "network_manager.h"
#include "esp_check.h"
#include "ui_runtime.h"
#include "esp_check.h"

#define REQUEST_MAX_BYTES 768

static const char *TAG = "portal";
static httpd_handle_t s_server;

static int hex_value(char character)
{
    if ((character >= '0') && (character <= '9')) {
        return character - '0';
    }
    character = (char)tolower((unsigned char)character);
    return ((character >= 'a') && (character <= 'f')) ? character - 'a' + 10 : -1;
}

static bool decode_form_value(const char *body, const char *key, char *output, size_t output_size)
{
    const size_t key_length = strlen(key);
    const char *value = body;
    while ((value = strstr(value, key)) != NULL) {
        if (((value == body) || (value[-1] == '&')) && (value[key_length] == '=')) {
            value += key_length + 1U;
            size_t written = 0;
            while ((*value != '\0') && (*value != '&') && (written + 1U < output_size)) {
                if ((*value == '+')) {
                    output[written++] = ' ';
                    value++;
                } else if ((*value == '%') && (hex_value(value[1]) >= 0) && (hex_value(value[2]) >= 0)) {
                    output[written++] = (char)((hex_value(value[1]) << 4) | hex_value(value[2]));
                    value += 3;
                } else {
                    output[written++] = *value++;
                }
            }
            output[written] = '\0';
            return true;
        }
        value += key_length;
    }
    return false;
}

static esp_err_t root_handler(httpd_req_t *request)
{
    static const char page[] =
        "<!doctype html><html><meta name=viewport content='width=device-width,initial-scale=1'>"
        "<title>Mini TV setup</title><body><h2>Mini TV Provisioning Portal</h2>"
        "<form method=post action=/save><label>Wi-Fi SSID <input name=ssid maxlength=32 required></label><br>"
        "<label>Wi-Fi password <input name=password type=password maxlength=64></label><br>"
        "<label>HA endpoint <input name=endpoint maxlength=127 placeholder=https://ha.lan:8123></label><br>"
        "<label>HA token <input name=token type=password maxlength=255></label><br>"
        "<label>Weather entity <input name=weather maxlength=63 placeholder=weather.home></label><br>"
        "<button>Save and connect</button></form></body></html>";
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    return httpd_resp_send(request, page, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t save_handler(httpd_req_t *request)
{
    if ((request->content_len <= 0) || (request->content_len >= REQUEST_MAX_BYTES)) {
        httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid request size");
        return ESP_FAIL;
    }
    char body[REQUEST_MAX_BYTES] = {0};
    int received = 0;
    while (received < request->content_len) {
        const int result = httpd_req_recv(request, body + received, request->content_len - received);
        if (result <= 0) {
            return ESP_FAIL;
        }
        received += result;
    }
    app_config_t config = {0};
    (void)decode_form_value(body, "ssid", config.wifi_ssid, sizeof(config.wifi_ssid));
    (void)decode_form_value(body, "password", config.wifi_password, sizeof(config.wifi_password));
    (void)decode_form_value(body, "endpoint", config.ha_endpoint, sizeof(config.ha_endpoint));
    (void)decode_form_value(body, "token", config.ha_token, sizeof(config.ha_token));
    (void)decode_form_value(body, "weather", config.weather_entity, sizeof(config.weather_entity));
    config.brightness_percent = 55;
    if ((config.wifi_ssid[0] == '\0') || !config_store_endpoint_is_valid(config.ha_endpoint)) {
        httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "SSID or endpoint is invalid");
        return ESP_FAIL;
    }
    if (config_store_save(&config) != ESP_OK) {
        httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Configuration was not saved");
        return ESP_FAIL;
    }
    app_model_set_status("Configuration saved; connecting");
    ui_runtime_request_refresh();
    (void)network_manager_connect(&config);
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    return httpd_resp_sendstr(request, "<h2>Saved</h2><p>Mini TV is connecting. The token is never shown here.</p>");
}

esp_err_t provisioning_portal_start(void)
{
    if (s_server != NULL) {
        return ESP_OK;
    }
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 2;
    config.stack_size = 4096;
    ESP_RETURN_ON_ERROR(httpd_start(&s_server, &config), TAG, "HTTP server start failed");
    const httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = root_handler};
    const httpd_uri_t save = {.uri = "/save", .method = HTTP_POST, .handler = save_handler};
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &root), TAG, "root handler register failed");
    return httpd_register_uri_handler(s_server, &save);
}

void provisioning_portal_request_start(void)
{
    network_manager_start_provisioning();
}
