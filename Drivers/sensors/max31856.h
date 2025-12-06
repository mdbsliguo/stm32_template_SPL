/**
 * @file max31856.h
 * @brief MAX31856热电偶温度传感器模块驱动
 * @version 1.0.0
 * @date 2024-01-01
 * @details 支持硬件SPI和软件SPI两种接口的MAX31856热电偶温度传感器驱动，完整功能实现
 *          - 支持多种热电偶类型（K、J、N、R、S、T、E、B型）
 *          - 读取热电偶温度
 *          - 读取冷端温度
 *          - 错误检测（开路、过温、欠温等）
 *          - 配置采样模式、滤波等
 * @note 解耦版本：软件SPI部分已解耦，直接使用soft_spi模块，不再重复实现SPI时序
 */

#ifndef MAX31856_H
#define MAX31856_H

#ifdef __cplusplus
extern "C" {
#endif

#include "board.h"
#include "error_code.h"
#include "gpio.h"
#include <stdint.h>

/* ==================== 模块依赖处理（解耦设计） ==================== */
/* 
 * 原则：模块不应定义其他模块的类型，只应使用
 * - SPI_Instance_t 由 spi_hw.h 定义
 * - SPI_SW_Instance_t 由 spi_sw.h 定义
 * - max31856.h 只负责使用这些类型，不负责定义
 * 
 * 使用规则：
 * - 使用硬件SPI模式前，使用者必须先包含 spi_hw.h
 * - 使用软件SPI模式前，使用者必须先包含 spi_sw.h
 */

/* 条件编译：硬件SPI模块 */
#ifdef CONFIG_MODULE_SPI_ENABLED
#if CONFIG_MODULE_SPI_ENABLED
#ifndef SPI_HW_H
#include "spi_hw.h"
#endif
#endif
#endif

/* 条件编译：软件SPI模块 */
#ifdef CONFIG_MODULE_SOFT_SPI_ENABLED
#if CONFIG_MODULE_SOFT_SPI_ENABLED
#ifndef SPI_SW_H
#include "spi_sw.h"
#endif
#endif
#endif

/**
 * @brief MAX31856错误码
 */
typedef enum {
    MAX31856_OK = ERROR_OK,                              /**< 操作成功 */
    MAX31856_ERROR_NOT_INITIALIZED = ERROR_BASE_MAX31856 - 1,  /**< 未初始化 */
    MAX31856_ERROR_INVALID_PARAM = ERROR_BASE_MAX31856 - 2,   /**< 参数非法 */
    MAX31856_ERROR_SPI_FAILED = ERROR_BASE_MAX31856 - 3,     /**< SPI通信失败 */
    MAX31856_ERROR_TIMEOUT = ERROR_BASE_MAX31856 - 4,         /**< 操作超时 */
    MAX31856_ERROR_GPIO_FAILED = ERROR_BASE_MAX31856 - 5,     /**< GPIO配置失败 */
    MAX31856_ERROR_FAULT = ERROR_BASE_MAX31856 - 6,           /**< 传感器故障 */
    MAX31856_ERROR_NOT_READY = ERROR_BASE_MAX31856 - 7,      /**< 传感器未就绪（如冷端温度传感器未初始化完成） */
} MAX31856_Status_t;

/**
 * @brief MAX31856接口类型
 */
typedef enum {
    MAX31856_INTERFACE_HARDWARE = 0,  /**< 硬件SPI接口 */
    MAX31856_INTERFACE_SOFTWARE = 1,  /**< 软件SPI接口 */
} MAX31856_InterfaceType_t;

/**
 * @brief MAX31856热电偶类型
 */
typedef enum {
    MAX31856_TC_TYPE_B = 0x00,  /**< B型热电偶 */
    MAX31856_TC_TYPE_E = 0x01,  /**< E型热电偶 */
    MAX31856_TC_TYPE_J = 0x02,  /**< J型热电偶 */
    MAX31856_TC_TYPE_K = 0x03,  /**< K型热电偶 */
    MAX31856_TC_TYPE_N = 0x04,  /**< N型热电偶 */
    MAX31856_TC_TYPE_R = 0x05,  /**< R型热电偶 */
    MAX31856_TC_TYPE_S = 0x06,  /**< S型热电偶 */
    MAX31856_TC_TYPE_T = 0x07,  /**< T型热电偶 */
} MAX31856_ThermocoupleType_t;

/**
 * @brief MAX31856采样模式
 */
typedef enum {
    MAX31856_AVG_SEL_1 = 0x00,   /**< 1次采样 */
    MAX31856_AVG_SEL_2 = 0x01,   /**< 2次采样 */
    MAX31856_AVG_SEL_4 = 0x02,   /**< 4次采样 */
    MAX31856_AVG_SEL_8 = 0x03,   /**< 8次采样 */
    MAX31856_AVG_SEL_16 = 0x04,  /**< 16次采样 */
} MAX31856_AvgSel_t;

/**
 * @brief MAX31856过温故障阈值
 */
typedef enum {
    MAX31856_OCFAULT_0 = 0x00,  /**< 过温故障阈值：0°C */
    MAX31856_OCFAULT_1 = 0x10,  /**< 过温故障阈值：1°C */
    MAX31856_OCFAULT_2 = 0x20,  /**< 过温故障阈值：2°C */
    MAX31856_OCFAULT_3 = 0x30,  /**< 过温故障阈值：3°C */
} MAX31856_OCFault_t;

/**
 * @brief MAX31856故障模式
 */
typedef enum {
    MAX31856_FAULT_MODE_COMPARATOR = 0,  /**< 比较器模式 */
    MAX31856_FAULT_MODE_INTERRUPT = 1,   /**< 中断模式 */
} MAX31856_FaultMode_t;

/**
 * @brief MAX31856冷端温度源
 */
typedef enum {
    MAX31856_CJ_SOURCE_INTERNAL = 0,  /**< 内部冷端温度 */
    MAX31856_CJ_SOURCE_EXTERNAL = 1,   /**< 外部冷端温度 */
} MAX31856_CJSource_t;

/**
 * @brief MAX31856转换模式
 */
typedef enum {
    MAX31856_CONV_MODE_CONTINUOUS = 0,  /**< 连续转换模式 */
    MAX31856_CONV_MODE_ONE_SHOT = 1,    /**< 单次转换模式 */
} MAX31856_ConvMode_t;

/**
 * @brief MAX31856故障状态标志
 */
typedef enum {
    MAX31856_FAULT_CJ_HIGH = 0x80,      /**< 冷端温度过高 */
    MAX31856_FAULT_CJ_LOW = 0x40,       /**< 冷端温度过低 */
    MAX31856_FAULT_TC_HIGH = 0x20,      /**< 热电偶温度过高 */
    MAX31856_FAULT_TC_LOW = 0x10,       /**< 热电偶温度过低 */
    MAX31856_FAULT_CJ_RANGE = 0x08,     /**< 冷端温度超出范围 */
    MAX31856_FAULT_TC_RANGE = 0x04,     /**< 热电偶温度超出范围 */
    MAX31856_FAULT_OV_UV = 0x02,        /**< 过压/欠压故障 */
    MAX31856_FAULT_OPEN = 0x01,         /**< 热电偶开路 */
} MAX31856_Fault_t;

/**
 * @brief MAX31856硬件SPI配置结构体
 * @note 仅当硬件SPI模块启用（CONFIG_MODULE_SPI_ENABLED=1）时可用
 * @note 使用前必须先包含 spi_hw.h
 */
#ifdef SPI_HW_H
typedef struct {
    SPI_Instance_t spi_instance;    /**< SPI实例（SPI_INSTANCE_1、SPI_INSTANCE_2或SPI_INSTANCE_3） */
    GPIO_TypeDef *cs_port;          /**< CS引脚端口 */
    uint16_t cs_pin;                /**< CS引脚号 */
} MAX31856_HardwareSPI_Config_t;
#else
/* 硬件SPI模块未启用，使用不完整类型（仅用于编译，不应使用） */
typedef struct MAX31856_HardwareSPI_Config MAX31856_HardwareSPI_Config_t;
#endif

/**
 * @brief MAX31856软件SPI配置结构体
 * @note 仅当软件SPI模块启用（CONFIG_MODULE_SOFT_SPI_ENABLED=1）时可用
 * @note 使用前必须先包含 spi_sw.h
 * @note 解耦版本：直接使用soft_spi模块，不再重复实现SPI时序
 */
#ifdef SPI_SW_H
typedef struct {
    SPI_SW_Instance_t soft_spi_instance;  /**< 软件SPI实例索引（SPI_SW_INSTANCE_1等） */
    GPIO_TypeDef *cs_port;                /**< CS引脚端口 */
    uint16_t cs_pin;                      /**< CS引脚号 */
} MAX31856_SoftwareSPI_Config_t;
#else
/* 软件SPI模块未启用，使用不完整类型（仅用于编译，不应使用） */
typedef struct MAX31856_SoftwareSPI_Config MAX31856_SoftwareSPI_Config_t;
#endif

/**
 * @brief MAX31856配置结构体（统一配置接口）
 * @note 根据 interface_type 选择对应的配置结构体
 */
typedef struct {
    MAX31856_InterfaceType_t interface_type;  /**< 接口类型（硬件SPI或软件SPI） */
    union {
#ifdef SPI_HW_H
        MAX31856_HardwareSPI_Config_t hardware;  /**< 硬件SPI配置（仅当硬件SPI模块启用时可用） */
#endif
#ifdef SPI_SW_H
        MAX31856_SoftwareSPI_Config_t software;   /**< 软件SPI配置（仅当软件SPI模块启用时可用） */
#endif
        uint8_t _placeholder[8];  /**< 占位符，确保union不为空（当两个模块都未启用时） */
    } config;                                 /**< 接口配置（根据interface_type选择） */
} MAX31856_Config_t;

/**
 * @brief MAX31856初始化
 * @param[in] config 配置结构体指针（包含接口类型和硬件参数）
 * @return MAX31856_Status_t 错误码
 * @note 硬件SPI模式：需要先初始化SPI模块
 * @note 软件SPI模式：自动配置GPIO引脚
 * 
 * @example 硬件SPI初始化
 * MAX31856_Config_t config = {
 *     .interface_type = MAX31856_INTERFACE_HARDWARE,
 *     .config.hardware = {
 *         .spi_instance = SPI_INSTANCE_1,
 *         .cs_port = GPIOA,
 *         .cs_pin = GPIO_Pin_4
 *     }
 * };
 * MAX31856_Init(&config);
 * 
 * @example 软件SPI初始化
 * MAX31856_Config_t config = {
 *     .interface_type = MAX31856_INTERFACE_SOFTWARE,
 *     .config.software = {
 *         .soft_spi_instance = SPI_SW_INSTANCE_1,
 *         .cs_port = GPIOA,
 *         .cs_pin = GPIO_Pin_4
 *     }
 * };
 * MAX31856_Init(&config);
 */
MAX31856_Status_t MAX31856_Init(const MAX31856_Config_t *config);

/**
 * @brief MAX31856反初始化
 * @return MAX31856_Status_t 错误码
 */
MAX31856_Status_t MAX31856_Deinit(void);

/**
 * @brief 检查MAX31856是否已初始化
 * @return uint8_t 1=已初始化，0=未初始化
 */
uint8_t MAX31856_IsInitialized(void);

/**
 * @brief 设置热电偶类型
 * @param[in] tc_type 热电偶类型
 * @return MAX31856_Status_t 错误码
 */
MAX31856_Status_t MAX31856_SetThermocoupleType(MAX31856_ThermocoupleType_t tc_type);

/**
 * @brief 获取热电偶类型
 * @param[out] tc_type 热电偶类型指针
 * @return MAX31856_Status_t 错误码
 */
MAX31856_Status_t MAX31856_GetThermocoupleType(MAX31856_ThermocoupleType_t *tc_type);

/**
 * @brief 设置采样模式
 * @param[in] avg_sel 采样模式
 * @return MAX31856_Status_t 错误码
 */
MAX31856_Status_t MAX31856_SetAvgMode(MAX31856_AvgSel_t avg_sel);

/**
 * @brief 获取采样模式
 * @param[out] avg_sel 采样模式指针
 * @return MAX31856_Status_t 错误码
 */
MAX31856_Status_t MAX31856_GetAvgMode(MAX31856_AvgSel_t *avg_sel);

/**
 * @brief 读取热电偶温度（摄氏度）
 * @param[out] temperature 温度值指针（单位：°C）
 * @return MAX31856_Status_t 错误码
 */
MAX31856_Status_t MAX31856_ReadThermocoupleTemperature(float *temperature);

/**
 * @brief 读取冷端温度（摄氏度）
 * @param[out] temperature 温度值指针（单位：°C）
 * @return MAX31856_Status_t 错误码
 */
MAX31856_Status_t MAX31856_ReadColdJunctionTemperature(float *temperature);

/**
 * @brief 读取故障状态
 * @param[out] fault_flags 故障标志指针（位掩码，多个故障可能同时存在）
 * @return MAX31856_Status_t 错误码
 * @note 故障标志位定义见 MAX31856_Fault_t 枚举
 */
MAX31856_Status_t MAX31856_ReadFault(uint8_t *fault_flags);

/**
 * @brief 检查特定故障
 * @param[in] fault_type 故障类型
 * @param[out] fault_present 故障存在标志指针（1=存在，0=不存在）
 * @return MAX31856_Status_t 错误码
 */
MAX31856_Status_t MAX31856_CheckFault(MAX31856_Fault_t fault_type, uint8_t *fault_present);

/**
 * @brief 清除故障状态
 * @return MAX31856_Status_t 错误码
 */
MAX31856_Status_t MAX31856_ClearFault(void);

/**
 * @brief 设置故障屏蔽
 * @param[in] mask_flags 屏蔽标志（位掩码，1=屏蔽，0=不屏蔽）
 * @return MAX31856_Status_t 错误码
 */
MAX31856_Status_t MAX31856_SetFaultMask(uint8_t mask_flags);

/**
 * @brief 获取故障屏蔽
 * @param[out] mask_flags 屏蔽标志指针（位掩码）
 * @return MAX31856_Status_t 错误码
 */
MAX31856_Status_t MAX31856_GetFaultMask(uint8_t *mask_flags);

/**
 * @brief 触发单次转换
 * @return MAX31856_Status_t 错误码
 * @note 触发后需要等待转换完成（约100ms）
 */
MAX31856_Status_t MAX31856_TriggerOneShot(void);

/**
 * @brief 检查转换完成状态
 * @param[out] ready 就绪标志指针（1=完成，0=未完成）
 * @return MAX31856_Status_t 错误码
 */
MAX31856_Status_t MAX31856_IsConversionReady(uint8_t *ready);

/**
 * @brief 设置过温故障阈值
 * @param[in] oc_fault 过温故障阈值
 * @return MAX31856_Status_t 错误码
 */
MAX31856_Status_t MAX31856_SetOCFault(MAX31856_OCFault_t oc_fault);

/**
 * @brief 设置故障模式
 * @param[in] fault_mode 故障模式（比较器/中断）
 * @return MAX31856_Status_t 错误码
 */
MAX31856_Status_t MAX31856_SetFaultMode(MAX31856_FaultMode_t fault_mode);

/**
 * @brief 设置冷端温度源
 * @param[in] cj_source 冷端温度源（内部/外部）
 * @return MAX31856_Status_t 错误码
 */
MAX31856_Status_t MAX31856_SetCJSource(MAX31856_CJSource_t cj_source);

/**
 * @brief 设置转换模式
 * @param[in] conv_mode 转换模式（连续/单次）
 * @return MAX31856_Status_t 错误码
 */
MAX31856_Status_t MAX31856_SetConvMode(MAX31856_ConvMode_t conv_mode);

/**
 * @brief 读取控制寄存器0（CR0）
 * @param[out] cr0_value CR0寄存器值指针
 * @return MAX31856_Status_t 错误码
 */
MAX31856_Status_t MAX31856_ReadCR0(uint8_t *cr0_value);

/**
 * @brief 读取控制寄存器1（CR1）
 * @param[out] cr1_value CR1寄存器值指针
 * @return MAX31856_Status_t 错误码
 */
MAX31856_Status_t MAX31856_ReadCR1(uint8_t *cr1_value);

#ifdef __cplusplus
}
#endif

#endif /* MAX31856_H */
