/*
Copyright (C) 2008-2012  O.Sezer <sezero@users.sourceforge.net>

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
// config.h -- config definitions

int CFG_OpenConfig (char *cfg_name);
// opens the given config file and keeps it open
// until CFG_CloseConfig is called

void CFG_CloseConfig (void);
// closes the currently open config file

void CFG_ReadCvars (char **vars, int num_vars);
// reads the values of cvars in the given list
// from the currently open config file

void CFG_ReadCvarOverrides (char **vars, int num_vars);
// reads the "+" command line override values of cvars
// in the given list.  doesn't care about the config file.
// call this after CFG_ReadCvars() and before locking your
// cvars.

