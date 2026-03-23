#pragma once

#include <lvgl.h>

typedef enum {
    W_ICON_TEXT = 0,
} w_icon_field_t;

lv_obj_t *w_icon_create(lv_obj_t *parent, int x, int y, int w, int h);
void w_icon_set_text(lv_obj_t *obj, const char *text);
void w_icon_set_field(lv_obj_t *obj, int field_id, const char *val);
