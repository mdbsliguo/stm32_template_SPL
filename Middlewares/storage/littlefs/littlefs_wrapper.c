/**
 * @file littlefs_wrapper.c
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

#include "littlefs_wrapper.h"
#include "config.h"
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
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

/* ==================== 对齐宏定义 ==================== */
/**
 * @brief 统一的对齐宏（兼容Keil和GCC）
 * @note F103栈区不保证对齐，必须使用全局/静态变量+强制对齐
 */
#ifdef __CC_ARM
    #define LFS_ALIGN(N) __align(N)
#else
    #define LFS_ALIGN(N) __attribute__((aligned(N)))
#endif

/* ==================== 内部常量定义 ==================== */

#define LITTLEFS_DEFAULT_READ_SIZE      256     /**< 默认读取大小（性能优化配置） */
#define LITTLEFS_DEFAULT_PROG_SIZE      256     /**< 默认编程大小（性能优化配置） */
#define LITTLEFS_DEFAULT_BLOCK_SIZE     4096    /**< 默认块大小（W25Q扇区大小） */
#define LITTLEFS_DEFAULT_BLOCK_CYCLES   1000    /**< 默认磨损均衡周期（性能优化：减少搬迁） */
#define LITTLEFS_DEFAULT_CACHE_SIZE     256     /**< 默认缓存大小（性能优化配置） */
#define LITTLEFS_MIN_LOOKAHEAD_SIZE     8       /**< 最小前瞻缓冲区大小（字节） */
#define LITTLEFS_MAX_LOOKAHEAD_SIZE     256     /**< 最大前瞻缓冲区大小（性能优化配置） */

/* ==================== 全局状态变量 ==================== */

/**
 * @brief LittleFS缓冲区（独立静态全局变量，确保4字节对齐）
 * @note F103栈区不保证对齐，必须使用全局/静态变量 + 强制对齐
 * @note Keil ARMCC: 使用 __align(4) static
 * @note GCC: 使用 __attribute__((aligned(4))) static
 * @note 缓冲区大小必须 >= read_size, prog_size, cache_size, lookahead_size
 */
#ifdef __CC_ARM
__align(4) static uint8_t g_littlefs_read_buffer[LITTLEFS_DEFAULT_READ_SIZE];
__align(4) static uint8_t g_littlefs_prog_buffer[LITTLEFS_DEFAULT_PROG_SIZE];
__align(4) static uint8_t g_littlefs_lookahead_buffer[LITTLEFS_MAX_LOOKAHEAD_SIZE];
#else
static uint8_t g_littlefs_read_buffer[LITTLEFS_DEFAULT_READ_SIZE] __attribute__((aligned(4)));
static uint8_t g_littlefs_prog_buffer[LITTLEFS_DEFAULT_PROG_SIZE] __attribute__((aligned(4)));
static uint8_t g_littlefs_lookahead_buffer[LITTLEFS_MAX_LOOKAHEAD_SIZE] __attribute__((aligned(4)));
#endif

/**
 * @brief LittleFS设备状态结构体
 * @note 缓冲区已移出结构体，改为独立的静态全局变量（ARMCC兼容性要求）
 */
typedef struct {
    LittleFS_State_t state;                    /**< 模块状态 */
    lfs_t lfs;                                 /**< littlefs文件系统对象 */
    struct lfs_config config;                 /**< littlefs配置结构体 */
    LittleFS_Config_t user_config;            /**< 用户配置结构体 */
    const w25q_dev_t* cached_dev_info;        /**< 缓存的W25Q设备信息（优化：避免重复调用W25Q_GetInfo） */
    uint32_t cached_max_addr;                 /**< 缓存的最大地址（优化：避免重复计算） */
    LittleFS_LogCallback_t log_callback;      /**< 日志回调函数 */
    LittleFS_LockCallback_t lock_callback;    /**< RTOS锁回调函数 */
    LittleFS_UnlockCallback_t unlock_callback; /**< RTOS解锁回调函数 */
    void *lock_context;                        /**< 锁回调函数的用户上下文 */
} littlefs_dev_t;

/* 多实例支持：支持最多LITTLEFS_INSTANCE_MAX个实例 */
/* 确保结构体4字节对齐，保证内部缓冲区对齐 */
#ifdef __CC_ARM
__align(4) static littlefs_dev_t g_littlefs_devices[LITTLEFS_INSTANCE_MAX];
#else
static littlefs_dev_t g_littlefs_devices[LITTLEFS_INSTANCE_MAX] __attribute__((aligned(4)));
#endif

/* 默认实例编号（向后兼容） */
#define LITTLEFS_DEFAULT_INSTANCE LITTLEFS_INSTANCE_0

/* ==================== 内部宏定义（优化） ==================== */

/**
 * @brief RTOS锁保护宏（优化：统一锁操作，减少代码重复）
 * @param dev 设备结构体指针
 */
#define LITTLEFS_LOCK(dev) \
    do { \
        if ((dev) != NULL && (dev)->lock_callback != NULL) { \
            int _lock_result = (dev)->lock_callback((dev)->lock_context); \
            if (_lock_result != 0) { \
                return LFS_ERR_IO; \
            } \
        } \
    } while(0)

/**
 * @brief RTOS解锁宏（优化：统一解锁操作，减少代码重复）
 * @param dev 设备结构体指针
 */
#define LITTLEFS_UNLOCK(dev) \
    do { \
        if ((dev) != NULL && (dev)->unlock_callback != NULL) { \
            (dev)->unlock_callback((dev)->lock_context); \
        } \
    } while(0)

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 从context获取实例编号
 * @param[in] context 用户上下文指针
 * @return LittleFS_Instance_t 实例编号，无效时返回LITTLEFS_INSTANCE_MAX
 */
static LittleFS_Instance_t littlefs_get_instance_from_context(void *context)
{
    if (context == NULL) {
        return LITTLEFS_DEFAULT_INSTANCE;  /* 默认实例 */
    }
    
    /* context指向实例编号（转换为指针） */
    uintptr_t instance_ptr = (uintptr_t)context;
    if (instance_ptr < LITTLEFS_INSTANCE_MAX) {
        return (LittleFS_Instance_t)instance_ptr;
    }
    
    return LITTLEFS_INSTANCE_MAX;  /* 无效 */
}

/**
 * @brief 检查地址范围（优化：统一地址范围检查，减少代码重复）
 * @param[in] dev 设备结构体指针
 * @param[in] addr 起始地址
 * @param[in] size 数据大小
 * @return int 0=地址有效，-1=地址超出范围
 */
static int littlefs_check_addr_range(littlefs_dev_t *dev, uint32_t addr, uint32_t size)
{
    if (dev == NULL) {
        return -1;  /* 设备无效 */
    }
    
    /* 使用缓存的最大地址（优化：避免重复调用W25Q_GetInfo） */
    if (dev->cached_max_addr == 0) {
        /* 缓存未初始化，这是异常情况，应该返回错误 */
        /* 正常情况下，cached_max_addr应该在初始化时设置 */
        return -1;  /* 设备未初始化，地址无效 */
    }
    
    if (addr >= dev->cached_max_addr || (addr + size) > dev->cached_max_addr) {
        return -1;  /* 地址超出范围 */
    }
    
    return 0;  /* 地址有效 */
}

/**
 * @brief 获取设备结构体指针
 * @param[in] instance 实例编号
 * @return littlefs_dev_t* 设备结构体指针，NULL=无效实例
 */
static littlefs_dev_t* littlefs_get_device(LittleFS_Instance_t instance)
{
    if (instance >= LITTLEFS_INSTANCE_MAX) {
        return NULL;
    }
    return &g_littlefs_devices[instance];
}

/**
 * @brief 日志输出（内部使用）
 * @param[in] instance 实例编号
 * @param[in] level 日志级别（0=DEBUG, 1=INFO, 2=WARN, 3=ERROR）
 * @param[in] format 格式化字符串
 * @param[in] ... 可变参数
 * @note 由于C标准不支持将va_list传递给可变参数函数，使用vsnprintf格式化后再调用回调
 */
static void littlefs_log_internal(LittleFS_Instance_t instance, int level, const char *format, ...)
{
    littlefs_dev_t *dev = littlefs_get_device(instance);
    if (dev == NULL || dev->log_callback == NULL) {
        return;
    }
    
    /* 使用静态缓冲区格式化字符串（最大256字节） */
    static char log_buffer[256];
    va_list args;
    va_start(args, format);
    
    /* 使用vsnprintf格式化字符串 */
    int len = vsnprintf(log_buffer, sizeof(log_buffer), format, args);
    va_end(args);
    
    if (len > 0 && len < (int)sizeof(log_buffer)) {
        /* 格式化成功，调用回调函数（传递格式化后的字符串） */
        /* 注意：回调函数类型是可变参数的，但我们已经格式化了字符串 */
        /* 这里使用"%s"作为format，log_buffer作为参数 */
        dev->log_callback(level, "%s", log_buffer);
    } else if (len >= (int)sizeof(log_buffer)) {
        /* 缓冲区太小，截断 */
        log_buffer[sizeof(log_buffer) - 1] = '\0';
        dev->log_callback(level, "%s", log_buffer);
    }
}

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
    
    /* 从context获取实例编号 */
    LittleFS_Instance_t instance = littlefs_get_instance_from_context(c->context);
    littlefs_dev_t *dev = littlefs_get_device(instance);
    if (dev == NULL) {
        return LFS_ERR_IO;
    }
    
    /* RTOS锁保护（优化：使用宏统一锁操作） */
    LITTLEFS_LOCK(dev);
    
    /* 检查W25Q是否已初始化 */
    if (!W25Q_IsInitialized()) {
        LITTLEFS_UNLOCK(dev);
        return LFS_ERR_IO;
    }
    
    /* 计算Flash地址：块号 * 块大小 + 偏移 */
    uint32_t block_size = c->block_size;
    uint32_t addr = (uint32_t)block * block_size + (uint32_t)off;
    
    /* 检查地址是否超出范围（优化：使用统一的地址范围检查函数，使用缓存的最大地址） */
    if (littlefs_check_addr_range(dev, addr, size) != 0) {
        LITTLEFS_UNLOCK(dev);
        return LFS_ERR_IO;  /* 地址超出范围 */
    }
    
    /* 挂载前：确保CS引脚为高（释放状态） */
    #ifdef CONFIG_MODULE_W25Q_ENABLED
    #if CONFIG_MODULE_W25Q_ENABLED
    SPI_Instance_t spi_instance = W25Q_SPI_INSTANCE;
    SPI_NSS_High(spi_instance);  /* 确保CS为高（释放） */
    Delay_us(5);  /* 短暂延时确保CS稳定 */
    #endif
    #endif
    
    /* 读取数据 */
    W25Q_Status_t status = W25Q_Read(addr, (uint8_t*)buffer, (uint32_t)size);
    
    /* RTOS解锁（优化：使用宏统一解锁操作） */
    LITTLEFS_UNLOCK(dev);
    
    if (status != W25Q_OK) {
        littlefs_log_internal(instance, 3, "littlefs_bd_read: W25Q_Read failed: %d", status);
        return LFS_ERR_IO;
    }
    
    if (dev->user_config.debug_enabled) {
        littlefs_log_internal(instance, 0, "littlefs_bd_read: block=%lu, off=%lu, size=%lu, addr=0x%08lX", 
                              (unsigned long)block, (unsigned long)off, (unsigned long)size, (unsigned long)addr);
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
    
    /* 从context获取实例编号 */
    LittleFS_Instance_t instance = littlefs_get_instance_from_context(c->context);
    littlefs_dev_t *dev = littlefs_get_device(instance);
    if (dev == NULL) {
        return LFS_ERR_IO;
    }
    
    /* RTOS锁保护（优化：使用宏统一锁操作） */
    LITTLEFS_LOCK(dev);
    
    /* 检查W25Q是否已初始化 */
    if (!W25Q_IsInitialized()) {
        LITTLEFS_UNLOCK(dev);
        return LFS_ERR_IO;
    }
    
    /* 计算Flash地址：块号 * 块大小 + 偏移 */
    uint32_t block_size = c->block_size;
    uint32_t addr = (uint32_t)block * block_size + (uint32_t)off;
    
    /* 检查地址是否超出范围（优化：使用统一的地址范围检查函数，使用缓存的最大地址） */
    if (littlefs_check_addr_range(dev, addr, size) != 0) {
        LITTLEFS_UNLOCK(dev);
        return LFS_ERR_IO;  /* 地址超出范围 */
    }
    
    /* 关中断确保写入原子性（防止CTZ头写入被中断覆盖） */
    /* 保存当前中断状态并关闭中断 */
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    
    /* 写入数据（CS锁定由W25Q_Write内部处理） */
    W25Q_Status_t status = W25Q_Write(addr, (const uint8_t*)buffer, (uint32_t)size);
    
    /* 恢复中断状态 */
    if ((primask & 1) == 0) {
        __enable_irq();
    }
    
    /* RTOS解锁（优化：使用宏统一解锁操作） */
    LITTLEFS_UNLOCK(dev);
    
    if (status != W25Q_OK) {
        littlefs_log_internal(instance, 3, "littlefs_bd_prog: W25Q_Write failed: %d", status);
        return LFS_ERR_IO;
    }
    
    if (dev->user_config.debug_enabled) {
        littlefs_log_internal(instance, 0, "littlefs_bd_prog: block=%lu, off=%lu, size=%lu, addr=0x%08lX", 
                              (unsigned long)block, (unsigned long)off, (unsigned long)size, (unsigned long)addr);
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
    
    /* 从context获取实例编号 */
    LittleFS_Instance_t instance = littlefs_get_instance_from_context(c->context);
    littlefs_dev_t *dev = littlefs_get_device(instance);
    if (dev == NULL) {
        return LFS_ERR_IO;
    }
    
    /* RTOS锁保护（优化：使用宏统一锁操作） */
    LITTLEFS_LOCK(dev);
    
    /* 检查W25Q是否已初始化 */
    if (!W25Q_IsInitialized()) {
        LITTLEFS_UNLOCK(dev);
        return LFS_ERR_IO;
    }
    
    /* 计算Flash地址：块号 * 块大小 */
    uint32_t block_size = c->block_size;
    uint32_t addr = (uint32_t)block * block_size;
    
    /* 检查地址是否超出范围（优化：使用统一的地址范围检查函数，使用缓存的最大地址） */
    if (littlefs_check_addr_range(dev, addr, block_size) != 0) {
        LITTLEFS_UNLOCK(dev);
        return LFS_ERR_IO;  /* 地址超出范围 */
    }
    
    /* 擦除扇区 */
    W25Q_Status_t status = W25Q_EraseSector(addr);
    
    /* RTOS解锁（优化：使用宏统一解锁操作） */
    LITTLEFS_UNLOCK(dev);
    
    if (status != W25Q_OK) {
        littlefs_log_internal(instance, 3, "littlefs_bd_erase: W25Q_EraseSector failed: %d", status);
        return LFS_ERR_IO;
    }
    
    if (dev->user_config.debug_enabled) {
        littlefs_log_internal(instance, 0, "littlefs_bd_erase: block=%lu, addr=0x%08lX", 
                              (unsigned long)block, (unsigned long)addr);
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
    
    /* 从context获取实例编号 */
    LittleFS_Instance_t instance = littlefs_get_instance_from_context(c->context);
    littlefs_dev_t *dev = littlefs_get_device(instance);
    if (dev == NULL) {
        return LFS_ERR_IO;
    }
    
    /* RTOS锁保护（优化：使用宏统一锁操作） */
    LITTLEFS_LOCK(dev);
    
    /* 检查W25Q是否已初始化 */
    if (!W25Q_IsInitialized()) {
        LITTLEFS_UNLOCK(dev);
        return LFS_ERR_IO;
    }
    
    /* 等待Flash就绪 */
    W25Q_Status_t status = W25Q_WaitReady(0);
    
    /* RTOS解锁（优化：使用宏统一解锁操作） */
    LITTLEFS_UNLOCK(dev);
    
    if (status != W25Q_OK) {
        littlefs_log_internal(instance, 3, "littlefs_bd_sync: W25Q_WaitReady failed: %d", status);
        return LFS_ERR_IO;
    }
    
    if (dev->user_config.debug_enabled) {
        littlefs_log_internal(instance, 0, "littlefs_bd_sync: completed");
    }
    
    return LFS_ERR_OK;
}

#ifdef LFS_THREADSAFE
/**
 * @brief RTOS锁回调函数
 */
static int littlefs_bd_lock(const struct lfs_config* c)
{
    if (c == NULL) {
        return -1;
    }
    
    LittleFS_Instance_t instance = littlefs_get_instance_from_context(c->context);
    littlefs_dev_t *dev = littlefs_get_device(instance);
    if (dev == NULL || dev->lock_callback == NULL) {
        return -1;
    }
    
    return dev->lock_callback(dev->lock_context);
}

/**
 * @brief RTOS解锁回调函数
 */
static int littlefs_bd_unlock(const struct lfs_config* c)
{
    if (c == NULL) {
        return -1;
    }
    
    LittleFS_Instance_t instance = littlefs_get_instance_from_context(c->context);
    littlefs_dev_t *dev = littlefs_get_device(instance);
    if (dev == NULL || dev->unlock_callback == NULL) {
        return -1;
    }
    
    return dev->unlock_callback(dev->lock_context);
}
#endif

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
 * @brief 获取默认配置
 */
LittleFS_Status_t LittleFS_GetDefaultConfig(LittleFS_Config_t *config)
{
    /* 参数校验 */
    if (config == NULL) {
        return LITTLEFS_ERROR_INVALID_PARAM;
    }
    
    /* 获取W25Q设备信息 */
    const w25q_dev_t* dev_info = W25Q_GetInfo();
    if (dev_info == NULL || dev_info->capacity_mb == 0) {
        return LITTLEFS_ERROR_NOT_INIT;
    }
    
    /* 计算总容量和块数 */
    uint32_t total_bytes = dev_info->capacity_mb * 1024 * 1024;
    uint32_t block_count = total_bytes / LITTLEFS_DEFAULT_BLOCK_SIZE;
    
    /* 计算前瞻缓冲区大小 */
    uint32_t lookahead_size = block_count / 8;
    if (lookahead_size < LITTLEFS_MIN_LOOKAHEAD_SIZE) {
        lookahead_size = LITTLEFS_MIN_LOOKAHEAD_SIZE;
    }
    if (lookahead_size > LITTLEFS_MAX_LOOKAHEAD_SIZE) {
        lookahead_size = LITTLEFS_MAX_LOOKAHEAD_SIZE;
    }
    /* 确保lookahead_size是8的倍数 */
    lookahead_size = (lookahead_size / 8) * 8;
    if (lookahead_size < LITTLEFS_MIN_LOOKAHEAD_SIZE) {
        lookahead_size = LITTLEFS_MIN_LOOKAHEAD_SIZE;
    }
    
    /* 填充默认配置 */
    memset(config, 0, sizeof(LittleFS_Config_t));
    config->read_size = LITTLEFS_DEFAULT_READ_SIZE;
    config->prog_size = LITTLEFS_DEFAULT_PROG_SIZE;
    config->block_size = LITTLEFS_DEFAULT_BLOCK_SIZE;
    config->block_count = block_count;
    config->block_cycles = LITTLEFS_DEFAULT_BLOCK_CYCLES;
    config->cache_size = LITTLEFS_DEFAULT_CACHE_SIZE;
    config->lookahead_size = lookahead_size;
    config->name_max = 0;  /* 使用默认值LFS_NAME_MAX */
    config->file_max = 0;  /* 使用默认值LFS_FILE_MAX */
    config->attr_max = 0;  /* 使用默认值LFS_ATTR_MAX */
    config->log_callback = NULL;
    config->lock_callback = NULL;
    config->unlock_callback = NULL;
    config->lock_context = NULL;
    config->debug_enabled = 0;
    
    return LITTLEFS_OK;
}

/**
 * @brief 配置littlefs配置结构体（指定实例）
 * @param[in] instance 实例编号
 * @param[in] user_config 用户配置结构体指针（NULL=使用默认配置）
 * @return LittleFS_Status_t 错误码
 */
static LittleFS_Status_t LittleFS_ConfigInitInstance(LittleFS_Instance_t instance, const LittleFS_Config_t *user_config)
{
    /* 参数校验 */
    if (instance >= LITTLEFS_INSTANCE_MAX) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    littlefs_dev_t *dev = littlefs_get_device(instance);
    if (dev == NULL) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    /* 获取W25Q设备信息（优化：缓存设备信息，避免重复调用） */
    const w25q_dev_t* dev_info = W25Q_GetInfo();
    if (dev_info == NULL) {
        return LITTLEFS_ERROR_NOT_INIT;
    }
    
    /* 验证设备信息有效性 */
    if (dev_info->capacity_mb == 0) {
        return LITTLEFS_ERROR_NOT_INIT;
    }
    
    /* 缓存设备信息和最大地址（优化：避免在块设备操作中重复调用W25Q_GetInfo） */
    dev->cached_dev_info = dev_info;
    dev->cached_max_addr = dev_info->capacity_mb * 1024 * 1024;
    
    /* 获取用户配置或使用默认配置 */
    LittleFS_Config_t config;
    if (user_config != NULL) {
        memcpy(&config, user_config, sizeof(LittleFS_Config_t));
    } else {
        LittleFS_Status_t status = LittleFS_GetDefaultConfig(&config);
        if (status != LITTLEFS_OK) {
            return status;
        }
    }
    
    /* 保存用户配置 */
    memcpy(&dev->user_config, &config, sizeof(LittleFS_Config_t));
    
    /* 计算总容量和块数（如果用户未指定） */
    if (config.block_count == 0) {
        uint32_t total_bytes = dev_info->capacity_mb * 1024 * 1024;
        config.block_count = total_bytes / config.block_size;
    }
    
    /* 验证块数有效性 */
    if (config.block_count == 0) {
        return LITTLEFS_ERROR_NOT_INIT;
    }
    
    /* 配置块设备回调函数 */
    dev->config.read = littlefs_bd_read;
    dev->config.prog = littlefs_bd_prog;
    dev->config.erase = littlefs_bd_erase;
    dev->config.sync = littlefs_bd_sync;
    
    /* 配置RTOS锁回调（如果启用LFS_THREADSAFE） */
    #ifdef LFS_THREADSAFE
    if (config.lock_callback != NULL && config.unlock_callback != NULL) {
        dev->config.lock = littlefs_bd_lock;
        dev->config.unlock = littlefs_bd_unlock;
    } else {
        dev->config.lock = NULL;
        dev->config.unlock = NULL;
    }
    #endif
    
    /* 配置块设备参数 */
    dev->config.read_size = config.read_size;
    dev->config.prog_size = config.prog_size;
    dev->config.block_size = config.block_size;
    dev->config.block_count = config.block_count;
    dev->config.block_cycles = config.block_cycles;
    dev->config.cache_size = config.cache_size;
    
    /* 配置前瞻缓冲区大小 */
    uint32_t lookahead_size = config.lookahead_size;
    if (lookahead_size == 0) {
        lookahead_size = config.block_count / 8;
        if (lookahead_size < LITTLEFS_MIN_LOOKAHEAD_SIZE) {
            lookahead_size = LITTLEFS_MIN_LOOKAHEAD_SIZE;
        }
    }
    if (lookahead_size > sizeof(g_littlefs_lookahead_buffer)) {
        lookahead_size = sizeof(g_littlefs_lookahead_buffer);
    }
    /* 确保lookahead_size是8的倍数 */
    lookahead_size = (lookahead_size / 8) * 8;
    if (lookahead_size < LITTLEFS_MIN_LOOKAHEAD_SIZE) {
        lookahead_size = LITTLEFS_MIN_LOOKAHEAD_SIZE;
    }
    dev->config.lookahead_size = lookahead_size;
    
    /* 配置静态缓冲区（使用独立的全局缓冲区，确保4字节对齐） */
    dev->config.read_buffer = g_littlefs_read_buffer;
    dev->config.prog_buffer = g_littlefs_prog_buffer;
    dev->config.lookahead_buffer = g_littlefs_lookahead_buffer;
    
    /* 配置限制参数 */
    dev->config.name_max = config.name_max;
    dev->config.file_max = config.file_max;
    dev->config.attr_max = config.attr_max;
    
    /* 配置context（指向实例编号，用于回调函数识别实例） */
    dev->config.context = (void*)(uintptr_t)instance;
    
    /* 保存回调函数 */
    dev->log_callback = config.log_callback;
    dev->lock_callback = config.lock_callback;
    dev->unlock_callback = config.unlock_callback;
    dev->lock_context = config.lock_context;
    
    return LITTLEFS_OK;
}

/* ==================== 公共API实现 ==================== */

/**
 * @brief LittleFS初始化（使用默认配置，向后兼容）
 */
LittleFS_Status_t LittleFS_Init(void)
{
    return LittleFS_InitWithConfig(LITTLEFS_DEFAULT_INSTANCE, NULL);
}

/**
 * @brief LittleFS初始化（使用自定义配置）
 */
LittleFS_Status_t LittleFS_InitWithConfig(LittleFS_Instance_t instance, const LittleFS_Config_t *config)
{
    /* 参数校验 */
    if (instance >= LITTLEFS_INSTANCE_MAX) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    littlefs_dev_t *dev = littlefs_get_device(instance);
    if (dev == NULL) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已初始化 */
    if (dev->state != LITTLEFS_STATE_UNINITIALIZED) {
        return LITTLEFS_ERROR_ALREADY_INIT;
    }
    
    /* 手动初始化设备结构体 */
    memset(dev, 0, sizeof(littlefs_dev_t));
    dev->state = LITTLEFS_STATE_UNINITIALIZED;
    dev->cached_dev_info = NULL;  /* 初始化缓存字段 */
    dev->cached_max_addr = 0;
    
    /* 检查W25Q是否已初始化 */
    if (!W25Q_IsInitialized()) {
        return LITTLEFS_ERROR_NOT_INIT;
    }
    
    /* 初始化lfs结构体（清零） */
    memset(&dev->lfs, 0, sizeof(lfs_t));
    
    /* 配置littlefs配置结构体 */
    LittleFS_Status_t status = LittleFS_ConfigInitInstance(instance, config);
    if (status != LITTLEFS_OK) {
        return status;
    }
    
    /* 标记为已初始化 */
    dev->state = LITTLEFS_STATE_INITIALIZED;
    
    if (dev->log_callback != NULL) {
        littlefs_log_internal(instance, 1, "LittleFS instance %d initialized", instance);
    }
    
    return LITTLEFS_OK;
}

/**
 * @brief LittleFS反初始化（默认实例，向后兼容）
 */
LittleFS_Status_t LittleFS_Deinit(void)
{
    return LittleFS_DeinitInstance(LITTLEFS_DEFAULT_INSTANCE);
}

/**
 * @brief LittleFS反初始化（指定实例）
 */
LittleFS_Status_t LittleFS_DeinitInstance(LittleFS_Instance_t instance)
{
    /* 参数校验 */
    if (instance >= LITTLEFS_INSTANCE_MAX) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    littlefs_dev_t *dev = littlefs_get_device(instance);
    if (dev == NULL) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    /* 如果已挂载，先卸载 */
    if (dev->state == LITTLEFS_STATE_MOUNTED) {
        LittleFS_Status_t unmount_status = LittleFS_UnmountInstance(instance);
        if (unmount_status != LITTLEFS_OK) {
            /* 卸载失败，记录错误但继续反初始化（强制清理） */
            if (dev->log_callback != NULL) {
                littlefs_log_internal(instance, 2, "LittleFS instance %d unmount failed during deinit: %d", 
                                      instance, unmount_status);
            }
            /* 注意：即使卸载失败，也继续反初始化，因为这是清理操作 */
            /* 但需要确保lfs结构体被正确清理 */
        }
    }
    
    /* 清除状态 */
    dev->state = LITTLEFS_STATE_UNINITIALIZED;
    memset(&dev->lfs, 0, sizeof(lfs_t));
    memset(&dev->config, 0, sizeof(struct lfs_config));
    memset(&dev->user_config, 0, sizeof(LittleFS_Config_t));
    dev->cached_dev_info = NULL;  /* 清除缓存 */
    dev->cached_max_addr = 0;
    
    if (dev->log_callback != NULL) {
        littlefs_log_internal(instance, 1, "LittleFS instance %d deinitialized", instance);
    }
    
    return LITTLEFS_OK;
}

/**
 * @brief 检查LittleFS是否已初始化（默认实例，向后兼容）
 */
uint8_t LittleFS_IsInitialized(void)
{
    return LittleFS_IsInitializedInstance(LITTLEFS_DEFAULT_INSTANCE);
}

/**
 * @brief 检查LittleFS是否已初始化（指定实例）
 */
uint8_t LittleFS_IsInitializedInstance(LittleFS_Instance_t instance)
{
    if (instance >= LITTLEFS_INSTANCE_MAX) {
        return 0;
    }
    
    littlefs_dev_t *dev = littlefs_get_device(instance);
    if (dev == NULL) {
        return 0;
    }
    
    return (dev->state != LITTLEFS_STATE_UNINITIALIZED) ? 1 : 0;
}

/**
 * @brief 挂载文件系统（默认实例，向后兼容）
 */
LittleFS_Status_t LittleFS_Mount(void)
{
    return LittleFS_MountInstance(LITTLEFS_DEFAULT_INSTANCE);
}

/**
 * @brief 挂载文件系统（指定实例）
 */
LittleFS_Status_t LittleFS_MountInstance(LittleFS_Instance_t instance)
{
    /* 参数校验 */
    if (instance >= LITTLEFS_INSTANCE_MAX) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    littlefs_dev_t *dev = littlefs_get_device(instance);
    if (dev == NULL) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已初始化 */
    if (dev->state == LITTLEFS_STATE_UNINITIALIZED) {
        return LITTLEFS_ERROR_NOT_INIT;
    }
    
    /* 如果已挂载，直接返回成功 */
    if (dev->state == LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_OK;
    }
    
    /* 挂载前：确保CS引脚正确配置和初始状态 */
    #ifdef CONFIG_MODULE_W25Q_ENABLED
    #if CONFIG_MODULE_W25Q_ENABLED
    SPI_Instance_t spi_instance = W25Q_SPI_INSTANCE;
    
    /* 确保CS引脚为高（释放状态） */
    SPI_NSS_High(spi_instance);
    
    /* 短暂延时确保CS稳定 */
    Delay_us(10);
    #endif
    #endif
    
    /* 挂载文件系统 */
    int lfs_err = lfs_mount(&dev->lfs, &dev->config);
    if (lfs_err == LFS_ERR_OK) {
        dev->state = LITTLEFS_STATE_MOUNTED;
        if (dev->log_callback != NULL) {
            littlefs_log_internal(instance, 1, "LittleFS instance %d mounted", instance);
        }
        return LITTLEFS_OK;
    }
    
    if (dev->log_callback != NULL) {
        littlefs_log_internal(instance, 2, "LittleFS instance %d mount failed: %d", instance, lfs_err);
    }
    
    return LittleFS_ConvertError(lfs_err);
}

/**
 * @brief 卸载文件系统（默认实例，向后兼容）
 */
LittleFS_Status_t LittleFS_Unmount(void)
{
    return LittleFS_UnmountInstance(LITTLEFS_DEFAULT_INSTANCE);
}

/**
 * @brief 卸载文件系统（指定实例）
 */
LittleFS_Status_t LittleFS_UnmountInstance(LittleFS_Instance_t instance)
{
    /* 参数校验 */
    if (instance >= LITTLEFS_INSTANCE_MAX) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    littlefs_dev_t *dev = littlefs_get_device(instance);
    if (dev == NULL) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已挂载 */
    if (dev->state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_OK;  /* 未挂载，直接返回成功 */
    }
    
    /* 卸载文件系统 */
    int lfs_err = lfs_unmount(&dev->lfs);
    if (lfs_err == LFS_ERR_OK) {
        dev->state = LITTLEFS_STATE_INITIALIZED;
        if (dev->log_callback != NULL) {
            littlefs_log_internal(instance, 1, "LittleFS instance %d unmounted", instance);
        }
        return LITTLEFS_OK;
    }
    
    if (dev->log_callback != NULL) {
        littlefs_log_internal(instance, 2, "LittleFS instance %d unmount failed: %d", instance, lfs_err);
    }
    
    return LittleFS_ConvertError(lfs_err);
}

/**
 * @brief 格式化文件系统（默认实例，向后兼容）
 */
LittleFS_Status_t LittleFS_Format(void)
{
    return LittleFS_FormatInstance(LITTLEFS_DEFAULT_INSTANCE);
}

/**
 * @brief 格式化文件系统（指定实例）
 */
LittleFS_Status_t LittleFS_FormatInstance(LittleFS_Instance_t instance)
{
    /* 参数校验 */
    if (instance >= LITTLEFS_INSTANCE_MAX) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    littlefs_dev_t *dev = littlefs_get_device(instance);
    if (dev == NULL) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已初始化 */
    if (dev->state == LITTLEFS_STATE_UNINITIALIZED) {
        return LITTLEFS_ERROR_NOT_INIT;
    }
    
    /* 如果已挂载，先卸载 */
    if (dev->state == LITTLEFS_STATE_MOUNTED) {
        LittleFS_Status_t unmount_status = LittleFS_UnmountInstance(instance);
        if (unmount_status != LITTLEFS_OK) {
            /* 卸载失败，无法格式化 */
            if (dev->log_callback != NULL) {
                littlefs_log_internal(instance, 2, "LittleFS instance %d unmount failed before format: %d", 
                                      instance, unmount_status);
            }
            return unmount_status;
        }
    }
    
    /* 格式化文件系统 */
    int lfs_err = lfs_format(&dev->lfs, &dev->config);
    if (lfs_err == LFS_ERR_OK) {
        if (dev->log_callback != NULL) {
            littlefs_log_internal(instance, 1, "LittleFS instance %d formatted", instance);
        }
        return LITTLEFS_OK;
    }
    
    if (dev->log_callback != NULL) {
        littlefs_log_internal(instance, 2, "LittleFS instance %d format failed: %d", instance, lfs_err);
    }
    
    return LittleFS_ConvertError(lfs_err);
}

/**
 * @brief 获取文件系统信息（默认实例，向后兼容）
 */
LittleFS_Status_t LittleFS_GetInfo(uint64_t *total_bytes, uint64_t *free_bytes)
{
    return LittleFS_GetInfoInstance(LITTLEFS_DEFAULT_INSTANCE, total_bytes, free_bytes);
}

/**
 * @brief 获取文件系统信息（指定实例）
 */
LittleFS_Status_t LittleFS_GetInfoInstance(LittleFS_Instance_t instance, uint64_t *total_bytes, uint64_t *free_bytes)
{
    /* 参数校验 */
    if (total_bytes == NULL || free_bytes == NULL) {
        return LITTLEFS_ERROR_INVALID_PARAM;
    }
    
    if (instance >= LITTLEFS_INSTANCE_MAX) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    littlefs_dev_t *dev = littlefs_get_device(instance);
    if (dev == NULL) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已挂载 */
    if (dev->state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 计算总空间 */
    *total_bytes = (uint64_t)dev->config.block_count * dev->config.block_size;
    
    /* 计算空闲空间：使用lfs_fs_size获取已用空间 */
    lfs_ssize_t used_size = lfs_fs_size(&dev->lfs);
    if (used_size >= 0) {
        *free_bytes = *total_bytes - (uint64_t)used_size;
    } else {
        /* 如果无法获取，使用块数估算 */
        *free_bytes = *total_bytes;  /* 保守估计，假设全部空闲 */
    }
    
    return LITTLEFS_OK;
}

/**
 * @brief 文件系统健康检查（默认实例）
 */
LittleFS_Status_t LittleFS_HealthCheck(LittleFS_HealthStatus_t *health_status)
{
    return LittleFS_HealthCheckInstance(LITTLEFS_DEFAULT_INSTANCE, health_status);
}

/**
 * @brief 文件系统健康检查（指定实例）
 */
LittleFS_Status_t LittleFS_HealthCheckInstance(LittleFS_Instance_t instance, LittleFS_HealthStatus_t *health_status)
{
    /* 参数校验 */
    if (health_status == NULL) {
        return LITTLEFS_ERROR_INVALID_PARAM;
    }
    
    if (instance >= LITTLEFS_INSTANCE_MAX) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    littlefs_dev_t *dev = littlefs_get_device(instance);
    if (dev == NULL) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已挂载 */
    if (dev->state != LITTLEFS_STATE_MOUNTED) {
        *health_status = LITTLEFS_HEALTH_UNKNOWN;
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 尝试读取文件系统信息来检测损坏 */
    uint64_t total_bytes, free_bytes;
    LittleFS_Status_t status = LittleFS_GetInfoInstance(instance, &total_bytes, &free_bytes);
    if (status != LITTLEFS_OK) {
        *health_status = LITTLEFS_HEALTH_CORRUPT;
        return status;
    }
    
    /* 检查块设备是否正常（尝试读取第一个块） */
    uint8_t test_buffer[256];
    int read_result = littlefs_bd_read(&dev->config, 0, 0, test_buffer, sizeof(test_buffer));
    if (read_result != LFS_ERR_OK) {
        *health_status = LITTLEFS_HEALTH_BLOCK_DEVICE_ERROR;
        return LITTLEFS_ERROR_BLOCK_DEVICE_ERROR;
    }
    
    /* 如果所有检查都通过，认为健康 */
    *health_status = LITTLEFS_HEALTH_OK;
    return LITTLEFS_OK;
}

/**
 * @brief 设置日志回调函数（默认实例）
 */
LittleFS_Status_t LittleFS_SetLogCallback(LittleFS_LogCallback_t callback)
{
    return LittleFS_SetLogCallbackInstance(LITTLEFS_DEFAULT_INSTANCE, callback);
}

/**
 * @brief 设置日志回调函数（指定实例）
 */
LittleFS_Status_t LittleFS_SetLogCallbackInstance(LittleFS_Instance_t instance, LittleFS_LogCallback_t callback)
{
    /* 参数校验 */
    if (instance >= LITTLEFS_INSTANCE_MAX) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    littlefs_dev_t *dev = littlefs_get_device(instance);
    if (dev == NULL) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已初始化 */
    if (dev->state == LITTLEFS_STATE_UNINITIALIZED) {
        return LITTLEFS_ERROR_NOT_INIT;
    }
    
    /* 设置日志回调 */
    dev->log_callback = callback;
    dev->user_config.log_callback = callback;
    
    return LITTLEFS_OK;
}

/**
 * @brief 设置RTOS锁回调函数（默认实例）
 */
LittleFS_Status_t LittleFS_SetLockCallback(LittleFS_LockCallback_t lock_callback, 
                                           LittleFS_UnlockCallback_t unlock_callback, 
                                           void *context)
{
    return LittleFS_SetLockCallbackInstance(LITTLEFS_DEFAULT_INSTANCE, lock_callback, unlock_callback, context);
}

/**
 * @brief 设置RTOS锁回调函数（指定实例）
 */
LittleFS_Status_t LittleFS_SetLockCallbackInstance(LittleFS_Instance_t instance,
                                                    LittleFS_LockCallback_t lock_callback, 
                                                    LittleFS_UnlockCallback_t unlock_callback, 
                                                    void *context)
{
    /* 参数校验 */
    if (instance >= LITTLEFS_INSTANCE_MAX) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    littlefs_dev_t *dev = littlefs_get_device(instance);
    if (dev == NULL) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已初始化 */
    if (dev->state == LITTLEFS_STATE_UNINITIALIZED) {
        return LITTLEFS_ERROR_NOT_INIT;
    }
    
    /* 如果已挂载，不允许修改锁回调 */
    if (dev->state == LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_INVALID_PARAM;
    }
    
    /* 设置锁回调 */
    dev->lock_callback = lock_callback;
    dev->unlock_callback = unlock_callback;
    dev->lock_context = context;
    dev->user_config.lock_callback = lock_callback;
    dev->user_config.unlock_callback = unlock_callback;
    dev->user_config.lock_context = context;
    
    /* 更新littlefs配置中的锁回调 */
    #ifdef LFS_THREADSAFE
    if (lock_callback != NULL && unlock_callback != NULL) {
        dev->config.lock = littlefs_bd_lock;
        dev->config.unlock = littlefs_bd_unlock;
    } else {
        dev->config.lock = NULL;
        dev->config.unlock = NULL;
    }
    #endif
    
    return LITTLEFS_OK;
}

/* ==================== 文件操作 ==================== */

/**
 * @brief 打开文件（使用默认实例）
 */
LittleFS_Status_t LittleFS_FileOpen(lfs_file_t *file, const char *path, int flags)
{
    /* 参数校验 */
    if (file == NULL || path == NULL) {
        return LITTLEFS_ERROR_INVALID_PARAM;
    }
    
    littlefs_dev_t *dev = littlefs_get_device(LITTLEFS_DEFAULT_INSTANCE);
    if (dev == NULL) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已挂载 */
    if (dev->state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 配置文件缓冲区：使用read_buffer作为文件缓冲区 */
    struct lfs_file_config file_cfg = {
        .buffer = g_littlefs_read_buffer,
        .attrs = NULL,
        .attr_count = 0
    };
    
    /* 打开文件（使用文件配置） */
    int lfs_err = lfs_file_opencfg(&dev->lfs, file, path, flags, &file_cfg);
    
    return LittleFS_ConvertError(lfs_err);
}

/**
 * @brief 关闭文件（使用默认实例）
 */
LittleFS_Status_t LittleFS_FileClose(lfs_file_t *file)
{
    /* 参数校验 */
    if (file == NULL) {
        return LITTLEFS_ERROR_INVALID_PARAM;
    }
    
    littlefs_dev_t *dev = littlefs_get_device(LITTLEFS_DEFAULT_INSTANCE);
    if (dev == NULL) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已挂载 */
    if (dev->state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 关闭文件 */
    int lfs_err = lfs_file_close(&dev->lfs, file);
    
    return LittleFS_ConvertError(lfs_err);
}

/**
 * @brief 读取文件（使用默认实例）
 */
LittleFS_Status_t LittleFS_FileRead(lfs_file_t *file, void *buffer, uint32_t size, uint32_t *bytes_read)
{
    /* 参数校验 */
    if (file == NULL || buffer == NULL) {
        return LITTLEFS_ERROR_INVALID_PARAM;
    }
    
    littlefs_dev_t *dev = littlefs_get_device(LITTLEFS_DEFAULT_INSTANCE);
    if (dev == NULL) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已挂载 */
    if (dev->state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 读取文件 */
    lfs_ssize_t result = lfs_file_read(&dev->lfs, file, buffer, size);
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
    
    littlefs_dev_t *dev = littlefs_get_device(LITTLEFS_DEFAULT_INSTANCE);
    if (dev == NULL) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已挂载 */
    if (dev->state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 写入文件 */
    lfs_ssize_t result = lfs_file_write(&dev->lfs, file, buffer, size);
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
    
    littlefs_dev_t *dev = littlefs_get_device(LITTLEFS_DEFAULT_INSTANCE);
    if (dev == NULL) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已挂载 */
    if (dev->state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 文件定位 */
    lfs_soff_t result = lfs_file_seek(&dev->lfs, file, offset, whence);
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
    
    littlefs_dev_t *dev = littlefs_get_device(LITTLEFS_DEFAULT_INSTANCE);
    if (dev == NULL) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已挂载 */
    if (dev->state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 获取文件大小 */
    lfs_soff_t result = lfs_file_size(&dev->lfs, file);
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
    
    littlefs_dev_t *dev = littlefs_get_device(LITTLEFS_DEFAULT_INSTANCE);
    if (dev == NULL) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已挂载 */
    if (dev->state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 截断文件 */
    int lfs_err = lfs_file_truncate(&dev->lfs, file, size);
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
    
    littlefs_dev_t *dev = littlefs_get_device(LITTLEFS_DEFAULT_INSTANCE);
    if (dev == NULL) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已挂载 */
    if (dev->state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 同步文件 */
    int lfs_err = lfs_file_sync(&dev->lfs, file);
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
    
    littlefs_dev_t *dev = littlefs_get_device(LITTLEFS_DEFAULT_INSTANCE);
    if (dev == NULL) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已挂载 */
    if (dev->state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 删除文件 */
    int lfs_err = lfs_remove(&dev->lfs, path);
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
    
    littlefs_dev_t *dev = littlefs_get_device(LITTLEFS_DEFAULT_INSTANCE);
    if (dev == NULL) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已挂载 */
    if (dev->state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 重命名文件 */
    int lfs_err = lfs_rename(&dev->lfs, old_path, new_path);
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
    
    littlefs_dev_t *dev = littlefs_get_device(LITTLEFS_DEFAULT_INSTANCE);
    if (dev == NULL) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已挂载 */
    if (dev->state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 打开目录 */
    int lfs_err = lfs_dir_open(&dev->lfs, dir, path);
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
    
    littlefs_dev_t *dev = littlefs_get_device(LITTLEFS_DEFAULT_INSTANCE);
    if (dev == NULL) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已挂载 */
    if (dev->state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 关闭目录 */
    int lfs_err = lfs_dir_close(&dev->lfs, dir);
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
    
    littlefs_dev_t *dev = littlefs_get_device(LITTLEFS_DEFAULT_INSTANCE);
    if (dev == NULL) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已挂载 */
    if (dev->state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 读取目录项 */
    int lfs_result = lfs_dir_read(&dev->lfs, dir, info);
    
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
    
    littlefs_dev_t *dev = littlefs_get_device(LITTLEFS_DEFAULT_INSTANCE);
    if (dev == NULL) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已挂载 */
    if (dev->state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 创建目录 */
    int lfs_err = lfs_mkdir(&dev->lfs, path);
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
    
    littlefs_dev_t *dev = littlefs_get_device(LITTLEFS_DEFAULT_INSTANCE);
    if (dev == NULL) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已挂载 */
    if (dev->state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 删除目录 */
    int lfs_err = lfs_remove(&dev->lfs, path);
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
    
    littlefs_dev_t *dev = littlefs_get_device(LITTLEFS_DEFAULT_INSTANCE);
    if (dev == NULL) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已挂载 */
    if (dev->state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 设置文件属性 */
    int lfs_err = lfs_setattr(&dev->lfs, path, type, buffer, size);
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
    
    littlefs_dev_t *dev = littlefs_get_device(LITTLEFS_DEFAULT_INSTANCE);
    if (dev == NULL) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已挂载 */
    if (dev->state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 获取文件属性 */
    lfs_ssize_t result = lfs_getattr(&dev->lfs, path, type, buffer, size);
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
    
    littlefs_dev_t *dev = littlefs_get_device(LITTLEFS_DEFAULT_INSTANCE);
    if (dev == NULL) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已挂载 */
    if (dev->state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 移除文件属性 */
    int lfs_err = lfs_removeattr(&dev->lfs, path, type);
    return LittleFS_ConvertError(lfs_err);
}

/* ==================== 调试支持 ==================== */

/**
 * @brief 遍历文件系统（递归辅助函数）
 * @param[in] dev 设备结构体指针
 * @param[in] path 当前路径
 * @param[in] callback 遍历回调函数
 * @param[in] user_data 用户数据指针
 * @return LittleFS_Status_t 错误码
 */
static LittleFS_Status_t littlefs_traverse_recursive(littlefs_dev_t *dev, const char *path, 
                                                      LittleFS_TraverseCallback_t callback, 
                                                      void *user_data)
{
    lfs_dir_t dir;
    struct lfs_info info;
    char full_path[256];  /* 最大路径长度 */
    
    /* 打开目录 */
    int lfs_err = lfs_dir_open(&dev->lfs, &dir, path);
    if (lfs_err != LFS_ERR_OK) {
        return LittleFS_ConvertError(lfs_err);
    }
    
    /* 遍历目录项 */
    while (1) {
        int result = lfs_dir_read(&dev->lfs, &dir, &info);
        if (result < 0) {
            /* 错误 */
            lfs_dir_close(&dev->lfs, &dir);
            return LittleFS_ConvertError(result);
        } else if (result == 0) {
            /* 目录读取完毕 */
            break;
        }
        
        /* 构建完整路径 */
        if (strcmp(path, "/") == 0) {
            snprintf(full_path, sizeof(full_path), "/%s", info.name);
        } else {
            snprintf(full_path, sizeof(full_path), "%s/%s", path, info.name);
        }
        
        /* 调用回调函数 */
        if (callback != NULL) {
            int callback_result = callback(full_path, &info, user_data);
            if (callback_result != 0) {
                /* 回调函数要求停止遍历 */
                lfs_dir_close(&dev->lfs, &dir);
                return LITTLEFS_OK;
            }
        }
        
        /* 如果是目录，递归遍历 */
        if (info.type == LFS_TYPE_DIR) {
            LittleFS_Status_t status = littlefs_traverse_recursive(dev, full_path, callback, user_data);
            if (status != LITTLEFS_OK && status != LITTLEFS_ERROR_NOENT) {
                lfs_dir_close(&dev->lfs, &dir);
                return status;
            }
        }
    }
    
    /* 关闭目录 */
    lfs_dir_close(&dev->lfs, &dir);
    return LITTLEFS_OK;
}

/**
 * @brief 遍历文件系统（默认实例）
 */
LittleFS_Status_t LittleFS_Traverse(const char *root_path, LittleFS_TraverseCallback_t callback, void *user_data)
{
    return LittleFS_TraverseInstance(LITTLEFS_DEFAULT_INSTANCE, root_path, callback, user_data);
}

/**
 * @brief 遍历文件系统（指定实例）
 */
LittleFS_Status_t LittleFS_TraverseInstance(LittleFS_Instance_t instance, const char *root_path, 
                                            LittleFS_TraverseCallback_t callback, void *user_data)
{
    /* 参数校验 */
    if (root_path == NULL) {
        return LITTLEFS_ERROR_INVALID_PARAM;
    }
    
    if (instance >= LITTLEFS_INSTANCE_MAX) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    littlefs_dev_t *dev = littlefs_get_device(instance);
    if (dev == NULL) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已挂载 */
    if (dev->state != LITTLEFS_STATE_MOUNTED) {
        return LITTLEFS_ERROR_NOT_MOUNTED;
    }
    
    /* 递归遍历 */
    return littlefs_traverse_recursive(dev, root_path, callback, user_data);
}

/**
 * @brief 获取底层LittleFS对象指针（用于调试和原始API访问）
 * @param[in] instance 实例编号
 * @return lfs_t* LittleFS对象指针，NULL=无效实例或未挂载
 */
lfs_t* LittleFS_GetLFS(LittleFS_Instance_t instance)
{
    littlefs_dev_t *dev = littlefs_get_device(instance);
    if (dev == NULL) {
        return NULL;
    }
    
    /* 检查是否已挂载 */
    if (dev->state != LITTLEFS_STATE_MOUNTED) {
        return NULL;
    }
    
    return &dev->lfs;
}

/**
 * @brief 获取文件缓存缓冲区指针（用于原始API访问）
 * @param[in] instance 实例编号
 * @return uint8_t* 缓存缓冲区指针，NULL=无效实例或未初始化
 */
uint8_t* LittleFS_GetCacheBuffer(LittleFS_Instance_t instance)
{
    littlefs_dev_t *dev = littlefs_get_device(instance);
    if (dev == NULL) {
        return NULL;
    }
    
    /* 检查是否已初始化 */
    if (dev->state == LITTLEFS_STATE_UNINITIALIZED) {
        return NULL;
    }
    
    return g_littlefs_read_buffer;
}

/**
 * @brief 获取所有LittleFS缓冲区地址（用于诊断对齐问题）
 * @param[in] instance 实例编号
 * @param[out] read_buf_addr read_buffer地址（可为NULL）
 * @param[out] prog_buf_addr prog_buffer地址（可为NULL）
 * @param[out] lookahead_buf_addr lookahead_buffer地址（可为NULL）
 * @return LittleFS_Status_t 错误码
 */
LittleFS_Status_t LittleFS_GetBufferAddresses(LittleFS_Instance_t instance,
                                               uint32_t *read_buf_addr,
                                               uint32_t *prog_buf_addr,
                                               uint32_t *lookahead_buf_addr)
{
    littlefs_dev_t *dev = littlefs_get_device(instance);
    if (dev == NULL) {
        return LITTLEFS_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已初始化 */
    if (dev->state == LITTLEFS_STATE_UNINITIALIZED) {
        return LITTLEFS_ERROR_NOT_INIT;
    }
    
    if (read_buf_addr != NULL) {
        *read_buf_addr = (uint32_t)g_littlefs_read_buffer;
    }
    if (prog_buf_addr != NULL) {
        *prog_buf_addr = (uint32_t)g_littlefs_prog_buffer;
    }
    if (lookahead_buf_addr != NULL) {
        *lookahead_buf_addr = (uint32_t)g_littlefs_lookahead_buffer;
    }
    
    return LITTLEFS_OK;
}

#endif /* CONFIG_MODULE_W25Q_ENABLED */
#endif /* CONFIG_MODULE_W25Q_ENABLED */
#endif /* CONFIG_MODULE_LITTLEFS_ENABLED */
#endif /* CONFIG_MODULE_LITTLEFS_ENABLED */

