#pragma once

#include <cstdint>

// Stable engine ids (plan §4.4). Presets store them, so an id is assigned by hand and
// never reused, even after its engine is removed. 0x0000 is ENGINE_ID_NONE.
namespace engine_id {

static constexpr uint16_t kPassthrough = 0x0001u;
static constexpr uint16_t kTestDelay   = 0x0002u;

}
