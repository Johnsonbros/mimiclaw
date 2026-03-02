#include "display/display.h"

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "esp_check.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "display/display_panel.h"
#include "board/board_pins.h"

#if MIMI_BOARD_PROFILE == MIMI_BOARD_XIAOZHI_ST7789
#include "display/font5x7.h"
#include "qrcode.h"
#endif

#if MIMI_BOARD_PROFILE == MIMI_BOARD_WAVESHARE_146B
#include "lvgl.h"
#include "ui/lvgl_adapter.h"

#define LOGO_W  360
#define LOGO_H  360
extern const uint8_t _binary_aisync_logo_360x360_rgb565_start[];
extern const uint8_t _binary_aisync_logo_360x360_rgb565_end[];
#endif

#define LEDC_TIMER             LEDC_TIMER_0
#define LEDC_MODE              LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL           LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT
#define LEDC_FREQUENCY_HZ       4000

#define BACKLIGHT_MIN_PERCENT  10
#define BACKLIGHT_MAX_PERCENT  100
#define BACKLIGHT_STEP_PERCENT 10

static const char *TAG = "display";

static esp_lcd_panel_handle_t panel_handle = NULL;
static uint8_t backlight_percent = 50;

/* ──────────────────────────────────────────────
 * Xiaozhi ST7789T: custom framebuffer rendering
 * ────────────────────────────────────────────── */
#if MIMI_BOARD_PROFILE == MIMI_BOARD_XIAOZHI_ST7789

#define BANNER_W  320
#define BANNER_H  172

static uint16_t *framebuffer = NULL;

typedef struct {
    int x;
    int y;
    int box;
    uint16_t fg;
} qr_draw_ctx_t;

static qr_draw_ctx_t s_qr_ctx;

extern const uint8_t _binary_banner_320x172_rgb565_start[];
extern const uint8_t _binary_banner_320x172_rgb565_end[];

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static void fb_ensure(void)
{
    if (!framebuffer) {
        framebuffer = (uint16_t *)calloc(BANNER_W * BANNER_H, sizeof(uint16_t));
    }
}

static inline void fb_set_pixel(int x, int y, uint16_t color)
{
    if (x < 0 || y < 0 || x >= BANNER_W || y >= BANNER_H || !framebuffer) {
        return;
    }
    framebuffer[y * BANNER_W + x] = color;
}

static void fb_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    if (!framebuffer) {
        return;
    }
    for (int yy = y; yy < y + h; yy++) {
        for (int xx = x; xx < x + w; xx++) {
            fb_set_pixel(xx, yy, color);
        }
    }
}

static void fb_fill_rect_clipped(int x, int y, int w, int h, uint16_t color, int clip_x0, int clip_x1)
{
    if (!framebuffer) {
        return;
    }
    int x0 = x;
    int x1 = x + w;
    if (x0 < clip_x0) {
        x0 = clip_x0;
    }
    if (x1 > clip_x1) {
        x1 = clip_x1;
    }
    if (x1 <= x0) {
        return;
    }
    for (int yy = y; yy < y + h; yy++) {
        for (int xx = x0; xx < x1; xx++) {
            fb_set_pixel(xx, yy, color);
        }
    }
}

static void fb_draw_char_scaled_clipped(int x, int y, char c, uint16_t color, int scale, int clip_x0, int clip_x1)
{
    if (c < 32 || c > 127) {
        c = '?';
    }
    const uint8_t *glyph = font5x7[(uint8_t)c - 32];
    for (int col = 0; col < FONT5X7_WIDTH; col++) {
        uint8_t bits = glyph[col];
        for (int row = 0; row < FONT5X7_HEIGHT; row++) {
            if (bits & (1 << row)) {
                int px = x + col * scale;
                int py = y + row * scale;
                fb_fill_rect_clipped(px, py, scale, scale, color, clip_x0, clip_x1);
            }
        }
    }
}

static void fb_draw_text_clipped(int x, int y, const char *text, uint16_t color, int line_height, int scale,
                                 int clip_x0, int clip_x1)
{
    int cx = x;
    int cy = y;
    for (size_t i = 0; text[i] != '\0'; i++) {
        if (text[i] == '\n') {
            cy += line_height;
            cx = x;
            continue;
        }
        fb_draw_char_scaled_clipped(cx, cy, text[i], color, scale, clip_x0, clip_x1);
        cx += (FONT5X7_WIDTH + 1) * scale;
    }
}

#endif /* MIMI_BOARD_XIAOZHI_ST7789 */

/* ──────────────────────────────────────────────
 * Waveshare 1.46B: LVGL screen objects
 * ────────────────────────────────────────────── */
#if MIMI_BOARD_PROFILE == MIMI_BOARD_WAVESHARE_146B

static lv_obj_t *s_scr_banner  = NULL;
static lv_obj_t *s_scr_config  = NULL;
static lv_obj_t *s_scr_stdui   = NULL;

/* Config screen LVGL widgets */
static lv_obj_t *s_cfg_ip_label  = NULL;
static lv_obj_t *s_cfg_title     = NULL;
static lv_obj_t *s_cfg_line_labels[12];
static size_t    s_cfg_line_label_count = 0;

/* Standard UI LVGL widgets */
static lv_obj_t *s_stdui_title    = NULL;
static lv_obj_t *s_stdui_subtitle = NULL;
static lv_obj_t *s_stdui_chip     = NULL;
static lv_obj_t *s_stdui_footer   = NULL;
static lv_obj_t *s_stdui_line_labels[12];
static size_t    s_stdui_line_label_count = 0;

#endif /* MIMI_BOARD_WAVESHARE_146B */

/* ──────────────────────────────────────────────
 * Backlight (shared: both profiles use LEDC PWM)
 * ────────────────────────────────────────────── */
static void backlight_ledc_init(void)
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = LEDC_FREQUENCY_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .timer_sel = LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = BOARD_LCD_BACKLIGHT,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

void display_set_backlight_percent(uint8_t percent)
{
    if (percent > BACKLIGHT_MAX_PERCENT) {
        percent = BACKLIGHT_MAX_PERCENT;
    }
    backlight_percent = percent;

    uint32_t duty_max = (1U << LEDC_DUTY_RES) - 1;
    uint32_t duty = (duty_max * backlight_percent) / 100;
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
}

uint8_t display_get_backlight_percent(void)
{
    return backlight_percent;
}

void display_cycle_backlight(void)
{
    uint8_t next = backlight_percent + BACKLIGHT_STEP_PERCENT;
    if (next > BACKLIGHT_MAX_PERCENT) {
        next = BACKLIGHT_MIN_PERCENT;
    }
    display_set_backlight_percent(next);
    ESP_LOGI(TAG, "Backlight -> %u%%", next);
}

/* ──────────────────────────────────────────────
 * display_init / display_get_panel
 * ────────────────────────────────────────────── */
esp_err_t display_init(void)
{
    ESP_RETURN_ON_ERROR(display_panel_create(&panel_handle), TAG, "panel create failed");

    backlight_ledc_init();
    display_set_backlight_percent(backlight_percent);

    return ESP_OK;
}

esp_lcd_panel_handle_t display_get_panel(void)
{
    return panel_handle;
}

/* ──────────────────────────────────────────────
 * display_show_banner
 * ────────────────────────────────────────────── */
void display_show_banner(void)
{
    if (!panel_handle) {
        ESP_LOGW(TAG, "display not initialized");
        return;
    }

#if MIMI_BOARD_PROFILE == MIMI_BOARD_XIAOZHI_ST7789
    const uint8_t *start = _binary_banner_320x172_rgb565_start;
    const uint8_t *end = _binary_banner_320x172_rgb565_end;
    size_t len = (size_t)(end - start);
    size_t expected = (size_t)BANNER_W * (size_t)BANNER_H * 2;
    if (len < expected) {
        ESP_LOGW(TAG, "banner data too small (%u < %u)", (unsigned)len, (unsigned)expected);
        return;
    }
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, BANNER_W, BANNER_H, start));

#elif MIMI_BOARD_PROFILE == MIMI_BOARD_WAVESHARE_146B
    lvgl_adapter_lock();
    if (!s_scr_banner) {
        s_scr_banner = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(s_scr_banner, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(s_scr_banner, LV_OPA_COVER, 0);

        /* AiSync gear logo — centered, fills the round display */
        static lv_image_dsc_t logo_dsc = {
            .header = {
                .cf = LV_COLOR_FORMAT_RGB565,
                .w  = LOGO_W,
                .h  = LOGO_H,
            },
            .data_size = LOGO_W * LOGO_H * 2,
            .data = NULL,
        };
        logo_dsc.data = _binary_aisync_logo_360x360_rgb565_start;

        lv_obj_t *logo = lv_image_create(s_scr_banner);
        lv_image_set_src(logo, &logo_dsc);
        lv_obj_center(logo);
    }
    lv_scr_load(s_scr_banner);
    lv_obj_invalidate(s_scr_banner);
    lvgl_adapter_unlock();
#endif
}

/* ──────────────────────────────────────────────
 * display_show_config_screen
 * ────────────────────────────────────────────── */
#if MIMI_BOARD_PROFILE == MIMI_BOARD_XIAOZHI_ST7789

static void qr_draw_cb(esp_qrcode_handle_t qrcode)
{
    int size = esp_qrcode_get_size(qrcode);
    int quiet = 2;
    int scale = s_qr_ctx.box / (size + quiet * 2);
    if (scale < 1) {
        scale = 1;
    }
    int qr_px = (size + quiet * 2) * scale;
    int origin_x = s_qr_ctx.x + (s_qr_ctx.box - qr_px) / 2 + quiet * scale;
    int origin_y = s_qr_ctx.y + (s_qr_ctx.box - qr_px) / 2 + quiet * scale;

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            if (esp_qrcode_get_module(qrcode, x, y)) {
                fb_fill_rect(origin_x + x * scale, origin_y + y * scale, scale, scale, s_qr_ctx.fg);
            }
        }
    }
}

#endif /* MIMI_BOARD_XIAOZHI_ST7789 */

void display_show_config_screen(const char *qr_text, const char *ip_text,
                                const char **lines, size_t line_count, size_t scroll,
                                size_t selected, int selected_offset_px)
{
    if (!panel_handle) {
        ESP_LOGW(TAG, "display not initialized");
        return;
    }
    if (!qr_text || !ip_text || !lines) {
        return;
    }

#if MIMI_BOARD_PROFILE == MIMI_BOARD_XIAOZHI_ST7789
    fb_ensure();
    if (!framebuffer) {
        ESP_LOGW(TAG, "framebuffer alloc failed");
        return;
    }

    const uint16_t color_bg = rgb565(0, 0, 0);
    const uint16_t color_fg = rgb565(255, 255, 255);
    const uint16_t color_qr_bg = rgb565(255, 255, 255);
    const uint16_t color_qr_fg = rgb565(0, 0, 0);
    const uint16_t color_title = rgb565(100, 200, 255);
    const uint16_t color_sel_bg = rgb565(50, 80, 120);

    fb_fill_rect(0, 0, BANNER_W, BANNER_H, color_bg);

    const int left_pad = 6;
    const int qr_box = 110;
    const int qr_x = left_pad;
    const int qr_y = (BANNER_H - qr_box) / 2 - 8;

    fb_fill_rect(qr_x, qr_y, qr_box, qr_box, color_qr_bg);

    s_qr_ctx.x = qr_x;
    s_qr_ctx.y = qr_y;
    s_qr_ctx.box = qr_box;
    s_qr_ctx.fg = color_qr_fg;

    esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
    cfg.display_func = qr_draw_cb;
    cfg.max_qrcode_version = 6;
    cfg.qrcode_ecc_level = ESP_QRCODE_ECC_MED;
    esp_qrcode_generate(&cfg, qr_text);

    fb_draw_text_clipped(qr_x, qr_y + qr_box + 4, ip_text, color_fg, 10, 1, 0, BANNER_W);

    const int right_x = qr_x + qr_box + 10;
    const int right_w = BANNER_W - right_x - 6;
    (void)right_w;
    fb_draw_text_clipped(right_x, 4, "Configuration", color_title, 14, 2, right_x, BANNER_W);

    const int line_height = 16;
    const int start_y = 24;
    size_t lines_per_page = (BANNER_H - start_y - 6) / line_height;
    for (size_t i = 0; i < lines_per_page; i++) {
        if (line_count == 0) {
            break;
        }
        size_t idx = (scroll + i) % line_count;
        if (idx < line_count) {
            int line_y = start_y + (int)i * line_height;
            if (idx == selected) {
                fb_fill_rect(right_x, line_y - 1, BANNER_W - right_x - 2, line_height + 2, color_sel_bg);
                fb_draw_text_clipped(right_x - selected_offset_px, line_y, lines[idx], color_fg, line_height, 2, right_x, BANNER_W);
            } else {
                fb_fill_rect(right_x, line_y - 1, BANNER_W - right_x - 2, line_height + 2, color_bg);
                fb_draw_text_clipped(right_x, line_y, lines[idx], color_fg, line_height, 2, right_x, BANNER_W);
            }
        }
    }

    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, BANNER_W, BANNER_H, framebuffer));

#elif MIMI_BOARD_PROFILE == MIMI_BOARD_WAVESHARE_146B
    lvgl_adapter_lock();

    if (!s_scr_config) {
        s_scr_config = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(s_scr_config, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(s_scr_config, LV_OPA_COVER, 0);
        lv_obj_set_flex_flow(s_scr_config, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(s_scr_config, 20, 0);
        lv_obj_set_style_pad_row(s_scr_config, 4, 0);

        /* Title */
        s_cfg_title = lv_label_create(s_scr_config);
        lv_obj_set_style_text_color(s_cfg_title, lv_color_hex(0x64C8FF), 0);
        lv_obj_set_style_text_font(s_cfg_title, &lv_font_montserrat_20, 0);
        lv_label_set_text(s_cfg_title, "Configuration");

        /* IP label */
        s_cfg_ip_label = lv_label_create(s_scr_config);
        lv_obj_set_style_text_color(s_cfg_ip_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(s_cfg_ip_label, &lv_font_montserrat_14, 0);

        /* Config lines */
        s_cfg_line_label_count = 0;
    }

    lv_label_set_text(s_cfg_ip_label, ip_text);

    /* Remove old line labels and recreate */
    for (size_t i = 0; i < s_cfg_line_label_count; i++) {
        lv_obj_del(s_cfg_line_labels[i]);
    }
    s_cfg_line_label_count = 0;

    size_t max_lines = line_count < 12 ? line_count : 12;
    for (size_t i = 0; i < max_lines; i++) {
        size_t idx = (scroll + i) % line_count;
        lv_obj_t *lbl = lv_label_create(s_scr_config);
        lv_label_set_text(lbl, lines[idx]);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_width(lbl, DISPLAY_WIDTH - 50);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);

        if (idx == selected) {
            lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_bg_color(lbl, lv_color_hex(0x325078), 0);
            lv_obj_set_style_bg_opa(lbl, LV_OPA_COVER, 0);
            lv_obj_set_style_pad_all(lbl, 2, 0);
        } else {
            lv_obj_set_style_text_color(lbl, lv_color_hex(0xC0C0C0), 0);
        }

        s_cfg_line_labels[s_cfg_line_label_count++] = lbl;
    }

    lv_scr_load(s_scr_config);
    lvgl_adapter_unlock();
#endif
}

/* ──────────────────────────────────────────────
 * display_show_standard_ui_screen
 * ────────────────────────────────────────────── */
void display_show_standard_ui_screen(const char *title, const char *subtitle,
                                     const char *status_chip, const char **lines,
                                     size_t line_count, const char *footer_hint)
{
    if (!panel_handle) {
        ESP_LOGW(TAG, "display not initialized");
        return;
    }
    if (!title || !subtitle || !status_chip || !lines || !footer_hint) {
        return;
    }

#if MIMI_BOARD_PROFILE == MIMI_BOARD_XIAOZHI_ST7789
    fb_ensure();
    if (!framebuffer) {
        ESP_LOGW(TAG, "framebuffer alloc failed");
        return;
    }

    const uint16_t color_bg = rgb565(8, 12, 20);
    const uint16_t color_header = rgb565(23, 42, 74);
    const uint16_t color_card = rgb565(16, 24, 38);
    const uint16_t color_fg = rgb565(230, 237, 246);
    const uint16_t color_muted = rgb565(135, 154, 180);
    const uint16_t color_chip = rgb565(68, 189, 120);
    const uint16_t color_footer = rgb565(37, 51, 74);

    fb_fill_rect(0, 0, BANNER_W, BANNER_H, color_bg);
    fb_fill_rect(0, 0, BANNER_W, 26, color_header);
    fb_fill_rect(8, 32, BANNER_W - 16, BANNER_H - 52, color_card);
    fb_fill_rect(BANNER_W - 86, 4, 78, 16, color_chip);
    fb_fill_rect(0, BANNER_H - 18, BANNER_W, 18, color_footer);

    fb_draw_text_clipped(6, 6, title, color_fg, 12, 2, 0, BANNER_W);
    fb_draw_text_clipped(10, 38, subtitle, color_muted, 10, 1, 10, BANNER_W - 10);
    fb_draw_text_clipped(BANNER_W - 82, 8, status_chip, color_bg, 10, 1, BANNER_W - 84, BANNER_W - 6);

    const int start_y = 54;
    const int line_h = 14;
    for (size_t i = 0; i < line_count; i++) {
        fb_draw_text_clipped(12, start_y + ((int)i * line_h), lines[i], color_fg, line_h, 1, 12, BANNER_W - 12);
    }

    fb_draw_text_clipped(8, BANNER_H - 14, footer_hint, color_muted, 10, 1, 8, BANNER_W - 8);
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, BANNER_W, BANNER_H, framebuffer));

#elif MIMI_BOARD_PROFILE == MIMI_BOARD_WAVESHARE_146B
    lvgl_adapter_lock();

    if (!s_scr_stdui) {
        s_scr_stdui = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(s_scr_stdui, lv_color_hex(0x080C14), 0);
        lv_obj_set_style_bg_opa(s_scr_stdui, LV_OPA_COVER, 0);

        /* Header bar */
        lv_obj_t *header = lv_obj_create(s_scr_stdui);
        lv_obj_set_size(header, DISPLAY_WIDTH, 50);
        lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_color(header, lv_color_hex(0x172A4A), 0);
        lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(header, 0, 0);
        lv_obj_set_style_radius(header, 0, 0);
        lv_obj_set_style_pad_all(header, 8, 0);

        s_stdui_title = lv_label_create(header);
        lv_obj_set_style_text_color(s_stdui_title, lv_color_hex(0xE6EDF6), 0);
        lv_obj_set_style_text_font(s_stdui_title, &lv_font_montserrat_16, 0);
        lv_obj_align(s_stdui_title, LV_ALIGN_LEFT_MID, 0, 0);

        s_stdui_chip = lv_label_create(header);
        lv_obj_set_style_text_color(s_stdui_chip, lv_color_hex(0x080C14), 0);
        lv_obj_set_style_text_font(s_stdui_chip, &lv_font_montserrat_12, 0);
        lv_obj_set_style_bg_color(s_stdui_chip, lv_color_hex(0x44BD78), 0);
        lv_obj_set_style_bg_opa(s_stdui_chip, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(s_stdui_chip, 4, 0);
        lv_obj_set_style_radius(s_stdui_chip, 4, 0);
        lv_obj_align(s_stdui_chip, LV_ALIGN_RIGHT_MID, 0, 0);

        /* Card body */
        lv_obj_t *card = lv_obj_create(s_scr_stdui);
        lv_obj_set_size(card, DISPLAY_WIDTH - 30, DISPLAY_HEIGHT - 120);
        lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x101826), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(card, 0, 0);
        lv_obj_set_style_radius(card, 8, 0);
        lv_obj_set_style_pad_all(card, 12, 0);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(card, 6, 0);

        s_stdui_subtitle = lv_label_create(card);
        lv_obj_set_style_text_color(s_stdui_subtitle, lv_color_hex(0x879AB4), 0);
        lv_obj_set_style_text_font(s_stdui_subtitle, &lv_font_montserrat_12, 0);

        /* Pre-create line labels */
        s_stdui_line_label_count = 0;
        for (int i = 0; i < 12; i++) {
            lv_obj_t *lbl = lv_label_create(card);
            lv_obj_set_style_text_color(lbl, lv_color_hex(0xE6EDF6), 0);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
            lv_obj_add_flag(lbl, LV_OBJ_FLAG_HIDDEN);
            s_stdui_line_labels[i] = lbl;
        }

        /* Footer */
        s_stdui_footer = lv_label_create(s_scr_stdui);
        lv_obj_set_style_text_color(s_stdui_footer, lv_color_hex(0x879AB4), 0);
        lv_obj_set_style_text_font(s_stdui_footer, &lv_font_montserrat_12, 0);
        lv_obj_align(s_stdui_footer, LV_ALIGN_BOTTOM_MID, 0, -10);
    }

    /* Update content */
    lv_label_set_text(s_stdui_title, title);
    lv_label_set_text(s_stdui_subtitle, subtitle);
    lv_label_set_text(s_stdui_chip, status_chip);
    lv_label_set_text(s_stdui_footer, footer_hint);

    size_t max_lines = line_count < 12 ? line_count : 12;
    for (size_t i = 0; i < 12; i++) {
        if (i < max_lines) {
            lv_label_set_text(s_stdui_line_labels[i], lines[i]);
            lv_obj_clear_flag(s_stdui_line_labels[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_stdui_line_labels[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    lv_scr_load(s_scr_stdui);
    lvgl_adapter_unlock();
#endif
}

/* ──────────────────────────────────────────────
 * display_get_banner_center_rgb
 * ────────────────────────────────────────────── */
bool display_get_banner_center_rgb(uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (!r || !g || !b) {
        return false;
    }

#if MIMI_BOARD_PROFILE == MIMI_BOARD_XIAOZHI_ST7789
    const uint8_t *start = _binary_banner_320x172_rgb565_start;
    const uint8_t *end = _binary_banner_320x172_rgb565_end;
    size_t len = (size_t)(end - start);
    size_t expected = (size_t)BANNER_W * (size_t)BANNER_H * 2;
    if (len < expected) {
        return false;
    }

    size_t cx = BANNER_W / 2;
    size_t cy = BANNER_H / 2;
    size_t idx = (cy * BANNER_W + cx) * 2;
    uint16_t pixel = (uint16_t)start[idx] | ((uint16_t)start[idx + 1] << 8);

    uint8_t r5 = (pixel >> 11) & 0x1F;
    uint8_t g6 = (pixel >> 5) & 0x3F;
    uint8_t b5 = pixel & 0x1F;

    *r = (uint8_t)((r5 * 255) / 31);
    *g = (uint8_t)((g6 * 255) / 63);
    *b = (uint8_t)((b5 * 255) / 31);
    return true;

#elif MIMI_BOARD_PROFILE == MIMI_BOARD_WAVESHARE_146B
    /* No embedded banner for round display; return a default color */
    *r = 8;
    *g = 20;
    *b = 32;
    return true;
#else
    return false;
#endif
}
