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

#ifndef __ardour_resampled_source_h__
#define __ardour_resampled_source_h__

#include <samplerate.h>

#include "ardour/types.h"
#include "ardour/importable_source.h"

namespace ARDOUR {

class ResampledImportableSource : public ImportableSource
{
  public:
	ResampledImportableSource (boost::shared_ptr<ImportableSource>, nframes_t rate, SrcQuality);

	~ResampledImportableSource ();

	nframes_t read (Sample* buffer, nframes_t nframes);
	float ratio() const { return src_data.src_ratio; }
	uint32_t channels() const { return source->channels(); }
	nframes_t length() const { return source->length(); }
	nframes_t samplerate() const { return source->samplerate(); }
	void      seek (nframes_t pos) { source->seek (pos); }
	nframes64_t natural_position() const { return source->natural_position(); }

	static const uint32_t blocksize;

   private:
	boost::shared_ptr<ImportableSource> source;
        float* input;
	SRC_STATE*	src_state;
	SRC_DATA	src_data;
};

}

#endif /* __ardour_resampled_source_h__ */
