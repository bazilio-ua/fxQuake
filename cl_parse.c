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
// cl_parse.c  -- parse a message received from the server

#include "quakedef.h"

char *svc_strings[] =
{
	"svc_bad",
	"svc_nop",
	"svc_disconnect",
	"svc_updatestat",
	"svc_version",		// [long] server version
	"svc_setview",		// [short] entity number
	"svc_sound",			// <see code>
	"svc_time",			// [float] server time
	"svc_print",			// [string] null terminated string
	"svc_stufftext",		// [string] stuffed into client's console buffer
						// the string should be \n terminated
	"svc_setangle",		// [vec3] set the view angle to this absolute value
	
	"svc_serverinfo",		// [long] version
						// [string] signon string
						// [string]..[0]model cache [string]...[0]sounds cache
						// [string]..[0]item cache
	"svc_lightstyle",		// [byte] [string]
	"svc_updatename",		// [byte] [string]
	"svc_updatefrags",	// [byte] [short]
	"svc_clientdata",		// <shortbits + data>
	"svc_stopsound",		// <see code>
	"svc_updatecolors",	// [byte] [byte]
	"svc_particle",		// [vec3] <variable>
	"svc_damage",			// [byte] impact [byte] blood [vec3] from
	
	"svc_spawnstatic",
	"OBSOLETE svc_spawnbinary",
	"svc_spawnbaseline",
	
	"svc_temp_entity",		// <variable>
	"svc_setpause",
	"svc_signonnum",
	"svc_centerprint",
	"svc_killedmonster",
	"svc_foundsecret",
	"svc_spawnstaticsound",
	"svc_intermission",
	"svc_finale",			// [string] music [string] text
	"svc_cdtrack",			// [byte] track [byte] looptrack
	"svc_sellscreen",
	"svc_cutscene",
//johnfitz -- new server messages
	"svc_showlmp",		// [string] iconlabel [string] lmpfile [byte] x [byte] y
	"svc_hidelmp",		// [string] iconlabel
	"svc_skybox",			// [string] skyname
	"?", // 38
	"?", // 39
	"svc_bf", // 40						// no data
	"svc_fog", // 41				// [byte] density [byte] red [byte] green [byte] blue [float] time
	"svc_spawnbaseline2", //42			// support for large modelindex, large framenum, alpha, using flags
	"svc_spawnstatic2", // 43			// support for large modelindex, large framenum, alpha, using flags
	"svc_spawnstaticsound2", //	44		// [coord3] [short] samp [byte] vol [byte] aten
	"?", // 45
	"?", // 46
	"?", // 47
	"?", // 48
	"?", // 49
	"svc_skyboxsize", // [coord] size
	"svc_fogn"		// [byte] enable <optional past this point, only included if enable is true> [float] density [byte] red [byte] green [byte] blue
};


extern vec3_t	v_punchangles[2];
extern int	stufftext_frame;

qboolean warn_about_nehahra_protocol; //johnfitz

//=============================================================================

/*
===============
CL_EntityNum

This function checks and tracks the total number of entities
===============
*/
entity_t	*CL_EntityNum (int num)
{
	if (num < 0 || num >= cl.num_entities)
	{
		if (num < 0 || num >= MAX_EDICTS)
			Host_Error ("CL_EntityNum: invalid edict number (%d, max = %d)", num, MAX_EDICTS);

		while (cl.num_entities<=num)
		{
			cl_entities[cl.num_entities].colormap = 0;
			cl_entities[cl.num_entities].lerpflags |= LERP_RESETMOVE|LERP_RESETANIM; //johnfitz
			cl.num_entities++;
		}
	}
		
	return &cl_entities[num];
}

/*
==================
CL_ParseStartSoundPacket
==================
*/
void CL_ParseStartSoundPacket(void)
{
    vec3_t  pos;
    int 	channel, ent;
    int 	sound_num=0; // keep compiler happy
    int 	volume;
    int 	field_mask;
    float 	attenuation;  
    int		i;

    field_mask = MSG_ReadByte(net_message); 

    if (field_mask & SND_VOLUME)
		volume = MSG_ReadByte (net_message);
	else
		volume = DEFAULT_SOUND_PACKET_VOLUME;
	
    if (field_mask & SND_ATTENUATION)
		attenuation = MSG_ReadByte (net_message) / 64.0;
	else
		attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (cl.protocol == PROTOCOL_FITZQUAKE || cl.protocol == PROTOCOL_FITZQUAKE_PLUS)
	{
		if (field_mask & SND_LARGEENTITY)
		{
			ent = (unsigned short) MSG_ReadShort (net_message);
			channel = MSG_ReadByte (net_message);
		}
		else
		{
			channel = (unsigned short) MSG_ReadShort (net_message);
			ent = channel >> 3;
			channel &= 7;
		}

		if (field_mask & SND_LARGESOUND)
			sound_num = (unsigned short) MSG_ReadShort (net_message);
		else
			sound_num = MSG_ReadByte (net_message);
	}
	else
	{
		channel = MSG_ReadShort (net_message);

		if (cl.protocol == PROTOCOL_NETQUAKE)
			sound_num = MSG_ReadByte (net_message);
		else if (cl.protocol == PROTOCOL_BJP)
			sound_num = MSG_ReadByte (net_message);
		else if (cl.protocol == PROTOCOL_BJP2 || cl.protocol == PROTOCOL_BJP3)
			sound_num = MSG_ReadShort (net_message);

		ent = (channel & 0x7FFF) >> 3;
		channel &= 7;
	}
	//johnfitz

	if (sound_num >= MAX_SOUNDS)
		Host_Error ("CL_ParseStartSoundPacket: invalid sound_num (%d, max = %d)", sound_num, MAX_SOUNDS);

	if (ent < 0 || ent >= MAX_EDICTS)
		Host_Error ("CL_ParseStartSoundPacket: invalid edict (%d, max = %d)", ent, MAX_EDICTS);
	
	for (i=0 ; i<3 ; i++)
		pos[i] = MSG_ReadCoord (net_message);
 
    S_StartSound (ent, channel, cl.sound_precache[sound_num], pos, volume/255.0, attenuation);
}       

/*
==================
CL_KeepaliveMessage

When the client is taking a long time to load stuff, send keepalive messages
so the server doesn't disconnect.
==================
*/
void CL_KeepaliveMessage (void)
{
	float	time;
	static float lastmsg;
	int		ret;
	sizebuf_t	old;
	byte		olddata[NET_MAXMESSAGE], data;
	
	if (sv.active)
		return;		// no need if server is local
	if (cls.demoplayback)
		return;

// read messages from server, should just be nops
	old = *net_message->message;
	memcpy (olddata, net_message->message->data, net_message->message->cursize);
	
	do
	{
		ret = CL_GetMessage ();
		switch (ret)
		{
		default:
			Host_Error ("CL_KeepaliveMessage: CL_GetMessage failed (%d)", ret);
		case 0:
			break;	// nothing waiting
		case 1:
			Host_Error ("CL_KeepaliveMessage: received a message");
			break;
		case 2:
			data = MSG_ReadByte (net_message);

			if (data != svc_nop)
				Host_Error ("CL_KeepaliveMessage: datagram %d wasn't a nop", data);
			break;
		}
	} while (ret);

	*net_message->message = old;
	memcpy (net_message->message->data, olddata, net_message->message->cursize);

// check time
	time = Sys_DoubleTime ();
	if (time - lastmsg < 5)
		return;
	lastmsg = time;

// write out a nop
	Con_Printf ("--> client to server keepalive\n");

	MSG_WriteByte (&cls.message, clc_nop);
	NET_SendMessage (cls.netcon, &cls.message);
	SZ_Clear (&cls.message);
}

/*
==================
CL_ParseServerInfo
==================
*/
void CL_ParseServerInfo (void)
{
	char	*str, str2[MAX_QPATH + 100];
	int		i;
	int		nummodels, numsounds;
	char	model_precache[MAX_MODELS][MAX_QPATH];
	char	sound_precache[MAX_SOUNDS][MAX_QPATH];
	
	Con_DPrintf ("\nServerinfo packet received.\n");
//
// wipe the client_state_t struct
//
	CL_ClearState ();

// parse protocol version number
	i = MSG_ReadLong (net_message);
	if (i == PROTOCOL_BJP || i == PROTOCOL_BJP2 || i == PROTOCOL_BJP3)
		Con_SafePrintf ("\nusing BJP demo protocol %i\n", i);
	//johnfitz -- support multiple protocols
	else if (i != PROTOCOL_NETQUAKE && i != PROTOCOL_FITZQUAKE && i != PROTOCOL_FITZQUAKE_PLUS)
	{
		Con_SafePrintf ("\n"); // because there's no newline after serverinfo print
		Host_Error ("CL_ParseServerInfo: Server returned unknown protocol version %i, not %i, %i or %i", i, 
			PROTOCOL_NETQUAKE, PROTOCOL_FITZQUAKE, PROTOCOL_FITZQUAKE_PLUS);
	}

	cl.protocol = i;
	Con_DPrintf ("Server protocol is %i", i);

// parse maxclients
	cl.maxclients = MSG_ReadByte (net_message);
	if (cl.maxclients < 1 || cl.maxclients > MAX_SCOREBOARD)
	{
		Con_Printf("Bad maxclients (%u) from server\n", cl.maxclients);
		return;
	}
	cl.scores = Hunk_AllocName (cl.maxclients*sizeof(*cl.scores), "scores");

// parse gametype
	cl.gametype = MSG_ReadByte (net_message);

// parse signon message
	str = MSG_ReadString (net_message);
	strncpy (cl.levelname, str, sizeof(cl.levelname)-1);

// seperate the printfs so the server message can have a color
	Con_SafePrintf ("\n\n%s\n", Con_Quakebar(40));
	Con_SafePrintf ("%c%s\n", 2, str);

// tell user which protocol this is
	Con_SafePrintf ("Using protocol %i\n", cl.protocol);

// first we go through and touch all of the precache data that still
// happens to be in the cache, so precaching something else doesn't
// needlessly purge it
//

// precache models
	memset (cl.model_precache, 0, sizeof(cl.model_precache));
	for (nummodels=1 ; ; nummodels++)
	{
		str = MSG_ReadString (net_message);
		if (!str[0])
			break;
		if (nummodels == ((cl.protocol == PROTOCOL_NETQUAKE) ? 256 : MAX_MODELS))
			Host_Error ("CL_ParseServerInfo: Server sent too many model precaches (max = %d)", (cl.protocol == PROTOCOL_NETQUAKE) ? 256 : MAX_MODELS);
		strcpy (model_precache[nummodels], str);
		Mod_TouchModel (str);
	}

	//johnfitz -- check for excessive models
	if (nummodels >= 256)
		Con_DWarning ("CL_ParseServerInfo: models exceeds standard limit (%d, normal max = %d)\n", nummodels, 256);
	//johnfitz

// precache sounds
	memset (cl.sound_precache, 0, sizeof(cl.sound_precache));
	for (numsounds=1 ; ; numsounds++)
	{
		str = MSG_ReadString (net_message);
		if (!str[0])
			break;
		if (numsounds == ((cl.protocol == PROTOCOL_NETQUAKE) ? 256 : MAX_SOUNDS))
			Host_Error ("CL_ParseServerInfo: Server sent too many sound precaches (max = %d)", (cl.protocol == PROTOCOL_NETQUAKE) ? 256 : MAX_SOUNDS);
		strcpy (sound_precache[numsounds], str);
		S_TouchSound (str);
	}

	//johnfitz -- check for excessive sounds
	if (numsounds >= 256)
		Con_DWarning ("CL_ParseServerInfo: sounds exceeds standard limit (%d, normal max = %d)\n", numsounds, 256);
	//johnfitz
	
	// Baker: maps/e1m1.bsp ---> e1m1
	// We need this early for external vis to know if a model is worldmodel or not
	COM_StripExtension (COM_SkipPath(model_precache[1]), cl.worldname);

//
// now we try to load everything else until a cache allocation fails
//

	for (i=1 ; i<nummodels ; i++)
	{
		cl.model_precache[i] = Mod_ForName (model_precache[i], false);
		if (cl.model_precache[i] == NULL)
		{
			sprintf (str2, "Model %s not found", model_precache[i]);

			if (i == 1)
				Host_Error (str2); // World not found

			Con_Printf ("%s\n", str2);
			return;
		}
		CL_KeepaliveMessage ();
	}

	for (i=1 ; i<numsounds ; i++)
	{
		cl.sound_precache[i] = S_PrecacheSound (sound_precache[i]);
		CL_KeepaliveMessage ();
	}

// local state
	cl_entities[0].model = cl.worldmodel = cl.model_precache[1];

	R_NewMap ();

	Hunk_Check ();		// make sure nothing is hurt

	warn_about_nehahra_protocol = true; //johnfitz -- warn about Nehahra protocol hack once per server connection
}

/*
==================
CL_ParseUpdate

Parse an entity update message from the server
If an entities model or origin changes from frame to frame, it must be
relinked.  Other attributes can change without relinking.
==================
*/
void CL_ParseUpdate (int bits)
{
	int			i;
	model_t		*model;
	int			modnum=0; // keep compiler happy
	qboolean	forcelink;
	entity_t	*ent;
	int			num;
	int			skin;

	if (cls.signon == SIGNONS - 1)
	{	// first update is the final signon stage
		cls.signon = SIGNONS;
		CL_SignonReply ();
	}

	if (bits & U_MOREBITS)
	{
		i = MSG_ReadByte (net_message);
		bits |= (i<<8);
	}

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (cl.protocol == PROTOCOL_FITZQUAKE || cl.protocol == PROTOCOL_FITZQUAKE_PLUS)
	{
		if (bits & U_EXTEND1)
			bits |= MSG_ReadByte (net_message) << 16;
		if (bits & U_EXTEND2)
			bits |= MSG_ReadByte (net_message) << 24;
	}
	//johnfitz

	if (bits & U_LONGENTITY)
		num = MSG_ReadShort (net_message);
	else
		num = MSG_ReadByte (net_message);

	ent = CL_EntityNum (num);

	// Exclude the view from this, otherwise screen sometimes jerks badly in some demos
//	if (ent->msgtime != cl.mtime[1] && (!cls.demoplayback || ent != &cl_entities[cl.viewentity]))
	if (ent->msgtime != cl.mtime[1])
		forcelink = true;	// no previous frame to lerp from
	else
		forcelink = false;

	//johnfitz -- lerping
	if (ent->msgtime + 0.2 < cl.mtime[0]) //more than 0.2 seconds since the last message (most entities think every 0.1 sec)
		ent->lerpflags |= LERP_RESETANIM; //if we missed a think, we'd be lerping from the wrong frame
	//johnfitz

	ent->msgtime = cl.mtime[0];

	if (bits & U_MODEL)
	{
		if (cl.protocol == PROTOCOL_FITZQUAKE || cl.protocol == PROTOCOL_FITZQUAKE_PLUS)
			modnum = MSG_ReadByte (net_message);
		else if (cl.protocol == PROTOCOL_NETQUAKE)
			modnum = MSG_ReadByte (net_message);
		else if (cl.protocol == PROTOCOL_BJP || cl.protocol == PROTOCOL_BJP2 || cl.protocol == PROTOCOL_BJP3)
			modnum = MSG_ReadShort (net_message);

		if (modnum < 0 || modnum >= ((cl.protocol == PROTOCOL_NETQUAKE) ? 256 : MAX_MODELS))
			Host_Error ("CL_ParseUpdate: invalid model (%d, max = %d)", modnum, (cl.protocol == PROTOCOL_NETQUAKE) ? 256 : MAX_MODELS);
	}
	else
		modnum = ent->baseline.modelindex;

	if (bits & U_FRAME)
		ent->frame = MSG_ReadByte (net_message);
	else
		ent->frame = ent->baseline.frame;

	if (bits & U_COLORMAP)
		i = MSG_ReadByte(net_message);
	else
		i = ent->baseline.colormap;
	if (!i)
		ent->colormap = 0;
	else
	{
		if (i < 0 || i > cl.maxclients)
			Host_Error ("CL_ParseUpdate: invalid colormap (%d, max = %d)", i, cl.maxclients);
		ent->colormap = (byte *)i;
	}

	if (bits & U_SKIN)
		skin = MSG_ReadByte(net_message);
	else
		skin = ent->baseline.skin;

	if (skin != ent->skinnum) 
	{
		// skin has changed
		ent->skinnum = skin;
		if (num > 0 && num <= cl.maxclients)
			R_TranslatePlayerSkin (num - 1);
	}

	if (bits & U_EFFECTS)
		ent->effects = MSG_ReadByte(net_message);
	else
		ent->effects = ent->baseline.effects;

// shift the known values for interpolation
	VectorCopy (ent->msg_origins[0], ent->msg_origins[1]);
	VectorCopy (ent->msg_angles[0], ent->msg_angles[1]);

	if (bits & U_ORIGIN1)
		ent->msg_origins[0][0] = MSG_ReadCoord (net_message);
	else
		ent->msg_origins[0][0] = ent->baseline.origin[0];
	if (bits & U_ANGLE1)
		if (cl.protocol == PROTOCOL_FITZQUAKE_PLUS)
			ent->msg_angles[0][0] = MSG_ReadAngle16 (net_message); // Baker change
		else
			ent->msg_angles[0][0] = MSG_ReadAngle (net_message);
	else
		ent->msg_angles[0][0] = ent->baseline.angles[0];

	if (bits & U_ORIGIN2)
		ent->msg_origins[0][1] = MSG_ReadCoord (net_message);
	else
		ent->msg_origins[0][1] = ent->baseline.origin[1];
	if (bits & U_ANGLE2)
		if (cl.protocol == PROTOCOL_FITZQUAKE_PLUS)
			ent->msg_angles[0][1] = MSG_ReadAngle16 (net_message); // Baker change
		else
			ent->msg_angles[0][1] = MSG_ReadAngle (net_message);
	else
		ent->msg_angles[0][1] = ent->baseline.angles[1];

	if (bits & U_ORIGIN3)
		ent->msg_origins[0][2] = MSG_ReadCoord (net_message);
	else
		ent->msg_origins[0][2] = ent->baseline.origin[2];
	if (bits & U_ANGLE3)
		if (cl.protocol == PROTOCOL_FITZQUAKE_PLUS)
			ent->msg_angles[0][2] = MSG_ReadAngle16 (net_message); // Baker change
		else
			ent->msg_angles[0][2] = MSG_ReadAngle (net_message);
	else
		ent->msg_angles[0][2] = ent->baseline.angles[2];

	//johnfitz -- lerping for movetype_step entities
	if (bits & U_STEP)
	{
		ent->lerpflags |= LERP_MOVESTEP;
		ent->forcelink = true;
	}
	else
		ent->lerpflags &= ~LERP_MOVESTEP;
	//johnfitz

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (cl.protocol == PROTOCOL_FITZQUAKE || cl.protocol == PROTOCOL_FITZQUAKE_PLUS)
	{
		if (bits & U_ALPHA)
			ent->alpha = MSG_ReadByte (net_message);
		else
			ent->alpha = ent->baseline.alpha;
		if (bits & U_FRAME2)
			ent->frame = (ent->frame & 0x00FF) | (MSG_ReadByte (net_message) << 8);
		if (bits & U_MODEL2)
			modnum = (modnum & 0x00FF) | (MSG_ReadByte (net_message) << 8);
		if (bits & U_LERPFINISH)
		{
			ent->lerpfinish = ent->msgtime + ((float)(MSG_ReadByte (net_message)) / 255);
			ent->lerpflags |= LERP_FINISH;
		}
		else
			ent->lerpflags &= ~LERP_FINISH;
	}
	else // PROTOCOL_NETQUAKE and PROTOCOL_NEHAHRA
	{
		// Nehahra
		if (bits & U_TRANS) // if this bit is set, assume this is Nehahra protocol
		{
			float trans = MSG_ReadFloat (net_message);
			float a = MSG_ReadFloat (net_message);

			if (!nehahra && warn_about_nehahra_protocol && 
				cl.protocol != PROTOCOL_BJP && 
				cl.protocol != PROTOCOL_BJP2 && 
				cl.protocol != PROTOCOL_BJP3) // don't warning if we played demo using BJP proto
			{
				Con_Warning ("nonstandard update bit, assuming Nehahra protocol\n");
				warn_about_nehahra_protocol = false;
			}

			if (trans == 2)
				ent->fullbright = MSG_ReadFloat (net_message); // fullbright (not using this yet)

			ent->alpha = ENTALPHA_ENCODE(a); // alpha
		}
		else
		{
			ent->fullbright = 0;
//			ent->alpha = ENTALPHA_DEFAULT;
			ent->alpha = ent->baseline.alpha;
		}
	}
	//johnfitz

	//johnfitz -- moved here from above (because the model num could be changed by extend bits)
	model = cl.model_precache[modnum];
	if (model != ent->model)
	{
		ent->model = model;
		// automatic animation (torches, etc) can be either all together
		// or randomized
		if (model)
		{
			if (model->synctype == ST_RAND)
				ent->syncbase = (float)(rand()&0x7fff) / 0x7fff;
			else
				ent->syncbase = 0.0;
		}
		else
			forcelink = true;	// hack to make null model players work

		if (num > 0 && num <= cl.maxclients)
			R_TranslatePlayerSkin (num - 1);

		ent->lerpflags |= LERP_RESETANIM; //johnfitz -- don't lerp animation across model changes
	}
	//johnfitz

	if (forcelink)
	{	// didn't have an update last message
		VectorCopy (ent->msg_origins[0], ent->msg_origins[1]);
		VectorCopy (ent->msg_origins[0], ent->origin);
		VectorCopy (ent->msg_angles[0], ent->msg_angles[1]);
		VectorCopy (ent->msg_angles[0], ent->angles);
		ent->forcelink = true;
	}
}

/*
==================
CL_ParseBaseline
==================
*/
void CL_ParseBaseline (entity_t *ent, int version) //johnfitz -- added argument
{
	int			i;
	int bits; //johnfitz

	bits = (version == 2) ? MSG_ReadByte (net_message) : 0;

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (cl.protocol == PROTOCOL_FITZQUAKE || cl.protocol == PROTOCOL_FITZQUAKE_PLUS)
	{
		ent->baseline.modelindex = (bits & B_LARGEMODEL) ? MSG_ReadShort (net_message) : MSG_ReadByte (net_message);
		ent->baseline.frame = (bits & B_LARGEFRAME) ? MSG_ReadShort (net_message) : MSG_ReadByte (net_message);
		ent->baseline.colormap = MSG_ReadByte (net_message);
		ent->baseline.skin = MSG_ReadByte (net_message);
	}
	//johnfitz
	else
	{
		if (cl.protocol == PROTOCOL_NETQUAKE)
			ent->baseline.modelindex = MSG_ReadByte (net_message);
		else if (cl.protocol == PROTOCOL_BJP || cl.protocol == PROTOCOL_BJP2 || cl.protocol == PROTOCOL_BJP3)
			ent->baseline.modelindex = MSG_ReadShort (net_message);

		ent->baseline.frame = MSG_ReadByte (net_message);
		ent->baseline.colormap = MSG_ReadByte(net_message);
		ent->baseline.skin = MSG_ReadByte(net_message);
	}

	for (i=0 ; i<3 ; i++)
	{
		ent->baseline.origin[i] = MSG_ReadCoord (net_message);
		ent->baseline.angles[i] = MSG_ReadAngle (net_message);
	}

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (cl.protocol == PROTOCOL_FITZQUAKE || cl.protocol == PROTOCOL_FITZQUAKE_PLUS)
		ent->baseline.alpha = (bits & B_ALPHA) ? MSG_ReadByte (net_message) : ENTALPHA_DEFAULT;
	else 
		ent->baseline.alpha = ENTALPHA_DEFAULT;
}


/*
==================
CL_ParseClientdata

Server information pertaining to this client only
==================
*/
void CL_ParseClientdata (void)
{
	int		i, j;
	int		bits;

	bits = (unsigned short)MSG_ReadShort (net_message); // read bits here isntead of in CL_ParseServerMessage()

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (cl.protocol == PROTOCOL_FITZQUAKE || cl.protocol == PROTOCOL_FITZQUAKE_PLUS)
	{
		if (bits & SU_EXTEND1)
			bits |= (MSG_ReadByte (net_message) << 16);
		if (bits & SU_EXTEND2)
			bits |= (MSG_ReadByte (net_message) << 24);
	}
	//johnfitz

	if (bits & SU_VIEWHEIGHT)
		cl.viewheight = MSG_ReadChar (net_message);
	else
		cl.viewheight = DEFAULT_VIEWHEIGHT;

	if (bits & SU_IDEALPITCH)
		cl.idealpitch = MSG_ReadChar (net_message);
	else
		cl.idealpitch = 0;
	
	VectorCopy (cl.mvelocity[0], cl.mvelocity[1]);
	for (i=0 ; i<3 ; i++)
	{
		if (bits & (SU_PUNCH1<<i) )
			cl.punchangle[i] = MSG_ReadChar(net_message);
		else
			cl.punchangle[i] = 0;
		if (bits & (SU_VELOCITY1<<i) )
			cl.mvelocity[0][i] = MSG_ReadChar(net_message)*16;
		else
			cl.mvelocity[0][i] = 0;
	}

	// update v_punchangles
	if (v_punchangles[0][0] != cl.punchangle[0] || v_punchangles[0][1] != cl.punchangle[1] || v_punchangles[0][2] != cl.punchangle[2])
	{
		VectorCopy (v_punchangles[0], v_punchangles[1]);
		VectorCopy (cl.punchangle, v_punchangles[0]);
	}

// [always sent]	if (bits & SU_ITEMS)
		i = MSG_ReadLong (net_message);

	if (cl.items != i)
	{	// set flash times
		Sbar_Changed ();
		for (j=0 ; j<32 ; j++)
			if ( (i & (1<<j)) && !(cl.items & (1<<j)))
				cl.item_gettime[j] = cl.time;
		cl.items = i;
	}
		
	cl.onground = (bits & SU_ONGROUND) != 0;
	cl.inwater = (bits & SU_INWATER) != 0;

	if (bits & SU_WEAPONFRAME)
		cl.stats[STAT_WEAPONFRAME] = MSG_ReadByte (net_message);
	else
		cl.stats[STAT_WEAPONFRAME] = 0;

	if (bits & SU_ARMOR)
		i = MSG_ReadByte (net_message);
	else
		i = 0;
	if (cl.stats[STAT_ARMOR] != i)
	{
		cl.stats[STAT_ARMOR] = i;
		Sbar_Changed ();
	}

	if (bits & SU_WEAPON)
	{
		if (cl.protocol == PROTOCOL_FITZQUAKE || cl.protocol == PROTOCOL_FITZQUAKE_PLUS)
			i = MSG_ReadByte (net_message);
		else if (cl.protocol == PROTOCOL_NETQUAKE)
			i = MSG_ReadByte (net_message);
		else if (cl.protocol == PROTOCOL_BJP || cl.protocol == PROTOCOL_BJP2 || cl.protocol == PROTOCOL_BJP3)
			i = MSG_ReadShort (net_message);
	}
	else
		i = 0;
	if (cl.stats[STAT_WEAPON] != i)
	{
		cl.stats[STAT_WEAPON] = i;
		Sbar_Changed ();

		//johnfitz -- lerping
		if (cl.viewent.model != cl.model_precache[cl.stats[STAT_WEAPON]])
			cl.viewent.lerpflags |= LERP_RESETANIM; //don't lerp animation across model changes
		//johnfitz
	}
	
	i = MSG_ReadShort (net_message);
	if (cl.stats[STAT_HEALTH] != i)
	{
		cl.stats[STAT_HEALTH] = i;
		Sbar_Changed ();
	}

	i = MSG_ReadByte (net_message);
	if (cl.stats[STAT_AMMO] != i)
	{
		cl.stats[STAT_AMMO] = i;
		Sbar_Changed ();
	}

	for (i=0 ; i<4 ; i++)
	{
		j = MSG_ReadByte (net_message);
		if (cl.stats[STAT_SHELLS+i] != j)
		{
			cl.stats[STAT_SHELLS+i] = j;
			Sbar_Changed ();
		}
	}

	i = MSG_ReadByte (net_message);

	if (standard_quake)
	{
		if (cl.stats[STAT_ACTIVEWEAPON] != i)
		{
			cl.stats[STAT_ACTIVEWEAPON] = i;
			Sbar_Changed ();
		}
	}
	else
	{
		if (cl.stats[STAT_ACTIVEWEAPON] != (1<<i))
		{
			cl.stats[STAT_ACTIVEWEAPON] = (1<<i);
			Sbar_Changed ();
		}
	}

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (cl.protocol == PROTOCOL_FITZQUAKE || cl.protocol == PROTOCOL_FITZQUAKE_PLUS)
	{
		if (bits & SU_WEAPON2)
			cl.stats[STAT_WEAPON] |= (MSG_ReadByte(net_message) << 8);
		if (bits & SU_ARMOR2)
			cl.stats[STAT_ARMOR] |= (MSG_ReadByte(net_message) << 8);
		if (bits & SU_AMMO2)
			cl.stats[STAT_AMMO] |= (MSG_ReadByte(net_message) << 8);
		if (bits & SU_SHELLS2)
			cl.stats[STAT_SHELLS] |= (MSG_ReadByte(net_message) << 8);
		if (bits & SU_NAILS2)
			cl.stats[STAT_NAILS] |= (MSG_ReadByte(net_message) << 8);
		if (bits & SU_ROCKETS2)
			cl.stats[STAT_ROCKETS] |= (MSG_ReadByte(net_message) << 8);
		if (bits & SU_CELLS2)
			cl.stats[STAT_CELLS] |= (MSG_ReadByte(net_message) << 8);
		if (bits & SU_WEAPONFRAME2)
			cl.stats[STAT_WEAPONFRAME] |= (MSG_ReadByte(net_message) << 8);
		if (bits & SU_WEAPONALPHA)
			cl.viewent.alpha = MSG_ReadByte(net_message);
		else
			cl.viewent.alpha = ENTALPHA_DEFAULT;
	}
	else 
		cl.viewent.alpha = ENTALPHA_DEFAULT;
	//johnfitz
}

/*
=====================
CL_NewTranslation
=====================
*/
void CL_NewTranslation (int slot)
{
	R_TranslatePlayerSkin (slot);
}

/*
=====================
CL_ParseStatic
=====================
*/
void CL_ParseStatic (int version) //johnfitz -- added a parameter
{
	entity_t *ent;
	int		i;

	i = cl.num_statics;
	if (i >= MAX_STATIC_ENTITIES)
		Host_Error ("CL_ParseStatic: too many (%d) static entities, max = %d", i, MAX_STATIC_ENTITIES);
	ent = &cl_static_entities[i];
	cl.num_statics++;
	CL_ParseBaseline (ent, version); //johnfitz -- added second parameter

// copy it to the current state
	ent->model = cl.model_precache[ent->baseline.modelindex];
	ent->lerpflags |= LERP_RESETANIM; //johnfitz -- lerping
	ent->frame = ent->baseline.frame;
	ent->syncbase = (float)rand() / RAND_MAX; // Hack to make flames unsynchronized
	ent->colormap = 0;
	ent->skinnum = ent->baseline.skin;
	ent->effects = ent->baseline.effects;
	ent->alpha = ent->baseline.alpha; //johnfitz -- alpha

	VectorCopy (ent->baseline.origin, ent->origin);
	VectorCopy (ent->baseline.angles, ent->angles);
	R_AddEfrags (ent);
}

/*
===================
CL_ParseStaticSound
===================
*/
void CL_ParseStaticSound (int version) //johnfitz -- added argument
{
	vec3_t		org;
	int			sound_num=0, vol, atten; // keep compiler happy
	int			i;
	
	for (i=0 ; i<3 ; i++)
		org[i] = MSG_ReadCoord (net_message);

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (cl.protocol == PROTOCOL_FITZQUAKE || cl.protocol == PROTOCOL_FITZQUAKE_PLUS)
	{
		if (version == 2)
			sound_num = MSG_ReadShort (net_message);
		else
			sound_num = MSG_ReadByte (net_message);
	}
	//johnfitz
	else
	{
		if (cl.protocol == PROTOCOL_NETQUAKE)
			sound_num = MSG_ReadByte (net_message);
		else if (cl.protocol == PROTOCOL_BJP || cl.protocol == PROTOCOL_BJP3)
			sound_num = MSG_ReadByte (net_message);
		else if (cl.protocol == PROTOCOL_BJP2)
			sound_num = MSG_ReadShort (net_message);
	}

	if (sound_num >= MAX_SOUNDS)
		Host_Error ("CL_ParseStaticSound: invalid sound (%d, max = %d)", sound_num, MAX_SOUNDS);

	vol = MSG_ReadByte (net_message);
	atten = MSG_ReadByte (net_message);
	
	S_StaticSound (cl.sound_precache[sound_num], org, vol, atten);
}

/*
===================
Svc_Name
===================
*/
static char *Svc_Name (int cmd)
{
	if (cmd == -1)
		return "none";

	cmd &= 255;

	if (cmd & 128)
		return "fast update";

	return svc_strings[cmd];
}

#define SHOWNET(x) if(cl_shownet.value==2)Con_Printf ("%3i:%s\n", net_message->readcount - 1, x);

/*
=====================
CL_ParseServerMessage
=====================
*/
void CL_ParseServerMessage (void)
{
	int cmd = -1;
	int i, lastpos = 0, lastcmd;

//
// if recording demos, copy the message out
//
	if (cl_shownet.value == 1)
		Con_Printf ("%i ",net_message->message->cursize);
	else if (cl_shownet.value == 2)
		Con_Printf ("------------------\n");
	
	cl.onground = false;	// unless the server says otherwise	
//
// parse the message
//
	MSG_BeginReading (net_message);

	while (1)
	{
		if (net_message->badread)
		{
			char s[512];

			sprintf (s, "CL_ParseServerMessage: insufficient data in service '%s', size %d", Svc_Name(cmd), net_message->readcount - lastpos);
			Host_Error (s);
		}

		lastpos = net_message->readcount;
		lastcmd = cmd;

		cmd = MSG_ReadByte (net_message);

		if (cmd == -1)
		{
			SHOWNET("END OF MESSAGE");
			return;		// end of message
		}

	// if the high bit of the command byte is set, it is a fast update
		if (cmd & U_SIGNAL) // was 128, changed for clarity
		{
			SHOWNET("fast update");
			CL_ParseUpdate (cmd&127);
			continue;
		}

		SHOWNET(svc_strings[cmd]);
	
	// other commands
		switch (cmd)
		{
		default:
			Host_Error ("CL_ParseServerMessage: illegible server message (service %d, last service '%s')", cmd, Svc_Name(lastcmd));
			break;
			
		case svc_nop:
//			Con_Printf ("svc_nop\n");
			break;
			
		case svc_time:
			cl.mtime[1] = cl.mtime[0];
			cl.mtime[0] = MSG_ReadFloat (net_message);
			break;
			
		case svc_clientdata:
			CL_ParseClientdata (); // removed bits parameter, we will read this inside CL_ParseClientdata()
			break;
			
		case svc_version:
			// svc_version is never used in the engine. wtf? maybe it's from an older version of stuff?
			// don't read flags anyway for compatibility as we have no control over what sent the message
			i = MSG_ReadLong (net_message);
			if (i == PROTOCOL_BJP || i == PROTOCOL_BJP2 || i == PROTOCOL_BJP3)
				Con_SafePrintf ("using BJP demo protocol version %i\n", i);
			//johnfitz -- support multiple protocols
			else if (i != PROTOCOL_NETQUAKE && i != PROTOCOL_FITZQUAKE && i != PROTOCOL_FITZQUAKE_PLUS)
				Host_Error ("CL_ParseServerMessage: Server protocol is %i instead of %i, %i or %i", i, 
					PROTOCOL_NETQUAKE, PROTOCOL_FITZQUAKE, PROTOCOL_FITZQUAKE_PLUS);
			cl.protocol = i;
			Con_DPrintf ("Using protocol version %i\n", cl.protocol);
			//johnfitz
			break;
			
		case svc_disconnect:
			Con_Printf ("Disconnect\n");
			Host_EndGame ("Server disconnected\n");
			
		case svc_print:
			Con_SafePrintf ("%s", MSG_ReadString (net_message));
			break;
			
		case svc_centerprint:
			SCR_CenterPrint (MSG_ReadString (net_message));
			break;
			
		case svc_stufftext:
			stufftext_frame = host_framecount; // Pa3PyX: allow full frame update on stuff messages.
			Cbuf_AddText (MSG_ReadString (net_message));
			break;
			
		case svc_damage:
			V_ParseDamage ();
			break;
			
		case svc_serverinfo:
			CL_ParseServerInfo ();
			vid.recalc_refdef = true;	// leave intermission full screen
			break;
			
		case svc_setangle:
			for (i=0 ; i<3 ; i++)
				cl.viewangles[i] = MSG_ReadAngle (net_message);

			if (!cls.demoplayback)
			{
				VectorCopy (cl.mviewangles[0], cl.mviewangles[1]);

				// From ProQuake - hack with cl.last_angle_time to autodetect continuous svc_setangles
				if (cl.last_angle_time > host_time - 0.3)
					cl.last_angle_time = host_time + 0.3;
				else if (cl.last_angle_time > host_time - 0.6)
					cl.last_angle_time = host_time;
				else
					cl.last_angle_time = host_time - 0.3;

				for (i=0 ; i<3 ; i++)
					cl.mviewangles[0][i] = cl.viewangles[i];
			}
			break;
			
		case svc_setview:
			cl.viewentity = MSG_ReadShort (net_message);
			if (cl.viewentity >= MAX_EDICTS)
				Host_Error ("CL_ParseServerMessage: svc_setview %d >= MAX_EDICTS (%d)", cl.viewentity, MAX_EDICTS);
			break;
			
		case svc_lightstyle:
			i = MSG_ReadByte (net_message);
			if (i >= MAX_LIGHTSTYLES)
				Host_Error ("CL_ParseServerMessage: svc_lightstyle %d >= MAX_LIGHTSTYLES (%d)", i, MAX_LIGHTSTYLES);
			strcpy (cl_lightstyle[i].map,  MSG_ReadString(net_message));
			cl_lightstyle[i].length = strlen(cl_lightstyle[i].map);
			break;
			
		case svc_sound:
			CL_ParseStartSoundPacket();
			break;
			
		case svc_stopsound:
			i = MSG_ReadShort(net_message);
			S_StopSound(i>>3, i&7);
			break;
			
		case svc_updatename:
			Sbar_Changed ();
			i = MSG_ReadByte (net_message);
			if (i >= cl.maxclients)
				Host_Error ("CL_ParseServerMessage: svc_updatename %d >= cl.maxclients (%d)", i, cl.maxclients);
			strcpy (cl.scores[i].name, MSG_ReadString (net_message));
			break;
			
		case svc_updatefrags:
			Sbar_Changed ();
			i = MSG_ReadByte (net_message);
			if (i >= cl.maxclients)
				Host_Error ("CL_ParseServerMessage: svc_updatefrags %d >= cl.maxclients (%d)", i, cl.maxclients);
			cl.scores[i].frags = MSG_ReadShort (net_message);
			break;			
			
		case svc_updatecolors:
			Sbar_Changed ();
			i = MSG_ReadByte (net_message);
			if (i >= cl.maxclients)
				Host_Error ("CL_ParseServerMessage: svc_updatecolors %d >= cl.maxclients (%d)", i, cl.maxclients);
			cl.scores[i].colors = MSG_ReadByte (net_message);
			CL_NewTranslation (i);
			break;
			
		case svc_particle:
			R_ParseParticleEffect ();
			break;
			
		case svc_spawnbaseline:
			i = MSG_ReadShort (net_message);
			// must use CL_EntityNum() to force cl.num_entities up
			CL_ParseBaseline (CL_EntityNum(i), 1); // johnfitz -- added second parameter
			break;
			
		case svc_spawnstatic:
			CL_ParseStatic (1); //johnfitz -- added parameter
			break;
			
		case svc_temp_entity:
			CL_ParseTEnt ();
			break;
			
		case svc_setpause:
			cl.paused = MSG_ReadByte (net_message);
			if (cl.paused)
				CDAudio_Pause ();
			else
				CDAudio_Resume ();
			break;
			
		case svc_signonnum:
			i = MSG_ReadByte (net_message);
			if (i <= cls.signon)
				Host_Error ("CL_ParseServerMessage: Received signon %i when at %i", i, cls.signon);
			cls.signon = i;
			if (i == 2) // if signonnum==2, signon packet has been fully parsed, so check for excessive static ents
			{
				if (cl.num_statics > 128) // old limit warning
					Con_DWarning ("CL_ParseServerMessage: static entities exceeds standard limit (%d, normal max = %d)\n", cl.num_statics, 128);
			}
			CL_SignonReply ();
			break;
			
		case svc_killedmonster:
			cl.stats[STAT_MONSTERS]++;
			if ((!cls.demoplayback || developer.value) &&
			    cl.stats[STAT_TOTALMONSTERS] && cl.stats[STAT_MONSTERS] > cl.stats[STAT_TOTALMONSTERS])
			{
				Con_Warning ("CL_ParseServerMessage: killed monsters %d > total monsters %d\n", cl.stats[STAT_MONSTERS], cl.stats[STAT_TOTALMONSTERS]);
			}
			break;
			
		case svc_foundsecret:
			cl.stats[STAT_SECRETS]++;
			break;
			
		case svc_updatestat:
			i = MSG_ReadByte (net_message);
			if (i >= MAX_CL_STATS)
				Host_Error ("CL_ParseServerMessage: invalid svc_updatestat (%d, max = %d)", i, MAX_CL_STATS);
			cl.stats[i] = MSG_ReadLong (net_message);;
			break;
			
		case svc_spawnstaticsound:
			CL_ParseStaticSound (1); //johnfitz -- added parameter
			break;
			
		case svc_cdtrack:
			cl.cdtrack = MSG_ReadByte (net_message);
			cl.looptrack = MSG_ReadByte (net_message);
			if (strcasecmp(bgmtype.string, "cd") == 0)
			{
				if ( (cls.demoplayback || cls.demorecording) && (cls.forcetrack != -1) )
					CDAudio_Play ((byte)cls.forcetrack, true);
				else
					CDAudio_Play ((byte)cl.cdtrack, true);
			}
			else
				CDAudio_Stop ();
			break;
			
		case svc_intermission:
			cl.intermission = 1;
			cl.completed_time = cl.time;
			vid.recalc_refdef = true;	// go to full screen
			if (cls.demoplayback)
			{
				// Fix camera view angles (better way to do it?)
				entity_t *ent = &cl_entities[cl.viewentity];
				VectorCopy (ent->msg_angles[0], ent->angles);
			}
			break;
			
		case svc_finale:
			cl.intermission = 2;
			cl.completed_time = cl.time;
			vid.recalc_refdef = true;	// go to full screen
			if (cls.demoplayback)
			{
				// Fix camera view angles (better way to do it?)
				entity_t *ent = &cl_entities[cl.viewentity];
				VectorCopy (ent->msg_angles[0], ent->angles);
			}
			SCR_CenterPrint (MSG_ReadString (net_message));
			break;
			
		case svc_cutscene:
			cl.intermission = 3;
			cl.completed_time = cl.time;
			vid.recalc_refdef = true;	// go to full screen
			if (cls.demoplayback)
			{
				// Fix camera view angles (better way to do it?)
				entity_t *ent = &cl_entities[cl.viewentity];
				VectorCopy (ent->msg_angles[0], ent->angles);
			}
			SCR_CenterPrint (MSG_ReadString (net_message));
			break;
			
		case svc_sellscreen:
			Cmd_ExecuteString ("help", src_command);
			break;
// FIXME:
// Nehahra
		case svc_hidelmp:
			MSG_ReadString (net_message); // Just parse msg
			break;
			
		case svc_showlmp:
			// Just parse msg
			MSG_ReadString (net_message);
			MSG_ReadString (net_message);
			MSG_ReadByte(net_message);
			MSG_ReadByte(net_message);
			break;
			
		case svc_skybox:
			R_LoadSkyBox (MSG_ReadString(net_message));
			break;
			
		case svc_skyboxsize:
			// Just parse msg
			MSG_ReadCoord (net_message);
			break;
			
		case svc_bf:
			Cmd_ExecuteString ("bf", src_command);
			break;
			
// PROTOCOL_FITZQUAKE
		case svc_fog:
			R_FogParseServerMessage ();
			break;
			
		//johnfitz
		case svc_spawnbaseline2: //PROTOCOL_FITZQUAKE
			i = MSG_ReadShort (net_message);
			// must use CL_EntityNum() to force cl.num_entities up
			CL_ParseBaseline (CL_EntityNum(i), 2);
			break;
			
		case svc_spawnstatic2: //PROTOCOL_FITZQUAKE
			CL_ParseStatic (2);
			break;
			
		case svc_spawnstaticsound2: //PROTOCOL_FITZQUAKE
			CL_ParseStaticSound (2);
			break;
		//johnfitz
			
// Nehahra fog
		case svc_fogn:
			if (MSG_ReadByte(net_message))
			{
				R_FogParseServerMessage2 ();
			} 
			break;
		}
	}
}

