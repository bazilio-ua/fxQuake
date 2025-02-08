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
// render.h -- public interface to refresh functions

#define	MAXCLIPPLANES	11

#define	TOP_RANGE		16			// soldier uniform colors
#define	BOTTOM_RANGE	96

//=============================================================================

typedef struct efrag_s
{
	struct mleaf_s		*leaf;
	struct efrag_s		*leafnext;
	struct entity_s		*entity;
	struct efrag_s		*entnext;
} efrag_t;

//johnfitz -- for lerping
#define LERP_MOVESTEP	(1<<0) //this is a MOVETYPE_STEP entity, enable movement lerp
#define LERP_RESETANIM	(1<<1) //disable anim lerping until next anim frame
#define LERP_RESETANIM2	(1<<2) //set this and previous flag to disable anim lerping for two anim frames
#define LERP_RESETMOVE	(1<<3) //disable movement lerping until next origin/angles change
#define LERP_FINISH		(1<<4) //use lerpfinish time from server update instead of assuming interval of 0.1
//johnfitz

//johnfitz -- struct for passing lerp information to drawing functions
typedef struct {
	short pose1;
	short pose2;
	float blend;
	vec3_t origin;
	vec3_t angles;
} lerpdata_t;
//johnfitz

typedef struct entity_s
{
	qboolean				forcelink;		// model changed

	entity_state_t			baseline;		// to fill in defaults in updates

	double					msgtime;		// time of last update
	vec3_t					msg_origins[2];	// last two updates (0 is newest)
	vec3_t					origin;
	vec3_t					msg_angles[2];	// last two updates (0 is newest)
	vec3_t					angles;	
	struct model_s			*model;			// NULL = no model
	struct efrag_s			*efrag;			// linked list of efrags
	int						frame;
	float					syncbase;		// for client-side animations
	byte					*colormap;
	int						effects;		// light, particles, etc
	int						skinnum;		// for Alias models
	int						visframe;		// last frame this entity was found in an active leaf

	qboolean				rotated;
	float					matrix[16];		// the matrix used for transforming this entity
	
	float					dist;			// distance from client (for depth sorting)
	
	// Nehahra - Model Transparency
	byte					alpha;			// alpha
	byte					scale;			// PROTOCOL_RMQ scale
	float					fullbright;		// fullbright

	// fitzquake protocol+lerping
	byte					lerpflags;		//johnfitz -- lerping
	float					lerpstart;		//johnfitz -- animation lerping
	float					lerptime;		//johnfitz -- animation lerping
	float					lerpfinish;		//johnfitz -- lerping -- server sent us a more accurate interval, use it instead of 0.1
	short					previouspose;	//johnfitz -- animation lerping
	short					currentpose;	//johnfitz -- animation lerping
//	short					futurepose;		//johnfitz -- animation lerping
	float					movelerpstart;	//johnfitz -- transform lerping
	vec3_t					previousorigin;	//johnfitz -- transform lerping
	vec3_t					currentorigin;	//johnfitz -- transform lerping
	vec3_t					previousangles;	//johnfitz -- transform lerping
	vec3_t					currentangles;	//johnfitz -- transform lerping

} entity_t;

typedef struct
{
	vrect_t		vrect;				// subwindow in video for refresh

	vec3_t		vieworg;
	vec3_t		viewangles;
	
	float		fov_x, fov_y;
	float		weaponfov_x;		// weaponfov
} refdef_t;


//
// refresh
//
extern	refdef_t	r_refdef;
extern	vec3_t	r_origin, vpn, vright, vup;

extern	struct texture_s	*notexture_mip;
extern	struct texture_s	*notexture_mip2;

extern	entity_t r_worldentity;

void R_Init (void);
void R_InitTextures (void);
void R_InitEfrags (void);
void R_SndExtraUpdate (void);
void R_RenderView (void);		// must set r_refdef first
void R_InitSky (struct texture_s *mt);	// called at level load

void R_AddEfrags (entity_t *ent);
void R_RemoveEfrags (entity_t *ent);

void R_NewMap (void);

void R_ParseParticleEffect (void);
void R_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count);
void R_RocketTrail (vec3_t start, vec3_t end, int type);
void R_UpdateParticles (void);

void R_EntityParticles (entity_t *ent);
void R_BlobExplosion (vec3_t org);
void R_ParticleExplosion (vec3_t org);
void R_ParticleExplosion2 (vec3_t org, int colorStart, int colorLength);
void R_LavaSplash (vec3_t org);
void R_TeleportSplash (vec3_t org);

void R_PushDlights (void);

