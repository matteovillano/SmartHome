#pragma once

#include <stdint.h>

// Single source of truth for device state on the M7. Executes COMMAND_REQUESTs
// (validate, send to the M4, record) and publishes STATE_CHANGED on the bus.
namespace state {

// Subscribes to the bus. Call in setup().
void begin();

// Current value 0..100 of light `device` (1-based), or -1 if the id is unknown.
int16_t light(uint8_t device);

// Publishes STATE_CHANGED for every device, e.g. after an MQTT reconnect so the
// broker's retained states are correct again.
void publishAll();

}  // namespace state
