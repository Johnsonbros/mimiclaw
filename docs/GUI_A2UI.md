# GUI / A2UI Integration Guide for MimiClaw

This guide explains how to add a richer GUI system (including A2UI-style experiences) to MimiClaw on ESP32-S3 hardware.

## Current GUI baseline in MimiClaw

MimiClaw already has a working display path:

- ST7789T LCD driver and panel init in `main/display/display.c`
- a software framebuffer + primitive text rendering
- a config screen (`main/ui/config_screen.c`) showing QR/IP/config values
- button and IMU input infrastructure already present

So you do **not** need to start from zero; you can layer a full GUI framework on top of this.

## About your Amazon board link

The short link (`https://a.co/d/0aTshn6P`) currently resolves to a 404 from this environment, so the exact board model could not be verified automatically.

Because of that, the first step is to identify:

1. LCD controller (ST7789, ST7701, GC9A01, etc.)
2. resolution + orientation
3. touch controller (if any, e.g. CST816/FT6236/GT911)
4. available PSRAM (important for GUI framebuffers)

## Recommended GUI architecture

### Option A (recommended): LVGL as core GUI engine

Use LVGL as the rendering/UI framework and keep MimiClaw logic unchanged.

High-level layering:

1. **MimiClaw core** (agent loop, Telegram, memory, tools)
2. **UI state adapter** (maps system state -> view model)
3. **LVGL views** (status, chat preview, setup, settings)
4. **Display/touch HAL** (flush/input callbacks)

Why this is best now:

- mature ESP-IDF support
- good memory/performance controls for ESP32-S3
- easy to build menu/status/chat-like widgets

### Option B: Keep custom framebuffer UI

You can continue expanding `display_show_config_screen()` style drawing. This is lighter but scales poorly once you need multiple screens, interactions, and animation.

## Step-by-step implementation plan

### 1) Add LVGL dependency

In `main/idf_component.yml`, add LVGL:

```yaml
dependencies:
  lvgl/lvgl: "^9"
```

(Use LVGL 8.x if preferred by your board examples.)

### 2) Create a UI service module

Add a new module:

- `main/ui/ui_service.h`
- `main/ui/ui_service.c`

Responsibilities:

- initialize LVGL
- register display flush callback
- register touch/button input callback
- create a periodic tick + `lv_timer_handler()` task

### 3) Bridge existing display driver to LVGL

Either:

- keep current ST7789 panel init and implement LVGL flush into `esp_lcd_panel_draw_bitmap`, or
- swap to `esp_lvgl_port` if you want faster bring-up with slightly more abstraction.

For MimiClaw, reuse of the current `esp_lcd_panel_*` path is the least risky.

### 4) Add first three screens

Start small and useful:

1. **Boot/status**: Wi-Fi state, API provider, heap usage
2. **QR setup**: current IP + QR for onboarding (already conceptually present)
3. **Assistant status**: last inbound text + last outbound text snippet + tool activity

### 5) Connect events from core subsystems

Publish lightweight UI events from:

- Wi-Fi manager (connecting/connected/IP)
- agent loop (processing/tool call/idle)
- Telegram/WebSocket channels (last message metadata)

Use a small ring buffer/queue to avoid blocking core tasks.

### 6) Memory/performance guardrails (important on ESP32-S3)

- use partial frame buffers (double small draw buffers, not full-screen if RAM tight)
- place large GUI buffers in PSRAM
- keep animation effects minimal
- keep font count low
- avoid large image assets unless compressed and streamed

## "A2UI features" mapping for MimiClaw

If by A2UI you mean a polished assistant-first interface, these features map cleanly:

- **cards/panels** -> LVGL containers
- **quick actions** -> buttons triggering local commands (reconnect Wi-Fi, show QR, reboot)
- **status chips** -> small labels/icons for network/model/tool state
- **chat snapshot** -> rolling last-N message list
- **setup wizard** -> multi-step screen for network + token configuration

## Hardware compatibility checklist

Before coding UI screens, verify these with your board:

1. `idf.py menuconfig` -> PSRAM enabled
2. LCD init works with your panel driver
3. backlight pin/PWM control correct
4. touch IRQ/I2C works (if touch exists)
5. sustained refresh is stable (target 20+ FPS for smoothness, lower is okay for status UI)

## Practical next action for your board

Because the exact board SKU is unknown from the short link, do this first:

1. Share the full product name (or screenshot/spec sheet).
2. I can then provide:
   - exact pin mapping changes,
   - which panel/touch driver to use,
   - and a minimal LVGL patch set for MimiClaw.

If your board is one of the common ESP32-S3 LCD modules, MimiClaw is already very close to being GUI-ready.
