#include "w_bar.h"
#include "../theme.h"
#include <string.h>
#include <stdlib.h>

#define BAR_LABEL_MAX 64

typedef struct {
    char text[BAR_LABEL_MAX];
} bar_ctx_t;

static void bar_draw_label_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    bar_ctx_t *ctx = lv_obj_get_user_data(obj);
    if (!ctx || ctx->text[0] == '\0') return;

    lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(e);

    int val   = lv_bar_get_value(obj);
    int min   = lv_bar_get_min_value(obj);
    int max   = lv_bar_get_max_value(obj);
    int range = max - min;
    if (range <= 0) return;

    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);

    lv_coord_t bar_w   = lv_area_get_width(&coords);
    lv_coord_t fill_px = (lv_coord_t)((float)(val - min) / (float)range * (float)bar_w);

    lv_area_t filled   = { coords.x1,           coords.y1, coords.x1 + fill_px, coords.y2 };
    lv_area_t unfilled = { coords.x1 + fill_px, coords.y1, coords.x2,           coords.y2 };

    lv_draw_label_dsc_t dsc;
    lv_draw_label_dsc_init(&dsc);
    dsc.font  = theme_font();
    dsc.align = LV_TEXT_ALIGN_CENTER;

    /* lv_draw_label rysuje od gory obszaru — centrujemy recznie w pionie */
    lv_coord_t font_h = lv_font_get_line_height(dsc.font);
    lv_coord_t bar_h  = lv_area_get_height(&coords);
    lv_coord_t y_ofs  = (bar_h - font_h) / 2;
    lv_area_t text_area = { coords.x1, coords.y1 + y_ofs, coords.x2, coords.y2 };

    const lv_area_t *orig = draw_ctx->clip_area;
    lv_area_t clip;

    /* tekst na tle DIM — kolor FG */
    dsc.color = theme_color(THEME_FG);
    if (_lv_area_intersect(&clip, orig, &unfilled)) {
        draw_ctx->clip_area = &clip;
        lv_draw_label(draw_ctx, &dsc, &text_area, ctx->text, NULL);
    }

    /* tekst na tle wypelnienia — kolor BG dla kontrastu */
    dsc.color = theme_color(THEME_BG);
    if (_lv_area_intersect(&clip, orig, &filled)) {
        draw_ctx->clip_area = &clip;
        lv_draw_label(draw_ctx, &dsc, &text_area, ctx->text, NULL);
    }

    draw_ctx->clip_area = orig;
}

static void bar_delete_cb(lv_event_t *e)
{
    bar_ctx_t *ctx = lv_obj_get_user_data(lv_event_get_target(e));
    if (ctx) lv_mem_free(ctx);
}

lv_obj_t *w_bar_create(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *obj = lv_bar_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_bar_set_range(obj, 0, 100);
    lv_bar_set_value(obj, 0, LV_ANIM_OFF);
    theme_apply(obj, "bar");

    bar_ctx_t *ctx = lv_mem_alloc(sizeof(*ctx));
    memset(ctx, 0, sizeof(*ctx));
    lv_obj_set_user_data(obj, ctx);

    lv_obj_add_event_cb(obj, bar_draw_label_cb, LV_EVENT_DRAW_MAIN_END, NULL);
    lv_obj_add_event_cb(obj, bar_delete_cb,     LV_EVENT_DELETE,        NULL);

    return obj;
}

void w_bar_set_value(lv_obj_t *obj, int value, int max)
{
    lv_bar_set_range(obj, 0, max);
    lv_bar_set_value(obj, value, LV_ANIM_OFF);
    lv_obj_invalidate(obj);
}

void w_bar_set_label(lv_obj_t *obj, const char *text)
{
    bar_ctx_t *ctx = lv_obj_get_user_data(obj);
    if (!ctx) return;
    strncpy(ctx->text, text, BAR_LABEL_MAX - 1);
    lv_obj_invalidate(obj);
}

void w_bar_set_field(lv_obj_t *obj, int field_id, const char *val)
{
    switch (field_id) {
        case W_BAR_VALUE:
            lv_bar_set_value(obj, atoi(val), LV_ANIM_OFF);
            lv_obj_invalidate(obj);
            break;
        case W_BAR_MAX:
            lv_bar_set_range(obj, 0, atoi(val));
            lv_obj_invalidate(obj);
            break;
        case W_BAR_LABEL:
            w_bar_set_label(obj, val);
            break;
    }
}
