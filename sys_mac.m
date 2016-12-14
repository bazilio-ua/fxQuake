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
#include "unixquake.h"
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
	int rc = mkdir (path, 0777);
	if (rc != 0 && errno == EEXIST)
		rc = 0;
	if (rc != 0)
	{
		rc = errno;
		Sys_Error("Unable to create directory %s: %s", path, strerror(rc));
	} 
}

/*
================
Sys_ScanDirFileList
================
*/
void Sys_ScanDirFileList(char *path, char *subdir, char *ext, qboolean stripext, filelist_t **list)
{
	DIR		*dir_p;
	struct dirent	*dir_t;
	char		filename[32];
	char		filestring[MAX_OSPATH];
	
	snprintf (filestring, sizeof(filestring), "%s/%s", path, subdir);
	dir_p = opendir(filestring);
	if (dir_p == NULL)
		return;
	
	while ((dir_t = readdir(dir_p)) != NULL)
	{
		if (!strcasecmp(COM_FileExtension(dir_t->d_name), ext))
		{
			if (stripext)
				COM_StripExtension(dir_t->d_name, filename);
			else
				strcpy(filename, dir_t->d_name);
			
			COM_FileListAdd (filename, list);
		}
	}
	
	closedir(dir_p);
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
	fcntl (STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) & ~O_NONBLOCK);
    
	fflush (stdout);
}

void Sys_Quit (int code)
{
	Host_Shutdown ();
    
	Sys_Shutdown ();
    
	exit (code);
}


/*
================
Sys_DoubleTime
================
*/
double Sys_DoubleTime (void)
{
	struct timeval tp;
	struct timezone tzp; 
	static int      secbase; 
    
	gettimeofday(&tp, &tzp);  
    
	if (!secbase)
	{
		secbase = tp.tv_sec;
		return tp.tv_usec / 1000000.0;
	}
    
	return (tp.tv_sec - secbase) + tp.tv_usec / 1000000.0;
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
	usleep (1);
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
	double time, oldtime, newtime;
	quakeparms_t parms;
	int t;
    
	signal(SIGFPE, SIG_IGN);
    
	memset(&parms, 0, sizeof(parms));
    
	COM_InitArgv (argc, argv);
	parms.argc = com_argc;
	parms.argv = com_argv;
    
	parms.memsize = DEFAULT_MEMORY_SIZE * 1024 * 1024;
    
	if (COM_CheckParm ("-heapsize"))
	{
		t = COM_CheckParm("-heapsize") + 1;
        
		if (t < com_argc)
			parms.memsize = atoi (com_argv[t]) * 1024;
	}
	else if (COM_CheckParm ("-mem"))
	{
		t = COM_CheckParm("-mem") + 1;
        
		if (t < com_argc)
			parms.memsize = atoi (com_argv[t]) * 1024 * 1024;
	}
    
	parms.membase = malloc (parms.memsize);
    
	if (!parms.membase)
		Sys_Error ("Not enough memory free, check disk space");
	
    //	parms.basedir = qbasedir;
    // caching is disabled by default, use -cachedir to enable
    //	parms.cachedir = qcachedir;
	parms.basedir = stringify(QBASEDIR); 
	parms.cachedir = NULL;
    
	if (COM_CheckParm("-nostdout"))
		nostdout = true;
    
	Sys_Init();
    
	Sys_Printf ("Host init started\n");
	Host_Init (&parms);
    
	// Make stdin non-blocking
	if (!nostdout)
	{
		fcntl (STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) | O_NONBLOCK);
		printf ("fxQuake %4.2f\n", (float)VERSION);
	}
    
	oldtime = Sys_DoubleTime () - 0.1;
	// main message loop
	while (1)
	{
		// find time spent rendering last frame
		newtime = Sys_DoubleTime ();
		time = newtime - oldtime;
        
		if (cls.state == ca_dedicated)
		{
			if (time < sys_ticrate.value)
			{
				Sys_Sleep ();
				continue; // not time to run a server only tic yet
			}
			time = sys_ticrate.value;
		}
		else
		{
			// yield the CPU for a little while when minimized, not the focus or blocked for drawing
			if (!vid_activewindow || vid_hiddenwindow || block_drawing)
				Sys_Sleep (); // Prevent CPU hogging
		}
        
		if (time > sys_ticrate.value * 2)
			oldtime = newtime;
		else
			oldtime += time;
        
		Host_Frame (time);
	}
    
	// return success of application
	return 1;
}

