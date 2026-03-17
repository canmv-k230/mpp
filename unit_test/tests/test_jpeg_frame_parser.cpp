#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <vector>

#include "JpegFrameParser.hh"

namespace {

std::vector<unsigned char> BuildMinimalValidJpeg() {
    std::vector<unsigned char> data;

    auto append = [&data](std::initializer_list<unsigned char> bytes) {
        data.insert(data.end(), bytes.begin(), bytes.end());
    };

    // SOI
    append({0xFF, 0xD8});

    // DQT length=0x0084 (2 tables of 8-bit quantization values)
    append({0xFF, 0xDB, 0x00, 0x84, 0x00});
    for (int i = 0; i < 64; ++i) data.push_back(static_cast<unsigned char>(i + 1));
    data.push_back(0x01);
    for (int i = 0; i < 64; ++i) data.push_back(static_cast<unsigned char>(64 - i));

    // SOF0 length=0x0011, precision=8, width=16, height=16, 3 components
    append({
        0xFF, 0xC0, 0x00, 0x11, 0x08,
        0x00, 0x10,  // height
        0x00, 0x10,  // width
        0x03,        // 3 components
        0x01, 0x21, 0x00,  // Y
        0x02, 0x11, 0x01,  // Cb
        0x03, 0x11, 0x01   // Cr
    });

    // SOS length=0x000C
    append({
        0xFF, 0xDA, 0x00, 0x0C, 0x03,
        0x01, 0x00, 0x02, 0x11, 0x03, 0x11,
        0x00, 0x3F, 0x00
    });

    // Scan payload + EOI
    append({0xAA, 0xBB, 0xCC, 0xDD, 0xFF, 0xD9});
    return data;
}

}  // namespace

TEST(JpegFrameParserTest, ParseValidFrameAndExposeMetadata) {
    auto frame = BuildMinimalValidJpeg();
    JpegFrameParser parser;

    ASSERT_EQ(parser.parse(frame.data(), static_cast<unsigned int>(frame.size())), 0);
    EXPECT_EQ(parser.width(), 2);   // 16 / 8
    EXPECT_EQ(parser.height(), 2);  // 16 / 8
    EXPECT_EQ(parser.type(), 0);

    unsigned short qlen = 0;
    const unsigned char *qtables = parser.quantizationTables(qlen);
    ASSERT_NE(qtables, nullptr);
    EXPECT_EQ(qlen, 128);

    unsigned int scan_len = 0;
    const unsigned char *scan = parser.scandata(scan_len);
    ASSERT_NE(scan, nullptr);
    ASSERT_GE(scan_len, 4U);
    EXPECT_EQ(scan[0], 0xAA);
    EXPECT_EQ(scan[1], 0xBB);
    EXPECT_EQ(scan[2], 0xCC);
    EXPECT_EQ(scan[3], 0xDD);
}

TEST(JpegFrameParserTest, RejectInvalidFrameWithoutMandatoryMarkers) {
    std::vector<unsigned char> invalid {0xFF, 0xD8, 0x00, 0x01, 0x02, 0x03, 0xFF, 0xD9};
    JpegFrameParser parser;
    EXPECT_EQ(parser.parse(invalid.data(), static_cast<unsigned int>(invalid.size())), -1);
}

TEST(JpegFrameParserTest, CopyAndAssignPreserveParsedState) {
    auto frame = BuildMinimalValidJpeg();
    JpegFrameParser parser;
    ASSERT_EQ(parser.parse(frame.data(), static_cast<unsigned int>(frame.size())), 0);

    JpegFrameParser copy(parser);
    JpegFrameParser assigned;
    assigned = parser;

    EXPECT_EQ(copy.width(), parser.width());
    EXPECT_EQ(copy.height(), parser.height());
    EXPECT_EQ(assigned.width(), parser.width());
    EXPECT_EQ(assigned.height(), parser.height());

    unsigned short len_a = 0;
    unsigned short len_b = 0;
    const unsigned char *qa = parser.quantizationTables(len_a);
    const unsigned char *qb = copy.quantizationTables(len_b);
    ASSERT_NE(qa, nullptr);
    ASSERT_NE(qb, nullptr);
    ASSERT_EQ(len_a, len_b);
    EXPECT_NE(qa, qb);
    EXPECT_EQ(std::memcmp(qa, qb, len_a), 0);
}
