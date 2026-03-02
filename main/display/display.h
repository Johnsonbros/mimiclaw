#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "board/board_pins.h"

#ifdef __cplusplus
extern "C" {
#endif

#if MIMI_BOARD_PROFILE == MIMI_BOARD_WAVESHARE_146B
#define DISPLAY_WIDTH 412
#define DISPLAY_HEIGHT 412
#else
#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 172
#endif

esp_err_t display_init(void);
esp_lcd_panel_handle_t display_get_panel(void);
void display_show_banner(void);
void display_set_backlight_percent(uint8_t percent);
uint8_t display_get_backlight_percent(void);
void display_cycle_backlight(void);
bool display_get_banner_center_rgb(uint8_t *r, uint8_t *g, uint8_t *b);
void display_show_config_screen(const char *qr_text, const char *ip_text,
                                const char **lines, size_t line_count, size_t scroll,
                                size_t selected, int selected_offset_px);
void display_show_standard_ui_screen(const char *title, const char *subtitle,
                                     const char *status_chip, const char **lines,
                                     size_t line_count, const char *footer_hint);

#ifdef __cplusplus
}
#endif
