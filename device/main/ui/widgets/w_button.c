#include "w_button.h"
#include "../../callback.h"
#include "../theme.h"
#include <string.h>
#include <stdlib.h>

typedef struct {
    char action[64];
} btn_event_action_t;

static void tap_dispatch_cb(lv_event_t *e)
{
    btn_event_action_t *ctx = lv_event_get_user_data(e);
    if (!ctx) return;
    if (!ctx->action[0]) return;
    callback_dispatch(ctx->action);
}

static void hold_dispatch_cb(lv_event_t *e)
{
    btn_event_action_t *ctx = lv_event_get_user_data(e);
    if (!ctx) return;
    if (!ctx->action[0]) return;
    callback_dispatch(ctx->action);
}

static void action_delete_cb(lv_event_t *e)
{
    void *p = lv_event_get_user_data(e);
    if (p) lv_mem_free(p);
}

lv_obj_t *w_button_create(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *obj = lv_btn_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    theme_apply(obj, "button");

    lv_obj_t *lbl = lv_label_create(obj);
    lv_obj_center(lbl);
    lv_label_set_text(lbl, "");

    return obj;
}

void w_button_set_label(lv_obj_t *obj, const char *text)
{
    lv_obj_t *lbl = lv_obj_get_child(obj, 0);
    if (lbl) lv_label_set_text(lbl, text ? text : "");
}

void w_button_set_on_tap(lv_obj_t *obj, lv_event_cb_t cb, void *user_data)
{
    lv_obj_add_event_cb(obj, cb, LV_EVENT_CLICKED, user_data);
}

void w_button_set_on_hold(lv_obj_t *obj, lv_event_cb_t cb, void *user_data)
{
    lv_obj_add_event_cb(obj, cb, LV_EVENT_LONG_PRESSED, user_data);
}

void w_button_set_tap_action(lv_obj_t *obj, const char *action)
{
    if (!obj || !action || !action[0]) return;

    btn_event_action_t *ctx = lv_mem_alloc(sizeof(*ctx));
    if (!ctx) return;

    memset(ctx, 0, sizeof(*ctx));
    strncpy(ctx->action, action, sizeof(ctx->action) - 1);

    lv_obj_add_event_cb(obj, tap_dispatch_cb, LV_EVENT_CLICKED, ctx);
    lv_obj_add_event_cb(obj, action_delete_cb, LV_EVENT_DELETE, ctx);
}

void w_button_set_hold_action(lv_obj_t *obj, const char *action)
{
    if (!obj || !action || !action[0]) return;

    btn_event_action_t *ctx = lv_mem_alloc(sizeof(*ctx));
    if (!ctx) return;

    memset(ctx, 0, sizeof(*ctx));
    strncpy(ctx->action, action, sizeof(ctx->action) - 1);

    lv_obj_add_event_cb(obj, hold_dispatch_cb, LV_EVENT_LONG_PRESSED, ctx);
    lv_obj_add_event_cb(obj, action_delete_cb, LV_EVENT_DELETE, ctx);
}

void w_button_set_field(lv_obj_t *obj, int field_id, const char *val)
{
    if (field_id == W_BUTTON_LABEL) {
        w_button_set_label(obj, val);
    }
}