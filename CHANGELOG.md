# Changelog

All notable changes to the Blue Raven protocol specification.

## v0.2.0 (draft) - 2026-06-11

Shaped by community feedback from builders running pull-based devices in the field.

### Added

- **Device modes.** Certification now covers two operating modes: push mode (device POSTs envelope payloads to the owner-configured endpoint, the original v0.1 model) and serve mode (device initiates zero outbound connections and hosts a local HTTP API with `GET /br/latest` and `GET /br/manifest`). Devices may certify in either mode or both.
- **Device manifest (Rule 6).** Every certified device must expose `GET /br/manifest`: a machine-readable self-description covering identity, firmware, mode, every endpoint with parameters and response schemas, and captive-portal config fields. A developer or AI coding agent should integrate a certified device from its manifest alone, with zero additional documentation.
- **No phoning home (Rule 7).** The philosophy is now a numbered rule: no outbound connections to non-owner endpoints. Carve-out: opt-in update checks or error reports to a manufacturer domain are allowed only if disabled by default, toggleable in the captive portal, listed in the manifest under `telemetry`, and free of sensor data.
- **`schema_version` field** in the envelope payload.

### Planned

- Reference firmware implementation of `/br/manifest` and serve mode in `/firmware/captive-portal`.
- Wire format specification: signing, retries, batching.

## v0.1 (draft)

- Initial release: envelope payload (device_id, timestamp, readings), Rules 1 through 5 (unique identity, authenticated requests, local time awareness, graceful degradation, captive portal with NVS-backed config).
