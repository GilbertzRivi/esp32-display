#include "w_gauge.h"
#include "../theme.h"
#include <stdlib.h>

static const w_gauge_config_t default_cfg = W_GAUGE_CONFIG_DEFAULT;

lv_obj_t *w_gauge_create(lv_obj_t *parent, int x, int y, int w, int h,
                          const w_gauge_config_t *cfg)
{
    if (!cfg) cfg = &default_cfg;

    lv_obj_t *obj = lv_arc_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_arc_set_rotation(obj, cfg->rotation);
    lv_arc_set_bg_angles(obj, 0, cfg->sweep);
    lv_arc_set_range(obj, 0, 100);
    lv_arc_set_value(obj, 0);
    lv_obj_remove_style(obj, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);

    int arc_w = theme_gauge_arc_width(LV_MIN(w, h));
    lv_obj_set_style_arc_color(obj, theme_color(THEME_DIM), LV_PART_MAIN);
    lv_obj_set_style_arc_width(obj, arc_w, LV_PART_MAIN);
    lv_obj_set_style_arc_color(obj, theme_color(THEME_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(obj, arc_w, LV_PART_INDICATOR);
    theme_apply(obj, "gauge");

    lv_obj_t *lbl = lv_label_create(obj);
    lv_obj_center(lbl);
    lv_obj_set_style_text_font(lbl, theme_font(), LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, theme_color(THEME_FG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lbl, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_label_set_text(lbl, "");

    return obj;
}

void w_gauge_set_value(lv_obj_t *obj, int value, int max)
{
    lv_arc_set_range(obj, 0, max);
    lv_arc_set_value(obj, value);
}

void w_gauge_set_label(lv_obj_t *obj, const char *text)
{
    lv_obj_t *lbl = lv_obj_get_child(obj, 0);
    if (lbl) lv_label_set_text(lbl, text);
}

void w_gauge_set_field(lv_obj_t *obj, int field_id, const char *val)
{
    switch (field_id) {
        case W_GAUGE_VALUE: lv_arc_set_value(obj, atoi(val));    break;
        case W_GAUGE_MAX:   lv_arc_set_range(obj, 0, atoi(val)); break;
        case W_GAUGE_LABEL: w_gauge_set_label(obj, val);         break;
    }
}
