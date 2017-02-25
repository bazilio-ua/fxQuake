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
// vid_cgl.m -- Core OpenGL interface to the OS X implementation of the OpenGL specification 

#include "quakedef.h"
#include "unixquake.h"
#include "macquake.h"

CGDirectDisplayID   display;
NSOpenGLContext     *context = nil;
NSWindow            *window = nil;

viddef_t vid; // global video state

cvar_t		vid_gamma = {"gamma", "1", true};


//==========================================================================
//
//  HARDWARE GAMMA
//
//==========================================================================


/*
================
VID_Gamma_Set

apply gamma correction
================
*/
void VID_Gamma_Set (void)
{
	
}

/*
================
VID_Gamma_Restore

restore system gamma
================
*/
void VID_Gamma_Restore (void)
{
	
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
	Cvar_RegisterVariable (&vid_gamma, VID_Gamma);
}

//====================================

/*
=================
GL_BeginRendering

sets values of glx, gly, glwidth, glheight
=================
*/
inline void GL_BeginRendering (int *x, int *y, int *width, int *height)
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
inline void GL_EndRendering (void)
{
	
}

//====================================

#define MAX_DISPLAYS 128

/*
===============
VID_Init
===============
*/
void VID_Init (void)
{
    CGDisplayErr err;
    CGDirectDisplayID displays[MAX_DISPLAYS];
    uint32_t displayCount;
    uint32_t displayIndex;
    
    NSOpenGLPixelFormatAttribute pixelAttributes[] = {
        NSOpenGLPFAMinimumPolicy,
        NSOpenGLPFAAccelerated,
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFADepthSize, 24,
        NSOpenGLPFAAlphaSize, 8,
        NSOpenGLPFAStencilSize, 8,
        NSOpenGLPFAColorSize, 32,
        NSOpenGLPFASampleBuffers, 1,
        NSOpenGLPFASamples, 8,
        0
    };
    
    NSOpenGLPixelFormat *pixelFormat = nil;
    
    // set vid parameters
	vid.width = 640;
	vid.height = 480;
	vid.numpages = 2;
    
    // Get the active display list
    err = CGGetActiveDisplayList(MAX_DISPLAYS, displays, &displayCount);
    if (err != CGDisplayNoErr)
        Sys_Error("Cannot get display list -- CGGetActiveDisplayList returned %d.", err);
    
    // By default, we use the main screen
    display = displays[0];
    
    // Get the GL pixel format
    pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:pixelAttributes];
    if (!pixelFormat) {
        Sys_Error("No pixel format found");
    }
    
    // Create a context with the desired pixel attributes
    context = [[NSOpenGLContext alloc] initWithFormat:pixelFormat shareContext:nil];
    
    
	VID_Gamma_Init ();
}

/*
===============
VID_Shutdown

called at shutdown
===============
*/
void VID_Shutdown (void)
{
	
}

