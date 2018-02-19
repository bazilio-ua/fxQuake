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
// mathlib.h -- math primitives

typedef float vec_t;
typedef vec_t vec3_t[3];
typedef vec_t vec5_t[5];

#ifndef M_PI
#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
#endif

#define M_PI_DIV_180 (M_PI / 180.0F)
#define DEG2RAD( a ) ( (a) * M_PI_DIV_180 )

extern vec3_t vec3_origin;

#define	NANMASK		(255 << 23)	/* 7F800000 */

// new func to avoid violating strict aliasing rules
static inline int IS_NAN (float x) {
	union { float f; int i; } num;
	num.f = x;
	return ((num.i & NANMASK) == NANMASK);
}

#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

// Will return minval also if minval > maxval
#define CLAMP(minval, x, maxval) ((x) < (minval) || (minval) > (maxval) ? (minval) : (x) > (maxval) ? (maxval) : (x)) // johnfitz

#define Q_rint(x) ((x) > 0 ? (int)((x) + 0.5) : (int)((x) - 0.5)) // johnfitz -- from joequake

/*-----------------------------------------------------------------*/

float	anglemod(float a);

/*-----------------------------------------------------------------*/

vec_t DotProduct (vec3_t v1, vec3_t v2);
void CrossProduct (vec3_t v1, vec3_t v2, vec3_t cross);

void LerpVector (vec3_t from, vec3_t to, float frac, vec3_t out);
void LerpAngles (vec3_t from, vec3_t to, float frac, vec3_t out);

void VectorAngles (vec3_t forward, vec3_t angles);
void AngleVectors (vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);

void VectorMA (vec3_t veca, float scale, vec3_t vecb, vec3_t vecc);
void VectorSubtract (vec3_t veca, vec3_t vecb, vec3_t out);
void VectorAdd (vec3_t veca, vec3_t vecb, vec3_t out);
void VectorCopy (vec3_t in, vec3_t out);
int VectorCompare (vec3_t v1, vec3_t v2);
vec_t VectorLength(vec3_t v);

float VectorNormalize (vec3_t v);
void VectorInverse (vec3_t v);
void VectorNegate (vec3_t in, vec3_t out);
void VectorSet (vec3_t v, float a, float b, float c);
void VectorClear (vec3_t v);
void VectorScale (vec3_t in, vec_t scale, vec3_t out);
void TurnVector (vec3_t out, vec3_t forward, vec3_t side, float angle);
void VectorNormalizeFast(vec3_t v);

/*-----------------------------------------------------------------*/

struct mplane_s;

int BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct mplane_s *p);

#define BOX_ON_PLANE_SIDE(emins, emaxs, p)	\
	(((p)->type < 3)?						\
	(										\
		((p)->dist <= (emins)[(p)->type])?	\
			1								\
		:									\
		(									\
			((p)->dist >= (emaxs)[(p)->type])?\
				2							\
			:								\
				3							\
		)									\
	)										\
	:										\
		BoxOnPlaneSide( (emins), (emaxs), (p)))

