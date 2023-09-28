/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// vid_wgl.c -- Windows NT OpenGL video driver

#include "quakedef.h"
#include "winquake.h"
#include "resource.h"

#define MAX_MODE_LIST		600 // was 30

#define MAXWIDTH		8192 // was 10000
#define MAXHEIGHT		8192 // was 10000

#define MODE_WINDOWED			0
#define NO_MODE					(MODE_WINDOWED - 1)
#define MODE_FULLSCREEN_DEFAULT	(MODE_WINDOWED + 1)

typedef struct {
	modestate_t	type;
	int			width;
	int			height;
	int			modenum;
	int			dib;
	int			fullscreen;
	int			bpp;
	int			refreshrate;
	int			halfscreen;
	char		modedesc[17];
} vmode_t;

typedef struct {
	int			width;
	int			height;
} lmode_t;

lmode_t	lowresmodes[] = {
	{320, 200},
	{320, 240},
	{400, 300},
	{512, 384},
};

static vmode_t	modelist[MAX_MODE_LIST];
static int		nummodes;
static vmode_t	badmode;

static DEVMODE	gdevmode;
static qboolean	vid_initialized = false;
static qboolean	windowed, leavecurrentmode;
static qboolean vid_canalttab = false;
static qboolean vid_wassuspended = false;
extern qboolean	mouseactive;  // from in_win.c
extern qboolean	dinput;  // from in_win.c

static HICON	hIcon;

DWORD		WindowStyle, ExWindowStyle;

HWND	mainwindow;

int			vid_modenum = NO_MODE;
int			vid_realmode;
int			vid_default = MODE_WINDOWED;

int			desktop_bpp; // query this during startup
int			desktop_refreshrate; // query this during startup

HGLRC	baseRC;
HDC		maindc;

BOOL bSetupPixelFormat(HDC hDC)
{
    static PIXELFORMATDESCRIPTOR pfd = 
    {
		sizeof(PIXELFORMATDESCRIPTOR),	// size of this pfd
		1,								// version number
		PFD_DRAW_TO_WINDOW |			// support window
		PFD_SUPPORT_OPENGL |			// support OpenGL
		PFD_DOUBLEBUFFER,				// double buffered
		PFD_TYPE_RGBA,					// RGBA type
		32,								// 32-bit color depth
		0, 0, 0, 0, 0, 0,				// color bits ignored
		8,								// 8 bit destination alpha
		0,								// shift bit ignored
		0,								// no accumulation buffer
		0, 0, 0, 0, 					// accum bits ignored
		24,								// 24-bit z-buffer	
		8,								// 8-bit stencil buffer
		0,								// no auxiliary buffer
		PFD_MAIN_PLANE,					// main layer
		0,								// reserved
		0, 0, 0							// layer masks ignored
    };
    
    int pixelformat;

    if ( (pixelformat = ChoosePixelFormat(hDC, &pfd)) == 0 )
    {
        MessageBox(NULL, "ChoosePixelFormat failed", "Error", MB_OK);
        return FALSE;
    }

    if (SetPixelFormat(hDC, pixelformat, &pfd) == FALSE)
    {
        MessageBox(NULL, "SetPixelFormat failed", "Error", MB_OK);
        return FALSE;
    }

    return TRUE;
}

viddef_t vid; // global video state

modestate_t modestate = MS_UNINIT;

LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LONG CDAudio_MessageHandler (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
char *VID_GetExtModeDescription (int mode);
void VID_UpdateWindowStatus (void);

int			window_center_x, window_center_y, window_x, window_y, window_width, window_height;
RECT		window_rect;

//==========================================================================
//
//  HARDWARE GAMMA
//
//==========================================================================

cvar_t		vid_gamma = {"gamma", "1", true}; // moved here from view.c
cvar_t		vid_contrast = {"contrast", "1", true}; // QuakeSpasm, MarkV

unsigned short vid_gammaramp[768];
unsigned short vid_systemgammaramp[768]; // to restore gamma on exit
qboolean vid_gammaworks = false;

/*
================
VID_Gamma_Set

apply gamma correction
================
*/
void VID_Gamma_Set (void)
{
	if (!vid_gammaworks)
		return;

	if (!SetDeviceGammaRamp (maindc, vid_gammaramp))
		Con_Printf ("VID_Gamma_Set: failed on SetDeviceGammaRamp\n");
/*
	FIXME: for some reason, this always returns false.

	which is supposed to return true or false depending on if the call succeeds,
	but I've found on my machine that the function call always returns false,
	regardless of the function working or not.
*/
}

/*
================
VID_Gamma_Restore

restore system gamma
================
*/
void VID_Gamma_Restore (void)
{
	if (!vid_gammaworks)
		return;

	if (!SetDeviceGammaRamp (maindc, vid_systemgammaramp))
		Con_Printf ("VID_Gamma_Restore: failed on SetDeviceGammaRamp\n");
}

/*
================
VID_Gamma_Shutdown

called on exit
================
*/
void VID_Gamma_Shutdown (void)
{
	VID_Gamma_Restore ();
}

/*
================
VID_Gamma

callback when the cvar changes
================
*/
void VID_Gamma (void)
{
	int i;
	static float oldgamma;
	static float oldcontrast;

	if (vid_gamma.value == oldgamma && vid_contrast.value == oldcontrast)
		return;

	oldgamma = vid_gamma.value;
    oldcontrast = vid_contrast.value;

	// Refresh gamma
	for (i=0; i<256; i++)
		vid_gammaramp[i] = vid_gammaramp[i+256] = vid_gammaramp[i+512] =
			CLAMP(0, (int)((255 * pow ((i+0.5)/255.5, vid_gamma.value) + 0.5) * vid_contrast.value), 255) << 8;

	VID_Gamma_Set ();
}

/*
================
VID_Gamma_Init

call on init
================
*/
void VID_Gamma_Init (void)
{
    qboolean success = GetDeviceGammaRamp (maindc, vid_systemgammaramp);
    if (!success)
        Con_Printf ("VID_Gamma_Init: failed on GetDeviceGammaRamp\n");
    else
        vid_gammaworks = true;

	if (!vid_gammaworks)
		Con_Printf ("Hardware gamma unavailable\n");
}

//==============================================================================
//
//	VIDEO
//
//==============================================================================

/*
================
CenterWindow
================
*/
void CenterWindow (HWND window)
{
	RECT area, rect;
	int x, y;

	// get the size of the desktop working area and the window
	if (SystemParametersInfo (SPI_GETWORKAREA, 0, &area, 0) && GetWindowRect (window, &rect))
	{
		// center window properly in the working area
		x = area.left + ((area.right - area.left) - (rect.right - rect.left)) / 2;
		y = area.top  + ((area.bottom - area.top) - (rect.bottom - rect.top)) / 2;
		if (x < 0) x = 0;
		if (y < 0) y = 0;

		SetWindowPos (window, NULL, x, y, 0, 0,
				SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);
	}
}

/*
================
VID_SetWindowedMode
================
*/
qboolean VID_SetWindowedMode (int modenum)
{
	HDC				hdc;
	int				width, height;
	RECT			rect;

	rect.top = rect.left = 0;

	rect.right = modelist[modenum].width;
	rect.bottom = modelist[modenum].height;

	window_width = modelist[modenum].width;
	window_height = modelist[modenum].height;

	// if width and height match desktop size, make the window without titlebar/borders
	if (window_width == GetSystemMetrics(SM_CXSCREEN) && window_height == GetSystemMetrics(SM_CYSCREEN))
		WindowStyle = WS_POPUP; // Window covers entire screen: no caption, borders etc
	else
		WindowStyle = WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

	ExWindowStyle = 0;

	AdjustWindowRectEx(&rect, WindowStyle, FALSE, 0);

	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	// Create the main window
	mainwindow = CreateWindowEx (
		ExWindowStyle,
		"fxQuake",
		"fxQuake",
		WindowStyle,
		rect.left, rect.top,
		width,
		height,
		NULL,
		NULL,
		global_hInstance,
		NULL
	);

	if (!mainwindow)
		Sys_Error ("Couldn't create main window");

	// Center and show the main window
	CenterWindow (mainwindow);

	ShowWindow (mainwindow, SW_SHOWDEFAULT);
	UpdateWindow (mainwindow);

	modestate = MS_WINDOWED;

// because we have set the background brush for the window to NULL
// (to avoid flickering when re-sizing the window on the desktop),
// we clear the window to black when created, otherwise it will be
// empty while Quake starts up.
	hdc = GetDC(mainwindow);
	PatBlt(hdc,0,0,rect.right,rect.bottom,BLACKNESS);
	ReleaseDC(mainwindow, hdc);

	vid.width = modelist[modenum].width;
	vid.height = modelist[modenum].height;
	vid.conwidth = vid.width & 0xFFFFFFF8;
	vid.conheight = vid.conwidth * vid.height / vid.width;

	vid.numpages = 2;

	SendMessage (mainwindow, WM_SETICON, (WPARAM)TRUE, (LPARAM)hIcon);
	SendMessage (mainwindow, WM_SETICON, (WPARAM)FALSE, (LPARAM)hIcon);

	return true;
}

/*
================
VID_SetFullScreenMode
================
*/
qboolean VID_SetFullScreenMode (int modenum)
{
	HDC				hdc;
	int				width, height;
	RECT			rect;

	if (!leavecurrentmode)
	{
		gdevmode.dmFields = DM_BITSPERPEL | 
							DM_PELSWIDTH |
							DM_PELSHEIGHT |
							DM_DISPLAYFREQUENCY; // refreshrate
		gdevmode.dmBitsPerPel = modelist[modenum].bpp;
		gdevmode.dmPelsWidth = modelist[modenum].width << modelist[modenum].halfscreen;
		gdevmode.dmPelsHeight = modelist[modenum].height;
		gdevmode.dmDisplayFrequency = modelist[modenum].refreshrate; // refreshrate
		gdevmode.dmSize = sizeof (gdevmode);

		if (ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
			Sys_Error ("Couldn't set fullscreen mode");
	}

	modestate = MS_FULLSCREEN;

	rect.top = rect.left = 0;

	rect.right = modelist[modenum].width;
	rect.bottom = modelist[modenum].height;

	window_width = modelist[modenum].width;
	window_height = modelist[modenum].height;

	WindowStyle = WS_POPUP;
	ExWindowStyle = 0;

	AdjustWindowRectEx(&rect, WindowStyle, FALSE, 0);

	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	// Create the main window
	mainwindow = CreateWindowEx (
		ExWindowStyle,
		"fxQuake",
		"fxQuake",
		WindowStyle,
		rect.left, rect.top,
		width,
		height,
		NULL,
		NULL,
		global_hInstance,
		NULL
	);

	if (!mainwindow)
		Sys_Error ("Couldn't create main window");

	// Show the main window
	ShowWindow (mainwindow, SW_SHOWDEFAULT);
	UpdateWindow (mainwindow);

	// Because we have set the background brush for the window to NULL
	// (to avoid flickering when re-sizing the window on the desktop), we
	// clear the window to black when created, otherwise it will be
	// empty while Quake starts up.
	hdc = GetDC(mainwindow);
	PatBlt(hdc,0,0,rect.right,rect.bottom,BLACKNESS);
	ReleaseDC(mainwindow, hdc);

	vid.width = modelist[modenum].width;
	vid.height = modelist[modenum].height;
	vid.conwidth = vid.width & 0xFFFFFFF8;
	vid.conheight = vid.conwidth * vid.height / vid.width;

	vid.numpages = 2;

// needed because we're not getting WM_MOVE messages fullscreen on NT
	window_x = 0;
	window_y = 0;

	SendMessage (mainwindow, WM_SETICON, (WPARAM)TRUE, (LPARAM)hIcon);
	SendMessage (mainwindow, WM_SETICON, (WPARAM)FALSE, (LPARAM)hIcon);

	return true;
}

/*
================
VID_SetMode
================
*/
int VID_SetMode (int modenum)
{
	int			temp;
	qboolean		stat;
	MSG			msg;

	if ((windowed && (modenum != 0)) ||
		(!windowed && (modenum < 1)) ||
		(!windowed && (modenum >= nummodes)))
	{
		Sys_Error ("Bad video mode");
	}

// so Con_Printfs don't mess us up by forcing vid and snd updates
	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

	CDAudio_Pause ();

	// Set either the fullscreen or windowed mode
	stat = false; 
	if (modelist[modenum].type == MS_WINDOWED)
	{
		if (key_dest == key_game)
//		if (key_dest != key_console)
		{
			stat = VID_SetWindowedMode(modenum);
			IN_ActivateMouse ();
			IN_HideMouse ();
		}
		else
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
			stat = VID_SetWindowedMode(modenum);
		}
	}
	else if (modelist[modenum].type == MS_FULLSCREEN)
	{
		stat = VID_SetFullScreenMode(modenum);
		IN_ActivateMouse ();
		IN_HideMouse ();
	}
	else
	{
		Sys_Error ("VID_SetMode: Bad mode type in modelist");
	}

	VID_UpdateWindowStatus ();

	CDAudio_Resume ();
	scr_disabled_for_loading = temp;

	if (!stat)
	{
		Sys_Error ("Couldn't set video mode");
	}

// now we try to make sure we get the focus on the mode switch, because
// sometimes in some systems we don't.  We grab the foreground, then
// finish setting up, pump all our messages, and sleep for a little while
// to let messages finish bouncing around the system, then we put
// ourselves at the top of the z order, then grab the foreground again,
// Who knows if it helps, but it probably doesn't hurt
	SetForegroundWindow (mainwindow);
	vid_modenum = modenum;

	while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage (&msg);
		DispatchMessage (&msg);
	}

	Sleep (100);

	SetWindowPos (mainwindow, HWND_TOP, 0, 0, 0, 0,
				  SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW |
				  SWP_NOCOPYBITS);

	SetForegroundWindow (mainwindow);

// fix the leftover Alt from any Alt-Tab or the like that switched us away
	Key_ClearStates ();
	IN_ClearStates ();

	Con_SafePrintf ("Video mode %s initialized\n", VID_GetExtModeDescription (vid_modenum));

	vid.recalc_refdef = true;

	return true;
}


/*
================
VID_UpdateWindowStatus
================
*/
void VID_UpdateWindowStatus (void)
{
	window_rect.left = window_x;
	window_rect.top = window_y;
	window_rect.right = window_x + window_width;
	window_rect.bottom = window_y + window_height;
	window_center_x = (window_rect.left + window_rect.right) / 2;
	window_center_y = (window_rect.top + window_rect.bottom) / 2;

	IN_UpdateClipCursor ();
}


//====================================


/*
=================
GL_BeginRendering

sets values of glx, gly, glwidth, glheight
=================
*/
void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
	*x = *y = 0;

	*width = vid.width;
	*height = vid.height;
}

/*
=================
GL_EndRendering
=================
*/
void GL_EndRendering (void)
{
	// swap on each frame
	SwapBuffers(maindc);

	if (fullsbardraw)
		Sbar_Changed();
}


//====================================


/*
================
VID_Shutdown
================
*/
void VID_Shutdown (void)
{
	HGLRC hRC;
	HDC hDC;

	if (vid_initialized)
	{
		vid_canalttab = false;
		hRC = wglGetCurrentContext();
		hDC = wglGetCurrentDC();

		wglMakeCurrent(NULL, NULL);

		if (hRC)
			wglDeleteContext(hRC);

		VID_Gamma_Shutdown ();

		if (hDC && mainwindow)
			ReleaseDC(mainwindow, hDC);

		if (modestate == MS_FULLSCREEN)
			ChangeDisplaySettings (NULL, 0);

		if (maindc && mainwindow)
			ReleaseDC (mainwindow, maindc);
	}
}


//==========================================================================


static const byte scantokey[128] = 
{
//  0               1               2               3               4               5               6               7
//  8               9               A               B               C               D               E               F
    0,              K_ESCAPE,       '1',            '2',            '3',            '4',            '5',            '6',
    '7',            '8',            '9',            '0',            '-',            '=',            K_BACKSPACE,    K_TAB,      // 0
    'q',            'w',            'e',            'r',            't',            'y',            'u',            'i',
    'o',            'p',            '[',            ']',            K_ENTER,        K_CTRL,         'a',            's',        // 1
    'd',            'f',            'g',            'h',            'j',            'k',            'l',            ';',
    '\'',           '`',            K_SHIFT,        '\\',           'z',            'x',            'c',            'v',        // 2
    'b',            'n',            'm',            ',',            '.',            '/',            K_SHIFT,        '*',
    K_ALT,          ' ',            K_CAPSLOCK,     K_F1,           K_F2,           K_F3,           K_F4,           K_F5,       // 3
    K_F6,           K_F7,           K_F8,           K_F9,           K_F10,          K_PAUSE,        0,              K_HOME,
    K_UPARROW,      K_PGUP,         '-',            K_LEFTARROW,    '5',            K_RIGHTARROW,   '+',            K_END,      // 4
    K_DOWNARROW,    K_PGDN,         K_INS,          K_DEL,          0,              0,              0,              K_F11,
    K_F12,          0,              0,              0,              0,              0,              0,              0,          // 5
    0,              0,              0,              0,              0,              0,              0,              0,
    0,              0,              0,              0,              0,              0,              0,              0,          // 6
    0,              0,              0,              0,              0,              0,              0,              0,
    0,              0,              0,              0,              0,              0,              0,              0           // 7
};

/*
=======
MapKey

Map from windows to quake keynums
=======
*/
int MapKey (int key)
{
	key = (key>>16)&255;
	if (key > 127)
		return 0;
	if (scantokey[key] == 0)
		Con_DPrintf("key 0x%02x has no translation\n", key);
	return scantokey[key];
}

/*
===================================================================

MAIN WINDOW

===================================================================
*/

/*
==================
MainWndProc

main window procedure
==================
*/
LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LONG	lRet = 1;
	int		button;
	extern unsigned int uiWheelMessage;
	static qboolean active = false;

	if ( uMsg == uiWheelMessage )
		uMsg = WM_MOUSEWHEEL;

	switch (uMsg)
	{
	case WM_KILLFOCUS:
		break;

	case WM_SETFOCUS:
		break;

	case WM_CREATE:
		break;

	case WM_MOVE:
		window_x = (int) LOWORD(lParam);
		window_y = (int) HIWORD(lParam);
		VID_UpdateWindowStatus ();
		break;

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		Key_Event (MapKey(lParam), true);
		break;
		
	case WM_KEYUP:
	case WM_SYSKEYUP:
		Key_Event (MapKey(lParam), false);
		break;

	case WM_SYSCHAR:
		// keep Alt-Space from happening
		break;

	// this is complicated because Win32 seems to pack multiple mouse events into
	// one update sometimes, so we always check all states and look for events
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_XBUTTONDOWN:
	case WM_XBUTTONUP:
	case WM_MOUSEMOVE:
		button = 0;

		if (wParam & MK_LBUTTON)
			button |= 1;

		if (wParam & MK_RBUTTON)
			button |= 2;

		if (wParam & MK_MBUTTON)
			button |= 4;

		// Intellimouse(c) explorer
		if (wParam & MK_XBUTTON1)
			button |= 8;

		if (wParam & MK_XBUTTON2)
			button |= 16;

		// copied from DarkPlaces in an attempt to grab more buttons
		if (wParam & MK_XBUTTON3)
			button |= 32;

		if (wParam & MK_XBUTTON4)
			button |= 64;

		if (wParam & MK_XBUTTON5)
			button |= 128;

		if (wParam & MK_XBUTTON6)
			button |= 256;

		if (wParam & MK_XBUTTON7)
			button |= 512;

		IN_MouseEvent (button);
		break;

	// JACK: This is the mouse wheel support
	// Its delta is either positive or negative, and we generate the proper event
	case WM_MOUSEWHEEL: 
		if ((short) HIWORD(wParam) > 0) 
		{
			Key_Event(K_MWHEELUP, true);
			Key_Event(K_MWHEELUP, false);
		}
		else 
		{
			Key_Event(K_MWHEELDOWN, true);
			Key_Event(K_MWHEELDOWN, false);
		}
		break;

	case WM_SIZE:
		break;

	case WM_CLOSE:
		if (MessageBox (mainwindow, "Are you sure you want to quit?", "Confirm Exit",
			MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION) == IDYES)
		{
			Sys_Quit (0);
		}
		break;

	case WM_ACTIVATE:
		switch (LOWORD(wParam))
		{
		case WA_ACTIVE:
		case WA_CLICKACTIVE:
			vid_activewindow = true;
			if (modestate == MS_FULLSCREEN)
				vid_hiddenwindow = false; // because HIWORD(wParam) give us incorrect value in fullscreen
			else if (modestate == MS_WINDOWED)
				vid_hiddenwindow = (BOOL) HIWORD(wParam);
			break;

		case WA_INACTIVE:
			vid_activewindow = false;
			if (modestate == MS_FULLSCREEN)
				vid_hiddenwindow = true; // because HIWORD(wParam) give us incorrect value in fullscreen
			else if (modestate == MS_WINDOWED)
				vid_hiddenwindow = (BOOL) HIWORD(wParam);
			break;
		}

		// enable/disable sound, set/restore gamma and activate/deactivate mouse
		// on focus gain/loss
		if (vid_activewindow && !vid_hiddenwindow)// && !active)
		{
            if (!active) {
			if (modestate == MS_FULLSCREEN)
			{
				IN_ActivateMouse ();
				IN_HideMouse ();
				if (vid_canalttab && vid_wassuspended)
				{
					ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN);
					ShowWindow(mainwindow, SW_SHOWNORMAL);
					MoveWindow(mainwindow, 0, 0, gdevmode.dmPelsWidth, gdevmode.dmPelsHeight, false); // Fix for alt-tab bug in NVidia drivers
					vid_wassuspended = false;
				}
			}
			else if ((modestate == MS_WINDOWED) && key_dest == key_game)
			//else if (modestate == MS_WINDOWED)
			{
				IN_ActivateMouse ();
				IN_HideMouse ();
			}
            CDAudio_Resume ();
			S_UnblockSound ();
			S_ClearBuffer ();
			VID_Gamma_Set ();
			active = true;
            }
		}
		else //if (active) //if (!vid_activewindow)
		{
            if (active) {
			if (modestate == MS_FULLSCREEN)
			{
				IN_DeactivateMouse ();
				IN_ShowMouse ();
				if (vid_canalttab)
				{ 
					ChangeDisplaySettings (NULL, 0);
					ShowWindow(mainwindow, SW_SHOWMINNOACTIVE); // moved here from WM_KILLFOCUS case
					vid_wassuspended = true;
				}
			}
			else if ((modestate == MS_WINDOWED))
			{
				IN_DeactivateMouse ();
				IN_ShowMouse ();
			}
            CDAudio_Pause ();
			S_BlockSound ();
			S_ClearBuffer ();
			VID_Gamma_Restore ();
			active = false;
            }
		}

		// fix the leftover Alt from any Alt-Tab or the like that switched us away
		Key_ClearStates ();
		IN_ClearStates ();
		break;

	case WM_DESTROY:
		if (mainwindow)
			DestroyWindow (mainwindow);

		PostQuitMessage (0);
		break;

	case MM_MCINOTIFY:
		lRet = CDAudio_MessageHandler (hWnd, uMsg, wParam, lParam);
		break;

	default:
		/* pass all unhandled messages to DefWindowProc */
		lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);
		break;
	}

	/* return 1 if handled message, 0 if not */
	return lRet;
}

//==========================================================================
//
//  COMMANDS
//
//==========================================================================

/*
=================
VID_GetModePtr
=================
*/
vmode_t *VID_GetModePtr (int modenum)
{
	if ((modenum >= 0) && (modenum < nummodes))
		return &modelist[modenum];
	else
		return &badmode;
}


/*
=================
VID_GetExtModeDescription

KJB: Added this to return the mode driver name in description for console
=================
*/
char *VID_GetExtModeDescription (int mode)
{
	static char	pinfo[256];
	vmode_t		*pv;

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	pv = VID_GetModePtr (mode);
	if (modelist[mode].type == MS_FULLSCREEN)
	{
		if (!leavecurrentmode)
		{
			sprintf(pinfo,"%s fullscreen", pv->modedesc);
		}
		else
		{
			sprintf (pinfo, "desktop resolution (%dx%dx%d %dHz)", // added bpp, refreshrate
					 modelist[MODE_FULLSCREEN_DEFAULT].width,
					 modelist[MODE_FULLSCREEN_DEFAULT].height,
					 modelist[MODE_FULLSCREEN_DEFAULT].bpp, // added bpp
					 modelist[MODE_FULLSCREEN_DEFAULT].refreshrate); // added refreshrate
		}
	}
	else
	{
		if (modestate == MS_WINDOWED)
			sprintf(pinfo, "%s windowed", pv->modedesc);
		else
			sprintf(pinfo, "windowed");
	}

	return pinfo;
}


/*
=================
VID_DescribeCurrentMode_f
=================
*/
void VID_DescribeCurrentMode_f (void)
{
	Con_Printf ("%s\n", VID_GetExtModeDescription (vid_modenum));
}


/*
=================
VID_DescribeModes_f

changed formatting, and added refresh rates after each mode.
=================
*/
void VID_DescribeModes_f (void)
{
	int			i, lnummodes, t;
	char		*pinfo;
	int			count = 0;

	lnummodes = nummodes;

	t = leavecurrentmode;
	leavecurrentmode = 0;

	for (i=1 ; i<lnummodes ; i++)
	{
		pinfo = VID_GetExtModeDescription (i);
		Con_Printf ("   %s\n", pinfo);
		count++;
	}
	Con_Printf ("%i modes\n", count);

	leavecurrentmode = t;
}

//==========================================================================
//
//  INIT
//
//==========================================================================

/*
=================
VID_InitWindow
=================
*/
void VID_InitWindow (HINSTANCE hInstance)
{
	DEVMODE			devmode;
	WNDCLASS		wc;

	/* Register the frame class */
	wc.style         = 0;
	wc.lpfnWndProc   = (WNDPROC)MainWndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon         = 0;
	wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName  = 0;
	wc.lpszClassName = "fxQuake";

	if (!RegisterClass (&wc) )
		Sys_Error ("Couldn't register window class");

	modelist[0].type = MS_WINDOWED;

	if (COM_CheckParm("-width"))
		modelist[0].width = atoi(com_argv[COM_CheckParm("-width")+1]);
	else
		modelist[0].width = 640;

	if (modelist[0].width < 320)
		modelist[0].width = 320;

	if (COM_CheckParm("-height"))
		modelist[0].height= atoi(com_argv[COM_CheckParm("-height")+1]);
	else
		modelist[0].height = modelist[0].width * 240/320;

	if (modelist[0].height < 200) // was 240
		modelist[0].height = 200; // was 240

	// get desktop bit depth and refresh rate
	if (EnumDisplaySettings (NULL, ENUM_CURRENT_SETTINGS, &devmode))
	{
		modelist[0].bpp = desktop_bpp = devmode.dmBitsPerPel;
		modelist[0].refreshrate = desktop_refreshrate = devmode.dmDisplayFrequency;
	}

	sprintf (modelist[0].modedesc, "%dx%dx%d %dHz", // added bpp, refreshrate
			 modelist[0].width,
			 modelist[0].height,
			 modelist[0].bpp, // added bpp
			 modelist[0].refreshrate); // added refreshrate

	modelist[0].modenum = MODE_WINDOWED;
	modelist[0].dib = 1;
	modelist[0].fullscreen = 0;
	modelist[0].halfscreen = 0;

	nummodes = 1;
}


/*
=================
VID_InitFullScreen
=================
*/
void VID_InitFullScreen (HINSTANCE hInstance)
{
	DEVMODE	devmode;
	int		i, modenum, originalnummodes, existingmode, numlowresmodes;
	int		j, bpp;
	qboolean	done;
	BOOL	stat;

// enumerate >8 bpp modes
	originalnummodes = nummodes;
	modenum = 0;

	do
	{
		stat = EnumDisplaySettings (NULL, modenum, &devmode);

		if ((devmode.dmBitsPerPel >= 15) &&
			(devmode.dmPelsWidth <= MAXWIDTH) &&
			(devmode.dmPelsHeight <= MAXHEIGHT) &&
			(nummodes < MAX_MODE_LIST))
		{
			devmode.dmFields = DM_BITSPERPEL |
							   DM_PELSWIDTH |
							   DM_PELSHEIGHT |
							   DM_DISPLAYFREQUENCY; // refreshrate

			if (ChangeDisplaySettings (&devmode, CDS_TEST | CDS_FULLSCREEN) ==
					DISP_CHANGE_SUCCESSFUL)
			{
				modelist[nummodes].type = MS_FULLSCREEN;
				modelist[nummodes].width = devmode.dmPelsWidth;
				modelist[nummodes].height = devmode.dmPelsHeight;
				modelist[nummodes].modenum = 0;
				modelist[nummodes].halfscreen = 0;
				modelist[nummodes].dib = 1;
				modelist[nummodes].fullscreen = 1;
				modelist[nummodes].bpp = devmode.dmBitsPerPel;
				modelist[nummodes].refreshrate = devmode.dmDisplayFrequency; // refreshrate
				sprintf (modelist[nummodes].modedesc, "%dx%dx%d %dHz", // refreshrate
						 (int)devmode.dmPelsWidth,
						 (int)devmode.dmPelsHeight,
						 (int)devmode.dmBitsPerPel,
						 (int)devmode.dmDisplayFrequency); // refreshrate

			// if the width is more than twice the height, reduce it by half because this
			// is probably a dual-screen monitor
				if (!COM_CheckParm("-noadjustaspect"))
				{
					if (modelist[nummodes].width > (modelist[nummodes].height << 1))
					{
						modelist[nummodes].width >>= 1;
						modelist[nummodes].halfscreen = 1;
						sprintf (modelist[nummodes].modedesc, "%dx%dx%d %dHz", // refreshrate
								 modelist[nummodes].width,
								 modelist[nummodes].height,
								 modelist[nummodes].bpp,
								 modelist[nummodes].refreshrate); // refreshrate
					}
				}

				for (i=originalnummodes, existingmode = 0 ; i<nummodes ; i++)
				{
					if ((modelist[nummodes].width == modelist[i].width)   &&
						(modelist[nummodes].height == modelist[i].height) &&
						(modelist[nummodes].bpp == modelist[i].bpp) &&
						(modelist[nummodes].refreshrate == modelist[i].refreshrate)) // refreshrate
					{
						existingmode = 1;
						break;
					}
				}

				if (!existingmode)
				{
					nummodes++;
				}
			}
		}

		modenum++;
	} while (stat);

// see if there are any low-res modes that aren't being reported
	numlowresmodes = sizeof(lowresmodes) / sizeof(lowresmodes[0]);
	bpp = 16;
	done = false;

	do
	{
		for (j=0 ; (j<numlowresmodes) && (nummodes < MAX_MODE_LIST) ; j++)
		{
			devmode.dmBitsPerPel = bpp;
			devmode.dmPelsWidth = lowresmodes[j].width;
			devmode.dmPelsHeight = lowresmodes[j].height;
			devmode.dmFields = DM_BITSPERPEL |
							   DM_PELSWIDTH |
							   DM_PELSHEIGHT |
							   DM_DISPLAYFREQUENCY; // refreshrate;

			if (ChangeDisplaySettings (&devmode, CDS_TEST | CDS_FULLSCREEN) ==
					DISP_CHANGE_SUCCESSFUL)
			{
				modelist[nummodes].type = MS_FULLSCREEN;
				modelist[nummodes].width = devmode.dmPelsWidth;
				modelist[nummodes].height = devmode.dmPelsHeight;
				modelist[nummodes].modenum = 0;
				modelist[nummodes].halfscreen = 0;
				modelist[nummodes].dib = 1;
				modelist[nummodes].fullscreen = 1;
				modelist[nummodes].bpp = devmode.dmBitsPerPel;
				modelist[nummodes].refreshrate = devmode.dmDisplayFrequency; // refreshrate
				sprintf (modelist[nummodes].modedesc, "%dx%dx%d %dHz", // refreshrate
						 (int)devmode.dmPelsWidth,
						 (int)devmode.dmPelsHeight,
						 (int)devmode.dmBitsPerPel,
						 (int)devmode.dmDisplayFrequency); // refreshrate

				for (i=originalnummodes, existingmode = 0 ; i<nummodes ; i++)
				{
					if ((modelist[nummodes].width == modelist[i].width)   &&
						(modelist[nummodes].height == modelist[i].height) &&
						(modelist[nummodes].bpp == modelist[i].bpp) &&
						(modelist[nummodes].refreshrate == modelist[i].refreshrate)) // refreshrate
					{
						existingmode = 1;
						break;
					}
				}

				if (!existingmode)
				{
					nummodes++;
				}
			}
		}
		switch (bpp)
		{
		case 16:
			bpp = 32;
			break;

		case 32:
			bpp = 24;
			break;

		case 24:
			done = true;
			break;
		}
	} while (!done);

	if (nummodes == originalnummodes)
		Con_SafePrintf ("No fullscreen modes found\n");
}

/*
===================
VID_Init
===================
*/
void VID_Init (void)
{
	int		i, existingmode;
	int		width, height, bpp, refreshrate;
	qboolean	findbpp, done;
	HDC		hdc;
	DEVMODE	devmode;

	memset(&devmode, 0, sizeof(devmode));

	Cmd_AddCommand ("vid_describecurrentmode", VID_DescribeCurrentMode_f);
	Cmd_AddCommand ("vid_describemodes", VID_DescribeModes_f);

	hIcon = LoadIcon (global_hInstance, MAKEINTRESOURCE (IDI_ICON2));

	VID_InitWindow (global_hInstance);

	VID_InitFullScreen (global_hInstance);

	if (COM_CheckParm("-window"))
	{
		hdc = GetDC (NULL);

		if (GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE)
		{
			Sys_Error ("Can't run in non-RGB mode");
		}

		ReleaseDC (NULL, hdc);

		windowed = true;

		vid_default = MODE_WINDOWED;
	}
	else
	{
		if (nummodes == 1)
			Sys_Error ("No RGB fullscreen modes available");

		windowed = false;

		if (COM_CheckParm("-mode"))
		{
			vid_default = atoi(com_argv[COM_CheckParm("-mode")+1]);
		}
		else
		{
			if (COM_CheckParm("-current"))
			{
				if (EnumDisplaySettings (NULL, ENUM_CURRENT_SETTINGS, &devmode))
				{
					modelist[MODE_FULLSCREEN_DEFAULT].width = devmode.dmPelsWidth;
					modelist[MODE_FULLSCREEN_DEFAULT].height = devmode.dmPelsHeight;
					modelist[MODE_FULLSCREEN_DEFAULT].bpp = devmode.dmBitsPerPel; // added bpp
					modelist[MODE_FULLSCREEN_DEFAULT].refreshrate = devmode.dmDisplayFrequency; // added refreshrate
				}
				vid_default = MODE_FULLSCREEN_DEFAULT;
				leavecurrentmode = 1;
			}
			else
			{
				if (COM_CheckParm("-width"))
				{
					width = atoi(com_argv[COM_CheckParm("-width")+1]);
				}
				else
				{
					width = 640;
				}

				if (COM_CheckParm("-height"))
				{
					height = atoi(com_argv[COM_CheckParm("-height")+1]);
				}
				else
				{
					height = width * 240/320;
				}

				if (COM_CheckParm("-bpp"))
				{
					bpp = atoi(com_argv[COM_CheckParm("-bpp")+1]);
					findbpp = false;
				}
				else
				{
					bpp = desktop_bpp;
					findbpp = true;
				}

				if (COM_CheckParm("-refreshrate"))
				{
					refreshrate = atoi(com_argv[COM_CheckParm("-refreshrate")+1]);
				}
				else
				{
					refreshrate = desktop_refreshrate;
				}

				// if they want to force it, add the specified mode to the list
				if (COM_CheckParm("-force") && (nummodes < MAX_MODE_LIST))
				{
					modelist[nummodes].type = MS_FULLSCREEN;
					modelist[nummodes].width = width;
					modelist[nummodes].height = height;
					modelist[nummodes].modenum = 0;
					modelist[nummodes].halfscreen = 0;
					modelist[nummodes].dib = 1;
					modelist[nummodes].fullscreen = 1;
					modelist[nummodes].bpp = bpp;
					modelist[nummodes].refreshrate = refreshrate;
					sprintf (modelist[nummodes].modedesc, "%dx%dx%d %dHz", // refreshrate
							 (int)devmode.dmPelsWidth,
							 (int)devmode.dmPelsHeight,
							 (int)devmode.dmBitsPerPel,
							 (int)devmode.dmDisplayFrequency); // refreshrate

					for (i=nummodes, existingmode = 0 ; i<nummodes ; i++)
					{
						if ((modelist[nummodes].width == modelist[i].width)   &&
							(modelist[nummodes].height == modelist[i].height) &&
							(modelist[nummodes].bpp == modelist[i].bpp) &&
							(modelist[nummodes].refreshrate == modelist[i].refreshrate)) // refreshrate
						{
							existingmode = 1;
							break;
						}
					}

					if (!existingmode)
					{
						nummodes++;
					}
				}

				done = false;

				do
				{
					if (COM_CheckParm("-height"))
					{
						height = atoi(com_argv[COM_CheckParm("-height")+1]);

						for (i=1, vid_default=0 ; i<nummodes ; i++)
						{
							if ((modelist[i].width == width) &&
								(modelist[i].height == height) &&
								(modelist[i].bpp == bpp) &&
								(modelist[i].refreshrate == refreshrate))
							{
								vid_default = i;
								done = true;
								break;
							}
						}
					}
					else
					{
						for (i=1, vid_default=0 ; i<nummodes ; i++)
						{
							if ((modelist[i].width == width) && (modelist[i].bpp == bpp) && (modelist[i].refreshrate == refreshrate))
							{
								vid_default = i;
								done = true;
								break;
							}
						}
					}

					if (!done)
					{
						if (findbpp)
						{
							switch (bpp)
							{
							case 15:
								bpp = 16;
								break;
							case 16:
								bpp = 32;
								break;
							case 32:
								bpp = 24;
								break;
							case 24:
								done = true;
								break;
							}
						}
						else
						{
							done = true;
						}
					}
				} while (!done);

				if (!vid_default)
				{
					Sys_Error ("Specified video mode not available");
				}
			}
		}
	}

	vid_initialized = true;

    vid.colormap = host_colormap;

	DestroyWindow (hwnd_dialog);

	VID_SetMode (vid_default);

	maindc = GetDC(mainwindow);
	bSetupPixelFormat(maindc);

	baseRC = wglCreateContext( maindc );
	if (!baseRC)
		Sys_Error ("Could not initialize GL (wglCreateContext failed).\n\nMake sure you in are 65535 color mode, and try running -window.");
	if (!wglMakeCurrent( maindc, baseRC ))
		Sys_Error ("wglMakeCurrent failed");

	GL_Init ();

	VID_Gamma_Init ();

	Cvar_RegisterVariable (&vid_gamma, VID_Gamma);
	Cvar_RegisterVariable (&vid_contrast, VID_Gamma);

	vid_realmode = vid_modenum;

	strcpy (badmode.modedesc, "Bad mode");
	vid_canalttab = true;

	if (COM_CheckParm("-fullsbar"))
		fullsbardraw = true;
}

