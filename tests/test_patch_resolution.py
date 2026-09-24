#!/usr/bin/env python3
"""Exercise production patch resolvers against three user-supplied .z64 ROMs.

Usage:
    python tests/test_patch_resolution.py retail.z64 plus.z64 pd_rev1.z64
    python tests/test_patch_resolution.py --sanitize retail.z64 plus.z64 pd_rev1.z64

Requires Python 3 and a native C compiler (cc, or CC environment variable).
No ROMs or decompressed game binaries are included or retained. This tests
patch discovery, validation, restoration and simulated RAM writes; it is not
an emulator gameplay test. Sanitizers are optional for compiler portability.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile
import zlib

preamble = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
typedef int BOOL;
typedef long LONG;
static LONG InterlockedExchange(volatile LONG *address,LONG value) { LONG old=*address; *address=value; return old; }
#define TRUE 1
#define FALSE 0
static struct { unsigned char *ROM_Image; } gMemoryState;
static unsigned int gAllocationLength;
static unsigned char gMS_RDRAM[0x800000];
static unsigned int current_rdram_size = sizeof gMS_RDRAM;
static unsigned char *TLB_sDWORD_R[0x100000];
#define GHACK_NONE 0
#define GHACK_GE 1
#define GHACK_PD 2
#define TV_SYSTEM_NTSC 0
static struct { unsigned int game_hack, gepd_pause; } emustatus;
static struct { unsigned int TV_System; } rominfo;
static struct { unsigned int GEFiringRateHack, GEDisableHeadRoll, OverclockFactor, PDSpeedHack; } emuoptions;
static struct { unsigned int crc1, crc2; } currentromoptions;
#define LOAD_UWORD_PARAM(x) (*(unsigned int *)(gMS_RDRAM + ((x) & 0x7fffffU)))
#define LOAD_SWORD_PARAM(x) (*(int *)(gMS_RDRAM + ((x) & 0x7fffffU)))
static unsigned int invalidations, pageinvalidations;
static void Check_And_Invalidate_Compiled_Blocks_By_DMA(unsigned int a, unsigned int n, char *s) {
    assert(a >= 0x80000000U && a + n <= 0x80800000U); (void)s; invalidations++;
}
static void InvalidateOneBlock(unsigned int a) { assert((a >= 0x70000000U && a < 0x80800000U) || (a >= 0xa0000000U && a < 0xa0800000U)); pageinvalidations++; }
'''

main = r'''
static void load(const char *p) {
    FILE *f = fopen(p, "rb"); long n; unsigned int i;
    assert(f); fseek(f, 0, SEEK_END); n = ftell(f); rewind(f);
    GEPDRestoreROMHacks(); free(gMemoryState.ROM_Image); gMemoryState.ROM_Image = malloc(n); assert(gMemoryState.ROM_Image);
    assert(fread(gMemoryState.ROM_Image, 1, n, f) == (unsigned long)n); fclose(f); gAllocationLength = n;
    for(i=0; i<gAllocationLength; i+=4) {
        unsigned char *b = gMemoryState.ROM_Image+i, tmp;
        tmp=b[0]; b[0]=b[3]; b[3]=tmp; tmp=b[1]; b[1]=b[2]; b[2]=tmp;
    }
    memset(gMS_RDRAM, 0, sizeof gMS_RDRAM); invalidations = pageinvalidations = 0; GEPDResetHackResolution();
}
static void run(const char *path, int retail) {
    GE_HACK_RESOLUTION r; unsigned int i, saved, before;
    load(path); r = *GEGetHackResolution();
    assert(r.gamevalid && r.firingvalid && r.headrollvalid);
    assert(r.guardvalid == retail);
    assert(GEUsesROMCodeMapping()==retail);
    assert(r.pause == (retail ? 0x80048370U : 0x80048810U));
    emustatus.game_hack=GHACK_GE;
    GEPDPause(TRUE); assert(LOAD_UWORD_PARAM(r.pause)==1);
    GEPDPause(FALSE); assert(LOAD_UWORD_PARAM(r.pause)==0);
    printf("%s: fire=%x guard=%d drone=%d head=%x\n", retail?"retail":"Plus", r.readfiringrate, r.guardvalid, r.dronevalid, r.headrollnop[0]);
    /* Stop/Play reuses the ROM allocation: restore byte-exact source and rediscover. */
    {
        unsigned char *original=malloc(gAllocationLength); assert(original);
        memcpy(original,gMemoryState.ROM_Image,gAllocationLength);
        GEFiringRateHack(); GEDisableHeadRoll();
        assert(memcmp(original,gMemoryState.ROM_Image,gAllocationLength)!=0);
        GEPDRestoreROMHacks();
        assert(memcmp(original,gMemoryState.ROM_Image,gAllocationLength)==0);
        GEPDResetHackResolution(); r=*GEGetHackResolution();
        assert(r.firingvalid && r.headrollvalid && r.guardvalid==retail);
        GEFiringRateHack(); GEDisableHeadRoll();
        GEWriteROMWord(r.headrollnop[0],0xf1234567);
        GEPDRestoreROMHacks();
        /* Another writer owns the changed multiword group; preserve all of it. */
        assert(GEReadROMWord(r.headrollnop[0])==0xf1234567);
        for(i=1;i<6;i++) assert(GEReadROMWord(r.headrollnop[i])==0);
        assert(GEReadROMWord(r.readfiringrate)==0);
        memcpy(gMemoryState.ROM_Image,original,gAllocationLength); free(original);
        GEPDResetHackResolution(); r=*GEGetHackResolution();
    }
    /* A changed mod AI/drone instruction cannot disable its independent fire fix. */
    if(r.dronevalid) GEWriteROMWord(r.dronegunfiringrate, 0x250b0003);
    if(r.guardvalid) GEWriteROMWord(r.updateaimtarget, 0);
    GEPDResetHackResolution(); assert(GEGetHackResolution()->firingvalid);
    GEFiringRateHack(); assert(GEReadROMWord(r.readfiringrate)==0x00021040);
    /* Native RAM loading at arbitrary positions; repeat call is a no-op. */
    load(path); r = *GEGetHackResolution();
    memcpy(gMS_RDRAM+0x510000, gMemoryState.ROM_Image+r.readfiringrate-0x10, sizeof gefiringratepattern);
    memcpy(gMS_RDRAM+0x520000, gMemoryState.ROM_Image+r.headrollnop[0]-4, 5*0x1c+12);
    GEFiringRateHack(); GEDisableHeadRoll();
    assert(LOAD_UWORD_PARAM(0x80510010)==0x00021040);
    for(i=0;i<6;i++) assert(LOAD_UWORD_PARAM(0x80520004+i*0x1c)==0);
    assert(invalidations==7 && pageinvalidations==14);
    before=invalidations; GEFiringRateHack(); GEDisableHeadRoll(); assert(invalidations==before);
    /* Two RAM matches must not be patched. */
    load(path); r=*GEGetHackResolution();
    memcpy(gMS_RDRAM+0x510000, gMemoryState.ROM_Image+r.readfiringrate-0x10, sizeof gefiringratepattern);
    memcpy(gMS_RDRAM+0x530000, gMS_RDRAM+0x510000, sizeof gefiringratepattern);
    memcpy(gMS_RDRAM+0x520000, gMemoryState.ROM_Image+r.headrollnop[0]-4, 5*0x1c+12);
    memcpy(gMS_RDRAM+0x540000, gMS_RDRAM+0x520000, 5*0x1c+12);
    GEPatchRAMFiringRate(); GEPatchRAMHeadRoll(); assert(invalidations==0);
    /* Corrupt RAM fingerprint must not be patched. */
    memset(gMS_RDRAM+0x530000,0, sizeof gefiringratepattern);
    memset(gMS_RDRAM+0x540000,0, 5*0x1c+12);
    LOAD_UWORD_PARAM(0x80510000)^=1; LOAD_UWORD_PARAM(0x80520004)^=1;
    GEPatchRAMFiringRate(); GEPatchRAMHeadRoll(); assert(invalidations==0);
    /* Duplicates in ROM invalidate exactly the duplicated feature. */
    load(path); r=*GEGetHackResolution();
    memcpy(gMemoryState.ROM_Image+0x500000, gMemoryState.ROM_Image+r.readfiringrate-0x10, sizeof gefiringratepattern);
    GEPDResetHackResolution(); assert(!GEGetHackResolution()->firingvalid && GEGetHackResolution()->headrollvalid);
    GEFiringRateHack(); assert(GEReadROMWord(r.readfiringrate)==0);
    load(path); r=*GEGetHackResolution(); saved=GEReadROMWord(r.readfiringrate-0x10);
    GEWriteROMWord(r.readfiringrate-0x10,saved^1); GEPDResetHackResolution(); assert(!GEGetHackResolution()->firingvalid);
    /* Restored source and reused allocation work after reset. */
    GEWriteROMWord(r.readfiringrate-0x10,saved); GEPDResetHackResolution(); assert(GEGetHackResolution()->firingvalid);
}

static void loadblob(const char *path, unsigned int base) {
    FILE *f=fopen(path,"rb"); unsigned char b[4]; assert(f);
    while(fread(b,1,4,f)==4) { LOAD_UWORD_PARAM(base)=((unsigned int)b[0]<<24)|((unsigned int)b[1]<<16)|((unsigned int)b[2]<<8)|b[3]; base+=4; assert(base <= 0x80800000U); }
    fclose(f);
}
static void resetpd(void) { memset(gMS_RDRAM,0,sizeof gMS_RDRAM); GEPDResetHackResolution(); invalidations=pageinvalidations=0; emustatus.game_hack=GHACK_PD; currentromoptions.crc1=0x41F2B98F; currentromoptions.crc2=0xB458B466; }
static void runpd(const char *lib, const char *game) {
    unsigned int i;
    resetpd(); loadblob(lib,0x80001050); loadblob(game,0x80220000);
    assert(GEPDFindRAMPattern(pdspeedpattern,pdspeedmask,27)==0x8001437c);
    PDSpeedHack(); assert(LOAD_UWORD_PARAM(0x80014388)==0x10000013); assert(invalidations==1);
    PDTimingHack(); assert(invalidations==47); assert(LOAD_UWORD_PARAM(PD_masterclock)==0x0bc69e38);
    PDSpeedHack(); PDTimingHack(); assert(invalidations==47);
    /* Corrupted retail cave is rejected before any timing write. */
    resetpd(); loadblob(game,0x80220000); LOAD_UWORD_PARAM(PD_newcodearea+4)^=1;
    PDTimingHack(); assert(invalidations==0);
    /* Header-different mod does not get retail-specific timing stub. */
    resetpd(); loadblob(game,0x80220000); currentromoptions.crc1^=1;
    PDTimingHack(); assert(invalidations==0);
    /* Moved lib with data references and calls changed, alias invalidation. */
    resetpd();
    for(i=0;i<27;i++) LOAD_UWORD_PARAM(0x80510000+i*4)=pdspeedpattern[i]^(~pdspeedmask[i]&0x1234);
    TLB_sDWORD_R[0x70014]=gMS_RDRAM+0x510000;
    PDSpeedHack(); assert(LOAD_UWORD_PARAM(0x8051000c)==0x10000013); assert(invalidations==1 && pageinvalidations==3);
    PDSpeedHack(); assert(invalidations==1); TLB_sDWORD_R[0x70014]=0;
    /* Corrupt/ambiguous sites cause no writes. */
    resetpd(); memcpy(gMS_RDRAM+0x510000,pdspeedpattern,sizeof pdspeedpattern); memcpy(gMS_RDRAM+0x520000,pdspeedpattern,sizeof pdspeedpattern);
    PDSpeedHack(); assert(invalidations==0);
    memset(gMS_RDRAM+0x520000,0,sizeof pdspeedpattern); LOAD_UWORD_PARAM(0x80510000)^=1;
    PDSpeedHack(); assert(invalidations==0);
    
    /* Head-roll getter must resolve independently and keep its saved-data load. */
    resetpd(); loadblob(game,0x80220000);
    PDDisableHeadRoll(); assert(invalidations==1); assert(LOAD_UWORD_PARAM(pdHeadRollSite)==0x00001025);
    PDDisableHeadRoll(); assert(invalidations==1);
    resetpd(); for(i=0;i<9;i++) LOAD_UWORD_PARAM(0x80510000+i*4)=pdheadrollpattern[i]^(~pdheadrollmask[i]&0x1234);
    PDDisableHeadRoll(); assert(invalidations==1 && LOAD_UWORD_PARAM(0x80510020)==0x00001025);
    resetpd(); memcpy(gMS_RDRAM+0x510000,pdheadrollpattern,sizeof pdheadrollpattern); memcpy(gMS_RDRAM+0x520000,pdheadrollpattern,sizeof pdheadrollpattern);
    PDDisableHeadRoll(); assert(invalidations==0);
    memset(gMS_RDRAM+0x520000,0,sizeof pdheadrollpattern); LOAD_UWORD_PARAM(0x80510000)^=1; PDDisableHeadRoll(); assert(invalidations==0);
    /* Pause resolves actual data, retains only acquired flag, and rejects duplicates. */
    resetpd(); loadblob(game,0x80220000);
    assert(GEPDResolvePauseAddress()==0x80084014);
    GEPDPause(TRUE); assert(LOAD_UWORD_PARAM(0x80084014)==1);
    GEPDPause(FALSE); assert(LOAD_UWORD_PARAM(0x80084014)==0);
    LOAD_UWORD_PARAM(0x80084014)=1; GEPDPause(TRUE); GEPDPause(FALSE); assert(LOAD_UWORD_PARAM(0x80084014)==1);
    resetpd(); memcpy(gMS_RDRAM+0x510000,pdpausepattern,sizeof pdpausepattern); memcpy(gMS_RDRAM+0x520000,pdpausepattern,sizeof pdpausepattern);
    GEPDPause(TRUE); assert(LOAD_UWORD_PARAM(0x80084014)==0);
    memset(gMS_RDRAM+0x520000,0,sizeof pdpausepattern);
    LOAD_UWORD_PARAM(0x80510000)=0x3c028035; LOAD_UWORD_PARAM(0x80510008)=0x8c428020;
    GEPDPause(TRUE); assert(LOAD_UWORD_PARAM(0x80348020)==1);
    memset(gMS_RDRAM+0x510000,0,sizeof pdpausepattern); GEPDPause(FALSE); assert(LOAD_UWORD_PARAM(0x80348020)==0);
    /* UI request queues only; VI callback performs writes once and consumes request. */
    resetpd(); memcpy(gMS_RDRAM+0x510000,pdspeedpattern,sizeof pdspeedpattern);
    emuoptions.PDSpeedHack=1; emuoptions.OverclockFactor=18; emuoptions.GEDisableHeadRoll=0;
    GEPDQueueRuntimeHacks(); assert(invalidations==0);
    GEPDApplyPendingHacks(); assert(invalidations==0); /* IPL gate */
    GEPDOnGameEntry(); assert(invalidations==1);
    GEPDApplyPendingHacks(); assert(invalidations==1);
    puts("PD actual-code tests passed: speed/head-roll, alias, duplicate/corrupt, timing, pause, queued VI writes");
}
int main(int argc, char **argv) { assert(argc==5); run(argv[1],1); run(argv[2],0); runpd(argv[3],argv[4]); free(gMemoryState.ROM_Image); puts("GE actual-code tests passed: discovery, RAM patches, Stop/Play restoration, external-write preservation"); return 0; }

'''

def run_tests():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("retail", type=Path, help="clean GoldenEye USA .z64")
    parser.add_argument("plus", type=Path, help="goldeneye_007_plus.z64 fixture")
    parser.add_argument("pd_rev1", type=Path, help="clean Perfect Dark USA Rev 1 .z64")
    parser.add_argument("--sanitize", action="store_true", help="enable AddressSanitizer and UndefinedBehaviorSanitizer")
    args = parser.parse_args()
    for path in (args.retail, args.plus, args.pd_rev1):
        with path.open("rb") as handle:
            if handle.read(4) != bytes.fromhex("80371240"):
                parser.error(f"{path}: expected a big-endian .z64 image")
    rom = args.pd_rev1.read_bytes()
    if rom[0x10:0x18] != bytes.fromhex("41f2b98fb458b466"):
        parser.error("The PD fixture must be original Perfect Dark USA Rev 1.")
    lib = rom[0x1050:0x3050] + zlib.decompress(rom[0x3055:], -15)
    game = bytearray()
    for index in range(0, 0x4000, 4):
        offset = 0x4fc40 + int.from_bytes(rom[0x4fc40+index:0x4fc44+index], "big") + 2
        if rom[offset:offset+2] != b"\x11\x73":
            break
        part = zlib.decompress(rom[offset+5:offset+0x1000], -15)
        game.extend(part)
        if len(part) != 0x1000:
            break
    if len(lib) != 0x58f90 or len(game) != 0x1b99e0:
        parser.error("Unexpected PD decompressed segment sizes.")
    repo = Path(__file__).resolve().parents[1]
    src = (repo / "win32/Wingui.c").read_text(encoding="latin1")
    start = src.index("#define PD_frameratecal")
    code = src[start:src.index("void SetCodeCheckMethod", start)]
    compiler = shlex.split(os.environ.get("CC", "cc"))
    with tempfile.TemporaryDirectory(prefix="gepd-patch-test-") as directory:
        temp = Path(directory)
        (temp / "pd_lib.bin").write_bytes(lib)
        (temp / "pd_game.bin").write_bytes(game)
        extracted = temp / "patch_test.c"
        extracted.write_text(preamble + code + main)
        executable = temp / ("patch_test.exe" if os.name == "nt" else "patch_test")
        flags = ["-std=c99", "-O2", "-Wall", "-Wextra", "-Wno-unused-const-variable"]
        if args.sanitize:
            flags += ["-fsanitize=address,undefined"]
        subprocess.run(compiler + flags + [str(extracted), "-o", str(executable)], check=True)
        subprocess.run([str(executable), str(args.retail.resolve()), str(args.plus.resolve()),
                        str(temp / "pd_lib.bin"), str(temp / "pd_game.bin")], check=True)

if __name__ == "__main__":
    run_tests()
