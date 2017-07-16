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
NSScreen            *screen = nil;
CGDisplayModeRef    desktopMode;
CGDisplayModeRef    gameMode;

viddef_t vid; // global video state

cvar_t		vid_gamma = {"gamma", "1", true};


//==========================================================================
//
//  HARDWARE GAMMA
//
//==========================================================================

CGGammaValue	 vid_gammaramp[3][256];
CGGammaValue	 vid_systemgammaramp[3][256]; // to restore gamma
qboolean vid_gammaworks;

/*
================
VID_Gamma_Set

apply gamma correction
================
*/
void VID_Gamma_Set (void)
{
	if (!vid_gammaworks)
		return;
    
    CGError err = CGSetDisplayTransferByTable(display, 256, 
                                              vid_gammaramp[0], 
                                              vid_gammaramp[1], 
                                              vid_gammaramp[2]);
    if (err != kCGErrorSuccess)
        Con_Printf ("VID_Gamma_Set: Failed to set gamma table ramp\n");
}

/*
================
VID_Gamma_Restore

restore system gamma
================
*/
void VID_Gamma_Restore (void)
{
	if (!vid_gammaworks)
		return;

	CGError err = CGSetDisplayTransferByTable(display, 256, 
                                              vid_systemgammaramp[0], 
                                              vid_systemgammaramp[1], 
                                              vid_systemgammaramp[2]);
    if (err != kCGErrorSuccess)
        Con_Printf ("VID_Gamma_Restore: Failed to set gamma table ramp\n");
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
    int i;
	static float oldgamma;
    
	if (vid_gamma.value == oldgamma)
		return;
    
	oldgamma = vid_gamma.value;
    
    
    
    
//    vid_gamma.value = 0.7; // DEBUG
    
    
    
    // Refresh gamma (X11)
//	for (i=0; i<256; i++)
//		vid_gammaramp[0][i] = vid_gammaramp[1][i] = vid_gammaramp[2][i] =
//            CLAMP(0, (int) (255 * pow ((i+0.5)/255.5, vid_gamma.value) + 0.5), 255) << 8;
    
    
//    for (i=0; i<256; i++)
//        vid_gammaramp[0][i] = vid_gammaramp[1][i] = vid_gammaramp[2][i] =
//            CLAMP(0, (int)(65535 * pow ((i+0.5)/255.5, vid_gamma.value) + 0.5), 65535) / 65535.0;

//!    work!
    for (i=0; i<256; i++)
        vid_gammaramp[0][i] = vid_gammaramp[1][i] = vid_gammaramp[2][i] =
            CLAMP(0, (int)(255 * pow ((i+0.5)/255.5, vid_gamma.value) + 0.5), 255) / 255.0;
    
    
    
//    float value = vid_gamma.value;
//    value = CLAMP(0.5, value, 1.0);
//    value = ((1.4f - value) * 2.5f);
//    
//    for (i=0; i<256; i++)
//    {
//        vid_gammaramp[0][i] = value * vid_systemgammaramp[0][i];
//        vid_gammaramp[1][i] = value * vid_systemgammaramp[1][i];
//        vid_gammaramp[2][i] = value * vid_systemgammaramp[2][i];
//    }
    
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
    uint32_t capacity = CGDisplayGammaTableCapacity(display);
    uint32_t sampleCount;
    
	vid_gammaworks = (capacity == 256);
    if (vid_gammaworks) {
        CGError err = CGGetDisplayTransferByTable(display, capacity, 
                                                  vid_systemgammaramp[0], 
                                                  vid_systemgammaramp[1], 
                                                  vid_systemgammaramp[2], &sampleCount);
        if (err != kCGErrorSuccess)
            Con_Printf ("VID_Gamma_Init: Failed to get gamma table ramp\n");
    } else {
		Con_Printf ("Hardware gamma unavailable\n");
    }
    
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
//    [context flushBuffer];
	CGLFlushDrawable([context CGLContextObj]);
    
//    glClear(GL_DEPTH_BUFFER_BIT);
//    glClear (GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    
	if (fullsbardraw)
		Sbar_Changed();
}

//====================================

//NSOpenGLPixelFormat *OpenGLPixelFormat (int bpp, qboolean fullscreen)
//{
//    NSOpenGLPixelFormat *format = nil;
//    NSOpenGLPixelFormatAttribute attributes[32];
//    int i = 0;
//    
//    if (bpp < 16)
//        bpp = 16;
//    else if (bpp > 16)
//        bpp = 32;
//    
//    attributes[i++] = NSOpenGLPFANoRecovery;
//    attributes[i++] = NSOpenGLPFAMinimumPolicy;
//    attributes[i++] = NSOpenGLPFAAccelerated;
//    attributes[i++] = NSOpenGLPFADoubleBuffer;
//    
//    attributes[i++] = NSOpenGLPFADepthSize;
//    attributes[i++] = 1;
//    
//    attributes[i++] = NSOpenGLPFAAlphaSize;
//    attributes[i++] = 0;
//    
//    attributes[i++] = NSOpenGLPFAStencilSize;
//    attributes[i++] = 0;
//    
//    attributes[i++] = NSOpenGLPFAAccumSize;
//    attributes[i++] = 0;
//    
//    attributes[i++] = NSOpenGLPFAColorSize;
//    attributes[i++] = bpp;
//    
//    if (fullscreen) {
//        attributes[i++] = NSOpenGLPFAFullScreen;
//        attributes[i++] = NSOpenGLPFAScreenMask;
//        attributes[i++] = CGDisplayIDToOpenGLDisplayMask(display);
//    } else
//        attributes[i++] = NSOpenGLPFAWindow;
//    
//    attributes[i++] = 0;
//    
//    format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
//    
//    return format;
//}

void CGL_SwapInterval (qboolean enable)
{
    const GLint state = (enable) ? 1 : 0;
    
    [context makeCurrentContext];
    
    CGLError glerr = CGLSetParameter([context CGLContextObj], kCGLCPSwapInterval, &state);
    if (glerr == kCGLNoError) {
        Con_Printf ("%s CGL swap interval\n", (state == 1) ? "Enable" : "Disable");
    } else {
        Con_Warning ("Unable to set CGL swap interval\n");
    }
}

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
    
    CGError err;
    CGDirectDisplayID displays[MAX_DISPLAYS];
    uint32_t displayCount;
    uint32_t displayIndex = 0;
    int colorDepth = 32, refreshRate = 0;
    qboolean isStretched = false;
    
    // set vid parameters
	vid.width = 640;
	vid.height = 480;
	vid.numpages = 2;
    
    // Get the active display list
    err = CGGetActiveDisplayList(MAX_DISPLAYS, displays, &displayCount);
    if (err != kCGErrorSuccess)
        Sys_Error("Cannot get display list");
    
    // check for command-line display number
    if ((i = COM_CheckParm("-display"))) 
    {
		if (i >= com_argc-1)
			Sys_Error("VID_Init: -display <display>");
        
		displayIndex = atoi(com_argv[i+1]);
		if (displayIndex >= displayCount)
			Sys_Error("VID_Init: Bad display number");
    }
    
    // By default, we use the main screen
    display = displays[displayIndex];
    
    NSArray *screens = [NSScreen screens];
    if ([screens count] != displayCount) {
        Sys_Error("Wrong screen counts");
    }
    
    // get current screen from display
    screen = [screens objectAtIndex:displayIndex];
    if ([[[screen deviceDescription] objectForKey:@"NSScreenNumber"] unsignedIntValue] != display) {
        Sys_Error("Wrong screen ID");
    }
    
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
    
    if (fullscreen && (vid.width < 640 || vid.height < 480))
    {
        Con_Warning ("Fullscreen in low-res mode not available\n");
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
            if (!mode) {
                Sys_Error("Unable to find requested display mode");
            }
            
            // Make sure we get the right mode
            if ((int)CGDisplayModeGetWidth(mode) == vid.width && 
                (int)CGDisplayModeGetHeight(mode) == vid.height && 
                (int)[[(NSDictionary *)*((long *)mode + 2) objectForKey:(id)kCGDisplayBitsPerPixel] intValue] == colorDepth &&
                (int)CGDisplayModeGetRefreshRate(mode) == refreshRate &&
                (qboolean)((CGDisplayModeGetIOFlags(mode) & kDisplayModeStretchedFlag) == kDisplayModeStretchedFlag) == isStretched) {
                
                break; // we got it
            }
            
            bestModeIndex = modeIndex;
        }
        
        if (bestModeIndex == 0xFFFFFFFF) {
            Sys_Error("No suitable display mode available");
        }
        
        gameMode = mode;
        
        // Capture the main display
        err = CGDisplayCapture(display);
        if (err != kCGErrorSuccess)
            Sys_Error("Unable to capture display");
        
        // Switch to the correct resolution
        err = CGDisplaySetDisplayMode(display, gameMode, NULL);
        if (err != kCGErrorSuccess)
            Sys_Error("Unable to set display mode");
        
        vidmode_fullscreen = true;
        
    } else {
        gameMode = desktopMode;
    }
    
    // Get the GL pixel format
    NSOpenGLPixelFormatAttribute pixelAttributes[] = {
        NSOpenGLPFANoRecovery,      //0
        NSOpenGLPFAMinimumPolicy,   //1
        NSOpenGLPFAAccelerated,     //2
        NSOpenGLPFADoubleBuffer,    //3
        NSOpenGLPFADepthSize, 1,    //4 5
        NSOpenGLPFAAlphaSize, 0,    //6 7
        NSOpenGLPFAStencilSize, 0,  //8 9
        NSOpenGLPFAAccumSize, 0,    //10 11
        NSOpenGLPFAColorSize, 32,   //12 13
        0, 0, 0, 0                  //14 15 16 17 - reserved
    };
    
    if (colorDepth < 16)
        colorDepth = 16;
    else if (colorDepth > 16)
        colorDepth = 32;
    
    pixelAttributes[13] = colorDepth;
    
    if (fullscreen) {
        pixelAttributes[14] = NSOpenGLPFAFullScreen;
        pixelAttributes[15] = NSOpenGLPFAScreenMask;
        pixelAttributes[16] = CGDisplayIDToOpenGLDisplayMask(display);
    } else {
        pixelAttributes[14] = NSOpenGLPFAWindow;
    }
    
    NSOpenGLPixelFormat *pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:pixelAttributes];
//    pixelFormat = OpenGLPixelFormat(colorDepth, fullscreen);
    if (!pixelFormat) {
        Sys_Error("No pixel format found");
    }
    
    // Create a context with the desired pixel attributes
    context = [[NSOpenGLContext alloc] initWithFormat:pixelFormat shareContext:nil];
    if (!context) {
        Sys_Error("Cannot create OpenGL context");
    }
    
    if (!vidmode_fullscreen) {
        NSRect windowRect;
        
        // Create a window of the desired size
        windowRect.origin.x = ([screen frame].size.width - vid.width) / 2;
        windowRect.origin.y = ([screen frame].size.height - vid.height) / 2;
//        windowRect.origin.x = ((int)CGDisplayModeGetWidth(desktopMode) - vid.width) / 2;
//        windowRect.origin.y = ((int)CGDisplayModeGetHeight(desktopMode) - vid.height) / 2;
        windowRect.size.width = vid.width;
        windowRect.size.height = vid.height;
        
        window = [[NSWindow alloc] initWithContentRect:windowRect 
                                             styleMask:NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask
                                               backing:NSBackingStoreBuffered 
                                                 defer:NO 
                                                screen:screen];
        [window setTitle:@"fxQuake"];
//        [window center];

        
        NSLog(@"%@", NSStringFromRect(windowRect));

        
        NSRect bounds = [window frame];
        NSLog(@"%@", NSStringFromRect(bounds));

        NSRect bounds2 = [screen frame];
        NSLog(@"%@", NSStringFromRect(bounds2));

        
        NSRect bounds3 = [ [ window contentView ] frame ];
        NSLog(@"%@", NSStringFromRect(bounds3));
        
//        window - (NSRect)frameRectForContentRect:(NSRect)contentRect;
        NSRect bounds4 = [window frameRectForContentRect:bounds3];
        NSLog(@"%@", NSStringFromRect(bounds4));
        
        
//        [window orderFront:nil];
        [window makeKeyAndOrderFront: nil];
//        [window makeMainWindow];
        
        // Always get mouse moved events (if mouse support is turned off (rare) the event system will filter them out.
        [window setAcceptsMouseMovedEvents:YES];
        [window setDelegate:(id<NSWindowDelegate>)[[NSApplication sharedApplication] delegate]];
        
        // Direct the context to draw in this window
        [context setView:[window contentView]];
    } else {
        // Set the context to full screen
        CGLError glerr = CGLSetFullScreenOnDisplay([context CGLContextObj], CGDisplayIDToOpenGLDisplayMask(display));
        if (glerr) {
            Sys_Error("Cannot set fullscreen");
        }
    }
    
    [context makeCurrentContext];
    
    vid_activewindow = true;
	vid_hiddenwindow = false;

    vid.conwidth = vid.width;
	vid.conheight = vid.height;
    
    GL_Init();
    
    CGLError glerr = CGLEnable([context CGLContextObj], kCGLCEMPEngine);
    if (glerr == kCGLNoError) {
        Con_Printf("Enable multi-threaded OpenGL\n");
    }

//    CGL_SwapInterval(true); // DEBUG
    
	VID_Gamma_Init ();
    
    vid.recalc_refdef = true; // force a surface cache flush
    
	if (COM_CheckParm("-fullsbar"))
		fullsbardraw = true;
    
	Con_SafePrintf ("Video mode %dx%dx%d %dHz %s%s initialized\n", vid.width, vid.height, colorDepth, refreshRate, isStretched ? "(stretched) " : "", vidmode_fullscreen ? "fullscreen" : "windowed");
}

/*
===============
VID_Shutdown

called at shutdown
===============
*/
void VID_Shutdown (void)
{
    if (display) {
        if (context) {
            [NSOpenGLContext clearCurrentContext];
            CGLClearDrawable([context CGLContextObj]);
            [context clearDrawable];
            [context release];
            context = nil;
        }
        
        VID_Gamma_Shutdown ();
        
        if (window) {
//            [window close];
            [window release];
            window = nil;
        }
        
        // Switch back to the original screen resolution
        if (vidmode_fullscreen) {
            if (desktopMode) {
                CGDisplaySetDisplayMode(display, desktopMode, NULL);
            }
        }
        
        // Release the main display
        if (CGDisplayIsCaptured(display)) {
            CGDisplayRelease(display);
        }
    }
    
    vidmode_fullscreen = false;
}

