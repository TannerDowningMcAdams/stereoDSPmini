#pragma once

#include "audio_buffer.hpp"
#include "engine_arena.hpp"
#include "engine_link.h"
#include "spi_protocol.h"
#include <array>
#include <cstdint>

// The engine model of plan §4.4. An engine is one effect or one fixed signal chain.
// Engines are static instances that are never constructed or destroyed at runtime:
// activate() and deactivate() take the place of both. TestDelayEngine is the template,
// and plan §4.4 "Writing an engine" lists the rules.

static constexpr uint8_t  kEngineParamNone = 0xFFu;
static constexpr uint16_t kParamHalfScale  = 0x8000u;

using ParamDefaults = std::array<uint16_t, SPI_PARAM_COUNT>;

// The listed params in order, and half scale for the rest. A partial array literal
// would fill the rest with 0.
template <typename... T>
constexpr ParamDefaults paramDefaults(T... values)
{
    static_assert(sizeof...(T) <= SPI_PARAM_COUNT, "more defaults than params");
    const uint16_t listed[] = { static_cast<uint16_t>(values)..., 0u };
    ParamDefaults defaults {};
    for (uint32_t i = 0u; i < SPI_PARAM_COUNT; i++)
    {
        defaults[i] = (i < sizeof...(T)) ? listed[i] : kParamHalfScale;
    }
    return defaults;
}

// One discrete field, 1..3 bits wide. Fields pack from bit 0 in list order, and
// {0, 0} ends the list.
struct FieldInfo
{
    uint8_t width;
    uint8_t defaultValue;
};

// Everything the H7 knows about an engine without running it. Written with designated
// initializers in declaration order; members left out take the defaults here.
struct EngineInfo
{
    uint16_t      id           = ENGINE_ID_NONE;    // engine_ids.hpp; never reused
    uint8_t       modeOptions  = 0;                 // 0..4 values of field 0, the mode field
    uint8_t       auxRole      = AUX_ROLE_NONE;
    uint8_t       auxField     = 0;                 // the 1-bit field AUX toggles
    bool          usesTempo    = false;
    FieldInfo     fields[ENGINE_MAX_DISCRETE_FIELDS] = {};
    ParamDefaults paramDefault = paramDefaults();   // normalized 0..65535
    // The param the bypass controller mixes dry against wet by, or kEngineParamNone to
    // run fully wet. Analog dry skips the converters and the DMA, so it leads the wet
    // signal by about 2 ms and comb-filters with any dry signal inside the wet one.
    // Delays and reverbs are fine; phasers, flangers, chorus and parallel filters mix
    // internally and declare no blend param.
    uint8_t       blendParam   = kEngineParamNone;
    uint32_t      fastBytes    = 0;                 // worst case per arena, in arenaBytes() units
    uint32_t      largeBytes   = 0;

    constexpr uint32_t descriptor() const
    {
        uint8_t widths[ENGINE_MAX_DISCRETE_FIELDS] = {};
        for (uint8_t i = 0u; i < ENGINE_MAX_DISCRETE_FIELDS; i++) { widths[i] = fields[i].width; }
        return engineDescPack(modeOptions, auxRole, auxField, usesTempo, widths);
    }

    constexpr uint16_t discreteDefault() const
    {
        uint16_t discrete = 0u;
        for (uint8_t i = 0u; i < ENGINE_MAX_DISCRETE_FIELDS; i++)
        {
            discrete = engineFieldSet(descriptor(), discrete, i, fields[i].defaultValue);
        }
        return discrete;
    }
};

// What an engine reads, every block. The Processor fills it; see engine_controls.hpp.
struct BlockContext
{
    float    params[SPI_PARAM_COUNT] = {};              // 0..1
    uint8_t  field[ENGINE_MAX_DISCRETE_FIELDS] = {};    // per EngineInfo::fields
    uint8_t  paramsChanged = 0;                         // bit i: params[i] changed on this block
    uint16_t fieldsChanged = 0;                         // bit i: field[i] changed on this block
    int32_t  beatIndex = -1;                            // sample at which a beat falls, or -1
    float    beatPhase = 0.0f;                          // 0..1 at sample 0; 0 while tempoHz is 0
    float    tempoHz   = 0.0f;                          // 0 while no tempo is set
    bool     engaged   = false;
    // The first block after activate(). Both change masks are clear on it, so
    // initialise from params and field, and snap any smoothing.
    bool     first     = false;
};

static_assert(SPI_PARAM_COUNT <= 8u, "paramsChanged holds one bit per param");

class Engine {
public:
    virtual const EngineInfo& info() const = 0;
    // Thread mode, while the engine is parked; may take any time. Takes the engine's
    // worst case from the arenas, which hand it out zeroed.
    virtual void activate(EngineArenas& arenas, uint32_t sampleRate) = 0;
    virtual void deactivate() {}
    // PendSV, the only audio-context entry point. Writes wet only: the bypass controller
    // mixes the dry path. Whatever it does with ctx on every block must be cheap and
    // give the same result when repeated with the same values.
    virtual void process(dsp::ConstAudioBuffer in, dsp::AudioBuffer out, const BlockContext& ctx) = 0;

    uint16_t id() const { return info().id; }

protected:
    // Non-virtual and protected, so no deleting destructor pulls in operator delete.
    ~Engine() = default;
};
