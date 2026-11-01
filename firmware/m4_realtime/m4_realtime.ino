// M4 core: peripherals only. Reads sensors, reports changes to the M7 and
// executes the commands it receives. No decisions are taken here.
#include <SmartHomeM4.h>

void setup() {
  lights::begin();
  // TODO (phase 1): begin() of the other sensor and actuator drivers.
  m7link::begin();
  m7link::log("[m4] started");
}

void loop() {
  // TODO (phase 1): poll sensor drivers and m7link::sendEvent() on change.

  Command command;
  while (m7link::pollCommand(command)) {
    bool handled = false;
    switch (command.type) {
      case LIGHT_SET:
        handled = lights::apply(command);
        break;
      case SHUTTER_SET:  // TODO (phase 1): servo driver
      case RELAY_SET:    // TODO (phase 1): relay driver
        break;
    }
    if (!handled) {
      char line[64];
      snprintf(line, sizeof line, "[m4] ignored command type=%d device=%d", command.type, command.device);
      m7link::log(line);
    }
  }
}
