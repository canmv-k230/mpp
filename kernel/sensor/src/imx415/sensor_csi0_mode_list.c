static k_sensor_ae_info sensor_csi0_ae_info[] = {
    {
        /* 37.125MHz@891: VMAX=0x08CA=2250, HMAX=0x044C=1100 → ~30fps */
        .frame_length = IMX415_VMAX_LINEAR,
        .cur_frame_length = IMX415_VMAX_LINEAR,
        /* Line period = HMAX / 74.25MHz (INCK*2); 1100/74.25e6 ≈ 14.815µs */
        .one_line_exp_time = 0.000014815,
        .gain_accuracy = 1024,

        .min_gain = 1.0,
        /*
         * AE software ceiling 100x (~40 dB ≈ again reg 133).
         * Sensor HW allows up to 0xf0 (~72 dB); keep AE lower for noise.
         */
        .max_gain = 100.0,

        .int_time_delay_frame = 2,
        .gain_delay_frame = 2,
        .color_type = SENSOR_COLOR,

        .integration_time_increment = 0.000014815,
        .gain_increment = IMX415_AGAIN_STEP,

        .max_long_integraion_line = IMX415_VMAX_LINEAR - IMX415_SHR0_MIN,
        .min_long_integraion_line = 1,

        .max_integraion_line = IMX415_VMAX_LINEAR - IMX415_SHR0_MIN,
        .min_integraion_line = 1,

        .max_vs_integraion_line = IMX415_VMAX_LINEAR - IMX415_SHR0_MIN,
        .min_vs_integraion_line = 1,

        .max_long_integraion_time = 0.000014815 * (IMX415_VMAX_LINEAR - IMX415_SHR0_MIN),
        .min_long_integraion_time = 0.000014815 * 1,

        .max_integraion_time = 0.000014815 * (IMX415_VMAX_LINEAR - IMX415_SHR0_MIN),
        .min_integraion_time = 0.000014815 * 1,

        .max_vs_integraion_time = 0.000014815 * (IMX415_VMAX_LINEAR - IMX415_SHR0_MIN),
        .min_vs_integraion_time = 0.000014815 * 1,

        .cur_long_integration_time = 0.0,
        .cur_integration_time = 0.0,
        .cur_vs_integration_time = 0.0,

        .cur_long_again = 0.0,
        .cur_long_dgain = 0.0,

        .cur_again = 0.0,
        .cur_dgain = 0.0,

        .cur_vs_again = 0.0,
        .cur_vs_dgain = 0.0,

        .a_long_gain.min = 1.0,
        .a_long_gain.max = 100.0,
        .a_long_gain.step = IMX415_AGAIN_STEP,

        .a_gain.min = 1.0,
        .a_gain.max = 100.0,
        .a_gain.step = IMX415_AGAIN_STEP,

        .a_vs_gain.min = 1.0,
        .a_vs_gain.max = 100.0,
        .a_vs_gain.step = IMX415_AGAIN_STEP,

        .d_long_gain.max = 1.0,
        .d_long_gain.min = 1.0,
        .d_long_gain.step = 1.0f,

        .d_gain.max = 1.0,
        .d_gain.min = 1.0,
        .d_gain.step = 1.0f,

        .d_vs_gain.max = 1.0,
        .d_vs_gain.min = 1.0,
        .d_vs_gain.step = 1.0f,

        .cur_fps = 30,
    },
};

static const k_sensor_mode sensor_csi0_mode_list[] = {
    {
        .index = 0,
        /* VICAP max is 3840x2160; sensor WINMODE crops from 3864x2192 */
        .sensor_type = IMX415_MIPI_CSI0_4LANE_3840X2160_30FPS_10BIT_LINEAR,
        .size = {
            .bounds_width = 3840,
            .bounds_height = 2160,
            .top = 0,
            .left = 0,
            .width = 3840,
            .height = 2160,
        },
        .fps = 30000,
        .hdr_mode = SENSOR_MODE_LINEAR,
        .bit_width = 10,
        .bayer_pattern = BAYER_PAT_GBRG, /* MEDIA_BUS_FMT_SGBRG10 */
        .mipi_info = {
            .csi_id = 0,
            .mipi_lanes = 4,
            .data_type = 0x2B, /* RAW10 */
        },
        .mclk_setting = {
            {K_FALSE},
            {K_FALSE},
            {K_FALSE},
        },
        .reg_list = imx415_mipi4lane_raw10_3840x2160_30fps_mclk_37m125,
        .sensor_ae_info = &sensor_csi0_ae_info[0],
    },
};
