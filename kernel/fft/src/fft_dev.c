
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

#include <stdlib.h>
#include <string.h>
#include <rtthread.h>
#include "io.h"
#include "cache.h"
#include "riscv_io.h"
#include "rtdef.h"
#include <rthw.h>

#ifdef RT_USING_POSIX
    #include <dfs_posix.h>
    #include <dfs_poll.h>
    #include <posix_termios.h>
#endif

#include "k_vvo_comm.h"
#include "k_gsdma_comm.h"
#include "k_module.h"
#include "k_type.h"
#include "k_fft_ioctl.h"

#include <lwp_user_mm.h>
#include <board.h>
#include <ioremap.h>

#define IRQN_fft_INTERRUPT    (16 + 174)
typedef struct 
{
   volatile k_fft_cfg_reg_st cfg;
   volatile k_u64 rsv0;
   volatile k_u64 enable; // 0x10 write 1 enable fft module  //bit 1-bit63 reserver
   volatile k_u64 rsv2;
   volatile k_u64 int_clr;   //0x20 write 1 to clear fft intr
   volatile k_u64 rsv4;

   volatile k_u64 int_org; //0x30 原始中断寄存器
   volatile k_u64 rsv6;

   volatile k_u64 fft_inter_fifo;//0x40 fft in/out 4096 depth buffer
   volatile k_u64 rsv10;

    /*中断号寄存器，0表示正常中断，1表示计算过程中有异常写入（包括多写数据或者计算读取过程重新配置寄存器），
    2表示计算过程中有异常读取(包括多读数据或者写入及计算过程中异常读取数据)，3表示fft运行超时*/
   volatile k_u64 intr_num;//0x50
   volatile k_u64 rsv7;

   volatile k_u64 debug_0;  //0x60
   volatile k_u64 rsv8;
   volatile k_u64 debug_1; //0x70
   volatile k_u64 rsv9;
}__attribute__ ((packed)) fft_reg_st;

#define FFT_FIFI_REG_ADD  (FFT_BASE_ADDR+0X40)
#ifndef DBGLV
    #define DBGLV 7
#endif 

#define fft_log(out_loglevel, fmt, ...) \
    if ( DBGLV >= out_loglevel) \
        rt_kprintf(fmt, ##__VA_ARGS__);  

#define fft_err(s...) do { \
    fft_log(0,"<err>[%s:%d] ", __func__, __LINE__); \
    fft_log(0,s); \
    fft_log(0,"\r\n"); \
} while (0)

typedef struct 
{
    struct rt_device dev;
    volatile fft_reg_st *reg;
    volatile k_u32 *reset_reg;
    struct rt_event event;
    volatile unsigned short int_num;
}fft_dev_st;

static void dump_buff(char *buff, int len)
{
    int i=0;
    rt_kprintf("dump buff=%08lx len=%d", buff, len);
    for(i=0;i<len;i++){
        if(i%32 == 0)
            rt_kprintf("\n%08lx ",buff+i);

        rt_kprintf("%02hx ",*(buff+i));            
    }  
    rt_kprintf("\n");  
}

static void fft_reset(fft_dev_st *pfft_dev)
{
    pfft_dev->reg->enable = 0;
}

static int fft_device_open(struct dfs_fd *file)
{
    return RT_EOK;
}

static int fft_device_close(struct dfs_fd *file)
{
    return RT_EOK;
}

static int fft_input_data_from_buff(fft_dev_st* pfft_dev, k_fft_args_st* pcfg)
{
    extern k_s32 sdma_send_transfer(k_sdma_transfer_cfg_t *cfg);

    int ret = 0;
    int fft_data_len = 64 << pcfg->reg.point << 2;

    if (pcfg->reg.im == RRRR)
        fft_data_len = fft_data_len / 2;

    pfft_dev->int_num = 0;
    writeq(pcfg->reg.cfg_value, &pfft_dev->reg->cfg);
    writeq(1, &pfft_dev->reg->enable);
    rt_hw_cpu_dcache_clean(pcfg->data, sizeof(pcfg->data));

    k_sdma_transfer_cfg_t cfg = {
        .src_addr = (void*)(pcfg->data) + PV_OFFSET,
        .dst_addr = (void*)FFT_FIFI_REG_ADD,
        .dimension = DIMENSION1,
        .line_size = fft_data_len,
        .timeout_ms = RT_WAITING_FOREVER,
        .ch_cfg.dat_mode = 0,
        .ch_cfg.src_fixed = 0,
        .ch_cfg.dst_fixed = 1,
        .ch_cfg.wr_outstanding = 15,
        .ch_cfg.rd_outstanding = 15,
    };
    ret = sdma_send_transfer(&cfg);

    if (ret)
        fft_reset(pfft_dev);

    return ret;
}

static int fft_copy_data_to_user(fft_dev_st* pfft_dev, k_fft_args_st* pcfg, void* args)
{
    int ret = 0;
    int fft_data_len = 64 << pcfg->reg.point << 2;

    k_sdma_transfer_cfg_t cfg = {
        .src_addr = (void*)FFT_FIFI_REG_ADD,
        .dst_addr = (void*)(pcfg->data) + PV_OFFSET,
        .dimension = DIMENSION1,
        .line_size = fft_data_len,
        .timeout_ms = RT_WAITING_FOREVER,
        .ch_cfg.dat_mode = 0,
        .ch_cfg.src_fixed = 1,
        .ch_cfg.dst_fixed = 0,
        .ch_cfg.wr_outstanding = 15,
        .ch_cfg.rd_outstanding = 15,
    };
    ret = sdma_send_transfer(&cfg);

    if (ret == 0) {
        rt_hw_cpu_dcache_invalidate(pcfg->data, sizeof(pcfg->data));
        if (sizeof(k_fft_args_st) != lwp_put_to_user(args, pcfg, sizeof(k_fft_args_st)))
            ret = -EIO;
    }
    fft_reset(pfft_dev);

    return ret;
}

static int fft_device_ioctl(struct dfs_fd* file, int cmd, void* args)
{
    int ret = -EINVAL;
    fft_dev_st* pfft_dev = (fft_dev_st*)file->fnode->data;
    k_fft_args_st* pfft_args;

    switch (cmd) {
    case KD_IOC_CMD_FFT_IFFT:
        pfft_args = (k_fft_args_st*)rt_malloc(sizeof(k_fft_args_st));
        if (pfft_args == NULL)
            return -ENOMEM;
        if (sizeof(k_fft_args_st) == lwp_get_from_user(pfft_args, args, sizeof(k_fft_args_st))) {
            ret = fft_input_data_from_buff(pfft_dev, pfft_args); // input data
            if (0 == ret) {
                rt_event_recv(&pfft_dev->event, 0xfffff, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, NULL); // wait
                ret = fft_copy_data_to_user(pfft_dev, pfft_args, args); // output data
            }
        }
        rt_free(pfft_args);
        break;
    default:
        break;
    }

    return ret;
}

static const struct dfs_file_ops fft_input_fops = {
    .open = fft_device_open,
    .close = fft_device_close,
    .ioctl = fft_device_ioctl,
};

static void irq_callback_fft(int irq, void* data)
{
    fft_dev_st* pfft_dev = (fft_dev_st*)data;
    pfft_dev->int_num = readq(&pfft_dev->reg->intr_num);

    if (pfft_dev->int_num == 0) {
        // pfft_dev->reg->int_clr = 1;
        writeq(1, &pfft_dev->reg->int_clr);
    } else {
        rt_kprintf("fft debug: %lx %lx \n", pfft_dev->reg->debug_0, pfft_dev->reg->debug_1);
        rt_kprintf("fft cfg %lx  int %lx %lx\n", pfft_dev->reg->cfg, pfft_dev->reg->intr_num, pfft_dev->reg->int_org);

        rt_kprintf("fft debug: %lx %lx \n", readq(&pfft_dev->reg->debug_0), readq(&pfft_dev->reg->debug_1));
        rt_kprintf("fft cfg %lx  int %lx %lx\n", readq(&pfft_dev->reg->cfg), readq(&pfft_dev->reg->intr_num), readq(&pfft_dev->reg->int_org));

        rt_kprintf("int 1: err write 2: err read 3: timeout\n");

        fft_reset(pfft_dev);
        rt_kprintf("fft cfg %lx  int %lx %lx\n", readq(&pfft_dev->reg->cfg), readq(&pfft_dev->reg->intr_num), readq(&pfft_dev->reg->int_org));
    }

    rt_event_send(&pfft_dev->event, pfft_dev->int_num + 1);
}

int fft_device_init(void)
{
    int ret = 0;
    rt_device_t device;
    fft_dev_st* pfft_dev;
    static fft_dev_st fft_dev;

    pfft_dev = &fft_dev;
    device = &pfft_dev->dev;

    ret = rt_device_register(device, "fft_device", RT_DEVICE_FLAG_RDWR);
    if (ret) {
        fft_err("device register fail\n");
        return ret;
    }

    ret = rt_event_init(&pfft_dev->event, "fft_event", RT_IPC_FLAG_PRIO);
    if (ret) {
        fft_err("event init failed\n");
        return -ENOMEM;
    }

    pfft_dev->reg = rt_ioremap((void*)FFT_BASE_ADDR, sizeof(fft_reg_st));
    pfft_dev->reset_reg = rt_ioremap((void*)0x91101014, 4);

    fft_reset(pfft_dev);

    rt_hw_interrupt_install(IRQN_fft_INTERRUPT, irq_callback_fft, pfft_dev, "fft_irq");
    rt_hw_interrupt_umask(IRQN_fft_INTERRUPT);

    device->fops = &fft_input_fops;
    device->user_data = pfft_dev;

    return ret;
}
