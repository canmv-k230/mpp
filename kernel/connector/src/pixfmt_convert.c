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

#include <rtthread.h>
#include <string.h>

#include "pixfmt_convert.h"

/*
 * BT.601 YUV->RGB fixed-point conversion (Q10 = 1024 scale):
 *
 *   R = Y + 1.402  * (V - 128)  =>  Y + (1436 * (V-128)) >> 10
 *   G = Y - 0.3441 * (U - 128) - 0.7141 * (V - 128)
 *                                =>  Y - (352 * (U-128) + 731 * (V-128)) >> 10
 *   B = Y + 1.772  * (U - 128)  =>  Y + (1815 * (U-128)) >> 10
 */
#define FP_SHIFT 10
#define CV_R_V   1613 // 1.5748 * 1024
#define CV_G_U   192 // 0.1873 * 1024
#define CV_G_V   479 // 0.4681 * 1024
#define CV_B_U   1900 // 1.8556 * 1024

static inline k_u8 clamp_u8(k_s32 val)
{
    if (val < 0)
        return 0;
    if (val > 255)
        return 255;
    return (k_u8)val;
}

static inline k_u16 rgb565_swap_bytes_scalar(k_u16 pixel)
{
    return (k_u16)((pixel << 8) | (pixel >> 8));
}

/*
 * RVV-optimized: YUV420SP (NV12) -> RGB565
 *
 * Uses vlseg2e8 to deinterleave UV pairs and Y even/odd pairs in hardware,
 * widening multiply (vwmul/vwmacc) for BT.601 coefficients, and vsseg2e16
 * to store interleaved even/odd pixel output.
 *
 * Each iteration handles vl pixel-pairs (2*vl pixels) where vl is set by
 * vsetvli for e16/m1 (= 8 on K230 with VLEN=128).
 */
static void yuv420sp_to_rgb565_rvv(const k_u8* y_ptr, const k_u8* uv_ptr,
                                   k_u8* dst, k_u32 width, k_u32 height,
                                   k_bool swap_bytes, k_bool swap_pixels)
{
    for (k_u32 row = 0; row < height; row++) {
        const k_u8* yp  = y_ptr + row * width;
        const k_u8* uvp = uv_ptr + (row >> 1) * width;
        k_u16*      dp  = (k_u16*)(dst + row * width * 2);
        k_u32       npairs = width >> 1;

        while (npairs > 0) {
            size_t vl;

            __asm__ volatile(
                /* Set vl for e16/m1 — determines how many pixel pairs */
                "vsetvli  %[vl], %[np], e16, m1, ta, ma\n\t"

                /* Load UV and Y with e8/mf2 segment loads.
                 * vlseg2e8 deinterleaves:
                 *   UV -> v0={U0,U1,...}, v1={V0,V1,...}
                 *    Y -> v2={Ye0,Ye1,...}, v3={Yo0,Yo1,...}  */
                "vsetvli  zero, %[vl], e8, mf2, ta, ma\n\t"
                "vlseg2e8.v v0, (%[uvp])\n\t"
                "vlseg2e8.v v2, (%[yp])\n\t"

                /* Widen u8 -> u16, bias UV to signed */
                "vsetvli  zero, %[vl], e16, m1, ta, ma\n\t"
                "vzext.vf2 v4, v0\n\t"
                "vzext.vf2 v5, v1\n\t"
                "vzext.vf2 v6, v2\n\t"
                "vzext.vf2 v7, v3\n\t"
                "li  t0, 128\n\t"
                "vsub.vx v4, v4, t0\n\t"
                "vsub.vx v5, v5, t0\n\t"

                /* r_add = (1613 * (V-128)) >> 10 */
                "li  t0, 1613\n\t"
                "vwmul.vx  v8, v5, t0\n\t"
                "vnsra.wi  v0, v8, 10\n\t"

                /* g_sub = (192*(U-128) + 479*(V-128)) >> 10 */
                "li  t0, 192\n\t"
                "vwmul.vx  v8, v4, t0\n\t"
                "li  t0, 479\n\t"
                "vwmacc.vx v8, t0, v5\n\t"
                "vnsra.wi  v1, v8, 10\n\t"

                /* b_add = (1900 * (U-128)) >> 10 */
                "li  t0, 1900\n\t"
                "vwmul.vx  v8, v4, t0\n\t"
                "vnsra.wi  v2, v8, 10\n\t"

                /* Even pixels (Y in v6) -> RGB565 in v12 */
                "li  t0, 255\n\t"
                "vadd.vv v3, v6, v0\n\t"
                "vmax.vx v3, v3, zero\n\t"
                "vmin.vx v3, v3, t0\n\t"
                "vsub.vv v10, v6, v1\n\t"
                "vmax.vx v10, v10, zero\n\t"
                "vmin.vx v10, v10, t0\n\t"
                "vadd.vv v11, v6, v2\n\t"
                "vmax.vx v11, v11, zero\n\t"
                "vmin.vx v11, v11, t0\n\t"
                "vsrl.vi v3, v3, 3\n\t"
                "vsll.vi v3, v3, 11\n\t"
                "vsrl.vi v10, v10, 2\n\t"
                "vsll.vi v10, v10, 5\n\t"
                "vsrl.vi v11, v11, 3\n\t"
                "vor.vv  v3, v3, v10\n\t"
                "vor.vv  v12, v3, v11\n\t"

                /* Odd pixels (Y in v7) -> RGB565 in v13 */
                "vadd.vv v3, v7, v0\n\t"
                "vmax.vx v3, v3, zero\n\t"
                "vmin.vx v3, v3, t0\n\t"
                "vsub.vv v10, v7, v1\n\t"
                "vmax.vx v10, v10, zero\n\t"
                "vmin.vx v10, v10, t0\n\t"
                "vadd.vv v11, v7, v2\n\t"
                "vmax.vx v11, v11, zero\n\t"
                "vmin.vx v11, v11, t0\n\t"
                "vsrl.vi v3, v3, 3\n\t"
                "vsll.vi v3, v3, 11\n\t"
                "vsrl.vi v10, v10, 2\n\t"
                "vsll.vi v10, v10, 5\n\t"
                "vsrl.vi v11, v11, 3\n\t"
                "vor.vv  v3, v3, v10\n\t"
                "vor.vv  v13, v3, v11\n\t"

                : [vl] "=&r"(vl)
                : [np] "r"(npairs),
                  [yp] "r"(yp), [uvp] "r"(uvp), [dp] "r"(dp)
                : "t0",
                  "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
                  "v8", "v9", "v10", "v11", "v12", "v13",
                  "memory"
            );

            if (swap_bytes && swap_pixels) {
                __asm__ volatile(
                    "vsetvli  zero, %[vl], e16, m1, ta, ma\n\t"
                    "vsll.vi  v3, v12, 8\n\t"
                    "vsrl.vi  v10, v12, 8\n\t"
                    "vor.vv   v12, v3, v10\n\t"
                    "vsll.vi  v3, v13, 8\n\t"
                    "vsrl.vi  v10, v13, 8\n\t"
                    "vor.vv   v13, v3, v10\n\t"
                    "vmv.v.v  v14, v12\n\t"
                    "vmv.v.v  v12, v13\n\t"
                    "vmv.v.v  v13, v14\n\t"
                    "vsseg2e16.v v12, (%[dp])\n\t"
                    :
                    : [vl] "r"(vl), [dp] "r"(dp)
                    : "v3", "v10", "v12", "v13", "v14", "memory"
                );
            } else if (swap_bytes) {
                __asm__ volatile(
                    "vsetvli  zero, %[vl], e16, m1, ta, ma\n\t"
                    "vsll.vi  v3, v12, 8\n\t"
                    "vsrl.vi  v10, v12, 8\n\t"
                    "vor.vv   v12, v3, v10\n\t"
                    "vsll.vi  v3, v13, 8\n\t"
                    "vsrl.vi  v10, v13, 8\n\t"
                    "vor.vv   v13, v3, v10\n\t"
                    "vsseg2e16.v v12, (%[dp])\n\t"
                    :
                    : [vl] "r"(vl), [dp] "r"(dp)
                    : "v3", "v10", "v12", "v13", "memory"
                );
            } else if (swap_pixels) {
                __asm__ volatile(
                    "vsetvli  zero, %[vl], e16, m1, ta, ma\n\t"
                    "vmv.v.v  v14, v12\n\t"
                    "vmv.v.v  v12, v13\n\t"
                    "vmv.v.v  v13, v14\n\t"
                    "vsseg2e16.v v12, (%[dp])\n\t"
                    :
                    : [vl] "r"(vl), [dp] "r"(dp)
                    : "v12", "v13", "v14", "memory"
                );
            } else {
                __asm__ volatile(
                    "vsetvli  zero, %[vl], e16, m1, ta, ma\n\t"
                    "vsseg2e16.v v12, (%[dp])\n\t"
                    :
                    : [vl] "r"(vl), [dp] "r"(dp)
                    : "v12", "v13", "memory"
                );
            }

            yp     += vl * 2;
            uvp    += vl * 2;
            dp     += vl * 2;
            npairs -= vl;
        }
    }
}

/*
 * RVV-optimized: YUV420SP (NV12) -> RGB888
 *
 * Similar to the RGB565 path, but stores each pixel-pair as
 * Re,Ge,Be, Ro,Go,Bo using a 6-field segmented byte store.
 */
static void yuv420sp_to_rgb888_rvv(const k_u8* y_ptr, const k_u8* uv_ptr, k_u8* dst, k_u32 width, k_u32 height)
{
    for (k_u32 row = 0; row < height; row++) {
        const k_u8* yp     = y_ptr + row * width;
        const k_u8* uvp    = uv_ptr + (row >> 1) * width;
        k_u8*       dp     = dst + row * width * 3;
        k_u32       npairs = width >> 1;

        while (npairs > 0) {
            size_t vl;

            __asm__ volatile(
                /* Set vl for pixel-pairs, then load UV and even/odd Y bytes. */
                "vsetvli  %[vl], %[np], e16, m1, ta, ma\n\t"
                "vsetvli  zero, %[vl], e8, mf2, ta, ma\n\t"
                "vlseg2e8.v v0, (%[uvp])\n\t"
                "vlseg2e8.v v2, (%[yp])\n\t"

                /* Widen u8 -> u16 and bias UV into signed domain. */
                "vsetvli  zero, %[vl], e16, m1, ta, ma\n\t"
                "vzext.vf2 v4, v0\n\t"
                "vzext.vf2 v5, v1\n\t"
                "vzext.vf2 v6, v2\n\t"
                "vzext.vf2 v7, v3\n\t"
                "li  t0, 128\n\t"
                "vsub.vx v4, v4, t0\n\t"
                "vsub.vx v5, v5, t0\n\t"

                /* r_add = (CV_R_V * (V - 128)) >> FP_SHIFT */
                "li  t0, %c[cv_r_v]\n\t"
                "vwmul.vx  v8, v5, t0\n\t"
                "vnsra.wi  v0, v8, %c[fp_shift]\n\t"

                /* g_sub = (CV_G_U * U + CV_G_V * V) >> FP_SHIFT */
                "li  t0, %c[cv_g_u]\n\t"
                "vwmul.vx  v8, v4, t0\n\t"
                "li  t0, %c[cv_g_v]\n\t"
                "vwmacc.vx v8, t0, v5\n\t"
                "vnsra.wi  v1, v8, %c[fp_shift]\n\t"

                /* b_add = (CV_B_U * (U - 128)) >> FP_SHIFT */
                "li  t0, %c[cv_b_u]\n\t"
                "vwmul.vx  v8, v4, t0\n\t"
                "vnsra.wi  v2, v8, %c[fp_shift]\n\t"

                /* Even pixel RGB, clamped to 0..255 in v12/v13/v14. */
                "li  t0, 255\n\t"
                "vadd.vv v12, v6, v0\n\t"
                "vmax.vx v12, v12, zero\n\t"
                "vmin.vx v12, v12, t0\n\t"
                "vsub.vv v13, v6, v1\n\t"
                "vmax.vx v13, v13, zero\n\t"
                "vmin.vx v13, v13, t0\n\t"
                "vadd.vv v14, v6, v2\n\t"
                "vmax.vx v14, v14, zero\n\t"
                "vmin.vx v14, v14, t0\n\t"

                /* Odd pixel RGB, clamped to 0..255 in v15/v16/v17. */
                "vadd.vv v15, v7, v0\n\t"
                "vmax.vx v15, v15, zero\n\t"
                "vmin.vx v15, v15, t0\n\t"
                "vsub.vv v16, v7, v1\n\t"
                "vmax.vx v16, v16, zero\n\t"
                "vmin.vx v16, v16, t0\n\t"
                "vadd.vv v17, v7, v2\n\t"
                "vmax.vx v17, v17, zero\n\t"
                "vmin.vx v17, v17, t0\n\t"

                /* Narrow to u8 and store Re,Ge,Be,Ro,Go,Bo per pair. */
                "vsetvli  zero, %[vl], e8, mf2, ta, ma\n\t"
                "vnsrl.wi v0, v12, 0\n\t"
                "vnsrl.wi v1, v13, 0\n\t"
                "vnsrl.wi v2, v14, 0\n\t"
                "vnsrl.wi v3, v15, 0\n\t"
                "vnsrl.wi v4, v16, 0\n\t"
                "vnsrl.wi v5, v17, 0\n\t"
                "vsseg6e8.v v0, (%[dp])\n\t"

                : [vl] "=&r"(vl)
                : [np] "r"(npairs),
                  [yp] "r"(yp), [uvp] "r"(uvp), [dp] "r"(dp),
                  [fp_shift] "i"(FP_SHIFT),
                  [cv_r_v] "i"(CV_R_V),
                  [cv_g_u] "i"(CV_G_U),
                  [cv_g_v] "i"(CV_G_V),
                  [cv_b_u] "i"(CV_B_U)
                : "t0",
                  "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
                  "v8", "v12", "v13", "v14", "v15", "v16", "v17",
                  "memory"
            );

            yp     += vl * 2;
            uvp    += vl * 2;
            dp     += vl * 6;
            npairs -= vl;
        }
    }
}

/*
 * Scalar fallback: YUV420SP (NV12) -> RGB565 big-endian
 * Processes 2 pixels at a time (sharing UV), row by row.
 */
static void yuv420sp_to_rgb565_scalar(const k_u8* y_ptr, const k_u8* uv_ptr, k_u8* dst, k_u32 width, k_u32 height,
                                      k_bool swap_bytes, k_bool swap_pixels)
{
    k_u32 y_stride   = width;
    k_u32 uv_stride  = width; /* NV12: interleaved UV, same width as Y */
    k_u32 dst_stride = width * 2;

    for (k_u32 row = 0; row < height; row += 2) {
        const k_u8* y_row0 = y_ptr + row * y_stride;
        const k_u8* y_row1 = y_ptr + (row + 1) * y_stride;
        const k_u8* uv_row = uv_ptr + (row / 2) * uv_stride;
        k_u16*      d_row0 = (k_u16*)(dst + row * dst_stride);
        k_u16*      d_row1 = (k_u16*)(dst + (row + 1) * dst_stride);

        for (k_u32 col = 0; col < width; col += 2) {
            k_s32 u = (k_s32)uv_row[col] - 128;
            k_s32 v = (k_s32)uv_row[col + 1] - 128;

            k_s32 r_add = (CV_R_V * v) >> FP_SHIFT;
            k_s32 g_sub = (CV_G_U * u + CV_G_V * v) >> FP_SHIFT;
            k_s32 b_add = (CV_B_U * u) >> FP_SHIFT;

            /* 4 pixels sharing the same UV pair (2x2 block) */
            for (k_u32 dy = 0; dy < 2; dy++) {
                const k_u8* y_r = (dy == 0) ? y_row0 : y_row1;
                k_u16*      d_r = (dy == 0) ? d_row0 : d_row1;

                k_u16 pixels[2];

                for (k_u32 dx = 0; dx < 2; dx++) {
                    k_s32 y_val = (k_s32)y_r[col + dx];
                    k_u8  r     = clamp_u8(y_val + r_add);
                    k_u8  g     = clamp_u8(y_val - g_sub);
                    k_u8  b     = clamp_u8(y_val + b_add);

                    pixels[dx] = ((k_u16)(r >> 3) << 11) | ((k_u16)(g >> 2) << 5) | ((k_u16)(b >> 3));
                    if (swap_bytes)
                        pixels[dx] = rgb565_swap_bytes_scalar(pixels[dx]);
                }

                if (swap_pixels) {
                    d_r[col + 0] = pixels[1];
                    d_r[col + 1] = pixels[0];
                } else {
                    d_r[col + 0] = pixels[0];
                    d_r[col + 1] = pixels[1];
                }
            }
        }
    }
}

/*
 * Scalar fallback: YUV420SP (NV12) -> RGB888
 */
static void yuv420sp_to_rgb888_scalar(const k_u8* y_ptr, const k_u8* uv_ptr, k_u8* dst, k_u32 width, k_u32 height)
{
    k_u32 y_stride   = width;
    k_u32 uv_stride  = width;
    k_u32 dst_stride = width * 3;

    for (k_u32 row = 0; row < height; row += 2) {
        const k_u8* y_row0 = y_ptr + row * y_stride;
        const k_u8* y_row1 = y_ptr + (row + 1) * y_stride;
        const k_u8* uv_row = uv_ptr + (row / 2) * uv_stride;
        k_u8*       d_row0 = dst + row * dst_stride;
        k_u8*       d_row1 = dst + (row + 1) * dst_stride;

        for (k_u32 col = 0; col < width; col += 2) {
            k_s32 u = (k_s32)uv_row[col] - 128;
            k_s32 v = (k_s32)uv_row[col + 1] - 128;

            k_s32 r_add = (CV_R_V * v) >> FP_SHIFT;
            k_s32 g_sub = (CV_G_U * u + CV_G_V * v) >> FP_SHIFT;
            k_s32 b_add = (CV_B_U * u) >> FP_SHIFT;

            for (k_u32 dy = 0; dy < 2; dy++) {
                const k_u8* y_r = (dy == 0) ? y_row0 : y_row1;
                k_u8*       d_r = (dy == 0) ? d_row0 : d_row1;

                for (k_u32 dx = 0; dx < 2; dx++) {
                    k_s32 y_val  = (k_s32)y_r[col + dx];
                    k_u32 off    = (col + dx) * 3;
                    d_r[off]     = clamp_u8(y_val + r_add); /* R */
                    d_r[off + 1] = clamp_u8(y_val - g_sub); /* G */
                    d_r[off + 2] = clamp_u8(y_val + b_add); /* B */
                }
            }
        }
    }
}

int pixfmt_convert_yuv420sp_to_rgb(const k_u8* y_virt, const k_u8* uv_virt, k_u8* dst_virt, k_u32 width, k_u32 height,
                                   enum bridge_pixel_format dst_fmt, k_bool swap_bytes, k_bool swap_pixels)
{
    if (!y_virt || !uv_virt || !dst_virt)
        return -1;
    if (width == 0 || height == 0)
        return -1;
    /* YUV420 requires even dimensions */
    if ((width & 1) || (height & 1))
        return -1;

    switch (dst_fmt) {
    case BRIDGE_PIXFMT_RGB565:
        yuv420sp_to_rgb565_rvv(y_virt, uv_virt, dst_virt, width, height, swap_bytes, swap_pixels);
        break;
    case BRIDGE_PIXFMT_RGB888:
        yuv420sp_to_rgb888_rvv(y_virt, uv_virt, dst_virt, width, height);
        break;
    default:
        rt_kprintf("pixfmt_convert: unsupported format %d\n", dst_fmt);
        return -1;
    }

    return 0;
}

/**
 * @brief In‑place swap of high and low bytes of RGB565 pixels using vrev8.v.
 *
 * @param buf      Buffer containing RGB565 data (modified in‑place)
 * @param n_bytes  Number of bytes (must be even)
 */
void rgb565_swap_bytes_rvv_inplace(k_u8* buf, k_u32 n_bytes)
{
    k_u32   pixels = n_bytes / 2;
    k_u8* p      = buf;

    while (pixels > 0) {
        size_t vl;
        __asm__ volatile("vsetvli %0, %1, e16, m1, ta, ma\n\t"
                         "vle16.v v0, (%2)\n\t" // load pixels
                         "vsll.vi v1, v0, 8\n\t" // low byte → high, zero low
                         "vsrl.vi v2, v0, 8\n\t" // high byte → low, zero high
                         "vor.vv v0, v1, v2\n\t" // combine → byte‑swapped value
                         "vse16.v v0, (%2)\n\t" // store back
                         : "=&r"(vl)
                         : "r"(pixels), "r"(p)
                         : "v0", "v1", "v2", "memory");
        p += vl * 2;
        pixels -= vl;
    }
}
