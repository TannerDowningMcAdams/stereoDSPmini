#pragma once

#include "midi.h"
#include "spi_protocol.h"
#include <stdbool.h>
#include <stdint.h>

// The UI contexts: BOOT, then BYPASS, RUN, the RECALL, STORE and ENGINE menus, and
// SETTINGS, which only a cold boot with ON held enters. The live state it sends is
// param_store's.

void uiInit(void);

// 1 ms tick, after switchPoll() and any pot update.
void uiTick(void);

// Each message received this tick, before uiTick(). atUs is when its last byte
// arrived. Clock and transport go to the tempo in every context; the rest is routed
// by context, and dropped in BOOT and SETTINGS.
void uiMidi(const MidiMessage* message, uint32_t atUs);

// Fills the payload of the next frame. False during BOOT, when no frame is sent and
// the H7 holds its last applied state.
bool uiFillFrame(UiToH7Packet* tx);
