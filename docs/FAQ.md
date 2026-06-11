# FAQ

## How is this different from Matter?

Matter is a real step forward: one consortium standard so devices from different brands can talk to each other. But it answers a different question than Blue Raven does.

Matter standardizes how devices talk to **ecosystems**. It is run by the Connectivity Standards Alliance, whose promoter members are the same corporations whose ecosystems your data flows into. Joining the alliance and certifying a product costs thousands of dollars, and in practice you drive Matter devices through a controller app: Apple Home, Google Home, or Alexa. It is built for manufacturers selling into those ecosystems.

Blue Raven standardizes who devices **answer to**. A certified device posts to an endpoint you control, or serves a local API on your network that your own software queries. No controller app, no account, no consortium membership, no certification fee. The spec is Apache 2.0; compliance is verifiable by anyone reading the firmware.

If you are building a product for the Apple/Google/Amazon shelf, Matter is your standard. If you are building for yourself, Blue Raven is.

## Why not just use Home Assistant?

Home Assistant is excellent and Blue Raven devices work with it happily: point a push device at a webhook automation, or poll a serve device with a RESTful sensor. The difference is scope. Home Assistant is a hub you run; Blue Raven is a contract the hardware itself honors. A certified device needs no hub at all. It works with Home Assistant, a bare Supabase table, an n8n workflow, a Replit app, or sixty lines of Python, because all it speaks is HTTP and JSON to whatever you choose.

## What does certification cost?

Nothing. The spec is open, the rules are public, and compliance is testable from the device itself (fetch `/br/manifest`, watch the wire). There is no fee, no membership, and no gatekeeper to satisfy.

## Can I sell hardware that carries the badge?

Yes. That is the point. Build a device that meets the rules in [`spec/PROTOCOL.md`](../spec/PROTOCOL.md), keep the firmware open, and your customers get a guarantee no platform can copy: the device answers to them, and it keeps working even if you (or we) disappear.

## Who is this for?

People who build for themselves. Weekend builders wiring sensors into a personal health app, homelabbers who want every byte on their own disk, and hardware makers who think "no subscription required" is a feature worth printing on the box.
