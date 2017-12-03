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

Boolean audioGraphIsRunning = false;
AudioUnitElement unitElement = 0;

AUGraph audioGraph;
AUNode outputNode;
AUNode mixerNode;
AUNode converterNode;
AudioUnit mixerUnit;
AudioUnit converterUnit;

AudioDeviceID audioDevice = kAudioDeviceUnknown;
static AudioStreamBasicDescription outputStreamBasicDescription;
static AudioDeviceIOProcID ioprocid = NULL;

// 64K is > 1 second at 16-bit, 22050 Hz
#define	WAV_BUFFERS				64
#define	WAV_BUFFER_SIZE			1024 // chunk size
//#define OUTPUT_BUFFER_SIZE	(2 * WAV_BUFFER_SIZE)
#define OUTPUT_BUFFER_SIZE	(4 * WAV_BUFFER_SIZE)
//#define OUTPUT_BUFFER_SIZE	(1 * WAV_BUFFER_SIZE)

static byte buffer[WAV_BUFFERS * WAV_BUFFER_SIZE];
static UInt32 bufferPosition;

static qboolean snd_inited;

OSStatus renderCallback(void *inRefCon,
                        AudioUnitRenderActionFlags *ioActionFlags,
                        const AudioTimeStamp *inTimeStamp,
                        UInt32 inBusNumber,
                        UInt32 inNumberFrames,
                        AudioBufferList *ioData);

/*
 ===============
 renderCallback
 ===============
 */
OSStatus renderCallback(void *inRefCon,
                        AudioUnitRenderActionFlags *ioActionFlags,
                        const AudioTimeStamp *inTimeStamp,
                        UInt32 inBusNumber,
                        UInt32 inNumberFrames,
                        AudioBufferList *ioData)
{
    byte *outBuffer = (byte *)ioData->mBuffers[0].mData;
    UInt32 outBufferByteSize = ioData->mBuffers[0].mDataByteSize;
    
    memcpy((void *)outBuffer, &(buffer[bufferPosition]), outBufferByteSize);
    
    // Increase the buffer position. This is the next buffer we will submit
    bufferPosition += outBufferByteSize;
    if (bufferPosition >= sizeof (buffer))
        bufferPosition = 0;
    
    return kAudioHardwareNoError;
}


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
//    UInt32 propertySize;
//    AudioObjectPropertyAddress propertyAddress;
//    AudioValueRange bufferSizeRange;
//    UInt32 bufferSize;
    int i;
    
    snd_inited = false;
	shm = &sn;
    
    
    status = NewAUGraph(&audioGraph);
    if (status) {
        Con_DPrintf("NewAUGraph returned %d\n", status);
        return false;
    }
    
    AudioComponentDescription outputDescription;
    outputDescription.componentType = kAudioUnitType_Output;
    outputDescription.componentSubType = kAudioUnitSubType_DefaultOutput;
    outputDescription.componentManufacturer = kAudioUnitManufacturer_Apple;
    outputDescription.componentFlags = 0;
    outputDescription.componentFlagsMask = 0;
    
    status = AUGraphAddNode(audioGraph, &outputDescription, &outputNode);
    if (status) {
        Con_DPrintf("AUGraphAddNode returned %d\n", status);
        return false;
    }

    AudioComponentDescription mixerDescription;
    mixerDescription.componentType = kAudioUnitType_Mixer;
    mixerDescription.componentSubType = kAudioUnitSubType_StereoMixer;
    mixerDescription.componentManufacturer = kAudioUnitManufacturer_Apple;
    mixerDescription.componentFlags = 0;
    mixerDescription.componentFlagsMask = 0;
    
    status = AUGraphAddNode(audioGraph, &mixerDescription, &mixerNode);
    if (status) {
        Con_DPrintf("AUGraphAddNode returned %d\n", status);
        return false;
    }
    
    status = AUGraphConnectNodeInput(audioGraph, mixerNode, 0, outputNode, 0);
    if (status) {
        Con_DPrintf("AUGraphConnectNodeInput returned %d\n", status);
        return false;
    }
    
    status = AUGraphOpen(audioGraph);
    if (status) {
        Con_DPrintf("AUGraphOpen returned %d\n", status);
        return false;
    }
    
    status = AUGraphInitialize(audioGraph);
    if (status) {
        Con_DPrintf("AUGraphInitialize returned %d\n", status);
        return false;
    }
    
    status = AUGraphNodeInfo(audioGraph, mixerNode, 0, &mixerUnit);
    if (status) {
        Con_DPrintf("AUGraphNodeInfo returned %d\n", status);
        return false;
    }
        
/*    
    // Get the output device
    propertySize = sizeof(audioDevice);
    propertyAddress.mElement = kAudioObjectPropertyElementMaster;    
    propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
    status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &propertySize, &audioDevice);
    if (status) {
        Con_DPrintf("AudioHardwareGetProperty returned %d\n", status);
        return false;
    }
    
    if (audioDevice == kAudioDeviceUnknown) {
        Con_DPrintf("AudioHardwareGetProperty: audioDevice is kAudioDeviceUnknown\n");
        return false;
    }
    
    // Get the buffer range on output device
    propertySize = sizeof(bufferSizeRange);
    propertyAddress.mScope = kAudioDevicePropertyScopeOutput;
    propertyAddress.mSelector = kAudioDevicePropertyBufferSizeRange;
    status = AudioObjectGetPropertyData(audioDevice, &propertyAddress, 0, NULL, &propertySize, &bufferSizeRange);
    if (status) {
        Con_DPrintf("AudioDeviceGetProperty: returned %d when getting kAudioDevicePropertyBufferSizeRange\n", status);
        return false;
    }
    
    // Configure the output device	
    propertySize = sizeof(bufferSize);
    propertyAddress.mSelector = kAudioDevicePropertyBufferSize;
    
    bufferSize = OUTPUT_BUFFER_SIZE * sizeof(float);
    
    status = AudioObjectSetPropertyData(audioDevice, &propertyAddress, 0, NULL, propertySize, &bufferSize);
    if (status) {
        Con_DPrintf("AudioDeviceSetProperty: returned %d when setting kAudioDevicePropertyBufferSize to %d\n", status, bufferSize);
        return false;
    }
    status = AudioObjectGetPropertyData(audioDevice, &propertyAddress, 0, NULL, &propertySize, &bufferSize);
    if (status) {
        Con_DPrintf("AudioDeviceGetProperty: returned %d when getting kAudioDevicePropertyBufferSize\n", status);
        return false;
    }
    
    // Print out the device status
    propertySize = sizeof(outputStreamBasicDescription);
    propertyAddress.mSelector = kAudioDevicePropertyStreamFormat;
    status = AudioObjectGetPropertyData(audioDevice, &propertyAddress, 0, NULL, &propertySize, &outputStreamBasicDescription);
    if (status) {
        Con_DPrintf("AudioDeviceGetProperty: returned %d when getting kAudioDevicePropertyStreamFormat\n", status);
        return false;
    }
    
    Con_DPrintf("Hardware format:\n");
    Con_DPrintf("  %f mSampleRate\n", outputStreamBasicDescription.mSampleRate);
    Con_DPrintf("  %c%c%c%c mFormatID\n",
                (outputStreamBasicDescription.mFormatID & 0xff000000) >> 24,
                (outputStreamBasicDescription.mFormatID & 0x00ff0000) >> 16,
                (outputStreamBasicDescription.mFormatID & 0x0000ff00) >>  8,
                (outputStreamBasicDescription.mFormatID & 0x000000ff) >>  0);
    Con_DPrintf("  %5d mBytesPerPacket\n", outputStreamBasicDescription.mBytesPerPacket);
    Con_DPrintf("  %5d mFramesPerPacket\n", outputStreamBasicDescription.mFramesPerPacket);
    Con_DPrintf("  %5d mBytesPerFrame\n", outputStreamBasicDescription.mBytesPerFrame);
    Con_DPrintf("  %5d mChannelsPerFrame\n", outputStreamBasicDescription.mChannelsPerFrame);
    Con_DPrintf("  %5d mBitsPerChannel\n", outputStreamBasicDescription.mBitsPerChannel);
    
    if (outputStreamBasicDescription.mFormatID != kAudioFormatLinearPCM) {
        Con_DPrintf("Default Audio Device doesn't support Linear PCM!\n");
        return false;
    }
    
    // Add sound IOProcID
    status = AudioDeviceCreateIOProcID(audioDevice, audioDeviceIOProc, NULL, &ioprocid);
    if (status) {
        Con_DPrintf("AudioDeviceAddIOProc: returned %d\n", status);
        return false;
    }
    if (ioprocid == NULL) {
        Con_DPrintf("Cannot create IOProcID\n");
        return false;
    }
*/    
    
    status = AUGraphIsRunning(audioGraph, &audioGraphIsRunning);
    if (status) {
        Con_DPrintf("AUGraphIsRunning returned %d\n", status);
        return false;
    }
    
//    if (audioGraphIsRunning) {
//        status = AUGraphStop(audioGraph);
//        if (status) {
//            Con_DPrintf("AUGraphStop returned %d\n", status);
//            return false;
//        }
//    }
    
    AudioComponentDescription converterDescription;
    converterDescription.componentType = kAudioUnitType_FormatConverter;
    converterDescription.componentSubType = kAudioUnitSubType_AUConverter;
    converterDescription.componentManufacturer = kAudioUnitManufacturer_Apple;
    converterDescription.componentFlags = 0;
    converterDescription.componentFlagsMask = 0;

    status = AUGraphAddNode(audioGraph, &converterDescription, &converterNode);
    if (status) {
        Con_DPrintf("AUGraphAddNode returned %d\n", status);
        return false;
    }
    
    status = AUGraphNodeInfo(audioGraph, converterNode, 0, &converterUnit);
    if (status) {
        Con_DPrintf("AUGraphNodeInfo returned %d\n", status);
        return false;
    }
    
    
    AURenderCallbackStruct callbackStruct;
    callbackStruct.inputProc = &renderCallback;
    callbackStruct.inputProcRefCon = NULL;
    
    status = AudioUnitSetProperty(converterUnit, 
                                  kAudioUnitProperty_SetRenderCallback, 
                                  kAudioUnitScope_Input, 
                                  0, 
                                  &callbackStruct, 
                                  sizeof(callbackStruct));
    if (status) {
        Con_DPrintf("AudioUnitSetProperty returned %d\n", status);
        return false;
    }
    
    
    UInt32 sampleRate = 44100;
    UInt32 bitsPerChannel = 16;
    UInt32 channelsPerFrame = 2;
    UInt32 bytesPerFrame = channelsPerFrame * (bitsPerChannel >> 3);
    UInt32 framesPerPacket = 1;
    UInt32 bytesPerPacket = bytesPerFrame * framesPerPacket;
    
    AudioStreamBasicDescription streamBasicDescription;
    streamBasicDescription.mSampleRate = sampleRate;
    streamBasicDescription.mFormatID = kAudioFormatLinearPCM;
    streamBasicDescription.mFormatFlags = kLinearPCMFormatFlagIsPacked;
    streamBasicDescription.mBytesPerPacket = bytesPerPacket;
    streamBasicDescription.mFramesPerPacket = framesPerPacket;
    streamBasicDescription.mChannelsPerFrame = channelsPerFrame;
    streamBasicDescription.mBitsPerChannel = bitsPerChannel;
    if (bitsPerChannel > 8) {
        streamBasicDescription.mFormatFlags |= kLinearPCMFormatFlagIsSignedInteger;
    }
    
    status = AudioUnitSetProperty(converterUnit, 
                                  kAudioUnitProperty_StreamFormat, 
                                  kAudioUnitScope_Input, 
                                  0, 
                                  &streamBasicDescription, 
                                  sizeof(streamBasicDescription));
    if (status) {
        Con_DPrintf("AudioUnitSetProperty returned %d\n", status);
        return false;
    }
    
    status = AUGraphConnectNodeInput(audioGraph, converterNode, 0, mixerNode, unitElement);
    if (status) {
        Con_DPrintf("AUGraphConnectNodeInput returned %d\n", status);
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
    
//    shm->width = outputStreamBasicDescription.mBytesPerFrame / 8;
    shm->width = 1;//outputStreamBasicDescription.mBytesPerFrame / 8;
    shm->channels = 2;
    shm->samples = sizeof(buffer) / (shm->samplebits >> 3);
    shm->samplepos = 0;
    shm->submission_chunk = OUTPUT_BUFFER_SIZE;
    shm->buffer = buffer;
    
    // We haven't enqueued anything yet
    bufferPosition = 0;
/*    
    status = AudioDeviceStart(audioDevice, ioprocid);
    if (status) {
        Con_DPrintf("AudioDeviceStart: returned %d\n", status);
        return false;
    }
*/    
//    if (audioGraphIsRunning) {
        status = AUGraphStart(audioGraph);
        if (status) {
            Con_DPrintf("AUGraphStart returned %d\n", status);
            return false;
        }
//    }
    
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
    OSStatus status;
    
    if (snd_inited)
	{
        status = AUGraphIsRunning(audioGraph, &audioGraphIsRunning);
        if (status) {
            Con_DPrintf("AUGraphIsRunning returned %d\n", status);
        }
        
//        if (audioGraphIsRunning) {
            status = AUGraphStop(audioGraph);
            if (status) {
                Con_DPrintf("AUGraphStop returned %d\n", status);
            }
//        }
        
        status = AUGraphDisconnectNodeInput(audioGraph, mixerNode, unitElement);
        if (status) {
            Con_DPrintf("AUGraphDisconnectNodeInput returned %d\n", status);
        }
        
        status = AUGraphRemoveNode(audioGraph, converterNode);
        if (status) {
            Con_DPrintf("AUGraphRemoveNode returned %d\n", status);
        }
        
        status = DisposeAUGraph(audioGraph);
        if (status) {
            Con_DPrintf("DisposeAUGraph returned %d\n", status);
        }
        
/*        
        status = AudioDeviceStop(audioDevice, ioprocid);
        if (status) {
            Con_DPrintf("AudioDeviceStop: returned %d\n", status);
        }
        
        // Remove sound IOProcID
        status = AudioDeviceDestroyIOProcID(audioDevice, ioprocid);
        if (status) {
            Con_DPrintf("AudioDeviceRemoveIOProc: returned %d\n", status);
        }
*/        
        snd_inited = false;
    }
}

