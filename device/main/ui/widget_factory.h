#pragma once

#include <esp_err.h>

void      widget_factory_init(void);
esp_err_t widget_factory_build(const char *layout_json);
