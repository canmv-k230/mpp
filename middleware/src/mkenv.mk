ifneq ($(MKENV_INCLUDED),1)
export SDK_SRC_ROOT_DIR := $(realpath $(dir $(realpath $(lastword $(MAKEFILE_LIST))))/../../../../../)
endif

include $(SDK_SRC_ROOT_DIR)/tools/mkenv.mk

include $(SDK_SRC_ROOT_DIR)/.config

MPP_MIDDLEWARE_LIB_INSTALL_PATH := $(SDK_RTSMART_BUILD_DIR)/mpp/middleware/lib/
MPP_MIDDLEWARE_INC_INSTALL_PATH := $(SDK_RTSMART_BUILD_DIR)/mpp/middleware/include/

export MKENV_INCLUDED_MPP_MIDDLEWARE=1
