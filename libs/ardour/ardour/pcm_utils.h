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

    $Id$
*/

#ifndef __ardour_pcm_utils_h__
#define __ardour_pcm_utils_h__

typedef	void	tribyte ;

#define	SIZEOF_TRIBYTE	3

#define BET2H_INT_PTR(x)		(((x) [0] << 24) + ((x) [1] << 16) + ((x) [2] << 8))
#define LET2H_INT_PTR(x)		(((x) [0] << 8) + ((x) [1] << 16) + ((x) [2] << 24))



void pcm_let2f_array (tribyte *src, int count, float *dest);
void pcm_bet2f_array (tribyte *src, int count, float *dest);
void pcm_f2let_array (float *src, tribyte *dest, int count);
void pcm_f2let_clip_array (float *src, tribyte *dest, int count);
void pcm_f2bet_array (const float *src, tribyte *dest, int count);
void pcm_f2bet_clip_array (const float *src, tribyte *dest, int count);



#endif
