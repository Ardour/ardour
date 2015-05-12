/*
    Copyright (C) 2001 Paul Davis

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

#include <glibmm/threads.h>

#include "pbd/error.h"

#include "ardour/amp.h"
#include "ardour/audio_diskstream.h"
#include "ardour/audio_port.h"
#include "ardour/audioengine.h"
#include "ardour/audioplaylist.h"
#include "ardour/audioregion.h"
#include "ardour/auditioner.h"
#include "ardour/data_type.h"
#include "ardour/delivery.h"
#include "ardour/midi_diskstream.h"
#include "ardour/midi_region.h"
#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"
#include "ardour/profile.h"
#include "ardour/region_factory.h"
#include "ardour/route.h"
#include "ardour/session.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

#include "i18n.h"

Auditioner::Auditioner (Session& s)
	: Track (s, "auditioner", Route::Auditioner)
	, current_frame (0)
	, _auditioning (0)
	, length (0)
	, _seek_frame (-1)
	, _seeking (false)
	, _seek_complete (false)
	, via_monitor (false)
	, _midi_audition (false)
	, _synth_added (false)
	, _synth_changed (false)
	, _queue_panic (false)
	, _import_position (0)
{
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

	_output->add_port ("", this, DataType::MIDI);

	lookup_synth();

	_output->changed.connect_same_thread (*this, boost::bind (&Auditioner::output_changed, this, _1, _2));
	Config->ParameterChanged.connect_same_thread (*this, boost::bind (&Auditioner::config_changed, this, _1));

	return 0;
}

Auditioner::~Auditioner ()
{
}

void
Auditioner::lookup_synth ()
{
	string plugin_id = Config->get_midi_audition_synth_uri();
	asynth = boost::shared_ptr<Processor>();
	if (!plugin_id.empty()) {
		boost::shared_ptr<Plugin> p;
		p = find_plugin (_session, plugin_id, ARDOUR::LV2);
		if (!p) {
			p = find_plugin (_session, "https://community.ardour.org/node/7596", ARDOUR::LV2);
			if (p) {
				warning << _("Falling back to Reasonable Synth for Midi Audition") << endmsg;
			} else {
				warning << _("No synth for midi-audition found.") << endmsg;
				Config->set_midi_audition_synth_uri(""); // Don't check again for Reasonable Synth (ie --no-lv2)
			}
		}
		if (p) {
			asynth = boost::shared_ptr<Processor> (new PluginInsert (_session, p));
		}
	}
}

void
Auditioner::config_changed (std::string p)
{
	if (p == "midi-audition-synth-uri") {
		_synth_changed = true;
	}
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
                if (_session.monitor_out()) {
                        left = _session.monitor_out()->input()->audio (0)->name();
                        via_monitor = true;
                } else {
			if (outputs.size() > 0) {
				left = outputs[0];
			}
                }
	}

	if (right.empty() || right == "default") {
                if (_session.monitor_out()) {
                        right = _session.monitor_out()->input()->audio (1)->name();
                        via_monitor = true;
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

			boost::shared_ptr<Port> oleft (_output->nth (0));
			boost::shared_ptr<Port> oright (_output->nth (1));
			if (oleft) {
				oleft->connect (left);
			}
			if (oright) {
				oright->connect (right);
			}
		}
			
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

boost::shared_ptr<Diskstream>
Auditioner::create_diskstream () {

	{
		AudioDiskstream::Flag dflags = AudioDiskstream::Flag (0);
		dflags = AudioDiskstream::Flag (dflags | AudioDiskstream::Hidden);
		_diskstream_audio = boost::shared_ptr<AudioDiskstream> (new AudioDiskstream (_session, name(), dflags));
	}

	{
		MidiDiskstream::Flag dflags = MidiDiskstream::Flag (0);
		dflags = MidiDiskstream::Flag (dflags | MidiDiskstream::Hidden);
		_diskstream_midi = boost::shared_ptr<Diskstream> (new MidiDiskstream (_session, name(), dflags));
		_diskstream_midi->do_refill_with_alloc ();
		_diskstream_midi->playlist()->set_orig_track_id (id());
	}

	return _diskstream_audio;
}

int
Auditioner::roll (pframes_t nframes, framepos_t start_frame, framepos_t end_frame, int declick, bool& need_butler) {
	if (_midi_audition) {
		return roll_midi(nframes, start_frame, end_frame, declick, need_butler);
	} else {
		return roll_audio(nframes, start_frame, end_frame, declick, need_butler);
	}
}

int
Auditioner::roll_midi (pframes_t nframes, framepos_t start_frame, framepos_t end_frame, int declick, bool& need_butler)
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock, Glib::Threads::TRY_LOCK);
	if (!lm.locked()) {
		return 0;
	}

	assert(_active);

	framecnt_t playback_distance = nframes;
	boost::shared_ptr<MidiDiskstream> diskstream = midi_diskstream();
	BufferSet& bufs = _session.get_route_buffers (n_process_buffers());
	MidiBuffer& mbuf (bufs.get_midi (0));
	_silent = false;

	ChanCount cnt (DataType::MIDI, 1);
	cnt.set (DataType::AUDIO, bufs.count().n_audio());
	bufs.set_count (cnt);

	if (_queue_panic) {
		_queue_panic = false;
		for (uint8_t chn = 0; chn < 0xf; ++chn) {
			uint8_t buf[3] = { ((uint8_t) (MIDI_CMD_CONTROL | chn)), ((uint8_t) MIDI_CTL_SUSTAIN), 0 };
			mbuf.push_back(0, 3, buf);
			buf[1] = MIDI_CTL_ALL_NOTES_OFF;
			mbuf.push_back(0, 3, buf);
			buf[1] = MIDI_CTL_RESET_CONTROLLERS;
			mbuf.push_back(0, 3, buf);
		}
		process_output_buffers (bufs, start_frame, start_frame+1, 1, false, false);

		for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
			boost::shared_ptr<Delivery> d = boost::dynamic_pointer_cast<Delivery> (*i);
			if (d) {
				d->flush_buffers (nframes);
			}
		}
	}

	diskstream->get_playback (mbuf, nframes);

	process_output_buffers (bufs, start_frame, end_frame, nframes,
				declick, (!diskstream->record_enabled() && !_session.transport_stopped()));

	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		boost::shared_ptr<Delivery> d = boost::dynamic_pointer_cast<Delivery> (*i);
		if (d) {
			d->flush_buffers (nframes);
		}
	}

	need_butler = diskstream->commit (playback_distance);
	return 0;
}


int
Auditioner::roll_audio (pframes_t nframes, framepos_t start_frame, framepos_t end_frame, int declick, bool& need_butler) {
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock, Glib::Threads::TRY_LOCK);
	if (!lm.locked()) {
		return 0;
	}

	assert(n_outputs().n_total() > 0);
	assert(_active);

	int dret;
	framecnt_t playback_distance;
	framepos_t transport_frame = _session.transport_frame();
	boost::shared_ptr<AudioDiskstream> diskstream = audio_diskstream();
	BufferSet& bufs = _session.get_route_buffers (n_process_buffers ());

	_silent = false;
	_amp->apply_gain_automation(false);

	if ((dret = diskstream->process (bufs, transport_frame, nframes, playback_distance, (monitoring_state() == MonitoringDisk))) != 0) {
		need_butler = diskstream->commit (playback_distance);
		silence (nframes);
		return dret;
	}

	process_output_buffers (bufs, start_frame, end_frame, nframes, declick, (!diskstream->record_enabled() && _session.transport_rolling()));
	need_butler = diskstream->commit (playback_distance);
	return 0;
}

void
Auditioner::set_diskstream (boost::shared_ptr<Diskstream> ds)
{
	Track::set_diskstream (ds);

	_diskstream->set_track (this);
	if (Profile->get_trx()) {
		_diskstream->set_destructive (false);
	} else {
		_diskstream->set_destructive (_mode == Destructive);
	}
	_diskstream->set_non_layered (_mode == NonLayered);
	_diskstream->set_record_enabled (false);
	_diskstream->request_input_monitoring (false);

	DiskstreamChanged (); /* EMIT SIGNAL */
}

AudioPlaylist&
Auditioner::prepare_playlist ()
{
	// used by CrossfadeEditor::audition()

	_midi_audition = false;
	set_diskstream(_diskstream_audio);
	if (_synth_added) {
		remove_processor(asynth);
		_synth_added = false;
	}

	// FIXME auditioner is still audio-only
	boost::shared_ptr<AudioPlaylist> apl = boost::dynamic_pointer_cast<AudioPlaylist>(_diskstream->playlist());
	assert(apl);

	apl->clear ();
	return *apl;
}

void
Auditioner::audition_region (boost::shared_ptr<Region> region)
{
	if (g_atomic_int_get (&_auditioning)) {
		/* don't go via session for this, because we are going
		   to remain active.
		*/
		cancel_audition ();
	}

	Glib::Threads::Mutex::Lock lm (lock);

	if (boost::dynamic_pointer_cast<AudioRegion>(region) != 0) {

		_midi_audition = false;
		set_diskstream(_diskstream_audio);
		if (_synth_added) {
			remove_processor(asynth);
			_synth_added = false;
		}
		midi_region.reset();
		_import_position = 0;

		/* copy it */
		the_region = boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (region));
		the_region->set_position (0);

		_diskstream->playlist()->drop_regions ();
		_diskstream->playlist()->add_region (the_region, 0, 1);

		if (_diskstream->n_channels().n_audio() < the_region->n_channels()) {
			audio_diskstream()->add_channel (the_region->n_channels() - _diskstream->n_channels().n_audio());
		} else if (_diskstream->n_channels().n_audio() > the_region->n_channels()) {
			audio_diskstream()->remove_channel (_diskstream->n_channels().n_audio() - the_region->n_channels());
		}

		ProcessorStreams ps;
		{
			Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());

			if (configure_processors (&ps)) {
				error << string_compose (_("Cannot setup auditioner processing flow for %1 channels"),
							 _diskstream->n_channels()) << endmsg;
				return;
			}
		}

	} else if (boost::dynamic_pointer_cast<MidiRegion>(region)) {
		_midi_audition = true;
		set_diskstream(_diskstream_midi);
		the_region.reset();
		_import_position = region->position();

		/* copy it */
		midi_region = (boost::dynamic_pointer_cast<MidiRegion> (RegionFactory::create (region)));
		midi_region->set_position (_import_position);

		_diskstream->playlist()->drop_regions ();
		_diskstream->playlist()->add_region (midi_region, _import_position, 1);
		midi_diskstream()->reset_tracker();

		ProcessorStreams ps;

		if (_synth_changed && _synth_added) {
			remove_processor(asynth);
			_synth_added = false;
		}
		if (_synth_changed && !_synth_added) {
			_synth_added = false;
			lookup_synth();
		}


		if (!_synth_added && asynth) {
			int rv = add_processor (asynth, PreFader, &ps, true);
			if (rv) {
				error << _("Failed to load synth for MIDI-Audition.") << endmsg;
			} else {
				_synth_added = true;
			}
		} else {
			_queue_panic = true;
		}

		{
			Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());

			if (configure_processors (&ps)) {
				error << string_compose (_("Cannot setup auditioner processing flow for %1 channels"),
							 _diskstream->n_channels()) << endmsg;
				return;
			}
		}

	} else {
		error << _("Auditioning of regions other than Audio or Midi is not supported.") << endmsg;
		return;
	}

	/* force a panner reset now that we have all channels */
	_main_outs->reset_panner();

	_seek_frame = -1;
	_seeking = false;

	int dir;
	framecnt_t offset;

	if (_midi_audition) {
		length = midi_region->length();
		offset = _import_position + midi_region->sync_offset (dir);
	} else {
		length = the_region->length();
		offset = the_region->sync_offset (dir);
	}

	/* can't audition from a negative sync point */

	if (dir < 0) {
		offset = 0;
	}

	_diskstream->seek (offset);
	current_frame = offset;

	g_atomic_int_set (&_auditioning, 1);
}

int
Auditioner::play_audition (framecnt_t nframes)
{
	bool need_butler = false;
	framecnt_t this_nframes;
	int ret;

	if (g_atomic_int_get (&_auditioning) == 0) {
		silence (nframes);
		return 0;
	}

#if 0 // TODO
	if (_seeking && _seek_complete) {
		// set FADE-IN
	} else if (_seek_frame >= 0 && _seek_frame < length && !_seeking) {
		// set FADE-OUT -- use/override amp? || use region-gain ?
	}
#endif

	if (_seeking && _seek_complete) {
		_seek_complete = false;
		_seeking = false;
		_seek_frame = -1;
		if (_midi_audition && midi_diskstream()) {
			midi_diskstream()->reset_tracker();
		}
	}

	if(!_seeking) {
		/* process audio */
		this_nframes = min (nframes, length - current_frame + _import_position);

		if ((ret = roll (this_nframes, current_frame, current_frame + nframes, false, need_butler)) != 0) {
			silence (nframes);
			return ret;
		}

		current_frame += this_nframes;

	} else {
		silence (nframes);
	}

	if (_seek_frame >= 0 && _seek_frame < length && !_seeking) {
		_queue_panic = true;
		_seek_complete = false;
		_seeking = true;
		need_butler = true;
	}

	if (!_seeking) {
		AuditionProgress(current_frame - _import_position, length); /* emit */
	}

	if (current_frame >= length + _import_position) {
		_session.cancel_audition ();
		return 0;
	} else {
		return need_butler ? 1 : 0;
	}
}

void
Auditioner::output_changed (IOChange change, void* /*src*/)
{
	if (change.type & IOChange::ConnectionsChanged) {
		string phys;
		vector<string> connections;
		vector<string> outputs;
		_session.engine().get_physical_outputs (DataType::AUDIO, outputs);
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
}

ChanCount
Auditioner::input_streams () const
{
	/* auditioner never has any inputs - its channel configuration
		 depends solely on the region we are auditioning.
		 */

	if (!_midi_audition && audio_diskstream()) {
		return audio_diskstream()->n_channels();
	}
	if (_midi_audition && midi_diskstream()) {
		ChanCount cnt (DataType::MIDI, 1);
		return cnt;
	}

	return ChanCount ();
}

MonitorState
Auditioner::monitoring_state () const
{
	return MonitoringDisk;
}

boost::shared_ptr<AudioDiskstream>
Auditioner::audio_diskstream() const
{
	return boost::dynamic_pointer_cast<AudioDiskstream> (_diskstream);
}

boost::shared_ptr<MidiDiskstream>
Auditioner::midi_diskstream() const
{
	return boost::dynamic_pointer_cast<MidiDiskstream> (_diskstream);
}
