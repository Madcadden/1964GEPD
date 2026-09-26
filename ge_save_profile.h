/* Automatic save selection for the verified GoldenEye Plus EEPROM driver.
 * GPL-2.0-or-later. No ROM writes, title/CRC allowlist, save conversion or I/O.
 * Called once before Init_iPIF, after the actual ROM has been loaded/swapped.
 * Recognizes the driver's type-2 probe, 2048-byte bounds, linked joy wrappers
 * and 4K/16K libultra probe. Relocated calls/data are checked by their links.
 * Unsupported or ambiguous drivers leave the user's normal settings alone.
 */
#ifndef GEPD_PLUS_SAVE_PROFILE_H
#define GEPD_PLUS_SAVE_PROFILE_H
#include <stddef.h>

typedef struct GE_SAVE_WORD { unsigned int value, mask; } GE_SAVE_WORD;
static unsigned int GESaveWord(const unsigned char *rom, unsigned int at)
{
    /* 1964 stores the ROM in 32-bit word-swapped (little-endian) order. */
    return (unsigned int)rom[at] | ((unsigned int)rom[at + 1] << 8) |
        ((unsigned int)rom[at + 2] << 16) | ((unsigned int)rom[at + 3] << 24);
}
static int GESaveMatch(const unsigned char *rom, unsigned int size,
    unsigned int at, const GE_SAVE_WORD *pattern, unsigned int count)
{
    unsigned int i;
    if((at & 3U) || at > size || count > (size - at) / 4U) return 0;
    for(i = 0; i < count; ++i)
        if((GESaveWord(rom, at + 4U * i) & pattern[i].mask) != pattern[i].value)
            return 0;
    return 1;
}
#define GE_SAVE_MATCH(r,s,a,p) GESaveMatch(r,s,a,p,sizeof(p)/sizeof((p)[0]))
static unsigned int GESaveTarget(unsigned int word)
{
    return 0x80000000U | ((word & 0x03FFFFFFU) << 2);
}
static unsigned int GESaveAddress(unsigned int upper, unsigned int lower)
{
    return ((upper & 0xFFFFU) << 16) + (int)(short)(lower & 0xFFFFU);
}

static const GE_SAVE_WORD geSaveProbe[] = {
    {0x27BDFFE8U,0xFFFFFFFFU},    {0xAFBF0014U,0xFFFFFFFFU},
    {0x0C000000U,0xFC000000U},    {0x00000000U,0xFFFFFFFFU},
    {0x8FBF0014U,0xFFFFFFFFU},    {0x384E0002U,0xFFFFFFFFU},
    {0x2DC20001U,0xFFFFFFFFU},    {0x03E00008U,0xFFFFFFFFU},
    {0x27BD0018U,0xFFFFFFFFU},
};

static const GE_SAVE_WORD geSaveTransfer[] = {
    {0x27BDFFE8U,0xFFFFFFFFU},    {0x10A0000CU,0xFFFFFFFFU},
    {0xAFBF0014U,0xFFFFFFFFU},    {0x308E0007U,0xFFFFFFFFU},
    {0x15C00009U,0xFFFFFFFFU},    {0x30CF0007U,0xFFFFFFFFU},
    {0x15E00007U,0xFFFFFFFFU},    {0x2C810801U,0xFFFFFFFFU},
    {0x10200005U,0xFFFFFFFFU},    {0x24180800U,0xFFFFFFFFU},
    {0x0304C823U,0xFFFFFFFFU},    {0x0326082BU,0xFFFFFFFFU},
    {0x10200003U,0xFFFFFFFFU},    {0x000410C2U,0xFFFFFFFFU},
    {0x10000017U,0xFFFFFFFFU},    {0x2402FFFFU,0xFFFFFFFFU},
    {0x2C410100U,0xFFFFFFFFU},    {0x10200005U,0xFFFFFFFFU},
    {0x000640C2U,0xFFFFFFFFU},    {0x01024821U,0xFFFFFFFFU},
    {0x2D210101U,0xFFFFFFFFU},    {0x14200003U,0xFFFFFFFFU},
    {0x00000000U,0xFFFFFFFFU},    {0x1000000EU,0xFFFFFFFFU},
    {0x2402FFFFU,0xFFFFFFFFU},    {0x14E00005U,0xFFFFFFFFU},
    {0x24010001U,0xFFFFFFFFU},    {0x0C000000U,0xFC000000U},
    {0x304400FFU,0xFFFFFFFFU},    {0x10000009U,0xFFFFFFFFU},
    {0x8FBF0014U,0xFFFFFFFFU},    {0x54E10006U,0xFFFFFFFFU},
    {0x2402FFFFU,0xFFFFFFFFU},    {0x0C000000U,0xFC000000U},
    {0x304400FFU,0xFFFFFFFFU},    {0x10000003U,0xFFFFFFFFU},
    {0x8FBF0014U,0xFFFFFFFFU},    {0x2402FFFFU,0xFFFFFFFFU},
    {0x8FBF0014U,0xFFFFFFFFU},    {0x27BD0018U,0xFFFFFFFFU},
    {0x03E00008U,0xFFFFFFFFU},    {0x00000000U,0xFFFFFFFFU},
};

static const GE_SAVE_WORD geSaveJoyProbe[] = {
    {0x27BDFFE0U,0xFFFFFFFFU},    {0xAFBF0014U,0xFFFFFFFFU},
    {0x0C000000U,0xFC000000U},    {0x00000000U,0xFFFFFFFFU},
    {0x3C040000U,0xFFFF0000U},    {0x0C000000U,0xFC000000U},
    {0x24840000U,0xFFFF0000U},    {0x0C000000U,0xFC000000U},
    {0xAFA2001CU,0xFFFFFFFFU},    {0x8FBF0014U,0xFFFFFFFFU},
    {0x8FA2001CU,0xFFFFFFFFU},    {0x27BD0020U,0xFFFFFFFFU},
    {0x03E00008U,0xFFFFFFFFU},    {0x00000000U,0xFFFFFFFFU},
};

static const GE_SAVE_WORD geSaveJoyLong[] = {
    {0x27BDFFE0U,0xFFFFFFFFU},    {0xAFBF0014U,0xFFFFFFFFU},
    {0xAFA40020U,0xFFFFFFFFU},    {0xAFA50024U,0xFFFFFFFFU},
    {0x0C000000U,0xFC000000U},    {0xAFA60028U,0xFFFFFFFFU},
    {0x3C040000U,0xFFFF0000U},    {0x24840000U,0xFFFF0000U},
    {0x93A50023U,0xFFFFFFFFU},    {0x8FA60024U,0xFFFFFFFFU},
    {0x0C000000U,0xFC000000U},    {0x8FA70028U,0xFFFFFFFFU},
    {0x0C000000U,0xFC000000U},    {0xAFA2001CU,0xFFFFFFFFU},
    {0x8FBF0014U,0xFFFFFFFFU},    {0x8FA2001CU,0xFFFFFFFFU},
    {0x27BD0020U,0xFFFFFFFFU},    {0x03E00008U,0xFFFFFFFFU},
    {0x00000000U,0xFFFFFFFFU},
};

static const GE_SAVE_WORD geSaveOSProbe[] = {
    {0x27BDFFD8U,0xFFFFFFFFU},    {0xAFBF0014U,0xFFFFFFFFU},
    {0xAFA40028U,0xFFFFFFFFU},    {0x0C000000U,0xFC000000U},
    {0xAFA00024U,0xFFFFFFFFU},    {0x8FA40028U,0xFFFFFFFFU},
    {0x0C000000U,0xFC000000U},    {0x27A50020U,0xFFFFFFFFU},
    {0xAFA20024U,0xFFFFFFFFU},    {0x8FAE0024U,0xFFFFFFFFU},
    {0x15C00013U,0xFFFFFFFFU},    {0x00000000U,0xFFFFFFFFU},
    {0x97AF0020U,0xFFFFFFFFU},    {0x34018000U,0xFFFFFFFFU},
    {0x31F8C000U,0xFFFFFFFFU},    {0x3319FFFFU,0xFFFFFFFFU},
    {0x17210004U,0xFFFFFFFFU},    {0xA7B8001EU,0xFFFFFFFFU},
    {0x24080001U,0xFFFFFFFFU},    {0x1000000BU,0xFFFFFFFFU},
    {0xAFA80024U,0xFFFFFFFFU},    {0x97A9001EU,0xFFFFFFFFU},
    {0x3401C000U,0xFFFFFFFFU},    {0x15210004U,0xFFFFFFFFU},
    {0x00000000U,0xFFFFFFFFU},    {0x240A0002U,0xFFFFFFFFU},
    {0x10000004U,0xFFFFFFFFU},    {0xAFAA0024U,0xFFFFFFFFU},
    {0x10000002U,0xFFFFFFFFU},    {0xAFA00024U,0xFFFFFFFFU},
    {0xAFA00024U,0xFFFFFFFFU},    {0x0C000000U,0xFC000000U},
    {0x00000000U,0xFFFFFFFFU},    {0x8FBF0014U,0xFFFFFFFFU},
    {0x8FA20024U,0xFFFFFFFFU},    {0x27BD0028U,0xFFFFFFFFU},
    {0x03E00008U,0xFFFFFFFFU},    {0x00000000U,0xFFFFFFFFU},
};

static int GEPlusHas16KSaveDriver(const unsigned char *rom, unsigned int size)
{
    unsigned int at, driver = 0, found = 0, probeRAM, io, readRAM, writeRAM;
    unsigned int readROM, writeROM, osProbeROM, queue, disable, enable;
    if(rom == NULL || size < 0x1100U || size > 0x04000000U || (size & 3U) ||
        GESaveWord(rom, 0) != 0x80371240U) return 0;
    for(at = 0x1000U; at <= size - 204U; at += 4U) {
        if(GESaveWord(rom, at) == 0x27BDFFE8U &&
            GE_SAVE_MATCH(rom, size, at, geSaveProbe) &&
            GE_SAVE_MATCH(rom, size, at + 36U, geSaveTransfer)) {
            if(driver) return 0;
            driver = at;
        }
    }
    if(!driver) return 0;
    io = driver + 36U;
    probeRAM = GESaveTarget(GESaveWord(rom, driver + 8U));
    readRAM = GESaveTarget(GESaveWord(rom, io + 108U));
    writeRAM = GESaveTarget(GESaveWord(rom, io + 132U));
    if(probeRAM >= 0x80800000U || readRAM >= 0x80800000U ||
        writeRAM >= 0x80800000U || readRAM == writeRAM) return 0;
    for(at = 0x1000U; at <= size - 56U; at += 4U) {
        if(GESaveWord(rom, at) != 0x27BDFFE0U ||
            !GE_SAVE_MATCH(rom, size, at, geSaveJoyProbe)) continue;
        /* All three wrappers must agree on the resident ROM/RAM mapping.
         * Unsigned underflow produces an out-of-range offset, rejected below. */
        readROM = at + readRAM - probeRAM;
        writeROM = at + writeRAM - probeRAM;
        osProbeROM = at + GESaveTarget(GESaveWord(rom, at + 20U)) - probeRAM;
        if(!GE_SAVE_MATCH(rom, size, readROM, geSaveJoyLong) ||
            !GE_SAVE_MATCH(rom, size, writeROM, geSaveJoyLong) ||
            !GE_SAVE_MATCH(rom, size, osProbeROM, geSaveOSProbe)) continue;
        queue = GESaveAddress(GESaveWord(rom, at + 16U), GESaveWord(rom, at + 24U));
        disable = GESaveWord(rom, at + 8U);
        enable = GESaveWord(rom, at + 28U);
        if((queue & 0xFF800003U) != 0x80000000U || disable == enable ||
            GESaveWord(rom, readROM + 16U) != disable ||
            GESaveWord(rom, writeROM + 16U) != disable ||
            GESaveWord(rom, readROM + 48U) != enable ||
            GESaveWord(rom, writeROM + 48U) != enable ||
            GESaveAddress(GESaveWord(rom, readROM + 24U), GESaveWord(rom, readROM + 28U)) != queue ||
            GESaveAddress(GESaveWord(rom, writeROM + 24U), GESaveWord(rom, writeROM + 28U)) != queue ||
            GESaveWord(rom, readROM + 40U) == GESaveWord(rom, writeROM + 40U)) continue;
        if(found) return 0;
        found = 1;
    }
    return found != 0;
}
#undef GE_SAVE_MATCH
#endif
