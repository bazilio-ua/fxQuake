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

static AudioDeviceID outputDeviceID;
static AudioStreamBasicDescription outputStreamBasicDescription;

AudioDeviceID deviceID;
AudioDeviceID builtinDeviceID;
UInt32 source;

int chunkSize = 2048;
int bufferSize = 16384;

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
    UInt32 propertySize, bufferByteCount;
    AudioValueRange bufferFrameRange;
    AudioObjectPropertyAddress propertyAddress;
    
    propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement = kAudioObjectPropertyElementMaster;    
    
    // Get the output device
    propertySize = sizeof(outputDeviceID);
    propertyAddress.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
    status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &propertySize, &outputDeviceID);
//    status = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice, &propertySize, &outputDeviceID);
    if (status) {
        Con_Printf("AudioHardwareGetProperty returned %d\n", status);
        return false;
    }
    
    if (outputDeviceID == kAudioDeviceUnknown) {
        Con_Printf("AudioHardwareGetProperty: outputDeviceID is kAudioDeviceUnknown\n");
        return false;
    }
    
    // Get the buffer range on output device
    propertySize = sizeof(bufferFrameRange);
    propertyAddress.mSelector = kAudioDevicePropertyBufferFrameSizeRange;
    status = AudioObjectGetPropertyData(outputDeviceID, &propertyAddress, 0, NULL, &propertySize, &bufferFrameRange);
    if (status) {
        Con_Printf("AudioDeviceGetProperty: returned %d when getting kAudioDevicePropertyBufferSizeRange\n", status);
        return false;
    }

    
    // Configure the output device	
    propertySize = sizeof(bufferByteCount);
    propertyAddress.mSelector = kAudioDevicePropertyBufferFrameSize;
    
    bufferByteCount = chunkSize;
    
    status = AudioObjectSetPropertyData(outputDeviceID, &propertyAddress, 0, NULL, propertySize, &bufferByteCount);
//    status = AudioDeviceSetProperty(outputDeviceID, NULL, 0, NO, kAudioDevicePropertyBufferSize, propertySize, &bufferByteCount);
    if (status) {
        Con_Printf("AudioDeviceSetProperty: returned %d when setting kAudioDevicePropertyBufferSize to %d\n", status, bufferByteCount);
        return false;
    }
    
    status = AudioObjectGetPropertyData(outputDeviceID, &propertyAddress, 0, NULL, &propertySize, &bufferByteCount);
//    status = AudioDeviceGetProperty(outputDeviceID, 0, NO, kAudioDevicePropertyBufferSize, &propertySize, &bufferByteCount);
    if (status) {
        Con_Printf("AudioDeviceGetProperty: returned %d when getting kAudioDevicePropertyBufferSize\n", status);
        return false;
    }
    
    // Print out the device status
    propertySize = sizeof(outputStreamBasicDescription);
    propertyAddress.mSelector = kAudioDevicePropertyStreamFormat;
    propertyAddress.mScope = kAudioDevicePropertyScopeOutput;
    
    status = AudioObjectGetPropertyData(outputDeviceID, &propertyAddress, 0, NULL, &propertySize, &outputStreamBasicDescription);
//    status = AudioDeviceGetProperty(outputDeviceID, 0, NO, kAudioDevicePropertyStreamFormat, &propertySize, &outputStreamBasicDescription);
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
    
    // Start sound running
//    status = AudioDeviceAddIOProc(outputDeviceID, audioDeviceIOProc, NULL);
//    if (status) {
//        Con_Printf("AudioDeviceAddIOProc: returned %d\n", status);
//        return false;
//    }

    return false;
    
//	return true;
}

/*
===============
SNDDMA_GetDMAPos
===============
*/
int SNDDMA_GetDMAPos(void)
{
	return 0;
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
	
}

