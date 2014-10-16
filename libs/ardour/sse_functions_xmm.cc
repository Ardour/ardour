/*
    Copyright (C) 2007 Paul sDavis
    Written by Sampo Savolainen

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

#include <xmmintrin.h>
#include "ardour/types.h"

void
x86_sse_find_peaks(const ARDOUR::Sample* buf, ARDOUR::pframes_t nframes, float *min, float *max)
{
	__m128 current_max, current_min, work;

	// Load max and min values into all four slots of the XMM registers
	current_min = _mm_set1_ps(*min);
	current_max = _mm_set1_ps(*max);

	// Work input until "buf" reaches 16 byte alignment
	while ( ((intptr_t)buf) % 16 != 0 && nframes > 0) {

		// Load the next float into the work buffer
		work = _mm_set1_ps(*buf);

		current_min = _mm_min_ps(current_min, work);
		current_max = _mm_max_ps(current_max, work);

		buf++;
		nframes--;
	}

        // use 64 byte prefetch for quadruple quads
        while (nframes >= 16) {
#ifdef COMPILER_MSVC                                    
				_mm_prefetch(((char*)buf+64), 0);  // A total guess! Assumed to be eqivalent to
#else                                              // the line below but waiting to be tested !!
                __builtin_prefetch(buf+64,0,0);
#endif
                work = _mm_load_ps(buf);
                current_min = _mm_min_ps(current_min, work);
                current_max = _mm_max_ps(current_max, work);
                buf+=4;
                work = _mm_load_ps(buf);
                current_min = _mm_min_ps(current_min, work);
                current_max = _mm_max_ps(current_max, work);
                buf+=4;
                work = _mm_load_ps(buf);
                current_min = _mm_min_ps(current_min, work);
                current_max = _mm_max_ps(current_max, work);
                buf+=4;
                work = _mm_load_ps(buf);
                current_min = _mm_min_ps(current_min, work);
                current_max = _mm_max_ps(current_max, work);
                buf+=4;
                nframes-=16;
        }

	// work through aligned buffers
	while (nframes >= 4) {

		work = _mm_load_ps(buf);

		current_min = _mm_min_ps(current_min, work);
		current_max = _mm_max_ps(current_max, work);

		buf+=4;
		nframes-=4;
	}

	// work through the rest < 4 samples
	while ( nframes > 0) {

		// Load the next float into the work buffer
		work = _mm_set1_ps(*buf);

		current_min = _mm_min_ps(current_min, work);
		current_max = _mm_max_ps(current_max, work);

		buf++;
		nframes--;
	}

	// Find min & max value in current_max through shuffle tricks

	work = current_min;
	work = _mm_shuffle_ps(work, work, _MM_SHUFFLE(2, 3, 0, 1));
	work = _mm_min_ps (work, current_min);
	current_min = work;
	work = _mm_shuffle_ps(work, work, _MM_SHUFFLE(1, 0, 3, 2));
	work = _mm_min_ps (work, current_min);

	_mm_store_ss(min, work);

	work = current_max;
	work = _mm_shuffle_ps(work, work, _MM_SHUFFLE(2, 3, 0, 1));
	work = _mm_max_ps (work, current_max);
	current_max = work;
	work = _mm_shuffle_ps(work, work, _MM_SHUFFLE(1, 0, 3, 2));
	work = _mm_max_ps (work, current_max);

	_mm_store_ss(max, work);
}



