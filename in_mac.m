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

static const byte scantokey[128] = 
{
/* 0x00 */    'a',            's',            'd',            'f',            'h',            'g',            'z',            'x',
/* 0x08 */    'c',            'v',            '?',            'b',            'q',            'w',            'e',            'r',
/* 0x10 */    'y',            't',            '1',            '2',            '3',            '4',            '6',            '5',
/* 0x18 */    '=',            '9',            '7',            '-',            '8',            '0',            ']',            'o',
/* 0x20 */    'u',            '[',            'i',            'p',            K_ENTER,        'l',            'j',            '\'',
/* 0x28 */    'k',            ';',            '\\',           ',',            '/',            'n',            'm',            '.',
/* 0x30 */    K_TAB,          K_SPACE,        '`',            K_BACKSPACE,    '?',            K_ESCAPE,       '?',            K_COMMAND,
/* 0x38 */    K_SHIFT,        K_CAPSLOCK,     K_ALT,          K_CTRL,         '?',            '?',            '?',            '?',
/* 0x40 */    '?',            K_KP_DEL,       '?',            K_KP_STAR,      '?',            K_KP_PLUS,      '?',            K_KP_NUMLOCK,
/* 0x48 */    '?',            '?',            '?',            K_KP_SLASH,     K_KP_ENTER,     '?',            K_KP_MINUS,     '?',
/* 0x50 */    '?',            '?',            K_KP_INS,       K_KP_END,       K_KP_DOWNARROW, K_KP_PGDN,      K_KP_LEFTARROW, K_KP_5,
/* 0x58 */    K_KP_RIGHTARROW,K_KP_HOME,      '?',            K_KP_UPARROW,   K_KP_PGUP,      '?',            '?',            '?',
/* 0x60 */    K_F5,           K_F6,           K_F7,           K_F3,           K_F8,           K_F9,           '?',            K_F11,
/* 0x68 */    '?',            '?',            '?',            K_F14,          '?',            K_F10,          '?',            K_F12,
/* 0x70 */    '?',            K_F15,          K_INS,          K_HOME,         K_PGUP,         K_DEL,          K_F4,           K_END,
/* 0x78 */    K_F2,           K_PGDN,         K_F1,           K_LEFTARROW,    K_RIGHTARROW,   K_DOWNARROW,    K_UPARROW,      '?'
};

/*
 ===========
 EventToKey
 
 Transform from OSX event to Quake's symbols
 ===========
 */
int EventToKey (NSEvent *event)
{
	int key = 0;
    NSString *characters;
    NSUInteger characterCount;
    NSUInteger characterIndex;
    unichar character;
    
    characters = [event charactersIgnoringModifiers];
    characterCount = [characters length];
    for (characterIndex = 0; characterIndex < characterCount; characterIndex++) 
    {
        character = [characters characterAtIndex:characterIndex];
        switch (character) 
        {
        case NSPageUpFunctionKey:
            key = K_PGUP;
            break;
            
        case NSPageDownFunctionKey:
            key = K_PGDN;
            break;
            
        case NSHomeFunctionKey:
            key = K_HOME;
            break;
            
        case NSEndFunctionKey:
            key = K_END;
            break;
            
        case NSUpArrowFunctionKey:
            key = K_UPARROW;
            break;
            
        case NSDownArrowFunctionKey:
            key = K_DOWNARROW;
            break;
            
        case NSLeftArrowFunctionKey:
            key = K_LEFTARROW;
            break;
            
        case NSRightArrowFunctionKey:
            key = K_RIGHTARROW;
            break;
            
        case '\033':
            key = K_ESCAPE;
            break;
            
        case 0x03:
        case '\n':
            key = K_ENTER;
            break;
            
        case '\t':
            key = K_TAB;
            break;
            
        case NSF1FunctionKey:
            key = K_F1;
            break;
            
        case NSF2FunctionKey:
            key = K_F2;
            break;
            
        case NSF3FunctionKey:
            key = K_F3;
            break;
            
        case NSF4FunctionKey:
            key = K_F4;
            break;
            
        case NSF5FunctionKey:
            key = K_F5;
            break;
            
        case NSF6FunctionKey:
            key = K_F6;
            break;
            
        case NSF7FunctionKey:
            key = K_F7;
            break;
            
        case NSF8FunctionKey:
            key = K_F8;
            break;
            
        case NSF9FunctionKey:
            key = K_F9;
            break;
            
        case NSF10FunctionKey:
            key = K_F10;
            break;
            
        case NSF11FunctionKey:
            key = K_F11;
            break;
            
        case NSF12FunctionKey:
            key = K_F12;
            break;
            
        case '\b':
        case '\177':
            key = K_BACKSPACE;
            break;
            
        case NSDeleteFunctionKey:
            key = K_DEL;
            break;
            
        case NSPauseFunctionKey:
            key = K_PAUSE;
            break;
            
        case NSInsertFunctionKey:
            key = K_INS;
            break;
            
        default:
            key = character;
            if ((key & 0xFF00) == 0xF700) 
                key -= 0xF700;
            else
            {
                if (key < 0x80)
                    if ((key >= 'A') && (key <= 'Z'))
                        key += 'a' - 'A';
            }
            break;
        }
    }
    
    return key;
}

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

    
    
//    NSString *characters;
//    NSUInteger characterCount;
//    NSUInteger characterIndex;
//    unichar character;
    
	NSEvent *event;
    while ((event = [NSApp nextEventMatchingMask:NSAnyEventMask 
                                       untilDate:[NSDate distantPast] 
                                          inMode:NSDefaultRunLoopMode 
                                         dequeue:YES])) 
    {
        NSEventType eventType = [event type];
        switch (eventType) 
        {
//            /* These six event types are ignored since we do all of our mouse down/up process via the uber-mouse system defined event. 
//                We have to accept these events however since they get enqueued and the queue will fill up if we don't. */
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
//                
//            case NSMouseMoved:
//            case NSLeftMouseDragged:
//            case NSRightMouseDragged:
//            case NSOtherMouseDragged:   // other mouse dragged
//                return;
                
            case NSKeyDown: // key pressed
            case NSKeyUp: // key released
                
                Key_Event(EventToKey(event), eventType == NSKeyDown);
                
//                characters = [event charactersIgnoringModifiers];
//                characterCount = [characters length];
//                for (characterIndex = 0; characterIndex < characterCount; characterIndex++) 
//                {
//                    character = [characters characterAtIndex:characterIndex];
//                    
//                    Key_Event (character, eventType == NSKeyDown);
//                    
//                }
                
                return;
                
//            case NSFlagsChanged:
//                return;
//            case NSSystemDefined:
//                return;
//            case NSScrollWheel:
//                return;
                
                
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
                
                
            default:
                break;
        }
        
        [NSApp sendEvent:event];
    }
    
	
}

