#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <thread>

#include "RtspPusherImpl.h"
#include "rtsp_pusher.h"

namespace {

RtspPusherInitParam MakeInitParam() {
    RtspPusherInitParam param{};
    param.video_width = 1280;
    param.video_height = 720;
    std::snprintf(param.sRtspUrl, sizeof(param.sRtspUrl), "%s", "rtsp://127.0.0.1/test");
    param.on_event = nullptr;
    return param;
}

bool WaitForPushCalls(size_t expected_min, int timeout_ms = 500) {
    const auto start = std::chrono::steady_clock::now();
    while (true) {
        {
            std::lock_guard<std::mutex> lock(RTSPPusherImpl::mock_mutex_);
            if (RTSPPusherImpl::push_calls_.size() >= expected_min) {
                return true;
            }
        }
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start)
                .count() > timeout_ms) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

}  // namespace

TEST(RtspPusherRuntimeTest, PushBeforeOpenFails) {
    RTSPPusherImpl::ResetMockState();

    KdRtspPusher pusher;
    auto param = MakeInitParam();
    ASSERT_EQ(pusher.Init(param), 0);

    const uint8_t payload[] = {0x01, 0x02, 0x03};
    EXPECT_EQ(pusher.PushVideoData(payload, sizeof(payload), false, 10), -1);

    pusher.DeInit();
}

TEST(RtspPusherRuntimeTest, OversizedHeaderRejected) {
    RTSPPusherImpl::ResetMockState();

    KdRtspPusher pusher;
    auto param = MakeInitParam();
    ASSERT_EQ(pusher.Init(param), 0);

    std::vector<uint8_t> header(1025, 0x11);
    EXPECT_EQ(pusher.PushVideoHeader(header.data(), header.size()), -1);

    pusher.DeInit();
}

TEST(RtspPusherRuntimeTest, OpenPushAndCloseDeliversFrame) {
    RTSPPusherImpl::ResetMockState();

    KdRtspPusher pusher;
    auto param = MakeInitParam();
    ASSERT_EQ(pusher.Init(param), 0);

    const uint8_t header[] = {0xaa, 0xbb};
    const uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};

    ASSERT_EQ(pusher.PushVideoHeader(header, sizeof(header)), 0);
    ASSERT_EQ(pusher.Open(), 0);
    ASSERT_EQ(pusher.PushVideoData(payload, sizeof(payload), true, 12345), 0);

    ASSERT_TRUE(WaitForPushCalls(1));

    {
        std::lock_guard<std::mutex> lock(RTSPPusherImpl::mock_mutex_);
        ASSERT_FALSE(RTSPPusherImpl::push_calls_.empty());
        const auto& call = RTSPPusherImpl::push_calls_.front();
        ASSERT_EQ(call.data.size(), sizeof(header) + sizeof(payload));
        EXPECT_EQ(static_cast<uint8_t>(call.data[0]), header[0]);
        EXPECT_EQ(static_cast<uint8_t>(call.data[1]), header[1]);
        EXPECT_EQ(static_cast<uint8_t>(call.data[2]), payload[0]);
        EXPECT_TRUE(call.key_frame);
        EXPECT_EQ(call.timestamp, 12345ULL);
    }

    pusher.Close();
    pusher.DeInit();

    {
        std::lock_guard<std::mutex> lock(RTSPPusherImpl::mock_mutex_);
        EXPECT_EQ(RTSPPusherImpl::init_calls_, 1);
        EXPECT_EQ(RTSPPusherImpl::open_calls_, 1);
        EXPECT_EQ(RTSPPusherImpl::close_calls_, 1);
        EXPECT_EQ(RTSPPusherImpl::deinit_calls_, 1);
    }
}
