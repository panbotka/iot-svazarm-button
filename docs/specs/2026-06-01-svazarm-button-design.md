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
- **Power:** mains via **HLK-PM01** (230V AC → 5V DC, isolated, PCB-mount) into the `5V` pin — no external power brick. USB used only for programming (never simultaneously with mains). The device therefore contains lethal mains voltage; see WIRING.md.

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

Backend URL, Auth token, and the send cooldown are configurable at runtime via
the WiFiManager portal (custom parameter fields), stored in NVS namespace `cfg`.
The `config.h` values are first-boot defaults only; NVS overrides them. The
firmware reads `g_apiUrl` / `g_authToken` at request time and `g_cooldownMs` for
the throttle.

### Send throttle (cooldown)

- A button press sends the POST only if at least the cooldown has elapsed since the last **successful** send; otherwise the press is ignored (quick double blink).
- Default `OPEN_COOLDOWN_S` = 14400 s (4 h); `0` disables the throttle.
- Failed sends do not start the cooldown, so they can be retried immediately.
- The throttle state is held in RAM (`lastSendMs`, `hasSent`) and resets on reboot — there is no RTC, so wall-clock persistence is out of scope.

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

## Power

- CPU underclocked to 80 MHz (from the 240 MHz default) to reduce heat. 80 MHz is the floor that keeps WiFi functional; lower frequencies disable the radio.
- WiFi modem sleep enabled — the connection stays up while the radio naps between beacons, so button response remains instant.

## Out of Scope (for now)

- Deep sleep / battery operation (assumed mains/USB powered for instant response)
- Idle light-sleep with reconnect-on-press (considered, rejected to keep instant response)
- OTA updates
- Multiple buttons / multiple endpoints
