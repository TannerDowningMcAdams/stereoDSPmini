#pragma once
#include "status_hal.hpp"
#include <cstddef>
#include <cstdint>

// AN3155 client for the G0's system bootloader on USART3. Blocking with timeouts:
// it runs before audio starts, with nothing else to interleave.
class G0Bootloader {
public:

    struct Config
    {
        UART_HandleTypeDef* uart;   // Reconfigured to 115200 8E1 by init()
    };

    // The step that last ran; on failure, the one that failed.
    enum class Phase : uint8_t
    {
        Idle, Image, Probe, Get, GetId, Erase, Write, Verify, Go, Done
    };

    struct Result
    {
        Status   status          = Status::INIT;
        Phase    phase           = Phase::Idle;
        uint32_t address         = 0;   // Flash address of the failing write or read
        uint16_t chipId          = 0;
        uint8_t  protocolVersion = 0;
        uint8_t  attempts        = 0;   // program() calls since init()
        uint32_t bytesWritten    = 0;   // In the latest attempt
    };

    static constexpr uint32_t kFlashBase    = 0x08000000u;
    static constexpr uint32_t kFlashSize    = 64u * 1024u;
    // FLASH region of STM32G030xx_FLASH.ld. The pages above it hold presets and settings.
    static constexpr uint32_t kAppSize      = 56u * 1024u;
    static constexpr uint32_t kPageSize     = 2048u;
    static constexpr uint32_t kRamBase      = 0x20000000u;
    static constexpr uint32_t kRamSize      = 8u * 1024u;
    static constexpr uint16_t kExpectedChip = 0x0466;   // STM32G03x/G04x, AN2606

    Status init(const Config& config);

    // True if the bootloader answered within windowMs. Sends 0x7F until it does.
    bool probe(uint32_t windowMs);

    // Get ID, erase, write, verify, Go. The first word is written last, so an interrupted
    // session leaves it blank and the G0's empty check boots the bootloader on next power-up.
    Status program(const uint8_t* image, size_t size);

    // After a failed program(): returns a bootloader left mid-command to a command boundary.
    bool resync();

    const Result& result() const { return result_; }

private:

    static constexpr uint8_t kAck       = 0x79;
    static constexpr uint8_t kNack      = 0x1F;
    static constexpr uint8_t kSync      = 0x7F;
    static constexpr uint8_t kCmdGet    = 0x00;
    static constexpr uint8_t kCmdGetId  = 0x02;
    static constexpr uint8_t kCmdRead   = 0x11;
    static constexpr uint8_t kCmdGo     = 0x21;
    static constexpr uint8_t kCmdWrite  = 0x31;
    static constexpr uint8_t kCmdErase  = 0x44;   // Extended Erase
    static constexpr uint32_t kChunk    = 256u;   // AN3155 maximum per read or write

    static constexpr uint32_t kByteTimeoutMs  = 100u;
    static constexpr uint32_t kWriteTimeoutMs = 500u;
    static constexpr uint32_t kEraseTimeoutMs = 5000u;
    static constexpr uint32_t kQuietMs        = 100u;   // Silence that ends a drain
    static constexpr uint32_t kResyncProbeMs  = 100u;

    Config config_ {};
    Result result_ {};

    static bool imageValid(const uint8_t* image, size_t size);
    Status fail(Phase phase, Status status);
    void   flushRx();
    void   drainUntilQuiet();
    Status send(const uint8_t* data, uint16_t length);
    Status receive(uint8_t* data, uint16_t length, uint32_t timeoutMs);
    Status waitAck(uint32_t timeoutMs);
    Status command(uint8_t cmd);
    Status sendAddress(uint32_t address);

    Status get();
    Status getId();
    Status erasePages(uint16_t first, uint16_t count);
    Status writeChunk(uint32_t address, const uint8_t* data, uint32_t length);
    Status verifyChunk(uint32_t address, const uint8_t* data, uint32_t length);
    Status go(uint32_t address);
};
