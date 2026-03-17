#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "FramedSource.hh"
#include "LiveFrameSource.h"
#include "g711LiveFrameSource.h"
#include "h264LiveFrameSource.h"
#include "h265LiveFrameSource.h"

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

class G711Probe : public G711LiveFrameSource {
  public:
    G711Probe(UsageEnvironment& env, size_t q) : G711LiveFrameSource(env, q) {}
    ~G711Probe() override = default;

    using G711LiveFrameSource::GetEncodeType;
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    static UsageEnvironment env;
    static H264Probe h264(env, 8);
    static H265Probe h265(env, 8);
    static G711Probe g711(env, 8);

    size_t n = (size == 0) ? 1 : size;
    auto shared = make_shared_array<uint8_t>(n);
    if (size > 0) {
        std::copy(data, data + size, shared.get());
    } else {
        shared.get()[0] = 0;
    }

    timeval ref{};
    ref.tv_sec = (size > 0) ? data[0] : 0;
    ref.tv_usec = (size > 1) ? data[1] * 1000 : 0;

    auto p264 = h264.parseFrame(shared, n, ref);
    auto p265 = h265.parseFrame(shared, n, ref);
    (void)p264.size();
    (void)p265.size();

    size_t sz1 = n;
    size_t out1 = 0;
    (void)h264.extractFrame(shared.get(), sz1, out1);

    size_t sz2 = n;
    size_t out2 = 0;
    (void)h265.extractFrame(shared.get(), sz2, out2);

    (void)h264.getAuxLine();
    (void)h265.getAuxLine();
    (void)g711.GetEncodeType();

    return 0;
}
