/*
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
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

#ifndef __ardour_ca_importable_source_h__
#define __ardour_ca_importable_source_h__

#include "pbd/failed_constructor.h"
#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "ardour/importable_source.h"

#ifdef COREAUDIO105
#include "CAAudioFile.h"
#else
#include "CAExtAudioFile.h"
#endif

namespace ARDOUR {

class LIBARDOUR_API CAImportableSource : public ImportableSource {
    public:
	CAImportableSource (const std::string& path);
	virtual ~CAImportableSource();

	samplecnt_t read (Sample* buffer, samplecnt_t nframes);
	uint32_t    channels() const;
	samplecnt_t length() const;
	samplecnt_t samplerate() const;
	void        seek (samplepos_t pos);
	samplepos_t natural_position () const;
	bool        clamped_at_unity () const { return false; }

   protected:
#ifdef COREAUDIO105
	mutable CAAudioFile af;
#else
	mutable CAExtAudioFile af;
#endif
};

}

#endif /* __ardour_ca_importable_source_h__ */
