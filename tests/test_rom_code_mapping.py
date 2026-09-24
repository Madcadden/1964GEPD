#!/usr/bin/env python3
"""Check production ROM-map admission separately from optional AI patches.

Runs synthetic pager fixtures by default. Optional user-supplied .z64 inputs:
  python tests/test_rom_code_mapping.py --retail retail.z64 \
      --paged pd_ai.z64 --physical goldeneye_007_plus.z64
No ROMs are distributed by this test. This verifies the actual resolver and
InitTLBOther mapping code, not Windows emulation or live gameplay.
"""
from pathlib import Path
import argparse
import os
import runpy
import shlex
import subprocess
import tempfile

MAIN = r'''
static unsigned int cases;
static void reset(unsigned int size) {
    GEPDRestoreROMHacks(); free(gMemoryState.ROM_Image);
    gMemoryState.ROM_Image = calloc(1, size); assert(gMemoryState.ROM_Image);
    gAllocationLength = size; GEPDResetHackResolution();
    memset(Direct_TLB_Lookup_Table, 0, sizeof Direct_TLB_Lookup_Table);
    memset(TLB_sDWORD_R, 0, sizeof TLB_sDWORD_R);
    emustatus.game_hack = GHACK_GE; rominfo.TV_System = TV_SYSTEM_NTSC;
}
static void pager(unsigned int off, unsigned int base) {
    const unsigned int words[] = {
        0x3c0100ff,0x3421e000,0x00104340,0x3c0a0000,
        0x00414824,0x254a0000,0x03282021,0xafa40034,
        0x012a2821,0x01201025,0xafa90024,0x0c00170f,0x24062000
    };
    unsigned int i;
    for(i=0;i<13;i++) GEWriteROMWord(off+i*4, words[i]);
    GEWriteROMWord(off+12, 0x3c0a0000 | ((base+0x8000)>>16));
    GEWriteROMWord(off+20, 0x254a0000 | (base&0xffff));
}
static void fixture(unsigned int base) {
    unsigned int i; reset(0x30000);
    for(i=0;i<5;i++) GEWriteROMWord(base+i*4,gegamesegmentpattern[i]);
    pager(0x1600,base);
}
static void check(int expected, unsigned int base) {
    unsigned int pages, i;
    GEPDResetHackResolution();
    assert(!GEGetHackResolution()->guardvalid);
    assert(GEUsesROMCodeMapping()==expected);
    InitTLBOther();
    assert(trigger_tlb_exception_faster==expected);
    if(expected) {
        pages=(gAllocationLength-base)/0x1000;
        if(pages>0xfcb) pages=0xfcb;
        for(i=0;i<pages;i++) {
            assert(Direct_TLB_Lookup_Table[0x7f000+i]==0x90000000+base+i*0x1000);
            assert(TLB_sDWORD_R[0x7f000+i]==gMemoryState.ROM_Image+base+i*0x1000);
        }
    } else assert(TLB_sDWORD_R[0x7f000]==NULL);
    cases++;
}
static void synthetic(void) {
    unsigned int i;
    fixture(0x14000); check(1,0x14000);
    fixture(0x1a000); check(1,0x1a000); /* sign-extended ADDIU */
    fixture(0x14000); pager(0x1600,0x18000); check(0,0);
    fixture(0x14000); pager(0x1800,0x14000); check(0,0);
    fixture(0x14000); GEWriteROMWord(0x1600,0); check(0,0);
    fixture(0x14000); GEWriteROMWord(0x14000,0); check(0,0);
    fixture(0x14000);
    for(i=0;i<5;i++) GEWriteROMWord(0x18000+i*4,gegamesegmentpattern[i]);
    check(0,0);
    fixture(0x14000); GEWriteROMWord(0x1600,0); pager(0x18000,0x14000); check(0,0);
    fixture(0x14000); GEWriteROMWord(0x1600,0); pager(0x14000-52,0x14000); check(1,0x14000);
    fixture(0x14000); GEWriteROMWord(0x1600,0); pager(0x14000-48,0x14000); check(0,0);
    /* Every constrained instruction is essential, including 8 KiB DMA,
     * source addition, destination calculation and actual JAL opcode. */
    for(i=0;i<13;i++) {
        fixture(0x14000);
        GEWriteROMWord(0x1600+i*4,GEReadROMWord(0x1600+i*4)^0x04000000);
        check(0,0);
    }
    fixture(0x14000); GEWriteROMWord(0x162c,0x0c001234); check(1,0x14000);
    reset(0x1000); check(0,0);
    reset(48); check(0,0);
    printf("PASS %u synthetic mapping cases; AI guard patch remains disabled\n",cases);
}
static void actual(const char *path, char kind) {
    FILE *f=fopen(path,"rb"); long size; unsigned int i,base=0;
    const GE_HACK_RESOLUTION *r;
    assert(f); fseek(f,0,SEEK_END);size=ftell(f);rewind(f);
    assert(size>=0x1000 && (size&3)==0);reset((unsigned int)size);
    assert(fread(gMemoryState.ROM_Image,1,size,f)==(size_t)size);fclose(f);
    assert(!memcmp(gMemoryState.ROM_Image,"\x80\x37\x12\x40",4));
    for(i=0;i<gAllocationLength;i+=4) {
        unsigned char *b=gMemoryState.ROM_Image+i,t;
        t=b[0];b[0]=b[3];b[3]=t;t=b[1];b[1]=b[2];b[2]=t;
    }
    r=GEGetHackResolution();
    assert(r->gamevalid && r->firingvalid && r->headrollvalid);
    assert(r->guardvalid==(kind=='r'));
    assert(GEUsesROMCodeMapping()==(kind!='x'));
    if(kind=='p') check(1,GEFindUniqueROMPattern(gegamesegmentpattern,gegamesegmentmask,5));
    else {
        InitTLBOther(); assert(trigger_tlb_exception_faster==(kind=='r'));
        if(kind=='r') {assert(GEFindGameSegment(&base));assert(TLB_sDWORD_R[0x7f000]==gMemoryState.ROM_Image+base);}
        else assert(TLB_sDWORD_R[0x7f000]==NULL);
    }
    printf("PASS %s: map=%d guard=%d\n",path,GEUsesROMCodeMapping(),r->guardvalid);
}
int main(int argc,char **argv) {
    int i;synthetic();assert((argc&1)==1);
    for(i=1;i<argc;i+=2) actual(argv[i+1],argv[i][0]);
    free(gMemoryState.ROM_Image);return 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--retail", type=Path)
    parser.add_argument("--paged", type=Path, action="append", default=[])
    parser.add_argument("--physical", type=Path)
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    existing = runpy.run_path(str(repo / "tests/test_patch_resolution.py"))
    preamble = existing["preamble"].replace(
        "unsigned int crc1, crc2;", "unsigned int crc1, crc2; char Game_Name[32];")
    preamble += r'''
typedef unsigned int uint32;
#define gMS_ROM_Image gMemoryState.ROM_Image
static uint32 Direct_TLB_Lookup_Table[0x100000];
static BOOL trigger_tlb_exception_faster;
'''
    source = (repo / "win32/Wingui.c").read_text(encoding="latin1")
    begin = source.index("#define PD_frameratecal")
    resolver = source[begin:source.index("void SetCodeCheckMethod", begin)]
    tlb = (repo / "Tlb.c").read_text(encoding="latin1")
    segment = tlb[tlb.index("static BOOL GEIsGameSegmentStart"):tlb.index("void InitTLB(void)")]
    begin = tlb.index("void InitTLBOther(void)\n{")
    mapping = tlb[begin:tlb.index("/*", tlb.index("\n}\n", begin))]
    with tempfile.TemporaryDirectory(prefix="gepd-map-test-") as directory:
        temp = Path(directory)
        c = temp / "mapping.c"
        c.write_text(preamble + resolver + segment + mapping + MAIN)
        executable = temp / ("mapping.exe" if os.name == "nt" else "mapping")
        flags = ["-std=c99", "-O2", "-Wall", "-Wextra", "-Wno-unused-function", "-Wno-unused-const-variable"]
        if args.sanitize:
            flags += ["-fsanitize=address,undefined"]
        subprocess.run(shlex.split(os.environ.get("CC", "cc")) + flags + [str(c), "-o", str(executable)], check=True)
        inputs = ([('r', args.retail)] if args.retail else []) + [('p', p) for p in args.paged]
        if args.physical:
            inputs.append(('x', args.physical))
        command = [str(executable)]
        for kind, path in inputs:
            command.extend([kind, str(path.resolve())])
        subprocess.run(command, check=True)


if __name__ == "__main__":
    main()
