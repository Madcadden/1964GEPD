#!/usr/bin/env python3
"""Exercise production ROM selection and option generation after header repair.

Runs on a host C compiler without Windows or copyrighted ROM data. Windows UI,
allocation and INI storage are isolated; selection, insertion, header decoding,
settings copying and runtime option generation use the production C functions.
"""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
romlist = (root / "romlist.c").read_text()
ini = (root / "1964ini.c").read_text()
fileio = (root / "fileio.c").read_text()
wingui = (root / "win32/Wingui.c").read_text()


def function(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    end, depth = opening + 1, 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


# Playback must reject a missing selection before generating options or hiding UI.
play = function(wingui, "void Play(")
assert play.index("if(!RomListSelectLoadedRomEntry()) return;") < play.index("PrepareBeforePlay(")
load = function(wingui, "BOOL WinLoadRomStep2(")
assert load.index("ReadRomData(") < load.index("RomListSetLoadedRomPath(szFileName);")

harness = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
typedef uint8_t uint8;
typedef uint32_t uint32;
typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define stricmp strcasecmp
#define __try if(1)
#define __except(...) else
#define MAX_ROMLIST 4
#define MEM_COMMIT 1
#define PAGE_READWRITE 1
#define CODE_CHECK_PROTECT_MEMORY 1
#define CODE_CHECK_NONE 2
#define USE4KBLINKBLOCK_YES 1
#define USE4KBLINKBLOCK_NO 2
#define SRAM_SAVETYPE 3
#define ANYUSED_SAVETYPE 1
#define EEPROMSIZE_16KB 2
typedef struct {
 char Game_Name[64], Comments[128], Alt_Title[64];
 uint32 crc1, crc2;
 uint8 countrycode;
 int RDRAM_Size, Emulator, Code_Check, Save_Type, Max_FPS, Use_TLB;
 int Eeprom_size, Counter_Factor, Use_Register_Caching, FPU_Hack;
 int DMA_Segmentation, Link_4KB_Blocks, Advanced_Block_Analysis;
 int Assume_32bit, Use_HLE;
} INI_ENTRY;
typedef struct {
 INI_ENTRY *pinientry;
 char romfilename[256];
 long size;
} ROMLIST_ENTRY;
static ROMLIST_ENTRY *romlist[MAX_ROMLIST];
static INI_ENTRY *ini_entries[16];
static INI_ENTRY currentromoptions, defaultoptions;
static int romlist_count, selected_rom_index, ini_count, errors, fail_alloc;
static char loadedrompath[sizeof(((ROMLIST_ENTRY *)0)->romfilename)];
static unsigned char header[64];
static unsigned char *gMS_ROM_Image = header;
static unsigned int gAllocationLength = 0x4000000;
static struct {unsigned char *ROM_Image;} gMemoryState;
static struct {char name[64];} rominfo;
static void DisplayError(const char *message) {(void)message; ++errors;}
static void *VirtualAlloc(void *addr, size_t size, int kind, int access) {
 (void)addr; (void)kind; (void)access;
 return fail_alloc ? NULL : calloc(1,size);
}
static int GEPlusHas16KSaveDriver(void *p,unsigned int n) {(void)p;(void)n;return 0;}
static void WriteProject64RDB(uint32 a,uint32 b,uint8 c) {(void)a;(void)b;(void)c;}
static char *strnstr(const char *hay,const char *needle,int n) {(void)n;return strstr(hay,needle);}
'''
harness += function(ini, "void CopyIniEntry(") + "\n"
harness += r'''
static int FindIniEntry2(const INI_ENTRY *e) {
 int i;
 for(i=0;i<ini_count;++i)
  if(!strcmp(e->Game_Name,ini_entries[i]->Game_Name) && e->crc1==ini_entries[i]->crc1 &&
     e->crc2==ini_entries[i]->crc2 && e->countrycode==ini_entries[i]->countrycode) return i;
 return -1;
}
static int AddIniEntry(const INI_ENTRY *e) {
 assert(ini_count<16);
 ini_entries[ini_count]=calloc(1,sizeof(*e)); assert(ini_entries[ini_count]);
 CopyIniEntry(ini_entries[ini_count],e); return ini_count++;
}
'''
for source, signature in [
    (fileio, "void SwapRomName("),
    (romlist, "void ReadRomHeaderInMemory("),
    (romlist, "int RomListAddEntry("),
    (romlist, "void RomListSetLoadedRomPath("),
    (romlist, "BOOL RomListSelectLoadedRomEntry("),
    (romlist, "ROMLIST_ENTRY *RomListSelectedEntry("),
    (ini, "void GenerateCurrentRomOptions("),
]:
    harness += function(source, signature) + "\n"

harness += r'''
static void reset(void) {
 int i;
 for(i=0;i<romlist_count;++i) free(romlist[i]);
 for(i=0;i<ini_count;++i) free(ini_entries[i]);
 memset(romlist,0,sizeof(romlist)); memset(ini_entries,0,sizeof(ini_entries));
 romlist_count=ini_count=errors=fail_alloc=0; selected_rom_index=-1;
 memset(&currentromoptions,0,sizeof(currentromoptions));
 memset(&defaultoptions,0,sizeof(defaultoptions));
 defaultoptions.Code_Check=CODE_CHECK_PROTECT_MEMORY;
 defaultoptions.Emulator=7; defaultoptions.RDRAM_Size=8;
 defaultoptions.Save_Type=ANYUSED_SAVETYPE; defaultoptions.Eeprom_size=1;
 defaultoptions.Counter_Factor=1; defaultoptions.Use_TLB=1;
 RomListSetLoadedRomPath(NULL);
}
static void identity(const char *name,uint32 crc1,uint32 crc2,uint8 region) {
 size_t i;
 memset(header,0,sizeof(header));
 for(i=0;i<20;++i) header[(0x20+i)^3]=i<strlen(name)?name[i]:' ';
 memcpy(header+0x10,&crc1,4); memcpy(header+0x14,&crc2,4); header[0x3d]=region;
 strcpy(rominfo.name,name);
}
static void add(const char *path,int setting) {
 INI_ENTRY e={0};
 ReadRomHeaderInMemory(&e);
 e.Counter_Factor=setting; e.Code_Check=CODE_CHECK_PROTECT_MEMORY;
 e.Save_Type=ANYUSED_SAVETYPE; e.RDRAM_Size=8; e.Use_TLB=1;
 strcpy(e.Comments,"Preserve my options"); strcpy(e.Alt_Title,"My beta");
 assert(RomListAddEntry(&e,(char *)path,gAllocationLength)>=0);
}
static void verify_ec(const char *path) {
 INI_ENTRY original;
 INI_ENTRY *old;
 reset(); identity("Unrecognized EC",0x3ec36c75,0xf03048f1,0xf9);
 add(path,9); old=RomListSelectedEntry()->pinientry; original=*old;
 identity("Perfect Dark DBGNTSC",0x0f82040f,0x1b6c559e,'E');
 RomListSetLoadedRomPath(path);
 assert(RomListSelectLoadedRomEntry()); assert(romlist_count==2);
 assert(selected_rom_index>=0 && selected_rom_index<romlist_count);
 assert(!memcmp(old,&original,sizeof(original))); /* source INI unchanged */
 GenerateCurrentRomOptions();
 assert(!strcmp(currentromoptions.Game_Name,"Perfect Dark DBGNTSC"));
 assert(currentromoptions.crc1==0x0f82040f && currentromoptions.countrycode=='E');
 assert(currentromoptions.Eeprom_size==EEPROMSIZE_16KB);
 assert(currentromoptions.Counter_Factor==9 && currentromoptions.RDRAM_Size==8);
 assert(currentromoptions.Save_Type==ANYUSED_SAVETYPE && currentromoptions.Use_TLB==1);
 assert(!strcmp(currentromoptions.Comments,original.Comments));
 assert(!strcmp(currentromoptions.Alt_Title,original.Alt_Title));
 RomListSelectedEntry()->pinientry->Counter_Factor=13;
 assert(RomListSelectLoadedRomEntry()); assert(romlist_count==2);
 GenerateCurrentRomOptions(); assert(currentromoptions.Counter_Factor==13);
 assert(errors==0);
}
int main(void) {
 int i, saved;
 char path[300];
 verify_ec("C:\\ROMs\\EC.rom");
 verify_ec("C:\\ROMs\\EC.zip");
 reset(); identity("Perfect Dark",0x41f2b98f,0xb458b466,'E');
 add("retail.z64",4); RomListSetLoadedRomPath("retail.z64");
 assert(RomListSelectLoadedRomEntry() && romlist_count==1);
 GenerateCurrentRomOptions(); assert(currentromoptions.Counter_Factor==4);
 reset(); identity("Perfect Dark",0xf9864452,0x890a5ea3,'P');
 add("pal.rom",3); RomListSetLoadedRomPath("pal.rom");
 RomListSelectedEntry()->pinientry->Save_Type=SRAM_SAVETYPE;
 assert(RomListSelectLoadedRomEntry()); GenerateCurrentRomOptions();
 assert(currentromoptions.Eeprom_size==EEPROMSIZE_16KB);
 assert(currentromoptions.Save_Type==SRAM_SAVETYPE); /* explicit device unchanged */
 reset(); identity("Other PAL game",11,12,'P');
 add("other.rom",3); RomListSetLoadedRomPath("other.rom");
 assert(RomListSelectLoadedRomEntry()); GenerateCurrentRomOptions();
 assert(currentromoptions.Eeprom_size==1); /* PD fix does not resize other saves */
 reset(); identity("Unknown Game",1,2,'E'); RomListSetLoadedRomPath("new.rom");
 assert(RomListSelectLoadedRomEntry() && romlist_count==1);
 GenerateCurrentRomOptions(); assert(currentromoptions.Emulator==7);
 assert(currentromoptions.Counter_Factor==1 && currentromoptions.RDRAM_Size==8);
 reset(); identity("Missing",3,4,'E');
 assert(!RomListSelectLoadedRomEntry() && selected_rom_index==-1 && romlist_count==0);
 memset(path,'x',sizeof(path)-1);path[sizeof(path)-1]=0;RomListSetLoadedRomPath(path);
 assert(!RomListSelectLoadedRomEntry() && selected_rom_index==-1);
 reset();
 for(i=0;i<MAX_ROMLIST;++i) {identity("Existing",i+10,i+20,'E');add("old.rom",i+1);}
 saved=selected_rom_index; identity("Missing",90,91,'E');RomListSetLoadedRomPath("new.rom");
 assert(!RomListSelectLoadedRomEntry());
 assert(romlist_count==MAX_ROMLIST && selected_rom_index==saved);
 reset(); identity("Missing",3,4,'E');RomListSetLoadedRomPath("new.rom");fail_alloc=1;
 assert(!RomListSelectLoadedRomEntry() && selected_rom_index==-1 && romlist_count==0);
 reset(); puts("Loaded ROM identity/settings regression checks passed"); return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="loaded-rom-options-") as directory:
    source = Path(directory) / "harness.c"
    executable = Path(directory) / "harness"
    source.write_text(harness)
    subprocess.run([
        "cc", "-std=c99", "-Wall", "-Wextra", "-Werror",
        "-Wno-pointer-sign", "-fsanitize=undefined,address",
        str(source), "-o", str(executable),
    ], check=True)
    # LeakSanitizer cannot enumerate threads in restricted execution workers.
    # Address and undefined-behavior checks remain enabled.
    environment = dict(os.environ, ASAN_OPTIONS="detect_leaks=0")
    subprocess.run([str(executable)], check=True, env=environment)
