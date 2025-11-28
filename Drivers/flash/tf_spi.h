/**
 * @file tf_spi.h
 * @brief TF卡SPI驱动模块
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的TF卡（MicroSD卡）SPI驱动，实现SD卡SPI协议层
 * 
 * @note 设计原则：
 * - 只负责底层SD卡SPI协议，不包含文件系统相关代码
 * - 提供标准的块设备接口（扇区读写），便于与文件系统解耦
 * - 依赖SPI驱动模块，不直接操作SPI寄存器
 * - 支持SD卡v1.0和v2.0+（SDHC/SDXC）
 */

#ifndef TF_SPI_H
#define TF_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include "config.h"
#include "board.h"
#include <stdint.h>

/* 模块开关检查 */
#ifdef CONFIG_MODULE_TF_SPI_ENABLED
#if CONFIG_MODULE_TF_SPI_ENABLED

/* 包含SPI驱动 */
#ifdef CONFIG_MODULE_SPI_ENABLED
#if CONFIG_MODULE_SPI_ENABLED
#include "spi_hw.h"
#endif
#endif

/**
 * @brief TF_SPI状态枚举
 */
typedef enum {
    TF_SPI_STATE_UNINITIALIZED = 0,  /**< 未初始化 */
    TF_SPI_STATE_INITIALIZED = 1     /**< 已初始化 */
} TF_SPI_State_t;

/**
 * @brief TF_SPI卡类型枚举
 */
typedef enum {
    TF_SPI_CARD_TYPE_UNKNOWN = 0,    /**< 未知类型 */
    TF_SPI_CARD_TYPE_SDSC = 1,       /**< 标准容量SD卡（SDSC，≤2GB） */
    TF_SPI_CARD_TYPE_SDHC = 2,       /**< 高容量SD卡（SDHC，2GB-32GB） */
    TF_SPI_CARD_TYPE_SDXC = 3        /**< 扩展容量SD卡（SDXC，32GB-2TB） */
} TF_SPI_CardType_t;

/**
 * @brief TF_SPI设备信息结构体
 * @note 所有设备信息必须保存在此结构体中，禁止分散存储
 */
typedef struct {
    uint32_t capacity_mb;            /**< 容量（MB） */
    uint32_t block_size;             /**< 块大小（字节），通常为512 */
    uint32_t block_count;             /**< 块数量 */
    TF_SPI_CardType_t card_type;     /**< 卡类型（SDSC/SDHC/SDXC） */
    uint8_t is_sdhc;                 /**< 是否为SDHC/SDXC（1=是，0=否） */
    TF_SPI_State_t state;            /**< 模块状态 */
} tf_spi_dev_t;

/**
 * @brief TF_SPI错误码
 */
typedef enum {
    TF_SPI_OK = ERROR_OK,                                    /**< 操作成功 */
    TF_SPI_ERROR_INIT_FAILED = ERROR_BASE_TF_SPI - 1,        /**< 初始化失败 */
    TF_SPI_ERROR_NOT_INIT = ERROR_BASE_TF_SPI - 2,          /**< 模块未初始化 */
    TF_SPI_ERROR_INVALID_PARAM = ERROR_BASE_TF_SPI - 3,     /**< 无效参数 */
    TF_SPI_ERROR_NULL_PTR = ERROR_BASE_TF_SPI - 4,          /**< 空指针错误 */
    TF_SPI_ERROR_TIMEOUT = ERROR_BASE_TF_SPI - 5,           /**< 操作超时 */
    TF_SPI_ERROR_CMD_FAILED = ERROR_BASE_TF_SPI - 6,        /**< 命令执行失败 */
    TF_SPI_ERROR_CRC = ERROR_BASE_TF_SPI - 7,                /**< CRC错误 */
    TF_SPI_ERROR_WRITE_PROTECT = ERROR_BASE_TF_SPI - 8,      /**< 写保护 */
    TF_SPI_ERROR_OUT_OF_BOUND = ERROR_BASE_TF_SPI - 9,      /**< 地址越界 */
} TF_SPI_Status_t;

/**
 * @brief TF_SPI初始化
 * @return TF_SPI_Status_t 错误码
 * @note 自动检测SD卡类型并初始化，支持SDSC/SDHC/SDXC
 * @note 初始化失败时模块进入 UNINITIALIZED 状态，后续所有API返回 TF_SPI_ERROR_NOT_INIT
 * @note 需要先初始化SPI模块（SPI_HW_Init()）
 */
TF_SPI_Status_t TF_SPI_Init(void);

/**
 * @brief TF_SPI反初始化
 * @return TF_SPI_Status_t 错误码
 */
TF_SPI_Status_t TF_SPI_Deinit(void);

/**
 * @brief 获取TF_SPI设备信息（只读指针）
 * @return const tf_spi_dev_t* 设备信息指针，未初始化时返回NULL
 */
const tf_spi_dev_t* TF_SPI_GetInfo(void);

/**
 * @brief 检查TF_SPI是否已初始化
 * @return uint8_t 1-已初始化，0-未初始化
 */
uint8_t TF_SPI_IsInitialized(void);

/**
 * @brief 读取单个块（扇区）
 * @param[in] block_addr 块地址（扇区地址）
 * @param[out] buf 数据缓冲区（至少512字节）
 * @return TF_SPI_Status_t 错误码
 * @note 块地址范围：0 ~ (block_count - 1)
 * @note 缓冲区大小必须至少512字节
 */
TF_SPI_Status_t TF_SPI_ReadBlock(uint32_t block_addr, uint8_t *buf);

/**
 * @brief 写入单个块（扇区）
 * @param[in] block_addr 块地址（扇区地址）
 * @param[in] buf 数据缓冲区（512字节）
 * @return TF_SPI_Status_t 错误码
 * @note 块地址范围：0 ~ (block_count - 1)
 * @note 缓冲区大小必须为512字节
 * @note 写入前会自动擦除，无需手动擦除
 */
TF_SPI_Status_t TF_SPI_WriteBlock(uint32_t block_addr, const uint8_t *buf);

/**
 * @brief 读取多个块（扇区）
 * @param[in] block_addr 起始块地址（扇区地址）
 * @param[in] block_count 块数量
 * @param[out] buf 数据缓冲区（至少 block_count * 512 字节）
 * @return TF_SPI_Status_t 错误码
 * @note 块地址范围：0 ~ (block_count - 1)
 * @note 缓冲区大小必须至少 block_count * 512 字节
 */
TF_SPI_Status_t TF_SPI_ReadBlocks(uint32_t block_addr, uint32_t block_count, uint8_t *buf);

/**
 * @brief 写入多个块（扇区）
 * @param[in] block_addr 起始块地址（扇区地址）
 * @param[in] block_count 块数量
 * @param[in] buf 数据缓冲区（block_count * 512 字节）
 * @return TF_SPI_Status_t 错误码
 * @note 块地址范围：0 ~ (block_count - 1)
 * @note 缓冲区大小必须为 block_count * 512 字节
 * @note 写入前会自动擦除，无需手动擦除
 */
TF_SPI_Status_t TF_SPI_WriteBlocks(uint32_t block_addr, uint32_t block_count, const uint8_t *buf);

/**
 * @brief 发送状态查询命令（CMD13）
 * @param[out] status 状态字节（R1响应）
 * @return TF_SPI_Status_t 错误码
 * @note 用于查询SD卡当前状态（写保护、锁定、错误标志等）
 */
TF_SPI_Status_t TF_SPI_SendStatus(uint8_t *status);

/* ==================== 底层命令访问接口（教学/调试用） ==================== */

/**
 * @brief 发送SD命令（底层接口）
 * @param[in] cmd 命令字节（0x00-0x3F，不需要加0x40）
 * @param[in] arg 命令参数（32位地址）
 * @param[out] response R1响应值
 * @return TF_SPI_Status_t 错误码
 * @note 底层接口，主要用于教学演示和调试
 * @note 实际应用应使用高级API（如TF_SPI_Init()、TF_SPI_ReadBlock()等）
 * @note 需要先初始化SPI模块（SPI_HW_Init()），但不要求调用TF_SPI_Init()
 */
TF_SPI_Status_t TF_SPI_SendCMD(uint8_t cmd, uint32_t arg, uint8_t *response);

/**
 * @brief 读取CSD寄存器（底层接口）
 * @param[out] csd CSD寄存器数据（16字节）
 * @return TF_SPI_Status_t 错误码
 * @note 底层接口，主要用于教学演示和调试
 * @note 实际应用应使用TF_SPI_GetInfo()获取设备信息
 * @note 需要先初始化SPI模块（SPI_HW_Init()），但不要求调用TF_SPI_Init()
 */
TF_SPI_Status_t TF_SPI_ReadCSD(uint8_t *csd);

/**
 * @brief 读取CID寄存器（底层接口）
 * @param[out] cid CID寄存器数据（16字节）
 * @return TF_SPI_Status_t 错误码
 * @note 底层接口，主要用于教学演示和调试
 * @note 需要先初始化SPI模块（SPI_HW_Init()），但不要求调用TF_SPI_Init()
 */
TF_SPI_Status_t TF_SPI_ReadCID(uint8_t *cid);

/**
 * @brief 读取OCR寄存器（底层接口）
 * @param[out] ocr OCR寄存器值（32位）
 * @return TF_SPI_Status_t 错误码
 * @note 底层接口，主要用于教学演示和调试
 * @note 需要先初始化SPI模块（SPI_HW_Init()），但不要求调用TF_SPI_Init()
 */
TF_SPI_Status_t TF_SPI_ReadOCR(uint32_t *ocr);

/**
 * @brief 手动初始化后设置设备信息和状态（底层接口）
 * @param[in] csd CSD寄存器数据（16字节）
 * @param[in] ocr OCR寄存器值（用于判断CCS位）
 * @return TF_SPI_Status_t 错误码
 * @note 底层接口：主要用于教学示例中的手动初始化
 * @note 实际应用应使用TF_SPI_Init()进行自动初始化
 * @note 此函数会解析CSD并填充设备信息，然后设置状态为已初始化
 */
TF_SPI_Status_t TF_SPI_SetDeviceInfoFromCSD(const uint8_t *csd, uint32_t ocr);

#endif /* CONFIG_MODULE_TF_SPI_ENABLED */
#endif /* CONFIG_MODULE_TF_SPI_ENABLED */

#ifdef __cplusplus
}
#endif

#endif /* TF_SPI_H */

