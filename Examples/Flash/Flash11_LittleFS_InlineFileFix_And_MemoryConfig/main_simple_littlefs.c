/**
 * @file main_simple_littlefs.c
 * @brief LittleFS 简单测试（参考官方Demo）
 * @note 使用正确的对齐和清零方式，确保inline文件正常工作
 */

#include "littlefs_wrapper.h"
#include "w25q_spi.h"
#include "uart.h"
#include "log.h"
#include "delay.h"
#include <string.h>
#include <stdio.h>

/*────────────────────────────────────────────
 * 1. LittleFS 缓冲区（必须 4 字节对齐）
 *────────────────────────────────────────────*/
#ifdef __CC_ARM
__align(4) static uint8_t lfs_read_buf[16];
__align(4) static uint8_t lfs_prog_buf[16];
__align(4) static uint8_t lfs_cache_buf[16];
__align(4) static uint8_t lfs_lookahead_buf[32];
#else
static uint8_t lfs_read_buf[16] __attribute__((aligned(4)));
static uint8_t lfs_prog_buf[16] __attribute__((aligned(4)));
static uint8_t lfs_cache_buf[16] __attribute__((aligned(4)));
static uint8_t lfs_lookahead_buf[32] __attribute__((aligned(4)));
#endif

/*────────────────────────────────────────────
 * 2. 主程序 Demo
 *────────────────────────────────────────────*/
int main(void)
{
    /* 系统初始化 */
    System_Init();
    UART_Init();
    Debug_Init();
    Log_Init();
    
    LOG_INFO("DEMO", "=== LittleFS Simple Demo ===");
    
    /* 初始化 SPI2 + W25Q64 */
    SPI_HW_Init();
    if (W25Q_Init() != W25Q_OK) {
        LOG_ERROR("DEMO", "W25Q64 Init Failed");
        while(1);
    }
    LOG_INFO("DEMO", "W25Q64 OK");
    
    /* LittleFS 初始化 */
    if (LittleFS_Init() != LITTLEFS_OK) {
        LOG_ERROR("DEMO", "LittleFS Init Failed");
        while(1);
    }
    
    /* 格式化（如果需要） */
    if (LittleFS_Mount() != LITTLEFS_OK) {
        LOG_INFO("DEMO", "Mount failed, formatting...");
        if (LittleFS_Format() != LITTLEFS_OK) {
            LOG_ERROR("DEMO", "Format Failed");
            while(1);
        }
        if (LittleFS_Mount() != LITTLEFS_OK) {
            LOG_ERROR("DEMO", "Mount After Format Failed");
            while(1);
        }
    }
    
    LOG_INFO("DEMO", "LittleFS mounted!");
    
    /*────────────────────────────────────────────
     * 3. 写文件（INLINE 模式测试）
     *────────────────────────────────────────────*/
    lfs_file_t file;
    memset(&file, 0, sizeof(file));    /* ★ 必须，否则 CTZ / inline 会出错 */
    
    if (LittleFS_FileOpen(&file, "/test.txt", LFS_O_CREAT | LFS_O_TRUNC | LFS_O_RDWR) != LITTLEFS_OK) {
        LOG_ERROR("DEMO", "Open for write failed");
        while(1);
    }
    
    const char *msg = "Hello";
    uint32_t written = 0;
    if (LittleFS_FileWrite(&file, msg, strlen(msg), &written) != LITTLEFS_OK) {
        LOG_ERROR("DEMO", "Write failed");
        LittleFS_FileClose(&file);
        while(1);
    }
    
    if (LittleFS_FileClose(&file) != LITTLEFS_OK) {
        LOG_ERROR("DEMO", "Close after write failed");
        while(1);
    }
    
    LOG_INFO("DEMO", "Write OK: %lu bytes", (unsigned long)written);
    
    /*────────────────────────────────────────────
     * 4. 读文件（INLINE 文件读回验证）
     *────────────────────────────────────────────*/
    memset(&file, 0, sizeof(file));    /* ★ 必须，避免 lfs_file 结构体污染 */
    
    if (LittleFS_FileOpen(&file, "/test.txt", LFS_O_RDONLY) != LITTLEFS_OK) {
        LOG_ERROR("DEMO", "Read Open ERR (inline bug?)");
        while(1);
    }
    
#ifdef __CC_ARM
    __align(4) static char readbuf[32];
#else
    static char readbuf[32] __attribute__((aligned(4)));
#endif
    
    uint32_t bytes_read = 0;
    if (LittleFS_FileRead(&file, readbuf, sizeof(readbuf) - 1, &bytes_read) != LITTLEFS_OK) {
        LOG_ERROR("DEMO", "Read failed");
        LittleFS_FileClose(&file);
        while(1);
    }
    
    if (LittleFS_FileClose(&file) != LITTLEFS_OK) {
        LOG_ERROR("DEMO", "Close after read failed");
        while(1);
    }
    
    readbuf[bytes_read] = 0;
    LOG_INFO("DEMO", "Read: %s (size: %lu)", readbuf, (unsigned long)bytes_read);  /* 应输出：Hello */
    
    /*────────────────────────────────────────────
     * 5. 卸载
     *────────────────────────────────────────────*/
    if (LittleFS_Unmount() != LITTLEFS_OK) {
        LOG_ERROR("DEMO", "Unmount failed");
    }
    
    LOG_INFO("DEMO", "Done");
    
    while(1) {
        Delay_ms(1000);
    }
}
