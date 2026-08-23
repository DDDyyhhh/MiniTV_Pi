#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

esp_err_t ui_runtime_init(void);
esp_err_t ui_runtime_start(void);
void ui_runtime_request_refresh(void);
void ui_runtime_request_control_center_toggle(void);
bool ui_runtime_is_task(void);
uint16_t ui_runtime_fps(void);
uint16_t ui_runtime_buffer_lines(void);
uint32_t ui_runtime_buffer_bytes(void);
bool ui_runtime_lock(uint32_t timeout_ms);
void ui_runtime_unlock(void);
