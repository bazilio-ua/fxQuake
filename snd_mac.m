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
// snd_mac.m

#include "quakedef.h"
#include "unixquake.h"
#include "macquake.h"

AudioDeviceID outputDeviceID = kAudioDeviceUnknown;
//static AudioDeviceID outputDeviceID = kAudioDeviceUnknown;
static AudioStreamBasicDescription outputStreamBasicDescription;
static AudioDeviceIOProcID ioprocid = NULL;

// 64K is > 1 second at 16-bit, 22050 Hz
#define	WAV_BUFFERS				64
#define	WAV_BUFFER_SIZE			1024 // chunk size
#define OUTPUT_BUFFER_SIZE	(2 * WAV_BUFFER_SIZE)

static byte buffer[WAV_BUFFERS * WAV_BUFFER_SIZE];
static UInt32 bufferPosition;

static qboolean snd_inited;

/*
===============
audioDeviceIOProc
===============
*/
OSStatus audioDeviceIOProc(AudioDeviceID inDevice,
                           const AudioTimeStamp *inNow,
                           const AudioBufferList *inInputData,
                           const AudioTimeStamp *inInputTime,
                           AudioBufferList *outOutputData,
                           const AudioTimeStamp *inOutputTime,
                           void *inClientData)
{
    UInt32 sampleIndex;
    short *samples = (short *)buffer + bufferPosition / (shm->samplebits >> 3);
    float *outBuffer = (float *)outOutputData->mBuffers[0].mData;
    float scale = 1.0f / 32768.0f;
    
    // Convert the buffer to float, required by CoreAudio
    for (sampleIndex = 0; sampleIndex < OUTPUT_BUFFER_SIZE; sampleIndex++)
    {
        // Convert the samples from shorts to floats.  Scale the floats to be [-1..1].
        outBuffer[sampleIndex] = samples[sampleIndex] * scale;
    }
    
    // Increase the buffer position. This is the next buffer we will submit
    bufferPosition += OUTPUT_BUFFER_SIZE * (shm->samplebits >> 3);
    if (bufferPosition >= sizeof (buffer))
        bufferPosition = 0;
    
	return kAudioHardwareNoError;
}

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
    OSStatus status;
    UInt32 propertySize;
    AudioObjectPropertyAddress propertyAddress;
    AudioValueRange bufferSizeRange;
    UInt32 bufferSize;
    int i;
    
    snd_inited = false;
	shm = &sn;
    
    // Get the output device
    propertySize = sizeof(outputDeviceID);
    propertyAddress.mElement = kAudioObjectPropertyElementMaster;    
    propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
    status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &propertySize, &outputDeviceID);
    if (status) {
        Con_Printf("AudioHardwareGetProperty returned %d\n", status);
        return false;
    }
    
    if (outputDeviceID == kAudioDeviceUnknown) {
        Con_Printf("AudioHardwareGetProperty: outputDeviceID is kAudioDeviceUnknown\n");
        return false;
    }
    
    // Get the buffer range on output device
    propertySize = sizeof(bufferSizeRange);
    propertyAddress.mScope = kAudioDevicePropertyScopeOutput;
    propertyAddress.mSelector = kAudioDevicePropertyBufferSizeRange;
    status = AudioObjectGetPropertyData(outputDeviceID, &propertyAddress, 0, NULL, &propertySize, &bufferSizeRange);
    if (status) {
        Con_Printf("AudioDeviceGetProperty: returned %d when getting kAudioDevicePropertyBufferSizeRange\n", status);
        return false;
    }
    
    // Configure the output device	
    propertySize = sizeof(bufferSize);
    propertyAddress.mSelector = kAudioDevicePropertyBufferSize;
    
    bufferSize = OUTPUT_BUFFER_SIZE * sizeof(float);
    
    status = AudioObjectSetPropertyData(outputDeviceID, &propertyAddress, 0, NULL, propertySize, &bufferSize);
    if (status) {
        Con_Printf("AudioDeviceSetProperty: returned %d when setting kAudioDevicePropertyBufferSize to %d\n", status, bufferSize);
        return false;
    }
    status = AudioObjectGetPropertyData(outputDeviceID, &propertyAddress, 0, NULL, &propertySize, &bufferSize);
    if (status) {
        Con_Printf("AudioDeviceGetProperty: returned %d when getting kAudioDevicePropertyBufferSize\n", status);
        return false;
    }
    
    // Print out the device status
    propertySize = sizeof(outputStreamBasicDescription);
    propertyAddress.mSelector = kAudioDevicePropertyStreamFormat;
    status = AudioObjectGetPropertyData(outputDeviceID, &propertyAddress, 0, NULL, &propertySize, &outputStreamBasicDescription);
    if (status) {
        Con_Printf("AudioDeviceGetProperty: returned %d when getting kAudioDevicePropertyStreamFormat\n", status);
        return false;
    }
    
    Con_Printf("Hardware format:\n");
    Con_Printf("  %f mSampleRate\n", outputStreamBasicDescription.mSampleRate);
    Con_Printf("  %c%c%c%c mFormatID\n",
               (outputStreamBasicDescription.mFormatID & 0xff000000) >> 24,
               (outputStreamBasicDescription.mFormatID & 0x00ff0000) >> 16,
               (outputStreamBasicDescription.mFormatID & 0x0000ff00) >>  8,
               (outputStreamBasicDescription.mFormatID & 0x000000ff) >>  0);
    Con_Printf("  %5d mBytesPerPacket\n", outputStreamBasicDescription.mBytesPerPacket);
    Con_Printf("  %5d mFramesPerPacket\n", outputStreamBasicDescription.mFramesPerPacket);
    Con_Printf("  %5d mBytesPerFrame\n", outputStreamBasicDescription.mBytesPerFrame);
    Con_Printf("  %5d mChannelsPerFrame\n", outputStreamBasicDescription.mChannelsPerFrame);
    Con_Printf("  %5d mBitsPerChannel\n", outputStreamBasicDescription.mBitsPerChannel);
    
    if (outputStreamBasicDescription.mFormatID != kAudioFormatLinearPCM) {
        Con_Printf("Default Audio Device doesn't support Linear PCM!\n");
        return false;
    }
    
    // Add the sound to IOProc
    status = AudioDeviceCreateIOProcID(outputDeviceID, audioDeviceIOProc, NULL, &ioprocid);
    if (status) {
        Con_Printf("AudioDeviceAddIOProc: returned %d\n", status);
        return false;
    }

    if (ioprocid == NULL) {
        Con_Printf("Cannot create IOProcID\n");
        return false;
    }
    
    // Tell the main app what we expect from it
    shm->samplebits = 16;
//    shm->speed = 44100;
    
	// sound speed
	if ((i = COM_CheckParm("-sndspeed")) != 0 && i < com_argc - 1)
		shm->speed = atoi(com_argv[i + 1]);
	else
		shm->speed = 44100;
    
    shm->width = outputStreamBasicDescription.mBytesPerFrame / 8;
    shm->channels = 2;
    shm->samples = sizeof(buffer) / (shm->samplebits >> 3);
    shm->samplepos = 0;
    shm->submission_chunk = OUTPUT_BUFFER_SIZE;
    shm->buffer = buffer;
    
    // We haven't enqueued anything yet
    bufferPosition = 0;
    
    // Start sound running
    status = AudioDeviceStart(outputDeviceID, ioprocid);
    if (status) {
        Con_Printf("AudioDeviceStart: returned %d\n", status);
        return false;
    }
    
    snd_inited = true;
    
	return true;
}

/*
===============
SNDDMA_GetDMAPos
===============
*/
int SNDDMA_GetDMAPos(void)
{
	if (!snd_inited) 
		return 0;
    
    return bufferPosition / (shm->samplebits >> 3);
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
        // do shutdown
        AudioDeviceStop(outputDeviceID, ioprocid);
        
        AudioDeviceDestroyIOProcID(outputDeviceID, ioprocid);
        
        snd_inited = false;
    }
}

