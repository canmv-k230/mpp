#!/usr/bin/env python3
"""Check the production VO timeout conversion at RT-Thread timer boundaries."""

from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]
source = (ROOT / "kernel/vo/src/drv_vo.c").read_text()
start = source.index("static rt_err_t vo_release_wait_timeout(")
end = source.index("static rt_int32_t vo_release_wait_remaining(", start)
helper = source[start:end]

prefix = r"""
#include <assert.h>
#include <errno.h>
#include <stdint.h>

typedef int32_t k_s32;
typedef int32_t rt_int32_t;
typedef int rt_err_t;
typedef uint64_t rt_uint64_t;
#define RT_EOK 0
#define RT_WAITING_FOREVER (-1)
#define RT_TICK_MAX UINT32_MAX
"""

checks = r"""
static void accepts(int32_t ms, int32_t expected) {
    rt_int32_t timeout = 123;
    assert(vo_release_wait_timeout(ms, &timeout) == RT_EOK);
    assert(timeout == expected);
    assert(timeout == RT_WAITING_FOREVER ||
           (timeout >= 0 && (uint32_t)timeout < RT_TICK_MAX / 2));
}

static void rejects(int32_t ms) {
    rt_int32_t timeout = 123;
    assert(vo_release_wait_timeout(ms, &timeout) == -EINVAL);
    assert(timeout == 123);
}

int main(void) {
    accepts(-1, RT_WAITING_FOREVER);
    accepts(0, 0);
    rejects(-2);
    rejects(INT32_MIN);
    accepts(1000, RT_TICK_PER_SECOND);

#if RT_TICK_PER_SECOND == 100
    accepts(1, 1);
    accepts(10, 1);
    accepts(11, 2);
    accepts(INT32_MAX, 214748365);
#elif RT_TICK_PER_SECOND == 1000
    accepts(1, 1);
    accepts(INT32_MAX - 1, INT32_MAX - 1);
    rejects(INT32_MAX);
#elif RT_TICK_PER_SECOND == 1024
    accepts(1, 2);
    accepts(2097151998, 2147483646);
    rejects(2097151999);
    rejects(INT32_MAX);
#elif RT_TICK_PER_SECOND == 2000
    accepts(1, 2);
    accepts(1073741823, 2147483646);
    rejects(1073741824);
    rejects(INT32_MAX);
#elif RT_TICK_PER_SECOND == 1000000
    accepts(1, 1000);
    accepts(2147483, 2147483000);
    rejects(2147484);
    rejects(INT32_MAX);
#else
#error Missing timeout boundary cases for this tick frequency
#endif
    return 0;
}
"""

with tempfile.TemporaryDirectory(prefix="mpp-vo-timeout-") as directory:
    test = Path(directory) / "test.c"
    binary = Path(directory) / "test"
    test.write_text(prefix + helper + checks)
    for frequency in (100, 1000, 1024, 2000, 1000000):
        subprocess.run([
            "cc", "-std=c99", "-Wall", "-Wextra", "-Werror",
            "-fsanitize=undefined", "-fno-sanitize-recover=all",
            "-DRT_TICK_PER_SECOND=%d" % frequency,
            str(test), "-o", str(binary),
        ], check=True)
        subprocess.run([str(binary)], check=True)
        print("PASS VO timeout conversion at %d Hz" % frequency)
