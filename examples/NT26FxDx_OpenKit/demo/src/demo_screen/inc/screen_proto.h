/**
 * @file screen_proto.h
 * @brief 云播报屏协议组包：dev.reg 与 0xAA 心跳帧
 *
 * 协议约定见课程文档「统一通信协议」：
 *   - JSON 中的 ts 为 Unix 秒；
 *   - 0xAA 帧为 9 + N 字节，帧尾 0x0F，倒数第二字节为 XOR。
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-11-26
 * @version 1.0
 */

#ifndef SCREEN_PROTO_H
#define SCREEN_PROTO_H

#include <stdint.h>

#define SCREEN_FRAME_HEADER     (0xAAU)     /*!< 通用上行帧帧头 */
#define SCREEN_FRAME_FOOTER     (0x0FU)     /*!< 三种帧统一帧尾 */
#define SCREEN_FRAME_TYPE_HB    (0x01U)     /*!< 心跳消息类型 */
#define SCREEN_HB_PAYLOAD_LEN   (10U)       /*!< 心跳 payload 固定 10 字节 */
#define SCREEN_HB_FRAME_LEN     (9U + SCREEN_HB_PAYLOAD_LEN)

/**
 * @brief 组 dev.reg 注册 JSON
 * @param[in]  imei     设备 IMEI
 * @param[in]  stu_no   学号
 * @param[in]  seq      上行序号 0-255
 * @param[out] buf      输出缓冲区
 * @param[in]  buf_size 缓冲区长度
 * @return 成功返回 JSON 字符串长度（不含结束符），失败返回 -1
 */
int screen_proto_build_dev_reg(const char *imei, const char *stu_no,
                               uint8_t seq, char *buf, uint32_t buf_size);

/**
 * @brief 组 0xAA 心跳帧
 * @param[in]  imei     设备 IMEI，用于取末两位数字
 * @param[in]  seq      上行序号 0-255
 * @param[in]  uptime_s 设备运行时长，单位秒
 * @param[in]  sent     累计心跳发送次数
 * @param[in]  ok       累计上报成功次数
 * @param[in]  rssi     信号强度，dBm，可为负
 * @param[out] buf      输出缓冲区
 * @param[in]  buf_size 缓冲区长度，至少 SCREEN_HB_FRAME_LEN
 * @return 成功返回帧总长度，失败返回 -1
 */
int screen_proto_build_heartbeat(const char *imei, uint8_t seq,
                                 uint32_t uptime_s, uint16_t sent,
                                 uint16_t ok, int8_t rssi,
                                 uint8_t *buf, uint32_t buf_size);

#endif /* SCREEN_PROTO_H */
