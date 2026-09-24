#ifndef ENGINE_LINK_H
#define ENGINE_LINK_H

#include <stdbool.h>
#include <stdint.h>

/* The link-level parts of the engine model (plan §4.4). Engines are defined on the H7
 * only. The G0 treats an engine id as opaque and reaches the discrete fields through
 * the descriptor the H7 sends for the active engine. */

#ifdef __cplusplus
#define ENGINE_LINK_FN static constexpr
#else
#define ENGINE_LINK_FN static inline
#endif

#define ENGINE_ID_NONE              0x0000u

/* MIDI CCs 103-111 reach at most 9 fields (plan §3.6). */
#define ENGINE_MAX_DISCRETE_FIELDS  9u
#define ENGINE_MAX_FIELD_WIDTH      3u
/* Field 0 is the mode field whenever modeOptions > 0. */
#define ENGINE_MODE_FIELD           0u
#define ENGINE_MODE_FIELD_WIDTH     2u
#define ENGINE_MAX_MODE_OPTIONS     4u

enum {
    AUX_ROLE_NONE = 0,
    AUX_ROLE_TAP,
    AUX_ROLE_TOGGLE             /* toggles the 1-bit field auxField */
};

/* Descriptor, 32 bits. Field widths are 2 bits each, and the first 0 ends the list.
 * Fields are packed contiguously from bit 0 of discrete, so offsets follow from the
 * widths. */
#define ENGINE_DESC_MODE_OPTIONS_SHIFT  0u
#define ENGINE_DESC_MODE_OPTIONS_MASK   0x7u
#define ENGINE_DESC_AUX_ROLE_SHIFT      3u
#define ENGINE_DESC_AUX_ROLE_MASK       0x3u
#define ENGINE_DESC_AUX_FIELD_SHIFT     5u
#define ENGINE_DESC_AUX_FIELD_MASK      0xFu
#define ENGINE_DESC_USES_TEMPO_SHIFT    9u
#define ENGINE_DESC_WIDTHS_SHIFT        10u
#define ENGINE_DESC_WIDTH_BITS          2u
#define ENGINE_DESC_WIDTH_MASK          0x3u

ENGINE_LINK_FN uint32_t engineDescPack(uint8_t modeOptions, uint8_t auxRole, uint8_t auxField,
                                       bool usesTempo, const uint8_t* widths)
{
    uint32_t desc = ((uint32_t) (modeOptions & ENGINE_DESC_MODE_OPTIONS_MASK) << ENGINE_DESC_MODE_OPTIONS_SHIFT) |
                    ((uint32_t) (auxRole & ENGINE_DESC_AUX_ROLE_MASK) << ENGINE_DESC_AUX_ROLE_SHIFT) |
                    ((uint32_t) (auxField & ENGINE_DESC_AUX_FIELD_MASK) << ENGINE_DESC_AUX_FIELD_SHIFT) |
                    ((uint32_t) (usesTempo ? 1u : 0u) << ENGINE_DESC_USES_TEMPO_SHIFT);
    for (uint8_t i = 0u; i < ENGINE_MAX_DISCRETE_FIELDS; i++)
    {
        desc |= (uint32_t) (widths[i] & ENGINE_DESC_WIDTH_MASK) << (ENGINE_DESC_WIDTHS_SHIFT + ENGINE_DESC_WIDTH_BITS * i);
    }
    return desc;
}

ENGINE_LINK_FN uint8_t engineDescModeOptions(uint32_t desc)
{
    return (uint8_t) ((desc >> ENGINE_DESC_MODE_OPTIONS_SHIFT) & ENGINE_DESC_MODE_OPTIONS_MASK);
}

ENGINE_LINK_FN uint8_t engineDescAuxRole(uint32_t desc)
{
    return (uint8_t) ((desc >> ENGINE_DESC_AUX_ROLE_SHIFT) & ENGINE_DESC_AUX_ROLE_MASK);
}

ENGINE_LINK_FN uint8_t engineDescAuxField(uint32_t desc)
{
    return (uint8_t) ((desc >> ENGINE_DESC_AUX_FIELD_SHIFT) & ENGINE_DESC_AUX_FIELD_MASK);
}

ENGINE_LINK_FN bool engineDescUsesTempo(uint32_t desc)
{
    return ((desc >> ENGINE_DESC_USES_TEMPO_SHIFT) & 1u) != 0u;
}

/* Width of a field, or 0 when the list ends at or before it. */
ENGINE_LINK_FN uint8_t engineFieldWidth(uint32_t desc, uint8_t field)
{
    uint8_t width = 0u;
    for (uint8_t i = 0u; i <= field && i < ENGINE_MAX_DISCRETE_FIELDS; i++)
    {
        width = (uint8_t) ((desc >> (ENGINE_DESC_WIDTHS_SHIFT + ENGINE_DESC_WIDTH_BITS * i)) & ENGINE_DESC_WIDTH_MASK);
        if (width == 0u) { return 0u; }
    }
    return (field < ENGINE_MAX_DISCRETE_FIELDS) ? width : 0u;
}

ENGINE_LINK_FN uint8_t engineFieldOffset(uint32_t desc, uint8_t field)
{
    uint8_t offset = 0u;
    for (uint8_t i = 0u; i < field; i++) { offset = (uint8_t) (offset + engineFieldWidth(desc, i)); }
    return offset;
}

/* 0 for a field the descriptor does not have. */
ENGINE_LINK_FN uint8_t engineFieldGet(uint32_t desc, uint16_t discrete, uint8_t field)
{
    const uint8_t width = engineFieldWidth(desc, field);
    if (width == 0u) { return 0u; }
    return (uint8_t) ((discrete >> engineFieldOffset(desc, field)) & ((1u << width) - 1u));
}

/* Unchanged for a field the descriptor does not have. */
ENGINE_LINK_FN uint16_t engineFieldSet(uint32_t desc, uint16_t discrete, uint8_t field, uint8_t value)
{
    const uint8_t width = engineFieldWidth(desc, field);
    if (width == 0u) { return discrete; }
    const uint8_t  offset = engineFieldOffset(desc, field);
    const uint16_t mask   = (uint16_t) (((1u << width) - 1u) << offset);
    return (uint16_t) ((discrete & ~mask) | (((uint16_t) value << offset) & mask));
}

#endif
