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
#define SCREEN_STUDENT_NO           "23040722"
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

/* ------------------------------ MQTT 运行参数 ------------------------------ */
#define SCREEN_MQTT_QOS                 (1U)        /* 订阅与发布统一使用 QoS1 */
#define SCREEN_MQTT_KEEPALIVE_S         (30U)       /* MQTT 协议 keepalive */
#define SCREEN_MQTT_DELIVERY_TIME_S     (5U)        /* 重传间隔 */
#define SCREEN_MQTT_DELIVERY_CNT        (3U)        /* 重传次数 */
#define SCREEN_MQTT_PING_TIMEOUT_S      (5U)        /* ping 超时 */
#define SCREEN_MQTT_TASK_STACK          (8192U)     /* 网络任务栈大小 */
#define SCREEN_MQTT_TASK_NAME           "screen_mqtt"
#define SCREEN_MQTT_QUEUE_DEPTH         (8U)        /* 心跳事件队列深度 */
#define SCREEN_MQTT_CONNECT_TIMEOUT_MS  (30000U)    /* 连接 broker 超时 */
#define SCREEN_MQTT_OP_TIMEOUT_MS       (10000U)    /* 发布/订阅超时 */
#define SCREEN_MQTT_POLL_MS             (5000U)     /* 在线状态轮询周期 */
#define SCREEN_MQTT_RETRY_DELAY_MS      (5000U)     /* 重连前等待 */
#define SCREEN_MQTT_DATACALL_DELAY_MS   (5000U)     /* 拨号后等待 PDP 就绪 */
#define SCREEN_HEARTBEAT_PERIOD_MS      (30000U)    /* 应用层心跳周期 */

/* ------------------------------ 信号强度换算 ------------------------------ */
#define SCREEN_CSQ_VALID_MAX        (31)    /* CSQ 原始值上限，99 表示无效 */
#define SCREEN_CSQ_TO_DBM(csq)      (2 * (int)(csq) - 113)  /* CSQ 0-31 转 dBm */
#define SCREEN_RSRP_VALID_MAX       (97)    /* RSRP 原始值上限，255 表示无效 */
#define SCREEN_RSRP_OFFSET_DBM      (141)   /* RSRP dBm = 原始值 - 141 */

/* ------------------------------ 网络参数 ------------------------------ */
#define SCREEN_DATA_CALL_CID            (1)         /* PDP 上下文编号 */
#define SCREEN_SIM_ID                   (0U)        /* SIM 卡序号 */
#define SCREEN_NET_REG_TIMEOUT_S        (120U)      /* 单次等待注网超时 */
#define SCREEN_NET_REG_RETRY_MAX        (10U)       /* 注网重试次数 */
#define SCREEN_NET_RETRY_DELAY_MS       (1000U)     /* 注网重试间隔 */

/* ------------------------------ 协议缓冲区 ------------------------------ */
#define SCREEN_TOPIC_MAX_LEN            (64U)       /* topic 缓冲区 */
#define SCREEN_URL_MAX_LEN              (64U)       /* mqtt://host:port 缓冲区 */
#define SCREEN_JSON_MAX_LEN             (256U)      /* JSON 缓冲区 */
#define SCREEN_FW_VER_FALLBACK          "appV01"    /* 取版本失败时的兜底值 */

#define SCREEN_TOPIC_SUB_FMT            "screen/%s/msg"    /* 下行屏幕消息 */
#define SCREEN_TOPIC_ACK_FMT            "screen/%s/ack"    /* 上行回执 */
#define SCREEN_TOPIC_REG_FMT            "device/%s/up"     /* 上行 JSON */
#define SCREEN_TOPIC_HB_FMT             "device/%s/upb"    /* 上行二进制心跳 */

/* ------------------------------ 界面布局 ------------------------------ */
#define SCREEN_UI_BAR_HEIGHT        (16U)           /* 标题栏/状态栏高度 */
#define SCREEN_UI_TITLE_Y           (0U)            /* 标题栏 Y 坐标 */
#define SCREEN_UI_LINE1_Y           (16U)           /* 内容第一行 Y 坐标 */
#define SCREEN_UI_LINE2_Y           (32U)           /* 内容第二行 Y 坐标 */
#define SCREEN_UI_STATUS_Y          (48U)           /* 状态栏 Y 坐标 */

#endif /* SCREEN_CONFIG_H */
