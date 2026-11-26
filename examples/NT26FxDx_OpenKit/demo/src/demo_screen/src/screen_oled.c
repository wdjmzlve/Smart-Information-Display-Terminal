/**
 * @file screen_oled.c
 * @brief SSD1306 128x64 OLED 显示实现
 *
 * 帧缓冲采用 SSD1306 原生页式布局：1024 字节，每字节代表一列中的 8 个纵向像素，
 * bit0 为最上方像素。刷新时整块写入驱动 gram，再调用 refresh 通过 I2C 送到屏幕。
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-11-26
 * @version 1.0
 */

#include <string.h>

#include "liot_gpio2.h"
#include "liot_lcd.h"
#include "liot_log.h"
#include "liot_os.h"
#include "liot_sleep.h"

#include "screen_config.h"
#include "screen_font.h"
#include "screen_oled.h"

/* SSD1306 驱动由 basePkg 提供，这里只做外部声明后绑定到 lcdDev */
LIOT_ADD_DISPLAY(liot_ssd1306_12864_dev);

/* 帧缓冲：128 列 x 8 页 */
static uint8_t s_fb[SCREEN_OLED_WIDTH * SCREEN_OLED_PAGES];

/* LCD 句柄，NULL 表示未初始化 */
static liot_lcd_handle_t s_lcd = NULL;

/**
 * @brief 设置帧缓冲中的单个像素
 * @param[in] x     横坐标
 * @param[in] y     纵坐标
 * @param[in] color SCREEN_OLED_ON 或 SCREEN_OLED_OFF
 * @return 无
 */
static void screen_oled_set_pixel(uint16_t x, uint16_t y, uint8_t color)
{
    uint16_t index;
    uint8_t  mask;

    if ((x >= SCREEN_OLED_WIDTH) || (y >= SCREEN_OLED_HEIGHT))
    {
        return;
    }

    index = (uint16_t)((y / 8U) * SCREEN_OLED_WIDTH + x);
    mask  = (uint8_t)(1U << (y % 8U));

    if (color != SCREEN_OLED_OFF)
    {
        s_fb[index] |= mask;
    }
    else
    {
        s_fb[index] &= (uint8_t)(~mask);
    }
}

/**
 * @brief 绘制单个 ASCII 字符
 * @param[in] x      左上角 X 坐标
 * @param[in] y      左上角 Y 坐标
 * @param[in] ch     待绘制字符
 * @param[in] invert 非 0 时反显
 * @return 无
 */
static void screen_oled_draw_char(uint16_t x, uint16_t y, char ch,
                                  uint8_t invert)
{
    const uint8_t *glyph = screen_font_ascii_glyph(ch);
    uint16_t row;
    uint16_t col;

    if (glyph == NULL)
    {
        /* 非 ASCII（例如 UTF-8 中文）：画空心方框占位，避免静默丢字 */
        screen_oled_fill_rect(x, y, SCREEN_FONT_WIDTH, SCREEN_FONT_HEIGHT,
                              (invert != 0U) ? SCREEN_OLED_ON : SCREEN_OLED_OFF);

        for (col = 0U; col < SCREEN_FONT_WIDTH; col++)
        {
            screen_oled_set_pixel((uint16_t)(x + col), y,
                                  (invert != 0U) ? SCREEN_OLED_OFF : SCREEN_OLED_ON);
            screen_oled_set_pixel((uint16_t)(x + col),
                                  (uint16_t)(y + SCREEN_FONT_HEIGHT - 1U),
                                  (invert != 0U) ? SCREEN_OLED_OFF : SCREEN_OLED_ON);
        }

        for (row = 0U; row < SCREEN_FONT_HEIGHT; row++)
        {
            screen_oled_set_pixel(x, (uint16_t)(y + row),
                                  (invert != 0U) ? SCREEN_OLED_OFF : SCREEN_OLED_ON);
            screen_oled_set_pixel((uint16_t)(x + SCREEN_FONT_WIDTH - 1U),
                                  (uint16_t)(y + row),
                                  (invert != 0U) ? SCREEN_OLED_OFF : SCREEN_OLED_ON);
        }
        return;
    }

    for (row = 0U; row < SCREEN_FONT_HEIGHT; row++)
    {
        uint8_t bits = glyph[row];

        for (col = 0U; col < SCREEN_FONT_WIDTH; col++)
        {
            uint8_t on = (uint8_t)((bits >> (7U - col)) & 0x01U);

            if (invert != 0U)
            {
                on = (uint8_t)((on == 0U) ? 1U : 0U);
            }

            screen_oled_set_pixel((uint16_t)(x + col), (uint16_t)(y + row), on);
        }
    }
}

/**
 * @brief 初始化 OLED
 * @return 0 成功，-1 失败
 */
int screen_oled_init(void)
{
    liot_lcd_config_t   cfg;
    LiotSleepModeCfg_t  sleep_cfg;
    liot_gpioerr_e      gpio_ret;

    if (s_lcd != NULL)
    {
        return 0;
    }

    gpio_ret = Liot_AonPowerCtl(true);
    if (gpio_ret != L_GPIO_ERR_SUCCESS)
    {
        liot_trace("[OLED] AON power on failed: %d", (int)gpio_ret);
        return -1;
    }

    gpio_ret = Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);
    if (gpio_ret != L_GPIO_ERR_SUCCESS)
    {
        liot_trace("[OLED] set voltage failed: %d", (int)gpio_ret);
        return -1;
    }

    /* GPIO25 为使能外设 LDO 的控制脚，必须输出高电平 */
    gpio_ret = Liot_GpioInit(L_GPIO_25, L_IO_OUTPUT, L_IO_HIGH, NULL);
    if (gpio_ret != L_GPIO_ERR_SUCCESS)
    {
        liot_trace("[OLED] LDO enable failed: %d", (int)gpio_ret);
        return -1;
    }

    sleep_cfg.mode = LIOT_SLEEP_MODE_NORMAL;
    if (Liot_SleepSetMode(&sleep_cfg) != LIOT_SLEEP_SUCCESS)
    {
        liot_trace("[OLED] sleep mode set failed");
        return -1;
    }

    /* 等 LDO 与屏内电荷泵稳定后再发初始化命令 */
    liot_rtos_task_sleep_ms(SCREEN_OLED_POWER_DELAY_MS);

    /* I2C1：SCL 模块引脚 66 复用功能 2，SDA 模块引脚 57 复用功能 3 */
    gpio_ret = Liot_SetPinFunc(SCREEN_OLED_SCL_PIN,
                               (liot_pinfunc_e)SCREEN_OLED_SCL_FUNC);
    if (gpio_ret != L_GPIO_ERR_SUCCESS)
    {
        liot_trace("[OLED] SCL pin mux failed: %d", (int)gpio_ret);
        return -1;
    }

    gpio_ret = Liot_SetPinFunc(SCREEN_OLED_SDA_PIN,
                               (liot_pinfunc_e)SCREEN_OLED_SDA_FUNC);
    if (gpio_ret != L_GPIO_ERR_SUCCESS)
    {
        liot_trace("[OLED] SDA pin mux failed: %d", (int)gpio_ret);
        return -1;
    }

    memset(&cfg, 0, sizeof(cfg));
    cfg.interface.type        = LIOT_LCD_INTERFACE_I2C;
    cfg.interface.i2c.num     = (liot_i2c_channel_e)SCREEN_OLED_I2C_NUM;
    cfg.interface.i2c.sda     = (int8_t)SCREEN_OLED_SDA_PIN;
    cfg.interface.i2c.scl     = (int8_t)SCREEN_OLED_SCL_PIN;
    cfg.interface.i2c.speed   = LIOT_STANDARD_MODE;
    cfg.interface.i2c.addr    = (uint8_t)SCREEN_OLED_I2C_ADDR;
    cfg.interface.i2c.cb      = NULL;
    cfg.interface.blk.type    = LIOT_LCD_NO_BACKLIGHT;
    cfg.interface.blk.pin     = -1;
    cfg.interface.rst.pin     = -1;
    cfg.interface.rst.delay   = 0;
    cfg.lcdDev                = &liot_ssd1306_12864_dev;

    s_lcd = liot_lcd_init(&cfg);
    if (s_lcd == NULL)
    {
        liot_trace("[OLED] lcd init failed");
        return -1;
    }

    if (screen_oled_clear() != 0)
    {
        return -1;
    }

    liot_trace("[OLED] init done (%ux%u, i2c%u addr=0x%02X)",
               (unsigned)SCREEN_OLED_WIDTH, (unsigned)SCREEN_OLED_HEIGHT,
               (unsigned)SCREEN_OLED_I2C_NUM, (unsigned)SCREEN_OLED_I2C_ADDR);
    return 0;
}

/**
 * @brief 清空帧缓冲
 * @return 0 成功，-1 未初始化
 */
int screen_oled_clear(void)
{
    if (s_lcd == NULL)
    {
        return -1;
    }

    memset(s_fb, 0, sizeof(s_fb));
    return 0;
}

/**
 * @brief 填充矩形
 * @param[in] x      左上角 X 坐标
 * @param[in] y      左上角 Y 坐标
 * @param[in] width  宽度
 * @param[in] height 高度
 * @param[in] color  SCREEN_OLED_ON 或 SCREEN_OLED_OFF
 * @return 0 成功，-1 未初始化
 */
int screen_oled_fill_rect(uint16_t x, uint16_t y, uint16_t width,
                          uint16_t height, uint8_t color)
{
    uint16_t col;
    uint16_t row;
    uint16_t x_end;
    uint16_t y_end;

    if (s_lcd == NULL)
    {
        return -1;
    }

    x_end = (uint16_t)(x + width);
    y_end = (uint16_t)(y + height);

    if (x_end > SCREEN_OLED_WIDTH)
    {
        x_end = SCREEN_OLED_WIDTH;
    }
    if (y_end > SCREEN_OLED_HEIGHT)
    {
        y_end = SCREEN_OLED_HEIGHT;
    }

    for (row = y; row < y_end; row++)
    {
        for (col = x; col < x_end; col++)
        {
            screen_oled_set_pixel(col, row, color);
        }
    }

    return 0;
}

/**
 * @brief 绘制 ASCII 字符串
 * @param[in] x      起始 X 坐标
 * @param[in] y      起始 Y 坐标
 * @param[in] text   字符串
 * @param[in] invert 非 0 时反显
 * @return 0 成功，-1 参数错误或未初始化
 */
int screen_oled_draw_text(uint16_t x, uint16_t y, const char *text,
                          uint8_t invert)
{
    if ((text == NULL) || (s_lcd == NULL))
    {
        return -1;
    }

    /* 不自动换行：超出屏幕右侧的字符直接截断 */
    while ((*text != '\0') &&
           ((uint16_t)(x + SCREEN_FONT_WIDTH) <= SCREEN_OLED_WIDTH))
    {
        screen_oled_draw_char(x, y, *text, invert);
        x = (uint16_t)(x + SCREEN_FONT_WIDTH);
        text++;
    }

    return 0;
}

/**
 * @brief 把帧缓冲写到屏幕
 * @return 0 成功，-1 未初始化或驱动错误
 */
int screen_oled_flush(void)
{
    liot_lcd_errcode_e ret;

    if (s_lcd == NULL)
    {
        return -1;
    }

    /* 坐标为闭区间，整屏即 (0,0)-(127,63) */
    ret = liot_lcd_write(s_lcd, 0U, 0U,
                         (uint16_t)(SCREEN_OLED_WIDTH - 1U),
                         (uint16_t)(SCREEN_OLED_HEIGHT - 1U),
                         s_fb);
    if (ret != LIOT_LCD_OK)
    {
        liot_trace("[OLED] write failed: %d", (int)ret);
        return -1;
    }

    ret = liot_lcd_refresh(s_lcd);
    if (ret != LIOT_LCD_OK)
    {
        liot_trace("[OLED] refresh failed: %d", (int)ret);
        return -1;
    }

    return 0;
}
