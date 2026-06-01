# Wiring Guide — Svazarm Button

A full build guide for the Svazarm Button hardware. Everything here is
**low-voltage DC (SELV)** — there is **no mains voltage** anywhere in this
device. The board is powered from 5 V (USB or an external 5 V supply) and all
logic runs at **3.3 V**.

> ⚠️ **3.3 V logic, NOT 5 V tolerant.** Never feed 5 V into any GPIO, the `3V3`
> pin, or the `EN` pin. 5 V belongs only on the `5V`/`VIN` pin (or the USB
> connector). Doing otherwise can permanently damage the ESP32.

---

## 1. Bill of Materials

| # | Part | Spec | Notes |
|---|------|------|-------|
| 1 | ESP32 DevKitC | 38-pin, ESP-WROOM-32 | The controller board |
| 2 | Momentary push button | SPST-NO, normally open | Any tactile / panel button; no rating concerns at 3.3 V / <1 mA |
| 3 | LED | 3 mm or 5 mm, **red / yellow / green preferred** | Feedback indicator (see §5 for colour choice) |
| 4 | Resistor for LED | **330 Ω**, ¼ W, ±5 % | Current-limiting; see §5 for the calculation |
| 5 | Resistor — button pull-up (optional) | 10 kΩ, ¼ W | Only for long button wires; firmware already uses the internal pull-up |
| 6 | Capacitor — button debounce (optional) | 100 nF ceramic | Hardware debounce for noisy/long runs |
| 7 | Power supply | Regulated **5 V DC**, ≥ 500 mA | USB charger or a 5 V PSU; ESP32 + WiFi peaks ~250–500 mA |
| 8 | Hook-up wire | 0.25–0.5 mm² (AWG 22–24) | Stranded for flexibility |
| 9 | Enclosure | Any small box | Optional, recommended for a wall/panel button |

Average current draw is low: the firmware underclocks the CPU to 80 MHz and uses
WiFi modem sleep, so steady-state is roughly **40–80 mA**, with short transmit
peaks. A 5 V / 1 A supply is plenty.

---

## 2. Pins Used

Only four board pins are used. Pin **names are printed on the DevKitC silkscreen**
— always go by the silkscreen label, not by physical position, because pin order
differs between board revisions.

| Board pin | Direction | Connects to | Purpose |
|-----------|-----------|-------------|---------|
| `GPIO27` | Input (pull-up) | Button leg A | Reads the button (active LOW) |
| `GPIO26` | Output | LED anode via 330 Ω | Drives the feedback LED (active HIGH) |
| `GND` | — | Button leg B **and** LED cathode | Common ground / 0 V reference |
| `5V` (a.k.a. `VIN`) | Power in | 5 V supply + | Board power (skip if powering over USB) |

Other relevant pins (not wired, but good to know):

- `3V3` — regulated 3.3 V **output** from the on-board LDO. Can power small 3.3 V
  loads; do **not** feed power into it unless you are deliberately bypassing the
  regulator (§6, advanced).
- `EN` — chip reset (active LOW). The on-board button labelled **EN/RST** triggers
  a reset; pressing it **twice within 3 s** wipes stored WiFi (see §9).
- `GND` — there are **several** GND pins; any of them is fine and they are all
  common.

---

## 3. Full Schematic

```
                         ESP32 DevKitC (38-pin)
                    ┌──────────────────────────────┐
   5V supply (+) ───┤ 5V / VIN                      │
              (−) ──┤ GND ───────────┐              │
                    │                │              │
                    │           GPIO26 ├──[ R1 330Ω ]──►|──┐
                    │                │            LED D1    │
                    │                │         (anode)(cathode)
                    │                │              │       │
                    │           GPIO27 ├───────┐    │       │
                    │                │         │    │       │
                    │              GND ├──┐     │    │       │
                    └──────────────────────────────┘ │      │
                                       │     │        │      │
                                       │     │  SW1   │      │
                                       │     └──○─○───┘      │
                                       │      (push button)  │
                                       │                     │
                                       └──────── GND ─────────┘
                                       (common 0 V: PSU −, LED cathode,
                                        button leg B)

   Legend:  ──►|──  LED (arrow = anode → cathode, current flows this way)
            ──○─○── momentary normally-open contact
            [ R ]   resistor
```

### Current-flow summary

- **LED ON:** firmware drives `GPIO26` HIGH (3.3 V) → current flows `GPIO26 → R1 →
  LED anode → LED cathode → GND`. `GPIO26` LOW = LED off.
- **Button:** `GPIO27` idles HIGH via the internal pull-up. Pressing `SW1` shorts
  `GPIO27` to `GND`, pulling it LOW — that LOW edge is the "press".

---

## 4. Button Sub-circuit

```
   GPIO27 ─────────────┬───────────○  ○───────── GND
                       │           (SW1, NO)
            (optional) │
            R2 10kΩ ───┤  to 3V3   (extra pull-up, usually unnecessary)
                       │
            (optional) │
            C1 100nF ──┴───────────────────────── GND   (debounce)
```

- **Baseline (recommended):** just the button between `GPIO27` and `GND`. The
  firmware enables `INPUT_PULLUP` (~45 kΩ internal), so the line sits at 3.3 V
  when released and 0 V when pressed. No external parts needed.
- **Long wire runs (> ~30 cm) or electrically noisy environment:** add **C1
  (100 nF)** from `GPIO27` to `GND` right at the board for hardware debounce, and
  optionally **R2 (10 kΩ)** from `GPIO27` to `3V3` to stiffen the pull-up. The
  firmware also debounces in software (~50 ms), so this is belt-and-suspenders.
- **Polarity:** the button is not polarised — either leg can go to `GPIO27` or
  `GND`.

---

## 5. LED Sub-circuit & Resistor Selection

```
   GPIO26 ──[ R1 ]──►|── GND
                     LED
                (anode)(cathode)
```

The resistor limits the LED current. Size it from:

```
        Vgpio − Vf
  R  =  ──────────         Vgpio = 3.3 V (ESP32 GPIO HIGH)
            I              Vf    = LED forward voltage (colour-dependent)
                           I     = target LED current (5–10 mA is plenty)
```

| LED colour | Typ. Vf | R for ~6 mA | Nearest E12 | Result |
|------------|---------|-------------|-------------|--------|
| Red | 1.8–2.0 V | (3.3−1.9)/0.006 ≈ 233 Ω | **220–330 Ω** | Bright, ideal |
| Yellow / amber | 2.0–2.2 V | ≈ 200 Ω | **220–330 Ω** | Bright |
| Green (standard) | 2.0–2.2 V | ≈ 200 Ω | **220–330 Ω** | Bright |
| Blue / white / "pure green" | 2.8–3.4 V | marginal at 3.3 V | 100–150 Ω | Dim & unreliable — **avoid** |

**Use 330 Ω with a red/yellow/green LED.** It gives ~4–5 mA — clearly visible and
well within the ESP32 GPIO limits.

- **GPIO current limits:** an ESP32 GPIO can source up to ~40 mA absolute, but the
  recommended continuous limit is **≤ 20 mA per pin** (≤ 12 mA is comfortable).
  330 Ω keeps you far below that.
- **Polarity matters:** the **anode (longer leg, +)** goes toward the resistor /
  `GPIO26`; the **cathode (shorter leg, flat side of the rim, −)** goes to `GND`.
  A reversed LED simply won't light.
- The resistor can be on either side of the LED (anode side or cathode side) — it
  limits current in series either way. Anode side (as drawn) is conventional.

---

## 6. Power Options

Pick **one** of these — do not combine USB and an external 5 V feed at the same
time unless you know your board has input ORing/diode protection.

1. **USB (simplest).** Plug a 5 V USB charger into the board's USB connector. The
   on-board LDO produces 3.3 V for the ESP32. Best for bench work and most
   installs.
2. **External 5 V to `5V`/`VIN`.** Feed a regulated 5 V supply: **+** to `5V`,
   **−** to `GND`. Uses the same on-board LDO. Good for a permanent install
   wired to a 5 V PSU.
3. **Regulated 3.3 V to `3V3` (advanced).** If you already have a clean 3.3 V rail,
   feed it to `3V3` and `GND` and leave `5V`/USB disconnected. This **bypasses the
   on-board regulator** — the 3.3 V must be clean and current-capable (≥ 500 mA).
   Only do this if you understand the trade-offs.

Grounding: the supply **−**, the LED cathode, and the button must all share the
**same GND** as the board. With a single board and short wiring this is automatic
(one common GND node).

---

## 7. Assembly Steps

1. **Power off** — do all wiring with no power applied.
2. **LED:** identify anode (long leg) and cathode (short leg / flat). Solder `R1`
   (330 Ω) in series with the anode. Connect the free end of `R1` to `GPIO26` and
   the LED cathode to `GND`.
3. **Button:** connect one leg to `GPIO27`, the other to `GND`. (Optional: add
   `C1`/`R2` per §4.)
4. **Power:** wire your chosen power option from §6.
5. **Double-check** against §3 before applying power — especially LED polarity and
   that no 5 V touches a GPIO.
6. **First power-up:** the board boots, prints to serial at 115200 baud, and (if
   it can't find known WiFi) opens the `SvazarmButton-Setup` AP for configuration.

---

## 8. Verification (multimeter + serial)

Before/after powering up, sanity-check with a multimeter:

| Check | How | Expected |
|-------|-----|----------|
| No short on power | Resistance `5V`↔`GND`, power off | Not 0 Ω (no dead short) |
| Supply voltage | DC volts across `5V` ↔ `GND`, powered | ~4.7–5.2 V |
| Logic rail | DC volts across `3V3` ↔ `GND`, powered | ~3.2–3.3 V |
| Button released | DC volts `GPIO27` ↔ `GND`, powered, idle | ~3.3 V (internal pull-up) |
| Button pressed | same, holding the button | ~0 V |
| LED drive | DC volts `GPIO26` ↔ `GND` during a 10× blink | toggling 0 V / ~3.3 V |

On the serial monitor (`make monitor`, 115200 baud) you should see the boot
banner, `CPU frequency: 80 MHz`, the WiFi connection result, and on each accepted
press a `POST … -> 204 (OK)` line followed by the 10× LED blink.

---

## 9. Operating Notes & Pin Cautions

- **Active levels:** button is **active LOW** (pressed = 0 V); LED is **active
  HIGH** (`GPIO26` HIGH = lit). If you wire the LED the other way
  (`GPIO26 → cathode`, anode → `3V3`), you must invert `ledSet()` in `src.ino`.
- **Avoid strapping pins** for the button or LED: `GPIO0`, `GPIO2`, `GPIO5`,
  `GPIO12`, `GPIO15`. Holding them at the wrong level during boot changes the boot
  mode. `GPIO26`/`GPIO27` (used here) are safe, general-purpose pins.
- **Input-only pins** (`GPIO34/35/36/39`) cannot drive an LED and have no internal
  pull-ups — do not use them for the LED or rely on them for the button pull-up.
- **Double-reset:** pressing the board's **EN/RST** button twice within 3 s clears
  stored WiFi credentials and reopens the captive portal.
- **Config portal on demand:** hold the **push button** during power-up (~3 s,
  until the LED double-blinks) to open the portal and edit Backend URL / Auth
  token / cooldown without losing WiFi (see the README).
