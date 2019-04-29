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
// host.c -- coordinates spawning and killing of local servers

#include "quakedef.h"

/*

A server can always be started, even if the system started out as a client
to a remote system.

A client can NOT be started if the system started as a dedicated server.

Memory is cleared / released when a server or client begins, not when they end.

*/

void Host_WriteConfiguration (char *configname);

quakeparms_t *host_parms;

qboolean	host_initialized;		// true if into command execution

double		host_frametime;
double		host_time;
double		realtime;				// without any filtering or bounding
double		oldrealtime;			// last frame run
int			host_framecount;

int			host_hunklevel;

int			minimum_memory;

client_t	*host_client;			// current client

jmp_buf 	host_abortserver;

byte		*host_basepal = NULL; // set to null

cvar_t	host_framerate = {"host_framerate","0"};	// set for slow motion
cvar_t	host_timescale = {"host_timescale","0"};	// more sensitivity slow motion
cvar_t	host_speeds = {"host_speeds","0"};			// set for running times
cvar_t	host_maxfps = {"host_maxfps", "72"};

cvar_t	sys_ticrate = {"sys_ticrate","0.05"};
cvar_t	serverprofile = {"serverprofile","0"};

cvar_t	fraglimit = {"fraglimit","0",false,true};
cvar_t	timelimit = {"timelimit","0",false,true};
cvar_t	teamplay = {"teamplay","0",false,true};

cvar_t	samelevel = {"samelevel","0"};
cvar_t	noexit = {"noexit","0",false,true};

cvar_t	developer = {"developer","0"};	// should be 0 for release!

cvar_t	skill = {"skill","1"};						// 0 - 3
cvar_t	deathmatch = {"deathmatch","0"};			// 0, 1, or 2
cvar_t	coop = {"coop","0"};			// 0 or 1

cvar_t	pausable = {"pausable","1"};

cvar_t	temp1 = {"temp1","0"};

cvar_t	cutscene = {"cutscene", "1"}; // Nehahra

/*
================
Host_EndGame
================
*/
void Host_EndGame (char *message, ...)
{
	va_list		argptr;
	char		string[MAX_PRINTMSG]; // was 1024 

	va_start (argptr, message);
	vsnprintf (string, sizeof(string), message, argptr);
	va_end (argptr);
	Con_DPrintf ("Host_EndGame: %s\n", string);

	if (sv.active)
		Host_ShutdownServer (false);

	if (cls.state == ca_dedicated)
		Sys_Error ("Host_EndGame: %s",string);	// dedicated servers exit

	if (cls.demonum != -1)
		CL_NextDemo ();
	else
		CL_Disconnect ();

	longjmp (host_abortserver, 1);
}

/*
================
Host_Error

This shuts down both the client and server
================
*/
void Host_Error (char *error, ...)
{
	va_list		argptr;
	char		 string[MAX_PRINTMSG]; // was 1024 
	static	qboolean inerror = false;

	if (inerror)
		Sys_Error ("Host_Error: recursively entered");
	inerror = true;

	SCR_EndLoadingPlaque ();		// reenable screen updates

	va_start (argptr, error);
	vsnprintf (string, sizeof(string), error, argptr);
	va_end (argptr);

	Con_Printf ("Host Error: %s\n", string);

	if (sv.active)
		Host_ShutdownServer (false);

	if (cls.state == ca_dedicated)
		Sys_Error ("Host_Error: %s", string);	// dedicated servers exit

	CL_Disconnect ();
	cls.demonum = -1;
	cl.intermission = 0; // for errors during intermissions (changelevel with no map found, etc.)

	inerror = false;

	longjmp (host_abortserver, 1);
}

/*
================
Host_FindMaxClients
================
*/
void	Host_FindMaxClients (void)
{
	int		i;

	svs.maxclients = 1;

	i = COM_CheckParm ("-dedicated");
	if (i)
	{
		cls.state = ca_dedicated;
		if (i != (com_argc - 1))
			svs.maxclients = atoi (com_argv[i+1]);
		else
			svs.maxclients = 8;
	}
	else
		cls.state = ca_disconnected;

	i = COM_CheckParm ("-listen");
	if (i)
	{
		if (cls.state == ca_dedicated)
			Sys_Error ("Only one of -dedicated or -listen can be specified");
		if (i != (com_argc - 1))
			svs.maxclients = atoi (com_argv[i+1]);
		else
			svs.maxclients = 8;
	}

	if (svs.maxclients < 1)
		svs.maxclients = 8;
	else if (svs.maxclients > MAX_SCOREBOARD)
		svs.maxclients = MAX_SCOREBOARD;

	svs.maxclientslimit = svs.maxclients;
	if (svs.maxclientslimit < 4)
		svs.maxclientslimit = 4;

	svs.clients = Hunk_AllocName (svs.maxclientslimit*sizeof(client_t), "clients");

	if (svs.maxclients > 1)
		Cvar_SetValue ("deathmatch", 1.0);
	else
		Cvar_SetValue ("deathmatch", 0.0);
}

/*
===============
Host_SaveConfig_f
===============
*/
void Host_SaveConfig_f (void)
{

	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("saveconfig <savename> : save a config file\n");
		return;
	}

	if (strstr(Cmd_Argv(1), ".."))
	{
		Con_Printf ("Relative pathnames are not allowed.\n");
		return;
	}

	Host_WriteConfiguration (Cmd_Argv(1));
}


/*
=======================
Host_InitLocal
======================
*/
void Host_InitLocal (void)
{
	int		i;

	Host_InitFileList ();
	Host_InitCommands ();

	Cmd_AddCommand ("saveconfig", Host_SaveConfig_f);

	Cvar_RegisterVariable (&host_framerate, NULL);
	Cvar_RegisterVariable (&host_timescale, NULL);
	Cvar_RegisterVariable (&host_speeds, NULL);
	Cvar_RegisterVariable (&host_maxfps, NULL);

	Cvar_RegisterVariable (&sys_ticrate, NULL);
	Cvar_RegisterVariable (&serverprofile, NULL);

	Cvar_RegisterVariable (&fraglimit, NULL);
	Cvar_RegisterVariable (&timelimit, NULL);
	Cvar_RegisterVariable (&teamplay, NULL);
	Cvar_RegisterVariable (&samelevel, NULL);
	Cvar_RegisterVariable (&noexit, NULL);
	Cvar_RegisterVariable (&skill, NULL);
	Cvar_RegisterVariable (&deathmatch, NULL);
	Cvar_RegisterVariable (&coop, NULL);

	Cvar_RegisterVariable (&pausable, NULL);

	Cvar_RegisterVariable (&developer, NULL);
	i = COM_CheckParm ("-developer");
	if (i)
	{
		if (i != (com_argc - 1))
			Cvar_SetValue ("developer", atoi(com_argv[i+1]));
		else
			Cvar_SetValue ("developer", 1);
	}

	Cvar_RegisterVariable (&temp1, NULL);

	Cvar_RegisterVariable (&cutscene, NULL); // Nehahra

	Host_FindMaxClients ();

	host_time = 1.0;		// so a think at time 0 won't get called
}


/*
===============
Host_WriteConfiguration

Writes key bindings and archived cvars to config.cfg
===============
*/
void Host_WriteConfiguration (char *configname)
{
	FILE	*f;

// dedicated servers initialize the host but don't parse and set the
// config.cfg cvars
	if (host_initialized && cls.state != ca_dedicated && !host_parms->errstate)
	{
		f = fopen (va("%s/%s",com_gamedir, configname), "w");
		if (!f)
		{
			Con_Printf ("Couldn't write %s.\n", configname);
			return;
		}

		Key_WriteBindings (f);
		Cvar_WriteVariables (f);

		if (in_mlook.state & 1)		//if mlook was down, keep it that way
			fprintf (f, "+mlook\n");

		fclose (f);
	}
}


/*
=================
SV_ClientPrintf

Sends text across to be displayed 
FIXME: make this just a stuffed echo?
=================
*/
void SV_ClientPrintf (char *fmt, ...)
{
	va_list		argptr;
	char		string[MAX_PRINTMSG]; // was 1024 
	
	va_start (argptr, fmt);
	vsnprintf (string, sizeof(string), fmt, argptr);
	va_end (argptr);
	
	MSG_WriteByte (&host_client->message, svc_print);
	MSG_WriteString (&host_client->message, string);
}

/*
=================
SV_BroadcastPrintf

Sends text to all active clients
=================
*/
void SV_BroadcastPrintf (char *fmt, ...)
{
	va_list		argptr;
	char		string[MAX_PRINTMSG]; //was 1024 
	int			i;
	
	va_start (argptr, fmt);
	vsnprintf (string, sizeof(string), fmt, argptr);
	va_end (argptr);
	
	for (i=0 ; i<svs.maxclients ; i++)
	{
		if (svs.clients[i].active && svs.clients[i].spawned)
		{
			MSG_WriteByte (&svs.clients[i].message, svc_print);
			MSG_WriteString (&svs.clients[i].message, string);
		}
	}
}

/*
=================
Host_ClientCommands

Send text over to the client to be executed
=================
*/
void Host_ClientCommands (char *fmt, ...)
{
	va_list		argptr;
	char		string[MAX_PRINTMSG]; // was 1024 
	
	va_start (argptr, fmt);
	vsnprintf (string, sizeof(string), fmt, argptr);
	va_end (argptr);
	
	MSG_WriteByte (&host_client->message, svc_stufftext);
	MSG_WriteString (&host_client->message, string);
}

/*
=====================
SV_DropClient

Called when the player is getting totally kicked off the host
if (crash = true), don't bother sending signofs
=====================
*/
void SV_DropClient (qboolean crash)
{
	int		saveSelf;
	int		i;
	client_t *client;

	// ProQuake: - don't drop a client that's already been dropped!
	if (!host_client->active)
		return;

	if (!crash)
	{
		// send any final messages (don't check for errors)
		if (NET_CanSendMessage (host_client->netconnection))
		{
			MSG_WriteByte (&host_client->message, svc_disconnect);
			NET_SendMessage (host_client->netconnection, &host_client->message);
		}
	
		if (host_client->edict && host_client->spawned)
		{
		// call the prog function for removing a client
		// this will set the body to a dead frame, among other things
			saveSelf = pr_global_struct->self;
			pr_global_struct->self = EDICT_TO_PROG(host_client->edict);
			PR_ExecuteProgram (pr_global_struct->ClientDisconnect);
			pr_global_struct->self = saveSelf;
		}

		Sys_Printf ("Client %s disconnected\n",host_client->name);
	}

// break the net connection
	NET_Close (host_client->netconnection);
	host_client->netconnection = NULL;

// free the client (the body stays around)
	host_client->active = false;
	host_client->name[0] = 0;
	host_client->old_frags = -999999;
	net_activeconnections--;

// send notification to all clients
	for (i=0, client = svs.clients ; i<svs.maxclients ; i++, client++)
	{
		if (!client->active)
			continue;
		MSG_WriteByte (&client->message, svc_updatename);
		MSG_WriteByte (&client->message, host_client - svs.clients);
		MSG_WriteString (&client->message, "");
		MSG_WriteByte (&client->message, svc_updatefrags);
		MSG_WriteByte (&client->message, host_client - svs.clients);
		MSG_WriteShort (&client->message, 0);
		MSG_WriteByte (&client->message, svc_updatecolors);
		MSG_WriteByte (&client->message, host_client - svs.clients);
		MSG_WriteByte (&client->message, 0);
	}
}

/*
==================
Host_ShutdownServer

This only happens at the end of a game, not between levels
==================
*/
void Host_ShutdownServer (qboolean crash)
{
	int		i;
	int		count;
	sizebuf_t	buf;
	char		message[4];
	double	start;

	if (!sv.active)
		return;

	sv.active = false;

// stop all client sounds immediately
	if (cls.state == ca_connected)
		CL_Disconnect ();

// flush any pending messages - like the score!!!
	start = Sys_DoubleTime();
	do
	{
		count = 0;
		for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
		{
			if (host_client->active && host_client->message.cursize)
			{
				if (NET_CanSendMessage (host_client->netconnection))
				{
					NET_SendMessage(host_client->netconnection, &host_client->message);
					SZ_Clear (&host_client->message);
				}
				else
				{
					NET_GetMessage(host_client->netconnection);
					count++;
				}
			}
		}
		if ((Sys_DoubleTime() - start) > 3.0)
			break;
	}
	while (count);

// make sure all the clients know we're disconnecting
	buf.data = (byte *)message;
	buf.maxsize = 4;
	buf.cursize = 0;

	MSG_WriteByte(&buf, svc_disconnect);
	count = NET_SendToAll(&buf, 5, false);
	if (count)
		Con_Printf("Host_ShutdownServer: NET_SendToAll failed for %u clients\n", count);

	for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
		if (host_client->active)
			SV_DropClient(crash);

//
// clear structures
//
	memset (&sv, 0, sizeof(sv));
	memset (svs.clients, 0, svs.maxclientslimit*sizeof(client_t));
}


/*
================
Host_ClearMemory

This clears all the memory used by both the client and server, but does
not reinitialize anything.
================
*/
void Host_ClearMemory (void)
{
	Con_DPrintf ("Clearing memory\n");
	Mod_ClearAll ();
	if (host_hunklevel)
		Hunk_FreeToLowMark (host_hunklevel);

	cls.signon = 0;
	memset (&sv, 0, sizeof(sv));
	memset (&cl, 0, sizeof(cl));
}


//============================================================================


/*
===================
Host_FilterTime

Returns false if the time is too short to run a frame
===================
*/
qboolean Host_FilterTime (float time)
{
	float maxfps;

	realtime += time;

	maxfps = CLAMP (1.0, host_maxfps.value, 1000.0);

	if (!cls.timedemo && realtime - oldrealtime < 1.0 / maxfps)
		return false;	// framerate is too high

	host_frametime = realtime - oldrealtime;
	oldrealtime = realtime;

	// host_timescale is more intuitive than host_framerate
	if (host_timescale.value > 0)
		host_frametime *= CLAMP (1.0, host_timescale.value, 1000.0);
	else if (host_framerate.value > 0)
		host_frametime = CLAMP (1.0, host_framerate.value, 1000.0);
	else // don't allow really long or short frames
		host_frametime = CLAMP (0.001, host_frametime, 0.1); // use CLAMP
	
	return true;
}


/*
===================
Host_GetConsoleCommands

Add them exactly as if they had been typed at the console
===================
*/
void Host_GetConsoleCommands (void)
{
	char	*cmd;

	while (1)
	{
		cmd = Sys_ConsoleInput ();
		if (!cmd)
			break;
		Cbuf_AddText (cmd);
	}
}


/*
==================
Host_ServerFrame
==================
*/
void Host_ServerFrame (void)
{
// run the world state	
	pr_global_struct->frametime = host_frametime;

// set the time and clear the general datagram
	SV_ClearDatagram ();

// check for new clients
	SV_CheckForNewClients ();

// read client messages
	SV_RunClients ();

// move things around and think
// always pause in single player if in console or menus
	if ( !(sv.paused || (svs.maxclients == 1 && key_dest != key_game) ) )
		SV_Physics ();

// send all messages to the clients
	SV_SendClientMessages ();
}


#define HOST_AVG 10

/*
==================
Host_Frame

Runs all active servers
==================
*/
void _Host_Frame (double time)
{
	static double		time1 = 0;
	static double		time2 = 0;
	static double		time3 = 0;
	static int	avg_index;

	if (setjmp (host_abortserver) )
		return;			// something bad happened, or the server disconnected

// keep the random time dependent
	rand ();
	
// decide the simulation time
	if (!Host_FilterTime (time))
		return;			// don't run too fast, or packets will flood out
		
	if (!host_speeds.value)
		time1 = time2 = time3 = avg_index = 0; // Reset

// get new events from environment
	IN_ProcessEvents ();

// allow other external controllers to add commands
	IN_Commands ();

// process console commands
	Cbuf_Execute ();

	if (host_speeds.value && !time3)
		time3 = Sys_DoubleTime (); // No previous time3

	NET_Poll();

// if running the server locally, make intentions now
	if (sv.active)
		CL_SendCmd ();
	else // hack from baker to allow console scrolling by dinput mouse wheel when con_forcedup
	if (con_forcedup && (key_dest == key_game || key_dest == key_console))
		IN_MouseWheel (); // Grab mouse wheel input for DirectInput

//-------------------
//
// server operations
//
//-------------------

// check for commands typed to the host
	Host_GetConsoleCommands ();
	
	if (sv.active)
		Host_ServerFrame ();

//-------------------
//
// client operations
//
//-------------------

// if running the server remotely, send intentions now after
// the incoming messages have been read
	if (!sv.active)
		CL_SendCmd ();

	host_time += host_frametime;

// fetch results from server
	if (cls.state == ca_connected)
	{
		CL_ReadFromServer ();
	}

// run particle logic seperated from rendering
	if (!sv.frozen)
		R_UpdateParticles ();

// update video
	if (host_speeds.value)
		time1 = Sys_DoubleTime ();
		
	SCR_UpdateScreen ();

	if (host_speeds.value)
		time2 = Sys_DoubleTime ();
		
// update audio
	if (cls.signon == SIGNONS)
	{
		S_Update (r_origin, vpn, vright, vup);
		if (!sv.frozen)
			CL_DecayLights ();
	}
	else
		S_Update (vec3_origin, vec3_origin, vec3_origin, vec3_origin);
	
	CDAudio_Update();

	if (host_speeds.value)
	{
		static float pass1[HOST_AVG + 1], pass2[HOST_AVG + 1], pass3[HOST_AVG + 1];

		if (host_speeds.value < 2 || avg_index > HOST_AVG)
			avg_index = 0;

		pass1[avg_index] = (time1 - time3)*1000;
		time3 = Sys_DoubleTime ();
		pass2[avg_index] = (time2 - time1)*1000;
		pass3[avg_index] = (time3 - time2)*1000;

		if (avg_index == HOST_AVG - 1)
		{
			int i;

			// Calculate average
			pass1[HOST_AVG] = pass2[HOST_AVG] = pass3[HOST_AVG] = 0;

			for (i = 0; i < HOST_AVG; ++i)
			{
				pass1[HOST_AVG] += pass1[i];
				pass2[HOST_AVG] += pass2[i];
				pass3[HOST_AVG] += pass3[i];
			}

			pass1[HOST_AVG] /= HOST_AVG;
			pass2[HOST_AVG] /= HOST_AVG;
			pass3[HOST_AVG] /= HOST_AVG;

			++avg_index;
		}

		if (host_speeds.value < 2 || avg_index == HOST_AVG)
			Con_Printf ("%3.0f tot %3.0f server %3.0f gfx %3.0f snd\n",
				    pass1[avg_index] + pass2[avg_index] + pass3[avg_index], pass1[avg_index], pass2[avg_index], pass3[avg_index]);

		if (host_speeds.value > 1)
			++avg_index;
	}
	
	host_framecount++;
}

void Host_Frame (double time)
{
	double	time1, time2;
	static double	timetotal;
	static int		timecount;
	int		i, c, m;

	if (!serverprofile.value)
	{
		_Host_Frame (time);
		return;
	}
	
	time1 = Sys_DoubleTime ();
	_Host_Frame (time);
	time2 = Sys_DoubleTime ();	
	
	timetotal += time2 - time1;
	timecount++;
	
	if (timecount < 1000)
		return;

	m = timetotal*1000/timecount;
	timecount = 0;
	timetotal = 0;
	c = 0;
	for (i=0 ; i<svs.maxclients ; i++)
	{
		if (svs.clients[i].active)
			c++;
	}

	Con_Printf ("serverprofile: %2i clients %2i msec\n",  c,  m);
}


/*
====================
Host_Init
====================
*/
//void Host_Init (quakeparms_t *parms)
void Host_Init (void)
{
	if (standard_quake)
		minimum_memory = MINIMUM_MEMORY;
	else
		minimum_memory = MINIMUM_MEMORY_LEVELPAK;

	if (COM_CheckParm ("-minmemory"))
		host_parms->memsize = minimum_memory;

//	host_parms = *parms;

	if (host_parms->memsize < minimum_memory)
		Sys_Error ("Only %4.1f megs of memory available, can't execute game", host_parms->memsize / (float)0x100000);

	com_argc = host_parms->argc;
	com_argv = host_parms->argv;

	Memory_Init (host_parms->membase, host_parms->memsize);
	Cbuf_Init ();
	Cmd_Init ();
	Cvar_Init ();
	V_Init ();
	Chase_Init ();
	COM_Init ();
	LOG_Init ();
	Host_InitLocal ();
	W_LoadWadFile ();
	History_Init ();
	Key_Init ();
	Con_Init ();

	Con_Printf ("Compiled: "__TIME__" "__DATE__"\n");
	Con_Printf ("%4.1f megabyte heap\n", host_parms->memsize / (1024*1024.0));

	M_Init ();
	PR_Init ();
	Mod_Init ();
	NET_Init ();
	SV_Init ();

	R_InitTextures ();		// needed even for dedicated servers
	R_LoadPalette ();
 
	if (cls.state != ca_dedicated)
	{
		VID_Init ();
		Draw_Init ();
		SCR_Init ();
		R_Init ();
		S_Init ();
		CDAudio_Init ();
		Sbar_Init ();
		CL_Init ();
		IN_Init ();
	}

	Cbuf_InsertText ("exec quake.rc\n");

	Hunk_AllocName (0, "-HOST_HUNKLEVEL-");
	host_hunklevel = Hunk_LowMark ();

	host_initialized = true;
	Con_Printf ("Host initialized\n");

	if (cls.state == ca_dedicated)
		Con_Printf ("\n****** fxQuake initialized ******\n\n");
	else
		Con_Printf ("\n\x1d\x1e\x1e\x1e\x1e\x1f fxQuake initialized \x1d\x1e\x1e\x1e\x1e\x1f\n\n");

	if (cls.state == ca_dedicated)
	{
		if (!sv.active)
			Cbuf_AddText ("map start\n");
	}
}


/*
===============
Host_Shutdown

FIXME: this is a callback from Sys_Quit and Sys_Error.  It would be better
to run quit through here before the final handoff to the sys code.
===============
*/
void Host_Shutdown(void)
{
	static qboolean isdown = false;

	if (isdown)
	{
		printf ("recursive shutdown\n");
		return;
	}
	isdown = true;

// keep Con_Printf from trying to update the screen
	scr_disabled_for_loading = true;

	Host_WriteConfiguration ("config.cfg"); 
	History_Shutdown ();
	NET_Shutdown ();

	if (cls.state != ca_dedicated)
	{
		CDAudio_Shutdown ();
		S_Shutdown ();
		IN_Shutdown ();
		VID_Shutdown ();
	}

	LOG_Close ();
    
    host_initialized = false;
}

