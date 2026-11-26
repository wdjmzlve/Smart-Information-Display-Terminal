/**
 * @file screen_proto.c
 * @brief 云播报屏协议组包实现
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-11-26
 * @version 1.0
 */

#include <string.h>
#include <stdint.h>

#include "cJSON.h"
#include "liot_dev.h"
#include "liot_log.h"
#include "liot_rtc.h"

#include "screen_config.h"
#include "screen_proto.h"

/**
 * @brief 取 IMEI 倒数第 n 位数字
 * @param[in] imei     设备 IMEI
 * @param[in] from_end 倒数位置，1 表示最后一位
 * @return 0-9 的数字；IMEI 非法时返回 0
 */
static uint8_t screen_proto_imei_tail_digit(const char *imei, uint32_t from_end)
{
    size_t len;
    char   c;

    if ((imei == NULL) || (from_end == 0U))
    {
        return 0U;
    }

    len = strlen(imei);
    if (len < from_end)
    {
        return 0U;
    }

    c = imei[len - from_end];
    if ((c < '0') || (c > '9'))
    {
        return 0U;
    }

    return (uint8_t)(c - '0');
}

/**
 * @brief 逐字节异或校验
 * @param[in] data 数据起始地址
 * @param[in] len  数据长度
 * @return 异或结果
 */
static uint8_t screen_proto_xor(const uint8_t *data, uint32_t len)
{
    uint8_t value = 0U;
    uint32_t i;

    for (i = 0U; i < len; i++)
    {
        value ^= data[i];
    }

    return value;
}

/**
 * @brief 组 dev.reg 注册 JSON
 * @param[in]  imei     设备 IMEI
 * @param[in]  stu_no   学号
 * @param[in]  seq      上行序号
 * @param[out] buf      输出缓冲区
 * @param[in]  buf_size 缓冲区长度
 * @return 成功返回长度，失败返回 -1
 */
int screen_proto_build_dev_reg(const char *imei, const char *stu_no,
                               uint8_t seq, char *buf, uint32_t buf_size)
{
    cJSON *root = NULL;
    cJSON *data = NULL;
    char  *text = NULL;
    char   fw_ver[SCREEN_FW_VER_LEN];
    size_t text_len;
    int    result = -1;

    if ((imei == NULL) || (stu_no == NULL) || (buf == NULL))
    {
        return -1;
    }

    memset(fw_ver, 0, sizeof(fw_ver));
    if (liot_dev_get_firmware_version(fw_ver, sizeof(fw_ver)) != LIOT_DEV_SUCCESS)
    {
        liot_trace("[PROTO] get fw version failed, use fallback");
        (void)strncpy(fw_ver, SCREEN_FW_VER_FALLBACK, sizeof(fw_ver) - 1U);
    }

    root = cJSON_CreateObject();
    data = cJSON_CreateObject();
    if ((root == NULL) || (data == NULL))
    {
        liot_trace("[PROTO] json create failed");
        goto cleanup;
    }

    cJSON_AddStringToObject(root, "imei", imei);
    cJSON_AddNumberToObject(root, "ts", (double)liot_rtc_get_time_s());
    cJSON_AddStringToObject(root, "type", "dev.reg");
    cJSON_AddNumberToObject(root, "seq", (double)seq);

    /* 挂到 root 后由 root 负责释放，data 指针仍然有效 */
    cJSON_AddItemToObject(root, "data", data);
    cJSON_AddStringToObject(data, "stuNo", stu_no);
    cJSON_AddNumberToObject(data, "proj", (double)SCREEN_PROJECT_NO);
    cJSON_AddStringToObject(data, "fwVer", fw_ver);
    data = NULL;

    text = cJSON_PrintUnformatted(root);
    if (text == NULL)
    {
        liot_trace("[PROTO] json print failed");
        goto cleanup;
    }

    text_len = strlen(text);
    if (text_len + 1U > buf_size)
    {
        liot_trace("[PROTO] dev.reg too long: %u", (unsigned)text_len);
        goto cleanup;
    }

    memcpy(buf, text, text_len + 1U);
    result = (int)text_len;

cleanup:
    if (text != NULL)
    {
        cJSON_free(text);
    }
    if (data != NULL)
    {
        cJSON_Delete(data);
    }
    if (root != NULL)
    {
        cJSON_Delete(root);
    }

    return result;
}

/**
 * @brief 组 0xAA 心跳帧
 * @param[in]  imei     设备 IMEI
 * @param[in]  seq      上行序号
 * @param[in]  uptime_s 运行时长，单位秒
 * @param[in]  sent     累计发送次数
 * @param[in]  ok       累计成功次数
 * @param[in]  rssi     信号强度
 * @param[out] buf      输出缓冲区
 * @param[in]  buf_size 缓冲区长度
 * @return 成功返回帧长度，失败返回 -1
 */
int screen_proto_build_heartbeat(const char *imei, uint8_t seq,
                                 uint32_t uptime_s, uint16_t sent,
                                 uint16_t ok, int8_t rssi,
                                 uint8_t *buf, uint32_t buf_size)
{
    uint32_t idx = 0U;

    if ((imei == NULL) || (buf == NULL) || (buf_size < SCREEN_HB_FRAME_LEN))
    {
        return -1;
    }

    buf[idx++] = SCREEN_FRAME_HEADER;
    buf[idx++] = screen_proto_imei_tail_digit(imei, 2U);
    buf[idx++] = screen_proto_imei_tail_digit(imei, 1U);
    buf[idx++] = SCREEN_FRAME_TYPE_HB;
    buf[idx++] = seq;
    buf[idx++] = (uint8_t)((SCREEN_HB_PAYLOAD_LEN >> 8) & 0xFFU);
    buf[idx++] = (uint8_t)(SCREEN_HB_PAYLOAD_LEN & 0xFFU);

    /* payload：RSSI + uptime(4) + sent(2) + ok(2) + RFU(1) */
    buf[idx++] = (uint8_t)rssi;
    buf[idx++] = (uint8_t)((uptime_s >> 24) & 0xFFU);
    buf[idx++] = (uint8_t)((uptime_s >> 16) & 0xFFU);
    buf[idx++] = (uint8_t)((uptime_s >> 8) & 0xFFU);
    buf[idx++] = (uint8_t)(uptime_s & 0xFFU);
    buf[idx++] = (uint8_t)((sent >> 8) & 0xFFU);
    buf[idx++] = (uint8_t)(sent & 0xFFU);
    buf[idx++] = (uint8_t)((ok >> 8) & 0xFFU);
    buf[idx++] = (uint8_t)(ok & 0xFFU);
    buf[idx++] = 0x00U;

    buf[idx] = screen_proto_xor(buf, idx);
    idx++;
    buf[idx++] = SCREEN_FRAME_FOOTER;

    return (int)idx;
}

/**
 * @brief 拷贝 JSON 字符串字段
 * @param[in]  obj      JSON 对象
 * @param[in]  key      字段名
 * @param[out] dst      输出缓冲区
 * @param[in]  dst_size 缓冲区长度
 * @param[in]  required 1 表示字段缺失视为错误
 * @return 0 成功，-1 缺失或类型错误
 */
static int screen_proto_copy_string(const cJSON *obj, const char *key,
                                    char *dst, size_t dst_size, int required)
{
    const cJSON *item;
    size_t       len;

    item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (item == NULL)
    {
        return (required != 0) ? -1 : 0;
    }

    if (!cJSON_IsString(item) || (item->valuestring == NULL))
    {
        return -1;
    }

    len = strlen(item->valuestring);
    if (len >= dst_size)
    {
        len = dst_size - 1U;    /* 超长截断，但仍保证 '\0' 结尾 */
    }

    memcpy(dst, item->valuestring, len);
    dst[len] = '\0';
    return 0;
}

/**
 * @brief 读取 JSON 整数字段
 * @param[in]  obj JSON 对象
 * @param[in]  key 字段名
 * @param[out] out 输出值
 * @return 0 成功，-1 缺失或不是数字
 */
static int screen_proto_get_int(const cJSON *obj, const char *key, int *out)
{
    const cJSON *item;

    item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsNumber(item))
    {
        return -1;
    }

    *out = item->valueint;
    return 0;
}

/**
 * @brief 解析 screen.msg
 * @param[in]  payload 下行载荷
 * @param[in]  len     载荷长度
 * @param[out] out     解析结果
 * @return 解析结果码
 */
screen_proto_parse_result_e screen_proto_parse_screen_msg(const uint8_t *payload,
                                                          uint16_t len,
                                                          screen_msg_t *out)
{
    cJSON *root = NULL;
    cJSON *parsed_data = NULL;
    const cJSON *type_item;
    const cJSON *data;
    const cJSON *ts_item;
    const cJSON *tts_item;
    screen_proto_parse_result_e result = SCREEN_PROTO_PARSE_INVALID_JSON;
    int level = 0;
    int tts = SCREEN_MSG_TTS_OFF;

    if ((payload == NULL) || (out == NULL) || (len == 0U))
    {
        return SCREEN_PROTO_PARSE_INVALID_JSON;
    }

    memset(out, 0, sizeof(*out));

    /* 载荷不保证以 '\0' 结尾，必须按长度解析 */
    root = cJSON_ParseWithLength((const char *)payload, (size_t)len);
    if (root == NULL)
    {
        return SCREEN_PROTO_PARSE_INVALID_JSON;
    }

    type_item = cJSON_GetObjectItemCaseSensitive(root, "type");
    if (!cJSON_IsString(type_item) || (type_item->valuestring == NULL) ||
        (strcmp(type_item->valuestring, "screen.msg") != 0))
    {
        result = SCREEN_PROTO_PARSE_NOT_SCREEN_MSG;
        goto done;
    }

    data = cJSON_GetObjectItemCaseSensitive(root, "data");

    /* 少数服务器会把 data 作为字符串下发，这里尝试再解析一次 */
    if (cJSON_IsString(data) && (data->valuestring != NULL))
    {
        parsed_data = cJSON_Parse(data->valuestring);
        if (parsed_data != NULL)
        {
            data = parsed_data;
        }
    }

    if (!cJSON_IsObject(data))
    {
        liot_trace("[PROTO] missing data object");
        result = SCREEN_PROTO_PARSE_MISSING_FIELD;
        goto done;
    }

    /*
     * 部分服务器面板会把完整报文再包一层，真正的字段在 data.data 里。
     * 只兼容一层嵌套，且仅当 data 自身没有 msgId 时才下钻，避免误判。
     */
    if (cJSON_GetObjectItemCaseSensitive(data, "msgId") == NULL)
    {
        const cJSON *nested = cJSON_GetObjectItemCaseSensitive(data, "data");

        if (cJSON_IsObject(nested))
        {
            liot_trace("[PROTO] unwrap nested data");
            data = nested;
        }
    }

    if (screen_proto_copy_string(data, "msgId", out->msg_id,
                                 sizeof(out->msg_id), 1) != 0)
    {
        liot_trace("[PROTO] missing msgId");
        result = SCREEN_PROTO_PARSE_MISSING_FIELD;
        goto done;
    }
    if (out->msg_id[0] == '\0')
    {
        liot_trace("[PROTO] empty msgId");
        result = SCREEN_PROTO_PARSE_BAD_VALUE;
        goto done;
    }

    if (screen_proto_copy_string(data, "title", out->title,
                                 sizeof(out->title), 1) != 0)
    {
        liot_trace("[PROTO] missing title");
        result = SCREEN_PROTO_PARSE_MISSING_FIELD;
        goto done;
    }

    if (screen_proto_copy_string(data, "content", out->content,
                                 sizeof(out->content), 1) != 0)
    {
        liot_trace("[PROTO] missing content");
        result = SCREEN_PROTO_PARSE_MISSING_FIELD;
        goto done;
    }

    if (screen_proto_get_int(data, "level", &level) != 0)
    {
        liot_trace("[PROTO] missing level");
        result = SCREEN_PROTO_PARSE_MISSING_FIELD;
        goto done;
    }
    if ((level < SCREEN_MSG_LEVEL_MIN) || (level > SCREEN_MSG_LEVEL_MAX))
    {
        result = SCREEN_PROTO_PARSE_BAD_VALUE;
        goto done;
    }

    /* tts 可选：缺省按“仅显示”处理；存在但不是数字则判为非法 */
    tts_item = cJSON_GetObjectItemCaseSensitive(data, "tts");
    if (tts_item != NULL)
    {
        if (!cJSON_IsNumber(tts_item))
        {
            result = SCREEN_PROTO_PARSE_BAD_VALUE;
            goto done;
        }

        tts = tts_item->valueint;
        if ((tts != SCREEN_MSG_TTS_OFF) && (tts != SCREEN_MSG_TTS_ON))
        {
            result = SCREEN_PROTO_PARSE_BAD_VALUE;
            goto done;
        }
    }

    ts_item = cJSON_GetObjectItemCaseSensitive(root, "ts");
    if (cJSON_IsNumber(ts_item) && (ts_item->valuedouble > 0))
    {
        out->ts = (uint32_t)ts_item->valuedouble;
    }

    out->level = level;
    out->tts = tts;
    result = SCREEN_PROTO_PARSE_OK;

done:
    if (parsed_data != NULL)
    {
        cJSON_Delete(parsed_data);
    }
    cJSON_Delete(root);
    return result;
}
