#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_system.h>
#include <stdio.h>
#include <string.h>
#include <json_parser.h>
#include <lvgl.h>

#include "hw/display.h"
#include "hw/lvgl_port.h"
#include "hw/touch.h"

#include "ui/theme.h"
#include "ui/screen_mgr.h"
#include "ui/placeholder.h"
#include "ui/widget_factory.h"
#include "ui/widgets/w_ticker.h"

#include "net/wifi.h"
#include "net/http_fetch.h"
#include "net/ws.h"

#include "callback.h"
#include "sdkconfig.h"

#define TAG "main"

/* ------------------------------------------------------------------ */
/* Boot render task                                                     */
/*                                                                     */
/* Wywoluje lv_timer_handler() co ~16ms (ok. 60fps) przez caly czas   */
/* fazy boot. Dzieki temu animacje (ticker, spinner) dzialaja rowniez  */
/* gdy glowny task jest zablokowany przez wifi_connect/http_fetch.     */
/* ------------------------------------------------------------------ */

static volatile bool g_boot_done        = false;
static volatile bool g_render_task_done = false;

static void boot_render_task(void *arg)
{
    while (!g_boot_done) {
        lvgl_port_lock();
        lv_timer_handler();
        lvgl_port_unlock();
        vTaskDelay(pdMS_TO_TICKS(16));
    }
    /* Sygnalizuj ze task skonczyl dostep do LVGL zanim sie usunie. */
    g_render_task_done = true;
    vTaskDelete(NULL);
}

/* ------------------------------------------------------------------ */
/* Loading screen                                                       */
/* ------------------------------------------------------------------ */

static struct {
    lv_obj_t *wifi_right;
    lv_obj_t *cfg_right;
    lv_obj_t *layout_right;
    lv_obj_t *status_lbl;
    lv_obj_t *progress_bar;
} g_boot;

/* Kolory hardcoded — theme nie jest jeszcze zaladowany z serwera */
#define BOOT_BG     lv_color_hex(0x040c02)
#define BOOT_FG     lv_color_hex(0x5fff35)
#define BOOT_ACCENT lv_color_hex(0xa0ff50)
#define BOOT_DIM    lv_color_hex(0x0c2006)

static lv_obj_t *boot_sep(lv_obj_t *parent, int y, lv_color_t col)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, 480, 2);
    lv_obj_set_pos(obj, 0, y);
    lv_obj_set_style_bg_color(obj, col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

/* unscii_8: 8px/char → 60 chars = 480px szerokosci ekranu */
static lv_obj_t *boot_lbl(lv_obj_t *parent, int x, int y, int w, const char *txt, lv_color_t col)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_obj_set_pos(lbl, x, y);
    lv_obj_set_size(lbl, w, 12);
    lv_label_set_text(lbl, txt);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(lbl, col, LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_unscii_8, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lbl, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(lbl, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(lbl, 0, LV_PART_MAIN);
    return lbl;
}

/* Prawa kolumna statusu: x=432, w=48 (6 znakow × 8px).
 * Spinner kreciacy sie dopoki nie zostanie zastapiony przez "[ OK ]". */
static lv_obj_t *boot_spin(lv_obj_t *parent, int y)
{
    lv_obj_t *t = w_ticker_create(parent, 432, y, 48, 12);
    lv_obj_set_style_text_color(t, BOOT_FG, LV_PART_MAIN);
    lv_obj_set_style_text_font(t, &lv_font_unscii_8, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(t, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(t, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(t, 0, LV_PART_MAIN);
    static const char *spin_frames[] = {"  |   ", "  /   ", "  -   ", "  \\   "};
    w_ticker_start(t, spin_frames, 4, 150);
    return t;
}

/* Zatrzymuje spinner i wyswietla "[ OK ]" kolorem accent. */
static void boot_ok(lv_obj_t *right)
{
    w_ticker_stop(right);
    lv_label_set_text(right, "[ OK ]");
    lv_obj_set_style_text_color(right, BOOT_ACCENT, LV_PART_MAIN);
}

/*
 * Layout: 480x320, wszedzie unscii_8 (8px/char, 60 chars/row).
 * Lewa kolumna: x=4, w=424 (53 znaki max).
 * Prawa kolumna statusu: x=432, w=48 (6 znakow: "[ OK ]").
 * Kazda linia tekstu: h=12. Separatory: h=2.
 *
 * y=  0  sep accent (h=2)
 * y=  4  blink ticker (x=4, w=8) + tytul (x=16, w=464)    h=12
 * y= 18  sep fg
 * y= 24  podtytul                                           h=12
 * y= 38  sep fg
 * y= 46  HW left   + "[ OK ]" right                        h=12
 * y= 60  DISP left + "[ OK ]" right                        h=12
 * y= 74  TOUCH left+ "[ OK ]" right                        h=12
 * y= 88  WIFI left + spinner right           <- update      h=12
 * y=102  CFG left  + spinner right           <- update      h=12
 * y=116  LAYOUT left + spinner right         <- update      h=12
 * y=130  sep fg
 * y=136  >> PLEASE STAND BY                                 h=12
 * y=150  sep fg
 * y=156  status_lbl                          <- update      h=12
 * y=170  sep fg
 * y=176  progress bar                        <- update      h=16
 * y=194  sep fg
 * y=200  sys info                                           h=12
 * y=214  sep fg
 * y=220  hex dump                                           h=12
 * y=234  sep fg
 * y=240  SCANNER ticker, full width 480px                   h=12
 * y=254  sep fg
 * y=260  caution                                            h=12
 * y=274  sep fg
 * y=280  footer                                             h=12
 * y=294  sep accent (h=2) → konczy sie na y=296 < 320  ✓
 */
static void make_loading_screen(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, BOOT_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    /* ── header ── */
    boot_sep(scr, 0, BOOT_ACCENT);

    /* migajacy znaczek > (► nie jest w unscii_8) */
    lv_obj_t *blink = w_ticker_create(scr, 4, 4, 8, 12);
    lv_obj_set_style_text_color(blink, BOOT_ACCENT, LV_PART_MAIN);
    lv_obj_set_style_text_font(blink, &lv_font_unscii_8, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(blink, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(blink, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(blink, 0, LV_PART_MAIN);
    static const char *blink_frames[] = {">", " "};
    w_ticker_start(blink, blink_frames, 2, 500);

    /* tytul; x=16 zeby nie nachodzic na blink ticker (8px) */
    boot_lbl(scr, 16, 4, 464,
             "PIP-BOY 3000 MKII  //  ROBCO INDUSTRIES UNIFIED OS",
             BOOT_ACCENT);

    boot_sep(scr, 18, BOOT_FG);
    boot_lbl(scr, 4, 24, 472,
             "UNIFIED OS v2.3.1  (C) 2077 ROBCO INDUSTRIES",
             BOOT_FG);
    boot_sep(scr, 38, BOOT_FG);

    /* ── boot log: lewa kolumna (x=4, w=424) + prawa kolumna (x=432, w=48) ── */

    /* linie statyczne — zawsze DONE */
    boot_lbl(scr, 4,  46, 424,
             "HARDWARE INITIALIZATION ..........................", BOOT_FG);
    boot_lbl(scr, 432, 46, 48, "[ OK ]", BOOT_ACCENT);

    boot_lbl(scr, 4,  60, 424,
             "DISPLAY CONTROLLER ILI9488 .......................", BOOT_FG);
    boot_lbl(scr, 432, 60, 48, "[ OK ]", BOOT_ACCENT);

    boot_lbl(scr, 4,  74, 424,
             "TOUCH INTERFACE XPT2046 ..........................", BOOT_FG);
    boot_lbl(scr, 432, 74, 48, "[ OK ]", BOOT_ACCENT);

    /* linie dynamiczne — prawa kolumna kreciw sie dopoki nie DONE */
    boot_lbl(scr, 4,  88, 424,
             "WIRELESS NETWORK IEEE 802.11n ....................", BOOT_FG);
    g_boot.wifi_right = boot_spin(scr, 88);

    boot_lbl(scr, 4, 102, 424,
             "THEME CONFIGURATION ..............................", BOOT_FG);
    g_boot.cfg_right = boot_spin(scr, 102);

    boot_lbl(scr, 4, 116, 424,
             "LAYOUT DATA ......................................", BOOT_FG);
    g_boot.layout_right = boot_spin(scr, 116);

    boot_sep(scr, 130, BOOT_FG);

    /* ── status ── */
    boot_lbl(scr, 4, 136, 472, "  >>  PLEASE STAND BY", BOOT_ACCENT);
    boot_sep(scr, 150, BOOT_FG);

    g_boot.status_lbl = boot_lbl(scr, 4, 156, 472,
        "STATUS: SEARCHING FOR WIRELESS NETWORK...", BOOT_FG);
    boot_sep(scr, 170, BOOT_FG);

    /* ── progress bar ── */
    lv_obj_t *pbar = lv_bar_create(scr);
    lv_obj_set_size(pbar, 464, 16);
    lv_obj_set_pos(pbar, 8, 176);
    lv_bar_set_range(pbar, 0, 100);
    lv_bar_set_value(pbar, 8, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(pbar, BOOT_DIM,    LV_PART_MAIN);
    lv_obj_set_style_bg_opa(pbar, LV_OPA_COVER,  LV_PART_MAIN);
    lv_obj_set_style_border_width(pbar, 1,        LV_PART_MAIN);
    lv_obj_set_style_border_color(pbar, BOOT_FG,  LV_PART_MAIN);
    lv_obj_set_style_radius(pbar, 0,              LV_PART_MAIN);
    lv_obj_set_style_bg_color(pbar, BOOT_ACCENT,  LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(pbar, LV_OPA_COVER,   LV_PART_INDICATOR);
    lv_obj_set_style_radius(pbar, 0,              LV_PART_INDICATOR);
    g_boot.progress_bar = pbar;

    boot_sep(scr, 194, BOOT_FG);

    /* ── system info (51 znakow × 8px = 408px ✓) ── */
    boot_lbl(scr, 4, 200, 472,
             "MEM: 8MB+PSRAM  CPU: 240MHz XTENSA-LX7  ESP32-S3R8",
             BOOT_FG);
    boot_sep(scr, 214, BOOT_FG);

    /* ── hex dump (52 znaki = 416px ✓) ── */
    boot_lbl(scr, 4, 220, 472,
             "0000: 52 6F 62 43 6F 20 49 6E 64 75 73 74 72 69 65 73 37 12",
             BOOT_FG);
    boot_sep(scr, 234, BOOT_FG);

    /* ── animowany scanner: full-width 480px (60 znakow × 8px), unscii_8 ──
     * fill=16 znaków '=' przesuwa sie po 58-znakowym obszarze;
     * ramki generowane w czasie wykonania, pingpong forward→backward.  */
    static char scan_bufs[32][61];
    static const char *scan_frames[32];
    int n_scan = 0;
    {
        const int fill = 16, span = 42, step = 3;
        for (int pos = 0; pos <= span && n_scan < 32; pos += step) {
            scan_bufs[n_scan][0] = '[';
            memset(scan_bufs[n_scan] + 1, ' ', 58);
            memset(scan_bufs[n_scan] + 1 + pos, '=', fill);
            scan_bufs[n_scan][59] = ']';
            scan_bufs[n_scan][60] = '\0';
            scan_frames[n_scan] = scan_bufs[n_scan];
            n_scan++;
        }
        for (int pos = span - step; pos > 0 && n_scan < 32; pos -= step) {
            scan_bufs[n_scan][0] = '[';
            memset(scan_bufs[n_scan] + 1, ' ', 58);
            memset(scan_bufs[n_scan] + 1 + pos, '=', fill);
            scan_bufs[n_scan][59] = ']';
            scan_bufs[n_scan][60] = '\0';
            scan_frames[n_scan] = scan_bufs[n_scan];
            n_scan++;
        }
    }
    lv_obj_t *tkr = w_ticker_create(scr, 0, 240, 480, 12);
    lv_obj_set_style_text_color(tkr, BOOT_ACCENT, LV_PART_MAIN);
    lv_obj_set_style_text_font(tkr, &lv_font_unscii_8, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(tkr, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(tkr, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(tkr, 0, LV_PART_MAIN);
    w_ticker_start(tkr, scan_frames, n_scan, 120);

    boot_sep(scr, 254, BOOT_FG);

    /* ── caution (48 znakow = 384px ✓) ── */
    boot_lbl(scr, 4, 260, 472,
             "CAUTION: DO NOT POWER OFF DURING INITIALIZATION",
             BOOT_ACCENT);
    boot_sep(scr, 274, BOOT_FG);

    /* ── footer (50 znakow = 400px ✓) ── */
    boot_lbl(scr, 4, 280, 472,
             "(C) 2077 ROBCO INDUSTRIES  //  ALL RIGHTS RESERVED",
             BOOT_FG);

    /* dolna ramka accent; konczy sie na y=296 < 320 ✓ */
    boot_sep(scr, 294, BOOT_ACCENT);

    lv_scr_load(scr);
}

/* Aktualizatory stanu — wywolywane miedzy etapami bootu.
   Wywolywac pod lvgl_port_lock(). */

static void loading_wifi_ok(void)
{
    boot_ok(g_boot.wifi_right);
    lv_label_set_text(g_boot.status_lbl,
        "STATUS: CONNECTED — FETCHING THEME CONFIGURATION...");
    lv_bar_set_value(g_boot.progress_bar, 35, LV_ANIM_OFF);
}

static void loading_theme_ok(void)
{
    boot_ok(g_boot.cfg_right);
    lv_label_set_text(g_boot.status_lbl,
        "STATUS: THEME LOADED - FETCHING LAYOUT DATA...");
    lv_bar_set_value(g_boot.progress_bar, 65, LV_ANIM_OFF);
}

static void loading_layout_ok(void)
{
    boot_ok(g_boot.layout_right);
    lv_label_set_text(g_boot.status_lbl,
        "STATUS: BOOT COMPLETE - LAUNCHING DASHBOARD...");
    lv_bar_set_value(g_boot.progress_bar, 100, LV_ANIM_OFF);
}

/* ------------------------------------------------------------------ */
/* WebSocket data handler                                               */
/* ------------------------------------------------------------------ */

static void ws_data_handler(const char *json)
{
    jparse_ctx_t jctx;
    if (json_parse_start(&jctx, json, (int)strlen(json)) != OS_SUCCESS) return;

    if (jctx.num_tokens < 1 || jctx.tokens[0].type != JSMN_OBJECT) goto done;

    for (int i = 1; i + 1 < jctx.num_tokens; i++) {
        json_tok_t *k = &jctx.tokens[i];
        if (k->parent != 0 || k->type != JSMN_STRING) continue;

        json_tok_t *v = &jctx.tokens[i + 1];
        int klen = k->end - k->start;
        int vlen = v->end - v->start;
        if (klen <= 0 || klen >= 32 || vlen <= 0 || vlen >= 32) continue;

        char name[32], vbuf[32];
        strncpy(name, jctx.js + k->start, klen); name[klen] = '\0';
        strncpy(vbuf, jctx.js + v->start, vlen); vbuf[vlen] = '\0';

        if (strcmp(name, "__reload") == 0) {
            ESP_LOGI(TAG, "reload requested, restarting...");
            esp_restart();
        }
        placeholder_update(name, vbuf);
    }

done:
    json_parse_end(&jctx);
}

/* ------------------------------------------------------------------ */
/* reply namespace handler                                              */
/* ------------------------------------------------------------------ */

static void reply_handler(const char *args)
{
    char msg[256];
    char tmp[128];
    snprintf(tmp, sizeof(tmp), "%s", args);
    char *colon = strchr(tmp, ':');
    if (colon) {
        *colon = '\0';
        snprintf(msg, sizeof(msg), "{\"event\":\"%s\",\"arg\":\"%s\"}", tmp, colon + 1);
    } else {
        snprintf(msg, sizeof(msg), "{\"event\":\"%s\"}", tmp);
    }
    ws_send(msg);
}

/* ------------------------------------------------------------------ */
/* app_main                                                             */
/* ------------------------------------------------------------------ */

void app_main(void)
{
    /* HW */
    display_spi_init();
    esp_lcd_panel_handle_t panel = display_panel_init(lvgl_port_get_disp_drv());
    lvgl_port_init(panel);

    touch_hw_init();
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read;
    lv_indev_drv_register(&indev_drv);

    /* UI init */
    theme_init();
    screen_mgr_init();
    placeholder_init();
    widget_factory_init();
    callback_init();
    callback_register("reply", reply_handler);

    /* Loading screen + render task */
    make_loading_screen();
    xTaskCreate(boot_render_task, "boot_render", 4096, NULL, 5, NULL);

    /* ── WiFi ── */
    ESP_ERROR_CHECK(wifi_connect());
    lvgl_port_lock();
    loading_wifi_ok();
    lvgl_port_unlock();

    /* ── Theme from host ── */
    char *theme_json = http_fetch(CONFIG_HOST_URL "/theme");
    if (theme_json) {
        theme_load_json(theme_json);
        free(theme_json);
    }
    theme_build_styles();

    lvgl_port_lock();
    loading_theme_ok();
    lv_obj_report_style_change(NULL);
    lvgl_port_unlock();

    /* ── Layout (retry until host responds) ── */
    char *layout_json = NULL;
    while (!layout_json) {
        layout_json = http_fetch(CONFIG_HOST_URL "/layout");
        if (!layout_json) {
            ESP_LOGW(TAG, "GET /layout failed, retrying in 3s...");
            vTaskDelay(pdMS_TO_TICKS(3000));
        }
    }
    lvgl_port_lock();
    loading_layout_ok();
    lvgl_port_unlock();

    /* Krotka pauza zeby uzytkownik zobaczyl "BOOT COMPLETE" */
    vTaskDelay(pdMS_TO_TICKS(600));

    /* Zatrzymanie render taska — czekamy az naprawde skonczy biezacy cykl.
       Race condition: dwa rownoczesne lv_timer_handler() korumpuja listy LVGL. */
    g_boot_done = true;
    while (!g_render_task_done) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    /* Zwolnienie pamieci LVGL zajmowanej przez ekran bootowania.
       ticker_delete_cb automatycznie zwalnia ramki i timery kazdego tickera.
       Bez tego lv_mem_alloc() w widget_factory_build() zwraca NULL -> crash. */
    lv_obj_clean(lv_scr_act());

    ESP_ERROR_CHECK(widget_factory_build(layout_json));
    free(layout_json);

    /* Show first screen */
    screen_mgr_show("main");
    ESP_LOGI(TAG, "screen loaded, starting WS...");

    /* WebSocket live data */
    char ws_url[128];
    snprintf(ws_url, sizeof(ws_url), "%s/ws", CONFIG_HOST_URL);
    ws_connect(ws_url, ws_data_handler);

    /* Glowna petla — render task juz nie dziala, wylaczna kontrola LVGL.
       Minimum 10ms delay zapewnia ze IDLE dostaje CPU i resetuje WDT.
       Cap next_ms do 100ms: lv_timer_handler() moze zwrocic UINT32_MAX
       (LV_NO_TIMER_READY) gdy nie ma timerow — vTaskDelay przepelniloby sie. */
    ESP_LOGI(TAG, "main loop start");
    while (1) {
        ws_process_queue();
        uint32_t next_ms = lv_timer_handler();
        if (next_ms > 100) next_ms = 100;
        vTaskDelay(pdMS_TO_TICKS(next_ms > 10 ? next_ms : 10));
    }
}
