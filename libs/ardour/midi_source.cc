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

#include <glibmm/fileutils.h>

#include "pbd/xml++.h"
#include "pbd/pthread_utils.h"
#include "pbd/basename.h"

#include "ardour/audioengine.h"
#include "ardour/debug.h"
#include "ardour/midi_model.h"
#include "ardour/midi_ring_buffer.h"
#include "ardour/midi_state_tracker.h"
#include "ardour/midi_source.h"
#include "ardour/session.h"
#include "ardour/session_directory.h"
#include "ardour/source_factory.h"
#include "ardour/tempo.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

PBD::Signal1<void,MidiSource*> MidiSource::MidiSourceCreated;

MidiSource::MidiSource (Session& s, string name, Source::Flag flags)
	: Source(s, DataType::MIDI, name, flags)
	, _read_data_count(0)
	, _write_data_count(0)
	, _writing(false)
	, _model_iter_valid(false)
	, _length_beats(0.0)
	, _last_read_end(0)
	, _last_write_end(0)
{
}

MidiSource::MidiSource (Session& s, const XMLNode& node)
	: Source(s, node)
	, _read_data_count(0)
	, _write_data_count(0)
	, _writing(false)
	, _model_iter_valid(false)
	, _length_beats(0.0)
	, _last_read_end(0)
	, _last_write_end(0)
{
	_read_data_count = 0;
	_write_data_count = 0;

	if (set_state (node, Stateful::loading_state_version)) {
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
MidiSource::set_state (const XMLNode& node, int /*version*/)
{
	const XMLProperty* prop;

	if ((prop = node.property ("captured-for")) != 0) {
		_captured_for = prop->value();
	}

	return 0;
}

bool
MidiSource::empty () const
{
        return _length_beats == 0;
}

framecnt_t
MidiSource::length (framepos_t pos) const
{
        if (_length_beats == 0) {
                return 0;
        }

	BeatsFramesConverter converter(_session.tempo_map(), pos);
	return converter.to(_length_beats);
}

void
MidiSource::update_length (sframes_t /*pos*/, sframes_t /*cnt*/)
{
	// You're not the boss of me!
}

void
MidiSource::invalidate ()
{
	_model_iter_valid = false;
	_model_iter.invalidate();
}

/** @param filtered A set of parameters whose MIDI messages will not be returned */
nframes_t
MidiSource::midi_read (Evoral::EventSink<nframes_t>& dst, sframes_t source_start,
                       sframes_t start, nframes_t cnt,
                       sframes_t stamp_offset, sframes_t negative_stamp_offset,
                       MidiStateTracker* tracker,
		       std::set<Evoral::Parameter> const & filtered) const
{
	Glib::Mutex::Lock lm (_lock);

	BeatsFramesConverter converter(_session.tempo_map(), source_start);

	if (_model) {
		Evoral::Sequence<double>::const_iterator& i = _model_iter;

		// If the cached iterator is invalid, search for the first event past start
		if (_last_read_end == 0 || start != _last_read_end || !_model_iter_valid) {
			DEBUG_TRACE (DEBUG::MidiSourceIO, string_compose ("*** %1 search for relevant iterator for %1 / %2\n", _name, source_start, start));
			for (i = _model->begin(0, filtered); i != _model->end(); ++i) {
				if (converter.to(i->time()) >= start) {
					break;
				}
			}
			_model_iter_valid = true;
		} else {
			DEBUG_TRACE (DEBUG::MidiSourceIO, string_compose ("*** %1 use cached iterator for %1 / %2\n", _name, source_start, start));
		}

		_last_read_end = start + cnt;

		// Read events up to end
		for (; i != _model->end(); ++i) {
			const sframes_t time_frames = converter.to(i->time());
			if (time_frames < start + cnt) {
				dst.write(time_frames + stamp_offset - negative_stamp_offset,
						i->event_type(), i->size(), i->buffer());
				if (tracker) {
					Evoral::MIDIEvent<Evoral::MusicalTime>& ev (*(Evoral::MIDIEvent<Evoral::MusicalTime>*) (&(*i)));
					if (ev.is_note_on()) {
						DEBUG_TRACE (DEBUG::MidiSourceIO, string_compose ("\t%1 add note on %2 @ %3\n", _name, ev.note(), time_frames));
						tracker->add (ev.note(), ev.channel());
					} else if (ev.is_note_off()) {
						DEBUG_TRACE (DEBUG::MidiSourceIO, string_compose ("\t%1 add note off %2 @ %3\n", _name, ev.note(), time_frames));
						tracker->remove (ev.note(), ev.channel());
					}
				}
			} else {
				break;
			}
		}
		return cnt;
	} else {
		return read_unlocked (dst, source_start, start, cnt, stamp_offset, negative_stamp_offset, tracker);
	}
}

nframes_t
MidiSource::midi_write (MidiRingBuffer<nframes_t>& source, sframes_t source_start, nframes_t duration)
{
	Glib::Mutex::Lock lm (_lock);
	const nframes_t ret = write_unlocked (source, source_start, duration);
	_last_write_end += duration;
	return ret;
}

void
MidiSource::mark_streaming_midi_write_started (NoteMode mode, sframes_t start_frame)
{
	set_timeline_position(start_frame);

	if (_model) {
		_model->set_note_mode(mode);
		_model->start_write();
	}

	_last_write_end = start_frame;
	_writing = true;
}

void
MidiSource::mark_streaming_write_started ()
{
	NoteMode note_mode = _model ? _model->note_mode() : Sustained;
	mark_streaming_midi_write_started(note_mode, _session.transport_frame());
}

void
MidiSource::mark_streaming_write_completed ()
{
	if (_model) {
		_model->end_write(false);
	}

	_writing = false;
}

boost::shared_ptr<MidiSource>
MidiSource::clone (Evoral::MusicalTime begin, Evoral::MusicalTime end)
{
        string newname = PBD::basename_nosuffix(_name.val());
        string newpath;

        /* get a new name for the MIDI file we're going to write to
         */

        do { 

                newname = bump_name_once (newname, '-');
                /* XXX build path safely */
                newpath = _session.session_directory().midi_path().to_string() +"/"+ newname + ".mid";

        } while (Glib::file_test (newpath, Glib::FILE_TEST_EXISTS));
        
        boost::shared_ptr<MidiSource> newsrc = boost::dynamic_pointer_cast<MidiSource>(
                SourceFactory::createWritable(DataType::MIDI, _session,
                                              newpath, false, _session.frame_rate()));
        
        newsrc->set_timeline_position(_timeline_position);

        if (_model) {
                if (begin == Evoral::MinMusicalTime && end == Evoral::MaxMusicalTime) {
                        _model->write_to (newsrc);
                } else {
                        _model->write_section_to (newsrc, begin, end);
                }
        } else {
                error << string_compose (_("programming error: %1"), X_("no model for MidiSource during ::clone()"));
                return boost::shared_ptr<MidiSource>();
        }

        newsrc->flush_midi();

        /* force a reload of the model if the range is partial */
        
        if (begin != Evoral::MinMusicalTime || end != Evoral::MaxMusicalTime) {
                newsrc->load_model (true, true);
        } else {
		newsrc->set_model (_model);
	}
        
        return newsrc;
}

void
MidiSource::session_saved()
{
        /* this writes a copy of the data to disk. 
           XXX do we need to do this every time?
        */

	flush_midi();

	if (_model && _model->edited()) {
#if 0 // old style: clone the source if necessary on every session save
      // and switch to the new source
		boost::shared_ptr<MidiSource> newsrc = clone ();

                if (newsrc) {
                        _model->set_midi_source (newsrc);
                        Switched (newsrc); /* EMIT SIGNAL */
                }
#else
                // new style: if the model is edited, write its contents into
                // the current source file (overwiting previous contents.

                /* temporarily drop our reference to the model so that
                   as the model pushes its current state to us, we don't
                   try to update it.
                */

                boost::shared_ptr<MidiModel> mm = _model ;
                _model.reset ();   
                mm->sync_to_source ();
                _model = mm;
                /* data is in the file now, its not removable */
#endif
        }

        cerr << name() << " @ " << this << " length at save = " << _length_beats << endl;
}

void
MidiSource::set_note_mode(NoteMode mode)
{
	if (_model) {
		_model->set_note_mode(mode);
	}
}

void
MidiSource::drop_model ()
{
        _model.reset(); 
	ModelChanged (); /* EMIT SIGNAL */
}

void
MidiSource::set_model (boost::shared_ptr<MidiModel> m)
{
	_model = m;
	ModelChanged (); /* EMIT SIGNAL */
}
