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
// client.h

// R_RocketTrail types (self-documenting code!)
#define RT_ROCKET			0
#define RT_GRENADE			1
#define RT_GIB				2
#define RT_WIZARD			3
#define RT_ZOMGIB			4
#define RT_KNIGHT			5
#define RT_VORE				6


typedef struct
{
	vec3_t	viewangles;

// intended velocities
	float	forwardmove;
	float	sidemove;
	float	upmove;

} usercmd_t;

typedef struct
{
	int		length;
	char	map[MAX_STYLESTRING];
} lightstyle_t;

typedef struct
{
	char	name[MAX_SCOREBOARDNAME];
	float	entertime;
	int		frags;
	int		colors;			// two 4 bit fields
	byte	translations[VID_GRADES*256];
} scoreboard_t;

typedef struct
{
	int		destcolor[3];
	int		percent;		// 0-256
} cshift_t;

#define	CSHIFT_CONTENTS	0
#define	CSHIFT_DAMAGE	1
#define	CSHIFT_BONUS	2
#define	CSHIFT_POWERUP	3
#define	NUM_CSHIFTS		4

#define	NAME_LENGTH	64


//
// client_state_t should hold all pieces of the client state
//

#define	MAX_DLIGHTS		512 //johnfitz -- was 32
typedef struct
{
	vec3_t	origin;
	float	radius;
	float	die;				// stop lighting after this time
	float	decay;				// drop this each second
	float	minlight;			// don't add when contributing less
	int		key;
	vec3_t	color;				// lit support via lordhavoc
} dlight_t;

// keep dlight colours in the one place so that if i need to change them i only need to do it once

#define DL_COLOR_SHOT		236, 4
#define DL_COLOR_LASER_SHOT	228, 6	// hipnotic
#define DL_COLOR_LASER		231, 5	// id laser
#define DL_COLOR_LASER2		247, 5	// hipnotic red laser
#define DL_COLOR_LASER3		252, 3	// rogue yellow laser
#define DL_COLOR_LIGHTNING	244, 3
#define DL_COLOR_BEAM		240, 4
#define DL_COLOR_LAVA		226, 10
#define DL_COLOR_ROCKET		232, 8
#define DL_COLOR_FIRE		228, 12
#define DL_COLOR_QUAD		37, 6
#define DL_COLOR_PENT		73, 6
#define DL_COLOR_POWER		129, 6
#define DL_COLOR_FLAME		236, 4
#define DL_COLOR_FLAME2		238, 2
#define DL_COLOR_FLAME3		233, 6	// candle
#define DL_COLOR_FLAME4		234, 6	// candle nehahra

#define DL_COLOR_T_EXPLOSION	37, 8
#define DL_COLOR_K_SPIKE	104, 4
#define DL_COLOR_V_SPIKE	146, 3
#define DL_COLOR_W_SPIKE	192, 5
#define DL_COLOR_W_BALL		247, 3	// rogue wrath spike

#define DL_COLOR_8			8		// gray 123
#define DL_COLOR_15			15		// dim white 235
#define DL_COLOR_235		235		// fire
#define DL_COLOR_252		252		// beam
#define DL_COLOR_254		254		// full white
#define DL_COLOR_167		167

#define DL_COLOR_47			47		// blue
#define DL_COLOR_79			79		// red
#define DL_COLOR_144		144		// red&blue

#define DL_COLOR_111		111		// light sprite
#define DL_COLOR_246		246		// lantern

#define DL_COLOR_253		253
#define DL_COLOR_245		245
#define DL_COLOR_250		250

#define	MAX_BEAMS	256 //was 32 //johnfitz -- was 24
typedef struct
{
	int		entity;
	struct model_s	*model;
	float	endtime;
	vec3_t	start, end;
} beam_t;

#define	MAX_EFRAGS	8192 // fx 4096 // was 2048 // orig. was 640

#define	MAX_MAPSTRING	2048
#define	MAX_DEMOS		8
#define	MAX_DEMONAME	16

#define    SIGNONS        4            // signon messages to receive before connected
typedef enum 
{
	ca_dedicated, 		// a dedicated server with no ability to start a client
	ca_disconnected, 	// full screen console with no connection
	ca_connected		// valid netcon, talking to a server
} cactive_t;

//
// the client_static_t structure is persistant through an arbitrary number
// of server connections
//
typedef struct
{
	cactive_t	state;

// personalization data sent to server	
	char		mapstring[MAX_QPATH];
	char		spawnparms[MAX_MAPSTRING];	// to restart a level

// demo loop control
	int			demonum;		// -1 = don't play demos
	char		demos[MAX_DEMOS][MAX_DEMONAME];		// when not playing

// demo recording info must be here, because record is started before
// entering a map (and clearing client_state_t)
	qboolean	demorecording;
	qboolean	demoplayback;
	qboolean	timedemo;
	int			forcetrack;			// -1 = use normal cd track
	FILE		*demofile;
	int			td_lastframe;		// to meter out one message a frame
	int			td_startframe;		// host_framecount at start
	float		td_starttime;		// realtime at second frame of timedemo
	int			stufftext_frame;	// host_framecount when svc_stufftext is received

// connection information
	int			signon;			// 0 to SIGNONS
	struct qsocket_s	*netcon;
	sizebuf_t	message;		// writing buffer to send to server
	
} client_static_t;

extern client_static_t	cls;

//
// the client_state_t structure is wiped completely at every
// server signon
//
typedef struct
{
	int			movemessages;	// since connecting to this server
								// throw out the first couple, so the player
								// doesn't accidentally do something the 
								// first frame
	usercmd_t	cmd;			// last command sent to the server

// information for local display
	int			stats[MAX_CL_STATS];	// health, etc
	int			items;			// inventory bit flags
	float	item_gettime[32];	// cl.time of aquiring item, for blinking
	float		faceanimtime;	// use anim frame if cl.time < this

	cshift_t	cshifts[NUM_CSHIFTS];	// color shifts for damage, powerups
	cshift_t	prev_cshifts[NUM_CSHIFTS];	// and content types

// the client maintains its own idea of view angles, which are
// sent to the server each frame.  The server sets punchangle when
// the view is temporarliy offset, and an angle reset commands at the start
// of each level and after teleporting.
	vec3_t		mviewangles[2];	// during demo playback viewangles is lerped
								// between these
	vec3_t		viewangles;
	
	vec3_t		mvelocity[2];	// update by server, used for lean+bob
								// (0 is newest)
	vec3_t		velocity;		// lerped between mvelocity[0] and [1]

	vec3_t		punchangle;		// temporary offset
	
// pitch drifting vars
	float		idealpitch;
	float		pitchvel;
	qboolean	nodrift;
	float		driftmove;
	double		laststop;

	float		viewheight;
	float		crouch;			// local amount for smoothing stepups

	qboolean	paused;			// send over by server
	qboolean	onground;
	qboolean	inwater;
	
	int			intermission;	// don't change view angle, full screen, etc
	int			completed_time;	// latched at intermission start
	
	double		mtime[2];		// the timestamp of last two messages	
	double		time;			// clients view of time, should be between
								// servertime and oldservertime to generate
								// a lerp point for other data
	double		oldtime;		// previous cl.time, time-oldtime is used
								// to decay light values and smooth step ups
	

	float		last_received_message;	// (realtime) for net trouble icon

//
// information that is static for the entire time connected to a server
//
	struct model_s		*model_precache[MAX_MODELS];
	struct sfx_s		*sound_precache[MAX_SOUNDS];

	char		worldname[MAX_QPATH];
	char		levelname[256];	// for display on solo scoreboard
	int			viewentity;		// cl_entities[cl.viewentity] = player
	int			maxclients;
	int			gametype;

// refresh related state
	struct model_s	*worldmodel;	// cl_entities[0].model
	struct efrag_s	*free_efrags;
	int			num_entities;	// held in cl_entities array
	int			num_statics;	// held in cl_staticentities array
	entity_t	viewent;			// the gun model

	int			cdtrack, looptrack;	// cd audio

// frag scoreboard
	scoreboard_t	*scores;		// [cl.maxclients]

	double			last_angle_time;
	vec3_t			lerpangles;

	qboolean		noclip_anglehack;

	int			protocol;
	unsigned	protocolflags;
    
} client_state_t;


//
// cvars
//
extern	cvar_t	cl_name;
extern	cvar_t	cl_color;

extern	cvar_t	cl_upspeed;
extern	cvar_t	cl_forwardspeed;
extern	cvar_t	cl_backspeed;
extern	cvar_t	cl_sidespeed;

extern	cvar_t	cl_movespeedkey;

extern	cvar_t	cl_yawspeed;
extern	cvar_t	cl_pitchspeed;

extern	cvar_t	cl_maxpitch; // variable pitch clamping
extern	cvar_t	cl_minpitch; // variable pitch clamping

extern	cvar_t	cl_anglespeedkey;

extern	cvar_t	cl_shownet;
extern	cvar_t	cl_nolerp;
extern	cvar_t	cl_lerpmuzzleflash;

extern	cvar_t	cl_coloredlight;
extern	cvar_t	cl_extradlight;
extern	cvar_t	cl_extradlightstatic;

extern	cvar_t	cl_pitchdriftspeed;
extern	cvar_t	lookspring;
extern	cvar_t	lookstrafe;
extern	cvar_t	sensitivity;

extern	cvar_t	m_pitch;
extern	cvar_t	m_yaw;
extern	cvar_t	m_forward;
extern	cvar_t	m_side;


#define	MAX_TEMP_ENTITIES		512 // was 64			// lightning bolts, etc
#define	MAX_STATIC_ENTITIES		4096 // fx 512 // was 128 //bjp 256	// torches, etc

extern	client_state_t	cl;

// FIXME, allocate dynamically
extern	efrag_t			cl_efrags[MAX_EFRAGS];
extern	entity_t		cl_entities[MAX_EDICTS];
extern	entity_t		cl_static_entities[MAX_STATIC_ENTITIES];
extern	lightstyle_t	cl_lightstyle[MAX_LIGHTSTYLES];
extern	dlight_t		cl_dlights[MAX_DLIGHTS];
extern	entity_t		cl_temp_entities[MAX_TEMP_ENTITIES];
extern	beam_t			cl_beams[MAX_BEAMS];

//=============================================================================

//
// cl_main.c
//
dlight_t *CL_AllocDlight (int key);
void	CL_ColorDlight (dlight_t *dl, float r, float g, float b);
void	CL_ColorDlightPalette (dlight_t *dl, int i);
void	CL_ColorDlightPaletteLength (dlight_t *dl, int start, int length);
void	CL_DecayLights (void);

void CL_Init (void);

void CL_EstablishConnection (char *host);
void CL_Signon1 (void);
void CL_Signon2 (void);
void CL_Signon3 (void);
void CL_Signon4 (void);

void CL_Reconnect (void);
void CL_Disconnect (void);

// bumped for new value of MAX_EDICTS
#define		MAX_VISEDICTS		MAX_EDICTS // was 256
extern	int				cl_numvisedicts;
extern	entity_t		*cl_visedicts[MAX_VISEDICTS];

//
// cl_input.c
//
typedef struct
{
	int		down[2];		// key nums holding it down
	int		state;			// low bit is down state
} kbutton_t;

extern	kbutton_t	in_mlook, in_klook;
extern 	kbutton_t 	in_strafe;
extern 	kbutton_t 	in_speed;
extern	kbutton_t	in_attack; // added this for completeness

void CL_InitInput (void);
void CL_SendCmd (void);
void CL_SendMove (usercmd_t *cmd);

void CL_ParseTEnt (void);
void CL_UpdateTEnts (void);

void CL_ClearState (void);


int  CL_ReadFromServer (void);
void CL_WriteToServer (usercmd_t *cmd);
void CL_BaseMove (usercmd_t *cmd);


float CL_KeyState (kbutton_t *key);
char *Key_KeynumToString (int keynum);

//
// cl_demo.c
//
void CL_NextDemo (void);
void CL_StopPlayback (void);
int CL_GetMessage (void);

void CL_Stop_f (void);
void CL_Record_f (void);
void CL_PlayDemo_f (void);
void CL_TimeDemo_f (void);

//
// cl_parse.c
//
void CL_ParseServerMessage (void);
void CL_NewTranslation (int slot);

//
// view.c
//
void V_StartPitchDrift (void);
void V_StopPitchDrift (void);

void V_Register (void);
void V_ParseDamage (void);
void V_SetContentsColor (int contents);

//
// cl_tent.c
//
void CL_InitTEnts (void);
void CL_SignonReply (void);

//
// chase.c
//
void TraceLine (vec3_t start, vec3_t end, vec3_t impact);

