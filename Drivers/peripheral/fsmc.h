/**
 * @file fsmc.h
 * @brief FSMC驱动模块
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的FSMC驱动，支持外部SRAM、NOR Flash、NAND Flash、LCD接口
 * @note 需要外部存储器，通常配合大容量存储或LCD使用
 */

#ifndef FSMC_H
#define FSMC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include <stdint.h>

/**
 * @brief FSMC错误码
 */
typedef enum {
    FSMC_OK = ERROR_OK,                                    /**< 操作成功 */
    FSMC_ERROR_NOT_INITIALIZED = ERROR_BASE_FSMC - 1,     /**< 未初始化 */
    FSMC_ERROR_INVALID_PARAM = ERROR_BASE_FSMC - 2,       /**< 参数非法 */
    FSMC_ERROR_INVALID_BANK = ERROR_BASE_FSMC - 3,        /**< 无效的Bank */
} FSMC_Status_t;

/**
 * @brief FSMC Bank枚举
 */
typedef enum {
    FSMC_BANK_NORSRAM1 = 0,   /**< NOR/SRAM Bank1 */
    FSMC_BANK_NORSRAM2 = 1,   /**< NOR/SRAM Bank2 */
    FSMC_BANK_NORSRAM3 = 2,   /**< NOR/SRAM Bank3 */
    FSMC_BANK_NORSRAM4 = 3,   /**< NOR/SRAM Bank4 */
    FSMC_BANK_NAND2 = 4,      /**< NAND Bank2 */
    FSMC_BANK_NAND3 = 5,      /**< NAND Bank3 */
    FSMC_BANK_PCCARD = 6,     /**< PCCARD Bank4 */
} FSMC_Bank_t;

/* Include board.h, which includes stm32f10x.h and stm32f10x_fsmc.h */
#include "board.h"

/**
 * @brief FSMC NOR/SRAM初始化
 * @param[in] bank FSMC Bank
 * @param[in] memory_type 存储器类型（FSMC_MemoryType_SRAM/NOR/PSRAM等）
 * @param[in] memory_width 数据宽度（FSMC_MemoryDataWidth_8b/16b）
 * @param[in] address_setup_time 地址建立时间（0-15）
 * @param[in] data_setup_time 数据建立时间（0-255）
 * @return FSMC_Status_t 错误码
 */
FSMC_Status_t FSMC_NORSRAM_Init(FSMC_Bank_t bank, uint32_t memory_type, 
                                 uint32_t memory_width, uint8_t address_setup_time, 
                                 uint8_t data_setup_time);

/**
 * @brief FSMC NOR/SRAM反初始化
 * @param[in] bank FSMC Bank
 * @return FSMC_Status_t 错误码
 */
FSMC_Status_t FSMC_NORSRAM_Deinit(FSMC_Bank_t bank);

/**
 * @brief 使能FSMC NOR/SRAM
 * @param[in] bank FSMC Bank
 * @return FSMC_Status_t 错误码
 */
FSMC_Status_t FSMC_NORSRAM_Enable(FSMC_Bank_t bank);

/**
 * @brief 禁用FSMC NOR/SRAM
 * @param[in] bank FSMC Bank
 * @return FSMC_Status_t 错误码
 */
FSMC_Status_t FSMC_NORSRAM_Disable(FSMC_Bank_t bank);

/**
 * @brief FSMC NAND初始化
 * @param[in] bank FSMC Bank（FSMC_BANK_NAND2或FSMC_BANK_NAND3）
 * @return FSMC_Status_t 错误码
 */
FSMC_Status_t FSMC_NAND_Init(FSMC_Bank_t bank);

/**
 * @brief FSMC NAND反初始化
 * @param[in] bank FSMC Bank
 * @return FSMC_Status_t 错误码
 */
FSMC_Status_t FSMC_NAND_Deinit(FSMC_Bank_t bank);

/**
 * @brief 使能FSMC NAND
 * @param[in] bank FSMC Bank
 * @return FSMC_Status_t 错误码
 */
FSMC_Status_t FSMC_NAND_Enable(FSMC_Bank_t bank);

/**
 * @brief 禁用FSMC NAND
 * @param[in] bank FSMC Bank
 * @return FSMC_Status_t 错误码
 */
FSMC_Status_t FSMC_NAND_Disable(FSMC_Bank_t bank);

/**
 * @brief 使能FSMC NAND ECC
 * @param[in] bank FSMC Bank
 * @return FSMC_Status_t 错误码
 */
FSMC_Status_t FSMC_NAND_EnableECC(FSMC_Bank_t bank);

/**
 * @brief 禁用FSMC NAND ECC
 * @param[in] bank FSMC Bank
 * @return FSMC_Status_t 错误码
 */
FSMC_Status_t FSMC_NAND_DisableECC(FSMC_Bank_t bank);

/**
 * @brief FSMC PCCARD初始化
 * @return FSMC_Status_t 错误码
 */
FSMC_Status_t FSMC_PCCARD_Init(void);

/**
 * @brief FSMC PCCARD反初始化
 * @return FSMC_Status_t 错误码
 */
FSMC_Status_t FSMC_PCCARD_Deinit(void);

/**
 * @brief 使能FSMC PCCARD
 * @return FSMC_Status_t 错误码
 */
FSMC_Status_t FSMC_PCCARD_Enable(void);

/**
 * @brief 禁用FSMC PCCARD
 * @return FSMC_Status_t 错误码
 */
FSMC_Status_t FSMC_PCCARD_Disable(void);

/**
 * @brief 使能FSMC中断
 * @param[in] bank FSMC Bank
 * @param[in] fsmc_it 中断类型
 * @return FSMC_Status_t 错误码
 */
FSMC_Status_t FSMC_EnableIT(uint32_t bank, uint32_t fsmc_it);

/**
 * @brief 禁用FSMC中断
 * @param[in] bank FSMC Bank
 * @param[in] fsmc_it 中断类型
 * @return FSMC_Status_t 错误码
 */
FSMC_Status_t FSMC_DisableIT(uint32_t bank, uint32_t fsmc_it);

/**
 * @brief 获取FSMC标志状态
 * @param[in] bank FSMC Bank
 * @param[in] flag 标志位
 * @return uint8_t 1=置位，0=复位
 */
uint8_t FSMC_GetFlagStatus(uint32_t bank, uint32_t flag);

/**
 * @brief 清除FSMC标志
 * @param[in] bank FSMC Bank
 * @param[in] flag 标志位
 * @return FSMC_Status_t 错误码
 */
FSMC_Status_t FSMC_ClearFlag(uint32_t bank, uint32_t flag);

/**
 * @brief 获取FSMC中断状态
 * @param[in] bank FSMC Bank
 * @param[in] it 中断类型
 * @return uint8_t 1=置位，0=复位
 */
uint8_t FSMC_GetITStatus(uint32_t bank, uint32_t it);

/**
 * @brief 清除FSMC中断挂起位
 * @param[in] bank FSMC Bank
 * @param[in] it 中断类型
 * @return FSMC_Status_t 错误码
 */
FSMC_Status_t FSMC_ClearITPendingBit(uint32_t bank, uint32_t it);

#ifdef __cplusplus
}
#endif

#endif /* FSMC_H */
