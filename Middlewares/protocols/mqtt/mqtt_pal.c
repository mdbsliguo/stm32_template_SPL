/**
 * @file mqtt_pal.c
 * @brief MQTT-C PAL: W5500 Socket0 TCP
 */

#include "config.h"

#if CONFIG_MODULE_MQTT_CLIENT_ENABLED

#include "mqtt_pal.h"
#include "mqtt.h"
#include "w5500.h"
#include "delay.h"
#include <string.h>

#define MQTT_PAL_IO_CHUNK   512U

uint32_t MQTT_Pal_GetTimeSec(void)
{
    return (uint32_t)(Delay_GetTick() / 1000U);
}

ssize_t mqtt_pal_sendall(mqtt_pal_socket_handle fd, const void *buf, size_t len, int flags)
{
    W5500_Status_t st;
    const uint8_t *data;
    size_t offset;
    uint16_t chunk;

    (void)fd;
    (void)flags;

    if ((buf == NULL) || (len == 0U))
    {
        return 0;
    }

    data = (const uint8_t *)buf;
    offset = 0U;
    while (offset < len)
    {
        chunk = (uint16_t)((len - offset) > MQTT_PAL_IO_CHUNK ? MQTT_PAL_IO_CHUNK : (len - offset));
        st = W5500_SocketWrite(W5500_SOCKET_0, data + offset, chunk);
        if (st != W5500_OK)
        {
            if (offset > 0U)
            {
                return (ssize_t)offset;
            }
            return MQTT_ERROR_SOCKET_ERROR;
        }
        offset += chunk;
    }
    return (ssize_t)len;
}

ssize_t mqtt_pal_recvall(mqtt_pal_socket_handle fd, void *buf, size_t bufsz, int flags)
{
    W5500_Status_t st;
    uint16_t read_len;
    uint8_t *out;
    size_t total;

    (void)fd;
    (void)flags;

    if ((buf == NULL) || (bufsz == 0U))
    {
        return 0;
    }

    out = (uint8_t *)buf;
    total = 0U;
    while (total < bufsz)
    {
        read_len = 0U;
        st = W5500_SocketRead(W5500_SOCKET_0, out + total,
                              (uint16_t)(bufsz - total), &read_len);
        if (st != W5500_OK)
        {
            if (total > 0U)
            {
                return (ssize_t)total;
            }
            return MQTT_ERROR_SOCKET_ERROR;
        }
        if (read_len == 0U)
        {
            break;
        }
        total += read_len;
    }
    return (ssize_t)total;
}

#endif /* CONFIG_MODULE_MQTT_CLIENT_ENABLED */
