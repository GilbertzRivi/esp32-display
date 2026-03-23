#include "w_button.h"
#include "../theme.h"
#include "../../callback.h"
#include <string.h>

typedef struct {
    char tap_action[64];
    char hold_action[64];
} btn_action_ctx_t;

static void tap_dispatch_cb(lv_event_t *e)
{
    btn_action_ctx_t *ctx = lv_obj_get_user_data(lv_event_get_target(e));
    if (ctx && ctx->tap_action[0]) callback_dispatch(ctx->tap_action);
}

static void hold_dispatch_cb(lv_event_t *e)
{
    btn_action_ctx_t *ctx = lv_obj_get_user_data(lv_event_get_target(e));
    if (ctx && ctx->hold_action[0]) callback_dispatch(ctx->hold_action);
}

static void btn_delete_cb(lv_event_t *e)
{
    btn_action_ctx_t *ctx = lv_obj_get_user_data(lv_event_get_target(e));
    if (ctx) lv_mem_free(ctx);
}

static btn_action_ctx_t *get_or_create_ctx(lv_obj_t *obj)
{
    btn_action_ctx_t *ctx = lv_obj_get_user_data(obj);
    if (!ctx) {
        ctx = lv_mem_alloc(sizeof(*ctx));
        memset(ctx, 0, sizeof(*ctx));
        lv_obj_set_user_data(obj, ctx);
        lv_obj_add_event_cb(obj, tap_dispatch_cb,  LV_EVENT_CLICKED,      NULL);
        lv_obj_add_event_cb(obj, hold_dispatch_cb, LV_EVENT_LONG_PRESSED, NULL);
        lv_obj_add_event_cb(obj, btn_delete_cb,    LV_EVENT_DELETE,       NULL);
    }
    return ctx;
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
    if (lbl) lv_label_set_text(lbl, text);
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
    btn_action_ctx_t *ctx = get_or_create_ctx(obj);
    strncpy(ctx->tap_action, action, sizeof(ctx->tap_action) - 1);
}

void w_button_set_hold_action(lv_obj_t *obj, const char *action)
{
    btn_action_ctx_t *ctx = get_or_create_ctx(obj);
    strncpy(ctx->hold_action, action, sizeof(ctx->hold_action) - 1);
}

void w_button_set_field(lv_obj_t *obj, int field_id, const char *val)
{
    if (field_id == W_BUTTON_LABEL) w_button_set_label(obj, val);
}
