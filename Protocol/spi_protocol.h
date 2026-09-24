#ifndef SPI_PROTOCOL_H
#define SPI_PROTOCOL_H

#include <assert.h>
#include <stdint.h>

/* Protocol v3 (ui-application-plan.md §5, §5.1). One frame each way every 1 ms, full
 * duplex, 16-bit words. The SPI hardware appends a CRC word; a frame that fails it
 * is dropped by the receiver, which keeps its last good values. */
#define SPI_PACKET_NUM_WORDS   32u

#define SPI_MSG_ID_G0_TO_H7    0x5A9Fu
#define SPI_MSG_ID_H7_TO_G0    0xA5F9u

/* Accepted by the G0 on any CRC-valid H7 frame, whatever its version. */
#define SPI_BOOTLOADER_MAGIC   0xB00710ADu

#define SPI_PARAM_COUNT        8u

/* Words 0-2 of G0ToH7Packet and words 0-4 of H7ToG0Packet keep their positions in
 * every protocol version. They carry the version check and the G0 update path, which
 * must keep working between boards built against different versions.
 *
 * Every field is naturally aligned, so the layout does not depend on the packing. */

typedef struct __attribute__((packed)) {    /* G0 -> H7 */
    uint16_t messageId;                     /* SPI_MSG_ID_G0_TO_H7 */
    uint16_t version;                       /* PROTOCOL_VERSION */
    uint16_t g0FwVersion;                   /* G0_FW_VERSION */
    uint16_t frameSeq;
    uint16_t runFlags;                      /* RUN_FLAG_* */
    uint16_t engineId;                      /* wanted engine, a level (plan §4.1); ENGINE_ID_NONE = none yet */
    uint8_t  presetIndex;                   /* 0..63, for the echo only */
    uint8_t  engineQuery;                   /* registry index whose id the G0 wants */
    uint8_t  defaultsSeq;                   /* incremented to ask for the wanted engine's defaults */
    uint8_t  g0Flags;                       /* G0_FLAG_* */
    uint16_t param[SPI_PARAM_COUNT];        /* normalized 0..65535 */
    uint16_t discrete;                      /* fields per the engine descriptor, engine_link.h */
    uint16_t eventToggles;                  /* each occurrence flips its bit */
    float    tempoHz;
    uint16_t tempoPhase;                    /* phase at this frame's CS rising edge */
    uint16_t ownerMask;                     /* params the panel or MIDI own, for the echo */
    uint16_t reserved[10];
} G0ToH7Packet;

typedef struct __attribute__((packed)) {    /* H7 -> G0 */
    uint16_t messageId;                     /* SPI_MSG_ID_H7_TO_G0 */
    uint16_t version;                       /* PROTOCOL_VERSION */
    uint32_t bootloaderMagic;               /* SPI_BOOTLOADER_MAGIC, or 0 */
    uint16_t h7FwVersion;                   /* H7_FW_VERSION */
    uint16_t frameSeqEcho;                  /* frameSeq of the last valid G0 frame */
    uint16_t h7Flags;                       /* H7_FLAG_* */
    uint16_t activeEngine;                  /* ENGINE_ID_NONE while none is running */
    uint32_t engineDesc;                    /* descriptor of activeEngine, engine_link.h */
    uint8_t  engineCount;                   /* registry size */
    uint8_t  activeIndex;                   /* registry index of activeEngine */
    uint8_t  engineQueryEcho;               /* the engineQuery answered below */
    uint8_t  defaultsSeqEcho;               /* last defaultsSeq applied */
    uint16_t engineQueryId;                 /* id at engineQueryEcho, or ENGINE_ID_NONE past the end */
    uint8_t  presetIndex;                   /* ---- echo of the last applied state ---- */
    uint8_t  reserved8;
    uint16_t runFlags;
    uint16_t param[SPI_PARAM_COUNT];
    uint16_t discrete;
    float    tempoHz;
    uint16_t ownerMask;                     /* ---- end echo ---- */
    uint16_t reserved[5];
} H7ToG0Packet;

static_assert(sizeof(G0ToH7Packet) == SPI_PACKET_NUM_WORDS * 2u, "G0ToH7Packet must be 32 words");
static_assert(sizeof(H7ToG0Packet) == SPI_PACKET_NUM_WORDS * 2u, "H7ToG0Packet must be 32 words");

/* runFlags */
#define RUN_FLAG_ENGAGED          (1u << 0)
#define RUN_FLAG_TRAILS           (1u << 1)     /* trails bypass; clear = true bypass */
#define RUN_FLAG_ANALOG_DRY       (1u << 2)
#define RUN_FLAG_STEREO_IN        (1u << 3)     /* clear = mono in */
#define RUN_FLAG_TEMPO_SRC_SHIFT  4u
#define RUN_FLAG_TEMPO_SRC_MASK   (3u << RUN_FLAG_TEMPO_SRC_SHIFT)

#define TEMPO_SRC_NONE            0u
#define TEMPO_SRC_INTERNAL        1u            /* tap or preset recall */
#define TEMPO_SRC_MIDI            2u

/* g0Flags */
/* A defaults request is outstanding: param[] and discrete are not yet the G0's state,
 * and the H7 ignores them. With this clear, defaultsSeq only records what the G0 has
 * adopted, so an H7 that restarts does not mistake it for a request. */
#define G0_FLAG_DEFAULTS_PENDING  (1u << 0)

/* h7Flags */
#define H7_FLAG_STATE_VALID       (1u << 0)     /* the echo holds a state applied since H7 boot */
#define H7_FLAG_ENGINE_READY      (1u << 1)     /* activeEngine is running */
#define H7_FLAG_LOADING           (1u << 2)     /* an engine switch is in progress */
#define H7_FLAG_ENGINE_UNKNOWN    (1u << 3)     /* the wanted id is not in the registry */

#endif
