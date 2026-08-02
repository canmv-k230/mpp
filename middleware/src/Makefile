include mkenv.mk

subdirs-y := kdmedia live555 mp4_format rtsp_client rtsp_server ogg

subdirs-$(CONFIG_MPP_ENABLE_MIDDLEWARE_LIB_FFMPEG)		+= ffmpeg/src
subdirs-$(CONFIG_MPP_ENABLE_MIDDLEWARE_LIB_RTSP_PUSHER)	+= rtsp_pusher

.PHONY: all clean

all: $(subdirs-y)
	@echo [BUILD] mpp middleware done.

clean:
	$(Q)rm -rf $(MPP_MIDDLEWARE_LIB_INSTALL_PATH)/*
	@for dir in $(subdirs-y); do \
		echo "[CLEAN] mpp middleware $$dir"; \
		$(MAKE) -C $$dir clean || exit $$?; \
	done

ifneq ($(strip $(subdirs-y)),)
.PHONY: $(subdirs-y)

$(subdirs-y):
	@echo "[BUILD] mpp middleware $@"
	@$(MAKE) -C $@ all
	@# Use shell conditional to check the target name during execution
	@case "$@" in \
		"ffmpeg/src") \
			echo "[INSTALL] $@"; \
			$(MAKE) -C $@ install || exit $$?; \
			;; \
	esac

endif

# Define Dependencies (Sequential Order)

rtsp_client: live555

rtsp_server: live555

ifeq ($(CONFIG_MPP_ENABLE_MIDDLEWARE_LIB_FFMPEG),y)

mp4_format: ffmpeg/src

endif

ifeq ($(CONFIG_MPP_ENABLE_MIDDLEWARE_LIB_RTSP_PUSHER),y)

rtsp_pusher:ffmpeg/src

endif
