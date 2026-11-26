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
