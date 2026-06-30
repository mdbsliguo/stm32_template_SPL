/**
 * @file paho_platform.c
 * @brief Paho MQTT Embedded C 平台层实现（W5500 Socket0 TCP）
 */

#include "config.h"

#if CONFIG_MODULE_PAHO_MQTT_CLIENT_ENABLED

#include "paho_platform.h"
#include "w5500.h"
#include "delay.h"
#include <string.h>
#include <stdio.h>

#define PAHO_NET_IO_CHUNK       512U
#define PAHO_TCP_WAIT_MS        5000U

static uint8_t g_broker_ip[4];
static uint16_t g_broker_port;
static uint16_t g_local_port;

static W5500_Status_t paho_platform_wait_tcp_connected(uint32_t timeout_ms)
{
    uint32_t start;
    W5500_SocketStatus_t status;

    start = Delay_GetTick();
    while (Delay_GetElapsed(Delay_GetTick(), start) < timeout_ms)
    {
        (void)W5500_SyncSocketState(W5500_SOCKET_0);
        if (W5500_GetSocketStatus(W5500_SOCKET_0, &status) == W5500_OK)
        {
            if ((status.flags & (W5500_SOCK_FLAG_INIT | W5500_SOCK_FLAG_CONN)) ==
                (W5500_SOCK_FLAG_INIT | W5500_SOCK_FLAG_CONN))
            {
                return W5500_OK;
            }
        }
        Delay_ms(10U);
    }
    return W5500_ERROR_TIMEOUT;
}

static W5500_Status_t paho_platform_open_tcp(void)
{
    W5500_SocketConfig_t sock_cfg;
    W5500_Status_t st;

    (void)W5500_SocketClose(W5500_SOCKET_0);

    memset(&sock_cfg, 0, sizeof(sock_cfg));
    sock_cfg.local_port = g_local_port;
    memcpy(sock_cfg.dest_ip, g_broker_ip, 4U);
    sock_cfg.dest_port = g_broker_port;

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

    return paho_platform_wait_tcp_connected(PAHO_TCP_WAIT_MS);
}

static int paho_platform_mqtt_read(Network *network,
                                   unsigned char *buffer,
                                   int len,
                                   int timeout_ms)
{
    uint32_t start;
    int total;

    if ((network == NULL) || (buffer == NULL) || (len <= 0))
    {
        return -1;
    }

    start = Delay_GetTick();
    total = 0;
    while (total < len)
    {
        W5500_Status_t st;
        uint16_t read_len;

        if (Delay_GetElapsed(Delay_GetTick(), start) >= (uint32_t)timeout_ms)
        {
            break;
        }

        (void)W5500_SyncSocketState(W5500_SOCKET_0);
        read_len = 0U;
        st = W5500_SocketRead(W5500_SOCKET_0,
                              buffer + total,
                              (uint16_t)(len - total),
                              &read_len);
        if (st != W5500_OK)
        {
            return (total > 0) ? total : -1;
        }
        if (read_len == 0U)
        {
            Delay_ms(1U);
            continue;
        }
        total += (int)read_len;
    }

    return total;
}

static int paho_platform_mqtt_write(Network *network,
                                    unsigned char *buffer,
                                    int len,
                                    int timeout_ms)
{
    uint32_t start;
    int total;

    if ((network == NULL) || (buffer == NULL) || (len <= 0))
    {
        return -1;
    }

    start = Delay_GetTick();
    total = 0;
    while (total < len)
    {
        W5500_Status_t st;
        uint16_t chunk;

        if (Delay_GetElapsed(Delay_GetTick(), start) >= (uint32_t)timeout_ms)
        {
            break;
        }

        chunk = (uint16_t)(((len - total) > PAHO_NET_IO_CHUNK) ?
                           PAHO_NET_IO_CHUNK : (len - total));
        st = W5500_SocketWrite(W5500_SOCKET_0, buffer + total, chunk);
        if (st != W5500_OK)
        {
            return (total > 0) ? total : -1;
        }
        total += (int)chunk;
    }

    return (total == len) ? total : -1;
}

void TimerInit(Timer *timer)
{
    if (timer != NULL)
    {
        timer->end_tick = 0U;
    }
}

char TimerIsExpired(Timer *timer)
{
    if (timer == NULL)
    {
        return 1;
    }
    return (Delay_GetTick() >= timer->end_tick) ? 1 : 0;
}

void TimerCountdownMS(Timer *timer, unsigned int timeout_ms)
{
    if (timer != NULL)
    {
        timer->end_tick = Delay_GetTick() + timeout_ms;
    }
}

void TimerCountdown(Timer *timer, unsigned int timeout_sec)
{
    TimerCountdownMS(timer, timeout_sec * 1000U);
}

int TimerLeftMS(Timer *timer)
{
    uint32_t now;

    if (timer == NULL)
    {
        return 0;
    }

    now = Delay_GetTick();
    if (now >= timer->end_tick)
    {
        return 0;
    }
    return (int)(timer->end_tick - now);
}

void NetworkInit(Network *network)
{
    if (network == NULL)
    {
        return;
    }

    network->local_port = g_local_port;
    network->tcp_connected = 0U;
    network->mqttread = paho_platform_mqtt_read;
    network->mqttwrite = paho_platform_mqtt_write;
}

void Paho_Platform_SetEndpoint(const uint8_t broker_ip[4],
                               uint16_t broker_port,
                               uint16_t local_port)
{
    if (broker_ip != NULL)
    {
        memcpy(g_broker_ip, broker_ip, 4U);
    }
    g_broker_port = broker_port;
    g_local_port = local_port;
}

int NetworkConnect(Network *network, char *addr, int port)
{
    W5500_Status_t st;
    unsigned int a;
    unsigned int b;
    unsigned int c;
    unsigned int d;

    (void)addr;
    (void)port;

    if (network == NULL)
    {
        return -1;
    }

    {
        uint8_t phy_linked;

        if ((W5500_GetLinkStatus(&phy_linked) != W5500_OK) || (phy_linked == 0U))
        {
            network->tcp_connected = 0U;
            return -1;
        }
    }

    if (sscanf(addr, "%u.%u.%u.%u", &a, &b, &c, &d) == 4)
    {
        g_broker_ip[0] = (uint8_t)a;
        g_broker_ip[1] = (uint8_t)b;
        g_broker_ip[2] = (uint8_t)c;
        g_broker_ip[3] = (uint8_t)d;
    }

    g_broker_port = (uint16_t)port;
    network->local_port = g_local_port;

    st = paho_platform_open_tcp();
    if (st != W5500_OK)
    {
        network->tcp_connected = 0U;
        return -1;
    }

    network->tcp_connected = 1U;
    return 0;
}

void NetworkDisconnect(Network *network)
{
    if (network == NULL)
    {
        return;
    }

    (void)W5500_SocketClose(W5500_SOCKET_0);
    network->tcp_connected = 0U;
}

#endif /* CONFIG_MODULE_PAHO_MQTT_CLIENT_ENABLED */
