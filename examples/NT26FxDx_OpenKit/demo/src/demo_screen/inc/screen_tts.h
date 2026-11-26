/**
 * @file screen_tts.h
 * @brief TTS 语音播报接口
 *
 * liot_tts_start() 是同步阻塞的，必须放在独立任务里，
 * 否则会卡住界面刷新与 MQTT 心跳。
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-11-26
 * @version 1.0
 */

#ifndef SCREEN_TTS_H
#define SCREEN_TTS_H

#include <stdint.h>

/**
 * @brief 播报结束回调
 * @param[in] ok 1 播放完成，0 失败或被打断
 */
typedef void (*screen_tts_done_cb_t)(uint8_t ok);

/**
 * @brief 启动 TTS 任务
 * @return 0 成功，-1 创建队列/任务失败
 */
int screen_tts_init(void);

/**
 * @brief 请求播报一段文本
 * @param[in] text 以 '\0' 结尾的 UTF-8 文本
 * @return 0 已入队，-1 参数错误或队列满
 */
int screen_tts_speak(const char *text);

/**
 * @brief 请求停止当前播报（由 TTS 任务实际执行停止）
 * @return 无
 */
void screen_tts_stop(void);

/**
 * @brief 查询是否正在播报
 * @return 1 正在播报，0 空闲
 */
uint8_t screen_tts_is_busy(void);

/**
 * @brief 注册播报结束回调
 * @param[in] cb 回调函数，运行在 TTS 任务上下文
 * @return 无
 */
void screen_tts_set_done_cb(screen_tts_done_cb_t cb);

#endif /* SCREEN_TTS_H */
