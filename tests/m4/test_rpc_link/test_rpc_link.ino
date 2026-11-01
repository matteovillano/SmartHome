// RPC link test, M4 side. Flash together with tests/m7/test_rpc_link.
// Every second sends a fake MOTION event (value toggles 1/0). The M7 answers with
// LIGHT_SET device 1, which is shown on the blue LED; the round trip is logged.
#include <SmartHomeM4.h>

constexpr uint32_t SEND_PERIOD_MS = 1000;

uint32_t lastSendMs = 0;
uint32_t sentAtUs = 0;  // micros(): the round trip is below 1 ms
int16_t motion = 0;

void setup() {
  pinMode(LEDB, OUTPUT);
  digitalWrite(LEDB, HIGH);  // active LOW: off
  m7link::begin();
  m7link::log("[m4 test] started");
}

void loop() {
  uint32_t now = millis();
  if (now - lastSendMs >= SEND_PERIOD_MS) {
    lastSendMs = now;
    motion = motion ? 0 : 1;
    sentAtUs = micros();
    m7link::sendEvent(Event{MOTION, 1, motion, now});
  }

  Command command;
  while (m7link::pollCommand(command)) {
    if (command.type == LIGHT_SET && command.device == 1) {
      digitalWrite(LEDB, command.value > 0 ? LOW : HIGH);
    }
    char line[96];
    snprintf(line, sizeof line, "[m4 test] cmd type=%d device=%d value=%d round trip=%lu us dropped=%lu",
             command.type, command.device, command.value, (unsigned long)(micros() - sentAtUs),
             (unsigned long)m7link::droppedCommands());
    m7link::log(line);
  }
}
