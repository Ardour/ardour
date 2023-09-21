/*
 * Copyright (C) 2001-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2018 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2019 Ben Loftis <ben@harrisonconsoles.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <glibmm/threads.h>

#include "pbd/error.h"

#include "ardour/amp.h"
#include "ardour/audio_port.h"
#include "ardour/audioengine.h"
#include "ardour/audioplaylist.h"
#include "ardour/audioregion.h"
#include "ardour/auditioner.h"
#include "ardour/data_type.h"
#include "ardour/delivery.h"
#include "ardour/disk_reader.h"
#include "ardour/disk_writer.h"
#include "ardour/midi_playlist.h"
#include "ardour/midi_region.h"
#include "ardour/plugin_insert.h"
#include "ardour/plugin_manager.h"
#include "ardour/profile.h"
#include "ardour/region_factory.h"
#include "ardour/route.h"
#include "ardour/session.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

#include "pbd/i18n.h"

Auditioner::Auditioner (Session& s)
	: Track (s, "auditioner", PresentationInfo::Auditioner)
	, current_sample (0)
	, length (0)
	, _seek_sample (-1)
	, _reload_synth (false)
	, _seeking (false)
	, _seek_complete (false)
	, via_monitor (false)
	, _midi_audition (false)
	, _queue_panic (false)
	, _import_position (0)
{
	_auditioning.store (0);
}

int
Auditioner::init ()
{
	if (Track::init ()) {
		return -1;
	}

	if (connect ()) {
		return -1;
	}

	_disk_writer->unset_flag (DiskIOProcessor::Recordable);

	use_new_playlist (DataType::MIDI);

	if (!audition_synth_info) {
		lookup_fallback_synth ();
	}

	_output->changed.connect_same_thread (*this, boost::bind (&Auditioner::output_changed, this, _1, _2));

	return 0;
}

Auditioner::~Auditioner ()
{
	unload_synth(true);
}

PluginInfoPtr
Auditioner::lookup_fallback_synth_plugin_info (std::string const& uri) const
{
	PluginManager& mgr (PluginManager::instance());
	PluginInfoList plugs;
	plugs = mgr.lv2_plugin_info();
	for (PluginInfoList::const_iterator i = plugs.begin (); i != plugs.end (); ++i) {
		if (uri == (*i)->unique_id){
			return (*i);
		}
	}
	return PluginInfoPtr ();
}

void
Auditioner::lookup_fallback_synth ()
{

	PluginInfoPtr nfo = lookup_fallback_synth_plugin_info ("http://gareus.org/oss/lv2/gmsynth");

	//GMsynth not found: fallback to Reasonable Synth
	if (!nfo) {
		nfo = lookup_fallback_synth_plugin_info ("https://community.ardour.org/node/7596");
		if (nfo) {
			warning << _("Falling back to Reasonable Synth for Midi Audition") << endmsg;
		}
	}

	if (!nfo) {
		warning << _("No synth for midi-audition found.") << endmsg;
		return;
	}

	set_audition_synth_info(nfo);
}

bool
Auditioner::load_synth ()
{
	if (!audition_synth_info) {
		lookup_fallback_synth ();
	}

	if (!audition_synth_info) {
		unload_synth (true);
		return false;
	}

	if (asynth && !_reload_synth) {
		asynth->deactivate ();
		asynth->activate ();
		_queue_panic = true;
		return true;
	}

	unload_synth (true);

	std::shared_ptr<Plugin> p = audition_synth_info->load (_session);
	if (p) {
		asynth = std::shared_ptr<Processor> (new PluginInsert (_session, *this, p));
	}

	if (asynth) {
		ProcessorStreams ps;
		asynth->set_owner (this);
		if (add_processor (asynth, PreFader, &ps, true)) {
			error << _("Failed to load synth for MIDI-Audition.") << endmsg;
		}

		Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
		if (configure_processors (&ps)) {
			error << _("Cannot setup auditioner processing flow.") << endmsg;
			unload_synth (true);
			return false;
		}
		_reload_synth = false;
	}
	return true;
}

void
Auditioner::unload_synth (bool need_lock)
{
	if (asynth) {
		asynth->drop_references ();
		remove_processor (asynth, NULL, need_lock);
	}
	asynth.reset ();
}

int
Auditioner::connect ()
{
	string left = Config->get_auditioner_output_left();
	string right = Config->get_auditioner_output_right();

	vector<string> outputs;
	_session.engine().get_physical_outputs (DataType::AUDIO, outputs);

	via_monitor = false;

	if (left.empty() || left == "default") {
		if (_session.monitor_out() && _session.monitor_out()->input()->audio (0)) {
			left = _session.monitor_out()->input()->audio (0)->name();
		} else {
			if (outputs.size() > 0) {
				left = outputs[0];
			}
		}
	}

	if (right.empty() || right == "default") {
		if (_session.monitor_out() && _session.monitor_out()->input()->audio (1)) {
			right = _session.monitor_out()->input()->audio (1)->name();
		} else {
			if (outputs.size() > 1) {
				right = outputs[1];
			}
		}
	}

	_output->disconnect (this);

	if (left.empty() && right.empty()) {
		if (_output->n_ports().n_audio() == 0) {
			/* ports not set up, so must be during startup */
			warning << _("no outputs available for auditioner - manual connection required") << endmsg;
		}
	} else {

		if (_output->n_ports().n_audio() == 0) {

			/* create (and connect) new ports */

			_main_outs->defer_pan_reset ();

			if (left.length()) {
				_output->add_port (left, this, DataType::AUDIO);
			}

			if (right.length()) {
				_output->add_port (right, this, DataType::AUDIO);
			}

			_main_outs->allow_pan_reset ();
			_main_outs->reset_panner ();

		} else {

			/* reconnect existing ports */

			std::shared_ptr<Port> oleft (_output->nth (0));
			std::shared_ptr<Port> oright (_output->nth (1));
			if (oleft) {
				oleft->connect (left);
			}
			if (oright) {
				oright->connect (right);
			}
		}

	}

	if (_session.monitor_out () && _output->connected_to (_session.monitor_out ()->input())) {
		via_monitor = true;
	}

	return 0;
}

DataType
Auditioner::data_type () const {
	if (_midi_audition) {
		return DataType::MIDI;
	} else {
		return DataType::AUDIO;
	}
}

int
Auditioner::roll (pframes_t nframes, samplepos_t start_sample, samplepos_t end_sample, bool& need_butler)
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock, Glib::Threads::TRY_LOCK);
	if (!lm.locked()) {
		return 0;
	}

	assert(_active);

	BufferSet& bufs = _session.get_route_buffers (n_process_buffers());

	if (_queue_panic) {
		MidiBuffer& mbuf (bufs.get_midi (0));
		_queue_panic = false;
		for (uint8_t chn = 0; chn < 0xf; ++chn) {
			uint8_t buf[3] = { ((uint8_t) (MIDI_CMD_CONTROL | chn)), ((uint8_t) MIDI_CTL_SUSTAIN), 0 };
			mbuf.push_back(0, Evoral::MIDI_EVENT, 3, buf);
			buf[1] = MIDI_CTL_ALL_NOTES_OFF;
			mbuf.push_back(0, Evoral::MIDI_EVENT, 3, buf);
			buf[1] = MIDI_CTL_RESET_CONTROLLERS;
			mbuf.push_back(0, Evoral::MIDI_EVENT, 3, buf);
		}
	}

	process_output_buffers (bufs, start_sample, end_sample, nframes, !_session.transport_stopped(), true);

	if (_midi_audition) {
		update_controls (bufs);
	}

	/* note: auditioner never writes to disk, so we don't care about the
	 * disk writer status (it's buffers will always have no data in them).
	 */

	if (_disk_reader->need_butler()) {
		need_butler = true;
	}

	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		std::shared_ptr<Delivery> d = std::dynamic_pointer_cast<Delivery> (*i);
		if (d) {
			d->flush_buffers (nframes);
		}
	}

	return 0;
}

void
Auditioner::update_controls (BufferSet const& bufs)
{
	const MidiBuffer& buf = bufs.get_midi(0);
	for (MidiBuffer::const_iterator e = buf.begin(); e != buf.end(); ++e) {
		const Evoral::Event<samplepos_t>& ev = *e;
		const uint8_t* buf = ev.buffer();
		const uint8_t channel = buf[0] & 0x0F;
		const int bank = _patch_change[channel].is_set () ? _patch_change[channel].bank () : 0;
		switch (midi_parameter_type (buf[0])) {
			case MidiPgmChangeAutomation:
				if (!_patch_change[channel].is_set ()) {
					_patch_change[channel] = Evoral::PatchChange<MidiBuffer::TimeType> (0, channel, 0, 0);
				}
				_patch_change[channel].set_program (ev.pgm_number ());
				break;
			case MidiCCAutomation:
				switch (ev.cc_number ()) {
					case MIDI_CTL_MSB_BANK:
						if (!_patch_change[channel].is_set ()) {
							_patch_change[channel] = Evoral::PatchChange<MidiBuffer::TimeType> (0, channel, 0, 0);
						}
						_patch_change[channel].set_bank ((bank & 0x007f) | (ev.cc_value () << 7));
						break;
					case MIDI_CTL_LSB_BANK:
						if (!_patch_change[channel].is_set ()) {
							_patch_change[channel] = Evoral::PatchChange<MidiBuffer::TimeType> (0, channel, 0, 0);
						}
						_patch_change[channel].set_bank ((bank & 0x3f80) | ev.cc_value ());
						break;
					default:
						break;
				}
				break;
			default:
				break;
		}
	}
}

void
Auditioner::audition_region (std::shared_ptr<Region> region, bool loop)
{
	if (_auditioning.load ()) {
		/* don't go via session for this, because we are going
		   to remain active.
		*/
		cancel_audition ();
	}

	_loop = loop;

	Glib::Threads::Mutex::Lock lm (lock);

	if (std::dynamic_pointer_cast<AudioRegion>(region) != 0) {

		_midi_audition = false;

		unload_synth (true);

		midi_region.reset();
		_import_position = timepos_t (Temporal::AudioTime);

		/* copy it */

		the_region = std::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (region, false));
		the_region->set_position (timepos_t (Temporal::AudioTime));

		_disk_reader->midi_playlist()->drop_regions ();

		_disk_reader->audio_playlist()->drop_regions ();
		_disk_reader->audio_playlist()->add_region (the_region, timepos_t (Temporal::AudioTime), 1);

		{
			ProcessorStreams ps;
			Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());

			if (configure_processors (&ps)) {
				error << string_compose (_("Cannot setup auditioner processing flow for %1 channels"),
				                         region->sources().size()) << endmsg;
				return;
			}
		}

	} else if (std::dynamic_pointer_cast<MidiRegion>(region)) {
		_midi_audition = true;

		the_region.reset();
		_import_position = region->position();

		/* copy it */
		midi_region = (std::dynamic_pointer_cast<MidiRegion> (RegionFactory::create (region, false)));
		midi_region->set_position (_import_position);

		/* avoid truncated notes: round up the length of midi regions to seconds, at least 2 seconds long */
		/* TODO:  maybe round up to the nearest bar like it's done in import.cc write_midi_data_to_new_files */
		samplecnt_t smpl = midi_region->length_samples();
		double seconds = smpl/_session.sample_rate();
		seconds = max (2.0, ceil(seconds));
		timecnt_t new_len( seconds * _session.sample_rate() );
		midi_region->set_length(new_len);

		_disk_reader->audio_playlist()->drop_regions();

		_disk_reader->midi_playlist()->drop_regions ();
		_disk_reader->midi_playlist()->add_region (midi_region, _import_position, 1);
		_disk_reader->reset_tracker();

		if (!load_synth ()) {
			return;
		}

	} else {
		error << _("Auditioning of regions other than Audio or Midi is not supported.") << endmsg;
		return;
	}

	/* force a panner reset now that we have all channels */
	_main_outs->reset_panner();

	_seek_sample = -1;
	_seeking = false;

	int dir;
	timepos_t offset;

	if (_midi_audition) {
		length = midi_region->length();
		offset = _import_position + midi_region->sync_offset (dir);
	} else {
		length = the_region->length();
		offset = the_region->sync_offset (dir);
	}

	if (length == 0) {
		error << _("Cannot audition empty file.") << endmsg;
		unload_synth (true);
		return;
	}

	/* can't audition from a negative sync point */

	if (dir < 0) {
		offset = timecnt_t (Temporal::AudioTime);
	}

	_disk_reader->seek (offset.samples(), true);

	if (_midi_audition) {
		/* Fill MIDI buffers.
		 * This is safe to call from here. ::::audition_region()
		 * is called by the butler thread. Also the session is not
		 * yet auditioning. So Session::non_realtime_overwrite()
		 * does call the auditioner's DR.
		 */
		set_pending_overwrite (PlaylistModified);
		_disk_reader->overwrite_existing_buffers ();
	}

	current_sample = offset.samples();

	_auditioning.store (1);
}

void
Auditioner::set_audition_synth_info(PluginInfoPtr in)
{
	if (audition_synth_info == in) {
		return;
	}
	audition_synth_info = in;
	_reload_synth = true;
}

int
Auditioner::play_audition (samplecnt_t nframes)
{
	bool need_butler = false;
	samplecnt_t this_nframes;
	int ret;

	if (_auditioning.load () == 0) {
		silence (nframes);
		if (_reload_synth) {
			unload_synth (false);
		}
		return 0;
	}

#if 0 // TODO
	if (_seeking && _seek_complete) {
		// set FADE-IN
	} else if (_seek_sample >= 0 && _seek_sample < length && !_seeking) {
		// set FADE-OUT -- use/override amp? || use region-gain ?
	}
#endif

	if (_seeking && _seek_complete) {
		_seek_complete = false;
		_seeking = false;
		_seek_sample = -1;
		if (_midi_audition) {
			/* Force MIDI note tracker to resolve any notes that are
			 * still playing -> set DR::run_must_resolve */
			_disk_reader->set_pending_overwrite (PlaylistModified);
			_disk_reader->overwrite_existing_buffers ();
		}
	}

	if(!_seeking) {
		/* process audio */
		this_nframes = min (nframes, length.samples() - current_sample + _import_position.samples());

		if (this_nframes > 0 && 0 != (ret = roll (this_nframes, current_sample, current_sample + this_nframes, need_butler))) {
			silence (nframes);
			return ret;
		}

		current_sample += this_nframes;

		if (this_nframes < nframes) {
			if (this_nframes > 0) {
				_session.engine().split_cycle (this_nframes);
			}
			silence (nframes - this_nframes);
		}

	} else {
		silence (nframes);
	}

	if (_seek_sample >= 0 && _seek_sample < length.samples() && !_seeking) {
		_queue_panic = true;
		_seek_complete = false;
		_seeking = true;
		need_butler = true;
	}

	if (!_seeking) {
		AuditionProgress(current_sample - _import_position.samples(), length.samples()); /* emit */
	}

	if (current_sample >= (length + _import_position).samples()) {
		if (_loop) {
			_seek_sample = 0;
			return 1;
		}
		_session.cancel_audition ();
		if (_reload_synth) {
			unload_synth (false);
		}
		return 0;
	} else {
		return need_butler ? 1 : 0;
	}
}

void
Auditioner::cancel_audition () {
	_auditioning.store (0);
}

bool
Auditioner::auditioning() const {
	return _auditioning.load ();
}

void
Auditioner::seek_to_sample (sampleoffset_t pos) {
	if (_seek_sample < 0 && !_seeking) {
		_seek_sample = pos;
	}
}

void
Auditioner::seek_to_percent (float const pos) {
	if (_seek_sample < 0 && !_seeking) {
		_seek_sample = floorf(length.samples() * pos / 100.0);
	}
}

void
Auditioner::seek_response (sampleoffset_t pos) {
	/* called from the butler thread */
	_seek_complete = true;
	if (_seeking) {
		current_sample = pos;
		_seek_complete = true;
	}
}

void
Auditioner::idle_synth_update ()
{
	if (auditioning() || !asynth) {
		return;
	}
	auto pi = std::dynamic_pointer_cast<PluginInsert> (asynth);

	/* Note: calling thread must have process buffers */
	BufferSet   bufs;
	samplepos_t start     = 0;
	pframes_t   n_samples = 16;
	/* MIDI buffers need to be able to hold patch/pgm change messages. (16 bytes + msg-size) per event */
	bufs.ensure_buffers (max (asynth->input_streams (), asynth->output_streams ()), std::max<size_t> (1024, n_samples));
	pi->run (bufs, start, start + n_samples, 1.0, n_samples, false);
	update_controls (bufs);
}

void
Auditioner::output_changed (IOChange change, void* /*src*/)
{
	if (0 == (change.type & IOChange::ConnectionsChanged)) {
		return;
	}
	if (_session.inital_connect_or_deletion_in_progress ()) {
		return;
	}
	if (_session.reconnection_in_progress ()) {
		return;
	}

	string phys;
	vector<string> connections;
	vector<string> outputs;
	_session.engine().get_physical_outputs (DataType::AUDIO, outputs);

	if (_session.monitor_out () && _output->connected_to (_session.monitor_out ()->input ())) {
		Config->set_auditioner_output_left ("default");
		Config->set_auditioner_output_right ("default");
		via_monitor = true;
		return;
	}

	if (_output->nth (0)->get_connections (connections)) {
		if (outputs.size() > 0) {
			phys = outputs[0];
		}
		if (phys != connections[0]) {
			Config->set_auditioner_output_left (connections[0]);
		} else {
			Config->set_auditioner_output_left ("default");
		}
	} else {
		Config->set_auditioner_output_left ("");
	}

	connections.clear ();

	if (_output->nth (1)->get_connections (connections)) {
		if (outputs.size() > 1) {
			phys = outputs[1];
		}
		if (phys != connections[0]) {
			Config->set_auditioner_output_right (connections[0]);
		} else {
			Config->set_auditioner_output_right ("default");
		}
	} else {
		Config->set_auditioner_output_right ("");
	}
}

ChanCount
Auditioner::input_streams () const
{
	/* auditioner never has any inputs - its channel configuration
	   depends solely on the region we are auditioning.
	*/

	if (_midi_audition) {
		return ChanCount (DataType::MIDI, 1);
	} else {
		if (the_region) {
			return ChanCount (DataType::AUDIO, the_region->n_channels ());
		}
	}

	return ChanCount (DataType::AUDIO, 1);
}

MonitorState
Auditioner::monitoring_state () const
{
	return MonitoringDisk;
}
