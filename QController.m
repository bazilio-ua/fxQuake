//
//  QController.m
//  fxQuake
//
//  Created by Basil Nikityuk on 4/15/17.
//  Copyright (c) 2017 fixme. All rights reserved.
//

#import "QController.h"

#include "quakedef.h"
#include "unixquake.h"
#include "macquake.h"

@interface QController ()

@end

@implementation QController

- (void)dealloc {
    [super dealloc];
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    // Insert code here to initialize your application
    NS_DURING {
        [self quakeMain];
    } NS_HANDLER {
        Sys_Error("%@", [localException reason]);
    } NS_ENDHANDLER;
    
    Sys_Quit(0);
}

- (void)applicationDidHide:(NSNotification *)notification {
    
}

- (void)applicationDidUnhide:(NSNotification *)notification {
    
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
    return NSTerminateNow;
}

- (void)quakeMain {
//    Sys_MainLoop();
//    Sys_Main();
    
    int argc = 0;
//    const char **argv;//[MAX_ARGC];
//    char **argv;//[MAX_ARGC];
    
    char *argv[MAX_NUM_ARGVS];

    
    NSProcessInfo *processInfo;
    NSArray *arguments;
    unsigned int argumentIndex, argumentCount;
    
    char *basepath;
	double time, oldtime, newtime;
	quakeparms_t parms;
	int t;
    
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [NSApplication sharedApplication];
    
    [NSApp setServicesProvider:self];
    
    processInfo = [NSProcessInfo processInfo];
    arguments = [processInfo arguments];
    argumentCount = [arguments count];
    for (argumentIndex = 0; argumentIndex < argumentCount; argumentIndex++) {
        NSString *arg;
        
        arg = [arguments objectAtIndex:argumentIndex];
        // Don't pass the Process Serial Number command line arg that the Window Server/Finder invokes us with
        if ([arg hasPrefix: @"-psn_"])
            continue;
        
        argv[argc++] = strdup([arg cStringUsingEncoding:NSASCIIStringEncoding]);
    }
    
    // temporary for debug
	static char cwd[MAX_OSPATH];
    
	getcwd( cwd, sizeof( cwd ) - 1 );
	cwd[MAX_OSPATH-1] = 0;
    NSLog(@"cwd %s\n", cwd);
    
    basepath = (char *)[[[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent] UTF8String];
    NSLog(@"basepath %s\n", basepath);
    if (*basepath && strstr(basepath, cwd)) {
        chdir(basepath);
    }
    // temporary for debug
    
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
    
    //    Sys_MainLoop();
    
    [pool release];
    
    
}

@end
