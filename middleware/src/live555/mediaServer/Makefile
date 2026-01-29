# Include base configuration
include ../../mkenv.mk
include $(SDK_RTSMART_SRC_DIR)/libs/mk/toolchain_riscv64_musl.mk

# Build configuration
DIR 	:= $(shell basename `pwd`)
BUILD	:= $(SDK_RTSMART_BUILD_DIR)/mpp/middleware/$(DIR)
BIN 	:= $(SDK_RTSMART_BUILD_DIR)/mpp/middleware/mediaServer.elf

# Source files handling
C_SRCS    := $(wildcard *.c)
CXX_SRCS  := $(wildcard *.cpp)

# Consistent variable naming
C_OBJS    := $(patsubst %.c, $(BUILD)/%.o, $(C_SRCS))
CXX_OBJS  := $(patsubst %.cpp, $(BUILD)/%.o, $(CXX_SRCS))

OBJS      := $(C_OBJS) $(CXX_OBJS)
DEPS      := $(OBJS:.o=.d)

# Directory creation logic - Captured and sorted
OBJ_DIRS  := $(sort $(dir $(OBJS) $(BIN)))

# Library configuration
include $(SDK_RTSMART_SRC_DIR)/libs/mk/libmpp.mk

# Compiler flags
C_CXX_FLAGS := -fopenmp -march=rv64imafdcv -mabi=lp64d -mcmodel=medany -Os
C_CXX_FLAGS += -I. $(LIB_CFLAGS) -I$(SDK_SRC_ROOT_DIR)/include -include "generated/autoconf.h"
C_CXX_FLAGS += -Wall -Wextra -ffunction-sections -fdata-sections

C_CXX_FLAGS += -DSOCKLEN_T=socklen_t -DNO_SSTREAM=1 -D_LARGEFILE_SOURCE=1 -D_FILE_OFFSET_BITS=64 -DNO_OPENSSL=1

CFLAGS 		:= $(C_CXX_FLAGS) -std=gnu99
CXX_FLAGS 	:= $(C_CXX_FLAGS) -std=c++20 

# Linker flags
LDFLAGS := -T $(SDK_RTSMART_SRC_DIR)/libs/mk/link.lds --static -Wl,--gc-sections
LDFLAGS += $(LIB_LDFLAGS)

# Phony targets
.PHONY: all clean distclean

all: $(BIN)
	@$(ECHO) [BUILD] mpp middleware $(DIR) done.


# Rule to link the final ELF - Order-only dependency on directory
$(BIN): $(OBJS) | $(OBJ_DIRS)
	@echo "[LD] $@"
	$(Q)$(CXX) $^ $(LDFLAGS) -o $@
	$(Q)$(STRIP) $@

# Rule for C objects - Order-only dependency on directory
$(BUILD)/%.o: %.c | $(OBJ_DIRS)
	@echo "[CC] $@"
	$(Q)$(CC) $(CFLAGS) -MD -MP -MF $(@:.o=.d) -c $< -o $@

# Rule for C++ objects - Order-only dependency on directory
$(BUILD)/%.o: %.cpp | $(OBJ_DIRS)
	@echo "[CXX] $@"
	$(Q)$(CXX) $(CXX_FLAGS) -MD -MP -MF $(@:.o=.d) -c $< -o $@

# Actual directory creation rule
$(OBJ_DIRS):
	$(Q)mkdir -p $@

clean:
	@rm -rf $(BIN)

distclean: clean
	@rm -rf $(BUILD)

-include $(DEPS)
