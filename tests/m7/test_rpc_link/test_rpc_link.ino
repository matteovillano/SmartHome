// RPC link test, M7 side. Flash together with tests/m4/test_rpc_link, then open
// the serial monitor. Prints every event from the M4, answers each MOTION with
// LIGHT_SET device 1 (on/off) and forwards the M4 log.
#include <SmartHomeM7.h>

void setup() {
  Serial.begin(SERIAL_BAUD);
  m4link::begin();
  Serial.println("[m7 test] started");
}

void loop() {
  Event event;
  while (m4link::pollEvent(event)) {
    Serial.print("[m7 test] event type=");
    Serial.print(event.type);
    Serial.print(" room=");
    Serial.print(event.room);
    Serial.print(" value=");
    Serial.print(event.value);
    Serial.print(" dropped=");
    Serial.println(m4link::droppedEvents());

    if (event.type == MOTION) {
      m4link::sendCommand(Command{LIGHT_SET, 1, static_cast<int16_t>(event.value ? 100 : 0)});
    }
  }
  m4link::forwardM4Log(Serial);
}
