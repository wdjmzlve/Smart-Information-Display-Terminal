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
#define SCREEN_NET_REG_TIMEOUT_S        (20U)       /* 单次等待注网超时，拔卡后能快速返回 */
#define SCREEN_NET_REG_RETRY_MAX        (3U)        /* 单轮注网重试次数，失败后回到 RTRY 再重来 */
#define SCREEN_NET_RETRY_DELAY_MS       (1000U)     /* 注网重试间隔 */
#define SCREEN_NET_RECOVERY_PERIOD_MS   (60000U)    /* 长时间注不上网时，两次模组射频复位的最小间隔 */

/* ------------------------------ 下行消息 ------------------------------ */
#define SCREEN_DOWNLINK_MAX_LEN         (512U)      /* 单条 MQTT 下行载荷上限 */
#define SCREEN_MSG_ID_MAX_LEN           (48U)       /* msgId 缓冲区 */
#define SCREEN_MSG_TITLE_MAX_LEN        (64U)       /* 标题缓冲区 */
#define SCREEN_MSG_CONTENT_MAX_LEN      (256U)      /* 正文缓冲区 */
#define SCREEN_MSG_LEVEL_MIN            (1)         /* 消息等级下限：普通 */
#define SCREEN_MSG_LEVEL_MAX            (3)         /* 消息等级上限：紧急 */
#define SCREEN_MSG_TTS_OFF              (0)         /* 不播报 */
#define SCREEN_MSG_TTS_ON               (1)         /* 需要播报 */

/* ------------------------------ 回执 ------------------------------ */
#define SCREEN_ACK_STATUS_MAX_LEN       (12U)       /* displayed/done/restored/failed */
#define SCREEN_ACK_ERR_MAX_LEN          (40U)       /* 失败原因缓冲区 */

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
#define SCREEN_UI_LINE_HEIGHT       (16U)           /* 内容行高 */
#define SCREEN_UI_STATUS_Y          (48U)           /* 状态栏 Y 坐标 */

/* ------------------------------ 文本排版 ------------------------------ */
#define SCREEN_LINE_MAX_BYTES       (32U)           /* 单行缓冲：8 个汉字约 24 字节 */
#define SCREEN_MAX_LINES            (16U)           /* 最多保留 16 行，避免长文无限占用 */
#define SCREEN_LINES_PER_PAGE       (2U)            /* 内容区每页 2 行 */
#define SCREEN_PAGE_INTERVAL_MS     (4000U)         /* 多页时自动翻页间隔 */

/* ------------------------------ TTS ------------------------------ */
#define SCREEN_TTS_TASK_STACK       (10240U)        /* TTS 任务栈，合成 + 播放空间较大 */
#define SCREEN_TTS_TASK_NAME        "screen_tts"
#define SCREEN_TTS_QUEUE_DEPTH      (2U)            /* 播报请求队列深度 */
#define SCREEN_TTS_VOLUME           (60)            /* 软件播放音量 0-100 */
#define SCREEN_TTS_CODEC_VOLUME     (60)            /* ES8311 硬件音量 0-100 */
#define SCREEN_TTS_POWER_ON_DELAY_MS (2000U)        /* 上电到音频初始化的等待时间 */
#define SCREEN_TTS_MAX_WAIT_S       (30U)           /* 单次播报等待播放完成的最长秒数 */
#define SCREEN_TTS_AUDIO_I2C_NUM    (0)             /* ES8311 挂在 I2C0 */
#define SCREEN_TTS_AUDIO_I2S_NUM    (0)             /* 音频走 I2S0 */
#define SCREEN_TTS_AUDIO_PA_GPIO    (11)            /* 功放使能 GPIO */
#define SCREEN_TTS_PRESILENCE_MS    (200U)          /* 起播前的静音预热时长，避免首字被吞 */
#define SCREEN_TTS_BYTES_PER_MS     (32U)           /* 16 kHz × 16 bit × 单声道 = 32 字节/ms */
#define SCREEN_TTS_PRESILENCE_BYTES (SCREEN_TTS_PRESILENCE_MS * SCREEN_TTS_BYTES_PER_MS)
#define SCREEN_TTS_STOP_SETTLE_MS   (50U)           /* 停止播放后等待音频通路稳定的时间 */

/* ------------------------------ 三色灯 WS2812B ------------------------------ */
#define SCREEN_LED_COUNT            (3U)            /* 板载 WS2812B 灯珠数量 */
#define SCREEN_LED_SPI_PORT         (0U)            /* 三色灯挂在 SPI0 */
#define SCREEN_LED_SPI_CLK_HZ       (5000000U)      /* 与官方 demo_ws2812b.c 一致，1 个 SPI 字节表示 1 位 */
#define SCREEN_LED_BRIGHTNESS       (128U)          /* 全局亮度 0-255，兼顾白天可见与夜间不刺眼 */
#define SCREEN_LED_BLINK_INTERVAL_MS (500U)         /* 紧急等级红闪的半周期 */
#define SCREEN_LED_RGB_NORMAL_R     (0U)            /* 普通：绿 */
#define SCREEN_LED_RGB_NORMAL_G     (160U)
#define SCREEN_LED_RGB_NORMAL_B     (0U)
#define SCREEN_LED_RGB_NOTICE_R     (0U)            /* 提醒：蓝 */
#define SCREEN_LED_RGB_NOTICE_G     (0U)
#define SCREEN_LED_RGB_NOTICE_B     (200U)
#define SCREEN_LED_RGB_URGENT_R     (220U)          /* 紧急：红 */
#define SCREEN_LED_RGB_URGENT_G     (0U)
#define SCREEN_LED_RGB_URGENT_B     (0U)
/* SPI0 引脚不在代码里设置：由 iodriver.ini 生成，SPI 驱动初始化时统一配置 */

/* ------------------------------ 时间换算 ------------------------------ */
#define SCREEN_SECONDS_PER_MINUTE   (60U)           /* 每小时秒数换算系数 */
#define SCREEN_MINUTES_PER_HOUR     (60U)           /* 每小时分钟数 */
#define SCREEN_HOURS_PER_DAY        (24U)           /* 每天小时数 */

#endif /* SCREEN_CONFIG_H */
