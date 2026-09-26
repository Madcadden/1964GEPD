/*$T Dll_Video.c GC 1.136 03/09/02 17:41:40 */


/*$6
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    Video plugin interface functions
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */


/*
 * 1964 Copyright (C) 1999-2002 Joel Middendorf, <schibo@emulation64.com> This
 * program is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version. This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details. You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. To contact the
 * authors: email: schibo@emulation64.com, rice1964@yahoo.com
 */
#include <windows.h>
#include <stdio.h>
#include "../globals.h"
#include "../memory.h"
#include "registry.h"
#include "DLL_Video.h"
#include "../romlist.h"
#include "wingui.h"
#include "../emulator.h"
#include "../zlib/zlib.h"

uint16		GfxPluginVersion;
HINSTANCE	hinstLibVideo = NULL;
GFX_INFO	Gfx_Info;

/* BEGIN GE GRAPHICS HEADER
 * GLideN64 selects its GoldenEye depth-buffer handling by the ROM title.
 * Keep this plugin-only copy alive until CloseDLL; the real header and all
 * other plugins retain the mod's identity. */
/* Match the graphics engine, not a mod title, ROM checksum or gameplay
 * patch pattern. Boot accessors provide a fast path, then the graphics code
 * itself is found in compressed or raw form. ROM and RAM stay intact. */
static unsigned int VIDEO_ROMWord(unsigned int offset)
{
    return *((unsigned int *)(gMemoryState.ROM_Image + offset));
}

/* Verify the same F3DGOLDEN graphics code independently of the loader.
 * A ROM title/CRC is deliberately not used: e.g. GoldenEye X uses PD's
 * engine and must NOT inherit GoldenEye's depth-buffer correction. */
static BOOL VIDEO_GoldenEyeCompressedBlock(unsigned int start, unsigned int end)
{
    unsigned int index, position, count;
    unsigned char input[2048], prefix[0x10D0];
    z_stream stream;
    int result;
    BOOL match = FALSE;
    if(start >= end || end > gAllocationLength || end - start < 3U ||
        gMemoryState.ROM_Image[start ^ 3U] != 0x11U ||
        gMemoryState.ROM_Image[(start + 1U) ^ 3U] != 0x72U)
        return FALSE;
    position = start + 2U;
    /* Bound input and output even for incomplete or empty deflate blocks. */
    if(end - position > 0x10000U)
        end = position + 0x10000U;
    memset(&stream, 0, sizeof(stream));
    stream.next_out = prefix;
    stream.avail_out = sizeof(prefix);
    if(inflateInit2(&stream, -MAX_WBITS) != Z_OK)
        return FALSE;
    while(stream.avail_out != 0)
    {
        if(stream.avail_in == 0)
        {
            if(position == end)
                break;
            count = end - position;
            if(count > sizeof(input))
                count = sizeof(input);
            for(index = 0; index < count; index++)
                input[index] = gMemoryState.ROM_Image[(position + index) ^ 3U];
            position += count;
            stream.next_in = input;
            stream.avail_in = count;
        }
        result = inflate(&stream, Z_NO_FLUSH);
        if(result != Z_OK && result != Z_STREAM_END)
            break;
        if(stream.avail_out == 0)
        {
            /* Canonical-byte counterpart of GLideN64's F3DGOLDEN CRC. */
            match = crc32(0L, prefix + 0xD0U, 4096U) == 0x9CBA9D04UL;
            break;
        }
        if(result == Z_STREAM_END)
            break;
    }
    inflateEnd(&stream);
    return match;
}

static BOOL VIDEO_GoldenEyeMicrocode(unsigned int accessor)
{
    unsigned int index, start, end;
    if(accessor > gAllocationLength || gAllocationLength - accessor < 24U)
        return FALSE;
    for(index = 0; index < 24U; index += 12U)
        if((VIDEO_ROMWord(accessor + index) & 0xFFFF0000U) != 0x3C020000U ||
            VIDEO_ROMWord(accessor + index + 4U) != 0x03E00008U ||
            (VIDEO_ROMWord(accessor + index + 8U) & 0xFFFF0000U) != 0x24420000U)
            return FALSE;
    start = ((VIDEO_ROMWord(accessor) & 0xFFFFU) << 16) +
        (int)(short)(VIDEO_ROMWord(accessor + 8U) & 0xFFFFU);
    end = ((VIDEO_ROMWord(accessor + 12U) & 0xFFFFU) << 16) +
        (int)(short)(VIDEO_ROMWord(accessor + 20U) & 0xFFFFU);
    return VIDEO_GoldenEyeCompressedBlock(start, end);
}

static BOOL VIDEO_GoldenEyeRawMicrocode(unsigned int start)
{
    /* A short reject filter avoids calculating a CRC at every ROM byte.
     * The complete 4096-byte graphics-code CRC remains the deciding check. */
    static const unsigned char signature[16] = {
        0x09, 0x00, 0x05, 0xEA, 0x20, 0x1D, 0x01, 0x10,
        0x0D, 0x00, 0x04, 0x47, 0x03, 0x00, 0x98, 0x20
    };
    unsigned char code[4096];
    unsigned int index;
    if(start > gAllocationLength || gAllocationLength - start < sizeof(code))
        return FALSE;
    for(index = 0; index < sizeof(signature); index++)
        if(gMemoryState.ROM_Image[(start + index) ^ 3U] != signature[index])
            return FALSE;
    for(index = 0; index < sizeof(code); index++)
        code[index] = gMemoryState.ROM_Image[(start + index) ^ 3U];
    return crc32(0L, code, sizeof(code)) == 0x9CBA9D04UL;
}

static BOOL GEPDUseGoldenEyeGraphicsProfile(void)
{
    unsigned int offset, limit;
    if(gMemoryState.ROM_Image == NULL || gAllocationLength < 0x10E0U ||
        (gAllocationLength & 3U) != 0 || VIDEO_ROMWord(0) != 0x80371240U)
        return FALSE;
    if(VIDEO_GoldenEyeMicrocode(0x10C8U))
        return TRUE;
    /* Preserve the existing fast path for resident boot accessors. */
    limit = gAllocationLength < 0x20000U ? gAllocationLength : 0x20000U;
    for(offset = 0x1000U; offset <= limit - 24U; offset += 4U)
        if(offset != 0x10C8U && VIDEO_GoldenEyeMicrocode(offset))
            return TRUE;
    /* Loader-independent fallback. Compressed or raw graphics code may be
     * anywhere in the ROM, including byte-unaligned resource containers.
     * This runs only at graphics initialization/RomOpen, never per frame.
     * ROM/RAM, other plugins, save identity and gameplay patches stay intact. */
    for(offset = 0x1000U; offset < gAllocationLength - 2U; offset++)
    {
        unsigned char first = gMemoryState.ROM_Image[offset ^ 3U];
        if(first == 0x11U &&
            gMemoryState.ROM_Image[(offset + 1U) ^ 3U] == 0x72U &&
            VIDEO_GoldenEyeCompressedBlock(offset, gAllocationLength))
            return TRUE;
        if(first == 0x09U && VIDEO_GoldenEyeRawMicrocode(offset))
            return TRUE;
    }
    return FALSE;
}

static unsigned char videoGraphicsHeader[0x40];
static BOOL videoIsGLideN64 = FALSE;
static BOOL videoHeaderIsWordSwapped = FALSE;
static BOOL videoGoldenEyeProfileRequested = FALSE;
static char videoPluginName[100];

static BOOL VIDEO_IsGLideN64Name(const char *name)
{
    char suffix;
    if(name == NULL || _strnicmp(name, "GLideN64", 8) != 0)
        return FALSE;
    suffix = name[8];
    /* Forks may use a date/version after a separator, not just a space.
     * Do not confuse the separate Glide64 plugin with GLideN64. */
    return suffix == '\0' || suffix == ' ' || suffix == '\t' ||
        suffix == '_' || suffix == '-' || suffix == '(' || suffix == '[';
}

static void VIDEO_RefreshGraphicsHeader(void)
{
	static const char retailTitle[21] = "GOLDENEYE           ";
	unsigned int index;
	memcpy(videoGraphicsHeader, HeaderDllPass, sizeof(videoGraphicsHeader));
	videoGoldenEyeProfileRequested = videoIsGLideN64 && videoHeaderIsWordSwapped &&
		GEPDUseGoldenEyeGraphicsProfile();
	if(videoGoldenEyeProfileRequested)
		for(index = 0; index < 20; index++)
			videoGraphicsHeader[(0x20U + index) ^ 3U] = retailTitle[index];
}
/* One bounded diagnostic file beside the executable, never per-frame I/O.
 * Reports the header requested from the plugin, NOT an unobserved internal
 * GLideN64 flag or an assertion that a level has been visually tested. */
static void VIDEO_LogGraphicsProfile(const char *phase)
{
    static const char logName[] = "GEPD-Graphics.log";
    char path[MAX_PATH], originalTitle[21], graphicsTitle[21];
    char *separator;
    DWORD length;
    unsigned int index, offset;
    FILE *file;
    length = GetModuleFileNameA(NULL, path, sizeof(path));
    if(length == 0 || length >= sizeof(path))
        return;
    separator = strrchr(path, '\\');
    if(separator == NULL || (size_t)(separator + 1 - path) + sizeof(logName) > sizeof(path))
        return;
    strcpy(separator + 1, logName);
    for(index = 0; index < 20; index++)
    {
        offset = 0x20U + index;
        if(videoHeaderIsWordSwapped)
            offset ^= 3U;
        originalTitle[index] = (char)HeaderDllPass[offset];
        graphicsTitle[index] = (char)videoGraphicsHeader[offset];
        if((unsigned char)originalTitle[index] < 32U || (unsigned char)originalTitle[index] > 126U)
            originalTitle[index] = '.';
        if((unsigned char)graphicsTitle[index] < 32U || (unsigned char)graphicsTitle[index] > 126U)
            graphicsTitle[index] = '.';
    }
    originalTitle[20] = graphicsTitle[20] = '\0';
    file = fopen(path, "w");
    if(file == NULL)
        return;
    fprintf(file, "1964GEPD GE-depth-auto-20260926\n"
        "Plugin: %.99s\nGLideN64 recognized: %s\nWord-swapped header: %s\n"
        "Original ROM title: %.20s\nGraphics-only title: %.20s\n"
        "GoldenEye graphics profile requested: %s\nStage: %s\n"
        "Requested workaround: %s\n"
        "This records profile delivery, not visual verification of the renderer.\n",
        videoPluginName, videoIsGLideN64 ? "yes" : "no",
        videoHeaderIsWordSwapped ? "yes" : "no", originalTitle, graphicsTitle,
        videoGoldenEyeProfileRequested ? "yes" : "no", phase,
        videoGoldenEyeProfileRequested ? "hack_clearAloneDepthBuffer" : "unchanged");
    fclose(file);
}
/* END GE GRAPHICS HEADER */

BOOL (__cdecl *_VIDEO_InitiateGFX) (GFX_INFO) = NULL;
void (__cdecl *_VIDEO_ProcessDList) (void) = NULL;
void (__cdecl *_VIDEO_RomOpen) (void) = NULL;
void (__cdecl *_VIDEO_RomClosed) (void) = NULL;
void (__cdecl *_VIDEO_DllClose) () = NULL;
void (__cdecl *_VIDEO_UpdateScreen) () = NULL;
void (__cdecl *_VIDEO_GetDllInfo) (PLUGIN_INFO *) = NULL;
void (__cdecl *_VIDEO_ExtraChangeResolution) (HWND, long, HWND) = NULL;
void (__cdecl *_VIDEO_DllConfig) (HWND hParent) = NULL;
void (__cdecl *_VIDEO_Test) (HWND) = NULL;
void (__cdecl *_VIDEO_About) (HWND) = NULL;
void (__cdecl *_VIDEO_MoveScreen) (int, int) = NULL;
void (__cdecl *_VIDEO_DrawScreen) (void) = NULL;
void (__cdecl *_VIDEO_ViStatusChanged) (void) = NULL;
void (__cdecl *_VIDEO_ViWidthChanged) (void) = NULL;
void (__cdecl *_VIDEO_ChangeWindow) (int) = NULL;

/* For spec 1.3 */
void (__cdecl *_VIDEO_ChangeWindow_1_3) (void) = NULL;
void (__cdecl *_VIDEO_CaptureScreen) (char *Directory) = NULL;
void (__cdecl *_VIDEO_ProcessRDPList) (void) = NULL;
void (__cdecl *_VIDEO_ShowCFB) (void) = NULL;

/* Used when selecting plugins */
void (__cdecl *_VIDEO_Under_Selecting_Test) (HWND) = NULL;
void (__cdecl *_VIDEO_Under_Selecting_About) (HWND) = NULL;

/*
 =======================================================================================================================
 =======================================================================================================================
 */
BOOL LoadVideoPlugin(char *libname)
{
	videoIsGLideN64 = FALSE;
	videoHeaderIsWordSwapped = FALSE;
	videoGoldenEyeProfileRequested = FALSE;
	videoPluginName[0] = '\0';
	/* Release the video plug-in if it has already been loaded */
	if(hinstLibVideo != NULL)
	{
		FreeLibrary(hinstLibVideo);
	}

	hinstLibVideo = LoadLibrary(libname);

	if(hinstLibVideo != NULL)						/* Here the library is loaded successfully */
	{
		/* Get the VIDEO_GetDllInfo function address in the loaded DLL file */
		_VIDEO_GetDllInfo = (void(__cdecl *) (PLUGIN_INFO *)) GetProcAddress(hinstLibVideo, "GetDllInfo");

		if(_VIDEO_GetDllInfo != NULL)
		{
			/*~~~~~~~~~~~~~~~~~~~~*/
			PLUGIN_INFO Plugin_Info;
			/*~~~~~~~~~~~~~~~~~~~~*/

			ZeroMemory(&Plugin_Info, sizeof(Plugin_Info));

			VIDEO_GetDllInfo(&Plugin_Info);
			Plugin_Info.Name[sizeof(Plugin_Info.Name) - 1] = '\0';
			GfxPluginVersion = Plugin_Info.Version;

			if(Plugin_Info.Type == PLUGIN_TYPE_GFX) /* Check if this is a video plugin */
			{
				videoIsGLideN64 = VIDEO_IsGLideN64Name(Plugin_Info.Name);
				strncpy(videoPluginName, Plugin_Info.Name, sizeof(videoPluginName) - 1);
				videoPluginName[sizeof(videoPluginName) - 1] = '\0';
				_VIDEO_DllClose = (void(__cdecl *) (void)) GetProcAddress(hinstLibVideo, "CloseDLL");
				_VIDEO_ExtraChangeResolution = (void(__cdecl *) (HWND, long, HWND)) GetProcAddress
					(
						hinstLibVideo,
						"ChangeWinSize"
					);
				_VIDEO_Test = (void(__cdecl *) (void)) GetProcAddress(hinstLibVideo, "DllTest");
				_VIDEO_About = (void(__cdecl *) (void)) GetProcAddress(hinstLibVideo, "DllAbout");
				_VIDEO_DllConfig = (void(__cdecl *) (HWND)) GetProcAddress(hinstLibVideo, "DllConfig");
				_VIDEO_MoveScreen = (void(__cdecl *) (void)) GetProcAddress(hinstLibVideo, "MoveScreen");
				_VIDEO_DrawScreen = (void(__cdecl *) (void)) GetProcAddress(hinstLibVideo, "DrawScreen");
				_VIDEO_ViStatusChanged = (void(__cdecl *) (void)) GetProcAddress(hinstLibVideo, "ViStatusChanged");
				_VIDEO_ViWidthChanged = (void(__cdecl *) (void)) GetProcAddress(hinstLibVideo, "ViWidthChanged");
				_VIDEO_InitiateGFX = (BOOL(__cdecl *) (GFX_INFO)) GetProcAddress(hinstLibVideo, "InitiateGFX");
				_VIDEO_RomOpen = (void(__cdecl *) (void)) GetProcAddress(hinstLibVideo, "RomOpen");
				_VIDEO_RomClosed = (void(__cdecl *) (void)) GetProcAddress(hinstLibVideo, "RomClosed");
				_VIDEO_ProcessDList = (void(__cdecl *) (void)) GetProcAddress(hinstLibVideo, "ProcessDList");
				_VIDEO_UpdateScreen = (void(__cdecl *) (void)) GetProcAddress(hinstLibVideo, "UpdateScreen");
				_VIDEO_ChangeWindow = (void(__cdecl *) (int)) GetProcAddress(hinstLibVideo, "ChangeWindow");

				/* for spec 1.3 */
				_VIDEO_ChangeWindow_1_3 = (void(__cdecl *) (void)) GetProcAddress(hinstLibVideo, "ChangeWindow");
				_VIDEO_CaptureScreen = (void(__cdecl *) (char *)) GetProcAddress(hinstLibVideo, "CaptureScreen");
				_VIDEO_ProcessRDPList = (void(__cdecl *) (void)) GetProcAddress(hinstLibVideo, "ProcessRDPList");
				_VIDEO_ShowCFB = (void(__cdecl *) (void)) GetProcAddress(hinstLibVideo, "ShowCFB");

				return(TRUE);
			}
		}
	}

	return FALSE;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void VIDEO_GetDllInfo(PLUGIN_INFO *Plugin_Info)
{
	if(_VIDEO_GetDllInfo != NULL)
	{
		__try
		{
			_VIDEO_GetDllInfo(Plugin_Info);
		}

		__except(NULL, EXCEPTION_EXECUTE_HANDLER)
		{
			DisplayError("GettDllInfo Failed.");
		}
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */

g_GFX_PluginRECT GFX_PluginRECT;

// If the plugin wants to resize the window, respect its wishes,
// but not until after the rom is loaded.
void GetPluginsResizeRequest(LPRECT lpRect)
{
	RECT RequestRect;
	GetWindowRect(gui.hwnd1964main, &RequestRect);
	if ( (RequestRect.right  != lpRect->right)  || 
	   (RequestRect.left   != lpRect->left)   ||
	   (RequestRect.top    != lpRect->top)    || 
	   (RequestRect.bottom != lpRect->bottom) )

		if ( ((RequestRect.right - RequestRect.left) > 300) && 
			 ((RequestRect.bottom - RequestRect.top) > 200) )
		{
			GFX_PluginRECT.rect.left   = RequestRect.left;
			GFX_PluginRECT.rect.right  = RequestRect.right;
			GFX_PluginRECT.rect.top    = RequestRect.top;
			GFX_PluginRECT.rect.bottom = RequestRect.bottom;
			GFX_PluginRECT.UseThis     = TRUE;
		}
}

BOOL VIDEO_InitiateGFX(GFX_INFO Gfx_Info)
{
	RECT Rect;
	
	VIDEO_DllClose();
	GFX_PluginRECT.UseThis = FALSE;
	videoHeaderIsWordSwapped = Gfx_Info.MemoryBswaped != 0;
	if(videoIsGLideN64)
	{
		VIDEO_RefreshGraphicsHeader();
		Gfx_Info.HEADER = (__int8 *)videoGraphicsHeader;
	}

	__try
	{
		GetWindowRect(gui.hwnd1964main, &Rect);
		_VIDEO_InitiateGFX(Gfx_Info);
		GetPluginsResizeRequest(&Rect);
	}
	__except(NULL, EXCEPTION_EXECUTE_HANDLER)
	{
		/* DisplayError("Cannot Initialize Graphics"); */
	}

	return(1);	/* why not for now.. */
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void VIDEO_ProcessDList(void)
{
	/* try/except is handled from the call */
	if(_VIDEO_ProcessDList != NULL) _VIDEO_ProcessDList();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void VIDEO_RomOpen(void)
{
	if(_VIDEO_RomOpen != NULL)
	{
		__try
		{
			RECT Rect;
			GetWindowRect(gui.hwnd1964main, &Rect);
			VIDEO_RefreshGraphicsHeader();
			VIDEO_LogGraphicsProfile("RomOpen starting");
			_VIDEO_RomOpen();
			VIDEO_LogGraphicsProfile("RomOpen returned");
			GetPluginsResizeRequest(&Rect);
		}

		__except(NULL, EXCEPTION_EXECUTE_HANDLER)
		{
			VIDEO_LogGraphicsProfile("RomOpen failed");
			DisplayError("Video RomOpen Failed.");
		}
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void VIDEO_RomClosed(void)
{
	if(_VIDEO_RomClosed != NULL)
	{
		__try
		{
			_VIDEO_RomClosed();
		}

		__except(NULL, EXCEPTION_EXECUTE_HANDLER)
		{
			DisplayError("Video RomClosed Failed.");
		}
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void VIDEO_ChangeWindow(int window)
{
	int passed = 0;

	if(GfxPluginVersion == 0x0103)
	{
		if(_VIDEO_ChangeWindow_1_3 != NULL)
		{
			__try
			{
				_VIDEO_ChangeWindow_1_3();
				guistatus.IsFullScreen ^= 1;
				passed = 1;
			}

			__except(NULL, EXCEPTION_EXECUTE_HANDLER)
			{
				DisplayError("VIDEO ChangeWindow failed");
				passed = 0;
			}
		}
	}
	else
	{
		if(_VIDEO_ChangeWindow != NULL)
		{
			__try
			{
				_VIDEO_ChangeWindow(window);
				guistatus.IsFullScreen ^= 1;
				passed = 1;
			}

			__except(NULL, EXCEPTION_EXECUTE_HANDLER)
			{
				DisplayError("VIDEO ChangeWindow failed");
				passed = 0;
			}
		}
	}

	if( guistatus.IsFullScreen && (passed==1))
	{
		EnableWindow(gui.hToolBar, FALSE);
		ShowWindow(gui.hToolBar, SW_HIDE);
		EnableWindow(gui.hReBar, FALSE);
		ShowWindow(gui.hReBar, SW_HIDE);
		EnableWindow((HWND)gui.hMenu1964main, FALSE);
		ShowWindow((HWND)gui.hMenu1964main, FALSE);
		ShowWindow(gui.hStatusBar, SW_HIDE);
		HideCursor(TRUE);
	}
	else
	{
		ShowWindow(gui.hReBar, SW_SHOW);
		EnableWindow(gui.hReBar, TRUE);
		EnableWindow(gui.hToolBar, TRUE);
		EnableWindow((HWND)gui.hMenu1964main, TRUE);
		ShowWindow(gui.hToolBar, SW_SHOW);
		ShowWindow(gui.hStatusBar, SW_SHOW);
		HideCursor(FALSE);
		DockStatusBar();
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void VIDEO_DllClose(void)
{
	if(_VIDEO_DllClose != NULL)
	{
		__try
		{
			_VIDEO_DllClose();
		}

		__except(NULL, EXCEPTION_EXECUTE_HANDLER)
		{
			DisplayError("VIDEO DllClose failed");
		}
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void CloseVideoPlugin(void)
{
	VIDEO_DllClose();
	videoIsGLideN64 = FALSE;
	videoHeaderIsWordSwapped = FALSE;
	videoGoldenEyeProfileRequested = FALSE;
	videoPluginName[0] = '\0';

	if(hinstLibVideo) FreeLibrary(hinstLibVideo);

	hinstLibVideo = NULL;

	_VIDEO_InitiateGFX = NULL;
	_VIDEO_ProcessDList = NULL;
	_VIDEO_RomOpen = NULL;
	_VIDEO_DllClose = NULL;
	_VIDEO_DllConfig = NULL;
	_VIDEO_GetDllInfo = NULL;
	_VIDEO_UpdateScreen = NULL;
	_VIDEO_ExtraChangeResolution = NULL;

	_VIDEO_ChangeWindow = NULL;
	_VIDEO_Test = NULL;
	_VIDEO_About = NULL;
	_VIDEO_MoveScreen = NULL;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void VIDEO_DllConfig(HWND hParent)
{
	RECT Rect;

	if(_VIDEO_DllConfig != NULL)
	{
		GetWindowRect(gui.hwnd1964main, &Rect);
		_VIDEO_DllConfig(hParent);
		GetPluginsResizeRequest(&Rect);
		if (Rom_Loaded == FALSE)
		SetWindowPos(gui.hwnd1964main, NULL, Rect.left, Rect.top, 
			Rect.right-Rect.left, 
			Rect.bottom-Rect.top,
			SWP_NOZORDER | SWP_SHOWWINDOW);
	}
	else
	{
		DisplayError("%s cannot be configured.", "Video Plugin");
	}
}
/*
 =======================================================================================================================
 =======================================================================================================================
 */
void VIDEO_About(HWND hParent)
{
	if(_VIDEO_About != NULL)
	{
		_VIDEO_About(hParent);
	}
	else
	{
		DisplayError("%s: About information is not available for this plug-in.", "Video Plugin");
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void VIDEO_Test(HWND hParent)
{
	if(_VIDEO_Test != NULL)
	{
		_VIDEO_Test(hParent);
	}
	else
	{
		DisplayError("%s: Test function is not available for this plug-in.", "Video Plugin");
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void VIDEO_MoveScreen(int x, int y)
{
	if(_VIDEO_MoveScreen != NULL)
	{
		_VIDEO_MoveScreen(x, y);
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
#include "../n64rcp.h"
void VIDEO_UpdateScreen(void)
{
	//static int recall = 0x04000000+307200*2;
	//static int k=0;


	if(_VIDEO_UpdateScreen != NULL) __try
	{
		_VIDEO_UpdateScreen();
	}

	__except(NULL, EXCEPTION_EXECUTE_HANDLER)
	{
		DisplayError("Video UpdateScreen failed.");
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void VIDEO_DrawScreen(void)
{
	if(_VIDEO_DrawScreen != NULL) __try
	{
		_VIDEO_DrawScreen();
	}

	__except(NULL, EXCEPTION_EXECUTE_HANDLER)
	{
		DisplayError("Video DrawScreen failed.");
	}

	//VIDEO_UpdateScreen();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void VIDEO_ViStatusChanged(void)
{
	if(_VIDEO_ViStatusChanged != NULL)
	{
		__try
		{
			_VIDEO_ViStatusChanged();
		}

		__except(NULL, EXCEPTION_EXECUTE_HANDLER)
		{
			DisplayError("Exception in ViStatusChanged");
		}
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void VIDEO_ViWidthChanged(void)
{
	if(_VIDEO_ViWidthChanged != NULL)
	{
		__try
		{
			_VIDEO_ViWidthChanged();
		}

		__except(NULL, EXCEPTION_EXECUTE_HANDLER)
		{
			DisplayError("Exception in ViWidthChanged");
		}
	}
}

/*
 =======================================================================================================================
    changes for spec 1.3
 =======================================================================================================================
 */
void VIDEO_CaptureScreen(char *Directory)
{
	if(_VIDEO_CaptureScreen != NULL)
	{
		__try
		{
			_VIDEO_CaptureScreen(Directory);
		}

		__except(NULL, EXCEPTION_EXECUTE_HANDLER)
		{
			DisplayError("Exception in Capture Screen");
		}
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void VIDEO_ProcessRDPList(void)
{
	if(_VIDEO_ProcessRDPList != NULL)
	{
		__try
		{
			_VIDEO_ProcessRDPList();
		}

		__except(NULL, EXCEPTION_EXECUTE_HANDLER)
		{
			DisplayError("Exception in Processing RDP List");
		}
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void VIDEO_ShowCFB(void)
{
	if(_VIDEO_ShowCFB != NULL)
	{
		__try
		{
			_VIDEO_ShowCFB();
		}

		__except(NULL, EXCEPTION_EXECUTE_HANDLER)
		{
			DisplayError("Exception in VIDEO_ShowCFB");
		}
	}
}

/*
 =======================================================================================================================
    Used when selecting plugins
 =======================================================================================================================
 */
void VIDEO_Under_Selecting_About(HWND hParent)
{
	if(_VIDEO_Under_Selecting_About != NULL)
	{
		_VIDEO_Under_Selecting_About(hParent);
	}
	else
	{
		DisplayError("%s: About information is not available for this plug-in.", "Video Plugin");
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void VIDEO_Under_Selecting_Test(HWND hParent)
{
	if(_VIDEO_Under_Selecting_Test != NULL)
	{
		_VIDEO_Under_Selecting_Test(hParent);
	}
	else
	{
		DisplayError("%s: Test function is not available for this plug-in.", "Video Plugin");
	}
}
