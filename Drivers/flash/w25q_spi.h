/**
 * @file w25q_spi.h
 * @brief W25Q SPI Flash驱动模块
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的W25Q系列SPI Flash驱动，支持自动识别、4字节模式管理、统一地址接口
 * 
 * @note 设计约束：
 * - 状态集中管理：所有容量差异信息保存在全局结构体 g_w25q_device 中
 * - 接口纯净性：API参数仅限 uint32_t addr，禁止传入sector_idx或page_num
 * - 失败熔断机制：初始化失败后所有API返回 W25Q_ERROR_NOT_INIT
 * - 64位地址运算：所有 addr + len 计算必须强制转换为 uint64_t
 * - 4字节模式非易失性：≥128Mb芯片每次初始化必须重新发送0xB7指令
 */

#ifndef W25Q_SPI_H
#define W25Q_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include "config.h"
#include "board.h"
#include <stdint.h>

/* 模块开关检查 */
#ifdef CONFIG_MODULE_W25Q_ENABLED
#if CONFIG_MODULE_W25Q_ENABLED

/* 包含SPI驱动 */
#ifdef CONFIG_MODULE_SPI_ENABLED
#if CONFIG_MODULE_SPI_ENABLED
#include "spi_hw.h"
#endif
#endif

/**
 * @brief W25Q状态枚举
 */
typedef enum {
    W25Q_STATE_UNINITIALIZED = 0,  /**< 未初始化 */
    W25Q_STATE_INITIALIZED = 1     /**< 已初始化 */
} W25Q_State_t;

/**
 * @brief W25Q设备信息结构体（唯一真理源）
 * @note 所有容量差异信息必须保存在此结构体中，禁止分散存储
 */
typedef struct {
    uint32_t capacity_mb;        /**< 容量（MB） */
    uint8_t addr_bytes;          /**< 地址字节数（3或4） */
    uint8_t is_4byte_mode;       /**< 4字节模式标志（1=4字节模式，0=3字节模式） */
    W25Q_State_t state;          /**< 模块状态 */
    uint16_t manufacturer_id;    /**< 制造商ID */
    uint16_t device_id;          /**< 设备ID */
} w25q_dev_t;

/**
 * @brief W25Q错误码
 */
typedef enum {
    W25Q_OK = ERROR_OK,                                    /**< 操作成功 */
    W25Q_ERROR_INIT_FAILED = ERROR_BASE_W25Q - 1,          /**< 初始化失败 */
    W25Q_ERROR_ID_MISMATCH = ERROR_BASE_W25Q - 2,         /**< ID不匹配 */
    W25Q_ERROR_OUT_OF_BOUND = ERROR_BASE_W25Q - 3,       /**< 地址越界 */
    W25Q_ERROR_4BYTE_MODE_FAIL = ERROR_BASE_W25Q - 4,     /**< 4字节模式切换失败 */
    W25Q_ERROR_NOT_INIT = ERROR_BASE_W25Q - 5,           /**< 模块未初始化 */
    W25Q_ERROR_TIMEOUT = ERROR_BASE_W25Q - 6,            /**< 操作超时 */
} W25Q_Status_t;

/**
 * @brief W25Q初始化
 * @return W25Q_Status_t 错误码
 * @note 自动识别Flash型号并配置，≥128Mb芯片自动进入4字节模式
 * @note 初始化失败时模块进入 UNINITIALIZED 状态，后续所有API返回 W25Q_ERROR_NOT_INIT
 */
W25Q_Status_t W25Q_Init(void);

/**
 * @brief W25Q反初始化
 * @return W25Q_Status_t 错误码
 */
W25Q_Status_t W25Q_Deinit(void);

/**
 * @brief 获取W25Q设备信息（只读指针）
 * @return const w25q_dev_t* 设备信息指针，未初始化时返回NULL
 */
const w25q_dev_t* W25Q_GetInfo(void);

/**
 * @brief 检查W25Q是否已初始化
 * @return uint8_t 1-已初始化，0-未初始化
 * @note 兼容项目规范，内部查询 g_w25q_device.state
 */
uint8_t W25Q_IsInitialized(void);

/**
 * @brief 读取数据
 * @param[in] addr 起始地址（uint32_t全局地址）
 * @param[out] buf 数据缓冲区
 * @param[in] len 数据长度（字节）
 * @return W25Q_Status_t 错误码
 * @note 内部自动处理跨页，地址越界返回 W25Q_ERROR_OUT_OF_BOUND
 */
W25Q_Status_t W25Q_Read(uint32_t addr, uint8_t *buf, uint32_t len);

/**
 * @brief 写入数据
 * @param[in] addr 起始地址（uint32_t全局地址）
 * @param[in] buf 数据缓冲区
 * @param[in] len 数据长度（字节）
 * @return W25Q_Status_t 错误码
 * @note 内部自动处理跨页和页边界（256字节），地址越界返回 W25Q_ERROR_OUT_OF_BOUND
 */
W25Q_Status_t W25Q_Write(uint32_t addr, const uint8_t *buf, uint32_t len);

/**
 * @brief 擦除扇区（4KB）
 * @param[in] addr 扇区起始地址（uint32_t全局地址）
 * @return W25Q_Status_t 错误码
 * @note 自动检查地址对齐（4KB边界），地址越界返回 W25Q_ERROR_OUT_OF_BOUND
 */
W25Q_Status_t W25Q_EraseSector(uint32_t addr);

/**
 * @brief 整片擦除
 * @return W25Q_Status_t 错误码
 * @warning 此操作会擦除整个Flash，请谨慎使用
 */
W25Q_Status_t W25Q_EraseChip(void);

/**
 * @brief 等待Flash就绪
 * @param[in] timeout_ms 超时时间（毫秒），0表示使用默认超时
 * @return W25Q_Status_t 错误码
 * @note 动态超时补偿：Gb级芯片使用更长超时时间
 */
W25Q_Status_t W25Q_WaitReady(uint32_t timeout_ms);

#endif /* CONFIG_MODULE_W25Q_ENABLED */
#endif /* CONFIG_MODULE_W25Q_ENABLED */

#ifdef __cplusplus
}
#endif

#endif /* W25Q_SPI_H */

