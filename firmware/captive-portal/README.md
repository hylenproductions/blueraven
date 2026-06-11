# Blue Raven Captive Portal Firmware

Reference ESP32 firmware for Blue Raven certified devices. Handles WiFi
provisioning via a local captive portal and posts sensor readings to any
HTTP endpoint that accepts the Blue Raven payload format.

---

## What it does

**First boot / unconfigured:**
- Creates a WiFi access point named `BlueRaven-[TAG]`
- Serves a setup page at `192.168.4.1` (most OSes pop this automatically)
- Collects WiFi credentials, API endpoint URL, and API key
- Writes config to NVS flash and reboots

**Normal operation:**
- Connects to the configured WiFi network
- Posts a signed JSON reading to the configured endpoint every 30 seconds
- If WiFi drops, restarts and retries up to 3 times before falling back to AP mode

**Factory reset:**
- Hold the BOOT button (GPIO0) for 5 seconds
- Clears all stored configuration and restarts into AP mode
- Works in both AP and connected states

---

## Hardware requirements

- Any ESP32 devkit (ESP32-WROOM, ESP32-S3, etc.)
- Soil moisture sensor (or any analog sensor) on GPIO34
- GPIO34 is input-only and has no internal pull-up, suitable for ADC use

Pin map:

| Pin  | Function          |
|------|-------------------|
| 0    | BOOT button (factory reset, active-low, built-in) |
| 2    | Status LED (built-in on most devkits) |
| 34   | Moisture sensor ADC input |

---

## Flashing

### PlatformIO (recommended)

```bash
cd firmware/captive-portal
pio run --target upload
pio device monitor
```

PlatformIO handles the toolchain automatically. No library installs needed:
all dependencies are bundled with the espressif32 platform package.

### Arduino IDE

Arduino IDE requires the sketch folder name to match the `.ino` filename.
Copy the sketch to a properly-named folder first:

```bash
cp -r firmware/captive-portal ~/Arduino/blueraven-captive-portal
```

Then open `~/Arduino/blueraven-captive-portal/blueraven-captive-portal.ino`.

**Board setup:**
1. File → Preferences → Additional boards URL:
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
2. Tools → Board Manager → search "esp32" → install **esp32 by Espressif Systems**
3. Tools → Board → ESP32 Arduino → **ESP32 Dev Module**
4. Tools → Upload Speed → **921600**

All required libraries (`WiFi`, `DNSServer`, `WebServer`, `Preferences`,
`HTTPClient`) ship with the ESP32 Arduino core. No additional library installs.

---

## Configuration

### Device tag

The device tag is set at compile time via `BR_DEVICE_TAG`. It appears in the
AP network name and in every payload as the `device_id` field.

**PlatformIO**: edit `platformio.ini`:
```ini
build_flags = -DBR_DEVICE_TAG='"B2K9"'
```

**Arduino IDE**: edit the define at the top of the `.ino`:
```cpp
#define BR_DEVICE_TAG "B2K9"
```

**Per-unit unique tags from chip ID:**

If you want every unit to have a unique tag without reflashing, derive it
from the ESP32's hardware ID in `setup()`:

```cpp
// In setup(), before loadConfig():
char tag[9];
uint64_t chipid = ESP.getEfuseMac();
snprintf(tag, sizeof(tag), "%04X%04X", (uint32_t)(chipid >> 32), (uint32_t)chipid);
// Then build DEVICE_ID and AP_SSID as Strings at runtime.
```

### Sensor calibration

The moisture sensor mapping uses two constants in the `.ino`:

```cpp
#define MOISTURE_RAW_DRY  3200  // ADC reading in open air
#define MOISTURE_RAW_WET   800  // ADC reading submerged in water
```

Calibrate by opening the serial monitor at 115200 baud and watching the raw
ADC values printed in each POST log line. Adjust the constants to match your
probe's actual range.

### Post interval

```cpp
#define POST_INTERVAL_MS  30000UL  // 30 seconds
```

### WiFi retries before AP fallback

```cpp
#define WIFI_MAX_RETRIES  3
```

---

## Payload format

```json
{
  "device_id": "BR-A7X3",
  "schema_version": "0.2",
  "timestamp": 12345,
  "readings": {
    "moisture": 42.5,
    "raw_value": 2100
  }
}
```

**Timestamp:** The reference firmware uses `millis()/1000` (seconds since boot)
as a timestamp stand-in. For real Unix timestamps, add NTP sync after WiFi
connects:

```cpp
// After connectWiFi() succeeds:
configTime(0, 0, "pool.ntp.org");
```

Then replace `millis() / 1000UL` in `postReading()` with `(unsigned long)time(nullptr)`.

**Authorization:** If an API key is configured, it is sent as:
```
Authorization: Bearer <api_key>
```

---

## Local API: /br/manifest and /br/latest

While connected to WiFi, the device serves a local HTTP API on its LAN IP
(printed to serial on boot). This satisfies Rule 6 of the protocol spec
(v0.2.0) and certifies the device in both push and serve modes.

**`GET /br/manifest`** returns a machine-readable self-description: device
identity, firmware version, mode, every endpoint with its response schema,
and the captive-portal config fields. The intent: a developer (or an AI
coding agent) fetches the manifest and integrates the device with zero
additional documentation.

**`GET /br/latest`** samples the sensor on demand and returns the reading
as a standard envelope payload.

Try it from any machine on the same network:

```
curl http://<device-ip>/br/manifest
curl http://<device-ip>/br/latest
```

The manifest is also served in AP mode at `http://192.168.4.1/br/manifest`,
so a device can be inspected before it has ever been configured.

---

## HTTPS and certificate validation

`HTTPClient` supports TLS out of the box on ESP32. By default this firmware
does not validate the server certificate, which protects against passive
eavesdropping but not active MITM attacks.

For production hardware, pin the server's CA certificate:

```cpp
#include <WiFiClientSecure.h>

static const char CA_CERT[] PROGMEM = R"(
-----BEGIN CERTIFICATE-----
...your CA cert...
-----END CERTIFICATE-----
)";

// In postReading():
WiFiClientSecure client;
client.setCACert(CA_CERT);
http.begin(client, g_endpoint);
```

See `docs/QUICKSTART.md` for how to extract the certificate from your endpoint.

---

## Adding sensors

`postReading()` builds the payload manually with `snprintf`. To add more
readings, either extend the format string or pull in ArduinoJson:

```cpp
#include <ArduinoJson.h>

// In postReading():
JsonDocument doc;
doc["device_id"]             = DEVICE_ID;
doc["timestamp"]             = millis() / 1000UL;
doc["readings"]["moisture"]  = r.moisture_pct;
doc["readings"]["raw_value"] = r.raw_value;
doc["readings"]["temp_c"]    = readTemperature();

String payload;
serializeJson(doc, payload);
```

---

## Serial output reference

```
[BOOT] Blue Raven BR-A7X3 starting.
[BOOT] WiFi attempt 1/3
[WiFi] Connecting to "MyNetwork"......... OK. IP: 192.168.1.42
[BOOT] Ready. Posting every 30s.
[POST] {"device_id":"BR-A7X3","timestamp":30,"readings":{"moisture":63.2,"raw_value":1240}}
[POST] 200
```

In AP mode:
```
[BOOT] No config found. Entering AP mode.
[AP] Starting. SSID: BlueRaven-A7X3
[AP] Portal at http://192.168.4.1/
[NVS] Config saved.
```

Factory reset:
```
[RESET] BOOT held. Hold 5s to factory reset.
[RESET] Wiping config and restarting.
[NVS] Config cleared.
```
