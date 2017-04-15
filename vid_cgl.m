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
CGDisplayModeRef    desktopMode;
CGDisplayModeRef    gameMode;

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
    int i;
	qboolean fullscreen = true;
    
    CGDisplayErr err;
    CGDirectDisplayID displays[MAX_DISPLAYS];
    uint32_t displayCount;
    __unused uint32_t displayIndex;
    int colorDepth = 32, refreshRate = 0;
    qboolean isStretched = false;
    
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
        Sys_Error("Cannot get display list");
    
    // By default, we use the main screen
    display = displays[0];
    
    // get current mode
    desktopMode = CGDisplayCopyDisplayMode(display);
    if (!desktopMode) {
        Sys_Error("Could not get current graphics mode for display");
    }
    
    // check for command-line size parameters
	if ((i = COM_CheckParm("-width"))) 
	{
		if (i >= com_argc-1)
			Sys_Error("VID_Init: -width <width>");
        
		vid.width = atoi(com_argv[i+1]);
		if (!vid.width)
			Sys_Error("VID_Init: Bad width");
		if (vid.width < 320)
			Sys_Error("VID_Init: width < 320 is not supported");
	}
	if ((i = COM_CheckParm("-height"))) 
	{
		if (i >= com_argc-1)
			Sys_Error("VID_Init: -height <height>");
        
		vid.height = atoi(com_argv[i+1]);
		if (!vid.height)
			Sys_Error("VID_Init: Bad height");
		if (vid.height < 200)
			Sys_Error("VID_Init: height < 200 is not supported");
	}
    
	if ((i = COM_CheckParm("-bpp"))) 
    {
		if (i >= com_argc-1)
			Sys_Error("VID_Init: -bpp <bpp>");
        
        colorDepth = atoi(com_argv[i+1]);
    } else {
        colorDepth = (int)[[(NSDictionary *)*((long *)desktopMode + 2) objectForKey:(id)kCGDisplayBitsPerPixel] intValue];
    }
    
	if ((i = COM_CheckParm("-refreshrate"))) 
    {
		if (i >= com_argc-1)
			Sys_Error("VID_Init: -refreshrate <refreshrate>");
        
        refreshRate = atoi(com_argv[i+1]);
    } else {
        refreshRate = (int)CGDisplayModeGetRefreshRate(desktopMode);
    }
    
	if ((i = COM_CheckParm("-stretched"))) 
    {
        isStretched = true;
    } else {
        isStretched = (qboolean)(CGDisplayModeGetIOFlags(desktopMode) & kDisplayModeStretchedFlag) == kDisplayModeStretchedFlag;
    }
    
    // check for command-line video parameters
	if (COM_CheckParm("-current"))
	{
		vid.width = (int)CGDisplayModeGetWidth(desktopMode);
		vid.height = (int)CGDisplayModeGetHeight(desktopMode);
        colorDepth = (int)[[(NSDictionary *)*((long *)desktopMode + 2) objectForKey:(id)kCGDisplayBitsPerPixel] intValue];
        refreshRate = (int)CGDisplayModeGetRefreshRate(desktopMode);
        isStretched = (qboolean)(CGDisplayModeGetIOFlags(desktopMode) & kDisplayModeStretchedFlag) == kDisplayModeStretchedFlag;
	}
	else if (COM_CheckParm("-window"))
	{
		fullscreen = false;
	}
    
    // get video mode list
    CFArrayRef displayModes = CGDisplayCopyAllDisplayModes(display, NULL);
    if (!displayModes) {
        Sys_Error("Display available modes returned NULL");
    }
    
    if (fullscreen) {
        CGDisplayModeRef mode;
        CFIndex modeCount, modeIndex, bestModeIndex;
        modeCount = CFArrayGetCount(displayModes);
        
        // Default to the current desktop mode
        bestModeIndex = 0xFFFFFFFF;
        
        for (modeIndex = 0; modeIndex < modeCount; modeIndex++) {
            mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(displayModes, modeIndex);
            
            // Make sure we get the right size
            if ((int)CGDisplayModeGetWidth(mode) != vid.width || (int)CGDisplayModeGetHeight(mode) != vid.height) {
                continue;
            }
            
            if ((int)[[(NSDictionary *)*((long *)mode + 2) objectForKey:(id)kCGDisplayBitsPerPixel] intValue] != colorDepth) {
                continue;
            }
            
            if ((int)CGDisplayModeGetRefreshRate(mode) != refreshRate) {
                continue;
            }
            
            if ((qboolean)((CGDisplayModeGetIOFlags(mode) & kDisplayModeStretchedFlag) == kDisplayModeStretchedFlag) != isStretched) {
                continue;
            }
            
            bestModeIndex = modeIndex;
        }
        
        if (bestModeIndex == 0xFFFFFFFF) {
            Sys_Error("No suitable display mode available");
        }
        
        if (!mode) {
            Sys_Error("Unable to find requested display mode");
        }
        
        gameMode = mode;
    } else {
        gameMode = desktopMode;
    }
    
    // Get the GL pixel format
    pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:pixelAttributes];
    if (!pixelFormat) {
        Sys_Error("No pixel format found");
    }
    
    // Create a context with the desired pixel attributes
    context = [[NSOpenGLContext alloc] initWithFormat:pixelFormat shareContext:nil];
    if (!context) {
        Sys_Error("Cannot create OpenGL context");
    }
    
    if (!fullscreen) {
        NSRect windowRect;
        
        // Create a window of the desired size
        windowRect.origin.x = ((int)CGDisplayModeGetWidth(desktopMode) - vid.width) / 2;
        windowRect.origin.y = ((int)CGDisplayModeGetHeight(desktopMode) - vid.height) / 2;
        windowRect.size.width = vid.width;
        windowRect.size.height = vid.height;
        
        window = [[NSWindow alloc] initWithContentRect:windowRect 
                                             styleMask:NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask
                                               backing:NSBackingStoreBuffered // NSBackingStoreRetained 
                                                 defer:NO];
        [window setTitle:@"fxQuake"];
        [window orderFront:nil];
        // Always get mouse moved events (if mouse support is turned off (rare) the event system will filter them out.
        [window setAcceptsMouseMovedEvents:YES];
        
        // Direct the context to draw in this window
        [context setView:[window contentView]];
    } else {
        CGError err;
        err = CGLSetFullScreenOnDisplay([context CGLContextObj], CGDisplayIDToOpenGLDisplayMask(display));
        if (err) {
            Sys_Error("Cannot set fullscreen");
        }
    }
    
    vid.conwidth = vid.width;
	vid.conheight = vid.height;

	VID_Gamma_Init ();
    
    vid.recalc_refdef = true; // force a surface cache flush
    
	if (COM_CheckParm("-fullsbar"))
		fullsbardraw = true;
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

