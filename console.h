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
// console.h


#define		CON_TEXTSIZE	262144

extern int con_totallines;
extern int con_backscroll;
extern qboolean con_forcedup;	// because no entities to refresh
extern qboolean con_initialized;
//extern byte *con_chars;
extern int	con_vislines;
extern int	con_current;
extern int	con_x;
extern int	con_linewidth;
extern char	*con_text;
extern char	con_lastcenterstring[MAX_PRINTMSG];

void Con_DrawCharacter (int cx, int line, int num);

void Con_CheckResize (void);
void Con_Init (void);
void Con_DrawConsole (int lines, qboolean drawinput);
void Con_Print (char *txt);
void Con_Printf (char *fmt, ...);
void Con_Error (char *fmt, ...);
void Con_Warning (char *fmt, ...);
void Con_DWarning (char *fmt, ...);
void Con_DPrintf (char *fmt, ...);
void Con_SafePrintf (char *fmt, ...);
void Con_Clear_f (void);
void Con_DrawNotify (void);
void Con_ClearNotify (void);
void Con_ToggleConsole_f (void);
void Con_LogCenterPrint (char *str);
char *Con_Quakebar (int len);

void Con_NotifyBox (char *text);	// during startup for sound / cd warnings

void Con_TabComplete (void);

void LOG_Init (void);
void LOG_Close (void);
void Con_DebugLog (char *fmt, ...);

