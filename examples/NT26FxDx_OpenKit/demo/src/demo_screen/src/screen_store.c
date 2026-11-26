/**
 * @file screen_store.c
 * @brief 最后一条消息的 NVM 持久化
 *
 * 存储结构带 magic 与版本号，读取时校验，避免旧数据或损坏数据被当成有效消息。
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-11-26
 * @version 1.0
 */

#include <string.h>

#include "liot_log.h"
#include "liot_nv.h"

#include "screen_config.h"
#include "screen_proto.h"
#include "screen_store.h"

#define SCREEN_STORE_MAGIC      (0x5343524EU)   /*!< 'SCRN'，用于识别本模块的数据 */
#define SCREEN_STORE_VERSION    (1U)            /*!< 结构版本，字段变化时递增 */

/**
 * @brief NVM 中实际保存的数据块
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
 */
int screen_store_save(const screen_msg_t *msg)
{
    screen_store_blob_t blob;
    int ret;

    if (msg == NULL)
    {
        return -1;
    }

    memset(&blob, 0, sizeof(blob));
    blob.magic = SCREEN_STORE_MAGIC;
    blob.version = SCREEN_STORE_VERSION;
    blob.msg = *msg;

    ret = liot_cust_nvm_fwrite(&blob, sizeof(blob), 1U);
    if (ret != (int)sizeof(blob))
    {
        liot_trace("[STORE] save failed: %d", ret);
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
    int ret;

    if (msg == NULL)
    {
        return -1;
    }

    memset(&blob, 0, sizeof(blob));
    ret = liot_cust_nvm_fread(&blob, sizeof(blob), 1U);
    if (ret != (int)sizeof(blob))
    {
        liot_trace("[STORE] no saved message: %d", ret);
        return -1;
    }

    if ((blob.magic != SCREEN_STORE_MAGIC) ||
        (blob.version != SCREEN_STORE_VERSION))
    {
        liot_trace("[STORE] invalid record, magic=0x%08X ver=%u",
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
