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
// cvar.c -- dynamic variable tracking

#include "quakedef.h"

cvar_t	*cvar_vars;
char	*cvar_null_string = "";

//==============================================================================
//
//  USER COMMANDS
//
//==============================================================================

/*
============
Cvar_List_f
============
*/
void Cvar_List_f (void)
{
	cvar_t	*var;
	char 	*partial;
	int		len, count;

	if (Cmd_Argc() > 1)
	{
		partial = Cmd_Argv (1);
		len = strlen(partial);
	}
	else
	{
		partial = NULL;
		len = 0;
	}

	count=0;
	for (var=cvar_vars ; var ; var=var->next)
	{
		if (partial && strncmp (partial,var->name, len))
		{
			continue;
		}
		Con_SafePrintf ("%c%c%c %s \"%s\"\n",
			var->flags & CVAR_ARCHIVE ? '*' : ' ',
			var->flags & CVAR_SERVER ? 's' : ' ',
			var->flags & CVAR_ROM ? 'r' : ' ',
			var->name,
			var->string);
		count++;
	}

	Con_SafePrintf ("%i cvars", count);
	if (partial)
	{
		Con_SafePrintf (" beginning with \"%s\"", partial);
	}
	Con_SafePrintf ("\n");
}

/*
============
Cvar_Inc_f
============
*/
void Cvar_Inc_f (void)
{
	switch (Cmd_Argc())
	{
	default:
	case 1:
		Con_Printf("inc <cvar> [amount] : increment cvar\n");
		break;
	case 2:
		Cvar_SetValue (Cmd_Argv(1), Cvar_VariableValue(Cmd_Argv(1)) + 1);
		break;
	case 3:
		Cvar_SetValue (Cmd_Argv(1), Cvar_VariableValue(Cmd_Argv(1)) + atof(Cmd_Argv(2)));
		break;
	}
}

/*
============
Cvar_Dec_f
============
*/
void Cvar_Dec_f (void)
{
	switch (Cmd_Argc())
	{
	default:
	case 1:
		Con_Printf("dec <cvar> [amount] : decrement cvar\n");
		break;
	case 2:
		Cvar_SetValue (Cmd_Argv(1), Cvar_VariableValue(Cmd_Argv(1)) - 1);
		break;
	case 3:
		Cvar_SetValue (Cmd_Argv(1), Cvar_VariableValue(Cmd_Argv(1)) - atof(Cmd_Argv(2)));
		break;
	}
}

/*
============
Cvar_Toggle_f
============
*/
void Cvar_Toggle_f (void)
{
	switch (Cmd_Argc())
	{
	default:
	case 1:
		Con_Printf("toggle <cvar> : toggle cvar\n");
		break;
	case 2:
		if (Cvar_VariableValue(Cmd_Argv(1)))
			Cvar_Set (Cmd_Argv(1), "0");
		else
			Cvar_Set (Cmd_Argv(1), "1");
		break;
	}
}

/*
============
Cvar_Cycle_f
============
*/
void Cvar_Cycle_f (void)
{
	int i;

	if (Cmd_Argc() < 3)
	{
		Con_Printf("cycle <cvar> <value list>: cycle cvar through a list of values\n");
		return;
	}

	//loop through the args until you find one that matches the current cvar value.
	//yes, this will get stuck on a list that contains the same value twice.
	//it's not worth dealing with, and i'm not even sure it can be dealt with.

	for (i=2;i<Cmd_Argc();i++)
	{
		//zero is assumed to be a string, even though it could actually be zero.  The worst case
		//is that the first time you call this command, it won't match on zero when it should, but after that,
		//it will be comparing strings that all had the same source (the user) so it will work.
		if (atof(Cmd_Argv(i)) == 0)
		{
			if (!strcmp(Cmd_Argv(i), Cvar_VariableString(Cmd_Argv(1))))
				break;
		}
		else
		{
			if (atof(Cmd_Argv(i)) == Cvar_VariableValue(Cmd_Argv(1)))
				break;
		}
	}

	if (i == Cmd_Argc())
		Cvar_Set (Cmd_Argv(1), Cmd_Argv(2)); // no match
	else if (i + 1 == Cmd_Argc())
		Cvar_Set (Cmd_Argv(1), Cmd_Argv(2)); // matched last value in list
	else
		Cvar_Set (Cmd_Argv(1), Cmd_Argv(i+1)); // matched earlier in list
}

/*
============
Cvar_Reset_f -- johnfitz
============
*/
void Cvar_Reset_f (void)
{
	switch (Cmd_Argc())
	{
	default:
	case 1:
		Con_Printf ("reset <cvar> : reset cvar to default\n");
		break;
	case 2:
		Cvar_Reset (Cmd_Argv(1));
		break;
	}
}

/*
============
Cvar_ResetAll_f -- johnfitz
============
*/
void Cvar_ResetAll_f (void)
{
	cvar_t	*var;

	for (var = cvar_vars; var; var = var->next)
		Cvar_Reset (var->name);
}

/*
============
Cvar_ResetCfg_f -- QuakeSpasm
============
*/
void Cvar_ResetCfg_f (void)
{
	cvar_t	*var;

	for (var = cvar_vars ; var ; var = var->next)
		if (var->flags & CVAR_ARCHIVE)
			Cvar_Reset (var->name);
}

//==============================================================================
//
//  INIT
//
//==============================================================================

/*
============
Cvar_Init
============
*/

void Cvar_Init (void)
{
	Cmd_AddCommand ("cvarlist", Cvar_List_f);
	Cmd_AddCommand ("toggle", Cvar_Toggle_f);
	Cmd_AddCommand ("cycle", Cvar_Cycle_f);
	Cmd_AddCommand ("inc", Cvar_Inc_f);
	Cmd_AddCommand ("dec", Cvar_Dec_f);
	Cmd_AddCommand ("reset", Cvar_Reset_f);
	Cmd_AddCommand ("resetall", Cvar_ResetAll_f);
	Cmd_AddCommand ("resetcfg", Cvar_ResetCfg_f);
}

//==============================================================================
//
//  CVAR FUNCTIONS
//
//==============================================================================

/*
============
Cvar_FindVar
============
*/
cvar_t *Cvar_FindVar (char *var_name)
{
	cvar_t	*var;
	
	for (var=cvar_vars ; var ; var=var->next)
		if (!strcmp (var_name, var->name))
			return var;

	return NULL;
}

/*
============
Cvar_NextServerVar

moved from net_dgrm.c to here, command == CCREQ_RULE_INFO case
============
*/
cvar_t *Cvar_NextServerVar (char *var_name)
{
	cvar_t	*var;
	
	// find the search start location
	if (*var_name)
	{
		var = Cvar_FindVar (var_name);
		if (!var)
			return NULL;
		var = var->next;
	}
	else
		var = cvar_vars;
	
	// search for the next server cvar
	while (var)
	{
		if (var->flags & CVAR_SERVER)
			break;
		var = var->next;
	}
	
	return var;
}


/*
============
Cvar_VariableValue
============
*/
float	Cvar_VariableValue (char *var_name)
{
	cvar_t	*var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
		return 0;
	return atof (var->string);
}


/*
============
Cvar_VariableString
============
*/
char *Cvar_VariableString (char *var_name)
{
	cvar_t *var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
		return cvar_null_string;
	return var->string;
}


/*
============
Cvar_CompleteVariable
============
*/
char *Cvar_CompleteVariable (char *partial)
{
	cvar_t		*var;
	int			len;

	len = strlen(partial);

	if (!len)
		return NULL;

// check functions
	for (var=cvar_vars ; var ; var=var->next)
		if (!strncmp (partial,var->name, len))
			return var->name;

	return NULL;
}

/*
============
Cvar_Reset -- johnfitz
============
*/
void Cvar_Reset (char *var_name)
{
	cvar_t	*var;

	var = Cvar_FindVar (var_name);
	if (!var)
		Con_Printf ("Cvar_Reset: variable \"%s\" not found\n", var_name);
	else
		Cvar_Set (var->name, var->default_string);
}

/*
============
Cvar_Set
============
*/
void Cvar_Set (char *var_name, char *value)
{
	cvar_t	*var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
	{	// there is an error in C code if this happens
		Con_Printf ("Cvar_Set: variable \"%s\" not found\n", var_name);
		return;
	}

	if (var->flags & CVAR_ROM) {
		Con_Printf ("Cvar_Set: variable \"%s\" is read-only, cannot modify\n", var_name);
		return;
	}
	
	if (!strcmp(var->string, value))
		return;	// no change
	
	Z_Free (var->string);	// free the old value string
	
	var->string = Z_Strdup (value);
	var->value = atof (var->string);

	//johnfitz -- during initialization, update default too
	if (!host_initialized)
	{
		Z_Free (var->default_string);
		
		var->default_string = Z_Strdup (value);
	}
	//johnfitz

	if (var->flags & CVAR_SERVER)
	{
		if (sv.active)
			SV_BroadcastPrintf ("\"%s\" changed to \"%s\"\n", var->name, var->string);
	}

	//johnfitz
	if(var->callback)
		var->callback();
	//johnfitz

	// rcon (64 doesn't mean anything special, but we need some extra space because NET_MAXMESSAGE == RCON_BUFF_SIZE)
	if (rcon_active && (rcon_message.cursize < rcon_message.maxsize - (int)strlen(var->name) - (int)strlen(var->string) - 64))
	{
		rcon_message.cursize--;
		MSG_WriteString(&rcon_message, va("\"%s\" set to \"%s\"\n", var->name, var->string));
	}
}


/*
============
Cvar_SetValue
============
*/
void Cvar_SetValue (char *var_name, float value)
{
	static char str[32];
	int			i;

	sprintf (str, "%f", value);

	// Strip off ending zeros
	for (i = strlen(str) - 1 ; i > 0 && str[i] == '0' ; i--)
		str[i] = 0;

	// Strip off ending period
	if (str[i] == '.')
		str[i] = 0;

	Cvar_Set (var_name, str); 
}


/*
============
Cvar_SetROM
============
*/
void Cvar_SetROM (char *var_name, char *value)
{
	cvar_t *var = Cvar_FindVar (var_name);
	
	if (var)
	{
		var->flags &= ~CVAR_ROM;
		Cvar_Set (var->name, value);
		var->flags |= CVAR_ROM;
	}
}

/*
============
Cvar_SetValueROM
============
*/
void Cvar_SetValueROM (char *var_name, float value)
{
	cvar_t *var = Cvar_FindVar (var_name);
	
	if (var)
	{
		var->flags &= ~CVAR_ROM;
		Cvar_SetValue (var->name, value);
		var->flags |= CVAR_ROM;
	}
}


/*
============
Cvar_RegisterVariable

Adds a freestanding variable to the variable list.
============
*/
void Cvar_RegisterVariable (cvar_t *var)
{
	Cvar_RegisterVariableCallback (var, NULL);
}

void Cvar_RegisterVariableCallback (cvar_t *var, void *function)
{
	cvar_t	*cursor,*prev; //johnfitz -- sorted list insert

// first check to see if it has already been defined
	if (Cvar_FindVar (var->name))
	{
		Con_Printf ("Can't register variable %s, already defined\n", var->name);
		return;
	}
	
// check for overlap with a command
	if (Cmd_Exists (var->name))
	{
		Con_Printf ("   %s is a command\n", var->name);
		return;
	}
		
// copy the value off, because future sets will Z_Free it
	var->string = Z_Strdup (var->string);	
	var->value = atof (var->string);

	//johnfitz -- save initial value for "reset" command
	var->default_string = Z_Strdup (var->string);
	//johnfitz

// link the variable in
	//johnfitz -- insert each entry in alphabetical order
	if (cvar_vars == NULL || strcmp(var->name, cvar_vars->name) < 0) // insert at front
	{
		var->next = cvar_vars;
		cvar_vars = var;
	}
	else //insert later
	{
		prev = cvar_vars;
		cursor = cvar_vars->next;
		while (cursor && (strcmp(var->name, cursor->name) > 0))
		{
			prev = cursor;
			cursor = cursor->next;
		}
		var->next = prev->next;
		prev->next = var;
	}
	//johnfitz

	var->callback = function; //johnfitz
}

/*
============
Cvar_Command

Handles variable inspection and changing from the console
============
*/
qboolean	Cvar_Command (void)
{
	cvar_t			*var;

// check variables
	var = Cvar_FindVar (Cmd_Argv(0));
	if (!var)
		return false;
		
// perform a variable print or set
	if (Cmd_Argc() == 1)
	{
		Con_Printf ("\"%s\" is \"%s\"\n", var->name, var->string);
		return true;
	}

	Cvar_Set (var->name, Cmd_Argv(1));
	return true;
}


/*
============
Cvar_WriteVariables

Writes lines containing "set variable value" for all variables
with the archive flag set to true.
============
*/
void Cvar_WriteVariables (FILE *f)
{
	cvar_t	*var;
	
	for (var = cvar_vars ; var ; var = var->next)
		if (var->flags & CVAR_ARCHIVE)
			fprintf (f, "%s \"%s\"\n", var->name, var->string);
}

