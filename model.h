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
// model.h

#include "modelgen.h"
#include "spritegn.h"

/*

d*_t structures are on-disk representations
m*_t structures are in-memory

*/

// entity effects

#define	EF_BRIGHTFIELD			1
#define	EF_MUZZLEFLASH 			2
#define	EF_BRIGHTLIGHT 			4
#define	EF_DIMLIGHT 			8
// Nehahra
#define	EF_NODRAW				16
#define	EF_BLUE					64
#define	EF_RED					128


/*
==============================================================================

BRUSH MODELS

==============================================================================
*/


//
// in memory representation
//
typedef struct
{
	vec3_t		position;
} mvertex_t;

#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2

#define BOX_ON_PLANE_SIDE(emins, emaxs, p)	\
    (((p)->type < 3)?						\
    (										\
        ((p)->dist <= (emins)[(p)->type])?	\
            1								\
        :									\
        (									\
            ((p)->dist >= (emaxs)[(p)->type])?\
                2							\
            :								\
                3							\
        )									\
    )										\
    :										\
        BoxOnPlaneSide( (emins), (emaxs), (p)))

// plane_t structure
typedef struct mplane_s
{
	vec3_t	normal;
	float	dist;
	byte	type;			// for texture axis selection and fast side tests
	byte	signbits;		// signx + signy<<1 + signz<<1
	byte	pad[2];
} mplane_t;

// ericw -- each texture has two chains,
// so we can clear the model chains without affecting the world
typedef enum 
{
	chain_world = 0,
	chain_model = 1
} texchain_t;

typedef struct texture_s
{
	char		name[16];
	unsigned	width, height;
	struct gltexture_s	*gltexture; // pointer to gltexture
	struct gltexture_s	*fullbright; // fullbright mask texture
	struct gltexture_s	*warpimage; // for water animation
	qboolean	update_warp;			// update warp this frame
	struct msurface_s	*texturechain;	// for texture chains (OLD way)
	struct msurface_s	*texturechains[2];	// for texture chains
	int			anim_total;				// total tenths in sequence ( 0 = no)
	int			anim_min, anim_max;		// time for this frame min <=time< max
	struct texture_s *anim_next;		// in the animation sequence
	struct texture_s *alternate_anims;	// bmodels in frame 1 use these
	unsigned	offsets[MIPLEVELS];		// four mip maps stored
} texture_t;

#define	SURF_PLANEBACK		2
#define	SURF_DRAWSKY		4
#define SURF_DRAWSPRITE		8
#define SURF_DRAWTURB		0x10
#define SURF_DRAWTILED		0x20
#define SURF_DRAWBACKGROUND	0x40
#define SURF_UNDERWATER		0x80
#define SURF_TRANSLUCENT	0x100 // EER1
#define SURF_DRAWBLACK		0x200
#define SURF_NOTEXTURE		0x400 // johnfitz
#define SURF_DRAWALPHA		0x800
#define SURF_DRAWLAVA		0x1000
#define SURF_DRAWSLIME		0x2000
#define SURF_DRAWTELEPORT	0x4000
#define SURF_DRAWWATER		0x8000
#define SURF_DRAWFENCE		0x10000 // EER1

typedef struct
{
	unsigned int	v[2]; // bsp2 support. was (short)
	unsigned int	cachededgeoffset;
} medge_t;

typedef struct
{
	float		vecs[2][4];
	float		mipadjust;
	texture_t	*texture;
	int			flags;
} mtexinfo_t;

#define	VERTEXSIZE	7

typedef struct glpoly_s
{
	struct	glpoly_s	*next;
	struct	glpoly_s	*chain;
	int		numverts;

	float	verts[4][VERTEXSIZE];	// variable sized (xyz s1t1 s2t2)
} glpoly_t;

typedef struct msurface_s
{
	mtexinfo_t	*texinfo;
	
	int			visframe;		// should be drawn when node is crossed
//	qboolean	culled;			// johnfitz -- for frustum culling
	float		mins[3];		// johnfitz -- for frustum culling
	float		maxs[3];		// johnfitz -- for frustum culling
	
	float		midp[3];		// for alpha sorting

	mplane_t	*plane;
	int			flags;

	int			firstedge;	// look up in model->surfedges[], negative numbers
	int			numedges;	// are backwards edges
	
	short		texturemins[2];
	short		extents[2];

	int			light_s, light_t;	// gl lightmap coordinates

	glpoly_t	*polys;				// multiple if warped
	struct	msurface_s	*texturechain;

// lighting info
	int			dlightframe;
	unsigned int		dlightbits[(MAX_DLIGHTS + 31) >> 5];
				// int is 32 bits, need an array for MAX_DLIGHTS > 32

	int			lightmaptexture;
	byte		styles[MAXLIGHTMAPS];
	int			cached_light[MAXLIGHTMAPS];	// values currently used in lightmap
	qboolean	cached_dlight;				// true if dynamic light in cache
	byte		*samples;		// [numstyles*surfsize]
} msurface_t;

typedef struct mnode_s
{
// common with leaf
	int			contents;		// 0, to differentiate from leafs
	int			visframe;		// node needs to be traversed if current
	
	float		minmaxs[6];		// for bounding box culling

	struct mnode_s	*parent;

// node specific
	mplane_t	*plane;
	struct mnode_s	*children[2];	

	unsigned int		firstsurface; // bsp2 support. was (short)
	unsigned int		numsurfaces; // bsp2 support. was (short)
} mnode_t;


typedef struct mleaf_s
{
// common with node
	int			contents;		// wil be a negative contents number
	int			visframe;		// node needs to be traversed if current

	float		minmaxs[6];		// for bounding box culling

	struct mnode_s	*parent;

// leaf specific
	byte		*compressed_vis;
	efrag_t		*efrags;

	msurface_t	**firstmarksurface;
	int			nummarksurfaces;
	int			key;			// BSP sequence number for leaf's contents
	byte		ambient_sound_level[NUM_AMBIENTS];
} mleaf_t;

typedef struct mclipnode_s
{
//johnfitz -- for clipnodes>32k
	int			planenum;
	int			children[2];	// negative numbers are contents
} mclipnode_t;

typedef struct
{
	mclipnode_t	*clipnodes; //johnfitz -- was dclipnode_t
	mplane_t	*planes;
	int			firstclipnode;
	int			lastclipnode;
	vec3_t		clip_mins;
	vec3_t		clip_maxs;
	int			available;
} hull_t;


// on-disk vis data structure:  stored in little endian format
#define VISPATCH_MAPNAME_IDLEN	32
#define VISPATCH_HEADER_LEN		36
typedef struct
{
	// header
	char	mapname[VISPATCH_MAPNAME_IDLEN];	// Baker: DO NOT CHANGE THIS to MAX_QPATH, must be 32
	int		datalen;							// length of data after VisPatch header (VIS+Leafs)

	// data
	int		vislen;
	byte	*visdata;
	int		leaflen;
	byte	*leafdata;
} vispatch_t;


/*
==============================================================================

SPRITE MODELS

==============================================================================
*/


// FIXME: shorten these?
typedef struct mspriteframe_s
{
	int		width, height;
	float	up, down, left, right;
	float	smax, tmax; // image might be padded
	struct	gltexture_s	*gltexture;
} mspriteframe_t;

typedef struct
{
	int				numframes;
	float			*intervals;
	mspriteframe_t	*frames[1];
} mspritegroup_t;

typedef struct
{
	spriteframetype_t	type;
	mspriteframe_t		*frameptr;
} mspriteframedesc_t;

typedef struct
{
	int					type;
	int					maxwidth;
	int					maxheight;
	int					numframes;
	float				beamlength;		// remove?
	void				*cachespot;		// remove?
	mspriteframedesc_t	frames[1];
} msprite_t;


/*
==============================================================================

ALIAS MODELS

Alias models are position independent, so the cache manager can move them.
==============================================================================
*/

typedef struct
{
	int					firstpose;
	int					numposes;
	float				interval;
	trivertx_t			bboxmin;
	trivertx_t			bboxmax;
	int					frame;
	char				name[16];
} maliasframedesc_t;

typedef struct
{
	trivertx_t			bboxmin;
	trivertx_t			bboxmax;
	int					frame;
} maliasgroupframedesc_t;

typedef struct
{
	int						numframes;
	int						intervals;
	maliasgroupframedesc_t	frames[1];
} maliasgroup_t;

typedef struct mtriangle_s {
	int					facesfront;
	int					vertindex[3];
} mtriangle_t;


#define	MAX_SKINS	32
typedef struct {
	int			ident;
	int			version;
	vec3_t		scale;
	vec3_t		scale_origin;
	float		boundingradius;
	vec3_t		eyeposition;
	int			numskins;
	int			skinwidth;
	int			skinheight;
	int			numverts;
	int			numtris;
	int			numframes;
	synctype_t	synctype;
	int			flags;
	float		size;

	int					numposes;
	int					poseverts;
	int					posedata;	// numposes*poseverts trivert_t
	int					commands;	// gl command list with embedded s/t

	struct gltexture_s	*gltexture[MAX_SKINS][4];
	struct gltexture_s	*fullbright[MAX_SKINS][4];

	int					texels[MAX_SKINS];	// only for player skins
	maliasframedesc_t	frames[1];	// variable sized
} aliashdr_t;

#define	MAXALIASVERTS	4096	//1024
#define	MAXALIASFRAMES	1024	//256
#define	MAXALIASTRIS	4096	//2048
extern	aliashdr_t	*pheader;
extern	stvert_t	stverts[MAXALIASVERTS];
extern	mtriangle_t	triangles[MAXALIASTRIS];
extern	trivertx_t	*poseverts[MAXALIASFRAMES];

//===================================================================

//
// Whole model
//

typedef enum {mod_brush, mod_sprite, mod_alias} modtype_t;

#define	EF_ROCKET	1			// leave a trail
#define	EF_GRENADE	2			// leave a trail
#define	EF_GIB		4			// leave a trail
#define	EF_ROTATE	8			// rotate (bonus items)
#define	EF_TRACER	16			// green split trail
#define	EF_ZOMGIB	32			// small blood trail
#define	EF_TRACER2	64			// orange split trail + rotate
#define	EF_TRACER3	128			// purple trail

#define	MF_HOLEY	(1u<<14)		// MarkV/QSS -- make index 255 transparent on mdl's

typedef struct model_s
{
	char		name[MAX_QPATH];
	unsigned int	path_id;	// path id of the game directory that this model came from

	qboolean	needload;		// bmodels and sprites don't cache normally
	int		size;			// size of model

	modtype_t	type;
	int			numframes;
	synctype_t	synctype;
	
	int			flags;

//
// volume occupied by the model graphics
//		
	vec3_t		mins, maxs;
	vec3_t		ymins, ymaxs; // bounds for entities with nonzero yaw
	vec3_t		rmins, rmaxs; // bounds for entities with nonzero pitch or roll

//
// brush model
//
	int			firstmodelsurface, nummodelsurfaces;

	int			numsubmodels;
	dmodel_t	*submodels;

	int			numplanes;
	mplane_t	*planes;

	int			numleafs;		// number of visible leafs, not counting 0
	mleaf_t		*leafs;

	int			numvertexes;
	mvertex_t	*vertexes;

	int			numedges;
	medge_t		*edges;

	int			numnodes;
	mnode_t		*nodes;

	int			numtexinfo;
	mtexinfo_t	*texinfo;

	int			numsurfaces;
	msurface_t	*surfaces;

	int			numsurfedges;
	int			*surfedges;

	int			numclipnodes;
	mclipnode_t	*clipnodes; //johnfitz -- was dclipnode_t

	int			nummarksurfaces;
	msurface_t	**marksurfaces;

	hull_t		hulls[MAX_MAP_HULLS];

	int			numtextures;
	texture_t	**textures;

	byte		*visdata;
	byte		*lightdata;
	char		*entities;

	qboolean	viswarn; // for Mod_DecompressVis()
    
	qboolean	isworldmodel;
	int			bspversion;

//
// additional model data
//
	cache_user_t	cache;		// only access through Mod_Extradata

} qmodel_t;

//============================================================================

void	Mod_Init (void);
void	Mod_ClearAll (void);
qmodel_t *Mod_ForName (char *name, qboolean crash);
void	*Mod_Extradata (qmodel_t *mod);	// handles caching
void	Mod_TouchModel (char *name);

mleaf_t *Mod_PointInLeaf (float *p, qmodel_t *model);
byte	*Mod_LeafPVS (mleaf_t *leaf, qmodel_t *model);
byte	*Mod_NoVisPVS (qmodel_t *model);

