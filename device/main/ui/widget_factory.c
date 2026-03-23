#include "widget_factory.h"
#include "placeholder.h"
#include "screen_mgr.h"
#include "theme.h"
#include "../callback.h"
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


#define TAG "widget_factory"

void widget_factory_init(void) {}

static int field_id_for_key(const char *type, const char *key)
{
    if (strcmp(type, "label") == 0) {
        if (strcmp(key, "text")  == 0) return W_LABEL_TEXT;
    } else if (strcmp(type, "button") == 0) {
        if (strcmp(key, "label") == 0) return W_BUTTON_LABEL;
    } else if (strcmp(type, "bar") == 0) {
        if (strcmp(key, "value") == 0) return W_BAR_VALUE;
        if (strcmp(key, "max")   == 0) return W_BAR_MAX;
        if (strcmp(key, "label") == 0) return W_BAR_LABEL;
    } else if (strcmp(type, "graph") == 0) {
        if (strcmp(key, "value") == 0) return W_GRAPH_VALUE;
    } else if (strcmp(type, "sparkline") == 0) {
        if (strcmp(key, "value") == 0) return W_SPARKLINE_VALUE;
    } else if (strcmp(type, "gauge") == 0) {
        if (strcmp(key, "value") == 0) return W_GAUGE_VALUE;
        if (strcmp(key, "max")   == 0) return W_GAUGE_MAX;
        if (strcmp(key, "label") == 0) return W_GAUGE_LABEL;
    } else if (strcmp(type, "icon") == 0) {
        if (strcmp(key, "text")  == 0) return W_ICON_TEXT;
    }
    return -1;
}

static widget_set_field_fn set_field_fn_for_type(const char *type)
{
    if (strcmp(type, "label")     == 0) return w_label_set_field;
    if (strcmp(type, "button")    == 0) return w_button_set_field;
    if (strcmp(type, "bar")       == 0) return w_bar_set_field;
    if (strcmp(type, "graph")     == 0) return w_graph_set_field;
    if (strcmp(type, "sparkline") == 0) return w_sparkline_set_field;
    if (strcmp(type, "gauge")     == 0) return w_gauge_set_field;
    if (strcmp(type, "icon")      == 0) return w_icon_set_field;
    return NULL;
}

static const char *s_bind_keys[] = {"text", "label", "value", "max", "src", NULL};

/* Cursor is inside bind object when called. */
static void apply_bind(jparse_ctx_t *jctx, const char *type, lv_obj_t *obj)
{
    widget_set_field_fn fn = set_field_fn_for_type(type);
    for (int k = 0; s_bind_keys[k]; k++) {
        int fid = field_id_for_key(type, s_bind_keys[k]);
        if (fid < 0) continue;

        char sval[64];
        int  ival;
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

/* Aplikuje theme-level ws_* overrides dla danego typu widgetu. */
static void apply_theme_ws(const char *type, lv_obj_t *obj)
{
    theme_data_t *t = theme_data();
    if (strcmp(type, "label")     == 0) { ws_apply(&t->ws_label,    obj, LV_PART_MAIN); }
    else if (strcmp(type, "button") == 0) {
        ws_apply(&t->ws_button,          obj, LV_PART_MAIN);
        /* pressed_bg/text sa w ws_button — ws_apply kieruje je do STATE_PRESSED automatycznie */
    }
    else if (strcmp(type, "bar")    == 0) {
        ws_apply(&t->ws_bar,     obj, LV_PART_MAIN);
        ws_apply(&t->ws_bar_ind, obj, LV_PART_INDICATOR);
    }
    else if (strcmp(type, "graph")     == 0) { ws_apply(&t->ws_graph,     obj, LV_PART_MAIN); }
    else if (strcmp(type, "sparkline") == 0) { ws_apply(&t->ws_sparkline, obj, LV_PART_MAIN); }
    else if (strcmp(type, "gauge")     == 0) {
        ws_apply(&t->ws_gauge,     obj, LV_PART_MAIN);
        ws_apply(&t->ws_gauge_ind, obj, LV_PART_INDICATOR);
    }
    else if (strcmp(type, "spinner") == 0) {
        ws_apply(&t->ws_spinner,     obj, LV_PART_MAIN);
        ws_apply(&t->ws_spinner_ind, obj, LV_PART_INDICATOR);
    }
    else if (strcmp(type, "icon")      == 0) { ws_apply(&t->ws_icon,      obj, LV_PART_MAIN); }
    else if (strcmp(type, "container") == 0) { ws_apply(&t->ws_container, obj, LV_PART_MAIN); }
    else if (strcmp(type, "ticker")    == 0) { ws_apply(&t->ws_ticker,    obj, LV_PART_MAIN); }
    else if (strcmp(type, "screen")    == 0) { ws_apply(&t->ws_screen,    obj, LV_PART_MAIN); }
    else if (strcmp(type, "toggle")    == 0) {
        ws_apply(&t->ws_toggle,         obj, LV_PART_MAIN);
        ws_apply(&t->ws_toggle_checked, obj, LV_PART_MAIN | LV_STATE_CHECKED);
    }
}

/* Forward declaration for recursion. */
static void build_widget(jparse_ctx_t *jctx, lv_obj_t *parent);

/* Cursor is inside widget object when called; leaves cursor in same object. */
static void build_widget(jparse_ctx_t *jctx, lv_obj_t *parent)
{
    char type[32] = {0};
    json_obj_get_string(jctx, "type", type, sizeof(type));
    if (!type[0]) return;

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
        obj = w_graph_create(parent, x, y, w, h, NULL);
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
    } else if (strcmp(type, "container") == 0) {
        char layout[16] = "row";
        json_obj_get_string(jctx, "layout", layout, sizeof(layout));
        obj = w_container_create(parent, x, y, w, h, layout);
    } else if (strcmp(type, "ticker") == 0) {
        obj = w_ticker_create(parent, x, y, w, h);
    } else {
        ESP_LOGW(TAG, "unknown type: %s", type);
        return;
    }

    if (!obj) return;

    /* theme-level widget style overrides (ws_* z theme_data) */
    apply_theme_ws(type, obj);

    /* per-widget style override z layout JSON: "style": { ... } */
    if (json_obj_get_object(jctx, "style") == OS_SUCCESS) {
        widget_style_t ws = {0};
        ws_parse(jctx, &ws);
        ws_apply(&ws, obj, LV_PART_MAIN);
        /* jesli sa indicator/pressed — tez parsuj sub-obiekty */
        if (json_obj_get_object(jctx, "indicator") == OS_SUCCESS) {
            widget_style_t ws_ind = {0};
            ws_parse(jctx, &ws_ind);
            ws_apply(&ws_ind, obj, LV_PART_INDICATOR);
            json_obj_leave_object(jctx);
        }
        json_obj_leave_object(jctx);
    }

    /* bind (not for container/ticker) */
    if (strcmp(type, "container") != 0 && strcmp(type, "ticker") != 0) {
        if (json_obj_get_object(jctx, "bind") == OS_SUCCESS) {
            apply_bind(jctx, type, obj);
            json_obj_leave_object(jctx);
        }
    }

    /* on_tap / on_hold */
    char action[64];
    if (json_obj_get_string(jctx, "on_tap", action, sizeof(action)) == OS_SUCCESS)
        w_button_set_tap_action(obj, action);
    if (json_obj_get_string(jctx, "on_hold", action, sizeof(action)) == OS_SUCCESS)
        w_button_set_hold_action(obj, action);

    /* container children */
    if (strcmp(type, "container") == 0) {
        int num_children;
        if (json_obj_get_array(jctx, "children", &num_children) == OS_SUCCESS) {
            for (int i = 0; i < num_children; i++) {
                if (json_arr_get_object(jctx, i) == OS_SUCCESS) {
                    build_widget(jctx, obj);
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
        int num_frames;
        if (json_obj_get_array(jctx, "frames", &num_frames) == OS_SUCCESS && num_frames > 0) {
            const char **frames = malloc(num_frames * sizeof(char *));
            char (*fdata)[32]   = malloc(num_frames * 32);
            if (frames && fdata) {
                for (int i = 0; i < num_frames; i++) {
                    fdata[i][0] = '\0';
                    json_arr_get_string(jctx, i, fdata[i], 32);
                    frames[i] = fdata[i];
                }
                w_ticker_start(obj, frames, num_frames, interval_ms);
            }
            free(frames);
            free(fdata);
            json_obj_leave_array(jctx);
        }
    }
}

esp_err_t widget_factory_build(const char *layout_json)
{
    if (!layout_json) return ESP_ERR_INVALID_ARG;

    int len = (int)strlen(layout_json);
    size_t free_total   = heap_caps_get_free_size(MALLOC_CAP_8BIT);
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
