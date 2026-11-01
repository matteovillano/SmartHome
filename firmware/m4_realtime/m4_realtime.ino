// M4 core: peripherals only. Reads sensors, reports changes to the M7 and
// executes the commands it receives. No decisions are taken here.
#include <SmartHomeM4.h>

void setup() {
  m7link::begin();
  // TODO (phase 1): begin() of sensor and actuator drivers.
  m7link::log("[m4] started");
}

void loop() {
  // TODO (phase 1): poll sensor drivers and m7link::sendEvent() on change.

  Command command;
  while (m7link::pollCommand(command)) {
    // TODO (phase 1): dispatch to the actuator driver for command.type.
  }
}
