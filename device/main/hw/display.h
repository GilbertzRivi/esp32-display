#pragma once

#include <esp_lcd_panel_ops.h>
#include <lvgl.h>

void display_spi_init(void);
esp_lcd_panel_handle_t display_panel_init(lv_disp_drv_t *disp_drv);
