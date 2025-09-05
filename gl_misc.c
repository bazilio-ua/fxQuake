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
====================
R_WaterAlpha -- ericw
====================
*/
void R_WaterAlpha (void)
{
	map_wateralpha = CLAMP(0.0, r_wateralpha.value, 1.0);
}

/*
====================
R_LavaAlpha -- ericw
====================
*/
void R_LavaAlpha (void)
{
	map_lavaalpha = CLAMP(0.0, r_lavaalpha.value, 1.0);
}

/*
====================
R_TeleAlpha -- ericw
====================
*/
void R_TeleAlpha (void)
{
	map_telealpha = CLAMP(0.0, r_telealpha.value, 1.0);
}

/*
====================
R_SlimeAlpha -- ericw
====================
*/
void R_SlimeAlpha (void)
{
	map_slimealpha = CLAMP(0.0, r_slimealpha.value, 1.0);
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
	Cvar_RegisterVariableCallback (&r_ambient, R_Ambient);
	Cvar_RegisterVariable (&r_drawentities);
	Cvar_RegisterVariable (&r_drawworld);
	Cvar_RegisterVariable (&r_drawviewmodel);
	Cvar_RegisterVariable (&r_flatturb);
	Cvar_RegisterVariable (&r_waterquality);
	Cvar_RegisterVariableCallback (&r_wateralpha, R_WaterAlpha);
	Cvar_RegisterVariable (&r_lockalpha);
	Cvar_RegisterVariableCallback (&r_lavaalpha, R_LavaAlpha);
	Cvar_RegisterVariableCallback (&r_slimealpha, R_SlimeAlpha);
	Cvar_RegisterVariableCallback (&r_telealpha, R_TeleAlpha);
	Cvar_RegisterVariable (&r_litwater);
	Cvar_RegisterVariable (&r_noalphasort);
	Cvar_RegisterVariable (&r_dynamic);
	Cvar_RegisterVariable (&r_dynamicscale);
	Cvar_RegisterVariable (&r_novis);
	Cvar_RegisterVariable (&r_lockfrustum);
	Cvar_RegisterVariable (&r_lockpvs);
	Cvar_RegisterVariable (&r_pos);
	Cvar_RegisterVariable (&r_speeds);
	Cvar_RegisterVariable (&r_waterwarp);
	Cvar_RegisterVariableCallback (&r_clearcolor, R_ClearColor);
	Cvar_RegisterVariableCallback (&r_fastsky, R_FastSkyColor);
	Cvar_RegisterVariableCallback (&r_fastskycolor, R_FastSkyColor);
	Cvar_RegisterVariable (&r_skyquality);
	Cvar_RegisterVariableCallback (&r_skyalpha, R_SkyAlpha);
	Cvar_RegisterVariableCallback (&r_skyfog, R_Skyfog);
	Cvar_RegisterVariable (&r_oldsky);
	Cvar_RegisterVariable (&r_flatworld);
	Cvar_RegisterVariable (&r_flatmodels);
	Cvar_RegisterVariable (&r_flatlightstyles);

	Cvar_RegisterVariable (&gl_finish);
	Cvar_RegisterVariable (&gl_clear);
	Cvar_RegisterVariable (&gl_cull);
	Cvar_RegisterVariable (&gl_farclip);
	Cvar_RegisterVariable (&gl_smoothmodels);
	Cvar_RegisterVariable (&gl_affinemodels);
	Cvar_RegisterVariable (&gl_gammablend);
	Cvar_RegisterVariable (&gl_polyblend);
	Cvar_RegisterVariable (&gl_flashblend);
	Cvar_RegisterVariable (&gl_flashblendview);
	Cvar_RegisterVariable (&gl_flashblendscale);
	Cvar_RegisterVariableCallback (&gl_overbright, GL_Overbright);
	Cvar_RegisterVariable (&gl_oldspr);
	Cvar_RegisterVariable (&gl_nocolors);

	// Nehahra
	Cvar_RegisterVariable (&gl_fogenable);
	Cvar_RegisterVariable (&gl_fogdensity);
	Cvar_RegisterVariable (&gl_fogred);
	Cvar_RegisterVariable (&gl_foggreen);
	Cvar_RegisterVariable (&gl_fogblue);

	Cvar_RegisterVariable (&gl_bloom);
	Cvar_RegisterVariable (&gl_bloomdarken);
	Cvar_RegisterVariable (&gl_bloomalpha);
	Cvar_RegisterVariable (&gl_bloomintensity);
	Cvar_RegisterVariable (&gl_bloomdiamondsize);
	Cvar_RegisterVariableCallback (&gl_bloomsamplesize, R_InitBloomTextures); // NULL
	Cvar_RegisterVariable (&gl_bloomfastsample);

	R_InitParticles ();

	R_InitPlayerTextures ();

	R_InitSkyBoxTextures ();

	R_InitBloomTextures();
}

/*
===============
R_InitPlayerTextures

johnfitz -- handle a game switch
===============
*/
void R_InitPlayerTextures (void)
{
	int i;
	
	// clear playertexture pointers (the textures themselves were freed by texmgr_newgame)
	for (i = 0; i < MAX_SCOREBOARD; i++)
		playertextures[i] = NULL;
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
R_InitSkyBoxTextures
=================
*/
void R_InitSkyBoxTextures (void)
{
	int i;

	skybox_name[0] = 0;
	// clear skyboxtextures pointers
	for (i=0; i<6; i++)
		skyboxtextures[i] = NULL;
}

float	map_wateralpha, map_lavaalpha, map_telealpha, map_slimealpha;

/*
=================
R_ParseWorldspawn

called at map load
=================
*/
void R_ParseWorldspawn (void)
{
	char	key[128], value[4096];
	char	*data;
	int i;

	// initially no skybox
	skyfog = r_skyfog.value;
	skyalpha = r_skyalpha.value;
	oldsky = true;
	strcpy (skybox_name, "");
	for (i=0; i<6; i++)
		skyboxtextures[i] = NULL;

	// initially no fog
	R_FogReset ();

	// initially no wateralpha
	map_wateralpha = r_wateralpha.value;
	map_lavaalpha = r_lavaalpha.value;
	map_telealpha = r_telealpha.value;
	map_slimealpha = r_slimealpha.value;

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
		else if (!strcmp("skyalpha", key) && value[0])
			skyalpha = atof(value);
		else if (!strcmp("skyfog", key) && value[0])
			skyfog = atof(value);
		else if (!strcmp("fog", key) && value[0])
		{
			float density, red, green, blue;

			sscanf(value, "%f %f %f %f", &density, &red, &green, &blue);

			R_FogUpdate (density, red, green, blue, 0.0);
		}
		else if (!strcmp("wateralpha", key) && value[0])
			map_wateralpha = atof(value);
		else if (!strcmp("lavaalpha", key) && value[0])
			map_lavaalpha = atof(value);
		else if (!strcmp("telealpha", key) && value[0])
			map_telealpha = atof(value);
		else if (!strcmp("slimealpha", key) && value[0])
			map_slimealpha = atof(value);
		else if (!strcmp("mapversion", key) && value[0])
			Con_DPrintf("mapversion is %i\n", atoi(value));
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

