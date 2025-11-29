/**
 * @file fatfs_wrapper.c
 * @brief FatFS封装层实现
 * @version 1.0.0
 * @date 2024-01-01
 * @details 提供统一的FatFS接口，封装FatFS底层API，统一错误码转换
 */

/* Include config.h first to get module enable flags */
#include "config.h"

#ifdef CONFIG_MODULE_FATFS_ENABLED
#if CONFIG_MODULE_FATFS_ENABLED

#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "fatfs_wrapper.h"
#include "diskio.h"
#include "diskio_spi.h"
#include "diskio_sdio.h"
#include "ff.h"

/* 挂载状态标志 */
static uint8_t g_fatfs_mounted[FF_VOLUMES] = {0};

/* 文件系统对象 */
static FATFS g_fatfs[FF_VOLUMES];

#if FF_MULTI_PARTITION
/* 逻辑驱动器到物理驱动器和分区的映射表 */
/* pd: 物理驱动器号（0=SPI, 1=SDIO等） */
/* pt: 分区号（0=自动检测, 1-4=强制指定分区） */
/* 注意：当使用分区方案时，路径中的分区号会覆盖此配置 */
/* 例如：VolToPart[0] = {0, 1} 表示逻辑驱动器0 -> 物理驱动器0，分区1 */
/* 使用路径 "0:1:" 时会使用分区1，使用路径 "0:" 时会使用VolToPart中的配置 */
PARTITION VolToPart[FF_VOLUMES] = {
    {FATFS_VOLUME_SPI, 1},  /* 逻辑驱动器0 -> 物理驱动器0（SPI），分区1（FAT32分区） */
};
#endif

/**
 * @brief 将FatFS错误码转换为项目错误码
 */
static FatFS_Status_t FatFS_ConvertError(FRESULT fr)
{
    switch (fr) {
        case FR_OK:
            return FATFS_OK;
        case FR_DISK_ERR:
            return FATFS_ERROR_DISK_ERROR;
        case FR_INT_ERR:
            return FATFS_ERROR_INVALID_PARAM;
        case FR_NOT_READY:
            return FATFS_ERROR_NOT_READY;
        case FR_NO_FILE:
            return FATFS_ERROR_NO_FILE;
        case FR_NO_PATH:
            return FATFS_ERROR_NO_PATH;
        case FR_INVALID_NAME:
            return FATFS_ERROR_INVALID_NAME;
        case FR_DENIED:
            return FATFS_ERROR_DENIED;
        case FR_EXIST:
            return FATFS_ERROR_EXIST;
        case FR_INVALID_OBJECT:
            return FATFS_ERROR_INVALID_OBJECT;
        case FR_WRITE_PROTECTED:
            return FATFS_ERROR_WRITE_PROTECTED;
        case FR_INVALID_DRIVE:
            return FATFS_ERROR_INVALID_DRIVE;
        case FR_NOT_ENABLED:
            return FATFS_ERROR_NOT_ENABLED;
        case FR_NO_FILESYSTEM:
            return FATFS_ERROR_NO_FILESYSTEM;
        case FR_TIMEOUT:
            return FATFS_ERROR_TIMEOUT;
        case FR_LOCKED:
            return FATFS_ERROR_LOCKED;
        case FR_NOT_ENOUGH_CORE:
            return FATFS_ERROR_NOT_ENOUGH_CORE;
        case FR_TOO_MANY_OPEN_FILES:
            return FATFS_ERROR_TOO_MANY_OPEN_FILES;
        case FR_INVALID_PARAMETER:
            return FATFS_ERROR_INVALID_PARAMETER;
        default:
            return FATFS_ERROR_INVALID_PARAM;
    }
}

/**
 * @brief 挂载文件系统
 */
FatFS_Status_t FatFS_Mount(FatFS_Volume_t volume, const char* path)
{
    /* ========== 参数校验 ========== */
    if (path == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    if (volume >= FF_VOLUMES) {
        return FATFS_ERROR_INVALID_VOLUME;
    }
    
    /* 检查是否已经挂载 */
    if (g_fatfs_mounted[volume] != 0) {
        /* 已经挂载，直接返回成功 */
        return FATFS_OK;
    }
    
    /* 挂载文件系统 */
    FRESULT fr = f_mount(&g_fatfs[volume], path, 1);
    if (fr == FR_OK) {
        g_fatfs_mounted[volume] = 1;
    }
    
    return FatFS_ConvertError(fr);
}

/**
 * @brief 卸载文件系统
 */
FatFS_Status_t FatFS_Unmount(FatFS_Volume_t volume)
{
    /* ========== 参数校验 ========== */
    if (volume >= FF_VOLUMES) {
        return FATFS_ERROR_INVALID_VOLUME;
    }
    
    /* 检查是否已经卸载 */
    if (g_fatfs_mounted[volume] == 0) {
        /* 已经卸载，直接返回成功 */
        return FATFS_OK;
    }
    
    /* 卸载文件系统 */
    char path[4];
    path[0] = '0' + volume;
    path[1] = ':';
    path[2] = '\0';
    
    FRESULT fr = f_mount(NULL, path, 0);
    if (fr == FR_OK) {
        g_fatfs_mounted[volume] = 0;
    }
    
    return FatFS_ConvertError(fr);
}

/**
 * @brief 格式化文件系统
 */
FatFS_Status_t FatFS_Format(FatFS_Volume_t volume, const char* path)
{
    /* ========== 参数校验 ========== */
    if (path == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    if (volume >= FF_VOLUMES) {
        return FATFS_ERROR_INVALID_VOLUME;
    }
    
    #if FF_USE_MKFS
    /* 格式化参数 */
    MKFS_PARM opt;
    opt.fmt = FM_FAT32;  /* 使用FAT32格式 */
    opt.n_fat = 1;       /* 1个FAT表 */
    opt.align = 0;       /* 自动对齐 */
    opt.n_root = 0;      /* 根目录项数（FAT32自动） */
    opt.au_size = 0;     /* 自动簇大小 */
    
    /* 工作缓冲区（需要至少FF_MAX_SS字节） */
    static BYTE work[FF_MAX_SS];
    
    FRESULT fr = f_mkfs(path, &opt, work, sizeof(work));
    return FatFS_ConvertError(fr);
    #else
    return FATFS_ERROR_NOT_IMPLEMENTED;
    #endif
}

/**
 * @brief 打开文件
 */
FatFS_Status_t FatFS_FileOpen(FIL* file, const char* path, uint8_t mode)
{
    /* ========== 参数校验 ========== */
    if (file == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    if (path == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    FRESULT fr = f_open(file, path, mode);
    return FatFS_ConvertError(fr);
}

/**
 * @brief 关闭文件
 */
FatFS_Status_t FatFS_FileClose(FIL* file)
{
    /* ========== 参数校验 ========== */
    if (file == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    FRESULT fr = f_close(file);
    return FatFS_ConvertError(fr);
}

/**
 * @brief 读取文件
 */
FatFS_Status_t FatFS_FileRead(FIL* file, void* buff, uint32_t btr, uint32_t* br)
{
    /* ========== 参数校验 ========== */
    if (file == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    if (buff == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    UINT bytes_read;
    FRESULT fr = f_read(file, buff, btr, &bytes_read);
    
    if (br != NULL) {
        *br = bytes_read;
    }
    
    return FatFS_ConvertError(fr);
}

/**
 * @brief 写入文件
 */
FatFS_Status_t FatFS_FileWrite(FIL* file, const void* buff, uint32_t btw, uint32_t* bw)
{
    /* ========== 参数校验 ========== */
    if (file == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    if (buff == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    UINT bytes_written;
    FRESULT fr = f_write(file, buff, btw, &bytes_written);
    
    if (bw != NULL) {
        *bw = bytes_written;
    }
    
    return FatFS_ConvertError(fr);
}

/**
 * @brief 定位文件指针
 */
FatFS_Status_t FatFS_FileSeek(FIL* file, uint32_t ofs)
{
    /* ========== 参数校验 ========== */
    if (file == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    FRESULT fr = f_lseek(file, ofs);
    return FatFS_ConvertError(fr);
}

/**
 * @brief 截断文件
 */
FatFS_Status_t FatFS_FileTruncate(FIL* file)
{
    /* ========== 参数校验 ========== */
    if (file == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    #if !FF_FS_READONLY
    FRESULT fr = f_truncate(file);
    return FatFS_ConvertError(fr);
    #else
    return FATFS_ERROR_NOT_IMPLEMENTED;
    #endif
}

/**
 * @brief 同步文件
 */
FatFS_Status_t FatFS_FileSync(FIL* file)
{
    /* ========== 参数校验 ========== */
    if (file == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    #if !FF_FS_READONLY
    FRESULT fr = f_sync(file);
    return FatFS_ConvertError(fr);
    #else
    return FATFS_OK;
    #endif
}

/**
 * @brief 删除文件
 */
FatFS_Status_t FatFS_FileDelete(const char* path)
{
    /* ========== 参数校验 ========== */
    if (path == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    #if !FF_FS_READONLY
    FRESULT fr = f_unlink(path);
    return FatFS_ConvertError(fr);
    #else
    return FATFS_ERROR_NOT_IMPLEMENTED;
    #endif
}

/**
 * @brief 重命名文件
 */
FatFS_Status_t FatFS_FileRename(const char* path_old, const char* path_new)
{
    /* ========== 参数校验 ========== */
    if (path_old == NULL || path_new == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    #if !FF_FS_READONLY
    FRESULT fr = f_rename(path_old, path_new);
    return FatFS_ConvertError(fr);
    #else
    return FATFS_ERROR_NOT_IMPLEMENTED;
    #endif
}

/**
 * @brief 打开目录
 */
FatFS_Status_t FatFS_DirOpen(DIR* dir, const char* path)
{
    /* ========== 参数校验 ========== */
    if (dir == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    if (path == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    FRESULT fr = f_opendir(dir, path);
    return FatFS_ConvertError(fr);
}

/**
 * @brief 关闭目录
 */
FatFS_Status_t FatFS_DirClose(DIR* dir)
{
    /* ========== 参数校验 ========== */
    if (dir == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    FRESULT fr = f_closedir(dir);
    return FatFS_ConvertError(fr);
}

/**
 * @brief 读取目录项
 */
FatFS_Status_t FatFS_DirRead(DIR* dir, FILINFO* fno)
{
    /* ========== 参数校验 ========== */
    if (dir == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    if (fno == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    FRESULT fr = f_readdir(dir, fno);
    return FatFS_ConvertError(fr);
}

/**
 * @brief 创建目录
 */
FatFS_Status_t FatFS_DirCreate(const char* path)
{
    /* ========== 参数校验 ========== */
    if (path == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    #if !FF_FS_READONLY
    FRESULT fr = f_mkdir(path);
    return FatFS_ConvertError(fr);
    #else
    return FATFS_ERROR_NOT_IMPLEMENTED;
    #endif
}

/**
 * @brief 删除目录
 */
FatFS_Status_t FatFS_DirDelete(const char* path)
{
    /* ========== 参数校验 ========== */
    if (path == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    #if !FF_FS_READONLY
    FRESULT fr = f_unlink(path);
    return FatFS_ConvertError(fr);
    #else
    return FATFS_ERROR_NOT_IMPLEMENTED;
    #endif
}

/**
 * @brief 获取空闲空间
 */
FatFS_Status_t FatFS_GetFreeSpace(FatFS_Volume_t volume, const char* path, 
                                   uint32_t* free_clusters, uint32_t* total_clusters)
{
    /* ========== 参数校验 ========== */
    if (path == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    if (volume >= FF_VOLUMES) {
        return FATFS_ERROR_INVALID_VOLUME;
    }
    
    DWORD nclst;
    FATFS* fatfs;
    FRESULT fr = f_getfree(path, &nclst, &fatfs);
    
    if (fr == FR_OK) {
        if (free_clusters != NULL) {
            *free_clusters = nclst;
        }
        if (total_clusters != NULL && fatfs != NULL) {
            *total_clusters = fatfs->n_fatent - 2;  /* 总簇数 = FAT项数 - 2 */
        }
    }
    
    return FatFS_ConvertError(fr);
}

/**
 * @brief 获取总空间
 */
FatFS_Status_t FatFS_GetTotalSpace(FatFS_Volume_t volume, const char* path, uint64_t* total_bytes)
{
    /* ========== 参数校验 ========== */
    if (path == NULL) {
        return FATFS_ERROR_NULL_PTR;
    }
    
    if (volume >= FF_VOLUMES) {
        return FATFS_ERROR_INVALID_VOLUME;
    }
    
    DWORD nclst;
    FATFS* fatfs;
    FRESULT fr = f_getfree(path, &nclst, &fatfs);
    
    if (fr == FR_OK && fatfs != NULL && total_bytes != NULL) {
        DWORD total_clusters = fatfs->n_fatent - 2;
        DWORD cluster_size = fatfs->csize * FF_MIN_SS;  /* 簇大小（字节） */
        /* 使用64位计算，支持最大16EB的文件系统 */
        *total_bytes = (uint64_t)total_clusters * (uint64_t)cluster_size;
    }
    
    return FatFS_ConvertError(fr);
}

/**
 * @brief 获取当前时间（FatFS时间戳函数）
 * @return DWORD 时间戳值
 * @details FatFS需要此函数来设置文件/目录的时间戳
 *          格式：bit31:25=年份(0-127, 1980+), bit24:21=月份(1-12), 
 *                bit20:16=日期(1-31), bit15:11=小时(0-23),
 *                bit10:5=分钟(0-59), bit4:0=秒/2(0-29)
 * @note 如果系统没有RTC，可以返回固定时间或使用系统时间
 */
#if !FF_FS_READONLY && !FF_FS_NORTC
DWORD get_fattime(void)
{
    /* 返回固定时间：2025年1月1日 12:00:00 */
    /* 年份：2025 - 1980 = 45 */
    /* 月份：1 */
    /* 日期：1 */
    /* 小时：12 */
    /* 分钟：0 */
    /* 秒：0 (秒/2 = 0) */
    
    DWORD time = 0;
    time |= ((DWORD)(2025 - 1980) << 25);  /* 年份：2025 */
    time |= ((DWORD)1 << 21);               /* 月份：1月 */
    time |= ((DWORD)1 << 16);               /* 日期：1日 */
    time |= ((DWORD)12 << 11);             /* 小时：12 */
    time |= ((DWORD)0 << 5);                /* 分钟：0 */
    time |= ((DWORD)(0 / 2));               /* 秒：0 (秒/2 = 0) */
    
    return time;
}
#endif /* !FF_FS_READONLY && !FF_FS_NORTC */

#endif /* CONFIG_MODULE_FATFS_ENABLED */
#endif /* CONFIG_MODULE_FATFS_ENABLED */

