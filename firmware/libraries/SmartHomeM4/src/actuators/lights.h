#pragma once

#include <messages.h>

// Light driver (M4). Executes LIGHT_SET commands; decides nothing.
namespace lights {

// Configures every pin in LIGHT_PINS and switches all lights off.
void begin();

// Applies a LIGHT_SET command. Unknown device ids are ignored (returns false);
// values are clamped to 0..100. Pins are on/off for now: any value > 0 is on.
bool apply(const Command& command);

}  // namespace lights
