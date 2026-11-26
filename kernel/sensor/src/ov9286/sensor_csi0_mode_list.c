static k_sensor_ae_info sensor_csi0_ae_info[] = {
    {
        .frame_length = OV9286_30_VTS,
        .cur_frame_length = OV9286_30_VTS,
        .one_line_exp_time = OV9286_30_ONE_LINE,
        .gain_accuracy = 1024,
        .min_gain = 1.0,
        .max_gain = OV9286_MAX_AGAIN,
        .int_time_delay_frame = 0,
        .gain_delay_frame = 0,
        .color_type = SENSOR_MONO,
        .integration_time_increment = OV9286_30_ONE_LINE,
        .gain_increment = OV9286_MIN_GAIN_STEP,
        .max_integraion_line = OV9286_30_MAX_EXP_LINE,
        .min_integraion_line = 1,
        .max_integraion_time = OV9286_30_ONE_LINE * OV9286_30_MAX_EXP_LINE,
        .min_integraion_time = OV9286_30_ONE_LINE * 1,
        .cur_integration_time = 0.0,
        .cur_again = 1.0,
        .cur_dgain = 1.0,
        .a_gain = {
            .min = 1.0,
            .max = OV9286_MAX_AGAIN,
            .step = OV9286_MIN_GAIN_STEP,
        },
        .d_gain = {
            .min = 1.0,
            .max = 1.0,
            .step = (1.0f / 1024.0f),
        },
        .cur_fps = OV9286_30_FPS,
    },
    {
        .frame_length = OV9286_60_VTS,
        .cur_frame_length = OV9286_60_VTS,
        .one_line_exp_time = OV9286_60_ONE_LINE,
        .gain_accuracy = 1024,
        .min_gain = 1.0,
        .max_gain = OV9286_MAX_AGAIN,
        .int_time_delay_frame = 0,
        .gain_delay_frame = 0,
        .color_type = SENSOR_MONO,
        .integration_time_increment = OV9286_60_ONE_LINE,
        .gain_increment = OV9286_MIN_GAIN_STEP,
        .max_integraion_line = OV9286_60_MAX_EXP_LINE,
        .min_integraion_line = 1,
        .max_integraion_time = OV9286_60_ONE_LINE * OV9286_60_MAX_EXP_LINE,
        .min_integraion_time = OV9286_60_ONE_LINE * 1,
        .cur_integration_time = 0.0,
        .cur_again = 1.0,
        .cur_dgain = 1.0,
        .a_gain = {
            .min = 1.0,
            .max = OV9286_MAX_AGAIN,
            .step = OV9286_MIN_GAIN_STEP,
        },
        .d_gain = {
            .min = 1.0,
            .max = 1.0,
            .step = (1.0f / 1024.0f),
        },
        .cur_fps = OV9286_60_FPS,
    },
};

static const k_sensor_mode sensor_csi0_mode_list[] = {
    {
        .index = 0,
        .sensor_type = OV9286_MIPI_CSI0_2LANE_1280X720_30FPS_10BIT_LINEAR,
        .size = {
            .bounds_width = 1280,
            .bounds_height = 720,
            .top = 0,
            .left = 0,
            .width = 1280,
            .height = 720,
        },
        .fps = 30000,
        .hdr_mode = SENSOR_MODE_LINEAR,
        .bit_width = 10,
        .bayer_pattern = BAYER_PAT_BGGR,
        .mipi_info = {
            .csi_id = 0,
            .mipi_lanes = 2,
            .data_type = 0x2B,
        },
#if defined (CONFIG_MPP_ENABLE_SENSOR_OV9286_ON_CSI0_USE_CHIP_CLK)
        .mclk_setting = {
            {
                .mclk_setting_en = K_TRUE,
                .setting.id = OV9286_CSI0_MCLK_ID,
                .setting.mclk_sel = SENSOR_PLL0_CLK_DIV4,
                .setting.mclk_div = 16, /* PLL0/4/16 = 25MHz */
            },
            {K_FALSE},
            {K_FALSE},
        },
        .reg_list = ov9286_mipi_2lane_1280x720_30fps_mclk_25m,
#else
        .mclk_setting = {
            {K_FALSE},
            {K_FALSE},
            {K_FALSE},
        },
        .reg_list = ov9286_mipi_2lane_1280x720_30fps,
#endif
        .sensor_ae_info = &sensor_csi0_ae_info[0],
    },
    {
        .index = 1,
        .sensor_type = OV9286_MIPI_CSI0_2LANE_1280X720_60FPS_10BIT_LINEAR,
        .size = {
            .bounds_width = 1280,
            .bounds_height = 720,
            .top = 0,
            .left = 0,
            .width = 1280,
            .height = 720,
        },
        .fps = 60000,
        .hdr_mode = SENSOR_MODE_LINEAR,
        .bit_width = 10,
        .bayer_pattern = BAYER_PAT_BGGR,
        .mipi_info = {
            .csi_id = 0,
            .mipi_lanes = 2,
            .data_type = 0x2B,
        },
#if defined (CONFIG_MPP_ENABLE_SENSOR_OV9286_ON_CSI0_USE_CHIP_CLK)
        .mclk_setting = {
            {
                .mclk_setting_en = K_TRUE,
                .setting.id = OV9286_CSI0_MCLK_ID,
                .setting.mclk_sel = SENSOR_PLL0_CLK_DIV4,
                .setting.mclk_div = 16,
            },
            {K_FALSE},
            {K_FALSE},
        },
#else
        .mclk_setting = {
            {K_FALSE},
            {K_FALSE},
            {K_FALSE},
        },
#endif
        .reg_list = ov9286_mipi_2lane_1280x720_60fps,
        .sensor_ae_info = &sensor_csi0_ae_info[1],
    },
};
