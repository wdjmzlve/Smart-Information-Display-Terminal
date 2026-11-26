/**
 * @file screen_types.h
 * @brief 云播报屏公共类型定义
 *
 * 跨任务通信统一走 screen_event_t，避免直接共享业务结构体。
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-11-25
 * @version 1.0
 */

#ifndef SCREEN_TYPES_H
#define SCREEN_TYPES_H

#include <stdint.h>

/**
 * @brief 应用事件类型
 *
 * 每新增一类跨任务消息就在这里增加一项，新增后同步更新 screen_app.c
 * 的事件分发 switch，否则会落到 default 分支被忽略。
 */
typedef enum
{
    SCREEN_EV_NONE = 0,     /*!< 无效事件，便于 memset 后判空 */
    SCREEN_EV_TICK,         /*!< 周期 tick，骨架阶段用于验证任务与定时器 */
    SCREEN_EV_MAX           /*!< 枚举上界，不参与业务 */
} screen_event_id_e;

/**
 * @brief 事件消息体
 *
 * 定长结构体可直接放进 RTOS 队列，避免队列内出现指针。
 */
typedef struct
{
    screen_event_id_e id;       /*!< 事件类型 */
    uint32_t          tick_ms;  /*!< 事件携带的周期值，单位 ms */
} screen_event_t;

#endif /* SCREEN_TYPES_H */
