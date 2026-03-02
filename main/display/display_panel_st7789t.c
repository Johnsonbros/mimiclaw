#include "display/display_panel.h"
#include "board/board_pins.h"

#if MIMI_BOARD_PROFILE == MIMI_BOARD_XIAOZHI_ST7789

#include "esp_check.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "display/Vernon_ST7789T/Vernon_ST7789T.h"

#define LCD_HOST            SPI3_HOST
#define LCD_PIXEL_CLOCK_HZ  (12 * 1000 * 1000)
#define LCD_CMD_BITS        8
#define LCD_PARAM_BITS      8
#define LCD_H_RES           172
#define LCD_V_RES           320
#define LCD_X_GAP           34
#define LCD_Y_GAP           0

static const char *TAG = "panel_st7789t";

esp_err_t display_panel_create(esp_lcd_panel_handle_t *out)
{
    spi_bus_config_t buscfg = {
        .sclk_io_num = BOARD_LCD_SPI_SCLK,
        .mosi_io_num = BOARD_LCD_SPI_MOSI,
        .miso_io_num = BOARD_LCD_SPI_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LCD_V_RES * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO), TAG, "spi bus init failed");

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BOARD_LCD_SPI_DC,
        .cs_gpio_num = BOARD_LCD_SPI_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .on_color_trans_done = NULL,
        .user_ctx = NULL,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle), TAG, "panel io init failed");

    esp_lcd_panel_dev_st7789t_config_t panel_config = {
        .reset_gpio_num = BOARD_LCD_SPI_RST,
        .rgb_endian = LCD_RGB_ENDIAN_BGR,
        .bits_per_pixel = 16,
    };

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789t(io_handle, &panel_config, out), TAG, "panel init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(*out), TAG, "panel reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(*out), TAG, "panel init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(*out, true, true), TAG, "panel mirror failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(*out, true), TAG, "panel swap failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_set_gap(*out, LCD_Y_GAP, LCD_X_GAP), TAG, "panel gap failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(*out, true), TAG, "panel on failed");

    return ESP_OK;
}

#endif /* MIMI_BOARD_XIAOZHI_ST7789 */
