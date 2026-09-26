#include "param_store.h"
#include "engine_link.h"
#include <stddef.h>

// Consecutive echoes that must report an unknown engine before the G0 reverts. For
// a few frames after the wanted id changes, the flag still describes the old id.
#define UNKNOWN_CONFIRM 8u

_Static_assert(sizeof(Preset) == 22u, "Preset is the 22-byte payload of plan §4.2");

static uint16_t engineId;           // wanted engine; ENGINE_ID_NONE until the H7 names one
static uint8_t  presetIndex;
static uint8_t  revertIndex;        // presetIndex before the last recall
static uint16_t param[SPI_PARAM_COUNT];
static uint16_t discrete;
static uint16_t ownerMask;

// A query asks the H7 for the id at a registry index. A defaults request asks it for
// the wanted engine's defaults, which the G0 adopts from the echo; until then the H7
// ignores param[] and discrete.
static uint8_t  engineQuery;
static bool     queryPending;
static uint8_t  defaultsSeq;
static bool     defaultsPending;
static uint8_t  unknownCount;

// The wanted engine's descriptor, as last echoed while it ran.
static uint32_t engineDesc;
static bool     engineReady;

void paramStoreInit(void)
{
    engineId        = ENGINE_ID_NONE;
    presetIndex     = 0u;
    revertIndex     = 0u;
    for (uint32_t i = 0; i < SPI_PARAM_COUNT; i++) { param[i] = 0u; }
    discrete        = 0u;
    ownerMask       = 0u;
    engineQuery     = 0u;
    queryPending    = false;
    defaultsSeq     = 0u;
    defaultsPending = false;
    unknownCount    = 0u;
    engineDesc      = 0u;
    engineReady     = false;
}

void paramStoreRequestEngineAt(uint8_t index)
{
    engineQuery  = index;
    queryPending = true;
    unknownCount = 0u;
}

void paramStoreApply(const Preset* preset, uint8_t slot)
{
    revertIndex = presetIndex;
    presetIndex = slot;
    engineId    = preset->engineId;
    for (uint32_t i = 0; i < SPI_PARAM_COUNT; i++) { param[i] = preset->param[i]; }
    discrete        = preset->discrete;
    ownerMask       = 0u;
    queryPending    = false;
    defaultsPending = false;
    unknownCount    = 0u;
}

void paramStoreSetSlot(uint8_t slot)
{
    presetIndex = slot;
    revertIndex = slot;
}

void paramStoreSnapshot(Preset* preset)
{
    preset->engineId = engineId;
    for (uint32_t i = 0; i < SPI_PARAM_COUNT; i++) { preset->param[i] = param[i]; }
    preset->discrete = discrete;
    preset->tempo    = 0u;
}

static void takeEchoValues(const H7ToG0Packet* echo)
{
    for (uint32_t i = 0; i < SPI_PARAM_COUNT; i++) { param[i] = echo->param[i]; }
    discrete  = echo->discrete;
    ownerMask = echo->ownerMask;
}

void paramStoreRestore(const H7ToG0Packet* echo)
{
    engineId        = echo->activeEngine;
    defaultsSeq     = echo->defaultsSeqEcho;
    defaultsPending = false;
    queryPending    = false;
    presetIndex     = echo->presetIndex;
    revertIndex     = presetIndex;
    takeEchoValues(echo);
}

void paramStoreSetParam(uint8_t index, uint16_t value)
{
    if (index >= SPI_PARAM_COUNT) { return; }
    param[index] = value;
    ownerMask    = (uint16_t) (ownerMask | (1u << index));
}

uint8_t paramStoreField(uint8_t field)
{
    return engineFieldGet(engineDesc, discrete, field);
}

void paramStoreSetField(uint8_t field, uint8_t value)
{
    if (!engineReady) { return; }
    discrete = engineFieldSet(engineDesc, discrete, field, value);
}

uint32_t paramStoreDesc(void)
{
    return engineDesc;
}

bool paramStoreReady(void)
{
    return engineReady;
}

uint8_t paramStorePresetIndex(void)
{
    return presetIndex;
}

uint8_t paramStoreTrack(const H7ToG0Packet* echo)
{
    engineReady = false;
    if (echo == NULL) { return 0u; }
    uint8_t events = 0u;

    // The registry is fixed, so an answer for this index is current even if it was
    // sent for an earlier query.
    if (queryPending && echo->engineQueryEcho == engineQuery)
    {
        queryPending = false;
        if (echo->engineQueryId != ENGINE_ID_NONE)
        {
            // Differs from both values the H7 may hold: the echo, and the G0's own last one.
            // After a reset the count restarts, and a request equal to an old echo matches at once.
            engineId    = echo->engineQueryId;
            defaultsSeq = (uint8_t) (defaultsSeq + 1u);
            if (defaultsSeq == echo->defaultsSeqEcho) { defaultsSeq = (uint8_t) (defaultsSeq + 1u); }
            defaultsPending = true;
        }
    }

    // An id the H7 does not have, e.g. from a preset stored under older firmware: back
    // to the engine it runs, with the values that engine has, and the previous slot.
    const bool unknown = (echo->h7Flags & H7_FLAG_ENGINE_UNKNOWN) != 0u && !queryPending &&
                         echo->activeEngine != engineId;
    unknownCount = unknown ? (uint8_t) (unknownCount + 1u) : 0u;
    if (unknownCount >= UNKNOWN_CONFIRM)
    {
        unknownCount    = 0u;
        engineId        = echo->activeEngine;
        defaultsPending = false;
        presetIndex     = revertIndex;
        takeEchoValues(echo);
        events |= PARAM_STORE_REVERTED;
    }

    const bool active = echo->activeEngine == engineId && (echo->h7Flags & H7_FLAG_ENGINE_READY) != 0u;
    if (!active) { return events; }
    engineDesc = echo->engineDesc;

    if (defaultsPending)
    {
        if (echo->defaultsSeqEcho != defaultsSeq) { return events; }
        for (uint32_t i = 0; i < SPI_PARAM_COUNT; i++) { param[i] = echo->param[i]; }
        discrete        = echo->discrete;
        ownerMask       = 0u;
        defaultsPending = false;
        events |= PARAM_STORE_ADOPTED;
    }
    engineReady = true;
    return events;
}

void paramStoreFill(G0ToH7Packet* tx)
{
    tx->engineId    = engineId;
    tx->presetIndex = presetIndex;
    tx->engineQuery = engineQuery;
    tx->defaultsSeq = defaultsSeq;
    tx->g0Flags     = defaultsPending ? G0_FLAG_DEFAULTS_PENDING : 0u;
    for (uint32_t i = 0; i < SPI_PARAM_COUNT; i++) { tx->param[i] = param[i]; }
    tx->discrete  = discrete;
    tx->ownerMask = ownerMask;
}
