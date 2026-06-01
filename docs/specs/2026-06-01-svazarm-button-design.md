# Svazarm Button — Design

**Date:** 2026-06-01

## Purpose

A physical button that "opens the pub". On press, an ESP32 sends an HTTP `POST`
to a configured server endpoint. The server (keg-scale backend) generates and
sends a WhatsApp message to the guests' group chat announcing that the pub at
Svazarm has just opened.

## Hardware

- **Board:** ESP32 DevKitC (38-pin), 3.3V logic
- **Button:** momentary push button, `GPIO27` → `GND`, internal pull-up (active LOW)
- **LED:** external LED + ~330Ω resistor on `GPIO26` → `GND` (active HIGH), press feedback
- **Power:** USB / 5V

## Firmware Stack

- Arduino C++ via `arduino-cli` + `Makefile` (not PlatformIO), FQBN `esp32:esp32:esp32`
- WiFiManager (tzapu) for captive-portal provisioning + multi-WiFi in NVS
- ESP32 `HTTPClient` + `WiFiClientSecure` (`setInsecure()`) for HTTPS POST
- ArduinoJson for the debug request body

Mirrors the sibling `lampicka` project's WiFi/NVS/double-reset patterns.

## Server Contract

- Endpoint: `POST /api/button/svazarm/open`
- Auth: `Authorization` header must EXACTLY equal the server's AuthToken (no `Bearer` prefix)
- Body: ignored by the server today; firmware sends small debug JSON anyway
- Success: HTTP `204 No Content` (any 2xx treated as success)

## Behaviour

1. On boot: connect via WiFiMulti using stored/default networks; fall back to captive portal.
2. WiFi kept alive in `loop()`.
3. Button press is debounced (~50 ms), fires on confirmed press, then a 3 s lockout prevents repeat fires.
4. On press: ensure WiFi (best-effort reconnect) → `POST` with `Authorization` header + debug JSON body.
5. Feedback on external LED: **10× blink** = accepted (2xx); **3 long blinks** = error (no WiFi / non-2xx).
6. Double-reset (twice within 3 s) clears stored WiFi credentials.

## Runtime Configuration

Backend URL and Auth token are configurable at runtime via the WiFiManager
portal (custom parameter fields), stored in NVS namespace `cfg`. The `config.h`
values are first-boot defaults only; NVS overrides them. The firmware reads
`g_apiUrl` / `g_authToken` at request time.

Portal triggers:

- **Button held ~3 s at boot** — opens the portal keeping WiFi; edit URL/token only, then Exit.
- **Connection failure** — portal opens automatically (180 s timeout, then reboot).
- **Double-reset** (twice within 3 s) — clears WiFi creds, opens full portal (WiFi + URL/token).

## Secrets

- `src/config.h` (gitignored) holds the real `API_URL` host, `AUTH_TOKEN`, and WiFi passwords.
- `src/config.example.h` is the committed template with placeholders only.
- The server host is itself secret and must not appear in any committed file.

## Repository

- `panbotka/iot-svazarm-button`, private
- License: WTFPL

## Out of Scope (for now)

- Deep sleep / battery operation (assumed mains/USB powered for instant response)
- OTA updates
- Multiple buttons / multiple endpoints
