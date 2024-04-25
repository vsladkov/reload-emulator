#ifndef AUDIO_H_FILE
#define AUDIO_H_FILE

#define AUDIO_BUFFER_SIZE 1024
#define AUDIO_CHUNK_SIZE  64
#define REPETITION_RATE   4

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void audio_init(int audio_pin, int sample_freq);

void audio_push_sample(const uint8_t sample);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_H_FILE */
