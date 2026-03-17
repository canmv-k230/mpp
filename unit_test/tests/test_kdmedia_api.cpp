#include <gtest/gtest.h>

#include <type_traits>

#include "media.h"

namespace {

class MockAEncData : public IOnAEncData {
  public:
    void OnAEncData(k_u32, k_u8*, size_t, k_u64) override {}
};

class MockVEncData : public IOnVEncData {
  public:
    void OnVEncData(k_u32, void*, size_t, k_venc_pack_type, uint64_t) override {}
};

}  // namespace

TEST(KdmediaApiTest, InputConfigDefaultsAreExpected) {
    KdMediaInputConfig cfg;
    EXPECT_TRUE(cfg.video_valid);
    EXPECT_EQ(cfg.sensor_type, SENSOR_TYPE_MAX);
    EXPECT_EQ(cfg.sensor_num, 1);
    EXPECT_EQ(cfg.video_type, KdMediaVideoType::kVideoTypeH265);
    EXPECT_EQ(cfg.venc_width, 1280);
    EXPECT_EQ(cfg.venc_height, 720);
    EXPECT_EQ(cfg.bitrate_kbps, 4000);
    EXPECT_EQ(cfg.audio_samplerate, 8000);
    EXPECT_EQ(cfg.audio_channel_cnt, 1);
    EXPECT_EQ(cfg.pitch_shift_semitones, 0);
}

TEST(KdmediaApiTest, VdecParamsDefaultsAreExpected) {
    VdecInitParams params;
    EXPECT_EQ(params.type, KdMediaVideoType::kVideoTypeH265);
    EXPECT_EQ(params.input_buf_size, static_cast<size_t>(1920 * 1088));
    EXPECT_EQ(params.input_buf_num, 4);
    EXPECT_EQ(params.max_width, 1920);
    EXPECT_EQ(params.max_height, 1088);
    EXPECT_EQ(params.output_buf_num, 6);
}

TEST(KdmediaApiTest, PublicInterfacesAndCopySemantics) {
    static_assert(!std::is_copy_constructible<KdMedia>::value, "KdMedia should be non-copyable");
    static_assert(!std::is_copy_assignable<KdMedia>::value, "KdMedia should be non-copy-assignable");

    MockAEncData aenc;
    MockVEncData venc;
    EXPECT_NE(&aenc, nullptr);
    EXPECT_NE(&venc, nullptr);
}
