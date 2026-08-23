#include "ui_control_center.h"

#include <stdio.h>

#include "lvgl.h"

#include "app_model.h"
#include "board_backlight.h"
#include "config_store.h"
#include "provisioning_portal.h"
#include "ui_fonts.h"
#include "ui_runtime.h"

#define COLOR_PANEL lv_color_hex(0x1C1C1E)
#define COLOR_BORDER lv_color_hex(0x3A3A3C)
#define COLOR_PRIMARY lv_color_hex(0xF5F5F7)
#define COLOR_SECONDARY lv_color_hex(0xA1A1A6)
#define COLOR_ACCENT lv_color_hex(0x0A84FF)

static lv_obj_t *s_scrim;
static lv_obj_t *s_panel;
static lv_obj_t *s_network;
static lv_obj_t *s_backend;
static lv_obj_t *s_brightness;

static const char *wifi_text(app_wifi_state_t state)
{
    switch (state) {
    case APP_WIFI_CONNECTED: return "Wi-Fi connected";
    case APP_WIFI_CONNECTING: return "Wi-Fi connecting";
    case APP_WIFI_PROVISIONING: return "Provisioning portal";
    case APP_WIFI_FAILED: return "Wi-Fi failed";
    default: return "Wi-Fi not configured";
    }
}

static const char *backend_text(app_backend_state_t state)
{
    switch (state) {
    case APP_BACKEND_ONLINE: return "Backend online";
    case APP_BACKEND_AUTH_FAILED: return "Backend authentication failed";
    case APP_BACKEND_OFFLINE: return "Backend offline";
    case APP_BACKEND_PROBING: return "Backend probing";
    default: return "HA not configured";
    }
}

static void brightness_changed(lv_event_t *event)
{
    const uint8_t value = (uint8_t)lv_slider_get_value(lv_event_get_target(event));
    board_backlight_set(value);
    app_model_set_brightness(value);
}

static void brightness_released(lv_event_t *event)
{
    const uint8_t value = (uint8_t)lv_slider_get_value(lv_event_get_target(event));
    (void)config_store_save_brightness(value);
}

static void reprovision_clicked(lv_event_t *event)
{
    (void)event;
    provisioning_portal_request_start();
}

static void scrim_clicked(lv_event_t *event)
{
    (void)event;
    ui_control_center_set_visible(false);
}

void ui_control_center_create(void)
{
    lv_obj_t *screen = lv_scr_act();
    s_scrim = lv_obj_create(screen);
    lv_obj_set_size(s_scrim, LV_PCT(100), LV_PCT(100));
    lv_obj_align(s_scrim, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s_scrim, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_scrim, LV_OPA_50, 0);
    lv_obj_set_style_border_width(s_scrim, 0, 0);
    lv_obj_set_style_pad_all(s_scrim, 0, 0);
    lv_obj_clear_flag(s_scrim, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_scrim, scrim_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(s_scrim, LV_OBJ_FLAG_HIDDEN);

    s_panel = lv_obj_create(s_scrim);
    lv_obj_set_size(s_panel, 232, 208);
    lv_obj_align(s_panel, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_bg_color(s_panel, COLOR_PANEL, 0);
    lv_obj_set_style_border_color(s_panel, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(s_panel, 1, 0);
    lv_obj_set_style_radius(s_panel, 14, 0);
    lv_obj_set_style_pad_all(s_panel, 12, 0);
    lv_obj_clear_flag(s_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(s_panel);
    lv_label_set_text(title, "控制中心");
    lv_obj_set_style_text_color(title, COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(title, UI_FONT_CJK_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    s_brightness = lv_slider_create(s_panel);
    lv_obj_set_width(s_brightness, 194);
    lv_obj_align(s_brightness, LV_ALIGN_TOP_LEFT, 0, 40);
    lv_slider_set_range(s_brightness, 10, 100);
    lv_obj_set_style_bg_color(s_brightness, COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_add_event_cb(s_brightness, brightness_changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_brightness, brightness_released, LV_EVENT_RELEASED, NULL);

    s_network = lv_label_create(s_panel);
    lv_obj_set_style_text_color(s_network, COLOR_SECONDARY, 0);
    lv_obj_set_style_text_font(s_network, UI_FONT_BODY, 0);
    lv_obj_set_width(s_network, 200);
    lv_label_set_long_mode(s_network, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_network, LV_ALIGN_TOP_LEFT, 0, 76);

    s_backend = lv_label_create(s_panel);
    lv_obj_set_style_text_color(s_backend, COLOR_SECONDARY, 0);
    lv_obj_set_style_text_font(s_backend, UI_FONT_BODY, 0);
    lv_obj_set_width(s_backend, 200);
    lv_label_set_long_mode(s_backend, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_backend, LV_ALIGN_TOP_LEFT, 0, 120);

    lv_obj_t *button = lv_btn_create(s_panel);
    lv_obj_set_size(button, 120, 36);
    lv_obj_align(button, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(button, COLOR_ACCENT, 0);
    lv_obj_add_event_cb(button, reprovision_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, "Re-provision");
    lv_obj_center(label);
}

void ui_control_center_set_visible(bool visible)
{
    if ((s_scrim == NULL) || !ui_runtime_is_task()) {
        return;
    }
    if (visible) {
        lv_obj_clear_flag(s_scrim, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_scrim);
    } else {
        lv_obj_add_flag(s_scrim, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_control_center_toggle(void)
{
    if ((s_scrim == NULL) || !ui_runtime_is_task()) {
        return;
    }
    if (lv_obj_has_flag(s_scrim, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_clear_flag(s_scrim, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_scrim);
    } else {
        lv_obj_add_flag(s_scrim, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_control_center_refresh(void)
{
    if ((s_panel == NULL) || !ui_runtime_is_task()) {
        return;
    }
    app_model_t model;
    app_model_get(&model);
    char line[96];
    (void)snprintf(line, sizeof(line), "%s\n%s  %s  RSSI %d", wifi_text(model.wifi_state),
                   model.ssid[0] != '\0' ? model.ssid : "-", model.ip[0] != '\0' ? model.ip : "-", model.rssi);
    lv_label_set_text(s_network, line);
    (void)snprintf(line, sizeof(line), "%s", backend_text(model.backend_state));
    lv_label_set_text(s_backend, line);
    lv_slider_set_value(s_brightness, model.brightness_percent, LV_ANIM_OFF);
}
