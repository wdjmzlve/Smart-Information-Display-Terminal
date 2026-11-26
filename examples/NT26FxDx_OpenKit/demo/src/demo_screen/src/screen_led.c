/**
 * @file screen_led.c
 * @brief 三色灯（WS2812B）灯效实现
 *
 * 等级映射（项目10 基础要求第 5 条）：
 *   level 1 普通 -> 绿色常亮；
 *   level 2 提醒 -> 蓝色常亮；
 *   level 3 紧急 -> 红色闪烁。
 *
 * 实现方式与官方 demo_ws2812b.c 保持一致：
 *   1. 引脚复用完全交给 iodriver.ini 与 SPI 驱动，代码里不调用 Liot_SetPinFunc；
 *   2. SPI0 使用 5 MHz，每个 SPI 字节表示 WS2812B 的 1 位；
 *   3. 数据按 G-R-B 顺序打包，高位在前；
 *   4. 上电后第一帧推迟到第一次 tick，给灯珠 LDO 留出稳定时间。
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-11-26
 * @version 2.0
 */

#include <string.h>

#include "liot_gpio2.h"
#include "liot_log.h"
#include "liot_os.h"
#include "liot_spi.h"

#include "screen_config.h"
#include "screen_led.h"

/* 逻辑 0：前 2 位高、后 6 位低，对应 0.4us 高电平 */
#define SCREEN_LED_BIT_LOW          (0xC0U)
/* 逻辑 1：前 6 位高、后 2 位低，对应 0.8us 高电平 */
#define SCREEN_LED_BIT_HIGH         (0xFCU)
/* 每颗灯 24 bit（G8/R8/B8），每 bit 占 1 个 SPI 字节 */
#define SCREEN_LED_BYTES_PER_LED    (24U)
/* 一次发送的总字节数 */
#define SCREEN_LED_BUF_SIZE         (SCREEN_LED_COUNT * SCREEN_LED_BYTES_PER_LED)

/**
 * @brief RGB 颜色
 */
typedef struct
{
    uint8_t r;  /*!< 红色分量 0-255 */
    uint8_t g;  /*!< 绿色分量 0-255 */
    uint8_t b;  /*!< 蓝色分量 0-255 */
} screen_led_rgb_t;

/* 等级颜色表，下标 = level - SCREEN_LEVEL_NORMAL */
static const screen_led_rgb_t s_level_color[SCREEN_LEVEL_MAX - SCREEN_LEVEL_NORMAL] =
{
    { SCREEN_LED_RGB_NORMAL_R, SCREEN_LED_RGB_NORMAL_G, SCREEN_LED_RGB_NORMAL_B },
    { SCREEN_LED_RGB_NOTICE_R, SCREEN_LED_RGB_NOTICE_G, SCREEN_LED_RGB_NOTICE_B },
    { SCREEN_LED_RGB_URGENT_R, SCREEN_LED_RGB_URGENT_G, SCREEN_LED_RGB_URGENT_B },
};

/* 灭灯颜色，闪烁的暗半周期使用 */
static const screen_led_rgb_t s_color_off = { 0U, 0U, 0U };

/* SPI 发送缓冲，避免每次发送都申请堆内存 */
static uint8_t s_spi_buf[SCREEN_LED_BUF_SIZE];

/**
 * @brief 灯效运行上下文
 */
typedef struct
{
    screen_level_e   level;         /*!< 当前灯效等级 */
    screen_led_rgb_t color;         /*!< 当前颜色，用于延迟发送 */
    uint32_t         blink_acc_ms;  /*!< 闪烁累计时间 */
    uint8_t          ready;         /*!< 1=SPI 已初始化 */
    uint8_t          blink_on;      /*!< 1=亮的半周期 */
    uint8_t          pending;       /*!< 1=上电第一帧还没发 */
    uint8_t          error_logged;  /*!< 发送失败只告警一次，避免刷屏 */
} screen_led_ctx_t;

static screen_led_ctx_t s_led_ctx;

/**
 * @brief 把一种颜色打包成 WS2812B 的 SPI 波形
 * @param[out] out   输出缓冲，长度 SCREEN_LED_BYTES_PER_LED
 * @param[in]  color 目标颜色
 * @return 无
 * @note WS2812B 数据顺序为 G-R-B，高位在前
 */
static void screen_led_pack(uint8_t *out, const screen_led_rgb_t *color)
{
    uint8_t  g = (uint8_t)(((uint16_t)color->g * SCREEN_LED_BRIGHTNESS) / 255U);
    uint8_t  r = (uint8_t)(((uint16_t)color->r * SCREEN_LED_BRIGHTNESS) / 255U);
    uint8_t  b = (uint8_t)(((uint16_t)color->b * SCREEN_LED_BRIGHTNESS) / 255U);
    uint8_t  i;

    for (i = 0U; i < 8U; i++)
    {
        out[i]      = ((g & (uint8_t)(1U << (7U - i))) != 0U) ? SCREEN_LED_BIT_HIGH : SCREEN_LED_BIT_LOW;
        out[8U + i] = ((r & (uint8_t)(1U << (7U - i))) != 0U) ? SCREEN_LED_BIT_HIGH : SCREEN_LED_BIT_LOW;
        out[16U + i] = ((b & (uint8_t)(1U << (7U - i))) != 0U) ? SCREEN_LED_BIT_HIGH : SCREEN_LED_BIT_LOW;
    }
}

/**
 * @brief 把颜色写到全部灯珠
 * @param[in] color 目标颜色
 * @return 0 成功，-1 SPI 不可用或发送失败
 */
static int screen_led_apply(const screen_led_rgb_t *color)
{
    liot_errcode_spi_e ret;
    uint32_t           i;

    if ((s_led_ctx.ready == 0U) || (color == NULL))
    {
        return -1;
    }

    for (i = 0U; i < SCREEN_LED_COUNT; i++)
    {
        screen_led_pack(&s_spi_buf[i * SCREEN_LED_BYTES_PER_LED], color);
    }

    ret = liot_spi_write((liot_spi_port_e)SCREEN_LED_SPI_PORT,
                         s_spi_buf, (unsigned int)SCREEN_LED_BUF_SIZE);
    if (ret != LIOT_SPI_SUCCESS)
    {
        if (s_led_ctx.error_logged == 0U)
        {
            liot_trace("[LED] spi write failed ret=%d", (int)ret);
            s_led_ctx.error_logged = 1U;
        }
        return -1;
    }

    s_led_ctx.color = *color;
    return 0;
}

/**
 * @brief 初始化三色灯
 * @return 0 成功，-1 上电或 SPI 初始化失败
 */
int screen_led_init(void)
{
    liot_errcode_spi_e ret;

    memset(&s_led_ctx, 0, sizeof(s_led_ctx));
    s_led_ctx.level = SCREEN_LEVEL_NORMAL;
    s_led_ctx.color = s_level_color[0];
    s_led_ctx.blink_on = 1U;

    /* 与 OLED 一致：AON 上电 + 3.3V；GPIO25 拉高才能给 WS2812B 的 LDO 供电 */
    if (Liot_AonPowerCtl(true) != L_GPIO_ERR_SUCCESS)
    {
        liot_trace("[LED] AON power on failed");
        return -1;
    }

    if (Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V) != L_GPIO_ERR_SUCCESS)
    {
        liot_trace("[LED] set voltage failed");
        return -1;
    }

    if (Liot_GpioInit(L_GPIO_25, L_IO_OUTPUT, L_IO_HIGH, NULL) != L_GPIO_ERR_SUCCESS)
    {
        liot_trace("[LED] LDO enable failed");
        return -1;
    }

    /* 引脚复用由 iodriver.ini 提供，这里只初始化总线。
     * 用轮询模式而不是 DMA 模式：音频播放同样走 DMA，避免两者争用同一资源。
     * 72 字节 @5MHz 约 115us 忙等，每秒最多一次，对主循环无影响。 */
    ret = liot_spi_init((liot_spi_port_e)SCREEN_LED_SPI_PORT,
                        LIOT_SPI_DIRECT_POLLING,
                        (liot_spi_clk_e)SCREEN_LED_SPI_CLK_HZ);
    if (ret != LIOT_SPI_SUCCESS)
    {
        liot_trace("[LED] spi init failed ret=%d", (int)ret);
        return -1;
    }

    s_led_ctx.ready = 1U;
    /* LDO 刚上电时灯珠还没准备好，第一帧推迟到第一次 tick 再发 */
    s_led_ctx.pending = 1U;

    liot_trace("[LED] ready, leds=%u clk=%u",
               (unsigned)SCREEN_LED_COUNT, (unsigned)SCREEN_LED_SPI_CLK_HZ);
    return 0;
}

/**
 * @brief 按消息等级切换灯效
 * @param[in] level 消息等级，越界时按普通处理
 * @return 无
 */
void screen_led_set_level(screen_level_e level)
{
    uint32_t index;

    if ((level < SCREEN_LEVEL_NORMAL) || (level >= SCREEN_LEVEL_MAX))
    {
        level = SCREEN_LEVEL_NORMAL;
    }

    s_led_ctx.level = level;
    s_led_ctx.blink_on = 1U;
    s_led_ctx.blink_acc_ms = 0U;
    s_led_ctx.pending = 0U;

    index = (uint32_t)(level - SCREEN_LEVEL_NORMAL);
    (void)screen_led_apply(&s_level_color[index]);
}

/**
 * @brief 灯效周期驱动
 * @param[in] elapsed_ms 距上次调用的时间，单位 ms
 * @return 无
 * @note 同时负责补发上电第一帧，以及紧急等级的亮灭翻转
 */
void screen_led_tick(uint32_t elapsed_ms)
{
    uint32_t index;

    if (s_led_ctx.ready == 0U)
    {
        return;
    }

    /* 上电后第一次 tick 才发第一帧，给 LDO 和灯珠留稳定时间 */
    if (s_led_ctx.pending != 0U)
    {
        s_led_ctx.pending = 0U;
        (void)screen_led_apply(&s_led_ctx.color);
    }

    if (s_led_ctx.level != SCREEN_LEVEL_URGENT)
    {
        return;
    }

    s_led_ctx.blink_acc_ms += elapsed_ms;
    if (s_led_ctx.blink_acc_ms < SCREEN_LED_BLINK_INTERVAL_MS)
    {
        return;
    }

    s_led_ctx.blink_acc_ms = 0U;
    s_led_ctx.blink_on ^= 1U;

    index = (uint32_t)(s_led_ctx.level - SCREEN_LEVEL_NORMAL);
    if (s_led_ctx.blink_on != 0U)
    {
        (void)screen_led_apply(&s_level_color[index]);
    }
    else
    {
        (void)screen_led_apply(&s_color_off);
    }
}
