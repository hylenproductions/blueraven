# Blue Raven Protocol Specification

Version: `v0.1` (draft)

> This document is a work in progress. The wire format is not yet stable.

---

## Device Certification Requirements

All Blue Raven certified devices must satisfy the following rules. A device that violates any rule is not eligible to carry the Blue Raven badge.

### Rule 1 — Unique device identity

Every device must transmit a unique `device_id` on every request. The ID must be stable across reboots and deep-sleep wake cycles (i.e., stored in NVS or hardware, not regenerated on boot).

### Rule 2 — Authenticated requests

Every outbound API request must include a valid `api_key` header. Keys are issued per-device and must not be shared across devices or embedded in client-side code.

### Rule 3 — Local time awareness

Devices that gate behavior on time of day (capture windows, reporting schedules, etc.) must resolve wall-clock time via NTP before making time-based decisions. UTC offsets must be applied correctly using a POSIX TZ string or equivalent — a device may not compare UTC hours against a local-time threshold.

### Rule 4 — Graceful degradation

A device that fails to connect to WiFi or NTP must not loop indefinitely. It must enter deep sleep for a defined retry interval and re-attempt on the next wake. The retry interval must be documented in firmware.

### Rule 5 — Captive portal with NVS-backed operational config

Every certified device must expose a captive portal setup page (accessible when no WiFi credentials are stored, or on factory reset) that allows configuration of all device-specific operational parameters. This includes, at minimum:

- Any time-window boundaries that gate device behavior (e.g., `capture_start_hour`, `capture_end_hour`)
- Any interval or frequency settings (e.g., `capture_interval_hours`, `report_interval_minutes`)
- Any device-specific thresholds (e.g., moisture alert levels, temperature bounds)

Requirements:

- Parameters must be exposed as editable form fields in the captive portal HTML page — not buried in firmware constants.
- All parameters must persist across reboots via `Preferences` (Arduino NVS) or equivalent non-volatile storage.
- The device must read these values from NVS on every wake cycle. Hardcoded constants used as defaults are acceptable, but must not override stored values.
- The portal must pre-populate each field with the current stored value so the user can see and adjust the active configuration without resetting to defaults.

This requirement exists so that operational behavior can be adjusted in the field without reflashing firmware.

---

*Wire format specification coming soon.*
