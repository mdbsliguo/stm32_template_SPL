/**
 * @file flash.h
 * @brief FLASH驱动模块
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的FLASH驱动，支持Flash编程/擦除、选项字节管理、Flash保护等功能
 */

#ifndef FLASH_H
#define FLASH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include <stdint.h>

/**
 * @brief FLASH错误码
 */
typedef enum {
    FLASH_OK = ERROR_OK,                                    /**< 操作成功 */
    FLASH_ERROR_INVALID_PARAM = ERROR_BASE_FLASH - 1,      /**< 参数非法 */
    FLASH_ERROR_BUSY = ERROR_BASE_FLASH - 2,               /**< FLASH忙 */
    ERROR_FLASH_ERROR_PG = ERROR_BASE_FLASH - 3,           /**< 编程错误 */
    ERROR_FLASH_ERROR_WRP = ERROR_BASE_FLASH - 4,           /**< 写保护错误 */
    FLASH_ERROR_TIMEOUT = ERROR_BASE_FLASH - 5,            /**< 操作超时 */
    FLASH_ERROR_NOT_ERASED = ERROR_BASE_FLASH - 6,          /**< 页未擦除 */
} FLASH_Status_t;

/**
 * @brief FLASH延迟周期枚举
 */
typedef enum {
    FLASH_LATENCY_0 = 0,    /**< 0个等待周期 */
    FLASH_LATENCY_1 = 1,    /**< 1个等待周期 */
    FLASH_LATENCY_2 = 2,    /**< 2个等待周期 */
} FLASH_Latency_t;

/* Include board.h, which includes stm32f10x.h and stm32f10x_flash.h */
#include "board.h"

/**
 * @brief 设置FLASH延迟周期
 * @param[in] latency 延迟周期
 * @return FLASH_Status_t 错误码
 * @note 根据系统时钟频率设置FLASH延迟周期，确保正确读取
 * @note 这是对SPL库FLASH_SetLatency的封装
 */
FLASH_Status_t FLASH_ConfigLatency(FLASH_Latency_t latency);

/**
 * @brief 使能FLASH预取缓冲器
 * @return FLASH_Status_t 错误码
 */
FLASH_Status_t FLASH_EnablePrefetchBuffer(void);

/**
 * @brief 禁用FLASH预取缓冲器
 * @return FLASH_Status_t 错误码
 */
FLASH_Status_t FLASH_DisablePrefetchBuffer(void);

/**
 * @brief 使能FLASH半周期访问
 * @return FLASH_Status_t 错误码
 */
FLASH_Status_t FLASH_EnableHalfCycleAccess(void);

/**
 * @brief 禁用FLASH半周期访问
 * @return FLASH_Status_t 错误码
 */
FLASH_Status_t FLASH_DisableHalfCycleAccess(void);

/**
 * @brief 解锁FLASH
 * @return FLASH_Status_t 错误码
 * @note 这是对SPL库FLASH_Unlock的封装
 */
FLASH_Status_t FLASH_UnlockFlash(void);

/**
 * @brief 锁定FLASH
 * @return FLASH_Status_t 错误码
 * @note 这是对SPL库FLASH_Lock的封装
 */
FLASH_Status_t FLASH_LockFlash(void);

/**
 * @brief 擦除FLASH页
 * @param[in] page_address 页地址
 * @return FLASH_Status_t 错误码
 */
FLASH_Status_t FLASH_ErasePage(uint32_t page_address);

/**
 * @brief 擦除所有FLASH页
 * @return FLASH_Status_t 错误码
 */
FLASH_Status_t FLASH_EraseAllPages(void);

/**
 * @brief 编程FLASH（半字）
 * @param[in] address 地址（必须是半字对齐）
 * @param[in] data 数据（半字）
 * @return FLASH_Status_t 错误码
 */
FLASH_Status_t FLASH_ProgramHalfWord(uint32_t address, uint16_t data);

/**
 * @brief 编程FLASH（字）
 * @param[in] address 地址（必须是字对齐）
 * @param[in] data 数据（字）
 * @return FLASH_Status_t 错误码
 */
FLASH_Status_t FLASH_ProgramWord(uint32_t address, uint32_t data);

/**
 * @brief 读取FLASH数据（半字）
 * @param[in] address 地址
 * @return uint16_t 数据
 */
uint16_t FLASH_ReadHalfWord(uint32_t address);

/**
 * @brief 读取FLASH数据（字）
 * @param[in] address 地址
 * @return uint32_t 数据
 */
uint32_t FLASH_ReadWord(uint32_t address);

/**
 * @brief 获取FLASH状态
 * @return FLASH_Status_t FLASH状态
 */
FLASH_Status_t FLASH_GetStatus(void);

/**
 * @brief 等待FLASH操作完成
 * @return FLASH_Status_t 错误码
 */
FLASH_Status_t FLASH_WaitForLastOperation(void);

/**
 * @brief 等待FLASH操作完成（带超时）
 * @param[in] timeout_ms 超时时间（毫秒）
 * @return FLASH_Status_t 错误码
 */
FLASH_Status_t FLASH_WaitForLastOperationWithTimeout(uint32_t timeout_ms);

/**
 * @brief 清除FLASH状态标志
 * @param[in] flag 标志位
 * @return FLASH_Status_t 错误码
 * @note 这是对SPL库FLASH_ClearFlag的封装
 */
FLASH_Status_t FLASH_ClearStatusFlag(uint32_t flag);

/* ========== 选项字节功能 ========== */

/**
 * @brief 解锁选项字节
 * @return FLASH_Status_t 错误码
 * @note 这是对SPL库FLASH_UnlockOptionBytes的封装
 */
FLASH_Status_t FLASH_UnlockOptionByte(void);

/**
 * @brief 锁定选项字节
 * @return FLASH_Status_t 错误码
 * @note 这是对SPL库FLASH_LockOptionBytes的封装
 */
FLASH_Status_t FLASH_LockOptionByte(void);

/**
 * @brief 擦除选项字节
 * @return FLASH_Status_t 错误码
 */
FLASH_Status_t FLASH_EraseOptionBytes(void);

/**
 * @brief 编程选项字节
 * @param[in] address 选项字节地址
 * @param[in] data 数据
 * @return FLASH_Status_t 错误码
 */
FLASH_Status_t FLASH_ProgramOptionByteData(uint32_t address, uint8_t data);

/**
 * @brief 使能写保护
 * @param[in] pages 要保护的页（位掩码）
 * @return FLASH_Status_t 错误码
 */
FLASH_Status_t FLASH_EnableWriteProtection(uint32_t pages);

/**
 * @brief 禁用写保护
 * @return FLASH_Status_t 错误码
 * @note 需要擦除选项字节才能禁用写保护
 */
FLASH_Status_t FLASH_DisableWriteProtection(void);

/**
 * @brief 读取用户选项字节
 * @return uint16_t 用户选项字节值
 * @note 这是对SPL库FLASH_GetUserOptionByte的封装
 */
uint16_t FLASH_ReadUserOptionByte(void);

/**
 * @brief 读取写保护选项字节
 * @return uint16_t 写保护选项字节值
 * @note 这是对SPL库FLASH_GetWriteProtectionOptionByte的封装
 */
uint16_t FLASH_ReadWriteProtectionOptionByte(void);

/**
 * @brief 读取读保护状态
 * @return uint8_t 1=已保护，0=未保护
 */
uint8_t FLASH_GetReadProtectionStatus(void);

#ifdef __cplusplus
}
#endif

#endif /* FLASH_H */
