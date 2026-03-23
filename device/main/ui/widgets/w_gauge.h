#pragma once

#include <lvgl.h>

typedef enum {
    W_GAUGE_VALUE = 0,
    W_GAUGE_MAX   = 1,
    W_GAUGE_LABEL = 2,
} w_gauge_field_t;

/*
 * Parametry z layout JSON — geometria wykresu.
 * rotation: poczatek luku w stopniach wzgledem godziny 12 (domyslnie 135 = lewy dolny rog)
 * sweep:    dlugosc luku w stopniach (domyslnie 270)
 * NULL -> uzywa wartosci domyslnych
 */
typedef struct {
    int rotation;
    int sweep;
} w_gauge_config_t;

#define W_GAUGE_CONFIG_DEFAULT { .rotation = 135, .sweep = 270 }

lv_obj_t *w_gauge_create(lv_obj_t *parent, int x, int y, int w, int h,
                          const w_gauge_config_t *cfg);
void w_gauge_set_value(lv_obj_t *obj, int value, int max);
void w_gauge_set_label(lv_obj_t *obj, const char *text);
void w_gauge_set_field(lv_obj_t *obj, int field_id, const char *val);
