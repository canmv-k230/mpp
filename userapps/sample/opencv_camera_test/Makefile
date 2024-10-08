include $(MPP_SRC_DIR)/userapps/sample/mpp.mk
include $(MPP_SRC_DIR)/userapps/sample/rt-smart.mk

CURRECT_DIR_NAME=$(shell basename `pwd`)
LOCAL_SRC_DIR = $(shell pwd)
BIN = $(MPP_SRC_DIR)/userapps/sample/elf/$(CURRECT_DIR_NAME).elf

LIBPATH = $(MPP_LIB_PATH)
LIBS = $(MPP_LIBS)

OPENCV_LIB_PATH=$(MPP_SRC_DIR)/userapps/src/opencv/build/install/lib
OPENCV_3RDPARTY_LIB_PATH=$(MPP_SRC_DIR)/userapps/src/opencv/build/install/lib/opencv4/3rdparty
OPENCV_LIBS = -latomic \
              -lopencv_core \
              -lzlib \
              -llibjpeg-turbo \
              -llibopenjp2 \
              -llibpng \
              -llibtiff \
              -llibwebp \
              -llibprotobuf \
              -lopencv_imgproc \
              -lopencv_imgcodecs \
              -lopencv_videoio \
              -lopencv_video \
              -lopencv_highgui \
              -lopencv_objdetect


LOCAL_CFLAGS = -I$(LOCAL_SRC_DIR) \
	       -I$(MPP_SRC_DIR)/userapps/src/opencv/build/install/include/opencv4 

SRCS = $(wildcard $(LOCAL_SRC_DIR)/*.c)

OBJS = $(patsubst %.c,%.o,$(SRCS))

CPP_SRCS = $(wildcard $(LOCAL_SRC_DIR)/*.cpp)

CPP_OBJS = $(patsubst %.cpp,%.o,$(CPP_SRCS))

all: $(BIN)
	@-rm -f *.o
	@echo "${PWD}/Makefile all"

$(OBJS): %.o : %.c
	@echo CC $@
	@$(CC) $(CC_CFLAGS) $(LOCAL_CFLAGS) $(BSP_CFLGAS) $(RTSMART_CFLAGS) $(MPP_USER_CFLGAS) -c $< -o $@

$(CPP_OBJS): %.o : %.cpp
	@echo CXX $@
	@$(CPP) $(CC_CFLAGS) $(LOCAL_CFLAGS) $(BSP_CFLGAS) $(RTSMART_CFLAGS) $(MPP_USER_CFLGAS) -c $< -o $@

$(BIN): $(OBJS) $(CPP_OBJS)
	@echo LD $@
	@$(CPP) -o $(BIN) $(LINKFLAG) -Wl,--whole-archive -Wl,--no-whole-archive -n --static $^ -L$(LIBPATH) -L$(OPENCV_LIB_PATH) -L$(OPENCV_3RDPARTY_LIB_PATH) -Wl,--start-group $(LIBS) $(OPENCV_LIBS) -Wl,--end-group

clean:
	echo "${PWD}/Makefile clean"
	-rm -rf $(BIN) $(OBJS)

.PHONY: all clean
