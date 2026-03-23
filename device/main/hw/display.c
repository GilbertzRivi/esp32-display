#include "display.h"
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_ili9488.h>
#include <esp_err.h>

#define SPI_CLK      GPIO_NUM_5
#define SPI_MOSI     GPIO_NUM_6
#define SPI_MISO     GPIO_NUM_4
#define LCD_CS       GPIO_NUM_9
#define LCD_RST      GPIO_NUM_8
#define LCD_DC       GPIO_NUM_7
#define LCD_HZ       40000000
#define LCD_W        480
#define LCD_BUF_LINES 25

static esp_lcd_panel_io_handle_t lcd_io;

static bool on_flush_done(esp_lcd_panel_io_handle_t io,
                          esp_lcd_panel_io_event_data_t *edata, void *ctx)
{
    lv_disp_flush_ready((lv_disp_drv_t *)ctx);
    return false;
}

void display_spi_init(void)
{
    spi_bus_config_t bus = {
        .mosi_io_num     = SPI_MOSI,
        .miso_io_num     = SPI_MISO,
        .sclk_io_num     = SPI_CLK,
        .quadwp_io_num   = GPIO_NUM_NC,
        .quadhd_io_num   = GPIO_NUM_NC,
        .data4_io_num    = GPIO_NUM_NC,
        .data5_io_num    = GPIO_NUM_NC,
        .data6_io_num    = GPIO_NUM_NC,
        .data7_io_num    = GPIO_NUM_NC,
        .max_transfer_sz = LCD_W * LCD_BUF_LINES * 3,  /* RGB666: 3 bytes/px = 36000 */
        .flags           = SPICOMMON_BUSFLAG_SCLK | SPICOMMON_BUSFLAG_MOSI | SPICOMMON_BUSFLAG_MISO | SPICOMMON_BUSFLAG_MASTER,
        .intr_flags      = ESP_INTR_FLAG_LOWMED,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
}

esp_lcd_panel_handle_t display_panel_init(lv_disp_drv_t *disp_drv)
{
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num         = LCD_CS,
        .dc_gpio_num         = LCD_DC,
        .spi_mode            = 0,
        .pclk_hz             = LCD_HZ,
        .trans_queue_depth   = 10,
        .on_color_trans_done = on_flush_done,
        .user_ctx            = disp_drv,
        .lcd_cmd_bits        = 8,
        .lcd_param_bits      = 8,
        .flags               = { 0 },
    };

    esp_lcd_panel_dev_config_t lcd_cfg = {
        .reset_gpio_num = LCD_RST,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 18,
        .flags          = { .reset_active_high = 0 },
        .vendor_config  = NULL,
    };

    esp_lcd_panel_handle_t panel;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_cfg, &lcd_io));
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9488(lcd_io, &lcd_cfg, LCD_W * LCD_BUF_LINES, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, false));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, true, false));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel, 0, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
    return panel;
}
