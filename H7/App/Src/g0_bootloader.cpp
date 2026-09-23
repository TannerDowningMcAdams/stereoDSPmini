#include "g0_bootloader.hpp"
#include <cstring>

Status G0Bootloader::init(const Config& config)
{
    config_ = config;
    result_ = Result {};

    // AN3155 is 8 data bits + even parity; on STM32 the parity bit counts toward the word length.
    UART_HandleTypeDef* uart = config_.uart;
    uart->Init.BaudRate   = 115200;
    uart->Init.WordLength = UART_WORDLENGTH_9B;
    uart->Init.Parity     = UART_PARITY_EVEN;
    uart->Init.StopBits   = UART_STOPBITS_1;
    if (HAL_UART_Init(uart) != HAL_OK) { return Status::ERROR; }
    // The FIFO absorbs a Read Memory burst between polling calls.
    return fromHAL(HAL_UARTEx_EnableFifoMode(uart));
}

bool G0Bootloader::probe(uint32_t windowMs)
{
    result_.phase = Phase::Probe;
    const uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < windowMs)
    {
        flushRx();
        uint8_t reply = 0;
        if (send(&kSync, 1) == Status::OK && receive(&reply, 1, 20u) == Status::OK)
        {
            // NACK means a session already autobauded and is waiting for a command.
            if (reply == kAck || reply == kNack) { return true; }
        }
    }
    result_.status = Status::TIMEOUT;
    return false;
}

Status G0Bootloader::program(const uint8_t* image, size_t size)
{
    if (image == nullptr || size == 0 || size > kFlashSize) { return fail(Phase::Idle, Status::ERROR); }

    Status status = get();
    if (status != Status::OK) { return fail(Phase::Get, status); }

    status = getId();
    if (status != Status::OK) { return fail(Phase::GetId, status); }
    if (result_.chipId != kExpectedChip) { return fail(Phase::GetId, Status::ERROR); }

    const uint16_t pages = static_cast<uint16_t>((size + kPageSize - 1u) / kPageSize);
    status = erasePages(0, pages);
    if (status != Status::OK) { return fail(Phase::Erase, status); }

    // Chunk 0 last: until it lands, the first word stays erased.
    result_.phase = Phase::Write;
    for (uint32_t offset = kChunk; offset < size; offset += kChunk)
    {
        const uint32_t length = (size - offset < kChunk) ? static_cast<uint32_t>(size - offset) : kChunk;
        status = writeChunk(kFlashBase + offset, image + offset, length);
        if (status != Status::OK) { return fail(Phase::Write, status); }
    }
    status = writeChunk(kFlashBase, image, (size < kChunk) ? static_cast<uint32_t>(size) : kChunk);
    if (status != Status::OK) { return fail(Phase::Write, status); }

    result_.phase = Phase::Verify;
    for (uint32_t offset = 0; offset < size; offset += kChunk)
    {
        const uint32_t length = (size - offset < kChunk) ? static_cast<uint32_t>(size - offset) : kChunk;
        status = verifyChunk(kFlashBase + offset, image + offset, length);
        if (status != Status::OK) { return fail(Phase::Verify, status); }
    }

    status = go(kFlashBase);
    if (status != Status::OK) { return fail(Phase::Go, status); }

    result_.phase  = Phase::Done;
    result_.status = Status::OK;
    return Status::OK;
}

Status G0Bootloader::fail(Phase phase, Status status)
{
    result_.phase  = phase;
    result_.status = status;
    return status;
}

void G0Bootloader::flushRx()
{
    __HAL_UART_CLEAR_FLAG(config_.uart, UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_PEF | UART_CLEAR_FEF);
    __HAL_UART_SEND_REQ(config_.uart, UART_RXDATA_FLUSH_REQUEST);
}

Status G0Bootloader::send(const uint8_t* data, uint16_t length)
{
    return fromHAL(HAL_UART_Transmit(config_.uart, data, length, kByteTimeoutMs + length));
}

Status G0Bootloader::receive(uint8_t* data, uint16_t length, uint32_t timeoutMs)
{
    return fromHAL(HAL_UART_Receive(config_.uart, data, length, timeoutMs));
}

Status G0Bootloader::waitAck(uint32_t timeoutMs)
{
    uint8_t reply = 0;
    const Status status = receive(&reply, 1, timeoutMs);
    if (status != Status::OK) { return status; }
    return (reply == kAck) ? Status::OK : Status::ERROR;
}

Status G0Bootloader::command(uint8_t cmd)
{
    flushRx();
    const uint8_t frame[2] = { cmd, static_cast<uint8_t>(~cmd) };
    const Status status = send(frame, sizeof(frame));
    if (status != Status::OK) { return status; }
    return waitAck(kByteTimeoutMs);
}

Status G0Bootloader::sendAddress(uint32_t address)
{
    uint8_t frame[5] = {
        static_cast<uint8_t>(address >> 24), static_cast<uint8_t>(address >> 16),
        static_cast<uint8_t>(address >> 8),  static_cast<uint8_t>(address),
        0
    };
    frame[4] = frame[0] ^ frame[1] ^ frame[2] ^ frame[3];
    const Status status = send(frame, sizeof(frame));
    if (status != Status::OK) { return status; }
    return waitAck(kByteTimeoutMs);
}

// Reply: N (= bytes - 1), version, N command codes, ACK.
Status G0Bootloader::get()
{
    Status status = command(kCmdGet);
    if (status != Status::OK) { return status; }

    uint8_t count = 0;
    status = receive(&count, 1, kByteTimeoutMs);
    if (status != Status::OK) { return status; }

    uint8_t reply[32] = {};
    const uint16_t length = static_cast<uint16_t>(count) + 1u;
    if (length > sizeof(reply)) { return Status::ERROR; }
    status = receive(reply, length, kByteTimeoutMs);
    if (status != Status::OK) { return status; }
    result_.protocolVersion = reply[0];

    // Refuse before erasing anything if a command this sequence needs is missing.
    const uint8_t required[] = { kCmdGetId, kCmdRead, kCmdGo, kCmdWrite, kCmdErase };
    for (uint8_t cmd : required)
    {
        if (std::memchr(&reply[1], cmd, length - 1u) == nullptr) { return Status::ERROR; }
    }
    return waitAck(kByteTimeoutMs);
}

// Reply: N (= 1), PID MSB, PID LSB, ACK.
Status G0Bootloader::getId()
{
    Status status = command(kCmdGetId);
    if (status != Status::OK) { return status; }

    uint8_t reply[3] = {};
    status = receive(reply, sizeof(reply), kByteTimeoutMs);
    if (status != Status::OK) { return status; }
    if (reply[0] != 1u) { return Status::ERROR; }
    result_.chipId = static_cast<uint16_t>((reply[1] << 8) | reply[2]);
    return waitAck(kByteTimeoutMs);
}

// Page list, not mass erase: bounded, and supported by every Extended Erase implementation.
Status G0Bootloader::erasePages(uint16_t first, uint16_t count)
{
    result_.phase = Phase::Erase;
    Status status = command(kCmdErase);
    if (status != Status::OK) { return status; }

    uint8_t frame[2 + 2 * (kFlashSize / kPageSize) + 1] = {};
    const uint16_t n = static_cast<uint16_t>(count - 1u);
    uint32_t length = 0;
    frame[length++] = static_cast<uint8_t>(n >> 8);
    frame[length++] = static_cast<uint8_t>(n);
    for (uint16_t page = first; page < first + count; page++)
    {
        frame[length++] = static_cast<uint8_t>(page >> 8);
        frame[length++] = static_cast<uint8_t>(page);
    }
    uint8_t checksum = 0;
    for (uint32_t i = 0; i < length; i++) { checksum ^= frame[i]; }
    frame[length++] = checksum;

    status = send(frame, static_cast<uint16_t>(length));
    if (status != Status::OK) { return status; }
    return waitAck(kEraseTimeoutMs);
}

// G0 flash programs double words, so the tail is padded to 8 bytes with erased-state 0xFF.
Status G0Bootloader::writeChunk(uint32_t address, const uint8_t* data, uint32_t length)
{
    result_.address = address;
    uint8_t frame[1 + kChunk + 1];
    const uint32_t padded = (length + 7u) & ~7u;
    std::memset(&frame[1], 0xFF, padded);
    std::memcpy(&frame[1], data, length);
    frame[0] = static_cast<uint8_t>(padded - 1u);
    uint8_t checksum = 0;
    for (uint32_t i = 0; i < padded + 1u; i++) { checksum ^= frame[i]; }
    frame[padded + 1u] = checksum;

    Status status = command(kCmdWrite);
    if (status != Status::OK) { return status; }
    status = sendAddress(address);
    if (status != Status::OK) { return status; }
    status = send(frame, static_cast<uint16_t>(padded + 2u));
    if (status != Status::OK) { return status; }
    status = waitAck(kWriteTimeoutMs);
    if (status == Status::OK) { result_.bytesWritten += length; }
    return status;
}

Status G0Bootloader::verifyChunk(uint32_t address, const uint8_t* data, uint32_t length)
{
    result_.address = address;
    Status status = command(kCmdRead);
    if (status != Status::OK) { return status; }
    status = sendAddress(address);
    if (status != Status::OK) { return status; }

    const uint8_t n = static_cast<uint8_t>(length - 1u);
    const uint8_t frame[2] = { n, static_cast<uint8_t>(~n) };
    status = send(frame, sizeof(frame));
    if (status != Status::OK) { return status; }
    status = waitAck(kByteTimeoutMs);
    if (status != Status::OK) { return status; }

    uint8_t readBack[kChunk];
    status = receive(readBack, static_cast<uint16_t>(length), kByteTimeoutMs + length);
    if (status != Status::OK) { return status; }
    return (std::memcmp(readBack, data, length) == 0) ? Status::OK : Status::ERROR;
}

// The bootloader ACKs the address, then jumps; there is no further reply.
Status G0Bootloader::go(uint32_t address)
{
    Status status = command(kCmdGo);
    if (status != Status::OK) { return status; }
    return sendAddress(address);
}
