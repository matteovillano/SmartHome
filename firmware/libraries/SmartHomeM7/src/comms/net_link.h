#pragma once

#include <stdint.h>

// WiFi + MQTT connection of the M7, run in a dedicated thread.
// Keeps itself connected and publishes home/system/status (retained):
// on connect, when the error count changes, and every STATUS_HEARTBEAT_MS.
// Its MQTT Last Will is {"online":false}, so the web app sees crashes and power loss.
namespace netlink {

struct NetConfig {
  const char* wifiSsid;
  const char* wifiPass;
  const char* mqttHost;
  uint16_t mqttPort;
  const char* mqttUser;
  const char* mqttPass;
};

// Starts the network thread and returns immediately. `config` strings must stay
// valid forever (use the SECRET_* literals from secrets.h).
void begin(const NetConfig& config);

// Thread-safe. Counted in the status payload and shown as last_error in the web app.
void reportError(const char* message);

// True while connected to the broker.
bool isOnline();

// Thread-safe, non-blocking: queues a message for the network thread.
// Returns false (message dropped) while offline or when the queue is full.
bool publish(const char* topic, const char* payload, bool retain);

// Called in the NETWORK THREAD for each message on home/+/+/set. The handler must
// only hand data over to thread-safe code (e.g. bus::publish). Set before begin().
using SetHandler = void (*)(const char* topic, const char* payload);
void setSetHandler(SetHandler handler);

// True once after every successful (re)connect; then false until the next one.
bool takeResyncRequest();

}  // namespace netlink
