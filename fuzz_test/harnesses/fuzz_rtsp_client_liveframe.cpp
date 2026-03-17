#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "FramedSource.hh"
#include "LiveFrameSource.h"
#include "g711LiveFrameSource.h"

class ClientProbe : public LiveFrameSource {
  public:
    ClientProbe(UsageEnvironment& env, size_t q) : LiveFrameSource(env, q) {}
    ~ClientProbe() override = default;

    using LiveFrameSource::parseFrame;
    using LiveFrameSource::queueFramePacket;
};

class ClientG711Probe : public G711LiveFrameSource {
  public:
    ClientG711Probe(UsageEnvironment& env, size_t q) : G711LiveFrameSource(env, q) {}
    ~ClientG711Probe() override = default;

    using G711LiveFrameSource::GetEncodeType;
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    static UsageEnvironment env;
    static ClientProbe probe(env, 8);
    static ClientG711Probe g711(env, 8);

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

    auto packets = probe.parseFrame(shared, n, ref);
    for (auto& p : packets) {
        probe.queueFramePacket(p);
    }

    (void)probe.GetEncodeType();
    (void)g711.GetEncodeType();
    return 0;
}
