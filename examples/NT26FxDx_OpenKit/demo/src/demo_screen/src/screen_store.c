/**
 * @file screen_store.c
 * @brief 最后一条消息的掉电存储
 *
 * 使用 SDK 的 NVRAM 保留分区（2 KB，用户不可擦除），而不是内部文件系统：
 * 文件系统分区会随 flash 布局（CODE_REGION_ADJUST、TTS 资源）变化而移动，
 * 分区里的旧数据在新布局下不可用，会导致掉电恢复失效。
 *
 * 存储结构带 magic 与版本号，读取时校验，避免旧数据或损坏数据被当成有效消息。
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-11-28
 * @version 2.0
 */

#include <stdbool.h>
#include <string.h>

#include "liot_log.h"
#include "liot_nv.h"

#include "screen_config.h"
#include "screen_proto.h"
#include "screen_store.h"

#define SCREEN_STORE_MAGIC      (0x5343524EU)   /*!< 'SCRN'，用于识别本模块的数据 */
#define SCREEN_STORE_VERSION    (1U)            /*!< 结构版本，字段变化时递增 */
#define SCREEN_STORE_OFFSET     (0U)            /*!< 在 NVRAM 保留分区中的偏移 */

/**
 * @brief 掉电保存的数据块
 */
typedef struct
{
    uint32_t     magic;     /*!< 固定魔数 */
    uint16_t     version;   /*!< 结构版本 */
    uint16_t     reserved;  /*!< 对齐保留 */
    screen_msg_t msg;       /*!< 最后一条消息 */
} screen_store_blob_t;

/**
 * @brief 保存最后一条消息
 * @param[in] msg 待保存消息
 * @return 0 成功，-1 失败
 * @note NVRAM 写入寿命有限，只在收到新消息时写一次
 */
int screen_store_save(const screen_msg_t *msg)
{
    screen_store_blob_t blob;

    if (msg == NULL)
    {
        return -1;
    }

    memset(&blob, 0, sizeof(blob));
    blob.magic = SCREEN_STORE_MAGIC;
    blob.version = SCREEN_STORE_VERSION;
    blob.msg = *msg;

    if (Liot_nvmWrite((UINT8 *)&blob, (UINT32)sizeof(blob), SCREEN_STORE_OFFSET) != true)
    {
        liot_trace("[STORE] save failed");
        return -1;
    }

    liot_trace("[STORE] saved msgId=%s", msg->msg_id);
    return 0;
}

/**
 * @brief 读取最后一条消息
 * @param[out] msg 输出消息
 * @return 0 成功，-1 无记录或数据损坏
 */
int screen_store_load(screen_msg_t *msg)
{
    screen_store_blob_t blob;

    if (msg == NULL)
    {
        return -1;
    }

    memset(&blob, 0, sizeof(blob));
    if (Liot_nvmRead((UINT8 *)&blob, (UINT32)sizeof(blob), SCREEN_STORE_OFFSET) != true)
    {
        liot_trace("[STORE] read failed");
        return -1;
    }

    if ((blob.magic != SCREEN_STORE_MAGIC) ||
        (blob.version != SCREEN_STORE_VERSION))
    {
        liot_trace("[STORE] no valid record, magic=0x%08X ver=%u",
                   (unsigned)blob.magic, (unsigned)blob.version);
        return -1;
    }

    *msg = blob.msg;

    /* 防御性补 '\0'，避免损坏数据导致后续越界读取 */
    msg->msg_id[SCREEN_MSG_ID_MAX_LEN - 1U] = '\0';
    msg->title[SCREEN_MSG_TITLE_MAX_LEN - 1U] = '\0';
    msg->content[SCREEN_MSG_CONTENT_MAX_LEN - 1U] = '\0';

    liot_trace("[STORE] loaded msgId=%s", msg->msg_id);
    return 0;
}
