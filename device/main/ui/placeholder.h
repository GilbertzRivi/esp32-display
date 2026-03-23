#pragma once

#include <lvgl.h>

typedef void (*widget_set_field_fn)(lv_obj_t *obj, int field_id, const char *val);

void placeholder_init(void);
void placeholder_bind(const char *name, lv_obj_t *obj, int field_id, widget_set_field_fn fn);
void placeholder_update(const char *name, const char *val);
