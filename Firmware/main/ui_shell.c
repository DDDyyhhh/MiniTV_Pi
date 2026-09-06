#include "ui_shell.h"

#include <stdio.h>
#include <string.h>

#include "lvgl.h"

#include "app_model.h"
#include "board_pins.h"
#include "card1.h"
#include "pc_monitor.h"
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
static lv_obj_t *s_time_tile;
static lv_obj_t *s_pc_tile;
static lv_obj_t *s_home_tile;
static lv_obj_t *s_clock_value;
static lv_obj_t *s_status_label;

static void set_label_text_if_changed(lv_obj_t *label, const char *text)
{
    if ((label != NULL) && (text != NULL) && (strcmp(lv_label_get_text(label), text) != 0)) {
        lv_label_set_text(label, text);
    }
}

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
    lv_obj_set_size(panel, BOARD_LCD_H_RES - 32, BOARD_LCD_V_RES - 58);
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
    lv_obj_set_width(subtitle_label, BOARD_LCD_H_RES - 56);
    lv_obj_align(subtitle_label, LV_ALIGN_TOP_LEFT, 0, 42);
    return panel;
}

static void tile_changed(lv_event_t *event)
{
    (void)event;
    const lv_obj_t *active = lv_tileview_get_tile_act(s_tileview);
    for (uint8_t index = 0; index < 3U; index++) {
        const lv_obj_t *tile = index == 0U ? s_home_tile : (index == 1U ? s_pc_tile : s_time_tile);
        lv_obj_set_style_bg_color(s_dots[index], tile == active ? COLOR_ACCENT : COLOR_BORDER, 0);
    }
    pc_monitor_set_visible(active == s_pc_tile);
    if (s_status_label != NULL) {
        if (active == s_pc_tile) {
            lv_obj_add_flag(s_status_label, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(s_status_label, LV_OBJ_FLAG_HIDDEN);
        }
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
    lv_obj_set_size(s_tileview, BOARD_LCD_H_RES, BOARD_LCD_V_RES);
    lv_obj_align(s_tileview, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_scrollbar_mode(s_tileview, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(s_tileview, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(s_tileview, tile_changed, LV_EVENT_VALUE_CHANGED, NULL);

    /* Keep the physical Card order: Smart Home, PC Monitor, Time. */
    s_time_tile = lv_tileview_add_tile(s_tileview, 2, 0, LV_DIR_HOR);
    (void)card1_create(s_time_tile);

    s_pc_tile = lv_tileview_add_tile(s_tileview, 1, 0, LV_DIR_HOR);
    if (pc_monitor_create(s_pc_tile) != ESP_OK) {
        ui_runtime_unlock();
        return ESP_ERR_NO_MEM;
    }

    s_home_tile = lv_tileview_add_tile(s_tileview, 0, 0, LV_DIR_HOR);
    (void)add_card_content(s_home_tile, "Smart Home Card", "Entity Tiles will be added in ticket 12.");

    for (uint8_t index = 0; index < 3U; index++) {
        s_dots[index] = lv_obj_create(screen);
        lv_obj_set_size(s_dots[index], 7, 7);
        lv_obj_set_style_radius(s_dots[index], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(s_dots[index], 0, 0);
        lv_obj_set_style_bg_color(s_dots[index], index == 2U ? COLOR_ACCENT : COLOR_BORDER, 0);
        lv_obj_align(s_dots[index], LV_ALIGN_BOTTOM_MID, (int32_t)(index - 1) * 16, -4);
    }
    lv_obj_set_tile_id(s_tileview, 2, 0, LV_ANIM_OFF);

    s_status_label = lv_label_create(screen);
    lv_obj_set_style_text_color(s_status_label, COLOR_SECONDARY, 0);
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_12, 0);
    lv_label_set_long_mode(s_status_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_status_label, 140);
    lv_obj_align(s_status_label, LV_ALIGN_TOP_RIGHT, -8, 4);


    lv_obj_t *hotzone = lv_obj_create(screen);
    lv_obj_set_size(hotzone, BOARD_LCD_H_RES, BOARD_CONTROL_CENTER_HOTZONE_PX);
    lv_obj_align(hotzone, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(hotzone, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hotzone, 0, 0);
    lv_obj_set_style_pad_all(hotzone, 0, 0);
    lv_obj_clear_flag(hotzone, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(hotzone, top_gesture, LV_EVENT_GESTURE, NULL);

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
        set_label_text_if_changed(s_clock_value, model.time_synced ? "Time synced" : "Not synced");
    }
    card1_refresh();
    pc_monitor_refresh();
    if (s_status_label != NULL) {
        if (lv_tileview_get_tile_act(s_tileview) == s_pc_tile) {
            lv_obj_add_flag(s_status_label, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(s_status_label, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (s_status_label != NULL) {
        set_label_text_if_changed(s_status_label, model.status_line);
    }
    ui_control_center_refresh();
}
