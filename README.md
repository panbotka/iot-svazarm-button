# Svazarm Button

A physical "open the pub" button. When pressed, an ESP32 sends an HTTP `POST`
to a configured server endpoint, which then announces in the guests' WhatsApp
group that the pub at Svazarm has just opened.

## How It Works

1. ESP32 connects to WiFi (configured via [WiFiManager](https://github.com/tzapu/WiFiManager) captive portal, with default networks from `config.h`)
2. On button press (debounced), it sends `POST` to the configured `API_URL`
3. The request carries an `Authorization` header (exact-match token, no `Bearer` prefix); the JSON body is small debug info and is ignored by the server
4. On a `2xx` response the external LED blinks **10×** (request accepted); on failure it shows an error pattern (3 long blinks)

The server side lives in the `keg-scale` backend (`POST /api/button/svazarm/open`).

## Hardware

| Component | Description |
|-----------|-------------|
| [ESP32 DevKitC (38pin)](https://dratek.cz/arduino-platforma/51547-esp32-devkitc-development-board-38pin.html) | WiFi/BLE microcontroller, 3.3V logic |
| Momentary push button | Between `GPIO27` and `GND` (internal pull-up, active LOW) |
| External LED + resistor (~330Ω) | On `GPIO26` to `GND` (active HIGH) — press feedback |
| USB / 5V supply | Powers the board |

## Wiring

See **[WIRING.md](WIRING.md)** for the full wiring guide, pinout, and assembly notes.

## Firmware

- **Framework:** Arduino (C++) with `arduino-cli` + `Makefile` (not PlatformIO)
- **WiFi:** [tzapu/WiFiManager](https://github.com/tzapu/WiFiManager) for captive-portal provisioning, multi-WiFi stored in NVS
- **HTTP:** built-in ESP32 `HTTPClient`
- **JSON:** ArduinoJson for the debug request body

### Build

```bash
make install   # one-time: install ESP32 core + board manager URL
make deps      # one-time: install WiFiManager + ArduinoJson
cp src/config.example.h src/config.h   # then fill in real values
make compile   # compile the sketch
make flash     # compile and upload
make monitor   # serial monitor @ 115200 baud
```

## Configuration

Copy `src/config.example.h` to `src/config.h` and fill in:

- `API_URL` — the server endpoint (**secret host** — never committed)
- `AUTH_TOKEN` — sent verbatim in the `Authorization` header
- `DEFAULT_NETWORKS` — default WiFi networks loaded on first boot

`src/config.h` is gitignored so secrets stay out of the repo.

## Notes

- Reset the board **twice within 3 s** to clear stored WiFi credentials and re-open the captive portal (`SvazarmButton-Setup`).
- A short press lockout (3 s) prevents one press from firing multiple requests.
