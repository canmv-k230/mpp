/**
 * @file k_sensor_comm.h
 * @author
 * @sxp
 * @version 1.0
 * @date 2023-03-20
 *
 * @copyright
 * Copyright (c) 2023, Canaan Bright Sight Co., Ltd
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
#ifndef __K_CONNECTOR_COMM_H__
#define __K_CONNECTOR_COMM_H__

#include "k_type.h"
#include "k_errno.h"
#include "k_module.h"
#include "k_vo_comm.h"

#ifdef __cplusplus
extern "C" {
#endif /* End of #ifdef __cplusplus */

/** @brief Connector chip identifier (6 bits, 0-63) */
typedef enum {
    K_CHIP_VIRTUAL  = 0,
    K_CHIP_HX8399   = 1,
    K_CHIP_ILI9806  = 2,
    K_CHIP_ILI9881  = 3,
    K_CHIP_NT35516  = 4,
    K_CHIP_NT35532  = 5,
    K_CHIP_GC9503   = 6,
    K_CHIP_ST7102   = 7,
    K_CHIP_AML020T  = 8,
    K_CHIP_ST7701   = 9,
    K_CHIP_JD9852   = 10,
    K_CHIP_LT9611   = 11,
    K_CHIP_ST7789   = 12,
    K_CHIP_NV3030B  = 13,
} k_connector_chip;

/** @brief Connector bus type (2 bits, 0-3) */
typedef enum {
    K_BUS_DSI        = 0,
    K_BUS_HDMI       = 1,
    K_BUS_SPI        = 2,
} k_connector_bus;

/*
 * k_connector_type encodes chip, bus, version, width and height into a 32-bit
 * value, similar to the Linux _IOC() ioctl encoding:
 *
 *   [31:26] chip    (6 bits)  — k_connector_chip
 *   [25:24] bus     (2 bits)  — k_connector_bus
 *   [23:20] version (4 bits)  — variant differentiator
 *   [19:10] width   (10 bits) — horizontal resolution >> 1
 *    [9:0]  height  (10 bits) — vertical resolution >> 1
 */

#define _CONN_CHIP_BITS   6
#define _CONN_BUS_BITS    2
#define _CONN_VER_BITS    4
#define _CONN_W_BITS      10
#define _CONN_H_BITS      10

#define _CONN_H_SHIFT     0
#define _CONN_W_SHIFT     10
#define _CONN_VER_SHIFT   20
#define _CONN_BUS_SHIFT   24
#define _CONN_CHIP_SHIFT  26

#define _CONN_MASK(bits)  ((1U << (bits)) - 1)

/** Compose a k_connector_type value from its fields. */
#define K_CONN_TYPE(chip, bus, w, h, ver)                                       \
    ((k_u32)                                                                    \
     ((((k_u32)(chip) & _CONN_MASK(_CONN_CHIP_BITS)) << _CONN_CHIP_SHIFT) |    \
      (((k_u32)(bus)  & _CONN_MASK(_CONN_BUS_BITS))  << _CONN_BUS_SHIFT)  |    \
      (((k_u32)(ver)  & _CONN_MASK(_CONN_VER_BITS))  << _CONN_VER_SHIFT)  |    \
      (((k_u32)((w) >> 1) & _CONN_MASK(_CONN_W_BITS)) << _CONN_W_SHIFT)   |    \
      (((k_u32)((h) >> 1) & _CONN_MASK(_CONN_H_BITS)) << _CONN_H_SHIFT)))

/** Extract fields from a k_connector_type value. */
#define K_CONN_CHIP(type)    ((k_connector_chip)(((type) >> _CONN_CHIP_SHIFT) & _CONN_MASK(_CONN_CHIP_BITS)))
#define K_CONN_BUS(type)     ((k_connector_bus)(((type) >> _CONN_BUS_SHIFT) & _CONN_MASK(_CONN_BUS_BITS)))
#define K_CONN_VER(type)     (((type) >> _CONN_VER_SHIFT) & _CONN_MASK(_CONN_VER_BITS))
#define K_CONN_WIDTH(type)   ((((type) >> _CONN_W_SHIFT) & _CONN_MASK(_CONN_W_BITS)) << 1)
#define K_CONN_HEIGHT(type)  ((((type) >> _CONN_H_SHIFT) & _CONN_MASK(_CONN_H_BITS)) << 1)

typedef k_u32 k_connector_type;

/* DSI panels */
#define HX8399_1080_1920_DSI_V1     K_CONN_TYPE(K_CHIP_HX8399,  K_BUS_DSI,  1080, 1920, 1)
#define ILI9806_480_800_DSI_V1      K_CONN_TYPE(K_CHIP_ILI9806,  K_BUS_DSI,  480,  800, 1)
#define ILI9881_800_1280_DSI_V1     K_CONN_TYPE(K_CHIP_ILI9881,  K_BUS_DSI,  800, 1280, 1)
#define NT35516_536_960_DSI_V1      K_CONN_TYPE(K_CHIP_NT35516,  K_BUS_DSI,  536,  960, 1)
#define NT35532_1080_1920_DSI_V1    K_CONN_TYPE(K_CHIP_NT35532,  K_BUS_DSI, 1080, 1920, 1)
#define GC9503_480_800_DSI_V1       K_CONN_TYPE(K_CHIP_GC9503,   K_BUS_DSI,  480,  800, 1)
#define ST7102_480_640_DSI_V1       K_CONN_TYPE(K_CHIP_ST7102,   K_BUS_DSI,  480,  640, 1)
#define AML020T_480_360_DSI_V1      K_CONN_TYPE(K_CHIP_AML020T,  K_BUS_DSI,  480,  360, 1)

/* ST7701 variants */
#define ST7701_480_800_DSI_V1       K_CONN_TYPE(K_CHIP_ST7701,   K_BUS_DSI,  480,  800, 1)
#define ST7701_480_854_DSI_V1       K_CONN_TYPE(K_CHIP_ST7701,   K_BUS_DSI,  480,  854, 1)
#define ST7701_480_640_DSI_V1       K_CONN_TYPE(K_CHIP_ST7701,   K_BUS_DSI,  480,  640, 1)
#define ST7701_368_544_DSI_V1       K_CONN_TYPE(K_CHIP_ST7701,   K_BUS_DSI,  368,  544, 1)

/* JD9852 */
#define JD9852_240_320_DSI_V1       K_CONN_TYPE(K_CHIP_JD9852,   K_BUS_DSI,  240,  320, 1)

/* LT9611 HDMI */

/* K230 Custom Timings */
#define LT9611_1920_1080_HDMI_V1    K_CONN_TYPE(K_CHIP_LT9611,  K_BUS_HDMI, 1920, 1080, 1) /* 30fps */
#define LT9611_1920_1080_HDMI_V2    K_CONN_TYPE(K_CHIP_LT9611,  K_BUS_HDMI, 1920, 1080, 2) /* 60fps */
#define LT9611_1280_720_HDMI_V1     K_CONN_TYPE(K_CHIP_LT9611,  K_BUS_HDMI, 1280,  720, 1) /* 60fps */
#define LT9611_1280_720_HDMI_V2     K_CONN_TYPE(K_CHIP_LT9611,  K_BUS_HDMI, 1280,  720, 2) /* 50fps */
#define LT9611_1280_720_HDMI_V3     K_CONN_TYPE(K_CHIP_LT9611,  K_BUS_HDMI, 1280,  720, 3) /* 30fps */
#define LT9611_640_480_HDMI_V1      K_CONN_TYPE(K_CHIP_LT9611,  K_BUS_HDMI,  640,  480, 1) /* 60fps */
/* VESA Timings */
#define LT9611_1920_1080_HDMI_V3    K_CONN_TYPE(K_CHIP_LT9611,  K_BUS_HDMI, 1920, 1080, 3) /* 30fps */
#define LT9611_1920_1080_HDMI_V4    K_CONN_TYPE(K_CHIP_LT9611,  K_BUS_HDMI, 1920, 1080, 4) /* 60fps */
#define LT9611_1280_720_HDMI_V4     K_CONN_TYPE(K_CHIP_LT9611,  K_BUS_HDMI, 1280,  720, 4) /* 60fps */
#define LT9611_1280_720_HDMI_V5     K_CONN_TYPE(K_CHIP_LT9611,  K_BUS_HDMI, 1280,  720, 5) /* 50fps */
#define LT9611_1280_720_HDMI_V6     K_CONN_TYPE(K_CHIP_LT9611,  K_BUS_HDMI, 1280,  720, 6) /* 30fps */
#define LT9611_640_480_HDMI_V2      K_CONN_TYPE(K_CHIP_LT9611,  K_BUS_HDMI,  640,  480, 2) /* 60fps */

/* SPI / SPI-like panels */
#define ST7789_320_240_SPI_V1       K_CONN_TYPE(K_CHIP_ST7789,   K_BUS_SPI,  320,  240, 1)
#define ST7789_240_280_I8080_V1     K_CONN_TYPE(K_CHIP_ST7789,   K_BUS_SPI,  240,  280, 2)
#define NV3030B_240_240_QSPI_V1     K_CONN_TYPE(K_CHIP_NV3030B,  K_BUS_SPI,  240,  240, 1)

/* Virtual / special */
#define VIRTUAL_DISPLAY_DEVICE      K_CONN_TYPE(K_CHIP_VIRTUAL,  0,            0,    0, 0)
#define CONNECTOR_BUTT              ((k_u32)0xFFFFFFFF)

#ifndef K_CONNECTOR_NO_COMPAT
#include "k_connector_compat.h"
#endif

typedef enum {
    K_CONNECTOR_BL_MODE_OFF = 0,
    K_CONNECTOR_BL_MODE_ON = 1,
    K_CONNECTOR_BL_MODE_PWM = 2,
} k_connector_backlight_mode;

typedef struct {
    k_connector_backlight_mode mode;
    k_u32 duty;
} k_connector_backlight_attr;

typedef struct {
    char connector_name[32];
    k_connector_type type;
    union {
        k_vo_timing timing;
        k_vo_timing resolution; // for compatible
    };
    k_u32 bg_color;
} k_connector_info;

typedef struct {
    k_u8 cmd_type;
    k_u8 cmd_delay;
    k_u8 cmd_size;
    k_u8 cmd_data[0];
} k_connector_cmd_slice;

#define CONNECTOR_CMD_SEQUENCE(type, delay, ...)                                                                       \
    (type), (delay), sizeof((k_u8[]) { __VA_ARGS__ }) / sizeof(k_u8), __VA_ARGS__

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
