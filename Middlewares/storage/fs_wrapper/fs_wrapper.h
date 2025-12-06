/**
 * @file fs_wrapper.h
 * @brief 文件系统应用层封装模块
 * @version 1.0.0
 * @date 2024-01-01
 * @details 在littlefs_wrapper基础上提供更便捷的应用层接口，解决路径管理、错误处理内聚和代码简化问题
 * 
 * @note 设计原则
 * - 轻量级封装（约100-200行代码）
 * - 路径集中管理，避免硬编码
 * - 错误处理内聚，简化业务代码
 * - 自动文件句柄管理
 * - 写入操作自动同步
 */

#ifndef FS_WRAPPER_H
#define FS_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include "config.h"
#include <stdint.h>

/* 模块开关检查 */
#ifdef CONFIG_MODULE_FS_WRAPPER_ENABLED
#if CONFIG_MODULE_FS_WRAPPER_ENABLED

/* 依赖littlefs_wrapper */
#ifdef CONFIG_MODULE_LITTLEFS_ENABLED
#if CONFIG_MODULE_LITTLEFS_ENABLED
#include "littlefs_wrapper.h"
#endif
#endif

/**
 * @brief 文件系统目录枚举
 */
typedef enum {
    FS_DIR_FONT,      /**< 字库目录 */
    FS_DIR_CONFIG,    /**< 配置目录 */
    FS_DIR_LOG,       /**< 日志目录 */
    FS_DIR_UI,        /**< UI资源目录 */
    FS_DIR_UPDATE,    /**< 更新目录 */
    FS_DIR_MAX        /**< 最大目录数 */
} fs_dir_t;

/**
 * @brief FS_WRAPPER状态枚举
 */
typedef enum {
    FS_WRAPPER_OK = ERROR_OK,                                    /**< 操作成功 */
    FS_WRAPPER_ERROR_NOT_INIT = ERROR_BASE_FS_WRAPPER - 1,      /**< 模块未初始化 */
    FS_WRAPPER_ERROR_INVALID_PARAM = ERROR_BASE_FS_WRAPPER - 2, /**< 无效参数 */
    FS_WRAPPER_ERROR_INVALID_DIR = ERROR_BASE_FS_WRAPPER - 3,   /**< 无效目录 */
    FS_WRAPPER_ERROR_NULL_PTR = ERROR_BASE_FS_WRAPPER - 4,       /**< 空指针 */
    FS_WRAPPER_ERROR_READ_FAILED = ERROR_BASE_FS_WRAPPER - 5,   /**< 读取失败 */
    FS_WRAPPER_ERROR_WRITE_FAILED = ERROR_BASE_FS_WRAPPER - 6,  /**< 写入失败 */
    FS_WRAPPER_ERROR_SYNC_FAILED = ERROR_BASE_FS_WRAPPER - 7,   /**< 同步失败 */
    FS_WRAPPER_ERROR_LITTLEFS = ERROR_BASE_FS_WRAPPER - 8        /**< LittleFS错误 */
} fs_wrapper_status_t;

/**
 * @brief 初始化文件系统封装模块
 * @return error_code_t 错误码，FS_WRAPPER_OK表示成功
 * @note 内部自动处理LittleFS的初始化、挂载和格式化
 * @note 如果挂载失败，会自动格式化并重新挂载
 */
error_code_t FS_Init(void);

/**
 * @brief 读取文件
 * @param[in] dir 目录枚举
 * @param[in] name 文件名（不含路径）
 * @param[in] offset 读取偏移量（字节）
 * @param[out] buf 数据缓冲区
 * @param[in] size 读取大小（字节）
 * @return error_code_t 错误码，FS_WRAPPER_OK表示成功
 * @note 内部自动管理文件句柄，业务代码无需关心
 */
error_code_t FS_ReadFile(fs_dir_t dir, const char* name, uint32_t offset, 
                         void* buf, uint32_t size);

/**
 * @brief 写入文件
 * @param[in] dir 目录枚举
 * @param[in] name 文件名（不含路径）
 * @param[in] buf 数据缓冲区
 * @param[in] size 写入大小（字节）
 * @return error_code_t 错误码，FS_WRAPPER_OK表示成功
 * @note 内部自动管理文件句柄，写入后自动同步
 * @note 如果文件不存在会自动创建，如果存在会覆盖
 */
error_code_t FS_WriteFile(fs_dir_t dir, const char* name, 
                          const void* buf, uint32_t size);

/**
 * @brief 追加数据到文件
 * @param[in] dir 目录枚举
 * @param[in] name 文件名（不含路径）
 * @param[in] buf 数据缓冲区
 * @param[in] size 写入大小（字节）
 * @return error_code_t 错误码，FS_WRAPPER_OK表示成功
 * @note 内部自动管理文件句柄，写入后自动同步
 * @note 如果文件不存在会自动创建，数据追加到文件末尾
 * @note 用于大文件分段写入（如通过UART传输字库文件）
 */
error_code_t FS_AppendFile(fs_dir_t dir, const char* name, 
                           const void* buf, uint32_t size);

/**
 * @brief 获取文件路径
 * @param[in] dir 目录枚举
 * @param[in] name 文件名（不含路径）
 * @return const char* 完整路径字符串，NULL表示参数无效
 * @note 返回的路径格式：/dir_name/file_name
 * @note 路径字符串存储在静态缓冲区中，不要修改返回值
 */
const char* FS_GetPath(fs_dir_t dir, const char* name);

/**
 * @brief 检查模块是否已初始化
 * @return uint8_t 1-已初始化，0-未初始化
 */
uint8_t FS_IsInitialized(void);

#endif /* CONFIG_MODULE_FS_WRAPPER_ENABLED */
#endif /* CONFIG_MODULE_FS_WRAPPER_ENABLED */

#ifdef __cplusplus
}
#endif

#endif /* FS_WRAPPER_H */
