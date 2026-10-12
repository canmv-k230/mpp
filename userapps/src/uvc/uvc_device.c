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

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "uvc_device.h"

static int video_dev_fd = -1;

static int uvc_device_ioctl(int cmd, void* data)
{
    int ret;

    if (0 > video_dev_fd) {
        return -1;
    }

    if (0x00 != (ret = ioctl(video_dev_fd, cmd, data))) {
        printf("[uvc_dev]: ioctl failed with cmd 0x%08X\n", cmd);
    }

    return ret;
}

int uvc_device_init(void)
{
    if (0 <= video_dev_fd) {
        return 0;
    }

    video_dev_fd = open("/dev/video", O_RDWR);
    if (video_dev_fd < 0) {
        printf("[uvc_dev] Failed to open device: %d\n", errno);
        return -1;
    }

    return 0;
}

void uvc_device_deinit(void)
{
    if (0 > video_dev_fd) {
        return;
    }

    close(video_dev_fd);
    video_dev_fd = -1;
}

int uvc_device_create_buffer_pool(uint32_t buffer_size, uint32_t buffer_count)
{
    struct usbd_video_create_pool_cfg_t cfg;

    cfg.buffer_size  = buffer_size;
    cfg.buffer_count = buffer_count;

    return uvc_device_ioctl(IOCTL_USBD_VIDEO_CREATE_POOL, &cfg);
}

int uvc_device_get_buf(uint32_t** buffer, uint32_t* max_size)
{
    struct usbd_video_buffer_wrap_t buff;

    if (!buffer || !max_size) {
        return -1;
    }

    if (0x00 != uvc_device_ioctl(IOCTL_USBD_VIDEO_GET_BUFFER, &buff)) {
        return -2;
    }

    if (!buff.user_buffer || !buff.buffer_size) {
        return -1;
    }

    *buffer   = buff.user_buffer;
    *max_size = buff.buffer_size;

    return 0;
}

int uvc_device_put_buf(uint32_t* buffer, uint32_t size)
{
    struct usbd_video_buffer_wrap_t buff;

    buff.user_buffer = buffer;
    buff.buffer_size = size;

    return uvc_device_ioctl(IOCTL_USBD_VIDEO_PUT_BUFFER, &buff);
}

int uvc_device_conf(struct uvc_device_conf_t* cfg) { return uvc_device_ioctl(IOCTL_USBD_VIDEO_CONFIGURE, cfg); }

int uvc_device_start() { return uvc_device_ioctl(IOCTL_USBD_VIDEO_STREAM_ON, NULL); }

int uvc_device_stop() { return uvc_device_ioctl(IOCTL_USBD_VIDEO_STREAM_OFF, NULL); }

int uvc_device_get_state(int* opened) { return uvc_device_ioctl(IOCTL_USBD_VIDEO_DEV_STATE, opened); }
