/**
 * @file paho_mqtt_client.c
 * @brief Paho MQTT Embedded C 客户端封装实现
 */

#include "config.h"

#if CONFIG_MODULE_PAHO_MQTT_CLIENT_ENABLED

#define MQTTCLIENT_PLATFORM_HEADER paho_platform.h

#include "paho_mqtt_client.h"
#include "paho_platform.h"
#include "MQTTClient.h"
#include "w5500.h"
#include "delay.h"
#include "log.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    PAHO_MQTT_Client_Config_t cfg;
    Network network;
    MQTTClient client;
    unsigned char sendbuf[PAHO_MQTT_CLIENT_SEND_BUF_SIZE];
    unsigned char readbuf[PAHO_MQTT_CLIENT_RECV_BUF_SIZE];
    PAHO_MQTT_Client_MessageCallback_t message_cb;
    void *user_data;
    uint8_t initialized;
    uint8_t tcp_ready;
    uint8_t publish_ready;
    uint8_t session_ready;
    uint8_t subscribed;
    uint8_t need_reconnect;
    uint8_t connack_session_present;
    uint8_t offline_drain_pending;
    uint8_t offline_draining;
    uint8_t mqtt_ever_connected;
    uint8_t yield_fail_count;
    uint32_t drain_start_tick;
    uint32_t drain_last_rx_tick;
    int last_rc;
    char broker_host[20];
    char reg_topics[PAHO_MQTT_CLIENT_EXTRA_HANDLERS_MAX][PAHO_MQTT_CLIENT_TOPIC_MAX];
    uint8_t reg_topic_count;
} PAHO_MQTT_Client_Context_t;

static PAHO_MQTT_Client_Context_t g_paho;

typedef struct {
    uint8_t valid;
    char topic[PAHO_MQTT_CLIENT_TOPIC_MAX];
    uint8_t payload[PAHO_MQTT_CLIENT_PAYLOAD_MAX];
    uint16_t topic_len;
    uint16_t payload_len;
} PAHO_MQTT_PendingMsg_t;

typedef struct {
    PAHO_MQTT_PendingMsg_t items[PAHO_MQTT_PENDING_QUEUE_DEPTH];
    uint8_t head;
    uint8_t count;
} PAHO_MQTT_PendingQueue_t;

static PAHO_MQTT_PendingQueue_t g_pending_queue;

static void paho_mqtt_message_arrived(MessageData *data);
static void paho_mqtt_dispatch_pending(void);
static PAHO_MQTT_Client_Status_t paho_mqtt_ensure_tcp(void);
static PAHO_MQTT_Client_Status_t paho_mqtt_register_sub_handler(void);
static PAHO_MQTT_Client_Status_t paho_mqtt_register_all_handlers(void);
static void paho_mqtt_disconnect_all(void);

static uint8_t paho_mqtt_tcp_is_healthy(void)
{
    W5500_SocketStatus_t status;
    uint8_t phy_linked;

    if (g_paho.network.tcp_connected == 0U)
    {
        return 0U;
    }

    if (W5500_GetLinkStatus(&phy_linked) == W5500_OK)
    {
        if (phy_linked == 0U)
        {
            return 0U;
        }
    }

    (void)W5500_SyncSocketState(W5500_SOCKET_0);
    if (W5500_GetSocketStatus(W5500_SOCKET_0, &status) != W5500_OK)
    {
        return 0U;
    }

    return (uint8_t)(((status.flags & (W5500_SOCK_FLAG_INIT | W5500_SOCK_FLAG_CONN)) ==
                      (W5500_SOCK_FLAG_INIT | W5500_SOCK_FLAG_CONN)) ? 1U : 0U);
}

static uint8_t paho_mqtt_link_ok(void)
{
    return (uint8_t)((paho_mqtt_tcp_is_healthy() != 0U) &&
                     (MQTTIsConnected(&g_paho.client) != 0));
}

static uint8_t paho_mqtt_has_sub_handler(void)
{
    int i;

    for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
    {
        if (g_paho.client.messageHandlers[i].fp != NULL)
        {
            return 1U;
        }
    }
    return 0U;
}

static void paho_mqtt_reset_client_state(void)
{
    int i;

    g_paho.client.isconnected = 0;
    g_paho.client.ping_outstanding = 0;
    g_paho.subscribed = 0U;
    for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
    {
        g_paho.client.messageHandlers[i].topicFilter = NULL;
        g_paho.client.messageHandlers[i].fp = NULL;
    }
}

static void paho_mqtt_clear_session(void)
{
    g_paho.tcp_ready = 0U;
    g_paho.publish_ready = 0U;
    g_paho.session_ready = 0U;
    g_paho.subscribed = 0U;
    g_paho.connack_session_present = 0U;
    g_paho.mqtt_ever_connected = 0U;
    g_paho.network.tcp_connected = 0U;
    g_paho.offline_draining = 0U;
    g_paho.drain_start_tick = 0U;
    g_paho.drain_last_rx_tick = 0U;
    g_paho.yield_fail_count = 0U;
    paho_mqtt_reset_client_state();
}

static PAHO_MQTT_Client_Status_t paho_mqtt_reopen_tcp(void)
{
    (void)W5500_SocketClose(W5500_SOCKET_0);
    g_paho.tcp_ready = 0U;
    g_paho.network.tcp_connected = 0U;
    g_paho.client.isconnected = 0;
    return paho_mqtt_ensure_tcp();
}

static PAHO_MQTT_Client_Status_t paho_mqtt_do_mqtt_connect(uint8_t *session_present_out)
{
    MQTTPacket_connectData options = MQTTPacket_connectData_initializer;
    MQTTConnackData connack = {0};
    PAHO_MQTT_Client_Status_t st;
    int rc;

    paho_mqtt_reset_client_state();

    options.clientID.cstring = g_paho.cfg.client_id;
    options.keepAliveInterval = g_paho.cfg.keep_alive_sec;
    options.cleansession = (g_paho.cfg.clean_session != 0U) ? (char)1 : (char)0;
    options.MQTTVersion = 4;

    st = paho_mqtt_register_all_handlers();
    if (st != PAHO_MQTT_CLIENT_OK)
    {
        return st;
    }

    rc = MQTTConnectWithResults(&g_paho.client, &options, &connack);
    if (rc != PAHO_MQTT_RC_SUCCESS)
    {
        g_paho.last_rc = rc;
        LOG_WARN("MQTT", "MQTTConnect fail rc=%d connack=%u", rc, (unsigned int)connack.rc);
        (void)W5500_SocketClose(W5500_SOCKET_0);
        g_paho.tcp_ready = 0U;
        g_paho.network.tcp_connected = 0U;
        g_paho.client.isconnected = 0;
        return PAHO_MQTT_CLIENT_ERROR_PROTOCOL;
    }

    if (connack.rc != MQTT_CONNECTION_ACCEPTED)
    {
        g_paho.last_rc = (int)connack.rc;
        LOG_WARN("MQTT", "CONNACK rejected rc=%u", (unsigned int)connack.rc);
        (void)W5500_SocketClose(W5500_SOCKET_0);
        g_paho.tcp_ready = 0U;
        g_paho.network.tcp_connected = 0U;
        g_paho.client.isconnected = 0;
        return PAHO_MQTT_CLIENT_ERROR_PROTOCOL;
    }

    g_paho.mqtt_ever_connected = 1U;
    g_paho.connack_session_present = connack.sessionPresent;
    if (session_present_out != NULL)
    {
        *session_present_out = connack.sessionPresent;
    }

    if ((connack.sessionPresent != 0U) && (g_paho.cfg.clean_session == 0U))
    {
        g_paho.offline_drain_pending = 1U;
    }

    LOG_INFO("MQTT", "CONNACK ok id=%s CleanSession=%u SessionPresent=%u",
             g_paho.cfg.client_id,
             (unsigned int)g_paho.cfg.clean_session,
             (unsigned int)connack.sessionPresent);
    g_paho.publish_ready = 1U;
    return PAHO_MQTT_CLIENT_OK;
}

static PAHO_MQTT_Client_Status_t paho_mqtt_register_sub_handler(void)
{
    if (g_paho.cfg.sub_topic[0] == '\0')
    {
        return PAHO_MQTT_CLIENT_OK;
    }

    if (MQTTSetMessageHandler(&g_paho.client,
                              g_paho.cfg.sub_topic,
                              paho_mqtt_message_arrived) != PAHO_MQTT_RC_SUCCESS)
    {
        return PAHO_MQTT_CLIENT_ERROR_PROTOCOL;
    }

    return PAHO_MQTT_CLIENT_OK;
}

static PAHO_MQTT_Client_Status_t paho_mqtt_register_all_handlers(void)
{
    PAHO_MQTT_Client_Status_t st;
    uint8_t i;

    st = paho_mqtt_register_sub_handler();
    if (st != PAHO_MQTT_CLIENT_OK)
    {
        return st;
    }

    for (i = 0U; i < g_paho.reg_topic_count; i++)
    {
        if (g_paho.reg_topics[i][0] == '\0')
        {
            continue;
        }
        if (strcmp(g_paho.reg_topics[i], g_paho.cfg.sub_topic) == 0)
        {
            continue;
        }
        if (MQTTSetMessageHandler(&g_paho.client,
                                  g_paho.reg_topics[i],
                                  paho_mqtt_message_arrived) != PAHO_MQTT_RC_SUCCESS)
        {
            return PAHO_MQTT_CLIENT_ERROR_PROTOCOL;
        }
    }

    return PAHO_MQTT_CLIENT_OK;
}

static void paho_mqtt_finish_offline_drain(void)
{
    g_paho.offline_draining = 0U;
    g_paho.offline_drain_pending = 0U;

    if (g_pending_queue.count > 0U)
    {
        LOG_INFO("MQTT", "drain RX %u msg(s)",
                 (unsigned int)g_pending_queue.count);
    }
}

static void paho_mqtt_arm_offline_drain(void)
{
    if (g_paho.offline_drain_pending == 0U)
    {
        return;
    }

    if (g_paho.cfg.clean_session != 0U)
    {
        g_paho.offline_drain_pending = 0U;
        return;
    }

    if (g_paho.offline_draining != 0U)
    {
        return;
    }

    LOG_INFO("MQTT", "drain offline...");
    g_paho.offline_draining = 1U;
    g_paho.drain_start_tick = Delay_GetTick();
    g_paho.drain_last_rx_tick = 0U;
}

static void paho_mqtt_drain_step(void)
{
    uint8_t cnt_before;
    int rc;

    cnt_before = g_pending_queue.count;
    rc = MQTTYield(&g_paho.client, PAHO_MQTT_CLIENT_OFFLINE_DRAIN_YIELD_MS);
    if (g_pending_queue.count > cnt_before)
    {
        g_paho.drain_last_rx_tick = Delay_GetTick();
    }

    if ((g_paho.drain_last_rx_tick != 0U) &&
        (Delay_GetElapsed(Delay_GetTick(), g_paho.drain_last_rx_tick) >=
         PAHO_MQTT_CLIENT_OFFLINE_DRAIN_IDLE_MS))
    {
        paho_mqtt_finish_offline_drain();
        return;
    }

    if (Delay_GetElapsed(Delay_GetTick(), g_paho.drain_start_tick) >=
        PAHO_MQTT_CLIENT_OFFLINE_DRAIN_MS)
    {
        paho_mqtt_finish_offline_drain();
        return;
    }

    if ((rc != PAHO_MQTT_RC_SUCCESS) && (paho_mqtt_tcp_is_healthy() == 0U))
    {
        paho_mqtt_finish_offline_drain();
    }
}

static void paho_mqtt_disconnect_all(void)
{
    /* 与 Net05 一致：直接关 Socket，不在死链路上发 MQTT DISCONNECT */
    (void)W5500_SocketClose(W5500_SOCKET_0);
    paho_mqtt_clear_session();
    memset(&g_pending_queue, 0, sizeof(g_pending_queue));
}

static void paho_mqtt_queue_message(const char *topic,
                                    uint16_t topic_len,
                                    const uint8_t *payload,
                                    uint16_t payload_len)
{
    uint16_t copy_len;
    uint8_t idx;
    PAHO_MQTT_PendingMsg_t *drop;
    PAHO_MQTT_PendingMsg_t *item;

    if ((topic == NULL) || ((payload == NULL) && (payload_len > 0U)))
    {
        return;
    }
    if (payload_len == 0U)
    {
        return;
    }

    copy_len = payload_len;
    if (copy_len > PAHO_MQTT_CLIENT_PAYLOAD_MAX)
    {
        LOG_WARN("MQTT", "payload too large %u, truncate to %u",
                 (unsigned int)payload_len,
                 (unsigned int)PAHO_MQTT_CLIENT_PAYLOAD_MAX);
        copy_len = PAHO_MQTT_CLIENT_PAYLOAD_MAX;
    }

    if (g_pending_queue.count >= PAHO_MQTT_PENDING_QUEUE_DEPTH)
    {
        drop = &g_pending_queue.items[g_pending_queue.head];
        LOG_WARN("MQTT", "pending queue full, drop topic=%s len=%u",
                 (drop->valid != 0U) ? drop->topic : "?",
                 (unsigned int)((drop->valid != 0U) ? drop->payload_len : 0U));
        drop->valid = 0U;
        g_pending_queue.head = (uint8_t)((g_pending_queue.head + 1U) % PAHO_MQTT_PENDING_QUEUE_DEPTH);
        g_pending_queue.count--;
    }

    if (topic_len == 0U)
    {
        return;
    }

    if (topic_len >= PAHO_MQTT_CLIENT_TOPIC_MAX)
    {
        topic_len = (uint16_t)(PAHO_MQTT_CLIENT_TOPIC_MAX - 1U);
    }

    idx = (uint8_t)((g_pending_queue.head + g_pending_queue.count) % PAHO_MQTT_PENDING_QUEUE_DEPTH);
    item = &g_pending_queue.items[idx];
    memset(item, 0, sizeof(*item));
    memcpy(item->topic, topic, topic_len);
    item->topic[topic_len] = '\0';
    item->topic_len = topic_len;
    item->payload_len = copy_len;
    if ((copy_len > 0U) && (payload != NULL))
    {
        memcpy(item->payload, payload, copy_len);
    }
    item->valid = 1U;
    g_pending_queue.count++;
}

static void paho_mqtt_dispatch_pending(void)
{
    PAHO_MQTT_PendingMsg_t *msg;

    if ((g_pending_queue.count == 0U) || (g_paho.message_cb == NULL))
    {
        return;
    }

    msg = &g_pending_queue.items[g_pending_queue.head];
    if (msg->valid != 0U)
    {
        g_paho.message_cb(msg->topic,
                          msg->topic_len,
                          msg->payload,
                          msg->payload_len,
                          g_paho.user_data);
        msg->valid = 0U;
    }
    g_pending_queue.head = (uint8_t)((g_pending_queue.head + 1U) % PAHO_MQTT_PENDING_QUEUE_DEPTH);
    g_pending_queue.count--;
}

static void paho_mqtt_message_arrived(MessageData *data)
{
    MQTTMessage *message;
    MQTTString *topic;
    uint16_t topic_len;
    uint16_t payload_len;
    static char topic_buf[PAHO_MQTT_CLIENT_TOPIC_MAX];

    if ((data == NULL) || (data->message == NULL) || (data->topicName == NULL))
    {
        return;
    }

    message = data->message;
    topic = data->topicName;
    if (message->payloadlen <= 0)
    {
        return;
    }
    if (message->payloadlen > (int)PAHO_MQTT_CLIENT_PAYLOAD_MAX)
    {
        payload_len = PAHO_MQTT_CLIENT_PAYLOAD_MAX;
    }
    else
    {
        payload_len = (uint16_t)message->payloadlen;
    }

    if (topic->cstring != NULL)
    {
        paho_mqtt_queue_message(topic->cstring,
                                (uint16_t)strlen(topic->cstring),
                                (const uint8_t *)message->payload,
                                payload_len);
        return;
    }

    topic_len = (uint16_t)topic->lenstring.len;
    if (topic_len >= sizeof(topic_buf))
    {
        topic_len = (uint16_t)(sizeof(topic_buf) - 1U);
    }
    memcpy(topic_buf, topic->lenstring.data, topic_len);
    topic_buf[topic_len] = '\0';
    paho_mqtt_queue_message(topic_buf,
                            topic_len,
                            (const uint8_t *)message->payload,
                            payload_len);
}

static PAHO_MQTT_Client_Status_t paho_mqtt_ensure_tcp(void)
{
    int rc;

    if ((g_paho.tcp_ready != 0U) && (paho_mqtt_tcp_is_healthy() != 0U))
    {
        return PAHO_MQTT_CLIENT_OK;
    }

    if (g_paho.tcp_ready != 0U)
    {
        LOG_WARN("MQTT", "TCP stale, reopen socket");
        paho_mqtt_disconnect_all();
    }

    Paho_Platform_SetEndpoint(g_paho.cfg.broker_ip,
                              g_paho.cfg.broker_port,
                              g_paho.cfg.local_port);
    NetworkInit(&g_paho.network);

    (void)snprintf(g_paho.broker_host,
                   sizeof(g_paho.broker_host),
                   "%u.%u.%u.%u",
                   g_paho.cfg.broker_ip[0],
                   g_paho.cfg.broker_ip[1],
                   g_paho.cfg.broker_ip[2],
                   g_paho.cfg.broker_ip[3]);

    rc = NetworkConnect(&g_paho.network,
                        g_paho.broker_host,
                        (int)g_paho.cfg.broker_port);
    if (rc != 0)
    {
        g_paho.last_rc = rc;
        LOG_WARN("MQTT", "TCP connect broker fail: %d", rc);
        return PAHO_MQTT_CLIENT_ERROR_NETWORK;
    }

    MQTTClientInit(&g_paho.client,
                   &g_paho.network,
                   PAHO_MQTT_CLIENT_CMD_TIMEOUT_MS,
                   g_paho.sendbuf,
                   sizeof(g_paho.sendbuf),
                   g_paho.readbuf,
                   sizeof(g_paho.readbuf));

    LOG_INFO("MQTT", "TCP connected %s:%u",
             g_paho.broker_host,
             (unsigned int)g_paho.cfg.broker_port);
    g_paho.tcp_ready = 1U;
    return PAHO_MQTT_CLIENT_OK;
}

static PAHO_MQTT_Client_Status_t paho_mqtt_ensure_mqtt_connect(void)
{
    PAHO_MQTT_Client_Status_t st;

    if ((g_paho.session_ready != 0U) && (paho_mqtt_link_ok() != 0U))
    {
        g_paho.publish_ready = 1U;
        return PAHO_MQTT_CLIENT_OK;
    }

    if ((g_paho.session_ready != 0U) && (paho_mqtt_link_ok() == 0U))
    {
        if (paho_mqtt_tcp_is_healthy() != 0U)
        {
            g_paho.session_ready = 0U;
            g_paho.publish_ready = 0U;
        }
        else
        {
            paho_mqtt_disconnect_all();
            g_paho.need_reconnect = 1U;
            return PAHO_MQTT_CLIENT_ERROR_NETWORK;
        }
    }

    if ((g_paho.tcp_ready != 0U) &&
        (MQTTIsConnected(&g_paho.client) == 0) &&
        (g_paho.mqtt_ever_connected != 0U))
    {
        st = paho_mqtt_reopen_tcp();
        if (st != PAHO_MQTT_CLIENT_OK)
        {
            return st;
        }
    }

    g_paho.session_ready = 0U;
    g_paho.publish_ready = 0U;
    g_paho.connack_session_present = 0U;

    return paho_mqtt_do_mqtt_connect(NULL);
}

static PAHO_MQTT_Client_Status_t paho_mqtt_ensure_subscribe(void)
{
    enum QoS qos;
    int rc;

    if (g_paho.cfg.sub_topic[0] == '\0')
    {
        g_paho.session_ready = 1U;
        return PAHO_MQTT_CLIENT_OK;
    }

    if ((g_paho.subscribed != 0U) &&
        (paho_mqtt_has_sub_handler() != 0U) &&
        (paho_mqtt_link_ok() != 0U))
    {
        g_paho.session_ready = 1U;
        return PAHO_MQTT_CLIENT_OK;
    }

    g_paho.subscribed = 0U;
    qos = (g_paho.cfg.qos == 0U) ? QOS0 : QOS1;

    if ((g_paho.connack_session_present != 0U) &&
        (paho_mqtt_has_sub_handler() != 0U))
    {
        LOG_INFO("MQTT", "session restored listen %s QoS %u",
                 g_paho.cfg.sub_topic, (unsigned int)g_paho.cfg.qos);
        g_paho.subscribed = 1U;
        g_paho.session_ready = 1U;
        paho_mqtt_arm_offline_drain();
        return PAHO_MQTT_CLIENT_OK;
    }

    rc = MQTTSubscribe(&g_paho.client,
                       g_paho.cfg.sub_topic,
                       qos,
                       paho_mqtt_message_arrived);
    if (rc != PAHO_MQTT_RC_SUCCESS)
    {
        g_paho.last_rc = rc;
        LOG_WARN("MQTT", "MQTTSubscribe fail rc=%d", rc);
        paho_mqtt_disconnect_all();
        return PAHO_MQTT_CLIENT_ERROR_PROTOCOL;
    }

    LOG_INFO("MQTT", "SUBACK ok listen %s QoS %u",
             g_paho.cfg.sub_topic, (unsigned int)g_paho.cfg.qos);
    g_paho.subscribed = 1U;
    g_paho.session_ready = 1U;
    paho_mqtt_arm_offline_drain();
    return PAHO_MQTT_CLIENT_OK;
}

PAHO_MQTT_Client_Status_t PAHO_MQTT_Client_Init(const PAHO_MQTT_Client_Config_t *config,
                                                 PAHO_MQTT_Client_MessageCallback_t message_cb,
                                                 void *user_data)
{
    if (config == NULL)
    {
        return PAHO_MQTT_CLIENT_ERROR_NULL_PTR;
    }
    if ((config->client_id[0] == '\0') || (config->broker_port == 0U))
    {
        return PAHO_MQTT_CLIENT_ERROR_INVALID_PARAM;
    }
    if (config->qos > 1U)
    {
        return PAHO_MQTT_CLIENT_ERROR_INVALID_PARAM;
    }
    if (config->clean_session > 1U)
    {
        return PAHO_MQTT_CLIENT_ERROR_INVALID_PARAM;
    }

    memset(&g_paho, 0, sizeof(g_paho));
    memset(&g_pending_queue, 0, sizeof(g_pending_queue));
    memcpy(&g_paho.cfg, config, sizeof(PAHO_MQTT_Client_Config_t));
    g_paho.message_cb = message_cb;
    g_paho.user_data = user_data;
    g_paho.initialized = 1U;
    g_paho.need_reconnect = 1U;
    return PAHO_MQTT_CLIENT_OK;
}

PAHO_MQTT_Client_Status_t PAHO_MQTT_Client_Process(void)
{
    PAHO_MQTT_Client_Status_t st;
    int rc;

    if (g_paho.initialized == 0U)
    {
        return PAHO_MQTT_CLIENT_ERROR_NOT_INITIALIZED;
    }

    if (g_paho.need_reconnect != 0U)
    {
        paho_mqtt_disconnect_all();
        g_paho.need_reconnect = 0U;
    }

    st = paho_mqtt_ensure_tcp();
    if (st != PAHO_MQTT_CLIENT_OK)
    {
        return st;
    }

    st = paho_mqtt_ensure_mqtt_connect();
    if (st != PAHO_MQTT_CLIENT_OK)
    {
        return st;
    }

    st = paho_mqtt_ensure_subscribe();
    if (st != PAHO_MQTT_CLIENT_OK)
    {
        return st;
    }

    if (g_paho.offline_draining != 0U)
    {
        paho_mqtt_drain_step();
        return PAHO_MQTT_CLIENT_OK;
    }

    rc = MQTTYield(&g_paho.client, PAHO_MQTT_CLIENT_YIELD_MS);
    if (rc != PAHO_MQTT_RC_SUCCESS)
    {
        g_paho.last_rc = rc;
        if (paho_mqtt_tcp_is_healthy() == 0U)
        {
            paho_mqtt_disconnect_all();
            g_paho.need_reconnect = 1U;
            g_paho.yield_fail_count = 0U;
            return PAHO_MQTT_CLIENT_ERROR_PROTOCOL;
        }

        g_paho.yield_fail_count++;
        if (g_paho.yield_fail_count >= 5U)
        {
            LOG_WARN("MQTT", "yield fail %u, reconnect", (unsigned int)g_paho.yield_fail_count);
            paho_mqtt_disconnect_all();
            g_paho.need_reconnect = 1U;
            g_paho.yield_fail_count = 0U;
            return PAHO_MQTT_CLIENT_ERROR_PROTOCOL;
        }
        return PAHO_MQTT_CLIENT_OK;
    }

    g_paho.yield_fail_count = 0U;

    if (MQTTIsConnected(&g_paho.client) == 0)
    {
        if (paho_mqtt_tcp_is_healthy() != 0U)
        {
            g_paho.session_ready = 0U;
            g_paho.publish_ready = 0U;
            return PAHO_MQTT_CLIENT_OK;
        }

        paho_mqtt_clear_session();
        g_paho.need_reconnect = 1U;
        return PAHO_MQTT_CLIENT_ERROR_NETWORK;
    }

    if (paho_mqtt_link_ok() == 0U)
    {
        if (paho_mqtt_tcp_is_healthy() != 0U)
        {
            g_paho.session_ready = 0U;
            g_paho.publish_ready = 0U;
            return PAHO_MQTT_CLIENT_OK;
        }

        paho_mqtt_disconnect_all();
        g_paho.need_reconnect = 1U;
        return PAHO_MQTT_CLIENT_ERROR_NETWORK;
    }

    return PAHO_MQTT_CLIENT_OK;
}

PAHO_MQTT_Client_Status_t PAHO_MQTT_Client_Drain(uint8_t max_rounds)
{
    PAHO_MQTT_Client_Status_t st;
    uint8_t i;

    if (max_rounds == 0U)
    {
        return PAHO_MQTT_CLIENT_OK;
    }

    st = PAHO_MQTT_CLIENT_OK;
    for (i = 0U; i < max_rounds; i++)
    {
        st = PAHO_MQTT_Client_Process();
        if (st != PAHO_MQTT_CLIENT_OK)
        {
            break;
        }
    }
    (void)PAHO_MQTT_Client_FlushPending();
    return st;
}

PAHO_MQTT_Client_Status_t PAHO_MQTT_Client_Publish(const char *topic,
                                                     const void *payload,
                                                     uint16_t payload_len,
                                                     uint8_t qos)
{
    MQTTMessage message;
    enum QoS paho_qos;
    int rc;

    if (g_paho.initialized == 0U)
    {
        return PAHO_MQTT_CLIENT_ERROR_NOT_INITIALIZED;
    }
    if ((topic == NULL) || ((payload == NULL) && (payload_len > 0U)))
    {
        return PAHO_MQTT_CLIENT_ERROR_NULL_PTR;
    }
    if (g_paho.publish_ready == 0U)
    {
        return PAHO_MQTT_CLIENT_ERROR_BUSY;
    }
    if (g_paho.offline_draining != 0U)
    {
        return PAHO_MQTT_CLIENT_ERROR_BUSY;
    }
    if (payload_len > PAHO_MQTT_CLIENT_PAYLOAD_MAX)
    {
        return PAHO_MQTT_CLIENT_ERROR_INVALID_PARAM;
    }
    if (paho_mqtt_link_ok() == 0U)
    {
        paho_mqtt_disconnect_all();
        g_paho.need_reconnect = 1U;
        return PAHO_MQTT_CLIENT_ERROR_NETWORK;
    }

    memset(&message, 0, sizeof(message));
    message.qos = (qos == 0U) ? QOS0 : QOS1;
    message.retained = 0;
    message.dup = 0;
    message.payload = (void *)payload;
    message.payloadlen = payload_len;
    paho_qos = message.qos;

    rc = MQTTPublish(&g_paho.client, topic, &message);
    if (rc != PAHO_MQTT_RC_SUCCESS)
    {
        g_paho.last_rc = rc;
        paho_mqtt_disconnect_all();
        g_paho.need_reconnect = 1U;
        return PAHO_MQTT_CLIENT_ERROR_PROTOCOL;
    }

    if (qos != 0U)
    {
        rc = MQTTYield(&g_paho.client, 300);
        if (rc != PAHO_MQTT_RC_SUCCESS)
        {
            g_paho.last_rc = rc;
            if (paho_mqtt_tcp_is_healthy() == 0U)
            {
                paho_mqtt_disconnect_all();
                g_paho.need_reconnect = 1U;
                return PAHO_MQTT_CLIENT_ERROR_PROTOCOL;
            }
            return PAHO_MQTT_CLIENT_OK;
        }
        if (paho_mqtt_link_ok() == 0U)
        {
            paho_mqtt_disconnect_all();
            g_paho.need_reconnect = 1U;
            return PAHO_MQTT_CLIENT_ERROR_NETWORK;
        }
    }

    (void)paho_qos;
    return PAHO_MQTT_CLIENT_OK;
}

uint8_t PAHO_MQTT_Client_IsSessionReady(void)
{
    return (uint8_t)((g_paho.initialized != 0U) &&
                     (g_paho.session_ready != 0U) &&
                     (paho_mqtt_link_ok() != 0U));
}

uint8_t PAHO_MQTT_Client_CanPublish(void)
{
    return (uint8_t)((g_paho.initialized != 0U) &&
                     (g_paho.publish_ready != 0U) &&
                     (paho_mqtt_link_ok() != 0U) &&
                     (g_paho.offline_draining == 0U));
}

const char *PAHO_MQTT_Client_GetLastErrorStr(void)
{
    static char err_buf[24];

    (void)snprintf(err_buf, sizeof(err_buf), "rc=%d", g_paho.last_rc);
    return err_buf;
}

PAHO_MQTT_Client_Status_t PAHO_MQTT_Client_ForceReconnect(void)
{
    if (g_paho.initialized == 0U)
    {
        return PAHO_MQTT_CLIENT_ERROR_NOT_INITIALIZED;
    }

    paho_mqtt_disconnect_all();
    g_paho.need_reconnect = 1U;
    g_paho.offline_drain_pending = 1U;
    return PAHO_MQTT_CLIENT_OK;
}

PAHO_MQTT_Client_Status_t PAHO_MQTT_Client_FlushPending(void)
{
    uint8_t guard;

    if (g_paho.initialized == 0U)
    {
        return PAHO_MQTT_CLIENT_ERROR_NOT_INITIALIZED;
    }

    guard = 0U;
    while ((g_pending_queue.count > 0U) && (guard < PAHO_MQTT_PENDING_QUEUE_DEPTH))
    {
        paho_mqtt_dispatch_pending();
        guard++;
    }
    return PAHO_MQTT_CLIENT_OK;
}

PAHO_MQTT_Client_Status_t PAHO_MQTT_Client_RegisterHandler(const char *topic)
{
    uint8_t i;

    if (g_paho.initialized == 0U)
    {
        return PAHO_MQTT_CLIENT_ERROR_NOT_INITIALIZED;
    }
    if ((topic == NULL) || (topic[0] == '\0'))
    {
        return PAHO_MQTT_CLIENT_ERROR_INVALID_PARAM;
    }

    for (i = 0U; i < g_paho.reg_topic_count; i++)
    {
        if (strcmp(g_paho.reg_topics[i], topic) == 0)
        {
            return PAHO_MQTT_CLIENT_OK;
        }
    }

    if (g_paho.reg_topic_count >= PAHO_MQTT_CLIENT_EXTRA_HANDLERS_MAX)
    {
        return PAHO_MQTT_CLIENT_ERROR_BUSY;
    }

    (void)strncpy(g_paho.reg_topics[g_paho.reg_topic_count],
                  topic,
                  PAHO_MQTT_CLIENT_TOPIC_MAX - 1U);
    g_paho.reg_topics[g_paho.reg_topic_count][PAHO_MQTT_CLIENT_TOPIC_MAX - 1U] = '\0';
    g_paho.reg_topic_count++;

    if ((g_paho.tcp_ready != 0U) && (MQTTIsConnected(&g_paho.client) != 0))
    {
        if (MQTTSetMessageHandler(&g_paho.client,
                                  topic,
                                  paho_mqtt_message_arrived) != PAHO_MQTT_RC_SUCCESS)
        {
            return PAHO_MQTT_CLIENT_ERROR_PROTOCOL;
        }
    }

    return PAHO_MQTT_CLIENT_OK;
}

uint8_t PAHO_MQTT_Client_HasPending(void)
{
    return (uint8_t)(g_pending_queue.count > 0U);
}

PAHO_MQTT_Client_Status_t PAHO_MQTT_Client_SubscribeTopic(const char *topic, uint8_t qos)
{
    MQTTSubackData suback;
    enum QoS paho_qos;
    int rc;

    if (g_paho.initialized == 0U)
    {
        return PAHO_MQTT_CLIENT_ERROR_NOT_INITIALIZED;
    }
    if ((topic == NULL) || (topic[0] == '\0'))
    {
        return PAHO_MQTT_CLIENT_ERROR_INVALID_PARAM;
    }
    if (qos > 1U)
    {
        return PAHO_MQTT_CLIENT_ERROR_INVALID_PARAM;
    }
    if (paho_mqtt_link_ok() == 0U)
    {
        return PAHO_MQTT_CLIENT_ERROR_BUSY;
    }

    memset(&suback, 0, sizeof(suback));

    if (MQTTSetMessageHandler(&g_paho.client, topic, paho_mqtt_message_arrived) !=
        PAHO_MQTT_RC_SUCCESS)
    {
        return PAHO_MQTT_CLIENT_ERROR_PROTOCOL;
    }

    paho_qos = (qos == 0U) ? QOS0 : QOS1;
    rc = MQTTSubscribeWithResults(&g_paho.client,
                                  topic,
                                  paho_qos,
                                  paho_mqtt_message_arrived,
                                  &suback);
    if (rc != PAHO_MQTT_RC_SUCCESS)
    {
        g_paho.last_rc = rc;
        return PAHO_MQTT_CLIENT_ERROR_PROTOCOL;
    }

    if (suback.grantedQoS == SUBFAIL)
    {
        LOG_WARN("MQTT", "SUBACK fail topic=%s", topic);
        return PAHO_MQTT_CLIENT_ERROR_PROTOCOL;
    }

    LOG_INFO("MQTT", "SUBACK topic=%s req_qos=%u granted=%u",
             topic,
             (unsigned int)qos,
             (unsigned int)suback.grantedQoS);
    return PAHO_MQTT_CLIENT_OK;
}

uint8_t PAHO_MQTT_Client_IsSessionRestored(void)
{
    return g_paho.connack_session_present;
}

#endif /* CONFIG_MODULE_PAHO_MQTT_CLIENT_ENABLED */
