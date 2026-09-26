#pragma once

#include "engine.hpp"
#include <cstdint>

// The engines this H7 build offers, in selection order (plan §4.4). Index 0 is the
// default engine: the one the H7 boots with and a new unit takes. The table and its
// compile-time checks are in engine_registry.cpp.
//
// Adding an engine: its header and source, an id in engine_ids.hpp, its type in the
// list in engine_registry.cpp, and its sources in H7/CMakeLists.txt (and in
// DSPminiHost's sync.sh when it has host tests).
class EngineRegistry {
public:

    static constexpr uint8_t kNotFound = 0xFFu;

    static uint8_t count();
    // nullptr past the end.
    static Engine* at(uint8_t index);
    // ENGINE_ID_NONE past the end.
    static uint16_t idAt(uint8_t index);
    // Registry index of an id, or kNotFound.
    static uint8_t indexOf(uint16_t id);
};
