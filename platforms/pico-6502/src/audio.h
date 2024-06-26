#ifndef AUDIO_H_FILE
#define AUDIO_H_FILE

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(OLIMEX_NEO6502) || defined(OLIMEX_RP2040PC)
#define _AUDIO_PIN (20)
#else
#define _AUDIO_PIN (8)
#endif

void audio_init(uint8_t audio_pin, uint16_t sample_freq);
void audio_push_sample(const uint8_t sample);

#ifdef __cplusplus
}
#endif

#endif  // AUDIO_H_FILE
