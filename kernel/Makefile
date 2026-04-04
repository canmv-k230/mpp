subdirs-y := sensor connector mediafreq pm

.PHONY: all clean

all: $(subdirs-y)
	@echo build mpp kernel done.

clean:
	@if [ -n "$(subdirs-y)" ]; then \
		for dir in $(subdirs-y); do \
			echo "[CLEAN] mpp kernel $$dir"; \
			$(MAKE) -C $$dir clean || exit $?; \
		done; \
	fi
	@echo "Make MPP Kernel clean done."

ifneq ($(strip $(subdirs-y)),)
.PHONY: $(subdirs-y)

$(subdirs-y):
	@echo "[BUILD] mpp kernel $@"
	@$(MAKE) -C $@ all || exit $?;
endif
