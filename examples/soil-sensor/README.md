# Soil Sensor Example

A complete example application built on the Blue Raven protocol: a soil moisture sensor that posts readings to a Next.js backend with a live dashboard.

The device side is this repo's reference firmware. Flash it from [`/firmware/captive-portal`](../../firmware/captive-portal), point it at your backend (see [QUICKSTART.md](../../docs/QUICKSTART.md) for ready-made endpoint recipes), and you have the full loop: sensor to your own database to your own dashboard.

A polished, cloneable example app (Next.js + Supabase, with charts and device status) is being prepared for public release. Until then, the QUICKSTART covers the same backend setup step by step.
