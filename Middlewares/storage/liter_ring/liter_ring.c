/**
 * @file liter_ring.c
 * @brief 按升明细环形日志实现
 */

#include "liter_ring.h"
#include "config.h"

#if CONFIG_MODULE_LITER_RING_ENABLED

#ifdef CONFIG_MODULE_LITTLEFS_ENABLED
#if CONFIG_MODULE_LITTLEFS_ENABLED

#include "littlefs_wrapper.h"
#include <string.h>

#ifdef CONFIG_MODULE_STORAGE_GUARD_ENABLED
#if CONFIG_MODULE_STORAGE_GUARD_ENABLED
#include "storage_guard.h"
#endif
#endif

#define LITER_RING_HEADER_SIZE      32u

#define LITER_RING_ERROR_NOT_INIT       (ERROR_BASE_LITER_RING - 1)
#define LITER_RING_ERROR_INVALID_PARAM  (ERROR_BASE_LITER_RING - 2)
#define LITER_RING_ERROR_IO             (ERROR_BASE_LITER_RING - 3)
#define LITER_RING_ERROR_CORRUPT        (ERROR_BASE_LITER_RING - 4)
#define LITER_RING_ERROR_NOT_MOUNTED    (ERROR_BASE_LITER_RING - 5)
#define LITER_RING_ERROR_STORAGE_UNAVAILABLE (ERROR_BASE_LITER_RING - 6)

static error_code_t liter_ring_check_storage(void)
{
#if CONFIG_MODULE_STORAGE_GUARD_ENABLED
#if CONFIG_MODULE_STORAGE_GUARD_ENABLED
    if (!StorageGuard_IsReady()) {
        return LITER_RING_ERROR_STORAGE_UNAVAILABLE;
    }
#endif
#endif
    return ERROR_OK;
}

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t record_size;
    uint32_t capacity;
    uint32_t head;
    uint32_t total_writes;
    uint32_t reserved[2];
} liter_ring_header_t;

static struct {
    uint8_t inited;
    char path[LITER_RING_PATH_MAX];
    uint32_t capacity;
    uint32_t sync_every;
    uint32_t since_sync;
    liter_ring_header_t hdr;
    uint8_t session_open;
    lfs_file_t session_file;
} g_ring;

static error_code_t liter_ring_read_header(lfs_file_t *file, liter_ring_header_t *hdr)
{
    uint32_t br = 0;
    LittleFS_Status_t st;

    if (file == NULL || hdr == NULL) {
        return LITER_RING_ERROR_INVALID_PARAM;
    }

    st = LittleFS_FileSeek(file, 0, LFS_SEEK_SET);
    if (st != LITTLEFS_OK) {
        return LITER_RING_ERROR_IO;
    }

    st = LittleFS_FileRead(file, hdr, LITER_RING_HEADER_SIZE, &br);
    if (st != LITTLEFS_OK || br != LITER_RING_HEADER_SIZE) {
        return LITER_RING_ERROR_IO;
    }

    if (hdr->magic != LITER_RING_MAGIC ||
        hdr->version != LITER_RING_VERSION ||
        hdr->record_size != LITER_RING_DEFAULT_REC_SIZE ||
        hdr->capacity == 0u) {
        return LITER_RING_ERROR_CORRUPT;
    }

    return ERROR_OK;
}

static error_code_t liter_ring_write_header(lfs_file_t *file, const liter_ring_header_t *hdr)
{
    uint32_t bw = 0;
    LittleFS_Status_t st;

    if (file == NULL || hdr == NULL) {
        return LITER_RING_ERROR_INVALID_PARAM;
    }

    st = LittleFS_FileSeek(file, 0, LFS_SEEK_SET);
    if (st != LITTLEFS_OK) {
        return LITER_RING_ERROR_IO;
    }

    st = LittleFS_FileWrite(file, hdr, LITER_RING_HEADER_SIZE, &bw);
    if (st != LITTLEFS_OK || bw != LITER_RING_HEADER_SIZE) {
        return LITER_RING_ERROR_IO;
    }

    return ERROR_OK;
}

static error_code_t liter_ring_create_file(const char *path, uint32_t capacity)
{
    lfs_file_t file;
    liter_ring_header_t hdr;
    LittleFS_Status_t st;
    error_code_t err;

    memset(&file, 0, sizeof(file));
    memset(&hdr, 0, sizeof(hdr));

    hdr.magic = LITER_RING_MAGIC;
    hdr.version = LITER_RING_VERSION;
    hdr.record_size = LITER_RING_DEFAULT_REC_SIZE;
    hdr.capacity = capacity;
    hdr.head = 0u;
    hdr.total_writes = 0u;

    st = LittleFS_FileOpen(&file, path, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (st != LITTLEFS_OK) {
        return LITER_RING_ERROR_IO;
    }

    err = liter_ring_write_header(&file, &hdr);
    if (err != ERROR_OK) {
        (void)LittleFS_FileClose(&file);
        return err;
    }

    st = LittleFS_FileSync(&file);
    if (st != LITTLEFS_OK) {
        (void)LittleFS_FileClose(&file);
        return LITER_RING_ERROR_IO;
    }

    st = LittleFS_FileClose(&file);
    if (st != LITTLEFS_OK) {
        return LITER_RING_ERROR_IO;
    }

    return ERROR_OK;
}

static error_code_t liter_ring_maybe_sync(lfs_file_t *file)
{
    LittleFS_Status_t st;

    if (g_ring.sync_every == 0u) {
        st = LittleFS_FileSync(file);
        return (st == LITTLEFS_OK) ? ERROR_OK : LITER_RING_ERROR_IO;
    }

    g_ring.since_sync++;
    if (g_ring.since_sync >= g_ring.sync_every) {
        g_ring.since_sync = 0u;
        st = LittleFS_FileSync(file);
        return (st == LITTLEFS_OK) ? ERROR_OK : LITER_RING_ERROR_IO;
    }

    return ERROR_OK;
}

error_code_t LiterRing_Init(const liter_ring_config_t *config)
{
    lfs_file_t file;
    liter_ring_header_t hdr;
    LittleFS_Status_t st;
    error_code_t err;

    if (config == NULL || config->file_path == NULL || config->capacity == 0u) {
        return LITER_RING_ERROR_INVALID_PARAM;
    }

    if (strlen(config->file_path) >= LITER_RING_PATH_MAX) {
        return LITER_RING_ERROR_INVALID_PARAM;
    }

    err = liter_ring_check_storage();
    if (err != ERROR_OK) {
        return err;
    }

    if (!LittleFS_IsInitialized()) {
        return LITER_RING_ERROR_NOT_MOUNTED;
    }

    memset(&g_ring, 0, sizeof(g_ring));
    strncpy(g_ring.path, config->file_path, LITER_RING_PATH_MAX - 1u);
    g_ring.capacity = config->capacity;
    g_ring.sync_every = config->sync_every;

    memset(&file, 0, sizeof(file));
    st = LittleFS_FileOpen(&file, g_ring.path, LFS_O_RDONLY);
    if (st != LITTLEFS_OK) {
        err = liter_ring_create_file(g_ring.path, g_ring.capacity);
        if (err != ERROR_OK) {
            return err;
        }
        st = LittleFS_FileOpen(&file, g_ring.path, LFS_O_RDONLY);
        if (st != LITTLEFS_OK) {
            return LITER_RING_ERROR_IO;
        }
    }

    err = liter_ring_read_header(&file, &hdr);
    if (err == LITER_RING_ERROR_CORRUPT) {
        (void)LittleFS_FileClose(&file);
        err = liter_ring_create_file(g_ring.path, g_ring.capacity);
        if (err != ERROR_OK) {
            return err;
        }
        memset(&file, 0, sizeof(file));
        st = LittleFS_FileOpen(&file, g_ring.path, LFS_O_RDONLY);
        if (st != LITTLEFS_OK) {
            return LITER_RING_ERROR_IO;
        }
        err = liter_ring_read_header(&file, &hdr);
    }

    (void)LittleFS_FileClose(&file);

    if (err != ERROR_OK) {
        return err;
    }

    if (hdr.capacity != g_ring.capacity) {
        return LITER_RING_ERROR_CORRUPT;
    }

    g_ring.hdr = hdr;
    g_ring.inited = 1u;
    return ERROR_OK;
}

error_code_t LiterRing_Deinit(void)
{
    if (g_ring.session_open) {
        (void)LittleFS_FileClose(&g_ring.session_file);
        g_ring.session_open = 0u;
    }

    memset(&g_ring, 0, sizeof(g_ring));
    return ERROR_OK;
}

uint8_t LiterRing_IsInitialized(void)
{
    return g_ring.inited;
}

error_code_t LiterRing_ForceSync(void)
{
    lfs_file_t file;
    LittleFS_Status_t st;
    error_code_t err;

    if (!g_ring.inited) {
        return LITER_RING_ERROR_NOT_INIT;
    }

    err = liter_ring_check_storage();
    if (err != ERROR_OK) {
        return err;
    }

    memset(&file, 0, sizeof(file));
    st = LittleFS_FileOpen(&file, g_ring.path, LFS_O_WRONLY);
    if (st != LITTLEFS_OK) {
        return LITER_RING_ERROR_IO;
    }

    st = LittleFS_FileSync(&file);
    (void)LittleFS_FileClose(&file);

    g_ring.since_sync = 0u;
    return (st == LITTLEFS_OK) ? ERROR_OK : LITER_RING_ERROR_IO;
}

static error_code_t liter_ring_append_core(lfs_file_t *file,
                                           liter_ring_header_t *hdr,
                                           const liter_ring_record_t *record)
{
    liter_ring_record_t rec;
    uint32_t offset;
    uint32_t bw = 0;
    LittleFS_Status_t st;
    error_code_t err;

    if (file == NULL || hdr == NULL || record == NULL) {
        return LITER_RING_ERROR_INVALID_PARAM;
    }

    rec = *record;
    rec.seq = hdr->total_writes + 1u;

    offset = LITER_RING_HEADER_SIZE + hdr->head * LITER_RING_DEFAULT_REC_SIZE;
    st = LittleFS_FileSeek(file, (int32_t)offset, LFS_SEEK_SET);
    if (st != LITTLEFS_OK) {
        return LITER_RING_ERROR_IO;
    }

    st = LittleFS_FileWrite(file, &rec, LITER_RING_DEFAULT_REC_SIZE, &bw);
    if (st != LITTLEFS_OK || bw != LITER_RING_DEFAULT_REC_SIZE) {
        return LITER_RING_ERROR_IO;
    }

    hdr->total_writes++;
    hdr->head = (hdr->head + 1u) % hdr->capacity;

    err = liter_ring_write_header(file, hdr);
    if (err != ERROR_OK) {
        return err;
    }

    return ERROR_OK;
}

error_code_t LiterRing_Append(const liter_ring_record_t *record)
{
    lfs_file_t file;
    liter_ring_header_t hdr;
    LittleFS_Status_t st;
    error_code_t err;

    if (!g_ring.inited) {
        return LITER_RING_ERROR_NOT_INIT;
    }

    if (record == NULL) {
        return LITER_RING_ERROR_INVALID_PARAM;
    }

    err = liter_ring_check_storage();
    if (err != ERROR_OK) {
        return err;
    }

    memset(&file, 0, sizeof(file));
    st = LittleFS_FileOpen(&file, g_ring.path, LFS_O_RDWR);
    if (st != LITTLEFS_OK) {
        return LITER_RING_ERROR_IO;
    }

    err = liter_ring_read_header(&file, &hdr);
    if (err != ERROR_OK) {
        (void)LittleFS_FileClose(&file);
        return err;
    }

    err = liter_ring_append_core(&file, &hdr, record);
    if (err != ERROR_OK) {
        (void)LittleFS_FileClose(&file);
        return err;
    }

    err = liter_ring_maybe_sync(&file);
    if (err != ERROR_OK) {
        (void)LittleFS_FileClose(&file);
        return err;
    }

    st = LittleFS_FileClose(&file);
    if (st != LITTLEFS_OK) {
        return LITER_RING_ERROR_IO;
    }

    g_ring.hdr = hdr;
    return ERROR_OK;
}

error_code_t LiterRing_AppendSessionBegin(void)
{
    LittleFS_Status_t st;
    error_code_t err;

    if (!g_ring.inited) {
        return LITER_RING_ERROR_NOT_INIT;
    }

    if (g_ring.session_open) {
        return LITER_RING_ERROR_IO;
    }

    err = liter_ring_check_storage();
    if (err != ERROR_OK) {
        return err;
    }

    memset(&g_ring.session_file, 0, sizeof(g_ring.session_file));
    st = LittleFS_FileOpen(&g_ring.session_file, g_ring.path, LFS_O_RDWR);
    if (st != LITTLEFS_OK) {
        return LITER_RING_ERROR_IO;
    }

    err = liter_ring_read_header(&g_ring.session_file, &g_ring.hdr);
    if (err != ERROR_OK) {
        (void)LittleFS_FileClose(&g_ring.session_file);
        return err;
    }

    g_ring.session_open = 1u;
    return ERROR_OK;
}

error_code_t LiterRing_AppendSession(const liter_ring_record_t *record)
{
    error_code_t err;

    if (!g_ring.inited) {
        return LITER_RING_ERROR_NOT_INIT;
    }

    if (!g_ring.session_open) {
        return LITER_RING_ERROR_NOT_INIT;
    }

    if (record == NULL) {
        return LITER_RING_ERROR_INVALID_PARAM;
    }

    err = liter_ring_check_storage();
    if (err != ERROR_OK) {
        return err;
    }

    err = liter_ring_append_core(&g_ring.session_file, &g_ring.hdr, record);
    if (err != ERROR_OK) {
        return err;
    }

    return liter_ring_maybe_sync(&g_ring.session_file);
}

error_code_t LiterRing_AppendSessionEnd(void)
{
    LittleFS_Status_t st;
    error_code_t err;

    if (!g_ring.inited) {
        return LITER_RING_ERROR_NOT_INIT;
    }

    if (!g_ring.session_open) {
        return ERROR_OK;
    }

    err = liter_ring_check_storage();
    if (err != ERROR_OK) {
        return err;
    }

    st = LittleFS_FileSync(&g_ring.session_file);
    if (st != LITTLEFS_OK) {
        (void)LittleFS_FileClose(&g_ring.session_file);
        g_ring.session_open = 0u;
        return LITER_RING_ERROR_IO;
    }

    st = LittleFS_FileClose(&g_ring.session_file);
    g_ring.session_open = 0u;
    g_ring.since_sync = 0u;
    if (st != LITTLEFS_OK) {
        return LITER_RING_ERROR_IO;
    }

    return ERROR_OK;
}

error_code_t LiterRing_ReadSlot(uint32_t slot, liter_ring_record_t *record)
{
    lfs_file_t file;
    liter_ring_header_t hdr;
    uint32_t offset;
    uint32_t br = 0;
    LittleFS_Status_t st;
    error_code_t err;

    if (!g_ring.inited) {
        return LITER_RING_ERROR_NOT_INIT;
    }

    if (record == NULL) {
        return LITER_RING_ERROR_INVALID_PARAM;
    }

    err = liter_ring_check_storage();
    if (err != ERROR_OK) {
        return err;
    }

    memset(&file, 0, sizeof(file));
    st = LittleFS_FileOpen(&file, g_ring.path, LFS_O_RDONLY);
    if (st != LITTLEFS_OK) {
        return LITER_RING_ERROR_IO;
    }

    err = liter_ring_read_header(&file, &hdr);
    if (err != ERROR_OK) {
        (void)LittleFS_FileClose(&file);
        return err;
    }

    if (slot >= hdr.capacity) {
        (void)LittleFS_FileClose(&file);
        return LITER_RING_ERROR_INVALID_PARAM;
    }

    if (hdr.total_writes == 0u) {
        (void)LittleFS_FileClose(&file);
        return LITER_RING_ERROR_IO;
    }

    offset = LITER_RING_HEADER_SIZE + slot * LITER_RING_DEFAULT_REC_SIZE;
    st = LittleFS_FileSeek(&file, (int32_t)offset, LFS_SEEK_SET);
    if (st != LITTLEFS_OK) {
        (void)LittleFS_FileClose(&file);
        return LITER_RING_ERROR_IO;
    }

    st = LittleFS_FileRead(&file, record, LITER_RING_DEFAULT_REC_SIZE, &br);
    (void)LittleFS_FileClose(&file);

    if (st != LITTLEFS_OK || br != LITER_RING_DEFAULT_REC_SIZE) {
        return LITER_RING_ERROR_IO;
    }

    return ERROR_OK;
}

error_code_t LiterRing_GetStats(liter_ring_stats_t *stats)
{
    if (!g_ring.inited) {
        return LITER_RING_ERROR_NOT_INIT;
    }

    if (stats == NULL) {
        return LITER_RING_ERROR_INVALID_PARAM;
    }

    stats->capacity = g_ring.hdr.capacity;
    stats->head = g_ring.hdr.head;
    stats->total_writes = g_ring.hdr.total_writes;
    return ERROR_OK;
}

error_code_t LiterRing_MarkStage4Done(void)
{
    lfs_file_t file;
    LittleFS_Status_t st;
    error_code_t err;

    if (!g_ring.inited) {
        return LITER_RING_ERROR_NOT_INIT;
    }

    err = liter_ring_check_storage();
    if (err != ERROR_OK) {
        return err;
    }

    memset(&file, 0, sizeof(file));
    st = LittleFS_FileOpen(&file, g_ring.path, LFS_O_RDWR);
    if (st != LITTLEFS_OK) {
        return LITER_RING_ERROR_IO;
    }

    err = liter_ring_read_header(&file, &g_ring.hdr);
    if (err != ERROR_OK) {
        (void)LittleFS_FileClose(&file);
        return err;
    }

    g_ring.hdr.reserved[0] = LITER_RING_STAGE4_DONE_MAGIC;
    err = liter_ring_write_header(&file, &g_ring.hdr);
    if (err != ERROR_OK) {
        (void)LittleFS_FileClose(&file);
        return err;
    }

    st = LittleFS_FileClose(&file);
    if (st != LITTLEFS_OK) {
        return LITER_RING_ERROR_IO;
    }

    return ERROR_OK;
}

uint8_t LiterRing_IsStage4Done(void)
{
    if (!g_ring.inited) {
        return 0u;
    }

    return (g_ring.hdr.reserved[0] == LITER_RING_STAGE4_DONE_MAGIC) ? 1u : 0u;
}

error_code_t LiterRing_VerifyWrap(uint32_t expect_total,
                                  uint32_t *oldest_seq,
                                  uint32_t *newest_seq)
{
    liter_ring_stats_t stats;
    liter_ring_record_t rec_old;
    liter_ring_record_t rec_new;
    uint32_t oldest_slot;
    uint32_t newest_slot;
    uint32_t expect_oldest;
    error_code_t err;

    err = LiterRing_GetStats(&stats);
    if (err != ERROR_OK) {
        return err;
    }

    if (stats.total_writes != expect_total) {
        return LITER_RING_ERROR_CORRUPT;
    }

    if (expect_total == 0u) {
        return LITER_RING_ERROR_INVALID_PARAM;
    }

    newest_slot = (stats.head + stats.capacity - 1u) % stats.capacity;
    err = LiterRing_ReadSlot(newest_slot, &rec_new);
    if (err != ERROR_OK) {
        return err;
    }

    if (rec_new.seq != expect_total) {
        return LITER_RING_ERROR_CORRUPT;
    }

    if (oldest_seq != NULL) {
        *oldest_seq = rec_new.seq;
    }
    if (newest_seq != NULL) {
        *newest_seq = rec_new.seq;
    }

    if (expect_total <= stats.capacity) {
        return ERROR_OK;
    }

    oldest_slot = stats.head;
    expect_oldest = expect_total - stats.capacity + 1u;

    err = LiterRing_ReadSlot(oldest_slot, &rec_old);
    if (err != ERROR_OK) {
        return err;
    }

    if (rec_old.seq != expect_oldest) {
        return LITER_RING_ERROR_CORRUPT;
    }

    if (oldest_seq != NULL) {
        *oldest_seq = rec_old.seq;
    }

    return ERROR_OK;
}

#endif /* CONFIG_MODULE_LITTLEFS_ENABLED */
#endif /* CONFIG_MODULE_LITTLEFS_ENABLED */

#endif /* CONFIG_MODULE_LITER_RING_ENABLED */
