/**
 * @file paho_platform.h
 * @brief Paho MQTT Embedded C Æ½Ì¨²ã£¨W5500 + delay£©
 */

#ifndef PAHO_PLATFORM_H
#define PAHO_PLATFORM_H

#include <stdint.h>

typedef struct Timer
{
    uint32_t end_tick;
} Timer;

void TimerInit(Timer *timer);
char TimerIsExpired(Timer *timer);
void TimerCountdownMS(Timer *timer, unsigned int timeout_ms);
void TimerCountdown(Timer *timer, unsigned int timeout_sec);
int TimerLeftMS(Timer *timer);

typedef struct Network
{
    uint16_t local_port;
    uint8_t tcp_connected;
    int (*mqttread)(struct Network *, unsigned char *, int, int);
    int (*mqttwrite)(struct Network *, unsigned char *, int, int);
} Network;

void NetworkInit(Network *network);
void Paho_Platform_SetEndpoint(const uint8_t broker_ip[4],
                               uint16_t broker_port,
                               uint16_t local_port);
int NetworkConnect(Network *network, char *addr, int port);
void NetworkDisconnect(Network *network);

#endif /* PAHO_PLATFORM_H */
