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
// gl_surf.c -- surface-related refresh code

#include "quakedef.h"

#define	BLOCK_WIDTH		128
#define	BLOCK_HEIGHT	128
// was 18*18, added lit support (*3 for RGB) and loosened surface extents maximum (BLOCK_WIDTH*BLOCK_HEIGHT)
#define BLOCKL_SIZE		(BLOCK_WIDTH*BLOCK_HEIGHT*3)
unsigned		blocklights[BLOCKL_SIZE];

#define	MAX_LIGHTMAPS	1024 // was 512 (orig. 64)
gltexture_t *lightmap_textures[MAX_LIGHTMAPS]; // changed to an array

typedef struct glRect_s
{
	byte l,t,w,h;
} glRect_t;

glpoly_t	*lightmap_polys[MAX_LIGHTMAPS];
qboolean	lightmap_modified[MAX_LIGHTMAPS];
glRect_t	lightmap_rectchange[MAX_LIGHTMAPS];

int			allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];
int			last_lightmap_allocated; //ericw -- optimization: remember the index of the last lightmap Lightmap_AllocBlock stored a surf in

// the lightmap texture data needs to be kept in
// main memory so texsubimage can update properly
byte		lightmaps[4*MAX_LIGHTMAPS*BLOCK_WIDTH*BLOCK_HEIGHT]; // (4)lightmap_bytes*MAX_LIGHTMAPS*BLOCK_WIDTH*BLOCK_HEIGHT

int			d_overbright = 1;
float		d_overbrightscale = OVERBRIGHT_SCALE;


/*
============================================================================================================

		ALPHA SORTING

============================================================================================================
*/

gl_alphalist_t	gl_alphalist[MAX_ALPHA_ITEMS];
int				gl_alphalist_num = 0;

/*
===============
R_SetAlphaSurface
===============
*/
qboolean R_SetAlphaSurface(msurface_t *s, float alpha)
{
    if (alpha < 1.0) {
        // do nothing
    } else if (s->flags & SURF_DRAWALPHA) { // TODO: trans 33/66 values
        alpha = 0.5f;
    } else if (s->flags & SURF_DRAWTURB) {
        alpha = R_GetTurbAlpha(s);
    }
    
    s->alpha = alpha;
    
    return (alpha < 1.0);
}

/*
===============
R_GetTurbAlpha
===============
*/
float R_GetTurbAlpha (msurface_t *s)
{
	float alpha = 1.0f;
	
	if (!r_lockalpha.value) // override water alpha for certain surface types
	{
		if (s->flags & SURF_DRAWLAVA)
			alpha = CLAMP(0.0, r_lavaalpha.value, 1.0);
		else if (s->flags & SURF_DRAWSLIME)
			alpha = CLAMP(0.0, r_slimealpha.value, 1.0);
		else if (s->flags & SURF_DRAWTELEPORT)
			alpha = CLAMP(0.0, r_teleportalpha.value, 1.0);
	}
	
	if (s->flags & SURF_DRAWWATER)
	{
		if (globalwateralpha > 0)
			alpha = globalwateralpha;
		else
			alpha = CLAMP(0.0, r_wateralpha.value, 1.0);
	}
	
	return alpha;
}

/*
===============
R_GetAlphaDist
===============
*/
vec_t R_GetAlphaDist (vec3_t origin)
{
	vec3_t	result;
	
	VectorSubtract (origin, r_origin, result);
	
	// no need to sqrt these as all we're concerned about is relative distances
	return DotProduct (result, result);
//	return VectorLength (result);
}

/*
===============
R_AddToAlpha
===============
*/
void R_AddToAlpha (int type, vec_t dist, void *data, entity_t *entity, float alpha)
{
	if (gl_alphalist_num == MAX_ALPHA_ITEMS)
		return;
	
	gl_alphalist[gl_alphalist_num].type = type;
	gl_alphalist[gl_alphalist_num].dist = dist;
	gl_alphalist[gl_alphalist_num].data = data;
	
	gl_alphalist[gl_alphalist_num].entity = entity;
	gl_alphalist[gl_alphalist_num].alpha = alpha;
	
	gl_alphalist_num++;
}

static inline int alphadistcompare (const void *arg1, const void *arg2) 
{
	// Sort in descending dist order, i.e. back to front
	// Sorted in reverse order
	const gl_alphalist_t *a1 = (gl_alphalist_t *)arg1;
	const gl_alphalist_t *a2 = (gl_alphalist_t *)arg2;
	
	// back to front ordering
	// this is more correct as it will order surfs properly if less than 1 unit separated
	if (a2->dist > a1->dist)
		return 1;
	else if (a2->dist < a1->dist)
		return -1;
	else
		return 0;
}

/*
=============
R_DrawAlpha
=============
*/
void R_DrawAlpha (void)
{
	int			i;
	gl_alphalist_t	a;
	
	if (gl_alphalist_num == 0)
		return;
		
	//
	// sort
	//
	if (gl_alphalist_num == 1)
	{
		// do nothing, no need to sort
	}
	else if (gl_alphalist_num == 2)
	{
		// exchange if necessary
		if (gl_alphalist[1].dist > gl_alphalist[0].dist)
		{
			gl_alphalist_t temp = gl_alphalist[0];
			gl_alphalist[0] = gl_alphalist[1];
			gl_alphalist[1] = temp;
		}
	}
	else
	{
		// sort fully
		qsort((void *)gl_alphalist, gl_alphalist_num, sizeof(gl_alphalist_t), alphadistcompare);
	}
	
	//
	// draw
	//
	for (i=0 ; i<gl_alphalist_num ; i++)
	{
		if ((i + 1) % 100 == 0)
			S_ExtraUpdateTime (); // don't let sound get messed up if going slow
		
		a = gl_alphalist[i];
		
		switch (a.type)
		{
		case ALPHA_SURFACE:
			{
				if (a.entity)
				{
					glPushMatrix ();
					glLoadMatrixf (a.entity->matrix); // load entity matrix
					
					R_DrawSequentialPoly ((msurface_t *)a.data, a.alpha, a.entity->frame); // draw entity surfaces
					
					glPopMatrix ();
				}
				else 
				{
					R_DrawSequentialPoly ((msurface_t *)a.data, a.alpha, 0); // draw world surfaces
				}
			}
			break;
			
		case ALPHA_ALIAS:
			R_DrawAliasModel ((entity_t *)a.data);
			break;
			
		case ALPHA_SPRITE:
			R_DrawSpriteModel ((entity_t *)a.data);
			break;
			
		case ALPHA_PARTICLE:
			R_DrawParticle ((particle_t *)a.data);
			break;
			
		case ALPHA_DLIGHTS:
			R_RenderDlight ((dlight_t *)a.data);
			break;
			
		default:
			break;
		}
	}
	
	gl_alphalist_num = 0;
}

// ----------------------------------------------------

/*
===============
R_AddDynamicLights
===============
*/
void R_AddDynamicLights (msurface_t *surf)
{
	int			lnum;
	int			sd, td;
	float		dist, rad, minlight;
	vec3_t		impact, local;
	int			s, t;
	int			i;
	int			smax, tmax;
	mtexinfo_t	*tex;
	// lit support via lordhavoc
	float		r, g, b, brightness;
	unsigned	*bl;
	float		scale;
	
	if (!r_dynamic.value) // EER1
		return;
	
	scale = CLAMP(1.0, r_dynamicscale.value, 32.0) * 256.0f;
	
	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	tex = surf->texinfo;

	for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		if ( !(surf->dlightbits[lnum >> 5] & (1U << (lnum & 31))) )
			continue;		// not lit by this light

		rad = cl_dlights[lnum].radius;
		dist = DotProduct (cl_dlights[lnum].origin, surf->plane->normal) - surf->plane->dist;
		rad -= fabs(dist);
		minlight = cl_dlights[lnum].minlight;

		if (rad < minlight)
			continue;

		minlight = rad - minlight;

		for (i=0 ; i<3 ; i++)
		{
			impact[i] = cl_dlights[lnum].origin[i] - surf->plane->normal[i]*dist;
		}

		local[0] = DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3];
		local[1] = DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3];

		local[0] -= surf->texturemins[0];
		local[1] -= surf->texturemins[1];
		
		// lit support via lordhavoc
		bl = blocklights;
		r = cl_dlights[lnum].color[0] * scale;//256.0f;
		g = cl_dlights[lnum].color[1] * scale;//256.0f;
		b = cl_dlights[lnum].color[2] * scale;//256.0f;

		for (t = 0 ; t<tmax ; t++)
		{
			td = local[1] - t*16;
			if (td < 0)
				td = -td;
			for (s=0 ; s<smax ; s++)
			{
				sd = local[0] - s*16;
				if (sd < 0)
					sd = -sd;
				if (sd > td)
					dist = sd + (td>>1);
				else
					dist = td + (sd>>1);

				if (dist < minlight)
				// lit support via lordhavoc
				{
					brightness = rad - dist;
					bl[0] += (int) (brightness * r);
					bl[1] += (int) (brightness * g);
					bl[2] += (int) (brightness * b);
				}
				bl += 3;
			}
		}
	}
}


/*
===============
R_BuildLightMap

combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/
void R_BuildLightMap (msurface_t *surf, byte *dest, int stride)
{
	int			smax, tmax;
	int			t;
	int			i, j, size;
	byte		*lightmap;
	unsigned	scale;
	int			maps;
	unsigned	*bl;
	int			shift;

	surf->cached_dlight = (surf->dlightframe == r_framecount);

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	size = smax*tmax;
	lightmap = surf->samples;

	if (size > BLOCKL_SIZE)
		Sys_Error ("R_BuildLightMap: too large blocklight size (%d, max = %d)", size, BLOCKL_SIZE);

	if (!r_fullbright.value && cl.worldmodel->lightdata)
	{
		// clear to no light
		memset (&blocklights[0], 0, size * 3 * sizeof (unsigned int)); // lit support via lordhavoc

		// add all the lightmaps
		if (lightmap)
			for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ; maps++)
			{
				scale = d_lightstyle[surf->styles[maps]];
				surf->cached_light[maps] = scale;	// 8.8 fraction
				// lit support via lordhavoc
				bl = blocklights;
				for (i=0 ; i<size ; i++)
				{
					*bl++ += *lightmap++ * scale;
					*bl++ += *lightmap++ * scale;
					*bl++ += *lightmap++ * scale;
				}
			}

		// add all the dynamic lights
		if (surf->dlightframe == r_framecount)
			R_AddDynamicLights (surf);
	}
	else
	{
		// set to full bright if no light data
		memset (&blocklights[0], 255, size * 3 * sizeof (unsigned int)); // lit support via lordhavoc
	}

	// bound, invert, and shift
	stride -= smax * 4;
	shift = 7 + d_overbright;
	bl = blocklights;
	for (i=0 ; i<tmax ; i++, dest += stride)
	{
		for (j=0 ; j<smax ; j++)
		{
			t = *bl++ >> shift;if (t > 255) t = 255;dest[0] = t;
			t = *bl++ >> shift;if (t > 255) t = 255;dest[1] = t;
			t = *bl++ >> shift;if (t > 255) t = 255;dest[2] = t;
			dest[3] = 255;
			dest += 4;
		}
	}
}

/*
===============
R_TextureAnimation

returns the proper texture for a given time and base texture
===============
*/
texture_t *R_TextureAnimation (texture_t *base, int frame)
{
	int		relative;
	int		count;

	if (frame)
		if (base->alternate_anims)
			base = base->alternate_anims;

	if (!base->anim_total)
		return base;

	relative = (int)(cl.time*10) % base->anim_total;

	count = 0;
	while (base->anim_min > relative || base->anim_max <= relative)
	{
		base = base->anim_next;
		if (!base)
			Host_Error ("R_TextureAnimation: broken cycle");
		if (++count > 100)
			Host_Error ("R_TextureAnimation: infinite cycle");
	}

	return base;
}

/*
=============================================================

	BRUSH MODELS

=============================================================
*/

/*
================
R_RenderDynamicLightmaps

================
*/
void R_RenderDynamicLightmaps (msurface_t *s)
{
	byte		*base;
	int			maps;
	glRect_t    *rect;
	int smax, tmax;

	if (s->flags & SURF_DRAWTILED) // not a lightmapped surface
		return;

	// add to lightmap chain
	s->polys->chain = lightmap_polys[s->lightmaptexture];
	lightmap_polys[s->lightmaptexture] = s->polys;

	// check for lightmap modification
	for (maps = 0 ; maps < MAXLIGHTMAPS && s->styles[maps] != 255 ; maps++)
		if (d_lightstyle[s->styles[maps]] != s->cached_light[maps])
			goto dynamic;

	if (s->dlightframe == r_framecount	// dynamic this frame
		|| s->cached_dlight)			// dynamic previously
	{
dynamic:
		if (!r_fullbright.value) // EER1
		{
			lightmap_modified[s->lightmaptexture] = true;
			rect = &lightmap_rectchange[s->lightmaptexture];

			if (s->light_t < rect->t) 
			{
				if (rect->h)
					rect->h += rect->t - s->light_t;
				rect->t = s->light_t;
			}
			if (s->light_s < rect->l) 
			{
				if (rect->w)
					rect->w += rect->l - s->light_s;
				rect->l = s->light_s;
			}

			smax = (s->extents[0]>>4)+1;
			tmax = (s->extents[1]>>4)+1;

			if ((rect->w + rect->l) < (s->light_s + smax))
				rect->w = (s->light_s-rect->l)+smax;
			if ((rect->h + rect->t) < (s->light_t + tmax))
				rect->h = (s->light_t-rect->t)+tmax;

			base = lightmaps + s->lightmaptexture*lightmap_bytes*BLOCK_WIDTH*BLOCK_HEIGHT;
			base += s->light_t * BLOCK_WIDTH * lightmap_bytes + s->light_s * lightmap_bytes;
			R_BuildLightMap (s, base, BLOCK_WIDTH*lightmap_bytes);
		}
	}
}


/*
===============
R_UploadLightmaps

uploads the modified lightmap to opengl if necessary
assumes lightmap texture is already bound
===============
*/
void R_UploadLightmaps (void)
{
	int lmap;
	glRect_t	*rect;

	for (lmap = 0; lmap < MAX_LIGHTMAPS; lmap++)
	{
		if (!lightmap_modified[lmap])
			continue;

		GL_Bind (lightmap_textures[lmap]);
		lightmap_modified[lmap] = false;

		rect = &lightmap_rectchange[lmap];

		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, rect->t, BLOCK_WIDTH, rect->h, GL_RGBA,
			GL_UNSIGNED_BYTE, lightmaps+(lmap* BLOCK_HEIGHT + rect->t) *BLOCK_WIDTH*lightmap_bytes);

		rect->l = BLOCK_WIDTH;
		rect->t = BLOCK_HEIGHT;
		rect->h = 0;
		rect->w = 0;

		// r_speeds
		rs_c_dynamic_lightmaps++;
	}
}


/*
================
R_DrawGLPoly34
================
*/
void R_DrawGLPoly34 (glpoly_t *p)
{
	float	*v;
	int		i;

	glBegin (GL_POLYGON);
	v = p->verts[0];
	for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
	{
		glTexCoord2f (v[3], v[4]);
		glVertex3fv (v);
	}
	glEnd ();
}

/*
================
R_DrawGLPoly56
================
*/
void R_DrawGLPoly56 (glpoly_t *p)
{
	float	*v;
	int		i;

	glBegin (GL_POLYGON);
	v = p->verts[0];
	for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
	{
		glTexCoord2f (v[5], v[6]);
		glVertex3fv (v);
	}
	glEnd ();
}

/*
================
R_DrawSequentialPoly

Systems that have fast state and texture changes can
just do everything as it passes with no need to sort
================
*/
void R_DrawSequentialPoly (msurface_t *s, float alpha, int frame)
{
	glpoly_t	*p;
	texture_t	*t;
	float		*v;
	float		lfog = 0; // keep compiler happy
	int			i;
    
	//
	// sky poly
	//
	if (s->flags & SURF_DRAWSKY)
		return; // skip it, already handled

	p = s->polys;
	
	//
	// water poly
	//
	if (s->flags & SURF_DRAWTURB)
	{
		if (alpha < 1.0)
		{
			glDepthMask(GL_FALSE);
			glEnable(GL_BLEND);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glColor4f(1, 1, 1, alpha);
		}

		GL_Bind (s->texinfo->texture->warpimage);

		if ( !(s->flags & (SURF_DRAWLAVA | SURF_DRAWSLIME)) )
		{
			R_DrawGLPoly34 (p);
			rs_c_brush_passes++; // r_speeds
		}
		else
		{
			R_FogDisableGFog ();
			R_DrawGLPoly34 (p);
			rs_c_brush_passes++; // r_speeds
			R_FogEnableGFog ();

			if (s->flags & SURF_DRAWLAVA)
				lfog = CLAMP(0.0, r_lavafog.value, 1.0);
			else if (s->flags & SURF_DRAWSLIME)
				lfog = CLAMP(0.0, r_slimefog.value, 1.0);

			if (R_FogGetDensity() > 0 && lfog > 0)
			{
				float *c = R_FogGetColor();

				glEnable (GL_BLEND);
				glColor4f (c[0],c[1],c[2], lfog);
				R_DrawGLPoly34 (p);
				rs_c_brush_passes++; // r_speeds
				glColor3f (1, 1, 1);
				glDisable (GL_BLEND);
			}
		}

		if (alpha < 1.0)
		{
			glDepthMask(GL_TRUE);
			glDisable(GL_BLEND);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			glColor3f(1, 1, 1);
		}
		
		return;
	}

	t = R_TextureAnimation (s->texinfo->texture, frame);

	//
	// missing texture
	//
	if (s->flags & SURF_NOTEXTURE)
	{
		if (alpha < 1.0)
		{
			glDepthMask(GL_FALSE);
			glEnable(GL_BLEND);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glColor4f(1, 1, 1, alpha);
		}

		GL_Bind (t->gltexture);
		R_DrawGLPoly34 (p);
		rs_c_brush_passes++; // r_speeds

		if (alpha < 1.0)
		{
			glDepthMask(GL_TRUE);
			glDisable(GL_BLEND);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			glColor3f(1, 1, 1);
		}

		return;
	}

	//
	// lightmapped poly
	//
	if ( !(s->flags & SURF_DRAWTILED) )
	{
		if (alpha < 1.0)
		{
			glDepthMask (GL_FALSE);
			glEnable (GL_BLEND);
			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glColor4f (1, 1, 1, alpha);
		}
		else
			glColor3f (1, 1, 1);

		if (s->flags & SURF_DRAWFENCE)
			glEnable (GL_ALPHA_TEST); // Flip on alpha test

		if (gl_mtexable && gl_texture_env_combine) // case 1: texture and lightmap in one pass, overbright using texture combiners
		{
			// Binds world to texture env 0
			GL_DisableMultitexture (); // selects TEXTURE0
			GL_Bind (t->gltexture);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

			// Binds lightmap to texture env 1
			GL_EnableMultitexture (); // selects TEXTURE1
			GL_Bind (lightmap_textures[s->lightmaptexture]);
			R_RenderDynamicLightmaps (s);

			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
			glTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE);
			glTexEnvf(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_PREVIOUS_EXT);
			glTexEnvf(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_TEXTURE);
			glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, d_overbrightscale);

			glBegin(GL_POLYGON);
			v = p->verts[0];
			for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
			{
				qglMultiTexCoord2f (GL_TEXTURE0_ARB, v[3], v[4]);
				qglMultiTexCoord2f (GL_TEXTURE1_ARB, v[5], v[6]);
				glVertex3fv (v);
			}
			glEnd ();
			rs_c_brush_passes++; // r_speeds

			glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 1.0f);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			GL_DisableMultitexture (); // selects TEXTURE0
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);//FX
		} 
		else if (alpha < 1.0 || (s->flags & SURF_DRAWFENCE)) // case 2: can't do multipass if brush has alpha, so just draw the texture
		{
			GL_Bind (t->gltexture);
			R_DrawGLPoly34 (p);
			rs_c_brush_passes++; // r_speeds
		}
		else // case 3: texture in one pass, lightmap in second pass using 2x modulation blend func, fog in third pass
		{
			// first pass -- texture with no fog
			R_FogDisableGFog ();
			GL_Bind (t->gltexture);
			R_DrawGLPoly34 (p);
			rs_c_brush_passes++; // r_speeds
			R_FogEnableGFog ();

			// second pass -- lightmap with black fog, modulate blended
			GL_Bind (lightmap_textures[s->lightmaptexture]);
			R_RenderDynamicLightmaps (s);

			glDepthMask (GL_FALSE); // don't bother writing Z
			glEnable (GL_BLEND);
			glBlendFunc (GL_DST_COLOR, GL_SRC_COLOR); // 2x modulate
			R_FogStartAdditive ();
			R_DrawGLPoly56 (p);
			rs_c_brush_passes++; // r_speeds
			R_FogStopAdditive ();

			// third pass -- black geo with normal fog, additive blended
			if (R_FogGetDensity() > 0)
			{
				glBlendFunc(GL_ONE, GL_ONE); //add
				glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
				glColor3f(0,0,0);
				R_DrawGLPoly34 (p);
				rs_c_brush_passes++; // r_speeds
				glColor3f(1,1,1);
				glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			}

			glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDisable (GL_BLEND);
			glDepthMask (GL_TRUE); // back to normal Z buffering
		}

		if (alpha < 1.0)
		{
			glDepthMask(GL_TRUE);
			glDisable(GL_BLEND);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			glColor3f(1, 1, 1);
		}

		if (s->flags & SURF_DRAWFENCE)
			glDisable (GL_ALPHA_TEST); // Flip alpha test back off

		if (t->fullbright)
		{
			GL_Bind (t->fullbright);
			glDepthMask (GL_FALSE); // don't bother writing Z
			glEnable (GL_BLEND);
			glBlendFunc (GL_ONE, GL_ONE);
			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glColor3f (alpha, alpha, alpha);
			R_FogStartAdditive ();
			R_DrawGLPoly34 (p);
			rs_c_brush_passes++; // r_speeds
			R_FogStopAdditive ();
			glColor3f (1, 1, 1);
			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDisable (GL_BLEND);
			glDepthMask (GL_TRUE); // back to normal Z buffering
		}
	}
}


/*
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModel (entity_t *e)
{
	int			k, i;
	msurface_t	*psurf;
	float		dot;
	mplane_t	*pplane;
	model_t		*clmodel;
	qboolean	rotated = false;
	float		alpha;
    qboolean	hasalpha = false;

	if (R_CullModelForEntity(e))
		return;
	
	clmodel = e->model;
	
	alpha = ENTALPHA_DECODE(e->alpha);
	
	VectorSubtract (r_refdef.vieworg, e->origin, modelorg);
	if (e->angles[0] || e->angles[1] || e->angles[2])
	{
		vec3_t	temp;
		vec3_t	forward, right, up;
		
		rotated = true;
		VectorCopy (modelorg, temp);
		AngleVectors (e->angles, forward, right, up);
		modelorg[0] = DotProduct (temp, forward);
		modelorg[1] = -DotProduct (temp, right);
		modelorg[2] = DotProduct (temp, up);
	}

    e->rotated = rotated;
    
	// calculate dynamic lighting for bmodel if it's not an instanced model
	if (clmodel->firstmodelsurface != 0) // EER1
	{
		for (k=0 ; k<MAX_DLIGHTS ; k++)
		{
			if ((cl_dlights[k].die < cl.time) ||
				(!cl_dlights[k].radius))
				continue;

			R_MarkLights (&cl_dlights[k], k, clmodel->nodes + clmodel->hulls[0].firstclipnode);
		}
	}

	glPushMatrix ();
	
	glTranslatef (e->origin[0],  e->origin[1],  e->origin[2]);
	if (rotated) 
	{
		glRotatef (e->angles[1],  0, 0, 1);
		glRotatef (e->angles[0],  0, 1, 0);
		glRotatef (e->angles[2],  1, 0, 0);
	}
	
    //
	// set all chains to null
    //
    R_ClearTextureChains(clmodel, chain_model);
    
	//
	// draw it
	//
	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];
	
	for (i=0 ; i<clmodel->nummodelsurfaces ; i++, psurf++)
	{
		// find which side of the node we are on
		pplane = psurf->plane;
		dot = DotProduct (modelorg, pplane->normal) - pplane->dist;

		// draw the polygon
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
            
            if (psurf->texinfo->texture->warpimage)
                psurf->texinfo->texture->update_warp = true; // FIXME: one frame too late!
            
            hasalpha = R_SetAlphaSurface(psurf, alpha);
            
            R_ChainSurface (psurf, chain_model);
			
			rs_c_brush_polys++; // r_speeds
		}
	}
	
    if (hasalpha)
        glGetFloatv (GL_MODELVIEW_MATRIX, e->matrix); // save entity matrix
    
    R_DrawTextureChains (clmodel, e, chain_model);
    
	glPopMatrix ();
}

/*
=============================================================

	WORLD MODEL

=============================================================
*/

int recursivecount;
/*
================
R_RecursiveWorldNode

this now only builds a surface chains and leafs
================
*/
void R_RecursiveWorldNode (mnode_t *node)
{
	int			c, side;
	mplane_t	*plane;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
	float		dot;
//	float		alpha = 1.0;
	
restart:
	if (node->contents == CONTENTS_SOLID)
		return;		// solid

	if (node->visframe != r_visframecount)
		return;

	if (R_CullBox (node->mins, node->maxs))
		return;

// if a leaf node, draw stuff
	if (node->contents < 0)
	{
		pleaf = (mleaf_t *)node;
		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c)
		{
			do
			{
				(*mark)->visframe = r_framecount;
				mark++;
			} while (--c);
		}

// deal with model fragments in this leaf
		if (pleaf->efrags)
			R_StoreEfrags (&pleaf->efrags);

		return;
	}

// node is just a decision point, so go down the apropriate sides

// find which side of the node we are on
	plane = node->plane;

	switch (plane->type)
	{
	case PLANE_X:
	case PLANE_Y:
	case PLANE_Z:
		dot = modelorg[plane->type] - plane->dist;
		break;
	default:
		dot = DotProduct (modelorg, plane->normal) - plane->dist;
		break;
	}

	if (dot >= 0)
		side = 0;
	else
		side = 1;

	if (++recursivecount % 2000 == 0)
		S_ExtraUpdateTime ();	// don't let sound get messed up if going slow

// recurse down the children, front side first
	R_RecursiveWorldNode (node->children[side]);

// draw stuff
	c = node->numsurfaces;

	if (c)
	{
		surf = cl.worldmodel->surfaces + node->firstsurface;

		if (dot < -BACKFACE_EPSILON)
			side = SURF_PLANEBACK;
		else if (dot > BACKFACE_EPSILON)
			side = 0;

		for ( ; c ; c--, surf++)
		{
			if (surf->visframe != r_framecount)
				continue;

			if (R_CullBox(surf->mins, surf->maxs))
				continue;		// outside

			if ((dot < 0) ^ !!(surf->flags & SURF_PLANEBACK))
				continue;		// wrong side

            if (surf->texinfo->texture->warpimage)
                surf->texinfo->texture->update_warp = true;
            
            R_SetAlphaSurface(surf, 1.0); // alpha
            
            R_ChainSurface(surf, chain_world);

			rs_c_brush_polys++; // r_speeds (count wpolys here)
		}
	}

// recurse down the back side
// optimize tail recursion
	node = node->children[!side];
	goto restart;
}

/*
===============
R_SetupSurfaces

setup surfaces based on PVS and rebuild texture chains
===============
*/
void R_SetupSurfaces (void)
{
    //
	// set all chains to null
    //
    R_ClearTextureChains(cl.worldmodel, chain_world);
    
    //
	// recursive rebuild chains
    //
    VectorCopy (r_refdef.vieworg, modelorg); // copy modelorg for recursiveWorldNode
    
    recursivecount = 0;
    R_RecursiveWorldNode (cl.worldmodel->nodes);
}


/*
=============================================================================

  TEXTURE CHAINS

=============================================================================
*/


/*
================
R_BuildLightmapChains -- johnfitz

ericw -- now always used at the start of R_DrawTextureChains for the 
mh dynamic lighting speedup
================
*/
void R_BuildLightmapChains (model_t *model, texchain_t chain)
{
	texture_t *t;
	msurface_t *s;
	int i;
    
	// clear lightmap chains
	memset (lightmap_polys, 0, sizeof(lightmap_polys));
    
	// now rebuild them
	for (i=0 ; i<model->numtextures ; i++)
	{
		t = model->textures[i];
        
		if (!t || !t->texturechains[chain])
			continue;
        
		for (s = t->texturechains[chain]; s; s = s->texturechain)
            R_RenderDynamicLightmaps (s);
	}
}


/*
================
R_DrawTextureChains_Alpha -- EER1
================
*/
void R_DrawTextureChains_Alpha (model_t *model, entity_t *e, texchain_t chain)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;
	qboolean	bound;
    
    for (i=0 ; i<model->numtextures ; i++)
    {
        t = model->textures[i];
        if (!t || !t->texturechains[chain] || t->texturechains[chain]->alpha == 1.0)
            continue;
        
        bound = false;
        
        for (s = t->texturechains[chain]; s; s = s->texturechain)
        {
            if (e) 
            {
                vec3_t	midp;
                vec_t	midp_dist;
                
                // transform the surface midpoint
                if (e->rotated)
                {
                    vec3_t	temp_midp;
                    vec3_t	forward, right, up;
                    
                    AngleVectors (e->angles, forward, right, up);
                    
                    VectorCopy (s->midp, temp_midp);
                    midp[0] = (DotProduct (temp_midp, forward) + e->origin[0]);
                    midp[1] = (DotProduct (temp_midp, right) + e->origin[1]);
                    midp[2] = (DotProduct (temp_midp, up) + e->origin[2]);
                }
                else
                {
                    VectorAdd (s->midp, e->origin, midp);
                }
                
                midp_dist = R_GetAlphaDist(midp);
                R_AddToAlpha (ALPHA_SURFACE, midp_dist, s, e, s->alpha);
            }
            else 
            {
                vec_t midp_dist;
                
                midp_dist = R_GetAlphaDist(s->midp);
                R_AddToAlpha (ALPHA_SURFACE, midp_dist, s, NULL, s->alpha);
            }
            
//            rs_c_brush_passes++;
        }
    }
}


/*
================
R_DrawTextureChains_Water -- johnfitz
================
*/
void R_DrawTextureChains_Water (model_t *model, entity_t *ent, texchain_t chain)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;
	qboolean	bound;
    
    for (i=0 ; i<model->numtextures ; i++)
    {
        t = model->textures[i];
        if (!t || !t->texturechains[chain] || t->texturechains[chain]->alpha < 1.0 || !(t->texturechains[chain]->flags & SURF_DRAWTURB))
            continue;
        
        bound = false;
        
        for (s = t->texturechains[chain]; s; s = s->texturechain)
        {
            if (!bound) //only bind once we are sure we need this texture
            {
                GL_Bind (t->warpimage);
                bound = true;
            }
            R_DrawGLPoly34 (s->polys);
            rs_c_brush_passes++;
        }
    }
}

/*
================
R_DrawTextureChains_NoTexture -- johnfitz

draws surfs whose textures were missing from the BSP
================
*/
void R_DrawTextureChains_NoTexture (model_t *model, texchain_t chain)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;
	qboolean	bound;
    
	for (i=0 ; i<model->numtextures ; i++)
	{
		t = model->textures[i];
		if (!t || !t->texturechains[chain] || t->texturechains[chain]->alpha < 1.0 || !(t->texturechains[chain]->flags & SURF_NOTEXTURE))
			continue;
        
		bound = false;
        
		for (s = t->texturechains[chain]; s; s = s->texturechain)
        {
            if (!bound) //only bind once we are sure we need this texture
            {
                GL_Bind (t->gltexture);
                bound = true;
            }
            R_DrawGLPoly34 (s->polys);
            rs_c_brush_passes++;
        }
	}
}

/*
================
R_DrawTextureChains_Multitexture -- johnfitz
================
*/
void R_DrawTextureChains_Multitexture (model_t *model, entity_t *ent, texchain_t chain)
{
	int			i, j;
	msurface_t	*s;
	texture_t	*t;
	float		*v;
	qboolean	bound;
    
	for (i=0 ; i<model->numtextures ; i++)
	{
		t = model->textures[i];
        
		if (!t || !t->texturechains[chain] || t->texturechains[chain]->alpha < 1.0 || t->texturechains[chain]->flags & (SURF_DRAWTILED | SURF_NOTEXTURE))
			continue;
        
		bound = false;
        
		for (s = t->texturechains[chain]; s; s = s->texturechain)
        {
            if (!bound) //only bind once we are sure we need this texture
            {
                GL_Bind ((R_TextureAnimation(t, ent != NULL ? ent->frame : 0))->gltexture);
                
                if (t->texturechains[chain]->flags & SURF_DRAWFENCE)
                    glEnable (GL_ALPHA_TEST); // Flip alpha test back on
                
                GL_EnableMultitexture(); // selects TEXTURE1
                bound = true;
            }
            GL_Bind (lightmap_textures[s->lightmaptexture]);
            glBegin(GL_POLYGON);
            v = s->polys->verts[0];
            for (j=0 ; j<s->polys->numverts ; j++, v+= VERTEXSIZE)
            {
                qglMultiTexCoord2f (GL_TEXTURE0_ARB, v[3], v[4]);
                qglMultiTexCoord2f (GL_TEXTURE1_ARB, v[5], v[6]);
                glVertex3fv (v);
            }
            glEnd ();
            rs_c_brush_passes++;
        }
		GL_DisableMultitexture(); // selects TEXTURE0
        
		if (bound && t->texturechains[chain]->flags & SURF_DRAWFENCE)
			glDisable (GL_ALPHA_TEST); // Flip alpha test back off
	}
}

/*
================
R_DrawTextureChains_TextureOnly -- johnfitz
================
*/
void R_DrawTextureChains_TextureOnly (model_t *model, entity_t *ent, texchain_t chain)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;
	qboolean	bound;
    
	for (i=0 ; i<model->numtextures ; i++)
	{
		t = model->textures[i];
        
		if (!t || !t->texturechains[chain] || t->texturechains[chain]->alpha < 1.0 || t->texturechains[chain]->flags & (SURF_DRAWTURB | SURF_DRAWSKY))
			continue;
        
		bound = false;
        
		for (s = t->texturechains[chain]; s; s = s->texturechain)
        {
            if (!bound) //only bind once we are sure we need this texture
            {
                GL_Bind ((R_TextureAnimation(t, ent != NULL ? ent->frame : 0))->gltexture);
                
                if (t->texturechains[chain]->flags & SURF_DRAWFENCE)
                    glEnable (GL_ALPHA_TEST); // Flip alpha test back on
                
                bound = true;
            }
            R_DrawGLPoly34 (s->polys);
            rs_c_brush_passes++;
        }
        
		if (bound && t->texturechains[chain]->flags & SURF_DRAWFENCE)
			glDisable (GL_ALPHA_TEST); // Flip alpha test back off
	}
}

/*
================
R_DrawLightmapChains -- johnfitz
 
R_BlendLightmaps stripped down to almost nothing
================
*/
void R_DrawLightmapChains (void)
{
	int			i, j;
	glpoly_t	*p;
	float		*v;
    
	for (i=0 ; i<MAX_LIGHTMAPS ; i++)
	{
		if (!lightmap_polys[i])
			continue;
        
		GL_Bind (lightmap_textures[i]);
		for (p = lightmap_polys[i]; p; p=p->chain)
		{
			glBegin (GL_POLYGON);
			v = p->verts[0];
			for (j=0 ; j<p->numverts ; j++, v+= VERTEXSIZE)
			{
				glTexCoord2f (v[5], v[6]);
				glVertex3fv (v);
			}
			glEnd ();
			rs_c_brush_passes++;
		}
	}
}

/*
================
R_DrawTextureChains_Glow -- johnfitz
================
*/
void R_DrawTextureChains_Glow (model_t *model, entity_t *ent, texchain_t chain)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;
	gltexture_t	*glt;
	qboolean	bound;
    
	for (i=0 ; i<model->numtextures ; i++)
	{
		t = model->textures[i];
        
		if (!t || !t->texturechains[chain] || t->texturechains[chain]->alpha < 1.0 || !(glt = R_TextureAnimation(t, ent != NULL ? ent->frame : 0)->fullbright))
			continue;
        
		bound = false;
        
		for (s = t->texturechains[chain]; s; s = s->texturechain)
        {
            if (!bound) //only bind once we are sure we need this texture
            {
                GL_Bind (glt);
                bound = true;
            }
            R_DrawGLPoly34 (s->polys);
            rs_c_brush_passes++;
        }
	}
}

/*
=============
R_DrawTextureChains -- johnfitz
=============
*/
void R_DrawTextureChains (model_t *model, entity_t *ent, texchain_t chain)
{
    float alpha = 1.0f;

    // ericw -- the mh dynamic lightmap speedup: make a first pass through all
    // surfaces we are going to draw, and rebuild any lightmaps that need it.
    // the previous implementation of the speedup uploaded lightmaps one frame
    // late which was visible under some conditions, this method avoids that.
	R_BuildLightmapChains (model, chain);
	R_UploadLightmaps ();
    
    R_DrawTextureChains_Alpha (model, ent, chain);
    R_DrawTextureChains_Water (model, ent, chain);
    R_DrawTextureChains_NoTexture (model, chain);
    
    if (gl_texture_env_combine && gl_mtexable) //case 1: texture and lightmap in one pass, overbright using texture combiners
    {
        GL_EnableMultitexture ();
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_PREVIOUS_EXT);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_TEXTURE);
        glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, d_overbrightscale);
        GL_DisableMultitexture ();
        R_DrawTextureChains_Multitexture (model, ent, chain);
        GL_EnableMultitexture ();
        glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 1.0f);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        GL_DisableMultitexture ();
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    }
    else if (alpha < 1) //case 2: can't do multipass if entity has alpha, so just draw the texture
    {
        R_DrawTextureChains_TextureOnly (model, ent, chain);
    }
    else //case 3: texture in one pass, lightmap in second pass using 2x modulation blend func, fog in third pass
    {
        //to make fog work with multipass lightmapping, need to do one pass
        //with no fog, one modulate pass with black fog, and one additive
        //pass with black geometry and normal fog
        R_FogDisableGFog ();
        R_DrawTextureChains_TextureOnly (model, ent, chain);
        R_FogEnableGFog ();
        
        glDepthMask (GL_FALSE);
        glEnable (GL_BLEND);
        glBlendFunc (GL_DST_COLOR, GL_SRC_COLOR); //2x modulate
        R_FogStartAdditive ();
        R_DrawLightmapChains ();
        R_FogStopAdditive ();
        
        if (R_FogGetDensity() > 0)
        {
            glBlendFunc(GL_ONE, GL_ONE); //add
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            glColor3f(0,0,0);
            R_DrawTextureChains_TextureOnly (model, ent, chain);
            glColor3f(1,1,1);
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        }
        
        glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable (GL_BLEND);
        glDepthMask (GL_TRUE);
    }
    
    glDepthMask (GL_FALSE);
    glEnable (GL_BLEND);
    glBlendFunc (GL_ONE, GL_ONE);
    glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glColor3f (alpha, alpha, alpha);
    R_FogStartAdditive ();
    R_DrawTextureChains_Glow (model, ent, chain);
    R_FogStopAdditive ();
    glColor3f (1, 1, 1);
    glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable (GL_BLEND);
    glDepthMask (GL_TRUE);
}


/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld (void)
{
	if (!r_drawworld.value)
		return;

    R_DrawTextureChains(cl.worldmodel, NULL, chain_world);
}

/*
===============
R_MarkLeaves
===============
*/
void R_MarkLeaves (void)
{
	byte	*vis;
	mnode_t	*node;
	msurface_t **mark;
	int	   i;
	qboolean   nearwaterportal = false;
	
	// check if near water to avoid HOMs when crossing the surface
	for (i=0, mark = r_viewleaf->firstmarksurface; i < r_viewleaf->nummarksurfaces; i++, mark++)
	{
		if ((*mark)->flags & SURF_DRAWTURB)
		{
//			Con_SafePrintf ("R_MarkLeaves: nearwaterportal, surfs=%d\n", r_viewleaf->nummarksurfaces);
			nearwaterportal = true;
			break;
		}
	}

	// return if viewleaf don't need regenerating
	if (r_oldviewleaf == r_viewleaf && !r_novis.value && !nearwaterportal)
		return;

	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	// choose vis data
	if (r_novis.value || r_viewleaf->contents == CONTENTS_SOLID || r_viewleaf->contents == CONTENTS_SKY)
        vis = Mod_NoVisPVS (cl.worldmodel);
	else if (nearwaterportal)
		vis = SV_FatPVS (r_origin, cl.worldmodel);
	else
		vis = Mod_LeafPVS (r_viewleaf, cl.worldmodel);
	
	// mark leafs and surfaces as visible	
	for (i=0 ; i<cl.worldmodel->numleafs ; i++)
	{
		if (vis[i>>3] & (1<<(i&7)))
		{
			node = (mnode_t *)&cl.worldmodel->leafs[i+1];
			do
			{
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}
}


/*
================
R_ChainSurface -- ericw

adds the given surface to its texture chain
================
*/
void R_ChainSurface (msurface_t *surf, texchain_t chain)
{
    // sort by texture
	surf->texturechain = surf->texinfo->texture->texturechains[chain];
	surf->texinfo->texture->texturechains[chain] = surf;
}

/*
================
R_ClearTextureChains -- ericw 

clears texture chains for all textures used by the given model
================
*/
void R_ClearTextureChains (model_t *model, texchain_t chain)
{
	int i;
    
	// set all chains to null
	for (i=0 ; i<model->numtextures ; i++)
		if (model->textures[i]) {
			model->textures[i]->texturechains[chain] = NULL;
        }
}


/*
=============================================================================

  LIGHTMAP ALLOCATION

=============================================================================
*/

/*
========================
Lightmap_AllocBlock

returns a texture number and the position inside it
========================
*/
int Lightmap_AllocBlock (int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;
	int		texnum;

	// ericw -- rather than searching starting at lightmap 0 every time,
	// start at the last lightmap we allocated a surface in.
	// This makes Lightmap_AllocBlock much faster on large levels (can shave off 3+ seconds
	// of load time on a level with 180 lightmaps), at a cost of not quite packing
	// lightmaps as tightly vs. not doing this (uses ~5% more lightmaps)
	for (texnum=last_lightmap_allocated ; texnum<MAX_LIGHTMAPS ; texnum++, last_lightmap_allocated++)
//	for (texnum=0 ; texnum<MAX_LIGHTMAPS ; texnum++)
	{
		best = BLOCK_HEIGHT;

		for (i=0 ; i<BLOCK_WIDTH-w ; i++)
		{
			best2 = 0;

			for (j=0 ; j<w ; j++)
			{
				if (allocated[texnum][i+j] >= best)
					break;
				if (allocated[texnum][i+j] > best2)
					best2 = allocated[texnum][i+j];
			}
			if (j == w)
			{	// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		for (i=0 ; i<w ; i++)
			allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	return -1;
}


/*
================
R_BuildSurfaceDisplayList
================
*/
mvertex_t	*r_pcurrentvertbase;
model_t		*currentmodel;

void R_BuildSurfaceDisplayList (msurface_t *surf)
{
	int			i, lindex, lnumverts;
	medge_t		*pedges, *r_pedge;
	float		*vec;
	float		s, t;
	glpoly_t	*poly;

// reconstruct the polygon
	pedges = currentmodel->edges;
	lnumverts = surf->numedges;

	//
	// draw texture
	//
	poly = Hunk_AllocName (sizeof(glpoly_t) + (lnumverts-4) * VERTEXSIZE*sizeof(float), "poly");
	poly->next = surf->polys;

	surf->polys = poly;
	poly->numverts = lnumverts;

	for (i=0 ; i<lnumverts ; i++)
	{
		lindex = currentmodel->surfedges[surf->firstedge + i];

		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];
			vec = r_pcurrentvertbase[r_pedge->v[0]].position;
		}
		else
		{
			r_pedge = &pedges[-lindex];
			vec = r_pcurrentvertbase[r_pedge->v[1]].position;
		}
		s = DotProduct (vec, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3];
		s /= surf->texinfo->texture->width;

		t = DotProduct (vec, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3];
		t /= surf->texinfo->texture->height;

		VectorCopy (vec, poly->verts[i]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		//
		// lightmap texture coordinates
		//
		s = DotProduct (vec, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3];
		s -= surf->texturemins[0];
		s += surf->light_s*16;
		s += 8;
		s /= BLOCK_WIDTH*16; //surf->texinfo->texture->width;

		t = DotProduct (vec, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3];
		t -= surf->texturemins[1];
		t += surf->light_t*16;
		t += 8;
		t /= BLOCK_HEIGHT*16; //surf->texinfo->texture->height;

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;
	}

	poly->numverts = lnumverts;
}

/*
========================
R_CreateSurfaceLightmap
========================
*/
void R_CreateSurfaceLightmap (msurface_t *surf)
{
	int		smax, tmax;
	byte	*base;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;

	surf->lightmaptexture = Lightmap_AllocBlock (smax, tmax, &surf->light_s, &surf->light_t);
	if (surf->lightmaptexture == -1)
		Sys_Error ("Lightmap_AllocBlock: full");

	base = lightmaps + surf->lightmaptexture*lightmap_bytes*BLOCK_WIDTH*BLOCK_HEIGHT;
	base += (surf->light_t * BLOCK_WIDTH + surf->light_s) * lightmap_bytes;
	R_BuildLightMap (surf, base, BLOCK_WIDTH*lightmap_bytes);
}


/*
==================
R_BuildLightmaps

called at level load time
Builds the lightmap texture
with all the surfaces from all brush models
==================
*/
void R_BuildLightmaps (void)
{
	char	name[64];
	byte	*data;
	int		i, j;
	model_t	*m;

	memset (allocated, 0, sizeof(allocated));
	last_lightmap_allocated = 0;

	r_framecount = 1;		// no dlightcache

	// null out array (the gltexture objects themselves were already freed by Mod_ClearAll)
	for (i=0; i < MAX_LIGHTMAPS; i++)
		lightmap_textures[i] = NULL;

	for (j=1 ; j<MAX_MODELS ; j++)
	{
		m = cl.model_precache[j];
		if (!m)
			continue; // When missing models, there might be NULL entries in the list
		if (m->name[0] == '*')
			continue;
		r_pcurrentvertbase = m->vertexes;
		currentmodel = m;
		for (i=0 ; i<m->numsurfaces ; i++)
		{
			// use SURF_DRAWTILED instead of the sky/water flags
			if (m->surfaces[i].flags & SURF_DRAWTILED)
				continue;
			R_CreateSurfaceLightmap (m->surfaces + i);
			R_BuildSurfaceDisplayList (m->surfaces + i);
		}
	}

	//
	// upload all lightmaps that were filled
	//
	for (i=0 ; i<MAX_LIGHTMAPS ; i++)
	{
		if (!allocated[i][0])
			break;		// no more used
		lightmap_modified[i] = false;
		lightmap_rectchange[i].l = BLOCK_WIDTH;
		lightmap_rectchange[i].t = BLOCK_HEIGHT;
		lightmap_rectchange[i].w = 0;
		lightmap_rectchange[i].h = 0;

		sprintf(name, "lightmap%03i",i);

		data = lightmaps+i*BLOCK_WIDTH*BLOCK_HEIGHT*lightmap_bytes;

		lightmap_textures[i] = GL_LoadTexture (cl.worldmodel, name, BLOCK_WIDTH, BLOCK_HEIGHT, SRC_LIGHTMAP, data, "", (uintptr_t)data, TEXPREF_LINEAR | TEXPREF_NOPICMIP);
	}

	// old limit warning
	if (i > 64)
		Con_DWarning ("R_BuildLightmaps: lightmaps exceeds standard limit (%d, normal max = %d)\n", i, 64);
}


/*
================
R_RebuildAllLightmaps -- johnfitz -- called when gl_overbright gets toggled
================
*/
void R_RebuildAllLightmaps (void)
{
	int			i, j;
	model_t		*mod;
	msurface_t	*fa;
	byte		*base;
    
	if (!cl.worldmodel) // is this the correct test?
		return;
    
	//for each surface in each model, rebuild lightmap with new scale
	for (i=1; i<MAX_MODELS; i++)
	{
		if (!(mod = cl.model_precache[i]))
			continue;
		fa = &mod->surfaces[mod->firstmodelsurface];
		for (j=0; j<mod->nummodelsurfaces; j++, fa++)
		{
			if (fa->flags & SURF_DRAWTILED)
				continue;
			base = lightmaps + fa->lightmaptexture*lightmap_bytes*BLOCK_WIDTH*BLOCK_HEIGHT;
			base += fa->light_t * BLOCK_WIDTH * lightmap_bytes + fa->light_s * lightmap_bytes;
			R_BuildLightMap (fa, base, BLOCK_WIDTH*lightmap_bytes);
		}
	}
    
	//for each lightmap, upload it
	for (i=0; i<MAX_LIGHTMAPS; i++)
	{
		if (!allocated[i][0])
			break;
		GL_Bind (lightmap_textures[i]);
		glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, BLOCK_WIDTH, BLOCK_HEIGHT, GL_RGBA,
                         GL_UNSIGNED_BYTE, lightmaps+i*BLOCK_WIDTH*BLOCK_HEIGHT*lightmap_bytes);
	}
}

