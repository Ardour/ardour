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

#ifndef __ardour_ca_importable_source_h__
#define __ardour_ca_importable_source_h__

#include "pbd/failed_constructor.h"
#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "ardour/importable_source.h"

#include <appleutility/CAAudioFile.h>

namespace ARDOUR {

class LIBARDOUR_API CAImportableSource : public ImportableSource {
    public:
	CAImportableSource (const std::string& path);
	virtual ~CAImportableSource();

	framecnt_t read (Sample* buffer, framecnt_t nframes);
	uint32_t  channels() const;
	framecnt_t length() const;
	framecnt_t samplerate() const;
	void      seek (framepos_t pos);
	framepos_t natural_position() const { return 0; }
	bool clamped_at_unity () const { return false; }

   protected:
	mutable CAAudioFile af;
};

}

#endif /* __ardour_ca_importable_source_h__ */
