#pragma once

#include "engine.hpp"
#include <cstdint>

// Turns a control set into the BlockContext an engine reads: params, unpacked fields
// and change masks. Used by the Processor, and by the host tests; engines do not
// include this.

// A new set for the running engine. Marks every param and field whose value differs
// from what ctx holds, except before the first block, whose masks stay clear.
inline void applyControls(const EngineInfo& info, const float* params, uint16_t discrete, BlockContext& ctx)
{
    const bool mark = !ctx.first;
    for (uint8_t i = 0u; i < SPI_PARAM_COUNT; i++)
    {
        if (mark && params[i] != ctx.params[i]) { ctx.paramsChanged = static_cast<uint8_t>(ctx.paramsChanged | (1u << i)); }
        ctx.params[i] = params[i];
    }

    // Packed from bit 0 in list order, as engine_link.h lays them out.
    uint8_t offset = 0u;
    for (uint8_t i = 0u; i < ENGINE_MAX_DISCRETE_FIELDS; i++)
    {
        const uint8_t width = info.fields[i].width;
        if (width == 0u) { break; }
        const uint8_t value = static_cast<uint8_t>((discrete >> offset) & ((1u << width) - 1u));
        offset = static_cast<uint8_t>(offset + width);
        if (mark && value != ctx.field[i]) { ctx.fieldsChanged = static_cast<uint16_t>(ctx.fieldsChanged | (1u << i)); }
        ctx.field[i] = value;
    }
}

// The first set after activate().
inline void beginControls(const EngineInfo& info, const float* params, uint16_t discrete, BlockContext& ctx)
{
    ctx = BlockContext {};
    ctx.first = true;
    applyControls(info, params, discrete, ctx);
}

// After the engine has run a block: the masks and first describe one block only.
inline void finishBlock(BlockContext& ctx)
{
    ctx.paramsChanged = 0u;
    ctx.fieldsChanged = 0u;
    ctx.first         = false;
}
