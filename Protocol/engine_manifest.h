#ifndef ENGINE_MANIFEST_H
#define ENGINE_MANIFEST_H

#include <stdbool.h>
#include <stdint.h>
#include "spi_protocol.h"

/* What each engine means by param[] and discrete (plan §4.1). The G0 reaches an
 * engine's settings only through this table, and the H7 decodes them with it. */

#ifdef __cplusplus
#define MANIFEST_CONST constexpr
#else
/* Unused in a C file that includes this only for the types. */
#define MANIFEST_CONST __attribute__((unused)) const
#endif

/* MIDI CCs 103-111 reach at most 9 fields (plan §3.6). */
#define ENGINE_MAX_DISCRETE_FIELDS  9u
/* Field 0 is the mode field whenever modeOptions > 0 (plan §3.6). */
#define ENGINE_MODE_FIELD           0u
#define ENGINE_MAX_MODE_OPTIONS     4u
#define ENGINE_PARAM_NONE           0xFFu

enum {
    ENGINE_PASSTHROUGH = 0,
    ENGINE_TEST_DELAY,          /* bench check for the tempo link (M3) */
    ENGINE_COUNT
};

/* ENGINE_TEST_DELAY: param 0 feedback, 1 mix, 2 click level. The mode field picks
 * the repeat: quarter, dotted eighth, eighth. */
enum {
    TEST_DELAY_PARAM_FEEDBACK = 0,
    TEST_DELAY_PARAM_MIX,
    TEST_DELAY_PARAM_CLICK
};

enum {
    AUX_ROLE_NONE = 0,
    AUX_ROLE_TAP,
    AUX_ROLE_TOGGLE             /* toggles the 1-bit field auxField */
};

typedef struct {
    uint8_t offset;             /* bit position in discrete */
    uint8_t width;              /* 1..3 bits */
    uint8_t defaultValue;
} DiscreteField;

typedef struct {
    uint16_t      paramDefault[SPI_PARAM_COUNT];
    DiscreteField field[ENGINE_MAX_DISCRETE_FIELDS];
    uint8_t       fieldCount;
    uint8_t       modeOptions;  /* 0..4 values of the mode field */
    uint8_t       auxRole;      /* AUX_ROLE_* */
    uint8_t       auxField;
    uint8_t       blendParam;   /* param index, or ENGINE_PARAM_NONE */
    bool          usesTempo;
    bool          needsLoad;
} EngineManifest;

static MANIFEST_CONST EngineManifest kEngineManifest[ENGINE_COUNT] = {
    /* ENGINE_PASSTHROUGH */
    {
        { 0x8000u, 0x8000u, 0x8000u, 0x8000u, 0x8000u, 0x8000u, 0x8000u, 0x8000u },
        { { 0u, 2u, 0u } },
        1u,
        3u,
        AUX_ROLE_NONE,
        0u,
        ENGINE_PARAM_NONE,
        false,
        false,
    },
    /* ENGINE_TEST_DELAY */
    {
        { 0x6000u, 0x8000u, 0x8000u, 0x8000u, 0x8000u, 0x8000u, 0x8000u, 0x8000u },
        { { 0u, 2u, 0u } },
        1u,
        3u,
        AUX_ROLE_TAP,
        0u,
        TEST_DELAY_PARAM_MIX,
        true,
        false,
    },
};

static inline uint16_t engineDiscreteDefault(const EngineManifest* engine)
{
    uint16_t discrete = 0u;
    for (uint8_t i = 0u; i < engine->fieldCount; i++)
    {
        discrete = (uint16_t) (discrete | (engine->field[i].defaultValue << engine->field[i].offset));
    }
    return discrete;
}

#ifdef __cplusplus
/* C cannot evaluate the table at compile time, so the H7 build checks it. */
constexpr bool engineManifestValid(const EngineManifest& engine)
{
    if (engine.fieldCount > ENGINE_MAX_DISCRETE_FIELDS)      { return false; }
    uint32_t used = 0u;
    for (uint8_t i = 0u; i < engine.fieldCount; i++)
    {
        const DiscreteField& field = engine.field[i];
        if (field.width < 1u || field.width > 3u)            { return false; }
        if (field.offset + field.width > 16u)                { return false; }
        if (field.defaultValue >= (1u << field.width))       { return false; }
        const uint32_t bits = ((1u << field.width) - 1u) << field.offset;
        if ((used & bits) != 0u)                             { return false; }
        used |= bits;
    }
    if (engine.modeOptions > ENGINE_MAX_MODE_OPTIONS)        { return false; }
    if (engine.modeOptions > 0u &&
        (engine.fieldCount == 0u || engine.field[ENGINE_MODE_FIELD].width != 2u)) { return false; }
    if (engine.auxRole == AUX_ROLE_TOGGLE &&
        (engine.auxField >= engine.fieldCount || engine.field[engine.auxField].width != 1u)) { return false; }
    if (engine.blendParam != ENGINE_PARAM_NONE && engine.blendParam >= SPI_PARAM_COUNT) { return false; }
    return true;
}

constexpr bool engineManifestsValid()
{
    for (const EngineManifest& engine : kEngineManifest)
    {
        if (!engineManifestValid(engine)) { return false; }
    }
    return true;
}

static_assert(engineManifestsValid(), "kEngineManifest has an invalid entry");
#endif

#endif
