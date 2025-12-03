include ../../mkenv.mk

include $(MPP_SRC_DIR)/middleware/mpp.mk
include $(MPP_SRC_DIR)/middleware/rt-smart.mk

CC=riscv64-unknown-linux-musl-gcc
AR=riscv64-unknown-linux-musl-ar

CURRECT_DIR_NAME=$(shell pwd)
LIB = $(MPP_MIDDLEWARE_LIB_INSTALL_PATH)/libogg.a

BUILD := $(SDK_RTSMART_BUILD_DIR)/mpp/middleware/ogg

INCS := -I$(CURRECT_DIR_NAME)/include -I$(CURRECT_DIR_NAME)/_include

SRC_DIRS := src

SRCS = $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.c))

COBJS 	:= $(patsubst %, $(BUILD)/%, $(SRCS:.c=.o))
CDEPS 	:= $(patsubst %, $(BUILD)/%, $(SRCS:.c=.o.d))

CPP_SRCS = $(wildcard *.cpp)
CPP_OBJS 	:= $(patsubst %, $(BUILD)/%, $(CPP_SRCS:.cpp=.o))
CPP_DEPS 	:= $(patsubst %, $(BUILD)/%, $(CPP_SRCS:.cpp=.o.d))

OBJS := $(COBJS) $(CPP_OBJS)
DEPS := $(CDEPS) $(CPP_DEPS)

CFLAGS :=
DEFINE :=

all:$(LIB)
	@echo "Make lib$(CURRECT_DIR_NAME) done."

$(OBJS) : $(BUILD)/%.o: %.c
	@echo [CC] $@
	$(Q)$(CC) -c -fPIC $(CFLAGS) $< -o $@ $(INCS)  $(DEFINE) -MD -MP -MF $@.d

$(LIB) : $(OBJS)
	@echo [LD] $@
	$(Q)$(AR) $(ARFLAGS) $@ $^

clean:
	$(Q)rm -f $(LIB) $(OBJS) $(DEPS)

.PHONY: all clean

# $(sort $(var)) removes duplicates
#
# The net effect of this, is it causes the objects to depend on the
# object directories (but only for existence), and the object directories
# will be created if they don't exist.
OBJ_DIRS = $(sort $(dir $(OBJS)))
$(OBJS): | $(OBJ_DIRS)
$(OBJ_DIRS):
	$(Q)mkdir -p $@

-include $(DEPS)
