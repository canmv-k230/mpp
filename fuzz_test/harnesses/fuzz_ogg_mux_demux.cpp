#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

extern "C" {
#include "libogg.h"
}

struct PageCollector {
    std::vector<std::vector<uint8_t>> pages;
};

struct FrameCollector {
    std::vector<std::vector<uint8_t>> frames;
};

static int CollectPage(const void* ptr, size_t size, void* user_data) {
    if (!ptr || size == 0 || !user_data) {
        return -1;
    }
    auto* c = static_cast<PageCollector*>(user_data);
    const auto* p = static_cast<const uint8_t*>(ptr);
    c->pages.emplace_back(p, p + size);
    return 0;
}

static void CollectFrame(const uint8_t* data, size_t len, void* user_data) {
    if (!data || len == 0 || !user_data) {
        return;
    }
    auto* c = static_cast<FrameCollector*>(user_data);
    c->frames.emplace_back(data, data + len);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (!data || size < 4) {
        return 0;
    }

    PageCollector pages;
    kd_ogg_muxer mux = nullptr;
    kd_ogg_muxer_params params{};
    params.sample_rate = 8000 + static_cast<uint32_t>(data[0]) * 4;
    params.channels = static_cast<uint32_t>((data[1] % 2) + 1);
    params.serial_no = static_cast<uint32_t>(data[2]) + 1;
    params.write_cb = CollectPage;
    params.user_data = &pages;

    if (kd_ogg_muxer_init(&mux, &params) != 0 || mux == nullptr) {
        return 0;
    }

    size_t off = 3;
    int frame_cnt = 0;
    while (off < size && frame_cnt < 32) {
        size_t len = static_cast<size_t>(data[off] % 96);
        off++;
        if (len == 0 || off >= size) {
            continue;
        }
        if (off + len > size) {
            len = size - off;
        }
        if (len == 0) {
            break;
        }

        kd_ogg_frame_params frame{};
        frame.data = data + off;
        frame.len = static_cast<uint32_t>(len);
        frame.frame_samples = static_cast<uint32_t>((data[off] % 240) + 1);
        (void)kd_ogg_write_frame(mux, &frame);

        off += len;
        frame_cnt++;
    }

    (void)kd_ogg_muxer_destroy(mux);

    FrameCollector frames;
    kd_ogg_demuxer demux = nullptr;
    kd_ogg_demuxer_params dparams{};
    dparams.frame_cb = CollectFrame;
    dparams.user_data = &frames;

    if (kd_ogg_demuxer_init(&demux, &dparams) == 0 && demux != nullptr) {
        for (const auto& p : pages.pages) {
            (void)kd_ogg_demuxer_feed_page(demux, p.data(), p.size());
        }
        (void)kd_ogg_demuxer_destroy(demux);
    }

    return 0;
}
