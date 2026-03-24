#pragma once

#include <lvgl.h>
#include "../theme.h"

typedef enum {
    W_GRAPH_VALUE = 0,
} w_graph_field_t;

/*
 * cfg == NULL  ->  uzywa theme_graph() jako domyslnego konfig.
 * cfg != NULL  ->  uzywa podanego konfig bez zmian.
 *
 * Wzorzec dla M3 widget_factory:
 *   graph_config_t cfg = *theme_graph();
 *   cfg.bar_width = 4;
 *   cfg.color_by_value = true;
 *   w_graph_create(parent, x, y, w, h, &cfg);
 */
lv_obj_t *w_graph_create(lv_obj_t *parent, int x, int y, int w, int h,
                          const graph_config_t *cfg);
void w_graph_set_color(lv_obj_t *obj, lv_color_t color);
void w_graph_set_range(lv_obj_t *obj, int min, int max);
void w_graph_push(lv_obj_t *obj, float value);
void w_graph_set_field(lv_obj_t *obj, int field_id, const char *val);
