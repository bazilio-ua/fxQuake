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
// common.h  -- general definitions

#ifndef TYPES_DEFINED
typedef unsigned char 		byte;
typedef unsigned short		word;
typedef unsigned long		dword;
#define TYPES_DEFINED 1
#endif

#undef true
#undef false

typedef enum {false, true}	qboolean;

#define stringify__(x) #x
#define stringify(x) stringify__(x)

//============================================================================

typedef struct sizebuf_s
{
	qboolean	allowoverflow;	// if false, do a Sys_Error
	qboolean	overflowed;		// set to true if the buffer size failed
	byte	*data;
	int		maxsize;
	int		cursize;
} sizebuf_t;

void SZ_Alloc (sizebuf_t *buf, int startsize);
void SZ_Free (sizebuf_t *buf);
void SZ_Clear (sizebuf_t *buf);
void *SZ_GetSpace (sizebuf_t *buf, int length);
void SZ_Write (sizebuf_t *buf, void *data, int length);
void SZ_Print (sizebuf_t *buf, char *data);	// strcats onto the sizebuf

//============================================================================

typedef struct link_s
{
	struct link_s	*prev, *next;
} link_t;

qboolean IsTimeout (float *prevtime, float waittime);

void ClearLink (link_t *l);
void RemoveLink (link_t *l);
void InsertLinkBefore (link_t *l, link_t *before);
void InsertLinkAfter (link_t *l, link_t *after);

// (type *)STRUCT_FROM_LINK(link_t *link, type, member)
// ent = STRUCT_FROM_LINK(link,entity_t,order)
// FIXME: remove this mess!
#define	STRUCT_FROM_LINK(l,t,m) ((t *)((byte *)l - (intptr_t)&(((t *)0)->m)))

//============================================================================

#ifndef NULL
#define NULL ((void *)0)
#endif

//============================================================================

static inline qboolean GetBit (const unsigned int *a, unsigned int i)
{
	return (a[i/32u] & (1u<<(i%32u))) != 0u;
}

static inline void SetBit (unsigned int *a, unsigned int i)
{
	a[i/32u] |= 1u<<(i%32u);
}

static inline void ClearBit (unsigned int *a, unsigned int i)
{
	a[i/32u] &= ~(1u<<(i%32u));
}

static inline void ToggleBit (unsigned int *a, unsigned int i)
{
	a[i/32u] ^= 1u<<(i%32u);
}

//============================================================================

size_t Q_strnlen (const char *s, size_t maxlen);
#ifndef strnlen
#define strnlen Q_strnlen
#endif

//============================================================================

extern	qboolean		bigendian;

extern	short	(*BigShort) (short l);
extern	short	(*LittleShort) (short l);
extern	int	(*BigLong) (int l);
extern	int	(*LittleLong) (int l);
extern	float	(*BigFloat) (float l);
extern	float	(*LittleFloat) (float l);

//============================================================================

void MSG_WriteChar (sizebuf_t *sb, int c);
void MSG_WriteByte (sizebuf_t *sb, int c);
void MSG_WriteShort (sizebuf_t *sb, int c);
void MSG_WriteLong (sizebuf_t *sb, int c);
void MSG_WriteFloat (sizebuf_t *sb, float f);
void MSG_WriteString (sizebuf_t *sb, char *s);
void MSG_WriteCoord16 (sizebuf_t *sb, float f); //johnfitz -- original behavior, 13.3 fixed point coords, max range +-4096
void MSG_WriteCoord24 (sizebuf_t *sb, float f); //johnfitz -- 16.8 fixed point coords, max range +-32768
void MSG_WriteCoord32f (sizebuf_t *sb, float f); //johnfitz -- 32-bit float coords
void MSG_WriteCoord (sizebuf_t *sb, float f, unsigned int flags);
void MSG_WriteAngle (sizebuf_t *sb, float f, unsigned int flags);
void MSG_WritePreciseAngle (sizebuf_t *sb, float f); // precise aim for ProQuake
void MSG_WriteAngle16 (sizebuf_t *sb, float f, unsigned int flags); //johnfitz -- PROTOCOL_FITZQUAKE

typedef struct msg_s {
	int readcount;
	qboolean badread;		// set if a read goes beyond end of message
	sizebuf_t *message;
	size_t badread_string_size;
	char *badread_string;
} qmsg_t;

void MSG_BeginReading (qmsg_t *msg);
int MSG_ReadChar (qmsg_t *msg);
int MSG_ReadByte (qmsg_t *msg);
int MSG_ReadShort (qmsg_t *msg);
int MSG_ReadLong (qmsg_t *msg);
float MSG_ReadFloat (qmsg_t *msg);
char *MSG_ReadString (qmsg_t *msg);
float MSG_ReadCoord16 (qmsg_t *msg); //johnfitz -- original behavior, 13.3 fixed point coords, max range +-4096
float MSG_ReadCoord24 (qmsg_t *msg); //johnfitz -- 16.8 fixed point coords, max range +-32768
float MSG_ReadCoord32f (qmsg_t *msg); //johnfitz -- 32-bit float coords
float MSG_ReadCoord (qmsg_t *msg, unsigned int flags);
float MSG_ReadAngle (qmsg_t *msg, unsigned int flags);
float MSG_ReadPreciseAngle (qmsg_t *msg); // precise aim for ProQuake
float MSG_ReadAngle16 (qmsg_t *msg, unsigned int flags); //johnfitz -- PROTOCOL_FITZQUAKE

//============================================================================

extern	char		com_token[1024];
extern	qboolean	com_eof;

char *COM_Parse (char *data);


extern	int		com_argc;
extern	char	**com_argv;

int COM_CheckParm (char *parm);
void COM_Init (void);
void COM_InitArgv (int argc, char **argv);

char *COM_SkipPath (char *path);
void COM_StripExtension (char *in, char *out);
char *COM_FileExtension (char *in);
void COM_FileBase (char *in, char *out);
void COM_DefaultExtension (char *path, char *ext);

char	*va(char *format, ...);
// does a varargs printf into a temp buffer

//============================================================================

//
// file IO
//

// returns the file size
// return -1 if file is not present
// the file should be in BINARY mode for stupid OSs that care
int Sys_FileOpenRead (char *path, int *handle);

int Sys_FileOpenWrite (char *path);
void Sys_FileClose (int handle);
void Sys_FileSeek (int handle, int position);
int Sys_FileRead (int handle, void *dst, int count);
int Sys_FileWrite (int handle, void *src, int count);
int Sys_FileTime (char *path);

//============================================================================
//	QUAKE FILESYSTEM
//============================================================================

extern int com_filesize;

//
// in memory
//

typedef struct
{
	char    name[MAX_QPATH];
	int             filepos, filelen;
} packfile_t;

typedef struct pack_s
{
	char    filename[MAX_OSPATH];
	int             handle;
	int             numfiles;
	packfile_t      *files;
} pack_t;

//
// on disk
//

typedef struct
{
	char    name[56];
	int             filepos, filelen;
} dpackfile_t;

typedef struct
{
	char    id[4];
	int             dirofs;
	int             dirlen;
} dpackheader_t;

#define MAX_FILES_IN_PACK       2048

extern	char	com_cachedir[MAX_OSPATH];
extern	char	com_gamedir[MAX_OSPATH];
extern	char	com_basedir[MAX_OSPATH];

typedef struct searchpath_s
{
	unsigned int path_id;	// identifier assigned to the game directory
							// Note that <install_dir>/game1 and
							// <userdir>/game1 have the same id.
	char    filename[MAX_OSPATH];
	pack_t  *pack;          // only one of filename / pack will be used
	struct searchpath_s *next;
} searchpath_t;

extern	searchpath_t    *com_searchpaths;
extern	searchpath_t	*com_base_searchpaths;

void COM_WriteFile (char *filename, void *data, int len);
int COM_OpenFile (char *filename, int *handle, unsigned int *path_id);
int COM_FOpenFile (char *filename, FILE **file, unsigned int *path_id);
void COM_CloseFile (int h);

struct cache_user_s;

byte *COM_LoadMallocFile (char *path, void *buffer, unsigned int *path_id);
byte *COM_LoadStackFile (char *path, void *buffer, int bufsize, unsigned int *path_id);
byte *COM_LoadTempFile (char *path, unsigned int *path_id);
byte *COM_LoadZoneFile (char *path, unsigned int *path_id);
byte *COM_LoadHunkFile (char *path, unsigned int *path_id);
void COM_LoadCacheFile (char *path, struct cache_user_s *cu, unsigned int *path_id);

void COM_CreatePath (char *path);

extern	struct cvar_s	registered;

extern qboolean		standard_quake, rogue, hipnotic, nehahra, quoth;

//==============================================================================
//	FILELIST
//==============================================================================

typedef struct filelist_s
{
	char			name[MAX_QPATH];
	struct filelist_s	*next;
} filelist_t;

void COM_FileListAdd (const char *name, filelist_t **list);
void COM_FileListClear (filelist_t **list);
void COM_ScanDirList (char *path, filelist_t **list);
void COM_ScanDirFileList(char *path, char *subdir, char *ext, qboolean stripext, filelist_t **list);
void COM_ScanPakFileList(pack_t *pack, char *subdir, char *ext, qboolean stripext, filelist_t **list);

