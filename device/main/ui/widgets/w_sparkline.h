#pragma once

#include <lvgl.h>
#include "../theme.h"

typedef enum {
    W_SPARKLINE_VALUE = 0,
} w_sparkline_field_t;

/*
 * cfg == NULL  ->  uzywa theme_sparkline() jako domyslnego konfig.
 *
 * Wzorzec dla M3 widget_factory:
 *   sparkline_config_t cfg = *theme_sparkline();
 *   cfg.color = lv_color_hex(0xff2222);
 *   cfg.y_max = 90;
 *   w_sparkline_create(parent, x, y, w, h, &cfg);
 */
lv_obj_t *w_sparkline_create(lv_obj_t *parent, int x, int y, int w, int h,
                               const sparkline_config_t *cfg);
void w_sparkline_push(lv_obj_t *obj, float value);
void w_sparkline_set_field(lv_obj_t *obj, int field_id, const char *val);

/* Runtime override zakresu (np. gdy host dynamicznie zmienia skale) */
void w_sparkline_set_range(lv_obj_t *obj, int min, int max);
