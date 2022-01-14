/*
 * Copyright (C) 2021 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __libardour_segment_descriptor_h__
#define __libardour_segment_descriptor_h__

#include "temporal/timeline.h"
#include "temporal/tempo.h"

class XMLNode;

namespace ARDOUR {

/* An object that describes an extent (duration & position), along with a
 * potentially expanding set of metadata about that extent (e.g. bpm, meter
 * etc.)
 */

class SegmentDescriptor {
public:
	SegmentDescriptor ();
	SegmentDescriptor (XMLNode const &, int version);

	/* This object does not use the tempo map to convert between time
	 * domains, since it describes things that are not (always) on the
	 * timeline.
	 */

	Temporal::TimeDomain time_domain() const { return _time_domain; }

	void set_position (Temporal::samplepos_t);
	void set_position (Temporal::Beats const &);

	void set_duration (Temporal::samplecnt_t);
	void set_duration (Temporal::Beats const &);

	void set_extent (Temporal::samplepos_t pos, Temporal::samplecnt_t dur);
	void set_extent (Temporal::Beats const & pos, Temporal::Beats const & dur);

	Temporal::timecnt_t extent() const;
	Temporal::timepos_t position() const { return extent().position(); }

	Temporal::Tempo tempo() const { return _tempo; }
	void set_tempo (Temporal::Tempo const&);

	Temporal::Meter meter() const { return _meter; }
	void set_meter (Temporal::Meter const&);

	/* Replicate the API of PBD::Stateful without the overhead */

	XMLNode& get_state (void) const;
	int set_state (const XMLNode&, int version);

private:
	Temporal::TimeDomain _time_domain;

	/* cannot use a union for these because Temporal::Beats has a
	   "non-trivial" constructor.
	*/

	Temporal::samplepos_t _position_samples;
	Temporal::Beats       _position_beats;
	Temporal::samplepos_t _duration_samples;
	Temporal::Beats       _duration_beats;

	Temporal::Tempo _tempo;
	Temporal::Meter _meter;
};

}

#endif /* __libardour_segment_descriptor_h__ */
