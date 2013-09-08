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
// mathlib.h

typedef float vec_t;
typedef vec_t vec3_t[3];
typedef vec_t vec5_t[5];

#ifndef M_PI
#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
#endif

#define M_PI_DIV_180 (M_PI / 180.0F)
#define DEG2RAD( a ) ( (a) * M_PI_DIV_180 )

struct mplane_s;

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

inline float	anglemod(float a)
{
	a = (360.0/65536) * ((int)(a*(65536/360.0)) & 65535);
	return a;
}

/*-----------------------------------------------------------------*/

static inline vec_t DotProduct (vec3_t v1, vec3_t v2)
{
	return v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
}

inline vec_t VectorLength(vec3_t v)
{
	return sqrt(DotProduct(v,v));
}

inline void LerpVector (vec3_t from, vec3_t to, float frac, vec3_t out)
{
	out[0] = from[0] + frac * (to[0] - from[0]);
	out[1] = from[1] + frac * (to[1] - from[1]);
	out[2] = from[2] + frac * (to[2] - from[2]);
}

inline void LerpAngles (vec3_t from, vec3_t to, float frac, vec3_t out)
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
inline void VectorAngles (vec3_t forward, vec3_t angles)
{
	vec3_t temp;

	temp[0] = forward[0];
	temp[1] = forward[1];
	temp[2] = 0;

	angles[PITCH] = -atan2(forward[2], VectorLength(temp)) / M_PI_DIV_180;
	angles[YAW] = atan2(forward[1], forward[0]) / M_PI_DIV_180;
	angles[ROLL] = 0;
}

inline void AngleVectors (vec3_t angles, vec3_t forward, vec3_t right, vec3_t up)
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

inline void VectorMA (vec3_t veca, float scale, vec3_t vecb, vec3_t vecc)
{
	vecc[0] = veca[0] + scale*vecb[0];
	vecc[1] = veca[1] + scale*vecb[1];
	vecc[2] = veca[2] + scale*vecb[2];
}

static inline void VectorSubtract (vec3_t veca, vec3_t vecb, vec3_t out)
{
	out[0] = veca[0]-vecb[0];
	out[1] = veca[1]-vecb[1];
	out[2] = veca[2]-vecb[2];
}

static inline void VectorAdd (vec3_t veca, vec3_t vecb, vec3_t out)
{
	out[0] = veca[0]+vecb[0];
	out[1] = veca[1]+vecb[1];
	out[2] = veca[2]+vecb[2];
}

static inline void VectorCopy (vec3_t in, vec3_t out)
{
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
}

static inline void VectorSet (vec3_t v, float a, float b, float c)
{
	v[0] = a;
	v[1] = b;
	v[2] = c;
}

inline int VectorCompare (vec3_t v1, vec3_t v2)
{
	int		i;

	for (i=0 ; i<3 ; i++)
		if (v1[i] != v2[i])
			return 0;

	return 1;
}

inline void CrossProduct (vec3_t v1, vec3_t v2, vec3_t cross)
{
	cross[0] = v1[1]*v2[2] - v1[2]*v2[1];
	cross[1] = v1[2]*v2[0] - v1[0]*v2[2];
	cross[2] = v1[0]*v2[1] - v1[1]*v2[0];
}

inline float VectorNormalize (vec3_t v)
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

// (RMQ Engine)
inline float VectorNormalize3f (float *x, float *y, float *z)
{
	float	length, ilength;

	length = x[0] * x[0] + y[0] * y[0] + z[0] * z[0];
	length = sqrt(length);

	if (length)
	{
		ilength = 1/length;
		x[0] *= ilength;
		y[0] *= ilength;
		z[0] *= ilength;
	}

	return length;
}

inline void VectorInverse (vec3_t v)
{
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}

inline void VectorScale (vec3_t in, vec_t scale, vec3_t out)
{
	out[0] = in[0]*scale;
	out[1] = in[1]*scale;
	out[2] = in[2]*scale;
}

/*-----------------------------------------------------------------*/

//johnfitz -- courtesy of lordhavoc
// QuakeSpasm: To avoid strict aliasing violations, use a float/int union instead of type punning.
static inline void VectorNormalizeFast(vec3_t v)
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

/*-----------------------------------------------------------------*/

int BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct mplane_s *plane);

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

