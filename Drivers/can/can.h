/**
 * @file can.h
 * @brief CAN总线驱动模块
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的CAN总线驱动，支持CAN1/2，标准帧/扩展帧，过滤器配置
 */

#ifndef CAN_H
#define CAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include <stdint.h>

/**
 * @brief CAN错误码
 */
typedef enum {
    CAN_OK = ERROR_OK,                                    /**< 操作成功 */
    CAN_ERROR_NOT_IMPLEMENTED = ERROR_BASE_CAN - 99,      /**< 功能未实现（占位空函数） */
    CAN_ERROR_NULL_PTR = ERROR_BASE_CAN - 1,              /**< 空指针错误 */
    CAN_ERROR_INVALID_PARAM = ERROR_BASE_CAN - 2,        /**< 参数非法（通用） */
    CAN_ERROR_INVALID_INSTANCE = ERROR_BASE_CAN - 3,      /**< 无效实例编号 */
    CAN_ERROR_INVALID_PERIPH = ERROR_BASE_CAN - 4,        /**< 无效的外设 */
    CAN_ERROR_GPIO_FAILED = ERROR_BASE_CAN - 5,           /**< GPIO配置失败 */
    CAN_ERROR_NOT_INITIALIZED = ERROR_BASE_CAN - 6,       /**< 未初始化 */
    CAN_ERROR_INIT_FAILED = ERROR_BASE_CAN - 7,            /**< 初始化失败 */
    CAN_ERROR_BUSY = ERROR_BASE_CAN - 8,                  /**< CAN总线忙 */
    CAN_ERROR_TIMEOUT = ERROR_BASE_CAN - 9,               /**< 操作超时 */
    CAN_ERROR_NO_MAILBOX = ERROR_BASE_CAN - 10,           /**< 无可用邮箱 */
    CAN_ERROR_NO_MESSAGE = ERROR_BASE_CAN - 11,           /**< 无消息 */
} CAN_Status_t;

/**
 * @brief CAN实例索引（用于多CAN支持）
 */
typedef enum {
    CAN_INSTANCE_1 = 0,   /**< CAN1实例 */
    CAN_INSTANCE_2 = 1,   /**< CAN2实例 */
    CAN_INSTANCE_MAX      /**< 最大实例数 */
} CAN_Instance_t;

/**
 * @brief CAN帧类型
 */
typedef enum {
    CAN_FRAME_STANDARD = 0,  /**< 标准帧（11位ID） */
    CAN_FRAME_EXTENDED = 1,  /**< 扩展帧（29位ID） */
} CAN_FrameType_t;

/**
 * @brief CAN帧类型（RTR）
 */
typedef enum {
    CAN_RTR_DATA = 0,        /**< 数据帧 */
    CAN_RTR_REMOTE = 1,      /**< 远程帧 */
} CAN_RTR_t;

/**
 * @brief CAN消息结构体
 */
typedef struct {
    uint32_t id;             /**< 消息ID（标准帧：11位，扩展帧：29位） */
    CAN_FrameType_t type;    /**< 帧类型（标准帧/扩展帧） */
    CAN_RTR_t rtr;           /**< RTR类型（数据帧/远程帧） */
    uint8_t dlc;             /**< 数据长度（0-8） */
    uint8_t data[8];         /**< 数据缓冲区 */
} CAN_Message_t;

/* Include board.h, which includes stm32f10x.h and stm32f10x_can.h */
#include "board.h"

/**
 * @brief CAN初始化
 * @param[in] instance CAN实例索引（CAN_INSTANCE_1或CAN_INSTANCE_2）
 * @return CAN_Status_t 错误码
 * @note 根据board.h中的配置初始化CAN外设和GPIO引脚
 */
CAN_Status_t CAN_Init(CAN_Instance_t instance);

/**
 * @brief CAN反初始化
 * @param[in] instance CAN实例索引
 * @return CAN_Status_t 错误码
 */
CAN_Status_t CAN_Deinit(CAN_Instance_t instance);

/**
 * @brief CAN发送消息（阻塞式）
 * @param[in] instance CAN实例索引
 * @param[in] message 要发送的消息
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return CAN_Status_t 错误码
 */
CAN_Status_t CAN_Transmit(CAN_Instance_t instance, const CAN_Message_t *message, uint32_t timeout);

/**
 * @brief CAN接收消息（阻塞式）
 * @param[in] instance CAN实例索引
 * @param[out] message 接收到的消息
 * @param[in] fifo_number FIFO编号（0或1）
 * @param[in] timeout 超时时间（毫秒），0表示使用默认超时
 * @return CAN_Status_t 错误码
 */
CAN_Status_t CAN_Receive(CAN_Instance_t instance, CAN_Message_t *message, uint8_t fifo_number, uint32_t timeout);

/**
 * @brief 检查是否有消息待接收
 * @param[in] instance CAN实例索引
 * @param[in] fifo_number FIFO编号（0或1）
 * @return uint8_t 待接收消息数量，0表示无消息
 */
uint8_t CAN_GetPendingMessageCount(CAN_Instance_t instance, uint8_t fifo_number);

/**
 * @brief 配置CAN过滤器
 * @param[in] instance CAN实例索引
 * @param[in] filter_number 过滤器编号（0-13）
 * @param[in] filter_id 过滤器ID
 * @param[in] filter_mask 过滤器掩码
 * @param[in] filter_type 过滤器类型（标准帧/扩展帧）
 * @param[in] fifo_number FIFO编号（0或1）
 * @return CAN_Status_t 错误码
 */
CAN_Status_t CAN_ConfigFilter(CAN_Instance_t instance, uint8_t filter_number,
                               uint32_t filter_id, uint32_t filter_mask,
                               CAN_FrameType_t filter_type, uint8_t fifo_number);

/**
 * @brief 检查CAN是否已初始化
 * @param[in] instance CAN实例索引
 * @return uint8_t 1-已初始化，0-未初始化
 */
uint8_t CAN_IsInitialized(CAN_Instance_t instance);

/**
 * @brief 获取CAN外设指针
 * @param[in] instance CAN实例索引
 * @return CAN_TypeDef* CAN外设指针，失败返回NULL
 */
CAN_TypeDef* CAN_GetPeriph(CAN_Instance_t instance);

/* ========== 中断模式功能 ========== */

/**
 * @brief CAN中断类型枚举
 */
typedef enum {
    CAN_IT_TX = 0,      /**< 发送完成中断 */
    CAN_IT_RX0 = 1,     /**< FIFO0接收中断 */
    CAN_IT_RX1 = 2,     /**< FIFO1接收中断 */
    CAN_IT_ERROR = 3,   /**< 错误中断（通用） */
    CAN_IT_EWG = 4,     /**< 错误警告中断 */
    CAN_IT_EPV = 5,     /**< 错误被动中断 */
    CAN_IT_BOF = 6,     /**< 总线关闭中断 */
    CAN_IT_LEC = 7,     /**< 最后错误码中断 */
    CAN_IT_FF0 = 8,     /**< FIFO0满中断 */
    CAN_IT_FF1 = 9,     /**< FIFO1满中断 */
    CAN_IT_FOV0 = 10,   /**< FIFO0溢出中断 */
    CAN_IT_FOV1 = 11,   /**< FIFO1溢出中断 */
} CAN_IT_t;

/**
 * @brief CAN中断回调函数类型
 * @param[in] instance CAN实例索引
 * @param[in] it_type 中断类型
 * @param[in] user_data 用户数据指针
 */
typedef void (*CAN_IT_Callback_t)(CAN_Instance_t instance, CAN_IT_t it_type, void *user_data);

/**
 * @brief 使能CAN中断
 * @param[in] instance CAN实例索引
 * @param[in] it_type 中断类型
 * @return CAN_Status_t 错误码
 */
CAN_Status_t CAN_EnableIT(CAN_Instance_t instance, CAN_IT_t it_type);

/**
 * @brief 禁用CAN中断
 * @param[in] instance CAN实例索引
 * @param[in] it_type 中断类型
 * @return CAN_Status_t 错误码
 */
CAN_Status_t CAN_DisableIT(CAN_Instance_t instance, CAN_IT_t it_type);

/**
 * @brief 设置CAN中断回调函数
 * @param[in] instance CAN实例索引
 * @param[in] it_type 中断类型
 * @param[in] callback 回调函数指针，NULL表示禁用回调
 * @param[in] user_data 用户数据指针
 * @return CAN_Status_t 错误码
 */
CAN_Status_t CAN_SetITCallback(CAN_Instance_t instance, CAN_IT_t it_type,
                                CAN_IT_Callback_t callback, void *user_data);

/**
 * @brief CAN非阻塞发送（中断模式）
 * @param[in] instance CAN实例索引
 * @param[in] message 要发送的消息
 * @return CAN_Status_t 错误码
 */
CAN_Status_t CAN_TransmitIT(CAN_Instance_t instance, const CAN_Message_t *message);

/**
 * @brief CAN中断服务函数（应在中断服务程序中调用）
 * @param[in] instance CAN实例索引
 * @param[in] fifo FIFO选择（0或1，仅用于接收中断）
 */
void CAN_IRQHandler(CAN_Instance_t instance, uint8_t fifo);

/**
 * @brief 获取CAN最后错误码
 * @param[in] instance CAN实例索引
 * @return uint8_t 错误码（CAN_LastErrorCode_NoError等）
 * @note 这是对SPL库CAN_GetLastErrorCode的封装，通过实例索引访问
 */
uint8_t CAN_GetInstanceLastErrorCode(CAN_Instance_t instance);

/**
 * @brief CAN总线恢复（从总线关闭状态恢复）
 * @param[in] instance CAN实例索引
 * @return CAN_Status_t 错误码
 * @note 当CAN进入总线关闭状态时，调用此函数可以重新初始化并恢复通信
 */
CAN_Status_t CAN_Recovery(CAN_Instance_t instance);

/* ========== CAN测试模式功能 ========== */

/**
 * @brief CAN工作模式枚举
 */
typedef enum {
    CAN_MODE_NORMAL = 0,            /**< 正常模式 */
    CAN_MODE_LOOPBACK = 1,          /**< 环回模式（用于自测试） */
    CAN_MODE_SILENT = 2,            /**< 静默模式（监听模式，不发送ACK） */
    CAN_MODE_SILENT_LOOPBACK = 3,   /**< 静默环回模式 */
} CAN_Mode_t;

/**
 * @brief CAN操作模式枚举
 */
typedef enum {
    CAN_OP_MODE_NORMAL = 0,         /**< 正常操作模式 */
    CAN_OP_MODE_SLEEP = 1,           /**< 睡眠模式（低功耗） */
    CAN_OP_MODE_INIT = 2,            /**< 初始化模式 */
} CAN_OperatingMode_t;

/**
 * @brief 配置CAN工作模式
 * @param[in] instance CAN实例索引
 * @param[in] mode CAN工作模式
 * @return CAN_Status_t 错误码
 * @note 需要重新初始化CAN才能生效
 */
CAN_Status_t CAN_SetMode(CAN_Instance_t instance, CAN_Mode_t mode);

/**
 * @brief 获取CAN当前工作模式
 * @param[in] instance CAN实例索引
 * @param[out] mode CAN工作模式
 * @return CAN_Status_t 错误码
 */
CAN_Status_t CAN_GetMode(CAN_Instance_t instance, CAN_Mode_t *mode);

/**
 * @brief 请求CAN操作模式
 * @param[in] instance CAN实例索引
 * @param[in] op_mode CAN操作模式
 * @return CAN_Status_t 错误码
 */
CAN_Status_t CAN_RequestOperatingMode(CAN_Instance_t instance, CAN_OperatingMode_t op_mode);

/**
 * @brief CAN进入睡眠模式
 * @param[in] instance CAN实例索引
 * @return CAN_Status_t 错误码
 */
CAN_Status_t CAN_Sleep(CAN_Instance_t instance);

/**
 * @brief CAN从睡眠模式唤醒
 * @param[in] instance CAN实例索引
 * @return CAN_Status_t 错误码
 */
CAN_Status_t CAN_WakeUp(CAN_Instance_t instance);

#ifdef __cplusplus
}
#endif

#endif /* CAN_H */
