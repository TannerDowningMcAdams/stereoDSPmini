#pragma once

#include <stdbool.h>
#include <stdint.h>

// Global settings, which apply to every preset. Each (page, pot) holds one option
// index; the SETTINGS context edits a working copy, which a save commits to the
// applied values and to storage. No HAL: storage goes through storage.h.

#define SETTINGS_PAGES          4u      // 3 and 4 are reserved
#define SETTINGS_PAGES_USED     2u
#define SETTINGS_POTS           5u

#define SETTINGS_PAGE_MIDI      1u      // pots 0..3 are the channel bits, bit 0 on pot 0
#define SETTINGS_CHANNEL_BITS   4u

// Reads the stored settings; anything missing or out of range takes its default.
// Call after storageInit().
void settingsInit(void);

// Option count of the setting at (page, pot). 0 where none is assigned.
uint8_t settingsOptions(uint8_t page, uint8_t pot);

// The working copy starts from the applied values.
void    settingsBeginEdit(void);
uint8_t settingsEditValue(uint8_t page, uint8_t pot);
// Ignored where no setting is assigned or option is out of range.
void    settingsEditSet(uint8_t page, uint8_t pot, uint8_t option);
// 0..15, from the working copy.
uint8_t settingsEditMidiChannel(void);
// Applies the working copy and stores it. False if the write failed; the values
// apply until the next power cycle either way.
bool    settingsSave(void);

// Applied values.
uint16_t settingsRunFlags(void);        // RUN_FLAG_TRAILS, RUN_FLAG_ANALOG_DRY, RUN_FLAG_STEREO_IN
uint8_t  settingsDimmer(void);          // PWM LED scale, 255 = full
bool     settingsFollowMidiClock(void);
uint8_t  settingsMidiChannel(void);     // 0..15
bool     settingsMidiOmni(void);
