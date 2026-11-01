#include "mqtt_bridge.h"

#include <Arduino.h>

#include "../core/event_bus.h"
#include "../core/state_manager.h"
#include "mqtt_topics.h"
#include "net_link.h"

namespace {

void onBusMessage(const BusMessage& message) {
  if (message.kind != BusKind::STATE_CHANGED) return;
  char topic[TOPIC_MAX_LEN];
  if (!formatDeviceTopic(topic, sizeof topic, message.command.type, message.command.device, "state")) return;
  char payload[8];
  snprintf(payload, sizeof payload, "%d", message.command.value);
  // Retained: a web app that opens later still gets the current state.
  // While offline the publish is dropped; publishAll() after reconnect fixes it.
  netlink::publish(topic, payload, /*retain=*/true);
}

// Runs in the network thread: only parses and hands over to the thread-safe bus.
void onSetMessage(const char* topic, const char* payload) {
  CommandType type;
  uint8_t device;
  int16_t value;
  if (!parseSetTopic(topic, type, device) || !parseLevel(payload, value)) {
    netlink::reportError("invalid set message");
    return;
  }
  BusMessage request{};
  request.kind = BusKind::COMMAND_REQUEST;
  request.source = CommandSource::REMOTE;
  request.command = Command{type, device, value};
  request.createdMs = millis();
  bus::publish(request);
}

}  // namespace

namespace mqttbridge {

void begin() {
  bus::subscribe(onBusMessage);
  netlink::setSetHandler(onSetMessage);
}

void loop() {
  if (netlink::takeResyncRequest()) state::publishAll();
}

}  // namespace mqttbridge
