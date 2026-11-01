#pragma once

// Translates between the event bus and MQTT:
//   STATE_CHANGED on the bus      -> retained home/room<R>/<device>/state
//   home/room<R>/<device>/set     -> COMMAND_REQUEST (source REMOTE) on the bus
namespace mqttbridge {

// Subscribes to the bus and registers the incoming-message handler. Call before netlink::begin().
void begin();

// Call from loop(): republishes every device state after each (re)connect.
void loop();

}  // namespace mqttbridge
