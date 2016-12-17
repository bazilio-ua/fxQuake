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
// in_mac.m -- Mac input/output driver code

#include "quakedef.h"
#include "unixquake.h"
#include "macquake.h"


qboolean mouse_available;		// Mouse available for use
qboolean keyboard_available;	// Keyboard available for use

qboolean mouse_grab_active, keyboard_grab_active;
qboolean dga_mouse_available, dga_keyboard_available;
qboolean dga_mouse_active, dga_keyboard_active;

float mouse_x=0, mouse_y=0;
static float old_mouse_x, old_mouse_y;

cvar_t m_filter = {"m_filter", "0", true};

qboolean vidmode_fullscreen = false; // was vidmode_active

qboolean	vid_activewindow;
qboolean	vid_hiddenwindow;

/*
 ===========
 IN_GrabMouse
 ===========
 */
void IN_GrabMouse (void)
{
    
}

/*
 ===========
 IN_UngrabMouse
 ===========
 */
void IN_UngrabMouse (void)
{
    
}

/*
 ===========
 IN_GrabKeyboard
 ===========
 */
void IN_GrabKeyboard (void)
{
    
}

/*
 ===========
 IN_UngrabKeyboard
 ===========
 */
void IN_UngrabKeyboard (void)
{
    
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
	
}

/*
===========
IN_Shutdown
===========
*/
void IN_Shutdown (void)
{
	
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
	
}

/*
===========
IN_Move
===========
*/
void IN_Move (usercmd_t *cmd)
{
	
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
    
	NSEvent *event;
    NSDate *date;
    
//    date = distantPast;
    
    date = [NSDate date];
    do {
        event = [NSApp nextEventMatchingMask:NSAnyEventMask 
                                   untilDate:[NSDate distantPast]//date 
                                      inMode:NSDefaultRunLoopMode 
                                     dequeue:YES];
        if (event) {
            [NSApp sendEvent:event];
        }
    } while (event);
	
}

