#include "theme.h"
#include <string.h>
#include <stdlib.h>
#include <json_parser.h>

static theme_data_t g_theme;

static lv_style_t style_screen;
static lv_style_t style_label;
static lv_style_t style_button;
static lv_style_t style_button_pressed;
static lv_style_t style_bar_bg;
static lv_style_t style_bar_ind;
static lv_style_t style_graph;
static lv_style_t style_sparkline;
static lv_style_t style_gauge;
static lv_style_t style_spinner;
static lv_style_t style_icon;
static lv_style_t style_container;
static lv_style_t style_ticker;
static lv_style_t style_value;
static lv_style_t style_toggle;
static lv_style_t style_toggle_checked;
static lv_style_t style_toggle_ind;
static lv_style_t style_toggle_knob;

theme_data_t *theme_data(void)              { return &g_theme; }
lv_color_t   theme_color(theme_color_role_t role) { return g_theme.palette[role]; }
const lv_font_t *theme_font(void)           { return g_theme.font; }

lv_color_t theme_color_from_str(const char *s)
{
    if (!s || !s[0]) return lv_color_hex(0x000000);
    if (s[0] == '#') {
        uint32_t c = (uint32_t)strtoul(s + 1, NULL, 16);
        return lv_color_hex(c);
    }
    /* nazwy palety */
    if (strcmp(s, "bg")     == 0) return g_theme.palette[THEME_BG];
    if (strcmp(s, "fg")     == 0) return g_theme.palette[THEME_FG];
    if (strcmp(s, "accent") == 0) return g_theme.palette[THEME_ACCENT];
    if (strcmp(s, "dim")    == 0) return g_theme.palette[THEME_DIM];
    if (strcmp(s, "danger") == 0) return g_theme.palette[THEME_DANGER];
    return lv_color_hex(0x000000);
}

void ws_parse(jparse_ctx_t *jctx, widget_style_t *ws)
{
    char sval[32];
    int  ival;

#define _COLOR(key, field, flag) \
    if (json_obj_get_string(jctx, key, sval, sizeof(sval)) == OS_SUCCESS) { \
        ws->field = theme_color_from_str(sval); ws->flags |= (flag); }

#define _INT(key, field, flag) \
    if (json_obj_get_int(jctx, key, &ival) == OS_SUCCESS) { \
        ws->field = (int16_t)ival; ws->flags |= (flag); }

#define _OPA(key, field, flag) \
    if (json_obj_get_int(jctx, key, &ival) == OS_SUCCESS) { \
        ws->field = (uint8_t)ival; ws->flags |= (flag); }

    _COLOR("bg_color",      bg_color,      WS_BG_COLOR)
    _OPA  ("bg_opa",        bg_opa,        WS_BG_OPA)
    _COLOR("border_color",  border_color,  WS_BORDER_COLOR)
    _INT  ("border_width",  border_width,  WS_BORDER_WIDTH)
    _OPA  ("border_opa",    border_opa,    WS_BORDER_OPA)
    _INT  ("radius",        radius,        WS_RADIUS)
    _INT  ("pad_top",       pad_top,       WS_PAD_TOP)
    _INT  ("pad_bottom",    pad_bottom,    WS_PAD_BOTTOM)
    _INT  ("pad_left",      pad_left,      WS_PAD_LEFT)
    _INT  ("pad_right",     pad_right,     WS_PAD_RIGHT)
    _INT  ("pad_gap",       pad_gap,       WS_PAD_GAP)
    _COLOR("text_color",    text_color,    WS_TEXT_COLOR)
    _OPA  ("text_opa",      text_opa,      WS_TEXT_OPA)
    _INT  ("letter_space",  letter_space,  WS_LETTER_SPACE)
    _INT  ("line_space",    line_space,    WS_LINE_SPACE)
    _COLOR("fill_color",    fill_color,    WS_FILL_COLOR)
    _OPA  ("fill_opa",      fill_opa,      WS_FILL_OPA)
    _COLOR("arc_color",     arc_color,     WS_ARC_COLOR)
    _INT  ("arc_width",     arc_width,     WS_ARC_WIDTH)
    _COLOR("line_color",    line_color,    WS_LINE_COLOR)
    _INT  ("line_width",    line_width,    WS_LINE_WIDTH)
    _COLOR("pressed_bg",    pressed_bg,    WS_PRESSED_BG)
    _COLOR("pressed_text",  pressed_text,  WS_PRESSED_TEXT)
    _COLOR("shadow_color",  shadow_color,  WS_SHADOW_COLOR)
    _INT  ("shadow_width",  shadow_width,  WS_SHADOW_WIDTH)
    _COLOR("outline_color", outline_color, WS_OUTLINE_COLOR)
    _INT  ("outline_width", outline_width, WS_OUTLINE_WIDTH)

    /* text_align: "left" | "center" | "right" */
    if (json_obj_get_string(jctx, "text_align", sval, sizeof(sval)) == OS_SUCCESS) {
        if      (strcmp(sval, "center") == 0) { ws->text_align = LV_TEXT_ALIGN_CENTER; ws->flags |= WS_TEXT_ALIGN; }
        else if (strcmp(sval, "right")  == 0) { ws->text_align = LV_TEXT_ALIGN_RIGHT;  ws->flags |= WS_TEXT_ALIGN; }
        else if (strcmp(sval, "left")   == 0) { ws->text_align = LV_TEXT_ALIGN_LEFT;   ws->flags |= WS_TEXT_ALIGN; }
    }

    /* justify (flex main axis): "start"|"end"|"center"|"space-between"|"space-around"|"space-evenly" */
    if (json_obj_get_string(jctx, "justify", sval, sizeof(sval)) == OS_SUCCESS) {
        lv_flex_align_t ja = LV_FLEX_ALIGN_START;
        bool ok = true;
        if      (strcmp(sval, "start")         == 0) ja = LV_FLEX_ALIGN_START;
        else if (strcmp(sval, "end")           == 0) ja = LV_FLEX_ALIGN_END;
        else if (strcmp(sval, "center")        == 0) ja = LV_FLEX_ALIGN_CENTER;
        else if (strcmp(sval, "space-between") == 0) ja = LV_FLEX_ALIGN_SPACE_BETWEEN;
        else if (strcmp(sval, "space-around")  == 0) ja = LV_FLEX_ALIGN_SPACE_AROUND;
        else if (strcmp(sval, "space-evenly")  == 0) ja = LV_FLEX_ALIGN_SPACE_EVENLY;
        else ok = false;
        if (ok) { ws->flex_justify = ja; ws->flags |= WS_FLEX_JUSTIFY; }
    }

    /* pad_all — skrot, ustawia wszystkie 4 strony */
    if (json_obj_get_int(jctx, "pad_all", &ival) == OS_SUCCESS) {
        ws->pad_top = ws->pad_bottom = ws->pad_left = ws->pad_right = (int16_t)ival;
        ws->flags |= WS_PAD_TOP | WS_PAD_BOTTOM | WS_PAD_LEFT | WS_PAD_RIGHT;
    }

    /* font po nazwie */
    if (json_obj_get_string(jctx, "font", sval, sizeof(sval)) == OS_SUCCESS) {
        const lv_font_t *f = theme_font_by_name(sval);
        if (f) { ws->font = f; ws->flags |= WS_FONT; }
    }

#undef _COLOR
#undef _INT
#undef _OPA
}

void ws_apply(const widget_style_t *ws, lv_obj_t *obj, lv_style_selector_t sel)
{
    if (!ws || !ws->flags) return;

    if (ws->flags & WS_BG_COLOR)     lv_obj_set_style_bg_color(obj, ws->bg_color, sel);
    if (ws->flags & WS_BG_OPA)       lv_obj_set_style_bg_opa(obj, ws->bg_opa, sel);
    if (ws->flags & WS_BORDER_COLOR) lv_obj_set_style_border_color(obj, ws->border_color, sel);
    if (ws->flags & WS_BORDER_WIDTH) lv_obj_set_style_border_width(obj, ws->border_width, sel);
    if (ws->flags & WS_BORDER_OPA)   lv_obj_set_style_border_opa(obj, ws->border_opa, sel);

    if (ws->flags & WS_RADIUS) {
        lv_obj_set_style_radius(obj, ws->radius, sel);
        lv_obj_set_style_clip_corner(obj, ws->radius > 0, sel);
    }

    if (ws->flags & WS_PAD_TOP)      lv_obj_set_style_pad_top(obj, ws->pad_top, sel);
    if (ws->flags & WS_PAD_BOTTOM)   lv_obj_set_style_pad_bottom(obj, ws->pad_bottom, sel);
    if (ws->flags & WS_PAD_LEFT)     lv_obj_set_style_pad_left(obj, ws->pad_left, sel);
    if (ws->flags & WS_PAD_RIGHT)    lv_obj_set_style_pad_right(obj, ws->pad_right, sel);
    if (ws->flags & WS_PAD_GAP) {
        lv_obj_set_style_pad_gap(obj, ws->pad_gap, sel);
        lv_obj_set_style_pad_row(obj, ws->pad_gap, sel);
        lv_obj_set_style_pad_column(obj, ws->pad_gap, sel);
    }

    if (ws->flags & WS_TEXT_COLOR)   lv_obj_set_style_text_color(obj, ws->text_color, sel);
    if (ws->flags & WS_TEXT_OPA)     lv_obj_set_style_text_opa(obj, ws->text_opa, sel);
    if (ws->flags & WS_LETTER_SPACE) lv_obj_set_style_text_letter_space(obj, ws->letter_space, sel);
    if (ws->flags & WS_LINE_SPACE)   lv_obj_set_style_text_line_space(obj, ws->line_space, sel);
    if (ws->flags & WS_FONT)         lv_obj_set_style_text_font(obj, ws->font, sel);
    if (ws->flags & WS_ARC_COLOR)    lv_obj_set_style_arc_color(obj, ws->arc_color, sel);
    if (ws->flags & WS_ARC_WIDTH)    lv_obj_set_style_arc_width(obj, ws->arc_width, sel);
    if (ws->flags & WS_LINE_COLOR)   lv_obj_set_style_line_color(obj, ws->line_color, sel);
    if (ws->flags & WS_LINE_WIDTH)   lv_obj_set_style_line_width(obj, ws->line_width, sel);
    if (ws->flags & WS_SHADOW_COLOR) lv_obj_set_style_shadow_color(obj, ws->shadow_color, sel);
    if (ws->flags & WS_SHADOW_WIDTH) lv_obj_set_style_shadow_width(obj, ws->shadow_width, sel);
    if (ws->flags & WS_OUTLINE_COLOR) lv_obj_set_style_outline_color(obj, ws->outline_color, sel);
    if (ws->flags & WS_OUTLINE_WIDTH) lv_obj_set_style_outline_width(obj, ws->outline_width, sel);
    if (ws->flags & WS_TEXT_ALIGN)    lv_obj_set_style_text_align(obj, ws->text_align, sel);
    if (ws->flags & WS_FLEX_JUSTIFY)  lv_obj_set_style_flex_main_place(obj, ws->flex_justify, sel);

    /* fill_* zawsze uderza w INDICATOR — bg i arc obie sciezki */
    if (ws->flags & WS_FILL_COLOR) {
        lv_obj_set_style_bg_color(obj, ws->fill_color, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(obj, ws->fill_color, LV_PART_INDICATOR);
    }
    if (ws->flags & WS_FILL_OPA) {
        lv_obj_set_style_bg_opa(obj, ws->fill_opa, LV_PART_INDICATOR);
        lv_obj_set_style_arc_opa(obj, ws->fill_opa, LV_PART_INDICATOR);
    }

    /* pressed_* zawsze uderza w STATE_PRESSED */
    if (ws->flags & WS_PRESSED_BG)
        lv_obj_set_style_bg_color(obj, ws->pressed_bg, LV_PART_MAIN | LV_STATE_PRESSED);
    if (ws->flags & WS_PRESSED_TEXT)
        lv_obj_set_style_text_color(obj, ws->pressed_text, LV_PART_MAIN | LV_STATE_PRESSED);
}

const lv_font_t *theme_font_by_name(const char *name)
{
    if (!name) return g_theme.font;
#if LV_FONT_UNSCII_8
    if (strcmp(name, "unscii_8")      == 0) return &lv_font_unscii_8;
#endif
#if LV_FONT_UNSCII_16
    if (strcmp(name, "unscii_16")     == 0) return &lv_font_unscii_16;
#endif
#if LV_FONT_MONTSERRAT_12
    if (strcmp(name, "montserrat_12") == 0) return &lv_font_montserrat_12;
#endif
#if LV_FONT_MONTSERRAT_14
    if (strcmp(name, "montserrat_14") == 0) return &lv_font_montserrat_14;
#endif
#if LV_FONT_MONTSERRAT_20
    if (strcmp(name, "montserrat_20") == 0) return &lv_font_montserrat_20;
#endif
    return g_theme.font;
}

const graph_config_t     *theme_graph(void)     { return &g_theme.graph; }
const sparkline_config_t *theme_sparkline(void) { return &g_theme.sparkline; }
int theme_spinner_speed_ms(void)    { return g_theme.spinner_speed_ms; }
int theme_spinner_arc_deg(void)     { return g_theme.spinner_arc_deg; }
int theme_container_gap(void)       { return g_theme.container_gap; }
int theme_gauge_arc_width(int dim)  { return LV_MAX(2, dim / 10); }
int theme_rotation(void)            { return g_theme.rotation; }

void theme_build_styles(void)
{
    lv_color_t *p = g_theme.palette;
    const lv_font_t *font = g_theme.font;

    lv_style_reset(&style_screen);
    lv_style_set_bg_color(&style_screen, p[THEME_BG]);
    lv_style_set_bg_opa(&style_screen, LV_OPA_COVER);
    lv_style_set_border_width(&style_screen, 0);
    lv_style_set_radius(&style_screen, 0);
    lv_style_set_pad_all(&style_screen, 0);

    lv_style_reset(&style_label);
    lv_style_set_text_color(&style_label, p[THEME_FG]);
    lv_style_set_bg_opa(&style_label, LV_OPA_TRANSP);
    lv_style_set_border_width(&style_label, 0);
    lv_style_set_text_font(&style_label, font);
    lv_style_set_pad_all(&style_label, 0);

    lv_style_reset(&style_button);
    lv_style_set_bg_color(&style_button, p[THEME_DIM]);
    lv_style_set_bg_opa(&style_button, LV_OPA_COVER);
    lv_style_set_text_color(&style_button, p[THEME_FG]);
    lv_style_set_text_font(&style_button, font);
    lv_style_set_border_width(&style_button, g_theme.button_border_w);
    lv_style_set_border_color(&style_button, p[THEME_FG]);
    lv_style_set_radius(&style_button, 0);
    lv_style_set_pad_all(&style_button, g_theme.button_pad);

    lv_style_reset(&style_button_pressed);
    lv_style_set_bg_color(&style_button_pressed, p[THEME_FG]);
    lv_style_set_text_color(&style_button_pressed, p[THEME_BG]);

    lv_style_reset(&style_bar_bg);
    lv_style_set_bg_color(&style_bar_bg, p[THEME_DIM]);
    lv_style_set_bg_opa(&style_bar_bg, LV_OPA_COVER);
    lv_style_set_border_width(&style_bar_bg, g_theme.bar_border_w);
    lv_style_set_border_color(&style_bar_bg, p[THEME_FG]);
    lv_style_set_radius(&style_bar_bg, 0);

    lv_style_reset(&style_bar_ind);
    lv_style_set_bg_color(&style_bar_ind, p[THEME_FG]);
    lv_style_set_bg_opa(&style_bar_ind, LV_OPA_COVER);
    lv_style_set_radius(&style_bar_ind, 0);

    lv_style_reset(&style_graph);
    lv_style_set_bg_color(&style_graph, p[THEME_BG]);
    lv_style_set_bg_opa(&style_graph, LV_OPA_COVER);
    lv_style_set_border_width(&style_graph, 1);
    lv_style_set_border_color(&style_graph, p[THEME_DIM]);
    lv_style_set_radius(&style_graph, 0);
    lv_style_set_pad_all(&style_graph, 0);

    lv_style_reset(&style_sparkline);
    lv_style_set_bg_opa(&style_sparkline, LV_OPA_TRANSP);
    lv_style_set_border_width(&style_sparkline, 0);
    lv_style_set_radius(&style_sparkline, 0);
    lv_style_set_pad_all(&style_sparkline, 0);

    lv_style_reset(&style_gauge);
    lv_style_set_bg_color(&style_gauge, p[THEME_BG]);
    lv_style_set_bg_opa(&style_gauge, LV_OPA_COVER);
    lv_style_set_border_width(&style_gauge, 0);

    lv_style_reset(&style_spinner);
    lv_style_set_bg_opa(&style_spinner, LV_OPA_TRANSP);
    lv_style_set_border_width(&style_spinner, 0);

    lv_style_reset(&style_icon);
    lv_style_set_text_color(&style_icon, p[THEME_ACCENT]);
    lv_style_set_bg_opa(&style_icon, LV_OPA_TRANSP);
    lv_style_set_border_width(&style_icon, 0);
    lv_style_set_text_font(&style_icon, font);
    lv_style_set_pad_all(&style_icon, 0);

    lv_style_reset(&style_container);
    lv_style_set_bg_color(&style_container, p[THEME_BG]);
    lv_style_set_bg_opa(&style_container, LV_OPA_COVER);
    lv_style_set_border_width(&style_container, 1);
    lv_style_set_border_color(&style_container, p[THEME_DIM]);
    lv_style_set_radius(&style_container, 0);
    lv_style_set_pad_all(&style_container, 0);
    lv_style_set_pad_gap(&style_container, g_theme.container_gap);

    lv_style_reset(&style_ticker);
    lv_style_set_text_color(&style_ticker, p[THEME_FG]);
    lv_style_set_bg_opa(&style_ticker, LV_OPA_TRANSP);
    lv_style_set_border_width(&style_ticker, 0);
    lv_style_set_text_font(&style_ticker, font);
    lv_style_set_pad_all(&style_ticker, 0);

    lv_style_reset(&style_value);
    lv_style_set_text_color(&style_value, p[THEME_ACCENT]);
    lv_style_set_bg_opa(&style_value, LV_OPA_TRANSP);
    lv_style_set_border_width(&style_value, 0);
    lv_style_set_text_font(&style_value, font);
    lv_style_set_pad_all(&style_value, 0);

    lv_style_reset(&style_toggle);
    lv_style_set_bg_color(&style_toggle, p[THEME_DIM]);
    lv_style_set_bg_opa(&style_toggle, LV_OPA_COVER);
    lv_style_set_border_width(&style_toggle, 1);
    lv_style_set_border_color(&style_toggle, p[THEME_FG]);
    lv_style_set_radius(&style_toggle, 0);

    lv_style_reset(&style_toggle_checked);
    lv_style_set_bg_color(&style_toggle_checked, p[THEME_ACCENT]);
    lv_style_set_bg_opa(&style_toggle_checked, LV_OPA_COVER);

    lv_style_reset(&style_toggle_ind);
    lv_style_set_bg_opa(&style_toggle_ind, LV_OPA_TRANSP);
    lv_style_set_radius(&style_toggle_ind, 0);

    lv_style_reset(&style_toggle_knob);
    lv_style_set_bg_color(&style_toggle_knob, p[THEME_FG]);
    lv_style_set_bg_opa(&style_toggle_knob, LV_OPA_COVER);
    lv_style_set_radius(&style_toggle_knob, 0);
    lv_style_set_pad_all(&style_toggle_knob, 2);
}

void theme_init(void)
{
    g_theme.palette[THEME_BG]     = lv_color_hex(0x040c02);
    g_theme.palette[THEME_FG]     = lv_color_hex(0x5fff35);
    g_theme.palette[THEME_ACCENT] = lv_color_hex(0xa0ff50);
    g_theme.palette[THEME_DIM]    = lv_color_hex(0x0c2006);
    g_theme.palette[THEME_DANGER] = lv_color_hex(0xc25757);

    g_theme.rotation         = 0;
    g_theme.font             = &lv_font_unscii_8;
    g_theme.button_pad       = 4;
    g_theme.button_border_w  = 1;
    g_theme.bar_border_w     = 1;
    g_theme.spinner_speed_ms = 1000;
    g_theme.spinner_arc_deg  = 60;
    g_theme.container_gap    = 2;

    g_theme.graph.style                 = GRAPH_STYLE_BARS;
    g_theme.graph.bar_width             = 2;
    g_theme.graph.bar_gap               = 1;
    g_theme.graph.line_width            = 1;
    g_theme.graph.color                 = lv_color_hex(0xa0ff50);
    g_theme.graph.fill_color            = g_theme.palette[THEME_DIM];
    g_theme.graph.fill_opa              = LV_OPA_50;
    g_theme.graph.grid_h_lines          = 2;
    g_theme.graph.grid_color            = lv_color_hex(0x0c2006);
    g_theme.graph.color_by_value        = false;
    g_theme.graph.threshold_count       = 0;
    g_theme.graph.thresholds[0]         = (graph_threshold_t){ .threshold = 50, .color = lv_color_hex(0x5fff35) };
    g_theme.graph.thresholds[1]         = (graph_threshold_t){ .threshold = 80, .color = lv_color_hex(0xa0ff50) };
    g_theme.graph.sym_color_by_distance = false;
    g_theme.graph.sym_center_color      = lv_color_hex(0x5fff35);
    g_theme.graph.sym_peak_color        = lv_color_hex(0xa0ff50);
    g_theme.graph.y_min                 = 0;
    g_theme.graph.y_max                 = 100;
    g_theme.graph.point_count           = 0;

    g_theme.sparkline.color       = lv_color_hex(0x5fff35);
    g_theme.sparkline.line_width  = 1;
    g_theme.sparkline.y_min       = 0;
    g_theme.sparkline.y_max       = 100;
    g_theme.sparkline.point_count = 0;

    memset(&g_theme.ws_screen,          0, sizeof(widget_style_t));
    memset(&g_theme.ws_label,           0, sizeof(widget_style_t));
    memset(&g_theme.ws_button,          0, sizeof(widget_style_t));
    memset(&g_theme.ws_button_pressed,  0, sizeof(widget_style_t));
    memset(&g_theme.ws_bar,             0, sizeof(widget_style_t));
    memset(&g_theme.ws_bar_ind,         0, sizeof(widget_style_t));
    memset(&g_theme.ws_graph,           0, sizeof(widget_style_t));
    memset(&g_theme.ws_sparkline,       0, sizeof(widget_style_t));
    memset(&g_theme.ws_gauge,           0, sizeof(widget_style_t));
    memset(&g_theme.ws_gauge_ind,       0, sizeof(widget_style_t));
    memset(&g_theme.ws_spinner,         0, sizeof(widget_style_t));
    memset(&g_theme.ws_spinner_ind,     0, sizeof(widget_style_t));
    memset(&g_theme.ws_icon,            0, sizeof(widget_style_t));
    memset(&g_theme.ws_container,       0, sizeof(widget_style_t));
    memset(&g_theme.ws_ticker,          0, sizeof(widget_style_t));
    memset(&g_theme.ws_value,           0, sizeof(widget_style_t));
    memset(&g_theme.ws_toggle,          0, sizeof(widget_style_t));
    memset(&g_theme.ws_toggle_checked,  0, sizeof(widget_style_t));

    lv_disp_set_theme(lv_disp_get_default(), NULL);

    lv_style_init(&style_screen);
    lv_style_init(&style_label);
    lv_style_init(&style_button);
    lv_style_init(&style_button_pressed);
    lv_style_init(&style_bar_bg);
    lv_style_init(&style_bar_ind);
    lv_style_init(&style_graph);
    lv_style_init(&style_sparkline);
    lv_style_init(&style_gauge);
    lv_style_init(&style_spinner);
    lv_style_init(&style_icon);
    lv_style_init(&style_container);
    lv_style_init(&style_ticker);
    lv_style_init(&style_value);
    lv_style_init(&style_toggle);
    lv_style_init(&style_toggle_checked);
    lv_style_init(&style_toggle_ind);
    lv_style_init(&style_toggle_knob);

    theme_build_styles();
}

esp_err_t theme_load_json(const char *json)
{
    jparse_ctx_t jctx;
    if (json_parse_start(&jctx, json, (int)strlen(json)) != OS_SUCCESS)
        return ESP_FAIL;

    if (json_obj_get_object(&jctx, "palette") == OS_SUCCESS) {
        static const struct { const char *key; theme_color_role_t role; } map[] = {
            {"bg",     THEME_BG},
            {"fg",     THEME_FG},
            {"accent", THEME_ACCENT},
            {"dim",    THEME_DIM},
            {"danger", THEME_DANGER},
        };
        char colorstr[16];
        for (int i = 0; i < 5; i++) {
            if (json_obj_get_string(&jctx, map[i].key, colorstr, sizeof(colorstr)) == OS_SUCCESS
                    && colorstr[0] == '#') {
                uint32_t c = (uint32_t)strtoul(colorstr + 1, NULL, 16);
                g_theme.palette[map[i].role] = lv_color_hex(c);
            }
        }
        json_obj_leave_object(&jctx);
    }

    int rot_val;
    if (json_obj_get_int(&jctx, "rotation", &rot_val) == OS_SUCCESS) {
        if (rot_val == 90 || rot_val == 180 || rot_val == 270)
            g_theme.rotation = rot_val;
        else
            g_theme.rotation = 0;
    }

    char fontname[32];
    if (json_obj_get_string(&jctx, "font", fontname, sizeof(fontname)) == OS_SUCCESS) {
        const lv_font_t *f = theme_font_by_name(fontname);
        if (f) g_theme.font = f;
    }

    if (json_obj_get_object(&jctx, "widgets") == OS_SUCCESS) {

#define _WS_TYPE(key, field) \
        if (json_obj_get_object(&jctx, key) == OS_SUCCESS) { \
            ws_parse(&jctx, &g_theme.field); \
            json_obj_leave_object(&jctx); \
        }

#define _WS_TYPE_IND(key, field, ind_field) \
        if (json_obj_get_object(&jctx, key) == OS_SUCCESS) { \
            ws_parse(&jctx, &g_theme.field); \
            if (json_obj_get_object(&jctx, "indicator") == OS_SUCCESS) { \
                ws_parse(&jctx, &g_theme.ind_field); \
                json_obj_leave_object(&jctx); \
            } \
            json_obj_leave_object(&jctx); \
        }

        _WS_TYPE    ("screen",    ws_screen)
        _WS_TYPE    ("label",     ws_label)
        _WS_TYPE    ("button",    ws_button)
        _WS_TYPE    ("icon",      ws_icon)
        _WS_TYPE    ("ticker",    ws_ticker)
        _WS_TYPE    ("value",     ws_value)
        _WS_TYPE    ("container", ws_container)
        _WS_TYPE_IND("bar",     ws_bar,     ws_bar_ind)
        _WS_TYPE_IND("gauge",   ws_gauge,   ws_gauge_ind)
        _WS_TYPE_IND("spinner", ws_spinner, ws_spinner_ind)

        if (json_obj_get_object(&jctx, "toggle") == OS_SUCCESS) {
            ws_parse(&jctx, &g_theme.ws_toggle);
            if (json_obj_get_object(&jctx, "checked") == OS_SUCCESS) {
                ws_parse(&jctx, &g_theme.ws_toggle_checked);
                json_obj_leave_object(&jctx);
            }
            json_obj_leave_object(&jctx);
        }

        if (json_obj_get_object(&jctx, "graph") == OS_SUCCESS) {
            ws_parse(&jctx, &g_theme.ws_graph);

            char gstr[16];
            int  gint;
            if (json_obj_get_string(&jctx, "style", gstr, sizeof(gstr)) == OS_SUCCESS) {
                if      (strcmp(gstr, "line")      == 0) g_theme.graph.style = GRAPH_STYLE_LINE;
                else if (strcmp(gstr, "area")      == 0) g_theme.graph.style = GRAPH_STYLE_AREA;
                else if (strcmp(gstr, "bars")      == 0) g_theme.graph.style = GRAPH_STYLE_BARS;
                else if (strcmp(gstr, "symmetric") == 0) g_theme.graph.style = GRAPH_STYLE_SYMMETRIC;
            }

#define _GI(key, field) \
            if (json_obj_get_int(&jctx, key, &gint) == OS_SUCCESS) g_theme.graph.field = gint;
            _GI("bar_width",    bar_width)
            _GI("bar_gap",      bar_gap)
            _GI("line_width",   line_width)
            _GI("grid_h_lines", grid_h_lines)
            _GI("y_min",        y_min)
            _GI("y_max",        y_max)
            _GI("point_count",  point_count)
            _GI("fill_opa",     fill_opa)
#undef _GI

#define _GC(key, field) \
            if (json_obj_get_string(&jctx, key, gstr, sizeof(gstr)) == OS_SUCCESS) \
                g_theme.graph.field = theme_color_from_str(gstr);
            _GC("color",      color)
            _GC("fill_color", fill_color)
            _GC("grid_color", grid_color)
#undef _GC

            if (json_obj_get_int(&jctx, "color_by_value", &gint) == OS_SUCCESS)
                g_theme.graph.color_by_value = (gint != 0);

            int nth;
            if (json_obj_get_array(&jctx, "thresholds", &nth) == OS_SUCCESS) {
                int n = LV_MIN(nth, GRAPH_MAX_THRESHOLDS);
                g_theme.graph.threshold_count = n;
                for (int ti = 0; ti < n; ti++) {
                    if (json_arr_get_object(&jctx, ti) == OS_SUCCESS) {
                        if (json_obj_get_int(&jctx, "threshold", &gint) == OS_SUCCESS)
                            g_theme.graph.thresholds[ti].threshold = gint;
                        if (json_obj_get_string(&jctx, "color", gstr, sizeof(gstr)) == OS_SUCCESS)
                            g_theme.graph.thresholds[ti].color = theme_color_from_str(gstr);
                        json_arr_leave_object(&jctx);
                    }
                }
                json_obj_leave_array(&jctx);
            }

            if (json_obj_get_int(&jctx, "sym_color_by_distance", &gint) == OS_SUCCESS)
                g_theme.graph.sym_color_by_distance = (gint != 0);
            if (json_obj_get_string(&jctx, "sym_center_color", gstr, sizeof(gstr)) == OS_SUCCESS)
                g_theme.graph.sym_center_color = theme_color_from_str(gstr);
            if (json_obj_get_string(&jctx, "sym_peak_color", gstr, sizeof(gstr)) == OS_SUCCESS)
                g_theme.graph.sym_peak_color = theme_color_from_str(gstr);

            json_obj_leave_object(&jctx);
        }

        if (json_obj_get_object(&jctx, "sparkline") == OS_SUCCESS) {
            ws_parse(&jctx, &g_theme.ws_sparkline);
            char sstr[16];
            int sint;
            if (json_obj_get_string(&jctx, "color", sstr, sizeof(sstr)) == OS_SUCCESS)
                g_theme.sparkline.color = theme_color_from_str(sstr);
            if (json_obj_get_int(&jctx, "line_width",   &sint) == OS_SUCCESS) g_theme.sparkline.line_width  = sint;
            if (json_obj_get_int(&jctx, "y_min",        &sint) == OS_SUCCESS) g_theme.sparkline.y_min       = sint;
            if (json_obj_get_int(&jctx, "y_max",        &sint) == OS_SUCCESS) g_theme.sparkline.y_max       = sint;
            if (json_obj_get_int(&jctx, "point_count",  &sint) == OS_SUCCESS) g_theme.sparkline.point_count = sint;
            json_obj_leave_object(&jctx);
        }

#undef _WS_TYPE
#undef _WS_TYPE_IND

        json_obj_leave_object(&jctx);
    }

    json_parse_end(&jctx);
    return ESP_OK;
}

void theme_apply(lv_obj_t *obj, const char *widget_type)
{
    if (strcmp(widget_type, "screen") == 0) {
        lv_obj_remove_style_all(obj);
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_style(obj, &style_screen, LV_PART_MAIN);
    } else if (strcmp(widget_type, "label") == 0) {
        lv_obj_add_style(obj, &style_label, LV_PART_MAIN);
    } else if (strcmp(widget_type, "button") == 0) {
        lv_obj_add_style(obj, &style_button,         LV_PART_MAIN);
        lv_obj_add_style(obj, &style_button_pressed, LV_PART_MAIN | LV_STATE_PRESSED);
    } else if (strcmp(widget_type, "bar") == 0) {
        lv_obj_add_style(obj, &style_bar_bg,  LV_PART_MAIN);
        lv_obj_add_style(obj, &style_bar_ind, LV_PART_INDICATOR);
    } else if (strcmp(widget_type, "graph") == 0) {
        lv_obj_add_style(obj, &style_graph, LV_PART_MAIN);
    } else if (strcmp(widget_type, "sparkline") == 0) {
        lv_obj_add_style(obj, &style_sparkline, LV_PART_MAIN);
    } else if (strcmp(widget_type, "gauge") == 0) {
        lv_obj_add_style(obj, &style_gauge, LV_PART_MAIN);
    } else if (strcmp(widget_type, "spinner") == 0) {
        lv_obj_add_style(obj, &style_spinner, LV_PART_MAIN);
    } else if (strcmp(widget_type, "icon") == 0) {
        lv_obj_add_style(obj, &style_icon, LV_PART_MAIN);
    } else if (strcmp(widget_type, "container") == 0) {
        lv_obj_add_style(obj, &style_container, LV_PART_MAIN);
    } else if (strcmp(widget_type, "ticker") == 0) {
        lv_obj_add_style(obj, &style_ticker, LV_PART_MAIN);
    } else if (strcmp(widget_type, "value") == 0) {
        lv_obj_add_style(obj, &style_value, LV_PART_MAIN);
    } else if (strcmp(widget_type, "toggle") == 0) {
        lv_obj_add_style(obj, &style_toggle,         LV_PART_MAIN);
        lv_obj_add_style(obj, &style_toggle_checked, LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_add_style(obj, &style_toggle_ind,     LV_PART_INDICATOR);
        lv_obj_add_style(obj, &style_toggle_knob,    LV_PART_KNOB);
    }
}