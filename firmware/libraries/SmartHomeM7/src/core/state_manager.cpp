#include "state_manager.h"

#include <Arduino.h>
#include <config.h>

#include "../comms/m4_link.h"
#include "event_bus.h"

namespace {

int16_t lightLevels[LIGHT_COUNT] = {};  // the M4 starts with every light off

const char* sourceName(CommandSource source) {
  switch (source) {
    case CommandSource::REMOTE: return "remote";
    case CommandSource::BUTTON: return "button";
    case CommandSource::RULE: return "rule";
    case CommandSource::SCHEDULER: return "scheduler";
  }
  return "?";
}

void publishLight(uint8_t device) {
  BusMessage changed{};
  changed.kind = BusKind::STATE_CHANGED;
  changed.command = Command{LIGHT_SET, device, lightLevels[device - 1]};
  changed.createdMs = millis();
  bus::publish(changed);
}

void handleCommand(const BusMessage& message) {
  const Command& requested = message.command;
  if (requested.type != LIGHT_SET) {
    Serial.println("[state] command type not supported yet");  // shutters, relay: phase 1
    return;
  }
  if (requested.device < 1 || requested.device > LIGHT_COUNT) {
    Serial.println("[state] unknown light id");
    return;
  }
  // TODO (FR7): ask the override manager whether a RULE command is allowed now.
  int16_t value = constrain(requested.value, 0, 100);
  m4link::sendCommand(Command{LIGHT_SET, requested.device, value});
  lightLevels[requested.device - 1] = value;

  // Always confirm, even if unchanged: the web app waits for this state to clear "pending".
  publishLight(requested.device);

  Serial.print("[state] light ");
  Serial.print(requested.device);
  Serial.print(" -> ");
  Serial.print(value);
  Serial.print(" (");
  Serial.print(sourceName(message.source));
  Serial.print(", ");
  Serial.print(millis() - message.createdMs);
  Serial.println(" ms on the bus)");
}

void onBusMessage(const BusMessage& message) {
  switch (message.kind) {
    case BusKind::COMMAND_REQUEST:
      handleCommand(message);
      break;
    case BusKind::SENSOR_EVENT:  // TODO (phase 1): record sensor values
    case BusKind::STATE_CHANGED:
      break;
  }
}

}  // namespace

namespace state {

void begin() { bus::subscribe(onBusMessage); }

int16_t light(uint8_t device) {
  if (device < 1 || device > LIGHT_COUNT) return -1;
  return lightLevels[device - 1];
}

void publishAll() {
  for (uint8_t d = 1; d <= LIGHT_COUNT; ++d) publishLight(d);
}

}  // namespace state
