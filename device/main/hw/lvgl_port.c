#include "lvgl_port.h"
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static SemaphoreHandle_t g_mutex;

void lvgl_port_lock(void)   { xSemaphoreTakeRecursive(g_mutex, portMAX_DELAY); }
void lvgl_port_unlock(void) { xSemaphoreGiveRecursive(g_mutex); }

#define LCD_W         480
#define LCD_H         320
#define LCD_BUF_LINES  25
#define LV_TICK_MS      5

static lv_disp_drv_t    disp_drv;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t       *buf;

lv_disp_drv_t *lvgl_port_get_disp_drv(void)
{
    return &disp_drv;
}

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *map)
{
    esp_lcd_panel_draw_bitmap(
        (esp_lcd_panel_handle_t)drv->user_data,
        area->x1, area->y1, area->x2 + 1, area->y2 + 1, map);
    /* lv_disp_flush_ready() wola on_flush_done() w display.c po zakonczeniu DMA. */
}

/* LVGL v8 po wywolaniu flush_cb czeka na draw_buf->flushing == 0.
   Bez wait_cb robi busy-loop ktory nie oddaje CPU → IDLE nie dziala → WDT.
   wait_cb z vTaskDelay(1) oddaje CPU na 1 tick i pozwala ISR/DMA skonczyc. */
static void lvgl_wait_cb(lv_disp_drv_t *drv)
{
    (void)drv;
    vTaskDelay(1);
}

/* esp_timer callbacks wywolywane sa z taska (nie ISR) — IRAM_ATTR zbedny. */
static void lvgl_tick_cb(void *arg)
{
    lv_tick_inc(LV_TICK_MS);
}

void lvgl_port_init(esp_lcd_panel_handle_t panel)
{
    lv_init();

    buf = heap_caps_malloc(LCD_W * LCD_BUF_LINES * sizeof(lv_color_t), MALLOC_CAP_DMA);
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, LCD_W * LCD_BUF_LINES);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res   = LCD_W;
    disp_drv.ver_res   = LCD_H;
    disp_drv.flush_cb  = lvgl_flush_cb;
    disp_drv.wait_cb   = lvgl_wait_cb;
    disp_drv.draw_buf  = &draw_buf;
    disp_drv.user_data = panel;
    lv_disp_set_default(lv_disp_drv_register(&disp_drv));

    esp_timer_handle_t timer;
    esp_timer_create_args_t args = { .callback = lvgl_tick_cb, .name = "lv_tick" };
    ESP_ERROR_CHECK(esp_timer_create(&args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, LV_TICK_MS * 1000));

    g_mutex = xSemaphoreCreateRecursiveMutex();
}
