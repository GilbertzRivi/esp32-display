#include "w_graph.h"
#include "../theme.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    graph_style_t style;
    lv_chart_series_t *ser;

    /* symmetric */
    lv_color_t *canvas_buf;
    int canvas_w;
    int canvas_h;

    int range_min;
    int range_max;

    /* snapshot of theme.graph at creation — color-by-value and sym gradient */
    bool              color_by_value;
    graph_threshold_t thresholds[GRAPH_MAX_THRESHOLDS];
    int               threshold_count;
    lv_color_t        base_color;

    bool       sym_color_by_distance;
    lv_color_t sym_center_color;
    lv_color_t sym_peak_color;
} graph_ctx_t;

/* ------------------------------------------------------------------ */
/* Kolor na podstawie progow                                            */
/* ------------------------------------------------------------------ */

static lv_color_t ctx_threshold_color(const graph_ctx_t *ctx, int value)
{
    for (int i = 0; i < ctx->threshold_count; i++) {
        if (value <= ctx->thresholds[i].threshold)
            return ctx->thresholds[i].color;
    }
    return ctx->base_color;
}

/* ------------------------------------------------------------------ */
/* Draw event — nadpisuje kolor per slupek / per punkt linii           */
/* ------------------------------------------------------------------ */

static void graph_draw_cb(lv_event_t *e)
{
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
    if (!dsc) return;

    graph_ctx_t *ctx = lv_obj_get_user_data(lv_event_get_target(e));
    if (!ctx || !ctx->color_by_value) return;

    if (dsc->value == LV_CHART_POINT_NONE) return;
    lv_color_t c = ctx_threshold_color(ctx, (int)dsc->value);

    if (dsc->type == LV_CHART_DRAW_PART_BAR && dsc->rect_dsc) {
        dsc->rect_dsc->bg_color = c;
    } else if (dsc->type == LV_CHART_DRAW_PART_LINE_AND_POINT) {
        if (dsc->line_dsc) dsc->line_dsc->color = c;
        if (dsc->rect_dsc) dsc->rect_dsc->bg_color = c;
    }
}

/* ------------------------------------------------------------------ */
/* Symmetric (canvas)                                                   */
/* ------------------------------------------------------------------ */

static lv_color_t sym_pixel_color(const graph_ctx_t *ctx, float dist_norm)
{
    if (!ctx->sym_color_by_distance)
        return ctx->base_color;
    /* lv_color_mix(c1, c2, ratio): ratio=0 → c2, ratio=255 → c1 */
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

    /* shift left by one column */
    for (int y = 0; y < h; y++)
        memmove(&ctx->canvas_buf[y * w],
                &ctx->canvas_buf[y * w + 1],
                (w - 1) * sizeof(lv_color_t));

    float range = (float)(ctx->range_max - ctx->range_min);
    if (range <= 0) range = 1;
    float norm = (value - ctx->range_min) / range;
    if (norm < 0) norm = 0;
    if (norm > 1) norm = 1;

    sym_draw_col(ctx, w - 1, norm);
    lv_obj_invalidate(obj);
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                            */
/* ------------------------------------------------------------------ */

static void graph_delete_cb(lv_event_t *e)
{
    graph_ctx_t *ctx = lv_obj_get_user_data(lv_event_get_target(e));
    if (!ctx) return;
    if (ctx->canvas_buf) lv_mem_free(ctx->canvas_buf);
    lv_mem_free(ctx);
}

/* ------------------------------------------------------------------ */
/* Create                                                               */
/* ------------------------------------------------------------------ */

lv_obj_t *w_graph_create(lv_obj_t *parent, int x, int y, int w, int h,
                          const graph_config_t *cfg)
{
    const graph_config_t *tg = cfg ? cfg : theme_graph();

    int y_min       = tg->y_min;
    int y_max       = tg->y_max;
    int point_count = tg->point_count;

    graph_ctx_t *ctx = lv_mem_alloc(sizeof(graph_ctx_t));
    memset(ctx, 0, sizeof(*ctx));
    ctx->style       = tg->style;
    ctx->range_min   = y_min;
    ctx->range_max   = y_max;
    ctx->base_color  = tg->color;

    /* snapshot color-by-value config */
    ctx->color_by_value   = tg->color_by_value;
    ctx->threshold_count  = tg->threshold_count;
    memcpy(ctx->thresholds, tg->thresholds,
           tg->threshold_count * sizeof(graph_threshold_t));

    /* snapshot symmetric gradient config */
    ctx->sym_color_by_distance = tg->sym_color_by_distance;
    ctx->sym_center_color      = tg->sym_center_color;
    ctx->sym_peak_color        = tg->sym_peak_color;

    lv_obj_t *obj;

    if (tg->style == GRAPH_STYLE_SYMMETRIC) {
        size_t buf_sz = LV_CANVAS_BUF_SIZE_TRUE_COLOR(w, h);
        ctx->canvas_buf = lv_mem_alloc(buf_sz);
        ctx->canvas_w   = w;
        ctx->canvas_h   = h;

        obj = lv_canvas_create(parent);
        lv_canvas_set_buffer(obj, ctx->canvas_buf, w, h, LV_IMG_CF_TRUE_COLOR);
        lv_canvas_fill_bg(obj, theme_color(THEME_BG), LV_OPA_COVER);
    } else {
        obj = lv_chart_create(parent);
        lv_chart_set_range(obj, LV_CHART_AXIS_PRIMARY_Y, y_min, y_max);
        lv_chart_set_div_line_count(obj, tg->grid_h_lines, 0);
        lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);

        if (tg->grid_h_lines > 0) {
            lv_obj_set_style_line_color(obj, tg->grid_color, LV_PART_MAIN);
            lv_obj_set_style_line_opa(obj, LV_OPA_50, LV_PART_MAIN);
        }

        if (tg->style == GRAPH_STYLE_BARS) {
            lv_chart_set_type(obj, LV_CHART_TYPE_BAR);

            int bw   = LV_MAX(1, tg->bar_width);
            int bg   = LV_MAX(0, tg->bar_gap);
            int slot = bw + bg;
            int pts  = (point_count > 0) ? point_count : LV_MAX(1, w / slot);
            lv_chart_set_point_count(obj, pts);
            lv_obj_set_style_pad_column(obj, bg / 2, LV_PART_MAIN);

        } else {
            /* line / area */
            lv_chart_set_type(obj, LV_CHART_TYPE_LINE);
            int pts = (point_count > 0) ? point_count : w;
            lv_chart_set_point_count(obj, pts);
            lv_obj_set_style_pad_column(obj, 0, LV_PART_MAIN);
            lv_obj_set_style_size(obj, 0, LV_PART_INDICATOR);
            lv_obj_set_style_line_width(obj, tg->line_width, LV_PART_ITEMS);

            if (tg->style == GRAPH_STYLE_AREA) {
                lv_obj_set_style_bg_color(obj, tg->fill_color, LV_PART_ITEMS);
                lv_obj_set_style_bg_opa(obj, tg->fill_opa, LV_PART_ITEMS);
            }
        }

        ctx->ser = lv_chart_add_series(obj, tg->color, LV_CHART_AXIS_PRIMARY_Y);

        if (tg->color_by_value)
            lv_obj_add_event_cb(obj, graph_draw_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);

        theme_apply(obj, "graph");
    }

    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_user_data(obj, ctx);
    lv_obj_add_event_cb(obj, graph_delete_cb, LV_EVENT_DELETE, NULL);

    return obj;
}

/* ------------------------------------------------------------------ */
/* Settery                                                              */
/* ------------------------------------------------------------------ */

void w_graph_set_range(lv_obj_t *obj, int min, int max)
{
    graph_ctx_t *ctx = lv_obj_get_user_data(obj);
    if (!ctx) return;
    ctx->range_min = min;
    ctx->range_max = max;
    if (ctx->style != GRAPH_STYLE_SYMMETRIC)
        lv_chart_set_range(obj, LV_CHART_AXIS_PRIMARY_Y, min, max);
}

void w_graph_push(lv_obj_t *obj, float value)
{
    graph_ctx_t *ctx = lv_obj_get_user_data(obj);
    if (!ctx) return;
    if (ctx->style == GRAPH_STYLE_SYMMETRIC)
        sym_push(obj, ctx, value);
    else
        lv_chart_set_next_value(obj, ctx->ser, (lv_coord_t)value);
}

void w_graph_set_field(lv_obj_t *obj, int field_id, const char *val)
{
    if (field_id == W_GRAPH_VALUE) w_graph_push(obj, atof(val));
}
