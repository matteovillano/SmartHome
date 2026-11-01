#include "event_bus.h"

#include <config.h>

#include "platform/CircularBuffer.h"

namespace {

// CircularBuffer uses a critical section, so push from another thread is safe.
mbed::CircularBuffer<BusMessage, BUS_QUEUE_SIZE> queue;
bus::Handler handlers[BUS_MAX_SUBSCRIBERS];
uint8_t handlerCount = 0;
volatile uint32_t droppedCount = 0;

}  // namespace

namespace bus {

bool subscribe(Handler handler) {
  if (handlerCount >= BUS_MAX_SUBSCRIBERS) return false;
  handlers[handlerCount++] = handler;
  return true;
}

bool publish(const BusMessage& message) {
  if (queue.full()) {
    droppedCount = droppedCount + 1;
    return false;
  }
  queue.push(message);
  return true;
}

void dispatch(uint8_t maxMessages) {
  BusMessage message;
  for (uint8_t n = 0; n < maxMessages && queue.pop(message); ++n) {
    for (uint8_t i = 0; i < handlerCount; ++i) handlers[i](message);
  }
}

uint32_t dropped() { return droppedCount; }

}  // namespace bus
