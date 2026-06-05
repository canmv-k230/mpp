static k_sensor_ae_info sensor_csi1_ae_info[] = {
    {
        /* 7 fps */
        .frame_length = 2336,
        .cur_frame_length = 2336,
        .one_line_exp_time = 0.000061155,
        .gain_accuracy = 1024,

        .min_gain = 1.0,
        .max_gain = 18.0,

        .int_time_delay_frame = 2,
        .gain_delay_frame = 2,
        .color_type = SENSOR_COLOR,

        .integration_time_increment = 0.000061155,
        .gain_increment = (1.0f/16.0f),

        .max_long_integraion_line = 0x10000,
        .min_long_integraion_line = 1,

        .max_integraion_line = 0x10000,
        .min_integraion_line = 1,

        .max_vs_integraion_line = 0x10000,
        .min_vs_integraion_line = 1,

        .max_long_integraion_time = 0.000061155 * (0x10000 - 12),
        .min_long_integraion_time = 0.000061155 * 2,

        .max_integraion_time = 0.000061155 * (0x10000 - 12),
        .min_integraion_time = 0.000061155 * 2,

        .max_vs_integraion_time = 0.000061155 * (0x10000 - 12),
        .min_vs_integraion_time = 0.000061155 * 2,

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
        .a_long_gain.max = 63.984375,
        .a_long_gain.step = (1.0f / 64.0f),

        .a_gain.min = 1.0,
        .a_gain.max = 63.984375,
        .a_gain.step = (1.0f / 64.0f),

        .a_vs_gain.min = 1.0,
        .a_vs_gain.max = 63.984375,
        .a_vs_gain.step = (1.0f / 64.0f),

        .d_long_gain.max = 1.0,
        .d_long_gain.min = 1.0,
        .d_long_gain.step = (1.0f / 1024.0f),

        .d_gain.max = 1.0,
        .d_gain.min = 1.0,
        .d_gain.step = (1.0f / 1024.0f),

        .d_vs_gain.max = 1.0,
        .d_vs_gain.min = 1.0,
        .d_vs_gain.step = (1.0f / 1024.0f),

        .cur_fps = 7,
    },
};

static const k_sensor_mode sensor_csi1_mode_list[] = {
    {
        .index = 0,
        .sensor_type = OV13850_MIPI_CSI1_3840x2160_7FPS_10BIT_LINEAR,
        .size = {
            .bounds_width = 3840,
            .bounds_height = 2160,
            .top = 0,
            .left = 0,
            .width = 3840,
            .height = 2160,
        },
        .fps = 7000,
        .hdr_mode = SENSOR_MODE_LINEAR,
        .bit_width = 10,
        .bayer_pattern = BAYER_PAT_BGGR,
        .mipi_info = {
            .csi_id = 0,
            .mipi_lanes = 2,
            .data_type = 0x2C,
        },
#if defined (CONFIG_MPP_SENSOR_OV13850_ON_CSI1_USE_CHIP_CLK)
        .reg_list = ov13850_mipi_2lane_raw10_3840x2160_7fps_regs,
        .mclk_setting = {
            {
                .mclk_setting_en = K_TRUE,
                .setting.id = CONFIG_MPP_CSI_DEV1_MCLK_NUM,
                .setting.mclk_sel = SENSOR_PLL1_CLK_DIV4,
                .setting.mclk_div = 8,
            },
            {K_FALSE},
            {K_FALSE},
        },
        .sensor_ae_info = &sensor_csi1_ae_info[0],
#else
        .reg_list = ov13850_mipi_2lane_raw10_3840x2160_7fps_regs,
        .mclk_setting = {
            {K_FALSE},
            {K_FALSE},
            {K_FALSE},
        },
        .sensor_ae_info = &sensor_csi1_ae_info[0],
#endif
    },
};

#if defined (CONFIG_MPP_SENSOR_OV13850_ON_CSI1_USE_CHIP_CLK)
_Static_assert(CONFIG_MPP_CSI_DEV1_MCLK_NUM >= 1 && (CONFIG_MPP_CSI_DEV1_MCLK_NUM <= 3), "Invalid CONFIG_MPP_CSI_DEV1_MCLK_NUM");
#endif
