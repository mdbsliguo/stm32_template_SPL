/**
 * @file storage_guard.c
 * @brief ´æ´¢ÈÝ´íÊØÎÀÊµÏÖ
 */

#include "storage_guard.h"
#include "config.h"

#if CONFIG_MODULE_STORAGE_GUARD_ENABLED

#include "board.h"
#include "gpio.h"
#include "delay.h"
#include "log.h"

#ifdef CONFIG_MODULE_W25Q_ENABLED
#if CONFIG_MODULE_W25Q_ENABLED
#include "w25q_spi.h"
#endif
#endif

#ifdef CONFIG_MODULE_LITTLEFS_ENABLED
#if CONFIG_MODULE_LITTLEFS_ENABLED
#include "littlefs_wrapper.h"
#endif
#endif

static storage_guard_state_t s_state = STORAGE_GUARD_STATE_OFFLINE;
static uint32_t s_last_mount_ms = 0u;

static uint8_t s_test_skip_w25q = 0u;
static uint8_t s_test_force_mount_fail = 0u;
static LittleFS_Status_t s_test_force_mount_status = LITTLEFS_ERROR_IO;

static W25Q_Status_t storage_guard_init_w25q(void)
{
    GPIO_Status_t gpio_st;
    W25Q_Status_t w25q_st;

#if CONFIG_MODULE_W25Q_ENABLED
    gpio_st = GPIO_EnableClock(SPI2_NSS_PORT);
    if (gpio_st != GPIO_OK) {
        return W25Q_ERROR_INIT_FAILED;
    }

    gpio_st = GPIO_Config(SPI2_NSS_PORT, SPI2_NSS_PIN,
                          GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHz);
    if (gpio_st != GPIO_OK) {
        return W25Q_ERROR_INIT_FAILED;
    }

    (void)GPIO_WritePin(SPI2_NSS_PORT, SPI2_NSS_PIN, Bit_SET);

    w25q_st = W25Q_Init();
    if (w25q_st != W25Q_OK) {
        return w25q_st;
    }

    if (W25Q_GetInfo() == NULL) {
        return W25Q_ERROR_INIT_FAILED;
    }

    return W25Q_OK;
#else
    (void)gpio_st;
    (void)w25q_st;
    return W25Q_ERROR_INIT_FAILED;
#endif
}

static LittleFS_Status_t storage_guard_mount_policy(void)
{
    LittleFS_Status_t st;

#if CONFIG_MODULE_LITTLEFS_ENABLED
    st = LittleFS_Mount();
    if (st == LITTLEFS_OK) {
        return LITTLEFS_OK;
    }

    if (st == LITTLEFS_ERROR_CORRUPT) {
        LOG_WARN("SG", "mount corrupt, format once");
        st = LittleFS_Format();
        if (st != LITTLEFS_OK) {
            return st;
        }
        return LittleFS_Mount();
    }

    LOG_WARN("SG", "mount fail %d, no format", (int)st);
    return st;
#else
    return LITTLEFS_ERROR_NOT_INIT;
#endif
}

static error_code_t storage_guard_apply_mount_result(LittleFS_Status_t st, uint32_t elapsed_ms)
{
    s_last_mount_ms = elapsed_ms;

    if (elapsed_ms > STORAGE_GUARD_MOUNT_TIMEOUT_MS) {
        LOG_WARN("SG", "mount slow %lums > %u, degraded",
                 (unsigned long)elapsed_ms,
                 (unsigned int)STORAGE_GUARD_MOUNT_TIMEOUT_MS);
        s_state = STORAGE_GUARD_STATE_DEGRADED;
        return STORAGE_GUARD_ERROR_DEGRADED;
    }

    if (st == LITTLEFS_OK) {
        s_state = STORAGE_GUARD_STATE_READY;
        LOG_INFO("SG", "READY mount %lums", (unsigned long)elapsed_ms);
        return STORAGE_GUARD_OK;
    }

    s_state = STORAGE_GUARD_STATE_DEGRADED;
    return STORAGE_GUARD_ERROR_DEGRADED;
}

error_code_t StorageGuard_Init(void)
{
    uint32_t t0;
    LittleFS_Status_t st = LITTLEFS_OK;
    W25Q_Status_t wst;

    if (s_test_skip_w25q) {
        s_state = STORAGE_GUARD_STATE_OFFLINE;
        LOG_WARN("SG", "OFFLINE (test skip W25Q)");
        return STORAGE_GUARD_ERROR_OFFLINE;
    }

#if CONFIG_MODULE_W25Q_ENABLED
    wst = storage_guard_init_w25q();
    if (wst != W25Q_OK) {
        s_state = STORAGE_GUARD_STATE_OFFLINE;
        LOG_ERROR("SG", "W25Q init fail %d -> OFFLINE", (int)wst);
        return STORAGE_GUARD_ERROR_OFFLINE;
    }
#else
    s_state = STORAGE_GUARD_STATE_OFFLINE;
    return STORAGE_GUARD_ERROR_OFFLINE;
#endif

#if CONFIG_MODULE_LITTLEFS_ENABLED
    if (!LittleFS_IsInitialized()) {
        st = LittleFS_Init();
        if (st != LITTLEFS_OK) {
            s_state = STORAGE_GUARD_STATE_DEGRADED;
            LOG_ERROR("SG", "LFS init fail %d -> DEGRADED", (int)st);
            return STORAGE_GUARD_ERROR_DEGRADED;
        }
    }

    if (s_test_force_mount_fail) {
        s_state = STORAGE_GUARD_STATE_DEGRADED;
        s_last_mount_ms = 0u;
        LOG_WARN("SG", "DEGRADED (test force mount %d)",
                 (int)s_test_force_mount_status);
        return STORAGE_GUARD_ERROR_DEGRADED;
    }

    t0 = Delay_GetTick();
    st = storage_guard_mount_policy();
    return storage_guard_apply_mount_result(st,
                                            Delay_GetElapsed(Delay_GetTick(), t0));
#else
    s_state = STORAGE_GUARD_STATE_OFFLINE;
    return STORAGE_GUARD_ERROR_OFFLINE;
#endif
}

error_code_t StorageGuard_Shutdown(void)
{
    s_state = STORAGE_GUARD_STATE_OFFLINE;
    s_last_mount_ms = 0u;
    return STORAGE_GUARD_OK;
}

error_code_t StorageGuard_TryRecover(void)
{
    uint32_t t0;
    LittleFS_Status_t st;

    if (s_test_skip_w25q) {
        s_state = STORAGE_GUARD_STATE_OFFLINE;
        return STORAGE_GUARD_ERROR_OFFLINE;
    }

#if CONFIG_MODULE_W25Q_ENABLED
    if (storage_guard_init_w25q() != W25Q_OK) {
        s_state = STORAGE_GUARD_STATE_OFFLINE;
        return STORAGE_GUARD_ERROR_OFFLINE;
    }
#endif

#if CONFIG_MODULE_LITTLEFS_ENABLED
    if (s_test_force_mount_fail) {
        s_state = STORAGE_GUARD_STATE_DEGRADED;
        return STORAGE_GUARD_ERROR_DEGRADED;
    }

    if (!LittleFS_IsInitialized()) {
        st = LittleFS_Init();
        if (st != LITTLEFS_OK) {
            s_state = STORAGE_GUARD_STATE_DEGRADED;
            return STORAGE_GUARD_ERROR_DEGRADED;
        }
    }

    t0 = Delay_GetTick();
    st = storage_guard_mount_policy();
    return storage_guard_apply_mount_result(st,
                                            Delay_GetElapsed(Delay_GetTick(), t0));
#else
    s_state = STORAGE_GUARD_STATE_OFFLINE;
    return STORAGE_GUARD_ERROR_OFFLINE;
#endif
}

storage_guard_state_t StorageGuard_GetState(void)
{
    return s_state;
}

uint8_t StorageGuard_IsReady(void)
{
    return (s_state == STORAGE_GUARD_STATE_READY) ? 1u : 0u;
}

uint32_t StorageGuard_GetLastMountMs(void)
{
    return s_last_mount_ms;
}

error_code_t StorageGuard_NotifyLfsError(int32_t lfs_status)
{
    if (s_state == STORAGE_GUARD_STATE_READY) {
        s_state = STORAGE_GUARD_STATE_DEGRADED;
        LOG_WARN("SG", "degraded by LFS err %d", (int)lfs_status);
    }
    return STORAGE_GUARD_OK;
}

const char *StorageGuard_StateName(storage_guard_state_t state)
{
    switch (state) {
    case STORAGE_GUARD_STATE_READY:
        return "READY";
    case STORAGE_GUARD_STATE_DEGRADED:
        return "DEGRADED";
    case STORAGE_GUARD_STATE_OFFLINE:
    default:
        return "OFFLINE";
    }
}

error_code_t StorageGuard_SetTestSkipW25q(uint8_t skip)
{
    s_test_skip_w25q = skip ? 1u : 0u;
    return STORAGE_GUARD_OK;
}

error_code_t StorageGuard_SetTestForceMountFail(uint8_t enable,
                                                LittleFS_Status_t fake_status)
{
    s_test_force_mount_fail = enable ? 1u : 0u;
    if (enable) {
        s_test_force_mount_status = fake_status;
    }
    return STORAGE_GUARD_OK;
}

#endif /* CONFIG_MODULE_STORAGE_GUARD_ENABLED */
