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
// cd_mac.m

#include "quakedef.h"
#include "unixquake.h"
#include "macquake.h"

static qboolean isPaused = false;
static qboolean	initialized = false;
static qboolean	enabled = true;
static qboolean playLooping = false;
//static byte 	remap[100];
static byte		playTrack;
static byte		maxTrack;

typedef struct _AIFFChunkHeader {
    unsigned int chunkID;
    unsigned int chunkSize;
    unsigned int fileType;
} AIFFChunkHeader;

typedef struct _AIFFGenericChunk {
    unsigned int chunkID;
    unsigned int chunkSize;
} AIFFGenericChunk;

typedef struct _AIFFSSNDData {
    unsigned int offset;
    unsigned int blockSize;
} AIFFSSNDData;

typedef struct _AIFFInfo {
    FILE *file;
} AIFFInfo; 

AIFFInfo *AIFFOpen(NSString *path);
void AIFFClose(AIFFInfo *aiff);
unsigned int AIFFRead(AIFFInfo *aiff, short *samples, unsigned int sampleCount);

/* ------------------------------------------------------------------------------------ */

NSMutableArray *cdTracks;

static float	old_cdvolume;

//#define SAMPLES_PER_BUFFER (8*1024)
#define SAMPLES_PER_BUFFER (2*1024)

static AIFFInfo         *aiffInfo;
static short            *samples;

//static AudioDeviceID outputDeviceID;
//static AudioStreamBasicDescription outputStreamBasicDescription;
static AudioDeviceIOProcID ioprocid = NULL;

static OSStatus audioDeviceIOProc(AudioDeviceID inDevice,
                                  const AudioTimeStamp *inNow,
                                  const AudioBufferList *inInputData,
                                  const AudioTimeStamp *inInputTime,
                                  AudioBufferList *outOutputData,
                                  const AudioTimeStamp *inOutputTime,
                                  void *inClientData);


/*
====================

AIFF-C read routines

====================
*/

AIFFInfo *AIFFOpen(NSString *path)
{
    const char *pathStr;
    AIFFInfo *aiff;
    FILE *file;
    AIFFChunkHeader chunkHeader;
    AIFFGenericChunk chunk;
    AIFFSSNDData ssndData;
    
    pathStr = [path fileSystemRepresentation];
    file = fopen(pathStr, "r");
    if (!file) {
        perror(pathStr);
        return NULL;
    }
    
    aiff = malloc(sizeof(*aiff));
    aiff->file = file;
    
    fread(&chunkHeader, 1, sizeof(chunkHeader), aiff->file);
    chunkHeader.chunkID = BigLong(chunkHeader.chunkID);
    if (chunkHeader.chunkID != 'FORM') {
        Con_DWarning("AIFFOpen: chunkID is not 'FORM'\n");
        AIFFClose(aiff);
        return NULL;
    }
    chunkHeader.fileType = BigLong(chunkHeader.fileType);
    if (chunkHeader.fileType != 'AIFC') {
        Con_DWarning("AIFFOpen: file format is not 'AIFC'\n");
        AIFFClose(aiff);
        return NULL;
    }
    
    // Skip up to the 'SSND' chunk, ignoring all the type, compression, format, chunks.
    while (1) {
        fread(&chunk, 1, sizeof(chunk), aiff->file);
        chunk.chunkID = BigLong(chunk.chunkID);
        chunk.chunkSize = BigLong(chunk.chunkSize);
        
        if (chunk.chunkID == 'SSND')
            break;
        
        Con_DPrintf("AIFFOpen: skipping chunk %c%c%c%c\n", 
                    (chunk.chunkID >> 24) & 0xff, 
                    (chunk.chunkID >> 16) & 0xff, 
                    (chunk.chunkID >> 8) & 0xff, 
                    (chunk.chunkID >> 0) & 0xff);
        
        // Skip the chunk data
        fseek(aiff->file, chunk.chunkSize, SEEK_CUR);
    }
    
    Con_DPrintf("AIFFOpen: Found SSND, size = %d\n", chunk.chunkSize);
    
    fread(&ssndData, 1, sizeof(ssndData), aiff->file);
    ssndData.offset =  BigLong(ssndData.offset);
    ssndData.blockSize = BigLong(ssndData.blockSize);
    
    Con_DPrintf("AIFFOpen: offset = %d\n", ssndData.offset);
    Con_DPrintf("AIFFOpen: blockSize = %d\n", ssndData.blockSize);
    
    return aiff;
}

void AIFFClose(AIFFInfo *aiff)
{
//    if (aiff) {
//        fclose(aiff->file);
//        free(aiff);
//        aiff = NULL;
//    }
}

unsigned int AIFFRead(AIFFInfo *aiff, short *samples, unsigned int sampleCount)
{
    unsigned int index;
    
    sampleCount = fread(samples, sizeof(*samples), sampleCount, aiff->file);
    
    for (index = 0; index < sampleCount; index++) {
        short sample;
        
        sample = samples[index];
        
        // CD data is stored in little-endian order, but we want big endian, being on PPC.
//        sample = ((sample & 0xff00) >> 8) | ((sample & 0x00ff) << 8);
        
        samples[index] = sample;
    }
    
    return sampleCount;
}

/* ------------------------------------------------------------------------------------ */

/*
====================
CoreAudio IO Proc
====================
*/

OSStatus audioDeviceIOProc(AudioDeviceID inDevice,
                           const AudioTimeStamp *inNow,
                           const AudioBufferList *inInputData,
                           const AudioTimeStamp *inInputTime,
                           AudioBufferList *outOutputData,
                           const AudioTimeStamp *inOutputTime,
                           void *inClientData)
{
    unsigned int sampleIndex, sampleCount;
    float *outBuffer;
    float scale = (old_cdvolume / 32768.0f);
    
    // The buffer that we need to fill
    outBuffer = (float *)outOutputData->mBuffers[0].mData;
    
    // Read some samples from the file.
    sampleCount = AIFFRead(aiffInfo, samples, SAMPLES_PER_BUFFER);
    
    // Convert whatever samples we got into floats. Scale the floats to be [-1..1].
    for (sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++) {
        // Convert the samples from shorts to floats.  Scale the floats to be [-1..1].
        outBuffer[sampleIndex] = samples[sampleIndex] * scale;
    }
    
    // Fill in zeros in the rest of the buffer
    for (; sampleIndex < SAMPLES_PER_BUFFER; sampleIndex++)
        outBuffer[sampleIndex] = 0.0;
    
    return 0;
}


//static void CDAudio_Eject(void)
//{
//	
//}
//
//static void CDAudio_CloseDoor(void)
//{
//	
//}

/* ------------------------------------------------------------------------------------ */

qboolean CDAudio_GetAudioDiskInfo(void)
{
    NSDirectoryEnumerator *dirEnum;
    NSFileManager *fileManager;
    unsigned int mountCount;
    struct statfs  *mounts;
    NSString *mountPath;
    NSString *filePath;
    
    // Get rid of old info
    [cdTracks release];
    cdTracks = [[NSMutableArray alloc] init];
    
    // Get the list of file system mount points
    mountCount = getmntinfo(&mounts, MNT_NOWAIT);
    if (mountCount <= 0) {
        Con_DWarning("GetAudioDiskInfo: getmntinfo failed");
        return false;
    }
    
    fileManager = [NSFileManager defaultManager];
    while (mountCount--) {
        const char *lastComponent;
        
        if ((mounts[mountCount].f_flags & MNT_RDONLY) != MNT_RDONLY) {
            // CDs are read-only.
            continue;
        }
        
        if ((mounts[mountCount].f_flags & MNT_LOCAL) != MNT_LOCAL) {
            // CDs are not network filesystems
            continue;
        }
        
        if (strcmp(mounts[mountCount].f_fstypename, "cddafs")) {
            // Check the file system type just to be extra sure
            continue;
        }
        
        lastComponent = strrchr(mounts[mountCount].f_mntonname, '/');
        if (!lastComponent) {
            // No slash in the mount point!  How is that possible?
            continue;
        }
        
        // This looks good
        Con_DPrintf("FOUND CD:\n");
        Con_DPrintf("   type: %d\n", mounts[mountCount].f_type);
        Con_DPrintf("   flags: %u\n", mounts[mountCount].f_flags);
        Con_DPrintf("   fstype: %s\n", mounts[mountCount].f_fstypename);
        Con_DPrintf("   f_mntonname: %s\n", mounts[mountCount].f_mntonname);
        Con_DPrintf("   f_mntfromname: %s\n", mounts[mountCount].f_mntfromname);
        
        mountPath = [NSString stringWithCString: mounts[mountCount].f_mntonname encoding:NSUTF8StringEncoding];
        dirEnum = [fileManager enumeratorAtPath: mountPath];
        while ((filePath = [dirEnum nextObject])) {
            if ([[filePath pathExtension] isEqualToString: @"aiff"])
                [cdTracks addObject: [mountPath stringByAppendingPathComponent: filePath]];
        }
    }
    
    if (![cdTracks count]) {
        [cdTracks release];
        cdTracks = NULL;
        Con_DPrintf("CDAudio: no music tracks\n");
        return false;
    }
    
    maxTrack = [cdTracks count];
    
	return true;
}

void CDAudio_Play(byte track, qboolean looping)
{
    OSStatus status;
    
	if (!enabled)
		return;
    
    if (track < 1 || track > maxTrack)
    {
        Con_DPrintf("CDAudio: Bad track number %u.\n", track);
        return;
    }
	
    playTrack = track;
    aiffInfo = AIFFOpen([cdTracks objectAtIndex:track - 1]);
    
    // Start playing audio
    status = AudioDeviceStart(outputDeviceID, ioprocid);
    if (status) {
        Con_Printf("AudioDeviceStart: returned %d\n", status);
    } else {
        if (!old_cdvolume)
            CDAudio_Pause();
        
        isPaused = false;
        playLooping = looping;
    }
}

void CDAudio_Stop(void)
{
	if (!enabled)
		return;
    
    // Stop playing audio
    AudioDeviceStop(outputDeviceID, ioprocid);
    
    AIFFClose(aiffInfo);
}

void CDAudio_Pause(void)
{
	if (!enabled)
		return;
    
    AudioDeviceStop(outputDeviceID, ioprocid);
    isPaused = true;
}

void CDAudio_Resume(void)
{
	if (!enabled)
		return;
    
    if (!isPaused)
        return;
    
    AudioDeviceStart (outputDeviceID, ioprocid);
    isPaused = false;
}

static void CD_f (void)
{
	
}


void CDAudio_Update(void)
{
	if (!enabled)
		return;
    
    if (old_cdvolume != bgmvolume.value) {
        if (old_cdvolume) {
            old_cdvolume = bgmvolume.value;
            if (!old_cdvolume)
                CDAudio_Pause();
        } else {
            old_cdvolume = bgmvolume.value;
            if (!old_cdvolume)
                CDAudio_Resume();
        }
    }
}

int CDAudio_Init(void)
{
    OSStatus status;
    
	if (cls.state == ca_dedicated)
		return -1;
    
	if (COM_CheckParm("-nocdaudio"))
		return -1;
    
    Cmd_AddCommand ("cd", CD_f);
    
	old_cdvolume = bgmvolume.value;
    
//    aiffInfo = aiff;
    samples = (short *)malloc(SAMPLES_PER_BUFFER * sizeof(*samples));
    
    // Add the sound to IOProc
    status = AudioDeviceCreateIOProcID(outputDeviceID, audioDeviceIOProc, NULL, &ioprocid);
    if (status) {
        Con_Printf("AudioDeviceAddIOProc: returned %d\n", status);
        return -1;
    }
    
    if (ioprocid == NULL) {
        Con_Printf("Cannot create IOProcID\n");
        return -1;
    }
    
    initialized = true;
    
    Con_Printf("CD Audio initialized (using CoreAudio)\n");
    
    if (CDAudio_GetAudioDiskInfo()) {
        isPaused = false;
    } else {
        return -1;
    }
    
    enabled = true;
    
	return 0;
}

void CDAudio_Shutdown(void)
{
    if (!initialized)
		return;
    
    CDAudio_Stop();
    
    // Remove sound IOProcID
    AudioDeviceDestroyIOProcID(outputDeviceID, ioprocid);
    
    if (cdTracks) {
        [cdTracks release];
        cdTracks = NULL;
    }
    
    initialized = false;
}

