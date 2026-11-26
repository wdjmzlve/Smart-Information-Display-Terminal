/**
 * @file screen_types.h
 * @brief 云播报屏公共类型定义
 *
 * 跨任务通信统一走 screen_event_t。下行报文体积较大，事件里只放指针：
 * 由生产者（MQTT 回调）申请内存，消费者（应用主任务）处理完后释放，
 * 避免在回调栈上放 512 字节的副本。
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-11-26
 * @version 1.1
 */

#ifndef SCREEN_TYPES_H
#define SCREEN_TYPES_H

#include <stdint.h>

/**
 * @brief 应用事件类型
 */
typedef enum
{
    SCREEN_EV_NONE = 0,     /*!< 无效事件，便于 memset 后判空 */
    SCREEN_EV_TICK,         /*!< 周期 tick，用于刷新状态栏 */
    SCREEN_EV_DOWNLINK,     /*!< 收到一条 MQTT 下行报文 */
    SCREEN_EV_MAX           /*!< 枚举上界，不参与业务 */
} screen_event_id_e;

/**
 * @brief 事件消息体
 *
 * data 仅 SCREEN_EV_DOWNLINK 使用：生产者负责申请，消费者负责释放。
 */
typedef struct
{
    screen_event_id_e id;       /*!< 事件类型 */
    uint32_t          tick_ms;  /*!< 事件携带的周期值，单位 ms */
    uint16_t          data_len; /*!< 下行载荷长度 */
    uint8_t          *data;     /*!< 下行载荷指针，可为 NULL */
} screen_event_t;

#endif /* SCREEN_TYPES_H */
