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

AudioUnitElement unitElementSND = 0;

AUGraph audioGraph;
AUNode outputNode;
AUNode mixerNode;
AUNode converterNode;
AudioUnit mixerUnit;
AudioUnit converterUnit;
AudioUnit outputUnit;

// 64K is > 1 second at 16-bit, 22050 Hz
#define	WAV_BUFFERS				64
#define	WAV_BUFFER_SIZE			1024 // chunk size

#define	BUFFER_SIZE				(WAV_BUFFERS * WAV_BUFFER_SIZE)
static byte buffer[BUFFER_SIZE];
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
    
    UInt32 availableBufferBytes = sizeof(buffer) - bufferPosition;
    UInt32 overflowedBufferBytes = 0;
    if (availableBufferBytes < outBufferByteSize) {
        overflowedBufferBytes = outBufferByteSize - availableBufferBytes;
        outBufferByteSize = availableBufferBytes;
    }
    
    for (UInt32 index = 0; index < outBufferByteSize; index++) {
        outBuffer[index] = buffer[bufferPosition + index];
    }
    
    // Increase the buffer position. This is the next buffer we will submit
    bufferPosition += outBufferByteSize;
    if (bufferPosition >= sizeof(buffer))
        bufferPosition = 0;
    
    if (overflowedBufferBytes) {
        for (UInt32 index = 0; index < overflowedBufferBytes; index++) {
            outBuffer[outBufferByteSize + index] = buffer[bufferPosition + index];
        }
        
        // Increase the buffer position. There is no need to check position out of buffer bounds
        bufferPosition += overflowedBufferBytes;
    }
    
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
	UInt32 propertySize;
    OSStatus status;
    int i;
    
    snd_inited = false;
	shm = &sn;
    
	//
	// get default device id
	//
	AudioDeviceID defaultDevice = kAudioObjectUnknown;
	propertySize = sizeof(AudioDeviceID);
	
	AudioObjectPropertyAddress defaultDeviceProperty;
	defaultDeviceProperty.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
	defaultDeviceProperty.mScope = kAudioObjectPropertyScopeGlobal;
	defaultDeviceProperty.mElement = kAudioObjectPropertyElementMaster;
	
	status = AudioObjectGetPropertyData(kAudioObjectSystemObject,
										&defaultDeviceProperty,
										0,
										NULL,
										&propertySize,
										&defaultDevice);
	if (status) {
		Con_DPrintf("AudioObjectGetPropertyData returned %d\n", status);
		return false;
	}
	
	//
	// get default device name
	//
	CFStringRef cfName = NULL;
	propertySize = sizeof(CFStringRef);
	
	AudioObjectPropertyAddress defaultDeviceNameProperty;
	defaultDeviceNameProperty.mSelector = kAudioObjectPropertyName;
	defaultDeviceNameProperty.mScope = kAudioObjectPropertyScopeGlobal;
	defaultDeviceNameProperty.mElement = kAudioObjectPropertyElementMaster;
	
	status = AudioObjectGetPropertyData(defaultDevice,
										&defaultDeviceNameProperty,
										0,
										NULL,
										&propertySize,
										&cfName);
	if (status) {
		Con_DPrintf("AudioObjectGetPropertyData returned %d\n", status);
		return false;
	}
	CFRelease(cfName);
	
	//
	// get default device sample rate
	//
	Float64 sampleRate1 = 0;
	propertySize = sizeof(Float64);
	
	AudioObjectPropertyAddress defaultDeviceNominalSampleRateProperty;
	defaultDeviceNominalSampleRateProperty.mSelector = kAudioDevicePropertyNominalSampleRate;
	defaultDeviceNominalSampleRateProperty.mScope = kAudioObjectPropertyScopeGlobal;
	defaultDeviceNominalSampleRateProperty.mElement = kAudioObjectPropertyElementMaster;
	
	status = AudioObjectGetPropertyData(defaultDevice,
										&defaultDeviceNominalSampleRateProperty,
										0,
										NULL,
										&propertySize,
										&sampleRate1);
	if (status) {
		Con_DPrintf("AudioObjectGetPropertyData returned %d\n", status);
		return false;
	}
	
	//
	// get default device buffer size
	//
//	UInt32 size;
//	
//	AudioObjectPropertyAddress defaultDeviceStreamConfigurationProperty;
//	defaultDeviceStreamConfigurationProperty.mSelector = kAudioDevicePropertyStreamConfiguration;
//	defaultDeviceStreamConfigurationProperty.mScope = kAudioDevicePropertyScopeOutput;
//	defaultDeviceStreamConfigurationProperty.mElement = kAudioObjectPropertyElementMaster;
//	
//	status = AudioObjectGetPropertyDataSize(defaultDevice,
//											&defaultDeviceStreamConfigurationProperty,
//											0,
//											NULL,
//											&size);
//	if (status) {
//		Con_DPrintf("AudioObjectGetPropertyDataSize returned %d\n", status);
//		return false;
//	}
	
	//
	// get default device buffer list
	//
//	AudioBufferList *bufferList = NULL;
//	bufferList = (AudioBufferList *)malloc(size);
//	if (!bufferList)
//		return false;
//
//	status = AudioObjectGetPropertyData(defaultDevice,
//										&defaultDeviceStreamConfigurationProperty,
//										0,
//										NULL,
//										&size,
//										bufferList);
//	if (status) {
//		Con_DPrintf("AudioObjectGetPropertyData returned %d\n", status);
//		return false;
//	}
//
//	UInt32 j;
//	UInt32 channelsPerFrame1 = 0;
//	for (j = 0; j < bufferList->mNumberBuffers; j++) {
//		channelsPerFrame1 += bufferList->mBuffers[j].mNumberChannels;
//	}
//	free(bufferList);
	
	//
	// Check if device is hogged by another process
	//
	pid_t hoggingProcess;
	propertySize = sizeof(pid_t);

	AudioObjectPropertyAddress defaultDeviceHogModeProperty;
	defaultDeviceHogModeProperty.mSelector = kAudioDevicePropertyHogMode;
	defaultDeviceHogModeProperty.mScope = kAudioObjectPropertyScopeGlobal;
	defaultDeviceHogModeProperty.mElement = kAudioObjectPropertyElementMaster;

	status = AudioObjectGetPropertyData(defaultDevice,
										&defaultDeviceHogModeProperty,
										0,
										NULL,
										&propertySize,
										&hoggingProcess);
	if (status) {
		Con_DPrintf("AudioObjectGetPropertyData returned %d\n", status);
		return false;
	}

	if ((hoggingProcess != -1) && (hoggingProcess != getpid())) {
		Con_Printf ("The sound device is already hogged by another application\nUnable to get exclusive access\n");
		return false;
	}
	
	//
	// Take exclusive access to the device (hog mode)
	//
	hoggingProcess = getpid();
	status = AudioObjectSetPropertyData(defaultDevice, &defaultDeviceHogModeProperty, 0, NULL, sizeof(pid_t), &hoggingProcess);
	if (status) {
		Con_DPrintf("AudioObjectSetPropertyData returned %d\n", status);
		return false;
	}
	
	
	// AUGraph init
    status = NewAUGraph(&audioGraph);
    if (status) {
        Con_DPrintf("NewAUGraph returned %d\n", status);
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
    
	status = AUGraphNodeInfo(audioGraph, outputNode, NULL, &outputUnit);
	if (status) {
		Con_DPrintf("AUGraphNodeInfo returned %d\n", status);
		return false;
	}
	
	//
	// Set the Current Device to the Default Output Unit
	//
	status = AudioUnitSetProperty(outputUnit,
								  kAudioOutputUnitProperty_CurrentDevice,
								  kAudioUnitScope_Global,
								  0,
								  &defaultDevice,
								  sizeof(defaultDevice));
	if (status) {
		Con_DPrintf("AudioUnitSetProperty returned %d\n", status);
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
    
    status = AUGraphNodeInfo(audioGraph, mixerNode, 0, &mixerUnit);
    if (status) {
        Con_DPrintf("AUGraphNodeInfo returned %d\n", status);
        return false;
    }
    
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
    
	
	//
	// use the stream format coming out of the AUHAL (should be de-interleaved)
	//
//	AudioStreamBasicDescription streamFormat;
//	propertySize = sizeof(AudioStreamBasicDescription);
//	status = AudioUnitGetProperty(converterUnit,
//								  kAudioUnitProperty_StreamFormat,
//								  kAudioUnitScope_Output,
//								  0,
//								  &streamFormat,
//								  &propertySize);
//	if (status) {
//		Con_DPrintf("AudioUnitGetProperty returned %d\n", status);
//		return false;
//	}
	
	//
	// check the input device's stream format
	//
//	AudioStreamBasicDescription deviceFormat;
//	status = AudioUnitGetProperty(converterUnit,
//								  kAudioUnitProperty_StreamFormat,
//								  kAudioUnitScope_Input,
//								  0,
//								  &deviceFormat,
//								  &propertySize);
//	if (status) {
//		Con_DPrintf("AudioUnitGetProperty returned %d\n", status);
//		return false;
//	}
//	
//	printf ("device rate %f, graph rate %f\n",
//			deviceFormat.mSampleRate,
//			streamFormat.mSampleRate);
	
	
/*
	// Tell the main app what we expect from it
	// sound speed
	if ((i = COM_CheckParm("-sndspeed")) != 0 && i < com_argc - 1)
		shm->speed = atoi(com_argv[i + 1]);
	else
		shm->speed = (int)snd_speed.value;
//		shm->speed = 44100;
    
    shm->samplebits = 16;
    shm->channels = 2;
    
*/
	
	shm->speed = 44100;
	shm->samplebits = 16;
	shm->channels = 2;
	
    UInt32 sampleRate = shm->speed;
    UInt32 bitsPerChannel = shm->samplebits;
    UInt32 channelsPerFrame = shm->channels;
    UInt32 bytesPerFrame = channelsPerFrame * (bitsPerChannel >> 3);
    UInt32 framesPerPacket = 1;
    UInt32 bytesPerPacket = bytesPerFrame * framesPerPacket;
    
    AudioStreamBasicDescription streamBasicDescription;
    streamBasicDescription.mSampleRate = sampleRate;
    streamBasicDescription.mFormatID = kAudioFormatLinearPCM;
    streamBasicDescription.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
    streamBasicDescription.mBytesPerPacket = bytesPerPacket;
    streamBasicDescription.mFramesPerPacket = framesPerPacket;
    streamBasicDescription.mBytesPerFrame = bytesPerFrame;    
    streamBasicDescription.mChannelsPerFrame = channelsPerFrame;
    streamBasicDescription.mBitsPerChannel = bitsPerChannel;
    
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
    
    status = AUGraphConnectNodeInput(audioGraph, converterNode, 0, mixerNode, unitElementSND);
    if (status) {
        Con_DPrintf("AUGraphConnectNodeInput returned %d\n", status);
        return false;
    }
    
    shm->samples = sizeof(buffer) / (shm->samplebits >> 3);
    shm->samplepos = 0;
    shm->submission_chunk = 1;
    shm->buffer = buffer;
    
    // We haven't enqueued anything yet
    bufferPosition = 0;
    
    status = AUGraphStart(audioGraph);
    if (status) {
        Con_DPrintf("AUGraphStart returned %d\n", status);
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
    OSStatus status;
    
    if (snd_inited)
	{
        status = AUGraphStop(audioGraph);
        if (status) {
            Con_DPrintf("AUGraphStop returned %d\n", status);
        }
        
        status = AUGraphDisconnectNodeInput(audioGraph, mixerNode, unitElementSND);
        if (status) {
            Con_DPrintf("AUGraphDisconnectNodeInput returned %d\n", status);
        }
        
        status = AUGraphRemoveNode(audioGraph, converterNode);
        if (status) {
            Con_DPrintf("AUGraphRemoveNode returned %d\n", status);
        }
        
        status = AUGraphUninitialize(audioGraph);
        if (status) {
            Con_DPrintf("AUGraphUninitialize returned %d\n", status);
        }
        
        status = DisposeAUGraph(audioGraph);
        if (status) {
            Con_DPrintf("DisposeAUGraph returned %d\n", status);
        }
        
        snd_inited = false;
    }
}

