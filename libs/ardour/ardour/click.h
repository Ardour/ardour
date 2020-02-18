/*
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
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

#ifndef __ardour_click_h__
#define __ardour_click_h__

#include <list>

#include "pbd/pool.h"
#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "ardour/io.h"

namespace ARDOUR {

class LIBARDOUR_API Click {
public:
	samplepos_t start;
	samplecnt_t duration;
	samplecnt_t offset;
	const Sample *data;

	Click (samplepos_t s, samplecnt_t d, const Sample *b) : start (s), duration (d), offset (0), data (b) {}

	void *operator new (size_t) {
		return pool.alloc ();
    };

	void operator delete(void *ptr, size_t /*size*/) {
		pool.release (ptr);
	}

private:
	static Pool pool;
};

class LIBARDOUR_API ClickIO : public IO
{
public:
	ClickIO (Session& s, const std::string& name) : IO (s, name, IO::Output) {}
	~ClickIO() {}

protected:
	uint32_t pans_required () const { return 1; }
};

}; /* namespace ARDOUR */

#endif /*__ardour_click_h__ */
