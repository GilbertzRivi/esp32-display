#pragma once

#include <lvgl.h>

typedef enum {
    W_BAR_VALUE = 0,
    W_BAR_MAX   = 1,
    W_BAR_LABEL = 2,
} w_bar_field_t;

lv_obj_t *w_bar_create(lv_obj_t *parent, int x, int y, int w, int h);
void w_bar_set_value(lv_obj_t *obj, int value, int max);
void w_bar_set_label(lv_obj_t *obj, const char *text);
void w_bar_set_field(lv_obj_t *obj, int field_id, const char *val);
