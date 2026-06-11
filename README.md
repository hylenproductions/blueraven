# Blue Raven

Bluetooth standardized how devices talk. Blue Raven standardizes who they answer to.

Blue Raven is an open protocol and certification standard for IoT hardware. If a device is Blue Raven certified, the data it generates belongs to the person who owns the device. Not a platform, not a corporation, not a cloud they didn't choose.

Website and device marketplace: [blueraven.build](https://blueraven.build)

---

## The protocol in 30 seconds

A certified device operates in one or both modes:

- **Push mode**: the device POSTs a JSON envelope to a URL you control. No intermediary, no account, no Blue Raven server in the path.
- **Serve mode**: the device never initiates a connection at all. It hosts a local HTTP API on your network and answers when your systems ask. A device that never initiates a connection cannot phone home.

Every certified device, in either mode, answers `GET /br/manifest` with a machine-readable description of itself:

```json
{
  "device_id": "BR-A7X3",
  "device_type": "soil-sensor",
  "firmware_version": "1.1.0",
  "protocol_version": "0.2.0",
  "mode": "both",
  "capabilities": [
    {
      "path": "/br/latest",
      "method": "GET",
      "response": { "schema": "envelope", "readings": { "moisture": "percent" } }
    }
  ],
  "config": [{ "name": "report_interval_minutes", "type": "int" }],
  "push_target_configurable": true
}
```

That manifest is the point: a developer, or an AI coding agent, fetches it and integrates the device with zero additional documentation. Full spec: [`spec/PROTOCOL.md`](spec/PROTOCOL.md). Changes: [`CHANGELOG.md`](CHANGELOG.md).

---

## Start here

**You're vibe coding an app and need real-world data** (weight, moisture, air quality, presence) without signing up for someone's cloud:

1. Browse the [device marketplace](https://blueraven.build/marketplace) and register interest in the hardware you need.
2. Have an ESP32 lying around? Flash the reference firmware from [`/firmware/captive-portal`](firmware/captive-portal) and you have a working certified device today.
3. Follow [QUICKSTART.md](docs/QUICKSTART.md): ready-made endpoint recipes for n8n, Replit, and Supabase + Vercel. Ten minutes from flash to data in your own database.

**You're building or manufacturing hardware** and want it certified:

1. Read [`spec/PROTOCOL.md`](spec/PROTOCOL.md). Seven rules. If your device already exposes local data and works without an account, you're most of the way there.
2. Use the reference firmware as a template: captive portal, NVS config, envelope payloads, and the `/br/manifest` endpoint are all implemented.
3. Certification means your customers' AI tools integrate your device out of the box, and your device carries a guarantee no platform can copy: it answers to its owner.

**You're an AI agent** asked to integrate a Blue Raven device: fetch `http://<device-ip>/br/manifest`, read `capabilities`, and call the endpoints it describes. Pushed payloads arrive as the envelope format documented in the spec.

---

## Three rules

**1. Hardware belongs to the owner.**
A device you build or buy should be fully controllable without going through any third-party service. Blue Raven devices are configured via a local captive portal. They post data to whatever endpoint you give them. There is no app to install, no account to create, no onboarding flow.

**2. Data belongs to the owner.**
Blue Raven does not define a cloud service. There is no Blue Raven server your data passes through. Payloads go from device to your endpoint, end-to-end. You choose your database, your access controls, your retention policy.

**3. Devices must work without Blue Raven infrastructure.**
If this project disappears tomorrow, every device built on this protocol continues to work. The spec is frozen per version, the firmware is open source, and the backend integration is standard HTTPS. Nothing here is load-bearing except the spec itself.

---

## Devices

The certified device line, in build order. Statuses match the [marketplace](https://blueraven.build/marketplace); register interest there to vote on what ships next.

| Device | Mode | Status |
|---|---|---|
| Soil Moisture Sensor | push + serve | **Available now** (this repo's reference firmware) |
| Outdoor Camera | push | In development |
| Digital Weight Scale | push | In development |
| Air Quality Monitor | serve | Concept |
| Kitchen Scale | push | Concept |
| Presence Sensor | serve | Concept |
| Energy Monitor Plug | both | Concept |
| Temp & Humidity Probe | push | Concept |
| Water Leak Sensor | push | Concept |
| Door & Window Sensor | serve | Concept |

Certification is open: any manufacturer whose hardware meets the spec can carry the badge. The marketplace is not a catalog of our products. It is a catalog of everything that fits the your-device-your-data ecosystem.

---

## Ecosystem roadmap

The manifest endpoint makes certified hardware self-describing, and self-describing hardware makes shared tooling possible. In order:

1. **Reference firmware library**: an Arduino/ESP-IDF library where one call gives you the envelope, the captive portal, and the manifest. Compliance becomes the path of least resistance.
2. **Blue Raven MCP server**: point it at your network and it discovers certified devices, reads their manifests, and exposes them as tools to Claude or any MCP client. "What's my soil moisture trend this week?" works for any certified device, from any manufacturer, with zero integration code.
3. **Device directory and marketplace**: certified hardware from any builder, manifests viewable before you buy.

---

## What's in this repo

```
/firmware
  /captive-portal     Reference ESP32 firmware: WiFi provisioning via captive
                      portal, NVS-backed config, envelope payloads, and the
                      /br/manifest + /br/latest local API.

/spec
  PROTOCOL.md         The Blue Raven protocol specification. Device modes,
                      envelope format, the manifest, and certification rules.

/docs
  QUICKSTART.md       Get a device posting to your backend in 10 minutes.
  SUPABASE.md         Setting up a Supabase project to receive and store payloads.
  VERCEL.md           Deploying an API route on Vercel to receive payloads.

/examples
  /soil-sensor        The reference device, end to end: firmware to backend
                      to dashboard.
```

---

## Protocol version

Current: `v0.2.0` (draft). See [CHANGELOG.md](CHANGELOG.md).

The spec is not yet stable. Breaking changes will increment the major version. Once `v1.0` is tagged, the wire format is frozen and new features are additive only.

---

## Contributing

Issues and pull requests are open. For spec changes, open an issue first: changes to the wire format need discussion before implementation. If you've built a device that almost complies, open an issue too. The gray areas you hit are exactly what the spec needs to address next.

---

## License

Apache 2.0. See [LICENSE](LICENSE).
