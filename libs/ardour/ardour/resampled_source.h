/*
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_resampled_source_h__
#define __ardour_resampled_source_h__

#include <samplerate.h>

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "ardour/importable_source.h"

namespace ARDOUR {

class LIBARDOUR_API ResampledImportableSource : public ImportableSource
{
  public:
	ResampledImportableSource (boost::shared_ptr<ImportableSource>, samplecnt_t rate, SrcQuality);

	~ResampledImportableSource ();

	samplecnt_t read (Sample* buffer, samplecnt_t nframes);
	float       ratio() const { return _src_data.src_ratio; }
	uint32_t    channels() const { return source->channels(); }
	samplecnt_t length() const { return source->length(); }
	samplecnt_t samplerate() const { return source->samplerate(); }
	void        seek (samplepos_t);
	samplepos_t natural_position() const;

	bool clamped_at_unity () const {
		/* resampling may generate inter-sample peaks with magnitude > 1 */
		return false;
	}

	static const uint32_t blocksize;

   private:
	boost::shared_ptr<ImportableSource> source;
	float*          _input;
	int             _src_type;
	SRC_STATE*      _src_state;
	SRC_DATA        _src_data;
	bool            _end_of_input;
};

}

#endif /* __ardour_resampled_source_h__ */
