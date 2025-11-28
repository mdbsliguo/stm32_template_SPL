/**
 * @file main_example.c
 * @brief Flash06 - TF卡（MicroSD卡）手动初始化读写测速示例
 * @details 演示TF卡手动初始化、真正的CMD18/CMD25多块传输、不同分频下的1MB测速、增量写入和插拔卡处理
 * 
 * 硬件连接：
 * - TF卡（MicroSD卡）连接到SPI2
 *   - CS：PA11（软件NSS模式）
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
 * 
 * 功能演示：
 * 1. 手动初始化演示（CMD0、CMD8、ACMD41、CMD58、CMD9、CMD16）
 * 2. 真正的CMD18/CMD25多块传输（不是循环调用单块读写）
 * 3. 不同SPI分频下的1MB读写速度测试（使用CMD18/CMD25多块传输）
 * 4. 增量写入功能（每5秒写入100KB，使用8分频，读取全部并校验）
 * 5. 插拔卡检测和自动重初始化
 * 
 * @note 本示例使用手动初始化，不依赖TF_SPI_Init()
 * @note 使用真正的CMD18/CMD25多块传输，提高传输效率
 * @note 测速测试：1MB测试数据，使用32块批量传输（约16KB）提高效率
 * @note 增量写入：100KB数据，使用8分频（4.5MHz）标准速度
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
#include "tf_spi.h"
#include "gpio.h"
#include "config.h"
#include "board.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ==================== SD卡命令定义 ==================== */

#define SD_CMD_GO_IDLE_STATE       0x00    /* CMD0：复位卡 */
#define SD_CMD_SEND_IF_COND        0x08    /* CMD8：检查电压兼容性 */
#define SD_CMD_SEND_CSD            0x09    /* CMD9：读取CSD寄存器 */
#define SD_CMD_SEND_CID            0x0A    /* CMD10：读取CID寄存器 */
#define SD_CMD_STOP_TRANSMISSION   0x0C    /* CMD12：停止传输 */
#define SD_CMD_SEND_STATUS         0x0D    /* CMD13：发送状态 */
#define SD_CMD_SET_BLOCKLEN        0x10    /* CMD16：设置块长度 */
#define SD_CMD_READ_SINGLE_BLOCK   0x11    /* CMD17：读取单个块 */
#define SD_CMD_READ_MULTIPLE_BLOCK 0x12    /* CMD18：读取多个块 */
#define SD_CMD_WRITE_BLOCK         0x18    /* CMD24：写入单个块 */
#define SD_CMD_WRITE_MULTIPLE_BLOCK 0x19   /* CMD25：写入多个块 */
#define SD_CMD_APP_CMD             0x37    /* CMD55：应用命令前缀 */
#define SD_CMD_READ_OCR            0x3A    /* CMD58：读取OCR寄存器 */
#define SD_ACMD_SD_SEND_OP_COND    0x29    /* ACMD41：初始化SD卡 */

/* SD卡响应格式 */
#define SD_R1_IDLE_STATE           0x01    /* R1响应：空闲状态 */

/* SD卡数据令牌 */
#define SD_TOKEN_START_BLOCK       0xFE    /* 数据块开始令牌 */
#define SD_TOKEN_STOP_TRANSMISSION 0xFD    /* 停止传输令牌 */
#define SD_TOKEN_DATA_ACCEPTED     0x05    /* 数据接受令牌 */
#define SD_TOKEN_DATA_CRC_ERROR    0x0B    /* 数据CRC错误 */
#define SD_TOKEN_DATA_WRITE_ERROR  0x0D    /* 数据写入错误 */

/* SD卡块大小 */
#define SD_BLOCK_SIZE              512     /* SD卡块大小（字节） */

/* ==================== 测试配置 ==================== */

/* 测速测试数据大小：1MB（便于调试） */
#define SPEED_TEST_SIZE_MB       1
#define SPEED_TEST_SIZE_BYTES    (SPEED_TEST_SIZE_MB * 1024 * 1024)
#define SPEED_TEST_BLOCK_COUNT   (SPEED_TEST_SIZE_BYTES / 512)  /* 每块512字节 */

/* 增量写入配置 */
#define INCREMENTAL_WRITE_SIZE_KB    100
#define INCREMENTAL_WRITE_BLOCK_COUNT (INCREMENTAL_WRITE_SIZE_KB * 1024 / 512)  /* 100KB = 200块 */
#define INCREMENTAL_WRITE_START_BLOCK 1000  /* 起始块地址（避开系统区域） */
#define INCREMENTAL_WRITE_INTERVAL_MS 5000   /* 增量写入间隔：5秒 */
#define INCREMENTAL_WRITE_PRESCALER   SPI_BaudRatePrescaler_8  /* 增量写入使用8分频（4.5MHz） */
#define INCREMENTAL_WRITE_MAX_COUNT   10     /* 最大写入次数（用于测试插拔卡功能） */

/* 插拔卡检测间隔 */
#define CARD_DETECT_INTERVAL_MS  5000  /* 每5秒检测一次 */

/* SPI分频测试列表 */
#define PRESCALER_COUNT       8
static const uint16_t g_prescalers[PRESCALER_COUNT] = {
    SPI_BaudRatePrescaler_2,      /* 分频2（18MHz，最高速度） */
    SPI_BaudRatePrescaler_4,      /* 分频4（9MHz） */
    SPI_BaudRatePrescaler_8,      /* 分频8（4.5MHz） */
    SPI_BaudRatePrescaler_16,     /* 分频16（2.25MHz） */
    SPI_BaudRatePrescaler_32,     /* 分频32（1.125MHz） */
    SPI_BaudRatePrescaler_64,     /* 分频64（562.5kHz） */
    SPI_BaudRatePrescaler_128,    /* 分频128（281.25kHz） */
    SPI_BaudRatePrescaler_256,    /* 分频256（140.625kHz） */
};

/* 分频值对应的数值（用于显示） */
static const uint16_t g_prescaler_values[PRESCALER_COUNT] = {2, 4, 8, 16, 32, 64, 128, 256};

/* ==================== 设备信息结构体 ==================== */

/* 设备信息结构体（自己管理，不依赖TF_SPI模块） */
typedef struct {
    uint32_t capacity_mb;      /**< 容量（MB） */
    uint32_t block_size;       /**< 块大小（字节） */
    uint32_t block_count;     /**< 块数量 */
    uint8_t is_sdhc;          /**< 是否为SDHC/SDXC（1=是，0=SDSC） */
    uint8_t is_initialized;   /**< 是否已初始化（1=是，0=否） */
} ManualDeviceInfo_t;

static ManualDeviceInfo_t g_device_info = {
    .capacity_mb = 0,
    .block_size = 512,
    .block_count = 0,
    .is_sdhc = 0,
    .is_initialized = 0
};

/* ==================== 全局状态变量 ==================== */

/* 速度测试结果结构体 */
typedef struct {
    uint16_t prescaler_value;     /**< 分频值（2, 4, 8...） */
    uint32_t write_time_ms;        /**< 写入耗时（毫秒） */
    uint32_t read_time_ms;         /**< 读取耗时（毫秒） */
    float write_speed_kbps;        /**< 写入速度（KB/s） */
    float read_speed_kbps;         /**< 读取速度（KB/s） */
} SpeedTestResult_t;

/* 测试结果数组 */
static SpeedTestResult_t g_speed_test_results[PRESCALER_COUNT];

/* 测速测试缓冲区（静态分配，避免使用malloc） */
/* 注意：STM32F103C8T6只有20KB RAM，使用16KB缓冲区 */
/* 增加到32块（约16KB），提高传输效率 */
static uint8_t g_speed_test_buffer[512 * 32];  /* 每次处理32块（约16KB） */

/* 增量写入状态 */
typedef struct {
    uint32_t current_block;        /**< 当前写入块地址 */
    uint32_t write_count;          /**< 写入次数 */
    uint32_t last_write_time_ms;   /**< 上次写入时间（毫秒） */
    uint8_t initialized;           /**< 是否已初始化 */
} IncrementalWriteState_t;

static IncrementalWriteState_t g_incremental_write_state = {
    .current_block = INCREMENTAL_WRITE_START_BLOCK,
    .write_count = 0,
    .last_write_time_ms = 0,
    .initialized = 0
};

/* 插拔卡检测状态 */
typedef struct {
    uint32_t last_detect_time_ms;  /**< 上次检测时间（毫秒） */
    uint8_t card_present;          /**< 卡是否存在（1=存在，0=不存在） */
    uint8_t last_init_status;       /**< 上次初始化状态 */
} CardDetectState_t;

static CardDetectState_t g_card_detect_state = {
    .last_detect_time_ms = 0,
    .card_present = 0,
    .last_init_status = 0
};

/* ==================== 辅助函数 ==================== */

/**
 * @brief 动态修改SPI分频
 * @param[in] prescaler SPI分频值（SPI_BaudRatePrescaler_2等）
 * @return SPI_Status_t 错误码
 */
static SPI_Status_t ChangeSPIPrescaler(uint16_t prescaler)
{
    SPI_TypeDef *spi_periph = SPI2;
    uint16_t cr1_temp;
    
    if (spi_periph == NULL)
    {
        return SPI_ERROR_INVALID_PERIPH;
    }
    
    /* 读取CR1寄存器 */
    cr1_temp = spi_periph->CR1;
    
    /* 清除BR位（bit 3-5） */
    cr1_temp &= ~(SPI_CR1_BR);
    
    /* 设置新的分频值 */
    cr1_temp |= prescaler;
    
    /* 写回CR1寄存器 */
    spi_periph->CR1 = cr1_temp;
    
    /* 等待SPI总线稳定 */
    Delay_ms(10);
    
    return SPI_OK;
}

/**
 * @brief 等待SD卡响应（非0xFF）
 * @param[in] instance SPI实例
 * @param[in] timeout_ms 超时时间（毫秒）
 * @return uint8_t 响应值，超时返回0xFF
 */
static uint8_t WaitResponse(SPI_Instance_t instance, uint32_t timeout_ms)
{
    uint8_t response = 0xFF;
    uint32_t start_tick = Delay_GetTick();
    uint8_t rx_data = 0xFF;
    SPI_Status_t spi_status;
    
    while (Delay_GetElapsed(Delay_GetTick(), start_tick) < timeout_ms)
    {
        rx_data = 0xFF;
        spi_status = SPI_MasterTransmitReceive(instance, &rx_data, &response, 1, 0);
        
        if (spi_status == SPI_OK && response != 0xFF)
        {
            return response;
        }
    }
    
    return 0xFF;  /* 超时 */
}

/**
 * @brief 发送SD卡命令（不控制CS，用于CMD18/CMD25等多块传输）
 * @param[in] instance SPI实例
 * @param[in] cmd 命令字节（0x00-0x3F，不需要加0x40）
 * @param[in] arg 命令参数（32位地址/参数）
 * @param[out] response R1响应值
 * @return TF_SPI_Status_t 错误码
 * @note 此函数不控制CS，调用前必须已经拉低CS，调用后不要立即拉高CS
 */
static TF_SPI_Status_t SendCMDNoCS(SPI_Instance_t instance, uint8_t cmd, uint32_t arg, uint8_t *response)
{
    uint8_t cmd_buf[6];
    uint8_t r1;
    uint8_t crc;
    SPI_Status_t spi_status;
    
    /* 构造命令包（6字节：命令+地址+CRC） */
    cmd_buf[0] = cmd | 0x40;  /* 命令字节（bit 6=1） */
    cmd_buf[1] = (uint8_t)(arg >> 24);
    cmd_buf[2] = (uint8_t)(arg >> 16);
    cmd_buf[3] = (uint8_t)(arg >> 8);
    cmd_buf[4] = (uint8_t)(arg);
    
    /* CRC计算（简化：CMD0和CMD8使用固定CRC，其他使用0xFF） */
    uint8_t cmd_index = cmd & 0x3F;
    if (cmd_index == 0x00)  /* CMD0 */
    {
        crc = 0x95;
    }
    else if (cmd_index == 0x08)  /* CMD8 */
    {
        crc = 0x87;
    }
    else
    {
        crc = 0xFF;  /* 其他命令使用0xFF（SD卡SPI模式不检查CRC） */
    }
    cmd_buf[5] = crc;
    
    /* 发送命令 */
    spi_status = SPI_MasterTransmit(instance, cmd_buf, 6, 1000);
    if (spi_status != SPI_OK)
    {
        if (response != NULL)
        {
            *response = 0xFF;
        }
        return TF_SPI_ERROR_CMD_FAILED;
    }
    
    /* 等待响应（最多8字节，超时100ms） */
    r1 = WaitResponse(instance, 100);
    
    if (response != NULL)
    {
        *response = r1;
    }
    
    if (r1 == 0xFF)
    {
        return TF_SPI_ERROR_TIMEOUT;
    }
    
    return TF_SPI_OK;
}

/**
 * @brief 等待SD卡数据令牌（0xFE）
 * @param[in] instance SPI实例
 * @param[in] timeout_ms 超时时间（毫秒，0表示无限等待）
 * @return uint8_t 令牌值，超时返回0xFF
 */
static uint8_t WaitDataToken(SPI_Instance_t instance, uint32_t timeout_ms)
{
    uint8_t token = 0xFF;
    uint32_t start_tick = Delay_GetTick();
    uint8_t rx_data = 0xFF;
    SPI_Status_t spi_status;
    
    while (1)
    {
        if (timeout_ms > 0)
        {
            if (Delay_GetElapsed(Delay_GetTick(), start_tick) >= timeout_ms)
            {
                return 0xFF;  /* 超时 */
            }
        }
        
        rx_data = 0xFF;
        spi_status = SPI_MasterTransmitReceive(instance, &rx_data, &token, 1, 0);
        
        if (spi_status == SPI_OK && token != 0xFF)
        {
            return token;
        }
    }
}

/**
 * @brief 等待SD卡忙（DO=0）
 * @param[in] instance SPI实例
 * @param[in] timeout_ms 超时时间（毫秒）
 * @return uint8_t 1-成功（卡就绪），0-超时
 */
static uint8_t WaitCardReady(SPI_Instance_t instance, uint32_t timeout_ms)
{
    uint8_t response = 0x00;
    uint32_t start_tick = Delay_GetTick();
    uint8_t rx_data = 0xFF;
    SPI_Status_t spi_status;
    
    while (Delay_GetElapsed(Delay_GetTick(), start_tick) < timeout_ms)
    {
        rx_data = 0xFF;
        spi_status = SPI_MasterTransmitReceive(instance, &rx_data, &response, 1, 0);
        
        if (spi_status == SPI_OK && response == 0xFF)
        {
            return 1;  /* 卡就绪 */
        }
    }
    
    return 0;  /* 超时 */
}

/**
 * @brief 发送应用命令（ACMD）
 * @param[in] instance SPI实例
 * @param[in] cmd 应用命令字节
 * @param[in] arg 命令参数
 * @param[out] response R1响应值
 * @return TF_SPI_Status_t 错误码
 */
static TF_SPI_Status_t SendACMD(SPI_Instance_t instance, uint8_t cmd, uint32_t arg, uint8_t *response)
{
    TF_SPI_Status_t status;
    uint8_t r1;
    
    /* 先发送CMD55（应用命令前缀） */
    status = TF_SPI_SendCMD(SD_CMD_APP_CMD, 0, &r1);
    if (status != TF_SPI_OK)
    {
        if (response != NULL)
        {
            *response = r1;
        }
        return status;
    }
    
    /* CMD55可以返回0x00（正常）或0x01（IDLE状态），都是正常的 */
    /* 在初始化过程中，CMD55返回0x01是正常的，表示卡还在IDLE状态 */
    if (r1 != 0x00 && r1 != 0x01)
    {
        /* CMD55返回其他值（如0xFF超时、0x04 ILLEGAL_CMD等），表示错误 */
        if (response != NULL)
        {
            *response = r1;
        }
        return TF_SPI_ERROR_CMD_FAILED;
    }
    
    /* 再发送实际的应用命令 */
    status = TF_SPI_SendCMD(cmd, arg, &r1);
    if (response != NULL)
    {
        *response = r1;
    }
    
    return status;
}

/**
 * @brief 块地址转换（SDHC/SDXC使用块地址，SDSC使用字节地址）
 * @param[in] block_addr 块地址
 * @return uint32_t 转换后的地址
 */
static uint32_t BlockToAddr(uint32_t block_addr)
{
    if (g_device_info.is_sdhc)
    {
        /* SDHC/SDXC：直接使用块地址 */
        return block_addr;
    }
    else
    {
        /* SDSC：使用字节地址 */
        return block_addr * 512;
    }
}

/**
 * @brief 解析CSD寄存器（CSD版本1.0 - SDSC）
 * @param[in] csd CSD寄存器数据（16字节）
 * @param[out] capacity_mb 容量（MB）
 * @param[out] block_size 块大小（字节）
 * @param[out] block_count 块数量
 * @return uint8_t 1-成功，0-失败
 */
static uint8_t ParseCSD_V1(const uint8_t *csd, uint32_t *capacity_mb, uint32_t *block_size, uint32_t *block_count)
{
    uint32_t c_size;
    uint8_t c_size_mult;
    uint32_t read_bl_len;
    uint64_t capacity_bytes_64;
    
    if (csd == NULL || capacity_mb == NULL || block_size == NULL || block_count == NULL)
    {
        return 0;
    }
    
    /* 计算C_SIZE */
    c_size = ((uint32_t)(csd[6] & 0x03) << 10) | ((uint32_t)csd[7] << 2) | ((csd[8] >> 6) & 0x03);
    
    /* 计算C_SIZE_MULT */
    c_size_mult = ((csd[9] & 0x03) << 1) | ((csd[10] >> 7) & 0x01);
    
    /* 计算READ_BL_LEN */
    read_bl_len = csd[5] & 0x0F;
    
    /* 容量 = (C_SIZE + 1) * 512 * 2^(C_SIZE_MULT + 2) 字节 */
    capacity_bytes_64 = ((uint64_t)(c_size + 1)) * (1ULL << (c_size_mult + 2)) * (1ULL << read_bl_len);
    
    /* 检查是否超出32位范围 */
    if (capacity_bytes_64 > UINT32_MAX)
    {
        return 0;
    }
    
    *capacity_mb = (uint32_t)(capacity_bytes_64 / (1024UL * 1024UL));
    *block_size = 512;
    *block_count = (uint32_t)(capacity_bytes_64 / 512);
    
    return 1;
}

/**
 * @brief 解析CSD寄存器（CSD版本2.0 - SDHC/SDXC）
 * @param[in] csd CSD寄存器数据（16字节）
 * @param[out] capacity_mb 容量（MB）
 * @param[out] block_size 块大小（字节）
 * @param[out] block_count 块数量
 * @return uint8_t 1-成功，0-失败
 */
static uint8_t ParseCSD_V2(const uint8_t *csd, uint32_t *capacity_mb, uint32_t *block_size, uint32_t *block_count)
{
    uint32_t c_size;
    uint64_t capacity_bytes_64;
    
    if (csd == NULL || capacity_mb == NULL || block_size == NULL || block_count == NULL)
    {
        return 0;
    }
    
    /* 计算C_SIZE */
    c_size = ((uint32_t)(csd[7] & 0x3F) << 16) | ((uint32_t)csd[8] << 8) | csd[9];
    
    /* 容量 = (C_SIZE + 1) * 512KB */
    capacity_bytes_64 = ((uint64_t)(c_size + 1)) * 512ULL * 1024ULL;
    
    /* 使用64位计算容量，然后转换为32位（如果超出32位范围则限制） */
    uint64_t capacity_mb_64 = capacity_bytes_64 / (1024ULL * 1024ULL);
    uint64_t block_count_64 = capacity_bytes_64 / 512ULL;
    
    /* 检查是否超出32位范围，如果超出则限制，但尽量显示正确值 */
    if (capacity_mb_64 > UINT32_MAX)
    {
        *capacity_mb = UINT32_MAX;
    }
    else
    {
        *capacity_mb = (uint32_t)capacity_mb_64;
    }
    
    if (block_count_64 > UINT32_MAX)
    {
        *block_count = UINT32_MAX;
    }
    else
    {
        *block_count = (uint32_t)block_count_64;
    }
    
    *block_size = 512;
    
    return 1;
}

/* ==================== 手动初始化函数 ==================== */

/**
 * @brief 手动初始化TF卡
 * @return TF_SPI_Status_t 错误码
 * @note 实现完整的手动初始化流程：CMD0、CMD8、ACMD41、CMD58、CMD9、CMD16
 */
static TF_SPI_Status_t ManualInitTF(void)
{
    SPI_Instance_t spi_instance = TF_SPI_SPI_INSTANCE;
    TF_SPI_Status_t status;
    uint8_t response;
    uint8_t cmd8_buf[6];
    uint8_t r7[4];
    uint32_t ocr;
    uint8_t csd[16];
    uint8_t csd_structure;
    uint8_t is_sd_v2 = 0;
    uint32_t retry_count;
    uint8_t dummy = 0xFF;
    SPI_Status_t spi_status;
    
    /* 清除设备信息 */
    memset(&g_device_info, 0, sizeof(g_device_info));
    g_device_info.block_size = 512;
    
    /* 检查SPI是否已初始化 */
    if (!SPI_IsInitialized(spi_instance))
    {
        LOG_ERROR("MAIN", "SPI未初始化");
        return TF_SPI_ERROR_INIT_FAILED;
    }
    
    /* 1. 上电复位：CS拉高后发送至少8个时钟周期（10个0xFF） */
    LOG_INFO("MAIN", "=== 手动初始化TF卡 ===");
    LOG_INFO("MAIN", "步骤1: 上电复位（发送10个0xFF）");
    
    SPI_NSS_High(spi_instance);
    for (uint8_t i = 0; i < 10; i++)
    {
        SPI_MasterTransmit(spi_instance, &dummy, 1, 100);
    }
    Delay_ms(10);
    
    /* 2. CMD0（复位卡） */
    LOG_INFO("MAIN", "步骤2: 发送CMD0（复位卡）");
    
    /* 如果之前已经初始化过，可能需要多次发送CMD0才能让卡进入IDLE状态 */
    for (uint8_t retry = 0; retry < 3; retry++)
    {
        status = TF_SPI_SendCMD(SD_CMD_GO_IDLE_STATE, 0, &response);
        
        if (status == TF_SPI_OK && response == SD_R1_IDLE_STATE)
        {
            LOG_INFO("MAIN", "CMD0成功: response=0x%02X (IDLE_STATE)", response);
            break;
        }
        else if (retry < 2)
        {
            LOG_WARN("MAIN", "CMD0重试 %d: status=%d, response=0x%02X", retry + 1, status, response);
            Delay_ms(10);
        }
        else
        {
            LOG_ERROR("MAIN", "CMD0失败: status=%d, response=0x%02X", status, response);
            return TF_SPI_ERROR_INIT_FAILED;
        }
    }
    
    LOG_INFO("MAIN", "CMD0成功: response=0x%02X (IDLE_STATE)", response);
    Delay_ms(100);
    
    /* 3. CMD8（检查电压兼容性，SD V2.0+） */
    LOG_INFO("MAIN", "步骤3: 发送CMD8（检查电压兼容性）");
    
    /* 构造CMD8命令包 */
    cmd8_buf[0] = (SD_CMD_SEND_IF_COND | 0x40);
    cmd8_buf[1] = 0x00;
    cmd8_buf[2] = 0x00;
    cmd8_buf[3] = 0x01;  /* 电压范围：2.7-3.6V */
    cmd8_buf[4] = 0xAA;  /* 检查模式：0xAA */
    cmd8_buf[5] = 0x87;  /* CMD8的CRC */
    
    SPI_NSS_Low(spi_instance);
    dummy = 0xFF;
    SPI_MasterTransmit(spi_instance, &dummy, 1, 100);  /* 同步 */
    spi_status = SPI_MasterTransmit(spi_instance, cmd8_buf, 6, 1000);
    
    if (spi_status != SPI_OK)
    {
        SPI_NSS_High(spi_instance);
        SPI_MasterTransmit(spi_instance, &dummy, 1, 100);
        LOG_ERROR("MAIN", "CMD8发送失败");
        return TF_SPI_ERROR_INIT_FAILED;
    }
    
    /* 等待R1响应 */
    response = WaitResponse(spi_instance, 100);
    
    if (response == SD_R1_IDLE_STATE)
    {
        /* SD V2.0+，读取R7响应的剩余4字节 */
        spi_status = SPI_MasterReceive(spi_instance, r7, 4, 1000);
        SPI_NSS_High(spi_instance);
        SPI_MasterTransmit(spi_instance, &dummy, 1, 100);
        
        if (spi_status == SPI_OK)
        {
            is_sd_v2 = 1;
            LOG_INFO("MAIN", "CMD8成功: SD V2.0+, 电压=0x%02X, 检查模式=0x%02X", r7[2], r7[3]);
        }
        else
        {
            SPI_NSS_High(spi_instance);
            SPI_MasterTransmit(spi_instance, &dummy, 1, 100);
            LOG_ERROR("MAIN", "CMD8 R7读取失败");
            return TF_SPI_ERROR_INIT_FAILED;
        }
    }
    else if (response == (SD_R1_IDLE_STATE | 0x04))  /* ILLEGAL_CMD */
    {
        /* SD V1.0，不支持CMD8 */
        SPI_NSS_High(spi_instance);
        SPI_MasterTransmit(spi_instance, &dummy, 1, 100);
        is_sd_v2 = 0;
        LOG_INFO("MAIN", "CMD8返回ILLEGAL_CMD，检测到SD V1.0");
    }
    else
    {
        SPI_NSS_High(spi_instance);
        SPI_MasterTransmit(spi_instance, &dummy, 1, 100);
        LOG_ERROR("MAIN", "CMD8失败: response=0x%02X", response);
        return TF_SPI_ERROR_INIT_FAILED;
    }
    
    /* 4. ACMD41（初始化SD卡） */
    LOG_INFO("MAIN", "步骤4: 发送ACMD41（初始化SD卡）");
    
    retry_count = 0;
    uint8_t init_success = 0;
    ocr = 0;
    
    while (retry_count < 20)  /* 最多等待2秒（20次 * 100ms） */
    {
        /* 发送CMD55 + ACMD41 */
        status = SendACMD(spi_instance, SD_ACMD_SD_SEND_OP_COND, is_sd_v2 ? 0x40000000 : 0, &response);
        
        if (status != TF_SPI_OK)
        {
            LOG_WARN("MAIN", "ACMD41发送失败: status=%d, retry=%lu", status, retry_count);
            Delay_ms(100);
            retry_count++;
            continue;
        }
        
        /* 记录响应值 */
        if (retry_count == 0 || retry_count % 5 == 0)
        {
            LOG_INFO("MAIN", "ACMD41响应: 0x%02X (retry=%lu)", response, retry_count);
        }
        
        /* 读取OCR检查bit31（卡就绪），无论ACMD41返回0x00还是0x01都检查 */
        status = TF_SPI_ReadOCR(&ocr);
        if (status == TF_SPI_OK)
        {
            if (ocr & 0x80000000)
            {
                /* OCR bit31置位，卡就绪 */
                init_success = 1;
                LOG_INFO("MAIN", "ACMD41成功: OCR=0x%08X, 卡就绪 (retry=%lu)", ocr, retry_count);
                break;
            }
            else if (retry_count == 0 || retry_count % 5 == 0)
            {
                LOG_INFO("MAIN", "OCR=0x%08X, 卡未就绪，继续等待...", ocr);
            }
        }
        else
        {
            LOG_WARN("MAIN", "读取OCR失败: status=%d", status);
        }
        
        Delay_ms(100);
        retry_count++;
    }
    
    if (!init_success)
    {
        LOG_ERROR("MAIN", "ACMD41初始化失败: 已重试%lu次", retry_count);
        if (ocr != 0)
        {
            LOG_ERROR("MAIN", "最后OCR值: 0x%08X (bit31=%d)", ocr, (ocr & 0x80000000) ? 1 : 0);
        }
        return TF_SPI_ERROR_INIT_FAILED;
    }
    
    /* 5. CMD58（读取OCR，检查CCS位） */
    LOG_INFO("MAIN", "步骤5: 读取OCR（CMD58）");
    
    if (ocr == 0)
    {
        status = TF_SPI_ReadOCR(&ocr);
        if (status != TF_SPI_OK)
        {
            LOG_ERROR("MAIN", "CMD58失败");
            return TF_SPI_ERROR_INIT_FAILED;
        }
    }
    
    /* 检查CCS位（bit 30） */
    if (ocr & 0x40000000)
    {
        g_device_info.is_sdhc = 1;
        LOG_INFO("MAIN", "OCR: 0x%08X, CCS=1 (SDHC/SDXC)", ocr);
    }
    else
    {
        g_device_info.is_sdhc = 0;
        LOG_INFO("MAIN", "OCR: 0x%08X, CCS=0 (SDSC)", ocr);
    }
    
    /* 6. CMD9（读取CSD） */
    LOG_INFO("MAIN", "步骤6: 读取CSD（CMD9）");
    
    status = TF_SPI_ReadCSD(csd);
    if (status != TF_SPI_OK)
    {
        LOG_ERROR("MAIN", "CMD9失败");
        return TF_SPI_ERROR_INIT_FAILED;
    }
    
    /* 解析CSD */
    csd_structure = (csd[0] >> 6) & 0x03;
    
    if (csd_structure == 0)
    {
        /* CSD版本1.0（SDSC） */
        if (!ParseCSD_V1(csd, &g_device_info.capacity_mb, &g_device_info.block_size, &g_device_info.block_count))
        {
            LOG_ERROR("MAIN", "CSD V1解析失败");
            return TF_SPI_ERROR_INIT_FAILED;
        }
    }
    else if (csd_structure == 1)
    {
        /* CSD版本2.0（SDHC/SDXC） */
        if (!ParseCSD_V2(csd, &g_device_info.capacity_mb, &g_device_info.block_size, &g_device_info.block_count))
        {
            LOG_ERROR("MAIN", "CSD V2解析失败");
            return TF_SPI_ERROR_INIT_FAILED;
        }
    }
    else
    {
        LOG_ERROR("MAIN", "不支持的CSD版本: %d", csd_structure);
        return TF_SPI_ERROR_INIT_FAILED;
    }
    
    LOG_INFO("MAIN", "CSD解析成功: 容量=%d MB, 块大小=%d, 块数量=%d", 
             g_device_info.capacity_mb, g_device_info.block_size, g_device_info.block_count);
    
    /* 7. CMD16（设置块长度，仅SDSC） */
    if (!g_device_info.is_sdhc)
    {
        LOG_INFO("MAIN", "步骤7: 发送CMD16（设置块长度为512字节，仅SDSC）");
        status = TF_SPI_SendCMD(SD_CMD_SET_BLOCKLEN, 512, &response);
        
        if (status != TF_SPI_OK || response != 0x00)
        {
            LOG_ERROR("MAIN", "CMD16失败: status=%d, response=0x%02X", status, response);
            return TF_SPI_ERROR_INIT_FAILED;
        }
        
        LOG_INFO("MAIN", "CMD16成功");
    }
    
    /* 设置初始化标志 */
    g_device_info.is_initialized = 1;
    
    LOG_INFO("MAIN", "手动初始化成功: 容量=%d MB, 类型=%s", 
             g_device_info.capacity_mb, g_device_info.is_sdhc ? "SDHC/SDXC" : "SDSC");
    
    return TF_SPI_OK;
}

/* ==================== CMD18多块读取函数 ==================== */

/**
 * @brief 使用CMD18进行多块读取（真正的多块传输）
 * @param[in] block_addr 起始块地址
 * @param[in] block_count 块数量
 * @param[out] buf 数据缓冲区（block_count * 512字节）
 * @return TF_SPI_Status_t 错误码
 */
static TF_SPI_Status_t ManualReadBlocks(uint32_t block_addr, uint32_t block_count, uint8_t *buf)
{
    SPI_Instance_t spi_instance = TF_SPI_SPI_INSTANCE;
    TF_SPI_Status_t status;
    uint8_t response;
    uint32_t addr;
    uint8_t token;
    uint32_t i;
    SPI_Status_t spi_status;
    uint8_t dummy = 0xFF;
    uint16_t crc[2];
    
    /* 参数校验 */
    if (buf == NULL || block_count == 0)
    {
        return TF_SPI_ERROR_NULL_PTR;
    }
    
    if (!g_device_info.is_initialized)
    {
        return TF_SPI_ERROR_NOT_INIT;
    }
    
    if (block_addr + block_count > g_device_info.block_count)
    {
        return TF_SPI_ERROR_OUT_OF_BOUND;
    }
    
    /* 地址转换 */
    addr = BlockToAddr(block_addr);
    
    /* 1. CS拉低 */
    SPI_NSS_Low(spi_instance);
    dummy = 0xFF;
    SPI_MasterTransmit(spi_instance, &dummy, 1, 100);  /* 同步 */
    
    /* 2. 发送CMD18 + 块地址 + CRC（不控制CS） */
    status = SendCMDNoCS(spi_instance, SD_CMD_READ_MULTIPLE_BLOCK, addr, &response);
    
    if (status != TF_SPI_OK || response != 0x00)
    {
        SPI_NSS_High(spi_instance);
        SPI_MasterTransmit(spi_instance, &dummy, 1, 100);
        return TF_SPI_ERROR_CMD_FAILED;
    }
    
    /* 3. 循环读取多个块 */
    for (i = 0; i < block_count; i++)
    {
        /* 4. 等待数据令牌0xFE（可无限等待） */
        token = WaitDataToken(spi_instance, 0);  /* 0表示无限等待 */
        
        if (token != SD_TOKEN_START_BLOCK)
        {
            /* 发送CMD12停止传输（不控制CS，因为CS已经拉低） */
            SendCMDNoCS(spi_instance, SD_CMD_STOP_TRANSMISSION, 0, &response);
            SPI_NSS_High(spi_instance);
            SPI_MasterTransmit(spi_instance, &dummy, 1, 100);
            return TF_SPI_ERROR_CMD_FAILED;
        }
        
        /* 5. 读取512字节数据 */
        spi_status = SPI_MasterReceive(spi_instance, buf + i * SD_BLOCK_SIZE, SD_BLOCK_SIZE, 5000);
        if (spi_status != SPI_OK)
        {
            /* 发送CMD12停止传输 */
            TF_SPI_SendCMD(SD_CMD_STOP_TRANSMISSION, 0, &response);
            SPI_NSS_High(spi_instance);
            SPI_MasterTransmit(spi_instance, &dummy, 1, 100);
            return TF_SPI_ERROR_CMD_FAILED;
        }
        
        /* 6. 读取2字节CRC（丢弃） */
        SPI_MasterReceive(spi_instance, (uint8_t *)crc, 2, 100);
    }
    
    /* 7. 发送CMD12停止传输（不控制CS，因为CS已经拉低） */
    status = SendCMDNoCS(spi_instance, SD_CMD_STOP_TRANSMISSION, 0, &response);
    
    /* 8. CS拉高 */
    SPI_NSS_High(spi_instance);
    SPI_MasterTransmit(spi_instance, &dummy, 1, 100);
    
    return TF_SPI_OK;
}

/* ==================== CMD25多块写入函数 ==================== */

/**
 * @brief 使用CMD25进行多块写入（真正的多块传输）
 * @param[in] block_addr 起始块地址
 * @param[in] block_count 块数量
 * @param[in] buf 数据缓冲区（block_count * 512字节）
 * @return TF_SPI_Status_t 错误码
 */
static TF_SPI_Status_t ManualWriteBlocks(uint32_t block_addr, uint32_t block_count, const uint8_t *buf)
{
    SPI_Instance_t spi_instance = TF_SPI_SPI_INSTANCE;
    TF_SPI_Status_t status;
    uint8_t response;
    uint32_t addr;
    uint8_t token;
    uint32_t i;
    SPI_Status_t spi_status;
    uint8_t dummy = 0xFF;
    
    /* 参数校验 */
    if (buf == NULL || block_count == 0)
    {
        return TF_SPI_ERROR_NULL_PTR;
    }
    
    if (!g_device_info.is_initialized)
    {
        return TF_SPI_ERROR_NOT_INIT;
    }
    
    if (block_addr + block_count > g_device_info.block_count)
    {
        return TF_SPI_ERROR_OUT_OF_BOUND;
    }
    
    /* 地址转换 */
    addr = BlockToAddr(block_addr);
    
    /* 1. CS拉低 */
    SPI_NSS_Low(spi_instance);
    dummy = 0xFF;
    SPI_MasterTransmit(spi_instance, &dummy, 1, 100);  /* 同步 */
    
    /* 2. 发送CMD25 + 块地址 + CRC（不控制CS） */
    status = SendCMDNoCS(spi_instance, SD_CMD_WRITE_MULTIPLE_BLOCK, addr, &response);
    
    if (status != TF_SPI_OK || response != 0x00)
    {
        SPI_NSS_High(spi_instance);
        SPI_MasterTransmit(spi_instance, &dummy, 1, 100);
        return TF_SPI_ERROR_CMD_FAILED;
    }
    
    /* 3. 循环写入多个块 */
    for (i = 0; i < block_count; i++)
    {
        /* 4. 发送起始令牌0xFC */
        token = 0xFC;
        spi_status = SPI_MasterTransmit(spi_instance, &token, 1, 100);
        if (spi_status != SPI_OK)
        {
            /* 发送停止令牌0xFD */
            token = 0xFD;
            SPI_MasterTransmit(spi_instance, &token, 1, 100);
            WaitCardReady(spi_instance, 5000);
            SPI_NSS_High(spi_instance);
            SPI_MasterTransmit(spi_instance, &dummy, 1, 100);
            return TF_SPI_ERROR_CMD_FAILED;
        }
        
        /* 5. 写入512字节数据 */
        spi_status = SPI_MasterTransmit(spi_instance, buf + i * SD_BLOCK_SIZE, SD_BLOCK_SIZE, 5000);
        if (spi_status != SPI_OK)
        {
            /* 发送停止令牌0xFD */
            token = 0xFD;
            SPI_MasterTransmit(spi_instance, &token, 1, 100);
            WaitCardReady(spi_instance, 5000);
            SPI_NSS_High(spi_instance);
            SPI_MasterTransmit(spi_instance, &dummy, 1, 100);
            return TF_SPI_ERROR_CMD_FAILED;
        }
        
        /* 6. 发送2字节CRC（可固定0x0000） */
        dummy = 0x00;
        SPI_MasterTransmit(spi_instance, &dummy, 1, 100);
        SPI_MasterTransmit(spi_instance, &dummy, 1, 100);
        
        /* 7. 等待响应令牌（0x05=成功，0x0B=CRC错，0x0D=写入错） */
        response = WaitResponse(spi_instance, 100);
        
        if ((response & 0x1F) != SD_TOKEN_DATA_ACCEPTED)
        {
            /* 发送停止令牌0xFD */
            token = 0xFD;
            SPI_MasterTransmit(spi_instance, &token, 1, 100);
            WaitCardReady(spi_instance, 5000);
            SPI_NSS_High(spi_instance);
            SPI_MasterTransmit(spi_instance, &dummy, 1, 100);
            
            if ((response & 0x1F) == SD_TOKEN_DATA_CRC_ERROR)
            {
                return TF_SPI_ERROR_CRC;
            }
            else if ((response & 0x1F) == SD_TOKEN_DATA_WRITE_ERROR)
            {
                return TF_SPI_ERROR_WRITE_PROTECT;
            }
            else
            {
                return TF_SPI_ERROR_CMD_FAILED;
            }
        }
        
        /* 8. 等待SD卡忙（DO=0） */
        if (!WaitCardReady(spi_instance, 5000))
        {
            /* 发送停止令牌0xFD */
            token = 0xFD;
            SPI_MasterTransmit(spi_instance, &token, 1, 100);
            WaitCardReady(spi_instance, 5000);
            SPI_NSS_High(spi_instance);
            SPI_MasterTransmit(spi_instance, &dummy, 1, 100);
            return TF_SPI_ERROR_TIMEOUT;
        }
    }
    
    /* 9. 发送停止令牌0xFD */
    dummy = 0xFF;
    token = 0xFD;
    spi_status = SPI_MasterTransmit(spi_instance, &token, 1, 100);
    
    /* 10. 等待SD卡忙 */
    if (!WaitCardReady(spi_instance, 5000))
    {
        SPI_NSS_High(spi_instance);
        SPI_MasterTransmit(spi_instance, &dummy, 1, 100);
        return TF_SPI_ERROR_TIMEOUT;
    }
    
    /* 11. CS拉高 */
    SPI_NSS_High(spi_instance);
    SPI_MasterTransmit(spi_instance, &dummy, 1, 100);
    
    return TF_SPI_OK;
}

/* ==================== 辅助函数 ==================== */

/**
 * @brief 获取分频值对应的数值
 * @param[in] prescaler SPI分频值（SPI_BaudRatePrescaler_2等）
 * @return uint16_t 分频数值（2, 4, 8...）
 */
static uint16_t GetPrescalerValue(uint16_t prescaler)
{
    uint8_t i;
    for (i = 0; i < PRESCALER_COUNT; i++)
    {
        if (g_prescalers[i] == prescaler)
        {
            return g_prescaler_values[i];
        }
    }
    return 0;
}

/**
 * @brief 计算速度（KB/s）
 * @param[in] size_bytes 数据大小（字节）
 * @param[in] time_ms 耗时（毫秒）
 * @return float 速度（KB/s）
 */
static float CalculateSpeed(uint32_t size_bytes, uint32_t time_ms)
{
    if (time_ms == 0)
    {
        return 0.0f;
    }
    
    /* 速度 = 数据大小(KB) / 耗时(秒) = (size_bytes / 1024) / (time_ms / 1000) */
    return ((float)size_bytes / 1024.0f) / ((float)time_ms / 1000.0f);
}

/**
 * @brief 在OLED上显示当前测试状态
 * @param[in] prescaler_value 当前分频值
 * @param[in] test_index 当前测试索引（0-7）
 * @param[in] total_tests 总测试数
 * @param[in] operation 操作类型（"Write"或"Read"）
 */
static void DisplaySpeedTestStatus(uint16_t prescaler_value, uint8_t test_index, 
                                   uint8_t total_tests, const char *operation)
{
    char buffer[17];
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Speed Test");
    snprintf(buffer, sizeof(buffer), "P:%d %d/%d", prescaler_value, test_index + 1, total_tests);
    OLED_ShowString(2, 1, buffer);
    snprintf(buffer, sizeof(buffer), "%s %dMB...", operation, SPEED_TEST_SIZE_MB);
    OLED_ShowString(3, 1, buffer);
    OLED_ShowString(4, 1, "Please wait...");
}

/**
 * @brief 准备测试数据（递增序列）
 * @param[out] buffer 数据缓冲区
 * @param[in] size 数据大小
 */
static void PrepareTestData(uint8_t *buffer, uint32_t size)
{
    uint32_t i;
    for (i = 0; i < size; i++)
    {
        buffer[i] = (uint8_t)(i & 0xFF);  /* 递增序列：0x00-0xFF循环 */
    }
}

/* ==================== 演示1：手动初始化演示 ==================== */

/**
 * @brief 演示手动初始化函数列表和用法
 */
static void DemoManualInit(void)
{
    LOG_INFO("MAIN", "=== 演示1：手动初始化函数列表 ===");
    LOG_INFO("MAIN", "");
    LOG_INFO("MAIN", "1. ManualInitTF()");
    LOG_INFO("MAIN", "   功能：手动初始化TF卡，实现完整的SD协议初始化流程");
    LOG_INFO("MAIN", "   流程：CMD0 -> CMD8 -> ACMD41 -> CMD58 -> CMD9 -> CMD16");
    LOG_INFO("MAIN", "   返回：TF_SPI_Status_t（TF_SPI_OK表示成功）");
    LOG_INFO("MAIN", "");
    LOG_INFO("MAIN", "2. ManualReadBlocks(block_addr, block_count, buf)");
    LOG_INFO("MAIN", "   功能：使用CMD18进行真正的多块读取（不是循环调用单块读取）");
    LOG_INFO("MAIN", "   参数：block_addr（起始块地址），block_count（块数量），buf（缓冲区）");
    LOG_INFO("MAIN", "   返回：TF_SPI_Status_t");
    LOG_INFO("MAIN", "");
    LOG_INFO("MAIN", "3. ManualWriteBlocks(block_addr, block_count, buf)");
    LOG_INFO("MAIN", "   功能：使用CMD25进行真正的多块写入（不是循环调用单块写入）");
    LOG_INFO("MAIN", "   参数：block_addr（起始块地址），block_count（块数量），buf（数据）");
    LOG_INFO("MAIN", "   返回：TF_SPI_Status_t");
    LOG_INFO("MAIN", "");
    LOG_INFO("MAIN", "=== 当前设备信息 ===");
    
    if (g_device_info.is_initialized)
    {
        LOG_INFO("MAIN", "容量: %d MB", g_device_info.capacity_mb);
        LOG_INFO("MAIN", "块大小: %d 字节", g_device_info.block_size);
        LOG_INFO("MAIN", "块数量: %d", g_device_info.block_count);
        LOG_INFO("MAIN", "卡类型: %s", g_device_info.is_sdhc ? "SDHC/SDXC" : "SDSC");
    }
    else
    {
        LOG_WARN("MAIN", "设备未初始化，无法获取信息");
    }
    
    /* OLED显示 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Manual Init");
    OLED_ShowString(2, 1, "CMD18/CMD25");
    if (g_device_info.is_initialized)
    {
        char buf[17];
        snprintf(buf, sizeof(buf), "Cap: %d MB", g_device_info.capacity_mb);
        OLED_ShowString(3, 1, buf);
        OLED_ShowString(4, 1, "See UART Log");
    }
    else
    {
        OLED_ShowString(3, 1, "Not Init");
    }
    
    Delay_ms(3000);
}

/* ==================== 演示2：测速测试 ==================== */

/**
 * @brief 执行测速测试
 * @return uint8_t 1-成功，0-失败
 */
static uint8_t PerformSpeedTest(void)
{
    SPI_Status_t spi_status;
    uint8_t test_index;
    uint32_t start_time, end_time;
    uint32_t test_start_block = 1000;  /* 测试起始块地址 */
    uint8_t *test_buffer;
    uint32_t buffer_size;
    
    LOG_INFO("MAIN", "=== 演示2：不同分频下的1MB测速测试 ===");
    LOG_INFO("MAIN", "测试数据大小: %d MB (%d 块)", SPEED_TEST_SIZE_MB, SPEED_TEST_BLOCK_COUNT);
    LOG_INFO("MAIN", "测试分频: 2, 4, 8, 16, 32, 64, 128, 256");
    LOG_INFO("MAIN", "注意：初始化时使用256分频（≤400kHz），初始化完成后可切换到更高速度");
    LOG_INFO("MAIN", "如果某个分频测试失败，会自动跳过该分频");
    LOG_INFO("MAIN", "");
    
    /* 检查设备信息 */
    if (!g_device_info.is_initialized)
    {
        LOG_WARN("MAIN", "设备未初始化，尝试重新初始化...");
        /* 先恢复到初始化速度 */
        ChangeSPIPrescaler(SPI_BaudRatePrescaler_256);
        Delay_ms(10);
        
        if (ManualInitTF() != 0)
        {
            LOG_ERROR("MAIN", "SD卡重新初始化失败，无法执行测速测试");
            return 0;
        }
    }
    
    /* 检查容量是否足够 */
    if (test_start_block + SPEED_TEST_BLOCK_COUNT > g_device_info.block_count)
    {
        LOG_ERROR("MAIN", "SD卡容量不足，无法执行1MB测试");
        LOG_ERROR("MAIN", "需要: %d 块，可用: %d 块", 
                 test_start_block + SPEED_TEST_BLOCK_COUNT, g_device_info.block_count);
        return 0;
    }
    
    /* 使用全局静态缓冲区（避免使用malloc） */
    buffer_size = sizeof(g_speed_test_buffer);  /* 约5KB */
    test_buffer = g_speed_test_buffer;
    
    /* 准备测试数据 */
    PrepareTestData(test_buffer, buffer_size);
    
    /* 循环测试所有分频 */
    for (test_index = 0; test_index < PRESCALER_COUNT; test_index++)
    {
        uint16_t prescaler = g_prescalers[test_index];
        uint16_t prescaler_value = GetPrescalerValue(prescaler);
        uint32_t block_idx;
        uint32_t blocks_processed = 0;
        
        LOG_INFO("MAIN", "--- 测试分频 %d (%d/%d) ---", prescaler_value, test_index + 1, PRESCALER_COUNT);
        
        /* 检查SD卡状态，如果未初始化则尝试重新初始化 */
        if (!g_device_info.is_initialized)
        {
            LOG_WARN("MAIN", "SD卡未初始化，尝试重新初始化...");
            /* 先恢复到初始化速度 */
            ChangeSPIPrescaler(SPI_BaudRatePrescaler_256);
            Delay_ms(10);
            
            if (ManualInitTF() != 0)
            {
                LOG_WARN("MAIN", "SD卡重新初始化失败，跳过此分频测试");
                g_speed_test_results[test_index].write_time_ms = 0;
                g_speed_test_results[test_index].read_time_ms = 0;
                g_speed_test_results[test_index].write_speed_kbps = 0.0f;
                g_speed_test_results[test_index].read_speed_kbps = 0.0f;
                continue;  /* 跳过此分频，继续下一个 */
            }
            else
            {
                LOG_INFO("MAIN", "SD卡重新初始化成功，继续测试");
            }
        }
        
        /* 修改SPI分频 */
        spi_status = ChangeSPIPrescaler(prescaler);
        if (spi_status != SPI_OK)
        {
            LOG_ERROR("MAIN", "修改SPI分频失败: %d", spi_status);
            continue;
        }
        
        /* 分频修改后，等待SPI总线稳定，并发送一些dummy字节同步 */
        Delay_ms(10);
        
        /* 先测试写入1块数据，验证写入功能是否正常 */
        LOG_INFO("MAIN", "测试写入1块数据验证功能...");
        PrepareTestData(test_buffer, 512);
        if (ManualWriteBlocks(test_start_block, 1, test_buffer) != 0)
        {
            LOG_ERROR("MAIN", "测试写入失败，块地址: %lu", test_start_block);
            LOG_ERROR("MAIN", "分频 %d 可能太快，跳过此分频", prescaler_value);
            g_speed_test_results[test_index].write_time_ms = 0;
            g_speed_test_results[test_index].read_time_ms = 0;
            g_speed_test_results[test_index].write_speed_kbps = 0.0f;
            g_speed_test_results[test_index].read_speed_kbps = 0.0f;
            continue;
        }
        LOG_INFO("MAIN", "测试写入成功，开始正式写入测试");
        
        /* 写入测试 */
        DisplaySpeedTestStatus(prescaler_value, test_index, PRESCALER_COUNT, "Write");
        LOG_INFO("MAIN", "开始写入测试...");
        LOG_INFO("MAIN", "测试起始块: %lu, 总块数: %lu", test_start_block, SPEED_TEST_BLOCK_COUNT);
        
        start_time = Delay_GetTick();
        
        /* 进度显示变量（每个分频测试开始时重置） */
        uint8_t last_log_percent_write = 0;
        uint8_t last_oled_percent_write = 0;
        
        /* 分块写入（每次32块，约16KB，提高传输效率） */
        for (block_idx = 0; block_idx < SPEED_TEST_BLOCK_COUNT; block_idx += 32)
        {
            uint32_t blocks_to_write = (SPEED_TEST_BLOCK_COUNT - block_idx > 32) ? 32 : (SPEED_TEST_BLOCK_COUNT - block_idx);
            uint32_t current_block = test_start_block + block_idx;
            
            /* 更新测试数据（包含块索引信息） */
            PrepareTestData(test_buffer, blocks_to_write * 512);
            
            /* 写入前检查SD卡状态 */
            if (!g_device_info.is_initialized)
            {
                LOG_WARN("MAIN", "写入过程中检测到SD卡拔出，跳过此分频测试");
                g_speed_test_results[test_index].write_time_ms = 0;
                g_speed_test_results[test_index].read_time_ms = 0;
                g_speed_test_results[test_index].write_speed_kbps = 0.0f;
                g_speed_test_results[test_index].read_speed_kbps = 0.0f;
                break;  /* 跳出写入循环，继续下一个分频 */
            }
            
            if (ManualWriteBlocks(current_block, blocks_to_write, test_buffer) != 0)
            {
                LOG_ERROR("MAIN", "写入失败，块地址: %lu", current_block);
                LOG_WARN("MAIN", "跳过此分频的写入测试，继续下一个分频");
                g_speed_test_results[test_index].write_time_ms = 0;
                g_speed_test_results[test_index].read_time_ms = 0;
                g_speed_test_results[test_index].write_speed_kbps = 0.0f;
                g_speed_test_results[test_index].read_speed_kbps = 0.0f;
                break;  /* 跳出写入循环，继续下一个分频 */
            }
            
            blocks_processed += blocks_to_write;
            
            /* 按10%输出进度和更新OLED（基于百分比判断，而不是取模） */
            {
                uint8_t current_percent = (blocks_processed * 100) / SPEED_TEST_BLOCK_COUNT;
                
                /* 串口日志：每10%输出一次 */
                if (current_percent >= last_log_percent_write + 10 || blocks_processed >= SPEED_TEST_BLOCK_COUNT)
                {
                    LOG_INFO("MAIN", "写入进度: %lu/%lu 块 (%d%%)", 
                             blocks_processed, SPEED_TEST_BLOCK_COUNT, current_percent);
                    last_log_percent_write = (current_percent / 10) * 10;  /* 向下取整到10的倍数 */
                }
                
                /* OLED显示：每20%更新一次 */
                if (current_percent >= last_oled_percent_write + 20 || blocks_processed >= SPEED_TEST_BLOCK_COUNT)
                {
                    char buf[17];
                    snprintf(buf, sizeof(buf), "Write: %d%%", current_percent);
                    OLED_ShowString(4, 1, buf);
                    last_oled_percent_write = (current_percent / 20) * 20;  /* 向下取整到20的倍数 */
                }
            }
        }
        
        end_time = Delay_GetTick();
        g_speed_test_results[test_index].write_time_ms = Delay_GetElapsed(end_time, start_time);
        g_speed_test_results[test_index].prescaler_value = prescaler_value;
        g_speed_test_results[test_index].write_speed_kbps = CalculateSpeed(SPEED_TEST_SIZE_BYTES, 
                                                                           g_speed_test_results[test_index].write_time_ms);
        
        LOG_INFO("MAIN", "写入完成，耗时: %lu ms, 速度: %.2f KB/s", 
                 g_speed_test_results[test_index].write_time_ms,
                 g_speed_test_results[test_index].write_speed_kbps);
        
        Delay_ms(500);
        
        /* 读取测试 */
        DisplaySpeedTestStatus(prescaler_value, test_index, PRESCALER_COUNT, "Read");
        LOG_INFO("MAIN", "开始读取测试...");
        
        start_time = Delay_GetTick();
        blocks_processed = 0;
        
        /* 进度显示变量（每个分频测试开始时重置） */
        uint8_t last_log_percent_read = 0;
        uint8_t last_oled_percent_read = 0;
        
        /* 分块读取（每次32块，约16KB，提高传输效率） */
        for (block_idx = 0; block_idx < SPEED_TEST_BLOCK_COUNT; block_idx += 32)
        {
            uint32_t blocks_to_read = (SPEED_TEST_BLOCK_COUNT - block_idx > 32) ? 32 : (SPEED_TEST_BLOCK_COUNT - block_idx);
            uint32_t current_block = test_start_block + block_idx;
            
            /* 读取前检查SD卡状态 */
            if (!g_device_info.is_initialized)
            {
                LOG_WARN("MAIN", "读取过程中检测到SD卡拔出，跳过此分频测试");
                g_speed_test_results[test_index].read_time_ms = 0;
                g_speed_test_results[test_index].read_speed_kbps = 0;
                break;  /* 跳出读取循环，继续下一个分频 */
            }
            
            if (ManualReadBlocks(current_block, blocks_to_read, test_buffer) != 0)
            {
                LOG_ERROR("MAIN", "读取失败，块地址: %lu", current_block);
                LOG_WARN("MAIN", "跳过该分频的读取测试，继续下一个分频");
                /* 标记读取失败，但不退出整个测试 */
                g_speed_test_results[test_index].read_time_ms = 0;
                g_speed_test_results[test_index].read_speed_kbps = 0;
                break;  /* 跳出读取循环，继续下一个分频测试 */
            }
            
            blocks_processed += blocks_to_read;
            
            /* 按10%输出进度和更新OLED（基于百分比判断，而不是取模） */
            {
                uint8_t current_percent = (blocks_processed * 100) / SPEED_TEST_BLOCK_COUNT;
                
                /* 串口日志：每10%输出一次 */
                if (current_percent >= last_log_percent_read + 10 || blocks_processed >= SPEED_TEST_BLOCK_COUNT)
                {
                    LOG_INFO("MAIN", "读取进度: %lu/%lu 块 (%d%%)", 
                             blocks_processed, SPEED_TEST_BLOCK_COUNT, current_percent);
                    last_log_percent_read = (current_percent / 10) * 10;  /* 向下取整到10的倍数 */
                }
                
                /* OLED显示：每20%更新一次 */
                if (current_percent >= last_oled_percent_read + 20 || blocks_processed >= SPEED_TEST_BLOCK_COUNT)
                {
                    char buf[17];
                    snprintf(buf, sizeof(buf), "Read: %d%%", current_percent);
                    OLED_ShowString(4, 1, buf);
                    last_oled_percent_read = (current_percent / 20) * 20;  /* 向下取整到20的倍数 */
                }
            }
        }
        
        end_time = Delay_GetTick();
        g_speed_test_results[test_index].read_time_ms = Delay_GetElapsed(end_time, start_time);
        g_speed_test_results[test_index].read_speed_kbps = CalculateSpeed(SPEED_TEST_SIZE_BYTES, 
                                                                         g_speed_test_results[test_index].read_time_ms);
        
        LOG_INFO("MAIN", "读取完成，耗时: %lu ms, 速度: %.2f KB/s", 
                 g_speed_test_results[test_index].read_time_ms,
                 g_speed_test_results[test_index].read_speed_kbps);
        
        Delay_ms(500);
    }
    
    /* 输出速度对比表 */
    LOG_INFO("MAIN", "");
    LOG_INFO("MAIN", "=== 速度测试结果对比表 ===");
    LOG_INFO("MAIN", "分频 | 写入时间(ms) | 写入速度(KB/s) | 读取时间(ms) | 读取速度(KB/s)");
    LOG_INFO("MAIN", "-----|--------------|---------------|--------------|---------------");
    for (test_index = 0; test_index < PRESCALER_COUNT; test_index++)
    {
        LOG_INFO("MAIN", "  %2d  |   %8lu   |   %10.2f   |   %8lu   |   %10.2f",
                 g_speed_test_results[test_index].prescaler_value,
                 g_speed_test_results[test_index].write_time_ms,
                 g_speed_test_results[test_index].write_speed_kbps,
                 g_speed_test_results[test_index].read_time_ms,
                 g_speed_test_results[test_index].read_speed_kbps);
    }
    
    /* 查找最快和最慢的分频 */
    {
        uint8_t fastest_write_idx = 0, fastest_read_idx = 0;
        uint8_t slowest_write_idx = 0, slowest_read_idx = 0;
        
        for (test_index = 1; test_index < PRESCALER_COUNT; test_index++)
        {
            if (g_speed_test_results[test_index].write_speed_kbps > 
                g_speed_test_results[fastest_write_idx].write_speed_kbps)
            {
                fastest_write_idx = test_index;
            }
            if (g_speed_test_results[test_index].write_speed_kbps < 
                g_speed_test_results[slowest_write_idx].write_speed_kbps)
            {
                slowest_write_idx = test_index;
            }
            if (g_speed_test_results[test_index].read_speed_kbps > 
                g_speed_test_results[fastest_read_idx].read_speed_kbps)
            {
                fastest_read_idx = test_index;
            }
            if (g_speed_test_results[test_index].read_speed_kbps < 
                g_speed_test_results[slowest_read_idx].read_speed_kbps)
            {
                slowest_read_idx = test_index;
            }
        }
        
        LOG_INFO("MAIN", "");
        LOG_INFO("MAIN", "最快写入: 分频 %d, 速度 %.2f KB/s", 
                 g_speed_test_results[fastest_write_idx].prescaler_value,
                 g_speed_test_results[fastest_write_idx].write_speed_kbps);
        LOG_INFO("MAIN", "最慢写入: 分频 %d, 速度 %.2f KB/s", 
                 g_speed_test_results[slowest_write_idx].prescaler_value,
                 g_speed_test_results[slowest_write_idx].write_speed_kbps);
        LOG_INFO("MAIN", "最快读取: 分频 %d, 速度 %.2f KB/s", 
                 g_speed_test_results[fastest_read_idx].prescaler_value,
                 g_speed_test_results[fastest_read_idx].read_speed_kbps);
        LOG_INFO("MAIN", "最慢读取: 分频 %d, 速度 %.2f KB/s", 
                 g_speed_test_results[slowest_read_idx].prescaler_value,
                 g_speed_test_results[slowest_read_idx].read_speed_kbps);
    }
    
    /* 测速测试完成后，恢复SPI分频到8分频（增量写入使用的分频） */
    LOG_INFO("MAIN", "测速测试完成，恢复SPI到8分频（增量写入速度）");
    spi_status = ChangeSPIPrescaler(INCREMENTAL_WRITE_PRESCALER);
    if (spi_status != SPI_OK)
    {
        LOG_WARN("MAIN", "恢复SPI分频失败: %d", spi_status);
    }
    else
    {
        Delay_ms(10);  /* 等待SPI总线稳定 */
        LOG_INFO("MAIN", "SPI已恢复到8分频（4.5MHz）");
    }
    
    /* 检查SD卡状态，如果异常则尝试重新初始化 */
    if (!g_device_info.is_initialized)
    {
        LOG_WARN("MAIN", "测速测试后SD卡状态异常，尝试重新初始化...");
        /* 先恢复到初始化速度 */
        ChangeSPIPrescaler(SPI_BaudRatePrescaler_256);
        Delay_ms(10);
        
        if (ManualInitTF() == 0)
        {
            LOG_INFO("MAIN", "SD卡重新初始化成功");
            /* 恢复回8分频 */
            ChangeSPIPrescaler(INCREMENTAL_WRITE_PRESCALER);
            Delay_ms(10);
        }
        else
        {
            LOG_WARN("MAIN", "SD卡重新初始化失败");
        }
    }
    
    /* OLED显示结果 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Speed Test OK");
    OLED_ShowString(2, 1, "See UART Log");
    OLED_ShowString(3, 1, "For Details");
    
    return 1;
}

/* ==================== 演示3：增量写入功能 ==================== */

/**
 * @brief 执行增量写入（写入100KB数据，使用8分频）
 * @return uint8_t 1-成功，0-失败
 */
static uint8_t PerformIncrementalWrite(void)
{
    SPI_Status_t spi_status;
    uint32_t i;
    uint8_t *write_buffer = g_speed_test_buffer;  /* 使用全局缓冲区 */
    uint32_t buffer_size_blocks = sizeof(g_speed_test_buffer) / 512;  /* 约10块 */
    uint32_t start_time, end_time;
    
    /* 检查初始化状态 */
    if (!g_device_info.is_initialized)
    {
        LOG_WARN("MAIN", "TF卡未初始化，尝试重新初始化...");
        
        /* 先恢复到初始化速度 */
        spi_status = ChangeSPIPrescaler(SPI_BaudRatePrescaler_256);
        if (spi_status == SPI_OK)
        {
            Delay_ms(10);
            if (ManualInitTF() == 0)
            {
                LOG_INFO("MAIN", "TF卡重新初始化成功");
                /* 恢复回8分频 */
                ChangeSPIPrescaler(INCREMENTAL_WRITE_PRESCALER);
                Delay_ms(10);
            }
            else
            {
                LOG_WARN("MAIN", "TF卡重新初始化失败");
                return 0;
            }
        }
        else
        {
            LOG_WARN("MAIN", "恢复SPI分频失败: %d", spi_status);
            return 0;
        }
    }
    
    /* 检查是否达到最大写入次数 */
    if (g_incremental_write_state.write_count >= INCREMENTAL_WRITE_MAX_COUNT)
    {
        LOG_INFO("MAIN", "已达到最大写入次数 %d 次，增量写入完成", INCREMENTAL_WRITE_MAX_COUNT);
        /* 禁用增量写入功能，避免主循环继续尝试 */
        g_incremental_write_state.initialized = 0;
        return 0;
    }
    
    /* 检查容量是否足够 */
    if (g_incremental_write_state.current_block + INCREMENTAL_WRITE_BLOCK_COUNT > g_device_info.block_count)
    {
        LOG_WARN("MAIN", "SD卡容量不足，增量写入已满");
        return 0;
    }
    
    LOG_INFO("MAIN", "=== 增量写入：写入100KB数据（第 %lu/%d 次） ===", 
             g_incremental_write_state.write_count + 1, INCREMENTAL_WRITE_MAX_COUNT);
    LOG_INFO("MAIN", "写入块地址: %lu - %lu", 
             g_incremental_write_state.current_block,
             g_incremental_write_state.current_block + INCREMENTAL_WRITE_BLOCK_COUNT - 1);
    
    /* 切换到8分频（标准常用速度） */
    LOG_INFO("MAIN", "切换到8分频（4.5MHz）进行增量写入");
    spi_status = ChangeSPIPrescaler(INCREMENTAL_WRITE_PRESCALER);
    if (spi_status != SPI_OK)
    {
        LOG_ERROR("MAIN", "修改SPI分频失败: %d", spi_status);
        return 0;
    }
    Delay_ms(10);  /* 等待SPI总线稳定 */
    
    start_time = Delay_GetTick();
    
    /* 写入100KB数据（200块），使用多块写入 */
    uint8_t last_log_percent = 0;
    uint8_t last_oled_percent = 0;
    
    /* 使用多块写入（每次写入最多buffer_size_blocks块，提高效率） */
    for (i = 0; i < INCREMENTAL_WRITE_BLOCK_COUNT; i += buffer_size_blocks)
    {
        uint32_t blocks_to_write = (INCREMENTAL_WRITE_BLOCK_COUNT - i > buffer_size_blocks) ? 
                                    buffer_size_blocks : (INCREMENTAL_WRITE_BLOCK_COUNT - i);
        uint32_t current_block = g_incremental_write_state.current_block + i;
        
        /* 准备数据：包含块序号、时间戳、递增序列 */
        memset(write_buffer, 0, blocks_to_write * 512);
        for (uint32_t j = 0; j < blocks_to_write; j++)
        {
            uint32_t block_idx = i + j;
            uint8_t *block_buf = write_buffer + (j * 512);
            
            *((uint32_t*)&block_buf[0]) = g_incremental_write_state.write_count;  /* 写入次数 */
            *((uint32_t*)&block_buf[4]) = Delay_GetTick();  /* 时间戳 */
            *((uint32_t*)&block_buf[8]) = current_block + j;  /* 块地址 */
            *((uint32_t*)&block_buf[12]) = block_idx;  /* 块内序号 */
            
            /* 填充递增序列 */
            for (uint32_t k = 16; k < 512; k++)
            {
                block_buf[k] = (uint8_t)((current_block + j + k) & 0xFF);
            }
        }
        
        /* 执行多块写入 */
        if (ManualWriteBlocks(current_block, blocks_to_write, write_buffer) != 0)
        {
            LOG_ERROR("MAIN", "写入失败，块地址: %lu", current_block);
            
            /* 写入失败可能是SD卡状态异常，尝试重新初始化 */
            LOG_WARN("MAIN", "检测到SD卡通信异常，尝试清除状态并重新初始化...");
            
            /* 先清除初始化状态，避免状态不一致 */
            g_device_info.is_initialized = 0;
            memset(&g_device_info, 0, sizeof(g_device_info));
            
            /* 先恢复到初始化速度 */
            ChangeSPIPrescaler(SPI_BaudRatePrescaler_256);
            Delay_ms(10);
            
            if (ManualInitTF() == 0)
            {
                LOG_INFO("MAIN", "SD卡重新初始化成功，但本次写入已失败");
                /* 恢复回8分频 */
                ChangeSPIPrescaler(INCREMENTAL_WRITE_PRESCALER);
                Delay_ms(10);
            }
            else
            {
                LOG_WARN("MAIN", "SD卡重新初始化失败，可能卡已拔出");
            }
            
            return 0;
        }
        
        /* 进度显示：每10%输出一次串口日志，每20%更新一次OLED */
        {
            uint32_t blocks_written = i + blocks_to_write;
            uint8_t current_percent = (blocks_written * 100) / INCREMENTAL_WRITE_BLOCK_COUNT;
            
            /* 串口日志：每10%输出一次 */
            if (current_percent >= last_log_percent + 10 || blocks_written >= INCREMENTAL_WRITE_BLOCK_COUNT)
            {
                LOG_INFO("MAIN", "写入进度: %lu/%lu 块 (%d%%)",
                         blocks_written, INCREMENTAL_WRITE_BLOCK_COUNT, current_percent);
                last_log_percent = (current_percent / 10) * 10;  /* 向下取整到10的倍数 */
            }
            
            /* OLED显示：每20%更新一次 */
            if (current_percent >= last_oled_percent + 20 || blocks_written >= INCREMENTAL_WRITE_BLOCK_COUNT)
            {
                char buf[17];
                snprintf(buf, sizeof(buf), "Write: %d%%", current_percent);
                OLED_ShowString(4, 1, buf);
                last_oled_percent = (current_percent / 20) * 20;  /* 向下取整到20的倍数 */
            }
        }
    }
    
    end_time = Delay_GetTick();
    uint32_t write_time_ms = Delay_GetElapsed(end_time, start_time);
    
    /* 更新状态 */
    g_incremental_write_state.current_block += INCREMENTAL_WRITE_BLOCK_COUNT;
    g_incremental_write_state.write_count++;
    g_incremental_write_state.last_write_time_ms = Delay_GetTick();
    
    LOG_INFO("MAIN", "写入完成，耗时: %lu ms, 写入次数: %lu", 
             write_time_ms, g_incremental_write_state.write_count);
    {
        uint32_t total_size_kb = (g_incremental_write_state.current_block - INCREMENTAL_WRITE_START_BLOCK) * 512 / 1024;
        LOG_INFO("MAIN", "当前数据容量: %lu KB (%.2f MB)", 
                 total_size_kb, total_size_kb / 1024.0f);
    }
    
    /* OLED显示 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Incr Write OK");
    char buf[17];
    snprintf(buf, sizeof(buf), "Count: %lu", g_incremental_write_state.write_count);
    OLED_ShowString(2, 1, buf);
    {
        uint32_t total_size_kb = (g_incremental_write_state.current_block - INCREMENTAL_WRITE_START_BLOCK) * 512 / 1024;
        snprintf(buf, sizeof(buf), "Size: %lu KB", total_size_kb);
        OLED_ShowString(3, 1, buf);
    }
    
    return 1;
}

/**
 * @brief 读取并校验所有已写入的数据
 * @return uint8_t 1-校验通过，0-校验失败
 */
static uint8_t VerifyIncrementalData(void)
{
    SPI_Status_t spi_status;
    uint8_t *read_buffer = g_speed_test_buffer;  /* 使用全局缓冲区 */
    uint32_t buffer_size_blocks = sizeof(g_speed_test_buffer) / 512;  /* 约10块 */
    uint32_t total_blocks;
    uint32_t i;
    uint32_t error_count = 0;
    uint32_t start_time, end_time;
    
    if (!g_device_info.is_initialized)
    {
        LOG_WARN("MAIN", "TF卡未初始化，跳过数据校验");
        return 0;
    }
    
    if (g_incremental_write_state.write_count == 0)
    {
        LOG_INFO("MAIN", "尚未写入数据，跳过校验");
        return 1;
    }
    
    total_blocks = g_incremental_write_state.current_block - INCREMENTAL_WRITE_START_BLOCK;
    
    LOG_INFO("MAIN", "=== 读取并校验所有已写入数据 ===");
    LOG_INFO("MAIN", "总块数: %lu", total_blocks);
    
    /* 切换到8分频（标准常用速度） */
    LOG_INFO("MAIN", "使用8分频（4.5MHz）进行数据校验");
    spi_status = ChangeSPIPrescaler(INCREMENTAL_WRITE_PRESCALER);
    if (spi_status != SPI_OK)
    {
        LOG_ERROR("MAIN", "修改SPI分频失败: %d", spi_status);
        return 0;
    }
    Delay_ms(10);  /* 等待SPI总线稳定 */
    
    start_time = Delay_GetTick();
    
    /* 进度显示变量 */
    uint8_t last_log_percent = 0;
    uint8_t last_oled_percent = 0;
    
    /* 读取所有块并校验（使用多块读取提高效率） */
    for (i = 0; i < total_blocks; i += buffer_size_blocks)
    {
        uint32_t blocks_to_read = (total_blocks - i > buffer_size_blocks) ? 
                                  buffer_size_blocks : (total_blocks - i);
        uint32_t current_block = INCREMENTAL_WRITE_START_BLOCK + i;
        
        /* 读取块 */
        if (ManualReadBlocks(current_block, blocks_to_read, read_buffer) != 0)
        {
            LOG_ERROR("MAIN", "读取失败，块地址: %lu", current_block);
            
            /* 读取失败可能是SD卡状态异常，尝试重新初始化 */
            LOG_WARN("MAIN", "检测到SD卡通信异常，尝试重新初始化...");
            
            /* 先恢复到初始化速度 */
            ChangeSPIPrescaler(SPI_BaudRatePrescaler_256);
            Delay_ms(10);
            
            if (ManualInitTF() == 0)
            {
                LOG_INFO("MAIN", "SD卡重新初始化成功，但本次校验已失败");
                /* 恢复回8分频 */
                ChangeSPIPrescaler(INCREMENTAL_WRITE_PRESCALER);
                Delay_ms(10);
            }
            else
            {
                LOG_WARN("MAIN", "SD卡重新初始化失败");
            }
            
            error_count += blocks_to_read;
            continue;
        }
        
        /* 校验每个块的数据 */
        for (uint32_t j = 0; j < blocks_to_read; j++)
        {
            uint32_t block_idx = i + j;
            uint8_t *block_buf = read_buffer + (j * 512);
            uint32_t expected_block_addr;
            uint32_t expected_block_idx;
            
            /* 解析数据头 */
            expected_block_addr = *((uint32_t*)&block_buf[8]);
            expected_block_idx = *((uint32_t*)&block_buf[12]);
            
            /* 校验块地址 */
            if (expected_block_addr != (current_block + j))
            {
                if (error_count < 5)  /* 只记录前5个错误 */
                {
                    LOG_ERROR("MAIN", "块地址不匹配，块 %lu: 期望=%lu, 读取=%lu", 
                             block_idx, current_block + j, expected_block_addr);
                }
                error_count++;
            }
            
            /* 校验块内序号 */
            if (expected_block_idx != (block_idx % INCREMENTAL_WRITE_BLOCK_COUNT))
            {
                if (error_count < 5)
                {
                    LOG_ERROR("MAIN", "块内序号不匹配，块 %lu: 期望=%lu, 读取=%lu", 
                             block_idx, block_idx % INCREMENTAL_WRITE_BLOCK_COUNT, expected_block_idx);
                }
                error_count++;
            }
            
            /* 校验数据内容（检查递增序列） */
            for (uint32_t k = 16; k < 512; k++)
            {
                uint8_t expected = (uint8_t)((current_block + j + k) & 0xFF);
                if (block_buf[k] != expected)
                {
                    if (error_count < 5)
                    {
                        LOG_ERROR("MAIN", "数据不匹配，块 %lu, 偏移 %lu: 期望=0x%02X, 读取=0x%02X", 
                                 block_idx, k, expected, block_buf[k]);
                    }
                    error_count++;
                    break;  /* 每个块只记录第一个错误 */
                }
            }
        }
        
        /* 进度显示：每10%输出一次串口日志，每20%更新一次OLED */
        {
            uint32_t blocks_read = i + blocks_to_read;
            uint8_t current_percent = (blocks_read * 100) / total_blocks;
            
            /* 串口日志：每10%输出一次 */
            if (current_percent >= last_log_percent + 10 || blocks_read >= total_blocks)
            {
                LOG_INFO("MAIN", "校验进度: %lu/%lu 块 (%d%%)",
                         blocks_read, total_blocks, current_percent);
                last_log_percent = (current_percent / 10) * 10;  /* 向下取整到10的倍数 */
            }
            
            /* OLED显示：每20%更新一次 */
            if (current_percent >= last_oled_percent + 20 || blocks_read >= total_blocks)
            {
                char buf[17];
                snprintf(buf, sizeof(buf), "Verify: %d%%", current_percent);
                OLED_ShowString(4, 1, buf);
                last_oled_percent = (current_percent / 20) * 20;  /* 向下取整到20的倍数 */
            }
        }
    }
    
    end_time = Delay_GetTick();
    uint32_t verify_time_ms = Delay_GetElapsed(end_time, start_time);
    
    if (error_count == 0)
    {
        LOG_INFO("MAIN", "数据校验通过，总块数: %lu, 耗时: %lu ms", total_blocks, verify_time_ms);
        
        /* OLED显示 */
        OLED_Clear();
        OLED_ShowString(1, 1, "Verify OK");
        char buf[17];
        snprintf(buf, sizeof(buf), "Blocks: %lu", total_blocks);
        OLED_ShowString(2, 1, buf);
        OLED_ShowString(3, 1, "No Errors");
        
        return 1;
    }
    else
    {
        LOG_ERROR("MAIN", "数据校验失败，错误块数: %lu/%lu", error_count, total_blocks);
        
        /* OLED显示 */
        OLED_Clear();
        OLED_ShowString(1, 1, "Verify Failed");
        char buf[17];
        snprintf(buf, sizeof(buf), "Errors: %lu", error_count);
        OLED_ShowString(2, 1, buf);
        
        return 0;
    }
}

/* ==================== 插拔卡处理 ==================== */

/**
 * @brief 检测并处理插拔卡
 * @return uint8_t 1-卡存在且已初始化，0-卡不存在或未初始化
 */
static uint8_t DetectAndHandleCard(void)
{
    uint32_t current_time = Delay_GetTick();
    uint32_t elapsed = Delay_GetElapsed(current_time, g_card_detect_state.last_detect_time_ms);
    uint8_t current_init_status;
    
    /* 检查是否到了检测时间 */
    if (elapsed < CARD_DETECT_INTERVAL_MS)
    {
        return g_card_detect_state.card_present;  /* 返回上次状态 */
    }
    
    g_card_detect_state.last_detect_time_ms = current_time;
    
    /* 检查初始化状态 */
    current_init_status = g_device_info.is_initialized;
    
    /* 如果状态发生变化 */
    if (current_init_status != g_card_detect_state.last_init_status)
    {
        if (current_init_status)
        {
            /* 从未初始化变为已初始化：卡插入 */
            LOG_INFO("MAIN", "检测到SD卡插入");
            g_card_detect_state.card_present = 1;
            
            /* OLED显示 */
            OLED_Clear();
            OLED_ShowString(1, 1, "Card Inserted");
            OLED_ShowString(2, 1, "Initialized");
            Delay_ms(1000);
        }
        else
        {
            /* 从已初始化变为未初始化：卡拔出 */
            LOG_WARN("MAIN", "检测到SD卡拔出");
            g_card_detect_state.card_present = 0;
            
            /* OLED显示 */
            OLED_Clear();
            OLED_ShowString(1, 1, "Card Removed");
            Delay_ms(1000);
        }
    }
    else if (!current_init_status)
    {
        /* 如果未初始化，尝试重新初始化 */
        LOG_INFO("MAIN", "尝试重新初始化SD卡...");
        
        /* 重要：重新初始化前，先将SPI恢复到初始化速度（256分频，≤400kHz） */
        /* 因为增量写入可能已经切换到8分频（4.5MHz），而初始化需要低速 */
        SPI_Status_t spi_status = ChangeSPIPrescaler(SPI_BaudRatePrescaler_256);
        if (spi_status != SPI_OK)
        {
            LOG_WARN("MAIN", "恢复SPI分频失败: %d", spi_status);
        }
        else
        {
            LOG_INFO("MAIN", "已恢复SPI到256分频（初始化速度）");
            Delay_ms(10);  /* 等待SPI总线稳定 */
        }
        
        if (ManualInitTF() == 0)
        {
            LOG_INFO("MAIN", "SD卡重新初始化成功");
            g_card_detect_state.card_present = 1;
            g_card_detect_state.last_init_status = 1;
            
            /* OLED显示 */
            OLED_Clear();
            OLED_ShowString(1, 1, "Card Re-Init");
            OLED_ShowString(2, 1, "Success");
            Delay_ms(1000);
        }
        else
        {
            LOG_WARN("MAIN", "SD卡重新初始化失败");
            LOG_WARN("MAIN", "可能原因：1.卡未插入 2.MISO上拉电阻 3.SPI速度过快");
            g_card_detect_state.card_present = 0;
        }
    }
    else
    {
        /* 已初始化，卡存在 */
        g_card_detect_state.card_present = 1;
    }
    
    g_card_detect_state.last_init_status = current_init_status;
    
    return g_card_detect_state.card_present;
}

/* ==================== 主函数 ==================== */

int main(void)
{
    SPI_Status_t spi_status;
    SoftI2C_Status_t i2c_status;
    UART_Status_t uart_status;
    OLED_Status_t oled_status;
    Log_Status_t log_status;
    int debug_status;
    log_config_t log_config;
    uint32_t last_incremental_write_time = 0;
    
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
    LOG_INFO("MAIN", "=== TF卡（MicroSD卡）手动初始化与多块传输测速示例 ===");
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
        OLED_ShowString(1, 1, "TF Manual Init");
        OLED_ShowString(2, 1, "Initializing...");
        LOG_INFO("MAIN", "OLED 已初始化");
    }
    
    Delay_ms(500);
    
    /* ========== 步骤9：SPI初始化 ========== */
    OLED_ShowString(3, 1, "Init SPI2...");
    
    /* 手动配置PA11为GPIO输出（软件NSS模式） */
    GPIO_EnableClock(GPIOA);
    GPIO_Config(GPIOA, GPIO_Pin_11, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHz);
    GPIO_WritePin(GPIOA, GPIO_Pin_11, Bit_SET);  /* NSS默认拉高（不选中） */
    
    spi_status = SPI_HW_Init(SPI_INSTANCE_2);
    if (spi_status != SPI_OK)
    {
        char err_buf[17];
        snprintf(err_buf, sizeof(err_buf), "SPI Fail:%d", spi_status);
        OLED_ShowString(4, 1, err_buf);
        LOG_ERROR("MAIN", "SPI2 初始化失败: %d", spi_status);
        ErrorHandler_Handle(spi_status, "SPI");
        while(1) { Delay_ms(1000); }
    }
    else
    {
        OLED_ShowString(4, 1, "SPI2: OK");
        LOG_INFO("MAIN", "SPI2 已初始化: PB13(SCK), PB14(MISO), PB15(MOSI), PA11(CS)");
    }
    
    Delay_ms(500);
    
    /* ========== 步骤10：TF卡手动初始化 ========== */
    OLED_Clear();
    OLED_ShowString(1, 1, "TF Card Init");
    Delay_ms(500);
    
    LOG_INFO("MAIN", "=== TF卡手动初始化 ===");
    
    if (ManualInitTF() == 0)
    {
        OLED_ShowString(2, 1, "Init: OK");
        LOG_INFO("MAIN", "ManualInitTF()成功！");
        
        char buf[17];
        snprintf(buf, sizeof(buf), "Cap: %d MB", g_device_info.capacity_mb);
        OLED_ShowString(3, 1, buf);
        
        LOG_INFO("MAIN", "SD卡信息：");
        LOG_INFO("MAIN", "  容量: %d MB", g_device_info.capacity_mb);
        LOG_INFO("MAIN", "  块大小: %d 字节", g_device_info.block_size);
        LOG_INFO("MAIN", "  块数量: %d", g_device_info.block_count);
        LOG_INFO("MAIN", "  卡类型: %s", 
                 g_device_info.is_sdhc ? "SDHC/SDXC" : "SDSC");
        
        g_card_detect_state.card_present = 1;
        g_card_detect_state.last_init_status = 1;
    }
    else
    {
        OLED_ShowString(2, 1, "Init: Failed");
        char err_buf[17];
        snprintf(err_buf, sizeof(err_buf), "Error");
        OLED_ShowString(3, 1, err_buf);
        LOG_ERROR("MAIN", "ManualInitTF()失败");
        LOG_ERROR("MAIN", "请检查SD卡是否插入");
        
        g_card_detect_state.card_present = 0;
        g_card_detect_state.last_init_status = 0;
    }
    
    Delay_ms(2000);
    
    /* ========== 步骤11：执行演示1（手动初始化演示） ========== */
    DemoManualInit();
    Delay_ms(2000);
    
    /* ========== 步骤12：执行演示2（测速测试） ========== */
    if (g_device_info.is_initialized)
    {
        PerformSpeedTest();
        Delay_ms(500);  /* 减少延迟，快速进入主循环 */
    }
    else
    {
        LOG_WARN("MAIN", "TF卡未初始化，跳过测速测试");
    }
    
    /* ========== 步骤13：初始化增量写入状态 ========== */
    g_incremental_write_state.initialized = 1;
    /* 设置为当前时间减去间隔时间，这样进入主循环后会立即执行一次增量写入 */
    last_incremental_write_time = Delay_GetTick() - INCREMENTAL_WRITE_INTERVAL_MS;
    
    LOG_INFO("MAIN", "=== 进入主循环 ===");
    LOG_INFO("MAIN", "增量写入模式：每5秒写入100KB，使用8分频（4.5MHz），自动校验");
    LOG_INFO("MAIN", "最大写入次数：%d 次（便于测试插拔卡功能）", INCREMENTAL_WRITE_MAX_COUNT);
    LOG_INFO("MAIN", "插拔卡检测：每5秒检测一次");
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Running...");
    OLED_ShowString(2, 1, "Incr Write");
    OLED_ShowString(3, 1, "Mode Active");
    
    /* ========== 步骤14：主循环 ========== */
    while (1)
    {
        uint32_t current_time = Delay_GetTick();
        uint32_t elapsed;
        
        /* 插拔卡检测 */
        DetectAndHandleCard();
        
        /* 增量写入处理 */
        if (g_device_info.is_initialized && g_incremental_write_state.initialized)
        {
            /* 检查是否已达到最大写入次数 */
            if (g_incremental_write_state.write_count >= INCREMENTAL_WRITE_MAX_COUNT)
            {
                /* 已达到最大次数，不再执行增量写入 */
                /* 这个检查已经在PerformIncrementalWrite中做了，但这里提前检查可以避免不必要的函数调用 */
            }
            else
            {
                elapsed = Delay_GetElapsed(current_time, last_incremental_write_time);
                
                if (elapsed >= INCREMENTAL_WRITE_INTERVAL_MS)
                {
                    /* 执行增量写入 */
                    if (PerformIncrementalWrite())
                    {
                        /* 读取并校验所有数据 */
                        VerifyIncrementalData();
                        
                        /* 检查是否已达到最大写入次数 */
                        if (g_incremental_write_state.write_count >= INCREMENTAL_WRITE_MAX_COUNT)
                        {
                            LOG_INFO("MAIN", "增量写入任务完成，已写入 %d 次，停止增量写入", INCREMENTAL_WRITE_MAX_COUNT);
                        }
                        else
                        {
                            /* 写入和校验完成后，等待5秒后继续下一次写入 */
                            LOG_INFO("MAIN", "本次写入完成，等待5秒后继续下一次写入");
                            Delay_ms(INCREMENTAL_WRITE_INTERVAL_MS);  /* 等待5秒 */
                        }
                    }
                    else
                    {
                        /* 增量写入失败，检查是否因为达到最大次数 */
                        if (g_incremental_write_state.write_count >= INCREMENTAL_WRITE_MAX_COUNT)
                        {
                            /* 已达到最大次数，不再重试 */
                            LOG_INFO("MAIN", "增量写入任务完成，已写入 %d 次", INCREMENTAL_WRITE_MAX_COUNT);
                        }
                        else
                        {
                            /* 增量写入失败，记录日志但不阻塞主循环 */
                            LOG_WARN("MAIN", "增量写入失败，将在下次循环时重试（如果SD卡已恢复）");
                            /* 失败时也等待一段时间，避免频繁重试 */
                            Delay_ms(1000);
                        }
                    }
                    
                    /* 无论成功或失败，都更新时间戳，避免立即重试 */
                    last_incremental_write_time = Delay_GetTick();
                }
            }
        }
        else if (!g_device_info.is_initialized && g_incremental_write_state.initialized)
        {
            /* SD卡未初始化，但增量写入已启用，等待插拔卡检测处理 */
            /* 插拔卡检测会尝试重新初始化 */
        }
        
        /* LED闪烁指示系统运行 */
        LED_Toggle(LED_1);
        Delay_ms(500);
    }
}

