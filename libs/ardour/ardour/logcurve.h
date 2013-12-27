/*
    Copyright (C) 2001 Steve Harris & Paul Davis

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

#ifndef __ardour_logcurve_h__
#define __ardour_logcurve_h__

#include "pbd/fastlog.h"
#include <glibmm/threads.h>

namespace ARDOUR {

class LIBARDOUR_API LogCurve {
  public:
	LogCurve (float steepness = 0.2, uint32_t len = 0) {
		l = len;
		S = steepness;
		a = log(S);
		b = 1.0f / log(1.0f + (1.0f / S));
	}

	bool operator== (const LogCurve& other) const {
		return S == other.S && l == other.l;
	}

	bool operator!= (const LogCurve& other) const {
		return S != other.S || l != other.l;
	}

	float value (float frac) const {
		return (fast_log(frac + S) - a) * b;
	}

	float value (uint32_t pos) const {
		return (fast_log(((float) pos/l) + S) - a) * b;
	}

	float invert_value (float frac) const {
		return (a - fast_log(frac + S)) * b;
	}

	float invert_value (uint32_t pos) const {
		return (a - fast_log(((float) pos/l) + S)) * b;
	}

	void fill (float *vec, uint32_t veclen, bool invert) const {
		float dx = 1.0f/veclen;
		float x;
		uint32_t i;

		if (!invert) {

			vec[0] = 0.0;
			vec[veclen-1] = 1.0;

			for (i = 1, x = 0; i < veclen - 1; x += dx, i++) {
				vec[i] = value (x);
			}

		} else {

			vec[0] = 1.0;
			vec[veclen-1] = 0.0;

			for (i = veclen-2, x = 0.0f; i > 0; x += dx, i--) {
				vec[i] = value (x);
			}
		}
	}

	float steepness() const { return S; }
	uint32_t length() const { return l; }

	void set_steepness (float steepness) {
		S = steepness;
		a = log(S);
		b = 1.0f / log(1.0f + (1.0f / S));
	}
	void set_length (uint32_t len) { l = len; }

	mutable Glib::Threads::Mutex lock;

  protected:
	float a;
	float b;
	float S;
	uint32_t l;
};

class LIBARDOUR_API LogCurveIn : public LogCurve
{
  public:
	LogCurveIn (float steepness = 0.2, uint32_t len = 0)
		: LogCurve (steepness, len) {}

	float value (float frac) const {
		return (fast_log(frac + S) - a) * b;
	}

	float value (uint32_t pos) const {
		return (fast_log(((float) pos/l) + S) - a) * b;
	}
};

class LIBARDOUR_API LogCurveOut : public LogCurve
{
  public:
	LogCurveOut (float steepness = 0.2, uint32_t len = 0)
		: LogCurve (steepness, len) {}

};

} // namespace ARDOUR

#endif /* __ardour_logcurve_h__ */


