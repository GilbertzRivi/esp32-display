#include "w_toggle.h"
#include "../theme.h"
#include "../../callback.h"
#include <string.h>
#include <stdlib.h>

typedef struct {
    char on_action[64];
    char off_action[64];
    bool dispatch_enabled;
} toggle_ctx_t;

static void toggle_event_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    toggle_ctx_t *ctx = lv_obj_get_user_data(obj);
    if (!ctx || !ctx->dispatch_enabled) return;

    bool checked = lv_obj_has_state(obj, LV_STATE_CHECKED);
    if (checked && ctx->on_action[0])
        callback_dispatch(ctx->on_action);
    else if (!checked && ctx->off_action[0])
        callback_dispatch(ctx->off_action);
}

static void toggle_delete_cb(lv_event_t *e)
{
    toggle_ctx_t *ctx = lv_obj_get_user_data(lv_event_get_target(e));
    if (ctx) lv_mem_free(ctx);
}

lv_obj_t *w_toggle_create(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *obj = lv_switch_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    theme_apply(obj, "toggle");

    toggle_ctx_t *ctx = lv_mem_alloc(sizeof(*ctx));
    memset(ctx, 0, sizeof(*ctx));
    ctx->dispatch_enabled = true;
    lv_obj_set_user_data(obj, ctx);

    lv_obj_add_event_cb(obj, toggle_event_cb,  LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(obj, toggle_delete_cb, LV_EVENT_DELETE,        NULL);

    return obj;
}

void w_toggle_set_actions(lv_obj_t *obj, const char *on_action, const char *off_action)
{
    toggle_ctx_t *ctx = lv_obj_get_user_data(obj);
    if (!ctx) return;
    if (on_action)  strncpy(ctx->on_action,  on_action,  sizeof(ctx->on_action)  - 1);
    if (off_action) strncpy(ctx->off_action, off_action, sizeof(ctx->off_action) - 1);
}

void w_toggle_set_state(lv_obj_t *obj, bool state)
{
    toggle_ctx_t *ctx = lv_obj_get_user_data(obj);
    if (ctx) ctx->dispatch_enabled = false;
    if (state)
        lv_obj_add_state(obj, LV_STATE_CHECKED);
    else
        lv_obj_clear_state(obj, LV_STATE_CHECKED);
    if (ctx) ctx->dispatch_enabled = true;
}

void w_toggle_set_field(lv_obj_t *obj, int field_id, const char *val)
{
    if (field_id == W_TOGGLE_STATE)
        w_toggle_set_state(obj, atoi(val) != 0);
}
