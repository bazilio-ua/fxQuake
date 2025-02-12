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
// gl_bloom.c -- handle bloom routines

#include "quakedef.h"


/*
============================================================================== 

	LIGHT BLOOMS

2D lighting post process effect - adapted from fteqw
bloom (light bleeding from bright objects)
============================================================================== 
*/

/*
info about bloom algo:
bloom is basically smudging.
screen is nearest-downsampled to some usable scale and filtered to remove low-value light (this is what stops non-bright stuff from blooming)
this filtered image is then downsized multiple times
the downsized image is then blured 
the downsized images are then blured horizontally, and then vertically.
final pass simply adds each blured level to the original image.
all samples are then added together for final rendering (with some kind of tone mapping if you want proper hdr).

note: the horizontal/vertical bluring is a guassian filter
note: bloom comes from the fact that the most downsampled image doesn't have too many pixels. the pixels that it does have are spread over a large area.

http://prideout.net/archive/bloom/ contains some sample code

old link: http://www.quakesrc.org/forums/viewtopic.php?t=4340&start=0
*/


static int bloom_size;

cvar_t r_bloom					= {"r_bloom", "0", CVAR_ARCHIVE};
cvar_t r_bloom_darken			= {"r_bloom_darken", "1", CVAR_ARCHIVE};
cvar_t r_bloom_alpha			= {"r_bloom_alpha", "0.2", CVAR_ARCHIVE};
cvar_t r_bloom_intensity		= {"r_bloom_intensity", "0.8", CVAR_ARCHIVE};
cvar_t r_bloom_diamond_size		= {"r_bloom_diamond_size", "8", CVAR_ARCHIVE};
cvar_t r_bloom_sample_size		= {"r_bloom_sample_size", "64", CVAR_ARCHIVE}; // was 512
cvar_t r_bloom_fast_sample		= {"r_bloom_fast_sample", "0", CVAR_ARCHIVE};

gltexture_t *bloomscreentexture;
gltexture_t *bloomeffecttexture;
gltexture_t *bloombackuptexture;
gltexture_t *bloomdownsamplingtexture;

static int screen_downsampling_texture_size;
static int screen_texture_width, screen_texture_height;
static int screen_backup_texture_width, screen_backup_texture_height;

// texture coordinates of screen data inside screen texture
static float screen_texture_coord_width;
static float screen_texture_coord_height;

static int sample_texture_width;
static int sample_texture_height;

// texture coordinates of adjusted textures
static float sample_texture_coord_width;
static float sample_texture_coord_height;

/*
=================
R_Bloom_InitTextures
=================
*/
void R_Bloom_InitTextures (void)
{
	byte *bloomscreendata;
	byte *bloomeffectdata;
	byte *bloomdownsamplingdata;
	byte *bloombackupdata;
	int limit;
	int mark;

	// find closer power of 2 to screen size
	for (screen_texture_width = 1; screen_texture_width < glwidth; screen_texture_width <<= 1)
		;
	for (screen_texture_height = 1; screen_texture_height < glheight; screen_texture_height <<= 1)
		;

	// disable blooms if we can't handle a texture of that size
	if (screen_texture_width > TexMgr_SafeTextureSize(screen_texture_width) || screen_texture_height > TexMgr_SafeTextureSize(screen_texture_height))
	{
		screen_texture_width = screen_texture_height = 0;
		Cvar_SetValue ("r_bloom", 0);
		Con_Warning ("R_Bloom_InitTextures: too high resolution for Light Bloom. Effect disabled\n");
		return;
	}

	mark = Hunk_LowMark ();

	// init the screen texture
	bloomscreendata = Hunk_Alloc (screen_texture_width * screen_texture_height * 4);
	bloomscreentexture = TexMgr_LoadTexture (NULL, "bloomscreentexture", screen_texture_width, screen_texture_height, SRC_BLOOM, 
										 bloomscreendata,
										 "",
										 (uintptr_t)bloomscreendata, TEXPREF_BLOOM | TEXPREF_LINEAR);

	// validate bloom size
	if (r_bloom_sample_size.value < 32)
		Cvar_SetValue ("r_bloom_sample_size", 32);

	// make sure bloom size doesn't have funny values
	limit = min( (int)r_bloom_sample_size.value, min( screen_texture_width, screen_texture_height ) );

	// make sure bloom size is a power of 2
	for( bloom_size = 32; (bloom_size<<1) <= limit; bloom_size <<= 1 )
		;

	if (bloom_size != r_bloom_sample_size.value)
		Cvar_SetValue ("r_bloom_sample_size", bloom_size);

	// init the bloom effect texture
	bloomeffectdata = Hunk_Alloc (bloom_size * bloom_size * 4);
	bloomeffecttexture = TexMgr_LoadTexture (NULL, "bloomeffecttexture", bloom_size, bloom_size, SRC_BLOOM, 
										 bloomeffectdata,
										 "",
										 (uintptr_t)bloomeffectdata, TEXPREF_BLOOM | TEXPREF_LINEAR);

	// if screen size is more than 2x the bloom effect texture, set up for stepped downsampling
	bloomdownsamplingtexture = NULL;
	screen_downsampling_texture_size = 0;

	if ( (glwidth > (bloom_size * 2) || glheight > (bloom_size * 2) ) && !r_bloom_fast_sample.value)
	{
		screen_downsampling_texture_size = (int)(bloom_size * 2);
		bloomdownsamplingdata = Hunk_Alloc (screen_downsampling_texture_size * screen_downsampling_texture_size * 4);
		bloomdownsamplingtexture = TexMgr_LoadTexture (NULL, "bloomdownsamplingtexture", screen_downsampling_texture_size, screen_downsampling_texture_size, SRC_BLOOM, 
												   bloomdownsamplingdata,
												   "",
												   (uintptr_t)bloomdownsamplingdata, TEXPREF_BLOOM | TEXPREF_LINEAR);
	}

	// init the screen backup texture
	if (screen_downsampling_texture_size)
	{
		screen_backup_texture_width  = screen_downsampling_texture_size;
		screen_backup_texture_height = screen_downsampling_texture_size;
	}
	else
	{
		screen_backup_texture_width  = bloom_size;
		screen_backup_texture_height = bloom_size;
	}

	bloombackupdata = Hunk_Alloc (screen_backup_texture_width * screen_backup_texture_height * 4);
	bloombackuptexture = TexMgr_LoadTexture (NULL, "bloombackuptexture", screen_backup_texture_width, screen_backup_texture_height, SRC_BLOOM, 
										 bloombackupdata,
										 "",
										 (uintptr_t)bloombackupdata, TEXPREF_BLOOM | TEXPREF_LINEAR);

	Hunk_FreeToLowMark (mark);
}

/*
=================
R_InitBloomTextures
=================
*/
void R_InitBloomTextures (void)
{
	bloom_size = 0;
	bloomscreentexture = NULL;	// first init

	if (!r_bloom.value)
		return;

	R_Bloom_InitTextures ();
}

/*
=================
R_Bloom_SamplePass

sample size workspace coordinates
=================
*/
static inline void R_Bloom_SamplePass (int x, int y)
{
	glBegin(GL_QUADS);
	glTexCoord2f(0, sample_texture_coord_height);
	glVertex2f(x, y);
	glTexCoord2f(0, 0);
	glVertex2f(x, y+sample_texture_height);
	glTexCoord2f(sample_texture_coord_width, 0);
	glVertex2f(x+sample_texture_width, y+sample_texture_height);
	glTexCoord2f(sample_texture_coord_width, sample_texture_coord_height);
	glVertex2f(x+sample_texture_width, y);
	glEnd();
}

/*
=================
R_Bloom_Quad
=================
*/
static inline void R_Bloom_Quad (int x, int y, int width, int height, float texwidth, float texheight)
{
	glBegin(GL_QUADS);
	glTexCoord2f(0, texheight);
	glVertex2f(x, y);
	glTexCoord2f(0, 0);
	glVertex2f(x, y+height);
	glTexCoord2f(texwidth, 0);
	glVertex2f(x+width, y+height);
	glTexCoord2f(texwidth, texheight);
	glVertex2f(x+width, y);
	glEnd();
}

/*
=================
R_Bloom_DrawEffect
=================
*/
void R_Bloom_DrawEffect (void)
{
	float	alpha;

	alpha = CLAMP(0.0, r_bloom_alpha.value, 1.0);

	GL_BindTexture (bloomeffecttexture);

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glColor4f(alpha, alpha, alpha, 1.0f);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glBegin(GL_QUADS);
	glTexCoord2f(0, sample_texture_coord_height);
	glVertex2f(glx, gly);
	glTexCoord2f(0, 0);
	glVertex2f(glx, gly + glheight);
	glTexCoord2f(sample_texture_coord_width, 0);
	glVertex2f(glx + glwidth, gly + glheight);
	glTexCoord2f(sample_texture_coord_width, sample_texture_coord_height);
	glVertex2f(glx + glwidth, gly);
	glEnd();

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_BLEND);
}

/*
=================
R_Bloom_GeneratexDiamonds
=================
*/
void R_Bloom_GeneratexDiamonds (void)
{
	int		i, j;
	float	intensity, scale, rad, point;

	// setup sample size workspace
	glViewport (0, 0, sample_texture_width, sample_texture_height);

	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();

	glOrtho (0, sample_texture_width, sample_texture_height, 0, -10, 100);

	glMatrixMode (GL_MODELVIEW);
	glLoadIdentity ();

	// copy small scene into bloomeffecttexture
	GL_BindTexture (bloomeffecttexture);
	glCopyTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_texture_width, sample_texture_height);

	// start modifying the small scene corner
	glColor4f (1.0f, 1.0f, 1.0f, 1.0f);
	glEnable (GL_BLEND);

	// darkening passes
	if (r_bloom_darken.value < 0)
		Cvar_SetValue("r_bloom_darken", 0);

    glBlendFunc (GL_DST_COLOR, GL_ZERO);
    glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    for (i = 0; i < r_bloom_darken.value; i++) 
    {
        R_Bloom_SamplePass (0, 0);
    }

    glCopyTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_texture_width, sample_texture_height);

	// bluring passes
    if (r_bloom_diamond_size.value < 2)
        Cvar_SetValue("r_bloom_diamond_size", 2);
    if (r_bloom_intensity.value < 0)
        Cvar_SetValue("r_bloom_intensity", 0);

    rad = r_bloom_diamond_size.value / 2.0f;
    point = (r_bloom_diamond_size.value - 1) / 2.0f;
    scale = min(1.0f, r_bloom_intensity.value * 2.0f / rad);

	glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_COLOR);

	for (i = 0; i < r_bloom_diamond_size.value; i++) 
	{
		for (j = 0; j < r_bloom_diamond_size.value; j++) 
		{
			intensity = scale * ((point + 1.0f) - (fabs(point - i) + fabs(point - j))) / (point + 1.0f);
			if (intensity < 0.005f)
				continue;

			glColor4f (intensity, intensity, intensity, 1.0f);
			R_Bloom_SamplePass (i - rad, j - rad);
		}
	}

	glCopyTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_texture_width, sample_texture_height);

	glDisable (GL_BLEND);
	glColor4f (1.0f, 1.0f, 1.0f, 1.0f);
}

/*
=================
R_Bloom_DownsampleView
=================
*/
void R_Bloom_DownsampleView (void)
{
	//
	// setup textures coordinates
	//
	screen_texture_coord_width = ((float)glwidth / (float)screen_texture_width);
	screen_texture_coord_height = ((float)glheight / (float)screen_texture_height);

	if (glheight > glwidth)
	{
		sample_texture_coord_width = ((float)glwidth / (float)glheight);
		sample_texture_coord_height = 1.0f;
	}
	else
	{
		sample_texture_coord_width = 1.0f;
		sample_texture_coord_height = ((float)glheight / (float)glwidth);
	}

	sample_texture_width = ( bloom_size * sample_texture_coord_width );
	sample_texture_height = ( bloom_size * sample_texture_coord_height );


	if (screen_downsampling_texture_size)
	{
		// stepped downsample
		int midsample_texture_width = ( screen_downsampling_texture_size * sample_texture_coord_width );
		int midsample_texture_height = ( screen_downsampling_texture_size * sample_texture_coord_height );

		// copy the screen and draw resized
		GL_BindTexture (bloomscreentexture);
		glCopyTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, glx, glheight - (gly + glheight), glwidth, glheight);
		R_Bloom_Quad (0, glheight - midsample_texture_height, midsample_texture_width, midsample_texture_height, screen_texture_coord_width, screen_texture_coord_height);

		// now copy into downsampling (mid-sized) texture
		GL_BindTexture (bloomdownsamplingtexture);
		glCopyTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, 0, 0, midsample_texture_width, midsample_texture_height);

		// now draw again in bloom size
		glColor4f (0.5f, 0.5f, 0.5f, 1.0f);
		R_Bloom_Quad (0, glheight - sample_texture_height, sample_texture_width, sample_texture_height, sample_texture_coord_width, sample_texture_coord_height);

		// now blend the big screen texture into the bloom generation space (hoping it adds some blur)
		glEnable (GL_BLEND);
		glBlendFunc (GL_ONE, GL_ONE);
		glColor4f (0.5f, 0.5f, 0.5f, 1.0f);
		GL_BindTexture (bloomscreentexture);
		R_Bloom_Quad (0, glheight - sample_texture_height, sample_texture_width, sample_texture_height, screen_texture_coord_width, screen_texture_coord_height);
		glColor4f (1.0f, 1.0f, 1.0f, 1.0f);
		glDisable (GL_BLEND);
	} 
	else
	{
		// downsample simple
		GL_BindTexture (bloomscreentexture);
		glCopyTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, glx, glheight - (gly + glheight), glwidth, glheight);
		R_Bloom_Quad (0, glheight - sample_texture_height, sample_texture_width, sample_texture_height, screen_texture_coord_width, screen_texture_coord_height);
	}
}

/*
=================
R_BloomBlend
=================
*/
void R_BloomBlend (void)
{
	if (!r_bloom.value)
		return;

	if (!bloom_size || screen_texture_width < glwidth || screen_texture_height < glheight)
		R_Bloom_InitTextures ();

	// previous function can unset this
	if (!r_bloom.value)
		return;

	if (screen_texture_width < bloom_size || screen_texture_height < bloom_size)
		return;

	//
	// setup full screen workspace
	//
	glViewport (0, 0, glwidth, glheight);

	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();

	glOrtho (0, glwidth, glheight, 0, -10, 100);

	glMatrixMode (GL_MODELVIEW);
	glLoadIdentity ();

	//
	// set drawing parms
	// 
	glDisable (GL_DEPTH_TEST);
	glDisable (GL_CULL_FACE);

	//
	// copy the screen space we'll use to work into the backup texture
	//
	GL_BindTexture (bloombackuptexture);
	glCopyTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, 0, 0, screen_backup_texture_width, screen_backup_texture_height);

	//
	// create the bloom image
	//
	R_Bloom_DownsampleView ();
	R_Bloom_GeneratexDiamonds ();

	//
	// restore full screen workspace
	//
	glViewport (0, 0, glwidth, glheight);

	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();

	glOrtho (0, glwidth, glheight, 0, -10, 100);

	glMatrixMode (GL_MODELVIEW);
	glLoadIdentity ();

	//
	// restore the screen-backup to the screen
	//
	GL_BindTexture (bloombackuptexture);
	R_Bloom_Quad (0, glheight - screen_backup_texture_height, screen_backup_texture_width, screen_backup_texture_height, 1.0f, 1.0f);
	
	// draw the bloom effect
	R_Bloom_DrawEffect ();
}

