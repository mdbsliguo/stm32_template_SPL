/**
 * @file fatfs_wrapper.h
 * @brief FatFS封装层头文件
 * @version 1.0.0
 * @date 2024-01-01
 * @details 提供统一的FatFS接口，封装FatFS底层API，统一错误码转换
 */

#ifndef FATFS_WRAPPER_H
#define FATFS_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"

#ifdef CONFIG_MODULE_FATFS_ENABLED
#if CONFIG_MODULE_FATFS_ENABLED

#include "error_code.h"
#include "ff.h"
#include <stdint.h>

/**
 * @brief FatFS卷类型枚举
 */
typedef enum {
    FATFS_VOLUME_SPI = 0,   /**< SPI接口卷（卷0） */
    FATFS_VOLUME_SDIO = 1   /**< SDIO接口卷（卷1） */
} FatFS_Volume_t;

/**
 * @brief FatFS错误码
 */
typedef enum {
    FATFS_OK = ERROR_OK,                                    /**< 操作成功 */
    FATFS_ERROR_NOT_IMPLEMENTED = ERROR_BASE_FATFS - 99,  /**< 功能未实现（占位空函数） */
    FATFS_ERROR_NULL_PTR = ERROR_BASE_FATFS - 1,          /**< 空指针错误 */
    FATFS_ERROR_INVALID_PARAM = ERROR_BASE_FATFS - 2,     /**< 无效参数 */
    FATFS_ERROR_INVALID_VOLUME = ERROR_BASE_FATFS - 3,    /**< 无效卷号 */
    FATFS_ERROR_NOT_MOUNTED = ERROR_BASE_FATFS - 4,       /**< 卷未挂载 */
    FATFS_ERROR_DISK_ERROR = ERROR_BASE_FATFS - 5,        /**< 磁盘错误 */
    FATFS_ERROR_NOT_READY = ERROR_BASE_FATFS - 6,         /**< 磁盘未就绪 */
    FATFS_ERROR_NO_FILE = ERROR_BASE_FATFS - 7,           /**< 文件不存在 */
    FATFS_ERROR_NO_PATH = ERROR_BASE_FATFS - 8,           /**< 路径不存在 */
    FATFS_ERROR_INVALID_NAME = ERROR_BASE_FATFS - 9,      /**< 无效文件名 */
    FATFS_ERROR_DENIED = ERROR_BASE_FATFS - 10,           /**< 访问被拒绝 */
    FATFS_ERROR_EXIST = ERROR_BASE_FATFS - 11,            /**< 文件已存在 */
    FATFS_ERROR_INVALID_OBJECT = ERROR_BASE_FATFS - 12,   /**< 无效对象 */
    FATFS_ERROR_WRITE_PROTECTED = ERROR_BASE_FATFS - 13,  /**< 写保护 */
    FATFS_ERROR_INVALID_DRIVE = ERROR_BASE_FATFS - 14,    /**< 无效驱动器 */
    FATFS_ERROR_NOT_ENABLED = ERROR_BASE_FATFS - 15,      /**< 未启用 */
    FATFS_ERROR_NO_FILESYSTEM = ERROR_BASE_FATFS - 16,    /**< 无文件系统 */
    FATFS_ERROR_TIMEOUT = ERROR_BASE_FATFS - 17,          /**< 超时 */
    FATFS_ERROR_LOCKED = ERROR_BASE_FATFS - 18,           /**< 锁定 */
    FATFS_ERROR_NOT_ENOUGH_CORE = ERROR_BASE_FATFS - 19,  /**< 内存不足 */
    FATFS_ERROR_TOO_MANY_OPEN_FILES = ERROR_BASE_FATFS - 20, /**< 打开文件过多 */
    FATFS_ERROR_INVALID_PARAMETER = ERROR_BASE_FATFS - 21  /**< 无效参数 */
} FatFS_Status_t;

/* ==================== 文件系统操作 ==================== */

/**
 * @brief 挂载文件系统
 * @param[in] volume 卷类型（FATFS_VOLUME_SPI或FATFS_VOLUME_SDIO）
 * @param[in] path 卷路径（如"0:"或"1:"）
 * @return FatFS_Status_t 错误码
 */
FatFS_Status_t FatFS_Mount(FatFS_Volume_t volume, const char* path);

/**
 * @brief 卸载文件系统
 * @param[in] volume 卷类型
 * @return FatFS_Status_t 错误码
 */
FatFS_Status_t FatFS_Unmount(FatFS_Volume_t volume);

/**
 * @brief 格式化文件系统
 * @param[in] volume 卷类型
 * @param[in] path 卷路径
 * @return FatFS_Status_t 错误码
 * @note 需要启用FF_USE_MKFS功能
 */
FatFS_Status_t FatFS_Format(FatFS_Volume_t volume, const char* path);

/* ==================== 文件操作 ==================== */

/**
 * @brief 打开文件
 * @param[out] file 文件对象指针
 * @param[in] path 文件路径
 * @param[in] mode 打开模式（FA_READ, FA_WRITE, FA_OPEN_ALWAYS等）
 * @return FatFS_Status_t 错误码
 */
FatFS_Status_t FatFS_FileOpen(FIL* file, const char* path, uint8_t mode);

/**
 * @brief 关闭文件
 * @param[in] file 文件对象指针
 * @return FatFS_Status_t 错误码
 */
FatFS_Status_t FatFS_FileClose(FIL* file);

/**
 * @brief 读取文件
 * @param[in] file 文件对象指针
 * @param[out] buff 数据缓冲区
 * @param[in] btr 要读取的字节数
 * @param[out] br 实际读取的字节数（可为NULL）
 * @return FatFS_Status_t 错误码
 */
FatFS_Status_t FatFS_FileRead(FIL* file, void* buff, uint32_t btr, uint32_t* br);

/**
 * @brief 写入文件
 * @param[in] file 文件对象指针
 * @param[in] buff 数据缓冲区
 * @param[in] btw 要写入的字节数
 * @param[out] bw 实际写入的字节数（可为NULL）
 * @return FatFS_Status_t 错误码
 */
FatFS_Status_t FatFS_FileWrite(FIL* file, const void* buff, uint32_t btw, uint32_t* bw);

/**
 * @brief 定位文件指针
 * @param[in] file 文件对象指针
 * @param[in] ofs 偏移量
 * @return FatFS_Status_t 错误码
 */
FatFS_Status_t FatFS_FileSeek(FIL* file, uint32_t ofs);

/**
 * @brief 截断文件
 * @param[in] file 文件对象指针
 * @return FatFS_Status_t 错误码
 */
FatFS_Status_t FatFS_FileTruncate(FIL* file);

/**
 * @brief 同步文件（刷新缓存）
 * @param[in] file 文件对象指针
 * @return FatFS_Status_t 错误码
 */
FatFS_Status_t FatFS_FileSync(FIL* file);

/**
 * @brief 删除文件
 * @param[in] path 文件路径
 * @return FatFS_Status_t 错误码
 */
FatFS_Status_t FatFS_FileDelete(const char* path);

/**
 * @brief 重命名文件
 * @param[in] path_old 旧路径
 * @param[in] path_new 新路径
 * @return FatFS_Status_t 错误码
 */
FatFS_Status_t FatFS_FileRename(const char* path_old, const char* path_new);

/* ==================== 目录操作 ==================== */

/**
 * @brief 打开目录
 * @param[out] dir 目录对象指针
 * @param[in] path 目录路径
 * @return FatFS_Status_t 错误码
 */
FatFS_Status_t FatFS_DirOpen(DIR* dir, const char* path);

/**
 * @brief 关闭目录
 * @param[in] dir 目录对象指针
 * @return FatFS_Status_t 错误码
 */
FatFS_Status_t FatFS_DirClose(DIR* dir);

/**
 * @brief 读取目录项
 * @param[in] dir 目录对象指针
 * @param[out] fno 文件信息结构体指针
 * @return FatFS_Status_t 错误码
 */
FatFS_Status_t FatFS_DirRead(DIR* dir, FILINFO* fno);

/**
 * @brief 创建目录
 * @param[in] path 目录路径
 * @return FatFS_Status_t 错误码
 */
FatFS_Status_t FatFS_DirCreate(const char* path);

/**
 * @brief 删除目录
 * @param[in] path 目录路径
 * @return FatFS_Status_t 错误码
 */
FatFS_Status_t FatFS_DirDelete(const char* path);

/* ==================== 文件系统信息 ==================== */

/**
 * @brief 获取空闲空间
 * @param[in] volume 卷类型
 * @param[in] path 卷路径
 * @param[out] free_clusters 空闲簇数（可为NULL）
 * @param[out] total_clusters 总簇数（可为NULL）
 * @return FatFS_Status_t 错误码
 */
FatFS_Status_t FatFS_GetFreeSpace(FatFS_Volume_t volume, const char* path, 
                                   uint32_t* free_clusters, uint32_t* total_clusters);

/**
 * @brief 获取总空间
 * @param[in] volume 卷类型
 * @param[in] path 卷路径
 * @param[out] total_bytes 总字节数（可为NULL，使用uint64_t支持最大16EB）
 * @return FatFS_Status_t 错误码
 */
FatFS_Status_t FatFS_GetTotalSpace(FatFS_Volume_t volume, const char* path, uint64_t* total_bytes);

#endif /* CONFIG_MODULE_FATFS_ENABLED */
#endif /* CONFIG_MODULE_FATFS_ENABLED */

#ifdef __cplusplus
}
#endif

#endif /* FATFS_WRAPPER_H */

