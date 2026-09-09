include ../mkenv.mk
include $(SDK_RTSMART_SRC_DIR)/libs/mk/toolchain_riscv64_musl.mk

# Path Definitions
DIR 	:= $(shell basename `pwd`)
BUILD	:= $(SDK_RTSMART_BUILD_DIR)/mpp/userapps/$(DIR)
# Direct target path for the library
LIB		:= $(MPP_USERAPPS_LIB_INSTALL_PATH)/lib$(DIR).a

MPP_TOOLS_DIR          := $(SDK_RTSMART_SRC_DIR)/mpp/tools
GEN_ISP_CONFIG_BIN     := $(MPP_TOOLS_DIR)/gen_isp_config_bin
ISP_CONFIG_SRC_DIR     := $(SDK_RTSMART_SRC_DIR)/mpp/userapps/src/sensor/config
ISP_CONFIG_OUT_DIR     := $(SDK_BUILD_IMAGES_DIR)/bin

ISP_SENSOR_CONFIG_PATTERNS :=
ISP_SENSOR_CONFIG_PATTERNS += $(if $(filter y,$(CONFIG_MPP_ENABLE_SENSOR_GC2093)),gc2093-*)
ISP_SENSOR_CONFIG_PATTERNS += $(if $(filter y,$(CONFIG_MPP_ENABLE_SENSOR_OV5647)),ov5647-*)
ISP_SENSOR_CONFIG_PATTERNS += $(if $(filter y,$(CONFIG_MPP_ENABLE_SENSOR_IMX335)),imx335-*)
ISP_SENSOR_CONFIG_PATTERNS += $(if $(filter y,$(CONFIG_MPP_ENABLE_SENSOR_IMX415)),imx415-*)
ISP_SENSOR_CONFIG_PATTERNS += $(if $(filter y,$(CONFIG_MPP_ENABLE_SENSOR_SC132GS)),sc132gs-*)
ISP_SENSOR_CONFIG_PATTERNS += $(if $(filter y,$(CONFIG_MPP_ENABLE_SENSOR_SC130GS)),sc130gs-*)
ISP_SENSOR_CONFIG_PATTERNS += $(if $(filter y,$(CONFIG_MPP_ENABLE_SENSOR_XS9950)),xs9950-*)
ISP_SENSOR_CONFIG_PATTERNS += $(if $(filter y,$(CONFIG_MPP_ENABLE_SENSOR_BF3238)),bf3238-*)
ISP_SENSOR_CONFIG_PATTERNS += $(if $(filter y,$(CONFIG_MPP_ENABLE_SENSOR_OV13850)),ov13850-*)
ISP_SENSOR_CONFIG_PATTERNS += $(if $(filter y,$(CONFIG_MPP_ENABLE_SENSOR_IMX675)),imx675-*)
ISP_SENSOR_CONFIG_PATTERNS += $(if $(filter y,$(CONFIG_MPP_ENABLE_SENSOR_IMX290)),imx290-*)

ISP_CONFIG_DATABASES := $(foreach pattern,$(ISP_SENSOR_CONFIG_PATTERNS),\
	$(patsubst %_auto.json,%,$(notdir $(wildcard $(ISP_CONFIG_SRC_DIR)/$(pattern)_auto.json))))

include $(SDK_RTSMART_SRC_DIR)/libs/mk/libmpp.mk

# Source and Object Handling
C_SRCS          := $(wildcard *.c)
CXX_SRCS        := $(wildcard *.cpp)

C_OBJS          := $(patsubst %.c, $(BUILD)/%.o, $(C_SRCS))
CXX_OBJS        := $(patsubst %.cpp, $(BUILD)/%.o, $(CXX_SRCS))

OBJS            := $(C_OBJS) $(CXX_OBJS)
DEPS            := $(OBJS:.o=.d)

# Directory Creation Logic
OBJ_DIRS        := $(sort $(dir $(OBJS)) $(MPP_USERAPPS_LIB_INSTALL_PATH))

# Compiler Flags
C_CXX_FLAGS     := -fopenmp -march=rv64imafdcv -mabi=lp64d -mcmodel=medany -Os
C_CXX_FLAGS     += -Iinclude -I. -Wall -Wextra -ffunction-sections -fdata-sections
C_CXX_FLAGS     += -Wno-multichar -Wno-unused-result -Wno-unused-variable -Wno-format
C_CXX_FLAGS     += $(LIB_CFLAGS)

CFLAGS          := $(C_CXX_FLAGS) -std=gnu99
CXX_FLAGS       := $(C_CXX_FLAGS) -std=c++20 -Wno-register 

# --- Targets ---

.PHONY: all clean install_isp_configs

# 'all' now ensures the library is built directly into the target and headers are synced
all: $(LIB)
	@$(ECHO) [BUILD] mpp userapps $(DIR) done.

# Rule to archive the library directly into the target folder
$(LIB): $(OBJS) | $(MPP_USERAPPS_LIB_INSTALL_PATH)
	@$(ECHO) "[AR] $@"
	$(Q)$(AR) rcs $@ $^

# Compilation rules for intermediate objects
$(BUILD)/%.o : %.c | $(OBJ_DIRS)
	@$(ECHO) "[CC] $@"
	$(Q)$(CC) $(CFLAGS) -MD -MP -MF $(@:.o=.d) -c $< -o $@

$(BUILD)/%.o : %.cpp | $(OBJ_DIRS)
	@$(ECHO) "[CXX] $@"
	$(Q)$(CXX) $(CXX_FLAGS) -MD -MP -MF $(@:.o=.d) -c $< -o $@

$(OBJ_DIRS):
	$(Q)mkdir -p $@

$(ISP_CONFIG_OUT_DIR):
	$(Q)mkdir -p $@

install_isp_configs: | $(ISP_CONFIG_OUT_DIR)
ifeq ($(CONFIG_MPP_ISP_CONFIG_INSTALL_BIN),y)
	@if [ -z "$(strip $(ISP_CONFIG_DATABASES))" ]; then \
		echo "No enabled sensor configs to generate."; \
		exit 0; \
	fi; \
	if [ ! -x "$(GEN_ISP_CONFIG_BIN)" ]; then \
		echo "Error: missing ISP config generator: $(GEN_ISP_CONFIG_BIN)"; \
		echo "Run make -C $(SDK_RTSMART_SRC_DIR)/mpp/userapps/src/vicap/gen_isp_config_bin all, or install the prebuilt tool."; \
		exit 1; \
	fi; \
	echo "Generate binary ISP configs in $(ISP_CONFIG_OUT_DIR)/"; \
	$(GEN_ISP_CONFIG_BIN) $(ISP_CONFIG_SRC_DIR) $(ISP_CONFIG_OUT_DIR) $(ISP_CONFIG_DATABASES)
endif
ifeq ($(CONFIG_MPP_ISP_CONFIG_INSTALL_JSON_XML),y)
	@$(ECHO) "Copy JSON and XML ISP configs from $(ISP_CONFIG_SRC_DIR)/ to $(ISP_CONFIG_OUT_DIR)/"
	@if [ -n "$(strip $(ISP_SENSOR_CONFIG_PATTERNS))" ]; then \
		for isp_config in $(ISP_SENSOR_CONFIG_PATTERNS); do \
			rsync -a $(ISP_CONFIG_SRC_DIR)/$$isp_config.json $(ISP_CONFIG_SRC_DIR)/$$isp_config.xml $(ISP_CONFIG_OUT_DIR)/ || exit $$?; \
		done; \
	else \
		echo "No enabled sensor configs to copy."; \
	fi
endif

clean:
	$(Q)rm -rf $(LIB) $(BUILD)

-include $(DEPS)
