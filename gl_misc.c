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
// gl_misc.c

#include "quakedef.h"

texture_t	*notexture_mip; // moved here from gl_main.c 
texture_t	*notexture_mip2; // used for non-lightmapped surfs with a missing texture

gltexture_t *particletexture;	// particle texture
gltexture_t *particletexture1;	// circle
gltexture_t *particletexture2;	// square

/*
==================
R_InitTextures
==================
*/
void R_InitTextures (void)
{
	// create a simple checkerboard texture for the default (notexture miptex)
	notexture_mip = Hunk_AllocName (sizeof(texture_t), "notexture_mip");
	strcpy (notexture_mip->name, "notexture");
	notexture_mip->height = notexture_mip->width = 16;

	notexture_mip2 = Hunk_AllocName (sizeof(texture_t), "notexture_mip2");
	strcpy (notexture_mip2->name, "notexture2");
	notexture_mip2->height = notexture_mip2->width = 16;
}

/*
===============
R_LoadPalette
===============
*/
void R_LoadPalette (void)
{
	byte *pal, *src, *dst;
	int i;

	host_basepal = COM_LoadHunkFile ("gfx/palette.lmp", NULL);
	if (!host_basepal)
		Sys_Error ("R_LoadPalette: couldn't load gfx/palette.lmp");
    host_colormap = COM_LoadHunkFile ("gfx/colormap.lmp", NULL);
    if (!host_colormap)
        Sys_Error ("R_LoadPalette: couldn't load gfx/colormap.lmp");

	pal = host_basepal;

	//
	//standard palette, 255 is transparent
	//
	dst = (byte *)d_8to24table;
	src = pal;
	for (i=0; i<256; i++)
	{
		dst[0] = *src++;
		dst[1] = *src++;
		dst[2] = *src++;
		dst[3] = 255;
		dst += 4;
	}
	((byte *)&d_8to24table[255])[3] = 0;

	//
	//fullbright palette, 0-223 are black (for additive blending)
	//
	src = pal + 224*3;
	dst = (byte *)&d_8to24table_fbright[224];
	for (i=224; i<256; i++)
	{
		dst[0] = *src++;
		dst[1] = *src++;
		dst[2] = *src++;
		dst[3] = 255;
		dst += 4;
	}
	for (i=0; i<224; i++)
	{
		dst = (byte *)&d_8to24table_fbright[i];
		dst[0] = 0;
		dst[1] = 0;
		dst[2] = 0;
		dst[3] = 255;
	}

	//
	//nobright palette, 224-255 are black (for additive blending)
	//
	dst = (byte *)d_8to24table_nobright;
	src = pal;
	for (i=0; i<256; i++)
	{
		dst[0] = *src++;
		dst[1] = *src++;
		dst[2] = *src++;
		dst[3] = 255;
		dst += 4;
	}
	for (i=224; i<256; i++)
	{
		dst = (byte *)&d_8to24table_nobright[i];
		dst[0] = 0;
		dst[1] = 0;
		dst[2] = 0;
		dst[3] = 255;
	}

	//
	//fullbright palette, for fence textures
	//
	memcpy(d_8to24table_fbright_fence, d_8to24table_fbright, 256*4);
	d_8to24table_fbright_fence[255] = 0; // alpha of zero

	//
	//nobright palette, for fence textures
	//
	memcpy(d_8to24table_nobright_fence, d_8to24table_nobright, 256*4);
	d_8to24table_nobright_fence[255] = 0; // alpha of zero	
	
	//
	//conchars palette, 0 and 255 are transparent
	//
	memcpy(d_8to24table_conchars, d_8to24table, 256*4);
	((byte *)&d_8to24table_conchars[0])[3] = 0;
}

/*
====================
R_FullBright
====================
*/
void R_FullBright (void)
{
	// Refresh lightmaps
    R_RebuildAllLightmaps ();
}

/*
====================
R_ClearColor
====================
*/
void R_ClearColor (void)
{
	byte *rgb;

	// Refresh clearcolor
	rgb = (byte *)(d_8to24table + ((int)r_clearcolor.value & 0xFF));
	glClearColor (rgb[0]/255.0, rgb[1]/255.0, rgb[2]/255.0, 0);
}

/*
====================
GL_Overbright
====================
*/
void GL_Overbright (void)
{
	if (gl_mtexable && gl_texture_env_combine && gl_texture_env_add)
	{
		float m;
		
		d_overbright = CLAMP(0.0, gl_overbright.value, 2.0);
		m = d_overbright > 0 ? d_overbright : 0.5f;
		d_overbrightscale = OVERBRIGHT_SCALE * m;
	}
	
	// Refresh lightmaps
	R_RebuildAllLightmaps ();
}

/*
===============
R_Init
===============
*/
void R_Init (void)
{
	Cmd_AddCommand ("timerefresh", R_TimeRefresh_f);
	Cmd_AddCommand ("pointfile", R_ReadPointFile_f);

	Cmd_AddCommand ("sky", R_Sky_f);
	Cmd_AddCommand ("loadsky", R_Sky_f); // Nehahra
	Cmd_AddCommand ("fog", R_Fog_f);

	Cvar_RegisterVariable (&r_norefresh);
	Cvar_RegisterVariableCallback (&r_fullbright, R_FullBright);
	Cvar_RegisterVariable (&r_drawentities);
	Cvar_RegisterVariable (&r_drawworld);
	Cvar_RegisterVariable (&r_drawviewmodel);
	Cvar_RegisterVariable (&r_waterquality);
	Cvar_RegisterVariable (&r_wateralpha);
	Cvar_RegisterVariable (&r_lockalpha);
	Cvar_RegisterVariable (&r_lavafog);
	Cvar_RegisterVariable (&r_slimefog);
	Cvar_RegisterVariable (&r_lavaalpha);
	Cvar_RegisterVariable (&r_slimealpha);
	Cvar_RegisterVariable (&r_teleportalpha);
	Cvar_RegisterVariable (&r_dynamic);
	Cvar_RegisterVariable (&r_dynamicscale);
	Cvar_RegisterVariable (&r_novis);
	Cvar_RegisterVariable (&r_lockfrustum);
	Cvar_RegisterVariable (&r_lockpvs);
	Cvar_RegisterVariable (&r_speeds);
	Cvar_RegisterVariable (&r_waterwarp);
	Cvar_RegisterVariableCallback (&r_clearcolor, R_ClearColor);
	Cvar_RegisterVariable (&r_fastsky);
	Cvar_RegisterVariable (&r_skyquality);
	Cvar_RegisterVariable (&r_skyalpha);
	Cvar_RegisterVariable (&r_skyfog);
	Cvar_RegisterVariable (&r_oldsky);

	Cvar_RegisterVariable (&gl_finish);
	Cvar_RegisterVariable (&gl_clear);
	Cvar_RegisterVariable (&gl_cull);
	Cvar_RegisterVariable (&gl_farclip);
	Cvar_RegisterVariable (&gl_smoothmodels);
	Cvar_RegisterVariable (&gl_affinemodels);
	Cvar_RegisterVariable (&gl_polyblend);
	Cvar_RegisterVariable (&gl_flashblend);
	Cvar_RegisterVariable (&gl_flashblendview);
	Cvar_RegisterVariableCallback (&gl_overbright, GL_Overbright);
	Cvar_RegisterVariable (&gl_oldspr);
	Cvar_RegisterVariable (&gl_nocolors);

	// Nehahra
	Cvar_RegisterVariable (&gl_fogenable);
	Cvar_RegisterVariable (&gl_fogdensity);
	Cvar_RegisterVariable (&gl_fogred);
	Cvar_RegisterVariable (&gl_foggreen);
	Cvar_RegisterVariable (&gl_fogblue);

	Cvar_RegisterVariable (&r_bloom);
	Cvar_RegisterVariable (&r_bloom_darken);
	Cvar_RegisterVariable (&r_bloom_alpha);
	Cvar_RegisterVariable (&r_bloom_intensity);
	Cvar_RegisterVariable (&r_bloom_diamond_size);
	Cvar_RegisterVariableCallback (&r_bloom_sample_size, R_InitBloomTextures); // NULL
	Cvar_RegisterVariable (&r_bloom_fast_sample);

	R_InitParticles ();

	R_InitTranslatePlayerTextures ();

	R_InitMapGlobals ();

	R_InitBloomTextures();
}

/*
===============
R_InitTranslatePlayerTextures
===============
*/
static int oldtop[MAX_SCOREBOARD]; 
static int oldbottom[MAX_SCOREBOARD];
static int oldskinnum[MAX_SCOREBOARD];
void R_InitTranslatePlayerTextures (void)
{
	int i;

	for (i = 0; i < MAX_SCOREBOARD; i++)
	{
		oldtop[i] = -1;
		oldbottom[i] = -1;
		oldskinnum[i] = -1;

		playertextures[i] = NULL; //clear playertexture pointers
	}
}

/*
===============
R_TranslatePlayerSkin

Translates a skin texture by the per-player color lookup
===============
*/
void R_TranslatePlayerSkin (int playernum)
{
	int		top, bottom;
	byte	translate[256];
	int		i, size;
	entity_t	*e;
	model_t	*model;
	aliashdr_t *paliashdr;
	byte	*original;
	byte	*src, *dst; 
	byte	*pixels = NULL;
	char		name[64];
	int 	mark;

	//
	// locate the original skin pixels
	//
	e = &cl_entities[1+playernum];
	model = e->model;

	if (!model)
		return;	// player doesn't have a model yet
	if (model->type != mod_alias)
		return; // only translate skins on alias models

	paliashdr = (aliashdr_t *)Mod_Extradata (model);

	top = cl.scores[playernum].colors & 0xf0;
	bottom = (cl.scores[playernum].colors &15)<<4;

	if (!strcmp (e->model->name, "progs/player.mdl"))
	{
		if (top == oldtop[playernum] && bottom == oldbottom[playernum] && e->skinnum == oldskinnum[playernum])
			return; // translate if only player change his color
	}
	else
	{
		oldtop[playernum] = -1;
		oldbottom[playernum] = -1;
		oldskinnum[playernum] = -1;
		goto skip;
	}

	oldtop[playernum] = top;
	oldbottom[playernum] = bottom;
	oldskinnum[playernum] = e->skinnum;

skip:
	for (i=0 ; i<256 ; i++)
		translate[i] = i;

	for (i=0 ; i<16 ; i++)
	{
		if (top < 128)	// the artists made some backwards ranges. sigh.
			translate[TOP_RANGE+i] = top+i;
		else
			translate[TOP_RANGE+i] = top+15-i;

		if (bottom < 128)
			translate[BOTTOM_RANGE+i] = bottom+i;
		else
			translate[BOTTOM_RANGE+i] = bottom+15-i;
	}

	if (e->skinnum < 0 || e->skinnum >= paliashdr->numskins)
		original = (byte *)paliashdr + paliashdr->texels[0];
	else
		original = (byte *)paliashdr + paliashdr->texels[e->skinnum];

	mark = Hunk_LowMark ();

	//
	// translate texture
	//
	sprintf (name, "%s_%i_%i", e->model->name, e->skinnum, playernum);
	size = paliashdr->skinwidth * paliashdr->skinheight;

	// allocate dynamic memory
	pixels = Hunk_Alloc (size);

	dst = pixels;
	src = original;

	for (i=0; i<size; i++)
		*dst++ = translate[*src++];

	original = pixels;

	//upload new image
	playertextures[playernum] = GL_LoadTexture (e->model, name, paliashdr->skinwidth, paliashdr->skinheight, SRC_INDEXED, original, "", (uintptr_t)original, TEXPREF_PAD | TEXPREF_OVERWRITE);

	// free allocated memory
	Hunk_FreeToLowMark (mark);
}


/*
===============
R_NewMap
===============
*/
void R_NewMap (void)
{
	int		i;

	for (i=0 ; i<256 ; i++)
		d_lightstyle[i] = 264;		// normal light value

	memset (&r_worldentity, 0, sizeof(r_worldentity));
	r_worldentity.model = cl.worldmodel;

// clear out efrags in case the level hasn't been reloaded
// FIXME: is this one short?
	for (i=0 ; i<cl.worldmodel->numleafs ; i++)
		cl.worldmodel->leafs[i].efrags = NULL;

	r_viewleaf = NULL;
	
	R_ClearParticles ();

	R_BuildLightmaps ();

	R_ParseWorldspawn ();
}


/*
====================
R_TimeRefresh_f

For program optimization
====================
*/
void R_TimeRefresh_f (void)
{
	int			i;
	float		start, stop, time;

	if (cls.state != ca_connected)
	{
		Con_Printf ("Not connected to a server\n");
		return;
	}

	start = Sys_DoubleTime ();
	for (i=0 ; i<128 ; i++)
	{
		GL_BeginRendering (&glx, &gly, &glwidth, &glheight);

		r_refdef.viewangles[1] = i/128.0*360.0;
		R_RenderView ();
//		glFinish ();

// workaround to avoid flickering uncovered by 3d refresh 2d areas when bloom enabled
		GL_Set2D ();  
		if (scr_sbar.value || scr_viewsize.value < 100)
		{
			SCR_TileClear ();
			Sbar_Changed ();
			Sbar_Draw ();
		}

		GL_EndRendering ();
	}

	stop = Sys_DoubleTime ();
	time = stop-start;
	Con_Printf ("%f seconds (%f fps)\n", time, 128/time);
}

