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

#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <float.h>
#include <cerrno>
#include <ctime>
#include <cmath>
#include <iomanip>
#include <algorithm>

#include <pbd/xml++.h>
#include <pbd/pthread_utils.h>
#include <pbd/basename.h>

#include <ardour/audioengine.h>
#include <ardour/midi_model.h>
#include <ardour/midi_ring_buffer.h>
#include <ardour/midi_source.h>
#include <ardour/session.h>
#include <ardour/session_directory.h>
#include <ardour/source_factory.h>
#include <ardour/tempo.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

sigc::signal<void,MidiSource *> MidiSource::MidiSourceCreated;

MidiSource::MidiSource (Session& s, string name, Source::Flag flags)
	: Source (s, name, DataType::MIDI, flags)
	, _timeline_position(0)
	, _read_data_count(0)
	, _write_data_count(0)
	, _converter(s, _timeline_position)
	, _writing (false)
	, _last_read_end(0)
{
}

MidiSource::MidiSource (Session& s, const XMLNode& node) 
	: Source (s, node)
	, _timeline_position(0)
	, _read_data_count(0)
	, _write_data_count(0)
	, _converter(s, _timeline_position)
	, _writing (false)
	, _last_read_end(0)
{
	_read_data_count = 0;
	_write_data_count = 0;

	if (set_state (node)) {
		throw failed_constructor();
	}
}

MidiSource::~MidiSource ()
{
}

XMLNode&
MidiSource::get_state ()
{
	XMLNode& node (Source::get_state());

	if (_captured_for.length()) {
		node.add_property ("captured-for", _captured_for);
	}

	return node;
}

int
MidiSource::set_state (const XMLNode& node)
{
	const XMLProperty* prop;

	Source::set_state (node);

	if ((prop = node.property ("captured-for")) != 0) {
		_captured_for = prop->value();
	}

	return 0;
}

nframes_t
MidiSource::midi_read (MidiRingBuffer<nframes_t>& dst, nframes_t start, nframes_t cnt,
		nframes_t stamp_offset, nframes_t negative_stamp_offset) const
{

	Glib::Mutex::Lock lm (_lock);
	if (_model) {
#define BEATS_TO_FRAMES(t) (_converter.to(t) + stamp_offset - negative_stamp_offset)

		Evoral::Sequence<double>::const_iterator& i = _model_iter;
		
		if (_last_read_end == 0 || start != _last_read_end) {
			cerr << "MidiSource::midi_read seeking to frame " << start << endl;
			for (i = _model->begin(); i != _model->end(); ++i) {
				if (BEATS_TO_FRAMES(i->time()) >= start) {
					break;
				}
			}
		}
		
		_last_read_end = start + cnt;

		for (; i != _model->end(); ++i) {
			const nframes_t time_frames = BEATS_TO_FRAMES(i->time());
			if (time_frames < start + cnt) {
				dst.write(time_frames, i->event_type(), i->size(), i->buffer());
			} else {
				break;
			}
		}
		return cnt;
	} else {
		return read_unlocked (dst, start, cnt, stamp_offset, negative_stamp_offset);
	}
}

nframes_t
MidiSource::midi_write (MidiRingBuffer<nframes_t>& dst, nframes_t cnt)
{
	Glib::Mutex::Lock lm (_lock);
	return write_unlocked (dst, cnt);
}

bool
MidiSource::file_changed (string path)
{
	struct stat stat_file;

	int e1 = stat (path.c_str(), &stat_file);
	
	return !e1;
}

void
MidiSource::set_timeline_position (nframes_t when)
{
	_timeline_position = when;
	_converter.set_origin(when);
}

void
MidiSource::mark_streaming_midi_write_started (NoteMode mode, nframes_t start_frame)
{
	set_timeline_position(start_frame); // why do I have a feeling this can break somehow...

	if (_model) {
		_model->set_note_mode(mode);
		_model->start_write();
	}
	
	_writing = true;
}

void
MidiSource::mark_streaming_write_started ()
{
	if (_model) {
		_model->start_write();
	}

	_writing = true;
}

void
MidiSource::mark_streaming_write_completed ()
{
	if (_model) {
		_model->end_write(false); // FIXME: param?
	}

	_writing = false;
}

void
MidiSource::session_saved()
{
	flush_midi();

	if (_model && _model->edited()) {
		string newname;
		const string basename = PBD::basename_nosuffix(_name);
		string::size_type last_dash = basename.find_last_of("-");
		if (last_dash == string::npos || last_dash == basename.find_first_of("-")) {
			newname = basename + "-1";
		} else {
			stringstream ss(basename.substr(last_dash+1));
			unsigned write_count = 0;
			ss >> write_count;
			cerr << "WRITE COUNT: " << write_count << endl;
			++write_count; // start at 1
			ss.clear();
			ss << basename.substr(0, last_dash) << "-" << write_count;
			newname = ss.str();
		}

		string newpath = _session.session_directory().midi_path().to_string() +"/"+ newname + ".mid";

		boost::shared_ptr<MidiSource> newsrc = boost::dynamic_pointer_cast<MidiSource>(
				SourceFactory::createWritable(DataType::MIDI, _session, newpath, 1, 0, true));

		newsrc->set_timeline_position(_timeline_position);
		_model->write_to(newsrc);

		// cyclic dependency here, ugly :(
		newsrc->set_model(_model);
		_model->set_midi_source(newsrc.get());
		
		newsrc->flush_midi();

		Switched.emit(newsrc);
	}
}

void
MidiSource::set_note_mode(NoteMode mode)
{
	if (_model) {
		_model->set_note_mode(mode);
	}
}

