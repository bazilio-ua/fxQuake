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
// gl_warp.c -- warping animation support

#include "quakedef.h"


cvar_t	r_waterquality = {"r_waterquality", "12", CVAR_NONE};
cvar_t	r_flatturb = {"r_flatturb","0", CVAR_NONE};

/*
==============================================================================

	RENDER-TO-FRAMEBUFFER WATER (adapted from fitzquake)

==============================================================================
*/

// speed up sin calculations - Ed
float turbsin[] =
{
	#include "gl_warp_sin.h"
};

/*
=============
R_UpdateWarpTextures

each frame, update warping textures
=============
*/
void R_UpdateWarpTextures (void)
{
	texture_t *tx;
	int i;
	float x, y, x2, warptess;

	warptess = 128.0 / CLAMP(4.0, floor(r_waterquality.value), 64.0);

	for (i=0; i<cl.worldmodel->numtextures; i++)
	{
		if (!(tx = cl.worldmodel->textures[i]))
			continue;

		if (tx->name[0] != '*')
			continue;

		if (!tx->update_warp)
			continue;

		// render warp
		glViewport (glx, gly + glheight - gl_warpimage_size, gl_warpimage_size, gl_warpimage_size);

		glMatrixMode (GL_PROJECTION);
		glLoadIdentity ();

		glOrtho (0, 128, 0, 128, -99999, 99999);

		glMatrixMode (GL_MODELVIEW);
		glLoadIdentity ();

		glDisable (GL_ALPHA_TEST); //FX new
		glEnable (GL_BLEND); //FX
		GL_BindTexture (tx->base);
		for (x=0.0; x<128.0; x=x2)
		{
			x2 = x + warptess;
			glBegin (GL_TRIANGLE_STRIP);
			for (y=0.0; y<128.01; y+=warptess) // .01 for rounding errors
			{
				glTexCoord2f (WARPCALC(x,y), WARPCALC(y,x));
				glVertex2f (x,y);
				glTexCoord2f (WARPCALC(x2,y), WARPCALC(y,x2));
				glVertex2f (x2,y);
			}
			glEnd ();
		}
		glEnable (GL_ALPHA_TEST); //FX new
		glDisable (GL_BLEND); //FX

		// copy to texture
		GL_BindTexture (tx->warpimage);
		glCopyTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, glx, gly + glheight - gl_warpimage_size, gl_warpimage_size, gl_warpimage_size);

		tx->update_warp = false;
	}

	// if warp render went down into sbar territory, we need to be sure to refresh it next frame
	if (gl_warpimage_size + sb_lines > vid.height)
		Sbar_Changed ();
}

