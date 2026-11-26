CUSTOM_DIR := examples/$(PROJECT)

IOINI  := $(TOP)/$(CUSTOM_DIR)/iodriver.ini

# Load project config (BUILD_MODE, HW_XXX_READY flags)
include $(TOP)/$(CUSTOM_DIR)/config

# Common include paths
CFLAGS_INC += -I $(TOP)/$(CUSTOM_DIR)/inc
CFLAGS_INC += -I $(TOP)/$(CUSTOM_DIR)/demo/inc

ifeq ($(BUILD_MODE), demo)

DEMO_NAME ?= hardware

ifeq ($(DEMO_NAME), hardware)

CUSTOM_SRC := $(CUSTOM_DIR)/demo/src
CUSTOM_COBJSTEMP := $(CUSTOM_SRC)/0demo_main.o

BUILD_DRIVER_LCD_EN ?= y
BUILD_DRIVER_WS2812B_EN ?= y
BUILD_DRIVER_CAMERA_EN ?= y
BUILD_DRIVER_TP_EN ?= y

ifeq ($(HWDEMO_KEY_EN), y)
CUSTOM_COBJSTEMP += $(CUSTOM_SRC)/demo_key.o
CFLAGS_DEFS += -DHWDEMO_KEY_EN
endif

ifeq ($(HWDEMO_LCD_EN), y)
CUSTOM_COBJSTEMP += $(CUSTOM_SRC)/demo_lcd.o
CFLAGS_DEFS += -DHWDEMO_LCD_EN
endif

ifeq ($(HWDEMO_CAMERA_EN), y)
CUSTOM_COBJSTEMP += $(CUSTOM_SRC)/demo_camera.o
CFLAGS_DEFS += -DHWDEMO_CAMERA_EN
endif

ifeq ($(HWDEMO_LCD_SSD1306_EN), y)
CUSTOM_COBJSTEMP += $(CUSTOM_SRC)/demo_lcd_ssd1306.o
CFLAGS_DEFS += -DHWDEMO_LCD_SSD1306_EN
endif

ifeq ($(HWDEMO_TP_EN), y)
CUSTOM_COBJSTEMP += $(CUSTOM_SRC)/demo_tp.o
CFLAGS_DEFS += -DHWDEMO_TP_EN
endif

ifeq ($(HWDEMO_SOUND_EN), y)
CUSTOM_COBJSTEMP += $(CUSTOM_SRC)/demo_sound.o
CFLAGS_DEFS += -DHWDEMO_SOUND_EN
endif

ifeq ($(HWDEMO_WS2812B_EN), y)
CUSTOM_COBJSTEMP += $(CUSTOM_SRC)/demo_ws2812b.o
CFLAGS_DEFS += -DHWDEMO_WS2812B_EN
endif

ifeq ($(HWDEMO_TCP_EN), y)
CUSTOM_COBJSTEMP += $(CUSTOM_SRC)/demo_tcp.o
CFLAGS_DEFS += -DHWDEMO_TCP_EN
endif

else ifeq ($(DEMO_NAME), demo_zhongyi_ai)

CUSTOM_SRC := $(CUSTOM_DIR)/demo/src/demo_zhongyi_ai/src
CUSTOM_INC := $(CUSTOM_DIR)/demo/src/demo_zhongyi_ai/inc
CFLAGS_INC += -I $(TOP)/$(CUSTOM_INC)
CUSTOM_COBJSTEMP := $(patsubst $(TOP)/%.c,%.o,$(wildcard $(TOP)/$(CUSTOM_SRC)/*.c))
PP_FILES += $(wildcard $(TOP)/$(CUSTOM_SRC)/*.c)

else ifeq ($(DEMO_NAME), demo_3a)

include $(TOP)/$(CUSTOM_DIR)/demo/src/demo_3a/Makefile

else ifeq ($(DEMO_NAME), demo_screen)

CUSTOM_SRC := $(CUSTOM_DIR)/demo/src/demo_screen/src
CUSTOM_INC := $(CUSTOM_DIR)/demo/src/demo_screen/inc
CFLAGS_INC += -I $(TOP)/$(CUSTOM_INC)
CUSTOM_COBJSTEMP := $(patsubst $(TOP)/%.c,%.o,$(wildcard $(TOP)/$(CUSTOM_SRC)/*.c))
PP_FILES += $(wildcard $(TOP)/$(CUSTOM_SRC)/*.c)

# 本工程外设：OLED 常开；WS2812B 待引脚确认后再开
BUILD_DRIVER_LCD_EN := y

# TTS：资源和库会占用 APP 区，采样率必须与音频配置保持一致
BUILD_COMP_TTS_EN   := y
TTS_RESOURCE_TYPE   ?= 16k_zh

# TTS 资源占 604 KB，默认代码区只剩 208 KB（链接会溢出约 190 KB）。
# 借用 256 KB 内部文件系统空间给代码区：FS 由 780 KB 降到约 524 KB，
# 仍远大于 mem_map_chk.h 要求的 8 KB；本工程持久化用 NVM，不依赖该 FS。
CODE_REGION_ADJUST  ?= 0x40000

else ifeq ($(DEMO_NAME), demo_watch)

include $(TOP)/$(CUSTOM_DIR)/demo/src/demo_watch/Makefile

else
$(error Unsupported DEMO_NAME "$(DEMO_NAME)"; use "hardware", "demo_zhongyi_ai", "demo_3a", or "demo_watch")
endif

else

CUSTOM_SRC := $(CUSTOM_DIR)/app/src
CUSTOM_COBJSTEMP := $(CUSTOM_SRC)/user_main.o

endif

CUSTOM_COBJS := $(addprefix $(BUILDDIR)/, $(CUSTOM_COBJSTEMP))

ifeq ($(TOOLCHAIN),GCC)
libusr-y += lib_NT26FxDx_OpenKit.a

$(BUILDDIR)/lib/libusr/lib_NT26FxDx_OpenKit.a: $(CUSTOM_COBJS)
	@mkdir -p $(dir $@)
	$(ECHO) AR $@
	$(Q)$(AR) -cr $@ $^
endif
