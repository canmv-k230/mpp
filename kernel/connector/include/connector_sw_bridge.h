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

#ifndef _CONNECTOR_SW_BRIDGE_H_
#define _CONNECTOR_SW_BRIDGE_H_

#include "k_type.h"

#ifdef __cplusplus
extern "C" {
#endif

struct panel_desc;

/**
 * sw_bridge_start - Start the software display bridge
 * @desc: Panel descriptor for the active non-DSI panel
 *
 * Creates a background thread that continuously captures composited VO frames
 * via the Write-Back Channel (WBC), converts from YUV420SP to the panel's
 * target pixel format (RGB565/RGB888), and sends the converted pixels to the
 * panel via bus_ops->send_frame().
 *
 * Only one bridge instance can be active at a time. The bridge uses the global
 * VO WBC driver instance obtained via drv_vo_get_wbc().
 *
 * Returns: 0 on success, negative error code on failure
 */
int sw_bridge_start(const struct panel_desc *desc);

/**
 * sw_bridge_stop - Stop the software display bridge
 *
 * Signals the bridge thread to stop, waits for it to exit, then releases
 * all allocated resources (WBC, conversion buffer VB pool).
 *
 * Returns: 0 on success, negative error code on failure
 */
int sw_bridge_stop(void);

/**
 * sw_bridge_is_running - Check if the software display bridge is active
 *
 * Returns: K_TRUE if bridge thread is running, K_FALSE otherwise
 */
k_bool sw_bridge_is_running(void);

#ifdef __cplusplus
}
#endif

#endif /* _CONNECTOR_SW_BRIDGE_H_ */
