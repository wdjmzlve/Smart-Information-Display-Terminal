/**
 * @file screen_store.h
 * @brief 最后一条消息的掉电存储
 *
 * 使用 SDK 的客户 NVM 区（≤1 KB），保存后重启可恢复显示内容。
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-11-26
 * @version 1.0
 */

#ifndef SCREEN_STORE_H
#define SCREEN_STORE_H

#include "screen_proto.h"

/**
 * @brief 保存最后一条消息
 * @param[in] msg 待保存消息
 * @return 0 成功，-1 失败
 */
int screen_store_save(const screen_msg_t *msg);

/**
 * @brief 读取最后一条消息
 * @param[out] msg 输出消息
 * @return 0 成功，-1 无记录或数据损坏
 */
int screen_store_load(screen_msg_t *msg);

#endif /* SCREEN_STORE_H */
