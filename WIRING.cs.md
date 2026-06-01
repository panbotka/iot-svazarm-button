# Návod k zapojení — Svazarm Button

*(English version: [WIRING.md](WIRING.md). Oba dokumenty se musí udržovat v synchronizaci.)*

Kompletní návod ke stavbě hardwaru tlačítka Svazarm. Zařízení je napájené ze sítě
přes modul **HLK-PM01** (230 V AC → 5 V DC), takže se zapojí přímo do zásuvky
**bez externího napájecího adaptéru**.

> ☠️ **NEBEZPEČÍ — UVNITŘ ZAŘÍZENÍ JE SÍŤOVÉ NAPĚTÍ 230 V.**
> Vstupní strana HLK-PM01 je pod životu nebezpečným síťovým napětím. **Síťovou
> část smí stavět a opravovat pouze osoba s odpovídající kvalifikací.** Vždy
> pracuj **bez napětí**: odpoj od zásuvky a před dotykem ověř zkoušečkou nulové
> napětí. Hotové zařízení **musí** být v uzavřené izolované krabičce bez
> odhalených síťových vodičů. Nikdy nepřipojuj USB a síť současně (viz §6/§7).

Zařízení má **dvě elektricky oddělené domény**:

- **Strana 230 V AC (nebezpečná):** síť → pojistka → vstup HLK-PM01.
- **Strana 5 V / 3,3 V DC (SELV, bezpečná na dotek):** výstup HLK-PM01 → ESP32,
  tlačítko, LED. Tato strana je od sítě **galvanicky oddělená** modulem HLK-PM01
  (izolace ~3 kVAC), což z ní dělá SELV.

> ⚠️ **Logika 3,3 V, NENÍ tolerantní k 5 V.** Nikdy nepřiváděj 5 V (ani síť!) na
> žádný GPIO, pin `3V3` ani `EN`. 5 V patří jen na pin `5V`/`VIN`.

---

## 1. Seznam součástek (BOM)

| # | Součástka | Specifikace | Poznámky |
|---|-----------|-------------|----------|
| 1 | [HLK-PM01](https://dratek.cz/arduino-platforma/176693-napajeci-zdroj-5vdc-600ma-3w-do-dps.html) | 100–264 V AC vstup → **5 V DC 600 mA 3 W** výstup, do DPS | Síťový zdroj; izolovaný výstup |
| 2 | ESP32 DevKitC | 38pinů, ESP-WROOM-32 | Řídicí deska |
| 3 | Mikrospínač / tlačítko | SPST-NO, normálně rozepnuté | Libovolné tlačítko (DC, <1 mA — bez nároků na proudovou zatížitelnost) |
| 4 | LED | 3/5 mm, **doporučeně červená / žlutá / zelená** | Signalizace (viz §5) |
| 5 | Rezistor k LED | **330 Ω**, ¼ W, ±5 % | Omezení proudu LED (viz §5) |
| 6 | **Pojistka + držák** (doporučeno) | **T 0,5 A**, 250 V, pomalá | Ve fázovém vodiči, před HLK-PM01 |
| 7 | Varistor / MOV (volitelné) | 275 VAC (např. S14K275) | Přepěťová ochrana mezi L–N |
| 8 | Rezistor — pull-up tlačítka (volitelné) | 10 kΩ, ¼ W | Jen pro dlouhé vedení k tlačítku |
| 9 | Kondenzátor — debounce (volitelné) | 100 nF keramický | HW debounce pro dlouhé/rušené vedení |
| 10 | Síťový vodič | ≥ 0,5 mm² (AWG 20), **izolace pro síť** | Pro vedení L/N k HLK-PM01 |
| 11 | Propojovací vodič | 0,25–0,5 mm² | Pro stranu 5 V/3,3 V |
| 12 | Svorkovnice / konektory | Pro síťové napětí | Pro bezpečné, rozebíratelné síťové spoje |
| 13 | **Krabička** | Uzavřená, izolovaná (ideálně samozhášivá) | **Povinné** — žádné odhalené síťové vodiče |

HLK-PM01 dodává 5 V / 600 mA (3 W). Tlačítko odebírá průměrně ~40–80 mA (CPU
podtaktováno na 80 MHz + WiFi modem sleep), s krátkými špičkami při vysílání —
hluboko pod možnostmi modulu.

---

## 2. Použité piny (DC strana)

Použité jsou jen čtyři piny desky. **Řiď se popiskami na potisku** desky, ne
fyzickou polohou — pořadí pinů se mezi revizemi desek liší.

| Pin desky | Směr | Připojení k | Účel |
|-----------|------|-------------|------|
| `5V` (`VIN`) | Napájení vstup | HLK-PM01 **Vo+** | 5 V ze síťového modulu |
| `GND` | — | HLK-PM01 **Vo−**, tlačítko noha B, katoda LED | Společná 0 V (DC strana) |
| `GPIO27` | Vstup (pull-up) | Tlačítko noha A | Čte tlačítko (aktivní v LOW) |
| `GPIO26` | Výstup | Anoda LED přes 330 Ω | Budí signalizační LED (aktivní v HIGH) |

Referenčně (nezapojeno): `3V3` je regulovaný **výstup** 3,3 V z LDO na desce; `EN`
je reset (dvojí stisk do 3 s smaže WiFi — §9).

---

## 3. Celkové schéma

```
   ╔═══════════════ STRANA 230 V AC — NEBEZPEČNÁ ════════════╗
   ║                                                          ║
   ║   Síť L ──[ F1  T0,5A ]───┬───────────── HLK-PM01  "L"   ║
   ║                           │                  (AC vstup)  ║
   ║                         [MOV]  (volitelné, L–N)          ║
   ║                           │                              ║
   ║   Síť N ───────────────────┴───────────── HLK-PM01  "N"  ║
   ║                                                          ║
   ║                              HLK-PM01  Vo+ ─┐   Vo− ─┐   ║
   ╚═════════════════════════════════════════════│════════│══╝
                  izolace (~3 kVAC)               │        │
   ╔═══════════════ STRANA 5 V / 3,3 V DC — SELV ═│════════│══╗
   ║                                              │        │  ║
   ║                              ESP32 DevKitC   │        │  ║
   ║                            ┌─────────────────┴──┐     │  ║
   ║                  5V (VIN) ─┤ 5V                  │     │  ║
   ║                       GND ─┤ GND ────────────────┼─────┘  ║
   ║                            │                     │ (společná 0 V)
   ║              GPIO26 ───────┤ GPIO26              │        ║
   ║                            │    └──[ R1 330Ω ]──►|──┐     ║
   ║                            │                LED D1   │     ║
   ║              GPIO27 ───────┤ GPIO27           (a)(k) │     ║
   ║                            │    └────○ ○───┐         │     ║
   ║                            └──────────────────────────────╜
   ║                                 SW1 (tlač.) │         │   ║
   ║                                             └────GND──┴───╜
   ╚══════════════════════════════════════════════════════════╝

   Legenda:  ──►|──  LED (šipka = anoda → katoda)
             ──○ ○── normálně rozepnutý kontakt tlačítka
             [ F1 ]  pojistka   [ R ] rezistor   [MOV] varistor
```

### Shrnutí domén

- **AC strana:** `L → pojistka F1 → HLK-PM01 "L"`, `N → HLK-PM01 "N"`. Volitelně MOV
  mezi L–N pro omezení přepětí. Na AC stranu **nepatří nic dalšího**.
- **DC strana:** `HLK Vo+ → ESP32 5V`, `HLK Vo− → ESP32 GND` (společná 0 V).
  Tlačítko a LED visí na GPIO úplně stejně jako dřív. Celá tato strana je od sítě
  izolovaná.

---

## 4. Síťový vstup (HLK-PM01)

```
   L ──[ F1  T0,5A 250V ]──┬────────── HLK-PM01  "AC L"
                           │
                         [ MOV 275VAC ]   (volitelné)
                           │
   N ──────────────────────┴────────── HLK-PM01  "AC N"

   HLK-PM01  Vo+ ───────────────────── ESP32  5V
   HLK-PM01  Vo− ───────────────────── ESP32  GND
```

- **Pojistka F1** patří do **fázového (L)** vodiče, *před* HLK-PM01. **T 0,5 A /
  250 V** pomalá je pro zdroj 3 W s rezervou a chrání při poruše modulu. Na trvalé
  síťové instalaci ji nevynechávej.
- **MOV (volitelný)** mezi L–N omezí síťová přepětí; volit cca 275 VAC.
- HLK-PM01 je **izolovaný modul třídy II / s dvojitou izolací** — jeho výstup 5 V je
  plovoucí a izolovaný od sítě, takže na výstupu **není potřeba ochranný vodič
  (PE)**. Pokud má tvoje síť vodič PE, bezpečně ho zakonči (např. na zemnicí
  svorku); s DC 0 V se **nespojuje**.
- Polarita vstupu HLK-PM01 funkčně nehraje roli (je to AC vstup), ale **L a N
  zřetelně označ** a pojistku drž ve fázi kvůli správné ochraně.
- HLK-PM01 je **trvale pod napětím**, dokud je zařízení v zásuvce — nemá vypínač.
  Neexistuje stav vypnuto/standby.

---

## 5. Obvod LED a volba rezistoru (DC strana)

```
   GPIO26 ──[ R1 ]──►|── GND
                     LED
                (anoda)(katoda)
```

Sériový rezistor navrhni podle:

```
        Vgpio − Vf
  R  =  ──────────      Vgpio = 3,3 V (GPIO v HIGH),  Vf = úbytek na LED,
            I           I = cílový proud (5–10 mA bohatě stačí)
```

| Barva LED | Typ. Vf | R pro ~6 mA | Použij | Hodnocení |
|-----------|---------|-------------|--------|-----------|
| Červená | 1,8–2,0 V | ≈ 233 Ω | **220–330 Ω** | Jasná, ideální |
| Žlutá / oranžová | 2,0–2,2 V | ≈ 200 Ω | **220–330 Ω** | Jasná |
| Zelená (běžná) | 2,0–2,2 V | ≈ 200 Ω | **220–330 Ω** | Jasná |
| Modrá / bílá / „čistě zelená" | 2,8–3,4 V | hraniční | 100–150 Ω | Slabá/nespolehlivá — **nepoužívat** |

**Použij 330 Ω s červenou/žlutou/zelenou LED** (~4–5 mA — dobře viditelné, hluboko
pod limitem GPIO ESP32, doporučeno ≤ 20 mA). **Anoda (delší noha, +)** směrem k
rezistoru/`GPIO26`; **katoda (kratší noha, ploška na okraji, −)** na `GND`.
Obráceně zapojená LED prostě nesvítí.

---

## 6. Obvod tlačítka (DC strana)

```
   GPIO27 ─────────────┬───────────○ ○───────── GND
                       │           (SW1, NO)
            (volitelné)│
            R2 10kΩ ───┤  na 3V3   (extra pull-up, většinou zbytečný)
                       │
            (volitelné)│
            C1 100nF ──┴───────────────────────── GND   (debounce)
```

- **Základ:** jen tlačítko mezi `GPIO27` a `GND`. Firmware zapíná interní pull-up
  (~45 kΩ) a v softwaru dělá debounce ~50 ms — žádné externí součástky netřeba.
- **Dlouhé vedení (> ~30 cm) / rušené prostředí:** přidej **C1 (100 nF)** z `GPIO27`
  na `GND` přímo u desky, volitelně **R2 (10 kΩ)** na `3V3`.
- Tlačítko není polarizované — kteroukoli nohu lze dát na `GPIO27` nebo `GND`.

---

## 7. Napájení a programování

- **Běžný provoz:** napájení ze sítě přes HLK-PM01 (§4). Deska naběhne hned po
  zapojení do zásuvky.
- **Programování / práce na stole:** flashuj přes **USB** konektor desky **s
  odpojenou sítí**.

> ☠️ **Nikdy nemít USB a síť připojené současně.** Oba napájejí 5V rail; jejich
> spojení může způsobit zpětné napájení a zničit HLK-PM01, USB hostitele nebo
> desku. Před zapojením USB odpoj síť a naopak.

Uzemnění na DC straně: HLK `Vo−`, ESP32 `GND`, katoda LED a tlačítko sdílejí jeden
společný uzel 0 V. Tato 0 V je izolovaná od sítě — **nespojuj** ji s PE.

---

## 8. Postup montáže

1. **Bez napětí / odpojeno** po celou dobu práce.
2. **Nejdřív DC strana (bezpečná):**
   - LED: zapoj `R1` (330 Ω) sériově s anodou; volný konec → `GPIO26`, katoda →
     `GND`.
   - Tlačítko: jedna noha → `GPIO27`, druhá → `GND` (volitelně `C1`/`R2` dle §6).
   - Výstup HLK: `Vo+` → ESP32 `5V`, `Vo−` → ESP32 `GND`.
3. **AC strana (kvalifikovaná osoba):**
   - Osaď `F1` do fáze; zapoj `L (přes pojistku) → HLK "L"`, `N → HLK "N"`.
   - Volitelně MOV mezi L–N. Použij síťový vodič a svorkovnice.
   - Síťové vodiče drž dál od DC strany — dodrž **vzdušnou a povrchovou vzdálenost
     ≥ 6–8 mm** mezi sítí a SELV (viz §10).
4. **Zakrytuj:** vše smontuj do uzavřené izolované krabičky s odlehčením tahu na
   síťovém kabelu. Žádná odhalená síť. USB pro programování ať je přístupné jen s
   otevřenou krabičkou / odpojenou sítí.
5. **Zkontroluj** dle §3 — polarita LED, pojistka ve fázi, žádných 5 V/síť na GPIO,
   dostatečné vzdálenosti — **před** připojením napětí.
6. **První zapnutí:** odpoj USB, zapoj do sítě. Deska naběhne, vypisuje na sériovou
   linku 115200 (na stole dosažitelnou přes USB) a pokud nenajde známou WiFi,
   otevře AP `SvazarmButton-Setup`.

---

## 9. Ověření

Měření na síťové straně dělej **jen bez napětí**, nebo pod napětím **jen pokud máš
kvalifikaci a používáš správné přístroje**.

| Kontrola | Jak | Očekávání |
|----------|-----|-----------|
| Pojistka ve fázi | Vizuálně / prozvonění, odpojeno | F1 ve fázi, celá |
| Izolace síť↔DC | Měření izolace L/N ↔ Vo−, odpojeno | Velmi vysoký odpor (MΩ+) — potvrzuje izolaci |
| Výstup HLK | DC napětí `Vo+`↔`Vo−`, **síť pod napětím, jen kvalifikovaně** | ~4,7–5,2 V |
| Logická hladina | DC napětí `3V3`↔`GND` | ~3,2–3,3 V |
| Tlačítko puštěné / stisknuté | DC napětí `GPIO27`↔`GND` | ~3,3 V klid / ~0 V stisk |
| Buzení LED | DC napětí `GPIO26`↔`GND` během 10× bliknutí | přepíná 0 V / ~3,3 V |

Na sériové lince (`make monitor`, 115200, USB na stole) bys měl vidět úvodní
hlášku, `CPU frequency: 80 MHz`, výsledek připojení WiFi a po každém přijatém
stisku řádek `POST … -> 204 (OK)` následovaný 10× bliknutím LED.

---

## 10. Bezpečnost a uspořádání sítě

- **Síťovou stranu jen kvalifikované osoby.** Pokud si nejsi jistý, nech to udělat
  elektrikáře.
- **Vzdušná/povrchová vzdálenost:** drž ≥ 6–8 mm mezi jakýmkoli síťovým
  vodičem/ploškou a SELV stranou. Nevoď síť a nízké napětí stejným otvorem;
  odděl je a zajisti.
- **Krabička:** plně uzavřená, izolační (plast) nebo řádně uzemněný kov, ideálně
  samozhášivá. Žádná část síťové strany nesmí být zvenku dotknutelná.
- **Odlehčení tahu** na síťovém kabelu, aby zataháním nevytrhlo vodiče na DC stranu.
- **Pojistka ve fázi (L)**, nikdy v nule (N).
- HLK-PM01 se zahřívá — dopřej mu trochu prostoru/proudění; nezasypávej ho izolací.
- **Vybití a ověření:** po odpojení ber síťovou stranu jako pod napětím, dokud
  neověříš 0 V; vstupní filtrační kondenzátory mohou krátce držet náboj.

---

## 11. Provozní poznámky a upozornění k pinům

- **Aktivní úrovně:** tlačítko je **aktivní v LOW** (stisk = 0 V); LED je **aktivní
  v HIGH** (`GPIO26` v HIGH = svítí). Při obráceném zapojení LED (`GPIO26 → katoda`,
  anoda → `3V3`) je nutné invertovat `ledSet()` v `src.ino`.
- **Vyhni se strapping pinům** pro tlačítko/LED: `GPIO0/2/5/12/15`. `GPIO26`/`GPIO27`
  (zde použité) jsou bezpečné univerzální piny.
- **Pouze vstupní piny** (`GPIO34/35/36/39`) neumí budit LED a nemají interní
  pull-up — zde je nepoužívej.
- **Double-reset:** stiskni tlačítko **EN/RST** na desce dvakrát do 3 s pro smazání
  uložené WiFi a znovuotevření konfiguračního portálu.
- **Konfigurační portál na vyžádání:** podrž **tlačítko** při zapnutí (~3 s, dokud
  LED dvakrát neblikne) pro úpravu Backend URL / Auth tokenu / cooldownu bez
  ztráty WiFi (viz README).
