#include "mqtt_topics.h"

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

// Room of a device, or 0 if the id is unknown.
uint8_t roomOf(CommandType type, uint8_t device) {
  if (device < 1) return 0;
  switch (type) {
    case LIGHT_SET: return device <= LIGHT_COUNT ? LIGHT_ROOMS[device - 1] : 0;
    case SHUTTER_SET: return device <= SHUTTER_COUNT ? SHUTTER_ROOMS[device - 1] : 0;
    case RELAY_SET: return device <= RELAY_COUNT ? 1 : 0;  // TODO: RELAY_ROOMS in config.h
  }
  return 0;
}

const char* kindName(CommandType type) {
  switch (type) {
    case LIGHT_SET: return "light";
    case SHUTTER_SET: return "shutter";
    case RELAY_SET: return "relay";
  }
  return nullptr;
}

}  // namespace

bool formatDeviceTopic(char* out, size_t outLen, CommandType type, uint8_t device, const char* suffix) {
  uint8_t room = roomOf(type, device);
  const char* kind = kindName(type);
  if (room == 0 || kind == nullptr) return false;
  int n = snprintf(out, outLen, "home/room%u/%s%u/%s", room, kind, device, suffix);
  return n > 0 && static_cast<size_t>(n) < outLen;
}

bool parseSetTopic(const char* topic, CommandType& type, uint8_t& device) {
  unsigned room = 0, id = 0;
  char kind[8] = {};
  int consumed = 0;
  if (sscanf(topic, "home/room%u/%7[a-z]%u/set%n", &room, kind, &id, &consumed) != 3) return false;
  if (topic[consumed] != '\0' || id > 255) return false;

  if (strcmp(kind, "light") == 0) type = LIGHT_SET;
  else if (strcmp(kind, "shutter") == 0) type = SHUTTER_SET;
  else if (strcmp(kind, "relay") == 0) type = RELAY_SET;
  else return false;

  device = static_cast<uint8_t>(id);
  return room != 0 && roomOf(type, device) == room;
}

bool parseLevel(const char* payload, int16_t& value) {
  char* end = nullptr;
  long v = strtol(payload, &end, 10);
  if (end == payload || *end != '\0' || v < 0 || v > 100) return false;
  value = static_cast<int16_t>(v);
  return true;
}
