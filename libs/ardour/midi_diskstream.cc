/*
    Copyright (C) 2000-2003 Paul Davis 

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

    $Id: diskstream.cc 567 2006-06-07 14:54:12Z trutkin $
*/

#include <fstream>
#include <cstdio>
#include <unistd.h>
#include <cmath>
#include <cerrno>
#include <string>
#include <climits>
#include <fcntl.h>
#include <cstdlib>
#include <ctime>
#include <sys/stat.h>
#include <sys/mman.h>

#include <pbd/error.h>
#include <pbd/basename.h>
#include <glibmm/thread.h>
#include <pbd/xml++.h>

#include <ardour/ardour.h>
#include <ardour/audioengine.h>
#include <ardour/midi_diskstream.h>
#include <ardour/utils.h>
#include <ardour/configuration.h>
#include <ardour/smf_source.h>
#include <ardour/destructive_filesource.h>
#include <ardour/send.h>
#include <ardour/midi_playlist.h>
#include <ardour/cycle_timer.h>
#include <ardour/midi_region.h>

#include "i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

//sigc::signal<void,MidiDiskstream*>    MidiDiskstream::MidiDiskstreamCreated;
sigc::signal<void,list<SMFSource*>*> MidiDiskstream::DeleteSources;
//sigc::signal<void>                MidiDiskstream::DiskOverrun;
//sigc::signal<void>                MidiDiskstream::DiskUnderrun;

MidiDiskstream::MidiDiskstream (Session &sess, const string &name, Diskstream::Flag flag)
	: Diskstream(sess, name, flag)
	, _playlist(NULL)
{
	/* prevent any write sources from being created */

	in_set_state = true;

	init (flag);
	use_new_playlist ();

	in_set_state = false;

	DiskstreamCreated (this); /* EMIT SIGNAL */
}
	
MidiDiskstream::MidiDiskstream (Session& sess, const XMLNode& node)
	: Diskstream(sess, node)
	, _playlist(NULL)
{
	in_set_state = true;
	init (Recordable);

	if (set_state (node)) {
		in_set_state = false;
		throw failed_constructor();
	}

	in_set_state = false;

	if (destructive()) {
		use_destructive_playlist ();
	}

	DiskstreamCreated (this); /* EMIT SIGNAL */
}

void
MidiDiskstream::init (Diskstream::Flag f)
{
	Diskstream::init(f);

	/* there are no channels at this point, so these
	   two calls just get speed_buffer_size and wrap_buffer
	   size setup without duplicating their code.
	*/

	set_block_size (_session.get_block_size());
	allocate_temporary_buffers ();

	/* FIXME: this is now done before the above.  OK? */
	/*pending_overwrite = false;
	overwrite_frame = 0;
	overwrite_queued = false;
	input_change_pending = NoChange;*/

	_n_channels = 1;
}

MidiDiskstream::~MidiDiskstream ()
{
	Glib::Mutex::Lock lm (state_lock);

	if (_playlist)
		_playlist->unref ();
}
/*
void
MidiDiskstream::handle_input_change (IOChange change, void *src)
{
	Glib::Mutex::Lock lm (state_lock);

	if (!(input_change_pending & change)) {
		input_change_pending = IOChange (input_change_pending|change);
		_session.request_input_change_handling ();
	}
}
*/
void
MidiDiskstream::non_realtime_input_change ()
{
}

void
MidiDiskstream::get_input_sources ()
{
}		

int
MidiDiskstream::find_and_use_playlist (const string& name)
{
	Playlist* pl;
	MidiPlaylist* playlist;
		
	if ((pl = _session.get_playlist (name)) == 0) {
		error << string_compose(_("MidiDiskstream: Session doesn't know about a Playlist called \"%1\""), name) << endmsg;
		return -1;
	}

	if ((playlist = dynamic_cast<MidiPlaylist*> (pl)) == 0) {
		error << string_compose(_("MidiDiskstream: Playlist \"%1\" isn't an midi playlist"), name) << endmsg;
		return -1;
	}

	return use_playlist (playlist);
}

int
MidiDiskstream::use_playlist (Playlist* playlist)
{
	assert(dynamic_cast<MidiPlaylist*>(playlist));

	{
		Glib::Mutex::Lock lm (state_lock);

		if (playlist == _playlist) {
			return 0;
		}

		plstate_connection.disconnect();
		plmod_connection.disconnect ();
		plgone_connection.disconnect ();

		if (_playlist) {
			_playlist->unref();
		}
			
		_playlist = dynamic_cast<MidiPlaylist*>(playlist);
		_playlist->ref();

		if (!in_set_state && recordable()) {
			reset_write_sources (false);
		}
		
		plstate_connection = _playlist->StateChanged.connect (mem_fun (*this, &MidiDiskstream::playlist_changed));
		plmod_connection = _playlist->Modified.connect (mem_fun (*this, &MidiDiskstream::playlist_modified));
		plgone_connection = _playlist->GoingAway.connect (mem_fun (*this, &MidiDiskstream::playlist_deleted));
	}

	if (!overwrite_queued) {
		_session.request_overwrite_buffer (this);
		overwrite_queued = true;
	}
	
	PlaylistChanged (); /* EMIT SIGNAL */
	_session.set_dirty ();

	return 0;
}

int
MidiDiskstream::use_new_playlist ()
{
	string newname;
	MidiPlaylist* playlist;

	if (!in_set_state && destructive()) {
		return 0;
	}

	if (_playlist) {
		newname = Playlist::bump_name (_playlist->name(), _session);
	} else {
		newname = Playlist::bump_name (_name, _session);
	}

	if ((playlist = new MidiPlaylist (_session, newname, hidden())) != 0) {
		playlist->set_orig_diskstream_id (id());
		return use_playlist (playlist);
	} else { 
		return -1;
	}
}

int
MidiDiskstream::use_copy_playlist ()
{
	if (destructive()) {
		return 0;
	}

	if (_playlist == 0) {
		error << string_compose(_("MidiDiskstream %1: there is no existing playlist to make a copy of!"), _name) << endmsg;
		return -1;
	}

	string newname;
	MidiPlaylist* playlist;

	newname = Playlist::bump_name (_playlist->name(), _session);
	
	if ((playlist  = new MidiPlaylist (*_playlist, newname)) != 0) {
		playlist->set_orig_diskstream_id (id());
		return use_playlist (playlist);
	} else { 
		return -1;
	}
}


void
MidiDiskstream::playlist_deleted (Playlist* pl)
{
	/* this catches an ordering issue with session destruction. playlists 
	   are destroyed before diskstreams. we have to invalidate any handles
	   we have to the playlist.
	*/

	_playlist = 0;
}


void
MidiDiskstream::setup_destructive_playlist ()
{
	/* a single full-sized region */

	//MidiRegion* region = new MidiRegion (srcs, 0, max_frames, _name);
	//_playlist->add_region (*region, 0);		
}

void
MidiDiskstream::use_destructive_playlist ()
{
	/* use the sources associated with the single full-extent region */
	
	Playlist::RegionList* rl = _playlist->regions_at (0);

	if (rl->empty()) {
		reset_write_sources (false, true);
		return;
	}

	MidiRegion* region = dynamic_cast<MidiRegion*> (rl->front());

	if (region == 0) {
		throw failed_constructor();
	}

	delete rl;

	/* the source list will never be reset for a destructive track */
}

void
MidiDiskstream::set_io (IO& io)
{
	_io = &io;
	set_align_style_from_io ();
}

void
MidiDiskstream::non_realtime_set_speed ()
{
	if (_buffer_reallocation_required)
	{
		Glib::Mutex::Lock lm (state_lock);
		allocate_temporary_buffers ();

		_buffer_reallocation_required = false;
	}

	if (_seek_required) {
		if (speed() != 1.0f || speed() != -1.0f) {
			seek ((jack_nframes_t) (_session.transport_frame() * (double) speed()), true);
		}
		else {
			seek (_session.transport_frame(), true);
		}

		_seek_required = false;
	}
}

void
MidiDiskstream::check_record_status (jack_nframes_t transport_frame, jack_nframes_t nframes, bool can_record)
{
}

int
MidiDiskstream::process (jack_nframes_t transport_frame, jack_nframes_t nframes, jack_nframes_t offset, bool can_record, bool rec_monitors_input)
{
	return 0;
}

bool
MidiDiskstream::commit (jack_nframes_t nframes)
{
	return 0;
}

void
MidiDiskstream::set_pending_overwrite (bool yn)
{
	/* called from audio thread, so we can use the read ptr and playback sample as we wish */
	
	pending_overwrite = yn;

	overwrite_frame = playback_sample;
	//overwrite_offset = channels.front().playback_buf->get_read_ptr();
}

int
MidiDiskstream::overwrite_existing_buffers ()
{
	return 0;
}

int
MidiDiskstream::seek (jack_nframes_t frame, bool complete_refill)
{
	return 0;
}

int
MidiDiskstream::can_internal_playback_seek (jack_nframes_t distance)
{
	return 0;
}

int
MidiDiskstream::internal_playback_seek (jack_nframes_t distance)
{
	return 0;
}

int
MidiDiskstream::read (RawMidi* buf, RawMidi* mixdown_buffer, char * workbuf, jack_nframes_t& start, jack_nframes_t cnt, bool reversed)
{
	return 0;
}

int
MidiDiskstream::do_refill (RawMidi* mixdown_buffer, float* gain_buffer, char * workbuf)
{
	return 0;
}	

int
MidiDiskstream::do_flush (char * workbuf, bool force_flush)
{
	return 0;
}

void
MidiDiskstream::transport_stopped (struct tm& when, time_t twhen, bool abort_capture)
{
}

void
MidiDiskstream::finish_capture (bool rec_monitors_input)
{
}

void
MidiDiskstream::set_record_enabled (bool yn, void* src)
{
}

XMLNode&
MidiDiskstream::get_state ()
{
	XMLNode* node = new XMLNode ("MidiDiskstream");
	char buf[64];
	LocaleGuard lg (X_("POSIX"));

	snprintf (buf, sizeof(buf), "0x%x", _flags);
	node->add_property ("flags", buf);

	node->add_property ("playlist", _playlist->name());
	
	snprintf (buf, sizeof(buf), "%f", _visible_speed);
	node->add_property ("speed", buf);

	node->add_property("name", _name);
	snprintf (buf, sizeof(buf), "%" PRIu64, id());
	node->add_property("id", buf);

	if (!_capturing_sources.empty() && _session.get_record_enabled()) {

		XMLNode* cs_child = new XMLNode (X_("CapturingSources"));
		XMLNode* cs_grandchild;

		for (vector<SMFSource*>::iterator i = _capturing_sources.begin(); i != _capturing_sources.end(); ++i) {
			cs_grandchild = new XMLNode (X_("file"));
			cs_grandchild->add_property (X_("path"), (*i)->path());
			cs_child->add_child_nocopy (*cs_grandchild);
		}

		/* store the location where capture will start */

		Location* pi;

		if (_session.get_punch_in() && ((pi = _session.locations()->auto_punch_location()) != 0)) {
			snprintf (buf, sizeof (buf), "%" PRIu32, pi->start());
		} else {
			snprintf (buf, sizeof (buf), "%" PRIu32, _session.transport_frame());
		}

		cs_child->add_property (X_("at"), buf);
		node->add_child_nocopy (*cs_child);
	}

	if (_extra_xml) {
		node->add_child_copy (*_extra_xml);
	}

	return* node;
}

int
MidiDiskstream::set_state (const XMLNode& node)
{
	const XMLProperty* prop;
	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;
	uint32_t nchans = 1;
	XMLNode* capture_pending_node = 0;
	LocaleGuard lg (X_("POSIX"));

	in_set_state = true;

 	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
 		if ((*niter)->name() == IO::state_node_name) {
			deprecated_io_node = new XMLNode (**niter);
 		}

		if ((*niter)->name() == X_("CapturingSources")) {
			capture_pending_node = *niter;
		}
 	}

	/* prevent write sources from being created */
	
	in_set_state = true;
	
	if ((prop = node.property ("name")) != 0) {
		_name = prop->value();
	} 

	if (deprecated_io_node) {
		if ((prop = deprecated_io_node->property ("id")) != 0) {
			sscanf (prop->value().c_str(), "%" PRIu64, &_id);
		}
	} else {
		if ((prop = node.property ("id")) != 0) {
			sscanf (prop->value().c_str(), "%" PRIu64, &_id);
		}
	}

	if ((prop = node.property ("flags")) != 0) {
		_flags = strtol (prop->value().c_str(), 0, 0);
	}

	if ((prop = node.property ("channels")) != 0) {
		nchans = atoi (prop->value().c_str());
	}
	
	if ((prop = node.property ("playlist")) == 0) {
		return -1;
	}

	{
		bool had_playlist = (_playlist != 0);
	
		if (find_and_use_playlist (prop->value())) {
			return -1;
		}

		if (!had_playlist) {
			_playlist->set_orig_diskstream_id (_id);
		}
		
		if (!destructive() && capture_pending_node) {
			/* destructive streams have one and only one source per channel,
			   and so they never end up in pending capture in any useful
			   sense.
			*/
			use_pending_capture_data (*capture_pending_node);
		}

	}

	if ((prop = node.property ("speed")) != 0) {
		double sp = atof (prop->value().c_str());

		if (realtime_set_speed (sp, false)) {
			non_realtime_set_speed ();
		}
	}

	in_set_state = false;

	/* make sure this is clear before we do anything else */

	_capturing_sources.clear ();

	/* write sources are handled when we handle the input set 
	   up of the IO that owns this DS (::non_realtime_input_change())
	*/
		
	in_set_state = false;

	return 0;
}

int
MidiDiskstream::use_new_write_source (uint32_t n)
{
	return 0;
}

void
MidiDiskstream::reset_write_sources (bool mark_write_complete, bool force)
{
}

int
MidiDiskstream::rename_write_sources ()
{
	return 0;
}

void
MidiDiskstream::set_block_size (jack_nframes_t nframes)
{
}

void
MidiDiskstream::allocate_temporary_buffers ()
{
}

void
MidiDiskstream::monitor_input (bool yn)
{
}

void
MidiDiskstream::set_align_style_from_io ()
{
}


float
MidiDiskstream::playback_buffer_load () const
{
	return 0;
}

float
MidiDiskstream::capture_buffer_load () const
{
	return 0;
}


int
MidiDiskstream::use_pending_capture_data (XMLNode& node)
{
	return 0;
}
