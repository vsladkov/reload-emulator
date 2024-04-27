#include <string.h>
#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "pico/critical_section.h"

#include "audio.h"

#define SAMPLES_BUFFER_SIZE    2048
#define SAMPLES_CHUNK_SIZE     32
#define SAMPLE_REPETITION_RATE 4

typedef struct {
    critical_section_t cs;
    uint8_t samples[SAMPLES_BUFFER_SIZE];
    uint8_t empty_samples[SAMPLES_CHUNK_SIZE];
    uint16_t head;
    uint16_t tail;
    uint16_t size;
} audio_buffer_t;

static audio_buffer_t __not_in_flash() audio_buffer;

static uint32_t single_sample = 0;
static uint32_t *single_sample_ptr = &single_sample;
static int pwm_dma_channel, trigger_dma_channel, sample_dma_channel;

static inline void __not_in_flash_func(audio_buffer_init)(audio_buffer_t *audio_buffer) {
    critical_section_init(&audio_buffer->cs);
    memset(audio_buffer->samples, 0, SAMPLES_BUFFER_SIZE);
    memset(audio_buffer->empty_samples, 0, SAMPLES_CHUNK_SIZE);
    audio_buffer->head = 0;
    audio_buffer->tail = 0;
    audio_buffer->size = 0;
}

static inline void __not_in_flash_func(audio_buffer_enqueue)(audio_buffer_t *audio_buffer, uint8_t sample) {
    if (audio_buffer->size < SAMPLES_BUFFER_SIZE) {
        audio_buffer->samples[audio_buffer->head++] = sample;
        if (audio_buffer->head >= SAMPLES_BUFFER_SIZE) {
            audio_buffer->head = 0;
        }
        critical_section_enter_blocking(&audio_buffer->cs);
        audio_buffer->size++;
        critical_section_exit(&audio_buffer->cs);
    }
}

static inline uint8_t *__not_in_flash_func(audio_buffer_dequeue)(audio_buffer_t *audio_buffer) {
    if (audio_buffer->size >= SAMPLES_CHUNK_SIZE) {
        uint8_t *samples = &audio_buffer->samples[audio_buffer->tail];
        audio_buffer->tail += SAMPLES_CHUNK_SIZE;
        if (audio_buffer->tail >= SAMPLES_BUFFER_SIZE) {
            audio_buffer->tail = 0;
        }
        critical_section_enter_blocking(&audio_buffer->cs);
        audio_buffer->size -= SAMPLES_CHUNK_SIZE;
        critical_section_exit(&audio_buffer->cs);
        return samples;
    } else {
        int p = audio_buffer->tail == 0 ? SAMPLES_BUFFER_SIZE : audio_buffer->tail;
        int c = audio_buffer->samples[p - 1];
        memset(audio_buffer->empty_samples, c, SAMPLES_CHUNK_SIZE);
        return audio_buffer->empty_samples;    
    }
}

static void __not_in_flash_func(audio_dma_irq_handler)() {
    dma_channel_set_read_addr(sample_dma_channel, audio_buffer_dequeue(&audio_buffer), false);
    dma_channel_set_read_addr(trigger_dma_channel, &single_sample_ptr, true);
    dma_channel_acknowledge_irq1(trigger_dma_channel);
}

void audio_init(uint8_t audio_pin, uint16_t sample_freq) {
    gpio_set_function(audio_pin, GPIO_FUNC_PWM);

    int audio_pin_slice = pwm_gpio_to_slice_num(audio_pin);
    int audio_pin_channel = pwm_gpio_to_channel(audio_pin);

    uint32_t f_clk_sys = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
    float clock_div = ((float)f_clk_sys * 1000.0f) / 255.0f / (float)sample_freq / (float)SAMPLE_REPETITION_RATE - 0.02f;

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
                          SAMPLE_REPETITION_RATE,
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
                          // used need to multiply by sample repetition rate
                          SAMPLE_REPETITION_RATE * SAMPLES_CHUNK_SIZE,
                          // Don't start yet
                          false);

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
                          (char *)&single_sample + 2 * audio_pin_channel,
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
