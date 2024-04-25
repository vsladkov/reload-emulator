#include <string.h>
#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"
#include "hardware/clocks.h"

#include "audio.h"

static uint32_t single_sample = 0;
static uint32_t *single_sample_ptr = &single_sample;
static int pwm_dma_channel, trigger_dma_channel, sample_dma_channel;

typedef struct {
    uint8_t samples[AUDIO_BUFFER_SIZE];
    uint16_t head;
    uint16_t tail;
    uint16_t size;
} audio_buffer_t;

static audio_buffer_t __not_in_flash() audio_buffer;

static inline void __not_in_flash_func(audio_buffer_init)(audio_buffer_t *audio_buffer) {
    memset(audio_buffer->samples, 0, AUDIO_BUFFER_SIZE);
    audio_buffer->head = 0;
    audio_buffer->tail = 0;
    audio_buffer->size = 0;
}

static inline void __not_in_flash_func(audio_buffer_enqueue)(audio_buffer_t *audio_buffer, uint8_t sample) {
    if (audio_buffer->size < AUDIO_BUFFER_SIZE) {
        audio_buffer->samples[audio_buffer->head++] = sample;
        if (audio_buffer->head >= AUDIO_BUFFER_SIZE) {
            audio_buffer->head = 0;
        }
        audio_buffer->size++;
    }
}

static inline uint8_t* __not_in_flash_func(audio_buffer_dequeue)(audio_buffer_t *audio_buffer, uint16_t num_samples) {
    if (audio_buffer->size < num_samples) {
        int p = audio_buffer->head == 0 ? AUDIO_BUFFER_SIZE : audio_buffer->head;
        int c = audio_buffer->samples[p - 1];
        int len = num_samples - audio_buffer->size;
        memset(audio_buffer->samples + audio_buffer->head, c, len);
        audio_buffer->head += len;
        if (audio_buffer->head >= AUDIO_BUFFER_SIZE) {
            audio_buffer->head = 0;
        }
        audio_buffer->size = num_samples;
    }
    uint8_t *samples = &audio_buffer->samples[audio_buffer->tail];
    audio_buffer->tail += num_samples;
    if (audio_buffer->tail >= AUDIO_BUFFER_SIZE) {
        audio_buffer->tail = 0;
    }
    audio_buffer->size -= num_samples;
    return samples;
}

static void __not_in_flash_func(audio_dma_irq_handler)() {
    dma_channel_set_read_addr(sample_dma_channel, audio_buffer_dequeue(&audio_buffer, AUDIO_CHUNK_SIZE), false);
    dma_channel_set_read_addr(trigger_dma_channel, &single_sample_ptr, true);
    dma_channel_acknowledge_irq1(trigger_dma_channel);
}

void audio_init(int audio_pin, int sample_freq) {
    gpio_set_function(audio_pin, GPIO_FUNC_PWM);

    int audio_pin_slice = pwm_gpio_to_slice_num(audio_pin);
    int audio_pin_chan = pwm_gpio_to_channel(audio_pin);

    uint f_clk_sys = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
    float clock_div = ((float)f_clk_sys * 1000.0f) / 255.0f / (float)sample_freq / (float)REPETITION_RATE;

    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_div);
    pwm_config_set_wrap(&config, 255);
    pwm_init(audio_pin_slice, &config, true);

    pwm_dma_channel = dma_claim_unused_channel(true);
    trigger_dma_channel = dma_claim_unused_channel(true);
    sample_dma_channel = dma_claim_unused_channel(true);

    // Setup PWM DMA channel
    dma_channel_config pwm_dma_channel_config = dma_channel_get_default_config(pwm_dma_channel);
    // Transfer 32 bits at a time
    channel_config_set_transfer_data_size(&pwm_dma_channel_config, DMA_SIZE_32);
    // Read from a fixed location, always writes to the same address
    channel_config_set_read_increment(&pwm_dma_channel_config, false);
    channel_config_set_write_increment(&pwm_dma_channel_config, false);
    // Chain to sample DMA channel when done
    channel_config_set_chain_to(&pwm_dma_channel_config, sample_dma_channel);
    // Transfer on PWM cycle end
    channel_config_set_dreq(&pwm_dma_channel_config, DREQ_PWM_WRAP0 + audio_pin_slice);

    dma_channel_configure(pwm_dma_channel, &pwm_dma_channel_config,
                          // Write to PWM slice CC register
                          &pwm_hw->slice[audio_pin_slice].cc,
                          // Read from single_sample
                          &single_sample,
                          // Transfer once per desired sample repetition
                          REPETITION_RATE,
                          // Don't start yet
                          false);

    // Setup trigger DMA channel
    dma_channel_config trigger_dma_channel_config = dma_channel_get_default_config(trigger_dma_channel);
    // Transfer 32-bits at a time
    channel_config_set_transfer_data_size(&trigger_dma_channel_config, DMA_SIZE_32);
    // Always read and write from and to the same address
    channel_config_set_read_increment(&trigger_dma_channel_config, false);
    channel_config_set_write_increment(&trigger_dma_channel_config, false);
    // Transfer on PWM cycle end
    channel_config_set_dreq(&trigger_dma_channel_config, DREQ_PWM_WRAP0 + audio_pin_slice);

    dma_channel_configure(trigger_dma_channel, &trigger_dma_channel_config,
                          // Write to PWM DMA channel read address trigger
                          &dma_hw->ch[pwm_dma_channel].al3_read_addr_trig,
                          // Read from location containing the address of single_sample
                          &single_sample_ptr,
                          // Need to trigger once for each audio sample but as the PWM DREQ is
                          // used need to multiply by repetition rate
                          REPETITION_RATE * AUDIO_CHUNK_SIZE, false);

    // Fire interrupt when trigger DMA channel is done
    dma_channel_set_irq1_enabled(trigger_dma_channel, true);
    irq_set_exclusive_handler(DMA_IRQ_1, audio_dma_irq_handler);
    irq_set_enabled(DMA_IRQ_1, true);

    // Setup sample DMA channel
    dma_channel_config sample_dma_channel_config = dma_channel_get_default_config(sample_dma_channel);
    // Transfer 8-bits at a time
    channel_config_set_transfer_data_size(&sample_dma_channel_config, DMA_SIZE_8);
    // Increment read address to go through audio buffer
    channel_config_set_read_increment(&sample_dma_channel_config, true);
    // Always write to the same address
    channel_config_set_write_increment(&sample_dma_channel_config, false);

    dma_channel_configure(sample_dma_channel, &sample_dma_channel_config,
                          // Write to single_sample
                          (char *)&single_sample + 2 * audio_pin_chan,
                          // Read from audio buffer
                          audio_buffer.samples,
                          // Only do one transfer (once per PWM DMA completion due to chaining)
                          1,
                          // Don't start yet
                          false);

    audio_buffer_init(&audio_buffer);

    // Kick things off with the trigger DMA channel
    dma_channel_start(trigger_dma_channel);
}

void audio_push_sample(const uint8_t sample) { audio_buffer_enqueue(&audio_buffer, sample); }
