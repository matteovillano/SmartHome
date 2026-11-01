# Smart Home Brain

**Smart Home Brain** is a smart home controller built on the **Arduino Portenta H7** that senses the environment and drives lights and shutters through user-defined rules, processing data locally at the edge. It is demonstrated on a physical mock-up of a small house (3 rooms, low-voltage actuators).

The course is graded on edge computing: the key properties to preserve and measure are **low latency, offline operation, and minimal network traffic** (only events and state changes are transmitted, never raw sensor streams).

## Hardware

- Arduino Portenta H7: dual core, Cortex-M7 (480 MHz) + Cortex-M4 (240 MHz)
- Breakout board with microSD
- 3 PIR motion sensors, 3 ambient light sensors (I2C), 6 LEDs, 3 micro servos (shutters), 4 push buttons, 1 relay module (3.3 V), 1 small OLED display (I2C), external 5 V / 2 A supply

### Platform constraints (always respect)

- **3.3 V logic** on all pins.
- **WiFi only on the M7.**
- **The M4 cannot use `Serial`**: M4 debug output goes through RPC to the M7.
- Servos are powered by the external 5 V supply with a common ground, never from the board.
- M4 ↔ M7 communication uses **RPC** (`RPC.h`). On the M7, `RPC.begin()` also boots the M4 (it calls `bootM4()` internally), so the M4 runs only if the M7 sketch calls `RPC.begin()`.
- Built-in RGB LEDs (`LEDR`, `LEDG`, `LEDB`) are **active LOW**.

## Toolchain

- Code is edited in any IDE (e.g. VS Code with clangd). The Arduino IDE is **not** used: everything is compiled and uploaded with **`arduino-cli`** (tested with 1.5.1).
- Core: **Arduino Mbed OS Portenta Boards** (`arduino:mbed_portenta`, tested with 4.6.0).
- Both cores use the **same board**: the M4 is selected with a board option, not a separate FQBN.

| Target | FQBN |
|---|---|
| M7 (`m7_brain`, M7 tests) | `arduino:mbed_portenta:envie_m7` |
| M4 (`m4_realtime`, M4 tests) | `arduino:mbed_portenta:envie_m7:target_core=cm4` |

The flash split (default `split=50_50`, 1 MB M7 + 1 MB M4) must be the **same** for both uploads. If you change it, add `,split=...` to both FQBNs.

### Rules that shape the structure

These rules come from how `arduino-cli` builds a sketch. It copies the sketch folder to a temporary build directory, so anything outside the sketch can only be reached as a library.

- Sketch folder name must match the `.ino` name.
- Only the sketch root and its `src/` subfolder are compiled. **Relative includes outside the sketch folder (`../../...`) do not work.**
- All module code therefore lives in **libraries** under `firmware/libraries/`, so that the main sketches **and** the test sketches can use it. Each sketch `.ino` stays thin: it wires modules together, then runs `setup()` / `loop()`.
- Libraries are found **only by their top-level header**. Each library has an umbrella header in `src/` (`SmartHomeM4.h`, `SmartHomeM7.h`) that sketches include. `#include <sensors/pir.h>` alone is **not** discovered. Inside a library, use relative includes (`#include "../core/event_bus.h"`).
- **A library cannot include files from a sketch.** `secrets.h` lives in the `m7_brain/` sketch root and is included only by `m7_brain.ino`, which passes the credentials to the comms module at runtime (e.g. `wifi.begin(SECRET_SSID, SECRET_PASS)`).
- `firmware/libraries/` is not in the default library path, so **every compile must pass `--libraries firmware/libraries`** (see below).

## Repository structure

```
smart-home-brain/
├── firmware/
│   ├── libraries/
│   │   ├── SmartHomeCommon/         # shared M4/M7 contracts
│   │   │   ├── library.properties
│   │   │   └── src/
│   │   │       ├── SmartHomeCommon.h # umbrella header
│   │   │       ├── messages.h       # Event / Command contract
│   │   │       ├── rpc_protocol.h   # RPC method names
│   │   │       └── config.h         # rooms, devices, pins, thresholds
│   │   ├── SmartHomeM4/             # M4 modules
│   │   │   ├── library.properties
│   │   │   └── src/
│   │   │       ├── SmartHomeM4.h    # umbrella header
│   │   │       ├── sensors/         # pir, light, buttons
│   │   │       └── actuators/       # led, servo, relay
│   │   └── SmartHomeM7/             # M7 modules
│   │       ├── library.properties
│   │       └── src/
│   │           ├── SmartHomeM7.h    # umbrella header
│   │           ├── core/            # event_bus, state_manager
│   │           ├── logic/           # rule_engine, scheduler, override
│   │           ├── comms/           # wifi, mqtt, offline_buffer
│   │           └── storage/         # rules, config, logger
│   ├── m4_realtime/
│   │   ├── m4_realtime.ino
│   │   └── sketch.yaml              # default_fqbn (M4)
│   └── m7_brain/
│       ├── m7_brain.ino
│       ├── sketch.yaml              # default_fqbn (M7)
│       ├── secrets.h                # git-ignored
│       └── secrets.example.h
├── tests/
│   ├── m4/<test_name>/<test_name>.ino   # one sketch per M4 component (+ sketch.yaml)
│   ├── m7/<test_name>/<test_name>.ino   # one sketch per M7 component (+ sketch.yaml)
│   └── measurements/                    # latency / traffic scripts (T1, T6)
├── webapp/                          # static web app, MQTT over WebSockets (see webapp/README.md)
├── broker/                          # Mosquitto config, ACL, setup-users.sh
├── docs/                            # specification, wiring, report
└── build/                           # git-ignored: build output, compile_commands.json
```

Minimal `library.properties` (same for each library, changing `name` and `sentence`):

```properties
name=SmartHomeM4
version=0.1.0
author=Team
maintainer=Team
sentence=M4 sensor and actuator drivers
paragraph=
category=Other
url=
architectures=mbed_portenta
```

Each sketch commits a `sketch.yaml` with only the board, never the port (ports differ per machine):

```yaml
# firmware/m4_realtime/sketch.yaml
default_fqbn: arduino:mbed_portenta:envie_m7:target_core=cm4
```

```yaml
# firmware/m7_brain/sketch.yaml
default_fqbn: arduino:mbed_portenta:envie_m7
```

## Build and upload

Run all commands from the repository root.

### One-time setup

```bash
brew install arduino-cli
arduino-cli config init
arduino-cli core update-index
arduino-cli core install arduino:mbed_portenta
# third-party libraries used by the modules (pinned versions):
arduino-cli lib install "ArduinoMqttClient@0.1.8"
```

Add each third-party library to the list above as soon as a module starts using it, so every team member installs the same set.

### Find the port

```bash
arduino-cli board list        # e.g. /dev/cu.usbmodem14201 (macOS)
```

If the port disappears or the upload fails, double-press the reset button (the green LED pulses = bootloader mode), then run `board list` again.

### Compile

The FQBN comes from each sketch's `sketch.yaml`:

```bash
arduino-cli compile --libraries firmware/libraries firmware/m7_brain
arduino-cli compile --libraries firmware/libraries firmware/m4_realtime
```

### Upload (both cores)

```bash
PORT=/dev/cu.usbmodem14201
arduino-cli compile --upload -p $PORT --libraries firmware/libraries firmware/m4_realtime
arduino-cli compile --upload -p $PORT --libraries firmware/libraries firmware/m7_brain
```

Each core keeps its image in its own flash area, so after changing only one core, re-upload only that one.

### Single test sketch

```bash
arduino-cli compile --upload -p $PORT --libraries firmware/libraries tests/m7/test_rule_engine
```

An M4 test runs only if the M7 is running a sketch that calls `RPC.begin()`, and its output must be forwarded by the M7 over RPC → `Serial`. So keep `m7_brain` (or a small M7 forwarder test sketch) flashed on the M7 while running M4 tests.

### Serial monitor (M7 only)

```bash
arduino-cli monitor -p $PORT -c baudrate=115200
```

### IDE support (IntelliSense / clangd)

Generate a `compile_commands.json` for each sketch without building the binary, then point the IDE at it (e.g. VS Code + clangd: `--compile-commands-dir=build/m7_brain`):

```bash
arduino-cli compile --libraries firmware/libraries --build-path build/m7_brain    --only-compilation-database firmware/m7_brain
arduino-cli compile --libraries firmware/libraries --build-path build/m4_realtime --only-compilation-database firmware/m4_realtime
```

Re-run these after adding files or libraries. The M4 and M7 databases differ (`CORE_CM4` vs `CORE_CM7`), so use the one that matches the code you are editing.

## Architecture

Event-driven, with a strict split of responsibilities:

```
 Browser ──HTTPS──► Web app server
    │                    │
    └──MQTT over WebSocket──► MQTT broker (Mosquitto) ◄──MQTT over TCP (WiFi)──► M7 ◄──RPC──► M4 ◄──GPIO/I2C/PWM──► sensors & actuators
```

### M4 – peripherals only ("the body")

The M4 **decides nothing**. It only drives the hardware it is wired to and talks to the M7:

- **Sensor drivers** (`begin()`, `read()`, `changed()`): PIR, light sensors, buttons. Reads are reported to the M7 as `Event`s, **only on change or threshold crossing** (keeps RPC traffic low; the M7 never polls).
- **Actuator drivers** (`apply(const Command&)`): LEDs, servos, relay, OLED. Executes every `Command` received from the M7.
- **Hardware safety**: clamps values to the physical limits of each device (e.g. servo range) and runs the watchdog.
- **RPC link**: sends `Event`s and receives `Command`s. No rules, no state beyond what a driver needs, no network.

### M7 – all logic and all communication ("the brain")

The M7 is the only core with **WiFi and `Serial`**, so it holds all the logic and is the only part that talks to the outside world:

- **Event bus**: central queue. Modules publish and consume events and never call each other directly. Events come from the M4 (sensors, buttons), the scheduler, and the web app (remote commands).
- **State manager**: single source of truth for device and sensor state.
- **Rule engine**: evaluates rules (trigger → conditions → actions) for each event and sends the resulting `Command`s to the M4.
- **Scheduler**: time events for routines (NTP + RTC).
- **Override manager**: blocks automatic rules for a configurable time after a manual command (button or web app).
- **Comms**:
  - WiFi
  - MQTT client (receives commands, rules and routines from the web app; publishes state changes and events)
  - offline buffer (stores events while offline and sends them on reconnect)
- **Storage**: persists rules, routines and configuration (flash / microSD).
- **Logger**: local event log (also printed on `Serial` for debugging).

### Web app (off-board)

A web application hosted on a server. Opening the page lets the user:

- see the live state of every room, device and sensor (FR8)
- control lights, shutters and the relay manually (FR6)
- add, edit, enable and delete rules and routines (FR4, FR5, FR9)
- see system status (online, uptime, errors)

**Communication with the board uses MQTT** through a broker (Mosquitto), in both directions:

- The **M7** connects to the broker with MQTT over TCP.
- The **browser** cannot open raw TCP sockets, so it connects with **MQTT over WebSockets**. Enable a `websockets` listener on Mosquitto, and use a JS client such as MQTT.js on the page. The web app server only serves the page; it does not need to relay messages.
- **Security (NFR6)**: the broker requires username/password (one account for the board, one per web user) and an ACL that restricts what each account may publish and subscribe to. Use TLS (`mqtts` / `wss`) when the broker is reachable from outside the LAN.
- **Edge / offline (NFR3)**: the web app is only an interface. Rules, routines and buttons run entirely on the board, so the house keeps working if the server, the broker or the internet is down. Remote control needs the broker to be reachable; hosting the broker (and optionally the web app) on the local network keeps the UI working without internet.

### Example flows

- **Automatic**: PIR → M4 `Event{MOTION, room 1}` → RPC → M7 event bus → state manager → rule engine (motion AND light < 50 lux) → override check → `Command{LIGHT_SET, device 1, 100}` → RPC → M4 turns LED on. In parallel, M7 logs and publishes `home/room1/light1/state` (or buffers it if offline).
- **Remote command**: web app publishes `home/room1/shutter1/set` = `50` → broker → M7 MQTT client → event bus (`REMOTE_COMMAND`) → override manager (pauses automatic rules for that device) → `Command{SHUTTER_SET, 1, 50}` → RPC → M4 moves the servo → M7 publishes the new state → web app updates.

## Contracts (change only with whole-team agreement)

### M4 ↔ M7 messages (`messages.h`)

```cpp
enum EventType   : uint8_t { MOTION, LIGHT_LEVEL, BUTTON, TIME_TICK, REMOTE_COMMAND };
enum CommandType : uint8_t { LIGHT_SET, SHUTTER_SET, RELAY_SET };

struct Event   { EventType type; uint8_t room; int16_t value; uint32_t timestamp; };
struct Command { CommandType type; uint8_t device; int16_t value; };  // value 0-100
```

The M4 only produces `MOTION`, `LIGHT_LEVEL` and `BUTTON` events. `TIME_TICK` (scheduler) and `REMOTE_COMMAND` (MQTT) are created on the M7 and travel only on its internal event bus. `Command`s go only from M7 to M4.

### MQTT topics

| Topic | Direction | Content |
|---|---|---|
| `home/<room>/<device>/state` | board → web app | Device state |
| `home/<room>/<device>/set` | web app → board | Device command |
| `home/<room>/sensor/<type>` | board → web app | Sensor events (on change only) |
| `home/system/rules` | both | Add / edit / delete rules |
| `home/system/routines` | both | Add / edit / delete scheduled routines |
| `home/system/status` | board → web app | Online, uptime, errors |

### Status payload (`home/system/status`)

Retained, JSON. The board publishes it on connect, when `errors` changes, and as a heartbeat every 5 min. Its MQTT Last Will is `{"online":false}` (retained), so the web app sees a crash or power loss.

```json
{ "online": true, "uptime_s": 3600, "errors": 0, "last_error": "", "fw": "0.1.0" }
```

The web app extrapolates the uptime between messages, so no periodic stream is needed (NFR5).

### Rule format (JSON)

```json
{
  "id": 1,
  "name": "Light on motion in the dark",
  "trigger": { "event": "MOTION", "room": 1 },
  "conditions": [ { "sensor": "light", "room": 1, "op": "<", "value": 50 } ],
  "actions": [ { "type": "LIGHT_SET", "device": 1, "value": 100 } ],
  "enabled": true
}
```

A routine is a rule with a time trigger:

```json
{
  "id": 2,
  "name": "Open shutters on weekday mornings",
  "trigger": { "event": "TIME", "at": "07:30", "days": ["mon", "tue", "wed", "thu", "fri"] },
  "conditions": [],
  "actions": [ { "type": "SHUTTER_SET", "device": 1, "value": 100 } ],
  "enabled": true
}
```

## Requirements

### Functional (MoSCoW)

| ID | Requirement | Priority |
|---|---|---|
| FR1 | Turn lights on and off, on command or automatically | Must |
| FR2 | Open, close and position shutters | Must |
| FR3 | Read ambient light and detect motion in each room | Must |
| FR4 | Execute user-defined "if condition, then action" rules | Must |
| FR5 | Execute routines at scheduled times | Must |
| FR6 | Accept manual commands from buttons and a user interface | Must |
| FR7 | Give manual commands priority over automatic rules | Must |
| FR8 | Show the current state of devices and sensors | Should |
| FR9 | Add, edit and remove rules at runtime | Should |
| FR10 | Keep rules and settings after a power loss | Should |
| FR11 | Buffer events while the network is down | Should |
| FR12 | Send aggregated data to the cloud for remote monitoring | Could |

### Non-functional

| ID | Requirement | Target |
|---|---|---|
| NFR1 | Event-to-action latency | ≤ 200 ms |
| NFR2 | Command-to-action latency | ≤ 500 ms |
| NFR3 | Offline operation | Rules, routines and manual commands work without internet |
| NFR4 | Reliability | 24 h run without failure, watchdog recovery |
| NFR5 | Network traffic | Only events and state changes transmitted |
| NFR6 | Security | Commands accepted only from authenticated clients |

### Acceptance tests

T1 latency (50 trials, median ≤ 200 ms) · T2 rules and override · T3 offline (WiFi off 10 min) · T4 persistence (power cut) · T5 endurance (24 h) · T6 edge vs cloud (latency and bytes compared).

## Coding guidelines

- **Never use `delay()`**; use `millis()`-based timing or threads.
- Modules communicate only through the event bus.
- No hardcoded configuration: pins, rooms and thresholds in `config.h`, rules in JSON.
- One responsibility per file, each module with its own `.h` / `.cpp`.
- Naming: `PascalCase` classes/structs, `camelCase` functions/variables, `UPPER_CASE` constants and enum values.
- Comments in English, explaining *why*.
- Every component gets a standalone test sketch in `tests/m4/` or `tests/m7/` before integration.
- Credentials go in `firmware/m7_brain/secrets.h` (git-ignored); commit only `secrets.example.h`. Libraries never include `secrets.h`: the sketch passes the values in.
- Keep RAM and flash usage in mind: prefer static allocation, avoid `String` in hot paths.

## Team and workflow

| Role | Area | Requirements |
|---|---|---|
| A – Hardware and M4 | Drivers, wiring, power, event filter, watchdog, mock-up | FR1–FR3, NFR4 |
| B – Logic (M7) | Event bus, state manager, rule engine, override, scheduler | FR4, FR5, FR7, NFR1 |
| C – Connectivity and data | WiFi, MQTT, offline buffer, storage, auth, RPC | FR10, FR11, NFR3, NFR5, NFR6 |
| D – Interface and validation | Broker, web app, rule editor, measurements, tests, docs | FR6, FR8, FR9, T1–T6 |

- Branches: `main` (always working), `dev` (integration), `feature/<req-id>-<name>`.
- Every PR reviewed by at least one member, referencing the requirement ID.
- Commit messages prefixed by area: `m4:`, `m7:`, `comms:`, `webapp:`, `docs:`, `tests:`.
- Members develop against stubs (fake events, fake MQTT client) to work in parallel.

## Current status

- [x] Requirements and specification defined
- [x] Architecture and repository structure defined
- [x] Board received
- [x] Phase 0 – Contracts (`messages.h`, `config.h`, the three `library.properties` with umbrella headers, `sketch.yaml` files, `.gitignore` incl. `build/` and `secrets.h`, `secrets.example.h`)
- [x] M4 ↔ M7 RPC link (`m7link` / `m4link`), tested on the board: round trip ≈ 0.4 ms (`tests/*/test_rpc_link`)
- [x] Broker (auth + ACL) and minimal web app: login and board status, tested with a fake board
- [x] M7 WiFi + MQTT in its own thread (`netlink`): real `home/system/status` with Last Will, tested on the board
- [ ] Pin assignments in `config.h` (placeholders until the wiring is fixed)
- [ ] Phase 1 – Modules developed in parallel with stubs
- [ ] Phase 2 – Integration (MVP: all Must requirements)
- [ ] Phase 3 – Should requirements and measurements
- [ ] Phase 4 – Report and demo

Update this section as the project progresses.
