#pragma once

#include <lvgl.h>

/* layout: "row", "column", "grid", "absolute" */
lv_obj_t *w_container_create(lv_obj_t *parent, int x, int y, int w, int h, const char *layout);

/* Odstep miedzy dziecmi (flex gap) */
void w_container_set_gap(lv_obj_t *obj, int gap);

/* Wewnetrzny padding (miedzy borderem kontenera a jego dziecmi) */
void w_container_set_pad(lv_obj_t *obj, int top, int right, int bottom, int left);
