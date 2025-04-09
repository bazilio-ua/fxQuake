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
NSOpenGLContext     *glcontext = nil;
NSWindow            *window = nil;
NSScreen            *screen = nil;
CGDisplayModeRef    desktopMode;

CFArrayRef          displayModes;
CFIndex             displayModesCount;

viddef_t vid; // global video state

qboolean vid_locked = false; //johnfitz
qboolean vid_changed = false;

qboolean	vid_initialized = false;

//====================================

//johnfitz -- new cvars
cvar_t		vid_fullscreen = {"vid_fullscreen", "0", CVAR_ARCHIVE};	// QuakeSpasm, was "1"
cvar_t		vid_width = {"vid_width", "640", CVAR_ARCHIVE};		// QuakeSpasm, was 800
cvar_t		vid_height = {"vid_height", "480", CVAR_ARCHIVE};	// QuakeSpasm, was 600
cvar_t		vid_bpp = {"vid_bpp", "32", CVAR_ARCHIVE};
cvar_t		vid_refreshrate = {"vid_refreshrate", "60", CVAR_ARCHIVE};
cvar_t		vid_stretched = {"vid_stretched", "0", CVAR_ARCHIVE};
//johnfitz

//====================================

/*
=================
GL_BeginRendering

sets values of glx, gly, glwidth, glheight
=================
*/
void GL_BeginRendering (int *x, int *y, int *width, int *height)
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
void GL_EndRendering (void)
{
	[glcontext flushBuffer];
	
	if (fullsbardraw)
		Sbar_Changed();
}

//====================================

int DisplayModeGetBitsPerPixel (CGDisplayModeRef mode)
{
	CFStringRef encoding = CGDisplayModeCopyPixelEncoding(mode);
	int bpp = 0;
	
	if (CFStringCompare(encoding, CFSTR(IO32BitDirectPixels), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
		bpp = 32;
	else if (CFStringCompare(encoding, CFSTR(IO16BitDirectPixels), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
		bpp = 16;
	
	CFRelease(encoding);
	
	return bpp;
}

int DisplayModeGetRefreshRate (CGDisplayModeRef mode)
{
	return Q_rint(CGDisplayModeGetRefreshRate(mode));
}

qboolean DisplayModeGetStretchedFlag (CGDisplayModeRef mode)
{
	return (CGDisplayModeGetIOFlags(mode) & kDisplayModeStretchedFlag) == kDisplayModeStretchedFlag;
}

/*
================
VID_GetMatchingDisplayMode
================
*/
CGDisplayModeRef VID_GetMatchingDisplayMode (int width, int height, int refreshrate, int bpp, qboolean stretched)
{
	CGDisplayModeRef mode;
	CFIndex modeIndex;
	
	// Default to the current desktop mode
	
	for (modeIndex = 0; modeIndex < displayModesCount; modeIndex++)
	{
		mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(displayModes, modeIndex);
		if (!mode)
			Sys_Error("Unable to find requested display mode");
		
		// Make sure we get the right mode
		if ((int)CGDisplayModeGetWidth(mode) == width &&
			(int)CGDisplayModeGetHeight(mode) == height &&
			DisplayModeGetRefreshRate(mode) == refreshrate &&
			DisplayModeGetBitsPerPixel(mode) == bpp &&
			DisplayModeGetStretchedFlag(mode) == stretched)
		{
			return mode; // we got it
		}
	}
	
	return NULL;
}

/*
================
VID_CheckMode
================
*/
qboolean VID_CheckMode (int width, int height, int refreshrate, int bpp, qboolean fullscreen, qboolean stretched)
{
	if (width < 320)
		return false;
	
	if (height < 200)
		return false;
	
	if (!fullscreen && stretched)
		return false;
	
	if (fullscreen && (width < 640 || height < 480))
		return false;
	
	switch (bpp)
	{
		case 32:
		case 16:
			break;
		default:
			return false;
	}
	
	if (fullscreen && !VID_GetMatchingDisplayMode (width, height, refreshrate, bpp, stretched))
		return false;
	
	return true;
}

/*
================
VID_SetMode
================
*/
void VID_SetMode (int width, int height, int refreshrate, int bpp, qboolean fullscreen, qboolean stretched)
{
	
	int		temp;
	int		depth, stencil;
	
	CGError err;
	
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	// so Con_Printfs don't mess us up by forcing vid and snd updates
	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;
	
	
	
	if (glcontext) {
		[NSOpenGLContext clearCurrentContext];
		
		[glcontext clearDrawable];
		
		[glcontext release];
		glcontext = nil;
	}
	
	
	if (bpp == 32) {
		depth = 24;
		stencil = 8;
	} else { // bpp == 16
		depth = 16;
		stencil = 0;
	}
	
	// Get the GL pixel format
	NSOpenGLPixelFormatAttribute pixelAttributes[] = {
		NSOpenGLPFAAllowOfflineRenderers,
		NSOpenGLPFANoRecovery,
		NSOpenGLPFAClosestPolicy,
		NSOpenGLPFAAccelerated,
		NSOpenGLPFADoubleBuffer,
		
		NSOpenGLPFADepthSize, depth,
		NSOpenGLPFAAlphaSize, 0,
		NSOpenGLPFAStencilSize, stencil,
		NSOpenGLPFAAccumSize, 0,
		
		NSOpenGLPFAColorSize, bpp,
		NSOpenGLPFAScreenMask, CGDisplayIDToOpenGLDisplayMask(display),
		0
	};
	
	
	NSOpenGLPixelFormat *pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:pixelAttributes];
	if (!pixelFormat)
		Sys_Error("Unable to find a matching pixel format");
	
	// Create a context with the desired pixel attributes
	glcontext = [[NSOpenGLContext alloc] initWithFormat:pixelFormat shareContext:nil];
	if (!glcontext)
		Sys_Error("Cannot create OpenGL context");
	[glcontext makeCurrentContext];
	[pixelFormat release];
	
	
	
	// Switch back to the original screen resolution
	// We going to window from fullscreen
	if (!fullscreen && vid.fullscreen) {
		if (desktopMode) {
			err = CGDisplaySetDisplayMode(display, desktopMode, NULL); // Restoring desktop mode
			if (err != kCGErrorSuccess)
				Sys_Error("Unable to restore display mode");
		}
	}
	
	
	// We going to fullscreen from windowed
	if (fullscreen && !vid.fullscreen) {
		// Capture the main display
		if (CGDisplayIsMain(display)) {
			// If we don't capture all displays, Cocoa tries to rearrange windows...
			err = CGCaptureAllDisplays();
		} else {
			err = CGDisplayCapture(display);
		}
		if (err != kCGErrorSuccess)
			Sys_Error("Unable to capture display");
	}
	else
	// We going to window from fullscreen
	if (!fullscreen && vid.fullscreen) {
		// Release the main display
		if (CGDisplayIsMain(display)) {
			err = CGReleaseAllDisplays();
		} else {
			err = CGDisplayRelease(display);
		}
		if (err != kCGErrorSuccess)
			Sys_Error("Unable to release display");
	}
	
	
	
	if (!window) {
		window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 0, 0)
											 styleMask:NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask
											   backing:NSBackingStoreBuffered
												 defer:NO
												screen:screen];
	}
	
	[window orderOut:nil];
	
	if (!fullscreen) {
		NSRect contentRect;
		
		// Create a window of the desired content size
		contentRect.origin.x = ([screen frame].size.width - width) / 2;
		contentRect.origin.y = ([screen frame].size.height - height) / 2;
		contentRect.size.width = width;
		contentRect.size.height = height;
		
		[window setStyleMask:NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask];
		[window setFrame:[window frameRectForContentRect:contentRect] display:NO];
		[window setLevel:NSNormalWindowLevel];
		[window setTitle:@"fxQuake"];
		
	} else {
		
		// Switch to the requested resolution
		err = CGDisplaySetDisplayMode(display, VID_GetMatchingDisplayMode (width, height, refreshrate, bpp, stretched), NULL); // Do the physical switch
		if (err != kCGErrorSuccess)
			Sys_Error("Unable to set display mode");
		
		
		CGRect main = CGDisplayBounds(CGMainDisplayID());
		CGRect rect = CGDisplayBounds(display);
		NSRect contentRect = NSMakeRect(rect.origin.x, main.size.height - rect.origin.y - rect.size.height, rect.size.width, rect.size.height);
		
		[window setStyleMask:NSBorderlessWindowMask];
		[window setFrame:[window frameRectForContentRect:contentRect] display:YES];
		[window setLevel:CGShieldingWindowLevel()];
		
	}
	
	[window makeKeyAndOrderFront:nil];
	
	// Always get mouse moved events. If mouse support is turned off (rare) the event system will filter them out.
	[window setAcceptsMouseMovedEvents:YES];
	[window setDelegate:(id<NSWindowDelegate>)[NSApp delegate]];
	
	// Note: as of the macOS 10.15 SDK, this defaults to YES instead of NO when the NSHighResolutionCapable boolean is set in Info.plist.
	NSView *contentView = [window contentView];
	if ([contentView respondsToSelector:@selector(setWantsBestResolutionOpenGLSurface:)]) {
		[contentView setWantsBestResolutionOpenGLSurface:NO];
	}
	
	// Direct the context to draw in this window
	[glcontext setView:contentView];
	
	
	
	scr_disabled_for_loading = temp;
	
	
	Con_SafePrintf ("Video mode %dx%dx%d %dHz %s%s initialized\n",
					width,
					height,
					bpp,
					refreshrate,
					stretched ? "(stretched) " : "",
					fullscreen ? "fullscreen" : "windowed");
	
	// set vid parameters
	vid.width = width;
	vid.height = height;
	vid.refreshrate = refreshrate;
	vid.bpp = bpp;
	vid.fullscreen = fullscreen;
	vid.stretched = stretched;
	
	
	vid.numpages = 2;
	vid.colormap = host_colormap;
	
	vid.conwidth = vid.width;
	vid.conheight = vid.height;
	
	vid.recalc_refdef = true; // force a surface cache flush
	
	
	// no pending changes
	vid_changed = false;
	
	[pool release];
}

/*
================
VID_SyncCvars -- johnfitz -- set vid cvars to match current video mode
================
*/
void VID_SyncCvars (void)
{
	Cvar_SetValue ("vid_width", vid.width);
	Cvar_SetValue ("vid_height", vid.height);
	Cvar_SetValue ("vid_bpp", vid.bpp);
	Cvar_SetValue ("vid_refreshrate", vid.refreshrate);
	Cvar_Set ("vid_fullscreen", (vid.fullscreen) ? "1" : "0");
	Cvar_Set ("vid_stretched", (vid.stretched) ? "1" : "0");
	
	vid_changed = false;
}

/*
===================
VID_Changed -- kristian -- notify us that a value has changed that requires a vid_restart
===================
*/
void VID_Changed (void)
{
	vid_changed = true;
}

/*
===================
VID_Restart -- johnfitz -- change video modes on the fly
===================
*/
void VID_Restart (void)
{
	int width, height, refreshrate, bpp;
	qboolean fullscreen, stretched;

	if (vid_locked || !vid_changed)
		return;

	width = (int)vid_width.value;
	height = (int)vid_height.value;
	refreshrate = (int)vid_refreshrate.value;
	bpp = (int)vid_bpp.value;
	fullscreen = (int)vid_fullscreen.value;
	stretched = (int)vid_stretched.value;

	//
	// validate new mode
	//
	if (!VID_CheckMode(width, height, refreshrate, bpp, fullscreen, stretched))
	{
		Con_SafePrintf ("Video mode %dx%dx%d %dHz %s%s is not a valid mode\n",
						width,
						height,
						bpp,
						refreshrate,
						stretched ? "(stretched) " : "",
						fullscreen ? "fullscreen" : "windowed");
		return;
	}
	
	
	// textures are invalid after mode change,
	// we destroy and re-create GL context,
	// so we delete all GL textures now.
	TexMgr_DeleteTextures ();
	
	
	vid_activewindow = false;
	vid_hiddenwindow = true;
	vid_notifywindow = false;
	[[NSApp delegate] checkActive];
	
	//
	// set new mode
	//
	VID_SetMode (width, height, refreshrate, bpp, fullscreen, stretched);
	
	vid_activewindow = true;
	vid_hiddenwindow = false;
	vid_notifywindow = true;
	[[NSApp delegate] checkActive];
	
	
	GL_GetPixelFormatInfo ();
	GL_GetInfo ();
	
	// re-create and reload all GL textures with new context.
	TexMgr_GenerateTextures ();
	
	GL_SetupState ();
	GL_CheckMultithreadedGL ();
	GL_SwapInterval ();
	
	// warpimage needs to be recalculated
	TexMgr_UploadWarpImage ();
	
	//
	// keep cvars in line with actual mode
	//
	VID_SyncCvars ();
	
}

/*
================
VID_Test -- johnfitz -- like vid_restart, but asks for confirmation after switching modes
================
*/
void VID_Test (void)
{
	int old_width, old_height, old_refreshrate, old_bpp, old_fullscreen, old_stretched;
	
	if (vid_locked || !vid_changed)
		return;
}

/*
================
VID_Lock -- ericw

Subsequent changes to vid_* mode settings, and vid_restart commands, will
be ignored until the "vid_unlock" command is run.

Used when changing gamedirs so the current settings override what was saved
in the config.cfg.
================
*/
void VID_Lock (void)
{
	vid_locked = true;
}

/*
================
VID_Unlock -- johnfitz
================
*/
void VID_Unlock (void)
{
	vid_locked = false;
	VID_SyncCvars ();
}


#define MAX_DISPLAYS 32

/*
===============
VID_Init
===============
*/
void VID_Init (void)
{
    int		i;
	int		width, height, refreshrate, bpp;
	qboolean	fullscreen, stretched;
	
    CGError err;
    CGDirectDisplayID displays[MAX_DISPLAYS];
    uint32_t displayCount;
    uint32_t displayIndex = 0;
	
	
	Cvar_RegisterVariableCallback (&vid_fullscreen, VID_Changed); //johnfitz
	Cvar_RegisterVariableCallback (&vid_width, VID_Changed); //johnfitz
	Cvar_RegisterVariableCallback (&vid_height, VID_Changed); //johnfitz
	Cvar_RegisterVariableCallback (&vid_refreshrate, VID_Changed); //johnfitz
	Cvar_RegisterVariableCallback (&vid_bpp, VID_Changed); //johnfitz
	Cvar_RegisterVariableCallback (&vid_stretched, VID_Changed); //EER1
	
	Cmd_AddCommand ("vid_lock", VID_Lock); //EER1
	Cmd_AddCommand ("vid_unlock", VID_Unlock); //johnfitz
	Cmd_AddCommand ("vid_restart", VID_Restart); //johnfitz
	Cmd_AddCommand ("vid_test", VID_Test); //johnfitz
	
	
	width = (int)vid_width.value;
	height = (int)vid_height.value;
	refreshrate = (int)vid_refreshrate.value;
	bpp = (int)vid_bpp.value;
	fullscreen = (int)vid_fullscreen.value;
	stretched = (int)vid_stretched.value;
	
	
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
    if ([screens count] != displayCount)
        Sys_Error("Wrong screen counts");
    
    // get current screen from display
    screen = [screens objectAtIndex:displayIndex];
    if ([[[screen deviceDescription] objectForKey:@"NSScreenNumber"] unsignedIntValue] != display)
        Sys_Error("Wrong screen ID");
    
    // get current mode
    desktopMode = CGDisplayCopyDisplayMode(display);
    if (!desktopMode)
        Sys_Error("Could not get current graphics mode for display");
    
	// check for command-line video parameters
	if (COM_CheckParm("-current"))
	{
		width = (int)CGDisplayModeGetWidth(desktopMode);
		height = (int)CGDisplayModeGetHeight(desktopMode);
		refreshrate = DisplayModeGetRefreshRate(desktopMode);
		bpp = DisplayModeGetBitsPerPixel(desktopMode);
		stretched = DisplayModeGetStretchedFlag(desktopMode);
		fullscreen = true;
	}
	else
	{
		if ((i = COM_CheckParm("-width")))
		{
			if (i >= com_argc-1)
				Sys_Error("VID_Init: -width <width>");
			
			width = atoi(com_argv[i+1]);
			if (!width)
				Sys_Error("VID_Init: Bad width");
			if (width < 320)
				Sys_Error("VID_Init: width < 320 is not supported");
		}
		if ((i = COM_CheckParm("-height")))
		{
			if (i >= com_argc-1)
				Sys_Error("VID_Init: -height <height>");
			
			height = atoi(com_argv[i+1]);
			if (!height)
				Sys_Error("VID_Init: Bad height");
			if (height < 200)
				Sys_Error("VID_Init: height < 200 is not supported");
		}
		
		if ((i = COM_CheckParm("-refreshrate")))
		{
			if (i >= com_argc-1)
				Sys_Error("VID_Init: -refreshrate <refreshrate>");
			
			refreshrate = atoi(com_argv[i+1]);
		}
		
		if ((i = COM_CheckParm("-bpp")))
		{
			if (i >= com_argc-1)
				Sys_Error("VID_Init: -bpp <bpp>");
			
			bpp = atoi(com_argv[i+1]);
		}
		
		if (COM_CheckParm("-window") || COM_CheckParm("-w"))
		{
			fullscreen = false;
		}
		else if (COM_CheckParm("-fullscreen") || COM_CheckParm("-f"))
		{
			fullscreen = true;
			
			if (COM_CheckParm("-stretched"))
				stretched = true;
			
			if (width < 640 || height < 480)
			{
				Con_Warning ("Fullscreen in low-res mode not available\n");
				Con_Warning ("Forcing windowed mode\n");
				fullscreen = false;
				stretched = false;
			}
		}
	}
	
	if (COM_CheckParm("-fullsbar"))
		fullsbardraw = true;
	
	
    // get video mode list
    displayModes = CGDisplayCopyAllDisplayModes(display, NULL);
    if (!displayModes)
        Sys_Error("Display available modes returned NULL");
	displayModesCount = CFArrayGetCount(displayModes);
	
	
	if (!VID_CheckMode(width, height, refreshrate, bpp, fullscreen, stretched))
	{
		width = (int)vid_width.value;
		height = (int)vid_height.value;
		refreshrate = (int)vid_refreshrate.value;
		bpp = (int)vid_bpp.value;
		fullscreen = (int)vid_fullscreen.value;
		stretched = (int)vid_stretched.value;
	}
	
	if (!VID_CheckMode(width, height, refreshrate, bpp, fullscreen, stretched))
	{
		width = 640;
		height = 480;
		refreshrate = DisplayModeGetRefreshRate(desktopMode);
		bpp = DisplayModeGetBitsPerPixel(desktopMode);
		fullscreen = false;
		stretched = false;
	}
	
	//
	// set initial mode
	//
	VID_SetMode (width, height, refreshrate, bpp, fullscreen, stretched);
	
	vid_activewindow = true;
	vid_hiddenwindow = false;
	vid_notifywindow = true;
	[[NSApp delegate] checkActive];
	
	
	vid_initialized = true;
	
	
	GL_GetPixelFormatInfo ();
	GL_Init ();
	GL_SetupState ();
	
	GL_SwapInterval ();
	
	//QuakeSpasm: current vid settings should override config file settings.
	//so we have to lock the vid mode from now until after all config files are read.
//	vid_locked = true;
}

/*
===============
VID_Shutdown

called at shutdown
===============
*/
void VID_Shutdown (void)
{
	CGError err;

    if (display) {
        
        if (glcontext) {
            [NSOpenGLContext clearCurrentContext];
            
            [glcontext clearDrawable];
            
            [glcontext release];
            glcontext = nil;
        }
        
        if (window) {
            [window release];
            window = nil;
        }
        
        if (screen) {
            [screen release];
            screen = nil;
        }
        
        // Switch back to the original screen resolution
        if (vid.fullscreen) {
            if (desktopMode) {
				err = CGDisplaySetDisplayMode(display, desktopMode, NULL); // Restoring desktop mode
				if (err != kCGErrorSuccess)
					Con_Printf("Unable to restore display mode\n");
            }
        }
        
		// Release the main display
		if (vid.fullscreen) {
			if (CGDisplayIsMain(display)) {
				err = CGReleaseAllDisplays();
			} else {
				err = CGDisplayRelease(display);
			}
			if (err != kCGErrorSuccess)
				Con_Printf("Unable to release display\n");
		}
    }
    
    vid.fullscreen = false;
}

