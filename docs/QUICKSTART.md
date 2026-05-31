# Quickstart

If your platform can receive an HTTP POST request, it can receive Blue Raven sensor data. Blue Raven does not require a Blue Raven cloud. Your data goes where you tell it to go.

---

## 1. What Blue Raven sends

Every Blue Raven device posts this JSON to whatever URL you configure. That's the entire contract.

```json
{
  "device_id": "BR-A7X3",
  "timestamp": 1234567890,
  "readings": {
    "moisture": 42.5,
    "raw_value": 2100
  }
}
```

`readings` varies by sensor type. `device_id` and `timestamp` are always present.

---

## 2. Set up your device

Scan the QR code on your device packaging. Connect your phone to the `BlueRaven-[TAG]` WiFi network — the setup page opens automatically. Enter your WiFi credentials and your backend endpoint URL. Hit save. The device reboots and starts posting immediately.

Come back here once you have an endpoint URL.

---

## 3. Choose your backend

### Option A: No code — n8n (5 minutes)

1. Go to [n8n.io](https://n8n.io) and create an account.
2. Create a new workflow. Add a **Webhook** node, set the method to **POST**.
3. Copy the webhook URL — this is your Blue Raven endpoint.
4. Go back to device setup and paste the URL.
5. Add a second node: Google Sheets, Airtable, Notion, or any database.
6. Activate the workflow.

> **Free tier note:** n8n cloud limits operations. A device posting every 30 seconds sends 2,880 requests/day. For high-frequency devices, use n8n self-hosted on Railway or Render — it's free and has no operation limits. See [Option A in Section 5](#other-supported-destinations).

---

### Option B: Vibe coding — Replit (10 minutes)

Open **Replit Agent** and paste this prompt exactly:

```
Create a POST endpoint at /api/blueraven that accepts JSON payloads from Blue Raven IoT devices. The payload shape is: { "device_id": string, "timestamp": number, "readings": object }. Store all payloads in a database table named sensor_readings. Return {"success": true} on success. Show a simple table of recent readings on the home page.
```

Replit will generate the endpoint, database, and a basic dashboard. Copy the Replit app URL and add `/api/blueraven` — that's your endpoint. Paste it into your Blue Raven device setup.

---

### Option C: Developer backend — Supabase + Vercel (10 minutes)

**Step 1** — Create the table in the Supabase SQL editor:

```sql
create table sensor_readings (
  id bigint generated always as identity primary key,
  created_at timestamptz not null default now(),
  device_id text not null,
  timestamp bigint,
  readings jsonb not null
);
```

**Step 2** — Create `app/api/blueraven/route.ts` in your Next.js project:

```typescript
import { createClient } from '@supabase/supabase-js'
import { NextRequest, NextResponse } from 'next/server'

const supabase = createClient(
  process.env.SUPABASE_URL!,
  process.env.SUPABASE_SERVICE_ROLE_KEY!
)

export async function POST(req: NextRequest) {
  const payload = await req.json()
  const { error } = await supabase
    .from('sensor_readings')
    .insert({
      device_id: payload.device_id,
      timestamp: payload.timestamp,
      readings: payload.readings
    })
  if (error) return NextResponse.json({ error: error.message }, { status: 500 })
  return NextResponse.json({ success: true }, { status: 201 })
}
```

**Step 3** — Deploy to Vercel. Your endpoint is `https://your-app.vercel.app/api/blueraven`. Paste it into your Blue Raven device setup.

---

## 4. Verify it's working

Check your database or dashboard — you should see a new row every 30 seconds. If nothing is showing up, check that your endpoint URL is correct and that your device's red power LED is on.

---

## 5. Other supported destinations

Any platform that can receive an HTTP POST works. Here are copy-paste starting points for the most common ones.

| Platform | Notes |
|----------|-------|
| **Lovable** | Prompt: `"Create a POST endpoint at /api/blueraven that stores Blue Raven payloads in Supabase"` |
| **Bolt.new** | Same prompt as Lovable |
| **Base44** | Prompt: `"Build a webhook endpoint that receives Blue Raven sensor JSON and stores it in a table"` |
| **v0 + Vercel** | Use the same Next.js route from Option C above |
| **Firebase** | Cloud Function: `db.collection('sensor_readings').add(req.body)` |
| **Xano** | Create a POST endpoint, set input to JSON body, add a database insert step |
| **Appwrite** | Create a Function with an HTTP trigger, insert to collection |
| **Railway** | Deploy an Express app: `app.post('/blueraven', async (req, res) => { ... })` |
| **Render** | Identical to Railway |
| **Cloudflare Workers** | `await env.DB.prepare('INSERT INTO readings VALUES (?)').bind(JSON.stringify(payload)).run()` |
| **n8n self-hosted** | Same as Option A but host it yourself for unlimited operations |
| **Make** | Custom Webhook module, map fields to your database |
| **Zapier** | Catch Hook trigger, action to Airtable or Google Sheets. Task limits apply at high frequency. |
| **Pipedream** | HTTP trigger, add storage step. Generous free tier. |

---

## 6. Rate limits and free tier reality

Blue Raven devices post every 30 seconds by default — 2,880 requests/day, ~86,400/month. Here's what that means for free tiers:

| Platform | Free Tier Fit |
|----------|---------------|
| Supabase | Good — 500k edge function calls/month |
| n8n self-hosted | Unlimited |
| Replit | Watch for sleep on free tier |
| Railway | Good — generous free tier |
| Render | Good |
| Cloudflare Workers | Excellent — 100k requests/day free |
| Make | Bad — burns free tier in hours |
| Zapier | Bad — task limits apply |
| Pipedream | Usually fine |

To reduce request frequency, change the `POST_INTERVAL_MS` value in the firmware before flashing. See `firmware/captive-portal/README.md`.
