#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

// Max size of the tape image
#define MAX_TAP_IMAGE_SIZE (512 * 1024)
// Max size of the wave image
#define MAX_WAVE_IMAGE_SIZE (1024 * 1024)

static uint32_t tap_image_size;
static uint8_t tap_image[MAX_TAP_IMAGE_SIZE];

static uint32_t wave_image_size;
static uint8_t wave_image[MAX_WAVE_IMAGE_SIZE];

static uint8_t _current_level;
static uint8_t _shifter;
static uint8_t _shift_count;

void _flush_output() {
    for (int i = 0; i < 8 - _shift_count; i++) {
        _shifter = (_shifter << 1) | 1;
    }
    wave_image[wave_image_size++] = _shifter;
}

static void _output_half_period(uint8_t length) {
    for (int i = 0; i < length; i++) {
        _shifter = (_shifter << 1) | _current_level;
        _shift_count++;
        if (_shift_count == 8) {
            _shift_count = 0;
            wave_image[wave_image_size++] = _shifter;
        }
    }
    _current_level ^= 1;
}

static void _output_bit(uint8_t b) {
    _output_half_period(1);
    _output_half_period(b ? 1 : 2);
}

static void _output_byte(uint8_t b) {
    _output_half_period(1);
    _output_bit(0);

    uint8_t parity = 1;

    for (int i = 0; i < 8; i++) {
        uint8_t bit = b & 1;
        parity += bit;
        _output_bit(bit);
        b >>= 1;
    }

    _output_bit(parity & 1);

    _output_bit(1);
    _output_bit(1);
    _output_bit(1);
}

static bool _find_synchro(uint32_t* pos) {
    int synchro_state = 0;
    while (*pos < tap_image_size) {
        uint8_t b = tap_image[(*pos)++];
        if (b == 0x16) {
            if (synchro_state < 3) synchro_state++;
        } else if (b == 0x24 && synchro_state == 3) {
            return true;
        } else {
            synchro_state = 0;
        }
    }
    return false;
}

static void _output_big_synchro() {
    for (int i = 0; i < 259; i++) {
        _output_byte(0x16);
    }
    _output_byte(0x24);
}

static bool _output_file(uint32_t* pos) {
    uint8_t header[9];
    uint32_t i;

    i = 0;
    while (*pos < tap_image_size && i < 9) {
        uint8_t b = tap_image[(*pos)++];
        header[i++] = b;
        _output_byte(b);
    }
    if (*pos >= tap_image_size) {
        return false;
    }

    uint8_t b;
    while (*pos < tap_image_size && ((b = tap_image[(*pos)++]) != 0)) {
        _output_byte(b);
    }
    if (*pos >= tap_image_size) {
        return false;
    }
    _output_byte(0);

    for (int i = 0; i < 6; i++) {
        _output_half_period(1);
    }

    uint32_t start = header[6] * 256 + header[7];
    uint32_t end = header[4] * 256 + header[5];
    uint32_t size = end - start + 1;
    i = 0;
    while (*pos < tap_image_size && i < size) {
        uint8_t b = tap_image[(*pos)++];
        _output_byte(b);
        i++;
    }
    if (*pos == tap_image_size && i < size) {
        return false;
    }

    for (int i = 0; i < 2; i++) {
        _output_half_period(1);
    }

    return true;
}

// Convert TAP image into WAVE image
static void convert_tap_to_wave(const char* tap_file, const char* wave_file) {
    FILE *in, *out;

    in = fopen(tap_file, "rb");
    if (in == NULL) {
        fprintf(stderr, "Failed to open file for reading: %s", tap_file);
        return;
    }
    fseek(in, 0, SEEK_END);
    tap_image_size = ftell(in);
    if (tap_image_size > MAX_TAP_IMAGE_SIZE) {
        fprintf(stderr, "Invalid TAP image size: %s", tap_file);
        fclose(in);
        return;
    }
    fseek(in, 0, SEEK_SET);
    fread(tap_image, tap_image_size, 1, in);
    fclose(in);

    for (int i = 0; i < 5; i++) {
        _output_half_period(1);
    }

    uint32_t pos = 0;
    while (pos < tap_image_size) {
        if (_find_synchro(&pos)) {
            _output_big_synchro();
            if (!_output_file(&pos)) {
                return;
            }
        }
    }
    _flush_output();

    out = fopen(wave_file, "wb");
    if (out == NULL) {
        fprintf(stderr, "Failed to open file for writing: %s", wave_file);
        return;
    }
    fwrite(&wave_image_size, 4, 1, out);
    fwrite(wave_image, wave_image_size, 1, out);
    fclose(out);
}

static void print_usage(const char* argv0) {
    fprintf(stderr,
            "Usage: %s [-i TAP_file] [-o WAVE_file]\n"
            "\t-h show this help\n",
            argv0);
    exit(1);
}

int main(int argc, char* const argv[]) {
    char *infile = NULL, *outfile = NULL;
    int opt;

    while ((opt = getopt(argc, argv, "i:o:h")) != -1) {
        switch (opt) {
            case 'i':
                infile = strdup(optarg);
                break;
            case 'o':
                outfile = strdup(optarg);
                break;
            case 'h':
            default:
                print_usage(argv[0]);
                break;
        }
    }

    if (!infile || !outfile) {
        print_usage(argv[0]);
    } else {
        convert_tap_to_wave(infile, outfile);
    }

    return 0;
}
