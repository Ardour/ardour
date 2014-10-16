/* This code is based upon work that bore the legend:
 *
 * Copyright (C) 1997 David Mosberger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __ardour_spline_h__
#define __ardour_spline_h__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _spline Spline;
typedef struct _spline_point SplinePoint;

struct LIBARDOUR_API _spline_point
{
	float x;
	float y;
};

Spline *spline_new (void);
void    spline_free (Spline *);

void    spline_set (Spline *, uint32_t n, SplinePoint *);
void    spline_add (Spline *, uint32_t n, SplinePoint *);
void    spline_solve (Spline *);
float   spline_eval (Spline *, float val);
void    spline_fill (Spline *, float x0, float x1, float *vec, uint32_t veclen);
float   spline_get_max_x (Spline *);
float   spline_get_min_x (Spline *);

struct LIBARDOUR_API _spline
{
	float        *deriv2;
	float        *x;
	float        *y;
	float         max_x;
	float         min_x;
	SplinePoint  *points;
	uint32_t      npoints;
	uint32_t      space;

#ifdef  __cplusplus

	void set (uint32_t n, SplinePoint *points) {
		spline_set (this, n, points);
	}

	void add (uint32_t n, SplinePoint *points) {
		spline_add (this, n, points);
	}

	void solve () {
		spline_solve (this);
	}

	float eval (float val) {
		return spline_eval (this, val);
	}

	void fill (float x0, float x1, float *vec, uint32_t veclen) {
		spline_fill (this, x0, x1, vec, veclen);
	}

#endif /* __cplusplus */

};


#ifdef __cplusplus
}
#endif

#endif /* __ardour_spline_h__ */
