#pragma once

#include "midi.h"
#include <stdbool.h>
#include <stdint.h>

// The fixed MIDI map, the same for every engine and board. It uses the CCs the MIDI
// spec leaves undefined:
//   PC n              load preset slot n
//   CC 102            0-63 bypass, 64-127 engaged
//   CC 103-111        discrete field 0-8 in descriptor order; field 0 is the mode field
//   CC 112-119        param 0-7
// Which context acts on what is ui_context.c's decision. No HAL.

#define MIDI_CC_ENGAGE          102u
#define MIDI_CC_FIELD_FIRST     103u
#define MIDI_CC_PARAM_FIRST     112u

typedef enum {
    MIDI_ACTION_LOAD,       // value: program number; the caller checks it is a slot
    MIDI_ACTION_ENGAGE,     // value: 1 engaged, 0 bypassed
    MIDI_ACTION_FIELD,      // index: field, value: its state
    MIDI_ACTION_PARAM       // index: param, value: normalized 0..65535
} MidiActionType;

typedef struct {
    MidiActionType type;
    uint8_t        index;
    uint16_t       value;
} MidiAction;

// desc is the running engine's descriptor. A field CC divides 0-127 into equal
// shares of the field's states: the mode options for the mode field, 2^width for
// the others. False for anything unmapped, including a field the engine lacks.
bool midiMap(const MidiMessage* message, uint32_t desc, MidiAction* action);
