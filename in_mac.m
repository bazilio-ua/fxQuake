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

qboolean mouse_active;
qboolean do_warp;

float mouse_x=0, mouse_y=0;
static float old_mouse_x, old_mouse_y;

cvar_t m_filter = {"m_filter", "0", CVAR_ARCHIVE};

//qboolean vidmode_fullscreen = false; // was vidmode_active

// if window is not the active window, don't hog as much CPU time,
// let go of the mouse, turn off sound, and restore system gamma ramps...
qboolean vid_activewindow; 
// if window is hidden, don't update screen
qboolean vid_hiddenwindow;
// if mouse entered/leaved window
qboolean vid_notifywindow;

static const byte scantokey[128] = 
{
/* 0x00 */  'a',            's',            'd',            'f',            'h',            'g',            'z',            'x',
/* 0x08 */  'c',            'v',            '`',            'b',            'q',            'w',            'e',            'r',
/* 0x10 */  'y',            't',            '1',            '2',            '3',            '4',            '6',            '5',
/* 0x18 */  '=',            '9',            '7',            '-',            '8',            '0',            ']',            'o',
/* 0x20 */  'u',            '[',            'i',            'p',            K_ENTER,        'l',            'j',            '\'',
/* 0x28 */  'k',            ';',            '\\',           ',',            '/',            'n',            'm',            '.',
/* 0x30 */  K_TAB,          K_SPACE,        '`',            K_BACKSPACE,    0,              K_ESCAPE,       0,              K_COMMAND,
/* 0x38 */  K_SHIFT,        K_CAPSLOCK,     K_ALT,          K_CTRL,         0,              0,              0,              0,
/* 0x40 */  0,              K_KP_DEL,       0,              K_KP_STAR,      0,              K_KP_PLUS,      0,              K_KP_NUMLOCK,
/* 0x48 */  0,              0,              0,              K_KP_SLASH,     K_KP_ENTER,     0,              K_KP_MINUS,     0,
/* 0x50 */  0,              0,              K_KP_INS,       K_KP_END,       K_KP_DOWNARROW, K_KP_PGDN,      K_KP_LEFTARROW, K_KP_5,
/* 0x58 */  K_KP_RIGHTARROW,K_KP_HOME,      0,              K_KP_UPARROW,   K_KP_PGUP,      0,              0,              0,
/* 0x60 */  K_F5,           K_F6,           K_F7,           K_F3,           K_F8,           K_F9,           0,              K_F11,
/* 0x68 */  0,              0,              0,              K_F14,          0,              K_F10,          0,              K_F12,
/* 0x70 */  0,              K_F15,          K_INS,          K_HOME,         K_PGUP,         K_DEL,          K_F4,           K_END,
/* 0x78 */  K_F2,           K_PGDN,         K_F1,           K_LEFTARROW,    K_RIGHTARROW,   K_DOWNARROW,    K_UPARROW,      0
};

/*
===========
IN_ActivateMouse
===========
*/
void IN_ActivateMouse (void)
{
	if (mouse_available && !mouse_active)
	{
        // hide cursor
        CGDisplayHideCursor(kCGNullDirectDisplay);
        // grab pointer
        CGAssociateMouseAndMouseCursorPosition(false);
        
        CGPoint center;
		if (vid.fullscreen) {
//        if (vidmode_fullscreen) {
            CGRect bounds = CGDisplayBounds(display);
            
            // just center at the middle of the screen
            center.x = bounds.origin.x + bounds.size.width / 2;
            center.y = bounds.origin.y + bounds.size.height / 2;
        } else {
            NSRect screenFrame = [screen frame];
            NSRect windowFrame = [window frame];
            NSRect contentFrame = [[window contentView] frame];
            
            // calculate the window center
            center.x = windowFrame.origin.x + contentFrame.size.width / 2;
            center.y = -windowFrame.origin.y + screenFrame.size.height - contentFrame.size.height / 2 + screenFrame.origin.y;
        }
        // move the mouse to the window center again                  
        CGWarpMouseCursorPosition(center);
		do_warp = true;
		
		mouse_active = true;
	}
}

/*
===========
IN_DeactivateMouse
===========
*/
void IN_DeactivateMouse (void)
{
    if (mouse_available && mouse_active)
	{
        // ungrab pointer
        CGAssociateMouseAndMouseCursorPosition(true);
        // show cursor
        CGDisplayShowCursor(kCGNullDirectDisplay);
        
		mouse_active = false;
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
	Cvar_RegisterVariable (&m_filter);
    
	Cmd_AddCommand ("force_centerview", Force_CenterView_f);
    
	if (COM_CheckParm ("-nomouse"))
		mouse_available = false;
	else
		mouse_available = true;
    
	mouse_active = false;
    
	IN_ActivateMouse(); // grab mouse first!
}

/*
===========
IN_Shutdown
===========
*/
void IN_Shutdown (void)
{
	IN_DeactivateMouse();
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
    
	if (!mouse_active)
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
	NSEvent *event;
    while ( (event = [NSApp nextEventMatchingMask:NSAnyEventMask 
                                        untilDate:[NSDate distantPast] 
                                           inMode:NSDefaultRunLoopMode 
                                          dequeue:YES]) ) 
    {
        NSEventType eventType = [event type];
        switch (eventType) 
        {
        // These six event types are ignored since we do all of our mouse down/up process via the uber-mouse system defined event. 
        // We have to accept these events however since they get enqueued and the queue will fill up if we don't. 
    /*
        case NSLeftMouseDown:
            break;
        case NSLeftMouseUp:
            break;
        case NSRightMouseDown:
            break;
        case NSRightMouseUp:
            break;
        case NSOtherMouseDown:  // other mouse down
            break;
        case NSOtherMouseUp:    // other mouse up
            break;
    */
        case NSMouseMoved: // mouse moved
        case NSLeftMouseDragged:
        case NSRightMouseDragged:
        case NSOtherMouseDragged: // other mouse dragged
			if (do_warp)
				do_warp = false;
            else if (mouse_active)
            {
                mouse_x = [event deltaX];
                mouse_y = [event deltaY];
            }
            break;
            
        case NSKeyDown: // key pressed
        case NSKeyUp: // key released
            {
                unsigned short vkey = [event keyCode];
                int key = (byte)scantokey[vkey];
                
                Key_Event(key, eventType == NSKeyDown);
            }   
            break;
            
        case NSFlagsChanged: // special keys
            {
                static NSUInteger lastFlags = 0;
                const NSUInteger flags = [event modifierFlags];
                const NSUInteger filteredFlags = flags ^ lastFlags;
                
                lastFlags = flags;
                
                if (filteredFlags & NSAlphaShiftKeyMask)
                    Key_Event (K_CAPSLOCK, (flags & NSAlphaShiftKeyMask) ? true : false);
                
                if (filteredFlags & NSShiftKeyMask)
                    Key_Event (K_SHIFT, (flags & NSShiftKeyMask) ? true : false);
                
                if (filteredFlags & NSControlKeyMask)
                    Key_Event (K_CTRL, (flags & NSControlKeyMask) ? true : false);
                
                if (filteredFlags & NSAlternateKeyMask)
                    Key_Event (K_ALT, (flags & NSAlternateKeyMask) ? true : false);
                
                if (filteredFlags & NSCommandKeyMask)
                    Key_Event (K_COMMAND, (flags & NSCommandKeyMask) ? true : false);
                
                if (filteredFlags & NSNumericPadKeyMask)
                    Key_Event (K_NUMLOCK, (flags & NSNumericPadKeyMask) ? true : false);
            }
            break;
            
        case NSSystemDefined:
            if (mouse_active)
            {
                static NSInteger oldButtons = 0;
                NSInteger buttonsDelta;
                NSInteger buttons;
                qboolean isDown;
                
                if ([event subtype] == 7) 
                {
                    buttons = [event data2];
                    buttonsDelta = oldButtons ^ buttons;
                    
                    if (buttonsDelta & 1) {
                        isDown = buttons & 1;
                        Key_Event (K_MOUSE1, isDown);
                    }
                    
                    if (buttonsDelta & 2) {
                        isDown = buttons & 2;
                        Key_Event (K_MOUSE2, isDown);
                    }
                    
                    if (buttonsDelta & 4) {
                        isDown = buttons & 4;
                        Key_Event (K_MOUSE3, isDown);
                    }
                    
                    if (buttonsDelta & 8) {
                        isDown = buttons & 8;
                        Key_Event (K_MOUSE4, isDown);
                    }
                    
                    if (buttonsDelta & 16) {
                        isDown = buttons & 16;
                        Key_Event (K_MOUSE5, isDown);
                    }
                    
                    oldButtons = buttons;
                }
            }
            break;
            
        case NSScrollWheel: // scroll wheel
            //if (mouse_active) 
            {
                if ([event deltaY] > 0)
                {
					Key_Event (K_MWHEELUP, true);
					Key_Event (K_MWHEELUP, false);
                }
                else if ([event deltaY] < 0)
                {
					Key_Event (K_MWHEELDOWN, true);
					Key_Event (K_MWHEELDOWN, false);
                }
            }
            break;
            
        case NSMouseEntered:
        case NSMouseExited:
            vid_notifywindow = (eventType == NSMouseEntered);
            break;
            
        default:
            [NSApp sendEvent:event];
            break;
        }
    }
    
    // handle the mouse state when windowed if that's changed
	if (!vid.fullscreen)
//	if (!vidmode_fullscreen)
	{
		if ( key_dest == key_game && !mouse_active && vid_activewindow )
//		if ( key_dest != key_console && !mouse_active && vid_activewindow )
		{
			IN_ActivateMouse ();
		}
		else if ( key_dest != key_game && mouse_active ) 
//		else if ( key_dest == key_console && mouse_active ) 
		{
			IN_DeactivateMouse ();
		}
	}
    
}

