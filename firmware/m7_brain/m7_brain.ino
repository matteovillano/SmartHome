// M7 core: all logic and all external communication (WiFi, MQTT, Serial).
#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copy firmware/m7_brain/secrets.example.h to secrets.h and fill it in"
#endif

#include <SmartHomeM7.h>

void setup() {
  Serial.begin(SERIAL_BAUD);  // no while (!Serial): the board must run without USB
  m4link::begin();            // also boots the M4
  // Returns immediately: WiFi and MQTT connect in their own thread, so the
  // house logic below runs even when the network is down (NFR3).
  netlink::begin(netlink::NetConfig{SECRET_WIFI_SSID, SECRET_WIFI_PASS, SECRET_MQTT_HOST,
                                    SECRET_MQTT_PORT, SECRET_MQTT_USER, SECRET_MQTT_PASS});
  Serial.println("[m7] started");
}

void loop() {
  Event event;
  while (m4link::pollEvent(event)) {
    // TODO (phase 1): publish on the event bus.
  }
  m4link::forwardM4Log(Serial);
}
