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
// sys_mac.m

#include "quakedef.h"
#include "macquake.h"

static qboolean nostdout = false;

// =======================================================================
// General routines
// =======================================================================

/*
===============================================================================

FILE IO

===============================================================================
*/

/*
================
Sys_mkdir
================
*/
void Sys_mkdir (char *path)
{
	
}

/*
================
Sys_ScanDirFileList
================
*/
void Sys_ScanDirFileList(char *path, char *subdir, char *ext, qboolean stripext, filelist_t **list)
{
	
}

/*
================
Sys_EditFile

currently unused func
================
*/
void Sys_EditFile(char *filename)
{
	
}

/*
===============================================================================

SYSTEM IO

===============================================================================
*/

/*
================
Sys_Init
================
*/
void Sys_Init(void)
{
	
}

void Sys_Printf (char *fmt, ...)
{
	
}

void Sys_Error (char *error, ...)
{ 
	
} 

void Sys_Shutdown (void)
{
	
}

void Sys_Quit (int code)
{
	
}


/*
================
Sys_DoubleTime
================
*/
double Sys_DoubleTime (void)
{
	return 0;
}

/*
================
Sys_ConsoleInput
================
*/
char *Sys_ConsoleInput(void)
{
	return NULL;
}

/*
================
Sys_GetClipboardData

Clipboard function, thx quake2 icculus
================
*/
#define	SYS_CLIPBOARD_SIZE	MAX_CMDLINE		// 256

char *Sys_GetClipboardData (void)
{
	char *clipboard = NULL;
	
	return clipboard;
} 

void Sys_Sleep (void)
{
	
}

/*
================
main
================
*/
//char *qbasedir = ".";
//char *qcachedir = "/tmp";

int main (int argc, char **argv)
{
	// return success of application
	return 1;
}

