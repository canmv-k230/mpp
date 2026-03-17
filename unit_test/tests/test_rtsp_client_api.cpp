#include <gtest/gtest.h>

#include <type_traits>

#include "rtsp_client.h"

namespace {

class MockAudioData : public IOnAudioData {
  public:
    void OnAudioStart() override {}
    void OnAudioData(const uint8_t*, size_t, uint64_t) override {}
};

class MockVideoData : public IOnVideoData {
  public:
    void OnVideoType(VideoType, uint8_t*, size_t) override {}
    void OnVideoData(const uint8_t*, size_t, uint64_t, bool) override {}
};

class MockBackChannel : public IOnBackChannel {
  public:
    void OnBackChannelStart() override {}
};

class MockClientEvent : public IRtspClientEvent {
  public:
    void OnRtspClientEvent(int) override {}
};

}  // namespace

TEST(RtspClientApiTest, InitParamDefaultsAndEnumValues) {
    RtspClientInitParam param;
    EXPECT_EQ(param.on_video_data, nullptr);
    EXPECT_EQ(param.on_audio_data, nullptr);
    EXPECT_EQ(param.on_backchannel, nullptr);
    EXPECT_EQ(param.on_event, nullptr);

    EXPECT_EQ(IOnVideoData::VideoTypeInvalid, 0);
    EXPECT_EQ(IOnVideoData::VideoTypeH264, 1);
    EXPECT_EQ(IOnVideoData::VideoTypeH265, 2);
}

TEST(RtspClientApiTest, CallbackInterfacesCanBeWired) {
    MockAudioData audio;
    MockVideoData video;
    MockBackChannel back;
    MockClientEvent event;

    RtspClientInitParam param;
    param.on_audio_data = &audio;
    param.on_video_data = &video;
    param.on_backchannel = &back;
    param.on_event = &event;

    EXPECT_EQ(param.on_audio_data, &audio);
    EXPECT_EQ(param.on_video_data, &video);
    EXPECT_EQ(param.on_backchannel, &back);
    EXPECT_EQ(param.on_event, &event);
}

TEST(RtspClientApiTest, ClassIsNonCopyable) {
    static_assert(!std::is_copy_constructible<KdRtspClient>::value, "KdRtspClient should be non-copyable");
    static_assert(!std::is_copy_assignable<KdRtspClient>::value, "KdRtspClient should be non-copy-assignable");
}
