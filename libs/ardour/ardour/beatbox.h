/*
    Copyright (C) 2017 Paul Davis

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

#ifndef __ardour_beatbox_h__
#define __ardour_beatbox_h__

#include <algorithm>
#include <vector>
#include <set>
#include <cstring>

#include <stdint.h>

#include "pbd/pool.h"
#include "pbd/ringbuffer.h"

#include "temporal/bbt_time.h"

#include "ardour/midi_state_tracker.h"
#include "ardour/processor.h"

namespace ARDOUR {

class Source;
class SMFSource;
class StepSequencer;

typedef uint64_t superclock_t;

static const superclock_t superclock_ticks_per_second = 508032000; // 2^10 * 3^4 * 5^3 * 7^2
inline superclock_t superclock_to_samples (superclock_t s, int sr) { return (s * sr) / superclock_ticks_per_second; }
inline superclock_t samples_to_superclock (int samples, int sr) { return (samples * superclock_ticks_per_second) / sr; }

class BeatBox : public ARDOUR::Processor {
  public:
	BeatBox (ARDOUR::Session& s);
	~BeatBox ();

	StepSequencer& sequencer() const { return *_sequencer; }

	void run (BufferSet& /*bufs*/, samplepos_t /*start_frame*/, samplepos_t /*end_frame*/, double speed, pframes_t /*nframes*/, bool /*result_required*/);
	void silence (samplecnt_t nframes, samplepos_t start_frame);
	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);

	XMLNode& state();
	XMLNode& get_state(void);

	bool fill_source (boost::shared_ptr<Source>);

  private:
	StepSequencer* _sequencer;

	ARDOUR::MidiNoteTracker inbound_tracker;

	bool fill_midi_source (boost::shared_ptr<SMFSource>);

};

} /* namespace */

#endif /* __ardour_beatbox_h__ */
