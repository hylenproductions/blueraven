# Blue Raven

Bluetooth standardized how devices talk. Blue Raven standardizes who they answer to.

---

## What this is

Blue Raven is an open protocol and certification standard for IoT hardware. If a device is Blue Raven certified, the data it generates belongs to the person who owns the device. Not a platform, not a corporation, not a cloud they didn't choose.

In practice: a Blue Raven device sends a signed JSON payload over HTTPS to a URL you control. No intermediary. No account required. No platform that raises prices once you've built on it. The protocol is intentionally minimal: any HTTP server that can parse JSON and verify an HMAC-SHA256 signature can receive Blue Raven payloads.

As of v0.2.0 the protocol covers two device modes. Push devices send their readings to your endpoint. Serve devices never initiate a connection at all: they host a local API on your network and answer when your systems ask. Every certified device, in either mode, exposes a `/br/manifest` endpoint that describes its capabilities in a machine-readable form, so a developer (or an AI coding agent) can integrate it with zero additional documentation.

This repo contains the protocol specification, reference firmware for ESP32, integration guides for common backend stacks, and example applications.

---

## Three rules

**1. Hardware belongs to the owner.**
A device you build or buy should be fully controllable without going through any third-party service. Blue Raven devices are configured via a local captive portal. They post data to whatever endpoint you give them. There is no app to install, no account to create, no onboarding flow.

**2. Data belongs to the owner.**
Blue Raven does not define a cloud service. There is no Blue Raven server your data passes through. Payloads go from device to your endpoint, end-to-end. You choose your database, your access controls, your retention policy.

**3. Devices must work without Blue Raven infrastructure.**
If this project disappears tomorrow, every device built on this protocol continues to work. The spec is frozen per version, the firmware is open source, and the backend integration is standard HTTPS. Nothing here is load-bearing except the spec itself.

---

## What's in this repo

```
/firmware
  /captive-portal     Reference ESP32 firmware with WiFi provisioning and
                      endpoint configuration via a local web interface.

/spec
  PROTOCOL.md         The Blue Raven protocol specification. Payload format,
                      authentication, versioning, and device lifecycle.

/docs
  QUICKSTART.md       Get a device posting to your backend in 10 minutes.
  SUPABASE.md         Setting up a Supabase project to receive and store payloads.
  VERCEL.md           Deploying an API route on Vercel to receive payloads.

/examples
  /soil-sensor        Example Next.js application built on the protocol.
                      Full source at github.com/blueraven/blueraven-sandbox.
```

---

## Get started

**If you have hardware:**

Flash the reference firmware from `/firmware/captive-portal`, connect to the device's setup network, configure your WiFi credentials and target endpoint, and the device starts posting. See [QUICKSTART.md](docs/QUICKSTART.md).

**If you're building a backend:**

The spec is in [`/spec/PROTOCOL.md`](spec/PROTOCOL.md). Any HTTP server that can verify an HMAC-SHA256 signature and parse JSON can receive Blue Raven payloads. The Supabase and Vercel guides cover the most common setups.

**If you're writing firmware:**

Read the protocol spec. The wire format is simple by design. Payload validation, signing, and delivery should take an afternoon on any platform with an HTTP client and HMAC support.

---

## Protocol version

Current: `v0.2.0` (draft). See [CHANGELOG.md](CHANGELOG.md).

The spec is not yet stable. Breaking changes will increment the major version. Once `v1.0` is tagged, the wire format is frozen and new features are additive only.

---

## Contributing

Issues and pull requests are open. For spec changes, open an issue first: changes to the wire format need discussion before implementation.

---

## License

Apache 2.0. See [LICENSE](LICENSE).
