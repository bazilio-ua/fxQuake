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

#define CHUNK_SIZE 1024

static AudioDeviceID outputDeviceID = kAudioDeviceUnknown;
static AudioStreamBasicDescription outputStreamBasicDescription;
static AudioDeviceIOProcID ioprocid = NULL;

static AudioValueRange bufferSizeRange;
static UInt32 bufferByteCount;
static UInt32 bufferPosition;

static byte buffer[64*1024];
//static unsigned char 			gSoundBuffer[64*1024];

//#define OUTPUT_BUFFER_SIZE	(4 * 1024)
#define OUTPUT_BUFFER_SIZE	(2 * 1024)


//int chunkSize = 2048; // 2 * 1024
//int bufferSize = 65536; // 64 * 1024
//int bufferSize = 16384; // 64 * 1024



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
    
    
//    UInt32 sampleIndex;
//    short *samples = ((short *)buffer) + bufferPosition / (shm->samplebits >> 3);
//    float scale = (1.0f / 32768.0f);
//    float *outBuffer = (float *)outOutputData->mBuffers[0].mData;
//    
//    // Convert the buffer to float, required by CoreAudio
//    // Convert the samples from shorts to floats.  Scale the floats to be [-1..1].
//    for (sampleIndex=0 ; sampleIndex<bufferByteCount ; sampleIndex++)
//    {
//        *outBuffer++ = (*samples) * scale;
//        *samples++ = 0x0000;
//    }
//    
//    // Increase the buffer position
//    // this is the next buffer we will submit
//    bufferPosition += bufferByteCount * (shm->samplebits >> 3);
//    if (bufferPosition >= sizeof(buffer))
//        bufferPosition = 0;
//	
//    return kAudioHardwareNoError;
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
    
    snd_inited = false;
	shm = &sn;

    propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement = kAudioObjectPropertyElementMaster;    
    
    // Get the output device
    propertySize = sizeof(outputDeviceID);
    propertyAddress.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
//    status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &propertySize, &outputDeviceID);
    status = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice, &propertySize, &outputDeviceID);
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
//    propertyAddress.mSelector = kAudioDevicePropertyBufferFrameSizeRange;
    propertyAddress.mSelector = kAudioDevicePropertyBufferSizeRange;
    
//    status = AudioObjectGetPropertyData(outputDeviceID, &propertyAddress, 0, NULL, &propertySize, &bufferFrameRange);
    status = AudioDeviceGetProperty(outputDeviceID, 0, NO, kAudioDevicePropertyBufferSizeRange, &propertySize, &bufferSizeRange);
    if (status) {
        Con_Printf("AudioDeviceGetProperty: returned %d when getting kAudioDevicePropertyBufferSizeRange\n", status);
        return false;
    }

    
    // Configure the output device	
    propertySize = sizeof(bufferByteCount);
//    propertyAddress.mSelector = kAudioDevicePropertyBufferFrameSize;
    propertyAddress.mSelector = kAudioDevicePropertyBufferSize;
    
//    bufferByteCount = chunkSize * sizeof(float);
    bufferByteCount = OUTPUT_BUFFER_SIZE * sizeof(float);
    
//    status = AudioObjectSetPropertyData(outputDeviceID, &propertyAddress, 0, NULL, propertySize, &bufferByteCount);
    status = AudioDeviceSetProperty(outputDeviceID, NULL, 0, NO, kAudioDevicePropertyBufferSize, propertySize, &bufferByteCount);
    if (status) {
        Con_Printf("AudioDeviceSetProperty: returned %d when setting kAudioDevicePropertyBufferSize to %d\n", status, bufferByteCount);
        return false;
    }
    
//    status = AudioObjectGetPropertyData(outputDeviceID, &propertyAddress, 0, NULL, &propertySize, &bufferByteCount);
    status = AudioDeviceGetProperty(outputDeviceID, 0, NO, kAudioDevicePropertyBufferSize, &propertySize, &bufferByteCount);
    if (status) {
        Con_Printf("AudioDeviceGetProperty: returned %d when getting kAudioDevicePropertyBufferSize\n", status);
        return false;
    }
    
    // Print out the device status
    propertySize = sizeof(outputStreamBasicDescription);
    propertyAddress.mSelector = kAudioDevicePropertyStreamFormat;
    propertyAddress.mScope = kAudioDevicePropertyScopeOutput;
    
//    status = AudioObjectGetPropertyData(outputDeviceID, &propertyAddress, 0, NULL, &propertySize, &outputStreamBasicDescription);
    status = AudioDeviceGetProperty(outputDeviceID, 0, NO, kAudioDevicePropertyStreamFormat, &propertySize, &outputStreamBasicDescription);
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
        Con_Printf("Default Audio Device doesn't support Linear PCM!");
        return false;
    }
    
    // Add the sound to IOProc
    status = AudioDeviceAddIOProc(outputDeviceID, audioDeviceIOProc, NULL);
//    status = AudioDeviceCreateIOProcID(outputDeviceID, audioDeviceIOProc, NULL, &ioprocid);
    if (status) {
        Con_Printf("AudioDeviceAddIOProc: returned %d\n", status);
        return false;
    }

//    if (ioprocid == NULL) {
//        Con_Printf("Cannot create IOProcID\n");
//        return false;
//    }
    
    
    // Tell the main app what we expect from it
//    shm->samplebits = 8;
    shm->samplebits = 16;
//    shm->samplebits = outputStreamBasicDescription.mBytesPerFrame;
//    shm->speed = 96000;
//    shm->speed = 88200;
//    shm->speed = 48000;
    shm->speed = 44100;
//    shm->speed = 22050;
//    shm->speed = 11025;
//    shm->speed = outputStreamBasicDescription.mSampleRate;
    shm->channels = 2;
//    shm->channels = outputStreamBasicDescription.mChannelsPerFrame;
//    shm->samples = bufferSize; // crash
//    shm->samples = 16384;
    shm->samples = sizeof(buffer) / (shm->samplebits >> 3);
//    shm->samples = sizeof(buffer) / 4;
    shm->samplepos = 0;
//    shm->submission_chunk = bufferByteCount;
    shm->submission_chunk = OUTPUT_BUFFER_SIZE;
    shm->buffer = buffer;
    
    // We haven't enqueued anything yet
    bufferPosition = 0;
    
    // Start sound running
    status = AudioDeviceStart(outputDeviceID, audioDeviceIOProc);
//    status = AudioDeviceStart(outputDeviceID, ioprocid);
    if (status) {
        Con_Printf("AudioDeviceStart: returned %d\n", status);
        return false;
    }
    
    
    snd_inited = true;
    
//    return false; // DEBUG
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
    
//    shm->samplepos = bufferPosition / (shm->samplebits >> 3);
//    
//	return shm->samplepos;

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
        
//        AudioDeviceStop(outputDeviceID, audioDeviceIOProc);
//        
////        AudioDeviceRemoveIOProc(outputDeviceID, audioDeviceIOProc);
//        AudioDeviceDestroyIOProcID(outputDeviceID, ioprocid);
        
        
        AudioDeviceStop (outputDeviceID, audioDeviceIOProc);
        AudioDeviceRemoveIOProc (outputDeviceID, audioDeviceIOProc);
        
        
        snd_inited = false;
    }
}

