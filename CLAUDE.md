# Svazarm Button - IoT Pub Open Button

## Overview
ESP32 DevKitC with a physical button. On press it sends
`POST <API_URL>` (host is a secret, see `config.h`) with an `Authorization`
header. The server (`keg-scale` backend, `POST /api/button/svazarm/open`)
announces in the guests' WhatsApp group that the pub at Svazarm just opened.

## Hardware
- ESP32 DevKitC (38pin), 3.3V logic
- Momentary push button: GPIO27 -> GND (internal pull-up, active LOW)
- External LED + ~330Ω resistor: GPIO26 -> GND (active HIGH) for press feedback
- Powered via USB / 5V

## Firmware Stack
- Arduino (C++) with arduino-cli + Makefile (not PlatformIO)
- FQBN: `esp32:esp32:esp32`
- WiFiManager (tzapu/WiFiManager) for WiFi provisioning via captive portal
- ArduinoJson for the debug request body
- ESP32 HTTPClient + WiFiClientSecure (`setInsecure()`) for the HTTPS POST

## Build Commands
- `make compile` — compile the sketch
- `make flash` — compile and upload to the ESP32
- `make monitor` — open serial monitor (115200 baud)
- `make deps` — install required Arduino libraries
- `make install` — install ESP32 core + board manager URL
- `make list` — list connected boards

## Server Contract
- Endpoint: `POST /api/button/svazarm/open`
- Auth: `Authorization` header must EXACTLY equal the server's AuthToken (no `Bearer` prefix)
- Body: ignored by the server today (firmware still sends small debug JSON)
- Success: HTTP `204 No Content` (any 2xx is treated as success)

## Runtime Config (NVS)
- Backend URL + Auth token are stored in NVS namespace `cfg` (keys `url`, `token`).
- `config.h` `API_URL` / `AUTH_TOKEN` are first-boot DEFAULTS only; NVS values override them.
- `sendOpenRequest()` uses the runtime globals `g_apiUrl` / `g_authToken`, not the `#define`s.
- Set via the WiFiManager config portal (custom `WiFiManagerParameter` fields, `param`/`exit` menu, `saveParamsCallback` persists to NVS).

## Config Portal Triggers
- **Button held ~3s at boot** (`buttonHeldAtBoot()`): opens portal with WiFi kept; edit URL/token only, then Exit; board reconnects.
- **Connection failure**: portal opens automatically; restarts after 180s timeout.
- **Double-reset** (2x within 3s): clears WiFi creds, opens full portal (WiFi + URL/token).

## Secrets — do NOT commit
- `src/config.h` is gitignored. It holds the real `API_URL` host, `AUTH_TOKEN`, and WiFi passwords.
- `src/config.example.h` is the committed template with placeholders only.
- The server host (`API_URL`) is itself secret — keep it out of all committed files (README, docs, examples).

## Key Decisions
- Button is active LOW with internal pull-up; debounced (~50ms), fires on confirmed press
- 3s press lockout so one press sends exactly one request
- External LED (active HIGH): 10x blink = request accepted, 3 long blinks = error
- WiFi kept connected in `loop()`; `sendOpenRequest()` reconnects best-effort before POSTing
- Multi-WiFi: up to 20 networks in Preferences (NVS), WiFiMulti for auto-connect, WiFiManager captive portal as fallback
- Pattern mirrors the sibling `lampicka` project (same WiFi/NVS/DRD approach)
