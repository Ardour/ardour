/*
 * Copyright (C) 2003-2012 Fons Adriaensen <fons@kokkinizita.net>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2013 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __libardour_mtdm_h__
#define __libardour_mtdm_h__

#include <stddef.h>

#include "ardour/libardour_visibility.h"

class LIBARDOUR_API MTDM
{
public:
	MTDM (int fsamp);
	int  process (size_t len, float* inp, float* out);
	int  resolve (void);

	int inv (void)    const { return _inv; }
	double del (void) const { return _del; }
	double err (void) const { return _err; }

	void invert (void)
	{
		_inv ^= 1;
	}

	float get_peak ()
	{
		const float rv = _peak;
		_peak          = 0;
		return rv;
	}

private:
	class Freq
	{
		public:
			int   p;
			int   f;
			float xa;
			float ya;
			float x1;
			float y1;
			float x2;
			float y2;
	};

	double _del;
	double _err;
	float  _wlp;
	int    _cnt;
	int    _inv;
	Freq   _freq[13];
	float  _peak;
};

#endif /* __libardour_mtdm_h__ */
