static k_sensor_ae_info sensor_csi0_ae_info[] = {
    {
        /* 7 fps */
        .frame_length = 2336,
        .cur_frame_length = 2336,
        .one_line_exp_time = 0.000061155,
        .gain_accuracy = 1024,

        .min_gain = 1.0,
        .max_gain = 63.9375,

        .int_time_delay_frame = 2,
        .gain_delay_frame = 2,
        .color_type = SENSOR_COLOR,

        .integration_time_increment = 0.000061155,
        .gain_increment = (1.0f/16.0f),

        .max_long_integraion_line = 2336 - 12,
        .min_long_integraion_line = 1,

        .max_integraion_line = 2336 - 12,
        .min_integraion_line = 1,

        .max_vs_integraion_line = 2336 - 12,
        .min_vs_integraion_line = 1,

        .max_long_integraion_time = 0.000061155 * (2336 - 12),
        .min_long_integraion_time = 0.000061155 * 2,

        .max_integraion_time = 0.000061155 * (2336 - 12),
        .min_integraion_time = 0.000061155 * 2,

        .max_vs_integraion_time = 0.000061155 * (2336 - 12),
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
        .a_long_gain.max = 63.9375,
        .a_long_gain.step = (1.0f/16.0f),

        .a_gain.min = 1.0,
        .a_gain.max = 63.9375,
        .a_gain.step = (1.0f/16.0f),

        .a_vs_gain.min = 1.0,
        .a_vs_gain.max = 63.9375,
        .a_vs_gain.step = (1.0f/16.0f),

        .d_long_gain.max = 1.0,
        .d_long_gain.min = 1.0,
        .d_long_gain.step = (1.0f),

        .d_gain.max = 1.0,
        .d_gain.min = 1.0,
        .d_gain.step = (1.0f),

        .d_vs_gain.max = 1.0,
        .d_vs_gain.min = 1.0,
        .d_vs_gain.step = (1.0f),

        .cur_fps = 7,
    },
#if defined (CONFIG_MPP_SENSOR_OV13850_ENABLE_4LANE_CONFIGURE)
    {
        /* Legacy 27FPS enum: its table actually uses SCLK=30 MHz (about 10.7 fps). */
        .frame_length = 2336,
        .cur_frame_length = 2336,
        .one_line_exp_time = 0.00001550,
        .gain_accuracy = 1024,

        .min_gain = 1.0,
        .max_gain = 63.9375,

        .int_time_delay_frame = 2,
        .gain_delay_frame = 2,
        .color_type = SENSOR_COLOR,

        .integration_time_increment = 0.00001550,
        .gain_increment = (1.0f/16.0f),

        .max_long_integraion_line = 2336 - 12,
        .min_long_integraion_line = 1,

        .max_integraion_line = 2336 - 12,
        .min_integraion_line = 1,

        .max_vs_integraion_line = 2336 - 12,
        .min_vs_integraion_line = 1,

        .max_long_integraion_time = 0.00001550 * (2336 - 12),
        .min_long_integraion_time = 0.00001550 * 2,

        .max_integraion_time = 0.00001550 * (2336 - 12),
        .min_integraion_time = 0.00001550 * 2,

        .max_vs_integraion_time = 0.00001550 * (2336 - 12),
        .min_vs_integraion_time = 0.00001550 * 2,

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
        .a_long_gain.max = 63.9375,
        .a_long_gain.step = (1.0f/16.0f),

        .a_gain.min = 1.0,
        .a_gain.max = 63.9375,
        .a_gain.step = (1.0f/16.0f),

        .a_vs_gain.min = 1.0,
        .a_vs_gain.max = 63.9375,
        .a_vs_gain.step = (1.0f/16.0f),

        .d_long_gain.max = 1.0,
        .d_long_gain.min = 1.0,
        .d_long_gain.step = (1.0f),

        .d_gain.max = 1.0,
        .d_gain.min = 1.0,
        .d_gain.step = (1.0f),

        .d_vs_gain.max = 1.0,
        .d_vs_gain.min = 1.0,
        .d_vs_gain.step = (1.0f),

        .cur_fps = 27, /* Legacy nominal enum; use the new 30FPS mode for 4K30. */
    },
    {
        /* SCLK=120 MHz, HTS=4800, line=10 us, VTS=3333. */
        .frame_length = 3333,
        .cur_frame_length = 3333,
        .one_line_exp_time = 0.00001000,
        .gain_accuracy = 1024,

        .min_gain = 1.0,
        .max_gain = 63.9375,

        .int_time_delay_frame = 2,
        .gain_delay_frame = 2,
        .color_type = SENSOR_COLOR,

        .integration_time_increment = 0.00001000,
        .gain_increment = (1.0f/16.0f),

        .max_long_integraion_line = 3333 - 12,
        .min_long_integraion_line = 1,

        .max_integraion_line = 3333 - 12,
        .min_integraion_line = 1,

        .max_vs_integraion_line = 3333 - 12,
        .min_vs_integraion_line = 1,

        .max_long_integraion_time = 0.00001000 * (3333 - 12),
        .min_long_integraion_time = 0.00001000 * 2,

        .max_integraion_time = 0.00001000 * (3333 - 12),
        .min_integraion_time = 0.00001000 * 2,

        .max_vs_integraion_time = 0.00001000 * (3333 - 12),
        .min_vs_integraion_time = 0.00001000 * 2,

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
        .a_long_gain.max = 63.9375,
        .a_long_gain.step = (1.0f/16.0f),

        .a_gain.min = 1.0,
        .a_gain.max = 63.9375,
        .a_gain.step = (1.0f/16.0f),

        .a_vs_gain.min = 1.0,
        .a_vs_gain.max = 63.9375,
        .a_vs_gain.step = (1.0f/16.0f),

        .d_long_gain.max = 1.0,
        .d_long_gain.min = 1.0,
        .d_long_gain.step = (1.0f),

        .d_gain.max = 1.0,
        .d_gain.min = 1.0,
        .d_gain.step = (1.0f),

        .d_vs_gain.max = 1.0,
        .d_vs_gain.min = 1.0,
        .d_vs_gain.step = (1.0f),

        .cur_fps = 30,
    },
    {
        /* SCLK=120 MHz, HTS=4800, line=10 us, VTS=1667. */
        .frame_length = 1667,
        .cur_frame_length = 1667,
        .one_line_exp_time = 0.00001000,
        .gain_accuracy = 1024,

        .min_gain = 1.0,
        .max_gain = 63.9375,

        .int_time_delay_frame = 2,
        .gain_delay_frame = 2,
        .color_type = SENSOR_COLOR,

        .integration_time_increment = 0.00001000,
        .gain_increment = (1.0f/16.0f),

        .max_long_integraion_line = 1667 - 12,
        .min_long_integraion_line = 1,

        .max_integraion_line = 1667 - 12,
        .min_integraion_line = 1,

        .max_vs_integraion_line = 1667 - 12,
        .min_vs_integraion_line = 1,

        .max_long_integraion_time = 0.00001000 * (1667 - 12),
        .min_long_integraion_time = 0.00001000 * 2,

        .max_integraion_time = 0.00001000 * (1667 - 12),
        .min_integraion_time = 0.00001000 * 2,

        .max_vs_integraion_time = 0.00001000 * (1667 - 12),
        .min_vs_integraion_time = 0.00001000 * 2,

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
        .a_long_gain.max = 63.9375,
        .a_long_gain.step = (1.0f/16.0f),

        .a_gain.min = 1.0,
        .a_gain.max = 63.9375,
        .a_gain.step = (1.0f/16.0f),

        .a_vs_gain.min = 1.0,
        .a_vs_gain.max = 63.9375,
        .a_vs_gain.step = (1.0f/16.0f),

        .d_long_gain.max = 1.0,
        .d_long_gain.min = 1.0,
        .d_long_gain.step = (1.0f),

        .d_gain.max = 1.0,
        .d_gain.min = 1.0,
        .d_gain.step = (1.0f),

        .d_vs_gain.max = 1.0,
        .d_vs_gain.min = 1.0,
        .d_vs_gain.step = (1.0f),

        .cur_fps = 60,
    },
    {
        /* SCLK=120 MHz, HTS=4800, line=10 us, VTS=3333. */
        .frame_length = 3333,
        .cur_frame_length = 3333,
        .one_line_exp_time = 0.00001000,
        .gain_accuracy = 1024,

        .min_gain = 1.0,
        .max_gain = 63.9375,

        .int_time_delay_frame = 2,
        .gain_delay_frame = 2,
        .color_type = SENSOR_COLOR,

        .integration_time_increment = 0.00001000,
        .gain_increment = (1.0f/16.0f),

        .max_long_integraion_line = 3333 - 12,
        .min_long_integraion_line = 1,

        .max_integraion_line = 3333 - 12,
        .min_integraion_line = 1,

        .max_vs_integraion_line = 3333 - 12,
        .min_vs_integraion_line = 1,

        .max_long_integraion_time = 0.00001000 * (3333 - 12),
        .min_long_integraion_time = 0.00001000 * 2,

        .max_integraion_time = 0.00001000 * (3333 - 12),
        .min_integraion_time = 0.00001000 * 2,

        .max_vs_integraion_time = 0.00001000 * (3333 - 12),
        .min_vs_integraion_time = 0.00001000 * 2,

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
        .a_long_gain.max = 63.9375,
        .a_long_gain.step = (1.0f/16.0f),

        .a_gain.min = 1.0,
        .a_gain.max = 63.9375,
        .a_gain.step = (1.0f/16.0f),

        .a_vs_gain.min = 1.0,
        .a_vs_gain.max = 63.9375,
        .a_vs_gain.step = (1.0f/16.0f),

        .d_long_gain.max = 1.0,
        .d_long_gain.min = 1.0,
        .d_long_gain.step = (1.0f),

        .d_gain.max = 1.0,
        .d_gain.min = 1.0,
        .d_gain.step = (1.0f),

        .d_vs_gain.max = 1.0,
        .d_vs_gain.min = 1.0,
        .d_vs_gain.step = (1.0f),

        .cur_fps = 30,
    },
    {
        /* SCLK=120 MHz, HTS=4800, line=10 us, VTS=1667. */
        .frame_length = 1667,
        .cur_frame_length = 1667,
        .one_line_exp_time = 0.00001000,
        .gain_accuracy = 1024,

        .min_gain = 1.0,
        .max_gain = 63.9375,

        .int_time_delay_frame = 2,
        .gain_delay_frame = 2,
        .color_type = SENSOR_COLOR,

        .integration_time_increment = 0.00001000,
        .gain_increment = (1.0f/16.0f),

        .max_long_integraion_line = 1667 - 12,
        .min_long_integraion_line = 1,

        .max_integraion_line = 1667 - 12,
        .min_integraion_line = 1,

        .max_vs_integraion_line = 1667 - 12,
        .min_vs_integraion_line = 1,

        .max_long_integraion_time = 0.00001000 * (1667 - 12),
        .min_long_integraion_time = 0.00001000 * 2,

        .max_integraion_time = 0.00001000 * (1667 - 12),
        .min_integraion_time = 0.00001000 * 2,

        .max_vs_integraion_time = 0.00001000 * (1667 - 12),
        .min_vs_integraion_time = 0.00001000 * 2,

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
        .a_long_gain.max = 63.9375,
        .a_long_gain.step = (1.0f/16.0f),

        .a_gain.min = 1.0,
        .a_gain.max = 63.9375,
        .a_gain.step = (1.0f/16.0f),

        .a_vs_gain.min = 1.0,
        .a_vs_gain.max = 63.9375,
        .a_vs_gain.step = (1.0f/16.0f),

        .d_long_gain.max = 1.0,
        .d_long_gain.min = 1.0,
        .d_long_gain.step = (1.0f),

        .d_gain.max = 1.0,
        .d_gain.min = 1.0,
        .d_gain.step = (1.0f),

        .d_vs_gain.max = 1.0,
        .d_vs_gain.min = 1.0,
        .d_vs_gain.step = (1.0f),

        .cur_fps = 60,
    },
#endif // CONFIG_MPP_SENSOR_OV13850_ENABLE_4LANE_CONFIGURE
};

static const k_sensor_mode sensor_csi0_mode_list[] = {
    {
        .index = 0,
        .sensor_type = OV13850_MIPI_CSI0_3840x2160_7FPS_10BIT_LINEAR,
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
#if defined (CONFIG_MPP_SENSOR_OV13850_ON_CSI0_USE_CHIP_CLK)
        .reg_list = ov13850_mipi_2lane_raw10_3840x2160_7fps_regs,
        .mclk_setting = {
            {
                .mclk_setting_en = K_TRUE,
                .setting.id = CONFIG_MPP_CSI_DEV0_MCLK_NUM,
                .setting.mclk_sel = SENSOR_PLL1_CLK_DIV4,
                .setting.mclk_div = 8,
            },
            {K_FALSE},
            {K_FALSE},
        },
        .sensor_ae_info = &sensor_csi0_ae_info[0],
#else
        .reg_list = ov13850_mipi_2lane_raw10_3840x2160_7fps_regs,
        .mclk_setting = {
            {K_FALSE},
            {K_FALSE},
            {K_FALSE},
        },
        .sensor_ae_info = &sensor_csi0_ae_info[0],
#endif
    },
#if defined (CONFIG_MPP_SENSOR_OV13850_ENABLE_4LANE_CONFIGURE)
    {
        .index = 1,
        .sensor_type = OV13850_MIPI_CSI0_4LANE_3840X2160_27FPS_10BIT_LINEAR,
        .size = {
            .bounds_width = 3840,
            .bounds_height = 2160,
            .top = 0,
            .left = 0,
            .width = 3840,
            .height = 2160,
        },
        .fps = 27000,
        .hdr_mode = SENSOR_MODE_LINEAR,
        .bit_width = 10,
        .bayer_pattern = BAYER_PAT_BGGR,
        .mipi_info = {
            .csi_id = 0,
            .mipi_lanes = 4,
            .data_type = 0x2B,
        },
#if defined (CONFIG_MPP_SENSOR_OV13850_ON_CSI0_USE_CHIP_CLK)
        .reg_list = ov13850_mipi_4lane_raw10_3840x2160_27fps_regs,
        .mclk_setting = {
            {
                .mclk_setting_en = K_TRUE,
                .setting.id = CONFIG_MPP_CSI_DEV0_MCLK_NUM,
                .setting.mclk_sel = SENSOR_PLL1_CLK_DIV4,
                .setting.mclk_div = 8,
            },
            {K_FALSE},
            {K_FALSE},
        },
        .sensor_ae_info = &sensor_csi0_ae_info[1],
#else
        .reg_list = ov13850_mipi_4lane_raw10_3840x2160_27fps_regs,
        .mclk_setting = {
            {K_FALSE},
            {K_FALSE},
            {K_FALSE},
        },
        .sensor_ae_info = &sensor_csi0_ae_info[1],
#endif
    },
    {
        .index = 2,
        .sensor_type = OV13850_MIPI_CSI0_4LANE_1920X1080_30FPS_10BIT_LINEAR,
        .size = {
            .bounds_width = 1920,
            .bounds_height = 1080,
            .top = 0,
            .left = 0,
            .width = 1920,
            .height = 1080,
        },
        .fps = 30000,
        .hdr_mode = SENSOR_MODE_LINEAR,
        .bit_width = 10,
        .bayer_pattern = BAYER_PAT_BGGR,
        .mipi_info = {
            .csi_id = 0,
            .mipi_lanes = 4,
            .data_type = 0x2B,
        },
#if defined (CONFIG_MPP_SENSOR_OV13850_ON_CSI0_USE_CHIP_CLK)
        .reg_list = ov13850_mipi_4lane_raw10_1920x1080_30fps_regs,
        .mclk_setting = {
            {
                .mclk_setting_en = K_TRUE,
                .setting.id = CONFIG_MPP_CSI_DEV0_MCLK_NUM,
                .setting.mclk_sel = SENSOR_PLL1_CLK_DIV4,
                .setting.mclk_div = 8,
            },
            {K_FALSE},
            {K_FALSE},
        },
        .sensor_ae_info = &sensor_csi0_ae_info[2],
#else
        .reg_list = ov13850_mipi_4lane_raw10_1920x1080_30fps_regs,
        .mclk_setting = {
            {K_FALSE},
            {K_FALSE},
            {K_FALSE},
        },
        .sensor_ae_info = &sensor_csi0_ae_info[2],
#endif
    },
    {
        .index = 3,
        .sensor_type = OV13850_MIPI_CSI0_4LANE_1920X1080_60FPS_10BIT_LINEAR,
        .size = {
            .bounds_width = 1920,
            .bounds_height = 1080,
            .top = 0,
            .left = 0,
            .width = 1920,
            .height = 1080,
        },
        .fps = 60000,
        .hdr_mode = SENSOR_MODE_LINEAR,
        .bit_width = 10,
        .bayer_pattern = BAYER_PAT_BGGR,
        .mipi_info = {
            .csi_id = 0,
            .mipi_lanes = 4,
            .data_type = 0x2B,
        },
#if defined (CONFIG_MPP_SENSOR_OV13850_ON_CSI0_USE_CHIP_CLK)
        .reg_list = ov13850_mipi_4lane_raw10_1920x1080_30fps_regs,
        .mclk_setting = {
            {
                .mclk_setting_en = K_TRUE,
                .setting.id = CONFIG_MPP_CSI_DEV0_MCLK_NUM,
                .setting.mclk_sel = SENSOR_PLL1_CLK_DIV4,
                .setting.mclk_div = 8,
            },
            {K_FALSE},
            {K_FALSE},
        },
        .sensor_ae_info = &sensor_csi0_ae_info[3],
#else
        .reg_list = ov13850_mipi_4lane_raw10_1920x1080_30fps_regs,
        .mclk_setting = {
            {K_FALSE},
            {K_FALSE},
            {K_FALSE},
        },
        .sensor_ae_info = &sensor_csi0_ae_info[3],
#endif
    },
    {
        .index = 4,
        .sensor_type = OV13850_MIPI_CSI0_4LANE_2112X1568_30FPS_10BIT_LINEAR,
        .size = {
            .bounds_width = 2112,
            .bounds_height = 1568,
            .top = 0,
            .left = 0,
            .width = 2112,
            .height = 1568,
        },
        .fps = 30000,
        .hdr_mode = SENSOR_MODE_LINEAR,
        .bit_width = 10,
        .bayer_pattern = BAYER_PAT_BGGR,
        .mipi_info = {
            .csi_id = 0,
            .mipi_lanes = 4,
            .data_type = 0x2B,
        },
#if defined (CONFIG_MPP_SENSOR_OV13850_ON_CSI0_USE_CHIP_CLK)
        .reg_list = ov13850_mipi_4lane_raw10_2112x1568_30fps_regs,
        .mclk_setting = {
            {
                .mclk_setting_en = K_TRUE,
                .setting.id = CONFIG_MPP_CSI_DEV0_MCLK_NUM,
                .setting.mclk_sel = SENSOR_PLL1_CLK_DIV4,
                .setting.mclk_div = 8,
            },
            {K_FALSE},
            {K_FALSE},
        },
        .sensor_ae_info = &sensor_csi0_ae_info[4],
#else
        .reg_list = ov13850_mipi_4lane_raw10_2112x1568_30fps_regs,
        .mclk_setting = {
            {K_FALSE},
            {K_FALSE},
            {K_FALSE},
        },
        .sensor_ae_info = &sensor_csi0_ae_info[4],
#endif
    },
    {
        .index = 5,
        .sensor_type = OV13850_MIPI_CSI0_4LANE_2112X1568_60FPS_10BIT_LINEAR,
        .size = {
            .bounds_width = 2112,
            .bounds_height = 1568,
            .top = 0,
            .left = 0,
            .width = 2112,
            .height = 1568,
        },
        .fps = 60000,
        .hdr_mode = SENSOR_MODE_LINEAR,
        .bit_width = 10,
        .bayer_pattern = BAYER_PAT_BGGR,
        .mipi_info = {
            .csi_id = 0,
            .mipi_lanes = 4,
            .data_type = 0x2B,
        },
#if defined (CONFIG_MPP_SENSOR_OV13850_ON_CSI0_USE_CHIP_CLK)
        .reg_list = ov13850_mipi_4lane_raw10_2112x1568_30fps_regs,
        .mclk_setting = {
            {
                .mclk_setting_en = K_TRUE,
                .setting.id = CONFIG_MPP_CSI_DEV0_MCLK_NUM,
                .setting.mclk_sel = SENSOR_PLL1_CLK_DIV4,
                .setting.mclk_div = 8,
            },
            {K_FALSE},
            {K_FALSE},
        },
        .sensor_ae_info = &sensor_csi0_ae_info[5],
#else
        .reg_list = ov13850_mipi_4lane_raw10_2112x1568_30fps_regs,
        .mclk_setting = {
            {K_FALSE},
            {K_FALSE},
            {K_FALSE},
        },
        .sensor_ae_info = &sensor_csi0_ae_info[5],
#endif
    },
#endif // CONFIG_MPP_SENSOR_OV13850_ENABLE_4LANE_CONFIGURE
};

#if defined (CONFIG_MPP_SENSOR_OV13850_ON_CSI0_USE_CHIP_CLK)
_Static_assert(CONFIG_MPP_CSI_DEV0_MCLK_NUM >= 1 && (CONFIG_MPP_CSI_DEV0_MCLK_NUM <= 3), "Invalid CONFIG_MPP_CSI_DEV0_MCLK_NUM");
#endif
