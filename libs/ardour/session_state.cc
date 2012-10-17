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
    
*/

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
#include <sys/param.h>
#include <sys/mount.h>
#endif

#include <glibmm.h>
#include <glibmm/thread.h>

#include <midi++/mmc.h>
#include <midi++/port.h>

#include <pbd/error.h>
#include <pbd/pathscanner.h>
#include <pbd/pthread_utils.h>
#include <pbd/strsplit.h>
#include <pbd/stacktrace.h>
#include <pbd/copyfile.h>
#include <pbd/localeguard.h>

#include <ardour/audioengine.h>
#include <ardour/configuration.h>
#include <ardour/session.h>
#include <ardour/audio_diskstream.h>
#include <ardour/utils.h>
#include <ardour/audioplaylist.h>
#include <ardour/audiofilesource.h>
#include <ardour/silentfilesource.h>
#include <ardour/sndfilesource.h>
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
#include <ardour/cycle_timer.h>
#include <ardour/utils.h>
#include <ardour/named_selection.h>
#include <ardour/version.h>
#include <ardour/location.h>
#include <ardour/audioregion.h>
#include <ardour/crossfade.h>
#include <ardour/control_protocol_manager.h>
#include <ardour/region_factory.h>
#include <ardour/source_factory.h>
#include <ardour/playlist_factory.h>

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
		destroy ();
		throw failed_constructor();
	}

	char buf[PATH_MAX+1];
	if (!realpath (fullpath.c_str(), buf) && (errno != ENOENT)) {
		error << string_compose(_("Could not use path %1 (%s)"), buf, strerror(errno)) << endmsg;
		destroy ();
		throw failed_constructor();
	}

	_path = string(buf);

	if (_path[_path.length()-1] != G_DIR_SEPARATOR) {
		_path += G_DIR_SEPARATOR;
	}

	if (Glib::file_test (_path, Glib::FILE_TEST_EXISTS) && ::access (_path.c_str(), W_OK)) {
		cerr << "Session non-writable based on " << _path << endl;
		_writable = false;
	} else {
		cerr << "Session writable based on " << _path << endl;
		_writable = true;	
	}

	set_history_depth (Config->get_history_depth());
	

	/* these two are just provisional settings. set_state()
	   will likely override them.
	*/

	_name = _current_snapshot_name = snapshot_name;

	_current_frame_rate = _engine.frame_rate ();
	_nominal_frame_rate = _current_frame_rate;
	_base_frame_rate = _current_frame_rate;

	_tempo_map = new TempoMap (_current_frame_rate);
	_tempo_map->StateChanged.connect (mem_fun (*this, &Session::tempo_map_changed));

	g_atomic_int_set (&processing_prohibited, 0);
	post_transport_work = PostTransportWork (0);
	insert_cnt = 0;
	_transport_speed = 0;
	_last_transport_speed = 0;
	auto_play_legal = false;
	transport_sub_state = 0;
	_transport_frame = 0;
	last_stop_frame = 0;
	_requested_return_frame = -1;
	end_location = new Location (0, 0, _("end"), Location::Flags ((Location::IsMark|Location::IsEnd)));
	start_location = new Location (0, 0, _("start"), Location::Flags ((Location::IsMark|Location::IsStart)));
	_end_location_is_free = true;
	g_atomic_int_set (&_record_status, Disabled);
	loop_changing = false;
	play_loop = false;
	have_looped = false;
	_last_roll_location = 0;
	_last_roll_or_reversal_location = 0;
	_last_record_location = 0;
	pending_locate_frame = 0;
	pending_locate_roll = false;
	pending_locate_flush = false;
	dstream_buffer_size = 0;
	state_was_pending = false;
	set_next_event ();
	outbound_mtc_smpte_frame = 0;
	next_quarter_frame_to_send = -1;
	current_block_size = 0;
	solo_update_disabled = false;
	currently_soloing = false;
	_was_seamless = Config->get_seamless_loop ();
	_have_captured = false;
	_worst_output_latency = 0;
	_worst_input_latency = 0;
	_worst_track_latency = 0;
	_state_of_the_state = StateOfTheState(CannotSave|InitialConnecting|Loading);
	
	_slave = 0;
	_silent = false;
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
	_exporting = false;
	_gain_automation_buffer = 0;
	_pan_automation_buffer = 0;
	_npan_buffers = 0;
	pending_abort = false;
	pending_clear_substate = false;
	destructive_index = 0;
	current_trans = 0;
	first_file_data_format_reset = true;
	first_file_header_format_reset = true;
	butler_thread = (pthread_t) 0;
        _clicks_cleared = 0;

	AudioDiskstream::allocate_working_buffers();
	
	/* default short fade = 15ms */

	Crossfade::set_short_xfade_length ((nframes_t) floor (Config->get_short_xfade_seconds() * frame_rate()));
	SndFileSource::setup_standard_crossfades (frame_rate());

	last_mmc_step.tv_sec = 0;
	last_mmc_step.tv_usec = 0;
	step_speed = 0.0;

	/* click sounds are unset by default, which causes us to internal
	   waveforms for clicks.
	*/
	
	click_length = 0;
	click_emphasis_length = 0;
	_clicking = false;

	process_function = &Session::process_with_events;

	if (Config->get_use_video_sync()) {
		waiting_for_sync_offset = true;
	} else {
		waiting_for_sync_offset = false;
	}

	last_smpte_when = 0;
	_smpte_offset = 0;
	_smpte_offset_negative = true;
	last_smpte_valid = false;

	sync_time_vars ();

	last_rr_session_dir = session_dirs.begin();
	refresh_disk_space ();

	// set_default_fade (0.2, 5.0); /* steepness, millisecs */

	/* slave stuff */

	average_slave_delta = 1800; // !!! why 1800 ????
	have_first_delta_accumulator = false;
	delta_accumulator_cnt = 0;
	slave_state = Stopped;

	_engine.GraphReordered.connect (mem_fun (*this, &Session::graph_reordered));

	/* These are all static "per-class" signals */

	RegionFactory::CheckNewRegion.connect (mem_fun (*this, &Session::add_region));
	SourceFactory::SourceCreated.connect (mem_fun (*this, &Session::add_source));
	PlaylistFactory::PlaylistCreated.connect (mem_fun (*this, &Session::add_playlist));
	Redirect::RedirectCreated.connect (mem_fun (*this, &Session::add_redirect));
	NamedSelection::NamedSelectionCreated.connect (mem_fun (*this, &Session::add_named_selection));
        AutomationList::AutomationListCreated.connect (mem_fun (*this, &Session::add_automation_list));

	Controllable::Destroyed.connect (mem_fun (*this, &Session::remove_controllable));

	IO::MoreOutputs.connect (mem_fun (*this, &Session::ensure_passthru_buffers));

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

	if (start_midi_thread ()) {
		return -1;
	}

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


	_locations.changed.connect (mem_fun (this, &Session::locations_changed));
	_locations.added.connect (mem_fun (this, &Session::locations_added));
	setup_click_sounds (0);
	setup_midi_control ();

	/* Pay attention ... */

	_engine.Halted.connect (mem_fun (*this, &Session::engine_halted));
	_engine.Xrun.connect (mem_fun (*this, &Session::xrun_recovery));

	try {
		when_engine_running();
	}

	/* handle this one in a different way than all others, so that its clear what happened */

	catch (AudioEngine::PortRegistrationFailure& err) {
		destroy ();
		throw;
	}

	catch (...) {
		return -1;
	}

	BootMessage (_("Reset Remote Controls"));

	send_full_time_code ();
	_engine.transport_locate (0);
	deliver_mmc (MIDI::MachineControl::cmdMmcReset, 0);
	deliver_mmc (MIDI::MachineControl::cmdLocate, 0);

	/* initial program change will be delivered later; see ::config_changed() */

	if (new_session) {
		_end_location_is_free = true;
	} else {
		_end_location_is_free = false;
	}

	_state_of_the_state = Clean;
	
	DirtyChanged (); /* EMIT SIGNAL */

	if (state_was_pending) {
		save_state (_current_snapshot_name);
		remove_pending_capture_state ();
		state_was_pending = false;
	}
	
	BootMessage (_("Session loading complete"));

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
		/* sounds dir */

		AudioFileSource::set_search_path (Glib::build_filename(sp.path, sound_dir (false)));
		return;
	}

	remaining = path;

	while ((colon = remaining.find_first_of (':')) != string::npos) {
		
		sp.blocks = 0;
		sp.path = remaining.substr (0, colon);
		session_dirs.push_back (sp);

		/* add sounds to file search path */

		fspath += Glib::build_filename(sp.path, sound_dir (false));
		fspath += ':';

		remaining = remaining.substr (colon+1);
	}

	if (remaining.length()) {

		sp.blocks = 0;
		sp.path = remaining;

		fspath += ':';
		fspath += Glib::build_filename(sp.path, sound_dir (false));
		fspath += ':';

		session_dirs.push_back (sp);
	}

	/* set the AudioFileSource search path */

	AudioFileSource::set_search_path (fspath);

	/* reset the round-robin soundfile path thingie */

	last_rr_session_dir = session_dirs.begin();
}

int
Session::ensure_subdirs ()
{
	string dir;

	dir = peak_dir ();

	if (g_mkdir_with_parents (dir.c_str(), 0755) < 0) {
		error << string_compose(_("Session: cannot create session peakfile folder \"%1\" (%2)"), dir, strerror (errno)) << endmsg;
		return -1;
	}

	/* if this is is an existing session with an old "sounds" directory, just use it. see Session::sound_dir() for more details */

	if (!Glib::file_test (old_sound_dir(), Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_DIR)) {

		dir = sound_dir ();
		
		if (g_mkdir_with_parents (dir.c_str(), 0755) < 0) {
			error << string_compose(_("Session: cannot create session sounds folder \"%1\" (%2)"), dir, strerror (errno)) << endmsg;
			return -1;
		}
	}

	dir = dead_sound_dir ();

	if (g_mkdir_with_parents (dir.c_str(), 0755) < 0) {
		error << string_compose(_("Session: cannot create session dead sounds folder \"%1\" (%2)"), dir, strerror (errno)) << endmsg;
		return -1;
	}

	dir = export_dir ();

	if (g_mkdir_with_parents (dir.c_str(), 0755) < 0) {
		error << string_compose(_("Session: cannot create session export folder \"%1\" (%2)"), dir, strerror (errno)) << endmsg;
		return -1;
	}

	dir = analysis_dir ();

	if (g_mkdir_with_parents (dir.c_str(), 0755) < 0) {
		error << string_compose(_("Session: cannot create session analysis folder \"%1\" (%2)"), dir, strerror (errno)) << endmsg;
		return -1;
	}

	return 0;
}

int
Session::create (bool& new_session, const string& mix_template, nframes_t initial_length)
{

	if (g_mkdir_with_parents (_path.c_str(), 0755) < 0) {
		error << string_compose(_("Session: cannot create session folder \"%1\" (%2)"), _path, strerror (errno)) << endmsg;
		return -1;
	}

	if (ensure_subdirs ()) {
		return -1;
	}

	/* check new_session so we don't overwrite an existing one */

	if (!mix_template.empty()) {
		std::string in_path = mix_template;

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

	save_state ("");

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
			boost::shared_ptr<AudioDiskstream> dstream (new AudioDiskstream (*this, **citer));
			add_diskstream (dstream);
		} 
		
		catch (failed_constructor& err) {
			error << _("Session: could not load diskstream via XML state")			      << endmsg;
			return -1;
		}
	}

	return 0;
}

void
Session::maybe_write_autosave()
{
        if (dirty() && record_status() != Recording) {
                save_state("", true);
        }
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

/** Rename a state file.
 * @param snapshot_name Snapshot name.
 */
void
Session::rename_state (string old_name, string new_name)
{
	if (old_name == _current_snapshot_name || old_name == _name) {
		/* refuse to rename the current snapshot or the "main" one */
		return;
	}
	
	const string old_xml_path = _path + old_name + _statefile_suffix;
	const string new_xml_path = _path + new_name + _statefile_suffix;

	if (rename (old_xml_path.c_str(), new_xml_path.c_str()) != 0) {
		error << string_compose(_("could not rename snapshot %1 to %2"), old_name, new_name) << endmsg;
	}
}

/** Remove a state file.
 * @param snapshot_name Snapshot name.
 */
void
Session::remove_state (string snapshot_name)
{
	if (snapshot_name == _current_snapshot_name || snapshot_name == _name) {
		/* refuse to remove the current snapshot or the "main" one */
		return;
	}
	
	const string xml_path = _path + snapshot_name + _statefile_suffix;

	/* make a backup copy of the state file */
	const string bak_path = xml_path + ".bak";
	if (g_file_test (xml_path.c_str(), G_FILE_TEST_EXISTS)) {
		copy_file (xml_path, bak_path);
	}

	/* and delete it */
	unlink (xml_path.c_str());
}

int
Session::save_state (string snapshot_name, bool pending, bool switch_to_snapshot)
{
	XMLTree tree;
	string xml_path;
	string bak_path;

	if (!_writable || (_state_of_the_state & CannotSave)) {
		return 1;
	}

	if (!_engine.connected ()) {
		error << string_compose (_("The %1 audio engine is not connected and state saving would lose all I/O connections. Session not saved"),
					 PROGRAM_NAME)
		      << endmsg;
		return 1;
	}

	tree.set_root (&get_state());

	if (snapshot_name.empty()) {
		snapshot_name = _current_snapshot_name;
	} else if (switch_to_snapshot) {
		_current_snapshot_name = snapshot_name;
	}

	if (!pending) {

		/* proper save: use _statefile_suffix (.ardour in English) */
		xml_path = _path;
		xml_path += snapshot_name;
		xml_path += _statefile_suffix;

		/* make a backup copy of the old file */
		bak_path = xml_path;
		bak_path += ".bak";
		
		if (g_file_test (xml_path.c_str(), G_FILE_TEST_EXISTS)) {
			copy_file (xml_path, bak_path);
		}

	} else {

		/* pending save: use _pending_suffix (.pending in English) */
		xml_path = _path;
		xml_path += snapshot_name;
		xml_path += _pending_suffix;

	}

	string tmp_path;

	tmp_path = _path;
	tmp_path += snapshot_name;
	tmp_path += ".tmp";

	// cerr << "actually writing state to " << xml_path << endl;

	if (!tree.write (tmp_path)) {
		error << string_compose (_("state could not be saved to %1"), tmp_path) << endmsg;
		unlink (tmp_path.c_str());
		return -1;

	} else {

		if (rename (tmp_path.c_str(), xml_path.c_str()) != 0) {
			error << string_compose (_("could not rename temporary session file %1 to %2"), tmp_path, xml_path) << endmsg;
			unlink (tmp_path.c_str());
			return -1;
		}
	}

	if (!pending) {

                save_history (snapshot_name);

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

	if (Glib::file_test (xmlpath, Glib::FILE_TEST_EXISTS)) {

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
	
	if (!Glib::file_test (xmlpath, Glib::FILE_TEST_EXISTS)) {
		error << string_compose(_("%1: session state information file \"%2\" doesn't exist!"), _name, xmlpath) << endmsg;
		return 1;
	}

	state_tree = new XMLTree;

	set_dirty();

	/* writable() really reflects the whole folder, but if for any
	   reason the session state file can't be written to, still
	   make us unwritable.
	*/

	if (::access (xmlpath.c_str(), W_OK) != 0) {
		_writable = false;
	}

	if (!state_tree->read (xmlpath)) {
		error << string_compose(_("Could not understand ardour file %1"), xmlpath) << endmsg;
		delete state_tree;
		state_tree = 0;
		return -1;
	}

	XMLNode& root (*state_tree->root());
	
	if (root.name() != X_("Session")) {
		error << string_compose (_("Session file %1 is not a session"), xmlpath) << endmsg;
		delete state_tree;
		state_tree = 0;
		return -1;
	}

	const XMLProperty* prop;
	bool is_old = false;

	if ((prop = root.property ("version")) == 0) {
		/* no version implies very old version of Ardour */
		is_old = true;
	} else {
		int major_version;
		major_version = atoi (prop->value()); // grab just the first number before the period
		if (major_version < 2) {
			is_old = true;
		}
	}

	if (is_old) {
		string backup_path;

		backup_path = _path;
		backup_path += snapshot_name;
		backup_path += "-1";
		backup_path += _statefile_suffix;

		/* don't make another copy if it already exists */

		if (!Glib::file_test (backup_path, Glib::FILE_TEST_EXISTS)) {
			info << string_compose (_("Copying old session file %1 to %2\nUse %2 with %3 versions before 2.0 from now on"),
						xmlpath, backup_path, PROGRAM_NAME) 
			     << endmsg;
			
			copy_file (xmlpath, backup_path);
			
			/* if it fails, don't worry. right? */
		}
	}

	return 0;
}

int
Session::load_options (const XMLNode& node)
{
	XMLNode* child;
	XMLProperty* prop;
	LocaleGuard lg (X_("POSIX"));

	Config->set_variables (node, ConfigVariableBase::Session);

	/* now reset MIDI ports because the session can have its own 
	   MIDI configuration.
	*/

	setup_midi ();

	if ((child = find_named_node (node, "end-marker-is-free")) != 0) {
		if ((prop = child->property ("val")) != 0) {
			_end_location_is_free = string_is_affirmative (prop->value());
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
	snprintf(buf, sizeof(buf), "%d.%d.%d", libardour2_major_version, libardour2_minor_version, libardour2_micro_version);
	node->add_property("version", string(buf));
		
	/* store configuration settings */

	if (full_state) {
	
		node->add_property ("name", _name);
		snprintf (buf, sizeof (buf), "%" PRId32, _nominal_frame_rate);
		node->add_property ("sample-rate", buf);

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
		Glib::Mutex::Lock sl (audio_source_lock);

		for (AudioSourceList::iterator siter = audio_sources.begin(); siter != audio_sources.end(); ++siter) {
			
			/* Don't save information about AudioFileSources that are empty */
			
			boost::shared_ptr<AudioFileSource> fs;

			if ((fs = boost::dynamic_pointer_cast<AudioFileSource> (siter->second)) != 0) {

				/* destructive file sources are OK if they are empty, because
				   we will re-use them every time.
				*/

				if (!fs->destructive()) {
					if (fs->length() == 0) {
						continue;
					}
				}
			}
			
			child->add_child_nocopy (siter->second->get_state());
		}
	}

	child = node->add_child ("Regions");

	if (full_state) { 
		Glib::Mutex::Lock rl (region_lock);

		for (AudioRegionList::const_iterator i = audio_regions.begin(); i != audio_regions.end(); ++i) {
			
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

	if (full_state) {
		node->add_child_nocopy (_locations.get_state());
	} else {
		// for a template, just create a new Locations, populate it
		// with the default start and end, and get the state for that.
		Locations loc;
		Location* start = new Location(0, 0, _("start"), Location::Flags ((Location::IsMark|Location::IsStart)));
		Location* end = new Location(0, 0, _("end"), Location::Flags ((Location::IsMark|Location::IsEnd)));
		start->set_end(0);
		loc.add (start);
		end->set_end(compute_initial_length());
		loc.add (end);
		node->add_child_nocopy (loc.get_state());
	}
	
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
	return cpm.get_state();
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

	if ((prop = node.property ("name")) != 0) {
		_name = prop->value ();
	}

	if ((prop = node.property (X_("sample-rate"))) != 0) {

		_nominal_frame_rate = atoi (prop->value());
		
		if (_nominal_frame_rate != _current_frame_rate) {
			if (AskAboutSampleRateMismatch (_nominal_frame_rate, _current_frame_rate)) {
				throw SRMismatchRejected();
			}
		}
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

	Path
	extra
	Options/Config
	MIDI  <= relies on data from Options/Config
	Locations
	Sources
	AudioRegions
	AudioDiskstreams
	Connections
	Routes
	EditGroups
	MixGroups
	Click
	ControlProtocols
	*/

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

	if (use_config_midi_ports ()) {
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

	AudioFileSource::set_header_position_offset (start_location->start());

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
		ControlProtocolManager::instance().set_state (*child);
	}

	/* here beginneth the second phase ... */

	StateReady (); /* EMIT SIGNAL */

	return 0;

  out:
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
		XMLProperty* prop = (*niter)->property ("default-type");
		
		if (prop && prop->value() == "unknown" ) {
			std::cout << "ignoring route with type unknown. (video-track)" << std::endl;
			// Note: this may mess up remote_control IDs or more..
			continue;
		}

		boost::shared_ptr<Route> route (XMLRouteFactory (**niter));

		if (route == 0) {
			error << _("Session: cannot create Route from XML description.")			      << endmsg;
			return -1;
		}

		BootMessage (string_compose (_("Loaded track/bus %1"), route->name()));

		new_routes.push_back (route);
	}

	add_routes (new_routes, false);

	return 0;
}

boost::shared_ptr<Route>
Session::XMLRouteFactory (const XMLNode& node)
{
	if (node.name() != "Route") {
		return boost::shared_ptr<Route> ((Route*) 0);
	}

	if (node.property ("diskstream") != 0 || node.property ("diskstream-id") != 0) {
		boost::shared_ptr<Route> x (new AudioTrack (*this, node));
		return x;
	} else {
		boost::shared_ptr<Route> x (new Route (*this, node));
		return x;
	}
}

int
Session::load_regions (const XMLNode& node)
{
	XMLNodeList nlist;
	XMLNodeConstIterator niter;
	boost::shared_ptr<AudioRegion> region;

	nlist = node.children();

	set_dirty();

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((region = XMLRegionFactory (**niter, false)) == 0) {
			error << _("Session: cannot create Region from XML description.");
			const XMLProperty *name = (**niter).property("name");

			if (name) {
				error << " " << string_compose (_("Can not load state for region '%1'"), name->value());
			}

			error << endmsg;
		}
	}

	return 0;
}

boost::shared_ptr<AudioRegion>
Session::XMLRegionFactory (const XMLNode& node, bool full)
{
	const XMLProperty* prop;
	boost::shared_ptr<Source> source;
	boost::shared_ptr<AudioSource> as;
	SourceList sources;
	SourceList master_sources;
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

	for (uint32_t n=0; n < nchans; ++n) {
		snprintf (buf, sizeof(buf), X_("master-source-%d"), n);
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
			master_sources.push_back (as);
		} 
	}

	try {
		boost::shared_ptr<AudioRegion> region (boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (sources, node)));

		/* a final detail: this is the one and only place that we know how long missing files are */

		if (region->whole_file()) {
			for (SourceList::iterator sx = sources.begin(); sx != sources.end(); ++sx) {
				boost::shared_ptr<SilentFileSource> sfp = boost::dynamic_pointer_cast<SilentFileSource> (*sx);
				if (sfp) {
					sfp->set_length (region->length());
				}
			}
		}

		if (!master_sources.empty()) {
			if (master_sources.size() != nchans) {
				error << _("Session: XMLNode describing an AudioRegion is missing some master sources; ignored") << endmsg;
			} else {
				region->set_master_sources (master_sources);
			}
		}

		return region;
						       
	}

	catch (failed_constructor& err) {
		return boost::shared_ptr<AudioRegion>();
	}
}

XMLNode&
Session::get_sources_as_xml ()

{
	XMLNode* node = new XMLNode (X_("Sources"));
	Glib::Mutex::Lock lm (audio_source_lock);

	for (AudioSourceList::iterator i = audio_sources.begin(); i != audio_sources.end(); ++i) {
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

		if (!Glib::file_test (buf, Glib::FILE_TEST_EXISTS)) {
			return buf;
		}
	}

	error << string_compose (_("cannot create new file from region name \"%1\" with ident = \"%2\": too many existing files with similar names"),
				 name, identifier)
	      << endmsg;

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

		try {
			if ((source = XMLSourceFactory (**niter)) == 0) {
				error << _("Session: cannot create Source from XML description.") << endmsg;
			}
		}

		catch (non_existent_source& err) {
			warning << _("A sound file is missing. It will be replaced by silence.") << endmsg;
			source = SourceFactory::createSilent (*this, **niter, max_frames, _current_frame_rate);
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
		/* note: do peak building in another thread when loading session state */
		return SourceFactory::create (*this, node, true);
	}

	catch (failed_constructor& err) {
		error << string_compose (_("Found a sound file that cannot be used by %1. Talk to the progammers."), PROGRAM_NAME) << endmsg;
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

	xml_path = Glib::build_filename(dir, template_name + _template_suffix);

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
	string old_path = Glib::build_filename(template_dir(), old_name + _template_suffix);
	string new_path = Glib::build_filename(template_dir(), new_name + _template_suffix);

	return rename (old_path.c_str(), new_path.c_str());
}

int
Session::delete_template (string name) 
{
	string template_path = Glib::build_filename(template_dir(), name + _template_suffix);

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
	
	result = Glib::build_filename(path, sound_dir_name);
	
	if (g_mkdir_with_parents (result.c_str(), 0775)) {
		error << string_compose(_("cannot create sounds directory \"%1\"; ignored"), result) << endmsg;
		return -1;
	}

	dead = Glib::build_filename(path, dead_sound_dir_name);
	
	if (g_mkdir_with_parents (dead.c_str(), 0775)) {
		error << string_compose(_("cannot create dead sounds directory \"%1\"; ignored"), dead) << endmsg;
		return -1;
	}

	peak = Glib::build_filename(path, peak_dir_name);
	
	if (g_mkdir_with_parents (peak.c_str(), 0775)) {
		error << string_compose(_("cannot create peak file directory \"%1\"; ignored"), peak) << endmsg;
		return -1;
	}
	
	/* callers expect this to be terminated ... */
			
	result += G_DIR_SEPARATOR;
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
	boost::shared_ptr<Playlist> playlist;

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
	boost::shared_ptr<Playlist> playlist;

	nlist = node.children();

	set_dirty();

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		
		if ((playlist = XMLPlaylistFactory (**niter)) == 0) {
			error << _("Session: cannot create Playlist from XML description.") << endmsg;
			continue;
		}

		// now manually untrack it

		track_playlist (false, boost::weak_ptr<Playlist> (playlist));
	}

	return 0;
}


boost::shared_ptr<Playlist>
Session::XMLPlaylistFactory (const XMLNode& node)
{
	try {
		return PlaylistFactory::create (*this, node);
	}

	catch (failed_constructor& err) {
		return boost::shared_ptr<Playlist>();
	}
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

	return res;
}

string
Session::old_sound_dir (bool with_path) const
{
	string res;

	if (with_path) {
		res = _path;
	}

	res += old_sound_dir_name;

	return res;
}

string
Session::sound_dir (bool with_path) const
{
	string res;
	string full;
	vector<string> parts;

	if (with_path) {
		res = _path;
	} else {
		full = _path;
	}

	parts.push_back(interchange_dir_name);
	parts.push_back(legalize_for_path (_name));
	parts.push_back(sound_dir_name);

	res += Glib::build_filename(parts);
	if (with_path) {
		full = res;
	} else {
		full += res;
	}
	
	/* if this already exists, don't check for the old session sound directory */

	if (Glib::file_test (full, Glib::FILE_TEST_IS_DIR|Glib::FILE_TEST_EXISTS)) {
		return res;
	}
		
	/* possibly support old session structure */

	string old_nopath;
	string old_withpath;

	old_nopath += old_sound_dir_name;
	old_nopath += G_DIR_SEPARATOR;
	
	old_withpath = _path;
	old_withpath += old_sound_dir_name;
	
	if (Glib::file_test (old_withpath.c_str(), Glib::FILE_TEST_IS_DIR|Glib::FILE_TEST_EXISTS)) {
		if (with_path)
			return old_withpath;
		
		return old_nopath;
	}
	
	/* ok, old "sounds" directory isn't there, return the new path */

	return res;
}

string
Session::peak_dir () const
{
	return Glib::build_filename (_path, peak_dir_name);
}
	
string
Session::automation_dir () const
{
	return Glib::build_filename (_path, "automation");
}

string
Session::analysis_dir () const
{
	return Glib::build_filename (_path, "analysis");
}

string
Session::template_dir ()
{
	return Glib::build_filename (get_user_ardour_path(), "templates");
}

string
Session::route_template_dir ()
{
	return Glib::build_filename (get_user_ardour_path(), "route_templates");
}

string
Session::export_dir () const
{
	return Glib::build_filename (_path, export_dir_name);
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
		path += G_DIR_SEPARATOR;
		
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
Session::route_template_path ()
{
	return suffixed_search_path (X_("route_templates"), true);
}

string
Session::control_protocol_path ()
{
	char *p = getenv ("ARDOUR_CONTROL_SURFACE_PATH");
	if (p && *p) {
		return p;
	}
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
	if ((start = statename.find_last_of (G_DIR_SEPARATOR)) != string::npos) {
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
	rg->set_active(true, this);
	edit_groups.push_back (rg);
	edit_group_added (rg); /* EMIT SIGNAL */
	set_dirty();
	return rg;
}

RouteGroup *
Session::add_mix_group (string name)
{
	RouteGroup* rg = new RouteGroup (*this, name, RouteGroup::Relative);
	rg->set_active(true, this);
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

	if (current_trans->empty()) {
		return;
	}

	gettimeofday (&now, 0);
	current_trans->set_timestamp (now);

	_history.add (current_trans);
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

		boost::shared_ptr<Route> r = (i->first.lock());

		if (r) {
			r->set_meter_point (i->second, arg);
		}
	}
}

void
Session::set_global_route_boolean (GlobalRouteBooleanState s, void (Route::*method)(bool, void*), void* arg)
{
	for (GlobalRouteBooleanState::iterator i = s.begin(); i != s.end(); ++i) {

		boost::shared_ptr<Route> r = (i->first.lock());

		if (r) {
			Route* rp = r.get();
			(rp->*method) (i->second, arg);
		}
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

		start = fullpath.find_last_of (G_DIR_SEPARATOR) + 1;
		if ((end = fullpath.find_last_of ('.')) <0) {
			end = fullpath.length();
		}
		
		template_names.push_back(fullpath.substr(start, (end-start)));
	}
}

void
Session::get_route_templates (vector<RouteTemplateInfo>& template_names)
{
	vector<string *> *templates;
	PathScanner scanner;
	string path;

	path = route_template_path ();
	
	templates = scanner (path, template_filter, 0, false, true);
	
	if (!templates) {
	  return;
	}

	for (vector<string*>::iterator i = templates->begin(); i != templates->end(); ++i) {
		string fullpath = *(*i);

		XMLTree tree;

		if (!tree.read (fullpath.c_str())) {
		  continue;
		}

		XMLNode* root = tree.root();
		
		RouteTemplateInfo rti;

		rti.name = IO::name_from_state (*root->children().front());
		rti.path = fullpath;

		template_names.push_back (rti);
	}

	delete templates;
}

int
Session::read_favorite_dirs (FavoriteDirs & favs)
{
	std::string path = Glib::build_filename (get_user_ardour_path(), "favorite_dirs");

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
	std::string path = Glib::build_filename (get_user_ardour_path(), "favorite_dirs");

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

		if (Glib::path_is_absolute (prop->value())) {
			/* external file, ignore */
			continue;
		}

		/* now we have to actually find the file */

		bool is_new;
		uint16_t chan;
		std::string path;
		std::string name;
		
		if (AudioFileSource::find (prop->value(), true, false, is_new, chan, path, name)) {
			result.insert (path);
		}
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

	if (ripped[ripped.length()-1] == G_DIR_SEPARATOR) {
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

struct RegionCounter {
    typedef std::map<PBD::ID,boost::shared_ptr<AudioSource> > AudioSourceList;
    AudioSourceList::iterator iter;
    boost::shared_ptr<Region> region;
    uint32_t count;
    
    RegionCounter() : count (0) {}
};

int
Session::cleanup_sources (Session::cleanup_report& rep)
{
	vector<boost::shared_ptr<Source> > dead_sources;
	vector<boost::shared_ptr<Playlist> > playlists_tbd;
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

	for (vector<boost::shared_ptr<Playlist> >::iterator x = playlists_tbd.begin(); x != playlists_tbd.end(); ++x) {
		(*x)->drop_references ();
	}

	playlists_tbd.clear ();

	/* step 2: find all un-used sources */

	rep.paths.clear ();
	rep.space = 0;

	for (AudioSourceList::iterator i = audio_sources.begin(); i != audio_sources.end(); ) {
		
		AudioSourceList::iterator tmp;

		tmp = i;
		++tmp;

		/* do not bother with files that are zero size, otherwise we remove the current "nascent"
		   capture files.
		*/

 		if (!i->second->used() && i->second->length() > 0) {
			dead_sources.push_back (i->second);
			i->second->GoingAway();
		} 

		i = tmp;
	}

	/* build a list of all the possible sound directories for the session */

	for (i = session_dirs.begin(); i != session_dirs.end(); ) {

		nexti = i;
		++nexti;

		sound_path += (*i).path;
		sound_path += sound_dir (false);

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
	
	for (AudioSourceList::iterator i = audio_sources.begin(); i != audio_sources.end(); ++i) {
		boost::shared_ptr<AudioFileSource> fs;
		
		if ((fs = boost::dynamic_pointer_cast<AudioFileSource> (i->second)) != 0) {
			all_sources.insert (fs->path());
		} 
	}

	char tmppath1[PATH_MAX+1];
	char tmppath2[PATH_MAX+1];
	
	for (vector<string*>::iterator x = soundfiles->begin(); x != soundfiles->end(); ++x) {

		used = false;
		spath = **x;

		for (set<string>::iterator i = all_sources.begin(); i != all_sources.end(); ++i) {

			realpath(spath.c_str(), tmppath1);
			realpath((*i).c_str(),  tmppath2);

			if (strcmp(tmppath1, tmppath2) == 0) {
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

		if ((*x).find ("/sounds/") != string::npos) {

			/* old school, go up 1 level */

			newpath = Glib::path_get_dirname (*x);      // "sounds" 
			newpath = Glib::path_get_dirname (newpath); // "session-name"

		} else {

			/* new school, go up 4 levels */
			
			newpath = Glib::path_get_dirname (*x);      // "audiofiles" 
			newpath = Glib::path_get_dirname (newpath); // "session-name"
			newpath = Glib::path_get_dirname (newpath); // "interchange"
			newpath = Glib::path_get_dirname (newpath); // "session-dir"
		}

                newpath = Glib::build_filename (newpath, dead_sound_dir_name);

		if (g_mkdir_with_parents (newpath.c_str(), 0755) < 0) {
			error << string_compose(_("Session: cannot create session peakfile folder \"%1\" (%2)"), newpath, strerror (errno)) << endmsg;
			return -1;
		}
                
		newpath = Glib::build_filename (newpath, Glib::path_get_basename ((*x)));
		
		if (Glib::file_test (newpath, Glib::FILE_TEST_EXISTS)) {
			
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
			error << string_compose (_("cannot 4 rename audio file source from %1 to %2 (%3)"),
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

	_history.clear ();

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

			fullpath = Glib::build_filename (dead_sound_dir, dentry->d_name);

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
Session::set_deletion_in_progress ()
{
	_state_of_the_state = StateOfTheState (_state_of_the_state | Deletion);

}

void
Session::add_controllable (Controllable* c)
{
	/* this adds a controllable to the list managed by the Session.
	   this is a subset of those managed by the Controllable class
	   itself, and represents the only ones whose state will be saved
	   as part of the session.
	*/

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
	if (_writable) {
		Stateful::add_instant_xml (node, dir);
	}
	Config->add_instant_xml (node, get_user_ardour_path());
}

int 
Session::save_history (string snapshot_name)
{
    XMLTree tree;
    string xml_path;
    string bak_path;

    if (!_writable) {
	    return 0;
    }

    if (snapshot_name.empty()) {
	snapshot_name = _current_snapshot_name;
    }

    xml_path = _path + snapshot_name + ".history"; 

    bak_path = xml_path + ".bak";

    if (Glib::file_test (xml_path, Glib::FILE_TEST_EXISTS) && ::rename (xml_path.c_str(), bak_path.c_str())) {
        error << _("could not backup old history file, current history not saved.") << endmsg;
        return -1;
    }

    if (!Config->get_save_history() || Config->get_saved_history_depth() < 0) {
	    return 0;
    }

    tree.set_root (&_history.get_state (Config->get_saved_history_depth()));

    if (!tree.write (xml_path))
    {
        error << string_compose (_("history could not be saved to %1"), xml_path) << endmsg;

        /* don't leave a corrupt file lying around if it is
         * possible to fix.
         */

        if (unlink (xml_path.c_str())) {
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

    if (snapshot_name.empty()) {
	    snapshot_name = _current_snapshot_name;
    }

    /* read xml */
    xmlpath = _path + snapshot_name + ".history";
    info << string_compose(_("Loading history from '%1'."), xmlpath) << endmsg;

    if (!Glib::file_test (xmlpath, Glib::FILE_TEST_EXISTS)) {
	    return 1;
    }

    if (!tree.read (xmlpath)) {
	    error << string_compose (_("Could not understand session history file \"%1\""), xmlpath) << endmsg;
	    return -1;
    }

    /* replace history */
    _history.clear();

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
			    
		    } else if (n->name() == X_("GlobalRouteStateCommand")) {

			    if ((c = global_state_command_factory (*n))) {
				    ut->add_command (c);
			    }
			    
		    } else {

			    error << string_compose(_("Couldn't figure out how to make a Command out of a %1 XMLNode."), n->name()) << endmsg;
		    }
	    }

	    _history.add (ut);
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

		waiting_for_sync_offset = Config->get_use_video_sync();

	} else if (PARAM_IS ("mmc-control")) {

		poke_midi_thread ();

	} else if (PARAM_IS ("mmc-device-id") || PARAM_IS ("mmc-receive-device-id")) {

		set_mmc_receive_device_id (Config->get_mmc_receive_device_id());

	} else if (PARAM_IS ("mmc-send-device-id")) {

		set_mmc_send_device_id (Config->get_mmc_send_device_id());

	} else if (PARAM_IS ("midi-control")) {
		
		poke_midi_thread ();

	} else if (PARAM_IS ("raid-path")) {

		setup_raid_path (Config->get_raid_path());

	} else if (PARAM_IS ("smpte-format")) {

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
			if (session_send_mtc) {
				/* mark us ready to send */
				next_quarter_frame_to_send = 0;
			}
		} else {
			session_send_mtc = false;
		}

	} else if (PARAM_IS ("send-mmc")) {
		
		/* only set the internal flag if we have
		   a port.
		*/
		
		if (_mmc_port != 0) {
			session_send_mmc = Config->get_send_mmc();
		} else {
			mmc = 0;
			session_send_mmc = false; 
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

	} else if (PARAM_IS ("slave-source")) {
		set_slave_source (Config->get_slave_source());
	} else if (PARAM_IS ("remote-model")) {
		set_remote_control_ids ();
	} else if (PARAM_IS ("denormal-model")) {
		setup_fpu ();
	} else if (PARAM_IS ("history-depth")) {
		set_history_depth (Config->get_history_depth());
	} else if (PARAM_IS ("sync-all-route-ordering")) {
		sync_order_keys ("session"); 
	} else if (PARAM_IS ("initial-program-change")) {

		if (_mmc_port && Config->get_initial_program_change() >= 0) {
			MIDI::byte* buf = new MIDI::byte[2];
			
			buf[0] = MIDI::program; // channel zero by default
			buf[1] = (Config->get_initial_program_change() & 0x7f);
			deliver_midi (_mmc_port, buf, 2);
		}
	} else if (PARAM_IS ("solo-mute-override")) {
		catch_up_on_solo_mute_override ();
	}

	set_dirty ();
		   
#undef PARAM_IS

}

void
Session::set_history_depth (uint32_t d)
{
	_history.set_depth (d);
}
