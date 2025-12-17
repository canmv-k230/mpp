#ifndef LIBOGG_H
#define LIBOGG_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* kd_ogg_muxer;
typedef void* kd_ogg_demuxer;

// Callback type for writing Ogg data (used in stream mode)
typedef int (*kd_ogg_write_callback)(const void *ptr, size_t size, void *user_data);

// Callback type for receiving decoded frames
typedef void (*kd_ogg_frame_callback)(const uint8_t* data, size_t len, void* user_data);

// Parameters for initializing Ogg muxer
typedef struct {
    char filename[128];      // Filename for file mode (empty for stream mode)
    uint32_t sample_rate;    // Audio sample rate
    uint32_t channels;       // Number of audio channels
    uint32_t serial_no;      // Stream serial number (0 for auto-generate)
    kd_ogg_write_callback write_cb;  // Callback for stream mode
    void *user_data;         // User data passed to write callback
} kd_ogg_muxer_params;

// Parameters for writing an Ogg frame
typedef struct {
    const uint8_t *data;     // Frame data buffer
    uint32_t len;            // Length of frame data
    uint32_t frame_samples;  // Number of samples in this frame
} kd_ogg_frame_params;

// Parameters for writing an Ogg frame
typedef struct {
    const uint8_t *data;     // Frame data buffer
    uint32_t len;            // Length of frame data
    uint32_t frame_samples;  // Number of samples in this frame
    uint8_t *out_page;
    uint32_t *out_page_size;
} kd_ogg_frame_params_ex;

// Parameters for writing an Ogg frame
typedef struct {
    const uint8_t *page_data;
    uint32_t page_size;
    uint8_t *out_frame;
    uint32_t *out_frame_size;
} kd_ogg_page_params_ex;

// Parameters for initializing Ogg demuxer
typedef struct {
    // Input parameters
    char filename[128];                  // Filename for file mode (empty for stream mode)
    kd_ogg_frame_callback frame_cb;      // Callback for received frames
    void *user_data;                     // User data passed to frame callback

    // Output parameters (filled by demuxer)
    uint32_t sample_rate;                // Detected sample rate
    uint32_t channels;                   // Detected number of channels
} kd_ogg_demuxer_params;

// Muxer API functions
int kd_ogg_muxer_init(kd_ogg_muxer *ogg_muxer, kd_ogg_muxer_params *params);
int kd_ogg_write_frame(kd_ogg_muxer ogg_muxer, kd_ogg_frame_params *frame);
int kd_ogg_write_frame_ex(kd_ogg_muxer ogg_muxer, kd_ogg_frame_params_ex *frame);
int kd_ogg_muxer_destroy(kd_ogg_muxer ogg_muxer);

// Demuxer API functions
int kd_ogg_demuxer_init(kd_ogg_demuxer *ogg_demuxer, kd_ogg_demuxer_params *params);
int kd_ogg_demuxer_feed_page(kd_ogg_demuxer ogg_demuxer, const uint8_t *page_data, size_t page_size);
int kd_ogg_demuxer_feed_page_ex(kd_ogg_demuxer ogg_demuxer,kd_ogg_page_params_ex *page);
int kd_ogg_demuxer_destroy(kd_ogg_demuxer ogg_demuxer);

#ifdef __cplusplus
}
#endif

#endif // LIBOGG_H