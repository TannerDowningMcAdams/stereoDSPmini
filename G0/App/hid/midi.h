#pragma once

#include <stdbool.h>
#include <stdint.h>

// MIDI 1.0 byte parser. Running status for channel messages; real-time bytes act
// wherever they fall, inside other messages and SysEx included, without disturbing
// running status; SysEx is skipped. Channel messages off the receive channel are
// dropped unless omni is on. No HAL: the caller feeds the bytes.

#define MIDI_BYTE_US            320u        // one byte at 31250 baud

// Channel messages: status is the high nibble.
#define MIDI_NOTE_OFF           0x80u
#define MIDI_NOTE_ON            0x90u
#define MIDI_POLY_PRESSURE      0xA0u
#define MIDI_CONTROL_CHANGE     0xB0u
#define MIDI_PROGRAM_CHANGE     0xC0u
#define MIDI_CHANNEL_PRESSURE   0xD0u
#define MIDI_PITCH_BEND         0xE0u

// System messages: status is the whole byte.
#define MIDI_SYSEX              0xF0u
#define MIDI_TIME_CODE          0xF1u
#define MIDI_SONG_POSITION      0xF2u
#define MIDI_SONG_SELECT        0xF3u
#define MIDI_TUNE_REQUEST       0xF6u
#define MIDI_SYSEX_END          0xF7u
#define MIDI_CLOCK              0xF8u
#define MIDI_START              0xFAu
#define MIDI_CONTINUE           0xFBu
#define MIDI_STOP               0xFCu
#define MIDI_ACTIVE_SENSING     0xFEu
#define MIDI_SYSTEM_RESET       0xFFu

typedef struct {
    uint8_t status;     // MIDI_*
    uint8_t channel;    // 0..15, channel messages only
    uint8_t data1;
    uint8_t data2;      // 0 for one-byte messages
} MidiMessage;

void midiInit(void);

// channel 0..15. Omni receives every channel.
void midiSetChannel(uint8_t channel, bool omni);

// True when byte completes a message, which is then in message. Emits complete
// channel messages on the receive channel, system common messages, and clock, start,
// continue and stop. Active sensing and undefined bytes are dropped; system reset
// only clears the parser.
bool midiParse(uint8_t byte, MidiMessage* message);
