/**
 * @file flash08_app.h
 * @brief Flash08业务逻辑层头文件
 * @details 封装Flash08案例的业务逻辑，保持main函数简洁
 */

#ifndef FLASH08_APP_H
#define FLASH08_APP_H

#include "fatfs_wrapper.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Flash08应用状态枚举
 */
typedef enum {
    FLASH08_APP_OK = 0,                    /**< 操作成功 */
    FLASH08_APP_ERROR_INIT = -1,          /**< 初始化失败 */
    FLASH08_APP_ERROR_SD_CARD = -2,       /**< SD卡错误 */
    FLASH08_APP_ERROR_MOUNT = -3,         /**< 挂载失败 */
} Flash08_AppStatus_t;

/**
 * @brief 初始化Flash08应用（系统初始化）
 * @return Flash08_AppStatus_t 状态码
 * @note 初始化系统、UART、Debug、Log、LED、I2C、OLED、SPI等模块
 */
Flash08_AppStatus_t Flash08_AppInit(void);

/**
 * @brief 初始化SD卡（检测并初始化）
 * @return Flash08_AppStatus_t 状态码
 * @note 检测SD卡是否存在，检查是否满足使用要求
 */
Flash08_AppStatus_t Flash08_InitSDCard(void);

/**
 * @brief 挂载文件系统
 * @param[out] mount_path 挂载路径（输出，如"0:"）
 * @return Flash08_AppStatus_t 状态码
 * @note 尝试挂载文件系统，如果无文件系统则自动格式化
 */
Flash08_AppStatus_t Flash08_MountFileSystem(char* mount_path, uint32_t path_size);

/**
 * @brief 显示文件系统信息
 * @param[in] mount_path 挂载路径
 */
void Flash08_ShowFileSystemInfo(const char* mount_path);

/**
 * @brief 运行所有测试
 * @return Flash08_AppStatus_t 状态码
 * @note 执行目录创建、文件操作、重命名、删除、MCU区域边界测试
 */
Flash08_AppStatus_t Flash08_RunTests(void);

/**
 * @brief 运行主循环（测试插拔卡检测与挂载）
 * @param[in] mount_path 挂载路径
 * @param[in] loop_duration_ms 循环持续时间（毫秒）
 * @return Flash08_AppStatus_t 状态码
 */
Flash08_AppStatus_t Flash08_RunMainLoop(const char* mount_path, uint32_t loop_duration_ms);

/**
 * @brief 程序结束流程（倒计时、清除分区表）
 * @return Flash08_AppStatus_t 状态码
 */
Flash08_AppStatus_t Flash08_Shutdown(void);

#endif /* FLASH08_APP_H */


