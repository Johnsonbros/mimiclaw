# CODEX.md

Codex-oriented repository quick reference for MimiClaw.

## Project shape

- Firmware entrypoint: `main/mimi.c`
- Agent loop: `main/agent/agent_loop.c`
- Prompt/context assembly: `main/agent/context_builder.c`
- LLM provider bridge: `main/llm/llm_proxy.c`
- Tools: `main/tools/`
- Memory/session storage: `main/memory/`
- Telegram channel: `main/telegram/`
- WebSocket gateway: `main/gateway/`
- Runtime CLI: `main/cli/`

## Build/test essentials

- Target: ESP32-S3 (`idf.py set-target esp32s3`)
- Typical build: `idf.py build`
- Flash/monitor: `idf.py -p <PORT> flash monitor`

## Config model

- Build-time defaults in `main/mimi_secrets.h`
- Runtime overrides via serial CLI stored in NVS
- Runtime values take precedence over build-time defaults

## Documentation map

- Product overview: `README.md`
- Architecture: `docs/ARCHITECTURE.md`
- Roadmap/gaps: `docs/TODO.md`
- Runtime persona/user data templates: `spiffs_data/config/`
