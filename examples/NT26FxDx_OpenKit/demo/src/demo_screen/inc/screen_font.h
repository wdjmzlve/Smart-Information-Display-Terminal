/**
 * @file screen_font.h
 * @brief ASCII 8x16 点阵字模接口
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-11-26
 * @version 1.0
 */

#ifndef SCREEN_FONT_H
#define SCREEN_FONT_H

#include <stdint.h>

#define SCREEN_FONT_WIDTH   (8U)    /*!< 字符宽度，单位像素 */
#define SCREEN_FONT_HEIGHT  (16U)   /*!< 字符高度，单位像素 */

/**
 * @brief 取 ASCII 字符的 8x16 点阵
 * @param[in] ch 待查询字符
 * @return 16 字节字模指针；非 ASCII 可打印字符返回 NULL
 */
const uint8_t *screen_font_ascii_glyph(char ch);

#endif /* SCREEN_FONT_H */
