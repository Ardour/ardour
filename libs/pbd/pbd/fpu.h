/*
 * Copyright (C) 2007-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015 Robin Gareus <robin@gareus.org>
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

#ifndef __pbd_fpu_h__
#define __pbd_fpu_h__

#include "pbd/libpbd_visibility.h"

namespace PBD {

class LIBPBD_API FPU {
  private:
	enum Flags {
		HasFlushToZero = 0x1,
		HasDenormalsAreZero = 0x2,
		HasSSE = 0x4,
		HasSSE2 = 0x8,
		HasAVX = 0x10,
		HasNEON = 0x20,
		HasFMA = 0x40,
	};

  public:
	~FPU ();

	static FPU* instance();
	static void destroy();

	bool has_flush_to_zero () const { return _flags & HasFlushToZero; }
	bool has_denormals_are_zero () const { return _flags & HasDenormalsAreZero; }
	bool has_sse () const { return _flags & HasSSE; }
	bool has_sse2 () const { return _flags & HasSSE2; }
	bool has_avx () const { return _flags & HasAVX; }
	bool has_fma() const { return _flags & HasFMA; }
	bool has_neon () const { return _flags & HasNEON; }

  private:
	Flags _flags;

	static FPU* _instance;

	FPU ();
};

}

#endif /* __pbd_fpu_h__ */
