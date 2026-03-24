#include "http_fetch.h"
#include <esp_http_client.h>
#include <esp_log.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define TAG "http"

#define MAX_FETCH_SIZE  (64 * 1024)
#define MAX_BINARY_SIZE (512 * 1024)

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
            if (!nb) {
                ctx->error = true;
                break;
            }

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
    int status    = esp_http_client_get_status_code(client);
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

/* ------------------------------------------------------------------ */
/* Binary fetch                                                        */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t *buf;
    size_t   len;
    size_t   cap;
    bool     error;
} fetch_bin_ctx_t;

static esp_err_t http_event_binary(esp_http_client_event_t *evt)
{
    fetch_bin_ctx_t *ctx = evt->user_data;

    if (evt->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;
    if (ctx->error) return ESP_OK;

    if (ctx->len + (size_t)evt->data_len > MAX_BINARY_SIZE) {
        ESP_LOGE(TAG, "image too large (>%d B)", MAX_BINARY_SIZE);
        ctx->error = true;
        return ESP_OK;
    }

    if (ctx->len + (size_t)evt->data_len > ctx->cap) {
        size_t new_cap = ctx->cap ? ctx->cap * 2 : 16384;
        while (new_cap < ctx->len + (size_t)evt->data_len) new_cap *= 2;
        if (new_cap > MAX_BINARY_SIZE) new_cap = MAX_BINARY_SIZE;

        uint8_t *nb = realloc(ctx->buf, new_cap);
        if (!nb) {
            ctx->error = true;
            return ESP_OK;
        }

        ctx->buf = nb;
        ctx->cap = new_cap;
    }

    memcpy(ctx->buf + ctx->len, evt->data, evt->data_len);
    ctx->len += evt->data_len;
    return ESP_OK;
}

uint8_t *http_fetch_binary(const char *url, size_t *out_len)
{
    if (!out_len) return NULL;
    *out_len = 0;

    fetch_bin_ctx_t ctx = {0};

    esp_http_client_config_t cfg = {
        .url           = url,
        .event_handler = http_event_binary,
        .user_data     = &ctx,
        .timeout_ms    = 10000,
        .buffer_size   = 4096,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_err_t err = esp_http_client_perform(client);
    int status    = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK || status != 200 || ctx.error || !ctx.buf) {
        ESP_LOGE(TAG, "GET %s -> err=%d status=%d", url, err, status);
        free(ctx.buf);
        return NULL;
    }

    ESP_LOGI(TAG, "GET %s -> %d bytes (binary)", url, (int)ctx.len);
    *out_len = ctx.len;
    return ctx.buf;
}