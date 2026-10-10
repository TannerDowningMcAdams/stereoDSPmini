#include "settings.h"
#include "spi_protocol.h"
#include "storage.h"
#include <string.h>

// Pots of the general page.
#define POT_BYPASS      0u      // true, trails
#define POT_INPUT       1u      // mono, stereo
#define POT_DRY         2u      // analog, digital
#define POT_DIMMER      3u      // 4 levels, brightest fully clockwise
#define POT_CLOCK       4u      // follow, ignore

// Pot of the MIDI page after the channel bits.
#define POT_OMNI        4u      // off, on

#define SETTINGS_PAGE_GENERAL   0u
#define VALUE_COUNT             (SETTINGS_PAGES * SETTINGS_POTS)
#define UNASSIGNED              0xFFu

typedef struct {
    uint8_t options;
    uint8_t defaultValue;
} SettingInfo;

// Unlisted entries have no options.
static const SettingInfo kSettings[SETTINGS_PAGES][SETTINGS_POTS] = {
    { { 2u, 0u }, { 2u, 1u }, { 2u, 1u }, { 4u, 3u }, { 2u, 0u } },
    { { 2u, 0u }, { 2u, 0u }, { 2u, 0u }, { 2u, 0u }, { 2u, 0u } },
};

static const uint8_t kDimmerScale[4] = { 32u, 64u, 128u, 255u };

// Stored as one record. Unassigned entries hold 0xFF, as do the payload bytes past
// the array, so a setting added later reads its default from an older record.
static uint8_t applied[VALUE_COUNT];
static uint8_t edit[VALUE_COUNT];

_Static_assert(VALUE_COUNT <= STORAGE_PAYLOAD_SIZE, "the settings fit one record");

static uint8_t indexOf(uint8_t page, uint8_t pot)
{
    return (uint8_t) (page * SETTINGS_POTS + pot);
}

uint8_t settingsOptions(uint8_t page, uint8_t pot)
{
    return (page < SETTINGS_PAGES && pot < SETTINGS_POTS) ? kSettings[page][pot].options : 0u;
}

void settingsInit(void)
{
    uint8_t stored[VALUE_COUNT];
    if (!storageRead(STORAGE_TYPE_SETTINGS, 0u, stored, sizeof(stored))) { memset(stored, UNASSIGNED, sizeof(stored)); }

    for (uint8_t page = 0; page < SETTINGS_PAGES; page++)
    {
        for (uint8_t pot = 0; pot < SETTINGS_POTS; pot++)
        {
            const SettingInfo* info = &kSettings[page][pot];
            const uint8_t i = indexOf(page, pot);
            if (info->options == 0u)              { applied[i] = UNASSIGNED; }
            else if (stored[i] < info->options)   { applied[i] = stored[i]; }
            else                                  { applied[i] = info->defaultValue; }
        }
    }
    memcpy(edit, applied, sizeof(edit));
}

void settingsBeginEdit(void)
{
    memcpy(edit, applied, sizeof(edit));
}

uint8_t settingsEditValue(uint8_t page, uint8_t pot)
{
    return (settingsOptions(page, pot) != 0u) ? edit[indexOf(page, pot)] : 0u;
}

void settingsEditSet(uint8_t page, uint8_t pot, uint8_t option)
{
    if (option < settingsOptions(page, pot)) { edit[indexOf(page, pot)] = option; }
}

static uint8_t channelOf(const uint8_t* values)
{
    uint8_t channel = 0u;
    for (uint8_t bit = 0; bit < SETTINGS_CHANNEL_BITS; bit++)
    {
        channel |= (uint8_t) (values[indexOf(SETTINGS_PAGE_MIDI, bit)] << bit);
    }
    return channel;
}

uint8_t settingsEditMidiChannel(void)
{
    return channelOf(edit);
}

bool settingsSave(void)
{
    memcpy(applied, edit, sizeof(applied));
    return storageWrite(STORAGE_TYPE_SETTINGS, 0u, applied, sizeof(applied));
}

static uint8_t value(uint8_t page, uint8_t pot)
{
    return applied[indexOf(page, pot)];
}

uint16_t settingsRunFlags(void)
{
    uint16_t flags = 0u;
    if (value(SETTINGS_PAGE_GENERAL, POT_BYPASS) != 0u) { flags |= RUN_FLAG_TRAILS; }
    if (value(SETTINGS_PAGE_GENERAL, POT_INPUT) != 0u)  { flags |= RUN_FLAG_STEREO_IN; }
    if (value(SETTINGS_PAGE_GENERAL, POT_DRY) == 0u)    { flags |= RUN_FLAG_ANALOG_DRY; }
    return flags;
}

uint8_t settingsDimmer(void)
{
    return kDimmerScale[value(SETTINGS_PAGE_GENERAL, POT_DIMMER)];
}

bool settingsFollowMidiClock(void)
{
    return value(SETTINGS_PAGE_GENERAL, POT_CLOCK) == 0u;
}

uint8_t settingsMidiChannel(void)
{
    return channelOf(applied);
}

bool settingsMidiOmni(void)
{
    return value(SETTINGS_PAGE_MIDI, POT_OMNI) != 0u;
}
