#pragma once

#include <lvgl.h>

typedef enum {
    W_VALUE_TEXT = 0,
} w_value_field_t;

lv_obj_t *w_value_create(lv_obj_t *parent, int x, int y, int w, int h);
void w_value_set_text(lv_obj_t *obj, const char *text);
void w_value_set_field(lv_obj_t *obj, int field_id, const char *val);
