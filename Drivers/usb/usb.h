/**
 * @file usb.h
 * @brief USB驱动模块
 * @details 提供USB 2.0全速设备接口基础框架
 * @note 仅MD/HD/CL/XL型号支持USB，需要外部48MHz晶振
 * @note 本模块提供基础框架，完整的USB协议栈需要根据具体应用（HID/CDC/MSC等）扩展
 */

#ifndef USB_H
#define USB_H

#include "error_code.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(STM32F10X_MD) || defined(STM32F10X_HD) || defined(STM32F10X_CL) || defined(STM32F10X_XL)

/**
 * @brief USB状态枚举
 */
typedef enum {
    USB_OK = ERROR_OK,                                    /**< 操作成功 */
    USB_ERROR_NOT_IMPLEMENTED = ERROR_BASE_USB - 99,      /**< 功能未实现（占位空函数） */
    USB_ERROR_NULL_PTR = ERROR_BASE_USB - 1,              /**< 空指针错误 */
    USB_ERROR_INVALID_PARAM = ERROR_BASE_USB - 2,         /**< 参数错误（通用） */
    USB_ERROR_INVALID_ENDPOINT = ERROR_BASE_USB - 3,      /**< 无效的端点 */
    USB_ERROR_NOT_INITIALIZED = ERROR_BASE_USB - 4,       /**< 未初始化 */
    USB_ERROR_BUSY = ERROR_BASE_USB - 5,                  /**< USB忙 */
    USB_ERROR_TIMEOUT = ERROR_BASE_USB - 6,               /**< 操作超时 */
    USB_ERROR_ALREADY_INITIALIZED = ERROR_BASE_USB - 7,   /**< 已初始化 */
} USB_Status_t;

/**
 * @brief USB端点类型枚举
 */
typedef enum {
    USB_EP_TYPE_CONTROL = 0,     /**< 控制端点 */
    USB_EP_TYPE_ISOCHRONOUS = 1, /**< 同步端点 */
    USB_EP_TYPE_BULK = 2,        /**< 批量端点 */
    USB_EP_TYPE_INTERRUPT = 3,   /**< 中断端点 */
} USB_EP_Type_t;

/**
 * @brief USB端点方向枚举
 */
typedef enum {
    USB_EP_DIR_OUT = 0,  /**< 输出端点（主机到设备） */
    USB_EP_DIR_IN = 1,   /**< 输入端点（设备到主机） */
} USB_EP_Dir_t;

/**
 * @brief USB端点配置结构体
 */
typedef struct {
    uint8_t endpoint;        /**< 端点号（0-7） */
    USB_EP_Type_t type;       /**< 端点类型 */
    uint16_t tx_size;         /**< 发送缓冲区大小 */
    uint16_t rx_size;         /**< 接收缓冲区大小 */
    uint8_t enabled;          /**< 使能标志：1=启用，0=禁用 */
} USB_EP_Config_t;

/**
 * @brief USB事件回调函数类型
 */
typedef void (*USB_EventCallback_t)(void *user_data);

/**
 * @brief USB初始化
 * @return USB_Status_t 状态码
 * @note 初始化USB外设，配置时钟，使能USB中断
 */
USB_Status_t USB_Init(void);

/**
 * @brief USB反初始化
 * @return USB_Status_t 状态码
 */
USB_Status_t USB_Deinit(void);

/**
 * @brief 配置USB端点
 * @param[in] config 端点配置结构体
 * @return USB_Status_t 状态码
 */
USB_Status_t USB_ConfigEndpoint(const USB_EP_Config_t *config);

/**
 * @brief 发送数据到USB端点
 * @param[in] endpoint 端点号
 * @param[in] data 数据缓冲区
 * @param[in] length 数据长度
 * @return USB_Status_t 状态码
 */
USB_Status_t USB_Send(uint8_t endpoint, const uint8_t *data, uint16_t length);

/**
 * @brief 从USB端点接收数据
 * @param[in] endpoint 端点号
 * @param[out] data 数据缓冲区
 * @param[in,out] length 输入：缓冲区大小，输出：实际接收的数据长度
 * @return USB_Status_t 状态码
 */
USB_Status_t USB_Receive(uint8_t endpoint, uint8_t *data, uint16_t *length);

/**
 * @brief 设置USB事件回调函数
 * @param[in] callback 回调函数指针
 * @param[in] user_data 用户数据指针
 * @return USB_Status_t 状态码
 */
USB_Status_t USB_SetEventCallback(USB_EventCallback_t callback, void *user_data);

/**
 * @brief 使能USB
 * @return USB_Status_t 状态码
 */
USB_Status_t USB_Enable(void);

/**
 * @brief 禁用USB
 * @return USB_Status_t 状态码
 */
USB_Status_t USB_Disable(void);

/**
 * @brief 检查USB是否已连接
 * @return uint8_t 1=已连接，0=未连接
 */
uint8_t USB_IsConnected(void);

/**
 * @brief 获取USB当前状态
 * @return uint8_t USB状态（0=未初始化，1=已初始化，2=已连接，3=已配置）
 */
uint8_t USB_GetStatus(void);

#endif /* STM32F10X_MD || STM32F10X_HD || STM32F10X_CL || STM32F10X_XL */

#ifdef __cplusplus
}
#endif

#endif /* USB_H */
