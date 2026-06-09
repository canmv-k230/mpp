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

#ifndef _PIXFMT_CONVERT_H_
#define _PIXFMT_CONVERT_H_

#include "k_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Target pixel format for software display bridge conversion.
 * These are the formats that SPI/QSPI/I8080 panels typically accept.
 */
enum bridge_pixel_format {
    BRIDGE_PIXFMT_RGB565,     /* 16bpp, big-endian (R[15:11] G[10:5] B[4:0]) */
    BRIDGE_PIXFMT_RGB888,     /* 24bpp, R G B byte order */
};

/**
 * Get bytes per pixel for a bridge pixel format.
 */
static inline k_u32 bridge_pixfmt_bpp(enum bridge_pixel_format fmt)
{
    switch (fmt) {
    case BRIDGE_PIXFMT_RGB565:
        return 2;
    case BRIDGE_PIXFMT_RGB888:
        return 3;
    default:
        return 0;
    }
}

/**
 * pixfmt_convert_yuv420sp_to_rgb - Convert YUV420SP (NV12) to RGB565 or RGB888
 *
 * @y_virt:   Virtual address of Y plane
 * @uv_virt:  Virtual address of UV (interleaved) plane
 * @dst_virt: Virtual address of destination RGB buffer
 * @width:    Image width in pixels
 * @height:   Image height in pixels
 * @dst_fmt:  Target pixel format (BRIDGE_PIXFMT_RGB565 etc.)
 * @swap_bytes: If true, byte-swap RGB565 pixels before storing them.
 *              Ignored for non-RGB565 formats.
 * @swap_pixels: If true, swap adjacent RGB565 pixels in the output stream.
 *               This is useful when a 32-bit SPI path reverses byte order
 *               within each 32-bit transfer word.
 *
 * Performs BT.601 color space conversion from YUV420 semi-planar (NV12)
 * to the specified RGB format. Uses RVV vector instructions when available,
 * with scalar fixed-point fallback.
 *
 * Returns: 0 on success, negative on error
 */
int pixfmt_convert_yuv420sp_to_rgb(const k_u8 *y_virt, const k_u8 *uv_virt,
                                   k_u8 *dst_virt,
                                   k_u32 width, k_u32 height,
                                   enum bridge_pixel_format dst_fmt,
                                   k_bool swap_bytes,
                                   k_bool swap_pixels);

/**
 * rgb565_swap_bytes_rvv_inplace - In-place byte swap for RGB565 buffers using RVV
 * @buf: Pointer to RGB565 pixel buffer
 * @n_bytes: Size of the buffer in bytes (must be a multiple of 2)
 *
 * This function swaps the high and low bytes of each RGB565 pixel in-place,
 * using RISC-V Vector (RVV) instructions for efficient processing. It is intended
 * for use with SPI panels that require big-endian RGB565 data, when the source
 * data is in little-endian format.
 * The function processes the buffer in chunks of pixels that fit within the
 * vector length, using vrev8.v to swap bytes across the entire vector register. The caller must ensure
 * that the buffer is properly aligned and that n_bytes is even.
 * Returns: None
 */
void rgb565_swap_bytes_rvv_inplace(k_u8* buf, k_u32 n_bytes);

#ifdef __cplusplus
}
#endif

#endif /* _PIXFMT_CONVERT_H_ */
