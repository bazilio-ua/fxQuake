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
// gl_draw.c -- this is the only file outside the refresh that touches the vid buffer

#include "quakedef.h"

cvar_t		scr_conalpha = {"scr_conalpha", "1", CVAR_ARCHIVE};

byte		*draw_chars;				// 8*8 graphic characters
qpic_t		*draw_disc;
qpic_t		*draw_backtile;

gltexture_t	*char_texture; 

/*
=============================================================================

  scrap allocation

  Allocate all the little status bar objects into a single texture
  to crutch up stupid hardware / drivers

=============================================================================
*/

#define	MAX_SCRAPS		2
#define	SCRAP_WIDTH		256
#define	SCRAP_HEIGHT	256

int			scrap_allocated[MAX_SCRAPS][SCRAP_WIDTH];
byte		scrap_texels[MAX_SCRAPS][SCRAP_WIDTH*SCRAP_HEIGHT];
qboolean	scrap_dirty;
gltexture_t	*scrap_textures[MAX_SCRAPS]; // changed to array

// returns a texture number and the position inside it
int Scrap_AllocBlock (int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;
	int		texnum;

	for (texnum=0 ; texnum<MAX_SCRAPS ; texnum++)
	{
		best = SCRAP_HEIGHT;

		for (i=0 ; i<SCRAP_WIDTH-w ; i++)
		{
			best2 = 0;

			for (j=0 ; j<w ; j++)
			{
				if (scrap_allocated[texnum][i+j] >= best)
					break;
				if (scrap_allocated[texnum][i+j] > best2)
					best2 = scrap_allocated[texnum][i+j];
			}
			if (j == w)
			{	// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > SCRAP_HEIGHT)
			continue;

		for (i=0 ; i<w ; i++)
			scrap_allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	return -1;
}

void Scrap_Upload (void)
{
	int		i;
	char name[64];

	for (i = 0; i < MAX_SCRAPS; i++) 
	{
		sprintf (name, "scrap%i", i);
		scrap_textures[i] = TexMgr_LoadTexture (NULL, name, SCRAP_WIDTH, SCRAP_HEIGHT, SRC_INDEXED, scrap_texels[i], "", (uintptr_t)scrap_texels[i], TEXPREF_ALPHA | TEXPREF_OVERWRITE | TEXPREF_NOPICMIP);
	}
	scrap_dirty = false;
}


//=============================================================================
/* Support Routines */

typedef struct cachepic_s
{
	char		name[MAX_QPATH];
	qpic_t		pic;
	byte		padding[32];	// for appended glpic
} cachepic_t;

#define	MAX_CACHED_PICS		128
cachepic_t	menu_cachepics[MAX_CACHED_PICS];
int			menu_numcachepics;


qpic_t *Draw_PicFromWad (char *name)
{
	qpic_t	*p;
	glpic_t	*glp;
	uintptr_t offset; //johnfitz
	char texturename[64]; //johnfitz

	p = W_GetLumpName (name);

	// Sanity ...
	if (p->width & 0xC0000000 || p->height & 0xC0000000)
		Sys_Error ("Draw_PicFromWad: invalid dimensions (%dx%d) for '%s'", p->width, p->height, name);

	glp = (glpic_t *)p->data;

	// load little ones into the scrap
	if (p->width < 64 && p->height < 64)
	{
		int		x, y;
		int		i, j, k;
		int		texnum;

		texnum = Scrap_AllocBlock (p->width, p->height, &x, &y);
		if (texnum == -1)
			Sys_Error ("Scrap_AllocBlock: full");

		scrap_dirty = true;
		k = 0;
		for (i=0 ; i<p->height ; i++)
			for (j=0 ; j<p->width ; j++, k++)
				scrap_texels[texnum][(y+i)*SCRAP_WIDTH + x + j] = p->data[k];

		glp->gltexture = scrap_textures[texnum]; // changed to an array
		// no longer go from 0.01 to 0.99
		glp->sl = x/(float)SCRAP_WIDTH;
		glp->sh = (x+p->width)/(float)SCRAP_WIDTH;
		glp->tl = y/(float)SCRAP_HEIGHT;
		glp->th = (y+p->height)/(float)SCRAP_HEIGHT;
	}
	else
	{
		sprintf (texturename, "%s:%s", WADFILE, name); //johnfitz
		offset = (uintptr_t)p - (uintptr_t)wad_base + sizeof(int)*2; //johnfitz
		glp->gltexture = TexMgr_LoadTexture (NULL, texturename, p->width, p->height, SRC_INDEXED, p->data, WADFILE, offset, TEXPREF_ALPHA | TEXPREF_PAD | TEXPREF_NOPICMIP);
		glp->sl = 0;
        glp->sh = (float)p->width/(float)TexMgr_PadConditional(p->width); //johnfitz
		glp->tl = 0;
		glp->th = (float)p->height/(float)TexMgr_PadConditional(p->height); //johnfitz
	}

	return p;
}


/*
================
Draw_CachePic
================
*/
qpic_t *Draw_CachePic (char *path)
{
	cachepic_t	*cpic;
	int			i;
	qpic_t		*p;
	glpic_t		*glp;

	for (cpic=menu_cachepics, i=0 ; i<menu_numcachepics ; cpic++, i++)
		if (!strcmp (path, cpic->name))
			return &cpic->pic;

	if (menu_numcachepics == MAX_CACHED_PICS)
		Sys_Error ("Draw_CachePic: menu_numcachepics == MAX_CACHED_PICS (%d)", MAX_CACHED_PICS);
	menu_numcachepics++;
	strcpy (cpic->name, path);

//
// load the pic from disk
//
	p = (qpic_t *)COM_LoadTempFile (path, NULL);
	if (!p)
		Sys_Error ("Draw_CachePic: failed to load %s", path);
	SwapPic (p);


	cpic->pic.width = p->width;
	cpic->pic.height = p->height;

	glp = (glpic_t *)cpic->pic.data;
	glp->gltexture = TexMgr_LoadTexture (NULL, path, p->width, p->height, SRC_INDEXED, p->data, path, sizeof(int)*2, TEXPREF_ALPHA | TEXPREF_PAD | TEXPREF_NOPICMIP);
	glp->sl = 0;
	glp->sh = (float)p->width/(float)TexMgr_PadConditional(p->width); //johnfitz
	glp->tl = 0;
	glp->th = (float)p->height/(float)TexMgr_PadConditional(p->height); //johnfitz

	return &cpic->pic;
}


/*
===============
Draw_LoadPics
===============
*/
void Draw_LoadPics (void)
{
	uintptr_t	offset; // johnfitz
	char		texturename[64]; //johnfitz

	// load the console background and the charset
	// by hand, because we need to write the version
	// string into the background before turning
	// it into a texture
	draw_chars = W_GetLumpName ("conchars");

	if (!draw_chars)
		Sys_Error ("Draw_LoadPics: couldn't load conchars");

	// now turn them into textures
	sprintf (texturename, "%s:%s", WADFILE, "conchars"); // johnfitz
	offset = (uintptr_t)draw_chars - (uintptr_t)wad_base;
	char_texture = TexMgr_LoadTexture (NULL, texturename, 128, 128, SRC_INDEXED, draw_chars, WADFILE, offset, TEXPREF_ALPHA | TEXPREF_NEAREST | TEXPREF_NOPICMIP | TEXPREF_CONCHARS);

	//
	// get the other pics we need
	//
	draw_disc = Draw_PicFromWad ("disc");
	draw_backtile = Draw_PicFromWad ("backtile");
}


/*
===============
Draw_NewGame -- johnfitz
===============
*/
void Draw_NewGame (void)
{
	cachepic_t	*cpic;
	int			i;

	// empty scrap and reallocate gltextures
	memset(&scrap_allocated, 0, sizeof(scrap_allocated));
	memset(&scrap_texels, 255, sizeof(scrap_texels));
	Scrap_Upload (); // creates 2 empty gltextures

	// reload wad pics
	W_LoadWadFile (); //johnfitz -- filename is now hard-coded for honesty
	Draw_LoadPics ();
	SCR_LoadPics ();
	Sbar_LoadPics ();

	// empty lmp cache
	for (cpic=menu_cachepics, i=0 ; i<menu_numcachepics ; cpic++, i++)
		cpic->name[0] = 0;
	menu_numcachepics = 0;
}


/*
===============
Draw_Init

rewritten
===============
*/
void Draw_Init (void)
{
	Cvar_RegisterVariable (&scr_conalpha);

	// clear scrap and allocate gltextures
	memset(&scrap_allocated, 0, sizeof(scrap_allocated));
	memset(&scrap_texels, 255, sizeof(scrap_texels));
	Scrap_Upload (); // creates 2 empty textures

	// load pics
	Draw_LoadPics ();
}

/*
===============
Draw_Crosshair
===============
*/
void Draw_Crosshair (void)
{
	if (!crosshair.value) 
		return;

	Draw_Character (scr_vrect.x + scr_vrect.width/2 + cl_crossx.value, scr_vrect.y + scr_vrect.height/2 + cl_crossy.value, '+');
}

/*
================
Draw_CharacterQuad

seperate function to spit out verts
================
*/
void Draw_CharacterQuad (int x, int y, char num)
{
	int				row, col;
	float			frow, fcol, size;

	row = num>>4;
	col = num&15;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.0625;

	glTexCoord2f (fcol, frow);
	glVertex2f (x, y);
	glTexCoord2f (fcol + size, frow);
	glVertex2f (x+8, y);
	glTexCoord2f (fcol + size, frow + size);
	glVertex2f (x+8, y+8);
	glTexCoord2f (fcol, frow + size);
	glVertex2f (x, y+8);
}

/*
================
Draw_Character

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.

modified to call Draw_CharacterQuad
================
*/
void Draw_Character (int x, int y, int num)
{
	if (y <= -8)
		return;			// totally off screen

	num &= 255;

	if (num == 32)
		return; // don't waste verts on spaces

	GL_BindTexture (char_texture);
	glBegin (GL_QUADS);

	Draw_CharacterQuad (x, y, (char) num);

	glEnd ();
}

/*
================
Draw_String

modified to call Draw_CharacterQuad
================
*/
void Draw_String (int x, int y, char *str)
{
	if (y <= -8)
		return;			// totally off screen

	GL_BindTexture (char_texture);
	glBegin (GL_QUADS);

	while (*str)
	{
		if (*str != 32) // don't waste verts on spaces
			Draw_CharacterQuad (x, y, *str);
		str++;
		x += 8;
	}

	glEnd ();
}


/*
=============
Draw_AlphaPic
=============
*/
void Draw_AlphaPic (int x, int y, qpic_t *pic, float alpha)
{
	glpic_t			*glp;

	if (scrap_dirty)
		Scrap_Upload ();

	glp = (glpic_t *)pic->data;

	glDisable (GL_ALPHA_TEST);
	glEnable (GL_BLEND);
	glColor4f (1,1,1,alpha);
	GL_BindTexture (glp->gltexture);
	glBegin (GL_QUADS);
	glTexCoord2f (glp->sl, glp->tl);
	glVertex2f (x, y);
	glTexCoord2f (glp->sh, glp->tl);
	glVertex2f (x+pic->width, y);
	glTexCoord2f (glp->sh, glp->th);
	glVertex2f (x+pic->width, y+pic->height);
	glTexCoord2f (glp->sl, glp->th);
	glVertex2f (x, y+pic->height);
	glEnd ();
	glColor4f (1,1,1,1);
	glEnable (GL_ALPHA_TEST);
	glDisable (GL_BLEND);
}


/*
=============
Draw_Pic
=============
*/
void Draw_Pic (int x, int y, qpic_t *pic)
{
	glpic_t			*glp;

	if (scrap_dirty)
		Scrap_Upload ();

	glp = (glpic_t *)pic->data;

	glDisable (GL_ALPHA_TEST); //FX new
	glEnable (GL_BLEND); //FX
	glColor4f (1,1,1,1);
	GL_BindTexture (glp->gltexture);
	glBegin (GL_QUADS);
	glTexCoord2f (glp->sl, glp->tl);
	glVertex2f (x, y);
	glTexCoord2f (glp->sh, glp->tl);
	glVertex2f (x+pic->width, y);
	glTexCoord2f (glp->sh, glp->th);
	glVertex2f (x+pic->width, y+pic->height);
	glTexCoord2f (glp->sl, glp->th);
	glVertex2f (x, y+pic->height);
	glEnd ();
	glEnable (GL_ALPHA_TEST); //FX new
	glDisable (GL_BLEND); //FX
}


/*
=============
Draw_SubPic
=============
*/
void Draw_SubPic (int x, int y, qpic_t *pic, int srcx, int srcy, int width, int height)
{
	float			newsl, newtl, newsh, newth, oldglwidth, oldglheight; 
	glpic_t			*glp;

	if (scrap_dirty)
		Scrap_Upload ();

	glp = (glpic_t *)pic->data;

	oldglwidth = glp->sh - glp->sl;
	oldglheight = glp->th - glp->tl;

	newsl = glp->sl + (srcx * oldglwidth) / pic->width;
	newsh = newsl + (width * oldglwidth) / pic->width;

	newtl = glp->tl + (srcy * oldglheight) / pic->height;
	newth = newtl + (height * oldglheight) / pic->height; 

	glDisable (GL_ALPHA_TEST); //FX new
	glEnable (GL_BLEND); //FX
	glColor4f (1,1,1,1);
	GL_BindTexture (glp->gltexture);
	glBegin (GL_QUADS);
	glTexCoord2f (newsl, newtl);
	glVertex2f (x, y);
	glTexCoord2f (newsh, newtl);
	glVertex2f (x+width, y);
	glTexCoord2f (newsh, newth);
	glVertex2f (x+width, y+height);
	glTexCoord2f (newsl, newth);
	glVertex2f (x, y+height);
	glEnd ();
	glEnable (GL_ALPHA_TEST); //FX new
	glDisable (GL_BLEND); //FX
}


/*
=============
Draw_TransPic
=============
*/
void Draw_TransPic (int x, int y, qpic_t *pic)
{
	if (x < 0 || (x + pic->width) > vid.width || y < 0 || (y + pic->height) > vid.height)
	{
		Sys_Error ("Draw_TransPic: bad coordinates (%d, %d)", x, y);
	}

	Draw_Pic (x, y, pic);
}

/*
=============
Draw_TransPicTranslate

-- johnfitz -- rewritten to use texmgr to do translation
Only used for the player color selection menu
=============
*/
void Draw_TransPicTranslate (int x, int y, qpic_t *pic, int top, int bottom)
{
	static int oldtop = -2;
	static int oldbottom = -2;
	
	if (top != oldtop || bottom != oldbottom)
	{
		glpic_t *glp = (glpic_t *)pic->data;
		gltexture_t *glt = glp->gltexture;
		oldtop = top;
		oldbottom = bottom;
		TexMgr_ReloadTextureTranslation (glt, top, bottom);
	}
	Draw_Pic (x, y, pic);
}


/*
================
Draw_ConsoleBackground

================
*/
void Draw_ConsoleBackground (int lines)
{
	qpic_t *pic;
	int y;
	float alpha;

	pic = Draw_CachePic ("gfx/conback.lmp");
	pic->width = vid.width;
	pic->height = vid.height;

	alpha = (con_forcedup) ? 1.0 : CLAMP(0.0, scr_conalpha.value, 1.0);

	y = (vid.height * 3) >> 2;

	if (lines > y)
		Draw_Pic(0, lines - vid.height, pic);
	else
//		Draw_AlphaPic (0, lines - vid.height, pic, (float)(2 * alpha * lines)/y); //alpha depend on height console
		Draw_AlphaPic (0, lines - vid.height, pic, alpha);
}


/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void Draw_TileClear (int x, int y, int w, int h)
{
	glpic_t	*glp;

	glp = (glpic_t *)draw_backtile->data;

	glDisable (GL_ALPHA_TEST); //FX new
	glEnable (GL_BLEND); //FX
	glColor4f (1,1,1,1); //FX new
	GL_BindTexture (glp->gltexture);
	glBegin (GL_QUADS);
	glTexCoord2f (x/64.0, y/64.0);
	glVertex2f (x, y);
	glTexCoord2f ( (x+w)/64.0, y/64.0);
	glVertex2f (x+w, y);
	glTexCoord2f ( (x+w)/64.0, (y+h)/64.0);
	glVertex2f (x+w, y+h);
	glTexCoord2f ( x/64.0, (y+h)/64.0 );
	glVertex2f (x, y+h);
	glEnd ();
	glEnable (GL_ALPHA_TEST); //FX new
	glDisable (GL_BLEND); //FX
}


/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill (int x, int y, int w, int h, int c)
{
	byte *pal = (byte *)d_8to24table; // use d_8to24table instead of host_basepal
	float alpha = 1.0;

	glDisable (GL_TEXTURE_2D);
	glEnable (GL_BLEND); // for alpha
	glDisable (GL_ALPHA_TEST); // for alpha
	glColor4f (pal[c*4]/255.0, pal[c*4+1]/255.0, pal[c*4+2]/255.0, alpha); // added alpha

	glBegin (GL_QUADS);
	glVertex2f (x, y);
	glVertex2f (x+w, y);
	glVertex2f (x+w, y+h);
	glVertex2f (x, y+h);
	glEnd ();

	glColor3f (1,1,1);
	glDisable (GL_BLEND); // for alpha
	glEnable (GL_ALPHA_TEST); // for alpha
	glEnable (GL_TEXTURE_2D);
}

//=============================================================================

/*
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen (void)
{
	glEnable (GL_BLEND);
	glDisable (GL_ALPHA_TEST);
	glDisable (GL_TEXTURE_2D);
	glColor4f (0, 0, 0, 0.5);

	glBegin (GL_QUADS);
	glVertex2f (0, 0);
	glVertex2f (vid.width, 0);
	glVertex2f (vid.width, vid.height);
	glVertex2f (0, vid.height);
	glEnd ();

	glColor4f (1,1,1,1);
	glEnable (GL_TEXTURE_2D);
	glEnable (GL_ALPHA_TEST);
	glDisable (GL_BLEND);

	Sbar_Changed();
}

//=============================================================================

/*
================
Draw_BeginDisc

Draws the little blue disc in the corner of the screen.
Call before beginning any disc IO.
================
*/
void Draw_BeginDisc (void)
{
	if (!draw_disc || block_drawing || isIntel) // intel video workaround
		return;

	Draw_Pic (vid.width - 24, 0, draw_disc);
}


/*
================
Draw_EndDisc

Erases the disc icon.
Call after completing any disc IO
================
*/
void Draw_EndDisc (void)
{
	
}

