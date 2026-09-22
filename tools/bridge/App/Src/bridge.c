#include "bridge.h"
#include "main.h"
#include "usart.h"
#include "usb_device.h"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"
#include <stdbool.h>
#include <string.h>

#define UART_RX_RING_SIZE 1024u   // Power of two
#define UART_RX_RING_MASK (UART_RX_RING_SIZE - 1u)
#define UART_TX_TIMEOUT_MS 100u
#define LINE_CODING_SIZE 7u

extern USBD_HandleTypeDef hUsbDeviceFS;

// USART3 -> USB. Head is written only by the USART3 interrupt, tail only by bridgePoll().
static uint8_t uartRxRing[UART_RX_RING_SIZE];
static volatile uint32_t uartRxHead;
static volatile uint32_t uartRxTail;
static uint8_t uartRxByte;
static uint8_t usbTxPacket[CDC_DATA_FS_MAX_PACKET_SIZE];

// USB -> USART3. The OUT endpoint stays NAKed until bridgePoll() has sent the packet.
static uint8_t* volatile usbRxData;
static volatile uint32_t usbRxLength;
static volatile bool usbRxPending;

// CDC line coding: dwDTERate (LE), bCharFormat, bParityType, bDataBits. Default 115200 8E1.
static uint8_t lineCoding[LINE_CODING_SIZE] = { 0x00, 0xC2, 0x01, 0x00, 0u, 2u, 8u };
static volatile bool lineCodingPending;

// Diagnostic counters, for a debugger watch.
static volatile uint32_t uartRxDropped;
static volatile uint32_t uartErrors;
static volatile uint32_t lineCodingRejected;

static bool usbConfigured(void)
{
    return hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED && hUsbDeviceFS.pClassData != NULL;
}

static void startUartRx(void)
{
    (void) HAL_UART_Receive_IT(&huart3, &uartRxByte, 1u);
}

// Word length on STM32 includes the parity bit: 8 data + parity is UART_WORDLENGTH_9B.
static void applyLineCoding(void)
{
    uint8_t coding[LINE_CODING_SIZE];
    __disable_irq();
    memcpy(coding, lineCoding, sizeof(coding));
    __enable_irq();

    uint32_t baud = (uint32_t) coding[0] | ((uint32_t) coding[1] << 8)
                  | ((uint32_t) coding[2] << 16) | ((uint32_t) coding[3] << 24);
    uint8_t stop = coding[4];
    uint8_t parity = coding[5];
    uint8_t dataBits = coding[6];

    uint32_t frameBits = dataBits + (parity != 0u ? 1u : 0u);
    uint32_t wordLength;
    switch (frameBits)
    {
        case 7u: wordLength = UART_WORDLENGTH_7B; break;
        case 8u: wordLength = UART_WORDLENGTH_8B; break;
        case 9u: wordLength = UART_WORDLENGTH_9B; break;
        default: lineCodingRejected = lineCodingRejected + 1u; return;
    }

    uint32_t parityMode;
    switch (parity)
    {
        case 0u: parityMode = UART_PARITY_NONE; break;
        case 1u: parityMode = UART_PARITY_ODD;  break;
        case 2u: parityMode = UART_PARITY_EVEN; break;
        default: lineCodingRejected = lineCodingRejected + 1u; return;   // Mark and space
    }

    uint32_t stopBits;
    switch (stop)
    {
        case 0u: stopBits = UART_STOPBITS_1;   break;
        case 1u: stopBits = UART_STOPBITS_1_5; break;
        case 2u: stopBits = UART_STOPBITS_2;   break;
        default: lineCodingRejected = lineCodingRejected + 1u; return;
    }

    if (baud == 0u) { lineCodingRejected = lineCodingRejected + 1u; return; }

    (void) HAL_UART_AbortReceive(&huart3);
    huart3.Init.BaudRate = baud;
    huart3.Init.WordLength = wordLength;
    huart3.Init.Parity = parityMode;
    huart3.Init.StopBits = stopBits;
    if (HAL_UART_Init(&huart3) != HAL_OK) { lineCodingRejected = lineCodingRejected + 1u; }
    startUartRx();
}

static void forwardUsbToUart(void)
{
    if (!usbRxPending) { return; }

    if (usbRxLength > 0u)
    {
        (void) HAL_UART_Transmit(&huart3, usbRxData, (uint16_t) usbRxLength, UART_TX_TIMEOUT_MS);
    }
    usbRxPending = false;

    // Re-arming releases the NAK; the USB interrupt must not run inside the class call.
    if (!usbConfigured()) { return; }
    __disable_irq();
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, usbRxData);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    __enable_irq();
}

static void forwardUartToUsb(void)
{
    if (!usbConfigured()) { return; }
    USBD_CDC_HandleTypeDef* hcdc = (USBD_CDC_HandleTypeDef*) hUsbDeviceFS.pClassData;
    if (hcdc->TxState != 0u) { return; }

    uint32_t tail = uartRxTail;
    uint32_t count = uartRxHead - tail;
    if (count == 0u) { return; }
    if (count > sizeof(usbTxPacket)) { count = sizeof(usbTxPacket); }

    for (uint32_t i = 0; i < count; i++)
    {
        usbTxPacket[i] = uartRxRing[(tail + i) & UART_RX_RING_MASK];
    }
    __DMB();
    uartRxTail = tail + count;

    __disable_irq();
    (void) CDC_Transmit_FS(usbTxPacket, (uint16_t) count);
    __enable_irq();
}

void bridgeInit(void)
{
    applyLineCoding();
}

void bridgePoll(void)
{
    if (lineCodingPending)
    {
        lineCodingPending = false;
        applyLineCoding();
    }
    forwardUsbToUart();
    forwardUartToUsb();
}

void bridgeOnUsbReceive(uint8_t* data, uint32_t length)
{
    usbRxData = data;
    usbRxLength = length;
    usbRxPending = true;
}

void bridgeSetLineCoding(const uint8_t* coding)
{
    memcpy(lineCoding, coding, LINE_CODING_SIZE);
    lineCodingPending = true;
}

void bridgeGetLineCoding(uint8_t* coding)
{
    memcpy(coding, lineCoding, LINE_CODING_SIZE);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart)
{
    if (huart->Instance != USART3) { return; }

    uint32_t head = uartRxHead;
    if (head - uartRxTail < UART_RX_RING_SIZE)
    {
        uartRxRing[head & UART_RX_RING_MASK] = uartRxByte;
        __DMB();
        uartRxHead = head + 1u;
    }
    else
    {
        uartRxDropped = uartRxDropped + 1u;
    }
    startUartRx();
}

// Parity and framing errors leave reception running; overrun aborts it, so re-arm.
void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart)
{
    if (huart->Instance != USART3) { return; }
    uartErrors = uartErrors + 1u;
    if (huart->RxState == HAL_UART_STATE_READY) { startUartRx(); }
}
