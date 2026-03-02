#pragma once

#include "esp_lcd_panel_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

void lvgl_adapter_init(esp_lcd_panel_handle_t panel);
void lvgl_adapter_lock(void);
void lvgl_adapter_unlock(void);

#ifdef __cplusplus
}
#endif
