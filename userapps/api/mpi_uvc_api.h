/**
 * @file mpi_uvc_api.h
 * @author
 * @brief Defines APIs related to virtual video input device
 * @version 1.0
 * @date 2025-02-25
 *
 * @copyright
 * Copyright (c), Canaan Bright Sight Co., Ltd
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
#ifndef __MPI_UVC_API_H__
#define __MPI_UVC_API_H__

#include <stdint.h>
#include <unistd.h>

#include "k_vdec_comm.h"
#include "k_video_comm.h"

#ifdef __cplusplus
extern "C" {
#endif /* End of #ifdef __cplusplus */

#define INVALID_FRAME_INDEX (0xffffffff)

#define USBH_VIDEO_FOURCC(a, b, c, d)                                           \
    ((uint32_t)(uint8_t)(a) | ((uint32_t)(uint8_t)(b) << 8) |                   \
     ((uint32_t)(uint8_t)(c) << 16) | ((uint32_t)(uint8_t)(d) << 24))

#define USBH_VIDEO_FOURCC_YUY2  USBH_VIDEO_FOURCC('Y', 'U', 'Y', '2')
#define USBH_VIDEO_FOURCC_UYVY  USBH_VIDEO_FOURCC('U', 'Y', 'V', 'Y')
#define USBH_VIDEO_FOURCC_NV12  USBH_VIDEO_FOURCC('N', 'V', '1', '2')
#define USBH_VIDEO_FOURCC_I420  USBH_VIDEO_FOURCC('I', '4', '2', '0')
#define USBH_VIDEO_FOURCC_MJPEG USBH_VIDEO_FOURCC('M', 'J', 'P', 'G')

///////////////////////////////////////////////////////////////////////////////
// UVC Host ///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
struct uvc_frame {
    unsigned int index;
    unsigned int bytesused;
    char *userptr;
    union {
        k_video_frame_info v_info;
        k_vdec_stream v_stream;
    };
};

struct uvc_format {
    unsigned int width;
    unsigned int height;
    unsigned int fourcc;
    unsigned int frameinterval;
};

int uvc_host_init(struct uvc_format *fmt);
int uvc_host_start_stream(void);
int uvc_host_get_frame(struct uvc_frame *frame, unsigned int timeout_ms);
int uvc_host_put_frame(struct uvc_frame *frame);
void uvc_host_exit();

int uvc_host_get_devinfo(char *info, int len);
int uvc_host_get_formats(struct uvc_format **fmts);
void uvc_host_free_formats(struct uvc_format **fmts);

///////////////////////////////////////////////////////////////////////////////
// UVC Device /////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
int uvc_device_init(void);
void uvc_device_deinit(void);

int uvc_device_create_buffer_pool(uint32_t buffer_size, uint32_t buffer_count);

int uvc_device_get_buf(uint32_t **buffer, uint32_t *max_size);
int uvc_device_put_buf(uint32_t *buffer, uint32_t size);

struct uvc_device_conf_t {
    int frame_rate;
};
int uvc_device_conf(struct uvc_device_conf_t *cfg);

int uvc_device_start();
int uvc_device_stop();

int uvc_device_get_state(int *opened);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
