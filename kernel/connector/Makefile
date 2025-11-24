
include $(MPP_SRC_DIR)/kernel/mpp.mk
include $(MPP_SRC_DIR)/kernel/rt-smart.mk

include $(SDK_SRC_ROOT_DIR)/.config

CURRECT_DIR_NAME=$(shell basename `pwd`)
LOCAL_SRC_DIR = $(shell pwd)
LIB = $(MPP_SRC_DIR)/kernel/lib/lib$(CURRECT_DIR_NAME).a
BUILD := $(SDK_RTSMART_BUILD_DIR)/mpp/kernel/$(CURRECT_DIR_NAME)

LOCAL_CFLAGS = -I$(LOCAL_SRC_DIR)/src/ \
			   -I$(RTSMART_SRC_DIR)/kernel/bsp/maix3/board/interdrv/gpio

src-y := src/connector_comm.c src/connector_dev.c

src-$(CONFIG_MPP_ENABLE_DSI_DEBUGGER) += src/debugger.c

src-$(CONFIG_MPP_DSI_ENABLE_VIRT) += src/virtdev.c

src-$(CONFIG_MPP_DSI_ENABLE_HDMI_LT9611) += src/lt9611.c
src-$(CONFIG_MPP_DSI_ENABLE_LCD_HX8399) += src/hx8399.c

src-$(CONFIG_MPP_DSI_ENABLE_LCD_ST7701) += src/st7701.c
src-$(CONFIG_MPP_DSI_ENABLE_LCD_ILI9806) += src/ili9806.c
src-$(CONFIG_MPP_DSI_ENABLE_LCD_ILI9881) += src/ili9881.c
src-$(CONFIG_MPP_DSI_ENABLE_LCD_NT35516) += src/nt35516.c
src-$(CONFIG_MPP_DSI_ENABLE_LCD_NT35532) += src/nt35532.c
src-$(CONFIG_MPP_DSI_ENABLE_LCD_GC9503) += src/gc9503.c
src-$(CONFIG_MPP_DSI_ENABLE_LCD_ST7102) += src/st7102.c

SRCS = $(wildcard *.c)
COBJS 	:= $(patsubst %, $(BUILD)/%, $(src-y:.c=.o))
CDEPS 	:= $(patsubst %, $(BUILD)/%, $(src-y:.c=.o.d))

CPP_SRCS = $(wildcard *.cpp)
CPP_OBJS 	:= $(patsubst %, $(BUILD)/%, $(CPP_SRCS:.cpp=.o))
CPP_DEPS 	:= $(patsubst %, $(BUILD)/%, $(CPP_SRCS:.cpp=.o.d))

OBJS := $(COBJS) $(CPP_OBJS)
DEPS := $(CDEPS) $(CPP_DEPS)

.PHONY: all
all: $(LIB)
	@echo "Make lib$(CURRECT_DIR_NAME) done."

.PHONY: clean
clean:
	@rm -rf $(LIB) $(OBJS) $(DEPS)

$(LIB): $(OBJS)
	@echo "AR $@"
	@$(AR) $(ARFLAGS) $@ $(OBJS)

$(OBJS): $(BUILD)/%.o : %.c
	@echo [CC] $<
	@$(CC) $(CC_CFLAGS) -Wno-error $(LOCAL_CFLAGS) $(BSP_CFLGAS) $(RTSMART_CFLAGS) $(MPP_DRIVER_CFLGAS) -MD -MP -MF $@.d -c $< -o $@

# $(sort $(var)) removes duplicates
#
# The net effect of this, is it causes the objects to depend on the
# object directories (but only for existence), and the object directories
# will be created if they don't exist.
OBJ_DIRS = $(sort $(dir $(OBJS)))
$(OBJS): | $(OBJ_DIRS)
$(OBJ_DIRS):
	@mkdir -p $@

-include $(DEPS)
