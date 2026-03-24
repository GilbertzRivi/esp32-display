#pragma once

#include <lvgl.h>
#include <stdbool.h>
#include <stdint.h>

/* Tworzy pusty lv_img widget. */
lv_obj_t *w_image_create(lv_obj_t *parent, int x, int y, int w, int h);

/* Ustawia obraz z surowego bufora RGB565 (piksele w formacie lv_color_t LE).
   Przejmuje własność pixels — bufor będzie zwolniony przez free()
   przy podmianie obrazka albo LV_EVENT_DELETE.
   Zwraca false jeśli brakuje pamięci na descriptor. */
bool w_image_set_buf(lv_obj_t *obj, uint16_t img_w, uint16_t img_h, uint8_t *pixels);