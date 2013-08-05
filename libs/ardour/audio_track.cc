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

#include <boost/scoped_array.hpp>

#include "pbd/boost_debug.h"
#include "pbd/enumwriter.h"
#include "pbd/error.h"

#include "evoral/Curve.hpp"

#include "ardour/amp.h"
#include "ardour/audio_buffer.h"
#include "ardour/audio_diskstream.h"
#include "ardour/audio_track.h"
#include "ardour/audioplaylist.h"
#include "ardour/buffer_set.h"
#include "ardour/delivery.h"
#include "ardour/meter.h"
#include "ardour/playlist_factory.h"
#include "ardour/processor.h"
#include "ardour/region.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/session_playlists.h"
#include "ardour/source.h"
#include "ardour/utils.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

AudioTrack::AudioTrack (Session& sess, string name, Route::Flag flag, TrackMode mode)
	: Track (sess, name, flag, mode)
{
}

AudioTrack::~AudioTrack ()
{
}

boost::shared_ptr<Diskstream>
AudioTrack::create_diskstream ()
{
	AudioDiskstream::Flag dflags = AudioDiskstream::Flag (0);

	if (_flags & Auditioner) {
		dflags = AudioDiskstream::Flag (dflags | AudioDiskstream::Hidden);
	} else {
		dflags = AudioDiskstream::Flag (dflags | AudioDiskstream::Recordable);
	}

	if (_mode == Destructive) {
		dflags = AudioDiskstream::Flag (dflags | AudioDiskstream::Destructive);
	} else if (_mode == NonLayered){
		dflags = AudioDiskstream::Flag(dflags | AudioDiskstream::NonLayered);
	}

	return boost::shared_ptr<AudioDiskstream> (new AudioDiskstream (_session, name(), dflags));
}

void
AudioTrack::set_diskstream (boost::shared_ptr<Diskstream> ds)
{
	Track::set_diskstream (ds);

	_diskstream->set_track (this);
	_diskstream->set_destructive (_mode == Destructive);
	_diskstream->set_non_layered (_mode == NonLayered);

	if (audio_diskstream()->deprecated_io_node) {

		if (!IO::connecting_legal) {
			IO::ConnectingLegal.connect_same_thread (*this, boost::bind (&AudioTrack::deprecated_use_diskstream_connections, this));
		} else {
			deprecated_use_diskstream_connections ();
		}
	}

	_diskstream->set_record_enabled (false);
	_diskstream->request_jack_monitors_input (false);

	DiskstreamChanged (); /* EMIT SIGNAL */
}

boost::shared_ptr<AudioDiskstream>
AudioTrack::audio_diskstream() const
{
	return boost::dynamic_pointer_cast<AudioDiskstream>(_diskstream);
}

int
AudioTrack::set_mode (TrackMode m)
{
	if (m != _mode) {

		if (_diskstream->set_destructive (m == Destructive)) {
			return -1;
		}

		_diskstream->set_non_layered (m == NonLayered);
		_mode = m;

		TrackModeChanged (); /* EMIT SIGNAL */
	}

	return 0;
}

bool
AudioTrack::can_use_mode (TrackMode m, bool& bounce_required)
{
	switch (m) {
	case NonLayered:
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

	if ((prop = node.property ("gain")) != 0) {
		_amp->set_gain (atof (prop->value().c_str()), this);
	}

	if ((prop = node.property ("input-connection")) != 0) {
		boost::shared_ptr<Bundle> c = _session.bundle_by_name (prop->value());

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

		_input->connect_ports_to_bundle (c, true, this);

	} else if ((prop = node.property ("inputs")) != 0) {
		if (_input->set_ports (prop->value())) {
			error << string_compose(_("improper input channel list in XML node (%1)"), prop->value()) << endmsg;
			return -1;
		}
	}

	return 0;
}

int
AudioTrack::set_state (const XMLNode& node, int version)
{
	const XMLProperty *prop;

	if (Track::set_state (node, version)) {
		return -1;
	}

	if ((prop = node.property (X_("mode"))) != 0) {
		_mode = TrackMode (string_2_enum (prop->value(), _mode));
	} else {
		_mode = Normal;
	}

	pending_state = const_cast<XMLNode*> (&node);

	if (_session.state_of_the_state() & Session::Loading) {
		_session.StateReady.connect_same_thread (*this, boost::bind (&AudioTrack::set_state_part_two, this));
	} else {
		set_state_part_two ();
	}

	return 0;
}

XMLNode&
AudioTrack::state (bool full_state)
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
			(*i)->id.print (buf, sizeof (buf));
			inode->add_property (X_("id"), buf);
			inode->add_child_copy ((*i)->state);

			freeze_node->add_child_nocopy (*inode);
		}

		root.add_child_nocopy (*freeze_node);
	}

	root.add_property (X_("mode"), enum_2_string (_mode));

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

		_freeze_record.state = Frozen;

		for (vector<FreezeRecordProcessorInfo*>::iterator i = _freeze_record.processor_info.begin(); i != _freeze_record.processor_info.end(); ++i) {
			delete *i;
		}
		_freeze_record.processor_info.clear ();

		if ((prop = fnode->property (X_("playlist"))) != 0) {
			boost::shared_ptr<Playlist> pl = _session.playlists->by_name (prop->value());
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
}

/** @param need_butler to be set to true if this track now needs the butler, otherwise it can be left alone
 *  or set to false.
 */
int
AudioTrack::roll (pframes_t nframes, framepos_t start_frame, framepos_t end_frame, int declick, bool& need_butler)
{
	Glib::Threads::RWLock::ReaderLock lm (_processor_lock, Glib::Threads::TRY_LOCK);

	if (!lm.locked()) {
		boost::shared_ptr<AudioDiskstream> diskstream = audio_diskstream();
		framecnt_t playback_distance = diskstream->calculate_playback_distance(nframes);
		if (can_internal_playback_seek(llabs(playback_distance))) {
			/* TODO should declick */
			internal_playback_seek(playback_distance);
		}
		return 0;
	}

	framepos_t transport_frame;
	boost::shared_ptr<AudioDiskstream> diskstream = audio_diskstream();

	if (n_outputs().n_total() == 0 && _processors.empty()) {
		return 0;
	}

	if (!_active) {
		silence (nframes);
		if (_meter_point == MeterInput && (_monitoring & MonitorInput || _diskstream->record_enabled())) {
			_meter->reset();
		}
		return 0;
	}

	transport_frame = _session.transport_frame();

	int dret;
	framecnt_t playback_distance;
	
	if ((nframes = check_initial_delay (nframes, transport_frame)) == 0) {

		/* need to do this so that the diskstream sets its
		   playback distance to zero, thus causing diskstream::commit
		   to do nothing.
		*/

		BufferSet bufs; /* empty set, no matter - nothing will happen */

		dret = diskstream->process (bufs, transport_frame, 0, playback_distance, false);
		need_butler = diskstream->commit (playback_distance);
		return dret;
	}

	_silent = false;
	_amp->apply_gain_automation(false);

	BufferSet& bufs = _session.get_route_buffers (n_process_buffers ());

	fill_buffers_with_input (bufs, _input, nframes);
	
	if (_meter_point == MeterInput && (_monitoring & MonitorInput || _diskstream->record_enabled())) {
		_meter->run (bufs, start_frame, end_frame, nframes, true);
	}

	if ((dret = diskstream->process (bufs, transport_frame, nframes, playback_distance, (monitoring_state() == MonitoringDisk))) != 0) {
		need_butler = diskstream->commit (playback_distance);
		silence (nframes);
		return dret;
	}

	process_output_buffers (bufs, start_frame, end_frame, nframes, declick, (!diskstream->record_enabled() && _session.transport_rolling()));

	need_butler = diskstream->commit (playback_distance);

	return 0;
}

int
AudioTrack::export_stuff (BufferSet& buffers, framepos_t start, framecnt_t nframes,
			  boost::shared_ptr<Processor> endpoint, bool include_endpoint, bool for_export)
{
	boost::scoped_array<gain_t> gain_buffer (new gain_t[nframes]);
	boost::scoped_array<Sample> mix_buffer (new Sample[nframes]);
	boost::shared_ptr<AudioDiskstream> diskstream = audio_diskstream();

	Glib::Threads::RWLock::ReaderLock rlock (_processor_lock);

	boost::shared_ptr<AudioPlaylist> apl = boost::dynamic_pointer_cast<AudioPlaylist>(diskstream->playlist());

	assert(apl);
	assert(buffers.count().n_audio() >= 1);
	assert ((framecnt_t) buffers.get_audio(0).capacity() >= nframes);

	if (apl->read (buffers.get_audio(0).data(), mix_buffer.get(), gain_buffer.get(), start, nframes) != nframes) {
		return -1;
	}

	uint32_t n=1;
	Sample* b = buffers.get_audio(0).data();
	BufferSet::audio_iterator bi = buffers.audio_begin();
	++bi;
	for ( ; bi != buffers.audio_end(); ++bi, ++n) {
		if (n < diskstream->n_channels().n_audio()) {
			if (apl->read (bi->data(), mix_buffer.get(), gain_buffer.get(), start, nframes, n) != nframes) {
				return -1;
			}
			b = bi->data();
		} else {
			/* duplicate last across remaining buffers */
			memcpy (bi->data(), b, sizeof (Sample) * nframes);
		}
	}

	// If no processing is required, there's no need to go any further.

	if (!endpoint && !include_endpoint) {
		return 0;
	}

	for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {

		if (!include_endpoint && (*i) == endpoint) {
			break;
		}
		
		/* if we're not exporting, stop processing if we come across a routing processor.
		 */
		
		if (!for_export && (*i)->does_routing()) {
			break;
		}

		/* even for export, don't run any processor that does routing. 

		   oh, and don't bother with the peak meter either.
		 */

		if (!(*i)->does_routing() && !boost::dynamic_pointer_cast<PeakMeter>(*i)) {
			(*i)->run (buffers, start, start+nframes, nframes, true);
		}
		
		if ((*i) == endpoint) {
			break;
		}
	}

	return 0;
}

bool
AudioTrack::bounceable (boost::shared_ptr<Processor> endpoint, bool include_endpoint) const
{
	if (!endpoint && !include_endpoint) {
		/* no processing - just read from the playlist and create new
		   files: always possible.
		*/
		return true;
	}

	Glib::Threads::RWLock::ReaderLock lm (_processor_lock);
	uint32_t naudio = n_inputs().n_audio();

	for (ProcessorList::const_iterator r = _processors.begin(); r != _processors.end(); ++r) {

		/* if we're not including the endpoint, potentially stop
		   right here before we test matching i/o valences.
		*/

		if (!include_endpoint && (*r) == endpoint) {
			return true;
		}

		/* ignore any processors that do routing, because we will not
		 * use them during a bounce/freeze/export operation.
		 */

		if ((*r)->does_routing()) {
			continue;
		}

		/* does the output from the last considered processor match the
		 * input to this one?
		 */
		
		if (naudio != (*r)->input_streams().n_audio()) {
			return false;
		}

		/* we're including the endpoint - if we just hit it, 
		   then stop.
		*/

		if ((*r) == endpoint) {
			return true;
		}

		/* save outputs of this processor to test against inputs 
		   of the next one.
		*/

		naudio = (*r)->output_streams().n_audio();
	}

	return true;
}

boost::shared_ptr<Region>
AudioTrack::bounce (InterThreadInfo& itt)
{
	return bounce_range (_session.current_start_frame(), _session.current_end_frame(), itt, main_outs(), false);
}

boost::shared_ptr<Region>
AudioTrack::bounce_range (framepos_t start, framepos_t end, InterThreadInfo& itt,
			  boost::shared_ptr<Processor> endpoint, bool include_endpoint)
{
	vector<boost::shared_ptr<Source> > srcs;
	return _session.write_one_track (*this, start, end, false, srcs, itt, endpoint, include_endpoint, false);
}

void
AudioTrack::freeze_me (InterThreadInfo& itt)
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

		if (_session.playlists->by_name (candidate) == 0) {
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

	boost::shared_ptr<Region> res;

	if ((res = _session.write_one_track (*this, _session.current_start_frame(), _session.current_end_frame(), true, srcs, itt, 
					     main_outs(), false, false)) == 0) {
		return;
	}

	_freeze_record.processor_info.clear ();

	{
		Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

		for (ProcessorList::iterator r = _processors.begin(); r != _processors.end(); ++r) {

			if (!(*r)->does_routing() && !boost::dynamic_pointer_cast<PeakMeter>(*r)) {

				FreezeRecordProcessorInfo* frii  = new FreezeRecordProcessorInfo ((*r)->get_state(), (*r));

				frii->id = (*r)->id();

				_freeze_record.processor_info.push_back (frii);

				/* now deactivate the processor */

				(*r)->deactivate ();
			}
			
			_session.set_dirty ();
		}
	}

	new_playlist = PlaylistFactory::create (DataType::AUDIO, _session, new_playlist_name, false);

	/* XXX need main outs automation state _freeze_record.pan_automation_state = _mainpanner->automation_state(); */

	region_name = new_playlist_name;

	/* create a new region from all filesources, keep it private */

	PropertyList plist;

	plist.add (Properties::start, 0);
	plist.add (Properties::length, srcs[0]->length(srcs[0]->timeline_position()));
	plist.add (Properties::name, region_name);
	plist.add (Properties::whole_file, true);

	boost::shared_ptr<Region> region (RegionFactory::create (srcs, plist, false));

	new_playlist->set_orig_track_id (id());
	new_playlist->add_region (region, _session.current_start_frame());
	new_playlist->set_frozen (true);
	region->set_locked (true);

	diskstream->use_playlist (boost::dynamic_pointer_cast<AudioPlaylist>(new_playlist));
	diskstream->set_record_enabled (false);

	/* reset stuff that has already been accounted for in the freeze process */

	set_gain (1.0, this);
	_amp->gain_control()->set_automation_state (Off);
	/* XXX need to use _main_outs _panner->set_automation_state (Off); */

	_freeze_record.state = Frozen;
	FreezeChange(); /* EMIT SIGNAL */
}

void
AudioTrack::unfreeze ()
{
	if (_freeze_record.playlist) {
		audio_diskstream()->use_playlist (_freeze_record.playlist);

		{
			Glib::Threads::RWLock::ReaderLock lm (_processor_lock); // should this be a write lock? jlc
			for (ProcessorList::iterator i = _processors.begin(); i != _processors.end(); ++i) {
				for (vector<FreezeRecordProcessorInfo*>::iterator ii = _freeze_record.processor_info.begin(); ii != _freeze_record.processor_info.end(); ++ii) {
					if ((*ii)->id == (*i)->id()) {
						(*i)->set_state (((*ii)->state), Stateful::current_state_version);
						break;
					}
				}
			}
		}

		_freeze_record.playlist.reset ();
		/* XXX need to use _main_outs _panner->set_automation_state (_freeze_record.pan_automation_state); */
	}

	_freeze_record.state = UnFrozen;
	FreezeChange (); /* EMIT SIGNAL */
}

boost::shared_ptr<AudioFileSource>
AudioTrack::write_source (uint32_t n)
{
	boost::shared_ptr<AudioDiskstream> ds = boost::dynamic_pointer_cast<AudioDiskstream> (_diskstream);
	assert (ds);
	return ds->write_source (n);
}

boost::shared_ptr<Diskstream>
AudioTrack::diskstream_factory (XMLNode const & node)
{
	return boost::shared_ptr<Diskstream> (new AudioDiskstream (_session, node));
}
