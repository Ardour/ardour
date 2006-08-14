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
#include <pbd/stateful.h>
#include <pbd/xml++.h>

using std::string;

namespace ARDOUR {

/** Source for raw MIDI data */
class MidiSource : public Source
{
  public:
	MidiSource (string name);
	MidiSource (const XMLNode&);
	virtual ~MidiSource ();

	virtual jack_nframes_t read (RawMidi *dst, jack_nframes_t start, jack_nframes_t cnt) const;
	virtual jack_nframes_t write (RawMidi *src, jack_nframes_t cnt);

	virtual void mark_for_remove() = 0;
	virtual void mark_streaming_write_completed () {}

	string captured_for() const { return _captured_for; }
	void   set_captured_for (string str) { _captured_for = str; }

	uint32_t read_data_count()  const { return _read_data_count; }
	uint32_t write_data_count() const { return _write_data_count; }

	static sigc::signal<void,MidiSource*> MidiSourceCreated;
	       
	// The MIDI equivalent to "peaks"
	static int  start_view_data_thread ();
	static void stop_view_data_thread ();
	mutable sigc::signal<void,jack_nframes_t,jack_nframes_t> ViewDataRangeReady;
	
	XMLNode& get_state ();
	int set_state (const XMLNode&);

  protected:
	virtual jack_nframes_t read_unlocked (RawMidi* dst, jack_nframes_t start, jack_nframes_t cn) const = 0;
	virtual jack_nframes_t write_unlocked (RawMidi* dst, jack_nframes_t cnt) = 0;
	
	mutable Glib::Mutex _lock;
	string              _captured_for;
	mutable uint32_t    _read_data_count;  ///< modified in read()
	mutable uint32_t    _write_data_count; ///< modified in write()

  private:
	bool file_changed (string path);
};

}

#endif /* __ardour_midi_source_h__ */
