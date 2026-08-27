static k_sensor_ae_info sensor_csi0_ae_info[] = {
     {
        .frame_length = SC132GS_1080P_VTS,
        .cur_frame_length = SC132GS_1080P_VTS,
        .one_line_exp_time = SC132GS_1080P_ONE_LINE,
        .gain_accuracy = 1024,
        .min_gain = 1,
        .max_gain = SC132GS_MAX_AGAIN,
        .int_time_delay_frame = 2,
        .gain_delay_frame = 2,
        .color_type = SENSOR_COLOR,
        .integration_time_increment = SC132GS_1080P_ONE_LINE,
        .gain_increment = SC132GS_MIN_GAIN_STEP,
        .max_integraion_line = SC132GS_1080P_MAX_EXP_LINE,
        .min_integraion_line = 1,
        .max_integraion_time = SC132GS_1080P_ONE_LINE * SC132GS_1080P_MAX_EXP_LINE,
        .min_integraion_time = SC132GS_1080P_ONE_LINE * 1,
        .cur_integration_time = 0.0,
        .cur_again = 1.0,
        .cur_dgain = 1.0,
        .a_gain = {
            .min = 1.0,
            .max = SC132GS_MAX_AGAIN,
            .step = (1.0f/128.0f),
        },
        .d_gain = {
            .min = 1.0,
            .max = 1.0,
            .step = (1.0f/1024.0f),
        },
        .cur_fps = SC132GS_1080P_FPS,
    },
    /* 640x480 @ 240fps; VTS 0x02a3 from sc132gs_mipi_2lane_640x480_init. */
    {
        .frame_length = SC132GS_VGA_VTS,
        .cur_frame_length = SC132GS_VGA_VTS,
        .one_line_exp_time = SC132GS_VGA_ONE_LINE,
        .gain_accuracy = 1024,
        .min_gain = 1,
        .max_gain = SC132GS_MAX_AGAIN,
        .int_time_delay_frame = 4,
        .gain_delay_frame = 4,
        .color_type = SENSOR_MONO,
        .integration_time_increment = SC132GS_VGA_ONE_LINE,
        .gain_increment = SC132GS_MIN_GAIN_STEP,
        .max_integraion_line = SC132GS_VGA_MAX_EXP_LINE,
        .min_integraion_line = 1,
        .max_integraion_time = SC132GS_VGA_ONE_LINE * SC132GS_VGA_MAX_EXP_LINE,
        .min_integraion_time = SC132GS_VGA_ONE_LINE * 1,
        .cur_integration_time = 0.0,
        .cur_again = 1.0,
        .cur_dgain = 1.0,
        .a_gain = {
            .min = 1.0,
            .max = SC132GS_MAX_AGAIN,
            .step = (1.0f/128.0f),
        },
        .d_gain = {
            .min = 1.0,
            .max = 1.0,
            .step = (1.0f/1024.0f),
        },
        .cur_fps = SC132GS_VGA_FPS,
    },
};

static const k_sensor_mode sensor_csi0_mode_list[] = {
    {
        .index = 0,
        .sensor_type = SC132GS_MIPI_CSI0_1080X1280_120FPS_10BIT_LINEAR,
        .size = {
            .bounds_width = 1080,
            .bounds_height = 1280,
            .top = 0,
            .left = 0,
            .width = 1080,
            .height = 1280,
        },
        .fps = 120000,
        .hdr_mode = SENSOR_MODE_LINEAR,
        .bit_width = 10,
        .bayer_pattern = BAYER_PAT_BGGR,//BAYER_PAT_GRBG, //BAYER_PAT_RGGB,
        .mipi_info = {
            .csi_id = 0,
            .mipi_lanes = 2,
            .data_type = 0x2B,
        },
        .reg_list = sc132gs_mipi_2lane_1080x1280_120fps_init,
#if defined (CONFIG_MPP_ENABLE_SENSOR_SC132GS_ON_CSI0_USE_CHIP_CLK)
        .mclk_setting = {
            {
                .mclk_setting_en = K_TRUE,
                .setting.id = CONFIG_MPP_CSI_DEV0_MCLK_NUM,
                .setting.mclk_sel = SENSOR_PLL1_CLK_DIV4,
                .setting.mclk_div = 25,	// 594/25 = 23.76MHz
            },
            {K_FALSE},
            {K_FALSE},
        },
        .sensor_ae_info = &sensor_csi0_ae_info[0],
#else
        .mclk_setting = {
            {K_FALSE},
            {K_FALSE},
            {K_FALSE},
        },
        .sensor_ae_info = &sensor_csi0_ae_info[0],
#endif // TODO
    },
    {
        .index = 1,
        .sensor_type = SC132GS_MIPI_CSI0_640X480_240FPS_10BIT_LINEAR,
        .size = {
            .bounds_width = 640,
            .bounds_height = 480,
            .top = 0,
            .left = 0,
            .width = 640,
            .height = 480,
        },
        .fps = 240000,
        .hdr_mode = SENSOR_MODE_LINEAR,
        .bit_width = 10,
        .bayer_pattern = BAYER_PAT_BGGR,//BAYER_PAT_GRBG, //BAYER_PAT_RGGB,
        .mipi_info = {
            .csi_id = 0,
            .mipi_lanes = 2,
            .data_type = 0x2B,
        },
        .reg_list = sc132gs_mipi_2lane_640x480_init, 
#if defined (CONFIG_MPP_ENABLE_SENSOR_SC132GS_ON_CSI0_USE_CHIP_CLK)
        .mclk_setting = {
            {
                .mclk_setting_en = K_TRUE,
                .setting.id = CONFIG_MPP_CSI_DEV0_MCLK_NUM,
                .setting.mclk_sel = SENSOR_PLL1_CLK_DIV4,
                .setting.mclk_div = 24,	// 594/25 = 23.76MHz
            },
            {K_FALSE},
            {K_FALSE},
        },
        .sensor_ae_info = &sensor_csi0_ae_info[1],
#else
        .mclk_setting = {
            {K_FALSE},
            {K_FALSE},
            {K_FALSE},
        },
        .sensor_ae_info = &sensor_csi0_ae_info[1],
#endif // TODO
    },
};

#if defined (CONFIG_MPP_ENABLE_SENSOR_SC132GS_ON_CSI0_USE_CHIP_CLK)
_Static_assert(CONFIG_MPP_CSI_DEV0_MCLK_NUM >= 1 && (CONFIG_MPP_CSI_DEV0_MCLK_NUM <= 3), "Invalid CONFIG_MPP_CSI_DEV0_MCLK_NUM");
#endif
