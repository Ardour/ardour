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

#include "pbd/enumwriter.h"
#include "pbd/convert.h"
#include "evoral/midi_util.h"

#include "ardour/buffer_set.h"
#include "ardour/debug.h"
#include "ardour/delivery.h"
#include "ardour/meter.h"
#include "ardour/midi_diskstream.h"
#include "ardour/midi_playlist.h"
#include "ardour/midi_port.h"
#include "ardour/midi_track.h"
#include "ardour/port.h"
#include "ardour/processor.h"
#include "ardour/session.h"
#include "ardour/session_playlists.h"
#include "ardour/utils.h"

#include "i18n.h"

namespace ARDOUR {
class InterThreadInfo;
class MidiSource;
class Region;
class SMFSource;
}

using namespace std;
using namespace ARDOUR;
using namespace PBD;

MidiTrack::MidiTrack (Session& sess, string name, Route::Flag flag, TrackMode mode)
	: Track (sess, name, flag, mode, DataType::MIDI)
	, _immediate_events(1024) // FIXME: size?
	, _step_edit_ring_buffer(64) // FIXME: size?
	, _note_mode(Sustained)
	, _step_editing (false)
	, _input_active (true)
{
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

boost::shared_ptr<Diskstream>
MidiTrack::create_diskstream ()
{
	MidiDiskstream::Flag dflags = MidiDiskstream::Flag (0);

	if (_flags & Hidden) {
		dflags = MidiDiskstream::Flag (dflags | MidiDiskstream::Hidden);
	} else {
		dflags = MidiDiskstream::Flag (dflags | MidiDiskstream::Recordable);
	}

	assert(_mode != Destructive);

	return boost::shared_ptr<Diskstream> (new MidiDiskstream (_session, name(), dflags));
}


void
MidiTrack::set_record_enabled (bool yn, void *src)
{
	if (_step_editing) {
		return;
	}

	Track::set_record_enabled (yn, src);
}

void
MidiTrack::set_diskstream (boost::shared_ptr<Diskstream> ds)
{
	/* We have to do this here, as Track::set_diskstream will cause a buffer refill,
	   and the diskstream must be set up to fill its buffers using the correct _note_mode.
	*/
	boost::shared_ptr<MidiDiskstream> mds = boost::dynamic_pointer_cast<MidiDiskstream> (ds);
	mds->set_note_mode (_note_mode);
	
	Track::set_diskstream (ds);

	mds->reset_tracker ();	

	_diskstream->set_track (this);
	_diskstream->set_destructive (_mode == Destructive);
	_diskstream->set_record_enabled (false);

	_diskstream_data_recorded_connection.disconnect ();
	mds->DataRecorded.connect_same_thread (
		_diskstream_data_recorded_connection,
		boost::bind (&MidiTrack::diskstream_data_recorded, this, _1));

	DiskstreamChanged (); /* EMIT SIGNAL */
}

boost::shared_ptr<MidiDiskstream>
MidiTrack::midi_diskstream() const
{
	return boost::dynamic_pointer_cast<MidiDiskstream>(_diskstream);
}

int
MidiTrack::set_state (const XMLNode& node, int version)
{
	const XMLProperty *prop;

	/* This must happen before Track::set_state(), as there will be a buffer
	   fill during that call, and we must fill buffers using the correct
	   _note_mode.
	*/
	if ((prop = node.property (X_("note-mode"))) != 0) {
		_note_mode = NoteMode (string_2_enum (prop->value(), _note_mode));
	} else {
		_note_mode = Sustained;
	}

	if (Track::set_state (node, version)) {
		return -1;
	}

	// No destructive MIDI tracks (yet?)
	_mode = Normal;

	if ((prop = node.property ("input-active")) != 0) {
		set_input_active (string_is_affirmative (prop->value()));
	}

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
		freeze_node->add_property ("playlist", _freeze_record.playlist->name());
		freeze_node->add_property ("state", enum_2_string (_freeze_record.state));

		for (vector<FreezeRecordProcessorInfo*>::iterator i = _freeze_record.processor_info.begin(); i != _freeze_record.processor_info.end(); ++i) {
			inode = new XMLNode (X_("processor"));
			(*i)->id.print (buf, sizeof(buf));
			inode->add_property (X_("id"), buf);
			inode->add_child_copy ((*i)->state);

			freeze_node->add_child_nocopy (*inode);
		}

		root.add_child_nocopy (*freeze_node);
	}

	root.add_property (X_("note-mode"), enum_2_string (_note_mode));

	root.add_property ("step-editing", (_step_editing ? "yes" : "no"));
	root.add_property ("note-mode", enum_2_string (_note_mode));
	root.add_property ("input-active", (_input_active ? "yes" : "no"));

	return root;
}

void
MidiTrack::set_state_part_two ()
{
	XMLNode* fnode;
	XMLProperty* prop;
	LocaleGuard lg (X_("POSIX"));

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

		if ((prop = fnode->property (X_("playlist"))) != 0) {
			boost::shared_ptr<Playlist> pl = _session.playlists->by_name (prop->value());
			if (pl) {
				_freeze_record.playlist = boost::dynamic_pointer_cast<MidiPlaylist> (pl);
			} else {
				_freeze_record.playlist.reset();
				_freeze_record.state = NoFreeze;
			return;
			}
		}

		if ((prop = fnode->property (X_("state"))) != 0) {
			_freeze_record.state = FreezeState (string_2_enum (prop->value(), _freeze_record.state));
		}

		XMLNodeConstIterator citer;
		XMLNodeList clist = fnode->children();

		for (citer = clist.begin(); citer != clist.end(); ++citer) {
			if ((*citer)->name() != X_("processor")) {
				continue;
			}

			if ((prop = (*citer)->property (X_("id"))) == 0) {
				continue;
			}

			FreezeRecordProcessorInfo* frii = new FreezeRecordProcessorInfo (*((*citer)->children().front()),
										   boost::shared_ptr<Processor>());
			frii->id = prop->value ();
			_freeze_record.processor_info.push_back (frii);
		}
	}

	if (midi_diskstream ()) {
		midi_diskstream()->set_block_size (_session.get_block_size ());
	}

	return;
}

/** @param need_butler to be set to true if this track now needs the butler, otherwise it can be left alone
 *  or set to false.
 */
int
MidiTrack::roll (pframes_t nframes, framepos_t start_frame, framepos_t end_frame, int declick, bool& need_butler)
{
	Glib::RWLock::ReaderLock lm (_processor_lock, Glib::TRY_LOCK);
	if (!lm.locked()) {
		return 0;
	}

	boost::shared_ptr<MidiDiskstream> diskstream = midi_diskstream();

	automation_snapshot (start_frame);

	if (n_outputs().n_total() == 0 && _processors.empty()) {
		return 0;
	}

	if (!_active) {
		silence (nframes);
		return 0;
	}

	framepos_t transport_frame = _session.transport_frame();

	int dret;
	framecnt_t playback_distance;

	if ((nframes = check_initial_delay (nframes, transport_frame)) == 0) {
		/* need to do this so that the diskstream sets its
		   playback distance to zero, thus causing diskstream::commit
		   to do nothing.
		   */
		dret = diskstream->process (transport_frame, 0, playback_distance);
		need_butler = diskstream->commit (playback_distance);
		return dret;
	}

	_silent = false;

	if ((dret = diskstream->process (transport_frame, nframes, playback_distance)) != 0) {
		need_butler = diskstream->commit (playback_distance);
		silence (nframes);
		return dret;
	}

	/* special condition applies */

	if (_meter_point == MeterInput) {
		_input->process_input (_meter, start_frame, end_frame, nframes);
	}

	if (monitoring_state() == MonitoringInput) {

		/* not actually recording, but we want to hear the input material anyway,
		   at least potentially (depending on monitoring options)
		*/

		passthru (start_frame, end_frame, nframes, 0);

	} else {
		/*
		   XXX is it true that the earlier test on n_outputs()
		   means that we can avoid checking it again here? i think
		   so, because changing the i/o configuration of an IO
		   requires holding the AudioEngine lock, which we hold
		   while in the process() tree.
		   */


		/* copy the diskstream data to all output buffers */

		//const size_t limit = n_process_buffers().n_audio();
		BufferSet& bufs = _session.get_scratch_buffers (n_process_buffers());
		MidiBuffer& mbuf (bufs.get_midi (0));

		/* we are a MIDI track, so we always start the chain with a single-channel diskstream */
		ChanCount c;
		c.set_audio (0);
		c.set_midi (1);
		bufs.set_count (c);

		diskstream->get_playback (mbuf, nframes);

		/* append immediate messages to the first MIDI buffer (thus sending it to the first output port) */

		write_out_of_band_data (bufs, start_frame, end_frame, nframes);

		/* final argument: don't waste time with automation if we're recording or we've just stopped (yes it can happen) */

		process_output_buffers (
			bufs, start_frame, end_frame, nframes,
			declick, (!diskstream->record_enabled() && !_session.transport_stopped())
			);
	}

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
	Glib::RWLock::ReaderLock lm (_processor_lock, Glib::TRY_LOCK);

	if (!lm.locked ()) {
		return;
	}

	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		(*i)->realtime_locate ();
	}

	midi_diskstream()->reset_tracker ();
}

void
MidiTrack::realtime_handle_transport_stopped ()
{
	Glib::RWLock::ReaderLock lm (_processor_lock, Glib::TRY_LOCK);

	if (!lm.locked ()) {
		return;
	}

	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
		(*i)->realtime_handle_transport_stopped ();
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

			const Evoral::MIDIEvent<framepos_t> ev(*e, false);

			/* note on, since for step edit, note length is determined
			   elsewhere
			*/

			if (ev.is_note_on()) {
				/* we don't care about the time for this purpose */
				_step_edit_ring_buffer.write (0, ev.type(), ev.size(), ev.buffer());
			}
		}
	}
}

void
MidiTrack::write_out_of_band_data (BufferSet& bufs, framepos_t /*start*/, framepos_t /*end*/, framecnt_t nframes)
{
	MidiBuffer& buf (bufs.get_midi (0));

	// Append immediate events

	if (_immediate_events.read_space()) {

		DEBUG_TRACE (DEBUG::MidiIO, string_compose ("%1 has %2 of immediate events to deliver\n",
		                                            name(), _immediate_events.read_space()));

		/* write as many of the immediate events as we can, but give "true" as
		 * the last argument ("stop on overflow in destination") so that we'll
		 * ship the rest out next time.
		 *
		 * the (nframes-1) argument puts all these events at the last
		 * possible position of the output buffer, so that we do not
		 * violate monotonicity when writing.
		 */

		_immediate_events.read (buf, 0, 1, nframes-1, true);
	}
}

int
MidiTrack::export_stuff (BufferSet& /*bufs*/, framepos_t /*start_frame*/, framecnt_t /*nframes*/, 
			 boost::shared_ptr<Processor> /*endpoint*/, bool /*include_endpoint*/, bool /*forexport*/)
{
	return -1;
}

boost::shared_ptr<Region>
MidiTrack::bounce (InterThreadInfo& /*itt*/)
{
	std::cerr << "MIDI bounce currently unsupported" << std::endl;
	return boost::shared_ptr<Region> ();
}


boost::shared_ptr<Region>
MidiTrack::bounce_range (framepos_t /*start*/, framepos_t /*end*/, InterThreadInfo& /*itt*/,
			 boost::shared_ptr<Processor> /*endpoint*/, bool /*include_endpoint*/)
{
	std::cerr << "MIDI bounce range currently unsupported" << std::endl;
	return boost::shared_ptr<Region> ();
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
	midi_diskstream()->set_note_mode(m);
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
	const uint32_t type = EventTypeMap::instance().midi_event_type(buf[0]);
	return (_immediate_events.write(0, type, size, buf) == size);
}

void
MidiTrack::MidiControl::set_value(double val)
{
	bool valid = false;
	if (std::isinf(val)) {
		cerr << "MIDIControl value is infinity" << endl;
	} else if (std::isnan(val)) {
		cerr << "MIDIControl value is NaN" << endl;
	} else if (val < _list->parameter().min()) {
		cerr << "MIDIControl value is < " << _list->parameter().min() << endl;
	} else if (val > _list->parameter().max()) {
		cerr << "MIDIControl value is > " << _list->parameter().max() << endl;
	} else {
		valid = true;
	}

	if (!valid) {
		return;
	}

	assert(val <= _list->parameter().max());
	if ( ! automation_playback()) {
		size_t size = 3;
		uint8_t ev[3] = { _list->parameter().channel(), uint8_t (val), 0 };
		switch(_list->parameter().type()) {
		case MidiCCAutomation:
			ev[0] += MIDI_CMD_CONTROL;
			ev[1] = _list->parameter().id();
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

	AutomationControl::set_value(val);
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
	return midi_diskstream()->write_source ();
}

void
MidiTrack::set_channel_mode (ChannelMode mode, uint16_t mask)
{
	midi_diskstream()->set_channel_mode (mode, mask);
}

ChannelMode
MidiTrack::get_channel_mode ()
{
	return midi_diskstream()->get_channel_mode ();
}

uint16_t
MidiTrack::get_channel_mask ()
{
	return midi_diskstream()->get_channel_mask ();
}

boost::shared_ptr<MidiPlaylist>
MidiTrack::midi_playlist ()
{
	return midi_diskstream()->midi_playlist ();
}

void
MidiTrack::diskstream_data_recorded (boost::weak_ptr<MidiSource> src)
{
	DataRecorded (src); /* EMIT SIGNAL */
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

boost::shared_ptr<Diskstream>
MidiTrack::diskstream_factory (XMLNode const & node)
{
	return boost::shared_ptr<Diskstream> (new MidiDiskstream (_session, node));
}

boost::shared_ptr<MidiBuffer>
MidiTrack::get_gui_feed_buffer () const
{
	return midi_diskstream()->get_gui_feed_buffer ();
}

void
MidiTrack::act_on_mute ()
{
	/* this is called right after our mute status has changed.
	   if we are now muted, send suitable output to shutdown
	   all our notes.

	   XXX we should should also stop all relevant note trackers.
	*/

	if (muted()) {
		/* only send messages for channels we are using */

		uint16_t mask = get_channel_mask();

		for (uint8_t channel = 0; channel <= 0xF; channel++) {

			if ((1<<channel) & mask) {

				DEBUG_TRACE (DEBUG::MidiIO, string_compose ("%1 delivers mute message to channel %2\n", name(), channel+1));
				uint8_t ev[3] = { ((uint8_t) (MIDI_CMD_CONTROL | channel)), MIDI_CTL_SUSTAIN, 0 };
				write_immediate_event (3, ev);
				ev[1] = MIDI_CTL_ALL_NOTES_OFF;
				write_immediate_event (3, ev);
			}
		}
	}
}
	
void
MidiTrack::set_monitoring (MonitorChoice mc)
{
	Track::set_monitoring (mc);

	boost::shared_ptr<MidiDiskstream> md (midi_diskstream());

	if (md) {
		md->reset_tracker ();
	}
}

MonitorState
MidiTrack::monitoring_state () const
{
	/* Explicit requests */
	
	if (_monitoring & MonitorInput) {
		return MonitoringInput;
	}
		
	if (_monitoring & MonitorDisk) {
		return MonitoringDisk;
	}

	if (_session.transport_rolling()) {
		return MonitoringDisk;
	} 

	/* the return value here doesn't mean that we're actually monitoring
	 * input, let alone input *audio*. but it means that we are NOT 
	 * monitoring silence. this allows us to still hear any audio generated
	 * by using internal generation techniques
	 */

	return MonitoringInput;
}
