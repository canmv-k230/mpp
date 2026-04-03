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

#ifndef _CONNECTOR_BUS_H_
#define _CONNECTOR_BUS_H_

#include "connector_bus_dsi.h"
#include "connector_bus_spi.h"
#include "k_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
struct panel_desc;

/**
 * Panel bus type enumeration
 *
 * Defines all supported display bus interfaces:
 * - DSI: MIPI Display Serial Interface
 * - SPI: Standard Serial Peripheral Interface
 * - I8080_SPI: Intel 8080 parallel protocol tunneled through SPI
 * - QSPI: Quad Serial Peripheral Interface (4-bit parallel SPI)
 */
enum panel_bus_type {
    PANEL_BUS_DSI, /* MIPI DSI */
    PANEL_BUS_SPI, /* Standard SPI */
    PANEL_BUS_I8080_SPI, /* 8080 parallel over SPI */
    PANEL_BUS_QSPI, /* Quad SPI (4-bit parallel) */
    PANEL_BUS_NONE, /* No bus */
};

/**
 * Bus operations vtable
 *
 * Common interface implemented by all bus drivers (DSI, SPI, I8080, QSPI).
 * Provides unified abstraction for panel initialization and communication.
 *
 * Design principles:
 * - All operations receive panel_desc for bus-specific configuration access
 * - Return 0 on success, negative error code on failure
 * - Optional operations can be NULL (framework checks before calling)
 */
struct panel_bus_ops {
    /**
     * init - Initialize bus hardware and configure timing
     * @desc: Panel descriptor containing bus-specific configuration
     *
     * Called during panel power-on to configure bus hardware (PHY, clocks,
     * timing parameters). For DSI: configures PHY frequency and DSI attributes.
     * For SPI: configures SPI mode, speed, CS pin.
     *
     * MANDATORY for all bus types.
     *
     * Returns: 0 on success, negative error code on failure
     */
    int (*init)(const struct panel_desc* desc);

    /**
     * enable - Enable bus output
     * @desc: Panel descriptor (for bus context)
     *
     * Enables bus transmission (e.g., DSI enable, SPI CS assert).
     * Called after panel initialization sequence completes.
     *
     * OPTIONAL: Can be NULL if no enable step required.
     *
     * Returns: 0 on success, negative error code on failure
     */
    int (*enable)(const struct panel_desc* desc);

    /**
     * disable - Disable bus output
     * @desc: Panel descriptor (for bus context)
     *
     * Disables bus transmission (e.g., DSI disable, SPI CS deassert).
     * Called during panel power-off.
     *
     * OPTIONAL: Can be NULL if no disable step required.
     *
     * Returns: 0 on success, negative error code on failure
     */
    int (*disable)(const struct panel_desc* desc);

    /**
     * send_cmd - Send a command byte to the panel
     * @desc: Panel descriptor (for bus context)
     * @cmd: Command byte
     * @data: Parameter data (can be NULL if len == 0)
     * @len: Length of parameter data
     *
     * Sends a command with optional parameter bytes via the bus.
     * For SPI/QSPI/I8080: toggles DC pin LOW for command, HIGH for data.
     * For DSI: not used (DSI uses hardware video stream).
     *
     * OPTIONAL: Required only for software-driven buses (SPI, QSPI, I8080).
     *
     * Returns: 0 on success, negative error code on failure
     */
    int (*send_cmd)(const struct panel_desc* desc, k_u8 cmd, const k_u8* data, k_u32 len);

    /**
     * send_frame - Send pixel data to the panel
     * @desc: Panel descriptor (for bus context)
     * @data: Pointer to pixel data buffer (virtual address)
     * @size: Size of pixel data in bytes
     *
     * Transfers a full frame of pixel data to the panel via the bus.
     * For SPI: DC pin HIGH, then SPI DMA transfer.
     * For DSI: not used (DSI uses hardware video stream).
     *
     * OPTIONAL: Required only for software-driven buses (SPI, QSPI, I8080).
     *
     * Returns: 0 on success, negative error code on failure
     */
    int (*send_frame)(const struct panel_desc* desc, void* data, k_u32 size);
};

/* Bus driver registration functions (implemented by each bus driver) */
extern const struct panel_bus_ops dsi_bus_ops;
#ifdef CONFIG_MPP_ENABLE_SPI_LCD
extern const struct panel_bus_ops spi_bus_ops;
#endif
#ifdef CONFIG_MPP_ENABLE_OSPI_LCD
extern const struct panel_bus_ops i8080_spi_bus_ops;
#endif
#ifdef CONFIG_MPP_ENABLE_QSPI_LCD
extern const struct panel_bus_ops qspi_bus_ops;
#endif

#ifdef __cplusplus
}
#endif

#endif /* _CONNECTOR_BUS_H_ */
