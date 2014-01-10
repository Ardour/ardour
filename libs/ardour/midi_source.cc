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

#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <float.h>
#include <cerrno>
#include <ctime>
#include <cmath>
#include <iomanip>
#include <algorithm>

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/xml++.h"
#include "pbd/pthread_utils.h"
#include "pbd/basename.h"

#include "ardour/debug.h"
#include "ardour/midi_model.h"
#include "ardour/midi_state_tracker.h"
#include "ardour/midi_source.h"
#include "ardour/session.h"
#include "ardour/session_directory.h"
#include "ardour/source_factory.h"

#include "i18n.h"

namespace ARDOUR { template <typename T> class MidiRingBuffer; }

using namespace std;
using namespace ARDOUR;
using namespace PBD;

PBD::Signal1<void,MidiSource*> MidiSource::MidiSourceCreated;

MidiSource::MidiSource (Session& s, string name, Source::Flag flags)
	: Source(s, DataType::MIDI, name, flags)
	, _writing(false)
	, _model_iter_valid(false)
	, _length_beats(0.0)
	, _last_read_end(0)
	, _capture_length(0)
	, _capture_loop_length(0)
{
}

MidiSource::MidiSource (Session& s, const XMLNode& node)
	: Source(s, node)
	, _writing(false)
	, _model_iter_valid(false)
	, _length_beats(0.0)
	, _last_read_end(0)
	, _capture_length(0)
	, _capture_loop_length(0)
{
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

	for (InterpolationStyleMap::const_iterator i = _interpolation_style.begin(); i != _interpolation_style.end(); ++i) {
		XMLNode* child = node.add_child (X_("InterpolationStyle"));
		child->add_property (X_("parameter"), EventTypeMap::instance().to_symbol (i->first));
		child->add_property (X_("style"), enum_2_string (i->second));
	}

	for (AutomationStateMap::const_iterator i = _automation_state.begin(); i != _automation_state.end(); ++i) {
		XMLNode* child = node.add_child (X_("AutomationState"));
		child->add_property (X_("parameter"), EventTypeMap::instance().to_symbol (i->first));
		child->add_property (X_("state"), enum_2_string (i->second));
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

	XMLNodeList children = node.children ();
	for (XMLNodeConstIterator i = children.begin(); i != children.end(); ++i) {
		if ((*i)->name() == X_("InterpolationStyle")) {
			XMLProperty* prop;

			if ((prop = (*i)->property (X_("parameter"))) == 0) {
				error << _("Missing parameter property on InterpolationStyle") << endmsg;
				return -1;
			}

			Evoral::Parameter p = EventTypeMap::instance().new_parameter (prop->value());

			if ((prop = (*i)->property (X_("style"))) == 0) {
				error << _("Missing style property on InterpolationStyle") << endmsg;
				return -1;
			}

			Evoral::ControlList::InterpolationStyle s = static_cast<Evoral::ControlList::InterpolationStyle> (string_2_enum (prop->value(), s));
			set_interpolation_of (p, s);

		} else if ((*i)->name() == X_("AutomationState")) {

			XMLProperty* prop;

			if ((prop = (*i)->property (X_("parameter"))) == 0) {
				error << _("Missing parameter property on AutomationState") << endmsg;
				return -1;
			}

			Evoral::Parameter p = EventTypeMap::instance().new_parameter (prop->value());

			if ((prop = (*i)->property (X_("state"))) == 0) {
				error << _("Missing state property on AutomationState") << endmsg;
				return -1;
			}

			AutoState s = static_cast<AutoState> (string_2_enum (prop->value(), s));
			set_automation_state_of (p, s);
		}
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
MidiSource::update_length (framecnt_t)
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
framecnt_t
MidiSource::midi_read (Evoral::EventSink<framepos_t>& dst, framepos_t source_start,
                       framepos_t start, framecnt_t cnt,
                       MidiStateTracker* tracker,
                       std::set<Evoral::Parameter> const & filtered) const
{
	Glib::Threads::Mutex::Lock lm (_lock);

	BeatsFramesConverter converter(_session.tempo_map(), source_start);

	DEBUG_TRACE (DEBUG::MidiSourceIO, string_compose ("MidiSource::midi-read() %5 sstart %1 start %2 cnt %3 tracker %4\n",
							  source_start, start, cnt, tracker, name()));

	if (_model) {
		Evoral::Sequence<double>::const_iterator& i = _model_iter;

		// If the cached iterator is invalid, search for the first event past start
		if (_last_read_end == 0 || start != _last_read_end || !_model_iter_valid) {
			DEBUG_TRACE (DEBUG::MidiSourceIO, string_compose ("*** %1 search for relevant iterator for %1 / %2\n", _name, source_start, start));
			for (i = _model->begin(0, false, filtered); i != _model->end(); ++i) {
				if (converter.to(i->time()) >= start) {
					DEBUG_TRACE (DEBUG::MidiSourceIO, string_compose ("***\tstop iterator search @ %1\n", i->time()));
					break;
				}
			}
			_model_iter_valid = true;
		} else {
			DEBUG_TRACE (DEBUG::MidiSourceIO, string_compose ("*** %1 use cachediterator for %1 / %2\n", _name, source_start, start));
		}

		_last_read_end = start + cnt;

		// Read events up to end
		for (; i != _model->end(); ++i) {
			const framecnt_t time_frames = converter.to(i->time());
			if (time_frames < start + cnt) {
				/* convert event times to session frames by adding on the source start position in session frames */
				dst.write (time_frames + source_start, i->event_type(), i->size(), i->buffer());

                                DEBUG_TRACE (DEBUG::MidiSourceIO, string_compose ("%1: add event @ %2 type %3 size = %4\n",
                                                                                  _name, time_frames + source_start, i->event_type(), i->size()));

				if (tracker) {
					Evoral::MIDIEvent<Evoral::MusicalTime>& ev (*(reinterpret_cast<Evoral::MIDIEvent<Evoral::MusicalTime>*> 
										      (const_cast<Evoral::Event<Evoral::MusicalTime>*> (&(*i)))));
					if (ev.is_note_on()) {
						DEBUG_TRACE (DEBUG::MidiSourceIO, string_compose ("\t%1 track note on %2 @ %3 velocity %4\n", _name, (int) ev.note(), time_frames, (int) ev.velocity()));
						tracker->add (ev.note(), ev.channel());
					} else if (ev.is_note_off()) {
						DEBUG_TRACE (DEBUG::MidiSourceIO, string_compose ("\t%1 track note off %2 @ %3\n", _name, (int) ev.note(), time_frames));
						tracker->remove (ev.note(), ev.channel());
					}
				}
			} else {
                                DEBUG_TRACE (DEBUG::MidiSourceIO, string_compose ("%1: reached end with event @ %2 vs. %3\n",
                                                                                  _name, time_frames, start+cnt));
				break;
			}
		}
		return cnt;
	} else {
		return read_unlocked (dst, source_start, start, cnt, tracker);
	}
}

framecnt_t
MidiSource::midi_write (MidiRingBuffer<framepos_t>& source,
                        framepos_t                  source_start,
                        framecnt_t                  cnt)
{
	Glib::Threads::Mutex::Lock lm (_lock);

	const framecnt_t ret = write_unlocked (source, source_start, cnt);

	if (cnt == max_framecnt) {
		_last_read_end = 0;
	} else {
		_capture_length += cnt;
	}

	return ret;
}

void
MidiSource::mark_streaming_midi_write_started (NoteMode mode)
{
	if (_model) {
		_model->set_note_mode (mode);
		_model->start_write ();
	}

	_writing = true;
}

void
MidiSource::mark_write_starting_now (framecnt_t position,
                                     framecnt_t capture_length,
                                     framecnt_t loop_length)
{
	/* I'm not sure if this is the best way to approach this, but
	   _capture_length needs to be set up with the transport frame
	   when a record actually starts, as it is used by
	   SMFSource::write_unlocked to decide whether incoming notes
	   are within the correct time range.
	   mark_streaming_midi_write_started (perhaps a more logical
	   place to do this) is not called at exactly the time when
	   record starts, and I don't think it necessarily can be
	   because it is not RT-safe.
	*/

	set_timeline_position(position);
	_capture_length      = capture_length;
	_capture_loop_length = loop_length;

	BeatsFramesConverter converter(_session.tempo_map(), position);
	_length_beats = converter.from(capture_length);
}

void
MidiSource::mark_streaming_write_started ()
{
	NoteMode note_mode = _model ? _model->note_mode() : Sustained;
	mark_streaming_midi_write_started (note_mode);
}

void
MidiSource::mark_midi_streaming_write_completed (Evoral::Sequence<Evoral::MusicalTime>::StuckNoteOption option, Evoral::MusicalTime end)
{
	if (_model) {
		_model->end_write (option, end);
	}

	_writing = false;
}

void
MidiSource::mark_streaming_write_completed ()
{
	mark_midi_streaming_write_completed (Evoral::Sequence<Evoral::MusicalTime>::DeleteStuckNotes);
}

boost::shared_ptr<MidiSource>
MidiSource::clone (const string& path, Evoral::MusicalTime begin, Evoral::MusicalTime end)
{
	string newname = PBD::basename_nosuffix(_name.val());
	string newpath;

	if (path.empty()) {

		/* get a new name for the MIDI file we're going to write to
		 */
		
		do {
			newname = bump_name_once (newname, '-');
			newpath = Glib::build_filename (_session.session_directory().midi_path(), newname + ".mid");
			
		} while (Glib::file_test (newpath, Glib::FILE_TEST_EXISTS));
	} else {
		/* caller must check for pre-existing file */
		newpath = path;
	}

	boost::shared_ptr<MidiSource> newsrc = boost::dynamic_pointer_cast<MidiSource>(
		SourceFactory::createWritable(DataType::MIDI, _session,
		                              newpath, false, _session.frame_rate()));

	newsrc->set_timeline_position(_timeline_position);
	newsrc->copy_interpolation_from (this);
	newsrc->copy_automation_state_from (this);

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

	if (_model && _model->edited()) {
		
		// if the model is edited, write its contents into
		// the current source file (overwiting previous contents.

		/* temporarily drop our reference to the model so that
		   as the model pushes its current state to us, we don't
		   try to update it.
		*/

		boost::shared_ptr<MidiModel> mm = _model;
		_model.reset ();

		/* flush model contents to disk
		 */

		mm->sync_to_source ();

		/* reacquire model */

		_model = mm;

	} else {
		flush_midi();
	}
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

/** @return Interpolation style that should be used for control parameter \a p */
Evoral::ControlList::InterpolationStyle
MidiSource::interpolation_of (Evoral::Parameter p) const
{
	InterpolationStyleMap::const_iterator i = _interpolation_style.find (p);
	if (i == _interpolation_style.end()) {
		return EventTypeMap::instance().interpolation_of (p);
	}

	return i->second;
}

AutoState
MidiSource::automation_state_of (Evoral::Parameter p) const
{
	AutomationStateMap::const_iterator i = _automation_state.find (p);
	if (i == _automation_state.end()) {
		/* default to `play', otherwise if MIDI is recorded /
		   imported with controllers etc. they are by default
		   not played back, which is a little surprising.
		*/
		return Play;
	}

	return i->second;
}

/** Set interpolation style to be used for a given parameter.  This change will be
 *  propagated to anyone who needs to know.
 */
void
MidiSource::set_interpolation_of (Evoral::Parameter p, Evoral::ControlList::InterpolationStyle s)
{
	if (interpolation_of (p) == s) {
		return;
	}

	if (EventTypeMap::instance().interpolation_of (p) == s) {
		/* interpolation type is being set to the default, so we don't need a note in our map */
		_interpolation_style.erase (p);
	} else {
		_interpolation_style[p] = s;
	}

	InterpolationChanged (p, s); /* EMIT SIGNAL */
}

void
MidiSource::set_automation_state_of (Evoral::Parameter p, AutoState s)
{
	if (automation_state_of (p) == s) {
		return;
	}

	if (s == Play) {
		/* automation state is being set to the default, so we don't need a note in our map */
		_automation_state.erase (p);
	} else {
		_automation_state[p] = s;
	}

	AutomationStateChanged (p, s); /* EMIT SIGNAL */
}

void
MidiSource::copy_interpolation_from (boost::shared_ptr<MidiSource> s)
{
	copy_interpolation_from (s.get ());
}

void
MidiSource::copy_automation_state_from (boost::shared_ptr<MidiSource> s)
{
	copy_automation_state_from (s.get ());
}

void
MidiSource::copy_interpolation_from (MidiSource* s)
{
	_interpolation_style = s->_interpolation_style;

	/* XXX: should probably emit signals here */
}

void
MidiSource::copy_automation_state_from (MidiSource* s)
{
	_automation_state = s->_automation_state;

	/* XXX: should probably emit signals here */
}
