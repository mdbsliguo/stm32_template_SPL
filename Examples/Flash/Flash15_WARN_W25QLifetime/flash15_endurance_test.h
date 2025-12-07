/**
 * @file flash15_endurance_test.h
 * @brief Flash15正式寿命测试模块接口声明
 * @version 1.0.0
 * @date 2024-01-01
 * @details W25Q系列Flash芯片破坏性寿命极限测试流程（测到报废）
 * 
 * 测试流程：
 * 1. 测试准备与基准建立阶段
 * 2. 主循环：破坏性擦写测试
 * 3. 定期深度健康检查
 * 4. 报废判定标准
 * 5. 数据记录与分析
 */

#ifndef FLASH15_ENDURANCE_TEST_H
#define FLASH15_ENDURANCE_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include "w25q_spi.h"
#include <stdint.h>

/* ==================== 错误码定义 ==================== */

/**
 * @brief 寿命测试模块错误码基值
 * @note 案例层模块，使用临时基值
 */
#define ERROR_BASE_ENDURANCE_TEST -5200

/**
 * @brief 寿命测试状态枚举
 */
typedef enum {
    ENDURANCE_TEST_OK = ERROR_OK,                                    /**< 操作成功 */
    ENDURANCE_TEST_ERROR_NOT_INIT = ERROR_BASE_ENDURANCE_TEST - 1,     /**< 模块未初始化 */
    ENDURANCE_TEST_ERROR_NULL_PTR = ERROR_BASE_ENDURANCE_TEST - 2,     /**< 空指针错误 */
    ENDURANCE_TEST_ERROR_INVALID_PARAM = ERROR_BASE_ENDURANCE_TEST - 3, /**< 无效参数 */
    ENDURANCE_TEST_ERROR_W25Q_FAILED = ERROR_BASE_ENDURANCE_TEST - 4,  /**< W25Q操作失败 */
    ENDURANCE_TEST_ERROR_TIMEOUT = ERROR_BASE_ENDURANCE_TEST - 5,      /**< 超时错误 */
    ENDURANCE_TEST_ERROR_MEMORY = ERROR_BASE_ENDURANCE_TEST - 6,      /**< 内存不足 */
    ENDURANCE_TEST_ERROR_CHIP_DEAD = ERROR_BASE_ENDURANCE_TEST - 7     /**< 芯片已报废 */
} Endurance_Test_Status_t;

/* ==================== 芯片状态定义 ==================== */

/**
 * @brief 芯片状态枚举
 */
typedef enum {
    CHIP_STATUS_NORMAL = 0,      /**< 正常状态 */
    CHIP_STATUS_WARNING = 1,     /**< 预警状态（性能退化） */
    CHIP_STATUS_DANGER = 2,     /**< 危险状态（接近报废） */
    CHIP_STATUS_DEAD = 3         /**< 报废状态 */
} Chip_Status_t;

/* ==================== 测试配置 ==================== */

/**
 * @brief 寿命测试配置结构体
 */
typedef struct {
    uint32_t deep_check_interval;   /**< 深度检查间隔（每N次循环执行一次） */
    uint32_t log_interval;           /**< 日志记录间隔（每N次循环记录一次） */
    uint8_t verbose_log;             /**< 详细日志标志：1=输出详细日志，0=仅输出汇总 */
} Endurance_Test_Config_t;

/* ==================== 基准数据 ==================== */

/**
 * @brief 基准数据结构体（0次擦写循环时的初始状态）
 */
typedef struct {
    float erase_time_min;            /**< 最小擦除时间（毫秒） */
    float erase_time_avg;            /**< 平均擦除时间（毫秒） */
    float program_time_avg;          /**< 平均编程时间（毫秒） */
    float read_speed;                 /**< 读取速度（KB/s） */
    uint32_t error_rate;             /**< 初始错误率（位错误数） */
    uint64_t unique_id;              /**< Unique ID（用于追踪） */
} Baseline_Data_t;

/* ==================== 测试结果结构体 ==================== */

/**
 * @brief 寿命测试结果结构体
 */
typedef struct {
    /* 循环统计 */
    uint32_t total_cycles;           /**< 总P/E循环次数 */
    uint32_t current_cycle;           /**< 当前循环次数 */
    
    /* 数据量统计 */
    uint64_t total_data_written_mb;   /**< 总写入数据量（MB） */
    
    /* 时间统计 */
    float erase_time_current;         /**< 当前擦除时间（毫秒） */
    float erase_time_avg;             /**< 平均擦除时间（毫秒） */
    float erase_time_min;             /**< 最小擦除时间（毫秒） */
    float erase_time_max;             /**< 最大擦除时间（毫秒） */
    float program_time_avg;           /**< 平均编程时间（毫秒） */
    
    /* 错误统计 */
    uint32_t erase_errors;           /**< 擦除错误次数 */
    uint32_t program_errors;         /**< 编程错误次数 */
    uint32_t verify_errors;           /**< 校验错误次数（位错误数） */
    uint32_t bad_block_count;         /**< 坏块数量 */
    float error_rate;                 /**< 误码率 */
    
    /* 性能退化 */
    float erase_degradation_rate;     /**< 擦除时间退化率（%） */
    float program_degradation_rate;   /**< 编程时间退化率（%） */
    
    /* 读干扰 */
    uint32_t read_disturb_errors;     /**< 读干扰错误数 */
    
    /* 芯片状态 */
    Chip_Status_t chip_status;        /**< 芯片状态 */
    float lifetime_score;             /**< 寿命评分（0-100） */
    
    /* 基准数据 */
    Baseline_Data_t baseline;         /**< 基准数据（初始状态） */
    
    /* 测试标志 */
    uint8_t baseline_recorded;         /**< 基准数据已记录标志 */
    uint8_t chip_dead;                /**< 芯片已报废标志 */
} Endurance_Test_Result_t;

/* ==================== 公共接口函数 ==================== */

/**
 * @brief 寿命测试模块初始化
 * @return Endurance_Test_Status_t 错误码
 */
Endurance_Test_Status_t EnduranceTest_Init(void);

/**
 * @brief 运行完整寿命测试流程（测到报废）
 * @param[in] config 测试配置指针
 * @param[out] result 测试结果结构体指针
 * @return Endurance_Test_Status_t 错误码
 * @note 持续执行P/E循环，直至芯片报废
 */
Endurance_Test_Status_t EnduranceTest_Run(Endurance_Test_Config_t *config, Endurance_Test_Result_t *result);

/**
 * @brief 记录基准数据（0次擦写循环时）
 * @param[out] result 测试结果结构体指针
 * @return Endurance_Test_Status_t 错误码
 */
Endurance_Test_Status_t EnduranceTest_RecordBaseline(Endurance_Test_Result_t *result);

/**
 * @brief 执行单次P/E循环
 * @param[in,out] result 测试结果结构体指针
 * @return Endurance_Test_Status_t 错误码
 * @note 执行一次完整的P/E循环：擦除->写入->读取->校验
 */
Endurance_Test_Status_t EnduranceTest_RunSingleCycle(Endurance_Test_Result_t *result);

/**
 * @brief 执行深度健康检查
 * @param[in,out] result 测试结果结构体指针
 * @return Endurance_Test_Status_t 错误码
 * @note 每N次循环执行一次，监测性能退化轨迹
 */
Endurance_Test_Status_t EnduranceTest_DeepHealthCheck(Endurance_Test_Result_t *result);

/**
 * @brief 检查芯片是否达到报废标准
 * @param[in] result 测试结果结构体指针
 * @param[out] is_dead 是否报废标志（1=报废，0=未报废）
 * @return Endurance_Test_Status_t 错误码
 */
Endurance_Test_Status_t EnduranceTest_CheckEndOfLife(Endurance_Test_Result_t *result, uint8_t *is_dead);

/**
 * @brief 计算寿命评分
 * @param[in,out] result 测试结果结构体指针
 * @return Endurance_Test_Status_t 错误码
 */
Endurance_Test_Status_t EnduranceTest_CalculateLifetimeScore(Endurance_Test_Result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* FLASH15_ENDURANCE_TEST_H */

