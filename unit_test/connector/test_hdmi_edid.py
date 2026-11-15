#!/usr/bin/env python3
"""Exercise kernel/connector/src/panels/hdmi_edid.c (EDID mode choice for the
LT9611's LT9611_HDMI_AUTO) on the host, with AddressSanitizer and
UndefinedBehaviorSanitizer.

The clock filter is the one the LT9611 uses on the board, reduced to its
rule: the VO makes 594 MHz / N, and a mode is carried if one of those is
within 0.5% of its pixel clock, between 54 and 297 MHz.

Cases: the EDID of the DELL U2414H read on k230-2; the same with a lower
pixel clock cap (falls back to the best CEA mode); a 2560x1440@60 preferred
mode (241.5 MHz, not a VO clock) with a 4K30 video code (297 MHz, chosen);
range limits that exclude the preferred mode; a sink with only modes below
54 MHz; a bad checksum.
"""

from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]

TEST_C = r"""
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "hdmi_edid.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "CHECK %s at line %d\n", #x, __LINE__); return 1; } } while (0)

/* As lt9611_auto_clock(): 594 MHz / N within 0.5%. */
static k_u32 vo_clock(k_u32 pclk_khz, void *arg)
{
    (void)arg;
    for (k_u32 n = 2; n <= 64; n++) {
        k_u32 khz = 594000 / n, diff = khz > pclk_khz ? khz - pclk_khz : pclk_khz - khz;
        if (diff * 200 <= pclk_khz)
            return khz;
    }
    return 0;
}

static const struct hdmi_edid_limits lim = { 54000, 297000, 3840, 2160, vo_clock, NULL };

/* DELL U2414H, read through the LT9611 on k230-2. */
static const k_u8 dell[256] = {
    0x00,0xff,0xff,0xff,0xff,0xff,0xff,0x00,0x10,0xac,0xa4,0xa0,0x4c,0x56,0x34,0x31,
    0x27,0x1b,0x01,0x03,0x80,0x35,0x1e,0x78,0xee,0x7e,0x75,0xa7,0x55,0x52,0x9c,0x27,
    0x0f,0x50,0x54,0xa5,0x4b,0x00,0x71,0x4f,0x81,0x80,0xa9,0xc0,0xa9,0x40,0xd1,0xc0,
    0x01,0x01,0x01,0x01,0x01,0x01,0x02,0x3a,0x80,0x18,0x71,0x38,0x2d,0x40,0x58,0x2c,
    0x45,0x00,0x0f,0x28,0x21,0x00,0x00,0x1e,0x00,0x00,0x00,0xff,0x00,0x59,0x31,0x56,
    0x48,0x59,0x37,0x39,0x52,0x31,0x34,0x56,0x4c,0x0a,0x00,0x00,0x00,0xfc,0x00,0x44,
    0x45,0x4c,0x4c,0x20,0x55,0x32,0x34,0x31,0x34,0x48,0x0a,0x20,0x00,0x00,0x00,0xfd,
    0x00,0x38,0x4c,0x1e,0x53,0x11,0x00,0x0a,0x20,0x20,0x20,0x20,0x20,0x20,0x01,0x16,
    0x02,0x03,0x1f,0xf1,0x4c,0x90,0x05,0x04,0x03,0x02,0x07,0x16,0x01,0x14,0x1f,0x12,
    0x13,0x23,0x09,0x07,0x07,0x65,0x03,0x0c,0x00,0x10,0x00,0x83,0x01,0x00,0x00,0x02,
    0x3a,0x80,0x18,0x71,0x38,0x2d,0x40,0x58,0x2c,0x45,0x00,0x0f,0x28,0x21,0x00,0x00,
    0x1e,0x01,0x1d,0x80,0x18,0x71,0x1c,0x16,0x20,0x58,0x2c,0x25,0x00,0x0f,0x28,0x21,
    0x00,0x00,0x9e,0x01,0x1d,0x00,0x72,0x51,0xd0,0x1e,0x20,0x6e,0x28,0x55,0x00,0x0f,
    0x28,0x21,0x00,0x00,0x1e,0x8c,0x0a,0xd0,0x8a,0x20,0xe0,0x2d,0x10,0x10,0x3e,0x96,
    0x00,0x0f,0x28,0x21,0x00,0x00,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x37,
};

static void fix_checksum(k_u8 *b)
{
    k_u8 sum = 0;
    for (int i = 0; i < 127; i++)
        sum += b[i];
    b[127] = (k_u8)(0x100 - sum);
}

/* A detailed timing descriptor from porches. */
static void put_dtd(k_u8 *d, k_u32 pclk_khz, k_u32 ha, k_u32 hfp, k_u32 hs, k_u32 hbp,
                    k_u32 va, k_u32 vfp, k_u32 vs, k_u32 vbp)
{
    k_u32 p = pclk_khz / 10, hb = hfp + hs + hbp, vb = vfp + vs + vbp;
    memset(d, 0, 18);
    d[0] = p & 0xff; d[1] = p >> 8;
    d[2] = ha & 0xff; d[3] = hb & 0xff; d[4] = ((ha >> 8) << 4) | (hb >> 8);
    d[5] = va & 0xff; d[6] = vb & 0xff; d[7] = ((va >> 8) << 4) | (vb >> 8);
    d[8] = hfp & 0xff; d[9] = hs & 0xff; d[10] = ((vfp & 0xf) << 4) | (vs & 0xf);
    d[11] = ((hfp >> 8) << 6) | ((hs >> 8) << 4) | ((vfp >> 4) << 2) | (vs >> 4);
    d[17] = 0x1e;
}

/* A base block with @dtd preferred and, if @vics, a CEA extension listing them. */
static void make_edid(k_u8 *e, const k_u8 *vics, int nvic)
{
    static const k_u8 hdr[8] = { 0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0 };
    memset(e, 0, 256);
    memcpy(e, hdr, 8);
    e[18] = 1; e[19] = 3;
    for (int i = 38; i < 54; i++)
        e[i] = 1;
    e[126] = nvic ? 1 : 0;
    if (nvic) {
        k_u8 *x = &e[128];
        x[0] = 2; x[1] = 3;
        x[4] = (2 << 5) | nvic;
        memcpy(&x[5], vics, nvic);
        x[2] = 5 + nvic;
        fix_checksum(x);
    }
}

int main(void)
{
    k_vo_timing t;
    struct hdmi_edid_info info;
    k_u8 e[256];

    /* 1. The Dell: its preferred 1080p60, CEA timing. */
    CHECK(hdmi_edid_choose(dell, 256, &lim, &t, &info) == 0);
    CHECK(strcmp(info.name, "DELL U2414H") == 0);
    CHECK(info.range_vmin == 56 && info.range_vmax == 76 && info.range_hmax == 83);
    CHECK(t.hactive == 1920 && t.vactive == 1080 && t.pclk_khz == 148500);
    CHECK(t.hfront_porch == 88 && t.hsync_len == 44 && t.hback_porch == 148);
    CHECK(t.vfront_porch == 4 && t.vsync_len == 5 && t.vback_porch == 36);

    /* 2. Cap at 100 MHz: no 1080p60/50; 720p60 beats 720p50 (CEA code or
     * extension DTD, same timing). */
    struct hdmi_edid_limits cap = lim;
    cap.max_pclk_khz = 100000;
    CHECK(hdmi_edid_choose(dell, 256, &cap, &t, NULL) == 0);
    CHECK(t.hactive == 1280 && t.vactive == 720 && t.pclk_khz == 74250 && t.hfront_porch == 110);

    /* 3. 2560x1440@60 (241.5 MHz) preferred: no VO clock for it. 4K30 (VIC
     * 95, 297 MHz) and 1080p60 (VIC 16) listed: the larger one. */
    {
        k_u8 v[2] = { 16, 95 };
        make_edid(e, v, 2);
        put_dtd(&e[54], 241500, 2560, 48, 32, 80, 1440, 3, 5, 33);
        fix_checksum(e);
        CHECK(hdmi_edid_choose(e, 256, &lim, &t, NULL) == 0);
        CHECK(t.hactive == 3840 && t.vactive == 2160 && t.pclk_khz == 297000 && t.hfront_porch == 176);
    }

    /* 4. Range limits at most 55 Hz: preferred 1080p60 out, 1080p50 (VIC 31). */
    {
        k_u8 v[2] = { 16, 31 };
        make_edid(e, v, 2);
        put_dtd(&e[54], 148500, 1920, 88, 44, 148, 1080, 4, 5, 36);
        k_u8 *r = &e[72];
        r[3] = 0xfd; r[5] = 24; r[6] = 55; r[7] = 15; r[8] = 90; r[9] = 30;
        fix_checksum(e);
        CHECK(hdmi_edid_choose(e, 256, &lim, &t, NULL) == 0);
        CHECK(t.hactive == 1920 && t.hfront_porch == 528 && t.pclk_khz == 148500);
    }

    /* 5. Only modes below 54 MHz (640x480p60, 720x480p60): none. */
    {
        k_u8 v[2] = { 1, 2 };
        make_edid(e, v, 2);
        put_dtd(&e[54], 25175, 640, 16, 96, 48, 480, 10, 2, 33);
        fix_checksum(e);
        CHECK(hdmi_edid_choose(e, 256, &lim, &t, NULL) != 0);
    }

    /* 6. A corrupt base block is refused. */
    memcpy(e, dell, 256);
    e[60] ^= 0x01;
    CHECK(hdmi_edid_choose(e, 256, &lim, &t, NULL) != 0);

    puts("PASS hdmi_edid: Dell EDID, clock cap, 2560x1440 not a VO clock, range limits, low clocks only, bad checksum");
    return 0;
}
"""


def main():
    src = ROOT / "kernel/connector/src/panels"
    with tempfile.TemporaryDirectory(prefix="mpp-hdmi-edid-") as tmp:
        tmp = Path(tmp)
        (tmp / "test.c").write_text(TEST_C)
        (tmp / "k_autoconf_comm.h").write_text("/* generated by the SDK build */\n")
        binary = tmp / "edid_test"
        cmd = ["cc", "-std=gnu11", "-g", "-Wall", "-Wextra", "-Wno-unused-parameter",
               "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
               f"-I{tmp}", f"-I{src}", f"-I{ROOT / 'include'}", f"-I{ROOT / 'include/comm'}",
               str(src / "hdmi_edid.c"), str(tmp / "test.c"), "-o", str(binary)]
        subprocess.run(cmd, check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    sys.dont_write_bytecode = True
    main()
