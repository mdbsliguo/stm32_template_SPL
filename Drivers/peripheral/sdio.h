/**
 * @file sdio.h
 * @brief SDIO驱动模块
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的SDIO驱动，支持SD卡接口、MMC卡支持、4位/1位模式
 * @note 需要外部SD卡槽，通常配合文件系统使用
 */

#ifndef SDIO_H
#define SDIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include <stdint.h>

/**
 * @brief SDIO错误码
 */
typedef enum {
    SDIO_OK = ERROR_OK,                                    /**< 操作成功 */
    SDIO_ERROR_NOT_IMPLEMENTED = ERROR_BASE_SDIO - 99,    /**< 功能未实现（占位空函数） */
    SDIO_ERROR_INVALID_PARAM = ERROR_BASE_SDIO - 1,       /**< 参数非法（通用） */
    SDIO_ERROR_NOT_INITIALIZED = ERROR_BASE_SDIO - 2,      /**< 未初始化 */
    SDIO_ERROR_TIMEOUT = ERROR_BASE_SDIO - 3,              /**< 操作超时 */
    SDIO_ERROR_BUSY = ERROR_BASE_SDIO - 4,                /**< SDIO忙 */
} SDIO_Status_t;

/**
 * @brief SDIO总线宽度枚举
 */
typedef enum {
    SDIO_BUS_WIDTH_1BIT = 0,   /**< 1位模式 */
    SDIO_BUS_WIDTH_4BIT = 1,   /**< 4位模式 */
} SDIO_BusWidth_t;

/* Include board.h, which includes stm32f10x.h and stm32f10x_sdio.h */
#include "board.h"

/**
 * @brief SDIO初始化
 * @param[in] clock_div 时钟分频器（0-255）
 * @param[in] bus_width 总线宽度
 * @return SDIO_Status_t 错误码
 */
SDIO_Status_t SDIO_Init(uint8_t clock_div, SDIO_BusWidth_t bus_width);

/**
 * @brief SDIO反初始化
 * @return SDIO_Status_t 错误码
 */
SDIO_Status_t SDIO_Deinit(void);

/**
 * @brief 使能SDIO时钟
 * @return SDIO_Status_t 错误码
 */
SDIO_Status_t SDIO_EnableClock(void);

/**
 * @brief 禁用SDIO时钟
 * @return SDIO_Status_t 错误码
 */
SDIO_Status_t SDIO_DisableClock(void);

/**
 * @brief 设置SDIO电源状态
 * @param[in] power_state 电源状态（SDIO_PowerState_OFF/SDIO_PowerState_ON）
 * @return SDIO_Status_t 错误码
 */
SDIO_Status_t SDIO_SetPowerState(uint32_t power_state);

/**
 * @brief 获取SDIO电源状态
 * @return uint32_t 电源状态
 */
uint32_t SDIO_GetPowerState(void);

/**
 * @brief 使能SDIO中断
 * @param[in] sdio_it 中断类型
 * @return SDIO_Status_t 错误码
 */
SDIO_Status_t SDIO_EnableIT(uint32_t sdio_it);

/**
 * @brief 禁用SDIO中断
 * @param[in] sdio_it 中断类型
 * @return SDIO_Status_t 错误码
 */
SDIO_Status_t SDIO_DisableIT(uint32_t sdio_it);

/**
 * @brief 使能SDIO DMA
 * @return SDIO_Status_t 错误码
 */
SDIO_Status_t SDIO_EnableDMA(void);

/**
 * @brief 禁用SDIO DMA
 * @return SDIO_Status_t 错误码
 */
SDIO_Status_t SDIO_DisableDMA(void);

/**
 * @brief 发送SDIO命令
 * @param[in] cmd_index 命令索引（0-63）
 * @param[in] argument 命令参数
 * @param[in] response_type 响应类型
 * @return SDIO_Status_t 错误码
 */
SDIO_Status_t SDIO_SendCommand(uint8_t cmd_index, uint32_t argument, uint32_t response_type);

/**
 * @brief 获取SDIO响应
 * @param[in] response_register 响应寄存器（SDIO_RESP1-4）
 * @return uint32_t 响应值
 */
uint32_t SDIO_GetResponse(uint32_t response_register);

/**
 * @brief 配置SDIO数据传输
 * @param[in] data_length 数据长度（字节）
 * @param[in] block_size 块大小
 * @param[in] transfer_dir 传输方向（SDIO_TransferDir_ToCard/SDIO_TransferDir_ToSDIO）
 * @param[in] transfer_mode 传输模式（SDIO_TransferMode_Block/SDIO_TransferMode_Stream）
 * @return SDIO_Status_t 错误码
 */
SDIO_Status_t SDIO_ConfigData(uint32_t data_length, uint32_t block_size, 
                              uint32_t transfer_dir, uint32_t transfer_mode);

/**
 * @brief 读取SDIO数据
 * @return uint32_t 数据
 */
uint32_t SDIO_ReadData(void);

/**
 * @brief 写入SDIO数据
 * @param[in] data 数据
 * @return SDIO_Status_t 错误码
 */
SDIO_Status_t SDIO_WriteData(uint32_t data);

/**
 * @brief 获取SDIO FIFO计数
 * @return uint32_t FIFO计数
 */
uint32_t SDIO_GetFIFOCount(void);

/**
 * @brief 获取SDIO标志状态
 * @param[in] flag 标志位
 * @return uint8_t 1=置位，0=复位
 */
uint8_t SDIO_GetFlagStatus(uint32_t flag);

/**
 * @brief 清除SDIO标志
 * @param[in] flag 标志位
 * @return SDIO_Status_t 错误码
 */
SDIO_Status_t SDIO_ClearFlag(uint32_t flag);

/**
 * @brief 获取SDIO中断状态
 * @param[in] it 中断类型
 * @return uint8_t 1=置位，0=复位
 */
uint8_t SDIO_GetITStatus(uint32_t it);

/**
 * @brief 清除SDIO中断挂起位
 * @param[in] it 中断类型
 * @return SDIO_Status_t 错误码
 */
SDIO_Status_t SDIO_ClearITPendingBit(uint32_t it);

/**
 * @brief 检查SDIO是否已初始化
 * @return uint8_t 1=已初始化，0=未初始化
 */
uint8_t SDIO_IsInitialized(void);

#ifdef __cplusplus
}
#endif

#endif /* SDIO_H */

