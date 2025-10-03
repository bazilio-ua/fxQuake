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
// view.c -- player eye positioning

#include "quakedef.h"

/*

The view is allowed to move slightly from it's true position for bobbing,
but if it exceeds 8 pixels linear distance (spherical, not box), the list of
entities sent from the server may not include everything in the pvs, especially
when crossing a water boudnary.

*/

cvar_t	scr_ofsx = {"scr_ofsx","0", CVAR_NONE};
cvar_t	scr_ofsy = {"scr_ofsy","0", CVAR_NONE};
cvar_t	scr_ofsz = {"scr_ofsz","0", CVAR_NONE};

cvar_t	cl_rollspeed = {"cl_rollspeed", "200", CVAR_NONE};
cvar_t	cl_rollangle = {"cl_rollangle", "2.0", CVAR_NONE};

cvar_t	cl_bob = {"cl_bob","0.02", CVAR_NONE};
cvar_t	cl_bobcycle = {"cl_bobcycle","0.6", CVAR_NONE};
cvar_t	cl_bobup = {"cl_bobup","0.5", CVAR_NONE};

cvar_t	v_kicktime = {"v_kicktime", "0.5", CVAR_NONE};
cvar_t	v_kickroll = {"v_kickroll", "0.6", CVAR_NONE};
cvar_t	v_kickpitch = {"v_kickpitch", "0.6", CVAR_NONE};
cvar_t	v_gunkick = {"v_gunkick", "1", CVAR_ARCHIVE}; //johnfitz

cvar_t	v_iyaw_cycle = {"v_iyaw_cycle", "2", CVAR_NONE};
cvar_t	v_iroll_cycle = {"v_iroll_cycle", "0.5", CVAR_NONE};
cvar_t	v_ipitch_cycle = {"v_ipitch_cycle", "1", CVAR_NONE};
cvar_t	v_iyaw_level = {"v_iyaw_level", "0.3", CVAR_NONE};
cvar_t	v_iroll_level = {"v_iroll_level", "0.1", CVAR_NONE};
cvar_t	v_ipitch_level = {"v_ipitch_level", "0.3", CVAR_NONE};

cvar_t	v_idlescale = {"v_idlescale", "0", CVAR_NONE};

cvar_t	crosshair = {"crosshair", "0", CVAR_ARCHIVE};
cvar_t	cl_crossx = {"cl_crossx", "0", CVAR_NONE};
cvar_t	cl_crossy = {"cl_crossy", "0", CVAR_NONE};

cvar_t	gl_cshiftpercent = {"gl_cshiftpercent", "100", CVAR_NONE};

cvar_t	v_contentblend = {"v_contentblend", "1", CVAR_NONE};

float	v_dmg_time, v_dmg_roll, v_dmg_pitch;

extern	int			in_forward, in_forward2, in_back;

vec3_t	v_punchangles[2]; //johnfitz -- copied from cl.punchangle.  0 is current, 1 is previous value. never the same unless map just loaded

/*
===============
V_CalcRoll

Used by view and sv_user
===============
*/
float V_CalcRoll (vec3_t angles, vec3_t velocity)
{
	vec3_t	forward, right, up;
	float	sign;
	float	side;
	float	value;
	
	AngleVectors (angles, forward, right, up);
	side = DotProduct (velocity, right);
	sign = side < 0 ? -1 : 1;
	side = fabs(side);
	
	value = cl_rollangle.value;
//	if (cl.inwater)
//		value *= 6;

	if (side < cl_rollspeed.value)
		side = side * value / cl_rollspeed.value;
	else
		side = value;
	
	return side*sign;
	
}


/*
===============
V_CalcBob

===============
*/
float V_CalcBob (void)
{
	float	bob;
	float	cycle;

	if (!cl_bobcycle.value) // avoid divide by zero, don't bob
		return 0;

	cycle = cl.time - (int)(cl.time/cl_bobcycle.value)*cl_bobcycle.value;
	cycle /= cl_bobcycle.value;
	if (cycle < cl_bobup.value)
		cycle = M_PI * cycle / cl_bobup.value;
	else
		cycle = M_PI + M_PI*(cycle-cl_bobup.value)/(1.0 - cl_bobup.value);

// bob is proportional to velocity in the xy plane
// (don't count Z, or jumping messes it up)

	bob = sqrt(cl.velocity[0]*cl.velocity[0] + cl.velocity[1]*cl.velocity[1]) * cl_bob.value;
	//Con_Printf ("speed: %5.1f\n", VectorLength(cl.velocity));
	bob = bob*0.3 + bob*0.7*sin(cycle);
	if (bob > 4)
		bob = 4;
	else if (bob < -7)
		bob = -7;
	return bob;
	
}


//=============================================================================


cvar_t	v_centermove = {"v_centermove", "0.15", CVAR_NONE};
cvar_t	v_centerspeed = {"v_centerspeed","500", CVAR_NONE};


void V_StartPitchDrift (void)
{
	if (cl.laststop == cl.time)
	{
		return;		// something else is keeping it from drifting
	}

	if (cl.nodrift || !cl.pitchvel)
	{
		cl.pitchvel = v_centerspeed.value;
		cl.nodrift = false;
		cl.driftmove = 0;
	}
}

void V_StopPitchDrift (void)
{
	cl.laststop = cl.time;
	cl.nodrift = true;
	cl.pitchvel = 0;
}

/*
===============
V_DriftPitch

Moves the client pitch angle towards cl.idealpitch sent by the server.

If the user is adjusting pitch manually, either with lookup/lookdown,
mlook and mouse, or klook and keyboard, pitch drifting is constantly stopped.

Drifting is enabled when the center view key is hit, mlook is released and
lookspring is non 0, or when 
===============
*/
void V_DriftPitch (void)
{
	float		delta, move;

	if (cl.noclip_anglehack || !cl.onground || cls.demoplayback )
	//FIXME: noclip_anglehack is set on the server, so in a nonlocal game this won't work.
	{
		cl.driftmove = 0;
		cl.pitchvel = 0;
		return;
	}

// don't count small mouse motion
	if (cl.nodrift)
	{
		if ( fabs(cl.cmd.forwardmove) < cl_forwardspeed.value)
			cl.driftmove = 0;
		else
			cl.driftmove += host_frametime;
	
		if ( cl.driftmove > v_centermove.value)
		{
			if (lookspring.value) //jkrige - mlook and lookspring fix
				V_StartPitchDrift ();
		}
		return;
	}
	
	delta = cl.idealpitch - cl.viewangles[PITCH];

	if (!delta)
	{
		cl.pitchvel = 0;
		return;
	}

	move = host_frametime * cl.pitchvel;
	cl.pitchvel += host_frametime * v_centerspeed.value;
	
//Con_Printf ("move: %f (%f)\n", move, host_frametime);

	if (delta > 0)
	{
		if (move > delta)
		{
			cl.pitchvel = 0;
			move = delta;
		}
		cl.viewangles[PITCH] += move;
	}
	else if (delta < 0)
	{
		if (move > -delta)
		{
			cl.pitchvel = 0;
			move = -delta;
		}
		cl.viewangles[PITCH] -= move;
	}
}

/*
============================================================================== 
 
	VIEW BLENDING

============================================================================== 
*/ 
 
cshift_t	cshift_empty = { {0,0,0}, 0 };
cshift_t	cshift_water = { {130,80,50}, 128 };
cshift_t	cshift_slime = { {0,25,5}, 150 };
cshift_t	cshift_lava = { {255,80,0}, 150 };

cvar_t		v_gamma = {"gamma", "1", CVAR_ARCHIVE};
cvar_t		v_contrast = {"contrast", "1", CVAR_ARCHIVE}; // QuakeSpasm, MarkV

byte		gammatable[256];	// palette is sent through this

float		v_blend[4];		// rgba 0.0 - 1.0


void BuildGammaTable (float gamma, float contrast)
{
	int		i;

	// Refresh gamma table
	for (i=0 ; i<256 ; i++)
		gammatable[i] = CLAMP(0, (int)((255 * pow ((i+0.5)/255.5, gamma) + 0.5) * contrast), 255);
}

/*
=================
V_CheckGamma
=================
*/
qboolean V_CheckGamma (void)
{
	static float oldgamma;
	static float oldcontrast;
	
	if (v_gamma.value == oldgamma && v_contrast.value == oldcontrast)
		return false;
	
	oldgamma = v_gamma.value;
	oldcontrast = v_contrast.value;
	
	BuildGammaTable (v_gamma.value, v_contrast.value);
	vid.recalc_refdef = true;				// force a surface cache flush
	
	return true;
}


/*
===============
V_ParseDamage
===============
*/
void V_ParseDamage (void)
{
	int		armor, blood;
	vec3_t	from;
	int		i;
	vec3_t	forward, right, up;
	entity_t	*ent;
	float	side;
	float	count;
	
	armor = MSG_ReadByte (net_message);
	blood = MSG_ReadByte (net_message);
	for (i=0 ; i<3 ; i++)
		from[i] = MSG_ReadCoord (net_message, cl.protocolflags);

	count = blood*0.5 + armor*0.5;
	if (count < 10)
		count = 10;

	cl.faceanimtime = cl.time + 0.2;		// but sbar face into pain frame

	cl.cshifts[CSHIFT_DAMAGE].percent += 3*count;
	if (cl.cshifts[CSHIFT_DAMAGE].percent < 0)
		cl.cshifts[CSHIFT_DAMAGE].percent = 0;
	if (cl.cshifts[CSHIFT_DAMAGE].percent > 150)
		cl.cshifts[CSHIFT_DAMAGE].percent = 150;

	if (armor > blood)		
	{
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 200;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 100;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 100;
	}
	else if (armor)
	{
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 220;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 50;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 50;
	}
	else
	{
		cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 255;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 0;
		cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 0;
	}

//
// calculate view angle kicks
//
	ent = &cl_entities[cl.viewentity];
	
	VectorSubtract (from, ent->origin, from);
	VectorNormalize (from);
	
	AngleVectors (ent->angles, forward, right, up);

	side = DotProduct (from, right);
	v_dmg_roll = count*side*v_kickroll.value;
	
	side = DotProduct (from, forward);
	v_dmg_pitch = count*side*v_kickpitch.value;

	v_dmg_time = v_kicktime.value;
}


/*
==================
V_cshift_f
==================
*/
void V_cshift_f (void)
{
	cshift_empty.destcolor[0] = atoi(Cmd_Argv(1));
	cshift_empty.destcolor[1] = atoi(Cmd_Argv(2));
	cshift_empty.destcolor[2] = atoi(Cmd_Argv(3));
	cshift_empty.percent = atoi(Cmd_Argv(4));
}


/*
==================
V_BonusFlash_f

When you run over an item, the server sends this command
==================
*/
void V_BonusFlash_f (void)
{
	cl.cshifts[CSHIFT_BONUS].destcolor[0] = 215;
	cl.cshifts[CSHIFT_BONUS].destcolor[1] = 186;
	cl.cshifts[CSHIFT_BONUS].destcolor[2] = 69;
	cl.cshifts[CSHIFT_BONUS].percent = 50;
}

/*
=============
V_SetContentsColor

Underwater, lava, etc each has a color shift
=============
*/
void V_SetContentsColor (int contents)
{
	if (!v_contentblend.value) {
		cl.cshifts[CSHIFT_CONTENTS] = cshift_empty;
		return;
	}
	
	switch (contents)
	{
	case CONTENTS_EMPTY:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_empty;
		break;
	case CONTENTS_LAVA:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_lava;
		break;
	case CONTENTS_SLIME:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_slime;
		break;
	case CONTENTS_WATER:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_water;
		break;
	default:
		cl.cshifts[CSHIFT_CONTENTS] = cshift_empty;
	}
}

/*
=============
V_CalcPowerupCshift
=============
*/
void V_CalcPowerupCshift (void)
{
	if (cl.items & IT_QUAD)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 255;
		cl.cshifts[CSHIFT_POWERUP].percent = 30;
	}
	else if (cl.items & IT_SUIT)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
		cl.cshifts[CSHIFT_POWERUP].percent = 20;
	}
	else if (cl.items & IT_INVISIBILITY)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 100;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 100;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 100;
		cl.cshifts[CSHIFT_POWERUP].percent = 100;
	}
	else if (cl.items & IT_INVULNERABILITY)
	{
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
		cl.cshifts[CSHIFT_POWERUP].percent = 30;
	}
	else
		cl.cshifts[CSHIFT_POWERUP].percent = 0;
}

/*
=============
V_CalcBlend
=============
*/
void V_CalcBlend (void)
{
	float	r, g, b, a, a2;
	int		j;

	r = 0;
	g = 0;
	b = 0;
	a = 0;

	for (j=0 ; j<NUM_CSHIFTS ; j++)	
	{
		if (!gl_cshiftpercent.value)
			continue;

		//johnfitz -- only apply leaf contents color shifts during intermission
		if (cl.intermission && j != CSHIFT_CONTENTS)
			continue;

		a2 = ((cl.cshifts[j].percent * gl_cshiftpercent.value) / 100.0) / 255.0;
		if (!a2)
			continue;
		a = a + a2*(1-a);
//Con_Printf ("j:%i a:%f\n", j, a);
		a2 = a2/a;
		r = r*(1-a2) + cl.cshifts[j].destcolor[0]*a2;
		g = g*(1-a2) + cl.cshifts[j].destcolor[1]*a2;
		b = b*(1-a2) + cl.cshifts[j].destcolor[2]*a2;
	}

	v_blend[0] = r/255.0;
	v_blend[1] = g/255.0;
	v_blend[2] = b/255.0;
	v_blend[3] = a;
	if (v_blend[3] > 1)
		v_blend[3] = 1;
	if (v_blend[3] < 0)
		v_blend[3] = 0;
}

/*
=============
V_UpdateBlend

cleaned up and renamed V_UpdatePalette
=============
*/
void V_UpdateBlend (void)
{
	int		i, j;
	qboolean	changed;

	V_CalcPowerupCshift ();

	changed = false;

	for (i=0 ; i<NUM_CSHIFTS ; i++)
	{
		if (cl.cshifts[i].percent != cl.prev_cshifts[i].percent)
		{
			changed = true;
			cl.prev_cshifts[i].percent = cl.cshifts[i].percent;
		}
		for (j=0 ; j<3 ; j++)
			if (cl.cshifts[i].destcolor[j] != cl.prev_cshifts[i].destcolor[j])
			{
				changed = true;
				cl.prev_cshifts[i].destcolor[j] = cl.cshifts[i].destcolor[j];
			}
	}

// drop the damage value
	cl.cshifts[CSHIFT_DAMAGE].percent -= host_frametime*150;
	if (cl.cshifts[CSHIFT_DAMAGE].percent <= 0)
		cl.cshifts[CSHIFT_DAMAGE].percent = 0;

// drop the bonus value
	cl.cshifts[CSHIFT_BONUS].percent -= host_frametime*100;
	if (cl.cshifts[CSHIFT_BONUS].percent <= 0)
		cl.cshifts[CSHIFT_BONUS].percent = 0;

	if (changed)
		V_CalcBlend ();
}

/*
================
V_ReloadPalette
================
*/
void V_ReloadPalette (void)
{
	int		i;
	byte	*basepal, *newpal;
	byte	pal[768];
	int		ir, ig, ib;

	basepal = host_basepal;
	newpal = pal;
	
	for (i=0 ; i<256 ; i++)
	{
		ir = basepal[0];
		ig = basepal[1];
		ib = basepal[2];
		basepal += 3;
		
		newpal[0] = gammatable[ir];
		newpal[1] = gammatable[ig];
		newpal[2] = gammatable[ib];
		newpal += 3;
	}

	V_ShiftPalette (pal);
}

/*
================
V_UpdateGamma

callback when the gamma/contrast cvar changes
================
*/
void V_UpdateGamma (void)
{
	qboolean force;

	force = V_CheckGamma ();
	
	if (force)
	{
		V_ReloadPalette ();
	}
}

void V_ShiftPalette (byte *palette)
{
	V_SetPalette (palette);
	TexMgr_ReloadTextures ();
}

void SetPaletteColor (unsigned int *dst, byte r, byte g, byte b, byte a)
{
	((byte *)dst)[0] = r;
	((byte *)dst)[1] = g;
	((byte *)dst)[2] = b;
	((byte *)dst)[3] = a;
}

void V_SetPalette (byte *palette)
{
	byte *pal, *src;
	int i;

	pal = palette;
	
	//
	// fill color tables
	//
	src = pal;
	for (i = 0; i < 256; i++, src += 3)
	{
		// standard palette with alpha 255 for all colors
		SetPaletteColor (&d_8to24table_opaque[i], src[0], src[1], src[2], 255);
		if (GetBit (is_fullbright, i))
		{
			SetPaletteColor (&d_8to24table_fullbright[i], src[0], src[1], src[2], 255);
			// nobright palette, fullbright indices (224-255) are black (for additive blending)
			SetPaletteColor (&d_8to24table_nobright[i], 0, 0, 0, 255);
		}
		else
		{
			// fullbright palette, nobright indices (0-223) are black (for additive blending)
			SetPaletteColor (&d_8to24table_fullbright[i], 0, 0, 0, 255);
			SetPaletteColor (&d_8to24table_nobright[i], src[0], src[1], src[2], 255);
		}
	}
	
	// standard palette, 255 is transparent
	memcpy (d_8to24table, d_8to24table_opaque, 256*4);
	((byte *)&d_8to24table[255])[3] = 0;
	
	// fullbright palette, for holey textures (fence)
	memcpy (d_8to24table_fullbright_holey, d_8to24table_fullbright, 256*4);
	d_8to24table_fullbright_holey[255] = 0; // Alpha of zero.
	
	// nobright palette, for holey textures (fence)
	memcpy (d_8to24table_nobright_holey, d_8to24table_nobright, 256*4);
	d_8to24table_nobright_holey[255] = 0; // Alpha of zero.
	
	// conchars palette, 0 and 255 are transparent
	memcpy (d_8to24table_conchars, d_8to24table, 256*4);
	((byte *)&d_8to24table_conchars[0])[3] = 0;
}

void V_SetOriginalPalette (void)
{
	byte *pal, *src;
	int i;

	pal = host_basepal;

	//
	// fill color table
	//
	src = pal;
	for (i = 0; i < 256; i++, src += 3)
	{
		// standard palette - no transparency
		SetPaletteColor (&d_8to24table_original[i], src[0], src[1], src[2], 255);
	}

	// keep original table untouched from palette shifting by gamma changes
	// used in flood fill skin routine to detect black pixels
}

/*
==================
V_FindFullbrightColors
 
Use colormap to determine which colors are fullbright
instead of using a hardcoded index threshold of 224
==================
*/
void V_FindFullbrightColors (void)
{
	byte *pal, *src;
	byte *colormap;
	int i, j, numfb;
	
	pal = host_basepal;
	colormap = host_colormap;
	
	//
	// find fullbright colors
	//
	memset (is_fullbright, 0, sizeof (is_fullbright));
	numfb = 0;
	src = pal;
	for (i = 0; i < 256; i++, src += 3)
	{
		if (!src[0] && !src[1] && !src[2])
			continue; // black can't be fullbright
		
		for (j = 1; j < 64; j++)
			if (colormap[i + j * 256] != colormap[i])
				break;
		
		if (j == 64)
		{
			SetBit (is_fullbright, i);
			numfb++;
		}
	}
	
	Con_DPrintf ("Colormap has %d fullbright colors\n", numfb);
}

/*
============================================================================== 
 
						VIEW RENDERING 
 
============================================================================== 
*/ 

float angledelta (float a)
{
	a = anglemod(a);
	if (a > 180)
		a -= 360;
	return a;
}

/*
==================
CalcGunAngle
==================
*/
void CalcGunAngle (void)
{	
	float	yaw, pitch, move;
	static float oldyaw = 0;
	static float oldpitch = 0;
	
	yaw = r_refdef.viewangles[YAW];
	pitch = -r_refdef.viewangles[PITCH];

	yaw = angledelta(yaw - r_refdef.viewangles[YAW]) * 0.4;
	if (yaw > 10)
		yaw = 10;
	if (yaw < -10)
		yaw = -10;
	pitch = angledelta(-pitch - r_refdef.viewangles[PITCH]) * 0.4;
	if (pitch > 10)
		pitch = 10;
	if (pitch < -10)
		pitch = -10;
	move = host_frametime*20;
	if (yaw > oldyaw)
	{
		if (oldyaw + move < yaw)
			yaw = oldyaw + move;
	}
	else
	{
		if (oldyaw - move > yaw)
			yaw = oldyaw - move;
	}
	
	if (pitch > oldpitch)
	{
		if (oldpitch + move < pitch)
			pitch = oldpitch + move;
	}
	else
	{
		if (oldpitch - move > pitch)
			pitch = oldpitch - move;
	}
	
	oldyaw = yaw;
	oldpitch = pitch;

	cl.viewent.angles[YAW] = r_refdef.viewangles[YAW] + yaw;
	cl.viewent.angles[PITCH] = - (r_refdef.viewangles[PITCH] + pitch);

	cl.viewent.angles[ROLL] -= v_idlescale.value * sin(cl.time*v_iroll_cycle.value) * v_iroll_level.value;
	cl.viewent.angles[PITCH] -= v_idlescale.value * sin(cl.time*v_ipitch_cycle.value) * v_ipitch_level.value;
	cl.viewent.angles[YAW] -= v_idlescale.value * sin(cl.time*v_iyaw_cycle.value) * v_iyaw_level.value;
}

/*
==============
V_BoundOffsets
==============
*/
void V_BoundOffsets (void)
{
	entity_t	*ent;
	
	ent = &cl_entities[cl.viewentity];

// absolutely bound refresh reletive to entity clipping hull
// so the view can never be inside a solid wall

	if (r_refdef.vieworg[0] < ent->origin[0] - 14)
		r_refdef.vieworg[0] = ent->origin[0] - 14;
	else if (r_refdef.vieworg[0] > ent->origin[0] + 14)
		r_refdef.vieworg[0] = ent->origin[0] + 14;
	if (r_refdef.vieworg[1] < ent->origin[1] - 14)
		r_refdef.vieworg[1] = ent->origin[1] - 14;
	else if (r_refdef.vieworg[1] > ent->origin[1] + 14)
		r_refdef.vieworg[1] = ent->origin[1] + 14;
	if (r_refdef.vieworg[2] < ent->origin[2] - 22)
		r_refdef.vieworg[2] = ent->origin[2] - 22;
	else if (r_refdef.vieworg[2] > ent->origin[2] + 30)
		r_refdef.vieworg[2] = ent->origin[2] + 30;
}

/*
==============
V_AddIdle

Idle swaying
==============
*/
void V_AddIdle (void)
{
	r_refdef.viewangles[ROLL] += v_idlescale.value * sin(cl.time*v_iroll_cycle.value) * v_iroll_level.value;
	r_refdef.viewangles[PITCH] += v_idlescale.value * sin(cl.time*v_ipitch_cycle.value) * v_ipitch_level.value;
	r_refdef.viewangles[YAW] += v_idlescale.value * sin(cl.time*v_iyaw_cycle.value) * v_iyaw_level.value;
}


/*
==============
V_CalcViewRoll

Roll is induced by movement and damage
==============
*/
void V_CalcViewRoll (void)
{
	float		side;
		
	side = V_CalcRoll (cl_entities[cl.viewentity].angles, cl.velocity);
	r_refdef.viewangles[ROLL] += side;

	if (v_dmg_time > 0)
	{
		if (v_kicktime.value)
		{
			r_refdef.viewangles[ROLL] += v_dmg_time/v_kicktime.value*v_dmg_roll;
			r_refdef.viewangles[PITCH] += v_dmg_time/v_kicktime.value*v_dmg_pitch;
		}
		v_dmg_time -= host_frametime;
	}

	if (cl.stats[STAT_HEALTH] <= 0)
	{
		r_refdef.viewangles[ROLL] = 80;	// dead view angle
		return;
	}

}

/*
==================
V_CalcIntermissionRefdef

==================
*/
void V_CalcIntermissionRefdef (void)
{
	entity_t	*ent, *view;
	float		old;

// ent is the player model (visible when out of body)
	ent = &cl_entities[cl.viewentity];
// view is the weapon model (only visible from inside body)
	view = &cl.viewent;

	VectorCopy (ent->origin, r_refdef.vieworg);
	VectorCopy (ent->angles, r_refdef.viewangles);
	view->model = NULL;

// always idle in intermission
	old = v_idlescale.value;
	Cvar_SetValue ("v_idlescale", 1);
	V_AddIdle ();
	Cvar_SetValue ("v_idlescale", old);
}

/*
==================
V_CalcRefdef
==================
*/
void V_CalcRefdef (void)
{
	entity_t	*ent, *view;
	int			i;
	vec3_t		forward, right, up;
	vec3_t		angles;
	float		bob;
	static float oldz = 0;
	static vec3_t punchangle = {0,0,0}; //johnfitz -- lerped kick
	float delta; //johnfitz -- lerped kick

	V_DriftPitch ();

// ent is the player model (visible when out of body)
	ent = &cl_entities[cl.viewentity];
// view is the weapon model (only visible from inside body)
	view = &cl.viewent;

// transform the view offset by the model's matrix to get the offset from
// model origin for the view
	// JPG - viewangles -> lerpangles
	ent->angles[YAW] = cl.lerpangles[YAW];	// the model should face the view dir
	ent->angles[PITCH] = -cl.lerpangles[PITCH];	// the model should face the view dir

	bob = V_CalcBob ();

// refresh position
	VectorCopy (ent->origin, r_refdef.vieworg);
	r_refdef.vieworg[2] += cl.viewheight + bob;

// never let it sit exactly on a node line, because a water plane can
// dissapear when viewed with the eye exactly on it.
// the server protocol only specifies to 1/16 pixel, so add 1/32 in each axis
	r_refdef.vieworg[0] += 1.0/32;
	r_refdef.vieworg[1] += 1.0/32;
	r_refdef.vieworg[2] += 1.0/32;

	VectorCopy (cl.lerpangles, r_refdef.viewangles); // JPG - viewangles -> lerpangles

	V_CalcViewRoll ();
	V_AddIdle ();

// offsets
	angles[PITCH] = -ent->angles[PITCH];	// because entity pitches are actually backward
	angles[YAW] = ent->angles[YAW];
	angles[ROLL] = ent->angles[ROLL];

	AngleVectors (angles, forward, right, up);

	for (i=0 ; i<3 ; i++)
		r_refdef.vieworg[i] += scr_ofsx.value*forward[i]
			+ scr_ofsy.value*right[i]
			+ scr_ofsz.value*up[i];

	V_BoundOffsets ();

// set up gun position
	VectorCopy (cl.lerpangles, view->angles); // JPG - viewangles -> lerpangles

	CalcGunAngle ();

	VectorCopy (ent->origin, view->origin);
	view->origin[2] += cl.viewheight;

	for (i=0 ; i<3 ; i++)
	{
		view->origin[i] += forward[i]*bob*0.4;
//		view->origin[i] += right[i]*bob*0.4;
//		view->origin[i] += up[i]*bob*0.8;
	}
	view->origin[2] += bob;

// fudge position around to keep amount of weapon visible
// roughly equal with different FOV
	if (scr_weaponsize.value == 100) // scr_viewsize
		view->origin[2] += 2;
	else if (scr_weaponsize.value == 90) // scr_viewsize
		view->origin[2] += 1;
	else if (scr_weaponsize.value == 80) // scr_viewsize
		view->origin[2] += 0.5;
	else if (scr_weaponsize.value == 70) // scr_viewsize
		view->origin[2] += 0.25;
	else if (scr_weaponsize.value == 60) // scr_viewsize
		view->origin[2] += 0;

	view->model = cl.model_precache[cl.stats[STAT_WEAPON]];
	view->frame = cl.stats[STAT_WEAPONFRAME];
	view->colormap = vid.colormap;
	view->scale = ENTSCALE_DEFAULT;

// set up the refresh position
	if (v_gunkick.value == 1) //original quake kick
		VectorAdd (r_refdef.viewangles, cl.punchangle, r_refdef.viewangles);
	else if (v_gunkick.value == 2) //johnfitz -- lerped kick
	{
		for (i=0; i<3; i++)
			if (punchangle[i] != v_punchangles[0][i])
			{
				// speed determined by how far we need to lerp in 1/10th of a second
				delta = (v_punchangles[0][i]-v_punchangles[1][i]) * host_frametime * 10;

				if (delta > 0)
					punchangle[i] = min(punchangle[i]+delta, v_punchangles[0][i]);
				else if (delta < 0)
					punchangle[i] = max(punchangle[i]+delta, v_punchangles[0][i]);
			}

		VectorAdd (r_refdef.viewangles, punchangle, r_refdef.viewangles);
	}

// smooth out stair step ups
	if (!cl.noclip_anglehack && cl.onground && ent->origin[2] - oldz > 0) //johnfitz -- added exception for noclip
	//FIXME: noclip_anglehack is set on the server, so in a nonlocal game this won't work.
	{
		float steptime;

		steptime = cl.time - cl.oldtime;
		if (steptime < 0)
			//FIXME I_Error ("steptime < 0");
			steptime = 0;

		oldz += steptime * 80;
		if (oldz > ent->origin[2])
			oldz = ent->origin[2];
		if (ent->origin[2] - oldz > 12)
			oldz = ent->origin[2] - 12;
		r_refdef.vieworg[2] += oldz - ent->origin[2];
		view->origin[2] += oldz - ent->origin[2];
	}
	else
		oldz = ent->origin[2];

	if (chase_active.value)
		Chase_Update ();
}

/*
==================
V_RestoreAngles

Resets the viewentity angles to the last values received from the server
(undoing the manual adjustments performed by V_CalcRefdef)
==================
*/
void V_RestoreAngles (void)
{
	if (cls.demoplayback)
	{
		// Fix camera view angles (better way to do it?)
		entity_t *ent = &cl_entities[cl.viewentity];
		VectorCopy (ent->msg_angles[0], ent->angles);
	}
}

/*
==================
V_RenderView

The player's clipping box goes from (-16 -16 -24) to (16 16 32) from
the entity origin, so any view position inside that will be valid
==================
*/
void V_RenderView (void)
{
	if (con_forcedup)
		return;

// don't allow cheats in multiplayer
	if (cl.maxclients > 1)
	{
		Cvar_Set ("scr_ofsx", "0");
		Cvar_Set ("scr_ofsy", "0");
		Cvar_Set ("scr_ofsz", "0");
	}

// intermission / finale rendering
	if (cl.intermission)
	{
		V_CalcIntermissionRefdef ();
	}
	else
	{
		V_CalcRefdef ();
	}

	R_RenderView ();
}

/*
==============================================================================

	INIT

==============================================================================
*/

/*
=============
V_Init
=============
*/
void V_Init (void)
{
	Cmd_AddCommand ("v_cshift", V_cshift_f);
	Cmd_AddCommand ("bf", V_BonusFlash_f);
	Cmd_AddCommand ("centerview", V_StartPitchDrift);

	Cvar_RegisterVariable (&v_centermove);
	Cvar_RegisterVariable (&v_centerspeed);

	Cvar_RegisterVariable (&v_iyaw_cycle);
	Cvar_RegisterVariable (&v_iroll_cycle);
	Cvar_RegisterVariable (&v_ipitch_cycle);
	Cvar_RegisterVariable (&v_iyaw_level);
	Cvar_RegisterVariable (&v_iroll_level);
	Cvar_RegisterVariable (&v_ipitch_level);

	Cvar_RegisterVariable (&v_idlescale);
	Cvar_RegisterVariable (&crosshair);
	Cvar_RegisterVariable (&cl_crossx);
	Cvar_RegisterVariable (&cl_crossy);
	Cvar_RegisterVariable (&gl_cshiftpercent);
	Cvar_RegisterVariable (&v_contentblend);

	Cvar_RegisterVariable (&scr_ofsx);
	Cvar_RegisterVariable (&scr_ofsy);
	Cvar_RegisterVariable (&scr_ofsz);
	Cvar_RegisterVariable (&cl_rollspeed);
	Cvar_RegisterVariable (&cl_rollangle);
	Cvar_RegisterVariable (&cl_bob);
	Cvar_RegisterVariable (&cl_bobcycle);
	Cvar_RegisterVariable (&cl_bobup);

	Cvar_RegisterVariable (&v_kicktime);
	Cvar_RegisterVariable (&v_kickroll);
	Cvar_RegisterVariable (&v_kickpitch);
	Cvar_RegisterVariable (&v_gunkick); //johnfitz
	
	Cvar_RegisterVariableCallback (&v_gamma, V_UpdateGamma);
	Cvar_RegisterVariableCallback (&v_contrast, V_UpdateGamma);
}


