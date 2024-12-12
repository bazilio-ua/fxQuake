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
// gl_main.c

#include "quakedef.h"

entity_t	r_worldentity;

vec3_t		modelorg, r_entorigin;

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

mplane_t	frustum[4];

int			rs_c_brush_polys, rs_c_brush_passes, rs_c_alias_polys, rs_c_alias_passes, rs_c_sky_polys, rs_c_sky_passes;
int			rs_c_dynamic_lightmaps, rs_c_particles;

//up to 16 color translated skins
gltexture_t *playertextures[MAX_SCOREBOARD]; // changed to an array of pointers

//
// view origin
//
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;
vec3_t	r_origin;

//
// screen size info
//
refdef_t	r_refdef;

mleaf_t		*r_viewleaf, *r_oldviewleaf;

int		d_lightstyle[256];	// 8.8 fraction of base light value

float r_fovx, r_fovy;

// interpolation
void R_SetupAliasFrame (entity_t *e, aliashdr_t *paliashdr, lerpdata_t *lerpdata);
void R_SetupEntityTransform (entity_t *e, lerpdata_t *lerpdata);
void GL_DrawAliasFrame (aliashdr_t *paliashdr, lerpdata_t lerpdata);
void GL_EntityTransform (lerpdata_t lerpdata);


cvar_t	r_norefresh = {"r_norefresh","0", CVAR_NONE};
cvar_t	r_drawentities = {"r_drawentities","1", CVAR_NONE};
cvar_t	r_drawworld = {"r_drawworld","1", CVAR_NONE};
cvar_t	r_drawviewmodel = {"r_drawviewmodel","1", CVAR_NONE};
cvar_t	r_speeds = {"r_speeds","0", CVAR_NONE};
cvar_t	r_fullbright = {"r_fullbright","0", CVAR_NONE};
cvar_t	r_wateralpha = {"r_wateralpha","1", CVAR_ARCHIVE};
cvar_t	r_lockalpha = {"r_lockalpha","0", CVAR_ARCHIVE};
cvar_t	r_lavafog = {"r_lavafog","0.5", CVAR_ARCHIVE};
cvar_t	r_slimefog = {"r_slimefog","0.8", CVAR_ARCHIVE};
cvar_t	r_lavaalpha = {"r_lavaalpha","1", CVAR_ARCHIVE};
cvar_t	r_slimealpha = {"r_slimealpha","1", CVAR_ARCHIVE};
cvar_t	r_teleportalpha = {"r_teleportalpha","1", CVAR_ARCHIVE};
cvar_t	r_dynamic = {"r_dynamic","1", CVAR_ARCHIVE};
cvar_t	r_dynamicscale = {"r_dynamicscale","1", CVAR_ARCHIVE};
cvar_t	r_novis = {"r_novis","0", CVAR_NONE};
cvar_t	r_lockfrustum =	{"r_lockfrustum","0", CVAR_NONE};
cvar_t	r_lockpvs = {"r_lockpvs","0", CVAR_NONE};
cvar_t	r_waterwarp = {"r_waterwarp", "1", CVAR_ARCHIVE};
cvar_t	r_clearcolor = {"r_clearcolor", "2", CVAR_ARCHIVE}; // Closest to the original

cvar_t	gl_finish = {"gl_finish","0", CVAR_NONE};
cvar_t	gl_clear = {"gl_clear","0", CVAR_NONE};
cvar_t	gl_cull = {"gl_cull","1", CVAR_NONE};
cvar_t	gl_farclip = {"gl_farclip","16384", CVAR_ARCHIVE};
cvar_t	gl_smoothmodels = {"gl_smoothmodels","1", CVAR_NONE};
cvar_t	gl_affinemodels = {"gl_affinemodels","0", CVAR_NONE};
cvar_t	gl_gammablend = {"gl_gammablend","1", CVAR_ARCHIVE};
cvar_t	gl_polyblend = {"gl_polyblend","1", CVAR_ARCHIVE};
cvar_t	gl_flashblend = {"gl_flashblend","1", CVAR_ARCHIVE};
cvar_t	gl_flashblendview = {"gl_flashblendview","1", CVAR_ARCHIVE};
cvar_t	gl_flashblendscale = {"gl_flashblendscale","1", CVAR_ARCHIVE};
cvar_t	gl_overbright = {"gl_overbright", "1", CVAR_ARCHIVE};
cvar_t	gl_oldspr = {"gl_oldspr", "0", CVAR_NONE}; // Old opaque sprite
cvar_t	gl_nocolors = {"gl_nocolors","0", CVAR_NONE};


/*
==================
BoxOnPlaneSide

Returns 1, 2, or 1 + 2
==================
*/
int BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, mplane_t *p)
{
	float	dist1, dist2;
	int		sides;
    
	// this is done by the BOX_ON_PLANE_SIDE macro before calling this function
/*    
    // fast axial cases
	if (p->type < 3)
	{
		if (p->dist <= emins[p->type])
			return 1;
		if (p->dist >= emaxs[p->type])
			return 2;
		return 3;
	}
*/    
    // general case
	switch (p->signbits)
	{
        case 0:
            dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
            dist2 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
            break;
        case 1:
            dist1 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
            dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
            break;
        case 2:
            dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
            dist2 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
            break;
        case 3:
            dist1 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
            dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
            break;
        case 4:
            dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
            dist2 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
            break;
        case 5:
            dist1 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
            dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
            break;
        case 6:
            dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
            dist2 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
            break;
        case 7:
            dist1 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
            dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
            break;
        default:
            dist1 = dist2 = 0;		// make compiler happy
            Host_Error ("BoxOnPlaneSide: Bad signbits");
            break;
	}
    
	sides = 0;
	if (dist1 >= p->dist)
		sides = 1;
	if (dist2 < p->dist)
		sides |= 2;
    
	return sides;
}

/*-----------------------------------------------------------------*/

/*
=================
R_CullBox -- johnfitz -- replaced with new function from lordhavoc

Returns true if the box is completely outside the frustum
=================
*/
qboolean R_CullBox (vec3_t emins, vec3_t emaxs)
{
	int		i;
	mplane_t *p;
    
	for (i=0, p = frustum ; i<4 ; i++, p++)
    {
		switch(p->signbits)
		{
        default:
        case 0:
            if (p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2] < p->dist)
                return true;
            break;
        case 1:
            if (p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2] < p->dist)
                return true;
            break;
        case 2:
            if (p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2] < p->dist)
                return true;
            break;
        case 3:
            if (p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2] < p->dist)
                return true;
            break;
        case 4:
            if (p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2] < p->dist)
                return true;
            break;
        case 5:
            if (p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2] < p->dist)
                return true;
            break;
        case 6:
            if (p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2] < p->dist)
                return true;
            break;
        case 7:
            if (p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2] < p->dist)
                return true;
            break;
		}
    }
	return false;
}


/*
=================
R_CullSphere

Returns true if the sphere is completely outside the frustum
=================
*/
qboolean R_CullSphere (vec3_t origin, float radius)
{
	int		i;
	mplane_t *p;
	
	for (i=0, p = frustum ; i<4 ; i++, p++)
	{
		if (DotProduct (p->normal, origin) - p->dist <= -radius)
			return true;
	}
	return false;
}


/*
===============
R_CullModelForEntity

uses correct bounds based on rotation
===============
*/
qboolean R_CullModelForEntity (entity_t *e)
{
	vec3_t mins, maxs;

	if (e->angles[0] || e->angles[2]) // pitch or roll
	{
		VectorAdd (e->origin, e->model->rmins, mins);
		VectorAdd (e->origin, e->model->rmaxs, maxs);
	}
	else if (e->angles[1]) // yaw
	{
		VectorAdd (e->origin, e->model->ymins, mins);
		VectorAdd (e->origin, e->model->ymaxs, maxs);
	}
	else // no rotation
	{
		VectorAdd (e->origin, e->model->mins, mins);
		VectorAdd (e->origin, e->model->maxs, maxs);
	}

	return R_CullBox (mins, maxs);
}


/*
=============================================================

  SPRITE MODELS

=============================================================
*/

/*
================
R_GetSpriteFrame
================
*/
mspriteframe_t *R_GetSpriteFrame (entity_t *e)
{
	msprite_t		*psprite;
	mspritegroup_t	*pspritegroup;
	mspriteframe_t	*pspriteframe;
	int				i, numframes, frame;
	float			*pintervals, fullinterval, targettime, time;
	static float	lastmsg = 0;

	psprite = e->model->cache.data;
	frame = e->frame;

	if ((frame >= psprite->numframes) || (frame < 0))
	{
		if (IsTimeout (&lastmsg, 2))
		{
			Con_DWarning ("R_GetSpriteFrame: no such frame %d (%d frames) in %s\n", frame, psprite->numframes, e->model->name);
		}
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE)
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];

		time = cl.time + e->syncbase;

	// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
	// are positive, so we don't have to worry about division by 0
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		for (i=0 ; i<(numframes-1) ; i++)
		{
			if (pintervals[i] > targettime)
				break;
		}

		pspriteframe = pspritegroup->frames[i];
	}

	return pspriteframe;
}

/*
=================
R_DrawSpriteModel -- johnfitz -- rewritten: now supports all orientations
=================
*/
void R_DrawSpriteModel (entity_t *e)
{
	vec3_t			point, v_forward, v_right, v_up;
	msprite_t		*psprite;
	mspriteframe_t	*frame;
	float			*s_up, *s_right;
	float			angle, sr, cr;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	//TODO: frustum cull it?

	frame = R_GetSpriteFrame (e);
	psprite = e->model->cache.data;

	switch(psprite->type)
	{
	case SPR_VP_PARALLEL_UPRIGHT: //faces view plane, up is towards the heavens
		v_up[0] = 0;
		v_up[1] = 0;
		v_up[2] = 1;
		s_up = v_up;
		s_right = vright;
		break;
		
	case SPR_FACING_UPRIGHT: //faces camera origin, up is towards the heavens
		VectorSubtract(e->origin, r_origin, v_forward);
		v_forward[2] = 0;
		VectorNormalizeFast(v_forward);
		v_right[0] = v_forward[1];
		v_right[1] = -v_forward[0];
		v_right[2] = 0;
		v_up[0] = 0;
		v_up[1] = 0;
		v_up[2] = 1;
		s_up = v_up;
		s_right = v_right;
		break;
		
	case SPR_VP_PARALLEL: //faces view plane, up is towards the top of the screen
		s_up = vup;
		s_right = vright;
		break;
		
	case SPR_ORIENTED: //pitch yaw roll are independent of camera (bullet marks on walls)
		AngleVectors (e->angles, v_forward, v_right, v_up);
		s_up = v_up;
		s_right = v_right;
		break;
		
	case SPR_VP_PARALLEL_ORIENTED: //faces view plane, but obeys roll value
		angle = e->angles[ROLL] * M_PI_DIV_180;
		sr = sin(angle);
		cr = cos(angle);
		v_right[0] = vright[0] * cr + vup[0] * sr;
		v_right[1] = vright[1] * cr + vup[1] * sr;
		v_right[2] = vright[2] * cr + vup[2] * sr;
		v_up[0] = vright[0] * -sr + vup[0] * cr;
		v_up[1] = vright[1] * -sr + vup[1] * cr;
		v_up[2] = vright[2] * -sr + vup[2] * cr;
		s_up = v_up;
		s_right = v_right;
		break;
		
	default:
		return;
	}

//	GL_DisableMultitexture (); // selects TEXTURE0
	GL_SelectTMU0 ();
	GL_BindTexture (frame->gltexture);

	// offset decals
	if (psprite->type == SPR_ORIENTED)
	{
		glPolygonOffset (-1, -10000);
		glEnable (GL_POLYGON_OFFSET_LINE);
		glEnable (GL_POLYGON_OFFSET_FILL);
	}

	glColor3f (1,1,1);

	if (gl_oldspr.value)
		glEnable (GL_ALPHA_TEST);
	else
	{
		if (psprite->type == SPR_ORIENTED)
			glDepthMask (GL_FALSE); // don't bother writing Z
		glEnable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable (GL_ALPHA_TEST);
		glAlphaFunc (GL_GEQUAL, 0.5);
	}

	glBegin (GL_QUADS);

	glTexCoord2f (0, 1);
	VectorMA (e->origin, frame->down, s_up, point);
	VectorMA (point, frame->left, s_right, point);
	glVertex3fv (point);

	glTexCoord2f (0, 0);
	VectorMA (e->origin, frame->up, s_up, point);
	VectorMA (point, frame->left, s_right, point);
	glVertex3fv (point);

	glTexCoord2f (1, 0);
	VectorMA (e->origin, frame->up, s_up, point);
	VectorMA (point, frame->right, s_right, point);
	glVertex3fv (point);

	glTexCoord2f (1, 1);
	VectorMA (e->origin, frame->down, s_up, point);
	VectorMA (point, frame->right, s_right, point);
	glVertex3fv (point);

	glEnd ();

	if (gl_oldspr.value)
		glDisable (GL_ALPHA_TEST);
	else
	{
		if (psprite->type == SPR_ORIENTED)
			glDepthMask (GL_TRUE); // back to normal Z buffering
		glDisable (GL_BLEND);
		glDisable (GL_ALPHA_TEST);
		glAlphaFunc (GL_GREATER, 0.666);
	}

	// offset decals
	if (psprite->type == SPR_ORIENTED)
	{
		glPolygonOffset (0, 0);
		glDisable (GL_POLYGON_OFFSET_LINE);
		glDisable (GL_POLYGON_OFFSET_FILL);
	}
}

/*
=============================================================

  ALIAS MODELS

=============================================================
*/


#define NUMVERTEXNORMALS	162
float	r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

extern vec3_t	lightcolor; // replaces "float shadelight" for lit support

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT		16
float	r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anorm_dots.h"
;

float	*shadedots = r_avertexnormal_dots[0];

qboolean shading = true; // if false, disable vertex shading for various reasons (fullbright, etc)

float	aliasalpha;
qboolean	aliasglow;


/*
=================
R_DrawAliasModel
=================
*/
void R_DrawAliasModel (entity_t *e)
{
	int			lnum;
	dlight_t	*l;
	vec3_t		dist;
	float		add;
	float		dscale;
	model_t		*clmodel;
	aliashdr_t	*paliashdr;
	int			anim;
	qboolean	isclient = false;
	int			skinnum, client_no;
	gltexture_t	*tx, *fb;
	static float	lastmsg = 0;
	lerpdata_t	lerpdata;
	float		scale;
	qboolean	alphatest;
	qboolean	alphablend;
	
	//
	// locate the proper data
	//
	paliashdr = (aliashdr_t *)Mod_Extradata (e->model);

	//
	// setup pose/lerp data -- do it first so we don't miss updates due to culling
	//
	R_SetupAliasFrame (e, paliashdr, &lerpdata);
	R_SetupEntityTransform (e, &lerpdata);

	//
	// cull it
	//
	if (R_CullModelForEntity(e))
		return;

	VectorCopy (e->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	clmodel = e->model;
	skinnum = e->skinnum;
	client_no = e - cl_entities;

	// r_speeds
	rs_c_alias_polys += paliashdr->numtris;

	// check skin bounds
	if (skinnum >= paliashdr->numskins || skinnum < 0)
	{
		if (IsTimeout (&lastmsg, 2))
		{
			Con_DWarning ("R_DrawAliasModel: no such skin # %d (%d skins) in '%s'\n", skinnum, paliashdr->numskins, clmodel->name);
		}
		skinnum = 0; // ericw -- display skin 0 for winquake compatibility
	}

	//
	// transform it
	//
	glPushMatrix ();

	GL_EntityTransform (lerpdata); // FX

	// special handling of view model to keep FOV from altering look.
	if (e == &cl.viewent)
		scale = 1.0f / tan( DEG2RAD (r_fovx / 2.0f) ) * r_refdef.weaponfov_x / 90.0f; // reverse out fov and do fov we want
	else
		scale = 1.0f;

	glTranslatef (paliashdr->scale_origin[0] * scale, paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
	glScalef (paliashdr->scale[0] * scale, paliashdr->scale[1], paliashdr->scale[2]);

	//
	// model rendering stuff
	//
	if (gl_smoothmodels.value)
		glShadeModel (GL_SMOOTH);
	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	shading = true;

	//
	// set up for alpha blending
	//
	aliasalpha = ENTALPHA_DECODE(e->alpha);

//	aliasalpha = 0.5f; // test
	
    alphatest = !!(e->model->flags & MF_HOLEY);

	if (aliasalpha == 0)
		goto cleanup;

	alphablend = (aliasalpha < 1.0);
	if (alphablend)
	{
		glDepthMask (GL_FALSE);
		glEnable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
	if (alphatest)
		glEnable (GL_ALPHA_TEST);
	
	
//	if (aliasalpha < 1.0)
//	{
//		glDepthMask (GL_FALSE);
//		glEnable (GL_BLEND);
//	} else if (alphatest)
//		glEnable (GL_ALPHA_TEST);

	//
	// set up lighting
	//
	R_LightPoint (e->origin, lightcolor);

	// add dlights
	if (r_dynamic.value) // EER1
	{
		dscale = CLAMP(1.0, r_dynamicscale.value, 32.0);
		
		for (lnum=0, l = cl_dlights ; lnum<MAX_DLIGHTS ; lnum++, l++)
		{
			if (l->die < cl.time || !l->radius)
				continue;
			
			if (R_CullSphere (l->origin, l->radius))
				continue;
			
			VectorSubtract (e->origin, l->origin, dist);
			add = l->radius - VectorLength(dist);
			
			if (add > 0)
				VectorMA (lightcolor, add * dscale, l->color, lightcolor);
		}
	}

	// minimum light value on gun (24)
	if (e == &cl.viewent)
	{
		add = 72.0f - (lightcolor[0] + lightcolor[1] + lightcolor[2]);
		if (add > 0.0f)
		{
			lightcolor[0] += add / 3.0f;
			lightcolor[1] += add / 3.0f;
			lightcolor[2] += add / 3.0f;
		}
	}

	if (client_no >= 1 && client_no <= cl.maxclients)
	{
		isclient = true;

		// minimum light value on players (8)
		add = 24.0f - (lightcolor[0] + lightcolor[1] + lightcolor[2]);
		if (add > 0.0f)
		{
			lightcolor[0] += add / 3.0f;
			lightcolor[1] += add / 3.0f;
			lightcolor[2] += add / 3.0f;
		}
	}

	// clamp lighting so it doesn't overbright as much (96)
	add = 288.0f / (lightcolor[0] + lightcolor[1] + lightcolor[2]);
	if (add < 1.0f)
		VectorScale (lightcolor, add, lightcolor);

	shadedots = r_avertexnormal_dots[((int)(e->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];
	VectorScale (lightcolor, 1.0f / 200.0f, lightcolor);

	//
	// set up textures
	//
	anim = (int)(cl.time*10) & 3;
	tx = paliashdr->gltexture[skinnum][anim];
	fb = paliashdr->fullbright[skinnum][anim];

	aliasglow = (fb != NULL);
	
	// we can't dynamically colormap textures, so they are cached
	// seperately for the players.  Heads are just uncolored.
	if (e->colormap != vid.colormap && !gl_nocolors.value)
	{
		if (isclient)
			tx = playertextures[client_no - 1];
	}

	//
	// draw it
	//
	
	GL_SelectTMU0 ();
	GL_BindTexture (tx);
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
	glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE);
	glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
	glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PRIMARY_COLOR_EXT);
	glTexEnvf (GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, d_overbrightscale);
	
	if (aliasglow)
	{
		GL_SelectTMU1 ();
		GL_BindTexture (fb);
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
		glEnable (GL_BLEND);
	}
	
	GL_DrawAliasFrame (paliashdr, lerpdata); // FX
	
	if (aliasglow)
	{
		glDisable (GL_BLEND);
		GL_SelectTMU0 ();
	}
	
	glTexEnvf (GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 1.0f);
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	
	
/*
	if (gl_mtexable && gl_texture_env_combine && gl_texture_env_add && fb) // case 1: everything in one pass
	{
		// Binds normal skin to texture env 0
		GL_DisableMultitexture (); // selects TEXTURE0
		GL_BindTexture (tx);
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
		glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE);
		glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
		glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PRIMARY_COLOR_EXT);
		glTexEnvf (GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, d_overbrightscale);

		// Binds fullbright skin to texture env 1
		GL_EnableMultitexture (); // selects TEXTURE1
		GL_BindTexture (fb);
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
		glEnable (GL_BLEND);
		GL_DrawAliasFrame (paliashdr, lerpdata); // FX
		glDisable (GL_BLEND);
		GL_DisableMultitexture (); // selects TEXTURE0
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	}
	else if (gl_texture_env_combine) // case 2: overbright in one pass, then fullbright pass
	{
		// first pass
		GL_BindTexture (tx);
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
		glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE);
		glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
		glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PRIMARY_COLOR_EXT);
		glTexEnvf (GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, d_overbrightscale);
		GL_DrawAliasFrame (paliashdr, lerpdata); // FX
		glTexEnvf (GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 1.0f);
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

		// second pass
		if (fb)
		{
			GL_BindTexture (fb);
			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glEnable (GL_BLEND);
			glBlendFunc (GL_ONE, GL_ONE);
			glDepthMask (GL_FALSE);
			shading = false;
			glColor3f (aliasalpha, aliasalpha, aliasalpha);
			R_FogStartAdditive ();
			GL_DrawAliasFrame (paliashdr, lerpdata); // FX
			R_FogStopAdditive ();
			glDepthMask (GL_TRUE);
			glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDisable (GL_BLEND);
			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		}
	}
	else // case 3: overbright in two passes, then fullbright pass
	{
		// first pass
		GL_BindTexture (tx);
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		GL_DrawAliasFrame (paliashdr, lerpdata); // FX

		// second pass -- additive with black fog, to double the object colors but not the fog color
		glEnable (GL_BLEND);
		glBlendFunc (GL_ONE, GL_ONE);
		glDepthMask (GL_FALSE);
		R_FogStartAdditive ();
		GL_DrawAliasFrame (paliashdr, lerpdata); // FX
		R_FogStopAdditive ();
		glDepthMask (GL_TRUE);
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable (GL_BLEND);

		// third pass
		if (fb)
		{
			GL_BindTexture (fb);
			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glEnable (GL_BLEND);
			glBlendFunc (GL_ONE, GL_ONE);
			glDepthMask (GL_FALSE);
			shading = false;
			glColor3f (aliasalpha, aliasalpha, aliasalpha);
			R_FogStartAdditive ();
			GL_DrawAliasFrame (paliashdr, lerpdata); // FX
			R_FogStopAdditive ();
			glDepthMask (GL_TRUE);
			glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDisable (GL_BLEND);
			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		}
	}
*/
 
 
cleanup:
//	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST); // gl_affinemodels
	glShadeModel (GL_FLAT); // gl_smoothmodels
	
	
	if (alphablend)
	{
		glDepthMask (GL_TRUE);
		glDisable (GL_BLEND);
		glColor4f (1, 1, 1, 1);
	}
	else
	if (alphatest)
		glDisable (GL_ALPHA_TEST);
	
	
//	glDepthMask (GL_TRUE);
//	glDisable (GL_BLEND);
//	if (alphatest)
//		glDisable (GL_ALPHA_TEST);
//	glColor3f (1, 1, 1);
	
	glPopMatrix ();
}

//==================================================================================

/*
=============
R_DrawEntities
=============
*/
void R_DrawEntities (void)
{
	int		i;
	entity_t	*e;

	if (!r_drawentities.value)
		return;

	// draw standard entities
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		if ((i + 1) % 100 == 0)
			S_ExtraUpdateTime (); // don't let sound get messed up if going slow

		e = cl_visedicts[i];

		// chase_active
		if (e == &cl_entities[cl.viewentity])
			e->angles[0] *= 0.3;

		switch (e->model->type)
		{
		case mod_brush:
			R_DrawBrushModel (e);
			break;
			
		case mod_alias:
			if (ENTALPHA_DECODE(e->alpha) < 1)
				R_AddToAlpha (ALPHA_ALIAS, R_GetAlphaDist(e->origin), e, NULL, 0);
			else	
				R_DrawAliasModel (e);
			break;
			
		case mod_sprite:
			R_AddToAlpha (ALPHA_SPRITE, R_GetAlphaDist(e->origin), e, NULL, 0);
			break;
			
		default:
			break;
		}
	}
}


//==================================================================================

/*
=============
R_DrawViewModel
=============
*/
void R_DrawViewModel (void)
{
	entity_t	*e;

	if (!r_drawviewmodel.value || chase_active.value)
		return;

	if (cl.items & IT_INVISIBILITY || cl.stats[STAT_HEALTH] <= 0)
		return;

	e = &cl.viewent;

	if (!e->model)
		return;

	// this fixes a crash
	if (e->model->type != mod_alias)
		return;

	// Prevent weapon model error
	if (e->model->name[0] == '*')
	{
		Con_Warning ("R_DrawViewModel: viewmodel %s invalid\n", e->model->name);
		Cvar_Set ("r_drawviewmodel", "0");
		return;
	}

	// hack the depth range to prevent view model from poking into walls
	glDepthRange (0, 0.3);

	R_DrawAliasModel (e);

	glDepthRange (0, 1);
}


/*
============
R_PolyBlend
============
*/
void R_PolyBlend (void)
{
	float gamma = CLAMP(0.0, gl_gammablend.value, 1.0);
	
//	if (!gl_polyblend.value)
//		return;
//
//	if (!v_blend[3])
//		return;
	
	if ((gl_polyblend.value && v_blend[3]) || gamma < 1.0)
	{
//		GL_DisableMultitexture (); // selects TEXTURE0
		GL_SelectTMU0 ();
		
//		glDisable (GL_ALPHA_TEST); //FX don't disable perform alpha test here, because bloom later
		glDisable (GL_TEXTURE_2D);
		glDisable (GL_DEPTH_TEST);
		glEnable (GL_BLEND);
		
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity ();
		
		glOrtho (0, 1, 1, 0, -99999, 99999);
		
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity ();
		
		if (gl_polyblend.value && v_blend[3]) {
			glColor4fv (v_blend);
			
			glBegin (GL_QUADS);
			glVertex2f (0, 0);
			glVertex2f (1, 0);
			glVertex2f (1, 1);
			glVertex2f (0, 1);
			glEnd ();
		}
		
		if (gamma < 1.0) {
			glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
			glColor4f(1, 1, 1, gamma);
			
			glBegin (GL_QUADS);
			glVertex2f (0, 0);
			glVertex2f (1, 0);
			glVertex2f (1, 1);
			glVertex2f (0, 1);
			glEnd ();
			
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
		
//		glColor4fv (v_blend);
//		
//		glBegin (GL_QUADS);
//		glVertex2f (0, 0);
//		glVertex2f (1, 0);
//		glVertex2f (1, 1);
//		glVertex2f (0, 1);
//		glEnd ();
		
		glDisable (GL_BLEND);
		glEnable (GL_DEPTH_TEST);
		glEnable (GL_TEXTURE_2D);
//		glEnable (GL_ALPHA_TEST); //FX
	}
}


//==================================================================================

/*
===============
SignbitsForPlane
===============
*/
static inline int SignbitsForPlane (mplane_t *out)
{
	int	bits, j;

	// for fast box on planeside test - identify which corner(s) of the box to text against the plane

	bits = 0;
	for (j=0 ; j<3 ; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1<<j;
	}
	return bits;
}

/*
===============
R_SetFrustum
===============
*/
void R_SetFrustum (float fovx, float fovy)
{
	int		i;

	if (r_lockfrustum.value)
		return;		// Do not update!

	TurnVector(frustum[0].normal, vpn, vright, fovx/2 - 90); //left plane
	TurnVector(frustum[1].normal, vpn, vright, 90 - fovx/2); //right plane
	TurnVector(frustum[2].normal, vpn, vup, 90 - fovy/2); //bottom plane
	TurnVector(frustum[3].normal, vpn, vup, fovy/2 - 90); //top plane

	for (i=0 ; i<4 ; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal); //FIXME: shouldn't this always be zero?
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}

/*
=============
R_Clear
 
johnfitz -- rewritten and gutted
=============
*/
void R_Clear (void)
{
    unsigned int clearbits;
    
    clearbits = GL_DEPTH_BUFFER_BIT;
	// from mh -- if we get a stencil buffer, we should clear it, even though we don't use it
	if (gl_stencilbits)
		clearbits |= GL_STENCIL_BUFFER_BIT;
	if (gl_clear.value || isIntel) // intel video workaround
		clearbits |= GL_COLOR_BUFFER_BIT;
    
	glClear (clearbits);
}

/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame (void)
{
// don't allow cheats in multiplayer
	if (cl.maxclients > 1)
	{
		Cvar_Set ("r_drawentities", "1");
		Cvar_Set ("r_drawworld", "1");
		Cvar_Set ("r_fullbright", "0");
	}

	R_PushDlights ();
	R_AnimateLight ();

	r_framecount++;

	R_FogSetupFrame ();

// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, r_origin);
	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

// current viewleaf
	r_oldviewleaf = r_viewleaf;

	if (!r_lockpvs.value)  // Don't update if PVS is locked
		r_viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);

	V_SetContentsColor (r_viewleaf->contents);
	V_CalcBlend ();

	// calculate r_fovx and r_fovy here
	r_fovx = r_refdef.fov_x;
	r_fovy = r_refdef.fov_y;
	if (r_waterwarp.value && (r_viewleaf->contents == CONTENTS_WATER || r_viewleaf->contents == CONTENTS_SLIME || r_viewleaf->contents == CONTENTS_LAVA))
	{
		r_fovx = r_refdef.fov_x + sin(cl.time);
		r_fovy = r_refdef.fov_y - cos(cl.time);
	}

	R_SetFrustum (r_fovx, r_fovy); // use r_fov* vars
}

/*
=============
GL_SetFrustum
=============
*/
void GL_SetFrustum (float fovx, float fovy)
{
	float xmin, xmax, ymin, ymax;

	xmax = NEARCLIP * tan( fovx * M_PI / 360.0 );
	ymax = NEARCLIP * tan( fovy * M_PI / 360.0 );

	xmin = -xmax;
	ymin = -ymax;

	glFrustum (xmin, xmax, ymin, ymax, NEARCLIP, gl_farclip.value); // FARCLIP
}

/*
=============
R_SetupGL
=============
*/
void R_SetupGL (void)
{
	//
	// set up viewpoint
	//
	glViewport (glx + r_refdef.vrect.x,
				gly + glheight - r_refdef.vrect.y - r_refdef.vrect.height,
				r_refdef.vrect.width,
				r_refdef.vrect.height);

	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();

	GL_SetFrustum (r_fovx, r_fovy);

//	glCullFace (GL_FRONT);
//	glCullFace (GL_BACK); // johnfitz -- glquake used CCW with backwards culling -- let's do it right

	glMatrixMode (GL_MODELVIEW);
	glLoadIdentity ();

	glRotatef (-90,  1, 0, 0);	    // put Z going up
	glRotatef (90,  0, 0, 1);	    // put Z going up
	glRotatef (-r_refdef.viewangles[2],  1, 0, 0);
	glRotatef (-r_refdef.viewangles[0],  0, 1, 0);
	glRotatef (-r_refdef.viewangles[1],  0, 0, 1);
	glTranslatef (-r_refdef.vieworg[0],  -r_refdef.vieworg[1],  -r_refdef.vieworg[2]);

	//
	// get world matrix
	//
//	glGetFloatv (GL_MODELVIEW_MATRIX, r_worldentity.matrix);

	//
	// set drawing parms
	//
	if (gl_cull.value)
		glEnable (GL_CULL_FACE);
	else
		glDisable (GL_CULL_FACE);

	glDisable (GL_BLEND);
	glDisable (GL_ALPHA_TEST);
	glEnable (GL_DEPTH_TEST);
}

/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void R_RenderView (void)
{
	float	time1 = 0, time2;
	float	ms;
	char str[256]; 

	if (r_norefresh.value)
		return;

	if (!r_worldentity.model || !cl.worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");

	if (r_speeds.value)
	{
//		glFinish ();
		time1 = Sys_DoubleTime ();
	}

	// clear rendering statistics (r_speed)
	rs_c_brush_polys = 
	rs_c_brush_passes = 
	rs_c_alias_polys = 
	rs_c_alias_passes = 
	rs_c_sky_polys = 
	rs_c_sky_passes = 
	rs_c_dynamic_lightmaps = 
	rs_c_particles = 0;

	if (gl_finish.value /* || r_speeds.value */)
		glFinish ();

	// render normal view
	// r_refdef must be set before the first call
	R_SetupFrame ();
	R_MarkLeaves ();	// done here so we know if we're in water
    
    R_SetupSurfaces (); // create texture chains from PVS and cull it
	R_UpdateWarpTextures ();	// do this before R_Clear
	R_Clear ();
	R_SetupGL ();

	S_ExtraUpdateTime ();	// don't let sound get messed up if going slow

	R_FogEnableGFog ();
	R_DrawSky (); // handle worldspawn and bmodels
	R_DrawWorld (); // adds static entities to the list
	R_DrawEntities ();
	
	R_SetupParticles ();
	R_SetupDlights (); // flash blend dlights
	
	R_DrawAlpha (); // handle surfaces, entities, particles, dlights
	
	R_DrawViewModel ();
	R_FogDisableGFog ();
	R_PolyBlend ();
	R_BloomBlend (); // bloom on each frame

	S_ExtraUpdateTime ();	// don't let sound get messed up if going slow

	if (r_speeds.value)
	{
//		glFinish ();
		time2 = Sys_DoubleTime ();
		ms = 1000 * (time2 - time1);

		if (r_speeds.value == 2)
			sprintf (str, "%5.1f ms - %4i/%4i wpoly * %4i/%4i epoly * %4i/%4i sky * %4i lmaps * %4i part\n", ms,
				rs_c_brush_polys,
				rs_c_brush_passes,
				rs_c_alias_polys,
				rs_c_alias_passes,
				rs_c_sky_polys,
				rs_c_sky_passes,
				rs_c_dynamic_lightmaps,
				rs_c_particles);
		else
			sprintf (str, "%5.1f ms - %4i wpoly * %4i epoly * %4i lmaps\n", ms, 
				rs_c_brush_polys, 
				rs_c_alias_polys, 
				rs_c_dynamic_lightmaps);

		Con_Printf (str);
	}
}

/*
===============
GL_EntityTransform -- model transform interpolation

R_RotateForEntity renamed and modified to take lerpdata instead of pointer to entity
===============
*/
void GL_EntityTransform (lerpdata_t lerpdata)
{
	glTranslatef (lerpdata.origin[0], lerpdata.origin[1], lerpdata.origin[2]);
	glRotatef (lerpdata.angles[1],  0, 0, 1);
	glRotatef (stupidquakebugfix ? lerpdata.angles[0] : -lerpdata.angles[0],  0, 1, 0);
	glRotatef (lerpdata.angles[2],  1, 0, 0);
}

/*
=============
GL_DrawAliasFrame -- model animation interpolation (lerping)

support colored light, lerping, alpha, multitexture
=============
*/
void GL_DrawAliasFrame (aliashdr_t *paliashdr, lerpdata_t lerpdata)
{
	float		vertcolor[4]; // replaces "float l" for lit support
	trivertx_t	*verts1, *verts2;
	int			*commands;
	int			count;
	float		u,v;
	float		blend, iblend;
	qboolean	lerping;

	if (lerpdata.pose1 != lerpdata.pose2)
	{
		lerping = true;
		verts1  = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
		verts2  = verts1;

		verts1 += lerpdata.pose1 * paliashdr->poseverts;
		verts2 += lerpdata.pose2 * paliashdr->poseverts;
		blend = lerpdata.blend;
		iblend = 1.0f - blend;
	}
	else // poses the same means that the entity has paused its animation
	{
		lerping = false;
		verts1  = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
		verts2  = verts1; // avoid bogus compiler warning
		
		verts1 += lerpdata.pose1 * paliashdr->poseverts;
		blend = iblend = 0; // avoid bogus compiler warning
	}

	commands = (int *)((byte *)paliashdr + paliashdr->commands);

	vertcolor[3] = aliasalpha; // never changes, so there's no need to put this inside the loop

	while (1)
	{
		// get the vertex count and primitive type
		count = *commands++;

		if (!count) 
			break; // done

		if (count < 0)
		{
			count = -count;
			glBegin (GL_TRIANGLE_FAN);
		}
		else
			glBegin (GL_TRIANGLE_STRIP);

		do
		{
			// texture coordinates come from the draw list
			u = ((float *)commands)[0];
			v = ((float *)commands)[1];

//			if (mtexenabled)
			{
				qglMultiTexCoord2f (GL_TEXTURE0_ARB, u, v);
				if (aliasglow)
					qglMultiTexCoord2f (GL_TEXTURE1_ARB, u, v);
			}
//			else
//			{
//				glTexCoord2f (u, v);
//			}

			commands += 2;

			// normals and vertexes come from the frame list
			// blend the light intensity from the two frames together
			if (shading)
			{
				// lit support
				if (r_fullbright.value || !cl.worldmodel->lightdata)
				{
					vertcolor[0] = vertcolor[1] = vertcolor[2] = 1.0;
				}
				else if (lerping)
				{
					vertcolor[0] = (shadedots[verts1->lightnormalindex]*iblend + shadedots[verts2->lightnormalindex]*blend) * lightcolor[0];
					vertcolor[1] = (shadedots[verts1->lightnormalindex]*iblend + shadedots[verts2->lightnormalindex]*blend) * lightcolor[1];
					vertcolor[2] = (shadedots[verts1->lightnormalindex]*iblend + shadedots[verts2->lightnormalindex]*blend) * lightcolor[2];
				}
				else
				{
					vertcolor[0] = shadedots[verts1->lightnormalindex] * lightcolor[0];
					vertcolor[1] = shadedots[verts1->lightnormalindex] * lightcolor[1];
					vertcolor[2] = shadedots[verts1->lightnormalindex] * lightcolor[2];
				}
				glColor4fv (vertcolor);
			}

			// blend the vertex positions from each frame together
			if (lerping)
			{
				glVertex3f (verts1->v[0]*iblend + verts2->v[0]*blend,
							verts1->v[1]*iblend + verts2->v[1]*blend,
							verts1->v[2]*iblend + verts2->v[2]*blend);
				verts1++;
				verts2++;
			}
			else
			{
				glVertex3f (verts1->v[0], verts1->v[1], verts1->v[2]);
				verts1++;
			}
		}
		while (--count);

		glEnd ();
	}

	// r_speeds
	rs_c_alias_passes += paliashdr->numtris;
}

/*
=================
R_SetupAliasFrame -- model animation interpolation (lerping)

set up animation part of lerpdata
=================
*/
void R_SetupAliasFrame (entity_t *e, aliashdr_t *paliashdr, lerpdata_t *lerpdata)
{
	int		frame = e->frame;
	int		posenum, numposes;
	static float	lastmsg = 0;

	if ((frame >= paliashdr->numframes) || (frame < 0))
	{
		if (IsTimeout (&lastmsg, 2))
		{
			Con_DWarning ("R_SetupAliasFrame: no such frame ");
			// Single frame?
			if (paliashdr->frames[0].name[0])
				Con_DPrintf ("%d ('%s', %d frames)", frame, paliashdr->frames[0].name, paliashdr->numframes);
			else
				Con_DPrintf ("group %d (%d groups)", frame, paliashdr->numframes);
			Con_DPrintf (" in %s\n", e->model->name);
		}
		frame = 0;
	}

	posenum = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1)
	{
		int firstpose = posenum;

		posenum = numposes * e->syncbase; // Hack to make flames unsynchronized
		e->lerptime = paliashdr->frames[frame].interval;
		posenum += (int)(cl.time / e->lerptime) % numposes;
		posenum = firstpose + posenum % numposes;
	}
	else
	{
		e->lerptime = 0.1; // One tenth of a second is a good for most Quake animations.
	}

	if (e->lerpflags & LERP_RESETANIM) // kill any lerp in progress
	{
		e->lerpstart = 0;
		e->previouspose = posenum;
		e->currentpose = posenum;
		e->lerpflags -= LERP_RESETANIM;
	}
	else if (e->currentpose != posenum)  // pose changed, start new lerp
	{
		if (e->lerpflags & LERP_RESETANIM2) // defer lerping one more time
		{
			e->lerpstart = 0;
			e->previouspose = posenum;
			e->currentpose = posenum;
			e->lerpflags -= LERP_RESETANIM2;
		}
		else
        {
            e->lerpstart = cl.time;
            e->previouspose = e->currentpose;
            e->currentpose = posenum;
        }
	}

	// set up values
	// always lerp
	{
		if (e->lerpflags & LERP_FINISH && numposes == 1)
			lerpdata->blend = CLAMP (0, (cl.time - e->lerpstart) / (e->lerpfinish - e->lerpstart), 1);
		else
			lerpdata->blend = CLAMP (0, (cl.time - e->lerpstart) / e->lerptime, 1);
		lerpdata->pose1 = e->previouspose;
		lerpdata->pose2 = e->currentpose;
	}
	// don't lerp
/*	{
		lerpdata->blend = 1;
		lerpdata->pose1 = posenum;
		lerpdata->pose2 = posenum;
	}	*/
}

/*
=============
R_SetupEntityTransform -- model transform interpolation

set up transform part of lerpdata
=============
*/
void R_SetupEntityTransform (entity_t *e, lerpdata_t *lerpdata)
{
	float	blend;
	vec3_t	d;
	int	i;

	if (e->lerpflags & LERP_RESETMOVE) // kill any lerps in progress
	{
		e->movelerpstart = 0;
		VectorCopy (e->origin, e->previousorigin);
		VectorCopy (e->origin, e->currentorigin);
		VectorCopy (e->angles, e->previousangles);
		VectorCopy (e->angles, e->currentangles);
		e->lerpflags -= LERP_RESETMOVE;
	}
	else if (!VectorCompare (e->origin, e->currentorigin) || !VectorCompare (e->angles, e->currentangles)) // origin/angles changed, start new lerp
	{
		e->movelerpstart = cl.time;
		VectorCopy (e->currentorigin, e->previousorigin);
		VectorCopy (e->origin,  e->currentorigin);
		VectorCopy (e->currentangles, e->previousangles);
		VectorCopy (e->angles,  e->currentangles);
	}

	// set up values
	if (stupidquakebugfix && e == &cl.viewent)
		e->angles[0] = -e->angles[0]; // stupid quake bug

	if (e != &cl.viewent && e->lerpflags & LERP_MOVESTEP)
	{
		if (e->lerpflags & LERP_FINISH)
			blend = CLAMP (0, (cl.time - e->movelerpstart) / (e->lerpfinish - e->movelerpstart), 1);
		else
			blend = CLAMP (0, (cl.time - e->movelerpstart) / 0.1, 1);

		// positional interpolation (translation)
		VectorSubtract (e->currentorigin, e->previousorigin, d);
		lerpdata->origin[0] = e->previousorigin[0] + d[0] * blend;
		lerpdata->origin[1] = e->previousorigin[1] + d[1] * blend;
		lerpdata->origin[2] = e->previousorigin[2] + d[2] * blend;

		// orientation interpolation (rotation)
		VectorSubtract (e->currentangles, e->previousangles, d);

		// always interpolate along the shortest path
		for (i = 0; i < 3; i++)
		{
			if (d[i] > 180)
				d[i] -= 360;
			else if (d[i] < -180)
				d[i] += 360;
		}
		lerpdata->angles[0] = e->previousangles[0] + d[0] * blend;
		lerpdata->angles[1] = e->previousangles[1] + d[1] * blend;
		lerpdata->angles[2] = e->previousangles[2] + d[2] * blend;
	}
	else // don't lerp
	{
		VectorCopy (e->origin, lerpdata->origin);
		VectorCopy (e->angles, lerpdata->angles);
	}
}
