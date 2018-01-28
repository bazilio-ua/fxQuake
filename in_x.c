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
// in_x.c -- X Window System input/output driver code

#include "quakedef.h"
#include "unixquake.h"
#include "xquake.h"

Display *x_disp = NULL;
Window x_win;

qboolean mouse_available;		// Mouse available for use
qboolean keyboard_available;	// Keyboard available for use

qboolean mouse_grab_active, keyboard_grab_active;
qboolean dga_mouse_available, dga_keyboard_available;
qboolean dga_mouse_active, dga_keyboard_active;

float mouse_x=0, mouse_y=0;
static float old_mouse_x, old_mouse_y;

cvar_t m_filter = {"m_filter", "0", true};

qboolean vidmode_fullscreen = false; // was vidmode_active

// if window is not the active window, don't hog as much CPU time,
// let go of the mouse, turn off sound, and restore system gamma ramps...
qboolean vid_activewindow; 
// if window is hidden, don't update screen
qboolean vid_hiddenwindow;
// if mouse entered/leaved window
qboolean vid_notifywindow;

static const byte scantokey[128] = 
{
/*  0 */    0,              0,              0,              0,              0,              0,              0,              0,
/*  8 */    0,              K_ESCAPE,       '1',            '2',            '3',            '4',            '5',            '6',
/* 10 */    '7',            '8',            '9',            '0',            '-',            '=',            K_BACKSPACE,    K_TAB,
/* 18 */    'q',            'w',            'e',            'r',            't',            'y',            'u',            'i',
/* 20 */    'o',            'p',            '[',            ']',            K_ENTER,        K_CTRL,         'a',            's',
/* 28 */    'd',            'f',            'g',            'h',            'j',            'k',            'l',            ';',
/* 30 */    '\'',           '`',            K_SHIFT,        '\\',           'z',            'x',            'c',            'v',
/* 38 */    'b',            'n',            'm',            ',',            '.',            '/',            K_SHIFT,        K_KP_STAR,
/* 40 */    K_ALT,          ' ',            K_CAPSLOCK,     K_F1,           K_F2,           K_F3,           K_F4,           K_F5,
/* 48 */    K_F6,           K_F7,           K_F8,           K_F9,           K_F10,          K_PAUSE,        0,              K_HOME,
/* 50 */    K_UPARROW,      K_PGUP,         K_KP_MINUS,     K_LEFTARROW,    K_KP_5,         K_RIGHTARROW,   K_KP_PLUS,      K_END,
/* 58 */    K_DOWNARROW,    K_PGDN,         K_INS,          K_DEL,          0,              0,              '\\',           K_F11,
/* 60 */    K_F12,          K_HOME,         K_UPARROW,      K_PGUP,         K_LEFTARROW,    0,              K_RIGHTARROW,   K_END,
/* 68 */    K_DOWNARROW,    K_PGDN,         K_INS,          K_DEL,          K_ENTER,        K_CTRL,         K_PAUSE,        0,
/* 70 */    '/',            K_ALT,          0,              0,              0,              0,              0,              0,
/* 78 */    0,              0,              0,              0,              0,              0,              0,              0
}; 

/*
===========
XLateKey

Transform from X key symbols to Quake's symbols
===========
*/
int XLateKey (XKeyEvent *ev)
{
	int key;
	char buf[64];
	KeySym keysym;
	int lookupRet;

	key = 0;

	lookupRet = XLookupString(ev, buf, sizeof(buf), &keysym, 0);

	switch(keysym)
	{
	case XK_KP_Page_Up:
	case XK_Page_Up:
		key = K_PGUP;
		break;

	case XK_KP_Page_Down:
	case XK_Page_Down:
		key = K_PGDN;
		break;

	case XK_KP_Home:
	case XK_Home:
		key = K_HOME;
		break;

	case XK_KP_End:
	case XK_End:
		key = K_END;
		break;

	case XK_KP_Left:
	case XK_Left:
		key = K_LEFTARROW;
		break;

	case XK_KP_Right:
	case XK_Right:
		key = K_RIGHTARROW;
		break;

	case XK_KP_Down:
	case XK_Down:
		key = K_DOWNARROW;
		break;

	case XK_KP_Up:
	case XK_Up:
		key = K_UPARROW;
		break;

	case XK_Escape:
		key = K_ESCAPE;
		break;

	case XK_KP_Enter:
	case XK_Return:
		key = K_ENTER;
		break;

	case XK_Tab:
		key = K_TAB; 
		break;

	case XK_F1: 
		key = K_F1;
		break;

	case XK_F2: 
		key = K_F2;
		break;

	case XK_F3: 
		key = K_F3;
		break;

	case XK_F4: 
		key = K_F4;
		break;

	case XK_F5:
		key = K_F5;
		break;

	case XK_F6:
		key = K_F6;
		break;

	case XK_F7:
		key = K_F7;
		break;

	case XK_F8: 
		key = K_F8;
		break;

	case XK_F9:
		key = K_F9;
		break;

	case XK_F10:
		key = K_F10;
		break;

	case XK_F11:
		key = K_F11;
		break;

	case XK_F12:
		key = K_F12;
		break;

	case XK_BackSpace: 
		key = K_BACKSPACE; 
		break;

	case XK_KP_Delete:
	case XK_Delete: 
		key = K_DEL; 
		break;

	case XK_Pause:
		key = K_PAUSE;
		break;

	case XK_Shift_L:
	case XK_Shift_R:
		key = K_SHIFT;
		break;

	case XK_Execute:
	case XK_Control_L:
	case XK_Control_R:
		key = K_CTRL;
		break;

	case XK_Alt_L:
	case XK_Meta_L:
	case XK_Alt_R:
	case XK_Meta_R:
		key = K_ALT;
		break;

	case XK_KP_Begin: 
		key = K_AUX30;
		break; 

	case XK_Insert:
	case XK_KP_Insert: 
		key = K_INS; 
		break;

	case XK_KP_Multiply: 
		key = '*'; 
		break;

	case XK_KP_Add: 
		key = '+'; 
		break;

	case XK_KP_Subtract: 
		key = '-'; 
		break;

	case XK_KP_Divide: 
		key = '/'; 
		break;

	case XK_section:
		key = '~';
		break;

	case XK_Caps_Lock: 
		key = K_CAPSLOCK; 
		break;

	case XK_Num_Lock: 
		key = K_KP_NUMLOCK; 
		break;

	default:
		if (lookupRet > 0)
		{
			key = *(byte *)buf;
			if (key >= 'A' && key <= 'Z')
				key = key - 'A' + 'a';
			// clipboard
			if (key >= 1 && key <= 26)
				key = key + 'a' - 1;
		}
		else
		{
            key = scantokey[ev->keycode];
		}
		break;
	} 
	return key;
}


/*
===========
CreateNullCursor

makes a null cursor
===========
*/
static Cursor CreateNullCursor (void)
{
	Pixmap cursormask;
	XGCValues xgc;
	GC gc;
	XColor dummycolour;
	Cursor cursor;

	cursormask = XCreatePixmap(x_disp, x_win, 1, 1, 1 /* depth */ );
	xgc.function = GXclear;
	gc = XCreateGC(x_disp, cursormask, GCFunction, &xgc);
	XFillRectangle(x_disp, cursormask, gc, 0, 0, 1, 1);
	dummycolour.pixel = 0;
	dummycolour.flags = 0;
	cursor = XCreatePixmapCursor(x_disp, cursormask, cursormask, &dummycolour, &dummycolour, 0, 0);
	XFreePixmap(x_disp, cursormask);
	XFreeGC(x_disp, gc);

	return cursor;
}


/*
===========
IN_ActivateDGAMouse
===========
*/
static void IN_ActivateDGAMouse (void)
{
	int DGAflags;

	if (dga_mouse_available && !dga_mouse_active)
	{
		DGAflags = XF86DGADirectMouse;
		if (dga_keyboard_active)
		{
			XF86DGADirectVideo(x_disp, DefaultScreen(x_disp), 0);
			DGAflags |= XF86DGADirectKeyb;
		}

		XF86DGADirectVideo(x_disp, DefaultScreen(x_disp), DGAflags);
		dga_mouse_active = true;
	}
}

/*
===========
IN_DeactivateDGAMouse
===========
*/
static void IN_DeactivateDGAMouse (void)
{
	int DGAflags;

	if (dga_mouse_available && dga_mouse_active)
	{
		DGAflags = 0;
		if (dga_keyboard_active)
		{
			XF86DGADirectVideo(x_disp, DefaultScreen(x_disp), 0);
			DGAflags |= XF86DGADirectKeyb;
		}

		XF86DGADirectVideo(x_disp, DefaultScreen(x_disp), DGAflags);
		dga_mouse_active = false;
	}
}

/*
===========
IN_GrabMouse
===========
*/
void IN_GrabMouse (void)
{
	if (mouse_available && !mouse_grab_active && x_win)
	{
		XWindowAttributes attribs_1;
		XSetWindowAttributes attribs_2;

		XGetWindowAttributes(x_disp, x_win, &attribs_1);
		attribs_2.event_mask = attribs_1.your_event_mask | KEY_MASK | MOUSE_MASK;
		XChangeWindowAttributes(x_disp, x_win, CWEventMask, &attribs_2);

		// hide cursor
		XDefineCursor(x_disp, x_win, CreateNullCursor());
		// grab pointer
		XGrabPointer(x_disp, x_win, True, 0, GrabModeAsync, GrabModeAsync, x_win, None, CurrentTime);

		if (dga_mouse_available)
		{
			if (!vidmode_fullscreen && (vid.width < 640 || vid.height < 480))
				Con_Warning ("Running low-res windowed mode, XFree86 DGA Mouse disabled\n");
			else
				IN_ActivateDGAMouse();

		}

		mouse_grab_active = true;
	}
}

/*
===========
IN_UngrabMouse
===========
*/
void IN_UngrabMouse (void)
{
	if (mouse_available && mouse_grab_active)
	{
		if (dga_mouse_active)
		{
			IN_DeactivateDGAMouse();
		}

		// ungrab pointer
		XUngrabPointer(x_disp, CurrentTime);
		// show cursor
		if (x_win)
			XUndefineCursor(x_disp, x_win);

		mouse_grab_active = false;
	}
}


/*
===========
IN_ActivateDGAKeyboard
===========
*/
static void IN_ActivateDGAKeyboard (void)
{
	int DGAflags;

	if (dga_keyboard_available && !dga_keyboard_active)
	{
		DGAflags = XF86DGADirectKeyb;
		if (dga_mouse_active)
		{
			XF86DGADirectVideo(x_disp, DefaultScreen(x_disp), 0);
			DGAflags |= XF86DGADirectMouse;
		}

		XF86DGADirectVideo(x_disp, DefaultScreen(x_disp), DGAflags);
		dga_keyboard_active = true;
	}
}

/*
===========
IN_DeactivateDGAKeyboard
===========
*/
static void IN_DeactivateDGAKeyboard (void)
{
	int DGAflags;

	if (dga_keyboard_available && dga_keyboard_active)
	{
		DGAflags = 0;
		if (dga_mouse_active)
		{
			XF86DGADirectVideo(x_disp, DefaultScreen(x_disp), 0);
			DGAflags |= XF86DGADirectMouse;
		}

		XF86DGADirectVideo(x_disp, DefaultScreen(x_disp), DGAflags);
		dga_keyboard_active = false;
	}
}

/*
===========
IN_GrabKeyboard
===========
*/
void IN_GrabKeyboard (void)
{
	if (keyboard_available && !keyboard_grab_active && x_win)
	{
		// grab keyboard
		XGrabKeyboard(x_disp, x_win, False, GrabModeAsync, GrabModeAsync, CurrentTime);

		if (dga_keyboard_available)
		{
			if (!vidmode_fullscreen && (vid.width < 640 || vid.height < 480))
				Con_Warning ("Running low-res windowed mode, XFree86 DGA Keyboard disabled\n");
			else
				IN_ActivateDGAKeyboard();
		}

		keyboard_grab_active = true;
	}
}

/*
===========
IN_UngrabKeyboard
===========
*/
void IN_UngrabKeyboard (void)
{
	if (keyboard_available && keyboard_grab_active)
	{
		if (dga_keyboard_active)
		{
			IN_DeactivateDGAKeyboard();
		}

		// ungrab keyboard
		XUngrabKeyboard(x_disp, CurrentTime);

		keyboard_grab_active = false;
	}
}


/*
===========
Force_CenterView_f
===========
*/
void Force_CenterView_f (void)
{
	cl.viewangles[PITCH] = 0;
}

/*
===========
IN_Init
===========
*/
void IN_Init (void)
{
	int MajorVersion = 0, MinorVersion = 0;
	qboolean DGA = false;

	Cvar_RegisterVariable (&m_filter, NULL);

	Cmd_AddCommand ("force_centerview", Force_CenterView_f);

	if (COM_CheckParm ("-nomouse"))
		mouse_available = false;
	else
		mouse_available = true;

	mouse_grab_active = false;
	dga_mouse_available = false;
	dga_mouse_active = false;

/*	if (COM_CheckParm ("-nokeyb"))
		keyboard_available = false;
	else
*/		keyboard_available = true;

	keyboard_grab_active = false;
	dga_keyboard_available = false;
	dga_keyboard_active = false;

	if (x_disp == NULL)
		Sys_Error ("IN_Init: x_disp not initialised before input...");

	DGA = XF86DGAQueryVersion(x_disp, &MajorVersion, &MinorVersion);

	if(COM_CheckParm("-nodga"))
	{
		Con_Warning ("XFree86 DGA extension disabled at command line\n");
	}
	else if (DGA) 
	{
		Con_Printf ("XFree86 DGA extension version %d.%d found\n", MajorVersion, MinorVersion);

		if (COM_CheckParm("-nodgamouse"))
			Con_Warning ("XFree86 DGA Mouse disabled at command line\n");
		else
			dga_mouse_available = true;

		if (COM_CheckParm("-nodgakeyb"))
			Con_Warning ("XFree86 DGA Keyboard disabled at command line\n");
		else
			dga_keyboard_available = true;
	}
	else
	{
		Con_Warning ("XFree86 DGA extension not supported\n");
	}

	IN_GrabMouse(); // grab mouse first!
	IN_GrabKeyboard();
}

/*
===========
IN_Shutdown
===========
*/
void IN_Shutdown (void)
{
	IN_UngrabMouse();
	IN_UngrabKeyboard();
}

/*
===========
IN_Commands
===========
*/
void IN_Commands (void)
{
}

/*
===========
IN_MouseWheel
===========
*/
void IN_MouseWheel (void)
{
}

/*
===========
IN_MouseMove
===========
*/
void IN_MouseMove (usercmd_t *cmd)
{
	float	mx, my;

	if (!mouse_grab_active)
		return;

	// apply m_filter if it is on
	mx = mouse_x;
	my = mouse_y;

	if (m_filter.value)
	{
		mouse_x = (mx + old_mouse_x) * 0.5;
		mouse_y = (my + old_mouse_y) * 0.5;
	}

	old_mouse_x = mx;
	old_mouse_y = my;

	mouse_x *= sensitivity.value;
	mouse_y *= sensitivity.value;

// add mouse X/Y movement to cmd
	if ( (in_strafe.state & 1) || (lookstrafe.value && (in_mlook.state & 1) ))
		cmd->sidemove += m_side.value * mouse_x;
	else
		cl.viewangles[YAW] -= m_yaw.value * mouse_x;
	
	if (in_mlook.state & 1)
		V_StopPitchDrift ();
		
	if ( (in_mlook.state & 1) && !(in_strafe.state & 1))
	{
		cl.viewangles[PITCH] += m_pitch.value * mouse_y;
		// variable pitch clamping
		if (cl.viewangles[PITCH] > cl_maxpitch.value)
			cl.viewangles[PITCH] = cl_maxpitch.value;
		if (cl.viewangles[PITCH] < cl_minpitch.value)
			cl.viewangles[PITCH] = cl_minpitch.value;
	}
	else
	{
		if ((in_strafe.state & 1) && cl.noclip_anglehack)
			cmd->upmove -= m_forward.value * mouse_y;
		else
			cmd->forwardmove -= m_forward.value * mouse_y;
	}

	mouse_x = 0;
	mouse_y = 0;
}

/*
===========
IN_Move
===========
*/
void IN_Move (usercmd_t *cmd)
{
	if (vid_activewindow && !vid_hiddenwindow)
		IN_MouseMove(cmd);
}


/*
===========
IN_ProcessEvents
===========
*/
void IN_ProcessEvents (void)
{

	// handle the mouse state when windowed if that's changed
	if (!vidmode_fullscreen)
	{
		if ( key_dest == key_game && !mouse_grab_active && vid_activewindow )
//		if ( key_dest != key_console && !mouse_grab_active && vid_activewindow )
		{
			IN_GrabMouse ();
		}
		else if ( key_dest != key_game && mouse_grab_active ) 
//		else if ( key_dest == key_console && mouse_grab_active ) 
		{
			IN_UngrabMouse ();
		}
	}

	// getting and handle events
	{
		XEvent x_event;
	
		static qboolean active = true;
	
		if (!x_disp)
			return;
	
		while (XPending(x_disp)) 
		{
			XNextEvent(x_disp, &x_event);
	
			switch (x_event.type) 
			{
			case KeyPress: // key pressed
			case KeyRelease: // key released
				Key_Event(XLateKey(&x_event.xkey), x_event.type == KeyPress);
				break;

			case MotionNotify: // mouse moved
				if (mouse_grab_active)
				{
					if (dga_mouse_active)
					{
						mouse_x += (float)x_event.xmotion.x_root;
						mouse_y += (float)x_event.xmotion.y_root;
					}
					else
					{
						mouse_x = (float)x_event.xmotion.x - (float)(vid.width / 2);
						mouse_y = (float)x_event.xmotion.y - (float)(vid.height / 2);

						if (mouse_x || mouse_y) // do warp
						{
							// move the mouse to the window center again
							XWarpPointer(x_disp, None, x_win, 0, 0, 0, 0, vid.width / 2, vid.height / 2);
						}
					}
				}
				break;

			case ButtonPress: // mouse button pressed
			case ButtonRelease: // mouse button released
				switch (x_event.xbutton.button)
				{
				case 1:
					Key_Event (K_MOUSE1, x_event.type == ButtonPress);
					break;

				case 2:
					Key_Event (K_MOUSE3, x_event.type == ButtonPress);
					break;

				case 3:
					Key_Event (K_MOUSE2, x_event.type == ButtonPress);
					break;

				case 4:
					Key_Event (K_MWHEELUP, x_event.type == ButtonPress);
					break;

				case 5:
					Key_Event (K_MWHEELDOWN, x_event.type == ButtonPress);
					break;

				case 6:
					Key_Event (K_MOUSE4, x_event.type == ButtonPress);
					break;

				case 7:
					Key_Event (K_MOUSE5, x_event.type == ButtonPress);
					break;

				case 8:
					Key_Event (K_MOUSE6, x_event.type == ButtonPress);
					break;

				case 9:
					Key_Event (K_MOUSE7, x_event.type == ButtonPress);
					break;

				case 10:
					Key_Event (K_MOUSE8, x_event.type == ButtonPress);
					break;
				}
				break;

			case CreateNotify: // window created
				window_x = x_event.xcreatewindow.x;
				window_y = x_event.xcreatewindow.y;
				window_width = x_event.xcreatewindow.width;
				window_height = x_event.xcreatewindow.height;
				break;

			case ConfigureNotify: // window changed size/location
				window_x = x_event.xconfigure.x;
				window_y = x_event.xconfigure.y;
				window_width = x_event.xconfigure.width;
				window_height = x_event.xconfigure.height;

				// check for resize
				if (!vidmode_fullscreen)
				{
					if (window_width < 320)
						window_width = 320;
					if (window_height < 200)
						window_height = 200;

//					x_event.xconfigure.width = window_width;
//					x_event.xconfigure.height = window_height;

					vid.width = window_width;
					vid.height = window_height;

					vid.conwidth = vid.width;
					vid.conheight = vid.height;

					vid.recalc_refdef = true; // force a surface cache flush
				}
				break;

			case DestroyNotify: // window has been destroyed
				Sys_Quit (0);
				break; 

			case ClientMessage: // window manager messages
				if ((x_event.xclient.format == 32) && ((unsigned int)x_event.xclient.data.l[0] == wm_delete_window_atom))
					Sys_Quit (0);
				break; 

			case MapNotify: // window restored
			case UnmapNotify: // window iconified/rolledup/whatever
				vid_hiddenwindow = (x_event.type == UnmapNotify);
			case FocusIn: // window is now the input focus
			case FocusOut: // window is no longer the input focus

				if (x_event.type == FocusIn || x_event.type == FocusOut)
				{
					if (x_event.xfocus.mode == NotifyGrab || x_event.xfocus.mode == NotifyUngrab)
						continue;

					vid_activewindow = (x_event.type == FocusIn);

				}

//				switch (x_event.xfocus.mode)
//				{
//				case NotifyNormal:
//				case NotifyGrab:
//				case NotifyUngrab:
//					vid_activewindow = (x_event.type == FocusIn);
//					break;
//				}

				if(vidmode_fullscreen)
				{
					if(x_event.type == MapNotify)
					{
						// set our video mode
						XF86VidModeSwitchToMode(x_disp, scrnum, &game_vidmode);
	
						// move the viewport to top left
						XF86VidModeSetViewPort(x_disp, scrnum, 0, 0);
					}
					else if(x_event.type == UnmapNotify)
					{
						// set our video mode
						XF86VidModeSwitchToMode(x_disp, scrnum, &init_vidmode);
					}
				}
				else //if (!vidmode_fullscreen)
				{
					// enable/disable sound, set/restore gamma and grab/ungrab keyb
					// on focus gain/loss
					if (vid_activewindow && !vid_hiddenwindow)// && !active)
					{
						if (!active) {
						S_UnblockSound ();
						S_ClearBuffer ();
						VID_Gamma_Set ();
						IN_GrabKeyboard();
						active = true;
						printf("*** Active ***\n");
						}
					}
					else //if (active)
					{
						if (active) {
						S_BlockSound ();
						S_ClearBuffer ();
						VID_Gamma_Restore ();
						IN_UngrabKeyboard();
						active = false;
						printf("*** Inactive ***\n");
						}
					}
				}

				// fix the leftover Alt from any Alt-Tab or the like that switched us away
				Key_ClearStates ();
				break;

			case EnterNotify: // mouse entered window
			case LeaveNotify: // mouse left window
				vid_notifywindow = (x_event.type == EnterNotify);
				break;
			}
		}
	}
}

