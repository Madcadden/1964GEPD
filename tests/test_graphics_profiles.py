#!/usr/bin/env python3
"""Verify graphics-only GoldenEye family recognition and GLideN64 lifecycle.

Provide local ROM fixtures with --ge/--non-ge. No ROM is modified or included.
This tests host code and plugin callbacks, not rendered gameplay.
"""
from pathlib import Path
import argparse, hashlib, json, os, shlex, subprocess, tempfile

STUBS = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <zlib.h>
typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define _strnicmp strncasecmp
#define __int8 char
#define __try if(1)
#define __except(...) else
static struct { unsigned char *ROM_Image; } gMemoryState;
static unsigned int gAllocationLength;
typedef struct { char *HEADER; int MemoryBswaped; } GFX_INFO;
typedef struct { int unused; } RECT;
static struct { int hwnd1964main; } gui;
static struct { int UseThis; } GFX_PluginRECT;
static unsigned char HeaderDllPass[64];
static void GetWindowRect(int w, RECT *r) { (void)w;(void)r; }
static void GetPluginsResizeRequest(RECT *r) { (void)r; }
static void DisplayError(const char *s) { fprintf(stderr,"%s\n",s);abort(); }
static BOOL (*_VIDEO_InitiateGFX)(GFX_INFO);
static void (*_VIDEO_RomOpen)(void), (*_VIDEO_RomClosed)(void), (*_VIDEO_DllClose)(void);
'''

CASES = r'''
static unsigned int cases, opens, closes;
static char *retainedHeader, observedName[21];
static unsigned char *read_rom(const char *path, unsigned int *size) {
    FILE *f=fopen(path,"rb");long n;unsigned int i;unsigned char *out;assert(f);
    fseek(f,0,SEEK_END);n=ftell(f);rewind(f);assert(n>0 && !(n&3));out=malloc(n);assert(out);
    assert(fread(out,1,n,f)==(size_t)n);fclose(f);*size=n;
    for(i=0;i<*size;i+=4) { unsigned char *b=out+i,t=b[0];b[0]=b[3];b[3]=t;t=b[1];b[1]=b[2];b[2]=t; }
    return out;
}
static void select_rom(unsigned char *rom, unsigned int size) {
    gMemoryState.ROM_Image=rom;gAllocationLength=size;
    if(rom && size>=64)memcpy(HeaderDllPass,rom,64);else memset(HeaderDllPass,0,64);
}
static void set_word(unsigned int offset, unsigned int value) { memcpy(gMemoryState.ROM_Image+offset,&value,4); }
static BOOL plugin_init(GFX_INFO info) { retainedHeader=info.HEADER;return TRUE; }
static void plugin_open(void) {
    unsigned int i;opens++;assert(retainedHeader);
    for(i=0;i<20;i++)observedName[i]=retainedHeader[(32+i)^3];observedName[20]=0;
    while(i && observedName[i-1]==' ')observedName[--i]=0;
}
static void plugin_close(void) { closes++;if(retainedHeader){volatile char b=retainedHeader[0];(void)b;} }
static void assert_header_only(void) {
    unsigned int i;assert(!memcmp(HeaderDllPass,gMemoryState.ROM_Image,64));
    for(i=0;i<64;i++)if(i<32 || i>=52)assert((unsigned char)retainedHeader[i]==HeaderDllPass[i]);
}
int main(int argc,char **argv) {
    unsigned int i,j,size,old,firstSize,positives;unsigned char *rom,*firstRom,*copy;
    unsigned char header[64],accessors[24],moved[24];GFX_INFO info;char *first;
    const char *accepted[]={"GLideN64","GLideN64 rev.2020","GLIDEN64 rev.test"};
    const char *rejected[]={"","GLideN640","GLideN64Fake","Glide64","Jabo's Direct3D8","xGLideN64"};
    assert(argc>=4);positives=atoi(argv[1]);assert(positives>0 && positives<(unsigned int)argc-2);
    _VIDEO_InitiateGFX=plugin_init;_VIDEO_RomOpen=plugin_open;_VIDEO_RomClosed=plugin_close;_VIDEO_DllClose=plugin_close;
    info.HEADER=(char *)HeaderDllPass;info.MemoryBswaped=TRUE;videoIsGLideN64=TRUE;
    select_rom(NULL,0);assert(!GEPDUseGoldenEyeGraphicsProfile());VIDEO_InitiateGFX(info);first=retainedHeader;
    assert(first==(char *)videoGraphicsHeader && first!=info.HEADER && !memcmp(first,HeaderDllPass,64));cases++;
    firstRom=read_rom(argv[2],&firstSize);
    for(i=2;i<(unsigned int)argc;i++) {
        rom=read_rom(argv[i],&size);copy=malloc(size);assert(copy);memcpy(copy,rom,size);select_rom(rom,size);
        assert(GEPDUseGoldenEyeGraphicsProfile()==(i<2+positives));cases++;
        VIDEO_RomOpen();assert(retainedHeader==first);assert_header_only();
        if(i<2+positives)assert(!strcmp(observedName,"GOLDENEYE"));
        else assert(!memcmp(retainedHeader,HeaderDllPass,64));
        assert(!memcmp(rom,copy,size));cases++;
        VIDEO_RomClosed();VIDEO_RomOpen();assert(retainedHeader==first && !memcmp(rom,copy,size));cases++;
        free(copy);free(rom);
    }
    select_rom(firstRom,firstSize);memcpy(header,firstRom,64);
    /* Header names, checksums and region do not decide the graphics engine. */
    memset(firstRom+16,0x5a,8);memset(firstRom+32,0x71,20);firstRom[0x3e^3]=0x50;
    assert(GEPDUseGoldenEyeGraphicsProfile());select_rom(firstRom,firstSize);VIDEO_RomOpen();
    assert(!strcmp(observedName,"GOLDENEYE"));assert_header_only();memcpy(firstRom,header,64);cases++;
    /* Relocated boot accessors remain supported without fixed data offsets. */
    memcpy(accessors,firstRom+0x10c8,24);memcpy(moved,firstRom+0x1800,24);
    memset(firstRom+0x10c8,0,24);assert(!GEPDUseGoldenEyeGraphicsProfile());cases++;
    memcpy(firstRom+0x1800,accessors,24);assert(GEPDUseGoldenEyeGraphicsProfile());cases++;
    memcpy(firstRom+0x1800,moved,24);memcpy(firstRom+0x10c8,accessors,24);
    for(i=0;i<6;i++) {unsigned int a=0x10c8+i*4;old=VIDEO_ROMWord(a);set_word(a,0xffffffff);assert(!GEPDUseGoldenEyeGraphicsProfile());set_word(a,old);cases++;}
    old=VIDEO_ROMWord(0);set_word(0,0);assert(!GEPDUseGoldenEyeGraphicsProfile());set_word(0,old);cases++;
    /* Corrupt, incomplete and non-GoldenEye compressed prefixes are rejected. */
    j=((VIDEO_ROMWord(0x10c8)&65535)<<16)+(int)(short)VIDEO_ROMWord(0x10d0);
    for(i=0;i<3;i++){unsigned int a=(j+i)^3;unsigned char b=firstRom[a];firstRom[a]=i==2?0xff:0;assert(!GEPDUseGoldenEyeGraphicsProfile());firstRom[a]=b;cases++;}
    gAllocationLength=16;assert(!GEPDUseGoldenEyeGraphicsProfile());cases++;
    gAllocationLength=firstSize-1;assert(!GEPDUseGoldenEyeGraphicsProfile());cases++;
    gAllocationLength=j+4;assert(!GEPDUseGoldenEyeGraphicsProfile());cases++;gAllocationLength=firstSize;
    for(i=0;i<sizeof(accepted)/sizeof(accepted[0]);i++){assert(VIDEO_IsGLideN64Name(accepted[i]));cases++;}
    for(i=0;i<sizeof(rejected)/sizeof(rejected[0]);i++){assert(!VIDEO_IsGLideN64Name(rejected[i]));cases++;}
    select_rom(firstRom,firstSize);videoIsGLideN64=FALSE;VIDEO_InitiateGFX(info);assert(retainedHeader==info.HEADER);
    VIDEO_RomOpen();assert(!memcmp(retainedHeader,firstRom,64));cases++;
    videoIsGLideN64=TRUE;info.MemoryBswaped=FALSE;VIDEO_InitiateGFX(info);VIDEO_RomOpen();assert(!memcmp(retainedHeader,HeaderDllPass,64));cases++;
    info.MemoryBswaped=TRUE;VIDEO_InitiateGFX(info);VIDEO_RomOpen();assert(!strcmp(observedName,"GOLDENEYE"));assert_header_only();cases++;
    VIDEO_RomClosed();VIDEO_DllClose();assert(opens>positives && closes>positives);free(firstRom);
    printf("%u host cases passed; %u GoldenEye-family and %u other-engine fixtures\n",cases,positives,(unsigned int)argc-2-positives);return 0;
}
'''

def function(source, signature):
    start=source.index(signature);pos=source.index('{',start);depth=1;end=pos+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]+'\n'

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--ge',type=Path,action='append',required=True);ap.add_argument('--non-ge',type=Path,action='append',required=True)
    ap.add_argument('--source',type=Path,default=Path(__file__).resolve().parents[1]);ap.add_argument('--sanitize',action='store_true')
    a=ap.parse_args();source=(a.source/'win32/Dll_Video.c').read_bytes();video=source.decode()
    start=video.index('/* BEGIN GE GRAPHICS HEADER');end=video.index('/* END GE GRAPHICS HEADER */',start)
    helpers=video[start:end]+'\n';wrappers=''.join(function(video,s) for s in ['void VIDEO_DllClose(void)','BOOL VIDEO_InitiateGFX(GFX_INFO Gfx_Info)','void VIDEO_RomOpen(void)','void VIDEO_RomClosed(void)'])
    loader=function(video,'BOOL LoadVideoPlugin(char *libname)');closer=function(video,'void CloseVideoPlugin(void)')
    assert 'videoIsGLideN64 = FALSE;' in loader and 'videoIsGLideN64 = VIDEO_IsGLideN64Name(Plugin_Info.Name);' in loader
    assert closer.index('VIDEO_DllClose();')<closer.index('videoIsGLideN64 = FALSE;')
    with tempfile.TemporaryDirectory(prefix='ge-graphics-test-') as tmp:
        tmp=Path(tmp);c=tmp/'test.c';c.write_text(STUBS+helpers+wrappers+CASES);exe=tmp/'test'
        flags=['-std=c99','-O1','-Wall','-Wextra','-Wno-misleading-indentation']
        if a.sanitize:flags+=['-fsanitize=address,undefined']
        subprocess.run(shlex.split(os.environ.get('CC','cc'))+flags+[str(c),'-lz','-o',str(exe)],check=True)
        run=subprocess.run([str(exe),str(len(a.ge))]+[str(p.resolve()) for p in a.ge+a.non_ge],capture_output=True,text=True)
        assert run.returncode==0,run.stderr
        print(json.dumps({'result':run.stdout.strip(),'sanitizers':a.sanitize,'source_sha256':hashlib.sha256(source).hexdigest(),'limits':'Host callbacks and private ROM fixtures; no live Windows graphics replay.'},indent=2))
if __name__=='__main__':main()
