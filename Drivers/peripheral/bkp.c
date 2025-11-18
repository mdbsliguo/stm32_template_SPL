/**
 * @file bkp.c
 * @brief BKP备份寄存器模块实现
 */

#include "bkp.h"
#include "board.h"
#include "config.h"
#include "error_handler.h"
#include "stm32f10x.h"
#include "stm32f10x_bkp.h"
#include "stm32f10x_pwr.h"
#include "stm32f10x_rcc.h"
#include <stdbool.h>

#if CONFIG_MODULE_BKP_ENABLED

/* 寄存器编号到SPL库寄存器值的映射 */
static const uint16_t bkp_reg_map[] = {
    0,      /* 占位，不使用0 */
    BKP_DR1, BKP_DR2, BKP_DR3, BKP_DR4, BKP_DR5, BKP_DR6, BKP_DR7, BKP_DR8, BKP_DR9, BKP_DR10,
    BKP_DR11, BKP_DR12, BKP_DR13, BKP_DR14, BKP_DR15, BKP_DR16, BKP_DR17, BKP_DR18, BKP_DR19, BKP_DR20,
    BKP_DR21, BKP_DR22, BKP_DR23, BKP_DR24, BKP_DR25, BKP_DR26, BKP_DR27, BKP_DR28, BKP_DR29, BKP_DR30,
    BKP_DR31, BKP_DR32, BKP_DR33, BKP_DR34, BKP_DR35, BKP_DR36, BKP_DR37, BKP_DR38, BKP_DR39, BKP_DR40,
    BKP_DR41, BKP_DR42,
};

/* 初始化标志 */
static bool g_bkp_initialized = false;

/**
 * @brief BKP初始化
 */
BKP_Status_t BKP_Init(void)
{
    if (g_bkp_initialized)
    {
        ERROR_HANDLER_Report(ERROR_BASE_RTC, __FILE__, __LINE__, "BKP already initialized");
        return BKP_ERROR_ALREADY_INITIALIZED;
    }
    
    /* 使能PWR和BKP时钟 */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
    
    /* 使能备份域访问 */
    PWR_BackupAccessCmd(ENABLE);
    
    g_bkp_initialized = true;
    
    return BKP_OK;
}

/**
 * @brief BKP反初始化
 */
BKP_Status_t BKP_Deinit(void)
{
    if (!g_bkp_initialized)
    {
        return BKP_ERROR_NOT_INITIALIZED;
    }
    
    /* 禁用备份域访问 */
    PWR_BackupAccessCmd(DISABLE);
    
    g_bkp_initialized = false;
    
    return BKP_OK;
}

/**
 * @brief 写入备份寄存器
 */
BKP_Status_t BKP_WriteRegister(BKP_Register_t reg, uint16_t data)
{
    uint16_t bkp_dr;
    
    if (!g_bkp_initialized)
    {
        return BKP_ERROR_NOT_INITIALIZED;
    }
    
    if (reg < BKP_REG_DR1 || reg > BKP_REG_MAX)
    {
        return BKP_ERROR_INVALID_REGISTER;
    }
    
    /* 获取SPL库寄存器值 */
    bkp_dr = bkp_reg_map[reg];
    
    /* 写入备份寄存器 */
    BKP_WriteBackupRegister(bkp_dr, data);
    
    return BKP_OK;
}

/**
 * @brief 读取备份寄存器
 */
BKP_Status_t BKP_ReadRegister(BKP_Register_t reg, uint16_t *data)
{
    uint16_t bkp_dr;
    
    if (!g_bkp_initialized)
    {
        return BKP_ERROR_NOT_INITIALIZED;
    }
    
    if (reg < BKP_REG_DR1 || reg > BKP_REG_MAX)
    {
        return BKP_ERROR_INVALID_REGISTER;
    }
    
    if (data == NULL)
    {
        return BKP_ERROR_INVALID_PARAM;
    }
    
    /* 获取SPL库寄存器值 */
    bkp_dr = bkp_reg_map[reg];
    
    /* 读取备份寄存器 */
    *data = BKP_ReadBackupRegister(bkp_dr);
    
    return BKP_OK;
}

/**
 * @brief 检查BKP是否已初始化
 */
uint8_t BKP_IsInitialized(void)
{
    return g_bkp_initialized ? 1 : 0;
}

/* ========== Tamper引脚功能实现 ========== */

/* Tamper中断回调 */
static BKP_TamperCallback_t g_bkp_tamper_callback = NULL;
static void *g_bkp_tamper_user_data = NULL;

/**
 * @brief 配置BKP Tamper引脚
 */
BKP_Status_t BKP_ConfigTamperPin(BKP_TamperLevel_t level, BKP_TamperCallback_t callback, void *user_data)
{
    if (!g_bkp_initialized)
    {
        return BKP_ERROR_NOT_INITIALIZED;
    }
    
    /* 配置Tamper引脚电平 */
    if (level == BKP_TAMPER_LEVEL_HIGH)
    {
        BKP_TamperPinLevelConfig(BKP_TamperPinLevel_High);
    }
    else
    {
        BKP_TamperPinLevelConfig(BKP_TamperPinLevel_Low);
    }
    
    /* 保存回调函数 */
    g_bkp_tamper_callback = callback;
    g_bkp_tamper_user_data = user_data;
    
    /* 如果提供了回调函数，使能Tamper中断 */
    if (callback != NULL)
    {
        BKP_ITConfig(ENABLE);
        /* 使能EXTI中断（Tamper使用EXTI线19） */
        EXTI->IMR |= EXTI_Line19;
        EXTI->RTSR |= EXTI_Line19;  /* 上升沿触发 */
        EXTI->FTSR |= EXTI_Line19;  /* 下降沿触发 */
    }
    
    return BKP_OK;
}

/**
 * @brief 使能BKP Tamper引脚
 */
BKP_Status_t BKP_EnableTamperPin(void)
{
    if (!g_bkp_initialized)
    {
        return BKP_ERROR_NOT_INITIALIZED;
    }
    
    BKP_TamperPinCmd(ENABLE);
    
    return BKP_OK;
}

/**
 * @brief 禁用BKP Tamper引脚
 */
BKP_Status_t BKP_DisableTamperPin(void)
{
    if (!g_bkp_initialized)
    {
        return BKP_ERROR_NOT_INITIALIZED;
    }
    
    BKP_TamperPinCmd(DISABLE);
    BKP_ITConfig(DISABLE);
    
    return BKP_OK;
}

/**
 * @brief BKP Tamper中断处理（应在中断服务程序中调用）
 */
void BKP_Tamper_IRQHandler(void)
{
    if (BKP_GetFlagStatus() != RESET)
    {
        BKP_ClearFlag();
        
        if (g_bkp_tamper_callback != NULL)
        {
            g_bkp_tamper_callback(g_bkp_tamper_user_data);
        }
    }
}

#endif /* CONFIG_MODULE_BKP_ENABLED */

