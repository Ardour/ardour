/*
    Copyright (C) 2014 Paul Davis
    Written by: Robin Gareus <robin@gareus.org>

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

#ifndef __ardour_srcfilesource_h__
#define __ardour_srcfilesource_h__

#include <cstring>
#include <samplerate.h>
#include "ardour/audiofilesource.h"
#include "ardour/session.h"

namespace ARDOUR {

class SrcFileSource : public AudioFileSource {
public:
	SrcFileSource (Session&, boost::shared_ptr<AudioFileSource>, SrcQuality srcq = SrcQuality(SrcQuick));
	~SrcFileSource ();

	int update_header (framepos_t /*when*/, struct tm&, time_t) { return 0; }
	int flush_header () { return 0; }
	void set_header_timeline_position () {};
	void set_length (framecnt_t /*len*/) {};

	float sample_rate () const { return _session.nominal_frame_rate(); }

	framepos_t natural_position() const { return _source->natural_position() * _ratio;}
	framecnt_t readable_length() const { return _source->readable_length() * _ratio; }
	framecnt_t length (framepos_t pos) const { return _source->length(pos) * _ratio; }

	bool destructive() const { return false; }
	bool can_be_analysed() const { return false; }
	bool clamped_at_unity() const { return false; }

protected:
	framecnt_t read_unlocked (Sample *dst, framepos_t start, framecnt_t cnt) const;
	framecnt_t write_unlocked (Sample */*dst*/, framecnt_t /*cnt*/) { return 0; }

	int read_peaks_with_fpp (PeakData *peaks, framecnt_t npeaks, framepos_t /*start*/, framecnt_t /*cnt*/,
				 double /*samples_per_unit*/, framecnt_t /*fpp*/) const {
		memset (peaks, 0, sizeof (PeakData) * npeaks);
		return 0;
	}

private:
	static const uint32_t blocksize;
	boost::shared_ptr<AudioFileSource> _source;

	mutable SRC_STATE* _src_state;
	mutable SRC_DATA   _src_data;

	mutable Sample* _src_buffer;
	mutable framepos_t _source_position;
	mutable framepos_t _target_position;
	mutable double _fract_position;

	double _ratio;
	framecnt_t src_buffer_size;
};

} // namespace ARDOUR

#endif /* __ardour_audiofilesource_h__ */

