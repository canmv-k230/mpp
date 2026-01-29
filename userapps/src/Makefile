subdirs-y := sensor connector fft cipher pm vg_lite vg_lite/vg_lite_util uvc framework

.PHONY: all clean $(subdirs-y)

all: $(subdirs-y)
	@echo build mpp userapps done.

ifneq ($(strip $(subdirs-y)),)
.PHONY: $(subdirs-y)

$(subdirs-y):
	@echo "[BUILD] mpp userapps $@"
	@$(MAKE) -C $@ all || exit $?;
endif

clean:
	@if [ -n "$(subdirs-y)" ]; then \
		for dir in $(subdirs-y); do \
			echo "[CLEAN] mpp userapps $$dir"; \
			$(MAKE) -C $$dir clean || exit $?; \
		done; \
	fi
	@rm -rf lib/*.a
