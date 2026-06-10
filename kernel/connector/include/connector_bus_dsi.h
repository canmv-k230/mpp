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

#ifndef _CONNECTOR_BUS_DSI_H_
#define _CONNECTOR_BUS_DSI_H_

#include <stdint.h>
#include <stdbool.h>

#include "k_vo_comm.h"

#ifdef __cplusplus
extern "C" {
#endif

struct panel_desc;

/**
 * @name MIPI DSI DCS Command Types
 * Standard MIPI DSI packet types for DCS commands
 * @{
 */
#define DSI_DCS_SHORT_WRITE       0x05 /* DCS short write, no parameter */
#define DSI_DCS_SHORT_WRITE_PARAM 0x15 /* DCS short write, 1 parameter */
#define DSI_DCS_LONG_WRITE        0x39 /* DCS long write */
/** @} */

/**
 * @name MIPI DSI Generic Command Types
 * @{
 */
#define DSI_GENERIC_SHORT_WRITE_0_PARAM 0x03
#define DSI_GENERIC_SHORT_WRITE_1_PARAM 0x13
#define DSI_GENERIC_SHORT_WRITE_2_PARAM 0x23
#define DSI_GENERIC_LONG_WRITE          0x29
/** @} */

extern k_s32 dsi_send_cmd_sequence(const struct panel_desc* desc, const k_u8* cmd_seq, size_t cmd_size, k_bool dump);

extern k_u32 dsi_read_chip_id(const struct panel_desc* desc);

extern k_u32 dsi_correct_pclk(k_u32 pclk_hz, k_vo_dsi_lane_num lanes, k_u32 lane_rate_mbps);

// Forward Vo dwc_dsi_xxx
extern void dwc_dsi_init(k_vo_dsi_config* cfg);

extern void dwc_dsi_enable(void);

extern void dwc_dsi_disable(void);

extern int dwc_dsi_send_packet(uint8_t type, uint8_t vc, const uint8_t* data, uint32_t len, bool req_ack);

extern int dwc_dsi_dcs_write(const uint8_t* data, uint32_t len, uint8_t vc);

extern int dwc_dsi_dcs_read(uint8_t addr, uint8_t* buf, uint32_t len, uint8_t vc);

extern int dwc_dsi_generic_write(const uint8_t* data, uint32_t len, uint8_t vc);

extern int dwc_dsi_generic_read(const uint8_t* cmd, uint32_t cmd_len, uint8_t* buf, uint32_t len, uint8_t vc);

// for debug
extern void dwc_dsi_dump_reg_val(void);

#ifdef __cplusplus
}
#endif

#endif /* _CONNECTOR_BUS_DSI_H_ */
