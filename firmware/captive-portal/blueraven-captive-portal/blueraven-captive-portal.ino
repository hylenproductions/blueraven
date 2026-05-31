/*
 * Blue Raven Captive Portal Firmware
 * Reference implementation for ESP32
 *
 * On first boot (or after factory reset), the device creates a WiFi access
 * point and serves a configuration page at 192.168.4.1. The page lists
 * nearby networks from a scan that runs before the AP starts, so the user
 * selects their network from a dropdown rather than typing an SSID. A Rescan
 * button triggers a fresh scan without leaving the page.
 *
 * Once credentials are saved, the device connects to the configured network
 * and posts sensor readings to the configured endpoint on a fixed interval.
 *
 * Holding the BOOT button (GPIO0) for 5 seconds clears all stored
 * configuration and restarts into AP mode.
 *
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/blueraven/blueraven
 */

#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HTTPClient.h>

// ─── Device identity ──────────────────────────────────────────────────────────
//
// BR_DEVICE_TAG is the short identifier that appears in the AP name and in
// every payload. Change this per device family at compile time; for unique
// per-unit tags, derive it from the chip ID in setup() instead.

#ifndef BR_DEVICE_TAG
  #define BR_DEVICE_TAG "A7X3"
#endif

#define DEVICE_ID  "BR-" BR_DEVICE_TAG
#define AP_SSID    "BlueRaven-" BR_DEVICE_TAG

// ─── Hardware ─────────────────────────────────────────────────────────────────

#define PIN_BOOT_BUTTON  0   // GPIO0 — built-in BOOT button, active-low
#define PIN_STATUS_LED   2   // GPIO2 — built-in LED on most ESP32 devkits
#define PIN_MOISTURE     34  // GPIO34 — ADC1_CH6, input-only, no pull-up

// ─── Timing ───────────────────────────────────────────────────────────────────

#define WIFI_CONNECT_TIMEOUT_MS  15000UL  // per attempt
#define WIFI_MAX_RETRIES         3
#define POST_INTERVAL_MS         30000UL  // how often to post a reading
#define FACTORY_RESET_HOLD_MS    5000UL   // hold BOOT this long to wipe config

// ─── WiFi scan ────────────────────────────────────────────────────────────────

#define MAX_NETWORKS  20  // scan results to store; more than this are discarded

// ─── Captive portal ───────────────────────────────────────────────────────────

#define DNS_PORT  53
static const IPAddress AP_IP(192, 168, 4, 1);
static const IPAddress AP_MASK(255, 255, 255, 0);

// ─── NVS ──────────────────────────────────────────────────────────────────────

#define NVS_NAMESPACE  "blueraven"
#define NVS_KEY_SSID   "ssid"
#define NVS_KEY_PASS   "password"
#define NVS_KEY_URL    "endpoint"
#define NVS_KEY_APIKEY "api_key"

// ─── Soil moisture calibration ────────────────────────────────────────────────
//
// Resistive probes read HIGH when dry and LOW when wet. Calibrate these
// against your specific probe by reading the ADC in open air (dry) and
// submerged in water (wet).

#define MOISTURE_RAW_DRY   3200  // ADC value in open air
#define MOISTURE_RAW_WET    800  // ADC value submerged in water

// ─── State ────────────────────────────────────────────────────────────────────

enum class Mode { AP, Connected };

struct BRNetwork {
  String  ssid;
  int32_t rssi;
};

static Mode          g_mode          = Mode::AP;
static Preferences   g_prefs;
static DNSServer     g_dns;
static WebServer     g_server(80);

static String        g_ssid;
static String        g_password;
static String        g_endpoint;
static String        g_api_key;

static BRNetwork     g_networks[MAX_NETWORKS];
static int           g_network_count = 0;

static unsigned long g_last_post_ms        = 0;
static unsigned long g_boot_btn_pressed_ms = 0;
static bool          g_boot_btn_held       = false;

// ─── Sensor ───────────────────────────────────────────────────────────────────

struct SensorReading {
  float moisture_pct;
  int   raw_value;
};

/*
 * Read the moisture sensor and map it to a 0–100% range.
 *
 * map() does integer arithmetic, so we scale by 10 and divide to get one
 * decimal of resolution without floating-point in the intermediate step.
 */
static SensorReading readMoisture() {
  int raw = analogRead(PIN_MOISTURE);
  long scaled = map((long)raw, MOISTURE_RAW_DRY, MOISTURE_RAW_WET, 0L, 1000L);
  float pct = constrain((float)scaled / 10.0f, 0.0f, 100.0f);
  return { pct, raw };
}

// ─── NVS helpers ─────────────────────────────────────────────────────────────

/*
 * Returns true if a minimum viable config (SSID + endpoint) is present.
 */
static bool loadConfig() {
  g_prefs.begin(NVS_NAMESPACE, /*readOnly=*/true);
  g_ssid     = g_prefs.getString(NVS_KEY_SSID,   "");
  g_password = g_prefs.getString(NVS_KEY_PASS,   "");
  g_endpoint = g_prefs.getString(NVS_KEY_URL,     "");
  g_api_key  = g_prefs.getString(NVS_KEY_APIKEY, "");
  g_prefs.end();

  return g_ssid.length() > 0 && g_endpoint.length() > 0;
}

static void saveConfig(const String& ssid, const String& pass,
                       const String& url,  const String& key) {
  g_prefs.begin(NVS_NAMESPACE, /*readOnly=*/false);
  g_prefs.putString(NVS_KEY_SSID,   ssid);
  g_prefs.putString(NVS_KEY_PASS,   pass);
  g_prefs.putString(NVS_KEY_URL,    url);
  g_prefs.putString(NVS_KEY_APIKEY, key);
  g_prefs.end();
  Serial.println("[NVS] Config saved.");
}

static void clearConfig() {
  g_prefs.begin(NVS_NAMESPACE, /*readOnly=*/false);
  g_prefs.clear();
  g_prefs.end();
  Serial.println("[NVS] Config cleared.");
}

// ─── WiFi scan ────────────────────────────────────────────────────────────────

/*
 * Scan for nearby networks and store up to MAX_NETWORKS results sorted by
 * signal strength (strongest first). Hidden networks (empty SSID) are skipped.
 *
 * The scan runs synchronously and blocks for ~2–3 seconds. The AP stays up
 * during the scan — the ESP32 time-multiplexes the radio — but connected
 * clients will see a brief interruption. Acceptable for a setup flow.
 *
 * Requires WIFI_AP_STA mode; WIFI_AP alone cannot scan.
 */
static void runScan() {
  Serial.println("[SCAN] Scanning...");
  int n = WiFi.scanNetworks(/*async=*/false);
  g_network_count = 0;

  if (n == WIFI_SCAN_FAILED || n == WIFI_SCAN_RUNNING) {
    Serial.printf("[SCAN] Error: %d\n", n);
    WiFi.scanDelete();
    return;
  }

  for (int i = 0; i < n && g_network_count < MAX_NETWORKS; i++) {
    if (WiFi.SSID(i).length() == 0) continue; // skip hidden networks
    g_networks[g_network_count].ssid = WiFi.SSID(i);
    g_networks[g_network_count].rssi = WiFi.RSSI(i);
    g_network_count++;
  }

  WiFi.scanDelete(); // free the scan memory from the SDK

  // Insertion sort descending by RSSI. n is small (≤ MAX_NETWORKS).
  for (int i = 1; i < g_network_count; i++) {
    BRNetwork tmp = g_networks[i];
    int j = i - 1;
    while (j >= 0 && g_networks[j].rssi < tmp.rssi) {
      g_networks[j + 1] = g_networks[j];
      j--;
    }
    g_networks[j + 1] = tmp;
  }

  Serial.printf("[SCAN] %d network(s) found.\n", g_network_count);
}

// ─── HTML helpers ─────────────────────────────────────────────────────────────

/*
 * Encode special HTML characters so that SSIDs with apostrophes, quotes, or
 * angle brackets don't break attribute values or inject markup.
 */
static String htmlEncode(const String& s) {
  String out;
  out.reserve(s.length() + 8);
  for (unsigned int i = 0; i < s.length(); i++) {
    switch (s[i]) {
      case '&':  out += F("&amp;");  break;
      case '<':  out += F("&lt;");   break;
      case '>':  out += F("&gt;");   break;
      case '"':  out += F("&quot;"); break;
      case '\'': out += F("&#39;");  break;
      default:   out += s[i];        break;
    }
  }
  return out;
}

// ─── Captive portal HTML ──────────────────────────────────────────────────────
//
// Self-contained pages — no external resources. The device has no internet
// access while in AP mode. PROGMEM keeps these off the heap.
//
// The setup page is split at the <select> element so that handleSetup() can
// stream the head, inject dynamic <option> tags from scan results, then stream
// the tail — without building the full page in a heap-allocated String.

static const char HTML_SETUP_HEAD[] PROGMEM =
  "<!DOCTYPE html>"
  "<html lang='en'>"
  "<head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Blue Raven Setup</title>"
    "<style>"
      "*{box-sizing:border-box;margin:0;padding:0}"
      "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;"
           "background:#0f0f0f;color:#e0e0e0;min-height:100vh;"
           "display:flex;align-items:center;justify-content:center}"
      ".card{background:#1a1a1a;border:1px solid #2a2a2a;border-radius:8px;"
             "padding:2rem;width:100%;max-width:440px;margin:1rem}"
      "h1{font-size:1.2rem;font-weight:600;margin-bottom:.2rem}"
      ".did{font-size:.8rem;color:#555;font-family:monospace;margin-bottom:1.75rem}"
      "label{display:block;font-size:.75rem;color:#777;text-transform:uppercase;"
             "letter-spacing:.05em;margin-bottom:.35rem}"
      "select{display:block;width:100%;background:#111;border:1px solid #333;"
              "border-radius:4px;padding:.6rem .75rem;color:#e0e0e0;"
              "font-size:.95rem;margin-bottom:.5rem;outline:none}"
      "select:focus{border-color:#4a7aff}"
      "input{display:block;width:100%;background:#111;border:1px solid #333;"
             "border-radius:4px;padding:.6rem .75rem;color:#e0e0e0;"
             "font-size:.95rem;margin-bottom:1.2rem;outline:none}"
      "input:focus{border-color:#4a7aff}"
      ".btn-rescan{display:block;font-size:.78rem;color:#555;"
                  "text-decoration:none;padding:.15rem 0;margin-bottom:1.2rem}"
      ".btn-rescan:hover{color:#4a7aff}"
      "hr{border:none;border-top:1px solid #2a2a2a;margin:1.4rem 0}"
      "button{width:100%;background:#4a7aff;color:#fff;border:none;"
              "border-radius:4px;padding:.75rem;font-size:1rem;"
              "cursor:pointer;font-weight:500}"
      "button:hover{background:#3a6aef}"
      ".note{font-size:.75rem;color:#444;margin-top:1rem;text-align:center}"
    "</style>"
  "</head>"
  "<body><div class='card'>"
    "<h1>Blue Raven Setup</h1>"
    "<p class='did'>Device: " DEVICE_ID "</p>"
    "<form method='POST' action='/save'>"
      "<label>WiFi Network</label>"
      "<select name='ssid' required>";

// Scan results (<option> tags) are injected here by handleSetup().

static const char HTML_SETUP_TAIL[] PROGMEM =
      "</select>"
      "<a href='/rescan' class='btn-rescan'>&#8635; Rescan networks</a>"
      "<label>WiFi Password</label>"
      "<input type='password' name='password' placeholder='Leave blank for open networks' autocomplete='off'>"
      "<hr>"
      "<label>API Endpoint</label>"
      "<input type='url' name='endpoint' placeholder='https://your-api.example.com/readings' required>"
      "<label>API Key</label>"
      "<input type='text' name='api_key' placeholder='Leave blank if not required'>"
      "<button type='submit'>Save &amp; Connect</button>"
    "</form>"
    "<p class='note'>The device will reboot and connect to your network.</p>"
  "</div></body></html>";

static const char HTML_SAVED[] PROGMEM =
  "<!DOCTYPE html>"
  "<html lang='en'>"
  "<head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Saved — Blue Raven</title>"
    "<style>"
      "*{box-sizing:border-box;margin:0;padding:0}"
      "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;"
           "background:#0f0f0f;color:#e0e0e0;min-height:100vh;"
           "display:flex;align-items:center;justify-content:center}"
      ".card{background:#1a1a1a;border:1px solid #2a2a2a;border-radius:8px;"
             "padding:2rem;width:100%;max-width:440px;margin:1rem;text-align:center}"
      "h1{font-size:1.2rem;margin-bottom:.75rem}"
      "p{color:#666;font-size:.9rem;line-height:1.6}"
    "</style>"
  "</head>"
  "<body><div class='card'>"
    "<h1>Configuration saved.</h1>"
    "<p>The device is rebooting and will connect to your network.<br>"
       "You can close this page.</p>"
  "</div></body></html>";

// ─── Web server handlers ──────────────────────────────────────────────────────

/*
 * Stream the setup page in three parts: static head (PROGMEM), dynamic
 * <option> elements from g_networks[], static tail (PROGMEM). Chunked
 * transfer avoids allocating the full page as a heap String.
 */
static void handleSetup() {
  g_server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  g_server.send(200, "text/html", "");
  g_server.sendContent_P(HTML_SETUP_HEAD);

  if (g_network_count == 0) {
    g_server.sendContent(
      F("<option value='' disabled selected>No networks found — try rescanning</option>")
    );
  } else {
    for (int i = 0; i < g_network_count; i++) {
      String enc = htmlEncode(g_networks[i].ssid);
      String opt = "<option value='" + enc + "'>"
                   + enc + " (" + String(g_networks[i].rssi) + " dBm)"
                   + "</option>";
      g_server.sendContent(opt);
    }
  }

  g_server.sendContent_P(HTML_SETUP_TAIL);
}

/*
 * Redirect everything to the setup page. This catches captive portal probes
 * from all major operating systems and unknown paths.
 */
static void handleRedirect() {
  g_server.sendHeader("Location", "http://192.168.4.1/", /*first=*/true);
  g_server.send(302, "text/plain", "");
}

static void handleSave() {
  String ssid     = g_server.arg("ssid");
  String password = g_server.arg("password");
  String endpoint = g_server.arg("endpoint");
  String api_key  = g_server.arg("api_key");

  if (ssid.isEmpty() || endpoint.isEmpty()) {
    g_server.send(400, "text/plain", "SSID and endpoint are required.");
    return;
  }

  saveConfig(ssid, password, endpoint, api_key);

  g_server.send_P(200, "text/html", HTML_SAVED);
  delay(1500);
  ESP.restart();
}

/*
 * Run a fresh scan and redirect back to the setup page. The scan blocks for
 * ~2–3 seconds before the 302 is sent; the browser waits and then reloads
 * the page with updated results. Any values the user had typed in the form
 * are lost on redirect — this is expected behavior for a hardware setup flow.
 */
static void handleRescan() {
  runScan();
  g_server.sendHeader("Location", "http://192.168.4.1/", /*first=*/true);
  g_server.send(302, "text/plain", "");
}

// ─── AP mode ─────────────────────────────────────────────────────────────────

/*
 * Start the access point and captive portal. Uses WIFI_AP_STA (not WIFI_AP)
 * so that WiFi.scanNetworks() can run while the AP is up — the STA interface
 * must be active for scanning even if it's not connected to anything.
 *
 * The initial scan runs before softAP() so results are ready the moment
 * the first client connects.
 */
static void startAPMode() {
  g_mode = Mode::AP;
  Serial.println("[AP] Starting — SSID: " AP_SSID);

  // AP_STA: AP interface for the portal, STA interface for scanning.
  WiFi.mode(WIFI_AP_STA);
  runScan();

  WiFi.softAPConfig(AP_IP, AP_IP, AP_MASK);
  WiFi.softAP(AP_SSID);

  // Redirect all DNS queries to ourselves. This is what triggers the
  // "sign in to network" popup on iOS, Android, and Windows.
  g_dns.start(DNS_PORT, "*", AP_IP);

  g_server.on("/",        HTTP_GET,  handleSetup);
  g_server.on("/save",    HTTP_POST, handleSave);
  g_server.on("/rescan",  HTTP_GET,  handleRescan);

  // Captive portal detection endpoints. Each OS probes a different URL;
  // we respond in a way that causes each to pop up the portal.
  //
  // iOS / macOS
  g_server.on("/hotspot-detect.html",       HTTP_GET, handleRedirect);
  g_server.on("/library/test/success.html", HTTP_GET, handleRedirect);
  g_server.on("/bag/plain.html",            HTTP_GET, handleRedirect);
  // Android
  g_server.on("/generate_204",              HTTP_GET, handleRedirect);
  g_server.on("/gen_204",                   HTTP_GET, handleRedirect);
  // Windows / Microsoft
  g_server.on("/ncsi.txt",                  HTTP_GET, handleRedirect);
  g_server.on("/connecttest.txt",           HTTP_GET, handleRedirect);
  g_server.on("/redirect",                  HTTP_GET, handleRedirect);
  // Catch-all
  g_server.onNotFound(handleRedirect);

  g_server.begin();
  Serial.printf("[AP] Portal at http://%s/\n", AP_IP.toString().c_str());
}

// ─── WiFi connection ─────────────────────────────────────────────────────────

static bool connectWiFi() {
  Serial.printf("[WiFi] Connecting to \"%s\"", g_ssid.c_str());

  WiFi.mode(WIFI_STA);
  WiFi.begin(g_ssid.c_str(), g_password.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start >= WIFI_CONNECT_TIMEOUT_MS) {
      Serial.println(" timed out.");
      return false;
    }
    delay(500);
    Serial.print(".");
  }

  Serial.printf(" OK. IP: %s\n", WiFi.localIP().toString().c_str());
  return true;
}

// ─── Payload and POST ─────────────────────────────────────────────────────────

/*
 * Build the Blue Raven standard payload and POST it to the configured endpoint.
 *
 * Payload format:
 *   {
 *     "device_id":  "BR-A7X3",
 *     "timestamp":  1234567890,
 *     "readings": {
 *       "moisture":  42.5,
 *       "raw_value": 2100
 *     }
 *   }
 *
 * Timestamp note: this uses millis()/1000 (seconds since boot) as a stand-in.
 * For real Unix timestamps, call configTime() after WiFi connects and use
 * time(nullptr). See the NTP section in docs/QUICKSTART.md.
 *
 * JSON is built by hand here to avoid a library dependency on ArduinoJson
 * for a payload this simple. If your device has additional sensor channels,
 * use ArduinoJson to avoid manual escaping bugs.
 *
 * HTTPS: HTTPClient follows redirects and supports TLS, but certificate
 * validation requires installing the server's CA cert. For production
 * hardware, use WiFiClientSecure with a pinned certificate rather than
 * http.setInsecure(). See docs/QUICKSTART.md for the tradeoffs.
 */
static void postReading() {
  SensorReading r = readMoisture();

  char payload[256];
  snprintf(payload, sizeof(payload),
    "{"
      "\"device_id\":\"" DEVICE_ID "\","
      "\"timestamp\":%lu,"
      "\"readings\":{"
        "\"moisture\":%.1f,"
        "\"raw_value\":%d"
      "}"
    "}",
    millis() / 1000UL,
    r.moisture_pct,
    r.raw_value
  );

  Serial.printf("[POST] %s\n", payload);

  HTTPClient http;
  http.begin(g_endpoint);
  http.addHeader("Content-Type", "application/json");
  if (g_api_key.length() > 0) {
    http.addHeader("Authorization", "Bearer " + g_api_key);
  }

  int status = http.POST(payload);
  if (status > 0) {
    Serial.printf("[POST] %d\n", status);
  } else {
    Serial.printf("[POST] failed: %s\n", HTTPClient::errorToString(status).c_str());
  }
  http.end();
}

// ─── Factory reset ────────────────────────────────────────────────────────────

/*
 * Non-blocking. Call every loop iteration. If BOOT (GPIO0, active-low) is
 * held for FACTORY_RESET_HOLD_MS, wipe NVS and restart.
 *
 * The button is checked in both AP and connected modes so a stuck device
 * can always be recovered without a serial connection.
 */
static void checkFactoryReset() {
  if (digitalRead(PIN_BOOT_BUTTON) == LOW) {
    if (!g_boot_btn_held) {
      g_boot_btn_held        = true;
      g_boot_btn_pressed_ms  = millis();
      Serial.println("[RESET] BOOT held — hold 5s to factory reset.");
    } else if (millis() - g_boot_btn_pressed_ms >= FACTORY_RESET_HOLD_MS) {
      Serial.println("[RESET] Wiping config and restarting.");
      clearConfig();
      delay(200);
      ESP.restart();
    }
  } else {
    g_boot_btn_held = false;
  }
}

// ─── Setup ────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[BOOT] Blue Raven " DEVICE_ID " starting.");

  pinMode(PIN_BOOT_BUTTON, INPUT_PULLUP);
  pinMode(PIN_STATUS_LED,  OUTPUT);
  digitalWrite(PIN_STATUS_LED, LOW);

  bool configured = loadConfig();

  if (!configured) {
    Serial.println("[BOOT] No config found — entering AP mode.");
    startAPMode();
    return;
  }

  // Attempt connection, fall back to AP mode after WIFI_MAX_RETRIES failures.
  bool connected = false;
  for (int attempt = 1; attempt <= WIFI_MAX_RETRIES && !connected; attempt++) {
    Serial.printf("[BOOT] WiFi attempt %d/%d\n", attempt, WIFI_MAX_RETRIES);
    connected = connectWiFi();
  }

  if (!connected) {
    Serial.println("[BOOT] WiFi failed — entering AP mode for reconfiguration.");
    startAPMode();
    return;
  }

  g_mode = Mode::Connected;
  digitalWrite(PIN_STATUS_LED, HIGH); // solid = connected
  Serial.println("[BOOT] Ready. Posting every " + String(POST_INTERVAL_MS / 1000) + "s.");
}

// ─── Loop ─────────────────────────────────────────────────────────────────────

void loop() {
  checkFactoryReset();

  if (g_mode == Mode::AP) {
    g_dns.processNextRequest();
    g_server.handleClient();
    return;
  }

  // Connected mode.
  if (WiFi.status() != WL_CONNECTED) {
    // The ESP32 SDK handles background reconnection. If it's still down after
    // a full interval, the next postReading() call will log the error.
    // After a hard disconnect (reboot, power loss), restart to retrigger the
    // connection logic with retry counting.
    Serial.println("[LOOP] WiFi lost. Restarting.");
    delay(1000);
    ESP.restart();
  }

  unsigned long now = millis();
  if (now - g_last_post_ms >= POST_INTERVAL_MS) {
    g_last_post_ms = now;
    postReading();
  }
}
