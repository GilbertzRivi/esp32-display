#pragma once

#include <lvgl.h>

typedef enum {
    W_BUTTON_LABEL = 0,
} w_button_field_t;

lv_obj_t *w_button_create(lv_obj_t *parent, int x, int y, int w, int h);
void w_button_set_label(lv_obj_t *obj, const char *text);

/* C callbacki — dla kodu firmware */
void w_button_set_on_tap(lv_obj_t *obj, lv_event_cb_t cb, void *user_data);
void w_button_set_on_hold(lv_obj_t *obj, lv_event_cb_t cb, void *user_data);

/* String actions — dla widget_factory (layout JSON) */
void w_button_set_tap_action(lv_obj_t *obj, const char *action);
void w_button_set_hold_action(lv_obj_t *obj, const char *action);

void w_button_set_field(lv_obj_t *obj, int field_id, const char *val);
