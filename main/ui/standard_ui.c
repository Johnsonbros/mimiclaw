#include "ui/standard_ui.h"

#include <stdio.h>
#include <string.h>

#include "display/display.h"
#include "esp_heap_caps.h"
#include "mimi_config.h"
#include "mimi_secrets.h"
#include "nvs.h"
#include "wifi/wifi_manager.h"

#define UI_LINE_MAX 64
#define UI_LINES_PER_SCREEN 7
#define UI_SCREEN_COUNT 3

static bool s_active = false;
static size_t s_screen = 0;
static char s_lines[UI_LINES_PER_SCREEN][UI_LINE_MAX];
static const char *s_line_ptrs[UI_LINES_PER_SCREEN];

static const char *read_nvs_or_default(const char *ns, const char *key, const char *fallback,
                                       char *buf, size_t buf_len)
{
    nvs_handle_t nvs;
    if (nvs_open(ns, NVS_READONLY, &nvs) == ESP_OK) {
        size_t len = buf_len;
        if (nvs_get_str(nvs, key, buf, &len) == ESP_OK && buf[0] != '\0') {
            nvs_close(nvs);
            return buf;
        }
        nvs_close(nvs);
    }
    return fallback;
}

static void render_boot_status(void)
{
    const char *ip = wifi_manager_get_ip();
    if (!ip || ip[0] == '\0') {
        ip = "0.0.0.0";
    }

    snprintf(s_lines[0], UI_LINE_MAX, "WiFi: %s", wifi_manager_is_connected() ? "Connected" : "Connecting");
    snprintf(s_lines[1], UI_LINE_MAX, "IP: %s", ip);
    snprintf(s_lines[2], UI_LINE_MAX, "Heap: %d KB", (int)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));
    snprintf(s_lines[3], UI_LINE_MAX, "PSRAM: %d KB", (int)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
    snprintf(s_lines[4], UI_LINE_MAX, "Board: ESP32-S3");
    snprintf(s_lines[5], UI_LINE_MAX, "Display: %dx%d", DISPLAY_WIDTH, DISPLAY_HEIGHT);
    snprintf(s_lines[6], UI_LINE_MAX, "Power: USB");

    display_show_standard_ui_screen("DOT", "Boot & system status",
                                    "READY", s_line_ptrs, UI_LINES_PER_SCREEN,
                                    "single: brightness  double: next screen");
}

static void render_setup_status(void)
{
    char provider[24] = {0};
    char model[32] = {0};
    char proxy_host[24] = {0};

    const char *provider_v = read_nvs_or_default(MIMI_NVS_LLM, MIMI_NVS_KEY_PROVIDER,
                                                 MIMI_SECRET_MODEL_PROVIDER, provider, sizeof(provider));
    const char *model_v = read_nvs_or_default(MIMI_NVS_LLM, MIMI_NVS_KEY_MODEL,
                                              MIMI_SECRET_MODEL, model, sizeof(model));
    const char *proxy_v = read_nvs_or_default(MIMI_NVS_PROXY, MIMI_NVS_KEY_PROXY_HOST,
                                              "(none)", proxy_host, sizeof(proxy_host));

    snprintf(s_lines[0], UI_LINE_MAX, "Provider: %s", provider_v);
    snprintf(s_lines[1], UI_LINE_MAX, "Model: %s", model_v);
    snprintf(s_lines[2], UI_LINE_MAX, "Proxy Host: %s", proxy_v);
    snprintf(s_lines[3], UI_LINE_MAX, "WS Port: 18789");
    snprintf(s_lines[4], UI_LINE_MAX, "Setup: QR + Config screen");
    snprintf(s_lines[5], UI_LINE_MAX, "CLI: config_show");
    snprintf(s_lines[6], UI_LINE_MAX, "Persist: NVS + SPIFFS");

    display_show_standard_ui_screen("DOT", "Onboarding and setup",
                                    "SETUP", s_line_ptrs, UI_LINES_PER_SCREEN,
                                    "shake: config  single: brightness");
}

static void render_assistant_status(void)
{
    snprintf(s_lines[0], UI_LINE_MAX, "Assistant: Idle/awaiting input");
    snprintf(s_lines[1], UI_LINE_MAX, "Channel: Telegram + WebSocket");
    snprintf(s_lines[2], UI_LINE_MAX, "Tools: web_search, cron, files");
    snprintf(s_lines[3], UI_LINE_MAX, "Memory: MEMORY.md + sessions");
    snprintf(s_lines[4], UI_LINE_MAX, "Heartbeat: periodic task scan");
    snprintf(s_lines[5], UI_LINE_MAX, "Cron: autonomous scheduled tasks");
    snprintf(s_lines[6], UI_LINE_MAX, "Privacy: local flash storage");

    display_show_standard_ui_screen("DOT", "Assistant activity overview",
                                    "AGENT", s_line_ptrs, UI_LINES_PER_SCREEN,
                                    "double: cycle views  shake: config");
}

void standard_ui_refresh(void)
{
    if (!s_active) {
        return;
    }

    switch (s_screen) {
    case 0:
        render_boot_status();
        break;
    case 1:
        render_setup_status();
        break;
    default:
        render_assistant_status();
        break;
    }
}

void standard_ui_init(void)
{
    for (size_t i = 0; i < UI_LINES_PER_SCREEN; i++) {
        s_line_ptrs[i] = s_lines[i];
    }
}

void standard_ui_toggle(void)
{
    if (s_active) {
        s_active = false;
        display_show_banner();
        return;
    }

    s_active = true;
    s_screen = 0;
    standard_ui_refresh();
}

bool standard_ui_is_active(void)
{
    return s_active;
}

void standard_ui_next_screen(void)
{
    if (!s_active) {
        return;
    }

    s_screen = (s_screen + 1) % UI_SCREEN_COUNT;
    standard_ui_refresh();
}
