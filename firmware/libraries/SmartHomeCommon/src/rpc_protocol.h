#pragma once

// RPC method names shared by both cores. Each side binds the method it receives.
// Structs are sent as plain int fields because RPC (msgpack) has no adapter for them.
constexpr const char* RPC_EVENT_METHOD = "shb_event";      // M4 -> M7: (type, room, value, timestamp)
constexpr const char* RPC_COMMAND_METHOD = "shb_command";  // M7 -> M4: (type, device, value)
