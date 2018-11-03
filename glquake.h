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
// glquake.h

#if defined __APPLE__ && defined __MACH__
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#ifndef GL_EXT_abgr
#include <OpenGL/glext.h>
#endif
#include <dlfcn.h>
#else

#include <GL/gl.h>

#ifndef _WIN32
#define GLX_GLXEXT_PROTOTYPES
#include <GL/glx.h>
#endif

#endif

extern unsigned int d_8to24table[256];
extern unsigned int d_8to24table_fbright[256];
extern unsigned int d_8to24table_fbright_fence[256];
extern unsigned int d_8to24table_nobright[256];
extern unsigned int d_8to24table_nobright_fence[256];
extern unsigned int d_8to24table_conchars[256];

// wgl uses APIENTRY
#ifndef APIENTRY
#define APIENTRY
#endif

// for platforms (wgl) that do not use GLAPIENTRY
#ifndef GLAPIENTRY
#define GLAPIENTRY APIENTRY
#endif 

#ifdef _WIN32
#define qglGetProcAddress wglGetProcAddress
#elif defined __APPLE__ && defined __MACH__
#define qglGetProcAddress(x) dlsym(RTLD_DEFAULT, (x))
#elif defined GLX_GLXEXT_PROTOTYPES
#define glXGetProcAddress glXGetProcAddressARB
#define qglGetProcAddress(x) glXGetProcAddress((const GLubyte *)(x))
#endif

// GL_ARB_multitexture
void (GLAPIENTRY *qglMultiTexCoord1f) (GLenum, GLfloat);
void (GLAPIENTRY *qglMultiTexCoord2f) (GLenum, GLfloat, GLfloat);
void (GLAPIENTRY *qglMultiTexCoord3f) (GLenum, GLfloat, GLfloat, GLfloat);
void (GLAPIENTRY *qglMultiTexCoord4f) (GLenum, GLfloat, GLfloat, GLfloat, GLfloat);
void (GLAPIENTRY *qglActiveTexture) (GLenum);
void (GLAPIENTRY *qglClientActiveTexture) (GLenum);
#ifndef GL_ACTIVE_TEXTURE_ARB
#define GL_ACTIVE_TEXTURE_ARB			0x84E0
#define GL_CLIENT_ACTIVE_TEXTURE_ARB	0x84E1
#define GL_MAX_TEXTURE_UNITS_ARB		0x84E2
#define GL_TEXTURE0_ARB					0x84C0
#define GL_TEXTURE1_ARB					0x84C1
#define GL_TEXTURE2_ARB					0x84C2
#define GL_TEXTURE3_ARB					0x84C3
#define GL_TEXTURE4_ARB					0x84C4
#define GL_TEXTURE5_ARB					0x84C5
#define GL_TEXTURE6_ARB					0x84C6
#define GL_TEXTURE7_ARB					0x84C7
#define GL_TEXTURE8_ARB					0x84C8
#define GL_TEXTURE9_ARB					0x84C9
#define GL_TEXTURE10_ARB				0x84CA
#define GL_TEXTURE11_ARB				0x84CB
#define GL_TEXTURE12_ARB				0x84CC
#define GL_TEXTURE13_ARB				0x84CD
#define GL_TEXTURE14_ARB				0x84CE
#define GL_TEXTURE15_ARB				0x84CF
#define GL_TEXTURE16_ARB				0x84D0
#define GL_TEXTURE17_ARB				0x84D1
#define GL_TEXTURE18_ARB				0x84D2
#define GL_TEXTURE19_ARB				0x84D3
#define GL_TEXTURE20_ARB				0x84D4
#define GL_TEXTURE21_ARB				0x84D5
#define GL_TEXTURE22_ARB				0x84D6
#define GL_TEXTURE23_ARB				0x84D7
#define GL_TEXTURE24_ARB				0x84D8
#define GL_TEXTURE25_ARB				0x84D9
#define GL_TEXTURE26_ARB				0x84DA
#define GL_TEXTURE27_ARB				0x84DB
#define GL_TEXTURE28_ARB				0x84DC
#define GL_TEXTURE29_ARB				0x84DD
#define GL_TEXTURE30_ARB				0x84DE
#define GL_TEXTURE31_ARB				0x84DF
#endif 

// GL_EXT_texture_env_combine, the values for GL_ARB_ are identical
#define GL_COMBINE_EXT					0x8570
#define GL_COMBINE_RGB_EXT				0x8571
#define GL_COMBINE_ALPHA_EXT			0x8572
#define GL_RGB_SCALE_EXT				0x8573
#define GL_CONSTANT_EXT					0x8576
#define GL_PRIMARY_COLOR_EXT			0x8577
#define GL_PREVIOUS_EXT					0x8578
#define GL_SOURCE0_RGB_EXT				0x8580
#define GL_SOURCE1_RGB_EXT				0x8581
#define GL_SOURCE0_ALPHA_EXT			0x8588
#define GL_SOURCE1_ALPHA_EXT			0x8589

// Multitexture
extern qboolean mtexenabled;

extern const char *gl_vendor;
extern const char *gl_renderer;
extern const char *gl_version;
extern const char *gl_extensions;
#ifdef GLX_GLXEXT_PROTOTYPES
extern const char *glx_extensions;
#endif

extern qboolean fullsbardraw;
extern qboolean isIntel; // intel video workaround
extern qboolean gl_mtexable;
extern qboolean gl_texture_env_combine;
extern qboolean gl_texture_env_add;
extern int		gl_stencilbits;

extern int gl_hardware_max_size;
extern int gl_texture_max_size;

extern int gl_warpimage_size;

// Swap control
GLint (GLAPIENTRY *qglSwapInterval)(GLint interval);

#ifdef _WIN32
#define SWAPCONTROLSTRING "WGL_EXT_swap_control"
#define SWAPINTERVALFUNC "wglSwapIntervalEXT"
#elif defined GLX_GLXEXT_PROTOTYPES
#define SWAPCONTROLSTRING "GLX_SGI_swap_control"
#define SWAPINTERVALFUNC "glXSwapIntervalSGI"
#endif

// Anisotropic filtering
#define GL_TEXTURE_MAX_ANISOTROPY_EXT		0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT	0x84FF

extern float gl_hardware_max_anisotropy;
extern float gl_texture_anisotropy;

//====================================================

#define TEXPREF_NONE			0x0000
#define TEXPREF_MIPMAP			0x0001	// generate mipmaps
#define TEXPREF_LINEAR			0x0002	// force linear
#define TEXPREF_NEAREST			0x0004	// force nearest
#define TEXPREF_ALPHA			0x0008	// allow alpha
#define TEXPREF_PAD				0x0010	// allow padding (UNUSED)
#define TEXPREF_PERSIST			0x0020	// never free (UNUSED)
#define TEXPREF_OVERWRITE		0x0040	// overwrite existing same-name texture
#define TEXPREF_NOPICMIP		0x0080	// always load full-sized
#define TEXPREF_FULLBRIGHT		0x0100	// use fullbright mask palette
#define TEXPREF_NOBRIGHT		0x0200	// use nobright mask palette
#define TEXPREF_CONCHARS		0x0400	// use conchars palette
#define TEXPREF_WARP			0x0800	// warp texture
#define TEXPREF_WARPIMAGE		0x1000	// resize this texture when gl_warpimage_size changes
#define TEXPREF_SKY				0x2000	// sky texture
#define TEXPREF_TRANSPARENT		0x4000	// color 0 is transparent, odd - translucent, even - full value
#define TEXPREF_HOLEY			0x8000	// color 0 is transparent
#define TEXPREF_SPECIAL_TRANS	0x10000	// special (particle translucency table) H2
#define TEXPREF_BLOOM			0x20000	// bloom texture (UNUSED)

enum srcformat {SRC_INDEXED, SRC_LIGHTMAP, SRC_RGBA, SRC_BLOOM};

typedef struct gltexture_s {
//managed by texture manager
	GLuint				texnum;
	struct gltexture_s	*next;
	model_t				*owner;
//managed by image loading
	char				name[64];
	unsigned int		width;						// size of image as it exists in opengl
	unsigned int		height;						// size of image as it exists in opengl
	unsigned int		flags;						// texture preference flags
	char				source_file[MAX_QPATH];		// relative filepath to data source, or "" if source is in memory
	unsigned int		source_offset;				// byte offset into file, or memory address
	enum srcformat		source_format;				// format of pixel data (indexed, lightmap, or rgba)
	unsigned int		source_width;				// size of image in source data
	unsigned int		source_height;				// size of image in source data
	unsigned short		source_crc;					// generated by source data before modifications
} gltexture_t;

typedef struct
{
	gltexture_t	*gltexture;
	float	sl, tl, sh, th;
} glpic_t;

// particles stuff

typedef enum 
{
	pt_static, pt_grav, pt_slowgrav, pt_fire, pt_explode, pt_explode2, pt_blob, pt_blob2
} ptype_t;

typedef struct particle_s
{
// driver-usable fields
	vec3_t		org;
	float		color;
// drivers never touch the following fields
	struct particle_s	*next;
	vec3_t		vel;
	float		ramp;
	float		die;
	ptype_t		type;
} particle_t;


extern	model_t	*loadmodel;

// vid_*gl*.c
void GL_BeginRendering (int *x, int *y, int *width, int *height);
void GL_EndRendering (void);

// gl_main.c
int BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, mplane_t *p);
qboolean R_CullBox (vec3_t emins, vec3_t emaxs);
qboolean R_CullModelForEntity (entity_t *e);
void R_DrawAliasModel (entity_t *e);
void R_DrawSpriteModel (entity_t *e);

// gl_draw.c
void GL_Upload8 (gltexture_t *glt, byte *data);
void GL_Upload32 (gltexture_t *glt, unsigned *data);
void GL_UploadBloom (gltexture_t *glt, unsigned *data);
void GL_UploadLightmap (gltexture_t *glt, byte *data);
void GL_FreeTexture (gltexture_t *purge);
void GL_FreeTextures (model_t *owner);
void GL_ReloadTexture (gltexture_t *glt);
void GL_ReloadTextures_f (void);
gltexture_t *GL_LoadTexture (model_t *owner, char *name, int width, int height, enum srcformat format, byte *data, char *source_file, unsigned source_offset, unsigned flags);
gltexture_t *GL_FindTexture (model_t *owner, char *name);
void GL_SetFilterModes (gltexture_t *glt);
void GL_Set2D (void);
void GL_SelectTexture (GLenum target);
void GL_Bind (gltexture_t *texture);
void GL_DeleteTexture (gltexture_t *texture);
void GL_DisableMultitexture (void);
void GL_EnableMultitexture (void);
void GL_Init (void);
void GL_SetupState (void);
void GL_SwapInterval (void);
void GL_UploadWarpImage (void);

// gl_mesh.c
void R_MakeAliasModelDisplayLists (model_t *m, aliashdr_t *hdr);

// gl_misc.c
void R_InitTranslatePlayerTextures (void);
void R_TranslatePlayerSkin (int playernum);

// gl_part.c
void R_InitParticles (void);
void R_SetupParticles (void);
void R_DrawParticle (particle_t *p);
void R_ClearParticles (void);

// gl_efrag.c
void R_StoreEfrags (efrag_t **ppefrag);
 
// gl_light.c
void R_AnimateLight (void);
void R_LightPoint (vec3_t p, vec3_t color);
void R_MarkLights (dlight_t *light, int num, mnode_t *node);
void R_SetupDlights (void);
void R_RenderDlight (dlight_t *light);

// gl_surf.c
void R_MarkLeaves (void);
void R_SetupSurfaces (void);
//void R_MarkSurfaces (void);
//void R_CullSurfaces (void);
//qboolean R_BackFaceCull (msurface_t *surf);
void R_ClearTextureChains (model_t *model, texchain_t chain);
void R_ChainSurface (msurface_t *surf, texchain_t chain);
void R_DrawTextureChains (model_t *model, entity_t *ent, texchain_t chain);
void R_DrawBrushModel (entity_t *e);
void R_DrawWorld (void);
//void R_DrawOpaque (void);
void R_DrawGLPoly34 (glpoly_t *p);
void R_DrawGLPoly56 (glpoly_t *p);
void R_DrawSequentialPoly (msurface_t *s, float alpha, int frame);
void R_BuildLightmaps (void);
void R_UploadLightmaps (void);
void R_RebuildAllLightmaps (void);

// gl_screen.c
void SCR_TileClear (void);

// gl_anim.c
void R_UpdateWarpTextures (void);
byte *GL_LoadImage (char *name, int *width, int *height);

void R_InitMapGlobals (void);
void R_ParseWorldspawn (void);
void R_DrawSky (void);
void R_LoadSkyBox (char *skybox);

void R_FogParseServerMessage (void);
void R_FogParseServerMessage2 (void);
float *R_FogGetColor (void);
float R_FogGetDensity (void);
void R_FogEnableGFog (void);
void R_FogDisableGFog (void);
void R_FogStartAdditive (void);
void R_FogStopAdditive (void);
void R_FogSetupFrame (void);

void R_InitBloomTextures (void);
void R_BloomBlend (void);


extern float turbsin[];
#define TURBSCALE (256.0 / (2 * M_PI))
#define WARPCALC(s,t) ((s + turbsin[(int)((t*2)+(cl.time*(128.0/M_PI))) & 255]) * (1.0/64)) // correct warp


extern	int glx, gly, glwidth, glheight;

#define	GL_UNUSED_TEXTURE	(~(GLuint)0)

// private refresh defs

#define ALIAS_BASE_SIZE_RATIO		(1.0 / 11.0)
					// normalizing factor so player model works out to about
					//  1 pixel per triangle
#define	MAX_LBM_HEIGHT		1024 // was 480

#define TILE_SIZE		128		// size of textures generated by tile

#define BACKFACE_EPSILON	0.01
#define COLINEAR_EPSILON	0.001

#define FARCLIP			16384 // orig. 4096
#define NEARCLIP		4

void R_TimeRefresh_f (void);
void R_ReadPointFile_f (void);

void R_Sky_f (void);
void R_Fog_f (void);

texture_t *R_TextureAnimation (texture_t *base, int frame);


//====================================================
// GL Alpha Sorting

#define ALPHA_SURFACE 				2
#define ALPHA_ALIAS 				6
#define ALPHA_SPRITE 				7
#define ALPHA_PARTICLE 				8
#define ALPHA_DLIGHTS 				9

#define MAX_ALPHA_ITEMS			65536
typedef struct gl_alphalist_s 
{
	int			type;
	vec_t		dist;
	void 		*data;
	
	// for alpha surface
	entity_t	*entity;
	float		alpha;
} gl_alphalist_t;

extern gl_alphalist_t	gl_alphalist[MAX_ALPHA_ITEMS];
extern int				gl_alphalist_num;

qboolean R_SetAlphaSurface(msurface_t *s, float alpha);
float R_GetTurbAlpha (msurface_t *s);
vec_t R_GetAlphaDist (vec3_t origin);
void R_AddToAlpha (int type, vec_t dist, void *data, entity_t *entity, float alpha);
void R_DrawAlpha (void);

//====================================================

extern	entity_t	r_worldentity;
extern	vec3_t		modelorg, r_entorigin;
extern	int			r_visframecount;	// ??? what difs?
extern	int			r_framecount;
extern	mplane_t	frustum[4];
extern	int			rs_c_brush_polys, rs_c_brush_passes, rs_c_alias_polys, rs_c_alias_passes, rs_c_sky_polys, rs_c_sky_passes;
extern	int			rs_c_dynamic_lightmaps, rs_c_particles;

//
// view origin
//
extern	vec3_t	vup;
extern	vec3_t	vpn;
extern	vec3_t	vright;
extern	vec3_t	r_origin;

//
// screen size info
//
extern	refdef_t	r_refdef;
extern	mleaf_t		*r_viewleaf, *r_oldviewleaf;

//
// light style
//
extern	int		d_lightstyle[256];	// 8.8 fraction of base light value

//
// texture stuff
//
extern int	indexed_bytes;
extern int	rgba_bytes;
extern int	lightmap_bytes;

extern	gltexture_t *notexture;
extern	gltexture_t *nulltexture;

extern	gltexture_t *particletexture;
extern	gltexture_t *particletexture1;
extern	gltexture_t *particletexture2;

extern	gltexture_t	*playertextures[MAX_SCOREBOARD];
extern	gltexture_t	*skyboxtextures[6];

extern	float globalwateralpha;

#define	OVERBRIGHT_SCALE	2.0
extern	int		d_overbright;
extern	float	d_overbrightscale;

//extern	msurface_t *skychain;

//
// cvars
//
extern	cvar_t	r_norefresh;
extern	cvar_t	r_drawentities;
extern	cvar_t	r_drawworld;
extern	cvar_t	r_drawviewmodel;
extern	cvar_t	r_speeds;
extern	cvar_t	r_waterwarp;
extern	cvar_t	r_fullbright;
extern	cvar_t	r_waterquality;
extern	cvar_t	r_wateralpha;
extern	cvar_t	r_lockalpha;
extern	cvar_t	r_lavafog;
extern	cvar_t	r_slimefog;
extern	cvar_t	r_lavaalpha;
extern	cvar_t	r_slimealpha;
extern	cvar_t	r_teleportalpha;
extern	cvar_t	r_dynamic;
extern	cvar_t	r_dynamicscale;
extern	cvar_t	r_novis;
extern	cvar_t	r_lockfrustum;
extern	cvar_t	r_lockpvs;
extern	cvar_t	r_clearcolor;
extern	cvar_t	r_fastsky;
extern	cvar_t	r_skyquality;
extern	cvar_t	r_skyalpha;
extern	cvar_t	r_skyfog;
extern	cvar_t	r_oldsky;

extern	cvar_t	gl_finish;
extern	cvar_t	gl_clear;
extern	cvar_t	gl_cull;
extern	cvar_t	gl_farclip;
extern	cvar_t	gl_smoothmodels;
extern	cvar_t	gl_affinemodels;
extern	cvar_t	gl_polyblend;
extern	cvar_t	gl_flashblend;
extern	cvar_t	gl_flashblendview;
extern	cvar_t	gl_overbright;
extern	cvar_t  gl_oldspr;

// Nehahra
extern	cvar_t  gl_fogenable;
extern	cvar_t  gl_fogdensity;
extern	cvar_t  gl_fogred;
extern	cvar_t  gl_foggreen;
extern	cvar_t  gl_fogblue;

extern	cvar_t	r_bloom;
extern	cvar_t	r_bloom_darken;
extern	cvar_t	r_bloom_alpha;
extern	cvar_t	r_bloom_intensity;
extern	cvar_t	r_bloom_diamond_size;
extern	cvar_t	r_bloom_sample_size;
extern	cvar_t	r_bloom_fast_sample;

