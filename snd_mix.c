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
// snd_mix.c -- portable code to mix sounds for snd_dma.c

#include "quakedef.h"

#define	PAINTBUFFER_SIZE	2048 // was 512, expanded to 2048
portable_samplepair_t paintbuffer[PAINTBUFFER_SIZE];
int		snd_scaletable[32][256];
int 	*snd_p, snd_linear_count, snd_vol;
short	*snd_out;

void S_WriteLinearBlastStereo16 (void)
{
	int		i;
	int		val;

	for (i=0 ; i<snd_linear_count ; i+=2)
	{
		val = (snd_p[i]*snd_vol)>>8;
		if (val > 0x7fff)
			snd_out[i] = 0x7fff;
		else if (val < (short)0x8000)
			snd_out[i] = (short)0x8000;
		else
			snd_out[i] = val;

		val = (snd_p[i+1]*snd_vol)>>8;
		if (val > 0x7fff)
			snd_out[i+1] = 0x7fff;
		else if (val < (short)0x8000)
			snd_out[i+1] = (short)0x8000;
		else
			snd_out[i+1] = val;
	}
}

void S_TransferStereo16 (int endtime)
{
	int		lpos;
	int		lpaintedtime;
	
	snd_vol = volume.value*256;

	snd_p = (int *) paintbuffer;
	lpaintedtime = paintedtime;

	while (lpaintedtime < endtime)
	{
	// handle recirculating buffer issues
		lpos = lpaintedtime & ((shm->samples>>1)-1);

		snd_out = (short *) shm->buffer + (lpos<<1);

		snd_linear_count = (shm->samples>>1) - lpos;
		if (lpaintedtime + snd_linear_count > endtime)
			snd_linear_count = endtime - lpaintedtime;

		snd_linear_count <<= 1;

	// write a linear blast of samples
		S_WriteLinearBlastStereo16 ();

		snd_p += snd_linear_count;
		lpaintedtime += (snd_linear_count>>1);
	}
}

void S_TransferPaintBuffer(int endtime)
{
	int 	out_idx;
	int 	count;
	int 	out_mask;
	int 	*p;
	int 	step;
	int		val;
	int		snd_vol;

	if (shm->samplebits == 16 && shm->channels == 2)
	{	// optimized case
		S_TransferStereo16 (endtime);
	}
	else
	{	// general case
		p = (int *) paintbuffer;
		count = (endtime - paintedtime) * shm->channels;
		out_mask = shm->samples - 1; 
		out_idx = paintedtime * shm->channels & out_mask;
		step = 3 - shm->channels;
		snd_vol = volume.value*256;
	
		if (shm->samplebits == 16)
		{
			short *out = (short *) shm->buffer;
			while (count--)
			{
				val = (*p * snd_vol) >> 8;
				p+= step;
				if (val > 0x7fff)
					val = 0x7fff;
				else if (val < (short)0x8000)
					val = (short)0x8000;
				out[out_idx] = val;
				out_idx = (out_idx + 1) & out_mask;
			}
		}
		else if (shm->samplebits == 8)
		{
			byte *out = (byte *) shm->buffer;
			while (count--)
			{
				val = (*p * snd_vol) >> 8;
				p+= step;
				if (val > 0x7fff)
					val = 0x7fff;
				else if (val < (short)0x8000)
					val = (short)0x8000;
				out[out_idx] = (val>>8) + 128;
				out_idx = (out_idx + 1) & out_mask;
			}
		}
	}
}


/*
===============================================================================

UNDERWATER EFFECT

===============================================================================
*/

static struct {
	float	intensity;
	float	alpha;
	float	accum[2];
} underwater = {0.f, 1.f, {0.f, 0.f}};

extern cvar_t snd_waterfx;

void S_SetUnderwaterIntensity (float target)
{
	target *= CLAMP (0.f, snd_waterfx.value, 2.f);
	if (underwater.intensity < target)
	{
		underwater.intensity += host_frametime * 4.f;
		underwater.intensity = min (underwater.intensity, target);
	}
	else if (underwater.intensity > target)
	{
		underwater.intensity -= host_frametime * 4.f;
		underwater.intensity = max (underwater.intensity, target);
	}
	underwater.alpha = exp (-underwater.intensity * log (12.f));
}

void S_UnderwaterFilter (int endtime)
{
	int i;
	for (i = 0; i < endtime; i++)
	{
		underwater.accum[0] += underwater.alpha * (paintbuffer[i].left  - underwater.accum[0]);
		underwater.accum[1] += underwater.alpha * (paintbuffer[i].right - underwater.accum[1]);
		paintbuffer[i].left  = (int) (underwater.accum[0] + 0.5f);
		paintbuffer[i].right = (int) (underwater.accum[1] + 0.5f);
	}
}


/*
===============================================================================

CHANNEL MIXING

===============================================================================
*/

void S_PaintChannelFrom8 (channel_t *ch, sfxcache_t *sc, int endtime);
void S_PaintChannelFrom16 (channel_t *ch, sfxcache_t *sc, int endtime);

void S_PaintChannels(int endtime)
{
	int 	i;
	int 	end;
	channel_t *ch;
	sfxcache_t	*sc;
	int		ltime, count, wdog;

	while (paintedtime < endtime)
	{
	// if paintbuffer is smaller than DMA buffer
		end = endtime;
		if (endtime - paintedtime > PAINTBUFFER_SIZE)
			end = paintedtime + PAINTBUFFER_SIZE;

	// clear the paint buffer
		memset(paintbuffer, 0, (end - paintedtime) * sizeof(portable_samplepair_t));

	// paint in the channels.
		ch = channels;
		for (i=0; i<total_channels ; i++, ch++)
		{
			if (!ch->sfx)
				continue;
			if (!ch->leftvol && !ch->rightvol)
				continue;
			sc = S_LoadSound (ch->sfx);
			if (!sc)
				continue;

			wdog = 0;
			ltime = paintedtime;

			while (ltime < end)
			{	// paint up to end
				if (ch->end < end)
					count = ch->end - ltime;
				else
					count = end - ltime;

				if (count > 0)
				{	
					if (sc->width == 1)
						S_PaintChannelFrom8(ch, sc, count);
					else
						S_PaintChannelFrom16(ch, sc, count);
	
					ltime += count;
				}
				else if (++wdog > 1024)
				{
					Con_Warning ("S_PaintChannels: runaway loop for '%s'\n", ch->sfx->name);
					ch->sfx = NULL;
					break;
				}

			// if at end of loop, restart
				if (ltime >= ch->end)
				{
					if (sc->loopstart >= 0)
					{
						ch->pos = sc->loopstart;
						ch->end = ltime + sc->length - ch->pos;
					}
					else				
					{	// channel just stopped
						ch->sfx = NULL;
						break;
					}
				}
			}
		}

	// apply the underwater effect
		S_UnderwaterFilter (end - paintedtime);

	// transfer out according to DMA format
		S_TransferPaintBuffer(end);
		paintedtime = end;
	}
}


/*
================
S_InitScaletable
================
*/
void S_InitScaletable (void)
{
	int		i, j;
	
	for (i=0 ; i<32 ; i++)
		for (j=0 ; j<256 ; j++)
//			snd_scaletable[i][j] = ((signed char)j) * i * 8;
			snd_scaletable[i][j] = ((j < 128) ? j : j - 256) * i * 8; // gcc optimization bug-fix
}


void S_PaintChannelFrom8 (channel_t *ch, sfxcache_t *sc, int count)
{
	int 	data;
	int		*lscale, *rscale;
	byte *sfx;
	int		i;

	if (ch->leftvol > 255)
		ch->leftvol = 255;
	if (ch->rightvol > 255)
		ch->rightvol = 255;
		
	lscale = snd_scaletable[ch->leftvol >> 3];
	rscale = snd_scaletable[ch->rightvol >> 3];
	sfx = (byte *)sc->data + ch->pos;

	for (i=0 ; i<count ; i++)
	{
		data = sfx[i];
		paintbuffer[i].left += lscale[data];
		paintbuffer[i].right += rscale[data];
	}
	
	ch->pos += count;
}


void S_PaintChannelFrom16 (channel_t *ch, sfxcache_t *sc, int count)
{
	int data;
	int left, right;
	int leftvol, rightvol;
	signed short *sfx;
	int	i;

	leftvol = ch->leftvol;
	rightvol = ch->rightvol;
	sfx = (signed short *)sc->data + ch->pos;

	for (i=0 ; i<count ; i++)
	{
		data = sfx[i];
		left = (data * leftvol) >> 8;
		right = (data * rightvol) >> 8;
		paintbuffer[i].left += left;
		paintbuffer[i].right += right;
	}

	ch->pos += count;
}

