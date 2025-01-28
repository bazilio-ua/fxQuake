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
// gl_light.c

#include "quakedef.h"

int	r_dlightframecount;


/*
==================
R_AnimateLight

Added lightstyle interpolation
==================
*/
void R_AnimateLight (void)
{
	int		j, k, flight, clight;
	float	l, lerpfrac, backlerp;

	if (!r_dynamic.value) // EER1
	{
		// set everything to normal 'm' light
		for (j = 0 ; j < MAX_LIGHTSTYLES ; j++)
			d_lightstyle[j] = 264;
	}
	else
	{
		// light animations
		// 'm' is normal light, 'a' is no light, 'z' is double bright
		flight = (int)floor(cl.time * 10);
		clight = (int)ceil(cl.time * 10);
		lerpfrac = (cl.time * 10) - flight;
		backlerp = 1.0 - lerpfrac;
	
		for (j = 0 ; j < MAX_LIGHTSTYLES ; j++)
		{
			if (!cl_lightstyle[j].length)
			{	// was 256, changed to 264 for consistency
				d_lightstyle[j] = 264;
				continue;
			}
			else if (cl_lightstyle[j].length == 1)
			{	// single length style so don't bother interpolating
				d_lightstyle[j] = 22 * (cl_lightstyle[j].map[0] - 'a');
				continue;
			}
	
			// interpolate animating light
			// frame just gone
			k = flight % cl_lightstyle[j].length;
			k = cl_lightstyle[j].map[k] - 'a';
			l = (float)(k * 22) * backlerp;
	
			// upcoming frame
			k = clight % cl_lightstyle[j].length;
			k = cl_lightstyle[j].map[k] - 'a';
			l += (float)(k * 22) * lerpfrac;
	
			d_lightstyle[j] = (int)l;
		}
	}
}

/*
=============================================================================

DYNAMIC LIGHTS BLEND RENDERING

=============================================================================
*/

void V_AddLightBlend (float r, float g, float b, float a2)
{
	float	a;

	v_blend[3] = a = v_blend[3] + a2*(1-v_blend[3]);

	a2 = a2/a;

	v_blend[0] = v_blend[0]*(1-a2) + r*a2; // was [1]
	v_blend[1] = v_blend[1]*(1-a2) + g*a2;
	v_blend[2] = v_blend[2]*(1-a2) + b*a2;
}

// sin and cos tables from 0 to 1 in 0.0625 increments to speed up glow rendering
float bubble_sintable[17] = 
{
		0.000000,
		0.382684,
		0.707107,
		0.923880,
		1.000000,
		0.923879,
		0.707105,
		0.382680,
		-0.000004,
		-0.382687,
		-0.707110,
		-0.923882,
		-1.000000,
		-0.923877,
		-0.707102,
		-0.382677,
		0.000008,
};

float bubble_costable[17] = 
{
		1.000000,
		0.923879,
		0.707106,
		0.382682,
		-0.000002,
		-0.382686,
		-0.707109,
		-0.923881,
		-1.000000,
		-0.923878,
		-0.707103,
		-0.382678,
		0.000006,
		0.382689,
		0.707112,
		0.923882,
		1.000000,
};

/*
=============
R_RenderDlight

=============
*/
//vec3_t	bubblecolor = {1.0, 0.5, 0.0};
void R_RenderDlight (dlight_t *light)
{
	int		i, j;
	vec3_t	v;
	vec3_t	color;
	float	rad;

	if (!gl_flashblend.value)
		return;
	
	
	
//	VectorCopy (light->colored ? light->color : bubblecolor, color);
	VectorCopy (light->color, color);
	rad = light->radius * 0.1 * CLAMP(1.0, gl_flashblendscale.value, 16.0); // (orig. 0.35) reduce the bubble size so that it coexists more peacefully with proper light
	
	VectorSubtract (light->origin, r_origin, v);
	if (VectorLength (v) < rad)
	{	// view is inside the dlight
		if (gl_flashblendview.value)
			V_AddLightBlend (color[0], color[1], color[2], light->radius * 0.0003);
		return;
	}
	
	
	
	glDepthMask (GL_FALSE); // don't bother writing Z	
	glDisable (GL_TEXTURE_2D);
	glShadeModel (GL_SMOOTH);
	glEnable (GL_BLEND);
	//glBlendFunc (GL_ONE, GL_ONE); // orig.
	glBlendFunc (GL_SRC_ALPHA, GL_ONE); // ver.2 attempt to make it more smooth
	
	R_FogDisableGFog ();
	
	
	glBegin (GL_TRIANGLE_FAN);
	VectorScale (color, 0.2, color);
	glColor3fv (color);
	for (i=0 ; i<3 ; i++)
		v[i] = light->origin[i] - vpn[i]*rad;
	glVertex3fv (v);
	glColor3f (0,0,0);
	for (i=16 ; i>=0 ; i--)
	{
		for (j=0 ; j<3 ; j++)
			v[j] = light->origin[j] + vright[j] * bubble_costable[i] * rad + vup[j] * bubble_sintable[i] * rad;
		glVertex3fv (v);
	}
	glEnd ();
	
	
	R_FogEnableGFog ();
	
	glColor3f (1,1,1);
	glDisable (GL_BLEND);
	glEnable (GL_TEXTURE_2D);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask (GL_TRUE); // back to normal Z buffering
	
	
}

/*
=============
R_SetupDlights

flash blend dlights
=============
*/
void R_SetupDlights (void)
{
	int		i;
	dlight_t	*l;

	if (!gl_flashblend.value)
		return;

	r_dlightframecount = r_framecount + 1;	// because the count hasn't advanced yet for this frame
	
	for (i=0, l = cl_dlights ; i<MAX_DLIGHTS ; i++, l++)
	{
		if (l->die < cl.time || !l->radius)
			continue;
		
		if (R_CullSphere (l->origin, l->radius))
			continue;
		
		R_AddToAlpha (ALPHA_DLIGHTS, R_GetAlphaDist(l->origin), l, NULL, NULL, 0);
	}
}


/*
=============================================================================

DYNAMIC LIGHTS

=============================================================================
*/

/*
=============
R_MarkLights

rewritten to use LordHavoc's lighting speedup
recursive goes through the nodes marking the surfaces near the dynamic light as lit
=============
*/
void R_MarkLights (dlight_t *light, int num, mnode_t *node)
{
	mplane_t	*plane;
	msurface_t	*surf;
	vec3_t		impact;
	float		dist, l, maxdist;
	int			i, j, s, t;

restart:
	if (node->contents < 0)
		return;

// node is just a decision point, so go down the apropriate sides

// find which side of the node we are on
	plane = node->plane;

	switch (plane->type)
	{
	case PLANE_X:
	case PLANE_Y:
	case PLANE_Z:
		dist = light->origin[plane->type] - plane->dist;
		break;
	default:
		dist = DotProduct (light->origin, plane->normal) - plane->dist;
		break;
	}

	if (dist > light->radius)
	{
		node = node->children[0];
		goto restart;
	}
	if (dist < -light->radius)
	{
		node = node->children[1];
		goto restart;
	}

	maxdist = light->radius*light->radius;
// mark the polygons
	surf = cl.worldmodel->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		if (surf->flags & SURF_DRAWTILED) // no lights on these
			continue;

		for (j=0 ; j<3 ; j++)
			impact[j] = light->origin[j] - surf->plane->normal[j]*dist;

		// clamp center of light to corner and check brightness
		l = DotProduct (impact, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3] - surf->texturemins[0];
		s = l + 0.5;
		s = CLAMP (0, s, surf->extents[0]);
		s = l - s;

		l = DotProduct (impact, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3] - surf->texturemins[1];
		t = l + 0.5;
		t = CLAMP (0, t, surf->extents[1]);
		t = l - t;

		// compare to minimum light
		if ((s*s+t*t+dist*dist) < maxdist)
		{
			if (surf->dlightframe != r_dlightframecount) // not dynamic until now
			{
				memset (surf->dlightbits, 0, sizeof(surf->dlightbits));
				surf->dlightbits[num >> 5] = 1U << (num & 31);
				surf->dlightframe = r_dlightframecount;
			}
			else // already dynamic
				surf->dlightbits[num >> 5] |= 1U << (num & 31);
		}
	}

	if (node->children[0]->contents >= 0)
		R_MarkLights (light, num, node->children[0]);
	if (node->children[1]->contents >= 0)
		R_MarkLights (light, num, node->children[1]);
}

/*
=============
R_PushDlights
=============
*/
void R_PushDlights (void)
{
	int		i;
	dlight_t	*l;
	
	if (!r_dynamic.value) // EER1
		return;
	
	r_dlightframecount = r_framecount + 1;	// because the count hasn't advanced yet for this frame

	for (i=0, l = cl_dlights ; i<MAX_DLIGHTS ; i++, l++)
	{
		if (l->die < cl.time || !l->radius)
			continue;
		
		if (R_CullSphere (l->origin, l->radius))
			continue;
		
		R_MarkLights (l, i, cl.worldmodel->nodes);
	}
}


/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

mplane_t		*lightplane;
vec3_t			lightspot;
vec3_t			lightcolor; // lit support via lordhavoc

/*
=============
R_RecursiveLightPoint

replaced entire function for lit support via lordhavoc
=============
*/
qboolean R_RecursiveLightPoint (vec3_t color, mnode_t *node, vec3_t start, vec3_t end)
{
	mplane_t	*plane;
	int			side;
	float		front, back, frac;
	vec3_t		mid;

restart:
	// check for a hit
	if (node->contents < 0)
		return false;		// didn't hit anything

// calculate mid point
// node is just a decision point, so go down the apropriate sides

// find which side of the node we are on
	plane = node->plane;

	switch (plane->type)
	{
	case PLANE_X:
	case PLANE_Y:
	case PLANE_Z:
		front = start[plane->type] - plane->dist;
		back = end[plane->type] - plane->dist;
		break;
	default:
		front = DotProduct(start, plane->normal) - plane->dist;
		back = DotProduct(end, plane->normal) - plane->dist;
		break;
	}

	side = front < 0;

	// LordHavoc: optimized recursion
	// completely on one side - tail recursion optimization
	if ((back < 0) == side)
	{
		node = node->children[side];
		goto restart;
	}

	frac = front / (front-back);
	mid[0] = start[0] + (end[0] - start[0])*frac;
	mid[1] = start[1] + (end[1] - start[1])*frac;
	mid[2] = start[2] + (end[2] - start[2])*frac;

// go down front side
	if (R_RecursiveLightPoint (color, node->children[side], start, mid))
		return true;	// hit something
	else
	{
		int i, ds, dt;
		msurface_t *surf;
		mtexinfo_t	*tex;

	// check for impact on this node
		VectorCopy (mid, lightspot);
		lightplane = node->plane;

		surf = cl.worldmodel->surfaces + node->firstsurface;
		for (i=0 ; i<node->numsurfaces ; i++, surf++)
		{
			if (surf->flags & SURF_DRAWTILED)
				continue;	// no lightmaps

			tex = surf->texinfo;
			
			ds = DotProduct (mid, tex->vecs[0]) + tex->vecs[0][3];
			dt = DotProduct (mid, tex->vecs[1]) + tex->vecs[1][3];

			if (ds < surf->texturemins[0] || dt < surf->texturemins[1])
				continue;	// out of range

			ds -= surf->texturemins[0];
			dt -= surf->texturemins[1];

			if (ds > surf->extents[0] || dt > surf->extents[1])
				continue;	// out of range

			if (surf->samples)
			{
				// LordHavoc: enhanced to interpolate lighting
				byte *lightmap;
				int maps, line3, dsfrac = ds & 15, dtfrac = dt & 15, r00 = 0, g00 = 0, b00 = 0, r01 = 0, g01 = 0, b01 = 0, r10 = 0, g10 = 0, b10 = 0, r11 = 0, g11 = 0, b11 = 0;
				float scale;
				line3 = ((surf->extents[0]>>4)+1)*3;

				lightmap = surf->samples + ((dt>>4) * ((surf->extents[0]>>4)+1) + (ds>>4))*3; // LordHavoc: *3 for color

				for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ; maps++)
				{
					scale = (float) d_lightstyle[surf->styles[maps]] * 1.0 / 256.0;
					r00 += (float) lightmap[      0] * scale;g00 += (float) lightmap[      1] * scale;b00 += (float) lightmap[2] * scale;
					r01 += (float) lightmap[      3] * scale;g01 += (float) lightmap[      4] * scale;b01 += (float) lightmap[5] * scale;
					r10 += (float) lightmap[line3+0] * scale;g10 += (float) lightmap[line3+1] * scale;b10 += (float) lightmap[line3+2] * scale;
					r11 += (float) lightmap[line3+3] * scale;g11 += (float) lightmap[line3+4] * scale;b11 += (float) lightmap[line3+5] * scale;
					lightmap += ((surf->extents[0]>>4)+1) * ((surf->extents[1]>>4)+1)*3; // LordHavoc: *3 for colored lighting
				}

				color[0] += (float) ((int) ((((((((r11-r10) * dsfrac) >> 4) + r10)-((((r01-r00) * dsfrac) >> 4) + r00)) * dtfrac) >> 4) + ((((r01-r00) * dsfrac) >> 4) + r00)));
				color[1] += (float) ((int) ((((((((g11-g10) * dsfrac) >> 4) + g10)-((((g01-g00) * dsfrac) >> 4) + g00)) * dtfrac) >> 4) + ((((g01-g00) * dsfrac) >> 4) + g00)));
				color[2] += (float) ((int) ((((((((b11-b10) * dsfrac) >> 4) + b10)-((((b01-b00) * dsfrac) >> 4) + b00)) * dtfrac) >> 4) + ((((b01-b00) * dsfrac) >> 4) + b00)));
			}
			return true; // success
		}

	// go down back side
		return R_RecursiveLightPoint (color, node->children[!side], mid, end);
	}
}

/*
=============
R_LightPoint

replaced entire function for lit support via lordhavoc
=============
*/
void R_LightPoint (vec3_t p, vec3_t color)
{
	vec3_t		end;

	if (r_fullbright.value || !cl.worldmodel->lightdata)
	{
		color[0] = color[1] = color[2] = 255;
		return;
	}

	// set end point (back to 2048 for less BSP tree tracing)
	end[0] = p[0];
	end[1] = p[1];
	end[2] = p[2] - 8192; // was 2048

	color[0] = color[1] = color[2] = max(0, r_ambient.value);
	R_RecursiveLightPoint (color, cl.worldmodel->nodes, p, end);
}
