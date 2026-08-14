static k_sensor_ae_info sensor_csi2_ae_info[] = {
    {
        .frame_length = 0x226,
        .cur_frame_length = 0x226,
        .one_line_exp_time = 0.000020202f, /* HTS/SCLK = 1000/49.5M */
        .gain_accuracy = 1024,
        .min_gain = 1,
        .max_gain = 110.88f,
        .int_time_delay_frame = 2,
        .gain_delay_frame = 2,
        .color_type = SENSOR_MONO,
        .integration_time_increment = 0.000020202f,
        .gain_increment = SC130GS_MIN_GAIN_STEP,
        .max_integraion_line = 0x226 - 8,
        .min_integraion_line = 1,
        .max_integraion_time = 0.000020202f * (0x226 - 8),
        .min_integraion_time = 0.000020202f * 1,
        .cur_integration_time = 0.000020202f * (0x2050 / 16), /* match init 0x3e01/0x3e02 */
        .cur_again = 1.0,
        .cur_dgain = 1.0,
        .a_gain = {
            .min = 1.0,
            .max = 110.88f,
            .step = (1.0f / 32.0f),
        },
        .d_gain = {
            .min = 1.0,
            .max = 1.0,
            .step = (1.0f / 1024.0f),
        },
        .cur_fps = 90,
    },
};

static const k_sensor_mode sensor_csi2_mode_list[] = {
    {
        .index = 0,
        .sensor_type = SC130GS_MIPI_CSI2_2LANE_1280X1024_90FPS_10BIT_LINEAR,
        .size = {
            .bounds_width = 1280,
            .bounds_height = 1024,
            .top = 0,
            .left = 0,
            .width = 1280,
            .height = 1024,
        },
        .fps = 90000,
        .hdr_mode = SENSOR_MODE_LINEAR,
        .bit_width = 10,
        .bayer_pattern = BAYER_PAT_BGGR,
        .mipi_info = {
            .csi_id = 2,
            .mipi_lanes = 2,
            .data_type = 0x2B,
        },
        .reg_list = sc130gs_mipi_2lane_1280x1024_90fps_init,
#if defined (CONFIG_MPP_ENABLE_SENSOR_SC130GS_ON_CSI2_USE_CHIP_CLK)
        .mclk_setting = {
            {
                .mclk_setting_en = K_TRUE,
                .setting.id = CONFIG_MPP_CSI_DEV2_MCLK_NUM,
                .setting.mclk_sel = SENSOR_PLL1_CLK_DIV4,
                .setting.mclk_div = 25,	/* 594/25 = 23.76MHz */
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

#if defined (CONFIG_MPP_ENABLE_SENSOR_SC130GS_ON_CSI2_USE_CHIP_CLK)
_Static_assert(CONFIG_MPP_CSI_DEV2_MCLK_NUM >= 1 && (CONFIG_MPP_CSI_DEV2_MCLK_NUM <= 3), "Invalid CONFIG_MPP_CSI_DEV2_MCLK_NUM");
#endif
