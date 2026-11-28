/**
 * @file screen_mqtt.c
 * @brief 云播报屏 MQTT 任务实现
 *
 * 任务流程：
 *   1. 读取 IMEI 并拼好 topic/URL；
 *   2. 等待网络注册并激活 PDP；
 *   3. 连接 broker、订阅下行主题、发布 dev.reg；
 *   4. 每 30 s 发送 0xAA 心跳，断线后自动重建连接。
 *
 * 回调（MQTT 内部任务）只释放信号量，所有发布动作都在本任务中完成。
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-11-26
 * @version 1.0
 */

#include <stdio.h>
#include <string.h>

#include "liot_datacall.h"
#include "liot_dev.h"
#include "liot_log.h"
#include "liot_mqtt_client.h"
#include "liot_nw.h"
#include "liot_os.h"
#include "liot_sim.h"

#include "screen_config.h"
#include "screen_mqtt.h"
#include "screen_proto.h"

/* MQTT 任务内部事件 */
typedef enum
{
    SCREEN_MQTT_EV_HB = 1,      /*!< 发送一次心跳 */
    SCREEN_MQTT_EV_ACK,         /*!< 发送一条回执 */
    SCREEN_MQTT_EV_MAX
} screen_mqtt_event_e;

typedef struct
{
    int     event;          /*!< screen_mqtt_event_e */
    uint8_t shown;          /*!< 回执 shown 字段 */
    char    msg_id[SCREEN_MSG_ID_MAX_LEN];      /*!< 回执 msgId */
    char    status[SCREEN_ACK_STATUS_MAX_LEN];  /*!< 回执状态 */
    char    err[SCREEN_ACK_ERR_MAX_LEN];        /*!< 失败原因 */
} screen_mqtt_msg_t;

typedef struct
{
    liot_mqtt_client_t client;  /*!< MQTT 客户端句柄 */
    liot_task_t        task;    /*!< 网络任务句柄 */
    liot_queue_t       queue;   /*!< 心跳事件队列 */
    liot_timer_t       hb_timer;/*!< 30 s 心跳定时器 */
    liot_sem_t         conn_sem;/*!< 连接结果信号量 */
    liot_sem_t         op_sem;  /*!< 发布/订阅完成信号量 */

    char imei[SCREEN_IMEI_LEN];         /*!< 设备 IMEI */
    char url[SCREEN_URL_MAX_LEN];       /*!< mqtt://host:port */
    char topic_sub[SCREEN_TOPIC_MAX_LEN];   /*!< screen/{imei}/msg */
    char topic_ack[SCREEN_TOPIC_MAX_LEN];   /*!< screen/{imei}/ack */
    char topic_reg[SCREEN_TOPIC_MAX_LEN];   /*!< device/{imei}/up */
    char topic_hb[SCREEN_TOPIC_MAX_LEN];    /*!< device/{imei}/upb */

    uint8_t  seq;               /*!< 上行序号 0-255 */
    uint16_t hb_sent;           /*!< 累计心跳发送次数 */
    uint16_t hb_ok;             /*!< 累计成功次数，Stage 3 起统计 */
    uint32_t start_tick;        /*!< 任务启动 tick，用于计算运行时长 */

    volatile uint8_t connected;             /*!< 1=已连接 */
    volatile screen_mqtt_state_e state;     /*!< 当前状态 */
    uint8_t sim_missing;                    /*!< 1=上一轮检测到无卡，用于抑制重复日志 */
    screen_mqtt_rx_handler_t rx_handler;    /*!< 下行报文回调，可为 NULL */
} screen_mqtt_ctx_t;

static screen_mqtt_ctx_t s_mqtt_ctx;

/**
 * @brief 清空信号量计数，避免上一轮残留
 * @param[in] sem 信号量句柄
 * @return 无
 */
static void screen_mqtt_sem_drain(liot_sem_t sem)
{
    if (sem == NULL)
    {
        return;
    }

    while (liot_rtos_semaphore_wait(sem, LIOT_NO_WAIT) == LIOT_OSI_SUCCESS)
    {
        /* 丢弃残留计数 */
    }
}

/**
 * @brief 等待信号量
 * @param[in] sem        信号量句柄
 * @param[in] timeout_ms 超时时间
 * @return 0 成功，-1 超时
 */
static int screen_mqtt_sem_wait(liot_sem_t sem, uint32_t timeout_ms)
{
    if (sem == NULL)
    {
        return -1;
    }

    return (liot_rtos_semaphore_wait(sem, timeout_ms) == LIOT_OSI_SUCCESS) ? 0 : -1;
}

/**
 * @brief MQTT 事件回调
 * @param[in] client 客户端句柄
 * @param[in] event  事件类型
 * @param[in] arg    用户参数
 * @param[in] data   事件数据
 * @return 无
 * @note 运行在 MQTT 内部任务，只更新状态和释放信号量
 */
static void screen_mqtt_event_cb(liot_mqtt_client_t *client, int event,
                                 void *arg, void *data)
{
    (void)client;
    (void)arg;

    switch (event)
    {
        case LIOT_MQTT_OPEN_EVENT:
        case LIOT_MQTT_CONNECT_EVENT:
        {
            int status = (data != NULL) ? *(int *)data : -1;

            if (status == 0)
            {
                s_mqtt_ctx.connected = 1U;
            }
            (void)liot_rtos_semaphore_release(s_mqtt_ctx.conn_sem);
            liot_trace("[MQTT] connect event, status=%d", status);
            break;
        }

        case LIOT_MQTT_CLOSE_EVENT:
        case LIOT_MQTT_DISCONNECT_EVENT:
            s_mqtt_ctx.connected = 0U;
            liot_trace("[MQTT] disconnected event");
            break;

        case LIOT_MQTT_PUB_EVENT:
        case LIOT_MQTT_SUB_EVENT:
        case LIOT_MQTT_UNSUB_EVENT:
            (void)liot_rtos_semaphore_release(s_mqtt_ctx.op_sem);
            break;

        case LIOT_MQTT_RECONNECT_EVENT:
            liot_trace("[MQTT] reconnect event");
            break;

        default:
            break;
    }
}

/**
 * @brief 下行消息回调
 * @param[in] client      客户端句柄
 * @param[in] arg         用户参数
 * @param[in] pkt_id      报文 ID
 * @param[in] topic       主题
 * @param[in] payload     载荷
 * @param[in] payload_len 载荷长度
 * @return 无
 * @note 运行在 MQTT 内部任务，只做主题过滤并把载荷转交给应用回调
 */
static void screen_mqtt_inpub_cb(liot_mqtt_client_t *client, void *arg,
                                 int pkt_id, const char *topic,
                                 const unsigned char *payload,
                                 unsigned short payload_len)
{
    (void)client;
    (void)arg;
    (void)pkt_id;

    /* 只接受本设备的下行主题，其他主题（如系统回包）直接忽略 */
    if ((topic != NULL) && (strcmp(topic, s_mqtt_ctx.topic_sub) != 0))
    {
        liot_trace("[MQTT] ignore topic=%s", topic);
        return;
    }

    if ((payload == NULL) || (payload_len == 0U))
    {
        liot_trace("[MQTT] empty payload");
        return;
    }

    if (s_mqtt_ctx.rx_handler != NULL)
    {
        s_mqtt_ctx.rx_handler((const uint8_t *)payload, (uint16_t)payload_len);
    }
    else
    {
        liot_trace("[MQTT] rx without handler, len=%u", (unsigned)payload_len);
    }
}

/**
 * @brief 发布一条消息并等待完成
 * @param[in] topic   主题
 * @param[in] payload 载荷
 * @param[in] len     载荷长度
 * @param[in] qos     QoS
 * @param[in] retain  保留标志
 * @return 0 成功，-1 失败
 */
static int screen_mqtt_publish(const char *topic, const void *payload,
                               uint16_t len, uint8_t qos, uint8_t retain)
{
    int ret;

    if ((topic == NULL) || (payload == NULL) || (len == 0U))
    {
        return -1;
    }

    if (s_mqtt_ctx.connected == 0U)
    {
        return -1;
    }

    screen_mqtt_sem_drain(s_mqtt_ctx.op_sem);

    ret = liot_mqtt_publish(&s_mqtt_ctx.client, topic, payload, len, qos,
                            retain, NULL, NULL);
    if (ret == LIOT_MQTTCLIENT_WOUNDBLOCK)
    {
        return screen_mqtt_sem_wait(s_mqtt_ctx.op_sem,
                                    SCREEN_MQTT_OP_TIMEOUT_MS);
    }

    return (ret == LIOT_MQTTCLIENT_SUCCESS) ? 0 : -1;
}

/**
 * @brief 订阅下行主题
 * @param[in] topic 主题
 * @return 0 成功，-1 失败
 */
static int screen_mqtt_subscribe(const char *topic)
{
    int ret;

    if (topic == NULL)
    {
        return -1;
    }

    screen_mqtt_sem_drain(s_mqtt_ctx.op_sem);

    ret = liot_mqtt_sub_unsub(&s_mqtt_ctx.client, topic, SCREEN_MQTT_QOS,
                              NULL, NULL, 1U);
    if (ret == LIOT_MQTTCLIENT_WOUNDBLOCK)
    {
        return screen_mqtt_sem_wait(s_mqtt_ctx.op_sem,
                                    SCREEN_MQTT_OP_TIMEOUT_MS);
    }

    return (ret == LIOT_MQTTCLIENT_SUCCESS) ? 0 : -1;
}

/**
 * @brief 发布 dev.reg
 * @return 0 成功，-1 失败
 */
static int screen_mqtt_send_dev_reg(void)
{
    char json[SCREEN_JSON_MAX_LEN];
    int  len;

    len = screen_proto_build_dev_reg(s_mqtt_ctx.imei, SCREEN_STUDENT_NO,
                                     s_mqtt_ctx.seq, json, sizeof(json));
    if (len <= 0)
    {
        liot_trace("[MQTT] build dev.reg failed");
        return -1;
    }

    if (screen_mqtt_publish(s_mqtt_ctx.topic_reg, json, (uint16_t)len,
                            SCREEN_MQTT_QOS, 0U) != 0)
    {
        liot_trace("[MQTT] publish dev.reg failed");
        return -1;
    }

    s_mqtt_ctx.seq++;
    liot_trace("[MQTT] dev.reg sent, imei=%s stuNo=%s", s_mqtt_ctx.imei,
               SCREEN_STUDENT_NO);
    return 0;
}

/**
 * @brief 取当前信号强度并换算成 dBm
 * @return 有符号 dBm；读取失败返回 0
 *
 * liot_nw_get_signal_strength() 返回的是原始索引，不是 dBm：
 *   rssi 原始值 0-31，dBm = 2 * rssi - 113（99 无效）
 *   rsrp 原始值 0-97，dBm = rsrp - 141（255 无效）
 * 协议要求心跳里填有符号 dBm，所以必须先换算。
 */
static int8_t screen_mqtt_get_rssi_dbm(void)
{
    liot_nw_signal_strength_info_s signal_info;
    int8_t rssi_dbm;

    memset(&signal_info, 0, sizeof(signal_info));
    if (liot_nw_get_signal_strength(SCREEN_SIM_ID, &signal_info) != LIOT_NW_SUCCESS)
    {
        return 0;
    }

    if ((signal_info.rssi >= 0) && (signal_info.rssi <= SCREEN_CSQ_VALID_MAX))
    {
        rssi_dbm = (int8_t)SCREEN_CSQ_TO_DBM(signal_info.rssi);
    }
    else if ((signal_info.rsrp >= 0) && (signal_info.rsrp <= SCREEN_RSRP_VALID_MAX))
    {
        rssi_dbm = (int8_t)(signal_info.rsrp - SCREEN_RSRP_OFFSET_DBM);
    }
    else
    {
        rssi_dbm = 0;   /* 两路都无效时填 0，避免出现虚假的强信号 */
    }

    return rssi_dbm;
}

/**
 * @brief 发送一帧 0xAA 心跳
 * @return 无
 */
static void screen_mqtt_send_heartbeat(void)
{
    uint8_t  frame[SCREEN_HB_FRAME_LEN];
    int8_t   rssi;
    uint32_t uptime_s;
    int      len;

    rssi = screen_mqtt_get_rssi_dbm();

    uptime_s = (liot_rtos_get_system_tick() - s_mqtt_ctx.start_tick) / 1000U;

    len = screen_proto_build_heartbeat(s_mqtt_ctx.imei, s_mqtt_ctx.seq, uptime_s,
                                       (uint16_t)(s_mqtt_ctx.hb_sent + 1U),
                                       s_mqtt_ctx.hb_ok, rssi, frame,
                                       sizeof(frame));
    if (len <= 0)
    {
        liot_trace("[MQTT] build heartbeat failed");
        return;
    }

    if (screen_mqtt_publish(s_mqtt_ctx.topic_hb, frame, (uint16_t)len,
                            SCREEN_MQTT_QOS, 0U) != 0)
    {
        liot_trace("[MQTT] publish heartbeat failed");
        return;
    }

    s_mqtt_ctx.hb_sent++;
    s_mqtt_ctx.seq++;
    s_mqtt_ctx.hb_ok++;     /* Stage 2 以发布完成为准，Stage 3 起按 sys.resp 统计 */
    liot_trace("[MQTT] heartbeat #%u uptime=%us rssi=%d",
               (unsigned)s_mqtt_ctx.hb_sent, (unsigned)uptime_s, (int)rssi);
}

/**
 * @brief 发布一条 screen.ack
 * @param[in] req 回执请求
 * @return 无
 * @note 只在 MQTT 任务中调用，保证发布串行
 */
static void screen_mqtt_publish_ack(const screen_mqtt_msg_t *req)
{
    char        json[SCREEN_JSON_MAX_LEN];
    const char *err;
    int         len;

    if (req == NULL)
    {
        return;
    }

    err = (req->err[0] != '\0') ? req->err : NULL;

    len = screen_proto_build_ack(s_mqtt_ctx.imei, req->msg_id, req->status,
                                 req->shown, err, s_mqtt_ctx.seq, json,
                                 sizeof(json));
    if (len <= 0)
    {
        liot_trace("[MQTT] build ack failed: %s", req->status);
        return;
    }

    if (screen_mqtt_publish(s_mqtt_ctx.topic_ack, json, (uint16_t)len,
                            SCREEN_MQTT_QOS, 0U) != 0)
    {
        liot_trace("[MQTT] publish ack failed: %s", req->status);
        return;
    }

    s_mqtt_ctx.seq++;
    liot_trace("[MQTT] ack sent msgId=%s status=%s", req->msg_id, req->status);
}

/**
 * @brief 心跳定时器回调
 * @param[in] arg 未使用
 * @return 无
 * @note 只投递事件，真正发送在 MQTT 任务中完成
 */
static void screen_mqtt_hb_timer_cb(void *arg)
{
    screen_mqtt_msg_t msg;

    (void)arg;

    memset(&msg, 0, sizeof(msg));
    msg.event = SCREEN_MQTT_EV_HB;
    (void)liot_rtos_queue_release(s_mqtt_ctx.queue, (uint32)sizeof(msg),
                                  (uint8 *)&msg, 0);
}

/**
 * @brief 等待网络注册并激活 PDP
 * @return 0 成功，-1 失败
 */
static int screen_mqtt_datacall(void)
{
    liot_datacall_errcode_e ret;
    liot_sim_status_e       sim_status = LIOT_SIM_STATUS_UNKNOW;
    int times = 0;

    /* 拔卡后不要干等注网超时：先看卡状态，没有卡就立刻返回，
     * 让状态栏回到 RTRY 并在下一轮重新尝试，插回卡后能尽快恢复。 */
    if (liot_sim_get_card_status(SCREEN_SIM_ID, &sim_status) == LIOT_SIM_SUCCESS)
    {
        if ((sim_status == LIOT_SIM_STATUS_NOSIM) ||
            (sim_status == LIOT_SIM_STATUS_UNKNOW))
        {
            /* 只在状态变化时打印，避免拔卡期间每轮刷屏 */
            if (s_mqtt_ctx.sim_missing == 0U)
            {
                liot_trace("[MQTT] sim unavailable, status=%d", (int)sim_status);
                s_mqtt_ctx.sim_missing = 1U;
            }
            return -1;
        }

        s_mqtt_ctx.sim_missing = 0U;
    }

    while ((ret = liot_network_register_wait(SCREEN_SIM_ID,
                                             SCREEN_NET_REG_TIMEOUT_S)) != LIOT_DATACALL_SUCCESS)
    {
        times++;
        if (times >= SCREEN_NET_REG_RETRY_MAX)
        {
            liot_trace("[MQTT] network register timeout");
            return -1;
        }
        liot_rtos_task_sleep_ms(SCREEN_NET_RETRY_DELAY_MS);
    }

    /* 关掉异步激活，才能拿到真实的 PDP 激活结果 */
    (void)liot_set_data_call_asyn_mode(SCREEN_SIM_ID, SCREEN_DATA_CALL_CID, 0);

    ret = liot_start_data_call(SCREEN_SIM_ID, SCREEN_DATA_CALL_CID,
                               LIOT_DATA_TYPE_IP, "", "", "",
                               LIOT_DATA_AUTH_TYPE_NONE);
    if ((ret != LIOT_DATACALL_SUCCESS) && (ret != LIOT_DATACALL_REPEAT_ACTIVE_ERR))
    {
        /* 失败时清掉残留 PDP，否则下一轮会一直误报“已激活” */
        (void)liot_stop_data_call(SCREEN_SIM_ID, SCREEN_DATA_CALL_CID);
        liot_trace("[MQTT] data call failed: 0x%x", (unsigned)ret);
        return -1;
    }

    /* 等待 PDP 地址分配完成 */
    liot_rtos_task_sleep_ms(SCREEN_MQTT_DATACALL_DELAY_MS);
    liot_trace("[MQTT] data call ready");
    return 0;
}

/**
 * @brief 读取设备信息并拼接 topic/URL
 * @return 0 成功，-1 失败
 */
static int screen_mqtt_prepare_device(void)
{
    int fmt_ret;

    if (liot_dev_get_imei(s_mqtt_ctx.imei, sizeof(s_mqtt_ctx.imei),
                          SCREEN_SIM_ID) != LIOT_DEV_SUCCESS)
    {
        liot_trace("[MQTT] get imei failed");
        return -1;
    }

    fmt_ret = snprintf(s_mqtt_ctx.url, sizeof(s_mqtt_ctx.url), "mqtt://%s:%d",
                       SCREEN_SERVER_HOST, SCREEN_SERVER_PORT);
    if ((fmt_ret < 0) || (fmt_ret >= (int)sizeof(s_mqtt_ctx.url)))
    {
        liot_trace("[MQTT] build url failed");
        return -1;
    }

    fmt_ret = snprintf(s_mqtt_ctx.topic_sub, sizeof(s_mqtt_ctx.topic_sub),
                       SCREEN_TOPIC_SUB_FMT, s_mqtt_ctx.imei);
    if ((fmt_ret < 0) || (fmt_ret >= (int)sizeof(s_mqtt_ctx.topic_sub)))
    {
        return -1;
    }

    fmt_ret = snprintf(s_mqtt_ctx.topic_ack, sizeof(s_mqtt_ctx.topic_ack),
                       SCREEN_TOPIC_ACK_FMT, s_mqtt_ctx.imei);
    if ((fmt_ret < 0) || (fmt_ret >= (int)sizeof(s_mqtt_ctx.topic_ack)))
    {
        return -1;
    }

    fmt_ret = snprintf(s_mqtt_ctx.topic_reg, sizeof(s_mqtt_ctx.topic_reg),
                       SCREEN_TOPIC_REG_FMT, s_mqtt_ctx.imei);
    if ((fmt_ret < 0) || (fmt_ret >= (int)sizeof(s_mqtt_ctx.topic_reg)))
    {
        return -1;
    }

    fmt_ret = snprintf(s_mqtt_ctx.topic_hb, sizeof(s_mqtt_ctx.topic_hb),
                       SCREEN_TOPIC_HB_FMT, s_mqtt_ctx.imei);
    if ((fmt_ret < 0) || (fmt_ret >= (int)sizeof(s_mqtt_ctx.topic_hb)))
    {
        return -1;
    }

    liot_trace("[MQTT] imei=%s sub=%s", s_mqtt_ctx.imei, s_mqtt_ctx.topic_sub);
    return 0;
}

/**
 * @brief 建立一次完整连接：连接、订阅、注册
 * @return 0 成功，-1 失败
 */
static int screen_mqtt_connect_once(void)
{
    liot_mqtt_client_option options;
    liot_mqtt_client_t     *client = &s_mqtt_ctx.client;
    int ret;

    memset(&options, 0, sizeof(options));
    options.version       = LIOT_MQTT_VERSION_4;
    options.pdp_cid       = SCREEN_DATA_CALL_CID;
    options.client_id     = (char *)SCREEN_STUDENT_NO;
    options.client_user   = (char *)SCREEN_MQTT_USER;
    options.client_pass   = (char *)SCREEN_MQTT_PASS;
    options.clean_session = 1U;
    options.kalive_time   = SCREEN_MQTT_KEEPALIVE_S;
    options.delivery_time = SCREEN_MQTT_DELIVERY_TIME_S;
    options.delivery_cnt  = SCREEN_MQTT_DELIVERY_CNT;
    options.ping_timeout  = SCREEN_MQTT_PING_TIMEOUT_S;

    s_mqtt_ctx.connected = 0U;
    screen_mqtt_sem_drain(s_mqtt_ctx.conn_sem);

    if (liot_mqtt_client_init_ex(client, SCREEN_DATA_CALL_CID,
                                 screen_mqtt_event_cb, NULL) != LIOT_MQTTCLIENT_SUCCESS)
    {
        liot_trace("[MQTT] client init failed");
        return -1;
    }

    ret = liot_mqtt_connect(client, s_mqtt_ctx.url, NULL, NULL, &options, NULL);
    if (ret == LIOT_MQTTCLIENT_WOUNDBLOCK)
    {
        if (screen_mqtt_sem_wait(s_mqtt_ctx.conn_sem,
                                 SCREEN_MQTT_CONNECT_TIMEOUT_MS) != 0)
        {
            liot_trace("[MQTT] connect timeout");
            return -1;
        }
    }
    else if (ret == LIOT_MQTTCLIENT_SUCCESS)
    {
        /* 少数情况下底层同步返回成功，事件回调可能不再上报 */
        s_mqtt_ctx.connected = 1U;
    }
    else
    {
        liot_trace("[MQTT] connect failed: %d", ret);
        return -1;
    }

    if ((s_mqtt_ctx.connected == 0U) ||
        (liot_mqtt_client_state(client) != MQTT_CONN_CONNECTED))
    {
        liot_trace("[MQTT] connect not confirmed");
        return -1;
    }

    if (liot_mqtt_set_inpub_callback(client, screen_mqtt_inpub_cb, NULL) !=
        LIOT_MQTTCLIENT_SUCCESS)
    {
        liot_trace("[MQTT] set inpub callback failed");
        return -1;
    }

    if (screen_mqtt_subscribe(s_mqtt_ctx.topic_sub) != 0)
    {
        liot_trace("[MQTT] subscribe failed");
        return -1;
    }

    if (screen_mqtt_send_dev_reg() != 0)
    {
        return -1;
    }

    return 0;
}

/**
 * @brief MQTT 任务主体
 * @param[in] arg 未使用
 * @return 无（正常情况下不返回）
 */
static void screen_mqtt_task(void *arg)
{
    uint32_t last_recovery_tick;

    (void)arg;

    if (screen_mqtt_prepare_device() != 0)
    {
        s_mqtt_ctx.state = SCREEN_MQTT_STATE_ERROR;
        liot_trace("[MQTT] device prepare failed, task halt");
        while (1)
        {
            liot_rtos_task_sleep_s(10);
        }
    }

    s_mqtt_ctx.start_tick = liot_rtos_get_system_tick();
    last_recovery_tick = s_mqtt_ctx.start_tick;

    while (1)
    {
        s_mqtt_ctx.state = SCREEN_MQTT_STATE_NET_WAIT;
        if (screen_mqtt_datacall() != 0)
        {
            s_mqtt_ctx.state = SCREEN_MQTT_STATE_RETRY;

            /* 拔卡或长时间注不上网时，模组可能不会重新识别 SIM；
             * 定期把射频从最小功能切回全功能，强制重新初始化。 */
            if ((liot_rtos_get_system_tick() - last_recovery_tick) >=
                SCREEN_NET_RECOVERY_PERIOD_MS)
            {
                last_recovery_tick = liot_rtos_get_system_tick();
                liot_trace("[MQTT] modem recovery: restart radio");

                (void)liot_dev_set_modem_fun(LIOT_DEV_CFUN_MIN, 0U, SCREEN_SIM_ID);
                liot_rtos_task_sleep_s(2);
                (void)liot_dev_set_modem_fun(LIOT_DEV_CFUN_FULL, 0U, SCREEN_SIM_ID);
                liot_rtos_task_sleep_s(2);
            }

            liot_rtos_task_sleep_ms(SCREEN_MQTT_RETRY_DELAY_MS);
            continue;
        }

        s_mqtt_ctx.state = SCREEN_MQTT_STATE_CONNECTING;
        if (screen_mqtt_connect_once() != 0)
        {
            (void)liot_mqtt_client_deinit(&s_mqtt_ctx.client);
            s_mqtt_ctx.client = 0;
            s_mqtt_ctx.state = SCREEN_MQTT_STATE_RETRY;
            liot_rtos_task_sleep_ms(SCREEN_MQTT_RETRY_DELAY_MS);
            continue;
        }

        s_mqtt_ctx.state = SCREEN_MQTT_STATE_ONLINE;
        liot_trace("[MQTT] online");
        (void)liot_rtos_timer_start(s_mqtt_ctx.hb_timer,
                                    SCREEN_HEARTBEAT_PERIOD_MS);

        while (s_mqtt_ctx.connected != 0U)
        {
            screen_mqtt_msg_t msg;

            memset(&msg, 0, sizeof(msg));
            if (liot_rtos_queue_wait(s_mqtt_ctx.queue, (uint8 *)&msg,
                                     (uint32)sizeof(msg),
                                     SCREEN_MQTT_POLL_MS) == LIOT_OSI_SUCCESS)
            {
                if (msg.event == SCREEN_MQTT_EV_HB)
                {
                    screen_mqtt_send_heartbeat();
                }
                else if (msg.event == SCREEN_MQTT_EV_ACK)
                {
                    screen_mqtt_publish_ack(&msg);
                }
            }

            /* 轮询客户端状态，兜底捕捉底层断链 */
            if (liot_mqtt_client_state(&s_mqtt_ctx.client) != MQTT_CONN_CONNECTED)
            {
                s_mqtt_ctx.connected = 0U;
            }
        }

        (void)liot_rtos_timer_stop(s_mqtt_ctx.hb_timer);
        (void)liot_mqtt_client_deinit(&s_mqtt_ctx.client);
        s_mqtt_ctx.client = 0;

        /* 断链可能来自拔卡或掉网，残留的 PDP 会让下一轮误判“已激活”，
         * 这里主动释放，重连时由 datacall 重新激活。 */
        (void)liot_stop_data_call(SCREEN_SIM_ID, SCREEN_DATA_CALL_CID);

        s_mqtt_ctx.state = SCREEN_MQTT_STATE_RETRY;
        liot_trace("[MQTT] link lost, data call released, retry");
        liot_rtos_task_sleep_ms(SCREEN_MQTT_RETRY_DELAY_MS);
    }
}

/**
 * @brief 启动 MQTT 任务
 * @return 0 成功，-1 失败
 */
int screen_mqtt_init(void)
{
    LiotOSStatus_t ret;

    memset(&s_mqtt_ctx, 0, sizeof(s_mqtt_ctx));
    s_mqtt_ctx.state = SCREEN_MQTT_STATE_OFF;
    s_mqtt_ctx.client = 0;

    ret = liot_rtos_queue_create(&s_mqtt_ctx.queue, (uint32)sizeof(screen_mqtt_msg_t),
                                 SCREEN_MQTT_QUEUE_DEPTH);
    if (ret != LIOT_OSI_SUCCESS)
    {
        liot_trace("[MQTT] queue create failed: %d", (int)ret);
        return -1;
    }

    ret = liot_rtos_semaphore_create(&s_mqtt_ctx.conn_sem, 0U);
    if (ret != LIOT_OSI_SUCCESS)
    {
        liot_trace("[MQTT] conn sem create failed: %d", (int)ret);
        return -1;
    }

    ret = liot_rtos_semaphore_create(&s_mqtt_ctx.op_sem, 0U);
    if (ret != LIOT_OSI_SUCCESS)
    {
        liot_trace("[MQTT] op sem create failed: %d", (int)ret);
        return -1;
    }

    ret = liot_rtos_timer_create(&s_mqtt_ctx.hb_timer, LIOT_TimerPeriodic,
                                 screen_mqtt_hb_timer_cb, NULL);
    if (ret != LIOT_OSI_SUCCESS)
    {
        liot_trace("[MQTT] hb timer create failed: %d", (int)ret);
        return -1;
    }

    ret = liot_rtos_task_create(&s_mqtt_ctx.task, SCREEN_MQTT_TASK_STACK,
                                LIOT_APP_TASK_PRIORITY,
                                (char *)SCREEN_MQTT_TASK_NAME,
                                screen_mqtt_task, NULL);
    if (ret != LIOT_OSI_SUCCESS)
    {
        liot_trace("[MQTT] task create failed: %d", (int)ret);
        return -1;
    }

    liot_trace("[MQTT] task started");
    return 0;
}

/**
 * @brief 读取当前状态
 * @return 当前状态
 */
screen_mqtt_state_e screen_mqtt_get_state(void)
{
    return s_mqtt_ctx.state;
}

/**
 * @brief 取状态短文本
 * @return 静态字符串
 */
const char *screen_mqtt_state_text(void)
{
    switch (s_mqtt_ctx.state)
    {
        case SCREEN_MQTT_STATE_NET_WAIT:
            return "WAIT";

        case SCREEN_MQTT_STATE_CONNECTING:
            return "CONN";

        case SCREEN_MQTT_STATE_ONLINE:
            return "ONLN";

        case SCREEN_MQTT_STATE_RETRY:
            return "RTRY";

        case SCREEN_MQTT_STATE_ERROR:
            return "ERROR";

        case SCREEN_MQTT_STATE_OFF:
        default:
            return "OFF";
    }
}

/**
 * @brief 注册下行报文回调
 * @param[in] handler 回调函数
 * @return 无
 */
void screen_mqtt_set_rx_handler(screen_mqtt_rx_handler_t handler)
{
    s_mqtt_ctx.rx_handler = handler;
}

/**
 * @brief 请求发送一条 screen.ack
 * @param[in] msg_id 原样回带的消息 ID
 * @param[in] status 回执状态
 * @param[in] shown  内容是否已显示
 * @param[in] err    失败原因，可为 NULL
 * @return 0 已入队，-1 失败
 */
int screen_mqtt_send_ack(const char *msg_id, const char *status,
                         uint8_t shown, const char *err)
{
    screen_mqtt_msg_t msg;

    if ((msg_id == NULL) || (status == NULL) || (s_mqtt_ctx.queue == NULL))
    {
        return -1;
    }

    memset(&msg, 0, sizeof(msg));
    msg.event = SCREEN_MQTT_EV_ACK;
    msg.shown = shown;
    (void)strncpy(msg.msg_id, msg_id, sizeof(msg.msg_id) - 1U);
    (void)strncpy(msg.status, status, sizeof(msg.status) - 1U);
    if (err != NULL)
    {
        (void)strncpy(msg.err, err, sizeof(msg.err) - 1U);
    }

    if (liot_rtos_queue_release(s_mqtt_ctx.queue, (uint32)sizeof(msg),
                                (uint8 *)&msg, 0) != LIOT_OSI_SUCCESS)
    {
        liot_trace("[MQTT] ack queue full");
        return -1;
    }

    return 0;
}
