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
// gl_fog.c -- global and volumetric fog

#include "quakedef.h"


// Nehahra
cvar_t	gl_fogenable = {"gl_fogenable", "0", CVAR_NONE};
cvar_t	gl_fogdensity = {"gl_fogdensity", "0", CVAR_NONE};
cvar_t	gl_fogred = {"gl_fogred","0.5", CVAR_NONE};
cvar_t	gl_foggreen = {"gl_foggreen","0.5", CVAR_NONE};
cvar_t	gl_fogblue = {"gl_fogblue","0.5", CVAR_NONE};

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
//void R_FogStartAdditive (void)
//{
//	vec3_t color = {0,0,0};
//
//	if (R_FogGetDensity() > 0)
//		glFogfv(GL_FOG_COLOR, color);
//}

/*
=============
R_FogStopAdditive

called after drawing stuff that is additive blended -- restores fog color
=============
*/
//void R_FogStopAdditive (void)
//{
//	if (R_FogGetDensity() > 0)
//		glFogfv(GL_FOG_COLOR, R_FogGetColor());
//}

