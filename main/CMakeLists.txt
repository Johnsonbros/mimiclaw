# Board profile: 0 = Xiaozhi ST7789T, 1 = Waveshare ESP32-S3-Touch-LCD-1.46B
if(NOT DEFINED MIMI_BOARD_PROFILE)
    set(MIMI_BOARD_PROFILE 0)
endif()

# Common sources (all profiles)
set(COMMON_SRCS
    "mimi.c"
    "display/display.c"
    "rgb/rgb.c"
    "buttons/multi_button.c"
    "buttons/button_driver.c"
    "imu/I2C_Driver.c"
    "imu/QMI8658.c"
    "imu/imu_manager.c"
    "ui/config_screen.c"
    "ui/standard_ui.c"
    "bus/message_bus.c"
    "wifi/wifi_manager.c"
    "telegram/telegram_bot.c"
    "llm/llm_proxy.c"
    "agent/agent_loop.c"
    "agent/context_builder.c"
    "memory/memory_store.c"
    "memory/session_mgr.c"
    "gateway/ws_server.c"
    "cli/serial_cli.c"
    "ota/ota_manager.c"
    "proxy/http_proxy.c"
    "tools/tool_registry.c"
    "tools/tool_web_search.c"
    "tools/tool_get_time.c"
    "tools/tool_files.c"
    "tools/tool_cron.c"
    "skills/skill_loader.c"
    "cron/cron_service.c"
    "heartbeat/heartbeat.c"
)

if(MIMI_BOARD_PROFILE EQUAL 0)
    # Xiaozhi ST7789T SPI profile
    set(PROFILE_SRCS
        "display/Vernon_ST7789T/Vernon_ST7789T.c"
        "display/display_panel_st7789t.c"
    )
    set(EMBED_ASSETS "../assets/banner_320x172.rgb565")
elseif(MIMI_BOARD_PROFILE EQUAL 1)
    # Waveshare ESP32-S3-Touch-LCD-1.46B (SPD2010 QSPI + LVGL)
    set(PROFILE_SRCS
        "display/display_panel_spd2010.c"
        "io_expander/tca9554.c"
        "ui/lvgl_adapter.c"
    )
    set(EMBED_ASSETS "../assets/aisync_logo_360x360.rgb565")
endif()

idf_component_register(
    SRCS
        ${COMMON_SRCS}
        ${PROFILE_SRCS}
    INCLUDE_DIRS
        "."
    EMBED_FILES
        ${EMBED_ASSETS}
    REQUIRES
        nvs_flash esp_wifi esp_netif esp_http_client esp_http_server
        esp_https_ota esp_event json spiffs console vfs app_update esp-tls
        driver esp_lcd esp_timer led_strip qrcode
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC MIMI_BOARD_PROFILE=${MIMI_BOARD_PROFILE})
