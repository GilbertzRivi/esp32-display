#pragma once

#include <lvgl.h>

void touch_hw_init(void);
void touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data);
