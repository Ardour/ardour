/*
    Copyright (C) 2006 Paul Davis
	Written by Dave Robillard, 2006

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
class SMFSource : public MidiSource, public FileSource, public Evoral::SMF {
public:
	/** Constructor for existing external-to-session files */
	SMFSource (Session& session, const Glib::ustring& path, bool embedded,
			Source::Flag flags = Source::Flag(0));

	/** Constructor for existing in-session files */
	SMFSource (Session& session, const XMLNode&, bool must_exist = false);

	virtual ~SMFSource ();
	
	bool safe_file_extension (const Glib::ustring& path) const {
		return safe_midi_file_extension(path);
	}

	bool set_name (const std::string& newname) { return (set_source_name(newname, false) == 0); }
	
	void append_event_unlocked_beats (const Evoral::Event<double>& ev);
	void append_event_unlocked_frames (const Evoral::Event<nframes_t>& ev, sframes_t position);

	void mark_streaming_midi_write_started (NoteMode mode, sframes_t start_time);
	void mark_streaming_write_completed ();

	XMLNode& get_state ();
	int set_state (const XMLNode&);

	void load_model (bool lock=true, bool force_reload=false);
	void destroy_model ();

	void flush_midi ();
	
	static void set_header_position_offset (nframes_t offset, bool negative);

	static bool safe_midi_file_extension (const Glib::ustring& path);

private:
	nframes_t read_unlocked (MidiRingBuffer<nframes_t>& dst,
			sframes_t position,
			sframes_t start,
			nframes_t cnt,
			sframes_t stamp_offset,
			sframes_t negative_stamp_offset) const;

	nframes_t write_unlocked (MidiRingBuffer<nframes_t>& src,
			sframes_t position,
			nframes_t cnt);

	void set_default_controls_interpolation ();

	double    _last_ev_time_beats;
	sframes_t _last_ev_time_frames;
};

}; /* namespace ARDOUR */

#endif /* __ardour_smf_source_h__ */

