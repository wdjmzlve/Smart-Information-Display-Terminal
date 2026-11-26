/**
 * @file screen_app.c
 * @brief 云播报屏应用主任务与事件分发
 *
 * Stage 3 职责：
 *   1. 接收 MQTT 下行报文（拷贝后入队）；
 *   2. 解析 screen.msg，非法报文丢弃并告警；
 *   3. 按 msgId 去重，保存最近一条有效消息供后续显示使用。
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-11-26
 * @version 1.3
 */

#include <stdio.h>
#include <string.h>

#include "liot_log.h"
#include "liot_os.h"

#include "screen_app.h"
#include "screen_config.h"
#include "screen_led.h"
#include "screen_mqtt.h"
#include "screen_oled.h"
#include "screen_proto.h"

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
    uint32_t     tick_count;    /*!< 已处理的 tick 数量 */
    uint8_t      oled_ready;    /*!< OLED 是否可用，0=不可用 */
    uint8_t      render_failed; /*!< 绘制失败是否已告警，避免每秒刷日志 */
    uint8_t      has_msg;       /*!< 是否已收到有效消息 */
    screen_msg_t last_msg;      /*!< 最近一条有效消息，Stage 4 用于渲染 */
} screen_app_ctx_t;

static screen_app_ctx_t s_app_ctx;  /*!< 唯一的应用上下文实例 */

/* 解析用临时缓冲区：只在应用主任务中访问，避免占栈 */
static screen_msg_t s_rx_msg;

/* MQTT 下行回调桥接，定义在文件后部 */
static void screen_app_rx_bridge(const uint8_t *data, uint16_t len);

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
    fmt_ret = snprintf(status, sizeof(status), "%04u %s",
                       (unsigned)(s_app_ctx.tick_count % 10000U),
                       screen_mqtt_state_text());
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

    /* 网络在独立任务中启动，不阻塞界面刷新 */
    if (screen_mqtt_init() != 0)
    {
        liot_trace("[APP] mqtt init failed, network disabled");
    }
    else
    {
        /* 下行报文统一交给应用主任务解析，避免在 MQTT 回调里做重活 */
        screen_mqtt_set_rx_handler(screen_app_rx_bridge);
    }

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
 * @brief 把 MQTT 下行报文投递给应用主任务
 * @param[in] data 载荷指针
 * @param[in] len  载荷长度
 * @return 0 成功，-1 失败
 * @note 运行在 MQTT 回调上下文：先 malloc 拷贝，主任务处理完负责释放
 */
int screen_app_post_downlink(const uint8_t *data, uint16_t len)
{
    screen_event_t event;
    uint8_t       *copy;

    if ((data == NULL) || (len == 0U) || (s_app_ctx.queue == NULL))
    {
        return -1;
    }

    if (len > SCREEN_DOWNLINK_MAX_LEN)
    {
        liot_trace("[APP] downlink too long: %u", (unsigned)len);
        return -1;
    }

    copy = (uint8_t *)liot_rtos_malloc(len);
    if (copy == NULL)
    {
        liot_trace("[APP] downlink malloc failed");
        return -1;
    }

    memcpy(copy, data, len);

    memset(&event, 0, sizeof(event));
    event.id = SCREEN_EV_DOWNLINK;
    event.data_len = len;
    event.data = copy;

    if (liot_rtos_queue_release(s_app_ctx.queue, (uint32)sizeof(event),
                                (uint8 *)&event, 0) != LIOT_OSI_SUCCESS)
    {
        liot_rtos_free(copy);
        liot_trace("[APP] downlink queue full");
        return -1;
    }

    return 0;
}

/**
 * @brief MQTT 下行回调桥接
 * @param[in] data 载荷指针
 * @param[in] len  载荷长度
 * @return 无
 * @note MQTT 层只关心“收到数据”，入队失败在这里统一告警
 */
static void screen_app_rx_bridge(const uint8_t *data, uint16_t len)
{
    if (screen_app_post_downlink(data, len) != 0)
    {
        liot_trace("[APP] post downlink failed");
    }
}

/**
 * @brief 处理一条下行报文：解析、告警、去重
 * @param[in] event 下行事件
 * @return 无
 */
static void screen_app_handle_downlink(const screen_event_t *event)
{
    screen_proto_parse_result_e result;

    if ((event == NULL) || (event->data == NULL) || (event->data_len == 0U))
    {
        return;
    }

    result = screen_proto_parse_screen_msg(event->data, event->data_len,
                                           &s_rx_msg);
    switch (result)
    {
        case SCREEN_PROTO_PARSE_OK:
            break;

        case SCREEN_PROTO_PARSE_NOT_SCREEN_MSG:
            liot_trace("[APP] not screen.msg, ignored");
            return;

        case SCREEN_PROTO_PARSE_INVALID_JSON:
            liot_trace("[APP] invalid json, dropped");
            return;

        case SCREEN_PROTO_PARSE_MISSING_FIELD:
            liot_trace("[APP] missing field, dropped");
            return;

        case SCREEN_PROTO_PARSE_BAD_VALUE:
            liot_trace("[APP] bad field value, dropped");
            return;

        default:
            return;
    }

    /* 同一 msgId 重复下发：不刷新、不重播，直接忽略 */
    if ((s_app_ctx.has_msg != 0U) &&
        (strcmp(s_app_ctx.last_msg.msg_id, s_rx_msg.msg_id) == 0))
    {
        liot_trace("[APP] duplicate msgId=%s, ignored", s_rx_msg.msg_id);
        return;
    }

    s_app_ctx.last_msg = s_rx_msg;
    s_app_ctx.has_msg = 1U;

    liot_trace("[APP] rx msgId=%s level=%d tts=%d",
               s_rx_msg.msg_id, s_rx_msg.level, s_rx_msg.tts);
    liot_trace("[APP] title=%s", s_rx_msg.title);
    liot_trace("[APP] content=%s", s_rx_msg.content);

    /* Stage 4 在这里把 s_app_ctx.last_msg 渲染到内容区 */
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
            /* 只更新状态栏，不打印日志，避免每秒刷屏淹没网络输出 */
            s_app_ctx.tick_count++;
            screen_led_tick(event->tick_ms);
            screen_app_render();
            break;

        case SCREEN_EV_DOWNLINK:
            screen_app_handle_downlink(event);
            if (event->data != NULL)
            {
                liot_rtos_free(event->data);    /* 生产者申请，消费者释放 */
            }
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
