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
void Sys_ScanDirFileList (char *path, char *subdir, char *ext, qboolean stripext, filelist_t **list)
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
void Sys_EditFile (char *filename)
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
void Sys_Init (void)
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
    
    host_parms->errstate++;

    // change stdin to non blocking
	fcntl (STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) & ~O_NONBLOCK);
    
	va_start (argptr, error);
	vsnprintf (string, sizeof(string), error, argptr);
	va_end (argptr);
    
	fprintf (stderr, "Quake Error: %s\n", string);
    
    NSString *message = [NSString stringWithCString:string encoding:NSASCIIStringEncoding];
    NSLog(@"Quake Error: %@", message);
    
	Host_Shutdown ();
    
	Sys_Shutdown ();
    
    NSRunCriticalAlertPanel(@"Quake Error", message, @"OK", nil, nil);
    
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
char *Sys_ConsoleInput (void)
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
	char *clipboard = NULL;
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSArray *types = [pasteboard types];
    
    if ([types containsObject:NSStringPboardType]) {
        NSString *clipboardText = [pasteboard stringForType:NSStringPboardType];
        NSUInteger length = [clipboardText length];
        const char *string = [clipboardText cStringUsingEncoding:NSASCIIStringEncoding];
        if (string && length > 0) {
            size_t size = length + 1;
            size = min(SYS_CLIPBOARD_SIZE, size);
            clipboard = (char *)Z_Malloc(size);
            strcpy(clipboard, string);
        }
    }
	
	return clipboard;
} 

void Sys_Sleep (void)
{
    [NSThread sleepForTimeInterval:0.001];
}

/*
================
main
================
*/
//char *qbasedir = ".";
//char *qcachedir = "/tmp";

int main (int argc, char *argv[])
{
    return NSApplicationMain(argc, (const char **)argv);
}

//
// Quake Controller
//
@interface QController (Private)

- (void)quakeMain;
- (void)checkActive;

@end

@implementation QController

- (void)dealloc {
    [super dealloc];
}

- (void)quakeMain {
    int argc = 0;
    char *argv[MAX_NUM_ARGVS];
    
	double time, oldtime, newtime;
	quakeparms_t parms;
	int t;
    
    NSProcessInfo *processInfo = [NSProcessInfo processInfo];
    NSArray *arguments = [processInfo arguments];
    NSUInteger argumentCount = [arguments count];
    for (NSUInteger argumentIndex = 0; argumentIndex < argumentCount; argumentIndex++) {
        NSString *arg = [arguments objectAtIndex:argumentIndex];
        // Don't pass the Process Serial Number command line arg that the Window Server/Finder invokes us with
        if ([arg hasPrefix: @"-psn_"])
            continue;
        
        argv[argc++] = strdup([arg cStringUsingEncoding:NSASCIIStringEncoding]);
    }
    
    NSString *basepath = [[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager changeCurrentDirectoryPath:basepath];
    
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
}

- (void)checkActive {
    static qboolean active = true;
    
    if (vidmode_fullscreen)
    {
        if (!vid_hiddenwindow)
        {
            // set our video mode
            
            // move the viewport to top left
        }
        else if (vid_hiddenwindow)
        {
            // set our video mode
        }
    }
    else //if (!vidmode_fullscreen)
    {
        // enable/disable sound, set/restore gamma and grab/ungrab keyb
        // on focus gain/loss
        if (vid_activewindow && !vid_hiddenwindow)// && !active)
        {
            if (!active) {
            CDAudio_Resume ();
            S_UnblockSound ();
            S_ClearBuffer ();
            VID_Gamma_Set ();
//            NSLog(@"*** Active ***");
            active = true;
            }
        }
        else //if (active)
        {
            if (active) {
            CDAudio_Pause ();
            S_BlockSound ();
            S_ClearBuffer ();
            VID_Gamma_Restore ();
            Key_ClearStates ();
//            NSLog(@"*** Inactive ***");
            active = false;
            }
        }
    }
    
    // fix the leftover Alt from any Alt-Tab or the like that switched us away
//    Key_ClearStates ();
}

/* <NSWindowDelegate> */

- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)frameSize {
    NSRect windowFrame = [sender frame];
    NSRect contentFrame = [[sender contentView] frame];
    NSSize titleSize;
    
    titleSize.width = windowFrame.size.width - contentFrame.size.width;
    titleSize.height = windowFrame.size.height - contentFrame.size.height;
    
    frameSize.width -= titleSize.width;
    frameSize.height -= titleSize.height;
    
    CGFloat window_width = frameSize.width;
    CGFloat window_height = frameSize.height;
    
    // check for resize
    if (window_width < 320)
        window_width = 320;
    if (window_height < 200)
        window_height = 200;
    
    vid.width = window_width;
    vid.height = window_height;
    
    vid.conwidth = vid.width;
    vid.conheight = vid.height;
    
    vid.recalc_refdef = true; // force a surface cache flush
    
    frameSize.width = window_width;
    frameSize.height = window_height;
    
    frameSize.width += titleSize.width;
    frameSize.height += titleSize.height;

    return frameSize;
}

- (void)windowDidResize:(NSNotification *)notification {
    [glcontext update];
	GL_UploadWarpImage();
    
    Host_Frame(0.02);
}

- (void)windowDidChangeScreen:(NSNotification *)notification {
    VID_Gamma_Shutdown();
    
    NSScreen *currentScreen = [[notification object] screen];
    CGDirectDisplayID currentDisplay = [[[currentScreen deviceDescription] objectForKey:@"NSScreenNumber"] unsignedIntValue];
    
    display = currentDisplay;
    
    VID_Gamma_Init();
    VID_Gamma_Set();
}

- (BOOL)windowShouldClose:(id)sender {
    [NSApp terminate:nil];
    
    return NO;
}

- (void)windowWillClose:(NSNotification *)notification {
    
}

- (void)windowDidBecomeKey:(NSNotification *)notification {
    if (!vid_activewindow) {
        vid_activewindow = true;
        
        [self checkActive];
    }
}

- (void)windowDidResignKey:(NSNotification *)notification {
    if (vid_activewindow) {
        vid_activewindow = false;
        
        [self checkActive];
    }
}

- (void)windowWillMiniaturize:(NSNotification *)notification {
    if (!vid_hiddenwindow) {
        vid_hiddenwindow = true;
        
        [self checkActive];
    }
}

- (void)windowDidDeminiaturize:(NSNotification *)notification {
    if (vid_hiddenwindow) {
        vid_hiddenwindow = false;
        
        [self checkActive];
    }
}

/* <NSApplicationDelegate> */

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    [NSApp setServicesProvider:self];
    [NSApp activateIgnoringOtherApps:YES];
    
    [self quakeMain];
}

- (void)applicationWillHide:(NSNotification *)notification {
    if (!vid_hiddenwindow && vid_activewindow) {
        vid_hiddenwindow = true;
        vid_activewindow = false;
        
        [self checkActive];
    }
}

- (void)applicationDidUnhide:(NSNotification *)notification {
    if (vid_hiddenwindow && !vid_activewindow) {
        vid_hiddenwindow = false;
        vid_activewindow = true;
        
        [self checkActive];
    }
}

- (void)applicationWillResignActive:(NSNotification *)notification {
    if (vid_activewindow) {
        vid_activewindow = false;
        
        [self checkActive];
    }
}

- (void)applicationDidBecomeActive:(NSNotification *)notification {
    if (!vid_activewindow) {
        vid_activewindow = true;
        
        [self checkActive];
    }
}

- (void)applicationWillTerminate:(NSNotification *)notification {
    
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
    NSApplicationTerminateReply terminateReply = NSTerminateNow;
    
    if (host_initialized) {
        if ([NSApp isHidden] || ![NSApp isActive]) {
            [NSApp activateIgnoringOtherApps:YES];
        }
        
        if (window) {
            if ([window isMiniaturized]) {
                [window deminiaturize:nil];
            }
            [window orderFront:nil];
        }
        
        if (cls.state == ca_dedicated) {
            Sys_Quit(0);
        } else {
            M_Menu_Quit_f();
            terminateReply = NSTerminateCancel;
        }
    }
    
    return terminateReply;
}

@end

