#include "display/display_panel.h"
#include "board/board_pins.h"

#if MIMI_BOARD_PROFILE == MIMI_BOARD_WAVESHARE_146B

#include "esp_check.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_spd2010.h"
#include "io_expander/tca9554.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define QSPI_HOST       SPI2_HOST
#define LCD_H_RES       412
#define LCD_V_RES       412

static const char *TAG = "panel_spd2010";

esp_err_t display_panel_create(esp_lcd_panel_handle_t *out)
{
    /* --- TCA9554 IO expander: reset LCD via EXIO2 --- */
    ESP_RETURN_ON_ERROR(tca9554_init(BOARD_EXIO_I2C_ADDR), TAG, "tca9554 init failed");

    /* LCD reset: EXIO2 low → 10ms → high → 50ms */
    ESP_RETURN_ON_ERROR(tca9554_set_pin(BOARD_EXIO_I2C_ADDR, BOARD_LCD_RST_EXIO, 0), TAG, "lcd rst low");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(tca9554_set_pin(BOARD_EXIO_I2C_ADDR, BOARD_LCD_RST_EXIO, 1), TAG, "lcd rst high");
    vTaskDelay(pdMS_TO_TICKS(50));

    /* --- QSPI bus --- */
    size_t max_transfer = LCD_H_RES * 20 * sizeof(uint16_t); /* ~20 lines per DMA transfer */
    spi_bus_config_t buscfg = SPD2010_PANEL_BUS_QSPI_CONFIG(
        BOARD_LCD_QSPI_SCK,
        BOARD_LCD_QSPI_D0,
        BOARD_LCD_QSPI_D1,
        BOARD_LCD_QSPI_D2,
        BOARD_LCD_QSPI_D3,
        max_transfer
    );
    ESP_RETURN_ON_ERROR(spi_bus_initialize(QSPI_HOST, &buscfg, SPI_DMA_CH_AUTO), TAG, "qspi bus init failed");

    /* --- Panel IO --- */
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = SPD2010_PANEL_IO_QSPI_CONFIG(
        BOARD_LCD_QSPI_CS,
        NULL, NULL
    );
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)QSPI_HOST, &io_config, &io_handle), TAG, "panel io init failed");

    /* --- Panel device --- */
    spd2010_vendor_config_t vendor_cfg = {
        .init_cmds = NULL,
        .init_cmds_size = 0,
        .flags = {
            .use_qspi_interface = 1,
        },
    };

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1,  /* Reset handled by TCA9554 above */
        .rgb_endian = LCD_RGB_ENDIAN_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_cfg,
    };

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_spd2010(io_handle, &panel_config, out), TAG, "panel create failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(*out), TAG, "panel reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(*out), TAG, "panel init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(*out, true), TAG, "panel on failed");

    ESP_LOGI(TAG, "SPD2010 QSPI panel initialized (%dx%d)", LCD_H_RES, LCD_V_RES);
    return ESP_OK;
}

#endif /* MIMI_BOARD_WAVESHARE_146B */
