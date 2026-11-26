/**
 * @file screen_app.c
 * @brief 云播报屏应用主任务与事件分发
 *
 * Stage 1 职责：
 *   1. 创建事件队列与 1 s 周期定时器；
 *   2. 初始化 SSD1306 并绘制固定的三区画面；
 *   3. 每次 tick 更新状态栏，验证帧缓冲与刷新链路。
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-11-26
 * @version 1.1
 */

#include <stdio.h>
#include <string.h>

#include "liot_log.h"
#include "liot_os.h"

#include "screen_app.h"
#include "screen_config.h"
#include "screen_led.h"
#include "screen_oled.h"

/* 标题与内容区的演示文本，Stage 3 起由服务器下发内容替换 */
#define SCREEN_UI_TITLE_TEXT    "SCREEN DEMO"
#define SCREEN_UI_LINE1_TEXT    "PROJECT 10"
#define SCREEN_UI_LINE2_TEXT    "CLOUD SCREEN"

/**
 * @brief 应用运行上下文
 *
 * 所有运行期状态集中在此，文件内静态持有，避免跨模块全局变量。
 */
typedef struct
{
    liot_queue_t queue;         /*!< 事件队列句柄 */
    liot_timer_t timer;         /*!< 周期定时器句柄 */
    uint32_t     tick_count;    /*!< 已处理的 tick 数量，用于串口验证 */
    uint8_t      oled_ready;    /*!< OLED 是否可用，0=不可用 */
    uint8_t      render_failed; /*!< 绘制失败是否已告警，避免每秒刷日志 */
} screen_app_ctx_t;

static screen_app_ctx_t s_app_ctx;  /*!< 唯一的应用上下文实例 */

/**
 * @brief 周期定时器回调
 * @param[in] arg screen_app_ctx_t 指针，创建定时器时传入
 * @return 无
 * @note 运行在定时器任务上下文，只做入队，不打印、不访问外设
 */
static void screen_app_timer_cb(void *arg)
{
    screen_app_ctx_t *ctx = (screen_app_ctx_t *)arg;
    screen_event_t event;

    if (ctx == NULL)
    {
        return;
    }

    memset(&event, 0, sizeof(event));
    event.id = SCREEN_EV_TICK;
    event.tick_ms = SCREEN_TICK_PERIOD_MS;

    /* 队列满时直接丢弃本次 tick，不能让定时器回调阻塞 */
    (void)liot_rtos_queue_release(ctx->queue, (uint32)sizeof(event),
                                  (uint8 *)&event, 0);
}

/**
 * @brief 绘制三区静态画面并刷新
 * @return 无
 * @note 只在应用主任务中调用，避免多任务同时操作帧缓冲
 */
static void screen_app_render(void)
{
    char status[SCREEN_STATUS_TEXT_LEN];
    int  ret = 0;
    int  fmt_ret;

    if (s_app_ctx.oled_ready == 0U)
    {
        return;
    }

    if (screen_oled_clear() != 0)
    {
        return;
    }

    /* 标题栏：白底黑字 */
    ret |= screen_oled_fill_rect(0U, SCREEN_UI_TITLE_Y, SCREEN_OLED_WIDTH,
                                 SCREEN_UI_BAR_HEIGHT, SCREEN_OLED_ON);
    ret |= screen_oled_draw_text(0U, SCREEN_UI_TITLE_Y,
                                 SCREEN_UI_TITLE_TEXT, 1U);

    /* 内容区：两行 ASCII 文本 */
    ret |= screen_oled_draw_text(0U, SCREEN_UI_LINE1_Y,
                                 SCREEN_UI_LINE1_TEXT, 0U);
    ret |= screen_oled_draw_text(0U, SCREEN_UI_LINE2_Y,
                                 SCREEN_UI_LINE2_TEXT, 0U);

    /* 状态栏：白底黑字，显示启动后的 tick 计数 */
    ret |= screen_oled_fill_rect(0U, SCREEN_UI_STATUS_Y, SCREEN_OLED_WIDTH,
                                 SCREEN_UI_BAR_HEIGHT, SCREEN_OLED_ON);
    fmt_ret = snprintf(status, sizeof(status), "BOOT %04u",
                       (unsigned)(s_app_ctx.tick_count % 10000U));
    if ((fmt_ret < 0) || (fmt_ret >= (int)sizeof(status)))
    {
        ret = -1;
    }
    ret |= screen_oled_draw_text(0U, SCREEN_UI_STATUS_Y, status, 1U);

    if (ret != 0)
    {
        if (s_app_ctx.render_failed == 0U)
        {
            s_app_ctx.render_failed = 1U;
            liot_trace("[APP] oled text draw failed");
        }
    }
    else
    {
        s_app_ctx.render_failed = 0U;
    }

    if (screen_oled_flush() != 0)
    {
        liot_trace("[APP] oled flush failed");
    }
}

/**
 * @brief 初始化应用队列、周期定时器与 OLED
 * @return 0 成功，-1 队列/定时器失败
 * @note OLED 初始化失败不阻塞主循环，串口会打印错误，便于单独排查接线
 */
int screen_app_init(void)
{
    LiotOSStatus_t ret;

    memset(&s_app_ctx, 0, sizeof(s_app_ctx));

    ret = liot_rtos_queue_create(&s_app_ctx.queue,
                                 (uint32)sizeof(screen_event_t),
                                 SCREEN_EVENT_QUEUE_DEPTH);
    if (ret != LIOT_OSI_SUCCESS)
    {
        liot_trace("[APP] queue create failed: %d", (int)ret);
        return -1;
    }

    ret = liot_rtos_timer_create(&s_app_ctx.timer, LIOT_TimerPeriodic,
                                 screen_app_timer_cb, &s_app_ctx);
    if (ret != LIOT_OSI_SUCCESS)
    {
        liot_trace("[APP] timer create failed: %d", (int)ret);
        return -1;
    }

    ret = liot_rtos_timer_start(s_app_ctx.timer, SCREEN_TICK_PERIOD_MS);
    if (ret != LIOT_OSI_SUCCESS)
    {
        liot_trace("[APP] timer start failed: %d", (int)ret);
        return -1;
    }

    if (screen_oled_init() == 0)
    {
        s_app_ctx.oled_ready = 1U;
        screen_app_render();
    }
    else
    {
        liot_trace("[APP] oled init failed, display disabled");
    }

    liot_trace("[APP] init done, tick=%u ms", (unsigned)SCREEN_TICK_PERIOD_MS);
    return 0;
}

/**
 * @brief 向应用主任务投递事件
 * @param[in] event 待投递事件
 * @return 0 成功，-1 失败
 */
int screen_app_post(const screen_event_t *event)
{
    if ((event == NULL) || (s_app_ctx.queue == NULL))
    {
        return -1;
    }

    if (liot_rtos_queue_release(s_app_ctx.queue, (uint32)sizeof(*event),
                                (uint8 *)event, 0) != LIOT_OSI_SUCCESS)
    {
        return -1;
    }

    return 0;
}

/**
 * @brief 处理单个事件
 * @param[in] event 待处理事件
 * @return 无
 */
static void screen_app_handle_event(const screen_event_t *event)
{
    if (event == NULL)
    {
        return;
    }

    switch (event->id)
    {
        case SCREEN_EV_TICK:
            s_app_ctx.tick_count++;
            liot_trace("[APP] tick %u", (unsigned)s_app_ctx.tick_count);
            screen_led_tick(event->tick_ms);
            screen_app_render();
            break;

        default:
            /* 未知事件直接忽略，避免异常消息影响主循环 */
            break;
    }
}

/**
 * @brief 应用主循环
 * @return 无（正常情况下不返回）
 */
void screen_app_run_forever(void)
{
    screen_event_t event;

    while (1)
    {
        memset(&event, 0, sizeof(event));

        if (liot_rtos_queue_wait(s_app_ctx.queue, (uint8 *)&event,
                                 (uint32)sizeof(event),
                                 LIOT_WAIT_FOREVER) == LIOT_OSI_SUCCESS)
        {
            screen_app_handle_event(&event);
        }
    }
}
