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
/* Boot render task                                                    */
/*                                                                     */
/* Wywoluje lv_timer_handler() co ~16ms (ok. 60fps) przez caly czas   */
/* fazy boot. Dzieki temu animacje (ticker, spinner) dzialaja rowniez */
/* gdy glowny task jest zablokowany przez wifi_connect/http_fetch.    */
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
/* Loading screen                                                      */
/* ------------------------------------------------------------------ */

static struct {
    lv_obj_t *wifi_right;
    lv_obj_t *cfg_right;
    lv_obj_t *layout_right;
    lv_obj_t *status_lbl;
    lv_obj_t *progress_bar;
} g_boot;

#define BOOT_FONT (&lv_font_unscii_8)

#define BOOT_BG         lv_color_hex(0x0A0F14)
#define BOOT_PANEL      lv_color_hex(0x10161D)
#define BOOT_LINE       lv_color_hex(0x24303C)
#define BOOT_TEXT       lv_color_hex(0xD8E1EA)
#define BOOT_MUTED      lv_color_hex(0x8A98A8)
#define BOOT_ACCENT     lv_color_hex(0x67B7FF)
#define BOOT_ACCENT_DIM lv_color_hex(0x162332)
#define BOOT_OK         lv_color_hex(0x7CD992)
#define BOOT_OK_DIM     lv_color_hex(0x16241B)
#define BOOT_BAR_BG     lv_color_hex(0x18212B)

static lv_obj_t *boot_box(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);

    lv_obj_set_style_bg_color(obj, BOOT_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(obj, BOOT_LINE, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);

    return obj;
}

static lv_obj_t *boot_hline(lv_obj_t *parent, int x, int y, int w, lv_color_t col)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, 1);
    lv_obj_set_style_bg_color(obj, col, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, 0, LV_PART_MAIN);
    return obj;
}

static lv_obj_t *boot_lbl(lv_obj_t *parent,
                          int x, int y, int w, int h,
                          const char *txt,
                          lv_color_t col,
                          lv_text_align_t align)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_obj_set_pos(lbl, x, y);
    lv_obj_set_size(lbl, w, h);
    lv_label_set_text(lbl, txt);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);

    lv_obj_set_style_text_color(lbl, col, LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, BOOT_FONT, LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl, align, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lbl, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(lbl, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(lbl, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(lbl, 0, LV_PART_MAIN);

    return lbl;
}

static void boot_style_badge(lv_obj_t *obj, lv_color_t fg, lv_color_t bg, lv_color_t border)
{
    lv_obj_set_style_text_color(obj, fg, LV_PART_MAIN);
    lv_obj_set_style_text_font(obj, BOOT_FONT, LV_PART_MAIN);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    lv_obj_set_style_bg_color(obj, bg, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(obj, border, LV_PART_MAIN);

    lv_obj_set_style_radius(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN);
}

static lv_obj_t *boot_badge(lv_obj_t *parent,
                            int x, int y, int w, int h,
                            const char *txt,
                            lv_color_t fg, lv_color_t bg, lv_color_t border)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_obj_set_pos(lbl, x, y);
    lv_obj_set_size(lbl, w, h);
    lv_label_set_text(lbl, txt);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    boot_style_badge(lbl, fg, bg, border);
    return lbl;
}

static lv_obj_t *boot_spin_badge(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *t = w_ticker_create(parent, x, y, w, h);
    boot_style_badge(t, BOOT_ACCENT, BOOT_ACCENT_DIM, BOOT_ACCENT);

    /* stale szerokosci, zeby napis nie "plywal" */
    static const char *frames[] = {
        "LOAD   ",
        "LOAD.  ",
        "LOAD.. ",
        "LOAD..."
    };
    w_ticker_start(t, frames, 4, 180);
    return t;
}

static lv_obj_t *boot_step_row(lv_obj_t *parent, int y, const char *label, bool ready)
{
    boot_lbl(parent, 14, y, 340, 10, label, BOOT_TEXT, LV_TEXT_ALIGN_LEFT);

    if (ready) {
        return boot_badge(parent, 372, y - 1, 76, 12,
                          "READY", BOOT_OK, BOOT_OK_DIM, BOOT_OK);
    }
    return boot_spin_badge(parent, 372, y - 1, 76, 12);
}

static void boot_ok(lv_obj_t *right)
{
    w_ticker_stop(right);
    lv_label_set_text(right, "READY");
    boot_style_badge(right, BOOT_OK, BOOT_OK_DIM, BOOT_OK);
}

static void make_loading_screen(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, BOOT_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    /* ───────────────────────── header ───────────────────────── */

    lv_obj_t *header = boot_box(scr, 8, 8, 464, 30);

    lv_obj_t *accent = lv_obj_create(header);
    lv_obj_remove_style_all(accent);
    lv_obj_set_pos(accent, 8, 7);
    lv_obj_set_size(accent, 4, 14);
    lv_obj_set_style_bg_color(accent, BOOT_ACCENT, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(accent, 0, LV_PART_MAIN);

    boot_lbl(header, 18, 6, 190, 10,
             "STARTUP SEQUENCE",
             BOOT_TEXT, LV_TEXT_ALIGN_LEFT);

    boot_lbl(header, 18, 16, 438, 10,
             "DISPLAY / TOUCH / NET / THEME / LAYOUT",
             BOOT_MUTED, LV_TEXT_ALIGN_RIGHT);

    /* ───────────────────────── stages ───────────────────────── */

    lv_obj_t *stages = boot_box(scr, 8, 46, 464, 192);

    boot_lbl(stages, 14, 8, 180, 10,
             "INITIALIZATION",
             BOOT_MUTED, LV_TEXT_ALIGN_LEFT);

    boot_lbl(stages, 360, 8, 88, 10,
             "STATE",
             BOOT_MUTED, LV_TEXT_ALIGN_RIGHT);

    boot_hline(stages, 14, 22, 434, BOOT_LINE);

    boot_step_row(stages, 34,  "Hardware initialization", true);
    boot_hline(stages, 14, 48, 434, BOOT_LINE);

    boot_step_row(stages, 60,  "Display controller ILI9488", true);
    boot_hline(stages, 14, 74, 434, BOOT_LINE);

    boot_step_row(stages, 86,  "Touch interface XPT2046", true);
    boot_hline(stages, 14, 100, 434, BOOT_LINE);

    g_boot.wifi_right = boot_step_row(stages, 112, "Wireless network IEEE 802.11n", false);
    boot_hline(stages, 14, 126, 434, BOOT_LINE);

    g_boot.cfg_right = boot_step_row(stages, 138, "Theme configuration", false);
    boot_hline(stages, 14, 152, 434, BOOT_LINE);

    g_boot.layout_right = boot_step_row(stages, 164, "Layout data", false);

    /* ───────────────────────── status ───────────────────────── */

    lv_obj_t *status = boot_box(scr, 8, 246, 464, 42);

    boot_lbl(status, 14, 8, 120, 10,
             "CURRENT STATUS",
             BOOT_MUTED, LV_TEXT_ALIGN_LEFT);

    boot_hline(status, 14, 20, 434, BOOT_LINE);

    g_boot.status_lbl = boot_lbl(status, 14, 28, 434, 10,
        "Searching for wireless network...",
        BOOT_TEXT, LV_TEXT_ALIGN_LEFT);

    /* ───────────────────────── progress ───────────────────────── */

    lv_obj_t *pbar = lv_bar_create(scr);
    lv_obj_set_size(pbar, 434, 8);
    lv_obj_set_pos(pbar, 22, 300);
    lv_bar_set_range(pbar, 0, 100);
    lv_bar_set_value(pbar, 8, LV_ANIM_OFF);

    lv_obj_set_style_bg_color(pbar, BOOT_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(pbar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(pbar, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(pbar, BOOT_LINE, LV_PART_MAIN);
    lv_obj_set_style_radius(pbar, 0, LV_PART_MAIN);

    lv_obj_set_style_bg_color(pbar, BOOT_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(pbar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(pbar, 0, LV_PART_INDICATOR);

    g_boot.progress_bar = pbar;

    lv_scr_load(scr);
}

/* Aktualizatory stanu — wywolywane miedzy etapami bootu.
   Wywolywac pod lvgl_port_lock(). */

static void loading_wifi_ok(void)
{
    boot_ok(g_boot.wifi_right);
    lv_label_set_text(g_boot.status_lbl,
        "Connected. Downloading theme...");
    lv_bar_set_value(g_boot.progress_bar, 35, LV_ANIM_OFF);
}

static void loading_theme_ok(void)
{
    boot_ok(g_boot.cfg_right);
    lv_label_set_text(g_boot.status_lbl,
        "Theme loaded. Downloading layout...");
    lv_bar_set_value(g_boot.progress_bar, 65, LV_ANIM_OFF);
}

static void loading_layout_ok(void)
{
    boot_ok(g_boot.layout_right);
    lv_label_set_text(g_boot.status_lbl,
        "Boot complete. Launching dashboard...");
    lv_bar_set_value(g_boot.progress_bar, 100, LV_ANIM_OFF);
}

/* ------------------------------------------------------------------ */
/* WebSocket data handler                                              */
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
/* reply namespace handler                                             */
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
/* app_main                                                            */
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

    static const lv_disp_rot_t rot_map[] = {
        [0] = LV_DISP_ROT_NONE,
        [1] = LV_DISP_ROT_90,
        [2] = LV_DISP_ROT_180,
        [3] = LV_DISP_ROT_270,
    };
    int rot_deg = theme_rotation();
    lv_disp_rot_t rot = rot_map[(rot_deg / 90) % 4];
    if (rot != LV_DISP_ROT_NONE)
        lv_disp_set_rotation(lv_disp_get_default(), rot);

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