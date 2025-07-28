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

#define MAX_MODE_LIST	600 //johnfitz -- was 30
#define MAX_BPPS_LIST	5
#define MAX_RATES_LIST	20

typedef struct
{
	int			width;
	int			height;
	int			refreshrate;
	int			bpp;
} vmode_t;

vmode_t	modelist[MAX_MODE_LIST];
int		nummodes;

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

qboolean HasValidDisplayModeFlags (CGDisplayModeRef mode)
{
	uint32_t flags = CGDisplayModeGetIOFlags(mode);
	
	// Filter out modes which have flags that we don't want
	if (flags & (kDisplayModeNeverShowFlag | kDisplayModeNotGraphicsQualityFlag | kDisplayModeInterlacedFlag | kDisplayModeStretchedFlag))
		return false;
	
	// Filter out modes which don't have flags that we want
	if (!(flags & kDisplayModeValidFlag) || !(flags & kDisplayModeSafeFlag))
		return false;
	
	return true;
}

/*
================
VID_GetMatchingDisplayMode
================
*/
CGDisplayModeRef VID_GetMatchingDisplayMode (int width, int height, int refreshrate, int bpp)
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
			DisplayModeGetBitsPerPixel(mode) == bpp)
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
qboolean VID_CheckMode (int width, int height, int refreshrate, int bpp, qboolean fullscreen)
{
	if (width < 320)
		return false;
	
	if (height < 200)
		return false;
	
	if (fullscreen && (width < 640 || height < 400))
		return false;
	
	switch (bpp)
	{
		case 32:
		case 16:
			break;
		default:
			return false;
	}
	
	if (fullscreen && !VID_GetMatchingDisplayMode (width, height, refreshrate, bpp))
		return false;
	
	return true;
}

/*
================
VID_SyncCvars -- johnfitz -- set vid cvars to match current video mode
================
*/
void VID_SyncCvars (void)
{
	if (vid_locked || !vid_changed)
		return;

	Cvar_SetValue ("vid_width", vid.width);
	Cvar_SetValue ("vid_height", vid.height);
	Cvar_SetValue ("vid_bpp", vid.bpp);
	Cvar_SetValue ("vid_refreshrate", vid.refreshrate);
	Cvar_Set ("vid_fullscreen", (vid.fullscreen) ? "1" : "0");
	
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

/*
================
VID_SetMode
================
*/
void VID_SetMode (int width, int height, int refreshrate, int bpp, qboolean fullscreen)
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
	
	// We should exit from windowed fullscreen mode before any mode change
	// NSFullScreenWindowMask flag is absent in OS X 10.6 SDK
	if (([window styleMask] & (1 << 14)) == (1 << 14)) {
		if ([window respondsToSelector:@selector(toggleFullScreen:)])
			[window toggleFullScreen:nil];
	}
	
	[window orderOut:nil]; // hide
	
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
		err = CGDisplaySetDisplayMode(display, VID_GetMatchingDisplayMode (width, height, refreshrate, bpp), NULL); // Do the physical switch
		if (err != kCGErrorSuccess)
			Sys_Error("Unable to set display mode");
		
		CGRect main = CGDisplayBounds(CGMainDisplayID());
		CGRect rect = CGDisplayBounds(display);
		NSRect contentRect = NSMakeRect(rect.origin.x, main.size.height - rect.origin.y - rect.size.height, rect.size.width, rect.size.height);
		
		[window setStyleMask:NSBorderlessWindowMask];
		[window setFrame:[window frameRectForContentRect:contentRect] display:YES];
		[window setLevel:CGShieldingWindowLevel()];
	}
	
	[window makeKeyAndOrderFront:nil]; // show
	
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
	
	Con_SafePrintf ("Video mode %dx%dx%d %dHz %s initialized\n",
					width,
					height,
					bpp,
					refreshrate,
					fullscreen ? "fullscreen" : "windowed");
	
	// set vid parameters
	vid.width = width;
	vid.height = height;
	vid.refreshrate = refreshrate;
	vid.bpp = bpp;
	vid.fullscreen = fullscreen;
	
	vid.conwidth = vid.width;
	vid.conheight = vid.height;
	
	vid.numpages = 2;
	vid.colormap = host_colormap;
	
	vid.recalc_refdef = true; // force a surface cache flush
	
	// keep cvars in line with actual mode
	VID_SyncCvars ();
	
	[pool release];
}


/*
===================
VID_Restart -- johnfitz -- change video modes on the fly
===================
*/
void VID_Restart (void)
{
	int width, height, refreshrate, bpp;
	qboolean fullscreen;

	if (vid_locked || !vid_changed)
		return;

	width = (int)vid_width.value;
	height = (int)vid_height.value;
	refreshrate = (int)vid_refreshrate.value;
	bpp = (int)vid_bpp.value;
	fullscreen = (qboolean)vid_fullscreen.value;

	//
	// validate new mode
	//
	if (!VID_CheckMode(width, height, refreshrate, bpp, fullscreen))
	{
		Con_SafePrintf ("Video mode %dx%dx%d %dHz %s is not a valid mode\n",
						width,
						height,
						bpp,
						refreshrate,
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
	IN_CheckActive ();
	
	//
	// set new mode
	//
	VID_SetMode (width, height, refreshrate, bpp, fullscreen);
	
	vid_activewindow = true;
	vid_hiddenwindow = false;
	vid_notifywindow = true;
	IN_CheckActive ();
	
	GL_GetPixelFormatInfo ();
	GL_GetGLInfo ();
	
	// re-create and reload all GL textures with new context.
	TexMgr_GenerateTextures ();
	
	GL_SetupState ();
	GL_CheckSwapInterval ();
	GL_CheckMultithreadedGL ();
	
	// warpimage needs to be recalculated
	TexMgr_UploadWarpImage ();
}

/*
================
VID_Test -- johnfitz -- like vid_restart, but asks for confirmation after switching modes
================
*/
void VID_Test (void)
{
	int old_width, old_height, old_refreshrate, old_bpp;
	qboolean old_fullscreen;
	
	if (vid_locked || !vid_changed)
		return;
	
	//
	// now try the switch
	//
	old_width = vid.width;
	old_height = vid.height;
	old_refreshrate = vid.refreshrate;
	old_bpp = vid.bpp;
	old_fullscreen = vid.fullscreen;
	
	VID_Restart ();
	
	//pop up confirmation dialoge
	if (!SCR_ModalMessageTimeout("Would you like to keep this\nvideo mode?\n", 5.0f))
	{
		//revert cvars and mode
		Cvar_SetValue ("vid_width", old_width);
		Cvar_SetValue ("vid_height", old_height);
		Cvar_SetValue ("vid_bpp", old_bpp);
		Cvar_SetValue ("vid_refreshrate", old_refreshrate);
		Cvar_Set ("vid_fullscreen", (old_fullscreen) ? "1" : "0");
		
		VID_Restart ();
	}
}

/*
================
VID_Toggle -- new proc by S.A., called by alt-return key binding.

EER1 -- rewritten
================
*/
void	VID_Toggle (void)
{
	qboolean do_toggle;
	qboolean fullscreen;
	
	S_ClearBuffer ();
	
	fullscreen = !(qboolean)vid_fullscreen.value;
	do_toggle = VID_CheckMode((int)vid_width.value,
							  (int)vid_height.value,
							  (int)vid_refreshrate.value,
							  (int)vid_bpp.value,
							  fullscreen);
	
	if (do_toggle)
	{
		Cvar_Set ("vid_fullscreen", (fullscreen) ? "1" : "0");
		Cbuf_AddText ("vid_restart\n");
	}
}


/*
=================
VID_DescribeCurrentMode_f
=================
*/
void VID_DescribeCurrentMode_f (void)
{
	if (vid_initialized)
		Con_SafePrintf ("mode %dx%dx%d %dHz %s\n",
						vid.width,
						vid.height,
						vid.bpp,
						vid.refreshrate,
						vid.fullscreen ? "fullscreen" : "windowed");
}

/*
=================
VID_DescribeModes_f -- johnfitz -- changed formatting, and added refresh rates after each mode.
=================
*/
void VID_DescribeModes_f (void)
{
	int	i;
	int	lastwidth, lastheight, lastbpp, lastrefreshrate, count;
	
	lastwidth = lastheight = lastbpp = lastrefreshrate = count = 0;
	
	for (i = 0; i < nummodes; i++)
	{
		if (lastwidth != modelist[i].width || lastheight != modelist[i].height || lastbpp != modelist[i].bpp || lastrefreshrate != modelist[i].refreshrate)
		{
			if (count > 0)
				Con_SafePrintf ("\n");
			Con_SafePrintf ("   %4i x %4i x %i : %iHz", modelist[i].width, modelist[i].height, modelist[i].bpp, modelist[i].refreshrate);
			lastwidth = modelist[i].width;
			lastheight = modelist[i].height;
			lastbpp = modelist[i].bpp;
			lastrefreshrate = modelist[i].refreshrate;
			count++;
		}
	}
	Con_Printf ("\n%i modes\n", count);
}


int vmodecompare(const void *inmode1, const void *inmode2)
{
	// sort lowest res to highest
	const vmode_t *mode1 = (vmode_t *)inmode1;
	const vmode_t *mode2 = (vmode_t *)inmode2;
	
	if (mode1->width < mode2->width)
		return 1;
	if (mode1->width > mode2->width)
		return -1;
	if (mode1->height < mode2->height)
		return 1;
	if (mode1->height > mode2->height)
		return -1;
	if (mode1->bpp < mode2->bpp)
		return 1;
	if (mode1->bpp > mode2->bpp)
		return -1;
	if (mode1->refreshrate < mode2->refreshrate)
		return 1;
	if (mode1->refreshrate > mode2->refreshrate)
		return -1;
	
	return 0;
}

/*
=================
VID_InitModelist
=================
*/
void VID_InitModelist (void)
{
	CGDisplayModeRef mode;
	CFIndex modeIndex;
	
	for (modeIndex = 0; modeIndex < displayModesCount; modeIndex++)
	{
		if (nummodes >= MAX_MODE_LIST)
			break;
		
		mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(displayModes, modeIndex);
		if (mode && HasValidDisplayModeFlags(mode) && DisplayModeGetBitsPerPixel(mode) >= 16)
		{
			modelist[nummodes].width = (int)CGDisplayModeGetWidth(mode);
			modelist[nummodes].height = (int)CGDisplayModeGetHeight(mode);
			modelist[nummodes].bpp = DisplayModeGetBitsPerPixel(mode);
			modelist[nummodes].refreshrate = DisplayModeGetRefreshRate(mode);
			nummodes++;
		}
	}
	
	qsort((void *)modelist, nummodes, sizeof(vmode_t), vmodecompare);
}

/*
=================
VID_InitDisplayModes
=================
*/
void VID_InitDisplayModes (void)
{
	CFDictionaryRef options = NULL;
	
#ifdef MAC_OS_X_VERSION_10_8
	if (floor(NSAppKitVersionNumber) > NSAppKitVersionNumber10_7)
	{
		const CFStringRef keys[] = { kCGDisplayShowDuplicateLowResolutionModes };
		const CFBooleanRef values[] = { kCFBooleanTrue };
		options = CFDictionaryCreate(NULL,
									 (const void **)keys,
									 (const void **)values, 1,
									 &kCFCopyStringDictionaryKeyCallBacks,
									 &kCFTypeDictionaryValueCallBacks);
	}
#endif
	
	// get video mode list
	displayModes = CGDisplayCopyAllDisplayModes(display, options);
	if (!displayModes)
		Sys_Error("Display available modes returned NULL");
	displayModesCount = CFArrayGetCount(displayModes);
	
	if (options)
		CFRelease(options);
}

/*
================
VID_ReadCvars

early reading video variables
================
*/
void VID_ReadCvars (void)
{
	char *cvars[] = {
		"vid_fullscreen",
		"vid_width",
		"vid_height",
		"vid_refreshrate",
		"vid_bpp"
	};
	int num = sizeof(cvars)/sizeof(cvars[0]);
	
	if (CFG_OpenConfig("config.cfg") == 0)
	{
		CFG_ReadCvars(cvars, num);
		CFG_CloseConfig();
	}
	CFG_ReadCvarOverrides(cvars, num);
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
	qboolean	fullscreen;
	
    CGError err;
    CGDirectDisplayID displays[MAX_DISPLAYS];
    uint32_t displayCount;
    uint32_t displayIndex = 0;
	
	Cvar_RegisterVariableCallback (&vid_fullscreen, VID_Changed); //johnfitz
	Cvar_RegisterVariableCallback (&vid_width, VID_Changed); //johnfitz
	Cvar_RegisterVariableCallback (&vid_height, VID_Changed); //johnfitz
	Cvar_RegisterVariableCallback (&vid_refreshrate, VID_Changed); //johnfitz
	Cvar_RegisterVariableCallback (&vid_bpp, VID_Changed); //johnfitz
	
	Cmd_AddCommand ("vid_lock", VID_Lock); //EER1
	Cmd_AddCommand ("vid_unlock", VID_Unlock); //johnfitz
	Cmd_AddCommand ("vid_restart", VID_Restart); //johnfitz
	Cmd_AddCommand ("vid_test", VID_Test); //johnfitz
	Cmd_AddCommand ("vid_describecurrentmode", VID_DescribeCurrentMode_f);
	Cmd_AddCommand ("vid_describemodes", VID_DescribeModes_f);
	
	VID_ReadCvars ();
	
	width = (int)vid_width.value;
	height = (int)vid_height.value;
	refreshrate = (int)vid_refreshrate.value;
	bpp = (int)vid_bpp.value;
	fullscreen = (qboolean)vid_fullscreen.value;
	
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
			
			if (width < 640 || height < 400)
			{
				Con_Warning ("Fullscreen in low resolution not supported\n");
				Con_Warning ("Forcing windowed mode\n");
				fullscreen = false;
			}
		}
	}
	
	if (COM_CheckParm("-fullsbar"))
		fullsbardraw = true;
	
	VID_InitDisplayModes ();
	
	VID_InitModelist ();
	
	if (!VID_CheckMode(width, height, refreshrate, bpp, fullscreen))
	{
		width = (int)vid_width.value;
		height = (int)vid_height.value;
		refreshrate = (int)vid_refreshrate.value;
		bpp = (int)vid_bpp.value;
		fullscreen = (qboolean)vid_fullscreen.value;
	}
	
	if (!VID_CheckMode(width, height, refreshrate, bpp, fullscreen))
	{
		width = 640;
		height = 480;
		refreshrate = DisplayModeGetRefreshRate(desktopMode);
		bpp = DisplayModeGetBitsPerPixel(desktopMode);
		fullscreen = false;
	}
	
	//
	// set initial mode
	//
	VID_SetMode (width, height, refreshrate, bpp, fullscreen);
	
	vid_activewindow = true;
	vid_hiddenwindow = false;
	vid_notifywindow = true;
	IN_CheckActive ();
	
	vid_initialized = true;
	
	GL_GetPixelFormatInfo ();
	GL_Init ();
	GL_SetupState ();
	
	GL_SwapInterval ();
	
	// enable the video options menu
	vid_menucmdfn = VID_MenuCmd; //johnfitz
	vid_menudrawfn = VID_MenuDraw;
	vid_menukeyfn = VID_MenuKey;
	
	VID_MenuInit (); //johnfitz
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
		
		if (desktopMode)
			CGDisplayModeRelease (desktopMode);
		
		if (displayModes)
			CFRelease (displayModes);
    }
    
    vid.fullscreen = false;
}

//==========================================================================
//
//  NEW VIDEO MENU -- johnfitz
//
//==========================================================================

enum {
	VID_OPT_MODE,
	VID_OPT_BPP,
	VID_OPT_REFRESHRATE,
	VID_OPT_FULLSCREEN,
	VID_OPT_VSYNC,
	VID_OPT_TEST,
	VID_OPT_APPLY,
	VIDEO_OPTIONS_ITEMS
};

int	video_options_cursor = 0;

typedef struct {
	int width, height;
} vid_menu_mode;

// TODO: replace these fixed-length arrays with hunk_allocated buffers
vid_menu_mode vid_menu_modes[MAX_MODE_LIST];
int vid_menu_nummodes = 0;

int vid_menu_bpps[MAX_BPPS_LIST];
int vid_menu_numbpps = 0;

int vid_menu_rates[MAX_RATES_LIST];
int vid_menu_numrates=0;

/*
================
VID_MenuInit
================
*/
void VID_MenuInit (void)
{
	int i, j, h, w;

	for (i = 0; i < nummodes; i++)
	{
		w = modelist[i].width;
		h = modelist[i].height;

		for (j = 0; j < vid_menu_nummodes; j++)
		{
			if (vid_menu_modes[j].width == w &&
				vid_menu_modes[j].height == h)
				break;
		}

		if (j == vid_menu_nummodes)
		{
			vid_menu_modes[j].width = w;
			vid_menu_modes[j].height = h;
			vid_menu_nummodes++;
		}
	}
}

/*
================
VID_Menu_RebuildBppList

regenerates bpp list based on current vid_width and vid_height
================
*/
void VID_Menu_RebuildBppList (void)
{
	int i, j, b;

	vid_menu_numbpps = 0;

	for (i = 0; i < nummodes; i++)
	{
		if (vid_menu_numbpps >= MAX_BPPS_LIST)
			break;

		// bpp list is limited to bpps available with current width/height
		if (modelist[i].width != vid_width.value ||
			modelist[i].height != vid_height.value)
			continue;

		b = modelist[i].bpp;

		for (j = 0; j < vid_menu_numbpps; j++)
		{
			if (vid_menu_bpps[j] == b)
				break;
		}

		if (j == vid_menu_numbpps)
		{
			vid_menu_bpps[j] = b;
			vid_menu_numbpps++;
		}
	}

	// if there are no valid fullscreen bpps for this width/height, just pick one
	if (vid_menu_numbpps == 0)
	{
		Cvar_SetValue ("vid_bpp", (float)modelist[0].bpp);
		return;
	}

	// if vid_bpp is not in the new list, change vid_bpp
	for (i = 0; i < vid_menu_numbpps; i++)
		if (vid_menu_bpps[i] == (int)(vid_bpp.value))
			break;

	if (i == vid_menu_numbpps)
		Cvar_SetValue ("vid_bpp", (float)vid_menu_bpps[0]);
}

/*
================
VID_Menu_RebuildRateList

regenerates rate list based on current vid_width, vid_height and vid_bpp
================
*/
void VID_Menu_RebuildRateList (void)
{
	int i, j, r;

	vid_menu_numrates = 0;

	for (i = 0; i < nummodes; i++)
	{
		// rate list is limited to rates available with current width/height/bpp
		if (modelist[i].width != vid_width.value ||
			modelist[i].height != vid_height.value ||
			modelist[i].bpp != vid_bpp.value)
			continue;

		r = modelist[i].refreshrate;

		for (j = 0; j < vid_menu_numrates; j++)
		{
			if (vid_menu_rates[j] == r)
				break;
		}

		if (j == vid_menu_numrates)
		{
			vid_menu_rates[j] = r;
			vid_menu_numrates++;
		}
	}

	// if there are no valid fullscreen refreshrates for this width/height, just pick one
	if (vid_menu_numrates == 0)
	{
		Cvar_SetValue ("vid_refreshrate", (float)modelist[0].refreshrate);
		return;
	}

	// if vid_refreshrate is not in the new list, change vid_refreshrate
	for (i = 0; i < vid_menu_numrates; i++)
		if (vid_menu_rates[i] == (int)(vid_refreshrate.value))
			break;

	if (i == vid_menu_numrates)
		Cvar_SetValue ("vid_refreshrate", (float)vid_menu_rates[0]);
}

/*
================
VID_Menu_ChooseNextMode

chooses next resolution in order, then updates vid_width and
vid_height cvars, then updates bpp and refreshrate lists
================
*/
void VID_Menu_ChooseNextMode (int dir)
{
	int i;

	if (vid_menu_nummodes)
	{
		for (i = 0; i < vid_menu_nummodes; i++)
		{
			if (vid_menu_modes[i].width == vid_width.value &&
				vid_menu_modes[i].height == vid_height.value)
				break;
		}

		if (i == vid_menu_nummodes) // can't find it in list, so it must be a custom windowed res
		{
			i = 0;
		}
		else
		{
			i += dir;
			if (i >= vid_menu_nummodes)
				i = 0;
			else if (i < 0)
				i = vid_menu_nummodes-1;
		}

		Cvar_SetValue ("vid_width", (float)vid_menu_modes[i].width);
		Cvar_SetValue ("vid_height", (float)vid_menu_modes[i].height);
		VID_Menu_RebuildBppList ();
		VID_Menu_RebuildRateList ();
	}
}

/*
================
VID_Menu_ChooseNextBpp

chooses next bpp in order, then updates vid_bpp cvar
================
*/
void VID_Menu_ChooseNextBpp (int dir)
{
	int i;

	if (vid_menu_numbpps)
	{
		for (i = 0; i < vid_menu_numbpps; i++)
		{
			if (vid_menu_bpps[i] == vid_bpp.value)
				break;
		}

		if (i == vid_menu_numbpps) // can't find it in list
		{
			i = 0;
		}
		else
		{
			i += dir;
			if (i >= vid_menu_numbpps)
				i = 0;
			else if (i < 0)
				i = vid_menu_numbpps-1;
		}

		Cvar_SetValue ("vid_bpp", (float)vid_menu_bpps[i]);
	}
}

/*
================
VID_Menu_ChooseNextRate

chooses next refresh rate in order, then updates vid_refreshrate cvar
================
*/
void VID_Menu_ChooseNextRate (int dir)
{
	int i;

	for (i = 0; i < vid_menu_numrates; i++)
	{
		if (vid_menu_rates[i] == vid_refreshrate.value)
			break;
	}

	if (i == vid_menu_numrates) // can't find it in list
	{
		i = 0;
	}
	else
	{
		i += dir;
		if (i >= vid_menu_numrates)
			i = 0;
		else if (i < 0)
			i = vid_menu_numrates-1;
	}

	Cvar_SetValue ("vid_refreshrate", (float)vid_menu_rates[i]);
}

/*
================
VID_MenuKey
================
*/
void VID_MenuKey (int key)
{
	switch (key)
	{
	case K_ESCAPE:
		VID_SyncCvars (); // sync cvars before leaving menu. FIXME: there are other ways to leave menu
		S_LocalSound ("misc/menu1.wav");
		M_Menu_Options_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		video_options_cursor--;
		if (video_options_cursor < 0)
			video_options_cursor = VIDEO_OPTIONS_ITEMS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		video_options_cursor++;
		if (video_options_cursor >= VIDEO_OPTIONS_ITEMS)
			video_options_cursor = 0;
		break;

	case K_LEFTARROW:
		S_LocalSound ("misc/menu3.wav");
		switch (video_options_cursor)
		{
		case VID_OPT_MODE:
			VID_Menu_ChooseNextMode (1);
			break;
		case VID_OPT_BPP:
			VID_Menu_ChooseNextBpp (1);
			break;
		case VID_OPT_REFRESHRATE:
			VID_Menu_ChooseNextRate (1);
			break;
		case VID_OPT_FULLSCREEN:
			Cbuf_AddText ("toggle vid_fullscreen\n");
			break;
		case VID_OPT_VSYNC:
			Cbuf_AddText ("toggle gl_swapinterval\n");
			break;
		default:
			break;
		}
		break;

	case K_RIGHTARROW:
		S_LocalSound ("misc/menu3.wav");
		switch (video_options_cursor)
		{
		case VID_OPT_MODE:
			VID_Menu_ChooseNextMode (-1);
			break;
		case VID_OPT_BPP:
			VID_Menu_ChooseNextBpp (-1);
			break;
		case VID_OPT_REFRESHRATE:
			VID_Menu_ChooseNextRate (-1);
			break;
		case VID_OPT_FULLSCREEN:
			Cbuf_AddText ("toggle vid_fullscreen\n");
			break;
		case VID_OPT_VSYNC:
			Cbuf_AddText ("toggle gl_swapinterval\n");
			break;
		default:
			break;
		}
		break;

	case K_ENTER:
	case K_KP_ENTER:
		m_entersound = true;
		switch (video_options_cursor)
		{
		case VID_OPT_MODE:
			VID_Menu_ChooseNextMode (1);
			break;
		case VID_OPT_BPP:
			VID_Menu_ChooseNextBpp (1);
			break;
		case VID_OPT_REFRESHRATE:
			VID_Menu_ChooseNextRate (1);
			break;
		case VID_OPT_FULLSCREEN:
			Cbuf_AddText ("toggle vid_fullscreen\n");
			break;
		case VID_OPT_VSYNC:
			Cbuf_AddText ("toggle gl_swapinterval\n");
			break;
		case VID_OPT_TEST:
			Cbuf_AddText ("vid_test\n");
			break;
		case VID_OPT_APPLY:
			Cbuf_AddText ("vid_restart\n");
			break;
		default:
			break;
		}
		break;

	default:
		break;
	}
}

/*
================
VID_MenuDraw
================
*/
void VID_MenuDraw (void)
{
	int i, y;
	qpic_t *p;

	y = 4;

	// plaque
//	p = Draw_CachePic ("gfx/qplaque.lmp");
//	M_DrawTransPic (16, y, p);

	p = Draw_CachePic ("gfx/vidmodes.lmp");
	M_DrawPic ( (320-p->width)/2, y, p);

	y += 28;

	// options
	for (i = 0; i < VIDEO_OPTIONS_ITEMS; i++)
	{
		switch (i)
		{
		case VID_OPT_MODE:
			M_Print (16, y, "        Video mode");
			M_Print (184, y, va("%ix%i", (int)vid_width.value, (int)vid_height.value));
			break;
		case VID_OPT_BPP:
			M_Print (16, y, "       Color depth");
			M_Print (184, y, va("%i", (int)vid_bpp.value));
			break;
		case VID_OPT_REFRESHRATE:
			M_Print (16, y, "      Refresh rate");
			M_Print (184, y, va("%i", (int)vid_refreshrate.value));
			break;
		case VID_OPT_FULLSCREEN:
			M_Print (16, y, "        Fullscreen");
			M_DrawCheckbox (184, y, (int)vid_fullscreen.value);
			break;
		case VID_OPT_VSYNC:
			M_Print (16, y, "     Vertical sync");
			if (gl_swap_control)
				M_DrawCheckbox (184, y, (int)gl_swapinterval.value);
			else
				M_Print (184, y, "N/A");
			break;
		case VID_OPT_TEST:
			y += 8; // separate the test and apply items
			M_Print (16, y, "      Test changes");
			break;
		case VID_OPT_APPLY:
			M_Print (16, y, "     Apply changes");
			break;
		}

		if (video_options_cursor == i)
			M_DrawCharacter (168, y, 12+((int)(realtime*4)&1));

		y += 8;
	}
}

/*
================
VID_MenuCmd
================
*/
void VID_MenuCmd (void)
{
	key_dest = key_menu;
	m_state = m_video;
	m_entersound = true;

	// set all the cvars to match the current mode when entering the menu
	VID_SyncCvars ();

	// set up bpp and rate lists based on current cvars
	VID_Menu_RebuildBppList ();
	VID_Menu_RebuildRateList ();
}

