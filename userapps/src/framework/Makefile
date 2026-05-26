include ../mkenv.mk
include $(SDK_RTSMART_SRC_DIR)/libs/mk/toolchain_riscv64_musl.mk

# Path Definitions
DIR 	:= $(shell basename `pwd`)
BUILD	:= $(SDK_RTSMART_BUILD_DIR)/mpp/userapps/$(DIR)
# Direct target path for the library
LIB		:= $(MPP_USERAPPS_LIB_INSTALL_PATH)/lib$(DIR).a

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

.PHONY: all clean

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

clean:
	$(Q)rm -rf $(LIB) $(BUILD)

-include $(DEPS)
