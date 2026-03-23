#include "w_sparkline.h"
#include "../theme.h"
#include <stdlib.h>

lv_obj_t *w_sparkline_create(lv_obj_t *parent, int x, int y, int w, int h,
                               const sparkline_config_t *cfg)
{
    const sparkline_config_t *c = cfg ? cfg : theme_sparkline();

    int pts = (c->point_count > 0) ? c->point_count : w;

    lv_obj_t *obj = lv_chart_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_chart_set_type(obj, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(obj, pts);
    lv_chart_set_range(obj, LV_CHART_AXIS_PRIMARY_Y, c->y_min, c->y_max);
    lv_chart_set_div_line_count(obj, 0, 0);

    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_size(obj, 0, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(obj, LV_MAX(1, c->line_width), LV_PART_ITEMS);

    lv_chart_series_t *ser = lv_chart_add_series(obj, c->color, LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_set_user_data(obj, ser);

    return obj;
}

void w_sparkline_push(lv_obj_t *obj, float value)
{
    lv_chart_series_t *ser = lv_obj_get_user_data(obj);
    if (!ser) return;
    lv_chart_set_next_value(obj, ser, (lv_coord_t)value);
}

void w_sparkline_set_range(lv_obj_t *obj, int min, int max)
{
    lv_chart_set_range(obj, LV_CHART_AXIS_PRIMARY_Y, min, max);
}

void w_sparkline_set_field(lv_obj_t *obj, int field_id, const char *val)
{
    if (field_id == W_SPARKLINE_VALUE) w_sparkline_push(obj, atof(val));
}
