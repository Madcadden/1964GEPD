#!/usr/bin/env python3
"""Test production PD speed resolution and dispatch with user-supplied ROMs.

Usage: python tests/test_pd_speed_compat.py NTSC-beta.rom PAL-beta.rom retail.z64 [EC-beta.rom]
Accepts z64, v64 and n64 byte order. No ROM is distributed with this test.
The test compiles the actual resolver and Wingui functions, then checks them
against decompressed library code. It does not claim interactive emulation.
"""
from pathlib import Path
import subprocess
import sys
import tempfile
import zlib

source = Path(__file__).resolve().parents[1]
wingui = (source / 'win32/Wingui.c').read_text()

def function(name):
    a = wingui.index('void ' + name + '(')
    b = wingui.index('{', a)
    depth = 1
    end = b + 1
    while depth:
        depth += (wingui[end] == '{') - (wingui[end] == '}')
        end += 1
    return wingui[a:end]

a = wingui.index('\temustatus.game_hack = GHACK_NONE;', wingui.index('void PrepareBeforePlay('))
b = wingui.index('\n\tinit_whole_mem_func_array()', a)
classification = wingui[a:b]

harness = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "PerfectDarkSpeedCompat.h"
#define GHACK_NONE 0
#define GHACK_GE 1
#define GHACK_PD 2
#define TV_SYSTEM_PAL 0
#define TV_SYSTEM_NTSC 1
#define TRUE 1
#define FALSE 0
struct {int game_hack;} emustatus;
struct {int TV_System;} rominfo;
struct {int GEFiringRateHack, OverclockFactor, GEDisableHeadRoll, PDSpeedHack, UsingRspPlugin;} emuoptions;
struct {char Game_Name[256];} currentromoptions;
struct GEResolution {int gamevalid;} geResolution;
static struct GEResolution *GEGetHackResolution(void) {return &geResolution;}
static char *strnstr(char *hay, const char *needle, int n) {(void)n; return strstr(hay,needle);}
static unsigned int ram[0x800000/4];
static unsigned int *gMS_RDRAM=ram;
static unsigned int current_rdram_size=sizeof(ram);
static unsigned int pdSpeedSite, pdSpeedWords, pdSpeedBranch, pdSpeedContext[PD_SPEED_MAX_WORDS];
static int writes, geCalls, headCalls;
static int gepdGameEntryReached=1, gepdPatchesPending;
#define LOAD_UWORD_PARAM(addr) ram[((addr)-0x80000000U)/4]
static void GEPDWriteRAMCode(unsigned int address, unsigned int word) {
 assert(address>=0x80000000U && address-0x80000000U<=current_rdram_size-4);
 LOAD_UWORD_PARAM(address)=word; writes++;
}
static int InterlockedExchange(int *a, int b) {int old=*a; *a=b; return old;}
static void GEReconcileNativeEditorReturn(void) {geCalls++;}
static void GEReconcileEditorTextures(void) {geCalls++;}
static void GEFiringRateHack(void) {geCalls++;}
static void GEDisableHeadRoll(void) {geCalls++;}
static void PDDisableHeadRoll(void) {headCalls++;}
'''+function('PDSpeedHack')+'\n'+function('GEPDApplyPendingHacks')+'''
static void classify(void) {
'''+classification+r'''
}
static void reset(void) {
 memset(ram,0,sizeof(ram)); pdSpeedSite=pdSpeedWords=pdSpeedBranch=0;
 current_rdram_size=sizeof(ram); writes=geCalls=headCalls=0;
 emustatus.game_hack=GHACK_PD;
}
int main(int argc,char **argv) {
 unsigned int *input, count, i, n, found[32], contextOffset, branch, mutated;
 PD_SPEED_RESOLUTION resolved;
 FILE *file;
 assert(argc==2);
 file=fopen(argv[1],"rb"); assert(file);
 fseek(file,0,SEEK_END); n=(unsigned int)ftell(file); rewind(file);
 input=malloc(n); assert(input); assert(fread(input,1,n,file)==n); fclose(file);
 assert(n+0x1000<sizeof(ram));
 reset(); memcpy(ram+0x1000/4,input,n);
 resolved=PDResolveSpeedPatch(ram,sizeof(ram));
 assert(resolved.context);
 contextOffset=resolved.context-0x80000000U;
 count=resolved.words; branch=resolved.branch;
 assert(count==27 || count==32);
 memcpy(found,ram+contextOffset/4,count*4);
 /* Every variant must branch to the original load-ra/add-sp/jr/nop epilogue. */
 assert(3+1+(branch&0xffff)==count-4);
 assert(found[count-4]==0x8FBF0014 && found[count-3]==0x27BD0018 && found[count-2]==0x03E00008 && found[count-1]==0);
 PDSpeedHack(); assert(writes==1 && pdSpeedSite==resolved.context+12);
 for(i=0;i<n/4;i++) assert(ram[0x1000/4+i] == (0x1000+i*4==contextOffset+12 ? branch : input[i]));
 PDSpeedHack(); assert(writes==1); /* cached/idempotent */
 LOAD_UWORD_PARAM(pdSpeedSite)=found[3]; PDSpeedHack(); assert(writes==2); /* loaded state restored code */
 LOAD_UWORD_PARAM(pdSpeedSite+4)=1; PDSpeedHack(); assert(writes==2 && pdSpeedSite==0); /* refuse changed context */
 for(mutated=0;mutated<2;mutated++) {
  reset(); memcpy(ram+0x6540/4,found,count*4);
  if(mutated) { /* Real block relocated, plus linked call/global operands. */
   const unsigned int *mask=count==32?pdspeedbetamask:pdspeedmask;
   for(i=0;i<count;i++) ram[0x6540/4+i]^=0x1234&~mask[i];
  }
  PDSpeedHack(); assert(writes==1 && pdSpeedSite==0x8000654cU && pdSpeedBranch==branch);
 }
 reset(); memcpy(ram+0x1230/4,found,count*4); memcpy(ram+0x7650/4,found,count*4);
 PDSpeedHack(); assert(writes==0); /* ambiguous same variant */
 reset(); memcpy(ram+0x1230/4,found,count*4);
 memcpy(ram+0x7650/4,count==32?pdspeedpattern:pdspeedbetapattern,(count==32?27:32)*4);
 PDSpeedHack(); assert(writes==0); /* ambiguous different variants */
 for(i=0;i<count;i++) {
  const unsigned int *mask=count==32?pdspeedbetamask:pdspeedmask;
  reset(); memcpy(ram+0x1230/4,found,count*4);
  ram[0x1230/4+i]^=(mask[i]&0x80000000U)?0x80000000U:0x04000000U;
  PDSpeedHack(); assert(writes==0); /* every instruction's structural mask matters */
 }
 reset(); memcpy(ram+0x1230/4,found,count*4);
 current_rdram_size=0x1230+count*4-4; PDSpeedHack(); assert(writes==0);
 current_rdram_size=0; PDSpeedHack(); assert(writes==0);
 reset(); memcpy(ram+0x1230/4,found,count*4);
 emustatus.game_hack=GHACK_GE; PDSpeedHack(); assert(writes==0);
 /* Exercise actual production dispatch on PAL and preserve GE's NTSC gate. */
 reset(); memcpy(ram+0x1230/4,found,count*4); rominfo.TV_System=TV_SYSTEM_PAL;
 emuoptions.OverclockFactor=18; emuoptions.PDSpeedHack=1; emuoptions.GEDisableHeadRoll=1;
 gepdPatchesPending=1; GEPDApplyPendingHacks(); assert(writes==1 && headCalls==1 && geCalls==0);
 emustatus.game_hack=GHACK_GE; gepdPatchesPending=1; GEPDApplyPendingHacks(); assert(geCalls==0);
 rominfo.TV_System=TV_SYSTEM_NTSC; gepdPatchesPending=1; GEPDApplyPendingHacks(); assert(geCalls==3);
 reset(); memcpy(ram+0x1230/4,found,count*4); gepdGameEntryReached=0;
 gepdPatchesPending=1; GEPDApplyPendingHacks(); assert(writes==0); gepdGameEntryReached=1;
 strcpy(currentromoptions.Game_Name,"Perfect Dark"); rominfo.TV_System=TV_SYSTEM_PAL;
 emuoptions.UsingRspPlugin=1; classify(); assert(emustatus.game_hack==GHACK_PD && emuoptions.UsingRspPlugin==0);
 strcpy(currentromoptions.Game_Name,"GOLDENEYE"); rominfo.TV_System=TV_SYSTEM_NTSC;
 classify(); assert(emustatus.game_hack==GHACK_GE && emuoptions.UsingRspPlugin==1);
 strcpy(currentromoptions.Game_Name,"Unrelated title"); rominfo.TV_System=TV_SYSTEM_PAL;
 classify(); assert(emustatus.game_hack==GHACK_NONE);
 printf("PASS %s: actual %u-word block at lib+0x%x, branch0x%08x; relocation, ambiguity, corruption, bounds, cache, PAL/GE dispatch\n", argv[1],count,contextOffset-0x1000,branch);
 free(input); return 0;
}
'''

def lib_from_rom(path):
    rom = Path(path).read_bytes()
    if rom[:4] == bytes.fromhex('37804012'):
        rom = b''.join(rom[i:i+2][::-1] for i in range(0,len(rom),2))
    elif rom[:4] == bytes.fromhex('40123780'):
        rom = b''.join(rom[i:i+4][::-1] for i in range(0,len(rom),4))
    assert rom[:4] == bytes.fromhex('80371240'), path
    assert rom[0x3050:0x3052] == b'\x11\x73', path
    return rom[0x1050:0x3050]+zlib.decompress(rom[0x3055:],-15)

if len(sys.argv)<4:
    raise SystemExit(__doc__)
with tempfile.TemporaryDirectory(prefix='pd-speed-') as temporary:
    temp=Path(temporary)
    (temp/'test.c').write_text(harness)
    subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-I',str(source/'win32'),str(temp/'test.c'),'-o',str(temp/'test')],check=True)
    for index,path in enumerate(sys.argv[1:]):
        lib=lib_from_rom(path)
        # Host little-endian word representation used by the production emulator.
        fixture=temp/(str(index)+'-lib.bin')
        fixture.write_bytes(b''.join(lib[i:i+4][::-1] for i in range(0,len(lib),4)))
        print('ROM:',Path(path).name,flush=True)
        subprocess.run([str(temp/'test'),str(fixture)],check=True)
