/*
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
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

#ifndef __ardour_sndfile_importable_source_h__
#define __ardour_sndfile_importable_source_h__

#include <boost/shared_ptr.hpp>
#include <sndfile.h>
#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "ardour/importable_source.h"

namespace ARDOUR {

class LIBARDOUR_API SndFileImportableSource : public ImportableSource {
public:
	SndFileImportableSource (const std::string& path);
	virtual ~SndFileImportableSource();

	samplecnt_t read (Sample* buffer, samplecnt_t nframes);
	uint32_t    channels() const;
	samplecnt_t length() const;
	samplecnt_t samplerate() const;
	void       seek (samplepos_t pos);
	bool       clamped_at_unity () const;
	samplepos_t natural_position () const;

protected:
	SF_INFO sf_info;
	boost::shared_ptr<SNDFILE> in;

	/* these are int64_t so as to be independent of whatever
	 * types Ardour may use for samplepos_t, samplecnt_t etc.
	 */
	int64_t timecode;
	int64_t get_timecode_info (SNDFILE*, SF_BROADCAST_INFO*, bool&);
};

}

#endif /* __ardour_sndfile_importable_source_h__ */
