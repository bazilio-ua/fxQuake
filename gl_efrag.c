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
// gl_efrag.c

#include "quakedef.h"


//===========================================================================

/*
===============================================================================

					ENTITY FRAGMENT FUNCTIONS

ericw -- GLQuake only uses efrags for static entities, and they're never
removed, so I trimmed out unused functionality and fields in efrag_t.

Now, efrags are just a linked list for each leaf of the static
entities that touch that leaf. The efrags are hunk-allocated so there is no
fixed limit.

This is inspired by MH's tutorial, and code from RMQEngine.
http://forums.insideqc.com/viewtopic.php?t=1930

===============================================================================
*/

// let's get rid of some more globals...
typedef struct r_efragdef_s
{
    vec3_t		mins, maxs;
    entity_t	*addent;
} r_efragdef_t;

#define EXTRA_EFRAGS	128

// based on RMQEngine
efrag_t *R_GetEfrag (void)
{
	// we could just Hunk_Alloc a single efrag_t and return it, but since
	// the struct is so small (2 pointers) allocate groups of them
	// to avoid wasting too much space on the hunk allocation headers.
	
	if (cl.free_efrags)
	{
		efrag_t *ef = cl.free_efrags;
		cl.free_efrags = ef->leafnext;
		ef->leafnext = NULL;
		
		return ef;
	}
	else
	{
		int i;
		
		cl.free_efrags = (efrag_t *) Hunk_AllocName (EXTRA_EFRAGS * sizeof (efrag_t), "efrags");
		
		for (i = 0; i < EXTRA_EFRAGS - 1; i++)
			cl.free_efrags[i].leafnext = &cl.free_efrags[i + 1];
		
		cl.free_efrags[i].leafnext = NULL;
		
		// call recursively to get a newly allocated free efrag
		return R_GetEfrag ();
	}
}


/*
===================
R_SplitEntityOnNode
===================
*/
void R_SplitEntityOnNode (mnode_t *node, r_efragdef_t *ed)
{
	efrag_t		*ef;
	mplane_t	*splitplane;
	mleaf_t		*leaf;
	int			sides;
	
	if (node->contents == CONTENTS_SOLID)
		return;
	
// add an efrag if the node is a leaf
	if (node->contents < 0)
	{
		leaf = (mleaf_t *)node;
        
// grab an efrag off the free list
		ef = R_GetEfrag();
		ef->entity = ed->addent;
        
// set the leaf links
		ef->leafnext = leaf->efrags;
		leaf->efrags = ef;
        
		return;
	}
	
// NODE_MIXED

// split on this plane
	splitplane = node->plane;
	sides = BOX_ON_PLANE_SIDE(ed->mins, ed->maxs, splitplane);

// recurse down the contacted sides
	if (sides & 1)
		R_SplitEntityOnNode (node->children[0], ed);
	if (sides & 2)
		R_SplitEntityOnNode (node->children[1], ed);
}


/*
===========
R_AddEfrags
===========
*/
void R_AddEfrags (entity_t *ent)
{
    r_efragdef_t ed;
	model_t		*entmodel;
	vec_t		scalefactor;

	// entities with no model won't get drawn
	if (!ent->model)
		return;

    // never add the world
	if (ent == &cl_entities[0]) 
        return;

	// init the efrag definition struct so that we can avoid more ugly globals
	ed.addent = ent;
			
	entmodel = ent->model;
	
	scalefactor = ENTSCALE_DECODE(ent->scale);
	if (scalefactor != 1.0f)
	{
		VectorMA (ent->origin, scalefactor, entmodel->mins, ed.mins);
		VectorMA (ent->origin, scalefactor, entmodel->maxs, ed.maxs);
	}
	else
	{
		VectorAdd (ent->origin, entmodel->mins, ed.mins);
		VectorAdd (ent->origin, entmodel->maxs, ed.maxs);
	}
	
	if (!cl.worldmodel)
		Host_Error ("R_AddEfrags: NULL worldmodel");

	R_SplitEntityOnNode (cl.worldmodel->nodes, &ed);
}


/*
================
R_StoreEfrags

johnfitz -- pointless switch statement removed.
================
*/
void R_StoreEfrags (efrag_t **efrags)
{
	entity_t	*ent;
	efrag_t		*efrag;

    while ((efrag = *efrags) != NULL)
	{
		ent = efrag->entity;
        
        if (!ent)
            Host_Error ("R_StoreEfrags: ent is NULL");

        // some progs might try to send static ents with no model through here...
		if (!ent->model) 
            continue;
        
		// prevent adding twice in this render frame (or if an entity is in more than one leaf)
		if ((ent->visframe != r_framecount) && (cl_numvisedicts < MAX_VISEDICTS))
		{
			// add it to the visible edicts list
			cl_visedicts[cl_numvisedicts++] = ent;
            
            // mark that we've recorded this entity for this frame
			ent->visframe = r_framecount;
		}
        
		efrags = &efrag->leafnext;
	}
}

