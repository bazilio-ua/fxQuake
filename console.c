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
// console.c

#ifndef _MSC_VER
#include <unistd.h>
#else
#include <io.h>
#endif
#include <fcntl.h>
#include <time.h>
#include "quakedef.h"

int 		con_linewidth;

float		con_cursorspeed = 4;

#define		CON_TEXTSIZE	0x40000 //16384

qboolean 	con_forcedup;		// because no entities to refresh

int			con_totallines;		// total lines in console scrollback
int			con_backscroll;		// lines up from bottom to display
int			con_current;		// where next message will be printed
int			con_x;				// offset in current line for next print
char		*con_text=0;

cvar_t		con_notifytime = {"con_notifytime","3"};		//seconds
cvar_t		con_logcenterprint = {"con_logcenterprint", "1"};	// log centerprints to console
cvar_t		con_removecr = {"con_removecr", "1"}; // remove \r from console output

char		con_lastcenterstring[MAX_PRINTMSG];

#define	NUM_CON_TIMES 5
float		con_times[NUM_CON_TIMES];	// realtime time the line was generated
								// for transparent notify lines

int			con_vislines;

qboolean	con_debuglog = false;

extern	char	key_lines[64][MAX_CMDLINE];
extern	int		edit_line;
extern	int		history_line;
extern	int		key_linepos;
extern	int		key_insert;

extern	char	chat_buffer[];

qboolean	con_initialized = false;

/*
================
Con_Quakebar -- returns a bar of the desired length, but never wider than the console

includes a newline, unless len >= con_linewidth.
================
*/
char *Con_Quakebar (int len)
{
	static char bar[42];
	int	    i;

	len = min(len, sizeof(bar) - 2);
	len = min(len, con_linewidth);

	bar[0] = '\35';
	for (i = 1; i < len - 1; i++)
		bar[i] = '\36';
	bar[len-1] = '\37';

	if (len < con_linewidth)
	{
		bar[len] = '\n';
		bar[len+1] = 0;
	}
	else
		bar[len] = 0;

	return bar;
}

/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f (void)
{
	if (key_dest == key_console)
	{
		if (cls.state == ca_connected)
		{
			key_dest = key_game;
			key_lines[edit_line][1] = 0;	// clear any typing
			key_linepos = 1;
			con_backscroll = 0; // toggleconsole should return you to the bottom of the scrollback
			history_line = edit_line; // it should also return you to the bottom of the command history
		}
		else
		{
			M_Menu_Main_f ();
		}
	}
	else
		key_dest = key_console;
	
	SCR_EndLoadingPlaque ();
	memset (con_times, 0, sizeof(con_times));
}

/*
================
Con_Clear_f
================
*/
void Con_Clear_f (void)
{
	if (con_text)
		memset (con_text, ' ', CON_TEXTSIZE);

	con_backscroll = 0; // if console is empty, being scrolled up is confusing
}

/*
================
Con_Dump_f

adapted from quake2 source
================
*/
void Con_Dump_f (void)
{
	int		l, x;
	char	*line;
	FILE	*f;
	char	buffer[1024];
	char	name[MAX_OSPATH];

	// there is a security risk in writing files with an arbitrary filename. so,
	// until stuffcmd is crippled to alleviate this risk, just force the default filename.
	sprintf (name, "%s/qcondump.log", com_gamedir);

	COM_CreatePath (name);
	f = fopen (name, "w");
	if (!f)
	{
		Con_Error ("couldn't open file.\n", name);
		return;
	}

	// skip initial empty lines
	for (l = con_current - con_totallines + 1 ; l <= con_current ; l++)
	{
		line = con_text + (l%con_totallines)*con_linewidth;
		for (x=0 ; x<con_linewidth ; x++)
			if (line[x] != ' ')
				break;
		if (x != con_linewidth)
			break;
	}

	// write the remaining lines
	buffer[con_linewidth] = 0;
	for ( ; l <= con_current ; l++)
	{
		line = con_text + (l%con_totallines)*con_linewidth;
		strncpy (buffer, line, con_linewidth);
		for (x=con_linewidth-1 ; x>=0 ; x--)
		{
			if (buffer[x] == ' ')
				buffer[x] = 0;
			else
				break;
		}
		for (x=0; buffer[x]; x++)
			buffer[x] &= 0x7f;

		fprintf (f, "%s\n", buffer);
	}

	fclose (f);
	Con_Printf ("Dumped console text to %s.\n", name);
}
						
/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify (void)
{
	int		i;
	
	for (i=0 ; i<NUM_CON_TIMES ; i++)
		con_times[i] = 0;
}

						
/*
================
Con_MessageMode_f
================
*/
extern qboolean team_message;

void Con_MessageMode_f (void)
{
	key_dest = key_message;
	team_message = false;
}

						
/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f (void)
{
	key_dest = key_message;
	team_message = true;
}

						
/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize (void)
{
	int	i, j, width, oldwidth, oldtotallines, numlines, numchars;
	char	tbuf[CON_TEXTSIZE];

	width = (vid.width >> 3) - 2;

	if (width == con_linewidth)
		return;

	if (width < 1)			// video hasn't been initialized yet
	{
		width = 38;
		con_linewidth = width;
		con_totallines = CON_TEXTSIZE / con_linewidth;
		memset (con_text, ' ', CON_TEXTSIZE);
	}
	else
	{
		oldwidth = con_linewidth;
		con_linewidth = width;
		oldtotallines = con_totallines;
		con_totallines = CON_TEXTSIZE / con_linewidth;
		numlines = oldtotallines;

		if (con_totallines < numlines)
			numlines = con_totallines;

		numchars = oldwidth;

		if (con_linewidth < numchars)
			numchars = con_linewidth;

		memcpy (tbuf, con_text, CON_TEXTSIZE);
		memset (con_text, ' ', CON_TEXTSIZE);

		for (i=0 ; i<numlines ; i++)
		{
			for (j=0 ; j<numchars ; j++)
			{
				con_text[(con_totallines - 1 - i) * con_linewidth + j] =
						tbuf[((con_current - i + oldtotallines) %
							  oldtotallines) * oldwidth + j];
			}
		}

		Con_ClearNotify ();
	}

	con_backscroll = 0;
	con_current = con_totallines - 1;
}


/*
================
Con_Init
================
*/
void Con_Init (void)
{
	con_text = Hunk_AllocName (CON_TEXTSIZE, "context");
	memset (con_text, ' ', CON_TEXTSIZE);
	con_linewidth = -1;
	Con_CheckResize ();

	con_initialized = true;
	Con_Printf ("Console initialized\n");

//
// register our commands
//
	Cvar_RegisterVariable (&con_notifytime, NULL);
	Cvar_RegisterVariable (&con_logcenterprint, NULL);
	Cvar_RegisterVariable (&con_removecr, NULL); // remove \r from console output

	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand ("messagemode", Con_MessageMode_f);
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand ("clear", Con_Clear_f);
	Cmd_AddCommand ("con_dump", Con_Dump_f);
}


/*
===============
Con_Linefeed
===============
*/
void Con_Linefeed (void)
{
	// improved scrolling
	if (con_backscroll)
		con_backscroll++;

	con_backscroll = CLAMP(0, con_backscroll, con_totallines - (int)(vid.height>>3) - 1);

	con_x = 0;
	con_current++;
	memset (&con_text[(con_current%con_totallines)*con_linewidth]
	, ' ', con_linewidth);
}

/*
================
Con_Print

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the notify window will pop up.
================
*/
void Con_Print (char *txt)
{
	int		y;
	int		c, l;
	static int	cr;
	int		mask;
	
//	con_backscroll = 0; // better console scrolling

	if (txt[0] == 1)
	{
		mask = 128;		// go to colored text
		S_LocalSound ("misc/talk.wav");
	// play talk wav
		txt++;
	}
	else if (txt[0] == 2)
	{
		mask = 128;		// go to colored text
		txt++;
	}
	else
		mask = 0;


	while ( (c = *txt) )
	{
	// count word length
		for (l=0 ; l< con_linewidth ; l++)
			if ( txt[l] <= ' ')
				break;

	// word wrap
		if (l != con_linewidth && (con_x + l > con_linewidth) )
			con_x = 0;

		txt++;

		if (cr)
		{
			con_current--;
			cr = false;
		}

		
		if (!con_x)
		{
			Con_Linefeed ();
		// mark time for transparent overlay
			if (con_current >= 0)
				con_times[con_current % NUM_CON_TIMES] = realtime;
		}

		switch (c)
		{
		case '\n':
			con_x = 0;
			break;

		case '\r':
			if (con_removecr.value)	// optionally remove '\r'
				c += 128;
			else
			{
				con_x = 0;
				cr = 1;
				break;
			}

		default:	// display character and advance
			y = con_current % con_totallines;
			con_text[y*con_linewidth+con_x] = c | mask;
			con_x++;
			if (con_x >= con_linewidth)
				con_x = 0;
			break;
		}
		
	}
}

/*
================
LOG_Init
================
*/
#define		LOG_FILE	"qconsole.log"
FILE	*logfile = NULL;

void LOG_Init (void)
{
	char	timestr[64];
	time_t	crt_time; 
	struct tm	*crt_tm; 
	char 	name[MAX_OSPATH];

	con_debuglog = COM_CheckParm("-condebug");

	if (!con_debuglog)
		return;

	// Build the time stamp (ex: "Sat Oct 25 05:16:32 2008");
	time (&crt_time);
	crt_tm = localtime (&crt_time);
	strftime (timestr, sizeof (timestr), "%a %b %d %H:%M:%S %Y", crt_tm);

	sprintf (name, "%s/%s", com_gamedir, LOG_FILE);
	logfile = fopen (name, "w"); 
	if (!logfile)
		Sys_Error ("Error opening %s: %s", name, strerror(errno));

	// Log current time to file
	Con_DebugLog (va("LOG started on: %s \n", timestr));
}

/*
================
LOG_Close
================
*/
void LOG_Close (void)
{
	if (!con_debuglog)
		return;

	fclose (logfile);
	logfile = NULL;
}

/*
================
Con_QuakeStr
================
*/
void Con_QuakeStr (byte str[])
{
	int  i;
	byte tchar;

	// Translate into simplified Quake character set
	for (i = 0; str[i] != '\0'; ++i)
	{
		tchar = str[i] & 0x7F;

		// Lower bits not CRLF ?
		if (tchar != 0x0A && tchar != 0x0D)
		{
			if (str[i] != 0x80)
				str[i] = tchar; // Ignore colour bit unless result becomes NUL

			if (str[i] < 0x1D)
				str[i] = 0x1D; // Filter colour triggers, control chars etc
		}
	}
}

/*
================
Con_DebugLog
================
*/
void Con_DebugLog (char *fmt, ...)
{
	va_list argptr; 
	static char data[MAX_PRINTMSG]; // was 1024

	va_start (argptr, fmt);
	vsnprintf (data, sizeof(data), fmt, argptr);
	va_end (argptr);

	Con_QuakeStr ((byte *)data);

	fprintf (logfile, "%s", data);
	fflush (logfile);		// force it to save every time
}


/*
================
Con_Printf

Handles cursor positioning, line wrapping, etc
================
*/
void Con_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAX_PRINTMSG];
	static qboolean	inupdate;
	
	va_start (argptr, fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);
	
// also echo to debugging console
	Sys_Printf ("%s", msg);

// log all messages to file
	if (con_debuglog)
		Con_DebugLog ("%s", msg);

	if (!con_initialized)
		return;
		
	if (cls.state == ca_dedicated)
		return;		// no graphics mode

// write it to the scrollable buffer
	Con_Print (msg);
	
// update the screen if the console is displayed
	if (cls.signon != SIGNONS && !scr_disabled_for_loading )
	{
	// protect against infinite loop if something in SCR_UpdateScreen calls
	// Con_Printd
		if (!inupdate)
		{
			inupdate = true;
			SCR_UpdateScreen ();
			inupdate = false;
		}
	}
}

/*
================
Con_Error

prints a error to the console
================
*/
void Con_Error (char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAX_PRINTMSG];

	va_start (argptr, fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	Con_SafePrintf ("\002Error: ");
	Con_SafePrintf ("%s", msg);
}

/*
================
Con_Warning

prints a warning to the console
================
*/
void Con_Warning (char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAX_PRINTMSG];

	va_start (argptr, fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	Con_SafePrintf ("\x02Warning: ");
	Con_SafePrintf ("%s", msg);
}

/*
================
Con_DWarning

prints a warning to the console (special "developer" case)
================
*/
void Con_DWarning (char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAX_PRINTMSG];

	if (!developer.value)
		return;			// don't confuse non-developers with techie stuff...

	va_start (argptr, fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	Con_SafePrintf ("%sDWarning: ", "\x02");
	Con_SafePrintf ("%s", msg);
}


/*
================
Con_DPrintf

A Con_Printf that only shows up if the "developer" cvar is set
================
*/
void Con_DPrintf (char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAX_PRINTMSG];

	if (!developer.value)
		return;			// don't confuse non-developers with techie stuff...

	va_start (argptr, fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	Con_SafePrintf ("%s", msg);
}


/*
==================
Con_SafePrintf

Okay to call even when the screen can't be updated
==================
*/
void Con_SafePrintf (char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAX_PRINTMSG]; // was 1024
	int			temp;
		
	va_start (argptr, fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;
	Con_Printf ("%s", msg);
	scr_disabled_for_loading = temp;
}

/*
================
Con_CenterPrintf

pad each line with spaces to make it appear centered
================
*/
void Con_CenterPrintf (int linewidth, char *fmt, ...)
{
	va_list	argptr;
	char	msg[MAX_PRINTMSG]; //the original message
	char	line[MAX_PRINTMSG]; //one line from the message
	char	spaces[21]; //buffer for spaces
	char	*src, *dst;
	int	len, s;

	va_start (argptr, fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	linewidth = min (linewidth, con_linewidth);
	for (src = msg; *src; )
	{
		dst = line;
		while (*src && *src != '\n')
			*dst++ = *src++;
		*dst = 0;
		if (*src == '\n')
			src++;

		len = strlen(line);
		if (len < linewidth)
		{
			s = min ((linewidth - len) / 2, sizeof(spaces) - 1);
			memset (spaces, ' ', s);
			spaces[s] = 0;
			Con_Printf ("%s%s\n", spaces, line);
		}
		else
			Con_Printf ("%s\n", line);
	}
}

/*
==================
Con_LogCenterPrint

echo centerprint message to the console
==================
*/
void Con_LogCenterPrint (char *str)
{
	if (!strcmp(str, con_lastcenterstring))
		return; //ignore duplicates

	if (cl.gametype == GAME_DEATHMATCH && con_logcenterprint.value != 2)
		return; //don't log in deathmatch

	strcpy(con_lastcenterstring, str);

	if (con_logcenterprint.value)
	{
		Con_Printf (Con_Quakebar(40));
		Con_CenterPrintf (40, "%s\n", str);
		Con_Printf (Con_Quakebar(40));
		Con_ClearNotify ();
	}
}

/*
==============================================================================

	TAB COMPLETION

==============================================================================
*/

// tab completion stuff
// unique defs
char key_tabpartial[MAX_CMDLINE];
typedef struct tab_s
{
	char			*name;
	char			*type;
	struct tab_s	*next;
	struct tab_s	*prev;
} tab_t;
tab_t	*tablist;

// defs from elsewhere
extern qboolean	keydown[256];
typedef struct cmd_function_s
{
	struct cmd_function_s	*next;
	char					*name;
	xcommand_t				function;
} cmd_function_t;
extern	cmd_function_t	*cmd_functions;
#define	MAX_ALIAS_NAME	32
typedef struct cmdalias_s
{
	struct cmdalias_s	*next;
	char	name[MAX_ALIAS_NAME];
	char	*value;
} cmdalias_t;
extern	cmdalias_t	*cmd_alias;

/*
============
AddToTabList

tablist is a doubly-linked loop, alphabetized by name
============
*/
void AddToTabList (char *name, char *type)
{
	tab_t	*t, *insert;

	t = Hunk_AllocName (sizeof(tab_t), "tablist");
	t->name = name;
	t->type = type;

	if (!tablist) // list empty
	{
		// create list
		tablist = t;
		t->next = t;
		t->prev = t;
	}
	else if (strcmp(name, tablist->name) < 0) // list not empty
	{
		// insert at front
		t->next = tablist;
		t->prev = tablist->prev;
		t->next->prev = t;
		t->prev->next = t;
		tablist = t;
	}
	else
	{
		// insert later
		insert = tablist;
		do
		{
			if (strcmp(name, insert->name) < 0)
				break;
			insert = insert->next;
		} while (insert != tablist);

		t->next = insert;
		t->prev = insert->prev;
		t->next->prev = t;
		t->prev->next = t;
	}
}

/*
============
BuildTabList
============
*/
void BuildTabList (char *partial)
{
	cmdalias_t		*alias;
	cvar_t			*cvar;
	cmd_function_t	*cmd;
	int				len;

	tablist = NULL;
	len = strlen(partial);

	for (cvar=cvar_vars ; cvar ; cvar=cvar->next)
		if (!strncmp (partial, cvar->name, len))
			AddToTabList (cvar->name, "cvar");

	for (cmd=cmd_functions ; cmd ; cmd=cmd->next)
		if (!strncmp (partial,cmd->name, len))
			AddToTabList (cmd->name, "command");

	for (alias=cmd_alias ; alias ; alias=alias->next)
		if (!strncmp (partial, alias->name, len))
			AddToTabList (alias->name, "alias");
}

/*
============
Con_TabComplete
============
*/
void Con_TabComplete (void)
{
	char		partial[MAX_CMDLINE];
	char		*match;
	static char	*c;
	tab_t		*t;
	int			mark, i;

// if editline is empty, return
	if (key_lines[edit_line][1] == 0)
		return;

// get partial string (space -> cursor)
	if (!strlen(key_tabpartial)) // first time through, find new insert point. (otherwise, use previous.)
	{
		// work back from cursor until you find a space, quote, semicolon, or prompt
		c = key_lines[edit_line] + key_linepos - 1; // start one space left of cursor
		while (*c!=' ' && *c!='\"' && *c!=';' && c!=key_lines[edit_line])
			c--;
		c++; // start 1 char after the seperator we just found
	}
	for (i = 0; c + i < key_lines[edit_line] + key_linepos; i++)
		partial[i] = c[i];
	partial[i] = 0;

// if partial is empty, return
	if (partial[0] == 0)
		return;

// trim trailing space becuase it screws up string comparisons
	if (i > 0 && partial[i-1] == ' ')
		partial[i-1] = 0;

// find a match
	mark = Hunk_LowMark();
	if (!strlen(key_tabpartial)) // first time through
	{
		strcpy (key_tabpartial, partial);
		BuildTabList (key_tabpartial);

		if (!tablist)
			return;

		// print list
		t = tablist;
		do
		{
			Con_SafePrintf("   %s (%s)\n", t->name, t->type);
			t = t->next;
		} while (t != tablist);

		// get first match
		match = tablist->name;
	}
	else
	{
		BuildTabList (key_tabpartial);

		if (!tablist)
			return;

		// find current match -- can't save a pointer because the list will be rebuilt each time
		t = tablist;
		do
		{
			if (!strcmp(t->name, partial))
				break;
			t = t->next;
		} while (t != tablist);

		// use prev or next to find next match
		match = keydown[K_SHIFT] ? t->prev->name : t->next->name;
	}
	Hunk_FreeToLowMark(mark); // it's okay to free it here because match is a pointer to persistent data

// insert new match into edit line
	strcpy (partial, match); // first copy match string
	strcat (partial, key_lines[edit_line] + key_linepos); // then add chars after cursor
	strcpy (c, partial); // now copy all of this into edit line
	key_linepos = c - key_lines[edit_line] + strlen(match); // set new cursor position

// if cursor is at end of string, let's append a space to make life easier
	if (key_lines[edit_line][key_linepos] == 0)
	{
		key_lines[edit_line][key_linepos] = ' ';
		key_linepos++;
		key_lines[edit_line][key_linepos] = 0;
	}
}

/*
==============================================================================

DRAWING

==============================================================================
*/


/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void Con_DrawNotify (void)
{
	int		x, v;
	char	*text;
	int		i;
	float	time;

	v = 0;
	for (i= con_current-NUM_CON_TIMES+1 ; i<=con_current ; i++)
	{
		if (i < 0)
			continue;
		time = con_times[i % NUM_CON_TIMES];
		if (time == 0)
			continue;
		time = realtime - time;
		if (time > con_notifytime.value)
			continue;
		text = con_text + (i % con_totallines)*con_linewidth;

		for (x = 0 ; x < con_linewidth ; x++)
			Draw_Character ( (x+1)<<3, v, text[x]);

		v += 8;
	}

	if (key_dest == key_message)
	{
		char *say_prompt;

		x = 0;

		// distinguish say and say_team
		if (team_message)
			say_prompt = "say_team:";
		else
			say_prompt = "say:";

		Draw_String (8, v, say_prompt);

		while(chat_buffer[x])
		{
			Draw_Character ( (x+strlen(say_prompt)+2)<<3, v, chat_buffer[x]);
			x++;
		}
		Draw_Character ( (x+strlen(say_prompt)+2)<<3, v, 10+((int)(realtime*con_cursorspeed)&1));
		v += 8;
	}
}


/*
================
Con_DrawInput - modified to allow insert editing

The input line scrolls horizontally if typing goes beyond the right edge
================
*/
void Con_DrawInput (void)
{
	int		i;
	char	*text, temp[MAX_CMDLINE];

	if (key_dest != key_console && !con_forcedup)
		return;		// don't draw anything

	text = strcpy (temp, key_lines[edit_line]);

// fill out remainder with spaces
	for (i = strlen(text) ; i < MAX_CMDLINE ; i++)
		text[i] = ' ';

// add the cursor frame
	if ((int)(realtime * con_cursorspeed) & 1)	// cursor is visible
//		text[key_linepos] = 11 + 130 * key_insert;	// either solid block or triagle facing right
//		text[key_linepos] = 11 + 84 * key_insert;
		text[key_linepos] = key_insert ? 11 : (11 + 84); // either solid block for insert mode or underline for replace mode

// prestep if horizontally scrolling
	if (key_linepos >= con_linewidth)
		text += 1 + key_linepos - con_linewidth;
	
// draw it
	for (i=0 ; i < con_linewidth ; i++)
		Draw_Character ( (i+1)<<3, con_vislines - 16, text[i]);
}


/*
================
Con_DrawConsole

Draws the console with the solid background
The typing input line at the bottom should only be drawn if typing is allowed
================
*/
void Con_DrawConsole (int lines, qboolean drawinput)
{
	int				i, x, y;
	int  rows, sb;
	char *text, ver[256];
	int  j, len;
	
	if (lines <= 0)
		return;

// draw the background
	Draw_ConsoleBackground (lines);

// draw the buffer text
	con_vislines = lines * vid.conheight / vid.height;

	rows = (con_vislines + 7) / 8;	// rows of text to draw
	y = con_vislines - rows * 8;	// may start slightly negative
	rows -= 2;			// for input and version lines
	sb = con_backscroll ? 1 : 0;	// > 1 generates blank lines in arrow printout below

	for (i= con_current - rows + 1 ; i<=con_current - sb ; i++, y+=8 )
	{
		j = i - con_backscroll;
		if (j<0)
			j = 0;
		text = con_text + (j % con_totallines)*con_linewidth;

		for (x=0 ; x<con_linewidth ; x++)
			Draw_Character ( (x+1)<<3, y, text[x]);
	}

// draw scrollback arrows
	if (con_backscroll)
	{
		y += (sb - 1) * 8; // 0 or more blank lines
		for (x=0 ; x<con_linewidth ; x+=4)
			Draw_Character ((x+1)<<3, y, '^');
		y+=8;
	}

// draw the input prompt, user text, and cursor if desired
	if (drawinput)
		Con_DrawInput ();

//draw version number in bottom right
	y += 8;
	sprintf (ver, "fxQuake %4.2f", (float)VERSION);
	len = strlen (ver);
	for (x = 0; x < len; x++)
		Draw_Character ((con_linewidth - len + x + 2) << 3, y, ver[x] /*+ 128*/);
}


/*
==================
Con_NotifyBox
==================
*/
void Con_NotifyBox (char *text)
{
	double		t1, t2;

// during startup for sound / cd warnings
	Con_Printf ("\n\n%s", Con_Quakebar(40));
	Con_Printf ("%s", text);

	Con_Printf ("Press a key.\n");
	Con_Printf (Con_Quakebar(40));

	key_count = -2;		// wait for a key down and up
	key_dest = key_console;

	do
	{
		t1 = Sys_DoubleTime ();
		SCR_UpdateScreen ();
		IN_ProcessEvents ();
		t2 = Sys_DoubleTime ();
		realtime += t2-t1;		// make the cursor blink
	} while (key_count < 0);

	Con_Printf ("\n");
	key_dest = key_game;
	realtime = 0;				// put the cursor back to invisible
}

