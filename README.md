# Smart Home Brain

A customizable smart home controller built on the **Arduino Portenta H7**. It senses the environment, drives lights and shutters through user-defined rules and processes data locally at the edge, so the core functions of the house keep working without an internet connection.

Developed for the *Edge Computing in the IoT* course and demonstrated on a physical mock-up of a small house.

---

## Table of contents

1. [Architecture](#architecture)
2. [Repository structure](#repository-structure)
3. [Interfaces (contracts)](#interfaces-contracts)
4. [Getting started](#getting-started)
5. [Coding guidelines](#coding-guidelines)
6. [Team and responsibilities](#team-and-responsibilities)
7. [Workflow](#workflow)
8. [Roadmap](#roadmap)

---

## Architecture

The system is **event-driven** and split across the two cores of the Portenta H7.

| Core | Role | Modules |
|---|---|---|
| **Cortex-M4** (real time, "the body") | Reads sensors, drives actuators | Sensor drivers, actuator drivers, event filter, safety and watchdog |
| **Cortex-M7** (decisions, "the brain") | Decides what to do, communicates | Event bus, state manager, rule engine, scheduler, override manager, comms, storage, logger |
| **Off-board** | User interaction and monitoring | MQTT broker, dashboard |

### Modules

**M4**
- **Sensor drivers** – PIR, ambient light, buttons. Common interface: `begin()`, `read()`, `changed()`.
- **Actuator drivers** – LEDs, servos, relay. Common interface: `apply(const Command&)`.
- **Event filter** – sends an event to the M7 only when a value changes or crosses a threshold.
- **Safety and watchdog** – servo limits, recovery from faults.

**M7**
- **Event bus** – central queue; modules publish and consume events, never call each other directly.
- **State manager** – single source of truth for the state of every device.
- **Rule engine** – evaluates rules (trigger → conditions → actions) for each event.
- **Scheduler** – generates time events for routines (NTP + RTC).
- **Override manager** – blocks automatic rules for a set time after a manual command.
- **Comms** – WiFi, MQTT client, offline buffer.
- **Storage** – persists rules and configuration (flash / microSD).
- **Logger** – local event log.

### Data flow (example: entering a dark room)

```
PIR detects motion
  → M4: Event{MOTION, room 1}                    ──RPC──►  M7
  → Event bus → State manager (update)
  → Rule engine: motion in room 1 AND light < 50 lux → LIGHT_SET(room 1, 100)
  → Override manager: no active manual override
  → Command{LIGHT_SET, device 1, 100}           ◄──RPC──  M7
  → M4: LED on
  → (in parallel) Logger + MQTT publish, or offline buffer if the network is down
```

### Platform constraints

- WiFi is available **only on the M7**.
- The M4 cannot use `Serial` directly: debug output from the M4 goes through RPC to the M7.
- The board uses **3.3 V logic**: every peripheral must be 3.3 V compatible.
- Servos are powered by an **external 5 V supply** with a common ground, never from the board.

---

## Repository structure

```
smart-home-brain/
├── firmware/
│   ├── common/              # shared between M4 and M7
│   │   ├── messages.h       # event and command format (the contract)
│   │   └── config.h         # rooms, devices, pins, thresholds
│   ├── m4_realtime/
│   │   ├── m4_realtime.ino
│   │   ├── sensors/         # pir, light, buttons
│   │   └── actuators/       # led, servo, relay
│   └── m7_brain/
│       ├── m7_brain.ino
│       ├── core/            # event_bus, state_manager
│       ├── logic/           # rule_engine, scheduler, override
│       ├── comms/           # wifi, mqtt, offline_buffer
│       └── storage/         # rules, config, logger
├── dashboard/               # Node-RED flows / web app
├── tests/                   # one test sketch per component + measurement scripts
└── docs/                    # specification, wiring diagrams, report
```

> **Shared code:** the Arduino IDE does not share code between two sketches. Either install `firmware/common/` as a library in the Arduino `libraries` folder, or use **PlatformIO** (recommended).

---

## Interfaces (contracts)

These are agreed **before** writing module code, so everyone can work in parallel. Changing them requires agreement from the whole team.

### M4 ↔ M7 messages (`firmware/common/messages.h`)

```cpp
enum EventType   : uint8_t { MOTION, LIGHT_LEVEL, BUTTON, TIME_TICK, REMOTE_COMMAND };
enum CommandType : uint8_t { LIGHT_SET, SHUTTER_SET, RELAY_SET };

struct Event {
  EventType type;
  uint8_t   room;
  int16_t   value;      // e.g. lux, button id, 0/1 for motion
  uint32_t  timestamp;  // millis()
};

struct Command {
  CommandType type;
  uint8_t     device;
  int16_t     value;    // e.g. 0-100 for brightness or shutter position
};
```

### MQTT topics

| Topic | Direction | Content |
|---|---|---|
| `home/<room>/<device>/state` | board → dashboard | Current state of a device |
| `home/<room>/<device>/set` | dashboard → board | Command for a device |
| `home/<room>/sensor/<type>` | board → dashboard | Sensor events (on change only) |
| `home/system/rules` | both | Add, edit, delete rules |
| `home/system/status` | board → dashboard | Online/offline, uptime, errors |

Raw sensor streams are **never** published, only events and state changes.

### Rule format (JSON)

```json
{
  "id": 1,
  "name": "Light on motion in the dark",
  "trigger": { "event": "MOTION", "room": 1 },
  "conditions": [
    { "sensor": "light", "room": 1, "op": "<", "value": 50 }
  ],
  "actions": [
    { "type": "LIGHT_SET", "device": 1, "value": 100 }
  ],
  "enabled": true
}
```

---

## Getting started

1. Install **Arduino IDE 2.x** (or VS Code + PlatformIO).
2. Install the **Arduino Mbed OS Portenta Boards** core from the Boards Manager.
3. Connect the board with a **USB-C data cable**. If it is not detected, double-press reset (green LED pulsing = bootloader mode).
4. On first use, update the bootloader and the WiFi firmware with the examples in *File → Examples → STM32H747_System*.
5. Upload `m4_realtime` with *Tools → Target core → M4* and `m7_brain` with *Target core → M7*. The M7 starts the M4 with `bootM4()`.
6. Copy `firmware/common/secrets.example.h` to `secrets.h` and fill in your WiFi and MQTT credentials. `secrets.h` is in `.gitignore`: **never commit credentials**.

For development, use a phone hotspot or a home router: university enterprise WiFi (eduroam) is usually not supported.

---

## Coding guidelines

- **No `delay()`.** Use `millis()`-based timing or threads, so no module blocks the others.
- **No direct calls between modules.** Communicate through the event bus.
- **No hardcoded configuration.** Pins, rooms, thresholds go in `config.h`; rules go in JSON.
- **One responsibility per file.** Each module has its own `.h` / `.cpp`.
- **Naming:** `PascalCase` for classes and structs, `camelCase` for functions and variables, `UPPER_CASE` for constants and enum values.
- **Comments in English.** Explain *why*, not *what*.
- **Test every component alone** in `tests/` before integrating it.

---

## Team and responsibilities

| Member | Role | Responsibilities | Requirements |
|---|---|---|---|
| _Name A_ | Hardware and M4 | Sensor and actuator drivers, wiring, power, event filter, watchdog, mock-up | FR1–FR3, NFR4 |
| _Name B_ | Logic (M7) | Event bus, state manager, rule engine, override, scheduler | FR4, FR5, FR7, NFR1 |
| _Name C_ | Connectivity and data | WiFi, MQTT, offline buffer, rule storage, authentication, M4↔M7 RPC | FR10, FR11, NFR3, NFR5, NFR6 |
| _Name D_ | Interface and validation | Broker, dashboard, rule editor, measurement scripts, acceptance tests, documentation | FR6, FR8, FR9, T1–T6 |

Each member develops against **stubs** (fake events, fake MQTT client) so nobody waits for another module.

---

## Workflow

### Branches

- `main` – always working; updated only through pull requests.
- `dev` – integration branch.
- `feature/<req-id>-<short-name>` – one branch per feature, e.g. `feature/FR7-manual-override`.

### Pull requests

- Every PR is reviewed by **at least one** other member.
- The PR description references the requirement ID and says how it was tested.
- Merge into `dev`; merge `dev` into `main` after each integration day.

### Commits

Short, imperative, prefixed by area:

```
m4: add PIR driver with debounce
m7: implement rule condition evaluation
comms: buffer MQTT events while offline
dashboard: add rule editor form
```

### Issues

One issue per requirement or task, titled with its ID (e.g. `FR7 – Manual override`). This gives the requirement traceability table for the final report.

### Meetings

- One short meeting per week to align on progress and blockers.
- One **integration day** per week on the real board (shared board: agree on time slots).

---

## Roadmap

| Phase | Duration | Goal |
|---|---|---|
| 0 – Contracts | 1 week | Messages, MQTT topics, rule format, repo setup, board tests |
| 1 – Modules | 2–3 weeks | Each module developed in parallel with stubs |
| 2 – Integration (MVP) | 1–2 weeks | All *Must* requirements working together on the mock-up |
| 3 – Extensions and measurements | 1–2 weeks | *Should* requirements; latency, traffic, offline and edge-vs-cloud tests |
| 4 – Finalization | 1 week | Polishing, report, demo preparation |

- [ ] Phase 0 – Contracts
- [ ] Phase 1 – Modules
- [ ] Phase 2 – Integration (MVP)
- [ ] Phase 3 – Extensions and measurements
- [ ] Phase 4 – Finalization
