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
// gl_anim.c -- handle warping sky/water animation, fog and bloom

#include "quakedef.h"

cvar_t	r_fastsky = {"r_fastsky","0", CVAR_NONE};
cvar_t	r_skyquality = {"r_skyquality","12", CVAR_NONE};
cvar_t	r_skyalpha = {"r_skyalpha","1", CVAR_ARCHIVE};
cvar_t	r_skyfog = {"r_skyfog","0.5", CVAR_ARCHIVE};
cvar_t	r_oldsky = {"r_oldsky", "0", CVAR_NONE};

cvar_t	r_waterquality = {"r_waterquality", "12", CVAR_NONE};
cvar_t	r_flatturb = {"r_flatturb","0", CVAR_NONE};

// Nehahra
cvar_t	gl_fogenable = {"gl_fogenable", "0", CVAR_NONE};
cvar_t	gl_fogdensity = {"gl_fogdensity", "0", CVAR_NONE};
cvar_t	gl_fogred = {"gl_fogred","0.5", CVAR_NONE};
cvar_t	gl_foggreen = {"gl_foggreen","0.5", CVAR_NONE};
cvar_t	gl_fogblue = {"gl_fogblue","0.5", CVAR_NONE};

//==================================================================================================================

/*
=================================================================

	IMAGE LOADING

=================================================================
*/

char loadfilename[MAX_OSPATH]; // file scope so that error messages can use it

//==============================================================================
//
//  TGA
//
//==============================================================================

typedef struct targaheader_s {
	byte 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	byte	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	byte	pixel_size, attributes;
} targaheader_t;

#define TARGAHEADERSIZE 18 // size on disk

int fgetLittleShort (FILE *f)
{
	byte	b1, b2;

	b1 = fgetc(f);
	b2 = fgetc(f);

	return (short)(b1 + b2*256);
}

int fgetLittleLong (FILE *f)
{
	byte	b1, b2, b3, b4;

	b1 = fgetc(f);
	b2 = fgetc(f);
	b3 = fgetc(f);
	b4 = fgetc(f);

	return b1 + (b2<<8) + (b3<<16) + (b4<<24);
}

/*
=============
Image_LoadTGA
=============
*/
byte *Image_LoadTGA (FILE *fin, int *width, int *height)
{
	int				columns, rows, numPixels;
	byte			*pixbuf;
	int				row, column;
	byte			*targa_rgba;
	int				realrow; //johnfitz -- fix for upside-down targas
	qboolean		upside_down; //johnfitz -- fix for upside-down targas
	targaheader_t targa_header;

	targa_header.id_length = fgetc(fin);
	targa_header.colormap_type = fgetc(fin);
	targa_header.image_type = fgetc(fin);

	targa_header.colormap_index = fgetLittleShort(fin);
	targa_header.colormap_length = fgetLittleShort(fin);
	targa_header.colormap_size = fgetc(fin);
	targa_header.x_origin = fgetLittleShort(fin);
	targa_header.y_origin = fgetLittleShort(fin);
	targa_header.width = fgetLittleShort(fin);
	targa_header.height = fgetLittleShort(fin);
	targa_header.pixel_size = fgetc(fin);
	targa_header.attributes = fgetc(fin);

	if (targa_header.image_type!=2 && targa_header.image_type!=10)
		Sys_Error ("Image_LoadTGA: %s is not a type 2 or type 10 targa", loadfilename);

	if (targa_header.colormap_type !=0 || (targa_header.pixel_size!=32 && targa_header.pixel_size!=24))
		Sys_Error ("Image_LoadTGA: %s is not a 24bit or 32bit targa", loadfilename);

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;
	upside_down = !(targa_header.attributes & 0x20); //johnfitz -- fix for upside-down targas

	targa_rgba = Hunk_Alloc (numPixels*4);

	if (targa_header.id_length != 0)
		fseek(fin, targa_header.id_length, SEEK_CUR);  // skip TARGA image comment

	if (targa_header.image_type==2) // Uncompressed, RGB images
	{
		for(row=rows-1; row>=0; row--)
		{
			//johnfitz -- fix for upside-down targas
			realrow = upside_down ? row : rows - 1 - row;
			pixbuf = targa_rgba + realrow*columns*4;
			//johnfitz
			for(column=0; column<columns; column++)
			{
				byte red,green,blue,alphabyte;
				switch (targa_header.pixel_size)
				{
				case 24:
					blue = getc(fin);
					green = getc(fin);
					red = getc(fin);
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = 255;
					break;
				case 32:
					blue = getc(fin);
					green = getc(fin);
					red = getc(fin);
					alphabyte = getc(fin);
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = alphabyte;
					break;
				}
			}
		}
	}
	else if (targa_header.image_type==10) // Runlength encoded RGB images
	{
		byte red,green,blue,alphabyte,packetHeader,packetSize,j;
		for(row=rows-1; row>=0; row--)
		{
			//johnfitz -- fix for upside-down targas
			realrow = upside_down ? row : rows - 1 - row;
			pixbuf = targa_rgba + realrow*columns*4;
			//johnfitz
			for(column=0; column<columns; )
			{
				packetHeader=getc(fin);
				packetSize = 1 + (packetHeader & 0x7f);
				if (packetHeader & 0x80) // run-length packet
				{
					switch (targa_header.pixel_size)
					{
					case 24:
						blue = getc(fin);
						green = getc(fin);
						red = getc(fin);
						alphabyte = 255;
						break;
					case 32:
						blue = getc(fin);
						green = getc(fin);
						red = getc(fin);
						alphabyte = getc(fin);
						break;
					default: /* avoid compiler warnings */
						blue = green = red = alphabyte = 0;
					}

					for(j=0;j<packetSize;j++)
					{
						*pixbuf++=red;
						*pixbuf++=green;
						*pixbuf++=blue;
						*pixbuf++=alphabyte;
						column++;
						if (column==columns) // run spans across rows
						{
							column=0;
							if (row>0)
								row--;
							else
								goto breakOut;
							//johnfitz -- fix for upside-down targas
							realrow = upside_down ? row : rows - 1 - row;
							pixbuf = targa_rgba + realrow*columns*4;
							//johnfitz
						}
					}
				}
				else // non run-length packet
				{
					for(j=0;j<packetSize;j++)
					{
						switch (targa_header.pixel_size)
						{
						case 24:
							blue = getc(fin);
							green = getc(fin);
							red = getc(fin);
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = 255;
							break;
						case 32:
							blue = getc(fin);
							green = getc(fin);
							red = getc(fin);
							alphabyte = getc(fin);
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = alphabyte;
							break;
						default: /* avoid compiler warnings */
							blue = green = red = alphabyte = 0;
						}
						column++;
						if (column==columns) // pixel packet run spans across rows
						{
							column=0;
							if (row>0)
								row--;
							else
								goto breakOut;
							//johnfitz -- fix for upside-down targas
							realrow = upside_down ? row : rows - 1 - row;
							pixbuf = targa_rgba + realrow*columns*4;
							//johnfitz
						}
					}
				}
			}
			breakOut:;
		}
	}

	fclose(fin);

	*width = (int)(targa_header.width);
	*height = (int)(targa_header.height);

	return targa_rgba;
}

//==============================================================================
//
//  PCX
//
//==============================================================================

typedef struct
{
    char			signature;
    char			version;
    char			encoding;
    char			bits_per_pixel;
    unsigned short	xmin,ymin,xmax,ymax;
    unsigned short	hdpi,vdpi;
    byte			colortable[48];
    char			reserved;
    char			color_planes;
    unsigned short	bytes_per_line;
    unsigned short	palette_type;
    char			filler[58];
} pcxheader_t;

/*
============
Image_LoadPCX
============
*/
byte *Image_LoadPCX (FILE *f, int *width, int *height)
{
	pcxheader_t	pcx;
	int			x, y, w, h, readbyte, runlength, start;
	byte		*p, *pcx_rgb;
	byte		palette[768];

	start = ftell (f); // save start of file (since we might be inside a pak file, SEEK_SET might not be the start of the pcx)

	fread(&pcx, sizeof(pcx), 1, f);
	pcx.xmin = (unsigned short)LittleShort (pcx.xmin);
	pcx.ymin = (unsigned short)LittleShort (pcx.ymin);
	pcx.xmax = (unsigned short)LittleShort (pcx.xmax);
	pcx.ymax = (unsigned short)LittleShort (pcx.ymax);
	pcx.bytes_per_line = (unsigned short)LittleShort (pcx.bytes_per_line);

	if (pcx.signature != 0x0A)
		Sys_Error ("Image_LoadPCX: '%s' is not a valid PCX file", loadfilename);

	if (pcx.version != 5)
		Sys_Error ("Image_LoadPCX: '%s' is version %i, should be 5", loadfilename, pcx.version);

	if (pcx.encoding != 1 || pcx.bits_per_pixel != 8 || pcx.color_planes != 1)
		Sys_Error ("Image_LoadPCX: '%s' has wrong encoding or bit depth", loadfilename);

	w = pcx.xmax - pcx.xmin + 1;
	h = pcx.ymax - pcx.ymin + 1;

	pcx_rgb = Hunk_Alloc ((w*h+1)*4); // +1 to allow reading padding byte on last line

	// load palette
	fseek (f, start + com_filesize - 768, SEEK_SET);
	fread (palette, 1, 768, f);

	// back to start of image data
	fseek (f, start + sizeof(pcx), SEEK_SET);

	for (y=0; y<h; y++)
	{
		p = pcx_rgb + y * w * 4;

		for (x=0; x<(pcx.bytes_per_line); ) // read the extra padding byte if necessary
		{
			readbyte = fgetc(f);

			if(readbyte >= 0xC0)
			{
				runlength = readbyte & 0x3F;
				readbyte = fgetc(f);
			}
			else
				runlength = 1;

			while(runlength--)
			{
				p[0] = palette[readbyte*3];
				p[1] = palette[readbyte*3+1];
				p[2] = palette[readbyte*3+2];
				p[3] = 255;
				p += 4;
				x++;
			}
		}
	}

	fclose(f);

	*width = w;
	*height = h;

	return pcx_rgb;
}


/*
============
GL_LoadImage

returns a pointer to hunk allocated RGBA data

TODO: search order: tga png jpg pcx lmp
============
*/
byte *GL_LoadImage (char *name, int *width, int *height)
{
	FILE	*f;

	sprintf (loadfilename, "%s.tga", name);
	COM_FOpenFile (loadfilename, &f, NULL);
	if (f)
		return Image_LoadTGA (f, width, height);

	sprintf (loadfilename, "%s.pcx", name);
	COM_FOpenFile (loadfilename, &f, NULL);
	if (f)
		return Image_LoadPCX (f, width, height);

	return NULL;
}


//==================================================================================================================


/*
==============================================================================

	RENDER-TO-FRAMEBUFFER WATER (adapted from fitzquake)

==============================================================================
*/

// speed up sin calculations - Ed
float turbsin[] =
{
	#include "gl_warp_sin.h"
};

/*
=============
R_UpdateWarpTextures

each frame, update warping textures
=============
*/
void R_UpdateWarpTextures (void)
{
	texture_t *tx;
	int i;
	float x, y, x2, warptess;

	warptess = 128.0 / CLAMP(4.0, floor(r_waterquality.value), 64.0);

	for (i=0; i<cl.worldmodel->numtextures; i++)
	{
		if (!(tx = cl.worldmodel->textures[i]))
			continue;

		if (tx->name[0] != '*')
			continue;

		if (!tx->update_warp)
			continue;

		// render warp
		glViewport (glx, gly + glheight - gl_warpimage_size, gl_warpimage_size, gl_warpimage_size);
//		glViewport (glx, gly, gl_warpimage_size, gl_warpimage_size);

		glMatrixMode (GL_PROJECTION);
		glLoadIdentity ();

		glOrtho (0, 128, 0, 128, -99999, 99999);

		glMatrixMode (GL_MODELVIEW);
		glLoadIdentity ();

		glDisable (GL_ALPHA_TEST); //FX new
		glEnable (GL_BLEND); //FX
		GL_BindTexture (tx->gltexture);
		for (x=0.0; x<128.0; x=x2)
		{
			x2 = x + warptess;
			glBegin (GL_TRIANGLE_STRIP);
			for (y=0.0; y<128.01; y+=warptess) // .01 for rounding errors
			{
				glTexCoord2f (WARPCALC(x,y), WARPCALC(y,x));
				glVertex2f (x,y);
				glTexCoord2f (WARPCALC(x2,y), WARPCALC(y,x2));
				glVertex2f (x2,y);
			}
			glEnd ();
		}
		glEnable (GL_ALPHA_TEST); //FX new
		glDisable (GL_BLEND); //FX

		// copy to texture
		GL_BindTexture (tx->warpimage);
		glCopyTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, glx, gly + glheight - gl_warpimage_size, gl_warpimage_size, gl_warpimage_size);
//		glCopyTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, glx, gly, gl_warpimage_size, gl_warpimage_size);

		tx->update_warp = false;
	}

	// if warp render went down into sbar territory, we need to be sure to refresh it next frame
	if (gl_warpimage_size + sb_lines > vid.height)
		Sbar_Changed ();
}


/*
==============================================================================

	FOG DRAW

==============================================================================
*/

/*
=============
R_FogUpdate

update internal variables
=============
*/
void R_FogUpdate (float density, float red, float green, float blue, float time)
{
	Cvar_SetValue ("gl_fogenable", density ? 1 : 0);
	Cvar_SetValue ("gl_fogdensity", density);
	Cvar_SetValue ("gl_fogred", red);
	Cvar_SetValue ("gl_foggreen", green);
	Cvar_SetValue ("gl_fogblue", blue);
}

/*
=============
R_FogParseServerMessage

handle an 'svc_fog' message from server
=============
*/
void R_FogParseServerMessage (void)
{
	float density, red, green, blue, time;

	density = MSG_ReadByte(net_message) / 255.0;
	red = MSG_ReadByte(net_message) / 255.0;
	green = MSG_ReadByte(net_message) / 255.0;
	blue = MSG_ReadByte(net_message) / 255.0;
	time = max(0.0, MSG_ReadShort(net_message) / 100.0);

	R_FogUpdate (density, red, green, blue, time);
}

/*
=============
R_FogParseServerMessage2 - parse Nehahra fog

handle an 'svc_fogn' message from server
=============
*/
void R_FogParseServerMessage2 (void)
{
	float density, red, green, blue;

	density = MSG_ReadFloat(net_message);
	red = MSG_ReadByte(net_message) / 255.0;
	green = MSG_ReadByte(net_message) / 255.0;
	blue = MSG_ReadByte(net_message) / 255.0;

	R_FogUpdate (density, red, green, blue, 0.0);
}

/*
=============
R_Fog_f

handle the 'fog' console command
=============
*/
void R_Fog_f (void)
{
	switch (Cmd_Argc())
	{
	default:
	case 1:
		Con_Printf ("usage:\n");
		Con_Printf ("   fog <density>\n");
		Con_Printf ("   fog <density> <rgb>\n");
		Con_Printf ("   fog <red> <green> <blue>\n");
		Con_Printf ("   fog <density> <red> <green> <blue>\n");
		Con_Printf("current values:\n");
		Con_Printf("   fog is %sabled\n", gl_fogenable.value ? "en" : "dis");
		Con_Printf("   density is %f\n", gl_fogdensity.value);
		Con_Printf("   red   is %f\n", gl_fogred.value);
		Con_Printf("   green is %f\n", gl_foggreen.value);
		Con_Printf("   blue  is %f\n", gl_fogblue.value);
		break;
	case 2:
		R_FogUpdate(max(0.0, atof(Cmd_Argv(1))), 
			gl_fogred.value, 
			gl_foggreen.value, 
			gl_fogblue.value, 
			0.0);
		break;
	case 3:
		R_FogUpdate(max(0.0, atof(Cmd_Argv(1))), 
			CLAMP(0.0, atof(Cmd_Argv(2)), 1.0), 
			CLAMP(0.0, atof(Cmd_Argv(2)), 1.0), 
			CLAMP(0.0, atof(Cmd_Argv(2)), 1.0), 
			0.0);
		break;
	case 4:
		R_FogUpdate(gl_fogdensity.value, 
			CLAMP(0.0, atof(Cmd_Argv(1)), 1.0), 
			CLAMP(0.0, atof(Cmd_Argv(2)), 1.0), 
			CLAMP(0.0, atof(Cmd_Argv(3)), 1.0), 
			0.0);
		break;
	case 5:
		R_FogUpdate(max(0.0, atof(Cmd_Argv(1))), 
			CLAMP(0.0, atof(Cmd_Argv(2)), 1.0), 
			CLAMP(0.0, atof(Cmd_Argv(3)), 1.0), 
			CLAMP(0.0, atof(Cmd_Argv(4)), 1.0), 
			0.0);
		break;
	}
}

/*
=============
R_FogGetColor

calculates fog color for this frame
=============
*/
float *R_FogGetColor (void)
{
	static float c[4];
	int i;

	c[0] = gl_fogred.value;
	c[1] = gl_foggreen.value;
	c[2] = gl_fogblue.value;
	c[3] = 1.0;

	// find closest 24-bit RGB value, so solid-colored sky can match the fog perfectly
	for (i=0;i<3;i++)
		c[i] = (float)(Q_rint(c[i] * 255)) / 255.0f;

	return c;
}

/*
=============
R_FogGetDensity

returns current density of fog
=============
*/
float R_FogGetDensity (void)
{
	if (gl_fogenable.value)
		return gl_fogdensity.value;
	else
		return 0;
}

/*
=============
R_FogSetupFrame

called at the beginning of each frame
=============
*/
void R_FogSetupFrame (void)
{
	glFogfv(GL_FOG_COLOR, R_FogGetColor());
	glFogf(GL_FOG_DENSITY, R_FogGetDensity() / 64.0f);
}

/*
=============
R_FogEnableGFog

called before drawing stuff that should be fogged
=============
*/
void R_FogEnableGFog (void)
{
	if (R_FogGetDensity() > 0)
		glEnable(GL_FOG);
}

/*
=============
R_FogDisableGFog

called after drawing stuff that should be fogged
=============
*/
void R_FogDisableGFog (void)
{
	if (R_FogGetDensity() > 0)
		glDisable(GL_FOG);
}

/*
=============
R_FogStartAdditive

called before drawing stuff that is additive blended -- sets fog color to black
=============
*/
void R_FogStartAdditive (void)
{
	vec3_t color = {0,0,0};

	if (R_FogGetDensity() > 0)
		glFogfv(GL_FOG_COLOR, color);
}

/*
=============
R_FogStopAdditive

called after drawing stuff that is additive blended -- restores fog color
=============
*/
void R_FogStopAdditive (void)
{
	if (R_FogGetDensity() > 0)
		glFogfv(GL_FOG_COLOR, R_FogGetColor());
}


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
char	skybox_name[MAX_QPATH] = ""; // name of current skybox, or "" if no skybox
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
		data = GL_LoadImage (name, &width, &height);
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

		if (R_FogGetDensity() > 0 && r_skyfog.value > 0)
		{
			float *c = R_FogGetColor();

			glEnable (GL_BLEND);
			glDisable (GL_TEXTURE_2D);
			glColor4f (c[0],c[1],c[2], CLAMP(0.0,r_skyfog.value,1.0));

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

#define	MAX_CLIP_VERTS 64

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
	if (gl_mtexable && skyalpha >= 1.0)
	{
		GL_BindTexture (solidskytexture);
//		GL_EnableMultitexture (); // selects TEXTURE1
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

//		GL_DisableMultitexture (); // selects TEXTURE0
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

	if (R_FogGetDensity() > 0 && r_skyfog.value > 0)
	{
		float *c = R_FogGetColor();

		glEnable (GL_BLEND);
		glDisable (GL_TEXTURE_2D);
		glColor4f (c[0],c[1],c[2], CLAMP(0.0,r_skyfog.value,1.0));

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

	squarerootof3 = sqrt(3.0);
	
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
	if (!r_fastsky.value && !(R_FogGetDensity() > 0 && r_skyfog.value >= 1))
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


//==================================================================================================================

/*
=================
R_InitMapGlobals

called when quake initializes
=================
*/
void R_InitMapGlobals (void)
{
	int i;

	// clear skyboxtextures pointers
	for (i=0; i<6; i++)
		skyboxtextures[i] = NULL;

	// set up global fog
	glFogi(GL_FOG_MODE, GL_EXP2);
	glHint(GL_FOG_HINT, GL_NICEST); /*  per pixel  */
}

float globalwateralpha = 0.0;

/*
=================
R_ParseWorldspawn
=================
*/
void R_ParseWorldspawn (void)
{
	char  key[MAX_KEY], value[MAX_VALUE];
	char  *data;
	int i;

	// initially no skybox
	oldsky = true;
	strcpy (skybox_name, "");
	for (i=0; i<6; i++)
		skyboxtextures[i] = NULL;

	// initially no fog enabled
	Cvar_SetValue ("gl_fogenable", 0);

	// initially no wateralpha
	globalwateralpha = 0.0;

	data = cl.worldmodel->entities;
	if (!data)
		return;

	data = COM_Parse(data);
	if (!data) // should never happen
		return; // error

	if (com_token[0] != '{') // should never happen
		return; // error

	while (1)
	{
		data = COM_Parse(data);
		if (!data)
			return; // error

		if (com_token[0] == '}')
			break; // end of worldspawn

		if (com_token[0] == '_')
			strcpy(key, com_token + 1); // Support "_sky" and "_fog" also
		else
			strcpy(key, com_token);

		while (key[strlen(key)-1] == ' ') // remove trailing spaces
			key[strlen(key)-1] = 0;

		data = COM_Parse(data);
		if (!data)
			return; // error

		strcpy(value, com_token);

		if (!strcmp("sky", key) && value[0])
			R_LoadSkyBox(value);
		// also accept non-standard keys
		else if (!strcmp("skyname", key) && value[0]) // half-life
			R_LoadSkyBox(value);
		else if (!strcmp("qlsky", key) && value[0]) // quake lives
			R_LoadSkyBox(value);
		else if (!strcmp("fog", key) && value[0])
		{
			float density, red, green, blue;

			sscanf(value, "%f %f %f %f", &density, &red, &green, &blue);

			R_FogUpdate (density, red, green, blue, 0.0);
		}
		else if (!strcmp("wateralpha", key) && value[0])
		{
			globalwateralpha = atof (value);
		}
		else if (!strcmp("mapversion", key) && value[0])
		{
			Con_DPrintf("mapversion is %i\n", atoi(value));
		}
	}
}


//==================================================================================================================

/*
============================================================================== 

	LIGHT BLOOMS

2D lighting post process effect - adapted from fteqw
bloom (light bleeding from bright objects)
============================================================================== 
*/

/*
info about bloom algo:
bloom is basically smudging.
screen is nearest-downsampled to some usable scale and filtered to remove low-value light (this is what stops non-bright stuff from blooming)
this filtered image is then downsized multiple times
the downsized image is then blured 
the downsized images are then blured horizontally, and then vertically.
final pass simply adds each blured level to the original image.
all samples are then added together for final rendering (with some kind of tone mapping if you want proper hdr).

note: the horizontal/vertical bluring is a guassian filter
note: bloom comes from the fact that the most downsampled image doesn't have too many pixels. the pixels that it does have are spread over a large area.

http://prideout.net/archive/bloom/ contains some sample code

old link: http://www.quakesrc.org/forums/viewtopic.php?t=4340&start=0
*/


static int bloom_size;

cvar_t r_bloom					= {"r_bloom", "0", CVAR_ARCHIVE};
cvar_t r_bloom_darken			= {"r_bloom_darken", "1", CVAR_ARCHIVE};
cvar_t r_bloom_alpha			= {"r_bloom_alpha", "0.2", CVAR_ARCHIVE};
cvar_t r_bloom_intensity		= {"r_bloom_intensity", "0.8", CVAR_ARCHIVE};
cvar_t r_bloom_diamond_size		= {"r_bloom_diamond_size", "8", CVAR_ARCHIVE};
cvar_t r_bloom_sample_size		= {"r_bloom_sample_size", "64", CVAR_ARCHIVE}; // was 512
cvar_t r_bloom_fast_sample		= {"r_bloom_fast_sample", "0", CVAR_ARCHIVE};

gltexture_t *bloomscreentexture;
gltexture_t *bloomeffecttexture;
gltexture_t *bloombackuptexture;
gltexture_t *bloomdownsamplingtexture;

static int screen_downsampling_texture_size;
static int screen_texture_width, screen_texture_height;
static int screen_backup_texture_width, screen_backup_texture_height;

// texture coordinates of screen data inside screen texture
static float screen_texture_coord_width;
static float screen_texture_coord_height;

static int sample_texture_width;
static int sample_texture_height;

// texture coordinates of adjusted textures
static float sample_texture_coord_width;
static float sample_texture_coord_height;

/*
=================
R_Bloom_InitTextures
=================
*/
void R_Bloom_InitTextures (void)
{
	byte *bloomscreendata;
	byte *bloomeffectdata;
	byte *bloomdownsamplingdata;
	byte *bloombackupdata;
	int limit;
	int mark;

	// find closer power of 2 to screen size
	for (screen_texture_width = 1; screen_texture_width < glwidth; screen_texture_width <<= 1)
		;
	for (screen_texture_height = 1; screen_texture_height < glheight; screen_texture_height <<= 1)
		;

	// disable blooms if we can't handle a texture of that size
//	if (screen_texture_width > gl_texture_max_size || screen_texture_height > gl_texture_max_size)
	if (screen_texture_width > TexMgr_SafeTextureSize(screen_texture_width) || screen_texture_height > TexMgr_SafeTextureSize(screen_texture_height))
	{
		screen_texture_width = screen_texture_height = 0;
		Cvar_SetValue ("r_bloom", 0);
		Con_Warning ("R_Bloom_InitTextures: too high resolution for Light Bloom. Effect disabled\n");
		return;
	}

	mark = Hunk_LowMark ();

	// init the screen texture
	bloomscreendata = Hunk_Alloc (screen_texture_width * screen_texture_height * 4); //sizeof(int)
	bloomscreentexture = TexMgr_LoadTexture (NULL, "bloomscreentexture", screen_texture_width, screen_texture_height, SRC_BLOOM, 
										 bloomscreendata,
										 "",
										 (uintptr_t)bloomscreendata, TEXPREF_BLOOM | TEXPREF_LINEAR /* | TEXPREF_OVERWRITE */ );

	// validate bloom size
	if (r_bloom_sample_size.value < 32)
		Cvar_SetValue ("r_bloom_sample_size", 32);

	// make sure bloom size doesn't have funny values
	limit = min( (int)r_bloom_sample_size.value, min( screen_texture_width, screen_texture_height ) );

	// make sure bloom size is a power of 2
	for( bloom_size = 32; (bloom_size<<1) <= limit; bloom_size <<= 1 )
		;

	if (bloom_size != r_bloom_sample_size.value)
		Cvar_SetValue ("r_bloom_sample_size", bloom_size);

	// init the bloom effect texture
	bloomeffectdata = Hunk_Alloc (bloom_size * bloom_size * 4); //sizeof(int)
	bloomeffecttexture = TexMgr_LoadTexture (NULL, "bloomeffecttexture", bloom_size, bloom_size, SRC_BLOOM, 
										 bloomeffectdata,
										 "",
										 (uintptr_t)bloomeffectdata, TEXPREF_BLOOM | TEXPREF_LINEAR /* | TEXPREF_OVERWRITE */ );

	// if screen size is more than 2x the bloom effect texture, set up for stepped downsampling
	bloomdownsamplingtexture = NULL;
	screen_downsampling_texture_size = 0;

	if ( (glwidth > (bloom_size * 2) || glheight > (bloom_size * 2) ) && !r_bloom_fast_sample.value)
	{
		screen_downsampling_texture_size = (int)(bloom_size * 2);
		bloomdownsamplingdata = Hunk_Alloc (screen_downsampling_texture_size * screen_downsampling_texture_size * 4); //sizeof(int)
		bloomdownsamplingtexture = TexMgr_LoadTexture (NULL, "bloomdownsamplingtexture", screen_downsampling_texture_size, screen_downsampling_texture_size, SRC_BLOOM, 
												   bloomdownsamplingdata,
												   "",
												   (uintptr_t)bloomdownsamplingdata, TEXPREF_BLOOM | TEXPREF_LINEAR /* | TEXPREF_OVERWRITE */ );
	}

	// init the screen backup texture
	if (screen_downsampling_texture_size)
	{
		screen_backup_texture_width  = screen_downsampling_texture_size;
		screen_backup_texture_height = screen_downsampling_texture_size;
	}
	else
	{
		screen_backup_texture_width  = bloom_size;
		screen_backup_texture_height = bloom_size;
	}

	bloombackupdata = Hunk_Alloc (screen_backup_texture_width * screen_backup_texture_height * 4); //sizeof(int)
	bloombackuptexture = TexMgr_LoadTexture (NULL, "bloombackuptexture", screen_backup_texture_width, screen_backup_texture_height, SRC_BLOOM, 
										 bloombackupdata,
										 "",
										 (uintptr_t)bloombackupdata, TEXPREF_BLOOM | TEXPREF_LINEAR /* | TEXPREF_OVERWRITE */ );

	Hunk_FreeToLowMark (mark);
}

/*
=================
R_InitBloomTextures
=================
*/
void R_InitBloomTextures (void)
{
	bloom_size = 0;
	bloomscreentexture = NULL;	// first init

	if (!r_bloom.value)
		return;

	R_Bloom_InitTextures ();
}

/*
=================
R_Bloom_SamplePass

sample size workspace coordinates
=================
*/
static inline void R_Bloom_SamplePass (int x, int y)
{
	glBegin(GL_QUADS);
	glTexCoord2f(0, sample_texture_coord_height);
	glVertex2f(x, y);
	glTexCoord2f(0, 0);
	glVertex2f(x, y+sample_texture_height);
	glTexCoord2f(sample_texture_coord_width, 0);
	glVertex2f(x+sample_texture_width, y+sample_texture_height);
	glTexCoord2f(sample_texture_coord_width, sample_texture_coord_height);
	glVertex2f(x+sample_texture_width, y);
	glEnd();
}

/*
=================
R_Bloom_Quad
=================
*/
static inline void R_Bloom_Quad (int x, int y, int width, int height, float texwidth, float texheight)
{
	glBegin(GL_QUADS);
	glTexCoord2f(0, texheight);
	glVertex2f(x, y);
	glTexCoord2f(0, 0);
	glVertex2f(x, y+height);
	glTexCoord2f(texwidth, 0);
	glVertex2f(x+width, y+height);
	glTexCoord2f(texwidth, texheight);
	glVertex2f(x+width, y);
	glEnd();
}

/*
=================
R_Bloom_DrawEffect
=================
*/
void R_Bloom_DrawEffect (void)
{
	float	alpha;

	alpha = CLAMP(0.0, r_bloom_alpha.value, 1.0);

	GL_BindTexture (bloomeffecttexture);

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glColor4f(alpha, alpha, alpha, 1.0f);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glBegin(GL_QUADS);
	glTexCoord2f(0, sample_texture_coord_height);
	glVertex2f(glx, gly);
	glTexCoord2f(0, 0);
	glVertex2f(glx, gly + glheight);
	glTexCoord2f(sample_texture_coord_width, 0);
	glVertex2f(glx + glwidth, gly + glheight);
	glTexCoord2f(sample_texture_coord_width, sample_texture_coord_height);
	glVertex2f(glx + glwidth, gly);
	glEnd();

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_BLEND);
}

/*
=================
R_Bloom_GeneratexDiamonds
=================
*/
void R_Bloom_GeneratexDiamonds (void)
{
	int		i, j;
	float	intensity, scale, rad, point;

	// setup sample size workspace
	glViewport (0, 0, sample_texture_width, sample_texture_height);

	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();

	glOrtho (0, sample_texture_width, sample_texture_height, 0, -10, 100);

	glMatrixMode (GL_MODELVIEW);
	glLoadIdentity ();

	// copy small scene into bloomeffecttexture
	GL_BindTexture (bloomeffecttexture);
	glCopyTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_texture_width, sample_texture_height);

	// start modifying the small scene corner
	glColor4f (1.0f, 1.0f, 1.0f, 1.0f);
	glEnable (GL_BLEND);

	// darkening passes
	if (r_bloom_darken.value < 0)
		Cvar_SetValue("r_bloom_darken", 0);

    glBlendFunc (GL_DST_COLOR, GL_ZERO);
    glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    for (i = 0; i < r_bloom_darken.value; i++) 
    {
        R_Bloom_SamplePass (0, 0);
    }

    glCopyTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_texture_width, sample_texture_height);

	// bluring passes
    if (r_bloom_diamond_size.value < 2)
        Cvar_SetValue("r_bloom_diamond_size", 2);
    if (r_bloom_intensity.value < 0)
        Cvar_SetValue("r_bloom_intensity", 0);

    rad = r_bloom_diamond_size.value / 2.0f;
    point = (r_bloom_diamond_size.value - 1) / 2.0f;
    scale = min(1.0f, r_bloom_intensity.value * 2.0f / rad);

	glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_COLOR);

	for (i = 0; i < r_bloom_diamond_size.value; i++) 
	{
		for (j = 0; j < r_bloom_diamond_size.value; j++) 
		{
			intensity = scale * ((point + 1.0f) - (fabs(point - i) + fabs(point - j))) / (point + 1.0f);
			if (intensity < 0.005f)
				continue;

			glColor4f (intensity, intensity, intensity, 1.0f);
			R_Bloom_SamplePass (i - rad, j - rad);
		}
	}

	glCopyTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_texture_width, sample_texture_height);

	glDisable (GL_BLEND);
	glColor4f (1.0f, 1.0f, 1.0f, 1.0f);
}

/*
=================
R_Bloom_DownsampleView
=================
*/
void R_Bloom_DownsampleView (void)
{
	//
	// setup textures coordinates
	//
	screen_texture_coord_width = ((float)glwidth / (float)screen_texture_width);
	screen_texture_coord_height = ((float)glheight / (float)screen_texture_height);

	if (glheight > glwidth)
	{
		sample_texture_coord_width = ((float)glwidth / (float)glheight);
		sample_texture_coord_height = 1.0f;
	}
	else
	{
		sample_texture_coord_width = 1.0f;
		sample_texture_coord_height = ((float)glheight / (float)glwidth);
	}

	sample_texture_width = ( bloom_size * sample_texture_coord_width );
	sample_texture_height = ( bloom_size * sample_texture_coord_height );

//	glDisable (GL_BLEND);
//	glColor4f (1.0f, 1.0f, 1.0f, 1.0f);

	if (screen_downsampling_texture_size)
	{
		// stepped downsample
		int midsample_texture_width = ( screen_downsampling_texture_size * sample_texture_coord_width );
		int midsample_texture_height = ( screen_downsampling_texture_size * sample_texture_coord_height );

		// copy the screen and draw resized
		GL_BindTexture (bloomscreentexture);
		glCopyTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, glx, glheight - (gly + glheight), glwidth, glheight);
		R_Bloom_Quad (0, glheight - midsample_texture_height, midsample_texture_width, midsample_texture_height, screen_texture_coord_width, screen_texture_coord_height);

		// now copy into downsampling (mid-sized) texture
		GL_BindTexture (bloomdownsamplingtexture);
		glCopyTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, 0, 0, midsample_texture_width, midsample_texture_height);

		// now draw again in bloom size
		glColor4f (0.5f, 0.5f, 0.5f, 1.0f);
		R_Bloom_Quad (0, glheight - sample_texture_height, sample_texture_width, sample_texture_height, sample_texture_coord_width, sample_texture_coord_height);

		// now blend the big screen texture into the bloom generation space (hoping it adds some blur)
		glEnable (GL_BLEND);
		glBlendFunc (GL_ONE, GL_ONE);
		glColor4f (0.5f, 0.5f, 0.5f, 1.0f);
		GL_BindTexture (bloomscreentexture);
		R_Bloom_Quad (0, glheight - sample_texture_height, sample_texture_width, sample_texture_height, screen_texture_coord_width, screen_texture_coord_height);
		glColor4f (1.0f, 1.0f, 1.0f, 1.0f);
		glDisable (GL_BLEND);
	} 
	else
	{
		// downsample simple
		GL_BindTexture (bloomscreentexture);
		glCopyTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, glx, glheight - (gly + glheight), glwidth, glheight);
		R_Bloom_Quad (0, glheight - sample_texture_height, sample_texture_width, sample_texture_height, screen_texture_coord_width, screen_texture_coord_height);
	}
}

/*
=================
R_BloomBlend
=================
*/
void R_BloomBlend (void)
{
	if (!r_bloom.value)
		return;

	if (!bloom_size || screen_texture_width < glwidth || screen_texture_height < glheight)
		R_Bloom_InitTextures ();

	// previous function can unset this
	if (!r_bloom.value)
		return;

	if (screen_texture_width < bloom_size || screen_texture_height < bloom_size)
		return;

	//
	// setup full screen workspace
	//
	glViewport (0, 0, glwidth, glheight);

	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();

	glOrtho (0, glwidth, glheight, 0, -10, 100);

	glMatrixMode (GL_MODELVIEW);
	glLoadIdentity ();

	//
	// set drawing parms
	// 
	glDisable (GL_DEPTH_TEST);
	glDisable (GL_CULL_FACE);
//	glEnable (GL_TEXTURE_2D);
//	glDisable (GL_BLEND);
//	glColor4f (1.0f, 1.0f, 1.0f, 1.0f);

	//
	// copy the screen space we'll use to work into the backup texture
	//
	GL_BindTexture (bloombackuptexture);
	glCopyTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, 0, 0, screen_backup_texture_width, screen_backup_texture_height);

	//
	// create the bloom image
	//
	R_Bloom_DownsampleView ();
	R_Bloom_GeneratexDiamonds ();

	//
	// restore full screen workspace
	//
	glViewport (0, 0, glwidth, glheight);

	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();

	glOrtho (0, glwidth, glheight, 0, -10, 100);

	glMatrixMode (GL_MODELVIEW);
	glLoadIdentity ();

	//
	// restore the screen-backup to the screen
	//
	GL_BindTexture (bloombackuptexture);
	R_Bloom_Quad (0, glheight - screen_backup_texture_height, screen_backup_texture_width, screen_backup_texture_height, 1.0f, 1.0f);
	
	// draw the bloom effect
	R_Bloom_DrawEffect ();
}

