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
R_Ambient
====================
*/
void R_Ambient (void)
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
	float m;
	
	d_overbright = CLAMP(0.0, gl_overbright.value, 2.0);
	m = d_overbright > 0 ? d_overbright : 0.5f;
	d_overbrightscale = OVERBRIGHT_SCALE * m;
	
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

	Cvar_Register (&r_norefresh);
	Cvar_RegisterCallback (&r_fullbright, R_FullBright);
	Cvar_RegisterCallback (&r_ambient, R_Ambient);
	Cvar_Register (&r_drawentities);
	Cvar_Register (&r_drawworld);
	Cvar_Register (&r_drawviewmodel);
	Cvar_Register (&r_flatturb);
	Cvar_Register (&r_waterquality);
	Cvar_Register (&r_wateralpha);
	Cvar_Register (&r_lockalpha);
	Cvar_Register (&r_lavaalpha);
	Cvar_Register (&r_slimealpha);
	Cvar_Register (&r_teleportalpha);
	Cvar_Register (&r_litwater);
	Cvar_Register (&r_noalphasort);
	Cvar_Register (&r_dynamic);
	Cvar_Register (&r_dynamicscale);
	Cvar_Register (&r_novis);
	Cvar_Register (&r_lockfrustum);
	Cvar_Register (&r_lockpvs);
	Cvar_Register (&r_speeds);
	Cvar_Register (&r_waterwarp);
	Cvar_RegisterCallback (&r_clearcolor, R_ClearColor);
	Cvar_RegisterCallback (&r_fastsky, R_FastSkyColor);
	Cvar_RegisterCallback (&r_fastskycolor, R_FastSkyColor);
	Cvar_Register (&r_skyquality);
	Cvar_Register (&r_skyalpha);
	Cvar_Register (&r_skyfog);
	Cvar_Register (&r_oldsky);
	Cvar_Register (&r_flatworld);
	Cvar_Register (&r_flatmodels);

	Cvar_Register (&gl_finish);
	Cvar_Register (&gl_clear);
	Cvar_Register (&gl_cull);
	Cvar_Register (&gl_farclip);
	Cvar_Register (&gl_smoothmodels);
	Cvar_Register (&gl_affinemodels);
	Cvar_Register (&gl_gammablend);
	Cvar_Register (&gl_polyblend);
	Cvar_Register (&gl_flashblend);
	Cvar_Register (&gl_flashblendview);
	Cvar_Register (&gl_flashblendscale);
	Cvar_RegisterCallback (&gl_overbright, GL_Overbright);
	Cvar_Register (&gl_oldspr);
	Cvar_Register (&gl_nocolors);

	// Nehahra
	Cvar_Register (&gl_fogenable);
	Cvar_Register (&gl_fogdensity);
	Cvar_Register (&gl_fogred);
	Cvar_Register (&gl_foggreen);
	Cvar_Register (&gl_fogblue);

	Cvar_Register (&r_bloom);
	Cvar_Register (&r_bloom_darken);
	Cvar_Register (&r_bloom_alpha);
	Cvar_Register (&r_bloom_intensity);
	Cvar_Register (&r_bloom_diamond_size);
	Cvar_RegisterCallback (&r_bloom_sample_size, R_InitBloomTextures); // NULL
	Cvar_Register (&r_bloom_fast_sample);

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
void R_InitTranslatePlayerTextures (void)
{
	int i;

	for (i = 0; i < MAX_SCOREBOARD; i++)
	{
		playertextures[i] = NULL; //clear playertexture pointers
	}
}

/*
===============
R_TranslatePlayerSkin

Translates a skin texture by the per-player color lookup
-- johnfitz -- rewritten.  also, only handles new colors, not new skins
===============
*/
void R_TranslatePlayerSkin (int playernum)
{
	int		top, bottom;
	
	top = (cl.scores[playernum].colors & 0xf0)>>4;
	bottom = cl.scores[playernum].colors &15;
	
	if (playertextures[playernum])
		TexMgr_ReloadTextureTranslation (playertextures[playernum], top, bottom);
}

/*
===============
R_TranslateNewPlayerSkin
 
-- johnfitz -- split off of TranslatePlayerSkin --
this is called when the skin or model actually changes, instead of just new colors
added bug fix from Bengt Jardrup
===============
*/
void R_TranslateNewPlayerSkin (int playernum)
{
	entity_t	*e;
	model_t	*model;
	aliashdr_t	*paliashdr;
	int		skinnum;
	byte	*pixels;
	char	name[64];

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


	skinnum = e->skinnum;
	if (skinnum >= paliashdr->numskins || skinnum < 0)
	{
		Con_DWarning ("R_TranslateNewPlayerSkin: (%d): Invalid player skin # %d (%d skins) in '%s'\n", playernum, skinnum, paliashdr->numskins, model->name);
		skinnum = 0;
	}

// get correct texture pixels
	pixels = (byte *)paliashdr + paliashdr->texels[skinnum]; // This is not a persistent place!

// upload new image
	sprintf (name, "%s_%i_%i", e->model->name, skinnum, playernum);
	playertextures[playernum] = TexMgr_LoadTexture (e->model, name, paliashdr->skinwidth, paliashdr->skinheight, SRC_INDEXED, pixels,
												paliashdr->base[skinnum][0]->source_file,
												paliashdr->base[skinnum][0]->source_offset, TEXPREF_PAD | TEXPREF_OVERWRITE);

// now recolor it
	R_TranslatePlayerSkin (playernum);
}


/*
=================
R_InitMapGlobals

called when quake initializes
=================
*/
void R_InitMapGlobals (void)
{
	int i;

	// clear skyboxtextures pointers
	for (i=0; i<6; i++)
		skyboxtextures[i] = NULL;

	// set up global fog
	glFogi(GL_FOG_MODE, GL_EXP2);
	glHint(GL_FOG_HINT, GL_NICEST); /*  per pixel  */
}

float globalwateralpha = 0.0;

/*
=================
R_ParseWorldspawn

called at map load
=================
*/
void R_ParseWorldspawn (void)
{
	char  key[MAX_KEY], value[MAX_VALUE];
	char  *data;
	int i;

	// initially no skybox
	oldsky = true;
	strcpy (skybox_name, "");
	for (i=0; i<6; i++)
		skyboxtextures[i] = NULL;

	// initially no fog enabled
	Cvar_SetValue ("gl_fogenable", 0);

	// initially no wateralpha
	globalwateralpha = 0.0;

	data = cl.worldmodel->entities;
	if (!data)
		return;

	data = COM_Parse(data);
	if (!data) // should never happen
		return; // error

	if (com_token[0] != '{') // should never happen
		return; // error

	while (1)
	{
		data = COM_Parse(data);
		if (!data)
			return; // error

		if (com_token[0] == '}')
			break; // end of worldspawn

		if (com_token[0] == '_')
			strcpy(key, com_token + 1); // Support "_sky" and "_fog" also
		else
			strcpy(key, com_token);

		while (key[strlen(key)-1] == ' ') // remove trailing spaces
			key[strlen(key)-1] = 0;

		data = COM_Parse(data);
		if (!data)
			return; // error

		strcpy(value, com_token);

		if (!strcmp("sky", key) && value[0])
			R_LoadSkyBox(value);
		// also accept non-standard keys
		else if (!strcmp("skyname", key) && value[0]) // half-life
			R_LoadSkyBox(value);
		else if (!strcmp("qlsky", key) && value[0]) // quake lives
			R_LoadSkyBox(value);
		else if (!strcmp("fog", key) && value[0])
		{
			float density, red, green, blue;

			sscanf(value, "%f %f %f %f", &density, &red, &green, &blue);

			R_FogUpdate (density, red, green, blue, 0.0);
		}
		else if (!strcmp("wateralpha", key) && value[0])
		{
			globalwateralpha = atof (value);
		}
		else if (!strcmp("mapversion", key) && value[0])
		{
			Con_DPrintf("mapversion is %i\n", atoi(value));
		}
	}
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

