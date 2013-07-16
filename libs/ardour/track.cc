/*
    Copyright (C) 2006 Paul Davis

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
#include "pbd/error.h"

#include "ardour/amp.h"
#include "ardour/debug.h"
#include "ardour/delivery.h"
#include "ardour/diskstream.h"
#include "ardour/io_processor.h"
#include "ardour/meter.h"
#include "ardour/playlist.h"
#include "ardour/port.h"
#include "ardour/processor.h"
#include "ardour/route_group_specialized.h"
#include "ardour/session.h"
#include "ardour/session_playlists.h"
#include "ardour/track.h"
#include "ardour/utils.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

Track::Track (Session& sess, string name, Route::Flag flag, TrackMode mode, DataType default_type)
	: Route (sess, name, flag, default_type)
        , _saved_meter_point (_meter_point)
        , _mode (mode)
	, _monitoring (MonitorAuto)
{
	_freeze_record.state = NoFreeze;
        _declickable = true;
}

Track::~Track ()
{
	DEBUG_TRACE (DEBUG::Destruction, string_compose ("track %1 destructor\n", _name));
}

int
Track::init ()
{
        if (Route::init ()) {
                return -1;
        }

	boost::shared_ptr<Route> rp (shared_from_this());
	boost::shared_ptr<Track> rt = boost::dynamic_pointer_cast<Track> (rp);
	_rec_enable_control = boost::shared_ptr<RecEnableControl> (new RecEnableControl(rt));
	_rec_enable_control->set_flags (Controllable::Toggle);

	/* don't add rec_enable_control to controls because we don't want it to
	 * appear as an automatable parameter
	 */

        return 0;
}

void
Track::use_new_diskstream ()
{
	boost::shared_ptr<Diskstream> ds = create_diskstream ();

	ds->do_refill_with_alloc ();
	ds->set_block_size (_session.get_block_size ());
	ds->playlist()->set_orig_track_id (id());

	set_diskstream (ds);
}

XMLNode&
Track::get_state ()
{
	return state (true);
}

XMLNode&
Track::state (bool full)
{
	XMLNode& root (Route::state (full));
	root.add_property (X_("monitoring"), enum_2_string (_monitoring));
	root.add_property (X_("saved-meter-point"), enum_2_string (_saved_meter_point));
	root.add_child_nocopy (_rec_enable_control->get_state());
	root.add_child_nocopy (_diskstream->get_state ());
	
	return root;
}	

int
Track::set_state (const XMLNode& node, int version)
{
	if (Route::set_state (node, version)) {
		return -1;
	}

	XMLNode* child;

	if (version >= 3000) {
		if ((child = find_named_node (node, X_("Diskstream"))) != 0) {
			boost::shared_ptr<Diskstream> ds = diskstream_factory (*child);
			ds->do_refill_with_alloc ();
			set_diskstream (ds);
		}
	}

	if (_diskstream) {
		_diskstream->playlist()->set_orig_track_id (id());
	}

	/* set rec-enable control *AFTER* setting up diskstream, because it may
	   want to operate on the diskstream as it sets its own state
	*/

	XMLNodeList nlist = node.children();
	for (XMLNodeConstIterator niter = nlist.begin(); niter != nlist.end(); ++niter) {
		child = *niter;

		XMLProperty* prop;
		if (child->name() == Controllable::xml_node_name && (prop = child->property ("name")) != 0) {
			if (prop->value() == X_("recenable")) {
				_rec_enable_control->set_state (*child, version);
			}
		}
	}
	
	const XMLProperty* prop;

	if ((prop = node.property (X_("monitoring"))) != 0) {
		_monitoring = MonitorChoice (string_2_enum (prop->value(), _monitoring));
	} else {
		_monitoring = MonitorAuto;
	}

	if ((prop = node.property (X_("saved-meter-point"))) != 0) {
		_saved_meter_point = MeterPoint (string_2_enum (prop->value(), _saved_meter_point));
	} else {
		_saved_meter_point = _meter_point;
	}

	return 0;
}

XMLNode&
Track::get_template ()
{
	return state (false);
}

Track::FreezeRecord::~FreezeRecord ()
{
	for (vector<FreezeRecordProcessorInfo*>::iterator i = processor_info.begin(); i != processor_info.end(); ++i) {
		delete *i;
	}
}

Track::FreezeState
Track::freeze_state() const
{
	return _freeze_record.state;
}

Track::RecEnableControl::RecEnableControl (boost::shared_ptr<Track> t)
	: AutomationControl (t->session(), RecEnableAutomation, boost::shared_ptr<AutomationList>(), X_("recenable"))
	, track (t)
{
	boost::shared_ptr<AutomationList> gl(new AutomationList(Evoral::Parameter(RecEnableAutomation)));
	set_list (gl);
}

void
Track::RecEnableControl::set_value (double val)
{
	boost::shared_ptr<Track> t = track.lock ();
	if (!t) {
		return;
	}
	
	t->set_record_enabled (val >= 0.5 ? true : false, this);
}

double
Track::RecEnableControl::get_value () const
{
	boost::shared_ptr<Track> t = track.lock ();
	if (!t) {
		return 0;
	}
	
	return (t->record_enabled() ? 1.0 : 0.0);
}

bool
Track::record_enabled () const
{
	return _diskstream && _diskstream->record_enabled ();
}

bool
Track::can_record()
{
	bool will_record = true;
	for (PortSet::iterator i = _input->ports().begin(); i != _input->ports().end() && will_record; ++i) {
		if (!i->connected())
			will_record = false;
	}

	return will_record;
}

void
Track::prep_record_enabled (bool yn, void *src)
{
	if (!_session.writable()) {
		return;
	}

	if (_freeze_record.state == Frozen) {
		return;
	}

	if (_route_group && src != _route_group && _route_group->is_active() && _route_group->is_recenable()) {
		_route_group->apply (&Track::prep_record_enabled, yn, _route_group);
		return;
	}

	/* keep track of the meter point as it was before we rec-enabled */
	if (!_diskstream->record_enabled()) {
		_saved_meter_point = _meter_point;
	}

	bool will_follow;
	
	if (yn) {
		will_follow = _diskstream->prep_record_enable ();
	} else {
		will_follow = _diskstream->prep_record_disable ();
	}

	if (will_follow) {
		if (yn) {
			if (_meter_point != MeterCustom) {
				set_meter_point (MeterInput);
			}
		} else {
			set_meter_point (_saved_meter_point);
		}
	}
}

void
Track::set_record_enabled (bool yn, void *src)
{
	if (!_session.writable()) {
		return;
	}

	if (_freeze_record.state == Frozen) {
		return;
	}

	if (_route_group && src != _route_group && _route_group->is_active() && _route_group->is_recenable()) {
		_route_group->apply (&Track::set_record_enabled, yn, _route_group);
		return;
	}

	_diskstream->set_record_enabled (yn);

	_rec_enable_control->Changed ();
}

bool
Track::set_name (const string& str)
{
	bool ret;

	if (record_enabled() && _session.actively_recording()) {
		/* this messes things up if done while recording */
		return false;
	}

	boost::shared_ptr<Track> me = boost::dynamic_pointer_cast<Track> (shared_from_this ());
	if (_diskstream->playlist()->all_regions_empty () && _session.playlists->playlists_for_track (me).size() == 1) {
		/* Only rename the diskstream (and therefore the playlist) if
		   a) the playlist has never had a region added to it and
		   b) there is only one playlist for this track.

		   If (a) is not followed, people can get confused if, say,
		   they have notes about a playlist with a given name and then
		   it changes (see mantis #4759).

		   If (b) is not followed, we rename the current playlist and not
		   the other ones, which is a bit confusing (see mantis #4977).
		*/
		_diskstream->set_name (str);
	}

	/* save state so that the statefile fully reflects any filename changes */

	if ((ret = Route::set_name (str)) == 0) {
		_session.save_state ("");
	}

	return ret;
}

void
Track::set_latency_compensation (framecnt_t longest_session_latency)
{
	Route::set_latency_compensation (longest_session_latency);
	_diskstream->set_roll_delay (_roll_delay);
}

int
Track::no_roll (pframes_t nframes, framepos_t start_frame, framepos_t end_frame, bool session_state_changing)
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock, Glib::Threads::TRY_LOCK);

	if (!lm.locked()) {
		return 0;
	}

	bool can_record = _session.actively_recording ();

	/* no outputs? nothing to do ... what happens if we have sends etc. ? */

	if (n_outputs().n_total() == 0) {
		return 0;
	}

	/* not active ... do the minimum possible by just outputting silence */

	if (!_active) {
		silence (nframes);
		if (_meter_point == MeterInput && (_monitoring & MonitorInput || _diskstream->record_enabled())) {
			_meter->reset();
		}
		return 0;
	}

	if (session_state_changing) {
		if (_session.transport_speed() != 0.0f) {
			/* we're rolling but some state is changing (e.g. our diskstream contents)
			   so we cannot use them. Be silent till this is over. Don't declick.

			   XXX note the absurdity of ::no_roll() being called when we ARE rolling!
			*/
			passthru_silence (start_frame, end_frame, nframes, 0);
			return 0;
		}
		/* we're really not rolling, so we're either delivery silence or actually
		   monitoring, both of which are safe to do while session_state_changing is true.
		*/
	}

	_diskstream->check_record_status (start_frame, can_record);

	bool be_silent;

	if (_have_internal_generator) {
		/* since the instrument has no input streams,
		   there is no reason to send any signal
		   into the route.
		*/
		be_silent = true;

	} else {

		MonitorState const s = monitoring_state ();
		/* we are not rolling, so be silent even if we are monitoring disk, as there
		   will be no disk data coming in.
		*/
		switch (s) {
		case MonitoringSilence:
			/* if there is an instrument, be_silent should always
			   be false
			*/
			be_silent = (the_instrument_unlocked() == 0);
			break;
		case MonitoringDisk:
			be_silent = true;
			break;
		case MonitoringInput:
			be_silent = false;
			break;
		}
	}

	_amp->apply_gain_automation (false);

	/* if have_internal_generator, or .. */

	if (be_silent) {

		if (_meter_point == MeterInput) {
			/* still need input monitoring and metering */

			bool const track_rec = _diskstream->record_enabled ();
			bool const auto_input = _session.config.get_auto_input ();
			bool const software_monitor = Config->get_monitoring_model() == SoftwareMonitoring;
			bool const tape_machine_mode = Config->get_tape_machine_mode ();
			bool no_meter = false;

			if (!auto_input && !track_rec) {
				no_meter=true;
			}
			else if (!software_monitor && tape_machine_mode && !track_rec) {
				no_meter=true;
			}
			else if (!software_monitor && !tape_machine_mode && !track_rec && !auto_input) {
				no_meter=true;
			}

			if (no_meter) {
				_meter->reset();
				_input->process_input (boost::shared_ptr<Processor>(), start_frame, end_frame, nframes);
			} else {
				_input->process_input (_meter, start_frame, end_frame, nframes);
			}
		}

		passthru_silence (start_frame, end_frame, nframes, 0);

	} else {

		BufferSet& bufs = _session.get_scratch_buffers (n_process_buffers());
		
		fill_buffers_with_input (bufs, _input, nframes);

		if (_meter_point == MeterInput) {
			_meter->run (bufs, start_frame, end_frame, nframes, true);
		}

		passthru (bufs, start_frame, end_frame, nframes, false);
	}

	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		boost::shared_ptr<Delivery> d = boost::dynamic_pointer_cast<Delivery> (*i);
		if (d) {
			d->flush_buffers (nframes);
		}
	}

	return 0;
}

int
Track::silent_roll (pframes_t nframes, framepos_t /*start_frame*/, framepos_t /*end_frame*/, bool& need_butler)
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock, Glib::Threads::TRY_LOCK);
	if (!lm.locked()) {
		return 0;
	}

	if (n_outputs().n_total() == 0 && _processors.empty()) {
		return 0;
	}

	if (!_active) {
		silence (nframes);
		return 0;
	}

	_silent = true;
	_amp->apply_gain_automation(false);

	silence (nframes);

	framecnt_t playback_distance;

	BufferSet& bufs (_session.get_silent_buffers (n_process_buffers()));

	int const dret = _diskstream->process (bufs, _session.transport_frame(), nframes, playback_distance, false);
	need_butler = _diskstream->commit (playback_distance);
	return dret;
}

void
Track::set_diskstream (boost::shared_ptr<Diskstream> ds)
{
	_diskstream = ds;

	ds->PlaylistChanged.connect_same_thread (*this, boost::bind (&Track::diskstream_playlist_changed, this));
	diskstream_playlist_changed ();
	ds->RecordEnableChanged.connect_same_thread (*this, boost::bind (&Track::diskstream_record_enable_changed, this));
	ds->SpeedChanged.connect_same_thread (*this, boost::bind (&Track::diskstream_speed_changed, this));
	ds->AlignmentStyleChanged.connect_same_thread (*this, boost::bind (&Track::diskstream_alignment_style_changed, this));
}

void
Track::diskstream_playlist_changed ()
{
	PlaylistChanged (); /* EMIT SIGNAL */
}

void
Track::diskstream_record_enable_changed ()
{
	RecordEnableChanged (); /* EMIT SIGNAL */
}

void
Track::diskstream_speed_changed ()
{
	SpeedChanged (); /* EMIT SIGNAL */
}

void
Track::diskstream_alignment_style_changed ()
{
	AlignmentStyleChanged (); /* EMIT SIGNAL */
}

boost::shared_ptr<Playlist>
Track::playlist ()
{
	return _diskstream->playlist ();
}

void
Track::request_jack_monitors_input (bool m)
{
	_diskstream->request_jack_monitors_input (m);
}

void
Track::ensure_jack_monitors_input (bool m)
{
	_diskstream->ensure_jack_monitors_input (m);
}

bool
Track::destructive () const
{
	return _diskstream->destructive ();
}

list<boost::shared_ptr<Source> > &
Track::last_capture_sources ()
{
	return _diskstream->last_capture_sources ();
}

void
Track::set_capture_offset ()
{
	_diskstream->set_capture_offset ();
}

list<boost::shared_ptr<Source> >
Track::steal_write_sources()
{
        return _diskstream->steal_write_sources ();
}

void
Track::reset_write_sources (bool r, bool force)
{
	_diskstream->reset_write_sources (r, force);
}

float
Track::playback_buffer_load () const
{
	return _diskstream->playback_buffer_load ();
}

float
Track::capture_buffer_load () const
{
	return _diskstream->capture_buffer_load ();
}

int
Track::do_refill ()
{
	return _diskstream->do_refill ();
}

int
Track::do_flush (RunContext c, bool force)
{
	return _diskstream->do_flush (c, force);
}

void
Track::set_pending_overwrite (bool o)
{
	_diskstream->set_pending_overwrite (o);
}

int
Track::seek (framepos_t p, bool complete_refill)
{
	return _diskstream->seek (p, complete_refill);
}

bool
Track::hidden () const
{
	return _diskstream->hidden ();
}

int
Track::can_internal_playback_seek (framecnt_t p)
{
	return _diskstream->can_internal_playback_seek (p);
}

int
Track::internal_playback_seek (framecnt_t p)
{
	return _diskstream->internal_playback_seek (p);
}

void
Track::non_realtime_input_change ()
{
	_diskstream->non_realtime_input_change ();
}

void
Track::non_realtime_locate (framepos_t p)
{
	Route::non_realtime_locate (p);

	if (!hidden()) {
		/* don't waste i/o cycles and butler calls
		   for hidden (secret) tracks
		*/
		_diskstream->non_realtime_locate (p);
	}
}

void
Track::non_realtime_set_speed ()
{
	_diskstream->non_realtime_set_speed ();
}

int
Track::overwrite_existing_buffers ()
{
	return _diskstream->overwrite_existing_buffers ();
}

framecnt_t
Track::get_captured_frames (uint32_t n) const
{
	return _diskstream->get_captured_frames (n);
}

int
Track::set_loop (Location* l)
{
	return _diskstream->set_loop (l);
}

void
Track::transport_looped (framepos_t p)
{
	_diskstream->transport_looped (p);
}

bool
Track::realtime_set_speed (double s, bool g)
{
	return _diskstream->realtime_set_speed (s, g);
}

void
Track::transport_stopped_wallclock (struct tm & n, time_t t, bool g)
{
	_diskstream->transport_stopped_wallclock (n, t, g);
}

bool
Track::pending_overwrite () const
{
	return _diskstream->pending_overwrite ();
}

double
Track::speed () const
{
	return _diskstream->speed ();
}

void
Track::prepare_to_stop (framepos_t p)
{
	_diskstream->prepare_to_stop (p);
}

void
Track::set_slaved (bool s)
{
	_diskstream->set_slaved (s);
}

ChanCount
Track::n_channels ()
{
	return _diskstream->n_channels ();
}

framepos_t
Track::get_capture_start_frame (uint32_t n) const
{
	return _diskstream->get_capture_start_frame (n);
}

AlignStyle
Track::alignment_style () const
{
	return _diskstream->alignment_style ();
}

AlignChoice
Track::alignment_choice () const
{
	return _diskstream->alignment_choice ();
}

framepos_t
Track::current_capture_start () const
{
	return _diskstream->current_capture_start ();
}

framepos_t
Track::current_capture_end () const
{
	return _diskstream->current_capture_end ();
}

void
Track::playlist_modified ()
{
	_diskstream->playlist_modified ();
}

int
Track::use_playlist (boost::shared_ptr<Playlist> p)
{
	int ret = _diskstream->use_playlist (p);
	if (ret == 0) {
		p->set_orig_track_id (id());
	}
	return ret;
}

int
Track::use_copy_playlist ()
{
	int ret =  _diskstream->use_copy_playlist ();

	if (ret == 0) {
		_diskstream->playlist()->set_orig_track_id (id());
	}

	return ret;
}

int
Track::use_new_playlist ()
{
	int ret = _diskstream->use_new_playlist ();

	if (ret == 0) {
		_diskstream->playlist()->set_orig_track_id (id());
	}

	return ret;
}

void
Track::set_align_style (AlignStyle s, bool force)
{
	_diskstream->set_align_style (s, force);
}

void
Track::set_align_choice (AlignChoice s, bool force)
{
	_diskstream->set_align_choice (s, force);
}

bool
Track::using_diskstream_id (PBD::ID id) const
{
	return (id == _diskstream->id ());
}

void
Track::set_block_size (pframes_t n)
{
	Route::set_block_size (n);
	_diskstream->set_block_size (n);
}

void
Track::adjust_playback_buffering ()
{
        if (_diskstream) {
                _diskstream->adjust_playback_buffering ();
        }
}

void
Track::adjust_capture_buffering ()
{
        if (_diskstream) {
                _diskstream->adjust_capture_buffering ();
        }
}

MonitorState
Track::monitoring_state () const
{
	/* Explicit requests */
	
	if (_monitoring & MonitorInput) {
		return MonitoringInput;
	}
		
	if (_monitoring & MonitorDisk) {
		return MonitoringDisk;
	}

	/* This is an implementation of the truth table in doc/monitor_modes.pdf;
	   I don't think it's ever going to be too pretty too look at.
	*/

	bool const roll = _session.transport_rolling ();
	bool const track_rec = _diskstream->record_enabled ();
	bool const auto_input = _session.config.get_auto_input ();
	bool const software_monitor = Config->get_monitoring_model() == SoftwareMonitoring;
	bool const tape_machine_mode = Config->get_tape_machine_mode ();
	bool session_rec;

	/* I suspect that just use actively_recording() is good enough all the
	 * time, but just to keep the semantics the same as they were before
	 * sept 26th 2012, we differentiate between the cases where punch is
	 * enabled and those where it is not.
	 */

	if (_session.config.get_punch_in() || _session.config.get_punch_out()) {
		session_rec = _session.actively_recording ();
	} else {
		session_rec = _session.get_record_enabled();
	}

	if (track_rec) {

		if (!session_rec && roll && auto_input) {
			return MonitoringDisk;
		} else {
			return software_monitor ? MonitoringInput : MonitoringSilence;
		}

	} else {

		if (tape_machine_mode) {

			return MonitoringDisk;

		} else {

			if (!roll && auto_input) {
				return software_monitor ? MonitoringInput : MonitoringSilence;
			} else {
				return MonitoringDisk;
			}
			
		}
	}

	/* NOTREACHED */
	return MonitoringSilence;
}

void
Track::maybe_declick (BufferSet& bufs, framecnt_t nframes, int declick)
{
        /* never declick if there is an internal generator - we just want it to
           keep generating sound without interruption.

	   ditto if we are monitoring inputs.
        */

        if (_have_internal_generator || monitoring_choice() == MonitorInput) {
                return;
        }

        if (!declick) {
		declick = _pending_declick;
	}

	if (declick != 0) {
		Amp::declick (bufs, nframes, declick);
	}
}

framecnt_t
Track::check_initial_delay (framecnt_t nframes, framepos_t& transport_frame)
{
	if (_roll_delay > nframes) {

		_roll_delay -= nframes;
		silence_unlocked (nframes);
		/* transport frame is not legal for caller to use */
		return 0;

	} else if (_roll_delay > 0) {

		nframes -= _roll_delay;
		silence_unlocked (_roll_delay);
		transport_frame += _roll_delay;

		/* shuffle all the port buffers for things that lead "out" of this Route
		   to reflect that we just wrote _roll_delay frames of silence.
		*/

		Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
		for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
			boost::shared_ptr<IOProcessor> iop = boost::dynamic_pointer_cast<IOProcessor> (*i);
			if (iop) {
				iop->increment_port_buffer_offset (_roll_delay);
			}
		}
		_output->increment_port_buffer_offset (_roll_delay);

		_roll_delay = 0;

	}

	return nframes; 
}

void
Track::set_monitoring (MonitorChoice mc)
{
	if (mc !=  _monitoring) {
		_monitoring = mc;

		for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
			(*i)->monitoring_changed ();
		}

		MonitoringChanged (); /* EMIT SIGNAL */
	}
}

MeterState
Track::metering_state () const
{
	bool rv;
	if (_session.transport_rolling ()) {
		// audio_track.cc || midi_track.cc roll() runs meter IFF:
		rv = _meter_point == MeterInput && (_monitoring & MonitorInput || _diskstream->record_enabled());
	} else {
		// track no_roll() always metering if
		rv = _meter_point == MeterInput;
	}
	return rv ? MeteringInput : MeteringRoute;
}

