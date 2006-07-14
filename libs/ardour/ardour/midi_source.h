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

	/* returns the number of items in this `midi_source' */

	// Applicable to MIDI?  With what unit? [DR]
	virtual jack_nframes_t length() const {
		return _length;
	}

	virtual jack_nframes_t read (unsigned char *dst, jack_nframes_t start, jack_nframes_t cnt, char * workbuf) const;
	virtual jack_nframes_t write (unsigned char *src, jack_nframes_t cnt, char * workbuf);

	virtual void mark_for_remove() = 0;
	virtual void mark_streaming_write_completed () {}

	void set_captured_for (string str) { _captured_for = str; }
	string captured_for() const { return _captured_for; }

	uint32_t read_data_count() const { return _read_data_count; }
	uint32_t write_data_count() const { return _write_data_count; }

	static sigc::signal<void,MidiSource*> MidiSourceCreated;
	       
	mutable sigc::signal<void>  PeaksReady;
	mutable sigc::signal<void,jack_nframes_t,jack_nframes_t>  PeakRangeReady;
	
	XMLNode& get_state ();
	int set_state (const XMLNode&);

  protected:
	jack_nframes_t   _length;
	string           _captured_for;

	mutable uint32_t _read_data_count;  // modified in read()
	mutable uint32_t _write_data_count; // modified in write()

	virtual jack_nframes_t read_unlocked (unsigned char *dst, jack_nframes_t start, jack_nframes_t cnt, char * workbuf) const = 0;
	virtual jack_nframes_t write_unlocked (unsigned char *dst, jack_nframes_t cnt, char * workbuf) = 0;
	
	void update_length (jack_nframes_t pos, jack_nframes_t cnt);

  private:
	bool file_changed (string path);
};

}

#endif /* __ardour_midi_source_h__ */
