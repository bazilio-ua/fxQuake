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
// cl_main.c  -- client main loop

#include "quakedef.h"

// we need to declare some mouse variables here, because the menu system
// references them even when on a unix system.

// these two are not intended to be set directly
cvar_t	cl_name = {"cl_name", "player", true};
cvar_t	cl_color = {"cl_color", "0", true};

cvar_t	cl_shownet = {"cl_shownet","0"};	// can be 0, 1, or 2
cvar_t	cl_nolerp = {"cl_nolerp","0"};

cvar_t	cl_coloredlight = {"cl_coloredlight","0", true};
cvar_t	cl_extradlight = {"cl_extradlight","0", true};

cvar_t	lookspring = {"lookspring","0", true};
cvar_t	lookstrafe = {"lookstrafe","0", true};
cvar_t	sensitivity = {"sensitivity","3", true};

cvar_t	m_pitch = {"m_pitch","0.022", true};
cvar_t	m_yaw = {"m_yaw","0.022", true};
cvar_t	m_forward = {"m_forward","1", true};
cvar_t	m_side = {"m_side","0.8", true};


client_static_t	cls;
client_state_t	cl;
// FIXME: put these on hunk?
efrag_t			cl_efrags[MAX_EFRAGS];
entity_t		cl_entities[MAX_EDICTS];
entity_t		cl_static_entities[MAX_STATIC_ENTITIES];
lightstyle_t	cl_lightstyle[MAX_LIGHTSTYLES];
dlight_t		cl_dlights[MAX_DLIGHTS];

int				cl_numvisedicts;
entity_t		*cl_visedicts[MAX_VISEDICTS];

/*
=====================
CL_ClearState

=====================
*/
void CL_ClearState (void)
{
	int			i;

	if (!sv.active)
		Host_ClearMemory ();

// wipe the entire cl structure
	memset (&cl, 0, sizeof(cl));

	// If disconnect was missing, stop sounds here
	if (cls.state == ca_connected)
		S_StopAllSounds (true);

	SZ_Clear (&cls.message);

// clear other arrays	
	memset (cl_efrags, 0, sizeof(cl_efrags));
	memset (cl_entities, 0, sizeof(cl_entities));
	memset (cl_dlights, 0, sizeof(cl_dlights));
	memset (cl_lightstyle, 0, sizeof(cl_lightstyle));
	memset (cl_temp_entities, 0, sizeof(cl_temp_entities));
	memset (cl_beams, 0, sizeof(cl_beams));

//
// allocate the efrags and chain together into a free list
//
	cl.free_efrags = cl_efrags;
	for (i=0 ; i<MAX_EFRAGS-1 ; i++)
		cl.free_efrags[i].entnext = &cl.free_efrags[i+1];
	cl.free_efrags[i].entnext = NULL;
}

/*
=====================
CL_Disconnect

Sends a disconnect message to the server
This is also called on Host_Error, so it shouldn't cause any errors
=====================
*/
void CL_Disconnect (void)
{
// stop sounds (especially looping!)
	S_StopAllSounds (true);

	CDAudio_Stop(); // Stop CD music

// if running a local server, shut it down
	cl.worldmodel = NULL; // This makes sure ambient sounds remain silent

// stop all intermissions (the intermission screen prints the map name so
// this is important for preventing a crash)
	cl.intermission = 0; // Baker: So critical.  SCR_UpdateScreen uses this.

// remove all center prints
	con_lastcenterstring[0] = 0;
	scr_centertime_off = 0;

	if (cls.demoplayback)
		CL_StopPlayback ();
	else if (cls.state == ca_connected)
	{
		if (cls.demorecording)
			CL_Stop_f ();

		Con_DPrintf ("Sending clc_disconnect\n");
		SZ_Clear (&cls.message);
		MSG_WriteByte (&cls.message, clc_disconnect);
		NET_SendUnreliableMessage (cls.netcon, &cls.message);
		SZ_Clear (&cls.message);
		NET_Close (cls.netcon);

		cls.state = ca_disconnected;
		if (sv.active)
			Host_ShutdownServer(false);
	}

	cls.demoplayback = cls.timedemo = false;
	cls.signon = 0;
}

void CL_Disconnect_f (void)
{
	CL_Disconnect ();
//	if (sv.active)
//		Host_ShutdownServer (false);
}


/*
=====================
CL_EstablishConnection

Host should be either "local" or a net address to be passed on
=====================
*/
void CL_EstablishConnection (char *host)
{
	if (cls.state == ca_dedicated)
		return;

	if (cls.demoplayback)
		return;

	CL_Disconnect ();

	cls.netcon = NET_Connect (host);
	if (!cls.netcon)
		Host_Error ("CL_EstablishConnection: connect failed");

	// JPG - ProQuake dprint
	if (cls.netcon->mod == MOD_PROQUAKE)
		Con_DPrintf ("Connected to ProQuake server %s\n", host);
	else
		Con_DPrintf ("Connected to server %s\n", host);

	cls.demonum = -1;			// not in the demo loop now
	cls.state = ca_connected;
	cls.signon = 0;				// need all the signon messages before playing

	MSG_WriteByte (&cls.message, clc_nop); // ProQuake NAT fix
}

/*
=====================
CL_SignonReply

An svc_signonnum has been received, perform a client side setup
=====================
*/
void CL_SignonReply (void)
{
	char 	str[100 + MAX_MAPSTRING];

	Con_DPrintf ("CL_SignonReply: %i\n", cls.signon);

	switch (cls.signon)
	{
	case 1:
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, "prespawn");
		break;
		
	case 2:		
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, va("name \"%s\"\n", cl_name.string));
	
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, va("color %i %i\n", ((int)cl_color.value)>>4, ((int)cl_color.value)&15));
	
		MSG_WriteByte (&cls.message, clc_stringcmd);
		sprintf (str, "spawn %s", cls.spawnparms);
		MSG_WriteString (&cls.message, str);
		break;
		
	case 3:	
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, "begin");
		Cache_Report ();		// print remaining memory
		break;
		
	case 4:
		SCR_EndLoadingPlaque ();		// allow normal screen updates
		break;
	}
}

/*
==============
CL_PrintEntities_f
==============
*/
void CL_PrintEntities_f (void)
{
	entity_t	*ent;
	int			i;
	
	for (i=0,ent=cl_entities ; i<cl.num_entities ; i++,ent++)
	{
		Con_Printf ("%3i:",i);
		if (!ent->model)
		{
			Con_Printf ("EMPTY\n");
			continue;
		}
		Con_Printf ("%s:%2i  (%5.1f,%5.1f,%5.1f) [%5.1f %5.1f %5.1f]\n"
		,ent->model->name,ent->frame, ent->origin[0], ent->origin[1], ent->origin[2], ent->angles[0], ent->angles[1], ent->angles[2]);
	}
}


/*
===============
CL_AllocDlight

===============
*/
dlight_t *CL_AllocDlight (int key)
{
	int		i;
	dlight_t	*dl;

// first look for an exact key match
	if (key)
	{
		dl = cl_dlights;
		for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
		{
			if (dl->key == key)
			{
				memset (dl, 0, sizeof(*dl));
				dl->key = key;
				dl->color[0] = dl->color[1] = dl->color[2] = 1.0; // lit support via lordhavoc
				return dl;
			}
		}
	}

// then look for anything else
	dl = cl_dlights;
	for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
	{
		if (dl->die < cl.time)
		{
			memset (dl, 0, sizeof(*dl));
			dl->key = key;
			dl->color[0] = dl->color[1] = dl->color[2] = 1.0; // lit support via lordhavoc
			return dl;
		}
	}

	dl = &cl_dlights[0];
	memset (dl, 0, sizeof(*dl));
	dl->key = key;
	dl->color[0] = dl->color[1] = dl->color[2] = 1.0; // lit support via lordhavoc
	return dl;
}

void CL_ColorDlight (dlight_t *dl, float r, float g, float b)
{
	// leave dlight with white value it had at allocation
	if (!cl_coloredlight.value)
		return;

	dl->color[0] = r;
	dl->color[1] = g;
	dl->color[2] = b;
}


/*
===============
CL_DecayLights

===============
*/
void CL_DecayLights (void)
{
	int			i;
	dlight_t	*dl;
	float		time;
	
	time = cl.time - cl.oldtime;

	dl = cl_dlights;
	for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
	{
		if (dl->die < cl.time || !dl->radius)
			continue;
		
		dl->radius -= time*dl->decay;
		if (dl->radius < 0)
			dl->radius = 0;
	}
}


/*
===============
CL_LerpPoint

Determines the fraction between the last two messages that the objects
should be put at.
===============
*/
float	CL_LerpPoint (void)
{
	float	f, frac;

	f = cl.mtime[0] - cl.mtime[1];

	if (!f || cls.timedemo || sv.active)
	{
		cl.time = cl.mtime[0];
		return 1;
	}

	if (f > 0.1)
	{	// dropped packet, or start of demo
		cl.mtime[1] = cl.mtime[0] - 0.1;
		f = 0.1;
	}
	frac = (cl.time - cl.mtime[1]) / f;
	//Con_Printf ("frac: %f\n",frac);
	if (frac < 0)
	{
		if (frac < -0.01)
		{
			cl.time = cl.mtime[1];
			//Con_Printf ("low frac\n");
		}
		frac = 0;
	}
	else if (frac > 1)
	{
		if (frac > 1.01)
		{
			cl.time = cl.mtime[0];
			//Con_Printf ("high frac\n");
		}
		frac = 1;
	}

	//johnfitz -- better nolerp behavior
	if (cl_nolerp.value)
		return 1;
	//johnfitz

	return frac;
}


/*
===============
CL_RelinkEntities
===============
*/
void CL_RelinkEntities (void)
{
	entity_t	*ent;
	int			i, j;
	float		frac, f, d;
	vec3_t		delta;
	float		objrotate;
	vec3_t		oldorg;
//	vec3_t		color;
	dlight_t	*dl;
	static float	lastmsg = 0;

// determine partial update time
	frac = CL_LerpPoint ();

	cl_numvisedicts = 0;

//
// interpolate player info
//
	for (i=0 ; i<3 ; i++)
		cl.velocity[i] = cl.mvelocity[1][i] + frac * (cl.mvelocity[0][i] - cl.mvelocity[1][i]);

	if (cls.demoplayback || cl.last_angle_time > host_time)
	{
	// interpolate the angles
		for (j=0 ; j<3 ; j++)
		{
			d = cl.mviewangles[0][j] - cl.mviewangles[1][j];
			if (d > 180)
				d -= 360;
			else if (d < -180)
				d += 360;
			cl.lerpangles[j] = cl.mviewangles[1][j] + frac*d;
			if (cls.demoplayback)
				cl.viewangles[j] = cl.mviewangles[1][j] + frac*d;
		}
	}
	else
		VectorCopy(cl.viewangles, cl.lerpangles);

	objrotate = anglemod(100*cl.time);

// start on the entity after the world
	for (i=1,ent=cl_entities+1 ; i<cl.num_entities ; i++,ent++)
	{
		if (!ent->model)
		{	// empty slot
			if (ent->forcelink)
				R_RemoveEfrags (ent);	// just became empty
			continue;
		}

// if the object wasn't included in the last packet, remove it
		if (ent->msgtime != cl.mtime[0])
		{
			ent->model = NULL;
			ent->lerpflags |= LERP_RESETMOVE|LERP_RESETANIM; //johnfitz -- next time this entity slot is reused, the lerp will need to be reset

			continue;
		}

		VectorCopy (ent->origin, oldorg);

		if (ent->forcelink)
		{	// the entity was not updated in the last message
			// so move to the final spot
			VectorCopy (ent->msg_origins[0], ent->origin);
			VectorCopy (ent->msg_angles[0], ent->angles);
		}
		else
		{	// if the delta is large, assume a teleport and don't lerp
			f = frac;
			for (j=0 ; j<3 ; j++)
			{
				delta[j] = ent->msg_origins[0][j] - ent->msg_origins[1][j];
				if (delta[j] > 100 || delta[j] < -100)
				{
					f = 1;		// assume a teleportation, not a motion
					ent->lerpflags |= LERP_RESETMOVE; //johnfitz -- don't lerp teleports
				}
			}

			//johnfitz -- don't cl_lerp entities that will be r_lerped
			if (ent->lerpflags & LERP_MOVESTEP)
				f = 1;
			//johnfitz

		// interpolate the origin and angles
			for (j=0 ; j<3 ; j++)
			{
				ent->origin[j] = ent->msg_origins[1][j] + f*delta[j];

				d = ent->msg_angles[0][j] - ent->msg_angles[1][j];
				if (d > 180)
					d -= 360;
				else if (d < -180)
					d += 360;
				ent->angles[j] = ent->msg_angles[1][j] + f*d;
			}
		}
		
// rotate binary objects locally
		if (ent->model->flags & EF_ROTATE)
			ent->angles[1] = objrotate;
		
		if (ent->effects & EF_BRIGHTFIELD)
			R_EntityParticles (ent);
		
		if (ent->effects & EF_MUZZLEFLASH)
		{
			vec3_t		fv, rv, uv;

			dl = CL_AllocDlight (i);
			VectorCopy (ent->origin,  dl->origin);
			dl->origin[2] += 16;
			AngleVectors (ent->angles, fv, rv, uv);
			VectorMA (dl->origin, 18, fv, dl->origin);
			dl->radius = 200 + (rand()&31);
			dl->minlight = 32;
			dl->die = cl.time + 0.1;

			if (i == cl.viewentity)
			{
				// switch the flash colour for the current weapon
				if (cl.stats[STAT_ACTIVEWEAPON] == IT_SUPER_LIGHTNING) 
					CL_ColorDlight (dl, DL_COLOR_CYAN);
				else if (cl.stats[STAT_ACTIVEWEAPON] == IT_LIGHTNING)
					CL_ColorDlight (dl, DL_COLOR_CYAN);
				//else if TODO: add more weapons
				else
					CL_ColorDlight (dl, DL_COLOR_ORANGE); //default orange
			}
			else
			{
				// some entities have different attacks resulting in a different flash colour
				if (!strcmp (ent->model->name, "progs/wizard.mdl"))
					CL_ColorDlight (dl, DL_COLOR_GREEN);
				else if (!strcmp (ent->model->name, "progs/shalrath.mdl"))
					CL_ColorDlight (dl, DL_COLOR_PURPLE);
				else if (!strcmp (ent->model->name, "progs/shambler.mdl"))
					CL_ColorDlight (dl, DL_COLOR_CYAN);
				//else if TODO: add more models
				else 
					CL_ColorDlight (dl, DL_COLOR_ORANGE); //default orange
				//dl->color[0] = 0.3; dl->color[1] = 0.2; dl->color[2] = 0.1;
			}
		}
		if (ent->effects & EF_BRIGHTLIGHT)
		{
			// uncoloured
			dl = CL_AllocDlight (i);
			VectorCopy (ent->origin,  dl->origin);
			dl->origin[2] += 16;
			dl->radius = 400 + (rand()&31);
			dl->die = cl.time + 0.001;

			CL_ColorDlight (dl, DL_COLOR_WHITE);
			//dl->color[0] = 0.1;dl->color[1] = 0.1;dl->color[2] = 0.1;
		}
		if (ent->effects & EF_DIMLIGHT)
		{			
			// powerup dynamic lights
			dl = CL_AllocDlight (i);
			VectorCopy (ent->origin,  dl->origin);
			dl->radius = 200 + (rand()&31);
			dl->die = cl.time + 0.001;
			
			if (i == cl.viewentity)
			{
				// set the appropriate colour depending on the current powerup(s)
				if ((cl.items & IT_QUAD) && (cl.items & IT_INVULNERABILITY))
					CL_ColorDlight (dl, DL_COLOR_PURPLE);
				else if (cl.items & IT_QUAD)
					CL_ColorDlight (dl, DL_COLOR_BLUE);
				else if (cl.items & IT_INVULNERABILITY)
					CL_ColorDlight (dl, DL_COLOR_RED);
				//else if TODO: add more powerups
				else
					CL_ColorDlight (dl, DL_COLOR_WHITE);
			}
			else
				CL_ColorDlight (dl, DL_COLOR_WHITE);
			//dl->color[0] = 0.1;dl->color[1] = 0.1;dl->color[2] = 0.1;
		}

// Nehahra
		if ((ent->effects & EF_RED) || (ent->effects & EF_BLUE))
		{
			dl = CL_AllocDlight (i);
			VectorCopy (ent->origin,  dl->origin);
			dl->radius = 200 + (rand()&31);
			dl->die = cl.time + 0.001;
			
			// set the appropriate colour
			if ((ent->effects & EF_RED) && (ent->effects & EF_BLUE))
				CL_ColorDlight (dl, DL_COLOR_PURPLE);
			else if (ent->effects & EF_RED)
				CL_ColorDlight (dl, DL_COLOR_RED);
			else if (ent->effects & EF_BLUE)
				CL_ColorDlight (dl, DL_COLOR_BLUE);
			//dl->color[0] = 0.7;dl->color[1] = 0.07;dl->color[2] = 0.7;
		}

/*		if (ent->effects & EF_RED) // red
		{			
			if (ent->effects & EF_BLUE) // magenta
			{
				dl = CL_AllocDlight (i);
				VectorCopy (ent->origin,  dl->origin);
				dl->radius = 200 + (rand()&31);
				dl->die = cl.time + 0.001;

				CL_ColorDlight (dl, 0.7, 0.07, 0.7);
				//dl->color[0] = 0.7;dl->color[1] = 0.07;dl->color[2] = 0.7;
			}
			else // red
			{
				dl = CL_AllocDlight (i);
				VectorCopy (ent->origin,  dl->origin);
				dl->radius = 200 + (rand()&31);
				dl->die = cl.time + 0.001;

				CL_ColorDlight (dl, 0.8, 0.05, 0.05);
				//dl->color[0] = 0.8;dl->color[1] = 0.05;dl->color[2] = 0.05;
			}
		}
		else if (ent->effects & EF_BLUE) // blue
		{
			dl = CL_AllocDlight (i);
			VectorCopy (ent->origin,  dl->origin);
			dl->radius = 200 + (rand()&31);
			dl->die = cl.time + 0.001;

			CL_ColorDlight (dl, 0.05, 0.05, 0.8);
			//dl->color[0] = 0.05;dl->color[1] = 0.05;dl->color[2] = 0.8;
		}
*/
		if (ent->model->flags & EF_GIB)
			R_RocketTrail (oldorg, ent->origin, 2);
		else if (ent->model->flags & EF_ZOMGIB)
			R_RocketTrail (oldorg, ent->origin, 4);
		else if (ent->model->flags & EF_TRACER)
		{
			// wizard trail
			if (cl_extradlight.value)
			{
				dl = CL_AllocDlight (i);
				VectorCopy (ent->origin, dl->origin);
				dl->radius = 200;
				dl->die = cl.time + 0.01;
				
				CL_ColorDlight (dl, DL_COLOR_GREEN);
			}
			
			R_RocketTrail (oldorg, ent->origin, 3);
		}
		else if (ent->model->flags & EF_TRACER2)
		{
			// knight trail
			if (cl_extradlight.value)
			{
				dl = CL_AllocDlight (i);
				VectorCopy (ent->origin, dl->origin);
				dl->radius = 200;
				dl->die = cl.time + 0.01;
				
				CL_ColorDlight (dl, DL_COLOR_ORANGE);
			}
			
			R_RocketTrail (oldorg, ent->origin, 5);
		}
		else if (ent->model->flags & EF_TRACER3)
		{
			// vore trail
			if (cl_extradlight.value)
			{
				dl = CL_AllocDlight (i);
				VectorCopy (ent->origin, dl->origin);
				dl->radius = 200;
				dl->die = cl.time + 0.01;
				
				CL_ColorDlight (dl, DL_COLOR_PURPLE);
			}
			
			R_RocketTrail (oldorg, ent->origin, 6);
		}
		else if (ent->model->flags & EF_ROCKET)
		{
			R_RocketTrail (oldorg, ent->origin, 0);
			
			dl = CL_AllocDlight (i);
			VectorCopy (ent->origin, dl->origin);
			dl->radius = 200;
			dl->die = cl.time + 0.01;

			CL_ColorDlight (dl, DL_COLOR_ORANGE);
			//dl->color[0] = 0.4; dl->color[1] = 0.2; dl->color[2] = 0.1;
		}
		else if (ent->model->flags & EF_GRENADE)
			R_RocketTrail (oldorg, ent->origin, 1);

		ent->forcelink = false;

		if (i == cl.viewentity && !chase_active.value)
			continue;

// Nehahra
// LordHavoc: enabled EF_NODRAW
		if (ent->effects & EF_NODRAW)
			continue;

		if (cl_numvisedicts < MAX_VISEDICTS)
		{
			cl_visedicts[cl_numvisedicts] = ent;
			cl_numvisedicts++;
		}
		else if (IsTimeout (&lastmsg, 2))
			Con_DPrintf ("CL_RelinkEntities: too many visedicts (max = %d)\n", MAX_VISEDICTS);
	}
}


/*
===============
CL_ReadFromServer

Read all incoming data from the server
===============
*/
int CL_ReadFromServer (void)
{
	int		ret;

	cl.oldtime = cl.time;
	cl.time += host_frametime;
	
	do
	{
		ret = CL_GetMessage ();
		if (ret == -1)
			Host_Error ("CL_ReadFromServer: lost server connection");
		if (!ret)
			break;
		
		cl.last_received_message = realtime;
		CL_ParseServerMessage ();
	} while (ret && cls.state == ca_connected);
	
	if (cl_shownet.value)
		Con_Printf ("\n");

	CL_RelinkEntities ();
	CL_UpdateTEnts ();

//
// bring the links up to date
//
	return 0;
}

/*
=================
CL_SendCmd
=================
*/
void CL_SendCmd (void)
{
	usercmd_t		cmd;

	if (cls.state != ca_connected)
		return;

	if (cls.signon == SIGNONS)
	{
	// get basic movement from keyboard
		CL_BaseMove (&cmd);
	
	// allow mice or other external controllers to add to the move
		IN_Move (&cmd);
	
	// send the unreliable message
		CL_SendMove (&cmd);
	
	}

	if (cls.demoplayback)
	{
		SZ_Clear (&cls.message);
		return;
	}
	
// send the reliable message
	if (!cls.message.cursize)
		return;		// no message at all
	
	if (!NET_CanSendMessage (cls.netcon))
	{
		Con_DPrintf ("CL_SendCmd: can't send\n");
		return;
	}

	if (NET_SendMessage (cls.netcon, &cls.message) == -1)
		Host_Error ("CL_SendCmd: lost server connection");

	SZ_Clear (&cls.message);
}

/*
=============
CL_Tracepos_f

display impact point of trace along VPN
=============
*/
void CL_Tracepos_f (void)
{
	vec3_t	v, w;

	VectorScale(vpn, 8192.0, v);
	TraceLine(r_refdef.vieworg, v, w);

	if (VectorLength(w) == 0)
		Con_Printf ("Tracepos: trace didn't hit anything\n");
	else
		Con_Printf ("Tracepos: (%i %i %i)\n", (int)w[0], (int)w[1], (int)w[2]);
}

/*
=============
CL_Viewpos_f

Display client's position and mangle
=============
*/
void CL_Viewpos_f (void)
{
	// Player position
	Con_Printf ("Viewpos: (%i %i %i) %i %i %i\n",
		(int)cl_entities[cl.viewentity].origin[0],
		(int)cl_entities[cl.viewentity].origin[1],
		(int)cl_entities[cl.viewentity].origin[2],
		(int)cl.viewangles[PITCH],
		(int)cl.viewangles[YAW],
		(int)cl.viewangles[ROLL]);
		
	if (sv_player && sv_player->v.movetype == MOVETYPE_NOCLIP && Cmd_Argc() == 4)
	{
		// Major hack ...
		sv_player->v.origin[0] = atoi (Cmd_Argv(1));
		sv_player->v.origin[1] = atoi (Cmd_Argv(2));
		sv_player->v.origin[2] = atoi (Cmd_Argv(3));
	}
}

/*
=============
CL_StaticEnts_f
=============
*/
void CL_StaticEnts_f (void)
{
	Con_Printf ("%d static entities\n", cl.num_statics);
}

/*
=================
CL_Init
=================
*/
void CL_Init (void)
{	
	SZ_Alloc (&cls.message, 8192); //1024, possibly dependant on CMDTEXTSIZE

	CL_InitInput ();
	CL_InitTEnts ();
	
//
// register our commands
//
	Cvar_RegisterVariable (&cl_name, NULL);
	Cvar_RegisterVariable (&cl_color, NULL);
	Cvar_RegisterVariable (&cl_upspeed, NULL);
	Cvar_RegisterVariable (&cl_forwardspeed, NULL);
	Cvar_RegisterVariable (&cl_backspeed, NULL);
	Cvar_RegisterVariable (&cl_sidespeed, NULL);
	Cvar_RegisterVariable (&cl_movespeedkey, NULL);
	Cvar_RegisterVariable (&cl_yawspeed, NULL);
	Cvar_RegisterVariable (&cl_pitchspeed, NULL);
	Cvar_RegisterVariable (&cl_maxpitch, NULL); // variable pitch clamping
	Cvar_RegisterVariable (&cl_minpitch, NULL); // variable pitch clamping
	Cvar_RegisterVariable (&cl_anglespeedkey, NULL);
	Cvar_RegisterVariable (&cl_shownet, NULL);
	Cvar_RegisterVariable (&cl_nolerp, NULL);

	Cvar_RegisterVariable (&cl_coloredlight, NULL);
	Cvar_RegisterVariable (&cl_extradlight, NULL);

	Cvar_RegisterVariable (&lookspring, NULL);
	Cvar_RegisterVariable (&lookstrafe, NULL);
	Cvar_RegisterVariable (&sensitivity, NULL);

	Cvar_RegisterVariable (&m_pitch, NULL);
	Cvar_RegisterVariable (&m_yaw, NULL);
	Cvar_RegisterVariable (&m_forward, NULL);
	Cvar_RegisterVariable (&m_side, NULL);

	Cmd_AddCommand ("entities", CL_PrintEntities_f);
	Cmd_AddCommand ("disconnect", CL_Disconnect_f);
	Cmd_AddCommand ("record", CL_Record_f);
	Cmd_AddCommand ("stop", CL_Stop_f);
	Cmd_AddCommand ("playdemo", CL_PlayDemo_f);
	Cmd_AddCommand ("timedemo", CL_TimeDemo_f);
	Cmd_AddCommand ("tracepos", CL_Tracepos_f); // fitz
	Cmd_AddCommand ("viewpos", CL_Viewpos_f);
	Cmd_AddCommand ("staticents", CL_StaticEnts_f);
}

