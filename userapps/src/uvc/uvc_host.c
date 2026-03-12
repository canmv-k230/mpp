/* Copyright (c) 2025, Canaan Bright Sight Co., Ltd
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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>

#include <sys/vfs.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/select.h>

#include "uvc_host.h"

#ifndef UVC_HOST_USE_RVV_CONVERT
#define UVC_HOST_USE_RVV_CONVERT 1
#endif

#define UVC_HOST_RVV_CHUNK_PAIRS 64U

static struct uvc_device uvc_dev = {.fd = -1, .is_streamon = false, .format_valid = false};

extern void *hal_rvv_memcpy(void *dst, const void *src, size_t n);

static void uvc_ioc_to_public_frame(struct uvc_frame *dst, const struct uvc_ioc_frame *src)
{
    memset(dst, 0, sizeof(*dst));
    dst->index = src->index;
    dst->bytesused = src->bytesused;
    memcpy(&dst->v_info, &src->v_info, sizeof(src->v_info));
}

static void uvc_public_to_ioc_frame(struct uvc_ioc_frame *dst, const struct uvc_frame *src)
{
    memset(dst, 0, sizeof(*dst));
    dst->index = src->index;
    dst->bytesused = src->bytesused;
    dst->userptr = src->userptr;
    memcpy(&dst->v_info, &src->v_info, sizeof(src->v_info));
}

#define uvc_host_get_current_format() uvc_host_get_current_format_impl(__func__)

int uvc_host_init(struct uvc_format *fmt)
{
    int fd;
    int ret = 0;
    bool found =false;
    struct uvc_fmtdesc fmt_desc;
    struct uvc_format format;
    struct uvc_requestbuffers requset_buf;
    struct uvc_ioc_frame ioc_frame;
    struct uvc_framedesc frame_desc;
    struct uvc_fpsdesc fps_desc;

    if(0 <= uvc_dev.fd) {
        uvc_host_exit();
    }

    uvc_dev.fd = -1;
    uvc_dev.is_streamon = false;
    uvc_dev.format_valid = false;
    memset(&uvc_dev.format, 0, sizeof(uvc_dev.format));

    fd = open("/dev/video0", O_RDWR);
    if (fd < 0) {
        printf("open dev fail: %s (errno: %d)\n", strerror(errno), errno);
        return -1;
    }

    uvc_dev.fd = fd;

    memcpy(&format, fmt, sizeof(*fmt));

    memset(&fmt_desc, 0, sizeof(fmt_desc));
    fmt_desc.index = 0;

    while (ioctl(fd, VIDIOC_ENUM_FMT, &fmt_desc) == 0) {
#if UVC_DEBUG
        printf("fmt fourcc is 0x%08x -> (%s)\n", fmt_desc.fourcc, fmt_desc.description);
#endif
        frame_desc.fourcc = fmt_desc.fourcc;
        frame_desc.index = 0;
        while (ioctl(fd, VIDIOC_ENUM_FRAME, &frame_desc) == 0) {
#if UVC_DEBUG
            printf("wWidth: %4d, wHeight: %4d, DefaultFrameInterval: %d\n",
                   frame_desc.width, frame_desc.height, frame_desc.defaultframeinterval);
#endif

            fps_desc.fourcc = fmt_desc.fourcc;
            fps_desc.width = frame_desc.width;
            fps_desc.height = frame_desc.height;
            fps_desc.index = 0;
            while (ioctl(fd, VIDIOC_ENUM_INTERVAL, &fps_desc) == 0) {
#if UVC_DEBUG
                printf("FrameInterval[%d]: %d\n", fps_desc.index, fps_desc.frameinterval);
#endif
                fps_desc.index ++;
            }
            frame_desc.index ++;
        }

        if ((format.fourcc == 0) || (fmt_desc.fourcc == format.fourcc)) {
            found = true;
        }
        fmt_desc.index ++;
    }

    if (!found) {
        printf("Don't support format\n");
        ret = -1;
        goto err;
    }

#if UVC_DEBUG
    printf("expect resolution: %d X %d @ %.2f, fourcc = 0x%08x\n",
           format.width, format.height, 10000000.0f / format.frameinterval, format.fourcc);
#endif
    if ((ret = ioctl(fd, VIDIOC_S_FMT, &format))) {
        printf("VIDIOC_S_FMT fail: %s (errno: %d)\n", strerror(errno), errno);
        goto err;
    }
    memcpy(fmt, &format, sizeof(*fmt));
    memcpy(&uvc_dev.format, &format, sizeof(format));
    uvc_dev.format_valid = true;

#if UVC_DEBUG
    printf("suite resolution: %d X %d @ %.2f, fourcc = 0x%08x\n",
           format.width, format.height, 10000000.0f / format.frameinterval, format.fourcc);
#endif

    requset_buf.count = BUF_CNT;

    if ((ret = ioctl(fd, VIDIOC_REQBUFS, &requset_buf))) {
        printf("VIDIOC_REQBUFS fail: %s (errno: %d)\n", strerror(errno), errno);
        goto err;
    }

    for (int i = 0; i < requset_buf.count; i ++) {
        struct dfs_mmap2_args mmap;

        memset(&ioc_frame, 0, sizeof(ioc_frame));
        ioc_frame.index = i;
        if ((ret = ioctl(fd, VIDIOC_QUERYBUF, &ioc_frame))) {
            printf("VIDIOC_QUERYBUF fail: %s (errno: %d)\n", strerror(errno), errno);
            goto err;
        }

        mmap.length = ioc_frame.length;
        mmap.pgoffset = ioc_frame.offset;
        //mmap.prot = ;
        if ((ret = ioctl(fd, VIDIOC_BUFMMAP, &mmap))) {
            printf("VIDIOC_BUFMMAP fail: %s (errno: %d)\n", strerror(errno), errno);
            goto err;
        }
        uvc_dev.frame_buf[i] = (char *)mmap.addr;
#if UVC_DEBUG
        printf("map addr = %p, len = %ld, offset = %d, vaddr = %p\n",
               mmap.addr, mmap.length, ioc_frame.offset, uvc_dev.frame_buf[i]);
#endif

        if ((ret = ioctl(fd, VIDIOC_QBUF, &ioc_frame))) {
            printf("VIDIOC_QBUF fail: %s (errno: %d)\n", strerror(errno), errno);
            goto err;
        }
    }

    return ret;

err:
    close(fd);

    uvc_dev.fd = -1;
    uvc_dev.is_streamon = false;
    uvc_dev.format_valid = false;
    memset(&uvc_dev.format, 0, sizeof(uvc_dev.format));

    return ret;
}

int uvc_host_start_stream(void)
{
    int ret;
    int fd = uvc_dev.fd;

    if(0 > fd) {
        printf("uvc not init\n");
        return -1;
    }

    if ((ret = ioctl(fd, VIDIOC_STREAMON, NULL))) {
        printf("VIDIOC_STREAMON fail: %s (errno: %d)\n", strerror(errno), errno);
        return -1;
    }

    uvc_dev.is_streamon = true;

    return 0;
}

void uvc_host_exit()
{
    int ret;
    int fd = uvc_dev.fd;

    if(0 > fd) {
        uvc_dev.format_valid = false;
        memset(&uvc_dev.format, 0, sizeof(uvc_dev.format));
        return;
    }

    if (uvc_dev.is_streamon == true) {
        if ((ret = ioctl(fd, VIDIOC_STREAMOFF, NULL))) {
            printf("VIDIOC_STREAMOFF fail: %s (errno: %d)\n", strerror(errno), errno);
        }
        uvc_dev.is_streamon = false;

        if (fd != -1) {
            close(fd);
            uvc_dev.fd = -1;
        }
    }

    uvc_dev.format_valid = false;
    memset(&uvc_dev.format, 0, sizeof(uvc_dev.format));

}

int uvc_host_get_frame(struct uvc_frame *frame, unsigned int timeout_ms)
{
    int fd = uvc_dev.fd;
    int ret = 0;
    fd_set readset;
    struct uvc_ioc_frame ioc_frame = {0};

    struct timeval tv = {
        .tv_sec = timeout_ms / 1000,
        .tv_usec = (timeout_ms % 1000) * 1000
    };

    if(0 > fd) {
        printf("uvc not init\n");
        return -1;
    }

    FD_ZERO(&readset);
    FD_SET(fd, &readset);

    if (select(fd + 1, &readset, NULL, NULL, &tv) == 0) {
        printf("uvc_host_get_frame do select fail\n");
        ret = -1;
        goto err;
    }

    if ((ret = ioctl(fd, VIDIOC_DQBUF, &ioc_frame))) {
        printf("VIDIOC_DQBUF fail: %s (errno: %d)\n", strerror(errno), errno);
        goto err;
    }

    uvc_ioc_to_public_frame(frame, &ioc_frame);
    frame->userptr = uvc_dev.frame_buf[frame->index];

err:
    return ret;
}

int uvc_host_put_frame(struct uvc_frame *frame)
{
    int ret = 0;
    int fd = uvc_dev.fd;
    struct uvc_ioc_frame ioc_frame;

    if(0 > fd) {
        printf("uvc not init\n");
        return -1;
    }

    uvc_public_to_ioc_frame(&ioc_frame, frame);
    if ((ret = ioctl(fd, VIDIOC_QBUF, &ioc_frame))) {
        printf("VIDIOC_QBUF fail: %s (errno: %d)\n", strerror(errno), errno);
    }

    return ret;
}

int uvc_host_get_devinfo(char *info, int len)
{
    int ret = 0;
    int fd = uvc_dev.fd;
    struct usb_string str_manufacturer;
    struct usb_string str_product;
    struct usb_index usb_index;
    int need_len = 0;

    if(0 > fd) {
        fd = open("/dev/video0", O_RDWR);
        if (fd < 0) {
            printf("open dev fail: %s (errno: %d)\n", strerror(errno), errno);
            return -1;
        }
    }

    if ((ret = ioctl(fd, VIDIOC_GET_INDEX, &usb_index))) {
        printf("VIDIOC_GET_INDEX fail: %s (errno: %d)\n", strerror(errno), errno);
        goto out;
    }

    str_manufacturer.index = usb_index.iManufacturer;
    if ((ret = ioctl(fd, VIDIOC_GET_STRING, &str_manufacturer))) {
        printf("get iManufacturer fail: %s (errno: %d)\n", strerror(errno), errno);
        goto out;
    }
    need_len += strlen(str_manufacturer.str);

    str_product.index = usb_index.iProduct;
    if ((ret = ioctl(fd, VIDIOC_GET_STRING, &str_product))) {
        printf("get iProduct fail: %s (errno: %d)\n", strerror(errno), errno);
        goto out;
    }

    need_len += strlen(str_product.str);
    need_len += 2;

    if (len < need_len) {
        ret = -1;
        goto out;
    }

    snprintf(info, len, "%s#%s", str_manufacturer.str, str_product.str);

out:
    if((0 > uvc_dev.fd) && (0 <= fd)) {
        close(fd);
    }

    return ret;
}

int uvc_host_get_formats(struct uvc_format **fmts)
{
    int fd = uvc_dev.fd;

    size_t fmt_count = 0;
    size_t fmt_index = 0;
    struct uvc_fmtdesc fmt_desc = {0};
    struct uvc_framedesc frame_desc = {0};
    struct uvc_fpsdesc fps_desc = {0};

    if(0 > fd) {
        fd = open("/dev/video0", O_RDWR);
        if (fd < 0) {
            printf("open dev fail: %s (errno: %d)\n", strerror(errno), errno);
            return -1;
        }
    }

    // 第一次遍历：计算支持的格式数量
    fmt_desc.index = 0;
    while (ioctl(fd, VIDIOC_ENUM_FMT, &fmt_desc) == 0) {
        frame_desc.index = 0;
        frame_desc.fourcc = fmt_desc.fourcc;
        while (ioctl(fd, VIDIOC_ENUM_FRAME, &frame_desc) == 0) {
            fps_desc.index = 0;
            fps_desc.fourcc = fmt_desc.fourcc;
            fps_desc.width = frame_desc.width;
            fps_desc.height = frame_desc.height;
            while (ioctl(fd, VIDIOC_ENUM_INTERVAL, &fps_desc) == 0) {
                fmt_count ++;
                fps_desc.index ++;
            }
            frame_desc.index ++;
        }
        fmt_desc.index ++;
    }

    if (0x00 == fmt_count) {
        goto _out;
    }

    // 分配内存
    *fmts = malloc(fmt_count * sizeof(struct uvc_format));
    if (!*fmts) {
        printf("Failed to allocate format memory");

        fmt_count = -1;
        goto _out;
    }

    // 第二次遍历：填充格式数据
    fmt_index = 0;
    fmt_desc.index = 0;
    while (ioctl(fd, VIDIOC_ENUM_FMT, &fmt_desc) == 0) {
        frame_desc.index = 0;
        frame_desc.fourcc = fmt_desc.fourcc;
        while (ioctl(fd, VIDIOC_ENUM_FRAME, &frame_desc) == 0) {
            fps_desc.index = 0;
            fps_desc.fourcc = fmt_desc.fourcc;
            fps_desc.width = frame_desc.width;
            fps_desc.height = frame_desc.height;
            while (ioctl(fd, VIDIOC_ENUM_INTERVAL, &fps_desc) == 0) {
                if (fmt_index >= fmt_count) {
                    printf("Format count mismatch\n");
                    break;
                }
    
                (*fmts)[fmt_index].width = frame_desc.width;
                (*fmts)[fmt_index].height = frame_desc.height;
                (*fmts)[fmt_index].fourcc = fmt_desc.fourcc;
                (*fmts)[fmt_index].frameinterval = fps_desc.frameinterval;
    
                fmt_index++;
                fps_desc.index ++;
            }
            frame_desc.index ++;
        }
        fmt_desc.index ++;
    }

_out:
    if((0 > uvc_dev.fd) && (0 <= fd)) {
        close(fd);
    }

    return (int)fmt_count;
}

void uvc_host_free_formats(struct uvc_format **fmts)
{
    if (fmts && *fmts) {
        free(*fmts);
        *fmts = NULL;
    }
}

static const struct uvc_format *uvc_host_get_current_format_impl(const char *name)
{
    if (!uvc_dev.format_valid) {
        printf("%s no active format\n", name);
        return NULL;
    }

    return &uvc_dev.format;
}

static int uvc_host_check_even_size(const char *name, const struct uvc_format *fmt)
{
    if (!fmt || (fmt->width == 0) || (fmt->height == 0) ||
        ((fmt->width & 0x1U) != 0) || ((fmt->height & 0x1U) != 0)) {
        printf("%s invalid size: %ux%u\n", name,
               fmt ? fmt->width : 0, fmt ? fmt->height : 0);
        return -1;
    }

    return 0;
}

static inline uint16_t uvc_host_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8U) << 8) | ((g & 0xFCU) << 3) | (b >> 3));
}

static void uvc_host_yuv_to_rgb(uint8_t y, uint8_t u, uint8_t v,
                                uint8_t *r, uint8_t *g, uint8_t *b)
{
    int c = y - 16;
    int d = u - 128;
    int e = v - 128;
    int rt = (298 * c + 409 * e + 128) >> 8;
    int gt = (298 * c - 100 * d - 208 * e + 128) >> 8;
    int bt = (298 * c + 516 * d + 128) >> 8;

    *r = (uint8_t)(rt < 0 ? 0 : (rt > 255 ? 255 : rt));
    *g = (uint8_t)(gt < 0 ? 0 : (gt > 255 ? 255 : gt));
    *b = (uint8_t)(bt < 0 ? 0 : (bt > 255 ? 255 : bt));
}

#if UVC_HOST_USE_RVV_CONVERT
static size_t uvc_host_unpack_yuyv_pairs(const uint8_t *src,
                                         uint8_t *y0, uint8_t *u, uint8_t *y1, uint8_t *v,
                                         size_t pairs)
{
    size_t vl;

    asm volatile(
        ".option push\n"
        ".option arch, +v\n"
        "vsetvli %0, %5, e8, m1, ta, ma\n"
        "vlseg4e8.v v0, (%1)\n"
        "vse8.v v0, (%2)\n"
        "vse8.v v1, (%3)\n"
        "vse8.v v2, (%4)\n"
        "vse8.v v3, (%6)\n"
        ".option pop\n"
        : "=&r"(vl)
        : "r"(src), "r"(y0), "r"(u), "r"(y1), "r"(pairs), "r"(v)
        : "v0", "v1", "v2", "v3", "memory");

    return vl;
}

static size_t uvc_host_unpack_uyvy_pairs(const uint8_t *src,
                                         uint8_t *y0, uint8_t *u, uint8_t *y1, uint8_t *v,
                                         size_t pairs)
{
    size_t vl;

    asm volatile(
        ".option push\n"
        ".option arch, +v\n"
        "vsetvli %0, %5, e8, m1, ta, ma\n"
        "vlseg4e8.v v0, (%1)\n"
        "vse8.v v1, (%2)\n"
        "vse8.v v0, (%3)\n"
        "vse8.v v3, (%4)\n"
        "vse8.v v2, (%6)\n"
        ".option pop\n"
        : "=&r"(vl)
        : "r"(src), "r"(y0), "r"(u), "r"(y1), "r"(pairs), "r"(v)
        : "v0", "v1", "v2", "v3", "memory");

    return vl;
}

static void uvc_host_uyvy_to_yuyv_rvv(const unsigned char *src, unsigned char *dst,
                                      unsigned int width, unsigned int height)
{
    const unsigned char *src_ptr = src;
    unsigned char *dst_ptr = dst;
    size_t remaining = ((size_t)width * height) / 2;

    while (remaining > 0) {
        size_t vl;
        size_t chunk = remaining > UVC_HOST_RVV_CHUNK_PAIRS ? UVC_HOST_RVV_CHUNK_PAIRS : remaining;

        asm volatile(
            ".option push\n"
            ".option arch, +v\n"
            "vsetvli %0, %2, e8, m1, ta, ma\n"
            "vlseg4e8.v v0, (%1)\n"
            "vmv.v.v v4, v1\n"
            "vmv.v.v v5, v0\n"
            "vmv.v.v v6, v3\n"
            "vmv.v.v v7, v2\n"
            "vsseg4e8.v v4, (%3)\n"
            ".option pop\n"
            : "=&r"(vl)
            : "r"(src_ptr), "r"(chunk), "r"(dst_ptr)
            : "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "memory");

        src_ptr += vl * 4;
        dst_ptr += vl * 4;
        remaining -= vl;
    }
}
#endif

static void uvc_host_yuy2_to_nv12_c(const unsigned char *src, unsigned char *dst,
                                    unsigned int width, unsigned int height)
{
    unsigned char *y_plane = dst;
    unsigned char *uv_plane = dst + width * height;

    for (unsigned int j = 0; j < height; j++) {
        for (unsigned int i = 0; i < width; i += 2) {
            unsigned int index = j * width * 2 + i * 2;

            y_plane[j * width + i] = src[index];
            y_plane[j * width + i + 1] = src[index + 2];

            if ((j & 0x1U) == 0) {
                uv_plane[(j / 2) * width + i] = src[index + 1];
                uv_plane[(j / 2) * width + i + 1] = src[index + 3];
            }
        }
    }
}

static void uvc_host_uyvy_to_nv12_c(const unsigned char *src, unsigned char *dst,
                                    unsigned int width, unsigned int height)
{
    unsigned char *y_plane = dst;
    unsigned char *uv_plane = dst + width * height;

    for (unsigned int j = 0; j < height; j++) {
        for (unsigned int i = 0; i < width; i += 2) {
            unsigned int index = j * width * 2 + i * 2;

            y_plane[j * width + i] = src[index + 1];
            y_plane[j * width + i + 1] = src[index + 3];

            if ((j & 0x1U) == 0) {
                uv_plane[(j / 2) * width + i] = src[index];
                uv_plane[(j / 2) * width + i + 1] = src[index + 2];
            }
        }
    }
}

static void uvc_host_i420_to_nv12_c(const unsigned char *src, unsigned char *dst,
                                    unsigned int width, unsigned int height)
{
    unsigned int y_size = width * height;
    unsigned int uv_plane_size = y_size / 4;
    const unsigned char *src_u = src + y_size;
    const unsigned char *src_v = src_u + uv_plane_size;
    unsigned char *dst_uv = dst + y_size;

    memcpy(dst, src, y_size);
    for (unsigned int i = 0; i < uv_plane_size; i++) {
        dst_uv[i * 2] = src_u[i];
        dst_uv[i * 2 + 1] = src_v[i];
    }
}

static void uvc_host_yuy2_to_rgb565_c(const unsigned char *src, unsigned char *dst,
                                      unsigned int width, unsigned int height)
{
    uint16_t *rgb = (uint16_t *)dst;
    unsigned int num_pixels = width * height;

    for (unsigned int i = 0; i < num_pixels; i += 2) {
        unsigned int index = i * 2;
        uint8_t y0 = src[index];
        uint8_t u = src[index + 1];
        uint8_t y1 = src[index + 2];
        uint8_t v = src[index + 3];
        uint8_t r, g, b;

        uvc_host_yuv_to_rgb(y0, u, v, &r, &g, &b);
        rgb[i] = uvc_host_rgb565(r, g, b);

        uvc_host_yuv_to_rgb(y1, u, v, &r, &g, &b);
        rgb[i + 1] = uvc_host_rgb565(r, g, b);
    }
}

static void uvc_host_uyvy_to_rgb565_c(const unsigned char *src, unsigned char *dst,
                                      unsigned int width, unsigned int height)
{
    uint16_t *rgb = (uint16_t *)dst;
    unsigned int num_pixels = width * height;

    for (unsigned int i = 0; i < num_pixels; i += 2) {
        unsigned int index = i * 2;
        uint8_t u = src[index];
        uint8_t y0 = src[index + 1];
        uint8_t v = src[index + 2];
        uint8_t y1 = src[index + 3];
        uint8_t r, g, b;

        uvc_host_yuv_to_rgb(y0, u, v, &r, &g, &b);
        rgb[i] = uvc_host_rgb565(r, g, b);

        uvc_host_yuv_to_rgb(y1, u, v, &r, &g, &b);
        rgb[i + 1] = uvc_host_rgb565(r, g, b);
    }
}

static void uvc_host_uyvy_to_yuyv_c(const unsigned char *src, unsigned char *dst,
                                    unsigned int width, unsigned int height)
{
    unsigned int total = width * height * 2;

    if (src == dst) {
        for (unsigned int i = 0; i < total; i += 4) {
            unsigned char u = dst[i];
            unsigned char y0 = dst[i + 1];
            unsigned char v = dst[i + 2];
            unsigned char y1 = dst[i + 3];

            dst[i] = y0;
            dst[i + 1] = u;
            dst[i + 2] = y1;
            dst[i + 3] = v;
        }
    } else {
        for (unsigned int i = 0; i < total; i += 4) {
            dst[i] = src[i + 1];
            dst[i + 1] = src[i];
            dst[i + 2] = src[i + 3];
            dst[i + 3] = src[i + 2];
        }
    }
}

#if UVC_HOST_USE_RVV_CONVERT
static void uvc_host_yuy2_to_nv12_rvv(const unsigned char *src, unsigned char *dst,
                                      unsigned int width, unsigned int height)
{
    unsigned char *y_plane = dst;
    unsigned char *uv_plane = dst + width * height;
    unsigned int pairs_per_row = width / 2;

    for (unsigned int j = 0; j < height; j++) {
        const unsigned char *row_src = src + (j * width * 2);
        unsigned char *row_y = y_plane + (j * width);
        unsigned char *row_uv = uv_plane + ((j / 2) * width);
        unsigned int remaining = pairs_per_row;

        while (remaining > 0) {
            size_t vl;
            size_t chunk = remaining > UVC_HOST_RVV_CHUNK_PAIRS ? UVC_HOST_RVV_CHUNK_PAIRS : remaining;

            if ((j & 0x1U) == 0) {
                asm volatile(
                    ".option push\n"
                    ".option arch, +v\n"
                    "vsetvli %0, %4, e8, m1, ta, ma\n"
                    "vlseg4e8.v v0, (%1)\n"
                    "vmv.v.v v4, v0\n"
                    "vmv.v.v v5, v2\n"
                    "vsseg2e8.v v4, (%2)\n"
                    "vmv.v.v v4, v1\n"
                    "vmv.v.v v5, v3\n"
                    "vsseg2e8.v v4, (%3)\n"
                    ".option pop\n"
                    : "=&r"(vl)
                    : "r"(row_src), "r"(row_y), "r"(row_uv), "r"(chunk)
                    : "v0", "v1", "v2", "v3", "v4", "v5", "memory");
                row_uv += vl * 2;
            } else {
                asm volatile(
                    ".option push\n"
                    ".option arch, +v\n"
                    "vsetvli %0, %3, e8, m1, ta, ma\n"
                    "vlseg4e8.v v0, (%1)\n"
                    "vmv.v.v v4, v0\n"
                    "vmv.v.v v5, v2\n"
                    "vsseg2e8.v v4, (%2)\n"
                    ".option pop\n"
                    : "=&r"(vl)
                    : "r"(row_src), "r"(row_y), "r"(chunk)
                    : "v0", "v1", "v2", "v3", "v4", "v5", "memory");
            }

            row_src += vl * 4;
            row_y += vl * 2;
            remaining -= (unsigned int)vl;
        }
    }
}

static void uvc_host_uyvy_to_nv12_rvv(const unsigned char *src, unsigned char *dst,
                                      unsigned int width, unsigned int height)
{
    unsigned char *y_plane = dst;
    unsigned char *uv_plane = dst + width * height;
    unsigned int pairs_per_row = width / 2;

    for (unsigned int j = 0; j < height; j++) {
        const unsigned char *row_src = src + (j * width * 2);
        unsigned char *row_y = y_plane + (j * width);
        unsigned char *row_uv = uv_plane + ((j / 2) * width);
        unsigned int remaining = pairs_per_row;

        while (remaining > 0) {
            size_t vl;
            size_t chunk = remaining > UVC_HOST_RVV_CHUNK_PAIRS ? UVC_HOST_RVV_CHUNK_PAIRS : remaining;

            if ((j & 0x1U) == 0) {
                asm volatile(
                    ".option push\n"
                    ".option arch, +v\n"
                    "vsetvli %0, %4, e8, m1, ta, ma\n"
                    "vlseg4e8.v v0, (%1)\n"
                    "vmv.v.v v4, v1\n"
                    "vmv.v.v v5, v3\n"
                    "vsseg2e8.v v4, (%2)\n"
                    "vmv.v.v v4, v0\n"
                    "vmv.v.v v5, v2\n"
                    "vsseg2e8.v v4, (%3)\n"
                    ".option pop\n"
                    : "=&r"(vl)
                    : "r"(row_src), "r"(row_y), "r"(row_uv), "r"(chunk)
                    : "v0", "v1", "v2", "v3", "v4", "v5", "memory");
                row_uv += vl * 2;
            } else {
                asm volatile(
                    ".option push\n"
                    ".option arch, +v\n"
                    "vsetvli %0, %3, e8, m1, ta, ma\n"
                    "vlseg4e8.v v0, (%1)\n"
                    "vmv.v.v v4, v1\n"
                    "vmv.v.v v5, v3\n"
                    "vsseg2e8.v v4, (%2)\n"
                    ".option pop\n"
                    : "=&r"(vl)
                    : "r"(row_src), "r"(row_y), "r"(chunk)
                    : "v0", "v1", "v2", "v3", "v4", "v5", "memory");
            }

            row_src += vl * 4;
            row_y += vl * 2;
            remaining -= (unsigned int)vl;
        }
    }
}

static void uvc_host_i420_to_nv12_rvv(const unsigned char *src, unsigned char *dst,
                                      unsigned int width, unsigned int height)
{
    unsigned int y_size = width * height;
    unsigned int uv_plane_size = y_size / 4;
    const unsigned char *src_u = src + y_size;
    const unsigned char *src_v = src_u + uv_plane_size;
    unsigned char *dst_uv = dst + y_size;
    unsigned int remaining = uv_plane_size;

    hal_rvv_memcpy(dst, src, y_size);

    while (remaining > 0) {
        size_t vl;
        size_t chunk = remaining > (UVC_HOST_RVV_CHUNK_PAIRS * 2) ? (UVC_HOST_RVV_CHUNK_PAIRS * 2) : remaining;

        asm volatile(
            ".option push\n"
            ".option arch, +v\n"
            "vsetvli %0, %4, e8, m1, ta, ma\n"
            "vle8.v v0, (%1)\n"
            "vle8.v v1, (%2)\n"
            "vsseg2e8.v v0, (%3)\n"
            ".option pop\n"
            : "=&r"(vl)
            : "r"(src_u), "r"(src_v), "r"(dst_uv), "r"(chunk)
            : "v0", "v1", "memory");

        src_u += vl;
        src_v += vl;
        dst_uv += vl * 2;
        remaining -= (unsigned int)vl;
    }
}

static void uvc_host_yuy2_to_rgb565_rvv(const unsigned char *src, unsigned char *dst,
                                        unsigned int width, unsigned int height)
{
    uint8_t y0[UVC_HOST_RVV_CHUNK_PAIRS];
    uint8_t u[UVC_HOST_RVV_CHUNK_PAIRS];
    uint8_t y1[UVC_HOST_RVV_CHUNK_PAIRS];
    uint8_t v[UVC_HOST_RVV_CHUNK_PAIRS];
    uint16_t *rgb = (uint16_t *)dst;
    size_t remaining = ((size_t)width * height) / 2;

    while (remaining > 0) {
        size_t chunk = remaining > UVC_HOST_RVV_CHUNK_PAIRS ? UVC_HOST_RVV_CHUNK_PAIRS : remaining;
        size_t vl = uvc_host_unpack_yuyv_pairs(src, y0, u, y1, v, chunk);

        for (size_t i = 0; i < vl; i++) {
            uint8_t r, g, b;

            uvc_host_yuv_to_rgb(y0[i], u[i], v[i], &r, &g, &b);
            rgb[i * 2] = uvc_host_rgb565(r, g, b);

            uvc_host_yuv_to_rgb(y1[i], u[i], v[i], &r, &g, &b);
            rgb[i * 2 + 1] = uvc_host_rgb565(r, g, b);
        }

        src += vl * 4;
        rgb += vl * 2;
        remaining -= vl;
    }
}

static void uvc_host_uyvy_to_rgb565_rvv(const unsigned char *src, unsigned char *dst,
                                        unsigned int width, unsigned int height)
{
    uint8_t y0[UVC_HOST_RVV_CHUNK_PAIRS];
    uint8_t u[UVC_HOST_RVV_CHUNK_PAIRS];
    uint8_t y1[UVC_HOST_RVV_CHUNK_PAIRS];
    uint8_t v[UVC_HOST_RVV_CHUNK_PAIRS];
    uint16_t *rgb = (uint16_t *)dst;
    size_t remaining = ((size_t)width * height) / 2;

    while (remaining > 0) {
        size_t chunk = remaining > UVC_HOST_RVV_CHUNK_PAIRS ? UVC_HOST_RVV_CHUNK_PAIRS : remaining;
        size_t vl = uvc_host_unpack_uyvy_pairs(src, y0, u, y1, v, chunk);

        for (size_t i = 0; i < vl; i++) {
            uint8_t r, g, b;

            uvc_host_yuv_to_rgb(y0[i], u[i], v[i], &r, &g, &b);
            rgb[i * 2] = uvc_host_rgb565(r, g, b);

            uvc_host_yuv_to_rgb(y1[i], u[i], v[i], &r, &g, &b);
            rgb[i * 2 + 1] = uvc_host_rgb565(r, g, b);
        }

        src += vl * 4;
        rgb += vl * 2;
        remaining -= vl;
    }
}
#endif

static void uvc_host_yuy2_to_nv12(const unsigned char *src, unsigned char *dst,
                                  unsigned int width, unsigned int height)
{
#if UVC_HOST_USE_RVV_CONVERT
    uvc_host_yuy2_to_nv12_rvv(src, dst, width, height);
#else
    uvc_host_yuy2_to_nv12_c(src, dst, width, height);
#endif
}

static void uvc_host_uyvy_to_nv12(const unsigned char *src, unsigned char *dst,
                                  unsigned int width, unsigned int height)
{
#if UVC_HOST_USE_RVV_CONVERT
    uvc_host_uyvy_to_nv12_rvv(src, dst, width, height);
#else
    uvc_host_uyvy_to_nv12_c(src, dst, width, height);
#endif
}

static void uvc_host_i420_to_nv12(const unsigned char *src, unsigned char *dst,
                                  unsigned int width, unsigned int height)
{
#if UVC_HOST_USE_RVV_CONVERT
    uvc_host_i420_to_nv12_rvv(src, dst, width, height);
#else
    uvc_host_i420_to_nv12_c(src, dst, width, height);
#endif
}

static void uvc_host_yuy2_to_rgb565(const unsigned char *src, unsigned char *dst,
                                    unsigned int width, unsigned int height)
{
#if UVC_HOST_USE_RVV_CONVERT
    uvc_host_yuy2_to_rgb565_rvv(src, dst, width, height);
#else
    uvc_host_yuy2_to_rgb565_c(src, dst, width, height);
#endif
}

static void uvc_host_uyvy_to_rgb565(const unsigned char *src, unsigned char *dst,
                                    unsigned int width, unsigned int height)
{
#if UVC_HOST_USE_RVV_CONVERT
    uvc_host_uyvy_to_rgb565_rvv(src, dst, width, height);
#else
    uvc_host_uyvy_to_rgb565_c(src, dst, width, height);
#endif
}

static void uvc_host_uyvy_to_yuyv(const unsigned char *src, unsigned char *dst,
                                  unsigned int width, unsigned int height)
{
#if UVC_HOST_USE_RVV_CONVERT
    uvc_host_uyvy_to_yuyv_rvv(src, dst, width, height);
#else
    uvc_host_uyvy_to_yuyv_c(src, dst, width, height);
#endif
}

int uvc_host_raw_to_nv12(const struct uvc_frame *frame, void *dst, size_t dst_len)
{
    const struct uvc_format *fmt;
    size_t required_len;

    if (!frame || !frame->userptr || !dst) {
        return -1;
    }

    fmt = uvc_host_get_current_format();
    if (!fmt) {
        return -1;
    }

    if (uvc_host_check_even_size("uvc_host_raw_to_nv12", fmt) != 0) {
        return -1;
    }

    required_len = (size_t)fmt->width * fmt->height * 3 / 2;
    if (dst_len < required_len) {
        printf("uvc_host_raw_to_nv12 dst too small: %u < %u\n",
               (unsigned int)dst_len, (unsigned int)required_len);
        return -1;
    }

    switch (fmt->fourcc) {
    case USBH_VIDEO_FOURCC_YUY2:
        uvc_host_yuy2_to_nv12((const unsigned char *)frame->userptr, (unsigned char *)dst,
                              fmt->width, fmt->height);
        return 0;
    case USBH_VIDEO_FOURCC_UYVY:
        uvc_host_uyvy_to_nv12((const unsigned char *)frame->userptr, (unsigned char *)dst,
                              fmt->width, fmt->height);
        return 0;
    case USBH_VIDEO_FOURCC_NV12:
        if (dst != frame->userptr) {
#if UVC_HOST_USE_RVV_CONVERT
            hal_rvv_memcpy(dst, frame->userptr, required_len);
#else
            memcpy(dst, frame->userptr, required_len);
#endif
        }
        return 0;
    case USBH_VIDEO_FOURCC_I420:
        uvc_host_i420_to_nv12((const unsigned char *)frame->userptr, (unsigned char *)dst,
                              fmt->width, fmt->height);
        return 0;
    default:
        printf("uvc_host_raw_to_nv12 unsupported fourcc: 0x%08x\n", fmt->fourcc);
        return -1;
    }
}

int uvc_host_raw_to_rgb565(const struct uvc_frame *frame, void *dst, size_t dst_len)
{
    const struct uvc_format *fmt;
    size_t required_len;

    if (!frame || !frame->userptr || !dst) {
        return -1;
    }

    fmt = uvc_host_get_current_format();
    if (!fmt) {
        return -1;
    }

    if (uvc_host_check_even_size("uvc_host_raw_to_rgb565", fmt) != 0) {
        return -1;
    }

    required_len = (size_t)fmt->width * fmt->height * 2;
    if (dst_len < required_len) {
        printf("uvc_host_raw_to_rgb565 dst too small: %u < %u\n",
               (unsigned int)dst_len, (unsigned int)required_len);
        return -1;
    }

    switch (fmt->fourcc) {
    case USBH_VIDEO_FOURCC_YUY2:
        uvc_host_yuy2_to_rgb565((const unsigned char *)frame->userptr, (unsigned char *)dst,
                                fmt->width, fmt->height);
        return 0;
    case USBH_VIDEO_FOURCC_UYVY:
        uvc_host_uyvy_to_rgb565((const unsigned char *)frame->userptr, (unsigned char *)dst,
                                fmt->width, fmt->height);
        return 0;
    default:
        printf("uvc_host_raw_to_rgb565 unsupported fourcc: 0x%08x\n", fmt->fourcc);
        return -1;
    }
}

int uvc_host_raw_to_yuyv(const struct uvc_frame *frame, void *dst, size_t dst_len)
{
    const struct uvc_format *fmt;
    size_t required_len;

    if (!frame || !frame->userptr || !dst) {
        return -1;
    }

    fmt = uvc_host_get_current_format();
    if (!fmt) {
        return -1;
    }

    if (uvc_host_check_even_size("uvc_host_raw_to_yuyv", fmt) != 0) {
        return -1;
    }

    required_len = (size_t)fmt->width * fmt->height * 2;
    if (dst_len < required_len) {
        printf("uvc_host_raw_to_yuyv dst too small: %u < %u\n",
               (unsigned int)dst_len, (unsigned int)required_len);
        return -1;
    }

    switch (fmt->fourcc) {
    case USBH_VIDEO_FOURCC_YUY2:
        if (dst != frame->userptr) {
#if UVC_HOST_USE_RVV_CONVERT
            hal_rvv_memcpy(dst, frame->userptr, required_len);
#else
            memcpy(dst, frame->userptr, required_len);
#endif
        }
        return 0;
    case USBH_VIDEO_FOURCC_UYVY:
        uvc_host_uyvy_to_yuyv((const unsigned char *)frame->userptr, (unsigned char *)dst,
                              fmt->width, fmt->height);
        return 0;
    default:
        printf("uvc_host_raw_to_yuyv unsupported fourcc: 0x%08x\n", fmt->fourcc);
        return -1;
    }
}
