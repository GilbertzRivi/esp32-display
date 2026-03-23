#include "w_spinner.h"
#include "../theme.h"

lv_obj_t *w_spinner_create(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *obj = lv_spinner_create(parent,
                                      theme_spinner_speed_ms(),
                                      theme_spinner_arc_deg());
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);

    int arc_w = theme_gauge_arc_width(LV_MIN(w, h));
    lv_obj_set_style_arc_color(obj, theme_color(THEME_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(obj, arc_w, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(obj, theme_color(THEME_DIM), LV_PART_MAIN);
    lv_obj_set_style_arc_width(obj, arc_w, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    return obj;
}
