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

#define LMBLOCK_WIDTH	256	//FIXME: make dynamic.
#define LMBLOCK_HEIGHT	256 // if we have a decent card there's no real reason not to use 4k or 16k (assuming there's no lightstyles/dynamics that need uploading...)
							// Alternatively, use texture arrays, which would avoid the need to switch textures as often.
// was 18*18, added lit support (*3 for RGB) and loosened surface extents maximum (LMBLOCK_WIDTH*LMBLOCK_HEIGHT)
#define BLOCKL_SIZE		(LMBLOCK_WIDTH*LMBLOCK_HEIGHT*3)
unsigned		blocklights[BLOCKL_SIZE];

#define	MAX_SANITY_LIGHTMAPS	(1u<<20)

typedef struct glRect_s
{
	unsigned short l,t,w,h;
} glRect_t;

typedef struct lightmap_s
{
	gltexture_t *texture;
	glpoly_t	*polys;
	qboolean	modified;
	glRect_t	rectchange;

	// the lightmap texture data needs to be kept in
	// main memory so texsubimage can update properly
	byte		*data;//[4*LMBLOCK_WIDTH*LMBLOCK_HEIGHT];
} lightmap_t;

lightmap_t	*lightmaps;
int			lightmap_count;

int			allocated[LMBLOCK_WIDTH];
int			last_lightmap_allocated; //ericw -- optimization: remember the index of the last lightmap Lightmap_AllocBlock stored a surf in


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
void R_AddToAlpha (int type, vec_t dist, void *data, model_t *model, entity_t *entity, float alpha)
{
	if (gl_alphalist_num == MAX_ALPHA_ITEMS)
		return;
	
	gl_alphalist[gl_alphalist_num].type = type;
	gl_alphalist[gl_alphalist_num].dist = dist;
	gl_alphalist[gl_alphalist_num].data = data;
	gl_alphalist[gl_alphalist_num].model = model;
	
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
	
	if (r_noalphasort.value) // EER1
		goto skipsort;
	
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
	
skipsort:
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
					
					R_DrawSequentialPoly ((msurface_t *)a.data, a.alpha, a.model, a.entity); // draw entity surfaces
					
					glPopMatrix ();
				}
				else 
				{
					R_DrawSequentialPoly ((msurface_t *)a.data, a.alpha, a.model, NULL); // draw world surfaces
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
	dlight_t	*l;
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
	float		dscale;
	
	if (!r_dynamic.value) // EER1
		return;
	
	dscale = CLAMP(1.0, r_dynamicscale.value, 32.0);
	
	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	tex = surf->texinfo;

	for (lnum=0, l = cl_dlights ; lnum<MAX_DLIGHTS ; lnum++, l++)
	{
		if ( !(surf->dlightbits[lnum >> 5] & (1U << (lnum & 31))) )
			continue;		// not lit by this light

		rad = l->radius;
		dist = DotProduct (l->origin, surf->plane->normal) - surf->plane->dist;
		rad -= fabs(dist);
		// rad is now the highest intensity on the plane
		minlight = l->minlight;

		if (rad < minlight)
			continue;

		minlight = rad - minlight;

		for (i=0 ; i<3 ; i++)
		{
			impact[i] = l->origin[i] - surf->plane->normal[i]*dist;
		}

		local[0] = DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3];
		local[1] = DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3];

		local[0] -= surf->texturemins[0];
		local[1] -= surf->texturemins[1];
		
		// lit support via lordhavoc
		bl = blocklights;
		r = l->color[0] * dscale * 256.0f;
		g = l->color[1] * dscale * 256.0f;
		b = l->color[2] * dscale * 256.0f;

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
	int			maps;
	unsigned	scale, *bl, ambient_light;
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
		
		// clear to ambient
		bl = blocklights;
		ambient_light = (unsigned int)(max(0, r_ambient.value)) << 8;
		for (i = 0; i < size; i++)
		{
			*bl++ = ambient_light;
			*bl++ = ambient_light;
			*bl++ = ambient_light;
		}
		
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
	glRect_t	*rect;
	lightmap_t	*lm;
	int smax, tmax;

	if (s->flags & SURF_DRAWTILED) // not a lightmapped surface
		return;

	// add to lightmap chain
	s->polys->chain = lightmaps[s->lightmaptexture].polys;
	lightmaps[s->lightmaptexture].polys = s->polys;

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
			lm = &lightmaps[s->lightmaptexture];
			lm->modified = true;
			rect = &lm->rectchange;
			
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

			base = lm->data;
			base += s->light_t * LMBLOCK_WIDTH * lightmap_bytes + s->light_s * lightmap_bytes;
			R_BuildLightMap (s, base, LMBLOCK_WIDTH*lightmap_bytes);
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
	lightmap_t	*lm;

	for (lmap = 0; lmap < lightmap_count; lmap++)
	{
		lm = &lightmaps[lmap];
		
		if (!lm->modified)
			continue;

		GL_BindTexture (lm->texture);
		
		lm->modified = false;
		
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, lm->rectchange.t, LMBLOCK_WIDTH, lm->rectchange.h, GL_RGBA,
			GL_UNSIGNED_BYTE, lm->data + lm->rectchange.t*LMBLOCK_WIDTH*lightmap_bytes);
				
		lm->rectchange.l = LMBLOCK_WIDTH;
		lm->rectchange.t = LMBLOCK_HEIGHT;
		lm->rectchange.h = 0;
		lm->rectchange.w = 0;
		
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
void R_DrawSequentialPoly (msurface_t *s, float alpha, model_t *model, entity_t *ent)
{
	glpoly_t	*p;
	texture_t	*t;
	float		*v;
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
		qboolean	flatcolor = r_flatturb.value;
		qboolean	litwater = model->haslitwater && r_litwater.value;
		qboolean	special;

		if (flatcolor)
			glDisable (GL_TEXTURE_2D);
		
		if (alpha < 1.0)
		{
			glDepthMask(GL_FALSE);
			glEnable(GL_BLEND);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glColor4f(1, 1, 1, alpha);
		}
		
		
		special = !!(s->flags & TEX_SPECIAL);
		
		// Binds world to texture env 0
		GL_SelectTMU0 ();
		GL_BindTexture (s->texinfo->texture->warpimage);
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		
		if (flatcolor) {
			glColor4f (s->texinfo->texture->base->colors.flatcolor[0],
					   s->texinfo->texture->base->colors.flatcolor[1],
					   s->texinfo->texture->base->colors.flatcolor[2], alpha);
		}
		
		if (litwater && !special) {
			// Binds lightmap to texture env 1
			GL_SelectTMU1 ();
			GL_BindTexture (lightmaps[s->lightmaptexture].texture);
			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
			glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
			glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB);
			glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE);
			glTexEnvf (GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, d_overbrightscale);
			
			R_RenderDynamicLightmaps (s);
		}
		
		
		glBegin (GL_POLYGON);
		v = p->verts[0];
		for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
		{
			qglMultiTexCoord2f (GL_TEXTURE0_ARB, v[3], v[4]);
			if (litwater && !special)
				qglMultiTexCoord2f (GL_TEXTURE1_ARB, v[5], v[6]);
			
			glVertex3fv (v);
		}
		glEnd ();
		rs_c_brush_passes++; // r_speeds
		
		
		if (litwater && !special) {
//			GL_SelectTMU1 ();
			glTexEnvf (GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1.0f);
			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		}
		
		GL_SelectTMU0 ();
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE); //FX
		
		
		if (alpha < 1.0)
		{
			glDepthMask(GL_TRUE);
			glDisable(GL_BLEND);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			glColor3f(1, 1, 1);
		}
		
		if (flatcolor) {
			glColor3f (1, 1, 1);
			glEnable (GL_TEXTURE_2D);
		}
		
		return;
	}

	t = R_TextureAnimation (s->texinfo->texture, ent != NULL ? ent->frame : 0);

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

		GL_BindTexture (t->base);
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
		qboolean	flatcolor = r_flatworld.value;
		
		if (flatcolor)
			glDisable (GL_TEXTURE_2D);
		
		if (alpha < 1.0)
		{
			glDepthMask (GL_FALSE);
			glEnable (GL_BLEND);
			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glColor4f (1, 1, 1, alpha);
		}
		else
			glColor3f (1, 1, 1);

		if (s->flags & SURF_DRAWHOLEY)
			glEnable (GL_ALPHA_TEST); // Flip on alpha test
		
		
		
		// Binds world to texture env 0
		GL_SelectTMU0 ();
		GL_BindTexture (t->base);
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		
		if (flatcolor) {
			glColor4f (t->base->colors.basecolor[0],
					   t->base->colors.basecolor[1],
					   t->base->colors.basecolor[2], alpha);
		}
		
		if (t->glow)
		{
			// Binds fullbright to texture env 2
			GL_SelectTMU2 ();
			GL_BindTexture (t->glow);
			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
			glEnable (GL_BLEND);
		}
		
		// Binds lightmap to texture env 1
		GL_SelectTMU1 ();
		GL_BindTexture (lightmaps[s->lightmaptexture].texture);
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
		glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
		glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB);
		glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE);
		glTexEnvf (GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, d_overbrightscale);
		
		
		R_RenderDynamicLightmaps (s);
		
		glBegin (GL_POLYGON);
		v = p->verts[0];
		for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
		{
			qglMultiTexCoord2f (GL_TEXTURE0_ARB, v[3], v[4]);
			qglMultiTexCoord2f (GL_TEXTURE1_ARB, v[5], v[6]);
			if (t->glow)
				qglMultiTexCoord2f (GL_TEXTURE2_ARB, v[3], v[4]);
			
			glVertex3fv (v);
		}
		glEnd ();
		rs_c_brush_passes++; // r_speeds
		
		
//		GL_SelectTMU1 ();
		glTexEnvf (GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1.0f);
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		
		if (t->glow)
		{
			glDisable (GL_TEXTURE_2D);
			GL_SelectTMU2 ();
			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			glDisable (GL_BLEND);
		}
		
		GL_SelectTMU0 ();
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE); //FX
		
		
		if (alpha < 1.0)
		{
			glDepthMask (GL_TRUE);
			glDisable (GL_BLEND);
			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			glColor3f (1, 1, 1);
		}

		if (s->flags & SURF_DRAWHOLEY)
			glDisable (GL_ALPHA_TEST); // Flip alpha test back off
		
		if (flatcolor) {
			glColor3f (1, 1, 1);
			glEnable (GL_TEXTURE_2D);
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
	dlight_t	*l;
	msurface_t	*surf;
	float		dot;
	mplane_t	*plane;
	model_t		*clmodel;
	qboolean	rotated = false;
	float		alpha;
    qboolean	hasalpha = false;
	float		scalefactor;
	
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
		for (k=0, l = cl_dlights ; k<MAX_DLIGHTS ; k++, l++)
		{
			if (l->die < cl.time || !l->radius)
				continue;

			if (R_CullSphere (l->origin, l->radius))
				continue;
			
			R_MarkLights (l, k, clmodel->nodes + clmodel->hulls[0].firstclipnode);
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
	
	scalefactor = ENTSCALE_DECODE(e->scale);
	if (scalefactor != 1.0f)
		glScalef(scalefactor, scalefactor, scalefactor);
	
    //
	// set all chains to null
    //
    R_ClearTextureChains(clmodel, chain_model);
    
	//
	// draw it
	//
	surf = &clmodel->surfaces[clmodel->firstmodelsurface];
	
	for (i=0 ; i<clmodel->nummodelsurfaces ; i++, surf++)
	{
		// find which side of the node we are on
		plane = surf->plane;
		dot = DotProduct (modelorg, plane->normal) - plane->dist;

		// draw the polygon
		if (((surf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(surf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
            
            if (surf->texinfo->texture->warpimage)
                surf->texinfo->texture->update_warp = true; // FIXME: one frame too late!
            
            hasalpha = R_SetAlphaSurface(surf, alpha);
            
            R_ChainSurface (surf, chain_model);
			
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
	mleaf_t		*leaf;
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
		leaf = (mleaf_t *)node;
		mark = leaf->firstmarksurface;
		c = leaf->nummarksurfaces;

		if (c)
		{
			do
			{
				(*mark)->visframe = r_framecount;
				mark++;
			} while (--c);
		}

// deal with model fragments in this leaf
		if (leaf->efrags)
			R_StoreEfrags (&leaf->efrags);

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
	for (i=0 ; i<lightmap_count ; i++)
		lightmaps[i].polys = NULL;
    
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
				R_AddToAlpha (ALPHA_SURFACE, midp_dist, s, model, e, s->alpha);
            }
            else 
            {
                vec_t midp_dist;
                
                midp_dist = R_GetAlphaDist(s->midp);
				R_AddToAlpha (ALPHA_SURFACE, midp_dist, s, model, NULL, s->alpha);
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
	int			i, j;
	msurface_t	*s;
	texture_t	*t;
	float		*v;
	qboolean	bound;
	qboolean	flatcolor = r_flatturb.value;
	qboolean	litwater = model->haslitwater && r_litwater.value;
	qboolean	special;
	
	if (flatcolor)
		glDisable (GL_TEXTURE_2D);
	
	for (i=0 ; i<model->numtextures ; i++)
	{
		t = model->textures[i];
		if (!t || !t->texturechains[chain] || t->texturechains[chain]->alpha < 1.0 || !(t->texturechains[chain]->flags & SURF_DRAWTURB))
			continue;
		
		special = !!(t->texturechains[chain]->texinfo->flags & TEX_SPECIAL);
		
		bound = false;
		
		for (s = t->texturechains[chain]; s; s = s->texturechain)
		{
			if (!bound) //only bind once we are sure we need this texture
			{
				GL_SelectTMU0 ();
				GL_BindTexture (t->warpimage);
				
				if (flatcolor)
					glColor3fv (t->base->colors.flatcolor);
				
				bound = true;
			}
			
			if (litwater && !special) {
				GL_SelectTMU1 ();
				GL_BindTexture (lightmaps[s->lightmaptexture].texture);
			}
			
			glBegin(GL_POLYGON);
			v = s->polys->verts[0];
			for (j=0 ; j<s->polys->numverts ; j++, v+= VERTEXSIZE)
			{
				qglMultiTexCoord2f (GL_TEXTURE0_ARB, v[3], v[4]);
				if (litwater && !special)
					qglMultiTexCoord2f (GL_TEXTURE1_ARB, v[5], v[6]);
				
				glVertex3fv (v);
			}
			glEnd ();
			rs_c_brush_passes++;
		}
		
		GL_SelectTMU0 ();
	}
	
	if (flatcolor) {
		glColor3f (1, 1, 1);
		glEnable (GL_TEXTURE_2D);
	}
}

/*
================
R_DrawTextureChains_NoTexture -- johnfitz

draws surfs whose textures were missing from the BSP
================
*/
void R_DrawTextureChains_NoTexture (model_t *model, entity_t *ent, texchain_t chain)
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
                GL_BindTexture (t->base);
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
	texture_t	*tx;
	float		*v;
	qboolean	bound;
	gltexture_t	*base, *glow;
	qboolean	flatcolor = r_flatworld.value;
	
	if (flatcolor)
		glDisable (GL_TEXTURE_2D);
	
	for (i=0 ; i<model->numtextures ; i++)
	{
		t = model->textures[i];
        
		if (!t || !t->texturechains[chain] || t->texturechains[chain]->alpha < 1.0 || t->texturechains[chain]->flags & (SURF_DRAWTURB | SURF_DRAWTILED | SURF_NOTEXTURE))
			continue;
        
		bound = false;
        
		for (s = t->texturechains[chain]; s; s = s->texturechain)
        {
            if (!bound) //only bind once we are sure we need this texture
            {
				tx = R_TextureAnimation (t, ent != NULL ? ent->frame : 0);
				
				base = tx->base;
				GL_SelectTMU0 ();
				GL_BindTexture (base);
				
				if (flatcolor)
					glColor3fv (base->colors.basecolor);
				
                if (t->texturechains[chain]->flags & SURF_DRAWHOLEY)
                    glEnable (GL_ALPHA_TEST); // Flip alpha test back on
				
				if ((glow = tx->glow))
				{
					GL_SelectTMU2 ();
					GL_BindTexture (glow);
					
					glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
					glEnable (GL_BLEND);
				}
				
                bound = true;
            }
			
			GL_SelectTMU1 ();
			GL_BindTexture (lightmaps[s->lightmaptexture].texture);
			
            glBegin(GL_POLYGON);
            v = s->polys->verts[0];
            for (j=0 ; j<s->polys->numverts ; j++, v+= VERTEXSIZE)
            {
                qglMultiTexCoord2f (GL_TEXTURE0_ARB, v[3], v[4]);
                qglMultiTexCoord2f (GL_TEXTURE1_ARB, v[5], v[6]);
				if (glow)
					qglMultiTexCoord2f (GL_TEXTURE2_ARB, v[3], v[4]);
				
                glVertex3fv (v);
            }
            glEnd ();
            rs_c_brush_passes++;
        }
		
		if (glow) // assume our current selection is TMU2
		{
			glDisable (GL_TEXTURE_2D);
			GL_SelectTMU2 ();
			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			glDisable (GL_BLEND);
		}
		
		GL_SelectTMU0 ();
        
		if (bound && t->texturechains[chain]->flags & SURF_DRAWHOLEY)
			glDisable (GL_ALPHA_TEST); // Flip alpha test back off
	}
	
	if (flatcolor) {
		glColor3f (1, 1, 1);
		glEnable (GL_TEXTURE_2D);
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
    
	for (i=0 ; i<lightmap_count ; i++)
	{
		if (!lightmaps[i].polys)
			continue;
        
		GL_BindTexture (lightmaps[i].texture);
		for (p = lightmaps[i].polys; p; p=p->chain)
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
=============
R_DrawTextureChains -- johnfitz
=============
*/
void R_DrawTextureChains (model_t *model, entity_t *ent, texchain_t chain)
{
    // ericw -- the mh dynamic lightmap speedup: make a first pass through all
    // surfaces we are going to draw, and rebuild any lightmaps that need it.
    // the previous implementation of the speedup uploaded lightmaps one frame
    // late which was visible under some conditions, this method avoids that.
	R_BuildLightmapChains (model, chain);
	R_UploadLightmaps ();
    
    R_DrawTextureChains_Alpha (model, ent, chain); // R_BuildTextureChains_Alpha
	
	
	GL_SelectTMU1 ();
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
	glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
	glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB);
	glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE);
	glTexEnvf (GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, d_overbrightscale);
	
	GL_SelectTMU0 ();
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE); // FIXME: already in this mode?
	
	R_DrawTextureChains_Water (model, ent, chain);
	R_DrawTextureChains_NoTexture (model, ent, chain);
	
	R_DrawTextureChains_Multitexture (model, ent, chain);
	
	GL_SelectTMU1 ();
	glTexEnvf (GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1.0f);
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	
	GL_SelectTMU0 ();
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	
    
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
	for (texnum=last_lightmap_allocated ; texnum<MAX_SANITY_LIGHTMAPS ; texnum++)
	{
		if (texnum == lightmap_count)
		{
			lightmap_count++;
			lightmaps = (lightmap_t *) realloc (lightmaps, sizeof(*lightmaps)*lightmap_count);
			memset (&lightmaps[texnum], 0, sizeof(lightmaps[texnum]));
			lightmaps[texnum].data = (byte *) calloc (1, 4*LMBLOCK_WIDTH*LMBLOCK_HEIGHT);
			//as we're only tracking one texture, we don't need multiple copies of allocated any more.
			memset (allocated, 0, sizeof(allocated));
		}
		
		best = LMBLOCK_HEIGHT;

		for (i=0 ; i<LMBLOCK_WIDTH-w ; i++)
		{
			best2 = 0;

			for (j=0 ; j<w ; j++)
			{
				if (allocated[i+j] >= best)
					break;
				if (allocated[i+j] > best2)
					best2 = allocated[i+j];
			}
			if (j == w)
			{	// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > LMBLOCK_HEIGHT)
			continue;

		for (i=0 ; i<w ; i++)
			allocated[*x + i] = best + h;
		
		last_lightmap_allocated = texnum;
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
	float		s, t, s0, t0, sdiv, tdiv;
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

	if (surf->flags & SURF_DRAWTURB)
	{
		// match Mod_PolyForUnlitSurface
		s0 = t0 = 0.f;
		sdiv = tdiv = 128.f;
	}
	else
	{
		s0 = surf->texinfo->vecs[0][3];
		t0 = surf->texinfo->vecs[1][3];
		sdiv = surf->texinfo->texture->width;
		tdiv = surf->texinfo->texture->height;
	}
	
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
		s = DotProduct (vec, surf->texinfo->vecs[0]) + s0;
		s /= sdiv;

		t = DotProduct (vec, surf->texinfo->vecs[1]) + t0;
		t /= tdiv;

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
		s /= LMBLOCK_WIDTH*16; //surf->texinfo->texture->width;

		t = DotProduct (vec, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3];
		t -= surf->texturemins[1];
		t += surf->light_t*16;
		t += 8;
		t /= LMBLOCK_HEIGHT*16; //surf->texinfo->texture->height;

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

	if (surf->flags & SURF_DRAWTILED)
	{
		surf->lightmaptexture = -1;
		return;
	}
	
	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;

	surf->lightmaptexture = Lightmap_AllocBlock (smax, tmax, &surf->light_s, &surf->light_t);
	if (surf->lightmaptexture == -1)
		Sys_Error ("Lightmap_AllocBlock: full");

	base = lightmaps[surf->lightmaptexture].data;
	base += (surf->light_t * LMBLOCK_WIDTH + surf->light_s) * lightmap_bytes;
	R_BuildLightMap (surf, base, LMBLOCK_WIDTH*lightmap_bytes);
}


/*
==================
R_BuildLightmaps -- called at level load time

Builds the lightmap texture
with all the surfaces from all brush models
==================
*/
void R_BuildLightmaps (void)
{
	char	name[64];
	int		i, j;
	lightmap_t *lm;
	model_t	*m;
	
	r_framecount = 1;		// no dlightcache
	
	// Spike -- wipe out all the lightmap data (johnfitz -- the gltexture objects were already freed by Mod_ClearAll)
	for (i=0; i < lightmap_count; i++)
		free (lightmaps[i].data);
	free (lightmaps);
	lightmaps = NULL;
	last_lightmap_allocated = 0;
	lightmap_count = 0;
	
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
	for (i=0; i<lightmap_count; i++)
	{
		lm = &lightmaps[i];
		lm->modified = false;
		lm->rectchange.l = LMBLOCK_WIDTH;
		lm->rectchange.t = LMBLOCK_HEIGHT;
		lm->rectchange.w = 0;
		lm->rectchange.h = 0;
		
		sprintf(name, "lightmap%07i",i);
		
		lm->texture = TexMgr_LoadTexture (cl.worldmodel, name, LMBLOCK_WIDTH, LMBLOCK_HEIGHT, SRC_LIGHTMAP, lm->data, "", (uintptr_t)lm->data, TEXPREF_LINEAR | TEXPREF_NOPICMIP);
	}
	
	//johnfitz -- warn about exceeding old limits
	//GLQuake limit was 64 textures of 128x128. Estimate how many 128x128 textures we would need
	//given that we are using lightmap_count of LMBLOCK_WIDTH x LMBLOCK_HEIGHT
	i = lightmap_count * ((LMBLOCK_WIDTH / 128) * (LMBLOCK_HEIGHT / 128));
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
	msurface_t	*s;
	byte		*base;
    
	if (!cl.worldmodel) // is this the correct test?
		return;
    
	//for each surface in each model, rebuild lightmap with new scale
	for (i=1; i<MAX_MODELS; i++)
	{
		if (!(mod = cl.model_precache[i]))
			continue;
		s = &mod->surfaces[mod->firstmodelsurface];
		for (j=0; j<mod->nummodelsurfaces; j++, s++)
		{
			if (s->flags & SURF_DRAWTILED)
				continue;
			base = lightmaps[s->lightmaptexture].data;
			base += s->light_t * LMBLOCK_WIDTH * lightmap_bytes + s->light_s * lightmap_bytes;
			R_BuildLightMap (s, base, LMBLOCK_WIDTH*lightmap_bytes);
		}
	}
    
	//for each lightmap, upload it
	for (i=0; i<lightmap_count; i++)
	{
		GL_BindTexture (lightmaps[i].texture);
		glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, LMBLOCK_WIDTH, LMBLOCK_HEIGHT, GL_RGBA,
						 GL_UNSIGNED_BYTE, lightmaps[i].data);
	}
}

