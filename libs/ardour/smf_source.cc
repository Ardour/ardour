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

#include "evoral/SMFReader.hpp"
#include "evoral/Control.hpp"

#include "ardour/audioengine.h"
#include "ardour/event_type_map.h"
#include "ardour/midi_model.h"
#include "ardour/midi_ring_buffer.h"
#include "ardour/session.h"
#include "ardour/smf_source.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace Glib;

/** Constructor used for new internal-to-session files.  File cannot exist. */
SMFSource::SMFSource (Session& s, const ustring& path, bool embedded, Source::Flag flags)
	: Source(s, DataType::MIDI, path, flags)
	, MidiSource(s, path)
	, FileSource(s, DataType::MIDI, path, embedded, flags)
	, Evoral::SMF()
	, _last_ev_time_beats(0.0)
	, _last_ev_time_frames(0)
	, _smf_last_read_end (0)
{
	if (init(_name, false)) {
		throw failed_constructor ();
	}

	if (create(path)) {
		throw failed_constructor ();
	}
}

/** Constructor used for existing internal-to-session files. */
SMFSource::SMFSource (Session& s, const XMLNode& node, bool must_exist)
	: Source(s, node)
	, MidiSource(s, node)
	, FileSource(s, node, must_exist)
	, _last_ev_time_beats(0.0)
	, _last_ev_time_frames(0)
	, _smf_last_read_end (0)
{
	if (set_state(node)) {
		throw failed_constructor ();
	}

	if (init(_name, true)) {
		throw failed_constructor ();
	}

	if (open(_path)) {
		throw failed_constructor ();
	}
}

SMFSource::~SMFSource ()
{
	if (removable()) {
		unlink (_path.c_str());
	}
}

/** All stamps in audio frames */
nframes_t
SMFSource::read_unlocked (MidiRingBuffer<nframes_t>& destination, sframes_t source_start,
		sframes_t start, nframes_t duration,
		sframes_t stamp_offset, sframes_t negative_stamp_offset) const
{
	int      ret  = 0;
	uint64_t time = 0; // in SMF ticks, 1 tick per _ppqn

	_read_data_count = 0;

	// Output parameters for read_event (which will allocate scratch in buffer as needed)
	uint32_t ev_delta_t = 0;
	uint32_t ev_type    = 0;
	uint32_t ev_size    = 0;
	uint8_t* ev_buffer  = 0;

	size_t scratch_size = 0; // keep track of scratch to minimize reallocs

	BeatsFramesConverter converter(_session, source_start);

	const uint64_t start_ticks = (uint64_t)(converter.from(start) * ppqn());

	if (_smf_last_read_end == 0 || start != _smf_last_read_end) {
		//cerr << "SMFSource::read_unlocked seeking to " << start << endl;
		Evoral::SMF::seek_to_start();
		while (time < start_ticks) {
			ret = read_event(&ev_delta_t, &ev_size, &ev_buffer);
			if (ret == -1) { // EOF
				_smf_last_read_end = start + duration;
				return duration;
			}
			time += ev_delta_t; // accumulate delta time
		}
	}

	_smf_last_read_end = start + duration;

	while (true) {
		ret = read_event(&ev_delta_t, &ev_size, &ev_buffer);
		if (ret == -1) { // EOF
			break;
		}

		time += ev_delta_t; // accumulate delta time

		if (ret == 0) { // meta-event (skipped, just accumulate time)
			continue;
		}

		ev_type = EventTypeMap::instance().midi_event_type(ev_buffer[0]);

#if 0
		cerr << "+++ SMF source read "
		     << " delta = " << ev_delta_t
		     << " time = " << time
		     << " buf[0] " << hex << (int) ev_buffer[0] << dec
		     << " type = " << ev_type;
#endif

		assert(time >= start_ticks);
		const sframes_t ev_frame_time = converter.to(time / (double)ppqn()) + stamp_offset;

#if 0
		cerr << " frames = " << ev_frame_time
		     << " w/offset = " << ev_frame_time - negative_stamp_offset
		     << endl;
#endif

		if (ev_frame_time < start + duration) {
			destination.write(ev_frame_time - negative_stamp_offset, ev_type, ev_size, ev_buffer);
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
			cerr << "SMFSource: WARNING: caller tried to write non SMF-Event of type "
					<< std::hex << int(ev.buffer()[0]) << endl;
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
		_model->append(ev);
	}
}

/** Append an event with a timestamp in frames (nframes_t) */
void
SMFSource::append_event_unlocked_frames (const Evoral::Event<nframes_t>& ev, sframes_t position)
{
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

	BeatsFramesConverter converter(_session, position);

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
		_model->append(beat_ev);
	}
}

XMLNode&
SMFSource::get_state ()
{
	return MidiSource::get_state();
}

int
SMFSource::set_state (const XMLNode& node)
{
	if (Source::set_state (node)) {
		return -1;
	}

	if (MidiSource::set_state (node)) {
		return -1;
	}

	if (FileSource::set_state (node)) {
		return -1;
	}

	return 0;
}

void
SMFSource::mark_streaming_midi_write_started (NoteMode mode, sframes_t start_frame)
{
	MidiSource::mark_streaming_midi_write_started (mode, start_frame);
	Evoral::SMF::begin_write ();
	_last_ev_time_beats = 0.0;
	_last_ev_time_frames = 0;
}

void
SMFSource::mark_streaming_write_completed ()
{
	MidiSource::mark_streaming_write_completed();

	if (!writable()) {
		return;
	}

	_model->set_edited(false);
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

	if (lock) {
		Glib::Mutex::Lock lm (_lock);
	}

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
			_model->append(ev);
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

#define LINEAR_INTERPOLATION_MODE_WORKS_PROPERLY 0

void
SMFSource::set_default_controls_interpolation ()
{
	// set interpolation style to defaults, can be changed by the GUI later
	Evoral::ControlSet::Controls controls = _model->controls();
	for (Evoral::ControlSet::Controls::iterator c = controls.begin(); c != controls.end(); ++c) {
		(*c).second->list()->set_interpolation(
			// to be enabled when ControlList::rt_safe_earliest_event_linear_unlocked works properly
			#if LINEAR_INTERPOLATION_MODE_WORKS_PROPERLY
			EventTypeMap::instance().interpolation_of((*c).first));
			#else
			Evoral::ControlList::Discrete);
			#endif
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
	Evoral::SMF::end_write();
}

