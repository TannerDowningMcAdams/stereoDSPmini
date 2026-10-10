#pragma once

#include <stdbool.h>
#include <stdint.h>

// Tempo and beat phase (plan §3.2 rule 5, §4.1 note 3). Tap and preset recall set
// the internal tempo; MIDI clock overrides it while present and followed. No HAL:
// every call takes the time, so this builds on the host.

#define TEMPO_MIN_PERIOD_US   200000u     // 300 BPM
#define TEMPO_MAX_PERIOD_US   2000000u    // 30 BPM
#define TEMPO_TAP_RESET_US    2000000u    // a longer gap starts a new tap sequence
#define TEMPO_TAP_INTERVALS   4u          // averaged
#define TEMPO_CLOCK_LOSS_US   500000u
#define TEMPO_CLOCK_PPQN      24u

void tempoInit(void);

// A tap that resolved as a short press, committed on release. pressUs is its press
// edge, which becomes the beat instant, so the phase lands where the tap was.
void tempoTap(uint32_t pressUs);

// Preset recall. 0 leaves the running tempo alone. The beat restarts at nowUs.
void tempoRecall(uint16_t centiBpm, uint32_t nowUs);

// MIDI real-time bytes, timestamped on arrival. The clock sets the tempo whether or
// not the sender is playing. Beats follow its song position: Start makes the next
// clock a beat, Stop holds the position while the beat runs on at the clock's tempo,
// and Continue resumes from it. Until the first Stop, every clock counts.
void tempoMidiClock(uint32_t nowUs);
void tempoMidiStart(void);
void tempoMidiStop(void);
void tempoMidiContinue(void);
// Song position pointer, in sixteenths: the position the next clock lands on.
void tempoMidiSongPosition(uint16_t sixteenths);

// The MIDI clock setting (plan §3.4). Following is the default.
void tempoSetFollowMidi(bool follow);

// Once per tick.
void tempoPoll(uint32_t nowUs);

// TEMPO_SRC_* of spi_protocol.h.
uint8_t  tempoSource(void);
// 0 while no tempo is set.
uint32_t tempoPeriodUs(void);
// The tap or recalled tempo that STORE saves, whatever the MIDI clock does. 0 = none.
uint16_t tempoInternalCentiBpm(void);
// Position in the beat at atUs, 0..65535. atUs may be up to a few ms ahead of the
// last tempoPoll().
uint16_t tempoPhaseAt(uint32_t atUs);
