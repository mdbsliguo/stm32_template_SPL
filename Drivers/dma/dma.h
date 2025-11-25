/**
 * @file dma.h
 * @brief DMA驱动模块
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的DMA驱动，支持DMA1/2所有通道，外设到内存、内存到外设、内存到内存传输
 */

#ifndef DMA_H
#define DMA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include <stdint.h>

/**
 * @brief DMA错误码
 */
typedef enum {
    DMA_OK = ERROR_OK,                                    /**< 操作成功 */
    DMA_ERROR_NOT_IMPLEMENTED = ERROR_BASE_DMA - 99,      /**< 功能未实现（占位空函数） */
    DMA_ERROR_NULL_PTR = ERROR_BASE_DMA - 1,             /**< 空指针错误 */
    DMA_ERROR_INVALID_PARAM = ERROR_BASE_DMA - 2,        /**< 参数非法（通用） */
    DMA_ERROR_INVALID_CHANNEL = ERROR_BASE_DMA - 3,       /**< 无效的通道 */
    DMA_ERROR_NOT_INITIALIZED = ERROR_BASE_DMA - 4,       /**< 未初始化 */
    DMA_ERROR_BUSY = ERROR_BASE_DMA - 5,                  /**< DMA忙 */
    DMA_ERROR_TIMEOUT = ERROR_BASE_DMA - 6,               /**< 操作超时 */
    DMA_ERROR_TRANSFER_FAILED = ERROR_BASE_DMA - 7,       /**< 传输失败 */
} DMA_Status_t;

/**
 * @brief DMA通道索引
 * @note DMA2仅在HD/CL/HD_VL型号上可用，MD/LD型号只有DMA1
 */
typedef enum {
    DMA_CHANNEL_1_1 = 0,   /**< DMA1 Channel1 */
    DMA_CHANNEL_1_2 = 1,   /**< DMA1 Channel2 */
    DMA_CHANNEL_1_3 = 2,   /**< DMA1 Channel3 */
    DMA_CHANNEL_1_4 = 3,   /**< DMA1 Channel4 */
    DMA_CHANNEL_1_5 = 4,   /**< DMA1 Channel5 */
    DMA_CHANNEL_1_6 = 5,   /**< DMA1 Channel6 */
    DMA_CHANNEL_1_7 = 6,   /**< DMA1 Channel7 */
#if defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_HD_VL)
    DMA_CHANNEL_2_1 = 7,   /**< DMA2 Channel1 (仅HD/CL/HD_VL型号) */
    DMA_CHANNEL_2_2 = 8,   /**< DMA2 Channel2 (仅HD/CL/HD_VL型号) */
    DMA_CHANNEL_2_3 = 9,   /**< DMA2 Channel3 (仅HD/CL/HD_VL型号) */
    DMA_CHANNEL_2_4 = 10,  /**< DMA2 Channel4 (仅HD/CL/HD_VL型号) */
    DMA_CHANNEL_2_5 = 11,  /**< DMA2 Channel5 (仅HD/CL/HD_VL型号) */
#endif
    DMA_CHANNEL_MAX        /**< 最大通道数 */
} DMA_Channel_t;

/**
 * @brief DMA传输方向
 */
typedef enum {
    DMA_DIR_PERIPHERAL_TO_MEMORY = 0,  /**< 外设到内存 */
    DMA_DIR_MEMORY_TO_PERIPHERAL = 1,  /**< 内存到外设 */
    DMA_DIR_MEMORY_TO_MEMORY = 2,      /**< 内存到内存 */
} DMA_Direction_t;

/**
 * @brief DMA中断类型枚举
 */
typedef enum
{
    DMA_IT_TYPE_TC = 0,
    DMA_IT_TYPE_HT = 1,
    DMA_IT_TYPE_TE = 2
} DMA_IT_t;

/**
 * @brief DMA传输完成回调函数类型
 * @param[in] channel DMA通道
 * @param[in] user_data 用户数据指针
 */
typedef void (*DMA_TransferCompleteCallback_t)(DMA_Channel_t channel, void *user_data);

/**
 * @brief DMA中断回调函数类型
 * @param[in] channel DMA通道
 * @param[in] it_type 中断类型
 * @param[in] user_data 用户数据指针
 */
typedef void (*DMA_IT_Callback_t)(DMA_Channel_t channel, DMA_IT_t it_type, void *user_data);

/* Include board.h, which includes stm32f10x.h and stm32f10x_dma.h */
#include "board.h"

/**
 * @brief DMA初始化
 * @param[in] channel DMA通道索引
 * @return DMA_Status_t 错误码
 * @note 根据board.h中的配置初始化DMA通道
 */
DMA_Status_t DMA_HW_Init(DMA_Channel_t channel);

/**
 * @brief DMA反初始化
 * @param[in] channel DMA通道索引
 * @return DMA_Status_t 错误码
 */
DMA_Status_t DMA_Deinit(DMA_Channel_t channel);

/**
 * @brief 配置DMA传输（外设到内存或内存到外设）
 * @param[in] channel DMA通道索引
 * @param[in] peripheral_addr 外设地址
 * @param[in] memory_addr 内存地址
 * @param[in] data_size 传输数据大小（字节数）
 * @param[in] direction 传输方向（外设到内存/内存到外设）
 * @param[in] data_width 数据宽度（1=字节，2=半字，4=字）
 * @return DMA_Status_t 错误码
 */
DMA_Status_t DMA_ConfigTransfer(DMA_Channel_t channel, uint32_t peripheral_addr, 
                                uint32_t memory_addr, uint16_t data_size,
                                DMA_Direction_t direction, uint8_t data_width);

/**
 * @brief 配置DMA内存到内存传输
 * @param[in] channel DMA通道索引
 * @param[in] src_addr 源地址
 * @param[in] dst_addr 目标地址
 * @param[in] data_size 传输数据大小（字节数）
 * @param[in] data_width 数据宽度（1=字节，2=半字，4=字）
 * @return DMA_Status_t 错误码
 */
DMA_Status_t DMA_ConfigMemoryToMemory(DMA_Channel_t channel, uint32_t src_addr,
                                      uint32_t dst_addr, uint16_t data_size, uint8_t data_width);

/**
 * @brief 启动DMA传输
 * @param[in] channel DMA通道索引
 * @return DMA_Status_t 错误码
 */
DMA_Status_t DMA_Start(DMA_Channel_t channel);

/**
 * @brief 停止DMA传输
 * @param[in] channel DMA通道索引
 * @return DMA_Status_t 错误码
 */
DMA_Status_t DMA_Stop(DMA_Channel_t channel);

/**
 * @brief 等待DMA传输完成（阻塞式）
 * @param[in] channel DMA通道索引
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return DMA_Status_t 错误码
 */
DMA_Status_t DMA_WaitComplete(DMA_Channel_t channel, uint32_t timeout);

/**
 * @brief 检查DMA传输是否完成
 * @param[in] channel DMA通道索引
 * @return uint8_t 1-完成，0-未完成
 */
uint8_t DMA_IsComplete(DMA_Channel_t channel);

/**
 * @brief 获取剩余传输数据量
 * @param[in] channel DMA通道索引
 * @return uint16_t 剩余数据量（字节数）
 */
uint16_t DMA_GetRemainingDataSize(DMA_Channel_t channel);

/**
 * @brief 设置DMA传输完成回调函数
 * @param[in] channel DMA通道索引
 * @param[in] callback 回调函数指针，NULL表示禁用回调
 * @param[in] user_data 用户数据指针
 * @return DMA_Status_t 错误码
 */
DMA_Status_t DMA_SetTransferCompleteCallback(DMA_Channel_t channel,
                                             DMA_TransferCompleteCallback_t callback,
                                             void *user_data);

/**
 * @brief 检查DMA是否已初始化
 * @param[in] channel DMA通道索引
 * @return uint8_t 1-已初始化，0-未初始化
 */
uint8_t DMA_IsInitialized(DMA_Channel_t channel);

/**
 * @brief 获取DMA通道外设指针
 * @param[in] channel DMA通道索引
 * @return DMA_Channel_TypeDef* DMA通道指针，失败返回NULL
 */
DMA_Channel_TypeDef* DMA_GetChannel(DMA_Channel_t channel);

/**
 * @brief 设置DMA传输模式（正常/循环）
 * @param[in] channel DMA通道索引
 * @param[in] mode 传输模式：0=正常模式，1=循环模式
 * @return DMA_Status_t 错误码
 * @note 循环模式适用于连续数据采集（如ADC），传输完成后自动重新开始
 * @note 内存到内存传输不支持循环模式
 */
DMA_Status_t DMA_SetMode(DMA_Channel_t channel, uint8_t mode);

/* ========== 中断模式功能 ========== */

/**
 * @brief 使能DMA中断
 * @param[in] channel DMA通道索引
 * @param[in] it_type 中断类型
 * @return DMA_Status_t 错误码
 */
DMA_Status_t DMA_EnableIT(DMA_Channel_t channel, DMA_IT_t it_type);

/**
 * @brief 禁用DMA中断
 * @param[in] channel DMA通道索引
 * @param[in] it_type 中断类型
 * @return DMA_Status_t 错误码
 */
DMA_Status_t DMA_DisableIT(DMA_Channel_t channel, DMA_IT_t it_type);

/**
 * @brief 设置DMA中断回调函数
 * @param[in] channel DMA通道索引
 * @param[in] it_type 中断类型
 * @param[in] callback 回调函数指针，NULL表示禁用回调
 * @param[in] user_data 用户数据指针
 * @return DMA_Status_t 错误码
 */
DMA_Status_t DMA_SetITCallback(DMA_Channel_t channel, DMA_IT_t it_type,
                                DMA_IT_Callback_t callback, void *user_data);

/**
 * @brief DMA中断服务函数（应在中断服务程序中调用）
 * @param[in] channel DMA通道索引
 */
void DMA_IRQHandler(DMA_Channel_t channel);

#ifdef __cplusplus
}
#endif

#endif /* DMA_H */

