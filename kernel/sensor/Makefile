include ../mkenv.mk
include $(SDK_RTSMART_SRC_DIR)/libs/mk/toolchain_riscv64_musl.mk

# Path Definitions
DIR 	:= $(shell basename `pwd`)
BUILD	:= $(SDK_RTSMART_BUILD_DIR)/mpp/kernel/$(DIR)
# Direct target path for the library
LIB		:= $(MPP_KERNEL_LIB_INSTALL_PATH)/lib$(DIR).a

src-y := src/sensor_comm.c src/sensor_dev.c src/autofocus/sensor_autofocus.c

src-$(CONFIG_MPP_ENABLE_SENSOR_GC2093) += src/gc2093/gc2093.c
src-$(CONFIG_MPP_ENABLE_SENSOR_OV5647) += src/ov5647/ov5647.c
src-$(CONFIG_MPP_ENABLE_SENSOR_IMX335) += src/imx335/imx335.c
src-$(CONFIG_MPP_ENABLE_SENSOR_SC132GS) += src/sc132gs/sc132gs.c
src-$(CONFIG_MPP_ENABLE_SENSOR_XS9950) += src/xs9950/xs9950.c
src-$(CONFIG_MPP_ENABLE_SENSOR_BF3238) += src/bf3238/bf3238.c

src-$(CONFIG_MPP_ENABLE_SENSOR_AF_DRV_DW9714P) += src/autofocus/af_dw9714p.c

# Source and Object Handling
C_SRCS          := $(src-y)
CXX_SRCS        := $(wildcard src/*.cpp)

C_OBJS          := $(patsubst %.c, $(BUILD)/%.o, $(C_SRCS))
CXX_OBJS        := $(patsubst %.cpp, $(BUILD)/%.o, $(CXX_SRCS))

OBJS            := $(C_OBJS) $(CXX_OBJS)
DEPS            := $(OBJS:.o=.d)

# Directory Creation Logic
OBJ_DIRS        := $(sort $(dir $(OBJS)) $(MPP_KERNEL_LIB_INSTALL_PATH))

# Compiler Flags
C_CXX_FLAGS     := -fopenmp -march=rv64imafdcv -mabi=lp64d -mcmodel=medany -O0
C_CXX_FLAGS     += -Iinclude -I. -Wall -Wextra -ffunction-sections -fdata-sections
C_CXX_FLAGS     += -Wno-multichar -Wno-unused-result -Wno-unused-variable -Wno-format
C_CXX_FLAGS     += $(LIB_CFLAGS) $(MPP_KERNEL_CFLAGS)

C_CXX_FLAGS		+= -Isrc

CFLAGS          := $(C_CXX_FLAGS) -std=gnu99
CXX_FLAGS       := $(C_CXX_FLAGS) -std=c++20 -Wno-register 

# --- Targets ---

.PHONY: all clean

# 'all' now ensures the library is built directly into the target and headers are synced
all: $(LIB)
	@$(ECHO) [BUILD] mpp kernel $(DIR) done.


# Rule to archive the library directly into the target folder
$(LIB): $(OBJS) | $(MPP_KERNEL_LIB_INSTALL_PATH)
	@echo "[AR] $@"
	$(Q)$(AR) rcs $@ $^

# Compilation rules for intermediate objects
$(BUILD)/%.o : %.c | $(OBJ_DIRS)
	@echo "[CC] $@"
	$(Q)$(CC) $(CFLAGS) -MD -MP -MF $(@:.o=.d) -c $< -o $@

$(BUILD)/%.o : %.cpp | $(OBJ_DIRS)
	@echo "[CXX] $@"
	$(Q)$(CXX) $(CXX_FLAGS) -MD -MP -MF $(@:.o=.d) -c $< -o $@

$(OBJ_DIRS):
	$(Q)mkdir -p $@

clean:
	$(Q)rm -rf $(LIB_TARGET) $(BUILD)

-include $(DEPS)
