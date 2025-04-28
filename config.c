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
// config.c -- misc reads from the config file

#include "quakedef.h"


static FILE			*cfg_file;

/*
===================
CFG_ReadCvars

used for doing early reads from config.cfg searching the list
of given cvar names for the user-set values. a temporary
solution until we merge a better cvar system.
the num_vars argument must be the exact number of strings in the
array, otherwise I have nothing against going out of bounds.
===================
*/
void CFG_ReadCvars (char **vars, int num_vars)
{
	char	buff[1024], *tmp;
	int			i, j;

	if (!cfg_file || num_vars < 1)
		return;

	j = 0;

	do {
		i = 0;
		memset (buff, 0, sizeof(buff));
		// we expect a line in the format that Cvar_WriteVariables
		// writes to the config file. although I'm trying to be as
		// much cautious as possible, if the user screws it up by
		// editing it, it's his fault.
		fgets(buff, sizeof(buff), cfg_file);
		if (!feof(cfg_file))
		{
			// remove end-of-line characters
			while (buff[i])
			{
				if (buff[i] == '\r' || buff[i] == '\n')
					buff[i] = '\0';
				// while we're here, replace tabs with spaces
				if (buff[i] == '\t')
					buff[i] = ' ';
				i++;
			}
			// go to the last character
			while (buff[i] == 0 && i > 0)
				i--;
			// remove trailing spaces
			while (i > 0)
			{
				if (buff[i] == ' ')
				{
					buff[i] = '\0';
					i--;
				}
				else
					break;
			}

			// the line must end with a quotation mark
			if (buff[i] != '\"')
				continue;
			buff[i] = '\0';

			for (i = 0; i < num_vars && vars[i]; i++)
			{
				// look for the cvar name + one space
				tmp = strstr(buff, va("%s ",vars[i]));
				if (tmp != buff)
					continue;
				// locate the first quotation mark
				tmp = strchr(buff, '\"');
				if (tmp)
				{
					Cvar_Set (vars[i], tmp+1);
					j++;
					break;
				}
			}
		}

		if (j == num_vars)
			break;

	} while (!feof(cfg_file));

	fseek (cfg_file, 0, SEEK_SET);
}

void CFG_ReadCvarOverrides (char **vars, int num_vars)
{
	char	buff[64];
	int		i, j;

	if (num_vars < 1)
		return;

	buff[0] = '+';

	for (i = 0; i < num_vars; i++)
	{
		strncpy (&buff[1], vars[i], sizeof(buff) - 1);
		buff[sizeof(buff) - 1] = 0;
		j = COM_CheckParm(buff);
		if (j != 0 && j < com_argc - 1)
		{
			if (com_argv[j + 1][0] != '-' && com_argv[j + 1][0] != '+')
				Cvar_Set(vars[i], com_argv[j + 1]);
		}
	}
}

void CFG_CloseConfig (void)
{
	if (cfg_file)
	{
		fclose (cfg_file);
		cfg_file = NULL;
	}
}

int CFG_OpenConfig (char *cfg_name)
{
	CFG_CloseConfig ();
	COM_FOpenFile (cfg_name, &cfg_file, NULL);
	if (!cfg_file)
		return -1;
	return 0;
}

