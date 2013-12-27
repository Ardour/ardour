/*
Copyright © 2013 Laurent de Soras <laurent.de.soras@free.fr>

This work is free. You can redistribute it and/or modify it under the
terms of the Do What The Fuck You Want To Public License, Version 2,
as published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
*/
#ifndef __pbd_fastlog_h__
#define __pbd_fastlog_h__

#include <math.h> /* for HUGE_VAL */

#include "pbd/libpbd_visibility.h"

static inline float fast_log2 (float val)
{
	/* don't use reinterpret_cast<> because that prevents this
	   from being used by pure C code (for example, GnomeCanvasItems)
	*/
	union {float f; int i;} t;
	t.f = val;
	int * const    exp_ptr =  &t.i;
	int            x = *exp_ptr;
	const int      log_2 = ((x >> 23) & 255) - 128;
	x &= ~(255 << 23);
	x += 127 << 23;
	*exp_ptr = x;
	
	val = ((-1.0f/3) * t.f + 2) * t.f - 2.0f/3;
	
	return (val + log_2);
}

static inline float fast_log (const float val)
{
	return (fast_log2 (val) * 0.69314718f);
}

static inline float fast_log10 (const float val)
{
	return fast_log2(val) / 3.312500f;
}

static inline float minus_infinity(void) { return -HUGE_VAL; }

#endif /* __pbd_fastlog_h__ */
