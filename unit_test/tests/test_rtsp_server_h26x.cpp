#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "FramedSource.hh"
#include "LiveFrameSource.h"
#include "g711LiveFrameSource.h"
#include "h264LiveFrameSource.h"
#include "h265LiveFrameSource.h"

namespace {

class H264Probe : public H264LiveFrameSource {
  public:
    H264Probe(UsageEnvironment& env, size_t q) : H264LiveFrameSource(env, q) {}
    using H264LiveFrameSource::extractFrame;
    using H264LiveFrameSource::parseFrame;
};

class H265Probe : public H265LiveFrameSource {
  public:
    H265Probe(UsageEnvironment& env, size_t q) : H265LiveFrameSource(env, q) {}
    using H265LiveFrameSource::extractFrame;
    using H265LiveFrameSource::parseFrame;
};

}  // namespace

TEST(RtspServerFrameSourceTest, G711FactoryAndEncodeType) {
    UsageEnvironment env;
    LiveFrameSource* source = G711LiveFrameSource::createNew(env, 4);
    ASSERT_NE(source, nullptr);
    EXPECT_EQ(source->GetEncodeType(), EncodeType::G711U);
}

TEST(RtspServerH264Test, ParseFrameExtractsSpsPpsAndRepeatsForIdr) {
    UsageEnvironment env;
    H264Probe probe(env, 8);

    const std::vector<uint8_t> data = {
        0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00, 0x1e,
        0x00, 0x00, 0x00, 0x01, 0x68, 0xce, 0x06, 0xe2,
        0x00, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84
    };

    auto shared = make_shared_array<uint8_t>(data.size());
    std::copy(data.begin(), data.end(), shared.get());
    const timeval ref{1, 2};

    auto packets = probe.parseFrame(shared, data.size(), ref);
    EXPECT_EQ(packets.size(), 5U);
    EXPECT_FALSE(probe.getAuxLine().empty());
}

TEST(RtspServerH264Test, ExtractFrameHandlesMissingMarker) {
    UsageEnvironment env;
    H264Probe probe(env, 8);

    std::vector<uint8_t> data = {0x11, 0x22, 0x33};
    size_t size = data.size();
    size_t outsize = 0;
    uint8_t* out = probe.extractFrame(data.data(), size, outsize);

    EXPECT_EQ(out, nullptr);
    EXPECT_EQ(outsize, 0U);
}

TEST(RtspServerH265Test, ParseFrameExtractsVpsSpsPpsAndRepeatsForIdr) {
    UsageEnvironment env;
    H265Probe probe(env, 8);

    const std::vector<uint8_t> data = {
        0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0xaa, 0xbb,
        0x00, 0x00, 0x00, 0x01, 0x42, 0x01, 0xcc, 0xdd,
        0x00, 0x00, 0x00, 0x01, 0x44, 0x01, 0xee, 0xff,
        0x00, 0x00, 0x00, 0x01, 0x26, 0x01, 0x12, 0x34
    };

    auto shared = make_shared_array<uint8_t>(data.size());
    std::copy(data.begin(), data.end(), shared.get());
    const timeval ref{3, 4};

    auto packets = probe.parseFrame(shared, data.size(), ref);
    EXPECT_EQ(packets.size(), 7U);
    EXPECT_FALSE(probe.getAuxLine().empty());
}
