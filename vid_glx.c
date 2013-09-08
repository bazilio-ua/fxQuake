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
// vid_glx.c -- OpenGL Extension X Window System video driver

#include "quakedef.h"
#include "xquake.h"
#include "quake.xpm"

int scrnum;
static GLXContext ctx = NULL;

Atom wm_delete_window_atom;

viddef_t vid; // global video state

cvar_t		vid_mode = {"vid_mode","0",false};
cvar_t		vid_gamma = {"gamma", "1", true};

int window_x, window_y, window_width, window_height;

XF86VidModeModeInfo init_vidmode, game_vidmode;

const char *glx_extensions;

//==========================================================================
//
//  HARDWARE GAMMA
//
//==========================================================================

unsigned short	 vid_gammaramp[3][256];
unsigned short	 vid_systemgammaramp[3][256]; // to restore gamma
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

	if (!XF86VidModeSetGammaRamp (x_disp, scrnum, 256, vid_gammaramp[0], vid_gammaramp[1], vid_gammaramp[2]))
		Con_Printf ("VID_Gamma_Set: failed on XF86VidModeSetGammaRamp\n");
/*
	FIXME: for some reason, this always returns false.

	which is supposed to return true or false depending on if the call succeeds,
	but I've found on my machine that the function call always returns false,
	regardless of the function working or not.
*/
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

	if (!XF86VidModeSetGammaRamp (x_disp, scrnum, 256, vid_systemgammaramp[0], vid_systemgammaramp[1], vid_systemgammaramp[2]))
		Con_Printf ("VID_Gamma_Restore: failed on XF86VidModeSetGammaRamp\n");
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

	// Refresh gamma
	for (i=0; i<256; i++)
		vid_gammaramp[0][i] = vid_gammaramp[1][i] = vid_gammaramp[2][i] =
			CLAMP(0, (int) (255 * pow ((i+0.5)/255.5, vid_gamma.value) + 0.5), 255) << 8;

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
	int	xf86vm_gammaramp_size;

	vid_gammaworks = false;

	XF86VidModeGetGammaRampSize (x_disp, scrnum, &xf86vm_gammaramp_size);
	vid_gammaworks = (xf86vm_gammaramp_size == 256);
	if (vid_gammaworks)
		XF86VidModeGetGammaRamp (x_disp, scrnum, xf86vm_gammaramp_size, vid_systemgammaramp[0], vid_systemgammaramp[1], vid_systemgammaramp[2]);

	if (!vid_gammaworks)
		Con_Printf ("Hardware gamma unavailable\n");

	Cvar_RegisterVariable (&vid_gamma, VID_Gamma);
}

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
	// swap on each frame
	glXSwapBuffers(x_disp, x_win);

	if (fullsbardraw)
		Sbar_Changed();
}

//====================================

/*
===============
VID_Init
===============
*/
void VID_Init (void)
{
	int i;
	int attrib[] = {
		GLX_RGBA,
		GLX_RED_SIZE, 1,
		GLX_GREEN_SIZE, 1,
		GLX_BLUE_SIZE, 1,
		GLX_DOUBLEBUFFER,
		GLX_DEPTH_SIZE, 1,
		None
	};

	XSetWindowAttributes attr;
	XClassHint *clshints;
	XWMHints *wmhints;
	XSizeHints *szhints;
	unsigned long mask;
	Window root;
	XVisualInfo *visinfo;
	qboolean fullscreen = true;
	qboolean vidmode_ext = false;
	qboolean VidMode = false;
	int MajorVersion = 0, MinorVersion = 0;

	Cvar_RegisterVariable (&vid_mode, NULL);

// set vid parameters
	vid.width = 640;
	vid.height = 480;
	vid.numpages = 2;
// open the display
	x_disp = XOpenDisplay(NULL);
	if (!x_disp) 
	{
		if (getenv("DISPLAY"))
			Sys_Error("VID_Init: Could not open display [%s]", getenv("DISPLAY"));
		else
			Sys_Error("VID_Init: Could not open local display");
	}

// get video mode list
	VidMode = XF86VidModeQueryVersion(x_disp, &MajorVersion, &MinorVersion);
	if (COM_CheckParm("-novidmode"))
	{
		Con_Warning ("XFree86 VidMode extension disabled at command line\n");
	}
	else if (VidMode)
	{
		Con_Printf("XFree86 VidMode extension version %d.%d found\n", MajorVersion, MinorVersion);
		vidmode_ext = true;
	}
	else
	{
		Con_Warning ("XFree86 VidMode extension not supported\n");
	}

// get visual
	scrnum = DefaultScreen(x_disp);
	root = RootWindow(x_disp, scrnum);
	visinfo = glXChooseVisual(x_disp, scrnum, attrib);
	if (!visinfo) 
	{
		Sys_Error ("VID_Init: couldn't get an RGB, Double-buffered, Depth visual");
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

// check for command-line video parameters
	if (COM_CheckParm("-current"))
	{
		vid.width = DisplayWidth (x_disp, scrnum);
		vid.height = DisplayHeight (x_disp, scrnum);
	}
	else if (COM_CheckParm("-window"))
	{
		fullscreen = false;
	}

// set the vidmode to the requested width/height if possible 
	if (vidmode_ext) 
	{
		int x, y, dist;
		int best_dist = 9999999;
		int num_modes;
		XF86VidModeModeInfo **modes;
		XF86VidModeModeInfo *mode = NULL;

		XF86VidModeGetAllModeLines(x_disp, scrnum, &num_modes, &modes);

		// are we going fullscreen?  if so, let's change video mode
		if (fullscreen) 
		{
			for (i = 0; i < num_modes; i++) 
			{
				if (vid.width > modes[i]->hdisplay || vid.height > modes[i]->vdisplay)
					continue;

				x = vid.width - modes[i]->hdisplay;
				y = vid.height - modes[i]->vdisplay;
				dist = (x * x) + (y * y);

				if (dist < best_dist) 
				{
					best_dist = dist;
					mode = modes[i];
				}
			}

			if (mode)
			{
				// change to the mode
				XF86VidModeSwitchToMode(x_disp, scrnum, mode);
				memcpy(&game_vidmode, mode, sizeof(game_vidmode)); // save game vidmode
				memcpy(&init_vidmode, modes[0], sizeof(init_vidmode)); // save system initial vidmode
				vidmode_fullscreen = true;

				// move the viewport to top left
				XF86VidModeSetViewPort(x_disp, scrnum, 0, 0);
				Con_DPrintf ("VID_Init: set fullscreen mode at %dx%d\n", vid.width, vid.height); 
			} 
			else
			{
				Con_DPrintf ("VID_Init: unable to set fullscreen mode at %dx%d\n", vid.width, vid.height); 
			}

			// manpage of XF86VidModeGetAllModeLines says it should be freed by the caller
			XFree(modes); 
		}
	}

// window attributes
	mask = CWBackPixel | CWColormap | CWEventMask;
	attr.background_pixel = 0;
	attr.colormap = XCreateColormap(x_disp, root, visinfo->visual, AllocNone);
	attr.event_mask = X_MASK;

// setup attributes for main window
	if (vidmode_fullscreen) 
	{
		mask |= CWSaveUnder | CWBackingStore | CWOverrideRedirect;
		attr.override_redirect = True;
		attr.backing_store = NotUseful;
		attr.save_under = False;
	} 
	else
	{
		// if width and height match desktop size, make the window without titlebar/borders
		if (vid.width == DisplayWidth (x_disp, scrnum) && vid.height == DisplayHeight (x_disp, scrnum))
		{
			mask |= CWSaveUnder | CWBackingStore | CWOverrideRedirect;
			attr.override_redirect = True;
			attr.backing_store = NotUseful;
			attr.save_under = False;
		}
		else
		{
			mask |= CWBorderPixel;
			attr.border_pixel = 0;
		}
	}

// create the main window
	x_win = XCreateWindow (
		x_disp, 
		root, 
		0, 0, // x, y
		vid.width, vid.height, 
		0, // borderwidth
		visinfo->depth, 
		InputOutput, 
		visinfo->visual, 
		mask, 
		&attr
	);

	wmhints = XAllocWMHints();
	if(XpmCreatePixmapFromData(x_disp, x_win, quake_xpm, &wmhints->icon_pixmap, &wmhints->icon_mask, NULL) == XpmSuccess)
		wmhints->flags |= IconPixmapHint | IconMaskHint;

	clshints = XAllocClassHint();
	clshints->res_name = strdup("fxQuake");
	clshints->res_class = strdup("fxQuake");

	szhints = XAllocSizeHints();
	if(vidmode_fullscreen)
	{
		szhints->min_width = szhints->max_width = vid.width;
		szhints->min_height = szhints->max_height = vid.height;
		szhints->flags |= PMinSize | PMaxSize;
	}

	XmbSetWMProperties(x_disp, x_win, "fxQuake", "fxQuake", (char **) com_argv, com_argc, szhints, wmhints, clshints);
	// strdup() allocates using malloc(), should be freed with free()
	free(clshints->res_name);
	free(clshints->res_class); 
	XFree(clshints);
	XFree(wmhints);
	XFree(szhints);

// map the window
	XMapWindow(x_disp, x_win);

// making the close button on a window do the right thing
// seems to involve this mess, sigh...
	wm_delete_window_atom = XInternAtom(x_disp, "WM_DELETE_WINDOW", false);
	XSetWMProtocols(x_disp, x_win, &wm_delete_window_atom, 1);

	if (vidmode_fullscreen) 
	{
		XMoveWindow(x_disp, x_win, 0, 0);
		XRaiseWindow(x_disp, x_win);

		// move the viewport to top left
		XF86VidModeSetViewPort(x_disp, scrnum, 0, 0);
	}
	else
	{
		int x, y;

		// get the size of the desktop and center window properly in it
		x = (DisplayWidth(x_disp, scrnum) - vid.width) / 2;
		y = (DisplayHeight(x_disp, scrnum) - vid.height) / 2;
		if (x < 0) x = 0;
		if (y < 0) y = 0;

		XMoveWindow(x_disp, x_win, x, y);
	}

	ctx = glXCreateContext(x_disp, visinfo, NULL, True);
	XFree(visinfo); // glXChooseVisual man page says to use XFree to free visinfo
	if (!ctx)
	{
		Sys_Error ("VID_Init: glXCreateContext failed");
	}
	if (!glXMakeCurrent(x_disp, x_win, ctx))
	{
		Sys_Error ("VID_Init: glXMakeCurrent failed");
	}

	XSync(x_disp, False); // sync it

	vid_activewindow = true;
	vid_hiddenwindow = false;
	vid_notifywindow = true;

	vid.conwidth = vid.width;
	vid.conheight = vid.height;

	glx_extensions = glXGetClientString (x_disp, GLX_EXTENSIONS);
	GL_Init();

	VID_Gamma_Init ();

	vid.recalc_refdef = true; // force a surface cache flush

	if (COM_CheckParm("-fullsbar"))
		fullsbardraw = true;

	Con_SafePrintf ("Video mode %dx%d %s initialized\n", vid.width, vid.height, vidmode_fullscreen ? "fullscreen" : "windowed");
}

/*
===============
VID_Shutdown

called at shutdown
===============
*/
void VID_Shutdown (void)
{
	if (x_disp) 
	{
		if (ctx)
			glXDestroyContext(x_disp, ctx);

		VID_Gamma_Shutdown ();

		if (x_win)
			XDestroyWindow(x_disp, x_win);

		if (vidmode_fullscreen)
			XF86VidModeSwitchToMode(x_disp, scrnum, &init_vidmode);

		XCloseDisplay (x_disp);
	}

	vidmode_fullscreen = false;
	x_disp = NULL;
	x_win = 0;
	ctx = NULL;
}

