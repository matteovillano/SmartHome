#pragma once

#include <Print.h>
#include <messages.h>

// M7 side of the RPC link. begin() also boots the M4 (RPC.begin() calls bootM4()).
namespace m4link {

void begin();

// Non-blocking. Returns false when no event is waiting.
bool pollEvent(Event& out);

// Non-blocking.
void sendCommand(const Command& command);

// Copies the M4's log text (m7link::log) to `out`. Call from loop().
void forwardM4Log(Print& out);

// Events lost because the queue was full (should stay 0).
uint32_t droppedEvents();

}  // namespace m4link
