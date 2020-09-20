/*
 * Copyright (C) 2014-2015 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_srcfilesource_h__
#define __ardour_srcfilesource_h__

#include <cstring>
#include <samplerate.h>

#include "ardour/libardour_visibility.h"
#include "ardour/audiofilesource.h"
#include "ardour/session.h"

namespace ARDOUR {

class LIBARDOUR_API SrcFileSource : public AudioFileSource {
public:
	SrcFileSource (Session&, boost::shared_ptr<AudioFileSource>, SrcQuality srcq = SrcQuality(SrcQuick));
	~SrcFileSource ();

	int  update_header (samplepos_t /*when*/, struct tm&, time_t) { return 0; }
	int  flush_header () { return 0; }
	void flush () { }
	void set_header_natural_position () {};
	void set_length (samplecnt_t /*len*/) {};

	float sample_rate () const { return _session.nominal_sample_rate(); }

	timepos_t natural_position() const { return _source->natural_position() * _ratio;}
	samplecnt_t readable_length_samples() const { return _source->length_samples (timepos_t (Temporal::AudioTime)) * _ratio; }
	samplecnt_t length (samplepos_t /*pos*/) const { return _source->length_samples (timepos_t (Temporal::AudioTime)) * _ratio; }

	bool can_be_analysed() const { return false; }
	bool clamped_at_unity() const { return false; }

protected:
	void close ();
	samplecnt_t read_unlocked (Sample *dst, samplepos_t start, samplecnt_t cnt) const;
	samplecnt_t write_unlocked (Sample */*dst*/, samplecnt_t /*cnt*/) { return 0; }

	int read_peaks_with_fpp (PeakData *peaks, samplecnt_t npeaks, samplepos_t /*start*/, samplecnt_t /*cnt*/,
				 double /*samples_per_unit*/, samplecnt_t /*fpp*/) const {
		memset (peaks, 0, sizeof (PeakData) * npeaks);
		return 0;
	}

private:
	static const uint32_t max_blocksize;
	boost::shared_ptr<AudioFileSource> _source;

	mutable SRC_STATE* _src_state;
	mutable SRC_DATA   _src_data;

	mutable Sample* _src_buffer;
	mutable samplepos_t _source_position;
	mutable samplepos_t _target_position;
	mutable double _fract_position;

	double _ratio;
	samplecnt_t src_buffer_size;
};

} // namespace ARDOUR

#endif /* __ardour_audiofilesource_h__ */

