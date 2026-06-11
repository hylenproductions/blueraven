# Blue Raven Protocol Specification

Version: `v0.2.0` (draft)

> This document is a work in progress. The wire format is not yet stable.

---

## Device Modes

A certified device operates in one or both of the following modes. Certification applies per mode: a device may be certified in push mode, serve mode, or both.

### Push mode

The device initiates transmission. It wakes, takes its readings, and POSTs an envelope payload to the owner-configured endpoint, then returns to sleep or idle. This is the original v0.1 model and fits battery-powered, deep-sleep sensors.

### Serve mode

The device initiates no outbound connections. It hosts a local HTTP API on the owner's network and responds to queries whenever the owner's systems ask. A serve-mode device must expose, at minimum:

- `GET /br/latest` returns the most recent reading as a standard envelope payload
- `GET /br/manifest` returns the device manifest (see Device Manifest below)

Serve mode is the strictest expression of the protocol's philosophy: a device that never initiates a connection cannot phone home.

A device certified in both modes serves its local API and also pushes to a configured endpoint.

---

## Envelope

Every payload, whether pushed to an endpoint or returned from `/br/latest`, uses the envelope format:

```json
{
  "device_id": "BR-A7X3",
  "schema_version": "0.2",
  "timestamp": 1234567890,
  "readings": {
    "moisture": 42.5,
    "raw_value": 2100
  }
}
```

- `device_id` (string): stable unique identifier, see Rule 1.
- `schema_version` (string): the envelope schema version the device implements. Added in v0.2.0.
- `timestamp` (number): Unix timestamp in seconds, resolved via NTP where available.
- `readings` (object): sensor-specific key/value pairs.

---

## Device Manifest

Every certified device, regardless of mode, must expose `GET /br/manifest` returning a JSON document that is both human and machine readable:

```json
{
  "device_id": "BR-A7X3",
  "device_type": "soil-sensor",
  "firmware_version": "1.4.0",
  "firmware_source_url": "https://github.com/hylenproductions/blueraven",
  "protocol_version": "0.2.0",
  "mode": "push",
  "capabilities": [
    {
      "path": "/br/latest",
      "method": "GET",
      "parameters": [],
      "response": {
        "schema": "envelope",
        "readings": { "moisture": "percent", "raw_value": "adc_counts" }
      }
    }
  ],
  "config": [
    { "name": "capture_start_hour", "type": "int" },
    { "name": "capture_end_hour", "type": "int" },
    { "name": "report_interval_minutes", "type": "int" }
  ],
  "push_target_configurable": true,
  "envelope_schema_version": "0.2"
}
```

Field requirements:

- `device_id`, `device_type`, `firmware_version`, `firmware_source_url`, `protocol_version`, and `mode` (`"push"`, `"serve"`, or `"both"`) are required for all devices.
- `capabilities` is an array describing every endpoint the device exposes: path, method, parameters (name, type, description), response schema, and units where applicable.
- `config` lists the NVS-backed fields editable via the captive portal (Rule 5), names and types only. Secret values (WiFi passwords, API keys) must never appear in the manifest.
- Push-mode devices must include `push_target_configurable: true` and the envelope schema version they emit.
- Devices using the telemetry carve-out (Rule 7) must list their telemetry destinations under a `telemetry` key.

Design intent: a developer or an AI coding agent should be able to fetch the manifest and integrate the device with zero additional documentation. The manifest is the machine-readable sibling of the captive portal.

---

## Device Certification Requirements

All Blue Raven certified devices must satisfy the following rules. A device that violates any rule is not eligible to carry the Blue Raven badge.

### Rule 1: Unique device identity

Every device must transmit a unique `device_id` on every request. The ID must be stable across reboots and deep-sleep wake cycles (i.e., stored in NVS or hardware, not regenerated on boot).

### Rule 2: Authenticated requests

Every outbound API request must include a valid `api_key` header. Keys are issued per-device and must not be shared across devices or embedded in client-side code. Serve-mode devices that make no outbound requests are exempt; authentication of inbound local queries is the owner's choice.

### Rule 3: Local time awareness

Devices that gate behavior on time of day (capture windows, reporting schedules, etc.) must resolve wall-clock time via NTP before making time-based decisions. UTC offsets must be applied correctly using a POSIX TZ string or equivalent. A device may not compare UTC hours against a local-time threshold.

### Rule 4: Graceful degradation

A device that fails to connect to WiFi or NTP must not loop indefinitely. It must enter deep sleep for a defined retry interval and re-attempt on the next wake. The retry interval must be documented in firmware.

### Rule 5: Captive portal with NVS-backed operational config

Every certified device must expose a captive portal setup page (accessible when no WiFi credentials are stored, or on factory reset) that allows configuration of all device-specific operational parameters. This includes, at minimum:

- Any time-window boundaries that gate device behavior (e.g., `capture_start_hour`, `capture_end_hour`)
- Any interval or frequency settings (e.g., `capture_interval_hours`, `report_interval_minutes`)
- Any device-specific thresholds (e.g., moisture alert levels, temperature bounds)

Requirements:

- Parameters must be exposed as editable form fields in the captive portal HTML page, not buried in firmware constants.
- All parameters must persist across reboots via `Preferences` (Arduino NVS) or equivalent non-volatile storage.
- The device must read these values from NVS on every wake cycle. Hardcoded constants used as defaults are acceptable, but must not override stored values.
- The portal must pre-populate each field with the current stored value so the user can see and adjust the active configuration without resetting to defaults.

This requirement exists so that operational behavior can be adjusted in the field without reflashing firmware.

### Rule 6: Device manifest

Every certified device must expose `GET /br/manifest` as specified in the Device Manifest section. The manifest must accurately describe every endpoint the device exposes and every captive-portal config field it honors. A manifest that omits or misrepresents device capabilities fails certification.

### Rule 7: No phoning home

A device must not initiate connections to any endpoint other than the owner-configured target(s). No analytics, no registration pings, no silent telemetry.

One carve-out is permitted. A device MAY perform update checks or error reports to a manufacturer domain only if all of the following hold:

1. The feature is disabled by default.
2. It can be enabled and disabled via the captive portal config (Rule 5).
3. Every destination URL is listed in the device manifest under a `telemetry` key.
4. No sensor data is ever included in these requests.

Any hardcoded, non-disableable outbound call to a non-owner endpoint fails certification.

---

*Full wire format specification (signing, retries, batching) coming soon.*
