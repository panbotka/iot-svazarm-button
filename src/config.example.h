#ifndef CONFIG_H
#define CONFIG_H

// Copy this file to config.h and fill in the real values.
// config.h is gitignored — never commit the real AuthToken or WiFi passwords.
//
// NOTE: API_URL and AUTH_TOKEN here are only first-boot DEFAULTS. Once set via
// the config portal they are stored in NVS and take precedence over these.

// --- Server endpoint ---
// Real host is a secret — keep it only in config.h, never commit it.
#define API_URL     "https://YOUR_SERVER_HOST/api/button/svazarm/open"
#define USER_AGENT  "SvazarmButton/1.0"

// Sent verbatim in the "Authorization" header. The server compares it for an
// exact match (no "Bearer " prefix). Get the real token from the keg-scale
// backend config (AuthToken).
#define AUTH_TOKEN  "REPLACE_WITH_REAL_TOKEN"

// Minimum seconds between sent requests (throttle). Default 4 hours. 0 = no throttle.
#define OPEN_COOLDOWN_S 14400

// --- Default WiFi networks (loaded on first boot when NVS is empty) ---
struct WiFiCredential {
  const char* ssid;
  const char* password;
};

static const WiFiCredential DEFAULT_NETWORKS[] = {
  {"YourSSID", "YourPassword"},
};

static const int DEFAULT_NETWORK_COUNT = sizeof(DEFAULT_NETWORKS) / sizeof(DEFAULT_NETWORKS[0]);

#endif
