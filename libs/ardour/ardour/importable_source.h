/*
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2007 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
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

#ifndef __ardour_importable_source_h__
#define __ardour_importable_source_h__

#include "pbd/failed_constructor.h"
#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

namespace ARDOUR {

class LIBARDOUR_API ImportableSource {
public:
	ImportableSource () {}
	virtual ~ImportableSource() {}

	virtual samplecnt_t read (Sample* buffer, samplecnt_t nframes) = 0;
	virtual float       ratio() const { return 1.0f; }
	virtual uint32_t    channels() const = 0;
	virtual samplecnt_t length() const = 0;
	virtual samplecnt_t samplerate() const = 0;
	virtual void        seek (samplepos_t pos) = 0;
	virtual samplepos_t natural_position() const = 0;

	virtual bool clamped_at_unity () const = 0;
};

}

#endif /* __ardour_importable_source_h__ */
