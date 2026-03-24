#include "w_image.h"
#include "../theme.h"
#include <esp_log.h>
#include <stdlib.h>
#include <string.h>

#define TAG "w_image"

typedef struct {
    lv_img_dsc_t dsc;
    uint8_t     *pixels;
} img_ctx_t;

static void img_ctx_free(img_ctx_t *ctx)
{
    if (!ctx) return;
    free(ctx->pixels);
    ctx->pixels = NULL;
    free(ctx);
}

static void img_delete_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    img_ctx_t *ctx = lv_obj_get_user_data(obj);

    lv_obj_set_user_data(obj, NULL);
    img_ctx_free(ctx);
}

lv_obj_t *w_image_create(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *obj = lv_img_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_img_set_zoom(obj, 256);
    lv_obj_add_event_cb(obj, img_delete_cb, LV_EVENT_DELETE, NULL);
    return obj;
}

bool w_image_set_buf(lv_obj_t *obj, uint16_t img_w, uint16_t img_h, uint8_t *pixels)
{
    if (!obj || !pixels || img_w == 0 || img_h == 0) {
        free(pixels);
        return false;
    }

    size_t pixel_bytes = (size_t)img_w * img_h * sizeof(lv_color_t);

    img_ctx_t *ctx = malloc(sizeof(*ctx));
    if (!ctx) {
        ESP_LOGE(TAG, "malloc ctx failed");
        free(pixels);
        return false;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->pixels = pixels;
    ctx->dsc.header.cf          = LV_IMG_CF_TRUE_COLOR;
    ctx->dsc.header.always_zero = 0;
    ctx->dsc.header.reserved    = 0;
    ctx->dsc.header.w           = img_w;
    ctx->dsc.header.h           = img_h;
    ctx->dsc.data_size          = pixel_bytes;
    ctx->dsc.data               = pixels;

    img_ctx_t *old = lv_obj_get_user_data(obj);
    lv_img_set_src(obj, NULL);
    lv_obj_set_user_data(obj, NULL);
    img_ctx_free(old);

    lv_obj_set_user_data(obj, ctx);
    lv_img_set_src(obj, &ctx->dsc);
    return true;
}