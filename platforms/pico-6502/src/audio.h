#ifndef AUDIO_H_FILE
#define AUDIO_H_FILE

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void audio_init(uint8_t audio_pin, uint16_t sample_freq);
void audio_push_sample(const uint8_t sample);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_H_FILE */
