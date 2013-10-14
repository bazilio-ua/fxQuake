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
// snd_unix.c

#include "quakedef.h"
#include "xquake.h"

int audio_fd;
int snd_inited;
static char default_snd_dev[] = _PATH_DEV "dsp";
static char *snd_dev = default_snd_dev;

static int tryrates[] = { 11025, 22050, 44100, 48000, 88200, 96000 };
static int num_tryrates = sizeof(tryrates) / sizeof(int);

/*
==================
S_BlockSound
==================
*/
void S_BlockSound (void)
{
	snd_blocked++;
}

/*
==================
S_UnblockSound
==================
*/
void S_UnblockSound (void)
{
	snd_blocked--;
}

/*
===============
SNDDMA_Init
===============
*/
qboolean SNDDMA_Init(void)
{
	int fmt;
	int tmp;
	int i;
	struct audio_buf_info info;
	int caps;

	snd_inited = 0;

	if ((i = COM_CheckParm("-snddev")) != 0 && i < com_argc - 1) 
		snd_dev = com_argv[i + 1];

// open snd_dev, confirm capability to mmap, and get size of dma buffer
	audio_fd = open(snd_dev, O_RDWR);
	if (audio_fd < 0)
	{
		perror(snd_dev);
		Con_Printf("Could not open %s\n", snd_dev);
		return 0;
	}

	if (ioctl(audio_fd, SNDCTL_DSP_RESET, 0) == -1)
	{
		perror(snd_dev);
		Con_Printf("Could not reset %s\n", snd_dev);
		close(audio_fd);
		return 0;
	}

	if (ioctl(audio_fd, SNDCTL_DSP_GETCAPS, &caps) == -1)
	{
		perror(snd_dev);
		Con_Printf("Could not retrieve %s capabilities.\n", snd_dev);
		close(audio_fd);
		return 0;
	}

	if (!(caps & DSP_CAP_TRIGGER) || !(caps & DSP_CAP_MMAP))
	{
		Con_Printf("Driver for %s doesn't support mmap or trigger\n", snd_dev);
		close(audio_fd);
		return 0;
	}

	shm = &sn;

// get sample bits format
	if ((i = COM_CheckParm("-sndbits")) != 0 && i < com_argc - 1)
		shm->samplebits = atoi(com_argv[i + 1]);
	else
		shm->samplebits = 0;

	if (shm->samplebits != 16 && shm->samplebits != 8 && shm->samplebits != 0)
		shm->samplebits = 0;

// try what the device gives us
	if (ioctl(audio_fd, SNDCTL_DSP_GETFMTS, &fmt) == -1)
	{
		perror(snd_dev);
		Con_Printf("Unable to retrieve supported formats.\n");
		close(audio_fd);
		return 0;
	}

	if (shm->samplebits == 0)
	{
		if (fmt & AFMT_S16_NE)
			shm->samplebits = 16;
		else if (fmt & AFMT_U8)
			shm->samplebits = 8;
	}

// try to set the requested format
	if (shm->samplebits == 16)
	{
		fmt = AFMT_S16_NE;
		if (ioctl(audio_fd, SNDCTL_DSP_SETFMT, &fmt) == -1)
		{
			perror(snd_dev);
			Con_Printf("Could not support 16-bit data. Try 8-bit.\n");
			close(audio_fd);
			return 0;
		}
	}
	else if (shm->samplebits == 8)
	{
		fmt = AFMT_U8;
		if (ioctl(audio_fd, SNDCTL_DSP_SETFMT, &fmt) == -1)
		{
			perror(snd_dev);
			Con_Printf("Could not support 8-bit data.\n");
			close(audio_fd);
			return 0;
		}
	}
	else
	{
		perror(snd_dev);
		Con_Printf("Neither 16 nor 8 bit format supported.\n");
		close(audio_fd);
		return 0;
	}

// set number of channels (only allow 1 or 2 for now)
	if ((i = COM_CheckParm("-sndmono")) != 0)
		shm->channels = 1;
	else if ((i = COM_CheckParm("-sndstereo")) != 0)
		shm->channels = 2;
	else 
		shm->channels = 2;

	if (ioctl(audio_fd, SNDCTL_DSP_CHANNELS, &shm->channels) == -1)
	{
		perror(snd_dev);
		Con_Printf("Could not set number of channels == %d on %s\n", shm->channels, snd_dev);
		close(audio_fd);
		return 0;
	}

// now get the requested speed or try to find one that works
	if ((i = COM_CheckParm("-sndspeed")) != 0 && i < com_argc - 1)
		shm->speed = atoi(com_argv[i + 1]);
	else
		shm->speed = 0;

	if (shm->speed == 0) 
	{
		for (i = 2; i < num_tryrates; ++i) // was 0, let's try from cd-quality [0 -> 2]
		{
			shm->speed = tryrates[i];
			if (ioctl(audio_fd, SNDCTL_DSP_SPEED, &shm->speed) == -1)
				perror(snd_dev);
			else if (tryrates[i] == shm->speed)
				break;
		}
		if (i == num_tryrates)
		{
			Con_Printf("Couldn't find a suitable sample speed.\n");
			close(audio_fd);
			return 0;
		}
	} 
	else 
	{
		if (ioctl(audio_fd, SNDCTL_DSP_SPEED, &shm->speed) == -1)
		{
			perror(snd_dev);
			Con_Printf("Could not set %s sample speed to %d", snd_dev, shm->speed);
			close(audio_fd);
			return 0;
		}
	}

// check how much space we have for non-blocking output
	if (ioctl(audio_fd, SNDCTL_DSP_GETOSPACE, &info) == -1)
	{
		perror("GETOSPACE");
		Con_Printf("Um, can't do GETOSPACE?\n");
		close(audio_fd);
		return 0;
	}

	shm->samples = info.fragstotal * info.fragsize / (shm->samplebits / 8);
	shm->submission_chunk = 1;

// memory map the dma buffer
// MAP_FILE required for some other unicies (HP-UX is one I think)
	shm->buffer = (byte *) mmap(NULL, info.fragstotal * info.fragsize, PROT_WRITE, MAP_FILE | MAP_SHARED, audio_fd, 0);
	if (!shm->buffer || shm->buffer == (byte *)MAP_FAILED)
	{
		perror(snd_dev);
		Con_Printf("Could not mmap %s\n", snd_dev);
		close(audio_fd);
		return 0;
	}

// toggle the trigger & start her up
	tmp = ~PCM_ENABLE_INPUT & ~PCM_ENABLE_OUTPUT;
	if (ioctl(audio_fd, SNDCTL_DSP_SETTRIGGER, &tmp) == -1)
	{
		perror(snd_dev);
		Con_Printf("Could not toggle %s.\n", snd_dev);
		close(audio_fd);
		return 0;
	}
	tmp = PCM_ENABLE_OUTPUT;
	if (ioctl(audio_fd, SNDCTL_DSP_SETTRIGGER, &tmp) == -1)
	{
		perror(snd_dev);
		Con_Printf("Could not toggle %s.\n", snd_dev);
		close(audio_fd);
		return 0;
	}

	shm->samplepos = 0;

	snd_inited = 1;
	return 1;

}

/*
===============
SNDDMA_GetDMAPos
===============
*/
int SNDDMA_GetDMAPos(void)
{

	struct count_info count;

	if (!snd_inited) 
		return 0;

	if (ioctl(audio_fd, SNDCTL_DSP_GETOPTR, &count) == -1)
	{
		perror(snd_dev);
		Con_Printf("Uh, sound dead.\n");
		close(audio_fd);
		snd_inited = 0;
		return 0;
	}
//	shm->samplepos = (count.bytes / (shm->samplebits / 8)) & (shm->samples-1);
//	fprintf(stderr, "%d    \r", count.ptr);
	shm->samplepos = count.ptr / (shm->samplebits / 8);

	return shm->samplepos;

}

/*
===============
SNDDMA_BeginPainting
===============
*/
void SNDDMA_BeginPainting(void)
{
}

/*
===============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/
void SNDDMA_Submit(void)
{
}

/*
===============
SNDDMA_Shutdown
===============
*/
void SNDDMA_Shutdown(void)
{
	if (snd_inited)
	{
		close(audio_fd);
		snd_inited = 0;
	}
}

