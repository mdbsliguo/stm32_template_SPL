/**
 * @file fuel_types.h
 * @brief ????????????????????????
 * @details liter_x100?1 ?? = 0.01 L????? xx.xx
 */

#ifndef FUEL_TYPES_H
#define FUEL_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define FUEL_USER_MAGIC             0x46555352u  /**< 'FUSR' */
#define FUEL_MACHINE_MAGIC          0x464D4347u  /**< 'FMCG' */
#define FUEL_CHECKPOINT_MAGIC       0x4643484Bu  /**< 'FCHK' */

#define FUEL_USER_VERSION           1u
#define FUEL_MACHINE_VERSION        2u
#define FUEL_CHECKPOINT_VERSION     1u

/**
 * @brief ??????????? / ??? / ???
 */
typedef enum {
    FUEL_STATE_READY = 0,
    FUEL_STATE_FUELING,
    FUEL_STATE_FAULT
} fuel_state_t;

/**
 * @brief ??????
 */
typedef enum {
    FUEL_RUN_ACTIVE = 0,
    FUEL_RUN_PAUSED
} fuel_run_substate_t;

/**
 * @brief ????
 */
typedef enum {
    FUEL_PAUSE_NONE = 0,
    FUEL_PAUSE_USER,
    FUEL_PAUSE_POWER,
    FUEL_PAUSE_FAULT
} fuel_pause_reason_t;

/**
 * @brief ???????=??????=??????>=LONG_MS?
 */
typedef enum {
    FUEL_KEY_EVT_NONE = 0,
    FUEL_KEY_EVT_GREEN_SHORT,
    FUEL_KEY_EVT_GREEN_LONG,
    FUEL_KEY_EVT_RED_SHORT,
    FUEL_KEY_EVT_RED_LONG
} fuel_key_event_t;

/**
 * @brief ????????
 */
typedef enum {
    FUEL_FAULT_NONE          = 0u,
    FUEL_FAULT_VFD_COMM      = (1u << 0),
    FUEL_FAULT_VFD_TRIP      = (1u << 1),
    FUEL_FAULT_OGM_STALL     = (1u << 2),
    FUEL_FAULT_OVER_TARGET   = (1u << 3),
    FUEL_FAULT_VALVE         = (1u << 4),
    FUEL_FAULT_STORAGE       = (1u << 5),
    FUEL_FAULT_CHECKPOINT    = (1u << 6),
    FUEL_FAULT_SENSOR        = (1u << 7)
} fuel_fault_t;

/**
 * @brief ? OGM ?????????liter_x100??? 2 ????
 */
static inline uint32_t Fuel_LiterX100FromPulse(uint32_t pulse_count,
                                               uint32_t pulses_per_liter)
{
    if (pulses_per_liter == 0u) {
        return 0u;
    }
    return (uint32_t)(((uint64_t)pulse_count * 100u) / (uint64_t)pulses_per_liter);
}

/**
 * @brief 由升数 liter_x100 换算脉冲数
 */
static inline uint32_t Fuel_PulseFromLiterX100(uint32_t liter_x100,
                                               uint32_t pulses_per_liter)
{
    if (pulses_per_liter == 0u || liter_x100 == 0u) {
        return 0u;
    }
    return (uint32_t)(((uint64_t)liter_x100 * (uint64_t)pulses_per_liter) / 100u);
}

/**
 * @brief 由目标升数 + 补偿升数（均 liter_x100）计算停泵目标脉冲
 */
static inline uint32_t Fuel_PulseTargetFromLiterX100(uint32_t liter_x100,
                                                     uint32_t pulses_per_liter,
                                                     uint32_t comp_liter_x100)
{
    uint64_t total;

    if (pulses_per_liter == 0u || liter_x100 == 0u) {
        return 0u;
    }

    total = Fuel_PulseFromLiterX100(liter_x100, pulses_per_liter);
    total += Fuel_PulseFromLiterX100(comp_liter_x100, pulses_per_liter);
    return (uint32_t)total;
}

/**
 * @brief ?? liter_x100 ???????
 */
static inline uint8_t Fuel_IsLiterX100InRange(uint32_t liter_x100,
                                              uint32_t min_x100,
                                              uint32_t max_x100)
{
    return (liter_x100 >= min_x100 && liter_x100 <= max_x100) ? 1u : 0u;
}

#ifdef __cplusplus
}
#endif

#endif /* FUEL_TYPES_H */
