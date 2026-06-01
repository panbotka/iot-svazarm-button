# Wiring Guide — Svazarm Button

*(Česká verze: [WIRING.cs.md](WIRING.cs.md). Both documents must be kept in sync.)*

A full build guide for the Svazarm Button hardware. The device is powered over
**USB-C** (5 V) — there is **no mains voltage anywhere**. Everything is
low-voltage DC (SELV): 5 V on the supply rail, 3.3 V logic.

> ⚠️ **3.3 V logic, NOT 5 V tolerant.** Never feed 5 V into any GPIO, the `3V3`
> pin, or the `EN` pin. 5 V belongs only on the `5V`/`VIN` pin (or the USB
> connector). Doing otherwise can permanently damage the ESP32.

---

## 1. Bill of Materials

| # | Part | Spec | Notes |
|---|------|------|-------|
| 1 | ESP32 DevKitC | 38-pin, ESP-WROOM-32, **USB-C** | The controller board (powers + programs over its USB-C port) |
| 2 | Momentary push button | SPST-NO, normally open | Any tactile / panel button; no rating concern at 3.3 V / <1 mA |
| 3 | LED | 3/5 mm, **red / yellow / green preferred** | Feedback indicator (see §5) |
| 4 | Resistor for LED | **330 Ω**, ¼ W, ±5 % | Current-limiting (see §5) |
| 5 | USB-C power source | 5 V, ≥ 500 mA (phone charger / power bank) | ESP32 + WiFi peaks ~250–500 mA |
| 6 | USB-C cable | data-capable for flashing | A charge-only cable powers but can't program |
| 7 | Resistor — button pull-up (optional) | 10 kΩ, ¼ W | Only for long button wiring |
| 8 | Capacitor — button debounce (optional) | 100 nF ceramic | Hardware debounce for long/noisy runs |
| 9 | USB-C panel jack + wire (optional) | breakout exposing VBUS + GND | Only if you want a panel-mounted USB-C socket feeding the `5V`/`GND` pins |
| 10 | Hook-up wire | 0.25–0.5 mm² (AWG 22–24) | Stranded for flexibility |
| 11 | Enclosure | Any small box | Optional; no mains, so not safety-critical |

Average current is low — the firmware underclocks the CPU to 80 MHz and uses WiFi
modem sleep, so steady-state is roughly **40–80 mA** with short transmit peaks.
Any 5 V / 1 A USB-C source is plenty.

---

## 2. Pins Used

Only four board pins are used (the button and LED). Power comes in through the
board's **USB-C** connector. **Go by the silkscreen labels**, not physical
position — pin order varies between board revisions.

| Board pin | Direction | Connects to | Purpose |
|-----------|-----------|-------------|---------|
| `GPIO27` | Input (pull-up) | Button leg A | Reads the button (active LOW) |
| `GPIO26` | Output | LED anode via 330 Ω | Drives the feedback LED (active HIGH) |
| `GND` | — | Button leg B **and** LED cathode | Common 0 V reference |
| `5V` (`VIN`) | Power | (internal from USB-C) — or a USB-C panel jack's VBUS | 5 V supply rail |

Reference (not wired): `3V3` is the regulated 3.3 V **output** of the on-board
LDO; `EN` is reset (double-press within 3 s wipes WiFi — §9).

---

## 3. Full Schematic

![Svazarm Button wiring schematic](wiring-schematic.svg)

The colour-coded schematic above is the complete build in one picture. A
text-only version follows for terminal/diff viewing:

```
   ┌──────────── 5 V / 3.3 V DC — SELV (USB-C powered, no mains) ────────────┐
   │                                                                         │
   │   USB-C ──VBUS(+5V)──────────────┐                                      │
   │   5V in                          ├── ESP32 5V                           │
   │         ──GND──────────┐         │                                      │
   │                        │     ESP32 GPIO26 ──[ R1 330Ω ]──►|──┐  LED     │
   │                        │         │                            │          │
   │                        │     ESP32 GPIO27 ─────○ ○────────────┤  SW1     │
   │                        │         │              (push)        │          │
   │                        └─── ESP32 GND ──────────── GND ───────┴── 0 V    │
   └─────────────────────────────────────────────────────────────────────────┘

   Legend:  ──►|──  LED (arrow = anode → cathode)
            ──○ ○── momentary normally-open contact      [ R ] resistor
```

### Summary

- **Power:** the board's USB-C port supplies 5 V internally to the `5V` rail and
  `GND`. (If you fit a separate USB-C panel jack, wire its **VBUS → `5V`** and
  **GND → `GND`**; only those two pins are used.)
- **LED:** `GPIO26` HIGH → current flows `GPIO26 → R1 → LED → GND`.
- **Button:** `GPIO27` idles HIGH via the internal pull-up; pressing `SW1` pulls
  it to `GND` (LOW) — that's the "press".

---

## 4. LED Sub-circuit & Resistor Selection

```
   GPIO26 ──[ R1 ]──►|── GND
                     LED
                (anode)(cathode)
```

Size the series resistor from:

```
        Vgpio − Vf
  R  =  ──────────      Vgpio = 3.3 V (GPIO HIGH),  Vf = LED forward voltage,
            I           I = target current (5–10 mA is plenty)
```

| LED colour | Typ. Vf | R for ~6 mA | Use | Verdict |
|------------|---------|-------------|-----|---------|
| Red | 1.8–2.0 V | ≈ 233 Ω | **220–330 Ω** | Bright, ideal |
| Yellow / amber | 2.0–2.2 V | ≈ 200 Ω | **220–330 Ω** | Bright |
| Green (standard) | 2.0–2.2 V | ≈ 200 Ω | **220–330 Ω** | Bright |
| Blue / white / "pure green" | 2.8–3.4 V | marginal | 100–150 Ω | Dim/unreliable — **avoid** |

**Use 330 Ω with a red/yellow/green LED** (~4–5 mA — clearly visible, far below the
ESP32 GPIO limit of ≤ 20 mA recommended). **Anode (long leg, +)** toward the
resistor/`GPIO26`; **cathode (short leg, flat side, −)** to `GND`. A reversed LED
just won't light. The resistor can sit on either side of the LED.

---

## 5. Button Sub-circuit

```
   GPIO27 ─────────────┬───────────○ ○───────── GND
                       │           (SW1, NO)
            (optional) │
            R2 10kΩ ───┤  to 3V3   (extra pull-up, usually unnecessary)
                       │
            (optional) │
            C1 100nF ──┴───────────────────────── GND   (debounce)
```

- **Baseline (recommended):** just the button between `GPIO27` and `GND`. The
  firmware enables `INPUT_PULLUP` (~45 kΩ) and debounces ~50 ms in software — no
  external parts needed.
- **Long runs (> ~30 cm) / noisy environment:** add **C1 (100 nF)** from `GPIO27`
  to `GND` at the board, optionally **R2 (10 kΩ)** to `3V3`.
- The button is not polarised — either leg to `GPIO27` or `GND`.

---

## 6. Power & Programming (USB-C)

- **Power & program over the same port:** plug a **data-capable USB-C cable** into
  the board's USB-C connector. The on-board LDO derives 3.3 V from the 5 V VBUS.
  Use `make flash` / `make monitor` over this connection.
- **A charge-only cable** will power the board but cannot upload firmware — use a
  proper data cable for flashing.
- **Permanent install:** any 5 V USB-C charger or power bank works. Optionally fit
  a **USB-C panel jack** in the enclosure and wire its **VBUS → `5V`** and
  **GND → `GND`** (only those two conductors are needed for power).
- Grounding: the USB-C `GND`, ESP32 `GND`, LED cathode and button all share one
  common 0 V node.

---

## 7. Assembly Steps

1. **LED:** identify anode (long leg) and cathode (short leg / flat). Solder `R1`
   (330 Ω) in series with the anode. Connect the free end of `R1` to `GPIO26` and
   the LED cathode to `GND`.
2. **Button:** connect one leg to `GPIO27`, the other to `GND` (optional `C1`/`R2`
   per §5).
3. **Power:** plug in the USB-C cable, or wire the optional panel jack
   (`VBUS → 5V`, `GND → GND`).
4. **Double-check** against §3 — LED polarity, no 5 V on a GPIO.
5. **First power-up:** the board boots, prints to serial at 115200 baud, and (if it
   can't find known WiFi) opens the `SvazarmButton-Setup` AP for configuration.

---

## 8. Verification (multimeter + serial)

| Check | How | Expected |
|-------|-----|----------|
| Supply rail | DC volts `5V` ↔ `GND`, powered | ~4.7–5.2 V |
| Logic rail | DC volts `3V3` ↔ `GND`, powered | ~3.2–3.3 V |
| Button released | DC volts `GPIO27` ↔ `GND`, idle | ~3.3 V (internal pull-up) |
| Button pressed | same, holding the button | ~0 V |
| LED drive | DC volts `GPIO26` ↔ `GND` during a 10× blink | toggling 0 V / ~3.3 V |

On the serial monitor (`make monitor`, 115200 baud) you should see the boot
banner, `CPU frequency: 80 MHz`, the WiFi connection result, and on each accepted
press a `POST … -> 204 (OK)` line followed by the 10× LED blink.

---

## 9. Operating Notes & Pin Cautions

- **Active levels:** button is **active LOW** (pressed = 0 V); LED is **active
  HIGH** (`GPIO26` HIGH = lit). Wiring the LED inverted (`GPIO26 → cathode`, anode
  → `3V3`) requires inverting `ledSet()` in `src.ino`.
- **Avoid strapping pins** for button/LED: `GPIO0/2/5/12/15`. `GPIO26`/`GPIO27`
  (used here) are safe general-purpose pins.
- **Input-only pins** (`GPIO34/35/36/39`) can't drive an LED and have no internal
  pull-ups — don't use them here.
- **Double-reset:** press the board's **EN/RST** button twice within 3 s to clear
  stored WiFi credentials and reopen the captive portal.
- **Config portal on demand:** hold the **push button** during power-up (~3 s,
  until the LED double-blinks) to edit Backend URL / Auth token / cooldown without
  losing WiFi (see the README).
