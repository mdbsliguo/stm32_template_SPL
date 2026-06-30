/**
 * @file fuel_persist.h
 * @brief 定量加油持久化数据结构（LittleFS）
 * @details
 * - user.dat：target_next（新一轮/最后设置），常改，断电保持
 * - machine.dat：标定与工艺参数，少改
 * - checkpoint.dat：本笔 target_session + dispensed + pulse，每秒更新，断电可续加
 */

#ifndef FUEL_PERSIST_H
#define FUEL_PERSIST_H

#ifdef __cplusplus
extern "C" {
#endif

#include "fuel_types.h"
#include <stdint.h>

/**
 * @brief 用户设置（config/user.dat）
 */
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t crc16;
    uint32_t target_next_liter_x100;
} fuel_user_settings_t;

/**
 * @brief 机器标定与工艺（config/machine.dat）
 */
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t crc16;

    uint32_t pulses_per_liter;
    uint32_t hose_comp_liter_x100;   /**< 每笔补偿升数 liter_x100，默认55=0.55L */
    uint32_t max_target_liter_x100;
    uint32_t factory_target_next_liter_x100;

    uint16_t valve_open_delay_ms;
    uint16_t pump_stop_delay_ms;
    uint16_t vfd_run_freq_x100;
    uint8_t  vfd_modbus_addr;
    uint8_t  vfd_baud_index;
    uint16_t led_refresh_ms;

    uint16_t ogm_stall_timeout_ms;
    uint16_t vfd_comm_timeout_ms;
    uint16_t fault_recovery_debounce_ms;

    uint16_t checkpoint_interval_ms;
    uint8_t  pause_on_powerup_active;
    uint8_t  valve_close_on_pause;
    uint16_t heartbeat_report_ms;

    uint8_t  reserved[8];
} fuel_machine_config_t;

/**
 * @brief 本笔会话检查点（session/checkpoint.dat）
 * @note 加注中每秒写入；含本笔目标与已加量，来电可继续加油
 */
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t crc16;

    uint32_t txn_id;
    uint8_t  saved_state;
    uint8_t  saved_sub;
    uint8_t  pause_reason;
    uint8_t  session_active;

    uint32_t target_session_liter_x100;
    uint32_t dispensed_liter_x100;
    uint32_t pulse_count;
    uint32_t target_pulse;

    uint32_t rtc_counter;
} fuel_checkpoint_t;

#ifdef __cplusplus
}
#endif

#endif /* FUEL_PERSIST_H */
