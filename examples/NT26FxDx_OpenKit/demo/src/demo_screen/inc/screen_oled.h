/**
 * @file screen_oled.h
 * @brief SSD1306 128x64 OLED 显示接口
 *
 * 驱动本身不提供文字接口，本模块维护一块 1 KB 页式帧缓冲，
 * 所有绘制先写缓冲，最后统一 flush 到屏幕。
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-11-26
 * @version 1.0
 */

#ifndef SCREEN_OLED_H
#define SCREEN_OLED_H

#include <stdint.h>

#define SCREEN_OLED_WIDTH   (128U)                      /*!< 屏幕宽度，单位像素 */
#define SCREEN_OLED_HEIGHT  (64U)                       /*!< 屏幕高度，单位像素 */
#define SCREEN_OLED_PAGES   (SCREEN_OLED_HEIGHT / 8U)   /*!< 页数，每页 8 行 */
#define SCREEN_OLED_ON      (1U)                        /*!< 像素点亮 */
#define SCREEN_OLED_OFF     (0U)                        /*!< 像素熄灭 */

/**
 * @brief 初始化 OLED（上电、引脚复用、驱动注册）
 * @return 0 成功，-1 失败
 */
int screen_oled_init(void);

/**
 * @brief 清空帧缓冲（不立即刷屏）
 * @return 0 成功，-1 未初始化
 */
int screen_oled_clear(void);

/**
 * @brief 在帧缓冲中填充矩形
 * @param[in] x      左上角 X 坐标
 * @param[in] y      左上角 Y 坐标
 * @param[in] width  宽度，单位像素
 * @param[in] height 高度，单位像素
 * @param[in] color  SCREEN_OLED_ON 或 SCREEN_OLED_OFF
 * @return 0 成功，-1 未初始化
 */
int screen_oled_fill_rect(uint16_t x, uint16_t y, uint16_t width,
                          uint16_t height, uint8_t color);

/**
 * @brief 在帧缓冲中绘制 ASCII 字符串
 * @param[in] x      起始 X 坐标
 * @param[in] y      起始 Y 坐标
 * @param[in] text   以 '\0' 结尾的字符串
 * @param[in] invert 0 正常显示，非 0 反显（白底黑字）
 * @return 0 成功，-1 参数错误
 */
int screen_oled_draw_text(uint16_t x, uint16_t y, const char *text,
                          uint8_t invert);

/**
 * @brief 把帧缓冲写到屏幕并刷新
 * @return 0 成功，-1 未初始化或驱动返回错误
 */
int screen_oled_flush(void);

#endif /* SCREEN_OLED_H */
