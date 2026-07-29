/* Copyright (c) 2026, Canaan Bright Sight Co., Ltd
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _CONNECTOR_PANEL_H_
#define _CONNECTOR_PANEL_H_

#include "k_connector_comm.h"
#include "k_vo_comm.h"

#include "connector_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VO_PIXEL_CLOCK_HZ 594000000UL

#define PANEL_BG_COLOR_BLACK 0x00000000
#define PANEL_BG_COLOR_WHITE 0x00FFFFFF

#define CONNECTOR_SW_BRIDGE_FLAG_SWAP_RGB565_BYTE_ORDER (1 << 0)
#define CONNECTOR_SW_BRIDGE_FLAG_SWAP_RGB565_PIXEL_ORDER (1 << 1)

/* Forward declarations */
struct panel_desc;

struct panel_drv {
    const char* connector_name;

    const struct panel_desc** panel_variants;
    const struct panel_desc*  active_panel;
};

/**
 * Panel GPIO configuration sub-structure
 */
struct panel_gpio_config {
    k_s32 reset_pin; /* Reset GPIO pin number, -1 if not used */
    k_u32 reset_delay_ms; /* Delay in ms after reset deassertion */
    k_u32 backlight_delay_ms; /* Delay in ms before backlight enable (default 0) */

    k_s32  backlight_pin; /* Backlight GPIO pin number, -1 if not used */
    k_bool reset_active_low; /* Reset active low (K_TRUE) or active high (K_FALSE) */
    k_bool backlight_active_low; /* Backlight active low (K_TRUE) or active high (K_FALSE) */
};

struct panel_dsi_config {
    k_vo_dsi_lane_num   lanes;
    k_u32               cmd_mode;
    k_vo_dsi_video_mode video_mode;
    k_u32               lane_rate_mbps; /* Optional fixed per-lane bit rate; 0 = derive from pclk */
    k_u8                vc_id;
    k_u8                lp_cmd_speed_mhz;
};

struct panel_sw_bridge_base {
    k_u32 pixel_format;
    k_u32 flag; /* see CONNECTOR_SW_BRIDGE_FLAG_* */
    k_u8  fps; /* Frames per second */
    k_u16 x_offset; /* Optional GRAM x offset for SPI-like panels */
    k_u16 y_offset; /* Optional GRAM y offset for SPI-like panels */
};

struct panel_spi_config {
    struct panel_sw_bridge_base base; // must be first for common handling in sw_bridge

    const char* spi_dev_name; /* Device name to register, e.g. "lcd_spi" */
    k_u32       spi_speed_hz;
    k_u8        spi_mode;
};

struct panel_i8080_spi_config {
    struct panel_sw_bridge_base base; // must be first for common handling in sw_bridge

    const char* spi_dev_name; /* Device name to register, e.g. "lcd_i8080" */
    k_u32       spi_speed_hz;
    k_u8        spi_mode;
    k_u32       bus_width;
};

struct panel_qspi_config {
    struct panel_sw_bridge_base base; // must be first for common handling in sw_bridge

    const char* qspi_dev_name; /* Device name to register, e.g. "lcd_qspi" */
    k_u32       qspi_speed_hz;
    k_u8        qspi_mode;
};

/**
 * Panel operations structure
 * Defines mandatory bus-specific operations for panel initialization and teardown
 */
struct panel_ops {
    int (*reset)(const struct panel_desc* desc);

    /**
     * init - Panel initialization function (MANDATORY)
     * @desc: Panel descriptor
     *
     * Sends panel-specific initialization commands via the appropriate bus.
     * For DSI panels: typically calls dwc_dsi_dcs_write() or dwc_dsi_generic_write()
     * For SPI panels: uses SPI transfer functions with DC pin toggling
     *
     * Returns: 0 on success, negative error code on failure
     */
    int (*init)(const struct panel_desc* desc);

    /**
     * power_off - Panel power-off function (OPTIONAL)
     * @desc: Panel descriptor
     *
     * Performs panel-specific power-off sequence (e.g., display-off commands)
     * before GPIO teardown. If NULL, framework uses default GPIO-only teardown.
     *
     * Returns: 0 on success, negative error code on failure
     */
    int (*power_off)(const struct panel_desc* desc);

    /**
     * read_chip_id - Read panel driver chip ID (OPTIONAL)
     * @desc: Panel descriptor
     *
     * Reads panel driver chip ID using panel-specific register sequence.
     * Different panels may use different DCS registers or vendor commands.
     *
     * Common implementations:
     * - DSI panels: Read DCS 0xDA/0xDB/0xDC registers
     * - SPI panels: Send 0x04 Read Display ID command
     * - Custom panels: Vendor-specific read sequences
     *
     * Returns: 24-bit chip ID on success, 0 if not supported/not readable
     */
    k_u32 (*read_chip_id)(const struct panel_desc* desc);
};

/**
 * Panel descriptor structure
 * Canonical descriptor for panel configuration and initialization
 */
struct panel_desc {
    const char*         name;
    k_u32               connector_type;
    enum panel_bus_type bus_type;

    k_vo_timing timing;

    k_u32 bg_color;

    struct panel_gpio_config gpio;

    union {
        struct panel_dsi_config       dsi;
        struct panel_spi_config       spi;
        struct panel_i8080_spi_config i8080_spi;
        struct panel_qspi_config      qspi;
    } bus;

    const struct panel_bus_ops* bus_ops;

    const struct panel_ops* ops;
};

extern k_bool g_panel_init_status;

k_u32 panel_correct_pclk(k_u32 pclk);

int panel_calculate_fps(const k_vo_timing* timing);

int panel_generic_reset(const struct panel_desc* desc);

int panel_generic_backlight(const struct panel_desc* desc, k_u32 mode, k_u32 duty);

k_s32 panel_generic_power_on(struct panel_desc* desc);

k_s32 panel_generic_power_off(const struct panel_desc* desc);

#ifdef __cplusplus
}
#endif

#endif /* _CONNECTOR_PANEL_H_ */
