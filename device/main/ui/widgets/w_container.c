#include "w_container.h"
#include "../theme.h"
#include <string.h>

lv_obj_t *w_container_create(lv_obj_t *parent, int x, int y, int w, int h, const char *layout)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_scroll_dir(obj, LV_DIR_NONE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    theme_apply(obj, "container");

    if (strcmp(layout, "row") == 0) {
        lv_obj_set_layout(obj, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    } else if (strcmp(layout, "column") == 0) {
        lv_obj_set_layout(obj, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    } else if (strcmp(layout, "grid") == 0) {
        /* same descriptors i komorki beda ustawione pozniej w widget_factory.c */
        lv_obj_set_layout(obj, LV_LAYOUT_GRID);
    }
    /* "absolute": nie ustawiamy layoutu */

    return obj;
}

void w_container_set_gap(lv_obj_t *obj, int gap)
{
    lv_obj_set_style_pad_gap(obj, gap, LV_PART_MAIN);
    lv_obj_set_style_pad_row(obj, gap, LV_PART_MAIN);
    lv_obj_set_style_pad_column(obj, gap, LV_PART_MAIN);
}

void w_container_set_pad(lv_obj_t *obj, int top, int right, int bottom, int left)
{
    lv_obj_set_style_pad_top(obj,    top,    LV_PART_MAIN);
    lv_obj_set_style_pad_right(obj,  right,  LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(obj, bottom, LV_PART_MAIN);
    lv_obj_set_style_pad_left(obj,   left,   LV_PART_MAIN);
}