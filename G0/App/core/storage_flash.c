#include "storage_flash.h"
#include "main.h"
#include <string.h>

// Linker script symbols.
extern const uint8_t __storage_start__[];
extern const uint8_t __storage_end__[];

// Set by the NMI during a read.
static volatile bool eccFault = false;

bool storageFlashRead(uint32_t offset, void* dst, uint32_t size)
{
    eccFault = false;
    memcpy(dst, &__storage_start__[offset], size);
    __DSB();    // the reads, and any NMI they raise, complete before the flag is read
    return !eccFault;
}

bool storageFlashErase(uint8_t page)
{
    FLASH_EraseInitTypeDef erase = { 0 };
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.Banks     = FLASH_BANK_1;
    erase.Page      = ((uint32_t) __storage_start__ - FLASH_BASE) / FLASH_PAGE_SIZE + page;
    erase.NbPages   = 1u;
    uint32_t failedPage = 0u;

    HAL_FLASH_Unlock();
    const HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase, &failedPage);
    HAL_FLASH_Lock();
    return status == HAL_OK;
}

// HAL_FLASH_Program()'s sequence for one double word. Calling it would also link its
// fast-programming routine, which HAL places in RAM and which the log never uses.
bool storageFlashProgram(uint32_t offset, uint64_t value)
{
    volatile uint32_t* word = (volatile uint32_t*) &__storage_start__[offset];

    HAL_FLASH_Unlock();
    HAL_StatusTypeDef status = FLASH_WaitForLastOperation(FLASH_TIMEOUT_VALUE);
    if (status == HAL_OK)
    {
        SET_BIT(FLASH->CR, FLASH_CR_PG);
        word[0] = (uint32_t) value;
        __ISB();    // the two words are written in order; the second starts programming
        word[1] = (uint32_t) (value >> 32);
        status = FLASH_WaitForLastOperation(FLASH_TIMEOUT_VALUE);
        CLEAR_BIT(FLASH->CR, FLASH_CR_PG);
    }
    HAL_FLASH_Lock();
    return status == HAL_OK;
}

bool storageFlashNmi(void)
{
    const uint32_t eccr = FLASH->ECCR;
    if ((eccr & FLASH_ECCR_ECCD) == 0u) { return false; }

    // ADDR_ECC counts double words from the start of flash.
    const uint32_t address = FLASH_BASE + ((eccr & FLASH_ECCR_ADDR_ECC) << 3);
    if (address < (uint32_t) __storage_start__ || address >= (uint32_t) __storage_end__) { return false; }

    // ECCC is also write-1-to-clear, so it is written as 0 to leave it as it is.
    FLASH->ECCR = (eccr & ~FLASH_ECCR_ECCC) | FLASH_ECCR_ECCD;
    eccFault = true;
    return true;
}
