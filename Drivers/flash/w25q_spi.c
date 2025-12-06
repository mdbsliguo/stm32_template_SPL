/**
 * @file w25q_spi.c
 * @brief W25Q SPI Flash驱动模块实现
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的W25Q系列SPI Flash驱动，支持自动识别、4字节模式管理、统一地址接口
 * 
 * @note 设计约束：
 * - 状态集中管理：所有容量差异信息保存在全局结构体 g_w25q_device 中
 * - 接口纯净性：API参数仅限 uint32_t addr，禁止传入sector_idx或page_num
 * - 失败熔断机制：初始化失败后所有API返回 W25Q_ERROR_NOT_INIT
 * - 64位地址运算：所有 addr + len 计算必须强制转换为 uint64_t
 * - 4字节模式非易失性：≥128Mb芯片每次初始化必须重新发送0xB7指令
 */

#include "w25q_spi.h"
#include "config.h"
#include "delay.h"
#include <stddef.h>
#include <string.h>

#if CONFIG_MODULE_LOG_ENABLED
#include "log.h"
/* 调试输出：默认禁用，需要调试时可启用 */
#ifndef W25Q_DEBUG_ENABLED
#define W25Q_DEBUG_ENABLED 0
#endif

#if W25Q_DEBUG_ENABLED
#define W25Q_LOG_DEBUG(fmt, ...) LOG_INFO("W25Q", fmt, ##__VA_ARGS__)
#else
#define W25Q_LOG_DEBUG(fmt, ...) ((void)0)
#endif
#else
#define W25Q_LOG_DEBUG(fmt, ...) ((void)0)
#endif

#if CONFIG_MODULE_W25Q_ENABLED

#if CONFIG_MODULE_SPI_ENABLED

/* ==================== 内部常量定义 ==================== */

/* W25Q指令定义 */
#define W25Q_CMD_READ_ID           0x9F    /**< 读取ID */
#define W25Q_CMD_READ_DATA_3BYTE   0x03    /**< 读取数据（3字节地址） */
#define W25Q_CMD_READ_DATA_4BYTE   0x13    /**< 读取数据（4字节地址） */
#define W25Q_CMD_PAGE_PROG_3BYTE   0x02    /**< 页编程（3字节地址） */
#define W25Q_CMD_PAGE_PROG_4BYTE   0x12    /**< 页编程（4字节地址） */
#define W25Q_CMD_SECTOR_ERASE_3BYTE 0x20  /**< 扇区擦除（3字节地址） */
#define W25Q_CMD_SECTOR_ERASE_4BYTE 0x21  /**< 扇区擦除（4字节地址） */
#define W25Q_CMD_CHIP_ERASE        0xC7   /**< 整片擦除 */
#define W25Q_CMD_READ_STATUS_REG1   0x05  /**< 读状态寄存器1 */
#define W25Q_CMD_READ_STATUS_REG2   0x35  /**< 读状态寄存器2（块保护状态） */
#define W25Q_CMD_READ_STATUS_REG3   0x15  /**< 读状态寄存器3（4字节模式标志） */
#define W25Q_CMD_WRITE_ENABLE       0x06  /**< 写使能 */
#define W25Q_CMD_WRITE_STATUS_REG   0x01  /**< 写状态寄存器（清除块保护） */
#define W25Q_CMD_ENTER_4BYTE_MODE   0xB7  /**< 进入4字节模式 */
#define W25Q_CMD_EXIT_4BYTE_MODE    0xE9  /**< 退出4字节模式 */

/* 状态寄存器位定义 */
#define W25Q_STATUS_BUSY           0x01   /**< 忙标志位（bit 0） */
#define W25Q_STATUS_WEL            0x02   /**< 写使能锁存（bit 1） */
#define W25Q_STATUS_REG3_ADDR_MOD  0x80   /**< 地址模式位（bit 7，S23） */

/* 页大小和扇区大小 */
#define W25Q_PAGE_SIZE              256   /**< 页大小（字节） */
#define W25Q_SECTOR_SIZE             4096  /**< 扇区大小（字节） */

/* 默认超时时间 */
#define W25Q_DEFAULT_TIMEOUT_MS     1000  /**< 默认超时时间（毫秒） */

/* 型号定义（用于编译时优化） */
#define W25Q_MODEL_W25Q16           0xEF4015  /**< W25Q16: 0xEF 0x40 0x15 */
#define W25Q_MODEL_W25Q32           0xEF4016  /**< W25Q32: 0xEF 0x40 0x16 */
#define W25Q_MODEL_W25Q64           0xEF4017  /**< W25Q64: 0xEF 0x40 0x17 */
#define W25Q_MODEL_W25Q128          0xEF4018  /**< W25Q128: 0xEF 0x40 0x18 */
#define W25Q_MODEL_W25Q256          0xEF4019  /**< W25Q256: 0xEF 0x40 0x19 */
#define W25Q_MODEL_GD25Q64          0xC84017  /**< GD25Q64: 0xC8 0x40 0x17 */

/* ==================== 全局状态变量（唯一真理源） ==================== */

/**
 * @brief W25Q设备全局状态结构体
 * @note 所有容量差异信息必须保存在此结构体中，禁止分散存储
 */
static w25q_dev_t g_w25q_device = {
    .capacity_mb = 0,
    .addr_bytes = 0,
    .is_4byte_mode = 0,
    .state = W25Q_STATE_UNINITIALIZED,
    .manufacturer_id = 0,
    .device_id = 0
};

/* ==================== 型号识别表 ==================== */

/**
 * @brief W25Q型号信息结构体
 */
typedef struct {
    uint32_t device_id;      /**< 设备ID（制造商ID << 16 | 设备ID） */
    uint32_t capacity_mb;    /**< 容量（MB） */
    uint8_t addr_bytes;      /**< 地址字节数（3或4） */
    uint8_t need_4byte_mode; /**< 是否需要4字节模式（1=需要，0=不需要） */
} w25q_model_info_t;

/**
 * @brief W25Q型号识别表
 */
static const w25q_model_info_t w25q_model_table[] = {
    {W25Q_MODEL_W25Q16,  2,  3, 0},  /* W25Q16: 2MB, 3字节地址 */
    {W25Q_MODEL_W25Q32,  4,  3, 0},  /* W25Q32: 4MB, 3字节地址 */
    {W25Q_MODEL_W25Q64,  8,  3, 0},  /* W25Q64: 8MB, 3字节地址 */
    {W25Q_MODEL_GD25Q64, 8,  3, 0},  /* GD25Q64: 8MB, 3字节地址（兼容W25Q64） */
    {W25Q_MODEL_W25Q128, 16, 4, 1},  /* W25Q128: 16MB, 4字节地址，需要4字节模式 */
    {W25Q_MODEL_W25Q256, 32, 4, 1},  /* W25Q256: 32MB, 4字节地址，需要4字节模式 */
};

#define W25Q_MODEL_TABLE_SIZE (sizeof(w25q_model_table) / sizeof(w25q_model_info_t))

/* ==================== 内部静态函数 ==================== */

/**
 * @brief 发送地址（根据addr_bytes自动选择3或4字节）
 * @param[in] addr 地址
 * @return W25Q_Status_t 错误码
 */
static W25Q_Status_t w25q_send_address(uint32_t addr)
{
    SPI_Instance_t spi_instance = W25Q_SPI_INSTANCE;
    SPI_Status_t spi_status;
    uint8_t addr_bytes[4];
    
    /* 检查addr_bytes有效性（防御性编程） */
    if (g_w25q_device.addr_bytes != 3 && g_w25q_device.addr_bytes != 4)
    {
        return W25Q_ERROR_INIT_FAILED;
    }
    
    /* 准备地址字节 */
    if (g_w25q_device.addr_bytes == 4)
    {
        /* 4字节地址模式：addr[31:24], addr[23:16], addr[15:8], addr[7:0] */
        addr_bytes[0] = (uint8_t)(addr >> 24);
        addr_bytes[1] = (uint8_t)(addr >> 16);
        addr_bytes[2] = (uint8_t)(addr >> 8);
        addr_bytes[3] = (uint8_t)(addr);
    }
    else
    {
        /* 3字节地址模式：addr[23:16], addr[15:8], addr[7:0] */
        addr_bytes[0] = (uint8_t)(addr >> 16);
        addr_bytes[1] = (uint8_t)(addr >> 8);
        addr_bytes[2] = (uint8_t)(addr);
    }
    
    /* 参考简单案例：逐个字节发送地址（而不是批量传输） */
    if (g_w25q_device.addr_bytes == 4)
    {
        /* 4字节地址模式：逐个发送 */
        spi_status = SPI_MasterTransmitByte(spi_instance, addr_bytes[0], 100);
        if (spi_status != SPI_OK) return W25Q_ERROR_INIT_FAILED;
        spi_status = SPI_MasterTransmitByte(spi_instance, addr_bytes[1], 100);
        if (spi_status != SPI_OK) return W25Q_ERROR_INIT_FAILED;
        spi_status = SPI_MasterTransmitByte(spi_instance, addr_bytes[2], 100);
        if (spi_status != SPI_OK) return W25Q_ERROR_INIT_FAILED;
        spi_status = SPI_MasterTransmitByte(spi_instance, addr_bytes[3], 100);
        if (spi_status != SPI_OK) return W25Q_ERROR_INIT_FAILED;
    }
    else
    {
        /* 3字节地址模式：逐个发送 */
        spi_status = SPI_MasterTransmitByte(spi_instance, addr_bytes[0], 100);
        if (spi_status != SPI_OK) return W25Q_ERROR_INIT_FAILED;
        spi_status = SPI_MasterTransmitByte(spi_instance, addr_bytes[1], 100);
        if (spi_status != SPI_OK) return W25Q_ERROR_INIT_FAILED;
        spi_status = SPI_MasterTransmitByte(spi_instance, addr_bytes[2], 100);
        if (spi_status != SPI_OK) return W25Q_ERROR_INIT_FAILED;
    }
    
    return W25Q_OK;
}

/**
 * @brief 读取状态寄存器1
 * @param[out] status 状态寄存器值
 * @return W25Q_Status_t 错误码
 */
static W25Q_Status_t w25q_read_status_reg1(uint8_t *status)
{
    SPI_Instance_t spi_instance = W25Q_SPI_INSTANCE;
    SPI_Status_t spi_status;
    
    if (status == NULL)
    {
        return W25Q_ERROR_INIT_FAILED;
    }
    
    SPI_NSS_Low(spi_instance);
    
    spi_status = SPI_MasterTransmitByte(spi_instance, W25Q_CMD_READ_STATUS_REG1, 100);
    if (spi_status != SPI_OK)
    {
        SPI_NSS_High(spi_instance);
        return W25Q_ERROR_INIT_FAILED;
    }
    
    spi_status = SPI_MasterReceiveByte(spi_instance, status, 100);
    
    SPI_NSS_High(spi_instance);
    
    if (spi_status != SPI_OK)
    {
        return W25Q_ERROR_INIT_FAILED;
    }
    
    return W25Q_OK;
}

/**
 * @brief 读取状态寄存器3（用于检查4字节模式）
 * @param[out] status 状态寄存器值
 * @return W25Q_Status_t 错误码
 */
static W25Q_Status_t w25q_read_status_reg3(uint8_t *status)
{
    SPI_Instance_t spi_instance = W25Q_SPI_INSTANCE;
    SPI_Status_t spi_status;
    
    if (status == NULL)
    {
        return W25Q_ERROR_INIT_FAILED;
    }
    
    SPI_NSS_Low(spi_instance);
    
    spi_status = SPI_MasterTransmitByte(spi_instance, W25Q_CMD_READ_STATUS_REG3, 100);
    if (spi_status != SPI_OK)
    {
        SPI_NSS_High(spi_instance);
        return W25Q_ERROR_INIT_FAILED;
    }
    
    spi_status = SPI_MasterReceiveByte(spi_instance, status, 100);
    
    SPI_NSS_High(spi_instance);
    
    if (spi_status != SPI_OK)
    {
        return W25Q_ERROR_INIT_FAILED;
    }
    
    return W25Q_OK;
}

/**
 * @brief 写使能
 * @return W25Q_Status_t 错误码
 */
static W25Q_Status_t w25q_write_enable(void)
{
    SPI_Instance_t spi_instance = W25Q_SPI_INSTANCE;
    SPI_Status_t spi_status;
    W25Q_Status_t w25q_status;
    uint8_t status_reg;
    uint8_t retry_count;
    
    /* 发送写使能命令 */
    SPI_NSS_Low(spi_instance);
    spi_status = SPI_MasterTransmitByte(spi_instance, W25Q_CMD_WRITE_ENABLE, 100);
    SPI_NSS_High(spi_instance);
    
    if (spi_status != SPI_OK)
    {
        return W25Q_ERROR_INIT_FAILED;
    }
    
    /* 工程模板：写使能后需要短暂延迟，确保Flash处理命令 */
    /* 不同型号Flash的响应时间可能不同，延迟确保兼容性 */
    Delay_us(5);
    
    /* 验证WEL位是否被设置（最多重试3次） */
    /* 工程模板：验证WEL位确保写使能真正生效，提高可靠性 */
    for (retry_count = 0; retry_count < 3; retry_count++)
    {
        w25q_status = w25q_read_status_reg1(&status_reg);
        if (w25q_status != W25Q_OK)
        {
            return w25q_status;
        }
        
        if ((status_reg & W25Q_STATUS_WEL) != 0)
        {
            /* WEL位已设置，写使能成功 */
            return W25Q_OK;
        }
        
        /* WEL位未设置，等待后重试 */
        Delay_us(10);
    }
    
    /* 重试3次后WEL位仍未设置，写使能失败 */
    return W25Q_ERROR_INIT_FAILED;
}

/**
 * @brief 进入4字节模式
 * @return W25Q_Status_t 错误码
 */
static W25Q_Status_t w25q_enter_4byte_mode(void)
{
    SPI_Instance_t spi_instance = W25Q_SPI_INSTANCE;
    SPI_Status_t spi_status;
    uint8_t status_reg3;
    W25Q_Status_t w25q_status;
    
    SPI_NSS_Low(spi_instance);
    
    spi_status = SPI_MasterTransmitByte(spi_instance, W25Q_CMD_ENTER_4BYTE_MODE, 100);
    
    SPI_NSS_High(spi_instance);
    
    if (spi_status != SPI_OK)
    {
        return W25Q_ERROR_4BYTE_MODE_FAIL;
    }
    
    /* 工程模板：等待模式切换完成，确保Flash处理命令 */
    /* 不同型号Flash的响应时间可能不同，延迟确保兼容性 */
    Delay_us(10);
    
    /* 验证4字节模式：读取状态寄存器3，检查S23位（ADDR_MOD） */
    w25q_status = w25q_read_status_reg3(&status_reg3);
    if (w25q_status != W25Q_OK)
    {
        return W25Q_ERROR_4BYTE_MODE_FAIL;
    }
    
    if ((status_reg3 & W25Q_STATUS_REG3_ADDR_MOD) == 0)
    {
        /* S23位为0，表示未进入4字节模式 */
        return W25Q_ERROR_4BYTE_MODE_FAIL;
    }
    
    return W25Q_OK;
}

/**
 * @brief 根据设备ID识别型号
 * @param[in] manufacturer_id 制造商ID
 * @param[in] device_id 设备ID
 * @return const w25q_model_info_t* 型号信息指针，未找到返回NULL
 */
static const w25q_model_info_t* w25q_identify_model(uint16_t manufacturer_id, uint16_t device_id)
{
    uint32_t full_device_id = ((uint32_t)manufacturer_id << 16) | device_id;
    uint32_t i;
    
    for (i = 0; i < W25Q_MODEL_TABLE_SIZE; i++)
    {
        if (w25q_model_table[i].device_id == full_device_id)
        {
            return &w25q_model_table[i];
        }
    }
    
    return NULL;
}

/* ==================== 公共API实现 ==================== */

/**
 * @brief W25Q初始化
 */
W25Q_Status_t W25Q_Init(void)
{
    SPI_Instance_t spi_instance = W25Q_SPI_INSTANCE;
    SPI_Status_t spi_status;
    W25Q_Status_t w25q_status;
    uint8_t manufacturer_id;
    uint8_t memory_type;
    uint8_t capacity_id;
    const w25q_model_info_t *model_info;
    uint8_t status_reg3;
    
    /* 检查是否已初始化 */
    if (g_w25q_device.state == W25Q_STATE_INITIALIZED)
    {
        return W25Q_OK;
    }
    
    /* 初始化SPI外设 */
    spi_status = SPI_HW_Init(spi_instance);
    if (spi_status != SPI_OK)
    {
        g_w25q_device.state = W25Q_STATE_UNINITIALIZED;
        return W25Q_ERROR_INIT_FAILED;
    }
    
    /* 确保CS引脚被正确配置为输出，初始状态为高（释放） */
    /* 注意：SPI_HW_Init()应该已经配置了CS引脚，但这里再次确认CS为高电平 */
    /* 这可以避免W25Q模块在CS浮空时输出中间电压（如1.1V） */
    SPI_NSS_High(spi_instance);
    Delay_us(10);  /* 短暂延时确保CS稳定 */
    
    /* 等待Flash就绪 */
    Delay_ms(10);
    
    /* 读取设备ID（0x9F指令） */
    SPI_NSS_Low(spi_instance);
    
    spi_status = SPI_MasterTransmitByte(spi_instance, W25Q_CMD_READ_ID, 100);
    if (spi_status != SPI_OK)
    {
        SPI_NSS_High(spi_instance);
        g_w25q_device.state = W25Q_STATE_UNINITIALIZED;
        return W25Q_ERROR_INIT_FAILED;
    }
    
    spi_status = SPI_MasterReceiveByte(spi_instance, &manufacturer_id, 100);
    if (spi_status != SPI_OK)
    {
        SPI_NSS_High(spi_instance);
        g_w25q_device.state = W25Q_STATE_UNINITIALIZED;
        return W25Q_ERROR_INIT_FAILED;
    }
    
    spi_status = SPI_MasterReceiveByte(spi_instance, &memory_type, 100);
    if (spi_status != SPI_OK)
    {
        SPI_NSS_High(spi_instance);
        g_w25q_device.state = W25Q_STATE_UNINITIALIZED;
        return W25Q_ERROR_INIT_FAILED;
    }
    
    spi_status = SPI_MasterReceiveByte(spi_instance, &capacity_id, 100);
    if (spi_status != SPI_OK)
    {
        SPI_NSS_High(spi_instance);
        g_w25q_device.state = W25Q_STATE_UNINITIALIZED;
        return W25Q_ERROR_INIT_FAILED;
    }
    
    SPI_NSS_High(spi_instance);
    
    /* 编译时优化：如果定义了W25Q_FIXED_MODEL，跳过ID识别 */
#ifdef W25Q_FIXED_MODEL
    /* 根据固定型号直接填充信息 */
    #if W25Q_FIXED_MODEL == W25Q_MODEL_W25Q64
        g_w25q_device.manufacturer_id = manufacturer_id;
        g_w25q_device.device_id = (memory_type << 8) | capacity_id;
        g_w25q_device.capacity_mb = 8;
        g_w25q_device.addr_bytes = 3;
        g_w25q_device.is_4byte_mode = 0;
    #elif W25Q_FIXED_MODEL == W25Q_MODEL_W25Q256
        g_w25q_device.manufacturer_id = manufacturer_id;
        g_w25q_device.device_id = (memory_type << 8) | capacity_id;
        g_w25q_device.capacity_mb = 32;
        g_w25q_device.addr_bytes = 4;
        g_w25q_device.is_4byte_mode = 1;
    #elif W25Q_FIXED_MODEL == W25Q_MODEL_W25Q128
        g_w25q_device.manufacturer_id = manufacturer_id;
        g_w25q_device.device_id = (memory_type << 8) | capacity_id;
        g_w25q_device.capacity_mb = 16;
        g_w25q_device.addr_bytes = 4;
        g_w25q_device.is_4byte_mode = 1;
    #elif W25Q_FIXED_MODEL == W25Q_MODEL_W25Q32
        g_w25q_device.manufacturer_id = manufacturer_id;
        g_w25q_device.device_id = (memory_type << 8) | capacity_id;
        g_w25q_device.capacity_mb = 4;
        g_w25q_device.addr_bytes = 3;
        g_w25q_device.is_4byte_mode = 0;
    #elif W25Q_FIXED_MODEL == W25Q_MODEL_W25Q16
        g_w25q_device.manufacturer_id = manufacturer_id;
        g_w25q_device.device_id = (memory_type << 8) | capacity_id;
        g_w25q_device.capacity_mb = 2;
        g_w25q_device.addr_bytes = 3;
        g_w25q_device.is_4byte_mode = 0;
    #elif W25Q_FIXED_MODEL == W25Q_MODEL_GD25Q64
        g_w25q_device.manufacturer_id = manufacturer_id;
        g_w25q_device.device_id = (memory_type << 8) | capacity_id;
        g_w25q_device.capacity_mb = 8;
        g_w25q_device.addr_bytes = 3;
        g_w25q_device.is_4byte_mode = 0;
    #else
        /* 其他固定型号：使用运行时识别 */
        model_info = w25q_identify_model(manufacturer_id, (memory_type << 8) | capacity_id);
        if (model_info == NULL)
        {
            g_w25q_device.state = W25Q_STATE_UNINITIALIZED;
            return W25Q_ERROR_ID_MISMATCH;
        }
        g_w25q_device.manufacturer_id = manufacturer_id;
        g_w25q_device.device_id = (memory_type << 8) | capacity_id;
        g_w25q_device.capacity_mb = model_info->capacity_mb;
        g_w25q_device.addr_bytes = model_info->addr_bytes;
        g_w25q_device.is_4byte_mode = model_info->need_4byte_mode;
    #endif
#else
    /* 运行时识别：根据ID查找型号 */
    model_info = w25q_identify_model(manufacturer_id, (memory_type << 8) | capacity_id);
    if (model_info == NULL)
    {
        g_w25q_device.state = W25Q_STATE_UNINITIALIZED;
        return W25Q_ERROR_ID_MISMATCH;
    }
    
    /* 填充设备信息 */
    g_w25q_device.manufacturer_id = manufacturer_id;
    g_w25q_device.device_id = (memory_type << 8) | capacity_id;
    g_w25q_device.capacity_mb = model_info->capacity_mb;
    g_w25q_device.addr_bytes = model_info->addr_bytes;
    g_w25q_device.is_4byte_mode = model_info->need_4byte_mode;
#endif
    
    /* 对于≥128Mb芯片，进入4字节模式 */
    if (g_w25q_device.is_4byte_mode)
    {
        w25q_status = w25q_enter_4byte_mode();
        if (w25q_status != W25Q_OK)
        {
            g_w25q_device.state = W25Q_STATE_UNINITIALIZED;
            return W25Q_ERROR_4BYTE_MODE_FAIL;
        }
        
        /* 再次验证4字节模式 */
        w25q_status = w25q_read_status_reg3(&status_reg3);
        if (w25q_status != W25Q_OK)
        {
            g_w25q_device.state = W25Q_STATE_UNINITIALIZED;
            return W25Q_ERROR_4BYTE_MODE_FAIL;
        }
        
        if ((status_reg3 & W25Q_STATUS_REG3_ADDR_MOD) == 0)
        {
            g_w25q_device.state = W25Q_STATE_UNINITIALIZED;
            return W25Q_ERROR_4BYTE_MODE_FAIL;
        }
    }
    
    /* 检查并清除块保护（如果启用） */
    {
        uint8_t status_reg1;
        w25q_status = w25q_read_status_reg1(&status_reg1);
        if (w25q_status == W25Q_OK)
        {
            uint8_t bp_bits = (status_reg1 >> 2) & 0x0F;
            if (bp_bits != 0)
            {
                /* 块保护已启用，需要清除 */
                /* 写使能 */
                w25q_status = w25q_write_enable();
                if (w25q_status == W25Q_OK)
                {
                    /* 写状态寄存器清除块保护位（BP[3:0] = 0） */
                    SPI_NSS_Low(spi_instance);
                    spi_status = SPI_MasterTransmitByte(spi_instance, W25Q_CMD_WRITE_STATUS_REG, 100);
                    if (spi_status == SPI_OK)
                    {
                        /* 写入状态寄存器1：清除BP位，保留其他位 */
                        uint8_t new_status = status_reg1 & ~0x3C;  /* 清除bit 2-5 (BP位) */
                        spi_status = SPI_MasterTransmitByte(spi_instance, new_status, 100);
                    }
                    SPI_NSS_High(spi_instance);
                    
                    if (spi_status == SPI_OK)
                    {
                        /* 等待写入完成 */
                        w25q_status = W25Q_WaitReady(0);
                        if (w25q_status == W25Q_OK)
                        {
                            /* 验证块保护是否已清除 */
                            w25q_status = w25q_read_status_reg1(&status_reg1);
                            if (w25q_status == W25Q_OK)
                            {
                                bp_bits = (status_reg1 >> 2) & 0x0F;
                                if (bp_bits != 0)
                                {
                                    /* 块保护清除失败，记录错误 */
                                    #if CONFIG_MODULE_LOG_ENABLED
                                    LOG_ERROR("W25Q", "Block Protection clear failed (BP[3:0]=0x%X)", bp_bits);
                                    #endif
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    /* 设置初始化状态 */
    g_w25q_device.state = W25Q_STATE_INITIALIZED;
    
    return W25Q_OK;
}

/**
 * @brief W25Q反初始化
 */
W25Q_Status_t W25Q_Deinit(void)
{
    g_w25q_device.state = W25Q_STATE_UNINITIALIZED;
    memset(&g_w25q_device, 0, sizeof(g_w25q_device));
    return W25Q_OK;
}

/**
 * @brief 获取W25Q设备信息（只读指针）
 */
const w25q_dev_t* W25Q_GetInfo(void)
{
    if (g_w25q_device.state == W25Q_STATE_INITIALIZED)
    {
        return &g_w25q_device;
    }
    return NULL;
}

/**
 * @brief 检查W25Q是否已初始化
 */
uint8_t W25Q_IsInitialized(void)
{
    return (g_w25q_device.state == W25Q_STATE_INITIALIZED) ? 1 : 0;
}

/**
 * @brief 等待Flash就绪
 */
W25Q_Status_t W25Q_WaitReady(uint32_t timeout_ms)
{
    uint8_t status;
    uint32_t start_tick;
    uint32_t elapsed;
    W25Q_Status_t w25q_status;
    
    if (g_w25q_device.state != W25Q_STATE_INITIALIZED)
    {
        return W25Q_ERROR_NOT_INIT;
    }
    
    if (timeout_ms == 0)
    {
        timeout_ms = W25Q_DEFAULT_TIMEOUT_MS;
    }
    
    /* 动态超时补偿：Gb级芯片使用更长超时时间 */
    if (g_w25q_device.capacity_mb >= 16)
    {
        /* Gb级芯片：增加超时时间 */
        timeout_ms = timeout_ms * 2;
    }
    
    start_tick = Delay_GetTick();
    
    while (1)
    {
        w25q_status = w25q_read_status_reg1(&status);
        if (w25q_status != W25Q_OK)
        {
            return w25q_status;
        }
        
        if ((status & W25Q_STATUS_BUSY) == 0)
        {
            /* 就绪 */
            return W25Q_OK;
        }
        
        /* 检查超时（优化：只调用一次Delay_GetTick） */
        {
            uint32_t current_tick = Delay_GetTick();
            elapsed = Delay_GetElapsed(current_tick, start_tick);
            if (elapsed >= timeout_ms)
            {
                return W25Q_ERROR_TIMEOUT;
            }
        }
        
        Delay_us(100);
    }
}

/**
 * @brief 读取数据
 */
W25Q_Status_t W25Q_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    SPI_Instance_t spi_instance = W25Q_SPI_INSTANCE;
    SPI_Status_t spi_status;
    W25Q_Status_t w25q_status;
    uint64_t capacity_bytes;
    
    if (g_w25q_device.state != W25Q_STATE_INITIALIZED)
    {
        return W25Q_ERROR_NOT_INIT;
    }
    
    if (buf == NULL)
    {
        return W25Q_ERROR_INIT_FAILED;
    }
    
    if (len == 0)
    {
        return W25Q_OK;
    }
    
    /* 64位地址运算：检查地址越界 */
    capacity_bytes = (uint64_t)g_w25q_device.capacity_mb * 1024ULL * 1024ULL;
    if ((uint64_t)addr + (uint64_t)len > capacity_bytes)
    {
        return W25Q_ERROR_OUT_OF_BOUND;
    }
    
    /* 读取操作不需要等待Flash就绪（读取不会触发编程/擦除） */
    /* w25q_status = W25Q_WaitReady(0); */  /* 已移除以提高性能 */
    
    SPI_NSS_Low(spi_instance);
    
    /* 发送读取指令 */
    if (g_w25q_device.addr_bytes == 4)
    {
        spi_status = SPI_MasterTransmitByte(spi_instance, W25Q_CMD_READ_DATA_4BYTE, 100);
    }
    else
    {
        spi_status = SPI_MasterTransmitByte(spi_instance, W25Q_CMD_READ_DATA_3BYTE, 100);
    }
    
    if (spi_status != SPI_OK)
    {
        SPI_NSS_High(spi_instance);
        return W25Q_ERROR_INIT_FAILED;
    }
    
    /* 发送地址 */
    w25q_status = w25q_send_address(addr);
    if (w25q_status != W25Q_OK)
    {
        SPI_NSS_High(spi_instance);
        return w25q_status;
    }
    
    /* 读取数据（使用批量传输，内部自动处理跨页和SPI长度限制） */
    {
        uint32_t remaining = len;
        uint32_t offset = 0;
        uint16_t chunk_size;
        
        while (remaining > 0)
        {
            /* 计算本次传输的字节数（SPI驱动最大支持65535字节） */
            chunk_size = (remaining > 65535) ? 65535 : (uint16_t)remaining;
            
            /* 注意：SPI_MasterReceive的超时时间需要根据数据量和SPI速度计算 */
            /* 挂载时会读取4096字节的块，如果SPI速度为0.56MHz（预分频128），4096字节需要约7ms */
            /* 加上余量，使用500ms超时时间，避免挂载时超时 */
            /* 对于大块数据（>256字节），使用更长的超时时间 */
            uint32_t read_timeout = (chunk_size > 256) ? 500 : 100;  /* 大块数据使用500ms，小块数据使用100ms */
            spi_status = SPI_MasterReceive(spi_instance, &buf[offset], chunk_size, read_timeout);
            if (spi_status != SPI_OK)
            {
                SPI_NSS_High(spi_instance);
                /* 如果超时，返回错误 */
                if (spi_status == SPI_ERROR_TIMEOUT)
                {
                    return W25Q_ERROR_INIT_FAILED;  /* 超时错误 */
                }
                return W25Q_ERROR_INIT_FAILED;
            }
            
            offset += chunk_size;
            remaining -= chunk_size;
        }
    }
    
    SPI_NSS_High(spi_instance);
    
    return W25Q_OK;
}

/**
 * @brief 写入数据
 */
W25Q_Status_t W25Q_Write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    SPI_Instance_t spi_instance = W25Q_SPI_INSTANCE;
    SPI_Status_t spi_status;
    W25Q_Status_t w25q_status;
    uint32_t page_offset;
    uint32_t bytes_to_write;
    uint32_t current_addr;
    uint64_t capacity_bytes;
    
    if (g_w25q_device.state != W25Q_STATE_INITIALIZED)
    {
        return W25Q_ERROR_NOT_INIT;
    }
    
    if (buf == NULL)
    {
        return W25Q_ERROR_INIT_FAILED;
    }
    
    if (len == 0)
    {
        return W25Q_OK;
    }
    
    /* 64位地址运算：检查地址越界 */
    capacity_bytes = (uint64_t)g_w25q_device.capacity_mb * 1024ULL * 1024ULL;
    if ((uint64_t)addr + (uint64_t)len > capacity_bytes)
    {
        return W25Q_ERROR_OUT_OF_BOUND;
    }
    
    /* 按照用户建议的清晰逻辑：使用bytes_written跟踪已写字节数 */
    uint32_t bytes_written = 0;
    current_addr = addr;
    
    /* 循环直到所有数据写完 */
    while (bytes_written < len)
    {
        /* 计算当前页内偏移（关键！使用按位与，等价于 % 256） */
        page_offset = current_addr & 0xFF;  /* 当前页内偏移（0-255） */
        
        /* 计算本页还能写多少字节 */
        uint32_t page_remain = W25Q_PAGE_SIZE - page_offset;
        
        /* 本次写入长度 = 剩余数据 和 页剩余空间的较小值 */
        bytes_to_write = (len - bytes_written) < page_remain 
                        ? (len - bytes_written) 
                        : page_remain;
        
        /* 检查：确保chunk > 0 */
        if (bytes_to_write == 0)
        {
            break;
        }
        
        /* 等待Flash就绪 */
        w25q_status = W25Q_WaitReady(0);
        if (w25q_status != W25Q_OK)
        {
            return w25q_status;
        }
        
        /* 写使能（每次Page Program前必须！） */
        w25q_status = w25q_write_enable();
        if (w25q_status != W25Q_OK)
        {
            return w25q_status;
        }
        
        /* CS拉低 */
        SPI_NSS_Low(spi_instance);
        
        /* 关键：确保CS拉低后，Flash有足够时间识别CS下降沿 */
        Delay_us(1);
        
        /* 发送页编程指令 */
        if (g_w25q_device.addr_bytes == 4)
        {
            spi_status = SPI_MasterTransmitByte(spi_instance, W25Q_CMD_PAGE_PROG_4BYTE, 100);
        }
        else
        {
            spi_status = SPI_MasterTransmitByte(spi_instance, W25Q_CMD_PAGE_PROG_3BYTE, 100);
        }
        
        if (spi_status != SPI_OK)
        {
            SPI_NSS_High(spi_instance);
            return W25Q_ERROR_INIT_FAILED;
        }
        
        /* 发送地址（在同一个CS周期内，连续发送） */
        w25q_status = w25q_send_address(current_addr);
        if (w25q_status != W25Q_OK)
        {
            SPI_NSS_High(spi_instance);
            return w25q_status;
        }
        
        /* 发送数据（使用批量传输，提高性能） */
        spi_status = SPI_MasterTransmit(spi_instance, &buf[bytes_written], bytes_to_write, 100);
        if (spi_status != SPI_OK)
        {
            SPI_NSS_High(spi_instance);
            return W25Q_ERROR_INIT_FAILED;
        }
        
        /* 关键：确保最后一个数据字节完全发送完成 */
        /* 等待SPI传输完成（TXE和RXNE标志位） */
        {
            SPI_TypeDef *spi_periph = SPI_GetPeriph(spi_instance);
            if (spi_periph != NULL)
            {
                uint32_t timeout_count = 1000;
                while (SPI_I2S_GetFlagStatus(spi_periph, SPI_I2S_FLAG_TXE) == RESET)
                {
                    if (timeout_count-- == 0) break;
                }
                while (SPI_I2S_GetFlagStatus(spi_periph, SPI_I2S_FLAG_RXNE) == RESET)
                {
                    if (timeout_count-- == 0) break;
                }
            }
        }
        
        /* CS拉高（Flash开始内部编程操作） */
        SPI_NSS_High(spi_instance);
        
        /* 关键：CS拉高后，Flash需要时间启动内部编程操作 */
        /* 根据W25Q数据手册，CS拉高后需要等待至少1us，Flash才会开始编程 */
        Delay_us(10);
        
        /* 等待编程完成（BUSY位清除） */
        w25q_status = W25Q_WaitReady(0);
        if (w25q_status != W25Q_OK)
        {
            return w25q_status;
        }
        
        /* 更新变量，准备写下一页 */
        bytes_written += bytes_to_write;
        current_addr += bytes_to_write;
    }
    
    return W25Q_OK;
}

/**
 * @brief 擦除扇区（4KB）
 */
W25Q_Status_t W25Q_EraseSector(uint32_t addr)
{
    SPI_Instance_t spi_instance = W25Q_SPI_INSTANCE;
    SPI_Status_t spi_status;
    W25Q_Status_t w25q_status;
    uint64_t capacity_bytes;
    uint32_t sector_idx;
    uint32_t timeout_ms;
    
    if (g_w25q_device.state != W25Q_STATE_INITIALIZED)
    {
        return W25Q_ERROR_NOT_INIT;
    }
    
    /* 64位地址运算：检查地址越界 */
    capacity_bytes = (uint64_t)g_w25q_device.capacity_mb * 1024ULL * 1024ULL;
    if ((uint64_t)addr >= capacity_bytes)
    {
        return W25Q_ERROR_OUT_OF_BOUND;
    }
    
    /* 检查地址对齐（4KB边界） */
    if ((addr % W25Q_SECTOR_SIZE) != 0)
    {
        return W25Q_ERROR_INIT_FAILED;
    }
    
    /* 等待Flash就绪 */
    w25q_status = W25Q_WaitReady(0);
    if (w25q_status != W25Q_OK)
    {
        return w25q_status;
    }
    
    /* 写使能 */
    w25q_status = w25q_write_enable();
    if (w25q_status != W25Q_OK)
    {
        return w25q_status;
    }
    
    SPI_NSS_Low(spi_instance);
    
    /* 发送扇区擦除指令 */
    if (g_w25q_device.addr_bytes == 4)
    {
        spi_status = SPI_MasterTransmitByte(spi_instance, W25Q_CMD_SECTOR_ERASE_4BYTE, 100);
    }
    else
    {
        spi_status = SPI_MasterTransmitByte(spi_instance, W25Q_CMD_SECTOR_ERASE_3BYTE, 100);
    }
    
    if (spi_status != SPI_OK)
    {
        SPI_NSS_High(spi_instance);
        return W25Q_ERROR_INIT_FAILED;
    }
    
    /* 发送地址 */
    w25q_status = w25q_send_address(addr);
    if (w25q_status != W25Q_OK)
    {
        SPI_NSS_High(spi_instance);
        return w25q_status;
    }
    
    SPI_NSS_High(spi_instance);
    
    /* 动态超时补偿：Gb级芯片使用更长超时时间 */
    sector_idx = addr / W25Q_SECTOR_SIZE;
    timeout_ms = 100;  /* 基础超时100ms */
    if (g_w25q_device.capacity_mb >= 16)
    {
        /* Gb级芯片：timeout = sector_idx * 2ms + 100ms */
        timeout_ms = sector_idx * 2 + 100;
        if (timeout_ms > 200)
        {
            timeout_ms = 200;  /* 最大200ms */
        }
    }
    
    /* 等待擦除完成 */
    w25q_status = W25Q_WaitReady(timeout_ms);
    if (w25q_status != W25Q_OK)
    {
        return w25q_status;
    }
    
    return W25Q_OK;
}

/**
 * @brief 整片擦除
 */
W25Q_Status_t W25Q_EraseChip(void)
{
    SPI_Instance_t spi_instance = W25Q_SPI_INSTANCE;
    SPI_Status_t spi_status;
    W25Q_Status_t w25q_status;
    uint32_t timeout_ms;
    
    if (g_w25q_device.state != W25Q_STATE_INITIALIZED)
    {
        return W25Q_ERROR_NOT_INIT;
    }
    
    /* 等待Flash就绪 */
    w25q_status = W25Q_WaitReady(0);
    if (w25q_status != W25Q_OK)
    {
        return w25q_status;
    }
    
    /* 写使能 */
    w25q_status = w25q_write_enable();
    if (w25q_status != W25Q_OK)
    {
        return w25q_status;
    }
    
    SPI_NSS_Low(spi_instance);
    
    /* 发送整片擦除指令 */
    spi_status = SPI_MasterTransmitByte(spi_instance, W25Q_CMD_CHIP_ERASE, 100);
    
    SPI_NSS_High(spi_instance);
    
    if (spi_status != SPI_OK)
    {
        return W25Q_ERROR_INIT_FAILED;
    }
    
    /* 动态超时补偿：Gb级芯片使用更长超时时间 */
    timeout_ms = 30000;  /* 基础超时30秒 */
    if (g_w25q_device.capacity_mb >= 16)
    {
        timeout_ms = timeout_ms * 2;  /* Gb级芯片：60秒 */
    }
    
    /* 等待擦除完成 */
    w25q_status = W25Q_WaitReady(timeout_ms);
    if (w25q_status != W25Q_OK)
    {
        return w25q_status;
    }
    
    return W25Q_OK;
}

#endif /* CONFIG_MODULE_SPI_ENABLED */
#endif /* CONFIG_MODULE_W25Q_ENABLED */
