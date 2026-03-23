#include "w_icon.h"
#include "../theme.h"

lv_obj_t *w_icon_create(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_CLIP);
    theme_apply(obj, "icon");
    return obj;
}

void w_icon_set_text(lv_obj_t *obj, const char *text)
{
    lv_label_set_text(obj, text);
}

void w_icon_set_field(lv_obj_t *obj, int field_id, const char *val)
{
    if (field_id == W_ICON_TEXT) lv_label_set_text(obj, val);
}
