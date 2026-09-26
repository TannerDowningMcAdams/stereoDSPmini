#pragma once

#include <stdbool.h>
#include <stdint.h>

// Presets and settings in a log of 32-byte records over 4 flash pages (plan §4.2).
// A record is found by type and slot; the newest valid one wins. Power-loss safe: an
// interrupted write loses at most that record, and an interrupted compaction is
// finished at the next storageInit(). No HAL: flash access goes through
// storage_flash.h.
//
// Writes block the thread. A record takes about 0.4 ms. Every 63 records a page
// changes, which erases up to two pages (20-40 ms each, with the CPU stalled) and
// copies the live records out of the oldest one.

#define STORAGE_PAYLOAD_SIZE    24u

// Bump when a payload layout changes. Records of another version read as absent,
// so their slots fall back to factory defaults, and compaction drops them.
#define STORAGE_SCHEMA_VERSION  1u

typedef enum {
    STORAGE_TYPE_PRESET = 1,    // slots 0..STORAGE_PRESET_SLOTS - 1
    STORAGE_TYPE_FAVOURITE,     // slot 0
    STORAGE_TYPE_SETTINGS       // slots 0..STORAGE_SETTINGS_SLOTS - 1
} StorageType;

#define STORAGE_PRESET_SLOTS    64u
#define STORAGE_SETTINGS_SLOTS  4u

// Mounts the log. A blank store mounts empty.
void storageInit(void);

// Copies up to size bytes of the newest record. False if the slot was never written.
bool storageRead(StorageType type, uint8_t slot, void* payload, uint8_t size);

// size is at most STORAGE_PAYLOAD_SIZE; the rest of the payload is left erased.
// Writing the payload a slot already holds changes nothing.
bool storageWrite(StorageType type, uint8_t slot, const void* payload, uint8_t size);
