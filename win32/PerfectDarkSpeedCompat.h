/* Perfect Dark controller-poll stall compatibility.
 * Copyright (C) 2026 1964GEPD contributors. GPL-2.0-or-later.
 *
 * The native joy_debug_joy fallback disables cyclic polling and waits for an
 * extra sample when the overclocked main thread catches the VI sampler. Skip
 * only that fallback, preserving the already consumed samples and callback.
 * NTSC beta adds debug call arguments and another helper, so its epilogue and
 * branch displacement differ from retail/PAL. Resolve both complete contexts
 * together and fail closed on ambiguity; no CRC or absolute RAM address.
 */
#ifndef PD_SPEED_COMPAT_H
#define PD_SPEED_COMPAT_H

#define PD_SPEED_MAX_WORDS 32

static const unsigned int pdspeedpattern[27] = {0x8C8501E4, 0x0040F809, 0x8C8601E0, 0x0C005431, 0x00000000, 0x10400011, 0x3C0F8006, 0x8DEFEEC0, 0x51E0000F, 0x8FBF0014, 0x0C005207, 0x00000000, 0x5C40000B, 0x8FBF0014, 0x0C00543A, 0x00000000, 0x0C00508E, 0x00000000, 0x0C005451, 0x00000000, 0x3C04800A, 0x0C005016, 0x24849A60, 0x8FBF0014, 0x27BD0018, 0x03E00008, 0x00000000};
static const unsigned int pdspeedmask[27] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFC000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0000, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFC000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFC000000, 0xFFFFFFFF, 0xFC000000, 0xFFFFFFFF, 0xFC000000, 0xFFFFFFFF, 0xFFFF0000, 0xFC000000, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};

static const unsigned int pdspeedbetapattern[32] = {
    0x8C8501E4, 0x0040F809, 0x8C8601E0, 0x0C005812,
    0x00000000, 0x10400016, 0x3C0E8006, 0x8DCE12B0,
    0x51C00014, 0x8FBF0014, 0x0C0055BF, 0x00000000,
    0x1C40000F, 0x240401F4, 0x3C057005, 0x0C00581B,
    0x24A55894, 0x0C005449, 0x00000000, 0x0C005427,
    0x00000000, 0x3C057005, 0x24A5589C, 0x0C005834,
    0x240401FB, 0x3C04800A, 0x0C0053AF, 0x2484E1C0,
    0x8FBF0014, 0x27BD0018, 0x03E00008, 0x00000000
};
static const unsigned int pdspeedbetamask[32] = {
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFC000000,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0000, 0xFFFF0000,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFC000000, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0000, 0xFC000000,
    0xFFFF0000, 0xFC000000, 0xFFFFFFFF, 0xFC000000,
    0xFFFFFFFF, 0xFFFF0000, 0xFFFF0000, 0xFC000000,
    0xFFFFFFFF, 0xFFFF0000, 0xFC000000, 0xFFFF0000,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
};

typedef struct PD_SPEED_RESOLUTION {
    unsigned int context;
    unsigned int words;
    unsigned int branch;
} PD_SPEED_RESOLUTION;

/* RAM is already in the emulator's host-word byte order. */
static PD_SPEED_RESOLUTION PDResolveSpeedPatch(const unsigned int *ram,
    unsigned int ramBytes)
{
    PD_SPEED_RESOLUTION result = {0, 0, 0};
    unsigned int offset, variant, index, count;
    const unsigned int *pattern, *mask;
    if(ram == 0 || ramBytes < 0x1000 + 27 * 4)
        return result;
    for(offset = 0x1000; offset <= ramBytes - 27 * 4; offset += 4)
    {
        if(ram[offset / 4] != 0x8C8501E4)
            continue;
        for(variant = 0; variant < 2; variant++)
        {
            count = variant ? 32 : 27;
            if(count * 4 > ramBytes - offset)
                continue;
            pattern = variant ? pdspeedbetapattern : pdspeedpattern;
            mask = variant ? pdspeedbetamask : pdspeedmask;
            for(index = 0; index < count; index++)
                if((ram[offset / 4 + index] & mask[index]) != (pattern[index] & mask[index]))
                    break;
            if(index == count)
            {
                if(result.context != 0)
                {
                    result.context = result.words = result.branch = 0;
                    return result;
                }
                result.context = 0x80000000 + offset;
                result.words = count;
                result.branch = variant ? 0x10000018 : 0x10000013;
            }
        }
    }
    return result;
}
#endif
