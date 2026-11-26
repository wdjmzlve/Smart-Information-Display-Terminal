/**
 * @file screen_config.h
 * @brief 云播报屏（项目10）编译期配置
 *
 * 所有可调参数集中在本文件，业务代码禁止出现魔法数字。
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-11-25
 * @version 1.0
 */

#ifndef SCREEN_CONFIG_H
#define SCREEN_CONFIG_H

/* ------------------------------ 服务器与账号 ------------------------------ */
/* 课程统一提供的 mosquitto broker，账号全班共用 */
#define SCREEN_SERVER_HOST          "49.235.147.4"  /* MQTT 服务器地址 */
#define SCREEN_SERVER_PORT          (1883)          /* MQTT 明文端口 */
#define SCREEN_MQTT_USER            "openkit"       /* broker 用户名 */
#define SCREEN_MQTT_PASS            "openkit2026"   /* broker 密码 */

/* ------------------------------ 学生身份 ------------------------------ */
/* 改成自己的学号：同时作为 MQTT clientId，必须全局唯一 */
#define SCREEN_STUDENT_NO           "2025001"
#define SCREEN_PROJECT_NO           (10)            /* 项目编号，dev.reg 上报用 */

/* ------------------------------ RTOS 资源 ------------------------------ */
#define SCREEN_EVENT_QUEUE_DEPTH    (8U)            /* 事件队列可缓存的消息条数 */

/* ------------------------------ 周期参数 ------------------------------ */
#define SCREEN_TICK_PERIOD_MS       (1000U)         /* 骨架阶段 tick 打印周期，单位 ms */

/* ------------------------------ 缓冲区长度 ------------------------------ */
#define SCREEN_IMEI_LEN             (16U)           /* 15 位 IMEI + 结束符 */
#define SCREEN_FW_VER_LEN           (32U)           /* 固件版本字符串缓冲区 */
#define SCREEN_STATUS_TEXT_LEN      (20U)           /* 状态栏字符串缓冲区 */

/* ------------------------------ OLED 硬件 ------------------------------ */
#define SCREEN_OLED_I2C_NUM         (1)             /* OLED 挂在 I2C1 */
#define SCREEN_OLED_I2C_ADDR        (0x3C)          /* SSD1306 7 位从地址 */
#define SCREEN_OLED_SCL_PIN         (66)            /* 模块引脚 SCL */
#define SCREEN_OLED_SDA_PIN         (57)            /* 模块引脚 SDA */
#define SCREEN_OLED_SCL_FUNC        (2U)            /* 复用功能 2，对应 L_PIN_FUNC_2 */
#define SCREEN_OLED_SDA_FUNC        (3U)            /* 复用功能 3，对应 L_PIN_FUNC_3 */
#define SCREEN_OLED_POWER_DELAY_MS  (300U)          /* LDO 稳定到发初始化命令的等待时间 */

/* ------------------------------ 界面布局 ------------------------------ */
#define SCREEN_UI_BAR_HEIGHT        (16U)           /* 标题栏/状态栏高度 */
#define SCREEN_UI_TITLE_Y           (0U)            /* 标题栏 Y 坐标 */
#define SCREEN_UI_LINE1_Y           (16U)           /* 内容第一行 Y 坐标 */
#define SCREEN_UI_LINE2_Y           (32U)           /* 内容第二行 Y 坐标 */
#define SCREEN_UI_STATUS_Y          (48U)           /* 状态栏 Y 坐标 */

#endif /* SCREEN_CONFIG_H */
