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
// gl_draw.c -- this is the only file outside the refresh that touches the vid buffer

#include "quakedef.h"

cvar_t		scr_conalpha = {"scr_conalpha", "1", true};
cvar_t		gl_picmip = {"gl_picmip", "0"};
cvar_t		gl_swapinterval = {"gl_swapinterval", "0", true};
cvar_t		gl_warp_image_size = {"gl_warp_image_size", "256", true}; // was 512, for water warp

byte		*draw_chars;				// 8*8 graphic characters
qpic_t		*draw_disc;
qpic_t		*draw_backtile;

gltexture_t	*notexture;
gltexture_t	*nulltexture;
gltexture_t	*char_texture; 

int		indexed_bytes = 1;
int		rgba_bytes = 4;
int		lightmap_bytes = 4;

#define MAXGLMODES 6

typedef struct
{
	int magfilter;
	int minfilter;
	char *name;
} glmode_t;

glmode_t modes[MAXGLMODES] = {
	{GL_NEAREST, GL_NEAREST,				"GL_NEAREST"},
	{GL_NEAREST, GL_NEAREST_MIPMAP_NEAREST,	"GL_NEAREST_MIPMAP_NEAREST"},
	{GL_NEAREST, GL_NEAREST_MIPMAP_LINEAR,	"GL_NEAREST_MIPMAP_LINEAR"},
	{GL_LINEAR,  GL_LINEAR,					"GL_LINEAR"},
	{GL_LINEAR,  GL_LINEAR_MIPMAP_NEAREST,	"GL_LINEAR_MIPMAP_NEAREST"},
	{GL_LINEAR,  GL_LINEAR_MIPMAP_LINEAR,	"GL_LINEAR_MIPMAP_LINEAR"},
};

int		gl_texturemode = 3; // linear
int		gl_filter_min = GL_LINEAR; // was GL_NEAREST
int		gl_filter_mag = GL_LINEAR;

float	gl_hardware_max_anisotropy = 1; // just in case
float 	gl_texture_anisotropy = 1;

qboolean gl_texture_NPOT = false; //ericw

int		gl_hardware_max_size = 1024; // just in case
int		gl_texture_max_size = 1024;

int		gl_warpimage_size = 256; // fitzquake has 512, for water warp

#define	MAX_GLTEXTURES	4096 // orig was 1024, prev 2048
gltexture_t	*active_gltextures, *free_gltextures;
int			numgltextures;

static GLuint currenttexture[3] = {GL_UNUSED_TEXTURE, GL_UNUSED_TEXTURE, GL_UNUSED_TEXTURE}; // to avoid unnecessary texture sets
static GLenum currenttarget = GL_TEXTURE0_ARB;
qboolean mtexenabled = false;

unsigned int d_8to24table[256];
unsigned int d_8to24table_fbright[256];
unsigned int d_8to24table_fbright_fence[256];
unsigned int d_8to24table_nobright[256];
unsigned int d_8to24table_nobright_fence[256];
unsigned int d_8to24table_conchars[256];

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

qboolean fullsbardraw = false;
qboolean isIntel = false; // intel video workaround
qboolean gl_mtexable = false;
qboolean gl_texture_env_combine = false;
qboolean gl_texture_env_add = false;
qboolean gl_swap_control = false;

/*
================================================

			OpenGL Stuff

================================================
*/

/*
================
GL_CheckSize

return smallest power of two greater than or equal to size
================
*/
int GL_CheckSize (int size)
{
	int checksize;

	for (checksize = 1; checksize < size; checksize <<= 1)
		;

	return checksize;
}

/*
===============
GL_UploadWarpImage

called during init,
choose correct warpimage size and reload existing warpimage textures if needed
===============
*/
void GL_UploadWarpImage (void)
{
//	int	oldsize;
	int mark;
	gltexture_t *glt;
	byte *dummy;

	//
	// find the new correct size
	//
//	oldsize = gl_warpimage_size;

	if ((int)gl_warp_image_size.value < 32)
		Cvar_SetValue ("gl_warp_image_size", 32);

	//
	// make sure warpimage size is a power of two
	//
	gl_warpimage_size = GL_CheckSize((int)gl_warp_image_size.value);

	while (gl_warpimage_size > vid.width)
		gl_warpimage_size >>= 1;
	while (gl_warpimage_size > vid.height)
		gl_warpimage_size >>= 1;

	if (gl_warpimage_size != gl_warp_image_size.value)
		Cvar_SetValue ("gl_warp_image_size", gl_warpimage_size);

//	if (gl_warpimage_size == oldsize)
//		return;
    
    // ericw -- removed early exit if (gl_warpimage_size == oldsize).
	// after reloads textures to source width/height, which might not match oldsize.
    
	//
	// resize the textures in opengl
	//
	mark = Hunk_LowMark ();
	
	dummy = Hunk_Alloc (gl_warpimage_size*gl_warpimage_size*4);

	for (glt=active_gltextures; glt; glt=glt->next)
	{
		if (glt->flags & TEXPREF_WARPIMAGE)
		{
			GL_Bind (glt);
			glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, gl_warpimage_size, gl_warpimage_size, 0, GL_RGBA, GL_UNSIGNED_BYTE, dummy);
			glt->width = glt->height = gl_warpimage_size;
            
            // set filter modes
            GL_SetFilterModes (glt);
		}
	}

	Hunk_FreeToLowMark (mark);
}

/*
===============
GL_CheckExtensions
===============
*/
void GL_CheckExtensions (void) 
{
	qboolean ARB = false;
	qboolean EXTcombine, ARBcombine;
	qboolean EXTadd, ARBadd;
#if !defined __APPLE__ && !defined __MACH__
	qboolean SWAPcontrol;
#endif
	qboolean anisotropy;
    qboolean npot;
	int units;

	//
	// poll max size from hardware
	//
	glGetIntegerv (GL_MAX_TEXTURE_SIZE, &gl_hardware_max_size);
	Con_Printf ("Maximum texture size %i\n", gl_hardware_max_size);

	// by default we sets maxsize as hardware maxsize
	gl_texture_max_size = gl_hardware_max_size; 

	//
	// Multitexture
	//
	ARB = strstr (gl_extensions, "GL_ARB_multitexture") != NULL;

	if (COM_CheckParm("-nomtex"))
	{
		Con_Warning ("Multitexture disabled at command line\n");
	}
	else if (ARB)
	{
		// Check how many texture units there actually are
		glGetIntegerv (GL_MAX_TEXTURE_UNITS_ARB, &units);

		qglMultiTexCoord2f = (void *) qglGetProcAddress ("glMultiTexCoord2fARB");
		qglActiveTexture = (void *) qglGetProcAddress ("glActiveTextureARB");
        qglClientActiveTexture = (void *) qglGetProcAddress ("glClientActiveTextureARB");

		if (units < 2)
		{
			Con_Warning ("Only %i TMU available, multitexture not supported\n", units);
		}
		else if (!qglMultiTexCoord2f || !qglActiveTexture || !qglClientActiveTexture)
		{
			Con_Warning ("Multitexture not supported (qglGetProcAddress failed)\n");
		}
		else
		{
			Con_Printf ("GL_ARB_multitexture extension found\n");
			Con_Printf ("   %i TMUs on hardware\n", units);

			gl_mtexable = true;
		} 
	}
	else
	{
		Con_Warning ("Multitexture not supported (extension not found)\n");
	}

	//
	// Texture combine environment mode
	//
	ARBcombine = strstr (gl_extensions, "GL_ARB_texture_env_combine") != NULL;
	EXTcombine = strstr (gl_extensions, "GL_EXT_texture_env_combine") != NULL;

	if (COM_CheckParm("-nocombine"))
	{
		Con_Warning ("Texture combine environment disabled at command line\n");
	}
	else if (ARBcombine || EXTcombine)
	{
		Con_Printf ("GL_%s_texture_env_combine extension found\n", ARBcombine ? "ARB" : "EXT");
		gl_texture_env_combine = true;
	}
	else
	{
		Con_Warning ("Texture combine environment not supported (extension not found)\n");
	}

	//
	// Texture add environment mode
	//
	ARBadd = strstr (gl_extensions, "GL_ARB_texture_env_add") != NULL;
	EXTadd = strstr (gl_extensions, "GL_EXT_texture_env_add") != NULL;

	if (COM_CheckParm("-noadd"))
	{
		Con_Warning ("Texture add environment disabled at command line\n");
	}
	else if (ARBadd || EXTadd)
	{
		Con_Printf ("GL_%s_texture_env_add extension found\n", ARBadd ? "ARB" : "EXT");
		gl_texture_env_add = true;
	}
	else
	{
		Con_Warning ("Texture add environment not supported (extension not found)\n");
	}

	//
	// Swap control
	//
#ifdef _WIN32
	SWAPcontrol = strstr (gl_extensions, SWAPCONTROLSTRING) != NULL;
#elif defined GLX_GLXEXT_PROTOTYPES
	SWAPcontrol = strstr (glx_extensions, SWAPCONTROLSTRING) != NULL;
#endif

#if !defined __APPLE__ && !defined __MACH__
	if (COM_CheckParm("-novsync"))
	{
		Con_Warning ("Vertical sync disabled at command line\n");
	}
	else if (SWAPcontrol)
	{
		qglSwapInterval = (void *) qglGetProcAddress (SWAPINTERVALFUNC);

		if (qglSwapInterval)
		{
			if (!qglSwapInterval(0))
				Con_Warning ("Vertical sync not supported (%s failed)\n", SWAPINTERVALFUNC);
			else
			{
				Con_Printf ("%s extension found\n", SWAPCONTROLSTRING);
				gl_swap_control = true;
			}
		}
		else
			Con_Warning ("Vertical sync not supported (qglGetProcAddress failed)\n");
	}
	else
		Con_Warning ("Vertical sync not supported (extension not found)\n");
#endif

	//
	// Anisotropic filtering
	//
	anisotropy = strstr (gl_extensions, "GL_EXT_texture_filter_anisotropic") != NULL;

	if (COM_CheckParm("-noanisotropy"))
	{
		Con_Warning ("Anisotropic filtering disabled at command line\n");
	}
	else if (anisotropy)
	{
		float test1, test2;
		GLuint tex;

		// test to make sure we really have control over it
		// 1.0 and 2.0 should always be legal values
		glGenTextures (1, &tex);
		glBindTexture (GL_TEXTURE_2D, tex);
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f);
		glGetTexParameterfv (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, &test1);
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 2.0f);
		glGetTexParameterfv (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, &test2);
		glDeleteTextures (1, &tex);

		if (test1 == 1 && test2 == 2)
			Con_Printf ("GL_EXT_texture_filter_anisotropic extension found\n");
		else
			Con_Warning ("Anisotropic filtering locked by driver. Current driver setting is %f\n", test1);

		// get max value either way, so the menu and stuff know it
		glGetFloatv (GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gl_hardware_max_anisotropy);
	}
	else
	{
		Con_Warning ("Anisotropic filtering not supported (extension not found)\n");
	}
    
    //
    // Texture non power of two
	//
    npot = strstr (gl_extensions, "GL_ARB_texture_non_power_of_two") != NULL;
    
	if (COM_CheckParm("-notexturenpot"))
	{
		Con_Warning ("Texture non power of two disabled at command line\n");
	}
	else if (npot)
    {
		Con_Printf ("GL_ARB_texture_non_power_of_two extension found\n");
		gl_texture_NPOT = true;
    }
	else
	{
		Con_Warning ("Texture non power of two not supported (extension not found)\n");
	}
}

/*
===============
GL_MakeExtensionsList
===============
*/
char *GL_MakeExtensionsList (const char *in)
{
	char *copy, *token, *out;
	int i, count;

	// each space will be replaced by 4 chars, so count the spaces before we malloc
	for (i = 0, count = 1; i < (int)strlen(in); i++)
		if (in[i] == ' ')
			count++;
	out = Z_Malloc (strlen(in) + count*3 + 1); // usually about 1-2k
	out[0] = 0;

	copy = Z_Malloc (strlen(in) + 1);
	strcpy(copy, in);

	for (token = strtok(copy, " "); token; token = strtok(NULL, " "))
	{
		strcat(out, "\n   ");
		strcat(out, token);
	}

	Z_Free (copy);
	return out;
}

/*
===============
GL_Info_f
===============
*/
void GL_Info_f (void)
{
	static char *gl_extensions_nice = NULL;
#ifdef GLX_GLXEXT_PROTOTYPES
	static char *glx_extensions_nice = NULL;
#endif

	if (!gl_extensions_nice)
		gl_extensions_nice = GL_MakeExtensionsList (gl_extensions);
#ifdef GLX_GLXEXT_PROTOTYPES
	if (!glx_extensions_nice)
		glx_extensions_nice = GL_MakeExtensionsList (glx_extensions);
#endif

	Con_SafePrintf ("GL_VENDOR: %s\n", gl_vendor);
	Con_SafePrintf ("GL_RENDERER: %s\n", gl_renderer);
	Con_SafePrintf ("GL_VERSION: %s\n", gl_version);
	Con_SafePrintf ("GL_EXTENSIONS: %s\n", gl_extensions_nice);
#ifdef GLX_GLXEXT_PROTOTYPES
	Con_SafePrintf ("GLX_EXTENSIONS: %s\n", glx_extensions_nice);
#endif
}

/*
===============
GL_SwapInterval
===============
*/
#if defined __APPLE__ && defined __MACH__
void CGL_SwapInterval (qboolean enable);
#endif

void GL_SwapInterval (void)
{
#if !defined __APPLE__ && !defined __MACH__    
	if (gl_swap_control)
	{
		if (gl_swapinterval.value)
		{
			if (!qglSwapInterval(1))
				Con_Printf ("GL_SwapInterval: failed on %s\n", SWAPINTERVALFUNC);
		}
		else
		{
			if (!qglSwapInterval(0))
				Con_Printf ("GL_SwapInterval: failed on %s\n", SWAPINTERVALFUNC);
		}
	}
#else
    if (gl_swapinterval.value) {
        CGL_SwapInterval(true);
    } else {
        CGL_SwapInterval(false);
    }
#endif
}

/*
===============
GL_SetupState

does all the stuff from GL_Init that needs to be done every time a new GL render context is created
GL_Init will still do the stuff that only needs to be done once
===============
*/
void GL_SetupState (void)
{
	glClearColor (0.15,0.15,0.15,0); // originally was 1,0,0,0

	glCullFace (GL_FRONT);
//	glCullFace (GL_BACK); // glquake used CCW with backwards culling -- let's do it right
//	glFrontFace (GL_CW); // glquake used CCW with backwards culling -- let's do it right

	glEnable (GL_TEXTURE_2D);

	glEnable (GL_ALPHA_TEST);
	glAlphaFunc (GL_GREATER, 0.666);

	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel (GL_FLAT);

	glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE); 

	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glDepthRange (0, 1); // moved here because gl_ztrick is gone.
	glDepthFunc (GL_LEQUAL); // moved here because gl_ztrick is gone.
}

/*
===============
GL_Init
===============
*/
void GL_Init (void)
{
	// gl_info
	gl_vendor = (const char *)glGetString (GL_VENDOR);
	gl_renderer = (const char *)glGetString (GL_RENDERER);
	gl_version = (const char *)glGetString (GL_VERSION);
	gl_extensions = (const char *)glGetString (GL_EXTENSIONS);

	Cmd_AddCommand ("gl_info", GL_Info_f);
	Cmd_AddCommand ("gl_reloadtextures", GL_ReloadTextures_f);

	Cvar_RegisterVariable (&gl_swapinterval, GL_SwapInterval);

	Con_Printf ("OpenGL initialized\n");

	GL_CheckExtensions ();

	if (!strcmp(gl_vendor, "Intel")) // intel video workaround
	{
		Con_Printf ("Intel Display Adapter detected\n");
		isIntel = true;
	}

	GL_SetupState ();
}

/*
================
GL_Bind
================
*/
void GL_Bind (gltexture_t *texture)
{
	if (!texture)
		texture = nulltexture;

	if (texture->texnum != currenttexture[currenttarget - GL_TEXTURE0_ARB])
	{
		currenttexture[currenttarget - GL_TEXTURE0_ARB] = texture->texnum;
		glBindTexture (GL_TEXTURE_2D, texture->texnum);
	}
}

/*
================
GL_DeleteTexture -- ericw

Wrapper around glDeleteTextures that also clears the given texture number
from our per-TMU cached texture binding table.
================
*/
void GL_DeleteTexture (gltexture_t *texture)
{
	glDeleteTextures (1, &texture->texnum);
    
	if (texture->texnum == currenttexture[0]) currenttexture[0] = GL_UNUSED_TEXTURE;
	if (texture->texnum == currenttexture[1]) currenttexture[1] = GL_UNUSED_TEXTURE;
	if (texture->texnum == currenttexture[2]) currenttexture[2] = GL_UNUSED_TEXTURE;
    
	texture->texnum = 0;
}

/*
================
GL_ClearBindings -- ericw

Invalidates cached bindings, so the next GL_Bind calls for each TMU will
make real glBindTexture calls.
Call this after changing the binding outside of GL_Bind.
================
*/
void GL_ClearBindings(void)
{
	int i;
    
	for (i = 0; i < 3; i++)
	{
		currenttexture[i] = GL_UNUSED_TEXTURE;
	}
}

/*
================
GL_SelectTexture
================
*/
void GL_SelectTexture (GLenum target)
{
	if (target == currenttarget)
		return;

	qglActiveTexture (target);
	currenttarget = target;
}

/*
================
GL_DisableMultitexture

selects texture unit 0
================
*/
void GL_DisableMultitexture (void) 
{
	if (mtexenabled) 
	{
		glDisable (GL_TEXTURE_2D);
		GL_SelectTexture (GL_TEXTURE0_ARB);
		mtexenabled = false;
	}
}

/*
================
GL_EnableMultitexture

selects texture unit 1
================
*/
void GL_EnableMultitexture (void) 
{
	if (gl_mtexable) 
	{
		GL_SelectTexture (GL_TEXTURE1_ARB);
		glEnable (GL_TEXTURE_2D);
		mtexenabled = true;
	}
}


/*
=============================================================================

  scrap allocation

  Allocate all the little status bar objects into a single texture
  to crutch up stupid hardware / drivers

=============================================================================
*/

#define	MAX_SCRAPS		2
#define	BLOCK_WIDTH		256
#define	BLOCK_HEIGHT	256

int			scrap_allocated[MAX_SCRAPS][BLOCK_WIDTH];
byte		scrap_texels[MAX_SCRAPS][BLOCK_WIDTH*BLOCK_HEIGHT];
qboolean	scrap_dirty;
gltexture_t	*scrap_textures[MAX_SCRAPS]; // changed to array

// returns a texture number and the position inside it
int Scrap_AllocBlock (int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;
	int		texnum;

	for (texnum=0 ; texnum<MAX_SCRAPS ; texnum++)
	{
		best = BLOCK_HEIGHT;

		for (i=0 ; i<BLOCK_WIDTH-w ; i++)
		{
			best2 = 0;

			for (j=0 ; j<w ; j++)
			{
				if (scrap_allocated[texnum][i+j] >= best)
					break;
				if (scrap_allocated[texnum][i+j] > best2)
					best2 = scrap_allocated[texnum][i+j];
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
			scrap_allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	return -1;
}

void Scrap_Upload (void)
{
	int		i;
	char name[64];

	for (i = 0; i < MAX_SCRAPS; i++) 
	{
		sprintf (name, "scrap%i", i);
		scrap_textures[i] = GL_LoadTexture (NULL, name, BLOCK_WIDTH, BLOCK_HEIGHT, SRC_INDEXED, scrap_texels[i], "", (unsigned)scrap_texels[i], TEXPREF_ALPHA | TEXPREF_OVERWRITE | TEXPREF_NOPICMIP);
	}
	scrap_dirty = false;
}


//=============================================================================
/* Support Routines */

typedef struct cachepic_s
{
	char		name[MAX_QPATH];
	qpic_t		pic;
	byte		padding[32];	// for appended glpic
} cachepic_t;

#define	MAX_CACHED_PICS		128
cachepic_t	menu_cachepics[MAX_CACHED_PICS];
int			menu_numcachepics;

byte		menuplyr_pixels[4096];

qpic_t *Draw_PicFromWad (char *name)
{
	qpic_t	*p;
	glpic_t	*gl;
	unsigned offset; //johnfitz
	char texturename[64]; //johnfitz

	p = W_GetLumpName (name);

	// Sanity ...
	if (p->width & 0xC0000000 || p->height & 0xC0000000)
		Sys_Error ("Draw_PicFromWad: invalid dimensions (%dx%d) for '%s'", p->width, p->height, name);

	gl = (glpic_t *)p->data;

	// load little ones into the scrap
	if (p->width < 64 && p->height < 64)
	{
		int		x, y;
		int		i, j, k;
		int		texnum;

		texnum = Scrap_AllocBlock (p->width, p->height, &x, &y);
		if (texnum == -1)
			Sys_Error ("Scrap_AllocBlock: full");

		scrap_dirty = true;
		k = 0;
		for (i=0 ; i<p->height ; i++)
			for (j=0 ; j<p->width ; j++, k++)
				scrap_texels[texnum][(y+i)*BLOCK_WIDTH + x + j] = p->data[k];

		gl->gltexture = scrap_textures[texnum]; // changed to an array
		// no longer go from 0.01 to 0.99
		gl->sl = x/(float)BLOCK_WIDTH;
		gl->sh = (x+p->width)/(float)BLOCK_WIDTH;
		gl->tl = y/(float)BLOCK_WIDTH;
		gl->th = (y+p->height)/(float)BLOCK_WIDTH;
	}
	else
	{
		sprintf (texturename, "%s:%s", WADFILE, name); //johnfitz
		offset = (unsigned)p - (unsigned)wad_base + sizeof(int)*2; //johnfitz
		gl->gltexture = GL_LoadTexture (NULL, texturename, p->width, p->height, SRC_INDEXED, p->data, WADFILE, offset, TEXPREF_ALPHA | TEXPREF_PAD | TEXPREF_NOPICMIP);
		gl->sl = 0;
		gl->sh = 1;
		gl->tl = 0;
		gl->th = 1;
	}
	return p;
}


/*
================
Draw_CachePic
================
*/
qpic_t *Draw_CachePic (char *path)
{
	cachepic_t	*pic;
	int			i;
	qpic_t		*dat;
	glpic_t		gl;

	for (pic=menu_cachepics, i=0 ; i<menu_numcachepics ; pic++, i++)
		if (!strcmp (path, pic->name))
			return &pic->pic;

	if (menu_numcachepics == MAX_CACHED_PICS)
		Sys_Error ("Draw_CachePic: menu_numcachepics == MAX_CACHED_PICS (%d)", MAX_CACHED_PICS);
	menu_numcachepics++;
	strcpy (pic->name, path);

//
// load the pic from disk
//
	dat = (qpic_t *)COM_LoadTempFile (path, NULL);
	if (!dat)
		Sys_Error ("Draw_CachePic: failed to load %s", path);
	SwapPic (dat);

	// HACK HACK HACK --- we need to keep the bytes for
	// the translatable player picture just for the menu
	// configuration dialog
	if (!strcmp (path, "gfx/menuplyr.lmp"))
		memcpy (menuplyr_pixels, dat->data, dat->width*dat->height);

	pic->pic.width = dat->width;
	pic->pic.height = dat->height;

	// fix gcc warnings
	gl.gltexture = GL_LoadTexture (NULL, path, dat->width, dat->height, SRC_INDEXED, dat->data, path, sizeof(int)*2, TEXPREF_ALPHA | TEXPREF_PAD | TEXPREF_NOPICMIP);
	gl.sl = 0;
	gl.sh = 1;
	gl.tl = 0;
	gl.th = 1;
	memcpy (pic->pic.data, &gl, sizeof(glpic_t));

	return &pic->pic;
}


/*
===============
GL_SetFilterModes
===============
*/
void GL_SetFilterModes (gltexture_t *glt)
{
//	GL_Bind (glt);

	if (glt->flags & TEXPREF_NEAREST)
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}
	else if (glt->flags & TEXPREF_LINEAR)
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
	else if (glt->flags & TEXPREF_MIPMAP)
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, modes[gl_texturemode].magfilter);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, modes[gl_texturemode].minfilter);
	}
	else
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, modes[gl_texturemode].magfilter);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, modes[gl_texturemode].magfilter);
	}

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_texture_anisotropy);
}

/*
===============
GL_TextureMode_f
===============
*/
void GL_TextureMode_f (void)
{
	int i;
	char *arg;
	gltexture_t *glt;

	if (Cmd_Argc() == 1)
	{
		for (i=0 ; i< MAXGLMODES ; i++)
		{
			if (gl_filter_min == modes[i].minfilter)
			{
				Con_Printf ("gl_texturemode is %s (%d)\n", modes[i].name, i + 1);
				return;
			}
		}
	}

	for (i=0 ; i< MAXGLMODES ; i++)
	{
		arg = Cmd_Argv(1);
		if (!strcasecmp (modes[i].name, arg ) || (isdigit(*arg) && atoi(arg) - 1 == i))
			break;
	}
	if (i == MAXGLMODES)
	{
		Con_Printf ("bad filter name, available are:\n");
		for (i=0 ; i< MAXGLMODES ; i++)
			Con_Printf ("%s (%d)\n", modes[i].name, i + 1);
		return;
	}

	gl_texturemode = i;
	gl_filter_min = modes[gl_texturemode].minfilter;
	gl_filter_mag = modes[gl_texturemode].magfilter;

	// change all the existing texture objects
	for (glt=active_gltextures; glt; glt=glt->next) 
    {
        GL_Bind (glt);
		GL_SetFilterModes (glt);
    }

	Sbar_Changed (); // sbar graphics need to be redrawn with new filter mode
}

/*
===============
GL_Texture_Anisotropy_f
===============
*/
void GL_Texture_Anisotropy_f (void)
{
	gltexture_t	*glt;

	if (Cmd_Argc() == 1)
	{
		Con_Printf ("gl_texture_anisotropy is %f, hardware limit is %f\n", gl_texture_anisotropy, gl_hardware_max_anisotropy);
		return;
	}

	gl_texture_anisotropy = CLAMP(1.0f, atof (Cmd_Argv(1)), gl_hardware_max_anisotropy);

	for (glt=active_gltextures; glt; glt=glt->next) 
    {
        GL_Bind (glt);
		GL_SetFilterModes (glt);
    }
}


/*
===============
Pics_Upload
===============
*/
void Pics_Upload (void)
{
	unsigned	offset; // johnfitz
	char		texturename[64]; //johnfitz

	// load the console background and the charset
	// by hand, because we need to write the version
	// string into the background before turning
	// it into a texture
	draw_chars = W_GetLumpName ("conchars");

	if (!draw_chars)
		Sys_Error ("Pics_Upload: couldn't load conchars");

	// now turn them into textures
	sprintf (texturename, "%s:%s", WADFILE, "conchars"); // johnfitz
	offset = (unsigned)draw_chars - (unsigned)wad_base;
	char_texture = GL_LoadTexture (NULL, texturename, 128, 128, SRC_INDEXED, draw_chars, WADFILE, offset, TEXPREF_ALPHA | TEXPREF_NEAREST | TEXPREF_NOPICMIP | TEXPREF_CONCHARS);

	//
	// get the other pics we need
	//
	draw_disc = Draw_PicFromWad ("disc");
	draw_backtile = Draw_PicFromWad ("backtile");
}


/*
===============
Draw_Init

must be called before any texture loading
===============
*/
void Draw_Init (void)
{
	int i;
	static byte notexture_data[16] = {159,91,83,255,0,0,0,255,0,0,0,255,159,91,83,255}; // black and pink checker
	static byte nulltexture_data[16] = {127,191,255,255,0,0,0,255,0,0,0,255,127,191,255,255}; // black and blue checker

	// init texture list
	free_gltextures = (gltexture_t *) Hunk_AllocName (MAX_GLTEXTURES * sizeof(gltexture_t), "gltextures");
	active_gltextures = NULL;
	for (i=0; i<MAX_GLTEXTURES-1; i++)
		free_gltextures[i].next = &free_gltextures[i+1];
	free_gltextures[i].next = NULL;
	numgltextures = 0;

	Cvar_RegisterVariable (&scr_conalpha, NULL);
	Cvar_RegisterVariable (&gl_picmip, NULL);
	Cvar_RegisterVariable (&gl_warp_image_size, GL_UploadWarpImage);

	Cmd_AddCommand ("gl_texturemode", &GL_TextureMode_f);
	Cmd_AddCommand ("gl_texture_anisotropy", &GL_Texture_Anisotropy_f);

	// load notexture images
	notexture = GL_LoadTexture (NULL, "notexture", 2, 2, SRC_RGBA, notexture_data, "", (unsigned)notexture_data, TEXPREF_NEAREST | TEXPREF_PERSIST | TEXPREF_NOPICMIP);
	nulltexture = GL_LoadTexture (NULL, "nulltexture", 2, 2, SRC_RGBA, nulltexture_data, "", (unsigned)nulltexture_data, TEXPREF_NEAREST | TEXPREF_PERSIST | TEXPREF_NOPICMIP);

	// have to assign these here because R_InitTextures is called before Draw_Init
	notexture_mip->gltexture = notexture_mip2->gltexture = notexture;

	// upload warpimage
	GL_UploadWarpImage ();

	// clear scrap and allocate gltextures
	memset(&scrap_allocated, 0, sizeof(scrap_allocated));
	memset(&scrap_texels, 255, sizeof(scrap_texels));
	Scrap_Upload (); // creates 2 empty textures

	// load pics
	Pics_Upload ();
}

/*
===============
Draw_Crosshair
===============
*/
void Draw_Crosshair (void)
{
	if (!crosshair.value) 
		return;

	Draw_Character (scr_vrect.x + scr_vrect.width/2 + cl_crossx.value, scr_vrect.y + scr_vrect.height/2 + cl_crossy.value, '+');
}

/*
================
Draw_CharacterQuad

seperate function to spit out verts
================
*/
void Draw_CharacterQuad (int x, int y, char num)
{
	int				row, col;
	float			frow, fcol, size;

	row = num>>4;
	col = num&15;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.0625;

	glTexCoord2f (fcol, frow);
	glVertex2f (x, y);
	glTexCoord2f (fcol + size, frow);
	glVertex2f (x+8, y);
	glTexCoord2f (fcol + size, frow + size);
	glVertex2f (x+8, y+8);
	glTexCoord2f (fcol, frow + size);
	glVertex2f (x, y+8);
}

/*
================
Draw_Character

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.

modified to call Draw_CharacterQuad
================
*/
void Draw_Character (int x, int y, int num)
{
	if (y <= -8)
		return;			// totally off screen

	num &= 255;

	if (num == 32)
		return; // don't waste verts on spaces

	GL_Bind (char_texture);
	glBegin (GL_QUADS);

	Draw_CharacterQuad (x, y, (char) num);

	glEnd ();
}

/*
================
Draw_String

modified to call Draw_CharacterQuad
================
*/
void Draw_String (int x, int y, char *str)
{
	if (y <= -8)
		return;			// totally off screen

	GL_Bind (char_texture);
	glBegin (GL_QUADS);

	while (*str)
	{
		if (*str != 32) // don't waste verts on spaces
			Draw_CharacterQuad (x, y, *str);
		str++;
		x += 8;
	}

	glEnd ();
}


/*
=============
Draw_AlphaPic
=============
*/
void Draw_AlphaPic (int x, int y, qpic_t *pic, float alpha)
{
	glpic_t			*gl;

	if (scrap_dirty)
		Scrap_Upload ();

	gl = (glpic_t *)pic->data;

	glDisable (GL_ALPHA_TEST);
	glEnable (GL_BLEND);
	glColor4f (1,1,1,alpha);
	GL_Bind (gl->gltexture);
	glBegin (GL_QUADS);
	glTexCoord2f (gl->sl, gl->tl);
	glVertex2f (x, y);
	glTexCoord2f (gl->sh, gl->tl);
	glVertex2f (x+pic->width, y);
	glTexCoord2f (gl->sh, gl->th);
	glVertex2f (x+pic->width, y+pic->height);
	glTexCoord2f (gl->sl, gl->th);
	glVertex2f (x, y+pic->height);
	glEnd ();
	glColor4f (1,1,1,1);
	glEnable (GL_ALPHA_TEST);
	glDisable (GL_BLEND);
}


/*
=============
Draw_Pic
=============
*/
void Draw_Pic (int x, int y, qpic_t *pic)
{
	glpic_t			*gl;

	if (scrap_dirty)
		Scrap_Upload ();

	gl = (glpic_t *)pic->data;

	glDisable (GL_ALPHA_TEST); //FX new
	glEnable (GL_BLEND); //FX
	glColor4f (1,1,1,1);
	GL_Bind (gl->gltexture);
	glBegin (GL_QUADS);
	glTexCoord2f (gl->sl, gl->tl);
	glVertex2f (x, y);
	glTexCoord2f (gl->sh, gl->tl);
	glVertex2f (x+pic->width, y);
	glTexCoord2f (gl->sh, gl->th);
	glVertex2f (x+pic->width, y+pic->height);
	glTexCoord2f (gl->sl, gl->th);
	glVertex2f (x, y+pic->height);
	glEnd ();
	glEnable (GL_ALPHA_TEST); //FX new
	glDisable (GL_BLEND); //FX
}


/*
=============
Draw_SubPic
=============
*/
void Draw_SubPic (int x, int y, qpic_t *pic, int srcx, int srcy, int width, int height)
{
	float			newsl, newtl, newsh, newth, oldglwidth, oldglheight; 
	glpic_t			*gl;

	if (scrap_dirty)
		Scrap_Upload ();

	gl = (glpic_t *)pic->data;

	oldglwidth = gl->sh - gl->sl;
	oldglheight = gl->th - gl->tl;

	newsl = gl->sl + (srcx * oldglwidth) / pic->width;
	newsh = newsl + (width * oldglwidth) / pic->width;

	newtl = gl->tl + (srcy * oldglheight) / pic->height;
	newth = newtl + (height * oldglheight) / pic->height; 

	glDisable (GL_ALPHA_TEST); //FX new
	glEnable (GL_BLEND); //FX
	glColor4f (1,1,1,1);
	GL_Bind (gl->gltexture);
	glBegin (GL_QUADS);
	glTexCoord2f (newsl, newtl);
	glVertex2f (x, y);
	glTexCoord2f (newsh, newtl);
	glVertex2f (x+width, y);
	glTexCoord2f (newsh, newth);
	glVertex2f (x+width, y+height);
	glTexCoord2f (newsl, newth);
	glVertex2f (x, y+height);
	glEnd ();
	glEnable (GL_ALPHA_TEST); //FX new
	glDisable (GL_BLEND); //FX
}


/*
=============
Draw_TransPic
=============
*/
void Draw_TransPic (int x, int y, qpic_t *pic)
{
	if (x < 0 || (x + pic->width) > vid.width || y < 0 || (y + pic->height) > vid.height)
	{
		Sys_Error ("Draw_TransPic: bad coordinates (%d, %d)", x, y);
	}

	Draw_Pic (x, y, pic);
}

/*
=============
Draw_TransPicTranslate

Only used for the player color selection menu
=============
*/
void Draw_TransPicTranslate (int x, int y, qpic_t *pic, byte *translation)
{
	int		size, mark;
	int		i;
	byte	*dst;
	byte	*src;
	byte	*data;
	byte	*trans = NULL;
	char	name[64];
	glpic_t	*gl;

	mark = Hunk_LowMark ();

	data = menuplyr_pixels;
	sprintf (name, "gfx/menuplyr.lmp");
	size = pic->width * pic->height;

	// allocate dynamic memory
	trans = Hunk_Alloc (size);

	dst = trans;
	src = data;

	for (i=0; i<size; i++)
		*dst++ = translation[*src++];

	data = trans;

	gl = (glpic_t *)pic->data;
	gl->gltexture = GL_LoadTexture (NULL, name, pic->width, pic->height, SRC_INDEXED, data, "", (unsigned)data, TEXPREF_ALPHA | TEXPREF_PAD | TEXPREF_NOPICMIP);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	Draw_Pic (x, y, pic);

	// free allocated memory
	Hunk_FreeToLowMark (mark);
}


/*
================
Draw_ConsoleBackground

================
*/
void Draw_ConsoleBackground (int lines)
{
	qpic_t *pic;
	int y;
	float alpha;

	pic = Draw_CachePic ("gfx/conback.lmp");
	pic->width = vid.width;
	pic->height = vid.height;

	alpha = (con_forcedup) ? 1.0 : CLAMP(0.0, scr_conalpha.value, 1.0);

	y = (vid.height * 3) >> 2;

	if (lines > y)
		Draw_Pic(0, lines - vid.height, pic);
	else
//		Draw_AlphaPic (0, lines - vid.height, pic, (float)(2 * alpha * lines)/y); //alpha depend on height console
		Draw_AlphaPic (0, lines - vid.height, pic, alpha);
}


/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void Draw_TileClear (int x, int y, int w, int h)
{
	glpic_t	*gl;

	gl = (glpic_t *)draw_backtile->data;

	glDisable (GL_ALPHA_TEST); //FX new
	glEnable (GL_BLEND); //FX
//	glColor3f (1,1,1);
	glColor4f (1,1,1,1); //FX new
	GL_Bind (gl->gltexture);
	glBegin (GL_QUADS);
	glTexCoord2f (x/64.0, y/64.0);
	glVertex2f (x, y);
	glTexCoord2f ( (x+w)/64.0, y/64.0);
	glVertex2f (x+w, y);
	glTexCoord2f ( (x+w)/64.0, (y+h)/64.0);
	glVertex2f (x+w, y+h);
	glTexCoord2f ( x/64.0, (y+h)/64.0 );
	glVertex2f (x, y+h);
	glEnd ();
	glEnable (GL_ALPHA_TEST); //FX new
	glDisable (GL_BLEND); //FX
}


/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill (int x, int y, int w, int h, int c)
{
	byte *pal = (byte *)d_8to24table; // use d_8to24table instead of host_basepal
	float alpha = 1.0;

	glDisable (GL_TEXTURE_2D);
	glEnable (GL_BLEND); // for alpha
	glDisable (GL_ALPHA_TEST); // for alpha
	glColor4f (pal[c*4]/255.0, pal[c*4+1]/255.0, pal[c*4+2]/255.0, alpha); // added alpha

	glBegin (GL_QUADS);
	glVertex2f (x, y);
	glVertex2f (x+w, y);
	glVertex2f (x+w, y+h);
	glVertex2f (x, y+h);
	glEnd ();

	glColor3f (1,1,1);
	glDisable (GL_BLEND); // for alpha
	glEnable (GL_ALPHA_TEST); // for alpha
	glEnable (GL_TEXTURE_2D);
}
//=============================================================================

/*
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen (void)
{
	glEnable (GL_BLEND);
	glDisable (GL_ALPHA_TEST);
	glDisable (GL_TEXTURE_2D);
	glColor4f (0, 0, 0, 0.5);

	glBegin (GL_QUADS);
	glVertex2f (0, 0);
	glVertex2f (vid.width, 0);
	glVertex2f (vid.width, vid.height);
	glVertex2f (0, vid.height);
	glEnd ();

	glColor4f (1,1,1,1);
	glEnable (GL_TEXTURE_2D);
	glEnable (GL_ALPHA_TEST);
	glDisable (GL_BLEND);

	Sbar_Changed();
}

//=============================================================================

/*
================
Draw_BeginDisc

Draws the little blue disc in the corner of the screen.
Call before beginning any disc IO.
================
*/
void Draw_BeginDisc (void)
{
	if (!draw_disc || isIntel) // intel video workaround
		return;

	glDrawBuffer (GL_FRONT);
	Draw_Pic (vid.width - 24, 0, draw_disc);
	glDrawBuffer (GL_BACK);
}


/*
================
Draw_EndDisc

Erases the disc icon.
Call after completing any disc IO
================
*/
void Draw_EndDisc (void)
{
	
}

/*
================
GL_Set2D

Setup as if the screen was 320*200
================
*/
void GL_Set2D (void)
{
	//
	// set up viewpoint
	//
	glViewport (glx, gly, glwidth, glheight);

	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();

	glOrtho (0, glwidth, glheight, 0, -99999, 99999);

	glMatrixMode (GL_MODELVIEW);
	glLoadIdentity ();

	//
	// set drawing parms
	//
	glDisable (GL_DEPTH_TEST);
	glDisable (GL_CULL_FACE);
	glDisable (GL_BLEND);
	glEnable (GL_ALPHA_TEST);

	glColor4f (1,1,1,1);
}

//====================================================================


/*
================
GL_ResampleTextureQuality

bilinear resample
================
*/
void GL_ResampleTextureQuality (unsigned *in, int inwidth, int inheight, unsigned *out,  int outwidth, int outheight, qboolean alpha)
{
	byte	 *nwpx, *nepx, *swpx, *sepx, *dest, *inlimit;
	unsigned xfrac, yfrac, x, y, modx, mody, imodx, imody, injump, outjump;
	int	 i, j;

	// Sanity ...
	if (inwidth <= 0 || inheight <= 0 || outwidth <= 0 || outheight <= 0 ||
		inwidth * 0x10000 & 0xC0000000 || inheight * outheight & 0xC0000000 ||
		inwidth * inheight & 0xC0000000)
		Sys_Error ("GL_ResampleTextureQuality: invalid parameters (in:%dx%d, out:%dx%d)", inwidth, inheight, outwidth, outheight);

	inlimit = (byte *)(in + inwidth * inheight);

	// Make sure "out" size is at least 2x2!
	xfrac = ((inwidth-1) << 16) / (outwidth-1);
	yfrac = ((inheight-1) << 16) / (outheight-1);
	y = outjump = 0;

	// Better resampling, less blurring of all texes, requires a lot of memory
	for (i=0; i<outheight; i++)
	{
		mody = (y>>8) & 0xFF;
		imody = 256 - mody;
		injump = (y>>16) * inwidth;
		x = 0;

		for (j=0; j<outwidth; j++)
		{
			modx = (x>>8) & 0xFF;
			imodx = 256 - modx;

			nwpx = (byte *)(in + (x>>16) + injump);
			nepx = nwpx + sizeof(int);
			swpx = nwpx + inwidth * sizeof(int); // Next line

			// Don't exceed "in" size
			if (swpx + sizeof(int) >= inlimit)
			{
//				Con_Error ("GL_ResampleTextureQuality: %4d\n", swpx + sizeof(int) - inlimit);
				swpx = nwpx; // There's no next line
			}

			sepx = swpx + sizeof(int);

			dest = (byte *)(out + outjump + j);

			dest[0] = (nwpx[0]*imodx*imody + nepx[0]*modx*imody + swpx[0]*imodx*mody + sepx[0]*modx*mody)>>16;
			dest[1] = (nwpx[1]*imodx*imody + nepx[1]*modx*imody + swpx[1]*imodx*mody + sepx[1]*modx*mody)>>16;
			dest[2] = (nwpx[2]*imodx*imody + nepx[2]*modx*imody + swpx[2]*imodx*mody + sepx[2]*modx*mody)>>16;
			if (alpha)
				dest[3] = (nwpx[3]*imodx*imody + nepx[3]*modx*imody + swpx[3]*imodx*mody + sepx[3]*modx*mody)>>16;
			else
				dest[3] = 255;

			x += xfrac;
		}
		outjump += outwidth;
		y += yfrac;
	}
}

/*
================
GL_MipMap

Operates in place, quartering the size of the texture
================
*/
void GL_MipMap (byte *in, int width, int height)
{
	int		i, j;
	byte	*out;

	width <<=2;
	height >>= 1;
	out = in;

	for (i=0 ; i<height ; i++, in+=width)
	{
		for (j=0 ; j<width ; j+=8, out+=4, in+=8)
		{
			out[0] = (in[0] + in[4] + in[width+0] + in[width+4])>>2;
			out[1] = (in[1] + in[5] + in[width+1] + in[width+5])>>2;
			out[2] = (in[2] + in[6] + in[width+2] + in[width+6])>>2;
			out[3] = (in[3] + in[7] + in[width+3] + in[width+7])>>2;
		}
	}
}

/*
===============
GL_AlphaEdgeFix

eliminate pink edges on sprites, etc.
operates in place on 32bit data
===============
*/
void GL_AlphaEdgeFix (byte *data, int width, int height)
{
	int i,j,n=0,b,c[3]={0,0,0},lastrow,thisrow,nextrow,lastpix,thispix,nextpix;
	byte *dest = data;

	for (i=0; i<height; i++)
	{
		lastrow = width * 4 * ((i == 0) ? height-1 : i-1);
		thisrow = width * 4 * i;
		nextrow = width * 4 * ((i == height-1) ? 0 : i+1);

		for (j=0; j<width; j++, dest+=4)
		{
			if (dest[3]) // not transparent
				continue;

			lastpix = 4 * ((j == 0) ? width-1 : j-1);
			thispix = 4 * j;
			nextpix = 4 * ((j == width-1) ? 0 : j+1);

			b = lastrow + lastpix; if (data[b+3]) {c[0] += data[b]; c[1] += data[b+1]; c[2] += data[b+2]; n++;}
			b = thisrow + lastpix; if (data[b+3]) {c[0] += data[b]; c[1] += data[b+1]; c[2] += data[b+2]; n++;}
			b = nextrow + lastpix; if (data[b+3]) {c[0] += data[b]; c[1] += data[b+1]; c[2] += data[b+2]; n++;}
			b = lastrow + thispix; if (data[b+3]) {c[0] += data[b]; c[1] += data[b+1]; c[2] += data[b+2]; n++;}
			b = nextrow + thispix; if (data[b+3]) {c[0] += data[b]; c[1] += data[b+1]; c[2] += data[b+2]; n++;}
			b = lastrow + nextpix; if (data[b+3]) {c[0] += data[b]; c[1] += data[b+1]; c[2] += data[b+2]; n++;}
			b = thisrow + nextpix; if (data[b+3]) {c[0] += data[b]; c[1] += data[b+1]; c[2] += data[b+2]; n++;}
			b = nextrow + nextpix; if (data[b+3]) {c[0] += data[b]; c[1] += data[b+1]; c[2] += data[b+2]; n++;}

			// average all non-transparent neighbors
			if (n)
			{
				dest[0] = (byte)(c[0]/n);
				dest[1] = (byte)(c[1]/n);
				dest[2] = (byte)(c[2]/n);

				n = c[0] = c[1] = c[2] = 0;
			}
		}
	}
}

/*
================
GL_ScaleSize
================
*/
int GL_ScaleSize (int oldsize, qboolean force)
{
	int newsize, nextsize;

	if (force)
		nextsize = oldsize;
	else
		nextsize = 3 * oldsize / 2; // Avoid unfortunate resampling

	for (newsize = 1; newsize < nextsize && newsize != oldsize; newsize <<= 1)
		;

	return newsize;
}

/*
===============
GL_Upload32

handles 32bit source data
===============
*/
void GL_Upload32 (gltexture_t *glt, unsigned *data)
{
	int			internalformat;
	int			scaled_width, scaled_height;
	int			picmip;
	unsigned	*scaled = NULL;

	scaled_width = GL_ScaleSize (glt->width, false);
	scaled_height = GL_ScaleSize (glt->height, false);

	if (glt->width && glt->height) // Don't process 0-sized images
	{
		// Preserve proportions
		while (glt->width > glt->height && scaled_width < scaled_height)
			scaled_width *= 2;

		while (glt->width < glt->height && scaled_width > scaled_height)
			scaled_height *= 2;
	}

	// Note: Can't use Con_Printf here!
	if (developer.value > 1 && (scaled_width != GL_ScaleSize (glt->width, true) || scaled_height != GL_ScaleSize (glt->height, true)))
		Con_DPrintf ("GL_Upload32: in:%dx%d, out:%dx%d, '%s'\n", glt->width, glt->height, scaled_width, scaled_height, glt->name);

	// Prevent too large or too small images (might otherwise crash resampling)
	scaled_width = CLAMP(2, scaled_width, gl_texture_max_size);
	scaled_height = CLAMP(2, scaled_height, gl_texture_max_size);

	// allocate dynamic memory
	scaled = Hunk_Alloc (scaled_width * scaled_height * sizeof(unsigned)); // 4

	// Resample up
	if (glt->width && glt->height) // Don't resample 0-sized images
		GL_ResampleTextureQuality (data, glt->width, glt->height, scaled, scaled_width, scaled_height, (glt->flags & TEXPREF_ALPHA));
	else
		memcpy (scaled, data, scaled_width * scaled_height * rgba_bytes); // FIXME: 0-sized texture, so we just copy it

	// mipmap down
	picmip = (glt->flags & TEXPREF_NOPICMIP) ? 0 : max((int)gl_picmip.value, 0);
	if (glt->flags & TEXPREF_MIPMAP)
	{
		int i;

		// Only affect mipmapped texes, typically not console graphics
		for (i = 0; i < picmip && (scaled_width > 1 || scaled_height > 1); ++i)
		{
			GL_MipMap ((byte *)scaled, scaled_width, scaled_height);
			scaled_width >>= 1;
			scaled_height >>= 1;
			scaled_width = max(scaled_width, 1);
			scaled_height = max(scaled_height, 1);

			if (glt->flags & TEXPREF_ALPHA)
				GL_AlphaEdgeFix ((byte *)scaled, scaled_width, scaled_height);
		}
	}

	// upload
	GL_Bind (glt);
	internalformat = (glt->flags & TEXPREF_ALPHA) ? GL_RGBA : GL_RGB;
	glTexImage2D (GL_TEXTURE_2D, 0, internalformat, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);

	// upload mipmaps
	if (glt->flags & TEXPREF_MIPMAP)
	{
		int miplevel = 0;

		while (scaled_width > 1 || scaled_height > 1)
		{
			GL_MipMap ((byte *)scaled, scaled_width, scaled_height);
			scaled_width >>= 1;
			scaled_height >>= 1;
			scaled_width = max(scaled_width, 1);
			scaled_height = max(scaled_height, 1);

			miplevel++;
			glTexImage2D (GL_TEXTURE_2D, miplevel, internalformat, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
		}
	}

	// set filter modes
	GL_SetFilterModes (glt);

	// free allocated memory
}

/*
================
GL_UploadBloom

handles bloom data
================
*/
void GL_UploadBloom (gltexture_t *glt, unsigned *data)
{
	// upload it
	GL_Bind (glt);
	glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, glt->width, glt->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	// set filter modes
	GL_SetFilterModes (glt);
}

/*
================
GL_UploadLightmap

handles lightmap data
================
*/
void GL_UploadLightmap (gltexture_t *glt, byte *data)
{
	// upload it
	GL_Bind (glt);
	glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, glt->width, glt->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	// set filter modes
	GL_SetFilterModes (glt);
}

/*
===============
GL_Upload8

handles 8bit source data, then passes it to GL_Upload32
===============
*/
void GL_Upload8 (gltexture_t *glt, byte *data)
{
	int			i, size;
	int			p;
	unsigned	*trans = NULL;
	unsigned int	*pal;

	// HACK HACK HACK -- taken from fitzquake
	if (strstr(glt->name, "shot1sid") && glt->width==32 && glt->height==32 && CRC_Block(data, 1024) == 65393)
	{
		// This texture in b_shell1.bsp has some of the first 32 pixels painted white.
		// They are invisible in software, but look really ugly in GL. So we just copy
		// 32 pixels from the bottom to make it look nice.
		memcpy (data, data + 32*31, 32);
	}

	size = glt->width * glt->height;
	if (size & 3)
		Con_DWarning ("GL_Upload8: size %d is not a multiple of 4 in '%s'\n", size, glt->name); // should be an error but ... (EER1)

	// allocate dynamic memory
	trans = Hunk_Alloc (size * sizeof(unsigned)); // 4

	// detect false alpha cases
	if (glt->flags & TEXPREF_ALPHA && !(glt->flags & TEXPREF_CONCHARS))
	{
		for (i=0 ; i<size ; i++)
		{
			p = data[i];
			if (p == 255) // transparent index
				break;
		}
		if (i == size)
			glt->flags &= ~TEXPREF_ALPHA;
	}

	// choose palette /* and convert to 32bit */
	if (glt->flags & TEXPREF_FULLBRIGHT)
	{
		if (glt->flags & TEXPREF_ALPHA)
			pal = d_8to24table_fbright_fence;
		else
			pal = d_8to24table_fbright;
	}
	else if (glt->flags & TEXPREF_NOBRIGHT)
	{
		if (glt->flags & TEXPREF_ALPHA)
			pal = d_8to24table_nobright_fence;
		else
			pal = d_8to24table_nobright;
	}
	else if (glt->flags & TEXPREF_CONCHARS)
	{
		pal = d_8to24table_conchars;
	}
	else
	{
		pal = d_8to24table;
	}

	// convert to 32bit
	for (i=0 ; i<size ; ++i)
	{
		p = data[i];
		trans[i] = pal[p];
	}

	// fix edges
	if (glt->flags & TEXPREF_ALPHA)
		GL_AlphaEdgeFix ((byte *)trans, glt->width, glt->height);

	// upload it
	GL_Upload32 (glt, trans);

	// free allocated memory
}


/*
================
GL_FindTexture
================
*/
gltexture_t *GL_FindTexture (model_t *owner, char *name)
{
	gltexture_t	*glt;

	if (name)
	{
		for (glt = active_gltextures; glt; glt = glt->next)
			if (glt->owner == owner && !strcmp (glt->name, name))
				return glt;
	}

	return NULL;
}


/*
================
GL_NewTexture
================
*/
gltexture_t *GL_NewTexture (void)
{
	gltexture_t *glt;

	if (numgltextures >= MAX_GLTEXTURES)
		Sys_Error ("GL_NewTexture: cache full, max is %i textures", MAX_GLTEXTURES);

	glt = free_gltextures;
	free_gltextures = glt->next;
	glt->next = active_gltextures;
	active_gltextures = glt;

	glGenTextures(1, &glt->texnum);
	numgltextures++;
	return glt;
}


/*
================
GL_FreeTexture
================
*/
void GL_FreeTexture (gltexture_t *purge)
{
	gltexture_t *glt;

	if (purge == NULL)
	{
		Con_SafePrintf ("GL_FreeTexture: NULL texture\n");
		return;
	}

	if (active_gltextures == purge)
	{
		active_gltextures = purge->next;
		purge->next = free_gltextures;
		free_gltextures = purge;

		GL_DeleteTexture(purge);
		numgltextures--;
		return;
	}

	for (glt = active_gltextures; glt; glt = glt->next)
	{
		if (glt->next == purge)
		{
			glt->next = purge->next;
			purge->next = free_gltextures;
			free_gltextures = purge;

            GL_DeleteTexture(purge);
			numgltextures--;
			return;
		}
	}

	Con_SafePrintf ("GL_FreeTexture: not found\n");
}

/*
================
GL_FreeTextures
================
*/
void GL_FreeTextures (model_t *owner)
{
	gltexture_t *glt, *next;

	for (glt = active_gltextures; glt; glt = next)
	{
		next = glt->next;
		if (glt && glt->owner == owner)
			GL_FreeTexture (glt);
	}
}

/*
================
GL_LoadTexture

the one entry point for loading all textures
================
*/
gltexture_t *GL_LoadTexture (model_t *owner, char *name, int width, int height, enum srcformat format, byte *data, char *source_file, unsigned source_offset, unsigned flags)
{
	int size = 0; // keep compiler happy
	gltexture_t	*glt;
	unsigned short crc;
	int mark;

	if (cls.state == ca_dedicated)
		return NULL; // No textures in dedicated mode

	// check format size
	size = width * height;
	switch (format)
	{
	case SRC_INDEXED:
		size *= indexed_bytes;
		break;
	case SRC_LIGHTMAP:
		size *= lightmap_bytes;
		break;
	case SRC_RGBA:
		size *= rgba_bytes;
		break;
	case SRC_BLOOM:
		size *= rgba_bytes;
		break;
	}

	if (size == 0)
		Con_DWarning ("GL_LoadTexture: texture '%s' has zero size\n", name);

	// Sanity check, max = 32kx32k
	if (width < 0 || height < 0 || size > 0x40000000)
		Sys_Error ("GL_LoadTexture: texture '%s' has invalid size (%dM, max = %dM)", name, size / (1024 * 1024), 0x40000000 / (1024 * 1024));

	// cache check
	crc = CRC_Block(data, size);

	if ((flags & TEXPREF_OVERWRITE) && (glt = GL_FindTexture (owner, name))) 
	{
		if (glt->source_crc == crc)
			return glt;
	}
	else
		glt = GL_NewTexture ();

	// copy data
	glt->owner = owner;
	strncpy (glt->name, name, sizeof(glt->name));
	glt->width = width;
	glt->height = height;
	glt->flags = flags;
	strncpy (glt->source_file, source_file, sizeof(glt->source_file));
	glt->source_offset = source_offset;
	glt->source_format = format;
	glt->source_width = width;
	glt->source_height = height;
	glt->source_crc = crc;

	//upload it
	mark = Hunk_LowMark ();

	switch (glt->source_format)
	{
	case SRC_INDEXED:
		GL_Upload8 (glt, data);
		break;
	case SRC_LIGHTMAP:
		GL_UploadLightmap (glt, data);
		break;
	case SRC_RGBA:
		GL_Upload32 (glt, (unsigned *)data);
		break;
	case SRC_BLOOM:
		GL_UploadBloom (glt, (unsigned *)data);
		break;
	}

	Hunk_FreeToLowMark (mark);

	return glt;
}


/*
================
GL_ReloadTexture

reloads a texture
================
*/
void GL_ReloadTexture (gltexture_t *glt)
{
	byte *data = NULL;
	int mark;
//
// get source data
//
	mark = Hunk_LowMark ();

	if (glt->source_file[0] && glt->source_offset)
	{
		// lump inside file
		FILE *f;
		int size;

		COM_FOpenFile(glt->source_file, &f, NULL);
		if (f)
		{
			fseek (f, glt->source_offset, SEEK_CUR);
	
			// check format size
			size = (glt->source_width * glt->source_height);
			switch (glt->source_format)
			{
			case SRC_INDEXED:
				size *= indexed_bytes;
				break;
			case SRC_LIGHTMAP:
				size *= lightmap_bytes;
				break;
			case SRC_RGBA:
				size *= rgba_bytes;
				break;
			case SRC_BLOOM:
				size *= rgba_bytes;
				break;
			}
	
			data = Hunk_Alloc (size);
			fread (data, 1, size, f);
			fclose (f);
		}
	}
	else if (glt->source_file[0] && !glt->source_offset)
		data = GL_LoadImage (glt->source_file, (int *)&glt->source_width, (int *)&glt->source_height); // simple file
	else if (!glt->source_file[0] && glt->source_offset)
		data = (byte *)glt->source_offset; // image in memory

	if (!data)
	{
		Con_Printf ("GL_ReloadTexture: invalid source for %s\n", glt->name);
		Hunk_FreeToLowMark (mark);
		return;
	}

	glt->width = glt->source_width;
	glt->height = glt->source_height;
//
// upload it
//
	switch (glt->source_format)
	{
	case SRC_INDEXED:
		GL_Upload8 (glt, data);
		break;
	case SRC_LIGHTMAP:
		GL_UploadLightmap (glt, data);
		break;
	case SRC_RGBA:
		GL_Upload32 (glt, (unsigned *)data);
		break;
	case SRC_BLOOM:
		GL_UploadBloom (glt, (unsigned *)data);
		break;
	}

	Hunk_FreeToLowMark (mark);
}

/*
================
GL_ReloadTextures_f

reloads all texture images.
================
*/
void GL_ReloadTextures_f (void)
{
	gltexture_t *glt;
    
	int mark;
	byte *dummy;
    
	for (glt=active_gltextures; glt; glt=glt->next)
	{
        if (!(glt->flags & TEXPREF_WARPIMAGE)) 
        {
            glGenTextures(1, &glt->texnum);
            GL_ReloadTexture (glt);
        }
	}
    
	mark = Hunk_LowMark ();
	
	dummy = Hunk_Alloc (gl_warpimage_size*gl_warpimage_size*4);
    
	for (glt=active_gltextures; glt; glt=glt->next)
	{
		if (glt->flags & TEXPREF_WARPIMAGE)
		{
			GL_Bind (glt);
			glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, gl_warpimage_size, gl_warpimage_size, 0, GL_RGBA, GL_UNSIGNED_BYTE, dummy);
			glt->width = glt->height = gl_warpimage_size;
            
            // set filter modes
            GL_SetFilterModes (glt);
		}
	}
    
	Hunk_FreeToLowMark (mark);
}

