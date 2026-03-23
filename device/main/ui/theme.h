#pragma once

#include <lvgl.h>
#include <stdbool.h>
#include <esp_err.h>
#include <json_parser.h>

/* ------------------------------------------------------------------ */
/* Kolory                                                               */
/* ------------------------------------------------------------------ */

typedef enum {
    THEME_BG,
    THEME_FG,
    THEME_ACCENT,
    THEME_DIM,
    THEME_DANGER,
    THEME_COLOR_COUNT
} theme_color_role_t;

/* ------------------------------------------------------------------ */
/* graph_config_t                                                       */
/* ------------------------------------------------------------------ */

typedef enum {
    GRAPH_STYLE_LINE,
    GRAPH_STYLE_AREA,
    GRAPH_STYLE_BARS,
    GRAPH_STYLE_SYMMETRIC,
} graph_style_t;

#define GRAPH_MAX_THRESHOLDS 4

typedef struct {
    int        threshold;
    lv_color_t color;
} graph_threshold_t;

typedef struct {
    graph_style_t style;
    int bar_width;
    int bar_gap;
    int line_width;
    lv_color_t color;
    lv_color_t fill_color;
    uint8_t    fill_opa;
    int        grid_h_lines;
    lv_color_t grid_color;
    bool              color_by_value;
    graph_threshold_t thresholds[GRAPH_MAX_THRESHOLDS];
    int               threshold_count;
    bool       sym_color_by_distance;
    lv_color_t sym_center_color;
    lv_color_t sym_peak_color;
    int y_min;
    int y_max;
    int point_count;
} graph_config_t;

/* ------------------------------------------------------------------ */
/* sparkline_config_t                                                   */
/* ------------------------------------------------------------------ */

typedef struct {
    lv_color_t color;
    int        line_width;
    int        y_min;
    int        y_max;
    int        point_count;
} sparkline_config_t;

/* ------------------------------------------------------------------ */
/* widget_style_t                                                       */
/*                                                                      */
/* Bitmaskowy zestaw overrides stylu dla konkretnego widgetu.          */
/* Ustawione flagi sa aplikowane przez ws_apply() jako lokalne style   */
/* (najwyzszy priorytet w LVGL, nadpisuja theme_apply()).              */
/*                                                                      */
/* Zrodla: theme.json "widgets.TYPE.*" i layout.json "style: {}".     */
/* ------------------------------------------------------------------ */

/* Flagi — jakie pola sa wazne */
#define WS_BG_COLOR       (1u << 0)
#define WS_BG_OPA         (1u << 1)
#define WS_BORDER_COLOR   (1u << 2)
#define WS_BORDER_WIDTH   (1u << 3)
#define WS_BORDER_OPA     (1u << 4)
#define WS_RADIUS         (1u << 5)
#define WS_PAD_TOP        (1u << 6)
#define WS_PAD_BOTTOM     (1u << 7)
#define WS_PAD_LEFT       (1u << 8)
#define WS_PAD_RIGHT      (1u << 9)
#define WS_PAD_GAP        (1u << 10)
#define WS_TEXT_COLOR     (1u << 11)
#define WS_TEXT_OPA       (1u << 12)
#define WS_LETTER_SPACE   (1u << 13)
#define WS_LINE_SPACE     (1u << 14)
#define WS_FONT           (1u << 15)
#define WS_FILL_COLOR     (1u << 16)  /* bg INDICATOR (bar fill) + arc INDICATOR */
#define WS_FILL_OPA       (1u << 17)
#define WS_ARC_COLOR      (1u << 18)  /* arc MAIN track */
#define WS_ARC_WIDTH      (1u << 19)
#define WS_LINE_COLOR     (1u << 20)
#define WS_LINE_WIDTH     (1u << 21)
#define WS_PRESSED_BG     (1u << 22)  /* bg przy STATE_PRESSED */
#define WS_PRESSED_TEXT   (1u << 23)
#define WS_SHADOW_COLOR   (1u << 24)
#define WS_SHADOW_WIDTH   (1u << 25)
#define WS_OUTLINE_COLOR  (1u << 26)
#define WS_OUTLINE_WIDTH  (1u << 27)

typedef struct {
    uint32_t flags;

    lv_color_t  bg_color;
    uint8_t     bg_opa;

    lv_color_t  border_color;
    int16_t     border_width;
    uint8_t     border_opa;

    int16_t     radius;

    int16_t     pad_top, pad_bottom, pad_left, pad_right;
    int16_t     pad_gap;

    lv_color_t  text_color;
    uint8_t     text_opa;
    int16_t     letter_space;
    int16_t     line_space;
    const lv_font_t *font;

    lv_color_t  fill_color;   /* LV_PART_INDICATOR: bg_color + arc_color */
    uint8_t     fill_opa;

    lv_color_t  arc_color;    /* LV_PART_MAIN arc track */
    int16_t     arc_width;

    lv_color_t  line_color;
    int16_t     line_width;

    lv_color_t  pressed_bg;
    lv_color_t  pressed_text;

    lv_color_t  shadow_color;
    int16_t     shadow_width;

    lv_color_t  outline_color;
    int16_t     outline_width;
} widget_style_t;

/* ------------------------------------------------------------------ */
/* theme_data_t                                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    lv_color_t palette[THEME_COLOR_COUNT];
    const lv_font_t *font;

    /* Zachowane dla theme_build_styles() — nie konfigurowalny przez JSON */
    int button_pad;
    int button_border_w;
    int bar_border_w;
    int spinner_speed_ms;
    int spinner_arc_deg;
    int container_gap;

    graph_config_t     graph;
    sparkline_config_t sparkline;

    /* Per-type widget style overrides (flags==0 = brak overridera) */
    widget_style_t ws_screen;
    widget_style_t ws_label;
    widget_style_t ws_button;
    widget_style_t ws_button_pressed;
    widget_style_t ws_bar;
    widget_style_t ws_bar_ind;
    widget_style_t ws_graph;
    widget_style_t ws_sparkline;
    widget_style_t ws_gauge;
    widget_style_t ws_gauge_ind;
    widget_style_t ws_spinner;
    widget_style_t ws_spinner_ind;
    widget_style_t ws_icon;
    widget_style_t ws_container;
    widget_style_t ws_ticker;
    widget_style_t ws_value;
    widget_style_t ws_toggle;
    widget_style_t ws_toggle_checked;
} theme_data_t;

/* ------------------------------------------------------------------ */
/* API                                                                  */
/* ------------------------------------------------------------------ */

void theme_init(void);
void theme_build_styles(void);

lv_color_t          theme_color(theme_color_role_t role);
const lv_font_t    *theme_font(void);
const lv_font_t    *theme_font_by_name(const char *name);

const graph_config_t     *theme_graph(void);
const sparkline_config_t *theme_sparkline(void);

int theme_spinner_speed_ms(void);
int theme_spinner_arc_deg(void);
int theme_container_gap(void);
int theme_gauge_arc_width(int widget_min_dim);

void theme_apply(lv_obj_t *obj, const char *widget_type);

/* Parsuje kolor: nazwe palety ("fg","accent",...) lub hex ("#rrggbb") */
lv_color_t theme_color_from_str(const char *s);

/* Parsuje widget_style_t z aktualnej pozycji w jparse_ctx (obiekt JSON).
   Kursor musi byc wewnatrz obiektu stylu; nie wywoluje leave_object. */
void ws_parse(jparse_ctx_t *jctx, widget_style_t *ws);

/* Aplikuje ustawione flagi ws jako lokalne style obiektu LVGL.
   selector = LV_PART_MAIN, LV_PART_INDICATOR itp.
   WS_FILL_* zawsze uderza w LV_PART_INDICATOR.
   WS_PRESSED_* zawsze uderza w LV_STATE_PRESSED. */
void ws_apply(const widget_style_t *ws, lv_obj_t *obj, lv_style_selector_t selector);

/* Graceful merge: missing keys keep previous value.
   Call theme_build_styles() + lv_obj_report_style_change(NULL) after. */
esp_err_t theme_load_json(const char *json);

theme_data_t *theme_data(void);
