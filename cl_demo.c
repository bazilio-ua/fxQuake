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
// cl_demo.c

#include "quakedef.h"

static long demofile_len, demofile_start;
int	stufftext_frame;

void CL_FinishTimeDemo (void);

/*
==============================================================================

DEMO CODE

When a demo is playing back, all NET_SendMessages are skipped, and
NET_GetMessages are read from the demo file.

Whenever cl.time gets past the last received message, another message is
read from the demo file.
==============================================================================
*/

// From ProQuake: space to fill out the demo header for record at any time
// support for recording demos after connecting to the server
byte	demo_head[3][MAX_MSGLEN];
int		demo_head_size[2];

/*
====================
CL_CloseDemoFile
====================
*/
void CL_CloseDemoFile (void)
{
	fclose (cls.demofile);
	cls.demofile = NULL;
}

/*
=====================
CL_NextDemo

Called to play the next demo in the demo loop
=====================
*/
void CL_NextDemo (void)
{
	char	str[1024];

	if (cls.demonum == -1)
		return;		// don't play demos

// Baker change (moved below)
//	SCR_BeginLoadingPlaque ();

	if (!cls.demos[cls.demonum][0] || cls.demonum == MAX_DEMOS)
	{
		cls.demonum = 0;
		if (!cls.demos[cls.demonum][0])
		{
			Con_Printf ("No demos listed with startdemos\n");
			cls.demonum = -1;
			CL_Disconnect ();
			return;
		}
	}

// Baker change (moved to AFTER we know demo will play)
	SCR_BeginLoadingPlaque ();

	sprintf (str,"playdemo %s\n", cls.demos[cls.demonum]);
	Cbuf_InsertText (str);
	cls.demonum++;
}

/*
==============
CL_StopPlayback

Called when a demo file runs out, or the user starts a game
==============
*/
void CL_StopPlayback (void)
{
	if (!cls.demoplayback)
		return;

	cls.demoplayback = false;
	CL_CloseDemoFile ();
	cls.state = ca_disconnected;
	
	// Make sure screen is updated shortly after this
	SCR_SetTimeout (0);

	if (cls.timedemo)
		CL_FinishTimeDemo ();
}

/*
====================
CL_WriteDemoMessage

Dumps the current net message, prefixed by the length and view angles
====================
*/
void CL_WriteDemoMessage (void)
{
	int	    len;
	int	    i;
	float	    f;

	len = LittleLong (net_message->message->cursize);
	fwrite (&len, 4, 1, cls.demofile);
	for (i=0 ; i<3 ; i++)
	{
		f = LittleFloat (cl.viewangles[i]);
		fwrite (&f, 4, 1, cls.demofile);
	}
	fwrite (net_message->message->data, net_message->message->cursize, 1, cls.demofile);
	fflush (cls.demofile);
}

/*
====================
CL_GetMessage

Handles recording and playback of demos, on top of NET_ code
====================
*/
int CL_GetMessage (void)
{
	int		r, i;
	float	f;

	if (cl.paused & 2)
		return 0;
	
	if (cls.demoplayback)
	{
	// decide if it is time to grab the next message		
		if (cls.signon == SIGNONS)	// always grab until fully connected
		{
			// Pa3PyX: Always wait for full frame update on stuff messages.
			// If the server stuffs a reconnect, we must wait for
			// the client to re-initialize before accepting further
			// messages. Otherwise demo playback may freeze.
			if (stufftext_frame == host_framecount)
				return 0;

			if (cls.timedemo)
			{
				if (host_framecount == cls.td_lastframe)
					return 0;		// already read this frame's message
				cls.td_lastframe = host_framecount;
			// if this is the second frame, grab the real td_starttime
			// so the bogus time on the first frame doesn't count
				if (host_framecount == cls.td_startframe + 1)
					cls.td_starttime = realtime;
			}
			else if ( /* cl.time > 0 && */ cl.time <= cl.mtime[0])
			{
					return 0;		// don't need another message yet
			}
		}

	// Detect EOF, especially for demos in pak files
		if (ftell(cls.demofile) - demofile_start >= demofile_len)
			Host_EndGame ("Missing disconnect in demofile\n");

	// get the next message
		fread (&net_message->message->cursize, 4, 1, cls.demofile);

		VectorCopy (cl.mviewangles[0], cl.mviewangles[1]);
		for (i=0 ; i<3 ; i++)
		{
			r = fread (&f, 4, 1, cls.demofile);
			cl.mviewangles[0][i] = LittleFloat (f);
		}

		net_message->message->cursize = LittleLong (net_message->message->cursize);
		if (net_message->message->cursize > MAX_MSGLEN)
			Host_Error ("Demo message %d > MAX_MSGLEN (%d)", net_message->message->cursize, MAX_MSGLEN);
		r = fread (net_message->message->data, net_message->message->cursize, 1, cls.demofile);
		if (r != 1)
		{
			CL_StopPlayback ();
			return 0;
		}

		return 1;
	}

	while (1)
	{
		r = NET_GetMessage (cls.netcon);
		
		if (r != 1 && r != 2)
			return r;
	
	// discard nop keepalive message
		if (net_message->message->cursize == 1 && net_message->message->data[0] == svc_nop)
			Con_Printf ("<-- server to client keepalive\n");
		else
			break;
	}

	if (cls.demorecording)
		CL_WriteDemoMessage ();

	// support for recording demos after connecting to the server
	if (cls.signon < 2)
	{
		memcpy(demo_head[cls.signon], net_message->message->data, net_message->message->cursize);
		demo_head_size[cls.signon] = net_message->message->cursize;

		if (!cls.signon)
		{
			char *ch = (char *) (demo_head[0] + demo_head_size[0]);

			*ch++ = svc_print;
			ch += 1 + sprintf (ch, "\nRecorded on fxQuake %4.2f\n\n", (float)VERSION);
			demo_head_size[0] = ch - (char *) demo_head[0];
		}
	}

	return r;
}

/*
====================
CL_Stop_f

stop recording a demo
====================
*/
void CL_Stop_f (void)
{
	if (cmd_source != src_command)
		return;

	if (!cls.demorecording)
	{
		Con_Printf ("Not recording a demo.\n");
		return;
	}

	// write a disconnect message to the demo file
	SZ_Clear (net_message->message);
	MSG_WriteByte (net_message->message, svc_disconnect);
	CL_WriteDemoMessage ();

	// finish up
	CL_CloseDemoFile ();

	cls.demorecording = false;
	Con_Printf ("Completed demo\n");
}

/*
====================
CL_Record_f

record <demoname> [<map> [cd track]]
====================
*/
void CL_Record_f (void)
{
	int		c;
	char	name[MAX_OSPATH];
	int		track;

	if (cmd_source != src_command)
		return;

	c = Cmd_Argc();
	if (c != 2 && c != 3 && c != 4)
	{
		Con_Printf ("record <demoname> [<map> [cd track]]\n");
		return;
	}

	if (strstr(Cmd_Argv(1), ".."))
	{
		Con_Printf ("Relative pathnames are not allowed.\n");
		return;
	}

	if (cls.demoplayback)
	{
		Con_Printf("Can't record during demo playback\n");
		return;
	}

	if (cls.demorecording)
	{
		Con_Printf("Can't record -- already recording\n");
		return;
	}

	if (c == 2 && cls.state == ca_connected && cls.signon < 2)
	{
		Con_Printf("Can't record -- try again when connected\n");
		return;
	}

// write the forced cd track number, or -1
	if (c == 4)
	{
		track = atoi(Cmd_Argv(3));
		Con_Printf ("Forcing CD track to %i\n", cls.forcetrack);
	}
	else
		track = -1;	

	sprintf (name, "%s/%s", com_gamedir, Cmd_Argv(1));

//
// start the map up
//
	if (c > 2)
		Cmd_ExecuteString ( va("map %s", Cmd_Argv(2)), src_command);

// Joe: if couldn't find the map, don't start recording
	if (cls.state != ca_connected)
	{
		Con_Printf("Can't record -- not connected\n");
		return;
	}

//
// open the demo file
//
	COM_DefaultExtension (name, ".dem");

	Con_Printf ("recording to %s.\n", name);
	cls.demofile = fopen (name, "wb");
	if (!cls.demofile)
	{
		Con_Error ("couldn't open.\n");
		return;
	}

	cls.forcetrack = track;
	fprintf (cls.demofile, "%i\n", cls.forcetrack);
	
	cls.demorecording = true;

// From ProQuake: initialize the demo file if we're already connected
	if (c == 2 && cls.state == ca_connected)
	{
		byte *data = net_message->message->data;
		int cursize = net_message->message->cursize;
		int i;

		for (i = 0 ; i < 2 ; i++)
		{
			net_message->message->data = demo_head[i];
			net_message->message->cursize = demo_head_size[i];
			CL_WriteDemoMessage();
		}

		net_message->message->data = demo_head[2];
		SZ_Clear (net_message->message);

	// current names, colors, and frag counts
		for (i=0 ; i < cl.maxclients ; i++)
		{
			MSG_WriteByte (net_message->message, svc_updatename);
			MSG_WriteByte (net_message->message, i);
			MSG_WriteString (net_message->message, cl.scores[i].name);
			MSG_WriteByte (net_message->message, svc_updatefrags);
			MSG_WriteByte (net_message->message, i);
			MSG_WriteShort (net_message->message, cl.scores[i].frags);
			MSG_WriteByte (net_message->message, svc_updatecolors);
			MSG_WriteByte (net_message->message, i);
			MSG_WriteByte (net_message->message, cl.scores[i].colors);
		}

	// send all current light styles
		for (i = 0 ; i < MAX_LIGHTSTYLES ; i++)
		{
			MSG_WriteByte (net_message->message, svc_lightstyle);
			MSG_WriteByte (net_message->message, i);
			MSG_WriteString (net_message->message, cl_lightstyle[i].map);
		}

	// what about the CD track or SVC fog ... future consideration.
		MSG_WriteByte (net_message->message, svc_updatestat);
		MSG_WriteByte (net_message->message, STAT_TOTALSECRETS);
		MSG_WriteLong (net_message->message, cl.stats[STAT_TOTALSECRETS]);

		MSG_WriteByte (net_message->message, svc_updatestat);
		MSG_WriteByte (net_message->message, STAT_TOTALMONSTERS);
		MSG_WriteLong (net_message->message, cl.stats[STAT_TOTALMONSTERS]);

		MSG_WriteByte (net_message->message, svc_updatestat);
		MSG_WriteByte (net_message->message, STAT_SECRETS);
		MSG_WriteLong (net_message->message, cl.stats[STAT_SECRETS]);

		MSG_WriteByte (net_message->message, svc_updatestat);
		MSG_WriteByte (net_message->message, STAT_MONSTERS);
		MSG_WriteLong (net_message->message, cl.stats[STAT_MONSTERS]);

	// view entity
		MSG_WriteByte (net_message->message, svc_setview);
		MSG_WriteShort (net_message->message, cl.viewentity);

	// signon
		MSG_WriteByte (net_message->message, svc_signonnum);
		MSG_WriteByte (net_message->message, 3);

		CL_WriteDemoMessage();

	// restore net_message
		net_message->message->data = data;
		net_message->message->cursize = cursize;
	}
}


/*
====================
CL_PlayDemo_f

playdemo [demoname]
====================
*/
void CL_PlayDemo_f (void)
{
	char	 name[MAX_OSPATH];
	int c;
	qboolean neg = false;

	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("playdemo <demoname> : plays a demo\n");
		return;
	}

//
// disconnect from server
//
	CL_Disconnect ();
	
//
// open the demo file
//
	strcpy (name, Cmd_Argv(1));
	COM_DefaultExtension (name, ".dem");

	Con_Printf ("Playing demo from %s.\n", name);
	demofile_len = COM_FOpenFile (name, &cls.demofile, NULL);
	if (!cls.demofile)
	{
		Con_Error ("couldn't open %s\n", name);
		cls.demonum = -1; // stop demo loop
		return;
	}
	demofile_start = ftell (cls.demofile);

// Viewing a demo. No reason to have console up.
	if (key_dest != key_game)
		key_dest = key_game;

	cls.demoplayback = true;
	cls.state = ca_connected;
	cls.forcetrack = 0;

	while ((c = getc(cls.demofile)) != '\n')
		if (c == '-')
			neg = true;
		else
			cls.forcetrack = cls.forcetrack * 10 + (c - '0');

	if (neg)
		cls.forcetrack = -cls.forcetrack;

// Pa3PyX: Get a new message on playback start.
// Moved from CL_TimeDemo_f to here.
	cls.td_lastframe = -1;		// get a new message this frame
}

/*
====================
CL_FinishTimeDemo

====================
*/
void CL_FinishTimeDemo (void)
{
	int		frames;
	float	time;
	
	cls.timedemo = false;
	
// the first frame didn't count
	frames = (host_framecount - cls.td_startframe) - 1;
	time = realtime - cls.td_starttime;
	if (!time)
		time = 1;
	Con_Printf ("%i frames %5.1f seconds %5.1f fps\n", frames, time, frames/time);
}

/*
====================
CL_TimeDemo_f

timedemo [demoname]
====================
*/
void CL_TimeDemo_f (void)
{
	if (cmd_source != src_command)
		return;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("timedemo <demoname> : gets demo speeds\n");
		return;
	}

	CL_PlayDemo_f ();
	
// don't trigger timedemo mode if playdemo fails
	if (!cls.demofile)
		return;

// cls.td_starttime will be grabbed at the second frame of the demo, so
// all the loading time doesn't get counted
	
	cls.timedemo = true;
	cls.td_startframe = host_framecount;
// Pa3PyX: Moved to CL_PlayDemo_f().
//	cls.td_lastframe = -1;		// get a new message this frame
}

