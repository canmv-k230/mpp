/* Copyright (c) 2026, Canaan Bright Sight Co., Ltd */

/*
 * EDID parsing and mode choice for HDMI outputs. No I/O: the caller reads
 * the EDID (base block plus one CEA-861 extension) and says which pixel
 * clocks its path can make.
 */

#ifndef __HDMI_EDID_H__
#define __HDMI_EDID_H__

#include "k_type.h"
#include "k_vo_comm.h"

#define HDMI_EDID_LEN 256

struct hdmi_edid_limits {
    k_u32 min_pclk_khz;
    k_u32 max_pclk_khz;
    k_u32 max_hactive;
    k_u32 max_vactive;
    /* The pixel clock the output will really use for @pclk_khz, or 0 if it
     * cannot make one close enough. */
    k_u32 (*clock)(k_u32 pclk_khz, void* arg);
    void* clock_arg;
};

struct hdmi_edid_info {
    char  name[14];         /* monitor name descriptor, "" if none */
    k_u32 range_vmin, range_vmax;   /* Hz, 0 if no range limits */
    k_u32 range_hmin, range_hmax;   /* kHz */
    k_u32 range_pclk_max_khz;
};

/*
 * Choose a mode from @edid: the sink's preferred (first detailed) timing if
 * the output can carry it and the sink's range limits allow it, else the
 * largest, then fastest, detailed or CEA-861 (VIC) timing that qualifies.
 * Progressive only. @out gets the timing with the pixel clock @lim->clock
 * returned. @info, if not NULL, gets the name and range limits.
 * Returns 0, or -1 if the EDID is invalid or no mode qualifies.
 */
int hdmi_edid_choose(const k_u8* edid, k_u32 len, const struct hdmi_edid_limits* lim, k_vo_timing* out,
                     struct hdmi_edid_info* info);

#endif
