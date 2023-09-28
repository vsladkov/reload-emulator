#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#define TRACKS_PER_DISK   35
#define SECTORS_PER_TRACK 16
#define BYTES_PER_SECTOR  256
#define BYTES_PER_TRACK   (SECTORS_PER_TRACK * BYTES_PER_SECTOR)
#define DSK_IMAGE_SIZE    (TRACKS_PER_DISK * BYTES_PER_TRACK)

#define BYTES_PER_NIB_SECTOR 374
#define BYTES_PER_NIB_TRACK  (SECTORS_PER_TRACK * BYTES_PER_NIB_SECTOR)
#define NIB_IMAGE_SIZE       (TRACKS_PER_DISK * BYTES_PER_NIB_TRACK)

#define PRIMARY_BUF_LEN   256
#define SECONDARY_BUF_LEN 86
#define DATA_LEN          (PRIMARY_BUF_LEN + SECONDARY_BUF_LEN)

#define PROLOG_LEN 3
#define EPILOG_LEN 3
#define GAP1_LEN   6
#define GAP2_LEN   5

#define DEFAULT_VOLUME 254
#define GAP_BYTE       0xff

typedef struct {
    uint8_t prolog[PROLOG_LEN];
    uint8_t volume[2];
    uint8_t track[2];
    uint8_t sector[2];
    uint8_t checksum[2];
    uint8_t epilog[EPILOG_LEN];
} addr_t;

typedef struct {
    uint8_t prolog[PROLOG_LEN];
    uint8_t data[DATA_LEN];
    uint8_t checksum;
    uint8_t epilog[EPILOG_LEN];
} data_t;

typedef struct {
    uint8_t gap1[GAP1_LEN];
    addr_t addr;
    uint8_t gap2[GAP2_LEN];
    data_t data;
} nib_sector_t;

static uint8_t addr_prolog[] = {0xd5, 0xaa, 0x96};
static uint8_t addr_epilog[] = {0xde, 0xaa, 0xeb};
static uint8_t data_prolog[] = {0xd5, 0xaa, 0xad};
static uint8_t data_epilog[] = {0xde, 0xaa, 0xeb};

// clang-format off
static uint8_t soft_interleave[SECTORS_PER_TRACK] = 
    {0, 7, 0xE, 6, 0xD, 5, 0xC, 4, 0xB, 3, 0xA, 2, 9, 1, 8, 0xF};
static uint8_t phys_interleave[SECTORS_PER_TRACK] = 
    {0, 0xD, 0xB, 9, 7, 5, 3, 1, 0xE, 0xC, 0xA, 8, 6, 4, 2, 0xF};
// clang-format on

static uint8_t primary_buf[PRIMARY_BUF_LEN];
static uint8_t secondary_buf[SECONDARY_BUF_LEN];

static nib_sector_t nib_sector;

static uint8_t dsk_image[DSK_IMAGE_SIZE];
static uint8_t nib_image[NIB_IMAGE_SIZE];

// Do "6 and 2" un-translation
#define TABLE_SIZE 0x40
// clang-format off
static uint8_t table[TABLE_SIZE] = {
    0x96, 0x97, 0x9a, 0x9b, 0x9d, 0x9e, 0x9f, 0xa6,
    0xa7, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb2, 0xb3,
    0xb4, 0xb5, 0xb6, 0xb7, 0xb9, 0xba, 0xbb, 0xbc,
    0xbd, 0xbe, 0xbf, 0xcb, 0xcd, 0xce, 0xcf, 0xd3,
    0xd6, 0xd7, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde,
    0xdf, 0xe5, 0xe6, 0xe7, 0xe9, 0xea, 0xeb, 0xec,
    0xed, 0xee, 0xef, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6,
    0xf7, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};
// clang-format on

// Encode 1 byte into two "4 and 4" bytes
static void odd_even_encode(uint8_t a[], int i) {
    a[0] = (i >> 1) & 0x55;
    a[0] |= 0xaa;

    a[1] = i & 0x55;
    a[1] |= 0xaa;
}

static uint8_t translate(uint8_t byte) { return table[byte & 0x3f]; }

// Convert 256 data bytes into 342 6+2 encoded bytes and a checksum
static void nibbilize(uint8_t track, uint8_t sector, uint8_t* dsk_image) {
    uint8_t* src = dsk_image + track * BYTES_PER_TRACK + sector * BYTES_PER_SECTOR;
    uint8_t* dest = nib_sector.data.data;

    // Nibbilize data into primary and secondary buffers
    memset(primary_buf, 0, PRIMARY_BUF_LEN);
    memset(secondary_buf, 0, SECONDARY_BUF_LEN);

    for (int i = 0; i < PRIMARY_BUF_LEN; i++) {
        primary_buf[i] = src[i] >> 2;

        int index = i % SECONDARY_BUF_LEN;
        int section = i / SECONDARY_BUF_LEN;
        uint8_t pair = ((src[i] & 2) >> 1) | ((src[i] & 1) << 1);  // Swap the low bits
        secondary_buf[index] |= pair << (section * 2);
    }

    // Xor pairs of nibbilized bytes in correct order
    int index = 0;
    dest[index++] = translate(secondary_buf[0]);

    for (int i = 1; i < SECONDARY_BUF_LEN; i++) {
        dest[index++] = translate(secondary_buf[i] ^ secondary_buf[i - 1]);
    }

    dest[index++] = translate(primary_buf[0] ^ secondary_buf[SECONDARY_BUF_LEN - 1]);

    for (int i = 1; i < PRIMARY_BUF_LEN; i++) {
        dest[index++] = translate(primary_buf[i] ^ primary_buf[i - 1]);
    }

    nib_sector.data.checksum = translate(primary_buf[PRIMARY_BUF_LEN - 1]);
}

// Convert DSK image into NIB image
static void convert_dsk_to_nib(const char* dsk_file, const char* nib_file) {
    FILE *in, *out;

    in = fopen(dsk_file, "rb");
    if (in == NULL) {
        fprintf(stderr, "Failed to open file for reading: %s", dsk_file);
        return;
    }
    fseek(in, 0, SEEK_END);
    if (ftell(in) != DSK_IMAGE_SIZE) {
        fprintf(stderr, "Invalid DSK image size: %s", dsk_file);
        fclose(in);
        return;
    }
    fseek(in, 0, SEEK_SET);
    fread(dsk_image, DSK_IMAGE_SIZE, 1, in);
    fclose(in);

    int volume = DEFAULT_VOLUME;

    // Init addr & data field marks & volume number
    memcpy(nib_sector.addr.prolog, addr_prolog, 3);
    memcpy(nib_sector.addr.epilog, addr_epilog, 3);
    memcpy(nib_sector.data.prolog, data_prolog, 3);
    memcpy(nib_sector.data.epilog, data_epilog, 3);
    odd_even_encode(nib_sector.addr.volume, volume);

    // Init gap fields
    memset(nib_sector.gap1, GAP_BYTE, GAP1_LEN);
    memset(nib_sector.gap2, GAP_BYTE, GAP2_LEN);

    // Loop through DSK tracks
    for (int track = 0; track < TRACKS_PER_DISK; track++) {
        // Loop through DSK sectors
        for (int sector = 0; sector < SECTORS_PER_TRACK; sector++) {
            int soft_sector = soft_interleave[sector];
            int phys_sector = phys_interleave[sector];

            // Set ADDR field contents
            int checksum = volume ^ track ^ sector;
            odd_even_encode(nib_sector.addr.track, track);
            odd_even_encode(nib_sector.addr.sector, sector);
            odd_even_encode(nib_sector.addr.checksum, checksum);

            // Set DATA field contents (encode sector data)
            nibbilize(track, soft_sector, dsk_image);

            // Copy to NIB image buffer
            uint8_t* buf = nib_image + track * BYTES_PER_NIB_TRACK + phys_sector * BYTES_PER_NIB_SECTOR;
            memcpy(buf, &nib_sector, sizeof(nib_sector));
        }
    }

    out = fopen(nib_file, "wb");
    if (out == NULL) {
        fprintf(stderr, "Failed to open file for writing: %s", nib_file);
        return;
    }
    fwrite(nib_image, NIB_IMAGE_SIZE, 1, out);
    fclose(out);
}

static void print_usage(const char* argv0) {
    fprintf(stderr,
            "Usage: %s [-i DSK_file] [-o NIB_file]\n"
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
        convert_dsk_to_nib(infile, outfile);
    }
    
    return 0;
}
