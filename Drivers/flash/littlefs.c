/**
 * @file littlefs.c
 * @brief LittleFS文件系统驱动模块实现
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

#include "littlefs.h"
#include "config.h"
#include <stddef.h>
#include <string.h>
#include "delay.h"  /* 用于Delay_us()函数 */

#ifdef CONFIG_MODULE_LITTLEFS_ENABLED
#if CONFIG_MODULE_LITTLEFS_ENABLED

#ifdef CONFIG_MODULE_W25Q_ENABLED
#if CONFIG_MODULE_W25Q_ENABLED

/* 包含littlefs核心库 */
#include "lfs.h"
#include "lfs_util.h"

/* 包含SPI驱动（用于CS引脚控制） */
#ifdef CONFIG_MODULE_SPI_ENABLED
#if CONFIG_MODULE_SPI_ENABLED
#include "spi_hw.h"
#endif
#endif

/* ==================== 内部常量定义 ==================== */

#define W25Q_PAGE_SIZE      256   /**< W25Q页大小（字节） */
#define W25Q_SECTOR_SIZE    4096  /**< W25Q扇区大小（字节） */

/* ==================== 全局状态变量 ==================== */

/**
 * @brief LittleFS设备全局状态结构体
 */
typedef struct {
    LittleFS_State_t state;        /**< 模块状态 */
    /* 关键修复：延迟分配lfs_t结构体，避免启动时占用大量RAM */
    /* lfs_t结构体较大（约200-300字节），如果启动时初始化失败会导致RAM不足 */
    /* 改为在LittleFS_Init()时动态分配，或者使用静态分配但延迟初始化 */
    lfs_t lfs;                     /**< littlefs文件系统对象（延迟初始化） */
    struct lfs_config config;     /**< littlefs配置结构体 */
    /* 关键修复：减少RAM占用，使用较小的缓存 */
    /* 原配置：read_buffer[4096] + prog_buffer[4096] = 8KB，对于STM32F103C8（20KB RAM）可能过大 */
    /* 新配置：使用256字节缓存（W25Q页大小），littlefs会自动处理跨页操作 */
    uint8_t read_buffer[W25Q_PAGE_SIZE];      /**< 读缓存（256字节，减少RAM占用） */
    uint8_t prog_buffer[W25Q_PAGE_SIZE];       /**< 写缓存（256字节，减少RAM占用） */
    uint8_t lookahead_buffer[64] __attribute__((aligned(4)));  /**< 前瞻缓冲区（64字节，8个块，32位对齐） */
    /* 关键：为文件操作添加文件缓冲区（每个打开的文件需要一个cache_size大小的缓冲区） */
    /* 注意：如果同时打开多个文件，需要多个缓冲区，这里只提供一个用于单文件操作 */
    uint8_t file_buffer[W25Q_PAGE_SIZE];      /**< 文件缓存（256字节，用于文件读写操作） */
} littlefs_dev_t;

/* 关键修复：使用未初始化的全局变量，避免启动时的初始化问题 */
/* 如果全局变量在启动时初始化失败，会导致程序无法启动 */
/* 改为在LittleFS_Init()时手动初始化，确保在系统完全启动后再分配 */
static littlefs_dev_t g_littlefs_device;

/* ==================== 块设备回调函数 ==================== */

/**
 * @brief 读取块设备数据
 */
static int littlefs_bd_read(const struct lfs_config* c, lfs_block_t block,
                             lfs_off_t off, void* buffer, lfs_size_t size)
{
    /* 参数校验 */
    if (c == NULL || buffer == NULL) {
        return LFS_ERR_INVAL;
    }
    
    /* 检查W25Q是否已初始化 */
    if (!W25Q_IsInitialized()) {
        return LFS_ERR_IO;
    }
    
    /* 计算Flash地址：块号 * 块大小 + 偏移 */
    uint32_t addr = (uint32_t)block * W25Q_SECTOR_SIZE + (uint32_t)off;
    
    /* 调试：检查地址是否超出范围 */
    const w25q_dev_t* dev_info = W25Q_GetInfo();
    if (dev_info != NULL) {
        uint32_t max_addr = dev_info->capacity_mb * 1024 * 1024;
        if (addr >= max_addr || (addr + size) > max_addr) {
            return LFS_ERR_IO;  /* 地址超出范围 */
        }
    }
    
    /* 挂载前：确保CS引脚为高（释放状态） */
    /* 关键：每次读取前确保CS引脚状态正确 */
    #ifdef CONFIG_MODULE_W25Q_ENABLED
    #if CONFIG_MODULE_W25Q_ENABLED
    SPI_Instance_t spi_instance = W25Q_SPI_INSTANCE;
    SPI_NSS_High(spi_instance);  /* 确保CS为高（释放） */
    Delay_us(5);  /* 短暂延时确保CS稳定 */
    #endif
    #endif
    
    /* 读取数据 */
    /* 注意：W25Q_Read()内部会控制CS引脚，这里先确保CS为高 */
    /* 注意：挂载时会多次调用此函数读取Flash，如果W25Q_Read()卡住，整个挂载会卡住 */
    /* 调试：如果挂载卡住，检查W25Q_Read()是否超时或SPI通信是否正常 */
    /* 关键：这里可能是卡住的位置！如果W25Q_Read()内部SPI通信超时或卡住，整个挂载会卡住 */
    W25Q_Status_t status = W25Q_Read(addr, (uint8_t*)buffer, (uint32_t)size);
    if (status != W25Q_OK) {
        /* 读取失败，返回IO错误 */
        /* 注意：如果W25Q_Read()返回超时错误，说明SPI通信有问题 */
        return LFS_ERR_IO;
    }
    
    return LFS_ERR_OK;
}

/**
 * @brief 写入块设备数据
 */
static int littlefs_bd_prog(const struct lfs_config* c, lfs_block_t block,
                             lfs_off_t off, const void* buffer, lfs_size_t size)
{
    /* 参数校验 */
    if (c == NULL || buffer == NULL) {
        return LFS_ERR_INVAL;
    }
    
    /* 检查W25Q是否已初始化 */
    if (!W25Q_IsInitialized()) {
        return LFS_ERR_IO;
    }
    
    /* 计算Flash地址：块号 * 块大小 + 偏移 */
    uint32_t addr = (uint32_t)block * W25Q_SECTOR_SIZE + (uint32_t)off;
    
    /* 写入数据 */
    W25Q_Status_t status = W25Q_Write(addr, (const uint8_t*)buffer, (uint32_t)size);
    if (status != W25Q_OK) {
        return LFS_ERR_IO;
    }
    
    return LFS_ERR_OK;
}

/**
 * @brief 擦除块设备块
 */
static int littlefs_bd_erase(const struct lfs_config* c, lfs_block_t block)
{
    /* 参数校验 */
    if (c == NULL) {
        return LFS_ERR_INVAL;
    }
    
    /* 检查W25Q是否已初始化 */
    if (!W25Q_IsInitialized()) {
        return LFS_ERR_IO;
    }
    
    /* 计算Flash地址：块号 * 块大小 */
    uint32_t addr = (uint32_t)block * W25Q_SECTOR_SIZE;
    
    /* 擦除扇区 */
    W25Q_Status_t status = W25Q_EraseSector(addr);
    if (status != W25Q_OK) {
        return LFS_ERR_IO;
    }
    
    return LFS_ERR_OK;
}

/**
 * @brief 同步块设备
 */
static int littlefs_bd_sync(const struct lfs_config* c)
{
    /* 参数校验 */
    if (c == NULL) {
        return LFS_ERR_INVAL;
    }
    
    /* 检查W25Q是否已初始化 */
    if (!W25Q_IsInitialized()) {
        return LFS_ERR_IO;
    }
    
    /* 等待Flash就绪 */
    W25Q_Status_t status = W25Q_WaitReady(0);
    if (status != W25Q_OK) {
        return LFS_ERR_IO;
    }
    
    return LFS_ERR_OK;
}

/* ==================== 内部函数 ==================== */

/**
 * @brief 将littlefs错误码转换为项目错误码
 */
static LittleFS_Status_t LittleFS_ConvertError(int lfs_err)
{
    switch (lfs_err) {
        case LFS_ERR_OK:
            return LITTLEFS_OK;
        case LFS_ERR_IO:
            return LITTLEFS_ERROR_IO;
        case LFS_ERR_CORRUPT:
            return LITTLEFS_ERROR_CORRUPT;
        case LFS_ERR_NOENT:
            return LITTLEFS_ERROR_NOENT;
        case LFS_ERR_EXIST:
            return LITTLEFS_ERROR_EXIST;
        case LFS_ERR_NOTDIR:
            return LITTLEFS_ERROR_NOTDIR;
        case LFS_ERR_ISDIR:
            return LITTLEFS_ERROR_ISDIR;
        case LFS_ERR_NOTEMPTY:
            return LITTLEFS_ERROR_NOTEMPTY;
        case LFS_ERR_BADF:
            return LITTLEFS_ERROR_BADF;
        case LFS_ERR_FBIG:
            return LITTLEFS_ERROR_FBIG;
        case LFS_ERR_INVAL:
            return LITTLEFS_ERROR_INVALID_PARAM;
        case LFS_ERR_NOSPC:
            return LITTLEFS_ERROR_NOSPC;
        case LFS_ERR_NOMEM:
            return LITTLEFS_ERROR_NOMEM;
        case LFS_ERR_NOATTR:
            return LITTLEFS_ERROR_NOATTR;
        case LFS_ERR_NAMETOOLONG:
            return LITTLEFS_ERROR_NAMETOOLONG;
        default:
            return LITTLEFS_ERROR_INVALID_PARAM;
    }
}

/**
 * @brief 配置littlefs配置结构体
 */
static LittleFS_Status_t LittleFS_ConfigInit(void)
{
    const w25q_dev_t* dev_info = W25Q_GetInfo();
    if (dev_info == NULL) {
        return LITTLEFS_ERROR_NOT_INIT;
    }
    
    /* 验证设备信息有效性 */
    if (dev_info->capacity_mb == 0) {
        return LITTLEFS_ERROR_NOT_INIT;
    }
    
    /* 计算总容量和块数 */
    uint32_t total_bytes = dev_info->capacity_mb * 1024 * 1024;
    uint32_t block_count = total_bytes / W25Q_SECTOR_SIZE;
    
    /* 验证块数有效性 */
    if (block_count == 0) {
        return LITTLEFS_ERROR_NOT_INIT;
    }
    
    /* 配置块设备回调函数 */
    g_littlefs_device.config.read = littlefs_bd_read;
    g_littlefs_device.config.prog = littlefs_bd_prog;
    g_littlefs_device.config.erase = littlefs_bd_erase;
    g_littlefs_device.config.sync = littlefs_bd_sync;
    
    /* 配置块设备参数 */
    g_littlefs_device.config.read_size = W25Q_PAGE_SIZE;        /* 最小读取大小 = 页大小 */
    g_littlefs_device.config.prog_size = W25Q_PAGE_SIZE;        /* 最小编程大小 = 页大小 */
    g_littlefs_device.config.block_size = W25Q_SECTOR_SIZE;     /* 块大小 = 扇区大小 */
    g_littlefs_device.config.block_count = block_count;        /* 块数 */
    g_littlefs_device.config.block_cycles = 500;               /* 磨损均衡周期，建议100-1000 */
    /* 关键修复：缓存大小改为页大小（256字节），而不是扇区大小（4096字节），减少RAM占用 */
    /* littlefs会自动处理跨页操作，使用较小的缓存不会影响功能 */
    g_littlefs_device.config.cache_size = W25Q_PAGE_SIZE;     /* 缓存大小 = 页大小（256字节，减少RAM占用） */
    
    /* 配置前瞻缓冲区大小（建议为block_count的1/8，最小8字节） */
    uint32_t lookahead_size = block_count / 8;
    if (lookahead_size < 8) {
        lookahead_size = 8;
    }
    if (lookahead_size > sizeof(g_littlefs_device.lookahead_buffer)) {
        lookahead_size = sizeof(g_littlefs_device.lookahead_buffer);
    }
    /* 确保lookahead_size是8的倍数（littlefs要求） */
    lookahead_size = (lookahead_size / 8) * 8;
    if (lookahead_size < 8) {
        lookahead_size = 8;
    }
    g_littlefs_device.config.lookahead_size = lookahead_size;
    
    /* 配置静态缓冲区 */
    g_littlefs_device.config.read_buffer = g_littlefs_device.read_buffer;
    g_littlefs_device.config.prog_buffer = g_littlefs_device.prog_buffer;
    g_littlefs_device.config.lookahead_buffer = g_littlefs_device.lookahead_buffer;
    
    /* 其他配置 */
    g_littlefs_device.config.context = NULL;
    g_littlefs_device.config.name_max = 0;  /* 使用默认值LFS_NAME_MAX */
    g_littlefs_device.config.file_max = 0;  /* 使用默认值LFS_FILE_MAX */
    g_littlefs_device.config.attr_max = 0;  /* 使用默认值LFS_ATTR_MAX */
    
    return LITTLEFS_OK;
}

/* ==================== 公共API实现 ==================== */

/**
 * @brief LittleFS初始化
 */
LittleFS_Status_t LittleFS_Init(void)
{
    /* 检查是否已初始化 */
    if (g_littlefs_device.state != LITTLEFS_STATE_UNINITIALIZED) {
        return LITTLEFS_OK;
    }
    
    /* 关键修复：手动初始化全局变量，避免启动时的初始化问题 */
    /* 如果全局变量在启动时初始化失败，会导致程序无法启动 */
    /* 改为在函数内部手动初始化，确保在系统完全启动后再分配 */
    memset(&g_littlefs_device, 0, sizeof(littlefs_dev_t));
    g_littlefs_device.state = LITTLEFS_STATE_UNINITIALIZED;
    
    /* 检查W25Q是否已初始化 */
    if (!W25Q_IsInitialized()) {
        return LITTLEFS_ERROR_NOT_INIT;
    }
    
    /* 初始化lfs结构体（清零） */
    memset(&g_littlefs_device.lfs, 0, sizeof(lfs_t));
    
    /* 配置littlefs配置结构体 */
    LittleFS_Status_t status = LittleFS_ConfigInit();
    if (status != LITTLEFS_OK) {
        return status;
    }
    
    /* 标记为已初始化 */
    g_littlefs_device.state = LITTLEFS_STATE_INITIALIZED;
    
    return LITTLEFS_OK;
}

/**
 * @brief LittleFS反初始化
 */
LittleFS_Status_t LittleFS_Deinit(void)
{
    /* 如果已挂载，先卸载 */
    if (g_littlefs_device.state == LITTLEFS_STATE_MOUNTED) {
        LittleFS_Unmount();
    }
    
    /* 清除状态 */
    g_littlefs_device.state = LITTLEFS_STATE_UNINITIALIZED;
    memset(&g_littlefs_device.lfs, 0, sizeof(lfs_t));
    memset(&g_littlefs_device.config, 0, sizeof(struct lfs_config));
    
    return LITTLEFS_OK;
}

/**
 * @brief 检查LittleFS是否已初始化
 */
uint8_t LittleFS_IsInitialized(void)
{
    return (g_littlefs_device.state != LITTLEFS_STATE_UNINITIALIZED) ? 1 : 0;
}

/**
 * @brief 挂载文件系统
 */
LittleFS_Status_t LittleFS_Mount(void)
{
    /* 检查是否已初始化 */
    if (g_littlefs_device.state == LITTLEFS_STATE_UNINITIALIZED) {
        return LITTLEFS_ERROR_NOT_INIT;
    }
    
    /* 如果已挂载，直接返回成功 */
    if (g_littlefs_device.state == LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_OK;
    }
    
    /* 挂载前：确保CS引脚正确配置和初始状态 */
    /* 关键：CS引脚必须配置为推挽输出，初始状态为高（释放） */
    /* 注意：挂载过程中littlefs会调用块设备回调函数，会操作CS引脚 */
    /* 重要：防止软件推挽输出与硬件NSS冲突导致电压跌落（3.1V -> 2.6V） */
    #ifdef CONFIG_MODULE_W25Q_ENABLED
    #if CONFIG_MODULE_W25Q_ENABLED
    SPI_Instance_t spi_instance = W25Q_SPI_INSTANCE;
    
    /* 确保CS引脚为高（释放状态） */
    SPI_NSS_High(spi_instance);
    
    /* 短暂延时确保CS稳定 */
    Delay_us(10);
    
    /* 关键修复：再次锁定CS引脚配置，防止被SPI外设或软件冲突修改 */
    /* 直接操作寄存器，强制锁定PA11为推挽输出（0b0011 = 0x3） */
    /* 注意：这里假设CS引脚是PA11，如果是其他引脚需要相应修改 */
    /* 方法：通过SPI配置获取CS引脚信息，但为了简化，这里直接操作GPIOA */
    /* 实际上，SPI_HW_Init()已经正确配置了CS引脚，这里只是确保不被改变 */
    #ifdef GPIOA
    /* 锁定PA11配置（如果CS是PA11） */
    /* 注意：这个检查是保守的，如果CS不是PA11，这个操作不会影响其他引脚 */
    /* 更安全的方法是：从SPI配置中获取CS引脚信息，但为了简化，这里先硬编码 */
    /* TODO: 从SPI配置中动态获取CS引脚信息 */
    GPIOA->CRH &= ~(0xF << ((11 - 8) * 4));  /* 清除PA11配置位 */
    GPIOA->CRH |= (0x3 << ((11 - 8) * 4));   /* 强制设置为推挽输出 */
    GPIOA->BSRR = GPIO_Pin_11;  /* 确保CS为高（释放状态） */
    #endif
    #endif
    #endif
    
    /* 挂载文件系统 */
    /* 注意：lfs_mount()内部会扫描Flash查找superblock，可能会多次调用littlefs_bd_read() */
    /* 如果Flash上没有有效的文件系统，可能会扫描很多块，但不会无限循环 */
    /* 关键：如果挂载卡住，问题可能在：
     * 1. lfs_mount()内部扫描时调用littlefs_bd_read()
     * 2. littlefs_bd_read()调用W25Q_Read()
     * 3. W25Q_Read()调用SPI_MasterReceive()时超时或卡住
     * 4. SPI_MasterReceive()等待SPI标志位时超时机制失效
     */
    int lfs_err = lfs_mount(&g_littlefs_device.lfs, &g_littlefs_device.config);
    if (lfs_err == LFS_ERR_OK) {
        g_littlefs_device.state = LITTLEFS_STATE_MOUNTED;
        return LITTLEFS_OK;
    }
    
    return LittleFS_ConvertError(lfs_err);
}

/**
 * @brief 卸载文件系统
 */
LittleFS_Status_t LittleFS_Unmount(void)
{
    /* 检查是否已挂载 */
    if (g_littlefs_device.state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_OK;  /* 未挂载，直接返回成功 */
    }
    
    /* 卸载文件系统 */
    int lfs_err = lfs_unmount(&g_littlefs_device.lfs);
    if (lfs_err == LFS_ERR_OK) {
        g_littlefs_device.state = LITTLEFS_STATE_INITIALIZED;
        return LITTLEFS_OK;
    }
    
    return LittleFS_ConvertError(lfs_err);
}

/**
 * @brief 格式化文件系统
 */
LittleFS_Status_t LittleFS_Format(void)
{
    /* 检查是否已初始化 */
    if (g_littlefs_device.state == LITTLEFS_STATE_UNINITIALIZED) {
        return LITTLEFS_ERROR_NOT_INIT;
    }
    
    /* 如果已挂载，先卸载 */
    if (g_littlefs_device.state == LITTLEFS_STATE_MOUNTED) {
        LittleFS_Unmount();
    }
    
    /* 格式化文件系统 */
    int lfs_err = lfs_format(&g_littlefs_device.lfs, &g_littlefs_device.config);
    if (lfs_err == LFS_ERR_OK) {
        return LITTLEFS_OK;
    }
    
    return LittleFS_ConvertError(lfs_err);
}

/**
 * @brief 获取文件系统信息
 */
LittleFS_Status_t LittleFS_GetInfo(uint64_t *total_bytes, uint64_t *free_bytes)
{
    /* 参数校验 */
    if (total_bytes == NULL || free_bytes == NULL) {
        return LITTLEFS_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已挂载 */
    if (g_littlefs_device.state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 计算总空间 */
    *total_bytes = (uint64_t)g_littlefs_device.config.block_count * g_littlefs_device.config.block_size;
    
    /* 计算空闲空间：使用lfs_fs_size获取已用空间 */
    lfs_ssize_t used_size = lfs_fs_size(&g_littlefs_device.lfs);
    if (used_size >= 0) {
        *free_bytes = *total_bytes - (uint64_t)used_size;
    } else {
        /* 如果无法获取，使用块数估算 */
        *free_bytes = *total_bytes;  /* 保守估计，假设全部空闲 */
    }
    
    return LITTLEFS_OK;
}

/* ==================== 文件操作 ==================== */

/**
 * @brief 打开文件
 */
LittleFS_Status_t LittleFS_FileOpen(lfs_file_t *file, const char *path, int flags)
{
    /* 参数校验 */
    if (file == NULL || path == NULL) {
        return LITTLEFS_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已挂载 */
    if (g_littlefs_device.state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 配置文件缓冲区（每个打开的文件需要一个cache_size大小的缓冲区） */
    /* 关键：littlefs 2.3 需要为每个文件提供缓冲区，否则会返回 LFS_ERR_NOMEM */
    struct lfs_file_config file_cfg = {
        .buffer = g_littlefs_device.file_buffer,  /* 使用静态分配的文件缓冲区 */
        .attr_count = 0  /* 不使用文件属性 */
    };
    
    /* 打开文件（使用文件配置） */
    int lfs_err = lfs_file_opencfg(&g_littlefs_device.lfs, file, path, flags, &file_cfg);
    return LittleFS_ConvertError(lfs_err);
}

/**
 * @brief 关闭文件
 */
LittleFS_Status_t LittleFS_FileClose(lfs_file_t *file)
{
    /* 参数校验 */
    if (file == NULL) {
        return LITTLEFS_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已挂载 */
    if (g_littlefs_device.state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 关闭文件 */
    int lfs_err = lfs_file_close(&g_littlefs_device.lfs, file);
    return LittleFS_ConvertError(lfs_err);
}

/**
 * @brief 读取文件
 */
LittleFS_Status_t LittleFS_FileRead(lfs_file_t *file, void *buffer, uint32_t size, uint32_t *bytes_read)
{
    /* 参数校验 */
    if (file == NULL || buffer == NULL) {
        return LITTLEFS_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已挂载 */
    if (g_littlefs_device.state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 读取文件 */
    lfs_ssize_t result = lfs_file_read(&g_littlefs_device.lfs, file, buffer, size);
    if (result < 0) {
        return LittleFS_ConvertError((int)result);
    }
    
    if (bytes_read != NULL) {
        *bytes_read = (uint32_t)result;
    }
    
    return LITTLEFS_OK;
}

/**
 * @brief 写入文件
 */
LittleFS_Status_t LittleFS_FileWrite(lfs_file_t *file, const void *buffer, uint32_t size, uint32_t *bytes_written)
{
    /* 参数校验 */
    if (file == NULL || buffer == NULL) {
        return LITTLEFS_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已挂载 */
    if (g_littlefs_device.state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 写入文件 */
    lfs_ssize_t result = lfs_file_write(&g_littlefs_device.lfs, file, buffer, size);
    if (result < 0) {
        return LittleFS_ConvertError((int)result);
    }
    
    if (bytes_written != NULL) {
        *bytes_written = (uint32_t)result;
    }
    
    return LITTLEFS_OK;
}

/**
 * @brief 文件定位
 */
LittleFS_Status_t LittleFS_FileSeek(lfs_file_t *file, int32_t offset, int whence)
{
    /* 参数校验 */
    if (file == NULL) {
        return LITTLEFS_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已挂载 */
    if (g_littlefs_device.state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 文件定位 */
    lfs_soff_t result = lfs_file_seek(&g_littlefs_device.lfs, file, offset, whence);
    if (result < 0) {
        return LittleFS_ConvertError((int)result);
    }
    
    return LITTLEFS_OK;
}

/**
 * @brief 获取文件大小
 */
LittleFS_Status_t LittleFS_FileSize(lfs_file_t *file, uint32_t *size)
{
    /* 参数校验 */
    if (file == NULL || size == NULL) {
        return LITTLEFS_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已挂载 */
    if (g_littlefs_device.state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 获取文件大小 */
    lfs_soff_t result = lfs_file_size(&g_littlefs_device.lfs, file);
    if (result < 0) {
        return LittleFS_ConvertError((int)result);
    }
    
    *size = (uint32_t)result;
    return LITTLEFS_OK;
}

/**
 * @brief 截断文件
 */
LittleFS_Status_t LittleFS_FileTruncate(lfs_file_t *file, uint32_t size)
{
    /* 参数校验 */
    if (file == NULL) {
        return LITTLEFS_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已挂载 */
    if (g_littlefs_device.state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 截断文件 */
    int lfs_err = lfs_file_truncate(&g_littlefs_device.lfs, file, size);
    return LittleFS_ConvertError(lfs_err);
}

/**
 * @brief 同步文件
 */
LittleFS_Status_t LittleFS_FileSync(lfs_file_t *file)
{
    /* 参数校验 */
    if (file == NULL) {
        return LITTLEFS_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已挂载 */
    if (g_littlefs_device.state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 同步文件 */
    int lfs_err = lfs_file_sync(&g_littlefs_device.lfs, file);
    return LittleFS_ConvertError(lfs_err);
}

/**
 * @brief 删除文件
 */
LittleFS_Status_t LittleFS_FileDelete(const char *path)
{
    /* 参数校验 */
    if (path == NULL) {
        return LITTLEFS_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已挂载 */
    if (g_littlefs_device.state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 删除文件 */
    int lfs_err = lfs_remove(&g_littlefs_device.lfs, path);
    return LittleFS_ConvertError(lfs_err);
}

/**
 * @brief 重命名文件
 */
LittleFS_Status_t LittleFS_FileRename(const char *old_path, const char *new_path)
{
    /* 参数校验 */
    if (old_path == NULL || new_path == NULL) {
        return LITTLEFS_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已挂载 */
    if (g_littlefs_device.state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 重命名文件 */
    int lfs_err = lfs_rename(&g_littlefs_device.lfs, old_path, new_path);
    return LittleFS_ConvertError(lfs_err);
}

/* ==================== 目录操作 ==================== */

/**
 * @brief 打开目录
 */
LittleFS_Status_t LittleFS_DirOpen(lfs_dir_t *dir, const char *path)
{
    /* 参数校验 */
    if (dir == NULL || path == NULL) {
        return LITTLEFS_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已挂载 */
    if (g_littlefs_device.state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 打开目录 */
    int lfs_err = lfs_dir_open(&g_littlefs_device.lfs, dir, path);
    return LittleFS_ConvertError(lfs_err);
}

/**
 * @brief 关闭目录
 */
LittleFS_Status_t LittleFS_DirClose(lfs_dir_t *dir)
{
    /* 参数校验 */
    if (dir == NULL) {
        return LITTLEFS_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已挂载 */
    if (g_littlefs_device.state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 关闭目录 */
    int lfs_err = lfs_dir_close(&g_littlefs_device.lfs, dir);
    return LittleFS_ConvertError(lfs_err);
}

/**
 * @brief 读取目录项
 */
LittleFS_Status_t LittleFS_DirRead(lfs_dir_t *dir, struct lfs_info *info)
{
    /* 参数校验 */
    if (dir == NULL || info == NULL) {
        return LITTLEFS_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已挂载 */
    if (g_littlefs_device.state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 读取目录项 */
    /* 关键：lfs_dir_read 的返回值语义与 lfs_file_read 不同！
     *   lfs_dir_read 返回：
     *     正数（通常是1） - 成功读取一个目录项
     *     0 - 没有更多条目（目录读取完毕，这是正常情况，不是错误！）
     *     负数错误码 - 实际错误（如 LFS_ERR_IO, LFS_ERR_CORRUPT 等）
     * 
     * 注意：不能直接将返回值传给 LittleFS_ConvertError，因为 0 会被转换为 LITTLEFS_OK，
     * 但实际上 0 表示目录读取完毕，应该返回 LITTLEFS_ERROR_NOENT。
     */
    int lfs_result = lfs_dir_read(&g_littlefs_device.lfs, dir, info);
    
    /* 处理返回值 */
    if (lfs_result > 0) {
        /* 成功读取一个目录项 */
        return LITTLEFS_OK;
    } else if (lfs_result == 0) {
        /* 目录读取完毕（没有更多条目），这是正常情况，返回 LITTLEFS_ERROR_NOENT */
        return LITTLEFS_ERROR_NOENT;
    } else {
        /* 实际错误，转换错误码 */
        return LittleFS_ConvertError(lfs_result);
    }
}

/**
 * @brief 创建目录
 */
LittleFS_Status_t LittleFS_DirCreate(const char *path)
{
    /* 参数校验 */
    if (path == NULL) {
        return LITTLEFS_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已挂载 */
    if (g_littlefs_device.state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 创建目录 */
    int lfs_err = lfs_mkdir(&g_littlefs_device.lfs, path);
    return LittleFS_ConvertError(lfs_err);
}

/**
 * @brief 删除目录
 */
LittleFS_Status_t LittleFS_DirDelete(const char *path)
{
    /* 参数校验 */
    if (path == NULL) {
        return LITTLEFS_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已挂载 */
    if (g_littlefs_device.state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 删除目录 */
    int lfs_err = lfs_remove(&g_littlefs_device.lfs, path);
    return LittleFS_ConvertError(lfs_err);
}

/* ==================== 文件属性操作 ==================== */

/**
 * @brief 设置文件属性
 */
LittleFS_Status_t LittleFS_FileSetAttr(const char *path, uint8_t type, const void *buffer, uint32_t size)
{
    /* 参数校验 */
    if (path == NULL || buffer == NULL) {
        return LITTLEFS_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已挂载 */
    if (g_littlefs_device.state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 设置文件属性 */
    int lfs_err = lfs_setattr(&g_littlefs_device.lfs, path, type, buffer, size);
    return LittleFS_ConvertError(lfs_err);
}

/**
 * @brief 获取文件属性
 */
LittleFS_Status_t LittleFS_FileGetAttr(const char *path, uint8_t type, void *buffer, uint32_t size, uint32_t *actual_size)
{
    /* 参数校验 */
    if (path == NULL || buffer == NULL) {
        return LITTLEFS_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已挂载 */
    if (g_littlefs_device.state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 获取文件属性 */
    lfs_ssize_t result = lfs_getattr(&g_littlefs_device.lfs, path, type, buffer, size);
    if (result < 0) {
        return LittleFS_ConvertError((int)result);
    }
    
    if (actual_size != NULL) {
        *actual_size = (uint32_t)result;
    }
    
    return LITTLEFS_OK;
}

/**
 * @brief 移除文件属性
 */
LittleFS_Status_t LittleFS_FileRemoveAttr(const char *path, uint8_t type)
{
    /* 参数校验 */
    if (path == NULL) {
        return LITTLEFS_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已挂载 */
    if (g_littlefs_device.state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 移除文件属性 */
    int lfs_err = lfs_removeattr(&g_littlefs_device.lfs, path, type);
    return LittleFS_ConvertError(lfs_err);
}

#endif /* CONFIG_MODULE_W25Q_ENABLED */
#endif /* CONFIG_MODULE_W25Q_ENABLED */
#endif /* CONFIG_MODULE_LITTLEFS_ENABLED */
#endif /* CONFIG_MODULE_LITTLEFS_ENABLED */

