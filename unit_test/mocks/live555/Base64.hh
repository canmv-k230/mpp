#ifndef UNIT_TEST_MOCK_BASE64_HH
#define UNIT_TEST_MOCK_BASE64_HH

#include <cstdlib>

inline char* base64Encode(char const* data, unsigned len) {
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const unsigned out_len = (len == 0) ? 1 : ((len + 2) / 3) * 4;
    char* out = static_cast<char*>(std::malloc(out_len + 1));
    if (!out) {
        return nullptr;
    }

    unsigned j = 0;
    unsigned i = 0;
    while (i + 2 < len) {
        const unsigned n = (static_cast<unsigned>(static_cast<unsigned char>(data[i])) << 16) |
                           (static_cast<unsigned>(static_cast<unsigned char>(data[i + 1])) << 8) |
                           static_cast<unsigned>(static_cast<unsigned char>(data[i + 2]));
        out[j++] = table[(n >> 18) & 0x3F];
        out[j++] = table[(n >> 12) & 0x3F];
        out[j++] = table[(n >> 6) & 0x3F];
        out[j++] = table[n & 0x3F];
        i += 3;
    }

    if (i < len) {
        unsigned n = static_cast<unsigned>(static_cast<unsigned char>(data[i])) << 16;
        out[j++] = table[(n >> 18) & 0x3F];
        if (i + 1 < len) {
            n |= static_cast<unsigned>(static_cast<unsigned char>(data[i + 1])) << 8;
            out[j++] = table[(n >> 12) & 0x3F];
            out[j++] = table[(n >> 6) & 0x3F];
            out[j++] = '=';
        } else {
            out[j++] = table[(n >> 12) & 0x3F];
            out[j++] = '=';
            out[j++] = '=';
        }
    }

    if (j == 0) {
        out[j++] = 'A';
    }

    out[j] = '\0';
    return out;
}

#endif
