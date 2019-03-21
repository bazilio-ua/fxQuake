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
// mathlib.c -- math primitives

#include "quakedef.h"

vec3_t vec3_origin = {0,0,0};

/*-----------------------------------------------------------------*/

float	anglemod(float a)
{
	a = (360.0/65536) * ((int)(a*(65536/360.0)) & 65535);
	return a;
}

/*-----------------------------------------------------------------*/

vec_t PreciseDotProduct (vec3_t v1, vec3_t v2)
{
	return ((double)v1[0]*v2[0] + (double)v1[1]*v2[1] + (double)v1[2]*v2[2]);
}

vec_t DotProduct (vec3_t v1, vec3_t v2)
{
	return v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
}

void CrossProduct (vec3_t v1, vec3_t v2, vec3_t cross)
{
	cross[0] = v1[1]*v2[2] - v1[2]*v2[1];
	cross[1] = v1[2]*v2[0] - v1[0]*v2[2];
	cross[2] = v1[0]*v2[1] - v1[1]*v2[0];
}

void LerpVector (vec3_t from, vec3_t to, float frac, vec3_t out)
{
	out[0] = from[0] + frac * (to[0] - from[0]);
	out[1] = from[1] + frac * (to[1] - from[1]);
	out[2] = from[2] + frac * (to[2] - from[2]);
}

void LerpAngles (vec3_t from, vec3_t to, float frac, vec3_t out)
{
	int i;
	float delta;
    
	for (i = 0; i < 3; i++)
	{
		delta = to[i] - from[i];
        
		if (delta > 180)
			delta -= 360;
		else if (delta < -180)
			delta += 360;
        
		out[i] = from[i] + frac * delta;
	}
}

//the opposite of AngleVectors.  this takes forward and generates pitch yaw roll
//TODO: take right and up vectors to properly set yaw and roll
void VectorAngles (vec3_t forward, vec3_t angles)
{
	vec3_t temp;
    
	temp[0] = forward[0];
	temp[1] = forward[1];
	temp[2] = 0;
    
	angles[PITCH] = -atan2(forward[2], VectorLength(temp)) / M_PI_DIV_180;
	angles[YAW] = atan2(forward[1], forward[0]) / M_PI_DIV_180;
	angles[ROLL] = 0;
}

void AngleVectors (vec3_t angles, vec3_t forward, vec3_t right, vec3_t up)
{
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	
	angle = angles[YAW] * (M_PI * 2 / 360);
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[PITCH] * (M_PI * 2 / 360);
	sp = sin(angle);
	cp = cos(angle);
	angle = angles[ROLL] * (M_PI * 2 / 360);
	sr = sin(angle);
	cr = cos(angle);
    
	forward[0] = cp*cy;
	forward[1] = cp*sy;
	forward[2] = -sp;
	right[0] = (-1*sr*sp*cy+-1*cr*-sy);
	right[1] = (-1*sr*sp*sy+-1*cr*cy);
	right[2] = -1*sr*cp;
	up[0] = (cr*sp*cy+-sr*-sy);
	up[1] = (cr*sp*sy+-sr*cy);
	up[2] = cr*cp;
}

void VectorMA (vec3_t veca, float scale, vec3_t vecb, vec3_t vecc)
{
	vecc[0] = veca[0] + scale*vecb[0];
	vecc[1] = veca[1] + scale*vecb[1];
	vecc[2] = veca[2] + scale*vecb[2];
}

void VectorSubtract (vec3_t veca, vec3_t vecb, vec3_t out)
{
	out[0] = veca[0]-vecb[0];
	out[1] = veca[1]-vecb[1];
	out[2] = veca[2]-vecb[2];
}

void VectorAdd (vec3_t veca, vec3_t vecb, vec3_t out)
{
	out[0] = veca[0]+vecb[0];
	out[1] = veca[1]+vecb[1];
	out[2] = veca[2]+vecb[2];
}

void VectorCopy (vec3_t in, vec3_t out)
{
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
}

int VectorCompare (vec3_t v1, vec3_t v2)
{
	int		i;
    
	for (i=0 ; i<3 ; i++)
		if (v1[i] != v2[i])
			return 0;
    
	return 1;
}

vec_t VectorLength(vec3_t v)
{
	return sqrt(DotProduct(v,v));
}

float VectorNormalize (vec3_t v)
{
	float	length, ilength;
    
	length = sqrt(DotProduct(v,v));
    
	if (length)
	{
		ilength = 1/length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}
    
	return length;
}

void VectorInverse (vec3_t v)
{
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}

void VectorNegate (vec3_t in, vec3_t out)
{
	out[0] = -in[0];
	out[1] = -in[1];
	out[2] = -in[2];
}

void VectorSet (vec3_t v, float a, float b, float c)
{
	v[0] = a;
	v[1] = b;
	v[2] = c;
}

void VectorClear (vec3_t v)
{
	v[0] = 0;
	v[1] = 0;
	v[2] = 0;
}

void VectorScale (vec3_t in, vec_t scale, vec3_t out)
{
	out[0] = in[0]*scale;
	out[1] = in[1]*scale;
	out[2] = in[2]*scale;
}

/*
 ===============
 TurnVector
 
 turn forward towards side on the plane defined by forward and side
 if angle = 90, the result will be equal to side
 assumes side and forward are perpendicular, and normalized
 to turn away from side, use a negative angle
 ===============
 */
void TurnVector (vec3_t out, vec3_t forward, vec3_t side, float angle)
{
	float scale_forward, scale_side;
    
	scale_forward = cos (DEG2RAD(angle));
	scale_side = sin (DEG2RAD(angle));
    
	out[0] = scale_forward*forward[0] + scale_side*side[0];
	out[1] = scale_forward*forward[1] + scale_side*side[1];
	out[2] = scale_forward*forward[2] + scale_side*side[2];
}

//johnfitz -- courtesy of lordhavoc
// QuakeSpasm: To avoid strict aliasing violations, use a float/int union instead of type punning.
void VectorNormalizeFast(vec3_t v)
{
	union { float f; int i; } y, num;
	num.f = DotProduct(v, v);
	if (num.f != 0.0)
	{
		y.i = 0x5f3759df - (num.i >> 1);
		y.f = y.f * (1.5f - (num.f * 0.5f * y.f * y.f));
		VectorScale(v, y.f, v);
	}
} 


/*
===============
CalcBoundingSphere -- EER1
===============
*/
void CalcBoundingSphere(vec3_t *firstposition, int numverts, vec3_t center, float *radius)
{
    vec3_t temp;
    float d;
    int i;
    
    VectorSet(temp, 0, 0, 0);
    *radius = 0.0f;
    
    for(i=0; i<numverts; i++)
        VectorAdd(temp, firstposition[i], temp);
    
    VectorScale(temp, 1.0f / numverts, center);
    
    for(i=0; i<numverts; i++)
    {
        VectorSubtract(firstposition[i], center, temp);
        d = VectorLength(temp);
        if (d > *radius)
            *radius = d;
    }
}

