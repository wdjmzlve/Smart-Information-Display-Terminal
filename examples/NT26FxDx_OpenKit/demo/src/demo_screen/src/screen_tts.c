/**
 * @file screen_tts.c
 * @brief TTS 播报任务实现
 *
 * 流程：
 *   1. 任务启动时初始化 ES8311、TTS 资源与引擎；
 *   2. 收到播报请求后调用 liot_tts_start()（同步）；
 *   3. 用音频 FINISH 事件 + 静默窗口判断播放结束；
 *   4. 通过回调把结果交给应用主任务去回执。
 *
 * @copyright Copyright (c) 2025 Lierda Technology Co., Ltd.
 * @date 2025-11-26
 * @version 1.0
 */

#include <string.h>

#include "liot_audio2.h"
#include "liot_flash.h"
#include "liot_gpio2.h"
#include "liot_log.h"
#include "liot_os.h"
#include "liot_tts.h"
#include "mem_map.h"

#include "screen_config.h"
#include "screen_tts.h"

/**
 * @brief 播报请求
 */
typedef struct
{
    char text[SCREEN_MSG_CONTENT_MAX_LEN];  /*!< 待播报文本 */
} screen_tts_msg_t;

/**
 * @brief TTS 任务上下文
 */
typedef struct
{
    liot_task_t  task;              /*!< TTS 任务句柄 */
    liot_queue_t queue;             /*!< 播报请求队列 */

    volatile uint8_t  busy;         /*!< 1=正在播报 */
    volatile uint8_t  stop_requested;   /*!< 1=请求打断 */
    volatile uint8_t  play_error;   /*!< 播放接口返回过错误 */
    volatile uint32_t finish_tick;  /*!< 最近一次播放完成事件的 tick */
    volatile uint32_t last_play_tick;   /*!< 最近一次提交音频数据的 tick */
    volatile uint32_t output_count; /*!< TTS 回调产出音频块的次数 */

    uint8_t hw_ready;               /*!< 1=音频与 TTS 初始化成功 */
    screen_tts_done_cb_t done_cb;   /*!< 播报结束回调 */
} screen_tts_ctx_t;

static screen_tts_ctx_t s_tts_ctx;

/**
 * @brief 读取 TTS 资源
 * @param[in]  pParameter 资源起始地址
 * @param[out] pBuffer    输出缓冲区
 * @param[in]  iPos       相对偏移
 * @param[in]  nSize      读取长度
 * @return 恒为 true
 */
static bool screen_tts_read_res_cb(void *pParameter, void *pBuffer,
                                   uint32_t iPos, uint32_t nSize)
{
    liot_flash_read((uint8_t *)pBuffer,
                    (uint32_t)((uint8_t *)pParameter + iPos), nSize);
    return true;
}

/**
 * @brief TTS 音频输出回调
 * @param[in] context 未使用
 * @param[in] msg     预留
 * @param[in] ds      预留
 * @param[in] param2  预留
 * @param[in] dSize   音频数据长度
 * @param[in] dBuffer 音频数据
 * @return 恒为 0
 */
static int screen_tts_output_cb(void *context, int msg, int ds, int param2,
                                int dSize, const void *dBuffer)
{
    Liot_AudErr_e ret;

    (void)context;
    (void)msg;
    (void)ds;
    (void)param2;

    s_tts_ctx.last_play_tick = liot_rtos_get_system_tick();
    s_tts_ctx.output_count++;

    if (s_tts_ctx.output_count == 1U)
    {
        liot_trace("[TTS] first pcm %d bytes", dSize);
    }

    ret = Liot_AudioPlay((uint8_t *)dBuffer, dSize);
    if (ret != L_AUD_ERR_SUCCESS)
    {
        if (s_tts_ctx.play_error == 0U)
        {
            liot_trace("[TTS] audio play failed ret=%d", (int)ret);
        }
        s_tts_ctx.play_error = 1U;
    }

    return 0;
}

/**
 * @brief 音频事件回调
 * @param[in] event   事件类型
 * @param[in] context 未使用
 * @return 无
 */
static void screen_tts_audio_evt_cb(Liot_AudEvent_e event, void *context)
{
    (void)context;

    if (event == L_AUD_EVT_FINISH)
    {
        s_tts_ctx.finish_tick = liot_rtos_get_system_tick();
    }
    else if (event == L_AUD_EVT_ERROR)
    {
        liot_trace("[TTS] audio error event");
        s_tts_ctx.play_error = 1U;
    }
}

/**
 * @brief 初始化音频硬件与 TTS 引擎
 * @return 0 成功，-1 失败
 */
static int screen_tts_hw_init(void)
{
    Liot_AudHwConfig_t cfg;
    Liot_AudErr_e      aud_ret;
    uint8_t            res_head[4] = {0};

    /* 等 LDO 与 ES8311 上电稳定，与官方音频 demo 保持一致 */
    liot_rtos_task_sleep_ms(SCREEN_TTS_POWER_ON_DELAY_MS);
    liot_trace("[TTS] audio init begin");

    if (Liot_AonPowerCtl(true) != L_GPIO_ERR_SUCCESS)
    {
        liot_trace("[TTS] AON power on failed");
        return -1;
    }

    if (Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V) != L_GPIO_ERR_SUCCESS)
    {
        liot_trace("[TTS] set voltage failed");
        return -1;
    }

    memset(&cfg, 0, sizeof(cfg));
    cfg.i2cNum     = (int8_t)SCREEN_TTS_AUDIO_I2C_NUM;
    cfg.i2sNum     = (int8_t)SCREEN_TTS_AUDIO_I2S_NUM;
    cfg.paGpioNum  = (int8_t)SCREEN_TTS_AUDIO_PA_GPIO;
    cfg.codecType  = L_AUD_ES8311;
    cfg.channel    = L_AUD_MONO_RIGHT;
    cfg.role       = L_AUD_ROLE_SLAVE;
    cfg.mode       = L_AUD_MODE_I2S;
    cfg.frameSize  = L_AUD_FRAMESIZE_16_16;
    cfg.samples    = L_AUD_16K_SAMPLES;
    cfg.callback   = screen_tts_audio_evt_cb;
    cfg.cbContext  = NULL;
    cfg.use3A      = 0U;

    aud_ret = Liot_AudioInit(&cfg);
    if (aud_ret != L_AUD_ERR_SUCCESS)
    {
        liot_trace("[TTS] audio init failed ret=%d", (int)aud_ret);
        return -1;
    }
    liot_trace("[TTS] audio init ok (ES8311 16k mono)");

    /* 软件音量与 ES8311 硬件音量都要设置，任何一个为 0 都会静音 */
    aud_ret = Liot_AudioSetVolume(SCREEN_TTS_VOLUME);
    if (aud_ret != L_AUD_ERR_SUCCESS)
    {
        liot_trace("[TTS] set sw volume failed ret=%d", (int)aud_ret);
    }

    aud_ret = Liot_AudioSetCodecVolume(SCREEN_TTS_CODEC_VOLUME);
    if (aud_ret != L_AUD_ERR_SUCCESS)
    {
        liot_trace("[TTS] set codec volume failed ret=%d", (int)aud_ret);
    }
    liot_trace("[TTS] volume sw=%d codec=%d", SCREEN_TTS_VOLUME,
               SCREEN_TTS_CODEC_VOLUME);

    /* 资源自检：全 FF 说明烧录包里没有 TTS 资源（常见于只烧了 app 分区） */
    liot_flash_read(res_head, (uint32_t)PKGFLXTTS_RES_ADDR, sizeof(res_head));
    liot_trace("[TTS] res addr=0x%X size=%u head=%02X%02X%02X%02X",
               (unsigned)PKGFLXTTS_RES_ADDR, (unsigned)PKGFLXTTS_RES_SIZE,
               res_head[0], res_head[1], res_head[2], res_head[3]);

    liot_tts_set_resource((void *)PKGFLXTTS_RES_ADDR, screen_tts_read_res_cb);

    if (liot_tts_engine_init(screen_tts_output_cb) != LIOT_TTS_SUCCESS)
    {
        liot_trace("[TTS] engine init failed");
        return -1;
    }

    /* cJSON 输出的是 UTF-8，TTS 必须按 UTF-8 解析 */
    if (liot_tts_set_config_param(LIOT_TTS_CONFIG_ENCODING,
                                  LIOT_TTS_UTF8) != LIOT_TTS_SUCCESS)
    {
        liot_trace("[TTS] set encoding failed");
    }

    return 0;
}

/**
 * @brief 等待本次播报播放完成
 * @return 1 正常结束，0 被打断、没有音频或超时
 */
static uint8_t screen_tts_wait_finish(void)
{
    uint32_t      waited_s = 0U;
    Liot_AudErr_e ret;

    if (s_tts_ctx.output_count == 0U)
    {
        /* TTS 一个字节都没产出，通常是资源缺失或引擎异常 */
        liot_trace("[TTS] no pcm produced");
        return 0U;
    }

    while (waited_s < SCREEN_TTS_MAX_WAIT_S)
    {
        if (s_tts_ctx.stop_requested != 0U)
        {
            (void)Liot_AudioStop();
            return 0U;
        }

        /* 每次只等 1 秒，便于新消息快速打断 */
        ret = Liot_AudioWaitPlayFinish(1);
        if (ret == L_AUD_ERR_SUCCESS)
        {
            return (s_tts_ctx.play_error == 0U) ? 1U : 0U;
        }

        waited_s++;
    }

    liot_trace("[TTS] wait finish timeout, finish=%u",
               (unsigned)s_tts_ctx.finish_tick);
    (void)Liot_AudioStop();
    return 0U;
}

/**
 * @brief TTS 任务主体
 * @param[in] arg 未使用
 * @return 无（正常情况下不返回）
 */
static void screen_tts_task(void *arg)
{
    screen_tts_msg_t msg;
    uint8_t          ok;

    (void)arg;

    /* 任务启动即打印，便于区分“任务没起来”和“卡在音频初始化里” */
    liot_trace("[TTS] task start");

    if (screen_tts_hw_init() == 0)
    {
        s_tts_ctx.hw_ready = 1U;
        liot_trace("[TTS] ready");
    }
    else
    {
        s_tts_ctx.hw_ready = 0U;
        liot_trace("[TTS] init failed, TTS disabled");
    }

    while (1)
    {
        if (liot_rtos_queue_wait(s_tts_ctx.queue, (uint8 *)&msg,
                                 (uint32)sizeof(msg),
                                 LIOT_WAIT_FOREVER) != LIOT_OSI_SUCCESS)
        {
            continue;
        }

        if (s_tts_ctx.hw_ready == 0U)
        {
            if (s_tts_ctx.done_cb != NULL)
            {
                s_tts_ctx.done_cb(0U);
            }
            continue;
        }

        if (msg.text[0] == '\0')
        {
            if (s_tts_ctx.done_cb != NULL)
            {
                s_tts_ctx.done_cb(1U);  /* 空文本无需播报，按成功处理 */
            }
            continue;
        }

        s_tts_ctx.stop_requested = 0U;
        s_tts_ctx.play_error = 0U;
        s_tts_ctx.finish_tick = 0U;
        s_tts_ctx.last_play_tick = 0U;
        s_tts_ctx.output_count = 0U;
        s_tts_ctx.busy = 1U;

        liot_trace("[TTS] speak len=%u", (unsigned)strlen(msg.text));

        if (liot_tts_start(msg.text, (unsigned int)strlen(msg.text)) !=
            LIOT_TTS_SUCCESS)
        {
            liot_trace("[TTS] start failed");
            ok = 0U;
        }
        else
        {
            ok = screen_tts_wait_finish();
        }

        s_tts_ctx.busy = 0U;

        /* 被打断时应用层已按 displayed 回执，这里不再回调 done */
        if ((s_tts_ctx.stop_requested == 0U) && (s_tts_ctx.done_cb != NULL))
        {
            s_tts_ctx.done_cb(ok);
        }
    }
}

/**
 * @brief 启动 TTS 任务
 * @return 0 成功，-1 失败
 */
int screen_tts_init(void)
{
    LiotOSStatus_t ret;

    memset(&s_tts_ctx, 0, sizeof(s_tts_ctx));

    ret = liot_rtos_queue_create(&s_tts_ctx.queue, (uint32)sizeof(screen_tts_msg_t),
                                 SCREEN_TTS_QUEUE_DEPTH);
    if (ret != LIOT_OSI_SUCCESS)
    {
        liot_trace("[TTS] queue create failed: %d", (int)ret);
        return -1;
    }

    ret = liot_rtos_task_create(&s_tts_ctx.task, SCREEN_TTS_TASK_STACK,
                                LIOT_APP_TASK_PRIORITY,
                                (char *)SCREEN_TTS_TASK_NAME,
                                screen_tts_task, NULL);
    if (ret != LIOT_OSI_SUCCESS)
    {
        liot_trace("[TTS] task create failed: %d", (int)ret);
        return -1;
    }

    return 0;
}

/**
 * @brief 请求播报
 * @param[in] text UTF-8 文本
 * @return 0 已入队，-1 失败
 */
int screen_tts_speak(const char *text)
{
    screen_tts_msg_t msg;

    if ((text == NULL) || (s_tts_ctx.queue == NULL))
    {
        return -1;
    }

    memset(&msg, 0, sizeof(msg));
    (void)strncpy(msg.text, text, sizeof(msg.text) - 1U);

    if (liot_rtos_queue_release(s_tts_ctx.queue, (uint32)sizeof(msg),
                                (uint8 *)&msg, 0) != LIOT_OSI_SUCCESS)
    {
        liot_trace("[TTS] queue full");
        return -1;
    }

    return 0;
}

/**
 * @brief 请求停止播报
 * @return 无
 */
void screen_tts_stop(void)
{
    s_tts_ctx.stop_requested = 1U;
}

/**
 * @brief 查询是否正在播报
 * @return 1 正在播报，0 空闲
 */
uint8_t screen_tts_is_busy(void)
{
    return s_tts_ctx.busy;
}

/**
 * @brief 注册播报结束回调
 * @param[in] cb 回调函数
 * @return 无
 */
void screen_tts_set_done_cb(screen_tts_done_cb_t cb)
{
    s_tts_ctx.done_cb = cb;
}
