#include <cstddef>
#include <cstdint>

#include "media.h"

class DummyAEnc final : public IOnAEncData {
  public:
    void OnAEncData(k_u32, k_u8*, size_t, k_u64) override {}
};

class DummyVEnc final : public IOnVEncData {
  public:
    void OnVEncData(k_u32, void*, size_t, k_venc_pack_type, uint64_t) override {}
};

static KdMediaVideoType ByteToVideoType(uint8_t v) {
    switch (v % 3) {
        case 0:
            return KdMediaVideoType::kVideoTypeH264;
        case 1:
            return KdMediaVideoType::kVideoTypeH265;
        default:
            return KdMediaVideoType::kVideoTypeMjpeg;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    KdMediaInputConfig cfg;
    VdecInitParams params;

    if (size > 0) cfg.video_valid = (data[0] & 1) != 0;
    if (size > 1) cfg.sensor_num = static_cast<int>(data[1] % 4);
    if (size > 2) cfg.video_type = ByteToVideoType(data[2]);
    if (size > 4) cfg.venc_width = 16 + static_cast<int>(data[3]) * 8;
    if (size > 5) cfg.venc_height = 16 + static_cast<int>(data[4]) * 8;
    if (size > 6) cfg.bitrate_kbps = 128 + static_cast<int>(data[5]) * 32;
    if (size > 7) cfg.audio_samplerate = 8000 + static_cast<int>(data[6]) * 32;
    if (size > 8) cfg.audio_channel_cnt = static_cast<int>((data[7] % 2) + 1);
    if (size > 9) cfg.pitch_shift_semitones = static_cast<int>(data[8] % 25) - 12;

    if (size > 10) params.type = ByteToVideoType(data[9]);
    if (size > 12) params.input_buf_size = static_cast<size_t>(256 + (data[10] * 64));
    if (size > 13) params.input_buf_num = static_cast<int>((data[11] % 8) + 1);
    if (size > 14) params.max_width = 16 + static_cast<int>(data[12]) * 8;
    if (size > 15) params.max_height = 16 + static_cast<int>(data[13]) * 8;
    if (size > 16) params.output_buf_num = static_cast<int>((data[14] % 8) + 1);

    DummyAEnc aenc;
    DummyVEnc venc;
    (void)aenc;
    (void)venc;

    volatile int sink = cfg.venc_width + cfg.venc_height + cfg.bitrate_kbps +
                        cfg.audio_samplerate + params.max_width + params.max_height +
                        params.input_buf_num + params.output_buf_num;
    (void)sink;
    return 0;
}
