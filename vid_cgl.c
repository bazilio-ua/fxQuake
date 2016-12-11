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
// vid_cgl.c -- Core OpenGL interface to the OS X implementation of the OpenGL specification 

#include "quakedef.h"
#include "macquake.h"


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

/*
===============
VID_Init
===============
*/
void VID_Init (void)
{
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

