include mkenv.mk

include $(SDK_SRC_ROOT_DIR)/.config

dir-y :=

# Directory assignment based on Kconfig options
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_FASTBOOT_APP) += fastboot_app # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_OPENCV_CAMERA_TEST) += opencv_camera_test
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_AHD_SENSOR) += sample_ahd_sensor
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_AUDIO) += sample_audio # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_AV) += sample_av # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_CIPHER) += sample_cipher/sample_aes # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_CIPHER) += sample_cipher/sample_hash # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_CIPHER) += sample_cipher/sample_hwhash # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_CIPHER) += sample_cipher/sample_pufs # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_CSC) += sample_csc # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_GSDMA) += sample_gsdma # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_DMA_BIND) += sample_dma_bind # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_DPU) += sample_dpu # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_DPU_VICAP) += sample_dpu_vicap # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_DPU_VO) += sample_dpu_vo # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_DW200) += sample_dw200 # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_EEPROM) += sample_eeprom # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_FACE_AE) += sample_face_ae # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_FACE_DETECT) += sample_face_detect # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_FFT) += sample_fft # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_I2C_SLAVE) += sample_i2c_slave # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_LOG) += sample_log # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_MCM) += sample_mcm
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_MMZ) += sample_mmz # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_NONAI_2D) += sample_nonai_2d # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_OTP) += sample_otp # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_PM) += sample_pm # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_PWM) += sample_pwm # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_SENSOR_OTP) += sample_sensor_otp # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_TRIPLE_CAMERA_FACEDETECT) += sample_triple_camera_facedetect # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_VB) += sample_vb # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_VDD_R) += sample_vdd_r # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_VDEC) += sample_vdec # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_VDSS) += sample_vdss # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_VDV) += sample_vdv # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_VENC) += sample_venc # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_VICAP) += sample_vicap # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_VIRTUAL_VIO) += sample_virtual_vio # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_VO) += sample_vo # compile passed
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_TIGER) += sample_tiger
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_UVC) += sample_uvc
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_UVC_DEVICE) += sample_uvc_dev_picture sample_uvc_dev_vicap
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_JAMLINK) += sample_jamlink
dir-$(CONFIG_MPP_ENABLE_USER_SAMPLES_SAMPLE_WBC_DUMP) += sample_wbc_dump

# Add directories to the build system
dirs := $(sort $(dir-y))

.PHONY: all clean

all:
	$(Q)rm -rf $(MPP_USERAPP_SMAPLE_ELF_DIR)/*
ifeq ($(CONFIG_MPP_ENABLE_USERAPPS_SAMPLES),y)
	$(Q)$(foreach dir,$(dirs),$(MAKE) -C $(dir) all || exit $?;)
endif
	@echo "Make MPP userapps samples done."

clean:
	$(Q)rm -rf $(MPP_USERAPP_SMAPLE_ELF_DIR)/*
ifeq ($(CONFIG_MPP_ENABLE_USERAPPS_SAMPLES),y)
	$(Q)$(foreach dir,$(dirs),$(MAKE) -C $(dir) clean || exit $?;)
endif
	@echo "Clean MPP userapps samples done."
