#pragma once

#include <esp_err.h>

typedef void (*ws_data_cb_t)(const char *json_frame);

esp_err_t ws_connect(const char *url, ws_data_cb_t on_data);
void      ws_send(const char *json);
void      ws_disconnect(void);
/* Call from main task to drain received frames and invoke the callback. */
void      ws_process_queue(void);
