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

// the lightmap texture data needs to be kept in
// main memory so texsubimage can update properly
byte		lightmaps[4*MAX_LIGHTMAPS*BLOCK_WIDTH*BLOCK_HEIGHT]; // (4)lightmap_bytes*MAX_LIGHTMAPS*BLOCK_WIDTH*BLOCK_HEIGHT

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
	float		cred, cgreen, cblue, brightness;
	unsigned	*bl;

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
		cred = cl_dlights[lnum].color[0] * 256.0f;
		cgreen = cl_dlights[lnum].color[1] * 256.0f;
		cblue = cl_dlights[lnum].color[2] * 256.0f;

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
					bl[0] += (int) (brightness * cred);
					bl[1] += (int) (brightness * cgreen);
					bl[2] += (int) (brightness * cblue);
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
				scale = d_lightstylevalue[surf->styles[maps]];
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
	bl = blocklights;
	for (i=0 ; i<tmax ; i++, dest += stride)
	{
		for (j=0 ; j<smax ; j++)
		{
			t = *bl++ >> 8;if (t > 255) t = 255;dest[0] = t;
			t = *bl++ >> 8;if (t > 255) t = 255;dest[1] = t;
			t = *bl++ >> 8;if (t > 255) t = 255;dest[2] = t;
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
			Sys_Error ("R_TextureAnimation: broken cycle");
		if (++count > 100)
			Sys_Error ("R_TextureAnimation: infinite cycle");
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
void R_RenderDynamicLightmaps (msurface_t *fa)
{
	byte		*base;
	int			maps;
	glRect_t    *theRect;
	int smax, tmax;

	if (fa->flags & SURF_DRAWTILED) // not a lightmapped surface
		return;

	// add to lightmap chain
	fa->polys->chain = lightmap_polys[fa->lightmaptexture];
	lightmap_polys[fa->lightmaptexture] = fa->polys;

	// check for lightmap modification
	for (maps = 0 ; maps < MAXLIGHTMAPS && fa->styles[maps] != 255 ; maps++)
		if (d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps])
			goto dynamic;

	if (fa->dlightframe == r_framecount	// dynamic this frame
		|| fa->cached_dlight)			// dynamic previously
	{
dynamic:
		if (r_dynamic.value && !r_fullbright.value)
		{
			lightmap_modified[fa->lightmaptexture] = true;
			theRect = &lightmap_rectchange[fa->lightmaptexture];

			if (fa->light_t < theRect->t) 
			{
				if (theRect->h)
					theRect->h += theRect->t - fa->light_t;
				theRect->t = fa->light_t;
			}
			if (fa->light_s < theRect->l) 
			{
				if (theRect->w)
					theRect->w += theRect->l - fa->light_s;
				theRect->l = fa->light_s;
			}

			smax = (fa->extents[0]>>4)+1;
			tmax = (fa->extents[1]>>4)+1;

			if ((theRect->w + theRect->l) < (fa->light_s + smax))
				theRect->w = (fa->light_s-theRect->l)+smax;
			if ((theRect->h + theRect->t) < (fa->light_t + tmax))
				theRect->h = (fa->light_t-theRect->t)+tmax;

			base = lightmaps + fa->lightmaptexture*lightmap_bytes*BLOCK_WIDTH*BLOCK_HEIGHT;
			base += fa->light_t * BLOCK_WIDTH * lightmap_bytes + fa->light_s * lightmap_bytes;
			R_BuildLightMap (fa, base, BLOCK_WIDTH*lightmap_bytes);
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
	glRect_t	*theRect;

	for (lmap = 0; lmap < MAX_LIGHTMAPS; lmap++)
	{
		if (!lightmap_modified[lmap])
			continue;

		GL_Bind (lightmap_textures[lmap]);
		lightmap_modified[lmap] = false;

		theRect = &lightmap_rectchange[lmap];

		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, BLOCK_WIDTH, theRect->h, GL_RGBA,
			GL_UNSIGNED_BYTE, lightmaps+(lmap* BLOCK_HEIGHT + theRect->t) *BLOCK_WIDTH*lightmap_bytes);

		theRect->l = BLOCK_WIDTH;
		theRect->t = BLOCK_HEIGHT;
		theRect->h = 0;
		theRect->w = 0;

		// r_speeds
		rs_c_dynamic_lightmaps++;
	}
}


/*
================
R_DrawSequentialPoly

Systems that have fast state and texture changes can
just do everything as it passes with no need to sort
================
*/
void R_DrawSequentialPoly (entity_t *e, msurface_t *s)
{
	glpoly_t	*p;
	texture_t	*t;
	float		*v;
	float		entalpha;
	int			i;

	p = s->polys;
	t = R_TextureAnimation (s->texinfo->texture, e->frame);
	entalpha = ENTALPHA_DECODE(e->alpha);

	//
	// sky poly
	//
	if (s->flags & SURF_DRAWSKY)
		return; // skip it, already handled

	//
	// water poly
	//
	if (s->flags & SURF_DRAWTURB)
		return; // skip it, render it later because wateralpha

	//
	// missing texture
	//
	if (s->flags & SURF_NOTEXTURE)
	{
		if (entalpha < 1.0)
		{
			glDepthMask(GL_FALSE);
			glEnable(GL_BLEND);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glColor4f(1, 1, 1, entalpha);
		}

		GL_Bind (t->gltexture);
		R_DrawGLPoly34 (p);
		rs_c_brush_passes++; // r_speeds

		if (entalpha < 1.0)
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
		if (entalpha < 1.0)
		{
			glDepthMask (GL_FALSE);
			glEnable (GL_BLEND);
			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glColor4f (1, 1, 1, entalpha);
		}
		else
			glColor3f (1, 1, 1);

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
			glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 2.0f);

			glBegin(GL_POLYGON);
			v = p->verts[0];
			for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
			{
				qglMultiTexCoord2f (TEXTURE0, v[3], v[4]);
				qglMultiTexCoord2f (TEXTURE1, v[5], v[6]);
				glVertex3fv (v);
			}
			glEnd ();
			rs_c_brush_passes++; // r_speeds

			glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 1.0f);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			GL_DisableMultitexture (); // selects TEXTURE0
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);//FX
		} 
		else if (entalpha < 1.0) // case 2: can't do multipass if entity has alpha, so just draw the texture
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

		if (entalpha < 1.0)
		{
			glDepthMask(GL_TRUE);
			glDisable(GL_BLEND);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			glColor3f(1, 1, 1);
		}

		if (t->fullbright)
		{
			GL_Bind (t->fullbright);
			glDepthMask (GL_FALSE); // don't bother writing Z
			glEnable (GL_BLEND);
			glBlendFunc (GL_ONE, GL_ONE);
			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glColor3f (entalpha, entalpha, entalpha);
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
================
R_DrawSequentialWaterPoly
================
*/
void R_DrawSequentialWaterPoly (entity_t *e, msurface_t *s)
{
	glpoly_t	*p;
//	texture_t	*t; // unused
	float		entalpha;
	float		entfog = 0; // keep compiler happy

	p = s->polys;
//	t = R_TextureAnimation (s->texinfo->texture, e->frame);
	entalpha = ENTALPHA_DECODE(e->alpha);

	//
	// water poly
	//
	if (s->flags & SURF_DRAWTURB)
	{
		if (e->alpha == ENTALPHA_DEFAULT)
		{
			if (!r_lockalpha.value) // override water alpha for certain surface types
			{
				if (s->flags & SURF_DRAWLAVA)
					entalpha = CLAMP(0.0, r_lavaalpha.value, 1.0);
				else if (s->flags & SURF_DRAWSLIME)
					entalpha = CLAMP(0.0, r_slimealpha.value, 1.0);
				else if (s->flags & SURF_DRAWTELE)
					entalpha = CLAMP(0.0, r_telealpha.value, 1.0);
			}
			else
				entalpha = 1.0f;

			if (s->flags & SURF_DRAWWATER)
			{
				if (globalwateralpha > 0)
					entalpha = globalwateralpha;
				else
					entalpha = CLAMP(0.0, r_wateralpha.value, 1.0);
			}
		}

		if (entalpha < 1.0)
		{
			glDepthMask(GL_FALSE);
			glEnable(GL_BLEND);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glColor4f(1, 1, 1, entalpha);
		}

		GL_Bind (s->texinfo->texture->warpimage);
		s->texinfo->texture->update_warp = true; // FIXME: one frame too late!

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
				entfog = CLAMP(0.0, r_lavafog.value, 1.0);
			else if (s->flags & SURF_DRAWSLIME)
				entfog = CLAMP(0.0, r_slimefog.value, 1.0);

			if (R_FogGetDensity() > 0 && entfog > 0)
			{
				float *c = R_FogGetColor();

				glEnable (GL_BLEND);
				glColor4f (c[0],c[1],c[2], entfog);
				R_DrawGLPoly34 (p);
				rs_c_brush_passes++; // r_speeds
				glColor3f (1, 1, 1);
				glDisable (GL_BLEND);
			}
		}

		if (entalpha < 1.0)
		{
			glDepthMask(GL_TRUE);
			glDisable(GL_BLEND);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			glColor3f(1, 1, 1);
		}
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
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModel (entity_t *e, qboolean water)
{
	int			k, i;
	msurface_t	*psurf;
	float		dot;
	mplane_t	*pplane;
	model_t		*clmodel;

	if (R_CullModelForEntity(e))
		return;

	clmodel = e->model;

	VectorSubtract (r_refdef.vieworg, e->origin, modelorg);
	if (e->angles[0] || e->angles[1] || e->angles[2])
	{
		vec3_t	temp;
		vec3_t	forward, right, up;

		VectorCopy (modelorg, temp);
		AngleVectors (e->angles, forward, right, up);
		modelorg[0] = DotProduct (temp, forward);
		modelorg[1] = -DotProduct (temp, right);
		modelorg[2] = DotProduct (temp, up);
	}

	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];

	// calculate dynamic lighting for bmodel if it's not an instanced model
	if (clmodel->firstmodelsurface != 0 /* && !gl_flashblend.value */) //FX -- commented out
	{
		for (k=0 ; k<MAX_DLIGHTS ; k++)
		{
			if ((cl_dlights[k].die < cl.time) ||
				(!cl_dlights[k].radius))
				continue;

			R_MarkLights (&cl_dlights[k], k,
				clmodel->nodes + clmodel->hulls[0].firstclipnode);
		}
	}

	/* MH: z-fighting is really a mapping problem, 
	and it should be fixed in the map and not by the engine. 
	
	The traditional "fix" (using polygon offset) sucks because (a) the z-buffer is non-linear 
	so offset factors will have different effects at different depths, 
	and (b) the OpenGL spec allows polygon offset to be implementation-dependent, 
	so the same offset factor may have different effects on different hardware. */
	if (gl_zfix.value) // z-fighting fix
	{
		glPolygonOffset (DIST_EPSILON, 0);
		glEnable (GL_POLYGON_OFFSET_FILL);
	}

	glPushMatrix ();

	glTranslatef (e->origin[0],  e->origin[1],  e->origin[2]);
	glRotatef (e->angles[1],  0, 0, 1);
	glRotatef (e->angles[0],  0, 1, 0);
	glRotatef (e->angles[2],  1, 0, 0);

	//
	// draw it
	//
	for (i=0 ; i<clmodel->nummodelsurfaces ; i++, psurf++)
	{
		// find which side of the node we are on
		pplane = psurf->plane;
		dot = DotProduct (modelorg, pplane->normal) - pplane->dist;

		// draw the polygon
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			if (water)
				R_DrawSequentialWaterPoly (e, psurf); // draw water entities
			else
				R_DrawSequentialPoly (e, psurf); // draw entities
			rs_c_brush_polys++; // r_speeds
		}
	}

	GL_DisableMultitexture (); // selects TEXTURE0

	glPopMatrix ();

	if (gl_zfix.value) // z-fighting fix
	{
		glPolygonOffset (0, 0);
		glDisable (GL_POLYGON_OFFSET_FILL);
	}
}

/*
=============================================================

	WORLD MODEL

=============================================================
*/


/*
================
R_DrawTextureChainsNoTexture

draws surfs whose textures were missing from the BSP
================
*/
void R_DrawTextureChainsNoTexture (void)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;
	qboolean	texbound;

	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
	{
		t = cl.worldmodel->textures[i];

		if (!t || !t->texturechain || !(t->texturechain->flags & SURF_NOTEXTURE))
			continue;

		texbound = false;

		for (s = t->texturechain; s; s = s->texturechain)
			if (!s->culled)
			{
				if (!texbound) // only bind once we are sure we need this texture
				{
					GL_Bind (t->gltexture);
					texbound = true;
				}

				R_DrawGLPoly34 (s->polys);
				rs_c_brush_passes++; // r_speeds
			}
	}
}

/*
================
R_DrawTextureChainsTextureOnly
================
*/
void R_DrawTextureChainsTextureOnly (void)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;
	qboolean	texbound;

	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
	{
		t = cl.worldmodel->textures[i];

		if (!t || !t->texturechain || t->texturechain->flags & (SURF_DRAWTURB | SURF_DRAWSKY))
			continue;

		texbound = false;

		for (s = t->texturechain; s; s = s->texturechain)
			if (!s->culled)
			{
				if (!texbound) // only bind once we are sure we need this texture
				{
					GL_Bind ((R_TextureAnimation(t,0))->gltexture);
					texbound = true;
				}

				R_RenderDynamicLightmaps (s); // adds to lightmap chain
				R_DrawGLPoly34 (s->polys);
				rs_c_brush_passes++; // r_speeds
			}
	}
}

/*
================
R_DrawTextureChainsWater
================
*/
void R_DrawTextureChainsWater (void)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;
	qboolean	texbound;
	float	wateralpha = 1.0f; // keep compiler happy
	float	lavafog = 0; // keep compiler happy

	if (!r_drawworld.value)
		return;

	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
	{
		t = cl.worldmodel->textures[i];

		if (!t || !t->texturechain || !(t->texturechain->flags & SURF_DRAWTURB))
			continue;

		texbound = false;

		for (s = t->texturechain; s; s = s->texturechain)
			if (!s->culled)
			{
				if (!r_lockalpha.value) // override water alpha for certain surface types
				{
					if (s->flags & SURF_DRAWLAVA)
						wateralpha = CLAMP(0.0, r_lavaalpha.value, 1.0);
					else if (s->flags & SURF_DRAWSLIME)
						wateralpha = CLAMP(0.0, r_slimealpha.value, 1.0);
					else if (s->flags & SURF_DRAWTELE)
						wateralpha = CLAMP(0.0, r_telealpha.value, 1.0);
				}
				else
					wateralpha = 1.0f;

				if (s->flags & SURF_DRAWWATER)
				{
					if (globalwateralpha > 0)
						wateralpha = globalwateralpha;
					else
						wateralpha = CLAMP(0.0, r_wateralpha.value, 1.0);
				}

				if (wateralpha < 1.0)
				{
					glDepthMask(GL_FALSE);
					glEnable (GL_BLEND);
					glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
					glColor4f (1, 1, 1, wateralpha);
				}

				if (!texbound) // only bind once we are sure we need this texture
				{
					GL_Bind (t->warpimage);
					texbound = true;
				}

				if ( !(s->flags & (SURF_DRAWLAVA | SURF_DRAWSLIME)) )
				{
					R_DrawGLPoly34 (s->polys);
					rs_c_brush_passes++; // r_speeds
				}
				else
				{
					R_FogDisableGFog ();
					R_DrawGLPoly34 (s->polys);
					rs_c_brush_passes++; // r_speeds
					R_FogEnableGFog ();

					if (s->flags & SURF_DRAWLAVA)
						lavafog = CLAMP(0.0, r_lavafog.value, 1.0);
					else if (s->flags & SURF_DRAWSLIME)
						lavafog = CLAMP(0.0, r_slimefog.value, 1.0);

					if (R_FogGetDensity() > 0 && lavafog > 0)
					{
						float *c = R_FogGetColor();

						glEnable (GL_BLEND);
						glColor4f (c[0],c[1],c[2], lavafog);
						R_DrawGLPoly34 (s->polys);
						rs_c_brush_passes++; // r_speeds
						glColor3f (1, 1, 1);
						glDisable (GL_BLEND);
					}
				}

				if (wateralpha < 1.0)
				{
					glDepthMask(GL_TRUE);
					glDisable (GL_BLEND);
					glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
					glColor3f (1, 1, 1);
				}
		}
	}
}

/*
================
R_DrawLightmapChains

stripped down to almost nothing
================
*/
void R_DrawLightmapChains (void)
{
	int			i;
	glpoly_t	*p;

	for (i=0 ; i<MAX_LIGHTMAPS ; i++)
	{
		if (!lightmap_polys[i])
			continue;

		GL_Bind (lightmap_textures[i]);

		for (p = lightmap_polys[i]; p; p=p->chain)
		{
			R_DrawGLPoly56 (p);
			rs_c_brush_passes++; // r_speeds
		}
	}
}

/*
================
R_DrawTextureChainsGlow
================
*/
void R_DrawTextureChainsGlow (void)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;
	gltexture_t	*glt;
	qboolean	texbound;

	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
	{
		t = cl.worldmodel->textures[i];

		if (!t || !t->texturechain || !(glt = R_TextureAnimation(t,0)->fullbright))
			continue;

		texbound = false;

		for (s = t->texturechain; s; s = s->texturechain)
			if (!s->culled)
			{
				if (!texbound) // only bind once we are sure we need this texture
				{
					GL_Bind (glt);
					texbound = true;
				}

				R_DrawGLPoly34 (s->polys);
				rs_c_brush_passes++; // r_speeds
			}
	}
}


/*
================
R_DrawTextureChainsMultitexture
================
*/
void R_DrawTextureChainsMultitexture (void)
{
	int			i, j;
	msurface_t	*s;
	texture_t	*t;
	float		*v;
	qboolean	texbound;

	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
	{
		t = cl.worldmodel->textures[i];

		if (!t || !t->texturechain || t->texturechain->flags & (SURF_DRAWTILED | SURF_NOTEXTURE))
			continue;

		texbound = false;

		for (s = t->texturechain; s; s = s->texturechain)
			if (!s->culled)
			{
				if (!texbound) // only bind once we are sure we need this texture
				{
					GL_Bind ((R_TextureAnimation(t,0))->gltexture);
					GL_EnableMultitexture(); // selects TEXTURE1
					texbound = true;
				}

				GL_Bind (lightmap_textures[s->lightmaptexture]);
				R_RenderDynamicLightmaps (s);

				glBegin(GL_POLYGON);
				v = s->polys->verts[0];
				for (j=0 ; j<s->polys->numverts ; j++, v+= VERTEXSIZE)
				{
					qglMultiTexCoord2f (TEXTURE0, v[3], v[4]);
					qglMultiTexCoord2f (TEXTURE1, v[5], v[6]);
					glVertex3fv (v);
				}
				glEnd ();
				rs_c_brush_passes++; // r_speeds
			}

		GL_DisableMultitexture(); // selects TEXTURE0
	}
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

	R_UploadLightmaps ();

	R_DrawTextureChainsNoTexture (); 

	if (gl_mtexable && gl_texture_env_combine)
	{
		GL_EnableMultitexture ();
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_PREVIOUS_EXT);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_TEXTURE);
		glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 2.0f);
		GL_DisableMultitexture ();
		R_DrawTextureChainsMultitexture ();
		GL_EnableMultitexture ();
		glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 1.0f);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		GL_DisableMultitexture ();
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	}
	else
	{
		//to make fog work with multipass lightmapping, need to do one pass
		//with no fog, one modulate pass with black fog, and one additive
		//pass with black geometry and normal fog
		R_FogDisableGFog ();
		R_DrawTextureChainsTextureOnly ();
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
			R_DrawTextureChainsTextureOnly ();
			glColor3f(1,1,1);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		}
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable (GL_BLEND);
		glDepthMask (GL_TRUE);
	}

	// fullbrights
	glDepthMask (GL_FALSE);
	glEnable (GL_BLEND);
	glBlendFunc (GL_ONE, GL_ONE);
	R_FogStartAdditive ();
	R_DrawTextureChainsGlow ();
	R_FogStopAdditive ();
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable (GL_BLEND);
	glDepthMask (GL_TRUE);
}


/*
===============
R_MarkSurfaces

mark surfaces based on PVS and rebuild texture chains
===============
*/
void R_MarkSurfaces (void)
{
	byte	*vis;
	mleaf_t		*leaf;
	mnode_t	*node;
	msurface_t	*surf, **mark;
	int			i, j;
	byte		solid[MAX_MAP_LEAFS / 2];
	qboolean	nearwaterportal = false;

	// clear lightmap chains
	memset (lightmap_polys, 0, sizeof(lightmap_polys));

	// Check if near water to avoid HOMs when crossing the surface
	// TODO: loop through all water surfs and use distance to leaf cullbox
	for (i=0, mark = r_viewleaf->firstmarksurface; i < r_viewleaf->nummarksurfaces; i++, mark++)
	{
		if ((*mark)->flags & SURF_DRAWTURB)
		{
			nearwaterportal = true;
//			Con_SafePrintf ("R_MarkSurfaces: nearwaterportal, surfs=%d\n", r_viewleaf->nummarksurfaces);
			break;
		}
	}

	// choose vis data
	if (r_novis.value || r_viewleaf->contents == CONTENTS_SOLID || r_viewleaf->contents == CONTENTS_SKY)
	{
		vis = solid;
		memset (solid, 0xff, (cl.worldmodel->numleafs+7)>>3);
	}
	else if (nearwaterportal)
		vis = SV_FatPVS (r_origin, cl.worldmodel);
	else
		vis = Mod_LeafPVS (r_viewleaf, cl.worldmodel);

	// if surface chains don't need regenerating, just add static entities and return
	if (r_oldviewleaf == r_viewleaf && !r_novis.value && !nearwaterportal)
	{
		leaf = &cl.worldmodel->leafs[1];
		for (i=0 ; i<cl.worldmodel->numleafs ; i++, leaf++)
			if (vis[i>>3] & (1<<(i&7)))
				if (leaf->efrags)
					R_StoreEfrags (&leaf->efrags);
		return;
	}

	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	// iterate through leaves, marking surfaces
	leaf = &cl.worldmodel->leafs[1];
	for (i=0 ; i<cl.worldmodel->numleafs ; i++, leaf++)
	{
		if (vis[i>>3] & (1<<(i&7)))
		{
			if (r_novis.value || leaf->contents != CONTENTS_SKY)
				for (j=0, mark = leaf->firstmarksurface; j<leaf->nummarksurfaces; j++, mark++)
					(*mark)->visframe = r_visframecount;

			// add static models
			if (leaf->efrags)
				R_StoreEfrags (&leaf->efrags);
		}
	}

	// set all chains to null
	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
		if (cl.worldmodel->textures[i])
			cl.worldmodel->textures[i]->texturechain = NULL;

	// rebuild chains

	//iterate through surfaces one node at a time to rebuild chains
	//need to do it this way if we want to work with tyrann's skip removal tool
	//because his tool doesn't actually remove the surfaces from the bsp surfaces lump
	//nor does it remove references to them in each leaf's marksurfaces list
	for (i=0, node = cl.worldmodel->nodes ; i<cl.worldmodel->numnodes ; i++, node++)
		for (j=0, surf=&cl.worldmodel->surfaces[node->firstsurface] ; j<node->numsurfaces ; j++, surf++)
			if (surf->visframe == r_visframecount)
			{
				surf->texturechain = surf->texinfo->texture->texturechain;
				surf->texinfo->texture->texturechain = surf;
			}

	//the old way
/*	surf = &cl.worldmodel->surfaces[cl.worldmodel->firstmodelsurface];
	for (i=0 ; i<cl.worldmodel->nummodelsurfaces ; i++, surf++)
	{
		if (surf->visframe == r_visframecount)
		{
			surf->texturechain = surf->texinfo->texture->texturechain;
			surf->texinfo->texture->texturechain = surf;
		}
	}
*/
}

/*
================
R_BackFaceCull

returns true if the surface is facing away from vieworg
================
*/
qboolean R_BackFaceCull (msurface_t *surf)
{
	double dot;

	switch (surf->plane->type)
	{
		case PLANE_X:
			dot = r_refdef.vieworg[0] - surf->plane->dist;
			break;
		case PLANE_Y:
			dot = r_refdef.vieworg[1] - surf->plane->dist;
			break;
		case PLANE_Z:
			dot = r_refdef.vieworg[2] - surf->plane->dist;
			break;
		default:
			dot = DotProduct (r_refdef.vieworg, surf->plane->normal) - surf->plane->dist;
			break;
	}

	if ((dot < 0) ^ !!(surf->flags & SURF_PLANEBACK))
		return true;

	return false;
}

/*
================
R_CullSurfaces
================
*/
void R_CullSurfaces (void)
{
	msurface_t *s;
	int i;

	if (!r_drawworld.value)
		return;

	s = &cl.worldmodel->surfaces[cl.worldmodel->firstmodelsurface];
	for (i=0 ; i<cl.worldmodel->nummodelsurfaces ; i++, s++)
	{
		if (s->visframe == r_visframecount)
		{
			if (R_CullBox(s->mins, s->maxs) || R_BackFaceCull (s))
				s->culled = true;
			else
			{
				s->culled = false;
				rs_c_brush_polys++; // r_speeds (count wpolys here)
				if (s->texinfo->texture->warpimage)
					s->texinfo->texture->update_warp = true;
			}
		}
	}
}


/*
=============================================================================

  LIGHTMAP ALLOCATION

=============================================================================
*/

// returns a texture number and the position inside it
int Lightmap_AllocBlock (int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;
	int		texnum;

	for (texnum=0 ; texnum<MAX_LIGHTMAPS ; texnum++)
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

void R_BuildSurfaceDisplayList (msurface_t *fa)
{
	int			i, lindex, lnumverts;
	medge_t		*pedges, *r_pedge;
	float		*vec;
	float		s, t;
	glpoly_t	*poly;

// reconstruct the polygon
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;

	//
	// draw texture
	//
	poly = Hunk_AllocName (sizeof(glpoly_t) + (lnumverts-4) * VERTEXSIZE*sizeof(float), "poly");
	poly->next = fa->polys;

	fa->polys = poly;
	poly->numverts = lnumverts;

	for (i=0 ; i<lnumverts ; i++)
	{
		lindex = currentmodel->surfedges[fa->firstedge + i];

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
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= fa->texinfo->texture->width;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= fa->texinfo->texture->height;

		VectorCopy (vec, poly->verts[i]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		//
		// lightmap texture coordinates
		//
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s -= fa->texturemins[0];
		s += fa->light_s*16;
		s += 8;
		s /= BLOCK_WIDTH*16; //fa->texinfo->texture->width;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t -= fa->texturemins[1];
		t += fa->light_t*16;
		t += 8;
		t /= BLOCK_HEIGHT*16; //fa->texinfo->texture->height;

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

		lightmap_textures[i] = GL_LoadTexture (cl.worldmodel, name, BLOCK_WIDTH, BLOCK_HEIGHT, SRC_LIGHTMAP, data, "", (unsigned)data, TEXPREF_LINEAR | TEXPREF_NOPICMIP);
	}

	// old limit warning
	if (i > 64)
		Con_DWarning ("R_BuildLightmaps: lightmaps exceeds standard limit (%d, normal max = %d)\n", i, 64);
}

