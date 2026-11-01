#include "net_link.h"

#include <Arduino.h>
#include <ArduinoMqttClient.h>
#include <WiFi.h>
#include <config.h>
#include <mbed.h>

#include "mqtt_topics.h"
#include "platform/CircularBuffer.h"

namespace {

struct OutMessage {
  char topic[TOPIC_MAX_LEN];
  char payload[PAYLOAD_MAX_LEN];
  bool retain;
};

constexpr size_t LAST_ERROR_LEN = 48;
constexpr size_t STATUS_JSON_LEN = 192;
constexpr const char* OFFLINE_PAYLOAD = "{\"online\":false}";

netlink::NetConfig cfg;
rtos::Thread netThread(osPriorityNormal, NET_THREAD_STACK_BYTES, nullptr, "net");

// Only the network thread touches these two.
WiFiClient wifiClient;
MqttClient mqtt(wifiClient);

// Shared with other threads through reportError()/isOnline().
rtos::Mutex errorMutex;
uint32_t errorCount = 0;
char lastError[LAST_ERROR_LEN] = "";
volatile bool statusDirty = false;
volatile bool online = false;
volatile bool resyncRequested = false;

// Producers: any thread. Consumer: the network thread.
mbed::CircularBuffer<OutMessage, MQTT_OUT_QUEUE_SIZE> outQueue;
volatile uint32_t droppedOut = 0;
netlink::SetHandler setHandler = nullptr;

void log(const char* message) {
  Serial.print("[net] ");
  Serial.println(message);
}

// Copies `in` into a JSON string body, escaping the characters JSON forbids.
void jsonEscape(const char* in, char* out, size_t outLen) {
  size_t o = 0;
  for (; *in && o + 2 < outLen; ++in) {
    char c = *in;
    if (c == '"' || c == '\\') {
      out[o++] = '\\';
      out[o++] = c;
    } else if (static_cast<unsigned char>(c) >= 0x20) {
      out[o++] = c;
    }
  }
  out[o] = '\0';
}

bool publishStatus() {
  char escaped[LAST_ERROR_LEN * 2];
  uint32_t errors;
  {
    rtos::ScopedMutexLock lock(errorMutex);
    errors = errorCount;
    jsonEscape(lastError, escaped, sizeof escaped);
  }
  char payload[STATUS_JSON_LEN];
  snprintf(payload, sizeof payload,
           "{\"online\":true,\"uptime_s\":%lu,\"errors\":%lu,\"last_error\":\"%s\",\"fw\":\"%s\"}",
           static_cast<unsigned long>(millis() / 1000), static_cast<unsigned long>(errors), escaped, FW_VERSION);

  if (!mqtt.beginMessage(TOPIC_STATUS, /*retain=*/true, /*qos=*/1)) return false;
  mqtt.print(payload);
  if (!mqtt.endMessage()) return false;
  statusDirty = false;
  return true;
}

// Called by mqtt.poll() (network thread) for every incoming message.
void onMqttMessage(int size) {
  char topic[TOPIC_MAX_LEN];
  char payload[PAYLOAD_MAX_LEN];
  String t = mqtt.messageTopic();
  strncpy(topic, t.c_str(), sizeof topic - 1);
  topic[sizeof topic - 1] = '\0';

  size_t n = 0;
  while (mqtt.available()) {
    int c = mqtt.read();
    if (n < sizeof payload - 1) payload[n++] = static_cast<char>(c);
  }
  payload[n] = '\0';

  if (size >= static_cast<int>(sizeof payload) || t.length() >= sizeof topic) {
    netlink::reportError("MQTT message too long");
    return;
  }
  if (setHandler) setHandler(topic, payload);
}

void drainOutQueue() {
  OutMessage message;
  while (outQueue.pop(message)) {
    if (!mqtt.beginMessage(message.topic, message.retain, /*qos=*/1)) return;
    mqtt.print(message.payload);
    if (!mqtt.endMessage()) return;
  }
}

bool ensureWifi() {
  static bool wasConnected = false;
  if (WiFi.status() == WL_CONNECTED) return true;

  if (wasConnected) {
    wasConnected = false;
    netlink::reportError("WiFi lost");
  }
  log("connecting to WiFi...");
  // Blocks for a few seconds: acceptable only because this is the network thread.
  if (WiFi.begin(cfg.wifiSsid, cfg.wifiPass) != WL_CONNECTED) {
    log("WiFi connection failed");
    return false;
  }
  wasConnected = true;
  Serial.print("[net] WiFi connected, IP ");
  Serial.println(WiFi.localIP());
  return true;
}

bool ensureMqtt() {
  static bool wasConnected = false;
  if (mqtt.connected()) return true;

  online = false;
  if (wasConnected) {
    wasConnected = false;
    mqtt.stop();
    netlink::reportError("MQTT connection lost");
  }

  mqtt.setId(MQTT_CLIENT_ID);
  mqtt.setUsernamePassword(cfg.mqttUser, cfg.mqttPass);
  mqtt.setKeepAliveInterval(MQTT_KEEPALIVE_MS);
  // The Last Will must be set before every connect.
  mqtt.beginWill(TOPIC_STATUS, /*retain=*/true, /*qos=*/1);
  mqtt.print(OFFLINE_PAYLOAD);
  mqtt.endWill();

  log("connecting to MQTT broker...");
  if (!mqtt.connect(cfg.mqttHost, cfg.mqttPort)) {
    // 5 = not authorized (wrong user/password or ACL), -2 = broker unreachable.
    Serial.print("[net] MQTT connection failed, error ");
    Serial.println(mqtt.connectError());
    return false;
  }
  mqtt.onMessage(onMqttMessage);
  if (!mqtt.subscribe(TOPIC_SET_FILTER, /*qos=*/1)) {
    netlink::reportError("MQTT subscribe failed");
  }
  wasConnected = true;
  online = true;
  statusDirty = true;      // replace the retained "offline" Last Will right away
  resyncRequested = true;  // the main loop republishes every device state
  log("MQTT connected");
  return true;
}

void run() {
  uint32_t lastStatusMs = 0;
  for (;;) {
    if (!ensureWifi() || !ensureMqtt()) {
      rtos::ThisThread::sleep_for(std::chrono::milliseconds(NET_RETRY_MS));
      continue;
    }
    mqtt.poll();
    drainOutQueue();

    uint32_t now = millis();
    if (statusDirty || now - lastStatusMs >= STATUS_HEARTBEAT_MS) {
      if (publishStatus()) lastStatusMs = now;
    }

    rtos::ThisThread::sleep_for(std::chrono::milliseconds(NET_POLL_MS));
  }
}

}  // namespace

namespace netlink {

void begin(const NetConfig& config) {
  cfg = config;
  netThread.start(mbed::callback(run));
}

void reportError(const char* message) {
  {
    rtos::ScopedMutexLock lock(errorMutex);
    errorCount++;
    strncpy(lastError, message, LAST_ERROR_LEN - 1);
    lastError[LAST_ERROR_LEN - 1] = '\0';
  }
  statusDirty = true;
  log(message);
}

bool isOnline() { return online; }

bool publish(const char* topic, const char* payload, bool retain) {
  if (!online || outQueue.full()) {
    droppedOut = droppedOut + 1;
    return false;
  }
  OutMessage message{};
  strncpy(message.topic, topic, sizeof message.topic - 1);
  strncpy(message.payload, payload, sizeof message.payload - 1);
  message.retain = retain;
  outQueue.push(message);
  return true;
}

void setSetHandler(SetHandler handler) { setHandler = handler; }

bool takeResyncRequest() {
  if (!resyncRequested) return false;
  resyncRequested = false;
  return true;
}

}  // namespace netlink
