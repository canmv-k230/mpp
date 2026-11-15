#!/usr/bin/env python3
"""The VIDEO1 vertical blank rule (kernel/connector).

A full-height VIDEO1 layer needs PANEL_VIDEO1_MIN_TAIL_LINES lines of
vertical front porch plus sync to take a bank switch (measured on k230-2, see
connector_panel.h).

1. panel_timing_video1_vporch(), taken verbatim from
   connector_panel_common.c (the rest of that file needs the GPIO and DSI
   drivers) and built with AddressSanitizer and UndefinedBehaviorSanitizer:
   CEA 1080p60, 720p30 and 4K30 timings, ones already meeting it, one
   without enough back porch, an empty timing.
2. Every fixed panel descriptor in kernel/connector/src/panels/ meets the
   rule, except the CEA-861 `_vesa` modes, which keep the standard timing.
3. Fixed custom 1080p60 and AUTO's fallback keep the sink-compatible 1080p60
   line/frame rates as well as the VIDEO1 vertical blank margin.
"""

from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
COMMON = ROOT / "kernel/connector/src/connector_panel_common.c"
HEADER = ROOT / "kernel/connector/include/connector_panel.h"
PANELS = ROOT / "kernel/connector/src/panels"

TEST_C = r"""
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

typedef uint32_t k_u32;
typedef int k_bool;
#define K_TRUE 1
#define K_FALSE 0
typedef struct {
    k_u32 pclk_khz, hactive, hsync_len, hback_porch, hfront_porch;
    k_u32 vactive, vsync_len, vback_porch, vfront_porch;
} k_vo_timing;

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "CHECK %s at line %d\n", #x, __LINE__); return 1; } } while (0)

@DEFINES@
@FUNCTIONS@

static k_vo_timing mk(k_u32 pclk, k_u32 ha, k_u32 hs, k_u32 hbp, k_u32 hfp, k_u32 va, k_u32 vs, k_u32 vbp, k_u32 vfp)
{
    k_vo_timing t = { pclk, ha, hs, hbp, hfp, va, vs, vbp, vfp };
    return t;
}

int main(void)
{
    k_vo_timing t;

    /* CEA 1080p60 4/5/36: 28+5, measured clean. */
    t = mk(148500, 1920, 44, 148, 88, 1080, 5, 36, 4);
    CHECK(panel_timing_video1_vporch(&t));
    CHECK(t.vfront_porch == 28 && t.vsync_len == 5 && t.vback_porch == 12);

    /* 24+5 (measured broken) is raised; 24+9 (measured clean) is kept. */
    t = mk(148500, 1920, 44, 148, 88, 1080, 5, 16, 24);
    CHECK(panel_timing_video1_vporch(&t) && t.vfront_porch == 28 && t.vback_porch == 12);
    t = mk(148500, 1920, 44, 148, 88, 1080, 9, 12, 24);
    CHECK(panel_timing_video1_vporch(&t) && t.vfront_porch == 24 && t.vback_porch == 12);

    /* CEA 4K30 8/10/72: 23+10 from its back porch. */
    t = mk(297000, 3840, 88, 296, 176, 2160, 10, 72, 8);
    CHECK(panel_timing_video1_vporch(&t));
    CHECK(t.vfront_porch == 23 && t.vsync_len == 10 && t.vback_porch == 57);

    /* CEA 720p30: a 30-line blank cannot reach it; best effort, refused. */
    t = mk(74250, 1280, 40, 220, 1760, 720, 5, 20, 5);
    CHECK(!panel_timing_video1_vporch(&t));
    CHECK(t.vfront_porch == 17 && t.vback_porch == 8);

    /* K230 custom 720p50 24+15 (measured clean): unchanged. */
    t = mk(54000, 1280, 25, 50, 25, 720, 15, 23, 24);
    CHECK(panel_timing_video1_vporch(&t) && t.vfront_porch == 24 && t.vback_porch == 23);

    /* Not enough back porch: it keeps PANEL_MIN_VBACK_PORCH. */
    t = mk(148500, 1920, 44, 148, 88, 1080, 5, 11, 4);
    CHECK(!panel_timing_video1_vporch(&t));
    CHECK(t.vfront_porch == 7 && t.vback_porch == 8);

    /* An empty timing and NULL. */
    t = mk(0, 0, 0, 0, 0, 0, 0, 0, 0);
    CHECK(!panel_timing_video1_vporch(&t) && t.vfront_porch == 0 && t.vback_porch == 0);
    CHECK(!panel_timing_video1_vporch(NULL));

    puts("PASS video1 vporch helper: 1080p60, 24+9 kept, 4K30, 720p30 refused, already long, short back porch, empty");
    return 0;
}
"""


def function_source(src, name):
    m = re.search(r"^[\w ]+\b" + name + r"\([^)]*\)\n\{.*?^\}\n", src, re.S | re.M)
    if not m:
        sys.exit(f"{name} not found in {COMMON}")
    return m.group(0)


def define(hdr, name):
    m = re.search(r"^#define\s+" + name + r"\s+(\d+)", hdr, re.M)
    if not m:
        sys.exit(f"{name} not found in {HEADER}")
    return int(m.group(1))


def check_descriptors(min_lines):
    """Every fixed descriptor but the `_vesa` ones has the front porch."""
    bad, seen = [], 0
    fields = ("pclk_khz", "hactive", "hsync_len", "hback_porch", "hfront_porch", "vfront_porch", "vsync_len")
    for f in sorted(PANELS.glob("*.c")):
        src = f.read_text(errors="replace")
        for m in re.finditer(r"struct\s+panel_desc\s+\w+\s*=\s*\{(.*?)\n\};", src, re.S):
            body = m.group(1)
            name = re.search(r'\.name\s*=\s*"([^"]*)"', body)
            name = name.group(1) if name else "?"
            vals = {}
            for k in fields:
                v = re.search(r"\." + k + r"\s*=\s*([^,\n]+)", body)
                vals[k] = v.group(1).strip() if v else None
            if None in vals.values():
                continue
            try:
                # Some LCD pixel clocks are written as products of constants.
                v = {k: int(eval(x, {"__builtins__": {}})) for k, x in vals.items()}
            except Exception:
                bad.append(f"{f.name}: {name}: cannot evaluate {vals}")
                continue
            ht = v["hactive"] + v["hsync_len"] + v["hback_porch"] + v["hfront_porch"]
            if v["pclk_khz"] == 0 or ht == 0:
                continue        # run-time timings (virtual display)
            seen += 1
            if name.endswith("_vesa"):
                continue
            tail = v["vfront_porch"] + v["vsync_len"]
            if tail < min_lines:
                bad.append(f"{f.name}: {name}: front porch + sync {tail} lines < {min_lines}")
    if bad:
        sys.exit("FAIL fixed descriptors:\n  " + "\n  ".join(bad))
    if seen < 20:
        sys.exit(f"FAIL only {seen} descriptors parsed")
    print(f"PASS fixed descriptors: {seen} parsed, every non-CEA one has a VIDEO1-safe vertical blank")


def check_hdmi_1080p60(min_lines):
    src = (PANELS / "hdmi_lt9611.c").read_text()
    fields = ("pclk_khz", "hactive", "hsync_len", "hback_porch", "hfront_porch",
              "vactive", "vsync_len", "vback_porch", "vfront_porch")
    for symbol in ("lt9611_panel_desc_res_1080p60", "lt9611_panel_desc_auto"):
        match = re.search(r"struct panel_desc\s+" + symbol + r"\s*=\s*\{(.*?)\n\};", src, re.S)
        if not match:
            sys.exit(f"FAIL missing {symbol}")
        t = {field: int(re.search(r"\." + field + r"\s*=\s*(\d+)", match.group(1)).group(1))
             for field in fields}
        ht = t["hactive"] + t["hsync_len"] + t["hback_porch"] + t["hfront_porch"]
        vt = t["vactive"] + t["vsync_len"] + t["vback_porch"] + t["vfront_porch"]
        if (t["hactive"], t["vactive"], t["pclk_khz"], ht, vt) != (1920, 1080, 148500, 2200, 1125):
            sys.exit(f"FAIL {symbol}: {ht}x{vt} total at {t['pclk_khz']} kHz is not the sink-compatible 1080p60 frame")
        if (t["hsync_len"], t["hback_porch"], t["hfront_porch"], t["vsync_len"]) != (44, 148, 88, 5):
            sys.exit(f"FAIL {symbol}: sync or horizontal porches differ from 1080p60")
        if t["vfront_porch"] + t["vsync_len"] < min_lines or t["vback_porch"] < 8:
            sys.exit(f"FAIL {symbol}: insufficient VIDEO1 or vertical back porch margin")
    print("PASS custom 1080p60 and AUTO fallback: 67.5 kHz lines, exact 60 Hz, VIDEO1 margin")


def main():
    src = COMMON.read_text()
    hdr = HEADER.read_text()
    min_lines = define(hdr, "PANEL_VIDEO1_MIN_TAIL_LINES")
    defines = (f"#define PANEL_VIDEO1_MIN_TAIL_LINES {min_lines}\n"
               f"#define PANEL_MIN_VBACK_PORCH {define(hdr, 'PANEL_MIN_VBACK_PORCH')}\n")
    funcs = function_source(src, "panel_timing_video1_vporch")
    with tempfile.TemporaryDirectory(prefix="mpp-video1-vporch-") as tmp:
        tmp = Path(tmp)
        (tmp / "test.c").write_text(TEST_C.replace("@DEFINES@", defines).replace("@FUNCTIONS@", funcs))
        binary = tmp / "vporch_test"
        subprocess.run(["cc", "-std=gnu11", "-g", "-Wall", "-Wextra", "-fsanitize=address,undefined",
                        "-fno-sanitize-recover=all", str(tmp / "test.c"), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
    check_descriptors(min_lines)
    check_hdmi_1080p60(min_lines)


if __name__ == "__main__":
    sys.dont_write_bytecode = True
    main()
