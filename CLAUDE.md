# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

MimiClaw is an AI assistant firmware for ESP32-S3, written in pure C on FreeRTOS (no Linux). It runs a ReAct agent loop that talks to Anthropic Claude or OpenAI GPT, with Telegram and WebSocket as user-facing channels. All data is stored locally on the device's 12 MB SPIFFS flash partition.

## Build Commands

Requires **ESP-IDF v5.5+** installed and sourced (`source $IDF_PATH/export.sh`).

```bash
# First-time setup
idf.py set-target esp32s3
cp main/mimi_secrets.h.example main/mimi_secrets.h  # then edit with credentials

# Build
idf.py build

# Clean build (required after mimi_secrets.h changes)
idf.py fullclean && idf.py build

# Flash and monitor (use native USB port, not COM port)
idf.py -p PORT flash monitor

# Monitor only
idf.py -p PORT monitor
```

There are no unit tests or linting tools configured. Verification is done by building (`idf.py build`) and testing on hardware.

## Architecture

**Dual-core task model:** Core 0 handles all I/O (Telegram polling, WebSocket, serial CLI, outbound dispatch). Core 1 is dedicated to the agent loop (context building, LLM API calls, tool execution).

**Message flow:** Channels push `mimi_msg_t` to an inbound FreeRTOS queue → agent loop pops, builds context, runs ReAct loop (up to 10 LLM iterations with tool calls) → pushes response to outbound queue → dispatcher routes back to originating channel.

**Two-layer config:** Build-time defaults in `main/mimi_secrets.h`, runtime overrides via serial CLI stored in NVS flash. NVS values take precedence. Most modules read config with an NVS-first fallback pattern.

**Memory model:** Large buffers (32 KB+) must use PSRAM via `heap_caps_calloc(1, size, MALLOC_CAP_SPIRAM)`. Internal SRAM is reserved for FreeRTOS stacks and WiFi. Check `mimi_config.h` for buffer size constants.

## Key Source Paths

- `main/mimi.c` — Entry point, `app_main()` init sequence and task creation
- `main/mimi_config.h` — All compile-time constants (buffer sizes, task params, NVS keys, SPIFFS paths)
- `main/agent/agent_loop.c` — ReAct loop: LLM call → tool dispatch → repeat until end_turn
- `main/agent/context_builder.c` — Assembles system prompt from SOUL.md + USER.md + AGENTS.md + MEMORY.md + daily notes + skills
- `main/llm/llm_proxy.c` — Dual-provider HTTP client (Anthropic native API + OpenAI compatible), non-streaming, tool_use parsing
- `main/tools/tool_registry.c` — Tool registration, JSON schema generation, dispatch by name (max 12 tools)
- `main/bus/message_bus.c` — `mimi_msg_t` inbound/outbound FreeRTOS queues (depth 8 each, content heap-allocated with ownership transfer)
- `main/memory/` — `memory_store.c` (MEMORY.md + daily notes) and `session_mgr.c` (JSONL per chat, ring buffer of last 20 messages)
- `main/telegram/telegram_bot.c` — Long polling with 30s timeout, message splitting for 4096 char limit
- `main/gateway/ws_server.c` — WebSocket on port 18789, JSON protocol, max 4 clients

## Adding a New Tool

1. Create `main/tools/tool_yourname.c` and `.h`
2. Implement the execute function: `esp_err_t tool_yourname_exec(const char *input_json, char *output, size_t out_size)`
3. Register in `tool_registry_init()` inside `main/tools/tool_registry.c` with name, description, and JSON schema
4. Add the `.c` file to `SRCS` in `main/CMakeLists.txt`

Tool input is a JSON string matching the schema. Output is written to a caller-provided buffer. See `tool_web_search.c` or `tool_get_time.c` for examples.

## SPIFFS Storage Layout

SPIFFS is flat (no real directories). Files use path-like names under `/spiffs/`:
- `/spiffs/config/` — Bootstrap files loaded into system prompt: `SOUL.md`, `USER.md`, `AGENTS.md`
- `/spiffs/memory/` — `MEMORY.md` (long-term) and `YYYY-MM-DD.md` (daily notes)
- `/spiffs/sessions/` — `tg_<chat_id>.jsonl` (conversation history, one JSON object per line)
- `/spiffs/skills/` — Skill markdown files auto-loaded into system prompt

## Hardware Target

ESP32-S3 with 16 MB flash (QIO 80 MHz) and 8 MB PSRAM (Octal SPI). Reference board: Waveshare ESP32-S3 Touch LCD 1.46B (ST7789 320x172 display, QMI8658 IMU, WS2812B RGB LED). Pin definitions are in `main/board/board_pins.h`.

## LLM Integration Details

Both providers use non-streaming requests. Anthropic uses `system` as a top-level field; OpenAI puts it in the messages array. The `stop_reason` field determines if the agent continues (`tool_use`) or finishes (`end_turn`). Max tokens: 4096. Timeout: 120s. Response buffer: 32 KB (PSRAM).

## Current Gaps (see docs/TODO.md)

P0: Memory persistence via agent tools, Telegram user allowlist. P1: Telegram markdown→HTML, media handling, subagent support. The suggested implementation order in TODO.md reflects dependency ordering.
