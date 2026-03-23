#pragma once

#include <lvgl.h>

void screen_mgr_init(void);
void screen_mgr_add(const char *id, lv_obj_t *screen);
void screen_mgr_show(const char *id);
