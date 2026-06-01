#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <vector>
#include "config.h"

// --- Pin definitions (ESP32 DevKitC 38-pin) ---
#define BUTTON_PIN 27   // momentary button between GPIO27 and GND (internal pull-up)
#define LED_PIN    26   // external LED via resistor to GND (active HIGH)

// --- Constants ---
#define CPU_FREQ_MHZ     80     // underclock from 240 to cut heat (WiFi needs >=80)
#define MAX_NETWORKS     20
#define WIFI_TIMEOUT_MS  15000
#define PORTAL_TIMEOUT_S 180
#define HTTP_TIMEOUT_MS  10000
#define DRD_WINDOW_MS    3000
#define DEBOUNCE_MS      50
#define PRESS_LOCKOUT_MS 3000   // ignore further presses for this long after one fires
#define SUCCESS_BLINKS   10     // 10x blink = request accepted
#define SUCCESS_BLINK_MS 150
#define LED_FAST_MS      200

static const char* AP_NAME = "SvazarmButton-Setup";
static const char* PREFS_NS = "wifi";   // WiFi credentials namespace
static const char* CFG_NS = "cfg";      // Backend URL + auth token namespace

// --- Globals ---
WiFiMulti wifiMulti;
Preferences prefs;

// Runtime config — loaded from NVS at boot, defaults from config.h.
String g_apiUrl;
String g_authToken;
unsigned long g_cooldownMs = 0;   // min interval between sends, in ms

// Send throttle state (RAM only — resets on reboot).
unsigned long lastSendMs = 0;     // millis() of last successful send
bool hasSent = false;             // a send has succeeded since boot

// Portal parameter handles — valid only while the config portal is running.
WiFiManagerParameter* g_urlParam = nullptr;
WiFiManagerParameter* g_tokenParam = nullptr;
WiFiManagerParameter* g_cooldownParam = nullptr;

// ============================================================
// LED status indication (external LED, active HIGH)
// ============================================================

void ledSet(bool on) {
  digitalWrite(LED_PIN, on ? HIGH : LOW);
}

// Blocking blink for a fixed duration (used during boot/DRD window).
void ledBlinkFor(unsigned long intervalMs, unsigned long durationMs) {
  unsigned long start = millis();
  while (millis() - start < durationMs) {
    ledSet(true);
    delay(intervalMs / 2);
    ledSet(false);
    delay(intervalMs / 2);
  }
}

// Blink a discrete number of times (success feedback).
void ledBlinkTimes(int times, unsigned long onMs) {
  for (int i = 0; i < times; i++) {
    ledSet(true);
    delay(onMs);
    ledSet(false);
    delay(onMs);
  }
}

// Error feedback: 3 long blinks.
void ledErrorPattern() {
  for (int i = 0; i < 3; i++) {
    ledSet(true);
    delay(600);
    ledSet(false);
    delay(250);
  }
}

// Non-blocking blink (used while connecting to WiFi).
void ledBlinkOnce(unsigned long intervalMs) {
  static unsigned long lastToggle = 0;
  static bool state = false;
  if (millis() - lastToggle >= intervalMs / 2) {
    state = !state;
    ledSet(state);
    lastToggle = millis();
  }
}

// ============================================================
// WiFi credential storage (Preferences / NVS)
// ============================================================

int getStoredCount() {
  prefs.begin(PREFS_NS, true);
  int count = prefs.getInt("count", 0);
  prefs.end();
  return count;
}

void loadCredentials() {
  prefs.begin(PREFS_NS, true);
  int count = prefs.getInt("count", 0);
  for (int i = 0; i < count && i < MAX_NETWORKS; i++) {
    String ssidKey = "ssid" + String(i);
    String passKey = "pass" + String(i);
    String ssid = prefs.getString(ssidKey.c_str(), "");
    String pass = prefs.getString(passKey.c_str(), "");
    if (ssid.length() > 0) {
      wifiMulti.addAP(ssid.c_str(), pass.c_str());
      Serial.printf("  Loaded network %d: %s\n", i, ssid.c_str());
    }
  }
  prefs.end();
}

void saveCredential(const char* ssid, const char* password) {
  prefs.begin(PREFS_NS, false);
  int count = prefs.getInt("count", 0);

  // Update in place if this SSID already exists.
  for (int i = 0; i < count && i < MAX_NETWORKS; i++) {
    String ssidKey = "ssid" + String(i);
    String stored = prefs.getString(ssidKey.c_str(), "");
    if (stored == ssid) {
      String passKey = "pass" + String(i);
      prefs.putString(passKey.c_str(), password);
      Serial.printf("  Updated existing network: %s (slot %d)\n", ssid, i);
      prefs.end();
      return;
    }
  }

  // Otherwise pick the next slot (circular buffer once full).
  int slot = count < MAX_NETWORKS ? count : (count % MAX_NETWORKS);
  String ssidKey = "ssid" + String(slot);
  String passKey = "pass" + String(slot);
  prefs.putString(ssidKey.c_str(), ssid);
  prefs.putString(passKey.c_str(), password);
  prefs.putInt("count", count + 1);

  Serial.printf("  Saved network: %s (slot %d)\n", ssid, slot);
  prefs.end();
}

void clearCredentials() {
  prefs.begin(PREFS_NS, false);
  for (int i = 0; i < MAX_NETWORKS; i++) {
    String ssidKey = "ssid" + String(i);
    String passKey = "pass" + String(i);
    prefs.remove(ssidKey.c_str());
    prefs.remove(passKey.c_str());
  }
  prefs.putInt("count", 0);
  prefs.end();
  Serial.println("  All WiFi credentials cleared");
}

// ============================================================
// Runtime config: Backend URL + Auth token (Preferences / NVS)
// ============================================================

void loadConfig() {
  prefs.begin(CFG_NS, true);
  g_apiUrl = prefs.getString("url", API_URL);          // default from config.h
  g_authToken = prefs.getString("token", AUTH_TOKEN);  // default from config.h
  g_cooldownMs = (unsigned long)prefs.getULong("cooldown", OPEN_COOLDOWN_S) * 1000UL;
  prefs.end();
}

void saveConfig(const char* url, const char* token, unsigned long cooldownS) {
  prefs.begin(CFG_NS, false);
  prefs.putString("url", url);
  prefs.putString("token", token);
  prefs.putULong("cooldown", cooldownS);
  prefs.end();
  Serial.println("  Config (URL/token/cooldown) saved to NVS");
}

// Invoked by WiFiManager when the parameters form is submitted.
void saveParamsCallback() {
  if (g_urlParam) g_apiUrl = g_urlParam->getValue();
  if (g_tokenParam) g_authToken = g_tokenParam->getValue();

  unsigned long cooldownS = g_cooldownMs / 1000UL;
  if (g_cooldownParam) {
    const char* v = g_cooldownParam->getValue();
    if (v && v[0] != '\0') {                 // keep current value if left blank
      cooldownS = strtoul(v, nullptr, 10);
      g_cooldownMs = cooldownS * 1000UL;
    }
  }
  saveConfig(g_apiUrl.c_str(), g_authToken.c_str(), cooldownS);
}

// ============================================================
// Double Reset Detection (reset twice within 3s clears WiFi)
// ============================================================

bool checkDoubleReset() {
  prefs.begin(PREFS_NS, false);
  bool drdFlag = prefs.getBool("drd", false);

  if (drdFlag) {
    prefs.putBool("drd", false);
    prefs.end();
    Serial.println("Double reset detected! Clearing credentials...");
    clearCredentials();
    return true;
  }

  prefs.putBool("drd", true);
  prefs.end();

  Serial.println("Waiting for double-reset window (3s)...");
  ledBlinkFor(LED_FAST_MS, DRD_WINDOW_MS);

  prefs.begin(PREFS_NS, false);
  prefs.putBool("drd", false);
  prefs.end();
  Serial.println("Single reset — proceeding normally");
  return false;
}

// ============================================================
// WiFi connection
// ============================================================

void initDefaultCredentials() {
  if (getStoredCount() > 0) return;
  Serial.println("NVS empty — loading default networks from config.h");
  for (int i = 0; i < DEFAULT_NETWORK_COUNT; i++) {
    saveCredential(DEFAULT_NETWORKS[i].ssid, DEFAULT_NETWORKS[i].password);
  }
}

bool connectWiFi() {
  initDefaultCredentials();
  Serial.println("Loading stored WiFi credentials...");
  loadCredentials();

  if (getStoredCount() == 0) {
    Serial.println("No stored credentials");
    return false;
  }

  Serial.println("Connecting via WiFiMulti...");
  unsigned long start = millis();
  while (millis() - start < WIFI_TIMEOUT_MS) {
    if (wifiMulti.run() == WL_CONNECTED) {
      Serial.printf("Connected to: %s (IP: %s)\n",
                    WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
      return true;
    }
    ledBlinkOnce(LED_FAST_MS);
    delay(100);
  }

  Serial.println("WiFiMulti connection timed out");
  return false;
}

// Opens the WiFiManager config portal with extra fields for Backend URL and
// Auth token. keepWiFi=false (connection failure / double-reset) restarts the
// board if the portal closes without a WiFi connection; keepWiFi=true (button
// held at boot) just returns so the caller can reconnect with existing creds.
void runConfigPortal(bool keepWiFi) {
  Serial.println("Starting config portal...");

  WiFiManager wm;
  wm.setConfigPortalTimeout(PORTAL_TIMEOUT_S);

  char cooldownStr[16];
  snprintf(cooldownStr, sizeof(cooldownStr), "%lu", g_cooldownMs / 1000UL);

  WiFiManagerParameter urlParam("apiurl", "Backend URL", g_apiUrl.c_str(), 200);
  WiFiManagerParameter tokenParam("token", "Auth token (Authorization header)",
                                  g_authToken.c_str(), 200);
  WiFiManagerParameter cooldownParam("cooldown", "Min seconds between sends",
                                     cooldownStr, 12);
  g_urlParam = &urlParam;
  g_tokenParam = &tokenParam;
  g_cooldownParam = &cooldownParam;
  wm.addParameter(&urlParam);
  wm.addParameter(&tokenParam);
  wm.addParameter(&cooldownParam);
  wm.setSaveParamsCallback(saveParamsCallback);

  // WiFi page, dedicated parameters page, info, and an Exit button so the
  // portal can be closed after editing params without changing WiFi.
  std::vector<const char*> menu = {"wifi", "param", "info", "exit"};
  wm.setMenu(menu);

  ledSet(false);
  wm.setAPCallback([](WiFiManager* /*wm*/) {
    Serial.println("Config portal active");
    Serial.printf("Connect to AP: %s, then open http://192.168.4.1\n", AP_NAME);
  });

  bool connected = wm.startConfigPortal(AP_NAME);

  if (connected) {
    // New WiFi credentials were entered — persist them in our multi-WiFi store.
    String ssid = WiFi.SSID();
    String pass = wm.getWiFiPass();
    Serial.printf("Portal connected to: %s\n", ssid.c_str());
    saveCredential(ssid.c_str(), pass.c_str());
  } else if (!keepWiFi) {
    Serial.println("Portal closed without WiFi — restarting...");
    ESP.restart();
  }

  g_urlParam = nullptr;
  g_tokenParam = nullptr;
  g_cooldownParam = nullptr;
}

// Returns true if the button is held LOW for ~3s right after boot — used to
// open the config portal on demand without losing WiFi.
bool buttonHeldAtBoot() {
  if (digitalRead(BUTTON_PIN) != LOW) return false;
  Serial.println("Button down at boot — keep holding 3s for config portal...");
  unsigned long start = millis();
  while (digitalRead(BUTTON_PIN) == LOW) {
    if (millis() - start >= 3000) {
      Serial.println("Config portal requested");
      ledBlinkTimes(2, SUCCESS_BLINK_MS);  // confirm request
      return true;
    }
    delay(10);
  }
  return false;
}

// ============================================================
// Open request (POST to the pub server)
// ============================================================

// Ensures WiFi is up (best effort), then POSTs to g_apiUrl with the auth header.
// Returns true on a 2xx response.
bool sendOpenRequest() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi down — attempting reconnect before POST...");
    unsigned long start = millis();
    while (millis() - start < WIFI_TIMEOUT_MS) {
      if (wifiMulti.run() == WL_CONNECTED) break;
      ledBlinkOnce(LED_FAST_MS);
      delay(100);
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi unavailable — cannot send request");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();  // skip certificate verification

  HTTPClient http;
  http.begin(client, g_apiUrl);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setUserAgent(USER_AGENT);
  http.addHeader("Authorization", g_authToken);
  http.addHeader("Content-Type", "application/json");

  // Body is ignored by the server today — send small debug info anyway.
  JsonDocument doc;
  doc["device"] = "svazarm-button";
  doc["reason"] = "button_press";
  doc["uptime_ms"] = millis();
  String body;
  serializeJson(doc, body);

  int httpCode = http.POST(body);
  http.end();

  bool ok = httpCode >= 200 && httpCode < 300;
  Serial.printf("[%lu] POST %s -> %d (%s)\n", millis(), g_apiUrl.c_str(), httpCode,
                ok ? "OK" : "FAIL");
  return ok;
}

// ============================================================
// Button handling (debounced, falling edge)
// ============================================================

// Returns true once per confirmed press (with lockout to avoid double-fire).
bool buttonPressed() {
  static int lastReading = HIGH;
  static int stableState = HIGH;
  static unsigned long lastChange = 0;
  static unsigned long lockoutUntil = 0;

  int reading = digitalRead(BUTTON_PIN);  // HIGH = released, LOW = pressed
  if (reading != lastReading) {
    lastChange = millis();
    lastReading = reading;
  }

  if (millis() - lastChange >= DEBOUNCE_MS && reading != stableState) {
    stableState = reading;
    if (stableState == LOW && millis() >= lockoutUntil) {
      lockoutUntil = millis() + PRESS_LOCKOUT_MS;
      return true;
    }
  }
  return false;
}

// ============================================================
// Arduino entry points
// ============================================================

void setup() {
  // Underclock the CPU to reduce heat/power. 80 MHz is the floor that still
  // lets the WiFi radio work (anything lower disables WiFi). Set before
  // Serial.begin so the UART divisor is computed against the final clock.
  setCpuFrequencyMhz(CPU_FREQ_MHZ);

  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Svazarm Button starting ===");
  Serial.printf("CPU frequency: %d MHz\n", getCpuFrequencyMhz());

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  ledSet(false);

  // WiFi modem sleep: keep the connection but let the radio nap between
  // beacons. Cuts idle power without affecting button responsiveness.
  WiFi.setSleep(true);

  loadConfig();
  Serial.printf("Backend URL: %s\n", g_apiUrl.c_str());

  // Hold the button during boot to open the config portal and edit the
  // Backend URL / Auth token without wiping WiFi.
  if (buttonHeldAtBoot()) {
    runConfigPortal(true);   // keep WiFi creds; reconnect afterwards
    connectWiFi();
    ledBlinkTimes(1, SUCCESS_BLINK_MS);
    return;
  }

  bool doubleReset = checkDoubleReset();

  if (!doubleReset && connectWiFi()) {
    // Connected — signal readiness with one short blink.
    ledBlinkTimes(1, SUCCESS_BLINK_MS);
    return;
  }

  // Couldn't connect (or double-reset wiped creds) — open the full portal.
  runConfigPortal(false);
}

void loop() {
  // Keep WiFi alive in the background.
  if (WiFi.status() != WL_CONNECTED) {
    wifiMulti.run();
  }

  if (buttonPressed()) {
    if (hasSent && (millis() - lastSendMs) < g_cooldownMs) {
      unsigned long remainingS = (g_cooldownMs - (millis() - lastSendMs)) / 1000UL;
      Serial.printf("[%lu] Button pressed but in cooldown (%lus left) — ignored\n",
                    millis(), remainingS);
      ledBlinkTimes(2, 80);  // quick double blink = ignored / cooling down
    } else {
      Serial.printf("[%lu] Button pressed — sending open request\n", millis());
      ledSet(true);  // solid while the request is in flight
      bool ok = sendOpenRequest();
      ledSet(false);
      if (ok) {
        lastSendMs = millis();   // start cooldown only on success
        hasSent = true;
        ledBlinkTimes(SUCCESS_BLINKS, SUCCESS_BLINK_MS);
      } else {
        ledErrorPattern();
      }
    }
  }

  delay(5);
}
