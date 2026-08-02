subdirs-y := UsageEnvironment BasicUsageEnvironment groupsock liveMedia mediaServer

.PHONY: all clean

all: $(subdirs-y)
	@echo [BUILD] mpp middleware live555 done.

ifneq ($(strip $(subdirs-y)),)
.PHONY: $(subdirs-y)

$(subdirs-y):
	@echo "[BUILD] mpp middleware live555 $@"
	@$(MAKE) -C $@ all
endif

# Define Dependencies (Sequential Order)
UsageEnvironment: BasicUsageEnvironment

liveMedia: UsageEnvironment groupsock

mediaServer: liveMedia

clean:
	@if [ -n "$(subdirs-y)" ]; then \
		for dir in $(subdirs-y); do \
			echo "[CLEAN] mpp middleware live555 $$dir"; \
			$(MAKE) -C $$dir clean || exit $$?; \
		done; \
	fi
