ifneq ($(MKENV_INCLUDED),1)
export SDK_SRC_ROOT_DIR := $(realpath $(dir $(realpath $(lastword $(MAKEFILE_LIST))))/../../../../../)
endif

include $(SDK_SRC_ROOT_DIR)/tools/mkenv.mk

include $(SDK_SRC_ROOT_DIR)/.config

MPP_USERAPPS_LIB_INSTALL_PATH := $(SDK_RTSMART_SRC_DIR)/mpp/userapps/lib/
