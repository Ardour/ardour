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

#ifndef __ardour_midi_pattern_source_h__
#define __ardour_midi_pattern_source_h__

#include "ardour/midi_source.h"
#include "ardour/pattern_source.h"

namespace ARDOUR {

/** Source for MIDI patterns */
class LIBARDOUR_API MidiPatternSource : public MidiSource, public PatternSource
{
  public:
	typedef Evoral::Beats TimeType;

	MidiPatternSource (Session& session, std::string const & name);
	MidiPatternSource (Session& session, const XMLNode&);
	virtual ~MidiPatternSource ();

	/* PatternSources are modified using the PatternSource API, not the
	   MidiSource one, so these are no-ops.
	*/
	void append_event_beats(const Lock&                         lock,
	                        boost::shared_ptr<Evoral::Event<Evoral::Beats> > const & ev) {}
	void append_event_frames(const Lock&                      lock,
	                         boost::shared_ptr<Evoral::Event<framepos_t> > const & ev,
	                         framepos_t                       source_start) {}
	void mark_streaming_midi_write_started (const Lock& lock, NoteMode mode) {}
	void mark_streaming_write_started (const Lock& lock) {}
	void mark_streaming_write_completed (const Lock& lock) {}
	void mark_write_starting_now (framecnt_t position,
	                              framecnt_t capture_length,
	                              framecnt_t loop_length) {}
	void mark_midi_streaming_write_completed (
		const Lock&                                      lock,
		Evoral::Sequence<Evoral::Beats>::StuckNoteOption stuck_option,
		Evoral::Beats                                    when = Evoral::Beats()) {}

	bool       empty () const;
	framecnt_t length (framepos_t pos) const;
	void       update_length (framecnt_t);

	void session_saved() {}

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	bool length_mutable() const { return true; }

	void     set_length_beats(TimeType l) { _length_beats = l; }
	TimeType length_beats() const         { return _length_beats; }

	virtual void load_model(const Glib::Threads::Mutex::Lock& lock, bool force_reload=false) = 0;
	virtual void destroy_model(const Glib::Threads::Mutex::Lock& lock) = 0;

  protected:
	void flush_midi(const Lock& lock) {}

	framecnt_t read_unlocked (const Lock&                    lock,
	                          Evoral::EventSink<framepos_t>& dst,
	                          framepos_t                     position,
	                          framepos_t                     start,
	                          framecnt_t                     cnt,
	                          Evoral::Range<framepos_t>*     loop_range,
	                          MidiStateTracker*              tracker,
	                          MidiChannelFilter*             filter) const;

	framecnt_t write_unlocked (const Lock&                 lock,
	                           MidiRingBuffer<framepos_t>& source,
	                           framepos_t                  position,
	                           framecnt_t                  cnt);
};

}

#endif /* __ardour_midi_source_h__ */
