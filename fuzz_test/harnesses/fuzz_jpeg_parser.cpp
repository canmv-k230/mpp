#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "JpegFrameParser.hh"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::vector<unsigned char> buf(data, data + size);
    JpegFrameParser parser;

    int ret = parser.parse(buf.data(), static_cast<unsigned int>(buf.size()));
    if (ret == 0) {
        (void)parser.width();
        (void)parser.height();
        (void)parser.type();
        (void)parser.precision();
        (void)parser.qFactor();
        (void)parser.restartInterval();

        unsigned short qlen = 0;
        const unsigned char* q = parser.quantizationTables(qlen);
        if (q && qlen > 0) {
            volatile unsigned char x = q[0];
            (void)x;
        }

        unsigned int slen = 0;
        const unsigned char* s = parser.scandata(slen);
        if (s && slen > 0) {
            volatile unsigned char y = s[0];
            (void)y;
        }

        JpegFrameParser copy(parser);
        JpegFrameParser assign;
        assign = parser;
        (void)copy.width();
        (void)assign.height();
    }

    return 0;
}
