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
// gl_screen.c -- master for refresh, status bar, console, chat, notify, etc

#include "quakedef.h"

/*

background clear
rendering
turtle/net/ram icons
sbar
centerprint / slow centerprint
notify lines
intermission / finale overlay
loading plaque
console
menu

required background clears
required update regions


syncronous draw mode or async
One off screen buffer, with updates either copied or xblited
Need to double buffer?


async draw will require the refresh area to be cleared, because it will be
xblited, but sync draw can just ignore it.

sync
draw

CenterPrint ()
SlowPrint ()
Screen_Update ();
Con_Printf ();

net 
turn off messages option

the refresh is always rendered, unless the console is full screen


console is:
	notify lines
	half
	full

*/


int			glx, gly, glwidth, glheight;

float		scr_con_current;
float		scr_conlines;		// lines of console to display

float		oldscreensize, oldfov, oldsbar, oldoverdrawsbar, oldweaponsize, oldweaponfov;
cvar_t		scr_viewsize = {"viewsize","100", CVAR_ARCHIVE};
cvar_t		scr_weaponsize = {"weaponsize","100", CVAR_ARCHIVE};
cvar_t		scr_fov = {"fov","90", CVAR_NONE};	// 10 - 170
cvar_t		scr_weaponfov = {"weaponfov","90", CVAR_NONE};	// 10 - 170
cvar_t		scr_conspeed = {"scr_conspeed","5000", CVAR_NONE}; //300
cvar_t		scr_centertime = {"scr_centertime","2", CVAR_NONE};
cvar_t		scr_showfps = {"scr_showfps", "0", CVAR_NONE};
cvar_t		scr_showstats = {"scr_showstats", "0", CVAR_NONE};
cvar_t		scr_showturtle = {"showturtle","0", CVAR_NONE};
cvar_t		scr_showpause = {"showpause","1", CVAR_NONE};
cvar_t		scr_printspeed = {"scr_printspeed","8", CVAR_NONE};
cvar_t		gl_triplebuffer = {"gl_triplebuffer", "1", CVAR_ARCHIVE};

qboolean	scr_initialized;		// ready to draw

qpic_t		*scr_ram;
qpic_t		*scr_net;
qpic_t		*scr_turtle;

extern	qpic_t	*sb_colon;
extern	qpic_t	*sb_slash;
extern	qpic_t	*sb_nums[2][11];

int			clearconsole;

int			sb_lines;

viddef_t	vid;				// global video state

vrect_t		scr_vrect;

qboolean	scr_disabled_for_loading;
qboolean	scr_drawloading;
float		scr_disabled_time;

#define SCR_DEFTIMEOUT 60

static float	scr_timeout;

qboolean	block_drawing;

void SCR_ScreenShot_f (void);

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

char		scr_centerstring[1024];
float		scr_centertime_start;	// for slow victory printing
float		scr_centertime_off;
int			scr_center_lines;
int			scr_erase_lines;
int			scr_erase_center;

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void SCR_CenterPrint (char *str)
{
	// the server sends a blank centerstring in some places.
	if (!str[0])
	{
		// an empty print is sometimes used to explicitly clear the previous centerprint
		con_lastcenterstring[0] = 0;
		scr_centertime_off = 0;
		return;
	}

	// only log if the previous centerprint has already been cleared
	Con_LogCenterPrint (str);

	strncpy (scr_centerstring, str, sizeof(scr_centerstring)-1);
	scr_centertime_off = scr_centertime.value;
	scr_centertime_start = cl.time;

// count the number of lines for centering
	scr_center_lines = 1;
	while (*str)
	{
		if (*str == '\n')
			scr_center_lines++;
		str++;
	}
}


void SCR_DrawCenterString (void)
{
	char	*start;
	int		l;
	int		j;
	int		x, y;
	int		remaining;

// the finale prints the characters one at a time
	if (cl.intermission)
		remaining = scr_printspeed.value * (cl.time - scr_centertime_start);
	else
		remaining = 9999;

	scr_erase_center = 0;
	start = scr_centerstring;

	if (scr_center_lines <= 4)
		y = vid.height*0.35;
	else
		y = 48;

	do	
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.width - l*8)/2;
		for (j=0 ; j<l ; j++, x+=8)
		{
			Draw_Character (x, y, start[j]);	
			if (!remaining--)
				return;
		}
			
		y += 8;

		if (y >= (int)vid.height)
			break; // Outside screen

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;		// skip the \n
	} while (1);
}

void SCR_CheckDrawCenterString (void)
{
	if (scr_center_lines > scr_erase_lines)
		scr_erase_lines = scr_center_lines;

	scr_centertime_off -= host_frametime;
	
	if (scr_centertime_off <= 0 && !cl.intermission)
		return;
	if (key_dest != key_game)
		return;
	if (cl.paused) //johnfitz -- don't show centerprint during a pause
		return;

	SCR_DrawCenterString ();
}

//=============================================================================

/*
====================
AdaptFovx
Adapt a 4:3 horizontal FOV to the current screen size using the "Hor+" scaling:
2.0 * atan(width / height * 3.0 / 4.0 * tan(fov_x / 2.0))
====================
*/
#define FOV_ASPECT 0.75
float AdaptFovx (float fov_x, float width, float height)
{
	float	a, x;

	if (fov_x < 1 || fov_x > 179)
		Host_Error ("Bad fov: %f", fov_x);

	if ((x = height / width) == FOV_ASPECT)
		return fov_x;
	a = atan(FOV_ASPECT / x * tan(fov_x / 360 * M_PI));
	a = a * 360 / M_PI;

	return a;
}

/*
====================
CalcFovy
====================
*/
float CalcFovy (float fov_x, float width, float height)
{
	float	a, x;

	if (fov_x < 1 || fov_x > 179)
		Host_Error ("Bad fov: %f", fov_x);

	x = width / tan(fov_x / 360 * M_PI);
	a = atan(height / x);
	a = a * 360 / M_PI;

	return a;
}

/*
=================
SCR_CalcRefdef

Must be called whenever vid changes
Internal use only
=================
*/
static void SCR_CalcRefdef (void)
{
	float		size;
	int		h;
	qboolean		full = false;

	vid.recalc_refdef = false;

// force the status bar to redraw
	Sbar_Changed ();

//========================================
	
// bound viewsize
	if (scr_viewsize.value < 30)
		Cvar_Set ("viewsize","30");
	if (scr_viewsize.value > 120)
		Cvar_Set ("viewsize","120");

// bound weaponsize
	if (scr_weaponsize.value < 60)
		Cvar_Set ("weaponsize","60");
	if (scr_weaponsize.value > 100)
		Cvar_Set ("weaponsize","100");

// bound field of view
	if (scr_fov.value < 10)
		Cvar_Set ("fov","10");
	if (scr_fov.value > 170)
		Cvar_Set ("fov","170");

// bound weapon field of view
	if (scr_weaponfov.value < 10)
		Cvar_Set ("weaponfov","10");
	if (scr_weaponfov.value > 170)
		Cvar_Set ("weaponfov","170");

// intermission is always full screen	
	if (cl.intermission)
		size = 120;
	else
		size = scr_viewsize.value;

	if (size >= 120)
		sb_lines = 0;		// no status bar at all
	else if (size >= 110)
		sb_lines = 24;		// no inventory
	else
		sb_lines = 24+16+8;

	if (scr_overdrawsbar.value)
		sb_lines = 0;

	if (scr_viewsize.value >= 100.0) 
	{
		full = true;
		size = 100.0;
	} 
	else
	{
		full = false;
		size = scr_viewsize.value;
	}

	if (cl.intermission)
	{
		full = true;
		size = 100;
		sb_lines = 0;
	}

	size /= 100.0;

	h = (!scr_sbar.value && full) ? vid.height : vid.height - sb_lines; 

	r_refdef.vrect.width = vid.width * size;

	if (r_refdef.vrect.width < 96)
	{
		size = 96.0 / r_refdef.vrect.width;
		r_refdef.vrect.width = 96;	// min for icons
	}

	r_refdef.vrect.height = vid.height * size;

	if (scr_sbar.value || !full)
	{
		if (r_refdef.vrect.height > (int)vid.height - sb_lines)
			r_refdef.vrect.height = vid.height - sb_lines;
	}
	else if (r_refdef.vrect.height > (int)vid.height)
	{
		r_refdef.vrect.height = vid.height;
	} 

	r_refdef.vrect.x = (vid.width - r_refdef.vrect.width)/2;

	if (full)
		r_refdef.vrect.y = 0;
	else 
		r_refdef.vrect.y = (h - r_refdef.vrect.height)/2;

	r_refdef.fov_x = AdaptFovx (scr_fov.value, vid.width, vid.height);
	r_refdef.fov_y = CalcFovy (r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height);
	
	r_refdef.weaponfov_x = AdaptFovx (scr_weaponfov.value, vid.width, vid.height);

	scr_vrect = r_refdef.vrect;
}


/*
=================
SCR_SizeUp_f

Keybinding command
=================
*/
void SCR_SizeUp_f (void)
{
	Cvar_SetValue ("viewsize",scr_viewsize.value+10);
	Cvar_SetValue ("weaponsize",scr_weaponsize.value+10);
	vid.recalc_refdef = true;
}


/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
void SCR_SizeDown_f (void)
{
	Cvar_SetValue ("viewsize",scr_viewsize.value-10);
	Cvar_SetValue ("weaponsize",scr_weaponsize.value-10);
	vid.recalc_refdef = true;
}

//============================================================================

/*
==================
SCR_Init
==================
*/
void SCR_Init (void)
{
	Cvar_Register (&scr_fov);
	Cvar_Register (&scr_viewsize);
	Cvar_Register (&scr_weaponsize);
	Cvar_Register (&scr_weaponfov);
	Cvar_Register (&scr_conspeed);
	Cvar_Register (&scr_showfps); 
	Cvar_Register (&scr_showstats); 
	Cvar_Register (&scr_showturtle);
	Cvar_Register (&scr_showpause);
	Cvar_Register (&scr_centertime);
	Cvar_Register (&scr_printspeed);
	Cvar_Register (&gl_triplebuffer);

	scr_initialized = true;
	Con_Printf ("Screen initialized\n");

//
// register our commands
//
	Cmd_AddCommand ("screenshot",SCR_ScreenShot_f);
	Cmd_AddCommand ("sizeup",SCR_SizeUp_f);
	Cmd_AddCommand ("sizedown",SCR_SizeDown_f);

	scr_ram = Draw_PicFromWad ("ram");
	scr_net = Draw_PicFromWad ("net");
	scr_turtle = Draw_PicFromWad ("turtle");
}

/*
==============
SCR_DrawFPS
==============
*/
void SCR_DrawFPS (void)
{
	static double	oldtime = 0, fps = 0;
	static int		oldframecount = 0;
	double			time;
	int				frames;
	char			str[12];

	if (!scr_showfps.value)
		return;

	time = realtime - oldtime;
	frames = r_framecount - oldframecount;

	if (time < 0 || frames < 0)
	{
		oldtime = realtime;
		oldframecount = r_framecount;
		return;
	}

	if (time > 0.75) //update value every 3/4 second
	{
		fps = frames / time;
		oldtime = realtime;
		oldframecount = r_framecount;
	}

	sprintf (str, "%4.0f fps", fps);
	Draw_String (vid.width - (strlen(str)<<3), 0, str);
}

/*
===============
SCR_DrawStats
===============
*/
void SCR_DrawStats (void)
{
	int		mins, secs, tens;
	int		y;
	char    str[100];

	if (!scr_showstats.value)
		return;

	y = scr_showfps.value ? 8 : 0;

	mins = cl.time / 60;
	secs = cl.time - 60 * mins;
	tens = (int)(cl.time * 10) % 10;

	sprintf (str,"%i:%i%i:%i", mins, secs/10, secs%10, tens);
	Draw_String (vid.width - (strlen(str)<<3), y, str);

	if (scr_showstats.value > 1)
	{
		sprintf (str,"s: %3i/%3i", cl.stats[STAT_SECRETS], cl.stats[STAT_TOTALSECRETS]);
		Draw_String (vid.width - (strlen(str)<<3), y + 8, str);

		sprintf (str,"m: %3i/%3i", cl.stats[STAT_MONSTERS], cl.stats[STAT_TOTALMONSTERS]);
		Draw_String (vid.width - (strlen(str)<<3), y + 16, str);
	}
} 

/*
==============
SCR_DrawTurtle
==============
*/
void SCR_DrawTurtle (void)
{
	static int	count;
	
	if (!scr_showturtle.value)
		return;

	if (host_frametime < 0.1)
	{
		count = 0;
		return;
	}

	count++;
	if (count < 3)
		return;

	Draw_Pic (scr_vrect.x, scr_vrect.y, scr_turtle);
}

/*
==============
SCR_DrawNet
==============
*/
void SCR_DrawNet (void)
{
	if (realtime - cl.last_received_message < 0.3)
		return;
	if (cls.demoplayback)
		return;

	Draw_Pic (scr_vrect.x+64, scr_vrect.y, scr_net);
}

/*
==============
DrawPause
==============
*/
void SCR_DrawPause (void)
{
	qpic_t	*pic;

	if (!scr_showpause.value)		// turn off for screenshots
		return;

	if (!cl.paused)
		return;

	pic = Draw_CachePic ("gfx/pause.lmp");
	Draw_Pic ( (vid.width - pic->width)/2, 
		(vid.height - 48 - pic->height)/2, pic);
}

/*
==============
SCR_DrawLoading
==============
*/
void SCR_DrawLoading (void)
{
	qpic_t	*pic;

	if (!scr_drawloading)
		return;
		
	pic = Draw_CachePic ("gfx/loading.lmp");
	Draw_Pic ( (vid.width - pic->width)/2, 
		(vid.height - 48 - pic->height)/2, pic);
}

//=============================================================================

/*
==================
SCR_SetUpToDrawConsole
==================
*/
void SCR_SetUpToDrawConsole (void)
{
	//johnfitz -- let's hack away the problem of slow console when host_timescale is <0
	float timescale;

	Con_CheckResize ();

	if (scr_drawloading)
		return;		// never a console with loading plaque

// decide on the height of the console
	con_forcedup = !cl.worldmodel || cls.signon != SIGNONS;

	if (con_forcedup)
	{
		scr_conlines = vid.height;		// full screen
		scr_con_current = scr_conlines;
	}
	else if (key_dest == key_console)
		scr_conlines = vid.height/2;	// half screen
	else
		scr_conlines = 0;				// none visible

	timescale = (host_timescale.value > 0) ? host_timescale.value : 1; //johnfitz -- timescale

	if (scr_conlines < scr_con_current)
	{
		scr_con_current -= scr_conspeed.value*host_frametime/timescale; // timescale
		if (scr_conlines > scr_con_current)
			scr_con_current = scr_conlines;

	}
	else if (scr_conlines > scr_con_current)
	{
		scr_con_current += scr_conspeed.value*host_frametime/timescale; // timescale
		if (scr_conlines < scr_con_current)
			scr_con_current = scr_conlines;
	}

	if (clearconsole++ < vid.numpages)
	{
		Sbar_Changed ();
	}
}
	
/*
==================
SCR_DrawConsole
==================
*/
void SCR_DrawConsole (void)
{
	if (scr_con_current)
	{
		Con_DrawConsole (scr_con_current, true);
		clearconsole = 0;
	}
	else
	{
		if (key_dest == key_game || key_dest == key_message)
			Con_DrawNotify ();	// only draw notify in game
	}
}

/* 
============================================================================== 
 
						SCREEN SHOTS 
 
============================================================================== 
*/

/* 
================== 
SCR_ScreenShot_f
================== 
*/  
void SCR_ScreenShot_f (void) 
{
	byte		*buffer;
	char		tganame[16]; 
	char		checkname[MAX_OSPATH];
	int			i;
	int			mark;

//
// find a file name to save it to 
//
	for (i=0; i<10000; i++)
	{ 
		sprintf (tganame, "quake%04i.tga", i);
		sprintf (checkname, "%s/%s", com_gamedir, tganame);
		if (Sys_FileTime(checkname) == -1)
			break;	// file doesn't exist
	} 
	if (i == 10000)
	{
		Con_Printf ("SCR_ScreenShot_f: Couldn't find an unused filename\n"); 
		return;
	}

//
// get data
//
	// Pa3PyX: now using hunk instead
	mark = Hunk_LowMark ();
	
	buffer = Hunk_AllocName(glwidth * glheight * 4, "buffer_sshot");
	
	glReadPixels (glx, gly, glwidth, glheight, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

//
// now write the file
//
	if (Image_WriteTGA (tganame, buffer, glwidth, glheight, 32, false))
		Con_Printf ("Wrote %s\n", tganame);
	else
		Con_Printf ("SCR_ScreenShot_f: Couldn't create a TGA file\n");

	// Pa3PyX: now using hunk instead
	Hunk_FreeToLowMark (mark);
} 

//=============================================================================

/*
===============
SCR_SetTimeout
================
*/
void SCR_SetTimeout (float timeout)
{
	scr_timeout = timeout;
}

/*
===============
SCR_BeginLoadingPlaque

================
*/
void SCR_BeginLoadingPlaque (void)
{
	S_StopAllSounds (true);

	CDAudio_Stop (); // Stop the CD music

	if (cls.state != ca_connected)
		return;
	if (cls.signon != SIGNONS)
		return;
	
// redraw with no console and the loading plaque
	Con_ClearNotify ();
	// remove all center prints
	con_lastcenterstring[0] = 0;
	scr_centerstring[0] = 0;
	scr_centertime_off = 0;
	scr_con_current = 0;

	scr_drawloading = true;
	Sbar_Changed ();
	SCR_UpdateScreen ();
	scr_drawloading = false;

	scr_disabled_for_loading = true;
	scr_disabled_time = realtime;
	SCR_SetTimeout (SCR_DEFTIMEOUT);
}

/*
===============
SCR_EndLoadingPlaque

================
*/
void SCR_EndLoadingPlaque (void)
{
	scr_disabled_for_loading = false;
	Con_ClearNotify ();
}

//=============================================================================

char	*scr_notifystring;
qboolean	scr_drawdialog;

void SCR_DrawNotifyString (void)
{
	char	*start;
	int		l;
	int		j;
	int		x, y;

	start = scr_notifystring;

	y = vid.height*0.35;

	do	
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (vid.width - l*8)/2;
		for (j=0 ; j<l ; j++, x+=8)
			Draw_Character (x, y, start[j]);	
			
		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;		// skip the \n
	} while (1);
}

/*
==================
SCR_ModalMessage

Displays a text string in the center of the screen and waits for a Y or N
keypress.  
==================
*/
int SCR_ModalMessage (char *text, float timeout) //johnfitz -- timeout
{
	double time1, time2; //johnfitz -- timeout

	if (cls.state == ca_dedicated)
		return true;

	scr_notifystring = text;
 
// draw a fresh screen
	scr_drawdialog = true;
	SCR_UpdateScreen ();
	scr_drawdialog = false;
	
	S_ClearBuffer ();		// so dma doesn't loop current sound

	time1 = Sys_DoubleTime () + timeout; //johnfitz -- timeout
	time2 = 0.0f; //johnfitz -- timeout

	do
	{
		key_count = -1;		// wait for a key down and up
		IN_ProcessEvents ();
		if (timeout)
			time2 = Sys_DoubleTime (); //johnfitz -- zero timeout means wait forever.
	} while (key_lastpress != 'y' &&
		key_lastpress != 'n' &&
		key_lastpress != K_ESCAPE &&
		time2 <= time1);

	SCR_UpdateScreen ();

	//johnfitz -- timeout
	if (time2 > time1)
		return false;
	//johnfitz

	return key_lastpress == 'y';
}

//=============================================================================

/*
==================
SCR_TileClear

fixed the dimentions of right, top and bottom panels
==================
*/
void SCR_TileClear (void)
{
	if (r_refdef.vrect.x > 0)
	{
		// left
		Draw_TileClear (0, 0, r_refdef.vrect.x, vid.height - sb_lines);
		// right
		Draw_TileClear (r_refdef.vrect.x + r_refdef.vrect.width, 0,
			vid.width - r_refdef.vrect.x + r_refdef.vrect.width,
			vid.height - sb_lines);
	}
	if (r_refdef.vrect.y > 0)
	{
		// top
		Draw_TileClear (r_refdef.vrect.x, 0,
			r_refdef.vrect.x + r_refdef.vrect.width,
			r_refdef.vrect.y);
		// bottom
		Draw_TileClear (r_refdef.vrect.x,
			r_refdef.vrect.y + r_refdef.vrect.height,
			r_refdef.vrect.width,
			vid.height - sb_lines - (r_refdef.vrect.height + r_refdef.vrect.y));
	}
}

/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.

WARNING: be very careful calling this from elsewhere, because the refresh
needs almost the entire 256k of stack space!
==================
*/
void SCR_UpdateScreen (void)
{
	if (cls.state == ca_dedicated)
		return;				// stdout only

	if (vid_hiddenwindow || block_drawing)
		return;				// don't suck up any cpu if minimized or blocked for drawing

	if (!scr_initialized || !con_initialized)
		return;				// not initialized yet

	vid.numpages = 2 + (gl_triplebuffer.value ? 1 : 0); // in case gl_triplebuffer is not 0 or 1

	if (scr_disabled_for_loading)
	{
		if (realtime - scr_disabled_time > scr_timeout)
		{
			scr_disabled_for_loading = false;

			if (scr_timeout == SCR_DEFTIMEOUT)
				Con_Printf ("screen update timeout -- load failed.\n");
		}
		else
			return;
	}

//
// check for vid changes
//
	if (oldscreensize != scr_viewsize.value)
	{
		oldscreensize = scr_viewsize.value;
		vid.recalc_refdef = true;
	}

	if (oldweaponsize != scr_weaponsize.value)
	{
		oldweaponsize = scr_weaponsize.value;
		vid.recalc_refdef = true;
	}

	if (oldfov != scr_fov.value)
	{
		oldfov = scr_fov.value;
		vid.recalc_refdef = true;
	}

	if (oldweaponfov != scr_weaponfov.value)
	{
		oldweaponfov = scr_weaponfov.value;
		vid.recalc_refdef = true;
	}

	if (oldsbar != scr_sbar.value)
	{
		oldsbar = scr_sbar.value;
		vid.recalc_refdef = true;
	} 

	if (oldoverdrawsbar != scr_overdrawsbar.value)
	{
		oldoverdrawsbar = scr_overdrawsbar.value;
		vid.recalc_refdef = true;
	}

	if (vid.recalc_refdef)
	{
		// something changed, so reorder the screen
		SCR_CalcRefdef ();
	}

	if (scr_overdrawsbar.value || gl_clear.value || isIntel) // intel video workaround
		Sbar_Changed ();

//
// do 3D refresh drawing, and then update the screen
//
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);

	SCR_SetUpToDrawConsole ();

	V_RenderView ();

	GL_Set2D ();

//
// draw any areas not covered by the refresh
//
	if (scr_sbar.value || scr_viewsize.value < 100)
	{
		SCR_TileClear ();
		Sbar_Changed ();
	}

	if (scr_drawdialog) //new game confirm
	{
		Sbar_Draw ();
		Draw_FadeScreen ();
		SCR_DrawNotifyString ();
	}
	else if (scr_drawloading) //loading
	{
		SCR_DrawLoading ();
		Sbar_Draw ();
	}
	else if (cl.intermission == 1 && key_dest == key_game) //end of level
	{
		Sbar_IntermissionOverlay ();
	}
	else if (cl.intermission == 2 && key_dest == key_game) //end of episode
	{
		Sbar_FinaleOverlay ();
		SCR_CheckDrawCenterString ();
	}
	else
	{
		Draw_Crosshair ();
		SCR_DrawNet ();
		SCR_DrawTurtle ();
		SCR_DrawPause ();
		SCR_CheckDrawCenterString ();
		SCR_DrawFPS ();
		SCR_DrawStats ();
		Sbar_Draw ();
		SCR_DrawConsole ();	
		M_Draw ();
	}

	V_UpdateBlend ();

	GL_EndRendering ();
}

