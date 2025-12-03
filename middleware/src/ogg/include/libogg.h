#ifndef LIBOGG_H
#define LIBOGG_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/** * Ogg/Opus encapsulation context (implementation hidden) */
typedef void* kd_ogg_muxer;

/** * Ogg/Opus parsing context (implementation hidden) */
typedef void* kd_ogg_demuxer;

/** * Encoder initialization parameters */
typedef struct {
    char filename[128];
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t serial_no;
} kd_ogg_muxer_params;

/** * Decoder initialization parameters */
typedef struct {
    char filename[128];
    uint32_t sample_rate;
    uint32_t channels;
} kd_ogg_demuxer_params;

/** * Frame write parameters */
typedef struct {
    uint8_t *data;
    uint32_t len;
    uint32_t frame_samples;
} kd_ogg_frame_params;

//Initialize Ogg muxer (automatically handles Opus header) 
int kd_ogg_muxer_init(kd_ogg_muxer *ogg_muxer,  kd_ogg_muxer_params *params);

// Write raw Opus frame data
int kd_ogg_write_frame(kd_ogg_muxer ogg_muxer,  kd_ogg_frame_params *params);

//Destroy Ogg muxer and release resources
int kd_ogg_muxer_destroy(kd_ogg_muxer ogg_muxer);

//Initialize Ogg demuxer (automatically parses Opus headers)
int kd_ogg_demuxer_init(kd_ogg_demuxer *ogg_demuxer,  kd_ogg_demuxer_params *params);

// Read an Opus frame 
int kd_ogg_read_frame(kd_ogg_demuxer ogg_demuxer, uint8_t *data, int *len);

//Destroy Ogg decoder and release resources
int kd_ogg_demuxer_destroy(kd_ogg_demuxer ogg_demuxer);

#ifdef __cplusplus
}
#endif

#endif // LIBOGG_H