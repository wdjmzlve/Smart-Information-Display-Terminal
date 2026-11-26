/**
 * @file screen_app.h
 * @brief 云播报屏应用层入口
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-11-25
 * @version 1.0
 */

#ifndef SCREEN_APP_H
#define SCREEN_APP_H

#include "screen_types.h"

/**
 * @brief 初始化应用事件队列与周期定时器
 * @return 0 成功，-1 失败
 */
int screen_app_init(void);

/**
 * @brief 向应用主任务投递一个事件
 * @param[in] event 待投递的事件，不能为 NULL
 * @return 0 成功，-1 参数错误或入队失败
 */
int screen_app_post(const screen_event_t *event);

/**
 * @brief 进入应用主循环（正常情况下不返回）
 * @return 无
 */
void screen_app_run_forever(void);

#endif /* SCREEN_APP_H */
