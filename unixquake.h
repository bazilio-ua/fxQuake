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
// unixquake.h -- Unix System specific Quake header file

#include <unistd.h>
#include <fcntl.h>
#include <paths.h>
#include <dirent.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/socket.h>

#ifdef __linux__
#include <sys/vt.h>
#endif

#if defined __FreeBSD__ || defined __OpenBSD__ || defined __NetBSD__
#include <sys/cdio.h>
#include <sys/soundcard.h>
#elif defined __linux__
#include <linux/cdrom.h>
#include <linux/soundcard.h>
#endif

#ifdef __OpenBSD__
#include <util.h>
#endif

#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netdb.h>

