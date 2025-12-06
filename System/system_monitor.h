/**
 * @file system_monitor.h
 * @brief 系统监控模块（System Monitor）
 * @version 1.0.0
 * @date 2024-01-01
 * @details 提供系统指标监控、健康检查、数据记录、告警机制
 * 
 * @note 配置管理：
 * - 模块开关：通过 System/config.h 中的 CONFIG_MODULE_SYSTEM_MONITOR_ENABLED 控制
 * - 检查间隔：通过 System/config.h 中的 CONFIG_SYSTEM_MONITOR_CHECK_INTERVAL 控制
 * - 日志间隔：通过 System/config.h 中的 CONFIG_SYSTEM_MONITOR_LOG_INTERVAL 控制
 * - 告警阈值：通过 System/config.h 中的 CONFIG_SYSTEM_MONITOR_CPU_THRESHOLD 等控制
 * 
 * @section 使用方法
 * 
 * @subsection 初始化
 * 初始化系统监控模块：
 * @code
 * SystemMonitor_ErrorCode_t err = SystemMonitor_Init();
 * if (err != SYSMON_OK)
 * {
 *     // 初始化失败
 * }
 * @endcode
 * 
 * @subsection 监控任务
 * 在主循环中定期调用监控任务：
 * @code
 * while (1)
 * {
 *     // 系统监控任务（建议每100ms调用一次）
 *     SystemMonitor_Task();
 *     
 *     // 其他业务逻辑
 *     // ...
 *     
 *     Delay_ms(100);
 * }
 * @endcode
 * 
 * @subsection 获取系统状态
 * 获取系统状态信息：
 * @code
 * SystemMonitor_Status_t status;
 * if (SystemMonitor_GetStatus(&status) == SYSMON_OK)
 * {
 *     // status.cpu_usage: CPU使用率（%）
 *     // status.free_heap: 空闲堆内存（字节）
 *     // status.min_free_heap: 最小空闲堆内存（字节）
 *     // status.stack_usage: 栈使用量（字节）
 *     // status.error_count: 错误总数
 *     // status.exception_count: 异常总数
 *     // status.uptime_sec: 运行时间（秒）
 *     // status.current_freq: 当前系统频率（Hz）
 *     // status.temperature: 内部温度（°C）
 * }
 * @endcode
 * 
 * @subsection 查询单项指标
 * 查询单项系统指标：
 * @code
 * uint8_t cpu_usage = SystemMonitor_GetCPUUsage();        // CPU使用率
 * uint32_t free_heap = SystemMonitor_GetFreeHeap();       // 空闲堆内存
 * uint32_t min_free_heap = SystemMonitor_GetMinFreeHeap(); // 最小空闲堆内存
 * uint32_t stack_usage = SystemMonitor_GetStackUsage();    // 栈使用量
 * uint32_t uptime = SystemMonitor_GetUptime();            // 运行时间（秒）
 * uint32_t error_count = SystemMonitor_GetErrorCount();   // 错误总数
 * uint32_t exception_count = SystemMonitor_GetExceptionCount(); // 异常总数
 * int16_t temperature = SystemMonitor_GetTemperature();    // 内部温度（°C）
 * @endcode
 * 
 * @subsection 记录异常
 * 在异常处理函数中记录异常：
 * @code
 * void HardFault_Handler(void)
 * {
 *     SystemMonitor_RecordException();  // 记录异常发生
 *     // 异常处理逻辑
 * }
 * @endcode
 * 
 * @subsection 日志输出
 * 手动触发状态日志输出：
 * @code
 * SystemMonitor_LogStatus();  // 输出系统状态到日志
 * @endcode
 * 
 * @subsection 重置统计
 * 重置统计信息：
 * @code
 * SystemMonitor_ResetStats();  // 重置错误计数、异常计数等
 * @endcode
 * 
 * @subsection 异常寄存器
 * 读取和清除异常寄存器：
 * @code
 * SystemMonitor_ExceptionRegs_t regs;
 * if (SystemMonitor_ReadExceptionRegs(&regs) == SYSMON_OK)
 * {
 *     // regs.cfsr: Configurable Fault Status Register
 *     // regs.hfsr: Hard Fault Status Register
 *     // regs.mmfar: Memory Manage Fault Address Register
 *     // regs.bfar: Bus Fault Address Register
 *     // regs.dfsr: Debug Fault Status Register
 * }
 * 
 * // 清除异常寄存器（建议先读取再清除）
 * SystemMonitor_ClearExceptionRegs();
 * @endcode
 * 
 * @section 监控功能
 * 
 * 系统监控模块提供以下监控功能：
 * - **CPU使用率**：通过clock_manager模块获取（如果启用）
 * - **堆内存**：自动检测FreeRTOS环境，优先使用FreeRTOS API
 * - **栈内存**：通过魔法数字法检测栈使用量
 * - **错误统计**：通过error_handler模块获取（如果启用）
 * - **异常统计**：记录异常发生次数
 * - **运行时间**：从初始化开始计算的运行时间
 * - **系统频率**：当前系统时钟频率
 * - **内部温度**：STM32内部温度传感器（ADC通道16）
 * 
 * @section 告警机制
 * 
 * 系统监控模块提供自动告警功能：
 * - **CPU使用率告警**：超过CONFIG_SYSTEM_MONITOR_CPU_THRESHOLD（默认80%）时告警
 * - **堆内存告警**：低于CONFIG_SYSTEM_MONITOR_HEAP_THRESHOLD（默认1024字节）时告警
 * - **告警抑制**：相同告警至少间隔5秒才再次输出，避免告警刷屏
 * 
 * @section 注意事项
 * 
 * 1. **初始化顺序**：需要在System_Init()之后初始化
 * 2. **定期调用**：需要在主循环中定期调用SystemMonitor_Task()，建议每100ms一次
 * 3. **堆内存统计**：堆内存统计需要编译器支持（Keil MDK），其他编译器可能不支持
 * 4. **栈内存统计**：栈内存统计通过魔法数字法，需要正确配置CONFIG_STACK_SIZE
 * 5. **FreeRTOS支持**：如果使用FreeRTOS，会自动使用FreeRTOS的堆内存API
 * 6. **温度传感器**：温度传感器首次使用时自动初始化，需要ADC1和GPIOA时钟
 * 7. **告警抑制**：告警有抑制机制，相同告警至少间隔5秒才再次输出
 * 
 * @section 配置说明
 * 
 * 模块开关在System/config.h中配置：
 * - CONFIG_MODULE_SYSTEM_MONITOR_ENABLED：系统监控模块开关（默认启用）
 * - CONFIG_SYSTEM_MONITOR_CHECK_INTERVAL：监控检查间隔（默认1000ms）
 * - CONFIG_SYSTEM_MONITOR_LOG_INTERVAL：日志输出间隔（默认5000ms）
 * - CONFIG_SYSTEM_MONITOR_CPU_THRESHOLD：CPU使用率告警阈值（默认80%）
 * - CONFIG_SYSTEM_MONITOR_HEAP_THRESHOLD：堆内存告警阈值（默认1024字节）
 * - CONFIG_STACK_SIZE：栈总大小（默认0x400=1KB），需与startup.s中的Stack_Size一致
 * 
 * @section 相关模块
 * 
 * - TIM2_TimeBase：时间基准模块（Drivers/timer/TIM2_TimeBase.c/h）
 * - ClockManager：时钟管理模块（System/clock_manager.c/h），用于获取CPU使用率
 * - ErrorHandler：错误处理模块（Common/error_handler.c/h），用于获取错误统计
 * - Log：日志模块（Debug/log.c/h），用于输出监控日志
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
