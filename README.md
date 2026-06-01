# Svazarm Button

A physical "open the pub" button. When pressed, an ESP32 sends an HTTP `POST`
to a configured server endpoint, which then announces in the guests' WhatsApp
group that the pub at Svazarm has just opened.

## How It Works

1. ESP32 connects to WiFi (configured via [WiFiManager](https://github.com/tzapu/WiFiManager) captive portal, with default networks from `config.h`)
2. On button press (debounced), it sends `POST` to the configured `API_URL`
3. The request carries an `Authorization` header (exact-match token, no `Bearer` prefix); the JSON body is small debug info and is ignored by the server
4. On a `2xx` response the external LED blinks **10×** (request accepted); on failure it shows an error pattern (3 long blinks)
5. A configurable **cooldown** (default 4 hours) throttles sends: a press within the cooldown window is ignored (quick double blink) so the request goes out at most once per period

The server side lives in the `keg-scale` backend (`POST /api/button/svazarm/open`).

## Hardware

| Component | Description |
|-----------|-------------|
| [ESP32 DevKitC (38pin)](https://dratek.cz/arduino-platforma/51547-esp32-devkitc-development-board-38pin.html) | WiFi/BLE microcontroller, 3.3V logic, USB-C |
| Momentary push button | Between `GPIO27` and `GND` (internal pull-up, active LOW) |
| External LED + resistor (~330Ω) | On `GPIO26` to `GND` (active HIGH) — press feedback |
| USB-C power | 5V from any USB-C charger / power bank — also used for programming |

This is a low-voltage (SELV) device — **no mains voltage** anywhere. See **[WIRING.md](WIRING.md)** for the full build guide and schematic.

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

`src/config.example.h` → copy to `src/config.h` and fill in the **first-boot defaults**:

- `API_URL` — the server endpoint (**secret host** — never committed)
- `AUTH_TOKEN` — sent verbatim in the `Authorization` header
- `DEFAULT_NETWORKS` — default WiFi networks loaded on first boot

`src/config.h` is gitignored so secrets stay out of the repo.

These values are only **defaults**. WiFi, Backend URL, and Auth token can all be
(re)configured at runtime through the config portal and are then stored in NVS,
overriding the compiled-in defaults.

### Config portal — WiFi setup

When the ESP32 **can't connect to any known WiFi**, it starts its own access
point and opens a configuration web page:

1. The board creates a WiFi network named **`SvazarmButton-Setup`**.
2. Connect to it with a phone/laptop — the captive portal pops up automatically (or open `http://192.168.4.1`).
3. Pick your WiFi network, enter the password, and **Save**.
4. The board stores the credentials in NVS and connects; next boot it connects automatically.
5. If nothing is entered within 3 minutes, the board reboots and retries.

### Config portal — changing Backend URL / Auth token / cooldown

The portal also has fields for **Backend URL**, **Auth token**, and **Min seconds
between sends** (the cooldown, in seconds — default `14400` = 4 hours, `0` disables
the throttle). There are two ways to open it:

**A) Hold the button at boot (keeps WiFi)** — best for editing just the URL/token:

1. Hold the button down, then power on / reset the board. **Keep holding ~3 s** until the LED gives a double blink.
2. Connect to the **`SvazarmButton-Setup`** network → open `http://192.168.4.1`.
3. Go to the **Setup** page, edit **Backend URL**, **Auth token**, and/or **Min seconds between sends**, then **Save**.
4. Click **Exit**. WiFi is untouched — the board reconnects with the stored credentials.

**B) Double-reset (full re-config)** — wipes WiFi and reconfigures everything:

1. Press the reset button **twice within 3 s**. This clears stored WiFi credentials.
2. The portal opens; set WiFi **and** edit Backend URL / Auth token, then **Save**.

## Notes

- The CPU runs **underclocked at 80 MHz** (down from 240) to reduce heat; 80 MHz is the lowest frequency that still keeps WiFi working. WiFi modem sleep is enabled, so the radio naps between beacons while staying connected — button response is unaffected.
- A short press lockout (3 s) prevents one press from firing multiple requests; the **cooldown** (default 4 h) is the higher-level throttle that limits how often a request is actually sent.
- Only **successful** sends start the cooldown — a failed request can be retried immediately.
- The cooldown timer lives in RAM, so a reboot/power-cycle resets it (the next press sends right away).
- The portal pre-fills the current Backend URL / Auth token / cooldown so you can tweak rather than retype.
