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
// xquake.h -- X Window System specific Quake header file

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <X11/xpm.h>

#include <X11/extensions/XShm.h>
#include <X11/extensions/Xxf86dga.h>
#include <X11/extensions/xf86vmode.h>

#define KEY_MASK (KeyPressMask | KeyReleaseMask)
#define MOUSE_MASK (ButtonPressMask | ButtonReleaseMask | PointerMotionMask | ButtonMotionMask)
#define X_MASK (KEY_MASK | MOUSE_MASK | VisibilityChangeMask | StructureNotifyMask | ExposureMask | FocusChangeMask | EnterWindowMask | LeaveWindowMask)

extern Display *x_disp;
extern Window x_win;
extern int scrnum;

extern Atom wm_delete_window_atom;

extern int window_x, window_y, window_width, window_height;

extern XF86VidModeModeInfo init_vidmode, game_vidmode;

extern qboolean mouse_available; // Mouse available for use
extern qboolean keyboard_available;	// Keyboard available for use

extern qboolean mouse_grab_active, keyboard_grab_active;
extern qboolean dga_mouse_available, dga_keyboard_available;
extern qboolean dga_mouse_active, dga_keyboard_active;

extern float mouse_x, mouse_y;

extern qboolean vidmode_fullscreen; // was vidmode_active

int XLateKey (XKeyEvent *ev);
void IN_GrabMouse (void);
void IN_UngrabMouse (void);
void IN_GrabKeyboard (void);
void IN_UngrabKeyboard (void);

