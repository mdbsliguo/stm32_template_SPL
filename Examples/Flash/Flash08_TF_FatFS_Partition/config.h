/**
 * @file config.h
 * @brief Flash08专用配置（独立工程）
 * @details 只启用Flash08需要的模块，禁用其他模块以减小代码体积
 * @note Flash08需要FF_MULTI_PARTITION和FF_USE_MKFS支持
 */

#ifndef EXAMPLE_CONFIG_H
#define EXAMPLE_CONFIG_H

/* ==================== 模块开关 ==================== */

/* 驱动层模块 */
#define CONFIG_MODULE_GPIO_ENABLED          1   /**< GPIO模块开关 - 必须（LED、SPI、软件I2C、OLED依赖） */
#define CONFIG_MODULE_LED_ENABLED           1   /**< LED模块开关 - 必须（Flash08使用） */
#define CONFIG_MODULE_OLED_ENABLED         1   /**< OLED模块开关 - 必须（Flash08使用，默认显示器） */
#define CONFIG_MODULE_SPI_ENABLED           1   /**< SPI模块开关 - 必须（TF_SPI使用SPI2） */
#define CONFIG_MODULE_TF_SPI_ENABLED        1   /**< TF_SPI模块开关 - 必须（Flash08使用） */
#define CONFIG_MODULE_FATFS_ENABLED         1   /**< FatFS模块开关 - 必须（Flash08使用） */
#define CONFIG_MODULE_FATFS_SPI_ENABLED     1   /**< FatFS SPI接口开关 - 必须（Flash08使用） */
#define CONFIG_MODULE_FATFS_SDIO_ENABLED    0   /**< FatFS SDIO接口开关 - 禁用（Flash08不使用） */
#define CONFIG_MODULE_UART_ENABLED          1   /**< UART模块开关 - 必须（Flash08使用，详细日志输出） */
#define CONFIG_MODULE_SOFT_I2C_ENABLED     1   /**< 软件I2C模块开关 - 必须（OLED使用软件I2C） */
#define CONFIG_MODULE_I2C_ENABLED           0   /**< 硬件I2C模块开关 - 禁用（Flash08使用软件I2C） */

/* 系统层模块 */
#define CONFIG_MODULE_DELAY_ENABLED        1   /**< 延时模块开关 - 必须（Flash08使用） */
#define CONFIG_MODULE_BASE_TIMER_ENABLED   1   /**< 基时定时器模块开关 - 必须（delay依赖） */
#define CONFIG_MODULE_SYSTEM_INIT_ENABLED   1   /**< 系统初始化模块开关 - 必须（Flash08使用） */
#define CONFIG_MODULE_IWDG_ENABLED         0   /**< 独立看门狗模块开关 - 禁用（Flash08不使用） */

/* 通用模块 */
#define CONFIG_MODULE_ERROR_HANDLER_ENABLED 1   /**< 错误处理模块开关 - 必须（Flash08使用） */
#define CONFIG_MODULE_LOG_ENABLED           1   /**< 日志模块开关 - 必须（Flash08使用） */
#define CONFIG_MODULE_MODULE_CTRL_ENABLED   0   /**< 模块开关总控开关 - 禁用（Flash08不使用） */

/* ==================== 功能开关 ==================== */

/* 错误处理模块功能 */
#define CONFIG_ERROR_HANDLER_STATS_EN       1   /**< 错误统计功能开关 */

/* 日志模块功能 */
#define CONFIG_LOG_LEVEL                    1   /**< 日志级别：0=DEBUG, 1=INFO, 2=WARN, 3=ERROR, 4=NONE */
#define CONFIG_LOG_TIMESTAMP_EN             0   /**< 时间戳功能开关（需要TIM2_TimeBase模块） */

/**
 * @brief 文件系统详细调试信息开关
 * @note 1=启用详细调试信息（MBR读取、disk_ioctl检查等），0=仅显示基本信息
 * @note 生产环境建议设置为0，调试时设置为1
 */
#define FATFS_DETAILED_DEBUG                0   /**< 默认启用，生产环境可关闭 */
#define CONFIG_LOG_MODULE_EN                1   /**< 模块名显示开关 */
#define CONFIG_LOG_COLOR_EN                 0   /**< 颜色输出开关（需要终端支持ANSI转义码） */

/* TF_SPI模块调试开关 */
#define TF_SPI_DEBUG_ENABLED                0   /**< TF_SPI调试输出开关 - 启用以查看CSD原始数据和容量计算过程 */

/* ==================== FatFS格式化配置 ==================== */
/**
 * @brief 强制格式化开关（仅用于调试）
 * @note 生产环境必须设置为0，只有调试时手动改为1
 * @note 1=强制格式化（优先级最高，直接格式化，仅检查保护标志），0=自动检测（有文件系统则挂载，无则格式化）
 * @warning ?? 警告：启用此选项将清空SD卡所有数据！
 */
#define FATFS_FORCE_FORMAT                  0   /**< 默认关闭，调试时手动改为1 */

/* ==================== FatFS分区配置 ==================== */

/**
 * @brief STM32直接访问区域大小（MB）
 * 此区域不格式化，STM32可以直接通过扇区地址访问
 * 位置：扇区2048开始，大小为FATFS_MCU_DIRECT_AREA_MB MB
 * FatFS完全不管这个区域
 */
#define FATFS_MCU_DIRECT_AREA_MB             100   /**< STM32直接访问区域：100MB */

/**
 * @brief 保留区大小（扇区）
 * 扇区1-2047：保留区（约1MB），对齐预留，避免覆盖MBR
 */
#define FATFS_RESERVED_AREA_SECTORS           2047  /**< 保留区：2047扇区（约1MB） */

/**
 * @brief FAT32分区起始扇区
 * 计算：MBR(1) + 保留区(2047) + STM32直接访问区(100MB) = 1 + 2047 + 204800 = 206848
 */
#define FATFS_PARTITION_START_SECTOR          206848  /**< FAT32分区起始扇区 */

/* ==================== FatFS挂载配置 ==================== */
/**
 * @brief 挂载分区配置（MBR分区编号）
 * 由于只有一个FAT32分区，固定挂载分区1
 */
#define FATFS_MOUNT_PARTITION                1   /**< 挂载分区1（唯一的FAT32分区） */

#endif /* EXAMPLE_CONFIG_H */

