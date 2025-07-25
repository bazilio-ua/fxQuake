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
// view.h

extern	cvar_t crosshair, cl_crossx, cl_crossy;

extern float v_blend[4];

void V_Init (void);
void V_RenderView (void);
void V_RestoreAngles (void);
float V_CalcRoll (vec3_t angles, vec3_t velocity);
void V_CalcBlend (void);
void V_UpdateBlend (void);

void V_FindFullbrightColors (void);
void V_SetOriginalPalette (void);
void V_SetPalette (byte *palette);
// called at startup and after any gamma correction

void V_ShiftPalette (byte *palette);
// called after gammatable updates

extern	cvar_t		v_gamma;
extern	cvar_t		v_contrast;
