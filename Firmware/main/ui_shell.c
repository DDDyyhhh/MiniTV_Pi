#include "ui_shell.h"

#include <stdio.h>

#include "lvgl.h"

#include "app_model.h"
#include "board_pins.h"
#include "ui_control_center.h"
#include "ui_fonts.h"
#include "ui_runtime.h"

#define COLOR_BG lv_color_hex(0x121214)
#define COLOR_PANEL lv_color_hex(0x1C1C1E)
#define COLOR_BORDER lv_color_hex(0x3A3A3C)
#define COLOR_PRIMARY lv_color_hex(0xF5F5F7)
#define COLOR_SECONDARY lv_color_hex(0xA1A1A6)
#define COLOR_ACCENT lv_color_hex(0x0A84FF)

static lv_obj_t *s_tileview;
static lv_obj_t *s_dots[3];
static lv_obj_t *s_clock_value;
static lv_obj_t *s_status_label;

static void apply_panel_style(lv_obj_t *object)
{
    lv_obj_set_style_bg_color(object, COLOR_PANEL, 0);
    lv_obj_set_style_border_color(object, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(object, 1, 0);
    lv_obj_set_style_radius(object, 14, 0);
    lv_obj_set_style_pad_all(object, 12, 0);
}

static lv_obj_t *add_card_content(lv_obj_t *tile, const char *title, const char *subtitle)
{
    lv_obj_t *panel = lv_obj_create(tile);
    lv_obj_set_size(panel, 216, 250);
    lv_obj_align(panel, LV_ALIGN_BOTTOM_MID, 0, -14);
    apply_panel_style(panel);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_label = lv_label_create(panel);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_color(title_label, COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(title_label, UI_FONT_TITLE, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 0, 2);

    lv_obj_t *subtitle_label = lv_label_create(panel);
    lv_label_set_text(subtitle_label, subtitle);
    lv_obj_set_style_text_color(subtitle_label, COLOR_SECONDARY, 0);
    lv_obj_set_style_text_font(subtitle_label, UI_FONT_BODY, 0);
    lv_label_set_long_mode(subtitle_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(subtitle_label, 188);
    lv_obj_align(subtitle_label, LV_ALIGN_TOP_LEFT, 0, 42);
    return panel;
}

static void tile_changed(lv_event_t *event)
{
    (void)event;
    const lv_obj_t *active = lv_tileview_get_tile_act(s_tileview);
    for (uint8_t index = 0; index < 3U; index++) {
        const lv_obj_t *tile = lv_obj_get_child(s_tileview, index);
        lv_obj_set_style_bg_color(s_dots[index], tile == active ? COLOR_ACCENT : COLOR_BORDER, 0);
    }
}

static void top_gesture(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_GESTURE) {
        return;
    }
    const lv_dir_t direction = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (direction == LV_DIR_BOTTOM) {
        ui_control_center_set_visible(true);
    }
}

esp_err_t ui_shell_create(void)
{
    if (!ui_runtime_lock(1000)) {
        return ESP_ERR_TIMEOUT;
    }
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    s_tileview = lv_tileview_create(screen);
    lv_obj_set_size(s_tileview, 240, 320);
    lv_obj_align(s_tileview, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_scrollbar_mode(s_tileview, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(s_tileview, tile_changed, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *time_tile = lv_tileview_add_tile(s_tileview, 0, 0, LV_DIR_HOR);
    lv_obj_t *time_panel = add_card_content(time_tile, "Time Card", "Time, weather, and calendar are not configured yet.");
    s_clock_value = lv_label_create(time_panel);
    lv_label_set_text(s_clock_value, "--:--");
    lv_obj_set_style_text_color(s_clock_value, COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(s_clock_value, UI_FONT_CLOCK, 0);
    lv_obj_align(s_clock_value, LV_ALIGN_BOTTOM_MID, 0, -18);

    lv_obj_t *pc_tile = lv_tileview_add_tile(s_tileview, 1, 0, LV_DIR_HOR);
    (void)add_card_content(pc_tile, "PC Monitor Card", "Confirmed Telemetry and Launch Actions will be added in ticket 11.");

    lv_obj_t *home_tile = lv_tileview_add_tile(s_tileview, 2, 0, LV_DIR_HOR);
    (void)add_card_content(home_tile, "Smart Home Card", "Entity Tiles will be added in ticket 12.");

    for (uint8_t index = 0; index < 3U; index++) {
        s_dots[index] = lv_obj_create(screen);
        lv_obj_set_size(s_dots[index], 7, 7);
        lv_obj_set_style_radius(s_dots[index], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(s_dots[index], 0, 0);
        lv_obj_set_style_bg_color(s_dots[index], index == 0U ? COLOR_ACCENT : COLOR_BORDER, 0);
        lv_obj_align(s_dots[index], LV_ALIGN_BOTTOM_MID, (int32_t)(index - 1) * 16, -4);
    }

    lv_obj_t *hotzone = lv_obj_create(screen);
    lv_obj_set_size(hotzone, 240, BOARD_CONTROL_CENTER_HOTZONE_PX);
    lv_obj_align(hotzone, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(hotzone, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hotzone, 0, 0);
    lv_obj_set_style_pad_all(hotzone, 0, 0);
    lv_obj_clear_flag(hotzone, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(hotzone, top_gesture, LV_EVENT_GESTURE, NULL);

    s_status_label = lv_label_create(screen);
    lv_obj_set_style_text_color(s_status_label, COLOR_SECONDARY, 0);
    lv_obj_set_style_text_font(s_status_label, UI_FONT_BODY, 0);
    lv_obj_align(s_status_label, LV_ALIGN_TOP_MID, 0, 3);
    ui_control_center_create();
    ui_runtime_unlock();
    ui_shell_refresh();
    return ESP_OK;
}

void ui_shell_refresh(void)
{
    app_model_t model;
    app_model_get(&model);
    if (s_clock_value != NULL) {
        lv_label_set_text(s_clock_value, model.time_synced ? "Time synced" : "Not synced");
    }
    if (s_status_label != NULL) {
        lv_label_set_text(s_status_label, model.status_line);
    }
    ui_control_center_refresh();
}
