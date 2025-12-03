#include "libogg.h"
#include "ogg.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

// Internal structure definitions
typedef struct {
    FILE *fp;
    ogg_stream_state os;
    ogg_page og;
    ogg_packet op;
    int serial_no;
    bool is_initialized;
    bool has_written_headers;
    ogg_int64_t granulepos; // Accumulated raw PCM sample count (e.g., at 8kHz)
    int sample_rate;        // Original sample rate (e.g., 8000)
    int channels;
    int pre_skip;           // Pre-skip in original sample rate (typically 312)
} kd_ogg_encoder_internal;

typedef struct {
    FILE *fp;
    ogg_sync_state oy;
    ogg_stream_state os;
    ogg_page og;
    ogg_packet op;
    int serial_no;
    bool is_initialized;
    bool is_eos;
    uint32_t sample_rate;
    uint16_t channels;
} kd_ogg_decoder_internal;

// Helper: convert raw samples to 48kHz-equivalent granulepos
static ogg_int64_t to_granulepos_48k(ogg_int64_t raw_samples, int sample_rate) {
    return (ogg_int64_t)(raw_samples) * 48000 / sample_rate;
}

// Generate OpusHead packet
static void generate_opus_header(uint8_t *header, uint32_t sample_rate, uint16_t channels) {
    memcpy(header, "OpusHead", 8);
    header[8] = 1; // version
    header[9] = channels;
    header[10] = 0x38; // 312 & 0xFF
    header[11] = 0x01; // (312 >> 8) & 0xFF
    // Sample rate (32-bit little-endian)
    header[12] = (sample_rate >> 0) & 0xFF;
    header[13] = (sample_rate >> 8) & 0xFF;
    header[14] = (sample_rate >> 16) & 0xFF;
    header[15] = (sample_rate >> 24) & 0xFF;
    header[16] = 0; // output gain
    header[17] = 0; // channel mapping family
    header[18] = 0;
}

static int generate_opus_comment(uint8_t *comment, const char *vendor) {
    if (!comment || !vendor) return 0;
    memcpy(comment, "OpusTags", 8);
    uint8_t *ptr = comment + 8;
    size_t vendor_len = strlen(vendor);
    ptr[0] = (vendor_len >> 0) & 0xFF;
    ptr[1] = (vendor_len >> 8) & 0xFF;
    ptr[2] = (vendor_len >> 16) & 0xFF;
    ptr[3] = (vendor_len >> 24) & 0xFF;
    ptr += 4;
    memcpy(ptr, vendor, vendor_len);
    ptr += vendor_len;
    memset(ptr, 0, 4); // user comment count = 0
    ptr += 4;
    return ptr - comment;
}

int kd_ogg_muxer_init(kd_ogg_muxer *ogg_muxer, kd_ogg_muxer_params *params) {
    if (!ogg_muxer || !params || !params->filename || params->sample_rate == 0 || params->channels == 0) {
        return -1;
    }
    kd_ogg_encoder_internal *encoder = (kd_ogg_encoder_internal*)calloc(1, sizeof(kd_ogg_encoder_internal));
    if (!encoder) {
        return -2;
    }
    encoder->fp = fopen(params->filename, "wb");
    if (!encoder->fp) {
        free(encoder);
        return -3;
    }
    int serial_no = params->serial_no;
    if (serial_no == 0) {
        srand((unsigned int)time(NULL));
        serial_no = rand();
    }
    if (ogg_stream_init(&encoder->os, serial_no) != 0) {
        fclose(encoder->fp);
        free(encoder);
        return -4;
    }
    encoder->serial_no = serial_no;
    encoder->sample_rate = params->sample_rate;
    encoder->channels = params->channels;
    encoder->pre_skip = 312;
    encoder->granulepos = 0;
    encoder->is_initialized = true;

    // Generate headers
    uint8_t opus_header[19];
    uint8_t opus_comment[1024];
    generate_opus_header(opus_header, params->sample_rate, params->channels);
    int comment_len = generate_opus_comment(opus_comment, "kd_ogg_encoder");
    ogg_int64_t pre_skip_48k = to_granulepos_48k(encoder->pre_skip, params->sample_rate);

    // BOS header packet
    encoder->op.packet = opus_header;
    encoder->op.bytes = 19;
    encoder->op.b_o_s = 1;
    encoder->op.e_o_s = 0;
    encoder->op.granulepos = pre_skip_48k;
    encoder->op.packetno = 0;
    if (ogg_stream_packetin(&encoder->os, &encoder->op) != 0) {
        goto init_fail;
    }

    // Comment packet
    encoder->op.packet = opus_comment;
    encoder->op.bytes = comment_len;
    encoder->op.b_o_s = 0;
    encoder->op.e_o_s = 0;
    encoder->op.granulepos = pre_skip_48k;
    encoder->op.packetno = 1;
    if (ogg_stream_packetin(&encoder->os, &encoder->op) != 0) {
        goto init_fail;
    }

    // Flush headers
    while (ogg_stream_flush(&encoder->os, &encoder->og)) {
        fwrite(encoder->og.header, 1, encoder->og.header_len, encoder->fp);
        fwrite(encoder->og.body, 1, encoder->og.body_len, encoder->fp);
    }
    encoder->has_written_headers = true;
    *ogg_muxer = (kd_ogg_muxer)encoder;
    return 0;

init_fail:
    ogg_stream_clear(&encoder->os);
    fclose(encoder->fp);
    free(encoder);
    return -5;
}

int kd_ogg_write_frame(kd_ogg_muxer ogg_muxer, kd_ogg_frame_params *params) {
    kd_ogg_encoder_internal *encoder = (kd_ogg_encoder_internal*)ogg_muxer;
    if (!encoder || !encoder->is_initialized || !encoder->has_written_headers ||
        !params || !params->data || params->len <= 0 || params->frame_samples <= 0) {
        return -1;
    }
    encoder->granulepos += params->frame_samples;
    ogg_int64_t granulepos_48k = to_granulepos_48k(encoder->granulepos, encoder->sample_rate);
    ogg_int64_t pre_skip_48k = to_granulepos_48k(encoder->pre_skip, encoder->sample_rate);
    ogg_int64_t packet_granulepos = granulepos_48k + pre_skip_48k;

    encoder->op.packet = (uint8_t *)params->data;
    encoder->op.bytes = params->len;
    encoder->op.b_o_s = 0;
    encoder->op.e_o_s = 0;
    encoder->op.granulepos = packet_granulepos;
    encoder->op.packetno++;

    if (ogg_stream_packetin(&encoder->os, &encoder->op) != 0) {
        return -2;
    }

    while (ogg_stream_flush(&encoder->os, &encoder->og)) {
        fwrite(encoder->og.header, 1, encoder->og.header_len, encoder->fp);
        fwrite(encoder->og.body, 1, encoder->og.body_len, encoder->fp);
    }
    fflush(encoder->fp);
    return 0;
}

int kd_ogg_muxer_destroy(kd_ogg_muxer ogg_muxer) {
    kd_ogg_encoder_internal *encoder = (kd_ogg_encoder_internal*)ogg_muxer;
    if (!encoder || !encoder->is_initialized) return -1;

    while (ogg_stream_flush(&encoder->os, &encoder->og)) {
        if (encoder->fp) {
            fwrite(encoder->og.header, 1, encoder->og.header_len, encoder->fp);
            fwrite(encoder->og.body, 1, encoder->og.body_len, encoder->fp);
        }
    }

    if (encoder->has_written_headers) {
        ogg_int64_t final_granule_48k = to_granulepos_48k(encoder->granulepos, encoder->sample_rate);
        ogg_int64_t pre_skip_48k = to_granulepos_48k(encoder->pre_skip, encoder->sample_rate);
        encoder->op.packet = NULL;
        encoder->op.bytes = 0;
        encoder->op.b_o_s = 0;
        encoder->op.e_o_s = 1;
        encoder->op.granulepos = final_granule_48k + pre_skip_48k;
        encoder->op.packetno++;
        if (ogg_stream_packetin(&encoder->os, &encoder->op) == 0) {
            while (ogg_stream_flush(&encoder->os, &encoder->og)) {
                if (encoder->fp) {
                    fwrite(encoder->og.header, 1, encoder->og.header_len, encoder->fp);
                    fwrite(encoder->og.body, 1, encoder->og.body_len, encoder->fp);
                }
            }
        }
    }

    ogg_stream_clear(&encoder->os);
    if (encoder->fp) {
        fclose(encoder->fp);
        encoder->fp = NULL;
    }
    free(encoder);
    return 0;
}

// ========== Decoder implementation ==========

int kd_ogg_demuxer_init(kd_ogg_demuxer *ogg_demuxer, kd_ogg_demuxer_params *params) {
    if (!ogg_demuxer || !params || !params->filename || !params->sample_rate || !params->channels) {
        return -1;
    }
    kd_ogg_decoder_internal *decoder = (kd_ogg_decoder_internal*)calloc(1, sizeof(kd_ogg_decoder_internal));
    if (!decoder) {
        return -2;
    }
    decoder->fp = fopen(params->filename, "rb");
    if (!decoder->fp) {
        free(decoder);
        return -3;
    }
    if (ogg_sync_init(&decoder->oy) != 0) {
        fclose(decoder->fp);
        free(decoder);
        return -4;
    }
    decoder->is_initialized = true;
    decoder->is_eos = false;

    uint8_t opus_header[19];
    int header_count = 0;
    int res;
    bool bos_found = false;

    while (1) {
        char *buffer = ogg_sync_buffer(&decoder->oy, 4096);
        if (!buffer) goto decode_fail;

        int bytes_read = fread(buffer, 1, 4096, decoder->fp);
        if (bytes_read < 0) goto decode_fail;
        if (ogg_sync_wrote(&decoder->oy, bytes_read) != 0) goto decode_fail;

        while (1) {
            res = ogg_sync_pageout(&decoder->oy, &decoder->og);
            if (res == 0) break;
            if (res < 0) {
                fprintf(stderr, "Warning: Lost sync while decoding Ogg page.\n");
                continue;
            }

            if (!bos_found) {
                if (!ogg_page_bos(&decoder->og)) {
                    fprintf(stderr, "Error: Expected BOS page not found.\n");
                    goto decode_fail;
                }
                bos_found = true;
                decoder->serial_no = ogg_page_serialno(&decoder->og);
                if (ogg_stream_init(&decoder->os, decoder->serial_no) != 0) goto decode_fail;
            } else {
                if (ogg_page_serialno(&decoder->og) != decoder->serial_no) {
                    continue;
                }
            }

            if (ogg_stream_pagein(&decoder->os, &decoder->og) != 0) {
                fprintf(stderr, "Warning: Page did not sync with stream.\n");
                continue;
            }

            while ((res = ogg_stream_packetout(&decoder->os, &decoder->op)) == 1) {
                if (header_count == 0) {
                    if (decoder->op.bytes < 19 || memcmp(decoder->op.packet, "OpusHead", 8) != 0) {
                        fprintf(stderr, "Error: Invalid or missing OpusHead packet.\n");
                        goto decode_fail;
                    }
                    memcpy(opus_header, decoder->op.packet, (decoder->op.bytes < 19 ? decoder->op.bytes : 19));
                    if (opus_header[8] != 1) {
                        fprintf(stderr, "Error: Unsupported Opus version (%d).\n", opus_header[8]);
                        goto decode_fail;
                    }
                    params->sample_rate = (opus_header[12] << 0) | (opus_header[13] << 8) |
                                          (opus_header[14] << 16) | (opus_header[15] << 24);
                    params->channels = opus_header[9];
                    decoder->sample_rate = params->sample_rate;
                    decoder->channels = params->channels;
                    header_count++;
                } else if (header_count == 1) {
                    if (decoder->op.bytes < 8 || memcmp(decoder->op.packet, "OpusTags", 8) != 0) {
                        fprintf(stderr, "Error: Invalid or missing OpusTags packet.\n");
                        goto decode_fail;
                    }
                    header_count++;
                    *ogg_demuxer = (kd_ogg_demuxer)decoder;
                    return 0;
                } else {
                    fprintf(stderr, "Error: Unexpected packet found after Opus headers.\n");
                    goto decode_fail;
                }
            }

            if (res < 0) {
                fprintf(stderr, "Warning: ogg_stream_packetout reported stream error (%d).\n", res);
            }
        }

        if (bytes_read == 0) {
            if (header_count < 2) {
                fprintf(stderr, "Error: Reached end of file before reading all Opus headers.\n");
                goto decode_fail;
            } else {
                *ogg_demuxer = (kd_ogg_demuxer)decoder;
                return 0;
            }
        }
    }

decode_fail:
    kd_ogg_demuxer_destroy((kd_ogg_demuxer)decoder);
    return -5;
}

int kd_ogg_read_frame(kd_ogg_demuxer ogg_demuxer, uint8_t *data, int *len) {
    kd_ogg_decoder_internal *decoder = (kd_ogg_decoder_internal*)ogg_demuxer;
    if (!decoder || !decoder->is_initialized || !data || !len || *len <= 0) {
        return -1;
    }
    if (decoder->is_eos) {
        return 1;
    }

    int res;
    while (1) {
        res = ogg_stream_packetout(&decoder->os, &decoder->op);
        if (res == 1) {
            if (decoder->op.e_o_s) {
                decoder->is_eos = true;
                return 1;
            }
            if (decoder->op.bytes > *len) {
                fprintf(stderr, "Error: Output buffer too small for Opus packet (need %ld, have %d)\n",
                        (long)decoder->op.bytes, *len);
                return -2;
            }
            *len = decoder->op.bytes;
            memcpy(data, decoder->op.packet, *len);
            return 0;
        } else if (res == 0) {
            break;
        } else {
            fprintf(stderr, "Warning: ogg_stream_packetout error (%d). Stream may be corrupt.\n", res);
            break;
        }
    }

    while (1) {
        res = ogg_sync_pageout(&decoder->oy, &decoder->og);
        if (res == 1) {
            if (ogg_page_serialno(&decoder->og) != decoder->serial_no) {
                continue;
            }
            if (ogg_stream_pagein(&decoder->os, &decoder->og) != 0) {
                fprintf(stderr, "Warning: Failed to submit page to stream decoder.\n");
            }
            break;
        } else if (res == 0) {
            char *buffer = ogg_sync_buffer(&decoder->oy, 4096);
            if (!buffer) {
                fprintf(stderr, "Error: Failed to get buffer from ogg_sync.\n");
                return -3;
            }
            int bytes_read = fread(buffer, 1, 4096, decoder->fp);
            if (bytes_read <= 0) {
                if (ogg_sync_pageout(&decoder->oy, &decoder->og) == 1) {
                    if (ogg_page_serialno(&decoder->og) == decoder->serial_no) {
                        ogg_stream_pagein(&decoder->os, &decoder->og);
                    }
                }
                decoder->is_eos = true;
                return 1;
            }
            if (ogg_sync_wrote(&decoder->oy, bytes_read) != 0) {
                fprintf(stderr, "Error: Failed to tell ogg_sync how many bytes were written.\n");
                return -4;
            }
        } else {
            fprintf(stderr, "Warning: Lost sync while reading Ogg page in read_frame.\n");
        }
    }

    return -5;
}

int kd_ogg_demuxer_destroy(kd_ogg_demuxer ogg_demuxer) {
    kd_ogg_decoder_internal *decoder = (kd_ogg_decoder_internal*)ogg_demuxer;
    if (!decoder) return -1;
    ogg_sync_clear(&decoder->oy);
    if (decoder->is_initialized) {
        ogg_stream_clear(&decoder->os);
    }
    if (decoder->fp) {
        fclose(decoder->fp);
        decoder->fp = NULL;
    }
    free(decoder);
    return 0;
}