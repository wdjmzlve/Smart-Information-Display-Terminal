/**
 * @file screen_text.h
 * @brief UTF-8 文本解析与按像素宽度折行
 *
 * 字模阶段先只支持 ASCII，但服务器正文是 UTF-8 中文，
 * 因此这里统一按码点处理：ASCII 占 8 像素，非 ASCII 占 16 像素。
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-11-26
 * @version 1.0
 */

#ifndef SCREEN_TEXT_H
#define SCREEN_TEXT_H

#include <stdint.h>

#include "screen_config.h"
#include "screen_font.h"

#define SCREEN_TEXT_CJK_WIDTH   (16U)   /*!< 非 ASCII 字符的显示宽度 */

/**
 * @brief 解码一个 UTF-8 码点
 * @param[in]  text     以 '\0' 结尾的字符串
 * @param[out] codepoint 解码出的码点
 * @return 消耗的字节数；字符串结束或参数错误返回 0，非法序列按 1 字节处理
 */
uint32_t screen_text_utf8_decode(const char *text, uint32_t *codepoint);

/**
 * @brief 取码点的显示宽度
 * @param[in] codepoint UTF-8 码点
 * @return ASCII 返回 8，其余返回 16
 */
uint32_t screen_text_cell_width(uint32_t codepoint);

/**
 * @brief 按像素宽度把文本折成多行
 * @param[in]  text      源文本，支持 '\n' 强制换行
 * @param[out] lines     行缓冲，二维数组，每行 SCREEN_LINE_MAX_BYTES 字节
 * @param[in]  max_lines 最多输出多少行
 * @param[in]  max_width 单行最大像素宽度
 * @return 实际行数（空文本返回 1，且首行为空串）
 */
uint32_t screen_text_wrap(const char *text,
                          char lines[][SCREEN_LINE_MAX_BYTES],
                          uint32_t max_lines,
                          uint32_t max_width);

#endif /* SCREEN_TEXT_H */
