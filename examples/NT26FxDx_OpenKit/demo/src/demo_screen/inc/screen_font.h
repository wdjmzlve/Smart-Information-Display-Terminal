/**
 * @file screen_font.h
 * @brief 点阵字模接口：ASCII 8x16 + 汉字 16x16
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-11-26
 * @version 2.0
 */

#ifndef SCREEN_FONT_H
#define SCREEN_FONT_H

#include <stdint.h>

#define SCREEN_FONT_WIDTH   (8U)    /*!< ASCII 字符宽度，单位像素 */
#define SCREEN_FONT_HEIGHT  (16U)   /*!< 字符高度，单位像素 */

/* 汉字点阵：16x16，每个字 16 行 x 2 字节 */
#define SCREEN_FONT_CJK_WIDTH   (16U)   /*!< 汉字宽度，单位像素 */
#define SCREEN_FONT_CJK_HEIGHT  (16U)   /*!< 汉字高度，单位像素 */
#define SCREEN_FONT_CJK_BYTES   (32U)   /*!< 每个汉字的点阵字节数 */

/* 基本汉字区码点范围，超出范围不必查表 */
#define SCREEN_FONT_CJK_FIRST   (0x4E00U)
#define SCREEN_FONT_CJK_LAST    (0x9FFFU)

/**
 * @brief 取 ASCII 字符的 8x16 点阵
 * @param[in] ch 待查询字符
 * @return 16 字节字模指针；非 ASCII 可打印字符返回 NULL
 */
const uint8_t *screen_font_ascii_glyph(char ch);

/**
 * @brief 取汉字的 16x16 点阵
 * @param[in] codepoint UTF-8 解码后的 Unicode 码点
 * @return 32 字节字模指针；字库中没有该字时返回 NULL
 */
const uint8_t *screen_font_cjk_glyph(uint32_t codepoint);

#endif /* SCREEN_FONT_H */
