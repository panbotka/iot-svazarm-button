#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "config.h"

// --- Pin definitions (ESP32 DevKitC 38-pin) ---
#define BUTTON_PIN 27   // momentary button between GPIO27 and GND (internal pull-up)
#define LED_PIN    26   // external LED via resistor to GND (active HIGH)

// --- Constants ---
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
static const char* PREFS_NS = "wifi";

// --- Globals ---
WiFiMulti wifiMulti;
Preferences prefs;

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

void startCaptivePortal() {
  Serial.println("Starting captive portal...");

  WiFiManager wm;
  wm.setConfigPortalTimeout(PORTAL_TIMEOUT_S);
  ledSet(false);

  wm.setAPCallback([](WiFiManager* /*wm*/) {
    Serial.println("Captive portal active");
    Serial.printf("Connect to AP: %s\n", AP_NAME);
  });

  bool connected = wm.startConfigPortal(AP_NAME);

  if (connected) {
    String ssid = WiFi.SSID();
    String pass = wm.getWiFiPass();
    Serial.printf("Portal connected to: %s\n", ssid.c_str());
    saveCredential(ssid.c_str(), pass.c_str());
  } else {
    Serial.println("Portal timed out — restarting...");
    ESP.restart();
  }
}

// ============================================================
// Open request (POST to the pub server)
// ============================================================

// Ensures WiFi is up (best effort), then POSTs to API_URL with the auth header.
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
  http.begin(client, API_URL);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setUserAgent(USER_AGENT);
  http.addHeader("Authorization", AUTH_TOKEN);
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
  Serial.printf("[%lu] POST %s -> %d (%s)\n", millis(), API_URL, httpCode,
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
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Svazarm Button starting ===");

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  ledSet(false);

  bool doubleReset = checkDoubleReset();

  if (!doubleReset && connectWiFi()) {
    // Connected — signal readiness with one short blink.
    ledBlinkTimes(1, SUCCESS_BLINK_MS);
    return;
  }

  startCaptivePortal();
}

void loop() {
  // Keep WiFi alive in the background.
  if (WiFi.status() != WL_CONNECTED) {
    wifiMulti.run();
  }

  if (buttonPressed()) {
    Serial.printf("[%lu] Button pressed — sending open request\n", millis());
    ledSet(true);  // solid while the request is in flight
    bool ok = sendOpenRequest();
    ledSet(false);
    if (ok) {
      ledBlinkTimes(SUCCESS_BLINKS, SUCCESS_BLINK_MS);
    } else {
      ledErrorPattern();
    }
  }

  delay(5);
}
