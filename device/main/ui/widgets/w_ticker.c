#include "w_ticker.h"
#include "../theme.h"
#include <string.h>

typedef struct {
    char **frames;
    int count;
    int current;
    lv_timer_t *timer;
} ticker_ctx_t;

static void ticker_timer_cb(lv_timer_t *timer)
{
    lv_obj_t *obj = (lv_obj_t *)timer->user_data;
    ticker_ctx_t *ctx = lv_obj_get_user_data(obj);
    if (!ctx) return;
    ctx->current = (ctx->current + 1) % ctx->count;
    lv_label_set_text(obj, ctx->frames[ctx->current]);
}

static void ticker_ctx_free(ticker_ctx_t *ctx)
{
    if (!ctx) return;
    if (ctx->timer) lv_timer_del(ctx->timer);
    for (int i = 0; i < ctx->count; i++) lv_mem_free(ctx->frames[i]);
    lv_mem_free(ctx->frames);
    lv_mem_free(ctx);
}

static void ticker_delete_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    ticker_ctx_free(lv_obj_get_user_data(obj));
    lv_obj_set_user_data(obj, NULL);
}

lv_obj_t *w_ticker_create(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_CLIP);
    theme_apply(obj, "ticker");
    lv_obj_add_event_cb(obj, ticker_delete_cb, LV_EVENT_DELETE, NULL);
    return obj;
}

void w_ticker_start(lv_obj_t *obj, const char **frames, int count, int interval_ms)
{
    ticker_ctx_free(lv_obj_get_user_data(obj));

    ticker_ctx_t *ctx = lv_mem_alloc(sizeof(ticker_ctx_t));
    ctx->frames  = lv_mem_alloc(count * sizeof(char *));
    ctx->count   = count;
    ctx->current = 0;

    for (int i = 0; i < count; i++) {
        int len = strlen(frames[i]) + 1;
        ctx->frames[i] = lv_mem_alloc(len);
        memcpy(ctx->frames[i], frames[i], len);
    }

    ctx->timer = lv_timer_create(ticker_timer_cb, interval_ms, obj);
    lv_obj_set_user_data(obj, ctx);
    lv_label_set_text(obj, ctx->frames[0]);
}

void w_ticker_stop(lv_obj_t *obj)
{
    ticker_ctx_t *ctx = lv_obj_get_user_data(obj);
    if (!ctx || !ctx->timer) return;
    lv_timer_del(ctx->timer);
    ctx->timer = NULL;
}
