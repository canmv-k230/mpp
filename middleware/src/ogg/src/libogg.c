#include "libogg.h"
#include "ogg.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

// Internal encoder structure
typedef struct {
    FILE *fp;                   // File pointer for file mode
    kd_ogg_write_callback write_cb;  // Write callback for stream mode
    void *write_user_data;      // User data for write callback
    bool is_stream_mode;        // True if in stream mode
    ogg_stream_state os;        // Ogg stream state
    ogg_page og;                // Ogg page structure
    ogg_packet op;              // Ogg packet structure
    int serial_no;              // Stream serial number
    bool is_initialized;        // Initialization flag
    bool has_written_headers;   // Headers written flag
    ogg_int64_t granulepos;     // Current granule position
    int sample_rate;            // Audio sample rate
    int channels;               // Number of channels
    int pre_skip;               // Pre-skip samples
} kd_ogg_encoder_internal;

// Internal decoder structure
typedef struct {
    FILE *fp;                    // File pointer for file mode
    bool is_stream_mode;         // True if in stream mode
    ogg_sync_state oy;           // Ogg sync state
    ogg_stream_state os;         // Ogg stream state
    ogg_page og;                 // Ogg page structure
    ogg_packet op;               // Ogg packet structure
    int serial_no;               // Stream serial number (0 = uninitialized)
    bool is_initialized;         // Initialization flag
    bool is_eos;                 // End-of-stream flag
    uint32_t sample_rate;        // Audio sample rate
    uint16_t channels;           // Number of channels
    bool headers_parsed;         // Headers parsed flag
    int header_packet_count;     // Header packet counter (0: none, 1: OpusHead, 2: OpusTags)

    // Frame output callback
    kd_ogg_frame_callback frame_cb;  // Frame callback function
    void *frame_user_data;       // User data for frame callback
} kd_ogg_decoder_internal;

/**
 * Converts raw sample count to 48kHz granule position
 * @param raw_samples Number of raw samples
 * @param sample_rate Original sample rate
 * @return Converted granule position
 */
static ogg_int64_t to_granulepos_48k(ogg_int64_t raw_samples, int sample_rate) {
    return (ogg_int64_t)(raw_samples) * 48000 / sample_rate;
}

/**
 * Generates Opus header packet
 * @param header Buffer to store header (must be at least 19 bytes)
 * @param sample_rate Audio sample rate
 * @param channels Number of channels
 */
static void generate_opus_header(uint8_t *header, uint32_t sample_rate, uint16_t channels) {
    memcpy(header, "OpusHead", 8);
    header[8] = 1;                // Version
    header[9] = channels;         // Channels
    header[10] = 0x38;            // Pre-skip LSB
    header[11] = 0x01;            // Pre-skip MSB
    header[12] = (sample_rate >> 0) & 0xFF;  // Sample rate LSB
    header[13] = (sample_rate >> 8) & 0xFF;
    header[14] = (sample_rate >> 16) & 0xFF;
    header[15] = (sample_rate >> 24) & 0xFF; // Sample rate MSB
    header[16] = 0;               // Output gain LSB
    header[17] = 0;               // Output gain MSB
    header[18] = 0;               // Channel map
}

/**
 * Generates Opus comment packet
 * @param comment Buffer to store comment
 * @param vendor Vendor string
 * @return Length of comment packet
 */
static int generate_opus_comment(uint8_t *comment, const char *vendor) {
    if (!comment || !vendor) return 0;
    memcpy(comment, "OpusTags", 8);
    uint8_t *ptr = comment + 8;
    size_t vendor_len = strlen(vendor);

    // Write vendor length (32-bit little-endian)
    ptr[0] = (vendor_len >> 0) & 0xFF;
    ptr[1] = (vendor_len >> 8) & 0xFF;
    ptr[2] = (vendor_len >> 16) & 0xFF;
    ptr[3] = (vendor_len >> 24) & 0xFF;
    ptr += 4;

    // Write vendor string
    memcpy(ptr, vendor, vendor_len);
    ptr += vendor_len;

    // No additional comments (4 bytes for count = 0)
    memset(ptr, 0, 4);
    ptr += 4;

    return ptr - comment;
}

static unsigned char ogg_page_buf[8000];  // Buffer for Ogg page assembly

/**
 * Writes an Ogg page using either file or stream mode
 * @param enc Encoder instance
 * @param page Ogg page to write
 * @return 0 on success, -1 on failure
 */
static int write_ogg_page(kd_ogg_encoder_internal *enc, ogg_page *page) {
    if (enc->is_stream_mode) {
        if (!enc->write_cb) return -1;
        size_t total_len = page->header_len + page->body_len;
        memcpy(ogg_page_buf, page->header, page->header_len);
        memcpy(ogg_page_buf + page->header_len, page->body, page->body_len);
        enc->write_cb(ogg_page_buf, total_len, enc->write_user_data);
        return 0;
    } else {
        if (!enc->fp) return -1;
        fwrite(page->header, 1, page->header_len, enc->fp);
        fwrite(page->body, 1, page->body_len, enc->fp);
        fflush(enc->fp);
        return 0;
    }
}

/**
 * Initializes Ogg muxer (supports both file and stream modes)
 * @param ogg_muxer Output pointer for muxer instance
 * @param params Muxer parameters
 * @return 0 on success, negative error code on failure
 */
int kd_ogg_muxer_init(kd_ogg_muxer *ogg_muxer, kd_ogg_muxer_params *params) {
    if (!ogg_muxer || !params || params->sample_rate == 0 || params->channels == 0) {
        return -1;
    }

    kd_ogg_encoder_internal *encoder = calloc(1, sizeof(*encoder));
    if (!encoder) return -2;

    // Determine mode (file or stream)
    encoder->is_stream_mode = (params->filename[0] == '\0');
    encoder->write_cb = params->write_cb;
    encoder->write_user_data = params->user_data;

    // Initialize file mode
    if (!encoder->is_stream_mode) {
        encoder->fp = fopen(params->filename, "wb");
        if (!encoder->fp) {
            free(encoder);
            return -3;
        }
    }

    // Initialize stream with serial number
    int serial_no = params->serial_no ? params->serial_no : rand();
    if (serial_no <= 0) {
        serial_no = 0xff;
    }
    printf("%s serial_no:%d\n", __func__, serial_no);

    if (ogg_stream_init(&encoder->os, serial_no) != 0) {
        if (!encoder->is_stream_mode && encoder->fp) fclose(encoder->fp);
        free(encoder);
        return -4;
    }

    // Initialize encoder parameters
    encoder->serial_no = serial_no;
    encoder->sample_rate = params->sample_rate;
    encoder->channels = params->channels;
    encoder->pre_skip = 312;
    encoder->granulepos = 0;
    encoder->is_initialized = true;

    // Generate and write headers
    uint8_t opus_header[19], opus_comment[1024];
    generate_opus_header(opus_header, params->sample_rate, params->channels);
    int comment_len = generate_opus_comment(opus_comment, "kd_ogg_encoder");
    ogg_int64_t pre_skip_48k = to_granulepos_48k(encoder->pre_skip, params->sample_rate);

    // Write OpusHead packet
    encoder->op.packet = opus_header;
    encoder->op.bytes = 19;
    encoder->op.b_o_s = 1;
    encoder->op.granulepos = pre_skip_48k;
    encoder->op.packetno = 0;
    if (ogg_stream_packetin(&encoder->os, &encoder->op) != 0) goto fail;

    // Write OpusTags packet
    encoder->op.packet = opus_comment;
    encoder->op.bytes = comment_len;
    encoder->op.b_o_s = 0;
    encoder->op.granulepos = pre_skip_48k;
    encoder->op.packetno = 1;
    if (ogg_stream_packetin(&encoder->os, &encoder->op) != 0) goto fail;

    // Flush header pages
    if (encoder->write_cb){
        while (ogg_stream_flush(&encoder->os, &encoder->og)) {
            if (write_ogg_page(encoder, &encoder->og) != 0) goto fail;
        }
    }


    encoder->has_written_headers = true;
    *ogg_muxer = encoder;
    return 0;

fail:
    ogg_stream_clear(&encoder->os);
    if (!encoder->is_stream_mode && encoder->fp) fclose(encoder->fp);
    free(encoder);
    return -5;
}

/**
 * Writes a frame to the Ogg stream
 * @param ogg_muxer Muxer instance
 * @param frame Frame parameters
 * @return 0 on success, negative error code on failure
 */
int kd_ogg_write_frame(kd_ogg_muxer ogg_muxer, kd_ogg_frame_params *frame) {
    if (!frame || !frame->data || frame->len == 0 || frame->frame_samples == 0) {
        return -1;
    }

    kd_ogg_encoder_internal *encoder = (kd_ogg_encoder_internal*)ogg_muxer;
    if (!encoder || !encoder->is_initialized || !encoder->has_written_headers) {
        return -2;
    }

    // Update granule position
    encoder->granulepos += frame->frame_samples;
    ogg_int64_t granulepos_48k = to_granulepos_48k(encoder->granulepos, encoder->sample_rate);
    ogg_int64_t pre_skip_48k = to_granulepos_48k(encoder->pre_skip, encoder->sample_rate);
    ogg_int64_t packet_granulepos = granulepos_48k + pre_skip_48k;

    // Prepare packet
    encoder->op.packet = (uint8_t *)frame->data;
    encoder->op.bytes = frame->len;
    encoder->op.b_o_s = 0;
    encoder->op.e_o_s = 0;
    encoder->op.granulepos = packet_granulepos;
    encoder->op.packetno++;

    // Write packet
    if (ogg_stream_packetin(&encoder->os, &encoder->op) != 0) return -3;
    while (ogg_stream_flush(&encoder->os, &encoder->og)) {
        if (write_ogg_page(encoder, &encoder->og) != 0) return -4;
    }

    return 0;
}

/**
 * Destroys Ogg muxer and cleans up resources
 * @param ogg_muxer Muxer instance
 * @return 0 on success, -1 on failure
 */
int kd_ogg_muxer_destroy(kd_ogg_muxer ogg_muxer) {
    kd_ogg_encoder_internal *encoder = (kd_ogg_encoder_internal*)ogg_muxer;
    if (!encoder || !encoder->is_initialized) return -1;

    // // Flush remaining data
    // while (ogg_stream_flush(&encoder->os, &encoder->og)) {
    //     write_ogg_page(encoder, &encoder->og);
    // }

    // // Write EOS packet if headers were written
    // if (encoder->has_written_headers) {
    //     ogg_int64_t final_granule_48k = to_granulepos_48k(encoder->granulepos, encoder->sample_rate);
    //     ogg_int64_t pre_skip_48k = to_granulepos_48k(encoder->pre_skip, encoder->sample_rate);

    //     encoder->op.packet = NULL;
    //     encoder->op.bytes = 0;
    //     encoder->op.b_o_s = 0;
    //     encoder->op.e_o_s = 1;
    //     encoder->op.granulepos = final_granule_48k + pre_skip_48k;
    //     encoder->op.packetno++;

    //     if (ogg_stream_packetin(&encoder->os, &encoder->op) == 0) {
    //         while (ogg_stream_flush(&encoder->os, &encoder->og)) {
    //             write_ogg_page(encoder, &encoder->og);
    //         }
    //     }
    // }

    // Cleanup
    ogg_stream_clear(&encoder->os);
    if (!encoder->is_stream_mode && encoder->fp) {
        fclose(encoder->fp);
    }
    free(encoder);
    return 0;
}

/**
 * Processes available packets in the decoder stream
 * @param decoder Decoder instance
 */
static void process_available_packets(kd_ogg_decoder_internal *decoder) {
    while (1) {
        int res = ogg_stream_packetout(&decoder->os, &decoder->op);
        if (res != 1) break;

        // Check for end of stream
        if (decoder->op.e_o_s) {
            decoder->is_eos = true;
            break;
        }

        // Process header packets first
        if (!decoder->headers_parsed) {
            if (decoder->header_packet_count == 0) {
                // Validate OpusHead packet
                if (decoder->op.bytes < 19 || memcmp(decoder->op.packet, "OpusHead", 8) != 0) {
                    decoder->is_eos = true; // Treat as error
                    break;
                }
                uint8_t *h = decoder->op.packet;
                decoder->channels = h[9];
                decoder->sample_rate = (h[12] << 0) | (h[13] << 8) |
                                      (h[14] << 16) | (h[15] << 24);
                decoder->header_packet_count = 1;
            } else if (decoder->header_packet_count == 1) {
                // Validate OpusTags packet
                if (decoder->op.bytes < 8 || memcmp(decoder->op.packet, "OpusTags", 8) != 0) {
                    decoder->is_eos = true; // Treat as error
                    break;
                }
                decoder->header_packet_count = 2;
                decoder->headers_parsed = true;
            }
            continue; // Skip output for header packets
        }

        // Pass audio frames to callback
        if (decoder->frame_cb) {
            decoder->frame_cb(decoder->op.packet, decoder->op.bytes, decoder->frame_user_data);
        }
    }
}

/**
 * Initializes Ogg demuxer (supports both file and stream modes)
 * @param ogg_demuxer Output pointer for demuxer instance
 * @param params Demuxer parameters
 * @return 0 on success, negative error code on failure
 */
int kd_ogg_demuxer_init(kd_ogg_demuxer *ogg_demuxer, kd_ogg_demuxer_params *params) {
    if (!ogg_demuxer || !params) {
        return -1;
    }

    kd_ogg_decoder_internal *decoder = calloc(1, sizeof(*decoder));
    if (!decoder) return -2;

    // Initialize decoder parameters
    decoder->frame_cb = params->frame_cb;
    decoder->frame_user_data = params->user_data;
    decoder->is_initialized = true;
    decoder->serial_no = 0; // Mark as uninitialized

    // Determine mode (file or stream)
    bool file_mode = (params->filename[0] != '\0');
    decoder->is_stream_mode = !file_mode;

    if (file_mode) {
        // Open file for reading
        decoder->fp = fopen(params->filename, "rb");
        if (!decoder->fp) {
            free(decoder);
            return -3;
        }

        // Initialize sync state
        if (ogg_sync_init(&decoder->oy) != 0) {
            fclose(decoder->fp);
            free(decoder);
            return -4;
        }

        // Read and process entire file
        while (!decoder->is_eos) {
            char *buffer = ogg_sync_buffer(&decoder->oy, 4096);
            size_t bytes = fread(buffer, 1, 4096, decoder->fp);
            if (bytes == 0) break;
            ogg_sync_wrote(&decoder->oy, bytes);

            // Process pages
            while (ogg_sync_pageout(&decoder->oy, &decoder->og) == 1) {
                ogg_uint32_t serialno = ogg_page_serialno(&decoder->og);

                // Initialize stream if not already done
                if (decoder->serial_no == 0) {
                    if (!ogg_page_bos(&decoder->og)) {
                        printf("First page must be BOS\n");
                        goto fail;
                    }
                    decoder->serial_no = serialno;
                    if (ogg_stream_init(&decoder->os, decoder->serial_no) != 0) {
                        printf("Failed to initialize stream\n");
                        goto fail;
                    }
                }

                // Skip pages from other streams
                if (serialno != (ogg_uint32_t)decoder->serial_no) {
                    continue;
                }

                // Process page
                if (ogg_stream_pagein(&decoder->os, &decoder->og) != 0) {
                    continue;
                }

                process_available_packets(decoder);
            }
        }

        // Fill output parameters
        params->sample_rate = decoder->sample_rate;
        params->channels = decoder->channels;

    } else {
        // Initialize sync state for stream mode
        if (ogg_sync_init(&decoder->oy) != 0) {
            free(decoder);
            return -4;
        }
    }

    *ogg_demuxer = decoder;
    return 0;

fail:
    kd_ogg_demuxer_destroy((kd_ogg_demuxer)decoder);
    return -5;
}

/**
 * Feeds an Ogg page into the demuxer (stream mode only)
 * @param ogg_demuxer Demuxer instance
 * @param page_data Pointer to page data
 * @param page_size Size of page data
 * @return 0 on success, negative error code on failure
 */
int kd_ogg_demuxer_feed_page(kd_ogg_demuxer ogg_demuxer, const uint8_t *page_data, size_t page_size) {
    kd_ogg_decoder_internal *decoder = (kd_ogg_decoder_internal*)ogg_demuxer;
    if (!decoder || !decoder->is_initialized || !decoder->is_stream_mode ||
        !page_data || page_size == 0) {
        return -1;
    }

    // Add page data to sync buffer
    char *sync_buf = ogg_sync_buffer(&decoder->oy, page_size);
    memcpy(sync_buf, page_data, page_size);
    ogg_sync_wrote(&decoder->oy, page_size);

    // Extract and process page
    if (ogg_sync_pageout(&decoder->oy, &decoder->og) != 1) {
        return -2;
    }

    ogg_uint32_t serialno = ogg_page_serialno(&decoder->og);

    // Initialize stream if first page
    if (decoder->serial_no == 0) {
        if (!ogg_page_bos(&decoder->og)) {
            return -3; // First page must be BOS
        }
        decoder->serial_no = serialno;
        if (ogg_stream_init(&decoder->os, decoder->serial_no) != 0) {
            return -4;
        }
    }

    // Skip pages from other streams
    if (serialno != (ogg_uint32_t)decoder->serial_no) {
        return 0;
    }

    // Process page
    if (ogg_stream_pagein(&decoder->os, &decoder->og) != 0) {
        return -5;
    }

    process_available_packets(decoder);
    return 0;
}

/**
 * Destroys Ogg demuxer and cleans up resources
 * @param ogg_demuxer Demuxer instance
 * @return 0 on success, -1 on failure
 */
int kd_ogg_demuxer_destroy(kd_ogg_demuxer ogg_demuxer) {
    kd_ogg_decoder_internal *decoder = (kd_ogg_decoder_internal*)ogg_demuxer;
    if (!decoder) return -1;

    // Cleanup Ogg structures
    ogg_sync_clear(&decoder->oy);
    if (decoder->is_initialized && decoder->serial_no != 0) {
        ogg_stream_clear(&decoder->os);
    }

    // Close file if open
    if (decoder->fp) {
        fclose(decoder->fp);
    }

    free(decoder);
    return 0;
}

int kd_ogg_write_frame_ex(kd_ogg_muxer ogg_muxer, kd_ogg_frame_params_ex *frame) {
    // Parameter validation
    if (!frame || !frame->data || frame->len == 0 || frame->frame_samples == 0 ||
        !frame->out_page || !frame->out_page_size) {
        return -1;
    }

    kd_ogg_encoder_internal *encoder = (kd_ogg_encoder_internal*)ogg_muxer;
    if (!encoder || !encoder->is_initialized || !encoder->has_written_headers) {
        return -2;
    }

    // Update timestamp (same as original logic)
    encoder->granulepos += frame->frame_samples;
    ogg_int64_t granulepos_48k = to_granulepos_48k(encoder->granulepos, encoder->sample_rate);
    ogg_int64_t pre_skip_48k = to_granulepos_48k(encoder->pre_skip, encoder->sample_rate);
    ogg_int64_t packet_granulepos = granulepos_48k + pre_skip_48k;

    // Prepare data packet
    encoder->op.packet = (uint8_t *)frame->data;
    encoder->op.bytes = frame->len;
    encoder->op.b_o_s = 0;
    encoder->op.e_o_s = 0;
    encoder->op.granulepos = packet_granulepos;
    encoder->op.packetno++;

    // Write packet and generate page
    if (ogg_stream_packetin(&encoder->os, &encoder->op) != 0) return -3;

    // Write page data directly to output buffer (instead of callback)
    *frame->out_page_size = 0;
    while (ogg_stream_flush(&encoder->os, &encoder->og)) {
        size_t page_total = encoder->og.header_len + encoder->og.body_len;
        memcpy(frame->out_page + *frame->out_page_size, encoder->og.header, encoder->og.header_len);
        memcpy(frame->out_page + *frame->out_page_size + encoder->og.header_len, encoder->og.body, encoder->og.body_len);
        *frame->out_page_size += page_total;
    }

    return 0;
}

int kd_ogg_demuxer_feed_page_ex(kd_ogg_demuxer ogg_demuxer,kd_ogg_page_params_ex *page) {
    // Parameter validation
    kd_ogg_decoder_internal *decoder = (kd_ogg_decoder_internal*)ogg_demuxer;
    if (!decoder || !decoder->is_initialized || !decoder->is_stream_mode ||
        !page->page_data || page->page_size == 0 || !page->out_frame || !page->out_frame_size) {
        return -1;
    }

    // Reset output
    *page->out_frame_size = 0;

    // Add page data to sync buffer
    char *sync_buf = ogg_sync_buffer(&decoder->oy, page->page_size);
    memcpy(sync_buf, page->page_data, page->page_size);
    ogg_sync_wrote(&decoder->oy, page->page_size);

    // Extract and process page
    if (ogg_sync_pageout(&decoder->oy, &decoder->og) != 1) {
        return -2; // Invalid page data
    }

    ogg_uint32_t serialno = ogg_page_serialno(&decoder->og);

    // Initialize stream (first processing)
    if (decoder->serial_no == 0) {
        if (!ogg_page_bos(&decoder->og)) {
            return -3; // First page must be BOS
        }
        decoder->serial_no = serialno;
        if (ogg_stream_init(&decoder->os, decoder->serial_no) != 0) {
            return -4;
        }
    }

    // Skip pages from other streams
    if (serialno != (ogg_uint32_t)decoder->serial_no) {
        return 0;
    }

    // Process page data
    if (ogg_stream_pagein(&decoder->os, &decoder->og) != 0) {
        return -5;
    }

    // Extract frame data (write directly to output buffer instead of callback)
    while (1) {
        int res = ogg_stream_packetout(&decoder->os, &decoder->op);
        if (res != 1) break;

        // Check end flag
        if (decoder->op.e_o_s) {
            decoder->is_eos = true;
            break;
        }

        // Process headers (skip, do not output)
        if (!decoder->headers_parsed) {
            if (decoder->header_packet_count == 0) {
                if (decoder->op.bytes >= 19 && memcmp(decoder->op.packet, "OpusHead", 8) == 0) {
                    uint8_t *h = decoder->op.packet;
                    decoder->channels = h[9];
                    decoder->sample_rate = (h[12] << 0) | (h[13] << 8) |
                                          (h[14] << 16) | (h[15] << 24);
                    decoder->header_packet_count = 1;
                } else {
                    return -6; // Invalid header
                }
            } else if (decoder->header_packet_count == 1) {
                if (decoder->op.bytes >= 8 && memcmp(decoder->op.packet, "OpusTags", 8) == 0) {
                    decoder->header_packet_count = 2;
                    decoder->headers_parsed = true;
                } else {
                    return -7; // Invalid header
                }
            }
            continue;
        }

        // Output frame data directly to buffer
        if (decoder->op.bytes > 0) {
            memcpy(page->out_frame + *page->out_frame_size, decoder->op.packet, decoder->op.bytes);
            *page->out_frame_size += decoder->op.bytes;
        }
    }

    return 0;
}