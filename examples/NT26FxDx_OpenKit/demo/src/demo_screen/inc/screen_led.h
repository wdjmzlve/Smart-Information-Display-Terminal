/**
 * @file screen_led.h
 * @brief 三色灯（WS2812B）接口
 *
 * 骨架阶段不驱动硬件：I2C1 与 SPI0 共用模块引脚 66，
 * 先保证 OLED 通路可用，Stage 7 再补灯效实现。
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-11-25
 * @version 1.0
 */

#ifndef SCREEN_LED_H
#define SCREEN_LED_H

#include <stdint.h>

/**
 * @brief 消息等级，与 screen.msg 的 level 字段一致
 */
typedef enum
{
    SCREEN_LEVEL_NORMAL = 1,    /*!< 普通：绿色常亮 */
    SCREEN_LEVEL_NOTICE = 2,    /*!< 提醒：蓝色常亮 */
    SCREEN_LEVEL_URGENT = 3,    /*!< 紧急：红色闪烁 */
    SCREEN_LEVEL_MAX            /*!< 枚举上界 */
} screen_level_e;

/**
 * @brief 初始化灯效模块
 * @return 0 成功，负值失败
 */
int screen_led_init(void);

/**
 * @brief 按消息等级切换灯效
 * @param[in] level 消息等级
 * @return 无
 */
void screen_led_set_level(screen_level_e level);

/**
 * @brief 灯效周期驱动
 * @param[in] elapsed_ms 距上次调用的时间，单位 ms
 * @return 无
 */
void screen_led_tick(uint32_t elapsed_ms);

#endif /* SCREEN_LED_H */
