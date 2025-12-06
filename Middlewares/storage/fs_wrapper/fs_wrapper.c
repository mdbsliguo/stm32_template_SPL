/**
 * @file fs_wrapper.c
 * @brief 文件系统应用层封装模块实现
 * @version 1.0.0
 * @date 2024-01-01
 * @details 在littlefs_wrapper基础上提供更便捷的应用层接口
 */

#include "fs_wrapper.h"
#include "config.h"
#include <string.h>
#include <stdio.h>

/* 条件编译：错误处理模块 */
#ifdef CONFIG_MODULE_ERROR_HANDLER_ENABLED
#if CONFIG_MODULE_ERROR_HANDLER_ENABLED
#include "error_handler.h"
#endif
#endif

#ifdef CONFIG_MODULE_FS_WRAPPER_ENABLED
#if CONFIG_MODULE_FS_WRAPPER_ENABLED

#ifdef CONFIG_MODULE_LITTLEFS_ENABLED
#if CONFIG_MODULE_LITTLEFS_ENABLED

/* ==================== 内部定义 ==================== */

/* 目录名称映射表 */
static const char* g_dir_names[FS_DIR_MAX] = {
    "font",      /* FS_DIR_FONT */
    "config",    /* FS_DIR_CONFIG */
    "log",       /* FS_DIR_LOG */
    "ui",        /* FS_DIR_UI */
    "update"     /* FS_DIR_UPDATE */
};

/* 模块初始化状态 */
static uint8_t g_fs_wrapper_initialized = 0;

/* 路径缓冲区（用于FS_GetPath） */
#define FS_PATH_BUFFER_SIZE 64
static char g_path_buffer[FS_PATH_BUFFER_SIZE];

/* ==================== 内部函数 ==================== */

/**
 * @brief 获取目录名称
 * @param[in] dir 目录枚举
 * @return const char* 目录名称，NULL表示无效
 */
static const char* fs_get_dir_name(fs_dir_t dir)
{
    if (dir >= FS_DIR_MAX) {
        return NULL;
    }
    return g_dir_names[dir];
}

/* ==================== 公共接口实现 ==================== */

/**
 * @brief 初始化文件系统封装模块
 */
error_code_t FS_Init(void)
{
    /* 检查是否已初始化 */
    if (g_fs_wrapper_initialized) {
        return FS_WRAPPER_OK;
    }
    
    /* 检查LittleFS是否已初始化 */
    if (!LittleFS_IsInitialized()) {
        LittleFS_Status_t status = LittleFS_Init();
        if (status != LITTLEFS_OK) {
            #ifdef CONFIG_MODULE_ERROR_HANDLER_ENABLED
            #if CONFIG_MODULE_ERROR_HANDLER_ENABLED
            ErrorHandler_Handle(status, "FS_WRAPPER");
            #endif
            #endif
            return FS_WRAPPER_ERROR_LITTLEFS;
        }
    }
    
    /* 尝试挂载文件系统 */
    LittleFS_Status_t mount_status = LittleFS_Mount();
    
    if (mount_status != LITTLEFS_OK) {
        /* 只在文件系统损坏或首次使用时格式化，其他错误（如IO错误）不格式化 */
        if (mount_status == LITTLEFS_ERROR_CORRUPT) {
            /* 文件系统损坏，尝试格式化恢复 */
            LittleFS_Status_t format_status = LittleFS_Format();
            if (format_status != LITTLEFS_OK) {
                #ifdef CONFIG_MODULE_ERROR_HANDLER_ENABLED
                #if CONFIG_MODULE_ERROR_HANDLER_ENABLED
                ErrorHandler_Handle(format_status, "FS_WRAPPER");
                #endif
                #endif
                return FS_WRAPPER_ERROR_LITTLEFS;
            }
            
            /* 格式化成功后重新挂载 */
            mount_status = LittleFS_Mount();
            if (mount_status != LITTLEFS_OK) {
                #ifdef CONFIG_MODULE_ERROR_HANDLER_ENABLED
                #if CONFIG_MODULE_ERROR_HANDLER_ENABLED
                ErrorHandler_Handle(mount_status, "FS_WRAPPER");
                #endif
                #endif
                return FS_WRAPPER_ERROR_LITTLEFS;
            }
        } else {
            /* 其他错误（如IO错误、配置错误等），不格式化，直接返回错误 */
            #ifdef CONFIG_MODULE_ERROR_HANDLER_ENABLED
            #if CONFIG_MODULE_ERROR_HANDLER_ENABLED
            ErrorHandler_Handle(mount_status, "FS_WRAPPER");
            #endif
            #endif
            return FS_WRAPPER_ERROR_LITTLEFS;
        }
    }
    
    /* 创建必要的目录 */
    for (fs_dir_t dir = FS_DIR_FONT; dir < FS_DIR_MAX; dir++) {
        const char* dir_name = fs_get_dir_name(dir);
        if (dir_name != NULL) {
            char dir_path[FS_PATH_BUFFER_SIZE];
            snprintf(dir_path, sizeof(dir_path), "/%s", dir_name);
            LittleFS_DirCreate(dir_path);  /* 忽略错误，目录可能已存在 */
        }
    }
    
    g_fs_wrapper_initialized = 1;
    return FS_WRAPPER_OK;
}

/**
 * @brief 获取文件路径
 */
const char* FS_GetPath(fs_dir_t dir, const char* name)
{
    /* 参数校验 */
    if (name == NULL) {
        return NULL;
    }
    
    if (dir >= FS_DIR_MAX) {
        return NULL;
    }
    
    const char* dir_name = fs_get_dir_name(dir);
    if (dir_name == NULL) {
        return NULL;
    }
    
    /* 生成路径 */
    snprintf(g_path_buffer, sizeof(g_path_buffer), "/%s/%s", dir_name, name);
    return g_path_buffer;
}

/**
 * @brief 读取文件
 */
error_code_t FS_ReadFile(fs_dir_t dir, const char* name, uint32_t offset, 
                         void* buf, uint32_t size)
{
    /* ========== 参数校验 ========== */
    
    if (name == NULL || buf == NULL) {
        return FS_WRAPPER_ERROR_NULL_PTR;
    }
    
    if (dir >= FS_DIR_MAX) {
        return FS_WRAPPER_ERROR_INVALID_DIR;
    }
    
    if (size == 0) {
        return FS_WRAPPER_ERROR_INVALID_PARAM;
    }
    
    if (!g_fs_wrapper_initialized) {
        return FS_WRAPPER_ERROR_NOT_INIT;
    }
    
    /* ========== 业务逻辑 ========== */
    
    /* 获取文件路径 */
    const char* path = FS_GetPath(dir, name);
    if (path == NULL) {
        return FS_WRAPPER_ERROR_INVALID_PARAM;
    }
    
    /* 打开文件 */
    lfs_file_t file;
    memset(&file, 0, sizeof(file));
    
    LittleFS_Status_t status = LittleFS_FileOpen(&file, path, LFS_O_RDONLY);
    if (status != LITTLEFS_OK) {
        #ifdef CONFIG_MODULE_ERROR_HANDLER_ENABLED
        #if CONFIG_MODULE_ERROR_HANDLER_ENABLED
        ErrorHandler_Handle(status, "FS_WRAPPER");
        #endif
        #endif
        return FS_WRAPPER_ERROR_LITTLEFS;
    }
    
    /* 定位到指定偏移量 */
    if (offset > 0) {
        status = LittleFS_FileSeek(&file, offset, LFS_SEEK_SET);
        if (status != LITTLEFS_OK) {
            LittleFS_FileClose(&file);
            #ifdef CONFIG_MODULE_ERROR_HANDLER_ENABLED
            #if CONFIG_MODULE_ERROR_HANDLER_ENABLED
            ErrorHandler_Handle(status, "FS_WRAPPER");
            #endif
            #endif
            return FS_WRAPPER_ERROR_LITTLEFS;
        }
    }
    
    /* 读取数据 */
    uint32_t bytes_read = 0;
    status = LittleFS_FileRead(&file, buf, size, &bytes_read);
    if (status != LITTLEFS_OK) {
        LittleFS_FileClose(&file);
        #ifdef CONFIG_MODULE_ERROR_HANDLER_ENABLED
        #if CONFIG_MODULE_ERROR_HANDLER_ENABLED
        ErrorHandler_Handle(status, "FS_WRAPPER");
        #endif
        #endif
        return FS_WRAPPER_ERROR_READ_FAILED;
    }
    
    /* 关闭文件 */
    status = LittleFS_FileClose(&file);
    if (status != LITTLEFS_OK) {
        #ifdef CONFIG_MODULE_ERROR_HANDLER_ENABLED
        #if CONFIG_MODULE_ERROR_HANDLER_ENABLED
        ErrorHandler_Handle(status, "FS_WRAPPER");
        #endif
        #endif
        return FS_WRAPPER_ERROR_LITTLEFS;
    }
    
    return FS_WRAPPER_OK;
}

/**
 * @brief 写入文件
 */
error_code_t FS_WriteFile(fs_dir_t dir, const char* name, 
                          const void* buf, uint32_t size)
{
    /* ========== 参数校验 ========== */
    
    if (name == NULL || buf == NULL) {
        return FS_WRAPPER_ERROR_NULL_PTR;
    }
    
    if (dir >= FS_DIR_MAX) {
        return FS_WRAPPER_ERROR_INVALID_DIR;
    }
    
    if (size == 0) {
        return FS_WRAPPER_ERROR_INVALID_PARAM;
    }
    
    if (!g_fs_wrapper_initialized) {
        return FS_WRAPPER_ERROR_NOT_INIT;
    }
    
    /* ========== 业务逻辑 ========== */
    
    /* 获取文件路径 */
    const char* path = FS_GetPath(dir, name);
    if (path == NULL) {
        return FS_WRAPPER_ERROR_INVALID_PARAM;
    }
    
    /* 打开文件（创建或覆盖） */
    lfs_file_t file;
    memset(&file, 0, sizeof(file));
    
    LittleFS_Status_t status = LittleFS_FileOpen(&file, path, 
                                                  LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (status != LITTLEFS_OK) {
        #ifdef CONFIG_MODULE_ERROR_HANDLER_ENABLED
        #if CONFIG_MODULE_ERROR_HANDLER_ENABLED
        ErrorHandler_Handle(status, "FS_WRAPPER");
        #endif
        #endif
        return FS_WRAPPER_ERROR_LITTLEFS;
    }
    
    /* 写入数据 */
    uint32_t bytes_written = 0;
    status = LittleFS_FileWrite(&file, buf, size, &bytes_written);
    if (status != LITTLEFS_OK) {
        LittleFS_FileClose(&file);
        #ifdef CONFIG_MODULE_ERROR_HANDLER_ENABLED
        #if CONFIG_MODULE_ERROR_HANDLER_ENABLED
        ErrorHandler_Handle(status, "FS_WRAPPER");
        #endif
        #endif
        return FS_WRAPPER_ERROR_WRITE_FAILED;
    }
    
    /* 同步文件（确保数据写入Flash） */
    status = LittleFS_FileSync(&file);
    if (status != LITTLEFS_OK) {
        LittleFS_FileClose(&file);
        #ifdef CONFIG_MODULE_ERROR_HANDLER_ENABLED
        #if CONFIG_MODULE_ERROR_HANDLER_ENABLED
        ErrorHandler_Handle(status, "FS_WRAPPER");
        #endif
        #endif
        return FS_WRAPPER_ERROR_SYNC_FAILED;
    }
    
    /* 关闭文件 */
    status = LittleFS_FileClose(&file);
    if (status != LITTLEFS_OK) {
        #ifdef CONFIG_MODULE_ERROR_HANDLER_ENABLED
        #if CONFIG_MODULE_ERROR_HANDLER_ENABLED
        ErrorHandler_Handle(status, "FS_WRAPPER");
        #endif
        #endif
        return FS_WRAPPER_ERROR_LITTLEFS;
    }
    
    return FS_WRAPPER_OK;
}

/**
 * @brief 追加数据到文件
 */
error_code_t FS_AppendFile(fs_dir_t dir, const char* name, 
                           const void* buf, uint32_t size)
{
    /* ========== 参数校验 ========== */
    
    if (name == NULL || buf == NULL) {
        return FS_WRAPPER_ERROR_NULL_PTR;
    }
    
    if (dir >= FS_DIR_MAX) {
        return FS_WRAPPER_ERROR_INVALID_DIR;
    }
    
    if (size == 0) {
        return FS_WRAPPER_ERROR_INVALID_PARAM;
    }
    
    if (!g_fs_wrapper_initialized) {
        return FS_WRAPPER_ERROR_NOT_INIT;
    }
    
    /* ========== 业务逻辑 ========== */
    
    /* 获取文件路径 */
    const char* path = FS_GetPath(dir, name);
    if (path == NULL) {
        return FS_WRAPPER_ERROR_INVALID_PARAM;
    }
    
    /* 打开文件（追加模式） */
    lfs_file_t file;
    memset(&file, 0, sizeof(file));
    
    LittleFS_Status_t status = LittleFS_FileOpen(&file, path, 
                                                  LFS_O_WRONLY | LFS_O_CREAT | LFS_O_APPEND);
    if (status != LITTLEFS_OK) {
        #ifdef CONFIG_MODULE_ERROR_HANDLER_ENABLED
        #if CONFIG_MODULE_ERROR_HANDLER_ENABLED
        ErrorHandler_Handle(status, "FS_WRAPPER");
        #endif
        #endif
        return FS_WRAPPER_ERROR_LITTLEFS;
    }
    
    /* 写入数据 */
    uint32_t bytes_written = 0;
    status = LittleFS_FileWrite(&file, buf, size, &bytes_written);
    if (status != LITTLEFS_OK) {
        LittleFS_FileClose(&file);
        #ifdef CONFIG_MODULE_ERROR_HANDLER_ENABLED
        #if CONFIG_MODULE_ERROR_HANDLER_ENABLED
        ErrorHandler_Handle(status, "FS_WRAPPER");
        #endif
        #endif
        return FS_WRAPPER_ERROR_WRITE_FAILED;
    }
    
    /* 同步文件（确保数据写入Flash） */
    status = LittleFS_FileSync(&file);
    if (status != LITTLEFS_OK) {
        LittleFS_FileClose(&file);
        #ifdef CONFIG_MODULE_ERROR_HANDLER_ENABLED
        #if CONFIG_MODULE_ERROR_HANDLER_ENABLED
        ErrorHandler_Handle(status, "FS_WRAPPER");
        #endif
        #endif
        return FS_WRAPPER_ERROR_SYNC_FAILED;
    }
    
    /* 关闭文件 */
    status = LittleFS_FileClose(&file);
    if (status != LITTLEFS_OK) {
        #ifdef CONFIG_MODULE_ERROR_HANDLER_ENABLED
        #if CONFIG_MODULE_ERROR_HANDLER_ENABLED
        ErrorHandler_Handle(status, "FS_WRAPPER");
        #endif
        #endif
        return FS_WRAPPER_ERROR_LITTLEFS;
    }
    
    return FS_WRAPPER_OK;
}

/**
 * @brief 检查模块是否已初始化
 */
uint8_t FS_IsInitialized(void)
{
    return g_fs_wrapper_initialized;
}

#endif /* CONFIG_MODULE_LITTLEFS_ENABLED */
#endif /* CONFIG_MODULE_LITTLEFS_ENABLED */

#endif /* CONFIG_MODULE_FS_WRAPPER_ENABLED */
#endif /* CONFIG_MODULE_FS_WRAPPER_ENABLED */
