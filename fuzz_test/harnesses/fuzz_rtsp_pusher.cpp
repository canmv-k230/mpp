#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "rtsp_pusher.h"

struct PusherContext {
    KdRtspPusher pusher;
    bool ready{false};

    PusherContext() {
        RtspPusherInitParam param{};
        param.video_width = 1280;
        param.video_height = 720;
        std::snprintf(param.sRtspUrl, sizeof(param.sRtspUrl), "%s", "rtsp://127.0.0.1/fuzz");
        if (pusher.Init(param) == 0) {
            if (pusher.Open() == 0) {
                ready = true;
            }
        }
    }

    ~PusherContext() {
        if (ready) {
            pusher.Close();
        }
        pusher.DeInit();
    }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    static PusherContext ctx;
    if (!ctx.ready || size == 0) {
        return 0;
    }

    size_t off = 0;
    size_t header_len = static_cast<size_t>(data[off] % 1100);
    off++;

    if (off + header_len > size) {
        header_len = size - off;
    }

    if (header_len > 0) {
        (void)ctx.pusher.PushVideoHeader(data + off, header_len);
        off += header_len;
    }

    int frames = 0;
    while (off < size && frames < 16) {
        size_t len = static_cast<size_t>(data[off] % 2048);
        off++;
        if (len == 0 || off >= size) {
            continue;
        }
        if (off + len > size) {
            len = size - off;
        }

        bool key = (data[off] & 1) != 0;
        uint64_t ts = (static_cast<uint64_t>(data[off]) << 24) |
                      (static_cast<uint64_t>(len) << 8) |
                      static_cast<uint64_t>(frames);
        (void)ctx.pusher.PushVideoData(data + off, len, key, ts);
        off += len;
        frames++;
    }

    return 0;
}
