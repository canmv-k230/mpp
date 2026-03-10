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

static struct uvc_device uvc_dev = {.fd = -1, .is_streamon = false};

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
