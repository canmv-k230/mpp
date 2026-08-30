ifneq ($(MKENV_INCLUDED),1)
export SDK_SRC_ROOT_DIR := $(realpath $(dir $(realpath $(lastword $(MAKEFILE_LIST))))/../../../../)
endif

include $(SDK_SRC_ROOT_DIR)/tools/mkenv.mk

include $(SDK_SRC_ROOT_DIR)/.config

MPP_KERNEL_LIB_INSTALL_PATH := $(SDK_RTSMART_SRC_DIR)/mpp/kernel/lib/

RTSMART_CFLAGS := \
	-I$(SDK_RTSMART_SRC_DIR)/rtsmart/kernel/rt-thread/include \
	-I$(SDK_RTSMART_SRC_DIR)/rtsmart/kernel/rt-thread/components/dfs/include \
	-I$(SDK_RTSMART_SRC_DIR)/rtsmart/kernel/rt-thread/components/dfs/filesystems/devfs \
	-I$(SDK_RTSMART_SRC_DIR)/rtsmart/kernel/rt-thread/components/dfs/filesystems/romfs \
	-I$(SDK_RTSMART_SRC_DIR)/rtsmart/kernel/rt-thread/components/drivers/include \
	-I$(SDK_RTSMART_SRC_DIR)/rtsmart/kernel/rt-thread/components/drivers/wlan \
	-I$(SDK_RTSMART_SRC_DIR)/rtsmart/kernel/rt-thread/components/drivers/tty/include \
	-I$(SDK_RTSMART_SRC_DIR)/rtsmart/kernel/rt-thread/components/finsh \
	-I$(SDK_RTSMART_SRC_DIR)/rtsmart/kernel/rt-thread/components/libc/compilers/musl \
	-I$(SDK_RTSMART_SRC_DIR)/rtsmart/kernel/rt-thread/components/libc/time \
	-I$(SDK_RTSMART_SRC_DIR)/rtsmart/kernel/rt-thread/components/lwp \
	-I$(SDK_RTSMART_SRC_DIR)/rtsmart/kernel/rt-thread/components/utilities/resource \
	-I$(SDK_RTSMART_SRC_DIR)/rtsmart/kernel/rt-thread/components/utilities/ulog \
	-I$(SDK_RTSMART_SRC_DIR)/rtsmart/kernel/rt-thread/components/lwp/arch/risc-v/rv64 \

RTSMART_CDEFS := \
	-DHAVE_CCONFIG_H -D__STDC_ISO_10646__=201206L -D_STDC_PREDEF_H -D__KERNEL__

BSP_CFLGAS := \
	-I$(SDK_RTSMART_SRC_DIR)/rtsmart/kernel/bsp/maix3/c908 \
	-I$(SDK_RTSMART_SRC_DIR)/rtsmart/kernel/bsp/maix3 \
	-I$(SDK_RTSMART_SRC_DIR)/rtsmart/kernel/bsp/maix3/board \
	-I$(SDK_RTSMART_SRC_DIR)/rtsmart/kernel/bsp/maix3/drivers/interdrv/sysctl/sysctl_boot \
	-I$(SDK_RTSMART_SRC_DIR)/rtsmart/kernel/bsp/maix3/drivers/interdrv/sysctl/sysctl_power \
	-I$(SDK_RTSMART_SRC_DIR)/rtsmart/kernel/bsp/maix3/drivers/interdrv/sysctl/sysctl_reset \
	-I$(SDK_RTSMART_SRC_DIR)/rtsmart/kernel/bsp/maix3/drivers/interdrv/tsensor \
	-I$(SDK_RTSMART_SRC_DIR)/rtsmart/kernel/bsp/maix3/drivers/interdrv/gpio \
	-I$(SDK_RTSMART_SRC_DIR)/rtsmart/kernel/bsp/maix3/drivers/interdrv/fpioa \
	-I$(SDK_RTSMART_SRC_DIR)/rtsmart/kernel/bsp/maix3/drivers/interdrv/pdma \
	-I$(SDK_RTSMART_SRC_DIR)/rtsmart/kernel/bsp/maix3/drivers/extdrv/regulator \
	-I$(SDK_RTSMART_SRC_DIR)/rtsmart/kernel/bsp/maix3/drivers/extcomponents/usage

MPP_CFLAGS := \
	-I$(SDK_RTSMART_SRC_DIR)/mpp/include/ \
	-I$(SDK_RTSMART_SRC_DIR)/mpp/include/comm \
	-I$(SDK_RTSMART_SRC_DIR)/mpp/include/ioctl \
	-I$(SDK_RTSMART_SRC_DIR)/mpp/kernel/ext_inc \
	-I$(SDK_RTSMART_SRC_DIR)/mpp/kernel/mediafreq/src/sysctl/sysctl_media_clock

MPP_KERNEL_CFLAGS := $(RTSMART_CFLAGS) $(RTSMART_CDEFS) $(BSP_CFLGAS) $(MPP_CFLAGS)
