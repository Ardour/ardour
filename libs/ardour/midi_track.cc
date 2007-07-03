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
#include <pbd/error.h>
#include <sigc++/retype.h>
#include <sigc++/retype_return.h>
#include <sigc++/bind.h>

#include <pbd/enumwriter.h>

#include <ardour/midi_track.h>
#include <ardour/midi_diskstream.h>
#include <ardour/session.h>
#include <ardour/io_processor.h>
#include <ardour/midi_region.h>
#include <ardour/midi_source.h>
#include <ardour/route_group_specialized.h>
#include <ardour/processor.h>
#include <ardour/midi_playlist.h>
#include <ardour/panner.h>
#include <ardour/utils.h>
#include <ardour/buffer_set.h>
#include <ardour/meter.h>
#include <ardour/midi_events.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

MidiTrack::MidiTrack (Session& sess, string name, Route::Flag flag, TrackMode mode)
	: Track (sess, name, flag, mode, DataType::MIDI)
	, _immediate_events(1024) // FIXME: size?
{
	MidiDiskstream::Flag dflags = MidiDiskstream::Flag (0);

	if (_flags & Hidden) {
		dflags = MidiDiskstream::Flag (dflags | MidiDiskstream::Hidden);
	} else {
		dflags = MidiDiskstream::Flag (dflags | MidiDiskstream::Recordable);
	}

	assert(mode != Destructive);

	boost::shared_ptr<MidiDiskstream> ds (new MidiDiskstream (_session, name, dflags));
	_session.add_diskstream (ds);

	set_diskstream (boost::dynamic_pointer_cast<MidiDiskstream> (ds));
	
	_declickable = true;
	_freeze_record.state = NoFreeze;
	_saved_meter_point = _meter_point;
	_mode = mode;

	set_input_minimum(ChanCount(DataType::MIDI, 1));
	set_input_maximum(ChanCount(DataType::MIDI, 1));
	set_output_minimum(ChanCount(DataType::MIDI, 1));
	set_output_maximum(ChanCount(DataType::MIDI, 1));
}

MidiTrack::MidiTrack (Session& sess, const XMLNode& node)
	: Track (sess, node)
	, _immediate_events(1024) // FIXME: size?
{
	_set_state(node, false);
	
	set_input_minimum(ChanCount(DataType::MIDI, 1));
	set_input_maximum(ChanCount(DataType::MIDI, 1));
	set_output_minimum(ChanCount(DataType::MIDI, 1));
	set_output_maximum(ChanCount(DataType::MIDI, 1));
}

MidiTrack::~MidiTrack ()
{
}


int
MidiTrack::set_diskstream (boost::shared_ptr<MidiDiskstream> ds)
{
	_diskstream = ds;
	_diskstream->set_io (*this);
	_diskstream->set_destructive (_mode == Destructive);

	_diskstream->set_record_enabled (false);
	//_diskstream->monitor_input (false);

	ic_connection.disconnect();
	ic_connection = input_changed.connect (mem_fun (*_diskstream, &MidiDiskstream::handle_input_change));

	DiskstreamChanged (); /* EMIT SIGNAL */

	return 0;
}	

int 
MidiTrack::use_diskstream (string name)
{
	boost::shared_ptr<MidiDiskstream> dstream;

	if ((dstream = boost::dynamic_pointer_cast<MidiDiskstream>(_session.diskstream_by_name (name))) == 0) {
		error << string_compose(_("MidiTrack: midi diskstream \"%1\" not known by session"), name) << endmsg;
		return -1;
	}
	
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
	
	if ((prop = node.property (X_("mode"))) != 0) {
		_mode = TrackMode (string_2_enum (prop->value(), _mode));
	} else {
		_mode = Normal;
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
		
		if (use_diskstream (id)) {
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

	_session.StateReady.connect (mem_fun (*this, &MidiTrack::set_state_part_two));

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
	
	XMLNode* align_node = new XMLNode (X_("alignment"));
	AlignStyle as = _diskstream->alignment_style ();
	align_node->add_property (X_("style"), enum_2_string (as));
	root.add_child_nocopy (*align_node);

	root.add_property (X_("mode"), enum_2_string (_mode));
	
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

		
		_freeze_record.have_mementos = false;
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

	if ((fnode = find_named_node (*pending_state, X_("alignment"))) != 0) {

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
MidiTrack::no_roll (nframes_t nframes, nframes_t start_frame, nframes_t end_frame, nframes_t offset, 
		     bool session_state_changing, bool can_record, bool rec_monitors_input)
{
	if (n_outputs().n_midi() == 0) {
		//return 0;
		throw; // FIXME
	}

	if (!_active) {
		silence (nframes, offset);
		//return 0; // FIXME
	}

	if (session_state_changing) {

		/* XXX is this safe to do against transport state changes? */

		passthru_silence (start_frame, end_frame, nframes, offset, 0, false);
		return 0;
	}

	midi_diskstream()->check_record_status (start_frame, nframes, can_record);

	bool send_silence;
	
	if (_have_internal_generator) {
		/* since the instrument has no input streams,
		   there is no reason to send any signal
		   into the route.
		*/
		send_silence = true;
	} else {

		if (Config->get_auto_input()) {
			if (Config->get_monitoring_model() == SoftwareMonitoring) {
				send_silence = false;
			} else {
				send_silence = true;
			}
		} else {
			if (_diskstream->record_enabled()) {
				if (Config->get_monitoring_model() == SoftwareMonitoring) {
					send_silence = false;
				} else {
					send_silence = true;
				}
			} else {
				send_silence = true;
			}
		}
	}

	apply_gain_automation = false;

	if (send_silence) {
		
		/* if we're sending silence, but we want the meters to show levels for the signal,
		   meter right here.
		*/
		
		if (_have_internal_generator) {
			passthru_silence (start_frame, end_frame, nframes, offset, 0, true);
		} else {
			if (_meter_point == MeterInput) {
				just_meter_input (start_frame, end_frame, nframes, offset);
			}
			passthru_silence (start_frame, end_frame, nframes, offset, 0, false);
		}

	} else {
	
		/* we're sending signal, but we may still want to meter the input. 
		 */

		passthru (start_frame, end_frame, nframes, offset, 0, (_meter_point == MeterInput));
	}

	return 0;
}

int
MidiTrack::roll (nframes_t nframes, nframes_t start_frame, nframes_t end_frame, nframes_t offset, int declick,
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
		silence (nframes, offset);
		return 0;
	}

	nframes_t transport_frame = _session.transport_frame();

	if ((nframes = check_initial_delay (nframes, offset, transport_frame)) == 0) {
		/* need to do this so that the diskstream sets its
		   playback distance to zero, thus causing diskstream::commit
		   to do nothing.
		   */
		return diskstream->process (transport_frame, 0, 0, can_record, rec_monitors_input);
	} 

	_silent = false;

	if ((dret = diskstream->process (transport_frame, nframes, offset, can_record, rec_monitors_input)) != 0) {

		silence (nframes, offset);

		return dret;
	}

	/* special condition applies */

	if (_meter_point == MeterInput) {
		just_meter_input (start_frame, end_frame, nframes, offset);
	}

	if (diskstream->record_enabled() && !can_record && !Config->get_auto_input()) {

		/* not actually recording, but we want to hear the input material anyway,
		   at least potentially (depending on monitoring options)
		   */

		passthru (start_frame, end_frame, nframes, offset, 0, true);

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

		process_output_buffers (bufs, start_frame, end_frame, nframes, offset,
				(!_session.get_record_enabled() || !Config->get_do_not_record_plugins()), declick, (_meter_point != MeterInput));
	
	}

	return 0;
}

int
MidiTrack::silent_roll (nframes_t nframes, nframes_t start_frame, nframes_t end_frame, nframes_t offset, 
			 bool can_record, bool rec_monitors_input)
{
	if (n_outputs().n_midi() == 0 && _processors.empty()) {
		return 0;
	}

	if (!_active) {
		silence (nframes, offset);
		return 0;
	}

	_silent = true;
	apply_gain_automation = false;

	silence (nframes, offset);

	return midi_diskstream()->process (_session.transport_frame() + offset, nframes, offset, can_record, rec_monitors_input);
}

void
MidiTrack::process_output_buffers (BufferSet& bufs,
			       nframes_t start_frame, nframes_t end_frame, 
			       nframes_t nframes, nframes_t offset, bool with_processors, int declick,
			       bool meter)
{
	/* There's no such thing as a MIDI bus for the time being.
	 * We'll do all the MIDI route work here for now, but the long-term goal is to have
	 * Route::process_output_buffers handle everything */
	
	if (meter && (_meter_point == MeterInput || _meter_point == MeterPreFader)) {
		_meter->run(bufs, start_frame, end_frame, nframes, offset);
	}

	// Run all processors
	if (with_processors) {
		Glib::RWLock::ReaderLock rm (_processor_lock, Glib::TRY_LOCK);
		if (rm.locked()) {
			for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
				(*i)->run (bufs, start_frame, end_frame, nframes, offset);
			}
		} 
	}
	
	if (meter && (_meter_point == MeterPostFader)) {
		_meter->run(bufs, start_frame, end_frame, nframes, offset);
	}
	
	// Main output stage
	if (muted()) {
		IO::silence(nframes, offset);
	} else {
		MidiBuffer& out_buf = bufs.get_midi(0);
		_immediate_events.read(out_buf, 0, 0, offset + nframes-1); // all stamps = 0
		deliver_output(bufs, start_frame, end_frame, nframes, offset);
	}
}

int
MidiTrack::export_stuff (BufferSet& bufs, nframes_t nframes, nframes_t end_frame)
{
	return -1;
}

void
MidiTrack::set_latency_delay (nframes_t longest_session_latency)
{
	Route::set_latency_delay (longest_session_latency);
	_diskstream->set_roll_delay (_roll_delay);
}

void
MidiTrack::bounce (InterThreadInfo& itt)
{
	throw;
	//vector<MidiSource*> srcs;
	//_session.write_one_midi_track (*this, 0, _session.current_end_frame(), false, srcs, itt);
}


void
MidiTrack::bounce_range (nframes_t start, nframes_t end, InterThreadInfo& itt)
{
	throw;
	//vector<MidiSource*> srcs;
	//_session.write_one_midi_track (*this, start, end, false, srcs, itt);
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

int
MidiTrack::set_mode (TrackMode m)
{
	assert(_diskstream);

	if (m != _mode) {

		if (_diskstream->set_destructive (m == Destructive)) {
			return -1;
		}

		_mode = m;
		
		TrackModeChanged (); /* EMIT SIGNAL */
	}

	return 0;
}
	
/** \return true on success, false on failure (no buffer space left)
 */
bool
MidiTrack::write_immediate_event(size_t size, const Byte* buf)
{
	return (_immediate_events.write(0, size, buf) == size);
}

void
MidiTrack::MidiControl::set_value(float val)
{
	assert(val >= 0);
	assert(val <= 127.0);

	boost::shared_ptr<MidiTrack> midi_track = _route.lock();

	if (midi_track && !_list->automation_playback()) {
		Byte ev[3] = { MIDI_CMD_CONTROL, _list->parameter().id(), (int)val };
		midi_track->write_immediate_event(3,  ev);
	}

	AutomationControl::set_value(val);
} 

