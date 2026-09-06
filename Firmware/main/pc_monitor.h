#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "lvgl.h"

typedef enum {
    PC_LAUNCH_ACTION_VSCODE,
    PC_LAUNCH_ACTION_BILIBILI,
    PC_LAUNCH_ACTION_DOUYIN,
} pc_launch_action_t;

esp_err_t pc_monitor_create(lv_obj_t *parent);
void pc_monitor_refresh(void);
void pc_monitor_start(void);
void pc_monitor_set_visible(bool visible);
void pc_monitor_request_action(pc_launch_action_t action);
