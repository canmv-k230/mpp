include ../mkenv.mk

include $(MPP_SRC_DIR)/middleware/mpp.mk
include $(MPP_SRC_DIR)/middleware/rt-smart.mk

include $(SDK_SRC_ROOT_DIR)/.config

dir-y := kdmedia live555 mp4_format mp4_player rtsp_client rtsp_server ogg

dir-$(CONFIG_MPP_ENABLE_MIDDLEWARE_LIB_X264)		+= x264/src
dir-$(CONFIG_MPP_ENABLE_MIDDLEWARE_LIB_FFMPEG)		+= ffmpeg/src
dir-$(CONFIG_MPP_ENABLE_MIDDLEWARE_LIB_RTSP_PUSHER)	+= rtsp_pusher

# Add directories to the build system
dirs := $(dir-y)

all:
	$(Q)rm -rf $(MPP_MIDDLEWARE_LIB_INSTALL_PATH)/*
	$(Q)$(foreach dir,$(dirs), \
		$(MAKE) -C $(dir) all || exit $?; \
		if [ "$(dir)" = "ffmpeg/src" ] || [ "$(dir)" = "x264/src" ]; then \
			$(MAKE) -C $(dir) install || exit $?; \
		fi; \
	)

	@echo "Make MPP middleware libraries done."

clean:
	$(Q)rm -rf $(MPP_MIDDLEWARE_LIB_INSTALL_PATH)/*
	$(Q)$(foreach dir,$(dirs),$(MAKE) -C $(dir) clean || exit $?;)
	@echo "Clean MPP middleware libraries done."

.PHONY: all clean
