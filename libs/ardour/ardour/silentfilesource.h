/*
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2008-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2016-2018 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_silentfilesource_h__
#define __ardour_silentfilesource_h__

#include <cstring>
#include "ardour/audiofilesource.h"

namespace ARDOUR {

class LIBARDOUR_API SilentFileSource : public AudioFileSource {
public:
	int update_header (samplepos_t /*when*/, struct tm&, time_t) { return 0; }
	int flush_header () { return 0; }
	float sample_rate () const { return _sample_rate; }

	void set_length (samplecnt_t len) { _length = timecnt_t (len); }
	void flush () {}

	bool can_be_analysed() const { return false; }

	bool clamped_at_unity() const { return false; }

protected:
	void close() {}
	friend class SourceFactory;

	SilentFileSource (Session& s, const XMLNode& x, samplecnt_t len, float srate)
		: Source (s, x)
		, AudioFileSource (s, x, false)
		, _sample_rate(srate)
	{
		_length = timecnt_t (len);
	}

	samplecnt_t read_unlocked (Sample *dst, samplepos_t start, samplecnt_t cnt) const {
		cnt = std::min (cnt, std::max<samplecnt_t> (0, _length.samples() - start));
		memset (dst, 0, sizeof (Sample) * cnt);
		return cnt;
	}

	samplecnt_t write_unlocked (Sample */*dst*/, samplecnt_t /*cnt*/) { return 0; }

	void set_header_natural_position () {}

	int read_peaks_with_fpp (PeakData *peaks, samplecnt_t npeaks, samplepos_t /*start*/, samplecnt_t /*cnt*/,
				 double /*samples_per_pixel*/, samplecnt_t /*fpp*/) const {
		memset (peaks, 0, sizeof (PeakData) * npeaks);
		return 0;
	}

	float _sample_rate;
};

} // namespace ARDOUR

#endif /* __ardour_audiofilesource_h__ */

