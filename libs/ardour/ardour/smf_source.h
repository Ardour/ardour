/*
    Copyright (C) 2006 Paul Davis
    Author: David Robillard

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

#ifndef __ardour_smf_source_h__
#define __ardour_smf_source_h__

#include <cstdio>
#include <time.h>
#include "evoral/SMF.hpp"
#include "ardour/midi_source.h"
#include "ardour/file_source.h"

namespace Evoral { template<typename T> class Event; }

namespace ARDOUR {

template<typename T> class MidiRingBuffer;

/** Standard Midi File (Type 0) Source */
class LIBARDOUR_API SMFSource : public MidiSource, public FileSource, public Evoral::SMF {
public:
	/** Constructor for new internal-to-session files */
	SMFSource (Session& session, const std::string& path, Source::Flag flags);

	/** Constructor for existing external-to-session files */
	SMFSource (Session& session, const std::string& path);

	/** Constructor for existing in-session files */
	SMFSource (Session& session, const XMLNode&, bool must_exist = false);

	virtual ~SMFSource ();

	bool safe_file_extension (const std::string& path) const {
		return safe_midi_file_extension(path);
	}

	void append_event_beats (const Lock& lock, const Evoral::Event<Temporal::Beats>& ev);
	void append_event_samples (const Lock& lock, const Evoral::Event<samplepos_t>& ev, samplepos_t source_start);

	void mark_streaming_midi_write_started (const Lock& lock, NoteMode mode);
	void mark_streaming_write_completed (const Lock& lock);
	void mark_midi_streaming_write_completed (const Lock& lock,
	                                          Evoral::Sequence<Temporal::Beats>::StuckNoteOption,
	                                          Temporal::Beats when = Temporal::Beats());

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	void load_model (const Glib::Threads::Mutex::Lock& lock, bool force_reload=false);
	void destroy_model (const Glib::Threads::Mutex::Lock& lock);

	static bool safe_midi_file_extension (const std::string& path);
	static bool valid_midi_file (const std::string& path);

	void prevent_deletion ();
	void set_path (const std::string& newpath);

  protected:
	void close ();
	void flush_midi (const Lock& lock);

  private:
	bool _open;
	Temporal::Beats     _last_ev_time_beats;
	samplepos_t          _last_ev_time_samples;
	/** end time (start + duration) of last call to read_unlocked */
	mutable samplepos_t _smf_last_read_end;
	/** time (in SMF ticks, 1 tick per _ppqn) of the last event read by read_unlocked */
	mutable samplepos_t _smf_last_read_time;

	int open_for_write ();

	void ensure_disk_file (const Lock& lock);

	samplecnt_t read_unlocked (const Lock&                    lock,
	                          Evoral::EventSink<samplepos_t>& dst,
	                          samplepos_t                     position,
	                          samplepos_t                     start,
	                          samplecnt_t                     cnt,
	                          Evoral::Range<samplepos_t>*     loop_range,
	                          MidiStateTracker*              tracker,
	                          MidiChannelFilter*             filter) const;

	samplecnt_t write_unlocked (const Lock&                 lock,
	                           MidiRingBuffer<samplepos_t>& src,
	                           samplepos_t                  position,
	                           samplecnt_t                  cnt);

};

}; /* namespace ARDOUR */

#endif /* __ardour_smf_source_h__ */

