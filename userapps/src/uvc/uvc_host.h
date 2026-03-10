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

#ifndef __UVC_HOST_H__
#define __UVC_HOST_H__

#include "mpi_uvc_api.h"

#ifdef __cplusplus
extern "C" {
#endif /* End of #ifdef __cplusplus */

#define UVC_DEBUG (0)

#define BUF_CNT (4)
#define MAX_USB_STRING_LEN (64)

struct uvc_device {
    int fd;
    bool is_streamon;
    char *frame_buf[BUF_CNT];
};

struct uvc_fmtdesc {
    unsigned int index;
    unsigned int fourcc;
    unsigned char description[32];
};

struct uvc_requestbuffers {
    unsigned int count;
};

struct uvc_framedesc {
    unsigned int index;
    unsigned int fourcc;
    unsigned int width;
    unsigned int height;
    unsigned int defaultframeinterval;
};

struct uvc_fpsdesc {
    unsigned int index;
    unsigned int fourcc;
    unsigned int width;
    unsigned int height;
    unsigned int frameinterval;
};

/* Internal ioctl ABI for VIDIOC_QUERYBUF/VIDIOC_(Q|DQ)BUF.
 * Public `struct uvc_frame` in mpi_uvc_api.h intentionally hides mapping info.
 */
struct uvc_ioc_frame {
    unsigned int index;
    unsigned int length;
    unsigned int offset;
    unsigned int bytesused;
    char *userptr;
    union {
        k_video_frame_info v_info;
        k_vdec_stream v_stream;
    };
};

struct dfs_mmap2_args
{
    void *addr;
    size_t length;
    int prot;
    int flags;
    off_t pgoffset;

    void *ret;
};

struct usb_string {
    /* Index is the usb string index, for example , we use
     * USB_STRING_PRODUCT_INDEX(0x2) to get product string */
    unsigned char index;
    char str[MAX_USB_STRING_LEN];
};

struct usb_index {
    unsigned char iManufacturer;      /* Index to manufacturer string */
    unsigned char iProduct;           /* Index to product string */
};

#define VIDIOC_ENUM_FMT         _IOWR('V', 1, struct uvc_fmtdesc)
#define VIDIOC_S_FMT            _IOWR('V', 2, struct uvc_format)
#define VIDIOC_REQBUFS          _IOWR('V', 3, struct uvc_requestbuffers)
#define VIDIOC_QUERYBUF         _IOWR('V',  4, struct uvc_ioc_frame)
#define VIDIOC_QBUF             _IOWR('V', 5, struct uvc_ioc_frame)
#define VIDIOC_DQBUF            _IOWR('V', 6, struct uvc_ioc_frame)
#define VIDIOC_BUFMMAP          _IOW('V', 7, struct dfs_mmap2_args)
#define VIDIOC_STREAMON         _IOW('V', 8, int)
#define VIDIOC_STREAMOFF        _IOW('V', 9, int)
#define VIDIOC_ENUM_FRAME       _IOWR('V', 10, struct uvc_framedesc)
#define VIDIOC_ENUM_INTERVAL    _IOWR('V', 11, struct uvc_fpsdesc)
#define VIDIOC_GET_STRING       _IOWR('V', 12, struct usb_string)
#define VIDIOC_GET_INDEX        _IOWR('V', 13, struct usb_index)

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
