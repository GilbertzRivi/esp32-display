#include "w_graph.h"
#include "../theme.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define GRAPH_CTX_MAGIC 0x47524150u /* 'GRAP' */

typedef struct {
    uint32_t magic;
    lv_obj_t *owner;

    graph_style_t style;
    lv_chart_series_t *ser;

    /* symmetric */
    lv_color_t *canvas_buf;
    int canvas_w;
    int canvas_h;

    int range_min;
    int range_max;

    bool              color_by_value;
    graph_threshold_t thresholds[GRAPH_MAX_THRESHOLDS];
    int               threshold_count;
    lv_color_t        base_color;

    bool       sym_color_by_distance;
    lv_color_t sym_center_color;
    lv_color_t sym_peak_color;
} graph_ctx_t;

static graph_ctx_t *graph_ctx_get(lv_obj_t *obj)
{
    if (!obj) return NULL;
    if (!lv_obj_is_valid(obj)) return NULL;

    graph_ctx_t *ctx = lv_obj_get_user_data(obj);
    if (!ctx) return NULL;
    if (ctx->magic != GRAPH_CTX_MAGIC) return NULL;
    if (ctx->owner != obj) return NULL;

    return ctx;
}

static int graph_clamp_threshold_count(int count)
{
    if (count < 0) return 0;
    if (count > GRAPH_MAX_THRESHOLDS) return GRAPH_MAX_THRESHOLDS;
    return count;
}

static int graph_resolve_point_count(graph_style_t style, int point_count, int w, int bar_width, int bar_gap)
{
    if (point_count > 0) return point_count;

    if (style == GRAPH_STYLE_BARS) {
        int bw   = LV_MAX(1, bar_width);
        int bg   = LV_MAX(0, bar_gap);
        int slot = bw + bg;
        return LV_MAX(1, w / slot);
    }

    return LV_MAX(1, w);
}

static lv_color_t ctx_threshold_color(const graph_ctx_t *ctx, int value)
{
    for (int i = 0; i < ctx->threshold_count; i++) {
        if (value <= ctx->thresholds[i].threshold)
            return ctx->thresholds[i].color;
    }
    return ctx->base_color;
}

static void graph_draw_cb(lv_event_t *e)
{
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
    if (!dsc) return;

    graph_ctx_t *ctx = lv_event_get_user_data(e);
    if (!ctx) return;
    if (ctx->magic != GRAPH_CTX_MAGIC) return;
    if (!ctx->color_by_value) return;

    if (dsc->value == LV_CHART_POINT_NONE) return;
    lv_color_t c = ctx_threshold_color(ctx, (int)dsc->value);

    if (dsc->type == LV_CHART_DRAW_PART_BAR && dsc->rect_dsc) {
        dsc->rect_dsc->bg_color = c;
    } else if (dsc->type == LV_CHART_DRAW_PART_LINE_AND_POINT) {
        if (dsc->line_dsc) dsc->line_dsc->color = c;
        if (dsc->rect_dsc) dsc->rect_dsc->bg_color = c;
    }
}

static lv_color_t sym_pixel_color(const graph_ctx_t *ctx, float dist_norm)
{
    if (!ctx->sym_color_by_distance)
        return ctx->base_color;

    uint8_t ratio = (uint8_t)(dist_norm * 255.0f);
    return lv_color_mix(ctx->sym_peak_color, ctx->sym_center_color, ratio);
}

static void sym_draw_col(graph_ctx_t *ctx, int x, float norm)
{
    int center = ctx->canvas_h / 2;
    int half   = (int)(norm * center);

    for (int y = 0; y < ctx->canvas_h; y++) {
        int dist = y - center;
        lv_color_t px;
        if (dist >= -half && dist <= half) {
            float dist_norm = (half > 0) ? (float)abs(dist) / (float)half : 0.0f;
            px = sym_pixel_color(ctx, dist_norm);
        } else {
            px = theme_color(THEME_BG);
        }
        ctx->canvas_buf[y * ctx->canvas_w + x] = px;
    }
}

static void sym_push(lv_obj_t *obj, graph_ctx_t *ctx, float value)
{
    int w = ctx->canvas_w;
    int h = ctx->canvas_h;

    if (!ctx->canvas_buf || w <= 0 || h <= 0) return;

    for (int y = 0; y < h; y++) {
        memmove(&ctx->canvas_buf[y * w],
                &ctx->canvas_buf[y * w + 1],
                (w - 1) * sizeof(lv_color_t));
    }

    float range = (float)(ctx->range_max - ctx->range_min);
    if (range <= 0.0f) range = 1.0f;

    float norm = (value - ctx->range_min) / range;
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;

    sym_draw_col(ctx, w - 1, norm);
    lv_obj_invalidate(obj);
}

static void graph_delete_cb(lv_event_t *e)
{
    graph_ctx_t *ctx = lv_event_get_user_data(e);
    if (!ctx) return;
    if (ctx->magic != GRAPH_CTX_MAGIC) return;

    ctx->magic = 0;
    ctx->owner = NULL;
    ctx->ser   = NULL;

    if (ctx->canvas_buf) {
        lv_mem_free(ctx->canvas_buf);
        ctx->canvas_buf = NULL;
    }

    lv_mem_free(ctx);
}

lv_obj_t *w_graph_create(lv_obj_t *parent, int x, int y, int w, int h,
                         const graph_config_t *cfg)
{
    const graph_config_t *tg = cfg ? cfg : theme_graph();
    if (!tg) return NULL;

    if (w < 1) w = 1;
    if (h < 1) h = 1;

    int y_min       = tg->y_min;
    int y_max       = tg->y_max;
    int point_count = graph_resolve_point_count(
        tg->style, tg->point_count, w, tg->bar_width, tg->bar_gap
    );

    graph_ctx_t *ctx = lv_mem_alloc(sizeof(*ctx));
    if (!ctx) return NULL;
    memset(ctx, 0, sizeof(*ctx));

    ctx->magic      = GRAPH_CTX_MAGIC;
    ctx->style      = tg->style;
    ctx->range_min  = y_min;
    ctx->range_max  = y_max;
    ctx->base_color = tg->color;

    ctx->color_by_value  = tg->color_by_value;
    ctx->threshold_count = graph_clamp_threshold_count(tg->threshold_count);
    if (ctx->threshold_count > 0) {
        memcpy(ctx->thresholds, tg->thresholds,
               ctx->threshold_count * sizeof(graph_threshold_t));
    }

    ctx->sym_color_by_distance = tg->sym_color_by_distance;
    ctx->sym_center_color      = tg->sym_center_color;
    ctx->sym_peak_color        = tg->sym_peak_color;

    lv_obj_t *obj = NULL;

    if (tg->style == GRAPH_STYLE_SYMMETRIC) {
        size_t buf_sz = LV_CANVAS_BUF_SIZE_TRUE_COLOR(w, h);
        ctx->canvas_buf = lv_mem_alloc(buf_sz);
        if (!ctx->canvas_buf) {
            lv_mem_free(ctx);
            return NULL;
        }

        ctx->canvas_w = w;
        ctx->canvas_h = h;

        obj = lv_canvas_create(parent);
        if (!obj) {
            lv_mem_free(ctx->canvas_buf);
            lv_mem_free(ctx);
            return NULL;
        }

        ctx->owner = obj;
        lv_obj_set_user_data(obj, ctx);
        lv_obj_add_event_cb(obj, graph_delete_cb, LV_EVENT_DELETE, ctx);

        lv_canvas_set_buffer(obj, ctx->canvas_buf, w, h, LV_IMG_CF_TRUE_COLOR);
        lv_canvas_fill_bg(obj, theme_color(THEME_BG), LV_OPA_COVER);
    } else {
        obj = lv_chart_create(parent);
        if (!obj) {
            lv_mem_free(ctx);
            return NULL;
        }

        ctx->owner = obj;
        lv_obj_set_user_data(obj, ctx);
        lv_obj_add_event_cb(obj, graph_delete_cb, LV_EVENT_DELETE, ctx);

        lv_chart_set_range(obj, LV_CHART_AXIS_PRIMARY_Y, y_min, y_max);
        lv_chart_set_div_line_count(obj, tg->grid_h_lines, 0);
        lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);

        if (tg->grid_h_lines > 0) {
            lv_obj_set_style_line_color(obj, tg->grid_color, LV_PART_MAIN);
            lv_obj_set_style_line_opa(obj, LV_OPA_50, LV_PART_MAIN);
        }

        if (tg->style == GRAPH_STYLE_BARS) {
            lv_chart_set_type(obj, LV_CHART_TYPE_BAR);

            int bw = LV_MAX(1, tg->bar_width);
            int bg = LV_MAX(0, tg->bar_gap);

            lv_chart_set_point_count(obj, LV_MAX(1, point_count));
            lv_obj_set_style_pad_column(obj, bg / 2, LV_PART_MAIN);
        } else {
            lv_chart_set_type(obj, LV_CHART_TYPE_LINE);
            lv_chart_set_point_count(obj, LV_MAX(1, point_count));
            lv_obj_set_style_pad_column(obj, 0, LV_PART_MAIN);
            lv_obj_set_style_size(obj, 0, LV_PART_INDICATOR);
            lv_obj_set_style_line_width(obj, tg->line_width, LV_PART_ITEMS);

            if (tg->style == GRAPH_STYLE_AREA) {
                lv_obj_set_style_bg_color(obj, tg->fill_color, LV_PART_ITEMS);
                lv_obj_set_style_bg_opa(obj, tg->fill_opa, LV_PART_ITEMS);
            }
        }

        ctx->ser = lv_chart_add_series(obj, tg->color, LV_CHART_AXIS_PRIMARY_Y);
        if (!ctx->ser) {
            lv_obj_del(obj);
            return NULL;
        }

        if (!ctx->ser->y_points) {
            lv_obj_del(obj);
            return NULL;
        }

        if (tg->color_by_value) {
            lv_obj_add_event_cb(obj, graph_draw_cb, LV_EVENT_DRAW_PART_BEGIN, ctx);
        }

        theme_apply(obj, "graph");
    }

    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);

    return obj;
}

void w_graph_set_color(lv_obj_t *obj, lv_color_t color)
{
    graph_ctx_t *ctx = graph_ctx_get(obj);
    if (!ctx) return;

    ctx->base_color = color;

    if (ctx->ser)
        lv_chart_set_series_color(obj, ctx->ser, color);
}

void w_graph_set_range(lv_obj_t *obj, int min, int max)
{
    graph_ctx_t *ctx = graph_ctx_get(obj);
    if (!ctx) return;

    ctx->range_min = min;
    ctx->range_max = max;

    if (ctx->style != GRAPH_STYLE_SYMMETRIC)
        lv_chart_set_range(obj, LV_CHART_AXIS_PRIMARY_Y, min, max);
}

void w_graph_push(lv_obj_t *obj, float value)
{
    graph_ctx_t *ctx = graph_ctx_get(obj);
    if (!ctx) return;

    if (ctx->style == GRAPH_STYLE_SYMMETRIC) {
        sym_push(obj, ctx, value);
        return;
    }

    if (!ctx->ser) return;
    if (!ctx->ser->y_points) return;
    if (lv_chart_get_point_count(obj) < 1) return;

    lv_chart_set_next_value(obj, ctx->ser, (lv_coord_t)value);
}

void w_graph_set_field(lv_obj_t *obj, int field_id, const char *val)
{
    if (field_id == W_GRAPH_VALUE)
        w_graph_push(obj, atof(val));
}