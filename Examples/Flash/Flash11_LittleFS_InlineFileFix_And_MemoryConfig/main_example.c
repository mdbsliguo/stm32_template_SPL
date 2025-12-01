/**
 * @file main_example.c
 * @brief Flash11 - LittleFS文件系统综合测试案例
 * @details 演示LittleFS文件系统在W25Q SPI Flash上的完整功能测试
 * 
 * 硬件连接：
 * - W25Q SPI Flash模块连接到SPI2
 *   - CS：PA11
 *   - SCK：PB13（SPI2_SCK）
 *   - MISO：PB14（SPI2_MISO）
 *   - MOSI：PB15（SPI2_MOSI）
 *   - VCC：3.3V
 *   - GND：GND
 * - OLED显示屏（用于显示关键信息）
 *   - SCL：PB8
 *   - SDA：PB9
 * - UART1（用于详细日志输出）
 *   - TX：PA9
 *   - RX：PA10
 * - LED1：PA1（系统状态指示）
 * 
 * 功能演示：
 * 1. 系统初始化
 * 2. UART、Debug、Log初始化
 * 3. LED初始化
 * 4. 软件I2C初始化
 * 5. OLED初始化
 * 6. SPI初始化
 * 7. W25Q初始化与设备识别
 * 8. LittleFS文件系统初始化、挂载
 * 9. 文件操作测试（创建、写入、读取、验证）
 * 10. 目录操作测试（创建、列表）
 * 11. 主循环（LED闪烁，OLED显示状态）
 */

#include "stm32f10x.h"
#include "system_init.h"
#include "led.h"
#include "delay.h"
#include "oled_ssd1306.h"
#include "i2c_sw.h"
#include "uart.h"
#include "debug.h"
#include "log.h"
#include "error_handler.h"
#include "spi_hw.h"
#include "w25q_spi.h"
#include "gpio.h"
#include "config.h"
#include "board.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* 自定义LittleFS断言实现（用于定位NOSPC错误） */
/* 注意：必须在包含littlefs_wrapper.h之前定义 */
#ifndef LFS_NO_ASSERT
#undef LFS_ASSERT
#define LFS_ASSERT(test) \
    do { \
        if (!(test)) { \
            LOG_ERROR("LFS_ASSERT", "断言失败: %s:%d - %s", __FILE__, __LINE__, #test); \
            while(1) { \
                /* 无限循环，等待调试器连接 */ \
                Delay_ms(100); \
            } \
        } \
    } while(0)
#endif

#include "littlefs_wrapper.h"

/* ==================== 测试函数 ==================== */

/**
 * @brief 测试1：绕开littleFS，直接验证W25Q64驱动
 * @note 参考文档建议：先验证SPI驱动是否正常
 */
static void test_w25q_direct(void)
{
    uint8_t wbuf[256];
    uint8_t rbuf[256];
    W25Q_Status_t w25q_status;
    int i;
    int error_count = 0;
    
    LOG_INFO("TEST", "=== 测试1：W25Q直接驱动测试 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "W25Q Test");
    
    /* 初始化测试数据 */
    for (i = 0; i < 256; i++) {
        wbuf[i] = 0x5A;
    }
    memset(rbuf, 0, 256);
    
    /* 擦除扇区0 */
    LOG_INFO("TEST", "擦除扇区0...");
    OLED_ShowString(2, 1, "Erasing...");
    w25q_status = W25Q_EraseSector(0);
    if (w25q_status != W25Q_OK) {
        LOG_ERROR("TEST", "擦除失败: %d", w25q_status);
        OLED_ShowString(3, 1, "Erase Failed!");
        return;
    }
    Delay_ms(100);  /* 额外等待，确保擦除完成 */
    LOG_INFO("TEST", "擦除完成");
    
    /* 写入数据 */
    LOG_INFO("TEST", "写入256字节数据...");
    OLED_ShowString(2, 1, "Writing...");
    w25q_status = W25Q_Write(0, wbuf, 256);
    if (w25q_status != W25Q_OK) {
        LOG_ERROR("TEST", "写入失败: %d", w25q_status);
        OLED_ShowString(3, 1, "Write Failed!");
        return;
    }
    Delay_ms(10);  /* 额外等待，确保写入完成 */
    LOG_INFO("TEST", "写入完成");
    
    /* 读取数据 */
    LOG_INFO("TEST", "读取256字节数据...");
    OLED_ShowString(2, 1, "Reading...");
    w25q_status = W25Q_Read(0, rbuf, 256);
    if (w25q_status != W25Q_OK) {
        LOG_ERROR("TEST", "读取失败: %d", w25q_status);
        OLED_ShowString(3, 1, "Read Failed!");
        return;
    }
    LOG_INFO("TEST", "读取完成");
    
    /* 对比数据 */
    LOG_INFO("TEST", "对比数据...");
    for (i = 0; i < 256; i++) {
        if (wbuf[i] != rbuf[i]) {
            error_count++;
            if (error_count <= 10) {  /* 只打印前10个错误 */
                LOG_ERROR("TEST", "位置 %d: 写入=0x%02X, 读取=0x%02X", i, wbuf[i], rbuf[i]);
            }
        }
    }
    
    if (error_count == 0) {
        LOG_INFO("TEST", "? SPI驱动正常，数据完全一致");
        OLED_ShowString(3, 1, "Driver OK");
        OLED_ShowString(4, 1, "Data Match");
    } else {
        LOG_ERROR("TEST", "? SPI驱动有BUG！数据不一致，共 %d 个错误", error_count);
        OLED_ShowString(3, 1, "Driver Error!");
        OLED_ShowString(4, 1, "Data Mismatch");
        /* 驱动不修复，littleFS永远报错 */
        while(1) { Delay_ms(1000); }
    }
    Delay_ms(2000);
}

/**
 * @brief 测试2：验证littleFS最小系统
 * @note 参考文档建议：驱动正常后，验证littleFS最小系统
 */
static void test_littlefs_minimal(void)
{
    LittleFS_Status_t littlefs_status;
    
    LOG_INFO("TEST", "=== 测试2：LittleFS最小系统测试 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "LittleFS Test");
    
    /* 1. 强制格式化（清除所有残留数据） */
    LOG_INFO("TEST", "强制格式化...");
    OLED_ShowString(2, 1, "Formatting...");
    littlefs_status = LittleFS_Format();
    if (littlefs_status != LITTLEFS_OK) {
        LOG_ERROR("TEST", "格式化失败: %d", littlefs_status);
        OLED_ShowString(3, 1, "Format Failed!");
        return;
    }
    LOG_INFO("TEST", "格式化成功");
    
    /* 2. 挂载 */
    LOG_INFO("TEST", "挂载文件系统...");
    OLED_ShowString(2, 1, "Mounting...");
    littlefs_status = LittleFS_Mount();
    if (littlefs_status != LITTLEFS_OK) {
        LOG_ERROR("TEST", "挂载失败: %d", littlefs_status);
        OLED_ShowString(3, 1, "Mount Failed!");
        return;
    }
    LOG_INFO("TEST", "挂载成功");
    
    /* 3. 创建目录（仅此一步） */
    LOG_INFO("TEST", "创建目录 /testdir...");
    OLED_ShowString(2, 1, "Creating Dir...");
    littlefs_status = LittleFS_DirCreate("/testdir");
    if (littlefs_status == LITTLEFS_OK) {
        LOG_INFO("TEST", "目录创建成功");
        OLED_ShowString(3, 1, "Created OK");
    } else if (littlefs_status == LITTLEFS_ERROR_EXIST) {
        LOG_INFO("TEST", "目录已存在（正常）");
        OLED_ShowString(3, 1, "Dir Exists");
    } else {
        LOG_ERROR("TEST", "创建目录失败: %d", littlefs_status);
        OLED_ShowString(3, 1, "Create Failed!");
        LittleFS_Unmount();
        return;
    }
    
    /* 4. 立即卸载确保落盘 */
    LOG_INFO("TEST", "卸载文件系统（确保数据落盘）...");
    OLED_ShowString(2, 1, "Unmounting...");
    littlefs_status = LittleFS_Unmount();
    if (littlefs_status != LITTLEFS_OK) {
        LOG_ERROR("TEST", "卸载失败: %d", littlefs_status);
        OLED_ShowString(3, 1, "Unmount Failed!");
        return;
    }
    LOG_INFO("TEST", "卸载成功");
    Delay_ms(500);
    
    /* 5. 重新挂载验证 */
    LOG_INFO("TEST", "重新挂载验证...");
    OLED_ShowString(2, 1, "Remounting...");
    littlefs_status = LittleFS_Mount();
    if (littlefs_status == LITTLEFS_OK) {
        LOG_INFO("TEST", "? LittleFS最小系统正常");
        OLED_ShowString(3, 1, "Test Pass");
        OLED_ShowString(4, 1, "System OK");
    } else {
        LOG_ERROR("TEST", "? 重新挂载失败: %d", littlefs_status);
        OLED_ShowString(3, 1, "Test Failed");
        OLED_ShowString(4, 1, "Mount Failed");
    }
    Delay_ms(2000);
}

/**
 * @brief 测试3：检查缓冲区地址和重叠
 * @note 参考文档建议：检查缓冲区是否4字节对齐，地址是否重叠
 */
static void test_buffer_addresses(void)
{
    LOG_INFO("TEST", "=== 测试3：缓冲区地址检查 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "Buffer Check");
    
    /* ? 关键：检查LittleFS缓冲区的实际地址和对齐情况 */
    uint32_t read_addr = 0, prog_addr = 0, lookahead_addr = 0;
    LittleFS_Status_t buf_status = LittleFS_GetBufferAddresses(LITTLEFS_INSTANCE_0, 
                                                                 &read_addr, 
                                                                 &prog_addr, 
                                                                 &lookahead_addr);
    
    if (buf_status == LITTLEFS_OK) {
        LOG_INFO("TEST", "缓冲区地址检查:");
        
        uint32_t read_mod4 = read_addr % 4;
        uint32_t prog_mod4 = prog_addr % 4;
        uint32_t lookahead_mod4 = lookahead_addr % 4;
        
        LOG_INFO("TEST", "  read_buffer: 0x%08X (mod4=%lu) %s", 
                 read_addr, (unsigned long)read_mod4, 
                 read_mod4 == 0 ? "OK" : "FAIL");
        LOG_INFO("TEST", "  prog_buffer: 0x%08X (mod4=%lu) %s", 
                 prog_addr, (unsigned long)prog_mod4, 
                 prog_mod4 == 0 ? "OK" : "FAIL");
        LOG_INFO("TEST", "  lookahead_buffer: 0x%08X (mod4=%lu) %s", 
                 lookahead_addr, (unsigned long)lookahead_mod4, 
                 lookahead_mod4 == 0 ? "OK" : "FAIL");
        
        /* 检查缓冲区间距 */
        uint32_t read_prog_diff = (prog_addr > read_addr) ? (prog_addr - read_addr) : (read_addr - prog_addr);
        LOG_INFO("TEST", "  read-prog间距: %lu 字节", (unsigned long)read_prog_diff);
        
        if (read_mod4 != 0 || prog_mod4 != 0 || lookahead_mod4 != 0) {
            LOG_ERROR("TEST", "缓冲区未4字节对齐！必须修复");
            OLED_ShowString(2, 1, "Buffer Align Fail");
        } else {
            LOG_INFO("TEST", "所有缓冲区4字节对齐 OK");
            OLED_ShowString(2, 1, "Buffer Align OK");
        }
    } else {
        LOG_INFO("TEST", "缓冲区大小检查:");
        LOG_INFO("TEST", "  read_buffer: %d 字节 (要求4字节对齐)", 256);
        LOG_INFO("TEST", "  prog_buffer: %d 字节 (要求4字节对齐)", 256);
        LOG_INFO("TEST", "  cache_size: %d 字节 (要求4字节对齐)", 256);
        LOG_INFO("TEST", "  lookahead_buffer: %d 字节 (要求4字节对齐)", 32);
        LOG_INFO("TEST", "注意：无法获取实际地址（文件系统未初始化）");
    }
    
    LOG_INFO("TEST", "缓冲区对齐要求:");
    LOG_INFO("TEST", "  1. 地址必须是4的倍数（4字节对齐）");
    LOG_INFO("TEST", "  2. 缓冲区之间不能重叠");
    LOG_INFO("TEST", "  3. 缓冲区地址相差至少256字节");
    
    OLED_ShowString(2, 1, "Check Done");
    OLED_ShowString(3, 1, "See UART log");
    Delay_ms(2000);
}

/* ? 错误码转字符串辅助函数 */
static const char *lfs_errstr(int e)
{
    switch (e) {
        case 0: return "OK";
        case LFS_ERR_IO: return "I/O";
        case LFS_ERR_CORRUPT: return "CORRUPT";
        case LFS_ERR_NOENT: return "NOENT";
        case LFS_ERR_EXIST: return "EXIST";
        case LFS_ERR_NOTDIR: return "NOTDIR";
        case LFS_ERR_ISDIR: return "ISDIR";
        case LFS_ERR_NOTEMPTY: return "NOTEMPTY";
        case LFS_ERR_BADF: return "BADF";
        case LFS_ERR_FBIG: return "FBIG";
        case LFS_ERR_INVAL: return "INVAL";
        case LFS_ERR_NOSPC: return "NOSPC";
        default: return "UNKNOWN";
    }
}

/**
 * @brief 测试4：原始API写入测试（绕过封装层）
 * @note 直接使用lfs_file_opencfg/lfs_file_write，排除封装层干扰
 */
static void test_raw_write(void)
{
    lfs_file_t raw_file;
    lfs_t* lfs = LittleFS_GetLFS(LITTLEFS_INSTANCE_0);
    uint8_t* cache_buf = LittleFS_GetCacheBuffer(LITTLEFS_INSTANCE_0);
    
    LOG_INFO("TEST", "=== 测试4：原始API写入测试 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "Raw Write Test");
    
    if (lfs == NULL) {
        LOG_ERROR("TEST", "获取lfs_t指针失败（文件系统未挂载）");
        OLED_ShowString(2, 1, "LFS NULL");
        return;
    }
    
    if (cache_buf == NULL) {
        LOG_ERROR("TEST", "获取缓存缓冲区失败");
        OLED_ShowString(2, 1, "Cache NULL");
        return;
    }
    
    /* 清零文件句柄 */
    memset(&raw_file, 0, sizeof(lfs_file_t));
    
    /* ? 诊断：打印memset后的CTZ状态 */
    LOG_INFO("TEST", "After memset - CTZ head: 0x%08X, size: %lu", 
             raw_file.ctz.head, (unsigned long)raw_file.ctz.size);
    
    /* 配置文件缓存：必须绑定缓存缓冲区 */
    const struct lfs_file_config file_cfg = {
        .buffer = cache_buf,  /* 关键：必须绑定缓存！ */
        .attrs = NULL,
        .attr_count = 0
    };
    
    /* 使用原始API打开文件（带缓存配置） */
    int err = lfs_file_opencfg(lfs, &raw_file, "/test.txt", LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC, &file_cfg);
    
    /* ? 诊断：打印打开后CTZ状态 */
    LOG_INFO("TEST", "After open - CTZ head: 0x%08X, size: %lu, flags: 0x%08X", 
             raw_file.ctz.head, (unsigned long)raw_file.ctz.size, raw_file.flags);
    
    if (err != 0) {
        LOG_ERROR("TEST", "Raw open write failed: %d", err);
        OLED_ShowString(2, 1, "Open Failed");
        return;
    }
    
    /* 使用原始API写入数据 */
    const char* test_data = "Hello";
    
    /* ? 诊断：打印写入前CTZ状态 */
    LOG_INFO("TEST", "Before write - CTZ head: 0x%08X, size: %lu", 
             raw_file.ctz.head, (unsigned long)raw_file.ctz.size);
    
    lfs_ssize_t written = lfs_file_write(lfs, &raw_file, test_data, 5);
    if (written < 0) {
        LOG_ERROR("TEST", "Raw write failed: %d", (int)written);
        OLED_ShowString(2, 1, "Write Failed");
        lfs_file_close(lfs, &raw_file);
        return;
    }
    
    LOG_INFO("TEST", "Raw write success: %d bytes", (int)written);
    OLED_ShowString(2, 1, "Write OK");
    
    /* ? 诊断：打印写入后（sync前）CTZ状态 */
    LOG_INFO("TEST", "After write (before sync) - CTZ head: 0x%08X, size: %lu", 
             raw_file.ctz.head, (unsigned long)raw_file.ctz.size);
    
    /* 强制同步 */
    err = lfs_file_sync(lfs, &raw_file);
    if (err != 0) {
        LOG_ERROR("TEST", "Raw sync failed: %d", err);
        OLED_ShowString(3, 1, "Sync Failed");
    } else {
        LOG_INFO("TEST", "Raw sync success");
        OLED_ShowString(3, 1, "Sync OK");
        
        /* ? 关键：等待Flash写入完成，确保CTZ元数据落盘 */
        Delay_ms(100);
        
        /* ? 诊断：打印sync后CTZ状态 */
        LOG_INFO("TEST", "After sync - CTZ head: 0x%08X, size: %lu", 
                 raw_file.ctz.head, (unsigned long)raw_file.ctz.size);
    }
    
    /* 关闭文件 */
    err = lfs_file_close(lfs, &raw_file);
    if (err != 0) {
        LOG_ERROR("TEST", "Raw close failed: %d", err);
        OLED_ShowString(4, 1, "Close Failed");
    } else {
        LOG_INFO("TEST", "Raw close success");
        OLED_ShowString(4, 1, "Close OK");
    }
    
    Delay_ms(2000);
}

/**
 * @brief 测试5：原始API读取测试（绕过封装层）
 * @note 直接使用lfs_file_opencfg/lfs_file_read，排除封装层干扰
 */
static void test_raw_read(void)
{
    LOG_INFO("TEST", "=== 测试5：原始API读取测试（稳健版） ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "Robust Read");
    
    int err;
    const char *path = "test.txt";  /* 使用目录里显示的名字（不要随意在前面加/） */
    
    /* 获取lfs_t指针 */
    lfs_t* lfs = LittleFS_GetLFS(LITTLEFS_INSTANCE_0);
    if (lfs == NULL) {
        LOG_ERROR("TEST", "获取lfs_t指针失败（文件系统未挂载）");
        OLED_ShowString(2, 1, "LFS NULL");
        return;
    }
    
    /* 1) 检查文件是否存在并打印信息 */
    struct lfs_info info;
    err = lfs_stat(lfs, path, &info);
    LOG_INFO("TEST", "lfs_stat('%s') -> %d (%s)", path, err, lfs_errstr(err));
    if (err != 0) {
        LOG_ERROR("TEST", "File not present or stat failed: %d (%s)", err, lfs_errstr(err));
        OLED_ShowString(2, 1, "Stat Failed");
        return;
    }
    LOG_INFO("TEST", "stat: name='%s' size=%lu type=%d", info.name, (unsigned long)info.size, info.type);
    
    /* 2) 准备文件句柄并清零（必须） */
    lfs_file_t file;
    memset(&file, 0, sizeof(file));
    
    /* 3) 准备 file_cfg 并绑定一个与 cfg->cache_size 等价的 buffer */
    /*    根据配置，cache_size = 256（LITTLEFS_DEFAULT_CACHE_SIZE） */
#ifdef __CC_ARM
    __align(4) static uint8_t file_cache_buf[256];
#else
    static uint8_t file_cache_buf[256] __attribute__((aligned(4)));
#endif
    
    const struct lfs_file_config file_cfg = {
        .buffer = file_cache_buf,
        .attrs = NULL,
        .attr_count = 0
    };
    
    /* 验证缓存缓冲区对齐 */
    {
        uint32_t buf_addr = (uint32_t)file_cache_buf;
        uint32_t buf_mod4 = buf_addr % 4;
        LOG_INFO("TEST", "File cache buffer addr: 0x%08X (mod4=%lu)", buf_addr, (unsigned long)buf_mod4);
        if (buf_mod4 != 0) {
            LOG_ERROR("TEST", "File cache buffer未4字节对齐！");
            OLED_ShowString(2, 1, "Cache Align Fail");
            return;
        }
    }
    
    /* 4) 使用 opencfg 打开（这样 lib 会使用你提供的缓存） */
    err = lfs_file_opencfg(lfs, &file, path, LFS_O_RDONLY, &file_cfg);
    LOG_INFO("TEST", "lfs_file_opencfg('%s') -> %d (%s)", path, err, lfs_errstr(err));
    if (err != 0) {
        LOG_ERROR("TEST", "Raw open read failed: %d (%s)", err, lfs_errstr(err));
        OLED_ShowString(2, 1, "Open Failed");
        return;
    }
    
    /* 打印文件信息 */
    LOG_INFO("TEST", "File opened: CTZ head: 0x%08X, size: %lu, flags: 0x%08X",
             file.ctz.head, (unsigned long)file.ctz.size, file.flags);
    if (file.flags & 0x100000) {  // LFS_F_INLINE flag
        LOG_INFO("TEST", "File is INLINE");
    } else {
        LOG_INFO("TEST", "File is REGULAR");
    }
    
    /* 5) 读取（确保用户 buffer 足够大且对齐） */
#ifdef __CC_ARM
    __align(4) static char readbuf[64];
#else
    static char readbuf[64] __attribute__((aligned(4)));
#endif
    memset(readbuf, 0, sizeof(readbuf));
    
    lfs_ssize_t r = lfs_file_read(lfs, &file, readbuf, sizeof(readbuf) - 1);
    if (r < 0) {
        LOG_ERROR("TEST", "lfs_file_read failed: %d (%s)", (int)r, lfs_errstr((int)r));
        OLED_ShowString(2, 1, "Read Failed");
    } else {
        readbuf[r] = '\0';
        LOG_INFO("TEST", "Read %d bytes: '%s'", (int)r, readbuf);
        OLED_ShowString(2, 1, "Read OK");
        OLED_ShowString(3, 1, readbuf);
    }
    
    /* 6) 关闭 */
    err = lfs_file_close(lfs, &file);
    LOG_INFO("TEST", "lfs_file_close -> %d (%s)", err, lfs_errstr(err));
    
    Delay_ms(2000);
}

/**
 * @brief 主函数
 */
int main(void)
{
    SPI_Status_t spi_status;
    W25Q_Status_t w25q_status;
    SoftI2C_Status_t i2c_status;
    UART_Status_t uart_status;
    OLED_Status_t oled_status;
    Log_Status_t log_status;
    int debug_status;
    log_config_t log_config;
    uint32_t loop_count = 0;
    
    /* ========== 步骤1：系统初始化 ========== */
    System_Init();
    
    /* ========== 步骤2：UART初始化 ========== */
    uart_status = UART_Init(UART_INSTANCE_1);
    if (uart_status != UART_OK)
    {
        while(1) { Delay_ms(1000); }
    }
    Delay_ms(100);
    
    /* ========== 步骤3：Debug模块初始化 ========== */
    debug_status = Debug_Init(DEBUG_MODE_UART, 115200);
    if (debug_status != 0)
    {
        while(1) { Delay_ms(1000); }
    }
    Delay_ms(100);
    
    /* ========== 步骤4：Log模块初始化 ========== */
    memset(&log_config, 0, sizeof(log_config));
    log_config.level = LOG_LEVEL_INFO;
    log_config.enable_timestamp = 0;
    log_config.enable_module = 1;
    log_config.enable_color = 0;
    log_status = Log_Init(&log_config);
    if (log_status != LOG_OK)
    {
        ErrorHandler_Handle(log_status, "LOG");
    }
    
    /* ========== 步骤5：输出初始化信息 ========== */
    LOG_INFO("MAIN", "=== Flash11 - LittleFS文件系统综合测试案例 ===");
    LOG_INFO("MAIN", "UART1 已初始化: PA9(TX), PA10(RX), 115200");
    LOG_INFO("MAIN", "Debug 模块已初始化: UART 模式");
    LOG_INFO("MAIN", "Log 模块已初始化");
    
    /* ========== 步骤6：LED初始化 ========== */
    if (LED_Init() != LED_OK)
    {
        LOG_ERROR("MAIN", "LED 初始化失败");
        while(1) { Delay_ms(1000); }
    }
    
    /* ========== 步骤7：软件I2C初始化（OLED需要） ========== */
    i2c_status = I2C_SW_Init(SOFT_I2C_INSTANCE_1);
    if (i2c_status != SOFT_I2C_OK)
    {
        LOG_ERROR("MAIN", "软件I2C 初始化失败: %d", i2c_status);
        ErrorHandler_Handle(i2c_status, "SOFT_I2C");
    }
    else
    {
        LOG_INFO("MAIN", "软件I2C 已初始化: PB8(SCL), PB9(SDA)");
    }
    
    /* ========== 步骤8：OLED初始化 ========== */
    oled_status = OLED_Init();
    if (oled_status != OLED_OK)
    {
        LOG_ERROR("MAIN", "OLED 初始化失败: %d", oled_status);
        ErrorHandler_Handle(oled_status, "OLED");
    }
    else
    {
        OLED_Clear();
        OLED_ShowString(1, 1, "Flash11");
        OLED_ShowString(2, 1, "Initializing...");
        LOG_INFO("MAIN", "OLED 已初始化");
    }
    
    Delay_ms(500);
    
    /* ========== 步骤9：SPI初始化 ========== */
    OLED_ShowString(3, 1, "Init SPI2...");
    
    /* 配置SPI2 NSS引脚为GPIO输出（软件NSS模式，SPI驱动不会自动配置） */
    /* 使用board.h中定义的宏，避免硬编码 */
    GPIO_EnableClock(SPI2_NSS_PORT);
    GPIO_Config(SPI2_NSS_PORT, SPI2_NSS_PIN, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHz);
    GPIO_WritePin(SPI2_NSS_PORT, SPI2_NSS_PIN, Bit_SET);  /* NSS默认拉高（不选中） */
    
    spi_status = SPI_HW_Init(SPI_INSTANCE_2);
    if (spi_status != SPI_OK)
    {
        char err_buf[17];
        snprintf(err_buf, sizeof(err_buf), "SPI Fail:%d", spi_status);
        OLED_ShowString(4, 1, err_buf);
        LOG_ERROR("MAIN", "SPI 初始化失败: %d", spi_status);
        ErrorHandler_Handle(spi_status, "SPI");
        Delay_ms(3000);
        while(1) { Delay_ms(1000); }
    }
    LOG_INFO("MAIN", "SPI2 已初始化: PB13(SCK), PB14(MISO), PB15(MOSI), PA11(CS)");
    
    /* ========== 步骤10：W25Q初始化 ========== */
    OLED_ShowString(3, 1, "Init W25Q...");
    w25q_status = W25Q_Init();
    if (w25q_status != W25Q_OK)
    {
        OLED_ShowString(4, 1, "W25Q Init Fail!");
        LOG_ERROR("MAIN", "W25Q 初始化失败: %d", w25q_status);
        ErrorHandler_Handle(w25q_status, "W25Q");
        while(1) { Delay_ms(1000); }
    }
    LOG_INFO("MAIN", "W25Q 初始化成功");
    
    /* 显示W25Q信息 */
    const w25q_dev_t* dev_info = W25Q_GetInfo();
    if (dev_info != NULL) {
        char buf[17];
        snprintf(buf, sizeof(buf), "Size:%d MB", dev_info->capacity_mb);
        OLED_ShowString(4, 1, buf);
        
        LOG_INFO("MAIN", "W25Q信息:");
        LOG_INFO("MAIN", "  容量: %d MB", dev_info->capacity_mb);
        LOG_INFO("MAIN", "  地址字节数: %d", dev_info->addr_bytes);
        LOG_INFO("MAIN", "  4字节模式: %s", dev_info->is_4byte_mode ? "是" : "否");
    }
    
    Delay_ms(1000);
    
    /* ========== 步骤10.5：诊断测试1 - W25Q直接驱动测试（在LittleFS之前） ========== */
    /* 测试1：绕开littleFS，直接验证W25Q64驱动 */
    test_w25q_direct();
    
    /* ========== 步骤11：LittleFS初始化 ========== */
    OLED_ShowString(3, 1, "Init LittleFS...");
    LittleFS_Status_t littlefs_status = LittleFS_Init();
    if (littlefs_status != LITTLEFS_OK)
    {
        OLED_ShowString(4, 1, "LittleFS Init Fail!");
        LOG_ERROR("MAIN", "LittleFS 初始化失败: %d", littlefs_status);
        ErrorHandler_Handle(littlefs_status, "LittleFS");
        while(1) { Delay_ms(1000); }
    }
    LOG_INFO("MAIN", "LittleFS 初始化成功");
    OLED_ShowString(4, 1, "LittleFS Ready");
    Delay_ms(500);
    
    /* ========== 步骤12：挂载前确保CS引脚配置正确 ========== */
    /* 确保SPI2 NSS引脚（CS）配置为推挽输出并拉高 */
    /* 使用GPIO模块API，避免直接操作寄存器（符合规范） */
    LOG_INFO("MAIN", "挂载前确保CS引脚配置正确...");
    
    /* 确保GPIO时钟已使能 */
    GPIO_EnableClock(SPI2_NSS_PORT);
    
    /* 确保CS引脚配置为推挽输出 */
    GPIO_Config(SPI2_NSS_PORT, SPI2_NSS_PIN, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHz);
    
    /* 确保CS引脚为高（释放状态） */
    GPIO_WritePin(SPI2_NSS_PORT, SPI2_NSS_PIN, Bit_SET);
    
    LOG_INFO("MAIN", "CS引脚已配置为推挽输出并拉高");
    Delay_ms(500);
    
    /* ========== 步骤13：挂载文件系统（根据配置决定是否强制格式化） ========== */
    LOG_INFO("MAIN", "开始挂载文件系统...");
    OLED_ShowString(3, 1, "Mounting FS...");
    LED_Toggle(LED_1);
    littlefs_status = LittleFS_Mount();
    LED_Toggle(LED_1);
    
#if CONFIG_LITTLEFS_FORCE_FORMAT
    /* 强制格式化模式：无论挂载是否成功，都格式化（解决NOSPC问题） */
    if (littlefs_status == LITTLEFS_OK)
    {
        LOG_INFO("MAIN", "挂载成功，但配置为强制格式化模式，准备卸载以便格式化...");
        OLED_ShowString(4, 1, "Unmounting...");
        Delay_ms(500);
        
        littlefs_status = LittleFS_Unmount();
        if (littlefs_status != LITTLEFS_OK)
        {
            LOG_ERROR("MAIN", "卸载失败: %d", littlefs_status);
            OLED_ShowString(4, 1, "Unmount Fail!");
            while(1) { Delay_ms(1000); }
        }
    }
    else
    {
        /* 挂载失败，可能是文件系统不存在或损坏（正常情况） */
        LOG_INFO("MAIN", "挂载失败: %d (文件系统不存在或损坏，需要格式化)", littlefs_status);
    }
    
    /* 强制格式化文件系统 */
    LOG_INFO("MAIN", "开始格式化文件系统（强制格式化模式）...");
    OLED_ShowString(3, 1, "Formatting...");
    OLED_ShowString(4, 1, "Please wait...");
    Delay_ms(1000);
    
    LED_Toggle(LED_1);
    littlefs_status = LittleFS_Format();
    LED_Toggle(LED_1);
    
    if (littlefs_status == LITTLEFS_OK)
    {
        LOG_INFO("MAIN", "格式化成功！");
        OLED_ShowString(4, 1, "Format OK");
        Delay_ms(500);
        
        /* 格式化后重新挂载 */
        LOG_INFO("MAIN", "格式化后重新挂载...");
        OLED_ShowString(3, 1, "Remounting...");
        LED_Toggle(LED_1);
        littlefs_status = LittleFS_Mount();
        LED_Toggle(LED_1);
        
        if (littlefs_status == LITTLEFS_OK)
        {
            LOG_INFO("MAIN", "重新挂载成功！");
            OLED_ShowString(4, 1, "Mount OK");
        }
        else
        {
            LOG_ERROR("MAIN", "重新挂载失败: %d", littlefs_status);
            OLED_ShowString(4, 1, "Mount Failed!");
            while(1) { Delay_ms(1000); }
        }
    }
    else
    {
        LOG_ERROR("MAIN", "格式化失败: %d", littlefs_status);
        OLED_ShowString(4, 1, "Format Failed!");
        while(1) { Delay_ms(1000); }
    }
#else
    /* 正常模式：只在挂载失败时格式化 */
    if (littlefs_status == LITTLEFS_OK)
    {
        LOG_INFO("MAIN", "挂载成功！");
        OLED_ShowString(4, 1, "Mount OK");
    }
    else
    {
        /* 挂载失败，可能是文件系统不存在或损坏（第一次使用是正常的） */
        LOG_INFO("MAIN", "挂载失败: %d (可能是文件系统不存在，需要格式化)", littlefs_status);
        OLED_ShowString(3, 1, "Mount Failed");
        OLED_ShowString(4, 1, "Start Format...");
        Delay_ms(1000);
        
        /* 尝试格式化 */
        LOG_INFO("MAIN", "开始格式化文件系统...");
        LED_Toggle(LED_1);
        littlefs_status = LittleFS_Format();
        LED_Toggle(LED_1);
        
        if (littlefs_status == LITTLEFS_OK)
        {
            LOG_INFO("MAIN", "格式化成功！");
            OLED_ShowString(4, 1, "Format OK");
            Delay_ms(500);
            
            /* 格式化后重新挂载 */
            LOG_INFO("MAIN", "格式化后重新挂载...");
            OLED_ShowString(3, 1, "Remounting...");
            LED_Toggle(LED_1);
            littlefs_status = LittleFS_Mount();
            LED_Toggle(LED_1);
            
            if (littlefs_status == LITTLEFS_OK)
            {
                LOG_INFO("MAIN", "重新挂载成功！");
                OLED_ShowString(4, 1, "Mount OK");
            }
            else
            {
                LOG_ERROR("MAIN", "重新挂载失败: %d", littlefs_status);
                OLED_ShowString(4, 1, "Mount Failed!");
                while(1) { Delay_ms(1000); }
            }
        }
        else
        {
            LOG_ERROR("MAIN", "格式化失败: %d", littlefs_status);
            OLED_ShowString(4, 1, "Format Failed!");
            while(1) { Delay_ms(1000); }
        }
    }
#endif /* CONFIG_LITTLEFS_FORCE_FORMAT */
    Delay_ms(2000);
    
    /* ========== 步骤13.5：诊断测试2和3（按参考文档顺序验证） ========== */
    /* 测试2：验证littleFS最小系统 */
    test_littlefs_minimal();
    
    /* 测试3：检查缓冲区地址和重叠 */
    test_buffer_addresses();
    
    /* ========== 步骤14：清理之前的测试文件（可选） ========== */
    LOG_INFO("MAIN", "清理之前的测试文件...");
    const char* test_files[] = {
        "/test.txt", "/test1.txt", "/test2.txt", "/test3.txt",
        "/old.txt", "/new.txt", "/delete.txt",
        "/attr.txt", "/atomic.txt", "/power.txt"
    };
    for (int i = 0; i < sizeof(test_files) / sizeof(test_files[0]); i++)
    {
        LittleFS_FileDelete(test_files[i]);  /* 忽略返回值，文件可能不存在 */
    }
    /* 删除测试目录 */
    LittleFS_DirDelete("/testdir");  /* 忽略返回值，目录可能不存在 */
    LOG_INFO("MAIN", "清理完成");
    Delay_ms(500);
    
    /* ========== 步骤15：显示文件系统信息 ========== */
    LOG_INFO("MAIN", "获取文件系统信息...");
    OLED_Clear();
    OLED_ShowString(1, 1, "FileSystem Info");
    
    uint64_t total_bytes = 0;
    uint64_t free_bytes = 0;
    littlefs_status = LittleFS_GetInfo(&total_bytes, &free_bytes);
    if (littlefs_status == LITTLEFS_OK)
    {
        LOG_INFO("MAIN", "文件系统信息:");
        LOG_INFO("MAIN", "  总空间: %llu 字节 (%.2f MB)", 
                 (unsigned long long)total_bytes, (double)total_bytes / (1024.0 * 1024.0));
        LOG_INFO("MAIN", "  空闲空间: %llu 字节 (%.2f MB)", 
                 (unsigned long long)free_bytes, (double)free_bytes / (1024.0 * 1024.0));
        
        char buf[17];
        snprintf(buf, sizeof(buf), "Total:%lluKB", (unsigned long long)total_bytes / 1024);
        OLED_ShowString(2, 1, buf);
        snprintf(buf, sizeof(buf), "Free:%lluKB", (unsigned long long)free_bytes / 1024);
        OLED_ShowString(3, 1, buf);
    }
    Delay_ms(2000);
    
    /* ========== 步骤16：原始API测试（绕过封装层） ========== */
    LOG_INFO("MAIN", "=== 开始原始API测试（绕过封装层） ===");
    
    /* 测试4：原始API写入测试 */
    test_raw_write();
    
    /* 测试5：原始API读取测试 */
    test_raw_read();
    
    /* 测试3：目录操作 */
    LOG_INFO("MAIN", "测试3：目录操作...");
    OLED_Clear();
    OLED_ShowString(1, 1, "Dir Test");
    
    /* 创建目录 */
    const char* test_dir = "/testdir";
    LOG_INFO("MAIN", "创建目录: %s", test_dir);
    OLED_ShowString(2, 1, "Creating Dir...");
    littlefs_status = LittleFS_DirCreate(test_dir);
    if (littlefs_status == LITTLEFS_OK)
    {
        LOG_INFO("MAIN", "目录创建成功");
        OLED_ShowString(3, 1, "Created OK");
    }
    else if (littlefs_status == LITTLEFS_ERROR_EXIST)
    {
        LOG_INFO("MAIN", "目录已存在（正常）");
        OLED_ShowString(3, 1, "Dir Exists");
    }
    else
    {
        LOG_ERROR("MAIN", "目录创建失败: %d", littlefs_status);
        OLED_ShowString(3, 1, "Create Failed");
    }
    Delay_ms(1000);
    
    /* 列出根目录内容 */
    LOG_INFO("MAIN", "测试4：列出根目录内容...");
    OLED_Clear();
    OLED_ShowString(1, 1, "List Dir");
    
    lfs_dir_t dir;
    struct lfs_info info;  /* 注意：使用struct lfs_info，不是LittleFS_Info_t */
    memset(&info, 0, sizeof(info));  /* 初始化info结构体 */
    littlefs_status = LittleFS_DirOpen(&dir, "/");
    if (littlefs_status == LITTLEFS_OK)
    {
        LOG_INFO("MAIN", "根目录内容:");
        int count = 0;
        while (1)
        {
            /* 每次读取前清空info结构体 */
            memset(&info, 0, sizeof(info));
            littlefs_status = LittleFS_DirRead(&dir, &info);
            if (littlefs_status == LITTLEFS_OK)
            {
                count++;
                const char* type_str = (info.type == LFS_TYPE_REG) ? "文件" : "目录";
                LOG_INFO("MAIN", "  [%d] %s: %s (大小: %lu 字节)", 
                         count, type_str, info.name, (unsigned long)info.size);
            }
            else if (littlefs_status == LITTLEFS_ERROR_NOENT)
            {
                /* 目录读取完毕，这是正常情况 */
                LOG_INFO("MAIN", "目录读取完毕（没有更多条目）");
                break;
            }
            else
            {
                /* 其他错误，记录并退出 */
                LOG_ERROR("MAIN", "读取目录项失败: %d (原始littlefs错误码可能未映射)", littlefs_status);
                break;
            }
        }
        LittleFS_DirClose(&dir);
        LOG_INFO("MAIN", "共 %d 个条目", count);
        
        char buf[17];
        snprintf(buf, sizeof(buf), "Total:%d items", count);
        OLED_ShowString(2, 1, buf);
    }
    else
    {
        LOG_ERROR("MAIN", "打开根目录失败: %d", littlefs_status);
        OLED_ShowString(2, 1, "Open Failed");
    }
    Delay_ms(2000);
    
    /* ========== 步骤15：显示初始化完成 ========== */
    OLED_Clear();
    OLED_ShowString(1, 1, "Flash11");
    OLED_ShowString(2, 1, "Init Done");
    OLED_ShowString(3, 1, "LittleFS Ready");
    LOG_INFO("MAIN", "=== 初始化完成，进入主循环 ===");
    Delay_ms(1000);
    
    /* ========== 步骤18：主循环 ========== */
    while (1)
    {
        loop_count++;
        LED_Toggle(LED_1);
        
        /* 每10次循环（约5秒）更新一次OLED显示 */
        if (loop_count % 10 == 0) {
            char buf[17];
            snprintf(buf, sizeof(buf), "Running:%lu", (unsigned long)loop_count);
            OLED_ShowString(4, 1, buf);
            LOG_INFO("MAIN", "主循环运行中... (循环 %lu)", (unsigned long)loop_count);
        }
        
        Delay_ms(500);
    }
}
