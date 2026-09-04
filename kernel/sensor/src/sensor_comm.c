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

#include "k_type.h"
#include "sensor_dev.h"
#include "k_sensor_ioctl.h"
#include "k_sensor_comm.h"

void sensor_set_mclk(const k_sensor_mclk_setting *setting)
{
    extern k_s32 vicap_set_mclk(k_vicap_mclk *mclk);

    k_vicap_mclk mclk;

    for (k_s32 idx = 0; idx < SENSOR_MCLK_MAX - 1; idx++) {
        const k_sensor_mclk *sensor_mclk = &setting[idx].setting;

        mclk.id = (k_vicap_mclk_id)sensor_mclk->id;
        mclk.mclk_div = sensor_mclk->mclk_div;
        mclk.mclk_sel = (k_vicap_mclk_sel)sensor_mclk->mclk_sel;

        if(setting[idx].mclk_setting_en) {
            mclk.mclk_en = 1;
        } else {
            mclk.mclk_en = 0;
        }
        vicap_set_mclk(&mclk);
    }
}

k_s32 sensor_reg_read(k_sensor_i2c_info *i2c_info, k_u16 reg_addr, k_u16 *buf)
{
    struct rt_i2c_msg msg[2];
    k_u8 i2c_reg[2];
    k_u8 i2c_buf[2];

    RT_ASSERT(i2c_info != RT_NULL);

    if (i2c_info->reg_addr_size == SENSOR_REG_VALUE_8BIT) {
        i2c_reg[0] = reg_addr & 0xff;
    } else if (i2c_info->reg_addr_size == SENSOR_REG_VALUE_16BIT) {
        i2c_reg[0] = reg_addr >> 8;
        i2c_reg[1] = reg_addr & 0xff;
    }

    msg[0].addr  = i2c_info->slave_addr;
    msg[0].flags = RT_I2C_WR;
    msg[0].len   = i2c_info->reg_addr_size;
    msg[0].buf   = i2c_reg;

    msg[1].addr  = i2c_info->slave_addr;
    msg[1].flags = RT_I2C_RD;
    msg[1].len   = i2c_info->reg_val_size;
    msg[1].buf   = i2c_buf;

    if (rt_i2c_transfer(i2c_info->i2c_bus, msg, 2) == 2)
    {
        *buf = (i2c_info->reg_val_size == SENSOR_REG_VALUE_8BIT) ? i2c_buf[0] : (i2c_buf[0] << 8) | i2c_buf[1];
        // rt_kprintf("sensor_reg_read: [0x%04x] = [0x%02x] i2c_info->size is %d \n", reg_addr, *buf, i2c_info->size);
        return RT_EOK;
    }
    // rt_kprintf("%s err.\n", __func__);

    return RT_ERROR;
}

k_s32 sensor_reg_write(k_sensor_i2c_info *i2c_info, k_u16 reg_addr, k_u16 reg_val)
{
    struct rt_i2c_msg msgs;
    k_u8 buf[4];
    k_u8 len = 0;

    RT_ASSERT(i2c_info != RT_NULL);

    if (i2c_info->reg_addr_size == SENSOR_REG_VALUE_8BIT) {
        buf[len++] = reg_addr & 0xff;
    } else if (i2c_info->reg_addr_size == SENSOR_REG_VALUE_16BIT) {
        buf[len++] = reg_addr >> 8;
        buf[len++] = reg_addr & 0xff;
    }

    if (i2c_info->reg_val_size == SENSOR_REG_VALUE_8BIT) {
        buf[len++] = reg_val & 0xff;
    } else if (i2c_info->reg_val_size == SENSOR_REG_VALUE_16BIT) {
        buf[len++] = reg_val >> 8;
        buf[len++] = reg_val & 0xff;
    }

    msgs.addr = i2c_info->slave_addr;
    msgs.flags = RT_I2C_WR;
    msgs.buf = buf;
    msgs.len = len;

    if (rt_i2c_transfer(i2c_info->i2c_bus, &msgs, 1) == 1)
    {
        //rt_kprintf("sensor_reg_wirte: [0x%04x] = [0x%02x]\n", reg_addr, reg_val);
        return RT_EOK;
    }

    return RT_ERROR;
}

k_s32 sensor_reg_list_write(k_sensor_i2c_info *i2c_info, const k_sensor_reg *reg_list)
{
	k_s32 ret = 0;
	k_u32 i;

	for (i = 0; reg_list[i].addr != REG_NULL; i++) {
		ret = sensor_reg_write(i2c_info, reg_list[i].addr, reg_list[i].val);
        if (ret) {
            rt_kprintf("%s err, [0x%04x] = [0x%02x]\n", __func__, reg_list[i].addr, reg_list[i].val);
            return -1;
        }
	}
	return ret;
}

k_s32 sensor_reg_list_read(k_sensor_i2c_info *i2c_info, const k_sensor_reg *reg_list)
{
	k_s32 ret = 0;
	k_u16 i, val;

	for (i = 0; reg_list[i].addr != REG_NULL; i++) {
		ret = sensor_reg_read(i2c_info, reg_list[i].addr, &val);
        if (ret) {
            rt_kprintf("%s err, [0x%04x] = [0x%02x]\n", __func__, reg_list[i].addr, reg_list[i].val);
            return -1;
        }
        rt_kprintf("{%04x, %02x}\n", reg_list[i].addr, val);
	}
	return ret;
}


/**
 * @brief Get sensor integration time range
 * @note This is a common implementation for all sensors
 *       The integration time range is stored in current_mode->ae_info
 * @param ctx: sensor device context
 * @param range: pointer to integration time range structure
 * @return 0 on success, -1 on failure
 */
k_s32 sensor_get_exposure_time_range_common(void *ctx, k_sensor_exposure_time_range *range)
{
    k_s32 ret = 0;
    struct sensor_driver_dev *dev = ctx;
    k_sensor_mode *current_mode = &dev->current_sensor_mode;


    if (!range) {
        rt_kprintf("%s, range is NULL\n", __func__);
        return -1;
    }

    range->max_intg_time_us = current_mode->ae_info.max_integraion_time * 1000000.0f;
    range->min_intg_time_us = current_mode->ae_info.min_integraion_time * 1000000.0f;

    return ret;
}


/**
 * @brief Get sensor gain range
 * @note This is a common implementation for all sensors
 *       The gain range is stored in current_mode->ae_info.a_gain
 * @param ctx: sensor device context
 * @param range: pointer to gain range structure
 * @return 0 on success, -1 on failure
 */
k_s32 sensor_get_gain_range_common(void *ctx, k_sensor_gain_info *range)
{
    k_s32 ret = 0;
    struct sensor_driver_dev *dev = ctx;
    k_sensor_mode *current_mode = &dev->current_sensor_mode;

    if (!range) {
        rt_kprintf("%s, range is NULL\n", __func__);
        return -1;
    }

    // Get gain range from ae_info.a_gain
    range->min = current_mode->ae_info.a_gain.min;
    range->max = current_mode->ae_info.a_gain.max;
    range->step = current_mode->ae_info.a_gain.step;

    return ret;
}

k_s32 sensor_priv_ioctl(struct sensor_driver_dev *dev, k_u32 cmd, void *args)
{
	k_s32 ret = -1;
	if (!dev) {
        rt_kprintf("%s error, dev null\n", __func__);
		return ret;
	}

    //rt_kprintf("[%s:%d]cmd 0x%08x\n", __func__, __LINE__, cmd);
	switch (cmd) {
        case KD_IOC_SENSOR_S_POWER:
        {
			k_s32 power_on;
			if (dev->sensor_func.sensor_power == NULL) {
                rt_kprintf("%s (%s)sensor_power is null\n", __func__, dev->sensor_name);
				return -1;
			}

            if (sizeof(k_s32) != lwp_get_from_user(&power_on, args, sizeof(k_s32))){
                rt_kprintf("%s:%d lwp_get_from_user err\n", __func__, __LINE__);
                return -1;
            }

            if (power_on && dev->power_flag) {
                ret = 0;
                break;
            }

			ret = dev->sensor_func.sensor_power(dev, power_on);
            if (!power_on) {
                dev->power_flag = K_FALSE;
                dev->init_flag = K_FALSE;
                dev->mode_init_flag = K_FALSE;
            } else if (!ret) {
                dev->power_flag = K_TRUE;
            }
			break;
		}
        case KD_IOC_SENSOR_S_INIT:
        {
			k_sensor_mode sensor_mode;
			if (dev->sensor_func.sensor_init == NULL) {
                rt_kprintf("%s (%s)sensor_init is null\n", __func__, dev->sensor_name);
				return -1;
			}

            if (sizeof(k_sensor_mode) != lwp_get_from_user(&sensor_mode, args, sizeof(k_sensor_mode))){
                rt_kprintf("%s:%d lwp_get_from_user err\n", __func__, __LINE__);
                return -1;
            }

            if (dev->mode_init_flag &&
                dev->current_sensor_mode.sensor_type == sensor_mode.sensor_type) {
                ret = 0;
                break;
            }

            dev->mode_init_flag = K_FALSE;
			ret = dev->sensor_func.sensor_init(dev, sensor_mode);
            if (!ret) {
                dev->mode_init_flag = K_TRUE;
            }
			break;
		}
        case KD_IOC_SENSOR_G_ID:
        {
			k_u32 chip_id = 0;
			if (dev->sensor_func.sensor_get_chip_id == NULL) {
                rt_kprintf("%s (%s)sensor_get_chip_id is null\n", __func__, dev->sensor_name);
				return -1;
			}
			ret = dev->sensor_func.sensor_get_chip_id(dev, &chip_id);
            if (ret) {
                // rt_kprintf("%s (%s)sensor_get_chip_id err\n", __func__, dev->sensor_name);
                return -1;
            }
            if (sizeof(chip_id) != lwp_put_to_user(args, &chip_id, sizeof(chip_id))){
                rt_kprintf("%s:%d lwp_put_to_user err\n", __func__, __LINE__);
                return -1;
            }
			break;
		}
        case KD_IOC_SENSOR_REG_READ:
        {
            k_sensor_reg reg;

            if (sizeof(k_sensor_reg) != lwp_get_from_user(&reg, args, sizeof(k_sensor_reg))){
                rt_kprintf("%s:%d lwp_get_from_user err\n", __func__, __LINE__);
                return -1;
            }

            k_u16 reg_val;
            ret = sensor_reg_read(&dev->i2c_info, reg.addr, &reg_val);
            if (ret) {
                // rt_kprintf("%s:%d sensor_reg_read err\n", __func__, __LINE__);
                return -1;
            }
            reg.val = reg_val;

            if (sizeof(k_sensor_reg) != lwp_put_to_user(args, &reg, sizeof(k_sensor_reg))){
                rt_kprintf("%s:%d lwp_put_to_user err\n", __func__, __LINE__);
                return -1;
            }
            break;
        }
        case KD_IOC_SENSOR_REG_WRITE:
        {
            k_sensor_reg reg;

            if (sizeof(k_sensor_reg) != lwp_get_from_user(&reg, args, sizeof(k_sensor_reg))){
                rt_kprintf("%s:%d lwp_get_from_user err\n", __func__, __LINE__);
                return -1;
            }

            ret = sensor_reg_write(&dev->i2c_info, reg.addr, reg.val);
            if (ret) {
                rt_kprintf("%s:%d sensor_reg_write err\n", __func__, __LINE__);
                return -1;
            }
            break;
        }
        case KD_IOC_SENSOR_G_MODE:
        {
            k_sensor_mode mode;
			if (dev->sensor_func.sensor_get_mode == NULL) {
                rt_kprintf("%s (%s)sensor_get_mode is null\n", __func__, dev->sensor_name);
				return -1;
			}

            if (sizeof(k_sensor_mode) != lwp_get_from_user(&mode, args, sizeof(k_sensor_mode))){
                rt_kprintf("%s:%d lwp_get_from_user err\n", __func__, __LINE__);
                return -1;
            }

			ret = dev->sensor_func.sensor_get_mode(dev, &mode);
            if (ret) {
                rt_kprintf("%s (%s)sensor_get_mode err\n", __func__, dev->sensor_name);
                return -1;
            }

            if (sizeof(k_sensor_mode) != lwp_put_to_user(args, &mode, sizeof(k_sensor_mode))){
                rt_kprintf("%s:%d lwp_put_to_user err\n", __func__, __LINE__);
                return -1;
            }

            break;
        }
        case KD_IOC_SENSOR_S_MODE:
        {
            k_sensor_mode mode;

            if (sizeof(k_sensor_mode) != lwp_get_from_user(&mode, args, sizeof(k_sensor_mode))){
                rt_kprintf("%s:%d lwp_get_from_user err\n", __func__, __LINE__);
                return -1;
            }

			if (dev->sensor_func.sensor_set_mode == NULL) {
                rt_kprintf("%s (%s)sensor_set_mode is null\n", __func__, dev->sensor_name);
				return -1;
			}
			ret = dev->sensor_func.sensor_set_mode(dev, mode);
            if (ret) {
                rt_kprintf("%s (%s)sensor_set_mode err\n", __func__, dev->sensor_name);
                return -1;
            }

            break;
        }
        case KD_IOC_SENSOR_ENUM_MODE:
        {
            k_sensor_enum_mode enum_mode;

            if (sizeof(k_sensor_enum_mode) != lwp_get_from_user(&enum_mode, args, sizeof(k_sensor_enum_mode))){
                rt_kprintf("%s:%d lwp_get_from_user err\n", __func__, __LINE__);
                return -1;
            }
            if (dev->sensor_func.sensor_enum_mode == NULL) {
                rt_kprintf("%s (%s)sensor_get_mode is null\n", __func__, dev->sensor_name);
                return -1;
            }
            ret = dev->sensor_func.sensor_enum_mode(dev, &enum_mode);
            if (ret) {
                rt_kprintf("%s (%s)sensor_get_mode err\n", __func__, dev->sensor_name);
                return -1;
            }
            if (sizeof(k_sensor_enum_mode) != lwp_put_to_user(args, &enum_mode, sizeof(k_sensor_enum_mode))){
                rt_kprintf("%s:%d lwp_put_to_user err\n", __func__, __LINE__);
                return -1;
            }

            break;
        }
        case KD_IOC_SENSOR_G_CAPS:
        {
            k_sensor_caps caps;

            if (sizeof(k_sensor_caps) != lwp_get_from_user(&caps, args, sizeof(k_sensor_caps))){
                rt_kprintf("%s:%d lwp_get_from_user err\n", __func__, __LINE__);
                return -1;
            }
            if (dev->sensor_func.sensor_get_caps == NULL) {
                rt_kprintf("%s (%s)sensor_get_caps is null\n", __func__, dev->sensor_name);
                return -1;
            }
            ret = dev->sensor_func.sensor_get_caps(dev, &caps);
            if (ret) {
                rt_kprintf("%s (%s)sensor_get_caps err\n", __func__, dev->sensor_name);
                return -1;
            }
            if (sizeof(k_sensor_caps) != lwp_put_to_user(args, &caps, sizeof(k_sensor_caps))){
                rt_kprintf("%s:%d lwp_put_to_user err\n", __func__, __LINE__);
                return -1;
            }

            break;
        }
        case KD_IOC_SENSOR_CHECK_CONN:
        {
            k_s32 conn = 0;
            if (dev->sensor_func.sensor_conn_check == NULL) {
                rt_kprintf("%s (%s)sensor_get_chip_id is null\n", __func__, dev->sensor_name);
                return -1;
            }
            ret = dev->sensor_func.sensor_conn_check(dev, &conn);
            if (ret) {
                rt_kprintf("%s (%s)sensor_get_chip_id err\n", __func__, dev->sensor_name);
                return -1;
            }
            if (sizeof(k_s32) != lwp_put_to_user(args, &conn, sizeof(k_s32))){
                rt_kprintf("%s:%d lwp_put_to_user err\n", __func__, __LINE__);
                return -1;
            }
            break;
        }
        case KD_IOC_SENSOR_S_STREAM:
        {
            k_s32 enable = 0;
			if (dev->sensor_func.sensor_set_stream == NULL) {
                rt_kprintf("%s (%s)sensor_set_stream is null\n", __func__, dev->sensor_name);
				return -1;
			}

            if (sizeof(k_s32) != lwp_get_from_user(&enable, args, sizeof(k_s32))){
                rt_kprintf("%s:%d lwp_get_from_user err\n", __func__, __LINE__);
                return -1;
            }

			ret = dev->sensor_func.sensor_set_stream(dev, enable);
            if (ret) {
                rt_kprintf("%s (%s)sensor_set_stream err\n", __func__, dev->sensor_name);
                return -1;
            }
            break;
        }
        case KD_IOC_SENSOR_G_AGAIN:
        {
            k_sensor_gain gain;
            if (sizeof(k_sensor_gain) != lwp_get_from_user(&gain, args, sizeof(k_sensor_gain))){
                rt_kprintf("%s:%d lwp_get_from_user err\n", __func__, __LINE__);
                return -1;
            }
            if (dev->sensor_func.sensor_get_again == NULL) {
                rt_kprintf("%s (%s)sensor_get_again is null\n", __func__, dev->sensor_name);
                return -1;
            }
            ret = dev->sensor_func.sensor_get_again(dev, &gain);
            if (ret) {
                rt_kprintf("%s (%s)sensor_get_again err\n", __func__, dev->sensor_name);
                return -1;
            }
            if (sizeof(k_sensor_gain) != lwp_put_to_user(args, &gain, sizeof(k_sensor_gain))){
                rt_kprintf("%s:%d lwp_put_to_user err\n", __func__, __LINE__);
                return -1;
            }

            break;
        }
        case KD_IOC_SENSOR_S_AGAIN:
        {
            k_sensor_gain gain;

            if (sizeof(k_sensor_gain) != lwp_get_from_user(&gain, args, sizeof(k_sensor_gain))){
                rt_kprintf("%s:%d lwp_get_from_user err\n", __func__, __LINE__);
                return -1;
            }

            if (dev->sensor_func.sensor_set_again == NULL) {
                rt_kprintf("%s (%s)sensor_set_again is null\n", __func__, dev->sensor_name);
                return -1;
            }
            ret = dev->sensor_func.sensor_set_again(dev, gain);
            if (ret) {
                rt_kprintf("%s (%s)sensor_set_again err\n", __func__, dev->sensor_name);
                return -1;
            }

            break;
        }
        case KD_IOC_SENSOR_G_DGAIN:
        {
            k_sensor_gain gain;
            if (sizeof(k_sensor_gain) != lwp_get_from_user(&gain, args, sizeof(k_sensor_gain))){
                rt_kprintf("%s:%d lwp_get_from_user err\n", __func__, __LINE__);
                return -1;
            }
            if (dev->sensor_func.sensor_get_dgain == NULL) {
                rt_kprintf("%s (%s)sensor_get_dgain is null\n", __func__, dev->sensor_name);
                return -1;
            }
            ret = dev->sensor_func.sensor_get_dgain(dev, &gain);
            if (ret) {
                rt_kprintf("%s (%s)sensor_get_dgain err\n", __func__, dev->sensor_name);
                return -1;
            }
            if (sizeof(k_sensor_gain) != lwp_put_to_user(args, &gain, sizeof(k_sensor_gain))){
                rt_kprintf("%s:%d lwp_put_to_user err\n", __func__, __LINE__);
                return -1;
            }

            break;
        }
        case KD_IOC_SENSOR_S_DGAIN:
        {
            k_sensor_gain gain;

            if (sizeof(k_sensor_gain) != lwp_get_from_user(&gain, args, sizeof(k_sensor_gain))){
                rt_kprintf("%s:%d lwp_get_from_user err\n", __func__, __LINE__);
                return -1;
            }

            if (dev->sensor_func.sensor_set_dgain == NULL) {
                rt_kprintf("%s (%s)sensor_get_dgain is null\n", __func__, dev->sensor_name);
                return -1;
            }
            ret = dev->sensor_func.sensor_set_dgain(dev, gain);
            if (ret) {
                rt_kprintf("%s (%s)sensor_get_dgain err\n", __func__, dev->sensor_name);
                return -1;
            }

            break;
        }
        case KD_IOC_SENSOR_G_INTG_TIME:
        {
            k_sensor_intg_time time;

            if (dev->sensor_func.sensor_get_intg_time == NULL) {
                rt_kprintf("%s (%s)sensor_get_intg_time is null\n", __func__, dev->sensor_name);
                return -1;
            }
            if (sizeof(k_sensor_intg_time) != lwp_get_from_user(&time, args, sizeof(k_sensor_intg_time))){
                rt_kprintf("%s:%d lwp_get_from_user err\n", __func__, __LINE__);
                return -1;
            }
            ret = dev->sensor_func.sensor_get_intg_time(dev, &time);
            if (ret) {
                rt_kprintf("%s (%s)k_sensor_intg_time err\n", __func__, dev->sensor_name);
                return -1;
            }
            if (sizeof(k_sensor_intg_time) != lwp_put_to_user(args, &time, sizeof(k_sensor_intg_time))){
                rt_kprintf("%s:%d lwp_put_to_user err\n", __func__, __LINE__);
                return -1;
            }

            break;
        }
        case KD_IOC_SENSOR_S_INTG_TIME:
        {
            k_sensor_intg_time time;

            if (sizeof(k_sensor_intg_time) != lwp_get_from_user(&time, args, sizeof(k_sensor_intg_time))){
                rt_kprintf("%s:%d lwp_get_from_user err\n", __func__, __LINE__);
                return -1;
            }

            if (dev->sensor_func.sensor_set_intg_time == NULL) {
                rt_kprintf("%s (%s)sensor_set_intg_time is null\n", __func__, dev->sensor_name);
                return -1;
            }
            ret = dev->sensor_func.sensor_set_intg_time(dev, time);
            if (ret) {
                rt_kprintf("%s (%s)sensor_set_intg_time err\n", __func__, dev->sensor_name);
                return -1;
            }

            break;
        }
        case KD_IOC_SENSOR_GET_EXP_PRAM:
        {
            k_sensor_exposure_param exp_parm;

            if (dev->sensor_func.sensor_get_exp_parm == NULL) {
                rt_kprintf("%s (%s)sensor_get_dgain is null\n", __func__, dev->sensor_name);
                return -1;
            }
            if (sizeof(k_sensor_exposure_param) != lwp_get_from_user(&exp_parm, args, sizeof(k_sensor_exposure_param))){
                rt_kprintf("%s:%d lwp_get_from_user err\n", __func__, __LINE__);
                return -1;
            }
            ret = dev->sensor_func.sensor_get_exp_parm(dev, &exp_parm);
            if (ret) {
                rt_kprintf("%s (%s)sensor_get_dgain err\n", __func__, dev->sensor_name);
                return -1;
            }
            if (sizeof(k_sensor_exposure_param) != lwp_put_to_user(args, &exp_parm, sizeof(k_sensor_exposure_param))){
                rt_kprintf("%s:%d lwp_put_to_user err\n", __func__, __LINE__);
                return -1;
            }

            break;
        }
        case KD_IOC_SENSOR_SET_EXP_PRAM:
        {
            k_sensor_exposure_param exp_parm;

            if (sizeof(k_sensor_exposure_param) != lwp_get_from_user(&exp_parm, args, sizeof(k_sensor_exposure_param))){
                rt_kprintf("%s:%d lwp_get_from_user err\n", __func__, __LINE__);
                return -1;
            }

            if (dev->sensor_func.sensor_set_exp_parm == NULL) {
                rt_kprintf("%s (%s)sensor_set_exp_parm is null\n", __func__, dev->sensor_name);
                return -1;
            }
            ret = dev->sensor_func.sensor_set_exp_parm(dev, exp_parm);
            if (ret) {
                rt_kprintf("%s (%s)sensor_set_exp_parm err\n", __func__, dev->sensor_name);
                return -1;
            }

            break;
        }
        case KD_IOC_SENSOR_G_FPS:
        {
            k_u32 fps;

            if (dev->sensor_func.sensor_get_fps == NULL) {
                rt_kprintf("%s (%s)sensor_get_fps is null\n", __func__, dev->sensor_name);
                return -1;
            }
            ret = dev->sensor_func.sensor_get_fps(dev, &fps);
            if (ret) {
                rt_kprintf("%s (%s)sensor_get_fps err\n", __func__, dev->sensor_name);
                return -1;
            }
            if (sizeof(k_u32) != lwp_put_to_user(args, &fps, sizeof(k_u32))){
                rt_kprintf("%s:%d lwp_put_to_user err\n", __func__, __LINE__);
                return -1;
            }

            break;
        }
        case KD_IOC_SENSOR_S_FPS:
        {
            k_u32 fps;
            if (sizeof(k_u32) != lwp_get_from_user(&fps, args, sizeof(k_u32))){
                rt_kprintf("%s:%d lwp_get_from_user err\n", __func__, __LINE__);
                return -1;
            }

            if (dev->sensor_func.sensor_set_fps == NULL) {
                rt_kprintf("%s (%s)sensor_set_fps is null\n", __func__, dev->sensor_name);
                return -1;
            }
            ret = dev->sensor_func.sensor_set_fps(dev, fps);
            if (ret) {
                rt_kprintf("%s (%s)sensor_set_fps err\n", __func__, dev->sensor_name);
                return -1;
            }

            break;
        }
        case KD_IOC_SENSOR_G_ISP_STATUS:
        {
            k_sensor_isp_status isp_status;

            if (dev->sensor_func.sensor_get_isp_status == NULL) {
                rt_kprintf("%s (%s)sensor_get_isp_status is null\n", __func__, dev->sensor_name);
                return -1;
            }
            ret = dev->sensor_func.sensor_get_isp_status(dev, &isp_status);
            if (ret) {
                rt_kprintf("%s (%s)sensor_get_fps err\n", __func__, dev->sensor_name);
                return -1;
            }
            if (sizeof(k_sensor_isp_status) != lwp_put_to_user(args, &isp_status, sizeof(k_sensor_isp_status))){
                rt_kprintf("%s:%d lwp_put_to_user err\n", __func__, __LINE__);
                return -1;
            }

            break;
        }
        case KD_IOC_SENSOR_S_BLC:
        {
            k_sensor_blc blc;
            if (sizeof(k_sensor_blc) != lwp_get_from_user(&blc, args, sizeof(k_sensor_blc))){
                rt_kprintf("%s:%d lwp_get_from_user err\n", __func__, __LINE__);
                return -1;
            }

            if (dev->sensor_func.sensor_set_blc == NULL) {
                rt_kprintf("%s (%s)sensor_set_blc is null\n", __func__, dev->sensor_name);
                return -1;
            }
            ret = dev->sensor_func.sensor_set_blc(dev, blc);
            if (ret) {
                rt_kprintf("%s (%s)sensor_set_blc err\n", __func__, dev->sensor_name);
                return -1;
            }

            break;
        }
        case KD_IOC_SENSOR_S_WB:
        {
            k_sensor_white_balance wb;
            if (sizeof(k_sensor_white_balance) != lwp_get_from_user(&wb, args, sizeof(k_sensor_white_balance))){
                rt_kprintf("%s:%d lwp_get_from_user err\n", __func__, __LINE__);
                return -1;
            }

            if (dev->sensor_func.sensor_set_wb == NULL) {
                rt_kprintf("%s (%s)sensor_set_wb is null\n", __func__, dev->sensor_name);
                return -1;
            }
            ret = dev->sensor_func.sensor_set_wb(dev, wb);
            if (ret) {
                rt_kprintf("%s (%s)sensor_set_wb err\n", __func__, dev->sensor_name);
                return -1;
            }

            break;
        }
        case KD_IOC_SENSOR_G_TPG:
        {
            k_sensor_test_pattern tpg;

            if (dev->sensor_func.sensor_get_tpg == NULL) {
                rt_kprintf("%s (%s)sensor_get_tpg is null\n", __func__, dev->sensor_name);
                return -1;
            }
            ret = dev->sensor_func.sensor_get_tpg(dev, &tpg);
            if (ret) {
                rt_kprintf("%s (%s)sensor_get_fps err\n", __func__, dev->sensor_name);
                return -1;
            }
            if (sizeof(k_sensor_test_pattern) != lwp_put_to_user(args, &tpg, sizeof(k_sensor_test_pattern))){
                rt_kprintf("%s:%d lwp_put_to_user err\n", __func__, __LINE__);
                return -1;
            }

            break;
        }
        case KD_IOC_SENSOR_S_TPG:
        {
            k_sensor_test_pattern tpg;
            if (sizeof(k_sensor_test_pattern) != lwp_get_from_user(&tpg, args, sizeof(k_sensor_test_pattern))){
                rt_kprintf("%s:%d lwp_get_from_user err\n", __func__, __LINE__);
                return -1;
            }

            if (dev->sensor_func.sensor_set_tpg == NULL) {
                rt_kprintf("%s (%s)sensor_set_tpg is null\n", __func__, dev->sensor_name);
                return -1;
            }
            ret = dev->sensor_func.sensor_set_tpg(dev, tpg);
            if (ret) {
                rt_kprintf("%s (%s)sensor_set_tpg err\n", __func__, dev->sensor_name);
                return -1;
            }

            break;
        }
        case KD_IOC_SENSOR_G_EXPAND_CURVE:
        {
            k_sensor_compand_curve compand_curve;

            if (dev->sensor_func.sensor_get_expand_curve == NULL) {
                rt_kprintf("%s (%s)sensor_get_expand_curve is null\n", __func__, dev->sensor_name);
                return -1;
            }
            ret = dev->sensor_func.sensor_get_expand_curve(dev, &compand_curve);
            if (ret) {
                rt_kprintf("%s (%s)sensor_get_fps err\n", __func__, dev->sensor_name);
                return -1;
            }
            if (sizeof(k_sensor_compand_curve) != lwp_put_to_user(args, &compand_curve, sizeof(k_sensor_compand_curve))){
                rt_kprintf("%s:%d lwp_put_to_user err\n", __func__, __LINE__);
                return -1;
            }

            break;
        }
        case KD_IOC_SENSOR_G_OTP_DATA:
        {
            k_sensor_otp_date otp_read_val;

            if ((sizeof(k_sensor_otp_date)) != lwp_get_from_user(&otp_read_val, args, sizeof(k_sensor_otp_date))){
                rt_kprintf("%s:%d lwp_get_from_user err\n", __func__, __LINE__);
                return -1;
            }

            if (dev->sensor_func.sensor_get_otp_data == NULL) {
                rt_kprintf("%s (%s)sensor_get_otp_data is null\n", __func__, dev->sensor_name);
                return -1;
            }
            ret = dev->sensor_func.sensor_get_otp_data(dev, &otp_read_val);
            if (ret) {
                rt_kprintf("%s (%s)sensor_get_otp_data err\n", __func__, dev->sensor_name);
                return -1;
            }
            if (sizeof(k_sensor_otp_date) != lwp_put_to_user(args, &otp_read_val, sizeof(k_sensor_otp_date))){
                rt_kprintf("%s:%d lwp_put_to_user err\n", __func__, __LINE__);
                return -1;
            }
            break;
        }
        case KD_IOC_SENSOR_S_OTP_DATA:
        {
            k_sensor_otp_date otp_write_val;

            if ((sizeof(k_sensor_otp_date)) != lwp_get_from_user(&otp_write_val, args, sizeof(k_sensor_otp_date))){
                rt_kprintf("%s:%d lwp_get_from_user err\n", __func__, __LINE__);
                return -1;
            }

            if (dev->sensor_func.sensor_set_otp_data == NULL) {
                rt_kprintf("%s (%s)sensor_set_otp_data is null\n", __func__, dev->sensor_name);
                return -1;
            }
            ret = dev->sensor_func.sensor_set_otp_data(dev, &otp_write_val);
            if (ret) {
                rt_kprintf("%s (%s)sensor_set_otp_data err\n", __func__, dev->sensor_name);
                return ret;
            }
            break;
        }

        case KD_IOC_SENSOR_S_MIRROR :
        {
            k_vicap_mirror_mode mirror;

            if ((sizeof(k_vicap_mirror_mode)) != lwp_get_from_user(&mirror, args, sizeof(k_vicap_mirror_mode))){
                rt_kprintf("%s:%d lwp_get_from_user err\n", __func__, __LINE__);
                return -1;
            }

            if (dev->sensor_func.sensor_mirror_set == NULL) {
                rt_kprintf("%s (%s)sensor_mirror_set is null\n", __func__, dev->sensor_name);
                return -1;
            }
            ret = dev->sensor_func.sensor_mirror_set(dev, mirror);
            if (ret) {
                rt_kprintf("%s (%s)sensor_mirror_set err\n", __func__, dev->sensor_name);
                return ret;
            }
            dev->mode_init_flag = K_FALSE;
            break;
        }
        case KD_IOC_SENSOR_S_FOCUS_POINT:
        {
            k_sensor_focus_pos set_focus;
            if (dev->sensor_func.sensor_set_focus_pos == NULL) {
                rt_kprintf("%s (%s)sensor_set_focus_pos is null\n", __func__, dev->sensor_name);
				return -1;
			}

            if (sizeof(set_focus) != lwp_get_from_user(&set_focus, args, sizeof(set_focus))){
                rt_kprintf("%s:%d lwp_get_from_user err\n", __func__, __LINE__);
                return -1;
            }

            ret = dev->sensor_func.sensor_set_focus_pos(dev, &set_focus);
			break;
        }
        case KD_IOC_SENSOR_G_FOCUS_POINT:
        {
            k_sensor_focus_pos get_focus;
            if (dev->sensor_func.sensor_get_focus_pos == NULL) {
                rt_kprintf("%s (%s)sensor_get_focus_pos is null\n", __func__, dev->sensor_name);
				return -1;
			}

            ret = dev->sensor_func.sensor_get_focus_pos(dev, &get_focus);
            if (ret) {
                rt_kprintf("%s (%s)sensor_get_focus_pos err\n", __func__, dev->sensor_name);
                return -1;
            }

            if (sizeof(get_focus) != lwp_put_to_user(args, &get_focus, sizeof(get_focus))){
                rt_kprintf("%s:%d lwp_put_to_user err\n", __func__, __LINE__);
                return -1;
            }

			break;
        }
        case KD_IOC_SENSOR_G_FOCUS_CAP: {
            k_sensor_autofocus_caps caps;

            if (dev->sensor_func.sensor_get_foucs_cap == NULL) {
                rt_kprintf("%s (%s)sensor_get_foucs_cap is null\n", __func__, dev->sensor_name);
				return -1;
			}

            ret = dev->sensor_func.sensor_get_foucs_cap(dev, &caps);
            if (ret) {
                rt_kprintf("%s (%s)sensor_get_foucs_cap err\n", __func__, dev->sensor_name);
                return -1;
            }

            if (sizeof(caps) != lwp_put_to_user(args, &caps, sizeof(caps))){
                rt_kprintf("%s:%d lwp_put_to_user err\n", __func__, __LINE__);
                return -1;
            }

			break;
        }
        case KD_IOC_SENSOR_S_FOCUS_POWER: {
            k_bool on_off = K_FALSE;
            unsigned long raw_arg = (unsigned long)args;

            if (dev->sensor_func.sensor_set_focus_power == NULL) {
                rt_kprintf("%s (%s)sensor_set_focus_power is null\n", __func__, dev->sensor_name);
                return -1;
            }

            if (raw_arg <= 1) {
                on_off = raw_arg ? K_TRUE : K_FALSE;
            } else if (sizeof(on_off) != lwp_get_from_user(&on_off, args, sizeof(on_off))) {
                rt_kprintf("%s:%d lwp_get_from_user err\n", __func__, __LINE__);
                return -1;
            }

            ret = dev->sensor_func.sensor_set_focus_power(dev, on_off);
            if (ret) {
                rt_kprintf("%s (%s)sensor_set_focus_power err\n", __func__, dev->sensor_name);
                return -1;
            }

            break;
        }
        case KD_IOC_SENSOR_G_EXPOSURE_TIME_RANGE:
        {
            k_sensor_exposure_time_range range;

            if (dev->sensor_func.sensor_get_exposure_time_range == NULL) {
                rt_kprintf("%s (%s)sensor_get_exposure_time_range is null\n", __func__, dev->sensor_name);
                return -1;
            }
            ret = dev->sensor_func.sensor_get_exposure_time_range(dev, &range);
            if (ret) {
                rt_kprintf("%s (%s)sensor_get_exposure_time_range err\n", __func__, dev->sensor_name);
                return -1;
            }
            if (sizeof(k_sensor_exposure_time_range) != lwp_put_to_user(args, &range, sizeof(k_sensor_exposure_time_range))){
                rt_kprintf("%s:%d lwp_put_to_user err\n", __func__, __LINE__);
                return -1;
            }

            break;
        }
        case KD_IOC_SENSOR_G_GAIN_RANGE:
        {
            k_sensor_gain_info range;

            if (dev->sensor_func.sensor_get_gain_range == NULL) {
                rt_kprintf("%s (%s)sensor_get_gain_range is null\n", __func__, dev->sensor_name);
                return -1;
            }
            ret = dev->sensor_func.sensor_get_gain_range(dev, &range);
            if (ret) {
                rt_kprintf("%s (%s)sensor_get_gain_range err\n", __func__, dev->sensor_name);
                return -1;
            }
            if (sizeof(k_sensor_gain_info) != lwp_put_to_user(args, &range, sizeof(k_sensor_gain_info))){
                rt_kprintf("%s:%d lwp_put_to_user err\n", __func__, __LINE__);
                return -1;
            }

            break;
        }
    	default:
        {
            rt_kprintf("unsupported command 0x%08x\n", cmd);
            break;
        }
    }

	return ret;
}
