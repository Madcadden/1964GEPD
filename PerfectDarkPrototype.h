/* In-memory compatibility for the original NTSC 6.4 development-cart dump.
 * The user's ROM file is never modified. Unknown or damaged images are left
 * untouched. The DC build contains the same library except for two reads of
 * the development board's identification registers; use its constants here.
 * Include after zlib.h. Byte addresses below are N64 big-endian addresses;
 * the caller has already converted the image to 1964's little-endian words.
 */
#ifndef PERFECT_DARK_PROTOTYPE_H
#define PERFECT_DARK_PROTOTYPE_H

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static uint32_t PDPrototypeReadWord(const unsigned char *rom, size_t offset)
{
    return ((uint32_t)rom[offset + 3] << 24) |
           ((uint32_t)rom[offset + 2] << 16) |
           ((uint32_t)rom[offset + 1] << 8) | rom[offset];
}

/* CIC 6105 checks the first MiB of payload using its boot-code lookup table.
 * Recompression changes that payload even when the expanded code is identical.
 * Update before the loader copies the header to SP_DMEM and HeaderDllPass.
 */
static void PDPrototypeUpdateChecksum(unsigned char *rom)
{
    uint32_t t1 = 0xdf26f436U, t2 = t1, t3 = t1;
    uint32_t t4 = t1, t5 = t1, t6 = t1;
    uint32_t d, sum, rotation, bits, crc1, crc2;
    size_t offset, i;

    for (offset = 0x1000; offset < 0x101000; offset += 4) {
        d = PDPrototypeReadWord(rom, offset);
        sum = t6 + d;
        if (sum < t6) ++t4;
        t6 = sum;
        t3 ^= d;
        bits = d & 31U;
        rotation = bits ? ((d << bits) | (d >> (32U - bits))) : d;
        t5 += rotation;
        t2 ^= t2 > d ? rotation : t6 ^ d;
        t1 += PDPrototypeReadWord(rom, 0x750 + (offset & 0xff)) ^ d;
    }
    crc1 = t6 ^ t4 ^ t3;
    crc2 = t5 ^ t2 ^ t1;
    for (i = 0; i < 4; ++i) {
        rom[(0x10 + i) ^ 3] = (unsigned char)(crc1 >> (24 - 8 * i));
        rom[(0x14 + i) ^ 3] = (unsigned char)(crc2 >> (24 - 8 * i));
    }
}

static int PDNormalizeDevelopmentCart(unsigned char *rom, size_t size)
{
    static const unsigned char ec_header[64] = {
        0x80,0x37,0x12,0x40,0x00,0x00,0x00,0x0f,
        0x80,0x00,0x10,0x00,0x00,0x00,0x14,0x49,
        0x3e,0xc3,0x6c,0x75,0xf0,0x30,0x48,0xf1,
        0x54,0xe4,0xd8,0x38,0x2f,0xbd,0x95,0x36,
        0x67,0x33,0xe0,0x7c,0x38,0x91,0x4e,0xe8,
        0x38,0x43,0x60,0x3d,0x6e,0x54,0x92,0xc4,
        0x48,0x35,0xdb,0x37,0x39,0x6b,0xc9,0x37,
        0xa4,0x81,0x99,0xd1,0xc9,0x38,0xf9,0x2b
    };
    static const unsigned char identity[32] = {
        'P','e','r','f','e','c','t',' ','D','a','r','k',' ','D','B','G',
        'N','T','S','C',0,0,0,0,0,0,0,'N','P','D','E',1
    };
    enum { packed_start = 0x3055, packed_end = 0x30850,
           packed_capacity = packed_end - packed_start, expanded_size = 0x58710 };
    unsigned char *packed = NULL, *expanded = NULL;
    z_stream stream;
    size_t i, output_size;
    int status, result = 0;

    if (!rom || size != 0x4000000) return 0;
    for (i = 0; i < sizeof(ec_header); ++i)
        if (rom[i ^ 3] != ec_header[i]) return 0;
    if (rom[0x3050 ^ 3] != 0x11 || rom[0x3051 ^ 3] != 0x73 ||
        rom[0x3052 ^ 3] != 5 || rom[0x3053 ^ 3] != 0x87 ||
        rom[0x3054 ^ 3] != 0x10) return 0;

    packed = (unsigned char *)malloc(packed_capacity);
    expanded = (unsigned char *)malloc(expanded_size);
    if (!packed || !expanded) goto done;
    for (i = 0; i < packed_capacity; ++i) packed[i] = rom[(packed_start + i) ^ 3];
    /* Exact supplied compressed block, including its padding. */
    if (crc32(0L, packed, packed_capacity) != 0x15758ac3UL) goto done;
    memset(&stream, 0, sizeof(stream));
    stream.next_in = packed;
    stream.avail_in = packed_capacity;
    stream.next_out = expanded;
    stream.avail_out = expanded_size;
    if (inflateInit2(&stream, -15) != Z_OK) goto done;
    status = inflate(&stream, Z_FINISH);
    output_size = stream.total_out;
    inflateEnd(&stream);
    if (status != Z_STREAM_END || output_size != expanded_size ||
        crc32(0L, expanded, expanded_size) != 0xe0ee291dUL) goto done;
    if (memcmp(expanded + 0xb0b0, "\x84\x82\x00\x00\x24\x01\x4f\x4a", 8) ||
        memcmp(expanded + 0xb0d8, "\x84\x82\x00\x02\x24\x01\x46\x53", 8)) goto done;
    memcpy(expanded + 0xb0b0, "\x24\x02\x4f\x4a", 4);
    memcpy(expanded + 0xb0d8, "\x24\x02\x46\x53", 4);

    memset(&stream, 0, sizeof(stream));
    stream.next_in = expanded;
    stream.avail_in = expanded_size;
    stream.next_out = packed;
    stream.avail_out = packed_capacity;
    if (deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, -15, 8,
                     Z_DEFAULT_STRATEGY) != Z_OK) goto done;
    status = deflate(&stream, Z_FINISH);
    output_size = stream.total_out;
    deflateEnd(&stream);
    if (status != Z_STREAM_END || !output_size || output_size > packed_capacity) goto done;

    /* Commit only after complete validation and bounded recompression. */
    for (i = 0; i < output_size; ++i) rom[(packed_start + i) ^ 3] = packed[i];
    for (; i < packed_capacity; ++i) rom[(packed_start + i) ^ 3] = 0;
    for (i = 0; i < sizeof(identity); ++i) rom[(0x20 + i) ^ 3] = identity[i];
    PDPrototypeUpdateChecksum(rom);
    result = 1;
done:
    free(expanded);
    free(packed);
    return result;
}

#endif
