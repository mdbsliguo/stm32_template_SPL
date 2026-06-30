#if !defined(__MQTT_PAL_H__)
#define __MQTT_PAL_H__

/**
 * @file mqtt_pal.h
 * @brief MQTT-C 平台抽象层（STM32 + W5500 裸机）
 */

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(__unix__) || defined(__APPLE__) || defined(__NuttX__)
    #include <limits.h>
    #include <string.h>
    #include <stdarg.h>
    #include <time.h>
    #include <arpa/inet.h>
    #include <pthread.h>

    #define MQTT_PAL_HTONS(s) htons(s)
    #define MQTT_PAL_NTOHS(s) ntohs(s)
    #define MQTT_PAL_TIME() time(NULL)
    typedef time_t mqtt_pal_time_t;
    typedef pthread_mutex_t mqtt_pal_mutex_t;
    #define MQTT_PAL_MUTEX_INIT(mtx_ptr) pthread_mutex_init(mtx_ptr, NULL)
    #define MQTT_PAL_MUTEX_LOCK(mtx_ptr) pthread_mutex_lock(mtx_ptr)
    #define MQTT_PAL_MUTEX_UNLOCK(mtx_ptr) pthread_mutex_unlock(mtx_ptr)
    typedef int mqtt_pal_socket_handle;

#elif defined(_MSC_VER) || defined(WIN32)
    #include <limits.h>
    #include <winsock2.h>
    #include <windows.h>
    #include <time.h>
    #include <stdint.h>

    typedef SSIZE_T ssize_t;
    #define MQTT_PAL_HTONS(s) htons(s)
    #define MQTT_PAL_NTOHS(s) ntohs(s)
    #define MQTT_PAL_TIME() time(NULL)
    typedef time_t mqtt_pal_time_t;
    typedef CRITICAL_SECTION mqtt_pal_mutex_t;
    #define MQTT_PAL_MUTEX_INIT(mtx_ptr) InitializeCriticalSection(mtx_ptr)
    #define MQTT_PAL_MUTEX_LOCK(mtx_ptr) EnterCriticalSection(mtx_ptr)
    #define MQTT_PAL_MUTEX_UNLOCK(mtx_ptr) LeaveCriticalSection(mtx_ptr)
    typedef SOCKET mqtt_pal_socket_handle;

#else
    /* STM32 裸机：W5500 TCP 由 mqtt_pal.c 对接 */
    #include <limits.h>
    #include <stdint.h>
    #include <stddef.h>
    #include <string.h>
    #include <stdarg.h>

    typedef int32_t ssize_t;
    typedef uint32_t mqtt_pal_time_t;
    typedef uint8_t mqtt_pal_mutex_t;
    typedef int mqtt_pal_socket_handle;

    #define MQTT_PAL_HTONS(s) \
        ((uint16_t)((((uint16_t)(s) & 0xFFU) << 8) | (((uint16_t)(s) >> 8) & 0xFFU)))
    #define MQTT_PAL_NTOHS(s) MQTT_PAL_HTONS(s)

    uint32_t MQTT_Pal_GetTimeSec(void);

    #define MQTT_PAL_TIME() ((mqtt_pal_time_t)MQTT_Pal_GetTimeSec())

    #define MQTT_PAL_MUTEX_INIT(mtx_ptr) ((void)(mtx_ptr))
    #define MQTT_PAL_MUTEX_LOCK(mtx_ptr)   ((void)(mtx_ptr))
    #define MQTT_PAL_MUTEX_UNLOCK(mtx_ptr) ((void)(mtx_ptr))

#endif

ssize_t mqtt_pal_sendall(mqtt_pal_socket_handle fd, const void *buf, size_t len, int flags);
ssize_t mqtt_pal_recvall(mqtt_pal_socket_handle fd, void *buf, size_t bufsz, int flags);

#if defined(__cplusplus)
}
#endif

#endif /* __MQTT_PAL_H__ */
