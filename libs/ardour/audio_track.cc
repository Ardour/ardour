/*
    Copyright (C) 2002 Paul Davis 

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

#include <sigc++/retype.h>
#include <sigc++/retype_return.h>
#include <sigc++/bind.h>

#include <pbd/error.h>
#include <pbd/enumwriter.h>

#include <ardour/audio_track.h>
#include <ardour/audio_diskstream.h>
#include <ardour/session.h>
#include <ardour/io_processor.h>
#include <ardour/audioregion.h>
#include <ardour/audiosource.h>
#include <ardour/region_factory.h>
#include <ardour/route_group_specialized.h>
#include <ardour/processor.h>
#include <ardour/plugin_insert.h>
#include <ardour/audioplaylist.h>
#include <ardour/playlist_factory.h>
#include <ardour/panner.h>
#include <ardour/utils.h>
#include <ardour/buffer_set.h>
#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

AudioTrack::AudioTrack (Session& sess, string name, Route::Flag flag, TrackMode mode)
	: Track (sess, name, flag, mode)
{
	AudioDiskstream::Flag dflags = AudioDiskstream::Flag (0);

	if (_flags & Hidden) {
		dflags = AudioDiskstream::Flag (dflags | AudioDiskstream::Hidden);
	} else {
		dflags = AudioDiskstream::Flag (dflags | AudioDiskstream::Recordable);
	}

	if (mode == Destructive) {
		dflags = AudioDiskstream::Flag (dflags | AudioDiskstream::Destructive);
	}

	boost::shared_ptr<AudioDiskstream> ds (new AudioDiskstream (_session, name, dflags));
	
	_session.add_diskstream (ds);

	set_diskstream (boost::dynamic_pointer_cast<AudioDiskstream> (ds), this);
}

AudioTrack::AudioTrack (Session& sess, const XMLNode& node)
	: Track (sess, node)
{
	_set_state (node, false);
}

AudioTrack::~AudioTrack ()
{
}

int
AudioTrack::set_mode (TrackMode m)
{
	if (m != _mode) {

		if (_diskstream->set_destructive (m == Destructive)) {
			return -1;
		}

		_mode = m;
		
		TrackModeChanged (); /* EMIT SIGNAL */
	}

	return 0;
}

bool
AudioTrack::can_use_mode (TrackMode m, bool& bounce_required)
{
	switch (m) {
	case Normal:
		bounce_required = false;
		return true;
		
	case Destructive:
	default:
		return _diskstream->can_become_destructive (bounce_required);
	}
}

int
AudioTrack::deprecated_use_diskstream_connections ()
{
	boost::shared_ptr<AudioDiskstream> diskstream = audio_diskstream();

	if (diskstream->deprecated_io_node == 0) {
		return 0;
	}

	const XMLProperty* prop;
	XMLNode& node (*diskstream->deprecated_io_node);

	/* don't do this more than once. */

	diskstream->deprecated_io_node = 0;

	set_input_minimum (ChanCount::ZERO);
	set_input_maximum (ChanCount::INFINITE);
	set_output_minimum (ChanCount::ZERO);
	set_output_maximum (ChanCount::INFINITE);
	
	if ((prop = node.property ("gain")) != 0) {
		set_gain (atof (prop->value().c_str()), this);
		_gain = _desired_gain;
	}

	if ((prop = node.property ("input-connection")) != 0) {
		Bundle* c = _session.bundle_by_name (prop->value());
		
		if (c == 0) {
		  	error << string_compose(_("Unknown bundle \"%1\" listed for input of %2"), prop->value(), _name) << endmsg;
			
			if ((c = _session.bundle_by_name (_("in 1"))) == 0) {
			  	error << _("No input bundles available as a replacement")
			        << endmsg;
				return -1;
			} else {
			  	info << string_compose (_("Bundle %1 was not available - \"in 1\" used instead"), prop->value())
			       << endmsg;
			}
		}

		use_input_bundle(*c, this);

	} else if ((prop = node.property ("inputs")) != 0) {
		if (set_inputs (prop->value())) {
		  	error << string_compose(_("improper input channel list in XML node (%1)"), prop->value()) << endmsg;
			return -1;
		}
	}
	
	return 0;
}

int
AudioTrack::set_diskstream (boost::shared_ptr<AudioDiskstream> ds, void *src)
{
	_diskstream = ds;
	_diskstream->set_io (*this);
	_diskstream->set_destructive (_mode == Destructive);

	if (audio_diskstream()->deprecated_io_node) {

		if (!connecting_legal) {
			ConnectingLegal.connect (mem_fun (*this, &AudioTrack::deprecated_use_diskstream_connections));
		} else {
			deprecated_use_diskstream_connections ();
		}
	}

	_diskstream->set_record_enabled (false);
	_diskstream->monitor_input (false);

	ic_connection.disconnect();
	ic_connection = input_changed.connect (mem_fun (*_diskstream, &Diskstream::handle_input_change));

	DiskstreamChanged (); /* EMIT SIGNAL */

	return 0;
}	

int 
AudioTrack::use_diskstream (string name)
{
	boost::shared_ptr<AudioDiskstream> dstream;

	if ((dstream = boost::dynamic_pointer_cast<AudioDiskstream>(_session.diskstream_by_name (name))) == 0) {
		error << string_compose(_("AudioTrack: audio diskstream \"%1\" not known by session"), name) << endmsg;
		return -1;
	}
	
	return set_diskstream (dstream, this);
}

int 
AudioTrack::use_diskstream (const PBD::ID& id)
{
	boost::shared_ptr<AudioDiskstream> dstream;

	if ((dstream = boost::dynamic_pointer_cast<AudioDiskstream> (_session.diskstream_by_id (id))) == 0) {
	  	error << string_compose(_("AudioTrack: audio diskstream \"%1\" not known by session"), id) << endmsg;
		return -1;
	}
	
	return set_diskstream (dstream, this);
}

boost::shared_ptr<AudioDiskstream>
AudioTrack::audio_diskstream() const
{
	return boost::dynamic_pointer_cast<AudioDiskstream>(_diskstream);
}

int
AudioTrack::set_state (const XMLNode& node)
{
	return _set_state (node, true);
}

int
AudioTrack::_set_state (const XMLNode& node, bool call_base)
{
	const XMLProperty *prop;
	XMLNodeConstIterator iter;

	if (call_base) {
		if (Route::_set_state (node, call_base)) {
			return -1;
		}
	}

	if ((prop = node.property (X_("mode"))) != 0) {
		_mode = TrackMode (string_2_enum (prop->value(), _mode));
	} else {
		_mode = Normal;
	}

	if ((prop = node.property ("diskstream-id")) == 0) {
		
		/* some old sessions use the diskstream name rather than the ID */

		if ((prop = node.property ("diskstream")) == 0) {
			fatal << _("programming error: AudioTrack given state without diskstream!") << endmsg;
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

	_session.StateReady.connect (mem_fun (*this, &AudioTrack::set_state_part_two));

	return 0;
}

XMLNode& 
AudioTrack::state(bool full_state)
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
			(*i)->id.print (buf, sizeof (buf));
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

	_diskstream->id().print (buf, sizeof (buf));
	root.add_property ("diskstream-id", buf);

	root.add_child_nocopy (_rec_enable_control->get_state());

	return root;
}

void
AudioTrack::set_state_part_two ()
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
				_freeze_record.playlist = boost::dynamic_pointer_cast<AudioPlaylist> (pl);
			} else {
				_freeze_record.playlist.reset ();
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
AudioTrack::no_roll (nframes_t nframes, nframes_t start_frame, nframes_t end_frame, nframes_t offset, 
		     bool session_state_changing, bool can_record, bool rec_monitors_input)
{
	if (n_outputs().n_total() == 0) {
		return 0;
	}

	if (!_active) {
		silence (nframes, offset);
		return 0;
	}

	if (session_state_changing) {

		/* XXX is this safe to do against transport state changes? */

		passthru_silence (start_frame, end_frame, nframes, offset, 0, false);
		return 0;
	}

	audio_diskstream()->check_record_status (start_frame, nframes, can_record);

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
AudioTrack::roll (nframes_t nframes, nframes_t start_frame, nframes_t end_frame, nframes_t offset, int declick,
		  bool can_record, bool rec_monitors_input)
{
	int dret;
	Sample* b;
	Sample* tmpb;
	nframes_t transport_frame;
	boost::shared_ptr<AudioDiskstream> diskstream = audio_diskstream();
	
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

	transport_frame = _session.transport_frame();

	if ((nframes = check_initial_delay (nframes, offset, transport_frame)) == 0) {
		/* need to do this so that the diskstream sets its
		   playback distance to zero, thus causing diskstream::commit
		   to do nothing.
		*/
		return diskstream->process (transport_frame, 0, 0, can_record, rec_monitors_input);
	} 

	_silent = false;
	apply_gain_automation = false;

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

	} else if ((b = diskstream->playback_buffer(0)) != 0) {

		/*
		  XXX is it true that the earlier test on n_outputs()
		  means that we can avoid checking it again here? i think
		  so, because changing the i/o configuration of an IO
		  requires holding the AudioEngine lock, which we hold
		  while in the process() tree.
		*/

		
		/* copy the diskstream data to all output buffers */
		
		const size_t limit = n_process_buffers().n_audio();
		BufferSet& bufs = _session.get_scratch_buffers (n_process_buffers());
		
		uint32_t n;
		uint32_t i;

		for (i = 0, n = 1; i < limit; ++i, ++n) {
			memcpy (bufs.get_audio(i).data(), b, sizeof (Sample) * nframes); 
			if (n < diskstream->n_channels().n_audio()) {
				tmpb = diskstream->playback_buffer(n);
				if (tmpb!=0) {
					b = tmpb;
				}
			}
		}

		/* don't waste time with automation if we're recording or we've just stopped (yes it can happen) */

		if (!diskstream->record_enabled() && _session.transport_rolling()) {
			Glib::Mutex::Lock am (_automation_lock, Glib::TRY_LOCK);
			
			if (am.locked() && gain_control()->list()->automation_playback()) {
				apply_gain_automation = gain_control()->list()->curve().rt_safe_get_vector (start_frame, end_frame, _session.gain_automation_buffer(), nframes);
			}
		}

		process_output_buffers (bufs, start_frame, end_frame, nframes, offset, (!_session.get_record_enabled() || !Config->get_do_not_record_plugins()), declick, (_meter_point != MeterInput));
		
	} else {
		/* problem with the diskstream; just be quiet for a bit */
		silence (nframes, offset);
	}

	return 0;
}

int
AudioTrack::silent_roll (nframes_t nframes, nframes_t start_frame, nframes_t end_frame, nframes_t offset, 
			 bool can_record, bool rec_monitors_input)
{
	if (n_outputs().n_total() == 0 && _processors.empty()) {
		return 0;
	}

	if (!_active) {
		silence (nframes, offset);
		return 0;
	}

	_silent = true;
	apply_gain_automation = false;

	silence (nframes, offset);

	return audio_diskstream()->process (_session.transport_frame() + offset, nframes, offset, can_record, rec_monitors_input);
}

int
AudioTrack::export_stuff (BufferSet& buffers, nframes_t start, nframes_t nframes)
{
	gain_t  gain_automation[nframes];
	gain_t  gain_buffer[nframes];
	float   mix_buffer[nframes];
	ProcessorList::iterator i;
	bool post_fader_work = false;
	gain_t this_gain = _gain;
	boost::shared_ptr<AudioDiskstream> diskstream = audio_diskstream();
	
	Glib::RWLock::ReaderLock rlock (_processor_lock);

	boost::shared_ptr<AudioPlaylist> apl = boost::dynamic_pointer_cast<AudioPlaylist>(diskstream->playlist());
	assert(apl);

	if (apl->read (buffers.get_audio(nframes).data(),
			mix_buffer, gain_buffer, start, nframes) != nframes) {
		return -1;
	}

	assert(buffers.count().n_audio() >= 1);
	uint32_t n=1;
	Sample* b = buffers.get_audio(0).data();
	BufferSet::audio_iterator bi = buffers.audio_begin();
	++bi;
	for ( ; bi != buffers.audio_end(); ++bi, ++n) {
		if (n < diskstream->n_channels().n_audio()) {
			if (apl->read (bi->data(), mix_buffer, gain_buffer, start, nframes, n) != nframes) {
				return -1;
			}
			b = bi->data();
		}
		else {
			/* duplicate last across remaining buffers */
			memcpy (bi->data(), b, sizeof (Sample) * nframes); 
		}
	}


	/* note: only run processors during export. other layers in the machinery
	   will already have checked that there are no external port processors.
	*/
	
	for (i = _processors.begin(); i != _processors.end(); ++i) {
		boost::shared_ptr<Processor> processor;
		
		if ((processor = boost::dynamic_pointer_cast<Processor>(*i)) != 0) {
			switch (processor->placement()) {
			case PreFader:
				processor->run (buffers, start, start+nframes, nframes, 0);
				break;
			case PostFader:
				post_fader_work = true;
				break;
			}
		}
	}
	
	if (gain_control()->list()->automation_state() == Play) {
		
		gain_control()->list()->curve().get_vector (start, start + nframes, gain_automation, nframes);

		for (BufferSet::audio_iterator bi = buffers.audio_begin(); bi != buffers.audio_end(); ++bi) {
			Sample *b = bi->data();
			for (nframes_t n = 0; n < nframes; ++n) {
				b[n] *= gain_automation[n];
			}
		}

	} else {

		for (BufferSet::audio_iterator bi = buffers.audio_begin(); bi != buffers.audio_end(); ++bi) {
			Sample *b = bi->data();
			for (nframes_t n = 0; n < nframes; ++n) {
				b[n] *= this_gain;
			}
		}
	}

	if (post_fader_work) {

		for (i = _processors.begin(); i != _processors.end(); ++i) {
			boost::shared_ptr<PluginInsert> processor;
			
			if ((processor = boost::dynamic_pointer_cast<PluginInsert>(*i)) != 0) {
				switch ((*i)->placement()) {
				case PreFader:
					break;
				case PostFader:
					processor->run (buffers, start, start+nframes, nframes, 0);
					break;
				}
			}
		}
	} 

	return 0;
}

void
AudioTrack::bounce (InterThreadInfo& itt)
{
	vector<boost::shared_ptr<Source> > srcs;
	_session.write_one_audio_track (*this, 0, _session.current_end_frame(), false, srcs, itt);
	itt.done = true;
}


void
AudioTrack::bounce_range (nframes_t start, nframes_t end, InterThreadInfo& itt)
{
	vector<boost::shared_ptr<Source> > srcs;
	_session.write_one_audio_track (*this, start, end, false, srcs, itt);
	itt.done = true;
}

void
AudioTrack::freeze (InterThreadInfo& itt)
{
	vector<boost::shared_ptr<Source> > srcs;
	string new_playlist_name;
	boost::shared_ptr<Playlist> new_playlist;
	string dir;
	string region_name;
	boost::shared_ptr<AudioDiskstream> diskstream = audio_diskstream();
	
	if ((_freeze_record.playlist = boost::dynamic_pointer_cast<AudioPlaylist>(diskstream->playlist())) == 0) {
		return;
	}

	uint32_t n = 1;

	while (n < (UINT_MAX-1)) {
	 
		string candidate;
		
		candidate = string_compose ("<F%2>%1", _freeze_record.playlist->name(), n);

		if (_session.playlist_by_name (candidate) == 0) {
			new_playlist_name = candidate;
			break;
		}

		++n;

	} 

	if (n == (UINT_MAX-1)) {
	  error << string_compose (X_("There are too many frozen versions of playlist \"%1\""
	  		    " to create another one"), _freeze_record.playlist->name())
	       << endmsg;
		return;
	}

	if (_session.write_one_audio_track (*this, _session.current_start_frame(), _session.current_end_frame(), true, srcs, itt)) {
		return;
	}

	_freeze_record.processor_info.clear ();
	_freeze_record.have_mementos = true;

	{
		Glib::RWLock::ReaderLock lm (_processor_lock);
		
		for (ProcessorList::iterator r = _processors.begin(); r != _processors.end(); ++r) {
			
			boost::shared_ptr<Processor> processor;

			if ((processor = boost::dynamic_pointer_cast<Processor>(*r)) != 0) {
				
				FreezeRecordProcessorInfo* frii  = new FreezeRecordProcessorInfo ((*r)->get_state(), processor);
				
				frii->id = processor->id();

				_freeze_record.processor_info.push_back (frii);
				
				/* now deactivate the processor */
				
				processor->set_active (false);
				_session.set_dirty ();
			}
		}
	}

	new_playlist = PlaylistFactory::create (DataType::AUDIO, _session, new_playlist_name, false);
	region_name = new_playlist_name;

	/* create a new region from all filesources, keep it private */

	boost::shared_ptr<Region> region (RegionFactory::create (srcs, 0, srcs[0]->length(), 
								 region_name, 0, 
								 (Region::Flag) (Region::WholeFile|Region::DefaultFlags),
								 false));

	new_playlist->set_orig_diskstream_id (diskstream->id());
	new_playlist->add_region (region, _session.current_start_frame());
	new_playlist->set_frozen (true);
	region->set_locked (true);

	diskstream->use_playlist (boost::dynamic_pointer_cast<AudioPlaylist>(new_playlist));
	diskstream->set_record_enabled (false);

	_freeze_record.state = Frozen;
	FreezeChange(); /* EMIT SIGNAL */
}

void
AudioTrack::unfreeze ()
{
	if (_freeze_record.playlist) {
		audio_diskstream()->use_playlist (_freeze_record.playlist);

		if (_freeze_record.have_mementos) {

			for (vector<FreezeRecordProcessorInfo*>::iterator i = _freeze_record.processor_info.begin(); i != _freeze_record.processor_info.end(); ++i) {
				(*i)->memento ();
			}

		} else {

			Glib::RWLock::ReaderLock lm (_processor_lock); // should this be a write lock? jlc
			for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
				for (vector<FreezeRecordProcessorInfo*>::iterator ii = _freeze_record.processor_info.begin(); ii != _freeze_record.processor_info.end(); ++ii) {
					if ((*ii)->id == (*i)->id()) {
						(*i)->set_state (((*ii)->state));
						break;
					}
				}
			}
		}
		
		_freeze_record.playlist.reset ();
	}

	_freeze_record.state = UnFrozen;
	FreezeChange (); /* EMIT SIGNAL */
}

