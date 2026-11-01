#pragma once

#include <messages.h>

// M4 side of the RPC link. The M4 never decides anything: it reports events
// and executes the commands the M7 sends.
namespace m7link {

void begin();

// Non-blocking. Use only for filtered events (on change), never raw samples.
void sendEvent(const Event& event);

// Non-blocking. Returns false when no command is waiting.
bool pollCommand(Command& out);

// The M4 has no Serial: text goes to the M7, which prints it.
void log(const char* message);

// Commands lost because the queue was full (should stay 0).
uint32_t droppedCommands();

}  // namespace m7link
