#include "w_image.h"
#include "../theme.h"

lv_obj_t *w_image_create(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_CLIP);
    lv_label_set_text(obj, "[img]");
    lv_obj_set_style_text_font(obj, theme_font(), LV_PART_MAIN);
    lv_obj_set_style_text_color(obj, theme_color(THEME_DIM), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(obj, theme_color(THEME_DIM), LV_PART_MAIN);
    return obj;
}
