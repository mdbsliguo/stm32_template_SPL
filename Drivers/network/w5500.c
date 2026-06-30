/**
 * @file w5500.c
 * @brief W5500以太网控制器驱动实现
 * @version 1.0.0
 * @date 2026-06-30
 */

#include "config.h"

#if CONFIG_MODULE_W5500_ENABLED

#if CONFIG_MODULE_SPI_ENABLED

#include "w5500.h"
#include "w5500_regs.h"
#include "gpio.h"
#include "delay.h"
#include <stddef.h>
#include <string.h>

#define W5500_SPI_TIMEOUT_MS    100U
#define W5500_RESET_PULSE_MS    50U
#define W5500_RESET_WAIT_MS     200U
#define W5500_LINK_TIMEOUT_MS   5000U
#define W5500_SOCKET_DELAY_MS   5U
#define W5500_GATEWAY_PROBE_MS  5U

static const W5500_Config_t g_w5500_cfg = W5500_CONFIG;
static uint8_t g_w5500_initialized = 0U;
static W5500_NetConfig_t g_net_config;
static W5500_SocketConfig_t g_socket_cfg[W5500_SOCKET_MAX];
static W5500_SocketStatus_t g_socket_status[W5500_SOCKET_MAX];

/* ==================== 内部辅助 ==================== */

static SPI_Instance_t w5500_spi_instance(void)
{
    return (SPI_Instance_t)g_w5500_cfg.spi_instance;
}

static W5500_Status_t w5500_check_init(void)
{
    if (!g_w5500_initialized)
    {
        return W5500_ERROR_NOT_INIT;
    }
    return W5500_OK;
}

static W5500_Status_t w5500_check_socket(W5500_Socket_t socket)
{
    if (socket >= W5500_SOCKET_MAX)
    {
        return W5500_ERROR_INVALID_SOCKET;
    }
    return W5500_OK;
}

static void w5500_cs_low(void)
{
    GPIO_ResetPin(g_w5500_cfg.cs_port, g_w5500_cfg.cs_pin);
}

static void w5500_cs_high(void)
{
    GPIO_SetPin(g_w5500_cfg.cs_port, g_w5500_cfg.cs_pin);
}

static W5500_Status_t w5500_spi_send_byte(uint8_t data)
{
    SPI_Status_t st = SPI_MasterTransmitByte(w5500_spi_instance(), data, W5500_SPI_TIMEOUT_MS);
    return (st == SPI_OK) ? W5500_OK : W5500_ERROR_SPI_FAILED;
}

static W5500_Status_t w5500_spi_send_short(uint16_t data)
{
    W5500_Status_t st;

    st = w5500_spi_send_byte((uint8_t)(data >> 8));
    if (st != W5500_OK)
    {
        return st;
    }
    return w5500_spi_send_byte((uint8_t)(data & 0xFFU));
}

static W5500_Status_t w5500_spi_read_byte(uint8_t *data)
{
    uint8_t tx = 0x00U;
    uint8_t rx = 0x00U;
    SPI_Status_t st;

    if (data == NULL)
    {
        return W5500_ERROR_INVALID_PARAM;
    }

    st = SPI_MasterTransmitReceive(w5500_spi_instance(), &tx, &rx, 1U, W5500_SPI_TIMEOUT_MS);
    if (st != SPI_OK)
    {
        return W5500_ERROR_SPI_FAILED;
    }
    *data = rx;
    return W5500_OK;
}

static uint8_t w5500_sock_reg_block(W5500_Socket_t socket)
{
    return (uint8_t)(socket * 0x20U + 0x08U);
}

static uint8_t w5500_sock_tx_block(W5500_Socket_t socket)
{
    return (uint8_t)(socket * 0x20U + 0x10U);
}

static uint8_t w5500_sock_rx_block(W5500_Socket_t socket)
{
    return (uint8_t)(socket * 0x20U + 0x18U);
}

static W5500_Status_t w5500_write_common_1byte(uint16_t reg, uint8_t data)
{
    W5500_Status_t st;

    w5500_cs_low();
    st = w5500_spi_send_short(reg);
    if (st == W5500_OK)
    {
        st = w5500_spi_send_byte(W5500_FDM1 | W5500_RWB_WRITE | W5500_COMMON_R);
    }
    if (st == W5500_OK)
    {
        st = w5500_spi_send_byte(data);
    }
    w5500_cs_high();
    return st;
}

static W5500_Status_t w5500_write_common_2byte(uint16_t reg, uint16_t data)
{
    W5500_Status_t st;

    w5500_cs_low();
    st = w5500_spi_send_short(reg);
    if (st == W5500_OK)
    {
        st = w5500_spi_send_byte(W5500_FDM2 | W5500_RWB_WRITE | W5500_COMMON_R);
    }
    if (st == W5500_OK)
    {
        st = w5500_spi_send_short(data);
    }
    w5500_cs_high();
    return st;
}

static W5500_Status_t w5500_write_common_nbyte(uint16_t reg, const uint8_t *data, uint16_t size)
{
    W5500_Status_t st = W5500_OK;
    uint16_t i;

    if (data == NULL)
    {
        return W5500_ERROR_INVALID_PARAM;
    }

    w5500_cs_low();
    st = w5500_spi_send_short(reg);
    if (st == W5500_OK)
    {
        st = w5500_spi_send_byte(W5500_VDM | W5500_RWB_WRITE | W5500_COMMON_R);
    }
    for (i = 0U; (i < size) && (st == W5500_OK); i++)
    {
        st = w5500_spi_send_byte(data[i]);
    }
    w5500_cs_high();
    return st;
}

static W5500_Status_t w5500_write_sock_1byte(W5500_Socket_t socket, uint16_t reg, uint8_t data)
{
    W5500_Status_t st;

    w5500_cs_low();
    st = w5500_spi_send_short(reg);
    if (st == W5500_OK)
    {
        st = w5500_spi_send_byte(W5500_FDM1 | W5500_RWB_WRITE | w5500_sock_reg_block(socket));
    }
    if (st == W5500_OK)
    {
        st = w5500_spi_send_byte(data);
    }
    w5500_cs_high();
    return st;
}

static W5500_Status_t w5500_write_sock_2byte(W5500_Socket_t socket, uint16_t reg, uint16_t data)
{
    W5500_Status_t st;

    w5500_cs_low();
    st = w5500_spi_send_short(reg);
    if (st == W5500_OK)
    {
        st = w5500_spi_send_byte(W5500_FDM2 | W5500_RWB_WRITE | w5500_sock_reg_block(socket));
    }
    if (st == W5500_OK)
    {
        st = w5500_spi_send_short(data);
    }
    w5500_cs_high();
    return st;
}

static W5500_Status_t w5500_write_sock_4byte(W5500_Socket_t socket, uint16_t reg, const uint8_t *data)
{
    W5500_Status_t st;

    if (data == NULL)
    {
        return W5500_ERROR_INVALID_PARAM;
    }

    w5500_cs_low();
    st = w5500_spi_send_short(reg);
    if (st == W5500_OK)
    {
        st = w5500_spi_send_byte(W5500_FDM4 | W5500_RWB_WRITE | w5500_sock_reg_block(socket));
    }
    if (st == W5500_OK)
    {
        st = w5500_spi_send_byte(data[0]);
    }
    if (st == W5500_OK)
    {
        st = w5500_spi_send_byte(data[1]);
    }
    if (st == W5500_OK)
    {
        st = w5500_spi_send_byte(data[2]);
    }
    if (st == W5500_OK)
    {
        st = w5500_spi_send_byte(data[3]);
    }
    w5500_cs_high();
    return st;
}

static W5500_Status_t w5500_read_common_1byte(uint16_t reg, uint8_t *data)
{
    W5500_Status_t st;

    if (data == NULL)
    {
        return W5500_ERROR_INVALID_PARAM;
    }

    w5500_cs_low();
    st = w5500_spi_send_short(reg);
    if (st == W5500_OK)
    {
        st = w5500_spi_send_byte(W5500_FDM1 | W5500_RWB_READ | W5500_COMMON_R);
    }
    if (st == W5500_OK)
    {
        st = w5500_spi_read_byte(data);
    }
    w5500_cs_high();
    return st;
}

static W5500_Status_t w5500_read_sock_1byte(W5500_Socket_t socket, uint16_t reg, uint8_t *data)
{
    W5500_Status_t st;

    if (data == NULL)
    {
        return W5500_ERROR_INVALID_PARAM;
    }

    w5500_cs_low();
    st = w5500_spi_send_short(reg);
    if (st == W5500_OK)
    {
        st = w5500_spi_send_byte(W5500_FDM1 | W5500_RWB_READ | w5500_sock_reg_block(socket));
    }
    if (st == W5500_OK)
    {
        st = w5500_spi_read_byte(data);
    }
    w5500_cs_high();
    return st;
}

static W5500_Status_t w5500_read_sock_2byte(W5500_Socket_t socket, uint16_t reg, uint16_t *data)
{
    W5500_Status_t st;
    uint8_t hi;
    uint8_t lo;

    if (data == NULL)
    {
        return W5500_ERROR_INVALID_PARAM;
    }

    w5500_cs_low();
    st = w5500_spi_send_short(reg);
    if (st == W5500_OK)
    {
        st = w5500_spi_send_byte(W5500_FDM2 | W5500_RWB_READ | w5500_sock_reg_block(socket));
    }
    if (st == W5500_OK)
    {
        st = w5500_spi_read_byte(&hi);
    }
    if (st == W5500_OK)
    {
        st = w5500_spi_read_byte(&lo);
    }
    w5500_cs_high();

    if (st == W5500_OK)
    {
        *data = (uint16_t)(((uint16_t)hi << 8) | lo);
    }
    return st;
}

static W5500_Status_t w5500_chip_soft_reset(void)
{
    W5500_Status_t st;
    uint8_t i;

    st = w5500_write_common_1byte(W5500_MR, W5500_MR_RST);
    if (st != W5500_OK)
    {
        return st;
    }
    Delay_ms(10U);

    for (i = 0U; i < W5500_SOCKET_MAX; i++)
    {
        st = w5500_write_sock_1byte(i, W5500_Sn_RXBUF_SIZE, 0x02U);
        if (st != W5500_OK)
        {
            return st;
        }
        st = w5500_write_sock_1byte(i, W5500_Sn_TXBUF_SIZE, 0x02U);
        if (st != W5500_OK)
        {
            return st;
        }
    }

    st = w5500_write_common_2byte(W5500_RTR, 0x07D0U);
    if (st != W5500_OK)
    {
        return st;
    }
    return w5500_write_common_1byte(W5500_RCR, 8U);
}

static W5500_Status_t w5500_socket_reinit(W5500_Socket_t socket)
{
    W5500_Status_t st;

    if (socket != W5500_SOCKET_0)
    {
        return W5500_ERROR_NOT_IMPLEMENTED;
    }

    st = w5500_write_sock_2byte(socket, W5500_Sn_MSSR, W5500_MAX_PACKET_SIZE);
    if (st != W5500_OK)
    {
        return st;
    }
    st = w5500_write_sock_2byte(socket, W5500_Sn_PORT, g_socket_cfg[socket].local_port);
    if (st != W5500_OK)
    {
        return st;
    }
    st = w5500_write_sock_2byte(socket, W5500_Sn_DPORTR, g_socket_cfg[socket].dest_port);
    if (st != W5500_OK)
    {
        return st;
    }
    return w5500_write_sock_4byte(socket, W5500_Sn_DIPR, g_socket_cfg[socket].dest_ip);
}

/* ==================== 公共API ==================== */

W5500_Status_t W5500_Init(void)
{
    W5500_Status_t st;
    uint8_t version = 0U;
    SPI_Status_t spi_st;

    if (!g_w5500_cfg.enabled)
    {
        return W5500_ERROR_DISABLED;
    }

    spi_st = SPI_HW_Init(w5500_spi_instance());
    if (spi_st != SPI_OK)
    {
        return W5500_ERROR_SPI_FAILED;
    }

    if (GPIO_Config(g_w5500_cfg.cs_port, g_w5500_cfg.cs_pin, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHz) != GPIO_OK)
    {
        return W5500_ERROR_INIT_FAILED;
    }
    GPIO_SetPin(g_w5500_cfg.cs_port, g_w5500_cfg.cs_pin);
    if (g_w5500_cfg.rst_port != NULL && g_w5500_cfg.rst_pin != 0U)
    {
        if (GPIO_Config(g_w5500_cfg.rst_port, g_w5500_cfg.rst_pin, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_10MHz) != GPIO_OK)
        {
            return W5500_ERROR_INIT_FAILED;
        }
        GPIO_ResetPin(g_w5500_cfg.rst_port, g_w5500_cfg.rst_pin);
    }

    st = w5500_chip_soft_reset();
    if (st != W5500_OK)
    {
        return st;
    }

    st = w5500_read_common_1byte(W5500_VERR, &version);
    if (st != W5500_OK)
    {
        return st;
    }
    if (version != W5500_EXPECTED_VERSION)
    {
        return W5500_ERROR_INIT_FAILED;
    }

    memset(&g_net_config, 0, sizeof(g_net_config));
    memset(g_socket_cfg, 0, sizeof(g_socket_cfg));
    memset(g_socket_status, 0, sizeof(g_socket_status));

    g_w5500_initialized = 1U;
    return W5500_OK;
}

W5500_Status_t W5500_Deinit(void)
{
    g_w5500_initialized = 0U;
    memset(&g_net_config, 0, sizeof(g_net_config));
    memset(g_socket_cfg, 0, sizeof(g_socket_cfg));
    memset(g_socket_status, 0, sizeof(g_socket_status));
    return W5500_OK;
}

W5500_Status_t W5500_HardwareReset(void)
{
    W5500_Status_t st;
    uint32_t start;
    uint8_t phy = 0U;

    st = w5500_check_init();
    if (st != W5500_OK)
    {
        return st;
    }

    if (g_w5500_cfg.rst_port != NULL && g_w5500_cfg.rst_pin != 0U)
    {
        GPIO_ResetPin(g_w5500_cfg.rst_port, g_w5500_cfg.rst_pin);
        Delay_ms(W5500_RESET_PULSE_MS);
        GPIO_SetPin(g_w5500_cfg.rst_port, g_w5500_cfg.rst_pin);
        Delay_ms(W5500_RESET_WAIT_MS);
    }

    start = Delay_GetTick();
    for (;;)
    {
        st = w5500_read_common_1byte(W5500_PHYCFGR, &phy);
        if (st != W5500_OK)
        {
            return st;
        }
        if ((phy & W5500_PHY_LINK) != 0U)
        {
            return W5500_OK;
        }
        if (Delay_GetElapsed(Delay_GetTick(), start) >= W5500_LINK_TIMEOUT_MS)
        {
            return W5500_ERROR_LINK_DOWN;
        }
        Delay_ms(10U);
    }
}

W5500_Status_t W5500_SetNetConfig(const W5500_NetConfig_t *config)
{
    W5500_Status_t st;

    st = w5500_check_init();
    if (st != W5500_OK)
    {
        return st;
    }
    if (config == NULL)
    {
        return W5500_ERROR_INVALID_PARAM;
    }
    if ((config->mac[0] & 0x01U) != 0U)
    {
        return W5500_ERROR_INVALID_PARAM;
    }

    st = w5500_write_common_nbyte(W5500_GAR, config->gateway, 4U);
    if (st != W5500_OK)
    {
        return st;
    }
    st = w5500_write_common_nbyte(W5500_SUBR, config->subnet, 4U);
    if (st != W5500_OK)
    {
        return st;
    }
    st = w5500_write_common_nbyte(W5500_SHAR, config->mac, 6U);
    if (st != W5500_OK)
    {
        return st;
    }
    st = w5500_write_common_nbyte(W5500_SIPR, config->ip, 4U);
    if (st != W5500_OK)
    {
        return st;
    }

    memcpy(&g_net_config, config, sizeof(g_net_config));
    return W5500_OK;
}

W5500_Status_t W5500_GetNetConfig(W5500_NetConfig_t *config)
{
    W5500_Status_t st;

    st = w5500_check_init();
    if (st != W5500_OK)
    {
        return st;
    }
    if (config == NULL)
    {
        return W5500_ERROR_INVALID_PARAM;
    }
    memcpy(config, &g_net_config, sizeof(g_net_config));
    return W5500_OK;
}

W5500_Status_t W5500_DetectGateway(void)
{
    W5500_Status_t st;
    uint8_t probe_ip[4];
    uint8_t sock_ir;
    uint8_t dhar;
    uint32_t start;

    st = w5500_check_init();
    if (st != W5500_OK)
    {
        return st;
    }

    probe_ip[0] = (uint8_t)(g_net_config.ip[0] + 1U);
    probe_ip[1] = (uint8_t)(g_net_config.ip[1] + 1U);
    probe_ip[2] = (uint8_t)(g_net_config.ip[2] + 1U);
    probe_ip[3] = (uint8_t)(g_net_config.ip[3] + 1U);

    /* OPEN 前须配置 MSSR/PORT，否则 Sn_SR 无法进入 SOCK_INIT */
    st = w5500_write_sock_2byte(W5500_SOCKET_0, W5500_Sn_MSSR, W5500_MAX_PACKET_SIZE);
    if (st != W5500_OK)
    {
        return st;
    }
    if (g_socket_cfg[W5500_SOCKET_0].local_port != 0U)
    {
        st = w5500_write_sock_2byte(W5500_SOCKET_0, W5500_Sn_PORT,
                                    g_socket_cfg[W5500_SOCKET_0].local_port);
    }
    else
    {
        st = w5500_write_sock_2byte(W5500_SOCKET_0, W5500_Sn_PORT, 0x1234U);
    }
    if (st != W5500_OK)
    {
        return st;
    }

    st = w5500_write_sock_4byte(W5500_SOCKET_0, W5500_Sn_DIPR, probe_ip);
    if (st != W5500_OK)
    {
        return st;
    }
    st = w5500_write_sock_1byte(W5500_SOCKET_0, W5500_Sn_MR, W5500_Sn_MR_TCP);
    if (st != W5500_OK)
    {
        return st;
    }
    st = w5500_write_sock_1byte(W5500_SOCKET_0, W5500_Sn_CR, W5500_Sn_CR_OPEN);
    if (st != W5500_OK)
    {
        return st;
    }
    Delay_ms(W5500_GATEWAY_PROBE_MS);

    st = w5500_read_sock_1byte(W5500_SOCKET_0, W5500_Sn_SR, &dhar);
    if (st != W5500_OK)
    {
        return st;
    }
    if (dhar != W5500_SOCK_INIT)
    {
        w5500_write_sock_1byte(W5500_SOCKET_0, W5500_Sn_CR, W5500_Sn_CR_CLOSE);
        return W5500_ERROR_GATEWAY;
    }

    st = w5500_write_sock_1byte(W5500_SOCKET_0, W5500_Sn_CR, W5500_Sn_CR_CONNECT);
    if (st != W5500_OK)
    {
        return st;
    }

    start = Delay_GetTick();
    for (;;)
    {
        st = w5500_read_sock_1byte(W5500_SOCKET_0, W5500_Sn_IR, &sock_ir);
        if (st != W5500_OK)
        {
            return st;
        }
        if (sock_ir != 0U)
        {
            w5500_write_sock_1byte(W5500_SOCKET_0, W5500_Sn_IR, sock_ir);
        }
        Delay_ms(W5500_GATEWAY_PROBE_MS);

        if ((sock_ir & W5500_Sn_IR_TIMEOUT) != 0U)
        {
            w5500_write_sock_1byte(W5500_SOCKET_0, W5500_Sn_CR, W5500_Sn_CR_CLOSE);
            return W5500_ERROR_GATEWAY;
        }

        st = w5500_read_sock_1byte(W5500_SOCKET_0, W5500_Sn_DHAR, &dhar);
        if (st != W5500_OK)
        {
            return st;
        }
        if (dhar != 0xFFU)
        {
            w5500_write_sock_1byte(W5500_SOCKET_0, W5500_Sn_CR, W5500_Sn_CR_CLOSE);
            return W5500_OK;
        }

        if (Delay_GetElapsed(Delay_GetTick(), start) >= W5500_LINK_TIMEOUT_MS)
        {
            w5500_write_sock_1byte(W5500_SOCKET_0, W5500_Sn_CR, W5500_Sn_CR_CLOSE);
            return W5500_ERROR_TIMEOUT;
        }
    }
}

W5500_Status_t W5500_GetLinkStatus(uint8_t *linked)
{
    W5500_Status_t st;
    uint8_t phy = 0U;

    st = w5500_check_init();
    if (st != W5500_OK)
    {
        return st;
    }
    if (linked == NULL)
    {
        return W5500_ERROR_INVALID_PARAM;
    }

    st = w5500_read_common_1byte(W5500_PHYCFGR, &phy);
    if (st != W5500_OK)
    {
        return st;
    }
    *linked = ((phy & W5500_PHY_LINK) != 0U) ? 1U : 0U;
    return W5500_OK;
}

W5500_Status_t W5500_ReadVersion(uint8_t *version)
{
    W5500_Status_t st;

    st = w5500_check_init();
    if (st != W5500_OK)
    {
        return st;
    }
    if (version == NULL)
    {
        return W5500_ERROR_INVALID_PARAM;
    }
    return w5500_read_common_1byte(W5500_VERR, version);
}

W5500_Status_t W5500_SocketInit(W5500_Socket_t socket, const W5500_SocketConfig_t *config)
{
    W5500_Status_t st;

    st = w5500_check_init();
    if (st != W5500_OK)
    {
        return st;
    }
    if (config == NULL)
    {
        return W5500_ERROR_INVALID_PARAM;
    }
    if (socket != W5500_SOCKET_0)
    {
        return W5500_ERROR_NOT_IMPLEMENTED;
    }

    memcpy(&g_socket_cfg[socket], config, sizeof(W5500_SocketConfig_t));
    g_socket_status[socket].flags = W5500_SOCK_FLAG_INIT;
    g_socket_status[socket].events = 0U;

    return w5500_socket_reinit(socket);
}

W5500_Status_t W5500_SocketConnect(W5500_Socket_t socket)
{
    W5500_Status_t st;
    uint8_t sr = 0U;

    st = w5500_check_init();
    if (st != W5500_OK)
    {
        return st;
    }
    st = w5500_check_socket(socket);
    if (st != W5500_OK)
    {
        return st;
    }
    if (socket != W5500_SOCKET_0)
    {
        return W5500_ERROR_NOT_IMPLEMENTED;
    }

    st = w5500_write_sock_1byte(socket, W5500_Sn_MR, W5500_Sn_MR_TCP);
    if (st != W5500_OK)
    {
        return st;
    }
    st = w5500_write_sock_1byte(socket, W5500_Sn_CR, W5500_Sn_CR_OPEN);
    if (st != W5500_OK)
    {
        return st;
    }
    Delay_ms(W5500_SOCKET_DELAY_MS);

    st = w5500_read_sock_1byte(socket, W5500_Sn_SR, &sr);
    if (st != W5500_OK)
    {
        return st;
    }
    if (sr != W5500_SOCK_INIT)
    {
        w5500_write_sock_1byte(socket, W5500_Sn_CR, W5500_Sn_CR_CLOSE);
        return W5500_ERROR_SOCKET;
    }

    st = w5500_write_sock_1byte(socket, W5500_Sn_CR, W5500_Sn_CR_CONNECT);
    return st;
}

W5500_Status_t W5500_SocketListen(W5500_Socket_t socket)
{
    W5500_Status_t st;
    uint8_t sr = 0U;

    st = w5500_check_init();
    if (st != W5500_OK)
    {
        return st;
    }
    st = w5500_check_socket(socket);
    if (st != W5500_OK)
    {
        return st;
    }
    if (socket != W5500_SOCKET_0)
    {
        return W5500_ERROR_NOT_IMPLEMENTED;
    }

    st = w5500_write_sock_1byte(socket, W5500_Sn_MR, W5500_Sn_MR_TCP);
    if (st != W5500_OK)
    {
        return st;
    }
    st = w5500_write_sock_1byte(socket, W5500_Sn_CR, W5500_Sn_CR_OPEN);
    if (st != W5500_OK)
    {
        return st;
    }
    Delay_ms(W5500_SOCKET_DELAY_MS);

    st = w5500_read_sock_1byte(socket, W5500_Sn_SR, &sr);
    if (st != W5500_OK)
    {
        return st;
    }
    if (sr != W5500_SOCK_INIT)
    {
        w5500_write_sock_1byte(socket, W5500_Sn_CR, W5500_Sn_CR_CLOSE);
        return W5500_ERROR_SOCKET;
    }

    st = w5500_write_sock_1byte(socket, W5500_Sn_CR, W5500_Sn_CR_LISTEN);
    if (st != W5500_OK)
    {
        return st;
    }
    Delay_ms(W5500_SOCKET_DELAY_MS);

    st = w5500_read_sock_1byte(socket, W5500_Sn_SR, &sr);
    if (st != W5500_OK)
    {
        return st;
    }
    if (sr != W5500_SOCK_LISTEN)
    {
        w5500_write_sock_1byte(socket, W5500_Sn_CR, W5500_Sn_CR_CLOSE);
        return W5500_ERROR_SOCKET;
    }

    g_socket_status[socket].flags = W5500_SOCK_FLAG_INIT;
    g_socket_status[socket].events = 0U;
    return W5500_OK;
}

W5500_Status_t W5500_SocketOpenUdp(W5500_Socket_t socket)
{
    W5500_Status_t st;
    uint8_t sr = 0U;

    st = w5500_check_init();
    if (st != W5500_OK)
    {
        return st;
    }
    st = w5500_check_socket(socket);
    if (st != W5500_OK)
    {
        return st;
    }
    if (socket != W5500_SOCKET_0)
    {
        return W5500_ERROR_NOT_IMPLEMENTED;
    }

    st = w5500_write_sock_1byte(socket, W5500_Sn_MR, W5500_Sn_MR_UDP);
    if (st != W5500_OK)
    {
        return st;
    }
    st = w5500_write_sock_1byte(socket, W5500_Sn_CR, W5500_Sn_CR_OPEN);
    if (st != W5500_OK)
    {
        return st;
    }
    Delay_ms(W5500_SOCKET_DELAY_MS);

    st = w5500_read_sock_1byte(socket, W5500_Sn_SR, &sr);
    if (st != W5500_OK)
    {
        return st;
    }
    if (sr != W5500_SOCK_UDP)
    {
        w5500_write_sock_1byte(socket, W5500_Sn_CR, W5500_Sn_CR_CLOSE);
        return W5500_ERROR_SOCKET;
    }
    return W5500_OK;
}

W5500_Status_t W5500_SocketClose(W5500_Socket_t socket)
{
    W5500_Status_t st;

    st = w5500_check_init();
    if (st != W5500_OK)
    {
        return st;
    }
    st = w5500_check_socket(socket);
    if (st != W5500_OK)
    {
        return st;
    }

    g_socket_status[socket].flags = 0U;
    g_socket_status[socket].events = 0U;
    return w5500_write_sock_1byte(socket, W5500_Sn_CR, W5500_Sn_CR_CLOSE);
}

W5500_Status_t W5500_SocketRead(W5500_Socket_t socket, uint8_t *buffer, uint16_t buffer_size, uint16_t *read_len)
{
    W5500_Status_t st;
    uint16_t rx_size;
    uint16_t offset;
    uint16_t offset1;
    uint16_t i;
    uint8_t byte_val;

    st = w5500_check_init();
    if (st != W5500_OK)
    {
        return st;
    }
    st = w5500_check_socket(socket);
    if (st != W5500_OK)
    {
        return st;
    }
    if ((buffer == NULL) || (read_len == NULL))
    {
        return W5500_ERROR_INVALID_PARAM;
    }

    *read_len = 0U;
    st = w5500_read_sock_2byte(socket, W5500_Sn_RX_RSR, &rx_size);
    if (st != W5500_OK)
    {
        return st;
    }
    if (rx_size == 0U)
    {
        return W5500_OK;
    }
    if (rx_size > W5500_MAX_PACKET_SIZE)
    {
        rx_size = W5500_MAX_PACKET_SIZE;
    }
    if (rx_size > buffer_size)
    {
        rx_size = buffer_size;
    }

    st = w5500_read_sock_2byte(socket, W5500_Sn_RX_RD, &offset);
    if (st != W5500_OK)
    {
        return st;
    }
    offset1 = offset;
    offset &= (uint16_t)(W5500_S_RX_SIZE - 1U);

    w5500_cs_low();
    st = w5500_spi_send_short(offset);
    if (st == W5500_OK)
    {
        st = w5500_spi_send_byte(W5500_VDM | W5500_RWB_READ | w5500_sock_rx_block(socket));
    }

    if ((st == W5500_OK) && ((offset + rx_size) < W5500_S_RX_SIZE))
    {
        for (i = 0U; i < rx_size; i++)
        {
            st = w5500_spi_read_byte(&byte_val);
            if (st != W5500_OK)
            {
                break;
            }
            buffer[i] = byte_val;
        }
    }
    else if (st == W5500_OK)
    {
        uint16_t part1 = (uint16_t)(W5500_S_RX_SIZE - offset);
        for (i = 0U; i < part1; i++)
        {
            st = w5500_spi_read_byte(&byte_val);
            if (st != W5500_OK)
            {
                break;
            }
            buffer[i] = byte_val;
        }
        w5500_cs_high();
        w5500_cs_low();
        st = w5500_spi_send_short(0U);
        if (st == W5500_OK)
        {
            st = w5500_spi_send_byte(W5500_VDM | W5500_RWB_READ | w5500_sock_rx_block(socket));
        }
        for (; (i < rx_size) && (st == W5500_OK); i++)
        {
            st = w5500_spi_read_byte(&byte_val);
            if (st == W5500_OK)
            {
                buffer[i] = byte_val;
            }
        }
    }
    w5500_cs_high();

    if (st != W5500_OK)
    {
        return st;
    }

    offset1 = (uint16_t)(offset1 + rx_size);
    st = w5500_write_sock_2byte(socket, W5500_Sn_RX_RD, offset1);
    if (st != W5500_OK)
    {
        return st;
    }
    st = w5500_write_sock_1byte(socket, W5500_Sn_CR, W5500_Sn_CR_RECV);
    if (st == W5500_OK)
    {
        *read_len = rx_size;
    }
    return st;
}

W5500_Status_t W5500_SocketWrite(W5500_Socket_t socket, const uint8_t *data, uint16_t size)
{
    W5500_Status_t st;
    uint8_t mr = 0U;
    uint16_t offset;
    uint16_t offset1;
    uint16_t i;

    st = w5500_check_init();
    if (st != W5500_OK)
    {
        return st;
    }
    st = w5500_check_socket(socket);
    if (st != W5500_OK)
    {
        return st;
    }
    if ((data == NULL) || (size == 0U))
    {
        return W5500_ERROR_INVALID_PARAM;
    }

    st = w5500_read_sock_1byte(socket, W5500_Sn_MR, &mr);
    if (st != W5500_OK)
    {
        return st;
    }
    if ((mr & 0x0FU) == W5500_Sn_MR_UDP)
    {
        st = w5500_write_sock_4byte(socket, W5500_Sn_DIPR, g_socket_cfg[socket].dest_ip);
        if (st != W5500_OK)
        {
            return st;
        }
        st = w5500_write_sock_2byte(socket, W5500_Sn_DPORTR, g_socket_cfg[socket].dest_port);
        if (st != W5500_OK)
        {
            return st;
        }
    }

    st = w5500_read_sock_2byte(socket, W5500_Sn_TX_WR, &offset);
    if (st != W5500_OK)
    {
        return st;
    }
    offset1 = offset;
    offset &= (uint16_t)(W5500_S_TX_SIZE - 1U);

    w5500_cs_low();
    st = w5500_spi_send_short(offset);
    if (st == W5500_OK)
    {
        st = w5500_spi_send_byte(W5500_VDM | W5500_RWB_WRITE | w5500_sock_tx_block(socket));
    }

    if ((st == W5500_OK) && ((offset + size) < W5500_S_TX_SIZE))
    {
        for (i = 0U; i < size; i++)
        {
            st = w5500_spi_send_byte(data[i]);
            if (st != W5500_OK)
            {
                break;
            }
        }
    }
    else if (st == W5500_OK)
    {
        uint16_t part1 = (uint16_t)(W5500_S_TX_SIZE - offset);
        for (i = 0U; i < part1; i++)
        {
            st = w5500_spi_send_byte(data[i]);
            if (st != W5500_OK)
            {
                break;
            }
        }
        w5500_cs_high();
        w5500_cs_low();
        st = w5500_spi_send_short(0U);
        if (st == W5500_OK)
        {
            st = w5500_spi_send_byte(W5500_VDM | W5500_RWB_WRITE | w5500_sock_tx_block(socket));
        }
        for (; (i < size) && (st == W5500_OK); i++)
        {
            st = w5500_spi_send_byte(data[i]);
        }
    }
    w5500_cs_high();

    if (st != W5500_OK)
    {
        return st;
    }

    offset1 = (uint16_t)(offset1 + size);
    st = w5500_write_sock_2byte(socket, W5500_Sn_TX_WR, offset1);
    if (st != W5500_OK)
    {
        return st;
    }
    return w5500_write_sock_1byte(socket, W5500_Sn_CR, W5500_Sn_CR_SEND);
}

/**
 * @brief 根据硬件 Sn_SR 同步 Socket 连接标志（轮询模式补全 Sn_IR 漏检）
 */
static void w5500_sync_socket_state(W5500_Socket_t socket)
{
    W5500_Status_t st;
    uint8_t sr;

    if (socket != W5500_SOCKET_0)
    {
        return;
    }

    st = w5500_read_sock_1byte(socket, W5500_Sn_SR, &sr);
    if (st != W5500_OK)
    {
        return;
    }

    if (sr == W5500_SOCK_ESTABLISHED)
    {
        g_socket_status[socket].flags = (uint8_t)(W5500_SOCK_FLAG_INIT | W5500_SOCK_FLAG_CONN);
    }
    else if (sr == W5500_SOCK_LISTEN)
    {
        g_socket_status[socket].flags = W5500_SOCK_FLAG_INIT;
    }
    else if (sr == W5500_SOCK_CLOSED)
    {
        if ((g_socket_status[socket].flags & W5500_SOCK_FLAG_CONN) != 0U)
        {
            g_socket_status[socket].flags &= (uint8_t)(~W5500_SOCK_FLAG_CONN);
        }
    }
}

W5500_Status_t W5500_InterruptProcess(void)
{
    W5500_Status_t st;
    uint8_t sir;
    uint8_t sock_ir;

    st = w5500_check_init();
    if (st != W5500_OK)
    {
        return st;
    }

    for (;;)
    {
        st = w5500_read_common_1byte(W5500_SIR, &sir);
        if (st != W5500_OK)
        {
            return st;
        }
        if ((sir & W5500_S0_INT) == 0U)
        {
            break;
        }

        st = w5500_read_sock_1byte(W5500_SOCKET_0, W5500_Sn_IR, &sock_ir);
        if (st != W5500_OK)
        {
            return st;
        }
        if (sock_ir != 0U)
        {
            w5500_write_sock_1byte(W5500_SOCKET_0, W5500_Sn_IR, sock_ir);
        }

        if ((sock_ir & W5500_Sn_IR_CON) != 0U)
        {
            g_socket_status[W5500_SOCKET_0].flags |= W5500_SOCK_FLAG_CONN;
        }
        if ((sock_ir & W5500_Sn_IR_DISCON) != 0U)
        {
            w5500_write_sock_1byte(W5500_SOCKET_0, W5500_Sn_CR, W5500_Sn_CR_CLOSE);
            w5500_socket_reinit(W5500_SOCKET_0);
            g_socket_status[W5500_SOCKET_0].flags = 0U;
            g_socket_status[W5500_SOCKET_0].events = 0U;
        }
        if ((sock_ir & W5500_Sn_IR_SEND_OK) != 0U)
        {
            g_socket_status[W5500_SOCKET_0].events |= W5500_SOCK_EVT_TX_OK;
        }
        if ((sock_ir & W5500_Sn_IR_RECV) != 0U)
        {
            g_socket_status[W5500_SOCKET_0].events |= W5500_SOCK_EVT_RECEIVE;
        }
        if ((sock_ir & W5500_Sn_IR_TIMEOUT) != 0U)
        {
            w5500_write_sock_1byte(W5500_SOCKET_0, W5500_Sn_CR, W5500_Sn_CR_CLOSE);
            w5500_socket_reinit(W5500_SOCKET_0);
            g_socket_status[W5500_SOCKET_0].flags = 0U;
            g_socket_status[W5500_SOCKET_0].events = 0U;
        }
    }

    w5500_sync_socket_state(W5500_SOCKET_0);
    return W5500_OK;
}

W5500_Status_t W5500_GetSocketStatus(W5500_Socket_t socket, W5500_SocketStatus_t *status)
{
    W5500_Status_t st;

    st = w5500_check_init();
    if (st != W5500_OK)
    {
        return st;
    }
    st = w5500_check_socket(socket);
    if (st != W5500_OK)
    {
        return st;
    }
    if (status == NULL)
    {
        return W5500_ERROR_INVALID_PARAM;
    }

    *status = g_socket_status[socket];
    return W5500_OK;
}

W5500_Status_t W5500_ClearSocketEvents(W5500_Socket_t socket, uint8_t event_mask)
{
    W5500_Status_t st;

    st = w5500_check_init();
    if (st != W5500_OK)
    {
        return st;
    }
    st = w5500_check_socket(socket);
    if (st != W5500_OK)
    {
        return st;
    }

    g_socket_status[socket].events &= (uint8_t)(~event_mask);
    return W5500_OK;
}

#endif /* CONFIG_MODULE_SPI_ENABLED */
#endif /* CONFIG_MODULE_W5500_ENABLED */
