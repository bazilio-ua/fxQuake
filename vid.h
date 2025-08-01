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
// vid.h -- video driver defs

#define VID_CBITS    6
#define VID_GRADES    (1 << VID_CBITS)

typedef struct vrect_s
{
	int				x,y,width,height;
	struct vrect_s	*pnext;
} vrect_t;

typedef struct
{
	byte			*colormap;		// 256 * VID_GRADES size
	int				width;		
	int				height;
	int				refreshrate;
	int				bpp;
	qboolean		fullscreen;
	int				numpages;
	qboolean		recalc_refdef;	// if true, recalc vid-based stuff
	int				conwidth;
	int				conheight;
} viddef_t;

extern viddef_t vid; // global video state

extern void (*vid_menudrawfn)(void);
extern void (*vid_menukeyfn)(int key);
extern void (*vid_menucmdfn)(void); //johnfitz

extern qboolean vid_hiddenwindow;
extern qboolean vid_activewindow;
extern qboolean vid_notifywindow;


void	VID_Init (void);
// Called at startup to set up translation tables, takes 256 8 bit RGB values
// the palette data will go away after the call, so it must be copied off if
// the video driver will need it again

void	VID_Shutdown (void);
// Called at shutdown

void	VID_Toggle (void);
// Called by alt-return key binding


void VID_MenuInit (void); //johnfitz
void VID_MenuCmd (void); //johnfitz
void VID_MenuDraw (void);
void VID_MenuKey (int key);
