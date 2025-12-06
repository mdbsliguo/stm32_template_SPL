/**
 * @file uart.h
 * @brief UART驱动模块
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的UART驱动，支持UART1/2/3，阻塞式发送/接收，可配置波特率、数据位、停止位、校验位
 */

#ifndef UART_H
#define UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include <stdint.h>

/**
 * @brief UART错误码
 */
typedef enum {
    UART_OK = ERROR_OK,                                    /**< 操作成功 */
    UART_ERROR_NOT_IMPLEMENTED = ERROR_BASE_UART - 99,    /**< 功能未实现（占位空函数） */
    UART_ERROR_NULL_PTR = ERROR_BASE_UART - 1,             /**< 空指针错误 */
    UART_ERROR_INVALID_PARAM = ERROR_BASE_UART - 2,        /**< 参数非法（通用） */
    UART_ERROR_INVALID_INSTANCE = ERROR_BASE_UART - 3,    /**< 无效实例编号 */
    UART_ERROR_INVALID_PERIPH = ERROR_BASE_UART - 4,      /**< 无效的外设 */
    UART_ERROR_NOT_INITIALIZED = ERROR_BASE_UART - 5,     /**< 未初始化 */
    UART_ERROR_GPIO_FAILED = ERROR_BASE_UART - 6,         /**< GPIO配置失败 */
    UART_ERROR_TIMEOUT = ERROR_BASE_UART - 7,             /**< 操作超时 */
    UART_ERROR_BUSY = ERROR_BASE_UART - 8,                /**< UART忙 */
    UART_ERROR_INTERRUPT_NOT_ENABLED = ERROR_BASE_UART - 9, /**< 中断未使能 */
    UART_ERROR_ORE = ERROR_BASE_UART - 10,                /**< 溢出错误（Overrun Error） */
    UART_ERROR_NE = ERROR_BASE_UART - 11,                 /**< 噪声错误（Noise Error） */
    UART_ERROR_FE = ERROR_BASE_UART - 12,                 /**< 帧错误（Framing Error） */
    UART_ERROR_PE = ERROR_BASE_UART - 13,                 /**< 奇偶校验错误（Parity Error） */
} UART_Status_t;

/**
 * @brief UART实例索引（用于多UART支持）
 */
typedef enum {
    UART_INSTANCE_1 = 0,   /**< UART1实例 */
    UART_INSTANCE_2 = 1,   /**< UART2实例 */
    UART_INSTANCE_3 = 2,   /**< UART3实例 */
    UART_INSTANCE_MAX      /**< 最大实例数 */
} UART_Instance_t;

/**
 * @brief UART中断类型枚举
 */
typedef enum {
    UART_IT_TXE = 0,       /**< 发送缓冲区空中断 */
    UART_IT_TC = 1,        /**< 发送完成中断 */
    UART_IT_RXNE = 2,      /**< 接收数据就绪中断 */
    UART_IT_IDLE = 3,      /**< 空闲中断 */
    UART_IT_PE = 4,        /**< 奇偶校验错误中断 */
    UART_IT_ERR = 5,       /**< 错误中断（包含ORE、NE、FE） */
    UART_IT_LBD = 6,       /**< LIN断点检测中断 */
    UART_IT_CTS = 7,       /**< CTS中断 */
} UART_IT_t;

/**
 * @brief UART中断回调函数类型
 * @param[in] instance UART实例索引
 * @param[in] it_type 中断类型
 * @param[in] user_data 用户数据指针
 */
typedef void (*UART_IT_Callback_t)(UART_Instance_t instance, UART_IT_t it_type, void *user_data);

/* Include board.h, which includes stm32f10x.h and stm32f10x_usart.h */
#include "board.h"

/**
 * @brief UART初始化
 * @param[in] instance UART实例索引（UART_INSTANCE_1/2/3）
 * @return UART_Status_t 错误码
 * @note 根据board.h中的配置初始化UART外设和GPIO引脚
 */
UART_Status_t UART_Init(UART_Instance_t instance);

/**
 * @brief UART反初始化
 * @param[in] instance UART实例索引
 * @return UART_Status_t 错误码
 */
UART_Status_t UART_Deinit(UART_Instance_t instance);

/**
 * @brief UART发送数据（阻塞式）
 * @param[in] instance UART实例索引
 * @param[in] data 要发送的数据缓冲区
 * @param[in] length 数据长度（字节数）
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return UART_Status_t 错误码
 */
UART_Status_t UART_Transmit(UART_Instance_t instance, const uint8_t *data, uint16_t length, uint32_t timeout);

/**
 * @brief UART接收数据（阻塞式）
 * @param[in] instance UART实例索引
 * @param[out] data 接收数据缓冲区
 * @param[in] length 要接收的数据长度（字节数）
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return UART_Status_t 错误码
 */
UART_Status_t UART_Receive(UART_Instance_t instance, uint8_t *data, uint16_t length, uint32_t timeout);

/**
 * @brief UART发送单个字节（阻塞式）
 * @param[in] instance UART实例索引
 * @param[in] byte 要发送的字节
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return UART_Status_t 错误码
 */
UART_Status_t UART_TransmitByte(UART_Instance_t instance, uint8_t byte, uint32_t timeout);

/**
 * @brief UART接收单个字节（阻塞式）
 * @param[in] instance UART实例索引
 * @param[out] byte 接收到的字节
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return UART_Status_t 错误码
 */
UART_Status_t UART_ReceiveByte(UART_Instance_t instance, uint8_t *byte, uint32_t timeout);

/**
 * @brief UART发送字符串（阻塞式）
 * @param[in] instance UART实例索引
 * @param[in] str 要发送的字符串（以'\0'结尾）
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return UART_Status_t 错误码
 */
UART_Status_t UART_TransmitString(UART_Instance_t instance, const char *str, uint32_t timeout);

/**
 * @brief 检查UART是否已初始化
 * @param[in] instance UART实例索引
 * @return uint8_t 1-已初始化，0-未初始化
 */
uint8_t UART_IsInitialized(UART_Instance_t instance);

/**
 * @brief 获取UART外设指针
 * @param[in] instance UART实例索引
 * @return USART_TypeDef* UART外设指针，失败返回NULL
 */
USART_TypeDef* UART_GetPeriph(UART_Instance_t instance);

/**
 * @brief 动态修改UART波特率（不重新初始化）
 * @param[in] uart_periph UART外设指针（USART1/USART2/USART3）
 * @param[in] baudrate 目标波特率
 * @return UART_Status_t 错误码
 * @note 此函数直接修改BRR寄存器，执行时间 < 1μs
 * @note 不影响UART其他配置（数据位、停止位等）
 * @note 不丢失发送/接收状态
 * 
 * @section 使用方法
 * 
 * @subsection 基本用法
 * @code
 * // 获取UART外设指针
 * USART_TypeDef *uart = UART_GetPeriph(UART_INSTANCE_1);
 * if (uart != NULL) {
 *     // 设置波特率为115200
 *     UART_Status_t status = UART_SetBaudRate(uart, 115200);
 *     if (status != UART_OK) {
 *         // 处理错误
 *     }
 * }
 * @endcode
 * 
 * @subsection 频率变化时调整波特率
 * @code
 * // 在系统频率变化后，需要重新设置UART波特率
 * // 例如：在时钟管理模块降频后调用
 * uint32_t current_freq = CLKM_GetCurrentFrequency();
 * if (current_freq != last_freq) {
 *     USART_TypeDef *uart = UART_GetPeriph(UART_INSTANCE_1);
 *     if (uart != NULL) {
 *         UART_SetBaudRate(uart, 115200);  // 使用固定波特率，BRR会自动适应新频率
 *     }
 *     last_freq = current_freq;
 * }
 * @endcode
 * 
 * @subsection 直接使用外设指针
 * @code
 * // 如果已知外设指针，可以直接使用
 * UART_SetBaudRate(USART1, 115200);
 * UART_SetBaudRate(USART2, 9600);
 * @endcode
 */
UART_Status_t UART_SetBaudRate(USART_TypeDef *uart_periph, uint32_t baudrate);

/* ========== 中断模式功能 ========== */

/**
 * @brief 使能UART中断
 * @param[in] instance UART实例索引
 * @param[in] it_type 中断类型
 * @return UART_Status_t 错误码
 */
UART_Status_t UART_EnableIT(UART_Instance_t instance, UART_IT_t it_type);

/**
 * @brief 禁用UART中断
 * @param[in] instance UART实例索引
 * @param[in] it_type 中断类型
 * @return UART_Status_t 错误码
 */
UART_Status_t UART_DisableIT(UART_Instance_t instance, UART_IT_t it_type);

/**
 * @brief 设置UART中断回调函数
 * @param[in] instance UART实例索引
 * @param[in] it_type 中断类型
 * @param[in] callback 回调函数指针，NULL表示禁用回调
 * @param[in] user_data 用户数据指针
 * @return UART_Status_t 错误码
 */
UART_Status_t UART_SetITCallback(UART_Instance_t instance, UART_IT_t it_type, 
                                  UART_IT_Callback_t callback, void *user_data);

/**
 * @brief 获取UART中断状态
 * @param[in] instance UART实例索引
 * @param[in] it_type 中断类型
 * @return uint8_t 1=中断挂起，0=未挂起
 */
uint8_t UART_GetITStatus(UART_Instance_t instance, UART_IT_t it_type);

/**
 * @brief 清除UART中断挂起标志
 * @param[in] instance UART实例索引
 * @param[in] it_type 中断类型
 * @return UART_Status_t 错误码
 */
UART_Status_t UART_ClearITPendingBit(UART_Instance_t instance, UART_IT_t it_type);

/**
 * @brief UART中断服务函数（应在中断服务程序中调用）
 * @param[in] instance UART实例索引
 */
void UART_IRQHandler(UART_Instance_t instance);

/**
 * @brief UART非阻塞发送（中断模式）
 * @param[in] instance UART实例索引
 * @param[in] data 要发送的数据缓冲区
 * @param[in] length 数据长度（字节数）
 * @return UART_Status_t 错误码
 * @note 需要先使能TXE中断，在中断回调中继续发送
 */
UART_Status_t UART_TransmitIT(UART_Instance_t instance, const uint8_t *data, uint16_t length);

/**
 * @brief UART非阻塞接收（中断模式）
 * @param[in] instance UART实例索引
 * @param[out] data 接收数据缓冲区
 * @param[in] max_length 最大接收长度（字节数）
 * @return UART_Status_t 错误码
 * @note 需要先使能RXNE中断，在中断回调中处理接收数据
 */
UART_Status_t UART_ReceiveIT(UART_Instance_t instance, uint8_t *data, uint16_t max_length);

/**
 * @brief 获取UART发送剩余字节数（中断模式）
 * @param[in] instance UART实例索引
 * @return uint16_t 剩余字节数
 */
uint16_t UART_GetTransmitRemaining(UART_Instance_t instance);

/**
 * @brief 获取UART接收到的字节数（中断模式）
 * @param[in] instance UART实例索引
 * @return uint16_t 接收到的字节数
 */
uint16_t UART_GetReceiveCount(UART_Instance_t instance);

/* ========== DMA模式功能 ========== */

/**
 * @brief UART DMA发送
 * @param[in] instance UART实例索引
 * @param[in] data 要发送的数据缓冲区
 * @param[in] length 数据长度（字节数）
 * @return UART_Status_t 错误码
 * @note 需要先配置对应的DMA通道，此函数会启动DMA传输
 */
UART_Status_t UART_TransmitDMA(UART_Instance_t instance, const uint8_t *data, uint16_t length);

/**
 * @brief UART DMA接收
 * @param[in] instance UART实例索引
 * @param[out] data 接收数据缓冲区
 * @param[in] length 要接收的数据长度（字节数）
 * @return UART_Status_t 错误码
 * @note 需要先配置对应的DMA通道，此函数会启动DMA传输
 */
UART_Status_t UART_ReceiveDMA(UART_Instance_t instance, uint8_t *data, uint16_t length);

/**
 * @brief 停止UART DMA发送
 * @param[in] instance UART实例索引
 * @return UART_Status_t 错误码
 */
UART_Status_t UART_StopTransmitDMA(UART_Instance_t instance);

/**
 * @brief 停止UART DMA接收
 * @param[in] instance UART实例索引
 * @return UART_Status_t 错误码
 */
UART_Status_t UART_StopReceiveDMA(UART_Instance_t instance);

/**
 * @brief 获取UART DMA发送剩余字节数
 * @param[in] instance UART实例索引
 * @return uint16_t 剩余字节数
 */
uint16_t UART_GetTransmitDMARemaining(UART_Instance_t instance);

/**
 * @brief 获取UART DMA接收剩余字节数
 * @param[in] instance UART实例索引
 * @return uint16_t 剩余字节数
 */
uint16_t UART_GetReceiveDMARemaining(UART_Instance_t instance);

/* ========== 硬件流控功能 ========== */

/**
 * @brief UART硬件流控模式枚举
 */
typedef enum {
    UART_HW_FLOW_NONE = 0,    /**< 无硬件流控 */
    UART_HW_FLOW_RTS = 1,     /**< 仅RTS（请求发送） */
    UART_HW_FLOW_CTS = 2,     /**< 仅CTS（清除发送） */
    UART_HW_FLOW_RTS_CTS = 3, /**< RTS和CTS */
} UART_HW_FlowControl_t;

/**
 * @brief 配置UART硬件流控
 * @param[in] instance UART实例索引
 * @param[in] flow_control 硬件流控模式
 * @return UART_Status_t 错误码
 * @note 需要重新初始化UART才能生效
 */
UART_Status_t UART_SetHardwareFlowControl(UART_Instance_t instance, UART_HW_FlowControl_t flow_control);

/**
 * @brief 获取UART硬件流控配置
 * @param[in] instance UART实例索引
 * @param[out] flow_control 硬件流控模式
 * @return UART_Status_t 错误码
 */
UART_Status_t UART_GetHardwareFlowControl(UART_Instance_t instance, UART_HW_FlowControl_t *flow_control);

/* ========== 单线半双工模式功能 ========== */

/**
 * @brief 使能UART单线半双工模式
 * @param[in] instance UART实例索引
 * @return UART_Status_t 错误码
 * @note 单线半双工模式下，TX和RX共用同一根线，需要配置为开漏输出模式
 * @note 发送时自动切换为输出模式，接收时切换为输入模式
 */
UART_Status_t UART_EnableHalfDuplex(UART_Instance_t instance);

/**
 * @brief 禁用UART单线半双工模式
 * @param[in] instance UART实例索引
 * @return UART_Status_t 错误码
 */
UART_Status_t UART_DisableHalfDuplex(UART_Instance_t instance);

/**
 * @brief 检查UART是否处于单线半双工模式
 * @param[in] instance UART实例索引
 * @return uint8_t 1-单线模式，0-全双工模式
 */
uint8_t UART_IsHalfDuplex(UART_Instance_t instance);

/* ========== LIN/IrDA/智能卡模式功能 ========== */

/**
 * @brief 使能UART LIN模式
 * @param[in] instance UART实例索引
 * @return UART_Status_t 错误码
 * @note LIN模式用于LIN总线通信
 */
UART_Status_t UART_EnableLINMode(UART_Instance_t instance);

/**
 * @brief 禁用UART LIN模式
 * @param[in] instance UART实例索引
 * @return UART_Status_t 错误码
 */
UART_Status_t UART_DisableLINMode(UART_Instance_t instance);

/**
 * @brief 发送LIN断点字符
 * @param[in] instance UART实例索引
 * @return UART_Status_t 错误码
 * @note LIN断点字符用于LIN总线同步
 */
UART_Status_t UART_SendBreak(UART_Instance_t instance);

/**
 * @brief 使能UART IrDA模式
 * @param[in] instance UART实例索引
 * @param[in] prescaler IrDA预分频器（1-31）
 * @return UART_Status_t 错误码
 * @note IrDA模式用于红外通信
 */
UART_Status_t UART_EnableIrDAMode(UART_Instance_t instance, uint8_t prescaler);

/**
 * @brief 禁用UART IrDA模式
 * @param[in] instance UART实例索引
 * @return UART_Status_t 错误码
 */
UART_Status_t UART_DisableIrDAMode(UART_Instance_t instance);

/**
 * @brief 使能UART智能卡模式
 * @param[in] instance UART实例索引
 * @return UART_Status_t 错误码
 * @note 智能卡模式用于ISO7816智能卡通信
 */
UART_Status_t UART_EnableSmartCardMode(UART_Instance_t instance);

/**
 * @brief 禁用UART智能卡模式
 * @param[in] instance UART实例索引
 * @return UART_Status_t 错误码
 */
UART_Status_t UART_DisableSmartCardMode(UART_Instance_t instance);

/* ========== UART高级功能 ========== */

/**
 * @brief UART自动波特率检测模式枚举
 */
typedef enum {
    UART_AUTOBAUD_MODE_START_BIT = 0,   /**< 起始位模式 */
    UART_AUTOBAUD_MODE_FALLING_EDGE = 1, /**< 下降沿模式 */
} UART_AutoBaudMode_t;

/**
 * @brief 使能UART自动波特率检测
 * @param[in] instance UART实例索引
 * @param[in] mode 自动波特率检测模式
 * @return UART_Status_t 错误码
 * @note 用于自动检测UART波特率，适用于未知波特率的通信
 */
UART_Status_t UART_EnableAutoBaudRate(UART_Instance_t instance, UART_AutoBaudMode_t mode);

/**
 * @brief 禁用UART自动波特率检测
 * @param[in] instance UART实例索引
 * @return UART_Status_t 错误码
 */
UART_Status_t UART_DisableAutoBaudRate(UART_Instance_t instance);

/**
 * @brief 使能UART接收器唤醒
 * @param[in] instance UART实例索引
 * @return UART_Status_t 错误码
 * @note 用于低功耗应用，接收器唤醒模式
 */
UART_Status_t UART_EnableReceiverWakeUp(UART_Instance_t instance);

/**
 * @brief 禁用UART接收器唤醒
 * @param[in] instance UART实例索引
 * @return UART_Status_t 错误码
 */
UART_Status_t UART_DisableReceiverWakeUp(UART_Instance_t instance);

/**
 * @brief 使能UART 8倍过采样
 * @param[in] instance UART实例索引
 * @return UART_Status_t 错误码
 * @note 8倍过采样可以提高波特率精度，但会增加功耗
 */
UART_Status_t UART_EnableOverSampling8(UART_Instance_t instance);

/**
 * @brief 禁用UART 8倍过采样（使用16倍过采样）
 * @param[in] instance UART实例索引
 * @return UART_Status_t 错误码
 */
UART_Status_t UART_DisableOverSampling8(UART_Instance_t instance);

#ifdef __cplusplus
}
#endif

#endif /* UART_H */
