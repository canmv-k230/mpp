include ../mkenv.mk
include $(SDK_RTSMART_SRC_DIR)/libs/mk/toolchain_riscv64_musl.mk

# Path Definitions
DIR 	:= $(shell basename `pwd`)
BUILD	:= $(SDK_RTSMART_BUILD_DIR)/mpp/kernel/$(DIR)
# Direct target path for the library
LIB		:= $(MPP_KERNEL_LIB_INSTALL_PATH)/lib$(DIR).a

# Source and Object Handling

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
src-$(CONFIG_MPP_DSI_ENABLE_LCD_AML020T) += src/aml020t.c
src-$(CONFIG_MPP_DSI_ENABLE_LCD_JD9852) += src/jd9852.c

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
