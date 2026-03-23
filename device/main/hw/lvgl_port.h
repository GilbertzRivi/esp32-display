#pragma once

#include <esp_lcd_panel_ops.h>
#include <lvgl.h>

lv_disp_drv_t *lvgl_port_get_disp_drv(void);
void lvgl_port_init(esp_lcd_panel_handle_t panel);

/* Rekurencyjny mutex dla LVGL — wymagany gdy lv_timer_handler() jest
   wolany z innego taska niz kod tworzacy/modyfikujacy obiekty. */
void lvgl_port_lock(void);
void lvgl_port_unlock(void);
