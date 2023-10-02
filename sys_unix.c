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
// sys_unix.c

#include "quakedef.h"
#include "unixquake.h"
#include "xquake.h"

static qboolean nostdout = false;

qboolean has_smp = false;

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
	char cmd[256];
	char *term;
	char *editor;

	term = getenv("TERM");
	if (term && !strcmp(term, "xterm"))
	{
		editor = getenv("VISUAL");
		if (!editor)
			editor = getenv("EDITOR");
		if (!editor)
			editor = getenv("EDIT");
		if (!editor)
			editor = "vi";
		sprintf(cmd, "xterm -e %s %s", editor, filename);
		system(cmd);
	}
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
    int numcpus;
    
    numcpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (numcpus != -1)
        numcpus = (numcpus < 1) ? 1 : numcpus;
    
    host_parms->numcpus = numcpus;
    has_smp = (numcpus > 1) ? true : false;
    Sys_Printf("System has %d CPU%s.\n", numcpus, has_smp ? "s" : "");
}

void Sys_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		text[MAX_PRINTMSG]; // was 1024 
	byte		*p;

	va_start (argptr, fmt);
	vsnprintf (text, sizeof(text), fmt, argptr);
	va_end (argptr);

	if (strlen(text) > sizeof(text))
		Sys_Error("memory overwrite in Sys_Printf");

	if (nostdout)
		return;

	for (p = (byte *)text; *p; p++) 
	{
		*p &= 0x7f;
		if ((*p > 128 || *p < 32) && *p != 10 && *p != 13 && *p != 9)
			printf("[%02x]", *p);
//			printf("*");
		else
			putc(*p, stdout);
	}

	// rcon (64 doesn't mean anything special, but we need some extra space because NET_MAXMESSAGE == RCON_BUFF_SIZE)
	if (rcon_active && (rcon_message.cursize < rcon_message.maxsize - (int)strlen(text) - 64))
	{
		rcon_message.cursize--;
		MSG_WriteString(&rcon_message, text);
	}
}

void Sys_Error (char *error, ...)
{ 
	va_list     argptr;
	char        string[MAX_PRINTMSG]; // was 1024
//    static int    in_sys_error0 = 0;
//    static int    in_sys_error1 = 0;
    static int    in_sys_error2 = 0;
    static int    in_sys_error3 = 0;

	host_parms->errstate++;

// change stdin to non blocking
	fcntl (STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) & ~O_NONBLOCK);

	va_start (argptr, error);
	vsnprintf (string, sizeof(string), error, argptr);
	va_end (argptr);

    Con_Printf ("Quake Error: %s\n", string); // write to console log as well

    if (!in_sys_error2)
    {
        in_sys_error2 = 1;
        Host_Shutdown ();
    }
    
    if (!in_sys_error3)
    {
        in_sys_error3 = 1;
        Sys_Shutdown ();
    }

	exit (1);
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
	static char text[256];
	int     len;
	fd_set	fdset;
	struct timeval timeout;

	if (cls.state != ca_dedicated) 
		return NULL;

	FD_ZERO(&fdset);
	FD_SET(STDIN_FILENO, &fdset);	// stdin
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	if (select(STDIN_FILENO + 1, &fdset, NULL, NULL, &timeout) == -1 || !FD_ISSET(STDIN_FILENO, &fdset))
		return NULL;

	len = read(STDIN_FILENO, text, sizeof(text));
	if (len < 1)
		return NULL;

	text[len - 1] = 0;    // rip off the /n and terminate

	return text;
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
	Window owner;
	Atom type, property;
	unsigned long len, bytes_left, tmp;
	byte *data;
	int format, result;
	char *clipboard = NULL;
	char *cliptext;
	
	owner = XGetSelectionOwner(x_disp, XA_PRIMARY);

	if (owner != None)
	{
		property = XInternAtom(x_disp, "GETCLIPBOARDDATA_PROP", False);
		XConvertSelection(x_disp, XA_PRIMARY, XA_STRING, property, x_win, CurrentTime);

		XFlush(x_disp);

		XGetWindowProperty (
			x_disp,
			x_win, property,
			0, 0, False, AnyPropertyType,
			&type, &format, &len,
			&bytes_left, &data
		);

		if (bytes_left > 0)
		{
			result = XGetWindowProperty (
				x_disp,
				x_win, property,
				0, bytes_left, True, AnyPropertyType,
				&type, &format, &len,
				&tmp, &data
			);

			if (result == Success)
			{
				size_t size;
				
				cliptext = (char *)data;
				size = strlen(cliptext) + 1;
				/* this is intended for simple small text copies
				 * such as an ip address, etc:  do chop the size
				 * here, otherwise we may experience Z_Malloc()
				 * failures and all other not-oh-so-fun stuff. */
				size = min(SYS_CLIPBOARD_SIZE, size);
				clipboard = Z_Malloc (size);
				strcpy (clipboard, cliptext);
			}
			
			XFree(data);
		}
	}
	
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
static char qcwd[MAX_OSPATH];
static char basepath[MAX_OSPATH];

int main (int argc, char **argv)
{
	double time, oldtime, newtime;
	quakeparms_t parms;
	int t;
    char *c;

    strncpy(basepath, argv[0], sizeof(basepath));
    c = (char *)basepath;
    while (*c != '\0')  /* go to end */
        c++;
    while (*c != '/')   /* back up to parent */
        c--;
    *c++ = '\0';        /* cut off last part (binary name) */

    if (chdir(basepath) == -1)
        Sys_Error ("Couldn't change to directory: %s", basepath);
    if (getcwd(qcwd, sizeof(qcwd)) == NULL)
        Sys_Error ("Couldn't determine current directory");
    else
        Sys_Printf("Current directory: %s\n", qcwd);

	signal(SIGFPE, SIG_IGN);

	memset(&parms, 0, sizeof(parms));

    host_parms = &parms;

	COM_InitArgv (argc, argv);
	parms.argc = com_argc;
	parms.argv = com_argv;

    parms.errstate = 0;

    Sys_Printf ("Starting Quake...\n");
    
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
//	Host_Init (&parms);
	Host_Init ();

	// Make stdin non-blocking
	if (!nostdout)
	{
		fcntl (STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) | O_NONBLOCK);
		printf ("fxQuake %4.2f\n", (float)VERSION);
	}

//	oldtime = Sys_DoubleTime () - 0.1;
    oldtime = Sys_DoubleTime();
	// main message loop
	while (1)
	{
		// find time spent rendering last frame
		newtime = Sys_DoubleTime();
		time = newtime - oldtime;

		if (cls.state == ca_dedicated)
		{
			if (time < sys_ticrate.value)
			{
				Sys_Sleep ();
				continue; // not time to run a server only tic yet
			}
//			time = sys_ticrate.value;
		}
		else
		{
			// yield the CPU for a little while when minimized, not the focus or blocked for drawing
			if (!vid_activewindow || vid_hiddenwindow || block_drawing)
				Sys_Sleep (); // Prevent CPU hogging
		}

//		if (time > sys_ticrate.value * 2)
//			oldtime = newtime;
//		else
//			oldtime += time;

		Host_Frame (time);
        
        if (time < sys_throttle.value && !cls.timedemo)
            Sys_Sleep();
        
        oldtime = newtime;
	}

	// return success of application
	return 1;
}

