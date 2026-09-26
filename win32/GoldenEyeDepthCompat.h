/* GoldenEye depth-image alias compatibility for the bundled GLN64_2020.
 * Copyright (C) 2026 1964GEPD Auto-Mod contributors. GPL-2.0-or-later.
 *
 * The reference plugin's DepthBufferList::saveBuffer uses a range lookup.
 * GoldenEye's lower split-screen views subtract 320*240 bytes from the depth
 * image address. That address can fall inside a tracked color framebuffer.
 * Accepting that unrelated range suppresses the standalone-depth clear and
 * aliases its depth attachment. For GoldenEye ONLY, continue the lookup until
 * an EXACT start-address match, or the original not-found path, is reached.
 *
 * This narrow adapter changes only the loaded copy of the exact 2020 DLL.
 * The DLL on disk, its settings, ROMs, and PD rendering remain unchanged.
 * Unsupported plugin builds keep the ordinary graphics-profile behavior.
 * Include once in Dll_Video.c, after videoGoldenEyeProfileRequested.
 */
#ifndef GE_DEPTH_COMPAT_H
#define GE_DEPTH_COMPAT_H
#include <stdio.h>
#include <string.h>

static unsigned char *videoDepthSite;
static void *videoDepthResume;
static void *videoDepthAdvance;
static unsigned long videoDepthAliasRejects;
static unsigned char videoDepthInstalledBytes[5];
static const unsigned char videoDepthOriginalBytes[5] = {0x8D,0x68,0x08,0x85,0xED};
static const char *videoDepthAdapterStatus = "not installed";

/* Register-preserving replacement for lea ebp,[eax+8]; test ebp,ebp.
 * EAX is the framebuffer list node; ESI is the requested depth address.
 * Neither continuation expects a new stack frame or changed volatile regs.
 */
__declspec(naked) static void VIDEO_DepthAliasGate(void)
{
    __asm {
        lea ebp, [eax + 8]
        cmp dword ptr [videoGoldenEyeProfileRequested], 0
        je acceptMatch
        cmp dword ptr [ebp], esi
        je acceptMatch
        inc dword ptr [videoDepthAliasRejects]
        jmp dword ptr [videoDepthAdvance]
    acceptMatch:
        test ebp, ebp
        jmp dword ptr [videoDepthResume]
    }
}

static BOOL VIDEO_DepthReferenceDLL(HMODULE module)
{
    char path[MAX_PATH];
    unsigned char block[16384];
    unsigned long table[256], crc = 0xFFFFFFFFUL, value, total = 0;
    unsigned int i, bit;
    size_t count, n;
    DWORD length;
    FILE *file;
    length = GetModuleFileNameA(module, path, sizeof(path));
    if(length == 0 || length >= sizeof(path)) return FALSE;
    file = fopen(path, "rb");
    if(file == NULL) return FALSE;
    for(i = 0; i < 256; ++i) {
        value = i;
        for(bit = 0; bit < 8; ++bit)
            value = (value >> 1) ^ ((value & 1) ? 0xEDB88320UL : 0);
        table[i] = value;
    }
    while((count = fread(block, 1, sizeof(block), file)) != 0) {
        total += (unsigned long)count;
        if(total > 11501568UL) break;
        for(n = 0; n < count; ++n)
            crc = table[(crc ^ block[n]) & 255] ^ (crc >> 8);
    }
    i = !ferror(file) && total == 11501568UL &&
        (crc ^ 0xFFFFFFFFUL) == 0x6FC3B689UL;
    fclose(file);
    return i != 0;
}

static BOOL VIDEO_WriteDepthGate(unsigned char *site, const unsigned char *bytes)
{
    DWORD oldProtection, ignored;
    if(!VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &oldProtection))
        return FALSE;
    memcpy(site, bytes, 5);
    FlushInstructionCache(GetCurrentProcess(), site, 5);
    VirtualProtect(site, 5, oldProtection, &ignored);
    return TRUE;
}

static void VIDEO_InstallDepthCompatibility(HMODULE module)
{
    /* Entire loop and selected-match path: no absolute relocated operands. */
    static const unsigned char loop[] = {
        0x8b,0x01,0x3b,0xc1,0x74,0x10,0x39,0x70,0x08,0x77,0x05,0x39,0x70,0x0c,0x73,0x20,
        0x8b,0x00,0x3b,0xc1,0x75,0xf0,0x33,0xed,0x8b,0x0f,0x8b,0x01,0x3b,0xc1,0x74,0x51,
        0x8d,0x49,0x00,0x39,0x70,0x08,0x74,0x2a,0x8b,0x00,0x3b,0xc1,0x75,0xf5,0xeb,0x41,
        0x8d,0x68,0x08,0x85,0xed,0x74,0xe1,0x8b,0x45,0x00,0xc6,0x45,0x29,0x01,0x3b,0xc6,
        0x74,0xd6,0x50,0x8b,0xcf,0xe8,0xb9,0xfc,0xff,0xff,0x8b,0xc8,0x89,0x44,0x24,0x24,0xeb,0x07
    };
    unsigned char *base = (unsigned char *)module, *site;
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;
    LONG relative;
    if(videoDepthSite != NULL) return;
    videoDepthAdapterStatus = "unsupported DLL; profile-only handling";
    if(module == NULL || !VIDEO_DepthReferenceDLL(module)) return;
    dos = (IMAGE_DOS_HEADER *)base;
    if(dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 4096) return;
    nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    if(nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt->FileHeader.TimeDateStamp != 0x5E4A2D53UL ||
        nt->OptionalHeader.SizeOfImage != 0xF17000UL ||
        memcmp(base + 0xD74D, loop, sizeof(loop)) != 0) return;
    site = base + 0xD77D;
    videoDepthResume = base + 0xD782;
    videoDepthAdvance = base + 0xD75D;
    videoDepthInstalledBytes[0] = 0xE9;
    relative = (LONG)((ULONG_PTR)VIDEO_DepthAliasGate - ((ULONG_PTR)site + 5));
    memcpy(videoDepthInstalledBytes + 1, &relative, 4);
    if(!VIDEO_WriteDepthGate(site, videoDepthInstalledBytes)) {
        videoDepthResume = videoDepthAdvance = NULL;
        videoDepthAdapterStatus = "could not install depth adapter";
        return;
    }
    videoDepthSite = site;
    videoDepthAdapterStatus = "installed for GLN64_2020; active only for detected GoldenEye";
}

static void VIDEO_RestoreDepthCompatibility(void)
{
    if(videoDepthSite != NULL && memcmp(videoDepthSite, videoDepthInstalledBytes, 5) == 0)
        VIDEO_WriteDepthGate(videoDepthSite, videoDepthOriginalBytes);
    videoDepthSite = NULL;
    videoDepthResume = videoDepthAdvance = NULL;
    videoDepthAdapterStatus = "not installed";
}
#endif
