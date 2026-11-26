# 智能信息发布终端（云播报屏）

> **本项目基于 Lierda（利尔达）Cat.1 Bis OpenCPU SDK 二次开发**，实现课程实战项目 10
> 「智能信息发布终端（云播报屏）」：设备通过 MQTT 接收服务器按 IMEI 点对点下发的屏显消息，
> 完成分屏显示、TTS 播报、等级灯效与回执，并保证断线重连、掉电恢复与重复消息幂等。
>
> - 上游 SDK 原始文档：**[README_LIERDA.md](README_LIERDA.md)**、**[README_LIERDA_ZH.md](README_LIERDA_ZH.md)**
>   （原 `README.md` / `README_ZH.md`，仅重命名，内容未做任何修改）
> - 本项目**只新增** `examples/NT26FxDx_OpenKit/demo/src/demo_screen/` 演示工程，
>   **未修改** `components/`、`rules/`、`tools/` 下的 SDK 内核、底包与工具链
> - 硬件平台：OpenKit Lite（NT26FxDx / EC718，Cat.1 bis）

---

## 1. 已实现功能

| 功能 | 说明 |
|---|---|
| 设备注册 | 连接自建 broker 后上报 `dev.reg`（学号、项目号、固件版本） |
| 周期心跳 | 每 30 s 上报二进制 `0xAA` 帧：RSSI(dBm)、运行时长、发送数、成功数、XOR 校验 |
| 点对点下发 | 订阅 `screen/{imei}/msg`，QoS 1；支持短 JSON 与 `{envelope, data:{...}}` 完整信封两种格式 |
| 断线重连 | 监听 MQTT 状态，断线后重新拨号、连接、订阅并补发注册 |
| 消息去重 | 按 `msgId` 幂等：重复消息不重绘、不重播，但仍回执让服务器停止重发 |
| 分屏显示 | 标题栏 / 正文两行 / 状态栏三区；正文按 128 px 自动折行 |
| 自动分页 | 超过一页时每 4 s 翻页，状态栏显示 `P n/m` |
| 状态栏 | `HH:MM` + 网络状态（WAIT/CONN/ONLN/RTRY/ERR/OFF）+ 页码，播报期间显示 `SPEAK` |
| 回执 | `screen/{imei}/ack`，QoS 1，状态分 `displayed` / `done` / `restored` / `failed` |
| 掉电恢复 | 最后一条消息写入 NVM，上电重绘，联网后补发 `restored` |
| TTS 播报 | ES8311 + I2S0，16 kHz 单声道；播报正文，完成后回执 `done`；新消息可打断旧播报 |
| 等级灯效 | WS2812B ×3：`level=1` 绿色常亮、`level=2` 蓝色常亮、`level=3` 红色闪烁 |
| 中文显示 | GB2312 一级 3755 个汉字的 16×16 点阵，脚本自动生成 |

## 2. 硬件与外设

| 外设 | 接口 | 引脚 / 地址 | 备注 |
|---|---|---|---|
| OLED SSD1306 128×64 | I2C1 | 0x3C；SCL=模组脚 66(func2)，SDA=脚 57(func3) | 无背光、无复位脚，需自建帧缓冲 |
| 三色灯 WS2812B ×3 | SPI0 | SCLK=86、SSN=83、MOSI=85、MISO=84（复用功能 1） | 5 MHz，1 个 SPI 字节表示 1 位；需 GPIO25 拉高使能 LDO |
| 音频 ES8311 | I2C0 + I2S0 | I2C0=38/39；PA 使能 GPIO11 | 16 kHz、单声道右声道、16 bit |
| 按键 KEY0/1/2 | GPIO | 20 / 22 / WAKEUPAD_0 | 本项目暂未使用 |

> SPI0 显式指定到 85/83/84/86，是为了避开 I2C1 的引脚 66；配置见
> [iodriver.ini](examples/NT26FxDx_OpenKit/iodriver.ini) 的 `[spi]` 段。

## 3. 工程结构

```text
CAT1.bis_OpenCPU/
├── README.md                    本文件（项目说明）
├── README_LIERDA.md             上游 SDK 说明（英文，原 README.md）
├── README_LIERDA_ZH.md          上游 SDK 说明（中文，原 README_ZH.md）
├── components/  rules/  tools/  SDK 内核、底包、工具链（未改动）
└── examples/NT26FxDx_OpenKit/
    ├── config                   构建开关：DEMO_NAME、TTS 资源、代码区调整
    ├── iodriver.ini             引脚复用表（I2C / SPI / I2S …）
    ├── Makefile                 工程构建入口，新增 demo_screen 分支
    └── demo/src/demo_screen/
        ├── inc/
        │   ├── screen_config.h  全部可调参数：服务器、学号、引脚、音量、周期
        │   ├── screen_types.h   事件类型与事件体（跨任务通信）
        │   ├── screen_app.h     应用入口与事件投递
        │   ├── screen_proto.h   JSON 组包/解析 + 0xAA 心跳帧
        │   ├── screen_mqtt.h    MQTT 连接、订阅、发布、重连
        │   ├── screen_oled.h    SSD1306 帧缓冲与绘制
        │   ├── screen_font.h    字模接口（ASCII 8×16 + 汉字 16×16）
        │   ├── screen_text.h    UTF-8 解码与按像素宽度折行
        │   ├── screen_tts.h     TTS 播报接口
        │   ├── screen_store.h   最后一条消息的掉电存储
        │   └── screen_led.h     三色灯接口
        ├── src/
        │   ├── user_main.c      程序入口
        │   ├── screen_app.c     主任务：状态机、渲染、回执、播报调度
        │   ├── screen_mqtt.c    网络任务：拨号、连接、注册、心跳、重连
        │   ├── screen_proto.c   协议编解码
        │   ├── screen_oled.c    OLED 驱动封装与图元绘制
        │   ├── screen_font.c    ASCII 点阵 + 汉字二分查表
        │   ├── screen_font_cjk.c 汉字点阵表（自动生成，勿手改）
        │   ├── screen_text.c    排版与折行
        │   ├── screen_tts.c     播报任务（TTS 引擎 + 音频播放）
        │   ├── screen_store.c   NVM 持久化
        │   └── screen_led.c     WS2812B 灯效
        └── tools/
            └── gen_cjk_font.ps1 汉字点阵生成脚本
```

### 任务与队列

| 任务 | 栈 | 职责 |
|---|---|---|
| app 主任务 | 10 KB | 唯一状态拥有者：解析、状态机、渲染、回执、持久化 |
| mqtt 任务 | 8 KB | 拨号、连接、订阅、发布、心跳、重连 |
| tts 任务 | 10 KB | 音频初始化与同步播报（`liot_tts_start` 阻塞） |
| 1 s 定时器 | — | 只向队列投递 tick：刷新状态栏、翻页、闪烁 |

MQTT 回调与定时器回调只做数据拷贝/入队，不做解析、不刷屏、不播报。

## 4. 通信协议

| 主题 | 方向 | 说明 |
|---|---|---|
| `screen/{imei}/msg` | 下行 | 屏幕消息（JSON） |
| `screen/{imei}/ack` | 上行 | 回执（JSON） |
| `device/{imei}/up` | 上行 | 注册等 JSON 报文 |
| `device/{imei}/upb` | 上行 | 0xAA 二进制心跳 |

下行消息示例：

```json
{"type":"screen.msg","msgId":"M-0001","title":"通知","content":"今天下午三点开会",
 "level":2,"tts":1,"ts":1764150000}
```

回执示例：

```json
{"type":"screen.ack","msgId":"M-0001","status":"displayed","shown":true}
```

心跳帧（9 + N 字节）：`0xAA | IMEI 倒数第2位 | IMEI 最后1位 | 0x01 | seq | len | RSSI(int8) | uptime(4) | sent(2) | ok(2) | RFU | XOR | 0x0F`。

## 5. 构建与烧录

```powershell
build.bat all PROJECT=NT26FxDx_OpenKit MODEM=NT26F6D0 MODEMPKG=F6D_A
```

USB boot 模式下整包烧录：

```powershell
make burn pkg=gccout/NT26FxDx_OpenKit/NT26FxDx_OpenKit.binpkg type=full mode=usb
```

**注意：改动 flash 分区、TTS 资源或字库后必须整包烧录。** TTS 资源不在 app 分区里，
只烧 `type=app` 会得到擦除态（串口打印 `head=FFFFFFFF`，TTS 被禁用）。

## 6. 关键配置

全部集中在 [screen_config.h](examples/NT26FxDx_OpenKit/demo/src/demo_screen/inc/screen_config.h)：

| 宏 | 默认值 | 说明 |
|---|---|---|
| `SCREEN_STUDENT_NO` | `23040722` | 学号，同时作为 MQTT clientId，必须全局唯一 |
| `SCREEN_SERVER_HOST` / `PORT` | `49.235.147.4` / `1883` | 课程 MQTT broker |
| `SCREEN_TTS_VOLUME` / `SCREEN_TTS_CODEC_VOLUME` | 60 / 60 | 软件音量与 ES8311 硬件音量，任一为 0 即静音 |
| `SCREEN_LED_BRIGHTNESS` | 128 | 三色灯总亮度 0–255 |
| `SCREEN_TICK_PERIOD_MS` | 1000 | 主循环 tick |
| `SCREEN_LED_BLINK_INTERVAL_MS` | 500 | 紧急等级闪烁半周期 |

flash 分区在 [Makefile](examples/NT26FxDx_OpenKit/Makefile) 的 `demo_screen` 分支：

| 项 | 值 |
|---|---|
| 代码区 | `0x9C000`（624 KB，`CODE_REGION_ADJUST=0x68000`） |
| TTS 资源 | `16k_zh`，约 604 KB，地址 `0x2F2000` |
| 内部 FS | 约 360 KB（`NVM` 持久化只占几 KB） |

## 7. 中文字库

- 字库覆盖 **GB2312 一级 3755 个汉字**，16×16 点阵，共 120 KB，全部 `const` 存放于 flash，不占 RAM。
- 数据由 [gen_cjk_font.ps1](examples/NT26FxDx_OpenKit/demo/src/demo_screen/tools/gen_cjk_font.ps1) 从系统宋体批量生成，无需手工取模：

```powershell
cd examples\NT26FxDx_OpenKit\demo\src\demo_screen\tools
powershell -ExecutionPolicy Bypass -File gen_cjk_font.ps1
```

- 可选参数：`-PixelSize 16`（字号）、`-Dy`（垂直微调）、`-MaxChars N`（只生成前 N 字，调试用）。
- 修改范围（例如扩展到二级字库）后重新生成即可，绘制与查表代码无需改动。

## 8. 已知限制

1. 汉字只覆盖一级字库，二级生僻字（部分人名、地名用字）会显示为方框占位。
2. 代码区借用了内部文件系统空间，FS 由约 524 KB 降到约 360 KB；本项目持久化使用 NVM，不依赖 FS。
3. 修改 `CODE_REGION_ADJUST`、TTS 资源或字库后，必须整包烧录，否则 TTS 资源区为空。
4. 心跳为 30 s 周期，服务器侧离线判定时间需大于该值。

## 9. 与上游 SDK 的差异

| 类型 | 路径 |
|---|---|
| 新增 | `examples/NT26FxDx_OpenKit/demo/src/demo_screen/**` |
| 修改 | `examples/NT26FxDx_OpenKit/Makefile`（新增 `demo_screen` 分支、TTS、代码区调整） |
| 修改 | `examples/NT26FxDx_OpenKit/config`（默认 `DEMO_NAME=demo_screen`） |
| 修改 | `examples/NT26FxDx_OpenKit/iodriver.ini`（SPI0 显式引脚） |
| 重命名 | `README.md` → `README_LIERDA.md`；`README_ZH.md` → `README_LIERDA_ZH.md` |
| 未改动 | `components/`、`rules/`、`tools/` 等 SDK 内核、底包与工具链 |

## 10. 许可

本项目继承上游 SDK 的 **Apache License 2.0**（见 [LICENSE](LICENSE)）。
