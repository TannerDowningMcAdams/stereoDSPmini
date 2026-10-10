#ifndef ENGINE_IMAGE_H
#define ENGINE_IMAGE_H

/* Engine image format, draft 1. The file an engine is distributed as: one engine per image, written by the SDK packer, read by a host's
 * loader. How a host stores images (flash layout, directory, install) is the host's
 * business and is not part of this format.
 *
 * All fields are little-endian. Offsets are from the start of the image. The format
 * version is separate from the ABI version, so a loader may accept several formats
 * of the same ABI. */

#include <assert.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENG_IMAGE_MAGIC         0x31474E45u     /* "ENG1" */
#define ENG_IMAGE_FORMAT        1u

/* Instruction set, FPU and calling convention the code was built for. */
#define ENG_ARCH_ARMV7EM_DP_HARD    1u  /* Cortex-M7, fpv5-d16, hard-float AAPCS, Thumb-2 */

/* ---- Sections ------------------------------------------------------------------
 *
 * Each section is placed as one block in memory of its class (engine_abi.h,
 * ENG_MEM_*). Distances inside a section are kept, so PC-relative references within
 * it need no relocation. The packer refuses a PC-relative reference between
 * sections. */

#define ENG_SECTION_CODE        0u  /* executable, read-only after load */
#define ENG_SECTION_RODATA      1u  /* read-only after load */
#define ENG_SECTION_DATA        2u  /* read-write; memSize past fileSize is zeroed (.bss) */

#define ENG_MAX_SECTIONS        8u

/* Placement hints. A host that does not know a flag may ignore it: each one affects
 * speed only. */
#define ENG_SECTION_KEEP_RAM    (1u << 0)   /* a read-only section a host keeps out of flash */

typedef struct {
    uint32_t fileOffset;        /* 0 when fileSize is 0 */
    uint32_t fileSize;
    uint32_t memSize;           /* >= fileSize */
    uint8_t  kind;              /* ENG_SECTION_* */
    uint8_t  memClass;          /* ENG_MEM_* */
    uint8_t  alignLog2;         /* the section's load address is a multiple of 1 << alignLog2 */
    uint8_t  flags;             /* ENG_SECTION_KEEP_RAM etc. */
} eng_section_t;

/* ---- Relocations -----------------------------------------------------------------
 *
 * The packer stores each relocated value as an offset from the base of its target
 * section. The loader adds the target's load address, which keeps the Thumb bit of
 * function pointers. */

#define ENG_RELOC_ABS32         0u  /* 32-bit word at `offset` */
#define ENG_RELOC_MOVW_MOVT     1u  /* MOVW at `offset` and MOVT at `offset2`, one 32-bit value */

typedef struct {
    uint32_t offset;            /* within `section` */
    uint8_t  section;           /* where the value is */
    uint8_t  target;            /* the section it points into */
    uint8_t  type;              /* ENG_RELOC_* */
    uint8_t  reserved;          /* 0 */
    uint32_t offset2;           /* ENG_RELOC_MOVW_MOVT only; else 0 */
} eng_reloc_t;

/* ---- Assets -----------------------------------------------------------------------
 *
 * Data the engine reads on demand through eng_host_t.assetRead(): impulse
 * responses, model weights, tables too large to load with the code. Never loaded
 * with the sections. */

typedef struct {
    uint32_t offset;
    uint32_t size;
    uint32_t crc;               /* CRC-32 of the asset's bytes */
    uint32_t reserved;          /* 0 */
} eng_asset_t;

/* ---- Header ------------------------------------------------------------------------ */

typedef struct {
    uint32_t magic;             /* ENG_IMAGE_MAGIC */
    uint16_t format;            /* ENG_IMAGE_FORMAT */
    uint16_t headerSize;        /* sizeof(eng_image_header_t) as written */
    uint32_t imageSize;         /* the whole image, header included */
    uint32_t headerCrc;         /* CRC-32 of the header with this field 0 */
    uint32_t imageCrc;          /* CRC-32 of bytes [headerSize, imageSize) */
    uint16_t arch;              /* ENG_ARCH_* */
    uint16_t abiMajor;          /* from the module's entry table */
    uint16_t abiMinor;
    uint16_t sectionCount;      /* 1..ENG_MAX_SECTIONS */

    uint32_t infoOffset;        /* copy of the module's eng_info_t, readable in place */
    uint32_t infoSize;
    uint32_t uiOffset;          /* copy of its eng_ui_t, or 0 */
    uint32_t uiSize;
    uint32_t sectionOffset;     /* eng_section_t[sectionCount] */
    uint32_t relocOffset;       /* eng_reloc_t[relocCount] */
    uint32_t relocCount;
    uint32_t assetOffset;       /* eng_asset_t[assetCount] */
    uint32_t assetCount;

    /* The module's eng_module_t, after relocation. */
    uint16_t entrySection;
    uint16_t initSection;       /* static constructors: initCount function pointers */
    uint32_t entryOffset;
    uint32_t initOffset;
    uint32_t initCount;

    /* Reserved for a signature over the image. 0 in format 1. */
    uint32_t signatureOffset;
    uint32_t signatureSize;
    uint32_t reserved[4];       /* 0 */
} eng_image_header_t;

static_assert(sizeof(eng_section_t) == 16u, "eng_section_t layout");
static_assert(sizeof(eng_reloc_t) == 12u, "eng_reloc_t layout");
static_assert(sizeof(eng_asset_t) == 16u, "eng_asset_t layout");
static_assert(sizeof(eng_image_header_t) == 104u, "eng_image_header_t layout");

#ifdef __cplusplus
}
#endif

#endif
