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

//efrag_t		**lastlink;

vec3_t		r_emins, r_emaxs;

entity_t	*r_addent;

#define EXTRA_EFRAGS	128

// based on RMQEngine
static efrag_t *R_GetEfrag (void)
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
================
R_RemoveEfrags

Call when removing an object from the world or moving it to another position
================
*/
//void R_RemoveEfrags (entity_t *ent)
//{
//	efrag_t		*ef, *old, *walk, **prev;
//	
//	ef = ent->efrag;
//	
//	while (ef)
//	{
//		prev = &ef->leaf->efrags;
//		while (1)
//		{
//			walk = *prev;
//			if (!walk)
//				break;
//			if (walk == ef)
//			{	// remove this fragment
//				*prev = ef->leafnext;
//				break;
//			}
//			else
//				prev = &walk->leafnext;
//		}
//				
//		old = ef;
//		ef = ef->entnext;
//		
//	// put it on the free list
//		old->entnext = cl.free_efrags;
//		cl.free_efrags = old;
//	}
//	
//	ent->efrag = NULL; 
//}

/*
===================
R_SplitEntityOnNode
===================
*/
void R_SplitEntityOnNode (mnode_t *node)
{
	efrag_t		*ef;
	mplane_t	*splitplane;
	mleaf_t		*leaf;
	int			sides;
//	static float	lastmsg = 0;
	
	if (node->contents == CONTENTS_SOLID)
	{
		return;
	}
	
// add an efrag if the node is a leaf
    
	if ( node->contents < 0)
	{
		leaf = (mleaf_t *)node;
        
// grab an efrag off the free list
		ef = R_GetEfrag();
		ef->entity = r_addent;
        
// set the leaf links
		ef->leafnext = leaf->efrags;
		leaf->efrags = ef;
        
		return;
	}
    
//// add an efrag if the node is a leaf
//
//	if ( node->contents < 0)
//	{
//		leaf = (mleaf_t *)node;
//
//// grab an efrag off the free list
//		ef = cl.free_efrags;
//		if (!ef)
//		{
//			if (IsTimeout (&lastmsg, 2))
//				Con_DWarning ("R_SplitEntityOnNode: Too many efrags! (max = %d)\n", MAX_EFRAGS);
//
//			return;		// no free fragments...
//		}
//		cl.free_efrags = cl.free_efrags->entnext;
//
//		ef->entity = r_addent;
//		
//// add the entity link	
//		*lastlink = ef;
//		lastlink = &ef->entnext;
//		ef->entnext = NULL;
//		
//// set the leaf links
//		ef->leaf = leaf;
//		ef->leafnext = leaf->efrags;
//		leaf->efrags = ef;
//			
//		return;
//	}
	
// NODE_MIXED

// split on this plane
	splitplane = node->plane;
	sides = BOX_ON_PLANE_SIDE(r_emins, r_emaxs, splitplane);

// recurse down the contacted sides
	if (sides & 1)
		R_SplitEntityOnNode (node->children[0]);
	if (sides & 2)
		R_SplitEntityOnNode (node->children[1]);
}


/*
===========
R_AddEfrags
===========
*/
void R_AddEfrags (entity_t *ent)
{
	model_t		*entmodel;
	int			i;
		
	if (!ent->model)
		return;

	r_addent = ent;
			
//	lastlink = &ent->efrag;
	
	entmodel = ent->model;

	for (i=0 ; i<3 ; i++)
	{
		r_emins[i] = ent->origin[i] + entmodel->mins[i];
		r_emaxs[i] = ent->origin[i] + entmodel->maxs[i];
	}

	if (!cl.worldmodel)
		Host_Error ("R_AddEfrags: NULL worldmodel");

	R_SplitEntityOnNode (cl.worldmodel->nodes);
}


/*
================
R_StoreEfrags

// FIXME: a lot of this goes away with edge-based
johnfitz -- pointless switch statement removed.
================
*/
void R_StoreEfrags (efrag_t **ppefrag)
{
	entity_t	*pent;
//	qmodel_t		*clmodel;
	efrag_t		*pefrag;

    while ((pefrag = *ppefrag) != NULL)
	{
		pent = pefrag->entity;
        
        if (!pent)
            Host_Error ("R_StoreEfrags: pent is NULL");

		if ((pent->visframe != r_framecount) && (cl_numvisedicts < MAX_VISEDICTS))
		{
			cl_visedicts[cl_numvisedicts++] = pent;
			pent->visframe = r_framecount;
		}
        
		ppefrag = &pefrag->leafnext;
	}

//	while ((pefrag = *ppefrag) != NULL)
//	{
//		pent = pefrag->entity;
//
//		if (!pent)
//			Sys_Error ("R_StoreEfrags: pent is NULL");
//
//		clmodel = pent->model;
//
//		switch (clmodel->type)
//		{
//		case mod_alias:
//		case mod_brush:
//		case mod_sprite:
//			pent = pefrag->entity;
//
//			if ((pent->visframe != r_framecount) && (cl_numvisedicts < MAX_VISEDICTS))
//			{
//				cl_visedicts[cl_numvisedicts++] = pent;
//
//			// mark that we've recorded this entity for this frame
//				pent->visframe = r_framecount;
//			}
//
//			ppefrag = &pefrag->leafnext;
//			break;
//
//		default:	
//			Sys_Error ("R_StoreEfrags: Bad entity type %d", clmodel->type);
//		}
//	}
}

