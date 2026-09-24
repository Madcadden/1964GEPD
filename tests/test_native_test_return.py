#!/usr/bin/env python3
"""Verify production GoldenEye Plus Native Test return compatibility repair.

Usage: python tests/test_native_test_return.py plus.z64 [--retail retail.z64]
Uses actual supplied ROM instructions and bounded simulated RDRAM; does not
execute guest gameplay. No ROM or save is included. Optional --ram accepts a
canonical big-endian 8 MiB captured RDRAM fixture for local checks.
"""
from pathlib import Path
import argparse, hashlib, json, os, shlex, struct, subprocess, tempfile

PREAMBLE = r'''
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
static unsigned char test_rdram[0x800000];
static unsigned char *gMS_RDRAM=test_rdram;
static unsigned int current_rdram_size = sizeof test_rdram;
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

#define STATUS 12
#define EPC 14
static unsigned int gHWS_pc, gHWS_COP0Reg[32];
static unsigned long long gHWS_GPR[32];
'''

CASES = r'''#define TITLE_ROM 0x00035850U
#define TITLE_RAM 0x00600D20U
#define TITLE_SIZE 284U
static unsigned char *original_rom, *original_ram, *patched_rom, *patched_ram;
static unsigned int original_rom_length, cases;
static void read_fixture(const char *path, unsigned char **out, unsigned int *size) {
    FILE *f=fopen(path,"rb"); long n; unsigned int i; assert(f);
    fseek(f,0,SEEK_END); n=ftell(f); rewind(f); assert(n>0 && !(n&3));
    *out=malloc(n); assert(*out); assert(fread(*out,1,n,f)==(size_t)n); fclose(f); *size=n;
    for(i=0;i<(unsigned int)n;i+=4) { unsigned char *b=*out+i,t=b[0];b[0]=b[3];b[3]=t;t=b[1];b[1]=b[2];b[2]=t; }
}
static void reset_case(void) {
    gMS_RDRAM=test_rdram;
    GEPDRestoreROMHacks();
    if(gMemoryState.ROM_Image) free(gMemoryState.ROM_Image);
    gMemoryState.ROM_Image=malloc(original_rom_length);assert(gMemoryState.ROM_Image);
    memcpy(gMemoryState.ROM_Image,original_rom,original_rom_length);gAllocationLength=original_rom_length;
    memcpy(gMS_RDRAM,original_ram,0x800000);current_rdram_size=0x800000;
    GEPDResetHackResolution();
    memset(&emuoptions,0,sizeof emuoptions);rominfo.TV_System=TV_SYSTEM_NTSC;emustatus.game_hack=GHACK_GE;
    invalidations=pageinvalidations=0;gHWS_pc=0x806e1a50;memset(gHWS_COP0Reg,0,sizeof gHWS_COP0Reg);cases++;
}
static void apply_entry(void) { GEPDQueueRuntimeHacks(); GEPDOnGameEntry(); }
static void assert_changed_only(const unsigned char *a,const unsigned char *b,unsigned int size,unsigned int start,unsigned int length) {
    assert(start<=size && length<=size-start);
    assert(!memcmp(a,b,start));assert(!memcmp(a+start+length,b+start+length,size-start-length));
    assert(memcmp(a+start,b+start,length));
}
static void write_window(const char *path,const unsigned char *data) {
    FILE *f=fopen(path,"wb");unsigned int i;assert(f);
    for(i=0;i<TITLE_SIZE;i+=4) { unsigned char b[4]={data[i+3],data[i+2],data[i+1],data[i]}; assert(fwrite(b,1,4,f)==4); }fclose(f);
}
static void assert_no_writes(void) {
    unsigned char *rom=malloc(gAllocationLength),*ram=malloc(sizeof test_rdram);unsigned int n=gAllocationLength;assert(rom&&ram);
    memcpy(rom,gMemoryState.ROM_Image,n);memcpy(ram,gMS_RDRAM,sizeof test_rdram);invalidations=pageinvalidations=0;gepdGameEntryReached=TRUE;
    GEReconcileNativeEditorReturn();
    assert(!memcmp(rom,gMemoryState.ROM_Image,n));assert(!memcmp(ram,gMS_RDRAM,sizeof test_rdram));assert(!invalidations && !pageinvalidations);
    free(rom);free(ram);
}
int main(int argc,char **argv) {
    unsigned int ram_size,i,changed=0,changed_offsets[TITLE_SIZE/4]; assert(argc==5 || argc==6);
    read_fixture(argv[1],&original_rom,&original_rom_length);read_fixture(argv[2],&original_ram,&ram_size);assert(ram_size==0x800000);
    assert(!memcmp(original_rom+TITLE_ROM,original_ram+TITLE_RAM,TITLE_SIZE));
    reset_case();
    /* Queuing and calling the VI dispatcher before boot entry cannot mutate IPL data. */
    GEPDQueueRuntimeHacks();GEPDApplyPendingHacks();
    assert(!memcmp(gMemoryState.ROM_Image,original_rom,original_rom_length));assert(!memcmp(gMS_RDRAM,original_ram,0x800000));assert(!invalidations);
    GEPDOnGameEntry();
    assert_changed_only(original_rom,gMemoryState.ROM_Image,original_rom_length,TITLE_ROM,TITLE_SIZE);
    assert_changed_only(original_ram,gMS_RDRAM,0x800000,TITLE_RAM,TITLE_SIZE);
    assert(!memcmp(gMemoryState.ROM_Image+TITLE_ROM,gMS_RDRAM+TITLE_RAM,TITLE_SIZE));
    patched_rom=malloc(original_rom_length);patched_ram=malloc(0x800000);assert(patched_rom&&patched_ram);
    memcpy(patched_rom,gMemoryState.ROM_Image,original_rom_length);memcpy(patched_ram,gMS_RDRAM,0x800000);
    for(i=0;i<TITLE_SIZE;i+=4) if(memcmp(original_rom+TITLE_ROM+i,patched_rom+TITLE_ROM+i,4)) changed_offsets[changed++]=i;
    assert(invalidations==changed && pageinvalidations==2*changed);
    write_window(argv[3],gMemoryState.ROM_Image+TITLE_ROM);write_window(argv[4],gMS_RDRAM+TITLE_RAM);
    /* Repeat is a no-op; loading an old save after boot requests repair anew. */
    invalidations=pageinvalidations=0;GEPDQueueRuntimeHacks();GEPDApplyPendingHacks();assert(!invalidations&&!pageinvalidations);
    memcpy(gMS_RDRAM,original_ram,0x800000);GEPDQueueRuntimeHacks();assert(!memcmp(gMS_RDRAM,original_ram,0x800000));
    GEPDApplyPendingHacks();assert(!memcmp(gMS_RDRAM,patched_ram,0x800000));assert(invalidations==changed && pageinvalidations==2*changed);
    /* Whole-group ownership restores Stop/Play exactly. */
    GEPDRestoreROMHacks();assert(!memcmp(gMemoryState.ROM_Image,original_rom,original_rom_length));
    GEPDResetHackResolution();apply_entry();assert(!memcmp(gMemoryState.ROM_Image,patched_rom,original_rom_length));
    GEPDRestoreROMHacks();assert(!memcmp(gMemoryState.ROM_Image,original_rom,original_rom_length));
    /* A ROM that already contains the tested repair is never owned/restored. */
    reset_case();memcpy(gMemoryState.ROM_Image,patched_rom,original_rom_length);memcpy(gMS_RDRAM,patched_ram,0x800000);
    apply_entry();assert(!invalidations&&!pageinvalidations);GEPDRestoreROMHacks();assert(!memcmp(gMemoryState.ROM_Image,patched_rom,original_rom_length));
    /* External mutation means the complete owned ROM group must be preserved. */
    reset_case();apply_entry();GEWriteROMWord(TITLE_ROM+12,GEReadROMWord(TITLE_ROM+12)^1);GEPDRestoreROMHacks();
    for(i=0;i<TITLE_SIZE;i+=4) if(i!=12) assert(!memcmp(gMemoryState.ROM_Image+TITLE_ROM+i,patched_rom+TITLE_ROM+i,4));
    assert(GEReadROMWord(TITLE_ROM+12)==(*(unsigned int *)(patched_rom+TITLE_ROM+12)^1));
    /* No cold-boot RAM code: patch only the source cartridge; wait for its loader. */
    reset_case();memset(gMS_RDRAM,0,0x800000);apply_entry();assert(!memcmp(gMemoryState.ROM_Image,patched_rom,original_rom_length));assert(!invalidations&&!pageinvalidations);
    for(i=0;i<0x800000;i++) assert(gMS_RDRAM[i]==0);
    /* One-word mixtures from both sides fail atomically at independent changed sites. */
    for(unsigned int sample=0;sample<3;sample++) {
        i=changed_offsets[sample==0?0:sample==1?changed/2:changed-1];
        reset_case();memcpy(gMemoryState.ROM_Image+TITLE_ROM+i,patched_rom+TITLE_ROM+i,4);assert_no_writes();
        reset_case();memcpy(gMemoryState.ROM_Image,patched_rom,original_rom_length);memcpy(gMemoryState.ROM_Image+TITLE_ROM+i,original_rom+TITLE_ROM+i,4);assert_no_writes();
    }
    /* A modified title word cannot produce partial writes, even if all other signatures match. */
    for(i=0;i<TITLE_SIZE;i+=4) if(i==0 || i==TITLE_SIZE/2/4*4 || i==TITLE_SIZE-4) {
        reset_case();GEWriteROMWord(TITLE_ROM+i,GEReadROMWord(TITLE_ROM+i)^1);assert_no_writes();
    }
    /* RAM mismatches leave all RAM intact while the separately verified ROM can remain fixed. */
    for(i=0;i<TITLE_SIZE;i+=4) if(i==0 || i==TITLE_SIZE/2/4*4 || i==TITLE_SIZE-4) {
        reset_case();memcpy(gMemoryState.ROM_Image,patched_rom,original_rom_length);
        *(unsigned int *)(gMS_RDRAM+TITLE_RAM+i)^=1;assert_no_writes();
    }
    /* Every independent code dependency must agree in ROM and in resident RAM. */
    for(i=0;i<sizeof geeditorcontexts/sizeof geeditorcontexts[0];i++) {
        unsigned int edge;
        for(edge=0;edge<2;edge++) {
            const GE_EDITOR_CONTEXT *c=&geeditorcontexts[i];unsigned int d=edge?(c->words-1)*4:0;
            reset_case();GEWriteROMWord(c->rom+d,GEReadROMWord(c->rom+d)^1);assert_no_writes();
            reset_case();memcpy(gMemoryState.ROM_Image,patched_rom,original_rom_length);
            LOAD_UWORD_PARAM(c->ram+d)^=1;assert_no_writes();
        }
    }
    reset_case();memcpy(gMemoryState.ROM_Image,patched_rom,original_rom_length);
    LOAD_UWORD_PARAM(0x80052c98)^=4;assert_no_writes();
    /* Old 4 MiB save memory cannot admit a physical function beyond its bound. */
    reset_case();memcpy(gMemoryState.ROM_Image,patched_rom,original_rom_length);current_rdram_size=0x400000;assert_no_writes();
    /* Never replace a function while the CPU is inside it, including an interrupted call. */
    for(i=0;i<4;i++) {
        reset_case();memcpy(gMemoryState.ROM_Image,patched_rom,original_rom_length);
        if(i==0) gHWS_pc=0x80600d20;
        if(i==1) gHWS_pc=0x80600e38;
        if(i==2) {gHWS_pc=0x800100f4;gHWS_COP0Reg[STATUS]=2;gHWS_COP0Reg[EPC]=0x80600d80;}
        if(i==3) {gHWS_pc=0x80634a00;LOAD_UWORD_PARAM(0x800241a8)=0x5a;LOAD_UWORD_PARAM(0x8002a8f0)=0xffffffff;}
        assert_no_writes();
        gHWS_pc=0x806e1a50;gHWS_COP0Reg[STATUS]=0;LOAD_UWORD_PARAM(0x8002a8f0)=11;
        GEReconcileNativeEditorReturn();assert(!memcmp(gMS_RDRAM+TITLE_RAM,patched_ram+TITLE_RAM,TITLE_SIZE));assert(invalidations==changed);
    }
    /* A change elsewhere in a runtime flag is data, not permission to write it. */
    reset_case();LOAD_UWORD_PARAM(0x8006e040)=0;LOAD_UWORD_PARAM(0x8006e048)=0;gepdGameEntryReached=TRUE;GEReconcileNativeEditorReturn();
    assert(LOAD_UWORD_PARAM(0x8006e040)==0 && LOAD_UWORD_PARAM(0x8006e048)==0);
    assert(!memcmp(gMS_RDRAM+0x400000,original_ram+0x400000,0x3e888));
    reset_case();gMS_RDRAM=NULL;gepdGameEntryReached=TRUE;GEReconcileNativeEditorReturn();
    assert(!memcmp(gMemoryState.ROM_Image,original_rom,original_rom_length));gMS_RDRAM=test_rdram;
    /* Bounds guards apply before any ROM word reads, including a null or tiny image. */
    for(i=0;i<3;i++) {
        reset_case();GEPDRestoreROMHacks();free(gMemoryState.ROM_Image);
        gAllocationLength=i==0?0:i==1?16:TITLE_ROM+TITLE_SIZE-4;
        gMemoryState.ROM_Image=i==0?NULL:calloc(1,gAllocationLength);assert(i==0 || gMemoryState.ROM_Image);
        gepdGameEntryReached=TRUE;GEReconcileNativeEditorReturn();assert(!invalidations&&!pageinvalidations);
    }
    if(argc==6) {
        unsigned char *other;unsigned int n;read_fixture(argv[5],&other,&n);
        reset_case();GEPDRestoreROMHacks();free(gMemoryState.ROM_Image);gMemoryState.ROM_Image=other;gAllocationLength=n;
        GEPDResetHackResolution();assert_no_writes();
    }
    printf("Native editor return: %u fixture cases; IPL gate, exact original and prepatched windows, old-save reload, bounded writes, invalidation, ownership restoration and partial/malformed windows passed\n",cases);
    GEPDRestoreROMHacks();free(gMemoryState.ROM_Image);free(original_rom);free(original_ram);free(patched_rom);free(patched_ram);return 0;
}
'''

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('plus',type=Path)
    ap.add_argument('--retail',type=Path)
    ap.add_argument('--ram',type=Path)
    ap.add_argument('--source',type=Path)
    ap.add_argument('--sanitize',action='store_true')
    args=ap.parse_args()
    repo=args.source or Path(__file__).resolve().parents[1]
    if not (repo/'win32/Wingui.c').exists():repo=repo/'emulator'
    source_bytes=(repo/'win32/Wingui.c').read_bytes()
    src=source_bytes.decode('latin1')
    start=src.index('#define PD_frameratecal')
    code=src[start:src.index('void SetCodeCheckMethod',start)]
    rom=args.plus.read_bytes()
    assert rom[:4]==bytes.fromhex('80371240'),'Expected canonical .z64'
    assert hashlib.sha256(rom[0x35850:0x3596c]).hexdigest()=='abea804c17835ce376577396a150e9ccc9ca2d2fb7852bd2f395268cc6444bd6','Expected original inspected title initializer'
    if args.ram:
        ram=args.ram.read_bytes();assert len(ram)==0x800000
    else:
        ram=bytearray(0x800000)
        # Plus loads this code directly at 0x80600000, not a paged 0x7f alias.
        ram[0x600000:0x800000]=rom[0x34b30:0x234b30]
        for address,value in [(0x52c98,0x80620e74),(0x241a8,0x5a),(0x2a8f0,11),(0x2a8f4,12),(0x6e040,1),(0x6e048,1)]:
            struct.pack_into('>I',ram,address,value)
        # A user map sentinel proves the code repair does not rewrite editor data.
        ram[0x400000:0x43e888]=bytes((i*17+3)&255 for i in range(0x3e888))
    # Isolate this title-function regression suite from the separately tested
    # texture correction. A changed texture-table identity closes that gate;
    # all title code and context remain the supplied ROM's exact bytes.
    title_rom=bytearray(rom)
    isolate_textures='GEReconcileEditorTextures' in code
    if isolate_textures:title_rom[0x21990]^=1
    with tempfile.TemporaryDirectory(prefix='native-return-test-') as d:
        d=Path(d);(d/'rom.bin').write_bytes(title_rom);(d/'ram.bin').write_bytes(ram);c=d/'test.c';c.write_text(PREAMBLE+code+CASES)
        exe=d/'test';flags=['-std=c99','-O1','-Wall','-Wextra','-Wno-unused-const-variable']
        if args.sanitize:flags+=['-fsanitize=address,undefined']
        subprocess.run(shlex.split(os.environ.get('CC','cc'))+flags+[str(c),'-o',str(exe)],check=True)
        command=[str(exe),str(d/'rom.bin'),str(d/'ram.bin'),str(d/'rom-payload.bin'),str(d/'ram-payload.bin')]
        if args.retail:command.append(str(args.retail.resolve()))
        run=subprocess.run(command,capture_output=True,text=True)
        assert run.returncode==0,run.stderr
        hashes={name:hashlib.sha256((d/name).read_bytes()).hexdigest() for name in ['rom-payload.bin','ram-payload.bin']}
        assert set(hashes.values())=={'0d0ac6cbbb16b0956833fbd891f248d54072600e6cd993757e4c3e395b373651'},'Production payload differs from user-tested repair'
        print(json.dumps({'source_sha256':hashlib.sha256(source_bytes).hexdigest(),'rom_sha256':hashlib.sha256(rom).hexdigest(),'payload_sha256':hashes,'sanitizers':args.sanitize,'texture_gate_isolated':isolate_textures,'result':run.stdout.strip(),'limits':'Host C executes production patch code against bounded ROM/RAM fixtures; user-tested guest-function payload is authenticated by hash. No live emulator replay.'},indent=2))
if __name__=='__main__':main()
