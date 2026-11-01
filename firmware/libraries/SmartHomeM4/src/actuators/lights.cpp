#include "lights.h"

#include <Arduino.h>
#include <config.h>

namespace {

void write(uint8_t index, bool on) {
  // Onboard RGB LEDs are active LOW: XOR the logical state with the polarity.
  digitalWrite(LIGHT_PINS[index], (on != LIGHT_ACTIVE_LOW[index]) ? HIGH : LOW);
}

}  // namespace

namespace lights {

void begin() {
  for (uint8_t i = 0; i < LIGHT_COUNT; ++i) {
    pinMode(LIGHT_PINS[i], OUTPUT);
    write(i, false);
  }
}

bool apply(const Command& command) {
  if (command.type != LIGHT_SET || command.device < 1 || command.device > LIGHT_COUNT) return false;
  int16_t value = constrain(command.value, 0, 100);
  // TODO: PWM dimming once the real LEDs are on PWM-capable pins.
  write(command.device - 1, value > 0);
  return true;
}

}  // namespace lights
