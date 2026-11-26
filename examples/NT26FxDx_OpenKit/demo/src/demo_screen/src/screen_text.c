/**
 * @file screen_text.c
 * @brief UTF-8 文本解析与折行实现
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-11-26
 * @version 1.0
 */

#include <string.h>

#include "screen_text.h"

/**
 * @brief 解码一个 UTF-8 码点
 * @param[in]  text     以 '\0' 结尾的字符串
 * @param[out] codepoint 解码出的码点
 * @return 消耗的字节数；结束或参数错误返回 0
 */
uint32_t screen_text_utf8_decode(const char *text, uint32_t *codepoint)
{
    const uint8_t *p = (const uint8_t *)text;
    uint32_t       cp;
    uint32_t       len;
    uint32_t       i;

    if ((text == NULL) || (codepoint == NULL) || (p[0] == 0U))
    {
        if (codepoint != NULL)
        {
            *codepoint = 0U;
        }
        return 0U;
    }

    /* 单字节 ASCII */
    if (p[0] < 0x80U)
    {
        *codepoint = p[0];
        return 1U;
    }

    if ((p[0] & 0xE0U) == 0xC0U)
    {
        len = 2U;
        cp = (uint32_t)(p[0] & 0x1FU);
    }
    else if ((p[0] & 0xF0U) == 0xE0U)
    {
        len = 3U;
        cp = (uint32_t)(p[0] & 0x0FU);
    }
    else if ((p[0] & 0xF8U) == 0xF0U)
    {
        len = 4U;
        cp = (uint32_t)(p[0] & 0x07U);
    }
    else
    {
        /* 非法首字节：按单字节处理，交给显示层画占位 */
        *codepoint = p[0];
        return 1U;
    }

    for (i = 1U; i < len; i++)
    {
        if ((p[i] & 0xC0U) != 0x80U)
        {
            /* 序列被截断，按首字节处理 */
            *codepoint = p[0];
            return 1U;
        }
        cp = (cp << 6) | (uint32_t)(p[i] & 0x3FU);
    }

    *codepoint = cp;
    return len;
}

/**
 * @brief 取码点的显示宽度
 * @param[in] codepoint UTF-8 码点
 * @return 显示宽度，单位像素
 */
uint32_t screen_text_cell_width(uint32_t codepoint)
{
    return (codepoint < 0x80U) ? SCREEN_FONT_WIDTH : SCREEN_TEXT_CJK_WIDTH;
}

/**
 * @brief 按像素宽度折行
 * @param[in]  text      源文本
 * @param[out] lines     行缓冲
 * @param[in]  max_lines 最多行数
 * @param[in]  max_width 单行最大宽度
 * @return 实际行数
 */
uint32_t screen_text_wrap(const char *text,
                          char lines[][SCREEN_LINE_MAX_BYTES],
                          uint32_t max_lines,
                          uint32_t max_width)
{
    uint32_t    line = 0U;
    uint32_t    used_px = 0U;
    uint32_t    used_bytes = 0U;
    const char *p = text;

    if ((text == NULL) || (lines == NULL) || (max_lines == 0U) ||
        (max_width == 0U))
    {
        return 0U;
    }

    lines[0][0] = '\0';

    while (*p != '\0')
    {
        uint32_t cp = 0U;
        uint32_t n;
        uint32_t w;

        n = screen_text_utf8_decode(p, &cp);
        if (n == 0U)
        {
            break;
        }

        if (cp == (uint32_t)'\r')
        {
            p += n;
            continue;
        }

        if (cp == (uint32_t)'\n')
        {
            if ((line + 1U) >= max_lines)
            {
                break;
            }
            line++;
            used_px = 0U;
            used_bytes = 0U;
            lines[line][0] = '\0';
            p += n;
            continue;
        }

        w = screen_text_cell_width(cp);

        /* 放不下就先换行，避免把字符劈成两半 */
        if ((used_px + w > max_width) && (used_px > 0U))
        {
            if ((line + 1U) >= max_lines)
            {
                break;
            }
            line++;
            used_px = 0U;
            used_bytes = 0U;
            lines[line][0] = '\0';
        }

        /* 行缓冲足够才写入；按最宽情况预留结束符 */
        if ((used_bytes + n + 1U) <= SCREEN_LINE_MAX_BYTES)
        {
            memcpy(&lines[line][used_bytes], p, n);
            used_bytes += n;
            lines[line][used_bytes] = '\0';
            used_px += w;
        }

        p += n;
    }

    return line + 1U;
}
