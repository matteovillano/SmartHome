#pragma once

#include <messages.h>
#include <stdint.h>

// Message on the M7 internal event bus. Never leaves the M7, so it can carry
// more than the M4 <-> M7 Event (e.g. a full Command for remote requests).
enum class BusKind : uint8_t {
  SENSOR_EVENT,     // `event` from the M4 (or the scheduler)
  COMMAND_REQUEST,  // someone asks for `command`; the state manager decides and executes
  STATE_CHANGED,    // `command` holds the new device value, already sent to the M4
};

// Who asked for a command. The override manager (FR7) will use it to let
// manual commands pause automatic rules.
enum class CommandSource : uint8_t { REMOTE, BUTTON, RULE, SCHEDULER };

struct BusMessage {
  BusKind kind;
  CommandSource source;
  Event event;
  Command command;
  uint32_t createdMs;  // millis() on the M7 when the message entered the bus (latency, T1)
};
