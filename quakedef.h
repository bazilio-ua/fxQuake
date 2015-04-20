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
// quakedef.h -- primary header for client and server


// Quake is a trademark of Id Software, Inc., (c) 1996 Id Software, Inc. All
// rights reserved.


// disable data conversion warnings for MSVC++ 
#ifdef _MSC_VER
#pragma warning(disable : 4244)	// double|int to float truncation
#pragma warning(disable : 4305)	// const double to float truncation
//#pragma warning(disable : 4018)	// signed/unsigned mismatch
//#pragma warning(disable : 4996)	// deprecated
#endif 

//#define	QUAKE_GAME			// as opposed to utilities
#define	VERSION				1.09

#define	GAMENAME	"id1"		// directory to look in by default

#include <math.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>
#include <errno.h>


// MSVC++ has a different name for several standard functions
#ifdef _MSC_VER
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define inline __inline
#endif 


#define	MINIMUM_MEMORY			0x1800000 // 24 Mb, was 0x550000 (5.6 Mb)
#define	MINIMUM_MEMORY_LEVELPAK	(MINIMUM_MEMORY + 0x400000) // +4 Mb, was 0x100000 (+1 Mb) 

#define DEFAULT_MEMORY_SIZE		64	// default memory size in Mb

#define MAX_NUM_ARGVS	50

// up / down
#define	PITCH	0

// left / right
#define	YAW		1

// fall over
#define	ROLL	2


#define	MAX_QPATH		128			// max length of a quake game pathname
#define	MAX_OSPATH		1024		// max length of a filesystem pathname

#define	ON_EPSILON		0.1			// point on plane side epsilon
#define	DIST_EPSILON		(0.03125)	// 1/32 epsilon to keep floating point happy (moved from world.c)

#define	MAX_MSGLEN			65528 // NETFLAG_DATA - NET_HEADERSIZE // (orig. 8000) // max length of a reliable message
#define	MAX_DATAGRAM		65528 // NETFLAG_DATA - NET_HEADERSIZE // (orig. 1024) // max length of unreliable message
// (driver MTU may be lower)
#define DATAGRAM_MTU		1450 // EER1 -- increase MTU to 1450 as QW, was 1400 // actual limit for unreliable messages to nonlocal clients
#define DATAGRAM_MTU_NQ		1032 // set MTU as orig NQ MAX_DATAGRAM + NET_HEADERSIZE, so old clients can connect

#define	MAX_PRINTMSG		8192

//
// per-level limits
//
#define MAX_EDICTS			8192 // was 600		// More than 8192 requires protocol change	// protocol limit, ents past 8192 can't play sounds in the standard protocol 
#define	MAX_LIGHTSTYLES		64
// protocol limit values - bumped from bjp
#define	MAX_MODELS			2048 // was 256		// these are sent over the net as bytes
#define	MAX_SOUNDS			2048 // was 256		// so they cannot be blindly increased
// Model and sound limits depend on the net protocol version being used
// Standard protocol sends the model/sound index as a byte (max = 256), but
// other protocols may send as a short (up to 65536, potentially).

#define	SAVEGAME_COMMENT_LENGTH	39

#define	MAX_STYLESTRING	64

//
// stats are integers communicated to the client by the server
//
#define	MAX_CL_STATS		32
#define	STAT_HEALTH			0
#define	STAT_FRAGS			1
#define	STAT_WEAPON			2
#define	STAT_AMMO			3
#define	STAT_ARMOR			4
#define	STAT_WEAPONFRAME	5
#define	STAT_SHELLS			6
#define	STAT_NAILS			7
#define	STAT_ROCKETS		8
#define	STAT_CELLS			9
#define	STAT_ACTIVEWEAPON	10
#define	STAT_TOTALSECRETS	11
#define	STAT_TOTALMONSTERS	12
#define	STAT_SECRETS		13		// bumped on client side by svc_foundsecret
#define	STAT_MONSTERS		14		// bumped by svc_killedmonster

// stock defines

#define	IT_SHOTGUN				1
#define	IT_SUPER_SHOTGUN		2
#define	IT_NAILGUN				4
#define	IT_SUPER_NAILGUN		8
#define	IT_GRENADE_LAUNCHER		16
#define	IT_ROCKET_LAUNCHER		32
#define	IT_LIGHTNING			64
#define IT_EXTRA_WEAPON         128 // was IT_SUPER_LIGHTNING, unused at id
#define IT_SHELLS               256
#define IT_NAILS                512
#define IT_ROCKETS              1024
#define IT_CELLS                2048
#define IT_AXE                  4096
#define IT_ARMOR1               8192
#define IT_ARMOR2               16384
#define IT_ARMOR3               32768
#define IT_SUPERHEALTH          65536
#define IT_KEY1                 131072
#define IT_KEY2                 262144
#define	IT_INVISIBILITY			524288
#define	IT_INVULNERABILITY		1048576
#define	IT_SUIT					2097152
#define	IT_QUAD					4194304
#define IT_SIGIL1               (1<<28)
#define IT_SIGIL2               (1<<29)
#define IT_SIGIL3               (1<<30)
#define IT_SIGIL4               (1<<31)

//===========================================
//rogue changed and added defines

#define RIT_SHELLS              128
#define RIT_NAILS               256
#define RIT_ROCKETS             512
#define RIT_CELLS               1024
#define RIT_AXE                 2048
#define RIT_LAVA_NAILGUN        4096
#define RIT_LAVA_SUPER_NAILGUN  8192
#define RIT_MULTI_GRENADE       16384
#define RIT_MULTI_ROCKET        32768
#define RIT_PLASMA_GUN          65536
#define RIT_ARMOR1              8388608
#define RIT_ARMOR2              16777216
#define RIT_ARMOR3              33554432
#define RIT_LAVA_NAILS          67108864
#define RIT_PLASMA_AMMO         134217728
#define RIT_MULTI_ROCKETS       268435456
#define RIT_SHIELD              536870912
#define RIT_ANTIGRAV            1073741824
#define RIT_SUPERHEALTH         2147483648

//MED 01/04/97 added hipnotic defines
//===========================================
//hipnotic added defines
#define HIT_PROXIMITY_GUN_BIT 16
#define HIT_MJOLNIR_BIT       7
#define HIT_LASER_CANNON_BIT  23
#define HIT_PROXIMITY_GUN   (1<<HIT_PROXIMITY_GUN_BIT)
#define HIT_MJOLNIR         (1<<HIT_MJOLNIR_BIT)
#define HIT_LASER_CANNON    (1<<HIT_LASER_CANNON_BIT)
#define HIT_WETSUIT         (1<<(23+1)) // corrected wetsuit define, was (1<<(23+2))
#define HIT_EMPATHY_SHIELDS (1<<(23+2)) // corrected empathy shield define, was (1<<(23+3))

//===========================================
// Nehahra defines
#define NIT_SPROCKET				128 // same as IT_EXTRA_WEAPON/IT_SUPER_LIGHTNING
#define NIT_AUTO_SHOTGUN			8388608

//===========================================

#define	MAX_SCOREBOARD		64 //16
#define	MAX_SCOREBOARDNAME	32

#define	SOUND_CHANNELS		8


#include "common.h"
#include "mathlib.h"
#include "cvar.h"
#include "vid.h"
#include "sys.h"
#include "zone.h"

#include "bspfile.h"
#include "wad.h"
#include "draw.h"
#include "screen.h"
#include "net.h"
#include "protocol.h"
#include "cmd.h"
#include "sbar.h"
#include "sound.h"
#include "render.h"
#include "client.h"
#include "progs.h"
#include "server.h"

#include "model.h"

#include "input.h"
#include "world.h"
#include "keys.h"
#include "console.h"
#include "view.h"
#include "menu.h"
#include "crc.h"
#include "cdaudio.h"

#include "glquake.h"

//=============================================================================

// the host system specifies the base of the directory tree, the
// command line parms passed to the program, and the amount of memory
// available for the program to use

typedef struct
{
	char	*basedir;
	char	*cachedir;		// for development over ISDN lines
	int		argc;
	char	**argv;
	void	*membase;
	int		memsize;
} quakeparms_t;


//=============================================================================


//
// host
//
extern	quakeparms_t host_parms;

extern	cvar_t		sys_ticrate;
extern	cvar_t		sys_nostdout;
extern	cvar_t		developer;
extern	cvar_t		host_timescale;

extern	qboolean	host_initialized;		// true if into command execution
extern	double		host_frametime;
extern	byte		*host_basepal;
extern	byte		*host_colormap;
extern	int			host_framecount;	// incremented every frame, never reset
extern	double		realtime;			// not bounded in any way, changed at
									// start of every frame, never reset

void Host_ClearMemory (void);
void Host_ServerFrame (void);
void Host_InitCommands (void);
void Host_Init (quakeparms_t *parms);
void Host_Shutdown(void);
void Host_Error (char *error, ...);
void Host_EndGame (char *message, ...);
void Host_Frame (double time);
void Host_Quit_f (void);
void Host_ClientCommands (char *fmt, ...);
void Host_ShutdownServer (qboolean crash);

extern int			current_skill;		// skill level for currently loaded level (in case
										//  the user changes the cvar while the level is
										//  running, this reflects the level actually in use)

//
// chase
//
extern	cvar_t	chase_active;

void Chase_Init (void);
void Chase_Reset (void);
void Chase_Update (void);

//
// vid
//
