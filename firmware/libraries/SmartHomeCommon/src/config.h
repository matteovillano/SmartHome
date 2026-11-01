#pragma once

#include <Arduino.h>
#include <stdint.h>

// Single place for rooms, devices, pins and thresholds (no hardcoded values elsewhere).
// PIN ASSIGNMENTS ARE PLACEHOLDERS until the mock-up wiring is fixed (role A).
// Servos and dimmable LEDs need PWM-capable pins; check the breakout pinout.

constexpr uint8_t ROOM_COUNT = 3;

// Sensors: one PIR and one light sensor per room, index = room - 1.
constexpr uint8_t PIR_PINS[ROOM_COUNT] = {D7, D8, D9};
constexpr uint8_t LIGHT_SENSOR_I2C_ADDR[ROOM_COUNT] = {0x23, 0x5C, 0x23};  // TODO: 3rd sensor needs a mux or another bus

// Buttons, index = button id - 1.
constexpr uint8_t BUTTON_COUNT = 4;
constexpr uint8_t BUTTON_PINS[BUTTON_COUNT] = {D10, D11, D12, D13};
constexpr uint8_t BUTTON_ROOMS[BUTTON_COUNT] = {1, 2, 3, 0};  // 0 = whole house

// Actuators, index = device id - 1.
// Target: 6 lights, 2 per room. Until they are wired, the only light is the
// onboard green LED (active LOW), used as light 1 in room 1.
constexpr uint8_t LIGHT_COUNT = 1;
constexpr uint8_t LIGHT_PINS[LIGHT_COUNT] = {LEDG};
constexpr bool LIGHT_ACTIVE_LOW[LIGHT_COUNT] = {true};
constexpr uint8_t LIGHT_ROOMS[LIGHT_COUNT] = {1};

constexpr uint8_t SHUTTER_COUNT = 3;
constexpr uint8_t SHUTTER_PINS[SHUTTER_COUNT] = {D0, D1, D2};
constexpr uint8_t SHUTTER_ROOMS[SHUTTER_COUNT] = {1, 2, 3};
constexpr uint8_t SERVO_CLOSED_DEG = 0;  // hardware limits, enforced on the M4
constexpr uint8_t SERVO_OPEN_DEG = 180;

constexpr uint8_t RELAY_COUNT = 1;
constexpr uint8_t RELAY_PINS[RELAY_COUNT] = {A1};

constexpr uint8_t OLED_I2C_ADDR = 0x3C;

// Event filtering on the M4: only changes are reported (NFR5).
constexpr uint32_t LIGHT_SAMPLE_PERIOD_MS = 500;
constexpr int16_t LIGHT_CHANGE_THRESHOLD_LUX = 10;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 30;
constexpr uint32_t PIR_CLEAR_AFTER_MS = 30000;

// M7 logic.
constexpr uint32_t OVERRIDE_DEFAULT_MS = 15UL * 60UL * 1000UL;

// Queue sizes (static allocation).
constexpr uint8_t EVENT_QUEUE_SIZE = 32;    // on the M7
constexpr uint8_t COMMAND_QUEUE_SIZE = 16;  // on the M4
constexpr uint8_t BUS_QUEUE_SIZE = 32;      // M7 event bus
constexpr uint8_t BUS_MAX_SUBSCRIBERS = 8;
constexpr uint8_t MQTT_OUT_QUEUE_SIZE = 16; // M7 -> network thread

constexpr unsigned long SERIAL_BAUD = 115200;

// Firmware version reported in home/system/status.
constexpr const char* FW_VERSION = "0.1.0";

// Network (M7). The network runs in its own thread so WiFi/MQTT reconnects,
// which block for seconds, never stall the logic loop (NFR1, NFR3).
constexpr const char* MQTT_CLIENT_ID = "shb-board";
constexpr uint32_t MQTT_KEEPALIVE_MS = 15000;          // broker marks the board offline ~1.5x this after a crash
constexpr uint32_t NET_RETRY_MS = 5000;                // wait between WiFi/MQTT connection attempts
constexpr uint32_t NET_POLL_MS = 10;                   // network thread period
constexpr uint32_t STATUS_HEARTBEAT_MS = 5UL * 60UL * 1000UL;
constexpr uint32_t NET_THREAD_STACK_BYTES = 8192;
