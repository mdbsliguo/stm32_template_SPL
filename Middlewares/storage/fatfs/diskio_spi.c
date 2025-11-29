/**
 * @file diskio_spi.c
 * @brief FatFS SPI接口磁盘I/O实现
 * @version 1.0.0
 * @date 2024-01-01
 * @details 实现FatFS的diskio接口，使用SPI接口（TF_SPI模块）访问SD卡
 */

/* Include config.h first to get module enable flags */
#include "config.h"

#ifdef CONFIG_MODULE_FATFS_ENABLED
#if CONFIG_MODULE_FATFS_ENABLED

#ifdef CONFIG_MODULE_FATFS_SPI_ENABLED
#if CONFIG_MODULE_FATFS_SPI_ENABLED

#include <stddef.h>
#include "ff.h"
#include "diskio.h"
#include "diskio_spi.h"

/* 包含TF_SPI模块 */
#ifdef CONFIG_MODULE_TF_SPI_ENABLED
#if CONFIG_MODULE_TF_SPI_ENABLED
#include "tf_spi.h"
#endif
#endif

/* 包含LED模块（用于格式化进度提示） */
#ifdef CONFIG_MODULE_LED_ENABLED
#if CONFIG_MODULE_LED_ENABLED
#include "led.h"
#endif
#endif

/* 初始化状态标志 */
static uint8_t g_diskio_spi_initialized = 0;

/**
 * @brief 初始化SPI磁盘
 */
DSTATUS disk_initialize_spi(BYTE pdrv)
{
    (void)pdrv;
    
    /* 检查TF_SPI模块是否启用 */
    #ifndef CONFIG_MODULE_TF_SPI_ENABLED
    return STA_NOINIT;
    #else
    #if !CONFIG_MODULE_TF_SPI_ENABLED
    return STA_NOINIT;
    #else
    
    /* 初始化TF_SPI模块 */
    TF_SPI_Status_t status = TF_SPI_Init();
    if (status != TF_SPI_OK) {
        g_diskio_spi_initialized = 0;
        return STA_NOINIT;
    }
    
    /* 检查是否已初始化 */
    if (!TF_SPI_IsInitialized()) {
        g_diskio_spi_initialized = 0;
        return STA_NOINIT;
    }
    
    g_diskio_spi_initialized = 1;
    return 0;  /* 成功 */
    
    #endif
    #endif
}

/**
 * @brief 获取SPI磁盘状态
 */
DSTATUS disk_status_spi(BYTE pdrv)
{
    (void)pdrv;
    
    /* 检查TF_SPI模块是否启用 */
    #ifndef CONFIG_MODULE_TF_SPI_ENABLED
    return STA_NOINIT | STA_NODISK;
    #else
    #if !CONFIG_MODULE_TF_SPI_ENABLED
    return STA_NOINIT | STA_NODISK;
    #else
    
    /* 检查是否已初始化 */
    if (!g_diskio_spi_initialized || !TF_SPI_IsInitialized()) {
        return STA_NOINIT | STA_NODISK;
    }
    
    /* 查询SD卡状态 */
    uint8_t card_status;
    TF_SPI_Status_t status = TF_SPI_SendStatus(&card_status);
    if (status != TF_SPI_OK) {
        return STA_NOINIT | STA_NODISK;
    }
    
    /* 检查写保护（如果支持） */
    DSTATUS result = 0;
    /* 注意：TF_SPI模块当前不提供写保护状态，这里返回0表示正常 */
    
    return result;
    
    #endif
    #endif
}

/**
 * @brief 读取SPI磁盘扇区
 */
DRESULT disk_read_spi(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count)
{
    (void)pdrv;
    
    /* 参数校验 */
    if (buff == NULL) {
        return RES_PARERR;
    }
    
    /* 检查TF_SPI模块是否启用 */
    #ifndef CONFIG_MODULE_TF_SPI_ENABLED
    return RES_ERROR;
    #else
    #if !CONFIG_MODULE_TF_SPI_ENABLED
    return RES_ERROR;
    #else
    
    /* 检查是否已初始化 */
    if (!g_diskio_spi_initialized || !TF_SPI_IsInitialized()) {
        return RES_NOTRDY;
    }
    
    /* 读取扇区 */
    TF_SPI_Status_t status;
    if (count == 1) {
        /* 单扇区读取 */
        status = TF_SPI_ReadBlock((uint32_t)sector, buff);
    } else {
        /* 多扇区读取 */
        status = TF_SPI_ReadBlocks((uint32_t)sector, (uint32_t)count, buff);
    }
    
    /* 转换错误码 */
    if (status == TF_SPI_OK) {
        return RES_OK;
    } else if (status == TF_SPI_ERROR_NOT_INIT) {
        return RES_NOTRDY;
    } else if (status == TF_SPI_ERROR_OUT_OF_BOUND) {
        return RES_PARERR;
    } else {
        return RES_ERROR;
    }
    
    #endif
    #endif
}

/**
 * @brief 写入SPI磁盘扇区
 */
DRESULT disk_write_spi(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count)
{
    (void)pdrv;
    
    /* 参数校验 */
    if (buff == NULL) {
        return RES_PARERR;
    }
    
    /* 检查TF_SPI模块是否启用 */
    #ifndef CONFIG_MODULE_TF_SPI_ENABLED
    return RES_ERROR;
    #else
    #if !CONFIG_MODULE_TF_SPI_ENABLED
    return RES_ERROR;
    #else
    
    /* 检查是否已初始化 */
    if (!g_diskio_spi_initialized || !TF_SPI_IsInitialized()) {
        return RES_NOTRDY;
    }
    
    /* 写入扇区 */
    TF_SPI_Status_t status;
    if (count == 1) {
        /* 单扇区写入 */
        status = TF_SPI_WriteBlock((uint32_t)sector, buff);
    } else {
        /* 多扇区写入 */
        status = TF_SPI_WriteBlocks((uint32_t)sector, (uint32_t)count, buff);
    }
    
    /* 格式化进度提示：每次写入后闪烁LED（让用户知道程序正在运行） */
    #ifdef CONFIG_MODULE_LED_ENABLED
    #if CONFIG_MODULE_LED_ENABLED
    /* 格式化时会频繁写入扇区，每次写入都闪烁LED以显示进度 */
    /* 使用静态变量控制闪烁频率，避免过于频繁 */
    static uint32_t write_count = 0;
    write_count++;
    if (write_count % 50 == 0) {  /* 每50次写入闪烁一次，平衡可见性和性能 */
        LED1_Toggle();
    }
    #endif
    #endif
    
    /* 转换错误码 */
    if (status == TF_SPI_OK) {
        return RES_OK;
    } else if (status == TF_SPI_ERROR_NOT_INIT) {
        return RES_NOTRDY;
    } else if (status == TF_SPI_ERROR_WRITE_PROTECT) {
        return RES_WRPRT;
    } else if (status == TF_SPI_ERROR_OUT_OF_BOUND) {
        return RES_PARERR;
    } else {
        return RES_ERROR;
    }
    
    #endif
    #endif
}

/**
 * @brief SPI磁盘控制命令
 */
DRESULT disk_ioctl_spi(BYTE pdrv, BYTE cmd, void* buff)
{
    (void)pdrv;
    
    /* 检查TF_SPI模块是否启用 */
    #ifndef CONFIG_MODULE_TF_SPI_ENABLED
    return RES_ERROR;
    #else
    #if !CONFIG_MODULE_TF_SPI_ENABLED
    return RES_ERROR;
    #else
    
    /* 检查是否已初始化 */
    if (!g_diskio_spi_initialized || !TF_SPI_IsInitialized()) {
        return RES_NOTRDY;
    }
    
    /* 获取设备信息 */
    const tf_spi_dev_t* dev_info = TF_SPI_GetInfo();
    if (dev_info == NULL) {
        return RES_ERROR;
    }
    
    DRESULT res = RES_OK;
    
    switch (cmd) {
        case CTRL_SYNC:
            /* 同步操作（TF_SPI模块自动处理） */
            res = RES_OK;
            break;
            
        case GET_SECTOR_COUNT:
            /* 获取扇区总数 */
            if (buff != NULL) {
                *(LBA_t*)buff = (LBA_t)dev_info->block_count;
            }
            res = RES_OK;
            break;
            
        case GET_SECTOR_SIZE:
            /* 获取扇区大小 */
            if (buff != NULL) {
                *(WORD*)buff = (WORD)dev_info->block_size;
            }
            res = RES_OK;
            break;
            
        case GET_BLOCK_SIZE:
            /* 获取擦除块大小（SD卡通常为1个扇区） */
            if (buff != NULL) {
                *(DWORD*)buff = 1;  /* 1个扇区 */
            }
            res = RES_OK;
            break;
            
        case CTRL_TRIM:
            /* Trim操作（SD卡不支持） */
            res = RES_OK;
            break;
            
        default:
            res = RES_PARERR;
            break;
    }
    
    return res;
    
    #endif
    #endif
}

#endif /* CONFIG_MODULE_FATFS_SPI_ENABLED */
#endif /* CONFIG_MODULE_FATFS_SPI_ENABLED */

#endif /* CONFIG_MODULE_FATFS_ENABLED */
#endif /* CONFIG_MODULE_FATFS_ENABLED */

