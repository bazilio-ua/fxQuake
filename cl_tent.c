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
// cl_tent.c -- client side temporary entities

#include "quakedef.h"

int			num_temp_entities;
entity_t	cl_temp_entities[MAX_TEMP_ENTITIES];
beam_t		cl_beams[MAX_BEAMS];

sfx_t			*cl_sfx_wizhit;
sfx_t			*cl_sfx_knighthit;
sfx_t			*cl_sfx_tink1;
sfx_t			*cl_sfx_ric1;
sfx_t			*cl_sfx_ric2;
sfx_t			*cl_sfx_ric3;
sfx_t			*cl_sfx_r_exp3;

/*
=================
CL_InitTEnts
=================
*/
void CL_InitTEnts (void)
{
	cl_sfx_wizhit = S_PrecacheSound ("wizard/hit.wav");
	cl_sfx_knighthit = S_PrecacheSound ("hknight/hit.wav");
	cl_sfx_tink1 = S_PrecacheSound ("weapons/tink1.wav");
	cl_sfx_ric1 = S_PrecacheSound ("weapons/ric1.wav");
	cl_sfx_ric2 = S_PrecacheSound ("weapons/ric2.wav");
	cl_sfx_ric3 = S_PrecacheSound ("weapons/ric3.wav");
	cl_sfx_r_exp3 = S_PrecacheSound ("weapons/r_exp3.wav");
}

/*
=================
CL_ParseBeam
=================
*/
void CL_ParseBeam (model_t *m, int ent, vec3_t start, vec3_t end)
{
	beam_t	*b;
	int		i;

// override any beam with the same entity
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
		if (b->entity == ent)
		{
			b->entity = ent;
			b->model = m;
			b->endtime = cl.time + 0.2;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			return;
		}

// find a free beam
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
		{
			b->entity = ent;
			b->model = m;
			b->endtime = cl.time + 0.2;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			return;
		}
	}
	Con_Printf ("beam list overflow (max = %d)\n", MAX_BEAMS);	
}

/*
=================
CL_ParseTEnt
=================
*/
void CL_ParseTEnt (void)
{
	int		type;
	vec3_t	pos;
	vec3_t	color;
	dlight_t	*dl;
	int		rnd;
	int		colorStart, colorLength;
	int		ent;
	vec3_t	start, end;
	char	*name;
	static float	lastmsg = 0;
	
	type = MSG_ReadByte (net_message);
	switch (type)
	{
	case TE_WIZSPIKE:			// spike hitting wall
		pos[0] = MSG_ReadCoord (net_message, cl.protocolflags);
		pos[1] = MSG_ReadCoord (net_message, cl.protocolflags);
		pos[2] = MSG_ReadCoord (net_message, cl.protocolflags);
		
		if (cl_extradlight.value)
		{
			dl = CL_AllocDlight (0);
			VectorCopy (pos, dl->origin);
			dl->radius = 250;
			dl->die = cl.time + 0.5;
			dl->decay = 300;
			
			CL_ColorDlightPaletteLength (dl, DL_COLOR_W_SPIKE);
		}
		
		R_RunParticleEffect (pos, vec3_origin, 20, 30);
		S_StartSound (-1, 0, cl_sfx_wizhit, pos, 1, 1);
		break;
		
	case TE_KNIGHTSPIKE:			// spike hitting wall
		pos[0] = MSG_ReadCoord (net_message, cl.protocolflags);
		pos[1] = MSG_ReadCoord (net_message, cl.protocolflags);
		pos[2] = MSG_ReadCoord (net_message, cl.protocolflags);
		
		if (cl_extradlight.value)
		{
			dl = CL_AllocDlight (0);
			VectorCopy (pos, dl->origin);
			dl->radius = 250;
			dl->die = cl.time + 0.5;
			dl->decay = 300;
			
			CL_ColorDlightPaletteLength (dl, DL_COLOR_K_SPIKE);
		}
		
		R_RunParticleEffect (pos, vec3_origin, 226, 20);
		S_StartSound (-1, 0, cl_sfx_knighthit, pos, 1, 1);
		break;
		
	case TE_SPIKE:			// spike hitting wall
		pos[0] = MSG_ReadCoord (net_message, cl.protocolflags);
		pos[1] = MSG_ReadCoord (net_message, cl.protocolflags);
		pos[2] = MSG_ReadCoord (net_message, cl.protocolflags);
		
		R_RunParticleEffect (pos, vec3_origin, 0, 10);
		if ( rand() % 5 )
			S_StartSound (-1, 0, cl_sfx_tink1, pos, 1, 1);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound (-1, 0, cl_sfx_ric1, pos, 1, 1);
			else if (rnd == 2)
				S_StartSound (-1, 0, cl_sfx_ric2, pos, 1, 1);
			else
				S_StartSound (-1, 0, cl_sfx_ric3, pos, 1, 1);
		}
		break;
		
	case TE_SUPERSPIKE:			// super spike hitting wall
		pos[0] = MSG_ReadCoord (net_message, cl.protocolflags);
		pos[1] = MSG_ReadCoord (net_message, cl.protocolflags);
		pos[2] = MSG_ReadCoord (net_message, cl.protocolflags);
		
		R_RunParticleEffect (pos, vec3_origin, 0, 20);
		if ( rand() % 5 )
			S_StartSound (-1, 0, cl_sfx_tink1, pos, 1, 1);
		else
		{
			rnd = rand() & 3;
			if (rnd == 1)
				S_StartSound (-1, 0, cl_sfx_ric1, pos, 1, 1);
			else if (rnd == 2)
				S_StartSound (-1, 0, cl_sfx_ric2, pos, 1, 1);
			else
				S_StartSound (-1, 0, cl_sfx_ric3, pos, 1, 1);
		}
		break;
		
	case TE_GUNSHOT:			// bullet hitting wall
		pos[0] = MSG_ReadCoord (net_message, cl.protocolflags);
		pos[1] = MSG_ReadCoord (net_message, cl.protocolflags);
		pos[2] = MSG_ReadCoord (net_message, cl.protocolflags);
		
		R_RunParticleEffect (pos, vec3_origin, 0, 20);
		break;
		
	case TE_EXPLOSION:			// rocket explosion
		pos[0] = MSG_ReadCoord (net_message, cl.protocolflags);
		pos[1] = MSG_ReadCoord (net_message, cl.protocolflags);
		pos[2] = MSG_ReadCoord (net_message, cl.protocolflags);
		
		dl = CL_AllocDlight (0);
		VectorCopy (pos, dl->origin);
		dl->radius = 350;
		dl->die = cl.time + 0.5;
		dl->decay = 300;
		
		CL_ColorDlightPaletteLength (dl, DL_COLOR_ROCKET);
		
		R_ParticleExplosion (pos);
		S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;
		
	case TE_TAREXPLOSION:			// tarbaby explosion
		pos[0] = MSG_ReadCoord (net_message, cl.protocolflags);
		pos[1] = MSG_ReadCoord (net_message, cl.protocolflags);
		pos[2] = MSG_ReadCoord (net_message, cl.protocolflags);
		
		if (cl_extradlight.value)
		{
			dl = CL_AllocDlight (0);
			VectorCopy (pos, dl->origin);
			dl->radius = 350;
			dl->die = cl.time + 0.5;
			dl->decay = 300;
			
			CL_ColorDlightPaletteLength (dl, DL_COLOR_T_EXPLOSION);
		}
		
		R_BlobExplosion (pos);
		S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;
		
	case TE_LIGHTNING1:				// lightning bolts
		ent = MSG_ReadShort (net_message);
		
		start[0] = MSG_ReadCoord (net_message, cl.protocolflags);
		start[1] = MSG_ReadCoord (net_message, cl.protocolflags);
		start[2] = MSG_ReadCoord (net_message, cl.protocolflags);
		
		end[0] = MSG_ReadCoord (net_message, cl.protocolflags);
		end[1] = MSG_ReadCoord (net_message, cl.protocolflags);
		end[2] = MSG_ReadCoord (net_message, cl.protocolflags);
		
		if (cl_extradlight.value)
		{
			dl = CL_AllocDlight (0);
			VectorCopy (start, dl->origin);
			dl->radius = 250;
			dl->die = cl.time + 0.1;
			dl->decay = 300;
			
			CL_ColorDlightPaletteLength (dl, DL_COLOR_LIGHTNING);
			
			dl = CL_AllocDlight (0);
			VectorCopy (end, dl->origin);
			dl->radius = 250;
			dl->die = cl.time + 0.1;
			dl->decay = 300;
			
			CL_ColorDlightPaletteLength (dl, DL_COLOR_LIGHTNING);
		}
		
		CL_ParseBeam (Mod_ForName("progs/bolt.mdl", true), ent, start, end);
		break;
		
	case TE_LIGHTNING2:				// lightning bolts
		ent = MSG_ReadShort (net_message);
		
		start[0] = MSG_ReadCoord (net_message, cl.protocolflags);
		start[1] = MSG_ReadCoord (net_message, cl.protocolflags);
		start[2] = MSG_ReadCoord (net_message, cl.protocolflags);
		
		end[0] = MSG_ReadCoord (net_message, cl.protocolflags);
		end[1] = MSG_ReadCoord (net_message, cl.protocolflags);
		end[2] = MSG_ReadCoord (net_message, cl.protocolflags);
		
		if (cl_extradlight.value)
		{
			dl = CL_AllocDlight (0);
			VectorCopy (start, dl->origin);
			dl->radius = 250;
			dl->die = cl.time + 0.1;
			dl->decay = 300;
			
			CL_ColorDlightPaletteLength (dl, DL_COLOR_LIGHTNING);
			
			dl = CL_AllocDlight (0);
			VectorCopy (end, dl->origin);
			dl->radius = 250;
			dl->die = cl.time + 0.1;
			dl->decay = 300;
			
			CL_ColorDlightPaletteLength (dl, DL_COLOR_LIGHTNING);
		}
		
		CL_ParseBeam (Mod_ForName("progs/bolt2.mdl", true), ent, start, end);
		break;
		
	case TE_LIGHTNING3:				// lightning bolts
		ent = MSG_ReadShort (net_message);
		
		start[0] = MSG_ReadCoord (net_message, cl.protocolflags);
		start[1] = MSG_ReadCoord (net_message, cl.protocolflags);
		start[2] = MSG_ReadCoord (net_message, cl.protocolflags);
		
		end[0] = MSG_ReadCoord (net_message, cl.protocolflags);
		end[1] = MSG_ReadCoord (net_message, cl.protocolflags);
		end[2] = MSG_ReadCoord (net_message, cl.protocolflags);
		
		if (cl_extradlight.value)
		{
			dl = CL_AllocDlight (0);
			VectorCopy (start, dl->origin);
			dl->radius = 250;
			dl->die = cl.time + 0.1;
			dl->decay = 300;
			
			CL_ColorDlightPaletteLength (dl, DL_COLOR_LIGHTNING);
			
			dl = CL_AllocDlight (0);
			VectorCopy (end, dl->origin);
			dl->radius = 250;
			dl->die = cl.time + 0.1;
			dl->decay = 300;
			
			CL_ColorDlightPaletteLength (dl, DL_COLOR_LIGHTNING);
		}
		
		CL_ParseBeam (Mod_ForName("progs/bolt3.mdl", true), ent, start, end);
		break;
		
// Nehahra		
	case TE_LIGHTNING4:				// lightning bolts
		// need to do it this way for correct parsing order
		name = MSG_ReadString (net_message);
		
		ent = MSG_ReadShort (net_message);
		
		start[0] = MSG_ReadCoord (net_message, cl.protocolflags);
		start[1] = MSG_ReadCoord (net_message, cl.protocolflags);
		start[2] = MSG_ReadCoord (net_message, cl.protocolflags);
		
		end[0] = MSG_ReadCoord (net_message, cl.protocolflags);
		end[1] = MSG_ReadCoord (net_message, cl.protocolflags);
		end[2] = MSG_ReadCoord (net_message, cl.protocolflags);
		
		if (cl_extradlight.value)
		{
			dl = CL_AllocDlight (0);
			VectorCopy (start, dl->origin);
			dl->radius = 250;
			dl->die = cl.time + 0.1;
			dl->decay = 300;
			
			CL_ColorDlightPaletteLength (dl, DL_COLOR_LIGHTNING);
			
			dl = CL_AllocDlight (0);
			VectorCopy (end, dl->origin);
			dl->radius = 250;
			dl->die = cl.time + 0.1;
			dl->decay = 300;
			
			CL_ColorDlightPaletteLength (dl, DL_COLOR_LIGHTNING);
		}
		
		CL_ParseBeam (Mod_ForName(name, true), ent, start, end);
		break;
		
// PGM 01/21/97 
	case TE_BEAM:				// grappling hook beam
		ent = MSG_ReadShort (net_message);
		
		start[0] = MSG_ReadCoord (net_message, cl.protocolflags);
		start[1] = MSG_ReadCoord (net_message, cl.protocolflags);
		start[2] = MSG_ReadCoord (net_message, cl.protocolflags);
		
		end[0] = MSG_ReadCoord (net_message, cl.protocolflags);
		end[1] = MSG_ReadCoord (net_message, cl.protocolflags);
		end[2] = MSG_ReadCoord (net_message, cl.protocolflags);
		
		if (cl_extradlight.value)
		{
			dl = CL_AllocDlight (0);
			VectorCopy (start, dl->origin);
			dl->radius = 30;
			dl->die = cl.time + 0.1;
			dl->decay = 300;
			
			CL_ColorDlightPaletteLength (dl, DL_COLOR_BEAM);
			
			dl = CL_AllocDlight (0);
			VectorCopy (end, dl->origin);
			dl->radius = 30;
			dl->die = cl.time + 0.1;
			dl->decay = 300;
			
			CL_ColorDlightPaletteLength (dl, DL_COLOR_BEAM);
		}
		
		CL_ParseBeam (Mod_ForName("progs/beam.mdl", true), ent, start, end);
		break;
// PGM 01/21/97
		
	case TE_LAVASPLASH:			// Chthon
		pos[0] = MSG_ReadCoord (net_message, cl.protocolflags);
		pos[1] = MSG_ReadCoord (net_message, cl.protocolflags);
		pos[2] = MSG_ReadCoord (net_message, cl.protocolflags);
		
		if (cl_extradlight.value)
		{
			dl = CL_AllocDlight (0);
			VectorCopy (pos, dl->origin);
			dl->radius = 450;
			dl->die = cl.time + 3.5;
			dl->decay = 300;
			
			CL_ColorDlightPaletteLength (dl, DL_COLOR_LAVA);
		}
		
		R_LavaSplash (pos);
		break;
		
	case TE_TELEPORT:			// all teleport
		pos[0] = MSG_ReadCoord (net_message, cl.protocolflags);
		pos[1] = MSG_ReadCoord (net_message, cl.protocolflags);
		pos[2] = MSG_ReadCoord (net_message, cl.protocolflags);
		
		if (cl_extradlight.value)
		{
			dl = CL_AllocDlight (0);
			VectorCopy (pos, dl->origin);
			dl->radius = 250;
			dl->die = cl.time + 0.2;
			dl->decay = 300;
			
			CL_ColorDlightPalette (dl, DL_COLOR_254); // white
		}
		
		R_TeleportSplash (pos);
		break;
		
	case TE_EXPLOSION2:				// color mapped explosion
		pos[0] = MSG_ReadCoord (net_message, cl.protocolflags);
		pos[1] = MSG_ReadCoord (net_message, cl.protocolflags);
		pos[2] = MSG_ReadCoord (net_message, cl.protocolflags);
		
		colorStart = MSG_ReadByte (net_message);
		colorLength = MSG_ReadByte (net_message);
		
		dl = CL_AllocDlight (0);
		VectorCopy (pos, dl->origin);
		dl->radius = 350;
		dl->die = cl.time + 0.5;
		dl->decay = 300;
		
		CL_ColorDlightPaletteLength (dl, colorStart, colorLength);
		
		R_ParticleExplosion2 (pos, colorStart, colorLength);
		S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;
		
// Nehahra
	case TE_SMOKE:
		// Just parse msg
		// falls through to explosion 3
		MSG_ReadCoord (net_message, cl.protocolflags);
		MSG_ReadCoord (net_message, cl.protocolflags);
		MSG_ReadCoord (net_message, cl.protocolflags);
		
		MSG_ReadByte (net_message);
		
	case TE_EXPLOSION3:				// rocket explosion (colored)
		pos[0] = MSG_ReadCoord (net_message, cl.protocolflags);
		pos[1] = MSG_ReadCoord (net_message, cl.protocolflags);
		pos[2] = MSG_ReadCoord (net_message, cl.protocolflags);
		
		dl = CL_AllocDlight (0);
		VectorCopy (pos, dl->origin);
		dl->radius = 350;
		dl->die = cl.time + 0.5;
		dl->decay = 300;
		
		color[0] = MSG_ReadCoord(net_message, cl.protocolflags);
		color[1] = MSG_ReadCoord(net_message, cl.protocolflags);
		color[2] = MSG_ReadCoord(net_message, cl.protocolflags);
		CL_ColorDlight (dl, color[0], color[1], color[2]);
		
		S_StartSound (-1, 0, cl_sfx_r_exp3, pos, 1, 1);
		break;
		
	case TE_NEW1:
		break;
		
	case TE_NEW2:
		break;
		
	default:
		// no need to crash the engine but we will crash the map, as it means we have
		// a malformed packet
//		Host_Error ("CL_ParseTEnt: bad type %d", type); // Sys_Error
		if (IsTimeout(&lastmsg, 2))
		{
			Con_Warning ("CL_ParseTEnt: bad type %d\n", type);
		}
		
		// Blind parsing ...
		// note - this might crash the server at some stage if more data is expected
		MSG_ReadCoord (net_message, cl.protocolflags);
		MSG_ReadCoord (net_message, cl.protocolflags);
		MSG_ReadCoord (net_message, cl.protocolflags);
		break;
	}
}


/*
=================
CL_NewTempEntity
=================
*/
entity_t *CL_NewTempEntity (void)
{
	entity_t	*ent;
	static float	lastmsg1 = 0, lastmsg2 = 0;

	if (cl_numvisedicts == MAX_VISEDICTS)
	{
		if (IsTimeout (&lastmsg1, 2))
			Con_DWarning ("CL_NewTempEntity: too many visedicts (max = %d)\n", MAX_VISEDICTS);

		return NULL;
	}
	if (num_temp_entities == MAX_TEMP_ENTITIES)
	{
		if (IsTimeout (&lastmsg2, 2))
			Con_DWarning ("CL_NewTempEntity: too many temp_entities (max = %d)\n", MAX_TEMP_ENTITIES);

		return NULL;
	}
	ent = &cl_temp_entities[num_temp_entities];
	memset (ent, 0, sizeof(*ent));
	num_temp_entities++;
	cl_visedicts[cl_numvisedicts] = ent;
	cl_numvisedicts++;

	ent->scale = ENTSCALE_DEFAULT;
	ent->colormap = vid.colormap;
	return ent;
}


/*
=================
CL_UpdateTEnts
=================
*/
void CL_UpdateTEnts (void)
{
	int	    i, j;
	beam_t		*b;
	vec3_t		dist, org;
	float		d;
	entity_t	*ent;
	float		yaw, pitch;
	float		forward;

	num_temp_entities = 0;

// update lightning
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
			continue;

	// if coming from the player, update the start position
		if (b->entity == cl.viewentity)
		{
			VectorCopy (cl_entities[cl.viewentity].origin, b->start);
		}

	// calculate pitch and yaw
		VectorSubtract (b->end, b->start, dist);

		if (dist[1] == 0 && dist[0] == 0)
		{
			yaw = 0;
			if (dist[2] > 0)
				pitch = 90;
			else
				pitch = 270;
		}
		else
		{
			yaw = (int) (atan2(dist[1], dist[0]) * 180 / M_PI);
			if (yaw < 0)
				yaw += 360;
	
			forward = sqrt(dist[0]*dist[0] + dist[1]*dist[1]);
			pitch = (int) (atan2(dist[2], forward) * 180 / M_PI);
			if (pitch < 0)
				pitch += 360;
		}

	// add new entities for the lightning
		VectorCopy (b->start, org);
		d = VectorNormalize(dist);
		while (d > 0)
		{
			ent = CL_NewTempEntity ();
			if (!ent)
				return;
			VectorCopy (org, ent->origin);
			ent->model = b->model;
			ent->angles[0] = pitch;
			ent->angles[1] = yaw;
			ent->angles[2] = rand()%360;

			for (j=0 ; j<3 ; j++)
				org[j] += dist[j]*30;
			d -= 30;
		}
	}
	
}


