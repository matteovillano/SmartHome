// Light driver test (M4). Turns every light on and off in turn, every 500 ms.
// The M4 runs only while an M7 sketch calls RPC.begin(): keep firmware/m7_brain
// or tests/m7/test_rpc_link on the M7 (its serial monitor shows this log).
#include <SmartHomeM4.h>

constexpr uint32_t STEP_MS = 500;

uint32_t lastStepMs = 0;
uint8_t step = 0;

void setup() {
  lights::begin();
  m7link::begin();
  m7link::log("[m4 test] lights started");
}

void loop() {
  uint32_t now = millis();
  if (now - lastStepMs < STEP_MS) return;
  lastStepMs = now;

  uint8_t device = step / 2 + 1;
  int16_t value = (step % 2 == 0) ? 100 : 0;
  bool ok = lights::apply(Command{LIGHT_SET, device, value});

  char line[48];
  snprintf(line, sizeof line, "[m4 test] light %d -> %d %s", device, value, ok ? "ok" : "REJECTED");
  m7link::log(line);
  step = (step + 1) % (LIGHT_COUNT * 2);
}
