#include <string.h>
#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"
#include "hardware/clocks.h"

#include "audio.h"

#define REPETITION_RATE 4

static uint32_t single_sample = 0;
static uint32_t *single_sample_ptr = &single_sample;
static int pwm_dma_chan, trigger_dma_chan, sample_dma_chan;

static uint8_t audio_buffers[2][AUDIO_BUFFER_SIZE];
static volatile int cur_audio_buffer;
static volatile int last_audio_buffer;

struct MIXER_SOURCE {
    const unsigned char *samples;
    int len;
    int loop_start;
    int pos;
    bool active;
    bool loop;
    uint16_t volume;  // 8.8 fixed point
};

static struct MIXER_SOURCE mixer_sources[AUDIO_MAX_SOURCES];
static int16_t mixer_buffer[AUDIO_BUFFER_SIZE];

static void __isr __time_critical_func(dma_handler)() {
    cur_audio_buffer = 1 - cur_audio_buffer;
    dma_hw->ch[sample_dma_chan].al1_read_addr = (intptr_t)&audio_buffers[cur_audio_buffer][0];
    dma_hw->ch[trigger_dma_chan].al3_read_addr_trig = (intptr_t)&single_sample_ptr;

    dma_hw->ints1 = 1u << trigger_dma_chan;
}

void audio_init(int audio_pin, int sample_freq) {
    gpio_set_function(audio_pin, GPIO_FUNC_PWM);

    int audio_pin_slice = pwm_gpio_to_slice_num(audio_pin);
    int audio_pin_chan = pwm_gpio_to_channel(audio_pin);

    uint f_clk_sys = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
    float clock_div = ((float)f_clk_sys * 1000.0f) / 254.0f / (float)sample_freq / (float)REPETITION_RATE;

    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_div);
    pwm_config_set_wrap(&config, 254);
    pwm_init(audio_pin_slice, &config, true);

    pwm_dma_chan = dma_claim_unused_channel(true);
    trigger_dma_chan = dma_claim_unused_channel(true);
    sample_dma_chan = dma_claim_unused_channel(true);

    // setup PWM DMA channel
    dma_channel_config pwm_dma_chan_config = dma_channel_get_default_config(pwm_dma_chan);
    channel_config_set_transfer_data_size(&pwm_dma_chan_config, DMA_SIZE_32);  // transfer 32 bits at a time
    channel_config_set_read_increment(&pwm_dma_chan_config, false);            // always read from the same address
    channel_config_set_write_increment(&pwm_dma_chan_config, false);           // always write to the same address
    channel_config_set_chain_to(&pwm_dma_chan_config, sample_dma_chan);        // trigger sample DMA channel when done
    channel_config_set_dreq(&pwm_dma_chan_config, DREQ_PWM_WRAP0 + audio_pin_slice);  // transfer on PWM cycle end
    dma_channel_configure(pwm_dma_chan, &pwm_dma_chan_config,
                          &pwm_hw->slice[audio_pin_slice].cc,  // write to PWM slice CC register
                          &single_sample,                      // read from single_sample
                          REPETITION_RATE,                     // transfer once per desired sample repetition
                          false                                // don't start yet
    );

    // setup trigger DMA channel
    dma_channel_config trigger_dma_chan_config = dma_channel_get_default_config(trigger_dma_chan);
    channel_config_set_transfer_data_size(&trigger_dma_chan_config, DMA_SIZE_32);  // transfer 32-bits at a time
    channel_config_set_read_increment(&trigger_dma_chan_config, false);            // always read from the same address
    channel_config_set_write_increment(&trigger_dma_chan_config, false);           // always write to the same address
    channel_config_set_dreq(&trigger_dma_chan_config, DREQ_PWM_WRAP0 + audio_pin_slice);  // transfer on PWM cycle end
    dma_channel_configure(
        trigger_dma_chan, &trigger_dma_chan_config,
        &dma_hw->ch[pwm_dma_chan].al3_read_addr_trig,  // write to PWM DMA channel read address trigger
        &single_sample_ptr,                            // read from location containing the address of single_sample
        REPETITION_RATE * AUDIO_BUFFER_SIZE,           // trigger once per audio sample per repetition rate
        false                                          // don't start yet
    );
    dma_channel_set_irq1_enabled(trigger_dma_chan, true);  // fire interrupt when trigger DMA channel is done
    irq_set_exclusive_handler(DMA_IRQ_1, dma_handler);
    irq_set_enabled(DMA_IRQ_1, true);

    // setup sample DMA channel
    dma_channel_config sample_dma_chan_config = dma_channel_get_default_config(sample_dma_chan);
    channel_config_set_transfer_data_size(&sample_dma_chan_config, DMA_SIZE_8);  // transfer 8-bits at a time
    channel_config_set_read_increment(&sample_dma_chan_config,
                                      true);  // increment read address to go through audio buffer
    channel_config_set_write_increment(&sample_dma_chan_config, false);  // always write to the same address
    dma_channel_configure(sample_dma_chan, &sample_dma_chan_config,
                          (char *)&single_sample + 2 * audio_pin_chan,  // write to single_sample
                          &audio_buffers[0][0],                         // read from audio buffer
                          1,     // only do one transfer (once per PWM DMA completion due to chaining)
                          false  // don't start yet
    );

    // clear audio buffers
    memset(audio_buffers[0], 128, AUDIO_BUFFER_SIZE);
    memset(audio_buffers[1], 128, AUDIO_BUFFER_SIZE);

    // kick things off with the trigger DMA channel
    dma_channel_start(trigger_dma_chan);
}

uint8_t *audio_get_buffer(void) {
    if (last_audio_buffer == cur_audio_buffer) {
        return NULL;
    }

    uint8_t *buf = audio_buffers[last_audio_buffer];
    last_audio_buffer = cur_audio_buffer;
    return buf;
}

static int audio_claim_unused_source(void) {
    for (int i = 0; i < AUDIO_MAX_SOURCES; i++) {
        if (!mixer_sources[i].active) {
            mixer_sources[i].active = true;
            return i;
        }
    }
    return -1;
}

int audio_play_once(const uint8_t *samples, int len) {
    int source_id = audio_claim_unused_source();
    if (source_id < 0) {
        return -1;
    }
    struct MIXER_SOURCE *source = &mixer_sources[source_id];
    source->samples = samples;
    source->len = len;
    source->pos = 0;
    source->loop = false;
    source->volume = 256;
    return source_id;
}

int audio_play_loop(const uint8_t *samples, int len, int loop_start) {
    int source_id = audio_play_once(samples, len);
    if (source_id < 0) {
        return -1;
    }
    struct MIXER_SOURCE *source = &mixer_sources[source_id];
    source->loop = true;
    source->loop_start = loop_start;
    return source_id;
}

void audio_source_stop(int source_id) { mixer_sources[source_id].active = false; }

void audio_source_set_volume(int source_id, uint16_t volume) { mixer_sources[source_id].volume = volume; }

extern void copy_audiobuf(uint8_t *dest, const uint8_t *src);

void audio_mixer_step(void) {
    uint8_t *audiobuf = audio_get_buffer();
    if (!audiobuf) return;

    struct MIXER_SOURCE *source = &mixer_sources[0];
    if (!source->active) return;

    copy_audiobuf(audiobuf, source->samples + source->pos);
    source->pos += AUDIO_BUFFER_SIZE;
    // handle source termination
    if (source->pos == source->len) {
        if (source->loop) {
            source->pos = source->loop_start;
        } else {
            source->active = false;
        }
    }

    // // setup 16-bit mixer buffer
    // memset(mixer_buffer, 0, sizeof(mixer_buffer));

    // // mix to 16-bit mixer buffer
    // for (int i = 0; i < AUDIO_MAX_SOURCES; i++) {
    //   if (! mixer_sources[i].active) continue;
    //   struct MIXER_SOURCE *source = &mixer_sources[i];

    //   // mix source
    //   int mix_len = source->len - source->pos;
    //   if (mix_len > AUDIO_BUFFER_SIZE) {
    //     mix_len = AUDIO_BUFFER_SIZE;
    //   }
    //   for (int i = 0; i < mix_len; i++) {
    //     mixer_buffer[i] += ((source->samples[i+source->pos]-128) * source->volume) >> 8;
    //   }
    //   source->pos += mix_len;

    //   // handle source termination
    //   if (source->pos == source->len) {
    //     if (source->loop) {
    //       source->pos = source->loop_start;
    //     } else {
    //       source->active = false;
    //     }
    //   }
    // }

    // // convert 16-bit mixer buffer to 8-bit output buffer
    // for (int i = 0; i < AUDIO_BUFFER_SIZE; i++) {
    //   int sample = mixer_buffer[i] + 128;
    //   if (sample < 0) {
    //     audio_buffer[i] = 0;
    //   } else if (sample > 255) {
    //     audio_buffer[i] = 255;
    //   } else {
    //     audio_buffer[i] = sample;
    //   }
    // }
}
