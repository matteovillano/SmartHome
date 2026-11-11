# Smart Home Brain: code guide

This document explains **how the code works**: what each part does, why it is built that way, and how the parts talk to each other. It describes the code as of the first end-to-end slice (the onboard green LED as "light 1", controlled from the web app).

- The **what and why of the project** (requirements, contracts, team workflow) is in [`README.md`](../README.md).
- **Short rules for AI assistants** are in [`CLAUDE.md`](../CLAUDE.md).

Suggested reading order: sections 1–3 for the big picture, then section 9 (a click followed end to end), then the details of the part you are working on.

---

## Contents

1. [The big picture](#1-the-big-picture)
2. [Repository map](#2-repository-map)
3. [Why the folders look like this (arduino-cli rules)](#3-why-the-folders-look-like-this-arduino-cli-rules)
4. [Shared library: SmartHomeCommon](#4-shared-library-smarthomecommon)
5. [M4 firmware: the body](#5-m4-firmware-the-body)
6. [M7 firmware: the brain](#6-m7-firmware-the-brain)
7. [MQTT broker](#7-mqtt-broker)
8. [Web app](#8-web-app)
9. [A click, followed end to end](#9-a-click-followed-end-to-end)
10. [Boot sequence](#10-boot-sequence)
11. [What happens when things go wrong](#11-what-happens-when-things-go-wrong)
12. [Tests](#12-tests)
13. [Build, flash, debug](#13-build-flash-debug)
14. [How to add a new device type](#14-how-to-add-a-new-device-type)
15. [Known limitations and TODOs](#15-known-limitations-and-todos)
16. [Glossary](#16-glossary)

---

## 1. The big picture

The system has four running pieces:

```
┌─────────────┐  HTTP (page)   ┌──────────────────────────┐
│   Browser   │◄──────────────►│ Mac: python http.server  │  serves webapp/ (static files)
│  (web app)  │                └──────────────────────────┘
│             │  MQTT over WebSocket :9001  ┌──────────────────────────┐
│             │◄───────────────────────────►│ Mac: Mosquitto broker    │
└─────────────┘                             │ auth + ACL (broker/)     │
                                            └────────────▲─────────────┘
                                                         │ MQTT over TCP :1883 (WiFi)
┌────────────────────────── Arduino Portenta H7 ─────────┼───────────────────────┐
│  M7 core (480 MHz) "the brain"                         │                       │
│   ├─ main loop:       event bus, state manager, MQTT bridge                    │
│   ├─ network thread:  WiFi + MQTT client  ─────────────┘                       │
│   └─ RPC thread:      receives events from the M4                              │
│            ▲  Event (sensor changes)            │ Command (do this)            │
│            │            RPC (shared memory)     ▼                              │
│  M4 core (240 MHz) "the body"                                                  │
│   ├─ main loop:  drivers (lights today; sensors, servos, relay later)          │
│   └─ RPC thread: receives commands from the M7                                 │
│                          │ GPIO                                                │
│                          ▼                                                     │
│                   onboard green LED (= light 1, room 1)                        │
└────────────────────────────────────────────────────────────────────────────────┘
```

**The single most important rule:** the **M4 decides nothing**. It drives hardware and reports changes. The **M7 decides everything** and is the only part that talks to the outside world (it alone has WiFi and `Serial`). The web app is only a remote control and a display: the house must keep working without it (offline operation, NFR3).

Three kinds of messages flow through the system:

| Message | Where | Defined in |
|---|---|---|
| `Event`, `Command` | between M4 and M7, over RPC | `SmartHomeCommon/src/messages.h` |
| `BusMessage` | inside the M7 only, on the event bus | `SmartHomeM7/src/core/bus_message.h` |
| MQTT topics + payloads | between M7, broker and web app | README "MQTT topics", `comms/mqtt_topics.*`, `webapp/src/app.js` |

---

## 2. Repository map

```
SmartHome1/
├── README.md                 project specification (requirements, contracts, workflow)
├── CLAUDE.md                 short rules for AI assistants
├── docs/CODE_GUIDE.md        this file
├── .gitignore                secrets.h, broker/passwd, broker/data/, build/
│
├── firmware/
│   ├── libraries/                          ← ALL module code lives here
│   │   ├── SmartHomeCommon/src/            shared by both cores
│   │   │   ├── SmartHomeCommon.h           umbrella header
│   │   │   ├── messages.h                  Event / Command (M4 ↔ M7 contract)
│   │   │   ├── rpc_protocol.h              RPC method names
│   │   │   └── config.h                    pins, rooms, devices, thresholds, queue sizes
│   │   ├── SmartHomeM4/src/                code that runs on the M4
│   │   │   ├── SmartHomeM4.h               umbrella header
│   │   │   ├── link/m7_link.*              M4 side of the RPC link
│   │   │   └── actuators/lights.*          light driver
│   │   └── SmartHomeM7/src/                code that runs on the M7
│   │       ├── SmartHomeM7.h               umbrella header
│   │       ├── core/bus_message.h          BusMessage, BusKind, CommandSource
│   │       ├── core/event_bus.*            the central queue
│   │       ├── core/state_manager.*        source of truth, executes commands
│   │       ├── comms/m4_link.*             M7 side of the RPC link
│   │       ├── comms/net_link.*            WiFi + MQTT network thread
│   │       ├── comms/mqtt_topics.*         topic names, topic/payload parsing
│   │       └── comms/mqtt_bridge.*         event bus ↔ MQTT translation
│   ├── m4_realtime/                        M4 sketch (thin: wires modules together)
│   │   ├── m4_realtime.ino
│   │   └── sketch.yaml                     board = M4
│   └── m7_brain/                           M7 sketch (thin)
│       ├── m7_brain.ino
│       ├── sketch.yaml                     board = M7
│       ├── secrets.example.h               template, committed, placeholders only
│       └── secrets.h                       real WiFi/MQTT credentials, git-ignored
│
├── tests/
│   ├── m4/test_rpc_link/                   M4 half of the RPC round-trip test
│   ├── m4/test_lights/                     light driver test
│   └── m7/test_rpc_link/                   M7 half of the RPC test (also forwards M4 logs)
│
├── broker/
│   ├── mosquitto.conf                      listeners 1883 (TCP) + 9001 (WebSocket), auth
│   ├── acl                                 who may read/write which topics
│   ├── setup-users.sh                      creates accounts in broker/passwd
│   ├── passwd                              password hashes, git-ignored
│   └── data/                               retained messages, git-ignored
│
└── webapp/
    ├── index.html                          page layout: login, status, rooms
    ├── src/app.js                          all the logic
    ├── src/style.css                       styles (light and dark mode)
    ├── vendor/mqtt.min.js                  MQTT.js 5.10.1, kept in the repo for offline use
    ├── dev/fake_board.sh                   fakes the board status for web development
    └── README.md                           how to run the web app
```

---

## 3. Why the folders look like this (arduino-cli rules)

Several design choices look odd until you know how `arduino-cli` builds a sketch. Each rule below was verified by compiling.

1. **A sketch is a folder whose name equals its `.ino` file.** `firmware/m7_brain/m7_brain.ino` is the sketch `m7_brain`.
2. **The sketch is copied to a temporary build folder before compiling.** So `#include "../../something.h"` that leaves the sketch folder **fails**: the relative path no longer exists in the copy.
3. Because of 2, **all real code is in libraries** (`firmware/libraries/…`). Libraries are found wherever they are, so both the main sketches and the test sketches can use the same modules. The `.ino` files only wire modules together.
4. **Libraries are found by a header at the top of their `src/` folder.** That is why each library has an umbrella header (`SmartHomeM4.h`, `SmartHomeM7.h`, `SmartHomeCommon.h`). `#include <core/event_bus.h>` from a sketch is *not* found; `#include <SmartHomeM7.h>` is.
5. **A library cannot include files from a sketch.** So `secrets.h` (inside the `m7_brain` sketch) is included only by `m7_brain.ino`, which passes the values to `netlink::begin()` at runtime.
6. **`firmware/libraries` is not a default library location**, so every compile passes `--libraries firmware/libraries`.
7. **Both cores use the same board.** The M4 is the same FQBN with an option: `arduino:mbed_portenta:envie_m7:target_core=cm4`. Each sketch stores its FQBN in `sketch.yaml`, so you never type it.

Inside a library, files include each other with relative paths (`#include "../core/event_bus.h"`). That is fine because the library folder is not copied.

---

## 4. Shared library: SmartHomeCommon

Used by both cores. If you change something here, **both** firmwares must be rebuilt and re-flashed.

### 4.1 `messages.h`: the M4 ↔ M7 contract

```cpp
enum EventType   : uint8_t { MOTION, LIGHT_LEVEL, BUTTON, TIME_TICK, REMOTE_COMMAND };
enum CommandType : uint8_t { LIGHT_SET, SHUTTER_SET, RELAY_SET };

struct Event   { EventType type; uint8_t room; int16_t value; uint32_t timestamp; };
struct Command { CommandType type; uint8_t device; int16_t value; };  // value 0-100
```

- An **`Event`** says "something happened" (motion, light level change, button press). It goes **M4 → M7** only.
- A **`Command`** says "do this" (set light 1 to 100). It goes **M7 → M4** only.
- `device` is a **1-based id within its type**: light 1, light 2…, shutter 1…. Light 1 and shutter 1 are different devices.
- `value` is always 0–100: brightness %, shutter open %, relay 0 = off / 100 = on.
- `timestamp` is `millis()` of the core that created the event. The two cores have **different clocks**, so never subtract an M4 timestamp from an M7 one. Measure latency on one core.
- The comments in the file propose the meaning of `room` and `value` for each event type. The team still has to confirm them.
- `TIME_TICK` and `REMOTE_COMMAND` are never sent by the M4. Remote commands are handled with `BusMessage` on the M7 instead (section 6.3).

### 4.2 `rpc_protocol.h`

```cpp
constexpr const char* RPC_EVENT_METHOD   = "shb_event";    // M4 -> M7: (type, room, value, timestamp)
constexpr const char* RPC_COMMAND_METHOD = "shb_command";  // M7 -> M4: (type, device, value)
```

RPC is "call a named function on the other core". Each core **binds** (registers) the name it receives, and the other core **sends** to that name. The library's serializer (msgpack) cannot send C++ structs, so the fields are sent as separate `int`s and rebuilt on arrival.

### 4.3 `config.h`: the only place for configuration

Everything that could change with the wiring or with tuning is here: no magic numbers elsewhere.

| Group | Contents | Notes |
|---|---|---|
| Rooms | `ROOM_COUNT = 3` | |
| Sensors | `PIR_PINS`, `LIGHT_SENSOR_I2C_ADDR`, `BUTTON_PINS`, `BUTTON_ROOMS` | **placeholders**, not used by code yet |
| Lights | `LIGHT_COUNT = 1`, `LIGHT_PINS = {LEDG}`, `LIGHT_ACTIVE_LOW = {true}`, `LIGHT_ROOMS = {1}` | the onboard green LED until real LEDs are wired |
| Shutters, relay, OLED | pins, rooms, servo limits | **placeholders** |
| Filtering (M4) | sample period, change threshold, debounce, PIR clear time | for future sensor drivers |
| Queues | `EVENT_QUEUE_SIZE`, `COMMAND_QUEUE_SIZE`, `BUS_QUEUE_SIZE`, `BUS_MAX_SUBSCRIBERS`, `MQTT_OUT_QUEUE_SIZE` | all memory is allocated statically |
| Network (M7) | `FW_VERSION`, `MQTT_CLIENT_ID`, keepalive, retry, poll period, heartbeat, thread stack | |

Arrays are indexed by `id - 1`: `LIGHT_PINS[0]` is light 1.

**Active LOW:** the Portenta's onboard RGB LEDs light up when the pin is LOW. `LIGHT_ACTIVE_LOW` records this per light, so the driver works for both onboard and external LEDs.

---

## 5. M4 firmware: the body

### 5.1 Threads on the M4

| Thread | What runs there |
|---|---|
| main (`loop()`) | drivers, command execution |
| RPC event thread (created by the RPC library) | `onCommand()`, called when the M7 sends a command |

Because a command arrives on a **different thread** than `loop()`, the code never drives hardware from the RPC callback. It puts the command in a queue, and `loop()` takes it out.

### 5.2 `link/m7_link.*`: the M4 side of the RPC link

Public functions (namespace `m7link`):

| Function | What it does |
|---|---|
| `begin()` | binds `shb_command` to `onCommand`, then `RPC.begin()` |
| `sendEvent(event)` | `RPC.send("shb_event", type, room, value, timestamp)`. A *notification*: no reply, does not block |
| `pollCommand(out)` | takes the next command from the queue; `false` if empty. Never blocks |
| `log(text)` | `RPC.println(text)`. The M4 has no `Serial`, so text goes to the M7, which prints it |
| `droppedCommands()` | how many commands were lost because the queue was full (should be 0) |

How it works inside:

```cpp
mbed::CircularBuffer<Command, COMMAND_QUEUE_SIZE> commandQueue;  // 16 commands

void onCommand(int type, int device, int value) {   // RPC thread
  if (commandQueue.full()) { dropped++; return; }
  commandQueue.push(Command{...});                  // loop() pops it later
}
```

`mbed::CircularBuffer` protects `push`/`pop` with a critical section (interrupts briefly disabled), so one thread can push while another pops.

### 5.3 `actuators/lights.*`: the light driver

| Function | What it does |
|---|---|
| `lights::begin()` | sets every light pin as output and switches it off |
| `lights::apply(command)` | rejects anything that is not `LIGHT_SET` or has an unknown id (returns `false`), clamps the value to 0–100, then sets the pin: on if value > 0 |

The polarity trick in `write()`:

```cpp
digitalWrite(pin, (on != LIGHT_ACTIVE_LOW[i]) ? HIGH : LOW);
```

| on | active LOW | pin |
|---|---|---|
| true | false | HIGH |
| false | false | LOW |
| true | true | LOW (onboard LED lights) |
| false | true | HIGH |

There is no dimming yet: the onboard LED pin is used as plain on/off. PWM dimming is a TODO for when real LEDs are on PWM pins.

### 5.4 `m4_realtime.ino`

```cpp
setup():  lights::begin();  m7link::begin();  m7link::log("[m4] started");
loop():   for each command from m7link::pollCommand():
            LIGHT_SET   -> lights::apply()
            SHUTTER_SET -> (TODO servo driver)
            RELAY_SET   -> (TODO relay driver)
            not handled -> log "[m4] ignored command ..."
```

Future sensor drivers go in `loop()` too: read, compare with the last value, and call `m7link::sendEvent()` only when something changed (NFR5: no raw streams).

**The M4 only runs if the M7 starts it.** On the M7, `RPC.begin()` boots the M4. If the M7 firmware does not call it, the M4 code never runs.

---

## 6. M7 firmware: the brain

### 6.1 Threads on the M7 and the concurrency rules

| Thread | Runs | Owns |
|---|---|---|
| **main** (`loop()`) | event bus dispatch, state manager, MQTT bridge, M4 log forwarding | device state (`lightLevels`) |
| **network** (`netThread`, created by `netlink::begin()`) | WiFi connect, MQTT connect/poll/publish, status | the `WiFiClient` and `MqttClient` objects |
| **RPC event thread** (RPC library) | `m4link`'s `onEvent()` when the M4 sends an event | nothing; only pushes to a queue |

Why a separate network thread: `WiFi.begin()` and `mqtt.connect()` **block for seconds**. If they ran in `loop()`, the house logic would freeze whenever WiFi drops. With the thread, the logic keeps running offline (NFR3) and keeps its latency (NFR1).

**Rules that keep this safe:**

- Each object belongs to one thread. Only the network thread touches `mqtt` and `wifiClient`; only the main thread touches `lightLevels`.
- Threads exchange data **only through thread-safe queues and flags**:

| Channel | From → to | Type |
|---|---|---|
| `m4link` event queue | RPC thread → main | `CircularBuffer<Event, 32>` |
| event bus queue | any thread → main | `CircularBuffer<BusMessage, 32>` |
| `netlink` out queue | main → network | `CircularBuffer<OutMessage, 16>` |
| `statusDirty`, `online`, `resyncRequested` | flags | `volatile bool` |
| error count + last error | any → network | protected by `rtos::Mutex` |

- Code running in the network thread (e.g. the MQTT message callback) only **parses and hands over** with `bus::publish()`. It never changes state directly.

### 6.2 `comms/m4_link.*`: the M7 side of the RPC link

The mirror of `m7link` (namespace `m4link`):

| Function | What it does |
|---|---|
| `begin()` | binds `shb_event` to `onEvent`, **then** `RPC.begin()`. Binding first means the M4's first events cannot arrive before anyone listens. `RPC.begin()` also boots the M4 |
| `pollEvent(out)` | next event from the queue, or `false` |
| `sendCommand(command)` | `RPC.send("shb_command", type, device, value)` |
| `forwardM4Log(out)` | copies the text the M4 wrote with `m7link::log()` to `out` (here `Serial`) |
| `droppedEvents()` | events lost because the queue was full |

Measured on the board: a full M4 → M7 → M4 round trip takes **≈ 0.4 ms**.

### 6.3 `core/bus_message.h`: the message on the bus

```cpp
enum class BusKind : uint8_t {
  SENSOR_EVENT,     // `event` from the M4 (or, later, the scheduler)
  COMMAND_REQUEST,  // someone asks for `command`; the state manager decides
  STATE_CHANGED,    // `command` holds the new value, already sent to the M4
};
enum class CommandSource : uint8_t { REMOTE, BUTTON, RULE, SCHEDULER };

struct BusMessage {
  BusKind kind;
  CommandSource source;   // who asked (for the future override manager, FR7)
  Event event;            // used by SENSOR_EVENT
  Command command;        // used by COMMAND_REQUEST and STATE_CHANGED
  uint32_t createdMs;     // M7 millis() when it entered the bus (latency measurement)
};
```

Why not use `Event` with `REMOTE_COMMAND`? A remote command needs a type, a device **and** a value, and `Event` has no room for all three. `BusMessage` never leaves the M7, so it can grow freely without changing the M4 ↔ M7 contract.

Note the difference between **request** and **change**: a `COMMAND_REQUEST` is a wish ("turn light 1 on"); only the state manager turns it into reality and announces a `STATE_CHANGED`. Later, the override manager can refuse a rule's request (FR7) at this point.

### 6.4 `core/event_bus.*`: the central queue

| Function | Thread | What it does |
|---|---|---|
| `bus::subscribe(handler)` | setup only | registers a function to call for every message (max `BUS_MAX_SUBSCRIBERS` = 8) |
| `bus::publish(message)` | **any** | pushes to the queue; `false` and a drop count if full. Never blocks |
| `bus::dispatch(max)` | main | pops up to `max` messages and calls **every** handler for each |
| `bus::dropped()` | any | messages lost |

Every handler receives every message and ignores the kinds it does not care about. A handler may publish while it is being dispatched (the state manager publishes `STATE_CHANGED` while handling `COMMAND_REQUEST`). The new message goes to the back of the queue and is usually delivered in the same `dispatch()` call. The `max` limit stops one call from running forever.

This is what "modules never call each other" (README coding guidelines) means in practice: the state manager does not know the MQTT bridge exists, and the bridge does not know who executes commands.

### 6.5 `core/state_manager.*`: the source of truth

Holds `int16_t lightLevels[LIGHT_COUNT]` (initially 0, matching the M4, which switches all lights off at boot).

On `COMMAND_REQUEST`:

1. If the type is not `LIGHT_SET`: log "not supported yet" and stop (shutters and relay come later).
2. If the device id is unknown: log and stop.
3. *(TODO FR7: ask the override manager if a `RULE` request is allowed.)*
4. Clamp the value to 0–100.
5. `m4link::sendCommand()` → the M4 executes it.
6. Store the new value.
7. Publish `STATE_CHANGED`, **even if the value did not change**. The web app waits for this confirmation to clear its "Sending…" state.
8. Log, e.g. `[state] light 1 -> 100 (remote, 1 ms on the bus)`.

Other functions:

| Function | What it does |
|---|---|
| `state::light(id)` | current value, or -1 for an unknown id |
| `state::publishAll()` | publishes `STATE_CHANGED` for every device; used after an MQTT reconnect |

`SENSOR_EVENT`s are received but not stored yet (TODO when sensors exist).

### 6.6 `comms/mqtt_topics.*`: topic names and parsing

Fixed topics: `home/system/status`, `home/system/rules`, `home/system/routines`, and the subscription filter `home/+/+/set` (`+` matches one level).

Device topics have the form `home/room<R>/<kind><N>/<suffix>`:

| Part | Meaning | Example |
|---|---|---|
| `R` | the device's room, **must match** `config.h` | `room1` |
| `kind` | `light`, `shutter` or `relay` | `light` |
| `N` | 1-based id within the type (= `Command.device`) | `1` |
| `suffix` | `state` (board → web) or `set` (web → board) | `set` |

| Function | What it does |
|---|---|
| `formatDeviceTopic(out, len, type, device, "state")` | builds `home/room1/light1/state`; `false` if the device is unknown |
| `parseSetTopic(topic, type, device)` | parses `home/room1/light1/set`. Rejects: wrong format, unknown kind, unknown id, trailing text, **room that does not match the device** |
| `parseLevel(payload, value)` | accepts only an integer 0–100 with nothing after it (`"100"` ok; `"abc"`, `"50%"`, `"101"` rejected) |

Strict parsing matters: everything that arrives over the network is untrusted input.

### 6.7 `comms/mqtt_bridge.*`: event bus ↔ MQTT

The only module that knows about both the bus and MQTT.

| Direction | Trigger | Action |
|---|---|---|
| board → web | `STATE_CHANGED` on the bus (main thread) | `netlink::publish("home/room1/light1/state", "100", retain=true)` |
| web → board | message on `…/set` (**network thread**) | parse topic and payload → `bus::publish(COMMAND_REQUEST, source REMOTE)`. Invalid → `netlink::reportError("invalid set message")` |
| resync | `mqttbridge::loop()` sees `netlink::takeResyncRequest()` (main) | `state::publishAll()` |

Why `…/state` is **retained**: the broker keeps the last value and hands it to any client that subscribes later. A web page opened now immediately shows the current state.

Why resync after reconnect: while the board was offline, states may have changed (later, rules or buttons will do this offline), and those publishes were dropped. Republishing everything makes the retained values correct again.

### 6.8 `comms/net_link.*`: the network thread

Public functions (namespace `netlink`):

| Function | Thread | What it does |
|---|---|---|
| `begin(config)` | main, setup | stores the credentials and starts the thread. **Returns immediately** |
| `reportError(text)` | any | increments the error count, stores `last_error`, marks the status for publishing |
| `isOnline()` | any | `true` while connected to the broker |
| `publish(topic, payload, retain)` | any | queues a message for the thread. `false` (dropped) while offline or when the queue is full |
| `setSetHandler(fn)` | setup | the function called for each `…/set` message (the bridge's) |
| `takeResyncRequest()` | main | `true` once after each (re)connect |

**The thread loop** (`run()`), every 10 ms:

```
loop forever:
  if not ensureWifi() or not ensureMqtt():
      sleep 5 s (NET_RETRY_MS) and try again
  mqtt.poll()          ← receives messages, sends keepalive pings
  drainOutQueue()      ← sends what other threads queued with publish()
  if status needs publishing, or 5 min since the last one:
      publishStatus()
  sleep 10 ms
```

**`ensureWifi()`**: if connected, nothing to do. Otherwise, if it *was* connected, report "WiFi lost". Then `WiFi.begin()` (blocks a few seconds, which is fine in this thread).

**`ensureMqtt()`**, on every new connection:

1. Set the client id `shb-board`, username and password, and a keepalive of 15 s.
2. **Set the Last Will:** topic `home/system/status`, payload `{"online":false}`, retained. The broker stores it and publishes it **by itself** if the board stops answering (no ping for about 1.5 × keepalive ≈ 22 s). That is how the web app learns about a crash or power loss.
3. `connect()`. On failure, print the error code: `5` = not authorized (wrong password or ACL), `-2` = broker unreachable (wrong IP, broker not running).
4. Register `onMqttMessage` and subscribe to `home/+/+/set`.
5. Set `statusDirty` (publish "online" now, replacing the retained Last Will) and `resyncRequested` (main loop republishes all states).

**`publishStatus()`** builds the JSON:

```json
{"online":true,"uptime_s":48,"errors":0,"last_error":"","fw":"0.1.0"}
```

It is published **retained**: on every connect, whenever the error count changes, and every 5 minutes as a heartbeat. `last_error` passes through `jsonEscape()` so a quote in a message cannot break the JSON. Uptime is not sent every second (NFR5); the web app counts it up between messages.

**`onMqttMessage()`** copies the topic and payload into fixed-size buffers (no heap growth). Messages that are too long are rejected and counted as errors. Valid ones go to the bridge's handler.

### 6.9 `m7_brain.ino`

```cpp
#if __has_include("secrets.h")   // clear error if someone forgot to create secrets.h
#include "secrets.h"
#else
#error "Copy firmware/m7_brain/secrets.example.h to secrets.h and fill it in"
#endif

setup():
  Serial.begin(115200);   // no while(!Serial): must run without USB
  state::begin();         // subscribe to the bus first...
  mqttbridge::begin();    // ...so no message is missed
  m4link::begin();        // also boots the M4
  netlink::begin({SECRET_WIFI_SSID, ...});   // starts the network thread
  
loop():                   // never blocks, never uses delay()
  M4 events  -> bus::publish(SENSOR_EVENT)
  bus::dispatch()         // runs the handlers (state manager, bridge)
  mqttbridge::loop()      // resync after reconnect
  m4link::forwardM4Log(Serial)
```

`Serial` output does not block without USB: the core's `write()` returns immediately when no serial terminal has the port open.

---

## 7. MQTT broker

Mosquitto runs on the Mac. Start it from the `broker/` folder (the config uses relative paths):

```bash
cd broker && ./setup-users.sh     # first time only: asks passwords for "board" and "webadmin"
mosquitto -c mosquitto.conf
```

### 7.1 `mosquitto.conf`

| Setting | Why |
|---|---|
| `allow_anonymous false` + `password_file passwd` | every client must log in (NFR6) |
| `acl_file acl` | per-user topic permissions |
| `listener 1883` / `protocol mqtt` | plain MQTT over TCP for the board |
| `listener 9001` / `protocol websockets` | browsers cannot open TCP sockets, only WebSockets |
| `persistence true`, `data/` | retained messages survive a broker restart |

Mosquitto does not allow comments at the end of a line: comments must be on their own line.

### 7.2 `acl`

```
user board        topic readwrite home/#
user webadmin     topic read      home/#
                  topic write     home/+/+/set, home/system/rules, home/system/routines
```

The web account can **read everything** but **write only commands, rules and routines**. It cannot fake the board's status or device states: the broker drops such messages silently (this was tested).

To add a web user: `./setup-users.sh <name>`, then copy the `webadmin` block in `acl` with the new name and restart Mosquitto.

---

## 8. Web app

A static page: HTML + CSS + one JS file, no framework, no build step, no backend. The browser connects directly to the broker over WebSockets.

```bash
python3 -m http.server 8080 -d webapp    # from the repo root, then open http://localhost:8080
```

### 8.1 Files

| File | Role |
|---|---|
| `index.html` | three sections: login form, "Board status" card, "Rooms" card |
| `src/style.css` | color tokens with a dark-mode variant, layout, the switch control |
| `src/app.js` | everything else |
| `vendor/mqtt.min.js` | MQTT.js 5.10.1, a local copy so the page works on a LAN without internet |

### 8.2 State kept in `app.js`

| Variable | Meaning |
|---|---|
| `client` | the MQTT.js connection, or `null` |
| `everConnected` | `true` after the first successful connect; separates "wrong URL" from "network dropped later" |
| `lastStatus`, `lastStatusAt` | the last status JSON and when it arrived |
| `devices` | `Map` from `"room1/light1"` to `{room, kind, id, value, pendingSince}` |

### 8.3 Flows

**Login** (`connect()`):
- `mqtt.connect(url, {username, password, clientId: random, reconnectPeriod: 3000})`.
- **Connected** → subscribe to `home/system/status` and `home/+/+/state`, show the dashboard.
- **Wrong credentials** (error code 4 or 5) → stop retrying, show "Login refused".
- **Closed before ever connecting** → stop and show "Cannot reach the broker".
- **Dropped after connecting** → MQTT.js keeps retrying every 3 s; the pill shows "Broker unreachable" / "Reconnecting…".
- The page saves only the URL and username in `localStorage`. The password is never stored, and there are no credentials in the code.
- The default URL is `ws://<host that served the page>:9001`, so it works unchanged when page and broker are on the same machine.

**Status** (`onStatusMessage()` → `renderStatus()`):
- Shows online/offline, errors, firmware, last update and last error.
- Uptime is extrapolated: `uptime_s + seconds since the message arrived`, refreshed every second.

**Rooms** (`onStateMessage()` → `renderRooms()`):
- Devices are **discovered** from the retained `…/state` topics (regex `^home/room(\d+)/(light|shutter|relay)(\d+)/state$`). The page has no copy of `config.h`: when the board adds a light, it appears automatically.
- `renderRooms()` rebuilds the cards each time (there are only a few devices). It uses `textContent`, never `innerHTML`, so data from the network cannot inject HTML.
- Switches are disabled while the board is offline.

**Sending a command** (`sendSet()`):
1. Mark the device `pendingSince = now` → the switch dims and shows "Sending…".
2. Publish `home/room1/light1/set` with `"100"` or `"0"`, QoS 1, **not retained** (a retained command would be replayed to the board later).
3. When the matching `…/state` arrives: clear pending and show "Last command confirmed in N ms". This is the full round trip web → broker → M7 → M4 → M7 → broker → web, so it can be compared with the NFR2 target (≤ 500 ms).
4. If no confirmation arrives within 3 s (`COMMAND_TIMEOUT_MS`): clear pending and warn that the command may not have been executed.

### 8.4 `dev/fake_board.sh`

Publishes a fake `home/system/status` every 30 s using the `board` account, and `{"online":false}` on Ctrl+C. Useful to work on the web app without the Portenta. It does not fake device states.

---

## 9. A click, followed end to end

You click the switch for *Room 1 → Light 1* (currently off):

| # | Where | Thread | What happens | Code |
|---|---|---|---|---|
| 1 | browser | JS | marks pending, publishes `home/room1/light1/set` = `"100"` | `app.js` `sendSet()` |
| 2 | broker | | checks the ACL (webadmin may write `home/+/+/set`), forwards to subscribers | `broker/acl` |
| 3 | M7 | network | `mqtt.poll()` receives it → `onMqttMessage()` copies topic and payload | `net_link.cpp` |
| 4 | M7 | network | bridge parses topic (light 1, room 1 ✓) and payload (100 ✓) → `bus::publish(COMMAND_REQUEST, REMOTE)` | `mqtt_bridge.cpp` `onSetMessage()` |
| 5 | M7 | main | `bus::dispatch()` delivers it to the state manager | `event_bus.cpp` |
| 6 | M7 | main | state manager validates, clamps, `m4link::sendCommand(LIGHT_SET, 1, 100)`, stores 100, publishes `STATE_CHANGED`, logs | `state_manager.cpp` |
| 7 | M4 | RPC | `onCommand()` puts the command in the queue | `m7_link.cpp` |
| 8 | M4 | main | `pollCommand()` → `lights::apply()` → `digitalWrite(LEDG, LOW)` → **LED on** | `m4_realtime.ino`, `lights.cpp` |
| 9 | M7 | main | same `dispatch()` call: the bridge sees `STATE_CHANGED` → `netlink::publish("home/room1/light1/state", "100", retained)` | `mqtt_bridge.cpp` |
| 10 | M7 | network | `drainOutQueue()` sends it to the broker | `net_link.cpp` |
| 11 | browser | JS | `onStateMessage()` → value 100, pending cleared, "confirmed in N ms" | `app.js` |

Steps 6–8 run in parallel with 9–10: the board confirms the state as soon as it has sent the command to the M4 (an on-chip RPC send, which is reliable).

Measured on the board: steps 3–6 take about **1 ms**. Most of the round trip shown in the web app is WiFi and network time.

---

## 10. Boot sequence

After power-on (USB or a charger; USB data is not needed):

| Time | M7 | M4 |
|---|---|---|
| 0 s | `setup()`: Serial, state manager, bridge subscribe to the bus | not running |
| | `m4link::begin()` → `RPC.begin()` **boots the M4** | `setup()`: all lights off, link ready, logs "[m4] started" |
| | `netlink::begin()` starts the network thread; `setup()` ends | `loop()` waiting for commands |
| | `loop()` runs: the house logic works **already, without network** | |
| ~5–20 s | network thread: WiFi connected | |
| | MQTT connected → status "online" (replaces the retained Last Will) → resync: all `…/state` republished | |
| | web app: "Board online", switches enabled | |

Measured: about 20–25 s from power-on to "online" in the web app, almost all of it WiFi association.

---

## 11. What happens when things go wrong

| Failure | What the code does | What you see |
|---|---|---|
| WiFi drops | network thread: `ensureWifi()` fails → retry every 5 s; reports "WiFi lost" once reconnected | serial: `[net] connecting to WiFi...`; web: "Board offline" after ~22 s (Last Will), then online again with `errors` +1 |
| Broker stopped | `ensureMqtt()` fails with `-2`, retries every 5 s; logic keeps running | serial: `MQTT connection failed, error -2`; web: "Broker unreachable", then reconnects by itself |
| Wrong MQTT password | `connect()` fails with `5`, retries every 5 s | serial: `error 5` |
| Board crashes or loses power | broker gets no ping for ~22 s → publishes the Last Will `{"online":false}` | web: "Board offline", switches disabled |
| Invalid `…/set` (wrong room, bad payload, unknown id) | bridge rejects it, `reportError("invalid set message")` | web: `errors` +1, "Last error: invalid set message"; serial: `[net] invalid set message` |
| Message too long | rejected in `onMqttMessage()` | `errors` +1 |
| Command not confirmed in 3 s | web clears pending | web: "No confirmation from the board…" |
| A queue is full | message dropped and counted (`droppedEvents`, `droppedCommands`, `bus::dropped`, `droppedOut`) | nothing yet: the counters are not shown anywhere (TODO) |
| `secrets.h` missing | `#error` at compile time | build error with instructions |
| Broker IP changed (Mac on another network) | `-2` forever | update `SECRET_MQTT_HOST` in `secrets.h`, re-flash the M7 |

---

## 12. Tests

There is no unit-test framework. Each test is a small sketch flashed to the board, and you read its output on the M7's serial monitor.

| Test | Core | What it checks | How to read the result |
|---|---|---|---|
| `tests/m4/test_rpc_link` + `tests/m7/test_rpc_link` | both | M4 sends a fake MOTION every second, M7 answers with LIGHT_SET, M4 sets the blue LED and measures the round trip | serial: `round trip=399 us dropped=0`; blue LED blinks every second |
| `tests/m4/test_lights` | M4 | turns each light on and off every 500 ms | green LED blinks; serial: `light 1 -> 100 ok` (needs an M7 sketch that calls `RPC.begin()` and forwards logs, e.g. `m7_brain` or `tests/m7/test_rpc_link`) |

End-to-end checks done by hand (with the broker running):

```bash
# watch states
mosquitto_sub -h localhost -u board -P <pw> -t 'home/+/+/state' -v
# send a command (the board account may also write)
mosquitto_pub -h localhost -u board -P <pw> -t home/room1/light1/set -m 100
```

**After running a test, re-flash the real firmware on both cores.**

---

## 13. Build, flash, debug

From the repo root (the port may differ: check `arduino-cli board list`):

```bash
PORT=/dev/cu.usbmodem101

# compile only
arduino-cli compile --libraries firmware/libraries firmware/m7_brain
arduino-cli compile --libraries firmware/libraries firmware/m4_realtime

# compile + flash (each core keeps its own flash area: flash only what changed)
arduino-cli compile --upload -p $PORT --libraries firmware/libraries firmware/m4_realtime
arduino-cli compile --upload -p $PORT --libraries firmware/libraries firmware/m7_brain

# serial log (M7 + forwarded M4 lines)
arduino-cli monitor -p $PORT -c baudrate=115200
```

If the port disappears or an upload fails, double-press the reset button (the green LED pulses: bootloader mode) and try again.

**Reading the serial log:** every line starts with its source.

| Prefix | Source |
|---|---|
| `[m7]` | `m7_brain.ino` |
| `[net]` | network thread (`net_link.cpp`) |
| `[state]` | state manager |
| `[m4]` | M4, forwarded through RPC |
| `[m4 test]`, `[m7 test]` | test sketches |

**IDE support** (code completion in VS Code with clangd, etc.):

```bash
arduino-cli compile --libraries firmware/libraries --build-path build/m7_brain --only-compilation-database firmware/m7_brain
```

Then point the IDE at `build/m7_brain/compile_commands.json`. Use the M4 database for M4 code: the two differ (`CORE_CM4` vs `CORE_CM7`).

**Dependencies:** the Portenta core `arduino:mbed_portenta` 4.6.0 and `ArduinoMqttClient@0.1.8` (`arduino-cli lib install "ArduinoMqttClient@0.1.8"`).

---

## 14. How to add a new device type

The relay as an example. The same pattern applies to shutters (with a servo driver instead).

1. **`config.h`**: set the real `RELAY_PINS`, and add `RELAY_ROOMS` and, if needed, `RELAY_ACTIVE_LOW`.
2. **M4 driver**: create `SmartHomeM4/src/actuators/relay.h/.cpp` with `begin()` and `apply(const Command&)`, like `lights`. Include it in `SmartHomeM4.h`.
3. **`m4_realtime.ino`**: call `relay::begin()` in `setup()`, and handle `case RELAY_SET:` in the switch.
4. **State manager**: add `relayStates[RELAY_COUNT]`, handle `RELAY_SET` in `handleCommand()` (validate, send to the M4, store, publish `STATE_CHANGED`), and add the relay to `publishAll()`.
5. **`mqtt_topics.cpp`**: `roomOf()` already knows `RELAY_SET`; replace its `TODO` with `RELAY_ROOMS`.
6. **Web app**: nothing to do. The relay appears automatically from `home/roomX/relay1/state` with an on/off switch. (Shutters will need a slider, since they use 0–100.)
7. **Test**: add `tests/m4/test_relay/`, flash it, then flash the real firmware and try it from the web app.
8. **Docs**: update the README status checklist.

For a **sensor**, the direction is reversed: the M4 driver reads and calls `m7link::sendEvent()` on change; the M7 receives a `SENSOR_EVENT` on the bus; the state manager stores it; the bridge (to be extended) publishes `home/roomX/sensor/<type>`.

---

## 15. Known limitations and TODOs

| Area | Status |
|---|---|
| Pins in `config.h` | placeholders except the onboard LED; wiring not decided |
| Light sensors | two of the three share an I2C address: needs a mux, a second bus or another model |
| Dimming | lights are on/off; PWM needs PWM-capable pins |
| Shutters, relay, PIR, light sensors, buttons, OLED | no drivers yet |
| Rule engine, scheduler, override manager (FR4, FR5, FR7) | not started; the state manager has a TODO where the override check goes |
| Persistence (FR10), offline buffer (FR11) | not started; while offline, state publishes are dropped (the resync fixes states, but events will need the buffer) |
| Sensor topics `home/<room>/sensor/<type>` | not published yet |
| Drop counters | counted but not reported in the status |
| Watchdog (NFR4) | not implemented |
| Broker IP | fixed in `secrets.h`; use a DHCP reservation for the Mac |
| TLS | not used; fine on a home LAN, needed if the broker is reachable from outside |
| Event field meanings, status payload | proposed; need team agreement (contract) |

---

## 16. Glossary

| Term | Meaning |
|---|---|
| **M7 / M4** | the two processor cores of the Portenta H7 (Cortex-M7 480 MHz, Cortex-M4 240 MHz). Each runs its own firmware |
| **RPC** | Remote Procedure Call: one core calls a named function on the other, through shared memory |
| **FQBN** | Fully Qualified Board Name, e.g. `arduino:mbed_portenta:envie_m7`; tells arduino-cli what to compile for |
| **Sketch** | an Arduino program: a folder with a `.ino` of the same name |
| **Umbrella header** | one header that includes all of a library's headers, so arduino-cli can find the library |
| **Active LOW** | the device is on when the pin is LOW (e.g. the onboard RGB LEDs) |
| **Thread** | an independent flow of execution on the same core (Mbed OS provides them) |
| **Critical section / mutex** | ways to stop two threads from changing the same data at once |
| **Circular buffer** | a fixed-size queue that reuses its memory; no heap allocation |
| **MQTT** | a publish/subscribe protocol: clients publish messages on *topics*, and the broker forwards them to clients subscribed to those topics |
| **Broker** | the MQTT server (Mosquitto) that routes messages |
| **Topic filter** | a subscription pattern; `+` matches one level, `#` matches everything below |
| **Retained message** | the broker keeps the last message on a topic and sends it to anyone who subscribes later |
| **QoS 1** | "at least once" delivery: the receiver acknowledges, the sender retries |
| **Last Will (LWT)** | a message the client gives the broker at connect time; the broker publishes it if the client disappears without disconnecting |
| **Keepalive** | the maximum silence between client and broker; the client pings to stay alive |
| **ACL** | Access Control List: which user may read/write which topics |
| **WebSocket** | a persistent two-way connection a browser can open; MQTT runs on top of it for the web app |
| **NFR / FR** | non-functional / functional requirement IDs from the README |
