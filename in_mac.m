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
//qboolean dga_mouse_available, dga_keyboard_available;
//qboolean dga_mouse_active, dga_keyboard_active;

float mouse_x=0, mouse_y=0;
static float old_mouse_x, old_mouse_y;

cvar_t m_filter = {"m_filter", "0", true};

qboolean vidmode_fullscreen = false; // was vidmode_active

qboolean	vid_activewindow;
qboolean	vid_hiddenwindow;

static const byte scantokey[128] = 
{
/* 0x00 */  'a',            's',            'd',            'f',            'h',            'g',            'z',            'x',
/* 0x08 */  'c',            'v',             0,             'b',            'q',            'w',            'e',            'r',
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
	Cvar_RegisterVariable (&m_filter, NULL);
    
	Cmd_AddCommand ("force_centerview", Force_CenterView_f);
    
	if (COM_CheckParm ("-nomouse"))
		mouse_available = false;
	else
		mouse_available = true;
    
	mouse_grab_active = false;
//	dga_mouse_available = false;
//	dga_mouse_active = false;
    
/*  if (COM_CheckParm ("-nokeyb"))
        keyboard_available = false;
    else
*/      keyboard_available = true;
    
	keyboard_grab_active = false;
//	dga_keyboard_available = false;
//	dga_keyboard_active = false;
	
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
	float	mx, my;
    
//	if (!mouse_grab_active)
//		return;
    
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
    
    
    
//    NSEvent *event = [NSApp nextEventMatchingMask:NSAnyEventMask 
//                                        untilDate:[NSDate distantPast]
//                                           inMode:NSDefaultRunLoopMode 
//                                          dequeue:YES];
//    
//    NSEventType eventType = [event type];
//    
//    switch (eventType) {
//            
//        default:
//            break;
//    }
//    
//    [NSApp sendEvent:event];

    
    
    
	NSEvent *event;
    while ((event = [NSApp nextEventMatchingMask:NSAnyEventMask 
                                       untilDate:[NSDate distantPast] 
                                          inMode:NSDefaultRunLoopMode 
                                         dequeue:YES])) 
    {
        NSEventType eventType = [event type];
        switch (eventType) 
        {
//        These six event types are ignored since we do all of our mouse down/up process via the uber-mouse system defined event. 
//        We have to accept these events however since they get enqueued and the queue will fill up if we don't. 
//            case NSLeftMouseDown:
//                return;
//            case NSLeftMouseUp:
//                return;
//            case NSRightMouseDown:
//                return;
//            case NSRightMouseUp:
//                return;
//            case NSOtherMouseDown:  // other mouse down
//                return;
//            case NSOtherMouseUp:    // other mouse up
//                return;
            
            case NSMouseMoved: // mouse moved
            case NSLeftMouseDragged:
            case NSRightMouseDragged:
            case NSOtherMouseDragged:   // other mouse dragged
                {
//                    if (mouse_grab_active) 
                    {
                        static int32_t	dx, dy;
                        
                        CGGetLastMouseDelta (&dx, &dy);
                        
                        mouse_x = (float)dx;
                        mouse_y = (float)dy;
                        
//						if (mouse_x || mouse_y) // do warp
//						{
//							// move the mouse to the window center again
//                            
//                            
//                            CGPoint center;
//                            // just center at the middle of the screen:
//                            center = CGPointMake ((float) (vid.width >> 1), (float) (vid.height >> 1));
//                            // and go:
//                            CGDisplayMoveCursorToPoint (kCGDirectMainDisplay, center);
//
//						}
                        
                    }
                }
                return;
                
            case NSKeyDown: // key pressed
            case NSKeyUp: // key released
                {
                    unsigned short vkey = [event keyCode];
                    int key = (byte)scantokey[vkey];
                    
                    Key_Event(key, eventType == NSKeyDown);
                }   
                return;
                
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
                return;
                
            case NSSystemDefined:
                {
//                    if (mouse_grab_active)
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
                }
                return;
                
            case NSScrollWheel: // scroll wheel
                {
//                    if (mouse_grab_active) 
                    {
                        if ([event deltaY] < 0.0)
                        {
                            Key_Event (K_MWHEELDOWN, true);
                            Key_Event (K_MWHEELDOWN, false);
                        }
                        else
                        {
                            Key_Event (K_MWHEELUP, true);
                            Key_Event (K_MWHEELUP, false);
                        }
                    }
                }
                return;
                
            default:
                break;
        }
        
        [NSApp sendEvent:event];
    }
    
	
}

