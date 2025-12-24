/* Copyright (c) 2023, Canaan Bright Sight Co., Ltd
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

#ifdef RT_USING_POSIX
#include <dfs_posix.h>
#include <dfs_poll.h>
#include <posix_termios.h>
#endif

#include "connector_dev.h"
#include "k_connector_comm.h"
#include "sysctl_pwr.h"

#include "k_autoconf_comm.h"

static k_s32 connector_dev_open(struct dfs_fd *file)
{
    struct rt_device *device;
	struct connector_driver_dev *pdriver_dev;
	k_s32 ret = 0;

    //rt_kprintf("%s enter, device(%p)\n", __func__, file->fnode->data);

    device = file->fnode->data;
	if (device == NULL) {
		rt_kprintf("%s: device is null\n", __func__);
		return -ENOMEM;
	}
    pdriver_dev = (struct connector_driver_dev *)device;

    sysctl_pwr_up(SYSCTL_PD_DISP);

	//rt_kprintf("%s exit\n", __func__);

	return 0;
}


static k_s32 connector_dev_close(struct dfs_fd *file)
{
    struct rt_device *device;
	struct connector_driver_dev *pdriver_dev;
	k_s32 ret = 0;

    //rt_kprintf("%s enter, device(%p)\n", __func__, file->fnode->data);

    device = file->fnode->data;
	if (device == NULL) {
		rt_kprintf("%s: device is null\n", __func__);
		return -ENOMEM;
	}
    pdriver_dev = (struct connector_driver_dev *)device;

	//rt_kprintf("%s exit\n", __func__);

	return 0;
}


static k_s32 connector_dev_ioctl(struct dfs_fd *file, k_s32 cmd, void *args)
{
    struct rt_device *device;
	struct connector_driver_dev *pdriver_dev;
	k_s32 ret = 0;

    device = file->fnode->data;
	if (device == NULL) {
		rt_kprintf("%s: device is null\n", __func__);
		return -ENOMEM;
	}

    pdriver_dev = (struct connector_driver_dev *)device;

	rt_mutex_take(&pdriver_dev->connector_mutex, RT_WAITING_FOREVER);
	ret = connector_priv_ioctl(pdriver_dev, cmd, args);
    rt_mutex_release(&pdriver_dev->connector_mutex);

    return ret;
}

static const struct dfs_file_ops connector_dev_fops = {
    .open = connector_dev_open,
    .close = connector_dev_close,
    .ioctl = connector_dev_ioctl,
};


k_s32 connector_drv_dev_init(struct connector_driver_dev *pdriver_dev)
{
    struct rt_device *device;
    char dev_name[32];
    k_s32 ret = 0;

    RT_ASSERT(pdriver_dev != NULL)

    device = &pdriver_dev->parent;
    rt_snprintf(dev_name, sizeof(dev_name), "connector_%s", pdriver_dev->connector_name);

    rt_mutex_init(&pdriver_dev->connector_mutex, "connector_mutex", RT_IPC_FLAG_PRIO);

    ret = rt_device_register(device, dev_name, RT_DEVICE_FLAG_RDWR);
    if (ret) {
        rt_kprintf("connector device register fail\n");
        return ret;
    }

    device->fops = &connector_dev_fops;
    device->user_data = pdriver_dev;
    return 0;
}

struct connector_driver_dev* connector_drv_list[] = {
#if defined (CONFIG_MPP_ENABLE_DSI_DEBUGGER)
    &debugger_connector_dev,
#endif // CONFIG_MPP_ENABLE_DSI_DEBUGGER

#ifdef CONFIG_MPP_DSI_ENABLE_VIRT
    &virtdev_connector_drv,
#endif // CONFIG_MPP_DSI_ENABLE_VIRT

#ifdef CONFIG_MPP_DSI_ENABLE_HDMI_LT9611
    &lt9611_connector_drv,
#endif // CONFIG_MPP_DSI_ENABLE_HDMI_LT9611

#ifdef CONFIG_MPP_DSI_ENABLE_LCD_HX8399
    &hx8399_connector_drv,
#endif // CONFIG_MPP_DSI_ENABLE_LCD_HX8399

#ifdef CONFIG_MPP_DSI_ENABLE_LCD_ST7701
    &st7701_connector_drv,
#endif // CONFIG_MPP_DSI_ENABLE_LCD_ST7701

#ifdef CONFIG_MPP_DSI_ENABLE_LCD_ILI9806
    &ili9806_connector_drv,
#endif // CONFIG_MPP_DSI_ENABLE_LCD_ILI9806

#ifdef CONFIG_MPP_DSI_ENABLE_LCD_ILI9881
    &ili9881c_connector_drv,
#endif // CONFIG_MPP_DSI_ENABLE_LCD_ILI9881

#ifdef CONFIG_MPP_DSI_ENABLE_LCD_NT35516
    &nt35516_connector_drv,
#endif // CONFIG_MPP_DSI_ENABLE_LCD_NT35516

#ifdef CONFIG_MPP_DSI_ENABLE_LCD_NT35532
    &nt35532_connector_drv,
#endif // CONFIG_MPP_DSI_ENABLE_LCD_NT35532

#ifdef CONFIG_MPP_DSI_ENABLE_LCD_GC9503
    &gc9503_connector_drv,
#endif // CONFIG_MPP_DSI_ENABLE_LCD_GC9503

#ifdef CONFIG_MPP_DSI_ENABLE_LCD_ST7102
    &st7102_connector_drv,
#endif // CONFIG_MPP_DSI_ENABLE_LCD_ST7102

#ifdef CONFIG_MPP_DSI_ENABLE_LCD_AML020T
    &aml020t_connector_drv,
#endif // CONFIG_MPP_DSI_ENABLE_LCD_AML020T

    NULL,
};

k_s32 connector_device_init(void)
{
    int index = 0;

    while(connector_drv_list[index]) {
        connector_drv_dev_init(connector_drv_list[index]);
        index++;
    }

    return 0;
}

#if !defined (CONFIG_SDK_ENABLE_CANMV)

#define CONNECTOR_TYPE_NAME(x) { .type = x, .name = #x }

struct connector_type_name {
    k_connector_type type;
    const char *name;
};

static const struct connector_type_name cth_table[] = {
    CONNECTOR_TYPE_NAME(HX8377_V2_MIPI_4LAN_1080X1920_30FPS),
    CONNECTOR_TYPE_NAME(ILI9806_MIPI_2LAN_480X800_30FPS),
    CONNECTOR_TYPE_NAME(ILI9881_MIPI_4LAN_800X1280_60FPS),

    CONNECTOR_TYPE_NAME(ST7701_V1_MIPI_2LAN_480X800_30FPS),
    CONNECTOR_TYPE_NAME(ST7701_V1_MIPI_2LAN_480X854_30FPS),
    CONNECTOR_TYPE_NAME(ST7701_V1_MIPI_2LAN_480X640_30FPS),
    CONNECTOR_TYPE_NAME(ST7701_V1_MIPI_2LAN_368X544_60FPS),
    CONNECTOR_TYPE_NAME(AML020T_MIPI_2LAN_480X360_30FPS),

    CONNECTOR_TYPE_NAME(LT9611_MIPI_ADAPT_RESOLUTION),
    CONNECTOR_TYPE_NAME(LT9611_MIPI_4LAN_1920X1080_30FPS),
    CONNECTOR_TYPE_NAME(LT9611_MIPI_4LAN_1920X1080_60FPS),
    CONNECTOR_TYPE_NAME(LT9611_MIPI_4LAN_1920X1080_50FPS),
    CONNECTOR_TYPE_NAME(LT9611_MIPI_4LAN_1920X1080_25FPS),
    CONNECTOR_TYPE_NAME(LT9611_MIPI_4LAN_1920X1080_24FPS),
    CONNECTOR_TYPE_NAME(LT9611_MIPI_4LAN_1280X720_60FPS),
    CONNECTOR_TYPE_NAME(LT9611_MIPI_4LAN_1280X720_50FPS),
    CONNECTOR_TYPE_NAME(LT9611_MIPI_4LAN_1280X720_30FPS),
    CONNECTOR_TYPE_NAME(LT9611_MIPI_4LAN_640X480_60FPS),

    CONNECTOR_TYPE_NAME(VIRTUAL_DISPLAY_DEVICE),
#if defined (CONFIG_MPP_ENABLE_DSI_DEBUGGER)
    CONNECTOR_TYPE_NAME(DSI_DEBUGGER_DEVICE),
#endif
    /* last type set to U32 MAX */
    {__UINT32_MAX__, "UNKNOWN"},
};

static void list_connector(k_s32 argc, char** argv)
{
    rt_kprintf("Connector Type List:\n");

    for(size_t i = 0; i < sizeof(cth_table) / sizeof(cth_table[0]); i++) {
        rt_kprintf("%17d -> %s\n", cth_table[i].type, cth_table[i].name);
    }

    return;
}

MSH_CMD_EXPORT(list_connector, list connector type)

#endif
