#include "m4_link.h"

#include <Arduino.h>
#include <RPC.h>
#include <config.h>
#include <rpc_protocol.h>

#include "platform/CircularBuffer.h"

namespace {

// Events arrive in the RPC thread; loop() consumes them (see m7_link.cpp on the M4).
mbed::CircularBuffer<Event, EVENT_QUEUE_SIZE> eventQueue;
volatile uint32_t dropped = 0;

void onEvent(int type, int room, int value, unsigned int timestamp) {
  if (eventQueue.full()) {
    dropped = dropped + 1;
    return;
  }
  eventQueue.push(Event{static_cast<EventType>(type), static_cast<uint8_t>(room),
                        static_cast<int16_t>(value), static_cast<uint32_t>(timestamp)});
}

}  // namespace

namespace m4link {

void begin() {
  // Bind before RPC.begin() boots the M4, so its first events are not lost.
  RPC.bind(RPC_EVENT_METHOD, onEvent);
  RPC.begin();
}

bool pollEvent(Event& out) { return eventQueue.pop(out); }

void sendCommand(const Command& command) {
  RPC.send(RPC_COMMAND_METHOD, static_cast<int>(command.type), static_cast<int>(command.device),
           static_cast<int>(command.value));
}

void forwardM4Log(Print& out) {
  while (RPC.available()) {
    out.write(static_cast<uint8_t>(RPC.read()));
  }
}

uint32_t droppedEvents() { return dropped; }

}  // namespace m4link
