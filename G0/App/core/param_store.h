#pragma once

#include "spi_protocol.h"
#include <stdbool.h>
#include <stdint.h>

// The live state the G0 sends (plan §4): the wanted engine, params and their owners,
// the discrete word and the preset index, plus the engine query and defaults
// handshake with the H7 (§4.4). The engine id and discrete word are opaque; fields
// are reached through the descriptor the H7 echoes for the running engine. No HAL.

// The stored preset (plan §4.2), 22 bytes.
typedef struct {
    uint16_t engineId;
    uint16_t param[SPI_PARAM_COUNT];
    uint16_t discrete;
    uint16_t tempo;         // centi-BPM, 0 = none
} Preset;

// paramStoreTrack() results.
#define PARAM_STORE_ADOPTED   (1u << 0)     // defaults adopted from the echo
#define PARAM_STORE_REVERTED  (1u << 1)     // the wanted engine is unknown; back on the running one

void paramStoreInit(void);

// Registry index 0 on a new unit, or the engine ENGINE select loads: asks the H7 for
// the id at index, then for that engine's defaults.
void paramStoreRequestEngineAt(uint8_t index);

// Recall. The previous preset index comes back if the H7 does not know the engine.
void paramStoreApply(const Preset* preset, uint8_t slot);
// An empty slot recalled, or the slot a STORE wrote.
void paramStoreSetSlot(uint8_t slot);
// Tempo is left 0.
void paramStoreSnapshot(Preset* preset);
// Warm reset: the state the H7 last applied (plan §4.3).
void paramStoreRestore(const H7ToG0Packet* echo);

// A pot or MIDI CC takes the param over.
void paramStoreSetParam(uint8_t index, uint16_t value);

// Fields of the running engine. Edits are ignored until paramStoreReady().
uint8_t  paramStoreField(uint8_t field);
void     paramStoreSetField(uint8_t field, uint8_t value);
uint32_t paramStoreDesc(void);

// The H7 runs the wanted engine and the G0 holds its values.
bool     paramStoreReady(void);
uint8_t  paramStorePresetIndex(void);

// Once per tick with spiLinkEcho(), which may be NULL. Returns PARAM_STORE_* bits.
uint8_t paramStoreTrack(const H7ToG0Packet* echo);

// Fills engineId, presetIndex, the query and defaults fields, param[], discrete and
// ownerMask.
void paramStoreFill(G0ToH7Packet* tx);
