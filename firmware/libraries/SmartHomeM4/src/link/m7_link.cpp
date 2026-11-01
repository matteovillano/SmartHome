#include "m7_link.h"

#include <Arduino.h>
#include <RPC.h>
#include <config.h>
#include <rpc_protocol.h>

#include "platform/CircularBuffer.h"

namespace {

// Commands arrive in the RPC thread; loop() consumes them. mbed's CircularBuffer
// is protected by a critical section, so it is safe between the two threads.
mbed::CircularBuffer<Command, COMMAND_QUEUE_SIZE> commandQueue;
volatile uint32_t dropped = 0;

void onCommand(int type, int device, int value) {
  // Only enqueue here: driving hardware from the RPC thread would race with loop().
  if (commandQueue.full()) {
    dropped = dropped + 1;
    return;
  }
  commandQueue.push(Command{static_cast<CommandType>(type), static_cast<uint8_t>(device),
                            static_cast<int16_t>(value)});
}

}  // namespace

namespace m7link {

void begin() {
  RPC.bind(RPC_COMMAND_METHOD, onCommand);
  RPC.begin();
}

void sendEvent(const Event& event) {
  // send() is a notification (no reply), so it does not block the M4.
  RPC.send(RPC_EVENT_METHOD, static_cast<int>(event.type), static_cast<int>(event.room),
           static_cast<int>(event.value), static_cast<unsigned int>(event.timestamp));
}

bool pollCommand(Command& out) { return commandQueue.pop(out); }

void log(const char* message) { RPC.println(message); }

uint32_t droppedCommands() { return dropped; }

}  // namespace m7link
