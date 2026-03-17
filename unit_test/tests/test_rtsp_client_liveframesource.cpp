#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "FramedSource.hh"
#include "LiveFrameSource.h"
#include "g711LiveFrameSource.h"

namespace {

class ClientLiveFrameSourceProbe : public LiveFrameSource {
  public:
    ClientLiveFrameSourceProbe(UsageEnvironment& env, size_t q) : LiveFrameSource(env, q) {}

    using LiveFrameSource::parseFrame;
    using LiveFrameSource::queueFramePacket;

    size_t PacketQueueSize() const { return fFramePacketQueue.size(); }
};

}  // namespace

TEST(RtspClientLiveFrameSourceTest, ParseFrameProducesSinglePacket) {
    UsageEnvironment env;
    ClientLiveFrameSourceProbe probe(env, 4);

    std::vector<uint8_t> bytes = {0x01, 0x02, 0x03, 0x04};
    auto shared = make_shared_array<uint8_t>(bytes.size());
    std::copy(bytes.begin(), bytes.end(), shared.get());
    const timeval ref{9, 10};

    auto packets = probe.parseFrame(shared, bytes.size(), ref);
    ASSERT_EQ(packets.size(), 1U);
    const auto& pkt = packets.front();
    EXPECT_EQ(pkt.offset_, 0U);
    EXPECT_EQ(pkt.size_, bytes.size());
    EXPECT_EQ(pkt.timestamp_.tv_sec, ref.tv_sec);
    EXPECT_EQ(pkt.timestamp_.tv_usec, ref.tv_usec);
}

TEST(RtspClientLiveFrameSourceTest, QueuePacketDropsOldestWhenFull) {
    UsageEnvironment env;
    ClientLiveFrameSourceProbe probe(env, 2);

    auto buf1 = make_shared_array<uint8_t>(1);
    auto buf2 = make_shared_array<uint8_t>(1);
    auto buf3 = make_shared_array<uint8_t>(1);
    const timeval ts{0, 0};

    LiveFrameSource::FramePacket p1(buf1, 0, 1, ts);
    LiveFrameSource::FramePacket p2(buf2, 0, 1, ts);
    LiveFrameSource::FramePacket p3(buf3, 0, 1, ts);

    probe.queueFramePacket(p1);
    probe.queueFramePacket(p2);
    probe.queueFramePacket(p3);

    EXPECT_EQ(probe.PacketQueueSize(), 2U);
}

TEST(RtspClientLiveFrameSourceTest, G711FactoryAndEncodeType) {
    UsageEnvironment env;
    LiveFrameSource* source = G711LiveFrameSource::createNew(env, 4);
    ASSERT_NE(source, nullptr);
    EXPECT_EQ(source->GetEncodeType(), EncodeType::G711U);
}
