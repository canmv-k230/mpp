subdirs-y := sensor connector pm vg_lite vg_lite/vg_lite_util uvc framework

GEN_ISP_CONFIG_BIN := $(abspath ../../tools/gen_isp_config_bin)

.PHONY: all clean check_gen_isp_config_bin $(subdirs-y)

all: $(subdirs-y)
	@echo build mpp userapps done.

ifneq ($(strip $(subdirs-y)),)
.PHONY: $(subdirs-y)

sensor: check_gen_isp_config_bin

$(subdirs-y):
	@echo "[BUILD] mpp userapps $@"
	@$(MAKE) -C $@ all

check_gen_isp_config_bin:
	@if [ ! -x "$(GEN_ISP_CONFIG_BIN)" ]; then \
		echo "Error: missing prebuilt ISP config generator: $(GEN_ISP_CONFIG_BIN)"; \
		echo "Please install the prebuilt mpp/tools/gen_isp_config_bin tool."; \
		exit 1; \
	fi
endif

clean:
	@if [ -n "$(subdirs-y)" ]; then \
		for dir in $(subdirs-y); do \
			echo "[CLEAN] mpp userapps $$dir"; \
			$(MAKE) -C $$dir clean || exit $$?; \
		done; \
	fi
	@rm -rf lib/*.a
