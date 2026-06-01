# Wiring Guide — Svazarm Button

ESP32 DevKitC (38-pin), 3.3V logic. Two peripherals: one momentary button
(input) and one external LED (output, feedback).

## Connections

| From (ESP32) | To | Notes |
|--------------|----|-------|
| `GPIO27` | Button leg A | Input, internal pull-up enabled in firmware |
| `GND` | Button leg B | Button shorts GPIO27 to GND when pressed (active LOW) |
| `GPIO26` | LED anode (+) via ~330Ω resistor | Output, active HIGH |
| `GND` | LED cathode (−) | |
| `5V` / USB | — | Board power |

## Diagram

```
ESP32 DevKitC
                 ┌──────────────┐
   GPIO27 ───────┤ ◜ button ◞  ├─────── GND
                 └──────────────┘
                    (pull-up internal; pressed = LOW)

   GPIO26 ──[ 330Ω ]──►|── GND
                       LED
                  (anode)  (cathode)
```

## Pin Choice Notes

- **GPIO27** — safe general-purpose pin, not a strapping pin, no boot-time conflicts. Good for a button.
- **GPIO26** — safe output pin. Drives the external LED directly through the resistor.
- Avoid strapping pins for the button (GPIO0, GPIO2, GPIO5, GPIO12, GPIO15) — holding them at boot can change boot mode.
- Resistor: ~220–470Ω depending on LED. 330Ω is a safe default for a standard 3mm/5mm LED on 3.3V.

## Assembly Notes

- The button needs no external pull-up/pull-down — firmware uses `INPUT_PULLUP`.
- LED is active HIGH: `GPIO26` HIGH = lit. If you wire it the other way (GPIO → cathode, anode → 3V3) you'd need to invert `ledSet()` in `src.ino`.
- Reset twice within 3 s to wipe stored WiFi and reopen the `SvazarmButton-Setup` captive portal.
