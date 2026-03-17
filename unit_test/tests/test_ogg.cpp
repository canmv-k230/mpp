#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include "ogg.h"
#include "libogg.h"
}

namespace {

struct PageCollector {
    std::vector<std::vector<uint8_t>> pages;
};

int CollectPage(const void *ptr, size_t size, void *user_data) {
    auto *collector = static_cast<PageCollector *>(user_data);
    if (!collector || !ptr || size == 0) {
        return -1;
    }
    const auto *begin = static_cast<const uint8_t *>(ptr);
    collector->pages.emplace_back(begin, begin + size);
    return 0;
}

struct FrameCollector {
    int call_count{0};
    std::vector<uint8_t> last_frame;
};

void CollectFrame(const uint8_t *data, size_t len, void *user_data) {
    auto *collector = static_cast<FrameCollector *>(user_data);
    if (!collector) {
        return;
    }
    collector->call_count++;
    collector->last_frame.assign(data, data + len);
}

}  // namespace

TEST(OggBitwiseTest, WriteReadRoundTrip) {
    oggpack_buffer writer {};
    oggpack_writeinit(&writer);

    oggpack_write(&writer, 0x5, 3);
    oggpack_write(&writer, 0x2AA, 10);
    oggpack_write(&writer, 0x1, 1);

    ASSERT_EQ(oggpack_bits(&writer), 14);
    ASSERT_GT(oggpack_bytes(&writer), 0);

    unsigned char *buffer = oggpack_get_buffer(&writer);
    ASSERT_NE(buffer, nullptr);

    oggpack_buffer reader {};
    oggpack_readinit(&reader, buffer, static_cast<int>(oggpack_bytes(&writer)));

    EXPECT_EQ(oggpack_read(&reader, 3), 0x5);
    EXPECT_EQ(oggpack_read(&reader, 10), 0x2AA);
    EXPECT_EQ(oggpack_read1(&reader), 1);

    oggpack_writeclear(&writer);
}

TEST(OggFramingTest, StreamPacketToPageContainsExpectedMetadata) {
    ogg_stream_state stream {};
    ASSERT_EQ(ogg_stream_init(&stream, 1234), 0);

    std::array<uint8_t, 6> payload {0x10, 0x20, 0x30, 0x40, 0x50, 0x60};
    ogg_packet packet {};
    packet.packet = payload.data();
    packet.bytes = static_cast<long>(payload.size());
    packet.b_o_s = 1;
    packet.e_o_s = 0;
    packet.granulepos = 0;
    packet.packetno = 0;

    ASSERT_EQ(ogg_stream_packetin(&stream, &packet), 0);

    ogg_page page {};
    ASSERT_EQ(ogg_stream_flush(&stream, &page), 1);
    ASSERT_NE(page.header, nullptr);
    ASSERT_NE(page.body, nullptr);
    ASSERT_GT(page.header_len, 0);
    ASSERT_GT(page.body_len, 0);
    EXPECT_EQ(ogg_page_serialno(&page), 1234);
    EXPECT_EQ(ogg_page_bos(&page), 2);
    EXPECT_GE(ogg_page_packets(&page), 1);

    EXPECT_EQ(ogg_stream_clear(&stream), 0);
}

TEST(OggLiboggTest, MuxDemuxRoundTripInStreamMode) {
    PageCollector page_collector;
    kd_ogg_muxer muxer = nullptr;
    kd_ogg_muxer_params mux_params {};
    mux_params.sample_rate = 16000;
    mux_params.channels = 1;
    mux_params.serial_no = 100;
    mux_params.write_cb = CollectPage;
    mux_params.user_data = &page_collector;

    ASSERT_EQ(kd_ogg_muxer_init(&muxer, &mux_params), 0);
    ASSERT_NE(muxer, nullptr);

    const std::vector<uint8_t> input_frame {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    kd_ogg_frame_params frame {};
    frame.data = input_frame.data();
    frame.len = static_cast<uint32_t>(input_frame.size());
    frame.frame_samples = 320;
    ASSERT_EQ(kd_ogg_write_frame(muxer, &frame), 0);
    ASSERT_EQ(kd_ogg_muxer_destroy(muxer), 0);

    ASSERT_FALSE(page_collector.pages.empty());

    FrameCollector frame_collector;
    kd_ogg_demuxer demuxer = nullptr;
    kd_ogg_demuxer_params demux_params {};
    demux_params.frame_cb = CollectFrame;
    demux_params.user_data = &frame_collector;

    ASSERT_EQ(kd_ogg_demuxer_init(&demuxer, &demux_params), 0);
    ASSERT_NE(demuxer, nullptr);

    for (const auto &page : page_collector.pages) {
        ASSERT_EQ(kd_ogg_demuxer_feed_page(demuxer, page.data(), page.size()), 0);
    }

    EXPECT_GE(frame_collector.call_count, 1);
    EXPECT_EQ(frame_collector.last_frame, input_frame);

    EXPECT_EQ(kd_ogg_demuxer_destroy(demuxer), 0);
}

TEST(OggLiboggTest, RejectInvalidMuxerParameters) {
    kd_ogg_muxer muxer = nullptr;
    kd_ogg_muxer_params params {};
    params.sample_rate = 0;
    params.channels = 1;

    EXPECT_EQ(kd_ogg_muxer_init(&muxer, &params), -1);
    EXPECT_EQ(kd_ogg_muxer_init(nullptr, &params), -1);
}
