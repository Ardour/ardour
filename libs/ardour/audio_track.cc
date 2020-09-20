/*
 * Copyright (C) 2002-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005-2006 Jesse Chappell <jesse@essej.net>
 * Copyright (C) 2006-2009 Sampo Savolainen <v2@iki.fi>
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2016 Tim Mayberry <mojofunk@gmail.com>
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

#include <boost/scoped_array.hpp>

#include "pbd/enumwriter.h"
#include "pbd/error.h"

#include "evoral/Curve.h"

#include "ardour/amp.h"
#include "ardour/audio_buffer.h"
#include "ardour/audio_track.h"
#include "ardour/audioplaylist.h"
#include "ardour/boost_debug.h"
#include "ardour/buffer_set.h"
#include "ardour/delivery.h"
#include "ardour/disk_reader.h"
#include "ardour/disk_writer.h"
#include "ardour/meter.h"
#include "ardour/monitor_control.h"
#include "ardour/playlist_factory.h"
#include "ardour/processor.h"
#include "ardour/profile.h"
#include "ardour/region.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/session_playlists.h"
#include "ardour/source.h"
#include "ardour/types_convert.h"
#include "ardour/utils.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

AudioTrack::AudioTrack (Session& sess, string name, TrackMode mode)
	: Track (sess, name, PresentationInfo::AudioTrack, mode)
{
}

AudioTrack::~AudioTrack ()
{
	if (_freeze_record.playlist && !_session.deletion_in_progress()) {
		_freeze_record.playlist->release();
	}
}

int
AudioTrack::set_state (const XMLNode& node, int version)
{
	if (!node.get_property (X_("mode"), _mode)) {
		_mode = Normal;
	}

	if (_mode == Destructive) {
		/* XXX warn user */
		_mode = Normal;
	}

	if (Track::set_state (node, version)) {
		return -1;
	}

	pending_state = const_cast<XMLNode*> (&node);

	if (_session.loading ()) {
		_session.StateReady.connect_same_thread (*this, boost::bind (&AudioTrack::set_state_part_two, this));
	} else {
		set_state_part_two ();
	}

	return 0;
}

XMLNode&
AudioTrack::state (bool save_template)
{
	XMLNode& root (Track::state (save_template));
	XMLNode* freeze_node;

	if (_freeze_record.playlist) {
		XMLNode* inode;

		freeze_node = new XMLNode (X_("freeze-info"));
		freeze_node->set_property ("playlist", _freeze_record.playlist->name());
		freeze_node->set_property ("playlist-id", _freeze_record.playlist->id().to_s ());
		freeze_node->set_property ("state", _freeze_record.state);

		for (vector<FreezeRecordProcessorInfo*>::iterator i = _freeze_record.processor_info.begin(); i != _freeze_record.processor_info.end(); ++i) {
			inode = new XMLNode (X_("processor"));
			inode->set_property (X_ ("id"), (*i)->id.to_s ());
			inode->add_child_copy ((*i)->state);

			freeze_node->add_child_nocopy (*inode);
		}

		root.add_child_nocopy (*freeze_node);
	}

	root.set_property (X_("mode"), _mode);

	return root;
}

void
AudioTrack::set_state_part_two ()
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
			_freeze_record.playlist = boost::dynamic_pointer_cast<AudioPlaylist> (freeze_pl);
			_freeze_record.playlist->use();
		} else {
			_freeze_record.playlist.reset ();
			_freeze_record.state = NoFreeze;
			return;
		}

		fnode->get_property (X_("state"), _freeze_record.state);

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

MonitorState
AudioTrack::get_input_monitoring_state (bool recording, bool talkback) const
{
	if (Config->get_monitoring_model() == SoftwareMonitoring && (recording || talkback)) {
		return MonitoringInput;
	} else {
		return MonitoringSilence;
	}
}

int
AudioTrack::export_stuff (BufferSet& buffers, samplepos_t start, samplecnt_t nframes,
                          boost::shared_ptr<Processor> endpoint, bool include_endpoint, bool for_export, bool for_freeze,
                          MidiStateTracker& /* ignored, this is audio */)
{
	boost::scoped_array<gain_t> gain_buffer (new gain_t[nframes]);
	boost::scoped_array<Sample> mix_buffer (new Sample[nframes]);

	Glib::Threads::RWLock::ReaderLock rlock (_processor_lock);

	boost::shared_ptr<AudioPlaylist> apl = boost::dynamic_pointer_cast<AudioPlaylist>(playlist());

	assert(apl);
	assert(buffers.count().n_audio() >= 1);
	assert ((samplecnt_t) buffers.get_audio(0).capacity() >= nframes);

	if (apl->read (buffers.get_audio(0).data(), mix_buffer.get(), gain_buffer.get(), timepos_t (start), timecnt_t (nframes)) != nframes) {
		return -1;
	}

	uint32_t n=1;
	Sample* b = buffers.get_audio(0).data();
	BufferSet::audio_iterator bi = buffers.audio_begin();
	++bi;
	for ( ; bi != buffers.audio_end(); ++bi, ++n) {
		if (n < _disk_reader->output_streams().n_audio()) {
			if (apl->read (bi->data(), mix_buffer.get(), gain_buffer.get(), timepos_t (start), timecnt_t (nframes), n) != nframes) {
				return -1;
			}
			b = bi->data();
		} else {
			/* duplicate last across remaining buffers */
			memcpy (bi->data(), b, sizeof (Sample) * nframes);
		}
	}

	bounce_process (buffers, start, nframes, endpoint, include_endpoint, for_export, for_freeze);

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

		if (boost::dynamic_pointer_cast<PeakMeter>(*r)) {
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
AudioTrack::bounce (InterThreadInfo& itt, std::string const& name)
{
	return bounce_range (_session.current_start_sample(), _session.current_end_sample(), itt, main_outs(), false, name);
}

boost::shared_ptr<Region>
AudioTrack::bounce_range (samplepos_t start,
                          samplepos_t end,
                          InterThreadInfo& itt,
                          boost::shared_ptr<Processor> endpoint,
                          bool include_endpoint,
                          std::string const& name)
{
	vector<boost::shared_ptr<Source> > srcs;
	return _session.write_one_track (*this, start, end, false, srcs, itt, endpoint, include_endpoint, false, false, name);
}

void
AudioTrack::freeze_me (InterThreadInfo& itt)
{
	vector<boost::shared_ptr<Source> > srcs;
	string new_playlist_name;
	boost::shared_ptr<Playlist> new_playlist;
	string dir;
	string region_name;

	if ((_freeze_record.playlist = boost::dynamic_pointer_cast<AudioPlaylist>(playlist())) == 0) {
		return;
	}

	uint32_t n = 1;

	while (n < (UINT_MAX-1)) {

		string candidate;

		candidate = string_compose ("<F%2>%1", _freeze_record.playlist->name(), n);

		if (_session.playlists()->by_name (candidate) == 0) {
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

	if ((res = _session.write_one_track (*this, _session.current_start_sample(), _session.current_end_sample(),
					true, srcs, itt, main_outs(), false, false, true, "")) == 0) {
		return;
	}

	_freeze_record.processor_info.clear ();

	{
		Glib::Threads::RWLock::ReaderLock lm (_processor_lock);

		for (ProcessorList::iterator r = _processors.begin(); r != _processors.end(); ++r) {

			if (boost::dynamic_pointer_cast<PeakMeter>(*r)) {
				continue;
			}

			if (!can_freeze_processor (*r)) {
				break;
			}

			FreezeRecordProcessorInfo* frii  = new FreezeRecordProcessorInfo ((*r)->get_state(), (*r));

			frii->id = (*r)->id();

			_freeze_record.processor_info.push_back (frii);

			/* now deactivate the processor, */
			if (!boost::dynamic_pointer_cast<Amp>(*r) && *r != _disk_reader && *r != main_outs()) {
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
	plist.add (Properties::length, srcs[0]->length());
	plist.add (Properties::name, region_name);
	plist.add (Properties::whole_file, true);

	boost::shared_ptr<Region> region (RegionFactory::create (srcs, plist, false));

	new_playlist->set_orig_track_id (id());
#warning NUTEMPO fixme this probably should not use samples unconditionally
	new_playlist->add_region (region, timepos_t (_session.current_start_sample()));
	new_playlist->set_frozen (true);
	region->set_locked (true);

	use_playlist (DataType::AUDIO, boost::dynamic_pointer_cast<AudioPlaylist>(new_playlist));
	_disk_writer->set_record_enabled (false);

	_freeze_record.playlist->use(); // prevent deletion

	/* reset stuff that has already been accounted for in the freeze process */

	gain_control()->set_value (GAIN_COEFF_UNITY, Controllable::NoGroup);
	gain_control()->set_automation_state (Off);

	/* XXX need to use _main_outs _panner->set_automation_state (Off); */

	_freeze_record.state = Frozen;
	FreezeChange(); /* EMIT SIGNAL */
}

void
AudioTrack::unfreeze ()
{
	if (_freeze_record.playlist) {
		_freeze_record.playlist->release();
		use_playlist (DataType::AUDIO, _freeze_record.playlist);

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

	for (vector<FreezeRecordProcessorInfo*>::iterator ii = _freeze_record.processor_info.begin(); ii != _freeze_record.processor_info.end(); ++ii) {
		delete *ii;
	}
	_freeze_record.processor_info.clear ();

	_freeze_record.state = UnFrozen;
	FreezeChange (); /* EMIT SIGNAL */
}

boost::shared_ptr<AudioFileSource>
AudioTrack::write_source (uint32_t n)
{
	assert (_disk_writer);
	return _disk_writer->audio_write_source (n);
}
