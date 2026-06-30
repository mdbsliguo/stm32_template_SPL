/**
 * @file fuel_store.c
 * @brief LittleFS file IO for fuel persistence
 */

#include "fuel_store.h"
#include "storage_guard.h"
#include "littlefs_wrapper.h"
#include "log.h"
#include "config.h"
#include "board.h"
#include <stddef.h>
#include <string.h>

#ifndef FUEL_HOSE_COMP_LITER_X100_DEFAULT
#define FUEL_HOSE_COMP_LITER_X100_DEFAULT  55u
#endif

#ifndef FUEL_VFD_RUN_FREQ_X100_DEFAULT
#define FUEL_VFD_RUN_FREQ_X100_DEFAULT  1000u
#endif

uint16_t FuelStore_Crc16(const void *data, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint16_t crc = 0xFFFFu;
    uint32_t i;
    uint8_t bit;

    if (p == NULL || len == 0u) {
        return 0u;
    }

    for (i = 0u; i < len; i++) {
        crc ^= (uint16_t)p[i] << 8;
        for (bit = 0u; bit < 8u; bit++) {
            if ((crc & 0x8000u) != 0u) {
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

static uint16_t fuel_store_calc_crc(void *obj, uint32_t size, size_t crc_off)
{
    uint16_t saved;
    uint16_t crc;
    uint8_t *p;

    if (obj == NULL) {
        return 0u;
    }

    p = (uint8_t *)obj;
    saved = *(uint16_t *)(p + crc_off);
    *(uint16_t *)(p + crc_off) = 0u;
    crc = FuelStore_Crc16(obj, size);
    *(uint16_t *)(p + crc_off) = saved;
    return crc;
}

static uint8_t fuel_store_can_write(void)
{
#if defined(CONFIG_MODULE_STORAGE_GUARD_ENABLED) && CONFIG_MODULE_STORAGE_GUARD_ENABLED
    return StorageGuard_IsReady();
#else
    return 1u;
#endif
}

static error_code_t fuel_store_ensure_dirs(void)
{
    LittleFS_Status_t st;

    st = LittleFS_DirCreate("config");
    if (st != LITTLEFS_OK && st != LITTLEFS_ERROR_EXIST) {
        return FUEL_STORE_ERROR_IO;
    }

    st = LittleFS_DirCreate("session");
    if (st != LITTLEFS_OK && st != LITTLEFS_ERROR_EXIST) {
        return FUEL_STORE_ERROR_IO;
    }
    return FUEL_STORE_OK;
}

static error_code_t fuel_store_read_blob(const char *path, void *buf, uint32_t size)
{
    lfs_file_t file;
    LittleFS_Status_t st;
    uint32_t nread = 0u;

    if (path == NULL || buf == NULL) {
        return FUEL_STORE_ERROR_INVALID_PARAM;
    }

    if (!fuel_store_can_write()) {
        return FUEL_STORE_ERROR_STORAGE;
    }

    st = LittleFS_FileOpen(&file, path, LFS_O_RDONLY);
    if (st != LITTLEFS_OK) {
        return FUEL_STORE_ERROR_IO;
    }

    st = LittleFS_FileRead(&file, buf, size, &nread);
    (void)LittleFS_FileClose(&file);

    if (st != LITTLEFS_OK || nread != size) {
        return FUEL_STORE_ERROR_IO;
    }
    return FUEL_STORE_OK;
}

static error_code_t fuel_store_write_blob(const char *path, const void *buf, uint32_t size)
{
    lfs_file_t file;
    LittleFS_Status_t st;
    uint32_t nwritten = 0u;

    if (path == NULL || buf == NULL) {
        return FUEL_STORE_ERROR_INVALID_PARAM;
    }

    if (!fuel_store_can_write()) {
        return FUEL_STORE_ERROR_STORAGE;
    }

    (void)fuel_store_ensure_dirs();

    st = LittleFS_FileOpen(&file, path, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (st != LITTLEFS_OK) {
        return FUEL_STORE_ERROR_IO;
    }

    st = LittleFS_FileWrite(&file, buf, size, &nwritten);
    (void)LittleFS_FileClose(&file);

    if (st != LITTLEFS_OK || nwritten != size) {
        return FUEL_STORE_ERROR_IO;
    }
    return FUEL_STORE_OK;
}

void FuelStore_FillMachineDefaults(fuel_machine_config_t *mc)
{
    if (mc == NULL) {
        return;
    }

    memset(mc, 0, sizeof(*mc));
    mc->magic = FUEL_MACHINE_MAGIC;
    mc->version = FUEL_MACHINE_VERSION;
    mc->pulses_per_liter = FUEL_PULSES_PER_LITER_DEFAULT;
    mc->hose_comp_liter_x100 = FUEL_HOSE_COMP_LITER_X100_DEFAULT;
    mc->max_target_liter_x100 = 99999u;
    mc->factory_target_next_liter_x100 = FUEL_DEFAULT_TARGET_NEXT_X100;
    mc->valve_open_delay_ms = 50u;
    mc->pump_stop_delay_ms = 0u;
    mc->vfd_run_freq_x100 = FUEL_VFD_RUN_FREQ_X100_DEFAULT;
    mc->vfd_modbus_addr = 1u;
    mc->vfd_baud_index = 0u;
    mc->led_refresh_ms = 200u;
    mc->ogm_stall_timeout_ms = 8000u;
    mc->vfd_comm_timeout_ms = 1000u;
    mc->fault_recovery_debounce_ms = 500u;
    mc->checkpoint_interval_ms = 1000u;
    mc->pause_on_powerup_active = 1u;
    mc->valve_close_on_pause = 1u;
    mc->heartbeat_report_ms = 2000u;
    mc->crc16 = fuel_store_calc_crc(mc, sizeof(*mc), offsetof(fuel_machine_config_t, crc16));
}

error_code_t FuelStore_Init(void)
{
    return FUEL_STORE_OK;
}

error_code_t FuelStore_LoadUser(fuel_user_settings_t *out)
{
    fuel_user_settings_t tmp;
    error_code_t ret;
    uint16_t crc;

    if (out == NULL) {
        return FUEL_STORE_ERROR_INVALID_PARAM;
    }

    memset(&tmp, 0, sizeof(tmp));
    ret = fuel_store_read_blob(FUEL_STORE_PATH_USER, &tmp, sizeof(tmp));
    if (ret != FUEL_STORE_OK) {
        memset(out, 0, sizeof(*out));
        out->magic = FUEL_USER_MAGIC;
        out->version = FUEL_USER_VERSION;
        out->target_next_liter_x100 = FUEL_DEFAULT_TARGET_NEXT_X100;
        out->crc16 = fuel_store_calc_crc(out, sizeof(*out), offsetof(fuel_user_settings_t, crc16));
        return FUEL_STORE_OK;
    }

    crc = fuel_store_calc_crc(&tmp, sizeof(tmp), offsetof(fuel_user_settings_t, crc16));
    if (tmp.magic != FUEL_USER_MAGIC || tmp.version != FUEL_USER_VERSION ||
        tmp.crc16 != crc) {
        memset(out, 0, sizeof(*out));
        out->magic = FUEL_USER_MAGIC;
        out->version = FUEL_USER_VERSION;
        out->target_next_liter_x100 = FUEL_DEFAULT_TARGET_NEXT_X100;
        out->crc16 = fuel_store_calc_crc(out, sizeof(*out), offsetof(fuel_user_settings_t, crc16));
        LOG_WARN("FUEL", "user.dat invalid, use default 20.00L");
        return FUEL_STORE_OK;
    }

    *out = tmp;
    return FUEL_STORE_OK;
}

error_code_t FuelStore_SaveUser(const fuel_user_settings_t *in)
{
    fuel_user_settings_t tmp;

    if (in == NULL) {
        return FUEL_STORE_ERROR_INVALID_PARAM;
    }

    tmp = *in;
    tmp.magic = FUEL_USER_MAGIC;
    tmp.version = FUEL_USER_VERSION;
    tmp.crc16 = fuel_store_calc_crc(&tmp, sizeof(tmp), offsetof(fuel_user_settings_t, crc16));
    return fuel_store_write_blob(FUEL_STORE_PATH_USER, &tmp, sizeof(tmp));
}

error_code_t FuelStore_LoadMachine(fuel_machine_config_t *out)
{
    fuel_machine_config_t tmp;
    error_code_t ret;
    uint16_t crc;

    if (out == NULL) {
        return FUEL_STORE_ERROR_INVALID_PARAM;
    }

    memset(&tmp, 0, sizeof(tmp));
    ret = fuel_store_read_blob(FUEL_STORE_PATH_MACHINE, &tmp, sizeof(tmp));
    if (ret != FUEL_STORE_OK) {
        FuelStore_FillMachineDefaults(out);
        return FUEL_STORE_OK;
    }

    crc = fuel_store_calc_crc(&tmp, sizeof(tmp), offsetof(fuel_machine_config_t, crc16));
    if (tmp.magic != FUEL_MACHINE_MAGIC ||
        (tmp.version != FUEL_MACHINE_VERSION) ||
        tmp.crc16 != crc) {
        FuelStore_FillMachineDefaults(out);
        LOG_WARN("FUEL", "machine.dat invalid, use defaults");
        return FUEL_STORE_OK;
    }

    *out = tmp;
    return FUEL_STORE_OK;
}

error_code_t FuelStore_SaveMachine(const fuel_machine_config_t *in)
{
    fuel_machine_config_t tmp;

    if (in == NULL) {
        return FUEL_STORE_ERROR_INVALID_PARAM;
    }

    tmp = *in;
    tmp.magic = FUEL_MACHINE_MAGIC;
    tmp.version = FUEL_MACHINE_VERSION;
    tmp.crc16 = fuel_store_calc_crc(&tmp, sizeof(tmp), offsetof(fuel_machine_config_t, crc16));
    return fuel_store_write_blob(FUEL_STORE_PATH_MACHINE, &tmp, sizeof(tmp));
}

error_code_t FuelStore_LoadCheckpoint(fuel_checkpoint_t *out)
{
    fuel_checkpoint_t tmp;
    error_code_t ret;
    uint16_t crc;

    if (out == NULL) {
        return FUEL_STORE_ERROR_INVALID_PARAM;
    }

    memset(out, 0, sizeof(*out));
    ret = fuel_store_read_blob(FUEL_STORE_PATH_CHECKPOINT, &tmp, sizeof(tmp));
    if (ret != FUEL_STORE_OK) {
        return FUEL_STORE_OK;
    }

    crc = fuel_store_calc_crc(&tmp, sizeof(tmp), offsetof(fuel_checkpoint_t, crc16));
    if (tmp.magic != FUEL_CHECKPOINT_MAGIC || tmp.version != FUEL_CHECKPOINT_VERSION ||
        tmp.crc16 != crc) {
        memset(out, 0, sizeof(*out));
        return FUEL_STORE_OK;
    }

    *out = tmp;
    return FUEL_STORE_OK;
}

error_code_t FuelStore_SaveCheckpoint(const fuel_checkpoint_t *in)
{
    fuel_checkpoint_t tmp;

    if (in == NULL) {
        return FUEL_STORE_ERROR_INVALID_PARAM;
    }

    tmp = *in;
    tmp.magic = FUEL_CHECKPOINT_MAGIC;
    tmp.version = FUEL_CHECKPOINT_VERSION;
    tmp.crc16 = fuel_store_calc_crc(&tmp, sizeof(tmp), offsetof(fuel_checkpoint_t, crc16));
    return fuel_store_write_blob(FUEL_STORE_PATH_CHECKPOINT, &tmp, sizeof(tmp));
}

error_code_t FuelStore_ClearCheckpoint(void)
{
    LittleFS_Status_t st;

    if (!fuel_store_can_write()) {
        return FUEL_STORE_ERROR_STORAGE;
    }

    st = LittleFS_FileDelete(FUEL_STORE_PATH_CHECKPOINT);
    if (st != LITTLEFS_OK && st != LITTLEFS_ERROR_NOENT) {
        return FUEL_STORE_ERROR_IO;
    }
    return FUEL_STORE_OK;
}
