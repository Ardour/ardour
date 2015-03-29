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

#include "pbd/file_utils.h"
#include "pbd/stl_delete.h"
#include "pbd/strsplit.h"

#include <glib/gstdio.h>
#include <glibmm/miscutils.h>
#include <glibmm/fileutils.h>

#include "evoral/Control.hpp"
#include "evoral/SMF.hpp"

#include "ardour/debug.h"
#include "ardour/midi_channel_filter.h"
#include "ardour/midi_model.h"
#include "ardour/midi_ring_buffer.h"
#include "ardour/midi_state_tracker.h"
#include "ardour/parameter_types.h"
#include "ardour/session.h"
#include "ardour/smf_source.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace Glib;
using namespace PBD;
using namespace Evoral;
using namespace std;

/** Constructor used for new internal-to-session files.  File cannot exist. */
SMFSource::SMFSource (Session& s, const string& path, Source::Flag flags)
	: Source(s, DataType::MIDI, path, flags)
	, MidiSource(s, path, flags)
	, FileSource(s, DataType::MIDI, path, string(), flags)
	, Evoral::SMF()
	, _open (false)
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

	_flags = Source::Flag (_flags | Empty);

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
	, _open (false)
	, _last_ev_time_beats(0.0)
	, _last_ev_time_frames(0)
	, _smf_last_read_end (0)
	, _smf_last_read_time (0)
{
	/* note that origin remains empty */

	if (init (_path, true)) {
		throw failed_constructor ();
	}
 
        assert (Glib::file_test (_path, Glib::FILE_TEST_EXISTS));
	existence_check ();

	if (_flags & Writable) {
		/* file is not opened until write */
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
	, _open (false)
	, _last_ev_time_beats(0.0)
	, _last_ev_time_frames(0)
	, _smf_last_read_end (0)
	, _smf_last_read_time (0)
{
	if (set_state(node, Stateful::loading_state_version)) {
		throw failed_constructor ();
	}

	/* we expect the file to exist, but if no MIDI data was ever added
	   it will have been removed at last session close. so, we don't
	   require it to exist if it was marked Empty.
	*/

	try {

		if (init (_path, true)) {
			throw failed_constructor ();
		}

	} catch (MissingSource& err) {

		if (_flags & Source::Empty) {
			/* we don't care that the file was not found, because
			   it was empty. But FileSource::init() will have
			   failed to set our _path correctly, so we have to do
			   this ourselves. Use the first entry in the search
			   path for MIDI files, which is assumed to be the
			   correct "main" location.
			*/
			std::vector<string> sdirs = s.source_search_path (DataType::MIDI);
			_path = Glib::build_filename (sdirs.front(), _path);
			/* This might be important, too */
			_file_is_new = true;
		} else {
			/* pass it on */
			throw;
		}
	}

	if (!(_flags & Source::Empty)) {
		assert (Glib::file_test (_path, Glib::FILE_TEST_EXISTS));
		existence_check ();
	} else {
		assert (_flags & Source::Writable);
		/* file will be opened on write */
		return;
	}

	if (open (_path)) {
		throw failed_constructor ();
	}

	_open = true;
}

SMFSource::~SMFSource ()
{
	if (removable()) {
		::g_unlink (_path.c_str());
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
SMFSource::read_unlocked (const Lock&                    lock,
                          Evoral::EventSink<framepos_t>& destination,
                          framepos_t const               source_start,
                          framepos_t                     start,
                          framecnt_t                     duration,
                          MidiStateTracker*              tracker,
                          MidiChannelFilter*             filter) const
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

	const uint64_t start_ticks = converter.from(start).to_ticks();
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

		ev_type = midi_parameter_type(ev_buffer[0]);

		DEBUG_TRACE (DEBUG::MidiSourceIO, string_compose ("SMF read_unlocked delta %1, time %2, buf[0] %3, type %4\n",
								  ev_delta_t, time, ev_buffer[0], ev_type));

		assert(time >= start_ticks);

		/* Note that we add on the source start time (in session frames) here so that ev_frame_time
		   is in session frames.
		*/
		const framepos_t ev_frame_time = converter.to(Evoral::Beats::ticks_at_rate(time, ppqn())) + source_start;

		if (ev_frame_time < start + duration) {
			if (!filter || !filter->filter(ev_buffer, ev_size)) {
				destination.write (ev_frame_time, ev_type, ev_size, ev_buffer);
				if (tracker) {
					tracker->track(ev_buffer);
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
SMFSource::write_unlocked (const Lock&                 lock,
                           MidiRingBuffer<framepos_t>& source,
                           framepos_t                  position,
                           framecnt_t                  cnt)
{
	if (!_writing) {
		mark_streaming_write_started (lock);
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
		ev.set_event_type(midi_parameter_type(ev.buffer()[0]));
		ev.set_id(Evoral::next_event_id());

		if (!(ev.is_channel_event() || ev.is_smf_meta_event() || ev.is_sysex())) {
			continue;
		}

		append_event_frames(lock, ev, position);
	}

	Evoral::SMF::flush ();
	free (buf);

	return cnt;
}

/** Append an event with a timestamp in beats */
void
SMFSource::append_event_beats (const Glib::Threads::Mutex::Lock&   lock,
                               const Evoral::Event<Evoral::Beats>& ev)
{
	if (!_writing || ev.size() == 0)  {
		return;
	}

	/*printf("SMFSource: %s - append_event_beats ID = %d time = %lf, size = %u, data = ",
               name().c_str(), ev.id(), ev.time(), ev.size());
	       for (size_t i = 0; i < ev.size(); ++i) printf("%X ", ev.buffer()[i]); printf("\n");*/

	Evoral::Beats time = ev.time();
	if (time < _last_ev_time_beats) {
		const Evoral::Beats difference = _last_ev_time_beats - time;
		if (difference.to_double() / (double)ppqn() < 1.0) {
			/* Close enough.  This problem occurs because Sequence is not
			   actually ordered due to fuzzy time comparison.  I'm pretty sure
			   this is inherently a bad idea which causes problems all over the
			   place, but tolerate it here for now anyway. */
			time = _last_ev_time_beats;
		} else {
			/* Out of order by more than a tick. */
			warning << string_compose(_("Skipping event with unordered beat time %1 < %2 (off by %3 beats, %4 ticks)"),
			                          ev.time(), _last_ev_time_beats, difference, difference.to_double() / (double)ppqn())
			        << endmsg;
			return;
		}
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

	_length_beats = max(_length_beats, time);

	const Evoral::Beats delta_time_beats = time - _last_ev_time_beats;
	const uint32_t      delta_time_ticks = delta_time_beats.to_ticks(ppqn());

	Evoral::SMF::append_event_delta(delta_time_ticks, ev.size(), ev.buffer(), event_id);
	_last_ev_time_beats = time;
	_flags = Source::Flag (_flags & ~Empty);
}

/** Append an event with a timestamp in frames (framepos_t) */
void
SMFSource::append_event_frames (const Glib::Threads::Mutex::Lock& lock,
                                const Evoral::Event<framepos_t>&  ev,
                                framepos_t                        position)
{
	if (!_writing || ev.size() == 0)  {
		return;
	}

	// printf("SMFSource: %s - append_event_frames ID = %d time = %u, size = %u, data = ",
	// name().c_str(), ev.id(), ev.time(), ev.size());
	// for (size_t i=0; i < ev.size(); ++i) printf("%X ", ev.buffer()[i]); printf("\n");

	if (ev.time() < _last_ev_time_frames) {
		warning << string_compose(_("Skipping event with unordered frame time %1 < %2"),
		                          ev.time(), _last_ev_time_frames)
		        << endmsg;
		return;
	}

	BeatsFramesConverter converter(_session.tempo_map(), position);
	const Evoral::Beats  ev_time_beats = converter.from(ev.time());
	Evoral::event_id_t   event_id;

	if (ev.id() < 0) {
		event_id  = Evoral::next_event_id();
	} else {
		event_id = ev.id();
	}

	if (_model) {
		const Evoral::Event<Evoral::Beats> beat_ev (ev.event_type(),
		                                            ev_time_beats,
		                                            ev.size(),
		                                            const_cast<uint8_t*>(ev.buffer()));
		_model->append (beat_ev, event_id);
	}

	_length_beats = max(_length_beats, ev_time_beats);

	const Evoral::Beats last_time_beats  = converter.from (_last_ev_time_frames);
	const Evoral::Beats delta_time_beats = ev_time_beats - last_time_beats;
	const uint32_t      delta_time_ticks = delta_time_beats.to_ticks(ppqn());

	Evoral::SMF::append_event_delta(delta_time_ticks, ev.size(), ev.buffer(), event_id);
	_last_ev_time_frames = ev.time();
	_flags = Source::Flag (_flags & ~Empty);
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
SMFSource::mark_streaming_midi_write_started (const Lock& lock, NoteMode mode)
{
	if (!_open && open_for_write()) {
		error << string_compose (_("cannot open MIDI file %1 for write"), _path) << endmsg;
		/* XXX should probably throw or return something */
		return;
	}

	MidiSource::mark_streaming_midi_write_started (lock, mode);
	Evoral::SMF::begin_write ();
	_last_ev_time_beats  = Evoral::Beats();
	_last_ev_time_frames = 0;
}

void
SMFSource::mark_streaming_write_completed (const Lock& lock)
{
	mark_midi_streaming_write_completed (lock, Evoral::Sequence<Evoral::Beats>::DeleteStuckNotes);
}

void
SMFSource::mark_midi_streaming_write_completed (const Lock& lm, Evoral::Sequence<Evoral::Beats>::StuckNoteOption stuck_notes_option, Evoral::Beats when)
{
	MidiSource::mark_midi_streaming_write_completed (lm, stuck_notes_option, when);

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
	const std::pair< Evoral::Event<Evoral::Beats>*, gint >& a,
	const std::pair< Evoral::Event<Evoral::Beats>*, gint >& b) {
	return ( a.first->time() < b.first->time() );
}

void
SMFSource::load_model (const Glib::Threads::Mutex::Lock& lock, bool force_reload)
{
	if (_writing) {
		return;
	}

	if (_model && !force_reload) {
		return;
	}

	if (!_model) {
		_model = boost::shared_ptr<MidiModel> (new MidiModel (shared_from_this ()));
	} else {
		_model->clear();
	}

	invalidate(lock);

	if (writable() && !_open) {
		return;
	}

	_model->start_write();
	Evoral::SMF::seek_to_start();

	uint64_t time = 0; /* in SMF ticks */
	Evoral::Event<Evoral::Beats> ev;

	uint32_t scratch_size = 0; // keep track of scratch and minimize reallocs

	uint32_t delta_t = 0;
	uint32_t size    = 0;
	uint8_t* buf     = NULL;
	int ret;
	gint event_id;
	bool have_event_id;

	// TODO simplify event allocation
	std::list< std::pair< Evoral::Event<Evoral::Beats>*, gint > > eventlist;

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
				const uint32_t            event_type = midi_parameter_type(buf[0]);
				const Evoral::Beats event_time = Evoral::Beats::ticks_at_rate(time, ppqn());
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
							new Evoral::Event<Evoral::Beats> (
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

	std::list< std::pair< Evoral::Event<Evoral::Beats>*, gint > >::iterator it;
	for (it=eventlist.begin(); it!=eventlist.end(); ++it) {
		_model->append (*it->first, it->second);
		delete it->first;
	}

	_model->end_write (Evoral::Sequence<Evoral::Beats>::ResolveStuckNotes, _length_beats);
	_model->set_edited (false);
	invalidate(lock);

	free(buf);
}

void
SMFSource::destroy_model (const Glib::Threads::Mutex::Lock& lock)
{
	//cerr << _name << " destroying model " << _model.get() << endl;
	_model.reset();
	invalidate(lock);
}

void
SMFSource::flush_midi (const Lock& lock)
{
	if (!writable() || _length_beats == 0.0) {
		return;
	}

	ensure_disk_file (lock);

	Evoral::SMF::end_write ();
	/* data in the file means its no longer removable */
	mark_nonremovable ();

	invalidate(lock);
}

void
SMFSource::set_path (const string& p)
{
	FileSource::set_path (p);
	SMF::set_path (_path);
}

/** Ensure that this source has some file on disk, even if it's just a SMF header */
void
SMFSource::ensure_disk_file (const Lock& lock)
{
	if (!writable()) {
		return;
	}

	if (_model) {
		/* We have a model, so write it to disk; see MidiSource::session_saved
		   for an explanation of what we are doing here.
		*/
		boost::shared_ptr<MidiModel> mm = _model;
		_model.reset ();
		mm->sync_to_source (lock);
		_model = mm;
		invalidate(lock);
	} else {
		/* No model; if it's not already open, it's an empty source, so create
		   and open it for writing.
		*/
		if (!_open) {
			open_for_write ();
		}
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
		
	
