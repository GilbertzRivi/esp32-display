#include "touch.h"
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_touch_xpt2046.h>
#include <esp_err.h>
#include <esp_log.h>

/* -----------------------------------------------------------------------
 * Kalibracja dotyku — wartości po wszystkich transformacjach (swap+mirror).
 *
 * Aby zmierzyć właściwe wartości:
 *   1. Ustaw TOUCH_CALIBRATION_LOG 1, flash.
 *   2. Dotknij kolejno: lewy brzeg, prawy brzeg, górny brzeg, dolny brzeg.
 *   3. Odczytaj minimalne i maksymalne X/Y z logów.
 *   4. Wpisz zmierzone wartości poniżej i ustaw TOUCH_CALIBRATION_LOG 0.
 * ----------------------------------------------------------------------- */
#define TOUCH_CALIBRATION_LOG  0   /* 1 = drukuj surowe wartości do kalibracji */

#define TOUCH_X_MIN   25    /* współrzędna X przy lewej krawędzi  */
#define TOUCH_X_MAX  455    /* współrzędna X przy prawej krawędzi */
#define TOUCH_Y_MIN   20    /* współrzędna Y przy górnej krawędzi */
#define TOUCH_Y_MAX  300    /* współrzędna Y przy dolnej krawędzi */

#define DISP_W  480
#define DISP_H  320

static int touch_remap(int v, int in_min, int in_max, int out_max)
{
    int r = (v - in_min) * out_max / (in_max - in_min);
    return r < 0 ? 0 : (r > out_max ? out_max : r);
}

#define TAG "touch"

#define TOUCH_CS   GPIO_NUM_3
#define TOUCH_INT  GPIO_NUM_2

static esp_lcd_touch_handle_t tp;

void touch_hw_init(void)
{
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_spi_config_t io_cfg = ESP_LCD_TOUCH_IO_SPI_XPT2046_CONFIG(TOUCH_CS);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_cfg, &io));

    gpio_config_t pen_cfg = {
        .pin_bit_mask = BIT64(TOUCH_INT),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&pen_cfg);

    esp_lcd_touch_config_t cfg = {
        .x_max        = 320,
        .y_max        = 480,
        .rst_gpio_num = -1,
        .int_gpio_num = TOUCH_INT,
        .flags        = { .swap_xy = 1, .mirror_x = 1, .mirror_y = 1 },
    };
    ESP_ERROR_CHECK(esp_lcd_touch_new_spi_xpt2046(io, &cfg, &tp));
}

void touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    if (esp_lcd_touch_read_data(tp) != ESP_OK) {
        data->state = LV_INDEV_STATE_REL;
        data->continue_reading = false;
        return;
    }

    uint16_t x[1], y[1], strength[1];
    uint8_t count = 0;
    if (esp_lcd_touch_get_coordinates(tp, x, y, strength, &count, 1) && count > 0) {
#if TOUCH_CALIBRATION_LOG
        ESP_LOGI(TAG, "raw x=%d y=%d", x[0], y[0]);
#endif
        data->point.x = touch_remap(x[0], TOUCH_X_MIN, TOUCH_X_MAX, DISP_W - 1);
        data->point.y = touch_remap(y[0], TOUCH_Y_MIN, TOUCH_Y_MAX, DISP_H - 1);
        data->state   = LV_INDEV_STATE_PR;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
    data->continue_reading = false;
}
