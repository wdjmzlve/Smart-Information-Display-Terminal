/**
 * @file screen_mqtt.h
 * @brief 云播报屏 MQTT 连接、注册与心跳
 *
 * MQTT 客户端运行在独立任务中，应用主任务只读取状态，
 * 避免拨号与重连阻塞界面刷新。
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-11-26
 * @version 1.0
 */

#ifndef SCREEN_MQTT_H
#define SCREEN_MQTT_H

#include <stdint.h>

/**
 * @brief 下行报文回调
 *
 * 运行在 MQTT 内部任务，回调里只能拷贝数据，不能解析或刷屏。
 */
typedef void (*screen_mqtt_rx_handler_t)(const uint8_t *payload, uint16_t len);

/**
 * @brief 网络/MQTT 状态
 */
typedef enum
{
    SCREEN_MQTT_STATE_OFF = 0,      /*!< 模块未启动 */
    SCREEN_MQTT_STATE_NET_WAIT,     /*!< 等待网络注册与拨号 */
    SCREEN_MQTT_STATE_CONNECTING,   /*!< 正在连接 broker */
    SCREEN_MQTT_STATE_ONLINE,       /*!< 已连接并完成注册 */
    SCREEN_MQTT_STATE_RETRY,        /*!< 连接失败，等待重试 */
    SCREEN_MQTT_STATE_ERROR,        /*!< 初始化失败，无法继续 */
    SCREEN_MQTT_STATE_MAX           /*!< 枚举上界 */
} screen_mqtt_state_e;

/**
 * @brief 启动 MQTT 任务
 * @return 0 成功，-1 创建队列/信号量/任务失败
 */
int screen_mqtt_init(void);

/**
 * @brief 读取当前状态，供界面状态栏使用
 * @return 当前状态
 */
screen_mqtt_state_e screen_mqtt_get_state(void);

/**
 * @brief 取当前状态的短文本（4 个字符，便于挤进状态栏）
 * @return 静态字符串指针
 */
const char *screen_mqtt_state_text(void);

/**
 * @brief 注册下行报文回调
 * @param[in] handler 回调函数；传 NULL 表示只打印日志
 * @return 无
 */
void screen_mqtt_set_rx_handler(screen_mqtt_rx_handler_t handler);

/**
 * @brief 请求发送一条 screen.ack 回执
 * @param[in] msg_id 原样回带的消息 ID
 * @param[in] status displayed / done / restored / failed
 * @param[in] shown  内容是否已显示，非 0 为 true
 * @param[in] err    失败原因，可为 NULL
 * @return 0 已入队，-1 参数错误或队列满
 * @note 只入队，实际发布在 MQTT 任务中进行，避免跨任务并发操作客户端
 */
int screen_mqtt_send_ack(const char *msg_id, const char *status,
                         uint8_t shown, const char *err);

#endif /* SCREEN_MQTT_H */
