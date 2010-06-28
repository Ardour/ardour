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

#include <vector>

#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "pbd/mountpoint.h"
#include "pbd/pathscanner.h"
#include "pbd/stl_delete.h"
#include "pbd/strsplit.h"

#include <glibmm/miscutils.h>

#include "evoral/Control.hpp"

#include "ardour/audioengine.h"
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

/** Constructor used for new internal-to-session files.  File cannot exist. */
SMFSource::SMFSource (Session& s, const ustring& path, Source::Flag flags)
	: Source(s, DataType::MIDI, path, flags)
	, MidiSource(s, path)
	, FileSource(s, DataType::MIDI, path, flags)
	, Evoral::SMF()
	, _last_ev_time_beats(0.0)
	, _last_ev_time_frames(0)
	, _smf_last_read_end (0)
	, _smf_last_read_time (0)
{
	if (init(_path, false)) {
		throw failed_constructor ();
	}

	if (create(path)) {
		throw failed_constructor ();
	}

	load_model(true, true); // FIXME
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

	if (init(_path, true)) {
		throw failed_constructor ();
	}

	if (open(_path)) {
		throw failed_constructor ();
	}

	load_model(true, true); // FIXME
}

SMFSource::~SMFSource ()
{
	if (removable()) {
                cerr << name() << " is removable, empty ? " << empty() << " UC " << use_count() << endl;
		unlink (_path.c_str());
	}
}

/** All stamps in audio frames */
nframes_t
SMFSource::read_unlocked (Evoral::EventSink<nframes_t>& destination, sframes_t source_start,
			  sframes_t start, nframes_t duration,
			  sframes_t stamp_offset, sframes_t negative_stamp_offset,
			  MidiStateTracker* tracker) const
{
	int      ret  = 0;
	uint64_t time = 0; // in SMF ticks, 1 tick per _ppqn

	DEBUG_TRACE (DEBUG::MidiSourceIO, string_compose ("SMF read_unlocked: start %1 duration %2\n", start, duration));

	_read_data_count = 0;

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
			ret = read_event(&ev_delta_t, &ev_size, &ev_buffer);
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
		ret = read_event(&ev_delta_t, &ev_size, &ev_buffer);
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
		const sframes_t ev_frame_time = converter.to(time / (double)ppqn()) + stamp_offset;

#if 1
		cerr << " frames = " << ev_frame_time
		     << " w/offset = " << ev_frame_time - negative_stamp_offset
		     << endl;
#endif

		if (ev_frame_time < start + duration) {
			destination.write(ev_frame_time - negative_stamp_offset, ev_type, ev_size, ev_buffer);

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

		_read_data_count += ev_size;

		if (ev_size > scratch_size) {
			scratch_size = ev_size;
		}
		ev_size = scratch_size; // ensure read_event only allocates if necessary
	}

	return duration;
}

/** All stamps in audio frames */
nframes_t
SMFSource::write_unlocked (MidiRingBuffer<nframes_t>& source, sframes_t position, nframes_t duration)
{
	_write_data_count = 0;

	nframes_t         time;
	Evoral::EventType type;
	uint32_t          size;

	size_t   buf_capacity = 4;
	uint8_t* buf          = (uint8_t*)malloc(buf_capacity);

	if (_model && ! _model->writing()) {
		_model->start_write();
	}

	Evoral::MIDIEvent<nframes_t> ev;

	while (true) {
		bool ret = source.peek_time(&time);
		if (!ret || time > _last_write_end + duration) {
			break;
		}

		ret = source.read_prefix(&time, &type, &size);
		if (!ret) {
			cerr << "ERROR: Unable to read event prefix, corrupt MIDI ring buffer" << endl;
			break;
		}

		if (size > buf_capacity) {
			buf_capacity = size;
			buf = (uint8_t*)realloc(buf, size);
		}

		ret = source.read_contents(size, buf);
		if (!ret) {
			cerr << "ERROR: Read time/size but not buffer, corrupt MIDI ring buffer" << endl;
			break;
		}

		assert(time >= position);
		time -= position;

		ev.set(buf, size, time);
		ev.set_event_type(EventTypeMap::instance().midi_event_type(ev.buffer()[0]));
		if (!(ev.is_channel_event() || ev.is_smf_meta_event() || ev.is_sysex())) {
			/*cerr << "SMFSource: WARNING: caller tried to write non SMF-Event of type "
					<< std::hex << int(ev.buffer()[0]) << endl;*/
			continue;
		}

		append_event_unlocked_frames(ev, position);
	}

	if (_model) {
		set_default_controls_interpolation();
	}

	Evoral::SMF::flush();
	free(buf);

	ViewDataRangeReady(position + _last_write_end, duration); /* EMIT SIGNAL */

	return duration;
}


/** Append an event with a timestamp in beats (double) */
void
SMFSource::append_event_unlocked_beats (const Evoral::Event<double>& ev)
{
	assert(_writing);
	if (ev.size() == 0)  {
		return;
	}

	/*printf("SMFSource: %s - append_event_unlocked_beats time = %lf, size = %u, data = ",
			name().c_str(), ev.time(), ev.size());
	for (size_t i = 0; i < ev.size(); ++i) printf("%X ", ev.buffer()[i]); printf("\n");*/

	assert(ev.time() >= 0);
	if (ev.time() < _last_ev_time_beats) {
		cerr << "SMFSource: Warning: Skipping event with non-monotonic time" << endl;
		return;
	}

	_length_beats = max(_length_beats, ev.time());

	const double delta_time_beats   = ev.time() - _last_ev_time_beats;
	const uint32_t delta_time_ticks = (uint32_t)lrint(delta_time_beats * (double)ppqn());

	Evoral::SMF::append_event_delta(delta_time_ticks, ev.size(), ev.buffer());
	_last_ev_time_beats = ev.time();

	_write_data_count += ev.size();

	if (_model) {
		_model->append (ev);
	}
}

/** Append an event with a timestamp in frames (nframes_t) */
void
SMFSource::append_event_unlocked_frames (const Evoral::Event<nframes_t>& ev, sframes_t position)
{
	assert(_writing);
	if (ev.size() == 0)  {
		return;
	}

	/*printf("SMFSource: %s - append_event_unlocked_frames time = %u, size = %u, data = ",
			name().c_str(), ev.time(), ev.size());
	for (size_t i=0; i < ev.size(); ++i) printf("%X ", ev.buffer()[i]); printf("\n");*/

	if (ev.time() < _last_ev_time_frames) {
		cerr << "SMFSource: Warning: Skipping event with non-monotonic time" << endl;
		return;
	}

	BeatsFramesConverter converter(_session.tempo_map(), position);

	_length_beats = max(_length_beats, converter.from(ev.time()));

	const sframes_t delta_time_frames = ev.time() - _last_ev_time_frames;
	const double    delta_time_beats  = converter.from(delta_time_frames);
	const uint32_t  delta_time_ticks  = (uint32_t)(lrint(delta_time_beats * (double)ppqn()));

	Evoral::SMF::append_event_delta(delta_time_ticks, ev.size(), ev.buffer());
	_last_ev_time_frames = ev.time();

	_write_data_count += ev.size();

	if (_model) {
		const double ev_time_beats = converter.from(ev.time());
		const Evoral::Event<double> beat_ev(
				ev.event_type(), ev_time_beats, ev.size(), (uint8_t*)ev.buffer());
		_model->append (beat_ev);
	}
}

XMLNode&
SMFSource::get_state ()
{
	return MidiSource::get_state();
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
SMFSource::mark_streaming_midi_write_started (NoteMode mode, sframes_t start_frame)
{
	Glib::Mutex::Lock lm (_lock);
	MidiSource::mark_streaming_midi_write_started (mode, start_frame);
	Evoral::SMF::begin_write ();
	_last_ev_time_beats = 0.0;
	_last_ev_time_frames = 0;
}

void
SMFSource::mark_streaming_write_completed ()
{
	Glib::Mutex::Lock lm (_lock);
	MidiSource::mark_streaming_write_completed();

	if (!writable()) {
                cerr << "\n\n\n[[[[[[[[[ This SMFS is not writable! ]]]]]]]]]]]\n\n\n";
		return;
	}

	if (_model) {
		_model->set_edited(false);
	}
	
	Evoral::SMF::end_write ();
}

bool
SMFSource::safe_midi_file_extension (const Glib::ustring& file)
{
	return (file.rfind(".mid") != Glib::ustring::npos);
}

void
SMFSource::load_model (bool lock, bool force_reload)
{
	if (_writing) {
		return;
	}

	boost::shared_ptr<Glib::Mutex::Lock> lm;
	if (lock)
		lm = boost::shared_ptr<Glib::Mutex::Lock>(new Glib::Mutex::Lock(_lock));

	if (_model && !force_reload) {
		return;
	}

	if (! _model) {
		_model = boost::shared_ptr<MidiModel>(new MidiModel(this));
		//cerr << _name << " loaded new model " << _model.get() << endl;
	} else {
		/*cerr << _name << " reloading model " << _model.get()
			<< " (" << _model->n_notes() << " notes)" << endl;*/
		_model->clear();
	}

	_model->start_write();
	Evoral::SMF::seek_to_start();

	uint64_t time = 0; /* in SMF ticks */
	Evoral::Event<double> ev;

	size_t scratch_size = 0; // keep track of scratch and minimize reallocs

	uint32_t delta_t = 0;
	uint32_t size    = 0;
	uint8_t* buf     = NULL;
	int ret;
	while ((ret = read_event(&delta_t, &size, &buf)) >= 0) {
		time += delta_t;
		ev.set(buf, size, time / (double)ppqn());

		if (ret > 0) { // didn't skip (meta) event
			ev.set_event_type(EventTypeMap::instance().midi_event_type(buf[0]));

#ifndef NDEBUG
                        std::string ss;
                        
                        for (uint32_t xx = 0; xx < size; ++xx) {
                                char b[8];
                                snprintf (b, sizeof (b), "0x%x ", buf[xx]);
                                ss += b;
                        }

                        DEBUG_TRACE (DEBUG::MidiSourceIO, string_compose ("SMF %6 load model delta %1, time %2, size %3 buf %4, type %5\n",
                                                                          delta_t, time, size, ss , ev.event_type(), name()));
#endif
                        
			_model->append (ev);
		}

		if (ev.size() > scratch_size) {
			scratch_size = ev.size();
		}
		ev.size() = scratch_size; // ensure read_event only allocates if necessary

		_length_beats = max(_length_beats, ev.time());
	}

	set_default_controls_interpolation();

	_model->end_write(false);
	_model->set_edited(false);

	_model_iter = _model->begin();

	free(buf);
}

void
SMFSource::set_default_controls_interpolation ()
{
	// set interpolation style to defaults, can be changed by the GUI later
	Evoral::ControlSet::Controls controls = _model->controls();
	for (Evoral::ControlSet::Controls::iterator c = controls.begin(); c != controls.end(); ++c) {
		(*c).second->list()->set_interpolation(
			EventTypeMap::instance().interpolation_of((*c).first));
	}
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
        if (!writable()) {
                cerr << "\n\n\n\n " << name() << " CANNOT FLUSH - not writable\n\n\n\n";
                return;
        }

	Evoral::SMF::end_write();
}

