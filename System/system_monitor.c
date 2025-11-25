/**
 * @file system_monitor.c
 * @brief 系统监控模块实现
 * @version 2.0.0
 * @date 2024-01-01
 * @details 提供系统指标监控、健康检查、数据记录、告警机制
 * 
 * @note 已完善：
 * - ? FreeRTOS支持：自动检测FreeRTOS环境，优先使用FreeRTOS的堆内存API
 * 
 * @details 实现说明：
 * - 堆内存监控：自动检测FreeRTOS环境，优先使用FreeRTOS API，否则使用Keil MDK符号
 * - 栈内存监控：通过魔法数字法（0x5A5A5A5A）检测栈使用量
 * - CPU使用率：通过clock_manager模块获取（如果启用）
 * - 温度传感器：使用STM32内部温度传感器（ADC通道16），首次使用时自动初始化
 * - 告警机制：提供CPU和堆内存告警，带抑制机制（相同告警至少间隔5秒）
 */

#include "system_monitor.h"
#include <config.h>
#include "TIM2_TimeBase.h"
#include "delay.h"
#include "stm32f10x.h"  /* 用于SystemCoreClock */
#include "core_cm3.h"   /* 用于访问SCB寄存器 */
#include <string.h>
#include <limits.h>  /* 用于UINT32_MAX */
#include <stdlib.h>  /* 用于malloc/free，初始化堆指针 */

/* FreeRTOS支持检测 */
#ifdef FREERTOS_H
#include "FreeRTOS.h"
#include "task.h"
#endif

/* 模块开关检查 */
#if !CONFIG_MODULE_SYSTEM_MONITOR_ENABLED
/* 如果模块被禁用，所有函数都为空操作 */
SystemMonitor_ErrorCode_t SystemMonitor_Init(void) { return SYSMON_OK; }
SystemMonitor_ErrorCode_t SystemMonitor_Deinit(void) { return SYSMON_OK; }
uint8_t SystemMonitor_IsInitialized(void) { return 0; }
void SystemMonitor_Task(void) {}
SystemMonitor_ErrorCode_t SystemMonitor_GetStatus(SystemMonitor_Status_t* status) { return SYSMON_ERROR_NOT_INITIALIZED; }
void SystemMonitor_LogStatus(void) {}
uint8_t SystemMonitor_GetCPUUsage(void) { return 0; }
uint32_t SystemMonitor_GetFreeHeap(void) { return 0; }
uint32_t SystemMonitor_GetMinFreeHeap(void) { return 0; }
uint32_t SystemMonitor_GetUptime(void) { return 0; }
uint32_t SystemMonitor_GetErrorCount(void) { return 0; }
uint32_t SystemMonitor_GetExceptionCount(void) { return 0; }
void SystemMonitor_RecordException(void) {}
void SystemMonitor_ResetStats(void) {}
uint8_t SystemMonitor_GetMemoryUsage(uint32_t total_heap) { (void)total_heap; return 0; }
SystemMonitor_ErrorCode_t SystemMonitor_ReadExceptionRegs(SystemMonitor_ExceptionRegs_t* regs) { (void)regs; return SYSMON_ERROR_NOT_INITIALIZED; }
SystemMonitor_ErrorCode_t SystemMonitor_ClearExceptionRegs(void) { return SYSMON_ERROR_NOT_INITIALIZED; }
#else

/* 条件包含相关模块 */
#ifdef CLOCK_MANAGER_H
#include "clock_manager.h"
#endif

#ifdef ERROR_HANDLER_H
#include "error_handler.h"
#endif

#ifdef LOG_H
#include "log.h"
#endif

#ifdef IWDG_H
#include "iwdg.h"
#endif

/* ADC和温度传感器相关 */
#include "stm32f10x_adc.h"
#include "stm32f10x_rcc.h"

/* 模块状态 */
static struct {
    uint8_t is_initialized;              /**< 是否已初始化 */
    uint32_t last_check_tick;            /**< 上次检查时间（ms） */
    uint32_t last_log_tick;              /**< 上次日志输出时间（ms） */
    uint32_t min_free_heap_recorded;     /**< 记录的最小空闲堆内存 */
    uint32_t exception_count;            /**< 异常总数 */
    uint32_t init_tick;                  /**< 初始化时的tick（用于计算运行时间） */
    uint32_t last_warn_cpu_tick;         /**< 上次CPU告警时间（ms），用于告警抑制 */
    uint32_t last_warn_heap_tick;        /**< 上次堆内存告警时间（ms），用于告警抑制 */
    uint8_t temp_sensor_initialized;      /**< 温度传感器是否已初始化 */
} g_sysmon_state = {0};

/* 外部符号：堆内存和栈内存地址（由启动文件定义，Keil MDK） */
/* 注意：这些符号由startup.s文件导出，需要确保启动文件正确链接 */
#if defined(__CC_ARM) || defined(__ARMCC_VERSION)
/* Keil MDK编译器 */
extern char __heap_base;      /* 堆起始地址（由startup.s导出） */
extern char __heap_limit;     /* 堆结束地址（由startup.s导出） */
extern char __initial_sp;     /* 栈顶地址（最高地址，由startup.s导出） */
/* __heapptr 是Keil运行时库维护的，只有在使用malloc时才存在 */
/* 如果未使用malloc，__heapptr可能不存在，需要使用替代方案 */
/* 注意：如果链接时找不到__heapptr，说明未使用malloc，需要提供替代实现 */
/* 方案：先声明为弱符号，如果不存在则使用__heap_base作为替代 */
#if defined(__CC_ARM) || defined(__ARMCC_VERSION)
/* ARMCC编译器支持弱符号 */
#pragma weak __heapptr
extern char *__heapptr;  /* Keil内部维护的当前堆指针（可能不存在） */
#else
extern char *__heapptr;  /* Keil内部维护的当前堆指针（可能不存在） */
#endif
#define HEAP_SYMBOLS_AVAILABLE 1
#else
/* 其他编译器，符号可能不同 */
#define HEAP_SYMBOLS_AVAILABLE 0
#endif

/* 栈监控：使用魔法数字填充栈底来检测栈使用量 */
#define STACK_MAGIC_VALUE 0x5A5A5A5A
#define STACK_MONITOR_SIZE 128  /* 监控栈底128字节 */

/**
 * @brief 获取空闲堆内存（内部函数）
 * @return 空闲堆内存（字节），如果不可用返回0
 * @note 自动检测FreeRTOS环境，优先使用FreeRTOS的API
 */
static uint32_t GetFreeHeapInternal(void)
{
    /* 如果使用FreeRTOS，优先使用FreeRTOS的API */
    #ifdef FREERTOS_H
    return xPortGetFreeHeapSize();
    #endif
    
    #if HEAP_SYMBOLS_AVAILABLE
    /* 使用Keil MDK的堆符号（由startup.s导出） */
    uint32_t heap_base = (uint32_t)&__heap_base;
    uint32_t heap_limit = (uint32_t)&__heap_limit;
    
    if (heap_limit <= heap_base)
    {
        return 0;  /* 无效的堆配置 */
    }
    
    uint32_t heap_total = heap_limit - heap_base;
    
    /* 尝试使用__heapptr来计算已使用的堆内存 */
    /* 注意：__heapptr指向堆中下一个可分配的位置 */
    /* 如果__heapptr未定义（未使用malloc），则返回总堆大小（假设堆未被使用） */
    uint32_t heapptr_addr;
    
    /* 检查__heapptr是否可用（通过检查其是否为NULL或地址是否在合理范围内） */
    #if defined(__CC_ARM) || defined(__ARMCC_VERSION)
    /* ARMCC编译器，如果使用了#pragma weak，__heapptr可能为NULL */
    if (__heapptr == NULL)
    {
        /* __heapptr不可用（未使用malloc），返回总堆大小 */
        return heap_total;
    }
    #endif
    
    heapptr_addr = (uint32_t)__heapptr;
    
    /* 检查__heapptr是否在有效范围内 */
    if (heapptr_addr < heap_base || heapptr_addr > heap_limit)
    {
        /* __heapptr不在有效范围内，可能未使用malloc，返回总堆大小 */
        return heap_total;  /* 假设堆未被使用，返回全部空闲 */
    }
    
    /* 计算已使用的堆内存 */
    uint32_t heap_used = heapptr_addr - heap_base;
    
    /* 检查堆指针是否在有效范围内 */
    if (heap_used > heap_total)
    {
        return 0;  /* 堆指针异常 */
    }
    
    /* 返回空闲堆内存 */
    return heap_total - heap_used;
    #else
    /* 其他编译器或不支持的情况 */
    return 0;  /* 当前不支持堆内存统计 */
    #endif
}

/* 栈监控状态 */
static struct {
    uint8_t is_stack_monitor_init;  /* 栈监控是否已初始化 */
    uint32_t stack_total;           /* 栈总大小（字节） */
} g_stack_monitor = {0};

/**
 * @brief 初始化栈监控（填充魔法数字）
 * @note 在SystemMonitor_Init()中调用一次
 */
static void InitStackMonitor(void)
{
    #if HEAP_SYMBOLS_AVAILABLE
    /* 默认栈大小：0x400 (1KB)，如果不同需要配置 */
    #ifndef CONFIG_STACK_SIZE
    #define CONFIG_STACK_SIZE 0x400  /* 默认1KB栈 */
    #endif
    
    uint32_t stack_size = CONFIG_STACK_SIZE;
    uint32_t *stack_bottom = (uint32_t *)((uint32_t)&__initial_sp - stack_size);
    
    /* 填充栈底为魔法数字0x5A5A5A5A */
    /* 填充栈底区域（128字节），用于检测栈使用量 */
    /* 注意：只填充栈底128字节，避免越界和性能问题 */
    uint32_t fill_words = STACK_MONITOR_SIZE / 4;  /* 填充128字节 = 32个字 */
    
    /* 边界检查：确保不会越界 */
    if (stack_size < STACK_MONITOR_SIZE)
    {
        fill_words = stack_size / 4;  /* 如果栈小于128字节，只填充栈大小 */
    }
    
    for (uint32_t i = 0; i < fill_words; i++)
    {
        stack_bottom[i] = STACK_MAGIC_VALUE;
    }
    
    g_stack_monitor.stack_total = stack_size;
    g_stack_monitor.is_stack_monitor_init = 1;
    #endif
}

/**
 * @brief 获取栈使用量（内部函数）
 * @return 栈使用量（字节），如果不可用返回0
 * @note 通过扫描栈底魔法数字来检测栈使用量
 */
static uint32_t GetStackUsageInternal(void)
{
    #if HEAP_SYMBOLS_AVAILABLE
    if (!g_stack_monitor.is_stack_monitor_init)
    {
        return 0;  /* 栈监控未初始化 */
    }
    
    /* 从栈底向上扫描，找到第一个被修改的位置 */
    uint32_t *p = (uint32_t *)((uint32_t)&__initial_sp - g_stack_monitor.stack_total);
    uint32_t unused_bytes = 0;
    
    /* 扫描栈底监控区域，统计未被覆盖的魔法数字 */
    /* 只要遇到魔法数字就继续，直到找到第一个非魔法数字的位置 */
    /* 限制扫描范围：最多扫描监控区域大小（128字节），避免越界 */
    uint32_t max_scan_bytes = STACK_MONITOR_SIZE;
    if (g_stack_monitor.stack_total < max_scan_bytes)
    {
        max_scan_bytes = g_stack_monitor.stack_total;  /* 如果栈小于128字节，只扫描栈大小 */
    }
    
    while (unused_bytes < max_scan_bytes)
    {
        if (*p != STACK_MAGIC_VALUE)
        {
            break;  /* 找到被使用的栈区域 */
        }
        unused_bytes += 4;
        p++;
    }
    
    /* 计算已使用的栈大小：已使用 = 总栈 - 未使用的（魔法数字区域） */
    /* 注意：如果扫描完监控区域都没找到被修改的位置，说明栈使用量超过了监控区域 */
    /* 此时无法准确计算，返回一个估算值：假设栈使用量至少是监控区域大小 */
    uint32_t stack_used;
    if (unused_bytes >= max_scan_bytes)
    {
        /* 扫描完监控区域都没找到被修改的位置，说明栈使用量至少是监控区域大小 */
        /* 返回监控区域大小作为最小使用量（保守估算） */
        stack_used = max_scan_bytes;
    }
    else
    {
        /* 找到了被修改的位置，可以准确计算 */
        stack_used = g_stack_monitor.stack_total - unused_bytes;
    }
    
    return stack_used;
    #else
    /* 其他编译器或不支持的情况 */
    return 0;  /* 当前不支持栈使用统计 */
    #endif
}

/**
 * @brief 计算时间差（处理溢出）
 * @param[in] current 当前tick
 * @param[in] previous 之前的tick
 * @return 时间差（ms）
 * @note 如果 previous == 0，表示从未设置过，返回一个很大的值（表示已经过了很长时间）
 */
static inline uint32_t CalculateElapsed(uint32_t current, uint32_t previous)
{
    /* 如果 previous == 0，表示从未设置过，返回一个很大的值 */
    if (previous == 0)
    {
        return UINT32_MAX;  /* 表示已经过了很长时间，应该立即执行 */
    }
    
    return (current >= previous) ?
           (current - previous) :
           (UINT32_MAX - previous + current + 1);
}

/**
 * @brief 计算运行时间（秒）
 * @param[in] current_tick 当前tick
 * @return 运行时间（秒）
 * @note init_tick 在初始化时已设置，可能为0（系统刚启动时），需要正常处理
 */
static inline uint32_t CalculateUptime(uint32_t current_tick)
{
    /* init_tick 在初始化时已设置，可能为0（系统刚启动时），需要正常处理 */
    /* 如果 init_tick == 0，直接返回 current_tick / 1000 */
    if (g_sysmon_state.init_tick == 0)
    {
        return current_tick / 1000;
    }
    
    uint32_t elapsed_tick = (current_tick >= g_sysmon_state.init_tick) ?
                           (current_tick - g_sysmon_state.init_tick) :
                           (UINT32_MAX - g_sysmon_state.init_tick + current_tick + 1);
    return elapsed_tick / 1000;
}

/**
 * @brief 检查系统健康状态
 * @param[in] current_tick 当前tick（用于告警抑制）
 * @note 检查CPU使用率、内存使用等，超过阈值时告警
 * @note 告警抑制：相同告警至少间隔5秒才再次输出
 */
static void CheckSystemHealth(uint32_t current_tick)
{
    /* 获取CPU使用率（直接调用内部函数，避免重复初始化检查） */
    #ifdef CLOCK_MANAGER_H
    uint8_t cpu_usage = CLKM_GetCPULoad();
    #else
    uint8_t cpu_usage = 0;
    #endif
    
    /* CPU使用率告警（带抑制机制） */
    if (cpu_usage > CONFIG_SYSTEM_MONITOR_CPU_THRESHOLD)
    {
        uint32_t elapsed_since_warn = CalculateElapsed(current_tick, g_sysmon_state.last_warn_cpu_tick);
        /* 如果从未告警过（last_warn_cpu_tick == 0）或间隔超过5秒，则告警 */
        if (g_sysmon_state.last_warn_cpu_tick == 0 || elapsed_since_warn >= 5000)
        {
            #ifdef LOG_H
            LOG_WARN("SYSMON", "CPU usage high: %d%%", cpu_usage);
            #endif
            g_sysmon_state.last_warn_cpu_tick = current_tick;
        }
    }
    
    /* 获取堆内存（直接调用内部函数，避免重复初始化检查） */
    uint32_t free_heap = GetFreeHeapInternal();
    
    /* 堆内存告警（带抑制机制） */
    if (free_heap > 0 && free_heap < CONFIG_SYSTEM_MONITOR_HEAP_THRESHOLD)
    {
        uint32_t elapsed_since_warn = CalculateElapsed(current_tick, g_sysmon_state.last_warn_heap_tick);
        /* 如果从未告警过（last_warn_heap_tick == 0）或间隔超过5秒，则告警 */
        if (g_sysmon_state.last_warn_heap_tick == 0 || elapsed_since_warn >= 5000)
        {
            #ifdef LOG_H
            LOG_WARN("SYSMON", "Heap memory low: %lu bytes", (unsigned long)free_heap);
            #endif
            g_sysmon_state.last_warn_heap_tick = current_tick;
        }
    }
}

/**
 * @brief 初始化系统监控模块
 */
SystemMonitor_ErrorCode_t SystemMonitor_Init(void)
{
    if (g_sysmon_state.is_initialized)
    {
        return SYSMON_OK;
    }
    
    /* 初始化状态 */
    memset(&g_sysmon_state, 0, sizeof(g_sysmon_state));
    
    /* 记录初始化时间 */
    g_sysmon_state.init_tick = TIM2_TimeBase_GetTick();
    g_sysmon_state.last_check_tick = g_sysmon_state.init_tick;
    g_sysmon_state.last_log_tick = g_sysmon_state.init_tick;
    g_sysmon_state.min_free_heap_recorded = UINT32_MAX;
    g_sysmon_state.last_warn_cpu_tick = 0;
    g_sysmon_state.last_warn_heap_tick = 0;
    
    /* 初始化栈监控（填充魔法数字） */
    InitStackMonitor();
    
    g_sysmon_state.is_initialized = 1;
    
    #ifdef LOG_H
    LOG_INFO("SYSMON", "System Monitor initialized");
    #endif
    
    return SYSMON_OK;
}

/**
 * @brief 反初始化系统监控模块
 */
SystemMonitor_ErrorCode_t SystemMonitor_Deinit(void)
{
    if (!g_sysmon_state.is_initialized)
    {
        return SYSMON_ERROR_NOT_INITIALIZED;
    }
    
    g_sysmon_state.is_initialized = 0;
    
    #ifdef LOG_H
    LOG_INFO("SYSMON", "System Monitor deinitialized");
    #endif
    
    return SYSMON_OK;
}

/**
 * @brief 检查系统监控模块是否已初始化
 */
uint8_t SystemMonitor_IsInitialized(void)
{
    return g_sysmon_state.is_initialized;
}

/**
 * @brief 系统监控任务
 */
void SystemMonitor_Task(void)
{
    if (!g_sysmon_state.is_initialized)
    {
        return;
    }
    
    uint32_t current_tick = TIM2_TimeBase_GetTick();
    
    /* 定期检查系统健康状态 */
    uint32_t check_elapsed = CalculateElapsed(current_tick, g_sysmon_state.last_check_tick);
    
    if (check_elapsed >= CONFIG_SYSTEM_MONITOR_CHECK_INTERVAL)
    {
        CheckSystemHealth(current_tick);
        
        /* 更新最小空闲堆内存 */
        uint32_t free_heap = GetFreeHeapInternal();
        if (free_heap > 0 && free_heap < g_sysmon_state.min_free_heap_recorded)
        {
            g_sysmon_state.min_free_heap_recorded = free_heap;
        }
        
        g_sysmon_state.last_check_tick = current_tick;
    }
    
    /* 定期输出日志 */
    uint32_t log_elapsed = CalculateElapsed(current_tick, g_sysmon_state.last_log_tick);
    
    if (log_elapsed >= CONFIG_SYSTEM_MONITOR_LOG_INTERVAL)
    {
        SystemMonitor_LogStatus();
        g_sysmon_state.last_log_tick = current_tick;
    }
}

/**
 * @brief 获取系统状态
 */
SystemMonitor_ErrorCode_t SystemMonitor_GetStatus(SystemMonitor_Status_t* status)
{
    if (status == NULL)
    {
        return SYSMON_ERROR_INVALID_PARAM;
    }
    
    if (!g_sysmon_state.is_initialized)
    {
        return SYSMON_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取CPU使用率 */
    #ifdef CLOCK_MANAGER_H
    status->cpu_usage = CLKM_GetCPULoad();
    #else
    status->cpu_usage = 0;
    #endif
    
    /* 获取内存信息 */
    status->free_heap = GetFreeHeapInternal();
    status->min_free_heap = (g_sysmon_state.min_free_heap_recorded == UINT32_MAX) ? 
                            status->free_heap : g_sysmon_state.min_free_heap_recorded;
    status->stack_usage = GetStackUsageInternal();
    
    /* 获取错误统计 */
    #ifdef ERROR_HANDLER_H
    #ifdef ERROR_HANDLER_ENABLE_STATS
    status->error_count = ErrorHandler_GetErrorCount();
    #else
    status->error_count = 0;
    #endif
    #else
    status->error_count = 0;
    #endif
    
    /* 获取异常统计 */
    status->exception_count = g_sysmon_state.exception_count;
    
    /* 获取运行时间 */
    uint32_t current_tick = TIM2_TimeBase_GetTick();
    status->uptime_sec = CalculateUptime(current_tick);
    
    /* 获取当前频率 */
    #ifdef CLOCK_MANAGER_H
    status->current_freq = CLKM_GetCurrentFrequency();
    #else
    status->current_freq = SystemCoreClock;
    #endif
    
    /* 获取温度 */
    status->temperature = SystemMonitor_GetTemperature();
    
    return SYSMON_OK;
}

/**
 * @brief 记录监控数据到日志
 */
void SystemMonitor_LogStatus(void)
{
    #ifdef LOG_H
    if (!Log_IsInitialized())
    {
        return;
    }
    
    SystemMonitor_Status_t status;
    if (SystemMonitor_GetStatus(&status) != SYSMON_OK)
    {
        return;
    }
    
    LOG_INFO("SYSMON", "=== System Status ===");
    LOG_INFO("SYSMON", "CPU Usage: %d%%", status.cpu_usage);
    LOG_INFO("SYSMON", "Current Freq: %lu Hz", (unsigned long)status.current_freq);
    
    if (status.free_heap > 0)
    {
        LOG_INFO("SYSMON", "Free Heap: %lu bytes", (unsigned long)status.free_heap);
        LOG_INFO("SYSMON", "Min Free Heap: %lu bytes", (unsigned long)status.min_free_heap);
    }
    
    if (status.stack_usage > 0)
    {
        LOG_INFO("SYSMON", "Stack Usage: %lu bytes", (unsigned long)status.stack_usage);
    }
    
    LOG_INFO("SYSMON", "Uptime: %lu seconds", (unsigned long)status.uptime_sec);
    LOG_INFO("SYSMON", "Error Count: %lu", (unsigned long)status.error_count);
    LOG_INFO("SYSMON", "Exception Count: %lu", (unsigned long)status.exception_count);
    #endif
}

/**
 * @brief 获取CPU使用率
 */
uint8_t SystemMonitor_GetCPUUsage(void)
{
    #ifdef CLOCK_MANAGER_H
    return CLKM_GetCPULoad();
    #else
    return 0;
    #endif
}

/**
 * @brief 获取空闲堆内存
 */
uint32_t SystemMonitor_GetFreeHeap(void)
{
    if (!g_sysmon_state.is_initialized)
    {
        return 0;
    }
    return GetFreeHeapInternal();
}

/**
 * @brief 获取最小空闲堆内存
 */
uint32_t SystemMonitor_GetMinFreeHeap(void)
{
    if (!g_sysmon_state.is_initialized)
    {
        return 0;
    }
    
    if (g_sysmon_state.min_free_heap_recorded == UINT32_MAX)
    {
        return GetFreeHeapInternal();
    }
    return g_sysmon_state.min_free_heap_recorded;
}

/**
 * @brief 获取栈使用量
 */
uint32_t SystemMonitor_GetStackUsage(void)
{
    if (!g_sysmon_state.is_initialized)
    {
        return 0;
    }
    return GetStackUsageInternal();
}

/**
 * @brief 获取运行时间
 */
uint32_t SystemMonitor_GetUptime(void)
{
    if (!g_sysmon_state.is_initialized)
    {
        return 0;
    }
    
    uint32_t current_tick = TIM2_TimeBase_GetTick();
    return CalculateUptime(current_tick);
}

/**
 * @brief 获取错误总数
 */
uint32_t SystemMonitor_GetErrorCount(void)
{
    #ifdef ERROR_HANDLER_H
    #ifdef ERROR_HANDLER_ENABLE_STATS
    return ErrorHandler_GetErrorCount();
    #else
    return 0;
    #endif
    #else
    return 0;
    #endif
}

/**
 * @brief 获取异常总数
 */
uint32_t SystemMonitor_GetExceptionCount(void)
{
    if (!g_sysmon_state.is_initialized)
    {
        return 0;
    }
    return g_sysmon_state.exception_count;
}

/**
 * @brief 记录异常发生
 */
void SystemMonitor_RecordException(void)
{
    if (g_sysmon_state.is_initialized)
    {
        g_sysmon_state.exception_count++;
    }
}

/**
 * @brief 重置统计信息
 */
void SystemMonitor_ResetStats(void)
{
    if (!g_sysmon_state.is_initialized)
    {
        return;
    }
    
    g_sysmon_state.exception_count = 0;
    g_sysmon_state.min_free_heap_recorded = UINT32_MAX;
    g_sysmon_state.last_warn_cpu_tick = 0;      /* 重置告警时间戳，允许立即告警 */
    g_sysmon_state.last_warn_heap_tick = 0;     /* 重置告警时间戳，允许立即告警 */
    
    #ifdef ERROR_HANDLER_H
    #ifdef ERROR_HANDLER_ENABLE_STATS
    ErrorHandler_ResetStats();
    #endif
    #endif
}

/**
 * @brief 获取内存使用率
 */
uint8_t SystemMonitor_GetMemoryUsage(uint32_t total_heap)
{
    if (total_heap == 0)
    {
        return 0;  /* 总内存未知，无法计算使用率 */
    }
    
    uint32_t free_heap = SystemMonitor_GetFreeHeap();
    if (free_heap == 0)
    {
        return 0;  /* 不支持堆内存统计 */
    }
    
    if (free_heap >= total_heap)
    {
        return 0;  /* 异常情况，空闲内存大于总内存 */
    }
    
    /* 计算使用率：使用率 = (总内存 - 空闲内存) / 总内存 * 100 */
    uint32_t used_heap = total_heap - free_heap;
    return (uint8_t)((used_heap * 100) / total_heap);
}

/**
 * @brief 获取栈使用率
 */
uint8_t SystemMonitor_GetStackUsagePercent(uint32_t total_stack)
{
    if (total_stack == 0)
    {
        return 0;  /* 总栈未知，无法计算使用率 */
    }
    
    uint32_t stack_usage = SystemMonitor_GetStackUsage();
    if (stack_usage == 0)
    {
        return 0;  /* 不支持栈内存统计 */
    }
    
    if (stack_usage > total_stack)
    {
        return 100;  /* 异常情况，使用量大于总栈，返回100% */
    }
    
    /* 计算使用率：使用率 = 已使用栈 / 总栈 * 100 */
    return (uint8_t)((stack_usage * 100) / total_stack);
}

/**
 * @brief 读取异常寄存器
 */
SystemMonitor_ErrorCode_t SystemMonitor_ReadExceptionRegs(SystemMonitor_ExceptionRegs_t* regs)
{
    if (regs == NULL)
    {
        return SYSMON_ERROR_INVALID_PARAM;
    }
    
    /* 读取SCB异常寄存器 */
    regs->cfsr = SCB->CFSR;   /* Configurable Fault Status Register */
    regs->hfsr = SCB->HFSR;   /* Hard Fault Status Register */
    regs->mmfar = SCB->MMFAR; /* Memory Manage Fault Address Register */
    regs->bfar = SCB->BFAR;   /* Bus Fault Address Register */
    regs->dfsr = SCB->DFSR;   /* Debug Fault Status Register */
    
    return SYSMON_OK;
}

/**
 * @brief 清除异常寄存器
 */
SystemMonitor_ErrorCode_t SystemMonitor_ClearExceptionRegs(void)
{
    /* 清除CFSR：写入1清除对应位（写1清除） */
    SCB->CFSR = SCB->CFSR;
    
    /* 清除HFSR：写入1清除对应位（除了DEBUGEVT和FORCED） */
    /* HFSR的FORCED位（bit 30）需要写1清除，其他位保留 */
    SCB->HFSR = 0x40000000;  /* 清除FORCED位（bit 30） */
    
    /* 清除DFSR：写入1清除对应位 */
    SCB->DFSR = 0x0000000F;  /* 清除所有DFSR位（HALTED, BKPT, DWTTRAP, VCATCH） */
    
    return SYSMON_OK;
}

/**
 * @brief 初始化温度传感器（内部函数）
 * @note 初始化ADC1和温度传感器通道
 */
static void InitTemperatureSensor(void)
{
    if (g_sysmon_state.temp_sensor_initialized)
    {
        return;  /* 已经初始化 */
    }
    
    /* 使能ADC1和GPIOA时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_GPIOA, ENABLE);
    
    /* 使能温度传感器和内部参考电压 */
    ADC_TempSensorVrefintCmd(ENABLE);
    
    /* 配置ADC1 */
    ADC_InitTypeDef ADC_InitStructure;
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);
    
    /* 使能ADC1 */
    ADC_Cmd(ADC1, ENABLE);
    
    /* 校准ADC1 */
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1));
    
    g_sysmon_state.temp_sensor_initialized = 1;
}

/**
 * @brief 获取内部温度传感器温度
 * @return 温度值（°C），如果不可用返回-128
 */
int16_t SystemMonitor_GetTemperature(void)
{
    if (!g_sysmon_state.is_initialized)
    {
        return -128;  /* 模块未初始化 */
    }
    
    /* 如果温度传感器未初始化，自动初始化 */
    if (!g_sysmon_state.temp_sensor_initialized)
    {
        InitTemperatureSensor();
    }
    
    /* 配置ADC通道16（温度传感器） */
    ADC_RegularChannelConfig(ADC1, ADC_Channel_16, 1, ADC_SampleTime_239Cycles5);
    
    /* 启动转换 */
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    
    /* 等待转换完成 */
    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
    
    /* 读取ADC值 */
    uint16_t adc_value = ADC_GetConversionValue(ADC1);
    
    /* 停止转换 */
    ADC_SoftwareStartConvCmd(ADC1, DISABLE);
    
    /* 计算温度 */
    /* STM32F10x温度传感器特性：
     * - V25 = 1.43V（25°C时的电压）
     * - Avg_Slope = 4.3mV/°C
     * - 温度计算公式：T(°C) = (V25 - VSENSE) / Avg_Slope + 25
     * - VSENSE = (ADC值 * VREF) / 4096
     * - 假设VREF = 3.3V（实际应该读取VREFINT，这里简化处理）
     */
    /* 简化计算：使用固定参考电压3.3V */
    /* VSENSE = adc_value * 3300 / 4096 (mV) */
    /* T = (1430 - VSENSE) / 4.3 + 25 */
    uint32_t vsense_mv = (adc_value * 3300) / 4096;
    int32_t temperature = ((1430 - (int32_t)vsense_mv) * 10) / 43 + 250;  /* 乘以10避免浮点，最后除以10 */
    
    /* 转换为int16_t（单位：°C） */
    int16_t temp_c = (int16_t)(temperature / 10);
    
    /* 限制温度范围（-40°C ~ 125°C） */
    if (temp_c < -40)
    {
        temp_c = -40;
    }
    else if (temp_c > 125)
    {
        temp_c = 125;
    }
    
    return temp_c;
}

#endif /* CONFIG_MODULE_SYSTEM_MONITOR_ENABLED */

