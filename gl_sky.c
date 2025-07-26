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
// gl_sky.c -- handle sky routines, cloud layers and skybox

#include "quakedef.h"


cvar_t	r_fastsky = {"r_fastsky","0", CVAR_NONE};
cvar_t	r_fastskycolor = {"r_fastskycolor", "", CVAR_ARCHIVE}; // woods #fastskycolor
cvar_t	r_skyquality = {"r_skyquality","12", CVAR_NONE};
cvar_t	r_skyalpha = {"r_skyalpha","1", CVAR_NONE};
cvar_t	r_skyfog = {"r_skyfog","0.5", CVAR_NONE};
cvar_t	r_oldsky = {"r_oldsky", "0", CVAR_NONE};

/*
==============================================================================

	SKY DRAW

==============================================================================
*/

// skybox
vec3_t	skyclip[6] = 
{
	{1,1,0},
	{1,-1,0},
	{0,-1,1},
	{0,1,1},
	{1,0,1},
	{-1,0,1}
};

int	st_to_vec[6][3] = 
{
	{3,-1,2},
	{-3,1,2},
	{1,3,2},
	{-1,-3,2},
 	{-2,-1,3},		// 0 degrees yaw, look straight up
 	{2,-1,-3}		// look straight down
};

int	vec_to_st[6][3] = 
{
	{-2,3,1},
	{2,3,-1},
	{1,3,2},
	{-1,3,-2},
	{-2,-1,3},
	{-2,1,-3}
};

float	skymins[2][6], skymaxs[2][6];
int		skytexorder[6] = {0,2,1,3,4,5}; // for skybox

float	squarerootof3;

/*
=============================================================

	RENDER SKYBOX

=============================================================
*/

gltexture_t		*skyboxtextures[6];

qboolean	oldsky;
char	skybox_name[MAX_OSPATH] = ""; // name of current skybox, or "" if no skybox
float	skyfog; // ericw
// 3dstudio environment map names
char	*suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"}; // for skybox

/*
==================
R_LoadSkyBox
==================
*/
void R_LoadSkyBox (char *skybox)
{
	int		i, width, height;
	int		mark;
	char	name[MAX_OSPATH];
	byte	*data;
	qboolean	nonefound = true;

	if (!strcmp(skybox_name, skybox)) 
		return; // no change

	// purge old textures
	for (i = 0; i < 6; i++)
	{
		if (skyboxtextures[i] && skyboxtextures[i] != notexture)
			TexMgr_FreeTexture (skyboxtextures[i]);
		skyboxtextures[i] = NULL;
	}

	// turn off skybox if sky is set to ""
	if (skybox[0] == 0)
	{
		skybox_name[0] = 0;
		oldsky = (skybox_name[0] == 0);
		Con_DPrintf ("skybox set to \"\"\n");
		return;
	}

	// load textures
	for (i = 0 ; i < 6 ; i++)
	{
		mark = Hunk_LowMark ();
		
		sprintf (name, "gfx/env/%s%s", skybox, suf[i]);
		data = Image_LoadImage (name, &width, &height);
		if (data)
		{
			skyboxtextures[i] = TexMgr_LoadTexture (cl.worldmodel, name, width, height, SRC_RGBA, data, name, 0, TEXPREF_SKY);
			nonefound = false;
		}
		else
		{
			Con_Printf ("Couldn't load %s\n", name);
			skyboxtextures[i] = notexture;
		}
		
		Hunk_FreeToLowMark (mark);
	}
	
	if (nonefound) // go back to scrolling sky if skybox is totally missing
	{
		for (i = 0; i < 6; i++)
		{
			if (skyboxtextures[i] && skyboxtextures[i] != notexture)
				TexMgr_FreeTexture (skyboxtextures[i]);
			skyboxtextures[i] = NULL;
		}
		skybox_name[0] = 0;
	}
	else
	{
		strcpy (skybox_name, skybox);
		Con_DPrintf ("skybox set to %s\n", skybox);
	}

	// enable/disable skybox
	oldsky = (skybox_name[0] == 0);
}

/*
=================
R_Sky_f
=================
*/
void R_Sky_f (void)
{
	switch (Cmd_Argc())
	{
	case 1:
		Con_Printf("\"sky\" is \"%s\"\n", skybox_name);
		break;
	case 2:
		R_LoadSkyBox(Cmd_Argv(1));
		break;
	default:
		Con_Printf("usage: sky <skyname>\n");
	}
}

/*
==============
R_SkyEmitSkyBoxVertex
==============
*/
void R_SkyEmitSkyBoxVertex (float s, float t, int axis)
{
	vec3_t		v, b;
	int			j, k;
	float		w, h;

	b[0] = s * gl_farclip.value / squarerootof3;
	b[1] = t * gl_farclip.value / squarerootof3;
	b[2] = gl_farclip.value / squarerootof3;

	for (j=0 ; j<3 ; j++)
	{
		k = st_to_vec[axis][j];
		if (k < 0)
			v[j] = -b[-k - 1];
		else
			v[j] = b[k - 1];
		v[j] += r_origin[j];
	}

	// convert from range [-1,1] to [0,1]
	s = (s+1)*0.5;
	t = (t+1)*0.5;

	// avoid bilerp seam
	w = skyboxtextures[skytexorder[axis]]->width;
	h = skyboxtextures[skytexorder[axis]]->height;

	s = s * (w-1)/w + 0.5/w;
	t = t * (h-1)/h + 0.5/h;

	t = 1.0 - t;

	glTexCoord2f (s, t);
	glVertex3fv (v);
}

/*
==============
R_SkyDrawSkyBox

FIXME: eliminate cracks by adding an extra vert on tjuncs
==============
*/
void R_SkyDrawSkyBox (void)
{
	int		i;

	for (i=0 ; i<6 ; i++)
	{
		if (skymins[0][i] >= skymaxs[0][i] || skymins[1][i] >= skymaxs[1][i])
			continue;

		GL_BindTexture (skyboxtextures[skytexorder[i]]);

//FIXME: this is to avoid tjunctions until i can do it the right way
		skymins[0][i] = -1;
		skymins[1][i] = -1;
		skymaxs[0][i] = 1;
		skymaxs[1][i] = 1;

		glBegin (GL_QUADS);
		R_SkyEmitSkyBoxVertex (skymins[0][i], skymins[1][i], i);
		R_SkyEmitSkyBoxVertex (skymins[0][i], skymaxs[1][i], i);
		R_SkyEmitSkyBoxVertex (skymaxs[0][i], skymaxs[1][i], i);
		R_SkyEmitSkyBoxVertex (skymaxs[0][i], skymins[1][i], i);
		glEnd ();

		// r_speeds
		rs_c_sky_polys++;
		rs_c_sky_passes++;

		if (R_FogGetDensity() > 0 && skyfog > 0)
		{
			float *c = R_FogGetColor();

			glEnable (GL_BLEND);
			glDisable (GL_TEXTURE_2D);
			glColor4f (c[0],c[1],c[2], skyfog);

			glBegin (GL_QUADS);
			R_SkyEmitSkyBoxVertex (skymins[0][i], skymins[1][i], i);
			R_SkyEmitSkyBoxVertex (skymins[0][i], skymaxs[1][i], i);
			R_SkyEmitSkyBoxVertex (skymaxs[0][i], skymaxs[1][i], i);
			R_SkyEmitSkyBoxVertex (skymaxs[0][i], skymins[1][i], i);
			glEnd ();

			glColor3f (1, 1, 1);
			glEnable (GL_TEXTURE_2D);
			glDisable (GL_BLEND);

			// r_speeds
			rs_c_sky_passes++;
		}
	}
}

/*
=============================================================

	RENDER CLOUDS

=============================================================
*/

gltexture_t		*solidskytexture, *alphaskytexture;

#define	MAX_CLIP_VERTS 256 // was 64

float	skyflatcolor[3];
byte	*skydata;

/*
====================
R_FastSkyColor
====================
*/
void R_FastSkyColor (void)
{
	int			i, j, p, r, g, b, count;
	unsigned	*rgba;
	byte *rgb;
	
	if (r_fastskycolor.string[0] != 0)
	{
	// update skycolor
		rgb = (byte *)(d_8to24table + ((int)r_fastskycolor.value & 0xFF));
		skyflatcolor[0] = (float)rgb[0]/255.0;
		skyflatcolor[1] = (float)rgb[1]/255.0;
		skyflatcolor[2] = (float)rgb[2]/255.0;
	}
	else
	if (skydata)
	{
	// calculate r_fastsky color based on average of all opaque foreground colors
		r = g = b = count = 0;
		for (i=0 ; i<128 ; i++)
		{
			for (j=0 ; j<128 ; j++)
			{
				p = skydata[i*256 + j];
				if (p != 0)
				{
					rgba = &d_8to24table[p];
					r += ((byte *)rgba)[0];
					g += ((byte *)rgba)[1];
					b += ((byte *)rgba)[2];
					count++;
				}
			}
		}
		
		skyflatcolor[0] = (float)r/(count*255);
		skyflatcolor[1] = (float)g/(count*255);
		skyflatcolor[2] = (float)b/(count*255);
	}
}

/*
====================
R_Skyfog -- ericw
====================
*/
void R_Skyfog (void)
{
// clear any skyfog setting from worldspawn
	skyfog = CLAMP(0.0, r_skyfog.value, 1.0);
}

/*
==============
R_SkySetBoxVert
==============
*/
void R_SkySetBoxVert (float s, float t, int axis, vec3_t v)
{
	vec3_t		b;
	int			j, k;

	b[0] = s * gl_farclip.value / squarerootof3;
	b[1] = t * gl_farclip.value / squarerootof3;
	b[2] = gl_farclip.value / squarerootof3;

	for (j=0 ; j<3 ; j++)
	{
		k = st_to_vec[axis][j];
		if (k < 0)
			v[j] = -b[-k - 1];
		else
			v[j] = b[k - 1];
		v[j] += r_origin[j];
	}
}

/*
=============
R_SkyGetTexCoord
=============
*/
void R_SkyGetTexCoord (vec3_t v, float speed, float *s, float *t)
{
	vec3_t	dir;
	float	length, scroll;

	VectorSubtract (v, r_origin, dir);
	dir[2] *= 3;	// flatten the sphere

	length = dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2];
	length = sqrt(length);
	length = 6*63/length;

	scroll = cl.time*speed;
	scroll -= (int)scroll & ~127;

	*s = (scroll + dir[0] * length) * (1.0/128);
	*t = (scroll + dir[1] * length) * (1.0/128);
}

/*
=============
R_SkyDrawFaceQuad
=============
*/
void R_SkyDrawFaceQuad (glpoly_t *p)
{
	int			i;
	float		*v;
	float		s, t;
	float		skyalpha;

	skyalpha = CLAMP(0.0, r_skyalpha.value, 1.0);
	if (skyalpha == 1.0)
	{
		GL_BindTexture (solidskytexture);
		GL_SelectTMU1 ();
		GL_BindTexture (alphaskytexture);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

		glBegin (GL_QUADS);
		for (i=0, v=p->verts[0] ; i<4 ; i++, v+=VERTEXSIZE)
		{
			R_SkyGetTexCoord (v, 8, &s, &t);
			qglMultiTexCoord2f (GL_TEXTURE0_ARB, s, t);
			R_SkyGetTexCoord (v, 16, &s, &t);
			qglMultiTexCoord2f (GL_TEXTURE1_ARB, s, t);
			glVertex3fv (v);
		}
		glEnd ();

		GL_SelectTMU0 ();

		// r_speeds
		rs_c_sky_polys++;
		rs_c_sky_passes++;
	}
	else
	{
		GL_BindTexture (solidskytexture);

		if (skyalpha < 1.0)
			glColor3f (1, 1, 1);

		glBegin (GL_QUADS);
		for (i=0, v=p->verts[0] ; i<4 ; i++, v+=VERTEXSIZE)
		{
			R_SkyGetTexCoord (v, 8, &s, &t);
			glTexCoord2f (s, t);
			glVertex3fv (v);
		}
		glEnd ();
		
		GL_BindTexture (alphaskytexture);
		glEnable (GL_BLEND);

		if (skyalpha < 1.0)
			glColor4f (1, 1, 1, skyalpha);

		glBegin (GL_QUADS);
		for (i=0, v=p->verts[0] ; i<4 ; i++, v+=VERTEXSIZE)
		{
			R_SkyGetTexCoord (v, 16, &s, &t);
			glTexCoord2f (s, t);
			glVertex3fv (v);
		}
		glEnd ();

		glDisable (GL_BLEND);

		// r_speeds
		rs_c_sky_polys++;
		rs_c_sky_passes += 2;
	}

	if (R_FogGetDensity() > 0 && skyfog > 0)
	{
		float *c = R_FogGetColor();

		glEnable (GL_BLEND);
		glDisable (GL_TEXTURE_2D);
		glColor4f (c[0],c[1],c[2], skyfog);

		glBegin (GL_QUADS);
		for (i=0, v=p->verts[0] ; i<4 ; i++, v+=VERTEXSIZE)
			glVertex3fv (v);
		glEnd ();

		glColor3f (1, 1, 1);
		glEnable (GL_TEXTURE_2D);
		glDisable (GL_BLEND);

		// r_speeds
		rs_c_sky_passes++;
	}
}

/*
=============
R_SkyDrawFace
=============
*/
void R_SkyDrawFace (int axis)
{
	glpoly_t	*p;
	vec3_t		verts[4];
	int			i, j;
	float		di,qi,dj,qj;
	vec3_t		vup, vright, temp1, temp2;
	int			mark;

	R_SkySetBoxVert(-1.0, -1.0, axis, verts[0]);
	R_SkySetBoxVert(-1.0,  1.0, axis, verts[1]);
	R_SkySetBoxVert(1.0,   1.0, axis, verts[2]);
	R_SkySetBoxVert(1.0,  -1.0, axis, verts[3]);

	mark = Hunk_LowMark ();
	
	p = Hunk_AllocName (sizeof(glpoly_t), "skyface");

	VectorSubtract(verts[2],verts[3],vup);
	VectorSubtract(verts[2],verts[1],vright);

	di = CLAMP(4.0, floor(r_skyquality.value), 64.0);
	qi = 1.0 / di;
	dj = (axis < 4) ? di*2 : di; // subdivide vertically more than horizontally on skybox sides
	qj = 1.0 / dj;

	for (i=0; i<di; i++)
	{
		for (j=0; j<dj; j++)
		{
			if (i*qi < skymins[0][axis]/2+0.5 - qi || i*qi > skymaxs[0][axis]/2+0.5 ||
				j*qj < skymins[1][axis]/2+0.5 - qj || j*qj > skymaxs[1][axis]/2+0.5)
				continue;

			//if (i&1 ^ j&1) continue; // checkerboard test
			VectorScale (vright, qi*i, temp1);
			VectorScale (vup, qj*j, temp2);
			VectorAdd(temp1,temp2,temp1);
			VectorAdd(verts[0],temp1,p->verts[0]);

			VectorScale (vup, qj, temp1);
			VectorAdd (p->verts[0],temp1,p->verts[1]);

			VectorScale (vright, qi, temp1);
			VectorAdd (p->verts[1],temp1,p->verts[2]);

			VectorAdd (p->verts[0],temp1,p->verts[3]);

			R_SkyDrawFaceQuad (p);
		}
	}
	
	Hunk_FreeToLowMark (mark);
}

/*
=================
R_SkyProjectPoly

update sky bounds
=================
*/
void R_SkyProjectPoly (int nump, vec3_t vecs)
{
	int		i,j;
	vec3_t	v, av;
	float	s, t, dv;
	int		axis;
	float	*vp;

	// decide which face it maps to
	VectorClear (v);
	for (i=0, vp=vecs ; i<nump ; i++, vp+=3)
	{
		VectorAdd (vp, v, v);
	}
	av[0] = fabs(v[0]);
	av[1] = fabs(v[1]);
	av[2] = fabs(v[2]);
	if (av[0] > av[1] && av[0] > av[2])
	{
		if (v[0] < 0)
			axis = 1;
		else
			axis = 0;
	}
	else if (av[1] > av[2] && av[1] > av[0])
	{
		if (v[1] < 0)
			axis = 3;
		else
			axis = 2;
	}
	else
	{
		if (v[2] < 0)
			axis = 5;
		else
			axis = 4;
	}

	// project new texture coords
	for (i=0 ; i<nump ; i++, vecs+=3)
	{
		j = vec_to_st[axis][2];
		if (j > 0)
			dv = vecs[j - 1];
		else
			dv = -vecs[-j - 1];

		j = vec_to_st[axis][0];
		if (j < 0)
			s = -vecs[-j -1] / dv;
		else
			s = vecs[j-1] / dv;
		j = vec_to_st[axis][1];
		if (j < 0)
			t = -vecs[-j -1] / dv;
		else
			t = vecs[j-1] / dv;

		if (s < skymins[0][axis])
			skymins[0][axis] = s;
		if (t < skymins[1][axis])
			skymins[1][axis] = t;
		if (s > skymaxs[0][axis])
			skymaxs[0][axis] = s;
		if (t > skymaxs[1][axis])
			skymaxs[1][axis] = t;
	}
}

/*
=================
R_SkyClipPoly
=================
*/
void R_SkyClipPoly (int nump, vec3_t vecs, int stage)
{
	float	*norm;
	float	*v;
	qboolean	front, back;
	float	d, e;
	float	dists[MAX_CLIP_VERTS];
	int		sides[MAX_CLIP_VERTS];
	vec3_t	newv[2][MAX_CLIP_VERTS];
	int		newc[2];
	int		i, j;
	static float lastmsg = 0;
	
	if (nump > 64-2) // old limit warning
		if (IsTimeout (&lastmsg, 2))
			Con_DWarning ("R_SkyClipPoly: nump exceeds standard limit (%d, normal max = %d)\n", nump, 64-2);
	
	if (nump > MAX_CLIP_VERTS-2)
		Host_Error ("R_SkyClipPoly: MAX_CLIP_VERTS");
	
	if (stage == 6) // fully clipped
	{
		R_SkyProjectPoly (nump, vecs);
		return;
	}

	front = back = false;
	norm = skyclip[stage];
	for (i=0, v = vecs ; i<nump ; i++, v+=3)
	{
		d = DotProduct (v, norm);
		if (d > ON_EPSILON)
		{
			front = true;
			sides[i] = SIDE_FRONT;
		}
		else if (d < ON_EPSILON)
		{
			back = true;
			sides[i] = SIDE_BACK;
		}
		else
			sides[i] = SIDE_ON;
		dists[i] = d;
	}

	if (!front || !back)
	{	// not clipped
		R_SkyClipPoly (nump, vecs, stage+1);
		return;
	}

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy (vecs, (vecs+(i*3)) );
	newc[0] = newc[1] = 0;

	for (i=0, v = vecs ; i<nump ; i++, v+=3)
	{
		switch (sides[i])
		{
		case SIDE_FRONT:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			break;
		case SIDE_BACK:
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		case SIDE_ON:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		}

		if (sides[i] == SIDE_ON || sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;

		d = dists[i] / (dists[i] - dists[i+1]);
		for (j=0 ; j<3 ; j++)
		{
			e = v[j] + d*(v[j+3] - v[j]);
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}
		newc[0]++;
		newc[1]++;
	}

	// continue
	R_SkyClipPoly (newc[0], newv[0][0], stage+1);
	R_SkyClipPoly (newc[1], newv[1][0], stage+1);
}

/*
================
R_SkyProcessPoly
================
*/
void R_SkyProcessPoly (glpoly_t *p)
{
	int			i;
	vec3_t		verts[MAX_CLIP_VERTS];

	// draw it (just make it transparent)
	R_DrawGLPoly34 (p);
	rs_c_brush_passes++; // r_speeds

	// update sky bounds
	if (!r_fastsky.value)
	{
		for (i=0 ; i<p->numverts ; i++)
			VectorSubtract (p->verts[i], r_origin, verts[i]);
		R_SkyClipPoly (p->numverts, verts[0], 0);
	}
}

/*
================
R_SkyProcessTextureChains

handles sky polys in world model
================
*/
void R_SkyProcessTextureChains (void)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;

	if (!r_drawworld.value)
		return;

	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
	{
		t = cl.worldmodel->textures[i];
        
		if (!t || !t->texturechains[chain_world] || !(t->texturechains[chain_world]->flags & SURF_DRAWSKY))
			continue;
        
		for (s = t->texturechains[chain_world]; s; s = s->texturechain)
            R_SkyProcessPoly (s->polys);
	}
}

/*
================
R_SkyProcessEntities

handles sky polys on brush models
================
*/
void R_SkyProcessEntities (void)
{
	entity_t	*e;
	msurface_t	*s;
	glpoly_t	*p;
	int			i,j,k,mark;
	float		dot;
	qboolean	rotated;
	vec3_t		temp, forward, right, up;

	if (!r_drawentities.value)
		return;

	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		if ((i + 1) % 100 == 0)
			S_ExtraUpdateTime (); // don't let sound get messed up if going slow

		e = cl_visedicts[i];

		if (e->model->type != mod_brush)
			continue;

		if (R_CullModelForEntity(e))
			continue;

		VectorSubtract (r_refdef.vieworg, e->origin, modelorg);
		if (e->angles[0] || e->angles[1] || e->angles[2])
		{
			rotated = true;
			AngleVectors (e->angles, forward, right, up);
			VectorCopy (modelorg, temp);
			modelorg[0] = DotProduct (temp, forward);
			modelorg[1] = -DotProduct (temp, right);
			modelorg[2] = DotProduct (temp, up);
		}
		else
			rotated = false;

		//
		// draw it
		//
		s = &e->model->surfaces[e->model->firstmodelsurface];

		for (j=0 ; j<e->model->nummodelsurfaces ; j++, s++)
		{
			if (s->flags & SURF_DRAWSKY)
			{
				dot = DotProduct (modelorg, s->plane->normal) - s->plane->dist;
				if (((s->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
					(!(s->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
				{
					// copy the polygon and translate manually, since R_SkyProcessPoly needs it to be in world space
					mark = Hunk_LowMark ();
					
					p = Hunk_AllocName (sizeof(*s->polys), "skypoly"); //FIXME: don't allocate for each poly
					p->numverts = s->polys->numverts;
					for (k=0; k<p->numverts; k++)
					{
						if (rotated)
						{
							p->verts[k][0] = e->origin[0] + s->polys->verts[k][0] * forward[0]
														  - s->polys->verts[k][1] * right[0]
														  + s->polys->verts[k][2] * up[0];
							p->verts[k][1] = e->origin[1] + s->polys->verts[k][0] * forward[1]
														  - s->polys->verts[k][1] * right[1]
														  + s->polys->verts[k][2] * up[1];
							p->verts[k][2] = e->origin[2] + s->polys->verts[k][0] * forward[2]
														  - s->polys->verts[k][1] * right[2]
														  + s->polys->verts[k][2] * up[2];
						}
						else
							VectorAdd(s->polys->verts[k], e->origin, p->verts[k]);
					}
					R_SkyProcessPoly (p);
					
					Hunk_FreeToLowMark (mark);
				}
			}
		}
	}
}

/*
==============
R_SkyDrawSkyLayers

draws the old-style scrolling cloud layers
==============
*/
void R_SkyDrawSkyLayers (void)
{
	int i;
	float	skyalpha;

	skyalpha = CLAMP(0.0, r_skyalpha.value, 1.0);
	if (skyalpha < 1.0)
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	for (i=0 ; i<6 ; i++)
		if (skymins[0][i] < skymaxs[0][i] && skymins[1][i] < skymaxs[1][i])
			R_SkyDrawFace (i); // draw skybox arround the world

	if (skyalpha < 1.0)
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

/*
==============
R_DrawSky

called once per frame before drawing anything else
==============
*/
void R_DrawSky (void)
{
	int				i;

	//
	// reset sky bounds
	//
	for (i=0 ; i<6 ; i++)
	{
		skymins[0][i] = skymins[1][i] =  FLT_MAX;
		skymaxs[0][i] = skymaxs[1][i] = -FLT_MAX;
	}

	R_FogDisableGFog ();
	
	//
	// process world and bmodels: draw flat-shaded sky surfs, and update skybounds
	//
	glDisable (GL_TEXTURE_2D);
	if (R_FogGetDensity() > 0)
		glColor3fv (R_FogGetColor());
	else
		glColor3fv (skyflatcolor);

	R_SkyProcessTextureChains ();
	R_SkyProcessEntities ();

	glColor3f (1, 1, 1);
	glEnable (GL_TEXTURE_2D);

	//
	// render slow sky: cloud layers or skybox
	//
	if (!r_fastsky.value && !(R_FogGetDensity() > 0 && skyfog >= 1))
	{
		glDepthFunc(GL_GEQUAL);
		glDepthMask (GL_FALSE); // don't bother writing Z

		if (!r_oldsky.value && !oldsky)
			R_SkyDrawSkyBox ();
		else
			R_SkyDrawSkyLayers();

		glDepthMask (GL_TRUE); // back to normal Z buffering
		glDepthFunc(GL_LEQUAL);
	}

	R_FogEnableGFog ();
}

/*
=================
Sky_ClearAll

Called on map unload/game change to avoid keeping pointers to freed data
=================
*/
void Sky_ClearAll (void)
{
	int i;
	
	skybox_name[0] = 0;
	for (i=0; i<6; i++)
		skyboxtextures[i] = NULL;
	solidskytexture = NULL;
	alphaskytexture = NULL;
	Cvar_Set ("r_skyfog", r_skyfog.default_string);
}

/*
=============
R_InitSky

A sky texture is 256*128, with the right side being a masked overlay
==============
*/
void R_InitSky (texture_t *mt)
{
	char		texturename[64];
	int			i, j;
	int			scaledx;
	byte		*src;
	byte		fixedsky[256*128];
	static byte	front_data[128*128];
	static byte	back_data[128*128];

	squarerootof3 = sqrt(3.0);
	
	src = (byte *)mt + mt->offsets[0];

	if (mt->width * mt->height != sizeof(fixedsky))
	{
		Con_DWarning ("R_InitSky: non-standard sky texture '%s' (%dx%d, should be 256x128)\n", mt->name, mt->width, mt->height);

		// Resize sky texture to correct size
		memset (fixedsky, 0, sizeof(fixedsky));

		for (i = 0; i < 256; ++i)
		{
			scaledx = i * mt->width / 256 * mt->height;

			for (j = 0; j < 128; ++j)
				fixedsky[i * 128 + j] = src[scaledx + j * mt->height / 128];
		}

		src = fixedsky;
	}

// extract back layer and upload
	for (i=0 ; i<128 ; i++)
	{
		for (j=0 ; j<128 ; j++)
		{
			back_data[(i*128) + j] = src[i*256 + j + 128];
		}
	}

	sprintf (texturename, "%s:%s_back", loadmodel->name, mt->name);
	solidskytexture = TexMgr_LoadTexture (loadmodel, texturename, 128, 128, SRC_INDEXED, back_data, "", (uintptr_t)back_data, TEXPREF_SKY);

// extract front layer and upload
	for (i=0 ; i<128 ; i++)
	{
		for (j=0 ; j<128 ; j++)
		{
			front_data[(i*128) + j] = src[i*256 + j];
			if (front_data[(i*128) + j] == 0)
				front_data[(i*128) + j] = 255;
		}
	}

	sprintf (texturename, "%s:%s_front", loadmodel->name, mt->name);
	alphaskytexture = TexMgr_LoadTexture (loadmodel, texturename, 128, 128, SRC_INDEXED, front_data, "", (uintptr_t)front_data, TEXPREF_SKY | TEXPREF_ALPHA);

	skydata = src;
	
	R_FastSkyColor ();
}

