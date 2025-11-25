/**
 * @file iwdg.c
 * @brief 独立看门狗管理模块实现（快速开发工程模板）
 * @version 1.0.0
 * @date 2024-01-01
 * @details 提供独立看门狗（IWDG）的初始化、喂狗、配置等功能
 * 
 * @details 实现说明：
 * - 自动计算预分频器和重装载值，根据目标超时时间选择最优配置
 * - 支持LSI自动启动和等待就绪
 * - 提供超时保护，防止配置失败导致系统死锁
 * - 启用后无法禁用，只能通过系统复位
 * 
 * @note LSI频率说明：
 * - LSI实际频率范围：30kHz~60kHz，典型值40kHz
 * - 本模块使用40kHz作为计算基准，实际超时时间可能有±25%偏差
 * - 如需精确超时，建议使用外部看门狗芯片
 */

#include "iwdg.h"
#include "error_code.h"
#include "error_handler.h"
#include "stm32f10x.h"
#include "stm32f10x_iwdg.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <config.h>

/* 模块开关检查 */
#ifndef CONFIG_MODULE_IWDG_ENABLED
#define CONFIG_MODULE_IWDG_ENABLED 1  /* 默认启用 */
#endif

#if !CONFIG_MODULE_IWDG_ENABLED
/* 如果看门狗模块被禁用，所有函数都为空操作 */
#else

/* LSI频率（Hz），实际范围30-60kHz，使用典型值40kHz */
#define IWDG_LSI_FREQ_HZ    40000U

/* 超时时间范围 */
#define IWDG_TIMEOUT_MIN_MS           1       /**< 最小超时时间（毫秒） */
#define IWDG_TIMEOUT_MAX_MS           32768   /**< 最大超时时间（毫秒） */
#define IWDG_DEFAULT_TIMEOUT_MS       1000    /**< 默认超时时间（毫秒） */

/* 预分频器和重装载值范围 */
#define IWDG_PRESCALER_MAX            6       /**< 最大预分频器索引（0-6） */
#define IWDG_RELOAD_MAX               4095    /**< 最大重装载值（0-4095） */

/* 等待超时（循环次数） */
#define IWDG_LSI_WAIT_TIMEOUT         100000  /**< LSI等待超时（循环次数，约100ms @ 1MHz） */
#define IWDG_FLAG_WAIT_TIMEOUT        1000    /**< 标志等待超时（循环次数） */

/* 预分频器值表（对应PR值0-6） */
static const uint16_t g_prescaler_values[] = {4, 8, 16, 32, 64, 128, 256};

/* 模块状态 */
static struct {
    bool is_initialized;      /**< 是否已初始化 */
    bool is_enabled;          /**< 是否已启用 */
    uint32_t timeout_ms;      /**< 当前超时时间（毫秒） */
    uint8_t prescaler;        /**< 当前预分频器（0-6） */
    uint16_t reload;          /**< 当前重装载值（0-4095） */
} g_iwdg_state = {0};

/* 内部辅助函数声明 */
static IWDG_Status_t IWDG_WaitFlagClear(uint16_t flag, uint32_t timeout);

/**
 * @brief 计算看门狗超时时间（辅助函数）
 * @param[in] prescaler 预分频器（0-6）
 * @param[in] reload 重装载值（0-4095）
 * @return 超时时间（毫秒），基于LSI=40kHz计算
 */
uint32_t IWDG_CalculateTimeout(uint8_t prescaler, uint16_t reload)
{
    if (prescaler > IWDG_PRESCALER_MAX || reload > IWDG_RELOAD_MAX)
    {
        return 0;
    }
    
    /* 超时时间 = (4 * 2^PR) * (RLR + 1) / LSI_freq */
    /* 转换为毫秒：* 1000 / LSI_freq */
    /* 注意：检查溢出风险 */
    /* 最大值：prescaler=256, reload=4095, 计算 256 * 4 * 4096 * 1000 = 4,194,304,000 */
    /* 这小于 uint32_t 最大值 4,294,967,295，不会溢出 */
    uint32_t prescaler_value = g_prescaler_values[prescaler];
    uint32_t timeout_ms = (prescaler_value * 4 * (reload + 1) * 1000) / IWDG_LSI_FREQ_HZ;
    
    return timeout_ms;
}

/**
 * @brief 根据超时时间计算预分频器和重装载值（辅助函数）
 * @param[in] timeout_ms 目标超时时间（毫秒）
 * @param[out] prescaler 计算得到的预分频器（0-6）
 * @param[out] reload 计算得到的重装载值（0-4095）
 * @return IWDG_Status_t 错误码
 */
IWDG_Status_t IWDG_CalculateParams(uint32_t timeout_ms, uint8_t* prescaler, uint16_t* reload)
{
    /* 参数校验 */
    if (prescaler == NULL || reload == NULL)
    {
        return IWDG_ERROR_INVALID_PARAM;
    }
    
    if (timeout_ms < IWDG_TIMEOUT_MIN_MS || timeout_ms > IWDG_TIMEOUT_MAX_MS)
    {
        return IWDG_ERROR_INVALID_PARAM;
    }
    
    /* 尝试不同的预分频器，找到最合适的组合 */
    uint8_t best_prescaler = 0;
    uint16_t best_reload = 0;
    uint32_t best_error = 0xFFFFFFFF;
    
    for (uint8_t pr = 0; pr <= 6; pr++)
    {
        uint32_t prescaler_value = g_prescaler_values[pr];
        /* 计算重装载值：RLR = (timeout_ms * LSI_freq) / (prescaler_value * 4 * 1000) - 1 */
        /* 注意：需要检查整数溢出 */
        uint32_t denominator = prescaler_value * 4 * 1000;
        
        /* 检查溢出：先检查乘法是否会溢出 */
        /* timeout_ms 最大 32768，IWDG_LSI_FREQ_HZ = 40000 */
        /* 32768 * 40000 = 1,310,720,000 < 2^31，不会溢出 */
        uint32_t numerator = timeout_ms * IWDG_LSI_FREQ_HZ;
        
        /* 检查除法结果：如果 numerator < denominator，结果会为0，跳过 */
        if (numerator < denominator)
        {
            continue;  /* 这个预分频器无法达到目标超时时间 */
        }
        
        uint32_t calculated_reload = numerator / denominator;
        
        /* 计算 RLR = calculated_reload - 1 */
        /* 注意：calculated_reload 必须 > 0，否则 RLR 无效（RLR 最小为0，对应 calculated_reload=1） */
        if (calculated_reload == 0)
        {
            continue;  /* 这个预分频器无法达到目标超时时间 */
        }
        
        uint16_t reload_value = (uint16_t)(calculated_reload - 1);
        
        /* 检查重装载值是否在有效范围内 */
        if (reload_value <= IWDG_RELOAD_MAX)
        {
            /* 计算实际超时时间 */
            uint32_t actual_timeout = IWDG_CalculateTimeout(pr, reload_value);
            uint32_t error = (actual_timeout > timeout_ms) ? (actual_timeout - timeout_ms) : (timeout_ms - actual_timeout);
            
            if (error < best_error)
            {
                best_error = error;
                best_prescaler = pr;
                best_reload = reload_value;
                
                /* 如果找到完美匹配（error=0），提前退出 */
                if (error == 0)
                {
                    break;
                }
            }
        }
    }
    
    if (best_error == 0xFFFFFFFF)
    {
        /* 所有预分频器都无法达到目标超时时间 */
        /* 可能是超时时间太短（小于最小可配置值）或太长（超过最大可配置值） */
        /* 检查是否是超时时间太长 */
        /* 最大超时时间：PR=6(256), RLR=4095 */
        /* timeout = 256 * 4 * 4096 * 1000 / 40000 = 104857600 ms ≈ 104857秒 ≈ 29小时 */
        /* 但实际最大超时时间约为32秒（32768ms），所以如果timeout_ms > IWDG_TIMEOUT_MAX_MS，应该是超时时间太长 */
        if (timeout_ms > IWDG_TIMEOUT_MAX_MS)
        {
            return IWDG_ERROR_TIMEOUT_TOO_LONG;
        }
        else
        {
            return IWDG_ERROR_TIMEOUT_TOO_SHORT;
        }
    }
    
    *prescaler = best_prescaler;
    *reload = best_reload;
    
    return IWDG_OK;
}

/**
 * @brief 等待IWDG标志位清除（内部辅助函数）
 * @param[in] flag 标志位（IWDG_FLAG_PVU 或 IWDG_FLAG_RVU）
 * @param[in] timeout 超时时间（循环次数）
 * @return IWDG_Status_t 错误码
 */
static IWDG_Status_t IWDG_WaitFlagClear(uint16_t flag, uint32_t timeout)
{
    while (IWDG_GetFlagStatus(flag) == SET && timeout > 0)
    {
        timeout--;
        __NOP();  /* 防止编译器优化 */
    }
    if (timeout == 0)
    {
        return IWDG_ERROR_CONFIG_FAILED;
    }
    return IWDG_OK;
}

/**
 * @brief 初始化看门狗模块
 * @param[in] config 看门狗配置（可为NULL，使用默认配置）
 * @return IWDG_Status_t 错误码
 */
IWDG_Status_t IWDG_Init(const iwdg_config_t* config)
{
    /* 如果已初始化且看门狗已启用，不允许重新初始化 */
    if (g_iwdg_state.is_initialized)
    {
        if (g_iwdg_state.is_enabled)
        {
            /* 看门狗已启用，无法重新初始化 */
            return IWDG_ERROR_ALREADY_ENABLED;
        }
        /* 已初始化但未启用，允许重新初始化（更新配置） */
        /* 注意：这允许用户在启用前修改配置 */
    }
    
    uint32_t timeout_ms = IWDG_DEFAULT_TIMEOUT_MS;  /* 默认1秒 */
    uint8_t prescaler = 0;
    uint16_t reload = 0;
    
    /* 从config.h读取默认配置（如果存在） */
#ifdef CONFIG_IWDG_TIMEOUT_MS
    timeout_ms = CONFIG_IWDG_TIMEOUT_MS;
#endif
    
    /* 如果提供了配置，使用配置值 */
    if (config != NULL)
    {
        if (config->timeout_ms < IWDG_TIMEOUT_MIN_MS || config->timeout_ms > IWDG_TIMEOUT_MAX_MS)
        {
            ErrorHandler_Handle(IWDG_ERROR_INVALID_PARAM, "IWDG");
            return IWDG_ERROR_INVALID_PARAM;
        }
        timeout_ms = config->timeout_ms;
        
        /* 如果提供了预分频器和重装载值，直接使用 */
        /* 注意：只有当 prescaler 和 reload 都 > 0 时，才使用用户提供的配置 */
        /* prescaler=0 是有效的（对应4分频），但这里0表示自动计算 */
        if (config->prescaler > 0 && config->reload > 0)
        {
            if (config->prescaler > IWDG_PRESCALER_MAX)
            {
                ErrorHandler_Handle(IWDG_ERROR_INVALID_PARAM, "IWDG");
                return IWDG_ERROR_INVALID_PARAM;
            }
            if (config->reload > IWDG_RELOAD_MAX)
            {
                ErrorHandler_Handle(IWDG_ERROR_INVALID_PARAM, "IWDG");
                return IWDG_ERROR_INVALID_PARAM;
            }
            prescaler = config->prescaler;
            reload = config->reload;
        }
        else
        {
            /* 自动计算预分频器和重装载值 */
            IWDG_Status_t calc_status = IWDG_CalculateParams(timeout_ms, &prescaler, &reload);
            if (calc_status != IWDG_OK)
            {
                ErrorHandler_Handle(calc_status, "IWDG");
                return calc_status;
            }
        }
    }
    else
    {
        /* 使用默认配置，自动计算参数 */
        IWDG_Status_t calc_status = IWDG_CalculateParams(timeout_ms, &prescaler, &reload);
        if (calc_status != IWDG_OK)
        {
            ErrorHandler_Handle(calc_status, "IWDG");
            return calc_status;
        }
    }
    
    /* 保存配置 */
    g_iwdg_state.timeout_ms = timeout_ms;
    g_iwdg_state.prescaler = prescaler;
    g_iwdg_state.reload = reload;
    g_iwdg_state.is_initialized = true;
    g_iwdg_state.is_enabled = false;
    
    return IWDG_OK;
}

/**
 * @brief 反初始化看门狗模块
 * @return IWDG_Status_t 错误码
 */
IWDG_Status_t IWDG_Deinit(void)
{
    if (!g_iwdg_state.is_initialized)
    {
        return IWDG_ERROR_NOT_INITIALIZED;
    }
    
    /* 注意：一旦启用看门狗，只能通过系统复位禁用 */
    /* 此函数仅清除模块状态，不会禁用硬件看门狗 */
    g_iwdg_state.is_initialized = false;
    g_iwdg_state.is_enabled = false;
    g_iwdg_state.timeout_ms = 0;
    g_iwdg_state.prescaler = 0;
    g_iwdg_state.reload = 0;
    
    return IWDG_OK;
}

/**
 * @brief 启用看门狗
 * @return IWDG_Status_t 错误码
 * @warning 一旦启用，只能通过系统复位禁用！
 */
IWDG_Status_t IWDG_Start(void)
{
    if (!g_iwdg_state.is_initialized)
    {
        return IWDG_ERROR_NOT_INITIALIZED;
    }
    
    if (g_iwdg_state.is_enabled)
    {
        return IWDG_ERROR_ALREADY_ENABLED;
    }
    
    /* 使能LSI时钟（如果未使能） */
    if (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET)
    {
        RCC_LSICmd(ENABLE);
        /* 等待LSI就绪（最多等待100ms，约100000个循环，假设CPU频率约1MHz） */
        /* 注意：这是一个粗略的等待，实际等待时间取决于CPU频率 */
        uint32_t timeout = IWDG_LSI_WAIT_TIMEOUT;
        while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET && timeout > 0)
        {
            timeout--;
            /* 添加空操作，防止编译器优化掉循环 */
            __NOP();
        }
        if (timeout == 0)
        {
            ErrorHandler_Handle(IWDG_ERROR_CONFIG_FAILED, "IWDG");
            return IWDG_ERROR_CONFIG_FAILED;
        }
    }
    
    /* 使能IWDG写访问 */
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    
    /* 配置预分频器 */
    IWDG_SetPrescaler(g_iwdg_state.prescaler);
    
    /* 等待预分频器更新完成（添加超时保护） */
    IWDG_Status_t pvu_status = IWDG_WaitFlagClear(IWDG_FLAG_PVU, IWDG_FLAG_WAIT_TIMEOUT);
    if (pvu_status != IWDG_OK)
    {
        ErrorHandler_Handle(pvu_status, "IWDG");
        return pvu_status;
    }
    
    /* 配置重装载值 */
    IWDG_SetReload(g_iwdg_state.reload);
    
    /* 等待重装载值更新完成（添加超时保护） */
    IWDG_Status_t rvu_status = IWDG_WaitFlagClear(IWDG_FLAG_RVU, IWDG_FLAG_WAIT_TIMEOUT);
    if (rvu_status != IWDG_OK)
    {
        ErrorHandler_Handle(rvu_status, "IWDG");
        return rvu_status;
    }
    
    /* 刷新看门狗（加载重装载值） */
    IWDG_ReloadCounter();
    
    /* 使能看门狗（调用SPL库函数） */
    /* 注意：SPL库的IWDG_Enable()函数名与我们的模块函数名相同，但这是正确的 */
    /* 在iwdg.c中，我们的IWDG_Enable()函数会调用SPL库的IWDG_Enable() */
    /* 由于SPL库函数是内联或通过头文件声明，这里直接调用即可 */
    IWDG->KR = 0xCCCC;  /* 直接写寄存器，避免函数名冲突 */
    
    /* 标记为已启用 */
    g_iwdg_state.is_enabled = true;
    
    return IWDG_OK;
}

/**
 * @brief 喂狗（刷新看门狗计数器）
 * @return IWDG_Status_t 错误码
 */
IWDG_Status_t IWDG_Refresh(void)
{
    if (!g_iwdg_state.is_initialized)
    {
        return IWDG_ERROR_NOT_INITIALIZED;
    }
    
    if (!g_iwdg_state.is_enabled)
    {
        /* 看门狗未启用，静默成功 */
        return IWDG_OK;
    }
    
    /* 刷新看门狗计数器 */
    IWDG_ReloadCounter();
    
    return IWDG_OK;
}

/**
 * @brief 配置看门狗超时时间
 * @param[in] timeout_ms 超时时间（毫秒），范围：1ms ~ 32768ms
 * @return IWDG_Status_t 错误码
 */
IWDG_Status_t IWDG_SetTimeout(uint32_t timeout_ms)
{
    if (!g_iwdg_state.is_initialized)
    {
        return IWDG_ERROR_NOT_INITIALIZED;
    }
    
    if (g_iwdg_state.is_enabled)
    {
        /* 看门狗已启用，无法修改配置 */
        ErrorHandler_Handle(IWDG_ERROR_ALREADY_ENABLED, "IWDG");
        return IWDG_ERROR_ALREADY_ENABLED;
    }
    
    if (timeout_ms < IWDG_TIMEOUT_MIN_MS || timeout_ms > IWDG_TIMEOUT_MAX_MS)
    {
        ErrorHandler_Handle(IWDG_ERROR_INVALID_PARAM, "IWDG");
        return IWDG_ERROR_INVALID_PARAM;
    }
    
    /* 计算预分频器和重装载值 */
    uint8_t prescaler = 0;
    uint16_t reload = 0;
    IWDG_Status_t calc_status = IWDG_CalculateParams(timeout_ms, &prescaler, &reload);
    if (calc_status != IWDG_OK)
    {
        ErrorHandler_Handle(calc_status, "IWDG");
        return calc_status;
    }
    
    /* 更新配置 */
    g_iwdg_state.timeout_ms = timeout_ms;
    g_iwdg_state.prescaler = prescaler;
    g_iwdg_state.reload = reload;
    
    return IWDG_OK;
}

/**
 * @brief 获取当前超时时间
 * @return 超时时间（毫秒），0表示未初始化
 */
uint32_t IWDG_GetTimeout(void)
{
    if (!g_iwdg_state.is_initialized)
    {
        return 0;
    }
    
    return g_iwdg_state.timeout_ms;
}

/**
 * @brief 检查看门狗是否已初始化
 * @return 1-已初始化，0-未初始化
 */
uint8_t IWDG_IsInitialized(void)
{
    return g_iwdg_state.is_initialized ? 1 : 0;
}

/**
 * @brief 检查看门狗是否已启用
 * @return 1-已启用，0-未启用
 */
uint8_t IWDG_IsEnabled(void)
{
    return g_iwdg_state.is_enabled ? 1 : 0;
}

#endif /* CONFIG_MODULE_IWDG_ENABLED */

