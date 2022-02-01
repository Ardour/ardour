/*
 * Copyright (C) 2006-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2007-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2008-2012 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2013-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2015-2018 Ben Loftis <ben@harrisonconsoles.com>
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
#ifdef HAVE_BEATBOX
#include "ardour/beatbox.h"
#endif
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
	, _immediate_event_buffer(6096)
	, _step_edit_ring_buffer(64) // FIXME: size?
	, _note_mode (Sustained)
	, _step_editing (false)
	, _input_active (true)
	, _restore_pgm_on_load (true)
{
	_session.SessionLoaded.connect_same_thread (*this, boost::bind (&MidiTrack::restore_controls, this));

	_playback_filter.ChannelModeChanged.connect_same_thread (*this, boost::bind (&Track::playlist_modified, this));
	_playback_filter.ChannelMaskChanged.connect_same_thread (*this, boost::bind (&Track::playlist_modified, this));
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

	_disk_writer->set_note_mode (_note_mode);
	_disk_reader->reset_tracker ();

	_disk_writer->DataRecorded.connect_same_thread (*this, boost::bind (&MidiTrack::data_recorded, this, _1));

#ifdef HAVE_BEATBOX
	_beatbox.reset (new BeatBox (_session));
	add_processor (_beatbox, PostFader);
#endif

	return 0;
}

void
MidiTrack::data_recorded (boost::weak_ptr<MidiSource> src)
{
	DataRecorded (src); /* EMIT SIGNAL */
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

MonitorState
MidiTrack::get_input_monitoring_state (bool recording, bool talkback) const
{
	if (!_session.config.get_layered_record_mode () && (recording || talkback)) {
		return MonitoringCue;
	} else if (!_session.config.get_layered_record_mode () || recording || talkback) {
		return MonitoringInput;
	} else {
		return MonitoringSilence;
	}
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

	if (node.get_property ("restore-pgm", yn)) {
		set_restore_pgm_on_load (yn);
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

	if (_session.loading ()) {
		_session.StateReady.connect_same_thread (
			*this, boost::bind (&MidiTrack::set_state_part_two, this));
	} else {
		set_state_part_two ();
	}

	return 0;
}

XMLNode&
MidiTrack::state(bool save_template)
{
	XMLNode& root (Track::state (save_template));
	XMLNode* freeze_node;
	char buf[64];

	if (_freeze_record.playlist) {
		XMLNode* inode;

		freeze_node = new XMLNode (X_("freeze-info"));
		freeze_node->set_property ("playlist", _freeze_record.playlist->name());
		freeze_node->set_property ("playlist-id", _freeze_record.playlist->id().to_s());
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
	root.set_property ("restore-pgm", _restore_pgm_on_load);

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
	XMLProperty const * prop;

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

		boost::shared_ptr<Playlist> freeze_pl;
		if ((prop = fnode->property (X_("playlist-id"))) != 0) {
			freeze_pl = _session.playlists()->by_id (prop->value());
		} else if ((prop = fnode->property (X_("playlist"))) != 0) {
			freeze_pl = _session.playlists()->by_name (prop->value());
		}
		if (freeze_pl) {
			_freeze_record.playlist = boost::dynamic_pointer_cast<MidiPlaylist> (freeze_pl);
			_freeze_record.playlist->use();
		} else {
			_freeze_record.playlist.reset ();
			_freeze_record.state = NoFreeze;
			return;
		}


		fnode->get_property (X_("state"), _freeze_record.state);

		XMLNodeConstIterator citer;
		XMLNodeList clist = fnode->children();

		std::string str;
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
	/* first CC (bank select) */
	for (Controls::const_iterator c = _controls.begin(); c != _controls.end(); ++c) {
		boost::shared_ptr<MidiTrack::MidiControl> mctrl = boost::dynamic_pointer_cast<MidiTrack::MidiControl>(c->second);
		if (mctrl && mctrl->parameter().type () != MidiPgmChangeAutomation) {
			mctrl->restore_value();
		}
	}

	if (!_restore_pgm_on_load) {
		return;
	}

	/* then restore PGM */
	for (Controls::const_iterator c = _controls.begin(); c != _controls.end(); ++c) {
		boost::shared_ptr<MidiTrack::MidiControl> mctrl = boost::dynamic_pointer_cast<MidiTrack::MidiControl>(c->second);
		if (mctrl && mctrl->parameter().type () == MidiPgmChangeAutomation) {
			mctrl->restore_value();
		}
	}
}

void
MidiTrack::update_controls (BufferSet const& bufs)
{
	const MidiBuffer& buf = bufs.get_midi(0);
	for (MidiBuffer::const_iterator e = buf.begin(); e != buf.end(); ++e) {
		const Evoral::Event<samplepos_t>&         ev     = *e;
		const Evoral::Parameter                  param   = midi_parameter(ev.buffer(), ev.size());
		const boost::shared_ptr<AutomationControl> control = automation_control (param);
		if (control) {
			double old = control->get_double (false, timepos_t::zero (true));
			control->set_double (ev.value(), timepos_t::zero (false), false);
			if (old != ev.value()) {
				control->Changed (false, Controllable::NoGroup);
			}
		}
	}
}

int
MidiTrack::no_roll_unlocked (pframes_t nframes, samplepos_t start_sample, samplepos_t end_sample, bool state_changing)
{
	int ret = Track::no_roll_unlocked (nframes, start_sample, end_sample, state_changing);

	if (ret == 0 && _step_editing) {
		push_midi_input_to_step_edit_ringbuffer (nframes);
	}

	return ret;
}

void
MidiTrack::realtime_locate (bool for_loop_end)
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock, Glib::Threads::TRY_LOCK);

	if (!lm.locked ()) {
		return;
	}

	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		(*i)->realtime_locate (for_loop_end);
	}
}

void
MidiTrack::non_realtime_locate (samplepos_t spos)
{
	timepos_t pos (spos);

	Track::non_realtime_locate (spos);

	boost::shared_ptr<MidiPlaylist> playlist = _disk_writer->midi_playlist();
	if (!playlist) {
		return;
	}

	/* Get the top unmuted region at this position. */
	boost::shared_ptr<MidiRegion> region = boost::dynamic_pointer_cast<MidiRegion> (playlist->top_unmuted_region_at (pos));

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
	const timepos_t pos_beats = timepos_t (region->source_position().distance (pos).beats ()); /* relative to source start */

	for (Controls::const_iterator c = _controls.begin(); c != _controls.end(); ++c) {

		boost::shared_ptr<AutomationControl> ac = boost::dynamic_pointer_cast<AutomationControl> (c->second);

		if (!ac->automation_playback()) {
			continue;
		}

		boost::shared_ptr<MidiTrack::MidiControl> tcontrol;
		boost::shared_ptr<Evoral::Control>        rcontrol;

		if ((tcontrol = boost::dynamic_pointer_cast<MidiTrack::MidiControl>(c->second)) &&

		    (rcontrol = region->control(tcontrol->parameter()))) {

			if (rcontrol->list()->size() > 0) {
				tcontrol->set_value(rcontrol->list()->eval(pos_beats), Controllable::NoGroup);
			}
		}
	}
}

void
MidiTrack::push_midi_input_to_step_edit_ringbuffer (samplecnt_t nframes)
{
	PortSet& ports (_input->ports());

	for (PortSet::iterator p = ports.begin(DataType::MIDI); p != ports.end(DataType::MIDI); ++p) {

		Buffer& b (p->get_buffer (nframes));
		const MidiBuffer* const mb = dynamic_cast<MidiBuffer*>(&b);
		assert (mb);

		for (MidiBuffer::const_iterator e = mb->begin(); e != mb->end(); ++e) {

			const Evoral::Event<samplepos_t> ev(*e, false);

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
MidiTrack::snapshot_out_of_band_data (samplecnt_t nframes)
{
	_immediate_event_buffer.clear ();
	if (0 == _immediate_events.read_space()) {
		return;
	}

	assert (nframes > 0);

	DEBUG_TRACE (DEBUG::MidiIO, string_compose ("%1 has %2 of immediate events to deliver\n", name(), _immediate_events.read_space()));

	/* write as many of the immediate events as we can, but give "true" as
	 * the last argument ("stop on overflow in destination") so that we'll
	 * ship the rest out next time.
	 *
	 * the (nframes-1) argument puts all these events at the last
	 * possible position of the output buffer, so that we do not
	 * violate monotonicity when writing.
	 */

	_immediate_events.read (_immediate_event_buffer, 0, 1, nframes - 1, true);
}

void
MidiTrack::write_out_of_band_data (BufferSet& bufs, samplecnt_t nframes) const
{
	MidiBuffer& buf (bufs.get_midi (0));
	buf.merge_from (_immediate_event_buffer, nframes);
}

int
MidiTrack::export_stuff (BufferSet&                   buffers,
                         samplepos_t                  start,
                         samplecnt_t                  nframes,
                         boost::shared_ptr<Processor> endpoint,
                         bool                         include_endpoint,
                         bool                         for_export,
                         bool                         for_freeze,
                         MidiNoteTracker&            tracker)
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

	/* Can't use a note tracker here, because the note off's might be in a
	 * subsequent call
	 */

	MidiNoteTracker ignored;

	/* XXX this doesn't fail, other than if the lock cannot be obtained */
	mpl->rendered()->read (buffers.get_midi(0), start, start+nframes, ignored, start);

	MidiBuffer& buf = buffers.get_midi(0);

	if (endpoint && !for_export) {
		for (MidiBuffer::iterator i = buf.begin(); i != buf.end(); ++i) {
			MidiBuffer::TimeType *t = i.timeptr ();
			*t -= start;
		}
		bounce_process (buffers, start, nframes, endpoint, include_endpoint, for_export, for_freeze);
	}

	/* Add to tracker so that we can resolve at the end of the export (in Session::write_one_track()) */

	for (MidiBuffer::iterator i = buf.begin(); i != buf.end(); ++i) {
		tracker.track (*i);
	}

	return 0;
}

boost::shared_ptr<Region>
MidiTrack::bounce (InterThreadInfo& itt, std::string const& name)
{
	return bounce_range (_session.current_start_sample(), _session.current_end_sample(), itt, main_outs(), false, name);
}

boost::shared_ptr<Region>
MidiTrack::bounce_range (samplepos_t                  start,
                         samplepos_t                  end,
                         InterThreadInfo&             itt,
                         boost::shared_ptr<Processor> endpoint,
                         bool                         include_endpoint,
                         std::string const&           name)
{
	vector<boost::shared_ptr<Source> > srcs;
	return _session.write_one_track (*this, start, end, false, srcs, itt, endpoint, include_endpoint, false, false, name);
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
		write_immediate_event (Evoral::MIDI_EVENT, 3, ev);
		ev[1] = MIDI_CTL_ALL_NOTES_OFF;
		write_immediate_event (Evoral::MIDI_EVENT, 3, ev);
		ev[1] = MIDI_CTL_RESET_CONTROLLERS;
		write_immediate_event (Evoral::MIDI_EVENT, 3, ev);
	}
}

/** \return true on success, false on failure (no buffer space left)
 */
bool
MidiTrack::write_immediate_event(Evoral::EventType event_type, size_t size, const uint8_t* buf)
{
	if (!Evoral::midi_event_is_valid(buf, size)) {
		cerr << "WARNING: Ignoring illegal immediate MIDI event" << endl;
		return false;
	}
	return (_immediate_events.write (0, event_type, size, buf) == size);
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

	if (_session.loading ()) {
		/* send events later in MidiTrack::restore_controls */
		AutomationControl::actually_set_value (val, group_override);
		return;
	}

	assert(val <= desc.upper);
	if (!_list || !automation_playback ()) {
		size_t size = 3;
		uint8_t ev[3] = { parameter.channel(), uint8_t (val), 0 };
		switch(parameter.type()) {
		case MidiCCAutomation:
			ev[0] |= MIDI_CMD_CONTROL;
			ev[1] = parameter.id();
			ev[2] = int(val);
			break;

		case MidiPgmChangeAutomation:
			size = 2;
			ev[0] |= MIDI_CMD_PGM_CHANGE;
			ev[1] = int(val);
			break;

		case MidiChannelPressureAutomation:
			size = 2;
			ev[0] |= MIDI_CMD_CHANNEL_PRESSURE;
			ev[1] = int(val);
			break;

		case MidiNotePressureAutomation:
			ev[0] |= MIDI_CMD_NOTE_PRESSURE;
			ev[1] = parameter.id();
			ev[2] = int(val);
			break;

		case MidiPitchBenderAutomation:
			ev[0] |= MIDI_CMD_BENDER;
			ev[1] = 0x7F & int(val);
			ev[2] = 0x7F & (int(val) >> 7);
			break;

		default:
			size = 0;
			assert(false);
		}
		_route->write_immediate_event(Evoral::LIVE_MIDI_EVENT, size, ev);
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

void
MidiTrack::set_restore_pgm_on_load (bool yn)
{
	if (_restore_pgm_on_load == yn) {
		return;
	}
	_restore_pgm_on_load = yn;
	_session.set_dirty();
}

bool
MidiTrack::restore_pgm_on_load () const
{
	return _restore_pgm_on_load;
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
	return _disk_writer->get_gui_feed_buffer ();
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
				write_immediate_event (Evoral::MIDI_EVENT, 3, ev);

				/* Note we do not send MIDI_CTL_ALL_NOTES_OFF here, since this may
				   silence notes that came from another non-muted track. */
			}
		}

		/* Resolve active notes. */
		_disk_reader->resolve_tracker (_immediate_events, 0);
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
		/* MIDI always monitor input as fallback */
		return MonitoringInput;
	}
	return ms;
}

void
MidiTrack::filter_input (BufferSet& bufs)
{
	_capture_filter.filter (bufs);
}

void
MidiTrack::realtime_handle_transport_stopped ()
{
	Route::realtime_handle_transport_stopped ();
	_disk_reader->resolve_tracker (_immediate_events, 0);
}

void
MidiTrack::playlist_contents_changed ()

{
}
