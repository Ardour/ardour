/*
    Copyright (C) 2006 Paul Davis , portions Erik de Castro Lopo

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifdef COMPILER_MSVC
#include <ardourext/float_cast.h>
#endif
#include "ardour/pcm_utils.h"
#include <cmath>

using namespace std;

// TODO: check CPU_CLIPS_POSITIVE and CPU_CLIPS_NEGATIVE with scons
#define CPU_CLIPS_NEGATIVE 0
#define CPU_CLIPS_POSITIVE 0

/* these routines deal with 24 bit int handling (tribytes)
 *   originally from libsndfile, but modified.  XXX - Copyright Erik de Castro Lopo
 */

void
pcm_let2f_array (tribyte *src, int count, float *dest)
{
	/* Special normfactor because tribyte value is read into an int. */
	static const float normfact = 1.0 / ((float) 0x80000000);

	unsigned char	*ucptr ;
	int				value ;

	ucptr = ((unsigned char*) src) + 3 * count ;
	while (--count >= 0)
	{	ucptr -= 3 ;
		value = LET2H_INT_PTR (ucptr) ;
		dest [count] = ((float) value) * normfact ;
		} ;
} /* let2f_array */

void
pcm_bet2f_array (tribyte *src, int count, float *dest)
{
	/* Special normfactor because tribyte value is read into an int. */
	static const float normfact = 1.0 / ((float) 0x80000000);

	unsigned char	*ucptr ;
	int				value ;


	ucptr = ((unsigned char*) src) + 3 * count ;
	while (--count >= 0)
	{	ucptr -= 3 ;
		value = BET2H_INT_PTR (ucptr) ;
		dest [count] = ((float) value) * normfact ;
			} ;
} /* bet2f_array */

void
pcm_f2let_array (float *src, tribyte *dest, int count)
{
	static const float normfact = (1.0 * 0x7FFFFF);

	unsigned char	*ucptr ;
	int				value ;

	ucptr = ((unsigned char*) dest) + 3 * count ;

	while (count)
	{	count -- ;
		ucptr -= 3 ;
		value = lrintf (src [count] * normfact) ;
		ucptr [0] = value ;
		ucptr [1] = value >> 8 ;
		ucptr [2] = value >> 16 ;
		} ;
} /* f2let_array */

void
pcm_f2let_clip_array (float *src, tribyte *dest, int count)
{
	static const float normfact = (8.0 * 0x10000000);

	unsigned char	*ucptr ;
	float			scaled_value ;
	int				value ;

	ucptr = ((unsigned char*) dest) + 3 * count ;

	while (count)
	{	count -- ;
		ucptr -= 3 ;
		scaled_value = src [count] * normfact ;
		if (CPU_CLIPS_POSITIVE == 0 && scaled_value >= (1.0 * 0x7FFFFFFF))
		{	ucptr [0] = 0xFF ;
			ucptr [1] = 0xFF ;
			ucptr [2] = 0x7F ;
			continue ;
			} ;
		if (CPU_CLIPS_NEGATIVE == 0 && scaled_value <= (-8.0 * 0x10000000))
		{	ucptr [0] = 0x00 ;
			ucptr [1] = 0x00 ;
			ucptr [2] = 0x80 ;
			continue ;
			} ;

		value = lrintf (scaled_value) ;
		ucptr [0] = value >> 8 ;
		ucptr [1] = value >> 16 ;
		ucptr [2] = value >> 24 ;
		} ;
} /* f2let_clip_array */

void
pcm_f2bet_array (const float *src, tribyte *dest, int count)
{
	static const float normfact = (1.0 * 0x7FFFFF);

	unsigned char	*ucptr ;
	int				value ;

	ucptr = ((unsigned char*) dest) + 3 * count ;

	while (--count >= 0)
	{	ucptr -= 3 ;
		value = lrintf (src [count] * normfact) ;
		ucptr [0] = value >> 16 ;
		ucptr [1] = value >> 8 ;
		ucptr [2] = value ;
		} ;
} /* f2bet_array */

void
pcm_f2bet_clip_array (const float *src, tribyte *dest, int count)
{
	static const float normfact = (8.0 * 0x10000000);

	unsigned char	*ucptr ;
	float			scaled_value ;
	int				value ;

	ucptr = ((unsigned char*) dest) + 3 * count ;

	while (--count >= 0)
	{	ucptr -= 3 ;
		scaled_value = src [count] * normfact ;
		if (CPU_CLIPS_POSITIVE == 0 && scaled_value >= (1.0 * 0x7FFFFFFF))
		{	ucptr [0] = 0x7F ;
			ucptr [1] = 0xFF ;
			ucptr [2] = 0xFF ;
			continue ;
			} ;
		if (CPU_CLIPS_NEGATIVE == 0 && scaled_value <= (-8.0 * 0x10000000))
		{	ucptr [0] = 0x80 ;
			ucptr [1] = 0x00 ;
			ucptr [2] = 0x00 ;
			continue ;
		} ;

		value = lrint (scaled_value) ;
		ucptr [0] = value >> 24 ;
		ucptr [1] = value >> 16 ;
		ucptr [2] = value >> 8 ;
		} ;
} /* f2bet_clip_array */

//@@@@@@@
