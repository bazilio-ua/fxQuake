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
// gl_part.c

#include "quakedef.h"

#define MAX_PARTICLES			8192 // was 2048	// default max # of particles at one time

int		ramp1[8] = {0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61};
int		ramp2[8] = {0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66};
int		ramp3[8] = {0x6d, 0x6b, 6, 5, 4, 3};

particle_t	*active_particles, *free_particles, *particles;

int				r_numparticles;

float			texturescalefactor; // compensate for apparent size of different particle textures

cvar_t	r_particles = {"r_particles","1",true};

/*
===============
R_ParticleTextureLookup

generate nice antialiased 32x32 circle for particles
===============
*/
int R_ParticleTextureLookup (int x, int y, int sharpness)
{
	int r; // distance from point x,y to circle origin, squared
	int a; // alpha value to return

	x -= 16;
	y -= 16;
	r = x * x + y * y;
	r = r > 255 ? 255 : r;
	a = sharpness * (255 - r);
	a = min(a,255);
	return a;
}

/*
===============
R_InitParticleTextures
===============
*/
void R_InitParticleTextures (void)
{
	int				x,y;
	static byte	particle1_data[64*64*4];
	static byte	particle2_data[2*2*4];
	byte			*dst;

	//
	// particle texture 1 - circle
	//
	dst = particle1_data;
	for (x=0 ; x<64 ; x++)
	{
		for (y=0 ; y<64 ; y++)
		{
			*dst++ = 255;
			*dst++ = 255;
			*dst++ = 255;
			*dst++ = R_ParticleTextureLookup(x, y, 8);
		}
	}
	particletexture1 = GL_LoadTexture (NULL, "particle1", 64, 64, SRC_RGBA, particle1_data, "", (unsigned)particle1_data, TEXPREF_PERSIST | TEXPREF_ALPHA | TEXPREF_LINEAR);

	//
	// particle texture 2 - square
	//
	dst = particle2_data;
	for (x=0 ; x<2 ; x++)
	{
		for (y=0 ; y<2 ; y++)
		{
			*dst++ = 255;
			*dst++ = 255;
			*dst++ = 255;
			*dst++ = x || y ? 0 : 255;
		}
	}
	particletexture2 = GL_LoadTexture (NULL, "particle2", 2, 2, SRC_RGBA, particle2_data, "", (unsigned)particle2_data, TEXPREF_PERSIST | TEXPREF_ALPHA | TEXPREF_NEAREST);

	// set default
	particletexture = particletexture1;
	texturescalefactor = 1.25;
}

/*
===============
R_Particles -- johnfitz
===============
*/
void R_Particles (void)
{
	switch ((int)(r_particles.value))
	{
	default:
	case 1:
		particletexture = particletexture1;
		texturescalefactor = 1.25;
		break;
	case 2:
		particletexture = particletexture2;
		texturescalefactor = 1.0;
		break;
	}
}

/*
===============
R_InitParticles
===============
*/
void R_InitParticles (void)
{
	r_numparticles = MAX_PARTICLES;

	particles = (particle_t *)
			Hunk_AllocName (r_numparticles * sizeof(particle_t), "particles");

	Cvar_RegisterVariable (&r_particles, R_Particles);

	R_InitParticleTextures ();
}

/*
===============
R_ClearParticles
===============
*/
void R_ClearParticles (void)
{
	int		i;
	
	free_particles = &particles[0];
	active_particles = NULL;

	for (i=0 ;i<r_numparticles ; i++)
		particles[i].next = &particles[i+1];
	particles[r_numparticles-1].next = NULL;
}

/*
===============
R_AllocParticle
===============
*/
static inline particle_t *R_AllocParticle (void)
{
	particle_t *p;
	
	if (!free_particles)
		return NULL;
	p = free_particles;
	free_particles = p->next;
	p->next = active_particles;
	active_particles = p;
	return p;
}

/*
===============
R_EntityParticles
===============
*/
#define NUMVERTEXNORMALS	162
extern	float	r_avertexnormals[NUMVERTEXNORMALS][3];
vec3_t	avelocities[NUMVERTEXNORMALS];
float	beamlength = 16;
vec3_t	avelocity = {23, 7, 3};
float	partstep = 0.01;
float	timescale = 0.01;

void R_EntityParticles (entity_t *ent)
{
	int			i;
	particle_t	*p;
	float		angle;
	float		sp, sy, cp, cy;
	vec3_t		forward;
	float		dist;
	
	dist = 64;

	if (!avelocities[0][0])
	{
		for (i=0 ; i<NUMVERTEXNORMALS ; i++)
		{
			avelocities[i][0] = (rand() & 255) * 0.01;
			avelocities[i][1] = (rand() & 255) * 0.01;
			avelocities[i][2] = (rand() & 255) * 0.01;
		}
	}

	for (i=0 ; i<NUMVERTEXNORMALS ; i++)
	{
		angle = cl.time * avelocities[i][0];
		sy = sin(angle);
		cy = cos(angle);
		angle = cl.time * avelocities[i][1];
		sp = sin(angle);
		cp = cos(angle);
		angle = cl.time * avelocities[i][2];
	
		forward[0] = cp*cy;
		forward[1] = cp*sy;
		forward[2] = -sp;

		p = R_AllocParticle ();
		if (!p)
			return;

		p->die = cl.time + 0.01;
		p->color = 0x6f;
		p->type = pt_explode;
		
		p->org[0] = ent->origin[0] + r_avertexnormals[i][0]*dist + forward[0]*beamlength;			
		p->org[1] = ent->origin[1] + r_avertexnormals[i][1]*dist + forward[1]*beamlength;			
		p->org[2] = ent->origin[2] + r_avertexnormals[i][2]*dist + forward[2]*beamlength;			
	}
}

/*
===============
R_ParseParticleEffect

Parse an effect out of the server message
===============
*/
void R_ParseParticleEffect (void)
{
	vec3_t		org, dir;
	int			i, count, msgcount, color;
	
	for (i=0 ; i<3 ; i++)
		org[i] = MSG_ReadCoord (net_message);
	for (i=0 ; i<3 ; i++)
		dir[i] = MSG_ReadChar (net_message) * (1.0/16);
	msgcount = MSG_ReadByte (net_message);
	color = MSG_ReadByte (net_message);

	if (msgcount == 255)
		count = 1024;
	else
		count = msgcount;

	R_RunParticleEffect (org, dir, color, count);
}
	
/*
===============
R_ParticleExplosion
===============
*/
void R_ParticleExplosion (vec3_t org)
{
	int			i, j;
	particle_t	*p;
	
	for (i=0 ; i<1024 ; i++)
	{
		p = R_AllocParticle ();
		if (!p)
			return;

		p->die = cl.time + 5;
		p->color = ramp1[0];
		p->ramp = rand()&3;
		if (i & 1)
		{
			p->type = pt_explode;
			for (j=0 ; j<3 ; j++)
			{
				p->org[j] = org[j] + ((rand()%32)-16);
				p->vel[j] = (rand()%512)-256;
			}
		}
		else
		{
			p->type = pt_explode2;
			for (j=0 ; j<3 ; j++)
			{
				p->org[j] = org[j] + ((rand()%32)-16);
				p->vel[j] = (rand()%512)-256;
			}
		}
	}
}

/*
===============
R_ParticleExplosion2
===============
*/
void R_ParticleExplosion2 (vec3_t org, int colorStart, int colorLength)
{
	int			i, j;
	particle_t	*p;
	int			colorMod = 0;

	for (i=0; i<512; i++)
	{
		p = R_AllocParticle ();
		if (!p)
			return;

		p->die = cl.time + 0.3;
		p->color = colorStart + (colorMod % colorLength);
		colorMod++;

		p->type = pt_blob;
		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = org[j] + ((rand()%32)-16);
			p->vel[j] = (rand()%512)-256;
		}
	}
}

/*
===============
R_BlobExplosion
===============
*/
void R_BlobExplosion (vec3_t org)
{
	int			i, j;
	particle_t	*p;
	
	for (i=0 ; i<1024 ; i++)
	{
		p = R_AllocParticle ();
		if (!p)
			return;

		p->die = cl.time + 1 + (rand()&8)*0.05;

		if (i & 1)
		{
			p->type = pt_blob;
			p->color = 66 + rand()%6;
			for (j=0 ; j<3 ; j++)
			{
				p->org[j] = org[j] + ((rand()%32)-16);
				p->vel[j] = (rand()%512)-256;
			}
		}
		else
		{
			p->type = pt_blob2;
			p->color = 150 + rand()%6;
			for (j=0 ; j<3 ; j++)
			{
				p->org[j] = org[j] + ((rand()%32)-16);
				p->vel[j] = (rand()%512)-256;
			}
		}
	}
}

/*
===============
R_RunParticleEffect
===============
*/
void R_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count)
{
	int			i, j;
	particle_t	*p;
	
	for (i=0 ; i<count ; i++)
	{
		p = R_AllocParticle ();
		if (!p)
			return;

		if (count == 1024)
		{	// rocket explosion
			p->die = cl.time + 5;
			p->color = ramp1[0];
			p->ramp = rand()&3;
			if (i & 1)
			{
				p->type = pt_explode;
				for (j=0 ; j<3 ; j++)
				{
					p->org[j] = org[j] + ((rand()%32)-16);
					p->vel[j] = (rand()%512)-256;
				}
			}
			else
			{
				p->type = pt_explode2;
				for (j=0 ; j<3 ; j++)
				{
					p->org[j] = org[j] + ((rand()%32)-16);
					p->vel[j] = (rand()%512)-256;
				}
			}
		}
		else
		{
			p->die = cl.time + 0.1*(rand()%5);
			p->color = (color&~7) + (rand()&7);
			p->type = pt_slowgrav;
			for (j=0 ; j<3 ; j++)
			{
				p->org[j] = org[j] + ((rand()&15)-8);
				p->vel[j] = dir[j]*15;// + (rand()%300)-150;
			}
		}
	}
}

/*
===============
R_LavaSplash
===============
*/
void R_LavaSplash (vec3_t org)
{
	int			i, j, k;
	particle_t	*p;
	float		vel;
	vec3_t		dir;

	for (i=-16 ; i<16 ; i++)
		for (j=-16 ; j<16 ; j++)
			for (k=0 ; k<1 ; k++)
			{
				p = R_AllocParticle ();
				if (!p)
					return;

				p->die = cl.time + 2 + (rand()&31) * 0.02;
				p->color = 224 + (rand()&7);
				p->type = pt_slowgrav;
				
				dir[0] = j*8 + (rand()&7);
				dir[1] = i*8 + (rand()&7);
				dir[2] = 256;

				p->org[0] = org[0] + dir[0];
				p->org[1] = org[1] + dir[1];
				p->org[2] = org[2] + (rand()&63);

				VectorNormalize (dir);
				vel = 50 + (rand()&63);
				VectorScale (dir, vel, p->vel);
			}
}

/*
===============
R_TeleportSplash
===============
*/
void R_TeleportSplash (vec3_t org)
{
	int			i, j, k;
	particle_t	*p;
	float		vel;
	vec3_t		dir;

	for (i=-16 ; i<16 ; i+=4)
		for (j=-16 ; j<16 ; j+=4)
			for (k=-24 ; k<32 ; k+=4)
			{
				p = R_AllocParticle ();
				if (!p)
					return;

				p->die = cl.time + 0.2 + (rand()&7) * 0.02;
				p->color = 7 + (rand()&7);
				p->type = pt_slowgrav;
				
				dir[0] = j*8;
				dir[1] = i*8;
				dir[2] = k*8;

				p->org[0] = org[0] + i + (rand()&3);
				p->org[1] = org[1] + j + (rand()&3);
				p->org[2] = org[2] + k + (rand()&3);

				VectorNormalize (dir);
				vel = 50 + (rand()&63);
				VectorScale (dir, vel, p->vel);
			}
}

/*
===============
R_RocketTrail

===============
*/
void R_RocketTrail (vec3_t start, vec3_t end, int type)
{
	vec3_t		vec;
	float		len;
	int			j;
	particle_t	*p;
	int			dec;
	static int	tracercount;

	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);
	if (type < 128)
		dec = 3;
	else
	{
		dec = 1;
		type -= 128;
	}

	while (len > 0)
	{
		len -= dec;

		p = R_AllocParticle ();
		if (!p)
			return;

		VectorClear (p->vel);
		p->die = cl.time + 2;

		switch (type)
		{
		case RT_ROCKET:		// rocket trail
			p->ramp = (rand()&3);
			p->color = ramp3[(int)p->ramp];
			p->type = pt_fire;
			for (j=0 ; j<3 ; j++)
				p->org[j] = start[j] + ((rand()%6)-3);
			break;

		case RT_GRENADE:	// smoke smoke
			p->ramp = (rand()&3) + 2;
			p->color = ramp3[(int)p->ramp];
			p->type = pt_fire;
			for (j=0 ; j<3 ; j++)
				p->org[j] = start[j] + ((rand()%6)-3);
			break;

		case RT_GIB:		// blood
			p->type = pt_grav;
			p->color = 67 + (rand()&3);
			for (j=0 ; j<3 ; j++)
				p->org[j] = start[j] + ((rand()%6)-3);
			break;

		case RT_WIZARD:		// wizard tracer
		case RT_KNIGHT:		// knight tracer
			p->die = cl.time + 0.5;
			p->type = pt_static;
			if (type == 3)
				p->color = 52 + ((tracercount&4)<<1);
			else
				p->color = 230 + ((tracercount&4)<<1);
		
			tracercount++;

			VectorCopy (start, p->org);
			if (tracercount & 1)
			{
				p->vel[0] = 30*vec[1];
				p->vel[1] = 30*-vec[0];
			}
			else
			{
				p->vel[0] = 30*-vec[1];
				p->vel[1] = 30*vec[0];
			}
			break;

		case RT_ZOMGIB:		// slight blood
			p->type = pt_grav;
			p->color = 67 + (rand()&3);
			for (j=0 ; j<3 ; j++)
				p->org[j] = start[j] + ((rand()%6)-3);
			len -= 3;
			break;

		case RT_VORE:		// voor trail
			p->color = 9*16 + 8 + (rand()&3);
			p->type = pt_static;
			p->die = cl.time + 0.3;
			for (j=0 ; j<3 ; j++)
				p->org[j] = start[j] + ((rand()&15)-8);
			break;
		}

		VectorAdd (start, vec, start);
	}
}

/*
===============
R_UpdateParticles

all the particle behavior, separated from R_DrawParticles
===============
*/
void R_UpdateParticles (void)
{
	particle_t		*p, *kill;
	float			time1, time2, time3;
	float			grav, dvel;
	float			frametime;
	int				i;

	if (cls.state == ca_disconnected)
		return;

	frametime = cl.time - cl.oldtime;
	time3 = frametime * 15;
	time2 = frametime * 10; // 15;
	time1 = frametime * 5;
	grav = frametime * sv_gravity.value * 0.05;
	dvel = 4*frametime;

	for ( ;; ) 
	{
		kill = active_particles;
		if (kill && kill->die < cl.time)
		{
			active_particles = kill->next;
			kill->next = free_particles;
			free_particles = kill;
			continue;
		}
		break;
	}

	for (p=active_particles ; p ; p=p->next)
	{
		for ( ;; )
		{
			kill = p->next;
			if (kill && kill->die < cl.time)
			{
				p->next = kill->next;
				kill->next = free_particles;
				free_particles = kill;
				continue;
			}
			break;
		}

		p->org[0] += p->vel[0]*frametime;
		p->org[1] += p->vel[1]*frametime;
		p->org[2] += p->vel[2]*frametime;
		
		switch (p->type)
		{
		case pt_static:
			break;
		case pt_fire:
			p->ramp += time1;
			if (p->ramp >= 6)
				p->die = -1;
			else
				p->color = ramp3[(int)p->ramp];
			p->vel[2] += grav;
			break;

		case pt_explode:
			p->ramp += time2;
			if (p->ramp >=8)
				p->die = -1;
			else
				p->color = ramp1[(int)p->ramp];
			for (i=0 ; i<3 ; i++)
				p->vel[i] += p->vel[i]*dvel;
			p->vel[2] -= grav;
			break;

		case pt_explode2:
			p->ramp += time3;
			if (p->ramp >=8)
				p->die = -1;
			else
				p->color = ramp2[(int)p->ramp];
			for (i=0 ; i<3 ; i++)
				p->vel[i] -= p->vel[i]*frametime;
			p->vel[2] -= grav;
			break;

		case pt_blob:
			for (i=0 ; i<3 ; i++)
				p->vel[i] += p->vel[i]*dvel;
			p->vel[2] -= grav;
			break;

		case pt_blob2:
			for (i=0 ; i<2 ; i++)
				p->vel[i] -= p->vel[i]*dvel;
			p->vel[2] -= grav;
			break;

		case pt_grav:
		case pt_slowgrav:
			p->vel[2] -= grav;
			break;
		}
	}
}

/*
===============
R_SetupParticles

moved all non-drawing code to R_UpdateParticles
===============
*/
void R_SetupParticles (void)
{
	particle_t		*p;
	int				i = 0;
	
	if (!r_particles.value)
		return;
	
	for (p=active_particles ; p ; p=p->next)
	{
		// improve sound when many particles
		if (++i % 8192 == 0)
			S_ExtraUpdateTime ();
		
		R_AddToAlpha (ALPHA_PARTICLE, R_GetAlphaDist(p->org), p, NULL, 0);
		
		rs_c_particles++; // r_speeds
	}
}

/*
===============
R_DrawParticle
===============
*/
//void R_DrawParticles (void)
void R_DrawParticle (particle_t *p)
{
//	particle_t		*p;
//	int				i = 0;
	vec3_t			up, right, p_up, p_right, p_upright;
	float			scale;
	byte			*color, alpha;

	if (!r_particles.value)
		return;

	VectorScale (vup, 1.5, up);
	VectorScale (vright, 1.5, right);

	GL_Bind (particletexture);

	glEnable (GL_BLEND);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glDepthMask (GL_FALSE); // don't bother writing Z (fix for particle z-buffer bug)

	glBegin (GL_QUADS); // quads save fillrate
//	for (p=active_particles ; p ; p=p->next)
//	{
		// improve sound when many particles
//		if (++i % 8192 == 0)
//			S_ExtraUpdateTime ();

		// hack a scale up to keep particles from disapearing
		scale = (p->org[0] - r_origin[0])*vpn[0] 
			  + (p->org[1] - r_origin[1])*vpn[1]
			  + (p->org[2] - r_origin[2])*vpn[2];
		
		if (scale < 20)
			scale = 1 + 0.08; // added .08 to be consistent
		else
			scale = 1 + scale * 0.004;

		scale /= 2.0; // quad is half the size of triangle 
		scale *= texturescalefactor; // compensate for apparent size of different particle textures

		// particle fade out
		color = (byte *)&d_8to24table[(int)p->color]; // fix gcc warnings
		alpha = 255; // reserved for alpha
		glColor4ub (color[0], color[1], color[2], alpha);

		glTexCoord2f (0,0);
		glVertex3fv (p->org);

		glTexCoord2f (0.5,0);
		VectorMA (p->org, scale, up, p_up);
		glVertex3fv (p_up);

		glTexCoord2f (0.5,0.5);
		VectorMA (p_up, scale, right, p_upright);
		glVertex3fv (p_upright);

		glTexCoord2f (0,0.5);
		VectorMA (p->org, scale, right, p_right);
		glVertex3fv (p_right);

//		rs_c_particles++; // r_speeds
//	}
	glEnd ();

	glDepthMask (GL_TRUE); // back to normal Z buffering
	glDisable (GL_BLEND);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glColor3f(1,1,1);
}


/*
===============
R_ReadPointFile_f
===============
*/
void R_ReadPointFile_f (void)
{
	FILE	*f;
	vec3_t	org;
	int		r;
	int		c;
	particle_t	*p;
	char	name[MAX_QPATH]; // change MAX_OSPATH
	
	if (cls.state != ca_connected)
		return;			// need an active map.

	sprintf (name, "maps/%s.pts", cl.worldname); // change sv.name

	COM_FOpenFile (name, &f, NULL);
	if (!f)
	{
		Con_Printf ("couldn't open %s\n", name);
		return;
	}
	
	Con_Printf ("Reading %s...\n", name);
	c = 0;
	for ( ;; )
	{
		r = fscanf (f,"%f %f %f\n", &org[0], &org[1], &org[2]);
		if (r != 3)
			break;
		c++;
		
		p = R_AllocParticle ();
		if (!p)
		{
			Con_Printf ("Not enough free particles\n");
			break;
		}

		p->die = 99999;
		p->color = (-c)&15;
		p->type = pt_static;
		VectorClear (p->vel);
		VectorCopy (org, p->org);
	}

	fclose (f);
	Con_Printf ("%i points read\n", c);
}

