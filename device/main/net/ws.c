#include "ws.h"
#include <esp_websocket_client.h>
#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#define TAG "ws"
#define WS_QUEUE_LEN 8

static esp_websocket_client_handle_t s_client;
static ws_data_cb_t s_on_data;
static QueueHandle_t s_queue;

static void ws_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    esp_websocket_event_data_t *ev = data;
    switch (id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "connected");
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "disconnected");
        break;
    case WEBSOCKET_EVENT_DATA:
        if (ev->op_code == 1 && ev->data_len > 0 && s_on_data) {
            char *buf = malloc(ev->data_len + 1);
            if (buf) {
                memcpy(buf, ev->data_ptr, ev->data_len);
                buf[ev->data_len] = '\0';
                if (xQueueSend(s_queue, &buf, 0) != pdTRUE) {
                    free(buf); /* queue full — drop frame */
                }
            }
        }
        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(TAG, "error");
        break;
    default:
        break;
    }
}

void ws_process_queue(void)
{
    char *buf;
    while (xQueueReceive(s_queue, &buf, 0) == pdTRUE) {
        if (s_on_data) s_on_data(buf);
        free(buf);
    }
}

esp_err_t ws_connect(const char *url, ws_data_cb_t on_data)
{
    s_queue   = xQueueCreate(WS_QUEUE_LEN, sizeof(char *));
    s_on_data = on_data;

    char ws_url[256];
    if (strncmp(url, "http://", 7) == 0) {
        snprintf(ws_url, sizeof(ws_url), "ws://%s", url + 7);
    } else {
        snprintf(ws_url, sizeof(ws_url), "%s", url);
    }

    esp_websocket_client_config_t cfg = {
        .uri = ws_url,
    };

    s_client = esp_websocket_client_init(&cfg);
    esp_websocket_register_events(s_client, WEBSOCKET_EVENT_ANY, ws_event_handler, NULL);
    esp_err_t err = esp_websocket_client_start(s_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "start failed: %d", err);
    }
    return err;
}

void ws_send(const char *json)
{
    if (!s_client) return;
    esp_websocket_client_send_text(s_client, json, strlen(json), portMAX_DELAY);
}

void ws_disconnect(void)
{
    if (!s_client) return;
    esp_websocket_client_stop(s_client);
    esp_websocket_client_destroy(s_client);
    s_client = NULL;
}
