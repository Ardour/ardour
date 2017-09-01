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
#include <cmath>

#ifdef COMPILER_MSVC
#include <float.h>

// 'std::isinf()' and 'std::isnan()' are not available in MSVC.
#define isinf_local(val) !((bool)_finite((double)val))
#define isnan_local(val) (bool)_isnan((double)val)
#else
#define isinf_local std::isinf
#define isnan_local std::isnan
#endif

#include "pbd/enumwriter.h"
#include "pbd/types_convert.h"
#include "evoral/midi_util.h"

#include "ardour/amp.h"
#include "ardour/beats_frames_converter.h"
#include "ardour/buffer_set.h"
#include "ardour/debug.h"
#include "ardour/delivery.h"
#include "ardour/disk_reader.h"
#include "ardour/disk_writer.h"
#include "ardour/event_type_map.h"
#include "ardour/meter.h"
#include "ardour/midi_playlist.h"
#include "ardour/midi_port.h"
#include "ardour/midi_region.h"
#include "ardour/midi_track.h"
#include "ardour/monitor_control.h"
#include "ardour/parameter_types.h"
#include "ardour/port.h"
#include "ardour/processor.h"
#include "ardour/profile.h"
#include "ardour/route_group_specialized.h"
#include "ardour/session.h"
#include "ardour/session_playlists.h"
#include "ardour/types_convert.h"
#include "ardour/utils.h"

#include "pbd/i18n.h"

namespace ARDOUR {
class InterThreadInfo;
class MidiSource;
class Region;
class SMFSource;
}

using namespace std;
using namespace ARDOUR;
using namespace PBD;

MidiTrack::MidiTrack (Session& sess, string name, TrackMode mode)
	: Track (sess, name, PresentationInfo::MidiTrack, mode, DataType::MIDI)
	, _immediate_events(6096) // FIXME: size?
	, _step_edit_ring_buffer(64) // FIXME: size?
	, _note_mode (Sustained)
	, _step_editing (false)
	, _input_active (true)
{
	_session.SessionLoaded.connect_same_thread (*this, boost::bind (&MidiTrack::restore_controls, this));

	_disk_writer->set_note_mode (_note_mode);
	_disk_reader->reset_tracker ();

}

MidiTrack::~MidiTrack ()
{
}

int
MidiTrack::init ()
{
	if (Track::init ()) {
		return -1;
	}

	_input->changed.connect_same_thread (*this, boost::bind (&MidiTrack::track_input_active, this, _1, _2));

	return 0;
}

bool
MidiTrack::can_be_record_safe ()
{
	if (_step_editing) {
		return false;
	}

	return Track::can_be_record_safe ();
}

bool
MidiTrack::can_be_record_enabled ()
{
	if (_step_editing) {
		return false;
	}

	return Track::can_be_record_enabled ();
}

int
MidiTrack::set_state (const XMLNode& node, int version)
{
	/* This must happen before Track::set_state(), as there will be a buffer
	   fill during that call, and we must fill buffers using the correct
	   _note_mode.
	*/
	if (!node.get_property (X_("note-mode"), _note_mode)) {
		_note_mode = Sustained;
	}

	if (Track::set_state (node, version)) {
		return -1;
	}

	// No destructive MIDI tracks (yet?)
	_mode = Normal;

	bool yn;
	if (node.get_property ("input-active", yn)) {
		set_input_active (yn);
	}

	ChannelMode playback_channel_mode = AllChannels;
	ChannelMode capture_channel_mode = AllChannels;

	node.get_property ("playback-channel-mode", playback_channel_mode);
	node.get_property ("capture-channel-mode", capture_channel_mode);

	if (node.get_property ("channel-mode", playback_channel_mode)) {
		/* 3.0 behaviour where capture and playback modes were not separated */
		capture_channel_mode = playback_channel_mode;
	}

	XMLProperty const * prop;

	unsigned int playback_channel_mask = 0xffff;
	unsigned int capture_channel_mask = 0xffff;

	if ((prop = node.property ("playback-channel-mask")) != 0) {
		sscanf (prop->value().c_str(), "0x%x", &playback_channel_mask);
	}
	if ((prop = node.property ("capture-channel-mask")) != 0) {
		sscanf (prop->value().c_str(), "0x%x", &capture_channel_mask);
	}
	if ((prop = node.property ("channel-mask")) != 0) {
		sscanf (prop->value().c_str(), "0x%x", &playback_channel_mask);
		capture_channel_mask = playback_channel_mask;
	}

	set_playback_channel_mode (playback_channel_mode, playback_channel_mask);
	set_capture_channel_mode (capture_channel_mode, capture_channel_mask);

	pending_state = const_cast<XMLNode*> (&node);

	if (_session.state_of_the_state() & Session::Loading) {
		_session.StateReady.connect_same_thread (
			*this, boost::bind (&MidiTrack::set_state_part_two, this));
	} else {
		set_state_part_two ();
	}

	return 0;
}

XMLNode&
MidiTrack::state(bool full_state)
{
	XMLNode& root (Track::state(full_state));
	XMLNode* freeze_node;
	char buf[64];

	if (_freeze_record.playlist) {
		XMLNode* inode;

		freeze_node = new XMLNode (X_("freeze-info"));
		freeze_node->set_property ("playlist", _freeze_record.playlist->name());
		freeze_node->set_property ("state", _freeze_record.state);

		for (vector<FreezeRecordProcessorInfo*>::iterator i = _freeze_record.processor_info.begin(); i != _freeze_record.processor_info.end(); ++i) {
			inode = new XMLNode (X_("processor"));
			inode->set_property (X_("id"), id());
			inode->add_child_copy ((*i)->state);

			freeze_node->add_child_nocopy (*inode);
		}

		root.add_child_nocopy (*freeze_node);
	}

	root.set_property("playback-channel-mode", get_playback_channel_mode());
	root.set_property("capture-channel-mode", get_capture_channel_mode());
	snprintf (buf, sizeof(buf), "0x%x", get_playback_channel_mask());
	root.set_property("playback-channel-mask", std::string(buf));
	snprintf (buf, sizeof(buf), "0x%x", get_capture_channel_mask());
	root.set_property("capture-channel-mask", std::string(buf));

	root.set_property ("note-mode", _note_mode);
	root.set_property ("step-editing", _step_editing);
	root.set_property ("input-active", _input_active);

	for (Controls::const_iterator c = _controls.begin(); c != _controls.end(); ++c) {
		if (boost::dynamic_pointer_cast<MidiTrack::MidiControl>(c->second)) {
			boost::shared_ptr<AutomationControl> ac = boost::dynamic_pointer_cast<AutomationControl> (c->second);
			assert (ac);
			root.add_child_nocopy (ac->get_state ());
		}
	}

	return root;
}

void
MidiTrack::set_state_part_two ()
{
	XMLNode* fnode;

	/* This is called after all session state has been restored but before
	   have been made ports and connections are established.
	*/

	if (pending_state == 0) {
		return;
	}

	if ((fnode = find_named_node (*pending_state, X_("freeze-info"))) != 0) {

		_freeze_record.state = Frozen;

		for (vector<FreezeRecordProcessorInfo*>::iterator i = _freeze_record.processor_info.begin(); i != _freeze_record.processor_info.end(); ++i) {
			delete *i;
		}
		_freeze_record.processor_info.clear ();

		std::string str;
		if (fnode->get_property (X_("playlist"), str)) {
			boost::shared_ptr<Playlist> pl = _session.playlists->by_name (str);
			if (pl) {
				_freeze_record.playlist = boost::dynamic_pointer_cast<MidiPlaylist> (pl);
			} else {
				_freeze_record.playlist.reset();
				_freeze_record.state = NoFreeze;
				return;
			}
		}

		fnode->get_property (X_("state"), _freeze_record.state);

		XMLNodeConstIterator citer;
		XMLNodeList clist = fnode->children();

		for (citer = clist.begin(); citer != clist.end(); ++citer) {
			if ((*citer)->name() != X_("processor")) {
				continue;
			}

			if (!(*citer)->get_property (X_("id"), str)) {
				continue;
			}

			FreezeRecordProcessorInfo* frii = new FreezeRecordProcessorInfo (*((*citer)->children().front()),
										   boost::shared_ptr<Processor>());
			frii->id = str;
			_freeze_record.processor_info.push_back (frii);
		}
	}

	return;
}

void
MidiTrack::restore_controls ()
{
	// TODO order events (CC before PGM to set banks)
	for (Controls::const_iterator c = _controls.begin(); c != _controls.end(); ++c) {
		boost::shared_ptr<MidiTrack::MidiControl> mctrl = boost::dynamic_pointer_cast<MidiTrack::MidiControl>(c->second);
		if (mctrl) {
			mctrl->restore_value();
		}
	}
}

void
MidiTrack::update_controls(const BufferSet& bufs)
{
	const MidiBuffer& buf = bufs.get_midi(0);
	for (MidiBuffer::const_iterator e = buf.begin(); e != buf.end(); ++e) {
		const Evoral::Event<framepos_t>&         ev      = *e;
		const Evoral::Parameter                  param   = midi_parameter(ev.buffer(), ev.size());
		const boost::shared_ptr<Evoral::Control> control = this->control(param);
		if (control) {
			control->set_double(ev.value(), _session.transport_frame(), false);
		}
	}
}

/** @param need_butler to be set to true if this track now needs the butler, otherwise it can be left alone
 *  or set to false.
 */
int
MidiTrack::roll (pframes_t nframes, framepos_t start_frame, framepos_t end_frame, int declick, bool& need_butler)
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
		if (_meter_point == MeterInput && ((_monitoring_control->monitoring_choice() & MonitorInput) || _disk_writer->record_enabled())) {
			_meter->reset();
		}
		return 0;
	}

	_silent = false;
	_amp->apply_gain_automation (false);

	BufferSet& bufs = _session.get_route_buffers (n_process_buffers());

	fill_buffers_with_input (bufs, _input, nframes);

	/* filter captured data before meter sees it */
	_capture_filter.filter (bufs);

	if (_meter_point == MeterInput && ((_monitoring_control->monitoring_choice() & MonitorInput) || _disk_writer->record_enabled())) {
		_meter->run (bufs, start_frame, end_frame, 1.0 /*speed()*/, nframes, true);
	}

	/* append immediate messages to the first MIDI buffer (thus sending it to the first output port) */

	write_out_of_band_data (bufs, start_frame, end_frame, nframes);

	/* final argument: don't waste time with automation if we're not recording or rolling */

	process_output_buffers (bufs, start_frame, end_frame, nframes, declick, (!_disk_writer->record_enabled() && !_session.transport_stopped()));

	flush_processor_buffers_locked (nframes);

	return 0;
}

int
MidiTrack::no_roll (pframes_t nframes, framepos_t start_frame, framepos_t end_frame, bool state_changing)
{
	int ret = Track::no_roll (nframes, start_frame, end_frame, state_changing);

	if (ret == 0 && _step_editing) {
		push_midi_input_to_step_edit_ringbuffer (nframes);
	}

	return ret;
}

void
MidiTrack::realtime_locate ()
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock, Glib::Threads::TRY_LOCK);

	if (!lm.locked ()) {
		return;
	}

	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		(*i)->realtime_locate ();
	}

	_disk_reader->reset_tracker ();
}

void
MidiTrack::realtime_handle_transport_stopped ()
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock, Glib::Threads::TRY_LOCK);

	if (!lm.locked ()) {
		return;
	}

	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		(*i)->realtime_handle_transport_stopped ();
	}
}

void
MidiTrack::non_realtime_locate (framepos_t pos)
{
	Track::non_realtime_locate(pos);

	boost::shared_ptr<MidiPlaylist> playlist = _disk_writer->midi_playlist();
	if (!playlist) {
		return;
	}

	/* Get the top unmuted region at this position. */
	boost::shared_ptr<MidiRegion> region = boost::dynamic_pointer_cast<MidiRegion>(
		playlist->top_unmuted_region_at(pos));
	if (!region) {
		return;
	}

	/* the source may be missing, but the control still referenced in the GUI */
	if (!region->midi_source() || !region->model()) {
		return;
	}

	Glib::Threads::Mutex::Lock lm (_control_lock, Glib::Threads::TRY_LOCK);
	if (!lm.locked()) {
		return;
	}

	/* Update track controllers based on its "automation". */
	const framepos_t     origin = region->position() - region->start();
	BeatsFramesConverter bfc(_session.tempo_map(), origin);
	for (Controls::const_iterator c = _controls.begin(); c != _controls.end(); ++c) {
		boost::shared_ptr<MidiTrack::MidiControl> tcontrol;
		boost::shared_ptr<Evoral::Control>        rcontrol;
		if ((tcontrol = boost::dynamic_pointer_cast<MidiTrack::MidiControl>(c->second)) &&
		    (rcontrol = region->control(tcontrol->parameter()))) {
			const Evoral::Beats pos_beats = bfc.from(pos - origin);
			if (rcontrol->list()->size() > 0) {
				tcontrol->set_value(rcontrol->list()->eval(pos_beats.to_double()), Controllable::NoGroup);
			}
		}
	}
}

void
MidiTrack::push_midi_input_to_step_edit_ringbuffer (framecnt_t nframes)
{
	PortSet& ports (_input->ports());

	for (PortSet::iterator p = ports.begin(DataType::MIDI); p != ports.end(DataType::MIDI); ++p) {

		Buffer& b (p->get_buffer (nframes));
		const MidiBuffer* const mb = dynamic_cast<MidiBuffer*>(&b);
		assert (mb);

		for (MidiBuffer::const_iterator e = mb->begin(); e != mb->end(); ++e) {

			const Evoral::Event<framepos_t> ev(*e, false);

			/* note on, since for step edit, note length is determined
			   elsewhere
			*/

			if (ev.is_note_on()) {
				/* we don't care about the time for this purpose */
				_step_edit_ring_buffer.write (0, ev.event_type(), ev.size(), ev.buffer());
			}
		}
	}
}

void
MidiTrack::write_out_of_band_data (BufferSet& bufs, framepos_t /*start*/, framepos_t /*end*/, framecnt_t nframes)
{
	MidiBuffer& buf (bufs.get_midi (0));

	update_controls (bufs);

	// Append immediate events

	if (_immediate_events.read_space()) {

		DEBUG_TRACE (DEBUG::MidiIO, string_compose ("%1 has %2 of immediate events to deliver\n",
		                                            name(), _immediate_events.read_space()));

		/* write as many of the immediate events as we can, but give "true" as
		 * the last argument ("stop on overflow in destination") so that we'll
		 * ship the rest out next time.
		 *
		 * the Port::port_offset() + (nframes-1) argument puts all these events at the last
		 * possible position of the output buffer, so that we do not
		 * violate monotonicity when writing. Port::port_offset() will
		 * be non-zero if we're in a split process cycle.
		 */
		_immediate_events.read (buf, 0, 1, Port::port_offset() + nframes - 1, true);
	}
}

int
MidiTrack::export_stuff (BufferSet&                   buffers,
                         framepos_t                   start,
                         framecnt_t                   nframes,
                         boost::shared_ptr<Processor> endpoint,
                         bool                         include_endpoint,
                         bool                         for_export,
                         bool                         for_freeze)
{
	if (buffers.count().n_midi() == 0) {
		return -1;
	}

	Glib::Threads::RWLock::ReaderLock rlock (_processor_lock);

	boost::shared_ptr<MidiPlaylist> mpl = _disk_writer->midi_playlist();
	if (!mpl) {
		return -2;
	}

	buffers.get_midi(0).clear();
	if (mpl->read(buffers.get_midi(0), start, nframes, 0) != nframes) {
		return -1;
	}

	//bounce_process (buffers, start, nframes, endpoint, include_endpoint, for_export, for_freeze);

	return 0;
}

boost::shared_ptr<Region>
MidiTrack::bounce (InterThreadInfo& itt)
{
	return bounce_range (_session.current_start_frame(), _session.current_end_frame(), itt, main_outs(), false);
}

boost::shared_ptr<Region>
MidiTrack::bounce_range (framepos_t                   start,
                         framepos_t                   end,
                         InterThreadInfo&             itt,
                         boost::shared_ptr<Processor> endpoint,
                         bool                         include_endpoint)
{
	vector<boost::shared_ptr<Source> > srcs;
	return _session.write_one_track (*this, start, end, false, srcs, itt, endpoint, include_endpoint, false, false);
}

void
MidiTrack::freeze_me (InterThreadInfo& /*itt*/)
{
	std::cerr << "MIDI freeze currently unsupported" << std::endl;
}

void
MidiTrack::unfreeze ()
{
	_freeze_record.state = UnFrozen;
	FreezeChange (); /* EMIT SIGNAL */
}

void
MidiTrack::set_note_mode (NoteMode m)
{
	_note_mode = m;
	_disk_writer->set_note_mode(m);
}

std::string
MidiTrack::describe_parameter (Evoral::Parameter param)
{
	const std::string str(instrument_info().get_controller_name(param));
	return str.empty() ? Automatable::describe_parameter(param) : str;
}

void
MidiTrack::midi_panic()
{
	DEBUG_TRACE (DEBUG::MidiIO, string_compose ("%1 delivers panic data\n", name()));
	for (uint8_t channel = 0; channel <= 0xF; channel++) {
		uint8_t ev[3] = { ((uint8_t) (MIDI_CMD_CONTROL | channel)), ((uint8_t) MIDI_CTL_SUSTAIN), 0 };
		write_immediate_event(3, ev);
		ev[1] = MIDI_CTL_ALL_NOTES_OFF;
		write_immediate_event(3, ev);
		ev[1] = MIDI_CTL_RESET_CONTROLLERS;
		write_immediate_event(3, ev);
	}
}

/** \return true on success, false on failure (no buffer space left)
 */
bool
MidiTrack::write_immediate_event(size_t size, const uint8_t* buf)
{
	if (!Evoral::midi_event_is_valid(buf, size)) {
		cerr << "WARNING: Ignoring illegal immediate MIDI event" << endl;
		return false;
	}
	return (_immediate_events.write (0, Evoral::MIDI_EVENT, size, buf) == size);
}

void
MidiTrack::set_parameter_automation_state (Evoral::Parameter param, AutoState state)
{
	switch (param.type()) {
	case MidiCCAutomation:
	case MidiPgmChangeAutomation:
	case MidiPitchBenderAutomation:
	case MidiChannelPressureAutomation:
	case MidiNotePressureAutomation:
	case MidiSystemExclusiveAutomation:
		/* The track control for MIDI parameters is for immediate events to act
		   as a control surface, write/touch for them is not currently
		   supported. */
		return;
	default:
		Automatable::set_parameter_automation_state(param, state);
	}
}

void
MidiTrack::MidiControl::restore_value ()
{
	actually_set_value (get_value(), Controllable::NoGroup);
}

void
MidiTrack::MidiControl::actually_set_value (double val, PBD::Controllable::GroupControlDisposition group_override)
{
	const Evoral::Parameter &parameter = _list ? _list->parameter() : Control::parameter();
	const Evoral::ParameterDescriptor &desc = EventTypeMap::instance().descriptor(parameter);

	bool valid = false;
	if (isinf_local(val)) {
		cerr << "MIDIControl value is infinity" << endl;
	} else if (isnan_local(val)) {
		cerr << "MIDIControl value is NaN" << endl;
	} else if (val < desc.lower) {
		cerr << "MIDIControl value is < " << desc.lower << endl;
	} else if (val > desc.upper) {
		cerr << "MIDIControl value is > " << desc.upper << endl;
	} else {
		valid = true;
	}

	if (!valid) {
		return;
	}

	assert(val <= desc.upper);
	if ( ! _list || ! automation_playback()) {
		size_t size = 3;
		uint8_t ev[3] = { parameter.channel(), uint8_t (val), 0 };
		switch(parameter.type()) {
		case MidiCCAutomation:
			ev[0] += MIDI_CMD_CONTROL;
			ev[1] = parameter.id();
			ev[2] = int(val);
			break;

		case MidiPgmChangeAutomation:
			size = 2;
			ev[0] += MIDI_CMD_PGM_CHANGE;
			ev[1] = int(val);
			break;

		case MidiChannelPressureAutomation:
			size = 2;
			ev[0] += MIDI_CMD_CHANNEL_PRESSURE;
			ev[1] = int(val);
			break;

		case MidiNotePressureAutomation:
			ev[0] += MIDI_CMD_NOTE_PRESSURE;
			ev[1] = parameter.id();
			ev[2] = int(val);
			break;

		case MidiPitchBenderAutomation:
			ev[0] += MIDI_CMD_BENDER;
			ev[1] = 0x7F & int(val);
			ev[2] = 0x7F & (int(val) >> 7);
			break;

		default:
			assert(false);
		}
		_route->write_immediate_event(size,  ev);
	}

	AutomationControl::actually_set_value(val, group_override);
}

void
MidiTrack::set_step_editing (bool yn)
{
	if (_session.record_status() != Session::Disabled) {
		return;
	}

	if (yn != _step_editing) {
		_step_editing = yn;
		StepEditStatusChange (yn);
	}
}

boost::shared_ptr<SMFSource>
MidiTrack::write_source (uint32_t)
{
	return _disk_writer->midi_write_source ();
}

void
MidiTrack::set_playback_channel_mode(ChannelMode mode, uint16_t mask)
{
	if (_playback_filter.set_channel_mode(mode, mask)) {
		_session.set_dirty();
	}
}

void
MidiTrack::set_capture_channel_mode(ChannelMode mode, uint16_t mask)
{
	if (_capture_filter.set_channel_mode(mode, mask)) {
		_session.set_dirty();
	}
}

void
MidiTrack::set_playback_channel_mask (uint16_t mask)
{
	if (_playback_filter.set_channel_mask(mask)) {
		_session.set_dirty();
	}
}

void
MidiTrack::set_capture_channel_mask (uint16_t mask)
{
	if (_capture_filter.set_channel_mask(mask)) {
		_session.set_dirty();
	}
}

boost::shared_ptr<MidiPlaylist>
MidiTrack::midi_playlist ()
{
	return boost::dynamic_pointer_cast<MidiPlaylist> (_playlists[DataType::MIDI]);
}

bool
MidiTrack::input_active () const
{
	return _input_active;
}

void
MidiTrack::set_input_active (bool yn)
{
	if (yn != _input_active) {
		_input_active = yn;
		map_input_active (yn);
		InputActiveChanged (); /* EMIT SIGNAL */
	}
}

void
MidiTrack::map_input_active (bool yn)
{
	if (!_input) {
		return;
	}

	PortSet& ports (_input->ports());

	for (PortSet::iterator p = ports.begin(DataType::MIDI); p != ports.end(DataType::MIDI); ++p) {
		boost::shared_ptr<MidiPort> mp = boost::dynamic_pointer_cast<MidiPort> (*p);
		if (yn != mp->input_active()) {
			mp->set_input_active (yn);
		}
	}
}

void
MidiTrack::track_input_active (IOChange change, void* /* src */)
{
	if (change.type & IOChange::ConfigurationChanged) {
		map_input_active (_input_active);
	}
}

boost::shared_ptr<MidiBuffer>
MidiTrack::get_gui_feed_buffer () const
{
	return _disk_reader->get_gui_feed_buffer ();
}

void
MidiTrack::act_on_mute ()
{
	/* this is called right after our mute status has changed.
	   if we are now muted, send suitable output to shutdown
	   all our notes.

	   XXX we should should also stop all relevant note trackers.
	*/

	/* If we haven't got a diskstream yet, there's nothing to worry about,
	   and we can't call get_channel_mask() anyway.
	*/
	if (!_disk_writer) {
		return;
	}

	if (muted() || _mute_master->muted_by_others_soloing_at (MuteMaster::AllPoints)) {
		/* only send messages for channels we are using */

		uint16_t mask = _playback_filter.get_channel_mask();

		for (uint8_t channel = 0; channel <= 0xF; channel++) {

			if ((1<<channel) & mask) {

				DEBUG_TRACE (DEBUG::MidiIO, string_compose ("%1 delivers mute message to channel %2\n", name(), channel+1));
				uint8_t ev[3] = { ((uint8_t) (MIDI_CMD_CONTROL | channel)), MIDI_CTL_SUSTAIN, 0 };
				write_immediate_event (3, ev);

				/* Note we do not send MIDI_CTL_ALL_NOTES_OFF here, since this may
				   silence notes that came from another non-muted track. */
			}
		}

		/* Resolve active notes. */
		_disk_reader->resolve_tracker(_immediate_events, Port::port_offset());
	}
}

void
MidiTrack::monitoring_changed (bool self, Controllable::GroupControlDisposition gcd)
{
	Track::monitoring_changed (self, gcd);

	/* monitoring state changed, so flush out any on notes at the
	 * port level.
	 */

	PortSet& ports (_output->ports());

	for (PortSet::iterator p = ports.begin(); p != ports.end(); ++p) {
		boost::shared_ptr<MidiPort> mp = boost::dynamic_pointer_cast<MidiPort> (*p);
		if (mp) {
			mp->require_resolve ();
		}
	}

	_disk_reader->reset_tracker ();
}

MonitorState
MidiTrack::monitoring_state () const
{
	MonitorState ms = Track::monitoring_state();
	if (ms == MonitoringSilence) {
		return MonitoringInput;
	}
	return ms;
}
