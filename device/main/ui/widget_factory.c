#include "widget_factory.h"
#include "placeholder.h"
#include "screen_mgr.h"
#include "theme.h"
#include "../callback.h"
#include "../net/http_fetch.h"
#include "widgets/w_label.h"
#include "widgets/w_button.h"
#include "widgets/w_bar.h"
#include "widgets/w_graph.h"
#include "widgets/w_sparkline.h"
#include "widgets/w_gauge.h"
#include "widgets/w_spinner.h"
#include "widgets/w_icon.h"
#include "widgets/w_image.h"
#include "widgets/w_container.h"
#include "widgets/w_ticker.h"
#include <json_parser.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <lvgl.h>
#include "sdkconfig.h"

#define TAG "widget_factory"

void widget_factory_init(void) {}

static int field_id_for_key(const char *type, const char *key)
{
    if (strcmp(type, "label") == 0) {
        if (strcmp(key, "text") == 0) return W_LABEL_TEXT;
    } else if (strcmp(type, "button") == 0) {
        if (strcmp(key, "label") == 0) return W_BUTTON_LABEL;
    } else if (strcmp(type, "bar") == 0) {
        if (strcmp(key, "value") == 0) return W_BAR_VALUE;
        if (strcmp(key, "max") == 0) return W_BAR_MAX;
        if (strcmp(key, "label") == 0) return W_BAR_LABEL;
    } else if (strcmp(type, "graph") == 0) {
        if (strcmp(key, "value") == 0) return W_GRAPH_VALUE;
    } else if (strcmp(type, "sparkline") == 0) {
        if (strcmp(key, "value") == 0) return W_SPARKLINE_VALUE;
    } else if (strcmp(type, "gauge") == 0) {
        if (strcmp(key, "value") == 0) return W_GAUGE_VALUE;
        if (strcmp(key, "max") == 0) return W_GAUGE_MAX;
        if (strcmp(key, "label") == 0) return W_GAUGE_LABEL;
    } else if (strcmp(type, "icon") == 0) {
        if (strcmp(key, "text") == 0) return W_ICON_TEXT;
    }
    return -1;
}

static widget_set_field_fn set_field_fn_for_type(const char *type)
{
    if (strcmp(type, "label") == 0) return w_label_set_field;
    if (strcmp(type, "button") == 0) return w_button_set_field;
    if (strcmp(type, "bar") == 0) return w_bar_set_field;
    if (strcmp(type, "graph") == 0) return w_graph_set_field;
    if (strcmp(type, "sparkline") == 0) return w_sparkline_set_field;
    if (strcmp(type, "gauge") == 0) return w_gauge_set_field;
    if (strcmp(type, "icon") == 0) return w_icon_set_field;
    return NULL;
}

static const char *s_bind_keys[] = {"text", "label", "value", "max", "src", NULL};

static void apply_bind(jparse_ctx_t *jctx, const char *type, lv_obj_t *obj)
{
    widget_set_field_fn fn = set_field_fn_for_type(type);
    for (int k = 0; s_bind_keys[k]; k++) {
        int fid = field_id_for_key(type, s_bind_keys[k]);
        if (fid < 0) continue;

        char sval[64];
        int ival;
        if (json_obj_get_string(jctx, s_bind_keys[k], sval, sizeof(sval)) == OS_SUCCESS) {
            if (sval[0] == '$') {
                if (fn) placeholder_bind(sval + 1, obj, fid, fn);
            } else {
                if (fn) fn(obj, fid, sval);
            }
        } else if (json_obj_get_int(jctx, s_bind_keys[k], &ival) == OS_SUCCESS) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", ival);
            if (fn) fn(obj, fid, buf);
        }
    }
}

static void apply_theme_ws(const char *type, lv_obj_t *obj)
{
    theme_data_t *t = theme_data();

    if (strcmp(type, "label") == 0) {
        ws_apply(&t->ws_label, obj, LV_PART_MAIN);
    } else if (strcmp(type, "button") == 0) {
        ws_apply(&t->ws_button, obj, LV_PART_MAIN);
    } else if (strcmp(type, "bar") == 0) {
        ws_apply(&t->ws_bar, obj, LV_PART_MAIN);
        ws_apply(&t->ws_bar_ind, obj, LV_PART_INDICATOR);
    } else if (strcmp(type, "graph") == 0) {
        ws_apply(&t->ws_graph, obj, LV_PART_MAIN);
    } else if (strcmp(type, "sparkline") == 0) {
        ws_apply(&t->ws_sparkline, obj, LV_PART_MAIN);
    } else if (strcmp(type, "gauge") == 0) {
        ws_apply(&t->ws_gauge, obj, LV_PART_MAIN);
        ws_apply(&t->ws_gauge_ind, obj, LV_PART_INDICATOR);
    } else if (strcmp(type, "spinner") == 0) {
        ws_apply(&t->ws_spinner, obj, LV_PART_MAIN);
        ws_apply(&t->ws_spinner_ind, obj, LV_PART_INDICATOR);
    } else if (strcmp(type, "icon") == 0) {
        ws_apply(&t->ws_icon, obj, LV_PART_MAIN);
    } else if (strcmp(type, "container") == 0) {
        ws_apply(&t->ws_container, obj, LV_PART_MAIN);
    } else if (strcmp(type, "ticker") == 0) {
        ws_apply(&t->ws_ticker, obj, LV_PART_MAIN);
    } else if (strcmp(type, "screen") == 0) {
        ws_apply(&t->ws_screen, obj, LV_PART_MAIN);
    } else if (strcmp(type, "toggle") == 0) {
        ws_apply(&t->ws_toggle, obj, LV_PART_MAIN);
        ws_apply(&t->ws_toggle_checked, obj, LV_PART_MAIN | LV_STATE_CHECKED);
    }
}

/* ------------------------------------------------------------------ */
/* Obrazki                                                            */
/* ------------------------------------------------------------------ */

static uint8_t *img_fetch(const char *name, uint16_t *out_w, uint16_t *out_h)
{
    char url[256];
    snprintf(url, sizeof(url), "%s/images/%s", CONFIG_HOST_URL, name);

    size_t len = 0;
    uint8_t *raw = http_fetch_binary(url, &len);
    if (!raw) return NULL;

    if (len < 4) {
        ESP_LOGE(TAG, "img %s: response too short (%d B)", name, (int)len);
        free(raw);
        return NULL;
    }

    uint16_t w = ((uint16_t)raw[0] << 8) | raw[1];
    uint16_t h = ((uint16_t)raw[2] << 8) | raw[3];

    size_t pixel_bytes = (size_t)w * h * sizeof(lv_color_t);
    size_t expected = 4 + pixel_bytes;

    if (len < expected) {
        ESP_LOGE(TAG, "img %s: %dx%d needs %d B, got %d",
                 name, w, h, (int)expected, (int)len);
        free(raw);
        return NULL;
    }

    uint8_t *pixels = malloc(pixel_bytes);
    if (!pixels) {
        ESP_LOGE(TAG, "img %s: malloc failed for %d B", name, (int)pixel_bytes);
        free(raw);
        return NULL;
    }

    memcpy(pixels, raw + 4, pixel_bytes);
    free(raw);

    *out_w = w;
    *out_h = h;
    ESP_LOGI(TAG, "img %s: %dx%d loaded (%d B)", name, w, h, (int)pixel_bytes);
    return pixels;
}

typedef struct {
    lv_img_dsc_t dsc;
    uint8_t *pixels;
} bg_img_ctx_t;

static void bg_img_delete_cb(lv_event_t *e)
{
    bg_img_ctx_t *ctx = lv_obj_get_user_data(lv_event_get_target(e));
    if (!ctx) return;
    free(ctx->pixels);
    free(ctx);
}

static void img_apply_bg(lv_obj_t *obj, const char *name)
{
    uint16_t w = 0, h = 0;
    uint8_t *pixels = img_fetch(name, &w, &h);
    if (!pixels) return;

    bg_img_ctx_t *ctx = malloc(sizeof(*ctx));
    if (!ctx) {
        free(pixels);
        return;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->pixels = pixels;
    ctx->dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    ctx->dsc.header.always_zero = 0;
    ctx->dsc.header.reserved = 0;
    ctx->dsc.header.w = w;
    ctx->dsc.header.h = h;
    ctx->dsc.data_size = (uint32_t)w * h * sizeof(lv_color_t);
    ctx->dsc.data = pixels;

    lv_obj_set_user_data(obj, ctx);
    lv_obj_add_event_cb(obj, bg_img_delete_cb, LV_EVENT_DELETE, NULL);
    lv_obj_set_style_bg_img_src(obj, &ctx->dsc, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
}

/* ------------------------------------------------------------------ */
/* Grid container helpers                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    lv_coord_t *col_dsc;
    lv_coord_t *row_dsc;
} container_grid_ctx_t;

static void container_grid_delete_cb(lv_event_t *e)
{
    container_grid_ctx_t *ctx = lv_event_get_user_data(e);
    if (!ctx) return;

    if (ctx->col_dsc) lv_mem_free(ctx->col_dsc);
    if (ctx->row_dsc) lv_mem_free(ctx->row_dsc);
    lv_mem_free(ctx);
}

static void container_configure_grid(lv_obj_t *obj, int cols, int child_count)
{
    if (!obj) return;
    if (cols < 1) cols = 1;
    if (child_count < 1) child_count = 1;

    int rows = (child_count + cols - 1) / cols;
    if (rows < 1) rows = 1;

    container_grid_ctx_t *ctx = lv_mem_alloc(sizeof(*ctx));
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));

    ctx->col_dsc = lv_mem_alloc(sizeof(lv_coord_t) * (cols + 1));
    ctx->row_dsc = lv_mem_alloc(sizeof(lv_coord_t) * (rows + 1));

    if (!ctx->col_dsc || !ctx->row_dsc) {
        if (ctx->col_dsc) lv_mem_free(ctx->col_dsc);
        if (ctx->row_dsc) lv_mem_free(ctx->row_dsc);
        lv_mem_free(ctx);
        return;
    }

    for (int i = 0; i < cols; i++) ctx->col_dsc[i] = LV_GRID_FR(1);
    ctx->col_dsc[cols] = LV_GRID_TEMPLATE_LAST;

    for (int i = 0; i < rows; i++) ctx->row_dsc[i] = LV_GRID_CONTENT;
    ctx->row_dsc[rows] = LV_GRID_TEMPLATE_LAST;

    lv_obj_set_layout(obj, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(obj, ctx->col_dsc, ctx->row_dsc);
    lv_obj_add_event_cb(obj, container_grid_delete_cb, LV_EVENT_DELETE, ctx);
}

static lv_obj_t *build_widget(jparse_ctx_t *jctx, lv_obj_t *parent);

/* ------------------------------------------------------------------ */
/* Build widget                                                       */
/* ------------------------------------------------------------------ */

static lv_obj_t *build_widget(jparse_ctx_t *jctx, lv_obj_t *parent)
{
    char type[32] = {0};
    json_obj_get_string(jctx, "type", type, sizeof(type));
    if (!type[0]) return NULL;

    int x = 0, y = 0, w = 100, h = 20;
    json_obj_get_int(jctx, "x", &x);
    json_obj_get_int(jctx, "y", &y);
    json_obj_get_int(jctx, "w", &w);
    json_obj_get_int(jctx, "h", &h);

    lv_obj_t *obj = NULL;

    if (strcmp(type, "label") == 0) {
        obj = w_label_create(parent, x, y, w, h);

    } else if (strcmp(type, "button") == 0) {
        obj = w_button_create(parent, x, y, w, h);

    } else if (strcmp(type, "bar") == 0) {
        obj = w_bar_create(parent, x, y, w, h);

    } else if (strcmp(type, "graph") == 0) {
        graph_config_t gcfg = *theme_graph();
        char gstyle[16];

        if (json_obj_get_string(jctx, "graph_style", gstyle, sizeof(gstyle)) == OS_SUCCESS) {
            if      (strcmp(gstyle, "line") == 0)      gcfg.style = GRAPH_STYLE_LINE;
            else if (strcmp(gstyle, "area") == 0)      gcfg.style = GRAPH_STYLE_AREA;
            else if (strcmp(gstyle, "bars") == 0)      gcfg.style = GRAPH_STYLE_BARS;
            else if (strcmp(gstyle, "symmetric") == 0) gcfg.style = GRAPH_STYLE_SYMMETRIC;
        }

        int gint;
        if (json_obj_get_int(jctx, "bar_width", &gint) == OS_SUCCESS) gcfg.bar_width = gint;
        if (json_obj_get_int(jctx, "bar_gap", &gint) == OS_SUCCESS) gcfg.bar_gap = gint;
        if (json_obj_get_int(jctx, "line_width", &gint) == OS_SUCCESS) gcfg.line_width = gint;
        if (json_obj_get_int(jctx, "grid_lines", &gint) == OS_SUCCESS) gcfg.grid_h_lines = gint;
        if (json_obj_get_int(jctx, "y_min", &gint) == OS_SUCCESS) gcfg.y_min = gint;
        if (json_obj_get_int(jctx, "y_max", &gint) == OS_SUCCESS) gcfg.y_max = gint;
        if (json_obj_get_int(jctx, "point_count", &gint) == OS_SUCCESS) gcfg.point_count = gint;

        char gcol[16];
        if (json_obj_get_string(jctx, "color", gcol, sizeof(gcol)) == OS_SUCCESS)
            gcfg.color = theme_color_from_str(gcol);

        obj = w_graph_create(parent, x, y, w, h, &gcfg);

    } else if (strcmp(type, "sparkline") == 0) {
        obj = w_sparkline_create(parent, x, y, w, h, NULL);

    } else if (strcmp(type, "gauge") == 0) {
        obj = w_gauge_create(parent, x, y, w, h, NULL);

    } else if (strcmp(type, "spinner") == 0) {
        obj = w_spinner_create(parent, x, y, w, h);

    } else if (strcmp(type, "icon") == 0) {
        obj = w_icon_create(parent, x, y, w, h);

    } else if (strcmp(type, "image") == 0) {
        obj = w_image_create(parent, x, y, w, h);

        char src_name[64] = {0};
        if (json_obj_get_string(jctx, "src", src_name, sizeof(src_name)) == OS_SUCCESS && src_name[0]) {
            uint16_t iw = 0, ih = 0;
            uint8_t *pixels = img_fetch(src_name, &iw, &ih);
            if (pixels && !w_image_set_buf(obj, iw, ih, pixels)) {
                free(pixels);
            }
        }

    } else if (strcmp(type, "container") == 0) {
        char layout[16] = "row";
        json_obj_get_string(jctx, "layout", layout, sizeof(layout));
        obj = w_container_create(parent, x, y, w, h, layout);

    } else if (strcmp(type, "ticker") == 0) {
        obj = w_ticker_create(parent, x, y, w, h);

    } else {
        ESP_LOGW(TAG, "unknown type: %s", type);
        return NULL;
    }

    if (!obj) return NULL;

    apply_theme_ws(type, obj);

    widget_style_t ws = {0};
    if (json_obj_get_object(jctx, "style") == OS_SUCCESS) {
        ws_parse(jctx, &ws);
        ws_apply(&ws, obj, LV_PART_MAIN);

        if (json_obj_get_object(jctx, "indicator") == OS_SUCCESS) {
            widget_style_t ws_ind = {0};
            ws_parse(jctx, &ws_ind);
            ws_apply(&ws_ind, obj, LV_PART_INDICATOR);
            json_obj_leave_object(jctx);
        }

        json_obj_leave_object(jctx);
    }

    if (strcmp(type, "graph") == 0 && (ws.flags & WS_LINE_COLOR))
        w_graph_set_color(obj, ws.line_color);

    if (strcmp(type, "bar")       != 0 &&
        strcmp(type, "graph")     != 0 &&
        strcmp(type, "button")    != 0 &&
        strcmp(type, "sparkline") != 0 &&
        strcmp(type, "image")     != 0) {
        char bgname[64] = {0};
        if (json_obj_get_string(jctx, "bg_image", bgname, sizeof(bgname)) == OS_SUCCESS && bgname[0]) {
            img_apply_bg(obj, bgname);
        }
    }

    if (strcmp(type, "container") != 0 && strcmp(type, "ticker") != 0) {
        if (json_obj_get_object(jctx, "bind") == OS_SUCCESS) {
            apply_bind(jctx, type, obj);
            json_obj_leave_object(jctx);
        }
    }

    char action[64];
    if (json_obj_get_string(jctx, "on_tap", action, sizeof(action)) == OS_SUCCESS)
        w_button_set_tap_action(obj, action);
    if (json_obj_get_string(jctx, "on_hold", action, sizeof(action)) == OS_SUCCESS)
        w_button_set_hold_action(obj, action);

    /* container children */
    if (strcmp(type, "container") == 0) {
        char layout_type[16] = "row";
        json_obj_get_string(jctx, "layout", layout_type, sizeof(layout_type));

        int cols = 1;
        if (strcmp(layout_type, "grid") == 0) {
            if (json_obj_get_int(jctx, "cols", &cols) != OS_SUCCESS || cols < 1)
                cols = 1;
        }

        int num_children = 0;
        if (json_obj_get_array(jctx, "children", &num_children) == OS_SUCCESS) {
            if (strcmp(layout_type, "grid") == 0) {
                container_configure_grid(obj, cols, num_children);
            }

            for (int i = 0; i < num_children; i++) {
                if (json_arr_get_object(jctx, i) == OS_SUCCESS) {
                    lv_obj_t *child = build_widget(jctx, obj);

                    if (child && strcmp(layout_type, "grid") == 0) {
                        int col = i % cols;
                        int row = i / cols;
                        lv_obj_set_grid_cell(
                            child,
                            LV_GRID_ALIGN_STRETCH, col, 1,
                            LV_GRID_ALIGN_CENTER, row, 1
                        );
                    }

                    json_arr_leave_object(jctx);
                }
            }

            json_obj_leave_array(jctx);
        }
    }

    /* ticker frames */
    if (strcmp(type, "ticker") == 0) {
        int interval_ms = 100;
        json_obj_get_int(jctx, "interval_ms", &interval_ms);

        int num_frames = 0;
        if (json_obj_get_array(jctx, "frames", &num_frames) == OS_SUCCESS) {
            if (num_frames > 0 && num_frames <= 256) {
                const char **frames = calloc((size_t)num_frames, sizeof(*frames));
                char (*fdata)[32] = calloc((size_t)num_frames, sizeof(*fdata));

                if (frames && fdata) {
                    for (int i = 0; i < num_frames; i++) {
                        fdata[i][0] = '\0';
                        json_arr_get_string(jctx, i, fdata[i], sizeof(fdata[i]));
                        frames[i] = fdata[i];
                    }
                    w_ticker_start(obj, frames, num_frames, interval_ms);
                } else {
                    ESP_LOGE(TAG, "ticker alloc failed: %d frames", num_frames);
                }

                free(frames);
                free(fdata);
            } else if (num_frames > 256) {
                ESP_LOGE(TAG, "ticker too many frames: %d", num_frames);
            }

            json_obj_leave_array(jctx);
        }
    }

    return obj;
}

esp_err_t widget_factory_build(const char *layout_json)
{
    if (!layout_json) return ESP_ERR_INVALID_ARG;

    int len = (int)strlen(layout_json);
    size_t free_total = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t free_largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

    ESP_LOGI(TAG, "JSON %d bytes | heap free %d | largest block %d",
             len, (int)free_total, (int)free_largest);
    ESP_LOGI(TAG, "json[0..79]:  %.80s", layout_json);
    ESP_LOGI(TAG, "json[-79..]:  %.80s", layout_json + (len > 80 ? len - 80 : 0));

    jparse_ctx_t jctx;
    if (json_parse_start(&jctx, layout_json, len) != OS_SUCCESS) {
        ESP_LOGE(TAG, "json_parse_start failed");
        return ESP_FAIL;
    }

    int num_screens;
    if (json_obj_get_array(&jctx, "screens", &num_screens) != OS_SUCCESS) {
        ESP_LOGE(TAG, "no 'screens' array");
        json_parse_end(&jctx);
        return ESP_FAIL;
    }

    for (int i = 0; i < num_screens; i++) {
        if (json_arr_get_object(&jctx, i) != OS_SUCCESS) continue;

        char id[32] = "screen";
        json_obj_get_string(&jctx, "id", id, sizeof(id));

        lv_obj_t *scr = lv_obj_create(NULL);
        theme_apply(scr, "screen");
        screen_mgr_add(id, scr);

        int num_widgets;
        if (json_obj_get_array(&jctx, "widgets", &num_widgets) == OS_SUCCESS) {
            for (int j = 0; j < num_widgets; j++) {
                if (json_arr_get_object(&jctx, j) == OS_SUCCESS) {
                    build_widget(&jctx, scr);
                    json_arr_leave_object(&jctx);
                }
            }
            json_obj_leave_array(&jctx);
        }

        json_arr_leave_object(&jctx);
    }

    json_obj_leave_array(&jctx);
    json_parse_end(&jctx);
    ESP_LOGI(TAG, "layout built");
    return ESP_OK;
}