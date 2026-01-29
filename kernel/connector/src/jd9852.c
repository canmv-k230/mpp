#include "connector_dev.h"
#include "io.h"
#include "drv_gpio.h"
#include "k_vo_comm.h"
#include "k_connector_comm.h"

#define DELAY_MS_BACKLIGHT_DEFAULT     200
#define DELAY_MS_BACKLIGHT_FIRST       1
#define MAX_DATA_BYTES 17

static k_s32 g_blacklight_delay_ms = DELAY_MS_BACKLIGHT_FIRST;

// #include "connector_dev.h"
// #include "io.h"
// #include "drv_gpio.h"
// #include "k_vo_comm.h"
// #include "k_connector_comm.h"

// #define DELAY_MS_BACKLIGHT_DEFAULT     200
// #define DELAY_MS_BACKLIGHT_FIRST       1
// #define MAX_DATA_BYTES 17

// static k_s32 g_blacklight_delay_ms = DELAY_MS_BACKLIGHT_FIRST;

// static void send_cmd_safe(const k_u8 *cmd, size_t len)
// {
//     if (len < 4) return;
//     size_t total_data_len = cmd[2];
//     const k_u8 cmd_type = cmd[0];
//     const k_u8 delay_param = cmd[1];

//     rt_kprintf("[JD9852] send_cmd_safe: type=0x%02X delay=%d data_len=%d\n",
//                cmd_type, delay_param, total_data_len);

//     if (total_data_len <= MAX_DATA_BYTES) {
//         connector_send_cmd(cmd, len, K_FALSE);
//     } else {
//         size_t sent = 0;
//         while (sent < total_data_len) {
//             size_t chunk = (total_data_len - sent > MAX_DATA_BYTES)
//                             ? MAX_DATA_BYTES : (total_data_len - sent);
//             k_u8 temp[3 + MAX_DATA_BYTES];
//             temp[0] = cmd_type;
//             temp[1] = delay_param;
//             temp[2] = chunk;
//             memcpy(&temp[3], &cmd[3 + sent], chunk);
//             rt_kprintf("[JD9852] send_cmd_safe chunk: %zu/%zu\n", sent + chunk, total_data_len);
//             connector_send_cmd(temp, 3 + chunk, K_FALSE);
//             sent += chunk;
//         }
//     }
// }

// /* 面板寄存器初始化序列 */
// static void jd9852_panel_init(void)
// {
//     rt_kprintf("[JD9852] ===== Panel Init Start =====\n");

//     const k_u8 seq0[]  = {0x39, 0, 4, 0xDF, 0x98, 0x51, 0xE9};
//     const k_u8 seq1[]  = {0x15, 0, 2, 0xDE, 0x00};
//     const k_u8 seq2[]  = {0x39, 0, 5, 0xB7, 0x16, 0x7D, 0x16, 0x3B};
//     const k_u8 seq3[]  = {0x39, 0, 33, 0xC8,
//         0x3F,0x2B,0x22,0x21,0x23,0x26,0x21,0x22,0x22,0x22,
//         0x1E,0x15,0x11,0x0A,0x09,0x0E,0x3F,0x2B,0x22,0x21,
//         0x24,0x27,0x22,0x22,0x22,0x21,0x1E,0x14,0x11,0x0A,
//         0x09,0x0E};
//     const k_u8 seq4[]  = {0x39, 0, 4, 0xB9, 0x33, 0x08, 0xCC};
//     const k_u8 seq5[]  = {0x39, 0, 9, 0xBB, 0x47,0x7A,0x30,0x40,0x7C,0x60,0x70,0x70};
//     const k_u8 seq6[]  = {0x39, 0, 3, 0xBC, 0x38, 0x3C};
//     const k_u8 seq7[]  = {0x39, 0, 2, 0xC0, 0x31};
//     const k_u8 seq8[]  = {0x15, 0, 2, 0x20, 0x00};
//     const k_u8 seq9[]  = {0x15, 0, 2, 0xC1, 0x12};
//     const k_u8 seq10[] = {0x39, 0, 10, 0xC3, 0x08,0x00,0x0A,0x10,0x08,0x54,0x45,0x71,0x2C};
//     const k_u8 seq11[] = {0x39, 0, 18, 0xC4,
//         0x00,0xA0,0x79,0x0E,0x0A,0x16,0x79,0x0E,0x0A,0x16,
//         0x79,0x0E,0x0A,0x16,0x82,0x00,0x03};
//     const k_u8 seq12[] = {0x39, 0, 7, 0xD0, 0x04,0x0C,0x6A,0x0F,0x00,0x03};
//     const k_u8 seq13[] = {0x39, 0, 3, 0xD7, 0x13, 0x00};
//     const k_u8 seq14[] = {0x15, 0, 2, 0xDE, 0x02};
//     const k_u8 seq15[] = {0x39, 0, 6, 0xB8, 0x1D,0xA0,0x2F,0x2C,0x2B};
//     const k_u8 seq16[] = {0x39, 0, 5, 0xC1, 0x10,0x66,0x66,0x01};
//     const k_u8 seq17[] = {0x15, 0, 2, 0xDE, 0x00};
//     const k_u8 seq18[] = {0x05, 0, 1, 0x11};
//     const k_u8 seq19[] = {0x05, 120, 1, 0x00}; 
//     const k_u8 seq_pixel[] = {0x15, 0, 2, 0x3A, 0x55}; // RGB565
//     const k_u8 seq20[] = {0x05, 0, 1, 0x29}; // Display ON

//     //  0xA0, 0x20, 0xE0
//     const k_u8 seq_rotate_90_deg[] = {0x15, 0, 2, 0x36, 0x60}; 

//     const k_u8* cmds[] = {
//         seq0, seq1, seq2, seq3, seq4, seq5, seq6, seq7, seq8,
//         seq9, seq10, seq11, seq12, seq13, seq14, seq15, seq16,
//         seq17, seq18, seq19, 
//         seq_rotate_90_deg,  // 
//         seq_pixel,         
//         seq20              
//     };
//     size_t sizes[] = {
//         sizeof(seq0), sizeof(seq1), sizeof(seq2), sizeof(seq3),
//         sizeof(seq4), sizeof(seq5), sizeof(seq6), sizeof(seq7), sizeof(seq8),
//         sizeof(seq9), sizeof(seq10), sizeof(seq11), sizeof(seq12), sizeof(seq13),
//         sizeof(seq14), sizeof(seq15), sizeof(seq16), sizeof(seq17), sizeof(seq18),
//         sizeof(seq19),
//         sizeof(seq_rotate_90_deg), 
//         sizeof(seq_pixel),
//         sizeof(seq20)
//     };

//     for (int i = 0; i < sizeof(cmds)/sizeof(cmds[0]); i++) {
//         rt_kprintf("[JD9852] Sending cmd %d/%d\n", i+1, (int)(sizeof(cmds)/sizeof(cmds[0])));
//         send_cmd_safe(cmds[i], sizes[i]);
//     }

//     rt_kprintf("[JD9852] Sleep Out delay 120ms\n");
//     rt_thread_mdelay(120); 

//     /* 显示模式 RGB565 */
//     rt_kprintf("[JD9852] Setting Pixel Format RGB565\n");
//     send_cmd_safe(seq_pixel, sizeof(seq_pixel)); 

//     /* Display ON */
//     rt_kprintf("[JD9852] Display ON delay 50ms\n");
//     send_cmd_safe(seq20, sizeof(seq20)); 
//     rt_thread_mdelay(50);

//     rt_kprintf("[JD9852] ===== Panel Init Done =====\n");
// }

// static void jd9852_power_reset(k_s32 on)
// {
//     k_u8 rst_gpio = CONFIG_MPP_DSI_LCD_RESET_PIN;
//     if (rst_gpio < 0) return;
//     rt_kprintf("[JD9852] power_reset %s\n", on ? "HIGH" : "LOW");
//     kd_pin_mode(rst_gpio, GPIO_DM_OUTPUT);
//     kd_pin_write(rst_gpio, on ? GPIO_PV_HIGH : GPIO_PV_LOW);
// }

// static void jd9852_set_backlight(k_s32 on)
// {
//     k_u8 bl_gpio = CONFIG_MPP_DSI_LCD_BACKLIGHT_PIN;
//     if (bl_gpio < 0) return;
//     rt_kprintf("[JD9852] backlight %s\n", on ? "ON" : "OFF");
//     kd_pin_mode(bl_gpio, GPIO_DM_OUTPUT);
//     kd_pin_write(bl_gpio, on ? GPIO_PV_HIGH : GPIO_PV_LOW);
// }

// static k_s32 jd9852_power_on(void* ctx, k_s32 on)
// {
//     rt_kprintf("[JD9852] power_on %s\n", on ? "ON" : "OFF");
//     if (on) {
//         jd9852_power_reset(0);
//         rt_thread_mdelay(20);
//         jd9852_power_reset(1);
//         rt_thread_mdelay(150);
//         jd9852_set_backlight(0);
//     } else {
//         jd9852_set_backlight(0);
//     }
//     return 0;
// }

// /* PHY 时钟配置 */
// static k_s32 jd9852_set_phy_freq(k_connectori_phy_attr *phy_attr)
// {
//     rt_kprintf("[JD9852] set_phy_freq m=%d n=%d hs_freq=0x%X voc=0x%X\n",
//                phy_attr->m, phy_attr->n, phy_attr->hs_freq, phy_attr->voc);

//     k_vo_mipi_phy_attr attr;
//     memset(&attr, 0, sizeof(attr));
//     attr.m = phy_attr->m;
//     attr.n = phy_attr->n;
//     attr.hs_freq = phy_attr->hs_freq;
//     attr.voc = phy_attr->voc;
//     attr.phy_lan_num = K_DSI_1LAN;
//     connector_set_phy_freq(&attr);
//     return 0;
// }

// /* VO 初始化 */
// static k_s32 jd9852_vo_resolution_init(k_vo_display_resolution *resolution,
//                                        k_u32 bg_color, k_u32 intr_line)
// {
//     rt_kprintf("[JD9852] VO Init: %dx%d bg_color=0x%X intr_line=%u\n",
//                resolution->hdisplay, resolution->vdisplay, bg_color, intr_line);

//     k_vo_pub_attr attr;
//     memset(&attr, 0, sizeof(attr));
//     attr.bg_color  = bg_color;
//     attr.intf_sync = 0; // 自定义时序
//     attr.intf_type = K_VO_INTF_MIPI;
//     attr.sync_info = resolution;

//     connector_set_vo_init();
//     connector_set_vtth_intr(1, intr_line);
//     connector_set_vo_param(&attr);
//     connector_set_vo_enable();
//     return 0;
// }

// /* DSI 初始化 */
// static k_s32 jd9852_dsi_resolution_init(k_connector_info *info)
// {
//     rt_kprintf("[JD9852] DSI Init: %dx%d lan=%d cmd_mode=%d work_mode=%d\n",
//                info->resolution.hdisplay,
//                info->resolution.vdisplay,
//                K_DSI_1LAN,
//                info->cmd_mode,
//                info->work_mode);

//     k_vo_dsi_attr attr;
//     memset(&attr, 0, sizeof(attr));
//     attr.lan_num = K_DSI_1LAN;
//     attr.cmd_mode = info->cmd_mode;
//     attr.lp_div = 8;
//     attr.work_mode = info->work_mode;
//     memcpy(&attr.resolution, &info->resolution, sizeof(k_vo_display_resolution));
//     connector_set_dsi_attr(&attr);

//     jd9852_panel_init();
//     connector_set_dsi_enable(1);

//     if (info->dsi_test_mode) {
//         rt_kprintf("JD9852: Enable DSI test mode (color bar)\n");
//         connector_set_dsi_test_mode();
//     }
//     return 0;
// }

// /* 初始化入口 */
// k_s32 jd9852_init(void *ctx, k_connector_info *info)
// {
//     rt_kprintf("[JD9852] ==== Init Start ====\n");
//     k_s32 ret = 0;
//     if (info->pixclk_div)
//         connector_set_pixclk(info->pixclk_div);

//     ret |= connector_set_cmd_buff_num(info->buff_num);
//     ret |= jd9852_set_phy_freq(&info->phy_attr);
//     ret |= jd9852_vo_resolution_init(&info->resolution, info->bg_color, info->intr_line);
//     ret |= jd9852_dsi_resolution_init(info);
//     jd9852_set_backlight(1);
//     rt_kprintf("[JD9852] ==== Init Done =====\n");
//     return ret;
// }

// /* 获取芯片 ID */
// static k_s32 jd9852_get_chip_id(void* ctx, k_u32* chip_id)
// {
//     *chip_id = 0x985200;
//     rt_kprintf("[JD9852] chip_id=0x%X\n", *chip_id);
//     return 0;
// }

// static k_s32 jd9852_conn_check(void* ctx, k_s32* conn)
// {
//     k_u32 id;
//     jd9852_get_chip_id(ctx, &id);
//     *conn = (id == 0x985200) ? 1 : 0;
//     rt_kprintf("[JD9852] conn_check result=%d\n", *conn);
//     return 0;
// }

// /* 驱动注册 */
// struct connector_driver_dev jd9852_connector_drv = {
//     .connector_name = "jd9852",
//     .connector_func = {
//         .connector_power       = jd9852_power_on,
//         .connector_init        = jd9852_init,
//         .connector_get_chip_id = jd9852_get_chip_id,
//         .connector_conn_check  = jd9852_conn_check,
//     },
// };


/* 面板寄存器初始化序列 */
static void jd9852_panel_init(k_u8 test_mode_en)
{
    const k_u8 init_sequence[]  = {
        0x05,100,1,0x01, // SW RESET
        // 0x39,0,4,0xDF,0x98,0x51,0xE9,
        // 0x39,1,2,0xDE,0x00,
        // 0x39,0,5,0xB7,0x1C,0x7F,0x1C,0x31,
        // 0x39,0,33,0xC8,0x3F,0x29,0x21,0x1B,0x1C,0x21,0x1E,0x20,0x22,0x21,0x20,0x15,0x11,0x0F,0x0C,0x0E,0x3F,0x29,0x21,0x1B,0x1C,0x21,0x1E,0x20,0x22,0x21,0x20,0x15,0x11,0x0F,0x0C,0x0E,
        // 0x39,0,4,0xB9,0x33,0x08,0xCC,
        // 0x39,0,9,0xBB,0x44,0x7A,0xC0,0x30,0x6C,0x60,0x50,0x70,
        // 0x39,0,3,0xBC,0x38,0x3C,
        // 0x39,0,3,0xC0,0x31,0x20,
        // 0x15,0,2,0xC1,0x12,
        // 0x39,1,10,0xC3,0x08,0x00,0x0A,0x10,0x08,0x54,0x45,0x71,0x2C,
        // 0x39,1,18,0xC4,0x00,0xA0,0x79,0x0E,0x0A,0x16,0x79,0x0E,0x0A,0x16,0x79,0x0E,0x0A,0x16,0x82,0x00,0x03,
        // 0x39,1,7,0xD0,0x04,0x0C,0x6B,0x0F,0x07,0x03,
        // 0x39,1,3,0xD7,0x18,0x00,
        // 0x39,1,2,0xDE,0x02,
        // 0x39,1,6,0xB8,0x19,0xA0,0x2F,0x04,0x33,
        // 0x39,1,5,0xB2,0x30,0xFA,0x93,0x6E,
        // 0x39,1,5,0xC1,0x10,0x66,0x66,0x01,
        // 0x39,1,2,0xDE,0x00,
        // 0x39,1,2,0x36,0x60,
        // 0x05,120,1,0x11,
        // 0x39,1,5,0x2A,0x00,0x00,0x01,0x3F,
        // 0x39,1,5,0x2B,0x00,0x00,0x00,0xEF,
        // 0x05,50,1,0x29,

        0x39,0,4,0xDF,0x98,0x51,0xE9,
        0x39,0,2,0xDE,0x00,
        0x39,0,5,0xB7,0x16,0x7D,0x16,0x3B,
        0x39,0,33,0xC8,0x3F,0x2B,0x22,0x21,0x23,0x26,0x21,0x22,0x22,0x22,0x1E,0x15,0x11,0x0A,0x09,0x0E,0x3F,0x2B,0x22,0x21,0x24,0x27,0x22,0x22,0x22,0x21,0x1E,0x14,0x11,0x0A,0x09,0x0E,
        0x39,0,4,0xB9,0x33,0x08,0xCC,
        0x39,0,9,0xBB,0x47,0x7A,0x30,0x40,0x7C,0x60,0x70,0x70,
        0x39,0,3,0xBC,0x38,0x3C,
        0x39,0,2,0xC0,0x31,
        0x39,0,2,0x20,0x00,
        0x39,0,2,0xC1,0x12,
        0x39,0,10,0xC3,0x08,0x00,0x0A,0x10,0x08,0x54,0x45,0x71,0x2C,
        0x39,0,18,0xC4,0x00,0xA0,0x79,0x0E,0x0A,0x16,0x79,0x0E,0x0A,0x16,0x79,0x0E,0x0A,0x16,0x82,0x00,0x03,
        0x39,0,7,0xD0,0x04,0x0C,0x6A,0x0F,0x00,0x03,
        0x39,0,3,0xD7,0x13,0x00,
        0x39,1,2,0xDE,0x02,
        0x39,0,6,0xB8,0x1D,0xA0,0x2F,0x2C,0x2B,
        0x39,0,5,0xC1,0x10,0x66,0x66,0x01,
        0x39,0,2,0xDE,0x00,
        0x39,0,2,0x3A,0x55,
        0x39,100,2,0x11,0x00,
        0x39,0,1,0x29
        };
    connector_send_cmd(init_sequence, sizeof(init_sequence), K_FALSE);
    }

// static void jd9852_power_reset(k_s32 on)
// {
//     k_u8 rst_gpio;
//     if(0 > (rst_gpio = CONFIG_MPP_DSI_LCD_RESET_PIN)) {
//         return;
//     }

//     kd_pin_mode(rst_gpio, GPIO_DM_OUTPUT);

//     if (on)
//         kd_pin_write(rst_gpio, GPIO_PV_HIGH); // GPIO_PV_LOW  GPIO_PV_HIGH
//     else
//         kd_pin_write(rst_gpio, GPIO_PV_LOW); // GPIO_PV_LOW  GPIO_PV_HIGH    
// }

static void jd9852_power_reset(k_s32 on)
{
    k_u8 rst_gpio;

    if(0 > (rst_gpio = CONFIG_MPP_DSI_LCD_RESET_PIN)) {
        return;
    }

    kd_pin_mode(rst_gpio, GPIO_DM_OUTPUT);

    if(on) {
        kd_pin_write(rst_gpio, GPIO_PV_HIGH);
        rt_thread_mdelay(10);
        kd_pin_write(rst_gpio, GPIO_PV_LOW); 
        rt_thread_mdelay(10);
        kd_pin_write(rst_gpio, GPIO_PV_HIGH);
    } else {
        kd_pin_write(rst_gpio, GPIO_PV_LOW); 
    }
}

static void jd9852_set_backlight(k_s32 on)
{
    k_u8 backlight_gpio;
    if(0 > (backlight_gpio = CONFIG_MPP_DSI_LCD_BACKLIGHT_PIN)) {
        return;
    }

    // rt_kprintf("backlight_gpio is %d \n",backlight_gpio);
    //在程序里手动开启
    // kd_pin_mode(backlight_gpio, GPIO_DM_OUTPUT);
    // if (on)
    //     kd_pin_write(backlight_gpio, GPIO_PV_HIGH);
    // else
    //     kd_pin_write(backlight_gpio, GPIO_PV_LOW);    
}

static k_s32 jd9852_power_on(void* ctx, k_s32 on)
{
    k_s32 ret = 0;
    struct connector_driver_dev* dev = ctx;
    // rst vo;
    k230_display_rst();

    if (on) {

        // rst st7701
        jd9852_power_reset(1);
        rt_thread_mdelay(g_blacklight_delay_ms);
        jd9852_power_reset(0);
        rt_thread_mdelay(g_blacklight_delay_ms);
        jd9852_power_reset(1);

        g_blacklight_delay_ms = DELAY_MS_BACKLIGHT_DEFAULT;
        //enable backlight
        jd9852_set_backlight(1);
    } else {
        jd9852_set_backlight(0);
    }

    return ret;   
}

/* PHY 时钟配置 */
static k_s32 jd9852_set_phy_freq(k_connectori_phy_attr *phy_attr)
{
    k_vo_mipi_phy_attr mipi_phy_attr;

    memset(&mipi_phy_attr, 0, sizeof(k_vo_mipi_phy_attr));

    mipi_phy_attr.m = phy_attr->m;
    mipi_phy_attr.n = phy_attr->n;
    mipi_phy_attr.hs_freq = phy_attr->hs_freq;
    mipi_phy_attr.voc = phy_attr->voc;
    mipi_phy_attr.phy_lan_num = K_DSI_1LAN;
    connector_set_phy_freq(&mipi_phy_attr);
    return 0;    
}

/* VO 初始化 */
static k_s32 jd9852_vo_resolution_init(k_vo_display_resolution *resolution,
                                       k_u32 bg_color, k_u32 intr_line)
{
    k_vo_display_resolution vo_resolution;
    k_vo_pub_attr attr;

    memset(&attr, 0, sizeof(k_vo_pub_attr));
    attr.bg_color = bg_color;
    attr.intf_sync = K_VO_OUT_1080P30;
    attr.intf_type = K_VO_INTF_MIPI;
    attr.sync_info = resolution;

    connector_set_vo_init();
    connector_set_vtth_intr(1, intr_line);
    connector_set_vo_param(&attr);
    connector_set_vo_enable();

    return 0;    
}

/* DSI 初始化 */
static k_s32 jd9852_dsi_resolution_init(k_connector_info *info)
{
    k_vo_dsi_attr attr;
    k_vo_display_resolution resolution;

    memset(&attr, 0, sizeof(k_vo_dsi_attr));
    attr.lan_num = K_DSI_1LAN;
    attr.cmd_mode = info->cmd_mode;
    attr.lp_div = 8;
    attr.work_mode = info->work_mode;
    memcpy(&resolution, &info->resolution, sizeof(k_vo_display_resolution));
    memcpy(&attr.resolution, &resolution, sizeof(k_vo_display_resolution));
    connector_set_dsi_attr(&attr);
    if (info->type == JD9852_MIPI_1LAN_240X320_60FPS)
    {
        if(info->screen_test_mode)
            jd9852_panel_init(1);
        else
            jd9852_panel_init(0);
    }

    connector_set_dsi_enable(1);

    if(info->dsi_test_mode == 1)
        connector_set_dsi_test_mode();

    return 0;
}

/* 初始化入口 */
k_s32 jd9852_init(void *ctx, k_connector_info *info)
{
    k_s32 ret = 0;
    struct connector_driver_dev* dev = ctx;

    if(info->pixclk_div != 0)
        connector_set_pixclk(info->pixclk_div);

    ret |= connector_set_cmd_buff_num(info->buff_num);
    ret |= jd9852_set_phy_freq(&info->phy_attr);
    ret |= jd9852_dsi_resolution_init(info);
    ret |= jd9852_vo_resolution_init(&info->resolution, info->bg_color, info->intr_line);

    return ret;

}

/* 获取芯片 ID */
static k_s32 jd9852_get_chip_id(void* ctx, k_u32* chip_id)
{
    k_s32 ret = 0;

    return ret;    
}

static k_s32 jd9852_conn_check(void* ctx, k_s32* conn)
{
    k_s32 ret = 0;

    *conn = 1;

    return ret;    
}

/* 驱动注册 */
struct connector_driver_dev jd9852_connector_drv = {
    .connector_name = "jd9852",
    .connector_func = {
        .connector_power       = jd9852_power_on,
        .connector_init        = jd9852_init,
        .connector_get_chip_id = jd9852_get_chip_id,
        .connector_conn_check  = jd9852_conn_check,
    },
}; 