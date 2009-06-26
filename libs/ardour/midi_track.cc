/*
    Copyright (C) 2006 Paul Davis 
	By Dave Robillard, 2006

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
#include <sigc++/retype.h>
#include <sigc++/retype_return.h>
#include <sigc++/bind.h>

#include "pbd/enumwriter.h"
#include "midi++/events.h"
#include "evoral/midi_util.h"

#include "ardour/amp.h"
#include "ardour/buffer_set.h"
#include "ardour/delivery.h"
#include "ardour/io_processor.h"
#include "ardour/meter.h"
#include "ardour/midi_diskstream.h"
#include "ardour/midi_playlist.h"
#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/midi_track.h"
#include "ardour/panner.h"
#include "ardour/processor.h"
#include "ardour/route_group_specialized.h"
#include "ardour/session.h"
#include "ardour/utils.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

MidiTrack::MidiTrack (Session& sess, string name, Route::Flag flag, TrackMode mode)
	: Track (sess, name, flag, mode, DataType::MIDI)
	, _immediate_events(1024) // FIXME: size?
	, _note_mode(Sustained)
{
	use_new_diskstream ();

	_declickable = true;
	_freeze_record.state = NoFreeze;
	_saved_meter_point = _meter_point;
	_mode = mode;
}

MidiTrack::MidiTrack (Session& sess, const XMLNode& node)
	: Track (sess, node, DataType::MIDI )
	, _immediate_events(1024) // FIXME: size?
	, _note_mode(Sustained)
{
	_set_state(node, false);
}

MidiTrack::~MidiTrack ()
{
}

void
MidiTrack::use_new_diskstream ()
{
	MidiDiskstream::Flag dflags = MidiDiskstream::Flag (0);

	if (_flags & Hidden) {
		dflags = MidiDiskstream::Flag (dflags | MidiDiskstream::Hidden);
	} else {
		dflags = MidiDiskstream::Flag (dflags | MidiDiskstream::Recordable);
	}

	assert(_mode != Destructive);

	boost::shared_ptr<MidiDiskstream> ds (new MidiDiskstream (_session, name(), dflags));
	_session.add_diskstream (ds);

	set_diskstream (boost::dynamic_pointer_cast<MidiDiskstream> (ds));
}	

int
MidiTrack::set_diskstream (boost::shared_ptr<MidiDiskstream> ds)
{
	_diskstream = ds;
	_diskstream->set_route (*this);
	_diskstream->set_destructive (_mode == Destructive);

	_diskstream->set_record_enabled (false);
	//_diskstream->monitor_input (false);

	ic_connection.disconnect();
	ic_connection = _input->changed.connect (mem_fun (*_diskstream, &MidiDiskstream::handle_input_change));

	DiskstreamChanged (); /* EMIT SIGNAL */

	return 0;
}	

int 
MidiTrack::use_diskstream (string name)
{
	boost::shared_ptr<MidiDiskstream> dstream;

	cerr << "\n\n\nMIDI use diskstream\n";

	if ((dstream = boost::dynamic_pointer_cast<MidiDiskstream>(_session.diskstream_by_name (name))) == 0) {
		error << string_compose(_("MidiTrack: midi diskstream \"%1\" not known by session"), name) << endmsg;
		return -1;
	}
	
	cerr << "\n\n\nMIDI found DS\n";
	return set_diskstream (dstream);
}

int 
MidiTrack::use_diskstream (const PBD::ID& id)
{
	boost::shared_ptr<MidiDiskstream> dstream;

	if ((dstream = boost::dynamic_pointer_cast<MidiDiskstream> (_session.diskstream_by_id (id))) == 0) {
	  	error << string_compose(_("MidiTrack: midi diskstream \"%1\" not known by session"), id) << endmsg;
		return -1;
	}
	
	return set_diskstream (dstream);
}

boost::shared_ptr<MidiDiskstream>
MidiTrack::midi_diskstream() const
{
	return boost::dynamic_pointer_cast<MidiDiskstream>(_diskstream);
}

int
MidiTrack::set_state (const XMLNode& node)
{
	return _set_state (node, true);
}

int
MidiTrack::_set_state (const XMLNode& node, bool call_base)
{
	const XMLProperty *prop;
	XMLNodeConstIterator iter;

	if (Route::_set_state (node, call_base)) {
		return -1;
	}
	
	// No destructive MIDI tracks (yet?)
	_mode = Normal;
	
	if ((prop = node.property (X_("note-mode"))) != 0) {
		_note_mode = NoteMode (string_2_enum (prop->value(), _note_mode));
	} else {
		_note_mode = Sustained;
	}

	if ((prop = node.property ("diskstream-id")) == 0) {
		
		/* some old sessions use the diskstream name rather than the ID */

		if ((prop = node.property ("diskstream")) == 0) {
			fatal << _("programming error: MidiTrack given state without diskstream!") << endmsg;
			/*NOTREACHED*/
			return -1;
		}

		if (use_diskstream (prop->value())) {
			return -1;
		}

	} else {
		
		PBD::ID id (prop->value());
		PBD::ID zero ("0");
		
		/* this wierd hack is used when creating tracks from a template. there isn't
		   a particularly good time to interpose between setting the first part of
		   the track state (notably Route::set_state() and the track mode), and the
		   second part (diskstream stuff). So, we have a special ID for the diskstream
		   that means "you should create a new diskstream here, not look for
		   an old one.
		*/

		cerr << "\n\n\n\n MIDI track " << name() << " found DS id " << id << endl;
		
		if (id == zero) {
			use_new_diskstream ();
		} else if (use_diskstream (id)) {
			return -1;
		}
	}

	XMLNodeList nlist;
	XMLNodeConstIterator niter;
	XMLNode *child;

	nlist = node.children();
	for (niter = nlist.begin(); niter != nlist.end(); ++niter){
		child = *niter;

		if (child->name() == X_("recenable")) {
			_rec_enable_control->set_state (*child);
			_session.add_controllable (_rec_enable_control);
		}
	}

	pending_state = const_cast<XMLNode*> (&node);

	if (_session.state_of_the_state() & Session::Loading) {
		_session.StateReady.connect (mem_fun (*this, &MidiTrack::set_state_part_two));
	} else {
		set_state_part_two ();
	}

	return 0;
}

XMLNode& 
MidiTrack::state(bool full_state)
{
	XMLNode& root (Route::state(full_state));
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

	/* Alignment: act as a proxy for the diskstream */
	
	XMLNode* align_node = new XMLNode (X_("Alignment"));
	AlignStyle as = _diskstream->alignment_style ();
	align_node->add_property (X_("style"), enum_2_string (as));
	root.add_child_nocopy (*align_node);

	root.add_property (X_("note-mode"), enum_2_string (_note_mode));
	
	/* we don't return diskstream state because we don't
	   own the diskstream exclusively. control of the diskstream
	   state is ceded to the Session, even if we create the
	   diskstream.
	*/

	_diskstream->id().print (buf, sizeof(buf));
	root.add_property ("diskstream-id", buf);
	
	root.add_child_nocopy (_rec_enable_control->get_state());

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
			boost::shared_ptr<Playlist> pl = _session.playlist_by_name (prop->value());
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

	/* Alignment: act as a proxy for the diskstream */

	if ((fnode = find_named_node (*pending_state, X_("Alignment"))) != 0) {

		if ((prop = fnode->property (X_("style"))) != 0) {

			/* fix for older sessions from before EnumWriter */

			string pstr;

			if (prop->value() == "capture") {
				pstr = "CaptureTime";
			} else if (prop->value() == "existing") {
				pstr = "ExistingMaterial";
			} else {
				pstr = prop->value();
			}

			AlignStyle as = AlignStyle (string_2_enum (pstr, as));
			_diskstream->set_persistent_align_style (as);
		}
	}
	return;
}	

int
MidiTrack::roll (nframes_t nframes, sframes_t start_frame, sframes_t end_frame, int declick,
		 bool can_record, bool rec_monitors_input)
{
	int dret;
	boost::shared_ptr<MidiDiskstream> diskstream = midi_diskstream();
	
	{
		Glib::RWLock::ReaderLock lm (_processor_lock, Glib::TRY_LOCK);
		if (lm.locked()) {
			// automation snapshot can also be called from the non-rt context
			// and it uses the redirect list, so we take the lock out here
			automation_snapshot (start_frame);
		}
	}

	if (n_outputs().n_total() == 0 && _processors.empty()) {
		return 0;
	}

	if (!_active) {
		silence (nframes);
		return 0;
	}

	nframes_t transport_frame = _session.transport_frame();

	
	if ((nframes = check_initial_delay (nframes, transport_frame)) == 0) {
		/* need to do this so that the diskstream sets its
		   playback distance to zero, thus causing diskstream::commit
		   to do nothing.
		   */
		return diskstream->process (transport_frame, 0, can_record, rec_monitors_input);
	} 

	_silent = false;

	if ((dret = diskstream->process (transport_frame, nframes, can_record, rec_monitors_input)) != 0) {

		silence (nframes);
		return dret;
	}

	/* special condition applies */

	if (_meter_point == MeterInput) {
		_input->process_input (_meter, start_frame, end_frame, nframes);
	}

	if (diskstream->record_enabled() && !can_record && !_session.config.get_auto_input()) {

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

		diskstream->get_playback(bufs.get_midi(0), start_frame, end_frame);

		process_output_buffers (bufs, start_frame, end_frame, nframes,
				(!_session.get_record_enabled() || !Config->get_do_not_record_plugins()), declick);
	
	}

	_main_outs->flush (nframes);

	return 0;
}

void
MidiTrack::write_controller_messages(MidiBuffer& output_buf, sframes_t start, sframes_t end, nframes_t nframes)
{
	// Append immediate events (UI controls)

	// XXX TAKE _port_offset in Port into account???

	_immediate_events.read (output_buf, 0, 0, nframes - 1); // all stamps = 0
}

int
MidiTrack::export_stuff (BufferSet& bufs, nframes_t nframes, sframes_t end_frame)
{
	return -1;
}

void
MidiTrack::set_latency_delay (nframes_t longest_session_latency)
{
	Route::set_latency_delay (longest_session_latency);
	_diskstream->set_roll_delay (_roll_delay);
}

boost::shared_ptr<Region>
MidiTrack::bounce (InterThreadInfo& itt)
{
	throw;
	// vector<MidiSource*> srcs;
	// return _session.write_one_track (*this, 0, _session.current_end_frame(), false, srcs, itt);
	return boost::shared_ptr<Region> ();
}


boost::shared_ptr<Region>
MidiTrack::bounce_range (nframes_t start, nframes_t end, InterThreadInfo& itt, bool enable_processing)
{
	throw;
	//vector<MidiSource*> srcs;
	//return _session.write_one_track (*this, start, end, false, srcs, itt);
	return boost::shared_ptr<Region> ();
}

void
MidiTrack::freeze (InterThreadInfo& itt)
{
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
	for (uint8_t channel = 0; channel <= 0xF; channel++) {
		uint8_t ev[3] = { MIDI_CMD_CONTROL | channel, MIDI_CTL_SUSTAIN, 0 };
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
MidiTrack::MidiControl::set_value(float val)
{
	bool valid = false;
	if (isinf(val)) {
		cerr << "MIDIControl value is infinity" << endl;
	} else if (isnan(val)) {
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
	size_t size = 3;

	if ( ! automation_playback()) {
		uint8_t ev[3] = { _list->parameter().channel(), int(val), 0 };
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

