#include "ui/lvgl_adapter.h"
#include "board/board_pins.h"

#if MIMI_BOARD_PROFILE == MIMI_BOARD_WAVESHARE_146B

#include "lvgl.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"

#define LVGL_TASK_STACK     (6 * 1024)
#define LVGL_TASK_PRIO      5
#define LVGL_TICK_MS        5
#define LVGL_DRAW_LINES     20   /* Lines per partial draw buffer */

#define DISP_H_RES          412
#define DISP_V_RES          412

static const char *TAG = "lvgl_adapter";

static SemaphoreHandle_t s_lvgl_mutex = NULL;
static esp_lcd_panel_handle_t s_panel = NULL;
static lv_display_t *s_disp = NULL;

/* ── LVGL flush callback ────────────────────── */
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_draw_bitmap(s_panel, area->x1, area->y1,
                              area->x2 + 1, area->y2 + 1, px_map);
    lv_display_flush_ready(disp);
}

/* ── LVGL tick source ───────────────────────── */
static void lvgl_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(LVGL_TICK_MS);
}

/* ── LVGL timer handler task ────────────────── */
static void lvgl_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "LVGL task started");
    while (1) {
        lvgl_adapter_lock();
        uint32_t next_ms = lv_timer_handler();
        lvgl_adapter_unlock();
        if (next_ms < LVGL_TICK_MS) {
            next_ms = LVGL_TICK_MS;
        }
        if (next_ms > 100) {
            next_ms = 100;  /* Cap to ~10 fps so screen updates aren't missed */
        }
        vTaskDelay(pdMS_TO_TICKS(next_ms));
    }
}

/* ── Public API ─────────────────────────────── */
void lvgl_adapter_init(esp_lcd_panel_handle_t panel)
{
    s_panel = panel;
    s_lvgl_mutex = xSemaphoreCreateRecursiveMutex();
    assert(s_lvgl_mutex);

    lv_init();

    /* Allocate two partial draw buffers in PSRAM */
    size_t buf_size = DISP_H_RES * LVGL_DRAW_LINES * sizeof(lv_color16_t);
    void *buf1 = heap_caps_calloc(1, buf_size, MALLOC_CAP_SPIRAM);
    void *buf2 = heap_caps_calloc(1, buf_size, MALLOC_CAP_SPIRAM);
    assert(buf1 && buf2);

    s_disp = lv_display_create(DISP_H_RES, DISP_V_RES);
    lv_display_set_flush_cb(s_disp, lvgl_flush_cb);
    lv_display_set_buffers(s_disp, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);

    /* Tick timer */
    const esp_timer_create_args_t tick_args = {
        .callback = lvgl_tick_cb,
        .name = "lvgl_tick",
    };
    esp_timer_handle_t tick_timer;
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, LVGL_TICK_MS * 1000));

    /* LVGL handler task on Core 0 */
    xTaskCreatePinnedToCore(lvgl_task, "lvgl", LVGL_TASK_STACK, NULL, LVGL_TASK_PRIO, NULL, 0);

    ESP_LOGI(TAG, "LVGL adapter initialized (%dx%d, %u-byte buffers)", DISP_H_RES, DISP_V_RES, (unsigned)buf_size);
}

void lvgl_adapter_lock(void)
{
    if (s_lvgl_mutex) {
        xSemaphoreTakeRecursive(s_lvgl_mutex, portMAX_DELAY);
    }
}

void lvgl_adapter_unlock(void)
{
    if (s_lvgl_mutex) {
        xSemaphoreGiveRecursive(s_lvgl_mutex);
    }
}

#else /* Not Waveshare — stubs to avoid linker errors */

void lvgl_adapter_init(esp_lcd_panel_handle_t panel) { (void)panel; }
void lvgl_adapter_lock(void) {}
void lvgl_adapter_unlock(void) {}

#endif /* MIMI_BOARD_WAVESHARE_146B */
