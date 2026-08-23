#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "esp_lcd_panel_ops.h"

esp_err_t board_display_init(void);
esp_lcd_panel_handle_t board_display_panel(void);
bool board_display_is_ready(void);
void board_display_wait_for_transfer(void);
