#include "http_fetch.h"
#include <esp_http_client.h>
#include <esp_log.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define TAG "http"

#define MAX_FETCH_SIZE (64 * 1024)

typedef struct {
    char  *buf;
    size_t len;
    size_t cap;
    bool   error;
} fetch_ctx_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    fetch_ctx_t *ctx = evt->user_data;
    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
        if (ctx->error) break;
        if (ctx->len + (size_t)evt->data_len + 1 > MAX_FETCH_SIZE) {
            ESP_LOGE(TAG, "response too large");
            ctx->error = true;
            break;
        }
        if (ctx->len + (size_t)evt->data_len + 1 > ctx->cap) {
            size_t new_cap = ctx->cap ? ctx->cap * 2 : 4096;
            while (new_cap < ctx->len + (size_t)evt->data_len + 1) new_cap *= 2;
            char *nb = realloc(ctx->buf, new_cap);
            if (!nb) { ctx->error = true; break; }
            ctx->buf = nb;
            ctx->cap = new_cap;
        }
        memcpy(ctx->buf + ctx->len, evt->data, evt->data_len);
        ctx->len += evt->data_len;
        break;
    default:
        break;
    }
    return ESP_OK;
}

char *http_fetch(const char *url)
{
    fetch_ctx_t ctx = {0};

    esp_http_client_config_t cfg = {
        .url           = url,
        .event_handler = http_event_handler,
        .user_data     = &ctx,
        .timeout_ms    = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK || status != 200) {
        ESP_LOGE(TAG, "GET %s -> err=%d status=%d", url, err, status);
        free(ctx.buf);
        return NULL;
    }
    if (ctx.error || !ctx.buf) {
        free(ctx.buf);
        return NULL;
    }

    ctx.buf[ctx.len] = '\0';
    ESP_LOGI(TAG, "GET %s -> %d bytes", url, (int)ctx.len);
    return ctx.buf;
}
