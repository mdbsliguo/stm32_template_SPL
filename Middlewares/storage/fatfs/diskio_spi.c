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
#if CONFIG_MODULE_LOG_ENABLED
#include "log.h"
#endif

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

/* 分区扇区数缓存（避免每次读取MBR） */
#if defined(FATFS_PARTITION_START_SECTOR) && (FATFS_PARTITION_START_SECTOR > 0)
static uint32_t g_cached_partition_sectors = 0;  /* 0表示未缓存 */
#endif

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
 * @note 在分区模式下，FatFS的逻辑扇区0映射到物理扇区FATFS_PARTITION_START_SECTOR
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
    
    /* 扇区映射：在分区模式下，FatFS的逻辑扇区需要映射到物理扇区 */
    #if defined(FATFS_PARTITION_START_SECTOR) && (FATFS_PARTITION_START_SECTOR > 0)
    /* 分区模式：逻辑扇区0映射到物理扇区FATFS_PARTITION_START_SECTOR */
    /* 但是，f_mkfs在读取MBR时会读取扇区0，此时需要读取物理扇区0（MBR） */
    /* 判断方法：如果读取扇区0且缓存未设置，说明是f_mkfs在读取MBR，读取物理扇区0 */
    /* 如果缓存已设置，说明格式化已开始，分区数据读取应该映射到物理扇区 */
    LBA_t physical_sector;
    if (sector == 0 && g_cached_partition_sectors == 0) {
        /* f_mkfs在读取MBR（格式化开始前），读取物理扇区0 */
        physical_sector = 0;
    } else {
        /* 正常分区数据读取，映射到物理扇区 */
        physical_sector = (LBA_t)((uint32_t)sector + FATFS_PARTITION_START_SECTOR);
    }
    #else
    /* 标准模式：直接使用逻辑扇区 */
    LBA_t physical_sector = sector;
    #endif
    
    /* 读取扇区 */
    TF_SPI_Status_t status;
    #if CONFIG_MODULE_LOG_ENABLED
    LOG_DEBUG("DISKIO", "disk_read_spi: logical_sector=%lu, physical_sector=%lu, count=%u", 
              (unsigned long)sector, (unsigned long)physical_sector, count);
    #endif
    if (count == 1) {
        /* 单扇区读取 */
        status = TF_SPI_ReadBlock((uint32_t)physical_sector, buff);
    } else {
        /* 多扇区读取 */
        status = TF_SPI_ReadBlocks((uint32_t)physical_sector, (uint32_t)count, buff);
    }
    #if CONFIG_MODULE_LOG_ENABLED
    if (status != TF_SPI_OK) {
        LOG_DEBUG("DISKIO", "disk_read_spi failed: status=%d", status);
    }
    #endif
    
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
 * @note 在分区模式下，FatFS的逻辑扇区0映射到物理扇区FATFS_PARTITION_START_SECTOR
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
    
    /* 扇区映射：在分区模式下，FatFS的逻辑扇区需要映射到物理扇区 */
    #if defined(FATFS_PARTITION_START_SECTOR) && (FATFS_PARTITION_START_SECTOR > 0)
    /* 分区模式：逻辑扇区0映射到物理扇区FATFS_PARTITION_START_SECTOR */
    /* 但是，写入扇区0时，可能是写入MBR，需要写入物理扇区0 */
    /* 判断方法：如果写入扇区0且缓存未设置，说明可能是写入MBR，写入物理扇区0 */
    LBA_t physical_sector;
    if (sector == 0 && g_cached_partition_sectors == 0) {
        /* 写入扇区0，可能是MBR，写入物理扇区0 */
        physical_sector = 0;
    } else {
        /* 正常分区数据写入，映射到物理扇区 */
        physical_sector = (LBA_t)((uint32_t)sector + FATFS_PARTITION_START_SECTOR);
    }
    #else
    /* 标准模式：直接使用逻辑扇区 */
    LBA_t physical_sector = sector;
    #endif
    
    /* 写入扇区 */
    TF_SPI_Status_t status;
    if (count == 1) {
        /* 单扇区写入 */
        status = TF_SPI_WriteBlock((uint32_t)physical_sector, buff);
    } else {
        /* 多扇区写入 */
        status = TF_SPI_WriteBlocks((uint32_t)physical_sector, (uint32_t)count, buff);
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
 * @note 在分区模式下，GET_SECTOR_COUNT返回FAT32分区的扇区数（从MBR读取）
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
                #if defined(FATFS_PARTITION_START_SECTOR) && (FATFS_PARTITION_START_SECTOR > 0)
                /* 分区模式：从MBR读取FAT32分区的扇区数 */
                uint32_t partition_sectors = 0;
                uint32_t expected_sectors = dev_info->block_count - FATFS_PARTITION_START_SECTOR;
                
                /* 如果缓存有效，使用缓存值 */
                if (g_cached_partition_sectors > 0 && 
                    g_cached_partition_sectors <= dev_info->block_count) {
                    partition_sectors = g_cached_partition_sectors;
                } else {
                    /* 从MBR读取 */
                    BYTE mbr_buf[512];
                    /* 直接读取物理扇区0（MBR），不使用disk_read_spi避免递归 */
                    TF_SPI_Status_t tf_status = TF_SPI_ReadBlock(0, mbr_buf);
                    if (tf_status == TF_SPI_OK) {
                        /* 读取分区1的扇区数（MBR分区表偏移446，分区1偏移8字节为起始LBA，偏移12字节为扇区数） */
                        #define MBR_PARTITION_TABLE_OFFSET  446
                        #define PTE_SIZE_LBA               12
                        BYTE* pte1 = mbr_buf + MBR_PARTITION_TABLE_OFFSET;
                        
                        /* 读取原始字节值 */
                        uint8_t byte0 = pte1[PTE_SIZE_LBA + 0];
                        uint8_t byte1 = pte1[PTE_SIZE_LBA + 1];
                        uint8_t byte2 = pte1[PTE_SIZE_LBA + 2];
                        uint8_t byte3 = pte1[PTE_SIZE_LBA + 3];
                        
                        /* 小端格式解析 */
                        uint32_t mbr_sectors = (uint32_t)byte0 |
                                             ((uint32_t)byte1 << 8) |
                                             ((uint32_t)byte2 << 16) |
                                             ((uint32_t)byte3 << 24);
                        
                        /* 验证读取的值是否合理 */
                        if (mbr_sectors > 0 && mbr_sectors <= dev_info->block_count) {
                            partition_sectors = mbr_sectors;
                            /* 缓存有效值 */
                            g_cached_partition_sectors = partition_sectors;
                        } else {
                            /* MBR值无效，使用期望值 */
                            partition_sectors = expected_sectors;
                            g_cached_partition_sectors = expected_sectors;
                        }
                    } else {
                        /* MBR读取失败，使用期望值 */
                        partition_sectors = expected_sectors;
                        g_cached_partition_sectors = expected_sectors;
                    }
                }
                
                *(LBA_t*)buff = (LBA_t)partition_sectors;
                #else
                /* 标准模式：返回整个SD卡的扇区数 */
                *(LBA_t*)buff = (LBA_t)dev_info->block_count;
                #endif
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

#if defined(FATFS_PARTITION_START_SECTOR) && (FATFS_PARTITION_START_SECTOR > 0)
/**
 * @brief 设置分区扇区数缓存
 */
void disk_ioctl_spi_set_partition_sectors(uint32_t sectors)
{
    g_cached_partition_sectors = sectors;
}

/**
 * @brief 清除分区扇区数缓存
 */
void disk_ioctl_spi_clear_partition_cache(void)
{
    g_cached_partition_sectors = 0;
}
#endif

#endif /* CONFIG_MODULE_FATFS_SPI_ENABLED */
#endif /* CONFIG_MODULE_FATFS_SPI_ENABLED */

#endif /* CONFIG_MODULE_FATFS_ENABLED */
#endif /* CONFIG_MODULE_FATFS_ENABLED */
