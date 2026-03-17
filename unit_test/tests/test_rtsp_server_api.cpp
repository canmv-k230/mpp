#include <gtest/gtest.h>

#include <type_traits>

#include "rtsp_server.h"

namespace {

class MockBackChannelServer : public IOnBackChannel {
  public:
    void OnBackChannelData(std::string&, const uint8_t*, size_t, uint64_t) override {}
};

}  // namespace

TEST(RtspServerApiTest, SessionDefaultsAndEnumValues) {
    SessionAttr attr;
    EXPECT_FALSE(attr.with_video);
    EXPECT_FALSE(attr.with_audio);
    EXPECT_FALSE(attr.with_audio_backchannel);

    EXPECT_EQ(VideoType::kVideoTypeH264, static_cast<VideoType>(0));
    EXPECT_EQ(VideoType::kVideoTypeH265, static_cast<VideoType>(1));
    EXPECT_EQ(VideoType::kVideoTypeMjpeg, static_cast<VideoType>(2));
}

TEST(RtspServerApiTest, BackChannelInterfaceCanBeImplemented) {
    MockBackChannelServer cb;
    std::string name = "test";
    cb.OnBackChannelData(name, nullptr, 0, 0);
    EXPECT_EQ(name, "test");
}

TEST(RtspServerApiTest, ClassIsNonCopyable) {
    static_assert(!std::is_copy_constructible<KdRtspServer>::value, "KdRtspServer should be non-copyable");
    static_assert(!std::is_copy_assignable<KdRtspServer>::value, "KdRtspServer should be non-copy-assignable");
}
