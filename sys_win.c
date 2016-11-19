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
// sys_win.c -- Win32 system interface code

#include "quakedef.h"
#include "winquake.h"
#include "resource.h"
#include "conproc.h"

#define CONSOLE_ERROR_TIMEOUT	60.0	// # of seconds to wait on Sys_Error running
										//  dedicated before exiting

int			starttime;
qboolean	vid_activewindow;
qboolean	vid_hiddenwindow;
qboolean	WinNT;

static double		pfreq;
static qboolean		hwtimer = false;

static qboolean		sc_return_on_enter = false;
HANDLE				hinput, houtput;

static HANDLE	tevent;
static HANDLE	hFile;
static HANDLE	heventParent;
static HANDLE	heventChild;

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
	if (CreateDirectory(path, NULL) != 0)
		return;
	if (GetLastError() != ERROR_ALREADY_EXISTS)
		Sys_Error("Unable to create directory %s", path);
}

/*
================
Sys_ScanDirFileList
================
*/
void Sys_ScanDirFileList(char *path, char *subdir, char *exts[], qboolean stripext, filelist_t **list)
{
	WIN32_FIND_DATA	data;
	HANDLE		handle;
	char		filename[32];
	char		filestring[MAX_OSPATH];
	int 		i, len;
	
	len = sizeof(exts) / sizeof(exts[0]);
	for (i=0 ; i<len && exts[i] != NULL ; i++)
	{
		char *ext = exts[i];
		snprintf (filestring, sizeof(filestring), "%s/%s*.%s", path, subdir ? subdir : "", ext);
		handle = FindFirstFile(filestring, &data);
		if (handle == INVALID_HANDLE_VALUE)
			return;
		do
		{
			if (stripext)
				COM_StripExtension(data.cFileName, filename);
			else
				strcpy(filename, data.cFileName);
			
			COM_FileListAdd (filename, list);
		}
		while (FindNextFile(handle, &data));
		FindClose(handle);
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
void Sys_Init (void)
{
	OSVERSIONINFO	vinfo;

	Sys_InitDoubleTime ();

	vinfo.dwOSVersionInfoSize = sizeof(vinfo);

	if (!GetVersionEx (&vinfo))
		Sys_Error ("Couldn't get OS info");

	if ((vinfo.dwMajorVersion < 4) ||
		(vinfo.dwPlatformId == VER_PLATFORM_WIN32s))
	{
		Sys_Error ("fxQuake requires at least Win95 or NT 4.0");
	}

	if (vinfo.dwPlatformId == VER_PLATFORM_WIN32_NT)
		WinNT = true;
	else
		WinNT = false;
}

void Sys_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		text[MAX_PRINTMSG]; // was 1024 
	DWORD		dummy;
	
	if (cls.state == ca_dedicated)
	{
		va_start (argptr, fmt);
		vsnprintf (text, sizeof(text), fmt, argptr);
		va_end (argptr);

		WriteFile(houtput, text, strlen (text), &dummy, NULL);

		// rcon (64 doesn't mean anything special, but we need some extra space because NET_MAXMESSAGE == RCON_BUFF_SIZE)
		if (rcon_active && (rcon_message.cursize < rcon_message.maxsize - (int)strlen(text) - 64))
		{
			rcon_message.cursize--;
			MSG_WriteString(&rcon_message, text);
		}
	}
}

void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[MAX_PRINTMSG], text2[MAX_PRINTMSG]; // was 1024 
	char		*text3 = "Press Enter to exit\n";
	char		*text4 = "***********************************\n";
	char		*text5 = "\n";
	DWORD		dummy;
	double		starttime;
	static int	in_sys_error0 = 0;
	static int	in_sys_error1 = 0;
	static int	in_sys_error2 = 0;
	static int	in_sys_error3 = 0;

	va_start (argptr, error);
	vsnprintf (text, sizeof(text), error, argptr);
	va_end (argptr);

	Con_Printf ("Quake Error: %s\n", text); // write to console log as well

	if (cls.state == ca_dedicated && host_initialized)
	{
		snprintf (text2, sizeof(text2), "QUAKE ERROR: %s\n", text);
		if (text2[sizeof(text2) - 2])
			strcpy (text2 + sizeof(text2) - 2, "\n"); // in case we truncated

		WriteFile (houtput, text5, strlen (text5), &dummy, NULL);
		WriteFile (houtput, text4, strlen (text4), &dummy, NULL);
		WriteFile (houtput, text2, strlen (text2), &dummy, NULL);
		WriteFile (houtput, text3, strlen (text3), &dummy, NULL);
		WriteFile (houtput, text4, strlen (text4), &dummy, NULL);

		starttime = Sys_DoubleTime ();
		sc_return_on_enter = true;	// so Enter will get us out of here

		while (!Sys_ConsoleInput () &&
				((Sys_DoubleTime () - starttime) < CONSOLE_ERROR_TIMEOUT))
		{
		}
	}
	else
	{
		qboolean NoMsgBox = COM_CheckParm ("-nomsgbox") != 0;

		// Prevent screen updates, otherwise secondary faults might
		// occur and mask the real error
		block_drawing = true;

		S_ClearBuffer (); // Avoid looping sounds

		// switch to windowed so the message box is visible, unless we already
		// tried that and failed
		if (!in_sys_error0)
		{
			in_sys_error0 = 1;
			IN_DeactivateMouse ();
			IN_ShowMouse ();
		}

		if (!in_sys_error1)
		{
			in_sys_error1 = 1;

			if (!NoMsgBox)
				MessageBox(NULL, text, "Quake Error", MB_OK | MB_SETFOREGROUND | MB_ICONSTOP);
		}
		else
		{
			if (!NoMsgBox)
				MessageBox(NULL, text, "Double Quake Error", MB_OK | MB_SETFOREGROUND | MB_ICONSTOP);
		}

		block_drawing = false; // Make sure to restore
	}

	if (!in_sys_error2)
	{
		in_sys_error2 = 1;
		Host_Shutdown ();
	}

// shut down QHOST hooks if necessary
	if (!in_sys_error3)
	{
		in_sys_error3 = 1;
		Sys_Shutdown ();
	}

	exit (1);
}

void Sys_Shutdown (void)
{
	if (tevent)
		CloseHandle (tevent);

	if (cls.state == ca_dedicated)
		FreeConsole ();

// shut down QHOST hooks if necessary
	DeinitConProc ();
}

void Sys_Quit (int code)
{
	Host_Shutdown ();

	Sys_Shutdown ();

	exit (code);
}


/*
================
Sys_InitDoubleTime
================
*/
void Sys_InitDoubleTime (void)
{
	__int64	freq;

	if (!COM_CheckParm("-nohwtimer") && QueryPerformanceFrequency ((LARGE_INTEGER *)&freq) && freq > 0)
	{
		// hardware timer available
		pfreq = (double)freq;
		hwtimer = true;
	}
	else
	{
		// make sure the timer is high precision, otherwise NT gets 18ms resolution
		timeBeginPeriod (1);
	}
}

/*
================
Sys_DoubleTime
================
*/
double Sys_DoubleTime (void)
{
	__int64		pcount;
	static	__int64	startcount;
	static	DWORD	starttime;
	static qboolean	first = true;
	DWORD	now;

	if (hwtimer)
	{
		QueryPerformanceCounter ((LARGE_INTEGER *)&pcount);
		if (first)
		{
			first = false;
			startcount = pcount;
			return 0.0;
		}
		// TODO: check for wrapping
		return (pcount - startcount) / pfreq;
	}

	now = timeGetTime ();

	if (first)
	{
		first = false;
		starttime = now;
		return 0.0;
	}

	if (now < starttime) // wrapped?
		return (now / 1000.0) + (LONG_MAX - starttime / 1000.0);

	if (now - starttime == 0)
		return 0.0;

	return (now - starttime) / 1000.0;
}

/*
================
Sys_ConsoleInput
================
*/
char *Sys_ConsoleInput (void)
{
	static char	text[256];
	static int		len;
	INPUT_RECORD	recs[1024];
	DWORD	dummy;
	int		ch;
	DWORD	numread, numevents;

	if (cls.state != ca_dedicated)
		return NULL;

	for ( ;; )
	{
		if (!GetNumberOfConsoleInputEvents (hinput, &numevents))
		{
			cls.state = ca_disconnected;
			Sys_Error ("Error getting # of console events (error code %x)", (unsigned int)GetLastError());
		}

		if (numevents <= 0)
			break;

		if (!ReadConsoleInput(hinput, recs, 1, &numread))
		{
			cls.state = ca_disconnected;
			Sys_Error ("Error reading console input (error code %x)", (unsigned int)GetLastError());
		}

		if (numread != 1)
		{
			cls.state = ca_disconnected;
			Sys_Error ("Couldn't read console input (error code %x)", (unsigned int)GetLastError());
		}

		if (recs[0].EventType == KEY_EVENT)
		{
			if (!recs[0].Event.KeyEvent.bKeyDown)
			{
				ch = recs[0].Event.KeyEvent.uChar.AsciiChar;
				switch (ch)
				{
				case '\r':
					WriteFile(houtput, "\r\n", 2, &dummy, NULL);	
					if (len)
					{
						text[len] = 0;
						len = 0;
						return text;
					}
					else if (sc_return_on_enter)
					{
					// special case to allow exiting from the error handler on Enter
						text[0] = '\r';
						len = 0;
						return text;
					}
					break;
					
				case '\b':
					WriteFile(houtput, "\b \b", 3, &dummy, NULL);	
					if (len)
					{
						len--;
					}
					break;
					
				default:
					if (ch >= ' ')
					{
						WriteFile(houtput, &ch, 1, &dummy, NULL);	
						text[len] = ch;
						len = (len + 1) & 0xff;
					}
					break;
				}
			}
		}
	}

	return NULL;
}

/*
================
Sys_GetClipboardData

Clipboard function
================
*/
#define	SYS_CLIPBOARD_SIZE	MAX_CMDLINE		// 256

char *Sys_GetClipboardData (void)
{
	char *clipboard = NULL;
	char *cliptext;
	
	if (OpenClipboard(NULL) != 0)
	{
		HANDLE hClipboardData;
		
		if ((hClipboardData = GetClipboardData(CF_TEXT)) != NULL)
		{
			cliptext = (char *)GlobalLock(hClipboardData);
			if (cliptext != NULL)
			{
				size_t size = GlobalSize(hClipboardData) + 1;
				/* this is intended for simple small text copies
				 * such as an ip address, etc:  do chop the size
				 * here, otherwise we may experience Z_Malloc()
				 * failures and all other not-oh-so-fun stuff. */
				size = min(SYS_CLIPBOARD_SIZE, size);
				clipboard = Z_Malloc(size);
				strcpy (clipboard, cliptext);
				GlobalUnlock (hClipboardData);
			}
		}
		CloseClipboard ();
	}
	
	return clipboard; 	
} 

void Sys_Sleep (void)
{
	Sleep (1);
}

/*
==============================================================================

 WINDOWS CRAP

==============================================================================
*/

/*
================
Sys_PageIn
================
*/
volatile int					sys_checksum;

void Sys_PageIn (void *ptr, int size)
{
	byte	*x;
	int		m, n;

// touch all the memory to make sure it's there. The 16-page skip is to
// keep Win 95 from thinking we're trying to page ourselves in (we are
// doing that, of course, but there's no reason we shouldn't)
	x = (byte *)ptr;

	for (n=0 ; n<4 ; n++)
	{
		for (m=0 ; m<(size - 16 * 0x1000) ; m += 4)
		{
			sys_checksum += *(int *)&x[m];
			sys_checksum += *(int *)&x[m + 16 * 0x1000];
		}
	}
}

/*
==================
WinMain
==================
*/
HINSTANCE	global_hInstance;
int			global_nCmdShow;
char		*argv[MAX_NUM_ARGVS];
static	char	*empty_string = "";
static	char	qcwd[MAX_OSPATH];
HWND		hwnd_dialog;

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	quakeparms_t	parms;
	double			time, oldtime, newtime;

	int				t;
	RECT			rect;

	// previous instances do not exist in Win32
	if (hPrevInstance)
		return 0;

	global_hInstance = hInstance;
	global_nCmdShow = nCmdShow;

	if (!GetCurrentDirectory (sizeof(qcwd), qcwd))
		Sys_Error ("Couldn't determine current directory");

	if (qcwd[strlen(qcwd)-1] == '/')
		qcwd[strlen(qcwd)-1] = 0;

	parms.basedir = qcwd;
	parms.cachedir = NULL;

	parms.argc = 1;
	argv[0] = empty_string;

	while (*lpCmdLine && (parms.argc < MAX_NUM_ARGVS))
	{
		while (*lpCmdLine && ((*lpCmdLine <= 32) || (*lpCmdLine > 126)))
			lpCmdLine++;

		if (*lpCmdLine)
		{
			argv[parms.argc] = lpCmdLine;
			parms.argc++;

			while (*lpCmdLine && ((*lpCmdLine > 32) && (*lpCmdLine <= 126)))
				lpCmdLine++;

			if (*lpCmdLine)
			{
				*lpCmdLine = 0;
				lpCmdLine++;
			}
			
		}
	}

	parms.argv = argv;

	COM_InitArgv (parms.argc, parms.argv);

	parms.argc = com_argc;
	parms.argv = com_argv;

	if (!COM_CheckParm ("-dedicated"))
	{
		hwnd_dialog = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, NULL);

		if (hwnd_dialog)
		{
			RECT area;
			if (SystemParametersInfo (SPI_GETWORKAREA, 0, &area, 0) && GetWindowRect (hwnd_dialog, &rect) && AdjustWindowRectEx(&rect, 0, FALSE, 0))
			{
				// center it properly in the working area (don't assume that top and left are 0!!!)
				SetWindowPos (hwnd_dialog, NULL, 
					area.left + ((area.right - area.left) - (rect.right - rect.left)) / 2, 
					area.top + ((area.bottom - area.top) - (rect.bottom - rect.top)) / 2, 
					0,
					0,
					SWP_NOZORDER | SWP_NOSIZE);
			}

			ShowWindow (hwnd_dialog, SW_SHOWDEFAULT);
			UpdateWindow (hwnd_dialog);
			SetForegroundWindow (hwnd_dialog);
		}
	}

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

	if(!COM_CheckParm ("-nopagein"))
	{
		Sys_PageIn (parms.membase, parms.memsize);
	}

	// initialize the windows dedicated server console if needed
	tevent = CreateEvent(NULL, FALSE, FALSE, NULL);

	if (!tevent)
		Sys_Error ("Couldn't create event");

	if (COM_CheckParm ("-dedicated"))
	{
		if (!AllocConsole ())
			Sys_Error ("Couldn't create dedicated server console (error code %x)", (unsigned int)GetLastError());

		houtput = GetStdHandle (STD_OUTPUT_HANDLE);
		hinput = GetStdHandle (STD_INPUT_HANDLE);

		if ((houtput == 0) || (houtput == INVALID_HANDLE_VALUE))
			Sys_Error ("Couldn't create dedicated server console");

	// give QHOST a chance to hook into the console
		if ((t = COM_CheckParm ("-HFILE")) > 0)
		{
			if (t < com_argc)
				hFile = (HANDLE)atoi (com_argv[t+1]);
		}

		if ((t = COM_CheckParm ("-HPARENT")) > 0)
		{
			if (t < com_argc)
				heventParent = (HANDLE)atoi (com_argv[t+1]);
		}

		if ((t = COM_CheckParm ("-HCHILD")) > 0)
		{
			if (t < com_argc)
				heventChild = (HANDLE)atoi (com_argv[t+1]);
		}

		InitConProc (hFile, heventParent, heventChild);
	}

	Sys_Init ();

// because sound is off until we become active
	S_BlockSound ();

	Sys_Printf ("Host init started\n");
	Host_Init (&parms);

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

