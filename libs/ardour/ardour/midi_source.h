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

#ifndef __ardour_midi_source_h__
#define __ardour_midi_source_h__

#include <string>

#include <time.h>

#include <glibmm/thread.h>

#include <sigc++/signal.h>

#include <ardour/source.h>
#include <ardour/ardour.h>
#include <ardour/buffer.h>
#include <ardour/midi_model.h>
#include <pbd/stateful.h>
#include <pbd/xml++.h>

using std::string;

namespace ARDOUR {

class MidiRingBuffer;

/** Source for MIDI data */
class MidiSource : public Source
{
  public:
	MidiSource (Session& session, string name);
	MidiSource (Session& session, const XMLNode&);
	virtual ~MidiSource ();
	
	/* Stub Readable interface */
	virtual nframes64_t read (Sample*, nframes64_t pos, nframes64_t cnt, int channel) const { return 0; }
	virtual nframes64_t readable_length() const { return length(); }
	virtual uint32_t    n_channels () const { return 1; }
	
	// FIXME: integrate this with the Readable::read interface somehow
	virtual nframes_t midi_read (MidiRingBuffer& dst, nframes_t start, nframes_t cnt, nframes_t stamp_offset) const;
	virtual nframes_t midi_write (MidiRingBuffer& src, nframes_t cnt);

	virtual void append_event_unlocked(const MidiEvent& ev) = 0;

	virtual void mark_for_remove() = 0;
	virtual void mark_streaming_midi_write_started (NoteMode mode, nframes_t start_time);
	virtual void mark_streaming_write_started ();
	virtual void mark_streaming_write_completed ();
	
	uint64_t timeline_position ()                   { return _timeline_position; }
	void     set_timeline_position (nframes_t when) { _timeline_position = when; }
	
	virtual void session_saved();

	string captured_for() const { return _captured_for; }
	void   set_captured_for (string str) { _captured_for = str; }

	uint32_t read_data_count()  const { return _read_data_count; }
	uint32_t write_data_count() const { return _write_data_count; }

	static sigc::signal<void,MidiSource*> MidiSourceCreated;
	       
	// Signal a range of recorded data is available for reading from model()
	mutable sigc::signal<void,nframes_t,nframes_t> ViewDataRangeReady;
	
	XMLNode& get_state ();
	int set_state (const XMLNode&);
	
	bool length_mutable() const { return true; }

	virtual void load_model(bool lock=true, bool force_reload=false) = 0;
	virtual void destroy_model() = 0;

	void set_note_mode(NoteMode mode) { if (_model) _model->set_note_mode(mode); }

	boost::shared_ptr<MidiModel> model() { return _model; }
	void set_model(boost::shared_ptr<MidiModel> m) { _model = m; }

  protected:
	virtual int flush_header() = 0;
	virtual int flush_footer() = 0;
	
	virtual nframes_t read_unlocked (MidiRingBuffer& dst, nframes_t start, nframes_t cnt, nframes_t stamp_offset) const = 0;
	virtual nframes_t write_unlocked (MidiRingBuffer& dst, nframes_t cnt) = 0;
	
	mutable Glib::Mutex _lock;
	string              _captured_for;
	uint64_t            _timeline_position;
	mutable uint32_t    _read_data_count;  ///< modified in read()
	mutable uint32_t    _write_data_count; ///< modified in write()

	boost::shared_ptr<MidiModel> _model;
	bool                         _writing;

  private:
	bool file_changed (string path);
};

}

#endif /* __ardour_midi_source_h__ */
