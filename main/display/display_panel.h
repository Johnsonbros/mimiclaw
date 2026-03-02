#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t display_panel_create(esp_lcd_panel_handle_t *out);

#ifdef __cplusplus
}
#endif
