#include "midi_map.h"
#include "engine_link.h"
#include "spi_protocol.h"

_Static_assert(MIDI_CC_FIELD_FIRST + ENGINE_MAX_DISCRETE_FIELDS <= MIDI_CC_PARAM_FIRST, "field CCs overlap the params");
_Static_assert(MIDI_CC_PARAM_FIRST + SPI_PARAM_COUNT <= 120u, "param CCs reach the channel mode messages");

static bool emit(MidiAction* action, MidiActionType type, uint8_t index, uint16_t value)
{
    *action = (MidiAction) { type, index, value };
    return true;
}

// The mode field has modeOptions states, which need not fill its 2 bits; the H7
// passes the field to the engine unchecked, so a state past the last option must
// never be sent.
static uint8_t fieldStates(uint32_t desc, uint8_t field, uint8_t width)
{
    const uint8_t modeOptions = engineDescModeOptions(desc);
    if (field == ENGINE_MODE_FIELD && modeOptions > 0u) { return modeOptions; }
    return (uint8_t) (1u << width);
}

bool midiMap(const MidiMessage* message, uint32_t desc, MidiAction* action)
{
    if (message->status == MIDI_PROGRAM_CHANGE) { return emit(action, MIDI_ACTION_LOAD, 0u, message->data1); }
    if (message->status != MIDI_CONTROL_CHANGE) { return false; }

    const uint8_t cc    = message->data1;
    const uint8_t value = message->data2;
    if (cc == MIDI_CC_ENGAGE) { return emit(action, MIDI_ACTION_ENGAGE, 0u, (value >= 64u) ? 1u : 0u); }

    if (cc >= MIDI_CC_FIELD_FIRST && cc < MIDI_CC_FIELD_FIRST + ENGINE_MAX_DISCRETE_FIELDS)
    {
        const uint8_t field = (uint8_t) (cc - MIDI_CC_FIELD_FIRST);
        const uint8_t width = engineFieldWidth(desc, field);
        if (width == 0u) { return false; }
        const uint8_t state = (uint8_t) (((uint16_t) value * fieldStates(desc, field, width)) >> 7);
        return emit(action, MIDI_ACTION_FIELD, field, state);
    }

    if (cc >= MIDI_CC_PARAM_FIRST && cc < MIDI_CC_PARAM_FIRST + SPI_PARAM_COUNT)
    {
        // Bit replication spans the whole range: 0 -> 0 and 127 -> 65535.
        const uint16_t param = (uint16_t) ((value << 9) | (value << 2) | (value >> 5));
        return emit(action, MIDI_ACTION_PARAM, (uint8_t) (cc - MIDI_CC_PARAM_FIRST), param);
    }
    return false;
}
