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
// cd_win.c

#include "quakedef.h"
#include "winquake.h"

/*
 * You just can't set the volume of CD playback via MCI :
 * http://blogs.msdn.com/larryosterman/archive/2005/10/06/477874.aspx
 * OTOH, using the aux APIs to control the CD audio volume is broken.
 */
#undef	USE_AUX_API
//#define	USE_AUX_API

cvar_t bgmvolume = {"bgmvolume", "1", true};
cvar_t bgmtype = {"bgmtype", "cd", true};   // cd or none

static qboolean cdValid = false;
static qboolean	playing = false;
static qboolean	wasPlaying = false;
static qboolean	initialized = false;
static qboolean	enabled = false;
static qboolean playLooping = false;
static byte 	remap[100];
static byte		playTrack;
static byte		maxTrack;

static float	old_cdvolume;
static UINT		wDeviceID;
#ifdef USE_AUX_API
static UINT		CD_ID;
static unsigned long	CD_OrigVolume;
static void CD_SetVolume(unsigned long Volume);
#endif	/* USE_AUX_API */


static void CDAudio_Eject(void)
{
	DWORD	dwReturn;

	dwReturn = mciSendCommand(wDeviceID, MCI_SET, MCI_SET_DOOR_OPEN, (DWORD_PTR)NULL);
	if (dwReturn)
		Con_DPrintf("CDAudio_Eject: MCI_SET_DOOR_OPEN failed (%u)\n", (unsigned int)dwReturn);
}


static void CDAudio_CloseDoor(void)
{
	DWORD	dwReturn;

	dwReturn = mciSendCommand(wDeviceID, MCI_SET, MCI_SET_DOOR_CLOSED, (DWORD_PTR)NULL);
	if (dwReturn)
		Con_DPrintf("CDAudio_CloseDoor: MCI_SET_DOOR_CLOSED failed (%u)\n", (unsigned int)dwReturn);
}


static int CDAudio_GetAudioDiskInfo(void)
{
	DWORD				dwReturn;
	MCI_STATUS_PARMS	mciStatusParms;


	cdValid = false;

	mciStatusParms.dwItem = MCI_STATUS_READY;
	dwReturn = mciSendCommand(wDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_WAIT, (DWORD_PTR) (LPVOID) &mciStatusParms);
	if (dwReturn)
	{
		Con_DPrintf("CDAudio_GetAudioDiskInfo: drive ready test - get status failed\n");
		return -1;
	}
	if (!mciStatusParms.dwReturn)
	{
		Con_DPrintf("CDAudio_GetAudioDiskInfo: drive not ready\n");
		return -1;
	}

	mciStatusParms.dwItem = MCI_STATUS_NUMBER_OF_TRACKS;
	dwReturn = mciSendCommand(wDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_WAIT, (DWORD_PTR) (LPVOID) &mciStatusParms);
	if (dwReturn)
	{
		Con_DPrintf("CDAudio_GetAudioDiskInfo: get tracks - status failed\n");
		return -1;
	}
	if (mciStatusParms.dwReturn < 1)
	{
		Con_DPrintf("CDAudio_GetAudioDiskInfo: no music tracks\n");
		return -1;
	}

	cdValid = true;
	maxTrack = mciStatusParms.dwReturn;

	return 0;
}


void CDAudio_Play(byte track, qboolean looping)
{
	DWORD				dwReturn;
    MCI_PLAY_PARMS		mciPlayParms;
	MCI_STATUS_PARMS	mciStatusParms;

	if (!enabled)
		return;
	
	if (!cdValid)
	{
		CDAudio_GetAudioDiskInfo();
		if (!cdValid)
			return;
	}

	track = remap[track];

	if (track < 1 || track > maxTrack)
	{
		Con_DPrintf("CDAudio_Play: Bad track number %u.\n", track);
		return;
	}

	// don't try to play a non-audio track
	mciStatusParms.dwItem = MCI_CDA_STATUS_TYPE_TRACK;
	mciStatusParms.dwTrack = track;
	dwReturn = mciSendCommand(wDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_TRACK | MCI_WAIT, (DWORD_PTR) (LPVOID) &mciStatusParms);
	if (dwReturn)
	{
		Con_DPrintf("CDAudio_Play: MCI_STATUS failed (%u)\n", (unsigned int)dwReturn);
		return;
	}
	if (mciStatusParms.dwReturn != MCI_CDA_TRACK_AUDIO)
	{
		Con_DPrintf("CDAudio_Play: track %i is not audio\n", track);
		return;
	}

	// get the length of the track to be played
	mciStatusParms.dwItem = MCI_STATUS_LENGTH;
	mciStatusParms.dwTrack = track;
	dwReturn = mciSendCommand(wDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_TRACK | MCI_WAIT, (DWORD_PTR) (LPVOID) &mciStatusParms);
	if (dwReturn)
	{
		Con_DPrintf("CDAudio_Play: MCI_STATUS failed (%u)\n", (unsigned int)dwReturn);
		return;
	}

	if (playing)
	{
		if (playTrack == track)
			return;
		CDAudio_Stop();
	}

	mciPlayParms.dwFrom = MCI_MAKE_TMSF(track, 0, 0, 0);
	mciPlayParms.dwTo = (mciStatusParms.dwReturn << 8) | track;
	mciPlayParms.dwCallback = (DWORD_PTR)mainwindow;
	dwReturn = mciSendCommand(wDeviceID, MCI_PLAY, MCI_NOTIFY | MCI_FROM | MCI_TO, (DWORD_PTR) (LPVOID) &mciPlayParms);
	if (dwReturn)
	{
		Con_DPrintf("CDAudio_Play: MCI_PLAY failed (%u)\n", (unsigned int)dwReturn);
		return;
	}

	playLooping = looping;
	playTrack = track;
	playing = true;

	if (bgmvolume.value == 0.0)
		CDAudio_Pause ();
}


void CDAudio_Stop(void)
{
	DWORD	dwReturn;

	if (!enabled)
		return;
	
	if (!playing)
		return;

	dwReturn = mciSendCommand(wDeviceID, MCI_STOP, 0, (DWORD_PTR)NULL);
	if (dwReturn)
		Con_DPrintf("CDAudio_Stop: MCI_STOP failed (%u)", (unsigned int)dwReturn);

	wasPlaying = false;
	playing = false;
}


void CDAudio_Pause(void)
{
	DWORD				dwReturn;
	MCI_GENERIC_PARMS	mciGenericParms;

	if (!enabled)
		return;

	if (!playing)
		return;

	mciGenericParms.dwCallback = (DWORD_PTR)mainwindow;
	dwReturn = mciSendCommand(wDeviceID, MCI_PAUSE, 0, (DWORD_PTR) (LPVOID) &mciGenericParms);
	if (dwReturn)
		Con_DPrintf("CDAudio_Pause: MCI_PAUSE failed (%u)", (unsigned int)dwReturn);

	wasPlaying = playing;
	playing = false;
}


void CDAudio_Resume(void)
{
	DWORD			dwReturn;
    MCI_PLAY_PARMS	mciPlayParms;

	if (!enabled)
		return;
	
	if (!cdValid)
		return;

	if (!wasPlaying)
		return;
	
	mciPlayParms.dwFrom = MCI_MAKE_TMSF(playTrack, 0, 0, 0);
	mciPlayParms.dwTo = MCI_MAKE_TMSF(playTrack + 1, 0, 0, 0);
	mciPlayParms.dwCallback = (DWORD_PTR)mainwindow;
	dwReturn = mciSendCommand(wDeviceID, MCI_PLAY, MCI_TO | MCI_NOTIFY, (DWORD_PTR) (LPVOID) &mciPlayParms);
	if (dwReturn)
	{
		Con_DPrintf("CDAudio_Resume: MCI_PLAY failed (%u)\n", (unsigned int)dwReturn);
		return;
	}
	playing = true;
}


static void CD_f (void)
{
	char	*command;
	int		ret;
	int		n;

	if (Cmd_Argc() < 2)
	{
		Con_Printf("commands: ");
		Con_Printf("on, off, reset, remap, \n");
		Con_Printf("play, stop, loop, pause, resume\n");
		Con_Printf("eject, close, info\n");
		return;
	}

	command = Cmd_Argv (1);

	if (strcasecmp(command, "on") == 0)
	{
		enabled = true;
		return;
	}

	if (strcasecmp(command, "off") == 0)
	{
		if (playing)
			CDAudio_Stop();
		enabled = false;
		return;
	}

	if (strcasecmp(command, "reset") == 0)
	{
		enabled = true;
		if (playing)
			CDAudio_Stop();
		for (n = 0; n < 100; n++)
			remap[n] = n;
		CDAudio_GetAudioDiskInfo();
		return;
	}

	if (strcasecmp(command, "remap") == 0)
	{
		ret = Cmd_Argc() - 2;
		if (ret <= 0)
		{
			for (n = 1; n < 100; n++)
				if (remap[n] != n)
					Con_Printf("  %u -> %u\n", n, remap[n]);
			return;
		}
		for (n = 1; n <= ret; n++)
			remap[n] = atoi(Cmd_Argv (n+1));
		return;
	}

	if (strcasecmp(command, "close") == 0)
	{
		CDAudio_CloseDoor();
		return;
	}

	if (!cdValid)
	{
		CDAudio_GetAudioDiskInfo();
		if (!cdValid)
		{
			Con_Printf("No CD in drive\n");
			return;
		}
	}

	if (strcasecmp(command, "play") == 0)
	{
		CDAudio_Play((byte)atoi(Cmd_Argv (2)), false);
		return;
	}

	if (strcasecmp(command, "loop") == 0)
	{
		CDAudio_Play((byte)atoi(Cmd_Argv (2)), true);
		return;
	}

	if (strcasecmp(command, "stop") == 0)
	{
		CDAudio_Stop();
		return;
	}

	if (strcasecmp(command, "pause") == 0)
	{
		CDAudio_Pause();
		return;
	}

	if (strcasecmp(command, "resume") == 0)
	{
		CDAudio_Resume();
		return;
	}

	if (strcasecmp(command, "eject") == 0)
	{
		if (playing)
			CDAudio_Stop();
		CDAudio_Eject();
		cdValid = false;
		return;
	}

	if (strcasecmp(command, "info") == 0)
	{
		Con_Printf("%u tracks\n", maxTrack);
		if (playing)
			Con_Printf("Currently %s track %u\n", playLooping ? "looping" : "playing", playTrack);
		else if (wasPlaying)
			Con_Printf("Paused %s track %u\n", playLooping ? "looping" : "playing", playTrack);
		Con_Printf("Volume is %f\n", bgmvolume.value);
		return;
	}
}


LONG CDAudio_MessageHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (lParam != (LPARAM)wDeviceID)
		return 1;

	switch (wParam)
	{
	case MCI_NOTIFY_SUCCESSFUL:
		if (playing)
		{
			playing = false;
			if (playLooping)
				CDAudio_Play(playTrack, true);
		}
		break;

	case MCI_NOTIFY_ABORTED:
	case MCI_NOTIFY_SUPERSEDED:
		break;

	case MCI_NOTIFY_FAILURE:
		Con_DPrintf("CDAudio_MessageHandler: MCI_NOTIFY_FAILURE\n");
		CDAudio_Stop ();
		cdValid = false;
		break;

	default:
		Con_DPrintf("CDAudio_MessageHandler: Unexpected MM_MCINOTIFY type (%Iu)\n", wParam);
		return 1;
	}

	return 0;
}


static void CDAudio_SetVolume (cvar_t *var)
{
	if (var->value < 0.0)
		Cvar_SetValue (var->name, 0.0);
	else if (var->value > 1.0)
		Cvar_SetValue (var->name, 1.0);
	old_cdvolume = var->value;

#ifdef USE_AUX_API
	CD_SetVolume (var->value * 0xffff);
#endif	/* USE_AUX_API */
	if (old_cdvolume == 0.0)
		CDAudio_Pause ();
	else
		CDAudio_Resume();
}

void CDAudio_Update(void)
{
	if (!enabled)
		return;

	if (old_cdvolume != bgmvolume.value)
		CDAudio_SetVolume (&bgmvolume);
}


#ifdef USE_AUX_API
static void CD_FindCDAux(void)
{
	UINT		NumDevs, counter;
	MMRESULT		Result;
	AUXCAPS			Caps;

	CD_ID = -1;
	if (!COM_CheckParm("-usecdvolume"))
		return;
	NumDevs = auxGetNumDevs();
	for (counter = 0; counter < NumDevs; counter++)
	{
		Result = auxGetDevCaps(counter,&Caps,sizeof(Caps));
		if (!Result) // valid
		{
			if (Caps.wTechnology == AUXCAPS_CDAUDIO)
			{
				CD_ID = counter;
				auxGetVolume(CD_ID,&CD_OrigVolume);
				return;
			}
		}
	}
}

static void CD_SetVolume(unsigned long Volume)
{
	if (CD_ID != -1) 
		auxSetVolume(CD_ID,(Volume<<16)+Volume);
}
#endif	/* USE_AUX_API */

int CDAudio_Init(void)
{
	DWORD	dwReturn;
	MCI_OPEN_PARMS	mciOpenParms;
    MCI_SET_PARMS	mciSetParms;
	int				n;

	if (cls.state == ca_dedicated)
		return -1;

	if (COM_CheckParm("-nocdaudio"))
		return -1;

    Cmd_AddCommand ("cd", CD_f);

    Cvar_RegisterVariable(&bgmvolume, NULL);
	Cvar_RegisterVariable(&bgmtype, NULL);

	mciOpenParms.lpstrDeviceType = "cdaudio";
	dwReturn = mciSendCommand(0, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_SHAREABLE, (DWORD_PTR) (LPVOID) &mciOpenParms);
	if (dwReturn)
	{
		Con_DPrintf("CDAudio_Init: MCI_OPEN failed (%u)\n", (unsigned int)dwReturn);
		return -1;
	}
	wDeviceID = mciOpenParms.wDeviceID;

	// Set the time format to track/minute/second/frame (TMSF).
	mciSetParms.dwTimeFormat = MCI_FORMAT_TMSF;
	dwReturn = mciSendCommand(wDeviceID, MCI_SET, MCI_SET_TIME_FORMAT, (DWORD_PTR) (LPVOID) &mciSetParms);
	if (dwReturn)
	{
		Con_DPrintf("CDAudio_Init: MCI_SET_TIME_FORMAT failed (%u)\n", (unsigned int)dwReturn);
		mciSendCommand(wDeviceID, MCI_CLOSE, 0, (DWORD_PTR)NULL);
		return -1;
	}

	for (n = 0; n < 100; n++)
		remap[n] = n;
    
	initialized = true;
	enabled = true;
	old_cdvolume = bgmvolume.value;

	if (CDAudio_GetAudioDiskInfo())
	{
		Con_Printf("No CD in drive\n");
		cdValid = false;
	}

#ifdef USE_AUX_API
	CD_FindCDAux();
#endif	/* USE_AUX_API */

	Con_Printf("CD Audio initialized\n");

	return 0;
}


void CDAudio_Shutdown(void)
{
	if (!initialized)
		return;
	CDAudio_Stop();
	if (mciSendCommand(wDeviceID, MCI_CLOSE, MCI_WAIT, (DWORD_PTR)NULL))
		Con_DPrintf("CDAudio_Shutdown: MCI_CLOSE failed\n");
}

