// sensor ae info
static k_sensor_ae_info sensor_csi2_ae_info[] = {
    {
        .frame_length = 2193,
        .cur_frame_length = 2193,
        .one_line_exp_time = 0.0000152,
        .gain_accuracy = 1024,

        .min_gain = 1.0,
        .max_gain = 15.9375,

        .int_time_delay_frame = 0,
        .gain_delay_frame = 0,
        //.ae_min_interval_frame =1.0,
        .color_type = SENSOR_MONO,	//mono sensor

        .integration_time_increment = 0.0000152,
        .gain_increment = XS9950_MIN_GAIN_STEP,

        .max_integraion_line = 165,    //2.5ms //197,  // 3ms
        .min_integraion_line = 1,

        .max_vs_integraion_line = 2193,
        .min_vs_integraion_line = 2193 - 1,

        .max_integraion_time = 0.0000152 * 165,
        .min_integraion_time = 0.0000152 * 1,
        .max_vs_integraion_time = 0.0000152 * 2193,
        .min_vs_integraion_time = 0.0000152 * (2193 - 1),

        .cur_integration_time = 0.0,
        .cur_vs_integration_time = 0.0,

        .cur_again = 0.0,
        .cur_dgain = 0.0,

        .cur_vs_again = 0.0,
        .cur_vs_dgain = 0.0,

        .a_gain.min = 1.0,
        .a_gain.max = 15.9375,
        .a_gain.step = (1.0f/16.0f),

        .a_vs_gain.min = 1.0,
        .a_vs_gain.max = 15.9375,
        .a_vs_gain.step = (1.0f/16.0f),

        .d_gain.max = 1.0,
        .d_gain.min = 1.0,
        .d_gain.step = (1.0f/1024.0f),

        .d_vs_gain.max = 1.0,
        .d_vs_gain.min = 1.0,
        .d_vs_gain.step = (1.0f/1024.0f),
        .cur_fps = 3000,
    },
};

static const k_sensor_mode sensor_csi2_mode_list[] = {
    {
        .index = 0,
        .sensor_type = XS9950_MIPI_CSI2_1280X720_30FPS_YUV422,
        .size = {
            .bounds_width = 1280,
            .bounds_height = 720,
            .top = 0,
            .left = 0,
            .width = 1280,
            .height = 720,
        },
        .fps = 30000,
        .hdr_mode = SENSOR_MODE_HDR_STITCH,
        .stitching_mode = SENSOR_STITCHING_3DOL,
        .bit_width = 8,
        .bayer_pattern = BAYER_PAT_RGGB,
        .mipi_info = {
            .csi_id = 0,
            .mipi_lanes = 2,
            .data_type = 0x1e,
        },
        .reg_list = xs9950_mipi4lane_720p_25fps_linear,
#if defined (CONFIG_MPP_ENABLE_SENSOR_XS9950_ON_CSI2_USE_CHIP_CLK)
        .mclk_setting = {
            {
                .mclk_setting_en = K_FALSE,
                .setting.id = SENSOR_MCLK0,
                .setting.mclk_sel = SENSOR_PLL0_CLK_DIV4,
                .setting.mclk_div = 16,
            },
            {K_FALSE},
            {K_FALSE},
        },
        .sensor_ae_info = &sensor_csi2_ae_info[0],
#else
        .mclk_setting = {
            {K_FALSE},
            {K_FALSE},
            {K_FALSE},
        },
        .sensor_ae_info = &sensor_csi2_ae_info[0],
#endif
    },
};

#if defined (CONFIG_MPP_ENABLE_SENSOR_XS9950_ON_CSI2_USE_CHIP_CLK)
_Static_assert(CONFIG_MPP_CSI_DEV2_MCLK_NUM >= 1 && (CONFIG_MPP_CSI_DEV2_MCLK_NUM <= 3), "Invalid CONFIG_MPP_CSI_DEV2_MCLK_NUM");
#endif
