#include "spi_link.h"
#include "bootloader.h"
#include "iwdg.h"
#include "main.h"
#include "protocol_version.h"
#include "spi.h"
#include "timebase.h"
#include <stdbool.h>
#include <stddef.h>

// Well above a flash page erase, as on the H7.
#define LINK_TIMEOUT_MS 100u

// The packets are packed (alignment 1); the SPI DMA moves halfwords.
static G0ToH7Packet txBuffers[2] __attribute__((aligned(4)));
static H7ToG0Packet rxBuffers[2] __attribute__((aligned(4)));

// txNext belongs to the thread while txNextReady is clear, and to the CC1 ISR while
// it is set. txActive belongs to the ISR and the DMA.
static G0ToH7Packet* txActive = &txBuffers[0];
static G0ToH7Packet* txNext   = &txBuffers[1];
static volatile bool txNextReady = false;

// rxReady belongs to the SPI ISR while rxReadyFull is clear, and to the thread while
// it is set. rxActive belongs to the ISR and the DMA.
static H7ToG0Packet* rxActive = &rxBuffers[0];
static H7ToG0Packet* rxReady  = &rxBuffers[1];
static volatile bool rxReadyFull = false;

static volatile uint32_t frameCount = 0;
static uint16_t frameSeq = 0;

// Thread only.
static H7ToG0Packet echo;
static bool echoValid = false;
static uint32_t echoDeadline = 0;

static void endFrame(void);

void spiLinkStartFrame(void)
{
    // A frame still in flight means the previous one hung; leave it for the watchdog.
    if (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY) { return; }

    if (txNextReady)
    {
        __DMB();    // acquire: flag read before the DMA reads the payload
        G0ToH7Packet* sent = txActive;
        txActive = txNext;
        txNext = sent;
        txNextReady = false;
    }

    HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_RESET);
    if (HAL_SPI_TransmitReceive_DMA(&hspi1, (uint8_t *) txActive,
                                    (uint8_t *) rxActive, SPI_PACKET_NUM_WORDS) != HAL_OK)
    {
        // Abandon the frame. The H7 re-arms on the rising edge regardless.
        HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_SET);
    }
}

// The rising edge is the H7's end-of-packet signal, so raise CS on every outcome.
static void endFrame(void)
{
    HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_SET);
    frameCount = frameCount + 1u;
}

G0ToH7Packet* spiLinkBeginTx(void)
{
    return txNextReady ? NULL : txNext;
}

void spiLinkCommitTx(void)
{
    txNext->messageId   = SPI_MSG_ID_G0_TO_H7;
    txNext->version     = PROTOCOL_VERSION;
    txNext->g0FwVersion = G0_FW_VERSION;
    txNext->frameSeq    = frameSeq;
    frameSeq = (uint16_t) (frameSeq + 1u);
    __DMB();    // release: payload visible before the flag that publishes it
    txNextReady = true;
}

void spiLinkPoll(void)
{
    // Refresh only while SPI frames are completing, so a stalled timer, DMA or SPI
    // resets the G0 rather than leaving the H7 holding stale controls.
    static uint32_t lastFrameCount = 0;
    const uint32_t count = frameCount;
    if (count != lastFrameCount)
    {
        lastFrameCount = count;
        HAL_IWDG_Refresh(&hiwdg);
    }

    if (!rxReadyFull) { return; }
    __DMB();    // acquire: flag read before payload read

    if (rxReady->messageId == SPI_MSG_ID_H7_TO_G0)
    {
        // Checked before the version: the magic keeps its position in every version.
        if (rxReady->bootloaderMagic == SPI_BOOTLOADER_MAGIC) { bootloaderRequest(); }
        if (rxReady->version == PROTOCOL_VERSION)
        {
            echo = *rxReady;
            echoValid = true;
            echoDeadline = deadlineSet(LINK_TIMEOUT_MS);
        }
    }

    __DMB();    // payload consumed before the buffer returns to the ISR
    rxReadyFull = false;
}

const H7ToG0Packet* spiLinkEcho(void)
{
    if (!echoValid) { return NULL; }
    if (deadlineExpired(echoDeadline))
    {
        echoValid = false;
        return NULL;
    }
    return &echo;
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance != SPI1) { return; }

    // A frame that arrives while the thread still holds the last one is dropped;
    // the next is 1 ms away.
    if (!rxReadyFull)
    {
        H7ToG0Packet* received = rxActive;
        rxActive = rxReady;
        rxReady = received;
        __DMB();    // release
        rxReadyFull = true;
    }
    endFrame();
}

// Includes a CRC mismatch on what the H7 sent back, e.g. when it is not yet armed.
// The frame is not used.
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1) { endFrame(); }
}
