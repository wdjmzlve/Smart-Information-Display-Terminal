/**
 * @file user_main.c
 * @brief 云播报屏（项目10）程序入口
 *
 * 系统层创建 10 KB 用户任务后调用本函数，初始化失败时保持空转，
 * 避免带着未初始化的状态进入事件循环。
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-11-25
 * @version 1.0
 */

#include "liot_log.h"
#include "liot_os.h"

#include "screen_app.h"

/**
 * @brief 用户任务入口
 * @return 无
 */
void user_main(void)
{
    liot_trace("==== SCREEN_APP demo ====");

    /* 灯效由 screen_app_init() 统一初始化，这里不再重复调用，避免 SPI 被重复初始化 */
    if (screen_app_init() != 0)
    {
        liot_trace("[APP] init failed, halt");
        while (1)
        {
            liot_rtos_task_sleep_s(1);
        }
    }

    liot_trace("[APP] enter event loop");
    screen_app_run_forever();
}
