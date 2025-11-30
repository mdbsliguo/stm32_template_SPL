/**
 * @file main_simple_littlefs.c
 * @brief 最简化的LittleFS测试 - 不遵循项目规范，快速验证
 */

#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_spi.h"
#include "lfs.h"
#include "lfs_util.h"
#include "w25q_spi.h"
#include "spi_hw.h"
#include "delay.h"
#include "board.h"
#include <string.h>

/* 简化配置：直接定义 */
#define W25Q_PAGE_SIZE      256
#define W25Q_SECTOR_SIZE    4096

/* LittleFS配置 */
static uint8_t read_buffer[256];
static uint8_t prog_buffer[256];
static uint8_t lookahead_buffer[64] __attribute__((aligned(4)));

static lfs_t lfs;
static struct lfs_config cfg;

/* 块设备回调函数 */
static int bd_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
{
    uint32_t addr = block * W25Q_SECTOR_SIZE + off;
    W25Q_Status_t status = W25Q_Read(addr, (uint8_t*)buffer, size);
    return (status == W25Q_OK) ? 0 : LFS_ERR_IO;
}

static int bd_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
    uint32_t addr = block * W25Q_SECTOR_SIZE + off;
    W25Q_Status_t status = W25Q_Write(addr, (const uint8_t*)buffer, size);
    return (status == W25Q_OK) ? 0 : LFS_ERR_IO;
}

static int bd_erase(const struct lfs_config *c, lfs_block_t block)
{
    uint32_t addr = block * W25Q_SECTOR_SIZE;
    W25Q_Status_t status = W25Q_EraseSector(addr);
    return (status == W25Q_OK) ? 0 : LFS_ERR_IO;
}

static int bd_sync(const struct lfs_config *c)
{
    return 0;
}

/* 简单延时 */
static void simple_delay(uint32_t count)
{
    while(count--);
}

/* LED控制（PA1） */
static void led_init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_SetBits(GPIOA, GPIO_Pin_1);
}

static void led_toggle(void)
{
    GPIOA->ODR ^= GPIO_Pin_1;
}

int main(void)
{
    /* 1. 系统时钟初始化 */
    SystemInit();
    
    /* 2. LED初始化 */
    led_init();
    led_toggle();  /* LED闪烁1次 */
    simple_delay(1000000);
    
    /* 3. 延时模块初始化（简单版本） */
    Delay_Init();
    led_toggle();  /* LED闪烁2次 */
    Delay_ms(100);
    
    /* 4. SPI初始化 */
    SPI_HW_Init(SPI_INSTANCE_2);
    led_toggle();  /* LED闪烁3次 */
    Delay_ms(100);
    
    /* 5. W25Q初始化 */
    W25Q_Init();
    led_toggle();  /* LED闪烁4次 */
    Delay_ms(100);
    
    /* 6. LittleFS配置 */
    memset(&cfg, 0, sizeof(cfg));
    cfg.read = bd_read;
    cfg.prog = bd_prog;
    cfg.erase = bd_erase;
    cfg.sync = bd_sync;
    cfg.read_size = W25Q_PAGE_SIZE;
    cfg.prog_size = W25Q_PAGE_SIZE;
    cfg.block_size = W25Q_SECTOR_SIZE;
    cfg.block_count = 8 * 1024 * 1024 / W25Q_SECTOR_SIZE;  /* 8MB / 4KB = 2048块 */
    cfg.block_cycles = 500;
    cfg.cache_size = W25Q_PAGE_SIZE;
    cfg.lookahead_size = 64;
    cfg.read_buffer = read_buffer;
    cfg.prog_buffer = prog_buffer;
    cfg.lookahead_buffer = lookahead_buffer;
    
    led_toggle();  /* LED闪烁5次 */
    Delay_ms(100);
    
    /* 7. 尝试挂载 */
    memset(&lfs, 0, sizeof(lfs));
    int err = lfs_mount(&lfs, &cfg);
    
    if (err == 0) {
        /* 挂载成功 - LED快速闪烁 */
        for(int i = 0; i < 10; i++) {
            led_toggle();
            Delay_ms(100);
        }
    } else {
        /* 挂载失败 - LED慢速闪烁 */
        while(1) {
            led_toggle();
            Delay_ms(500);
        }
    }
    
    /* 8. 主循环 */
    while(1) {
        led_toggle();
        Delay_ms(1000);
    }
}

