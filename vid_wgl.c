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

#define MAX_MODE_LIST		600 //johnfitz -- was 30

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
	int			refreshrate; //johnfitz
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

void VID_Menu_Init (void); //johnfitz
void VID_Menu_f (void); //johnfitz
void VID_MenuDraw (void);
void VID_MenuKey (int key);

LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LONG CDAudio_MessageHandler (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
char *VID_GetExtModeDescription (int mode);
char *VID_GetModeDescription (int mode);
void VID_UpdateWindowStatus (void);

qboolean vid_locked = false; //johnfitz
qboolean vid_changed = false;

//qboolean	vid_initialized = false;

//====================================

//johnfitz -- new cvars
cvar_t		vid_fullscreen = {"vid_fullscreen", "1", true};
cvar_t		vid_width = {"vid_width", "640", true};
cvar_t		vid_height = {"vid_height", "480", true};
cvar_t		vid_bpp = {"vid_bpp", "16", true};
cvar_t		vid_refreshrate = {"vid_refreshrate", "60", true};
cvar_t		vid_vsync = {"vid_vsync", "0", true};
//johnfitz


int			window_center_x, window_center_y, window_x, window_y, window_width, window_height;
RECT		window_rect;


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

	//johnfitz --  if width and height match desktop size, do aguirRe's trick of making the window have no titlebar/borders
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
							DM_DISPLAYFREQUENCY; //johnfitz -- refreshrate
		gdevmode.dmBitsPerPel = modelist[modenum].bpp;
		gdevmode.dmPelsWidth = modelist[modenum].width << modelist[modenum].halfscreen;
		gdevmode.dmPelsHeight = modelist[modenum].height;
		gdevmode.dmDisplayFrequency = modelist[modenum].refreshrate; //johnfitz -- refreshrate
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

	vid.conwidth = vid.width;
	vid.conheight = vid.height;

	vid.colormap = host_colormap;

	vid.recalc_refdef = true;

	return true;
}


/*
===================
VID_Restart -- johnfitz -- change video modes on the fly
===================
*/
void VID_Restart (void)
{
	HDC			hdc;
	HGLRC		hrc;
	int			i;
	qboolean	mode_changed = false;
	vmode_t		oldmode;

	if (vid_locked)
		return;

//
// check cvars against current mode
//
	if (vid_fullscreen.value)
	{
		if (modelist[vid_default].type == MS_WINDOWED)
			mode_changed = true;
		else if (modelist[vid_default].bpp != (int)vid_bpp.value)
			mode_changed = true;
		else if (modelist[vid_default].refreshrate != (int)vid_refreshrate.value)
			mode_changed = true;
	}
	else
		if (modelist[vid_default].type != MS_WINDOWED)
			mode_changed = true;

	if (modelist[vid_default].width != (int)vid_width.value ||
		modelist[vid_default].height != (int)vid_height.value)
		mode_changed = true;

	if (mode_changed)
	{
//
// decide which mode to set
//
		oldmode = modelist[vid_default];

		if (vid_fullscreen.value)
		{
			for (i=1; i<nummodes; i++)
			{
				if (modelist[i].width == (int)vid_width.value &&
					modelist[i].height == (int)vid_height.value &&
					modelist[i].bpp == (int)vid_bpp.value &&
					modelist[i].refreshrate == (int)vid_refreshrate.value)
				{
					break;
				}
			}

			if (i == nummodes)
			{
				Con_Printf ("%dx%dx%d %dHz is not a valid fullscreen mode\n",
							(int)vid_width.value,
							(int)vid_height.value,
							(int)vid_bpp.value,
							(int)vid_refreshrate.value);
				return;
			}

			windowed = false;
			vid_default = i;
		}
		else //not fullscreen
		{
			hdc = GetDC (NULL);
			if (GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE)
			{
				Con_Printf ("Can't run windowed on non-RGB desktop\n");
				ReleaseDC (NULL, hdc);
				return;
			}
			ReleaseDC (NULL, hdc);

			if (vid_width.value < 320)
			{
				Con_Printf ("Window width can't be less than 320\n");
				return;
			}

			if (vid_height.value < 200)
			{
				Con_Printf ("Window height can't be less than 200\n");
				return;
			}

			modelist[0].width = (int)vid_width.value;
			modelist[0].height = (int)vid_height.value;
			sprintf (modelist[0].modedesc, "%dx%dx%d %dHz",
					 modelist[0].width,
					 modelist[0].height,
					 modelist[0].bpp,
					 modelist[0].refreshrate);

			windowed = true;
			vid_default = 0;
		}
//
// destroy current window
//
		hrc = wglGetCurrentContext();
		hdc = wglGetCurrentDC();
		wglMakeCurrent(NULL, NULL);

		vid_canalttab = false;

		if (hdc && mainwindow)
			ReleaseDC (mainwindow, hdc);
		if (modestate == MS_FULLSCREEN)
			ChangeDisplaySettings (NULL, 0);
		if (maindc && mainwindow)
			ReleaseDC (mainwindow, maindc);
		maindc = NULL;
		if (mainwindow)
			DestroyWindow (mainwindow);

		// textures are invalid after mode change,
		// we destroy and re-create GL context,
		// so we delete all GL textures now.
		TexMgr_DeleteTextures();

		vid_activewindow = false;
		vid_hiddenwindow = true;
		vid_notifywindow = false;

//
// set new mode
//
		VID_SetMode (vid_default);
		maindc = GetDC(mainwindow);
		bSetupPixelFormat(maindc);

		// try to reuse existing render context, and if that fails create a new one
		if (!wglMakeCurrent (maindc, hrc))
		{
			wglDeleteContext (hrc);
			hrc = wglCreateContext (maindc);
			if (!wglMakeCurrent (maindc, hrc))
			{
				char szBuf[80];
				LPVOID lpMsgBuf;
				DWORD dw = GetLastError();
				FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &lpMsgBuf, 0, NULL );
				sprintf(szBuf, "VID_Restart: wglMakeCurrent failed with error %d: %s", dw, (char*)lpMsgBuf);
 				Sys_Error (szBuf);
			}
			//TexMgr_ReloadImages ();
			//GL_SetupState ();
		}

		vid_activewindow = true;
		vid_hiddenwindow = false;
		vid_notifywindow = true;

		GL_GetPixelFormatInfo();
		GL_GetGLInfo();

		// re-create and reload all GL textures with new context.
		TexMgr_GenerateTextures();

//		GL_Init();
		GL_SetupState();
//		GL_SwapInterval();

		GL_SetupState();
		GL_CheckSwapInterval();
		GL_CheckMultithreadedGL();


		vid_canalttab = true;

		//swapcontrol settings were lost when previous window was destroyed
//		VID_Vsync_f ();

		// re-create and reload all GL textures with new context.
//		TexMgr_GenerateTextures();

		//warpimages needs to be recalculated
		TexMgr_UploadWarpImage();

		//conwidth and conheight need to be recalculated
		//vid.conwidth = (scr_conwidth.value > 0) ? (int)scr_conwidth.value : (scr_conscale.value > 0) ? (int)(vid.width/scr_conscale.value) : vid.width;
		//vid.conwidth = CLAMP (320, vid.conwidth, vid.width);
		//vid.conwidth &= 0xFFFFFFF8;
		//vid.conheight = vid.conwidth * vid.height / vid.width;

		//vid.conwidth = vid.width;
		//vid.conheight = vid.height;
	}
//
// keep cvars in line with actual mode
//
	Cvar_Set ("vid_width", va("%i", modelist[vid_default].width));
	Cvar_Set ("vid_height", va("%i", modelist[vid_default].height));
	Cvar_Set ("vid_bpp", va("%i", modelist[vid_default].bpp));
	Cvar_Set ("vid_refreshrate", va("%i", modelist[vid_default].refreshrate));
	Cvar_Set ("vid_fullscreen", (windowed) ? "0" : "1");
}

/*
================
VID_Test -- johnfitz -- like vid_restart, but asks for confirmation after switching modes
================
*/
void VID_Test (void)
{
	vmode_t oldmode;
	qboolean	mode_changed = false;

	if (vid_locked)
		return;
//
// check cvars against current mode
//
	if (vid_fullscreen.value)
	{
		if (modelist[vid_default].type == MS_WINDOWED)
			mode_changed = true;
		else if (modelist[vid_default].bpp != (int)vid_bpp.value)
			mode_changed = true;
		else if (modelist[vid_default].refreshrate != (int)vid_refreshrate.value)
			mode_changed = true;
	}
	else
		if (modelist[vid_default].type != MS_WINDOWED)
			mode_changed = true;

	if (modelist[vid_default].width != (int)vid_width.value ||
		modelist[vid_default].height != (int)vid_height.value)
		mode_changed = true;

	if (!mode_changed)
		return;
//
// now try the switch
//
	oldmode = modelist[vid_default];

	VID_Restart ();

	//pop up confirmation dialoge
	if (!SCR_ModalMessageTimeout("Would you like to keep this\nvideo mode? (y/n)\n", 5.0f))
	{
		//revert cvars and mode
		Cvar_Set ("vid_width", va("%i", oldmode.width));
		Cvar_Set ("vid_height", va("%i", oldmode.height));
		Cvar_Set ("vid_bpp", va("%i", oldmode.bpp));
		Cvar_Set ("vid_refreshrate", va("%i", oldmode.refreshrate));
		Cvar_Set ("vid_fullscreen", (oldmode.type == MS_WINDOWED) ? "0" : "1");
		VID_Restart ();
	}
}

/*
================
VID_Unlock -- johnfitz
================
*/
void VID_Unlock (void)
{
	vid_locked = false;

	//sync up cvars in case they were changed during the lock
	Cvar_Set ("vid_width", va("%i", modelist[vid_default].width));
	Cvar_Set ("vid_height", va("%i", modelist[vid_default].height));
	Cvar_Set ("vid_bpp", va("%i", modelist[vid_default].bpp));
	Cvar_Set ("vid_refreshrate", va("%i", modelist[vid_default].refreshrate));
	Cvar_Set ("vid_fullscreen", (windowed) ? "0" : "1");
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

	// JACK: This is the mouse wheel support (Intellimouse)
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
VID_GetModeDescription
=================
*/
char *VID_GetModeDescription (int mode)
{
	char		*pinfo;
	vmode_t		*pv;
	static char	temp[100];

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	if (!leavecurrentmode)
	{
		pv = VID_GetModePtr (mode);
		pinfo = pv->modedesc;
	}
	else
	{
		sprintf (temp, "Desktop resolution (%ix%ix%i)", //johnfitz -- added bpp
				 modelist[MODE_FULLSCREEN_DEFAULT].width,
				 modelist[MODE_FULLSCREEN_DEFAULT].height,
				 modelist[MODE_FULLSCREEN_DEFAULT].bpp); //johnfitz -- added bpp
		pinfo = temp;
	}

	return pinfo;
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
	DEVMODE			devmode; //johnfitz
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

	if (modelist[0].height < 200) //johnfitz -- was 240
		modelist[0].height = 200; //johnfitz -- was 240

	// get desktop bit depth and refresh rate
	if (EnumDisplaySettings (NULL, ENUM_CURRENT_SETTINGS, &devmode))
	{
		modelist[0].bpp = desktop_bpp = devmode.dmBitsPerPel;
		modelist[0].refreshrate = desktop_refreshrate = devmode.dmDisplayFrequency;
	}

	sprintf (modelist[0].modedesc, "%dx%dx%d %dHz", //johnfitz -- added bpp, refreshrate
			 modelist[0].width,
			 modelist[0].height,
			 modelist[0].bpp, //johnfitz -- added bpp
			 modelist[0].refreshrate); //johnfitz -- added refreshrate

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
							   DM_DISPLAYFREQUENCY; //johnfitz -- refreshrate

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
				modelist[nummodes].refreshrate = devmode.dmDisplayFrequency; //johnfitz -- refreshrate
				sprintf (modelist[nummodes].modedesc, "%dx%dx%d %dHz", //johnfitz -- refreshrate
						 (int)devmode.dmPelsWidth,
						 (int)devmode.dmPelsHeight,
						 (int)devmode.dmBitsPerPel,
						 (int)devmode.dmDisplayFrequency); //johnfitz -- refreshrate

			// if the width is more than twice the height, reduce it by half because this
			// is probably a dual-screen monitor
				if (!COM_CheckParm("-noadjustaspect"))
				{
					if (modelist[nummodes].width > (modelist[nummodes].height << 1))
					{
						modelist[nummodes].width >>= 1;
						modelist[nummodes].halfscreen = 1;
						sprintf (modelist[nummodes].modedesc, "%dx%dx%d %dHz", //johnfitz -- refreshrate
								 modelist[nummodes].width,
								 modelist[nummodes].height,
								 modelist[nummodes].bpp,
								 modelist[nummodes].refreshrate); //johnfitz -- refreshrate
					}
				}

				for (i=originalnummodes, existingmode = 0 ; i<nummodes ; i++)
				{
					if ((modelist[nummodes].width == modelist[i].width)   &&
						(modelist[nummodes].height == modelist[i].height) &&
						(modelist[nummodes].bpp == modelist[i].bpp) &&
						(modelist[nummodes].refreshrate == modelist[i].refreshrate)) //johnfitz -- refreshrate
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
							   DM_DISPLAYFREQUENCY; //johnfitz -- refreshrate;

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
				modelist[nummodes].refreshrate = devmode.dmDisplayFrequency; //johnfitz -- refreshrate
				sprintf (modelist[nummodes].modedesc, "%dx%dx%d %dHz", //johnfitz -- refreshrate
						 (int)devmode.dmPelsWidth,
						 (int)devmode.dmPelsHeight,
						 (int)devmode.dmBitsPerPel,
						 (int)devmode.dmDisplayFrequency); //johnfitz -- refreshrate

				for (i=originalnummodes, existingmode = 0 ; i<nummodes ; i++)
				{
					if ((modelist[nummodes].width == modelist[i].width)   &&
						(modelist[nummodes].height == modelist[i].height) &&
						(modelist[nummodes].bpp == modelist[i].bpp) &&
						(modelist[nummodes].refreshrate == modelist[i].refreshrate)) //johnfitz -- refreshrate
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
================
VID_Toggle -- new proc by S.A., called by alt-return key binding.

EER1 -- rewritten
================
*/
void	VID_Toggle(void)
{
	//qboolean do_toggle;
	//qboolean fullscreen;

	//S_ClearBuffer();

	//fullscreen = !(qboolean)vid_fullscreen.value;
	//do_toggle = VID_CheckMode((int)vid_width.value,
	//	(int)vid_height.value,
	//	(int)vid_refreshrate.value,
	//	(int)vid_bpp.value,
	//	fullscreen);

	//if (do_toggle)
	//{
	//	Cvar_Set("vid_fullscreen", (fullscreen) ? "1" : "0");
	//	Cbuf_AddText("vid_restart\n");
	//}
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

	Cvar_RegisterVariable (&vid_fullscreen); //johnfitz
	Cvar_RegisterVariable (&vid_width); //johnfitz
	Cvar_RegisterVariable (&vid_height); //johnfitz
	Cvar_RegisterVariable (&vid_bpp); //johnfitz
	Cvar_RegisterVariable (&vid_refreshrate); //johnfitz

	Cmd_AddCommand ("vid_unlock", VID_Unlock); //johnfitz
	Cmd_AddCommand ("vid_restart", VID_Restart); //johnfitz
	Cmd_AddCommand ("vid_test", VID_Test); //johnfitz
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

//    vid.colormap = host_colormap;

	DestroyWindow (hwnd_dialog);

	VID_SetMode (vid_default);

	maindc = GetDC(mainwindow);
	bSetupPixelFormat(maindc);

	baseRC = wglCreateContext( maindc );
	if (!baseRC)
		Sys_Error ("Could not initialize GL (wglCreateContext failed).\n\nMake sure you in are 65535 color mode, and try running -window.");
	if (!wglMakeCurrent( maindc, baseRC ))
		Sys_Error ("wglMakeCurrent failed");

	GL_GetPixelFormatInfo();
	GL_Init ();
	GL_SetupState();

	GL_SwapInterval();


	vid_realmode = vid_modenum;

	vid_menucmdfn = VID_Menu_f; //johnfitz
	vid_menudrawfn = VID_MenuDraw;
	vid_menukeyfn = VID_MenuKey;

	strcpy (badmode.modedesc, "Bad mode");
	vid_canalttab = true;

	if (COM_CheckParm("-fullsbar"))
		fullsbardraw = true;
}

/*
================
VID_SyncCvars -- johnfitz -- set vid cvars to match current video mode
================
*/
void VID_SyncCvars (void)
{
	Cvar_Set ("vid_width", va("%i", modelist[vid_default].width));
	Cvar_Set ("vid_height", va("%i", modelist[vid_default].height));
	Cvar_Set ("vid_bpp", va("%i", modelist[vid_default].bpp));
	Cvar_Set ("vid_refreshrate", va("%i", modelist[vid_default].refreshrate));
	Cvar_Set ("vid_fullscreen", (windowed) ? "0" : "1");
}

//==========================================================================
//
//  NEW VIDEO MENU -- johnfitz
//
//==========================================================================

extern void M_Menu_Options_f (void);
extern void M_Print (int cx, int cy, char *str);
extern void M_PrintWhite (int cx, int cy, char *str);
extern void M_DrawCharacter (int cx, int line, int num);
extern void M_DrawTransPic (int x, int y, qpic_t *pic);
extern void M_DrawPic (int x, int y, qpic_t *pic);
extern void M_DrawCheckbox (int x, int y, int on);

extern qboolean	m_entersound;

//enum {
//	m_none,
//	m_main,
//	m_singleplayer,
//	m_load,
//	m_save,
//	m_multiplayer,
//	m_setup,
//	m_net,
//	m_options,
//	m_video,
//	m_keys,
//	m_help,
//	m_quit,
//	m_serialconfig,
//	m_modemconfig,
//	m_lanconfig,
//	m_gameoptions,
//	m_search,
//	m_slist
//} m_state;

#define VIDEO_OPTIONS_ITEMS 6
int		video_cursor_table[] = {48, 56, 64, 72, 88, 96};
int		video_options_cursor = 0;

typedef struct {int width,height;} vid_menu_mode;

int vid_menu_rwidth;
int vid_menu_rheight;

//TODO: replace these fixed-length arrays with hunk_allocated buffers

vid_menu_mode vid_menu_modes[MAX_MODE_LIST];
int vid_menu_nummodes=0;

int vid_menu_bpps[4];
int vid_menu_numbpps=0;

int vid_menu_rates[20];
int vid_menu_numrates=0;

/*
================
VID_Menu_Init
================
*/
void VID_Menu_Init (void)
{
	int i,j,h,w;

	for (i=1;i<nummodes;i++) //start i at mode 1 because 0 is windowed mode
	{
		w = modelist[i].width;
		h = modelist[i].height;

		for (j=0;j<vid_menu_nummodes;j++)
		{
			if (vid_menu_modes[j].width == w &&
				vid_menu_modes[j].height == h)
				break;
		}

		if (j==vid_menu_nummodes)
		{
			vid_menu_modes[j].width = w;
			vid_menu_modes[j].height = h;
			vid_menu_nummodes++;
		}
	}
}

/*
================
VID_Menu_RebuildBppList

regenerates bpp list based on current vid_width and vid_height
================
*/
void VID_Menu_RebuildBppList (void)
{
	int i,j,b;

	vid_menu_numbpps=0;

	for (i=1;i<nummodes;i++) //start i at mode 1 because 0 is windowed mode
	{
		//bpp list is limited to bpps available with current width/height
		if (modelist[i].width != vid_width.value ||
			modelist[i].height != vid_height.value)
			continue;

		b = modelist[i].bpp;

		for (j=0;j<vid_menu_numbpps;j++)
		{
			if (vid_menu_bpps[j] == b)
				break;
		}

		if (j==vid_menu_numbpps)
		{
			vid_menu_bpps[j] = b;
			vid_menu_numbpps++;
		}
	}

	//if there are no valid fullscreen bpps for this width/height, just pick one
	if (vid_menu_numbpps == 0)
	{
		Cvar_SetValue ("vid_bpp",(float)modelist[0].bpp);
		return;
	}

	//if vid_bpp is not in the new list, change vid_bpp
	for (i=0;i<vid_menu_numbpps;i++)
		if (vid_menu_bpps[i] == (int)(vid_bpp.value))
			break;

	if (i==vid_menu_numbpps)
		Cvar_SetValue ("vid_bpp",(float)vid_menu_bpps[0]);
}

/*
================
VID_Menu_RebuildRateList

regenerates rate list based on current vid_width, vid_height and vid_bpp
================
*/
void VID_Menu_RebuildRateList (void)
{
	int i,j,r;

	vid_menu_numrates=0;

	for (i=1;i<nummodes;i++) //start i at mode 1 because 0 is windowed mode
	{
		//rate list is limited to rates available with current width/height/bpp
		if (modelist[i].width != vid_width.value ||
			modelist[i].height != vid_height.value ||
			modelist[i].bpp != vid_bpp.value)
			continue;

		r = modelist[i].refreshrate;

		for (j=0;j<vid_menu_numrates;j++)
		{
			if (vid_menu_rates[j] == r)
				break;
		}

		if (j==vid_menu_numrates)
		{
			vid_menu_rates[j] = r;
			vid_menu_numrates++;
		}
	}

	//if there are no valid fullscreen refreshrates for this width/height, just pick one
	if (vid_menu_numrates == 0)
	{
		Cvar_SetValue ("vid_refreshrate",(float)modelist[0].refreshrate);
		return;
	}

	//if vid_refreshrate is not in the new list, change vid_refreshrate
	for (i=0;i<vid_menu_numrates;i++)
		if (vid_menu_rates[i] == (int)(vid_refreshrate.value))
			break;

	if (i==vid_menu_numrates)
		Cvar_SetValue ("vid_refreshrate",(float)vid_menu_rates[0]);
}

/*
================
VID_Menu_CalcAspectRatio

calculates aspect ratio for current vid_width/vid_height
================
*/
void VID_Menu_CalcAspectRatio (void)
{
	int w,h,f;
	w = vid_width.value;
	h = vid_height.value;
	f = 2;
	while (f < w && f < h)
	{
		if ((w/f)*f == w && (h/f)*f == h)
		{
			w/=f;
			h/=f;
			f=2;
		}
		else
			f++;
	}
	vid_menu_rwidth = w;
	vid_menu_rheight = h;
}

/*
================
VID_Menu_ChooseNextMode

chooses next resolution in order, then updates vid_width and
vid_height cvars, then updates bpp and refreshrate lists
================
*/
void VID_Menu_ChooseNextMode (int dir)
{
	int i;

	for (i=0;i<vid_menu_nummodes;i++)
	{
		if (vid_menu_modes[i].width == vid_width.value &&
			vid_menu_modes[i].height == vid_height.value)
			break;
	}

	if (i==vid_menu_nummodes) //can't find it in list, so it must be a custom windowed res
	{
		i = 0;
	}
	else
	{
		i+=dir;
		if (i>=vid_menu_nummodes)
			i = 0;
		else if (i<0)
			i = vid_menu_nummodes-1;
	}

	Cvar_SetValue ("vid_width",(float)vid_menu_modes[i].width);
	Cvar_SetValue ("vid_height",(float)vid_menu_modes[i].height);
	VID_Menu_RebuildBppList ();
	VID_Menu_RebuildRateList ();
	VID_Menu_CalcAspectRatio ();
}

/*
================
VID_Menu_ChooseNextBpp

chooses next bpp in order, then updates vid_bpp cvar, then updates refreshrate list
================
*/
void VID_Menu_ChooseNextBpp (int dir)
{
	int i;

	for (i=0;i<vid_menu_numbpps;i++)
	{
		if (vid_menu_bpps[i] == vid_bpp.value)
			break;
	}

	if (i==vid_menu_numbpps) //can't find it in list
	{
		i = 0;
	}
	else
	{
		i+=dir;
		if (i>=vid_menu_numbpps)
			i = 0;
		else if (i<0)
			i = vid_menu_numbpps-1;
	}

	Cvar_SetValue ("vid_bpp",(float)vid_menu_bpps[i]);
	VID_Menu_RebuildRateList ();
}

/*
================
VID_Menu_ChooseNextRate

chooses next refresh rate in order, then updates vid_refreshrate cvar
================
*/
void VID_Menu_ChooseNextRate (int dir)
{
	int i;

	for (i=0;i<vid_menu_numrates;i++)
	{
		if (vid_menu_rates[i] == vid_refreshrate.value)
			break;
	}

	if (i==vid_menu_numrates) //can't find it in list
	{
		i = 0;
	}
	else
	{
		i+=dir;
		if (i>=vid_menu_numrates)
			i = 0;
		else if (i<0)
			i = vid_menu_numrates-1;
	}

	Cvar_SetValue ("vid_refreshrate",(float)vid_menu_rates[i]);
}

/*
================
VID_MenuKey
================
*/
void VID_MenuKey (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		VID_SyncCvars (); //sync cvars before leaving menu. FIXME: there are other ways to leave menu
		S_LocalSound ("misc/menu1.wav");
		M_Menu_Options_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		video_options_cursor--;
		if (video_options_cursor < 0)
			video_options_cursor = VIDEO_OPTIONS_ITEMS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		video_options_cursor++;
		if (video_options_cursor >= VIDEO_OPTIONS_ITEMS)
			video_options_cursor = 0;
		break;

	case K_LEFTARROW:
		S_LocalSound ("misc/menu3.wav");
		switch (video_options_cursor)
		{
		case 0:
			VID_Menu_ChooseNextMode (-1);
			break;
		case 1:
			VID_Menu_ChooseNextBpp (-1);
			break;
		case 2:
			VID_Menu_ChooseNextRate (-1);
			break;
		case 3:
			Cbuf_AddText ("toggle vid_fullscreen\n");
			break;
		case 4:
		case 5:
		default:
			break;
		}
		break;

	case K_RIGHTARROW:
		S_LocalSound ("misc/menu3.wav");
		switch (video_options_cursor)
		{
		case 0:
			VID_Menu_ChooseNextMode (1);
			break;
		case 1:
			VID_Menu_ChooseNextBpp (1);
			break;
		case 2:
			VID_Menu_ChooseNextRate (1);
			break;
		case 3:
			Cbuf_AddText ("toggle vid_fullscreen\n");
			break;
		case 4:
		case 5:
		default:
			break;
		}
		break;

	case K_ENTER:
		m_entersound = true;
		switch (video_options_cursor)
		{
		case 0:
			VID_Menu_ChooseNextMode (1);
			break;
		case 1:
			VID_Menu_ChooseNextBpp (1);
			break;
		case 2:
			VID_Menu_ChooseNextRate (1);
			break;
		case 3:
			Cbuf_AddText ("toggle vid_fullscreen\n");
			break;
		case 4:
			Cbuf_AddText ("vid_test\n");
			break;
		case 5:
			Cbuf_AddText ("vid_restart\n");
			break;
		default:
			break;
		}
		break;

	default:
		break;
	}
}

/*
================
VID_MenuDraw
================
*/
void VID_MenuDraw (void)
{
	int i = 0;
	qpic_t *p;
	char *title;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp"));

	//p = Draw_CachePic ("gfx/vidmodes.lmp");
	p = Draw_CachePic ("gfx/p_option.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	// title
	title = "Video Options";
	M_PrintWhite ((320-8*strlen(title))/2, 32, title);

	// options
	M_Print (16, video_cursor_table[i], "        Video mode");
	M_Print (184, video_cursor_table[i], va("%ix%i (%i:%i)", (int)vid_width.value, (int)vid_height.value, vid_menu_rwidth, vid_menu_rheight));
	i++;

	M_Print (16, video_cursor_table[i], "       Color depth");
	M_Print (184, video_cursor_table[i], va("%i", (int)vid_bpp.value));
	i++;

	M_Print (16, video_cursor_table[i], "      Refresh rate");
	M_Print (184, video_cursor_table[i], va("%i Hz", (int)vid_refreshrate.value));
	i++;

	M_Print (16, video_cursor_table[i], "        Fullscreen");
	M_DrawCheckbox (184, video_cursor_table[i], (int)vid_fullscreen.value);
	i++;

	M_Print (16, video_cursor_table[i], "      Test changes");
	i++;

	M_Print (16, video_cursor_table[i], "     Apply changes");

	// cursor
	M_DrawCharacter (168, video_cursor_table[video_options_cursor], 12+((int)(realtime*4)&1));

	// notes          "345678901234567890123456789012345678"
//	M_Print (16, 172, "Windowed modes always use the desk- ");
//	M_Print (16, 180, "top color depth, and can never be   ");
//	M_Print (16, 188, "larger than the desktop resolution. ");
}

/*
================
VID_Menu_f
================
*/
void VID_Menu_f (void)
{
	key_dest = key_menu;
	m_state = m_video;
	m_entersound = true;

	//set all the cvars to match the current mode when entering the menu
	VID_SyncCvars ();

	//set up bpp and rate lists based on current cvars
	VID_Menu_RebuildBppList ();
	VID_Menu_RebuildRateList ();

	//aspect ratio
	VID_Menu_CalcAspectRatio ();
}
