/* Host-only native tests; no ROMs or gameplay are run. GPL-2.0-or-later. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static BOOL videoGoldenEyeProfileRequested;
#include "../win32/GoldenEyeDepthCompat.h"

static DWORD node[4], address, branch, savedFlags, regs[7];
static void *entry;
static unsigned int checks;
__declspec(naked) static void Accepted(void) {
    __asm {
        pushfd
        pop savedFlags
        mov regs[0], eax
        mov regs[4], ebx
        mov regs[8], ecx
        mov regs[12], edx
        mov regs[16], esi
        mov regs[20], edi
        mov regs[24], ebp
        mov branch, 1
        ret
    }
}
__declspec(naked) static void Advance(void) {
    __asm {
        pushfd
        pop savedFlags
        mov regs[0], eax
        mov regs[4], ebx
        mov regs[8], ecx
        mov regs[12], edx
        mov regs[16], esi
        mov regs[20], edi
        mov regs[24], ebp
        mov branch, 2
        ret
    }
}
static void Require(int condition, const char *message) {
    ++checks;
    if(!condition) { fprintf(stderr,"FAIL: %s\n",message); exit(1); }
}
static void RunCase(BOOL goldenEye, DWORD start, DWORD requested) {
    unsigned long before = videoDepthAliasRejects;
    DWORD expected = goldenEye && start != requested ? 2 : 1;
    videoGoldenEyeProfileRequested = goldenEye;
    node[2] = start;
    address = requested;
    branch = 0;
    __asm {
        pushad
        lea eax, node
        mov esi, address
        mov ebx, 0x11223344
        mov ecx, 0x22334455
        mov edx, 0x33445566
        mov edi, 0x44556677
        call dword ptr [entry]
        popad
    }
    Require(branch == expected,"exact-match/continue routing");
    Require(regs[0] == (DWORD)node && regs[1] == 0x11223344 && regs[2] == 0x22334455 &&
        regs[3] == 0x33445566 && regs[4] == requested && regs[5] == 0x44556677,
        "preserve all unaffected registers");
    Require(regs[6] == (DWORD)&node[2],"reproduce original EBP result");
    Require(videoDepthAliasRejects == before + (expected == 2),"alias counter");
    if(expected == 1) Require((savedFlags & 0x40) == 0,"reproduce original TEST zero flag");
}
int main(int argc, char **argv) {
    HMODULE plugin;
    unsigned char *site;
    void *resume, *advance;
    unsigned int i;
    DWORD seed = 0x19640007;
    Require(argc == 2,"reference DLL argument");
    plugin = LoadLibraryA(argv[1]);
    if(plugin == NULL) fprintf(stderr,"LoadLibrary error: %lu\n",GetLastError());
    Require(plugin != NULL,"load real reference graphics DLL");
    VIDEO_InstallDepthCompatibility(plugin);
    Require(videoDepthSite != NULL,"validate and install reference adapter");
    printf("Adapter: %s\n",videoDepthAdapterStatus);
    site = videoDepthSite;
    resume = videoDepthResume;
    advance = videoDepthAdvance;
    Require(resume == (unsigned char*)plugin + 0xD782 && advance == (unsigned char*)plugin + 0xD75D,
        "verified continuation addresses after relocation");
    entry = site;
    videoDepthResume = Accepted;
    videoDepthAdvance = Advance;
    RunCase(TRUE,0x300000,0x300000);
    RunCase(TRUE,0x300000,0x300040);
    RunCase(FALSE,0x300000,0x300040);
    RunCase(TRUE,0x300000,0x300000);
    for(i=0;i<1024;i++) {
        seed = seed * 1664525U + 1013904223U;
        RunCase(TRUE,seed,seed);
        RunCase(TRUE,seed,seed+64);
        RunCase(FALSE,seed,seed+64);
    }
    videoDepthResume = resume;
    videoDepthAdvance = advance;
    VIDEO_RestoreDepthCompatibility();
    Require(videoDepthSite == NULL && memcmp(site,videoDepthOriginalBytes,5)==0,
        "restore original plugin code");
    VIDEO_InstallDepthCompatibility(GetModuleHandleA(NULL));
    Require(videoDepthSite == NULL,"reject unknown executable without modifying it");
    VIDEO_InstallDepthCompatibility(plugin);
    Require(videoDepthSite != NULL,"reinstall after unload/selection lifecycle");
    VIDEO_RestoreDepthCompatibility();
    FreeLibrary(plugin);
    printf("PASS: %u native assertions over 3076 routing cases. No graphical gameplay test.\n",checks);
    return 0;
}
