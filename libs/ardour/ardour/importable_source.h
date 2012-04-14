/*
    Copyright (C) 2007 Paul Davis

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

#ifndef __ardour_importable_source_h__
#define __ardour_importable_source_h__

#include "pbd/failed_constructor.h"
#include "ardour/types.h"

namespace ARDOUR {

class ImportableSource {
public:
	ImportableSource () {}
	virtual ~ImportableSource() {}

	virtual framecnt_t read (Sample* buffer, framecnt_t nframes) = 0;
	virtual float      ratio() const { return 1.0f; }
	virtual uint32_t   channels() const = 0;
	virtual framecnt_t length() const = 0;
	virtual framecnt_t samplerate() const = 0;
	virtual void       seek (framepos_t pos) = 0;
	virtual framepos_t natural_position() const = 0;

	virtual bool clamped_at_unity () const = 0;
};

}

#endif /* __ardour_importable_source_h__ */
