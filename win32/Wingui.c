/*$T wingui.c GC 1.136 03/09/02 17:33:42 */


/*$6
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    The Windows User Interface
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
#include <commdlg.h>
#include <direct.h>
#include <shlobj.h>
#include "wingui.h"
#include <shellapi.h>
#include "../globals.h"
#include "../debug_option.h"
#include "../hardware.h"
#include "../fileio.h"
#include "../emulator.h"
#include "../interrupt.h"
#include "../memory.h"
#include "../iPIF.h"
#include "../gamesave.h"
#include "DLL_Video.h"
#include "DLL_Audio.h"
#include "DLL_Input.h"
#include "DLL_RSP.h"
#include "registry.h"
#include "../r4300i.h"
#include "../timer.h"
#include "../romlist.h"
#include "../cheatcode.h"
#include "../compiler.h"

#ifdef WINDEBUG_1964
#include "windebug.h"
#endif
struct EMU1964GUI	gui;
struct GUIOPTIONS	guioptions;
struct DIRECTORIES	directories;
struct GUISTATUS	guistatus;

int					ActiveApp;

unsigned int		cfmenulist[8] =
{
	ID_CF_CF1,
	ID_CF_CF2,
	ID_CF_CF3,
	ID_CF_CF4,
	ID_CF_CF5,
	ID_CF_CF6,
	ID_CF_CF7,
	ID_CF_CF8
};

unsigned int		codecheckmenulist[8] =
{
	ID_CPU_DYNACODECHECKING_NOCHECK,
	ID_CPU_DYNACODECHECKING_DMA,
	ID_CPU_DYNACODECHECKING_DWORD,
	ID_CPU_DYNACODECHECKING_QWORD,
	ID_CPU_DYNACODECHECKING_QWORDANDDMA,
	ID_CPU_DYNACODECHECKING_BLOCK,
	ID_CPU_DYNACODECHECKING_BLOCKANDDMA,
	ID_CPU_DYNACODECHECKING_PROTECTMEMORY
};

char				recent_rom_directory_lists[MAX_RECENT_ROM_DIR][260];
char				recent_game_lists[MAX_RECENT_GAME_LIST][260];

char				game_country_name[10];
int					game_country_tvsystem = 0;

int					Audio_Is_Initialized = 0;
int					timer;
int					StateFileNumber = 1;
int					togglecursor = FALSE;
int					firstlaunch1964 = 1;

extern int			selected_rom_index;
extern BOOL			Is_Reading_Rom_File;
extern BOOL			To_Stop_Reading_Rom_File;
extern BOOL			opcode_debugger_memory_is_allocated;
extern HINSTANCE	hinstControllerPlugin;
BOOL				NeedFreshromListAfterStop = TRUE;

#ifdef DEBUG_COMMON
void					ToggleDebugOptions(WPARAM wParam);
#endif
LRESULT APIENTRY		OptionsDialog(HWND hDlg, unsigned message, WORD wParam, LONG lParam);
LRESULT APIENTRY		SetVideoPluginDialog(HWND hDlg, unsigned message, WORD wParam, LONG lParam);

void					SelectVISpeed(WPARAM wParam);
void					SetupAdvancedMenus(void);
void					RegenerateAdvancedUserMenus(void);
void					DeleteAdvancedUserMenus(void);
void					RegenerateStateSelectorMenus(void);
void					DeleteStateSelectorMenus(void);
void					RegerateRecentGameMenus(void);
void					DeleteRecentGameMenus(void);
void					RegerateRecentRomDirectoryMenus(void);
void					DeleteRecentRomDirectoryMenus(void);
void					RefreshRecentGameMenus(char *newgamefilename);
void					RefreshRecentRomDirectoryMenus(char *newromdirectory);
void					ChangeToRecentDirectory(int id);
void					OpenRecentGame(int id);
void					UpdateCIC(void);
void					init_debug_options(void);
extern LRESULT APIENTRY PluginsDialog(HWND hDlg, unsigned message, WORD wParam, LONG lParam);
long					OnNotifyStatusBar(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
long					OnPopupMenuCommand(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
long					OnOpcodeDebuggerCommands(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void					OnFreshRomList();

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void CALLBACK TimerProc(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime)
{
	if(Rom_Loaded)
	{
		if(emustatus.Emu_Is_Running)
		{
			if(!emustatus.Emu_Is_Paused)
			{
				vips = (float)(viCountePerSecond);

				if(vips >= 100.0)
				{
					sprintf(generalmessage, "%3d VI/s", (int) vips);
				}
				else
				{
					sprintf(generalmessage, "%3.1f VI/s", vips);
				}

				viCountePerSecond = 0;
				QueryPerformanceCounter(&LastSecondTime);

				if( guistatus.IsFullScreen == FALSE && guioptions.display_statusbar == TRUE)
				{
					SetStatusBarText(1, generalmessage);

					if(guioptions.display_profiler_status)
					{
						format_profiler_result_msg(generalmessage);
						reset_profiler();
						SetStatusBarText(0, generalmessage);
					}
					else if(guioptions.display_detail_status)
					{
						sprintf
							(
							generalmessage,
							"PC=%08x, DList=%d, AList=%d, PI=%d, Cont=%d",
							gHWS_pc,
							emustatus.DListCount,
							emustatus.AListCount,
							emustatus.PIDMACount,
							emustatus.ControllerReadCount
							);
						SetStatusBarText(0, generalmessage);
					}
				}

				/* Apply the hack codes */
				if(emuoptions.auto_apply_cheat_code)
				{
#ifndef CHEATCODE_LOCK_MEMORY
					CodeList_ApplyAllCode(INGAME);
#endif
				}

				if(rominfo.TV_System == TV_SYSTEM_NTSC) // if USA ROM
				{
					GEPDQueueRuntimeHacks();
					if(emustatus.gepd_pause)
					{
						emustatus.gepd_pause--;
						if(!emustatus.gepd_pause)
							GEPDPause(FALSE);
					}
				}
			}
		}
	}
	if(mouseinjectorpresent)
		CONTROLLER_HookRDRAM((DWORD *)TLB_sDWord_ptr, emuoptions.OverclockFactor);
}

LRESULT APIENTRY PropertyPagesProc(HWND hDlg, unsigned message, WORD wParam, LONG lParam)
{
	switch(message)
	{
	case WM_INITDIALOG:
		return(TRUE);
	
	case WM_NOTIFY:
		{
		LPNMHDR lpnm = (LPNMHDR) lParam;

        switch (lpnm->code)
            {
			case PSN_APPLY:
				EndDialog(lpnm->hwndFrom, TRUE);
				break;

            case PSN_RESET :
                //Handle a Cancel button click, if necessary
				EndDialog(lpnm->hwndFrom, TRUE);
				break;
			}
		}
	}
	return(FALSE);
}

#define DPI_AWARENESS_CONTEXT						(HANDLE)
#define DPI_AWARENESS_CONTEXT_UNAWARE				DPI_AWARENESS_CONTEXT-1
#define DPI_AWARENESS_CONTEXT_SYSTEM_AWARE			DPI_AWARENESS_CONTEXT-2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE		DPI_AWARENESS_CONTEXT-3
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2	DPI_AWARENESS_CONTEXT-4

void PreventWindowsDPIScaling(void)
{
	typedef BOOL (NTAPI *pSetProcessDPIAwarenessContext)(HWND hwnd);
	typedef BOOL (NTAPI *pSetProcessDPIAware)(void);
	pSetProcessDPIAwarenessContext SetProcessDPIAwarenessContext = NULL;
	pSetProcessDPIAware SetProcessDPIAware = NULL;

    HMODULE hinstUser32 = LoadLibraryW(L"user32.dll");
	if(!hinstUser32)
		return;

    SetProcessDPIAwarenessContext = (pSetProcessDPIAwarenessContext)GetProcAddress(hinstUser32, "SetProcessDPIAwarenessContext");
    if(SetProcessDPIAwarenessContext)
    {
        SetProcessDPIAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
    }
    else
    {
        SetProcessDPIAware = (pSetProcessDPIAware)GetProcAddress(hinstUser32, "SetProcessDPIAware");
        if(SetProcessDPIAware)
        {
            SetProcessDPIAware();
        }
    }
	FreeLibrary(hinstUser32);
}


//Test: Creating property pages for all options
void CreateOptionsDialog(void)
{
	/*~~~~~~~~~~~~~~~~~~~*/
	PROPSHEETPAGE	psp[4];
	PROPSHEETHEADER psh;
	/*~~~~~~~~~~~~~~~~~~~*/

	psp[0].dwSize = sizeof(PROPSHEETPAGE);
	psp[0].dwFlags = PSP_USETITLE;
	psp[0].hInstance = gui.hInst;
	psp[0].pszTemplate = "CHEAT_HACK";
	psp[0].pszIcon = NULL;
	psp[0].pfnDlgProc = (DLGPROC) PropertyPagesProc;
	psp[0].pszTitle = "Cheat Codes";
	psp[0].lParam = 0;

	psp[1].dwSize = sizeof(PROPSHEETPAGE);
	psp[1].dwFlags = PSP_USETITLE;
	psp[1].hInstance = gui.hInst;
	psp[1].pszTemplate = "OPTIONS";
	psp[1].pszIcon = NULL;
	psp[1].pfnDlgProc = (DLGPROC) PropertyPagesProc;
	psp[1].pszTitle = "User Options";
	psp[1].lParam = 0;

	psp[2].dwSize = sizeof(PROPSHEETPAGE);
	psp[2].dwFlags = PSP_USETITLE;
	psp[2].hInstance = gui.hInst;
	psp[2].pszTemplate = "PLUGINS";
	psp[2].pszIcon = NULL;
	psp[2].pfnDlgProc = (DLGPROC) PropertyPagesProc;
	psp[2].pszTitle = "Change Plugins";
	psp[2].lParam = 0;

	psp[3].dwSize = sizeof(PROPSHEETPAGE);
	psp[3].dwFlags = PSP_USETITLE;
	psp[3].hInstance = gui.hInst;
	psp[3].pszTemplate = "ROM_OPTIONS";
	psp[3].pszIcon = NULL;
	psp[3].pfnDlgProc = (DLGPROC) PropertyPagesProc;
	psp[3].pszTitle = "ROM Properties";
	psp[3].lParam = 0;

	psh.dwSize = sizeof(PROPSHEETHEADER);
	psh.dwFlags = PSH_PROPSHEETPAGE | PSH_NOAPPLYNOW;
	psh.hwndParent = NULL;
	psh.hInstance = gui.hInst;
	psh.pszIcon = NULL;
	psh.pszCaption = (LPSTR) "1964 Options";
	psh.nStartPage = 0;
	psh.nPages = sizeof(psp) / sizeof(PROPSHEETPAGE);
	psh.ppsp = (LPCPROPSHEETPAGE) & psp;

	{
	HWND hCOP2Vecswnd;
	
	hCOP2Vecswnd = (HWND) PropertySheet(&psh);
	}
}


extern HWND WINAPI CreateTT(HWND hwndOwner);
/*
 =======================================================================================================================
 =======================================================================================================================
 */
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
	/*~~~~*/
	MSG msg;
	/*~~~~*/

	if(hPrevInstance) return FALSE;
	SaveCmdLineParameter(lpszCmdLine);
	PreventWindowsDPIScaling();

	gui.szBaseWindowTitle = "1964 0.8.5";
	gui.hwnd1964main = NULL;		/* handle to main window */
	gui.hwndRomList = NULL;			/* Handle to the rom list child window */
	gui.hStatusBar = NULL;			/* Window Handle of the status bar */
	gui.hToolBar = NULL;			/* Window Handle of the toolbar */
	gui.hClientWindow = NULL;		/* Window handle of the client child window */
	gui.hCriticalMsgWnd = NULL;		/* handle to critical message window */
	gui.hMenu1964main = NULL;
	gui.hMenuRomListPopup = NULL;

	Rom_Loaded = FALSE;
	guistatus.block_menu = TRUE;	/* block all menu commands during startip */
	emustatus.cpucore = DYNACOMPILER;
	emustatus.Emu_Is_Resetting = FALSE;
	guistatus.IsFullScreen = FALSE;
	guistatus.IsBorderless = FALSE;

#ifdef DEBUG_COMMON
	init_debug_options();
#endif
	p_gHardwareState = (HardwareState *) &gHardwareState;
	p_gMemoryState = (MemoryState *) &gMemoryState;

	gui.hInst = hInstance;
	LoadString(hInstance, IDS_MAINDISCLAIMER, MainDisclaimer, sizeof(MainDisclaimer));

	Set_1964_Directory();
	firstlaunch1964 = ReadConfiguration();	/* System registry settings */

	gui.hwnd1964main = InitWin98UI(hInstance, nCmdShow);
	if(gui.hwnd1964main == NULL)
	{
		DisplayError("Could not get a windows handle.");
		exit(1);
	}

	SetupAdvancedMenus();
	SetupDebuger();
	if(guioptions.highfreqtimer)
		SetHighResolutionTimer();

#ifndef ENABLE_OPCODE_DEBUGGER
	DeleteMenu(gui.hMenu1964main, ID_OPCODEDEBUGGER, MF_BYCOMMAND);
	DeleteMenu(gui.hMenu1964main, ID_OPCODEDEBUGGER_BLOCK_ONLY, MF_BYCOMMAND);
	DeleteMenu(gui.hMenu1964main, ID_DIRTYONLY, MF_BYCOMMAND);
#else
	if(debug_opcode!=0)
	{
		CheckMenuItem(gui.hMenu1964main, ID_OPCODEDEBUGGER, MF_CHECKED);
	}
	else
	{
		CheckMenuItem(gui.hMenu1964main, ID_OPCODEDEBUGGER, MF_UNCHECKED);
	}
#endif

	gui.hStatusBar = CreateStatusWindow(WS_CHILD | WS_VISIBLE | WS_OVERLAPPED, gui.staturbar_field.field_1, gui.hwnd1964main, 0x1122);
	{
		/*~~~~~~~~~~~~*/
		RECT	rc, src;
		/*~~~~~~~~~~~~*/

		GetWindowRect(gui.hwnd1964main, &rc);
		GetWindowRect(gui.hStatusBar, &src);
		DockStatusBar();
	}
	SetupToolBar();

	InitVirtualMemory();
	InitPluginData();

	SetStatusBarText(0, "Load Rom Setting from 1964.ini");
	FileIO_Load1964Ini();

	SetWindowText(gui.hwnd1964main, gui.szBaseWindowTitle);
	emustatus.cpucore = defaultoptions.Emulator;

	SetStatusBarText(2, "CF=1");
	SetStatusBarText(3, "8MB");
	SetStatusBarText(4, "D");

	gui.hwndRomList = NewRomList_CreateListViewControl(gui.hwnd1964main);	/* this must be before the video plugin init */
	SetStatusBarText(0, "Loading plugins");
	LoadPlugins(LOAD_ALL_PLUGIN);

	EnableRadioButtons(FALSE);

	ShowWindow(gui.hwnd1964main, SW_SHOW);
	UpdateWindow(gui.hwnd1964main);
	
	if(guistatus.WindowIsMaximized)
	{
		ShowWindow(gui.hwnd1964main, SW_SHOWMAXIMIZED);
	}
	
	SetStatusBarText(0, "Initialize emulator and r4300 core");
	r4300i_Init();
	timer = SetTimer(gui.hwnd1964main, 1, 1000, TimerProc);

/*	if( emuoptions.UsingRspPlugin )
	{
		EnableMenuItem(gui.hMenu1964main, ID_RSP_CONFIG, MF_ENABLED);
	}
	else
	{
		EnableMenuItem(gui.hMenu1964main, ID_RSP_CONFIG, MF_GRAYED);
	}
*/	
	if( StartGameByCommandLine() )
	{
	}
	else
	{
		NeedFreshromListAfterStop = FALSE;
		NewRomList_ListViewChangeWindowRect();
		DockStatusBar();

		SetStatusBarText(0, "Looking for ROM file in the ROM directory and Generate List");
		RomListReadDirectory(directories.rom_directory_to_use);
		NewRomList_ListViewFreshRomList();

		Set_Ready_Message();
	}
	guistatus.block_menu = FALSE;	/* allow menu commands */

	if(guioptions.show_critical_msg_window)
	{
		if(gui.hCriticalMsgWnd == NULL)
		{
			gui.hCriticalMsgWnd = CreateDialog(gui.hInst, "CRITICAL_MESSAGE", NULL, (DLGPROC) CriticalMessageDialog);
			SetActiveWindow(gui.hwnd1964main);
		}
	}

	SetFocus(gui.hwnd1964main);
	//CreateOptionsDialog();
	if(firstlaunch1964) // ask user what video plugin they wish to use and open the change ROM directory dialog window on first launch
	{
		/*~~~~~~~~~~~~*/
		char temp_video_plugin[256];
		/*~~~~~~~~~~~~*/

		strcpy(temp_video_plugin, gRegSettings.VideoPlugin);
		DialogBox(gui.hInst, "PICKVIDEOPLUGIN", gui.hwnd1964main, (DLGPROC)SetVideoPluginDialog);
		if(strcmp(gRegSettings.VideoPlugin, temp_video_plugin) != 0) // if plugin choice is different to default plugin, load new plugin
		{
			CloseVideoPlugin();
			LoadPlugins(LOAD_VIDEO_PLUGIN);
		}
		ChangeDirectory();
		MessageBox(gui.hwnd1964main, "4\t\t- Toggle mouse injector\nF3\t\t- Pause emulation (toggle)\nF4\t\t- Stop emulation\nF5\t\t- Quicksave\nF7\t\t- Quickload\nLSHIFT+1..9\t- Select State\nALT+ENTER\t- Fullscreen toggle\n\nFull list of hotkeys are in BUNDLE_README.txt located in the 1964 directory.", "1964 Hotkeys", MB_ICONINFORMATION | MB_OK);
	}


	while(1) // new message pump
	{
		if(GetMessage(&msg, NULL, 0, 0) > 0) // messages to be processed
		{
			if(!TranslateAccelerator(gui.hwnd1964main, gui.hAccTable, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else
				Sleep(10);
		}
		else // error, abort
			break;
	}
	return msg.wParam;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
HWND InitWin98UI(HANDLE hInstance, int nCmdShow)
{
	/*~~~~~~~~~~~*/
	WNDCLASS	wc;
	/*~~~~~~~~~~~*/

	wc.style = CS_SAVEBITS; /* | CS_DBLCLKS; */
	wc.lpfnWndProc = MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = (HINSTANCE) hInstance;
	wc.hIcon = LoadIcon((HINSTANCE) hInstance, MAKEINTRESOURCE(IDI_ICON2));
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);	/* hBrush; */
	wc.lpszMenuName = "WINGUI_MENU";

	wc.lpszClassName = "WinGui";

	RegisterClass(&wc);

	gui.hwnd1964main = CreateWindow
		(
			"WinGui",
			gui.szBaseWindowTitle,
			WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN,	/* | WS_VSCROLL, */
			guistatus.window_position.left,
			guistatus.window_position.top,
			guistatus.clientwidth,
			guistatus.clientheight,
			NULL,
			NULL,
			(HINSTANCE) hInstance,
			NULL
		);


	ShowWindow(gui.hToolBar, SW_SHOW);

	if(gui.hwnd1964main == NULL)
	{
		MessageBox(NULL, "CreateWindow() failed: Cannot create a window.", "Error", MB_OK);
		return(NULL);
	}

	gui.hAccTable = LoadAccelerators(gui.hInst, (LPCTSTR) WINGUI_ACC);
	gui.hMenu1964main = GetMenu(gui.hwnd1964main);

	SetOverclockFactor(emuoptions.OverclockFactor);
	SetCounterFactor(defaultoptions.Counter_Factor);

	return gui.hwnd1964main;
}

void ProcessMenuCommand(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int PlayButtonState; //For pause/Play
	BOOL wasRunning = !emustatus.Emu_Is_Paused; // flag to resume emulating after closing plugin dialog
	
	if(guistatus.block_menu)
			return; /* ok, all menu commands are blocked */

		switch(LOWORD(wParam))
		{
		case ID_ROM_STOP:
		case ID_BUTTON_STOP:
			CloseROM();
			break;
		case ID_ROM_PAUSE:
			ChangeButtonState(ID_BUTTON_PAUSE);
			PlayButtonState = ChangeButtonState(ID_BUTTON_PLAY);
			if((PlayButtonState &0x01) != TBSTATE_CHECKED)
			{
				PauseEmulator();
				HideCursor(FALSE);
			}
			else
			{
				ResumeEmulator(DO_NOTHING_AFTER_PAUSE);
				HideCursor(TRUE);
			}
			break;
		case ID_BUTTON_PLAY:
			ResumeEmulator(DO_NOTHING_AFTER_PAUSE);
			break;
		case ID_BUTTON_PAUSE:
			PauseEmulator();
			break;
		case ID_CPU_KILL:
			KillCPUThread();
			break;
		case ID_OPENROM:
		case ID_BUTTON_OPEN_ROM:

			EnableButton(ID_BUTTON_OPEN_ROM, FALSE);
			EnableMenuItem(gui.hMenu1964main, ID_OPENROM, MF_GRAYED);
			EnableButton(ID_BUTTON_SETUP_PLUGINS, FALSE);
			EnableMenuItem(gui.hMenu1964main, IDM_PLUGINS, MF_GRAYED);

			if (!guistatus.IsFullScreen)
			OpenROM();
			break;
		case ID_CLOSEROM:
			CloseROM();
			break;
		case ID_FILE_ROMINFO:
		case ID_BUTTON_ROM_PROPERTIES:
			if (!guistatus.IsFullScreen)
			RomListRomOptions(selected_rom_index);
			break;
		case ID_FILE_FRESHROMLIST:
			if (!guistatus.IsFullScreen)
			OnFreshRomList();
			break;
		case ID_DEFAULTOPTIONS:
			if (!guistatus.IsFullScreen)
			DialogBox(gui.hInst, "DEFAULT_OPTIONS", hWnd, (DLGPROC) DefaultOptionsDialog);
			break;
		case ID_PREFERENCE_OPTIONS:
			if (!guistatus.IsFullScreen)
			DialogBox(gui.hInst, "Options", hWnd, (DLGPROC) OptionsDialog);
			break;
		case ID_CHANGEDIRECTORY:
			if (!guistatus.IsFullScreen)
			ChangeDirectory();
			break;
		case ID_FILE_ROMDIRECTORY1:
			ChangeToRecentDirectory(0);
			break;
		case ID_FILE_ROMDIRECTORY2:
			if (!guistatus.IsFullScreen)
			ChangeToRecentDirectory(1);
			break;
		case ID_FILE_ROMDIRECTORY3:
			ChangeToRecentDirectory(2);
			break;
		case ID_FILE_ROMDIRECTORY4:
			ChangeToRecentDirectory(3);
			break;
		case ID_FILE_ROMDIRECTORY5:
			ChangeToRecentDirectory(4);
			break;
		case ID_FILE_ROMDIRECTORY6:
			ChangeToRecentDirectory(5);
			break;
		case ID_FILE_ROMDIRECTORY7:
			ChangeToRecentDirectory(6);
			break;
		case ID_FILE_ROMDIRECTORY8:
			ChangeToRecentDirectory(7);
			break;
		case ID_FILE_RECENTGAMES_GAME1:
			OpenRecentGame(0);
			break;
		case ID_FILE_RECENTGAMES_GAME2:
			OpenRecentGame(1);
			break;
		case ID_FILE_RECENTGAMES_GAME3:
			OpenRecentGame(2);
			break;
		case ID_FILE_RECENTGAMES_GAME4:
			OpenRecentGame(3);
			break;
		case ID_FILE_RECENTGAMES_GAME5:
			OpenRecentGame(4);
			break;
		case ID_FILE_RECENTGAMES_GAME6:
			OpenRecentGame(5);
			break;
		case ID_FILE_RECENTGAMES_GAME7:
			OpenRecentGame(6);
			break;
		case ID_FILE_RECENTGAMES_GAME8:
			OpenRecentGame(7);
			break;
		case ID_FILE_CHEAT:
			if (!guistatus.IsFullScreen)
			if(emustatus.Emu_Is_Running)
			{
				if(!emustatus.Emu_Is_Paused)
					PauseEmulator();
				//SuspendThread(CPUThreadHandle);
				HideCursor(FALSE);
				DialogBox(gui.hInst, "CHEAT_HACK", hWnd, (DLGPROC) CheatAndHackDialog);
				//ResumeThread(CPUThreadHandle);
				if(wasRunning)
					ResumeEmulator(DO_NOTHING_AFTER_PAUSE);
			}
			else
			{
				CodeList_ReadCode(romlist[selected_rom_index]->pinientry->Game_Name);
				DialogBox(gui.hInst, "CHEAT_HACK", hWnd, (DLGPROC) CheatAndHackDialog);
			}
			break;
		case ID_ABOUT:
			if (!guistatus.IsFullScreen)
			DialogBox(gui.hInst, "ABOUTBOX", hWnd, (DLGPROC) About);
			break;
		case ID_CHEATS_APPLY:
			CodeList_ApplyAllCode(GSBUTTON);
			break;
		case ID_CPU_AUDIOSYNC:
			if(emuoptions.SyncVI)
			{
				CheckMenuItem(gui.hMenu1964main, ID_CPU_AUDIOSYNC, MF_UNCHECKED);
				emuoptions.SyncVI = FALSE;
			}
			else
			{
				CheckMenuItem(gui.hMenu1964main, ID_CPU_AUDIOSYNC, MF_CHECKED);
				emuoptions.SyncVI = TRUE;
			}
			break;

		case ID_VIDEO_CONFIG:
			if (!guistatus.IsFullScreen)
			{
				if(emustatus.Emu_Is_Running)
				{
					if(!emustatus.Emu_Is_Paused)
						PauseEmulator();
					/* SuspendThread(CPUThreadHandle); */
					HideCursor(FALSE);
					VIDEO_DllConfig(hWnd);

					/* ResumeThread(CPUThreadHandle); */
					if(wasRunning)
						ResumeEmulator(DO_NOTHING_AFTER_PAUSE);
				}
				else
				{
					HideCursor(FALSE);
					VIDEO_DllConfig(hWnd);
					NewRomList_ListViewChangeWindowRect();
				}

				DockStatusBar();
			}
			break;
		case ID_AUD_CONFIG:
			if (!guistatus.IsFullScreen)
			if(emustatus.Emu_Is_Running)
			{
				if(!emustatus.Emu_Is_Paused)
					PauseEmulator();
				SuspendThread(CPUThreadHandle);
				HideCursor(FALSE);
				AUDIO_DllConfig(hWnd);
				ResumeThread(CPUThreadHandle);
				if(wasRunning)
					ResumeEmulator(DO_NOTHING_AFTER_PAUSE);
			}
			else
			{
				HideCursor(FALSE);
				AUDIO_DllConfig(hWnd);
			}
			break;
		case ID_DI_CONFIG:
			if (!guistatus.IsFullScreen)
			if(emustatus.Emu_Is_Running)
			{
				if(!emustatus.Emu_Is_Paused)
					PauseEmulator();
				/* SuspendThread(CPUThreadHandle); */
				HideCursor(FALSE);
				CONTROLLER_DllConfig(hWnd);
				/* ResumeThread(CPUThreadHandle); */
				if(wasRunning)
					ResumeEmulator(DO_NOTHING_AFTER_PAUSE);
			}
			else
			{
				HideCursor(FALSE);
				CONTROLLER_DllConfig(hWnd);
			}
			break;
		case ID_RSP_CONFIG:
			if (!guistatus.IsFullScreen)
			{
				if(emustatus.Emu_Is_Running && !emustatus.Emu_Is_Paused)
					PauseEmulator();
				RSPDllConfig(hWnd);
				if(wasRunning)
					ResumeEmulator(DO_NOTHING_AFTER_PAUSE);
			}
			break;
		case ID_INTERPRETER:
			CheckMenuItem(gui.hMenu1964main, ID_INTERPRETER, MF_CHECKED);
			CheckMenuItem(gui.hMenu1964main, ID_STATICCOMPILER, MF_UNCHECKED);
			CheckMenuItem(gui.hMenu1964main, ID_DYNAMICCOMPILER, MF_UNCHECKED);
			EmulatorSetCore(INTERPRETER);
			break;
		case ID_STATICCOMPILER:
			CheckMenuItem(gui.hMenu1964main, ID_INTERPRETER, MF_UNCHECKED);
			CheckMenuItem(gui.hMenu1964main, ID_STATICCOMPILER, MF_CHECKED);
			CheckMenuItem(gui.hMenu1964main, ID_DYNAMICCOMPILER, MF_UNCHECKED);
			EmulatorSetCore(1);
			break;
		case ID_DYNAMICCOMPILER:
			CheckMenuItem(gui.hMenu1964main, ID_INTERPRETER, MF_UNCHECKED);
			CheckMenuItem(gui.hMenu1964main, ID_STATICCOMPILER, MF_UNCHECKED);
			CheckMenuItem(gui.hMenu1964main, ID_DYNAMICCOMPILER, MF_CHECKED);
			EmulatorSetCore(DYNACOMPILER);
			break;
		case ID_CF_CF1:
			SetCounterFactor(1);
			break;
		case ID_CF_CF2:
			SetCounterFactor(2);
			break;
		case ID_CF_CF3:
			SetCounterFactor(3);
			break;
		case ID_CF_CF4:
			SetCounterFactor(4);
			break;
		case ID_CF_CF5:
			SetCounterFactor(5);
			break;
		case ID_CF_CF6:
			SetCounterFactor(6);
			break;
		case ID_CF_CF7:
			SetCounterFactor(7);
			break;
		case ID_CF_CF8:
			SetCounterFactor(8);
			break;
		case ID_CPU_DYNACODECHECKING_NOCHECK:
			SetCodeCheckMethod(1);
			break;
		case ID_CPU_DYNACODECHECKING_DMA:
			SetCodeCheckMethod(2);
			break;
		case ID_CPU_DYNACODECHECKING_DWORD:
			SetCodeCheckMethod(3);
			break;
		case ID_CPU_DYNACODECHECKING_QWORD:
			SetCodeCheckMethod(4);
			break;
		case ID_CPU_DYNACODECHECKING_QWORDANDDMA:
			SetCodeCheckMethod(5);
			break;
		case ID_CPU_DYNACODECHECKING_BLOCK:
			SetCodeCheckMethod(6);
			break;
		case ID_CPU_DYNACODECHECKING_BLOCKANDDMA:
			SetCodeCheckMethod(7);
			break;
		case ID_CPU_DYNACODECHECKING_PROTECTMEMORY:
			SetCodeCheckMethod(8);
			break;
		case ID_BUTTON_FULL_SCREEN:
		case IDM_FULLSCREEN:
			if(emustatus.Emu_Is_Running)
			{
				if(emustatus.Emu_Is_Paused && !guistatus.IsFullScreen)
					ResumeEmulator(DO_NOTHING_AFTER_PAUSE);
				if(guioptions.borderless_fullscreen == TRUE)
				{
					if(!guistatus.IsBorderless)
						SetWindowBorderless();
					else
						UnsetWindowBorderless();
				}
				else
					VIDEO_ChangeWindow(guistatus.IsFullScreen);
			}
			break;
		case ID_PLUGINS_SCREENSHOTS:
			CaptureScreenToFile();
			break;
		case ID_TOGGLECURSOR:
			if(!guioptions.auto_hide_cursor_when_active && emustatus.Emu_Is_Running)
				HideCursor(showcursor);
			break;
		case IDM_PLUGINS:
		case ID_BUTTON_SETUP_PLUGINS:
			if (!guistatus.IsFullScreen)
				DialogBox(gui.hInst, "PLUGINS", hWnd, (DLGPROC) PluginsDialog);
			break;
		case ID_CHECKWEB:
		case ID_ONLINE_HELP:
		case ID_BUTTON_HELP:
			if (!guistatus.IsFullScreen)
				MessageBox(gui.hwnd1964main, "If you are having issues, please read the file BUNDLE_README.txt located in the 1964 directory.", "Help", MB_OK);
			break;
		case ID_CONFIGURE_VIDEO:
			VIDEO_DllConfig(hWnd);
			break;
		case ID_HELP_FINDER:
			DisplayError("Help contents");
			break;
		case ID_OVERCLOCKSTOCK:
			SetOverclockFactor(1);
			SetCounterFactor(currentromoptions.Counter_Factor);
			break;
		case ID_OVERCLOCK6:
		case ID_OVERCLOCK9:
		case ID_OVERCLOCK12:
		case ID_OVERCLOCK15:
		case ID_OVERCLOCK18:
			SetOverclockFactor((LOWORD(wParam) - ID_OVERCLOCK6 + 2) * 3);
			SetCounterFactor(COUTERFACTOR_1);
			break;
		case ID_GEFIRINGHACK:
			if(emuoptions.GEFiringRateHack)
			{
				CheckMenuItem(gui.hMenu1964main, ID_GEFIRINGHACK, MF_UNCHECKED);
				emuoptions.GEFiringRateHack = FALSE;
				if (!guistatus.IsFullScreen && emustatus.Emu_Is_Running)
					MessageBox(gui.hwnd1964main, "Please restart ROM to disable firing rate hack.", "Information", MB_ICONINFORMATION | MB_OK);
			}
			else
			{
				CheckMenuItem(gui.hMenu1964main, ID_GEFIRINGHACK, MF_CHECKED);
				emuoptions.GEFiringRateHack = TRUE;
				if (!guistatus.IsFullScreen && emustatus.Emu_Is_Running)
					MessageBox(gui.hwnd1964main, "Please restart ROM to enable firing rate hack.", "Information", MB_ICONINFORMATION | MB_OK);
			}
			break;
		case ID_GEDISABLEHEADROLL:
			if(emuoptions.GEDisableHeadRoll)
			{
				CheckMenuItem(gui.hMenu1964main, ID_GEDISABLEHEADROLL, MF_UNCHECKED);
				emuoptions.GEDisableHeadRoll = FALSE;
				if (!guistatus.IsFullScreen && emustatus.Emu_Is_Running)
					MessageBox(gui.hwnd1964main, "Please restart ROM to enable head roll.", "Information", MB_ICONINFORMATION | MB_OK);
			}
			else
			{
				CheckMenuItem(gui.hMenu1964main, ID_GEDISABLEHEADROLL, MF_CHECKED);
				emuoptions.GEDisableHeadRoll = TRUE;
				if (!guistatus.IsFullScreen && emustatus.Emu_Is_Running)
					MessageBox(gui.hwnd1964main, "Please restart ROM to disable head roll.", "Information", MB_ICONINFORMATION | MB_OK);
			}
			break;
		case ID_PDSPEEDHACK:
			if(emuoptions.PDSpeedHack)
			{
				CheckMenuItem(gui.hMenu1964main, ID_PDSPEEDHACK, MF_UNCHECKED);
				emuoptions.PDSpeedHack = FALSE;
			}
			else
			{
				CheckMenuItem(gui.hMenu1964main, ID_PDSPEEDHACK, MF_CHECKED);
				emuoptions.PDSpeedHack = TRUE;
			}
			break;
		case ID_ABOUT_WARRANTY:
			if (!guistatus.IsFullScreen)
			{
				LoadString(gui.hInst, IDS_WARRANTY_SEC11, WarrantyPart1, 700);
				LoadString(gui.hInst, IDS_WARRANTY_SEC12, WarrantyPart2, 700);
				MessageBox(gui.hwnd1964main, WarrantyPart1, "NO WARRANTY", MB_OK);
				MessageBox(gui.hwnd1964main, WarrantyPart2, "NO WARRANTY", MB_OK);
			}
			break;

		case ID_REDISTRIBUTE:
			if (!guistatus.IsFullScreen)
			DialogBox(gui.hInst, "REDISTRIB_DIALOG", hWnd, (DLGPROC) ConditionsDialog);
			break;

		case ID_OPCODEDEBUGGER:
		case ID_OPCODEDEBUGGER_BLOCK_ONLY:
		case ID_DIRTYONLY:
			OnOpcodeDebuggerCommands(hWnd, message, wParam, lParam);
			break;
		case ID_SAVE_STATE_1:
		case ID_SAVE_STATE_2:
		case ID_SAVE_STATE_3:
		case ID_SAVE_STATE_4:
		case ID_SAVE_STATE_5:
		case ID_SAVE_STATE_6:
		case ID_SAVE_STATE_7:
		case ID_SAVE_STATE_8:
		case ID_SAVE_STATE_9:
		case ID_SAVE_STATE_0:
			SaveStateByNumber(wParam);
			break;
		case ID_LOAD_STATE_1:
		case ID_LOAD_STATE_2:
		case ID_LOAD_STATE_3:
		case ID_LOAD_STATE_4:
		case ID_LOAD_STATE_5:
		case ID_LOAD_STATE_6:
		case ID_LOAD_STATE_7:
		case ID_LOAD_STATE_8:
		case ID_LOAD_STATE_9:
		case ID_LOAD_STATE_0:
			LoadStateByNumber(wParam);
			break;
		case ID_SAVESTATE:
			if (!guistatus.IsFullScreen)
			SaveStateByDialog(SAVE_STATE_1964_FORMAT);
			break;
		case ID_LOADSTATE:
			if (!guistatus.IsFullScreen)
			LoadStateByDialog(SAVE_STATE_1964_FORMAT);
			break;
		case ID_CPU_IMPORTPJ64STATE:
			if (!guistatus.IsFullScreen)
			LoadStateByDialog(SAVE_STATE_PJ64_FORMAT);
			break;
		case ID_CPU_EXPORTPJ64STATE:
			if (!guistatus.IsFullScreen)
			SaveStateByDialog(SAVE_STATE_PJ64_FORMAT);
			break;
		case ID_POPUP_LOADPLAY:
		case ID_POPUP_LOADPLAYINFULLSCREEN:
		case ID_POPUP_LOADPLAYINWINDOWMODE:
		case ID_POPUP_ROM_SETTING:
		case ID_POPUP_CHEATCODE:
		case ID_HEADERPOPUP_SHOW_INTERNAL_NAME:
		case ID_HEADERPOPUP_SHOWALTERNATEROMNAME:
		case ID_HEADERPOPUP_SHOWROMFILENAME:
		case ID_HEADERPOPUP_1_SORT_ASCENDING:
		case ID_HEADERPOPUP_1_SORT_DESCENDING:
		case ID_HEADERPOPUP_2_SORT_ASCENDING:
		case ID_HEADERPOPUP_2_SORT_DESCENDING:
		case ID_HEADERPOPUP_1_SELECTING:
		case ID_HEADERPOPUP_2_SELECTING:
			OnPopupMenuCommand(hWnd, message, wParam, lParam);
			break;
		case ID_EXIT:
			KillTimer(hWnd, timer);
			Exit1964();
			break;
		default:
#ifdef DEBUG_COMMON
			ProcessDebugMenuCommand(wParam);
#endif
			break;
		}
}

void ProcessToolTips(LPARAM lParam)
{
    LPTOOLTIPTEXT lpttt; 

    lpttt = (LPTOOLTIPTEXT) lParam; 
    lpttt->hinst = gui.hInst; 

    // Specify the resource identifier of the descriptive 
    // text for the given button. 
    switch (lpttt->hdr.idFrom) 
	{ 
		case ID_BUTTON_OPEN_ROM:
			lpttt->lpszText = MAKEINTRESOURCE(ID_BUTTON_OPEN_ROM); 
			break;

		case ID_BUTTON_PLAY:
			lpttt->lpszText = MAKEINTRESOURCE(ID_BUTTON_PLAY);
			break; 

		case ID_BUTTON_PAUSE:
			lpttt->lpszText = MAKEINTRESOURCE(ID_BUTTON_PAUSE);
			break; 

		case ID_BUTTON_STOP:
			lpttt->lpszText = MAKEINTRESOURCE(ID_BUTTON_STOP);
			break;

		case ID_BUTTON_SETUP_PLUGINS:
			lpttt->lpszText = MAKEINTRESOURCE(ID_BUTTON_SETUP_PLUGINS);
			break;

		case ID_BUTTON_ROM_PROPERTIES:
			lpttt->lpszText = MAKEINTRESOURCE(ID_BUTTON_ROM_PROPERTIES); 
            break;

		case ID_BUTTON_HELP:
			lpttt->lpszText = MAKEINTRESOURCE(ID_BUTTON_HELP); 
            break; 

		case ID_BUTTON_FULL_SCREEN:
			lpttt->lpszText = MAKEINTRESOURCE(ID_BUTTON_FULL_SCREEN); 
            break;
    }
}

void OnWindowSize(WPARAM wParam)
{
	RECT rcStatusBar;
	RECT rcRomList;
	RECT rcToolBar;

	if(gui.hToolBar != NULL)
	{
		RECT rcMainWnd;

		GetClientRect(gui.hToolBar, &rcToolBar);
		GetWindowRect(gui.hwnd1964main, &rcMainWnd);
		SendMessage(gui.hReBar, WM_SIZE, 0, 0); //This will resize it, but there's bad flicker.
	}

	GetWindowRect(gui.hStatusBar, &rcStatusBar);

	if (gui.hwndRomList != NULL)
	{
		GetClientRect(gui.hwnd1964main, &rcRomList);
		if(gui.hToolBar != NULL)
		{
			rcRomList.top += (rcToolBar.bottom - rcToolBar.top - 1);
			rcRomList.bottom -= (rcToolBar.bottom - rcToolBar.top - 1);
		}
		rcRomList.bottom -= (rcStatusBar.bottom - rcStatusBar.top);
		SetWindowPos(gui.hwndRomList, HWND_BOTTOM, 0, rcRomList.top, rcRomList.right, rcRomList.bottom, 0);
		UpdateWindow(gui.hwndRomList);
	}

	DockStatusBar();

	if(wParam == SIZE_MAXIMIZED && !guistatus.window_is_maximized)
	{
		guistatus.window_is_maximized = TRUE;
		DockStatusBar();
	}
    else if(wParam == SIZE_MINIMIZED && !guistatus.window_is_minimized)
	{
		guistatus.window_is_minimized = TRUE;
	}
	else if(guistatus.window_is_maximized || guistatus.window_is_minimized)
	{
		DockStatusBar();
	}
}

void ProcessKeyboardInput(UINT message, WPARAM wParam, LPARAM lParam)
{
	BOOL				ctrlkey;
	BOOL				shiftkey;
	/* Disable Alt Key for menus in full screen */
     BYTE keymap[256];

    if ((wParam == VK_CONTROL) && (guistatus.IsFullScreen))
    {
		GetKeyboardState (keymap);
        keymap [(lParam & 0x1000000) ? VK_RCONTROL:VK_LCONTROL] &= ~0x80;
        SetKeyboardState (keymap);
    }
    else if (wParam == VK_MENU)
    {
		GetKeyboardState (keymap);
        keymap [(lParam & 0x1000000) ? VK_RMENU:VK_LMENU] &= ~0x80;
        SetKeyboardState (keymap);
    }

    if (message == WM_SYSKEYUP && wParam == VK_MENU) {
        /* ignore ALT key up event to stop it activating the menus */
		return;
    }

	ctrlkey = GetKeyState(VK_CONTROL) & 0xFF000000;
	shiftkey = GetKeyState(VK_LSHIFT) & 0xFF000000;
	switch(wParam)
	{
	case VK_F5:
		SaveState();
	break;
	
	case VK_F7:
		LoadState();
	break;
	
	case VK_F4:
	case VK_ESCAPE:
		if (guistatus.IsFullScreen)
		{
			if(emustatus.Emu_Is_Running)
			{
				if(PauseEmulator())
				{
					VIDEO_ChangeWindow(guistatus.IsFullScreen);
					ResumeEmulator(DO_NOTHING_AFTER_PAUSE);
				}
			}
		}
		else if(guistatus.IsBorderless == TRUE)
			UnsetWindowBorderless();
		if (wParam == VK_F4)
			CloseROM();
	break;

	case 0x30:
	case 0x31:
	case 0x32:
	case 0x33:
	case 0x34:
	case 0x35:
	case 0x36:
	case 0x37:
	case 0x38:
	case 0x39:
		if(shiftkey && !ctrlkey)
		{
			StateSetNumber(wParam - 0x30);
			MessageBeep(MB_ICONASTERISK);
		}
	break;
	
	default:
		CONTROLLER_WM_KeyUp(wParam, lParam);
	break;
	}
}


/*
 =======================================================================================================================
 =======================================================================================================================
 */
long FAR PASCAL MainWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	static PAINTSTRUCT	ps;
	static int			ok = 0;
	static BOOL			gamepausebyinactive = FALSE;	/* static for this looks like a bad idea. */
	static int			MenuCausedPause = FALSE;
	static int			lastCursorCheckCounter = 0;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	switch(message)
	{
	case WM_ACTIVATE:
		if(guioptions.pause_at_inactive)
		{
			BOOL	minimize = (BOOL) HIWORD(wParam);

			switch(LOWORD(wParam))
			{
			case WA_ACTIVE:
			case WA_CLICKACTIVE:
				if( emustatus.Emu_Is_Running && 
				    emustatus.Emu_Is_Paused  &&
				    gamepausebyinactive )
				{
					Resume();
					gamepausebyinactive = FALSE;
				}
			break;
			
			case WA_INACTIVE:
				if( minimize && 
				    emustatus.Emu_Is_Running &&
				    emustatus.Emu_Is_Paused == FALSE )
				{
					Pause();
					gamepausebyinactive = TRUE;
				}
			break;
			}
		}
	break;

	case WM_SETCURSOR:
		if(guioptions.auto_hide_cursor_when_active && emustatus.Emu_Is_Running && !emustatus.Emu_Is_Paused)
		{
			lastCursorCheckCounter++;
			if(lastCursorCheckCounter > 50)
			{
				HideCursor(HTCLIENT == LOWORD(lParam));
				lastCursorCheckCounter = 0;
			}
		}
	break;

	case WM_PAINT:
		BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
	break;

	case WM_ACTIVATEAPP:
		ActiveApp = wParam;
	break;

	case WM_SETFOCUS:
		ActiveApp = wParam;
	break;

	case WM_MOVE:
		if(emustatus.Emu_Is_Running)
		{
			/*~~~~~~~~~~~*/
			int xPos, yPos;
			/*~~~~~~~~~~~*/

			xPos = (int) (short) LOWORD(lParam);		/* horizontal position */
			yPos = (int) (short) HIWORD(lParam);		/* vertical position */
			VIDEO_MoveScreen(xPos, yPos);
		}
	break;
	
	case WM_EXITSIZEMOVE:
		guistatus.window_is_moving = FALSE;
	break;
		
	case WM_SIZE:
		OnWindowSize(wParam);
	break;

	case WM_MOVING:
		guistatus.window_is_moving = TRUE;
	break;

	case WM_KEYDOWN:
		CONTROLLER_WM_KeyDown(wParam, lParam);
	break;
	
	case WM_KEYUP:
    case WM_SYSKEYUP:
		ProcessKeyboardInput(message, wParam, lParam);
	break;

	case WM_NOTIFY:
		switch (((LPNMHDR) lParam)->code) 
		{ 
		    case TTN_GETDISPINFO: 
                ProcessToolTips(lParam);
				break;
		}
		if(((LPNMHDR) lParam)->hwndFrom == gui.hwndRomList)
		{
			OnNotifyRomList(hWnd, message, wParam, lParam);
		}
		else if(((LPNMHDR) lParam)->hwndFrom == gui.hStatusBar )
		{
			OnNotifyStatusBar(hWnd, message, wParam, lParam);
		}
		else if(((LPNMHDR) lParam)->hwndFrom == ListView_GetHeader(gui.hwndRomList) )
		{
			OnNotifyRomListHeader(hWnd, message, wParam, lParam);
		}
		else
		{
			return(DefWindowProc(hWnd, message, wParam, lParam));
		}
	break;

	case WM_COMMAND:
		ProcessMenuCommand(hWnd, message, wParam, lParam);
	break;

	case WM_ENTERMENULOOP:
		/* To pause game when user enter the menu bar */
		MenuCausedPause = emustatus.Emu_Is_Paused;
		if(guioptions.ok_to_pause_at_menu)
			if(emustatus.Emu_Is_Running && !emustatus.Emu_Is_Paused )
			{
				PauseEmulator();

			}
		emustatus.Emu_Is_Paused = MenuCausedPause;
	break;

	case WM_EXITMENULOOP:
		/* To resume game when user leaves the menu bar */
		if(guioptions.ok_to_pause_at_menu)
			if(emustatus.Emu_Is_Running && !MenuCausedPause )
			{
				ResumeEmulator(DO_NOTHING_AFTER_PAUSE);
			}
	break;

	case WM_CLOSE:
		KillTimer(hWnd, timer);
		Exit1964();
	break;

	case WM_POWERBROADCAST:
		switch( wParam )
		{
#ifndef PBT_APMQUERYSUSPEND
#define PBT_APMQUERYSUSPEND 0x0000
#endif
		case PBT_APMQUERYSUSPEND:
			// At this point, the app should save any data for open
			// network connections, files, etc., and prepare to go into
			// a suspended mode.
			return TRUE;
			
#ifndef PBT_APMRESUMESUSPEND
#define PBT_APMRESUMESUSPEND 0x0007
#endif
		case PBT_APMRESUMESUSPEND:
			// At this point, the app should recover any data, network
			// connections, files, etc., and resume running from when
			// the app was suspended.
			return TRUE;
		}
	break;

	case WM_SYSCOMMAND:
		switch (wParam) 
		{
		case SC_SCREENSAVE:
		case SC_MONITORPOWER:
			if(emustatus.Emu_Is_Running)
				return 1;	//Disable screen saver
		case SC_MOVE:
		case SC_SIZE:
		case SC_MAXIMIZE:
		case SC_KEYMENU:
			if (guistatus.IsFullScreen)
				return 1;
		default:
			return(DefWindowProc(hWnd, message, wParam, lParam));
		}
	break;
	}
	
	return(DefWindowProc(hWnd, message, wParam, lParam));
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void Pause(void)
{
//	if(emustatus.Emu_Is_Running)
//	{
//		if(!emustatus.Emu_Is_Paused)
//		{
			PauseEmulator();
//		}
//		else
//		{
//			ResumeEmulator(DO_NOTHING_AFTER_PAUSE);
//			sprintf(generalmessage, "%s - Running", gui.szWindowTitle);
//			SetWindowText(gui.hwnd1964main, generalmessage);
//		}
//	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void Resume(void)
{
	if(emustatus.Emu_Is_Running && emustatus.Emu_Is_Paused)
	{
		ResumeEmulator(DO_NOTHING_AFTER_PAUSE);
	}
}

void AfterStop(void);

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void KillCPUThread(void)
{
	if(emustatus.Emu_Is_Running)
	{
		SuspendThread(CPUThreadHandle);
		TerminateThread(CPUThreadHandle, 1);
		CloseHandle(CPUThreadHandle);

		if(currentromoptions.Code_Check == CODE_CHECK_PROTECT_MEMORY) UnprotectAllBlocks();

		AUDIO_RomClosed();
		CONTROLLER_RomClosed();
		VIDEO_RomClosed();

		AfterStop();
	}

	//To finish off the window placement and settings
	Rom_Loaded = TRUE;
	CloseROM(); 
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void Kill(void)
{
	StopEmulator();
	CPUThreadHandle = NULL;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */

void Play(BOOL WithFullScreen)
{
	/*~~~~~*/
	int core;
	/*~~~~~*/

	if(Rom_Loaded)
	{
		if(emustatus.Emu_Is_Running) 
		{
			Stop();
		}

		PrepareBeforePlay(guistatus.IsFullScreen);

		core = currentromoptions.Emulator;
		if(core == DYNACOMPILER)
		{					/* Dynarec */
			CheckMenuItem(gui.hMenu1964main, ID_INTERPRETER, MF_UNCHECKED);
			CheckMenuItem(gui.hMenu1964main, ID_DYNAMICCOMPILER, MF_CHECKED);
			emustatus.cpucore = DYNACOMPILER;
		}
		else
		{					/* Interpreter */
			CheckMenuItem(gui.hMenu1964main, ID_INTERPRETER, MF_CHECKED);
			CheckMenuItem(gui.hMenu1964main, ID_DYNAMICCOMPILER, MF_UNCHECKED);
			emustatus.cpucore = INTERPRETER;
		}

		if ((GFX_PluginRECT.UseThis == TRUE) && (emustatus.Emu_Is_Resetting == FALSE))
		{
			RECT Rect;

			GetWindowRect(gui.hwnd1964main, &Rect);
			SetWindowPos
			(
			gui.hwnd1964main,
			NULL,
			Rect.left,
			Rect.top,
			GFX_PluginRECT.rect.right - GFX_PluginRECT.rect.left,
			GFX_PluginRECT.rect.bottom - GFX_PluginRECT.rect.top,
			SWP_NOZORDER | SWP_SHOWWINDOW
			);
		}

		if (emustatus.Emu_Is_Resetting == 0)
			DockStatusBar();

		r4300i_Reset();
		RunEmulator(emustatus.cpucore);

		//EnableMenuItem(gui.hMenu1964main, ID_OPENROM, MF_GRAYED);
		EnableMenuItem(gui.hMenu1964main, IDM_PLUGINS, MF_GRAYED);
		EnableButton(ID_BUTTON_SETUP_PLUGINS, FALSE);

		EnableMenuItem(gui.hMenu1964main, ID_ROM_PAUSE, MF_ENABLED);
		EnableRadioButtons(TRUE);
		CheckButton(ID_BUTTON_PLAY, TRUE);
		EnableMenuItem(gui.hMenu1964main, ID_ROM_STOP, MF_ENABLED);
		EnableStateMenu();

		if(GfxPluginVersion == 0x0103)
		{
			EnableMenuItem(gui.hMenu1964main, ID_PLUGINS_SCREENSHOTS, MF_ENABLED);
		}
		else
		{
			EnableMenuItem(gui.hMenu1964main, ID_PLUGINS_SCREENSHOTS, MF_GRAYED);
		}

		sprintf(generalmessage, "%s - Running", gui.szWindowTitle);
		SetWindowText(gui.hwnd1964main, generalmessage);
		//if(emuoptions.auto_full_screen)
		if(WithFullScreen && (emustatus.Emu_Is_Resetting == 0))
		{
			if(guistatus.IsFullScreen == 0)
			{
				if(guioptions.borderless_fullscreen == TRUE)
				{
					if(!guistatus.IsBorderless)
						SetWindowBorderless();
					else
						UnsetWindowBorderless();
				}
				else
					VIDEO_ChangeWindow(guistatus.IsFullScreen);
			}
		}
	}
	else
		DisplayError("Please load a ROM first.");
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void Stop()
{
	if(emustatus.Emu_Is_Running)
	{
		StopEmulator();

		if (emustatus.Emu_Is_Resetting == 0)
		{
			if(guistatus.IsFullScreen)
			{
				VIDEO_ChangeWindow(guistatus.IsFullScreen);
				if(guioptions.borderless_fullscreen == TRUE)
					UnsetWindowBorderless();
			}
			HideCursor(FALSE);
			AfterStop();
		}
	}
	else if(Is_Reading_Rom_File)
	{
		To_Stop_Reading_Rom_File = TRUE;
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void RomListGetGoodRomNameToDisplay(char *buf, int index)
{
	switch( romlistNameToDisplay )
	{
	case ROMLIST_DISPLAY_ALTER_NAME:
		if(strlen(romlist[index]->pinientry->Alt_Title) > 1)
		{
			strcpy(buf, romlist[index]->pinientry->Alt_Title);
			break;
		}
	case ROMLIST_DISPLAY_INTERNAL_NAME:
		if(InternalNameIsValid(romlist[index]->pinientry->Game_Name))
		{
			strcpy(buf, romlist[index]->pinientry->Game_Name);
			break;
		}		
	case ROMLIST_DISPLAY_FILENAME:
		{
			/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
			char	drive[_MAX_DIR], dir[_MAX_DIR];
			char	fname[_MAX_DIR], ext[_MAX_EXT];
			/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
			
			_splitpath(romlist[index]->romfilename, drive, dir, fname, ext);
			strcat(fname, ext);
			strcpy(buf, fname);
		}
		break;
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void CloseROM(void)
{
	WINDOWPLACEMENT placement;

	GetWindowPlacement(gui.hwnd1964main, &placement);
	
	if(emustatus.Emu_Is_Running)
	{
		Stop();

		if (placement.showCmd == SW_SHOWMAXIMIZED)
		{
			guistatus.WindowIsMaximized = TRUE;
			ShowWindow(gui.hwnd1964main, SW_SHOWNORMAL);
			ShowWindow(gui.hwnd1964main, SW_SHOWMAXIMIZED);
		}

		Close_iPIF();
		GEPDRestoreROMHacks();
		FreeVirtualRomMemory();
		r4300i_Init();

		Rom_Loaded = FALSE;

		EnableMenuItem(gui.hMenu1964main, ID_OPENROM, MF_ENABLED);
		EnableButton(ID_BUTTON_OPEN_ROM, TRUE);
		EnableMenuItem(gui.hMenu1964main, IDM_PLUGINS, MF_ENABLED);
		EnableButton(ID_BUTTON_SETUP_PLUGINS, TRUE);
		EnableRadioButtons(FALSE);

		EnableMenuItem(gui.hMenu1964main, ID_ROM_PAUSE, MF_GRAYED);

		/* EnableMenuItem(gui.hMenu1964main, ID_ROM_STOP, MF_GRAYED); */
		EnableMenuItem(gui.hMenu1964main, ID_CLOSEROM, MF_GRAYED);
		EnableMenuItem(gui.hMenu1964main, ID_FILE_ROMINFO, MF_GRAYED);
		EnableButton(ID_BUTTON_ROM_PROPERTIES, FALSE);
		EnableMenuItem(gui.hMenu1964main, ID_FILE_CHEAT, MF_GRAYED);

		SetWindowText(gui.hwnd1964main, gui.szBaseWindowTitle);
	}
	HideCursor(FALSE);
	/*
	 * else
	 * DisplayError("Please load a ROM first.");
	 */
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void OpenROM(void)
{
	if(Rom_Loaded) 
	{
		CloseROM();
	}

	if(WinLoadRom() == TRUE)	/* If the user opened a rom, */
	{
		EnableRadioButtons(TRUE);
		EnableMenuItem(gui.hMenu1964main, ID_CLOSEROM, MF_ENABLED);
		EnableMenuItem(gui.hMenu1964main, ID_FILE_ROMINFO, MF_ENABLED);
		EnableButton(ID_BUTTON_ROM_PROPERTIES, TRUE);
		EnableMenuItem(gui.hMenu1964main, ID_FILE_CHEAT, MF_ENABLED);

		Play(emuoptions.auto_full_screen); /* autoplay */
	}
	else
	{
		EnableButton(ID_BUTTON_OPEN_ROM, TRUE);
		EnableMenuItem(gui.hMenu1964main, ID_OPENROM, MF_ENABLED);
		EnableButton(ID_BUTTON_SETUP_PLUGINS, TRUE);
		EnableMenuItem(gui.hMenu1964main, IDM_PLUGINS, MF_ENABLED);
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
BOOL WinLoadRom(void)
{
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	OPENFILENAME	ofn;
	char			szFileName[MAXFILENAME];
	char			szFileTitle[MAXFILENAME];
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	memset(&szFileName, 0, sizeof(szFileName));
	memset(&szFileTitle, 0, sizeof(szFileTitle));

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = gui.hwnd1964main;
	ofn.lpstrFilter = "N64 ROMs (*.ZIP, *.V64, *.BIN, *.ROM, *.Z64, *.N64, *.USA, *.PAL, *.J64)\0*.ZIP;*.V64;*.BIN;*.ROM;*.Z64;*.N64;*.USA;*.PAL;*.J64\0All Files (*.*)\0*.*\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = szFileName;
	ofn.nMaxFile = MAXFILENAME;
	ofn.lpstrInitialDir = directories.rom_directory_to_use;
	ofn.lpstrFileTitle = szFileTitle;
	ofn.nMaxFileTitle = MAXFILENAME;
	ofn.lpstrTitle = "Open ROM";
	ofn.lpstrDefExt = "TXT";
	ofn.Flags = OFN_HIDEREADONLY;

	if(!GetOpenFileName((LPOPENFILENAME) & ofn))
	{
		return FALSE;
	}

	_getcwd(directories.rom_directory_to_use, MAX_PATH);
	if ( strcmp(directories.last_rom_directory, directories.rom_directory_to_use) != 0)
	{ 
		NeedFreshromListAfterStop = TRUE;
	}
	strcpy(directories.last_rom_directory, directories.rom_directory_to_use);

	WriteConfiguration();

	if(WinLoadRomStep2(szFileName))
	{
		/*~~~~~~~~~~~~~~~~~*/
		INI_ENTRY	*pentry;
		long		filesize;
		/*~~~~~~~~~~~~~~~~~*/

		/* Check and create romlist entry for this new loaded rom */
		pentry = GetNewIniEntry();
		ReadRomHeaderInMemory(pentry);
		filesize = ReadRomHeader(szFileName, pentry);
		RomListAddEntry(pentry, szFileName, filesize);
		DeleteIniEntryByEntry(pentry);

		/* Read hack code for this rom */
		CodeList_ReadCode(rominfo.name);
		RefreshRecentGameMenus(szFileName);
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/*
=======================================================================================================================
=======================================================================================================================
*/
BOOL WinLoadRomStep2(char *szFileName)
{
	lstrcpy(gui.szWindowTitle, gui.szBaseWindowTitle);
	lstrcat(gui.szWindowTitle, " - ");
	
	if(ReadRomData(szFileName) == FALSE) 
	{
		Rom_Loaded = FALSE;
		return FALSE;
	}
	
	lstrcat(gui.szWindowTitle, rominfo.name);
	
	memcpy(&HeaderDllPass[0], &gMemoryState.ROM_Image[0], 0x40);
	EnableMenuItem(gui.hMenu1964main, IDM_PLUGINS, MF_GRAYED);
	EnableButton(ID_BUTTON_SETUP_PLUGINS, FALSE);
	EnableMenuItem(gui.hMenu1964main, ID_CLOSEROM, MF_ENABLED);
	SetWindowText(gui.hwnd1964main, gui.szWindowTitle);
	
	Rom_Loaded = TRUE;
	gHWS_pc = 0xA4000040;	/* We do it in r4300i_inithardware */
	
	UpdateCIC();
	sprintf(generalmessage, "%s - Loaded", gui.szWindowTitle);
	SetWindowText(gui.hwnd1964main, generalmessage);
	Set_Ready_Message();
	
	return TRUE;
}

BOOL StartGameByCommandLine()
{
	char szFileName[300], temp[300];
	GetCmdLineParameter(CMDLINE_GAME_FILENAME, szFileName);
	if( strlen(szFileName) == 0 )
	{
		return FALSE;
	}
	strcpy(temp, directories.last_rom_directory);
	strcat(temp, "\\");
	strcat(temp, szFileName);
	strcpy(szFileName, temp);

	if(WinLoadRomStep2(szFileName))
	{
		/*~~~~~~~~~~~~~~~~~*/
		INI_ENTRY	*pentry;
		long		filesize;
		char		tempstr[20];
		char		ocfactor;
		/*~~~~~~~~~~~~~~~~~*/
		
		/* Check and create romlist entry for this new loaded rom */
		pentry = GetNewIniEntry();
		ReadRomHeaderInMemory(pentry);
		filesize = ReadRomHeader(szFileName, pentry);
		RomListAddEntry(pentry, szFileName, filesize);
		DeleteIniEntryByEntry(pentry);
		
		/* Read hack code for this rom */
		CodeList_ReadCode(rominfo.name);
		RefreshRecentGameMenus(szFileName);

		EnableRadioButtons(TRUE);
		EnableMenuItem(gui.hMenu1964main, ID_CLOSEROM, MF_ENABLED);
		EnableMenuItem(gui.hMenu1964main, ID_FILE_ROMINFO, MF_ENABLED);
		EnableButton(ID_BUTTON_ROM_PROPERTIES, TRUE);
		EnableMenuItem(gui.hMenu1964main, ID_FILE_CHEAT, MF_ENABLED);
		
		GetCmdLineParameter(CMDLINE_OC_FACTOR, tempstr);
		if( strlen(tempstr) > 0 )
		{
			ocfactor = atoi(tempstr);
			if (ocfactor >= 0 && ocfactor <= 18)
				SetOverclockFactor(ocfactor);
		}

		GetCmdLineParameter(CMDLINE_FULL_SCREEN_FLAG, tempstr);
		if( strlen(tempstr) > 0 )
		{
			Play(TRUE);
		}
		else
		{
			Play(emuoptions.auto_full_screen); /* autoplay */
		}

		return TRUE;
	}
	else
	{
		return FALSE;
	}
	
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void SetHighResolutionTimer(void)
{
	LONG (NTAPI *_NtQueryTimerResolution) (unsigned long *minimumResolution, unsigned long *maximumResolution, unsigned long *currentResolution) = NULL;
	LONG (NTAPI *_NtSetTimerResolution) (unsigned long desiredResolution, BOOL setResolution, unsigned long *currentResolution) = NULL;
	unsigned long minResolution = 0U, maxResolution = 0U, curResolution = 0U;
	HINSTANCE hinstNTDLL = NULL;
	hinstNTDLL = LoadLibrary("ntdll.dll");
	if(hinstNTDLL != NULL)
	{
		_NtQueryTimerResolution = (LONG(NTAPI *) (unsigned long *minimumResolution, unsigned long *maximumResolution, unsigned long *currentResolution)) GetProcAddress(hinstNTDLL, "NtQueryTimerResolution");
		_NtSetTimerResolution = (LONG(NTAPI *) (unsigned long desiredResolution, BOOL setResolution, unsigned long *currentResolution)) GetProcAddress(hinstNTDLL, "NtSetTimerResolution");
		if(_NtSetTimerResolution != NULL && _NtQueryTimerResolution != NULL)
		{
			_NtQueryTimerResolution(&minResolution, &maxResolution, &curResolution);
			_NtSetTimerResolution(maxResolution, TRUE, &curResolution);
		}
		else
			DisplayError("Error: Could not increase Kernel Timing resolution");
		FreeLibrary(hinstNTDLL);
	}
	else
		DisplayError("Could not load ntdll.dll");
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void OpenRecentGame(int id)
{
	if(emustatus.Emu_Is_Running) return;

	if(id >= 0 && id < 8)
	{
		if(WinLoadRomStep2(recent_game_lists[id]))
		{
			/*~~~~~~~~~~~~~~~~~*/
			INI_ENTRY	*pentry;
			long		filesize;
			/*~~~~~~~~~~~~~~~~~*/

			/* Check and create romlist entry for this new loaded rom */
			pentry = GetNewIniEntry();
			ReadRomHeaderInMemory(pentry);
			filesize = ReadRomHeader(recent_game_lists[id], pentry);
			RomListAddEntry(pentry, recent_game_lists[id], filesize);
			DeleteIniEntryByEntry(pentry);

			/* Read hack code for this rom */
			CodeList_ReadCode(rominfo.name);

			EnableRadioButtons(TRUE);
			EnableMenuItem(gui.hMenu1964main, ID_CLOSEROM, MF_ENABLED);
			EnableMenuItem(gui.hMenu1964main, ID_FILE_ROMINFO, MF_ENABLED);
			EnableButton(ID_BUTTON_ROM_PROPERTIES, TRUE);
			EnableMenuItem(gui.hMenu1964main, ID_FILE_CHEAT, MF_ENABLED);

			strcpy(generalmessage, recent_game_lists[id]);
			RefreshRecentGameMenus(generalmessage);

			Play(emuoptions.auto_full_screen); /* autoplay */
		}
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void SaveState(void)
{
	if(Rom_Loaded && !emustatus.gepd_pause)
	{
		if(emustatus.Emu_Is_Running)
		{
			if(PauseEmulator())
			{
				sprintf(generalmessage, "%s - Saving State %d", gui.szWindowTitle, StateFileNumber);
				SetStatusBarText(0, generalmessage);

				FileIO_gzSaveState();
				ResumeEmulator(DO_NOTHING_AFTER_PAUSE);
			}
		}
		else
		{
			sprintf(generalmessage, "%s - Saving State %d", gui.szWindowTitle, StateFileNumber);
			SetStatusBarText(0, generalmessage);

			FileIO_gzSaveState();
		}
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void LoadState(void)
{
	/*~~~~~~~~~~~~~~~~~~~~*/
	int was_running = FALSE;
	/*~~~~~~~~~~~~~~~~~~~~*/

	if(Rom_Loaded && !emustatus.gepd_pause)
	{
		if(emustatus.Emu_Is_Running)
		{
			if(PauseEmulator())
			{
				sprintf(generalmessage, "%s - Loading State %d", gui.szWindowTitle, StateFileNumber);
				SetStatusBarText(0, generalmessage);
				FileIO_gzLoadState();
				Init_Count_Down_Counters();
				ResumeEmulator(REFRESH_DYNA_AFTER_PAUSE);
			}
		}
		else
		{
			sprintf(generalmessage, "%s - Loading State %d", gui.szWindowTitle, StateFileNumber);
			SetStatusBarText(0, generalmessage);
			FileIO_gzLoadState();
			Init_Count_Down_Counters();
		}
		GEPDPause(TRUE);
	}
}

unsigned int	statesavemenulist[10] =
{
	ID_SAVE_STATE_0,
	ID_SAVE_STATE_1,
	ID_SAVE_STATE_2,
	ID_SAVE_STATE_3,
	ID_SAVE_STATE_4,
	ID_SAVE_STATE_5,
	ID_SAVE_STATE_6,
	ID_SAVE_STATE_7,
	ID_SAVE_STATE_8,
	ID_SAVE_STATE_9
};
unsigned int	stateloadmenulist[10] =
{
	ID_LOAD_STATE_0,
	ID_LOAD_STATE_1,
	ID_LOAD_STATE_2,
	ID_LOAD_STATE_3,
	ID_LOAD_STATE_4,
	ID_LOAD_STATE_5,
	ID_LOAD_STATE_6,
	ID_LOAD_STATE_7,
	ID_LOAD_STATE_8,
	ID_LOAD_STATE_9
};

/*
 =======================================================================================================================
 =======================================================================================================================
 */

void StateSetNumber(int number)
{
	CheckMenuItem(gui.hMenu1964main, statesavemenulist[StateFileNumber], MF_UNCHECKED);
	CheckMenuItem(gui.hMenu1964main, stateloadmenulist[StateFileNumber], MF_UNCHECKED);
	StateFileNumber = number;
	CheckMenuItem(gui.hMenu1964main, statesavemenulist[StateFileNumber], MF_CHECKED);
	CheckMenuItem(gui.hMenu1964main, stateloadmenulist[StateFileNumber], MF_CHECKED);
	sprintf(generalmessage, "%s - Selected State Slot %d", gui.szWindowTitle, number);
	SetStatusBarText(0, generalmessage);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void EnableStateMenu(void)
{
	/*~~*/
	int i;
	/*~~*/

	EnableMenuItem(gui.hMenu1964main, ID_SAVESTATE, MF_ENABLED);
	EnableMenuItem(gui.hMenu1964main, ID_LOADSTATE, MF_ENABLED);
	EnableMenuItem(gui.hMenu1964main, ID_CPU_IMPORTPJ64STATE, MF_ENABLED);
	EnableMenuItem(gui.hMenu1964main, ID_CPU_EXPORTPJ64STATE, MF_ENABLED);

	for(i = 0; i < 10; i++)
	{
		EnableMenuItem(gui.hMenu1964main, statesavemenulist[i], MF_ENABLED);
		EnableMenuItem(gui.hMenu1964main, stateloadmenulist[i], MF_ENABLED);
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void DisableStateMenu(void)
{
	/*~~*/
	int i;
	/*~~*/

	EnableMenuItem(gui.hMenu1964main, ID_SAVESTATE, MF_GRAYED);
	EnableMenuItem(gui.hMenu1964main, ID_LOADSTATE, MF_GRAYED);
	EnableMenuItem(gui.hMenu1964main, ID_CPU_IMPORTPJ64STATE, MF_GRAYED);
	EnableMenuItem(gui.hMenu1964main, ID_CPU_EXPORTPJ64STATE, MF_GRAYED);

	for(i = 0; i < 10; i++)
	{
		EnableMenuItem(gui.hMenu1964main, statesavemenulist[i], MF_GRAYED);
		EnableMenuItem(gui.hMenu1964main, stateloadmenulist[i], MF_GRAYED);
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void SaveStateByNumber(WPARAM wparam)
{
	/*~~*/
	int i;
	/*~~*/

	for(i = 0; i < 10; i++)
	{
		CheckMenuItem(gui.hMenu1964main, statesavemenulist[i], MF_UNCHECKED);
		CheckMenuItem(gui.hMenu1964main, stateloadmenulist[i], MF_UNCHECKED);
		if(statesavemenulist[i] == wparam)
		{
			StateFileNumber = i;
			CheckMenuItem(gui.hMenu1964main, statesavemenulist[i], MF_CHECKED);
			CheckMenuItem(gui.hMenu1964main, stateloadmenulist[i], MF_CHECKED);
		}
	}

	SaveState();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void LoadStateByNumber(WPARAM wparam)
{
	/*~~*/
	int i;
	/*~~*/

	for(i = 0; i < 10; i++)
	{
		CheckMenuItem(gui.hMenu1964main, statesavemenulist[i], MF_UNCHECKED);
		CheckMenuItem(gui.hMenu1964main, stateloadmenulist[i], MF_UNCHECKED);
		if(stateloadmenulist[i] == wparam)
		{
			StateFileNumber = i;
			CheckMenuItem(gui.hMenu1964main, statesavemenulist[i], MF_CHECKED);
			CheckMenuItem(gui.hMenu1964main, stateloadmenulist[i], MF_CHECKED);
		}
	}

	LoadState();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void SaveStateByDialog(int format)
{
	if(!Rom_Loaded) return;
	if(!PauseEmulator())
		return;
	else
	{
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
		OPENFILENAME	ofn;
		char			szFileName[MAXFILENAME];
		char			szFileTitle[MAXFILENAME];
		char			szPath[_MAX_PATH];
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

		memset(&szFileName, 0, sizeof(szFileName));
		memset(&szFileTitle, 0, sizeof(szFileTitle));
		memset(szPath, 0, _MAX_PATH);

		strcpy(szPath, directories.save_directory_to_use);

		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hwndOwner = gui.hwnd1964main;
		if(format == SAVE_STATE_1964_FORMAT)
			ofn.lpstrFilter = "1964 State File (*.sav?)\0*.SAV?\0All Files (*.*)\0*.*\0";
		else
			ofn.lpstrFilter = "Project 64 State File (*.pj?)\0*.PJ;*.PJ?\0All Files (*.*)\0*.*\0";
		ofn.lpstrCustomFilter = NULL;
		ofn.nMaxCustFilter = 0;
		ofn.nFilterIndex = 1;
		ofn.lpstrFile = szFileName;
		ofn.nMaxFile = MAXFILENAME;
		ofn.lpstrInitialDir = szPath;
		ofn.lpstrFileTitle = szFileTitle;
		ofn.nMaxFileTitle = MAXFILENAME;
		ofn.lpstrTitle = "Save State";
		ofn.lpstrDefExt = "";
		ofn.Flags = OFN_ENABLESIZING | OFN_HIDEREADONLY;

		if(GetSaveFileName((LPOPENFILENAME) & ofn))
		{
			if(format == SAVE_STATE_1964_FORMAT)
				FileIO_gzSaveStateFile(szFileName);
			else
				FileIO_ExportPJ64State(szFileName);
		}

		if(emustatus.Emu_Is_Running) ResumeEmulator(DO_NOTHING_AFTER_PAUSE);
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void LoadStateByDialog(int format)
{
	if(!Rom_Loaded) return;
	if(!PauseEmulator())
		return;
	else
	{
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
		OPENFILENAME	ofn;
		char			szFileName[MAXFILENAME];
		char			szFileTitle[MAXFILENAME];
		char			szPath[_MAX_PATH];
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

		memset(&szFileName, 0, sizeof(szFileName));
		memset(&szFileTitle, 0, sizeof(szFileTitle));
		memset(szPath, 0, _MAX_PATH);

		strcpy(szPath, directories.save_directory_to_use);

		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hwndOwner = gui.hwnd1964main;
		if(format == SAVE_STATE_1964_FORMAT)
			ofn.lpstrFilter = "1964 State File (*.sav?)\0*.SAV?\0All Files (*.*)\0*.*\0";
		else
			ofn.lpstrFilter = "Project 64 State File (*.pj?)\0*.PJ;*.PJ?\0All Files (*.*)\0*.*\0";
		ofn.lpstrCustomFilter = NULL;
		ofn.nMaxCustFilter = 0;
		ofn.nFilterIndex = 1;
		ofn.lpstrFile = szFileName;
		ofn.nMaxFile = MAXFILENAME;
		ofn.lpstrInitialDir = szPath;
		ofn.lpstrFileTitle = szFileTitle;
		ofn.nMaxFileTitle = MAXFILENAME;
		ofn.lpstrTitle = "Load State";
		ofn.lpstrDefExt = "";
		ofn.Flags = OFN_ENABLESIZING | OFN_HIDEREADONLY;

		if(GetOpenFileName((LPOPENFILENAME) & ofn))
		{
			if(format == SAVE_STATE_1964_FORMAT)
				FileIO_gzLoadStateFile(szFileName);
			else
				FileIO_ImportPJ64State(szFileName);
		}

		if(emustatus.Emu_Is_Running) ResumeEmulator(REFRESH_DYNA_AFTER_PAUSE);
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
BOOL SelectDirectory(char *title, char buffer[MAX_PATH])
{
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	BROWSEINFO		bi;
	char			pszBuffer[MAX_PATH];
	LPITEMIDLIST	pidl;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	bi.hwndOwner = gui.hwnd1964main;
	bi.pidlRoot = NULL;
	bi.pszDisplayName = pszBuffer;
	bi.lpszTitle = title;
	bi.ulFlags = BIF_RETURNFSANCESTORS | BIF_RETURNONLYFSDIRS;
	bi.lpfn = NULL;
	bi.lParam = 0;

	if((pidl = SHBrowseForFolder(&bi)) != NULL)
	{
		if(SHGetPathFromIDList(pidl, buffer))
		{
			return TRUE;
		}
	}

	return FALSE;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void ChangeDirectory(void)
{
	/*~~~~~~~~~~~~~~~~~~~*/
	char	path[MAX_PATH];
	/*~~~~~~~~~~~~~~~~~~~*/

	if(emustatus.Emu_Is_Running) return;

	if(SelectDirectory("Select a ROM folder", path))
	{
		strcpy(directories.rom_directory_to_use, path);
		strcpy(directories.last_rom_directory, path);
		WriteConfiguration();
		RefreshRecentRomDirectoryMenus(path);

		ClearRomList();
		SetStatusBarText(0, "Looking for ROM file(s) in the ROM folder and Generating List");
		RomListReadDirectory(directories.rom_directory_to_use);
		NewRomList_ListViewFreshRomList();
		Set_Ready_Message();
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void ChangeToRecentDirectory(int id)
{
	if(emustatus.Emu_Is_Running) return;

	if(id >= 0 && id < MAX_RECENT_ROM_DIR)
	{
		strcpy(generalmessage, recent_rom_directory_lists[id]);
		strcpy(directories.rom_directory_to_use, generalmessage);
		strcpy(directories.last_rom_directory, generalmessage);
		WriteConfiguration();
		RefreshRecentRomDirectoryMenus(generalmessage);

		ClearRomList();
		SetStatusBarText(0, "Looking for ROM file in the ROM folder and Generate List");
		RomListReadDirectory(directories.rom_directory_to_use);
		NewRomList_ListViewFreshRomList();
		Set_Ready_Message();
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
LRESULT APIENTRY DefaultOptionsDialog(HWND hDlg, unsigned message, WORD wParam, LONG lParam)
{
	/*~~*/
	int i;
	/*~~*/

	switch(message)
	{
	case WM_INITDIALOG:
		SendDlgItemMessage
		(
			hDlg,
			IDC_DEFAULTOPTIONS_EXPANSIONPAK,
			BM_SETCHECK,
			defaultoptions.RDRAM_Size == RDRAMSIZE_8MB ? BST_CHECKED : BST_UNCHECKED,
			0
		);
		SendDlgItemMessage
		(
			hDlg,
			IDC_DEFAULTOPTIONS_USETLB,
			BM_SETCHECK,
			defaultoptions.Use_TLB == USETLB_YES ? BST_CHECKED : BST_UNCHECKED,
			0
		);
		SendDlgItemMessage
		(
			hDlg,
			IDC_DEFAULTOPTIONS_DMASEGMENTATION,
			BM_SETCHECK,
			emuoptions.dma_in_segments == USEDMASEG_YES ? BST_CHECKED : BST_UNCHECKED,
			0
		);
		SendDlgItemMessage
		(
			hDlg,
			IDC_DEFAULTOPTIONS_REGC,
			BM_SETCHECK,
			defaultoptions.Use_Register_Caching == USEREGC_YES ? BST_CHECKED : BST_UNCHECKED,
			0
		);
		SendDlgItemMessage
		(
			hDlg,
			IDC_DEFAULTOPTIONS_FPUHACK,
			BM_SETCHECK,
			defaultoptions.FPU_Hack == USEFPUHACK_YES ? BST_CHECKED : BST_UNCHECKED,
			0
		);

		SendDlgItemMessage(hDlg, IDC_DEFAULTOPTIONS_SAVETYPE, CB_RESETCONTENT, 0, 0);
		for(i = 1; i < 7; i++)
		{
			SendDlgItemMessage(hDlg, IDC_DEFAULTOPTIONS_SAVETYPE, CB_INSERTSTRING, i - 1, (LPARAM) save_type_names[i]);
			if(i == defaultoptions.Save_Type)
				SendDlgItemMessage(hDlg, IDC_DEFAULTOPTIONS_SAVETYPE, CB_SETCURSEL, i - 1, 0);
		}

		SendDlgItemMessage(hDlg, IDC_DEFAULTOPTIONS_CODECHECK, CB_RESETCONTENT, 0, 0);
		for(i = 1; i < 9; i++)
		{
			SendDlgItemMessage
			(
				hDlg,
				IDC_DEFAULTOPTIONS_CODECHECK,
				CB_INSERTSTRING,
				i - 1,
				(LPARAM) codecheck_type_names[i]
			);
			if(i == defaultoptions.Code_Check)
				SendDlgItemMessage(hDlg, IDC_DEFAULTOPTIONS_CODECHECK, CB_SETCURSEL, i - 1, 0);
		}

		SendDlgItemMessage(hDlg, IDC_DEFAULTOPTIONS_MAXVISPEED, CB_RESETCONTENT, 0, 0);
		for(i = 1; i < 5; i++)
		{
			SendDlgItemMessage
			(
				hDlg,
				IDC_DEFAULTOPTIONS_MAXVISPEED,
				CB_INSERTSTRING,
				i - 1,
				(LPARAM) maxfps_type_names[i]
			);
			if(i == defaultoptions.Max_FPS)
				SendDlgItemMessage(hDlg, IDC_DEFAULTOPTIONS_MAXVISPEED, CB_SETCURSEL, i - 1, 0);
		}

		SendDlgItemMessage(hDlg, IDC_DEFAULTOPTIONS_EEPROMSIZE, CB_RESETCONTENT, 0, 0);
		for(i = 1; i < 4; i++)
		{
			SendDlgItemMessage
			(
				hDlg,
				IDC_DEFAULTOPTIONS_EEPROMSIZE,
				CB_INSERTSTRING,
				i - 1,
				(LPARAM) eepromsize_type_names[i]
			);
			if(i == defaultoptions.Eeprom_size)
				SendDlgItemMessage(hDlg, IDC_DEFAULTOPTIONS_EEPROMSIZE, CB_SETCURSEL, i - 1, 0);
		}

		return(TRUE);

	case WM_COMMAND:
		switch(wParam)
		{
		case IDOK:
			{
				/* Read option setting from dialog */
				defaultoptions.RDRAM_Size =
					(SendDlgItemMessage(hDlg, IDC_DEFAULTOPTIONS_EXPANSIONPAK, BM_GETCHECK, 0, 0) == BST_CHECKED) +
					1;
				if(!emustatus.Emu_Is_Running)
					SetStatusBarText(3, defaultoptions.RDRAM_Size == RDRAMSIZE_4MB ? "4MB" : "8MB");
				defaultoptions.Save_Type = SendDlgItemMessage(hDlg, IDC_DEFAULTOPTIONS_SAVETYPE, CB_GETCURSEL, 0, 0) + 1;
				defaultoptions.Code_Check = SendDlgItemMessage(hDlg, IDC_DEFAULTOPTIONS_CODECHECK, CB_GETCURSEL, 0, 0) + 1;
				defaultoptions.Max_FPS = (uint8)SendDlgItemMessage(hDlg, IDC_DEFAULTOPTIONS_MAXVISPEED, CB_GETCURSEL, 0, 0) + 1;
				defaultoptions.Use_TLB = 2 - (SendDlgItemMessage(hDlg, IDC_DEFAULTOPTIONS_USETLB, BM_GETCHECK, 0, 0) == BST_CHECKED);
				defaultoptions.Eeprom_size = (uint8)SendDlgItemMessage
						(
							hDlg,
							IDC_DEFAULTOPTIONS_EEPROMSIZE,
							CB_GETCURSEL,
							0,
							0
						) +
					1;
				defaultoptions.Use_Register_Caching = (SendDlgItemMessage(hDlg, IDC_DEFAULTOPTIONS_REGC, BM_GETCHECK, 0, 0) == BST_CHECKED);
				defaultoptions.Use_Register_Caching = (defaultoptions.Use_Register_Caching ? USEREGC_YES : USEREGC_NO);
				defaultoptions.FPU_Hack = (SendDlgItemMessage(hDlg, IDC_DEFAULTOPTIONS_FPUHACK, BM_GETCHECK, 0, 0) == BST_CHECKED);
				defaultoptions.FPU_Hack = (defaultoptions.FPU_Hack ? USEFPUHACK_YES : USEFPUHACK_NO);
				emuoptions.dma_in_segments = (SendDlgItemMessage(hDlg, IDC_DEFAULTOPTIONS_DMASEGMENTATION, BM_GETCHECK, 0, 0) == BST_CHECKED);
				emuoptions.dma_in_segments = (emuoptions.dma_in_segments == 1 ? USEDMASEG_YES : USEDMASEG_NO);
				defaultoptions.DMA_Segmentation = emuoptions.dma_in_segments;

				EndDialog(hDlg, TRUE);
				return(TRUE);
			}

		case IDCANCEL:
			{
				EndDialog(hDlg, TRUE);
				return(TRUE);
			}
		}
	}

	return(FALSE);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
LRESULT APIENTRY OptionsDialog(HWND hDlg, unsigned message, WORD wParam, LONG lParam)
{
	/*~~~~~~~~~~~~~~~~~~~*/
	char	path[MAX_PATH];
	/*~~~~~~~~~~~~~~~~~~~*/

	switch(message)
	{
	case WM_INITDIALOG:
		SendDlgItemMessage
		(
			hDlg,
			IDC_OPTION_AUTOFULLSCREEN,
			BM_SETCHECK,
			emuoptions.auto_full_screen ? BST_CHECKED : BST_UNCHECKED,
			0
		);
		SendDlgItemMessage
		(
			hDlg,
			IDC_DEFAULTOPTIOS_PAUSEONMENU,
			BM_SETCHECK,
			guioptions.pause_at_menu ? BST_CHECKED : BST_UNCHECKED,
			0
		);
		SendDlgItemMessage
		(
			hDlg,
			IDC_DEFAULTOPTIONS_PAUSEWHENINACTIVE,
			BM_SETCHECK,
			guioptions.pause_at_inactive ? BST_CHECKED : BST_UNCHECKED,
			0
		);
		SendDlgItemMessage
		(
			hDlg,
			IDC_EXPERT_MODE,
			BM_SETCHECK,
			guioptions.show_expert_user_menu ? BST_CHECKED : BST_UNCHECKED,
			0
		);
		SendDlgItemMessage
		(
			hDlg,
			IDC_ENABLE_DIRECTORY_LIST,
			BM_SETCHECK,
			guioptions.show_recent_rom_directory_list ? BST_CHECKED : BST_UNCHECKED,
			0
		);
		SendDlgItemMessage
		(
			hDlg,
			IDC_ENABLE_GAME_LIST,
			BM_SETCHECK,
			guioptions.show_recent_game_list ? BST_CHECKED : BST_UNCHECKED,
			0
		);
		SendDlgItemMessage
		(
			hDlg,
			IDC_ENABLE_DETAIL_STATUS,
			BM_SETCHECK,
			guioptions.display_detail_status ? BST_CHECKED : BST_UNCHECKED,
			0
		);
		SendDlgItemMessage
		(
			hDlg,
			IDC_ENABLE_PROFILER,
			BM_SETCHECK,
			guioptions.display_profiler_status ? BST_CHECKED : BST_UNCHECKED,
			0
		);
		SendDlgItemMessage
		(
			hDlg,
			IDC_DISPLAY_STATUSBAR,
			BM_SETCHECK,
			guioptions.display_statusbar ? BST_CHECKED : BST_UNCHECKED,
			0
		);
		SendDlgItemMessage
		(
			hDlg,
			IDC_HIGHFREQTIMER,
			BM_SETCHECK,
			guioptions.highfreqtimer ? BST_CHECKED : BST_UNCHECKED,
			0
		);
		SendDlgItemMessage
		(
			hDlg,
			IDC_ENABLE_STATE_MENU,
			BM_SETCHECK,
			guioptions.show_state_selector_menu ? BST_CHECKED : BST_UNCHECKED,
			0
		);
		SendDlgItemMessage
		(
			hDlg,
			IDC_OPTION_ERROR_WINDOW,
			BM_SETCHECK,
			guioptions.show_critical_msg_window ? BST_CHECKED : BST_UNCHECKED,
			0
		);
		SendDlgItemMessage
		(
			hDlg,
			IDC_OPTION_BORDERLESSFULLSCREEN,
			BM_SETCHECK,
			guioptions.borderless_fullscreen ? BST_CHECKED : BST_UNCHECKED,
			0
		);
		SendDlgItemMessage
		(
			hDlg,
			IDC_AUTOHIDEMOUSEWHENACTIVE,
			BM_SETCHECK,
			guioptions.auto_hide_cursor_when_active ? BST_CHECKED : BST_UNCHECKED,
			0
		);
		SendDlgItemMessage
		(
			hDlg,
			IDC_OPTION_ROMBROWSER,
			BM_SETCHECK,
			guioptions.display_romlist ? BST_CHECKED : BST_UNCHECKED,
			0
		);
		SendDlgItemMessage
		(
			hDlg,
			IDC_OPTIONS_USE_DEFAULT_SAVE_DIRECTORY,
			BM_SETCHECK,
			guioptions.use_default_save_directory ? BST_CHECKED : BST_UNCHECKED,
			0
		);
		SendDlgItemMessage
		(
			hDlg,
			IDC_DEFAULTOPTIONS_USE1964PLUGINDIRECTORY,
			BM_SETCHECK,
			guioptions.use_default_plugin_directory ? BST_CHECKED : BST_UNCHECKED,
			0
		);
		SendDlgItemMessage
		(
			hDlg,
			IDC_DEFAULTOPTIONS_USELASTROMDIRECTORY,
			BM_SETCHECK,
			guioptions.use_last_rom_directory ? BST_CHECKED : BST_UNCHECKED,
			0
		);

		SetDlgItemText(hDlg, IDC_DEFAULTOPTIONS_STATESAVEDIRECTORY, state_save_directory);
		SetDlgItemText(hDlg, IDC_DEFAULTOPTIONS_ROMDIRECTORY, user_set_rom_directory);
		SetDlgItemText(hDlg, IDC_DEFAULTOPTIONS_SAVEDIRECTORY, user_set_save_directory);
		SetDlgItemText(hDlg, IDC_DEFAULTOPTIONS_PLUGINDIRECTORY, user_set_plugin_directory);

		return(TRUE);

	case WM_COMMAND:
		switch(wParam)
		{
		case IDC_DEFAULTOPTIONS_BUTTON_SAVEDIR:
			if(SelectDirectory("Selecting Game Save Directory", path))
			{
				strcat(path, "\\");
				SetDlgItemText(hDlg, IDC_DEFAULTOPTIONS_SAVEDIRECTORY, path);
			}
			break;
		case IDC_DEFAULTOPTIONS_BUTTON_PLUGINDIR:
			if(SelectDirectory("Selecting Plugin Directory", path))
			{
				strcat(path, "\\");
				SetDlgItemText(hDlg, IDC_DEFAULTOPTIONS_PLUGINDIRECTORY, path);
			}
			break;
		case IDC_DEFAULTOPTIONS_BUTTON_ROMDIR:
			if(SelectDirectory("Selecting Default ROM Directory", path))
			{
				strcat(path, "\\");
				SetDlgItemText(hDlg, IDC_DEFAULTOPTIONS_ROMDIRECTORY, path);
			}
			break;

		case IDOK:
			{
				/* Read option setting from dialog */
				emuoptions.auto_full_screen = (SendDlgItemMessage(hDlg, IDC_OPTION_AUTOFULLSCREEN, BM_GETCHECK, 0, 0) == BST_CHECKED);
				emuoptions.auto_apply_cheat_code = (SendDlgItemMessage(hDlg, IDC_DEFAULTOPTIONS_AUTOCHEAT, BM_GETCHECK, 0, 0) == BST_CHECKED);
				guioptions.pause_at_menu = (SendDlgItemMessage(hDlg, IDC_DEFAULTOPTIOS_PAUSEONMENU, BM_GETCHECK, 0, 0) == BST_CHECKED);
				guioptions.pause_at_inactive = (SendDlgItemMessage(hDlg, IDC_DEFAULTOPTIONS_PAUSEWHENINACTIVE, BM_GETCHECK, 0, 0) == BST_CHECKED);
				guioptions.borderless_fullscreen = (SendDlgItemMessage(hDlg, IDC_OPTION_BORDERLESSFULLSCREEN, BM_GETCHECK, 0, 0) == BST_CHECKED);
				guioptions.auto_hide_cursor_when_active = (SendDlgItemMessage(hDlg, IDC_AUTOHIDEMOUSEWHENACTIVE, BM_GETCHECK, 0, 0) == BST_CHECKED);

				if
				(
					guioptions.show_expert_user_menu !=
						(SendDlgItemMessage(hDlg, IDC_EXPERT_MODE, BM_GETCHECK, 0, 0) == BST_CHECKED)
				)
				{
					guioptions.show_expert_user_menu = 1 - guioptions.show_expert_user_menu;
					if(guioptions.show_expert_user_menu)
						RegenerateAdvancedUserMenus();
					else
					{
						DeleteAdvancedUserMenus();
					}
				}

				if
				(
					guioptions.show_recent_rom_directory_list !=
						(SendDlgItemMessage(hDlg, IDC_ENABLE_DIRECTORY_LIST, BM_GETCHECK, 0, 0) == BST_CHECKED)
				)
				{
					guioptions.show_recent_rom_directory_list = 1 - guioptions.show_recent_rom_directory_list;
					if(guioptions.show_recent_rom_directory_list)
						RegerateRecentRomDirectoryMenus();
					else
					{
						DeleteRecentRomDirectoryMenus();
					}
				}

				if
				(
					guioptions.show_recent_game_list !=
						(SendDlgItemMessage(hDlg, IDC_ENABLE_GAME_LIST, BM_GETCHECK, 0, 0) == BST_CHECKED)
				)
				{
					guioptions.show_recent_game_list = 1 - guioptions.show_recent_game_list;
					if(guioptions.show_recent_game_list)
						RegerateRecentGameMenus();
					else
					{
						DeleteRecentGameMenus();
					}
				}

				if
				(
					guioptions.show_state_selector_menu !=
						(SendDlgItemMessage(hDlg, IDC_ENABLE_STATE_MENU, BM_GETCHECK, 0, 0) == BST_CHECKED)
				)
				{
					guioptions.show_state_selector_menu = 1 - guioptions.show_state_selector_menu;
					if(guioptions.show_state_selector_menu)
						RegenerateStateSelectorMenus();
					else
					{
						DeleteStateSelectorMenus();
					}
				}

				if
				(
					guioptions.show_critical_msg_window !=
						(SendDlgItemMessage(hDlg, IDC_OPTION_ERROR_WINDOW, BM_GETCHECK, 0, 0) == BST_CHECKED)
				)
				{
					guioptions.show_critical_msg_window = 1 - guioptions.show_critical_msg_window;
					if(guioptions.show_critical_msg_window)
					{
						if(gui.hCriticalMsgWnd == NULL)
						{
							gui.hCriticalMsgWnd = CreateDialog
								(
									gui.hInst,
									"CRITICAL_MESSAGE",
									NULL,
									(DLGPROC) CriticalMessageDialog
								);
							SetActiveWindow(gui.hwnd1964main);
						}
					}
					else
					{
						if(gui.hCriticalMsgWnd != NULL)
						{
							DestroyWindow(gui.hCriticalMsgWnd);
							gui.hCriticalMsgWnd = NULL;
						}
					}
				}

				if(	guioptions.display_romlist !=(SendDlgItemMessage(hDlg, IDC_OPTION_ROMBROWSER, BM_GETCHECK, 0, 0) == BST_CHECKED))
				{
					guioptions.display_romlist = 1 - guioptions.display_romlist;
					EndDialog(hDlg, TRUE);
					SendMessage(gui.hwnd1964main, WM_COMMAND, ID_FILE_FRESHROMLIST, 0);
					return(TRUE);
				}

				guioptions.display_detail_status = (SendDlgItemMessage(hDlg, IDC_ENABLE_DETAIL_STATUS, BM_GETCHECK, 0, 0) == BST_CHECKED);
				guioptions.display_profiler_status = (SendDlgItemMessage(hDlg, IDC_ENABLE_PROFILER, BM_GETCHECK, 0, 0) == BST_CHECKED);
				if(SendDlgItemMessage(hDlg, IDC_DISPLAY_STATUSBAR, BM_GETCHECK, 0, 0) == BST_CHECKED)
				{
					ShowWindow(gui.hToolBar, SW_SHOW);
					ShowWindow(gui.hStatusBar, SW_SHOW);
				}
				else
					ShowWindow(gui.hStatusBar, SW_HIDE);
				if(guioptions.highfreqtimer != SendDlgItemMessage(hDlg, IDC_HIGHFREQTIMER, BM_GETCHECK, 0, 0))
					MessageBox(gui.hwnd1964main, "Please restart 1964 to apply high frequency settings.", "User Options", MB_OK);
				guioptions.highfreqtimer = (SendDlgItemMessage(hDlg, IDC_HIGHFREQTIMER, BM_GETCHECK, 0, 0) == BST_CHECKED);
				guioptions.display_statusbar = (SendDlgItemMessage(hDlg, IDC_DISPLAY_STATUSBAR, BM_GETCHECK, 0, 0) == BST_CHECKED);
				if(guioptions.display_statusbar == TRUE && guioptions.borderless_fullscreen == TRUE)
				{
					MessageBox(gui.hwnd1964main, "Borderless fullscreen will not work with status bar.\n\nTurning off status bar...", "User Options", MB_OK);
					ShowWindow(gui.hStatusBar, SW_HIDE);
					guioptions.display_statusbar = FALSE;
				}
				guioptions.use_default_save_directory = (SendDlgItemMessage(hDlg, IDC_OPTIONS_USE_DEFAULT_SAVE_DIRECTORY, BM_GETCHECK, 0, 0) == BST_CHECKED);
				guioptions.use_default_plugin_directory =
					(
						SendDlgItemMessage
						(
							hDlg,
							IDC_DEFAULTOPTIONS_USE1964PLUGINDIRECTORY,
							BM_GETCHECK,
							0,
							0
						) == BST_CHECKED
					);
				guioptions.use_last_rom_directory = (SendDlgItemMessage(hDlg, IDC_DEFAULTOPTIONS_USELASTROMDIRECTORY, BM_GETCHECK, 0, 0) == BST_CHECKED);

				GetDlgItemText(hDlg, IDC_DEFAULTOPTIONS_ROMDIRECTORY, user_set_rom_directory, _MAX_PATH);
				GetDlgItemText(hDlg, IDC_DEFAULTOPTIONS_SAVEDIRECTORY, user_set_save_directory, _MAX_PATH);
				GetDlgItemText(hDlg, IDC_DEFAULTOPTIONS_STATESAVEDIRECTORY, state_save_directory, _MAX_PATH);
				GetDlgItemText(hDlg, IDC_DEFAULTOPTIONS_PLUGINDIRECTORY, user_set_plugin_directory, _MAX_PATH);

				/* Set the save directory to use */
				if(guioptions.use_default_save_directory)
					strcpy(directories.save_directory_to_use, default_save_directory);
				else
					strcpy(directories.save_directory_to_use, user_set_save_directory);

				/* Set the ROM directory to use */
				if(guioptions.use_last_rom_directory)
					strcpy(directories.rom_directory_to_use, directories.last_rom_directory);
				else
					strcpy(directories.rom_directory_to_use, user_set_rom_directory);

				/* Set the plugin directory to use */
				if(guioptions.use_default_plugin_directory)
					strcpy(directories.plugin_directory_to_use, default_plugin_directory);
				else
					strcpy(directories.plugin_directory_to_use, user_set_plugin_directory);

				EndDialog(hDlg, TRUE);
				return(TRUE);
			}

		case IDCANCEL:
			{
				EndDialog(hDlg, TRUE);
				return(TRUE);
			}
		}
	}

	return(FALSE);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
LRESULT APIENTRY SetVideoPluginDialog(HWND hDlg, unsigned message, WORD wParam, LONG lParam)
{
	switch(message)
	{
	case WM_COMMAND:
		switch(wParam)
		{
		case IDC_JABO:
			strcpy(gRegSettings.VideoPlugin, "Jabo_Direct3D8.dll");
			EndDialog(hDlg, TRUE);
			return(TRUE);
		case IDC_GLIDEN64:
			strcpy(gRegSettings.VideoPlugin, "GLN64_2020.dll");
		default:
			EndDialog(hDlg, TRUE);
			return(TRUE);
		}
	}

	return(FALSE);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void SetOverclockFactor(int factor)
{
	int ocmenuid = ID_OVERCLOCK6;
	int cfmenuid = ID_CF_CF1;
	CheckMenuItem(gui.hMenu1964main, ID_OVERCLOCKSTOCK, factor == 1 ? MF_CHECKED : MF_UNCHECKED);
	while(ocmenuid <= ID_OVERCLOCK18)
	{
		if((LOWORD(ocmenuid) - ID_OVERCLOCK6 + 2) * 3 != factor)
			CheckMenuItem(gui.hMenu1964main, ocmenuid, MF_UNCHECKED);
		else
			CheckMenuItem(gui.hMenu1964main, ocmenuid, MF_CHECKED);
		ocmenuid++;
	}
	while(cfmenuid <= ID_CF_CF8)
	{
		if(factor != 1)
			EnableMenuItem(gui.hMenu1964main, cfmenuid, MF_GRAYED);
		else
			EnableMenuItem(gui.hMenu1964main, cfmenuid, MF_ENABLED);
		cfmenuid++;
	}
	if(!emustatus.gepd_pause)
		GEPDPause(TRUE);
	else
		emustatus.gepd_pause = 2;
	emuoptions.OverclockFactor = factor;
	EnableMenuItem(gui.hMenu1964main, ID_GEFIRINGHACK, factor != 1 ? MF_ENABLED : MF_GRAYED);
	EnableMenuItem(gui.hMenu1964main, ID_PDSPEEDHACK, factor != 1 ? MF_ENABLED : MF_GRAYED);
	CheckMenuItem(gui.hMenu1964main, ID_GEFIRINGHACK, emuoptions.GEFiringRateHack && factor != 1 ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(gui.hMenu1964main, ID_GEDISABLEHEADROLL, emuoptions.GEDisableHeadRoll ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(gui.hMenu1964main, ID_PDSPEEDHACK, emuoptions.PDSpeedHack && factor != 1 ? MF_CHECKED : MF_UNCHECKED);
	if(mouseinjectorpresent)
		CONTROLLER_HookRDRAM((DWORD *)TLB_sDWord_ptr, factor);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void SetCounterFactor(int factor)
{
	if(emuoptions.OverclockFactor != 1 || factor < COUTERFACTOR_1 || factor > COUTERFACTOR_8)
		factor = COUTERFACTOR_1;
	if(CounterFactor != factor)
	{
		CheckMenuItem(gui.hMenu1964main, cfmenulist[CounterFactor - 1], MF_UNCHECKED);
		CounterFactor = factor;
		if(emustatus.Emu_Is_Running)
		{
			Init_Count_Down_Counters();
			if(PauseEmulator())
				ResumeEmulator(REFRESH_DYNA_AFTER_PAUSE);	/* Need to init emu */
		}

		CheckMenuItem(gui.hMenu1964main, cfmenulist[CounterFactor - 1], MF_CHECKED);
		sprintf(generalmessage, "CF=%d", factor);
		SetStatusBarText(2, generalmessage);
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */

#define PD_frameratecal 0x80014388 // location of function that returns when to draw at 60fps (thank you Ryan Dwyer for the code and single-handedly decompiling PD - you absolute legend)
#define PD_masterclock 0x8038CECC // location of master clock code (TLB'd to 7F)
#define PD_updateaimtarget 0x8025A7C8 // location of AI function to update aim target (TLB'd to 7F)
#define PD_newcodearea 0x803C78E0 // free area to write new timing code for 60fps mode
#define PD_newcodearealastcode 0x803C7984 // used to check if memory is safe to write

static const unsigned int pdcodearray[42] = {0x3C028006, 0x8C42EE10, 0x240E0007, 0x51C2000E, 0x3C02800A, 0x3C02800B, 0x8042CB97, 0x304E0080, 0x15C00017, 0x304E0040, 0x11C00012, 0x3C02800A, 0x8C42A424, 0x14400012, 0x00000000, 0x10000010, 0x00129040, 0x00000000, 0x804221D3, 0x30420040, 0x10400007, 0x00000000, 0x3C02800A, 0x8C42A424, 0x14400007, 0x00000000, 0x10000005, 0x00129040, 0x3C02800A, 0x8C42A424, 0x54400001, 0x00129040, 0x0BC5B3B5, 0x3631EBC2, 0x8DCE0020, 0x85CF0014, 0x85D80016, 0x15F80002, 0x27180001, 0xA5D80016, 0x0BC0C495, 0xAC860050}; // hijack timing code to allow combat boost at 60fps and fix camping guards at 60fps
static const unsigned int gecodearray[20] = {0x27BDFFE8, 0x808E0007, 0x24010008, 0x00001025, 0x15C1000D, 0x8C8F004C, 0x31F80060, 0x1300000A, 0x8C8E001C, 0x85CF0030, 0x85D80032, 0x15F80002, 0x27180001, 0xA5D80032, 0xAC85004C, 0x0FC093E3, 0xAC860050, 0x34020001, 0x0BC0D71E, 0x27BD0018}; // fix camping guards at 60fps
static const unsigned int geheadrolloriginal[6] = {0xE450052C, 0xE4480530, 0xE4460534, 0xE4440538, 0xE452053C, 0xE4500540};

typedef struct GE_HACK_RESOLUTION
{
	unsigned int readfiringrate;
	unsigned int updateaimtarget;
	unsigned int updateaimtargetjal;
	unsigned int updateaimtargetreturn;
	unsigned int dronegunfiringrate;
	unsigned int headrollnop[6];
	unsigned int pause;
	BOOL gamevalid;
	BOOL firingvalid;
	BOOL guardvalid;
	BOOL rommappingvalid;
	BOOL dronevalid;
	BOOL headrollvalid;
} GE_HACK_RESOLUTION;

static const unsigned int gegamesegmentpattern[5] = {0x3C013F80, 0x44810000, 0x2402FFFF, 0x3C010000, 0xAC220000};
static const unsigned int gegamesegmentmask[5] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0000, 0xFFFF0000};
static const unsigned int geupdateaimtargetpattern[20] = {0x27BDFFE8, 0xAFBF0014, 0x808E0007, 0x24010008, 0x00001025, 0x15C1000A, 0x00000000, 0x8C8F004C, 0x31F80060, 0x13000006, 0x00000000, 0xAC85004C, 0x0C000000, 0xAC860050, 0x10000001, 0x24020001, 0x8FBF0014, 0x27BD0018, 0x03E00008, 0x00000000};
static const unsigned int geupdateaimtargetmask[20] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFC000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
static const unsigned int gefiringratepattern[9] = {0x8FBF0014, 0x80420022, 0x27BD0018, 0x03E00008, 0x00000000, 0x27BDFFE8, 0xAFBF0014, 0x0C000000, 0x00000000};
static const unsigned int gefiringratemask[9] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFC000000, 0xFFFFFFFF};
static const unsigned int gedronepattern[5] = {0x250B0002, 0xAE0B00C0, 0x8FAF013C, 0x8FA90138, 0x24190001};
static const unsigned int geexactmask5[5] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
static const unsigned int gecontinuationpattern[8] = {0x10400007, 0x02C02025, 0x02402825, 0x0C000000, 0x92260005, 0x00409025, 0x1000FE3E, 0x02C28821};
static const unsigned int gecontinuationmask[8] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFC000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};

static unsigned int GEReadROMWord(unsigned int offset)
{
	return *((unsigned int *)&gMemoryState.ROM_Image[offset]);
}

static void GEWriteROMWord(unsigned int offset, unsigned int value)
{
	gMemoryState.ROM_Image[offset] = value & 0xFF;
	gMemoryState.ROM_Image[offset + 1] = (value >> 8) & 0xFF;
	gMemoryState.ROM_Image[offset + 2] = (value >> 16) & 0xFF;
	gMemoryState.ROM_Image[offset + 3] = (value >> 24) & 0xFF;
}

static BOOL GEPatternMatches(unsigned int offset, const unsigned int *pattern, const unsigned int *mask, unsigned int wordcount)
{
	unsigned int index;
	unsigned int bytecount = wordcount * 4;

	if(offset > gAllocationLength || bytecount > gAllocationLength - offset)
		return FALSE;

	for(index = 0; index < wordcount; index++)
	{
		if((GEReadROMWord(offset + index * 4) & mask[index]) != (pattern[index] & mask[index]))
			return FALSE;
	}

	return TRUE;
}

static unsigned int GEFindUniqueROMPattern(const unsigned int *pattern, const unsigned int *mask, unsigned int wordcount)
{
	unsigned int offset;
	unsigned int match = 0;
	unsigned int bytecount = wordcount * 4;

	if(gAllocationLength < bytecount)
		return 0;

	for(offset = 0x1000; offset <= gAllocationLength - bytecount; offset += 4)
	{
		if(GEPatternMatches(offset, pattern, mask, wordcount))
		{
			if(match != 0)
				return 0;
			match = offset;
		}
	}

	return match;
}

static BOOL GEFindHeadRoll(unsigned int *offsets)
{
	unsigned int offset;
	unsigned int index;
	unsigned int match = 0;
	BOOL valid;

	if(gAllocationLength < 5 * 0x1C + 4)
		return FALSE;

	for(offset = 0x1000; offset <= gAllocationLength - (5 * 0x1C + 4); offset += 4)
	{
		if(GEReadROMWord(offset) != geheadrolloriginal[0])
			continue;

		valid = TRUE;
		for(index = 1; index < 6; index++)
		{
			if(GEReadROMWord(offset + index * 0x1C) != geheadrolloriginal[index])
			{
				valid = FALSE;
				break;
			}
		}

		if(valid)
		{
			if(match != 0)
				return FALSE;
			match = offset;
		}
	}

	if(match == 0)
		return FALSE;

	for(index = 0; index < 6; index++)
		offsets[index] = match + index * 0x1C;
	return TRUE;
}

/* Only restore bytes written by this session, never a mod's prepatched code.
 * Multiword guard/head-roll changes are restored as complete groups. */
typedef struct GE_ROM_PATCH_GROUP
{
	unsigned int count;
	unsigned int offsets[71];
	unsigned int original[71];
	unsigned int patched[71];
} GE_ROM_PATCH_GROUP;
static GE_ROM_PATCH_GROUP geROMPatchGroups[6];
static unsigned char *geROMPatchOwner = NULL;
static unsigned int geROMPatchLength = 0;

static void GERecordROMPatch(unsigned int group, unsigned int index,
	unsigned int offset, unsigned int patched)
{
	GE_ROM_PATCH_GROUP *record = &geROMPatchGroups[group];
	geROMPatchOwner = gMemoryState.ROM_Image;
	geROMPatchLength = gAllocationLength;
	record->offsets[index] = offset;
	record->original[index] = GEReadROMWord(offset);
	record->patched[index] = patched;
	record->count = index + 1;
}

void GEPDRestoreROMHacks(void)
{
	unsigned int group, index;
	GE_ROM_PATCH_GROUP *record;
	if(geROMPatchOwner != NULL && geROMPatchOwner == gMemoryState.ROM_Image &&
		geROMPatchLength == gAllocationLength)
	{
		for(group = 0; group < 6; group++)
		{
			record = &geROMPatchGroups[group];
			for(index = 0; index < record->count; index++)
			{
				if(record->offsets[index] > gAllocationLength ||
					gAllocationLength - record->offsets[index] < 4 ||
					GEReadROMWord(record->offsets[index]) != record->patched[index])
					break;
			}
			if(index != record->count)
				continue;
			for(index = 0; index < record->count; index++)
				GEWriteROMWord(record->offsets[index], record->original[index]);
		}
	}
	memset(geROMPatchGroups, 0, sizeof(geROMPatchGroups));
	geROMPatchOwner = NULL;
	geROMPatchLength = 0;
}

static GE_HACK_RESOLUTION geResolution;
static BOOL geResolutionInitialized = FALSE;
static unsigned int geRAMFiringSite = 0;
static unsigned int geRAMFiringContext[9];
static unsigned int pdSpeedSite = 0;
static unsigned int pdHeadRollSite = 0;
static unsigned int pdHeadRollContext[9];
static BOOL alreadypaused = FALSE;
static unsigned int gepdPauseAddress = 0;
static volatile LONG gepdPatchesPending = 0;
static BOOL gepdGameEntryReached = FALSE;
static unsigned int pdSpeedContext[27];
static unsigned int geRAMHeadRollSite = 0;
static unsigned int geRAMHeadRollContext[12];

static void GETextureResetResolution(void);
static void GEEditorResetResolution(void);
static BOOL GETexturePlusTitle(void);
static BOOL GETextureThreadsSafe(void);

/* A new boot must never inherit addresses from another image with the same CRC. */
static void GEPDResetHackResolution(void)
{
	GEPDRestoreROMHacks();
	GETextureResetResolution();
	GEEditorResetResolution();
	memset(&geResolution, 0, sizeof(geResolution));
	geResolutionInitialized = FALSE;
	geRAMFiringSite = 0;
	geRAMHeadRollSite = 0;
	pdSpeedSite = 0;
	pdHeadRollSite = 0;
	alreadypaused = FALSE;
	gepdPauseAddress = 0;
	gepdGameEntryReached = FALSE;
	InterlockedExchange(&gepdPatchesPending, 1);
}

static const unsigned int gepausepattern[12] = {0x3C013F80, 0x44816000, 0x24020001, 0x3C010000, 0x27BDFFC8, 0xAC220000, 0xAFB10024, 0x3C010000, 0x3C110000, 0xAC200000, 0x26310000, 0xAE220000};
static const unsigned int gepausealternate[12] = {0x3C013F80, 0x44816000, 0x24020001, 0x3C010000, 0x27BDFFC8, 0xAC220000, 0xAFB00020, 0x3C010000, 0x3C100000, 0xAC200000, 0x26100000, 0xAE020000};
static const unsigned int gepausemask[12] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0000, 0xFFFFFFFF, 0xFFFF0000, 0xFFFFFFFF, 0xFFFF0000, 0xFFFF0000, 0xFFFF0000, 0xFFFF0000, 0xFFFFFFFF};

static unsigned int GEPDOperandAddress(unsigned int upper, unsigned int lower)
{
	unsigned int address = (upper << 16) + (int)(short)(lower & 0xFFFF);
	if((address & 3) != 0 || address < 0x80000000 || address > 0x807FFFFC)
		return 0;
	return address;
}

static unsigned int GEFindPause(void)
{
	unsigned int offset, match = 0;
	if(gAllocationLength < 48)
		return 0;
	for(offset = 0x1000; offset <= gAllocationLength - 48; offset += 4)
	{
		if(GEPatternMatches(offset, gepausepattern, gepausemask, 12) ||
			GEPatternMatches(offset, gepausealternate, gepausemask, 12))
		{
			if(match != 0)
				return 0;
			match = offset;
		}
	}
	return match == 0 ? 0 : GEPDOperandAddress(GEReadROMWord(match + 28), GEReadROMWord(match + 36));
}

/* The native pager copies an 8 KiB page from gameSegment + (vaddr &
 * 0x00FFE000). Authenticate that address calculation independently of the
 * optional camping-guard fix: rewritten AI does not change the code map.
 * Physical-code mods such as GoldenEye Plus omit this paging operation. */
static BOOL GEHasROMCodePager(unsigned int gameSegment)
{
	static const unsigned int pattern[13] = {
		0x3C0100FF, 0x3421E000, 0x00104340, 0x3C0A0000,
		0x00414824, 0x254A0000, 0x03282021, 0xAFA40034,
		0x012A2821, 0x01201025, 0xAFA90024, 0x0C000000,
		0x24062000
	};
	static const unsigned int mask[13] = {
		0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0000,
		0xFFFFFFFF, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF,
		0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFC000000,
		0xFFFFFFFF
	};
	unsigned int offset, match = 0, source;
	if(gameSegment < 0x1000 + sizeof(pattern) || gameSegment > gAllocationLength)
		return FALSE;
	/* Resident pager code precedes the paged game segment. Do not identify
	 * instruction-like data in later maps, models or duplicated resources. */
	for(offset = 0x1000; offset <= gameSegment - sizeof(pattern); offset += 4)
	{
		if(GEPatternMatches(offset, pattern, mask, 13))
		{
			if(match != 0)
				return FALSE;
			match = offset;
		}
	}
	if(match == 0)
		return FALSE;
	source = ((GEReadROMWord(match + 12) & 0xFFFF) << 16) +
		(int)(short)(GEReadROMWord(match + 20) & 0xFFFF);
	return source == gameSegment;
}

static const GE_HACK_RESOLUTION *GEGetHackResolution(void)
{
	GE_HACK_RESOLUTION *result = &geResolution;
#define resolution (*result)
	unsigned int gameSegment;
	unsigned int updateAimTarget;
	unsigned int firingRateContext;
	unsigned int droneFiringRate;
	unsigned int continuation;
	unsigned int continuationDelta;
	unsigned int continuationAddress;

	if(geResolutionInitialized)
		return &resolution;

	memset(&resolution, 0, sizeof(resolution));
	geResolutionInitialized = TRUE;

	gameSegment = GEFindUniqueROMPattern(gegamesegmentpattern, gegamesegmentmask, 5);
	updateAimTarget = GEFindUniqueROMPattern(geupdateaimtargetpattern, geupdateaimtargetmask, 20);
	firingRateContext = GEFindUniqueROMPattern(gefiringratepattern, gefiringratemask, 9);
	droneFiringRate = GEFindUniqueROMPattern(gedronepattern, geexactmask5, 5);
	continuation = GEFindUniqueROMPattern(gecontinuationpattern, gecontinuationmask, 8);

	/* Independent fixes: a mod's rewritten drone or AI code must not disable
	 * the separately identified player weapon delay. */
	if(firingRateContext != 0)
	{
		resolution.readfiringrate = firingRateContext + 0x10;
		resolution.firingvalid = TRUE;
	}
	if(droneFiringRate != 0)
	{
		resolution.dronegunfiringrate = droneFiringRate;
		resolution.dronevalid = TRUE;
	}

	if(gameSegment != 0 && updateAimTarget >= gameSegment && continuation >= gameSegment + 8)
	{
		continuationDelta = continuation - gameSegment;
		continuationAddress = 0x7F000000 + continuationDelta;
		resolution.updateaimtargetjal = GEReadROMWord(updateAimTarget + 0x30);
		/* Bind the continuation to a call of this exact mapped-ROM function.
		 * RAM-loaded or rewritten AI must retain its own control flow. */
		if((resolution.updateaimtargetjal & 0xFC000000) == 0x0C000000 &&
			continuationDelta < 0x01000000 &&
			GEReadROMWord(continuation - 8) == (0x0C000000 |
			(((0x7F000000 + updateAimTarget - gameSegment) >> 2) & 0x03FFFFFF)))
		{
			resolution.updateaimtarget = updateAimTarget;
			resolution.updateaimtargetreturn = 0x08000000 | ((continuationAddress >> 2) & 0x03FFFFFF);
			resolution.guardvalid = TRUE;
		}
	}

	resolution.rommappingvalid = resolution.guardvalid || GEHasROMCodePager(gameSegment);
	resolution.pause = GEFindPause();
	resolution.headrollvalid = GEFindHeadRoll(resolution.headrollnop);
	resolution.gamevalid = gameSegment != 0 && resolution.firingvalid && resolution.headrollvalid;
	return &resolution;
#undef resolution
}

/* Mapping classification is independent of optional AI patch admission.
 * Either verified mapped callers or the native ROM pager prove the map.
 * RAM-loaded mods continue to use their real runtime TLB. */
BOOL GEUsesROMCodeMapping(void)
{
	return GEGetHackResolution()->rommappingvalid;
}

/* The emulator stores each emulated word in host byte order.  Scan only
 * allocated RDRAM; return zero for missing or ambiguous code. */
static unsigned int GEPDFindRAMPattern(const unsigned int *pattern,
	const unsigned int *mask, unsigned int count)
{
	unsigned int offset, index, match = 0;
	unsigned int *words = (unsigned int *)gMS_RDRAM;
	if(current_rdram_size < count * 4)
		return 0;
	for(offset = 0x1000; offset <= current_rdram_size - count * 4; offset += 4)
	{
		for(index = 0; index < count; index++)
			if((words[offset / 4 + index] & mask[index]) != (pattern[index] & mask[index]))
				break;
		if(index == count)
		{
			if(match != 0)
				return 0;
			match = offset;
		}
	}
	return match == 0 ? 0 : 0x80000000 + match;
}

static void GEPDWriteRAMCode(unsigned int address, unsigned int value)
{
	unsigned int page;
	unsigned char *backing;
	/* Code copied into RAM may already have been compiled before the timer
	 * runs. Invalidate both its physical and mapped virtual code aliases. */
	Check_And_Invalidate_Compiled_Blocks_By_DMA(address, 4, "GEPD patch");
	InvalidateOneBlock(address);
	InvalidateOneBlock(address | 0x20000000);
	backing = (unsigned char *)gMS_RDRAM + ((address & 0x007FFFFF) & ~0xFFF);
	for(page = 0x70000; page < 0x80000; page++)
		if((unsigned char *)TLB_sDWORD_R[page] == backing)
			InvalidateOneBlock(page << 12);
	LOAD_UWORD_PARAM(address) = value;
}

/* Native Test return: select Plus by its header and resolve the title and
 * completed-test helpers from their code and call relationships. Keep the
 * existing initializer correction while relocating its operands. */
#define GE_EDITOR_TITLE_WORDS 71U
static const unsigned int geeditororiginal[71] = {
	0x2402FFFF, 0x3C018003, 0xAC22A8F0, 0x3C018003, 0xAC22A8F8, 0x3C018003,
	0xAC20A970, 0x24030001, 0x3C018003, 0xAC23A974, 0x3C018003, 0xAC20A978,
	0x3C018003, 0xAC20A948, 0x3C018003, 0xAC22A94C, 0x3C018003, 0xAC23A950,
	0x3C018003, 0xAC20A90C, 0x3C018003, 0xAC20A910, 0x3C0E8003, 0x8DCEA964,
	0x3C018003, 0xAC20A914, 0x27BDFFE8, 0x3C018003, 0xAFBF0014, 0x11C00005,
	0xAC23A96C, 0x3C028003, 0x2442A8F4, 0x240F0005, 0xAC4F0000, 0x3C028003,
	0x2442A8F4, 0x8C580000, 0x3C040007, 0x24190005, 0x07010002, 0x34848000,
	0xAC590000, 0x0C002570, 0x24050004, 0x3C018003, 0x3C040004, 0xAC22A980,
	0x3484B040, 0x0C002570, 0x24050004, 0x3C038003, 0x2449003F, 0x2401FFC0,
	0x2463A984, 0x01215024, 0xAC620000, 0xAC6A0000, 0x3C018003, 0xAC20A98C,
	0x3C018003, 0xAC20A990, 0x3C018003, 0xAC20A994, 0x3C018003, 0x0C18031C,
	0xAC20A998, 0x8FBF0014, 0x27BD0018, 0x03E00008, 0x00000000
};
static const unsigned int geeditorpatched[71] = {
	0x2402FFFF, 0x3C018003, 0xAC22A8F0, 0xAC22A8F8, 0xAC20A970, 0x24030001,
	0xAC23A974, 0xAC20A978, 0xAC20A948, 0xAC22A94C, 0xAC23A950, 0xAC20A90C,
	0xAC20A910, 0x3C0E8003, 0x8DCEA964, 0xAC20A914, 0x27BDFFE8, 0xAFBF0014,
	0x11C00005, 0xAC23A96C, 0x3C028003, 0x2442A8F4, 0x240F0005, 0xAC4F0000,
	0x3C028003, 0x2442A8F4, 0x8C580000, 0x07010002, 0x24190005, 0xAC590000,
	0x0C18BDE2, 0x00000000, 0x10400006, 0x00000000, 0x0C18BDED, 0x00000000,
	0x3C018003, 0x240E001E, 0xAC2EA8F4, 0x3C040007, 0x34848000, 0x0C002570,
	0x24050004, 0x3C018003, 0x3C040004, 0xAC22A980, 0x3484B040, 0x0C002570,
	0x24050004, 0x3C038003, 0x2449003F, 0x2401FFC0, 0x2463A984, 0x01215024,
	0xAC620000, 0xAC6A0000, 0x3C018003, 0xAC20A98C, 0xAC20A990, 0xAC20A994,
	0x0C18031C, 0xAC20A998, 0x8FBF0014, 0x27BD0018, 0x03E00008, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000
};
static const unsigned int geeditorhelpers[87] = {
	0x3C028007, 0x03E00008, 0x8C42E040, 0x3C028007, 0x8C42E040, 0x0002702B,
	0x11C0000F, 0x01C01025, 0x3C028007, 0x8C42E044, 0x0002782B, 0x11E0000A,
	0x01E01025, 0x3C028007, 0x8C42E04C, 0x0002C02B, 0x13000005, 0x03001025,
	0x3C028007, 0x8C42E050, 0x0002C82B, 0x03201025, 0x03E00008, 0x00000000,
	0x3C0E8007, 0x8DCEE040, 0x240F0001, 0x3C018007, 0x11C00002, 0x00000000,
	0xAC2FE044, 0x03E00008, 0x00000000, 0x3C0E8007, 0x8DCEE040, 0x240F0001,
	0x3C018007, 0x11C00008, 0x00000000, 0xAC2FE048, 0x3C018007, 0xAC20E044,
	0x3C018007, 0xAC20E04C, 0x3C018007, 0xAC20E050, 0x03E00008, 0x00000000,
	0x3C028007, 0x8C42E040, 0x0002702B, 0x11C00005, 0x01C01025, 0x3C028007,
	0x8C42E048, 0x0002782B, 0x01E01025, 0x03E00008, 0x00000000, 0x3C018007,
	0xAC20E040, 0x3C018007, 0xAC20E044, 0x3C018007, 0xAC20E048, 0x3C018007,
	0xAC20E04C, 0x44800000, 0x3C018007, 0xAC20E050, 0x3C018007, 0xE420E078,
	0x3C018007, 0xE420E07C, 0x3C018007, 0xE420E080, 0x3C018007, 0xAC20E058,
	0x3C018007, 0xAC20E05C, 0x3C018007, 0xAC20E060, 0x3C018007, 0xAC20E064,
	0x3C018007, 0x03E00008, 0xAC20E054
};
static const unsigned int geeditortitlecaller[8] = {
	0x8FAF0038, 0x2401005A, 0x15E10005, 0x00000000, 0x0C180348, 0x00000000,
	0x10000079, 0x00000000
};
static const unsigned int geeditoreditorinit[20] = {
	0x27BDFFE8, 0xAFBF0014, 0x0C182F9C, 0x00000000, 0x3C048003, 0x8C84A980,
	0x3C010003, 0x3421119F, 0x00812021, 0x348E003F, 0x39C4003F, 0x240501B8,
	0x0C1BE2AE, 0x2406014A, 0x0C18B603, 0x00000000, 0x8FBF0014, 0x27BD0018,
	0x03E00008, 0x00000000
};
static const unsigned int geeditormapheader[22] = {
	0x3C028040, 0x8C420000, 0x3C014D4D, 0x34215331, 0x00417026, 0x03E00008,
	0x2DC20001, 0x27BDFFE8, 0xAFBF0014, 0x3C048040, 0x3C060003, 0x34C6E888,
	0x24840000, 0x0C005EAC, 0x00002825, 0x8FBF0014, 0x3C0E4D4D, 0x35CE5331,
	0x3C018040, 0xAC2E0000, 0x03E00008, 0x27BD0018
};
static const unsigned int geeditorpreservemap[14] = {
	0x27BDFFD8, 0xAFBF0014, 0x3C018005, 0x0C19CB00, 0xC42C44B8, 0x3C0E8007,
	0x8DCEDFC8, 0xE7A00018, 0x11C00005, 0x00000000, 0x0C18B5ED, 0x00000000,
	0x1440003F, 0x00000000
};
static const unsigned int geeditornativereturn[11] = {
	0x0C18BDE2, 0x00000000, 0x1040000A, 0x3C088003, 0x0C18BDED, 0x00000000,
	0x0C001B47, 0x24040017, 0x2404001E, 0x0C1881BB, 0x24050001
};
static const unsigned int geeditordispatch[9] = {
	0x8C980000, 0x2F01001F, 0x1020009B, 0x0018C080, 0x3C018005, 0x00380821,
	0x8C382C20, 0x03000008, 0x00000000
};
static const unsigned int geeditoreditorhandler[5] = {
	0x0C1874C8, 0x00000000, 0x3C048003, 0x1000000F, 0x2484A8F0
};
typedef struct GE_EDITOR_CONTEXT
{
	unsigned int rom, ram, words;
	const unsigned int *expected;
} GE_EDITOR_CONTEXT;
static GE_EDITOR_CONTEXT geeditorcontexts[] = {
    {0U, 0U, 87U, geeditorhelpers},
    {0U, 0U, 8U, geeditortitlecaller},
    {0U, 0U, 11U, geeditornativereturn},
    {0U, 0U, 9U, geeditordispatch},
    {0U, 0U, 5U, geeditoreditorhandler},
};

static const unsigned int geeditororiginalmask[71] = {
    0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFFFFFFU,
    0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U,
    0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U,
    0xFFFF0000U, 0xFFFF0000U, 0xFFFFFFFFU, 0xFFFF0000U, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFF0000U, 0xFFFF0000U,
    0xFFFF0000U, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFF0000U, 0xFFFF0000U, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU,
    0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFC000000U, 0xFFFFFFFFU, 0xFFFF0000U, 0xFFFFFFFFU, 0xFFFF0000U,
    0xFFFFFFFFU, 0xFC000000U, 0xFFFFFFFFU, 0xFFFF0000U, 0xFFFFFFFFU, 0xFFFF0000U, 0xFFFF0000U, 0xFFFFFFFFU,
    0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U,
    0xFFFF0000U, 0xFC000000U, 0xFFFF0000U, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU,
};
static const unsigned int geeditorpatchedmask[71] = {
    0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFFFFFFU, 0xFFFF0000U, 0xFFFF0000U,
    0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U,
    0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFFFFFFU, 0xFFFFFFFFU,
    0xFFFF0000U, 0xFFFF0000U, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFC000000U, 0xFFFFFFFFU,
    0xFFFFFFFFU, 0xFFFFFFFFU, 0xFC000000U, 0xFFFFFFFFU, 0xFFFF0000U, 0xFFFFFFFFU, 0xFFFF0000U, 0xFFFFFFFFU,
    0xFFFFFFFFU, 0xFC000000U, 0xFFFFFFFFU, 0xFFFF0000U, 0xFFFFFFFFU, 0xFFFF0000U, 0xFFFFFFFFU, 0xFC000000U,
    0xFFFFFFFFU, 0xFFFF0000U, 0xFFFFFFFFU, 0xFFFF0000U, 0xFFFF0000U, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU,
    0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFC000000U, 0xFFFF0000U, 0xFFFFFFFFU, 0xFFFFFFFFU,
    0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU,
};
static const unsigned int geeditorhelpersmask[87] = {
    0xFFFF0000U, 0xFFFFFFFFU, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU,
    0xFFFF0000U, 0xFFFF0000U, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFF0000U, 0xFFFF0000U, 0xFFFFFFFFU,
    0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFF0000U, 0xFFFF0000U, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU,
    0xFFFF0000U, 0xFFFF0000U, 0xFFFFFFFFU, 0xFFFF0000U, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFF0000U, 0xFFFFFFFFU,
    0xFFFFFFFFU, 0xFFFF0000U, 0xFFFF0000U, 0xFFFFFFFFU, 0xFFFF0000U, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFF0000U,
    0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFFFFFFU, 0xFFFFFFFFU,
    0xFFFF0000U, 0xFFFF0000U, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFF0000U, 0xFFFF0000U, 0xFFFFFFFFU,
    0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U,
    0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFFFFFFU, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U,
    0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U,
    0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFF0000U, 0xFFFFFFFFU, 0xFFFF0000U,
};
static const unsigned int geeditortitlecallermask[8] = {
    0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFC000000U, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU,
};
static const unsigned int geeditornativereturnmask[11] = {
    0xFC000000U, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFF0000U, 0xFC000000U, 0xFFFFFFFFU, 0xFC000000U, 0xFFFFFFFFU,
    0xFFFFFFFFU, 0xFC000000U, 0xFFFFFFFFU,
};
static const unsigned int geeditordispatchmask[9] = {
    0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFF0000U, 0xFFFFFFFFU, 0xFFFF0000U, 0xFFFFFFFFU,
    0xFFFFFFFFU,
};
static const unsigned int geeditoreditorhandlermask[5] = {
    0xFC000000U, 0xFFFFFFFFU, 0xFFFF0000U, 0xFFFFFFFFU, 0xFFFF0000U,
};
static const int geEditorPatchSources[71] = {
    0, 1, 2, 4, 6, 7, 9, 11, 13, 15, 17, 19, 21, 22, 23, 25, 26, 28, 29, 30, 31, 32, 33, 34, 31, 32, 37, 40, 39, 42, -1, 70, -1, 70, -1, 70, 1, -1, -1, 38, 41, 43, 44, 1, 46, 47, 48, 43, 44, 51, 52, 53, 54, 55, 56, 57, 1, 59, 61, 63, 65, 66, 67, 68, 69, 70, 70, 70, 70, 70, 70
};
static const unsigned int geEditorOriginalSources[71] = {
    0U, 1U, 2U, 1U, 3U, 1U, 4U, 5U, 1U, 6U, 1U, 7U, 1U, 8U, 1U, 9U, 1U, 10U, 1U, 11U, 1U, 12U, 13U, 14U, 1U, 15U, 16U, 1U, 17U, 18U, 19U, 20U, 21U, 22U, 23U, 20U, 21U, 26U, 39U, 28U, 27U, 40U, 29U, 41U, 42U, 1U, 44U, 45U, 46U, 41U, 42U, 49U, 50U, 51U, 52U, 53U, 54U, 55U, 1U, 57U, 1U, 58U, 1U, 59U, 1U, 60U, 61U, 62U, 63U, 64U, 31U
};

static BOOL GEEditorWordsMatch(BOOL ram, unsigned int address,
	const unsigned int *expected, unsigned int words)
{
	unsigned int index, offset = address;
	unsigned int length = ram ? current_rdram_size : gAllocationLength;
	if(ram)
	{
		if(address < 0x80000000U || address >= 0x80800000U)
			return FALSE;
		offset = address - 0x80000000U;
	}
	if(offset > length || words > (length - offset) / 4)
		return FALSE;
	for(index = 0; index < words; index++)
		if((ram ? LOAD_UWORD_PARAM(address + index * 4) :
			GEReadROMWord(address + index * 4)) != expected[index])
			return FALSE;
	return TRUE;
}


/* The title identifies Plus; code signatures follow relocated implementations.
 * Cache exact cartridge operands so a save from another layout is not rewritten. */
static BOOL geEditorResolutionInitialized = FALSE;
static BOOL geEditorResolutionValid = FALSE;
static unsigned int geEditorTitleROM, geEditorTitleRAM, geEditorMenuEntry;
static unsigned int geEditorOriginalCode[GE_EDITOR_TITLE_WORDS];
static unsigned int geEditorPatchedCode[GE_EDITOR_TITLE_WORDS];
static unsigned int geEditorContextCode[5][87];

static void GEEditorResetResolution(void)
{
    unsigned int index;
    geEditorResolutionInitialized = FALSE;
    geEditorResolutionValid = FALSE;
    geEditorTitleROM = geEditorTitleRAM = geEditorMenuEntry = 0;
    for(index = 0; index < 5U; index++)
        geeditorcontexts[index].rom = geeditorcontexts[index].ram = 0;
}

static unsigned int GEEditorCallTarget(unsigned int word)
{
    return (word >> 26) == 3U ? 0x80000000U | ((word & 0x03FFFFFFU) << 2) : 0;
}

static BOOL GEEditorResolve(void)
{
    static const unsigned int *masks[5] = {
        geeditorhelpersmask, geeditortitlecallermask, geeditornativereturnmask,
        geeditordispatchmask, geeditoreditorhandlermask
    };
    unsigned int index, other, word, titleRAM, delta, target, menu, table;
    BOOL prepatched = FALSE;
    if(geEditorResolutionInitialized)
        return geEditorResolutionValid;
    geEditorResolutionInitialized = TRUE;
    if(!GETexturePlusTitle())
        return FALSE;
    geEditorTitleROM = GEFindUniqueROMPattern(geeditororiginal,
        geeditororiginalmask, GE_EDITOR_TITLE_WORDS);
    if(geEditorTitleROM == 0)
    {
        geEditorTitleROM = GEFindUniqueROMPattern(geeditorpatched,
            geeditorpatchedmask, GE_EDITOR_TITLE_WORDS);
        prepatched = TRUE;
    }
    if(geEditorTitleROM == 0)
        return FALSE;
    for(index = 0; index < 5U; index++)
    {
        GE_EDITOR_CONTEXT *context = &geeditorcontexts[index];
        context->rom = GEFindUniqueROMPattern(context->expected, masks[index], context->words);
        if(context->rom == 0)
            return FALSE;
    }
    titleRAM = GEEditorCallTarget(GEReadROMWord(geeditorcontexts[1].rom + 16U));
    if(titleRAM < 0x80000000U || titleRAM > 0x80800000U - GE_EDITOR_TITLE_WORDS * 4U ||
        titleRAM < geEditorTitleROM)
        return FALSE;
    geEditorTitleRAM = titleRAM;
    delta = titleRAM - geEditorTitleROM;
    for(index = 0; index < 5U; index++)
    {
        GE_EDITOR_CONTEXT *context = &geeditorcontexts[index];
        if(context->rom > 0xFFFFFFFFU - delta)
            return FALSE;
        context->ram = context->rom + delta;
        if(context->ram < 0x80000000U || context->ram >= 0x80800000U ||
            context->words > (0x80800000U - context->ram) / 4U)
            return FALSE;
        for(word = 0; word < context->words; word++)
            geEditorContextCode[index][word] = GEReadROMWord(context->rom + word * 4U);
    }
    /* Bind the native return to its completed-test getter and reset functions. */
    if(GEEditorCallTarget(geEditorContextCode[2][0]) != geeditorcontexts[0].ram + 0xC0U ||
        GEEditorCallTarget(geEditorContextCode[2][4]) != geeditorcontexts[0].ram + 0xECU)
        return FALSE;
    for(index = 0; index < GE_EDITOR_TITLE_WORDS; index++)
    {
        word = prepatched ? geEditorOriginalSources[index] : index;
        geEditorOriginalCode[index] = GEReadROMWord(geEditorTitleROM + word * 4U);
    }
    /* Reused address loads and calls must retain their original relationships. */
    for(index = 0; index < GE_EDITOR_TITLE_WORDS; index++)
        for(other = 0; other < index; other++)
            if(geeditororiginal[index] == geeditororiginal[other] &&
                geeditororiginalmask[index] != 0xFFFFFFFFU &&
                geEditorOriginalCode[index] != geEditorOriginalCode[other])
                return FALSE;
    if((geEditorOriginalCode[1] & 0xFFFFU) != (geEditorOriginalCode[31] & 0xFFFFU))
        return FALSE;
    menu = GEPDOperandAddress(geEditorOriginalCode[1], geEditorOriginalCode[2]);
    target = GEPDOperandAddress(geEditorContextCode[4][2], geEditorContextCode[4][4]);
    if(menu == 0 || menu != target)
        return FALSE;
    table = GEPDOperandAddress(geEditorContextCode[3][4], geEditorContextCode[3][6]);
    if(table == 0 || table > 0x807FFFFCU - 30U * 4U)
        return FALSE;
    geEditorMenuEntry = table + 30U * 4U;
    for(index = 0; index < GE_EDITOR_TITLE_WORDS; index++)
        geEditorPatchedCode[index] = geEditorPatchSources[index] >= 0 ?
            geEditorOriginalCode[geEditorPatchSources[index]] : geeditorpatched[index];
    geEditorPatchedCode[30] = geEditorContextCode[2][0];
    geEditorPatchedCode[34] = geEditorContextCode[2][4];
    geEditorPatchedCode[38] = 0xAC2E0000U | (geEditorOriginalCode[32] & 0xFFFFU);
    /* This also rejects mixed original/patched initializers. */
    if(!GEEditorWordsMatch(FALSE, geEditorTitleROM,
        prepatched ? geEditorPatchedCode : geEditorOriginalCode, GE_EDITOR_TITLE_WORDS))
        return FALSE;
    geEditorResolutionValid = TRUE;
    return TRUE;
}

static BOOL GEEditorContextMatches(BOOL ram)
{
    unsigned int index;
    for(index = 0; index < 5U; index++)
    {
        const GE_EDITOR_CONTEXT *context = &geeditorcontexts[index];
        if(!GEEditorWordsMatch(ram, ram ? context->ram : context->rom,
            geEditorContextCode[index], context->words))
            return FALSE;
    }
    return TRUE;
}

static BOOL GEEditorPCInTitle(unsigned int pc)
{
    if(pc >= 0xA0000000U && pc < 0xA0800000U)
        pc -= 0x20000000U;
    else if(pc < 0x00800000U)
        pc += 0x80000000U;
    return geEditorResolutionValid && pc >= geEditorTitleRAM &&
        pc < geEditorTitleRAM + GE_EDITOR_TITLE_WORDS * 4U;
}

static void GEReconcileNativeEditorReturn(void)
{
    unsigned int index;
    BOOL original;
    if(!gepdGameEntryReached || emustatus.game_hack != GHACK_GE ||
        gMemoryState.ROM_Image == NULL || gMS_RDRAM == NULL ||
        current_rdram_size < 0x800000U || GEUsesROMCodeMapping() ||
        !GEEditorResolve() || !GEEditorContextMatches(FALSE))
        return;
    original = GEEditorWordsMatch(FALSE, geEditorTitleROM,
        geEditorOriginalCode, GE_EDITOR_TITLE_WORDS);
    if(!original && !GEEditorWordsMatch(FALSE, geEditorTitleROM,
        geEditorPatchedCode, GE_EDITOR_TITLE_WORDS))
        return;
    if(original)
    {
        for(index = 0; index < GE_EDITOR_TITLE_WORDS; index++)
            GERecordROMPatch(4, index, geEditorTitleROM + index * 4U,
                geEditorPatchedCode[index]);
        for(index = 0; index < GE_EDITOR_TITLE_WORDS; index++)
            GEWriteROMWord(geEditorTitleROM + index * 4U, geEditorPatchedCode[index]);
    }
    if(!GEEditorWordsMatch(TRUE, geEditorTitleRAM,
        geEditorOriginalCode, GE_EDITOR_TITLE_WORDS) || !GEEditorContextMatches(TRUE) ||
        LOAD_UWORD_PARAM(geEditorMenuEntry) != geeditorcontexts[4].ram)
        return;
    /* Shared scheduler guard covers live and suspended frames in either repair. */
    if(!GETextureThreadsSafe())
    {
        InterlockedExchange(&gepdPatchesPending, 1);
        return;
    }
    for(index = 0; index < GE_EDITOR_TITLE_WORDS; index++)
        if(geEditorOriginalCode[index] != geEditorPatchedCode[index])
            GEPDWriteRAMCode(geEditorTitleRAM + index * 4U, geEditorPatchedCode[index]);
}

/* Plus Map Maker textures: choose by the cartridge title, then locate the
 * compatible routines independently of ROM revisions and unrelated editor code.
 * Only address operands identified in the texture routines are normalized;
 * resident code must also match every exact operand in this cartridge image. */
typedef struct GE_TEXTURE_PATCH
{
    unsigned int ram, original, patched;
} GE_TEXTURE_PATCH;
static GE_TEXTURE_PATCH geTexturePatches[28];
typedef struct GE_TEXTURE_TEMPLATE
{
    unsigned int context, offset, original, patched;
} GE_TEXTURE_TEMPLATE;
static const GE_TEXTURE_TEMPLATE geTextureTemplates[] = {
    {0U, 0x0018U, 0x29C10BB8U, 0x29C10A8AU},
    {1U, 0x0068U, 0x2A010BB8U, 0x2A010A8AU},
    {1U, 0x006CU, 0x24100BB7U, 0x24100A89U},
    {1U, 0x0070U, 0x2A010BB8U, 0x2A010A8AU},
    {3U, 0x0000U, 0x27BDFFD8U, 0x27BDFFE0U},
    {3U, 0x0004U, 0xAFB20020U, 0xAFBF001CU},
    {3U, 0x0008U, 0xAFB00018U, 0xAFB00018U},
    {3U, 0x000CU, 0x00A09025U, 0xAFB10014U},
    {3U, 0x0010U, 0xAFBF0024U, 0x24080018U},
    {3U, 0x0014U, 0xAFB1001CU, 0x16880002U},
    {3U, 0x0018U, 0x18A00008U, 0x00808021U},
    {3U, 0x001CU, 0x00008025U, 0x03C58021U},
    {3U, 0x0020U, 0x00808825U, 0x18A00007U},
    {3U, 0x0024U, 0x0C1BC93BU, 0x00A08821U},
    {3U, 0x0028U, 0x24040001U, 0x0C1BC93BU},
    {3U, 0x002CU, 0x26100001U, 0x24040001U},
    {3U, 0x0030U, 0x26310001U, 0xA2020000U},
    {3U, 0x0034U, 0x1612FFFBU, 0x2631FFFFU},
    {3U, 0x0038U, 0xA222FFFFU, 0x1620FFFBU},
    {3U, 0x003CU, 0x8FBF0024U, 0x26100001U},
    {3U, 0x0040U, 0x8FB00018U, 0x8FBF001CU},
    {3U, 0x0044U, 0x8FB1001CU, 0x8FB00018U},
    {3U, 0x0048U, 0x8FB20020U, 0x8FB10014U},
    {3U, 0x004CU, 0x03E00008U, 0x03E00008U},
    {3U, 0x0050U, 0x27BD0028U, 0x27BD0020U},
    {4U, 0x09A4U, 0x03203025U, 0x00193042U},
    {4U, 0x09C0U, 0x000E7880U, 0x01C07821U},
    {4U, 0x09C4U, 0x01EE7823U, 0x00000000U},
};
typedef struct GE_TEXTURE_CONTEXT
{
    unsigned int ram, words, originalHash, patchedHash;
    unsigned int rom, cache;
} GE_TEXTURE_CONTEXT;
static GE_TEXTURE_CONTEXT geTextureContexts[] = {
    {0U, 36U, 0xF1D14DABU, 0x949CBD15U, 0U, 0U}, /* validator */
    {0U, 59U, 0xF9A27968U, 0xBFECF5CEU, 0U, 36U}, /* cycle */
    {0U, 614U, 0xFB4081EAU, 0xFB4081EAU, 0U, 95U}, /* decode */
    {0U, 21U, 0xB7FDFAA7U, 0x472F492DU, 0U, 709U}, /* alpha */
    {0U, 699U, 0xD7BC42EFU, 0x01EF0360U, 0U, 730U}, /* pixels */
    {0U, 1556U, 0x845F3AA0U, 0x845F3AA0U, 0U, 1429U}, /* loader */
};
/* Low address operands: verified independently from the original and V2
 * disassemblies. Branch displacements, arithmetic constants and register
 * operands remain exact. J/JAL destinations and LUI address halves relocate. */
typedef struct GE_TEXTURE_ADDRESS
{
    unsigned int context, offset;
} GE_TEXTURE_ADDRESS;
static const GE_TEXTURE_ADDRESS geTextureAddresses[] = {
    {0U, 0x0000002CU},
    {1U, 0x00000004U},
    {1U, 0x00000034U},
    {1U, 0x000000B4U},
    {1U, 0x000000C8U},
    {2U, 0x00000008U},
    {2U, 0x00000050U},
    {2U, 0x00000054U},
    {2U, 0x00000068U},
    {2U, 0x0000006CU},
    {2U, 0x000001C4U},
    {2U, 0x000001F0U},
    {2U, 0x00000214U},
    {2U, 0x000002A0U},
    {2U, 0x000002E8U},
    {2U, 0x000002F0U},
    {2U, 0x00000324U},
    {2U, 0x00000378U},
    {2U, 0x00000398U},
    {2U, 0x000003DCU},
    {2U, 0x0000043CU},
    {2U, 0x0000046CU},
    {2U, 0x000004D0U},
    {2U, 0x00000520U},
    {2U, 0x0000058CU},
    {2U, 0x000005F4U},
    {2U, 0x00000604U},
    {2U, 0x00000668U},
    {2U, 0x000006CCU},
    {2U, 0x00000710U},
    {2U, 0x00000738U},
    {2U, 0x000007C0U},
    {2U, 0x000007E4U},
    {2U, 0x000007F4U},
    {2U, 0x00000860U},
    {4U, 0x00000048U},
    {5U, 0x0000006CU},
    {5U, 0x00000428U},
    {5U, 0x00000E0CU},
    {5U, 0x0000100CU},
    {5U, 0x00001288U},
    {5U, 0x00001290U},
    {5U, 0x00001298U},
    {5U, 0x000012B0U},
    {5U, 0x000012C8U},
    {5U, 0x000013E8U},
    {5U, 0x000013F8U},
    {5U, 0x00001410U},
    {5U, 0x00001444U},
    {5U, 0x0000144CU},
    {5U, 0x0000145CU},
    {5U, 0x00001490U},
    {5U, 0x00001498U},
    {5U, 0x000014CCU},
    {5U, 0x00001538U},
    {5U, 0x00001584U},
    {5U, 0x000015A4U},
    {5U, 0x000015C0U},
    {5U, 0x00001618U},
    {5U, 0x00001640U},
    {5U, 0x000017B4U},
    {5U, 0x000017BCU},
    {5U, 0x000017C8U},
    {5U, 0x000017D0U},
    {5U, 0x000017DCU},
    {5U, 0x000017ECU},
    {5U, 0x00001828U},
};
static unsigned int geTextureOriginalCode[2985];
static unsigned int geTexturePatchedCode[2985];
static BOOL geTextureResolutionInitialized = FALSE;
static BOOL geTextureResolutionValid = FALSE;
static unsigned int geTextureROMAdd = 0;
static unsigned int geTextureTable = 0;

static void GETextureResetResolution(void)
{
    unsigned int index;
    geTextureResolutionInitialized = FALSE;
    geTextureResolutionValid = FALSE;
    geTextureROMAdd = 0;
    geTextureTable = 0;
    memset(geTexturePatches, 0, sizeof(geTexturePatches));
    for(index = 0; index < sizeof(geTextureContexts) / sizeof(geTextureContexts[0]); index++)
    {
        geTextureContexts[index].ram = 0;
        geTextureContexts[index].rom = 0;
    }
}

static BOOL GETexturePlusTitle(void)
{
    static const char title[] = "GOLDENEYE 007 PLUS";
    unsigned int start, index, value;
    if(gMemoryState.ROM_Image == NULL || gAllocationLength < 0x40U)
        return FALSE;
    /* Read exactly the 20-byte title from word-swapped emulator storage. */
    for(start = 0; start + sizeof(title) - 1U <= 20U; start++)
    {
        for(index = 0; index < sizeof(title) - 1U; index++)
        {
            value = GEReadROMWord(0x20U + ((start + index) & ~3U));
            value = (value >> ((3U - ((start + index) & 3U)) * 8U)) & 0xFFU;
            if(value >= 'a' && value <= 'z')
                value -= 'a' - 'A';
            if(value != (unsigned int)title[index])
                break;
        }
        if(index == sizeof(title) - 1U)
            return TRUE;
    }
    return FALSE;
}

static BOOL GETextureROMBounds(unsigned int address, unsigned int words)
{
    return (address & 3U) == 0 && address <= gAllocationLength &&
        words <= (gAllocationLength - address) / 4U;
}

static unsigned int GETextureWordMask(unsigned int context, unsigned int offset,
    unsigned int word)
{
    unsigned int index, opcode = word >> 26;
    if(opcode == 2U || opcode == 3U)
        return 0xFC000000U;
    if(opcode == 15U)
        return 0xFFFF0000U;
    for(index = 0; index < sizeof(geTextureAddresses) / sizeof(geTextureAddresses[0]); index++)
        if(geTextureAddresses[index].context == context &&
            geTextureAddresses[index].offset == offset)
            return 0xFFFF0000U;
    return 0xFFFFFFFFU;
}

static unsigned int GETextureNormalizedHash(unsigned int context, unsigned int address)
{
    unsigned int index, word, hash = 2166136261U;
    if(!GETextureROMBounds(address, geTextureContexts[context].words))
        return 0;
    for(index = 0; index < geTextureContexts[context].words; index++)
    {
        word = GEReadROMWord(address + index * 4U);
        hash ^= word & GETextureWordMask(context, index * 4U, word);
        hash *= 16777619U;
    }
    return hash;
}

static unsigned int GETextureFindRoutine(unsigned int context, BOOL *patched)
{
    /* Cheap exact instruction prefixes keep the whole-ROM search bounded. */
    static const unsigned int prefix[3][4] = {
        {0x308EFFFFU, 0xAFA40000U, 0x3401FFFFU, 0x01C02025U},
        {0x3C0E0000U, 0x8DCE0000U, 0x27BDFFD0U, 0xAFB00018U},
        {0x27BDFFD8U, 0xAFB20020U, 0xAFB00018U, 0x00A09025U}
    };
    static const unsigned int alphaPatched[4] = {
        0x27BDFFE0U, 0xAFBF001CU, 0xAFB00018U, 0xAFB10014U
    };
    unsigned int offset, index, value, hash, found = 0;
    unsigned int row = context == 3U ? 2U : context;
    BOOL originalPrefix, patchedPrefix;
    if(gAllocationLength < geTextureContexts[context].words * 4U)
        return 0;
    for(offset = 0x1000U; offset <= gAllocationLength - geTextureContexts[context].words * 4U; offset += 4U)
    {
        originalPrefix = TRUE;
        patchedPrefix = context == 3U;
        for(index = 0; index < 4U; index++)
        {
            value = GEReadROMWord(offset + index * 4U);
            value &= context == 1U && index < 2U ? 0xFFFF0000U : 0xFFFFFFFFU;
            if(value != prefix[row][index])
                originalPrefix = FALSE;
            if(context != 3U || value != alphaPatched[index])
                patchedPrefix = FALSE;
            if(!originalPrefix && !patchedPrefix)
                break;
        }
        if(!originalPrefix && !patchedPrefix)
            continue;
        hash = GETextureNormalizedHash(context, offset);
        if(hash != geTextureContexts[context].originalHash &&
            hash != geTextureContexts[context].patchedHash)
            continue;
        if(found != 0)
            return 0;
        found = offset;
        *patched = hash == geTextureContexts[context].patchedHash;
    }
    return found;
}

static BOOL GETexturePatchMatches(BOOL ram, BOOL patched)
{
    unsigned int index;
    for(index = 0; index < sizeof(geTexturePatches) / sizeof(geTexturePatches[0]); index++)
    {
        const GE_TEXTURE_PATCH *patch = &geTexturePatches[index];
        unsigned int value = patched ? patch->patched : patch->original;
        if(!GEEditorWordsMatch(ram, ram ? patch->ram : patch->ram - geTextureROMAdd, &value, 1))
            return FALSE;
    }
    return TRUE;
}

static BOOL GETextureContextMatches(BOOL ram, BOOL patched)
{
    unsigned int index;
    const unsigned int *code = patched ? geTexturePatchedCode : geTextureOriginalCode;
    for(index = 0; index < sizeof(geTextureContexts) / sizeof(geTextureContexts[0]); index++)
    {
        const GE_TEXTURE_CONTEXT *context = &geTextureContexts[index];
        if(!GEEditorWordsMatch(ram, ram ? context->ram : context->rom,
            code + context->cache, context->words))
            return FALSE;
    }
    return TRUE;
}

static unsigned int GETextureCallTarget(unsigned int address)
{
    unsigned int word = GEReadROMWord(address);
    return (word >> 26) == 3U ? 0x80000000U | ((word & 0x03FFFFFFU) << 2) : 0;
}

static BOOL GETextureResolve(void)
{
    static const unsigned int alphaCalls[5] = {0x338U, 0x3F0U, 0x480U, 0x67CU, 0x74CU};
    unsigned int index, word, target, alpha, address, cache;
    BOOL validatorPatched = FALSE, cyclePatched = FALSE, alphaPatched = FALSE;
    if(geTextureResolutionInitialized)
        return geTextureResolutionValid;
    geTextureResolutionInitialized = TRUE;
    if(!GETexturePlusTitle())
        return FALSE;
    geTextureContexts[0].rom = GETextureFindRoutine(0U, &validatorPatched);
    geTextureContexts[1].rom = GETextureFindRoutine(1U, &cyclePatched);
    alpha = GETextureFindRoutine(3U, &alphaPatched);
    if(!geTextureContexts[0].rom || !geTextureContexts[1].rom || alpha < 0x1C90U ||
        validatorPatched != cyclePatched || validatorPatched != alphaPatched ||
        alpha > gAllocationLength || gAllocationLength - alpha < 0x2698U)
        return FALSE;
    geTextureContexts[2].rom = alpha - 0x1C90U;
    geTextureContexts[3].rom = alpha;
    geTextureContexts[4].rom = alpha + 0x35CU;
    geTextureContexts[5].rom = alpha + 0xE48U;
    for(index = 0; index < sizeof(geTextureContexts) / sizeof(geTextureContexts[0]); index++)
    {
        const GE_TEXTURE_CONTEXT *context = &geTextureContexts[index];
        if(GETextureNormalizedHash(index, context->rom) !=
            (alphaPatched ? context->patchedHash : context->originalHash))
            return FALSE;
    }
    /* The alpha helper calls this reader inside the validated tail context.
     * Its absolute target establishes the physical code mapping. */
    target = GETextureCallTarget(alpha + (alphaPatched ? 0x28U : 0x24U));
    if(target < 0x80000000U || target >= 0x80800000U || target < alpha + 0x2614U)
        return FALSE;
    geTextureROMAdd = target - (alpha + 0x2614U);
    for(index = 0; index < sizeof(geTextureContexts) / sizeof(geTextureContexts[0]); index++)
    {
        GE_TEXTURE_CONTEXT *context = &geTextureContexts[index];
        if(context->rom > 0xFFFFFFFFU - geTextureROMAdd)
            return FALSE;
        context->ram = context->rom + geTextureROMAdd;
        if(context->ram < 0x80000000U || context->ram >= 0x80800000U ||
            context->words > (0x80800000U - context->ram) / 4U)
            return FALSE;
    }
    if(GETextureCallTarget(geTextureContexts[1].rom + 0x80U) != geTextureContexts[0].ram ||
        GETextureCallTarget(geTextureContexts[1].rom + 0xA4U) != geTextureContexts[0].ram)
        return FALSE;
    for(index = 0; index < sizeof(alphaCalls) / sizeof(alphaCalls[0]); index++)
        if(GETextureCallTarget(geTextureContexts[2].rom + alphaCalls[index]) != geTextureContexts[3].ram)
            return FALSE;
    word = GEReadROMWord(geTextureContexts[0].rom + 0x20U);
    target = GEReadROMWord(geTextureContexts[0].rom + 0x2CU);
    if((word & 0xFFFF0000U) != 0x3C060000U || (target & 0xFFFF0000U) != 0x24C60000U)
        return FALSE;
    geTextureTable = ((word & 0xFFFFU) << 16) + (int)(short)(target & 0xFFFFU);
    if((geTextureTable & 3U) || geTextureTable < 0x80000000U ||
        geTextureTable > 0x80800000U - 2699U * 8U)
        return FALSE;
    target = GEReadROMWord(alpha + (alphaPatched ? 0x28U : 0x24U));
    for(index = 0; index < sizeof(geTexturePatches) / sizeof(geTexturePatches[0]); index++)
    {
        const GE_TEXTURE_TEMPLATE *source = &geTextureTemplates[index];
        GE_TEXTURE_PATCH *patch = &geTexturePatches[index];
        patch->ram = geTextureContexts[source->context].ram + source->offset;
        patch->original = (source->original >> 26) == 3U ? target : source->original;
        patch->patched = (source->patched >> 26) == 3U ? target : source->patched;
    }
    if(!GETexturePatchMatches(FALSE, alphaPatched))
        return FALSE;
    for(index = 0; index < sizeof(geTextureContexts) / sizeof(geTextureContexts[0]); index++)
    {
        const GE_TEXTURE_CONTEXT *context = &geTextureContexts[index];
        for(word = 0; word < context->words; word++)
        {
            address = GEReadROMWord(context->rom + word * 4U);
            geTextureOriginalCode[context->cache + word] = address;
            geTexturePatchedCode[context->cache + word] = address;
        }
    }
    for(index = 0; index < sizeof(geTexturePatches) / sizeof(geTexturePatches[0]); index++)
    {
        const GE_TEXTURE_TEMPLATE *source = &geTextureTemplates[index];
        cache = geTextureContexts[source->context].cache + source->offset / 4U;
        geTextureOriginalCode[cache] = geTexturePatches[index].original;
        geTexturePatchedCode[cache] = geTexturePatches[index].patched;
    }
    geTextureResolutionValid = TRUE;
    return TRUE;
}

/* These scheduler signatures establish the active-thread list and saved
 * PC/RA offsets used below; no thread structure is inferred for other ROMs. */
static const unsigned int geTextureThreadCreate[] = {
    0x3C0A8002, 0x8D4A772C, 0x8FAB0028, 0x00408025, 0x3C018002,
    0xAD6A000C, 0x8FB90028, 0x02002025, 0x0C006120, 0xAC39772C
};
static const unsigned int geTextureThreadEntry[] = {
    0x3C1A8002, 0x8F5A7730, 0xDD090020, 0xFF490020, 0xDD090118, 0xFF490118
};
static const unsigned int geTextureThreadRegs[] = {
    0xFF5C00E8, 0xFF5D00F0, 0xFF5E00F8, 0xFF5F0100
};
static const unsigned int geTextureThreadPC[] = {
    0xAF490128, 0x40087000, 0xAF48011C, 0x8F480018
};
static const unsigned int geTextureThreadYield[] = {
    0xFCBD00F0, 0xFCBE00F8, 0xFCBF0100, 0x13600009, 0xACBF011C
};
static const GE_EDITOR_CONTEXT geTextureThreadContexts[] = {
    {0x0000DFD8U, 0x8000D3D8U, 10U, geTextureThreadCreate},
    {0x00010C48U, 0x80010048U, 6U, geTextureThreadEntry},
    {0x00010CE0U, 0x800100E0U, 4U, geTextureThreadRegs},
    {0x00010D60U, 0x80010160U, 4U, geTextureThreadPC},
    {0x00011258U, 0x80010658U, 5U, geTextureThreadYield}
};

static BOOL GETexturePCActive(unsigned int address)
{
    if(address >= 0xA0000000U && address < 0xA0800000U)
        address -= 0x20000000U;
    else if(address < 0x00800000U)
        address += 0x80000000U;
    return GEEditorPCInTitle(address) || (address >= geTextureContexts[0].ram &&
            address < geTextureContexts[0].ram + geTextureContexts[0].words * 4U) ||
        (address >= geTextureContexts[1].ram &&
            address < geTextureContexts[1].ram + geTextureContexts[1].words * 4U) ||
        (address >= geTextureContexts[2].ram &&
            address < geTextureContexts[5].ram + geTextureContexts[5].words * 4U);
}

static BOOL GETextureThreadsSafe(void)
{
    unsigned int index, thread, running, count = 0;
    BOOL foundRunning = FALSE;
    if(GETexturePCActive((unsigned int)gHWS_pc) ||
        GETexturePCActive((unsigned int)gHWS_GPR[31]) ||
        (((unsigned int)gHWS_COP0Reg[STATUS] & 2U) &&
         GETexturePCActive((unsigned int)gHWS_COP0Reg[EPC])))
        return FALSE;
    for(index = 0; index < sizeof(geTextureThreadContexts) / sizeof(geTextureThreadContexts[0]); index++)
    {
        const GE_EDITOR_CONTEXT *context = &geTextureThreadContexts[index];
        if(!GEEditorWordsMatch(FALSE, context->rom, context->expected, context->words) ||
            !GEEditorWordsMatch(TRUE, context->ram, context->expected, context->words))
            return FALSE;
    }
    if(LOAD_UWORD_PARAM(0x80027720U) != 0 || LOAD_UWORD_PARAM(0x80027724U) != 0xFFFFFFFFU)
        return FALSE;
    thread = LOAD_UWORD_PARAM(0x8002772CU);
    running = LOAD_UWORD_PARAM(0x80027730U);
    while(thread != 0x80027720U)
    {
        /* The bound also rejects a cycle without allocating host memory. */
        if(++count > 32U || (thread & 7U) || thread < 0x80000000U || thread > 0x807FFE50U)
            return FALSE;
        if(GETexturePCActive(LOAD_UWORD_PARAM(thread + 0x11CU)) ||
            GETexturePCActive(LOAD_UWORD_PARAM(thread + 0x104U)))
            return FALSE;
        if(thread == running)
            foundRunning = TRUE;
        thread = LOAD_UWORD_PARAM(thread + 0x0CU);
    }
    return foundRunning;
}

static void GEReconcileEditorTextures(void)
{
    unsigned int index;
    BOOL original;
    if(!gepdGameEntryReached || emustatus.game_hack != GHACK_GE ||
        gMemoryState.ROM_Image == NULL || gMS_RDRAM == NULL ||
        current_rdram_size < 0x800000U || !GETextureResolve() || GEUsesROMCodeMapping())
        return;
    original = GETexturePatchMatches(FALSE, FALSE);
    if(!original && (!GETexturePatchMatches(FALSE, TRUE) || !GETexturePatchMatches(TRUE, FALSE)))
        return;
    if(!GETextureContextMatches(FALSE, !original))
        return;
    if(original)
    {
        /* One owned group: never restore or admit a mixture of corrections. */
        for(index = 0; index < sizeof(geTexturePatches) / sizeof(geTexturePatches[0]); index++)
            GERecordROMPatch(5, index, geTexturePatches[index].ram - geTextureROMAdd,
                geTexturePatches[index].patched);
        for(index = 0; index < sizeof(geTexturePatches) / sizeof(geTexturePatches[0]); index++)
            GEWriteROMWord(geTexturePatches[index].ram - geTextureROMAdd, geTexturePatches[index].patched);
    }
    if(!GETexturePatchMatches(TRUE, FALSE) || !GETextureContextMatches(TRUE, FALSE) ||
        LOAD_UWORD_PARAM(geTextureTable + 2697U * 8U) != 0x002EE9DBU ||
        LOAD_UWORD_PARAM(geTextureTable + 2698U * 8U) != 0x002EEF18U)
        return;
    /* A save may interrupt the old helper (including a suspended thread).
     * Its frame must finish before installing the new stack layout. */
    if(!GETextureThreadsSafe())
    {
        InterlockedExchange(&gepdPatchesPending, 1);
        return;
    }
    for(index = 0; index < sizeof(geTexturePatches) / sizeof(geTexturePatches[0]); index++)
        if(geTexturePatches[index].original != geTexturePatches[index].patched)
            GEPDWriteRAMCode(geTexturePatches[index].ram, geTexturePatches[index].patched);
}

static void GEPatchRAMFiringRate(void)
{
	unsigned int context, index;
	if(geRAMFiringSite != 0)
	{
		for(index = 0; index < 9; index++)
			if(LOAD_UWORD_PARAM(geRAMFiringSite - 0x10 + index * 4) !=
				(index == 4 ? 0x00021040 : geRAMFiringContext[index]))
				break;
		if(index == 9)
			return;
	}
	geRAMFiringSite = 0;
	context = GEPDFindRAMPattern(gefiringratepattern, gefiringratemask, 9);
	if(context != 0)
	{
		geRAMFiringSite = context + 0x10;
		for(index = 0; index < 9; index++)
			geRAMFiringContext[index] = LOAD_UWORD_PARAM(context + index * 4);
		GEPDWriteRAMCode(geRAMFiringSite, 0x00021040);
	}
}

static void GEPatchRAMHeadRoll(void)
{
	unsigned int address, index, match = 0;
	BOOL valid;
	if(geRAMHeadRollSite != 0)
	{
		valid = TRUE;
		for(index = 0; index < 6; index++)
		{
			address = geRAMHeadRollSite + index * 0x1C;
			if(LOAD_UWORD_PARAM(address) != 0 ||
				LOAD_UWORD_PARAM(address - 4) != geRAMHeadRollContext[index * 2] ||
				LOAD_UWORD_PARAM(address + 4) != geRAMHeadRollContext[index * 2 + 1])
				valid = FALSE;
		}
		if(valid)
			return;
		geRAMHeadRollSite = 0;
	}
	if(current_rdram_size < 5 * 0x1C + 8)
		return;
	for(address = 0x80001000; address <= 0x80000000 + current_rdram_size - (5 * 0x1C + 8); address += 4)
	{
		for(index = 0; index < 6; index++)
			if(LOAD_UWORD_PARAM(address + index * 0x1C) != geheadrolloriginal[index])
				break;
		if(index == 6)
		{
			if(match != 0)
				return;
			match = address;
		}
	}
	if(match == 0)
		return;
	geRAMHeadRollSite = match;
	for(index = 0; index < 6; index++)
	{
		address = match + index * 0x1C;
		geRAMHeadRollContext[index * 2] = LOAD_UWORD_PARAM(address - 4);
		geRAMHeadRollContext[index * 2 + 1] = LOAD_UWORD_PARAM(address + 4);
		GEPDWriteRAMCode(address, 0);
	}
}

static void GEPatchWatchLaserWeapon(void)
{
	unsigned int address;
	unsigned int match = 0;

	for(address = 0x80020000; address <= 0x8007FF90; address += 4)
	{
		if(LOAD_UWORD_PARAM(address + 0x20) == 0x03E8FF00 && LOAD_UWORD_PARAM(address + 0x6C) == 0x00600F91)
		{
			if(match != 0)
				return;
			match = address;
		}
	}

	if(match != 0)
		LOAD_UWORD_PARAM(match + 0x20) = 0x03E8FF02;
}

void GEFiringRateHack(void)
{
	int codeindex;
	unsigned int code;
	const GE_HACK_RESOLUTION *resolution = GEGetHackResolution();

	if(resolution->firingvalid && GEReadROMWord(resolution->readfiringrate) == 0x00000000)
	{
		GERecordROMPatch(0, 0, resolution->readfiringrate, 0x00021040);
		GEWriteROMWord(resolution->readfiringrate, 0x00021040);
	}

	if(resolution->dronevalid && GEReadROMWord(resolution->dronegunfiringrate) == 0x250B0002)
	{
		GERecordROMPatch(1, 0, resolution->dronegunfiringrate, 0x250B0004);
		GEWriteROMWord(resolution->dronegunfiringrate, 0x250B0004);
	}

	if(resolution->guardvalid &&
		GEPatternMatches(resolution->updateaimtarget, geupdateaimtargetpattern, geupdateaimtargetmask, 20) &&
		GEReadROMWord(resolution->updateaimtarget + 0x30) == resolution->updateaimtargetjal)
	{
		for(codeindex = 0; codeindex < 20; codeindex++)
		{
			code = gecodearray[codeindex];
			if(codeindex == 15)
				code = resolution->updateaimtargetjal;
			else if(codeindex == 18)
				code = resolution->updateaimtargetreturn;
			GERecordROMPatch(2, codeindex, resolution->updateaimtarget + codeindex * 4, code);
			GEWriteROMWord(resolution->updateaimtarget + (codeindex * 4), code);
		}
	}

	/* Authenticated paged games execute this code through the patched ROM
	 * mapping. Searching their full RDRAM every timer tick is unnecessary. */
	if(!resolution->rommappingvalid)
		GEPatchRAMFiringRate();
	GEPatchWatchLaserWeapon();
}

void GEDisableHeadRoll(void)
{
	int index;
	const GE_HACK_RESOLUTION *resolution = GEGetHackResolution();

	/* Physical-RAM loaders need their live copy reconciled. Paged games
	 * already execute the ROM patch and must not scan all RDRAM here. */
	if(!resolution->rommappingvalid)
		GEPatchRAMHeadRoll();
	if(!resolution->headrollvalid)
		return;

	for(index = 0; index < 6; index++)
	{
		if(GEReadROMWord(resolution->headrollnop[index]) != geheadrolloriginal[index])
			return;
	}

	for(index = 0; index < 6; index++)
	{
		GERecordROMPatch(3, index, resolution->headrollnop[index], 0);
		GEWriteROMWord(resolution->headrollnop[index], 0);
	}
}

static const unsigned int pdspeedpattern[27] = {0x8C8501E4, 0x0040F809, 0x8C8601E0, 0x0C005431, 0x00000000, 0x10400011, 0x3C0F8006, 0x8DEFEEC0, 0x51E0000F, 0x8FBF0014, 0x0C005207, 0x00000000, 0x5C40000B, 0x8FBF0014, 0x0C00543A, 0x00000000, 0x0C00508E, 0x00000000, 0x0C005451, 0x00000000, 0x3C04800A, 0x0C005016, 0x24849A60, 0x8FBF0014, 0x27BD0018, 0x03E00008, 0x00000000};
static const unsigned int pdspeedmask[27] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFC000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0000, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFC000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFC000000, 0xFFFFFFFF, 0xFC000000, 0xFFFFFFFF, 0xFC000000, 0xFFFFFFFF, 0xFFFF0000, 0xFC000000, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
static const unsigned int pdmasteroriginal[20] = {0x3C10800A, 0x3C11000B, 0x3C120002, 0x3C130005, 0x3C140001, 0xAFBF002C, 0x36947D78, 0x3673F5E1, 0x3652FAF0, 0x3631EBC2, 0x26109FC0, 0x0C012144, 0x00000000, 0x8E0E0018, 0x8E0F0020, 0x8E190024, 0x004E1823, 0x01E33821, 0x00F3C021, 0x0311001B};
static const unsigned int pdguardoriginal[18] = {0x808E0007, 0x24010008, 0x00001025, 0x15C1000A, 0x00000000, 0x8C8F004C, 0x31F80060, 0x13000006, 0x00000000, 0xAC85004C, 0x0FC0C495, 0xAC860050, 0x10000001, 0x24020001, 0x8FBF0014, 0x27BD0018, 0x03E00008, 0x00000000};
static const unsigned int pdcaveoriginal[42] = {0x4C494748, 0x5453203A, 0x20486974, 0x206F6363, 0x75726564, 0x206F6E20, 0x6C696768, 0x74202564, 0x20696E20, 0x726F6F6D, 0x2025640A, 0x00000000, 0x4C322825, 0x6429202D, 0x3E200000, 0x4C32202D, 0x3E204255, 0x494C4420, 0x4C494748, 0x54532054, 0x52414E53, 0x46455220, 0x5441424C, 0x45202D20, 0x53746172, 0x74696E67, 0x0A000000, 0x4C322825, 0x6429202D, 0x3E200000, 0x4C325F42, 0x75696C64, 0x5472616E, 0x73666572, 0x5461626C, 0x6573202D, 0x3E20466F, 0x756E6420, 0x25642070, 0x6F727461, 0x6C730A00, 0x4C322825};

static BOOL PDPreservedWords(unsigned int address, const unsigned int *words, unsigned int count)
{
	unsigned int index;
	if(address < 0x80000000 || address - 0x80000000 > current_rdram_size ||
		count * 4 > current_rdram_size - (address - 0x80000000))
		return FALSE;
	for(index = 0; index < count; index++)
		if(LOAD_UWORD_PARAM(address + index * 4) != words[index])
			return FALSE;
	return TRUE;
}

void PDTimingHack(void)
{
	int codeindex;
	/* This shim embeds globals and object layouts specific to NTSC 1.1.
	 * Relocating only its entry point would silently corrupt other builds.
	 * Keep it on the verified retail layout until every dependency can be
	 * resolved; the independent lib speed patch below supports moved code. */
	if(emustatus.game_hack != GHACK_PD || currentromoptions.crc1 != 0x41F2B98F ||
		currentromoptions.crc2 != 0xB458B466 || current_rdram_size < 0x3C7988)
		return;
	if(LOAD_UWORD_PARAM(PD_masterclock) != 0x3652FAF0)
		return;
	if(!PDPreservedWords(PD_masterclock - 0x20, pdmasteroriginal, 20) ||
		!PDPreservedWords(PD_updateaimtarget - 0x28, pdguardoriginal, 18) ||
		!PDPreservedWords(PD_newcodearea, pdcaveoriginal, 42))
		return;
	for(codeindex = 0; codeindex < 42; codeindex++)
		GEPDWriteRAMCode(PD_newcodearea + codeindex * 4, pdcodearray[codeindex]);
	GEPDWriteRAMCode(PD_masterclock, 0x0BC69E38);
	GEPDWriteRAMCode(PD_masterclock + 4, 0x3652FAF0);
	GEPDWriteRAMCode(PD_updateaimtarget, 0x0FC69E5A);
	GEPDWriteRAMCode(PD_updateaimtarget + 4, 0x8C8E0020);
}

void PDSpeedHack(void)
{
	unsigned int context, index;
	if(emustatus.game_hack != GHACK_PD)
		return;
	if(pdSpeedSite != 0)
	{
		for(index = 0; index < 27; index++)
			if(LOAD_UWORD_PARAM(pdSpeedSite - 0xC + index * 4) !=
				(index == 3 ? 0x10000013 : pdSpeedContext[index]))
				break;
		if(index == 27)
			return;
	}
	pdSpeedSite = 0;
	context = GEPDFindRAMPattern(pdspeedpattern, pdspeedmask, 27);
	if(context == 0)
		return;
	for(index = 0; index < 27; index++)
		pdSpeedContext[index] = LOAD_UWORD_PARAM(context + index * 4);
	pdSpeedSite = context + 0xC;
	GEPDWriteRAMCode(pdSpeedSite, 0x10000013);
}

static const unsigned int pdheadrollpattern[9] = {0x00047080, 0x01C47021, 0x000E7140, 0x3C02800B, 0x004E1021, 0x9442C800, 0x304F0080, 0x03E00008, 0x000F102B};
static const unsigned int pdheadrollmask[9] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0000, 0xFFFFFFFF, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};

void PDDisableHeadRoll(void)
{
	unsigned int context, index;
	if(emustatus.game_hack != GHACK_PD)
		return;
	if(pdHeadRollSite != 0)
	{
		for(index = 0; index < 9; index++)
			if(LOAD_UWORD_PARAM(pdHeadRollSite - 0x20 + index * 4) !=
				(index == 8 ? 0x00001025 : pdHeadRollContext[index]))
				break;
		if(index == 9)
			return;
	}
	pdHeadRollSite = 0;
	context = GEPDFindRAMPattern(pdheadrollpattern, pdheadrollmask, 9);
	if(context == 0)
		return;
	for(index = 0; index < 9; index++)
		pdHeadRollContext[index] = LOAD_UWORD_PARAM(context + index * 4);
	/* options_get_head_roll: preserve its load/config data and return false
	 * in the JR delay slot. This is the game's existing head-roll option. */
	pdHeadRollSite = context + 0x20;
	GEPDWriteRAMCode(pdHeadRollSite, 0x00001025);
}

void GEPDQueueRuntimeHacks(void)
{
	InterlockedExchange(&gepdPatchesPending, 1);
}

void GEPDApplyPendingHacks(void)
{
	/* Called at VI dispatch on the emulation thread, after the current
	 * generated block has returned. Never race code compilation on the UI
	 * timer thread when mutating instructions or invalidating native code. */
	if(!gepdGameEntryReached || !InterlockedExchange(&gepdPatchesPending, 0) || rominfo.TV_System != TV_SYSTEM_NTSC)
		return;
	if(emustatus.game_hack == GHACK_GE)
	{
		GEReconcileNativeEditorReturn();
		GEReconcileEditorTextures();
		if(emuoptions.GEFiringRateHack && emuoptions.OverclockFactor != 1)
			GEFiringRateHack();
		if(emuoptions.GEDisableHeadRoll)
			GEDisableHeadRoll();
	}
	else if(emustatus.game_hack == GHACK_PD)
	{
		if(emuoptions.PDSpeedHack && emuoptions.OverclockFactor != 1)
			PDSpeedHack();
		if(emuoptions.GEDisableHeadRoll)
			PDDisableHeadRoll();
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */

void GEPDOnGameEntry(void)
{
	/* IPL has finished validating cartridge data. Open the mutation gate
	 * before the game's own loader can copy relocated code from ROM. */
	gepdGameEntryReached = TRUE;
	GEPDApplyPendingHacks();
}

static const unsigned int pdpausepattern[14] = {0x3C028008, 0x03E00008, 0x8C424014, 0x3C028008, 0x03E00008, 0x8C424020, 0x04800003, 0x28810004, 0x14200002, 0x00000000, 0x00002025, 0x3C018008, 0x03E00008, 0xAC244020};
static const unsigned int pdpausemask[14] = {0xFFFF0000, 0xFFFFFFFF, 0xFFFF0000, 0xFFFF0000, 0xFFFFFFFF, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFF0000, 0xFFFFFFFF, 0xFFFF0000};

static unsigned int GEPDResolvePauseAddress(void)
{
	unsigned int context, address = 0;
	if(emustatus.game_hack == GHACK_GE)
		address = GEGetHackResolution()->pause;
	else if(emustatus.game_hack == GHACK_PD)
	{
		context = GEPDFindRAMPattern(pdpausepattern, pdpausemask, 14);
		if(context != 0)
			address = GEPDOperandAddress(LOAD_UWORD_PARAM(context), LOAD_UWORD_PARAM(context + 8));
	}
	if(address < 0x80000000 || current_rdram_size < 4 ||
		address - 0x80000000 > current_rdram_size - 4)
		return 0;
	return address;
}

void GEPDPause(BOOL pause)
{
	unsigned int address, state;
	if(rominfo.TV_System != TV_SYSTEM_NTSC || emustatus.game_hack == GHACK_NONE)
		return;
	if(pause)
	{
		address = GEPDResolvePauseAddress();
		if(address == 0)
			return;
		state = LOAD_UWORD_PARAM(address);
		if(state > 1)
			return;
		gepdPauseAddress = address;
		alreadypaused = state != 0;
		emustatus.gepd_pause = state != 0 ? 1 : 2;
		if(state == 0)
			LOAD_UWORD_PARAM(address) = 1;
	}
	else
	{
		/* Release only the same validated flag this session acquired. */
		address = gepdPauseAddress;
		if(!alreadypaused && address >= 0x80000000 && current_rdram_size >= 4 &&
			address - 0x80000000 <= current_rdram_size - 4 && LOAD_UWORD_PARAM(address) == 1)
			LOAD_UWORD_PARAM(address) = 0;
		alreadypaused = FALSE;
		gepdPauseAddress = 0;
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void SetCodeCheckMethod(int method)
{
	if(emustatus.Emu_Is_Running)
	{
		CheckMenuItem(gui.hMenu1964main, codecheckmenulist[emustatus.CodeCheckMethod - 1], MF_UNCHECKED);
		if(PauseEmulator())
		{
			Dyna_Check_Codes = Dyna_Code_Check[method - 1];
			emustatus.CodeCheckMethod = method;
			ResumeEmulator(REFRESH_DYNA_AFTER_PAUSE);	/* Need to init emu */
			CheckMenuItem(gui.hMenu1964main, codecheckmenulist[method - 1], MF_CHECKED);
		}
	}
	else
	{
		CheckMenuItem(gui.hMenu1964main, codecheckmenulist[emustatus.CodeCheckMethod - 1], MF_UNCHECKED);
		emustatus.CodeCheckMethod = method;
		defaultoptions.Code_Check = method;
		CheckMenuItem(gui.hMenu1964main, codecheckmenulist[method - 1], MF_CHECKED);
	}
}

WINDOWPLACEMENT window_placement_save_romlist;

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void RememberWindowSize(void)
{
	/* Try to remember the main window position and size before playing a game */
	window_placement_save_romlist.length = sizeof(WINDOWPLACEMENT);
	GetWindowPlacement(gui.hwnd1964main, &window_placement_save_romlist);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void ResetWindowSizeAsRemembered(void)
{
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	RECT	*prect = &(window_placement_save_romlist.rcNormalPosition);
	RECT    Rect;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	GetWindowRect(gui.hwnd1964main, &Rect);
	SetWindowPos
	(
		gui.hwnd1964main,
		NULL,
		Rect.left,
		Rect.top,
		prect->right - prect->left ,
		prect->bottom - prect->top ,
		SWP_NOZORDER | SWP_SHOWWINDOW
	);
}

LONG windowedStyle;
LONG windowedExStyle;
RECT lastRect;

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void SetWindowBorderless(void)
{
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	RECT	rcToolBar;
	int     screenWidthPos, screenHeightPos, screenWidth, screenHeight;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	guistatus.IsBorderless = TRUE;
	windowedStyle = GetWindowLong(gui.hwnd1964main, GWL_STYLE);
	windowedExStyle = GetWindowLong(gui.hwnd1964main, GWL_EXSTYLE);

	GetWindowRect(gui.hToolBar, &rcToolBar);
	GetWindowRect(gui.hwnd1964main, &lastRect);
	screenWidthPos = lastRect.left;
	screenHeightPos = lastRect.top - ((rcToolBar.bottom - rcToolBar.top) / 2);
	screenWidth = lastRect.right - lastRect.left;
	screenHeight = lastRect.bottom - lastRect.top;

	SetMenu(gui.hwnd1964main, NULL);
	ShowWindow(gui.hStatusBar, SW_HIDE);
	ShowWindow(gui.hToolBar, SW_HIDE);
	SetWindowLong(gui.hwnd1964main, GWL_STYLE, WS_POPUP);
	SetWindowLong(gui.hwnd1964main, GWL_EXSTYLE, WS_EX_APPWINDOW | WS_EX_TOPMOST);
	SetWindowPos(gui.hwnd1964main, NULL, screenWidthPos > 0 ? screenWidthPos : 0, screenHeightPos > 0 ? screenHeightPos : 0, screenWidth > 0 ? screenWidth : 0, screenHeight > 0 ? screenHeight : 0, SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void UnsetWindowBorderless(void)
{
	guistatus.IsBorderless = FALSE;
	SetMenu(gui.hwnd1964main, gui.hMenu1964main);
	ShowWindow(gui.hStatusBar, SW_SHOW);
	ShowWindow(gui.hToolBar, SW_SHOW);
	SetWindowLong(gui.hwnd1964main, GWL_STYLE, windowedStyle);
	SetWindowLong(gui.hwnd1964main, GWL_EXSTYLE, windowedExStyle);

	SetWindowPos
	(
		gui.hwnd1964main,
		NULL,
		lastRect.left,
		lastRect.top,
		lastRect.right - lastRect.left,
		lastRect.bottom - lastRect.top,
		SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW
	);
}

extern char critical_msg_buffer[32 * 1024]; /* 32KB */

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void PrepareBeforePlay(int IsFullScreen)
{
	if (IsFullScreen == 0)	RememberWindowSize();
	critical_msg_buffer[0] = '\0';					/* clear the critical message buffer */

	if (IsFullScreen == 0)
	{
	/* Hide romlist */
	RomListSaveCurrentPos();
	ListView_SetExtendedListViewStyle(gui.hwndRomList, LVS_EX_FULLROWSELECT);
	NewRomList_ListViewHideHeader(gui.hwndRomList);
	ShowWindow(gui.hwndRomList, SW_HIDE);
	EnableWindow(gui.hwndRomList, FALSE);
	}

	/* Setting options */
	RomListSelectLoadedRomEntry();
	GenerateCurrentRomOptions();

	/* Hide cursor */
	if(guioptions.auto_hide_cursor_when_active)
		HideCursor(TRUE);

	GEPDResetHackResolution();
	emustatus.game_hack = GHACK_NONE;
	if(rominfo.TV_System == TV_SYSTEM_NTSC) // if USA ROM
	{
		if(GEGetHackResolution()->gamevalid || !strncmp(currentromoptions.Game_Name, "GOLDENEYE", 9) || strnstr(currentromoptions.Game_Name, "GOLD", 4) != NULL)
			emuoptions.UsingRspPlugin = TRUE, emustatus.game_hack = GHACK_GE;
		else if(!strncmp(currentromoptions.Game_Name, "Perfect Dark", 12) || !strncmp(currentromoptions.Game_Name, "GoldenEye X", 11) || strnstr(currentromoptions.Game_Name, "Perfect", 7) != NULL)
			emuoptions.UsingRspPlugin = FALSE, emustatus.game_hack = GHACK_PD;
	}

	init_whole_mem_func_array();					/* Needed here. The tlb function pointers change. */
	ResetRdramSize(currentromoptions.RDRAM_Size);
	if(strcpy(current_cheatcode_rom_internal_name, currentromoptions.Game_Name) != 0)
		CodeList_ReadCode(currentromoptions.Game_Name);

	SetCounterFactor(currentromoptions.Counter_Factor);
	emustatus.CodeCheckMethod = currentromoptions.Code_Check;

	/*
	 * Using the Check_QWORD to boot, will switch to ROM specified
	 * emustatus.CodeCheckMethod  
	 * at first FPU exception. I don't know why use NoCheck method will not boot  
	 * Game like SuperMario should not need to do DynaCodeCheck but how the ROM does
	 * not boot  
	 * with DynaCodeCheck, need debug
	 */
	if(emustatus.CodeCheckMethod == CODE_CHECK_NONE || emustatus.CodeCheckMethod == CODE_CHECK_DMA_ONLY)
	{
		Dyna_Check_Codes = Dyna_Code_Check_None_Boot;
		TRACE0("Set code check method = Dyna_Code_Check_None_Boot / Check_DMA_only");
	}
	else
	{
		Dyna_Check_Codes = Dyna_Code_Check[emustatus.CodeCheckMethod - 1];
	}

	emustatus.cpucore = currentromoptions.Emulator;
	SendMessage
	(
		gui.hwnd1964main,
		WM_COMMAND,
		emustatus.cpucore == DYNACOMPILER ? ID_DYNAMICCOMPILER : ID_INTERPRETER,
		0
	);

	/* About FPU usage exceptions */
	if(currentromoptions.FPU_Hack == USEFPUHACK_YES)
	{
		EnableFPUUnusableException();
	}
	else
	{
		DisableFPUUnusableException();
	}

	Flashram_Init();
	Init_iPIF();

	emustatus.DListCount = 0;
	emustatus.AListCount = 0;
	emustatus.PIDMACount = 0;
	emustatus.ControllerReadCount = 0;

	if(!QueryPerformanceFrequency(&Freq))
	{
		currentromoptions.Max_FPS = MAXFPS_NONE;	/* ok, this computer does not support */
		/* accurate timer, don't use speed limiter */
	}
	else
	{
		if(rominfo.TV_System == 0)					/* PAL */
		{
			vips_speed_limits[MAXFPS_AUTO_SYNC] = vips_speed_limits[MAXFPS_PAL_50];
		}
		else	/* NTSC */
		{
			vips_speed_limits[MAXFPS_AUTO_SYNC] = vips_speed_limits[MAXFPS_NTSC_60];
		}
	}
	if (IsFullScreen == 0)
	{
		CheckMenuItem(gui.hMenu1964main, codecheckmenulist[emustatus.CodeCheckMethod - 1], MF_UNCHECKED);
		CheckMenuItem(gui.hMenu1964main, codecheckmenulist[emustatus.CodeCheckMethod - 1], MF_CHECKED);
		sprintf(generalmessage, "CF=%d", emuoptions.OverclockFactor == 1 ? currentromoptions.Counter_Factor : 1);
		SetStatusBarText(2, generalmessage);
		SetStatusBarText(4, emustatus.cpucore == DYNACOMPILER ? "D" : "I");
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void AfterStop(void)
{
#ifdef CHEATCODE_LOCK_MEMORY
	CloseCheatCodeEngineMemoryLock();
#endif

	Close_Save();
	Close_iPIF();

	ResetWindowSizeAsRemembered();

	emustatus.Emu_Is_Running = FALSE;
	emustatus.game_hack = GHACK_NONE;
	EnableMenuItem(gui.hMenu1964main, ID_OPENROM, MF_ENABLED);
	EnableButton(ID_BUTTON_OPEN_ROM, TRUE);
	EnableMenuItem(gui.hMenu1964main, IDM_PLUGINS, MF_ENABLED);
	EnableButton(ID_BUTTON_SETUP_PLUGINS, TRUE);
	EnableMenuItem(gui.hMenu1964main, ID_CLOSEROM, MF_ENABLED);
	EnableMenuItem(gui.hMenu1964main, ID_ROM_PAUSE, MF_GRAYED);
	EnableRadioButtons(FALSE);

	/* EnableMenuItem(gui.hMenu1964main, ID_ROM_STOP, MF_GRAYED); */
	EnableMenuItem(gui.hMenu1964main, ID_PLUGINS_SCREENSHOTS, MF_GRAYED);
	DisableStateMenu();

	ShowWindow(gui.hwndRomList, SW_SHOW);

	EnableWindow(gui.hwndRomList, TRUE);
	ListView_SetExtendedListViewStyle(gui.hwndRomList, LVS_EX_FULLROWSELECT);	/* | LVS_EX_TRACKSELECT ); */
	NewRomList_ListViewShowHeader(gui.hwndRomList);

	/* refresh the rom list, just to prevent user has changed resolution */
	NewRomList_ListViewChangeWindowRect();
	DockStatusBar();
	RomListUseSavedPos();

	/* Reset some of the default options */
	defaultoptions.Emulator = DYNACOMPILER;
	emustatus.cpucore = defaultoptions.Emulator;
	SendMessage
	(
		gui.hwnd1964main,
		WM_COMMAND,
		emustatus.cpucore == DYNACOMPILER ? ID_DYNAMICCOMPILER : ID_INTERPRETER,
		0
	);
	SetStatusBarText(4, emustatus.cpucore == DYNACOMPILER ? "D" : "I");
	SetCounterFactor(defaultoptions.Counter_Factor);
	SetCodeCheckMethod(defaultoptions.Code_Check);

	/* Flash the status bar */
	if(guioptions.display_statusbar)
	{
		ShowWindow(gui.hStatusBar, SW_HIDE);
		ShowWindow(gui.hStatusBar, SW_SHOW);
	}
	SetStatusBarText(3, defaultoptions.RDRAM_Size == RDRAMSIZE_4MB ? "4MB" : "8MB");

	sprintf(generalmessage, "%s - Stopped", gui.szWindowTitle);
	SetWindowText(gui.hwnd1964main, generalmessage);
	
	Set_Ready_Message();
	SetStatusBarText(1, " 0 VI/s");

	if( NeedFreshromListAfterStop == TRUE )
	{
		NeedFreshromListAfterStop = FALSE;
		OnFreshRomList();
	}
}

/*
 =======================================================================================================================
    Move the status bar to the bottom of the main window.
 =======================================================================================================================
 */
void DockStatusBar(void)
{
	/*~~~~~~~~~~~~~~~~~~~~*/
	RECT	rc, rcstatusbar;
	RECT	rcToolBar;
	/*~~~~~~~~~~~~~~~~~~~~*/

	if(gui.hStatusBar == NULL) return;
	GetClientRect(gui.hwnd1964main, &rc);
	GetWindowRect(gui.hStatusBar, &rcstatusbar);
	GetWindowRect(gui.hToolBar, &rcToolBar);
	MoveWindow
	(
		gui.hStatusBar,
		0,
		rc.bottom - (rcstatusbar.bottom - rcstatusbar.top + 1),
		rcstatusbar.right - rcstatusbar.left + 1,
		rcstatusbar.bottom - rcstatusbar.top + 1,
		TRUE
	);
	MoveWindow
	(
		gui.hToolBar,
		0,
		0,
		rcToolBar.right - rcToolBar.left + 1,
		rcToolBar.bottom - rcToolBar.top + 1,
		TRUE
	);

//	ShowWindow(gui.hStatusBar, SW_HIDE);
	ShowWindow(gui.hToolBar, SW_SHOW);
	ShowWindow(gui.hStatusBar, SW_SHOW);

	InitStatusBarParts();
	ShowWindow(gui.hStatusBar, guistatus.IsFullScreen || !guioptions.display_statusbar ? SW_HIDE : SW_SHOW);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void InitStatusBarParts(void)
{
	/*~~~~~~~*/
	RECT	rc;
	/*~~~~~~~*/

	if(gui.hStatusBar == NULL)
		return;
	else
	{
		/*~~~~~~~~~*/
		int sizes[6];
		/*~~~~~~~~~*/

		GetWindowRect(gui.hStatusBar, &rc);

		/*
		 * sizes[5] = rc.right-rc.left-25;  
		 * sizes[4] = sizes[5]-40;
		 */
		sizes[4] = rc.right - rc.left - 25;
		sizes[3] = sizes[4] - 15;
		sizes[2] = sizes[3] - 30;
		sizes[1] = sizes[2] - 40;
		sizes[0] = sizes[1] - 60;

		SendMessage(gui.hStatusBar, SB_SETPARTS, 5, (LPARAM) sizes);
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void SetStatusBarText(int fieldno, char *text)
{
	SendMessage(gui.hStatusBar, SB_SETTEXT, fieldno, (LPARAM) text);
}

HMENU			file_submenu;
HMENU			CPU_submenu;
HMENU			recent_rom_directory_submenu;
HMENU			recent_game_submenu;
HMENU			switch_compiler_submenu;
MENUITEMINFO	switch_compiler_submenu_info;
HMENU			dyna_code_checking_submenu;
HMENU			counter_hack_submenu;
HMENU			state_save_submenu;
HMENU			state_load_submenu;
UINT			recent_rom_directory_submenu_pos;
UINT			recent_game_submenu_pos;
UINT			switch_compiler_submenu_pos;
UINT			dyna_code_checking_submenu_pos;
UINT			counter_hack_submenu_pos;
UINT			state_save_submenu_pos;
UINT			state_load_submenu_pos;

MENUITEMINFO	advanced_options_menuitem;
MENUITEMINFO	seperator_menuitem;
UINT			advanced_options_menuitem_pos;
UINT			seperator_menuitem_pos;

UINT			recent_game_menu_ids[MAX_RECENT_GAME_LIST] =
{
	ID_FILE_RECENTGAMES_GAME1,
	ID_FILE_RECENTGAMES_GAME2,
	ID_FILE_RECENTGAMES_GAME3,
	ID_FILE_RECENTGAMES_GAME4,
	ID_FILE_RECENTGAMES_GAME5,
	ID_FILE_RECENTGAMES_GAME6,
	ID_FILE_RECENTGAMES_GAME7,
	ID_FILE_RECENTGAMES_GAME8
};
UINT			recent_rom_directory_menu_ids[MAX_RECENT_ROM_DIR] =
{
	ID_FILE_ROMDIRECTORY1,
	ID_FILE_ROMDIRECTORY2,
	ID_FILE_ROMDIRECTORY3,
	ID_FILE_ROMDIRECTORY4,
	ID_FILE_ROMDIRECTORY5,
	ID_FILE_ROMDIRECTORY6,
	ID_FILE_ROMDIRECTORY7,
	ID_FILE_ROMDIRECTORY8
};

/*
 =======================================================================================================================
 =======================================================================================================================
 */

void ModifyMenuText(UINT menuid, char *newtext)
{
	ModifyMenu(gui.hMenu1964main, menuid, MF_BYCOMMAND, menuid, newtext);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void RegerateRecentGameMenus(void)
{
	if(guioptions.show_recent_rom_directory_list)
	{
		InsertMenu
		(
			file_submenu,
			recent_game_submenu_pos,
			MF_BYPOSITION | MF_POPUP,
			(UINT) recent_game_submenu,
			"Recent Games"
		);
	}
	else
	{
		InsertMenu
		(
			file_submenu,
			recent_rom_directory_submenu_pos,
			MF_BYPOSITION | MF_POPUP,
			(UINT) recent_game_submenu,
			"Recent Games"
		);
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void DeleteRecentGameMenus(void)
{
	/*~~~~~~~~~~~~~~~*/
	int		k, i, j, n;
	char	str[100];
	/*~~~~~~~~~~~~~~~*/

	i = GetMenuItemCount(gui.hMenu1964main);
	for(k = 0; k < i; k++)
	{
		GetMenuString(gui.hMenu1964main, k, str, 80, MF_BYPOSITION);
		if(strcmp(str, "&File") == 0)
		{
			file_submenu = GetSubMenu(gui.hMenu1964main, k);
			j = GetMenuItemCount(file_submenu);
			for(n = j - 1; n >= 0; n--) /* I have to delete the menu in reverse order */
			{
				GetMenuString(file_submenu, n, str, 80, MF_BYPOSITION);
				if(strcmp(str, "Recent Games") == 0)
				{
					recent_game_submenu = GetSubMenu(file_submenu, n);
					recent_game_submenu_pos = n;
					RemoveMenu(file_submenu, n, MF_BYPOSITION);
				}
			}
		}
	}
}

/*
 =======================================================================================================================
    char recent_game_lists[8][260];
 =======================================================================================================================
 */
void RefreshRecentGameMenus(char *newgamefilename)
{
	/*~~*/
	int i;
	/*~~*/

	for(i = 0; i < 8; i++)
	{
		if(strcmp(recent_game_lists[i], newgamefilename) == 0) break;
	}

	if(i != 0)
	{
		if(i == 8) i = 7;	/* if not found */

		/* need to move the most recent file to the 1st position */
		for(; i > 0; i--)
		{
			strcpy(recent_game_lists[i], recent_game_lists[i - 1]);
			ModifyMenuText(recent_game_menu_ids[i], recent_game_lists[i]);
		}

		strcpy(recent_game_lists[0], newgamefilename);
		ModifyMenuText(recent_game_menu_ids[0], newgamefilename);
	}

	return;
}

/*
 =======================================================================================================================
    char recent_rom_directory_lists[MAX_RECENT_ROM_DIR][260];
 =======================================================================================================================
 */
void RefreshRecentRomDirectoryMenus(char *newromdirectory)
{
	/*~~*/
	int i;
	/*~~*/

	for(i = 0; i < MAX_RECENT_ROM_DIR; i++)
	{
		if(strcmp(recent_rom_directory_lists[i], newromdirectory) == 0) break;
	}

	if(i != 0)
	{
		if(i == MAX_RECENT_ROM_DIR) i = MAX_RECENT_ROM_DIR - 1; /* if not found */

		/* need to move the most recent file to the 1st position */
		for(; i > 0; i--)
		{
			strcpy(recent_rom_directory_lists[i], recent_rom_directory_lists[i - 1]);
			ModifyMenuText(recent_rom_directory_menu_ids[i], recent_rom_directory_lists[i]);
		}

		strcpy(recent_rom_directory_lists[0], newromdirectory);
		ModifyMenuText(recent_rom_directory_menu_ids[0], newromdirectory);
	}

	return;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void RegerateRecentRomDirectoryMenus(void)
{
	InsertMenu
	(
		file_submenu,
		recent_rom_directory_submenu_pos,
		MF_BYPOSITION | MF_POPUP,
		(UINT) recent_rom_directory_submenu,
		"Recent ROM Folders"
	);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void DeleteRecentRomDirectoryMenus(void)
{
	/*~~~~~~~~~~~~~~~*/
	int		k, i, j, n;
	char	str[100];
	/*~~~~~~~~~~~~~~~*/

	i = GetMenuItemCount(gui.hMenu1964main);
	for(k = 0; k < i; k++)
	{
		GetMenuString(gui.hMenu1964main, k, str, 80, MF_BYPOSITION);
		if(strcmp(str, "&File") == 0)
		{
			file_submenu = GetSubMenu(gui.hMenu1964main, k);
			j = GetMenuItemCount(file_submenu);
			for(n = j - 1; n >= 0; n--) /* I have to delete the menu in reverse order */
			{
				GetMenuString(file_submenu, n, str, 80, MF_BYPOSITION);
				if(strcmp(str, "Recent ROM Folders") == 0)
				{
					recent_rom_directory_submenu = GetSubMenu(file_submenu, n);
					recent_rom_directory_submenu_pos = n;
					RemoveMenu(file_submenu, n, MF_BYPOSITION);
				}
			}
		}
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void RegenerateStateSelectorMenus(void)
{
//	if(guioptions.show_expert_user_menu)
	{
		InsertMenu
		(
			CPU_submenu,
			state_save_submenu_pos,
			MF_BYPOSITION | MF_POPUP,
			(UINT) state_save_submenu,
			"Save State\tF5"
		);
		InsertMenu
		(
			CPU_submenu,
			state_save_submenu_pos,
			MF_BYPOSITION | MF_POPUP,
			(UINT) state_load_submenu,
			"Load State\tF7"
		);
	}
//	else
//	{
//		AppendMenu(CPU_submenu, MF_POPUP, (UINT) state_save_submenu, "Save State\tF5");
//		AppendMenu(CPU_submenu, MF_POPUP, (UINT) state_load_submenu, "Load State\tF7");
//	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void DeleteStateSelectorMenus(void)
{
	/*~~~~~~~~~~~~~~~*/
	int		k, i, j, n;
	char	str[100];
	/*~~~~~~~~~~~~~~~*/

	i = GetMenuItemCount(gui.hMenu1964main);
	for(k = 0; k < i; k++)
	{
		GetMenuString(gui.hMenu1964main, k, str, 80, MF_BYPOSITION);
		if(strcmp(str, "CPU") == 0)
		{
			CPU_submenu = GetSubMenu(gui.hMenu1964main, k);
			j = GetMenuItemCount(CPU_submenu);
			for(n = j - 1; n >= 0; n--) /* I have to delete the menu in reverse order */
			{
				GetMenuString(CPU_submenu, n, str, 80, MF_BYPOSITION);

				/* Delete all cpu core switch menus */
				if(strcmp(str, "Load State\tF7") == 0)
				{
					state_load_submenu = GetSubMenu(CPU_submenu, n);
					state_load_submenu_pos = n;
					RemoveMenu(CPU_submenu, n, MF_BYPOSITION);
				}
				else if(strcmp(str, "Save State\tF5") == 0)
				{
					state_save_submenu = GetSubMenu(CPU_submenu, n);
					state_save_submenu_pos = n;
					RemoveMenu(CPU_submenu, n, MF_BYPOSITION);
				}
			}
		}
	}
}

int Separator = 0;
/*
 =======================================================================================================================
 =======================================================================================================================
 */
void RegenerateAdvancedUserMenus(void)
{
	AppendMenu(CPU_submenu, MF_POPUP, (UINT) switch_compiler_submenu, "Switch Compiler");
	AppendMenu(CPU_submenu, MF_POPUP, (UINT) dyna_code_checking_submenu, "Self-Modifying Code Detection Method");
	AppendMenu(CPU_submenu, MF_POPUP, (UINT) counter_hack_submenu, "Counter Factor");
#ifdef DEBUG_COMMON
	InsertMenu(file_submenu, ID_PREFERENCE_OPTIONS, MF_UNCHECKED, ID_DEFAULTOPTIONS, "Advanced Options ...");
#endif
	InsertMenu(CPU_submenu, ID_SAVESTATE, MF_GRAYED, ID_CPU_IMPORTPJ64STATE, "Import Project64 Save State...");
	InsertMenu(CPU_submenu, ID_SAVESTATE, MF_GRAYED, ID_CPU_EXPORTPJ64STATE, "Export Project64 Save State...");
}


/*
 =======================================================================================================================
 =======================================================================================================================
 */
void DeleteAdvancedUserMenus(void)
{
	/*~~~~~~~~~~~~~~~*/
	int		k, i, j, n;
	char	str[100];
	/*~~~~~~~~~~~~~~~*/

	i = GetMenuItemCount(gui.hMenu1964main);
	for(k = 0; k < i; k++)
	{
		GetMenuString(gui.hMenu1964main, k, str, 80, MF_BYPOSITION);
		if(strcmp(str, "CPU") == 0)
		{
			CPU_submenu = GetSubMenu(gui.hMenu1964main, k);
			j = GetMenuItemCount(CPU_submenu);
			for(n = j - 1; n >= 0; n--) /* I have to delete the menu in reverse order */
			{
				GetMenuString(CPU_submenu, n, str, 80, MF_BYPOSITION);

				/* Delete all cpu core switch menus */
				if(strcmp(str, "Switch Compiler") == 0)
				{
					switch_compiler_submenu = GetSubMenu(CPU_submenu, n);
					GetMenuItemInfo(CPU_submenu, n, MF_BYPOSITION, &switch_compiler_submenu_info);
					switch_compiler_submenu_pos = n;
					RemoveMenu(CPU_submenu, n, MF_BYPOSITION);
				}

				/* Delete all code check method switch menus */
				else if(strcmp(str, "Self-Modifying Code Detection Method") == 0)
				{
					dyna_code_checking_submenu = GetSubMenu(CPU_submenu, n);
					dyna_code_checking_submenu_pos = n;
					RemoveMenu(CPU_submenu, n, MF_BYPOSITION);
				}

				/* Delete all Counter Hack menus */
				else if(strcmp(str, "Counter Factor") == 0)
				{
					counter_hack_submenu = GetSubMenu(CPU_submenu, n);
					counter_hack_submenu_pos = n;
					RemoveMenu(CPU_submenu, n, MF_BYPOSITION);
				}

				/*
				 * else if( strnicmp(str+2,"port Project64",14 )==0 )  
				 * {  
				 * RemoveMenu(CPU_submenu, n, MF_BYPOSITION);  
				 * }
				 */
			}
		}
	}

	GetMenuItemInfo(gui.hMenu1964main, ID_DEFAULTOPTIONS, MF_BYCOMMAND, &advanced_options_menuitem);
	RemoveMenu(gui.hMenu1964main, ID_DEFAULTOPTIONS, MF_BYCOMMAND);
	RemoveMenu(gui.hMenu1964main, ID_CPU_IMPORTPJ64STATE, MF_BYCOMMAND);
	RemoveMenu(gui.hMenu1964main, ID_CPU_EXPORTPJ64STATE, MF_BYCOMMAND);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void SetupAdvancedMenus(void)
{
	/*~~*/
	int i;
	/*~~*/

	for(i = 0; i < MAX_RECENT_ROM_DIR; i++)
	{
		ModifyMenuText(recent_rom_directory_menu_ids[i], recent_rom_directory_lists[i]);
	}

	for(i = 0; i < MAX_RECENT_GAME_LIST; i++)
	{
		ModifyMenuText(recent_game_menu_ids[i], recent_game_lists[i]);
	}

	if(guioptions.show_expert_user_menu == FALSE) DeleteAdvancedUserMenus();
#ifndef DEBUG_COMMON
	else
	{	/* hide the default option menu from RELEASE MODE */
		RemoveMenu(gui.hMenu1964main, ID_DEFAULTOPTIONS, MF_BYCOMMAND);
	}
#endif
	if(guioptions.show_recent_rom_directory_list == FALSE) DeleteRecentRomDirectoryMenus();
	if(guioptions.show_recent_game_list == FALSE) DeleteRecentGameMenus();
	if(guioptions.show_state_selector_menu == FALSE) DeleteStateSelectorMenus();
	if(!emuoptions.SyncVI) CheckMenuItem(gui.hMenu1964main, ID_CPU_AUDIOSYNC, MF_UNCHECKED);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void CaptureScreenToFile(void)
{
	if(emustatus.Emu_Is_Running)
	{
		if(GfxPluginVersion != 0x0103)
			DisplayError("Current video plugin does not support screen capture");
		else
		{
			/*~~~~~~~~~~~~~~~~~~~~~~~~~*/
			char	directory[_MAX_PATH];
			/*~~~~~~~~~~~~~~~~~~~~~~~~~*/

			strcpy(directory, directories.main_directory);
			strcat(directory, "screens\\");
			VIDEO_CaptureScreen(directory);
		}
	}
}

static BOOL exiting_1964 = FALSE;

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void Exit1964(void)
{
	/*~~~~~~~~~~~~~~~~~~~~~~*/
	WINDOWPLACEMENT placement;
	/*~~~~~~~~~~~~~~~~~~~~~~*/

	if(exiting_1964) exit(0);

	exiting_1964 = TRUE;

	SetStatusBarText(0, "Exiting 1964");

	if(emustatus.Emu_Is_Running) Stop();

	placement.length = sizeof(WINDOWPLACEMENT);
	GetWindowPlacement(gui.hwnd1964main, &placement);
	//guistatus.clientwidth = placement.rcNormalPosition.right - placement.rcNormalPosition.left;
	//guistatus.clientheight = placement.rcNormalPosition.bottom - placement.rcNormalPosition.top;
	guistatus.window_position.left = placement.rcNormalPosition.left;
	guistatus.window_position.top = placement.rcNormalPosition.top;
	guistatus.WindowIsMaximized = (placement.showCmd == SW_SHOWMAXIMIZED);

	RomListRememberColumnWidth();

	Close_iPIF();			/* save mempak and eeprom */

	FreeVirtualMemory();

	FileIO_Write1964Ini();	/* Save 1964.ini */
	WriteConfiguration();
	DeleteAllIniEntries();	/* Release all ini entries */
	ClearRomList();			/* Clean the Rom List */

	FreePlugins();

	/*
	 * Here is the fix for the problem that 1964 crash when exiting if using opengl
	 * plugins.  
	 * I don't know why 1964 crash, looks like crash is not happen in 1964, but dll
	 * related.  
	 * just doing exit(0) will not crash,(maybe we have left some resource not
	 * released, donno)
	 */
	exit(0);

	/* PostQuitMessage(0); */
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void Set_Ready_Message(void)
{
	if(Rom_Loaded)
	{
		if(strlen(currentromoptions.Alt_Title) < 1 || strcmp(rominfo.name, currentromoptions.Game_Name) != 0)
			sprintf(generalmessage, "Ready - %s - [%s]", directories.rom_directory_to_use, rominfo.name);
		else
		{
			sprintf(generalmessage, "Ready - %s - [%s]", directories.rom_directory_to_use, currentromoptions.Alt_Title);
		}
	}
	else
	{
		sprintf(generalmessage, "Ready - %s", directories.rom_directory_to_use);
	}

	SetStatusBarText(0, generalmessage);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void DisableDebugMenu(void)
{
	/*~~~~~~~~~~~~*/
	/* Disable the DEBUG menu */
	int		i, k;
	char	str[80];
	/*~~~~~~~~~~~~*/

	i = GetMenuItemCount(gui.hMenu1964main);
	for(k = 0; k < i; k++)
	{
		GetMenuString(gui.hMenu1964main, k, str, 80, MF_BYPOSITION);
		if(strcmp(str, "Debug") == 0)
		{
			DeleteMenu(gui.hMenu1964main, k, MF_BYPOSITION);
		}
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void SetupDebuger(void)
{
#ifdef DEBUG_COMMON
	CheckMenuItem(gui.hMenu1964main, ID_DEBUG_CONTROLLER, debugoptions.debug_si_controller ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(gui.hMenu1964main, ID_DEBUGSPTASK, debugoptions.debug_sp_task ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(gui.hMenu1964main, ID_DEBUGSITASK, debugoptions.debug_si_task ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(gui.hMenu1964main, ID_DEBUGSPDMA, debugoptions.debug_sp_dma ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(gui.hMenu1964main, ID_DEBUGSIDMA, debugoptions.debug_si_dma ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(gui.hMenu1964main, ID_DEBUGPIDMA, debugoptions.debug_pi_dma ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(gui.hMenu1964main, ID_DEBUGMEMPAK, debugoptions.debug_si_mempak ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(gui.hMenu1964main, ID_DEBUGTLB, debugoptions.debug_tlb ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(gui.hMenu1964main, ID_DEBUGEEPROM, debugoptions.debug_si_eeprom ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(gui.hMenu1964main, ID_DEBUG_SRAM, debugoptions.debug_sram ? MF_CHECKED : MF_UNCHECKED);

	DebuggerBreakPointActive = FALSE;
	OpCount = 0;
	NextClearCode = 250;
	BreakAddress = -1;
	DebuggerActive = FALSE;
	OpenDebugger();
#else
	DisableDebugMenu();
#endif
}

long OnNotifyStatusBar(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if( ((LPNMHDR) lParam)->code == NM_DBLCLK )
	{
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
		int fieldno = ((LPNMLISTVIEW) lParam)->iItem;
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
		
		switch(fieldno)
		{
		case 2:						/* Counter Factor */
			/* Reset Counter Factor to default value */
			SendMessage(gui.hwnd1964main, WM_COMMAND, ID_CF_CF1, 0);
			break;
		case 4:						/* CPU core */
			/* Switch CPU core */
			if(emustatus.Emu_Is_Running)
			{
				SendMessage
					(
					gui.hwnd1964main,
					WM_COMMAND,
					emustatus.cpucore == DYNACOMPILER ? ID_INTERPRETER : ID_DYNAMICCOMPILER,
					0
					);
			}
			else
			{
				SendMessage
					(
					gui.hwnd1964main,
					WM_COMMAND,
					defaultoptions.Emulator == DYNACOMPILER ? ID_INTERPRETER : ID_DYNAMICCOMPILER,
					0
					);
			}
			break;
		}
	}

	return 0l;
}

long OnPopupMenuCommand(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(LOWORD(wParam))
	{
	case ID_POPUP_LOADPLAY:
		if( Rom_Loaded )
		{
			Play(emuoptions.auto_full_screen);
		}
		else
		{
			RomListOpenRom(RomListGetSelectedIndex(), TRUE);
		}
		break;
	case ID_POPUP_LOADPLAYINFULLSCREEN:
		RomListOpenRom(RomListGetSelectedIndex(), FALSE);
		Play(TRUE);
		break;
	case ID_POPUP_LOADPLAYINWINDOWMODE:
		RomListOpenRom(RomListGetSelectedIndex(), FALSE);
		Play(FALSE);
		break;
	case ID_POPUP_ROM_SETTING:
		RomListRomOptions(selected_rom_index);
		break;
	case ID_POPUP_CHEATCODE:
		if(emustatus.Emu_Is_Running)
		{
			PauseEmulator();
			//SuspendThread(CPUThreadHandle);
			DialogBox(gui.hInst, "CHEAT_HACK", hWnd, (DLGPROC) CheatAndHackDialog);
			//ResumeThread(CPUThreadHandle);
			ResumeEmulator(DO_NOTHING_AFTER_PAUSE);
		}
		else
		{
			CodeList_ReadCode(romlist[selected_rom_index]->pinientry->Game_Name);
			DialogBox(gui.hInst, "CHEAT_HACK", hWnd, (DLGPROC) CheatAndHackDialog);
		}
		break;
	case ID_HEADERPOPUP_SHOW_INTERNAL_NAME:
		romlistNameToDisplay = ROMLIST_DISPLAY_INTERNAL_NAME;
		SendMessage(gui.hwnd1964main, WM_COMMAND, ID_FILE_FRESHROMLIST, 0);
		break;
	case ID_HEADERPOPUP_SHOWALTERNATEROMNAME:
		romlistNameToDisplay = ROMLIST_DISPLAY_ALTER_NAME;
		SendMessage(gui.hwnd1964main, WM_COMMAND, ID_FILE_FRESHROMLIST, 0);
		break;
	case ID_HEADERPOPUP_SHOWROMFILENAME:
		romlistNameToDisplay = ROMLIST_DISPLAY_FILENAME;
		SendMessage(gui.hwnd1964main, WM_COMMAND, ID_FILE_FRESHROMLIST, 0);
		break;
	case ID_HEADERPOPUP_1_SORT_ASCENDING:
		romlist_sort_method = 0;
		NewRomList_Sort();
		NewRomList_ListViewFreshRomList();
		break;
	case ID_HEADERPOPUP_1_SORT_DESCENDING:
		romlist_sort_method = 4;
		NewRomList_Sort();
		NewRomList_ListViewFreshRomList();
		break;
	case ID_HEADERPOPUP_2_SORT_ASCENDING:
		romlist_sort_method = romListHeaderClickedColumn;
		NewRomList_Sort();
		NewRomList_ListViewFreshRomList();
		break;
	case ID_HEADERPOPUP_2_SORT_DESCENDING:
		romlist_sort_method = romListHeaderClickedColumn+4;
		NewRomList_Sort();
		NewRomList_ListViewFreshRomList();
		break;
	case ID_HEADERPOPUP_1_SELECTING:
	case ID_HEADERPOPUP_2_SELECTING:
		DialogBox(gui.hInst, "COL_SELECT", hWnd, (DLGPROC) ColumnSelectDialog);
		break;
	}

	return 0l;

}

long OnOpcodeDebuggerCommands(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(LOWORD(wParam))
	{
		case ID_OPCODEDEBUGGER:
            debug_opcode = 1 - debug_opcode;
			if(debug_opcode!=0)
				CheckMenuItem(gui.hMenu1964main, ID_OPCODEDEBUGGER, MF_CHECKED);
			else
				CheckMenuItem(gui.hMenu1964main, ID_OPCODEDEBUGGER, MF_UNCHECKED);

			if(opcode_debugger_memory_is_allocated == FALSE)
			{
				/* now we allocate the allocate the debugger */
				InitVirtualMemory1(&gMemoryState_Interpreter_Compare);
				InitMemoryLookupTables();
				gMemoryState_Interpreter_Compare.ROM_Image = gMemoryState.ROM_Image;

				/*
				 * TODO: here we need to copy the memorystate and hardware state to  
				 * interpreter_compare. Actually, it is probably better to just use the same
				 * method  
				 * you use when switching from interpreter to dyna, because a few  
				 * other things need to be initialized, like rdram size pointers for  
				 * interpreter_compare.
				 */
				opcode_debugger_memory_is_allocated = TRUE;
				TRACE0("Allocate memory for opcode debugger");
			}

			if(emustatus.Emu_Is_Running)
			{
				if(PauseEmulator())
				{
					Debugger_Copy_Memory(&gMemoryState_Interpreter_Compare, &gMemoryState);
					memcpy(&gHardwareState_Interpreter_Compare, &gHardwareState, sizeof(HardwareState));
					ResumeEmulator(REFRESH_DYNA_AFTER_PAUSE);	/* Need to init emu */
				}
			}
			break;
		case ID_OPCODEDEBUGGER_BLOCK_ONLY:
			debug_opcode_block = 1 - debug_opcode_block;
			if(debug_opcode_block)
				CheckMenuItem(gui.hMenu1964main, ID_OPCODEDEBUGGER_BLOCK_ONLY, MF_CHECKED);
			else
				CheckMenuItem(gui.hMenu1964main, ID_OPCODEDEBUGGER_BLOCK_ONLY, MF_UNCHECKED);
			if(emustatus.Emu_Is_Running)
			{
				if(debug_opcode != 1 && debug_opcode_block)
				{
                    debug_opcode = 1;
					CheckMenuItem(gui.hMenu1964main, ID_OPCODEDEBUGGER, MF_CHECKED);
				}

				if(PauseEmulator())
				{
					Debugger_Copy_Memory(&gMemoryState_Interpreter_Compare, &gMemoryState);
					memcpy(&gHardwareState_Interpreter_Compare, &gHardwareState, sizeof(HardwareState));
					ResumeEmulator(REFRESH_DYNA_AFTER_PAUSE);	/* Need to init emu */
				}
			}
			break;
	}

	return 0l;
}

void OnFreshRomList()
{
	if( !emustatus.Emu_Is_Running )
	{
		NewRomList_ListViewChangeWindowRect();
		DockStatusBar();
		ClearRomList();
		NewRomList_ListViewFreshRomList();
		SetStatusBarText(0, "Looking for ROM file in the ROM directory and Generating List");
		RomListReadDirectory(directories.rom_directory_to_use);
		NewRomList_ListViewFreshRomList();
		Set_Ready_Message();
	}
}

