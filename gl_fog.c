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
// gl_fog.c -- global fog

#include "quakedef.h"

#define DEFAULT_DENSITY 0.0
#define DEFAULT_GRAY 0.3

static float old_density;
static float old_red;
static float old_green;
static float old_blue;

static float fade_time; //duration of fade
static float fade_done; //time when fade will be done

// Nehahra
cvar_t	gl_fogenable = {"gl_fogenable", "0", CVAR_NONE};
cvar_t	gl_fogdensity = {"gl_fogdensity", "0", CVAR_NONE};
cvar_t	gl_fogred = {"gl_fogred","0.3", CVAR_NONE};
cvar_t	gl_foggreen = {"gl_foggreen","0.3", CVAR_NONE};
cvar_t	gl_fogblue = {"gl_fogblue","0.3", CVAR_NONE};

/*
==============================================================================

	FOG DRAW

==============================================================================
*/

/*
=============
R_FogReset

reset fog variables to default
called at map load
=============
*/
void R_FogReset (void)
{
	Cvar_SetValue ("gl_fogenable", 0);
	Cvar_SetValue ("gl_fogdensity", DEFAULT_DENSITY);
	Cvar_SetValue ("gl_fogred", DEFAULT_GRAY);
	Cvar_SetValue ("gl_foggreen", DEFAULT_GRAY);
	Cvar_SetValue ("gl_fogblue", DEFAULT_GRAY);
	
	old_density = DEFAULT_DENSITY;
	old_red = DEFAULT_GRAY;
	old_green = DEFAULT_GRAY;
	old_blue = DEFAULT_GRAY;
	
	fade_time = 0.0;
	fade_done = 0.0;
}

/*
=============
R_FogUpdate

update internal variables
=============
*/
void R_FogUpdate (float density, float red, float green, float blue, float time)
{
	//save previous settings for fade
	if (time > 0)
	{
		//check for a fade in progress
		if (fade_done > cl.time)
		{
			float f;

			f = (fade_done - cl.time) / fade_time;
			old_density = f * old_density + (1.0 - f) * gl_fogdensity.value;
			old_red = f * old_red + (1.0 - f) * gl_fogred.value;
			old_green = f * old_green + (1.0 - f) * gl_foggreen.value;
			old_blue = f * old_blue + (1.0 - f) * gl_fogblue.value;
		}
		else
		{
			old_density = gl_fogdensity.value;
			old_red = gl_fogred.value;
			old_green = gl_foggreen.value;
			old_blue = gl_fogblue.value;
		}
	}
	
	Cvar_SetValue ("gl_fogenable", time || density ? 1 : 0);
	Cvar_SetValue ("gl_fogdensity", density);
	Cvar_SetValue ("gl_fogred", red);
	Cvar_SetValue ("gl_foggreen", green);
	Cvar_SetValue ("gl_fogblue", blue);
	
	fade_time = time;
	fade_done = cl.time + time;
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
	time = MSG_ReadShort(net_message) / 100.0;
	if (time < 0.0f) time = 0.0f;

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
	float d, r, g, b, t;

	switch (Cmd_Argc())
	{
	default:
	case 1:
		Con_Printf ("usage:\n");
		Con_Printf ("   fog <density>\n");
		if (nehahra)
			Con_Printf ("   fog <density> <rgb>\n"); // neh RGB
		else
			Con_Printf ("   fog <density> <duration>\n");
		Con_Printf ("   fog <red> <green> <blue>\n");
		Con_Printf ("   fog <density> <red> <green> <blue>\n");
		Con_Printf ("   fog <density> <red> <green> <blue> <duration>\n");
		Con_Printf("current values:\n");
		Con_Printf("   fog is %sabled\n", gl_fogenable.value ? "en" : "dis");
		Con_Printf("   density is %f\n", gl_fogdensity.value);
		Con_Printf("   red   is %f\n", gl_fogred.value);
		Con_Printf("   green is %f\n", gl_foggreen.value);
		Con_Printf("   blue  is %f\n", gl_fogblue.value);
		return;
	case 2:
		d = atof(Cmd_Argv(1));
		t = 0.0f;
		r = gl_fogred.value;
		g = gl_foggreen.value;
		b = gl_fogblue.value;
		break;
	case 3: //TEST
		if (nehahra)
		{
			// neh RGB
			d = atof(Cmd_Argv(1));
			r = atof(Cmd_Argv(2));
			g = atof(Cmd_Argv(2));
			b = atof(Cmd_Argv(2));
			t = 0.0f;
		}
		else
		{
			d = atof(Cmd_Argv(1));
			t = atof(Cmd_Argv(2));
			r = gl_fogred.value;
			g = gl_foggreen.value;
			b = gl_fogblue.value;
		}
		break;
	case 4:
		d = gl_fogdensity.value;
		t = 0.0f;
		r = atof(Cmd_Argv(1));
		g = atof(Cmd_Argv(2));
		b = atof(Cmd_Argv(3));
		break;
	case 5:
		d = atof(Cmd_Argv(1));
		r = atof(Cmd_Argv(2));
		g = atof(Cmd_Argv(3));
		b = atof(Cmd_Argv(4));
		t = 0.0f;
		break;
	case 6: //TEST
		d = atof(Cmd_Argv(1));
		r = atof(Cmd_Argv(2));
		g = atof(Cmd_Argv(3));
		b = atof(Cmd_Argv(4));
		t = atof(Cmd_Argv(5));
		break;
	}
	
	if      (d < 0.0f) d = 0.0f;
	if      (r < 0.0f) r = 0.0f;
	else if (r > 1.0f) r = 1.0f;
	if      (g < 0.0f) g = 0.0f;
	else if (g > 1.0f) g = 1.0f;
	if      (b < 0.0f) b = 0.0f;
	else if (b > 1.0f) b = 1.0f;
	
	R_FogUpdate (d, r, g, b, t);
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
	float f;
	int i;

	if (fade_done > cl.time)
	{
		f = (fade_done - cl.time) / fade_time;
		c[0] = f * old_red + (1.0 - f) * gl_fogred.value;
		c[1] = f * old_green + (1.0 - f) * gl_foggreen.value;
		c[2] = f * old_blue + (1.0 - f) * gl_fogblue.value;
		c[3] = 1.0;
	}
	else
	{
		c[0] = gl_fogred.value;
		c[1] = gl_foggreen.value;
		c[2] = gl_fogblue.value;
		c[3] = 1.0;
	}

	for (i=0;i<3;i++)
		c[i] = CLAMP (0.f, c[i], 1.f);

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
	float f;

	if (gl_fogenable.value)
	{
		if (fade_done > cl.time)
		{
			f = (fade_done - cl.time) / fade_time;
			return f * old_density + (1.0 - f) * gl_fogdensity.value;
		}
		else
			return gl_fogdensity.value;
	}
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

