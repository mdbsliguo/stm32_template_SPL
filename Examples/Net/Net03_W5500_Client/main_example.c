/**
 * @file main_example.c
 * @brief Net03 - W5500 TCP Client EXTI 示例（小精灵 F103ZE）
 * @example Examples/Net/Net03_W5500_Client/main_example.c
 * @details 连接 Net01/Net02 Server（默认 192.168.101.101:8080），EXTI 处理事件
 */

#include "stm32f10x.h"
#include "system_init.h"
#include "board_early_init.h"
#include "board.h"
#include "uart.h"
#include "debug.h"
#include "log.h"
#include "error_handler.h"
#include "error_code.h"
#include "delay.h"
#include "config.h"
#include "led.h"
#include "w5500.h"
#include "exti.h"
#if CONFIG_MODULE_OLED_ENABLED
#include "oled_ssd1306.h"
#endif
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ==================== 网络参数（按需修改） ==================== */

#define NET_IP_ADDR             { 192, 168, 101, 202 }   /* 本机 IP */
#define NET_GATEWAY             { 192, 168, 101, 1 }
#define NET_SUBNET              { 255, 255, 255, 0 }
#define NET_SERVER_ADDR         { 192, 168, 101, 101 }   /* 目标 Server */
#define NET_SERVER_PORT         8080U
#define NET_LOCAL_PORT          50000U

#define NET_SEND_INTERVAL_MS    500U
#define NET_RECONNECT_MS        3000U
#define NET_RX_BUF_SIZE         1460U
#define NET_GATEWAY_DETECT_EN   0U

#define STM32_UID_BASE          0x1FFFF7E8UL

/* ==================== 应用上下文 ==================== */

typedef struct
{
    W5500_NetConfig_t net;
    uint8_t server_ip[4];
    uint16_t server_port;
    uint8_t phy_linked;
    uint8_t gw_ok;
    uint8_t oled_ready;
    uint8_t ui_flags;
    uint8_t server_on;      /* 已连上 Server */
} Net_AppCtx_t;

#define NET_UI_FLAG_TRYING   0x01U
#define NET_UI_FLAG_GW       0x02U
#define NET_UI_FLAG_CONN     0x04U

static Net_AppCtx_t g_app;
static const uint8_t g_client_msg[] = "\r\nW5500 TCP Client hello\r\n";
static uint8_t g_rx_buffer[NET_RX_BUF_SIZE];
static uint32_t g_last_send_tick;
static uint32_t g_last_reconnect_tick;
static uint32_t g_last_rx_idle_log_tick;
static volatile uint8_t g_w5500_irq_pending;

/* ==================== 基础工具 ==================== */

static void Net_GenerateMacFromUid(uint8_t mac[6])
{
    const uint8_t *uid = (const uint8_t *)STM32_UID_BASE;

    if (mac == NULL)
    {
        return;
    }

    mac[0] = 0x02U;
    mac[1] = (uint8_t)(uid[0] ^ uid[4]);
    mac[2] = (uint8_t)(uid[1] ^ uid[5]);
    mac[3] = uid[2];
    mac[4] = uid[3];
    mac[5] = (uint8_t)(uid[4] ^ uid[7]);
}

static void Net_FatalBlink(void)
{
    while (1)
    {
        LED1_On();
        Delay_ms(100);
        LED1_Off();
        Delay_ms(100);
    }
}

static W5500_Status_t Net_BuildNetConfig(W5500_NetConfig_t *cfg)
{
    static const uint8_t ip[] = NET_IP_ADDR;
    static const uint8_t gw[] = NET_GATEWAY;
    static const uint8_t mask[] = NET_SUBNET;
    static const uint8_t srv[] = NET_SERVER_ADDR;

    if (cfg == NULL)
    {
        return W5500_ERROR_INVALID_PARAM;
    }

    memcpy(cfg->ip, ip, 4U);
    memcpy(cfg->gateway, gw, 4U);
    memcpy(cfg->subnet, mask, 4U);
    memcpy(g_app.server_ip, srv, 4U);
    g_app.server_port = NET_SERVER_PORT;
    Net_GenerateMacFromUid(cfg->mac);
    return W5500_OK;
}

/* 将收到的数据原样输出到 UART（printf 重定向） */
static void Net_UartDumpRx(uint16_t len)
{
    uint16_t i;

    if (len == 0U)
    {
        return;
    }

    LOG_INFO("NET", "RX %u bytes from server:", (unsigned int)len);
    for (i = 0U; i < len; i++)
    {
        (void)Debug_PutChar((int)g_rx_buffer[i]);
    }
    (void)Debug_PutChar('\r');
    (void)Debug_PutChar('\n');
}

/* ==================== 调试与显示 ==================== */

static void Net_InitDebug(void)
{
    UART_Status_t uart_st;
    int dbg_st;
    log_config_t log_cfg;

    uart_st = UART_Init(UART_INSTANCE_1);
    if (uart_st != UART_OK)
    {
        Net_FatalBlink();
    }

    dbg_st = Debug_Init(DEBUG_MODE_UART, DEBUG_UART_BAUDRATE);
    if (dbg_st != 0)
    {
        Net_FatalBlink();
    }

    memset(&log_cfg, 0, sizeof(log_cfg));
    log_cfg.level = LOG_LEVEL_DEBUG;
    log_cfg.enable_timestamp = 0;
    log_cfg.enable_module = 1;
    log_cfg.enable_color = 0;

    if (Log_Init(&log_cfg) != LOG_OK)
    {
        Net_FatalBlink();
    }
}

static void Net_OledShowLinePadded(uint8_t line, const char *text)
{
#if CONFIG_MODULE_OLED_ENABLED
    char buf[17];
    uint8_t i;
    uint8_t len;

    if (!g_app.oled_ready || text == NULL)
    {
        return;
    }

    len = 0U;
    while ((text[len] != '\0') && (len < 16U))
    {
        len++;
    }
    for (i = 0U; i < len; i++)
    {
        buf[i] = text[i];
    }
    for (; i < 16U; i++)
    {
        buf[i] = ' ';
    }
    buf[16] = '\0';
    (void)OLED_ShowString(line, 1, buf);
#else
    (void)line;
    (void)text;
#endif
}

static void Net_InitOled(void)
{
#if CONFIG_MODULE_OLED_ENABLED
    g_app.oled_ready = 0U;
    if (OLED_Init() != OLED_OK)
    {
        LOG_WARN("MAIN", "OLED init fail");
        return;
    }
    g_app.oled_ready = 1U;
    Net_OledShowLinePadded(1, "Net03 Client");
    Net_OledShowLinePadded(2, "Init...");
    Net_OledShowLinePadded(3, "L:-- S:----");
    Net_OledShowLinePadded(4, "Boot...");
#endif
}

static void Net_OledShowLine4(uint8_t flags)
{
#if CONFIG_MODULE_OLED_ENABLED
    if (!g_app.oled_ready)
    {
        return;
    }

    if ((flags & NET_UI_FLAG_CONN) != 0U)
    {
        Net_OledShowLinePadded(4, "Srv ON");
    }
    else if ((flags & NET_UI_FLAG_TRYING) != 0U)
    {
        Net_OledShowLinePadded(4, "Connect...");
    }
    else
    {
        Net_OledShowLinePadded(4, "Srv OFF");
    }
#else
    (void)flags;
#endif
}

static void Net_UpdateOledFull(uint8_t flags)
{
#if CONFIG_MODULE_OLED_ENABLED
    char line[17];

    if (!g_app.oled_ready)
    {
        return;
    }

    Net_OledShowLinePadded(1, "Net03 Client");
    (void)snprintf(line, sizeof(line), "%u.%u.%u.%u",
                   g_app.net.ip[0], g_app.net.ip[1],
                   g_app.net.ip[2], g_app.net.ip[3]);
    Net_OledShowLinePadded(2, line);
    (void)snprintf(line, sizeof(line), "S:%u.%u.%u.%u",
                   g_app.server_ip[0], g_app.server_ip[1],
                   g_app.server_ip[2], g_app.server_ip[3]);
    Net_OledShowLinePadded(3, line);
    Net_OledShowLine4(flags);
    g_app.ui_flags = flags;
#else
    (void)flags;
#endif
}

static void Net_UpdateOledLine4Only(uint8_t flags)
{
#if CONFIG_MODULE_OLED_ENABLED
    if (!g_app.oled_ready)
    {
        return;
    }
    if (flags == g_app.ui_flags)
    {
        return;
    }
    Net_OledShowLine4(flags);
    g_app.ui_flags = flags;
#else
    (void)flags;
#endif
}

static uint8_t Net_BuildUiFlags(const W5500_SocketStatus_t *status)
{
    uint8_t flags = NET_UI_FLAG_TRYING;

    if (g_app.gw_ok != 0U)
    {
        flags |= NET_UI_FLAG_GW;
    }
    if ((status->flags & W5500_SOCK_FLAG_CONN) != 0U)
    {
        flags |= NET_UI_FLAG_CONN;
    }
    return flags;
}

static void Net_NotifyServerState(const W5500_SocketStatus_t *status)
{
    uint8_t on;
    uint8_t flags;

    if (status == NULL)
    {
        return;
    }

    on = ((status->flags & W5500_SOCK_FLAG_CONN) != 0U) ? 1U : 0U;
    flags = Net_BuildUiFlags(status);

    if (on != g_app.server_on)
    {
        g_app.server_on = on;
        if (on != 0U)
        {
            LOG_INFO("NET", "Server connected");
            g_last_send_tick = Delay_GetTick();
        }
        else
        {
            LOG_INFO("NET", "Server disconnected");
            g_last_reconnect_tick = Delay_GetTick();
        }
        Net_UpdateOledLine4Only(flags);
    }
}

/* ==================== W5500 EXTI ==================== */

static void Net_W5500ExtiCallback(EXTI_Line_t line, void *user_data)
{
    (void)line;
    (void)user_data;
    g_w5500_irq_pending = 1U;
}

static error_code_t Net_InitW5500Exti(void)
{
    EXTI_Status_t st;

    g_w5500_irq_pending = 0U;

    st = EXTI_HW_Init(W5500_EXTI_LINE, EXTI_TRIGGER_RISING_FALLING, EXTI_MODE_INTERRUPT);
    if (st != EXTI_OK)
    {
        LOG_ERROR("NET", "EXTI init fail: %d", (int)st);
        return (error_code_t)st;
    }

    st = EXTI_SetCallback(W5500_EXTI_LINE, Net_W5500ExtiCallback, NULL);
    if (st != EXTI_OK)
    {
        return (error_code_t)st;
    }

    st = EXTI_Enable(W5500_EXTI_LINE);
    if (st != EXTI_OK)
    {
        return (error_code_t)st;
    }

    LOG_INFO("NET", "W5500 EXTI PF9 edge OK");
    return ERROR_OK;
}

/* ==================== 网络业务 ==================== */

static void Net_ServiceW5500(void)
{
    W5500_Status_t st;
    uint8_t hint;

    hint = g_w5500_irq_pending;
    g_w5500_irq_pending = 0U;

    st = W5500_ProcessEvents(hint);
    if (st != W5500_OK)
    {
        LOG_WARN("NET", "ProcessEvents: %d", (int)st);
    }
}

static W5500_Status_t Net_StartTcpClient(void)
{
    W5500_SocketConfig_t sock_cfg;
    W5500_Status_t st;

    memset(&sock_cfg, 0, sizeof(sock_cfg));
    sock_cfg.local_port = NET_LOCAL_PORT;
    memcpy(sock_cfg.dest_ip, g_app.server_ip, 4U);
    sock_cfg.dest_port = g_app.server_port;

    st = W5500_SocketInit(W5500_SOCKET_0, &sock_cfg);
    if (st != W5500_OK)
    {
        return st;
    }

    st = W5500_SocketConnect(W5500_SOCKET_0);
    if (st != W5500_OK)
    {
        return st;
    }

    LOG_INFO("NET", "TCP Client connecting %u.%u.%u.%u:%u",
             g_app.server_ip[0], g_app.server_ip[1],
             g_app.server_ip[2], g_app.server_ip[3],
             (unsigned int)g_app.server_port);
    return W5500_OK;
}

static void Net_EnsureSocketConnected(void)
{
    W5500_Status_t st;
    W5500_SocketStatus_t status;
    uint32_t now;

    st = W5500_GetSocketStatus(W5500_SOCKET_0, &status);
    if (st != W5500_OK)
    {
        return;
    }

    if ((status.flags & W5500_SOCK_FLAG_INIT) != 0U)
    {
        return;
    }

    now = Delay_GetTick();
    if (g_last_reconnect_tick != 0U)
    {
        if (Delay_GetElapsed(now, g_last_reconnect_tick) < NET_RECONNECT_MS)
        {
            return;
        }
    }
    g_last_reconnect_tick = now;

    st = Net_StartTcpClient();
    if (st == W5500_OK)
    {
        Net_UpdateOledLine4Only(NET_UI_FLAG_TRYING | (g_app.gw_ok ? NET_UI_FLAG_GW : 0U));
    }
    else
    {
        LOG_WARN("NET", "SocketConnect fail: %d", (int)st);
    }
}

static void Net_ProcessReceive(void)
{
    W5500_Status_t st;
    W5500_SocketStatus_t status;
    uint16_t read_len;
    uint32_t now;

    (void)W5500_SyncSocketState(W5500_SOCKET_0);

    st = W5500_GetSocketStatus(W5500_SOCKET_0, &status);
    if (st != W5500_OK)
    {
        return;
    }
    /* 与周期发送相同：INIT+CONN 时才读 RX */
    if ((status.flags & (W5500_SOCK_FLAG_INIT | W5500_SOCK_FLAG_CONN)) !=
        (W5500_SOCK_FLAG_INIT | W5500_SOCK_FLAG_CONN))
    {
        return;
    }

    /* 轮询 Sn_RX_RSR，不依赖 RECV 软件事件（避免 EXTI 漏检收不到） */
    for (;;)
    {
        read_len = 0U;
        st = W5500_SocketRead(W5500_SOCKET_0, g_rx_buffer, NET_RX_BUF_SIZE, &read_len);
        if (st != W5500_OK)
        {
            LOG_WARN("NET", "SocketRead fail: %d", (int)st);
            return;
        }
        if (read_len == 0U)
        {
            now = Delay_GetTick();
            if (g_last_rx_idle_log_tick == 0U)
            {
                g_last_rx_idle_log_tick = now;
            }
            else if (Delay_GetElapsed(now, g_last_rx_idle_log_tick) >= 10000U)
            {
                g_last_rx_idle_log_tick = now;
                LOG_DEBUG("NET", "RX idle: server must send data (use Net01/02 echo or PC tool reply)");
            }
            return;
        }

        g_last_rx_idle_log_tick = 0U;
        (void)W5500_ClearSocketEvents(W5500_SOCKET_0, W5500_SOCK_EVT_RECEIVE);
        Net_UartDumpRx(read_len);
    }
}

static void Net_ProcessPeriodicSend(const W5500_SocketStatus_t *status)
{
    W5500_Status_t st;
    uint32_t now;

    if ((status->flags & (W5500_SOCK_FLAG_INIT | W5500_SOCK_FLAG_CONN)) !=
        (W5500_SOCK_FLAG_INIT | W5500_SOCK_FLAG_CONN))
    {
        return;
    }

    now = Delay_GetTick();
    if (Delay_GetElapsed(now, g_last_send_tick) < NET_SEND_INTERVAL_MS)
    {
        return;
    }
    g_last_send_tick = now;

    (void)W5500_ClearSocketEvents(W5500_SOCKET_0, W5500_SOCK_EVT_TX_OK);
    st = W5500_SocketWrite(W5500_SOCKET_0, g_client_msg,
                           (uint16_t)(sizeof(g_client_msg) - 1U));
    if (st != W5500_OK)
    {
        LOG_WARN("NET", "send fail: %d", (int)st);
    }
    else
    {
        LOG_INFO("NET", "TX %u bytes to server",
                 (unsigned int)(sizeof(g_client_msg) - 1U));
    }
}

static W5500_Status_t Net_InitW5500(void)
{
    W5500_Status_t st;
    uint8_t version = 0U;
    uint8_t oled_ready;

    oled_ready = g_app.oled_ready;
    memset(&g_app, 0, sizeof(g_app));
    g_app.oled_ready = oled_ready;

    st = Net_BuildNetConfig(&g_app.net);
    if (st != W5500_OK)
    {
        return st;
    }

    LOG_INFO("NET", "MAC %02X:%02X:%02X:%02X:%02X:%02X",
             g_app.net.mac[0], g_app.net.mac[1], g_app.net.mac[2],
             g_app.net.mac[3], g_app.net.mac[4], g_app.net.mac[5]);
    LOG_INFO("NET", "IP  %u.%u.%u.%u",
             g_app.net.ip[0], g_app.net.ip[1], g_app.net.ip[2], g_app.net.ip[3]);
    LOG_INFO("NET", "Server %u.%u.%u.%u:%u",
             g_app.server_ip[0], g_app.server_ip[1],
             g_app.server_ip[2], g_app.server_ip[3],
             (unsigned int)g_app.server_port);

    st = W5500_Init();
    if (st != W5500_OK)
    {
        return st;
    }

    st = W5500_SetNetConfig(&g_app.net);
    if (st != W5500_OK)
    {
        return st;
    }

    st = W5500_HardwareReset();
    if (st != W5500_OK)
    {
        LOG_WARN("NET", "wait link: %d (check cable)", (int)st);
    }

    st = W5500_ReadVersion(&version);
    if (st == W5500_OK)
    {
        LOG_INFO("NET", "W5500 version 0x%02X", version);
    }

    st = W5500_GetLinkStatus(&g_app.phy_linked);
    if (st == W5500_OK)
    {
        LOG_INFO("NET", "PHY link: %s", g_app.phy_linked ? "UP" : "DOWN");
    }

#if NET_GATEWAY_DETECT_EN
    st = W5500_DetectGateway();
    if (st != W5500_OK)
    {
        LOG_WARN("NET", "gateway detect: %d (LAN OK)", (int)st);
        g_app.gw_ok = 0U;
    }
    else
    {
        LOG_INFO("NET", "gateway OK");
        g_app.gw_ok = 1U;
    }
#else
    g_app.gw_ok = 0U;
#endif

    st = Net_StartTcpClient();
    if (st != W5500_OK)
    {
        return st;
    }

    st = W5500_EnableChipInterrupt();
    if (st != W5500_OK)
    {
        LOG_ERROR("NET", "EnableChipInterrupt fail: %d", (int)st);
        return st;
    }

    (void)W5500_ProcessEvents(1U);
    if (Net_InitW5500Exti() != ERROR_OK)
    {
        return W5500_ERROR_INIT_FAILED;
    }
    (void)W5500_ConfigureIntPin();
    (void)EXTI_ClearPending(W5500_EXTI_LINE);
    if (W5500_IsInterruptActive() != 0U)
    {
        g_w5500_irq_pending = 1U;
    }

    g_last_send_tick = Delay_GetTick();
    g_app.server_on = 0U;
    Net_UpdateOledFull(NET_UI_FLAG_TRYING | (g_app.gw_ok ? NET_UI_FLAG_GW : 0U));
    return W5500_OK;
}

static void Net_ProcessOnce(void)
{
    W5500_Status_t st;
    W5500_SocketStatus_t status;

    Net_ServiceW5500();
    Net_EnsureSocketConnected();

    st = W5500_GetSocketStatus(W5500_SOCKET_0, &status);
    if (st != W5500_OK)
    {
        return;
    }

    Net_NotifyServerState(&status);
    Net_ProcessReceive();
    Net_ProcessPeriodicSend(&status);
    /* 发送 hello 后立刻再读一次，便于捕获 Net01/02 回显 */
    Net_ProcessReceive();
}

/* ==================== 主函数 ==================== */

int main(void)
{
    W5500_Status_t st;

    if (Board_EarlyInit() != ERROR_OK)
    {
        Net_FatalBlink();
    }

    System_Init();
    Net_InitDebug();
    Net_InitOled();

    LOG_INFO("MAIN", "=== Net03 W5500 TCP Client EXTI ===");

    st = Net_InitW5500();
    if (st != W5500_OK)
    {
        LOG_ERROR("MAIN", "W5500 init fail: %d", (int)st);
        ErrorHandler_Handle((error_code_t)st, "MAIN");
        Net_FatalBlink();
    }

    LED1_On();

    while (1)
    {
        Net_ProcessOnce();
    }
}
