# Web app

Static page (HTML/CSS/JS, no build step). It talks to the board only through the MQTT broker, using MQTT over WebSockets (port 9001). There is no backend: any static file server can host it.

## Run locally

```bash
# 1. Broker (first time: create the accounts)
cd broker && ./setup-users.sh && mosquitto -c mosquitto.conf

# 2. Page (another terminal, from the repo root)
python3 -m http.server 8080 -d webapp
# open http://localhost:8080 and log in as webadmin

# 3. Optional: fake board status, without flashing the Portenta (another terminal)
webapp/dev/fake_board.sh
```

## Notes

- `vendor/mqtt.min.js` is MQTT.js 5.10.1, stored in the repo on purpose: a CDN link would break the page on a LAN without internet (NFR3).
- Never put credentials in the JS. The user logs in; the page remembers only the URL and username.
- The default broker URL is `ws://<host serving the page>:9001`, so the page works unchanged when the broker and the page run on the same machine.
- Implemented so far: login and board status (`home/system/status`). Rooms, devices, rules and routines are next.
