#pragma once

#include <stdint.h>

// M4 <-> M7 contract. Change only with whole-team agreement (see README "Contracts").

enum EventType : uint8_t { MOTION, LIGHT_LEVEL, BUTTON, TIME_TICK, REMOTE_COMMAND };
enum CommandType : uint8_t { LIGHT_SET, SHUTTER_SET, RELAY_SET };

// Proposed meaning of the fields per type (to be confirmed by the team):
//   MOTION         room 1..ROOM_COUNT, value 1 = motion detected, 0 = cleared
//   LIGHT_LEVEL    room 1..ROOM_COUNT, value = lux (clamped to INT16_MAX)
//   BUTTON         room = room the button belongs to, value = button id 1..BUTTON_COUNT (sent on press)
//   TIME_TICK      created on the M7 only (scheduler), room 0, value = minute of day 0..1439
//   REMOTE_COMMAND created on the M7 only (MQTT); never travels over RPC
// timestamp = millis() of the core that created the event. The two cores have
// different clocks, so cross-core latency must be measured on a single core.
struct Event {
  EventType type;
  uint8_t room;
  int16_t value;
  uint32_t timestamp;
};

// Sent only M7 -> M4. device = 1-based id within its type (light 1..LIGHT_COUNT, ...).
struct Command {
  CommandType type;
  uint8_t device;
  int16_t value;  // 0-100 (brightness %, shutter open %, relay 0 = off / 100 = on)
};
