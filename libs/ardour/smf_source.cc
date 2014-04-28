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

#include <vector>

#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <regex.h>

#include "pbd/pathscanner.h"
#include "pbd/stl_delete.h"
#include "pbd/strsplit.h"

#include <glibmm/miscutils.h>
#include <glibmm/fileutils.h>

#include "evoral/Control.hpp"
#include "evoral/evoral/SMF.hpp"

#include "ardour/event_type_map.h"
#include "ardour/midi_model.h"
#include "ardour/midi_ring_buffer.h"
#include "ardour/midi_state_tracker.h"
#include "ardour/session.h"
#include "ardour/smf_source.h"
#include "ardour/debug.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace Glib;
using namespace PBD;
using namespace Evoral;

/** Constructor used for new internal-to-session files.  File cannot exist. */
SMFSource::SMFSource (Session& s, const string& path, Source::Flag flags)
	: Source(s, DataType::MIDI, path, flags)
	, MidiSource(s, path, flags)
	, FileSource(s, DataType::MIDI, path, string(), flags)
	, Evoral::SMF()
	, _last_ev_time_beats(0.0)
	, _last_ev_time_frames(0)
	, _smf_last_read_end (0)
	, _smf_last_read_time (0)
{
	/* note that origin remains empty */

	if (init (_path, false)) {
		throw failed_constructor ();
	}
 
        assert (!Glib::file_test (_path, Glib::FILE_TEST_EXISTS));
	existence_check ();

	/* file is not opened until write */

	if (flags & Writable) {
		return;
	}

	if (open (_path)) {
		throw failed_constructor ();
	}

	_open = true;
}

/** Constructor used for external-to-session files.  File must exist. */
SMFSource::SMFSource (Session& s, const string& path)
	: Source(s, DataType::MIDI, path, Source::Flag (0))
	, MidiSource(s, path, Source::Flag (0))
	, FileSource(s, DataType::MIDI, path, string(), Source::Flag (0))
	, Evoral::SMF()
	, _last_ev_time_beats(0.0)
	, _last_ev_time_frames(0)
	, _smf_last_read_end (0)
	, _smf_last_read_time (0)
{
	/* note that origin remains empty */

	if (init (_path, false)) {
		throw failed_constructor ();
	}
 
        assert (Glib::file_test (_path, Glib::FILE_TEST_EXISTS));
	existence_check ();

	/* file is not opened until write */

	if (_flags & Writable) {
		return;
	}

	if (open (_path)) {
		throw failed_constructor ();
	}

	_open = true;
}

/** Constructor used for existing internal-to-session files. */
SMFSource::SMFSource (Session& s, const XMLNode& node, bool must_exist)
	: Source(s, node)
	, MidiSource(s, node)
	, FileSource(s, node, must_exist)
	, _last_ev_time_beats(0.0)
	, _last_ev_time_frames(0)
	, _smf_last_read_end (0)
	, _smf_last_read_time (0)
{
	if (set_state(node, Stateful::loading_state_version)) {
		throw failed_constructor ();
	}

	if (init (_path, true)) {
		throw failed_constructor ();
	}

        assert (Glib::file_test (_path, Glib::FILE_TEST_EXISTS));
	existence_check ();

	if (open(_path)) {
		throw failed_constructor ();
	}

	_open = true;
}

SMFSource::~SMFSource ()
{
	if (removable()) {
		unlink (_path.c_str());
	}
}

int
SMFSource::open_for_write ()
{
	if (create (_path)) {
		return -1;
	}
	_open = true;
	return 0;
}

/** All stamps in audio frames */
framecnt_t
SMFSource::read_unlocked (Evoral::EventSink<framepos_t>& destination,
                          framepos_t const               source_start,
                          framepos_t                     start,
                          framecnt_t                     duration,
                          MidiStateTracker*              tracker) const
{
	int      ret  = 0;
	uint64_t time = 0; // in SMF ticks, 1 tick per _ppqn

	if (writable() && !_open) {
		/* nothing to read since nothing has ben written */
		return duration;
	}

	DEBUG_TRACE (DEBUG::MidiSourceIO, string_compose ("SMF read_unlocked: start %1 duration %2\n", start, duration));

	// Output parameters for read_event (which will allocate scratch in buffer as needed)
	uint32_t ev_delta_t = 0;
	uint32_t ev_type    = 0;
	uint32_t ev_size    = 0;
	uint8_t* ev_buffer  = 0;

	size_t scratch_size = 0; // keep track of scratch to minimize reallocs

	BeatsFramesConverter converter(_session.tempo_map(), source_start);

	const uint64_t start_ticks = (uint64_t)(converter.from(start) * ppqn());
	DEBUG_TRACE (DEBUG::MidiSourceIO, string_compose ("SMF read_unlocked: start in ticks %1\n", start_ticks));

	if (_smf_last_read_end == 0 || start != _smf_last_read_end) {
		DEBUG_TRACE (DEBUG::MidiSourceIO, string_compose ("SMF read_unlocked: seek to %1\n", start));
		Evoral::SMF::seek_to_start();
		while (time < start_ticks) {
			gint ignored;

			ret = read_event(&ev_delta_t, &ev_size, &ev_buffer, &ignored);
			if (ret == -1) { // EOF
				_smf_last_read_end = start + duration;
				return duration;
			}
			time += ev_delta_t; // accumulate delta time
		}
	} else {
		DEBUG_TRACE (DEBUG::MidiSourceIO, string_compose ("SMF read_unlocked: set time to %1\n", _smf_last_read_time));
		time = _smf_last_read_time;
	}

	_smf_last_read_end = start + duration;

	while (true) {
		gint ignored; /* XXX don't ignore note id's ??*/

		ret = read_event(&ev_delta_t, &ev_size, &ev_buffer, &ignored);
		if (ret == -1) { // EOF
			break;
		}

		time += ev_delta_t; // accumulate delta time
		_smf_last_read_time = time;

		if (ret == 0) { // meta-event (skipped, just accumulate time)
			continue;
		}

		ev_type = EventTypeMap::instance().midi_event_type(ev_buffer[0]);

		DEBUG_TRACE (DEBUG::MidiSourceIO, string_compose ("SMF read_unlocked delta %1, time %2, buf[0] %3, type %4\n",
								  ev_delta_t, time, ev_buffer[0], ev_type));

		assert(time >= start_ticks);

		/* Note that we add on the source start time (in session frames) here so that ev_frame_time
		   is in session frames.
		*/
		const framepos_t ev_frame_time = converter.to(time / (double)ppqn()) + source_start;

		if (ev_frame_time < start + duration) {
			destination.write (ev_frame_time, ev_type, ev_size, ev_buffer);

			if (tracker) {
				if (ev_buffer[0] & MIDI_CMD_NOTE_ON) {
					tracker->add (ev_buffer[1], ev_buffer[0] & 0xf);
				} else if (ev_buffer[0] & MIDI_CMD_NOTE_OFF) {
					tracker->remove (ev_buffer[1], ev_buffer[0] & 0xf);
				}
			}
		} else {
			break;
		}

		if (ev_size > scratch_size) {
			scratch_size = ev_size;
		}
		ev_size = scratch_size; // ensure read_event only allocates if necessary
	}

	return duration;
}

framecnt_t
SMFSource::write_unlocked (MidiRingBuffer<framepos_t>& source,
                           framepos_t                  position,
                           framecnt_t                  cnt)
{
	if (!_writing) {
		mark_streaming_write_started ();
	}

	framepos_t        time;
	Evoral::EventType type;
	uint32_t          size;

	size_t   buf_capacity = 4;
	uint8_t* buf          = (uint8_t*)malloc(buf_capacity);

	if (_model && !_model->writing()) {
		_model->start_write();
	}

	Evoral::MIDIEvent<framepos_t> ev;
	while (true) {
		/* Get the event time, in frames since session start but ignoring looping. */
		bool ret;
		if (!(ret = source.peek ((uint8_t*)&time, sizeof (time)))) {
			/* Ring is empty, no more events. */
			break;
		}

		if ((cnt != max_framecnt) &&
		    (time > position + _capture_length + cnt)) {
			/* The diskstream doesn't want us to write everything, and this
			   event is past the end of this block, so we're done for now. */
			break;
		}

		/* Read the time, type, and size of the event. */
		if (!(ret = source.read_prefix (&time, &type, &size))) {
			error << _("Unable to read event prefix, corrupt MIDI ring") << endmsg;
			break;
		}

		/* Enlarge body buffer if necessary now that we know the size. */
		if (size > buf_capacity) {
			buf_capacity = size;
			buf = (uint8_t*)realloc(buf, size);
		}

		/* Read the event body into buffer. */
		ret = source.read_contents(size, buf);
		if (!ret) {
			error << _("Event has time and size but no body, corrupt MIDI ring") << endmsg;
			break;
		}

		/* Convert event time from absolute to source relative. */
		if (time < position) {
			error << _("Event time is before MIDI source position") << endmsg;
			break;
		}
		time -= position;
			
		ev.set(buf, size, time);
		ev.set_event_type(EventTypeMap::instance().midi_event_type(ev.buffer()[0]));
		ev.set_id(Evoral::next_event_id());

		if (!(ev.is_channel_event() || ev.is_smf_meta_event() || ev.is_sysex())) {
			continue;
		}

		append_event_unlocked_frames(ev, position);
	}

	Evoral::SMF::flush ();
	free (buf);

	return cnt;
}

/** Append an event with a timestamp in beats (double) */
void
SMFSource::append_event_unlocked_beats (const Evoral::Event<double>& ev)
{
	if (!_writing || ev.size() == 0)  {
		return;
	}

	/*printf("SMFSource: %s - append_event_unlocked_beats ID = %d time = %lf, size = %u, data = ",
               name().c_str(), ev.id(), ev.time(), ev.size());
	       for (size_t i = 0; i < ev.size(); ++i) printf("%X ", ev.buffer()[i]); printf("\n");*/

	if (ev.time() < _last_ev_time_beats) {
		warning << string_compose(_("Skipping event with unordered time %1"), ev.time())
		        << endmsg;
		return;
	}

	Evoral::event_id_t event_id;

	if (ev.id() < 0) {
		event_id  = Evoral::next_event_id();
	} else {
		event_id = ev.id();
	}

	if (_model) {
		_model->append (ev, event_id);
	}

	_length_beats = max(_length_beats, ev.time());

	const double delta_time_beats   = ev.time() - _last_ev_time_beats;
	const uint32_t delta_time_ticks = (uint32_t)lrint(delta_time_beats * (double)ppqn());

	Evoral::SMF::append_event_delta(delta_time_ticks, ev.size(), ev.buffer(), event_id);
	_last_ev_time_beats = ev.time();
}

/** Append an event with a timestamp in frames (framepos_t) */
void
SMFSource::append_event_unlocked_frames (const Evoral::Event<framepos_t>& ev, framepos_t position)
{
	if (!_writing || ev.size() == 0)  {
		return;
	}

	// printf("SMFSource: %s - append_event_unlocked_frames ID = %d time = %u, size = %u, data = ",
	// name().c_str(), ev.id(), ev.time(), ev.size());
	// for (size_t i=0; i < ev.size(); ++i) printf("%X ", ev.buffer()[i]); printf("\n");

	if (ev.time() < _last_ev_time_frames) {
		warning << string_compose(_("Skipping event with unordered time %1"), ev.time())
		        << endmsg;
		return;
	}

	BeatsFramesConverter converter(_session.tempo_map(), position);
	const double ev_time_beats = converter.from(ev.time());
	Evoral::event_id_t event_id;

	if (ev.id() < 0) {
		event_id  = Evoral::next_event_id();
	} else {
		event_id = ev.id();
	}

	if (_model) {
		const Evoral::Event<double> beat_ev (ev.event_type(),
		                                     ev_time_beats,
		                                     ev.size(),
		                                     const_cast<uint8_t*>(ev.buffer()));
		_model->append (beat_ev, event_id);
	}

	_length_beats = max(_length_beats, ev_time_beats);

	const Evoral::MusicalTime last_time_beats  = converter.from (_last_ev_time_frames);
	const Evoral::MusicalTime delta_time_beats = ev_time_beats - last_time_beats;
	const uint32_t            delta_time_ticks = (uint32_t)(lrint(delta_time_beats * (double)ppqn()));

	Evoral::SMF::append_event_delta(delta_time_ticks, ev.size(), ev.buffer(), event_id);
	_last_ev_time_frames = ev.time();
}

XMLNode&
SMFSource::get_state ()
{
	XMLNode& node = MidiSource::get_state();
	node.add_property (X_("origin"), _origin);
	return node;
}

int
SMFSource::set_state (const XMLNode& node, int version)
{
	if (Source::set_state (node, version)) {
		return -1;
	}

	if (MidiSource::set_state (node, version)) {
		return -1;
	}

	if (FileSource::set_state (node, version)) {
		return -1;
	}

	return 0;
}

void
SMFSource::mark_streaming_midi_write_started (NoteMode mode)
{
	/* CALLER MUST HOLD LOCK */

	if (!_open && open_for_write()) {
		error << string_compose (_("cannot open MIDI file %1 for write"), _path) << endmsg;
		/* XXX should probably throw or return something */
		return;
	}

	MidiSource::mark_streaming_midi_write_started (mode);
	Evoral::SMF::begin_write ();
	_last_ev_time_beats = 0.0;
	_last_ev_time_frames = 0;
}

void
SMFSource::mark_streaming_write_completed ()
{
	mark_midi_streaming_write_completed (Evoral::Sequence<Evoral::MusicalTime>::DeleteStuckNotes);
}

void
SMFSource::mark_midi_streaming_write_completed (Evoral::Sequence<Evoral::MusicalTime>::StuckNoteOption stuck_notes_option, Evoral::MusicalTime when)
{
	Glib::Threads::Mutex::Lock lm (_lock);
	MidiSource::mark_midi_streaming_write_completed (stuck_notes_option, when);

	if (!writable()) {
		warning << string_compose ("attempt to write to unwritable SMF file %1", _path) << endmsg;
		return;
	}

	if (_model) {
		_model->set_edited(false);
	}

	Evoral::SMF::end_write ();

	/* data in the file now, not removable */

	mark_nonremovable ();
}

bool
SMFSource::valid_midi_file (const string& file)
{
	if (safe_midi_file_extension (file) ) {
		return (SMF::test (file) );
	}
	return false;
}

bool
SMFSource::safe_midi_file_extension (const string& file)
{
	static regex_t compiled_pattern;
	static bool compile = true;
	const int nmatches = 2;
	regmatch_t matches[nmatches];
	
	if (Glib::file_test (file, Glib::FILE_TEST_EXISTS)) {
		if (!Glib::file_test (file, Glib::FILE_TEST_IS_REGULAR)) {
			/* exists but is not a regular file */
			return false;
		}
	}

	if (compile && regcomp (&compiled_pattern, "\\.[mM][iI][dD][iI]?$", REG_EXTENDED)) {
		return false;
	} else {
		compile = false;
	}
	
	if (regexec (&compiled_pattern, file.c_str(), nmatches, matches, 0)) {
		return false;
	}

	return true;
}

static bool compare_eventlist (
		const std::pair< Evoral::Event<double>*, gint >& a,
		const std::pair< Evoral::Event<double>*, gint >& b) {
	return ( a.first->time() < b.first->time() );
}

void
SMFSource::load_model (bool lock, bool force_reload)
{
	if (_writing) {
		return;
	}

	boost::shared_ptr<Glib::Threads::Mutex::Lock> lm;
	if (lock)
		lm = boost::shared_ptr<Glib::Threads::Mutex::Lock>(new Glib::Threads::Mutex::Lock(_lock));

	if (_model && !force_reload) {
		return;
	}

	if (!_model) {
		_model = boost::shared_ptr<MidiModel> (new MidiModel (shared_from_this ()));
	} else {
		_model->clear();
	}

	if (writable() && !_open) {
		return;
	}

	_model->start_write();
	Evoral::SMF::seek_to_start();

	uint64_t time = 0; /* in SMF ticks */
	Evoral::Event<double> ev;

	uint32_t scratch_size = 0; // keep track of scratch and minimize reallocs

	uint32_t delta_t = 0;
	uint32_t size    = 0;
	uint8_t* buf     = NULL;
	int ret;
	gint event_id;
	bool have_event_id;

	// TODO simplify event allocation
	std::list< std::pair< Evoral::Event<double>*, gint > > eventlist;

	for (unsigned i = 1; i <= num_tracks(); ++i) {
		if (seek_to_track(i)) continue;

		time = 0;
		have_event_id = false;

		while ((ret = read_event (&delta_t, &size, &buf, &event_id)) >= 0) {

			time += delta_t;

			if (ret == 0) {
				/* meta-event : did we get an event ID ?  */
				if (event_id >= 0) {
					have_event_id = true;
				}
				continue;
			}

			if (ret > 0) {
				/* not a meta-event */

				if (!have_event_id) {
					event_id = Evoral::next_event_id();
				}
				uint32_t event_type = EventTypeMap::instance().midi_event_type(buf[0]);
				double   event_time = time / (double) ppqn();
#ifndef NDEBUG
				std::string ss;

				for (uint32_t xx = 0; xx < size; ++xx) {
					char b[8];
					snprintf (b, sizeof (b), "0x%x ", buf[xx]);
					ss += b;
				}

				DEBUG_TRACE (DEBUG::MidiSourceIO, string_compose ("SMF %6 load model delta %1, time %2, size %3 buf %4, type %5\n",
							delta_t, time, size, ss , event_type, name()));
#endif

				eventlist.push_back(make_pair (
							new Evoral::Event<double> (
								event_type, event_time,
								size, buf, true)
							, event_id));

				// Set size to max capacity to minimize allocs in read_event
				scratch_size = std::max(size, scratch_size);
				size = scratch_size;

				_length_beats = max(_length_beats, event_time);
			}

			/* event ID's must immediately precede the event they are for */
			have_event_id = false;
		}
	}

	eventlist.sort(compare_eventlist);

	std::list< std::pair< Evoral::Event<double>*, gint > >::iterator it;
	for (it=eventlist.begin(); it!=eventlist.end(); ++it) {
		_model->append (*it->first, it->second);
		delete it->first;
	}

	_model->end_write (Evoral::Sequence<Evoral::MusicalTime>::ResolveStuckNotes, _length_beats);
	_model->set_edited (false);

	_model_iter = _model->begin();

	free(buf);
}

void
SMFSource::destroy_model ()
{
	//cerr << _name << " destroying model " << _model.get() << endl;
	_model.reset();
}

void
SMFSource::flush_midi ()
{
	if (!writable() || (writable() && !_open)) {
		return;
	}

	Evoral::SMF::end_write ();
	/* data in the file means its no longer removable */
	mark_nonremovable ();
}

void
SMFSource::set_path (const string& p)
{
	FileSource::set_path (p);
	SMF::set_path (_path);
}

/** Ensure that this source has some file on disk, even if it's just a SMF header */
void
SMFSource::ensure_disk_file ()
{
	if (_model) {
		/* We have a model, so write it to disk; see MidiSource::session_saved
		   for an explanation of what we are doing here.
		*/
		boost::shared_ptr<MidiModel> mm = _model;
		_model.reset ();
		mm->sync_to_source ();
		_model = mm;
	} else {
		/* No model; if it's not already open, it's an empty source, so create
		   and open it for writing.
		*/
		if (!_open) {
			open_for_write ();
		}

		/* Flush, which will definitely put something on disk */
		flush_midi ();
	}
}

void
SMFSource::prevent_deletion ()
{
	/* Unlike the audio case, the MIDI file remains mutable (because we can
	   edit MIDI data)
	*/
  
	_flags = Flag (_flags & ~(Removable|RemovableIfEmpty|RemoveAtDestroy));
}

int
SMFSource::rename (const string& newname)
{
	Glib::Threads::Mutex::Lock lm (_lock);
	string oldpath = _path;
	string newpath = _session.new_source_path_from_name (DataType::MIDI, newname);

	if (newpath.empty()) {
		error << string_compose (_("programming error: %1"), "cannot generate a changed file path") << endmsg;
		return -1;
	}

	// Test whether newpath exists, if yes notify the user but continue.
	if (Glib::file_test (newpath, Glib::FILE_TEST_EXISTS)) {
		error << string_compose (_("Programming error! %1 tried to rename a file over another file! It's safe to continue working, but please report this to the developers."), PROGRAM_NAME) << endmsg;
		return -1;
	}

	if (Glib::file_test (oldpath.c_str(), Glib::FILE_TEST_EXISTS)) { 
		/* rename only needed if file exists on disk */
		if (::rename (oldpath.c_str(), newpath.c_str()) != 0) {
			error << string_compose (_("cannot rename file %1 to %2 (%3)"), oldpath, newpath, strerror(errno)) << endmsg;
			return -1;
		}
	}

	_name = Glib::path_get_basename (newpath);
	_path = newpath;

	return 0;
}
