/**
 * @file system_monitor.h
 * @brief 系统监控模块（System Monitor）
 * @version 1.0.0
 * @date 2024-01-01
 * @details 提供系统指标监控、健康检查、数据记录、告警机制
 * 
 * @note 配置管理：
 * - 模块开关：通过 system/config.h 中的 CONFIG_MODULE_SYSTEM_MONITOR_ENABLED 控制
 * - 检查间隔：通过 system/config.h 中的 CONFIG_SYSTEM_MONITOR_CHECK_INTERVAL 控制
 * - 日志间隔：通过 system/config.h 中的 CONFIG_SYSTEM_MONITOR_LOG_INTERVAL 控制
 * - 告警阈值：通过 system/config.h 中的 CONFIG_SYSTEM_MONITOR_CPU_THRESHOLD 等控制
 */

#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include "error_code.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 系统监控错误码
 */
typedef enum {
    SYSMON_OK = ERROR_OK,                                    /**< 操作成功 */
    SYSMON_ERROR_NOT_INITIALIZED = ERROR_BASE_SYSTEM_MONITOR - 1,  /**< 未初始化 */
    SYSMON_ERROR_INVALID_PARAM = ERROR_BASE_SYSTEM_MONITOR - 2,    /**< 参数非法 */
} SystemMonitor_ErrorCode_t;

/**
 * @brief 系统监控状态结构体
 */
typedef struct {
    uint8_t cpu_usage;           /**< CPU使用率（%） */
    uint32_t free_heap;          /**< 空闲堆内存（字节），0表示不支持 */
    uint32_t min_free_heap;      /**< 最小空闲堆内存（字节），0表示不支持 */
    uint32_t stack_usage;        /**< 栈使用量（字节），0表示不支持 */
    uint32_t error_count;        /**< 错误总数 */
    uint32_t exception_count;    /**< 异常总数 */
    uint32_t uptime_sec;         /**< 运行时间（秒） */
    uint32_t current_freq;       /**< 当前系统频率（Hz） */
    int16_t temperature;         /**< 内部温度（°C），-128表示不支持或未初始化 */
} SystemMonitor_Status_t;

/**
 * @brief 异常寄存器信息结构体
 */
typedef struct {
    uint32_t cfsr;      /**< Configurable Fault Status Register */
    uint32_t hfsr;      /**< Hard Fault Status Register */
    uint32_t mmfar;     /**< Memory Manage Fault Address Register */
    uint32_t bfar;      /**< Bus Fault Address Register */
    uint32_t dfsr;      /**< Debug Fault Status Register */
} SystemMonitor_ExceptionRegs_t;

/**
 * @brief 初始化系统监控模块
 * @return SystemMonitor_ErrorCode_t 错误码
 * @note 需要在System_Init()之后调用
 */
SystemMonitor_ErrorCode_t SystemMonitor_Init(void);

/**
 * @brief 反初始化系统监控模块
 * @return SystemMonitor_ErrorCode_t 错误码
 */
SystemMonitor_ErrorCode_t SystemMonitor_Deinit(void);

/**
 * @brief 检查系统监控模块是否已初始化
 * @return 1-已初始化，0-未初始化
 */
uint8_t SystemMonitor_IsInitialized(void);

/**
 * @brief 系统监控任务（需要在主循环中定期调用）
 * @note 建议在主循环中每100ms调用一次
 * @note 内部会根据配置的间隔自动执行检查和日志输出
 */
void SystemMonitor_Task(void);

/**
 * @brief 获取系统状态
 * @param[out] status 系统状态结构体指针
 * @return SystemMonitor_ErrorCode_t 错误码
 */
SystemMonitor_ErrorCode_t SystemMonitor_GetStatus(SystemMonitor_Status_t* status);

/**
 * @brief 记录监控数据到日志
 * @note 如果日志系统可用，会输出系统状态信息
 */
void SystemMonitor_LogStatus(void);

/**
 * @brief 获取CPU使用率
 * @return CPU使用率（0-100），如果不可用返回0
 */
uint8_t SystemMonitor_GetCPUUsage(void);

/**
 * @brief 获取空闲堆内存
 * @return 空闲堆内存（字节），如果不可用返回0
 */
uint32_t SystemMonitor_GetFreeHeap(void);

/**
 * @brief 获取最小空闲堆内存
 * @return 最小空闲堆内存（字节），如果不可用返回0
 */
uint32_t SystemMonitor_GetMinFreeHeap(void);

/**
 * @brief 获取栈使用量
 * @return 栈使用量（字节），如果不可用返回0
 * @note 通过魔法数字法检测栈使用量
 */
uint32_t SystemMonitor_GetStackUsage(void);

/**
 * @brief 获取运行时间
 * @return 运行时间（秒）
 */
uint32_t SystemMonitor_GetUptime(void);

/**
 * @brief 获取错误总数
 * @return 错误总数，如果统计功能未启用返回0
 */
uint32_t SystemMonitor_GetErrorCount(void);

/**
 * @brief 获取异常总数
 * @return 异常总数
 */
uint32_t SystemMonitor_GetExceptionCount(void);

/**
 * @brief 记录异常发生
 * @note 在异常处理函数中调用，用于统计异常次数
 */
void SystemMonitor_RecordException(void);

/**
 * @brief 重置统计信息
 * @note 重置错误计数、异常计数等统计信息
 */
void SystemMonitor_ResetStats(void);

/**
 * @brief 获取内存使用率（堆内存）
 * @param[in] total_heap 总堆内存大小（字节），如果为0则返回0
 * @return 内存使用率（0-100），如果不可用返回0
 * @note 使用率 = (总内存 - 空闲内存) / 总内存 * 100
 * @note 如果 total_heap 为0，表示总内存未知，返回0
 */
uint8_t SystemMonitor_GetMemoryUsage(uint32_t total_heap);

/**
 * @brief 获取栈使用率
 * @param[in] total_stack 总栈内存大小（字节），如果为0则返回0
 * @return 栈使用率（0-100），如果不可用返回0
 * @note 使用率 = 已使用栈 / 总栈 * 100
 * @note 如果 total_stack 为0，表示总栈未知，返回0
 */
uint8_t SystemMonitor_GetStackUsagePercent(uint32_t total_stack);

/**
 * @brief 读取异常寄存器
 * @param[out] regs 异常寄存器结构体指针
 * @return SystemMonitor_ErrorCode_t 错误码
 * @note 读取SCB异常相关寄存器（CFSR、HFSR、MMFAR、BFAR、DFSR）
 * @note 需要包含 core_cm3.h 来访问 SCB 寄存器
 */
SystemMonitor_ErrorCode_t SystemMonitor_ReadExceptionRegs(SystemMonitor_ExceptionRegs_t* regs);

/**
 * @brief 清除异常寄存器
 * @return SystemMonitor_ErrorCode_t 错误码
 * @note 清除SCB异常相关寄存器（CFSR、HFSR、DFSR）
 * @warning 清除后无法追溯异常原因，建议先读取再清除
 */
SystemMonitor_ErrorCode_t SystemMonitor_ClearExceptionRegs(void);

/**
 * @brief 获取内部温度传感器温度
 * @return 温度值（°C），如果不可用返回-128
 * @note 使用STM32内部温度传感器（ADC通道16）
 * @note 如果温度传感器未初始化，会自动初始化
 */
int16_t SystemMonitor_GetTemperature(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_MONITOR_H */

