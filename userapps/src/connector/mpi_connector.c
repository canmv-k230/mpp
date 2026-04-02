#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "k_connector_comm.h"
#include "k_connector_ioctl.h"
#include "k_vo_comm.h"
#include "mpi_connector_api.h"

#define pr_debug(...) // printf(__VA_ARGS__)
#define pr_info(...)  // printf(__VA_ARGS__)
#define pr_warn(...)  // printf(__VA_ARGS__)
#define pr_err(...)   printf(__VA_ARGS__)

k_s32 kd_mpi_connector_open(const char *connector_name)
{
    (void)connector_name;

    int fd;

    fd = open("/dev/connector", O_RDWR);
    if (fd < 0) {
        pr_err("%s, open /dev/connector failed\n", __func__);
        return -1;
    }

    return fd;
}

k_s32 kd_mpi_connector_close(k_s32 fd)
{
    if (fd < 0) {
        pr_err("%s, invalid fd\n", __func__);
        return -1;
    }

    close(fd);
    return 0;
}

k_s32 kd_mpi_get_connector_info(k_connector_type connector_type, k_connector_info* connector_info)
{
    int fd = -1;
    k_s32 ret;

    if (!connector_info) {
        pr_err("%s, connector_info is null\n", __func__);
        return K_ERR_VO_NULL_PTR;
    }

    fd = open("/dev/connector", O_RDWR);
    if (fd < 0) {
        pr_err("%s, open /dev/connector failed\n", __func__);
        return K_ERR_UNEXIST;
    }

    memset(connector_info, 0, sizeof(*connector_info));
    connector_info->type = connector_type;

    ret = ioctl(fd, KD_IOC_CONNECTOR_GET_PANEL_INFO, connector_info);
    if (ret != 0) {
        pr_err("%s, ioctl error(%d), type=%d\n", __func__, ret, connector_type);
        ret = K_ERR_UNEXIST;
    }

    close(fd);

    return ret;
}

k_s32 kd_mpi_connector_init(k_s32 fd, k_connector_info info)
{
    k_s32 ret;
    k_connector_init_params init_params;

    memset(&init_params, 0, sizeof(init_params));
    init_params.bg_color = info.bg_color;
    init_params.connector_type = info.type;

    if (info.type == VIRTUAL_DISPLAY_DEVICE) {
        init_params.virtual_hdisplay = info.timing.hactive;
        init_params.virtual_vdisplay = info.timing.vactive;
        init_params.virtual_fps = info.timing.pclk_khz;
    }

    ret = ioctl(fd, KD_IOC_CONNECTOR_SET_PAENL_INIT, &init_params);
    if (ret != 0) {
        pr_err("%s, error(%d)\n", __func__, ret);
        return K_ERR_VO_NOT_SUPPORT;
    }

    return ret;
}

k_s32 kd_mpi_connector_power_set(k_s32 fd, k_bool on)
{
    k_s32 ret;

    if (fd < 0) {
        pr_err("%s, invalid fd\n", __func__);
        return -1;
    }

    ret = ioctl(fd, KD_IOC_CONNECTOR_SET_PANEL_POWER_OFF, &on);
    if (ret != 0) {
        pr_err("%s, error(%d)\n", __func__, ret);
        return K_ERR_VO_NOT_SUPPORT;
    }

    return ret;
}

k_s32 kd_mpi_connector_id_get(k_s32 fd, k_u32 *id)
{
    k_s32 ret;

    if (fd < 0) {
        pr_err("%s, invalid fd\n", __func__);
        return -1;
    }

    if (!id) {
        pr_err("%s, sensor_id is null\n", __func__);
        return K_ERR_VO_NULL_PTR;
    }

    ret = ioctl(fd, KD_IOC_CONNECTOR_GET_PAENL_ID, id);
    if (ret != 0) {
        pr_err("%s, error(%d)\n", __func__, ret);
        return K_ERR_VO_NOT_SUPPORT;
    }

    return ret;
}
