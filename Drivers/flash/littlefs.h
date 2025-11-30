/**
 * @file littlefs.h
 * @brief LittleFS文件系统驱动模块
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于littlefs核心库的文件系统驱动，支持SPI Flash设备（W25Q）
 * 
 * @note 设计约束：
 * - 直接使用littlefs核心库（lfs.c/h），不创建额外的封装层
 * - 提供标准驱动接口：Init、Deinit、IsInitialized
 * - 文件系统操作：Mount、Unmount、Format
 * - 文件操作：FileOpen、FileClose、FileRead、FileWrite、FileSeek等
 * - 目录操作：DirOpen、DirClose、DirRead、DirCreate、DirDelete等
 * - 错误码基于ERROR_BASE_LITTLEFS
 */

#ifndef LITTLEFS_H
#define LITTLEFS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include "config.h"
#include "board.h"
#include <stdint.h>

/* 模块开关检查 */
#ifdef CONFIG_MODULE_LITTLEFS_ENABLED
#if CONFIG_MODULE_LITTLEFS_ENABLED

/* 包含littlefs核心库 */
#include "lfs.h"

/* 包含W25Q驱动（必需） */
#ifdef CONFIG_MODULE_W25Q_ENABLED
#if CONFIG_MODULE_W25Q_ENABLED
#include "w25q_spi.h"
#endif
#endif

/**
 * @brief LittleFS状态枚举
 */
typedef enum {
    LITTLEFS_STATE_UNINITIALIZED = 0,  /**< 未初始化 */
    LITTLEFS_STATE_INITIALIZED = 1,    /**< 已初始化 */
    LITTLEFS_STATE_MOUNTED = 2         /**< 已挂载 */
} LittleFS_State_t;

/**
 * @brief LittleFS错误码
 */
typedef enum {
    LITTLEFS_OK = ERROR_OK,                                    /**< 操作成功 */
    LITTLEFS_ERROR_NOT_INIT = ERROR_BASE_LITTLEFS - 1,         /**< 模块未初始化 */
    LITTLEFS_ERROR_INVALID_PARAM = ERROR_BASE_LITTLEFS - 2,    /**< 参数无效 */
    LITTLEFS_ERROR_IO = ERROR_BASE_LITTLEFS - 3,                /**< IO错误 */
    LITTLEFS_ERROR_CORRUPT = ERROR_BASE_LITTLEFS - 4,          /**< 文件系统损坏 */
    LITTLEFS_ERROR_NOENT = ERROR_BASE_LITTLEFS - 5,            /**< 文件/目录不存在 */
    LITTLEFS_ERROR_EXIST = ERROR_BASE_LITTLEFS - 6,            /**< 文件/目录已存在 */
    LITTLEFS_ERROR_NOTDIR = ERROR_BASE_LITTLEFS - 7,           /**< 不是目录 */
    LITTLEFS_ERROR_ISDIR = ERROR_BASE_LITTLEFS - 8,            /**< 是目录 */
    LITTLEFS_ERROR_NOTEMPTY = ERROR_BASE_LITTLEFS - 9,         /**< 目录非空 */
    LITTLEFS_ERROR_BADF = ERROR_BASE_LITTLEFS - 10,            /**< 文件描述符无效 */
    LITTLEFS_ERROR_FBIG = ERROR_BASE_LITTLEFS - 11,            /**< 文件太大 */
    LITTLEFS_ERROR_NOSPC = ERROR_BASE_LITTLEFS - 12,           /**< 空间不足 */
    LITTLEFS_ERROR_NOMEM = ERROR_BASE_LITTLEFS - 13,           /**< 内存不足 */
    LITTLEFS_ERROR_NOATTR = ERROR_BASE_LITTLEFS - 14,          /**< 属性不存在 */
    LITTLEFS_ERROR_NAMETOOLONG = ERROR_BASE_LITTLEFS - 15,     /**< 文件名太长 */
    LITTLEFS_ERROR_NOT_MOUNTED = ERROR_BASE_LITTLEFS - 16,    /**< 文件系统未挂载 */
} LittleFS_Status_t;

/**
 * @brief LittleFS文件信息结构体
 */
typedef struct {
    uint32_t type;      /**< 文件类型（LFS_TYPE_REG或LFS_TYPE_DIR） */
    uint32_t size;      /**< 文件大小（字节） */
    char name[LFS_NAME_MAX + 1];  /**< 文件名 */
} LittleFS_Info_t;

/**
 * @brief LittleFS初始化
 * @return LittleFS_Status_t 错误码
 * @note 初始化文件系统模块，检查W25Q是否已初始化
 */
LittleFS_Status_t LittleFS_Init(void);

/**
 * @brief LittleFS反初始化
 * @return LittleFS_Status_t 错误码
 */
LittleFS_Status_t LittleFS_Deinit(void);

/**
 * @brief 检查LittleFS是否已初始化
 * @return uint8_t 1-已初始化，0-未初始化
 */
uint8_t LittleFS_IsInitialized(void);

/**
 * @brief 挂载文件系统
 * @return LittleFS_Status_t 错误码
 * @note 如果文件系统损坏，返回LITTLEFS_ERROR_CORRUPT，需要先格式化
 */
LittleFS_Status_t LittleFS_Mount(void);

/**
 * @brief 卸载文件系统
 * @return LittleFS_Status_t 错误码
 */
LittleFS_Status_t LittleFS_Unmount(void);

/**
 * @brief 格式化文件系统
 * @return LittleFS_Status_t 错误码
 * @warning 此操作会擦除所有数据，请谨慎使用
 */
LittleFS_Status_t LittleFS_Format(void);

/**
 * @brief 获取文件系统信息
 * @param[out] total_bytes 总空间（字节）
 * @param[out] free_bytes 空闲空间（字节）
 * @return LittleFS_Status_t 错误码
 */
LittleFS_Status_t LittleFS_GetInfo(uint64_t *total_bytes, uint64_t *free_bytes);

/* ==================== 文件操作 ==================== */

/**
 * @brief 打开文件
 * @param[out] file 文件句柄指针
 * @param[in] path 文件路径（绝对路径，如"/test.txt"）
 * @param[in] flags 打开标志（LFS_O_RDONLY、LFS_O_WRONLY、LFS_O_RDWR等）
 * @return LittleFS_Status_t 错误码
 */
LittleFS_Status_t LittleFS_FileOpen(lfs_file_t *file, const char *path, int flags);

/**
 * @brief 关闭文件
 * @param[in] file 文件句柄指针
 * @return LittleFS_Status_t 错误码
 */
LittleFS_Status_t LittleFS_FileClose(lfs_file_t *file);

/**
 * @brief 读取文件
 * @param[in] file 文件句柄指针
 * @param[out] buffer 数据缓冲区
 * @param[in] size 读取大小（字节）
 * @param[out] bytes_read 实际读取的字节数（可为NULL）
 * @return LittleFS_Status_t 错误码
 */
LittleFS_Status_t LittleFS_FileRead(lfs_file_t *file, void *buffer, uint32_t size, uint32_t *bytes_read);

/**
 * @brief 写入文件
 * @param[in] file 文件句柄指针
 * @param[in] buffer 数据缓冲区
 * @param[in] size 写入大小（字节）
 * @param[out] bytes_written 实际写入的字节数（可为NULL）
 * @return LittleFS_Status_t 错误码
 */
LittleFS_Status_t LittleFS_FileWrite(lfs_file_t *file, const void *buffer, uint32_t size, uint32_t *bytes_written);

/**
 * @brief 文件定位
 * @param[in] file 文件句柄指针
 * @param[in] offset 偏移量
 * @param[in] whence 定位方式（LFS_SEEK_SET、LFS_SEEK_CUR、LFS_SEEK_END）
 * @return LittleFS_Status_t 错误码
 */
LittleFS_Status_t LittleFS_FileSeek(lfs_file_t *file, int32_t offset, int whence);

/**
 * @brief 获取文件大小
 * @param[in] file 文件句柄指针
 * @param[out] size 文件大小（字节）
 * @return LittleFS_Status_t 错误码
 */
LittleFS_Status_t LittleFS_FileSize(lfs_file_t *file, uint32_t *size);

/**
 * @brief 截断文件
 * @param[in] file 文件句柄指针
 * @param[in] size 新文件大小（字节）
 * @return LittleFS_Status_t 错误码
 */
LittleFS_Status_t LittleFS_FileTruncate(lfs_file_t *file, uint32_t size);

/**
 * @brief 同步文件（确保数据写入Flash）
 * @param[in] file 文件句柄指针
 * @return LittleFS_Status_t 错误码
 */
LittleFS_Status_t LittleFS_FileSync(lfs_file_t *file);

/**
 * @brief 删除文件
 * @param[in] path 文件路径
 * @return LittleFS_Status_t 错误码
 */
LittleFS_Status_t LittleFS_FileDelete(const char *path);

/**
 * @brief 重命名文件
 * @param[in] old_path 旧路径
 * @param[in] new_path 新路径
 * @return LittleFS_Status_t 错误码
 */
LittleFS_Status_t LittleFS_FileRename(const char *old_path, const char *new_path);

/* ==================== 目录操作 ==================== */

/**
 * @brief 打开目录
 * @param[out] dir 目录句柄指针
 * @param[in] path 目录路径（绝对路径，如"/"或"/testdir"）
 * @return LittleFS_Status_t 错误码
 */
LittleFS_Status_t LittleFS_DirOpen(lfs_dir_t *dir, const char *path);

/**
 * @brief 关闭目录
 * @param[in] dir 目录句柄指针
 * @return LittleFS_Status_t 错误码
 */
LittleFS_Status_t LittleFS_DirClose(lfs_dir_t *dir);

/**
 * @brief 读取目录项
 * @param[in] dir 目录句柄指针
 * @param[out] info 文件信息结构体指针
 * @return LittleFS_Status_t 错误码
 * @note 返回LITTLEFS_ERROR_NOENT表示目录读取完毕
 */
LittleFS_Status_t LittleFS_DirRead(lfs_dir_t *dir, struct lfs_info *info);

/**
 * @brief 创建目录
 * @param[in] path 目录路径
 * @return LittleFS_Status_t 错误码
 */
LittleFS_Status_t LittleFS_DirCreate(const char *path);

/**
 * @brief 删除目录
 * @param[in] path 目录路径
 * @return LittleFS_Status_t 错误码
 * @note 目录必须为空才能删除
 */
LittleFS_Status_t LittleFS_DirDelete(const char *path);

/* ==================== 文件属性操作 ==================== */

/**
 * @brief 设置文件属性
 * @param[in] path 文件路径
 * @param[in] type 属性类型（用户自定义，0x100-0x1FF）
 * @param[in] buffer 属性数据缓冲区
 * @param[in] size 属性数据大小（字节）
 * @return LittleFS_Status_t 错误码
 */
LittleFS_Status_t LittleFS_FileSetAttr(const char *path, uint8_t type, const void *buffer, uint32_t size);

/**
 * @brief 获取文件属性
 * @param[in] path 文件路径
 * @param[in] type 属性类型
 * @param[out] buffer 属性数据缓冲区
 * @param[in] size 缓冲区大小（字节）
 * @param[out] actual_size 实际属性数据大小（可为NULL）
 * @return LittleFS_Status_t 错误码
 */
LittleFS_Status_t LittleFS_FileGetAttr(const char *path, uint8_t type, void *buffer, uint32_t size, uint32_t *actual_size);

/**
 * @brief 移除文件属性
 * @param[in] path 文件路径
 * @param[in] type 属性类型
 * @return LittleFS_Status_t 错误码
 */
LittleFS_Status_t LittleFS_FileRemoveAttr(const char *path, uint8_t type);

#endif /* CONFIG_MODULE_LITTLEFS_ENABLED */
#endif /* CONFIG_MODULE_LITTLEFS_ENABLED */

#ifdef __cplusplus
}
#endif

#endif /* LITTLEFS_H */

