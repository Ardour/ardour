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
#include <pbd/search_path.h>
#include <pbd/stacktrace.h>

#include <ardour/audioengine.h>
#include <ardour/configuration.h>
#include <ardour/session.h>
#include <ardour/session_directory.h>
#include <ardour/session_utils.h>
#include <ardour/session_state_utils.h>
#include <ardour/session_metadata.h>
#include <ardour/buffer.h>
#include <ardour/audio_diskstream.h>
#include <ardour/midi_diskstream.h>
#include <ardour/utils.h>
#include <ardour/audioplaylist.h>
#include <ardour/midi_playlist.h>
#include <ardour/smf_source.h>
#include <ardour/audiofilesource.h>
#include <ardour/silentfilesource.h>
#include <ardour/sndfilesource.h>
#include <ardour/midi_source.h>
#include <ardour/sndfile_helpers.h>
#include <ardour/auditioner.h>
#include <ardour/io_processor.h>
#include <ardour/send.h>
#include <ardour/processor.h>
#include <ardour/user_bundle.h>
#include <ardour/slave.h>
#include <ardour/tempo.h>
#include <ardour/audio_track.h>
#include <ardour/midi_track.h>
#include <ardour/midi_patch_manager.h>
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
#include <ardour/playlist_factory.h>
#include <ardour/filename_extensions.h>
#include <ardour/directory_names.h>
#include <ardour/template_utils.h>
#include <ardour/ticker.h>

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

	if (_path[_path.length()-1] != '/') {
		_path += '/';
	}

	/* these two are just provisional settings. set_state()
	   will likely override them.
	*/

	_name = _current_snapshot_name = snapshot_name;

	set_history_depth (Config->get_history_depth());

	_current_frame_rate = _engine.frame_rate ();
	_nominal_frame_rate = _current_frame_rate;
	_base_frame_rate = _current_frame_rate;

	_tempo_map = new TempoMap (_current_frame_rate);
	_tempo_map->StateChanged.connect (mem_fun (*this, &Session::tempo_map_changed));



	g_atomic_int_set (&processing_prohibited, 0);
	insert_cnt = 0;
	_transport_speed = 0;
	_last_transport_speed = 0;
	phi = (uint64_t) (0x1000000);
	target_phi = phi;
	auto_play_legal = false;
	transport_sub_state = 0;
	_transport_frame = 0;
	last_stop_frame = 0;
	end_location = new Location (0, 0, _("end"), Location::Flags ((Location::IsMark|Location::IsEnd)));
	start_location = new Location (0, 0, _("start"), Location::Flags ((Location::IsMark|Location::IsStart)));
	_end_location_is_free = true;
	g_atomic_int_set (&_record_status, Disabled);
	loop_changing = false;
	play_loop = false;
	have_looped = false;
	_last_roll_location = 0;
	_last_record_location = 0;
	pending_locate_frame = 0;
	pending_locate_roll = false;
	pending_locate_flush = false;
	audio_dstream_buffer_size = 0;
	midi_dstream_buffer_size = 0;
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
	session_send_mmc = false;
	session_send_mtc = false;
	post_transport_work = PostTransportWork (0);
	g_atomic_int_set (&butler_should_do_transport_work, 0);
	g_atomic_int_set (&_playback_load, 100);
	g_atomic_int_set (&_capture_load, 100);
	g_atomic_int_set (&_playback_load_min, 100);
	g_atomic_int_set (&_capture_load_min, 100);
	_play_range = false;
	_exporting = false;
	_exporting_realtime = false;
	_gain_automation_buffer = 0;
	_pan_automation_buffer = 0;
	_npan_buffers = 0;
	pending_abort = false;
	destructive_index = 0;
	current_trans = 0;
	first_file_data_format_reset = true;
	first_file_header_format_reset = true;
	butler_thread = (pthread_t) 0;
	//midi_thread = (pthread_t) 0;

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
	Processor::ProcessorCreated.connect (mem_fun (*this, &Session::add_processor));
	NamedSelection::NamedSelectionCreated.connect (mem_fun (*this, &Session::add_named_selection));
	AutomationList::AutomationListCreated.connect (mem_fun (*this, &Session::add_automation_list));

	Controllable::Destroyed.connect (mem_fun (*this, &Session::remove_controllable));

	IO::PortCountChanged.connect (mem_fun (*this, &Session::ensure_buffers));

	/* stop IO objects from doing stuff until we're ready for them */

	IO::disable_panners ();
	IO::disable_ports ();
	IO::disable_connecting ();
}

int
Session::second_stage_init (bool new_session)
{
	AudioFileSource::set_peak_dir (_session_dir->peak_path().to_string());

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
		error << _("Unable to create all required ports")
		      << endmsg;
		return -1;
	}

	catch (...) {
		return -1;
	}

	BootMessage (_("Reset Remote Controls"));

	send_full_time_code (0);
	_engine.transport_locate (0);
	deliver_mmc (MIDI::MachineControl::cmdMmcReset, 0);
	deliver_mmc (MIDI::MachineControl::cmdLocate, 0);
	
	MidiClockTicker::instance().set_session(*this);
	MIDI::Name::MidiPatchManager::instance().set_session(*this);

	/* initial program change will be delivered later; see ::config_changed() */

	BootMessage (_("Reset Control Protocols"));

	ControlProtocolManager::instance().set_session (*this);

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
	SearchPath raid_search_path;

	for (vector<space_and_path>::const_iterator i = session_dirs.begin(); i != session_dirs.end(); ++i) {
		raid_search_path += sys::path((*i).path);
	}
	
	return raid_search_path.to_string ();
}

void
Session::setup_raid_path (string path)
{
	if (path.empty()) {
		return;
	}
	
	space_and_path sp;
	string fspath;

	session_dirs.clear ();

	SearchPath search_path(path);
	SearchPath sound_search_path;
	SearchPath midi_search_path;

	for (
			SearchPath::const_iterator i = search_path.begin();
			i != search_path.end();
			++i
		)
	{
		sp.path = (*i).to_string ();
		sp.blocks = 0; // not needed
		session_dirs.push_back (sp);

		SessionDirectory sdir(sp.path);

		sound_search_path += sdir.sound_path ();
		midi_search_path += sdir.midi_path ();
	}

	// set the AudioFileSource and SMFSource search path

	AudioFileSource::set_search_path (sound_search_path.to_string ());
	SMFSource::set_search_path (midi_search_path.to_string ());


	// reset the round-robin soundfile path thingie

	last_rr_session_dir = session_dirs.begin();
}

int
Session::ensure_subdirs ()
{
	string dir;

	dir = session_directory().peak_path().to_string();

	if (g_mkdir_with_parents (dir.c_str(), 0755) < 0) {
		error << string_compose(_("Session: cannot create session peakfile folder \"%1\" (%2)"), dir, strerror (errno)) << endmsg;
		return -1;
	}

	dir = session_directory().sound_path().to_string();

	if (g_mkdir_with_parents (dir.c_str(), 0755) < 0) {
		error << string_compose(_("Session: cannot create session sounds dir \"%1\" (%2)"), dir, strerror (errno)) << endmsg;
		return -1;
	}
	
	dir = session_directory().midi_path().to_string();

	if (g_mkdir_with_parents (dir.c_str(), 0755) < 0) {
		error << string_compose(_("Session: cannot create session midi dir \"%1\" (%2)"), dir, strerror (errno)) << endmsg;
		return -1;
	}

	dir = session_directory().dead_sound_path().to_string();

	if (g_mkdir_with_parents (dir.c_str(), 0755) < 0) {
		error << string_compose(_("Session: cannot create session dead sounds folder \"%1\" (%2)"), dir, strerror (errno)) << endmsg;
		return -1;
	}

	dir = session_directory().export_path().to_string();

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
			out_path += statefile_suffix;

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
	
	/* Instantiate metadata */
	
	_metadata = new SessionMetadata ();

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
Session::maybe_write_autosave()
{
        if (dirty() && record_status() != Recording) {
                save_state("", true);
        }
}

void
Session::remove_pending_capture_state ()
{
	sys::path pending_state_file_path(_session_dir->root_path());
	
	pending_state_file_path /= _current_snapshot_name + pending_suffix;

	try
	{
		sys::remove (pending_state_file_path);
	}
	catch(sys::filesystem_error& ex)
	{
		error << string_compose(_("Could remove pending capture state at path \"%1\" (%2)"),
				pending_state_file_path.to_string(), ex.what()) << endmsg;
	}
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

	const string old_xml_filename = old_name + statefile_suffix;
	const string new_xml_filename = new_name + statefile_suffix;

	const sys::path old_xml_path = _session_dir->root_path() / old_xml_filename;
	const sys::path new_xml_path = _session_dir->root_path() / new_xml_filename;

	try
	{
		sys::rename (old_xml_path, new_xml_path);
	}
	catch (const sys::filesystem_error& err)
	{
		error << string_compose(_("could not rename snapshot %1 to %2 (%3)"),
				old_name, new_name, err.what()) << endmsg;
	}
}

/** Remove a state file.
 * @param snapshot_name Snapshot name.
 */
void
Session::remove_state (string snapshot_name)
{
	if (snapshot_name == _current_snapshot_name || snapshot_name == _name) {
		// refuse to remove the current snapshot or the "main" one
		return;
	}

	sys::path xml_path(_session_dir->root_path());

	xml_path /= snapshot_name + statefile_suffix;

	if (!create_backup_file (xml_path)) {
		// don't remove it if a backup can't be made
		// create_backup_file will log the error.
		return;
	}

	// and delete it
	sys::remove (xml_path);
}

int
Session::save_state (string snapshot_name, bool pending)
{
	XMLTree tree;
	sys::path xml_path(_session_dir->root_path());

	if (_state_of_the_state & CannotSave) {
		return 1;
	}

	if (!_engine.connected ()) {
		error << _("Ardour's audio engine is not connected and state saving would lose all I/O connections. Session not saved")
		      << endmsg;
		return 1;
	}

	/* tell sources we're saving first, in case they write out to a new file
	 * which should be saved with the state rather than the old one */
	for (SourceMap::const_iterator i = sources.begin(); i != sources.end(); ++i)
		i->second->session_saved();

	tree.set_root (&get_state());

	if (snapshot_name.empty()) {
		snapshot_name = _current_snapshot_name;
	}

	if (!pending) {

		/* proper save: use statefile_suffix (.ardour in English) */
		
		xml_path /= snapshot_name + statefile_suffix;

		/* make a backup copy of the old file */

		if (sys::exists(xml_path) && !create_backup_file (xml_path)) {
			// create_backup_file will log the error
			return -1;
		}

	} else {

		/* pending save: use pending_suffix (.pending in English) */
		xml_path /= snapshot_name + pending_suffix;
	}

	sys::path tmp_path(_session_dir->root_path());

	tmp_path /= snapshot_name + temp_suffix;

	// cerr << "actually writing state to " << xml_path.to_string() << endl;

	if (!tree.write (tmp_path.to_string())) {
		error << string_compose (_("state could not be saved to %1"), tmp_path.to_string()) << endmsg;
		sys::remove (tmp_path);
		return -1;

	} else {

		if (rename (tmp_path.to_string().c_str(), xml_path.to_string().c_str()) != 0) {
			error << string_compose (_("could not rename temporary session file %1 to %2"),
					tmp_path.to_string(), xml_path.to_string()) << endmsg;
			sys::remove (tmp_path);
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
	delete state_tree;
	state_tree = 0;

	state_was_pending = false;

	/* check for leftover pending state from a crashed capture attempt */

	sys::path xmlpath(_session_dir->root_path());
	xmlpath /= snapshot_name + pending_suffix;

	if (sys::exists (xmlpath)) {

		/* there is pending state from a crashed capture attempt */

		if (AskAboutPendingState()) {
			state_was_pending = true;
		} 
	} 

	if (!state_was_pending) {
		xmlpath = _session_dir->root_path();
		xmlpath /= snapshot_name + statefile_suffix;
	}
	
	if (!sys::exists (xmlpath)) {
		error << string_compose(_("%1: session state information file \"%2\" doesn't exist!"), _name, xmlpath.to_string()) << endmsg;
		return 1;
	}

	state_tree = new XMLTree;

	set_dirty();

	if (!state_tree->read (xmlpath.to_string())) {
		error << string_compose(_("Could not understand ardour file %1"), xmlpath.to_string()) << endmsg;
		delete state_tree;
		state_tree = 0;
		return -1;
	}

	XMLNode& root (*state_tree->root());
	
	if (root.name() != X_("Session")) {
		error << string_compose (_("Session file %1 is not an Ardour session"), xmlpath.to_string()) << endmsg;
		delete state_tree;
		state_tree = 0;
		return -1;
	}

	const XMLProperty* prop;
	bool is_old = false; // session is _very_ old (pre-2.0)

	if ((prop = root.property ("version")) == 0) {
		/* no version implies very old version of Ardour */
		is_old = true;
	} else {
		int major_version;
		major_version = atoi (prop->value().c_str()); // grab just the first number before the period
		if (major_version < 2) {
			is_old = true;
		}
	}

	if (is_old) {

		sys::path backup_path(_session_dir->root_path());

		backup_path /= snapshot_name + "-1" + statefile_suffix;

		// only create a backup once
		if (sys::exists (backup_path)) {
			return 0;
		}

		info << string_compose (_("Copying old session file %1 to %2\nUse %2 with Ardour versions before 2.0 from now on"),
					xmlpath.to_string(), backup_path.to_string()) 
		     << endmsg;

		try
		{
			sys::copy_file (xmlpath, backup_path);
		}
		catch(sys::filesystem_error& ex)
		{
			error << string_compose (_("Unable to make backup of state file %1 (%2)"),
					xmlpath.to_string(), ex.what())
				<< endmsg;
			return -1;
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
	snprintf(buf, sizeof(buf), "%d.%d.%d", libardour3_major_version, libardour3_minor_version, libardour3_micro_version);
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

	node->add_child_nocopy (_metadata->get_state());

	child = node->add_child ("Sources");

	if (full_state) {
		Glib::Mutex::Lock sl (source_lock);

		for (SourceMap::iterator siter = sources.begin(); siter != sources.end(); ++siter) {
			
			/* Don't save information about AudioFileSources that are empty */
			
			boost::shared_ptr<AudioFileSource> fs;

			if ((fs = boost::dynamic_pointer_cast<AudioFileSource> (siter->second)) != 0) {

				/* Don't save sources that are empty, unless they're destructive (which are OK
				   if they are empty, because we will re-use them every time.)
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
	
	child = node->add_child ("Bundles");
	{
		boost::shared_ptr<BundleList> bundles = _bundles.reader ();
		for (BundleList::iterator i = bundles->begin(); i != bundles->end(); ++i) {
			boost::shared_ptr<UserBundle> b = boost::dynamic_pointer_cast<UserBundle> (*i);
			if (b) {
				child->add_child_nocopy (b->get_state());
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
			if (!(*i)->is_hidden()) {
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
				return -1;
			}
		}
	}

	setup_raid_path(_session_dir->root_path().to_string());

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
	MIDI Control // relies on data from Options/Config
	Metadata
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

	if ((child = find_named_node (node, "Metadata")) == 0) {
		warning << _("Session: XML state has no metadata section (2.0 session?)") << endmsg;
	} else if (_metadata->set_state (*child)) {
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

	if ((child = find_named_node (node, "Bundles")) == 0) {
		warning << _("Session: XML state has no bundles section (2.0 session?)") << endmsg;
		//goto out;
	} else {
		/* We can't load Bundles yet as they need to be able
		   to convert from port names to Port objects, which can't happen until
		   later */
		_bundle_xml_node = new XMLNode (*child);
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
		ControlProtocolManager::instance().set_protocol_states (*child);
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

		boost::shared_ptr<Route> route (XMLRouteFactory (**niter));

		if (route == 0) {
			error << _("Session: cannot create Route from XML description.") << endmsg;
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

	for (uint32_t n=1; n < nchans; ++n) {
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
			if (master_sources.size() == nchans) {
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

boost::shared_ptr<MidiRegion>
Session::XMLMidiRegionFactory (const XMLNode& node, bool full)
{
	const XMLProperty* prop;
	boost::shared_ptr<Source> source;
	boost::shared_ptr<MidiSource> ms;
	SourceList sources;
	uint32_t nchans = 1;
	
	if (node.name() != X_("Region")) {
		return boost::shared_ptr<MidiRegion>();
	}

	if ((prop = node.property (X_("channels"))) != 0) {
		nchans = atoi (prop->value().c_str());
	}
	
	if ((prop = node.property ("name")) == 0) {
		cerr << "no name for this region\n";
		abort ();
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
		/* a final detail: this is the one and only place that we know how long missing files are */

		if (region->whole_file()) {
			for (SourceList::iterator sx = sources.begin(); sx != sources.end(); ++sx) {
				boost::shared_ptr<SilentFileSource> sfp = boost::dynamic_pointer_cast<SilentFileSource> (*sx);
				if (sfp) {
					sfp->set_length (region->length());
				}
			}
		}

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

	return *node;
}

string
Session::path_from_region_name (DataType type, string name, string identifier)
{
	char buf[PATH_MAX+1];
	uint32_t n;
	SessionDirectory sdir(get_best_session_directory_for_new_source());
	sys::path source_dir = ((type == DataType::AUDIO)
		? sdir.sound_path() : sdir.midi_path());

	string ext = ((type == DataType::AUDIO) ? ".wav" : ".mid");

	for (n = 0; n < 999999; ++n) {
		if (identifier.length()) {
			snprintf (buf, sizeof(buf), "%s%s%" PRIu32 "%s", name.c_str(), 
				  identifier.c_str(), n, ext.c_str());
		} else {
			snprintf (buf, sizeof(buf), "%s-%" PRIu32 "%s", name.c_str(),
					n, ext.c_str());
		}

		sys::path source_path = source_dir / buf;

		if (!sys::exists (source_path)) {
			return source_path.to_string();
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
		error << _("Found a sound file that cannot be used by Ardour. Talk to the progammers.") << endmsg;
		return boost::shared_ptr<Source>();
	}
}

int
Session::save_template (string template_name)
{
	XMLTree tree;

	if (_state_of_the_state & CannotSave) {
		return -1;
	}

	sys::path user_template_dir(user_template_directory());

	try
	{
		sys::create_directories (user_template_dir);
	}
	catch(sys::filesystem_error& ex)
	{
		error << string_compose(_("Could not create mix templates directory \"%1\" (%2)"),
				user_template_dir.to_string(), ex.what()) << endmsg;
		return -1;
	}

	tree.set_root (&get_template());

	sys::path template_file_path(user_template_dir);
	template_file_path /= template_name + template_suffix;

	if (sys::exists (template_file_path))
	{
		warning << string_compose(_("Template \"%1\" already exists - new version not created"),
				template_file_path.to_string()) << endmsg;
		return -1;
	}

	if (!tree.write (template_file_path.to_string())) {
		error << _("mix template not saved") << endmsg;
		return -1;
	}

	return 0;
}

int
Session::rename_template (string old_name, string new_name) 
{
	sys::path old_path (user_template_directory());
	old_path /= old_name + template_suffix;

	sys::path new_path(user_template_directory());
	new_path /= new_name + template_suffix;

	if (sys::exists (new_path)) {
		warning << string_compose(_("Template \"%1\" already exists - template not renamed"),
					  new_path.to_string()) << endmsg;
		return -1;
	}

	try {
		sys::rename (old_path, new_path);
		return 0;
	} catch (...) {
		return -1;
	}
}

int
Session::delete_template (string name) 
{
	sys::path path = user_template_directory();
	path /= name + template_suffix;

	try {
		sys::remove (path);
		return 0;
	} catch (...) {
		return -1;
	}
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

string
Session::get_best_session_directory_for_new_source ()
{
	vector<space_and_path>::iterator i;
	string result = _session_dir->root_path().to_string();

	/* handle common case without system calls */

	if (session_dirs.size() == 1) {
		return result;
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
		/* use RR selection process, ensuring that the one
		   picked works OK.
		*/

		i = last_rr_session_dir;

		do {
			if (++i == session_dirs.end()) {
				i = session_dirs.begin();
			}

			if ((*i).blocks * 4096 >= Config->get_disk_choice_space_threshold()) {
				if (create_session_directory ((*i).path)) {
					result = (*i).path;
					last_rr_session_dir = i;
					return result;
				}
			}

		} while (i != last_rr_session_dir);

	} else {

		/* pick FS with the most freespace (and that
		   seems to actually work ...)
		*/
		
		vector<space_and_path> sorted;
		space_and_path_ascending_cmp cmp;

		sorted = session_dirs;
		sort (sorted.begin(), sorted.end(), cmp);
		
		for (i = sorted.begin(); i != sorted.end(); ++i) {
			if (create_session_directory ((*i).path)) {
				result = (*i).path;
				last_rr_session_dir = i;
				return result;
			}
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
Session::automation_dir () const
{
	return Glib::build_filename (_path, "automation");
}

string
Session::analysis_dir () const
{
	return Glib::build_filename (_path, "analysis");
}

int
Session::load_bundles (XMLNode const & node)
{
 	XMLNodeList nlist = node.children();
 	XMLNodeConstIterator niter;

 	set_dirty();

 	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
 		if ((*niter)->name() == "InputBundle") {
			add_bundle (boost::shared_ptr<UserBundle> (new UserBundle (**niter, true)));
 		} else if ((*niter)->name() == "OutputBundle") {
 			add_bundle (boost::shared_ptr<UserBundle> (new UserBundle (**niter, false)));
 		} else {
 			error << string_compose(_("Unknown node \"%1\" found in Bundles list from state file"), (*niter)->name()) << endmsg;
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

void
Session::auto_save()
{
	save_state (_current_snapshot_name);
}

static bool
state_file_filter (const string &str, void *arg)
{
	return (str.length() > strlen(statefile_suffix) &&
		str.find (statefile_suffix) == (str.length() - strlen (statefile_suffix)));
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
Session::begin_reversible_command (const string& name)
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
		if (!(*i)->is_hidden()) {
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
		if (!(*i)->is_hidden()) {
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
accept_all_non_peak_files (const string& path, void *arg)
{
	return (path.length() > 5 && path.find (peakfile_suffix) != (path.length() - 5));
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

		sys::path source_path = _session_dir->sound_path ();

		source_path /= prop->value ();

		result.insert (source_path.to_string ());
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
	this_snapshot_path += statefile_suffix;

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
	// FIXME: needs adaptation to midi
	
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

	for (SourceMap::iterator i = sources.begin(); i != sources.end(); ) {
		
		SourceMap::iterator tmp;

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

		SessionDirectory sdir ((*i).path);
		sound_path += sdir.sound_path().to_string();

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

		newpath += '/';
		newpath += dead_sound_dir_name;

		if (g_mkdir_with_parents (newpath.c_str(), 0755) < 0) {
			error << string_compose(_("Session: cannot create session peakfile folder \"%1\" (%2)"), newpath, strerror (errno)) << endmsg;
			return -1;
		}

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
		peakpath += peakfile_suffix;

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
	// FIXME: needs adaptation for MIDI
	
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
Session::set_deletion_in_progress ()
{
	_state_of_the_state = StateOfTheState (_state_of_the_state | Deletion);

}

void
Session::add_controllable (boost::shared_ptr<Controllable> c)
{
	/* this adds a controllable to the list managed by the Session.
	   this is a subset of those managed by the Controllable class
	   itself, and represents the only ones whose state will be saved
	   as part of the session.
	*/

	Glib::Mutex::Lock lm (controllables_lock);
	controllables.insert (c);
}
	
struct null_deleter { void operator()(void const *) const {} };

void
Session::remove_controllable (Controllable* c)
{
	if (_state_of_the_state | Deletion) {
		return;
	}

	Glib::Mutex::Lock lm (controllables_lock);

	Controllables::iterator x = controllables.find(
		 boost::shared_ptr<Controllable>(c, null_deleter()));

	if (x != controllables.end()) {
		controllables.erase (x);
	}
}	

boost::shared_ptr<Controllable>
Session::controllable_by_id (const PBD::ID& id)
{
	Glib::Mutex::Lock lm (controllables_lock);
	
	for (Controllables::iterator i = controllables.begin(); i != controllables.end(); ++i) {
		if ((*i)->id() == id) {
			return *i;
		}
	}

	return boost::shared_ptr<Controllable>();
}

void 
Session::add_instant_xml (XMLNode& node, bool write_to_config)
{
	Stateful::add_instant_xml (node, _path);
	if (write_to_config) {
		Config->add_instant_xml (node);
	}
}

XMLNode*
Session::instant_xml (const string& node_name)
{
	return Stateful::instant_xml (node_name, _path);
}

int 
Session::save_history (string snapshot_name)
{
	XMLTree tree;
	
 	if (snapshot_name.empty()) {
		snapshot_name = _current_snapshot_name;
	}
  
 	const string history_filename = snapshot_name + history_suffix;
	const string backup_filename = history_filename + backup_suffix;
 	const sys::path xml_path = _session_dir->root_path() / history_filename;
	const sys::path backup_path = _session_dir->root_path() / backup_filename;

	if (sys::exists (xml_path)) {
		try
		{
			sys::rename (xml_path, backup_path);
		}
		catch (const sys::filesystem_error& err)
		{
			error << _("could not backup old history file, current history not saved") << endmsg;
			return -1;
		}
 	}


	if (!Config->get_save_history() || Config->get_saved_history_depth() < 0) {
		return 0;
	}

 	tree.set_root (&_history.get_state (Config->get_saved_history_depth()));

	if (!tree.write (xml_path.to_string()))
	{
		error << string_compose (_("history could not be saved to %1"), xml_path.to_string()) << endmsg;

		try
		{
			sys::remove (xml_path);
			sys::rename (backup_path, xml_path);
		}
		catch (const sys::filesystem_error& err)
		{
			error << string_compose (_("could not restore history file from backup %1 (%2)"),
					backup_path.to_string(), err.what()) << endmsg;
		}

		return -1;
	}

	return 0;
}

int
Session::restore_history (string snapshot_name)
{
	XMLTree tree;

	if (snapshot_name.empty()) {
		snapshot_name = _current_snapshot_name;
	}

	const string xml_filename = snapshot_name + history_suffix;
	const sys::path xml_path = _session_dir->root_path() / xml_filename;

    cerr << "Loading history from " << xml_path.to_string() << endmsg;

	if (!sys::exists (xml_path)) {
		info << string_compose (_("%1: no history file \"%2\" for this session."),
				_name, xml_path.to_string()) << endmsg;
		return 1;
	}

	if (!tree.read (xml_path.to_string())) {
		error << string_compose (_("Could not understand session history file \"%1\""),
				xml_path.to_string()) << endmsg;
		return -1;
	}

	// replace history
	_history.clear();

    for (XMLNodeConstIterator it  = tree.root()->children().begin(); it != tree.root()->children().end(); it++) {
	    
	    XMLNode *t = *it;
	    UndoTransaction* ut = new UndoTransaction ();
	    struct timeval tv;
	    
	    ut->set_name(t->property("name")->value());
	    stringstream ss(t->property("tv-sec")->value());
	    ss >> tv.tv_sec;
	    ss.str(t->property("tv-usec")->value());
	    ss >> tv.tv_usec;
	    ut->set_timestamp(tv);
	    
	    for (XMLNodeConstIterator child_it  = t->children().begin();
				child_it != t->children().end(); child_it++)
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
			    
		    } else if (n->name() == "DeltaCommand") {
		    	 PBD::ID  id(n->property("midi-source")->value());
	    		 boost::shared_ptr<MidiSource> midi_source = 
	    			 boost::dynamic_pointer_cast<MidiSource, Source>(source_by_id(id));
		    	 if(midi_source) {
		    		 ut->add_command(new MidiModel::DeltaCommand(midi_source->model(), *n));		    		 
		    	 } else {
		    		 error << "FIXME: Failed to downcast MidiSource for DeltaCommand" << endmsg;
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

		//poke_midi_thread ();

	} else if (PARAM_IS ("mmc-device-id") || PARAM_IS ("mmc-receive-id")) {

		if (mmc) {
			mmc->set_receive_device_id (Config->get_mmc_receive_device_id());
		}

	} else if (PARAM_IS ("mmc-send-id")) {

		if (mmc) {
			mmc->set_send_device_id (Config->get_mmc_send_device_id());
		}

	} else if (PARAM_IS ("midi-control")) {
		
		//poke_midi_thread ();

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
	}  else if (PARAM_IS ("denormal-model")) {
		setup_fpu ();
	} else if (PARAM_IS ("history-depth")) {
		set_history_depth (Config->get_history_depth());
	} else if (PARAM_IS ("sync-all-route-ordering")) {
		sync_order_keys ("session");
	} else if (PARAM_IS ("initial-program-change")) {

		if (_mmc_port && Config->get_initial_program_change() >= 0) {
			MIDI::byte buf[2];
			
			buf[0] = MIDI::program; // channel zero by default
			buf[1] = (Config->get_initial_program_change() & 0x7f);

			_mmc_port->midimsg (buf, sizeof (buf), 0);
		}
	} else if (PARAM_IS ("initial-program-change")) {

		if (_mmc_port && Config->get_initial_program_change() >= 0) {
			MIDI::byte* buf = new MIDI::byte[2];
			
			buf[0] = MIDI::program; // channel zero by default
			buf[1] = (Config->get_initial_program_change() & 0x7f);
			// deliver_midi (_mmc_port, buf, 2);
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
