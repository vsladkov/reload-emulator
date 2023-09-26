#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#define BYTES_PER_LINE 16

static void print_usage(const char* argv0) {
    fprintf(stderr,
            "Usage: %s [-i NIB_file] [-o header_file]\n"
            "\t-h show this help\n",
            argv0);
    exit(1);
}

static void print_file(FILE* in, FILE* out) {
    uint8_t c;
    int bytes = 0;
    int eof;

    fprintf(out, "#pragma once\n\n");
    fprintf(out, "// clang-format off\n");
    fprintf(out, "uint8_t __in_flash() disk1_nib_image[] = {");

    c = fgetc(in);
    eof = feof(in);
    while (!eof) {
        if ((bytes % BYTES_PER_LINE) == 0) {
            fprintf(out, "\n\t");
        }
        bytes++;
        fprintf(out, "0x%02x", c);

        c = fgetc(in);
        eof = feof(in);
        if (!eof) {
            fprintf(out, ", ");
        }
    }
    if (bytes) {
        fprintf(out, "\n");
    }
    fprintf(out, "};\n");
    fprintf(out, "// clang-format off\n");
}

void convert_nib_to_hdr(const char* infile, const char* outfile) {
    FILE *in, *out;

    in = fopen(infile, "rb");
    if (in) {
        out = fopen(outfile, "w");
        if (out) {
            print_file(in, out);
            fclose(out);
        } else {
            fprintf(stderr, "Failed to open file for writing: %s", outfile);
        }
        fclose(in);
    } else {
        fprintf(stderr, "Failed to open file for reading: %s", infile);
    }
}

int main(int argc, char* const argv[]) {
    char *infile = NULL, *outfile = NULL, *array_name = NULL;
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
                return 0;
        }
    }

    if (!infile || !outfile) {
        print_usage(argv[0]);
    } else {
        convert_nib_to_hdr(infile, outfile);
    }

    return 0;
}