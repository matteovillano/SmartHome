# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project state

Smart Home Brain: a smart home controller on an **Arduino Portenta H7** (dual core M7 + M4), graded on edge computing (low latency, offline operation, minimal network traffic). At the moment the repo contains only `README.md`, which is the full specification: requirements FR/NFR, acceptance tests T1–T6, team roles and the "Current status" checklist. Read it before starting a task, and update its "Current status" section when a phase advances. The layout described below is the agreed target; create folders following it.

## Build, upload, test

Code is edited outside the Arduino IDE and built only with `arduino-cli` (tested with 1.5.1, core `arduino:mbed_portenta` 4.6.0). Run everything from the repo root. **Every compile needs `--libraries firmware/libraries`**, otherwise `messages.h` etc. are not found.

```bash
arduino-cli board list                                   # find PORT
arduino-cli compile --libraries firmware/libraries firmware/m7_brain
arduino-cli compile --libraries firmware/libraries firmware/m4_realtime
arduino-cli compile --upload -p $PORT --libraries firmware/libraries firmware/m4_realtime
arduino-cli compile --upload -p $PORT --libraries firmware/libraries firmware/m7_brain
arduino-cli compile --upload -p $PORT --libraries firmware/libraries tests/m7/<test_name>   # single test
arduino-cli monitor -p $PORT -c baudrate=115200          # M7 only
# IDE/clangd support (no binary built):
arduino-cli compile --libraries firmware/libraries --build-path build/m7_brain --only-compilation-database firmware/m7_brain
```

Broker and web app (Mosquitto via `brew install mosquitto`; no Node needed):

```bash
cd broker && ./setup-users.sh && mosquitto -c mosquitto.conf   # TCP 1883 (board), WebSocket 9001 (browser)
python3 -m http.server 8080 -d webapp                          # open http://localhost:8080
webapp/dev/fake_board.sh                                       # fake home/system/status without the board
```

- Third-party libraries are installed globally with `arduino-cli lib install` (currently `ArduinoMqttClient@0.1.8`); add any new one to the README setup list and to `depends=` in the library's `library.properties`.
- `m7_brain` does not compile without `firmware/m7_brain/secrets.h` (copy `secrets.example.h`; keep the example free of real values).
- FQBNs are stored in each sketch's `sketch.yaml` as `default_fqbn` (commit the board, never the port). M7: `arduino:mbed_portenta:envie_m7`. M4: `arduino:mbed_portenta:envie_m7:target_core=cm4`. There is no separate M4 board.
- Both cores must use the same flash split (default `split=50_50`). Each core has its own flash area, so re-upload only the core that changed.
- An M4 sketch runs only while the M7 runs a sketch that calls `RPC.begin()`, which boots the M4 internally. M4 test output has to be forwarded by the M7 over RPC → `Serial`.
- If the port disappears, double-press reset to enter the bootloader (the green LED pulses).
- No test framework exists: tests are standalone sketches flashed to the board, one per component, in `tests/m4/` or `tests/m7/`.

## Code layout constraints (verified against arduino-cli)

`arduino-cli` copies each sketch to a temporary build dir, which drives the structure:

- **All module code lives in libraries** under `firmware/libraries/`: `SmartHomeCommon` (shared contracts `messages.h`, `config.h`), `SmartHomeM4` (sensors/actuators) and `SmartHomeM7` (core, logic, comms, storage). The main sketches (`firmware/m4_realtime`, `firmware/m7_brain`) are thin wiring, so that test sketches can reuse the same modules.
- Never use relative includes that leave a sketch folder (`../../...`): they fail.
- Libraries are found only by a header at the top of their `src/` folder. Sketches include the umbrella headers `<SmartHomeM4.h>` / `<SmartHomeM7.h>`, never `<sensors/pir.h>`. Inside a library, use relative includes.
- A library cannot include sketch files. `secrets.h` (git-ignored, with `secrets.example.h` committed) lives in `firmware/m7_brain/` and only the `.ino` includes it; credentials are passed to modules at runtime.
- The folder name must equal the `.ino` name.

## Architecture

Strict split. The **M4 decides nothing**: it drives peripherals (PIR, light sensors, buttons, LEDs, servos, relay, OLED), sends `Event`s to the M7 only on change or threshold crossing, executes `Command`s, clamps values to hardware limits and runs the watchdog. The **M7 holds all logic and all external communication**, because only it has WiFi and `Serial`. Its modules:

- **Event bus**: all modules talk only through it. Events come from the M4, the scheduler (`TIME_TICK`) and MQTT (`REMOTE_COMMAND`).
- **State manager**: single source of truth for device and sensor state.
- **Rule engine**: evaluates trigger → conditions → actions. A routine is a rule with a time trigger.
- **Scheduler** (NTP + RTC), **override manager** (manual commands pause automatic rules), **storage** (flash/microSD), **logger**.
- **Comms**: WiFi + MQTT run in a dedicated thread (`netlink`) because reconnects block for seconds; other threads talk to it only through thread-safe calls. Offline buffer still to do.

Off-board: a static web app (`webapp/`, plain JS, MQTT.js vendored in `webapp/vendor/` so it works offline, no credentials in code) talks to the M7 **only through MQTT** via Mosquitto (`broker/`). The browser uses MQTT over WebSockets; the M7 uses MQTT over TCP. The web app is only an interface: rules, routines and buttons must keep working with no network (NFR3). Only events and state changes go on the network, never raw sensor streams (NFR5).

**Contracts** (README "Contracts" section; change only with whole-team agreement): `Event` / `Command` structs in `messages.h`, the MQTT topic tree `home/<room>/<device>/{state,set}`, `home/<room>/sensor/<type>`, `home/system/{rules,routines,status}`, the status payload, and the JSON rule format. Broker access is enforced by `broker/acl`: the web account may write only `.../set`, rules and routines.

## Platform rules

- 3.3 V logic. The M4 cannot use `Serial`. Built-in `LEDR/LEDG/LEDB` are active LOW. Servos run on the external 5 V supply with a common ground.
- Never use `delay()`: use `millis()` timing or threads. Latency targets are ≤ 200 ms from event to action and ≤ 500 ms from command to action.
- No hardcoded configuration: pins, rooms and thresholds go in `config.h`; rules are JSON.
- Prefer static allocation and avoid `String` in hot paths.
- One responsibility per `.h` / `.cpp` pair. Naming: `PascalCase` types, `camelCase` functions and variables, `UPPER_CASE` constants and enum values. Comments in English, explaining *why*.

## Git workflow

Branches: `main` (always working), `dev`, `feature/<req-id>-<name>`. Commit prefixes: `m4:`, `m7:`, `comms:`, `webapp:`, `docs:`, `tests:`. PRs reference the requirement ID.
