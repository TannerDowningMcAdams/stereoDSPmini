#include "storage.h"
#include "storage_flash.h"
#include <stddef.h>
#include <string.h>

// Each page starts with a page record, whose seq orders the pages, followed by 63
// data records appended in order. Pages are used as a ring. The page after the head
// is always kept free, so the head can move on without an erase of live data: when
// it does, the live records of the oldest page (now the one after the head) are
// copied into the new head, and the oldest page is erased.
//
// Every record, copies included, takes the next seq, so all the records in flash lie
// within the last 4 pages of writes and a 16-bit seq compares safely across its wrap.

#define RECORD_SIZE         32u
#define RECORDS_PER_PAGE    (STORAGE_FLASH_PAGE_SIZE / RECORD_SIZE)
#define DWORDS_PER_RECORD   (RECORD_SIZE / 8u)
#define LOCATION_COUNT      (STORAGE_FLASH_PAGE_COUNT * RECORDS_PER_PAGE)

#define TYPE_PAGE           0x50u

// One key per (type, slot).
#define KEY_PRESET          0u
#define KEY_FAVOURITE       (KEY_PRESET + STORAGE_PRESET_SLOTS)
#define KEY_SETTINGS        (KEY_FAVOURITE + 1u)
#define KEY_COUNT           (KEY_SETTINGS + STORAGE_SETTINGS_SLOTS)
#define KEY_NONE            0xFFu

#define LOCATION_NONE       0xFFFFu

typedef struct {
    uint8_t  type;
    uint8_t  slot;
    uint8_t  schemaVer;
    uint8_t  reserved;
    uint16_t seq;
    uint16_t crc;           // CRC-16/CCITT over the other 30 bytes
    uint8_t  payload[STORAGE_PAYLOAD_SIZE];
} Record;

_Static_assert(sizeof(Record) == RECORD_SIZE, "a record is four flash double words");
_Static_assert(KEY_COUNT < KEY_NONE, "keys fit a byte");
_Static_assert(LOCATION_COUNT < LOCATION_NONE, "locations fit 16 bits");

typedef enum {
    SLOT_BLANK,
    SLOT_VALID,
    SLOT_INVALID            // torn, corrupt, or unreadable
} SlotState;

// Location of the newest valid record for each key: page * RECORDS_PER_PAGE + record.
static uint16_t keyLocation[KEY_COUNT];
static uint16_t keySeq[KEY_COUNT];          // used while mounting

static bool     pageValid[STORAGE_FLASH_PAGE_COUNT];
static uint16_t pageSeq[STORAGE_FLASH_PAGE_COUNT];
static uint8_t  head;
static uint8_t  headNext;                   // next record in the head; RECORDS_PER_PAGE = full
static uint16_t nextSeq;

static uint8_t keyOf(uint8_t type, uint8_t slot)
{
    switch (type)
    {
        case STORAGE_TYPE_PRESET:    return (slot < STORAGE_PRESET_SLOTS) ? (uint8_t) (KEY_PRESET + slot) : KEY_NONE;
        case STORAGE_TYPE_FAVOURITE: return (slot == 0u) ? (uint8_t) KEY_FAVOURITE : KEY_NONE;
        case STORAGE_TYPE_SETTINGS:  return (slot < STORAGE_SETTINGS_SLOTS) ? (uint8_t) (KEY_SETTINGS + slot) : KEY_NONE;
        default:                     return KEY_NONE;
    }
}

static bool newer(uint16_t a, uint16_t b)
{
    return (int16_t) (a - b) > 0;
}

static uint8_t pageOf(uint16_t location)
{
    return (uint8_t) (location / RECORDS_PER_PAGE);
}

static uint16_t locationOf(uint8_t page, uint8_t record)
{
    return (uint16_t) (page * RECORDS_PER_PAGE + record);
}

static uint16_t crcBytes(uint16_t crc, const uint8_t* data, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++)
    {
        crc ^= (uint16_t) ((uint16_t) data[i] << 8);
        for (uint8_t bit = 0; bit < 8u; bit++)
        {
            crc = (crc & 0x8000u) ? (uint16_t) ((crc << 1) ^ 0x1021u) : (uint16_t) (crc << 1);
        }
    }
    return crc;
}

static uint16_t recordCrc(const Record* record)
{
    const uint16_t crc = crcBytes(0xFFFFu, (const uint8_t*) record, offsetof(Record, crc));
    return crcBytes(crc, record->payload, sizeof(record->payload));
}

static SlotState readRecord(uint16_t location, Record* record)
{
    if (!storageFlashRead((uint32_t) location * RECORD_SIZE, record, RECORD_SIZE)) { return SLOT_INVALID; }

    const uint8_t* bytes = (const uint8_t*) record;
    bool blank = true;
    for (uint32_t i = 0; i < RECORD_SIZE && blank; i++) { blank = bytes[i] == 0xFFu; }
    if (blank) { return SLOT_BLANK; }
    return (record->crc == recordCrc(record)) ? SLOT_VALID : SLOT_INVALID;
}

// The header double word goes last, so a record cut short by a power loss is never
// valid. Its slot stays used, and the next record goes after it.
static bool programRecord(uint16_t location, Record* record)
{
    record->crc = recordCrc(record);
    uint64_t words[DWORDS_PER_RECORD];
    memcpy(words, record, RECORD_SIZE);

    const uint32_t base = (uint32_t) location * RECORD_SIZE;
    for (uint32_t i = 1; i < DWORDS_PER_RECORD; i++)
    {
        if (words[i] == UINT64_MAX) { continue; }
        if (!storageFlashProgram(base + 8u * i, words[i])) { return false; }
    }
    if (!storageFlashProgram(base, words[0])) { return false; }

    Record check;
    return readRecord(location, &check) == SLOT_VALID && memcmp(&check, record, RECORD_SIZE) == 0;
}

static bool pageBlank(uint8_t page)
{
    Record record;
    for (uint8_t i = 0; i < RECORDS_PER_PAGE; i++)
    {
        if (readRecord(locationOf(page, i), &record) != SLOT_BLANK) { return false; }
    }
    return true;
}

static bool pageHasLive(uint8_t page)
{
    for (uint8_t key = 0; key < KEY_COUNT; key++)
    {
        if (keyLocation[key] != LOCATION_NONE && pageOf(keyLocation[key]) == page) { return true; }
    }
    return false;
}

static bool erasePage(uint8_t page)
{
    pageValid[page] = false;
    return pageBlank(page) || storageFlashErase(page);
}

static bool openPage(uint8_t page)
{
    if (!erasePage(page)) { return false; }

    Record header;
    memset(&header, 0xFF, sizeof(header));
    header.type      = TYPE_PAGE;
    header.slot      = 0u;
    header.schemaVer = STORAGE_SCHEMA_VERSION;
    header.reserved  = 0u;
    header.seq       = nextSeq++;
    if (!programRecord(locationOf(page, 0u), &header)) { return false; }

    pageValid[page] = true;
    pageSeq[page]   = header.seq;
    head            = page;
    headNext        = 1u;
    return true;
}

// The head must have room.
static bool place(Record* record, uint8_t key)
{
    const uint16_t location = locationOf(head, headNext);
    headNext++;
    record->seq = nextSeq++;
    if (!programRecord(location, record)) { return false; }
    keyLocation[key] = location;
    return true;
}

// Frees the page after the head. Its live records fit: the head opened with room for
// a whole page and has taken only copies out of this page since, including across a
// power loss, which storageInit() resumes from.
static bool reclaim(void)
{
    const uint8_t spare = (uint8_t) ((head + 1u) % STORAGE_FLASH_PAGE_COUNT);
    for (uint8_t key = 0; key < KEY_COUNT; key++)
    {
        const uint16_t location = keyLocation[key];
        if (location == LOCATION_NONE || pageOf(location) != spare) { continue; }

        Record record;
        if (readRecord(location, &record) != SLOT_VALID)
        {
            keyLocation[key] = LOCATION_NONE;
            continue;
        }
        bool placed = false;
        while (!placed)
        {
            if (headNext >= RECORDS_PER_PAGE) { return false; }
            placed = place(&record, key);
        }
    }
    return erasePage(spare);
}

static bool append(Record* record, uint8_t key)
{
    // Every pass uses up a record or a page, so a failing flash cannot loop.
    for (uint8_t attempt = 0; attempt < 4u; attempt++)
    {
        if (headNext >= RECORDS_PER_PAGE)
        {
            const uint8_t next = (uint8_t) ((head + 1u) % STORAGE_FLASH_PAGE_COUNT);
            // Only if an earlier reclaim failed; erasing it would lose those records.
            if (pageHasLive(next)) { return false; }
            if (!openPage(next) || !reclaim()) { return false; }
            continue;
        }
        if (place(record, key)) { return true; }
    }
    return false;
}

static void noteSeq(uint16_t seq, uint16_t* latest, bool* seen)
{
    if (!*seen || newer(seq, *latest))
    {
        *latest = seq;
        *seen   = true;
    }
}

void storageInit(void)
{
    for (uint8_t key = 0; key < KEY_COUNT; key++) { keyLocation[key] = LOCATION_NONE; }
    head     = 0u;
    headNext = RECORDS_PER_PAGE;
    nextSeq  = 0u;

    uint16_t latest = 0u;
    bool     seen   = false;
    bool     any    = false;
    Record   record;

    for (uint8_t page = 0; page < STORAGE_FLASH_PAGE_COUNT; page++)
    {
        pageValid[page] = readRecord(locationOf(page, 0u), &record) == SLOT_VALID && record.type == TYPE_PAGE;
        if (!pageValid[page]) { continue; }
        pageSeq[page] = record.seq;
        if (!any || newer(record.seq, pageSeq[head])) { head = page; }
        any = true;
        noteSeq(record.seq, &latest, &seen);
    }

    if (!any)
    {
        (void) openPage(0u);
        (void) reclaim();
        return;
    }

    for (uint8_t page = 0; page < STORAGE_FLASH_PAGE_COUNT; page++)
    {
        if (!pageValid[page]) { continue; }
        for (uint8_t i = 1; i < RECORDS_PER_PAGE; i++)
        {
            const uint16_t location = locationOf(page, i);
            if (readRecord(location, &record) != SLOT_VALID) { continue; }
            noteSeq(record.seq, &latest, &seen);

            const uint8_t key = keyOf(record.type, record.slot);
            if (record.schemaVer != STORAGE_SCHEMA_VERSION || key == KEY_NONE) { continue; }
            if (keyLocation[key] == LOCATION_NONE || newer(record.seq, keySeq[key]))
            {
                keyLocation[key] = location;
                keySeq[key]      = record.seq;
            }
        }
    }
    nextSeq = (uint16_t) (latest + 1u);

    headNext = 1u;
    for (uint8_t i = RECORDS_PER_PAGE - 1u; i >= 1u; i--)
    {
        if (readRecord(locationOf(head, i), &record) != SLOT_BLANK)
        {
            headNext = (uint8_t) (i + 1u);
            break;
        }
    }

    // Finishes a page change that a power loss interrupted.
    (void) reclaim();
}

bool storageRead(StorageType type, uint8_t slot, void* payload, uint8_t size)
{
    const uint8_t key = keyOf((uint8_t) type, slot);
    if (key == KEY_NONE || size > STORAGE_PAYLOAD_SIZE || keyLocation[key] == LOCATION_NONE) { return false; }

    Record record;
    if (readRecord(keyLocation[key], &record) != SLOT_VALID) { return false; }
    memcpy(payload, record.payload, size);
    return true;
}

bool storageWrite(StorageType type, uint8_t slot, const void* payload, uint8_t size)
{
    const uint8_t key = keyOf((uint8_t) type, slot);
    if (key == KEY_NONE || size > STORAGE_PAYLOAD_SIZE) { return false; }

    Record record;
    memset(&record, 0xFF, sizeof(record));
    record.type      = (uint8_t) type;
    record.slot      = slot;
    record.schemaVer = STORAGE_SCHEMA_VERSION;
    record.reserved  = 0u;
    memcpy(record.payload, payload, size);

    if (keyLocation[key] != LOCATION_NONE)
    {
        Record current;
        if (readRecord(keyLocation[key], &current) == SLOT_VALID &&
            memcmp(current.payload, record.payload, STORAGE_PAYLOAD_SIZE) == 0)
        {
            return true;
        }
    }
    return append(&record, key);
}
