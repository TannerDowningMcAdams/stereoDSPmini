#include "midi.h"

// status is the message being received: a channel status stays for running status,
// a system common status is cleared once its data is complete. 0 = none, so stray
// data bytes are dropped.
static uint8_t status;
static uint8_t needed;          // data bytes of status
static uint8_t count;           // data bytes received so far
static uint8_t data[2];
static bool    sysex;
static uint8_t channel;
static bool    omni;

void midiInit(void)
{
    status  = 0u;
    needed  = 0u;
    count   = 0u;
    sysex   = false;
    channel = 0u;
    omni    = false;
}

void midiSetChannel(uint8_t receiveChannel, bool receiveOmni)
{
    channel = receiveChannel & 0x0Fu;
    omni    = receiveOmni;
}

static uint8_t dataLength(uint8_t statusByte)
{
    switch (statusByte & 0xF0u)
    {
        case MIDI_PROGRAM_CHANGE:
        case MIDI_CHANNEL_PRESSURE:
            return 1u;
        case 0xF0u:
            if (statusByte == MIDI_TIME_CODE || statusByte == MIDI_SONG_SELECT) { return 1u; }
            return (statusByte == MIDI_SONG_POSITION) ? 2u : 0u;
        default:
            return 2u;
    }
}

static bool realTime(uint8_t byte, MidiMessage* message)
{
    switch (byte)
    {
        case MIDI_CLOCK:
        case MIDI_START:
        case MIDI_CONTINUE:
        case MIDI_STOP:
            *message = (MidiMessage) { byte, 0u, 0u, 0u };
            return true;
        case MIDI_SYSTEM_RESET:
            status = 0u;
            count  = 0u;
            sysex  = false;
            return false;
        default:
            return false;
    }
}

bool midiParse(uint8_t byte, MidiMessage* message)
{
    if (byte >= MIDI_CLOCK) { return realTime(byte, message); }

    if ((byte & 0x80u) != 0u)
    {
        // Every other status ends SysEx and replaces the running status, even one
        // whose message is incomplete. Undefined and data-less system common
        // statuses leave none.
        sysex  = byte == MIDI_SYSEX;
        count  = 0u;
        needed = dataLength(byte);
        status = (byte < MIDI_SYSEX || needed > 0u) ? byte : 0u;
        return false;
    }

    if (sysex || status == 0u) { return false; }
    data[count++] = byte;
    if (count < needed) { return false; }
    count = 0u;

    const uint8_t received = status;
    if (received >= MIDI_SYSEX)
    {
        status = 0u;
        *message = (MidiMessage) { received, 0u, data[0], (needed == 2u) ? data[1] : 0u };
        return true;
    }

    const uint8_t messageChannel = received & 0x0Fu;
    if (!omni && messageChannel != channel) { return false; }
    *message = (MidiMessage) { (uint8_t) (received & 0xF0u), messageChannel, data[0], (needed == 2u) ? data[1] : 0u };
    return true;
}
