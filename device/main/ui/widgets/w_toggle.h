#pragma once

#include <lvgl.h>
#include <stdbool.h>

typedef enum {
    W_TOGGLE_STATE = 0,
} w_toggle_field_t;

lv_obj_t *w_toggle_create(lv_obj_t *parent, int x, int y, int w, int h);

/* Akcje dispatchowane przez callback_dispatch przy zmianie stanu przez uzytkownika.
   Programowa zmiana stanu (w_toggle_set_state / W_TOGGLE_STATE z WebSocket)
   nie wyzwala akcji — zeby uniknac petli. */
void w_toggle_set_actions(lv_obj_t *obj, const char *on_action, const char *off_action);

void w_toggle_set_state(lv_obj_t *obj, bool state);
void w_toggle_set_field(lv_obj_t *obj, int field_id, const char *val);
