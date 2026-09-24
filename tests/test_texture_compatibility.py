#!/usr/bin/env python3
"""Verify exact GoldenEye Plus texture compatibility correction integration.

Supply a legally held, original Plus ROM. The script creates synthetic host
buffers and executes the extracted production compatibility code only; it never
executes guest instructions. No ROM, captured save, or map is included.
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

PATCHES = [(2153935364, '29c10bb8', '29c10a8a'), (2153948520, '2a010bb8', '2a010a8a'), (2153948524, '24100bb7', '24100a89'), (2153948528, '2a010bb8', '2a010a8a'), (2154757848, '27bdffd8afb20020afb0001800a09025afbf0024afb1001c18a0000800008025008088250c1bc93b2404000126100001263100011612fffba222ffff8fbf00248fb000188fb1001c8fb2002003e0000827bd0028', '27bdffe0afbf001cafb00018afb1001424080018168800020080802103c5802118a0000700a088210c1bc93b24040001a20200002631ffff1620fffb261000018fbf001c8fb000188fb1001403e0000827bd0020'), (2154761176, '03203025', '00193042'), (2154761204, '000e788001ee7823', '01c0782100000000')]

CASES = r'''#define ROM_DELTA 0x805cb4d0U
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
    invalidations=pageinvalidations=0;gHWS_pc=0x806e1a50;memset(gHWS_COP0Reg,0,sizeof gHWS_COP0Reg);memset(gHWS_GPR,0,sizeof gHWS_GPR);
    memset(TLB_sDWORD_R,0,sizeof TLB_sDWORD_R);cases++;
}
static void apply_texture(void) { gepdGameEntryReached=TRUE; GEReconcileEditorTextures(); }
static void assert_nothing_changed(void) {
    unsigned char *rom=malloc(gAllocationLength?gAllocationLength:1),*ram=malloc(sizeof test_rdram);unsigned int n=gAllocationLength;assert(rom&&ram);
    if(n) memcpy(rom,gMemoryState.ROM_Image,n);memcpy(ram,test_rdram,sizeof test_rdram);invalidations=pageinvalidations=0;
    apply_texture();
    assert(!n || !memcmp(rom,gMemoryState.ROM_Image,n));assert(!memcmp(ram,test_rdram,sizeof test_rdram));assert(!invalidations && !pageinvalidations);
    free(rom);free(ram);
}
static void expected_patch(unsigned char *rom,unsigned char *ram) {
    unsigned int i;for(i=0;i<sizeof expected/sizeof expected[0];i++) {
        *(unsigned int *)(rom+expected[i].ram-ROM_DELTA)=expected[i].patched;
        *(unsigned int *)(ram+(expected[i].ram&0x7fffffU))=expected[i].patched;
    }
}
static void assert_patched(void) {
    assert(!memcmp(gMemoryState.ROM_Image,patched_rom,original_rom_length));
    assert(!memcmp(test_rdram,patched_ram,sizeof test_rdram));
}
static void dump_payload(const char *path,BOOL ram) {
    FILE *f=fopen(path,"wb"); unsigned int i;assert(f);
    for(i=0;i<sizeof expected/sizeof expected[0];i++) {
        unsigned int v=ram?LOAD_UWORD_PARAM(expected[i].ram):GEReadROMWord(expected[i].ram-ROM_DELTA);
        unsigned char b[4]={v>>24,v>>16,v>>8,v};assert(fwrite(b,1,4,f)==4);
    }
    fclose(f);
}
int main(int argc,char **argv) {
    unsigned int n,i,changed=0;assert(argc==5);
    read_fixture(argv[1],&original_rom,&original_rom_length);read_fixture(argv[2],&original_ram,&n);assert(n==0x800000);
    patched_rom=malloc(original_rom_length);patched_ram=malloc(n);assert(patched_rom&&patched_ram);
    memcpy(patched_rom,original_rom,original_rom_length);memcpy(patched_ram,original_ram,n);expected_patch(patched_rom,patched_ram);
    for(i=0;i<sizeof expected/sizeof expected[0];i++) {
        assert(*(unsigned int *)(original_rom+expected[i].ram-ROM_DELTA)==expected[i].original);
        assert(*(unsigned int *)(original_ram+(expected[i].ram&0x7fffffU))==expected[i].original);
        changed+=expected[i].original!=expected[i].patched;
    }
    reset_case();GEReconcileEditorTextures();
    assert(!memcmp(gMemoryState.ROM_Image,original_rom,original_rom_length));assert(!memcmp(test_rdram,original_ram,n));assert(!invalidations);
    apply_texture();assert_patched();assert(invalidations==changed && pageinvalidations==changed*2);
    dump_payload(argv[3],FALSE);dump_payload(argv[4],TRUE);
    /* Idempotence and restoring an old save do not alter any editor/map data. */
    invalidations=pageinvalidations=0;apply_texture();assert_patched();assert(!invalidations&&!pageinvalidations);
    memcpy(test_rdram,original_ram,n);apply_texture();assert_patched();assert(invalidations==changed);
    /* Full ownership group restores exactly, and a prepatched cartridge is not owned. */
    GEPDRestoreROMHacks();assert(!memcmp(gMemoryState.ROM_Image,original_rom,original_rom_length));
    GEPDResetHackResolution();apply_texture();assert_patched();GEPDRestoreROMHacks();assert(!memcmp(gMemoryState.ROM_Image,original_rom,original_rom_length));
    reset_case();memcpy(gMemoryState.ROM_Image,patched_rom,original_rom_length);memcpy(test_rdram,patched_ram,n);
    assert_nothing_changed();GEPDRestoreROMHacks();assert_patched();
    /* An external mutation prevents restoration of every word in the group. */
    reset_case();apply_texture();i=expected[14].ram-ROM_DELTA;GEWriteROMWord(i,GEReadROMWord(i)^1U);
    GEPDRestoreROMHacks();GEWriteROMWord(i,GEReadROMWord(i)^1U);assert_patched();
    /* Cold boot patches only the verified cartridge before code is resident. */
    reset_case();memset(test_rdram,0,n);apply_texture();assert(!memcmp(gMemoryState.ROM_Image,patched_rom,original_rom_length));
    assert(!invalidations&&!pageinvalidations);for(i=0;i<n;i++) assert(test_rdram[i]==0);
    /* Any single changed word from the other version is rejected atomically. */
    for(i=0;i<sizeof expected/sizeof expected[0];i++) if(expected[i].original!=expected[i].patched) {
        unsigned int rom=expected[i].ram-ROM_DELTA,ram=expected[i].ram;
        reset_case();GEWriteROMWord(rom,expected[i].patched);assert_nothing_changed();
        reset_case();memcpy(gMemoryState.ROM_Image,patched_rom,original_rom_length);GEWriteROMWord(rom,expected[i].original);assert_nothing_changed();
        reset_case();memcpy(gMemoryState.ROM_Image,patched_rom,original_rom_length);LOAD_UWORD_PARAM(ram)=expected[i].patched;assert_nothing_changed();
    }
    /* Context edges, not just modified words, must match in ROM and live RAM. */
    for(i=0;i<sizeof geTextureContexts/sizeof geTextureContexts[0];i++) {
        unsigned int edge;for(edge=0;edge<2;edge++) {
            unsigned int ram=geTextureContexts[i].ram+(edge?(geTextureContexts[i].words-1)*4:0),rom=ram-ROM_DELTA;
            reset_case();GEWriteROMWord(rom,GEReadROMWord(rom)^1U);assert_nothing_changed();
            reset_case();memcpy(gMemoryState.ROM_Image,patched_rom,original_rom_length);LOAD_UWORD_PARAM(ram)^=1U;assert_nothing_changed();
        }
    }
    /* Immutable compressed data identifies this texture count, independently of code. */
    reset_case();GEWriteROMWord(0x21990U,GEReadROMWord(0x21990U)^1U);assert_nothing_changed();
    reset_case();GEWriteROMWord(0x21990U+70388U-4U,GEReadROMWord(0x21990U+70388U-4U)^1U);assert_nothing_changed();
    /* Physical and virtual compiled aliases are invalidated for every changed word. */
    reset_case();TLB_sDWORD_R[0x70001]=test_rdram+((expected[0].ram&0x7fffffU)&~0xfffU);apply_texture();assert_patched();
    assert(invalidations==changed && pageinvalidations>2*changed);
    /* The real VI/save-resume dispatcher runs both independent compatibility groups. */
    reset_case();gepdGameEntryReached=TRUE;GEReconcileNativeEditorReturn();
    {
        unsigned char *expected_rom=malloc(original_rom_length),*expected_ram=malloc(n);assert(expected_rom&&expected_ram);
        memcpy(expected_rom,gMemoryState.ROM_Image,original_rom_length);memcpy(expected_ram,test_rdram,n);
        expected_patch(expected_rom,expected_ram);invalidations=pageinvalidations=0;
        GEPDQueueRuntimeHacks();GEPDApplyPendingHacks();
        assert(!memcmp(gMemoryState.ROM_Image,expected_rom,original_rom_length));assert(!memcmp(test_rdram,expected_ram,n));
        assert(invalidations==changed && pageinvalidations==2*changed);
        GEPDRestoreROMHacks();assert(!memcmp(gMemoryState.ROM_Image,original_rom,original_rom_length));
        free(expected_rom);free(expected_ram);
    }
    /* A suspended or interrupted helper must complete before its frame changes. */
    for(i=0;i<11;i++) {
        unsigned int first;
        reset_case();memcpy(gMemoryState.ROM_Image,patched_rom,original_rom_length);
        first=LOAD_UWORD_PARAM(0x8002772CU);
        switch(i) {
        case 0:gHWS_pc=0x806EFED8U;break;
        case 1:gHWS_pc=0xA06F24ECU;break;
        case 2:gHWS_GPR[31]=0x006EFF08U;break;
        case 3:gHWS_COP0Reg[STATUS]=2;gHWS_COP0Reg[EPC]=0x806F0240U;break;
        case 4:LOAD_UWORD_PARAM(first+0x11CU)=0x8062A568U;break;
        case 5:LOAD_UWORD_PARAM(first+0x104U)=0x806EFED8U;break;
        case 6:LOAD_UWORD_PARAM(first+0x0CU)=first;break;
        case 7:LOAD_UWORD_PARAM(0x8002772CU)=0x807FFF00U;break;
        case 8:LOAD_UWORD_PARAM(0x80027730U)=0x80123458U;break;
        case 9:LOAD_UWORD_PARAM(0x80027724U)=0;break;
        case 10:LOAD_UWORD_PARAM(0x8002772CU)=first+4;break;
        }
        assert_nothing_changed();assert(gepdPatchesPending==1);
        memcpy(test_rdram,original_ram,n);gHWS_pc=0x806e1a50;memset(gHWS_COP0Reg,0,sizeof gHWS_COP0Reg);memset(gHWS_GPR,0,sizeof gHWS_GPR);
        apply_texture();assert_patched();assert(invalidations==changed);
    }
    /* Saved PC/RA offsets are admitted only with the known scheduler implementation. */
    for(i=0;i<sizeof geTextureThreadContexts/sizeof geTextureThreadContexts[0];i++) {
        unsigned int edge;for(edge=0;edge<2;edge++) {
            const GE_EDITOR_CONTEXT *c=&geTextureThreadContexts[i];unsigned int d=edge?(c->words-1)*4:0;
            reset_case();memcpy(gMemoryState.ROM_Image,patched_rom,original_rom_length);GEWriteROMWord(c->rom+d,GEReadROMWord(c->rom+d)^1U);assert_nothing_changed();
            reset_case();memcpy(gMemoryState.ROM_Image,patched_rom,original_rom_length);LOAD_UWORD_PARAM(c->ram+d)^=1U;assert_nothing_changed();
        }
    }
    reset_case();memcpy(gMemoryState.ROM_Image,patched_rom,original_rom_length);LOAD_UWORD_PARAM(0x8004EBF8U)^=1U;assert_nothing_changed();
    reset_case();memcpy(gMemoryState.ROM_Image,patched_rom,original_rom_length);LOAD_UWORD_PARAM(0x8004EC00U)^=1U;assert_nothing_changed();
    /* Runtime identity, region, and allocation gates run before mutation. */
    reset_case();emustatus.game_hack=GHACK_PD;assert_nothing_changed();
    reset_case();rominfo.TV_System=1;assert_nothing_changed();
    reset_case();current_rdram_size=0x400000;assert_nothing_changed();
    reset_case();gMS_RDRAM=NULL;assert_nothing_changed();gMS_RDRAM=test_rdram;
    reset_case();geResolutionInitialized=TRUE;geResolution.rommappingvalid=TRUE;assert_nothing_changed();
    for(i=0;i<3;i++) {
        reset_case();GEPDRestoreROMHacks();free(gMemoryState.ROM_Image);gAllocationLength=i==0?0:i==1?16:0x32c84;
        gMemoryState.ROM_Image=i==0?NULL:calloc(1,gAllocationLength);assert(!i||gMemoryState.ROM_Image);assert_nothing_changed();
    }
    printf("Texture compatibility: %u host cases passed; exact payload, atomic admission, identity, old-save RAM, idempotence, ownership restoration, cold boot, bounded mutation and cache aliases\n",cases);
    GEPDRestoreROMHacks();free(gMemoryState.ROM_Image);free(original_rom);free(original_ram);free(patched_rom);free(patched_ram);return 0;
}
'''

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('plus',type=Path,help='User-supplied original canonical .z64; never modified')
    ap.add_argument('--ram',type=Path,help='Optional canonical 8 MiB private RDRAM fixture')
    ap.add_argument('--source',type=Path,required=True,help='Emulator source root containing win32/Wingui.c')
    ap.add_argument('--sanitize',action='store_true')
    args=ap.parse_args()
    source_bytes=(args.source/'win32/Wingui.c').read_bytes();src=source_bytes.decode('latin1')
    start=src.index('#define PD_frameratecal');code=src[start:src.index('void SetCodeCheckMethod',start)]
    rom=args.plus.read_bytes();assert rom[:4]==bytes.fromhex('80371240'),'Expected canonical .z64'
    if args.ram:
        ram=args.ram.read_bytes();assert len(ram)==0x800000
    else:
        ram=bytearray(0x800000)
        ram[0x600000:0x800000]=rom[0x34b30:0x234b30]
        for address,value in [(0x52c98,0x80620e74),(0x241a8,0x5a),(0x2a8f0,11),(0x2a8f4,12),(0x6e040,1),(0x6e048,1)]:
            struct.pack_into('>I',ram,address,value)
        ram[0x400000:0x43e888]=bytes((i*17+3)&255 for i in range(0x3e888))
        # Known scheduler windows and a single safe synthetic active thread.
        for rom_start,ram_start,length in [(0xdfd8,0xd3d8,40),(0x10c48,0x10048,24),(0x10ce0,0x100e0,16),(0x10d60,0x10160,16),(0x11258,0x10658,20)]:
            ram[ram_start:ram_start+length]=rom[rom_start:rom_start+length]
        for address,value in [(0x27720,0),(0x27724,0xffffffff),(0x2772c,0x80300000),(0x27730,0x80300000),(0x30000c,0x80027720),(0x300104,0x806e1a50),(0x30011c,0x806e1a50),(0x4ebf8,0x002ee9db),(0x4ec00,0x002eef18)]:
            struct.pack_into('>I',ram,address,value)
    words=[]
    for address,old,new in PATCHES:
        old=bytes.fromhex(old);new=bytes.fromhex(new);assert len(old)==len(new)
        for i in range(0,len(old),4):
            words.append((address+i,int.from_bytes(old[i:i+4],'big'),int.from_bytes(new[i:i+4],'big')))
    expected='static const struct {unsigned int ram,original,patched;} expected[] = {\n'
    expected+=''.join('    {0x%08xU,0x%08xU,0x%08xU},\n'%w for w in words)+'};\n'
    with tempfile.TemporaryDirectory(prefix='texture-compat-test-') as d:
        d=Path(d);(d/'ram.bin').write_bytes(ram);c=d/'test.c';c.write_text(PREAMBLE+code+expected+CASES)
        exe=d/'test';flags=['-std=c99','-O1','-Wall','-Wextra','-Wno-unused-const-variable']
        if args.sanitize:flags+=['-fsanitize=address,undefined']
        subprocess.run(shlex.split(os.environ.get('CC','cc'))+flags+[str(c),'-o',str(exe)],check=True)
        run=subprocess.run([str(exe),str(args.plus.resolve()),str(d/'ram.bin'),str(d/'rom-payload.bin'),str(d/'ram-payload.bin')],capture_output=True,text=True)
        assert run.returncode==0,run.stderr
        hashes={name:hashlib.sha256((d/name).read_bytes()).hexdigest() for name in ['rom-payload.bin','ram-payload.bin']}
        assert set(hashes.values())=={'b2b60b99bb3ec79c5ff7c464b02ed203f0b40cf6a1473c2a662f341534e13796'},'Production bytes differ from the user-confirmed correction'
        print(json.dumps({'source_sha256':hashlib.sha256(source_bytes).hexdigest(),'rom_sha256':hashlib.sha256(rom).hexdigest(),'payload_sha256':hashes,'sanitizers':args.sanitize,'result':run.stdout.strip(),'limits':'Production host C runs against bounded buffers; no guest gameplay or emulator scheduler replay. No game data is distributed.'},indent=2))
if __name__=='__main__':main()
