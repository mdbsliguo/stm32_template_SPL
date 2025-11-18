/**
 * @file fsmc.c
 * @brief FSMC驱动模块实现
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的FSMC驱动，支持外部SRAM、NOR Flash、NAND Flash、LCD接口
 */

/* Include config.h first to get module enable flags */
#include "config.h"

#if CONFIG_MODULE_FSMC_ENABLED

/* Include our header */
#include "fsmc.h"

#include "stm32f10x_rcc.h"
#include "stm32f10x_fsmc.h"
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief 获取FSMC Bank对应的SPL库Bank值
 */
static uint32_t FSMC_GetBankValue(FSMC_Bank_t bank)
{
    switch (bank)
    {
        case FSMC_BANK_NORSRAM1: return FSMC_Bank1_NORSRAM1;
        case FSMC_BANK_NORSRAM2: return FSMC_Bank1_NORSRAM2;
        case FSMC_BANK_NORSRAM3: return FSMC_Bank1_NORSRAM3;
        case FSMC_BANK_NORSRAM4: return FSMC_Bank1_NORSRAM4;
        case FSMC_BANK_NAND2: return FSMC_Bank2_NAND;
        case FSMC_BANK_NAND3: return FSMC_Bank3_NAND;
        case FSMC_BANK_PCCARD: return FSMC_Bank4_PCCARD;
        default: return 0;
    }
}

/**
 * @brief FSMC NOR/SRAM初始化
 */
FSMC_Status_t FSMC_NORSRAM_Init(FSMC_Bank_t bank, uint32_t memory_type, 
                                 uint32_t memory_width, uint8_t address_setup_time, 
                                 uint8_t data_setup_time)
{
    FSMC_NORSRAMInitTypeDef FSMC_NORSRAMInitStructure;
    FSMC_NORSRAMTimingInitTypeDef FSMC_NORSRAMTimingInitStructure;
    uint32_t bank_value;
    
    if (bank > FSMC_BANK_NORSRAM4)
    {
        return FSMC_ERROR_INVALID_BANK;
    }
    
    if (address_setup_time > 15 || data_setup_time > 255)
    {
        return FSMC_ERROR_INVALID_PARAM;
    }
    
    bank_value = FSMC_GetBankValue(bank);
    
    /* 使能FSMC时钟 */
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_FSMC, ENABLE);
    
    /* 配置时序参数 */
    FSMC_NORSRAMTimingInitStructure.FSMC_AddressSetupTime = address_setup_time;
    FSMC_NORSRAMTimingInitStructure.FSMC_AddressHoldTime = 0;
    FSMC_NORSRAMTimingInitStructure.FSMC_DataSetupTime = data_setup_time;
    FSMC_NORSRAMTimingInitStructure.FSMC_BusTurnAroundDuration = 0;
    FSMC_NORSRAMTimingInitStructure.FSMC_CLKDivision = 0;
    FSMC_NORSRAMTimingInitStructure.FSMC_DataLatency = 0;
    FSMC_NORSRAMTimingInitStructure.FSMC_AccessMode = FSMC_AccessMode_A;
    
    /* 配置FSMC NOR/SRAM */
    FSMC_NORSRAMInitStructure.FSMC_Bank = bank_value;
    FSMC_NORSRAMInitStructure.FSMC_DataAddressMux = FSMC_DataAddressMux_Disable;
    FSMC_NORSRAMInitStructure.FSMC_MemoryType = memory_type;
    FSMC_NORSRAMInitStructure.FSMC_MemoryDataWidth = memory_width;
    FSMC_NORSRAMInitStructure.FSMC_BurstAccessMode = FSMC_BurstAccessMode_Disable;
    FSMC_NORSRAMInitStructure.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low;
    FSMC_NORSRAMInitStructure.FSMC_WrapMode = FSMC_WrapMode_Disable;
    FSMC_NORSRAMInitStructure.FSMC_WaitSignalActive = FSMC_WaitSignalActive_BeforeWaitState;
    FSMC_NORSRAMInitStructure.FSMC_WriteOperation = FSMC_WriteOperation_Enable;
    FSMC_NORSRAMInitStructure.FSMC_WaitSignal = FSMC_WaitSignal_Disable;
    FSMC_NORSRAMInitStructure.FSMC_ExtendedMode = FSMC_ExtendedMode_Disable;
    FSMC_NORSRAMInitStructure.FSMC_WriteBurst = FSMC_WriteBurst_Disable;
    FSMC_NORSRAMInitStructure.FSMC_ReadWriteTimingStruct = &FSMC_NORSRAMTimingInitStructure;
    FSMC_NORSRAMInitStructure.FSMC_WriteTimingStruct = NULL;
    
    /* 初始化FSMC NOR/SRAM */
    FSMC_NORSRAMInit(&FSMC_NORSRAMInitStructure);
    
    return FSMC_OK;
}

/**
 * @brief FSMC NOR/SRAM反初始化
 */
FSMC_Status_t FSMC_NORSRAM_Deinit(FSMC_Bank_t bank)
{
    uint32_t bank_value;
    
    if (bank > FSMC_BANK_NORSRAM4)
    {
        return FSMC_ERROR_INVALID_BANK;
    }
    
    bank_value = FSMC_GetBankValue(bank);
    FSMC_NORSRAMDeInit(bank_value);
    
    return FSMC_OK;
}

/**
 * @brief 使能FSMC NOR/SRAM
 */
FSMC_Status_t FSMC_NORSRAM_Enable(FSMC_Bank_t bank)
{
    uint32_t bank_value;
    
    if (bank > FSMC_BANK_NORSRAM4)
    {
        return FSMC_ERROR_INVALID_BANK;
    }
    
    bank_value = FSMC_GetBankValue(bank);
    FSMC_NORSRAMCmd(bank_value, ENABLE);
    
    return FSMC_OK;
}

/**
 * @brief 禁用FSMC NOR/SRAM
 */
FSMC_Status_t FSMC_NORSRAM_Disable(FSMC_Bank_t bank)
{
    uint32_t bank_value;
    
    if (bank > FSMC_BANK_NORSRAM4)
    {
        return FSMC_ERROR_INVALID_BANK;
    }
    
    bank_value = FSMC_GetBankValue(bank);
    FSMC_NORSRAMCmd(bank_value, DISABLE);
    
    return FSMC_OK;
}

/**
 * @brief FSMC NAND初始化（基础框架）
 */
FSMC_Status_t FSMC_NAND_Init(FSMC_Bank_t bank)
{
    uint32_t bank_value;
    
    if (bank != FSMC_BANK_NAND2 && bank != FSMC_BANK_NAND3)
    {
        return FSMC_ERROR_INVALID_BANK;
    }
    
    bank_value = FSMC_GetBankValue(bank);
    
    /* 使能FSMC时钟 */
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_FSMC, ENABLE);
    
    /* 反初始化NAND */
    FSMC_NANDDeInit(bank_value);
    
    /* 注意：完整的NAND初始化需要配置复杂的时序参数 */
    /* 这里提供基础框架，实际使用时需要根据具体NAND芯片配置 */
    
    return FSMC_OK;
}

/**
 * @brief FSMC NAND反初始化
 */
FSMC_Status_t FSMC_NAND_Deinit(FSMC_Bank_t bank)
{
    uint32_t bank_value;
    
    if (bank != FSMC_BANK_NAND2 && bank != FSMC_BANK_NAND3)
    {
        return FSMC_ERROR_INVALID_BANK;
    }
    
    bank_value = FSMC_GetBankValue(bank);
    FSMC_NANDDeInit(bank_value);
    
    return FSMC_OK;
}

/**
 * @brief 使能FSMC NAND
 */
FSMC_Status_t FSMC_NAND_Enable(FSMC_Bank_t bank)
{
    uint32_t bank_value;
    
    if (bank != FSMC_BANK_NAND2 && bank != FSMC_BANK_NAND3)
    {
        return FSMC_ERROR_INVALID_BANK;
    }
    
    bank_value = FSMC_GetBankValue(bank);
    FSMC_NANDCmd(bank_value, ENABLE);
    
    return FSMC_OK;
}

/**
 * @brief 禁用FSMC NAND
 */
FSMC_Status_t FSMC_NAND_Disable(FSMC_Bank_t bank)
{
    uint32_t bank_value;
    
    if (bank != FSMC_BANK_NAND2 && bank != FSMC_BANK_NAND3)
    {
        return FSMC_ERROR_INVALID_BANK;
    }
    
    bank_value = FSMC_GetBankValue(bank);
    FSMC_NANDCmd(bank_value, DISABLE);
    
    return FSMC_OK;
}

/**
 * @brief 使能FSMC NAND ECC
 */
FSMC_Status_t FSMC_NAND_EnableECC(FSMC_Bank_t bank)
{
    uint32_t bank_value;
    
    if (bank != FSMC_BANK_NAND2 && bank != FSMC_BANK_NAND3)
    {
        return FSMC_ERROR_INVALID_BANK;
    }
    
    bank_value = FSMC_GetBankValue(bank);
    FSMC_NANDECCCmd(bank_value, ENABLE);
    
    return FSMC_OK;
}

/**
 * @brief 禁用FSMC NAND ECC
 */
FSMC_Status_t FSMC_NAND_DisableECC(FSMC_Bank_t bank)
{
    uint32_t bank_value;
    
    if (bank != FSMC_BANK_NAND2 && bank != FSMC_BANK_NAND3)
    {
        return FSMC_ERROR_INVALID_BANK;
    }
    
    bank_value = FSMC_GetBankValue(bank);
    FSMC_NANDECCCmd(bank_value, DISABLE);
    
    return FSMC_OK;
}

/**
 * @brief FSMC PCCARD初始化（基础框架）
 */
FSMC_Status_t FSMC_PCCARD_Init(void)
{
    /* 使能FSMC时钟 */
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_FSMC, ENABLE);
    
    /* 反初始化PCCARD */
    FSMC_PCCARDDeInit();
    
    /* 注意：完整的PCCARD初始化需要配置复杂的时序参数 */
    /* 这里提供基础框架，实际使用时需要根据具体PCCARD配置 */
    
    return FSMC_OK;
}

/**
 * @brief FSMC PCCARD反初始化
 */
FSMC_Status_t FSMC_PCCARD_Deinit(void)
{
    FSMC_PCCARDDeInit();
    return FSMC_OK;
}

/**
 * @brief 使能FSMC PCCARD
 */
FSMC_Status_t FSMC_PCCARD_Enable(void)
{
    FSMC_PCCARDCmd(ENABLE);
    return FSMC_OK;
}

/**
 * @brief 禁用FSMC PCCARD
 */
FSMC_Status_t FSMC_PCCARD_Disable(void)
{
    FSMC_PCCARDCmd(DISABLE);
    return FSMC_OK;
}

/**
 * @brief 使能FSMC中断
 */
FSMC_Status_t FSMC_EnableIT(uint32_t bank, uint32_t fsmc_it)
{
    FSMC_ITConfig(bank, fsmc_it, ENABLE);
    return FSMC_OK;
}

/**
 * @brief 禁用FSMC中断
 */
FSMC_Status_t FSMC_DisableIT(uint32_t bank, uint32_t fsmc_it)
{
    FSMC_ITConfig(bank, fsmc_it, DISABLE);
    return FSMC_OK;
}

/**
 * @brief 获取FSMC标志状态（直接调用SPL库函数）
 */
uint8_t FSMC_GetFlagStatus(uint32_t bank, uint32_t flag)
{
    if (FSMC_GetFlagStatus(bank, flag) != RESET)
    {
        return 1;
    }
    return 0;
}

/**
 * @brief 清除FSMC标志（直接调用SPL库函数）
 */
FSMC_Status_t FSMC_ClearFlag(uint32_t bank, uint32_t flag)
{
    FSMC_ClearFlag(bank, flag);
    return FSMC_OK;
}

/**
 * @brief 获取FSMC中断状态（直接调用SPL库函数）
 */
uint8_t FSMC_GetITStatus(uint32_t bank, uint32_t it)
{
    if (FSMC_GetITStatus(bank, it) != RESET)
    {
        return 1;
    }
    return 0;
}

/**
 * @brief 清除FSMC中断挂起位（直接调用SPL库函数）
 */
FSMC_Status_t FSMC_ClearITPendingBit(uint32_t bank, uint32_t it)
{
    FSMC_ClearITPendingBit(bank, it);
    return FSMC_OK;
}

#endif /* CONFIG_MODULE_FSMC_ENABLED */

