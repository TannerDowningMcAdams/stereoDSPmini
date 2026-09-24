#include "engine_registry.hpp"
#include "passthrough_engine.hpp"
#include "test_delay_engine.hpp"

namespace {

// One static instance per engine type.
template <typename E>
E instance;

template <typename... E>
struct EngineList
{
    static constexpr uint8_t           kCount     = sizeof...(E);
    static constexpr const EngineInfo* kInfo[]    = { &E::kInfo... };
    static constexpr Engine*           kEngines[] = { &instance<E>... };
};

// Selection order. Index 0 is the default engine.
using Registry = EngineList<
    TestDelayEngine,
    PassthroughEngine
>;

constexpr bool infoValid(const EngineInfo& info)
{
    if (info.id == ENGINE_ID_NONE) { return false; }

    uint8_t fields = 0u;
    uint8_t bits   = 0u;
    for (uint8_t i = 0u; i < ENGINE_MAX_DISCRETE_FIELDS; i++)
    {
        const uint8_t width = info.fieldWidth[i];
        if (width == 0u || fields < i)
        {
            // The first 0 ends the list; nothing may follow it.
            if (width != 0u || info.fieldDefault[i] != 0u) { return false; }
            continue;
        }
        if (width > ENGINE_MAX_FIELD_WIDTH)                  { return false; }
        if (info.fieldDefault[i] >= (1u << width))           { return false; }
        fields++;
        bits = static_cast<uint8_t>(bits + width);
    }
    if (bits > 16u) { return false; }

    if (info.modeOptions > ENGINE_MAX_MODE_OPTIONS) { return false; }
    if (info.modeOptions > 0u &&
        (info.fieldWidth[ENGINE_MODE_FIELD] != ENGINE_MODE_FIELD_WIDTH ||
         info.fieldDefault[ENGINE_MODE_FIELD] >= info.modeOptions)) { return false; }

    if (info.auxRole > AUX_ROLE_TOGGLE) { return false; }
    if (info.auxRole == AUX_ROLE_TOGGLE &&
        (info.auxField >= fields || info.fieldWidth[info.auxField] != 1u)) { return false; }

    if (info.blendParam != kEngineParamNone && info.blendParam >= SPI_PARAM_COUNT) { return false; }
    return true;
}

constexpr bool allValid()
{
    for (const EngineInfo* info : Registry::kInfo)
    {
        if (!infoValid(*info)) { return false; }
    }
    return true;
}

constexpr bool idsUnique()
{
    for (uint8_t i = 0u; i < Registry::kCount; i++)
    {
        for (uint8_t j = static_cast<uint8_t>(i + 1u); j < Registry::kCount; j++)
        {
            if (Registry::kInfo[i]->id == Registry::kInfo[j]->id) { return false; }
        }
    }
    return true;
}

constexpr bool arenasFit()
{
    for (const EngineInfo* info : Registry::kInfo)
    {
        if (info->fastBytes > EngineArenas::kFastBytes)   { return false; }
        if (info->largeBytes > EngineArenas::kLargeBytes) { return false; }
    }
    return true;
}

static_assert(Registry::kCount >= 1u && Registry::kCount < EngineRegistry::kNotFound, "registry size");
static_assert(allValid(),  "an EngineInfo is invalid: see infoValid()");
static_assert(idsUnique(), "engine ids must be unique");
static_assert(arenasFit(), "an engine's worst case does not fit an arena");

}

uint8_t EngineRegistry::count()
{
    return Registry::kCount;
}

Engine* EngineRegistry::at(uint8_t index)
{
    return (index < Registry::kCount) ? Registry::kEngines[index] : nullptr;
}

uint16_t EngineRegistry::idAt(uint8_t index)
{
    return (index < Registry::kCount) ? Registry::kInfo[index]->id : static_cast<uint16_t>(ENGINE_ID_NONE);
}

uint8_t EngineRegistry::indexOf(uint16_t id)
{
    for (uint8_t i = 0u; i < Registry::kCount; i++)
    {
        if (Registry::kInfo[i]->id == id) { return i; }
    }
    return kNotFound;
}
