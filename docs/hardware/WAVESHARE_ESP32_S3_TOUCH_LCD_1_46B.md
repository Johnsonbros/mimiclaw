# Waveshare ESP32-S3-Touch-LCD-1.46B integration notes

This document captures the board facts we need for MimiClaw and a concrete integration path for this repository.

## 1) Hardware facts (from Waveshare product/wiki)

- MCU: **ESP32-S3R8**, dual-core LX7 up to 240 MHz.
- Memory: **16 MB Flash + 8 MB PSRAM**.
- Display: **1.46" round LCD, 412×412**, driven over **QSPI** (not classic SPI 3-wire/4-wire control path).
- Touch: capacitive touch controller over **I2C** with interrupt support.
- IMU: **QMI8658** 6-axis IMU.
- Other onboard devices: PCF85063 RTC, TF card, microphone, speaker, USB-C, battery charging path.

### Internal wiring relevant to MimiClaw

From Waveshare wiki pin map:

- LCD (QSPI): SCK=GPIO40, D0=GPIO46, D1=GPIO45, D2=GPIO42, D3=GPIO41, CS=GPIO21, TE=GPIO18, BL=GPIO5.
- Touch + IMU share I2C bus: SCL=GPIO10, SDA=GPIO11.
- Touch interrupt: GPIO4.
- LCD reset / touch reset are behind onboard IO expander (EXIO), not direct ESP32 GPIO.

## 2) What this means for current MimiClaw firmware

Current `main/display/display.c` is implemented for an ST7789T-like **SPI** panel using:

- DC pin,
- direct RST pin,
- standard `esp_lcd_panel_io_spi` path.

That cannot directly drive the Waveshare 1.46B panel as-is, because this board's screen path is QSPI + EXIO-managed reset.

## 3) Repo changes made for board onboarding

To make the repo board-aware and prevent silent pin mismatches:

1. Added a centralized board profile file: `main/board/board_pins.h`.
2. Moved button and I2C pin definitions to board-profile macros.
3. Added a compile-time guard in `display.c` so selecting 1.46B fails fast until its QSPI driver is added.

This gives us a safe base for incremental board bring-up without breaking existing boards.

## 4) How to connect MimiClaw UI to the 1.46B display (implementation plan)

### Phase A — Display bring-up (no touch yet)

1. Add a new display backend, e.g. `main/display/waveshare_146b_display.c`.
2. Use ESP-IDF `esp_lcd` QSPI panel APIs for the 412×412 panel.
3. Implement EXIO (TCA9554) helper to toggle panel reset and touch reset lines.
4. Port `display_show_banner()` and `display_show_config_screen()` to this backend.
5. Keep existing UI API stable (`display.h`) so upper layers stay unchanged.

### Phase B — Touch input wiring

1. Add touch driver component (I2C + INT), polling in a dedicated task or timer.
2. Convert touch coordinates to UI events:
   - swipe up/down -> scroll config list,
   - tap on row -> future action/select,
   - long press -> screen toggle/back.
3. Keep button input as fallback for non-touch boards.

### Phase C — UI architecture upgrade (recommended)

Two viable options:

- **Option 1 (low RAM, smallest change):** keep custom framebuffer renderer and add touch gesture handling only.
- **Option 2 (richer UI):** move config screen to LVGL and feed touch points into LVGL input device API.

For this board specifically, Option 2 is practical because 8 MB PSRAM allows richer widgets and smoother transitions.

## 5) Concrete tasks to implement next in this repo

1. Create `main/periph/exio_tca9554.[ch]` for reset/CS helper lines.
2. Add `main/display/waveshare_146b_display.[ch]` backend and hook it via board profile switch.
3. Add `main/touch/touch_manager.[ch]` (I2C, interrupt, coordinate filtering).
4. Extend `main/ui/config_screen.c` with touch event handlers (scroll/select).
5. Add board-specific quick-start in README once display/touch path is merged.

## 6) Practical first-test checklist on hardware

1. Flash firmware with board profile set to `MIMI_BOARD_WAVESHARE_146B` (after QSPI backend is added).
2. Verify backlight control and a full-screen color test.
3. Render banner at 412×412 and validate orientation.
4. Enable config screen; test scroll via touch and button fallback.
5. Validate IMU I2C communication still works on shared bus (GPIO10/11).
