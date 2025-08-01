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
// gl_texmgr.c -- texture manager. manages opengl texture images (adapted from fitzquake)

#include "quakedef.h"

cvar_t		gl_picmip = {"gl_picmip", "0", CVAR_NONE};
cvar_t		gl_warpimagesize = {"gl_warpimagesize", "256", CVAR_ARCHIVE}; // was 512, for water warp
cvar_t		gl_compression = {"gl_compression", "1", CVAR_ARCHIVE};

cvar_t		gl_swapinterval = {"gl_swapinterval", "1", CVAR_ARCHIVE};

gltexture_t	*notexture;
gltexture_t	*nulltexture;

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

int		gl_texturemode = 0; // nearest
int		gl_filter_min = GL_NEAREST; // was GL_LINEAR
int		gl_filter_mag = GL_NEAREST;

float	gl_hardware_max_anisotropy = 1; // just in case
float 	gl_texture_anisotropy = 1;

GLint		gl_hardware_max_size = 1024; // just in case

int		warpimage_size = 256; // fitzquake has 512, for water warp

#define	MAX_GLTEXTURES	4096 // orig was 1024, prev 2048
gltexture_t	*active_gltextures, *free_gltextures;
int			numgltextures;

static GLuint currenttexture[3] = {GL_UNUSED_TEXTURE, GL_UNUSED_TEXTURE, GL_UNUSED_TEXTURE}; // to avoid unnecessary texture sets
static GLenum currenttarget = GL_TEXTURE0_ARB;

unsigned int d_8to24table_original[256];		//standard unmodifyed palette
unsigned int d_8to24table_opaque[256];			//standard palette with alpha 255 for all colors
unsigned int d_8to24table[256];					//standard palette, 255 is transparent
unsigned int d_8to24table_fullbright[256];			//fullbright palette, 0-223 are black (for additive blending)
unsigned int d_8to24table_fullbright_holey[256];	//fullbright palette, for holey textures (fence)
unsigned int d_8to24table_nobright[256];		//nobright palette, 224-255 are black (for additive blending)
unsigned int d_8to24table_nobright_holey[256];	//nobright palette, for holey textures (fence)
unsigned int d_8to24table_conchars[256];		//conchars palette, 0 and 255 are transparent

unsigned int is_fullbright[256/32];

char *gl_vendor;
char *gl_renderer;
char *gl_version;
char *gl_extensions;

qboolean fullsbardraw = false;
qboolean isIntel = false; // intel video workaround

qboolean gl_texture_NPOT = false; //ericw
qboolean gl_texture_compression = false; // EER1

qboolean gl_swap_control = false;
int gl_stencilbits;
int gl_depthbits;
int gl_alphabits;
int gl_redbits;
int gl_greenbits;
int gl_bluebits;

/*
================================================

			OpenGL Stuff

================================================
*/


/*
===============
GL_CheckExtension
===============
*/
void GL_CheckExtension_Multitexture (void)
{
	qboolean ARBmultitexture = false;
	int units;
	
	//
	// Multitexture
	//
	ARBmultitexture = strstr (gl_extensions, "GL_ARB_multitexture") != NULL;
	
	if (ARBmultitexture)
	{
		// Check how many texture units there actually are
		glGetIntegerv (GL_MAX_TEXTURE_UNITS_ARB, &units);
		
		qglMultiTexCoord2f = (void *) qglGetProcAddress ("glMultiTexCoord2fARB");
		qglActiveTexture = (void *) qglGetProcAddress ("glActiveTextureARB");
		qglClientActiveTexture = (void *) qglGetProcAddress ("glClientActiveTextureARB");
		
		if (units < 3)
		{
			Sys_Error ("Only %i TMU available, but this engine requires minimum 3 TMUs", units);
		}
		else if (!qglMultiTexCoord2f || !qglActiveTexture || !qglClientActiveTexture)
		{
			Sys_Error ("Multitexture not supported (qglGetProcAddress failed)");
		}
		else
		{
			Con_Printf ("Found GL_ARB_multitexture\n");
			Con_Printf ("   %i TMUs on hardware\n", units);
		}
	}
	else
	{
		Sys_Error ("Multitexture not supported (extension not found)");
	}
}

void GL_CheckExtension_EnvCombine (void)
{
	qboolean EXTcombine, ARBcombine;
	
	//
	// Texture combine environment mode
	//
	ARBcombine = strstr (gl_extensions, "GL_ARB_texture_env_combine") != NULL;
	EXTcombine = strstr (gl_extensions, "GL_EXT_texture_env_combine") != NULL;
	
	if (ARBcombine || EXTcombine)
	{
		Con_Printf ("Found GL_%s_texture_env_combine\n", ARBcombine ? "ARB" : "EXT");
	}
	else
	{
		Sys_Error ("Texture combine environment not supported (extension not found)");
	}
}

void GL_CheckExtension_EnvAdd (void)
{
	qboolean EXTadd, ARBadd;
	
	//
	// Texture add environment mode
	//
	ARBadd = strstr (gl_extensions, "GL_ARB_texture_env_add") != NULL;
	EXTadd = strstr (gl_extensions, "GL_EXT_texture_env_add") != NULL;
	
	if (ARBadd || EXTadd)
	{
		Con_Printf ("Found GL_%s_texture_env_add\n", ARBadd ? "ARB" : "EXT");
	}
	else
	{
		Sys_Error ("Texture add environment not supported (extension not found)");
	}
}

void GL_CheckExtension_NPoT (void)
{
	qboolean npot;
	
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
		Con_Printf ("Found GL_ARB_texture_non_power_of_two\n");
		gl_texture_NPOT = true;
	}
	else
	{
		Con_Warning ("Texture non power of two not supported (extension not found)\n");
	}
}

void GL_CheckExtension_TextureCompression (void)
{
	qboolean ARBcompression, EXTcompression;
	
	//
	// Texture add environment mode
	//
	ARBcompression = strstr (gl_extensions, "GL_ARB_texture_compression") != NULL;
	EXTcompression = strstr (gl_extensions, "GL_EXT_texture_compression_s3tc") != NULL;
	
	if (COM_CheckParm("-nocompression"))
	{
		Con_Warning ("Texture compression disabled at command line\n");
	}
	else if (ARBcompression || EXTcompression)
	{
		qglCompressedTexImage2D = (void *) qglGetProcAddress ("glCompressedTexImage2D");
		
		if (!qglCompressedTexImage2D)
		{
			Con_Warning ("Texture compression not supported (qglGetProcAddress failed)\n");
		}
		else
		{
			if (ARBcompression)
			{
				// query for the available compression formats
				GLint num = 0;
				glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &num);
				if (num)
				{
					GLint formats[num];
					glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, formats);
					
					qboolean DXT1ext = false;
					qboolean DXT5ext = false;
					for (int i = 0; i < num; i++)
					{
						if (formats[i] == GL_COMPRESSED_RGB_S3TC_DXT1_EXT)
							DXT1ext = true;
						if (formats[i] == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT)
							DXT5ext = true;
					}
					
					if (DXT1ext && DXT5ext)
						gl_texture_compression = true;
				}
			}
			
			if (EXTcompression)
				gl_texture_compression = true;
			
			if (gl_texture_compression)
				Con_Printf ("Found GL_%s_texture_compression%s\n", ARBcompression ? "ARB" : "EXT", ARBcompression ? "" : "_s3tc");
			else
				Con_Warning ("Texture compression not supported (compression formats unavailable)\n");
		}
	}
	else
	{
		Con_Warning ("Texture compression not supported (extension not found)\n");
	}
}

void GL_CheckExtension_Anisotropy (void)
{
	qboolean anisotropy;
	
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
			Con_Printf ("Found GL_EXT_texture_filter_anisotropic\n");
		else
			Con_Warning ("Anisotropic filtering locked by driver. Current driver setting is %f\n", test1);
		
		// get max value either way, so the menu and stuff know it
		glGetFloatv (GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gl_hardware_max_anisotropy);
	}
	else
	{
		Con_Warning ("Anisotropic filtering not supported (extension not found)\n");
	}
}

#if defined __APPLE__ && defined __MACH__
GLint gl_swapintervalstate = 0;
#endif

void GL_CheckExtension_VSync (void)
{
	qboolean SWAPcontrol;
	
	//
	// Swap control
	//
#ifdef _WIN32
	SWAPcontrol = strstr (gl_extensions, SWAPCONTROLSTRING) != NULL;
#elif defined GLX_GLXEXT_PROTOTYPES
	SWAPcontrol = strstr (glx_extensions, SWAPCONTROLSTRING) != NULL;
#elif defined __APPLE__ && defined __MACH__
	SWAPcontrol = true;
#endif

	if (COM_CheckParm("-novsync"))
	{
		Con_Warning ("Vertical sync disabled at command line\n");
	}
	else if (SWAPcontrol)
	{
#if defined __APPLE__ && defined __MACH__
		GLint state;
		
		CGLError glerr = CGLGetParameter(CGLGetCurrentContext(), kCGLCPSwapInterval, &state);
		if (glerr == kCGLNoError) {
			Con_Printf ("%s sync to vertical retrace\n", (state == 1) ? "Enabled" : "Disabled");
			gl_swapintervalstate = state;
			gl_swap_control = true;
		} else {
			Con_Warning ("Unable to get CGL swap interval\n");
		}
#else
		qglSwapInterval = (void *) qglGetProcAddress (SWAPINTERVALFUNC);
		
		if (qglSwapInterval)
		{
			if (!qglSwapInterval(0))
				Con_Warning ("Vertical sync not supported (%s failed)\n", SWAPINTERVALFUNC);
			else
			{
				Con_Printf ("Found %s\n", SWAPCONTROLSTRING);
				gl_swap_control = true;
			}
		}
		else
			Con_Warning ("Vertical sync not supported (qglGetProcAddress failed)\n");
#endif
	}
	else
		Con_Warning ("Vertical sync not supported (extension not found)\n");
}

/*
===============
GL_CheckSwapInterval -- check vsync after vid_restart
===============
*/
void GL_CheckSwapInterval (void)
{
	if (gl_swap_control)
	{
#if defined __APPLE__ && defined __MACH__
		GLint state = gl_swapintervalstate;
		
		CGLError glerr = CGLSetParameter(CGLGetCurrentContext(), kCGLCPSwapInterval, &state);
		if (glerr == kCGLNoError) {
			Con_Printf ("%s sync to vertical retrace\n", (state == 1) ? "Enabled" : "Disabled");
		} else {
			Con_Warning ("Unable to set CGL swap interval\n");
		}
#else
		if (!qglSwapInterval((gl_swapinterval.value) ? 1 : 0))
			Con_Printf ("GL_SwapInterval: failed on %s\n", SWAPINTERVALFUNC);
#endif
	}
}

/*
===============
GL_SwapInterval
===============
*/
void GL_SwapInterval (void)
{
	if (gl_swap_control)
	{
#if defined __APPLE__ && defined __MACH__
		const GLint state = (gl_swapinterval.value) ? 1 : 0;
		if (state == gl_swapintervalstate)
			return;
		
		CGLError glerr = CGLSetParameter(CGLGetCurrentContext(), kCGLCPSwapInterval, &state);
		if (glerr == kCGLNoError) {
			Con_Printf ("%s CGL swap interval\n", (state == 1) ? "Enabled" : "Disabled");
			gl_swapintervalstate = state;
		} else {
			Con_Warning ("Unable to set CGL swap interval\n");
		}
#else
		if (!qglSwapInterval((gl_swapinterval.value) ? 1 : 0))
			Con_Printf ("GL_SwapInterval: failed on %s\n", SWAPINTERVALFUNC);
#endif
	}
}

void GL_CheckMultithreadedGL (void)
{
	if (has_smp)
	{
#if defined __APPLE__ && defined __MACH__
		CGLError glerr = CGLEnable(CGLGetCurrentContext(), kCGLCEMPEngine);
		if (glerr == kCGLNoError) {
			Con_Printf("Enabled multi-threaded GL engine\n");
		}
#endif
	}
}

/*
===============
GL_CheckExtensions
===============
*/
void GL_CheckExtensions (void)
{
	//
	// poll max size from hardware
	//
	glGetIntegerv (GL_MAX_TEXTURE_SIZE, &gl_hardware_max_size);
	Con_Printf ("Maximum texture size %i\n", gl_hardware_max_size);

	// by default we sets maxsize as hardware maxsize
	
	GL_CheckExtension_Multitexture ();
	GL_CheckExtension_EnvCombine ();
	GL_CheckExtension_EnvAdd ();
	
	GL_CheckExtension_NPoT ();
	GL_CheckExtension_TextureCompression ();
	GL_CheckExtension_Anisotropy ();
	GL_CheckExtension_VSync ();
}

/*
===============
GL_MakeNiceExtensionsList -- johnfitz
===============
*/
char *GL_MakeNiceExtensionsList (char *in)
{
	char *copy, *token, *out;
	int i, count;

	if (!in) 
		return Z_Strdup ("(none)");

	// each space will be replaced by 4 chars, so count the spaces before we malloc
	for (i = 0, count = 1; i < (int)strlen(in); i++)
		if (in[i] == ' ')
			count++;

	out = Z_Malloc (strlen(in) + count*3 + 1); // usually about 1-2k
	out[0] = 0;

	copy = Z_Strdup (in);

	for (token = strtok(copy, " "); token; token = strtok(NULL, " "))
	{
		strcat(out, "\n   ");
		strcat(out, token);
	}

	Z_Free (copy);
	return out;
}


void GL_Info (qboolean all)
{
	static char *gl_extensions_nice = NULL;
#ifdef GLX_GLXEXT_PROTOTYPES
	static char *glx_extensions_nice = NULL;
#endif
	
	if (!gl_extensions_nice)
		gl_extensions_nice = GL_MakeNiceExtensionsList (gl_extensions);
#ifdef GLX_GLXEXT_PROTOTYPES
	if (!glx_extensions_nice)
		glx_extensions_nice = GL_MakeNiceExtensionsList (glx_extensions);
#endif
	
	Con_SafePrintf ("GL_VENDOR: %s\n", gl_vendor);
	Con_SafePrintf ("GL_RENDERER: %s\n", gl_renderer);
	Con_SafePrintf ("GL_VERSION: %s\n", gl_version);
	
	if (all)
	{
		Con_SafePrintf ("GL_EXTENSIONS: %s\n", gl_extensions_nice);
#ifdef GLX_GLXEXT_PROTOTYPES
		Con_SafePrintf ("GLX_EXTENSIONS: %s\n", glx_extensions_nice);
#endif
	}
}

/*
===============
GL_Info_f
===============
*/
void GL_Info_f (void)
{
	char	*command;
	
	if (Cmd_Argc() < 2)
	{
		Con_Printf("commands: ");
		Con_Printf("short, all, pf\n");
		return;
	}
	
	command = Cmd_Argv (1);
	if (strcasecmp(command, "short") == 0)
	{
		GL_Info (false);
		return;
	}
	if (strcasecmp(command, "all") == 0)
	{
		GL_Info (true);
		return;
	}
	if (strcasecmp(command, "pf") == 0)
	{
		GL_GetPixelFormatInfo ();
		return;
	}
}


//=============================================================================

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

//	glCullFace (GL_FRONT);
	glCullFace (GL_BACK); // johnfitz -- glquake used CCW with backwards culling -- let's do it right
	glFrontFace (GL_CW); // johnfitz -- glquake used CCW with backwards culling -- let's do it right

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
	
	// set up global fog
	glFogi (GL_FOG_MODE, GL_EXP2);
	glHint (GL_FOG_HINT, GL_NICEST); /*  per pixel  */
}

void GL_GetPixelFormatInfo (void)
{
	// read stencil bits
	glGetIntegerv (GL_STENCIL_BITS, &gl_stencilbits);
	// read depth bits
	glGetIntegerv (GL_DEPTH_BITS, &gl_depthbits);
	// read color bits
	glGetIntegerv (GL_RED_BITS, &gl_redbits);
	glGetIntegerv (GL_GREEN_BITS, &gl_greenbits);
	glGetIntegerv (GL_BLUE_BITS, &gl_bluebits);
	// read alpha bits
	glGetIntegerv (GL_ALPHA_BITS, &gl_alphabits);
	
	Con_Printf ("Pixel format\n"
				"   RGBA (%d/%d/%d/%d bits)\n"
				"   Z-buffer (%d bits)\n"
				"   stencil (%d bits)\n",
				gl_redbits, gl_greenbits, gl_bluebits, gl_alphabits,
				gl_depthbits,
				gl_stencilbits);
	
}

void GL_GetGLInfo (void)
{
	// gl_info
	gl_vendor = (char *)glGetString (GL_VENDOR);
	gl_renderer = (char *)glGetString (GL_RENDERER);
	gl_version = (char *)glGetString (GL_VERSION);
	gl_extensions = (char *)glGetString (GL_EXTENSIONS);
}

/*
===============
GL_Init
===============
*/
void GL_Init (void)
{
	GL_GetGLInfo ();
	
	// gl_info
	Con_SafePrintf ("GL_VENDOR: %s\n", gl_vendor);
	Con_SafePrintf ("GL_RENDERER: %s\n", gl_renderer);
	Con_SafePrintf ("GL_VERSION: %s\n", gl_version);
	
	GL_CheckExtensions ();
	GL_CheckMultithreadedGL ();

	Cvar_RegisterVariableCallback (&gl_swapinterval, GL_SwapInterval);

	Cmd_AddCommand ("gl_info", GL_Info_f);

	if (!strcmp(gl_vendor, "Intel")) // intel video workaround
	{
		Con_Printf ("Intel Display Adapter detected\n");
		isIntel = true;
	}

	Con_Printf ("OpenGL initialized\n");
}

/*
================
GL_BindTexture
================
*/
void GL_BindTexture (gltexture_t *texture)
{
	if (!texture)
		texture = nulltexture;

	if (texture->texnum != currenttexture[currenttarget - GL_TEXTURE0_ARB])
	{
		currenttexture[currenttarget - GL_TEXTURE0_ARB] = texture->texnum;
		glBindTexture (GL_TEXTURE_2D, texture->texnum);
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, texture->max_miplevel);
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
GL_SelectTexture
================
*/
void GL_SelectTexture (GLenum target)
{
//	if (target == currenttarget)
//		return;

	qglActiveTexture (target);
	currenttarget = target;
}


void GL_SelectTMU0 (void)
{
	if (GL_TEXTURE0_ARB == currenttarget)
		return;

	glDisable (GL_TEXTURE_2D);
	GL_SelectTexture (GL_TEXTURE0_ARB);
}

void GL_SelectTMU1 (void)
{
	if (GL_TEXTURE1_ARB == currenttarget)
		return;
	
	GL_SelectTexture (GL_TEXTURE1_ARB);
	glEnable (GL_TEXTURE_2D);
}

void GL_SelectTMU2 (void)
{
	if (GL_TEXTURE2_ARB == currenttarget)
		return;

	GL_SelectTexture (GL_TEXTURE2_ARB);
	glEnable (GL_TEXTURE_2D);
}

void GL_SelectTMU3 (void)
{
	if (GL_TEXTURE3_ARB == currenttarget)
		return;

	GL_SelectTexture (GL_TEXTURE3_ARB);
	glEnable (GL_TEXTURE_2D);
}

//=============================================================================

/*
===============
GL_SetFilterModes
===============
*/
void GL_SetFilterModes (gltexture_t *glt)
{
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
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_texture_anisotropy);
	}
	else
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, modes[gl_texturemode].magfilter);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, modes[gl_texturemode].magfilter);
	}
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
        GL_BindTexture (glt);
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
        GL_BindTexture (glt);
		GL_SetFilterModes (glt);
    }
}

//=============================================================================

/*
================
TexMgr_NewGame
================
*/
void TexMgr_NewGame (void)
{
	TexMgr_FreeTextures (0, TEXPREF_PERSIST); // deletes all textures where TEXPREF_PERSIST is unset
	TexMgr_LoadPalette ();
}

/*
===============
TexMgr_UploadWarpImage

called during init,
choose correct warpimage size and reload existing warpimage textures if needed
===============
*/
void TexMgr_UploadWarpImage (void)
{
	int mark;
	gltexture_t *glt;
	byte *dummy;

	//
	// find the new correct size
	//
	if ((int)gl_warpimagesize.value < 32)
		Cvar_SetValueNoCallback ("gl_warpimagesize", 32);

	//
	// make sure warpimage size is a power of two
	//
	warpimage_size = TexMgr_SafeTextureSize((int)gl_warpimagesize.value);

	while (warpimage_size*2 > vid.width) // *2 for glow
		warpimage_size >>= 1;
	while (warpimage_size > vid.height)
		warpimage_size >>= 1;

	if (warpimage_size != gl_warpimagesize.value)
		Cvar_SetValueNoCallback ("gl_warpimagesize", warpimage_size);

	// ericw -- removed early exit if (warpimage_size == oldsize).
	// after reloads textures to source width/height, which might not match oldsize.
	
	//
	// resize the textures in opengl
	//
	mark = Hunk_LowMark ();
	
	dummy = Hunk_Alloc (warpimage_size*warpimage_size*4);

	for (glt=active_gltextures; glt; glt=glt->next)
	{
		if (glt->flags & TEXPREF_WARPIMAGE)
		{
			GL_BindTexture (glt);
			glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, warpimage_size, warpimage_size, 0, GL_RGBA, GL_UNSIGNED_BYTE, dummy);
			glt->width = glt->height = warpimage_size;
			
			// set filter modes
			GL_SetFilterModes (glt);
		}
	}

	Hunk_FreeToLowMark (mark);
}


/*
=================
TexMgr_LoadPalette
=================
*/
void TexMgr_LoadPalette (void)
{
	V_FindFullbrightColors ();
	V_SetOriginalPalette ();
	V_SetPalette (host_basepal);
}

/*
===============
TexMgr_Init

must be called before any texture loading
===============
*/
void TexMgr_Init (void)
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

	// palette
	TexMgr_LoadPalette ();
	
	Cvar_RegisterVariableCallback (&gl_picmip, TexMgr_ReloadTextures);
	Cvar_RegisterVariableCallback (&gl_warpimagesize, TexMgr_UploadWarpImage);
	Cvar_RegisterVariableCallback (&gl_compression, TexMgr_ReloadTextures);

	Cmd_AddCommand ("gl_texturemode", &GL_TextureMode_f);
	Cmd_AddCommand ("gl_texture_anisotropy", &GL_Texture_Anisotropy_f);

	// load notexture images
	notexture = TexMgr_LoadTexture (NULL, "notexture", 2, 2, SRC_RGBA, notexture_data, "", (uintptr_t)notexture_data, TEXPREF_NEAREST | TEXPREF_PERSIST | TEXPREF_NOPICMIP);
	nulltexture = TexMgr_LoadTexture (NULL, "nulltexture", 2, 2, SRC_RGBA, nulltexture_data, "", (uintptr_t)nulltexture_data, TEXPREF_NEAREST | TEXPREF_PERSIST | TEXPREF_NOPICMIP);

	// have to assign these here because R_InitTextures is called before TexMgr_Init
	notexture_mip->base = notexture_mip2->base = notexture;

	// upload warpimage
	TexMgr_UploadWarpImage ();
}


/*
================
TexMgr_Pad -- return smallest power of two greater than or equal to size
================
*/
int TexMgr_Pad (int s)
{
	int i;
	
	for (i=1; i<s; i<<=1)
		;
	
	return i;
}

/*
===============
TexMgr_SafeTextureSize -- return a size with hardware and user prefs in mind
===============
*/
int TexMgr_SafeTextureSize (int s)
{
	if (!gl_texture_NPOT)
		s = TexMgr_Pad(s);
	
	s = CLAMP(1, s, gl_hardware_max_size);
	
	return s;
}

/*
================
TexMgr_PadConditional -- only pad if a texture of that size would be padded. (used for tex coords)
================
*/
int TexMgr_PadConditional (int s)
{
	if (s < TexMgr_SafeTextureSize(s))
		return TexMgr_Pad(s);
	else
		return s;
}

/*
================
TexMgr_MipMapW
================
*/
unsigned *TexMgr_MipMapW (unsigned *data, int width, int height)
{
	int		i, size;
	byte	*out, *in;
    
	out = in = (byte *)data;
	size = (width*height)>>1;
    
	for (i=0; i<size; i++, out+=4, in+=8)
	{
		out[0] = (in[0] + in[4])>>1;
		out[1] = (in[1] + in[5])>>1;
		out[2] = (in[2] + in[6])>>1;
		out[3] = (in[3] + in[7])>>1;
	}
    
	return data;
}

/*
================
TexMgr_MipMapH
================
*/
unsigned *TexMgr_MipMapH (unsigned *data, int width, int height)
{
	int		i, j;
	byte	*out, *in;
    
	out = in = (byte *)data;
	height>>=1;
	width<<=2;
    
	for (i=0; i<height; i++, in+=width)
		for (j=0; j<width; j+=4, out+=4, in+=4)
		{
			out[0] = (in[0] + in[width+0])>>1;
			out[1] = (in[1] + in[width+1])>>1;
			out[2] = (in[2] + in[width+2])>>1;
			out[3] = (in[3] + in[width+3])>>1;
		}
    
	return data;
}

/*
================
TexMgr_ResampleTexture -- bilinear resample
================
*/
unsigned *TexMgr_ResampleTexture (char *name, unsigned *in, int inwidth, int inheight, qboolean alpha)
{
	byte *nwpx, *nepx, *swpx, *sepx, *dest;
	unsigned xfrac, yfrac, x, y, modx, mody, imodx, imody, injump, outjump;
	unsigned *out;
	int i, j, outwidth, outheight;
    
	if (inwidth == TexMgr_Pad(inwidth) && inheight == TexMgr_Pad(inheight))
		return in;
    
	outwidth = TexMgr_Pad(inwidth);
	outheight = TexMgr_Pad(inheight);
	out = Hunk_Alloc(outwidth*outheight*4);
    
	if (developer.value > 1)
		Con_DPrintf ("TexMgr_ResampleTexture: in:%dx%d, out:%dx%d, '%s'\n", inwidth, inheight, outwidth, outheight, name);
	
	xfrac = ((inwidth-1) << 16) / (outwidth-1);
	yfrac = ((inheight-1) << 16) / (outheight-1);
	y = outjump = 0;
    
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
			nepx = nwpx + 4;
			swpx = nwpx + inwidth*4;
			sepx = swpx + 4;
            
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
    
	return out;
}

/*
===============
TexMgr_AlphaEdgeFix
 
eliminate pink edges on sprites, etc.
operates in place on 32bit data
===============
*/
void TexMgr_AlphaEdgeFix (byte *data, int width, int height)
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
			if (dest[3]) //not transparent
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
            
			//average all non-transparent neighbors
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
===============
TexMgr_PadEdgeFixW -- special case of AlphaEdgeFix for textures that only need it because they were padded

operates in place on 32bit data, and expects unpadded height and width values
===============
*/
void TexMgr_PadEdgeFixW (byte *data, int width, int height)
{
	byte *src, *dst;
	int i, padw, padh;
    
	padw = TexMgr_PadConditional(width);
	padh = TexMgr_PadConditional(height);
    
	//copy last full column to first empty column, leaving alpha byte at zero
	src = data + (width - 1) * 4;
	for (i=0; i<padh; i++)
	{
		src[4] = src[0];
		src[5] = src[1];
		src[6] = src[2];
		src += padw * 4;
	}
    
	//copy first full column to last empty column, leaving alpha byte at zero
	src = data;
	dst = data + (padw - 1) * 4;
	for (i=0; i<padh; i++)
	{
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		src += padw * 4;
		dst += padw * 4;
	}
}

/*
===============
TexMgr_PadEdgeFixH -- special case of AlphaEdgeFix for textures that only need it because they were padded

operates in place on 32bit data, and expects unpadded height and width values
===============
*/
void TexMgr_PadEdgeFixH (byte *data, int width, int height)
{
	byte *src, *dst;
	int i, padw, padh;
    
	padw = TexMgr_PadConditional(width);
	padh = TexMgr_PadConditional(height);
    
	//copy last full row to first empty row, leaving alpha byte at zero
	dst = data + height * padw * 4;
	src = dst - padw * 4;
	for (i=0; i<padw; i++)
	{
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		src += 4;
		dst += 4;
	}
    
	//copy first full row to last empty row, leaving alpha byte at zero
	dst = data + (padh - 1) * padw * 4;
	src = data;
	for (i=0; i<padw; i++)
	{
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		src += 4;
		dst += 4;
	}
}

/*
================
TexMgr_8to32
================
*/
unsigned *TexMgr_8to32 (byte *in, int pixels, unsigned int *usepal)
{
	int i;
	unsigned *out, *data;
    
	out = data = Hunk_Alloc(pixels*4);
    
	for (i=0 ; i<pixels ; i++)
		*out++ = usepal[*in++];
    
	return data;
}

/*
================
TexMgr_PadImageW -- return image with width padded up to power-of-two dimentions
================
*/
byte *TexMgr_PadImageW (char *name, byte *in, int width, int height, byte padbyte)
{
	int i, j, outwidth;
	byte *out, *data;
    
	if (width == TexMgr_Pad(width))
		return in;
    
	outwidth = TexMgr_Pad(width);
    
	out = data = Hunk_Alloc(outwidth*height);
    
	if (developer.value > 1)
		Con_DPrintf ("TexMgr_PadImageW: in:%d, out:%d, '%s'\n", width, outwidth, name);
	
	for (i=0; i<height; i++)
	{
		for (j=0; j<width; j++)
			*out++ = *in++;
		for (   ; j<outwidth; j++)
			*out++ = padbyte;
	}
    
	return data;
}

/*
================
TexMgr_PadImageH -- return image with height padded up to power-of-two dimentions
================
*/
byte *TexMgr_PadImageH (char *name, byte *in, int width, int height, byte padbyte)
{
	int i, srcpix, dstpix;
	byte *data, *out;
    
	if (height == TexMgr_Pad(height))
		return in;
    
	srcpix = width * height;
	dstpix = width * TexMgr_Pad(height);
    
	out = data = Hunk_Alloc(dstpix);
    
	if (developer.value > 1)
		Con_DPrintf ("TexMgr_PadImageH: in:%d, out:%d, '%s'\n", height, dstpix/width, name);
	
	for (i=0; i<srcpix; i++)
		*out++ = *in++;
	for (   ; i<dstpix; i++)
		*out++ = padbyte;
    
	return data;
}

//====================================================================


//====================================================
// GL Compression


#define STB_DXT_IMPLEMENTATION
#include "stb_dxt.h"

void DXT_ExtractColorBlock (unsigned dst[16], unsigned *in, int inwidth, int inheight, int x, int y)
{
	unsigned *src = in + y * inwidth + x;
	unsigned black;
	int width, height;
	int y1, x1;
	
	if (inheight - y >= 4 && inwidth - x >= 4) {
		/* Fast path for a full block */
		memcpy(dst +  0, src + inwidth * 0, 16);
		memcpy(dst +  4, src + inwidth * 1, 16);
		memcpy(dst +  8, src + inwidth * 2, 16);
		memcpy(dst + 12, src + inwidth * 3, 16);
	} else {
		/* Partial block, pad with black and alpha 1.0 */
		black = (unsigned)LittleLong(255ul << 24);
		width  = min(inwidth  - x, 4);
		height = min(inheight - y, 4);
		y1 = 0;
		for ( ; y1 < height; y1++) {
			x1 = 0;
			for ( ; x1 < width; x1++)
				dst[y1 * 4 + x1] = src[y1 * inwidth + x1];
			for ( ; x1 < 4; x1++)
				dst[y1 * 4 + x1] = black;
		}
		for ( ; y1 < 4; y1++) {
			for (x1 = 0; x1 < 4; x1++)
				dst[y1 * 4 + x1] = black;
		}
	}
}

void TexMgr_CompressDXT1 (unsigned *in, int inwidth, int inheight, byte *dst)
{
	unsigned colorblock[16];
	int width, height;
	int y, x;
	
	width = inwidth;
	height = inheight;
	for (y = 0; y < height; y += 4) {
		for (x = 0; x < width; x += 4) {
			DXT_ExtractColorBlock(colorblock, in, inwidth, inheight, x, y);
			stb_compress_dxt_block(dst, (byte *)colorblock, false, STB_DXT_HIGHQUAL);
			dst += 8;
		}
	}
}

void TexMgr_CompressDXT5 (unsigned *in, int inwidth, int inheight, byte *dst)
{
	unsigned colorblock[16];
	int width, height;
	int y, x;
	
	width = inwidth;
	height = inheight;
	for (y = 0; y < height; y += 4) {
		for (x = 0; x < width; x += 4) {
			DXT_ExtractColorBlock(colorblock, in, inwidth, inheight, x, y);
			stb_compress_dxt_block(dst, (byte *)colorblock, true, STB_DXT_HIGHQUAL);
			dst += 16;
		}
	}
}

int TexMgr_GetMipMemorySize (int width, int height, GLint format)
{
	int blocksize, blockbytes, blockwidth, blockheight;
	
	blocksize = 1;
	blockbytes = 4;
	switch (format) {
		case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
		case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
			blocksize = 4;
			blockbytes = 8;
			break;
		case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
			blocksize = 4;
			blockbytes = 16;
			break;
	}
	
	blockwidth  = (width  + blocksize - 1) / blocksize;
	blockheight = (height + blocksize - 1) / blocksize;
	
	return blockwidth * blockheight * blockbytes;
}

void TexMgr_CompressMip (unsigned *in, int inwidth, int inheight, GLint format, byte *dst)
{
	switch (format) {
		case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
			TexMgr_CompressDXT1(in, inwidth, inheight, dst);
			break;
		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
			TexMgr_CompressDXT5(in, inwidth, inheight, dst);
			break;
		default:
			Sys_Error("Unsupported compressed format");
	}
}

int TexMgr_GetMaxMipLevel (int width, int height, GLint format)
{
	int blocksize, max_level;
	
	blocksize = 1;
	switch (format) {
		case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
		case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
		case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
			blocksize = 4;
	}
	
	max_level = 0;
	while (width > blocksize || height > blocksize) {
		max_level++;
		width  = width  > 1 ? width  >> 1 : 1;
		height = height > 1 ? height >> 1 : 1;
	}
	
	return max_level;
}

//====================================================

/*
===============
TexMgr_Upload32

handles 32bit source data
===============
*/
void TexMgr_Upload32 (gltexture_t *glt, unsigned *data)
{
    int	internalformat,	miplevel, mipwidth, mipheight, picmip;
    unsigned	*scaled = NULL;
	byte *compressed = NULL;
	int mip_memory_size;
	int max_miplevel;
	
	if (gl_texture_NPOT) {
		scaled = data;
	} else {
        // resample up
        scaled = TexMgr_ResampleTexture (glt->name, data, glt->width, glt->height, glt->flags & TEXPREF_ALPHA);
        glt->width = TexMgr_Pad(glt->width);
        glt->height = TexMgr_Pad(glt->height);
    }
    
	// mipmap down
	picmip = (glt->flags & TEXPREF_NOPICMIP) ? 0 : max ((int)gl_picmip.value, 0);
	mipwidth = TexMgr_SafeTextureSize (glt->width >> picmip);
	mipheight = TexMgr_SafeTextureSize (glt->height >> picmip);
	while (glt->width > mipwidth)
	{
		scaled = TexMgr_MipMapW (scaled, glt->width, glt->height);
		glt->width >>= 1;
		if (glt->flags & TEXPREF_ALPHA)
			TexMgr_AlphaEdgeFix ((byte *)scaled, glt->width, glt->height);
	}
	while (glt->height > mipheight)
	{
		scaled = TexMgr_MipMapH (scaled, glt->width, glt->height);
		glt->height >>= 1;
		if (glt->flags & TEXPREF_ALPHA)
			TexMgr_AlphaEdgeFix ((byte *)scaled, glt->width, glt->height);
	}
    
	// upload
	GL_BindTexture (glt);
	
	internalformat = (glt->flags & TEXPREF_ALPHA) ? GL_RGBA : GL_RGB;
	if (gl_texture_compression && gl_compression.value && !(glt->flags & TEXPREF_NOPICMIP)) {
		internalformat = (glt->flags & TEXPREF_ALPHA) ? GL_COMPRESSED_RGBA_S3TC_DXT5_EXT : GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
		mip_memory_size = TexMgr_GetMipMemorySize(glt->width, glt->height, internalformat);
		switch (internalformat) {
			case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
			case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
				compressed = Hunk_Alloc(mip_memory_size);
				break;
		}
	}
	
	if (compressed) {
		TexMgr_CompressMip(scaled, glt->width, glt->height, internalformat, compressed);
		qglCompressedTexImage2D(GL_TEXTURE_2D, 0, internalformat, glt->width, glt->height, 0, mip_memory_size, compressed);
	} else {
		glTexImage2D (GL_TEXTURE_2D, 0, internalformat, glt->width, glt->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
	}
    
	glt->max_miplevel = 0;
	
	// upload mipmaps
	if (glt->flags & TEXPREF_MIPMAP)
	{
		mipwidth = glt->width;
		mipheight = glt->height;
		
		max_miplevel = TexMgr_GetMaxMipLevel(mipwidth, mipheight, internalformat);
		miplevel = 1;
		
		glt->max_miplevel = max_miplevel;
		
		while (miplevel <= max_miplevel)
		{
			if (mipwidth > 1)
			{
				scaled = TexMgr_MipMapW (scaled, mipwidth, mipheight);
				mipwidth >>= 1;
				if (glt->flags & TEXPREF_ALPHA)
					TexMgr_AlphaEdgeFix ((byte *)scaled, mipwidth, mipheight);
			}
			if (mipheight > 1)
			{
				scaled = TexMgr_MipMapH (scaled, mipwidth, mipheight);
				mipheight >>= 1;
				if (glt->flags & TEXPREF_ALPHA)
					TexMgr_AlphaEdgeFix ((byte *)scaled, mipwidth, mipheight);
			}
			
			if (compressed) {
				mip_memory_size = TexMgr_GetMipMemorySize(mipwidth, mipheight, internalformat);
				TexMgr_CompressMip(scaled, mipwidth, mipheight, internalformat, compressed);
				qglCompressedTexImage2D(GL_TEXTURE_2D, miplevel, internalformat, mipwidth, mipheight, 0, mip_memory_size, compressed);
			} else {
				glTexImage2D (GL_TEXTURE_2D, miplevel, internalformat, mipwidth, mipheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
			}
			
			miplevel++;
		}
	}
    
	// set filter modes
	GL_SetFilterModes (glt);
}

/*
================
TexMgr_UploadBloom

handles bloom data
================
*/
void TexMgr_UploadBloom (gltexture_t *glt, unsigned *data)
{
	// upload it
	GL_BindTexture (glt);
	glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, glt->width, glt->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	// set filter modes
	GL_SetFilterModes (glt);
}

/*
================
TexMgr_UploadLightmap

handles lightmap data
================
*/
void TexMgr_UploadLightmap (gltexture_t *glt, byte *data)
{
	// upload it
	GL_BindTexture (glt);
	glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, glt->width, glt->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	// set filter modes
	GL_SetFilterModes (glt);
}

/*
===============
TexMgr_CalculateFlatColors

calculate flat color based on average of all opaque colors
===============
*/
void TexMgr_CalculateFlatColors (gltexture_t *glt, byte *data, int size)
{
	int			i;
	int			p;
	unsigned	*rgba;
	int			r, g, b, count;
	int			r0, g0, b0, count0;
	int			r1, g1, b1, count1;
	
	r = g = b = count = 0;
	r0 = g0 = b0 = count0 = 0;
	r1 = g1 = b1 = count1 = 0;
	
	for (i=0 ; i<size ; i++)
	{
		p = data[i];
		if (p != 0 && p != 255)
		{
			rgba = &d_8to24table[p];
			
			r += ((byte *)rgba)[0];
			g += ((byte *)rgba)[1];
			b += ((byte *)rgba)[2];
			count++;
			
			if (GetBit (is_fullbright, p)) // fullbrights
			{
				r1 += ((byte *)rgba)[0];
				g1 += ((byte *)rgba)[1];
				b1 += ((byte *)rgba)[2];
				count1++;
			}
			else // nobrights
			{
				r0 += ((byte *)rgba)[0];
				g0 += ((byte *)rgba)[1];
				b0 += ((byte *)rgba)[2];
				count0++;
			}
		}
	}
	
	if (count)
	{
		glt->colors.flatcolor[0] = (float)r/(count*255);
		glt->colors.flatcolor[1] = (float)g/(count*255);
		glt->colors.flatcolor[2] = (float)b/(count*255);
	}
	
	if (count0)
	{
		glt->colors.basecolor[0] = (float)r0/(count0*255);
		glt->colors.basecolor[1] = (float)g0/(count0*255);
		glt->colors.basecolor[2] = (float)b0/(count0*255);
	}
	
	if (count1)
	{
		glt->colors.glowcolor[0] = (float)r1/(count1*255);
		glt->colors.glowcolor[1] = (float)g1/(count1*255);
		glt->colors.glowcolor[2] = (float)b1/(count1*255);
	}
}

/*
===============
TexMgr_Upload8

handles 8bit source data, then passes it to TexMgr_Upload32
===============
*/
void TexMgr_Upload8 (gltexture_t *glt, byte *data)
{
	int			i, size;
	int			p;
	unsigned	*trans = NULL;
	unsigned int	*pal;
	qboolean padw = false, padh = false;
	byte padbyte;
    
	// HACK HACK HACK -- taken from tomazquake
	if (strstr(glt->name, "shot1sid") && glt->width==32 && glt->height==32 && CRC_Block(data, 1024) == 65393)
	{
		// This texture in b_shell1.bsp has some of the first 32 pixels painted white.
		// They are invisible in software, but look really ugly in GL. So we just copy
		// 32 pixels from the bottom to make it look nice.
		memcpy (data, data + 32*31, 32);
	}

	size = glt->width * glt->height;
	if (size & 3)
		Con_DWarning ("TexMgr_Upload8: size %d is not a multiple of 4 in '%s'\n", size, glt->name); // should be an error but ... (EER1)

	if (glt->owner && glt->owner->type == mod_alias)
		Mod_FloodFillSkin (data, glt->width, glt->height, glt->owner->name);

	// calculate flat colors
	TexMgr_CalculateFlatColors (glt, data, size);
	
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

	// choose palette and padbyte
	if (glt->flags & TEXPREF_FULLBRIGHT)
	{
		if (glt->flags & TEXPREF_ALPHA)
			pal = d_8to24table_fullbright_holey;
		else
			pal = d_8to24table_fullbright;
		padbyte = 0;
	}
	else if (glt->flags & TEXPREF_NOBRIGHT)
	{
		if (glt->flags & TEXPREF_ALPHA)
			pal = d_8to24table_nobright_holey;
		else
			pal = d_8to24table_nobright;
		padbyte = 0;
	}
	else if (glt->flags & TEXPREF_CONCHARS)
	{
		pal = d_8to24table_conchars;
		padbyte = 0;
	}
	else
	{
		pal = d_8to24table;
		padbyte = 255;
	}
    
    // pad each dimention, but only if it's not going to be downsampled later
	if (glt->flags & TEXPREF_PAD)
	{
		if ((int) glt->width < TexMgr_SafeTextureSize(glt->width))
		{
			data = TexMgr_PadImageW (glt->name, data, glt->width, glt->height, padbyte);
			glt->width = TexMgr_Pad(glt->width);
			padw = true;
		}
		if ((int) glt->height < TexMgr_SafeTextureSize(glt->height))
		{
			data = TexMgr_PadImageH (glt->name, data, glt->width, glt->height, padbyte);
			glt->height = TexMgr_Pad(glt->height);
			padh = true;
		}
	}
	
    // convert to 32bit
	trans = TexMgr_8to32(data, glt->width * glt->height, pal);
    
    // fix edges
	if (glt->flags & TEXPREF_ALPHA)
		TexMgr_AlphaEdgeFix ((byte *)trans, glt->width, glt->height);
	else
	{
		if (padw)
			TexMgr_PadEdgeFixW ((byte *)trans, glt->source_width, glt->source_height);
		if (padh)
			TexMgr_PadEdgeFixH ((byte *)trans, glt->source_width, glt->source_height);
	}
    
	// upload it
	TexMgr_Upload32 (glt, trans);
}


/*
================
TexMgr_FindTexture
================
*/
gltexture_t *TexMgr_FindTexture (model_t *owner, char *name)
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
TexMgr_NewTexture
================
*/
gltexture_t *TexMgr_NewTexture (void)
{
	gltexture_t *glt;

	if (numgltextures >= MAX_GLTEXTURES)
		Sys_Error ("TexMgr_NewTexture: cache full, max is %i textures", MAX_GLTEXTURES);

	glt = free_gltextures;
	free_gltextures = glt->next;
	glt->next = active_gltextures;
	active_gltextures = glt;

	glGenTextures(1, &glt->texnum);
	numgltextures++;
	return glt;
}

//ericw -- workaround for preventing TexMgr_FreeTexture during TexMgr_GenerateTextures
qboolean in_reload_textures;

/*
================
TexMgr_FreeTexture
================
*/
void TexMgr_FreeTexture (gltexture_t *texture)
{
	gltexture_t *glt;

	if (in_reload_textures)
		return;

	if (texture == NULL)
	{
		Con_SafePrintf ("TexMgr_FreeTexture: NULL texture\n");
		return;
	}

	if (active_gltextures == texture)
	{
		active_gltextures = texture->next;
		texture->next = free_gltextures;
		free_gltextures = texture;

		GL_DeleteTexture(texture);
		numgltextures--;
		return;
	}

	for (glt = active_gltextures; glt; glt = glt->next)
	{
		if (glt->next == texture)
		{
			glt->next = texture->next;
			texture->next = free_gltextures;
			free_gltextures = texture;

            GL_DeleteTexture(texture);
			numgltextures--;
			return;
		}
	}

	Con_SafePrintf ("TexMgr_FreeTexture: not found\n");
}

/*
================
TexMgr_FreeTextures

compares each bit in "flags" to the one in glt->flags only if that bit is active in "mask"
================
*/
void TexMgr_FreeTextures (unsigned int flags, unsigned int mask)
{
	gltexture_t *glt, *next;

	for (glt = active_gltextures; glt; glt = next)
	{
		next = glt->next;
		if ((glt->flags & mask) == (flags & mask))
			TexMgr_FreeTexture (glt);
	}
}

/*
================
TexMgr_FreeTexturesForOwner
================
*/
void TexMgr_FreeTexturesForOwner (model_t *owner)
{
	gltexture_t *glt, *next;

	for (glt = active_gltextures; glt; glt = next)
	{
		next = glt->next;
		if (glt && glt->owner == owner)
			TexMgr_FreeTexture (glt);
	}
}

/*
================
TexMgr_DeleteTextures -- delete all textures. called only by vid_restart
================
*/
void TexMgr_DeleteTextures (void)
{
	gltexture_t *glt;

	for (glt = active_gltextures; glt; glt = glt->next)
	{
		GL_DeleteTexture (glt);
	}
}

/*
================
TexMgr_GenerateTextures -- re-create all textures. called only by vid_restart
================
*/
void TexMgr_GenerateTextures (void)
{
	gltexture_t *glt;
	
// ericw -- tricky bug: if the hunk is almost full, an allocation in TexMgr_ReloadTexture
// triggers cache items to be freed, which calls back into TexMgr to free the
// texture. If this frees 'glt' in the loop below, the active_gltextures
// list gets corrupted.
// A test case is jam3_tronyn.bsp with -heapsize 65536, and do several mode
// switches/fullscreen toggles
	in_reload_textures = true;
	
	for (glt = active_gltextures; glt; glt = glt->next)
	{
		glGenTextures(1, &glt->texnum);
		TexMgr_ReloadTextureTranslation (glt, -1, -1);
	}
	
	in_reload_textures = false;
}

/*
================
TexMgr_LoadTexture

the one entry point for loading all textures
================
*/
gltexture_t *TexMgr_LoadTexture (model_t *owner, char *name, int width, int height, enum srcformat format, byte *data, char *source_file, uintptr_t source_offset, unsigned flags)
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
		Con_DWarning ("TexMgr_LoadTexture: texture '%s' has zero size\n", name);

	// Sanity check, max = 32kx32k
	if (width < 0 || height < 0 || size > 0x40000000)
		Sys_Error ("TexMgr_LoadTexture: texture '%s' has invalid size (%dM, max = %dM)", name, size / (1024 * 1024), 0x40000000 / (1024 * 1024));

	// cache check
	crc = CRC_Block(data, size);

	if ((flags & TEXPREF_OVERWRITE) && (glt = TexMgr_FindTexture (owner, name))) 
	{
		if (glt->source_crc == crc)
			return glt;
	}
	else
		glt = TexMgr_NewTexture ();

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
	glt->top_color = -1;
	glt->bottom_color = -1;
	memset (&glt->colors, 0, sizeof(glt->colors));

	//upload it
	mark = Hunk_LowMark ();

	switch (glt->source_format)
	{
	case SRC_INDEXED:
		TexMgr_Upload8 (glt, data);
		break;
	case SRC_LIGHTMAP:
		TexMgr_UploadLightmap (glt, data);
		break;
	case SRC_RGBA:
		TexMgr_Upload32 (glt, (unsigned *)data);
		break;
	case SRC_BLOOM:
		TexMgr_UploadBloom (glt, (unsigned *)data);
		break;
	}

	Hunk_FreeToLowMark (mark);

	return glt;
}


/*
================
TexMgr_ReloadTexture

reloads a texture, and colormaps it if needed
================
*/
void TexMgr_ReloadTexture (gltexture_t *glt)
{
	TexMgr_ReloadTextureTranslation (glt, -1, -1);
}

void TexMgr_ReloadTextureTranslation (gltexture_t *glt, int top, int bottom)
{
	byte	translation[256];
	byte	*src, *dst, *data = NULL, *translated;
	int	mark, size, i;
	
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
		data = Image_LoadImage (glt->source_file, (int *)&glt->source_width, (int *)&glt->source_height); // simple file
	else if (!glt->source_file[0] && glt->source_offset)
		data = (byte *)glt->source_offset; // image in memory

	if (!data)
	{
		Con_Printf ("TexMgr_ReloadTexture: invalid source for %s\n", glt->name);
		Hunk_FreeToLowMark (mark);
		return;
	}

	glt->width = glt->source_width;
	glt->height = glt->source_height;
	
//
// apply top and bottom colors
//
// if top and bottom are -1,-1, use existing top and bottom colors
// if existing top and bottom colors are -1,-1, don't bother colormapping
	if (top > -1 && bottom > -1)
	{
		if (glt->source_format == SRC_INDEXED)
		{
			glt->top_color = top;
			glt->bottom_color = bottom;
		}
		else
			Con_Printf ("TexMgr_ReloadTexture: can't colormap a non SRC_INDEXED texture: %s\n", glt->name);
	}
	if (glt->top_color > -1 && glt->bottom_color > -1)
	{
		//create new translation table
		for (i = 0; i < 256; i++)
			translation[i] = i;
		
		top = glt->top_color * 16;
		if (top < 128)	// the artists made some backwards ranges.  sigh.
		{
			for (i = 0; i < 16; i++)
				translation[TOP_RANGE+i] = top + i;
		}
		else
		{
			for (i = 0; i < 16; i++)
				translation[TOP_RANGE+i] = top+15-i;
		}
		
		bottom = glt->bottom_color * 16;
		if (bottom < 128)
		{
			for (i = 0; i < 16; i++)
				translation[BOTTOM_RANGE+i] = bottom + i;
		}
		else
		{
			for (i = 0; i < 16; i++)
				translation[BOTTOM_RANGE+i] = bottom+15-i;
		}
		
		//translate texture
		size = glt->width * glt->height;
		dst = translated = (byte *) Hunk_Alloc (size);
		src = data;
		
		for (i = 0; i < size; i++)
			*dst++ = translation[*src++];
		
		data = translated;
	}
//
// upload it
//
	switch (glt->source_format)
	{
	case SRC_INDEXED:
		TexMgr_Upload8 (glt, data);
		break;
	case SRC_LIGHTMAP:
		TexMgr_UploadLightmap (glt, data);
		break;
	case SRC_RGBA:
		TexMgr_Upload32 (glt, (unsigned *)data);
		break;
	case SRC_BLOOM:
		TexMgr_UploadBloom (glt, (unsigned *)data);
		break;
	}

	Hunk_FreeToLowMark (mark);
}


/*
================
TexMgr_ReloadTextures

reloads all texture images.
================
*/
void TexMgr_ReloadTextures (void)
{
	gltexture_t *glt;
    
	int mark;
	byte *dummy;
    
	for (glt=active_gltextures; glt; glt=glt->next)
	{
        if (!(glt->flags & TEXPREF_WARPIMAGE)) 
        {
            TexMgr_ReloadTexture (glt);
        }
	}
    
	mark = Hunk_LowMark ();
	
	dummy = Hunk_Alloc (warpimage_size*warpimage_size*4);
    
	for (glt=active_gltextures; glt; glt=glt->next)
	{
		if (glt->flags & TEXPREF_WARPIMAGE)
		{
			GL_BindTexture (glt);
			glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, warpimage_size, warpimage_size, 0, GL_RGBA, GL_UNSIGNED_BYTE, dummy);
			glt->width = glt->height = warpimage_size;
            
            // set filter modes
            GL_SetFilterModes (glt);
		}
	}
    
	Hunk_FreeToLowMark (mark);
}

