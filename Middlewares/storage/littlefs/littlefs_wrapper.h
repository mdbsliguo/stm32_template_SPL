/**
 * @file littlefs_wrapper.h
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

#ifndef LITTLEFS_WRAPPER_H
#define LITTLEFS_WRAPPER_H

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
    LITTLEFS_ERROR_INVALID_INSTANCE = ERROR_BASE_LITTLEFS - 17, /**< 无效实例编号 */
    LITTLEFS_ERROR_ALREADY_INIT = ERROR_BASE_LITTLEFS - 18,    /**< 已初始化（重复初始化） */
    LITTLEFS_ERROR_HEALTH_CHECK_FAILED = ERROR_BASE_LITTLEFS - 19, /**< 健康检查失败 */
    LITTLEFS_ERROR_BLOCK_DEVICE_ERROR = ERROR_BASE_LITTLEFS - 20, /**< 块设备错误 */
    LITTLEFS_ERROR_LOCK_FAILED = ERROR_BASE_LITTLEFS - 21,     /**< 锁获取失败（RTOS） */
    LITTLEFS_ERROR_UNLOCK_FAILED = ERROR_BASE_LITTLEFS - 22,   /**< 锁释放失败（RTOS） */
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
 * @brief LittleFS日志回调函数类型
 * @param[in] level 日志级别（0=DEBUG, 1=INFO, 2=WARN, 3=ERROR）
 * @param[in] format 格式化字符串
 * @param[in] ... 可变参数
 */
typedef void (*LittleFS_LogCallback_t)(int level, const char *format, ...);

/**
 * @brief RTOS锁回调函数类型
 * @param[in] context 用户上下文（从配置中传入）
 * @return int 0=成功，<0=失败
 */
typedef int (*LittleFS_LockCallback_t)(void *context);

/**
 * @brief RTOS解锁回调函数类型
 * @param[in] context 用户上下文（从配置中传入）
 * @return int 0=成功，<0=失败
 */
typedef int (*LittleFS_UnlockCallback_t)(void *context);

/**
 * @brief LittleFS配置结构体
 */
typedef struct {
    /* 块设备参数 */
    uint32_t read_size;          /**< 最小读取大小（字节），默认256 */
    uint32_t prog_size;          /**< 最小编程大小（字节），默认256 */
    uint32_t block_size;         /**< 块大小（字节），默认4096 */
    uint32_t block_count;        /**< 块数量，自动计算 */
    int32_t block_cycles;        /**< 磨损均衡周期，默认500（100-1000） */
    uint32_t cache_size;         /**< 缓存大小（字节），默认256 */
    uint32_t lookahead_size;     /**< 前瞻缓冲区大小（字节），自动计算，最小8 */
    
    /* 限制参数 */
    uint32_t name_max;           /**< 文件名最大长度，0=使用默认值LFS_NAME_MAX */
    uint32_t file_max;           /**< 文件最大大小，0=使用默认值LFS_FILE_MAX */
    uint32_t attr_max;           /**< 属性最大大小，0=使用默认值LFS_ATTR_MAX */
    
    /* 回调函数 */
    LittleFS_LogCallback_t log_callback;    /**< 日志回调函数，NULL=禁用日志 */
    LittleFS_LockCallback_t lock_callback;   /**< RTOS锁回调函数，NULL=禁用线程安全 */
    LittleFS_UnlockCallback_t unlock_callback; /**< RTOS解锁回调函数，NULL=禁用线程安全 */
    void *lock_context;                     /**< 锁回调函数的用户上下文 */
    
    /* 调试选项 */
    uint8_t debug_enabled;       /**< 调试模式开关，1=启用，0=禁用 */
} LittleFS_Config_t;

/**
 * @brief LittleFS实例编号
 */
typedef enum {
    LITTLEFS_INSTANCE_0 = 0,     /**< LittleFS实例0 */
    LITTLEFS_INSTANCE_1 = 1,     /**< LittleFS实例1 */
    LITTLEFS_INSTANCE_MAX        /**< 最大实例数 */
} LittleFS_Instance_t;

/**
 * @brief LittleFS健康检查结果
 */
typedef enum {
    LITTLEFS_HEALTH_OK = 0,              /**< 健康 */
    LITTLEFS_HEALTH_CORRUPT = 1,         /**< 文件系统损坏 */
    LITTLEFS_HEALTH_BLOCK_DEVICE_ERROR = 2, /**< 块设备错误 */
    LITTLEFS_HEALTH_UNKNOWN = 3          /**< 未知状态 */
} LittleFS_HealthStatus_t;

/**
 * @brief LittleFS初始化（使用默认配置）
 * @return LittleFS_Status_t 错误码
 * @note 初始化文件系统模块，检查W25Q是否已初始化，使用默认配置
 * @note 向后兼容：保留此函数以保持API兼容性
 */
LittleFS_Status_t LittleFS_Init(void);

/**
 * @brief LittleFS初始化（使用自定义配置）
 * @param[in] instance 实例编号
 * @param[in] config 配置结构体指针（NULL=使用默认配置）
 * @return LittleFS_Status_t 错误码
 * @note 初始化指定实例的文件系统模块，支持自定义配置
 */
LittleFS_Status_t LittleFS_InitWithConfig(LittleFS_Instance_t instance, const LittleFS_Config_t *config);

/**
 * @brief LittleFS反初始化（默认实例）
 * @return LittleFS_Status_t 错误码
 * @note 向后兼容：保留此函数以保持API兼容性
 */
LittleFS_Status_t LittleFS_Deinit(void);

/**
 * @brief LittleFS反初始化（指定实例）
 * @param[in] instance 实例编号
 * @return LittleFS_Status_t 错误码
 */
LittleFS_Status_t LittleFS_DeinitInstance(LittleFS_Instance_t instance);

/**
 * @brief 检查LittleFS是否已初始化（默认实例）
 * @return uint8_t 1-已初始化，0-未初始化
 * @note 向后兼容：保留此函数以保持API兼容性
 */
uint8_t LittleFS_IsInitialized(void);

/**
 * @brief 检查LittleFS是否已初始化（指定实例）
 * @param[in] instance 实例编号
 * @return uint8_t 1-已初始化，0-未初始化
 */
uint8_t LittleFS_IsInitializedInstance(LittleFS_Instance_t instance);

/**
 * @brief 挂载文件系统（默认实例）
 * @return LittleFS_Status_t 错误码
 * @note 如果文件系统损坏，返回LITTLEFS_ERROR_CORRUPT，需要先格式化
 * @note 向后兼容：保留此函数以保持API兼容性
 */
LittleFS_Status_t LittleFS_Mount(void);

/**
 * @brief 挂载文件系统（指定实例）
 * @param[in] instance 实例编号
 * @return LittleFS_Status_t 错误码
 * @note 如果文件系统损坏，返回LITTLEFS_ERROR_CORRUPT，需要先格式化
 */
LittleFS_Status_t LittleFS_MountInstance(LittleFS_Instance_t instance);

/**
 * @brief 卸载文件系统（默认实例）
 * @return LittleFS_Status_t 错误码
 * @note 向后兼容：保留此函数以保持API兼容性
 */
LittleFS_Status_t LittleFS_Unmount(void);

/**
 * @brief 卸载文件系统（指定实例）
 * @param[in] instance 实例编号
 * @return LittleFS_Status_t 错误码
 */
LittleFS_Status_t LittleFS_UnmountInstance(LittleFS_Instance_t instance);

/**
 * @brief 格式化文件系统（默认实例）
 * @return LittleFS_Status_t 错误码
 * @warning 此操作会擦除所有数据，请谨慎使用
 * @note 向后兼容：保留此函数以保持API兼容性
 */
LittleFS_Status_t LittleFS_Format(void);

/**
 * @brief 格式化文件系统（指定实例）
 * @param[in] instance 实例编号
 * @return LittleFS_Status_t 错误码
 * @warning 此操作会擦除所有数据，请谨慎使用
 */
LittleFS_Status_t LittleFS_FormatInstance(LittleFS_Instance_t instance);

/**
 * @brief 获取文件系统信息（默认实例）
 * @param[out] total_bytes 总空间（字节）
 * @param[out] free_bytes 空闲空间（字节）
 * @return LittleFS_Status_t 错误码
 * @note 向后兼容：保留此函数以保持API兼容性
 */
LittleFS_Status_t LittleFS_GetInfo(uint64_t *total_bytes, uint64_t *free_bytes);

/**
 * @brief 获取文件系统信息（指定实例）
 * @param[in] instance 实例编号
 * @param[out] total_bytes 总空间（字节）
 * @param[out] free_bytes 空闲空间（字节）
 * @return LittleFS_Status_t 错误码
 */
LittleFS_Status_t LittleFS_GetInfoInstance(LittleFS_Instance_t instance, uint64_t *total_bytes, uint64_t *free_bytes);

/**
 * @brief 文件系统健康检查（默认实例）
 * @param[out] health_status 健康状态指针
 * @return LittleFS_Status_t 错误码
 * @note 检测文件系统是否损坏、块设备是否有错误
 */
LittleFS_Status_t LittleFS_HealthCheck(LittleFS_HealthStatus_t *health_status);

/**
 * @brief 文件系统健康检查（指定实例）
 * @param[in] instance 实例编号
 * @param[out] health_status 健康状态指针
 * @return LittleFS_Status_t 错误码
 * @note 检测文件系统是否损坏、块设备是否有错误
 */
LittleFS_Status_t LittleFS_HealthCheckInstance(LittleFS_Instance_t instance, LittleFS_HealthStatus_t *health_status);

/**
 * @brief 设置日志回调函数（默认实例）
 * @param[in] callback 日志回调函数指针（NULL=禁用日志）
 * @return LittleFS_Status_t 错误码
 */
LittleFS_Status_t LittleFS_SetLogCallback(LittleFS_LogCallback_t callback);

/**
 * @brief 设置日志回调函数（指定实例）
 * @param[in] instance 实例编号
 * @param[in] callback 日志回调函数指针（NULL=禁用日志）
 * @return LittleFS_Status_t 错误码
 */
LittleFS_Status_t LittleFS_SetLogCallbackInstance(LittleFS_Instance_t instance, LittleFS_LogCallback_t callback);

/**
 * @brief 设置RTOS锁回调函数（默认实例）
 * @param[in] lock_callback 锁回调函数指针（NULL=禁用线程安全）
 * @param[in] unlock_callback 解锁回调函数指针（NULL=禁用线程安全）
 * @param[in] context 用户上下文指针
 * @return LittleFS_Status_t 错误码
 */
LittleFS_Status_t LittleFS_SetLockCallback(LittleFS_LockCallback_t lock_callback, 
                                           LittleFS_UnlockCallback_t unlock_callback, 
                                           void *context);

/**
 * @brief 设置RTOS锁回调函数（指定实例）
 * @param[in] instance 实例编号
 * @param[in] lock_callback 锁回调函数指针（NULL=禁用线程安全）
 * @param[in] unlock_callback 解锁回调函数指针（NULL=禁用线程安全）
 * @param[in] context 用户上下文指针
 * @return LittleFS_Status_t 错误码
 */
LittleFS_Status_t LittleFS_SetLockCallbackInstance(LittleFS_Instance_t instance,
                                                    LittleFS_LockCallback_t lock_callback, 
                                                    LittleFS_UnlockCallback_t unlock_callback, 
                                                    void *context);

/**
 * @brief 获取默认配置
 * @param[out] config 配置结构体指针
 * @return LittleFS_Status_t 错误码
 * @note 填充默认配置值，用户可以根据需要修改后用于初始化
 */
LittleFS_Status_t LittleFS_GetDefaultConfig(LittleFS_Config_t *config);

/* ==================== 调试支持 ==================== */

/**
 * @brief 文件系统遍历回调函数类型
 * @param[in] path 文件/目录路径
 * @param[in] info 文件信息结构体指针
 * @param[in] user_data 用户数据指针
 * @return int 0=继续遍历，非0=停止遍历
 */
typedef int (*LittleFS_TraverseCallback_t)(const char *path, const struct lfs_info *info, void *user_data);

/**
 * @brief 遍历文件系统（默认实例）
 * @param[in] root_path 根路径（如"/"）
 * @param[in] callback 遍历回调函数
 * @param[in] user_data 用户数据指针
 * @return LittleFS_Status_t 错误码
 * @note 递归遍历文件系统，对每个文件/目录调用回调函数
 */
LittleFS_Status_t LittleFS_Traverse(const char *root_path, LittleFS_TraverseCallback_t callback, void *user_data);

/**
 * @brief 遍历文件系统（指定实例）
 * @param[in] instance 实例编号
 * @param[in] root_path 根路径（如"/"）
 * @param[in] callback 遍历回调函数
 * @param[in] user_data 用户数据指针
 * @return LittleFS_Status_t 错误码
 * @note 递归遍历文件系统，对每个文件/目录调用回调函数
 */
LittleFS_Status_t LittleFS_TraverseInstance(LittleFS_Instance_t instance, const char *root_path, 
                                            LittleFS_TraverseCallback_t callback, void *user_data);

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

/* ==================== 调试支持（原始API访问） ==================== */

/**
 * @brief 获取底层LittleFS对象指针（用于调试和原始API访问）
 * @param[in] instance 实例编号（可选，默认使用LITTLEFS_INSTANCE_0）
 * @return lfs_t* LittleFS对象指针，NULL=无效实例或未初始化
 * @warning 此函数仅供调试和高级用户使用，直接操作lfs_t可能破坏封装层的状态
 * @note 使用原始API时，需要确保文件系统已挂载
 */
lfs_t* LittleFS_GetLFS(LittleFS_Instance_t instance);

/**
 * @brief 获取文件缓存缓冲区指针（用于原始API访问）
 * @param[in] instance 实例编号
 * @return uint8_t* 缓存缓冲区指针，NULL=无效实例或未初始化
 * @warning 此函数仅供调试和高级用户使用，直接操作缓冲区可能破坏封装层的状态
 * @note 缓存缓冲区大小为256字节（LITTLEFS_DEFAULT_CACHE_SIZE），4字节对齐
 */
uint8_t* LittleFS_GetCacheBuffer(LittleFS_Instance_t instance);

/**
 * @brief 获取所有LittleFS缓冲区地址（用于诊断对齐问题）
 * @param[in] instance 实例编号
 * @param[out] read_buf_addr read_buffer地址（可为NULL）
 * @param[out] prog_buf_addr prog_buffer地址（可为NULL）
 * @param[out] lookahead_buf_addr lookahead_buffer地址（可为NULL）
 * @return LittleFS_Status_t 错误码
 * @note 用于诊断缓冲区对齐问题，所有缓冲区必须4字节对齐
 */
LittleFS_Status_t LittleFS_GetBufferAddresses(LittleFS_Instance_t instance,
                                               uint32_t *read_buf_addr,
                                               uint32_t *prog_buf_addr,
                                               uint32_t *lookahead_buf_addr);

#endif /* CONFIG_MODULE_LITTLEFS_ENABLED */
#endif /* CONFIG_MODULE_LITTLEFS_ENABLED */

#ifdef __cplusplus
}
#endif

#endif /* LITTLEFS_WRAPPER_H */

