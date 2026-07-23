/*
 * mp4_format - MP4/fMP4/TS muxer & demuxer built on libavformat
 *
 * Provides a thin C API (kd_mp4_*) over FFmpeg's libavformat.
 *
 * Muxer:
 *   - MP4, fragmented MP4 (fMP4), or MPEG-TS containers (auto-detected by ext)
 *   - H.264/H.265 video + G.711A/G.711U/Opus audio
 *   - VENC Annex B byte-stream NALs are converted to AVCC/HVCC length-prefixed
 *     NALs automatically (extract_avcc / extract_hvcc) for MP4 containers.
 *
 * Demuxer:
 *   - Opens MP4/fMP4/MPEG-TS via avformat_open_input / avformat_find_stream_info
 *   - Reads frames with av_read_frame
 *   - Converts AVCC/HVCC length-prefixed NALs back to Annex B byte-stream for
 *     video; audio (G.711/Opus) is passed through raw
 *   - EOF is signalled via frame_data->eof = 1 (returns 0)
 *
 * The public header (mp4_format.h) interface is preserved verbatim.
 */

#include "mp4_format.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavcodec/bsf.h>
}

#include <cstring>
#include <cstdio>
#include <iostream>
#include <string>
#include <time.h>
#include <vector>

// ============================================================================
// Performance measurement helper
// ============================================================================

/* Monotonic clock timestamp in microseconds, used for [PERF] logs */
static int64_t _perf_now_us() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

// ============================================================================
// Internal context structures (opaque to callers, cast from KD_HANDLE)
// ============================================================================

/*
 * Mp4TrackCtx - Per-track state, one per video (or audio) stream.
 *
 * start_pts / last_pts track the PTS range of written frames so that
 * duration can be computed without relying on libavformat internals.
 */
struct Mp4TrackCtx {
    AVStream *stream{nullptr};
    int stream_idx{-1};
    k_mp4_track_info_s track_info{};
    int64_t start_pts{0};       /* PTS of first written frame (us) */
    int64_t last_pts{0};        /* PTS of most recent frame (us) */
    uint64_t frame_count{0};
};

/*
 * Mp4MuxerCtx - Top-level muxer state, one per output file.
 *
 * Lifecycle:
 *   1. kd_mp4_create()     -> allocates ctx, sets file_name/fmp4_flag
 *   2. kd_mp4_create_track() -> adds video track
 *   3. First kd_mp4_write_frame() with HEADER data -> opens file, writes
 *      container header, sets recording=true
 *   4. Subsequent write_frame calls -> encode and write packets
 *   5. kd_mp4_destroy()    -> writes trailer, closes file, deletes ctx
 *
 * sps_pps_data caches the most recent SPS/PPS in Annex B format.
 * This is needed because:
 *   - For MP4: I-frame packets must include SPS/PPS as AVCC NALs
 *   - For TS:  I-frame packets must include SPS/PPS as Annex B prefix
 *   Both cases prepend the cached data to the I-frame payload.
 */
struct Mp4MuxerCtx {
    AVFormatContext *format_ctx{nullptr};
    std::string file_name;
    bool fmp4_flag{false};
    bool header_written{false};
    bool recording{false};

    Mp4TrackCtx video_track{};
    Mp4TrackCtx audio_track{};

    /* Cached SPS/PPS in original Annex B format (from last HEADER packet) */
    uint8_t *sps_pps_data{nullptr};
    size_t sps_pps_size{0};
    /* True after AVCC/HVCC extradata has been extracted and set on codecpar */
    bool sps_pps_extracted{false};

    /* Custom codec tag pointer array allocated for G.711A/G.711U support.
     * Freed in kd_mp4_destroy(). */
    const AVCodecTag **custom_codec_tags{nullptr};
    /* Original codec_tag saved before replacement; restored on destroy to avoid
     * leaving a dangling pointer in the shared AVOutputFormat structure. */
    const AVCodecTag * const *original_codec_tags{nullptr};
};

/*
 * Mp4DemuxerCtx - Top-level demuxer state, one per input file.
 *
 * Tracks are cached on create from avformat_find_stream_info(). av_read_frame()
 * delivers length-prefixed (AVCC/HVCC) NALs for video, which are converted to
 * Annex B byte-stream using FFmpeg's built-in h264_mp4toannexb / hevc_mp4toannexb
 * bitstream filters. Audio is passed through raw.
 */
struct Mp4DemuxerCtx {
    AVFormatContext *format_ctx{nullptr};
    std::string file_name;
    bool eof_reached{false};

    struct DemuxTrack {
        int stream_idx{-1};
        k_mp4_track_type_e track_type{K_MP4_STREAM_BUTT};
        k_mp4_codec_id_e codec_id{K_MP4_CODEC_ID_BUTT};
        k_mp4_track_info_s track_info{};
    };
    std::vector<DemuxTrack> tracks;

    /* Bitstream filters for AVCC/HVCC -> Annex B conversion.
     * h264_mp4toannexb: replaces 4-byte length prefixes with start codes,
     *                   prepends SPS/PPS before each keyframe.
     * hevc_mp4toannexb: same for H.265 (VPS+SPS+PPS before IRAP). */
    AVBSFContext *h264_bsf_ctx{nullptr};
    AVBSFContext *hevc_bsf_ctx{nullptr};

    /* Output frame buffer. Persists across get_frame() calls so the returned
     * pointer stays valid until the next get_frame() call. */
    std::vector<uint8_t> frame_buffer;

    DemuxTrack *find_track(int stream_idx) {
        for (auto &t : tracks) {
            if (t.stream_idx == stream_idx) return &t;
        }
        return nullptr;
    }
};

// ============================================================================
// Helper functions
// ============================================================================

/* Convert FFmpeg error code to human-readable string */
static std::string ff_error_str(int errnum)
{
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errnum, errbuf, sizeof(errbuf));
    return std::string(errbuf);
}

// Annex B start code: 00 00 00 01 (4-byte) or 00 00 01 (3-byte)
static size_t find_start_code(const uint8_t *data, size_t size)
{
    if (size >= 4 && data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 1) return 4;
    if (size >= 3 && data[0] == 0 && data[1] == 0 && data[2] == 1) return 3;
    return 0;
}

/* Locate the next Annex B start code after offset; returns size if none found */
static size_t find_next_start_code(const uint8_t *data, size_t size, size_t offset)
{
    for (size_t i = offset; i + 3 < size; i++) {
        if ((data[i] == 0 && data[i+1] == 0 && data[i+2] == 0 && data[i+3] == 1) ||
            (data[i] == 0 && data[i+1] == 0 && data[i+2] == 1)) {
            return i;
        }
    }
    return size;
}

/* H.264 NAL unit types (from nal_unit_type bits 4..0) */
#define H264_NAL_SPS       7
#define H264_NAL_PPS       8
#define H264_NAL_IDR_SLICE 5

/* H.265 NAL unit types (from nal_unit_type bits 8..1) */
#define H265_NAL_VPS       32
#define H265_NAL_SPS       33
#define H265_NAL_PPS       34

// ============================================================================
// NAL unit conversion (Annex B -> AVCC/HVCC)  [muxer]
//
// VENC hardware outputs NAL units in Annex B byte-stream format:
//   [00 00 00 01] [NAL header] [NAL payload] [00 00 00 01] [next NAL] ...
//
// MP4 containers store NALs in AVCC (H.264) or HVCC (H.265) format:
//   [4-byte big-endian length] [NAL header + payload] [4-byte length] [next NAL] ...
//
// The SPS/PPS parameter sets are also extracted and stored as codec
// extradata in the AVCC/HVCC configuration record format, which is
// written into the mp4 file's stsd/mp4v/hvc1 box by libavformat.
// ============================================================================

/*
 * extract_avcc - Build AVCC configuration record from H.264 Annex B SPS/PPS
 *
 * AVCC extradata layout (ISO 14496-15):
 *   [1]  version = 1
 *   [3]  profile, compatibility, level (copied from SPS bytes 1-3)
 *   [1]  0xFF (length_size_minus_one = 3, i.e. 4-byte NAL lengths)
 *   [1]  0xE1 (num_of_sps = 1)
 *   [2]  SPS NAL length (big-endian)
 *   [n]  SPS NAL payload (without start code)
 *   [1]  num_of_pps = 1
 *   [2]  PPS NAL length (big-endian)
 *   [m]  PPS NAL payload (without start code)
 */
static int extract_avcc(Mp4MuxerCtx *ctx, const uint8_t *data, size_t size)
{
    size_t sps_start = 0, sps_sc_len = 0;
    size_t pps_start = 0, pps_sc_len = 0;
    size_t sps_end = 0;

    for (size_t i = 0; i + 3 < size; i++) {
        size_t sc_len = find_start_code(data + i, size - i);
        if (sc_len == 0) continue;

        uint8_t nal_type = data[i + sc_len] & 0x1F;

        if (nal_type == H264_NAL_SPS && sps_start == 0) {
            sps_start = i;
            sps_sc_len = sc_len;
        } else if (nal_type == H264_NAL_PPS && pps_start == 0) {
            pps_start = i;
            pps_sc_len = sc_len;
            sps_end = i;
            break;
        }
    }

    if (sps_start == 0 || pps_start == 0) {
        std::cerr << "[mp4_format] H.264: SPS/PPS not found" << std::endl;
        return -1;
    }

    size_t pps_end = find_next_start_code(data, size, pps_start + pps_sc_len);
    size_t sps_nal_len = sps_end - (sps_start + sps_sc_len);
    size_t pps_nal_len = pps_end - (pps_start + pps_sc_len);
    size_t extradata_size = 11 + sps_nal_len + pps_nal_len;

    uint8_t *extradata = (uint8_t*)av_malloc(extradata_size);
    if (!extradata) {
        std::cerr << "[mp4_format] AVCC alloc failed" << std::endl;
        return -1;
    }

    extradata[0] = 1;
    extradata[1] = data[sps_start + sps_sc_len + 1];
    extradata[2] = data[sps_start + sps_sc_len + 2];
    extradata[3] = data[sps_start + sps_sc_len + 3];
    extradata[4] = 0xFF;
    extradata[5] = 0xE1;
    extradata[6] = (sps_nal_len >> 8) & 0xFF;
    extradata[7] = sps_nal_len & 0xFF;
    memcpy(extradata + 8, data + sps_start + sps_sc_len, sps_nal_len);

    size_t pps_offset = 8 + sps_nal_len;
    extradata[pps_offset] = 0x01;
    extradata[pps_offset + 1] = (pps_nal_len >> 8) & 0xFF;
    extradata[pps_offset + 2] = pps_nal_len & 0xFF;
    memcpy(extradata + pps_offset + 3, data + pps_start + pps_sc_len, pps_nal_len);

    if (ctx->video_track.stream->codecpar->extradata) {
        av_free(ctx->video_track.stream->codecpar->extradata);
    }
    ctx->video_track.stream->codecpar->extradata = extradata;
    ctx->video_track.stream->codecpar->extradata_size = extradata_size;

    if (!ctx->sps_pps_data) {
        ctx->sps_pps_data = (uint8_t*)av_malloc(size);
        if (ctx->sps_pps_data) {
            memcpy(ctx->sps_pps_data, data, size);
            ctx->sps_pps_size = size;
        }
    }

    ctx->sps_pps_extracted = true;
    return 0;
}

/*
 * extract_hvcc - Build HVCC configuration record from H.265 Annex B VPS/SPS/PPS
 *
 * HVCC extradata layout (ISO 14496-15 HEVCConfigurationRecord):
 *   [23]   Fixed header: version, profile/tier/level from SPS, indicator bytes
 *          Bytes 13-22 contain hardcoded indicator values:
 *            0xF0 = general_constraint_indicator flags
 *            0x00 = general_constraint_indicator (continued)
 *            0xFC = general_level_flag bits
 *            0xFD = min_spatial_segmentation_idc
 *            0xF8 = parallelismType
 *            0xF8 = chromaFormat
 *            0x00 = bitDepthLuma-8
 *            0x00 = bitDepthChroma-8
 *            0x03 = avgFrameRate[0]
 *            3    = numOfArrays (VPS + SPS + PPS)
 *   Then per NAL array:
 *     [1]    array_completeness | NAL type (0x80 | nal_type)
 *     [2]    numNalus = 1 (big-endian)
 *     [2]    NAL length (big-endian)
 *     [n]    NAL payload (without start code)
 *
 * Note: hardcoded indicator values work for the K230's Main profile output
 * but may need adjustment for other profiles/levels.
 */
static int extract_hvcc(Mp4MuxerCtx *ctx, const uint8_t *data, size_t size)
{
    struct NalInfo { const uint8_t *ptr; size_t len; };
    NalInfo vps = {nullptr, 0}, sps = {nullptr, 0}, pps = {nullptr, 0};

    size_t offset = 0;
    while (offset < size) {
        size_t sc_len = find_start_code(data + offset, size - offset);
        if (sc_len == 0) break;

        uint8_t nal_type = (data[offset + sc_len] >> 1) & 0x3F;
        size_t nal_start = offset + sc_len;
        size_t next_sc = find_next_start_code(data, size, nal_start);
        size_t nal_len = next_sc - nal_start;

        if (nal_type == H265_NAL_VPS && vps.ptr == nullptr) vps = {data + nal_start, nal_len};
        else if (nal_type == H265_NAL_SPS && sps.ptr == nullptr) sps = {data + nal_start, nal_len};
        else if (nal_type == H265_NAL_PPS && pps.ptr == nullptr) pps = {data + nal_start, nal_len};

        offset = next_sc;
    }

    if (sps.ptr == nullptr || pps.ptr == nullptr) {
        std::cerr << "[mp4_format] H.265: SPS/PPS not found" << std::endl;
        return -1;
    }

    size_t hvcc_size = 23 + 3 * 5 + (vps.ptr ? vps.len : 0) + sps.len + pps.len;
    uint8_t *extradata = (uint8_t*)av_malloc(hvcc_size);
    if (!extradata) {
        std::cerr << "[mp4_format] HVCC alloc failed" << std::endl;
        return -1;
    }

    memset(extradata, 0, hvcc_size);

    extradata[0] = 1;
    extradata[1]  = sps.ptr[2];
    extradata[2]  = sps.ptr[3];
    extradata[3]  = sps.ptr[4];
    extradata[4]  = sps.ptr[5];
    extradata[5]  = sps.ptr[6];
    extradata[6]  = sps.ptr[7];
    extradata[7]  = sps.ptr[8];
    extradata[8]  = sps.ptr[9];
    extradata[9]  = sps.ptr[10];
    extradata[10] = sps.ptr[11];
    extradata[11] = sps.ptr[12];
    extradata[12] = sps.ptr[13];
    extradata[13] = 0xF0;
    extradata[14] = 0x00;
    extradata[15] = 0xFC;
    extradata[16] = 0xFD;
    extradata[17] = 0xF8;
    extradata[18] = 0xF8;
    extradata[19] = 0x00;
    extradata[20] = 0x00;
    extradata[21] = 0x03;
    extradata[22] = 3;

    size_t pos = 23;

    if (vps.ptr) {
        extradata[pos++] = 0x80 | H265_NAL_VPS;
        extradata[pos++] = 0; extradata[pos++] = 1;
        extradata[pos++] = (vps.len >> 8) & 0xFF;
        extradata[pos++] = vps.len & 0xFF;
        memcpy(extradata + pos, vps.ptr, vps.len);
        pos += vps.len;
    }

    extradata[pos++] = 0x80 | H265_NAL_SPS;
    extradata[pos++] = 0; extradata[pos++] = 1;
    extradata[pos++] = (sps.len >> 8) & 0xFF;
    extradata[pos++] = sps.len & 0xFF;
    memcpy(extradata + pos, sps.ptr, sps.len);
    pos += sps.len;

    extradata[pos++] = 0x80 | H265_NAL_PPS;
    extradata[pos++] = 0; extradata[pos++] = 1;
    extradata[pos++] = (pps.len >> 8) & 0xFF;
    extradata[pos++] = pps.len & 0xFF;
    memcpy(extradata + pos, pps.ptr, pps.len);
    pos += pps.len;

    if (ctx->video_track.stream->codecpar->extradata) {
        av_free(ctx->video_track.stream->codecpar->extradata);
    }
    ctx->video_track.stream->codecpar->extradata = extradata;
    ctx->video_track.stream->codecpar->extradata_size = hvcc_size;

    if (!ctx->sps_pps_data) {
        ctx->sps_pps_data = (uint8_t*)av_malloc(size);
        if (ctx->sps_pps_data) {
            memcpy(ctx->sps_pps_data, data, size);
            ctx->sps_pps_size = size;
        }
    }

    ctx->sps_pps_extracted = true;
    return 0;
}

// ============================================================================
// Packet creation helpers  [muxer]
//
// These functions convert Annex B NAL data into AVPacket payloads suitable
// for av_interleaved_write_frame(). For MP4, NALs use 4-byte length prefix
// (AVCC format); for TS, Annex B data is passed through unchanged.
// ============================================================================

/*
 * create_av_packet - Convert a single Annex B NAL unit to AVCC-format packet
 *
 * Input:  [00 00 00 01] [NAL header + payload]
 * Output: [4-byte big-endian length] [NAL header + payload]
 */
static int create_av_packet(Mp4MuxerCtx *ctx, const uint8_t *data, size_t size, uint64_t timestamp, AVPacket *pkt)
{
    (void)ctx;
    (void)timestamp;
    if (!pkt || !data || size < 4) return -1;

    size_t sc_len = find_start_code(data, size);
    if (sc_len == 0) {
        std::cerr << "[mp4_format] Invalid start code" << std::endl;
        return -1;
    }

    size_t nal_len = size - sc_len;
    int ret = av_new_packet(pkt, 4 + nal_len);
    if (ret < 0) {
        std::cerr << "[mp4_format] Packet alloc failed: " << ff_error_str(ret) << std::endl;
        return -1;
    }

    pkt->data[0] = (nal_len >> 24) & 0xFF;
    pkt->data[1] = (nal_len >> 16) & 0xFF;
    pkt->data[2] = (nal_len >>  8) & 0xFF;
    pkt->data[3] = nal_len & 0xFF;
    memcpy(pkt->data + 4, data + sc_len, nal_len);
    pkt->size = 4 + nal_len;

    return 0;
}

/*
 * create_multi_nal_packet - Combine SPS/PPS header + I-frame into a single AVCC packet
 *
 * For MP4 containers, keyframes must include SPS/PPS in the same packet so the
 * decoder can initialize. This function:
 *   1. Parses header_data (cached SPS/PPS) into individual NAL units
 *   2. Appends the I-frame NAL from frame_data
 *   3. Concatenates all NALs with 4-byte length prefixes into one AVPacket
 *
 * For H.264: header contains SPS + PPS (2 NALs), total = 3 NALs in output
 * For H.265: header contains VPS + SPS + PPS (3 NALs), total = 4 NALs in output
 */
static int create_multi_nal_packet(Mp4MuxerCtx *ctx, const uint8_t *header_data, size_t header_size,
                                    const uint8_t *frame_data, size_t frame_size,
                                    uint64_t timestamp, AVPacket *pkt)
{
    (void)timestamp;
    if (!pkt || !header_data || !frame_data || header_size < 4 || frame_size < 4) return -1;

    struct NalUnit {
        const uint8_t *payload;
        size_t len;
    };
    NalUnit nals[4];
    int nal_count = 0;

    int codec_type = (ctx->video_track.track_info.video_info.codec_id == K_MP4_CODEC_ID_H265) ? 1 : 0;
    int max_nals = codec_type ? 3 : 2;
    size_t offset = 0;

    while (offset < header_size && nal_count < max_nals) {
        size_t sc_len = find_start_code(header_data + offset, header_size - offset);
        if (sc_len == 0) break;

        uint8_t nal_type;
        if (codec_type) {
            nal_type = (header_data[offset + sc_len] >> 1) & 0x3F;
        } else {
            nal_type = header_data[offset + sc_len] & 0x1F;
        }

        bool valid = false;
        if (codec_type) {
            valid = (nal_type == H265_NAL_VPS || nal_type == H265_NAL_SPS || nal_type == H265_NAL_PPS);
        } else {
            valid = (nal_type == H264_NAL_SPS || nal_type == H264_NAL_PPS);
        }
        if (!valid) break;

        size_t nal_start = offset + sc_len;
        size_t next_sc = find_next_start_code(header_data, header_size, nal_start);

        nals[nal_count].payload = header_data + nal_start;
        nals[nal_count].len = next_sc - nal_start;
        nal_count++;
        offset = next_sc;
    }

    size_t frame_sc_len = find_start_code(frame_data, frame_size);
    if (frame_sc_len == 0) return -1;

    nals[nal_count].payload = frame_data + frame_sc_len;
    nals[nal_count].len = frame_size - frame_sc_len;
    nal_count++;

    size_t total_size = 0;
    for (int i = 0; i < nal_count; i++) {
        total_size += 4 + nals[i].len;
    }

    int ret = av_new_packet(pkt, total_size);
    if (ret < 0) return -1;

    uint8_t *p = pkt->data;
    for (int i = 0; i < nal_count; i++) {
        uint32_t nal_len = nals[i].len;
        *p++ = (nal_len >> 24) & 0xFF;
        *p++ = (nal_len >> 16) & 0xFF;
        *p++ = (nal_len >>  8) & 0xFF;
        *p++ = nal_len & 0xFF;
        memcpy(p, nals[i].payload, nals[i].len);
        p += nals[i].len;
    }
    pkt->size = total_size;

    return 0;
}

// ============================================================================
// G.711A/G.711U codec tag registration for MP4 container  [muxer]
//
// Problem: FFmpeg 4.4's MP4 muxer uses a restricted codec_mp4_tags[] whitelist
// that does NOT include G.711A (pcm_alaw) or G.711U (pcm_mulaw), even though
// these are registered at mp4ra.org and the MOV muxer supports them fine.
// This causes avformat_write_header() to reject audio streams with:
//   "Could not find tag for codec pcm_alaw in stream #1,
//    codec not currently supported in container"
//
// Fix: Append the MOV audio tag list (obtained via avformat_get_mov_audio_tags())
// to the MP4 muxer's codec_tag list. The MOV audio tags already include
// PCM_ALAW → 'alaw' and PCM_MULAW → 'ulaw', so this makes the MP4 muxer
// accept them too. AVCodecTag is an incomplete type in FFmpeg's public headers,
// so we cannot construct our own tag array — reusing the MOV tags is the
// only clean approach.
// ============================================================================

/*
 * _register_g711_codec_tags - Extend the output format's codec_tag list
 * with MOV audio tags (which include G.711A/G.711U).
 *
 * The format's codec_tag is a NULL-terminated array of pointers to AVCodecTag
 * arrays. We allocate a new pointer array that includes all original entries
 * plus the MOV audio tag list, then replace the format's codec_tag field.
 * The allocated array is stored in ctx->custom_codec_tags for cleanup.
 */
static void _register_g711_codec_tags(Mp4MuxerCtx *ctx)
{
    if (!ctx || !ctx->format_ctx || !ctx->format_ctx->oformat) return;

    /* Only needed for MP4 container (MOV muxer already has these tags) */
    const char *fmt_name = ctx->format_ctx->oformat->name;
    if (!fmt_name || strcmp(fmt_name, "mp4") != 0) return;

    int tag_count = 0;
    if (ctx->format_ctx->oformat->codec_tag) {
        while (ctx->format_ctx->oformat->codec_tag[tag_count]) {
            tag_count++;
        }
    }

    ctx->custom_codec_tags = (const AVCodecTag **)av_malloc_array(
        tag_count + 2, sizeof(AVCodecTag *));
    if (!ctx->custom_codec_tags) {
        std::cerr << "[mp4_format] Failed to allocate custom codec tags" << std::endl;
        return;
    }

    for (int i = 0; i < tag_count; i++) {
        ctx->custom_codec_tags[i] = ctx->format_ctx->oformat->codec_tag[i];
    }
    /* Append MOV audio tags which include G.711A ('alaw') and G.711U ('ulaw') */
    ctx->custom_codec_tags[tag_count] = avformat_get_mov_audio_tags();
    ctx->custom_codec_tags[tag_count + 1] = NULL;

    /* Save original codec_tag before replacing (so we can restore on destroy) */
    ctx->original_codec_tags = ctx->format_ctx->oformat->codec_tag;

    /* Cast away constness to add our custom tags to the output format */
    AVOutputFormat *mutable_fmt = const_cast<AVOutputFormat*>(ctx->format_ctx->oformat);
    mutable_fmt->codec_tag = ctx->custom_codec_tags;
}

// ============================================================================
// AVCC/HVCC -> Annex B conversion  [demuxer]
//
// Uses FFmpeg's built-in bitstream filters (h264_mp4toannexb / hevc_mp4toannexb)
// which correctly handle:
//   - Replacing 4-byte NALU length prefixes with 00 00 00 01 start codes
//   - Prepending SPS/PPS (from codec extradata) before each keyframe
//   - Edge cases: multi-NAL packets, AUD filtering, etc.
//
// The BSFs are initialized once during kd_mp4_create (demuxer path) and
// reused for all subsequent kd_mp4_get_frame calls.
// ============================================================================

/*
 * Initialize a bitstream filter context for the given stream.
 * Returns 0 on success, -1 on failure.
 */
static int init_bsf(AVBSFContext **bsf_ctx, const char *filter_name,
                    AVCodecParameters *codecpar)
{
    const AVBitStreamFilter *bsf = av_bsf_get_by_name(filter_name);
    if (!bsf) {
        std::cerr << "[mp4_format] BSF '" << filter_name << "' not found" << std::endl;
        return -1;
    }

    int ret = av_bsf_alloc(bsf, bsf_ctx);
    if (ret < 0) {
        std::cerr << "[mp4_format] BSF alloc failed: " << ff_error_str(ret) << std::endl;
        return -1;
    }

    ret = avcodec_parameters_copy((*bsf_ctx)->par_in, codecpar);
    if (ret < 0) {
        std::cerr << "[mp4_format] BSF codecpar copy failed: " << ff_error_str(ret) << std::endl;
        av_bsf_free(bsf_ctx);
        return -1;
    }

    ret = av_bsf_init(*bsf_ctx);
    if (ret < 0) {
        std::cerr << "[mp4_format] BSF init failed: " << ff_error_str(ret) << std::endl;
        av_bsf_free(bsf_ctx);
        return -1;
    }

    return 0;
}

/*
 * Run a packet through the appropriate BSF to convert AVCC/HVCC -> Annex B.
 * The output is stored in ctx->frame_buffer.
 * Returns 0 on success, -1 on failure.
 */
static int bsf_filter_to_annexb(AVBSFContext *bsf_ctx, AVPacket *pkt,
                                Mp4DemuxerCtx *ctx)
{
    int ret = av_bsf_send_packet(bsf_ctx, pkt);
    if (ret < 0) {
        std::cerr << "[mp4_format] BSF send_packet failed: " << ff_error_str(ret) << std::endl;
        return -1;
    }

    AVPacket *filtered = av_packet_alloc();
    if (!filtered) return -1;

    ret = av_bsf_receive_packet(bsf_ctx, filtered);
    if (ret < 0) {
        std::cerr << "[mp4_format] BSF receive_packet failed: " << ff_error_str(ret) << std::endl;
        av_packet_free(&filtered);
        return -1;
    }

    ctx->frame_buffer.assign(filtered->data, filtered->data + filtered->size);
    av_packet_free(&filtered);
    return 0;
}

// ============================================================================
// Public API implementation
// ============================================================================

int kd_mp4_create(KD_HANDLE *mp4_handle, k_mp4_config_s *mp4_cfg)
{
    if (!mp4_handle || !mp4_cfg) return -1;

    if (mp4_cfg->config_type == K_MP4_CONFIG_MUXER) {
        Mp4MuxerCtx *ctx = new (std::nothrow) Mp4MuxerCtx();
        if (!ctx) {
            std::cerr << "[mp4_format] Failed to allocate muxer context" << std::endl;
            return -1;
        }

        ctx->file_name = mp4_cfg->muxer_config.file_name;
        ctx->fmp4_flag = mp4_cfg->muxer_config.fmp4_flag;

        // Auto-detect container format from file extension
        const char *format_name = nullptr;
        size_t ext_pos = ctx->file_name.find_last_of('.');
        if (ext_pos != std::string::npos) {
            std::string ext = ctx->file_name.substr(ext_pos);
            if (ext == ".ts") {
                format_name = "mpegts";
            } else if (ext == ".mp4") {
                // nullptr lets libavformat guess from filename; fmp4_flag is applied
                // later via movflags option in avformat_write_header
                format_name = ctx->fmp4_flag ? "mp4" : nullptr;
            }
        }

        int ret = avformat_alloc_output_context2(&ctx->format_ctx, nullptr, format_name, ctx->file_name.c_str());
        if (ret < 0) {
            std::cerr << "[mp4_format] Failed to allocate output context: " << ff_error_str(ret) << std::endl;
            delete ctx;
            return -1;
        }

        // Register G.711A/G.711U codec tags so the MP4 muxer accepts audio streams.
        // Without this, avformat_write_header() fails with:
        //   "Could not find tag for codec pcm_alaw, codec not currently supported in container"
        _register_g711_codec_tags(ctx);

        *mp4_handle = static_cast<KD_HANDLE>(ctx);
        return 0;
    } else if (mp4_cfg->config_type == K_MP4_CONFIG_DEMUXER) {
        Mp4DemuxerCtx *ctx = new (std::nothrow) Mp4DemuxerCtx();
        if (!ctx) {
            std::cerr << "[mp4_format] Failed to allocate demuxer context" << std::endl;
            return -1;
        }

        ctx->file_name = mp4_cfg->demuxer_config.file_name;

        int ret = avformat_open_input(&ctx->format_ctx, ctx->file_name.c_str(), nullptr, nullptr);
        if (ret < 0) {
            std::cerr << "[mp4_format] Failed to open input file: " << ff_error_str(ret) << std::endl;
            delete ctx;
            return -1;
        }

        ret = avformat_find_stream_info(ctx->format_ctx, nullptr);
        if (ret < 0) {
            std::cerr << "[mp4_format] Failed to find stream info: " << ff_error_str(ret) << std::endl;
            avformat_close_input(&ctx->format_ctx);
            delete ctx;
            return -1;
        }

        /* Cache track info from each stream for get_file_info / get_track_by_index */
        AVStream *h264_st = nullptr;
        AVStream *hevc_st = nullptr;
        for (unsigned int i = 0; i < ctx->format_ctx->nb_streams; i++) {
            AVStream *st = ctx->format_ctx->streams[i];
            AVCodecParameters *cp = st->codecpar;
            Mp4DemuxerCtx::DemuxTrack dt;
            dt.stream_idx = (int)i;

            if (cp->codec_type == AVMEDIA_TYPE_VIDEO) {
                dt.track_type = K_MP4_STREAM_VIDEO;
                if (cp->codec_id == AV_CODEC_ID_H265) {
                    dt.codec_id = K_MP4_CODEC_ID_H265;
                    if (!hevc_st) hevc_st = st;
                } else {
                    dt.codec_id = K_MP4_CODEC_ID_H264;
                    if (!h264_st) h264_st = st;
                }
                dt.track_info.track_type = K_MP4_STREAM_VIDEO;
                dt.track_info.time_scale = st->time_base.den ? (uint32_t)st->time_base.den : 1000;
                dt.track_info.video_info.width = (uint32_t)cp->width;
                dt.track_info.video_info.height = (uint32_t)cp->height;
                dt.track_info.video_info.track_id = i + 1;
                dt.track_info.video_info.codec_id = dt.codec_id;
            } else if (cp->codec_type == AVMEDIA_TYPE_AUDIO) {
                dt.track_type = K_MP4_STREAM_AUDIO;
                if (cp->codec_id == AV_CODEC_ID_PCM_ALAW) {
                    dt.codec_id = K_MP4_CODEC_ID_G711A;
                } else if (cp->codec_id == AV_CODEC_ID_PCM_MULAW) {
                    dt.codec_id = K_MP4_CODEC_ID_G711U;
                } else if (cp->codec_id == AV_CODEC_ID_OPUS) {
                    dt.codec_id = K_MP4_CODEC_ID_OPUS;
                } else {
                    dt.codec_id = K_MP4_CODEC_ID_BUTT;
                }
                dt.track_info.track_type = K_MP4_STREAM_AUDIO;
                dt.track_info.time_scale = cp->sample_rate ? (uint32_t)cp->sample_rate : 8000;
                dt.track_info.audio_info.channels = (uint32_t)cp->channels;
                dt.track_info.audio_info.sample_rate = (uint32_t)cp->sample_rate;
                dt.track_info.audio_info.bit_per_sample = (uint32_t)cp->bits_per_coded_sample;
                dt.track_info.audio_info.track_id = i + 1;
                dt.track_info.audio_info.codec_id = dt.codec_id;
            } else {
                continue; /* skip subtitle/hint tracks */
            }

            ctx->tracks.push_back(dt);
        }

        /* Initialize the AVCC/HVCC -> Annex B bitstream filters (one per video
         * codec type present). The BSF reads SPS/PPS/VPS from codecpar->extradata
         * and inserts them before each keyframe in the Annex B output. */
        if (h264_st) {
            if (init_bsf(&ctx->h264_bsf_ctx, "h264_mp4toannexb", h264_st->codecpar) < 0) {
                avformat_close_input(&ctx->format_ctx);
                delete ctx;
                return -1;
            }
        }
        if (hevc_st) {
            if (init_bsf(&ctx->hevc_bsf_ctx, "hevc_mp4toannexb", hevc_st->codecpar) < 0) {
                av_bsf_free(&ctx->h264_bsf_ctx);
                avformat_close_input(&ctx->format_ctx);
                delete ctx;
                return -1;
            }
        }

        *mp4_handle = static_cast<KD_HANDLE>(ctx);
        return 0;
    }

    std::cerr << "[mp4_format] Unsupported config type" << std::endl;
    return -1;
}

int kd_mp4_destroy(KD_HANDLE mp4_handle)
{
    if (!mp4_handle) return -1;

    /* Try muxer first; demuxer contexts don't have the muxer fields, so we
     * distinguish by checking whether the format context is an output context. */
    if (((Mp4MuxerCtx *)mp4_handle)->format_ctx &&
        ((Mp4MuxerCtx *)mp4_handle)->format_ctx->oformat) {
        Mp4MuxerCtx *ctx = static_cast<Mp4MuxerCtx*>(mp4_handle);

        if (ctx->recording) {
            if (ctx->header_written && ctx->format_ctx) {
                av_write_trailer(ctx->format_ctx);
            }

            if (ctx->format_ctx && ctx->format_ctx->pb) {
                avio_close(ctx->format_ctx->pb);
                ctx->format_ctx->pb = nullptr;
            }

            ctx->recording = false;
        }

        if (ctx->sps_pps_data) {
            av_free(ctx->sps_pps_data);
            ctx->sps_pps_data = nullptr;
            ctx->sps_pps_size = 0;
        }

        if (ctx->custom_codec_tags) {
            /* Restore original codec_tag on the shared AVOutputFormat before freeing
             * our custom array. Without this, oformat->codec_tag becomes a dangling
             * pointer, causing the next MP4 recording to crash (use-after-free). */
            if (ctx->format_ctx && ctx->format_ctx->oformat && ctx->original_codec_tags) {
                AVOutputFormat *mutable_fmt = const_cast<AVOutputFormat*>(ctx->format_ctx->oformat);
                mutable_fmt->codec_tag = const_cast<const AVCodecTag**>(ctx->original_codec_tags);
            }
            av_free(ctx->custom_codec_tags);
            ctx->custom_codec_tags = nullptr;
            ctx->original_codec_tags = nullptr;
        }

        if (ctx->format_ctx) {
            avformat_free_context(ctx->format_ctx);
            ctx->format_ctx = nullptr;
        }

        delete ctx;
        return 0;
    }

    /* Demuxer path */
    Mp4DemuxerCtx *ctx = static_cast<Mp4DemuxerCtx*>(mp4_handle);
    if (ctx->h264_bsf_ctx) {
        av_bsf_free(&ctx->h264_bsf_ctx);
    }
    if (ctx->hevc_bsf_ctx) {
        av_bsf_free(&ctx->hevc_bsf_ctx);
    }
    if (ctx->format_ctx) {
        avformat_close_input(&ctx->format_ctx);
    }
    delete ctx;
    return 0;
}

int kd_mp4_create_track(KD_HANDLE mp4_handle, KD_HANDLE *track_handle, k_mp4_track_info_s *mp4_track_info)
{
    if (!mp4_handle || !track_handle || !mp4_track_info) return -1;

    Mp4MuxerCtx *ctx = static_cast<Mp4MuxerCtx*>(mp4_handle);

    AVStream *stream = avformat_new_stream(ctx->format_ctx, nullptr);
    if (!stream) {
        std::cerr << "[mp4_format] Failed to create stream" << std::endl;
        return -1;
    }

    Mp4TrackCtx *track = nullptr;

    if (mp4_track_info->track_type == K_MP4_STREAM_VIDEO) {
        track = &ctx->video_track;
        track->stream = stream;
        track->stream_idx = stream->index;
        track->track_info = *mp4_track_info;

        AVCodecParameters *codecpar = stream->codecpar;
        codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        codecpar->codec_id = (mp4_track_info->video_info.codec_id == K_MP4_CODEC_ID_H265)
                             ? AV_CODEC_ID_H265 : AV_CODEC_ID_H264;
        codecpar->width = mp4_track_info->video_info.width;
        codecpar->height = mp4_track_info->video_info.height;
        codecpar->format = AV_PIX_FMT_YUV420P;
        codecpar->codec_tag = 0;

        bool is_ts = (ctx->file_name.size() > 3 && ctx->file_name.substr(ctx->file_name.size() - 3) == ".ts");
        stream->time_base = is_ts ? av_make_q(1, 90000) : av_make_q(1, mp4_track_info->time_scale);
    } else if (mp4_track_info->track_type == K_MP4_STREAM_AUDIO) {
        track = &ctx->audio_track;
        track->stream = stream;
        track->stream_idx = stream->index;
        track->track_info = *mp4_track_info;

        AVCodecParameters *codecpar = stream->codecpar;
        codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
        if (mp4_track_info->audio_info.codec_id == K_MP4_CODEC_ID_G711A) {
            codecpar->codec_id = AV_CODEC_ID_PCM_ALAW;
        } else if (mp4_track_info->audio_info.codec_id == K_MP4_CODEC_ID_G711U) {
            codecpar->codec_id = AV_CODEC_ID_PCM_MULAW;
        } else if (mp4_track_info->audio_info.codec_id == K_MP4_CODEC_ID_OPUS) {
            codecpar->codec_id = AV_CODEC_ID_OPUS;
        } else {
            std::cerr << "[mp4_format] Unsupported audio codec" << std::endl;
            return -1;
        }
        codecpar->sample_rate = mp4_track_info->audio_info.sample_rate;
        codecpar->channels = mp4_track_info->audio_info.channels;
        codecpar->bits_per_coded_sample = 8;
        codecpar->block_align = mp4_track_info->audio_info.channels;

        // Opus in MP4 requires OpusHead extradata (ISOBMFF Opus specification).
        // 19 bytes: 'OpusHead'(8) + version(1) + channels(1) + pre-skip(2 LE)
        //           + input_sample_rate(4 LE) + output_gain(2 LE) + mapping_family(1)
        if (mp4_track_info->audio_info.codec_id == K_MP4_CODEC_ID_OPUS) {
            static const uint8_t opus_head_mono_8k[] = {
                'O','p','u','s','H','e','a','d',  // magic
                0,                                  // version
                1,                                  // channel count
                0x38, 0x01,                         // pre-skip = 312 (little-endian)
                0x40, 0x1F, 0x00, 0x00,             // input sample rate = 8000 (little-endian)
                0x00, 0x00,                         // output gain = 0
                0                                   // channel mapping family = 0 (mono/stereo)
            };
            codecpar->extradata = (uint8_t*)av_malloc(sizeof(opus_head_mono_8k) + AV_INPUT_BUFFER_PADDING_SIZE);
            if (codecpar->extradata) {
                memcpy(codecpar->extradata, opus_head_mono_8k, sizeof(opus_head_mono_8k));
                codecpar->extradata_size = sizeof(opus_head_mono_8k);
                memset(codecpar->extradata + sizeof(opus_head_mono_8k), 0, AV_INPUT_BUFFER_PADDING_SIZE);
            }
            codecpar->bits_per_coded_sample = 16;
            codecpar->block_align = 1;  // Opus has no fixed block alignment
        }
        // time_base must be 1/sample_rate for MOV/MP4 audio tracks.
        stream->time_base = av_make_q(1, codecpar->sample_rate);
    } else {
        std::cerr << "[mp4_format] Unknown track type" << std::endl;
        return -1;
    }

    *track_handle = static_cast<KD_HANDLE>(track);
    return 0;
}

int kd_mp4_destroy_tracks(KD_HANDLE mp4_handle)
{
    if (!mp4_handle) return -1;

    Mp4MuxerCtx *ctx = static_cast<Mp4MuxerCtx*>(mp4_handle);
    ctx->video_track.stream = nullptr;
    ctx->video_track.stream_idx = -1;
    ctx->audio_track.stream = nullptr;
    ctx->audio_track.stream_idx = -1;

    return 0;
}

int kd_mp4_write_frame(KD_HANDLE mp4_handle, KD_HANDLE track_handle, k_mp4_frame_data_s *frame_data)
{
    if (!mp4_handle || !track_handle || !frame_data || !frame_data->data || frame_data->data_length == 0) return -1;

    Mp4MuxerCtx *ctx = static_cast<Mp4MuxerCtx*>(mp4_handle);
    Mp4TrackCtx *track = static_cast<Mp4TrackCtx*>(track_handle);

    if (!ctx->format_ctx || !track->stream) return -1;

    // --- Audio frame fast path: raw encoded data (G.711/Opus), no Annex B parsing ---
    if (frame_data->codec_id == K_MP4_CODEC_ID_G711A || frame_data->codec_id == K_MP4_CODEC_ID_G711U || frame_data->codec_id == K_MP4_CODEC_ID_OPUS) {
        if (!ctx->recording) {
            // Open output file on first audio frame if not already open
            if (!ctx->header_written) {
                int ret = avformat_write_header(ctx->format_ctx, nullptr);
                if (ret < 0) {
                    std::cerr << "[mp4_format] Audio: write_header failed: " << ff_error_str(ret) << std::endl;
                    return -1;
                }
                ctx->header_written = true;
            }
            ctx->recording = true;
        }

        // Each track uses its own start_pts for relative PTS calculation.
        // Audio may start later than video (AENC init takes time), which
        // the MP4 container handles via per-track stts/elst atoms.
        if (track->start_pts == 0) track->start_pts = frame_data->time_stamp;

        int64_t audio_ts = frame_data->time_stamp - track->start_pts;

        AVPacket pkt;
        av_init_packet(&pkt);
        pkt.stream_index = track->stream_idx;
        pkt.data = frame_data->data;
        pkt.size = frame_data->data_length;
        // Rescale from us to audio stream time_base (1/sample_rate).
        pkt.pts = av_rescale_q(audio_ts, av_make_q(1, 1000000), track->stream->time_base);
        pkt.dts = pkt.pts;
        pkt.flags = AV_PKT_FLAG_KEY;
        // Duration in time_base units (1/sample_rate):
        // G.711A: 1 byte per sample, block_align = channels,
        // samples = data_length / block_align.
        pkt.duration = frame_data->data_length / track->stream->codecpar->block_align;

        int ret = av_interleaved_write_frame(ctx->format_ctx, &pkt);
        if (ret < 0) {
            std::cerr << "[mp4_format] Audio write failed: " << ff_error_str(ret) << std::endl;
            return -1;
        }

        track->last_pts = frame_data->time_stamp;
        track->frame_count++;
        return 0;
    }

    // --- Video frame path: Annex B NAL unit parsing ---
    bool is_ts = (ctx->file_name.size() > 3 && ctx->file_name.substr(ctx->file_name.size() - 3) == ".ts");

    size_t sc_len = find_start_code(frame_data->data, frame_data->data_length);
    if (sc_len == 0) return -1;

    uint8_t nal_type;
    if (frame_data->codec_id == K_MP4_CODEC_ID_H265) {
        nal_type = (frame_data->data[sc_len] >> 1) & 0x3F;
    } else {
        nal_type = frame_data->data[sc_len] & 0x1F;
    }

    bool is_header = false;
    bool is_keyframe = false;

    if (frame_data->codec_id == K_MP4_CODEC_ID_H265) {
        is_header = (nal_type == H265_NAL_VPS || nal_type == H265_NAL_SPS || nal_type == H265_NAL_PPS);
        is_keyframe = (nal_type >= 16 && nal_type <= 23);
    } else {
        is_header = (nal_type == H264_NAL_SPS || nal_type == H264_NAL_PPS);
        is_keyframe = (nal_type == H264_NAL_IDR_SLICE);
    }

    /*
     * SPS/PPS header packet: cache it, convert to AVCC/HVCC if needed,
     * and open the output file on first encounter.
     */
    if (is_header) {
        if (ctx->sps_pps_data) av_free(ctx->sps_pps_data);
        ctx->sps_pps_data = (uint8_t*)av_malloc(frame_data->data_length);
        if (!ctx->sps_pps_data) return -1;
        memcpy(ctx->sps_pps_data, frame_data->data, frame_data->data_length);
        ctx->sps_pps_size = frame_data->data_length;

        if (!is_ts && !ctx->sps_pps_extracted) {
            int ret = (frame_data->codec_id == K_MP4_CODEC_ID_H265)
                      ? extract_hvcc(ctx, frame_data->data, frame_data->data_length)
                      : extract_avcc(ctx, frame_data->data, frame_data->data_length);
            if (ret < 0) return -1;
        }

        if (!is_ts) {
            ctx->sps_pps_extracted = true;
        }

        if (!ctx->recording) {
            int64_t t0 = _perf_now_us();
            int ret2 = avio_open(&ctx->format_ctx->pb, ctx->file_name.c_str(), AVIO_FLAG_WRITE);
            if (ret2 < 0) {
                std::cerr << "[mp4_format] Failed to open file: " << ff_error_str(ret2) << std::endl;
                return -1;
            }
            printf("[PERF]   avio_open: %.2f ms\n", (_perf_now_us() - t0) / 1000.0);

            t0 = _perf_now_us();
            AVDictionary *opts = nullptr;
            if (ctx->fmp4_flag) {
                av_dict_set(&opts, "movflags", "frag_keyframe+default_base_moof", 0);
            } else if (is_ts) {
                av_dict_set(&opts, "mpegts_flags", "resend_headers", 0);
            }

            ret2 = avformat_write_header(ctx->format_ctx, &opts);
            if (ret2 < 0) {
                std::cerr << "[mp4_format] Header write failed: " << ff_error_str(ret2) << std::endl;
                av_dict_free(&opts);
                avio_close(ctx->format_ctx->pb);
                ctx->format_ctx->pb = nullptr;
                return -1;
            }
            av_dict_free(&opts);
            printf("[PERF]   avformat_write_header: %.2f ms\n", (_perf_now_us() - t0) / 1000.0);

            ctx->header_written = true;
            ctx->recording = true;
            track->start_pts = 0;
            track->last_pts = 0;
            track->frame_count = 0;

            std::cout << "[mp4_format] Recording started: " << ctx->file_name << std::endl;
        }
        return 0;
    }

    if (!ctx->recording) return 0;

    AVPacket *pkt = av_packet_alloc();
    if (!pkt) return -1;

    int ret = 0;

    if (is_keyframe) {
        if (is_ts) {
            // TS: prepend cached SPS/PPS as raw Annex B prefix (decoder needs
            // parameter sets before each IDR in MPEG-TS for independent segments)
            size_t total_size = (ctx->sps_pps_data && ctx->sps_pps_size > 0)
                                ? (ctx->sps_pps_size + frame_data->data_length) : frame_data->data_length;
            ret = av_new_packet(pkt, total_size);
            if (ret >= 0) {
                if (ctx->sps_pps_data && ctx->sps_pps_size > 0) {
                    memcpy(pkt->data, ctx->sps_pps_data, ctx->sps_pps_size);
                    memcpy(pkt->data + ctx->sps_pps_size, frame_data->data, frame_data->data_length);
                } else {
                    memcpy(pkt->data, frame_data->data, frame_data->data_length);
                }
                pkt->size = total_size;
            }
        } else {
            // MP4: combine SPS/PPS + I-frame into a single AVCC multi-NAL packet
            if (ctx->sps_pps_data && ctx->sps_pps_size > 0) {
                ret = create_multi_nal_packet(ctx, ctx->sps_pps_data, ctx->sps_pps_size,
                                              frame_data->data, frame_data->data_length,
                                              frame_data->time_stamp, pkt);
            } else {
                ret = create_av_packet(ctx, frame_data->data, frame_data->data_length, frame_data->time_stamp, pkt);
            }
        }
    } else {
        // P-frame: simple conversion (AVCC for MP4, raw copy for TS)
        if (is_ts) {
            ret = av_new_packet(pkt, frame_data->data_length);
            if (ret >= 0) {
                memcpy(pkt->data, frame_data->data, frame_data->data_length);
                pkt->size = frame_data->data_length;
            }
        } else {
            ret = create_av_packet(ctx, frame_data->data, frame_data->data_length, frame_data->time_stamp, pkt);
        }
    }

    if (ret < 0) {
        av_packet_free(&pkt);
        return -1;
    }

    int64_t pts_for_rescale = frame_data->time_stamp;

    if (track->frame_count == 0) track->start_pts = pts_for_rescale;

    int64_t ts;
    if (is_ts) {
        // MPEG-TS: absolute 90kHz PTS (no base offset)
        ts = av_rescale_q(pts_for_rescale, av_make_q(1, 1000000), track->stream->time_base);
    } else {
        // MP4: relative PTS from this track's first frame
        ts = av_rescale_q(pts_for_rescale - track->start_pts, av_make_q(1, 1000000), track->stream->time_base);
    }

    pkt->pts = ts;
    pkt->dts = ts;
    pkt->duration = av_rescale_q(1000000 / 30, av_make_q(1, 1000000), track->stream->time_base);
    pkt->stream_index = track->stream_idx;

    if (is_keyframe) {
        pkt->flags |= AV_PKT_FLAG_KEY;
    }

    // Log slow writes (>100ms) — typically caused by SD card I/O stalls on K230
    int64_t t0 = _perf_now_us();
    ret = av_interleaved_write_frame(ctx->format_ctx, pkt);
    int64_t dt = _perf_now_us() - t0;
    if (dt > 100000) {
        printf("[PERF]   av_interleaved_write_frame: %.2f ms\n", dt / 1000.0);
    }

    av_packet_free(&pkt);

    if (ret < 0) {
        std::cerr << "[mp4_format] Write failed: " << ff_error_str(ret) << std::endl;
        return -1;
    }

    track->frame_count++;
    track->last_pts = pts_for_rescale;

    return 0;
}

int kd_mp4_get_file_info(KD_HANDLE mp4_handle, k_mp4_file_info_s *file_info)
{
    if (!mp4_handle || !file_info) return -1;

    file_info->duration = 0;
    file_info->track_num = 0;

    /* Works for both muxer and demuxer contexts. For the muxer we compute
     * duration from the written PTS range; for the demuxer we use the
     * container duration reported by libavformat. */
    if (((Mp4MuxerCtx *)mp4_handle)->format_ctx &&
        ((Mp4MuxerCtx *)mp4_handle)->format_ctx->oformat) {
        /* Muxer: duration from PTS range (microseconds) */
        Mp4MuxerCtx *ctx = static_cast<Mp4MuxerCtx*>(mp4_handle);
        file_info->track_num = ctx->format_ctx->nb_streams;
        if (ctx->video_track.frame_count > 0 && ctx->video_track.last_pts > ctx->video_track.start_pts) {
            file_info->duration = ctx->video_track.last_pts - ctx->video_track.start_pts;
        } else {
            file_info->duration = 0;
        }
        return 0;
    }

    /* Demuxer */
    Mp4DemuxerCtx *ctx = static_cast<Mp4DemuxerCtx*>(mp4_handle);
    file_info->track_num = (uint32_t)ctx->tracks.size();
    if (ctx->format_ctx->duration != AV_NOPTS_VALUE && ctx->format_ctx->duration > 0) {
        /* format_ctx->duration is in AV_TIME_BASE units (microseconds) */
        file_info->duration = (uint64_t)ctx->format_ctx->duration;
    } else {
        file_info->duration = 0;
    }
    return 0;
}

int kd_mp4_get_track_by_index(KD_HANDLE mp4_handle, uint32_t index, k_mp4_track_info_s *mp4_track_info)
{
    if (!mp4_handle || !mp4_track_info) return -1;

    /* Demuxer: return cached track info */
    Mp4DemuxerCtx *ctx = static_cast<Mp4DemuxerCtx*>(mp4_handle);
    if (!ctx->tracks.empty()) {
        if (index >= ctx->tracks.size()) return -1;
        *mp4_track_info = ctx->tracks[index].track_info;
        return 0;
    }

    /* Muxer fallback: index 0 = video track */
    Mp4MuxerCtx *mctx = static_cast<Mp4MuxerCtx*>(mp4_handle);
    if (index == 0 && mctx->video_track.stream) {
        *mp4_track_info = mctx->video_track.track_info;
        return 0;
    }
    return -1;
}

int kd_mp4_get_frame(KD_HANDLE mp4_handle, k_mp4_frame_data_s *frame_data)
{
    if (!mp4_handle || !frame_data) return -1;

    Mp4DemuxerCtx *ctx = static_cast<Mp4DemuxerCtx*>(mp4_handle);

    frame_data->eof = 0;
    frame_data->data = nullptr;
    frame_data->data_length = 0;
    frame_data->time_stamp = 0;
    frame_data->codec_id = K_MP4_CODEC_ID_BUTT;

    if (ctx->eof_reached) {
        frame_data->eof = 1;
        return 0;
    }

    AVPacket *pkt = av_packet_alloc();
    if (!pkt) return -1;

    int ret = av_read_frame(ctx->format_ctx, pkt);
    if (ret == AVERROR_EOF) {
        av_packet_free(&pkt);
        ctx->eof_reached = true;
        frame_data->eof = 1;
        return 0;
    }
    if (ret < 0) {
        std::cerr << "[mp4_format] get_frame read failed: " << ff_error_str(ret) << std::endl;
        av_packet_free(&pkt);
        return -1;
    }

    Mp4DemuxerCtx::DemuxTrack *dt = ctx->find_track(pkt->stream_index);
    if (!dt) {
        av_packet_free(&pkt);
        return -1;
    }

    AVStream *st = ctx->format_ctx->streams[pkt->stream_index];

    /* Save PTS/DTS before sending to BSF — av_bsf_send_packet() takes ownership
     * of the packet and the original pkt->pts/dts become invalid afterwards.
     * We also need the stream_index to find the right stream time_base. */
    int64_t pts = pkt->pts;
    int64_t dts = pkt->dts;

    if (dt->track_type == K_MP4_STREAM_VIDEO) {
        /* Use the pre-initialized bitstream filter to convert AVCC/HVCC -> Annex B.
         * The BSF replaces 4-byte length prefixes with start codes and prepends
         * SPS/PPS/VPS before keyframes automatically. */
        AVBSFContext *bsf = (dt->codec_id == K_MP4_CODEC_ID_H265)
                            ? ctx->hevc_bsf_ctx : ctx->h264_bsf_ctx;
        if (bsf) {
            int ret2 = bsf_filter_to_annexb(bsf, pkt, ctx);
            if (ret2 < 0) {
                av_packet_free(&pkt);
                return -1;
            }
            frame_data->data = ctx->frame_buffer.data();
            frame_data->data_length = (uint32_t)ctx->frame_buffer.size();
        } else {
            /* No BSF available (shouldn't happen for valid video streams) —
             * fall back to raw data pass-through */
            ctx->frame_buffer.assign(pkt->data, pkt->data + pkt->size);
            frame_data->data = ctx->frame_buffer.data();
            frame_data->data_length = (uint32_t)ctx->frame_buffer.size();
        }
    } else {
        /* Audio (G.711/Opus): raw pass-through. The data is valid only while
         * the packet is alive, so copy it into frame_buffer for stable lifetime. */
        ctx->frame_buffer.assign(pkt->data, pkt->data + pkt->size);
        frame_data->data = ctx->frame_buffer.data();
        frame_data->data_length = (uint32_t)ctx->frame_buffer.size();
    }

    /* time_stamp is always microseconds (matches kd_mp4_write_frame()'s input
     * unit), rescaled from the stream's own time_base -- 1/track_info.time_scale
     * for video, 1/sample_rate for audio. Use the pts/dts saved before BSF
     * took ownership of the packet. */
    if (pts != AV_NOPTS_VALUE) {
        frame_data->time_stamp = (uint64_t)av_rescale_q(pts, st->time_base, av_make_q(1, 1000000));
    } else if (dts != AV_NOPTS_VALUE) {
        frame_data->time_stamp = (uint64_t)av_rescale_q(dts, st->time_base, av_make_q(1, 1000000));
    } else {
        frame_data->time_stamp = 0;
    }

    frame_data->codec_id = dt->codec_id;

    av_packet_free(&pkt);
    return 0;
}
