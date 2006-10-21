/*
  Copyright (C) 1999-2002 Paul Davis 

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
    
  $Id$
*/

#define __STDC_FORMAT_MACROS 1
#include <stdint.h>

#include <algorithm>
#include <fstream>
#include <string>
#include <cerrno>

#include <sigc++/bind.h>

#include <cstdio> /* snprintf(3) ... grrr */
#include <cmath>
#include <unistd.h>
#include <sys/stat.h>
#include <climits>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <dirent.h>

#ifdef HAVE_SYS_VFS_H
#include <sys/vfs.h>
#else
#include <sys/mount.h>
#include <sys/param.h>
#endif

#include <glibmm.h>

#include <midi++/mmc.h>
#include <midi++/port.h>
#include <pbd/error.h>

#include <glibmm/thread.h>
#include <pbd/pathscanner.h>
#include <pbd/pthread_utils.h>
#include <pbd/strsplit.h>

#include <ardour/audioengine.h>
#include <ardour/configuration.h>
#include <ardour/session.h>
#include <ardour/buffer.h>
#include <ardour/audio_diskstream.h>
#include <ardour/midi_diskstream.h>
#include <ardour/utils.h>
#include <ardour/audioplaylist.h>
#include <ardour/midi_playlist.h>
#include <ardour/smf_source.h>
#include <ardour/audiofilesource.h>
#include <ardour/destructive_filesource.h>
#include <ardour/midi_source.h>
#include <ardour/sndfile_helpers.h>
#include <ardour/auditioner.h>
#include <ardour/export.h>
#include <ardour/redirect.h>
#include <ardour/send.h>
#include <ardour/insert.h>
#include <ardour/connection.h>
#include <ardour/slave.h>
#include <ardour/tempo.h>
#include <ardour/audio_track.h>
#include <ardour/midi_track.h>
#include <ardour/cycle_timer.h>
#include <ardour/utils.h>
#include <ardour/named_selection.h>
#include <ardour/version.h>
#include <ardour/location.h>
#include <ardour/audioregion.h>
#include <ardour/midi_region.h>
#include <ardour/crossfade.h>
#include <ardour/control_protocol_manager.h>
#include <ardour/region_factory.h>
#include <ardour/source_factory.h>

#include <control_protocol/control_protocol.h>

#include "i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

void
Session::first_stage_init (string fullpath, string snapshot_name)
{
	if (fullpath.length() == 0) {
		throw failed_constructor();
	}

	char buf[PATH_MAX+1];
	if (!realpath (fullpath.c_str(), buf) && (errno != ENOENT)) {
		error << string_compose(_("Could not use path %1 (%s)"), buf, strerror(errno)) << endmsg;
		throw failed_constructor();
	}

	_path = string(buf);

	if (_path[_path.length()-1] != '/') {
		_path += '/';
	}

	/* these two are just provisional settings. set_state()
	   will likely override them.
	*/

	_name = _current_snapshot_name = snapshot_name;

	_current_frame_rate = _engine.frame_rate ();
	_tempo_map = new TempoMap (_current_frame_rate);
	_tempo_map->StateChanged.connect (mem_fun (*this, &Session::tempo_map_changed));

	g_atomic_int_set (&processing_prohibited, 0);
	send_cnt = 0;
	insert_cnt = 0;
	_transport_speed = 0;
	_last_transport_speed = 0;
	transport_sub_state = 0;
	_transport_frame = 0;
	last_stop_frame = 0;
	end_location = new Location (0, 0, _("end"), Location::Flags ((Location::IsMark|Location::IsEnd)));
	start_location = new Location (0, 0, _("start"), Location::Flags ((Location::IsMark|Location::IsStart)));
	_end_location_is_free = true;
	g_atomic_int_set (&_record_status, Disabled);
	loop_changing = false;
	play_loop = false;
	_last_roll_location = 0;
	_last_record_location = 0;
	pending_locate_frame = 0;
	pending_locate_roll = false;
	pending_locate_flush = false;
	dstream_buffer_size = 0;
	state_tree = 0;
	state_was_pending = false;
	set_next_event ();
	outbound_mtc_smpte_frame = 0;
	next_quarter_frame_to_send = -1;
	current_block_size = 0;
	solo_update_disabled = false;
	currently_soloing = false;
	_have_captured = false;
	_worst_output_latency = 0;
	_worst_input_latency = 0;
	_worst_track_latency = 0;
	_state_of_the_state = StateOfTheState(CannotSave|InitialConnecting|Loading);
	_slave = 0;
	butler_mixdown_buffer = 0;
	butler_gain_buffer = 0;
	mmc = 0;
	session_send_mmc = false;
	session_send_mtc = false;
	post_transport_work = PostTransportWork (0);
	g_atomic_int_set (&butler_should_do_transport_work, 0);
	g_atomic_int_set (&butler_active, 0);
	g_atomic_int_set (&_playback_load, 100);
	g_atomic_int_set (&_capture_load, 100);
	g_atomic_int_set (&_playback_load_min, 100);
	g_atomic_int_set (&_capture_load_min, 100);
	_play_range = false;
	waiting_to_start = false;
	_exporting = false;
	_gain_automation_buffer = 0;
	_pan_automation_buffer = 0;
	_npan_buffers = 0;
	pending_abort = false;
	destructive_index = 0;
	current_trans = 0;
	first_file_data_format_reset = true;
	first_file_header_format_reset = true;

	AudioDiskstream::allocate_working_buffers();

	/* default short fade = 15ms */

	Crossfade::set_short_xfade_length ((nframes_t) floor (Config->get_short_xfade_seconds() * frame_rate()));
	DestructiveFileSource::setup_standard_crossfades (frame_rate());

	last_mmc_step.tv_sec = 0;
	last_mmc_step.tv_usec = 0;
	step_speed = 0.0;

	/* click sounds are unset by default, which causes us to internal
	   waveforms for clicks.
	*/
	
	click_data = 0;
	click_emphasis_data = 0;
	click_length = 0;
	click_emphasis_length = 0;
	_clicking = false;

	process_function = &Session::process_with_events;

	if (Config->get_use_video_sync()) {
		waiting_for_sync_offset = true;
	} else {
		waiting_for_sync_offset = false;
	}

	_current_frame_rate = 48000;
	_base_frame_rate = 48000;

	last_smpte_when = 0;
	_smpte_offset = 0;
	_smpte_offset_negative = true;
	last_smpte_valid = false;

	sync_time_vars ();

	last_rr_session_dir = session_dirs.begin();
	refresh_disk_space ();

	// set_default_fade (0.2, 5.0); /* steepness, millisecs */

	/* slave stuff */

	average_slave_delta = 1800;
	have_first_delta_accumulator = false;
	delta_accumulator_cnt = 0;
	slave_state = Stopped;

	_engine.GraphReordered.connect (mem_fun (*this, &Session::graph_reordered));

	/* These are all static "per-class" signals */

	RegionFactory::CheckNewRegion.connect (mem_fun (*this, &Session::add_region));
	SourceFactory::SourceCreated.connect (mem_fun (*this, &Session::add_source));
	Playlist::PlaylistCreated.connect (mem_fun (*this, &Session::add_playlist));
	Redirect::RedirectCreated.connect (mem_fun (*this, &Session::add_redirect));
	NamedSelection::NamedSelectionCreated.connect (mem_fun (*this, &Session::add_named_selection));
	Curve::CurveCreated.connect (mem_fun (*this, &Session::add_curve));
	AutomationList::AutomationListCreated.connect (mem_fun (*this, &Session::add_automation_list));

	Controllable::GoingAway.connect (mem_fun (*this, &Session::remove_controllable));

	IO::MoreChannels.connect (mem_fun (*this, &Session::ensure_buffers));

	/* stop IO objects from doing stuff until we're ready for them */

	IO::disable_panners ();
	IO::disable_ports ();
	IO::disable_connecting ();
}

int
Session::second_stage_init (bool new_session)
{
	AudioFileSource::set_peak_dir (peak_dir());

	if (!new_session) {
		if (load_state (_current_snapshot_name)) {
			return -1;
		}
		remove_empty_sounds ();
	}

	if (start_butler_thread()) {
		return -1;
	}

	/*if (start_midi_thread ()) {
		return -1;
	}*/

	// set_state() will call setup_raid_path(), but if it's a new session we need
	// to call setup_raid_path() here.
	if (state_tree) {
		if (set_state (*state_tree->root())) {
			return -1;
		}
	} else {
		setup_raid_path(_path);
	}

	/* we can't save till after ::when_engine_running() is called,
	   because otherwise we save state with no connections made.
	   therefore, we reset _state_of_the_state because ::set_state()
	   will have cleared it.

	   we also have to include Loading so that any events that get
	   generated between here and the end of ::when_engine_running()
	   will be processed directly rather than queued.
	*/

	_state_of_the_state = StateOfTheState (_state_of_the_state|CannotSave|Loading);

 	// set_auto_input (true);
	_locations.changed.connect (mem_fun (this, &Session::locations_changed));
	_locations.added.connect (mem_fun (this, &Session::locations_added));
	setup_click_sounds (0);
	setup_midi_control ();

	/* Pay attention ... */

	_engine.Halted.connect (mem_fun (*this, &Session::engine_halted));
	_engine.Xrun.connect (mem_fun (*this, &Session::xrun_recovery));

	if (_engine.running()) {
		when_engine_running();
	} else {
		first_time_running = _engine.Running.connect (mem_fun (*this, &Session::when_engine_running));
	}

	//send_full_time_code ();
	_engine.transport_locate (0);
	deliver_mmc (MIDI::MachineControl::cmdMmcReset, 0);
	deliver_mmc (MIDI::MachineControl::cmdLocate, 0);

	ControlProtocolManager::instance().set_session (*this);

	if (new_session) {
		_end_location_is_free = true;
	} else {
		_end_location_is_free = false;
	}
	
	return 0;
}

string
Session::raid_path () const
{
	string path;

	for (vector<space_and_path>::const_iterator i = session_dirs.begin(); i != session_dirs.end(); ++i) {
		path += (*i).path;
		path += ':';
	}
	
	return path.substr (0, path.length() - 1); // drop final colon
}

void
Session::setup_raid_path (string path)
{
	string::size_type colon;
	string remaining;
	space_and_path sp;
	string fspath;
	string::size_type len = path.length();
	int colons;

	colons = 0;

	if (path.length() == 0) {
		return;
	}

	session_dirs.clear ();

	for (string::size_type n = 0; n < len; ++n) {
		if (path[n] == ':') {
			colons++;
		}
	}

	if (colons == 0) {

		/* no multiple search path, just one location (common case) */

		sp.path = path;
		sp.blocks = 0;
		session_dirs.push_back (sp);

		string fspath;

		/* sounds dir */

		fspath += sp.path;
		if (fspath[fspath.length()-1] != '/') {
			fspath += '/';
		}
		fspath += sound_dir (false);
		
		AudioFileSource::set_search_path (fspath);
		SMFSource::set_search_path (fspath); // FIXME: should be different

		return;
	}

	remaining = path;

	while ((colon = remaining.find_first_of (':')) != string::npos) {
		
		sp.blocks = 0;
		sp.path = remaining.substr (0, colon);
		session_dirs.push_back (sp);

		/* add sounds to file search path */

		fspath += sp.path;
		if (fspath[fspath.length()-1] != '/') {
			fspath += '/';
		}
		fspath += sound_dir (false);
		fspath += ':';

		remaining = remaining.substr (colon+1);
	}

	if (remaining.length()) {

		sp.blocks = 0;
		sp.path = remaining;

		fspath += ':';
		fspath += sp.path;
		if (fspath[fspath.length()-1] != '/') {
			fspath += '/';
		}
		fspath += sound_dir (false);
		fspath += ':';

		session_dirs.push_back (sp);
	}

	/* set the AudioFileSource search path */

	AudioFileSource::set_search_path (fspath);
	SMFSource::set_search_path (fspath); // FIXME: should be different

	/* reset the round-robin soundfile path thingie */

	last_rr_session_dir = session_dirs.begin();
}

int
Session::create (bool& new_session, string* mix_template, nframes_t initial_length)
{
	string dir;

	if (g_mkdir_with_parents (_path.c_str(), 0755) < 0) {
		error << string_compose(_("Session: cannot create session dir \"%1\" (%2)"), _path, strerror (errno)) << endmsg;
		return -1;
	}

	dir = peak_dir ();

	if (g_mkdir_with_parents (dir.c_str(), 0755) < 0) {
		error << string_compose(_("Session: cannot create session peakfile dir \"%1\" (%2)"), dir, strerror (errno)) << endmsg;
		return -1;
	}

	dir = sound_dir ();

	if (g_mkdir_with_parents (dir.c_str(), 0755) < 0) {
		error << string_compose(_("Session: cannot create session sounds dir \"%1\" (%2)"), dir, strerror (errno)) << endmsg;
		return -1;
	}

	dir = dead_sound_dir ();

	if (g_mkdir_with_parents (dir.c_str(), 0755) < 0) {
		error << string_compose(_("Session: cannot create session dead sounds dir \"%1\" (%2)"), dir, strerror (errno)) << endmsg;
		return -1;
	}

	dir = automation_dir ();

	if (g_mkdir_with_parents (dir.c_str(), 0755) < 0) {
		error << string_compose(_("Session: cannot create session automation dir \"%1\" (%2)"), dir, strerror (errno)) << endmsg;
		return -1;
	}


	/* check new_session so we don't overwrite an existing one */

	if (mix_template) {
		std::string in_path = *mix_template;

		ifstream in(in_path.c_str());

		if (in){
			string out_path = _path;
			out_path += _name;
			out_path += _statefile_suffix;

			ofstream out(out_path.c_str());

			if (out){
				out << in.rdbuf();

				// okay, session is set up.  Treat like normal saved
				// session from now on.

				new_session = false;
				return 0;

			} else {
				error << string_compose (_("Could not open %1 for writing mix template"), out_path) 
					<< endmsg;
				return -1;
			}

		} else {
			error << string_compose (_("Could not open mix template %1 for reading"), in_path) 
				<< endmsg;
			return -1;
		}

	}

	/* set initial start + end point */

	start_location->set_end (0);
	_locations.add (start_location);

	end_location->set_end (initial_length);
	_locations.add (end_location);

	_state_of_the_state = Clean;

	if (save_state (_current_snapshot_name)) {
		save_history (_current_snapshot_name);
		return -1;
	}

	return 0;
}

int
Session::load_diskstreams (const XMLNode& node)
{
	XMLNodeList          clist;
	XMLNodeConstIterator citer;
	
	clist = node.children();

	for (citer = clist.begin(); citer != clist.end(); ++citer) {

		try {
			/* diskstreams added automatically by DiskstreamCreated handler */
			if ((*citer)->name() == "AudioDiskstream" || (*citer)->name() == "DiskStream") {
				boost::shared_ptr<AudioDiskstream> dstream (new AudioDiskstream (*this, **citer));
				add_diskstream (dstream);
			} else if ((*citer)->name() == "MidiDiskstream") {
				boost::shared_ptr<MidiDiskstream> dstream (new MidiDiskstream (*this, **citer));
				add_diskstream (dstream);
			} else {
				error << _("Session: unknown diskstream type in XML") << endmsg;
			}
		} 
		
		catch (failed_constructor& err) {
			error << _("Session: could not load diskstream via XML state") << endmsg;
			return -1;
		}
	}

	return 0;
}

void
Session::remove_pending_capture_state ()
{
	string xml_path;

	xml_path = _path;
	xml_path += _current_snapshot_name;
	xml_path += _pending_suffix;

	unlink (xml_path.c_str());
}

int
Session::save_state (string snapshot_name, bool pending)
{
	XMLTree tree;
	string xml_path;
	string bak_path;

	if (_state_of_the_state & CannotSave) {
		return 1;
	}

	tree.set_root (&get_state());

	if (snapshot_name.empty()) {
		snapshot_name = _current_snapshot_name;
	}

	if (!pending) {

		xml_path = _path;
		xml_path += snapshot_name;
		xml_path += _statefile_suffix;
		bak_path = xml_path;
		bak_path += ".bak";
		
		// Make backup of state file
		
		if ((access (xml_path.c_str(), F_OK) == 0) &&
		    (rename(xml_path.c_str(), bak_path.c_str()))) {
			error << _("could not backup old state file, current state not saved.")	<< endmsg;
			return -1;
		}

	} else {

		xml_path = _path;
		xml_path += snapshot_name;
		xml_path += _pending_suffix;

	}

	cerr << "actually writing state\n";

	if (!tree.write (xml_path)) {
		error << string_compose (_("state could not be saved to %1"), xml_path) << endmsg;

		/* don't leave a corrupt file lying around if it is
		   possible to fix.
		*/

		if (unlink (xml_path.c_str())) {
			error << string_compose (_("could not remove corrupt state file %1"), xml_path) << endmsg;
		} else {
			if (!pending) {
				if (rename (bak_path.c_str(), xml_path.c_str())) {
					error << string_compose (_("could not restore state file from backup %1"), bak_path) << endmsg;
				}
			}
		}

		return -1;
	}

	if (!pending) {
                save_history(snapshot_name);

		bool was_dirty = dirty();

		_state_of_the_state = StateOfTheState (_state_of_the_state & ~Dirty);
		
		if (was_dirty) {
			DirtyChanged (); /* EMIT SIGNAL */
		}
		
		StateSaved (snapshot_name); /* EMIT SIGNAL */
	}

	return 0;
}

int
Session::restore_state (string snapshot_name)
{
	if (load_state (snapshot_name) == 0) {
		set_state (*state_tree->root());
	}
	
	return 0;
}

int
Session::load_state (string snapshot_name)
{
	if (state_tree) {
		delete state_tree;
		state_tree = 0;
	}

	string xmlpath;
	
	state_was_pending = false;

	/* check for leftover pending state from a crashed capture attempt */

	xmlpath = _path;
	xmlpath += snapshot_name;
	xmlpath += _pending_suffix;

	if (!access (xmlpath.c_str(), F_OK)) {

		/* there is pending state from a crashed capture attempt */

		if (AskAboutPendingState()) {
			state_was_pending = true;
		} 
	} 

	if (!state_was_pending) {

		xmlpath = _path;
		xmlpath += snapshot_name;
		xmlpath += _statefile_suffix;
	}

	if (access (xmlpath.c_str(), F_OK)) {
		error << string_compose(_("%1: session state information file \"%2\" doesn't exist!"), _name, xmlpath) << endmsg;
		return 1;
	}

	state_tree = new XMLTree;

	set_dirty();

	if (state_tree->read (xmlpath)) {
		return 0;
	} else {
		error << string_compose(_("Could not understand ardour file %1"), xmlpath) << endmsg;
	}

	delete state_tree;
	state_tree = 0;
	return -1;
}

int
Session::load_options (const XMLNode& node)
{
	XMLNode* child;
	XMLProperty* prop;
	LocaleGuard lg (X_("POSIX"));

	Config->set_variables (node, ConfigVariableBase::Session);

	if ((child = find_named_node (node, "end-marker-is-free")) != 0) {
		if ((prop = child->property ("val")) != 0) {
			_end_location_is_free = (prop->value() == "yes");
		}
	}

	return 0;
}

bool
Session::save_config_options_predicate (ConfigVariableBase::Owner owner) const
{
	const ConfigVariableBase::Owner modified_by_session_or_user = (ConfigVariableBase::Owner)
		(ConfigVariableBase::Session|ConfigVariableBase::Interface);

	return owner & modified_by_session_or_user;
}

XMLNode&
Session::get_options () const
{
	XMLNode* child;
	LocaleGuard lg (X_("POSIX"));

	XMLNode& option_root = Config->get_variables (mem_fun (*this, &Session::save_config_options_predicate));

	child = option_root.add_child ("end-marker-is-free");
	child->add_property ("val", _end_location_is_free ? "yes" : "no");

	return option_root;
}

XMLNode&
Session::get_state()
{
	return state(true);
}

XMLNode&
Session::get_template()
{
	/* if we don't disable rec-enable, diskstreams
	   will believe they need to store their capture
	   sources in their state node. 
	*/
	
	disable_record (false);

	return state(false);
}

XMLNode&
Session::state(bool full_state)
{
	XMLNode* node = new XMLNode("Session");
	XMLNode* child;

	// store libardour version, just in case
	char buf[16];
	snprintf(buf, sizeof(buf)-1, "%d.%d.%d", 
		 libardour_major_version, libardour_minor_version, libardour_micro_version);
	node->add_property("version", string(buf));
		
	/* store configuration settings */

	if (full_state) {
	
		/* store the name */
		node->add_property ("name", _name);

		if (session_dirs.size() > 1) {

			string p;

			vector<space_and_path>::iterator i = session_dirs.begin();
			vector<space_and_path>::iterator next;

			++i; /* skip the first one */
			next = i;
			++next;

			while (i != session_dirs.end()) {

				p += (*i).path;

				if (next != session_dirs.end()) {
					p += ':';
				} else {
					break;
				}

				++next;
				++i;
			}
			
			child = node->add_child ("Path");
			child->add_content (p);
		}
	}

	/* save the ID counter */
	
	snprintf (buf, sizeof (buf), "%" PRIu64, ID::counter());
	node->add_property ("id-counter", buf);

	/* various options */

	node->add_child_nocopy (get_options());

	child = node->add_child ("Sources");

	if (full_state) {
		Glib::Mutex::Lock sl (source_lock);

		for (SourceMap::iterator siter = sources.begin(); siter != sources.end(); ++siter) {
			
			/* Don't save information about AudioFileSources that are empty */
			
			boost::shared_ptr<AudioFileSource> fs;

			if ((fs = boost::dynamic_pointer_cast<AudioFileSource> (siter->second)) != 0) {
				boost::shared_ptr<DestructiveFileSource> dfs = boost::dynamic_pointer_cast<DestructiveFileSource> (fs);

				/* Don't save sources that are empty, unless they're destructive (which are OK
				   if they are empty, because we will re-use them every time.)
				*/
				if ( ! dfs && siter->second->length() == 0) {
					continue;
				}
			}
			
			child->add_child_nocopy (siter->second->get_state());
		}
	}

	child = node->add_child ("Regions");

	if (full_state) { 
		Glib::Mutex::Lock rl (region_lock);

		for (RegionList::const_iterator i = regions.begin(); i != regions.end(); ++i) {
			
			/* only store regions not attached to playlists */

			if (i->second->playlist() == 0) {
				child->add_child_nocopy (i->second->state (true));
			}
		}
	}

	child = node->add_child ("DiskStreams");

	{ 
		boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();
		for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
			if (!(*i)->hidden()) {
				child->add_child_nocopy ((*i)->get_state());
			}
		}
	}

	node->add_child_nocopy (_locations.get_state());
	
	child = node->add_child ("Connections");
	{
		Glib::Mutex::Lock lm (connection_lock);
		for (ConnectionList::iterator i = _connections.begin(); i != _connections.end(); ++i) {
			if (!(*i)->system_dependent()) {
				child->add_child_nocopy ((*i)->get_state());
			}
		}
	}

	child = node->add_child ("Routes");
	{
		boost::shared_ptr<RouteList> r = routes.reader ();
		
		RoutePublicOrderSorter cmp;
		RouteList public_order (*r);
		public_order.sort (cmp);
		
		for (RouteList::iterator i = public_order.begin(); i != public_order.end(); ++i) {
			if (!(*i)->hidden()) {
				if (full_state) {
					child->add_child_nocopy ((*i)->get_state());
				} else {
					child->add_child_nocopy ((*i)->get_template());
				}
			}
		}
	}

	
	child = node->add_child ("EditGroups");
	for (list<RouteGroup *>::iterator i = edit_groups.begin(); i != edit_groups.end(); ++i) {
		child->add_child_nocopy ((*i)->get_state());
	}

	child = node->add_child ("MixGroups");
	for (list<RouteGroup *>::iterator i = mix_groups.begin(); i != mix_groups.end(); ++i) {
		child->add_child_nocopy ((*i)->get_state());
	}

	child = node->add_child ("Playlists");
	for (PlaylistList::iterator i = playlists.begin(); i != playlists.end(); ++i) {
		if (!(*i)->hidden()) {
			if (!(*i)->empty()) {
				if (full_state) {
					child->add_child_nocopy ((*i)->get_state());
				} else {
					child->add_child_nocopy ((*i)->get_template());
				}
			}
		}
	}

	child = node->add_child ("UnusedPlaylists");
	for (PlaylistList::iterator i = unused_playlists.begin(); i != unused_playlists.end(); ++i) {
		if (!(*i)->hidden()) {
			if (!(*i)->empty()) {
				if (full_state) {
					child->add_child_nocopy ((*i)->get_state());
				} else {
					child->add_child_nocopy ((*i)->get_template());
				}
			}
		}
	}
	
	
	if (_click_io) {
		child = node->add_child ("Click");
		child->add_child_nocopy (_click_io->state (full_state));
	}

	if (full_state) {
		child = node->add_child ("NamedSelections");
		for (NamedSelectionList::iterator i = named_selections.begin(); i != named_selections.end(); ++i) {
			if (full_state) {
				child->add_child_nocopy ((*i)->get_state());
			} 
		}
	}

	node->add_child_nocopy (_tempo_map->get_state());

	node->add_child_nocopy (get_control_protocol_state());

	if (_extra_xml) {
		node->add_child_copy (*_extra_xml);
	}

	return *node;
}

XMLNode&
Session::get_control_protocol_state ()
{
	ControlProtocolManager& cpm (ControlProtocolManager::instance());
	XMLNode* node = new XMLNode (X_("ControlProtocols"));

	cpm.foreach_known_protocol (bind (mem_fun (*this, &Session::add_control_protocol), node));
	
	return *node;
}

void
Session::add_control_protocol (const ControlProtocolInfo* const cpi, XMLNode* node)
{
	if (cpi->protocol) {
		node->add_child_nocopy (cpi->protocol->get_state());
	}
}

int
Session::set_state (const XMLNode& node)
{
	XMLNodeList nlist;
	XMLNode* child;
	const XMLProperty* prop;
	int ret = -1;

	_state_of_the_state = StateOfTheState (_state_of_the_state|CannotSave);
	
	if (node.name() != X_("Session")){
		fatal << _("programming error: Session: incorrect XML node sent to set_state()") << endmsg;
		return -1;
	}

	StateManager::prohibit_save ();

	if ((prop = node.property ("name")) != 0) {
		_name = prop->value ();
	}

	setup_raid_path(_path);

	if ((prop = node.property (X_("id-counter"))) != 0) {
		uint64_t x;
		sscanf (prop->value().c_str(), "%" PRIu64, &x);
		ID::init_counter (x);
	} else {
		/* old sessions used a timebased counter, so fake
		   the startup ID counter based on a standard
		   timestamp.
		*/
		time_t now;
		time (&now);
		ID::init_counter (now);
	}

	
	IO::disable_ports ();
	IO::disable_connecting ();

	/* Object loading order:

	MIDI
	Path
	extra
	Options/Config
	Sources
	AudioRegions
	AudioDiskstreams
	Connections
	Locations
	Routes
	EditGroups
	MixGroups
	Click
	ControlProtocols
	*/

	if (use_config_midi_ports ()) {
	}

	if ((child = find_named_node (node, "extra")) != 0) {
		_extra_xml = new XMLNode (*child);
	}

	if (((child = find_named_node (node, "Options")) != 0)) { /* old style */
		load_options (*child);
	} else if ((child = find_named_node (node, "Config")) != 0) { /* new style */
		load_options (*child);
	} else {
		error << _("Session: XML state has no options section") << endmsg;
	}

	if ((child = find_named_node (node, "Sources")) == 0) {
		error << _("Session: XML state has no sources section") << endmsg;
		goto out;
	} else if (load_sources (*child)) {
		goto out;
	}

	if ((child = find_named_node (node, "Regions")) == 0) {
		error << _("Session: XML state has no Regions section") << endmsg;
		goto out;
	} else if (load_regions (*child)) {
		goto out;
	}

	if ((child = find_named_node (node, "Playlists")) == 0) {
		error << _("Session: XML state has no playlists section") << endmsg;
		goto out;
	} else if (load_playlists (*child)) {
		goto out;
	}

	if ((child = find_named_node (node, "UnusedPlaylists")) == 0) {
		// this is OK
	} else if (load_unused_playlists (*child)) {
		goto out;
	}
	
	if ((child = find_named_node (node, "NamedSelections")) != 0) {
		if (load_named_selections (*child)) {
			goto out;
		}
	}

	if ((child = find_named_node (node, "DiskStreams")) == 0) {
		error << _("Session: XML state has no diskstreams section") << endmsg;
		goto out;
	} else if (load_diskstreams (*child)) {
		goto out;
	}

	if ((child = find_named_node (node, "Connections")) == 0) {
		error << _("Session: XML state has no connections section") << endmsg;
		goto out;
	} else if (load_connections (*child)) {
		goto out;
	}

	if ((child = find_named_node (node, "Locations")) == 0) {
		error << _("Session: XML state has no locations section") << endmsg;
		goto out;
	} else if (_locations.set_state (*child)) {
		goto out;
	}

	Location* location;

	if ((location = _locations.auto_loop_location()) != 0) {
		set_auto_loop_location (location);
	}

	if ((location = _locations.auto_punch_location()) != 0) {
		set_auto_punch_location (location);
	}

	if ((location = _locations.end_location()) == 0) {
		_locations.add (end_location);
	} else {
		delete end_location;
		end_location = location;
	}

	if ((location = _locations.start_location()) == 0) {
		_locations.add (start_location);
	} else {
		delete start_location;
		start_location = location;
	}

	_locations.save_state (_("initial state"));

	if ((child = find_named_node (node, "EditGroups")) == 0) {
		error << _("Session: XML state has no edit groups section") << endmsg;
		goto out;
	} else if (load_edit_groups (*child)) {
		goto out;
	}

	if ((child = find_named_node (node, "MixGroups")) == 0) {
		error << _("Session: XML state has no mix groups section") << endmsg;
		goto out;
	} else if (load_mix_groups (*child)) {
		goto out;
	}

	if ((child = find_named_node (node, "TempoMap")) == 0) {
		error << _("Session: XML state has no Tempo Map section") << endmsg;
		goto out;
	} else if (_tempo_map->set_state (*child)) {
		goto out;
	}

	if ((child = find_named_node (node, "Routes")) == 0) {
		error << _("Session: XML state has no routes section") << endmsg;
		goto out;
	} else if (load_routes (*child)) {
		goto out;
	}

	if ((child = find_named_node (node, "Click")) == 0) {
		warning << _("Session: XML state has no click section") << endmsg;
	} else if (_click_io) {
		_click_io->set_state (*child);
	}
	
	if ((child = find_named_node (node, "ControlProtocols")) != 0) {
		ControlProtocolManager::instance().set_protocol_states (*child);
	}

	/* here beginneth the second phase ... */

	StateReady (); /* EMIT SIGNAL */

	_state_of_the_state = Clean;

	StateManager::allow_save (_("initial state"), true);

	if (state_was_pending) {
		save_state (_current_snapshot_name);
		remove_pending_capture_state ();
		state_was_pending = false;
	}

	return 0;

  out:
	/* we failed, re-enable state saving but don't actually save internal state */
	StateManager::allow_save (X_("ignored"), false);
	return ret;
}

int
Session::load_routes (const XMLNode& node)
{
	XMLNodeList nlist;
	XMLNodeConstIterator niter;
	RouteList new_routes;

	nlist = node.children();

	set_dirty();

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		boost::shared_ptr<Route> route (XMLRouteFactory (**niter));

		if (route == 0) {
			error << _("Session: cannot create Route from XML description.")			      << endmsg;
			return -1;
		}

		new_routes.push_back (route);
	}

	add_routes (new_routes);

	return 0;
}

boost::shared_ptr<Route>
Session::XMLRouteFactory (const XMLNode& node)
{
	if (node.name() != "Route") {
		return boost::shared_ptr<Route> ((Route*) 0);
	}

	bool has_diskstream = (node.property ("diskstream") != 0 || node.property ("diskstream-id") != 0);
	
	DataType type = DataType::AUDIO;
	const XMLProperty* prop = node.property("default-type");
	if (prop)
		type = DataType(prop->value());
	
	assert(type != DataType::NIL);

	if (has_diskstream) {
		if (type == DataType::AUDIO) {
			boost::shared_ptr<Route> ret (new AudioTrack (*this, node));
			return ret;
		} else {
			boost::shared_ptr<Route> ret (new MidiTrack (*this, node));
			return ret;
		}
	} else {
		boost::shared_ptr<Route> ret (new Route (*this, node));
		return ret;
	}
}

int
Session::load_regions (const XMLNode& node)
{
	XMLNodeList nlist;
	XMLNodeConstIterator niter;
	boost::shared_ptr<Region> region;

	nlist = node.children();

	set_dirty();

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((region = XMLRegionFactory (**niter, false)) == 0) {
			error << _("Session: cannot create Region from XML description.") << endmsg;
		}
	}

	return 0;
}

boost::shared_ptr<Region>
Session::XMLRegionFactory (const XMLNode& node, bool full)
{
	const XMLProperty* type = node.property("type");

	try {
	
	if ( !type || type->value() == "audio" ) {

		return boost::shared_ptr<Region>(XMLAudioRegionFactory (node, full));
	
	} else if (type->value() == "midi") {
		
		return boost::shared_ptr<Region>(XMLMidiRegionFactory (node, full));

	}
	
	} catch (failed_constructor& err) {
		return boost::shared_ptr<Region> ();
	}

	return boost::shared_ptr<Region> ();
}

boost::shared_ptr<AudioRegion>
Session::XMLAudioRegionFactory (const XMLNode& node, bool full)
{
	const XMLProperty* prop;
	boost::shared_ptr<Source> source;
	boost::shared_ptr<AudioSource> as;
	SourceList sources;
	uint32_t nchans = 1;
	char buf[128];
	
	if (node.name() != X_("Region")) {
		return boost::shared_ptr<AudioRegion>();
	}

	if ((prop = node.property (X_("channels"))) != 0) {
		nchans = atoi (prop->value().c_str());
	}


	if ((prop = node.property ("name")) == 0) {
		cerr << "no name for this region\n";
		abort ();
	}
	
	if ((prop = node.property (X_("source-0"))) == 0) {
		if ((prop = node.property ("source")) == 0) {
			error << _("Session: XMLNode describing a AudioRegion is incomplete (no source)") << endmsg;
			return boost::shared_ptr<AudioRegion>();
		}
	}

	PBD::ID s_id (prop->value());

	if ((source = source_by_id (s_id)) == 0) {
		error << string_compose(_("Session: XMLNode describing a AudioRegion references an unknown source id =%1"), s_id) << endmsg;
		return boost::shared_ptr<AudioRegion>();
	}
	
	as = boost::dynamic_pointer_cast<AudioSource>(source);
	if (!as) {
		error << string_compose(_("Session: XMLNode describing a AudioRegion references a non-audio source id =%1"), s_id) << endmsg;
		return boost::shared_ptr<AudioRegion>();
	}

	sources.push_back (as);

	/* pickup other channels */

	for (uint32_t n=1; n < nchans; ++n) {
		snprintf (buf, sizeof(buf), X_("source-%d"), n);
		if ((prop = node.property (buf)) != 0) {
			
			PBD::ID id2 (prop->value());
			
			if ((source = source_by_id (id2)) == 0) {
				error << string_compose(_("Session: XMLNode describing a AudioRegion references an unknown source id =%1"), id2) << endmsg;
				return boost::shared_ptr<AudioRegion>();
			}
			
			as = boost::dynamic_pointer_cast<AudioSource>(source);
			if (!as) {
				error << string_compose(_("Session: XMLNode describing a AudioRegion references a non-audio source id =%1"), id2) << endmsg;
				return boost::shared_ptr<AudioRegion>();
			}
			sources.push_back (as);
		}
	}
	
	try {
		boost::shared_ptr<AudioRegion> region (boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (sources, node)));
		return region;
						       
	}

	catch (failed_constructor& err) {
		return boost::shared_ptr<AudioRegion>();
	}
}

boost::shared_ptr<MidiRegion>
Session::XMLMidiRegionFactory (const XMLNode& node, bool full)
{
	const XMLProperty* prop;
	boost::shared_ptr<Source> source;
	boost::shared_ptr<MidiSource> ms;
	MidiRegion::SourceList sources;
	uint32_t nchans = 1;
	
	if (node.name() != X_("Region")) {
		return boost::shared_ptr<MidiRegion>();
	}

	if ((prop = node.property (X_("channels"))) != 0) {
		nchans = atoi (prop->value().c_str());
	}

	// Multiple midi channels?  that's just crazy talk
	assert(nchans == 1);

	if ((prop = node.property (X_("source-0"))) == 0) {
		if ((prop = node.property ("source")) == 0) {
			error << _("Session: XMLNode describing a MidiRegion is incomplete (no source)") << endmsg;
			return boost::shared_ptr<MidiRegion>();
		}
	}

	PBD::ID s_id (prop->value());

	if ((source = source_by_id (s_id)) == 0) {
		error << string_compose(_("Session: XMLNode describing a MidiRegion references an unknown source id =%1"), s_id) << endmsg;
		return boost::shared_ptr<MidiRegion>();
	}

	ms = boost::dynamic_pointer_cast<MidiSource>(source);
	if (!ms) {
		error << string_compose(_("Session: XMLNode describing a MidiRegion references a non-midi source id =%1"), s_id) << endmsg;
		return boost::shared_ptr<MidiRegion>();
	}

	sources.push_back (ms);

	try {
		boost::shared_ptr<MidiRegion> region (boost::dynamic_pointer_cast<MidiRegion> (RegionFactory::create (sources, node)));
		return region;
	}

	catch (failed_constructor& err) {
		return boost::shared_ptr<MidiRegion>();
	}
}

XMLNode&
Session::get_sources_as_xml ()

{
	XMLNode* node = new XMLNode (X_("Sources"));
	Glib::Mutex::Lock lm (source_lock);

	for (SourceMap::iterator i = sources.begin(); i != sources.end(); ++i) {
		node->add_child_nocopy (i->second->get_state());
	}

	/* XXX get MIDI and other sources here */

	return *node;
}

string
Session::path_from_region_name (string name, string identifier)
{
	char buf[PATH_MAX+1];
	uint32_t n;
	string dir = discover_best_sound_dir ();

	for (n = 0; n < 999999; ++n) {
		if (identifier.length()) {
			snprintf (buf, sizeof(buf), "%s/%s%s%" PRIu32 ".wav", dir.c_str(), name.c_str(), 
				  identifier.c_str(), n);
		} else {
			snprintf (buf, sizeof(buf), "%s/%s-%" PRIu32 ".wav", dir.c_str(), name.c_str(), n);
		}
		if (access (buf, F_OK) != 0) {
			return buf;
		}
	}

	return "";
}
	

int
Session::load_sources (const XMLNode& node)
{
	XMLNodeList nlist;
	XMLNodeConstIterator niter;
	boost::shared_ptr<Source> source;

	nlist = node.children();

	set_dirty();

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		if ((source = XMLSourceFactory (**niter)) == 0) {
			error << _("Session: cannot create Source from XML description.") << endmsg;
		}
	}

	return 0;
}

boost::shared_ptr<Source>
Session::XMLSourceFactory (const XMLNode& node)
{
	if (node.name() != "Source") {
		return boost::shared_ptr<Source>();
	}

	try {
		return SourceFactory::create (*this, node);
	}
	
	catch (failed_constructor& err) {
		error << _("Found a sound file that cannot be used by Ardour. Talk to the progammers.") << endmsg;
		return boost::shared_ptr<Source>();
	}
}

int
Session::save_template (string template_name)
{
	XMLTree tree;
	string xml_path, bak_path, template_path;

	if (_state_of_the_state & CannotSave) {
		return -1;
	}

	DIR* dp;
	string dir = template_dir();

	if ((dp = opendir (dir.c_str()))) {
		closedir (dp);
	} else {
		if (g_mkdir_with_parents (dir.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)) {
			error << string_compose(_("Could not create mix templates directory \"%1\" (%2)"), dir, strerror (errno)) << endmsg;
			return -1;
		}
	}

	tree.set_root (&get_template());

	xml_path = dir;
	xml_path += template_name;
	xml_path += _template_suffix;

	ifstream in(xml_path.c_str());
	
	if (in) {
		warning << string_compose(_("Template \"%1\" already exists - new version not created"), template_name) << endmsg;
		return -1;
	} else {
		in.close();
	}

	if (!tree.write (xml_path)) {
		error << _("mix template not saved") << endmsg;
		return -1;
	}

	return 0;
}

int
Session::rename_template (string old_name, string new_name) 
{
	string old_path = template_dir() + old_name + _template_suffix;
	string new_path = template_dir() + new_name + _template_suffix;

	return rename (old_path.c_str(), new_path.c_str());
}

int
Session::delete_template (string name) 
{
	string template_path = template_dir();
	template_path += name;
	template_path += _template_suffix;

	return remove (template_path.c_str());
}

void
Session::refresh_disk_space ()
{
#if HAVE_SYS_VFS_H
	struct statfs statfsbuf;
	vector<space_and_path>::iterator i;
	Glib::Mutex::Lock lm (space_lock);
	double scale;

	/* get freespace on every FS that is part of the session path */

	_total_free_4k_blocks = 0;
	
	for (i = session_dirs.begin(); i != session_dirs.end(); ++i) {
		statfs ((*i).path.c_str(), &statfsbuf);

		scale = statfsbuf.f_bsize/4096.0;

		(*i).blocks = (uint32_t) floor (statfsbuf.f_bavail * scale);
		_total_free_4k_blocks += (*i).blocks;
	}
#endif
}

int
Session::ensure_sound_dir (string path, string& result)
{
	string dead;
	string peak;

	/* Ensure that the parent directory exists */
	
	if (g_mkdir_with_parents (path.c_str(), 0775)) {
		error << string_compose(_("cannot create session directory \"%1\"; ignored"), path) << endmsg;
		return -1;
	}
	
	/* Ensure that the sounds directory exists */
	
	result = path;
	result += '/';
	result += sound_dir_name;
	
	if (g_mkdir_with_parents (result.c_str(), 0775)) {
		error << string_compose(_("cannot create sounds directory \"%1\"; ignored"), result) << endmsg;
		return -1;
	}

	dead = path;
	dead += '/';
	dead += dead_sound_dir_name;
	
	if (g_mkdir_with_parents (dead.c_str(), 0775)) {
		error << string_compose(_("cannot create dead sounds directory \"%1\"; ignored"), dead) << endmsg;
		return -1;
	}

	peak = path;
	peak += '/';
	peak += peak_dir_name;
	
	if (g_mkdir_with_parents (peak.c_str(), 0775)) {
		error << string_compose(_("cannot create peak file directory \"%1\"; ignored"), peak) << endmsg;
		return -1;
	}
	
	/* callers expect this to be terminated ... */
			
	result += '/';
	return 0;
}	

string
Session::discover_best_sound_dir (bool destructive)
{
	vector<space_and_path>::iterator i;
	string result;

	/* handle common case without system calls */

	if (session_dirs.size() == 1) {
		return sound_dir();
	}

	/* OK, here's the algorithm we're following here:
	   
	We want to select which directory to use for 
	the next file source to be created. Ideally,
	we'd like to use a round-robin process so as to
	get maximum performance benefits from splitting
	the files across multiple disks.

	However, in situations without much diskspace, an
	RR approach may end up filling up a filesystem
	with new files while others still have space.
	Its therefore important to pay some attention to
	the freespace in the filesystem holding each
	directory as well. However, if we did that by
	itself, we'd keep creating new files in the file
	system with the most space until it was as full
	as all others, thus negating any performance
	benefits of this RAID-1 like approach.

	So, we use a user-configurable space threshold. If
	there are at least 2 filesystems with more than this
	much space available, we use RR selection between them. 
	If not, then we pick the filesystem with the most space.

	This gets a good balance between the two
	approaches.  
	*/
	
	refresh_disk_space ();
	
	int free_enough = 0;

	for (i = session_dirs.begin(); i != session_dirs.end(); ++i) {
		if ((*i).blocks * 4096 >= Config->get_disk_choice_space_threshold()) {
			free_enough++;
		}
	}

	if (free_enough >= 2) {

		bool found_it = false;

		/* use RR selection process, ensuring that the one
		   picked works OK.
		*/

		i = last_rr_session_dir;

		do {
			if (++i == session_dirs.end()) {
				i = session_dirs.begin();
			}

			if ((*i).blocks * 4096 >= Config->get_disk_choice_space_threshold()) {
				if (ensure_sound_dir ((*i).path, result) == 0) {
					last_rr_session_dir = i;
					found_it = true;
					break;
				}
			}

		} while (i != last_rr_session_dir);

		if (!found_it) {
			result = sound_dir();
		}

	} else {

		/* pick FS with the most freespace (and that
		   seems to actually work ...)
		*/
		
		vector<space_and_path> sorted;
		space_and_path_ascending_cmp cmp;

		sorted = session_dirs;
		sort (sorted.begin(), sorted.end(), cmp);
		
		for (i = sorted.begin(); i != sorted.end(); ++i) {
			if (ensure_sound_dir ((*i).path, result) == 0) {
				last_rr_session_dir = i;
				break;
			}
		}
		
		/* if the above fails, fall back to the most simplistic solution */
		
		if (i == sorted.end()) {
			return sound_dir();
		} 
	}

	return result;
}

int
Session::load_playlists (const XMLNode& node)
{
	XMLNodeList nlist;
	XMLNodeConstIterator niter;
	Playlist *playlist;

	nlist = node.children();

	set_dirty();

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		
		if ((playlist = XMLPlaylistFactory (**niter)) == 0) {
			error << _("Session: cannot create Playlist from XML description.") << endmsg;
		}
	}

	return 0;
}

int
Session::load_unused_playlists (const XMLNode& node)
{
	XMLNodeList nlist;
	XMLNodeConstIterator niter;
	Playlist *playlist;

	nlist = node.children();

	set_dirty();

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		
		if ((playlist = XMLPlaylistFactory (**niter)) == 0) {
			error << _("Session: cannot create Playlist from XML description.") << endmsg;
			continue;
		}

		// now manually untrack it

		track_playlist (playlist, false);
	}

	return 0;
}

Playlist *
Session::XMLPlaylistFactory (const XMLNode& node)
{
	const XMLProperty* type = node.property("type");

	try {
	
	if ( !type || type->value() == "audio" ) {

		return new AudioPlaylist (*this, node);
	
	} else if (type->value() == "midi") {

		return new MidiPlaylist (*this, node);
	
	}
	
	} catch (failed_constructor& err) {
		return 0;
	}

	return 0;
}

int
Session::load_named_selections (const XMLNode& node)
{
	XMLNodeList nlist;
	XMLNodeConstIterator niter;
	NamedSelection *ns;

	nlist = node.children();

	set_dirty();

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		
		if ((ns = XMLNamedSelectionFactory (**niter)) == 0) {
			error << _("Session: cannot create Named Selection from XML description.") << endmsg;
		}
	}

	return 0;
}

NamedSelection *
Session::XMLNamedSelectionFactory (const XMLNode& node)
{
	try {
		return new NamedSelection (*this, node);
	}

	catch (failed_constructor& err) {
		return 0;
	}
}

string
Session::dead_sound_dir () const
{
	string res = _path;
	res += dead_sound_dir_name;
	res += '/';
	return res;
}

string
Session::sound_dir (bool with_path) const
{
	/* support old session structure */

	struct stat statbuf;
	string old_nopath;
	string old_withpath;

	old_nopath += old_sound_dir_name;
	old_nopath += '/';
	
	old_withpath = _path;
	old_withpath += old_sound_dir_name;
	old_withpath += '/';

	if (stat (old_withpath.c_str(), &statbuf) == 0) {
		if (with_path)
			return old_withpath;
		
		return old_nopath;
	}

	string res;

	if (with_path) {
		res = _path;
	}

	res += interchange_dir_name;
	res += '/';
	res += legalize_for_path (_name);
	res += '/';
	res += sound_dir_name;
	res += '/';

	return res;
}

string
Session::peak_dir () const
{
	string res = _path;
	res += peak_dir_name;
	res += '/';
	return res;
}
	
string
Session::automation_dir () const
{
	string res = _path;
	res += "automation/";
	return res;
}

string
Session::template_dir ()
{
	string path = get_user_ardour_path();
	path += "templates/";

	return path;
}

string
Session::suffixed_search_path (string suffix, bool data)
{
	string path;

	path += get_user_ardour_path();
	if (path[path.length()-1] != ':') {
		path += ':';
	}

	if (data) {
		path += get_system_data_path();
	} else {
		path += get_system_module_path();
	}

	vector<string> split_path;
	
	split (path, split_path, ':');
	path = "";

	for (vector<string>::iterator i = split_path.begin(); i != split_path.end(); ++i) {
		path += *i;
		path += suffix;
		path += '/';
		
		if (distance (i, split_path.end()) != 1) {
			path += ':';
		}
	}
		
	return path;
}

string
Session::template_path ()
{
	return suffixed_search_path (X_("templates"), true);
}

string
Session::control_protocol_path ()
{
	return suffixed_search_path (X_("surfaces"), false);
}

int
Session::load_connections (const XMLNode& node)
{
	XMLNodeList nlist = node.children();
	XMLNodeConstIterator niter;

	set_dirty();

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() == "InputConnection") {
			add_connection (new ARDOUR::InputConnection (**niter));
		} else if ((*niter)->name() == "OutputConnection") {
			add_connection (new ARDOUR::OutputConnection (**niter));
		} else {
			error << string_compose(_("Unknown node \"%1\" found in Connections list from state file"), (*niter)->name()) << endmsg;
			return -1;
		}
	}

	return 0;
}				

int
Session::load_edit_groups (const XMLNode& node)
{
	return load_route_groups (node, true);
}

int
Session::load_mix_groups (const XMLNode& node)
{
	return load_route_groups (node, false);
}

int
Session::load_route_groups (const XMLNode& node, bool edit)
{
	XMLNodeList nlist = node.children();
	XMLNodeConstIterator niter;
	RouteGroup* rg;

	set_dirty();

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() == "RouteGroup") {
			if (edit) {
				rg = add_edit_group ("");
				rg->set_state (**niter);
			} else {
				rg = add_mix_group ("");
				rg->set_state (**niter);
			}
		}
	}
	
	return 0;
}				

static bool
state_file_filter (const string &str, void *arg)
{
	return (str.length() > strlen(Session::statefile_suffix()) &&
		str.find (Session::statefile_suffix()) == (str.length() - strlen (Session::statefile_suffix())));
}

struct string_cmp {
	bool operator()(const string* a, const string* b) {
		return *a < *b;
	}
};

static string*
remove_end(string* state)
{
	string statename(*state);
	
	string::size_type start,end;
	if ((start = statename.find_last_of ('/')) != string::npos) {
		statename = statename.substr (start+1);
	}
		
	if ((end = statename.rfind(".ardour")) == string::npos) {
		end = statename.length();
	}

	return new string(statename.substr (0, end));
}

vector<string *> *
Session::possible_states (string path) 
{
	PathScanner scanner;
	vector<string*>* states = scanner (path, state_file_filter, 0, false, false);
	
	transform(states->begin(), states->end(), states->begin(), remove_end);
	
	string_cmp cmp;
	sort (states->begin(), states->end(), cmp);
	
	return states;
}

vector<string *> *
Session::possible_states () const
{
	return possible_states(_path);
}

void
Session::auto_save()
{
	save_state (_current_snapshot_name);
}

RouteGroup *
Session::add_edit_group (string name)
{
	RouteGroup* rg = new RouteGroup (*this, name);
	edit_groups.push_back (rg);
	edit_group_added (rg); /* EMIT SIGNAL */
	set_dirty();
	return rg;
}

RouteGroup *
Session::add_mix_group (string name)
{
	RouteGroup* rg = new RouteGroup (*this, name, RouteGroup::Relative);
	mix_groups.push_back (rg);
	mix_group_added (rg); /* EMIT SIGNAL */
	set_dirty();
	return rg;
}

void
Session::remove_edit_group (RouteGroup& rg)
{
	list<RouteGroup*>::iterator i;

	if ((i = find (edit_groups.begin(), edit_groups.end(), &rg)) != edit_groups.end()) {
		(*i)->apply (&Route::drop_edit_group, this);
		edit_groups.erase (i);
		edit_group_removed (); /* EMIT SIGNAL */
	}

	delete &rg;
}

void
Session::remove_mix_group (RouteGroup& rg)
{
	list<RouteGroup*>::iterator i;

	if ((i = find (mix_groups.begin(), mix_groups.end(), &rg)) != mix_groups.end()) {
		(*i)->apply (&Route::drop_mix_group, this);
		mix_groups.erase (i);
		mix_group_removed (); /* EMIT SIGNAL */
	}

	delete &rg;
}

RouteGroup *
Session::mix_group_by_name (string name)
{
	list<RouteGroup *>::iterator i;

	for (i = mix_groups.begin(); i != mix_groups.end(); ++i) {
		if ((*i)->name() == name) {
			return* i;
		}
	}
	return 0;
}

RouteGroup *
Session::edit_group_by_name (string name)
{
	list<RouteGroup *>::iterator i;

	for (i = edit_groups.begin(); i != edit_groups.end(); ++i) {
		if ((*i)->name() == name) {
			return* i;
		}
	}
	return 0;
}

void
Session::begin_reversible_command (string name)
{
	current_trans = new UndoTransaction;
	current_trans->set_name (name);
}

void
Session::commit_reversible_command (Command *cmd)
{
	struct timeval now;

	if (cmd) {
		current_trans->add_command (cmd);
	}

	gettimeofday (&now, 0);
	current_trans->set_timestamp (now);

	history.add (current_trans);
}

Session::GlobalRouteBooleanState 
Session::get_global_route_boolean (bool (Route::*method)(void) const)
{
	GlobalRouteBooleanState s;
	boost::shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if (!(*i)->hidden()) {
			RouteBooleanState v;
			
			v.first =* i;
			Route* r = (*i).get();
			v.second = (r->*method)();
			
			s.push_back (v);
		}
	}

	return s;
}

Session::GlobalRouteMeterState
Session::get_global_route_metering ()
{
	GlobalRouteMeterState s;
	boost::shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if (!(*i)->hidden()) {
			RouteMeterState v;
			
			v.first =* i;
			v.second = (*i)->meter_point();
			
			s.push_back (v);
		}
	}

	return s;
}

void
Session::set_global_route_metering (GlobalRouteMeterState s, void* arg) 
{
	for (GlobalRouteMeterState::iterator i = s.begin(); i != s.end(); ++i) {
		i->first->set_meter_point (i->second, arg);
	}
}

void
Session::set_global_route_boolean (GlobalRouteBooleanState s, void (Route::*method)(bool, void*), void* arg)
{
	for (GlobalRouteBooleanState::iterator i = s.begin(); i != s.end(); ++i) {
		Route* r = i->first.get();
		(r->*method) (i->second, arg);
	}
}

void
Session::set_global_mute (GlobalRouteBooleanState s, void* src)
{
	set_global_route_boolean (s, &Route::set_mute, src);
}

void
Session::set_global_solo (GlobalRouteBooleanState s, void* src)
{
	set_global_route_boolean (s, &Route::set_solo, src);
}

void
Session::set_global_record_enable (GlobalRouteBooleanState s, void* src)
{
	set_global_route_boolean (s, &Route::set_record_enable, src);
}

#if 0
UndoAction
Session::global_mute_memento (void* src)
{
	return sigc::bind (mem_fun (*this, &Session::set_global_mute), get_global_route_boolean (&Route::muted), src);
}

UndoAction
Session::global_metering_memento (void* src)
{
	return sigc::bind (mem_fun (*this, &Session::set_global_route_metering), get_global_route_metering (), src);
}

UndoAction
Session::global_solo_memento (void* src)
{
	return sigc::bind (mem_fun (*this, &Session::set_global_solo), get_global_route_boolean (&Route::soloed), src);
}

UndoAction
Session::global_record_enable_memento (void* src)
{
	return sigc::bind (mem_fun (*this, &Session::set_global_record_enable), get_global_route_boolean (&Route::record_enabled), src);
}
#endif

static bool
template_filter (const string &str, void *arg)
{
	return (str.length() > strlen(Session::template_suffix()) &&
		str.find (Session::template_suffix()) == (str.length() - strlen (Session::template_suffix())));
}

void
Session::get_template_list (list<string> &template_names)
{
	vector<string *> *templates;
	PathScanner scanner;
	string path;

	path = template_path ();

	templates = scanner (path, template_filter, 0, false, true);
	
	vector<string*>::iterator i;
	for (i = templates->begin(); i != templates->end(); ++i) {
		string fullpath = *(*i);
		int start, end;

		start = fullpath.find_last_of ('/') + 1;
		if ((end = fullpath.find_last_of ('.')) <0) {
			end = fullpath.length();
		}
		
		template_names.push_back(fullpath.substr(start, (end-start)));
	}
}

int
Session::read_favorite_dirs (FavoriteDirs & favs)
{
	string path = get_user_ardour_path();
	path += "/favorite_dirs";

	ifstream fav (path.c_str());

	favs.clear();
	
	if (!fav) {
		if (errno != ENOENT) {
			//error << string_compose (_("cannot open favorite file %1 (%2)"), path, strerror (errno)) << endmsg;
			return -1;
		} else {
			return 1;
		}
	}

	while (true) {

	        string newfav;

		getline(fav, newfav);

		if (!fav.good()) {
			break;
		}

		favs.push_back (newfav);
	}

	return 0;
}

int
Session::write_favorite_dirs (FavoriteDirs & favs)
{
	string path = get_user_ardour_path();
	path += "/favorite_dirs";

	ofstream fav (path.c_str());

	if (!fav) {
		return -1;
	}

	for (FavoriteDirs::iterator i = favs.begin(); i != favs.end(); ++i) {
		fav << (*i) << endl;
	}
	
	return 0;
}

static bool
accept_all_non_peak_files (const string& path, void *arg)
{
	return (path.length() > 5 && path.find (".peak") != (path.length() - 5));
}

static bool
accept_all_state_files (const string& path, void *arg)
{
	return (path.length() > 7 && path.find (".ardour") == (path.length() - 7));
}

int 
Session::find_all_sources (string path, set<string>& result)
{
	XMLTree tree;
	XMLNode* node;

	if (!tree.read (path)) {
		return -1;
	}

	if ((node = find_named_node (*tree.root(), "Sources")) == 0) {
		return -2;
	}

	XMLNodeList nlist;
	XMLNodeConstIterator niter;

	nlist = node->children();

	set_dirty();

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		
		XMLProperty* prop;

		if ((prop = (*niter)->property (X_("name"))) == 0) {
			continue;
		}

		if (prop->value()[0] == '/') {
			/* external file, ignore */
			continue;
		}

		string path = _path; /* /-terminated */
		path += sound_dir_name;
		path += '/';
		path += prop->value();

		result.insert (path);
	}

	return 0;
}

int
Session::find_all_sources_across_snapshots (set<string>& result, bool exclude_this_snapshot)
{
	PathScanner scanner;
	vector<string*>* state_files;
	string ripped;
	string this_snapshot_path;

	result.clear ();

	ripped = _path;

	if (ripped[ripped.length()-1] == '/') {
		ripped = ripped.substr (0, ripped.length() - 1);
	}

	state_files = scanner (ripped, accept_all_state_files, (void *) 0, false, true);
	
	if (state_files == 0) {
		/* impossible! */
		return 0;
	}

	this_snapshot_path = _path;
	this_snapshot_path += _current_snapshot_name;
	this_snapshot_path += _statefile_suffix;

	for (vector<string*>::iterator i = state_files->begin(); i != state_files->end(); ++i) {

		if (exclude_this_snapshot && **i == this_snapshot_path) {
			continue;
		}

		if (find_all_sources (**i, result) < 0) {
			return -1;
		}
	}

	return 0;
}

int
Session::cleanup_sources (Session::cleanup_report& rep)
{
	vector<boost::shared_ptr<Source> > dead_sources;
	vector<Playlist*> playlists_tbd;
	PathScanner scanner;
	string sound_path;
	vector<space_and_path>::iterator i;
	vector<space_and_path>::iterator nexti;
	vector<string*>* soundfiles;
	vector<string> unused;
	set<string> all_sources;
	bool used;
	string spath;
	int ret = -1;
		
	_state_of_the_state = (StateOfTheState) (_state_of_the_state | InCleanup);
	
	/* step 1: consider deleting all unused playlists */

	for (PlaylistList::iterator x = unused_playlists.begin(); x != unused_playlists.end(); ++x) {
		int status;

		status = AskAboutPlaylistDeletion (*x);

		switch (status) {
		case -1:
			ret = 0;
			goto out;
			break;

		case 0:
			playlists_tbd.push_back (*x);
			break;

		default:
			/* leave it alone */
			break;
		}
	}

	/* now delete any that were marked for deletion */

	for (vector<Playlist*>::iterator x = playlists_tbd.begin(); x != playlists_tbd.end(); ++x) {
		PlaylistList::iterator foo;

		if ((foo = unused_playlists.find (*x)) != unused_playlists.end()) {
			unused_playlists.erase (foo);
		}
		delete *x;
	}

	/* step 2: find all un-referenced sources */

	rep.paths.clear ();
	rep.space = 0;

	for (SourceMap::iterator i = sources.begin(); i != sources.end(); ) {

		SourceMap::iterator tmp;

		tmp = i;
		++tmp;

		/* only remove files that are not in use and have some size
		   to them. otherwise we remove the current "nascent"
		   capture files.
		*/

		if (i->second.use_count() == 1 && i->second->length() > 0) {
			dead_sources.push_back (i->second);

			/* remove this source from our own list to avoid us
			   adding it to the list of all sources below
			*/

			sources.erase (i);
		}

		i = tmp;
	}

	/* Step 3: get rid of all regions in the region list that use any dead sources
	   in case the sources themselves don't go away (they might be referenced in
	   other snapshots).
	*/
		
	for (vector<boost::shared_ptr<Source> >::iterator i = dead_sources.begin(); i != dead_sources.end();++i) {

		for (RegionList::iterator r = regions.begin(); r != regions.end(); ) {
			RegionList::iterator tmp;

			tmp = r;
			++tmp;
			
			boost::shared_ptr<Region> reg = r->second;

			for (uint32_t n = 0; n < reg->n_channels(); ++n) {
				if (reg->source (n) == (*i)) {
					/* this region is dead */
					remove_region (reg);
				}
			}
			
			r = tmp;
		}
	}

	/* build a list of all the possible sound directories for the session */

	for (i = session_dirs.begin(); i != session_dirs.end(); ) {

		nexti = i;
		++nexti;

		sound_path += (*i).path;
		sound_path += sound_dir_name;

		if (nexti != session_dirs.end()) {
			sound_path += ':';
		}

		i = nexti;
	}
	
	/* now do the same thing for the files that ended up in the sounds dir(s) 
	   but are not referenced as sources in any snapshot.
	*/

	soundfiles = scanner (sound_path, accept_all_non_peak_files, (void *) 0, false, true);

	if (soundfiles == 0) {
		return 0;
	}

	/* find all sources, but don't use this snapshot because the
	   state file on disk still references sources we may have already
	   dropped.
	*/
	
	find_all_sources_across_snapshots (all_sources, true);

	/*  add our current source list
	 */
	
	for (SourceMap::iterator i = sources.begin(); i != sources.end(); ++i) {
		boost::shared_ptr<AudioFileSource> fs;
		
		if ((fs = boost::dynamic_pointer_cast<AudioFileSource> (i->second)) != 0) {
			all_sources.insert (fs->path());
		} 
	}

	for (vector<string*>::iterator x = soundfiles->begin(); x != soundfiles->end(); ++x) {

		used = false;
		spath = **x;

		for (set<string>::iterator i = all_sources.begin(); i != all_sources.end(); ++i) {

			if (spath == *i) {
				used = true;
				break;
			}

		}

		if (!used) {
			unused.push_back (spath);
		}
	}

	/* now try to move all unused files into the "dead_sounds" directory(ies) */

	for (vector<string>::iterator x = unused.begin(); x != unused.end(); ++x) {
		struct stat statbuf;

		rep.paths.push_back (*x);
		if (stat ((*x).c_str(), &statbuf) == 0) {
			rep.space += statbuf.st_size;
		}

		string newpath;
		
		/* don't move the file across filesystems, just
		   stick it in the `dead_sound_dir_name' directory
		   on whichever filesystem it was already on.
		*/

		newpath = Glib::path_get_dirname (*x);
		newpath = Glib::path_get_dirname (newpath);

		newpath += '/';
		newpath += dead_sound_dir_name;
		newpath += '/';
		newpath += Glib::path_get_basename ((*x));
		
		if (access (newpath.c_str(), F_OK) == 0) {
			
			/* the new path already exists, try versioning */
			
			char buf[PATH_MAX+1];
			int version = 1;
			string newpath_v;
			
			snprintf (buf, sizeof (buf), "%s.%d", newpath.c_str(), version);
			newpath_v = buf;

			while (access (newpath_v.c_str(), F_OK) == 0 && version < 999) {
				snprintf (buf, sizeof (buf), "%s.%d", newpath.c_str(), ++version);
				newpath_v = buf;
			}
			
			if (version == 999) {
				error << string_compose (_("there are already 1000 files with names like %1; versioning discontinued"),
						  newpath)
				      << endmsg;
			} else {
				newpath = newpath_v;
			}
			
		} else {
			
			/* it doesn't exist, or we can't read it or something */
			
		}

		if (::rename ((*x).c_str(), newpath.c_str()) != 0) {
			error << string_compose (_("cannot rename audio file source from %1 to %2 (%3)"),
					  (*x), newpath, strerror (errno))
			      << endmsg;
			goto out;
		}
		

		/* see if there an easy to find peakfile for this file, and remove it.
		 */

		string peakpath = (*x).substr (0, (*x).find_last_of ('.'));
		peakpath += ".peak";

		if (access (peakpath.c_str(), W_OK) == 0) {
			if (::unlink (peakpath.c_str()) != 0) {
				error << string_compose (_("cannot remove peakfile %1 for %2 (%3)"),
						  peakpath, _path, strerror (errno))
				      << endmsg;
				/* try to back out */
				rename (newpath.c_str(), _path.c_str());
				goto out;
			}
		}

	}

	ret = 0;

	/* dump the history list */

	history.clear ();

	/* save state so we don't end up a session file
	   referring to non-existent sources.
	*/
	
	save_state ("");

  out:
	_state_of_the_state = (StateOfTheState) (_state_of_the_state & ~InCleanup);
	return ret;
}

int
Session::cleanup_trash_sources (Session::cleanup_report& rep)
{
	vector<space_and_path>::iterator i;
	string dead_sound_dir;
	struct dirent* dentry;
	struct stat statbuf;
	DIR* dead;

	rep.paths.clear ();
	rep.space = 0;

	for (i = session_dirs.begin(); i != session_dirs.end(); ++i) {
		
		dead_sound_dir = (*i).path;
		dead_sound_dir += dead_sound_dir_name;

		if ((dead = opendir (dead_sound_dir.c_str())) == 0) {
			continue;
		}

		while ((dentry = readdir (dead)) != 0) {

			/* avoid '.' and '..' */
			
			if ((dentry->d_name[0] == '.' && dentry->d_name[1] == '\0') || 
			    (dentry->d_name[2] == '\0' && dentry->d_name[0] == '.' && dentry->d_name[1] == '.')) {
				continue;
			}

			string fullpath;

			fullpath = dead_sound_dir;
			fullpath += '/';
			fullpath += dentry->d_name;

			if (stat (fullpath.c_str(), &statbuf)) {
				continue;
			}

			if (!S_ISREG (statbuf.st_mode)) {
				continue;
			}

			if (unlink (fullpath.c_str())) {
				error << string_compose (_("cannot remove dead sound file %1 (%2)"),
						  fullpath, strerror (errno))
				      << endmsg;
			}

			rep.paths.push_back (dentry->d_name);
			rep.space += statbuf.st_size;
		}

		closedir (dead);
		
	}

	return 0;
}

void
Session::set_dirty ()
{
	bool was_dirty = dirty();

	_state_of_the_state = StateOfTheState (_state_of_the_state | Dirty);

	if (!was_dirty) {
		DirtyChanged(); /* EMIT SIGNAL */
	}
}


void
Session::set_clean ()
{
	bool was_dirty = dirty();
	
	_state_of_the_state = Clean;

	if (was_dirty) {
		DirtyChanged(); /* EMIT SIGNAL */
	}
}

void
Session::add_controllable (Controllable* c)
{
	Glib::Mutex::Lock lm (controllables_lock);
	controllables.insert (c);
}

void
Session::remove_controllable (Controllable* c)
{
	if (_state_of_the_state | Deletion) {
		return;
	}

	Glib::Mutex::Lock lm (controllables_lock);

	Controllables::iterator x = controllables.find (c);

	if (x != controllables.end()) {
		controllables.erase (x);
	}
}	

Controllable*
Session::controllable_by_id (const PBD::ID& id)
{
	Glib::Mutex::Lock lm (controllables_lock);
	
	for (Controllables::iterator i = controllables.begin(); i != controllables.end(); ++i) {
		if ((*i)->id() == id) {
			return *i;
		}
	}

	return 0;
}

void 
Session::add_instant_xml (XMLNode& node, const std::string& dir)
{
	Stateful::add_instant_xml (node, dir);
	Config->add_instant_xml (node, get_user_ardour_path());
}


int 
Session::save_history (string snapshot_name)
{
    XMLTree tree;
    string xml_path;
    string bak_path;
    
    tree.set_root (&history.get_state());

    if (snapshot_name.empty()) {
	snapshot_name = _current_snapshot_name;
    }

    xml_path = _path + snapshot_name + ".history"; 

    bak_path = xml_path + ".bak";

    if ((access (xml_path.c_str(), F_OK) == 0) &&
        (rename (xml_path.c_str(), bak_path.c_str())))
    {
        error << _("could not backup old history file, current history not saved.") << endmsg;
        return -1;
    }

    cerr << "actually writing history\n";

    if (!tree.write (xml_path))
    {
        error << string_compose (_("history could not be saved to %1"), xml_path) << endmsg;

        /* don't leave a corrupt file lying around if it is
         * possible to fix.
         */

        if (unlink (xml_path.c_str())) 
        {
            error << string_compose (_("could not remove corrupt history file %1"), xml_path) << endmsg;
        } else {
            if (rename (bak_path.c_str(), xml_path.c_str())) 
            {
                error << string_compose (_("could not restore history file from backup %1"), bak_path) << endmsg;
            }
        }

        return -1;
    }

    return 0;
}

int
Session::restore_history (string snapshot_name)
{
    XMLTree tree;
    string xmlpath;

    /* read xml */
    xmlpath = _path + snapshot_name + ".history";
    cerr << string_compose(_("Loading history from '%1'."), xmlpath) << endmsg;

    if (access (xmlpath.c_str(), F_OK)) {
        error << string_compose(_("%1: session history file \"%2\" doesn't exist!"), _name, xmlpath) << endmsg;
        return 1;
    }

    if (!tree.read (xmlpath)) {
        error << string_compose(_("Could not understand ardour file %1"), xmlpath) << endmsg;
        return -1;
    }

    /* replace history */
    history.clear();

    for (XMLNodeConstIterator it  = tree.root()->children().begin(); it != tree.root()->children().end(); it++) {
	    
	    XMLNode *t = *it;
	    UndoTransaction* ut = new UndoTransaction ();
	    struct timeval tv;
	    
	    ut->set_name(t->property("name")->value());
	    stringstream ss(t->property("tv_sec")->value());
	    ss >> tv.tv_sec;
	    ss.str(t->property("tv_usec")->value());
	    ss >> tv.tv_usec;
	    ut->set_timestamp(tv);
	    
	    for (XMLNodeConstIterator child_it  = t->children().begin();
		 child_it != t->children().end();
		 child_it++)
	    {
		    XMLNode *n = *child_it;
		    Command *c;
	
		    if (n->name() == "MementoCommand" ||
			n->name() == "MementoUndoCommand" ||
			n->name() == "MementoRedoCommand") {
			    if ((c = memento_command_factory(n))) {
				    ut->add_command(c);
			    }
		    } else {
			    error << string_compose(_("Couldn't figure out how to make a Command out of a %1 XMLNode."), n->name()) << endmsg;
		    }
	    }

	    history.add (ut);
    }

    return 0;
}

void
Session::config_changed (const char* parameter_name)
{
#define PARAM_IS(x) (!strcmp (parameter_name, (x)))

	if (PARAM_IS ("seamless-loop")) {
		
	} else if (PARAM_IS ("rf-speed")) {
		
	} else if (PARAM_IS ("auto-loop")) {
		
	} else if (PARAM_IS ("auto-input")) {

		if (Config->get_monitoring_model() == HardwareMonitoring && transport_rolling()) {
			/* auto-input only makes a difference if we're rolling */
			
			boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();
			
			for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
				if ((*i)->record_enabled ()) {
					//cerr << "switching to input = " << !auto_input << __FILE__ << __LINE__ << endl << endl;
					(*i)->monitor_input (!Config->get_auto_input());
				}
			}
		}

	} else if (PARAM_IS ("punch-in")) {

		Location* location;
		
		if ((location = _locations.auto_punch_location()) != 0) {
			
			if (Config->get_punch_in ()) {
				replace_event (Event::PunchIn, location->start());
			} else {
				remove_event (location->start(), Event::PunchIn);
			}
		}
		
	} else if (PARAM_IS ("punch-out")) {

		Location* location;
		
		if ((location = _locations.auto_punch_location()) != 0) {
			
			if (Config->get_punch_out()) {
				replace_event (Event::PunchOut, location->end());
			} else {
				clear_events (Event::PunchOut);
			}
		}

	} else if (PARAM_IS ("edit-mode")) {

		Glib::Mutex::Lock lm (playlist_lock);
		
		for (PlaylistList::iterator i = playlists.begin(); i != playlists.end(); ++i) {
			(*i)->set_edit_mode (Config->get_edit_mode ());
		}

	} else if (PARAM_IS ("use-video-sync")) {

		if (transport_stopped()) {
			if (Config->get_use_video_sync()) {
				waiting_for_sync_offset = true;
			}
		}

	} else if (PARAM_IS ("mmc-control")) {

		//poke_midi_thread ();

	} else if (PARAM_IS ("midi-control")) {
		
		//poke_midi_thread ();

	} else if (PARAM_IS ("raid-path")) {

		setup_raid_path (Config->get_raid_path());

	} else if (PARAM_IS ("smpte-frames-per-second") || PARAM_IS ("smpte-drop-frames")) {

		sync_time_vars ();

	} else if (PARAM_IS ("video-pullup")) {

		sync_time_vars ();

	} else if (PARAM_IS ("seamless-loop")) {

		if (play_loop && transport_rolling()) {
			// to reset diskstreams etc
			request_play_loop (true);
		}

	} else if (PARAM_IS ("rf-speed")) {

		cumulative_rf_motion = 0;
		reset_rf_scale (0);

	} else if (PARAM_IS ("click-sound")) {

		setup_click_sounds (1);

	} else if (PARAM_IS ("click-emphasis-sound")) {

		setup_click_sounds (-1);

	} else if (PARAM_IS ("clicking")) {

		if (Config->get_clicking()) {
			if (_click_io && click_data) { // don't require emphasis data
				_clicking = true;
			}
		} else {
			_clicking = false;
		}

	} else if (PARAM_IS ("send-mtc")) {
		
		/* only set the internal flag if we have
		   a port.
		*/
		
		if (_mtc_port != 0) {
			session_send_mtc = Config->get_send_mtc();
		}

	} else if (PARAM_IS ("send-mmc")) {
		
		/* only set the internal flag if we have
		   a port.
		*/
		
		if (_mmc_port != 0) {
			session_send_mmc = Config->get_send_mmc();
		}

	} else if (PARAM_IS ("midi-feedback")) {
		
		/* only set the internal flag if we have
		   a port.
		*/
		
		if (_mtc_port != 0) {
			session_midi_feedback = Config->get_midi_feedback();
		}

	} else if (PARAM_IS ("jack-time-master")) {

		engine().reset_timebase ();

	} else if (PARAM_IS ("native-file-header-format")) {

		if (!first_file_header_format_reset) {
			reset_native_file_format ();
		}

		first_file_header_format_reset = false;

	} else if (PARAM_IS ("native-file-data-format")) {

		if (!first_file_data_format_reset) {
			reset_native_file_format ();
		}

		first_file_data_format_reset = false;
	}
		
	set_dirty ();
		   
#undef PARAM_IS

}
