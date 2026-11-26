/**
 * @file screen_led.c
 * @brief 三色灯占位实现
 *
 * Stage 7 才启用 WS2812B：需要先确认 I2C1（OLED）与 SPI0 的引脚 66 冲突。
 * 当前接口保持可调用，业务层无需感知硬件是否就绪。
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-11-25
 * @version 1.0
 */

#include "screen_led.h"

#include "liot_log.h"

/**
 * @brief 初始化灯效模块（占位）
 * @return 恒为 0，表示软件层可用
 */
int screen_led_init(void)
{
    liot_trace("[LED] WS2812B disabled in skeleton stage");
    return 0;
}

/**
 * @brief 按等级设置灯效（占位）
 * @param[in] level 消息等级，当前忽略
 * @return 无
 */
void screen_led_set_level(screen_level_e level)
{
    (void)level;    /* Stage 7 按等级映射绿/蓝/红闪 */
}

/**
 * @brief 灯效周期驱动（占位）
 * @param[in] elapsed_ms 距上次调用的毫秒数，当前忽略
 * @return 无
 */
void screen_led_tick(uint32_t elapsed_ms)
{
    (void)elapsed_ms;   /* Stage 7 驱动闪烁节拍 */
}
