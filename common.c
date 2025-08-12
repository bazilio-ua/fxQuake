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
// common.c -- misc functions used in client and server

#include "quakedef.h"

#define NUM_SAFE_ARGVS  5

static char     *largv[MAX_NUM_ARGVS + NUM_SAFE_ARGVS + 1];
static char     *argvdummy = " ";

static char     *safeargvs[NUM_SAFE_ARGVS] =
	{"-nolan", "-nosound", "-nocdaudio", "-nojoy", "-nomouse"};

cvar_t  registered = {"registered","0", CVAR_ROM};
cvar_t  cmdline = {"cmdline","", CVAR_ROM};

qboolean        com_modified;   // set true if using non-id files


void COM_InitFilesystem (void);
void COM_Path_f (void);
void COM_Game_f (void);

// if a packfile directory differs from this, it is assumed to be hacked
#define PAK0_COUNT_V091		308		// id1/pak0.pak - v0.91/0.92, not supported
#define PAK0_CRC_V091		28804	// id1/pak0.pak - v0.91/0.92, not supported

#define PAK0_CRC_V100		13900	// id1/pak0.pak - v1.00
#define PAK0_CRC_V101		62751	// id1/pak0.pak - v1.01

#define PAK0_COUNT			339		// id1/pak0.pak - v1.0x
#define PAK0_CRC			32981	// id1/pak0.pak - v1.06

char	com_token[1024];
int		com_argc;
char	**com_argv;

#define CMDLINE_LENGTH	256
char	com_cmdline[CMDLINE_LENGTH];

qboolean		standard_quake = true, rogue = false, hipnotic = false, nehahra = false, quoth = false;

// this graphic needs to be in the pak file to use registered features
unsigned short pop[] =
{
 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
,0x0000,0x0000,0x6600,0x0000,0x0000,0x0000,0x6600,0x0000
,0x0000,0x0066,0x0000,0x0000,0x0000,0x0000,0x0067,0x0000
,0x0000,0x6665,0x0000,0x0000,0x0000,0x0000,0x0065,0x6600
,0x0063,0x6561,0x0000,0x0000,0x0000,0x0000,0x0061,0x6563
,0x0064,0x6561,0x0000,0x0000,0x0000,0x0000,0x0061,0x6564
,0x0064,0x6564,0x0000,0x6469,0x6969,0x6400,0x0064,0x6564
,0x0063,0x6568,0x6200,0x0064,0x6864,0x0000,0x6268,0x6563
,0x0000,0x6567,0x6963,0x0064,0x6764,0x0063,0x6967,0x6500
,0x0000,0x6266,0x6769,0x6a68,0x6768,0x6a69,0x6766,0x6200
,0x0000,0x0062,0x6566,0x6666,0x6666,0x6666,0x6562,0x0000
,0x0000,0x0000,0x0062,0x6364,0x6664,0x6362,0x0000,0x0000
,0x0000,0x0000,0x0000,0x0062,0x6662,0x0000,0x0000,0x0000
,0x0000,0x0000,0x0000,0x0061,0x6661,0x0000,0x0000,0x0000
,0x0000,0x0000,0x0000,0x0000,0x6500,0x0000,0x0000,0x0000
,0x0000,0x0000,0x0000,0x0000,0x6400,0x0000,0x0000,0x0000
};

/*


All of Quake's data access is through a hierchal file system, but the contents of the file system can be transparently merged from several sources.

The "base directory" is the path to the directory holding the quake.exe and all game directories.  The sys_* files pass this to host_init in quakeparms_t->basedir.  This can be overridden with the "-basedir" command line parm to allow code debugging in a different directory.  The base directory is
only used during filesystem initialization.

The "game directory" is the first tree on the search path and directory that all generated files (savegames, screenshots, demos, config files) will be saved to.  This can be overridden with the "-game" command line parameter.  The game directory can never be changed while quake is executing.  This is a precacution against having a malicious server instruct clients to write files over areas they shouldn't.

The "cache directory" is only used during development to save network bandwidth, especially over ISDN / T1 lines.  If there is a cache directory
specified, when a file is found by the normal search path, it will be mirrored
into the cache directory, then opened there.

	
*/

//============================================================================

qboolean IsTimeout (float *prevtime, float waittime)
{
	float currtime = Sys_DoubleTime ();

	if (*prevtime && currtime - *prevtime < waittime)
		return false;
	*prevtime = currtime;
	return true;
}

// ClearLink is used for new headnodes
void ClearLink (link_t *l)
{
	l->prev = l->next = l;
}

void RemoveLink (link_t *l)
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

void InsertLinkBefore (link_t *l, link_t *before)
{
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}
void InsertLinkAfter (link_t *l, link_t *after)
{
	l->next = after->next;
	l->prev = after;
	l->prev->next = l;
	l->next->prev = l;
}

/*
============================================================================

					LIBRARY REPLACEMENT FUNCTIONS

============================================================================
*/

/*
	Q_strnlen
	strlen with a cutoff
*/
size_t Q_strnlen (const char *s, size_t maxlen)
{
	size_t i;
	for (i = 0; i < maxlen && s[i]; i++) {}
    
    return i;
} 

/*
============================================================================

					BYTE ORDER FUNCTIONS

============================================================================
*/

qboolean        bigendian;

short   (*BigShort) (short l);
short   (*LittleShort) (short l);
int     (*BigLong) (int l);
int     (*LittleLong) (int l);
float   (*BigFloat) (float l);
float   (*LittleFloat) (float l);

short   ShortSwap (short l)
{
	byte    b1,b2;

	b1 = l&255;
	b2 = (l>>8)&255;

	return (b1<<8) + b2;
}

short   ShortNoSwap (short l)
{
	return l;
}

int    LongSwap (int l)
{
	byte    b1,b2,b3,b4;

	b1 = l&255;
	b2 = (l>>8)&255;
	b3 = (l>>16)&255;
	b4 = (l>>24)&255;

	return ((int)b1<<24) + ((int)b2<<16) + ((int)b3<<8) + b4;
}

int     LongNoSwap (int l)
{
	return l;
}

float FloatSwap (float f)
{
	union
	{
		float   f;
		byte    b[4];
	} dat1, dat2;
	
	
	dat1.f = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];
	return dat2.f;
}

float FloatNoSwap (float f)
{
	return f;
}

/*
==============================================================================

			MESSAGE IO FUNCTIONS

Handles byte ordering and avoids alignment errors
==============================================================================
*/

//
// writing functions
//

void MSG_WriteChar (sizebuf_t *sb, int c)
{
	byte    *buf;

	buf = SZ_GetSpace (sb, 1);
	buf[0] = c;
}

void MSG_WriteByte (sizebuf_t *sb, int c)
{
	byte    *buf;

	buf = SZ_GetSpace (sb, 1);
	buf[0] = c;
}

void MSG_WriteShort (sizebuf_t *sb, int c)
{
	byte    *buf;

	buf = SZ_GetSpace (sb, 2);
	buf[0] = c&0xff;
	buf[1] = c>>8;
}

void MSG_WriteLong (sizebuf_t *sb, int c)
{
	byte    *buf;
	
	buf = SZ_GetSpace (sb, 4);
	buf[0] = c&0xff;
	buf[1] = (c>>8)&0xff;
	buf[2] = (c>>16)&0xff;
	buf[3] = c>>24;
}

void MSG_WriteFloat (sizebuf_t *sb, float f)
{
	union
	{
		float   f;
		int     l;
	} dat;
	
	
	dat.f = f;
	dat.l = LittleLong (dat.l);
	
	SZ_Write (sb, &dat.l, 4);
}

void MSG_WriteString (sizebuf_t *sb, char *s)
{
	if (!s)
		SZ_Write (sb, "", 1);
	else
		SZ_Write (sb, s, strlen(s)+1);
}

//johnfitz -- original behavior, 13.3 fixed point coords, max range +-4096
void MSG_WriteCoord16 (sizebuf_t *sb, float f)
{
	MSG_WriteShort (sb, Q_rint(f * 8.0));
}

//johnfitz -- 16.8 fixed point coords, max range +-32768
void MSG_WriteCoord24 (sizebuf_t *sb, float f)
{
	MSG_WriteShort (sb, f);
	MSG_WriteByte (sb, (int)(f * 255.0) % 255);
}

//johnfitz -- 32-bit float coords
void MSG_WriteCoord32f (sizebuf_t *sb, float f)
{
	MSG_WriteFloat (sb, f);
}

void MSG_WriteCoord (sizebuf_t *sb, float f, unsigned int flags)
{
//	MSG_WriteShort (sb, (int)(f * 8.0));
    
	if (flags & PRFL_FLOATCOORD)
		MSG_WriteFloat (sb, f);
	else if (flags & PRFL_INT32COORD)
		MSG_WriteLong (sb, Q_rint(f * 16.0));
	else if (flags & PRFL_24BITCOORD)
		MSG_WriteCoord24 (sb, f);
	else 
        MSG_WriteCoord16 (sb, f);
}

void MSG_WriteAngle (sizebuf_t *sb, float f, unsigned int flags)
{
//	MSG_WriteByte (sb, (int)(f * 256.0 / 360.0) & 255);
    
	if (flags & PRFL_FLOATANGLE)
		MSG_WriteFloat (sb, f);
	else if (flags & PRFL_SHORTANGLE)
		MSG_WriteShort (sb, Q_rint(f * 65536.0 / 360.0) & 65535); //johnfitz -- use Q_rint instead of (int)
	else 
        MSG_WriteByte (sb, Q_rint(f * 256.0 / 360.0) & 255); //johnfitz -- use Q_rint instead of (int)
}

// precise aim for ProQuake
void MSG_WritePreciseAngle (sizebuf_t *sb, float f)
{
	MSG_WriteShort (sb, (int)(f * 65536.0 / 360.0) & 65535);
}

//johnfitz -- for PROTOCOL_FITZQUAKE
void MSG_WriteAngle16 (sizebuf_t *sb, float f, unsigned int flags)
{
//	MSG_WriteShort (sb, Q_rint(f * 65536.0 / 360.0) & 65535); //johnfitz -- use Q_rint instead of (int)
    
	if (flags & PRFL_FLOATANGLE)
		MSG_WriteFloat (sb, f);
	else 
        MSG_WriteShort (sb, Q_rint(f * 65536.0 / 360.0) & 65535); //johnfitz -- use Q_rint instead of (int)
}
//johnfitz

//
// reading functions
//

void MSG_BeginReading (qmsg_t *msg)
{
	msg->readcount = 0;
	msg->badread = false;
}

// returns -1 and sets msg_badread if no more characters are available
int MSG_ReadChar (qmsg_t *msg)
{
	if (msg->readcount + 1 <= msg->message->cursize)
		return (signed char) msg->message->data[msg->readcount++];

	msg->badread = true;
	return -1;
}

int MSG_ReadByte (qmsg_t *msg)
{
	if (msg->readcount + 1 <= msg->message->cursize)
		return (byte) msg->message->data[msg->readcount++];

	msg->badread = true;
	return -1;
}

// JPG - need this to check for ProQuake messages
int MSG_PeekByte (qmsg_t *msg)
{
	if (msg->readcount + 1 <= msg->message->cursize)
		return (byte) msg->message->data[msg->readcount];

	msg->badread = true;
	return -1;
}

int MSG_ReadShort (qmsg_t *msg)
{
	int         c;

	if (msg->readcount + 2 <= msg->message->cursize) 
	{
		c = (short) (msg->message->data[msg->readcount]
					 + (msg->message->data[msg->readcount + 1] << 8));
		msg->readcount += 2;
		return c;
	}
	msg->readcount = msg->message->cursize;
	msg->badread = true;
	return -1;
}

int MSG_ReadLong (qmsg_t *msg)
{
	int         c;

	if (msg->readcount + 4 <= msg->message->cursize) 
	{
		c = msg->message->data[msg->readcount]
			+ (msg->message->data[msg->readcount + 1] << 8)
			+ (msg->message->data[msg->readcount + 2] << 16)
			+ (msg->message->data[msg->readcount + 3] << 24);
		msg->readcount += 4;
		return c;
	}
	msg->readcount = msg->message->cursize;
	msg->badread = true;
	return -1;
}

float MSG_ReadFloat (qmsg_t *msg)
{
	union 
	{
		byte        b[4];
		float       f;
		int         l;
	} dat;

	if (msg->readcount + 4 <= msg->message->cursize) 
	{
		dat.b[0] = msg->message->data[msg->readcount];
		dat.b[1] = msg->message->data[msg->readcount + 1];
		dat.b[2] = msg->message->data[msg->readcount + 2];
		dat.b[3] = msg->message->data[msg->readcount + 3];
		msg->readcount += 4;

		dat.l = LittleLong (dat.l);

		return dat.f;
	}

	msg->readcount = msg->message->cursize;
	msg->badread = true;
	return -1;
}

char *MSG_ReadString (qmsg_t *msg)
{
	char   *string;
	size_t len, maxlen;

	if (msg->badread || msg->readcount + 1 > msg->message->cursize) 
	{
		msg->badread = true;
		return "";
	}

	string = (char *)&msg->message->data[msg->readcount];

	maxlen = msg->message->cursize - msg->readcount;
	len = strnlen (string, maxlen);
	if (len == maxlen) 
	{
		msg->readcount = msg->readcount;
		msg->badread = true;
		if (len + 1 > msg->badread_string_size) 
		{
			if (msg->badread_string)
				Z_Free (msg->badread_string);
			msg->badread_string = Z_Malloc (len + 1);
			msg->badread_string_size = len + 1;
		}

		strncpy (msg->badread_string, string, len);
		msg->badread_string[len] = 0;
		return msg->badread_string;
	}
	msg->readcount += len + 1;
	
	return string;
}

//johnfitz -- original behavior, 13.3 fixed point coords, max range +-4096
float MSG_ReadCoord16 (qmsg_t *msg)
{
	return MSG_ReadShort (msg) * (1.0 / 8.0);
}

//johnfitz -- 16.8 fixed point coords, max range +-32768
float MSG_ReadCoord24 (qmsg_t *msg)
{
	return MSG_ReadShort (msg) + MSG_ReadByte (msg) * (1.0 / 255.0);
}

//johnfitz -- 32-bit float coords
float MSG_ReadCoord32f (qmsg_t *msg)
{
	return MSG_ReadFloat (msg);
}

float MSG_ReadCoord (qmsg_t *msg, unsigned int flags)
{
//	return MSG_ReadShort (msg) * (1.0 / 8.0);
    
    if (flags & PRFL_FLOATCOORD)
		return MSG_ReadFloat (msg);
	else if (flags & PRFL_INT32COORD)
		return MSG_ReadLong (msg) * (1.0 / 16.0);
	else if (flags & PRFL_24BITCOORD)
		return MSG_ReadCoord24 (msg);
	else 
        return MSG_ReadCoord16 (msg);
}

float MSG_ReadAngle (qmsg_t *msg, unsigned int flags)
{
//	return MSG_ReadChar (msg) * (360.0 / 256.0);
    
	if (flags & PRFL_FLOATANGLE)
		return MSG_ReadFloat (msg);
	else if (flags & PRFL_SHORTANGLE)
		return MSG_ReadShort (msg) * (360.0 / 65536.0);
	else 
        return MSG_ReadChar (msg) * (360.0 / 256.0);
}

// precise aim for ProQuake
float MSG_ReadPreciseAngle (qmsg_t *msg)
{
	return MSG_ReadShort (msg) * (360.0 / 65536.0);
}

//johnfitz -- for PROTOCOL_FITZQUAKE
float MSG_ReadAngle16 (qmsg_t *msg, unsigned int flags)
{
//	return MSG_ReadShort (msg) * (360.0 / 65536.0);
    
	if (flags & PRFL_FLOATANGLE)
		return MSG_ReadFloat (msg);	// make sure
	else 
        return MSG_ReadShort (msg) * (360.0 / 65536.0);
}
//johnfitz

//===========================================================================

void SZ_Alloc (sizebuf_t *buf, int startsize)
{
	if (startsize < 256)
		startsize = 256;
	buf->data = Hunk_AllocName (startsize, "sizebuf");
	buf->maxsize = startsize;
	buf->cursize = 0;
}


void SZ_Free (sizebuf_t *buf)
{
	buf->cursize = 0;
}

void SZ_Clear (sizebuf_t *buf)
{
	buf->cursize = 0;
}

void *SZ_GetSpace (sizebuf_t *buf, int length)
{
	void    *data;
	
	if (buf->cursize + length > buf->maxsize)
	{
		if (!buf->allowoverflow)
			Sys_Error ("SZ_GetSpace: overflow without allowoverflow set (%d, max = %d)", buf->cursize + length, buf->maxsize);
		
		if (length > buf->maxsize)
			Sys_Error ("SZ_GetSpace: %i is > full buffer size %d", length, buf->maxsize);
			
		buf->overflowed = true;
		Con_Printf ("SZ_GetSpace: overflow (%d, max = %d)\n", buf->cursize + length, buf->maxsize);
		SZ_Clear (buf); 
	}

	data = buf->data + buf->cursize;
	buf->cursize += length;
	
	return data;
}

void SZ_Write (sizebuf_t *buf, void *data, int length)
{
	memcpy (SZ_GetSpace(buf,length),data,length);         
}

void SZ_Print (sizebuf_t *buf, char *data)
{
	int             len;
	
	len = strlen(data)+1;

// byte * cast to keep VC++ happy
	if (!buf->cursize || buf->data[buf->cursize-1]) // If buf->data has a trailing zero, overwrite it
		memcpy ((byte *)SZ_GetSpace(buf, len),data,len); // no trailing 0
	else
		memcpy ((byte *)SZ_GetSpace(buf, len-1)-1,data,len); // write over trailing 0
}


//============================================================================


/*
============
COM_SkipPath
============
*/
char *COM_SkipPath (char *path)
{
	char    *last;
	
	last = path;
	while (*path)
	{
		if (*path == '/')
			last = path+1;
		path++;
	}
	return last;
}

/*
============
COM_StripExtension
============
*/
void COM_StripExtension (char *in, char *out)
{
	char	*ext;
	
	if (out != in)
		strcpy (out, in);
	
	ext = strrchr(in, '.');
	if (ext)
		out[ext - in] = 0;
}

/*
============
COM_FileExtension
============
*/
char *COM_FileExtension (char *in)
{
	char	*ext;
	
	ext = strrchr(in, '.');
	return ext ? ++ext : "";
}

/*
============
COM_FileBase
============
*/
void COM_FileBase (char *in, char *out)
{
	char	*base;
	char	*ext;
	size_t	len;
	
	base = COM_SkipPath(in);
	ext = strrchr(base, '.');
	len = ext ? ext - base : strlen(base);
	
	if (len < 2)
		strcpy (out,"?model?");
	else
	{
		if (len >= MAX_QPATH)
			len = MAX_QPATH - 1;
		strncpy (out,base, len);
		out[len] = 0;
	}
}


/*
==================
COM_DefaultExtension

if path doesn't have a .EXT, append extension
(extension should include the .)
==================
*/
void COM_DefaultExtension (char *path, char *ext)
{
	char	*ext2;
	
	ext2 = strrchr(path, '.');
	if (ext2)
		return;                 // it has an extension
	
	strcat (path, ext);
}


/*
==============
COM_Parse

Parse a token out of a string
==============
*/
char *COM_Parse (char *data)
{
	int             c;
	int             len;
	
	len = 0;
	com_token[0] = 0;
	
	if (!data)
		return NULL;
		
// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
			return NULL;                    // end of file;
		data++;
	}
	
// skip // comments
	if (c=='/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}
	
// skip /*..*/ comments
	if (c=='/' && data[1] == '*')
	{
		data += 2;
		while (*data && !(*data == '*' && data[1] == '/'))
			data++;
		if (*data)
			data += 2;
		goto skipwhite;
	}

// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			if ((c = *data) != 0)
				++data;
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				return !c ? NULL : data; // NULL, otherwise data becomes out of bounds
			}
			com_token[len] = c;
			len++;
		}
	}

// parse single characters
	if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c==':')
	{
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data+1;
	}

// parse a regular word
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
		// commented out the check for ':' so that ip:port works
		if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' /* || c==':' */)
			break;
	} while (c>32);
	
	com_token[len] = 0;
	return data;
}


/*
================
COM_CheckParm

Returns the position (1 to argc-1) in the program's argument list
where the given parameter apears, or 0 if not present
================
*/
int COM_CheckParm (char *parm)
{
	int             i;
	
	for (i=1 ; i<com_argc ; i++)
	{
		if (!com_argv[i])
			continue;               // NEXTSTEP sometimes clears appkit vars.
		if (!strcmp (parm,com_argv[i]))
			return i;
	}
		
	return 0;
}

/*
================
COM_CheckRegistered

Looks for the pop.txt file and verifies it.
Sets the "registered" cvar.
Immediately exits out if an alternate game was attempted to be started without
being registered.
================
*/
void COM_CheckRegistered (void)
{
	int             h;
	unsigned short  check[128];
	int                     i;

	Cvar_SetROM ("cmdline", com_cmdline+1); // johnfitz: eliminate leading space

	COM_OpenFile("gfx/pop.lmp", &h, NULL);

	if (h == -1)
	{
		Cvar_SetROM ("registered", "0");
		
		Con_Printf ("Playing shareware version.\n");
		if (com_modified)
			Sys_Error ("You must have the registered version to use modified games");
		return;
	}

	Sys_FileRead (h, check, sizeof(check));
	COM_CloseFile (h);
	
	for (i=0 ; i<128 ; i++)
		if (pop[i] != (unsigned short)BigShort (check[i]))
			Sys_Error ("Corrupted data file.");

	Cvar_SetROM ("registered", "1");

	Con_Printf ("Playing registered version.\n");
}


/*
================
COM_InitArgv
================
*/
void COM_InitArgv (int argc, char **argv)
{
	qboolean        safe;
	int             i, j, n;

// reconstitute the command line for the cmdline externally visible cvar
	n = 0;

	for (j=0 ; (j<MAX_NUM_ARGVS) && (j< argc) ; j++)
	{
		i = 0;

		while ((n < (CMDLINE_LENGTH - 1)) && argv[j][i])
		{
			com_cmdline[n++] = argv[j][i++];
		}

		if (n < (CMDLINE_LENGTH - 1))
			com_cmdline[n++] = ' ';
		else
			break;
	}

	if (n > 0 && com_cmdline[n-1] == ' ')
		com_cmdline[n-1] = 0; // johnfitz: kill the trailing space

	Con_Printf("Command line: %s\n", com_cmdline);

	safe = false;

	for (com_argc=0 ; (com_argc<MAX_NUM_ARGVS) && (com_argc < argc) ; com_argc++)
	{
		largv[com_argc] = argv[com_argc];
		if (!strcmp ("-safe", argv[com_argc]))
			safe = true;
	}

	if (safe)
	{
	// force all the safe-mode switches. Note that we reserved extra space in
	// case we need to add these, so we don't need an overflow check
		for (i=0 ; i<NUM_SAFE_ARGVS ; i++)
		{
			largv[com_argc] = safeargvs[i];
			com_argc++;
		}
	}

	largv[com_argc] = argvdummy;
	com_argv = largv;

	if (COM_CheckParm ("-rogue"))
	{
		rogue = true;
		standard_quake = false;
	}
	
	if (COM_CheckParm ("-hipnotic") || COM_CheckParm ("-quoth"))
	{
		hipnotic = true;
		standard_quake = false;
		if (COM_CheckParm ("-quoth"))
			quoth = true;
	}

	if (COM_CheckParm ("-nehahra"))
		nehahra = true;
}


/*
================
COM_Init
================
*/
void COM_Init (void)
{
	unsigned short byteorder = 1; /* 0x0001 */

// set the byte swapping variables in a portable manner 
	if ( *((byte *) &byteorder) == 1 )
	{
		bigendian = false;
		BigShort = ShortSwap;
		LittleShort = ShortNoSwap;
		BigLong = LongSwap;
		LittleLong = LongNoSwap;
		BigFloat = FloatSwap;
		LittleFloat = FloatNoSwap;
	}
	else
	{
		bigendian = true;
		BigShort = ShortNoSwap;
		LittleShort = ShortSwap;
		BigLong = LongNoSwap;
		LittleLong = LongSwap;
		BigFloat = FloatNoSwap;
		LittleFloat = FloatSwap;
	}

	Cvar_RegisterVariable (&registered);
	Cvar_RegisterVariable (&cmdline);
	Cmd_AddCommand ("path", COM_Path_f);
	Cmd_AddCommand ("game", COM_Game_f); //johnfitz

	COM_InitFilesystem ();
	COM_CheckRegistered ();
}

/*
============
sys_char_map

The translation table between the graphical font and plain ASCII  --KB
============
*/
const char sys_char_map2[256] = {
	  0, '#', '#', '#', '#', '.', '#', '#',
	'#',   9,  10, '#', ' ',  13, '.', '.',
	'[', ']', '0', '1', '2', '3', '4', '5',
	'6', '7', '8', '9', '.', '<', '=', '>',
	' ', '!', '"', '#', '$', '%', '&','\'',
	'(', ')', '*', '+', ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', ':', ';', '<', '=', '>', '?',
	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
	'X', 'Y', 'Z', '[','\\', ']', '^', '_',
	'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
	'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
	'x', 'y', 'z', '{', '|', '}', '~', '<',

	'<', '=', '>', '#', '#', '.', '#', '#',
	'#', '#', ' ', '#', ' ', '>', '.', '.',
	'[', ']', '0', '1', '2', '3', '4', '5',
	'6', '7', '8', '9', '.', '<', '=', '>',
	' ', '!', '"', '#', '$', '%', '&','\'',
	'(', ')', '*', '+', ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', ':', ';', '<', '=', '>', '?',
	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
	'X', 'Y', 'Z', '[','\\', ']', '^', '_',
	'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
	'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
	'x', 'y', 'z', '{', '|', '}', '~', '<'
};

const char sys_char_map[256] = {
	'\0','.', '#', '#', '#', '.', '#', '#',
	'#','\t','\n', '#', ' ','\r', '.', '.',
	'[', ']', '0', '1', '2', '3', '4', '5',
	'6', '7', '8', '9', '.', '<', '-', '>',
	' ', '!','\"', '#', '$', '%', '&','\'',
	'(', ')', '*', '+', ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', ':', ';', '<', '=', '>', '?',
	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
	'X', 'Y', 'Z', '[','\\', ']', '^', '_',
	'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
	'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
	'x', 'y', 'z', '{', '|', '}', '~', '<',

	'(', '=', ')', '*', '#', '.', '#', '#',
	'#','\t','\n', '#', ' ', '>', '.', '.',
	'[', ']', '0', '1', '2', '3', '4', '5',
	'6', '7', '8', '9', '.', '<', '-', '>',
	' ', '!','\"', '#', '$', '%', '&','\'',
	'(', ')', '*', '+', ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', ':', ';', '<', '=', '>', '?',
	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
	'X', 'Y', 'Z', '[','\\', ']', '^', '_',
	'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
	'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
	'x', 'y', 'z', '{', '|', '}', '~', '<'
};

/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
cycles between NUM_BUFFS different static buffers.
FIXME: make this buffer size safe someday
============
*/
#define	NUM_BUFFS	8
char    *va(char *format, ...)
{
	va_list		argptr;
	static char	string[NUM_BUFFS][MAX_PRINTMSG]; // was 1024, changed to array
	static int	idx = 0;

	idx++;
	if (idx == NUM_BUFFS)
		idx = 0;

	va_start (argptr, format);
	vsnprintf (string[idx], sizeof(string[idx]), format, argptr);
	va_end (argptr);

	return string[idx];  
}

/*
===============================================================================

FILE IO

===============================================================================
*/

#define	MAX_HANDLES		100 //10
FILE	*sys_handles[MAX_HANDLES];

int Sys_FindHandle (void)
{
	int		i;

	for (i=1 ; i<MAX_HANDLES ; i++)
		if (!sys_handles[i])
			return i;
	Sys_Error ("out of handles");
	return -1;
}

int Sys_FileLength (FILE *f)
{
	int		pos;
	int		end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}

int Sys_FileOpenRead (char *path, int *handle)
{
	FILE	*f;
	int		i, ret;

	i = Sys_FindHandle ();

	f = fopen (path, "rb");

	if (!f)
	{
		*handle = -1;
		ret = -1;
	}
	else
	{
		sys_handles[i] = f;
		*handle = i;
		ret = Sys_FileLength(f);
	}

	return ret;
}

int Sys_FileOpenWrite (char *path)
{
	FILE	*f;
	int		i;

	i = Sys_FindHandle ();

	f = fopen (path, "wb");
	if (!f)
		Sys_Error ("Error opening %s: %s", path, strerror(errno));
	sys_handles[i] = f;

	return i;
}

void Sys_FileClose (int handle)
{
	fclose (sys_handles[handle]);
	sys_handles[handle] = NULL;
}

void Sys_FileSeek (int handle, int position)
{
	fseek (sys_handles[handle], position, SEEK_SET);
}

int Sys_FileRead (int handle, void *dst, int count)
{
	return fread (dst, 1, count, sys_handles[handle]);
}

int Sys_FileWrite (int handle, void *src, int count)
{
	return fwrite (src, 1, count, sys_handles[handle]);
}

/*
============
Sys_FileTime

returns -1 if not present
============
*/
int Sys_FileTime (char *path)
{
	FILE	*f;
	int		ret;

	f = fopen(path, "rb");

	if (f)
	{
		fclose(f);
		ret = 1;
	}
	else
	{
		ret = -1;
	}

	return ret;
}


/*
=============================================================================

QUAKE FILESYSTEM

=============================================================================
*/

int     com_filesize;

char    com_cachedir[MAX_OSPATH];
char    com_gamedir[MAX_OSPATH];
char    com_basedir[MAX_OSPATH];
char    *homedir = NULL;

searchpath_t    *com_searchpaths;
searchpath_t	*com_base_searchpaths;

/*
============
COM_Path_f

============
*/
void COM_Path_f (void)
{
	searchpath_t    *s;
	
	Con_Printf ("Current search path:\n");
	for (s=com_searchpaths ; s ; s=s->next)
	{
		if (s->pack)
			Con_Printf ("%s (%i files)\n", s->pack->filename, s->pack->numfiles);
		else
			Con_Printf ("%s\n", s->filename);
	}
}

/*
============
COM_WriteFile

The filename will be prefixed by the current game directory
============
*/
void COM_WriteFile (char *filename, void *data, int len)
{
	int             handle;
	char    name[MAX_OSPATH];

	sprintf (name, "%s/%s", com_gamedir, filename);

	handle = Sys_FileOpenWrite (name);
	if (handle == -1)
	{
		if (developer.value > 3)
			Con_DPrintf ("COM_WriteFile: failed on %s\n", name);
		return;
	}

	if (developer.value > 3)
		Con_DPrintf ("COM_WriteFile: %s\n", name);

	Sys_FileWrite (handle, data, len);
	Sys_FileClose (handle);
}

/*
============
COM_CreatePath
============
*/
void    COM_CreatePath (char *path)
{
	char    *ofs;
	
	for (ofs = path+1 ; *ofs ; ofs++)
	{
		if (*ofs == '/')
		{       // create the directory
			*ofs = 0;
			Sys_mkdir (path);
			*ofs = '/';
		}
	}
}

/*
===========
COM_CopyFile

Copies a file over from the net to the local cache, creating any directories
needed.  This is for the convenience of developers using ISDN from home.
===========
*/
void COM_CopyFile (char *netpath, char *cachepath)
{
	int             in, out;
	int             remaining, count;
	char    buf[4096];
	
	remaining = Sys_FileOpenRead (netpath, &in);            
	COM_CreatePath (cachepath);     // create directories up to the cache file
	out = Sys_FileOpenWrite (cachepath);
	
	while (remaining)
	{
		if (remaining < sizeof(buf))
			count = remaining;
		else
			count = sizeof(buf);
		Sys_FileRead (in, buf, count);
		Sys_FileWrite (out, buf, count);
		remaining -= count;
	}

	Sys_FileClose (in);
	Sys_FileClose (out);    
}

/*
===========
COM_FindFile

Finds the file in the search path.
Sets com_filesize and one of handle or file
===========
*/
int COM_FindFile (char *filename, int *handle, FILE **file, unsigned int *path_id)
{
	searchpath_t    *search;
	char            netpath[MAX_OSPATH];
	char            cachepath[MAX_OSPATH];
	pack_t          *pak;
	int                     i;
	int                     findtime, cachetime;

	if (file && handle)
		Sys_Error ("COM_FindFile: both handle and file set");
	if (!file && !handle)
		Sys_Error ("COM_FindFile: neither handle or file set");

//
// search through the path, one element at a time
//
	search = com_searchpaths;
	for ( ; search ; search = search->next)
	{
	// is the element a pak file?
		if (search->pack)
		{
		// look through all the pak file elements
			pak = search->pack;
			for (i=0 ; i<pak->numfiles ; i++)
			{
				if (strcmp(pak->files[i].name, filename))
					continue;

				// found it!
				if (developer.value > 3)
					Con_DPrintf ("PackFile: %s : %s\n",pak->filename, filename);

				if (path_id)
					*path_id = search->path_id;

				if (handle)
				{
					*handle = pak->handle;
					Sys_FileSeek (pak->handle, pak->files[i].filepos);
				}
				else
				{	// open a new file on the pakfile
					*file = fopen (pak->filename, "rb");
					if (*file)
						fseek (*file, pak->files[i].filepos, SEEK_SET);
				}
				com_filesize = pak->files[i].filelen;
				return com_filesize;
			}
		}
		else
		{
	// check a file in the directory tree
			if (!registered.value)
			{	// if not a registered version, don't ever go beyond base
				if ( strchr (filename, '/') || strchr (filename,'\\'))
					continue;
			}

			sprintf (netpath, "%s/%s",search->filename, filename);

			findtime = Sys_FileTime (netpath);
			if (findtime == -1)
				continue;

		// see if the file needs to be updated in the cache
			if (!com_cachedir[0])
				strcpy (cachepath, netpath);
			else
			{
#ifdef _WIN32
				if ((strlen(netpath) < 2) || (netpath[1] != ':'))
					sprintf (cachepath,"%s%s", com_cachedir, netpath);
				else
					sprintf (cachepath,"%s%s", com_cachedir, netpath+2);
#else
				sprintf (cachepath,"%s%s", com_cachedir, netpath);
#endif

				cachetime = Sys_FileTime (cachepath);
			
				if (cachetime < findtime)
					COM_CopyFile (netpath, cachepath);
				strcpy (netpath, cachepath);
			}

			if (developer.value > 3)
				Con_DPrintf ("FindFile: %s\n",netpath);

			if (path_id)
				*path_id = search->path_id;

			com_filesize = Sys_FileOpenRead (netpath, &i);
			if (handle)
				*handle = i;
			else
			{
				Sys_FileClose (i);
				*file = fopen (netpath, "rb");
			}
			return com_filesize;
		}
	}

	if (developer.value > 3)	
		Con_DPrintf ("FindFile: can't find %s\n", filename);

	if (handle)
		*handle = -1;
	else
		*file = NULL;
	com_filesize = -1;
	return -1;
}


/*
===========
COM_OpenFile

filename never has a leading slash, but may contain directory walks
returns a handle and a length
it may actually be inside a pak file
===========
*/
int COM_OpenFile (char *filename, int *handle, unsigned int *path_id)
{
	return COM_FindFile (filename, handle, NULL, path_id);
}

/*
===========
COM_FOpenFile

If the requested file is inside a packfile, a new FILE * will be opened
into the file.
===========
*/
int COM_FOpenFile (char *filename, FILE **file, unsigned int *path_id)
{
	return COM_FindFile (filename, NULL, file, path_id);
}

/*
============
COM_CloseFile

If it is a pak file handle, don't really close it
============
*/
void COM_CloseFile (int h)
{
	searchpath_t    *s;
	
	for (s = com_searchpaths ; s ; s=s->next)
		if (s->pack && s->pack->handle == h)
			return;
			
	Sys_FileClose (h);
}


/*
============
COM_LoadFile

Filename are reletive to the quake directory.
Always appends a 0 byte.
============
*/
#define	LOADFILE_ZONE		0
#define	LOADFILE_HUNK		1
#define	LOADFILE_TEMPHUNK	2
#define	LOADFILE_CACHE		3
#define	LOADFILE_STACK		4
#define	LOADFILE_MALLOC		5

byte	*loadbuf;
int		loadsize;
cache_user_t	*loadcache;

byte *COM_LoadFile (char *path, int usehunk, unsigned int *path_id)
{
	int             h;
	byte    *buf = NULL;	// quiet compiler warning
	char    base[MAX_QPATH];
	int             len;

// look for it in the filesystem or pack files
	len = COM_OpenFile (path, &h, path_id);
	if (h == -1)
		return NULL;
	
// extract the filename base name for hunk tag
	COM_FileBase (path, base);

	switch (usehunk)
	{
	case LOADFILE_HUNK:
		buf = Hunk_AllocName (len+1, base);
		break;
	case LOADFILE_TEMPHUNK:
		buf = Hunk_TempAlloc (len+1);
		break;
	case LOADFILE_ZONE:
		buf = Z_Malloc (len+1);
		break;
	case LOADFILE_CACHE:
		buf = Cache_Alloc (loadcache, len+1, base);
		break;
	case LOADFILE_STACK:
		if (len+1 > loadsize)
			buf = Hunk_TempAlloc (len+1);
		else
			buf = loadbuf;
		break;
	case LOADFILE_MALLOC:
		buf = malloc (len+1);
		break;
	default:
		Sys_Error ("COM_LoadFile: bad usehunk %d", usehunk);
	}

	if (!buf)
		Sys_Error ("COM_LoadFile: not enough space for %s (%dk)", path, (len+1) / 1024);

	((byte *)buf)[len] = 0;

	Draw_BeginDisc ();

	if (Sys_FileRead (h, buf, len) != len)
		Sys_Error ("COM_LoadFile: error reading %s", path);

	COM_CloseFile (h);

	Draw_EndDisc ();

	return buf;
}

byte *COM_LoadHunkFile (char *path, unsigned int *path_id)
{
	return COM_LoadFile (path, LOADFILE_HUNK, path_id);
}

byte *COM_LoadZoneFile (char *path, unsigned int *path_id)
{
	return COM_LoadFile (path, LOADFILE_ZONE, path_id);
}

byte *COM_LoadTempFile (char *path, unsigned int *path_id)
{
	return COM_LoadFile (path, LOADFILE_TEMPHUNK, path_id);
}

void COM_LoadCacheFile (char *path, struct cache_user_s *cu, unsigned int *path_id)
{
	loadcache = cu;
	COM_LoadFile (path, LOADFILE_CACHE, path_id);
}

// uses temp hunk if larger than bufsize
byte *COM_LoadStackFile (char *path, void *buffer, int bufsize, unsigned int *path_id)
{
	byte    *buf;
	
	loadbuf = (byte *)buffer;
	loadsize = bufsize;
	buf = COM_LoadFile (path, LOADFILE_STACK, path_id);
	
	return buf;
}

// returns malloc'd memory
byte *COM_LoadMallocFile (char *path, void *buffer, unsigned int *path_id)
{
	if (buffer)
		free (buffer);

	return COM_LoadFile (path, LOADFILE_MALLOC, path_id);
}

/*
=================
COM_LoadPackFile -- johnfitz -- modified based on topaz's tutorial

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
pack_t *COM_LoadPackFile (char *packfilename)
{
	dpackheader_t   header;
	int             i;
	packfile_t              *newfiles;
	int                             numpackfiles;
	pack_t                  *pack;
	int                             packhandle;
	dpackfile_t             info[MAX_FILES_IN_PACK];
	unsigned short          crc;

	if (Sys_FileOpenRead (packfilename, &packhandle) == -1)
	{
//		Con_SafePrintf ("COM_LoadPackFile: couldn't open %s\n", packfilename);
		return NULL;
	}
	if (Sys_FileRead (packhandle, (void *)&header, sizeof(header)) != sizeof(header) ||
	    header.ident[0] != 'P' || header.ident[1] != 'A' || header.ident[2] != 'C' || header.ident[3] != 'K')
		Sys_Error ("COM_LoadPackFile: %s is not a packfile, can't read header PACK id", packfilename);

	header.dirofs = LittleLong (header.dirofs);
	header.dirlen = LittleLong (header.dirlen);

	if (header.dirlen < 0 || header.dirofs < 0)
		Sys_Error ("COM_LoadPackFile: invalid packfile %s (dirlen: %i, dirofs: %i)", packfilename, header.dirlen, header.dirofs);
	
	numpackfiles = header.dirlen / sizeof(dpackfile_t);

	if (!numpackfiles)
	{
		Sys_Printf ("WARNING: %s has no files\n", packfilename);
//		Sys_Printf ("WARNING: %s has no files, ignored\n", packfilename);
//		Sys_FileClose (packhandle);
//		return NULL;
	}

	if (numpackfiles > MAX_FILES_IN_PACK)
		Sys_Error ("COM_LoadPackFile: packfile %s has too many files (%i, max = %i)", packfilename, numpackfiles, MAX_FILES_IN_PACK);

	//johnfitz -- dynamic gamedir loading
	//johnfitz -- modified to use zone alloc
	newfiles = (packfile_t *) Z_Malloc (numpackfiles * sizeof(packfile_t));

	Sys_FileSeek (packhandle, header.dirofs);
	if (Sys_FileRead (packhandle, (void *)info, header.dirlen) != header.dirlen)
		Sys_Error ("COM_LoadPackFile: can't read directory in packfile %s", packfilename);

// crc the directory to check for modifications
	CRC_Init (&crc);
	for (i=0 ; i<header.dirlen ; i++)
		CRC_ProcessByte (&crc, ((byte *)info)[i]);

	if (numpackfiles != PAK0_COUNT && crc != PAK0_CRC && crc != PAK0_CRC_V100 && crc != PAK0_CRC_V101)
		com_modified = true;    // not the original file

// parse the directory
	for (i=0 ; i<numpackfiles ; i++)
	{
		strcpy (newfiles[i].name, info[i].name);
		newfiles[i].filepos = LittleLong(info[i].filepos);
		newfiles[i].filelen = LittleLong(info[i].filelen);
	}

	//johnfitz -- dynamic gamedir loading
	//johnfitz -- modified to use zone alloc
	pack = (pack_t *) Z_Malloc (sizeof (pack_t));

	strcpy (pack->filename, packfilename);
	pack->handle = packhandle;
	pack->numfiles = numpackfiles;
	pack->files = newfiles;

	Con_Printf ("Added packfile %s (%i files)\n", packfilename, numpackfiles);
	return pack;
}


/*
================
COM_AddDirectory -- johnfitz -- modified based on topaz's tutorial

adds the directory to the head of the path,
then loads and adds pak1.pak pak2.pak ...
================
*/
void COM_AddDirectory (char *dir)
{
	int                             i;
	unsigned int    path_id;
	searchpath_t    *search;
	pack_t                  *pak;
	char                    pakfile[MAX_OSPATH];

//
// assign a path_id to this game directory
//
	if (com_searchpaths)
		path_id = com_searchpaths->path_id << 1;
	else
		path_id = 1U;

//
// add the directory to the search path
//
	//johnfitz -- dynamic gamedir loading
	//johnfitz -- modified to use zone alloc
	search = (searchpath_t *) Z_Malloc (sizeof(searchpath_t));
	search->path_id = path_id;
	strcpy (search->filename, dir);
	search->next = com_searchpaths;
	com_searchpaths = search;

//
// add any pak files in the format pak0.pak pak1.pak, ...
//
	for (i=0 ; ; i++)
	{
		sprintf (pakfile, "%s/pak%i.pak", dir, i);
		pak = COM_LoadPackFile (pakfile);
		if (!pak)
			break;
		//johnfitz -- dynamic gamedir loading
		//johnfitz -- modified to use zone alloc
		search = (searchpath_t *) Z_Malloc (sizeof(searchpath_t));
		search->path_id = path_id;
		search->pack = pak;
		search->next = com_searchpaths;
		com_searchpaths = search;
	}
}

/*
================
COM_AddGameDirectory

Sets com_gamedir, adds the directory to the head of the path,
then loads and adds pak1.pak pak2.pak ...
================
*/
void COM_AddGameDirectory (char *base, char *dir)
{
	strcpy (com_gamedir, va("%s/%s", base, dir));
	
	COM_AddDirectory(com_gamedir);
}

/*
================
COM_SetGamedirToHomeDirectory

sets gamedir to home dir
================
*/
void COM_SetGamedirToHomeDirectory (char *home, char *dir)
{
	if (!home)
		return;

#if defined __APPLE__ && defined __MACH__
	strcpy (com_gamedir, va("%s/Library/Application Support/fxQuake/%s", home, dir));
#else
	strcpy (com_gamedir, va("%s/.fxQuake/%s", home, dir));
#endif
}

/*
================
COM_AddUserDirectory

Sets com_gamedir, adds the user directory to the head of the path,
================
*/
void COM_AddUserDirectory (char *home, char *dir)
{
	if (!home)
		return;

	COM_SetGamedirToHomeDirectory (home, dir);

	COM_AddDirectory(com_gamedir);
	
	// If home is available, create the user directory
	COM_CreatePath (com_gamedir);
	Sys_mkdir (com_gamedir);
}


/*
============
COM_Game_f

johnfitz -- dynamic gamedir stuff
modified by QuakeSpasm team.
============
*/
void COM_Game_f (void)
{
	char *p = Cmd_Argv(1);
	char *p2 = Cmd_Argv(2);
	char *p3 = Cmd_Argv(3);
	char *p4 = Cmd_Argv(4);
	searchpath_t *search;

	if (Cmd_Argc() > 1)
	{
		if (!registered.value) // disable shareware quake
		{
			Con_Printf ("You must have the registered version to use modified games\n");
			return;
		}
		
		if (!*p || !strcmp(p, ".") || strstr(p, "..") || strstr(p, "/") || strstr(p, "\\") || strstr(p, ":"))
		{
			Con_Printf ("Relative pathnames are not allowed\n");
			Con_Printf ("game directory should be a single directory name, not a path\n");
			return;
		}
		
		if (*p2)
		{
			if (strcmp(p2,"-id1") && strcmp(p2,"-hipnotic") && strcmp(p2,"-rogue") && strcmp(p2,"-quoth") && strcmp(p2,"-nehahra")) {
				Con_Printf ("Invalid mission pack argument to \"game\"\n");
				return;
			}
			if (!strcasecmp(p, GAMENAME)) {
				Con_Printf ("No mission pack arguments to %s game\n", GAMENAME);
				return;
			}
		}
		
		if (*p3)
		{
			if (strcmp(p3, "-mod")) {
				Con_Printf ("Invalid mod argument to \"game\"\n");
				return;
			}
			if (!*p4 || !strcmp(p4, ".") || strstr(p4, "..") || strstr(p4, "/") || strstr(p4, "\\") || strstr(p4, ":"))
			{
				Con_Printf ("Relative pathnames are not allowed\n");
				Con_Printf ("mod directory should be a single directory name, not a path\n");
				return;
			}
			if (Sys_FileTime(va("%s/%s", com_basedir, p4)) == -1)
			{
				Con_Printf ("No such mod directory \"%s\"\n", p4);
				return;
			}
		}
		
		if (Sys_FileTime(va("%s/%s", com_basedir, p)) == -1)
		{
			Con_Printf ("No such game directory \"%s\"\n", p);
			return;
		}
		
		if (!strcasecmp(p, COM_SkipPath(com_gamedir))) // no change
		{
			if (com_searchpaths->path_id > 1)
			{
				// current game not id1
				if (*p2 && com_searchpaths->path_id == 2)
				{
					// QS: rely on treating '-game missionpack'
					// as '-missionpack', otherwise would be a mess
					if (!strcasecmp(p, &p2[1]))
						goto _same;
					Con_Printf ("reloading game \"%s\" with \"%s\" support\n", p, &p2[1]);
				}
				else if (!*p2 && com_searchpaths->path_id > 2)
					Con_Printf ("reloading game \"%s\" without mission pack support\n", p);
				else
					goto _same;
			}
			else
			{	_same:
				Con_Printf ("\"game\" is already \"%s\"\n", COM_SkipPath(com_gamedir));
				return;
			}
		}
		
		// Shutdown the server
		CL_Disconnect ();
		Host_ShutdownServer (true);
		
		// Write config file
		Host_WriteConfiguration ("config.cfg");
		
		History_Shutdown ();
		LOG_Close ();

		// Close the extra game if it is loaded
		while (com_searchpaths != com_base_searchpaths)
		{
			if (com_searchpaths->pack)
			{
				Sys_FileClose (com_searchpaths->pack->handle);
				Z_Free (com_searchpaths->pack->files);
				Z_Free (com_searchpaths->pack);
			}
			search = com_searchpaths->next;
			Z_Free (com_searchpaths);
			com_searchpaths = search;
		}
		
		com_modified = true;
		
		hipnotic = false;
		rogue = false;
		standard_quake = true;

		quoth = false;
		nehahra = false;
		
		if (strcasecmp(p, GAMENAME)) // game is not "id1"
		{
			if (*p2)
			{
				if (strcmp(p2,"-id1")) {
					COM_AddGameDirectory (com_basedir, &p2[1]);
					COM_AddUserDirectory (homedir, &p2[1]);
				}
				
				if (!strcmp(p2,"-hipnotic") || !strcmp(p2,"-quoth"))
				{
					hipnotic = true;
					standard_quake = false;
					if (!strcmp(p2,"-quoth"))
						quoth = true;
				}
				else if (!strcmp(p2,"-rogue"))
				{
					rogue = true;
					standard_quake = false;
				}
				else if (!strcmp(p2,"-nehahra"))
				{
					nehahra = true;
					standard_quake = true;
				}
				
				// "-mod"
				if (*p3)
				{
					if (strcasecmp(p4, &p2[1])) // don't load twice
					{
						COM_AddGameDirectory (com_basedir, p4);
						COM_AddUserDirectory (homedir, p4);
					}
					
					// QS: treat '-mod missionpack' as '-missionpack'
					if (!strcasecmp(p4,"hipnotic") || !strcasecmp(p4,"quoth"))
					{
						hipnotic = true;
						standard_quake = false;
						if (!strcasecmp(p4,"quoth"))
							quoth = true;
					}
					else if (!strcasecmp(p4,"rogue"))
					{
						rogue = true;
						standard_quake = false;
					}
					else if (!strcasecmp(p4,"nehahra"))
					{
						nehahra = true;
						standard_quake = true;
					}
					
					if (strcasecmp(p, p4) && strcasecmp(p, &p2[1])) // don't load twice
					{
						COM_AddGameDirectory (com_basedir, p);
						COM_AddUserDirectory (homedir, p);
					}
				}
				else
				if (strcasecmp(p, &p2[1])) // don't load twice
				{
					COM_AddGameDirectory (com_basedir, p);
					COM_AddUserDirectory (homedir, p);
				}
			}
			else
			{
				COM_AddGameDirectory (com_basedir, p);
				COM_AddUserDirectory (homedir, p);
				// QS: treat '-game missionpack' as '-missionpack'
				if (!strcasecmp(p,"hipnotic") || !strcasecmp(p,"quoth"))
				{
					hipnotic = true;
					standard_quake = false;
					if (!strcasecmp(p,"quoth"))
						quoth = true;
				}
				else if (!strcasecmp(p,"rogue"))
				{
					rogue = true;
					standard_quake = false;
				}
				else if (!strcasecmp(p,"nehahra"))
				{
					nehahra = true;
					standard_quake = true;
				}
			}
		}
		else // just update com_gamedir, game is "id1"
		{
			strcpy (com_gamedir, va("%s/%s", com_basedir, GAMENAME));
			COM_SetGamedirToHomeDirectory (homedir, GAMENAME);
		}

		LOG_Init ();
		History_Init ();

		if (nehahra)
			Host_AddExtCommands ();
		
		// clear out and reload appropriate data
		Cache_Flush ();
		Mod_ResetAll ();
		Sky_ClearAll();
		
		Host_LoadPalettes ();
		
		if (cls.state != ca_dedicated)
		{
			TexMgr_NewGame ();
			Draw_NewGame ();
			R_InitPlayerTextures ();
			R_InitBloomTextures ();
		}
		
		Host_MapListRebuild ();
		Host_Resetdemos ();
		Host_DemoListRebuild ();
		Host_SaveListRebuild ();
		Host_ConfigListRebuild ();
		
		Con_Printf("\"game\" changed to \"%s\"\n", COM_SkipPath(com_gamedir));
		
		Cbuf_AddText ("vid_lock\n");
		Cbuf_AddText ("exec quake.rc\n");
		Cbuf_AddText ("vid_unlock\n");
	}
	else // Diplay the current gamedir
		Con_Printf ("\"game\" is \"%s\"\n", COM_SkipPath(com_gamedir));
}


/*
================
COM_InitFilesystem

johnfitz -- modified based on topaz's tutorial
================
*/
void COM_InitFilesystem (void)
{
	int			i, j;
	char		*p;
	searchpath_t	*search;

#ifdef DO_USERDIRS
	homedir = getenv("HOME");
#endif

//
// -basedir <path>
// Overrides the system supplied base directory (under GAMENAME)
//
	i = COM_CheckParm ("-basedir");
	if (i && i < com_argc-1)
		strcpy (com_basedir, com_argv[i+1]);
	else
		strcpy (com_basedir, host_parms->basedir);

	j = strlen (com_basedir);
	if (j < 1) Sys_Error ("Bad argument to -basedir");
	if ((com_basedir[j-1] == '\\') || (com_basedir[j-1] == '/'))
		com_basedir[j-1] = 0;

//
// -cachedir <path>
// Overrides the system supplied cache directory (NULL or /qcache)
// -cachedir - will disable caching.
//
	i = COM_CheckParm ("-cachedir");
	if (i && i < com_argc-1)
	{
		if (com_argv[i+1][0] == '-')
			com_cachedir[0] = 0;
		else
			strcpy (com_cachedir, com_argv[i+1]);
	}
	else if (host_parms->cachedir)
		strcpy (com_cachedir, host_parms->cachedir);
	else
		com_cachedir[0] = 0;

//
// start up with GAMENAME by default (id1)
//
	COM_AddGameDirectory (com_basedir, GAMENAME);
	COM_AddUserDirectory (homedir, GAMENAME);

	/* this is the end of our base searchpath:
	 * any set gamedirs, such as those from -game command line
	 * arguments or by the 'game' console command will be freed
	 * up to here upon a new game command. */
	com_base_searchpaths = com_searchpaths;

	if (COM_CheckParm ("-rogue"))
	{
		COM_AddGameDirectory (com_basedir, "rogue");
		COM_AddUserDirectory (homedir, "rogue");
	}

	if (COM_CheckParm ("-hipnotic"))
	{
		COM_AddGameDirectory (com_basedir, "hipnotic");
		COM_AddUserDirectory (homedir, "hipnotic");
	}

	if (COM_CheckParm ("-quoth"))
	{
		COM_AddGameDirectory (com_basedir, "quoth");
		COM_AddUserDirectory (homedir, "quoth");
	}

	if (COM_CheckParm ("-nehahra"))
	{
		COM_AddGameDirectory (com_basedir, "nehahra");
		COM_AddUserDirectory (homedir, "nehahra");
	}

//
// -mod <moddir>
// Similar to "-game", for flexible adding 3rd party mods
// (mods based on other mods), e.g. warpspasm based on quoth.
// example: "-hipnotic -mod quoth -game warp"
//
	i = COM_CheckParm ("-mod");
	if (i && i < com_argc-1)
	{
		p = com_argv[i + 1];
		if (!*p || !strcmp(p, ".") || strstr(p, "..") || strstr(p, "/") || strstr(p, "\\") || strstr(p, ":"))
			Sys_Error ("moddir should be a single directory name, not a path\n");

		com_modified = true;
		
		// don't load mission packs twice
		if (p && COM_CheckParm ("-rogue") && !strcasecmp(p, "rogue")) p = NULL;
		if (p && COM_CheckParm ("-hipnotic") && !strcasecmp(p, "hipnotic")) p = NULL;
		if (p && COM_CheckParm ("-quoth") && !strcasecmp(p, "quoth")) p = NULL;
		if (p && COM_CheckParm ("-nehahra") && !strcasecmp(p, "nehahra")) p = NULL;

		if (p != NULL)
		{
			COM_AddGameDirectory (com_basedir, com_argv[i+1]);
			COM_AddUserDirectory (homedir, com_argv[i+1]);
			
			// QS: treat '-mod missionpack' as '-missionpack'
			if (!strcasecmp(p,"rogue"))
			{
				rogue = true;
				standard_quake = false;
			}
			if (!strcasecmp(p,"hipnotic") || !strcasecmp(p,"quoth"))
			{
				hipnotic = true;
				standard_quake = false;
				if (!strcasecmp(p,"quoth"))
					quoth = true;
			}
			if (!strcasecmp(p,"nehahra"))
			{
				nehahra = true;
				standard_quake = true;
			}
		}
	}

//
// -game <gamedir>
// Adds basedir/gamedir as an override game
//
	i = COM_CheckParm ("-game");
	if (i && i < com_argc-1)
	{
		p = com_argv[i + 1];
		if (!*p || !strcmp(p, ".") || strstr(p, "..") || strstr(p, "/") || strstr(p, "\\") || strstr(p, ":"))
			Sys_Error ("gamedir should be a single directory name, not a path\n");
		
		com_modified = true;
		
		// don't load mission packs twice
		if (p && COM_CheckParm ("-rogue") && !strcasecmp(p, "rogue")) p = NULL;
		if (p && COM_CheckParm ("-hipnotic") && !strcasecmp(p, "hipnotic")) p = NULL;
		if (p && COM_CheckParm ("-quoth") && !strcasecmp(p, "quoth")) p = NULL;
		if (p && COM_CheckParm ("-nehahra") && !strcasecmp(p, "nehahra")) p = NULL;
		
		if (p != NULL)
		{
			COM_AddGameDirectory (com_basedir, com_argv[i+1]);
			COM_AddUserDirectory (homedir, com_argv[i+1]);
			
			// QS: treat '-game missionpack' as '-missionpack'
			if (!strcasecmp(p,"rogue"))
			{
				rogue = true;
				standard_quake = false;
			}
			if (!strcasecmp(p,"hipnotic") || !strcasecmp(p,"quoth"))
			{
				hipnotic = true;
				standard_quake = false;
				if (!strcasecmp(p,"quoth"))
					quoth = true;
			}
			if (!strcasecmp(p,"nehahra"))
			{
				nehahra = true;
				standard_quake = true;
			}
		}
	}

//
// -path <dir or packfile> [<dir or packfile>] ...
// Fully specifies the exact search path, overriding the generated one
//
	i = COM_CheckParm ("-path");
	if (i)
	{
		com_modified = true;
		com_searchpaths = NULL;
		while (++i < com_argc)
		{
			if (!com_argv[i] || com_argv[i][0] == '+' || com_argv[i][0] == '-')
				break;
			//johnfitz -- dynamic gamedir loading
			//johnfitz -- modified to use zone alloc
			search = (searchpath_t *) Z_Malloc (sizeof(searchpath_t));
			if ( !strcasecmp(COM_FileExtension(com_argv[i]), "pak") )
			{
				search->pack = COM_LoadPackFile (com_argv[i]);
				if (!search->pack)
					Sys_Error ("COM_InitFilesystem: couldn't load packfile: %s", com_argv[i]);
			}
			else
				strcpy (search->filename, com_argv[i]);
			search->next = com_searchpaths;
			com_searchpaths = search;
		}
	}
}

/*
==============================================================================

	FILELIST

==============================================================================
*/

/*
==================
COM_FileListAdd
==================
*/
void COM_FileListAdd (char *name, filelist_t **list)
{
	filelist_t	*item, *cursor, *prev;

	// ignore duplicate
	for (item = *list; item; item = item->next)
	{
		if (!strcmp (name, item->name))
			return;
	}

	item = Z_Malloc (sizeof(filelist_t));
	strcpy (item->name, name);

	// insert each entry in alphabetical order
	if (*list == NULL ||
	    strcasecmp(item->name, (*list)->name) < 0) //insert at front
	{
		item->next = *list;
		*list = item;
	}
	else //insert later
	{
		prev = *list;
		cursor = (*list)->next;
		while (cursor && (strcasecmp(item->name, cursor->name) > 0))
		{
			prev = cursor;
			cursor = cursor->next;
		}
		item->next = prev->next;
		prev->next = item;
	}
}

/*
==================
COM_FileListClear
==================
*/
void COM_FileListClear (filelist_t **list)
{
	filelist_t *item;
	
	while (*list)
	{
		item = (*list)->next;
		Z_Free (*list);
		*list = item;
	}
}

/*
==================
COM_ScanDirList
==================
*/
void COM_ScanDirList(char *path, filelist_t **list)
{
	Sys_ScanDirList(path, list);
}

/*
==================
COM_ScanDirFileList
==================
*/
void COM_ScanDirFileList(char *path, char *subdir, char *ext, qboolean stripext, filelist_t **list)
{
	Sys_ScanDirFileList(path, subdir, ext, stripext, list);
}

/*
==================
COM_ScanPakFileList
==================
*/
void COM_ScanPakFileList(pack_t *pack, char *subdir, char *ext, qboolean stripext, filelist_t **list)
{
	int			i;
	pack_t		*pak;
	char		*path;
	char		filename[MAX_QPATH];
	
	for (i=0, pak = pack ; i<pak->numfiles ; i++)
	{
		if (strchr(pak->files[i].name, '/') && !subdir[0])
			continue;		
		
		if (subdir[0] && !strstr(pak->files[i].name, subdir))
			continue;
		
		if (!strcasecmp(COM_FileExtension(pak->files[i].name), ext))
		{
			path = COM_SkipPath(pak->files[i].name);
			if (stripext)
				COM_StripExtension(path, filename);
			else
				strcpy(filename, path);
			
			COM_FileListAdd (filename, list);
		}
	}
}

