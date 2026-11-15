/* Copyright (c) 2026, Canaan Bright Sight Co., Ltd */

#include <string.h>

#include "hdmi_edid.h"

/* CEA-861 progressive timings up to 4K30 (VIC, then pclk and porches). */
static const struct {
    k_u8        vic;
    k_vo_timing t;
} cea_modes[] = {
    /*            pclk    ha  hsync hbp  hfp   va   vs vbp vfp */
    { 1,  { 25175,  640,  96,  48,   16,  480, 2, 33, 10 } },
    { 2,  { 27000,  720,  62,  60,   16,  480, 6, 30, 9 } },
    { 3,  { 27000,  720,  62,  60,   16,  480, 6, 30, 9 } },
    { 4,  { 74250,  1280, 40,  220,  110, 720, 5, 20, 5 } },
    { 16, { 148500, 1920, 44,  148,  88,  1080, 5, 36, 4 } },
    { 17, { 27000,  720,  64,  68,   12,  576, 5, 39, 5 } },
    { 18, { 27000,  720,  64,  68,   12,  576, 5, 39, 5 } },
    { 19, { 74250,  1280, 40,  220,  440, 720, 5, 20, 5 } },
    { 31, { 148500, 1920, 44,  148,  528, 1080, 5, 36, 4 } },
    { 32, { 74250,  1920, 44,  148,  638, 1080, 5, 36, 4 } },
    { 33, { 74250,  1920, 44,  148,  528, 1080, 5, 36, 4 } },
    { 34, { 74250,  1920, 44,  148,  88,  1080, 5, 36, 4 } },
    { 60, { 59400,  1280, 40,  220,  1760, 720, 5, 20, 5 } },
    { 61, { 74250,  1280, 40,  220,  2420, 720, 5, 20, 5 } },
    { 62, { 74250,  1280, 40,  220,  1760, 720, 5, 20, 5 } },
    { 63, { 297000, 1920, 44,  148,  88,  1080, 5, 36, 4 } },
    { 64, { 297000, 1920, 44,  148,  528, 1080, 5, 36, 4 } },
    { 93, { 297000, 3840, 88,  296,  1276, 2160, 10, 72, 8 } },
    { 94, { 297000, 3840, 88,  296,  1056, 2160, 10, 72, 8 } },
    { 95, { 297000, 3840, 88,  296,  176, 2160, 10, 72, 8 } },
};

static k_u32 htotal(const k_vo_timing* t) { return t->hactive + t->hsync_len + t->hback_porch + t->hfront_porch; }
static k_u32 vtotal(const k_vo_timing* t) { return t->vactive + t->vsync_len + t->vback_porch + t->vfront_porch; }

/* Refresh in mHz, and line rate in Hz. */
static k_u32 refresh_mhz(const k_vo_timing* t)
{
    k_u64 pixels = (k_u64)htotal(t) * vtotal(t);

    return pixels ? (k_u32)(((k_u64)t->pclk_khz * 1000000ULL) / pixels) : 0;
}

static k_u32 line_hz(const k_vo_timing* t)
{
    k_u32 h = htotal(t);

    return h ? (k_u32)(((k_u64)t->pclk_khz * 1000ULL) / h) : 0;
}

/* An 18-byte detailed timing descriptor. Returns K_FALSE for other
 * descriptors and interlaced timings. */
static k_bool parse_dtd(const k_u8* d, k_vo_timing* t)
{
    k_u32 hblank, vblank;

    if (d[0] == 0 && d[1] == 0)
        return K_FALSE;
    if (d[17] & 0x80)
        return K_FALSE;     /* interlaced */
    t->pclk_khz     = (k_u32)(d[0] | (d[1] << 8)) * 10;
    t->hactive      = d[2] | ((d[4] & 0xf0) << 4);
    hblank          = d[3] | ((d[4] & 0x0f) << 8);
    t->vactive      = d[5] | ((d[7] & 0xf0) << 4);
    vblank          = d[6] | ((d[7] & 0x0f) << 8);
    t->hfront_porch = d[8] | ((d[11] & 0xc0) << 2);
    t->hsync_len    = d[9] | ((d[11] & 0x30) << 4);
    t->vfront_porch = (d[10] >> 4) | ((d[11] & 0x0c) << 2);
    t->vsync_len    = (d[10] & 0x0f) | ((d[11] & 0x03) << 4);
    if (t->hfront_porch + t->hsync_len > hblank || t->vfront_porch + t->vsync_len > vblank)
        return K_FALSE;
    t->hback_porch = hblank - t->hfront_porch - t->hsync_len;
    t->vback_porch = vblank - t->vfront_porch - t->vsync_len;
    return t->hactive && t->vactive;
}

static void parse_display_desc(const k_u8* d, struct hdmi_edid_info* info)
{
    if (d[0] || d[1] || d[2])
        return;
    if (d[3] == 0xfc) {
        int i;

        for (i = 0; i < 13 && d[5 + i] != 0x0a; i++)
            info->name[i] = (char)d[5 + i];
        info->name[i] = 0;
    } else if (d[3] == 0xfd) {
        /* Range limits; offsets flags in byte 4 (EDID 1.4) add 255. */
        info->range_vmin = d[5] + ((d[4] & 0x01) ? 255 : 0);
        info->range_vmax = d[6] + ((d[4] & 0x02) ? 255 : 0);
        info->range_hmin = d[7] + ((d[4] & 0x04) ? 255 : 0);
        info->range_hmax = d[8] + ((d[4] & 0x08) ? 255 : 0);
        info->range_pclk_max_khz = (k_u32)d[9] * 10000;
    }
}

static k_bool checksum_ok(const k_u8* b)
{
    k_u8 sum = 0;

    for (int i = 0; i < 128; i++)
        sum += b[i];
    return sum == 0;
}

/* Check output and sink limits. On K_TRUE, replace @t->pclk_khz with the
 * clock the output really makes; on K_FALSE, leave @t unchanged. */
static k_bool qualify_mode(k_vo_timing* t, const struct hdmi_edid_limits* lim, const struct hdmi_edid_info* info)
{
    k_u32 clk;

    if (t->pclk_khz < lim->min_pclk_khz || t->pclk_khz > lim->max_pclk_khz ||
        t->hactive > lim->max_hactive || t->vactive > lim->max_vactive)
        return K_FALSE;
    if (info->range_vmax) {
        k_u32 r = refresh_mhz(t), l = line_hz(t);

        /* 1 Hz / 1 kHz of slack: limits are whole numbers. */
        if (r + 1000 < info->range_vmin * 1000 || r > info->range_vmax * 1000 + 1000 ||
            l + 1000 < info->range_hmin * 1000 || l > info->range_hmax * 1000 + 1000 ||
            (info->range_pclk_max_khz && t->pclk_khz > info->range_pclk_max_khz))
            return K_FALSE;
    }
    clk = lim->clock ? lim->clock(t->pclk_khz, lim->clock_arg) : t->pclk_khz;
    if (clk == 0)
        return K_FALSE;
    t->pclk_khz = clk;
    return K_TRUE;
}

/* Larger area wins, then the higher refresh. */
static k_bool better(const k_vo_timing* a, const k_vo_timing* b)
{
    k_u64 aa = (k_u64)a->hactive * a->vactive, ab = (k_u64)b->hactive * b->vactive;

    if (aa != ab)
        return aa > ab;
    return refresh_mhz(a) > refresh_mhz(b);
}

int hdmi_edid_choose(const k_u8* edid, k_u32 len, const struct hdmi_edid_limits* lim, k_vo_timing* out,
                     struct hdmi_edid_info* info_out)
{
    static const k_u8     header[8] = { 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00 };
    struct hdmi_edid_info info;
    k_vo_timing           best = { 0 };
    k_bool                have = K_FALSE;
    k_vo_timing           t;

    memset(&info, 0, sizeof(info));
    if (!edid || len < 128 || memcmp(edid, header, sizeof(header)) != 0 || !checksum_ok(edid))
        return -1;

    for (int i = 0; i < 4; i++)
        parse_display_desc(&edid[54 + 18 * i], &info);
    if (info_out)
        *info_out = info;

    /* The preferred timing (first detailed descriptor) when it qualifies. */
    if (parse_dtd(&edid[54], &t) && qualify_mode(&t, lim, &info)) {
        *out = t;
        return 0;
    }

    for (int i = 1; i < 4; i++) {
        if (parse_dtd(&edid[54 + 18 * i], &t) && qualify_mode(&t, lim, &info) && (!have || better(&t, &best))) {
            best = t;
            have = K_TRUE;
        }
    }

    /* One CEA-861 extension: its video codes and detailed timings. */
    if (len >= 256 && edid[126] >= 1 && edid[128] == 0x02 && checksum_ok(&edid[128])) {
        const k_u8* x   = &edid[128];
        k_u32       dtd = x[2];

        for (k_u32 p = 4; p < dtd && p < 127;) {
            k_u32 tag = x[p] >> 5, n = x[p] & 0x1f;

            if (tag == 2) {     /* video data block */
                for (k_u32 k = 1; k <= n && p + k < 127; k++) {
                    k_u8 vic = x[p + k] & 0x7f;

                    for (k_u32 m = 0; m < sizeof(cea_modes) / sizeof(cea_modes[0]); m++) {
                        if (cea_modes[m].vic != vic)
                            continue;
                        t = cea_modes[m].t;
                        if (qualify_mode(&t, lim, &info) && (!have || better(&t, &best))) {
                            best = t;
                            have = K_TRUE;
                        }
                    }
                }
            }
            p += n + 1;
        }
        for (k_u32 q = dtd; dtd >= 4 && q + 18 <= 127; q += 18) {
            if (parse_dtd(&x[q], &t) && qualify_mode(&t, lim, &info) && (!have || better(&t, &best))) {
                best = t;
                have = K_TRUE;
            }
        }
    }

    if (!have)
        return -1;
    *out = best;
    return 0;
}
