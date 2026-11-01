#pragma once

#include "bus_message.h"

// Central queue of the M7. Modules publish and subscribe; they never call each other.
// publish() is safe from any thread (e.g. the network thread); dispatch() runs in loop().
namespace bus {

using Handler = void (*)(const BusMessage& message);

// Call during setup only. Returns false when BUS_MAX_SUBSCRIBERS is reached.
bool subscribe(Handler handler);

// Thread-safe, non-blocking. Returns false (and counts a drop) when the queue is full.
bool publish(const BusMessage& message);

// Delivers queued messages to every subscriber. Messages published by a handler
// are delivered in the same call, up to `maxMessages` (keeps loop() responsive).
void dispatch(uint8_t maxMessages);

uint32_t dropped();

}  // namespace bus
