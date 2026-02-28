# include $(MPP_SRC_DIR)/middleware/mpp.mk
# include $(MPP_SRC_DIR)/middleware/rt-smart.mk

include ../../mkenv.mk
include $(SDK_TOOLS_DIR)/toolchain_rtsmart.mk

SRCPATH=.
prefix=$(MPP_SRC_DIR)/middleware/src/x264/src/lib_x264
bindir=$(SDK_RTSMART_BUILD_DIR)/mpp/middleware/bin
libdir=$(MPP_MIDDLEWARE_LIB_INSTALL_PATH)
includedir=$(MPP_MIDDLEWARE_INC_INSTALL_PATH)
SYS_ARCH=RISCV64
SYS=LINUX
CC=$(CROSS_COMPILE)gcc
CFLAGS=-Wno-maybe-uninitialized -Wshadow -O3 -ffast-math  -Wall -I. -I$(SRCPATH) -fPIC -Wno-multichar -Wno-deprecated-declarations -Wno-unused-result -Wno-unused-variable -Wno-format -Wno-return-type -Wno-sign-compare -Wno-unused-label -std=gnu99 -D_GNU_SOURCE -fomit-frame-pointer -fno-tree-vectorize
COMPILER=GNU
COMPILER_STYLE=GNU
DEPMM=-MM -g0
DEPMT=-MT
# LD=riscv64-unknown-linux-musl-gcc -o
LD=$(CROSS_COMPILE)gcc -o
LDFLAGS= $(LINKFLAG) -Wl,--whole-archive -Wl,--no-whole-archive -n --static -lm -lpthread -ldl
LIBX264=libx264.a
AR=$(CROSS_COMPILE)ar rc
RANLIB=$(CROSS_COMPILE)ranlib
STRIP=strip
INSTALL=install
AS=
ASFLAGS= -I. -I$(SRCPATH) -DSTACK_ALIGNMENT=4
RC=
RCFLAGS=
EXE=
HAVE_GETOPT_LONG=1
DEVNULL=/dev/null
PROF_GEN_CC=-fprofile-generate
PROF_GEN_LD=-fprofile-generate
PROF_USE_CC=-fprofile-use
PROF_USE_LD=-fprofile-use
HAVE_OPENCL=yes
CC_O=-o $@
default: cli
install: install-cli
default: lib-static
install: install-lib-static
LDFLAGSCLI = -ldl
CLI_LIBX264 = $(LIBX264)
