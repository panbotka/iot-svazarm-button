# Svazarm Button - IoT Pub Open Button

## Overview
ESP32 DevKitC with a physical button. On press it sends
`POST <API_URL>` (host is a secret, see `config.h`) with an `Authorization`
header. The server (`keg-scale` backend, `POST /api/button/svazarm/open`)
announces in the guests' WhatsApp group that the pub at Svazarm just opened.

## Docs — IMPORTANT
- The wiring guide exists in TWO languages: `WIRING.md` (English) and `WIRING.cs.md` (Czech), each embedding its own colour SVG schematic (`wiring-schematic.svg` / `wiring-schematic.cs.svg`).
- **ALWAYS edit BOTH language versions together** — guide text AND the SVG schematic — so the English and Czech sets never drift out of sync.
- The SVGs are hand-authored; preview with `chromium --headless --no-sandbox --screenshot=/tmp/w.png --window-size=1040,720 file://$PWD/wiring-schematic.svg` and check for overlaps.

## Hardware
- ESP32 DevKitC (38pin), 3.3V logic
- Momentary push button: GPIO27 -> GND (internal pull-up, active LOW)
- External LED + ~330Ω resistor: GPIO26 -> GND (active HIGH) for press feedback
- Powered over USB-C (5V) — the board's USB-C port both powers and programs it. Low-voltage SELV device, no mains voltage. See WIRING.md.

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
- Backend URL + Auth token + cooldown are stored in NVS namespace `cfg` (keys `url`, `token`, `cooldown` seconds).
- `config.h` `API_URL` / `AUTH_TOKEN` / `OPEN_COOLDOWN_S` are first-boot DEFAULTS only; NVS values override them.
- `sendOpenRequest()` uses the runtime globals `g_apiUrl` / `g_authToken`; the throttle uses `g_cooldownMs`.
- Set via the WiFiManager config portal (custom `WiFiManagerParameter` fields, `param`/`exit` menu, `saveParamsCallback` persists to NVS).

## Send Throttle (cooldown)
- A press sends the POST only if `millis() - lastSendMs >= g_cooldownMs`; otherwise it's ignored with a quick double blink.
- Default `OPEN_COOLDOWN_S` = 14400 (4h). `0` disables the throttle.
- Only **successful** sends update `lastSendMs` / `hasSent` (failures can retry immediately).
- Throttle state is RAM-only (`lastSendMs`, `hasSent`) — resets on reboot; no RTC/persisted wall clock.

## Config Portal Triggers
- **Button held ~3s at boot** (`buttonHeldAtBoot()`): opens portal with WiFi kept; edit URL/token only, then Exit; board reconnects.
- **Connection failure**: portal opens automatically; restarts after 180s timeout.
- **Double-reset** (2x within 3s): clears WiFi creds, opens full portal (WiFi + URL/token).

## Secrets — do NOT commit
- `src/config.h` is gitignored. It holds the real `API_URL` host, `AUTH_TOKEN`, and WiFi passwords.
- `src/config.example.h` is the committed template with placeholders only.
- The server host (`API_URL`) is itself secret — keep it out of all committed files (README, docs, examples).

## Power
- CPU underclocked to 80 MHz (`setCpuFrequencyMhz(CPU_FREQ_MHZ)` first thing in setup) to cut heat — 80 MHz is the floor that keeps WiFi working; below it the radio is disabled.
- WiFi modem sleep enabled (`WiFi.setSleep(true)`): connection stays up, radio naps between beacons. Button response stays instant.

## Key Decisions
- Button is active LOW with internal pull-up; debounced (~50ms), fires on confirmed press
- 3s press lockout so one press sends exactly one request
- External LED (active HIGH): 3 short blinks = setup done / ready (`ledReady()`), 10x blink = request accepted, 3 long blinks = error, 2 quick blinks = ignored (cooldown)
- WiFi kept connected in `loop()`; `sendOpenRequest()` reconnects best-effort before POSTing
- Multi-WiFi: up to 20 networks in Preferences (NVS), WiFiMulti for auto-connect, WiFiManager captive portal as fallback
- Pattern mirrors the sibling `lampicka` project (same WiFi/NVS/DRD approach)
