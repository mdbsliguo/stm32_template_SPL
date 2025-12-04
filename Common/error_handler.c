/**
 * @file error_handler.c
 * @brief 统一错误处理模块实现
 * @version 3.0.0
 * @date 2024-01-01
 * @details 提供错误码转换、错误回调、错误日志等功能
 * 
 * @note 配置管理：
 * - 模块开关：通过 CONFIG_MODULE_ERROR_HANDLER_ENABLED 控制
 * - 统计功能：通过 CONFIG_ERROR_HANDLER_STATS_EN 控制（需定义 ERROR_HANDLER_ENABLE_STATS）
 * 
 * @note 已完善：
 * - ? 动态注册表：支持所有模块，自动注册
 * - ? FreeRTOS支持：条件编译，自动检测FreeRTOS环境
 * - ? 线程安全：FreeRTOS环境下自动使用临界区保护
 */

#include "error_handler.h"
#include "error_code.h"
#include <string.h>

/* 系统配置（如果存在） */
/* 注意：config.h 位于 system/ 目录，项目应该配置了 system/ 目录到包含路径 */
#include "config.h"

#ifdef LOG_H
#include "log.h"
#endif

/* 模块开关检查 */
#ifndef CONFIG_MODULE_ERROR_HANDLER_ENABLED
#define CONFIG_MODULE_ERROR_HANDLER_ENABLED 1  /* 默认启用 */
#endif

#if !CONFIG_MODULE_ERROR_HANDLER_ENABLED
#error "Error handler module is disabled in config.h"
#endif

/* 错误统计功能开关（从config.h映射） */
#ifdef CONFIG_ERROR_HANDLER_STATS_EN
#if CONFIG_ERROR_HANDLER_STATS_EN
#define ERROR_HANDLER_ENABLE_STATS
#endif
#endif

/* 错误回调函数指针 */
static error_callback_t g_error_callback = NULL;

/* 错误码范围检查宏（简化代码） */
#define ERROR_IN_RANGE(code, base) \
    ((code) <= (base) && (code) >= ((base) - 99))

/* FreeRTOS支持检测 */
#ifdef FREERTOS_H
#include "FreeRTOS.h"
#include "task.h"
#define ERR_LOCK()   taskENTER_CRITICAL()
#define ERR_UNLOCK() taskEXIT_CRITICAL()
#else
/* 单线程环境：空操作 */
#define ERR_LOCK()   ((void)0)
#define ERR_UNLOCK() ((void)0)
#endif

/* 错误统计（可选功能） */
#ifdef ERROR_HANDLER_ENABLE_STATS

/**
 * @brief 错误模块统计结构体（动态注册表）
 */
typedef struct {
    const char* name;      /**< 模块名称 */
    error_code_t base;    /**< 错误码基值 */
    uint32_t count;       /**< 错误计数 */
} ErrorModule_t;

/* 动态注册表：支持最多32个模块 */
#define ERROR_MODULE_MAX_COUNT 32
static ErrorModule_t g_error_modules[ERROR_MODULE_MAX_COUNT] = {0};
static uint8_t g_error_module_count = 0;  /**< 已注册模块数量 */
static uint32_t g_error_count = 0;        /**< 错误总数 */

/**
 * @brief 注册错误模块（内部函数，自动调用）
 * @param[in] name 模块名称
 * @param[in] base 错误码基值
 * @return 0=成功，1=失败（注册表已满）
 */
static uint8_t ErrorModule_Register(const char* name, error_code_t base)
{
    if (g_error_module_count >= ERROR_MODULE_MAX_COUNT)
    {
        return 1;  /* 注册表已满 */
    }
    
    g_error_modules[g_error_module_count].name = name;
    g_error_modules[g_error_module_count].base = base;
    g_error_modules[g_error_module_count].count = 0;
    g_error_module_count++;
    
    return 0;
}

/**
 * @brief 初始化错误模块注册表（自动注册所有模块）
 */
static void ErrorModule_Init(void)
{
    /* 自动注册所有已定义的错误码基值 */
    ErrorModule_Register("OLED", ERROR_BASE_OLED);
    ErrorModule_Register("SYSTICK", ERROR_BASE_SYSTICK);
    ErrorModule_Register("GPIO", ERROR_BASE_GPIO);
    ErrorModule_Register("LED", ERROR_BASE_LED);
    ErrorModule_Register("SYSTEM", ERROR_BASE_SYSTEM);
    ErrorModule_Register("CLOCK_MANAGER", ERROR_BASE_CLOCK_MANAGER);
    ErrorModule_Register("DELAY", ERROR_BASE_DELAY);
    ErrorModule_Register("BASE_TIMER", ERROR_BASE_BASE_TIMER);
    ErrorModule_Register("UART", ERROR_BASE_UART);
    ErrorModule_Register("I2C", ERROR_BASE_I2C);
    ErrorModule_Register("TIMER", ERROR_BASE_TIMER);
    ErrorModule_Register("ADC", ERROR_BASE_ADC);
    ErrorModule_Register("LOG", ERROR_BASE_LOG);
    ErrorModule_Register("IWDG", ERROR_BASE_IWDG);
    ErrorModule_Register("MODULE_CTRL", ERROR_BASE_MODULE_CTRL);
    ErrorModule_Register("SYSTEM_MONITOR", ERROR_BASE_SYSTEM_MONITOR);
    ErrorModule_Register("DS3231", ERROR_BASE_DS3231);
    ErrorModule_Register("SOFT_I2C", ERROR_BASE_SOFT_I2C);
    ErrorModule_Register("MODBUS_RTU", ERROR_BASE_MODBUS_RTU);
    /* 可以继续添加其他模块 */
}

/**
 * @brief 根据错误码查找对应的模块索引
 * @param[in] error_code 错误码
 * @return 模块索引，-1表示未找到
 */
static int8_t ErrorModule_FindIndex(error_code_t error_code)
{
    uint8_t i;
    for (i = 0; i < g_error_module_count; i++)
    {
        if (ERROR_IN_RANGE(error_code, g_error_modules[i].base))
        {
            return i;
        }
    }
    return -1;  /* 未找到 */
}

#endif /* ERROR_HANDLER_ENABLE_STATS */

/**
 * @brief 注册错误回调函数
 * @param[in] callback 回调函数指针，NULL表示取消注册
 * @return 错误码
 */
error_code_t ErrorHandler_RegisterCallback(error_callback_t callback)
{
    ERR_LOCK();
    g_error_callback = callback;
    ERR_UNLOCK();
    return ERROR_OK;
}

/**
 * @brief 初始化错误处理模块（内部调用，自动初始化）
 * @note 在首次调用ErrorHandler_Handle时自动初始化
 */
static void ErrorHandler_InitInternal(void)
{
    static uint8_t initialized = 0;
    if (initialized)
    {
        return;
    }
    
#ifdef ERROR_HANDLER_ENABLE_STATS
    ErrorModule_Init();
#endif
    
    initialized = 1;
}

/**
 * @brief 处理错误（记录日志并调用回调）
 * @param[in] error_code 错误码
 * @param[in] module_name 模块名称（可选，可为NULL）
 * @return 错误码（原样返回）
 */
error_code_t ErrorHandler_Handle(error_code_t error_code, const char* module_name)
{
    /* 参数校验 */
    if (error_code == ERROR_OK)
    {
        return ERROR_OK;
    }
    
    /* 自动初始化（首次调用时） */
    ErrorHandler_InitInternal();
    
    /* 记录错误日志（如果日志系统可用） */
#ifdef LOG_H
    const char* err_str = ErrorHandler_GetString(error_code);
    if (module_name != NULL)
    {
        LOG_ERROR("ERROR", "[%s] %s (code: %d)", module_name, err_str, error_code);
    }
    else
    {
        LOG_ERROR("ERROR", "%s (code: %d)", err_str, error_code);
    }
#endif
    
    /* 错误统计（如果启用） */
#ifdef ERROR_HANDLER_ENABLE_STATS
    ERR_LOCK();  /* 线程安全保护（FreeRTOS环境下自动使用临界区） */
    g_error_count++;
    
    /* 使用动态注册表查找并更新模块错误计数 */
    int8_t module_index = ErrorModule_FindIndex(error_code);
    if (module_index >= 0)
    {
        g_error_modules[module_index].count++;
    }
    /* 如果未找到对应模块，说明是新模块或错误码范围异常，忽略统计 */
    
    ERR_UNLOCK();  /* 线程安全保护 */
#endif
    
    /* 调用回调函数 */
    ERR_LOCK();  /* 保护回调函数指针 */
    if (g_error_callback != NULL)
    {
        error_callback_t callback = g_error_callback;  /* 保存指针，避免在回调中修改 */
        ERR_UNLOCK();
        callback(error_code, module_name);  /* 在锁外调用回调，避免死锁 */
    }
    else
    {
        ERR_UNLOCK();
    }
    
    return error_code;
}

/**
 * @brief 将错误码转换为字符串
 * @param[in] error_code 错误码
 * @return 错误描述字符串
 */
const char* ErrorHandler_GetString(error_code_t error_code)
{
    if (error_code == ERROR_OK)
    {
        return "OK";
    }
    
    /* OLED模块错误码 */
    /* 错误码范围：[ERROR_BASE_OLED - 99, ERROR_BASE_OLED]，即 [-199, -100] */
    if (ERROR_IN_RANGE(error_code, ERROR_BASE_OLED))
    {
        switch (error_code)
        {
            case ERROR_BASE_OLED - 1: return "OLED: Not initialized";
            case ERROR_BASE_OLED - 2: return "OLED: Invalid parameter";
            case ERROR_BASE_OLED - 3: return "OLED: GPIO operation failed";
            default: return "OLED: Unknown error";
        }
    }
    /* SYSTICK模块错误码 */
    /* 错误码范围：[ERROR_BASE_SYSTICK - 99, ERROR_BASE_SYSTICK]，即 [-299, -200] */
    else if (ERROR_IN_RANGE(error_code, ERROR_BASE_SYSTICK))
    {
        switch (error_code)
        {
            case ERROR_BASE_SYSTICK - 1: return "SysTick: Not initialized";
            case ERROR_BASE_SYSTICK - 2: return "SysTick: Invalid parameter";
            case ERROR_BASE_SYSTICK - 3: return "SysTick: Timeout overflow";
            default: return "SysTick: Unknown error";
        }
    }
    /* GPIO模块错误码 */
    /* 错误码范围：[ERROR_BASE_GPIO - 99, ERROR_BASE_GPIO]，即 [-399, -300] */
    else if (ERROR_IN_RANGE(error_code, ERROR_BASE_GPIO))
    {
        switch (error_code)
        {
            case ERROR_BASE_GPIO - 1: return "GPIO: Null pointer";
            case ERROR_BASE_GPIO - 2: return "GPIO: Invalid port";
            case ERROR_BASE_GPIO - 3: return "GPIO: Invalid pin";
            case ERROR_BASE_GPIO - 4: return "GPIO: Invalid mode";
            default: return "GPIO: Unknown error";
        }
    }
    /* LED模块错误码 */
    /* 错误码范围：[ERROR_BASE_LED - 99, ERROR_BASE_LED]，即 [-499, -400] */
    else if (ERROR_IN_RANGE(error_code, ERROR_BASE_LED))
    {
        switch (error_code)
        {
            case ERROR_BASE_LED - 1: return "LED: Invalid ID";
            case ERROR_BASE_LED - 2: return "LED: Disabled";
            case ERROR_BASE_LED - 3: return "LED: Null pointer";
            case ERROR_BASE_LED - 4: return "LED: Init failed";
            default: return "LED: Unknown error";
        }
    }
    /* 系统初始化模块错误码（包括clock_manager的历史错误码和异常错误码） */
    /* 错误码范围：[ERROR_BASE_SYSTEM - 99, ERROR_BASE_SYSTEM]，即 [-599, -500] */
    else if (ERROR_IN_RANGE(error_code, ERROR_BASE_SYSTEM))
    {
        switch (error_code)
        {
            /* 系统初始化错误码 */
            case ERROR_BASE_SYSTEM - 1: return "System: Clock init failed";
            case ERROR_BASE_SYSTEM - 2: return "System: SysTick init failed";
            case ERROR_BASE_SYSTEM - 3: return "System: BSP init failed";
            case ERROR_BASE_SYSTEM - 4: return "System: Driver init failed";
            
            /* 异常错误码 */
            case ERROR_BASE_SYSTEM - 20: return "Exception: HardFault";
            case ERROR_BASE_SYSTEM - 21: return "Exception: Memory Manage Fault";
            case ERROR_BASE_SYSTEM - 22: return "Exception: Bus Fault";
            case ERROR_BASE_SYSTEM - 23: return "Exception: Usage Fault";
            
            /* clock_manager模块错误码（历史原因，使用ERROR_BASE_SYSTEM） */
            case ERROR_BASE_SYSTEM - 10: return "CLKM: Not initialized";
            case ERROR_BASE_SYSTEM - 11: return "CLKM: Invalid frequency";
            case ERROR_BASE_SYSTEM - 12: return "CLKM: PLL lock timeout";
            case ERROR_BASE_SYSTEM - 13: return "CLKM: Switch too fast";
            case ERROR_BASE_SYSTEM - 14: return "CLKM: HSE not found";
            case ERROR_BASE_SYSTEM - 15: return "CLKM: Mode conflict";
            default: return "System: Unknown error";
        }
    }
    /* 时钟管理模块错误码 */
    /* 错误码范围：[ERROR_BASE_CLOCK_MANAGER - 99, ERROR_BASE_CLOCK_MANAGER]，即 [-699, -600] */
    else if (ERROR_IN_RANGE(error_code, ERROR_BASE_CLOCK_MANAGER))
    {
        switch (error_code)
        {
            case ERROR_BASE_CLOCK_MANAGER - 1: return "CLKM: Not initialized";
            case ERROR_BASE_CLOCK_MANAGER - 2: return "CLKM: Invalid frequency";
            case ERROR_BASE_CLOCK_MANAGER - 3: return "CLKM: PLL lock timeout";
            case ERROR_BASE_CLOCK_MANAGER - 4: return "CLKM: Switch too fast";
            case ERROR_BASE_CLOCK_MANAGER - 5: return "CLKM: HSE not found";
            case ERROR_BASE_CLOCK_MANAGER - 6: return "CLKM: Mode conflict";
            default: return "CLKM: Unknown error";
        }
    }
    /* 延时模块错误码 */
    /* 错误码范围：[ERROR_BASE_DELAY - 99, ERROR_BASE_DELAY]，即 [-799, -700] */
    else if (ERROR_IN_RANGE(error_code, ERROR_BASE_DELAY))
    {
        switch (error_code)
        {
            case ERROR_BASE_DELAY - 1: return "Delay: Not initialized";
            case ERROR_BASE_DELAY - 2: return "Delay: Invalid parameter";
            case ERROR_BASE_DELAY - 3: return "Delay: Timeout overflow";
            case ERROR_BASE_DELAY - 4: return "Delay: Base timer not initialized";
            default: return "Delay: Unknown error";
        }
    }
    /* 基时定时器模块错误码 */
    /* 错误码范围：[ERROR_BASE_BASE_TIMER - 99, ERROR_BASE_BASE_TIMER]，即 [-899, -800] */
    else if (ERROR_IN_RANGE(error_code, ERROR_BASE_BASE_TIMER))
    {
        switch (error_code)
        {
            case ERROR_BASE_BASE_TIMER - 1: return "TIM2_TimeBase: Not initialized";
            case ERROR_BASE_BASE_TIMER - 2: return "TIM2_TimeBase: Invalid parameter";
            case ERROR_BASE_BASE_TIMER - 3: return "TIM2_TimeBase: Calculation failed";
            case ERROR_BASE_BASE_TIMER - 4: return "TIM2_TimeBase: Reconfig failed";
            default: return "TIM2_TimeBase: Unknown error";
        }
    }
    /* 日志模块错误码 */
    /* 错误码范围：[ERROR_BASE_LOG - 99, ERROR_BASE_LOG]，即 [-1299, -1200] */
    /* UART妯″潡閿欒??鐮?*/
    /* 閿欒??鐮佽寖鍥达細[ERROR_BASE_UART - 99, ERROR_BASE_UART]锛屽嵆 [-999, -900] */
    else if (ERROR_IN_RANGE(error_code, ERROR_BASE_UART))
    {
        switch (error_code)
        {
            case ERROR_BASE_UART - 1: return "UART: Null pointer";
            case ERROR_BASE_UART - 2: return "UART: Invalid parameter";
            case ERROR_BASE_UART - 3: return "UART: Invalid instance";
            case ERROR_BASE_UART - 4: return "UART: Invalid peripheral";
            case ERROR_BASE_UART - 5: return "UART: Not initialized";
            case ERROR_BASE_UART - 6: return "UART: GPIO operation failed";
            case ERROR_BASE_UART - 7: return "UART: Timeout";
            case ERROR_BASE_UART - 8: return "UART: Busy";
            case ERROR_BASE_UART - 9: return "UART: Interrupt not enabled";
            case ERROR_BASE_UART - 10: return "UART: Overrun error";
            case ERROR_BASE_UART - 11: return "UART: Noise error";
            case ERROR_BASE_UART - 12: return "UART: Framing error";
            case ERROR_BASE_UART - 13: return "UART: Parity error";
            default: return "UART: Unknown error";
        }
    }
    else if (ERROR_IN_RANGE(error_code, ERROR_BASE_LOG))
    {
        switch (error_code)
        {
            case ERROR_BASE_LOG - 1: return "LOG: Not initialized";
            case ERROR_BASE_LOG - 2: return "LOG: Invalid parameter";
            case ERROR_BASE_LOG - 3: return "LOG: Buffer overflow";
            case ERROR_BASE_LOG - 4: return "LOG: Debug not ready";
            default: return "LOG: Unknown error";
        }
    }
    /* 独立看门狗模块错误码 */
    /* 错误码范围：[ERROR_BASE_IWDG - 99, ERROR_BASE_IWDG]，即 [-1399, -1300] */
    else if (ERROR_IN_RANGE(error_code, ERROR_BASE_IWDG))
    {
        switch (error_code)
        {
            case ERROR_BASE_IWDG - 1: return "IWDG: Not initialized";
            case ERROR_BASE_IWDG - 2: return "IWDG: Invalid parameter";
            case ERROR_BASE_IWDG - 3: return "IWDG: Timeout too short";
            case ERROR_BASE_IWDG - 4: return "IWDG: Timeout too long";
            case ERROR_BASE_IWDG - 5: return "IWDG: Already enabled";
            case ERROR_BASE_IWDG - 6: return "IWDG: Config failed";
            default: return "IWDG: Unknown error";
        }
    }
    /* 模块开关总控错误码 */
    /* 错误码范围：[ERROR_BASE_MODULE_CTRL - 99, ERROR_BASE_MODULE_CTRL]，即 [-1499, -1400] */
    else if (ERROR_IN_RANGE(error_code, ERROR_BASE_MODULE_CTRL))
    {
        switch (error_code)
        {
            case ERROR_BASE_MODULE_CTRL - 1: return "MODCTRL: Not initialized";
            case ERROR_BASE_MODULE_CTRL - 2: return "MODCTRL: Invalid module ID";
            case ERROR_BASE_MODULE_CTRL - 3: return "MODCTRL: Dependency not met";
            case ERROR_BASE_MODULE_CTRL - 4: return "MODCTRL: Already initialized";
            default: return "MODCTRL: Unknown error";
        }
    }
    /* 系统监控模块错误码 */
    /* 错误码范围：[ERROR_BASE_SYSTEM_MONITOR - 99, ERROR_BASE_SYSTEM_MONITOR]，即 [-1599, -1500] */
    else if (ERROR_IN_RANGE(error_code, ERROR_BASE_SYSTEM_MONITOR))
    {
        switch (error_code)
        {
            case ERROR_BASE_SYSTEM_MONITOR - 1: return "SYSMON: Not initialized";
            case ERROR_BASE_SYSTEM_MONITOR - 2: return "SYSMON: Invalid parameter";
            default: return "SYSMON: Unknown error";
        }
    }
    /* ModBusRTU模块错误码 */
    /* 错误码范围：[ERROR_BASE_MODBUS_RTU - 99, ERROR_BASE_MODBUS_RTU]，即 [-4199, -4100] */
    else if (ERROR_IN_RANGE(error_code, ERROR_BASE_MODBUS_RTU))
    {
        switch (error_code)
        {
            case ERROR_BASE_MODBUS_RTU - 1: return "ModBusRTU: Null pointer";
            case ERROR_BASE_MODBUS_RTU - 2: return "ModBusRTU: Invalid parameter";
            case ERROR_BASE_MODBUS_RTU - 3: return "ModBusRTU: Invalid instance";
            case ERROR_BASE_MODBUS_RTU - 4: return "ModBusRTU: Not initialized";
            case ERROR_BASE_MODBUS_RTU - 5: return "ModBusRTU: Timeout";
            case ERROR_BASE_MODBUS_RTU - 6: return "ModBusRTU: CRC error";
            case ERROR_BASE_MODBUS_RTU - 7: return "ModBusRTU: Invalid response";
            case ERROR_BASE_MODBUS_RTU - 8: return "ModBusRTU: Invalid address";
            case ERROR_BASE_MODBUS_RTU - 9: return "ModBusRTU: Invalid function code";
            case ERROR_BASE_MODBUS_RTU - 10: return "ModBusRTU: Exception";
            default: return "ModBusRTU: Unknown error";
        }
    }
    
    return "Unknown error";
}

/**
 * @brief 检查错误码并处理
 * @param[in] error_code 错误码
 * @param[in] module_name 模块名称
 * @return true-有错误，false-无错误
 */
uint8_t ErrorHandler_Check(error_code_t error_code, const char* module_name)
{
    if (error_code != ERROR_OK)
    {
        ErrorHandler_Handle(error_code, module_name);
        return 1;
    }
    return 0;
}

#ifdef ERROR_HANDLER_ENABLE_STATS
/**
 * @brief 获取错误统计总数
 * @return 错误总数
 * @note 线程安全：FreeRTOS环境下自动使用临界区保护
 */
uint32_t ErrorHandler_GetErrorCount(void)
{
    ERR_LOCK();
    uint32_t count = g_error_count;
    ERR_UNLOCK();
    return count;
}

/**
 * @brief 重置错误统计
 * @note 线程安全：FreeRTOS环境下自动使用临界区保护
 */
void ErrorHandler_ResetStats(void)
{
    uint8_t i;
    
    ERR_LOCK();
    g_error_count = 0;
    for (i = 0; i < g_error_module_count; i++)
    {
        g_error_modules[i].count = 0;
    }
    ERR_UNLOCK();
}
#endif
