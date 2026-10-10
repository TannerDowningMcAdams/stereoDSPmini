#ifndef ENGINE_ABI_H
#define ENGINE_ABI_H

/* Engine ABI, draft 1.0. The contract between a host firmware and a loadable engine
 * module. It is the only interface an engine sees,
 * and it never depends on a board: hosts on different DSP and UI boards run the
 * same engine images.
 *
 * Plain C, fixed-width fields, no bool, no enum-typed fields and no bitfields, so
 * the layout is the same for every conforming compiler. Structs that may grow start
 * with `size`; see "Versioning" below.
 *
 * Draft status: nothing here is frozen until ENG_ABI_MAJOR is released as 1 with
 * ENG_ABI_DRAFT removed. After that, the rules under "Versioning" apply. */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENG_ABI_DRAFT   1
#define ENG_ABI_MAJOR   1u
#define ENG_ABI_MINOR   0u

/* ---- Versioning -------------------------------------------------------------
 *
 * ENG_ABI_MAJOR changes only if the ABI breaks, which orphans every engine built
 * before it. ENG_ABI_MINOR grows when something is appended:
 *
 * - A field at the end of a struct that starts with `size`. A reader uses a field
 *   only when ENG_HAS_FIELD() says the writer's struct contains it; otherwise it
 *   takes the default the field's comment gives.
 * - A slot at the end of eng_module_t or eng_host_t, by the same rule.
 * - A new value for an ENG_* constant or flag. A reader treats an unknown value as
 *   the documented fallback, and a host refuses an engine whose required values it
 *   does not know.
 *
 * Nothing is reordered, resized or removed within a major version. */

#define ENG_HAS_FIELD(ptr, type, field) \
    ((ptr)->size >= offsetof(type, field) + sizeof(((type*) 0)->field))

/* ---- Limits -------------------------------------------------------------------
 *
 * Capacities of the ABI. A host or UI may reach fewer: every param, choice and
 * action has a usable default, and one that a UI cannot reach stays at it. */

#define ENG_MAX_PARAMS      64u
#define ENG_MAX_CHOICES     16u
#define ENG_MAX_OPTIONS     16u     /* per choice */
#define ENG_MAX_ACTIONS     8u
#define ENG_MAX_CHANNELS    4u
#define ENG_MAX_LABELS      64u     /* option labels, shared by all choices */

#define ENG_NAME_LEN        24u     /* bytes including the terminating 0 */
#define ENG_SHORT_LEN       8u      /* for small displays: "FDBK" */
#define ENG_LABEL_LEN       12u
#define ENG_SLUG_LEN        48u

#define ENG_PARAM_WORDS     (ENG_MAX_PARAMS / 32u)

/* ---- Execution environment ------------------------------------------------------
 *
 * What every host guarantees. 48 kHz is the one rate every engine supports and every
 * host runs by default; config.sampleRate is the converter's actual rate, which on a
 * 48 kHz host lies within the range below. A host runs another rate only with
 * engines whose range includes it. */

#define ENG_RATE_48K_MIN        47880u      /* 48 kHz - 0.25% */
#define ENG_RATE_48K_MAX        48120u      /* 48 kHz + 0.25% */
#define ENG_STACK_PROCESS       4096u       /* bytes of stack free on entry to process() */
#define ENG_STACK_THREAD        8192u       /* on entry to every thread-mode entry point */

/* ---- Identity -----------------------------------------------------------------
 *
 * An id names one engine and one meaning of its controls. Presets store it, so an
 * id is never reused, and an engine version whose params, choices or actions no
 * longer mean what stored presets expect takes a new id. The upper 16 bits name the
 * author from a public ledger; the author assigns the lower 16. */

#define ENG_ID_NONE             0x00000000u
#define ENG_ID(author, engine)  ((((uint32_t) (author)) << 16) | ((uint32_t) (engine) & 0xFFFFu))
#define ENG_AUTHOR_FIRST_PARTY  0x0001u
#define ENG_AUTHOR_LOCAL        0xFFFFu     /* unregistered; never shared */

#define ENG_VERSION(major, minor, patch) \
    ((((uint32_t) (major) & 0xFFu) << 24) | (((uint32_t) (minor) & 0xFFu) << 16) | ((uint32_t) (patch) & 0xFFFFu))

/* ---- Memory classes -------------------------------------------------------------
 *
 * What an image section or an allocation is guaranteed. A host may place memory in
 * a class with stronger guarantees than requested, never weaker. An engine whose
 * needs a host cannot meet is incompatible and is never listed. */

#define ENG_MEM_FAST            0u      /* zero wait, deterministic: TCM. Small hot state and code. */
#define ENG_MEM_INTERNAL        1u      /* on-chip, behind the cache: SRAM, or flash for code and rodata */
#define ENG_MEM_EXTERNAL        2u      /* off-chip, behind the cache: large buffers, slow random access */
#define ENG_MEM_CLASS_SLOTS     4u      /* array size; slot 3 is reserved and must be 0 */

/* ---- Profiles ---------------------------------------------------------------------
 *
 * The minimum budget per class that every host of a profile provides. An engine
 * within a profile runs on every host that meets it; the packer marks a larger one
 * as host-specific. Read-only is code and rodata, read-write is data, .bss and
 * alloc(). A profile's numbers never change once released. */

#define ENG_PROFILE1_FAST_CODE          (56u * 1024u)
#define ENG_PROFILE1_FAST_DATA          (96u * 1024u)
#define ENG_PROFILE1_INTERNAL_RW        (320u * 1024u)
#define ENG_PROFILE1_INTERNAL_RO        (768u * 1024u)
#define ENG_PROFILE1_EXTERNAL           (8u * 1024u * 1024u)

/* ---- Audio layouts ------------------------------------------------------------- */

#define ENG_IO_STEREO           (1u << 0)   /* 2 in, 2 out */

/* ---- Static description ---------------------------------------------------------
 *
 * Everything a host needs to list, check and drive an engine without running it.
 * Pointer-free, so the packer copies it into the image header and a host reads it
 * in place from storage. */

#define ENG_INFO_USES_TEMPO     (1u << 0)   /* the UI should keep and show a tempo */
#define ENG_INFO_MIDI_IN        (1u << 1)   /* the engine takes MIDI messages, eng_block_t.midi */

/* A param's order is its priority: a UI with 5 knobs reaches params 0 to 4. How a
 * value maps to what the engine does is the engine's alone, and display text comes
 * from eng_controller_t.formatParam().
 *
 * A stepped param has `steps` positions, for knobs with detents, encoders and MIDI.
 * Position k is sent as k / (steps - 1), and the engine takes a value v as position
 * min(steps - 1, floor(v * steps)), which gives every position an equal share of a
 * knob's travel. This is VST3's convention, with steps = stepCount + 1. */

typedef struct {
    uint16_t defaultValue;      /* normalized 0..65535, the unit presets and links use */
    uint16_t steps;             /* 0 = continuous; else 2..65535 positions */
    uint8_t  reserved[4];       /* 0 */
} eng_param_t;

#define ENG_CHOICE_PRIMARY      (1u << 0)   /* the choice a mode switch should drive; at most one */

typedef struct {
    uint8_t  options;           /* 2..ENG_MAX_OPTIONS */
    uint8_t  defaultOption;
    uint8_t  flags;             /* ENG_CHOICE_* */
    uint8_t  reserved;          /* 0 */
} eng_choice_t;

/* Actions: what an engine would do with a spare footswitch or button, in priority
 * order. A UI with one spare switch takes action 0. */
#define ENG_ACTION_NONE         0u
#define ENG_ACTION_TAP_TEMPO    1u  /* target unused */
#define ENG_ACTION_TOGGLE       2u  /* flips choice `target`, which has 2 options */
#define ENG_ACTION_MOMENTARY    3u  /* choice `target` (2 options) is 1 while held */
#define ENG_ACTION_TRIGGER      4u  /* sets bit `target` of eng_block_t.triggered */

typedef struct {
    uint8_t  kind;              /* ENG_ACTION_* */
    uint8_t  target;
    uint8_t  reserved[2];       /* 0 */
} eng_action_t;

#define ENG_BLEND_NONE          0xFFu

typedef struct {
    uint32_t size;              /* sizeof(eng_info_t) as built */
    uint32_t id;                /* ENG_ID() */
    uint32_t version;           /* ENG_VERSION(); for display and updates only */
    uint32_t flags;             /* ENG_INFO_* */
    /* The engine runs at any rate in [min, max], which includes
     * [ENG_RATE_48K_MIN, ENG_RATE_48K_MAX]. */
    uint32_t sampleRateMin;
    uint32_t sampleRateMax;
    uint32_t ioLayouts;         /* ENG_IO_*, every layout the engine supports */
    /* Worst case the engine takes through eng_host_t.alloc() at sampleRateMax, per
     * ENG_MEM_* class. Image sections are counted separately, from the image. */
    uint32_t allocBytes[ENG_MEM_CLASS_SLOTS];
    uint8_t  paramCount;
    uint8_t  choiceCount;
    uint8_t  actionCount;
    /* The param the host mixes dry against wet by, or ENG_BLEND_NONE to run fully
     * wet. With a blend param the engine outputs wet only. An analog dry path leads
     * the wet signal by a few ms, so engines that mix internally (phasers, flangers,
     * chorus) declare none. */
    uint8_t  blendParam;
    eng_param_t  param[ENG_MAX_PARAMS];
    eng_choice_t choice[ENG_MAX_CHOICES];
    eng_action_t action[ENG_MAX_ACTIONS];
} eng_info_t;

/* ---- UI description ---------------------------------------------------------------
 *
 * Names and labels for UIs that can show them. Optional, pointer-free, and read
 * from storage only when a UI asks, so its size costs no RAM. All strings are
 * 0-terminated UTF-8. Values are shown through eng_controller_t.formatParam(). */

typedef struct {
    char     name[ENG_NAME_LEN];
    char     shortName[ENG_SHORT_LEN];
} eng_param_ui_t;

typedef struct {
    char     name[ENG_NAME_LEN];
    char     shortName[ENG_SHORT_LEN];
    uint8_t  firstLabel;        /* labels [firstLabel, firstLabel + options) in eng_ui_t.label */
    uint8_t  reserved[3];       /* 0 */
} eng_choice_ui_t;

typedef struct {
    char     name[ENG_NAME_LEN];
    char     shortName[ENG_SHORT_LEN];
} eng_action_ui_t;

typedef struct {
    uint32_t size;              /* sizeof(eng_ui_t) as built */
    char     name[ENG_NAME_LEN];
    char     author[ENG_NAME_LEN];
    char     slug[ENG_SLUG_LEN];    /* "org.example.plate": stable, matches the id */
    eng_param_ui_t  param[ENG_MAX_PARAMS];
    eng_choice_ui_t choice[ENG_MAX_CHOICES];
    eng_action_ui_t action[ENG_MAX_ACTIONS];
    /* Last, so a later minor version may grow the pool. */
    char     label[ENG_MAX_LABELS][ENG_LABEL_LEN];
} eng_ui_t;

/* ---- Host services ------------------------------------------------------------------
 *
 * Passed to activate(). Valid until deactivate() returns. Every service is for
 * thread mode only: process() calls none of them. */

typedef struct eng_host eng_host_t;

struct eng_host {
    uint32_t size;              /* sizeof(eng_host_t) as built by the host */
    uint16_t abiMajor;
    uint16_t abiMinor;
    /* Zeroed memory of `memClass`, aligned to `align` (a power of two; 0 = 32).
     * NULL once the engine has taken more than eng_info_t.allocBytes declares for the
     * class; the host then refuses the activation whatever activate() returns. */
    void*   (*alloc)(const eng_host_t* host, uint32_t memClass, uint32_t bytes, uint32_t align);
    /* Assets packed in the engine's image, by index. Size in bytes, or -1 for no
     * such asset. */
    int32_t (*assetSize)(const eng_host_t* host, uint32_t asset);
    /* Copies bytes from an asset into dst. Returns the bytes copied, or -1. */
    int32_t (*assetRead)(const eng_host_t* host, uint32_t asset, uint32_t offset, void* dst, uint32_t bytes);
    /* A line of diagnostic text. A host may drop it. */
    void    (*log)(const eng_host_t* host, const char* text);
};

/* ---- Activation ------------------------------------------------------------------- */

typedef struct {
    uint32_t size;              /* sizeof(eng_config_t) as built by the host */
    uint32_t sampleRate;        /* within [sampleRateMin, sampleRateMax] */
    uint32_t maxFrames;         /* process() is called with 1..maxFrames frames */
    uint32_t ioLayout;          /* one ENG_IO_* bit, from eng_info_t.ioLayouts */
    uint32_t randomSeed;        /* from a hardware RNG where the host has one */
} eng_config_t;

/* ---- Per-block controls ------------------------------------------------------------
 *
 * Built by the host, read-only for the engine, and complete on every block. The
 * change masks and `triggered` describe this block only. */

#define ENG_BLOCK_ENGAGED       (1u << 0)
/* The first block after activate(). The masks are clear on it: initialise from
 * param and choice, and snap any smoothing. */
#define ENG_BLOCK_FIRST         (1u << 1)

/* One MIDI channel voice message, for engines with ENG_INFO_MIDI_IN. */
typedef struct {
    uint32_t frame;             /* frame of the block it applies at */
    uint8_t  status;            /* 0x80..0xEF */
    uint8_t  data1;
    uint8_t  data2;             /* 0 for two-byte messages */
    uint8_t  reserved;          /* 0 */
} eng_midi_t;

typedef struct {
    uint32_t size;                              /* sizeof(eng_block_t) as built by the host */
    uint32_t flags;                             /* ENG_BLOCK_* */
    float    param[ENG_MAX_PARAMS];             /* 0..1; defaults past paramCount */
    uint8_t  choice[ENG_MAX_CHOICES];           /* always < the choice's options */
    uint32_t paramsChanged[ENG_PARAM_WORDS];    /* bit i % 32 of word i / 32: param i changed */
    uint32_t choicesChanged;                    /* bit i: choice i changed */
    uint32_t triggered;                         /* bit i: trigger i fired since the last block */
    int32_t  beatFrame;                         /* frame at which a beat falls, or -1 */
    float    beatPhase;                         /* 0..1 at frame 0; 0 while tempoHz is 0 */
    float    tempoHz;                           /* 0 while no tempo is set */
    /* The messages routed to the engine for this block, in frame order; 0 and NULL
     * without ENG_INFO_MIDI_IN. The UI and host decide the routing: a controller
     * mapped to a param or choice arrives as that param or choice. */
    uint32_t          midiCount;
    const eng_midi_t* midi;
} eng_block_t;

#define ENG_PARAM_CHANGED(block, i) \
    ((((block)->paramsChanged[(i) / 32u]) >> ((i) % 32u)) & 1u)

/* ---- Controller ----------------------------------------------------------------------
 *
 * Entry points about the engine's controls, separate from its audio processing, as
 * VST3 separates its edit controller from its processor. They take no instance and
 * touch no read-write memory: code, rodata and their arguments only. A host may
 * therefore call them while the engine is not active, from code held in flash. */

typedef struct {
    uint32_t size;              /* sizeof(eng_controller_t) as built */
    /* Thread mode. Writes the display text for `value` (0..1) of `param`, given the
     * choices in `choice`, and returns its length; `capacity` tells it how short to
     * be. NULL when the engine shows no values; a UI then shows the position alone. */
    uint32_t (*formatParam)(uint32_t param, float value, const uint8_t* choice,
                            char* text, uint32_t capacity);
} eng_controller_t;

/* ---- Module entry table --------------------------------------------------------------
 *
 * The one symbol a module exports. Every entry point runs with the FPU in
 * flush-to-zero and default-NaN mode, round to nearest; an engine does not change
 * FPSCR. An engine reads and writes only its own image sections, what alloc() gave
 * it, the buffers process() is given and the host's tables, and touches no
 * peripheral or system register.
 *
 * Concurrency: process() may preempt background() and the controller, never the
 * reverse. activate() and deactivate() never overlap process(). */

typedef struct {
    uint32_t size;              /* sizeof(eng_module_t) as built */
    uint16_t abiMajor;          /* ENG_ABI_MAJOR the module was built against */
    uint16_t abiMinor;
    const eng_info_t*       info;
    const eng_ui_t*         ui;         /* NULL when the engine has none */
    const eng_controller_t* controller; /* NULL when the engine has none */

    /* Thread mode, any duration. Returns the instance every other entry point takes,
     * or NULL on failure. Image sections are loaded and zero-filled, and static
     * constructors have run, before the first call after a load. A host may call it
     * again after deactivate() without loading again (a resident engine always is),
     * so it sets up all the state it relies on. */
    void* (*activate)(const eng_host_t* host, const eng_config_t* config);
    /* Thread mode. NULL when there is nothing to do. */
    void  (*deactivate)(void* self);
    /* Audio context, every block while the engine is active. Bounded time, no host
     * services, no blocking. Writes every frame of every output channel. `in` and
     * `out` hold one pointer per channel of the configured layout, never alias, and
     * are 32-byte aligned. */
    void  (*process)(void* self, const float* const* in, float* const* out, uint32_t frames,
                     const eng_block_t* block);
    /* Thread mode, called repeatedly while the engine is active, for slow work such
     * as loading an asset a choice selected. NULL when not needed. The engine hands
     * results to process() itself. */
    void  (*background)(void* self);
    /* Thread mode, after activate(). Frames by which the wet output lags the input,
     * constant until the next activate(); a pitch shifter's window, for example. A host
     * may use it to align the dry path. NULL for none. */
    uint32_t (*latency)(void* self);
} eng_module_t;

/* The exported entry table's symbol and section, found by the packer. A resident
 * engine, linked into a host for bring-up or host tests, defines ENG_RESIDENT_NAME
 * so that each one gets its own symbol. */
#define ENG_MODULE_SECTION  ".eng_module"
#define ENG_CAT_(a, b)      a##b
#define ENG_CAT(a, b)       ENG_CAT_(a, b)
#if defined(ENG_RESIDENT_NAME)
#define ENG_MODULE_SYMBOL   ENG_CAT(eng_module_, ENG_RESIDENT_NAME)
#else
#define ENG_MODULE_SYMBOL   eng_module
#endif

/* ---- Layout checks --------------------------------------------------------------------
 *
 * The images run on 32-bit targets. Host tests on 64-bit machines compile engines
 * from source, where only the pointer-free structs keep this layout. */

static_assert(ENG_MAX_PARAMS % 32u == 0u, "paramsChanged holds whole words");
static_assert(ENG_MAX_CHOICES <= 32u && ENG_MAX_ACTIONS <= 32u, "one mask word each");
static_assert(ENG_MAX_OPTIONS <= 256u && ENG_MAX_LABELS <= 256u, "uint8_t indices");

static_assert(sizeof(eng_param_t) == 8u, "eng_param_t layout");
static_assert(sizeof(eng_choice_t) == 4u, "eng_choice_t layout");
static_assert(sizeof(eng_action_t) == 4u, "eng_action_t layout");
static_assert(offsetof(eng_info_t, paramCount) == 44u, "eng_info_t layout");
static_assert(offsetof(eng_info_t, param) == 48u, "eng_info_t layout");
static_assert(sizeof(eng_info_t) == 48u + 8u * ENG_MAX_PARAMS + 4u * ENG_MAX_CHOICES + 4u * ENG_MAX_ACTIONS,
              "eng_info_t layout");
static_assert(sizeof(eng_param_ui_t) == ENG_NAME_LEN + ENG_SHORT_LEN, "eng_param_ui_t layout");
static_assert(sizeof(eng_config_t) == 20u, "eng_config_t layout");
static_assert(sizeof(eng_midi_t) == 8u, "eng_midi_t layout");
static_assert(offsetof(eng_block_t, choice) == 8u + 4u * ENG_MAX_PARAMS, "eng_block_t layout");
static_assert(offsetof(eng_block_t, midiCount) == 8u + 4u * ENG_MAX_PARAMS + ENG_MAX_CHOICES + 4u * ENG_PARAM_WORDS + 20u,
              "eng_block_t layout");

#if UINTPTR_MAX == 0xFFFFFFFFu
static_assert(sizeof(eng_block_t) == offsetof(eng_block_t, midiCount) + 8u, "eng_block_t layout");
static_assert(sizeof(eng_host_t) == 24u, "eng_host_t layout");
static_assert(sizeof(eng_controller_t) == 8u, "eng_controller_t layout");
static_assert(sizeof(eng_module_t) == 40u, "eng_module_t layout");
#endif

#ifdef __cplusplus
}
#endif

#endif
