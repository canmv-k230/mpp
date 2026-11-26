static k_sensor_ae_info sensor_csi1_ae_info[] = {
    {
        .frame_length = OV9732_720P_VTS,
        .cur_frame_length = OV9732_720P_VTS,
        .one_line_exp_time = OV9732_720P_ONE_LINE,
        .gain_accuracy = 1024,
        .min_gain = 1.0,
        .max_gain = OV9732_MAX_AGAIN,
        .int_time_delay_frame = 2,
        .gain_delay_frame = 2,
        .color_type = SENSOR_COLOR_IR,
        .integration_time_increment = OV9732_720P_ONE_LINE,
        .gain_increment = OV9732_MIN_GAIN_STEP,
        .max_integraion_line = OV9732_720P_MAX_EXP_LINE,
        .min_integraion_line = 1,
        .max_integraion_time = OV9732_720P_ONE_LINE * OV9732_720P_MAX_EXP_LINE,
        .min_integraion_time = OV9732_720P_ONE_LINE * 1,
        .cur_integration_time = 0.0,
        .cur_again = 1.0,
        .cur_dgain = 1.0,
        .a_gain = {
            .min = 1.0,
            .max = OV9732_REG_AGAIN_MAX,
            .step = OV9732_MIN_GAIN_STEP,
        },
        .d_gain = {
            .min = 1.0,
            .max = 1.0,
            .step = (1.0f / 1024.0f),
        },
        .cur_fps = OV9732_720P_FPS,
    },
};

static const k_sensor_mode sensor_csi1_mode_list[] = {
    {
        .index = 0,
        .sensor_type = OV9732_MIPI_CSI1_1LANE_1280X720_30FPS_10BIT_LINEAR,
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
            .csi_id = 1,
            .mipi_lanes = 1,
            .data_type = 0x2B,
        },
#if defined (CONFIG_MPP_ENABLE_SENSOR_OV9732_ON_CSI1_USE_CHIP_CLK)
        .mclk_setting = {
            {
                .mclk_setting_en = K_TRUE,
                .setting.id = OV9732_CSI1_MCLK_ID,
                .setting.mclk_sel = SENSOR_PLL0_CLK_DIV4,
                .setting.mclk_div = 25, /* PLL0/4/25 = 16MHz */
            },
            {K_FALSE},
            {K_FALSE},
        },
        .reg_list = ov9732_mipi_1lane_1280x720_30fps_mclk_16m,
#else
        .mclk_setting = {
            {K_FALSE},
            {K_FALSE},
            {K_FALSE},
        },
        .reg_list = ov9732_mipi_1lane_1280x720_30fps,
#endif
        .sensor_ae_info = &sensor_csi1_ae_info[0],
    },
};
