#pragma once

#include <messages.h>
#include <stddef.h>
#include <stdint.h>

// MQTT topics (README "MQTT topics"). The web app uses the same strings.
constexpr const char* TOPIC_STATUS = "home/system/status";
constexpr const char* TOPIC_RULES = "home/system/rules";
constexpr const char* TOPIC_ROUTINES = "home/system/routines";
constexpr const char* TOPIC_SET_FILTER = "home/+/+/set";

constexpr size_t TOPIC_MAX_LEN = 48;
constexpr size_t PAYLOAD_MAX_LEN = 32;

// Device topics: home/room<R>/<kind><N>/<suffix>, N = id within its type (1-based),
// R = its room from config.h. Example: home/room1/light1/state.
// Writes the topic into `out`; false if the device id is unknown.
bool formatDeviceTopic(char* out, size_t outLen, CommandType type, uint8_t device, const char* suffix);

// Parses home/room<R>/<kind><N>/set. False if malformed, unknown, or R is not the device's room.
bool parseSetTopic(const char* topic, CommandType& type, uint8_t& device);

// Parses a set payload: an integer 0..100 with nothing else. False otherwise.
bool parseLevel(const char* payload, int16_t& value);
