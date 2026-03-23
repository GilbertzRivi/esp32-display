#pragma once

#include <lvgl.h>

lv_obj_t *w_ticker_create(lv_obj_t *parent, int x, int y, int w, int h);

/* Kopiuje frames wewnetrznie — caller nie musi trzymac tablicy przy zyciu. */
void w_ticker_start(lv_obj_t *obj, const char **frames, int count, int interval_ms);
void w_ticker_stop(lv_obj_t *obj);
