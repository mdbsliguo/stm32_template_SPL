/**
 * @file tf_spi.c
 * @brief TF卡SPI驱动模块实现
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的TF卡（MicroSD卡）SPI驱动，实现SD卡SPI协议层
 * 
 * @note 设计原则：
 * - 只负责底层SD卡SPI协议，不包含文件系统相关代码
 * - 提供标准的块设备接口（扇区读写），便于与文件系统解耦
 * - 依赖SPI驱动模块，不直接操作SPI寄存器
 * - 支持SD卡v1.0和v2.0+（SDHC/SDXC）
 */

#include "tf_spi.h"
#include "config.h"
#include "delay.h"
#include <stddef.h>
#include <string.h>
#include <limits.h>

#if CONFIG_MODULE_LOG_ENABLED
#include "log.h"
/* 调试输出：默认禁用，需要调试时可启用 */
#ifndef TF_SPI_DEBUG_ENABLED
#define TF_SPI_DEBUG_ENABLED 0
#endif

#if TF_SPI_DEBUG_ENABLED
#define TF_SPI_LOG_DEBUG(fmt, ...) LOG_INFO("TF_SPI", fmt, ##__VA_ARGS__)
#else
#define TF_SPI_LOG_DEBUG(fmt, ...) ((void)0)
#endif
#else
#define TF_SPI_LOG_DEBUG(fmt, ...) ((void)0)
#endif

#if CONFIG_MODULE_TF_SPI_ENABLED

#if CONFIG_MODULE_SPI_ENABLED

/* ==================== 内部常量定义 ==================== */

/* SD卡SPI命令定义 */
#define TF_SPI_CMD_GO_IDLE_STATE       0x40    /**< CMD0：复位卡 */
#define TF_SPI_CMD_SEND_IF_COND        0x48    /**< CMD8：检查电压兼容性 */
#define TF_SPI_CMD_SEND_CSD            0x49    /**< CMD9：读取CSD寄存器 */
#define TF_SPI_CMD_SEND_CID            0x4A    /**< CMD10：读取CID寄存器 */
#define TF_SPI_CMD_STOP_TRANSMISSION   0x4C    /**< CMD12：停止传输 */
#define TF_SPI_CMD_SET_BLOCKLEN        0x50    /**< CMD16：设置块长度 */
#define TF_SPI_CMD_READ_SINGLE_BLOCK   0x51    /**< CMD17：读取单个块 */
#define TF_SPI_CMD_READ_MULTIPLE_BLOCK 0x52    /**< CMD18：读取多个块 */
#define TF_SPI_CMD_WRITE_BLOCK         0x58    /**< CMD24：写入单个块 */
#define TF_SPI_CMD_WRITE_MULTIPLE_BLOCK 0x59   /**< CMD25：写入多个块 */
#define TF_SPI_CMD_APP_CMD             0x77    /**< CMD55：应用命令前缀 */
#define TF_SPI_CMD_READ_OCR            0x7A    /**< CMD58：读取OCR寄存器 */
#define TF_SPI_ACMD_SD_SEND_OP_COND    0x69    /**< ACMD41：初始化SD卡 */

/* SD卡响应格式 */
#define TF_SPI_R1_IDLE_STATE           0x01    /**< R1响应：空闲状态 */
#define TF_SPI_R1_ILLEGAL_CMD          0x04    /**< R1响应：非法命令 */
#define TF_SPI_R1_CRC_ERROR            0x08    /**< R1响应：CRC错误 */
#define TF_SPI_R1_ERASE_RESET          0x10    /**< R1响应：擦除复位 */
#define TF_SPI_R1_ADDRESS_ERROR        0x20    /**< R1响应：地址错误 */
#define TF_SPI_R1_PARAMETER_ERROR      0x40    /**< R1响应：参数错误 */

/* SD卡数据令牌 */
#define TF_SPI_TOKEN_START_BLOCK       0xFE    /**< 数据块开始令牌 */
#define TF_SPI_TOKEN_STOP_TRANSMISSION 0xFD   /**< 停止传输令牌 */
#define TF_SPI_TOKEN_DATA_ACCEPTED     0x05    /**< 数据接受令牌 */
#define TF_SPI_TOKEN_DATA_CRC_ERROR    0x0B    /**< 数据CRC错误令牌 */
#define TF_SPI_TOKEN_DATA_WRITE_ERROR  0x0D    /**< 数据写入错误令牌 */

/* SD卡块大小 */
#define TF_SPI_BLOCK_SIZE              512     /**< SD卡块大小（字节） */

/* 默认超时时间 */
#define TF_SPI_DEFAULT_TIMEOUT_MS      1000    /**< 默认超时时间（毫秒） */
#define TF_SPI_INIT_TIMEOUT_MS         30000   /**< 初始化超时时间（毫秒），工业级SD卡可能需要更长时间 */

/* SD卡初始化重试次数 */
#define TF_SPI_INIT_RETRY_COUNT        200     /**< 初始化重试次数，某些SD卡可能需要更多重试 */

/* ==================== 全局状态变量 ==================== */

/**
 * @brief TF_SPI设备全局状态结构体
 * @note 所有设备信息必须保存在此结构体中，禁止分散存储
 */
static tf_spi_dev_t g_tf_spi_device = {
    .capacity_mb = 0,
    .block_size = TF_SPI_BLOCK_SIZE,
    .block_count = 0,
    .card_type = TF_SPI_CARD_TYPE_UNKNOWN,
    .is_sdhc = 0,
    .state = TF_SPI_STATE_UNINITIALIZED
};

/* ==================== 内部静态函数 ==================== */

/**
 * @brief 设置SPI分频（用于SD卡初始化时使用低速，初始化完成后使用高速）
 * @param spi_instance SPI实例
 * @param prescaler 分频值（SPI_BaudRatePrescaler_2, SPI_BaudRatePrescaler_256等）
 */
static void tf_spi_set_prescaler(SPI_Instance_t spi_instance, uint16_t prescaler)
{
    SPI_TypeDef* spi_periph = SPI_GetPeriph(spi_instance);
    if (spi_periph == NULL)
    {
        return;
    }
    
    /* 禁用SPI */
    SPI_Cmd(spi_periph, DISABLE);
    
    /* 等待SPI总线空闲 */
    uint32_t timeout = 1000;
    while (SPI_I2S_GetFlagStatus(spi_periph, SPI_I2S_FLAG_BSY) == SET && timeout--)
    {
        Delay_us(1);
    }
    
    /* 修改分频值 */
    SPI_InitTypeDef SPI_InitStructure;
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = prescaler;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    
    /* 重新初始化SPI（只修改分频值） */
    SPI_Init(spi_periph, &SPI_InitStructure);
    
    /* 使能SPI */
    SPI_Cmd(spi_periph, ENABLE);
    
    /* 等待SPI总线稳定 */
    Delay_us(10);
}

/**
 * @brief 将块地址转换为SD卡地址（SDHC/SDXC使用块地址，SDSC使用字节地址）
 * @param[in] block_addr 块地址
 * @return uint32_t SD卡地址
 */
static uint32_t tf_spi_block_to_addr(uint32_t block_addr)
{
    if (g_tf_spi_device.is_sdhc)
    {
        return block_addr;  /* SDHC/SDXC：直接使用块地址 */
    }
    else
    {
        return block_addr * TF_SPI_BLOCK_SIZE;  /* SDSC：使用字节地址 */
    }
}

/**
 * @brief CS片选拉低（选中SD卡）
 * @param[in] instance SPI实例
 */
static void tf_spi_cs_low(SPI_Instance_t instance)
{
    SPI_Status_t status = SPI_NSS_Low(instance);
    /* 检查CS控制是否成功 */
    if (status != SPI_OK)
    {
        TF_SPI_LOG_DEBUG("CS Low failed: %d", status);
    }
    /* 注释掉CS切换日志以提升性能 */
    /* else
    {
        TF_SPI_LOG_DEBUG("CS Low OK (PA11=Low)");
    } */
}

/**
 * @brief CS片选拉高（释放SD卡）
 * @param[in] instance SPI实例
 */
static void tf_spi_cs_high(SPI_Instance_t instance)
{
    SPI_Status_t status = SPI_NSS_High(instance);
    /* 检查CS控制是否成功 */
    if (status != SPI_OK)
    {
        TF_SPI_LOG_DEBUG("CS High failed: %d", status);
    }
    /* 注释掉CS切换日志以提升性能 */
    /* else
    {
        TF_SPI_LOG_DEBUG("CS High OK (PA11=High)");
    } */
}

/**
 * @brief 发送dummy字节（用于同步）
 * @param[in] instance SPI实例
 * @param[in] count 发送的字节数
 * @note dummy字节发送失败不影响功能，仅用于时钟同步
 */
static void tf_spi_send_dummy(SPI_Instance_t instance, uint8_t count)
{
    uint8_t dummy = 0xFF;
    uint8_t i;
    SPI_Status_t spi_status;
    
    for (i = 0; i < count; i++)
    {
        spi_status = SPI_MasterTransmit(instance, &dummy, 1, 0);
        /* dummy字节发送失败不影响功能，仅用于时钟同步，不检查返回值 */
        (void)spi_status;
    }
}

/**
 * @brief 等待SD卡响应（非0xFF）
 * @param[in] instance SPI实例
 * @param[in] timeout_ms 超时时间（毫秒）
 * @return uint8_t 响应值，超时返回0xFF
 */
static uint8_t tf_spi_wait_response(SPI_Instance_t instance, uint32_t timeout_ms)
{
    uint8_t response = 0xFF;
    uint32_t start_tick = Delay_GetTick();
    uint8_t rx_data;
    uint32_t elapsed;
    SPI_Status_t spi_status;
    uint32_t retry_count = 0;
    
    while (1)
    {
        elapsed = Delay_GetElapsed(Delay_GetTick(), start_tick);
        if (elapsed >= timeout_ms)
        {
            TF_SPI_LOG_DEBUG("tf_spi_wait_response timeout: elapsed=%lu ms, retry_count=%lu", elapsed, retry_count);
            break;  /* 超时 */
        }
        
        rx_data = 0xFF;
        spi_status = SPI_MasterTransmitReceive(instance, &rx_data, &response, 1, 0);
        retry_count++;
        
        /* SPI通信失败时继续等待，直到超时 */
        if (spi_status != SPI_OK)
        {
            /* 每100次尝试记录一次SPI错误 */
            if (retry_count % 100 == 0)
            {
                TF_SPI_LOG_DEBUG("tf_spi_wait_response SPI error: status=%d, retry_count=%lu", spi_status, retry_count);
            }
            continue;
        }
        
        if (response != 0xFF)
        {
            /* 只在超时时记录日志，成功时不记录，避免日志过多 */
            return response;
        }
    }
    
    return 0xFF;  /* 超时 */
}

/**
 * @brief 发送SD卡命令
 * @param[in] instance SPI实例
 * @param[in] cmd 命令字节（包含命令索引）
 * @param[in] arg 命令参数（32位地址）
 * @return uint8_t R1响应值
 */
static uint8_t tf_spi_send_cmd(SPI_Instance_t instance, uint8_t cmd, uint32_t arg)
{
    uint8_t cmd_buf[6];
    uint8_t response;
    uint8_t crc;
    
    /* 构造命令包（6字节：命令+地址+CRC） */
    cmd_buf[0] = cmd | 0x40;  /* 命令字节（bit 6=1） */
    cmd_buf[1] = (uint8_t)(arg >> 24);
    cmd_buf[2] = (uint8_t)(arg >> 16);
    cmd_buf[3] = (uint8_t)(arg >> 8);
    cmd_buf[4] = (uint8_t)(arg);
    
    /* CRC计算（简化：CMD0和CMD8使用固定CRC，其他命令使用0xFF） */
    /* 注意：cmd可能是0x00-0x3F（从TF_SPI_SendCMD传入）或0x40-0x7F（内部使用） */
    /* 需要检查命令索引（去掉bit 6） */
    uint8_t cmd_index = cmd & 0x3F;
    
    if (cmd_index == 0x00)  /* CMD0 */
    {
        crc = 0x95;  /* CMD0的CRC */
    }
    else if (cmd_index == 0x08)  /* CMD8 */
    {
        crc = 0x87;  /* CMD8的CRC（参数0x1AA） */
    }
    else
    {
        crc = 0xFF;  /* 其他命令使用0xFF（SD卡SPI模式不检查CRC） */
    }
    cmd_buf[5] = crc;
    
    /* 发送命令 */
    SPI_Status_t spi_status = SPI_MasterTransmit(instance, cmd_buf, 6, TF_SPI_DEFAULT_TIMEOUT_MS);
    if (spi_status != SPI_OK)
    {
        /* SPI发送失败，返回错误响应 */
        return 0xFF;
    }
    
    /* 等待响应（最多8字节） */
    /* 注意：对于CMD55，某些SD卡可能需要更长的响应时间 */
    /* 特别是对于v2.0+的SD卡，在CMD8之后第一次发送CMD55时 */
    uint32_t timeout_ms = TF_SPI_DEFAULT_TIMEOUT_MS;
    
    /* 如果是CMD55（APP_CMD），增加超时时间到2秒 */
    /* 注意：cmd_index已经在上面声明了，这里直接使用 */
    if (cmd_index == 0x37)  /* CMD55 = 0x37 */
    {
        timeout_ms = 2000;  /* CMD55可能需要更长的响应时间 */
    }
    
    response = tf_spi_wait_response(instance, timeout_ms);
    
    return response;
}

/**
 * @brief 发送应用命令（ACMD）
 * @param[in] instance SPI实例
 * @param[in] cmd 应用命令字节
 * @param[in] arg 命令参数
 * @return uint8_t R1响应值
 */
static uint8_t tf_spi_send_acmd(SPI_Instance_t instance, uint8_t cmd, uint32_t arg)
{
    uint8_t response;
    
    /* 先发送CMD55（应用命令前缀） */
    /* 注意：CMD55和ACMD41必须在同一个CS周期内发送 */
    /* CS由调用者控制，这里只需要发送命令 */
    /* 注意：不要在CMD55之前发送dummy字节，因为调用者已经在CS拉低后发送了dummy字节 */
    
    /* 发送CMD55命令 */
    /* 注意：tf_spi_send_cmd内部会发送6字节命令包，然后等待R1响应 */
    response = tf_spi_send_cmd(instance, TF_SPI_CMD_APP_CMD, 0);
    
    /* 记录CMD55的详细响应信息 */
    if (response == 0xFF)
    {
        /* 0xFF表示超时或没有响应 */
        TF_SPI_LOG_DEBUG("CMD55 timeout (0xFF) - SD card not responding");
        TF_SPI_LOG_DEBUG("Possible causes: 1. SD card not ready after CMD8 2. CS control issue 3. SPI communication problem");
        return response;  /* CMD55失败，直接返回 */
    }
    else if (response != 0x00 && response != 0x01)
    {
        /* 其他错误响应值（如0x04 ILLEGAL_CMD, 0x08 CRC_ERROR等） */
        TF_SPI_LOG_DEBUG("CMD55 error response: 0x%02X", response);
        return response;  /* CMD55错误，直接返回 */
    }
    else
    {
        /* CMD55返回0x00（就绪）或0x01（IDLE_STATE）都是正常的 */
        /* 0x01表示SD卡在IDLE状态，这是初始化过程中的正常状态 */
        /* 应该继续发送ACMD41，以ACMD41的响应来判断初始化是否完成 */
        /* 不记录日志，避免日志过多 */
    }
    
    /* CMD55成功后（返回0x00或0x01），立即发送实际的应用命令（在同一个CS周期内） */
    /* 根据SD协议，ACMD命令必须在CMD55之后立即发送，不能有延迟 */
    /* 注意：不要在ACMD41之前发送dummy字节，因为SD协议要求立即发送 */
    
    response = tf_spi_send_cmd(instance, cmd, arg);
    
    /* 记录ACMD41的响应（只在关键状态时记录，避免日志过多） */
    if (response == 0x00)
    {
        TF_SPI_LOG_DEBUG("ACMD41 response: 0x%02X (initialization complete)", response);
    }
    else if (response == 0xFF)
    {
        TF_SPI_LOG_DEBUG("ACMD41 timeout (0xFF)");
    }
    else if (response != 0x01)
    {
        /* 只记录非0x01的错误响应 */
        TF_SPI_LOG_DEBUG("ACMD41 error response: 0x%02X", response);
    }
    /* 0x01响应不记录日志，因为这是正常的初始化状态 */
    
    return response;
}

/**
 * @brief 读取CSD寄存器并解析容量
 * @param[in] instance SPI实例
 * @return TF_SPI_Status_t 错误码
 */
static TF_SPI_Status_t tf_spi_read_csd(SPI_Instance_t instance)
{
    uint8_t csd[16];
    uint8_t response;
    uint32_t c_size;
    uint8_t csd_structure;
    
    /* 发送CMD9（读取CSD） */
    response = tf_spi_send_cmd(instance, TF_SPI_CMD_SEND_CSD, 0);
    if (response != 0x00)
    {
        return TF_SPI_ERROR_CMD_FAILED;
    }
    
    /* 等待数据令牌 */
    response = tf_spi_wait_response(instance, TF_SPI_DEFAULT_TIMEOUT_MS);
    if (response != TF_SPI_TOKEN_START_BLOCK)
    {
        return TF_SPI_ERROR_CMD_FAILED;
    }
    
    /* 读取CSD数据（16字节） */
    SPI_Status_t spi_status = SPI_MasterReceive(instance, csd, 16, TF_SPI_DEFAULT_TIMEOUT_MS);
    if (spi_status != SPI_OK)
    {
        return TF_SPI_ERROR_CMD_FAILED;
    }
    
    /* 读取CRC（2字节，忽略） */
    tf_spi_send_dummy(instance, 2);
    
    /* 解析CSD结构 */
    csd_structure = (csd[0] >> 6) & 0x03;
    
    /* 输出CSD原始数据用于调试 */
    TF_SPI_LOG_DEBUG("CSD raw data: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                     csd[0], csd[1], csd[2], csd[3], csd[4], csd[5], csd[6], csd[7],
                     csd[8], csd[9], csd[10], csd[11], csd[12], csd[13], csd[14], csd[15]);
    TF_SPI_LOG_DEBUG("CSD structure: %d (0=SDSC v1.0, 1=SDHC/SDXC v2.0)", csd_structure);
    
    if (csd_structure == 0)  /* CSD版本1.0（SDSC） */
    {
        /* 计算容量：C_SIZE = (csd[6] & 0x03) << 10 | csd[7] << 2 | (csd[8] >> 6) & 0x03 */
        c_size = ((uint32_t)(csd[6] & 0x03) << 10) | ((uint32_t)csd[7] << 2) | ((csd[8] >> 6) & 0x03);
        
        /* 容量 = (C_SIZE + 1) * 512 * 2^(C_SIZE_MULT + 2) 字节 */
        /* C_SIZE_MULT = ((csd[9] & 0x03) << 1) | ((csd[10] >> 7) & 0x01) */
        uint8_t c_size_mult = ((csd[9] & 0x03) << 1) | ((csd[10] >> 7) & 0x01);
        uint32_t read_bl_len = csd[5] & 0x0F;  /* READ_BL_LEN */
        
        TF_SPI_LOG_DEBUG("SDSC: C_SIZE=%lu, C_SIZE_MULT=%d, READ_BL_LEN=%lu", c_size, c_size_mult, read_bl_len);
        
        /* 使用64位运算防止溢出 */
        uint64_t capacity_bytes_64 = ((uint64_t)(c_size + 1)) * (1ULL << (c_size_mult + 2)) * (1ULL << read_bl_len);
        
        /* 保存容量信息（使用64位计算，然后转换为32位） */
        uint64_t capacity_mb_64 = capacity_bytes_64 / (1024ULL * 1024ULL);
        uint64_t block_count_64 = capacity_bytes_64 / TF_SPI_BLOCK_SIZE;
        
        TF_SPI_LOG_DEBUG("SDSC capacity: %llu bytes (%.2f MB), %llu blocks", 
                         capacity_bytes_64, (double)capacity_bytes_64 / (1024.0 * 1024.0), block_count_64);
        
        /* 检查是否超出32位范围 */
        if (capacity_mb_64 > UINT32_MAX || block_count_64 > UINT32_MAX)
        {
            return TF_SPI_ERROR_CMD_FAILED;
        }
        
        g_tf_spi_device.capacity_mb = (uint32_t)capacity_mb_64;
        g_tf_spi_device.block_count = (uint32_t)block_count_64;
        
        g_tf_spi_device.card_type = TF_SPI_CARD_TYPE_SDSC;
        g_tf_spi_device.is_sdhc = 0;
    }
    else if (csd_structure == 1)  /* CSD版本2.0（SDHC/SDXC） */
    {
        /* 计算容量：C_SIZE = (csd[7] & 0x3F) << 16 | csd[8] << 8 | csd[9] */
        c_size = ((uint32_t)(csd[7] & 0x3F) << 16) | ((uint32_t)csd[8] << 8) | csd[9];
        
        TF_SPI_LOG_DEBUG("SDHC/SDXC: C_SIZE=%lu (from csd[7]=0x%02X, csd[8]=0x%02X, csd[9]=0x%02X)", 
                         c_size, csd[7], csd[8], csd[9]);
        
        /* 容量 = (C_SIZE + 1) * 512KB，使用64位运算防止溢出 */
        uint64_t capacity_bytes_64 = ((uint64_t)(c_size + 1)) * 512ULL * 1024ULL;
        
        /* 避免使用浮点数，直接计算MB（整数运算） */
        uint64_t capacity_mb_64 = capacity_bytes_64 / (1024ULL * 1024ULL);
        
        #if TF_SPI_DEBUG_ENABLED
        uint64_t capacity_gb_64 = capacity_bytes_64 / (1024ULL * 1024ULL * 1024ULL);
        uint64_t capacity_gb_remainder = (capacity_bytes_64 % (1024ULL * 1024ULL * 1024ULL)) / (1024ULL * 1024ULL * 10ULL);  /* GB的小数部分（0.1GB精度） */
        TF_SPI_LOG_DEBUG("SDHC/SDXC capacity calculation: (C_SIZE+1)=%lu, capacity_bytes=%llu", 
                         c_size + 1, capacity_bytes_64);
        TF_SPI_LOG_DEBUG("SDHC/SDXC capacity: %llu MB (约 %llu.%llu GB)", 
                         capacity_mb_64, capacity_gb_64, capacity_gb_remainder);
        #endif
        
        /* 根据容量判断卡类型（使用64位比较） */
        if (capacity_bytes_64 >= 32ULL * 1024ULL * 1024ULL * 1024ULL)  /* >= 32GB */
        {
            g_tf_spi_device.card_type = TF_SPI_CARD_TYPE_SDXC;
        }
        else
        {
            g_tf_spi_device.card_type = TF_SPI_CARD_TYPE_SDHC;
        }
        g_tf_spi_device.is_sdhc = 1;
        
        /* 保存容量信息（使用64位计算，然后转换为32位） */
        /* 注意：即使容量超过32位范围，capacity_mb和block_count仍然可以正确计算 */
        /* 例如：8GB = 8192 MB（可以存储在uint32_t中） */
        /* 例如：8GB = 16,777,216 块（可以存储在uint32_t中） */
        /* capacity_mb_64已在上面计算 */
        uint64_t block_count_64 = capacity_bytes_64 / TF_SPI_BLOCK_SIZE;
        
        /* 检查是否超出32位范围，如果超出则限制，但尽量显示正确值 */
        if (capacity_mb_64 > UINT32_MAX)
        {
            g_tf_spi_device.capacity_mb = UINT32_MAX;
            TF_SPI_LOG_DEBUG("Capacity MB exceeds 32-bit limit, clamped to %lu MB", UINT32_MAX);
        }
        else
        {
            g_tf_spi_device.capacity_mb = (uint32_t)capacity_mb_64;
        }
        
        if (block_count_64 > UINT32_MAX)
        {
            g_tf_spi_device.block_count = UINT32_MAX;
            TF_SPI_LOG_DEBUG("Block count exceeds 32-bit limit, clamped to %lu", UINT32_MAX);
        }
        else
        {
            g_tf_spi_device.block_count = (uint32_t)block_count_64;
        }
        
        /* 记录实际容量（使用64位） */
        #if TF_SPI_DEBUG_ENABLED
        if (capacity_bytes_64 > UINT32_MAX)
        {
            uint64_t actual_gb_64 = capacity_bytes_64 / (1024ULL * 1024ULL * 1024ULL);
            uint64_t actual_gb_remainder = (capacity_bytes_64 % (1024ULL * 1024ULL * 1024ULL)) / (1024ULL * 1024ULL * 10ULL);
            TF_SPI_LOG_DEBUG("Actual capacity: %llu bytes (约 %llu.%llu GB), stored as %lu MB, %lu blocks", 
                             capacity_bytes_64, 
                             actual_gb_64,
                             actual_gb_remainder,
                             g_tf_spi_device.capacity_mb,
                             g_tf_spi_device.block_count);
        }
        #endif
    }
    else
    {
        return TF_SPI_ERROR_CMD_FAILED;
    }
    
    return TF_SPI_OK;
}

/* ==================== 公共API函数 ==================== */

/**
 * @brief TF_SPI初始化
 */
TF_SPI_Status_t TF_SPI_Init(void)
{
    SPI_Instance_t spi_instance = TF_SPI_SPI_INSTANCE;
    uint8_t response;
    uint32_t ocr;
    uint32_t start_tick;
    uint32_t retry_count;
    TF_SPI_Status_t status;
    
    /* ========== 参数校验 ========== */
    
    /* 检查SPI模块是否已初始化 */
    if (!SPI_IsInitialized(spi_instance))
    {
        TF_SPI_LOG_DEBUG("SPI module not initialized");
        return TF_SPI_ERROR_INIT_FAILED;
    }
    
    /* 如果已初始化，直接返回成功 */
    if (g_tf_spi_device.state == TF_SPI_STATE_INITIALIZED)
    {
        return TF_SPI_OK;
    }
    
    /* ========== 初始化流程 ========== */
    
    /* 0. 设置SPI为256分频（初始化时使用低速） */
    TF_SPI_LOG_DEBUG("Setting SPI prescaler to 256 for initialization...");
    tf_spi_set_prescaler(spi_instance, SPI_BaudRatePrescaler_256);
    
    /* 1. 上电后等待至少74个时钟周期（通过发送10个0xFF实现） */
    TF_SPI_LOG_DEBUG("Step 1: Power-on reset (CS high, send 10 dummy bytes)");
    tf_spi_cs_high(spi_instance);
    Delay_ms(10);  /* 参考Flash06，使用10ms延时确保CS稳定 */
    tf_spi_send_dummy(spi_instance, 10);
    Delay_ms(10);  /* 参考Flash06，使用10ms延时确保SPI总线稳定 */
    
    /* 2. 发送CMD0（复位卡） */
    /* 参考Flash06，可能需要多次尝试CMD0才能让卡进入IDLE状态 */
    TF_SPI_LOG_DEBUG("Step 2: Sending CMD0 (reset card)...");
    for (uint8_t retry = 0; retry < 3; retry++)
    {
    tf_spi_cs_low(spi_instance);
        Delay_us(10);  /* 短暂延时，确保CS拉低后SD卡准备好 */
    response = tf_spi_send_cmd(spi_instance, TF_SPI_CMD_GO_IDLE_STATE, 0);
        tf_spi_cs_high(spi_instance);
        tf_spi_send_dummy(spi_instance, 1);
        
        if (response == TF_SPI_R1_IDLE_STATE)
        {
            TF_SPI_LOG_DEBUG("CMD0 success: 0x%02X (IDLE_STATE) on retry %d", response, retry);
            break;
        }
        else if (retry < 2)
        {
            TF_SPI_LOG_DEBUG("CMD0 retry %d: response=0x%02X", retry + 1, response);
            Delay_ms(10);
        }
        else
        {
            TF_SPI_LOG_DEBUG("CMD0 failed after 3 retries: 0x%02X", response);
            return TF_SPI_ERROR_INIT_FAILED;
        }
    }
    tf_spi_cs_high(spi_instance);
    tf_spi_send_dummy(spi_instance, 1);
    
    if (response != TF_SPI_R1_IDLE_STATE)
    {
        TF_SPI_LOG_DEBUG("CMD0 failed: 0x%02X", response);
        return TF_SPI_ERROR_INIT_FAILED;
    }
    
    TF_SPI_LOG_DEBUG("CMD0 success: 0x%02X (IDLE_STATE)", response);
    
    /* CMD0完成后，等待一段时间再发送CMD8 */
    /* 根据SD协议，CMD0之后应该等待至少1ms */
    /* 某些SD卡可能需要更长的稳定时间 */
    TF_SPI_LOG_DEBUG("Waiting 100ms after CMD0 before sending CMD8...");
    Delay_ms(100);
    tf_spi_send_dummy(spi_instance, 8);  /* 发送8个dummy字节，确保SPI总线稳定 */
    
    /* 3. 发送CMD8（检查电压兼容性，SD卡v2.0+） */
    TF_SPI_LOG_DEBUG("Sending CMD8...");
    tf_spi_cs_low(spi_instance);
    /* 在CS拉低后，发送1个dummy字节，确保SD卡准备好接收命令 */
    tf_spi_send_dummy(spi_instance, 1);
    response = tf_spi_send_cmd(spi_instance, TF_SPI_CMD_SEND_IF_COND, 0x1AA);
    TF_SPI_LOG_DEBUG("CMD8 R1 response: 0x%02X", response);
    
    /* 记录卡类型，用于后续ACMD41参数选择 */
    uint8_t is_sd_v2 = 0;
    
    if (response == TF_SPI_R1_IDLE_STATE)
    {
        /* SD卡v2.0+，读取R7响应（4字节） */
        /* 注意：R1响应已经在tf_spi_send_cmd中读取，这里只需要读取R7的4字节数据 */
        /* R7格式：保留字节(1) + 电压范围(1) + 检查模式(1) + CRC(1) */
        uint8_t r7[4];
        SPI_Status_t spi_status = SPI_MasterReceive(spi_instance, r7, 4, TF_SPI_DEFAULT_TIMEOUT_MS);
        
        /* 注意：R7响应是在CS拉低的情况下读取的，读取完成后需要拉高CS */
        /* 但是，在读取R7之前，需要确保已经读取了CRC（2字节） */
        /* 实际上，R7响应是4字节，不需要读取CRC */
        
        if (spi_status != SPI_OK)
        {
            /* 如果读取R7失败，先拉高CS，然后返回错误 */
            tf_spi_cs_high(spi_instance);
            tf_spi_send_dummy(spi_instance, 1);
            TF_SPI_LOG_DEBUG("CMD8 R7 read failed");
            return TF_SPI_ERROR_INIT_FAILED;
        }
        
        /* 拉高CS，完成CMD8命令 */
        tf_spi_cs_high(spi_instance);
        tf_spi_send_dummy(spi_instance, 1);
        
        /* 记录R7响应内容用于调试 */
        TF_SPI_LOG_DEBUG("CMD8 R7 response: 0x%02X 0x%02X 0x%02X 0x%02X", 
                         r7[0], r7[1], r7[2], r7[3]);
        
        /* 根据SD协议，R7响应格式（4字节）：
         * r7[0] = 保留字节（通常为0x00）
         * r7[1] = 电压范围（0x01表示2.7-3.6V）
         * r7[2] = 检查模式（0xAA）
         * r7[3] = CRC（SPI模式下可忽略）
         * 
         * 但从实际响应来看：0x00 0x00 0x01 0xAA
         * 可能是字节顺序问题，或者响应格式不同
         * 让我们检查r7[2]是否为0x01（电压范围），r7[3]是否为0xAA（检查模式）
         */
        
        /* 根据SD协议，R7响应格式（4字节）：
         * r7[0] = 保留字节（通常为0x00）
         * r7[1] = 电压范围（0x01表示2.7-3.6V）
         * r7[2] = 检查模式（0xAA）
         * r7[3] = CRC（SPI模式下可忽略）
         * 
         * 但从实际响应来看：0x00 0x00 0x01 0xAA
         * 可能是读取R7时，第一个字节是R1的重复（在SPI模式下，R1响应后立即读取R7）
         * 所以实际格式可能是：r7[0]=R1重复, r7[1]=保留, r7[2]=电压, r7[3]=模式
         * 或者：r7[0]=保留, r7[1]=保留, r7[2]=电压, r7[3]=模式
         */
        
        /* 检查电压兼容性和检查模式 - 支持两种可能的格式 */
        uint8_t voltage_ok = 0;
        uint8_t pattern_ok = 0;
        
        /* 格式1：标准格式 r7[1]=电压(0x01), r7[2]=模式(0xAA) */
        if (r7[1] == 0x01 && r7[2] == 0xAA)
        {
            voltage_ok = 1;
            pattern_ok = 1;
            TF_SPI_LOG_DEBUG("R7 standard format: voltage=r7[1]=0x%02X, pattern=r7[2]=0x%02X", r7[1], r7[2]);
        }
        /* 格式2：偏移格式 r7[2]=电压(0x01), r7[3]=模式(0xAA) */
        else if (r7[2] == 0x01 && r7[3] == 0xAA)
        {
            voltage_ok = 1;
            pattern_ok = 1;
            TF_SPI_LOG_DEBUG("R7 offset format: voltage=r7[2]=0x%02X, pattern=r7[3]=0x%02X", r7[2], r7[3]);
        }
        
        if (!voltage_ok || !pattern_ok)
        {
            TF_SPI_LOG_DEBUG("R7 validation failed: r7[0]=0x%02X, r7[1]=0x%02X, r7[2]=0x%02X, r7[3]=0x%02X", 
                             r7[0], r7[1], r7[2], r7[3]);
            TF_SPI_LOG_DEBUG("Expected: voltage=0x01, pattern=0xAA");
            return TF_SPI_ERROR_INIT_FAILED;
        }
        
        is_sd_v2 = 1;  /* 标记为SD卡v2.0+ */
    }
    else if (response == (TF_SPI_R1_IDLE_STATE | TF_SPI_R1_ILLEGAL_CMD))
    {
        /* SD卡v1.0，不支持CMD8 */
        tf_spi_cs_high(spi_instance);
        tf_spi_send_dummy(spi_instance, 1);
        TF_SPI_LOG_DEBUG("SD card v1.0 detected");
        is_sd_v2 = 0;  /* 标记为SD卡v1.0 */
    }
    else
    {
        tf_spi_cs_high(spi_instance);
        tf_spi_send_dummy(spi_instance, 1);
        /* CMD8失败，记录详细错误信息 */
        if (response == 0xFF)
        {
            TF_SPI_LOG_DEBUG("CMD8 timeout (0xFF) - SD card not responding");
            TF_SPI_LOG_DEBUG("Possible causes: 1. MISO pin not connected 2. CS control issue 3. SD card not responding");
        }
        else
        {
            TF_SPI_LOG_DEBUG("CMD8 failed: 0x%02X", response);
        }
        return TF_SPI_ERROR_INIT_FAILED;
    }
    
    /* CMD8完成后，等待一段时间，确保SD卡准备好接收下一个命令 */
    /* 根据SD协议，在CMD8之后，建议等待至少1ms */
    /* 同时发送一些dummy字节，确保SPI总线稳定 */
    /* 注意：CS此时应该是拉高的，所以这些dummy字节是在CS高的情况下发送的 */
    /* 但是，某些SD卡在CMD8之后可能需要更长的稳定时间才能响应CMD55 */
    /* 增加等待时间到100ms，确保SD卡完全准备好 */
    /* 注意：在CS拉高后，需要确保CS保持高电平一段时间，然后再拉低发送CMD55 */
    TF_SPI_LOG_DEBUG("CMD8 completed, waiting 100ms before ACMD41");
    Delay_ms(100);  /* 等待100ms，确保SD卡完全准备好 */
    /* 注意：在CS拉高状态下发送dummy字节，确保SPI总线稳定 */
    tf_spi_send_dummy(spi_instance, 8);  /* 发送8个dummy字节，确保SPI总线稳定 */
    TF_SPI_LOG_DEBUG("Ready to send ACMD41, CS should be high now");
    
    /* 4. 发送ACMD41（初始化SD卡，带超时重试） */
    /* 根据卡类型选择ACMD41参数：v2.0+使用HCS位，v1.0不使用 */
    /* 注意：某些SD卡可能不支持HCS位，如果初始化失败，自动尝试不使用HCS位 */
    uint32_t acmd41_arg = is_sd_v2 ? 0x40000000 : 0x00;
    uint8_t try_without_hcs = 0;  /* 是否尝试不使用HCS位 */
    uint8_t init_success = 0;  /* 初始化是否成功 */
    ocr = 0;  /* OCR寄存器值，初始化为0 */
    
    TF_SPI_LOG_DEBUG("Starting ACMD41 initialization: is_sd_v2=%d, arg=0x%08lX", is_sd_v2, acmd41_arg);
    
    start_tick = Delay_GetTick();
    retry_count = 0;
    
    while (retry_count < TF_SPI_INIT_RETRY_COUNT && !init_success)
    {
        /* 检查超时（使用Delay_GetElapsed处理溢出） */
        uint32_t elapsed = Delay_GetElapsed(Delay_GetTick(), start_tick);
        if (elapsed >= TF_SPI_INIT_TIMEOUT_MS)
        {
            TF_SPI_LOG_DEBUG("ACMD41 timeout after %lu ms, retry_count=%lu", elapsed, retry_count);
            return TF_SPI_ERROR_TIMEOUT;
        }
        
        /* 拉低CS，开始ACMD41命令序列 */
        /* 注意：CMD55和ACMD41必须在同一个CS周期内发送 */
        /* 只在每20次重试时记录详细日志，避免日志过多 */
        if (retry_count % 20 == 0)
        {
            TF_SPI_LOG_DEBUG("ACMD41 retry=%lu: CS Low, sending CMD55...", retry_count);
        }
        tf_spi_cs_low(spi_instance);
        
        /* 发送1个dummy字节，确保SPI总线稳定，SD卡准备好接收命令 */
        /* 根据SD协议，在CS拉低后，建议发送至少1个dummy字节 */
        /* 注意：这个dummy字节必须在CS拉低后立即发送，不能有延迟 */
        tf_spi_send_dummy(spi_instance, 1);
        
        /* 发送ACMD41（内部会先发送CMD55，然后发送ACMD41） */
        /* 注意：CMD55和ACMD41必须在同一个CS周期内发送，不能拉高CS */
        response = tf_spi_send_acmd(spi_instance, TF_SPI_ACMD_SD_SEND_OP_COND, acmd41_arg);
        
        /* 记录CMD55的响应（如果失败） */
        if (response == 0xFF)
        {
            TF_SPI_LOG_DEBUG("ACMD41 failed: CMD55 returned 0xFF (no response from SD card)");
        }
        
        /* 拉高CS，结束ACMD41命令序列 */
        tf_spi_cs_high(spi_instance);
        tf_spi_send_dummy(spi_instance, 1);
        
        /* 记录ACMD41的响应（每20次记录一次，避免日志过多） */
        if (retry_count % 20 == 0 || response == 0x00)
        {
            TF_SPI_LOG_DEBUG("ACMD41 retry=%lu, elapsed=%lu ms, response=0x%02X", 
                             retry_count, elapsed, response);
        }
        
        if (response == 0x00)
        {
            /* 初始化完成 */
            TF_SPI_LOG_DEBUG("ACMD41 success after %lu retries, elapsed=%lu ms", retry_count, elapsed);
            init_success = 1;
            break;
        }
        
        /* 如果响应是0xFF（超时），说明SD卡没有响应，可能需要更长的等待时间 */
        if (response == 0xFF)
        {
            TF_SPI_LOG_DEBUG("ACMD41 no response (0xFF) at retry=%lu", retry_count);
            /* 对于0xFF响应，等待更长时间（50ms） */
            Delay_ms(50);
        }
        else if (response == 0x01)
        {
            /* 对于0x01响应（IDLE_STATE），说明卡还在初始化 */
            /* 但是，某些SD卡可能在ACMD41返回0x01时，OCR寄存器已经设置好了 */
            /* 每10次重试检查一次OCR寄存器，确认初始化是否完成 */
            if (retry_count % 10 == 0 && retry_count > 0)
            {
                TF_SPI_LOG_DEBUG("OCR check at retry=%lu: checking OCR register...", retry_count);
                
                /* 临时拉高CS，读取OCR寄存器 */
                tf_spi_cs_high(spi_instance);
                tf_spi_send_dummy(spi_instance, 1);
                Delay_ms(1);  /* 等待CS稳定 */
                
                /* 读取OCR寄存器 */
                tf_spi_cs_low(spi_instance);
                tf_spi_send_dummy(spi_instance, 1);
                uint8_t ocr_response = tf_spi_send_cmd(spi_instance, TF_SPI_CMD_READ_OCR, 0);
                
                TF_SPI_LOG_DEBUG("OCR check: CMD58 response=0x%02X", ocr_response);
                
                /* 根据SD协议，即使CMD58返回0x01（IDLE_STATE），我们也应该读取OCR寄存器 */
                /* 因为OCR的bit 31（卡就绪标志）可能在R1响应为0x01时就已经置位了 */
                if (ocr_response == 0x00 || ocr_response == 0x01)
                {
                    /* 读取OCR（4字节） */
                    uint8_t ocr_buf[4];
                    SPI_Status_t spi_status = SPI_MasterReceive(spi_instance, ocr_buf, 4, TF_SPI_DEFAULT_TIMEOUT_MS);
                    tf_spi_cs_high(spi_instance);
                    tf_spi_send_dummy(spi_instance, 1);
                    
                    if (spi_status == SPI_OK)
                    {
                        uint32_t temp_ocr = ((uint32_t)ocr_buf[0] << 24) | ((uint32_t)ocr_buf[1] << 16) | 
                                            ((uint32_t)ocr_buf[2] << 8) | ocr_buf[3];
                        
                        TF_SPI_LOG_DEBUG("OCR check: OCR=0x%08X, R1=0x%02X", temp_ocr, ocr_response);
                        
                        /* 检查OCR的bit 31（卡就绪标志） */
                        if (temp_ocr & 0x80000000)
                        {
                            /* 卡已经就绪，即使ACMD41返回0x01或CMD58返回0x01 */
                            TF_SPI_LOG_DEBUG("Card ready detected via OCR (0x%08X) at retry=%lu, even though ACMD41 returned 0x01", 
                                            temp_ocr, retry_count);
                            ocr = temp_ocr;
                            init_success = 1;
                            break;  /* 跳出循环，初始化完成 */
                        }
                        else
                        {
                            /* 分析OCR值，提供诊断信息 */
#if TF_SPI_DEBUG_ENABLED
                            uint8_t voltage_range = (temp_ocr >> 15) & 0xFF;
                            TF_SPI_LOG_DEBUG("OCR check: card not ready yet (OCR: 0x%08X, bit31=0) at retry=%lu", temp_ocr, retry_count);
                            TF_SPI_LOG_DEBUG("OCR analysis: voltage_range=0x%02X, bit31=0 (card not ready)", voltage_range);
#else
                            TF_SPI_LOG_DEBUG("OCR check: card not ready yet (OCR: 0x%08X, bit31=0) at retry=%lu", temp_ocr, retry_count);
#endif
                            
                            /* 如果OCR值一直是0x00FF8000，可能是硬件问题 */
                            if (temp_ocr == 0x00FF8000 && retry_count > 50)
                            {
                                TF_SPI_LOG_DEBUG("Warning: OCR stuck at 0x00FF8000, possible hardware issue:");
                                TF_SPI_LOG_DEBUG("  1. Check SD card power supply (should be 3.3V stable)");
                                TF_SPI_LOG_DEBUG("  2. Check MISO pin (PB14) - MUST have pull-up resistor (10k-50k ohm)");
                                TF_SPI_LOG_DEBUG("     Note: Only MISO needs pull-up, CS (PA11) does NOT need pull-up");
                                TF_SPI_LOG_DEBUG("  3. Check SPI clock frequency (should be <= 400kHz during init)");
                                TF_SPI_LOG_DEBUG("  4. Check CS pin (PA11) control logic (GPIO output, no pull-up needed)");
                            }
                        }
                    }
                    else
                    {
                        TF_SPI_LOG_DEBUG("OCR check: SPI_MasterReceive failed, status=%d", spi_status);
                    }
                }
                else
                {
                    tf_spi_cs_high(spi_instance);
                    tf_spi_send_dummy(spi_instance, 1);
                    TF_SPI_LOG_DEBUG("OCR check: CMD58 error response=0x%02X (not 0x00 or 0x01)", ocr_response);
                }
                
                /* OCR检查后，等待一段时间再继续ACMD41 */
                Delay_ms(10);
            }
            
            /* 等待100ms后重试，给工业级SD卡更多时间完成初始化 */
            Delay_ms(100);
        }
        else
        {
            /* 其他错误响应 */
            TF_SPI_LOG_DEBUG("ACMD41 error response: 0x%02X at retry=%lu", response, retry_count);
            Delay_ms(10);
        }
        
        retry_count++;
    }
    
    if (retry_count >= TF_SPI_INIT_RETRY_COUNT && !init_success)
    {
        uint32_t elapsed = Delay_GetElapsed(Delay_GetTick(), start_tick);
        TF_SPI_LOG_DEBUG("ACMD41 failed after %lu retries, elapsed=%lu ms", retry_count, elapsed);
        
        /* 如果使用HCS位失败，且是SD v2.0+，尝试不使用HCS位 */
        if (is_sd_v2 && acmd41_arg == 0x40000000 && !try_without_hcs)
        {
            TF_SPI_LOG_DEBUG("Trying ACMD41 without HCS bit (arg=0x00)...");
            acmd41_arg = 0x00;
            try_without_hcs = 1;
            start_tick = Delay_GetTick();
            retry_count = 0;
            ocr = 0;
            init_success = 0;
            
            /* 等待一段时间，让SD卡稳定 */
            Delay_ms(100);
            tf_spi_send_dummy(spi_instance, 8);
            
            /* 重新开始循环，使用新的参数 */
            while (retry_count < TF_SPI_INIT_RETRY_COUNT && !init_success)
            {
                /* 检查超时（使用Delay_GetElapsed处理溢出） */
                uint32_t elapsed = Delay_GetElapsed(Delay_GetTick(), start_tick);
                if (elapsed >= TF_SPI_INIT_TIMEOUT_MS)
                {
                    TF_SPI_LOG_DEBUG("ACMD41 timeout after %lu ms, retry_count=%lu", elapsed, retry_count);
                    return TF_SPI_ERROR_TIMEOUT;
                }
                
                /* 拉低CS，开始ACMD41命令序列 */
                if (retry_count % 20 == 0)
                {
                    TF_SPI_LOG_DEBUG("ACMD41 retry=%lu (without HCS): CS Low, sending CMD55...", retry_count);
                }
                tf_spi_cs_low(spi_instance);
                tf_spi_send_dummy(spi_instance, 1);
                
                /* 发送ACMD41 */
                response = tf_spi_send_acmd(spi_instance, TF_SPI_ACMD_SD_SEND_OP_COND, acmd41_arg);
                
                if (response == 0xFF)
                {
                    TF_SPI_LOG_DEBUG("ACMD41 failed: CMD55 returned 0xFF (no response from SD card)");
                }
                
                /* 拉高CS，结束ACMD41命令序列 */
                tf_spi_cs_high(spi_instance);
                tf_spi_send_dummy(spi_instance, 1);
                
                if (retry_count % 20 == 0 || response == 0x00)
                {
                    TF_SPI_LOG_DEBUG("ACMD41 retry=%lu, elapsed=%lu ms, response=0x%02X", 
                                     retry_count, elapsed, response);
                }
                
                if (response == 0x00)
                {
                    TF_SPI_LOG_DEBUG("ACMD41 success after %lu retries (without HCS), elapsed=%lu ms", retry_count, elapsed);
                    init_success = 1;
                    break;
                }
                
                if (response == 0xFF)
                {
                    TF_SPI_LOG_DEBUG("ACMD41 no response (0xFF) at retry=%lu", retry_count);
                    Delay_ms(50);
                }
                else if (response == 0x01)
                {
                    /* 每10次重试检查一次OCR寄存器 */
                    if (retry_count % 10 == 0 && retry_count > 0)
                    {
                        TF_SPI_LOG_DEBUG("OCR check at retry=%lu: checking OCR register...", retry_count);
                        
                        tf_spi_cs_high(spi_instance);
                        tf_spi_send_dummy(spi_instance, 1);
                        Delay_ms(1);
                        
                        tf_spi_cs_low(spi_instance);
                        tf_spi_send_dummy(spi_instance, 1);
                        uint8_t ocr_response = tf_spi_send_cmd(spi_instance, TF_SPI_CMD_READ_OCR, 0);
                        
                        TF_SPI_LOG_DEBUG("OCR check: CMD58 response=0x%02X", ocr_response);
                        
                        if (ocr_response == 0x00 || ocr_response == 0x01)
                        {
                            uint8_t ocr_buf[4];
                            SPI_Status_t spi_status = SPI_MasterReceive(spi_instance, ocr_buf, 4, TF_SPI_DEFAULT_TIMEOUT_MS);
                            tf_spi_cs_high(spi_instance);
                            tf_spi_send_dummy(spi_instance, 1);
                            
                            if (spi_status == SPI_OK)
                            {
                                uint32_t temp_ocr = ((uint32_t)ocr_buf[0] << 24) | ((uint32_t)ocr_buf[1] << 16) | 
                                                    ((uint32_t)ocr_buf[2] << 8) | ocr_buf[3];
                                
                                TF_SPI_LOG_DEBUG("OCR check: OCR=0x%08X, R1=0x%02X", temp_ocr, ocr_response);
                                
                                if (temp_ocr & 0x80000000)
                                {
                                    TF_SPI_LOG_DEBUG("Card ready detected via OCR (0x%08X) at retry=%lu", temp_ocr, retry_count);
                                    ocr = temp_ocr;
                                    init_success = 1;
                                    break;
                                }
                                else
                                {
                                    /* 分析OCR值，提供诊断信息 */
#if TF_SPI_DEBUG_ENABLED
                                    uint8_t voltage_range = (temp_ocr >> 15) & 0xFF;
                                    TF_SPI_LOG_DEBUG("OCR check: card not ready yet (OCR: 0x%08X, bit31=0) at retry=%lu", temp_ocr, retry_count);
                                    TF_SPI_LOG_DEBUG("OCR analysis: voltage_range=0x%02X, bit31=0 (card not ready)", voltage_range);
#else
                                    TF_SPI_LOG_DEBUG("OCR check: card not ready yet (OCR: 0x%08X, bit31=0) at retry=%lu", temp_ocr, retry_count);
#endif
                                    
                                    /* 如果OCR值一直是0x00FF8000，可能是硬件问题 */
                                    if (temp_ocr == 0x00FF8000 && retry_count > 50)
                                    {
                                        TF_SPI_LOG_DEBUG("Warning: OCR stuck at 0x00FF8000, possible hardware issue:");
                                        TF_SPI_LOG_DEBUG("  1. Check SD card power supply (should be 3.3V stable)");
                                        TF_SPI_LOG_DEBUG("  2. Check MISO pin (PB14) - MUST have pull-up resistor (10k-50k ohm)");
                                        TF_SPI_LOG_DEBUG("     Note: Only MISO needs pull-up, CS (PA11) does NOT need pull-up");
                                        TF_SPI_LOG_DEBUG("  3. Check SPI clock frequency (should be <= 400kHz during init)");
                                        TF_SPI_LOG_DEBUG("  4. Check CS pin (PA11) control logic (GPIO output, no pull-up needed)");
                                    }
                                }
                            }
                        }
                        else
                        {
                            tf_spi_cs_high(spi_instance);
                            tf_spi_send_dummy(spi_instance, 1);
                        }
                        
                        Delay_ms(10);
                    }
                    
                    Delay_ms(100);
                }
                else
                {
                    TF_SPI_LOG_DEBUG("ACMD41 error response: 0x%02X at retry=%lu", response, retry_count);
                    Delay_ms(10);
                }
                
                retry_count++;
            }
        }
        
        if (!init_success)
        {
            return TF_SPI_ERROR_INIT_FAILED;
        }
    }
    
    /* 5. 发送CMD58（读取OCR，确认初始化完成） */
    /* 注意：如果已经在循环中读取了OCR，这里可以跳过 */
    if (ocr == 0)
    {
        tf_spi_cs_low(spi_instance);
        response = tf_spi_send_cmd(spi_instance, TF_SPI_CMD_READ_OCR, 0);
        
        if (response != 0x00)
        {
            tf_spi_cs_high(spi_instance);
            tf_spi_send_dummy(spi_instance, 1);
            TF_SPI_LOG_DEBUG("CMD58 failed: 0x%02X", response);
            return TF_SPI_ERROR_INIT_FAILED;
        }
        
        /* 读取OCR（4字节） */
        {
            uint8_t ocr_buf[4];
            SPI_Status_t spi_status = SPI_MasterReceive(spi_instance, ocr_buf, 4, TF_SPI_DEFAULT_TIMEOUT_MS);
            if (spi_status != SPI_OK)
            {
                tf_spi_cs_high(spi_instance);
                tf_spi_send_dummy(spi_instance, 1);
                TF_SPI_LOG_DEBUG("CMD58 OCR read failed");
                return TF_SPI_ERROR_INIT_FAILED;
            }
            ocr = ((uint32_t)ocr_buf[0] << 24) | ((uint32_t)ocr_buf[1] << 16) | 
                  ((uint32_t)ocr_buf[2] << 8) | ocr_buf[3];
        }
        
        tf_spi_cs_high(spi_instance);
        tf_spi_send_dummy(spi_instance, 1);
    }
    
    /* 检查OCR的bit 31（卡就绪标志） */
    if (!(ocr & 0x80000000))
    {
        TF_SPI_LOG_DEBUG("Card not ready (OCR: 0x%08X)", ocr);
        TF_SPI_LOG_DEBUG("OCR analysis: bit31=0 (card not ready), voltage_range=0x%02X", (ocr >> 15) & 0xFF);
        TF_SPI_LOG_DEBUG("Possible causes:");
        TF_SPI_LOG_DEBUG("  1. SD card power supply issue (check 3.3V stability)");
        TF_SPI_LOG_DEBUG("  2. MISO pin (PB14) - MUST have pull-up resistor (10k-50k ohm)");
        TF_SPI_LOG_DEBUG("     Note: Only MISO (PB14) needs pull-up, CS (PA11) does NOT need pull-up");
        TF_SPI_LOG_DEBUG("  3. SPI clock frequency too high (should be <= 400kHz during init)");
        TF_SPI_LOG_DEBUG("  4. CS pin (PA11) control issue (should be GPIO output, no pull-up needed)");
        TF_SPI_LOG_DEBUG("  5. SD card may be damaged or incompatible");
        return TF_SPI_ERROR_INIT_FAILED;
    }
    
    /* 6. 发送CMD9（读取CSD，获取容量信息） */
    tf_spi_cs_low(spi_instance);
    status = tf_spi_read_csd(spi_instance);
    tf_spi_cs_high(spi_instance);
    tf_spi_send_dummy(spi_instance, 1);
    
    if (status != TF_SPI_OK)
    {
        TF_SPI_LOG_DEBUG("CMD9 failed");
        return status;
    }
    
    /* 7. 如果是SDSC，发送CMD16设置块长度为512字节 */
    if (!g_tf_spi_device.is_sdhc)
    {
        tf_spi_cs_low(spi_instance);
        response = tf_spi_send_cmd(spi_instance, TF_SPI_CMD_SET_BLOCKLEN, TF_SPI_BLOCK_SIZE);
        tf_spi_cs_high(spi_instance);
        tf_spi_send_dummy(spi_instance, 1);
        
        if (response != 0x00)
        {
            TF_SPI_LOG_DEBUG("CMD16 failed: 0x%02X", response);
            return TF_SPI_ERROR_INIT_FAILED;
        }
    }
    
    /* 初始化完成，切换到8分频进入工作模式（4.5MHz，平衡性能和稳定性） */
    TF_SPI_LOG_DEBUG("Initialization complete, switching SPI prescaler to 8 for operation...");
    tf_spi_set_prescaler(spi_instance, SPI_BaudRatePrescaler_8);
    
    /* 标记为已初始化 */
    g_tf_spi_device.state = TF_SPI_STATE_INITIALIZED;
    
    TF_SPI_LOG_DEBUG("TF_SPI initialized: %d MB, %s", 
                     g_tf_spi_device.capacity_mb,
                     g_tf_spi_device.is_sdhc ? "SDHC/SDXC" : "SDSC");
    
    return TF_SPI_OK;
}

/**
 * @brief TF_SPI反初始化
 */
TF_SPI_Status_t TF_SPI_Deinit(void)
{
    SPI_Instance_t spi_instance = TF_SPI_SPI_INSTANCE;
    
    /* 清除初始化标志 */
    g_tf_spi_device.state = TF_SPI_STATE_UNINITIALIZED;
    g_tf_spi_device.capacity_mb = 0;
    g_tf_spi_device.block_count = 0;
    g_tf_spi_device.card_type = TF_SPI_CARD_TYPE_UNKNOWN;
    g_tf_spi_device.is_sdhc = 0;
    
    /* CS拉高 */
    tf_spi_cs_high(spi_instance);
    
    return TF_SPI_OK;
}

/**
 * @brief 获取TF_SPI设备信息
 */
const tf_spi_dev_t* TF_SPI_GetInfo(void)
{
    if (g_tf_spi_device.state != TF_SPI_STATE_INITIALIZED)
    {
        return NULL;
    }
    
    return &g_tf_spi_device;
}

/**
 * @brief 检查TF_SPI是否已初始化
 */
uint8_t TF_SPI_IsInitialized(void)
{
    return (g_tf_spi_device.state == TF_SPI_STATE_INITIALIZED) ? 1 : 0;
}

/**
 * @brief 读取单个块（扇区）
 */
TF_SPI_Status_t TF_SPI_ReadBlock(uint32_t block_addr, uint8_t *buf)
{
    SPI_Instance_t spi_instance = TF_SPI_SPI_INSTANCE;
    uint8_t response;
    uint32_t addr;
    
    /* ========== 参数校验 ========== */
    
    if (buf == NULL)
    {
        return TF_SPI_ERROR_NULL_PTR;
    }
    
    if (g_tf_spi_device.state != TF_SPI_STATE_INITIALIZED)
    {
        return TF_SPI_ERROR_NOT_INIT;
    }
    
    if (block_addr >= g_tf_spi_device.block_count)
    {
        return TF_SPI_ERROR_OUT_OF_BOUND;
    }
    
    /* ========== 读取块 ========== */
    
    /* 计算地址（SDHC/SDXC使用块地址，SDSC使用字节地址） */
    addr = tf_spi_block_to_addr(block_addr);
    
    /* 发送CMD17（读取单个块） */
    tf_spi_cs_low(spi_instance);
    response = tf_spi_send_cmd(spi_instance, TF_SPI_CMD_READ_SINGLE_BLOCK, addr);
    
    if (response != 0x00)
    {
        #if CONFIG_MODULE_LOG_ENABLED
        LOG_WARN("TF_SPI", "TF_SPI_ReadBlock: CMD17 failed, response=0x%02X, block_addr=%lu", 
                 response, (unsigned long)block_addr);
        #endif
        tf_spi_cs_high(spi_instance);
        tf_spi_send_dummy(spi_instance, 1);
        return TF_SPI_ERROR_CMD_FAILED;
    }
    
    /* 等待数据令牌 */
    #if CONFIG_MODULE_LOG_ENABLED
    LOG_DEBUG("TF_SPI", "TF_SPI_ReadBlock: waiting for data token, block_addr=%lu", (unsigned long)block_addr);
    #endif
    response = tf_spi_wait_response(spi_instance, TF_SPI_DEFAULT_TIMEOUT_MS);
    if (response != TF_SPI_TOKEN_START_BLOCK)
    {
        #if CONFIG_MODULE_LOG_ENABLED
        LOG_WARN("TF_SPI", "TF_SPI_ReadBlock: data token timeout or invalid, response=0x%02X, expected=0x%02X, block_addr=%lu", 
                 response, TF_SPI_TOKEN_START_BLOCK, (unsigned long)block_addr);
        #endif
        tf_spi_cs_high(spi_instance);
        tf_spi_send_dummy(spi_instance, 1);
        return TF_SPI_ERROR_CMD_FAILED;
    }
    
    /* 读取数据块（512字节） */
    #if CONFIG_MODULE_LOG_ENABLED
    LOG_DEBUG("TF_SPI", "TF_SPI_ReadBlock: reading data block, block_addr=%lu", (unsigned long)block_addr);
    #endif
    
    /* 对于512字节的大块数据，增加超时时间到2秒 */
    SPI_Status_t spi_status = SPI_MasterReceive(spi_instance, buf, TF_SPI_BLOCK_SIZE, 2000);
    
    if (spi_status != SPI_OK)
    {
        #if CONFIG_MODULE_LOG_ENABLED
        LOG_ERROR("TF_SPI", "TF_SPI_ReadBlock: SPI_MasterReceive failed, status=%d, block_addr=%lu", 
                  spi_status, (unsigned long)block_addr);
        #endif
        tf_spi_cs_high(spi_instance);
        tf_spi_send_dummy(spi_instance, 1);
        return TF_SPI_ERROR_CMD_FAILED;
    }
    #if CONFIG_MODULE_LOG_ENABLED
    LOG_DEBUG("TF_SPI", "TF_SPI_ReadBlock: data block read complete, block_addr=%lu", (unsigned long)block_addr);
    #endif
    
    /* 读取CRC（2字节，忽略） */
    tf_spi_send_dummy(spi_instance, 2);
    
    tf_spi_cs_high(spi_instance);
    tf_spi_send_dummy(spi_instance, 1);
    
    return TF_SPI_OK;
}

/**
 * @brief 写入单个块（扇区）
 */
TF_SPI_Status_t TF_SPI_WriteBlock(uint32_t block_addr, const uint8_t *buf)
{
    SPI_Instance_t spi_instance = TF_SPI_SPI_INSTANCE;
    uint8_t response;
    uint32_t addr;
    uint32_t start_tick;
    
    /* ========== 参数校验 ========== */
    
    if (buf == NULL)
    {
        return TF_SPI_ERROR_NULL_PTR;
    }
    
    if (g_tf_spi_device.state != TF_SPI_STATE_INITIALIZED)
    {
        return TF_SPI_ERROR_NOT_INIT;
    }
    
    if (block_addr >= g_tf_spi_device.block_count)
    {
        return TF_SPI_ERROR_OUT_OF_BOUND;
    }
    
    /* ========== 写入块 ========== */
    
    /* 计算地址（SDHC/SDXC使用块地址，SDSC使用字节地址） */
    addr = tf_spi_block_to_addr(block_addr);
    
    /* 发送CMD24（写入单个块） */
    tf_spi_cs_low(spi_instance);
    response = tf_spi_send_cmd(spi_instance, TF_SPI_CMD_WRITE_BLOCK, addr);
    
    if (response != 0x00)
    {
        tf_spi_cs_high(spi_instance);
        tf_spi_send_dummy(spi_instance, 1);
        return TF_SPI_ERROR_CMD_FAILED;
    }
    
    /* 发送数据开始令牌 */
    {
        uint8_t token = TF_SPI_TOKEN_START_BLOCK;
        SPI_Status_t spi_status = SPI_MasterTransmit(spi_instance, &token, 1, TF_SPI_DEFAULT_TIMEOUT_MS);
        if (spi_status != SPI_OK)
        {
            tf_spi_cs_high(spi_instance);
            tf_spi_send_dummy(spi_instance, 1);
            return TF_SPI_ERROR_CMD_FAILED;
        }
    }
    
    /* 发送数据块（512字节） */
    SPI_Status_t spi_status = SPI_MasterTransmit(spi_instance, buf, TF_SPI_BLOCK_SIZE, TF_SPI_DEFAULT_TIMEOUT_MS);
    if (spi_status != SPI_OK)
    {
        tf_spi_cs_high(spi_instance);
        tf_spi_send_dummy(spi_instance, 1);
        return TF_SPI_ERROR_CMD_FAILED;
    }
    
    /* 发送CRC（2字节，0xFF） */
    tf_spi_send_dummy(spi_instance, 2);
    
    /* 等待数据响应令牌 */
    response = tf_spi_wait_response(spi_instance, TF_SPI_DEFAULT_TIMEOUT_MS);
    
    if ((response & 0x1F) != TF_SPI_TOKEN_DATA_ACCEPTED)
    {
        tf_spi_cs_high(spi_instance);
        tf_spi_send_dummy(spi_instance, 1);
        
        if ((response & 0x1F) == TF_SPI_TOKEN_DATA_CRC_ERROR)
        {
            return TF_SPI_ERROR_CRC;
        }
        else if ((response & 0x1F) == TF_SPI_TOKEN_DATA_WRITE_ERROR)
        {
            return TF_SPI_ERROR_WRITE_PROTECT;
        }
        else
        {
            return TF_SPI_ERROR_CMD_FAILED;
        }
    }
    
    /* 等待写入完成（卡忙） */
    start_tick = Delay_GetTick();
    while (Delay_GetElapsed(Delay_GetTick(), start_tick) < TF_SPI_DEFAULT_TIMEOUT_MS)
    {
        response = 0xFF;
        SPI_Status_t spi_status = SPI_MasterTransmitReceive(spi_instance, &response, &response, 1, 0);
        
        /* SPI通信失败时继续等待，直到超时 */
        if (spi_status != SPI_OK)
        {
            continue;
        }
        
        if (response == 0xFF)
        {
            break;  /* 写入完成 */
        }
    }
    
    /* 检查是否超时 */
    if (response != 0xFF)
    {
        tf_spi_cs_high(spi_instance);
        tf_spi_send_dummy(spi_instance, 1);
        TF_SPI_LOG_DEBUG("Write block timeout waiting for card ready");
        return TF_SPI_ERROR_TIMEOUT;
    }
    
    tf_spi_cs_high(spi_instance);
    tf_spi_send_dummy(spi_instance, 1);
    
    return TF_SPI_OK;
}

/**
 * @brief 读取多个块（扇区）
 */
TF_SPI_Status_t TF_SPI_ReadBlocks(uint32_t block_addr, uint32_t block_count, uint8_t *buf)
{
    uint32_t i;
    TF_SPI_Status_t status;
    
    /* ========== 参数校验 ========== */
    
    if (buf == NULL)
    {
        return TF_SPI_ERROR_NULL_PTR;
    }
    
    if (block_count == 0)
    {
        return TF_SPI_ERROR_INVALID_PARAM;
    }
    
    if (g_tf_spi_device.state != TF_SPI_STATE_INITIALIZED)
    {
        return TF_SPI_ERROR_NOT_INIT;
    }
    
    if ((uint64_t)block_addr + (uint64_t)block_count > (uint64_t)g_tf_spi_device.block_count)
    {
        return TF_SPI_ERROR_OUT_OF_BOUND;
    }
    
    /* ========== 读取多个块 ========== */
    
    /* 逐个读取块 */
    for (i = 0; i < block_count; i++)
    {
        status = TF_SPI_ReadBlock(block_addr + i, buf + i * TF_SPI_BLOCK_SIZE);
        if (status != TF_SPI_OK)
        {
            return status;
        }
    }
    
    return TF_SPI_OK;
}

/**
 * @brief 写入多个块（扇区）
 */
TF_SPI_Status_t TF_SPI_WriteBlocks(uint32_t block_addr, uint32_t block_count, const uint8_t *buf)
{
    uint32_t i;
    TF_SPI_Status_t status;
    
    /* ========== 参数校验 ========== */
    
    if (buf == NULL)
    {
        return TF_SPI_ERROR_NULL_PTR;
    }
    
    if (block_count == 0)
    {
        return TF_SPI_ERROR_INVALID_PARAM;
    }
    
    if (g_tf_spi_device.state != TF_SPI_STATE_INITIALIZED)
    {
        return TF_SPI_ERROR_NOT_INIT;
    }
    
    if ((uint64_t)block_addr + (uint64_t)block_count > (uint64_t)g_tf_spi_device.block_count)
    {
        return TF_SPI_ERROR_OUT_OF_BOUND;
    }
    
    /* ========== 写入多个块 ========== */
    
    /* 逐个写入块 */
    for (i = 0; i < block_count; i++)
    {
        status = TF_SPI_WriteBlock(block_addr + i, buf + i * TF_SPI_BLOCK_SIZE);
        if (status != TF_SPI_OK)
        {
            return status;
        }
    }
    
    return TF_SPI_OK;
}

/**
 * @brief 发送状态查询命令（CMD13）
 */
TF_SPI_Status_t TF_SPI_SendStatus(uint8_t *status)
{
    SPI_Instance_t spi_instance = TF_SPI_SPI_INSTANCE;
    uint8_t response;
    
    /* ========== 参数校验 ========== */
    
    if (status == NULL)
    {
        return TF_SPI_ERROR_NULL_PTR;
    }
    
    /* 检查SPI模块是否已初始化 */
    if (!SPI_IsInitialized(spi_instance))
    {
        return TF_SPI_ERROR_INIT_FAILED;
    }
    
    /* ========== 发送CMD13 ========== */
    
    tf_spi_cs_low(spi_instance);
    response = tf_spi_send_cmd(spi_instance, 0x4D, 0);  /* CMD13 */
    tf_spi_cs_high(spi_instance);
    tf_spi_send_dummy(spi_instance, 1);
    
    *status = response;
    
    /* 检查响应是否有效 */
    if (response == 0xFF)
    {
        return TF_SPI_ERROR_CMD_FAILED;
    }
    
    return TF_SPI_OK;
}

/* ==================== 底层命令访问接口（教学/调试用） ==================== */

/**
 * @brief 发送SD命令（底层接口）
 */
TF_SPI_Status_t TF_SPI_SendCMD(uint8_t cmd, uint32_t arg, uint8_t *response)
{
    SPI_Instance_t spi_instance = TF_SPI_SPI_INSTANCE;
    uint8_t r1;
    
    /* ========== 参数校验 ========== */
    
    if (response == NULL)
    {
        return TF_SPI_ERROR_NULL_PTR;
    }
    
    if (cmd > 0x3F)
    {
        return TF_SPI_ERROR_INVALID_PARAM;
    }
    
    /* 检查SPI模块是否已初始化 */
    if (!SPI_IsInitialized(spi_instance))
    {
        return TF_SPI_ERROR_INIT_FAILED;
    }
    
    /* ========== 发送命令 ========== */
    
    tf_spi_cs_low(spi_instance);
    r1 = tf_spi_send_cmd(spi_instance, cmd, arg);
    tf_spi_cs_high(spi_instance);
    tf_spi_send_dummy(spi_instance, 1);
    
    *response = r1;
    
    return TF_SPI_OK;
}

/**
 * @brief 读取CSD寄存器（底层接口）
 */
TF_SPI_Status_t TF_SPI_ReadCSD(uint8_t *csd)
{
    SPI_Instance_t spi_instance = TF_SPI_SPI_INSTANCE;
    uint8_t response;
    
    /* ========== 参数校验 ========== */
    
    if (csd == NULL)
    {
        return TF_SPI_ERROR_NULL_PTR;
    }
    
    /* 检查SPI模块是否已初始化 */
    if (!SPI_IsInitialized(spi_instance))
    {
        return TF_SPI_ERROR_INIT_FAILED;
    }
    
    /* ========== 读取CSD ========== */
    
    tf_spi_cs_low(spi_instance);
    response = tf_spi_send_cmd(spi_instance, TF_SPI_CMD_SEND_CSD, 0);
    
    if (response != 0x00)
    {
        tf_spi_cs_high(spi_instance);
        tf_spi_send_dummy(spi_instance, 1);
        return TF_SPI_ERROR_CMD_FAILED;
    }
    
    /* 等待数据令牌 */
    response = tf_spi_wait_response(spi_instance, TF_SPI_DEFAULT_TIMEOUT_MS);
    if (response != TF_SPI_TOKEN_START_BLOCK)
    {
        tf_spi_cs_high(spi_instance);
        tf_spi_send_dummy(spi_instance, 1);
        return TF_SPI_ERROR_CMD_FAILED;
    }
    
    /* 读取CSD数据（16字节） */
    SPI_Status_t spi_status = SPI_MasterReceive(spi_instance, csd, 16, TF_SPI_DEFAULT_TIMEOUT_MS);
    if (spi_status != SPI_OK)
    {
        tf_spi_cs_high(spi_instance);
        tf_spi_send_dummy(spi_instance, 1);
        return TF_SPI_ERROR_CMD_FAILED;
    }
    
    /* 读取CRC（2字节，忽略） */
    tf_spi_send_dummy(spi_instance, 2);
    
    tf_spi_cs_high(spi_instance);
    tf_spi_send_dummy(spi_instance, 1);
    
    return TF_SPI_OK;
}

/**
 * @brief 读取CID寄存器（底层接口）
 */
TF_SPI_Status_t TF_SPI_ReadCID(uint8_t *cid)
{
    SPI_Instance_t spi_instance = TF_SPI_SPI_INSTANCE;
    uint8_t response;
    
    /* ========== 参数校验 ========== */
    
    if (cid == NULL)
    {
        return TF_SPI_ERROR_NULL_PTR;
    }
    
    /* 检查SPI模块是否已初始化 */
    if (!SPI_IsInitialized(spi_instance))
    {
        return TF_SPI_ERROR_INIT_FAILED;
    }
    
    /* ========== 读取CID ========== */
    
    tf_spi_cs_low(spi_instance);
    response = tf_spi_send_cmd(spi_instance, TF_SPI_CMD_SEND_CID, 0);
    
    if (response != 0x00)
    {
        tf_spi_cs_high(spi_instance);
        tf_spi_send_dummy(spi_instance, 1);
        return TF_SPI_ERROR_CMD_FAILED;
    }
    
    /* 等待数据令牌 */
    response = tf_spi_wait_response(spi_instance, TF_SPI_DEFAULT_TIMEOUT_MS);
    if (response != TF_SPI_TOKEN_START_BLOCK)
    {
        tf_spi_cs_high(spi_instance);
        tf_spi_send_dummy(spi_instance, 1);
        return TF_SPI_ERROR_CMD_FAILED;
    }
    
    /* 读取CID数据（16字节） */
    SPI_Status_t spi_status = SPI_MasterReceive(spi_instance, cid, 16, TF_SPI_DEFAULT_TIMEOUT_MS);
    if (spi_status != SPI_OK)
    {
        tf_spi_cs_high(spi_instance);
        tf_spi_send_dummy(spi_instance, 1);
        return TF_SPI_ERROR_CMD_FAILED;
    }
    
    /* 读取CRC（2字节，忽略） */
    tf_spi_send_dummy(spi_instance, 2);
    
    tf_spi_cs_high(spi_instance);
    tf_spi_send_dummy(spi_instance, 1);
    
    return TF_SPI_OK;
}

/**
 * @brief 读取OCR寄存器（底层接口）
 */
TF_SPI_Status_t TF_SPI_ReadOCR(uint32_t *ocr)
{
    SPI_Instance_t spi_instance = TF_SPI_SPI_INSTANCE;
    uint8_t response;
    uint8_t ocr_buf[4];
    
    /* ========== 参数校验 ========== */
    
    if (ocr == NULL)
    {
        return TF_SPI_ERROR_NULL_PTR;
    }
    
    /* 检查SPI模块是否已初始化 */
    if (!SPI_IsInitialized(spi_instance))
    {
        return TF_SPI_ERROR_INIT_FAILED;
    }
    
    /* ========== 读取OCR ========== */
    
    tf_spi_cs_low(spi_instance);
    response = tf_spi_send_cmd(spi_instance, TF_SPI_CMD_READ_OCR, 0);
    
    /* CMD58的响应：0x00表示READY_STATE，0x01表示IDLE_STATE，都是有效的 */
    /* 根据SD卡规范，即使卡处于IDLE_STATE，也可以读取OCR寄存器 */
    if (response != 0x00 && response != 0x01)
    {
        tf_spi_cs_high(spi_instance);
        tf_spi_send_dummy(spi_instance, 1);
        TF_SPI_LOG_DEBUG("CMD58 failed: response=0x%02X", response);
        return TF_SPI_ERROR_CMD_FAILED;
    }
    
    /* 读取OCR数据（4字节） */
    SPI_Status_t spi_status = SPI_MasterReceive(spi_instance, ocr_buf, 4, TF_SPI_DEFAULT_TIMEOUT_MS);
    if (spi_status != SPI_OK)
    {
        tf_spi_cs_high(spi_instance);
        tf_spi_send_dummy(spi_instance, 1);
        return TF_SPI_ERROR_CMD_FAILED;
    }
    
    tf_spi_cs_high(spi_instance);
    tf_spi_send_dummy(spi_instance, 1);
    
    /* 解析OCR值（大端序） */
    *ocr = ((uint32_t)ocr_buf[0] << 24) | ((uint32_t)ocr_buf[1] << 16) | 
           ((uint32_t)ocr_buf[2] << 8) | ocr_buf[3];
    
    return TF_SPI_OK;
}

/**
 * @brief 手动初始化后设置设备信息和状态（底层接口）
 */
TF_SPI_Status_t TF_SPI_SetDeviceInfoFromCSD(const uint8_t *csd, uint32_t ocr)
{
    SPI_Instance_t spi_instance = TF_SPI_SPI_INSTANCE;
    uint8_t response;
    uint32_t c_size;
    uint8_t csd_structure;
    
    /* ========== 参数校验 ========== */
    
    if (csd == NULL)
    {
        return TF_SPI_ERROR_NULL_PTR;
    }
    
    /* 检查SPI模块是否已初始化 */
    if (!SPI_IsInitialized(spi_instance))
    {
        return TF_SPI_ERROR_INIT_FAILED;
    }
    
    /* ========== 解析CSD结构 ========== */
    
    csd_structure = (csd[0] >> 6) & 0x03;
    
    if (csd_structure == 0)  /* CSD版本1.0（SDSC） */
    {
        /* 计算容量：C_SIZE = (csd[6] & 0x03) << 10 | csd[7] << 2 | (csd[8] >> 6) & 0x03 */
        c_size = ((uint32_t)(csd[6] & 0x03) << 10) | ((uint32_t)csd[7] << 2) | ((csd[8] >> 6) & 0x03);
        
        /* 容量 = (C_SIZE + 1) * 512 * 2^(C_SIZE_MULT + 2) 字节 */
        /* C_SIZE_MULT = ((csd[9] & 0x03) << 1) | ((csd[10] >> 7) & 0x01) */
        uint8_t c_size_mult = ((csd[9] & 0x03) << 1) | ((csd[10] >> 7) & 0x01);
        uint32_t read_bl_len = csd[5] & 0x0F;  /* READ_BL_LEN */
        
        /* 使用64位运算防止溢出 */
        uint64_t capacity_bytes_64 = ((uint64_t)(c_size + 1)) * (1ULL << (c_size_mult + 2)) * (1ULL << read_bl_len);
        
        /* 保存容量信息（使用64位计算，然后转换为32位） */
        uint64_t capacity_mb_64 = capacity_bytes_64 / (1024ULL * 1024ULL);
        uint64_t block_count_64 = capacity_bytes_64 / TF_SPI_BLOCK_SIZE;
        
        /* 检查是否超出32位范围 */
        if (capacity_mb_64 > UINT32_MAX || block_count_64 > UINT32_MAX)
        {
            return TF_SPI_ERROR_CMD_FAILED;
        }
        
        g_tf_spi_device.capacity_mb = (uint32_t)capacity_mb_64;
        g_tf_spi_device.block_count = (uint32_t)block_count_64;
        
        g_tf_spi_device.card_type = TF_SPI_CARD_TYPE_SDSC;
        g_tf_spi_device.is_sdhc = 0;
    }
    else if (csd_structure == 1)  /* CSD版本2.0（SDHC/SDXC） */
    {
        /* 计算容量：C_SIZE = (csd[7] & 0x3F) << 16 | csd[8] << 8 | csd[9] */
        c_size = ((uint32_t)(csd[7] & 0x3F) << 16) | ((uint32_t)csd[8] << 8) | csd[9];
        
        /* 容量 = (C_SIZE + 1) * 512KB，使用64位运算防止溢出 */
        uint64_t capacity_bytes_64 = ((uint64_t)(c_size + 1)) * 512ULL * 1024ULL;
        
        /* 根据容量判断卡类型（使用64位比较） */
        if (capacity_bytes_64 >= 32ULL * 1024ULL * 1024ULL * 1024ULL)  /* >= 32GB */
        {
            g_tf_spi_device.card_type = TF_SPI_CARD_TYPE_SDXC;
        }
        else
        {
            g_tf_spi_device.card_type = TF_SPI_CARD_TYPE_SDHC;
        }
        g_tf_spi_device.is_sdhc = 1;
        
        /* 保存容量信息（使用64位计算，然后转换为32位） */
        uint64_t capacity_mb_64 = capacity_bytes_64 / (1024ULL * 1024ULL);
        uint64_t block_count_64 = capacity_bytes_64 / TF_SPI_BLOCK_SIZE;
        
        /* 检查是否超出32位范围，如果超出则限制 */
        if (capacity_mb_64 > UINT32_MAX)
        {
            g_tf_spi_device.capacity_mb = UINT32_MAX;
        }
        else
        {
            g_tf_spi_device.capacity_mb = (uint32_t)capacity_mb_64;
        }
        
        if (block_count_64 > UINT32_MAX)
        {
            g_tf_spi_device.block_count = UINT32_MAX;
        }
        else
        {
            g_tf_spi_device.block_count = (uint32_t)block_count_64;
        }
    }
    else
    {
        return TF_SPI_ERROR_CMD_FAILED;
    }
    
    /* 如果是SDSC，发送CMD16设置块长度为512字节 */
    if (!g_tf_spi_device.is_sdhc)
    {
        tf_spi_cs_low(spi_instance);
        response = tf_spi_send_cmd(spi_instance, TF_SPI_CMD_SET_BLOCKLEN, TF_SPI_BLOCK_SIZE);
        tf_spi_cs_high(spi_instance);
        tf_spi_send_dummy(spi_instance, 1);
        
        if (response != 0x00)
        {
            return TF_SPI_ERROR_CMD_FAILED;
        }
    }
    
    /* 标记为已初始化 */
    g_tf_spi_device.state = TF_SPI_STATE_INITIALIZED;
    
    return TF_SPI_OK;
}

#endif /* CONFIG_MODULE_SPI_ENABLED */
#endif /* CONFIG_MODULE_TF_SPI_ENABLED */
