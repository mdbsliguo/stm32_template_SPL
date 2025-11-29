/**
 * @file flash09_app.h
 * @brief Flash09最小化版本业务逻辑层头文件
 * @details 封装Flash09案例的业务逻辑，保持main函数简洁
 * @note 最小化版本：保留OLED显示，禁用UART、Log等模块，只保留核心功能
 */

#ifndef FLASH09_APP_H
#define FLASH09_APP_H

#include "fatfs_wrapper.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Flash09应用状态枚举
 */
typedef enum {
    FLASH09_APP_OK = 0,                    /**< 操作成功 */
    FLASH09_APP_ERROR_INIT = -1,          /**< 初始化失败 */
    FLASH09_APP_ERROR_SD_CARD = -2,       /**< SD卡错误 */
    FLASH09_APP_ERROR_MOUNT = -3,         /**< 挂载失败 */
} Flash09_AppStatus_t;

/**
 * @brief 初始化Flash09应用（系统初始化）
 * @return Flash09_AppStatus_t 状态码
 * @note 初始化系统、LED、OLED、SPI等核心模块（最小化版本）
 */
Flash09_AppStatus_t Flash09_AppInit(void);

/**
 * @brief 初始化SD卡（检测并初始化）
 * @return Flash09_AppStatus_t 状态码
 * @note 检测SD卡是否存在，检查是否满足使用要求
 */
Flash09_AppStatus_t Flash09_InitSDCard(void);

/**
 * @brief 挂载文件系统
 * @param[out] mount_path 挂载路径（输出，如"0:"）
 * @return Flash09_AppStatus_t 状态码
 * @note 尝试挂载文件系统，如果无文件系统则自动格式化
 */
Flash09_AppStatus_t Flash09_MountFileSystem(char* mount_path, uint32_t path_size);

/**
 * @brief 显示文件系统信息
 * @param[in] mount_path 挂载路径
 * @note 在OLED上显示文件系统总空间和空闲空间
 */
void Flash09_ShowFileSystemInfo(const char* mount_path);

/**
 * @brief 运行主循环（测试插拔卡检测与挂载）
 * @param[in] mount_path 挂载路径
 * @param[in] loop_duration_ms 循环持续时间（毫秒）
 * @return Flash09_AppStatus_t 状态码
 */
Flash09_AppStatus_t Flash09_RunMainLoop(const char* mount_path, uint32_t loop_duration_ms);

/**
 * @brief 程序结束流程（倒计时、清除分区表）
 * @return Flash09_AppStatus_t 状态码
 */
Flash09_AppStatus_t Flash09_Shutdown(void);

#endif /* FLASH09_APP_H */

