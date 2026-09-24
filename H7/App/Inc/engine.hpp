#pragma once

#include "audio_buffer.hpp"
#include "engine_arena.hpp"
#include "engine_link.h"
#include "spi_protocol.h"
#include <cstdint>

// The engine model of plan §4.4. An engine is one effect or one fixed signal chain.
// Engines are static instances that are never constructed or destroyed at runtime:
// activate() and deactivate() take the place of both.

static constexpr uint8_t kEngineParamNone = 0xFFu;

// Everything the H7 knows about an engine without running it.
struct EngineInfo
{
    uint16_t id;                                        // engine_ids.hpp; never reused
    uint8_t  modeOptions;                               // 0..4 values of the mode field
    uint8_t  auxRole;                                   // AUX_ROLE_*
    uint8_t  auxField;                                  // the 1-bit field AUX toggles
    bool     usesTempo;
    uint8_t  fieldWidth[ENGINE_MAX_DISCRETE_FIELDS];    // 1..3 bits; the first 0 ends the list
    uint8_t  fieldDefault[ENGINE_MAX_DISCRETE_FIELDS];
    uint16_t paramDefault[SPI_PARAM_COUNT];             // normalized 0..65535
    uint8_t  blendParam;                                // param index, or kEngineParamNone
    uint32_t fastBytes;                                 // worst case per arena, from arenaBytes()
    uint32_t largeBytes;

    constexpr uint32_t descriptor() const
    {
        return engineDescPack(modeOptions, auxRole, auxField, usesTempo, fieldWidth);
    }

    constexpr uint16_t discreteDefault() const
    {
        uint16_t discrete = 0u;
        for (uint8_t i = 0u; i < ENGINE_MAX_DISCRETE_FIELDS; i++)
        {
            discrete = engineFieldSet(descriptor(), discrete, i, fieldDefault[i]);
        }
        return discrete;
    }
};

// Per control set, in PendSV.
struct EngineControls
{
    float    params[SPI_PARAM_COUNT];   // 0..1
    uint16_t discrete;                  // fields per EngineInfo
    uint16_t eventEdges;                // event bits that flipped since the last set
};

// Per block, in PendSV.
struct BlockContext
{
    int32_t beatIndex;                  // sample at which a beat falls in this block, or -1
    float   tempoHz;                    // 0 while no tempo is set
    bool    engaged;
};

class Engine {
public:
    virtual const EngineInfo& info() const = 0;
    // Thread mode, while the engine is parked; may take any time. Takes the engine's
    // worst case from the arenas and clears it.
    virtual void activate(EngineArenas& arenas, uint32_t sampleRate) = 0;
    virtual void deactivate() = 0;
    // PendSV, or thread mode while parked.
    virtual void setControls(const EngineControls& controls) = 0;
    // PendSV. Writes wet only: the bypass controller mixes the dry path.
    virtual void process(dsp::ConstAudioBuffer in, dsp::AudioBuffer out, const BlockContext& ctx) = 0;

    uint16_t id() const { return info().id; }

protected:
    // Non-virtual and protected, so no deleting destructor pulls in operator delete.
    ~Engine() = default;
};
