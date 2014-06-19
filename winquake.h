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
// winquake.h -- Win32 specific Quake header file

#ifndef _WIN32
#error "You shouldn't be including this file for non Win32 stuff!"
#endif

#include <windows.h>
#include <dsound.h>
#include <dinput.h>
#include <direct.h>

// FIXME - mousewheel redefined? What is this magic number?
#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL		0x020A
#endif

// LordHavoc: thanks to backslash for this support for mouse buttons 4 and 5
/* These are #ifdefed out for non-Win2K in the February 2001 version of
   MS's platform SDK, but we need them for compilation... */
#ifndef WM_XBUTTONDOWN
#define WM_XBUTTONDOWN		0x020B
#define WM_XBUTTONUP		0x020C
#endif

#ifndef MK_XBUTTON1
#define MK_XBUTTON1			0x0020
#define MK_XBUTTON2			0x0040
#endif

#ifndef MK_XBUTTON3
// copied from DarkPlaces in an attempt to grab more buttons
#define MK_XBUTTON3         0x0080
#define MK_XBUTTON4         0x0100
#define MK_XBUTTON5         0x0200
#define MK_XBUTTON6         0x0400
#define MK_XBUTTON7         0x0800
#endif

extern	HINSTANCE	global_hInstance;
extern	int			global_nCmdShow;

extern LPDIRECTSOUND		pDS;
extern LPDIRECTSOUNDBUFFER	pDSBuf;
extern DWORD				gSndBufSize;

#ifndef DWORD_PTR
typedef unsigned long DWORD_PTR;
#endif

typedef enum {MS_WINDOWED, MS_FULLSCREEN, MS_UNINIT} modestate_t;

extern modestate_t	modestate;

extern HWND			mainwindow;

extern qboolean	WinNT;

qboolean IN_InitDInput (void);

void IN_ShowMouse (void);
void IN_HideMouse (void);
void IN_ActivateMouse (void);
void IN_DeactivateMouse (void);
void IN_MouseEvent (int mstate);
void IN_ClearStates (void); // restores all button and position states to defaults

extern int		window_center_x, window_center_y;
extern RECT		window_rect;

extern qboolean	mouseinitialized;
extern HWND		hwnd_dialog;

extern HANDLE	hinput, houtput;

void IN_UpdateClipCursor (void);

