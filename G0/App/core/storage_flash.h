#pragma once

#include <stdbool.h>
#include <stdint.h>

// The flash under the preset log (plan §4.2): the STORAGE region of the linker
// script, outside the image the H7 programs. The only HAL-facing part of storage;
// the host tests replace it with a RAM model.

#define STORAGE_FLASH_PAGE_COUNT  4u
#define STORAGE_FLASH_PAGE_SIZE   2048u

// Offsets are bytes from the start of the region. False if the read hit an
// uncorrectable ECC error, as a double word left half-programmed by a power loss does.
bool storageFlashRead(uint32_t offset, void* dst, uint32_t size);

bool storageFlashErase(uint8_t page);

// One double word, 8-byte aligned and erased.
bool storageFlashProgram(uint32_t offset, uint64_t value);

// Target only, from the NMI handler. True if the NMI was an ECC double error inside
// the region, which is then cleared so the read that caused it can report it.
bool storageFlashNmi(void);
