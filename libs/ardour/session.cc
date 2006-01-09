/*
    Copyright (C) 1999-2004 Paul Davis 

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

#include <algorithm>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <cstdio> /* sprintf(3) ... grrr */
#include <cmath>
#include <cerrno>
#include <unistd.h>
#include <limits.h>

#include <sigc++/bind.h>
#include <sigc++/retype.h>

#include <pbd/error.h>
#include <pbd/lockmonitor.h>
#include <pbd/pathscanner.h>
#include <pbd/stl_delete.h>
#include <pbd/basename.h>
#include <pbd/dirname.h>

#include <ardour/audioengine.h>
#include <ardour/configuration.h>
#include <ardour/session.h>
#include <ardour/diskstream.h>
#include <ardour/utils.h>
#include <ardour/audioplaylist.h>
#include <ardour/audioregion.h>
#include <ardour/source.h>
#include <ardour/filesource.h>
#include <ardour/destructive_filesource.h>
#include <ardour/sndfilesource.h>
#include <ardour/auditioner.h>
#include <ardour/recent_sessions.h>
#include <ardour/redirect.h>
#include <ardour/send.h>
#include <ardour/insert.h>
#include <ardour/connection.h>
#include <ardour/slave.h>
#include <ardour/tempo.h>
#include <ardour/audio_track.h>
#include <ardour/cycle_timer.h>
#include <ardour/named_selection.h>
#include <ardour/crossfade.h>
#include <ardour/playlist.h>
#include <ardour/click.h>
#include <ardour/timestamps.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
//using namespace sigc;

const char* Session::_template_suffix = X_(".template");
const char* Session::_statefile_suffix = X_(".ardour");
const char* Session::_pending_suffix = X_(".pending");
const char* Session::sound_dir_name = X_("sounds");
const char* Session::peak_dir_name = X_("peaks");
const char* Session::dead_sound_dir_name = X_("dead_sounds");

Session::compute_peak_t				Session::compute_peak 			= 0;
Session::apply_gain_to_buffer_t		Session::apply_gain_to_buffer 	= 0;
Session::mix_buffers_with_gain_t	Session::mix_buffers_with_gain 	= 0;
Session::mix_buffers_no_gain_t		Session::mix_buffers_no_gain 	= 0;

sigc::signal<int> Session::AskAboutPendingState;

int
Session::find_session (string str, string& path, string& snapshot, bool& isnew)
{
	struct stat statbuf;
	char buf[PATH_MAX+1];

	isnew = false;

	if (!realpath (str.c_str(), buf) && (errno != ENOENT && errno != ENOTDIR)) {
		error << string_compose (_("Could not resolve path: %1 (%2)"), buf, strerror(errno)) << endmsg;
		return -1;
	}

	str = buf;
	
	/* check to see if it exists, and what it is */

	if (stat (str.c_str(), &statbuf)) {
		if (errno == ENOENT) {
			isnew = true;
		} else {
			error << string_compose (_("cannot check session path %1 (%2)"), str, strerror (errno))
			      << endmsg;
			return -1;
		}
	}

	if (!isnew) {

		/* it exists, so it must either be the name
		   of the directory, or the name of the statefile
		   within it.
		*/

		if (S_ISDIR (statbuf.st_mode)) {

			string::size_type slash = str.find_last_of ('/');
		
			if (slash == string::npos) {
				
				/* a subdirectory of cwd, so statefile should be ... */

				string tmp;
				tmp = str;
				tmp += '/';
				tmp += str;
				tmp += _statefile_suffix;

				/* is it there ? */
				
				if (stat (tmp.c_str(), &statbuf)) {
					error << string_compose (_("cannot check statefile %1 (%2)"), tmp, strerror (errno))
					      << endmsg;
					return -1;
				}

				path = str;
				snapshot = str;

			} else {

				/* some directory someplace in the filesystem.
				   the snapshot name is the directory name
				   itself.
				*/

				path = str;
				snapshot = str.substr (slash+1);
					
			}

		} else if (S_ISREG (statbuf.st_mode)) {
			
			string::size_type slash = str.find_last_of ('/');
			string::size_type suffix;

			/* remove the suffix */
			
			if (slash != string::npos) {
				snapshot = str.substr (slash+1);
			} else {
				snapshot = str;
			}

			suffix = snapshot.find (_statefile_suffix);
			
			if (suffix == string::npos) {
				error << string_compose (_("%1 is not an Ardour snapshot file"), str) << endmsg;
				return -1;
			}

			/* remove suffix */

			snapshot = snapshot.substr (0, suffix);
			
			if (slash == string::npos) {
				
				/* we must be in the directory where the 
				   statefile lives. get it using cwd().
				*/

				char cwd[PATH_MAX+1];

				if (getcwd (cwd, sizeof (cwd)) == 0) {
					error << string_compose (_("cannot determine current working directory (%1)"), strerror (errno))
					      << endmsg;
					return -1;
				}

				path = cwd;

			} else {

				/* full path to the statefile */

				path = str.substr (0, slash);
			}
				
		} else {

			/* what type of file is it? */
			error << string_compose (_("unknown file type for session %1"), str) << endmsg;
			return -1;
		}

	} else {

		/* its the name of a new directory. get the name
		   as "dirname" does.
		*/

		string::size_type slash = str.find_last_of ('/');

		if (slash == string::npos) {
			
			/* no slash, just use the name, but clean it up */
			
			path = legalize_for_path (str);
			snapshot = path;
			
		} else {
			
			path = str;
			snapshot = str.substr (slash+1);
		}
	}

	return 0;
}

Session::Session (AudioEngine &eng,
		  string fullpath,
		  string snapshot_name,
		  string* mix_template)

	: _engine (eng),
	  _mmc_port (default_mmc_port),
	  _mtc_port (default_mtc_port),
	  _midi_port (default_midi_port),
	  pending_events (2048),
	  midi_requests (128), // the size of this should match the midi request pool size
	  main_outs (0)
{
	bool new_session;

	cerr << "Loading session " << fullpath << " using snapshot " << snapshot_name << endl;

	n_physical_outputs = _engine.n_physical_outputs();
	n_physical_inputs =  _engine.n_physical_inputs();

	first_stage_init (fullpath, snapshot_name);
	
	if (create (new_session, mix_template, _engine.frame_rate() * 60 * 5)) {
		throw failed_constructor ();
	}
	
	if (second_stage_init (new_session)) {
		throw failed_constructor ();
	}
	
	store_recent_sessions(_name, _path);
	
	bool was_dirty = dirty();

	_state_of_the_state = StateOfTheState (_state_of_the_state & ~Dirty);

	if (was_dirty) {
		DirtyChanged (); /* EMIT SIGNAL */
	}
}

Session::Session (AudioEngine &eng,
		  string fullpath,
		  string snapshot_name,
		  AutoConnectOption input_ac,
		  AutoConnectOption output_ac,
		  uint32_t control_out_channels,
		  uint32_t master_out_channels,
		  uint32_t requested_physical_in,
		  uint32_t requested_physical_out,
		  jack_nframes_t initial_length)

	: _engine (eng),
	  _mmc_port (default_mmc_port),
	  _mtc_port (default_mtc_port),
	  _midi_port (default_midi_port),
	  pending_events (2048),
	  midi_requests (16),
	  main_outs (0)

{
	bool new_session;

	cerr << "Loading session " << fullpath << " using snapshot " << snapshot_name << endl;

	n_physical_outputs = max (requested_physical_out, _engine.n_physical_outputs());
	n_physical_inputs = max (requested_physical_in, _engine.n_physical_inputs());

	first_stage_init (fullpath, snapshot_name);
	
	if (create (new_session, 0, initial_length)) {
		throw failed_constructor ();
	}

	if (control_out_channels) {
		Route* r;
		r = new Route (*this, _("monitor"), -1, control_out_channels, -1, control_out_channels, Route::ControlOut);
		add_route (r);
		_control_out = r;
	}

	if (master_out_channels) {
		Route* r;
		r = new Route (*this, _("master"), -1, master_out_channels, -1, master_out_channels, Route::MasterOut);
		add_route (r);
		_master_out = r;
	} else {
		/* prohibit auto-connect to master, because there isn't one */
		output_ac = AutoConnectOption (output_ac & ~AutoConnectMaster);
	}

	input_auto_connect = input_ac;
	output_auto_connect = output_ac;

	if (second_stage_init (new_session)) {
		throw failed_constructor ();
	}
	
	store_recent_sessions(_name, _path);
	
	bool was_dirty = dirty ();

	_state_of_the_state = StateOfTheState (_state_of_the_state & ~Dirty);

	if (was_dirty) {
		DirtyChanged (); /* EMIT SIGNAL */
	}
}

Session::~Session ()
{
	/* if we got to here, leaving pending capture state around
	   is a mistake.
	*/

	remove_pending_capture_state ();

	_state_of_the_state = StateOfTheState (CannotSave|Deletion);
	_engine.remove_session ();
	
	going_away (); /* EMIT SIGNAL */
	
	terminate_butler_thread ();
	terminate_midi_thread ();
	terminate_feedback ();
	
	if (click_data && click_data != default_click) {
		delete [] click_data;
	}

	if (click_emphasis_data && click_emphasis_data != default_click_emphasis) {
		delete [] click_emphasis_data;
	}

	clear_clicks ();

	if (_click_io) {
		delete _click_io;
	}


	if (auditioner) {
		delete auditioner;
	}

	for (vector<Sample*>::iterator i = _passthru_buffers.begin(); i != _passthru_buffers.end(); ++i) {
		free(*i);
	}

	for (vector<Sample*>::iterator i = _silent_buffers.begin(); i != _silent_buffers.end(); ++i) {
		free(*i);
	}

#undef TRACK_DESTRUCTION
#ifdef TRACK_DESTRUCTION
	cerr << "delete named selections\n";
#endif /* TRACK_DESTRUCTION */
	for (NamedSelectionList::iterator i = named_selections.begin(); i != named_selections.end(); ) {
		NamedSelectionList::iterator tmp;

		tmp = i;
		++tmp;

		delete *i;
		i = tmp;
	}

#ifdef TRACK_DESTRUCTION
	cerr << "delete playlists\n";
#endif /* TRACK_DESTRUCTION */
	for (PlaylistList::iterator i = playlists.begin(); i != playlists.end(); ) {
		PlaylistList::iterator tmp;

		tmp = i;
		++tmp;

		delete *i;
		
		i = tmp;
	}

#ifdef TRACK_DESTRUCTION
	cerr << "delete audio regions\n";
#endif /* TRACK_DESTRUCTION */
	for (AudioRegionList::iterator i = audio_regions.begin(); i != audio_regions.end(); ) {
		AudioRegionList::iterator tmp;

		tmp =i;
		++tmp;

		delete (*i).second;

		i = tmp;
	}
	
#ifdef TRACK_DESTRUCTION
	cerr << "delete routes\n";
#endif /* TRACK_DESTRUCTION */
	for (RouteList::iterator i = routes.begin(); i != routes.end(); ) {
		RouteList::iterator tmp;
		tmp = i;
		++tmp;
		delete *i;
		i = tmp;
	}

#ifdef TRACK_DESTRUCTION
	cerr << "delete diskstreams\n";
#endif /* TRACK_DESTRUCTION */
	for (DiskStreamList::iterator i = diskstreams.begin(); i != diskstreams.end(); ) {
		DiskStreamList::iterator tmp;

		tmp = i;
		++tmp;

		delete *i;

		i = tmp;
	}

#ifdef TRACK_DESTRUCTION
	cerr << "delete sources\n";
#endif /* TRACK_DESTRUCTION */
	for (SourceList::iterator i = sources.begin(); i != sources.end(); ) {
		SourceList::iterator tmp;

		tmp = i;
		++tmp;

		delete (*i).second;

		i = tmp;
	}

#ifdef TRACK_DESTRUCTION
	cerr << "delete mix groups\n";
#endif /* TRACK_DESTRUCTION */
	for (list<RouteGroup *>::iterator i = mix_groups.begin(); i != mix_groups.end(); ) {
		list<RouteGroup*>::iterator tmp;

		tmp = i;
		++tmp;

		delete *i;

		i = tmp;
	}

#ifdef TRACK_DESTRUCTION
	cerr << "delete edit groups\n";
#endif /* TRACK_DESTRUCTION */
	for (list<RouteGroup *>::iterator i = edit_groups.begin(); i != edit_groups.end(); ) {
		list<RouteGroup*>::iterator tmp;
		
		tmp = i;
		++tmp;

		delete *i;

		i = tmp;
	}
	
#ifdef TRACK_DESTRUCTION
	cerr << "delete connections\n";
#endif /* TRACK_DESTRUCTION */
	for (ConnectionList::iterator i = _connections.begin(); i != _connections.end(); ) {
		ConnectionList::iterator tmp;

		tmp = i;
		++tmp;

		delete *i;

		i = tmp;
	}

	if (butler_mixdown_buffer) {
		delete [] butler_mixdown_buffer;
	}

	if (butler_gain_buffer) {
		delete [] butler_gain_buffer;
	}

	Crossfade::set_buffer_size (0);

	if (mmc) {
		delete mmc;
	}

	if (state_tree) {
		delete state_tree;
	}
}

void
Session::set_worst_io_latencies (bool take_lock)
{
	_worst_output_latency = 0;
	_worst_input_latency = 0;

	if (!_engine.connected()) {
		return;
	}

	if (take_lock) {
		route_lock.read_lock ();
	}
	
	for (RouteList::iterator i = routes.begin(); i != routes.end(); ++i) {
		_worst_output_latency = max (_worst_output_latency, (*i)->output_latency());
		_worst_input_latency = max (_worst_input_latency, (*i)->input_latency());
	}

	if (take_lock) {
		route_lock.unlock ();
	}
}

void
Session::when_engine_running ()
{
	string first_physical_output;

	/* we don't want to run execute this again */

	first_time_running.disconnect ();

	set_block_size (_engine.frames_per_cycle());
	set_frame_rate (_engine.frame_rate());

	/* every time we reconnect, recompute worst case output latencies */

	_engine.Running.connect (sigc::bind (mem_fun (*this, &Session::set_worst_io_latencies), true));

	if (synced_to_jack()) {
		_engine.transport_stop ();
	}

	if (Config->get_jack_time_master()) {
		_engine.transport_locate (_transport_frame);
	}

	_clicking = false;

	try {
		XMLNode* child = 0;
		
		_click_io = new ClickIO (*this, "click", 0, 0, -1, -1);

		if (state_tree && (child = find_named_node (*state_tree->root(), "Click")) != 0) {

			/* existing state for Click */
			
			if (_click_io->set_state (*child->children().front()) == 0) {
				
				_clicking = click_requested;

			} else {

				error << _("could not setup Click I/O") << endmsg;
				_clicking = false;
			}

		} else {
			
			/* default state for Click */

			first_physical_output = _engine.get_nth_physical_output (0);
			
			if (first_physical_output.length()) {
				if (_click_io->add_output_port (first_physical_output, this)) {
					// relax, even though its an error
				} else {
					_clicking = click_requested;
				}
			}
		}
	}

	catch (failed_constructor& err) {
		error << _("cannot setup Click I/O") << endmsg;
	}

	set_worst_io_latencies (true);

	if (_clicking) {
		 ControlChanged (Clicking); /* EMIT SIGNAL */
	}

	if (auditioner == 0) {

		/* we delay creating the auditioner till now because
		   it makes its own connections to ports named
		   in the ARDOUR_RC config file. the engine has
		   to be running for this to work.
		*/

		try {
			auditioner = new Auditioner (*this);
		}

		catch (failed_constructor& err) {
			warning << _("cannot create Auditioner: no auditioning of regions possible") << endmsg;
		}
	}

	/* Create a set of Connection objects that map
	   to the physical outputs currently available
	*/

	/* ONE: MONO */

	for (uint32_t np = 0; np < n_physical_outputs; ++np) {
		char buf[32];
		snprintf (buf, sizeof (buf), _("out %" PRIu32), np+1);

		Connection* c = new OutputConnection (buf, true);

		c->add_port ();
		c->add_connection (0, _engine.get_nth_physical_output (np));

		add_connection (c);
	}

	for (uint32_t np = 0; np < n_physical_inputs; ++np) {
		char buf[32];
		snprintf (buf, sizeof (buf), _("in %" PRIu32), np+1);

		Connection* c = new InputConnection (buf, true);

		c->add_port ();
		c->add_connection (0, _engine.get_nth_physical_input (np));

		add_connection (c);
	}

	/* TWO: STEREO */

	for (uint32_t np = 0; np < n_physical_outputs; np +=2) {
		char buf[32];
		snprintf (buf, sizeof (buf), _("out %" PRIu32 "+%" PRIu32), np+1, np+2);

		Connection* c = new OutputConnection (buf, true);

		c->add_port ();
		c->add_port ();
		c->add_connection (0, _engine.get_nth_physical_output (np));
		c->add_connection (1, _engine.get_nth_physical_output (np+1));

		add_connection (c);
	}

	for (uint32_t np = 0; np < n_physical_inputs; np +=2) {
		char buf[32];
		snprintf (buf, sizeof (buf), _("in %" PRIu32 "+%" PRIu32), np+1, np+2);

		Connection* c = new InputConnection (buf, true);

		c->add_port ();
		c->add_port ();
		c->add_connection (0, _engine.get_nth_physical_input (np));
		c->add_connection (1, _engine.get_nth_physical_input (np+1));

		add_connection (c);
	}

	/* THREE MASTER */

	if (_master_out) {

		/* create master/control ports */
		
		if (_master_out) {
			uint32_t n;

			/* force the master to ignore any later call to this */
			
			if (_master_out->pending_state_node) {
				_master_out->ports_became_legal();
			}

			/* no panner resets till we are through */
			
			_master_out->defer_pan_reset ();
			
			while ((int) _master_out->n_inputs() < _master_out->input_maximum()) {
				if (_master_out->add_input_port ("", this)) {
					error << _("cannot setup master inputs") 
					      << endmsg;
					break;
				}
			}
			n = 0;
			while ((int) _master_out->n_outputs() < _master_out->output_maximum()) {
				if (_master_out->add_output_port (_engine.get_nth_physical_output (n), this)) {
					error << _("cannot setup master outputs")
					      << endmsg;
					break;
				}
				n++;
			}

			_master_out->allow_pan_reset ();
			
		}

		Connection* c = new OutputConnection (_("Master Out"), true);

		for (uint32_t n = 0; n < _master_out->n_inputs (); ++n) {
			c->add_port ();
			c->add_connection ((int) n, _master_out->input(n)->name());
		}
		add_connection (c);
	} 

	hookup_io ();

	/* catch up on send+insert cnts */

	insert_cnt = 0;
	
	for (slist<PortInsert*>::iterator i = _port_inserts.begin(); i != _port_inserts.end(); ++i) {
		uint32_t id;

		if (sscanf ((*i)->name().c_str(), "%*s %u", &id) == 1) {
			if (id > insert_cnt) {
				insert_cnt = id;
			}
		}
	}

	send_cnt = 0;

	for (slist<Send*>::iterator i = _sends.begin(); i != _sends.end(); ++i) {
		uint32_t id;
		
		if (sscanf ((*i)->name().c_str(), "%*s %u", &id) == 1) {
			if (id > send_cnt) {
				send_cnt = id;
			}
		}
	}

	_state_of_the_state = StateOfTheState (_state_of_the_state & ~(CannotSave|Dirty));

	/* hook us up to the engine */

	_engine.set_session (this);

	_state_of_the_state = Clean;

	DirtyChanged (); /* EMIT SIGNAL */
}

void
Session::hookup_io ()
{
	/* stop graph reordering notifications from
	   causing resorts, etc.
	*/

	_state_of_the_state = StateOfTheState (_state_of_the_state | InitialConnecting);

	/* Tell all IO objects to create their ports */

	IO::enable_ports ();

	if (_control_out) {
		uint32_t n;

		while ((int) _control_out->n_inputs() < _control_out->input_maximum()) {
			if (_control_out->add_input_port ("", this)) {
				error << _("cannot setup control inputs")
				      << endmsg;
				break;
			}
		}
		n = 0;
		while ((int) _control_out->n_outputs() < _control_out->output_maximum()) {
			if (_control_out->add_output_port (_engine.get_nth_physical_output (n), this)) {
				error << _("cannot set up master outputs")
				      << endmsg;
				break;
			}
			n++;
		}
	}

	/* Tell all IO objects to connect themselves together */

	IO::enable_connecting ();

	/* Now reset all panners */

	IO::reset_panners ();

	/* Anyone who cares about input state, wake up and do something */

	IOConnectionsComplete (); /* EMIT SIGNAL */

	_state_of_the_state = StateOfTheState (_state_of_the_state & ~InitialConnecting);

	/* now handle the whole enchilada as if it was one
	   graph reorder event.
	*/

	graph_reordered ();

	/* update mixer solo state */

	catch_up_on_solo();
}

void
Session::playlist_length_changed (Playlist* pl)
{
	/* we can't just increase end_location->end() if pl->get_maximum_extent() 
	   if larger. if the playlist used to be the longest playlist,
	   and its now shorter, we have to decrease end_location->end(). hence,
	   we have to iterate over all diskstreams and check the 
	   playlists currently in use.
	*/
	find_current_end ();
}

void
Session::diskstream_playlist_changed (DiskStream* dstream)
{
	Playlist *playlist;

	if ((playlist = dstream->playlist()) != 0) {
	  playlist->LengthChanged.connect (sigc::bind (mem_fun (this, &Session::playlist_length_changed), playlist));
	}
	
	/* see comment in playlist_length_changed () */
	find_current_end ();
}

bool
Session::record_enabling_legal () const
{
	/* this used to be in here, but survey says.... we don't need to restrict it */
 	// if (record_status() == Recording) {
 	//	return false;
 	// }

	if (all_safe) {
		return false;
	}
	return true;
}

void
Session::set_auto_play (bool yn)
{
	if (auto_play != yn) {
		auto_play = yn; 
		set_dirty ();
		ControlChanged (AutoPlay);
	}
}

void
Session::set_auto_return (bool yn)
{
	if (auto_return != yn) {
		auto_return = yn; 
		set_dirty ();
		ControlChanged (AutoReturn);
	}
}

void
Session::set_crossfades_active (bool yn)
{
	if (crossfades_active != yn) {
		crossfades_active = yn; 
		set_dirty ();
		ControlChanged (CrossFadesActive);
	}
}

void
Session::set_do_not_record_plugins (bool yn)
{
	if (do_not_record_plugins != yn) {
		do_not_record_plugins = yn; 
		set_dirty ();
		ControlChanged (RecordingPlugins); 
	}
}

void
Session::set_auto_input (bool yn)
{
	if (auto_input != yn) {
		auto_input = yn;
		
		if (Config->get_use_hardware_monitoring() && transport_rolling()) {
			/* auto-input only makes a difference if we're rolling */

			/* Even though this can called from RT context we are using
			   a non-tentative rwlock here,  because the action must occur.
			   The rarity and short potential lock duration makes this "OK"
			*/
			RWLockMonitor dsm (diskstream_lock, false, __LINE__, __FILE__);
			for (DiskStreamList::iterator i = diskstreams.begin(); i != diskstreams.end(); ++i) {
				if ((*i)->record_enabled ()) {
					//cerr << "switching to input = " << !auto_input << __FILE__ << __LINE__ << endl << endl;
					(*i)->monitor_input (!auto_input);   
				}
			}
		}

		set_dirty();
		ControlChanged (AutoInput);
	}
}

void
Session::reset_input_monitor_state ()
{
	if (transport_rolling()) {
		RWLockMonitor dsm (diskstream_lock, false, __LINE__, __FILE__);
		for (DiskStreamList::iterator i = diskstreams.begin(); i != diskstreams.end(); ++i) {
			if ((*i)->record_enabled ()) {
				//cerr << "switching to input = " << !auto_input << __FILE__ << __LINE__ << endl << endl;
				(*i)->monitor_input (Config->get_use_hardware_monitoring() && !auto_input);
			}
		}
	} else {
		RWLockMonitor dsm (diskstream_lock, false, __LINE__, __FILE__);
		for (DiskStreamList::iterator i = diskstreams.begin(); i != diskstreams.end(); ++i) {
			if ((*i)->record_enabled ()) {
				//cerr << "switching to input = " << !auto_input << __FILE__ << __LINE__ << endl << endl;
				(*i)->monitor_input (Config->get_use_hardware_monitoring());
			}
		}
	}
}


void
Session::set_input_auto_connect (bool yn)
{
	if (yn) {
		input_auto_connect = AutoConnectOption (input_auto_connect|AutoConnectPhysical);
	} else {
		input_auto_connect = AutoConnectOption (input_auto_connect|~AutoConnectPhysical);
	}
	set_dirty ();
}

bool
Session::get_input_auto_connect () const
{
	return (input_auto_connect & AutoConnectPhysical);
}

void
Session::set_output_auto_connect (AutoConnectOption aco)
{
	output_auto_connect = aco;
	set_dirty ();
}

void
Session::auto_punch_start_changed (Location* location)
{
	replace_event (Event::PunchIn, location->start());

	if (get_record_enabled() && get_punch_in()) {
		/* capture start has been changed, so save new pending state */
		save_state ("", true);
	}
}	

void
Session::auto_punch_end_changed (Location* location)
{
	jack_nframes_t when_to_stop = location->end();
	// when_to_stop += _worst_output_latency + _worst_input_latency;
	replace_event (Event::PunchOut, when_to_stop);
}	

void
Session::auto_punch_changed (Location* location)
{
	jack_nframes_t when_to_stop = location->end();

	replace_event (Event::PunchIn, location->start());
	//when_to_stop += _worst_output_latency + _worst_input_latency;
	replace_event (Event::PunchOut, when_to_stop);
}	

void
Session::auto_loop_changed (Location* location)
{
	replace_event (Event::AutoLoop, location->end(), location->start());

	if (transport_rolling() && get_auto_loop()) {

		//if (_transport_frame < location->start() || _transport_frame > location->end()) {

		if (_transport_frame > location->end()) {
			// relocate to beginning of loop
			clear_events (Event::LocateRoll);
			
			request_locate (location->start(), true);

		}
		else if (seamless_loop && !loop_changing) {
			
			// schedule a locate-roll to refill the diskstreams at the
			// previous loop end
			loop_changing = true;

			if (location->end() > last_loopend) {
				clear_events (Event::LocateRoll);
				Event *ev = new Event (Event::LocateRoll, Event::Add, last_loopend, last_loopend, 0, true);
				queue_event (ev);
			}

		}
	}	

	last_loopend = location->end();
	
}

void
Session::set_auto_punch_location (Location* location)
{
	Location* existing;

	if ((existing = _locations.auto_punch_location()) != 0 && existing != location) {
		auto_punch_start_changed_connection.disconnect();
		auto_punch_end_changed_connection.disconnect();
		auto_punch_changed_connection.disconnect();
		existing->set_auto_punch (false, this);
		remove_event (existing->start(), Event::PunchIn);
		clear_events (Event::PunchOut);
		auto_punch_location_changed (0);
	}

	set_dirty();

	if (location == 0) {
		return;
	}
	
	if (location->end() <= location->start()) {
		error << _("Session: you can't use that location for auto punch (start <= end)") << endmsg;
		return;
	}

	auto_punch_start_changed_connection.disconnect();
	auto_punch_end_changed_connection.disconnect();
	auto_punch_changed_connection.disconnect();
		
	auto_punch_start_changed_connection = location->start_changed.connect (mem_fun (this, &Session::auto_punch_start_changed));
	auto_punch_end_changed_connection = location->end_changed.connect (mem_fun (this, &Session::auto_punch_end_changed));
	auto_punch_changed_connection = location->changed.connect (mem_fun (this, &Session::auto_punch_changed));

	location->set_auto_punch (true, this);
	auto_punch_location_changed (location);
}

void
Session::set_punch_in (bool yn)
{
	if (punch_in == yn) {
		return;
	}

	Location* location;

	if ((location = _locations.auto_punch_location()) != 0) {
		if ((punch_in = yn) == true) {
			replace_event (Event::PunchIn, location->start());
		} else {
			remove_event (location->start(), Event::PunchIn);
		}
	}

	set_dirty();
	ControlChanged (PunchIn); /* EMIT SIGNAL */
}

void
Session::set_punch_out (bool yn)
{
	if (punch_out == yn) {
		return;
	}

	Location* location;

	if ((location = _locations.auto_punch_location()) != 0) {
		if ((punch_out = yn) == true) {
			replace_event (Event::PunchOut, location->end());
		} else {
			clear_events (Event::PunchOut);
		}
	}

	set_dirty();
	ControlChanged (PunchOut); /* EMIT SIGNAL */
}

void
Session::set_auto_loop_location (Location* location)
{
	Location* existing;

	if ((existing = _locations.auto_loop_location()) != 0 && existing != location) {
		auto_loop_start_changed_connection.disconnect();
		auto_loop_end_changed_connection.disconnect();
		auto_loop_changed_connection.disconnect();
		existing->set_auto_loop (false, this);
		remove_event (existing->end(), Event::AutoLoop);
		auto_loop_location_changed (0);
	}
	
	set_dirty();

	if (location == 0) {
		return;
	}

	if (location->end() <= location->start()) {
		error << _("Session: you can't use a mark for auto loop") << endmsg;
		return;
	}

	last_loopend = location->end();
	
	auto_loop_start_changed_connection.disconnect();
	auto_loop_end_changed_connection.disconnect();
	auto_loop_changed_connection.disconnect();
	
	auto_loop_start_changed_connection = location->start_changed.connect (mem_fun (this, &Session::auto_loop_changed));
	auto_loop_end_changed_connection = location->end_changed.connect (mem_fun (this, &Session::auto_loop_changed));
	auto_loop_changed_connection = location->changed.connect (mem_fun (this, &Session::auto_loop_changed));

	location->set_auto_loop (true, this);
	auto_loop_location_changed (location);
}

void
Session::locations_added (Location* ignored)
{
	set_dirty ();
}

void
Session::locations_changed ()
{
	_locations.apply (*this, &Session::handle_locations_changed);
}

void
Session::handle_locations_changed (Locations::LocationList& locations)
{
	Locations::LocationList::iterator i;
	Location* location;
	bool set_loop = false;
	bool set_punch = false;

	for (i = locations.begin(); i != locations.end(); ++i) {

		location =* i;

		if (location->is_auto_punch()) {
			set_auto_punch_location (location);
			set_punch = true;
		}
		if (location->is_auto_loop()) {
			set_auto_loop_location (location);
			set_loop = true;
		}
		
	}

	if (!set_loop) {
		set_auto_loop_location (0);
	}
	if (!set_punch) {
		set_auto_punch_location (0);
	}

	set_dirty();
}						     

void
Session::enable_record ()
{
	/* XXX really atomic compare+swap here */
	if (atomic_read (&_record_status) != Recording) {
		atomic_set (&_record_status, Recording);
		_last_record_location = _transport_frame;
		send_mmc_in_another_thread (MIDI::MachineControl::cmdRecordStrobe);

		if (Config->get_use_hardware_monitoring() && auto_input) {
			/* Even though this can be called from RT context we are using
			   a non-tentative rwlock here,  because the action must occur.
			   The rarity and short potential lock duration makes this "OK"
			*/
			RWLockMonitor dsm (diskstream_lock, false, __LINE__, __FILE__);
			
			for (DiskStreamList::iterator i = diskstreams.begin(); i != diskstreams.end(); ++i) {
				if ((*i)->record_enabled ()) {
					//cerr << "switching to input" << __FILE__ << __LINE__ << endl << endl;
					(*i)->monitor_input (true);   
				}
			}
		}

		RecordEnabled ();
	}
}

void
Session::disable_record ()
{
	if (atomic_read (&_record_status) != Disabled) {
		atomic_set (&_record_status, Disabled);
		send_mmc_in_another_thread (MIDI::MachineControl::cmdRecordExit);

		if (Config->get_use_hardware_monitoring() && auto_input) {
			/* Even though this can be called from RT context we are using
			   a non-tentative rwlock here,  because the action must occur.
			   The rarity and short potential lock duration makes this "OK"
			*/
			RWLockMonitor dsm (diskstream_lock, false, __LINE__, __FILE__);
			
			for (DiskStreamList::iterator i = diskstreams.begin(); i != diskstreams.end(); ++i) {
				if ((*i)->record_enabled ()) {
					//cerr << "switching from input" << __FILE__ << __LINE__ << endl << endl;
					(*i)->monitor_input (false);   
				}
			}
		}
		
		RecordDisabled ();
		remove_pending_capture_state ();

	}
}

void
Session::step_back_from_record ()
{
	atomic_set (&_record_status, Enabled);

	if (Config->get_use_hardware_monitoring()) {
		/* Even though this can be called from RT context we are using
		   a non-tentative rwlock here,  because the action must occur.
		   The rarity and short potential lock duration makes this "OK"
		*/
		RWLockMonitor dsm (diskstream_lock, false, __LINE__, __FILE__);
		
		for (DiskStreamList::iterator i = diskstreams.begin(); i != diskstreams.end(); ++i) {
		        if (auto_input && (*i)->record_enabled ()) {
			        //cerr << "switching from input" << __FILE__ << __LINE__ << endl << endl;
		                (*i)->monitor_input (false);   
			}
		}
	}
}

void
Session::maybe_enable_record ()
{
	atomic_set (&_record_status, Enabled);

	save_state ("", true);

	if (_transport_speed) {
		if (!punch_in) {
			enable_record ();
		} 
	} else {
		send_mmc_in_another_thread (MIDI::MachineControl::cmdRecordPause);
		RecordEnabled (); /* EMIT SIGNAL */
	}

	set_dirty();
}

jack_nframes_t
Session::audible_frame () const
{
	jack_nframes_t ret;
	jack_nframes_t offset;
	jack_nframes_t tf;

	/* the first of these two possible settings for "offset"
	   mean that the audible frame is stationary until 
	   audio emerges from the latency compensation
	   "pseudo-pipeline".

	   the second means that the audible frame is stationary
	   until audio would emerge from a physical port
	   in the absence of any plugin latency compensation
	*/

	offset = _worst_output_latency;

	if (offset > current_block_size) {
		offset -= current_block_size;
	} else { 
		/* XXX is this correct? if we have no external
		   physical connections and everything is internal
		   then surely this is zero? still, how
		   likely is that anyway?
		*/
		offset = current_block_size;
	}

	if (synced_to_jack()) {
		tf = _engine.transport_frame();
	} else {
		tf = _transport_frame;
	}

	if (_transport_speed == 0) {
		return tf;
	}

	if (tf < offset) {
		return 0;
	}

	ret = tf;

	if (!non_realtime_work_pending()) {

		/* MOVING */

		/* take latency into account */
		
		ret -= offset;
	}

	return ret;
}

void
Session::set_frame_rate (jack_nframes_t frames_per_second)
{
	/** \fn void Session::set_frame_size(jack_nframes_t)
		the AudioEngine object that calls this guarantees 
		that it will not be called while we are also in
		::process(). Its fine to do things that block
		here.
	*/

	_current_frame_rate = frames_per_second;
	_frames_per_smpte_frame = (double) _current_frame_rate / (double) smpte_frames_per_second;

	Route::set_automation_interval ((jack_nframes_t) ceil ((double) frames_per_second * 0.25));

	set_dirty();

	/* XXX need to reset/reinstantiate all LADSPA plugins */
}

void
Session::set_block_size (jack_nframes_t nframes)
{
	/* the AudioEngine guarantees 
	   that it will not be called while we are also in
	   ::process(). It is therefore fine to do things that block
	   here.
	*/

	{ 
		RWLockMonitor lm (route_lock, false, __LINE__, __FILE__);
		RWLockMonitor dsm (diskstream_lock, false, __LINE__, __FILE__);
		vector<Sample*>::iterator i;
		uint32_t np;
			
		current_block_size = nframes;
		
		for (np = 0, i = _passthru_buffers.begin(); i != _passthru_buffers.end(); ++i, ++np) {
			free (*i);
		}

		for (vector<Sample*>::iterator i = _silent_buffers.begin(); i != _silent_buffers.end(); ++i) {
			free (*i);
		}

		_passthru_buffers.clear ();
		_silent_buffers.clear ();

		ensure_passthru_buffers (np);

		if (_gain_automation_buffer) {
			delete [] _gain_automation_buffer;
		}
		_gain_automation_buffer = new gain_t[nframes];

		allocate_pan_automation_buffers (nframes, _npan_buffers, true);

		for (RouteList::iterator i = routes.begin(); i != routes.end(); ++i) {
			(*i)->set_block_size (nframes);
		}
		
		for (DiskStreamList::iterator i = diskstreams.begin(); i != diskstreams.end(); ++i) {
			(*i)->set_block_size (nframes);
		}

		set_worst_io_latencies (false);
	}
}

void
Session::set_default_fade (float steepness, float fade_msecs)
{
#if 0
	jack_nframes_t fade_frames;
	
	/* Don't allow fade of less 1 frame */
	
	if (fade_msecs < (1000.0 * (1.0/_current_frame_rate))) {

		fade_msecs = 0;
		fade_frames = 0;

	} else {
		
		fade_frames = (jack_nframes_t) floor (fade_msecs * _current_frame_rate * 0.001);
		
	}

	default_fade_msecs = fade_msecs;
	default_fade_steepness = steepness;

	{
		// jlc, WTF is this!
		RWLockMonitor lm (route_lock, false, __LINE__, __FILE__);
		AudioRegion::set_default_fade (steepness, fade_frames);
	}

	set_dirty();

	/* XXX have to do this at some point */
	/* foreach region using default fade, reset, then 
	   refill_all_diskstream_buffers ();
	*/
#endif
}

struct RouteSorter {
    bool operator() (Route* r1, Route* r2) {
	    if (r1->fed_by.find (r2) != r1->fed_by.end()) {
		    return false;
	    } else if (r2->fed_by.find (r1) != r2->fed_by.end()) {
		    return true;
	    } else {
		    if (r1->fed_by.empty()) {
			    if (r2->fed_by.empty()) {
				    /* no ardour-based connections inbound to either route. just use signal order */
				    return r1->order_key(N_("signal")) < r2->order_key(N_("signal"));
			    } else {
				    /* r2 has connections, r1 does not; run r1 early */
				    return true;
			    }
		    } else {
			    return r1->order_key(N_("signal")) < r2->order_key(N_("signal"));
		    }
	    }
    }
};

static void
trace_terminal (Route* r1, Route* rbase)
{
	Route* r2;

	if ((r1->fed_by.find (rbase) != r1->fed_by.end()) && (rbase->fed_by.find (r1) != rbase->fed_by.end())) {
		info << string_compose(_("feedback loop setup between %1 and %2"), r1->name(), rbase->name()) << endmsg;
		return;
	} 

	/* make a copy of the existing list of routes that feed r1 */

	set<Route *> existing = r1->fed_by;

	/* for each route that feeds r1, recurse, marking it as feeding
	   rbase as well.
	*/

	for (set<Route *>::iterator i = existing.begin(); i != existing.end(); ++i) {
		r2 =* i;

		/* r2 is a route that feeds r1 which somehow feeds base. mark
		   base as being fed by r2
		*/

		rbase->fed_by.insert (r2);

		if (r2 != rbase) {

			/* 2nd level feedback loop detection. if r1 feeds or is fed by r2,
			   stop here.
			 */

			if ((r1->fed_by.find (r2) != r1->fed_by.end()) && (r2->fed_by.find (r1) != r2->fed_by.end())) {
				continue;
			}

			/* now recurse, so that we can mark base as being fed by
			   all routes that feed r2
			*/

			trace_terminal (r2, rbase);
		}

	}
}

void
Session::resort_routes (void* src)
{
	/* don't do anything here with signals emitted
	   by Routes while we are being destroyed.
	*/

	if (_state_of_the_state & Deletion) {
		return;
	}

	/* Caller MUST hold the route_lock */

	RouteList::iterator i, j;

	for (i = routes.begin(); i != routes.end(); ++i) {

		(*i)->fed_by.clear ();
		
		for (j = routes.begin(); j != routes.end(); ++j) {

			/* although routes can feed themselves, it will
			   cause an endless recursive descent if we
			   detect it. so don't bother checking for
			   self-feeding.
			*/

			if (*j == *i) {
				continue;
			}

			if ((*j)->feeds (*i)) {
				(*i)->fed_by.insert (*j);
			} 
		}
	}
	
	for (i = routes.begin(); i != routes.end(); ++i) {
		trace_terminal (*i, *i);
	}

	RouteSorter cmp;
	routes.sort (cmp);

#if 0
	cerr << "finished route resort\n";
	
	for (i = routes.begin(); i != routes.end(); ++i) {
		cerr << " " << (*i)->name() << " signal order = " << (*i)->order_key ("signal") << endl;
	}
	cerr << endl;
#endif

}

AudioTrack*
Session::new_audio_track (int input_channels, int output_channels)
{
	AudioTrack *track;
	char track_name[32];
	uint32_t n = 0;
	uint32_t channels_used = 0;
	string port;
	uint32_t nphysical_in;
	uint32_t nphysical_out;

	/* count existing audio tracks */

	{
		RWLockMonitor lm (route_lock, false, __LINE__, __FILE__);
		for (RouteList::iterator i = routes.begin(); i != routes.end(); ++i) {
			if (dynamic_cast<AudioTrack*>(*i) != 0) {
				if (!(*i)->hidden()) {
					n++;
					channels_used += (*i)->n_inputs();
				}
			}
		}
	}

	/* check for duplicate route names, since we might have pre-existing
	   routes with this name (e.g. create Audio1, Audio2, delete Audio1,
	   save, close,restart,add new route - first named route is now
	   Audio2)
	*/

	do {
		snprintf (track_name, sizeof(track_name), "Audio %" PRIu32, n+1);
		if (route_by_name (track_name) == 0) {
			break;
		}
		n++;

	} while (n < (UINT_MAX-1));

	if (input_auto_connect & AutoConnectPhysical) {
		nphysical_in = n_physical_inputs;
	} else {
		nphysical_in = 0;
	}

	if (output_auto_connect & AutoConnectPhysical) {
		nphysical_out = n_physical_outputs;
	} else {
		nphysical_out = 0;
	}

	try {
		track = new AudioTrack (*this, track_name);

		if (track->ensure_io (input_channels, output_channels, false, this)) {
			error << string_compose (_("cannot configure %1 in/%2 out configuration for new audio track"),
					  input_channels, output_channels)
			      << endmsg;
		}

		if (nphysical_in) {
			for (uint32_t x = 0; x < track->n_inputs() && x < nphysical_in; ++x) {
				
				port = "";
				
				if (input_auto_connect & AutoConnectPhysical) {
					port = _engine.get_nth_physical_input ((channels_used+x)%nphysical_in);
				} 
				
				if (port.length() && track->connect_input (track->input (x), port, this)) {
					break;
				}
			}
		}
		
		for (uint32_t x = 0; x < track->n_outputs(); ++x) {
			
			port = "";

			if (nphysical_out && (output_auto_connect & AutoConnectPhysical)) {
				port = _engine.get_nth_physical_output ((channels_used+x)%nphysical_out);
			} else if (output_auto_connect & AutoConnectMaster) {
				if (_master_out) {
					port = _master_out->input (x%_master_out->n_inputs())->name();
				}
			}

			if (port.length() && track->connect_output (track->output (x), port, this)) {
				break;
			}
		}

		if (_control_out) {
			vector<string> cports;
			uint32_t ni = _control_out->n_inputs();

			for (n = 0; n < ni; ++n) {
				cports.push_back (_control_out->input(n)->name());
			}

			track->set_control_outs (cports);
		}

		track->diskstream_changed.connect (mem_fun (this, &Session::resort_routes));

		add_route (track);

		track->set_remote_control_id (ntracks());
	}

	catch (failed_constructor &err) {
		error << _("Session: could not create new audio track.") << endmsg;
		return 0;
	}

	return track;
}

Route*
Session::new_audio_route (int input_channels, int output_channels)
{
	Route *bus;
	char bus_name[32];
	uint32_t n = 0;
	string port;

	/* count existing audio busses */

	{
		RWLockMonitor lm (route_lock, false, __LINE__, __FILE__);
		for (RouteList::iterator i = routes.begin(); i != routes.end(); ++i) {
			if (dynamic_cast<AudioTrack*>(*i) == 0) {
				if (!(*i)->hidden()) {
					n++;
				}
			}
		}
	}

	do {
		snprintf (bus_name, sizeof(bus_name), "Bus %" PRIu32, n+1);
		if (route_by_name (bus_name) == 0) {
			break;
		}
		n++;

	} while (n < (UINT_MAX-1));

	try {
		bus = new Route (*this, bus_name, -1, -1, -1, -1);

		if (bus->ensure_io (input_channels, output_channels, false, this)) {
			error << string_compose (_("cannot configure %1 in/%2 out configuration for new audio track"),
					  input_channels, output_channels)
			      << endmsg;
		}

		for (uint32_t x = 0; x < bus->n_inputs(); ++x) {
			
			port = "";

			if (input_auto_connect & AutoConnectPhysical) {
				port = _engine.get_nth_physical_input ((n+x)%n_physical_inputs);
			} 
			
			if (port.length() && bus->connect_input (bus->input (x), port, this)) {
				break;
			}
		}

		for (uint32_t x = 0; x < bus->n_outputs(); ++x) {
			
			port = "";

			if (output_auto_connect & AutoConnectPhysical) {
				port = _engine.get_nth_physical_input ((n+x)%n_physical_outputs);
			} else if (output_auto_connect & AutoConnectMaster) {
				if (_master_out) {
					port = _master_out->input (x%_master_out->n_inputs())->name();
				}
			}

			if (port.length() && bus->connect_output (bus->output (x), port, this)) {
				break;
			}
		}

		if (_control_out) {
			vector<string> cports;
			uint32_t ni = _control_out->n_inputs();

			for (uint32_t n = 0; n < ni; ++n) {
				cports.push_back (_control_out->input(n)->name());
			}
			bus->set_control_outs (cports);
		}
		
		add_route (bus);
	}

	catch (failed_constructor &err) {
		error << _("Session: could not create new route.") << endmsg;
		return 0;
	}

	return bus;
}

void
Session::add_route (Route* route)
{
	{ 
		RWLockMonitor lm (route_lock, true, __LINE__, __FILE__);
		routes.push_front (route);
		resort_routes(0);
	}

	route->solo_changed.connect (sigc::bind (mem_fun (*this, &Session::route_solo_changed), route));
	route->mute_changed.connect (mem_fun (*this, &Session::route_mute_changed));
	route->output_changed.connect (mem_fun (*this, &Session::set_worst_io_latencies_x));
	route->redirects_changed.connect (mem_fun (*this, &Session::update_latency_compensation_proxy));

	if (route->master()) {
		_master_out = route;
	}

	if (route->control()) {
		_control_out = route;
	}

	set_dirty();
	save_state (_current_snapshot_name);

	RouteAdded (route); /* EMIT SIGNAL */
}

void
Session::add_diskstream (DiskStream* dstream)
{
	/* need to do this in case we're rolling at the time, to prevent false underruns */
	dstream->do_refill(0, 0);
	
	{ 
		RWLockMonitor lm (diskstream_lock, true, __LINE__, __FILE__);
		diskstreams.push_back (dstream);
	}

	/* take a reference to the diskstream, preventing it from
	   ever being deleted until the session itself goes away,
	   or chooses to remove it for its own purposes.
	*/

	dstream->ref();
	dstream->set_block_size (current_block_size);

	dstream->PlaylistChanged.connect (sigc::bind (mem_fun (*this, &Session::diskstream_playlist_changed), dstream));
	/* this will connect to future changes, and check the current length */
	diskstream_playlist_changed (dstream);

	dstream->prepare ();

	set_dirty();
	save_state (_current_snapshot_name);

	DiskStreamAdded (dstream); /* EMIT SIGNAL */
}

void
Session::remove_route (Route& route)
{
	{ 	
		RWLockMonitor lm (route_lock, true, __LINE__, __FILE__);
		routes.remove (&route);
		
		/* deleting the master out seems like a dumb
		   idea, but its more of a UI policy issue
		   than our concern.
		*/

		if (&route == _master_out) {
			_master_out = 0;
		}

		if (&route == _control_out) {
			_control_out = 0;

			/* cancel control outs for all routes */

			vector<string> empty;

			for (RouteList::iterator r = routes.begin(); r != routes.end(); ++r) {
				(*r)->set_control_outs (empty);
			}
		}

		update_route_solo_state ();
	}

	{
		RWLockMonitor lm (diskstream_lock, true, __LINE__, __FILE__);

		AudioTrack* at;

		if ((at = dynamic_cast<AudioTrack*>(&route)) != 0) {
			diskstreams.remove (&at->disk_stream());
			at->disk_stream().unref ();
		}

		find_current_end ();
	}
	
	update_latency_compensation (false, false);
	set_dirty();
	
	/* XXX should we disconnect from the Route's signals ? */

	save_state (_current_snapshot_name);

	delete &route;
}	

void
Session::route_mute_changed (void* src)
{
	set_dirty ();
}

void
Session::route_solo_changed (void* src, Route* route)
{      
	if (solo_update_disabled) {
		// We know already
		return;
	}
	
	RWLockMonitor lm (route_lock, false, __LINE__, __FILE__);
	bool is_track;
	
	is_track = (dynamic_cast<AudioTrack*>(route) != 0);
	
	for (RouteList::iterator i = routes.begin(); i != routes.end(); ++i) {
		
		/* soloing a track mutes all other tracks, soloing a bus mutes all other busses */
		
		if (is_track) {
			
			/* don't mess with busses */
			
			if (dynamic_cast<AudioTrack*>(*i) == 0) {
				continue;
			}
			
		} else {
			
			/* don't mess with tracks */
			
			if (dynamic_cast<AudioTrack*>(*i) != 0) {
				continue;
			}
		}
		
		if ((*i) != route &&
		    ((*i)->mix_group () == 0 ||
		     (*i)->mix_group () != route->mix_group () ||
		     !route->mix_group ()->is_active())) {
			
			if ((*i)->soloed()) {
				
				/* if its already soloed, and solo latching is enabled,
				   then leave it as it is.
				*/
				
				if (_solo_latched) {
					continue;
				} 
			}
			
			/* do it */

			solo_update_disabled = true;
			(*i)->set_solo (false, src);
			solo_update_disabled = false;
		}
	}
	
	bool something_soloed = false;
	bool same_thing_soloed = false;
	bool signal = false;

        for (RouteList::iterator i = routes.begin(); i != routes.end(); ++i) {
		if ((*i)->soloed()) {
			something_soloed = true;
			if (dynamic_cast<AudioTrack*>(*i)) {
				if (is_track) {
					same_thing_soloed = true;
					break;
				}
			} else {
				if (!is_track) {
					same_thing_soloed = true;
					break;
				}
			}
			break;
		}
	}
	
	if (something_soloed != currently_soloing) {
		signal = true;
		currently_soloing = something_soloed;
	}
	
	modify_solo_mute (is_track, same_thing_soloed);

	if (signal) {
		SoloActive (currently_soloing);
	}

	set_dirty();
}

void
Session::set_solo_latched (bool yn)
{
	if (yn != _solo_latched) {
		_solo_latched = yn;
		set_dirty ();
		ControlChanged (SoloLatch);
	}
}

void
Session::update_route_solo_state ()
{
	bool mute = false;
	bool is_track = false;
	bool signal = false;

	/* caller must hold RouteLock */

	/* this is where we actually implement solo by changing
	   the solo mute setting of each track.
	*/
		
        for (RouteList::iterator i = routes.begin(); i != routes.end(); ++i) {
		if ((*i)->soloed()) {
			mute = true;
			if (dynamic_cast<AudioTrack*>(*i)) {
				is_track = true;
			}
			break;
		}
	}

	if (mute != currently_soloing) {
		signal = true;
		currently_soloing = mute;
	}

	if (!is_track && !mute) {

		/* nothing is soloed */

		for (RouteList::iterator i = routes.begin(); i != routes.end(); ++i) {
			(*i)->set_solo_mute (false);
		}
		
		if (signal) {
			SoloActive (false);
		}

		return;
	}

	modify_solo_mute (is_track, mute);

	if (signal) {
		SoloActive (currently_soloing);
	}
}

void
Session::modify_solo_mute (bool is_track, bool mute)
{
        for (RouteList::iterator i = routes.begin(); i != routes.end(); ++i) {
		
		if (is_track) {
			
			/* only alter track solo mute */
			
			if (dynamic_cast<AudioTrack*>(*i)) {
				if ((*i)->soloed()) {
					(*i)->set_solo_mute (!mute);
				} else {
					(*i)->set_solo_mute (mute);
				}
			}

		} else {

			/* only alter bus solo mute */

			if (!dynamic_cast<AudioTrack*>(*i)) {

				if ((*i)->soloed()) {

					(*i)->set_solo_mute (false);

				} else {

					/* don't mute master or control outs
					   in response to another bus solo
					*/
					
					if ((*i) != _master_out &&
					    (*i) != _control_out) {
						(*i)->set_solo_mute (mute);
					}
				}
			}

		}
	}
}	


void
Session::catch_up_on_solo ()
{
	/* this is called after set_state() to catch the full solo
	   state, which can't be correctly determined on a per-route
	   basis, but needs the global overview that only the session
	   has.
	*/
        RWLockMonitor lm (route_lock, false, __LINE__, __FILE__);
	update_route_solo_state();
}	
		
Route *
Session::route_by_name (string name)
{
	RWLockMonitor lm (route_lock, false, __LINE__, __FILE__);

	for (RouteList::iterator i = routes.begin(); i != routes.end(); ++i) {
		if ((*i)->name() == name) {
			return* i;
		}
	}

	return 0;
}

void
Session::find_current_end ()
{
	jack_nframes_t max = 0;
	jack_nframes_t me; 

	if (_state_of_the_state & Loading) {
		return;
	}

	/* Don't take the diskstream lock. Caller must have other ways to
	   ensure atomicity.
	*/

	for (DiskStreamList::iterator i = diskstreams.begin(); i != diskstreams.end(); ++i) {
		Playlist* pl = (*i)->playlist();
		if ((me = pl->get_maximum_extent()) > max) {
			max = me;
		}
	}
	
	if (max > end_location->end()) {
		end_location->set_end (max);
		set_dirty();
		DurationChanged(); /* EMIT SIGNAL */
	}
}

DiskStream *
Session::diskstream_by_name (string name)
{
	RWLockMonitor lm (diskstream_lock, false, __LINE__, __FILE__);

	for (DiskStreamList::iterator i = diskstreams.begin(); i != diskstreams.end(); ++i) {
		if ((*i)->name() == name) {
			return* i;
		}
	}

	return 0;
}

DiskStream *
Session::diskstream_by_id (id_t id)
{
	RWLockMonitor lm (diskstream_lock, false, __LINE__, __FILE__);

	for (DiskStreamList::iterator i = diskstreams.begin(); i != diskstreams.end(); ++i) {
		if ((*i)->id() == id) {
			return *i;
		}
	}

	return 0;
}

/* AudioRegion management */

string
Session::new_region_name (string old)
{
	string::size_type last_period;
	uint32_t number;
	string::size_type len = old.length() + 64;
	char buf[len];

	if ((last_period = old.find_last_of ('.')) == string::npos) {
		
		/* no period present - add one explicitly */

		old += '.';
		last_period = old.length() - 1;
		number = 0;

	} else {

		number = atoi (old.substr (last_period+1).c_str());

	}

	while (number < (UINT_MAX-1)) {

		AudioRegionList::const_iterator i;
		string sbuf;

		number++;

		snprintf (buf, len, "%s%" PRIu32, old.substr (0, last_period + 1).c_str(), number);
		sbuf = buf;

		for (i = audio_regions.begin(); i != audio_regions.end(); ++i) {
			if ((*i).second->name() == sbuf) {
				break;
			}
		}
		
		if (i == audio_regions.end()) {
			break;
		}
	}

	if (number != (UINT_MAX-1)) {
		return buf;
	} 

	error << string_compose (_("cannot create new name for region \"%1\""), old) << endmsg;
	return old;
}

int
Session::region_name (string& result, string base, bool newlevel) const
{
	char buf[16];
	string subbase;

	if (base == "") {
		
		LockMonitor lm (region_lock, __LINE__, __FILE__);

		snprintf (buf, sizeof (buf), "%d", (int)audio_regions.size() + 1);

		
		result = "region.";
		result += buf;

	} else {

		/* XXX this is going to be slow. optimize me later */
		
		if (newlevel) {
			subbase = base;
		} else {
			string::size_type pos;

			if ((pos = base.find_last_of ('-')) == string::npos) {
				pos = base.find_last_of ('.');
			}

			/* pos may be npos, but then we just use entire base */

			subbase = base.substr (0, pos);
		}
		
		bool name_taken = true;
		
		{
			LockMonitor lm (region_lock, __LINE__, __FILE__);
			
			for (int n = 1; n < 5000; ++n) {
				
				result = subbase;
				snprintf (buf, sizeof (buf), ".%d", n);
				result += buf;
				
				name_taken = false;
				
				for (AudioRegionList::const_iterator i = audio_regions.begin(); i != audio_regions.end(); ++i) {
					if ((*i).second->name() == result) {
						name_taken = true;
						break;
					}
				}
				
				if (!name_taken) {
					break;
				}
			}
		}
			
		if (name_taken) {
			fatal << string_compose(_("too many regions with names like %1"), base) << endmsg;
			/*NOTREACHED*/
		}
	}

	return 0;
}	

void
Session::add_region (Region* region)
{
	AudioRegion* ar = 0;
	AudioRegion* oar = 0;
	bool added = false;

	{ 
		LockMonitor lm (region_lock, __LINE__, __FILE__);

		if ((ar = dynamic_cast<AudioRegion*> (region)) != 0) {

			AudioRegionList::iterator x;

			for (x = audio_regions.begin(); x != audio_regions.end(); ++x) {

				oar = dynamic_cast<AudioRegion*> (x->second);

				if (ar->region_list_equivalent (*oar)) {
					break;
				}
			}

			if (x == audio_regions.end()) {

				pair<AudioRegionList::key_type, AudioRegionList::mapped_type> entry;
	
				entry.first = region->id();
				entry.second = ar;

				pair<AudioRegionList::iterator,bool> x = audio_regions.insert (entry);
				
				if (!x.second) {
					return;
				}

				added = true;
			} 

		} else {

			fatal << _("programming error: ")
			      << X_("unknown region type passed to Session::add_region()")
			      << endmsg;
			/*NOTREACHED*/

		}
	}

	/* mark dirty because something has changed even if we didn't
	   add the region to the region list.
	*/
	
	set_dirty();
	
	if (added) {
		region->GoingAway.connect (mem_fun (*this, &Session::remove_region));
		region->StateChanged.connect (sigc::bind (mem_fun (*this, &Session::region_changed), region));
		AudioRegionAdded (ar); /* EMIT SIGNAL */
	}
}

void
Session::region_changed (Change what_changed, Region* region)
{
	if (what_changed & Region::HiddenChanged) {
		/* relay hidden changes */
		RegionHiddenChange (region);
	}
}

void
Session::region_renamed (Region* region)
{
	add_region (region);
}

void
Session::remove_region (Region* region)
{
	AudioRegionList::iterator i;
	AudioRegion* ar = 0;
	bool removed = false;

	{ 
		LockMonitor lm (region_lock, __LINE__, __FILE__);

		if ((ar = dynamic_cast<AudioRegion*> (region)) != 0) {
			if ((i = audio_regions.find (region->id())) != audio_regions.end()) {
				audio_regions.erase (i);
				removed = true;
			} 
		} else {
			fatal << _("programming error: ") 
			      << X_("unknown region type passed to Session::remove_region()")
			      << endmsg;
			/*NOTREACHED*/
		}
	}

	/* mark dirty because something has changed even if we didn't
	   remove the region from the region list.
	*/

	set_dirty();

	if (removed) {
		 AudioRegionRemoved(ar); /* EMIT SIGNAL */
	}
}

AudioRegion*
Session::find_whole_file_parent (AudioRegion& child)
{
	AudioRegionList::iterator i;
	AudioRegion* region;
	LockMonitor lm (region_lock, __LINE__, __FILE__);

	for (i = audio_regions.begin(); i != audio_regions.end(); ++i) {

		region = (*i).second;

		if (region->whole_file()) {

			if (child.source_equivalent (*region)) {
				return region;
			}
		}
	} 

	return 0;
}	

void
Session::find_equivalent_playlist_regions (AudioRegion& region, vector<AudioRegion*>& result)
{
	for (PlaylistList::iterator i = playlists.begin(); i != playlists.end(); ++i) {

		AudioPlaylist* pl;

		if ((pl = dynamic_cast<AudioPlaylist*>(*i)) == 0) {
			continue;
		}

		pl->get_region_list_equivalent_regions (region, result);
	}
}

int
Session::destroy_region (Region* region)
{
	AudioRegion* aregion;

	if ((aregion = dynamic_cast<AudioRegion*> (region)) == 0) {
		return 0;
	}

	if (aregion->playlist()) {
		aregion->playlist()->destroy_region (region);
	}

	vector<Source*> srcs;
	
	for (uint32_t n = 0; n < aregion->n_channels(); ++n) {
		srcs.push_back (&aregion->source (n));
	}

	for (vector<Source*>::iterator i = srcs.begin(); i != srcs.end(); ++i) {
		
		if ((*i)->use_cnt() == 0) {
			(*i)->mark_for_remove ();
			delete *i;
		}
	}

	return 0;
}

int
Session::destroy_regions (list<Region*> regions)
{
	for (list<Region*>::iterator i = regions.begin(); i != regions.end(); ++i) {
		destroy_region (*i);
	}
	return 0;
}

int
Session::remove_last_capture ()
{
	list<Region*> r;

	RWLockMonitor lm (diskstream_lock, false, __LINE__, __FILE__);
	
	for (DiskStreamList::iterator i = diskstreams.begin(); i != diskstreams.end(); ++i) {
		list<Region*>& l = (*i)->last_capture_regions();
		
		if (!l.empty()) {
			r.insert (r.end(), l.begin(), l.end());
			l.clear ();
		}
	}

	destroy_regions (r);
	return 0;
}

int
Session::remove_region_from_region_list (Region& r)
{
	remove_region (&r);
	return 0;
}

/* Source Management */

void
Session::add_source (Source* source)
{
	pair<SourceList::key_type, SourceList::mapped_type> entry;
	
	{
		LockMonitor lm (source_lock, __LINE__, __FILE__);
		entry.first = source->id();
		entry.second = source;
		sources.insert (entry);
	}
	
	source->GoingAway.connect (mem_fun (this, &Session::remove_source));
	set_dirty();
	
	SourceAdded (source); /* EMIT SIGNAL */
}

void
Session::remove_source (Source* source)
{
	SourceList::iterator i;

	{ 
		LockMonitor lm (source_lock, __LINE__, __FILE__);

		if ((i = sources.find (source->id())) != sources.end()) {
			sources.erase (i);
		}
	}

	if (!_state_of_the_state & InCleanup) {

		/* save state so we don't end up with a session file
		   referring to non-existent sources.
		*/
		
		save_state (_current_snapshot_name);
	}

	SourceRemoved(source); /* EMIT SIGNAL */
}

Source *
Session::get_source (ARDOUR::id_t id)
{
	LockMonitor lm (source_lock, __LINE__, __FILE__);
	SourceList::iterator i;
	Source* source = 0;

	if ((i = sources.find (id)) != sources.end()) {
		source = (*i).second;
	}

	return source;
}

FileSource *
Session::create_file_source (DiskStream& ds, int32_t chan, bool destructive)
{
	string spath;
	uint32_t cnt;
	char buf[PATH_MAX+1];
	const uint32_t limit = 10000;
	string legalized;

	buf[0] = '\0';
	legalized = legalize_for_path (ds.name());

	/* find a "version" of the file name that doesn't exist in
	   any of the possible directories.
	*/
	
	for (cnt = 1; cnt <= limit; ++cnt) {

		vector<space_and_path>::iterator i;
		uint32_t existing = 0;

		for (i = session_dirs.begin(); i != session_dirs.end(); ++i) {

			spath = (*i).path;
			spath += sound_dir_name;
			spath += '/';
			spath += legalized;

			if (ds.n_channels() < 2) {
				snprintf (buf, sizeof(buf), "%s-%u.wav", spath.c_str(), cnt);
			} else if (ds.n_channels() == 2) {
				if (chan == 0) {
					snprintf (buf, sizeof(buf), "%s-%u%%L.wav", spath.c_str(), cnt);
				} else {
					snprintf (buf, sizeof(buf), "%s-%u%%R.wav", spath.c_str(), cnt);
				}
			} else if (ds.n_channels() < 26) {
				snprintf (buf, sizeof(buf), "%s-%u%%%c.wav", spath.c_str(), cnt, 'a' + chan);
			} else {
				snprintf (buf, sizeof(buf), "%s-%u.wav", spath.c_str(), cnt);
			}

			if (access (buf, F_OK) == 0) {
				existing++;
			}
		}

		if (existing == 0) {
			break;
		}
	}

	if (cnt > limit) {
		error << string_compose(_("There are already %1 recordings for %2, which I consider too many."), limit, ds.name()) << endmsg;
		throw failed_constructor();
	}

	/* we now have a unique name for the file, but figure out where to
	   actually put it.
	*/

	string foo = buf;

	spath = discover_best_sound_dir ();

	string::size_type pos = foo.find_last_of ('/');
	
	if (pos == string::npos) {
		spath += foo;
	} else {
		spath += foo.substr (pos + 1);
	}

	/* this might throw failed_constructor(), which is OK */

	if (destructive) {
		return new DestructiveFileSource (spath, frame_rate());
	} else {
		return new FileSource (spath, frame_rate());
	}
}

/* Playlist management */

Playlist *
Session::get_playlist (string name)
{
	Playlist* ret = 0;

	if ((ret = playlist_by_name (name)) == 0) {
		ret = new AudioPlaylist (*this, name);
	}

	return ret;
}

Playlist *
Session::playlist_by_name (string name)
{
	LockMonitor lm (playlist_lock, __LINE__, __FILE__);
	for (PlaylistList::iterator i = playlists.begin(); i != playlists.end(); ++i) {
		if ((*i)->name() == name) {
			return* i;
		}
	}
	for (PlaylistList::iterator i = unused_playlists.begin(); i != unused_playlists.end(); ++i) {
		if ((*i)->name() == name) {
			return* i;
		}
	}
	return 0;
}

void
Session::add_playlist (Playlist* playlist)
{
	if (playlist->hidden()) {
		return;
	}

	{ 
		LockMonitor lm (playlist_lock, __LINE__, __FILE__);
		if (find (playlists.begin(), playlists.end(), playlist) == playlists.end()) {
			playlists.insert (playlists.begin(), playlist);
			// playlist->ref();
			playlist->InUse.connect (mem_fun (*this, &Session::track_playlist));
			playlist->GoingAway.connect (mem_fun (*this, &Session::remove_playlist));
		}
	}

	set_dirty();

	PlaylistAdded (playlist); /* EMIT SIGNAL */
}

void
Session::track_playlist (Playlist* pl, bool inuse)
{
	PlaylistList::iterator x;

	{ 
		LockMonitor lm (playlist_lock, __LINE__, __FILE__);

		if (!inuse) {
			//cerr << "shifting playlist to unused: " << pl->name() << endl;

			unused_playlists.insert (pl);
			
			if ((x = playlists.find (pl)) != playlists.end()) {
				playlists.erase (x);
			}

			
		} else {
			//cerr << "shifting playlist to used: " << pl->name() << endl;
			
			playlists.insert (pl);
			
			if ((x = unused_playlists.find (pl)) != unused_playlists.end()) {
				unused_playlists.erase (x);
			}
		}
	}
}

void
Session::remove_playlist (Playlist* playlist)
{
	if (_state_of_the_state & Deletion) {
		return;
	}

	{ 
		LockMonitor lm (playlist_lock, __LINE__, __FILE__);
		// cerr << "removing playlist: " << playlist->name() << endl;

		PlaylistList::iterator i;

		i = find (playlists.begin(), playlists.end(), playlist);

		if (i != playlists.end()) {
			playlists.erase (i);
		}

		i = find (unused_playlists.begin(), unused_playlists.end(), playlist);
		if (i != unused_playlists.end()) {
			unused_playlists.erase (i);
		}
		
	}

	set_dirty();

	PlaylistRemoved (playlist); /* EMIT SIGNAL */
}

void 
Session::set_audition (AudioRegion* r)
{
	pending_audition_region = r;
	post_transport_work = PostTransportWork (post_transport_work | PostTransportAudition);
	schedule_butler_transport_work ();
}

void
Session::non_realtime_set_audition ()
{
	if (pending_audition_region == (AudioRegion*) 0xfeedface) {
		auditioner->audition_current_playlist ();
	} else if (pending_audition_region) {
		auditioner->audition_region (*pending_audition_region);
	}
	pending_audition_region = 0;
	AuditionActive (true); /* EMIT SIGNAL */
}

void
Session::audition_playlist ()
{
	Event* ev = new Event (Event::Audition, Event::Add, Event::Immediate, 0, 0.0);
	ev->set_ptr ((void*) 0xfeedface);
	queue_event (ev);
}

void
Session::audition_region (AudioRegion& r)
{
	Event* ev = new Event (Event::Audition, Event::Add, Event::Immediate, 0, 0.0);
	ev->set_ptr (&r);
	queue_event (ev);
}

void
Session::cancel_audition ()
{
	if (auditioner->active()) {
		auditioner->cancel_audition ();
		 AuditionActive (false); /* EMIT SIGNAL */
	}
}

bool
Session::RoutePublicOrderSorter::operator() (Route* a, Route* b)
{
	return a->order_key(N_("signal")) < b->order_key(N_("signal"));
}

void
Session::remove_empty_sounds ()
{

	PathScanner scanner;
	string dir;

	dir = sound_dir ();

	vector<string *>* possible_audiofiles = scanner (dir, "\\.wav$", false, true);
	
	for (vector<string *>::iterator i = possible_audiofiles->begin(); i != possible_audiofiles->end(); ++i) {

		if (FileSource::is_empty (*(*i))) {

			unlink ((*i)->c_str());
			
			string peak_path = peak_path_from_audio_path (**i);
			unlink (peak_path.c_str());
		}

		delete* i;
	}

	delete possible_audiofiles;
}

bool
Session::is_auditioning () const
{
	/* can be called before we have an auditioner object */
	if (auditioner) {
		return auditioner->active();
	} else {
		return false;
	}
}


string
Session::peak_path_from_audio_path (string audio_path)
{
	/* XXX hardly bombproof! fix me */

	string res;

	res = PBD::dirname (audio_path);
	res = PBD::dirname (res);
	res += '/';
	res += peak_dir_name;
	res += '/';
	res += PBD::basename_nosuffix (audio_path);
	res += ".peak";

	return res;
}

string
Session::old_peak_path_from_audio_path (string audio_path)
{
	/* This is a hangover from when audio and peak files
	   lived in the same directory. We need it to to 
	   be able to open old sessions.
	*/

	/* XXX hardly bombproof! fix me */

 	string res = audio_path.substr (0, audio_path.find_last_of ('.'));
  	res += ".peak";
  	return res;
}

void
Session::set_all_solo (bool yn)
{
	{
		RWLockMonitor lm (route_lock, false, __LINE__, __FILE__);
		
		for (RouteList::iterator i = routes.begin(); i != routes.end(); ++i) {
			if (!(*i)->hidden()) {
				(*i)->set_solo (yn, this);
			}
		}
	}

	set_dirty();
}
		
void
Session::set_all_mute (bool yn)
{
	{
		RWLockMonitor lm (route_lock, false, __LINE__, __FILE__);
		
		for (RouteList::iterator i = routes.begin(); i != routes.end(); ++i) {
			if (!(*i)->hidden()) {
				(*i)->set_mute (yn, this);
			}
		}
	}

	set_dirty();
}
		
uint32_t
Session::n_diskstreams () const
{
	RWLockMonitor lm (diskstream_lock, false, __LINE__, __FILE__);
	uint32_t n = 0;

	for (DiskStreamList::const_iterator i = diskstreams.begin(); i != diskstreams.end(); ++i) {
		if (!(*i)->hidden()) {
			n++;
		}
	}
	return n;
}

void 
Session::foreach_diskstream (void (DiskStream::*func)(void)) 
{
	RWLockMonitor lm (diskstream_lock, false, __LINE__, __FILE__);
	for (DiskStreamList::iterator i = diskstreams.begin(); i != diskstreams.end(); ++i) {
		if (!(*i)->hidden()) {
			((*i)->*func)();
		}
	}
}

void
Session::graph_reordered ()
{
	/* don't do this stuff if we are setting up connections
	   from a set_state() call.
	*/

	if (_state_of_the_state & InitialConnecting) {
		return;
	}

	RWLockMonitor lm1 (route_lock, true, __LINE__, __FILE__);
	RWLockMonitor lm2 (diskstream_lock, false, __LINE__, __FILE__);

	resort_routes (0);

	/* force all diskstreams to update their capture offset values to 
	   reflect any changes in latencies within the graph.
	*/
	
	for (DiskStreamList::iterator i = diskstreams.begin(); i != diskstreams.end(); ++i) {
		(*i)->set_capture_offset ();
	}
}

void
Session::record_disenable_all ()
{
	record_enable_change_all (false);
}

void
Session::record_enable_all ()
{
	record_enable_change_all (true);
}

void
Session::record_enable_change_all (bool yn)
{
	RWLockMonitor lm1 (route_lock, false, __LINE__, __FILE__);
	
	for (RouteList::iterator i = routes.begin(); i != routes.end(); ++i) {
		AudioTrack* at;

		if ((at = dynamic_cast<AudioTrack*>(*i)) != 0) {
			at->set_record_enable (yn, this);
		}
	}
	
	/* since we don't keep rec-enable state, don't mark session dirty */
}

void
Session::add_redirect (Redirect* redirect)
{
	Send* send;
	Insert* insert;
	PortInsert* port_insert;
	PluginInsert* plugin_insert;

	if ((insert = dynamic_cast<Insert *> (redirect)) != 0) {
		if ((port_insert = dynamic_cast<PortInsert *> (insert)) != 0) {
			_port_inserts.insert (_port_inserts.begin(), port_insert);
		} else if ((plugin_insert = dynamic_cast<PluginInsert *> (insert)) != 0) {
			_plugin_inserts.insert (_plugin_inserts.begin(), plugin_insert);
		} else {
			fatal << _("programming error: unknown type of Insert created!") << endmsg;
			/*NOTREACHED*/
		}
	} else if ((send = dynamic_cast<Send *> (redirect)) != 0) {
		_sends.insert (_sends.begin(), send);
	} else {
		fatal << _("programming error: unknown type of Redirect created!") << endmsg;
		/*NOTREACHED*/
	}

	redirect->GoingAway.connect (mem_fun (*this, &Session::remove_redirect));

	set_dirty();
}

void
Session::remove_redirect (Redirect* redirect)
{
	Send* send;
	Insert* insert;
	PortInsert* port_insert;
	PluginInsert* plugin_insert;

	if ((insert = dynamic_cast<Insert *> (redirect)) != 0) {
		if ((port_insert = dynamic_cast<PortInsert *> (insert)) != 0) {
			_port_inserts.remove (port_insert);
		} else if ((plugin_insert = dynamic_cast<PluginInsert *> (insert)) != 0) {
			_plugin_inserts.remove (plugin_insert);
		} else {
			fatal << _("programming error: unknown type of Insert deleted!") << endmsg;
			/*NOTREACHED*/
		}
	} else if ((send = dynamic_cast<Send *> (redirect)) != 0) {
		_sends.remove (send);
	} else {
		fatal << _("programming error: unknown type of Redirect deleted!") << endmsg;
		/*NOTREACHED*/
	}

	set_dirty();
}

jack_nframes_t
Session::available_capture_duration ()
{
	const double scale = 4096.0 / sizeof (Sample);
	
	if (_total_free_4k_blocks * scale > (double) max_frames) {
		return max_frames;
	}
	
	return (jack_nframes_t) floor (_total_free_4k_blocks * scale);
}

void
Session::add_connection (ARDOUR::Connection* connection)
{
	{
		LockMonitor (connection_lock, __LINE__, __FILE__);
		_connections.push_back (connection);
	}
	
	ConnectionAdded (connection); /* EMIT SIGNAL */

	set_dirty();
}

void
Session::remove_connection (ARDOUR::Connection* connection)
{
	bool removed = false;

	{
		LockMonitor (connection_lock, __LINE__, __FILE__);
		ConnectionList::iterator i = find (_connections.begin(), _connections.end(), connection);
		
		if (i != _connections.end()) {
			_connections.erase (i);
			removed = true;
		}
	}

	if (removed) {
		 ConnectionRemoved (connection); /* EMIT SIGNAL */
	}

	set_dirty();
}

ARDOUR::Connection *
Session::connection_by_name (string name) const
{
	LockMonitor lm (connection_lock, __LINE__, __FILE__);

	for (ConnectionList::const_iterator i = _connections.begin(); i != _connections.end(); ++i) {
		if ((*i)->name() == name) {
			return* i;
		}
	}

	return 0;
}

void
Session::set_edit_mode (EditMode mode)
{
	_edit_mode = mode;
	
	{ 
		LockMonitor lm (playlist_lock, __LINE__, __FILE__);
		
		for (PlaylistList::iterator i = playlists.begin(); i != playlists.end(); ++i) {
			(*i)->set_edit_mode (mode);
		}
	}

	set_dirty ();
	ControlChanged (EditingMode); /* EMIT SIGNAL */
}

void
Session::tempo_map_changed (Change ignored)
{
	clear_clicks ();
	set_dirty ();
}

void
Session::ensure_passthru_buffers (uint32_t howmany)
{
	while (howmany > _passthru_buffers.size()) {
		Sample *p;
#ifdef NO_POSIX_MEMALIGN
		p =  (Sample *) malloc(current_block_size * sizeof(Sample));
#else
		posix_memalign((void **)&p,16,current_block_size * 4);
#endif			
		_passthru_buffers.push_back (p);

		*p = 0;
		
#ifdef NO_POSIX_MEMALIGN
		p =  (Sample *) malloc(current_block_size * sizeof(Sample));
#else
		posix_memalign((void **)&p,16,current_block_size * 4);
#endif			
		memset (p, 0, sizeof (Sample) * current_block_size);
		_silent_buffers.push_back (p);

	}
	allocate_pan_automation_buffers (current_block_size, howmany, false);
}

string
Session::next_send_name ()
{
	char buf[32];
	snprintf (buf, sizeof (buf), "send %" PRIu32, ++send_cnt);
	return buf;
}

string
Session::next_insert_name ()
{
	char buf[32];
	snprintf (buf, sizeof (buf), "insert %" PRIu32, ++insert_cnt);
	return buf;
}

/* Named Selection management */

NamedSelection *
Session::named_selection_by_name (string name)
{
	LockMonitor lm (named_selection_lock, __LINE__, __FILE__);
	for (NamedSelectionList::iterator i = named_selections.begin(); i != named_selections.end(); ++i) {
		if ((*i)->name == name) {
			return* i;
		}
	}
	return 0;
}

void
Session::add_named_selection (NamedSelection* named_selection)
{
	{ 
		LockMonitor lm (named_selection_lock, __LINE__, __FILE__);
		named_selections.insert (named_selections.begin(), named_selection);
	}

	set_dirty();

	 NamedSelectionAdded (); /* EMIT SIGNAL */
}

void
Session::remove_named_selection (NamedSelection* named_selection)
{
	bool removed = false;

	{ 
		LockMonitor lm (named_selection_lock, __LINE__, __FILE__);

		NamedSelectionList::iterator i = find (named_selections.begin(), named_selections.end(), named_selection);

		if (i != named_selections.end()) {
			delete (*i);
			named_selections.erase (i);
			set_dirty();
			removed = true;
		}
	}

	if (removed) {
		 NamedSelectionRemoved (); /* EMIT SIGNAL */
	}
}

void
Session::reset_native_file_format ()
{
	// jlc - WHY take routelock?
	//RWLockMonitor lm1 (route_lock, true, __LINE__, __FILE__);
	RWLockMonitor lm2 (diskstream_lock, false, __LINE__, __FILE__);

	for (DiskStreamList::iterator i = diskstreams.begin(); i != diskstreams.end(); ++i) {
		(*i)->reset_write_sources (false);
	}
}

bool
Session::route_name_unique (string n) const
{
	RWLockMonitor lm (route_lock, false, __LINE__, __FILE__);
	
	for (RouteList::const_iterator i = routes.begin(); i != routes.end(); ++i) {
		if ((*i)->name() == n) {
			return false;
		}
	}
	
	return true;
}

int
Session::remove_file_source (FileSource& fs)
{
	return fs.move_to_trash (dead_sound_dir_name);
}

uint32_t
Session::n_playlists () const
{
	LockMonitor lm (playlist_lock, __LINE__, __FILE__);
	return playlists.size();
}

void
Session::set_solo_model (SoloModel sm)
{
	if (sm != _solo_model) {
		_solo_model = sm;
		ControlChanged (SoloingModel);
		set_dirty ();
	}
}

void
Session::allocate_pan_automation_buffers (jack_nframes_t nframes, uint32_t howmany, bool force)
{
	if (!force && howmany <= _npan_buffers) {
		return;
	}

	if (_pan_automation_buffer) {

		for (uint32_t i = 0; i < _npan_buffers; ++i) {
			delete [] _pan_automation_buffer[i];
		}

		delete [] _pan_automation_buffer;
	}

	_pan_automation_buffer = new pan_t*[howmany];
	
	for (uint32_t i = 0; i < howmany; ++i) {
		_pan_automation_buffer[i] = new pan_t[nframes];
	}

	_npan_buffers = howmany;
}

void 
Session::add_instant_xml (XMLNode& node, const std::string& dir)
{
	Stateful::add_instant_xml (node, dir);
	Config->add_instant_xml (node, Config->get_user_ardour_path());
}

int
Session::freeze (InterThreadInfo& itt)
{
	RWLockMonitor lm (route_lock, false, __LINE__, __FILE__);

	for (RouteList::iterator i = routes.begin(); i != routes.end(); ++i) {

		AudioTrack *at;

		if ((at = dynamic_cast<AudioTrack*>(*i)) != 0) {
			/* XXX this is wrong because itt.progress will keep returning to zero at the start
			   of every track.
			*/
			at->freeze (itt);
		}
	}

	return 0;
}

int
Session::write_one_track (AudioTrack& track, jack_nframes_t start, jack_nframes_t len, 	bool overwrite, vector<Source*>& srcs,
			  InterThreadInfo& itt)
{
	int ret = -1;
	Playlist* playlist;
	FileSource* fsource;
	uint32_t x;
	char buf[PATH_MAX+1];
	string dir;
	uint32_t nchans;
	jack_nframes_t position;
	jack_nframes_t this_chunk;
	jack_nframes_t to_do;
	vector<Sample*> buffers;
	const jack_nframes_t chunk_size = (256 * 1024)/4;

	atomic_set (&processing_prohibited, 1);
	
	/* call tree *MUST* hold route_lock */
	
	if ((playlist = track.disk_stream().playlist()) == 0) {
		goto out;
	}

	/* external redirects will be a problem */

	if (track.has_external_redirects()) {
		goto out;
	}

	nchans = track.disk_stream().n_channels();
	
	dir = discover_best_sound_dir ();

	for (uint32_t chan_n=0; chan_n < nchans; ++chan_n) {

		for (x = 0; x < 99999; ++x) {
			snprintf (buf, sizeof(buf), "%s/%s-%d-bounce-%" PRIu32 ".wav", dir.c_str(), playlist->name().c_str(), chan_n, x+1);
			if (access (buf, F_OK) != 0) {
				break;
			}
		}
		
		if (x == 99999) {
			error << string_compose (_("too many bounced versions of playlist \"%1\""), playlist->name()) << endmsg;
			goto out;
		}
		
		try {
			fsource = new FileSource (buf, frame_rate());
		}
		
		catch (failed_constructor& err) {
			error << string_compose (_("cannot create new audio file \"%1\" for %2"), buf, track.name()) << endmsg;
			goto out;
		}

		srcs.push_back(fsource);
	}

	/* XXX need to flush all redirects */
	
	position = start;
	to_do = len;

	/* create a set of reasonably-sized buffers */

	for (vector<Sample*>::iterator i = _passthru_buffers.begin(); i != _passthru_buffers.end(); ++i) {
		Sample* b;
#ifdef NO_POSIX_MEMALIGN
		b =  (Sample *) malloc(chunk_size * sizeof(Sample));
#else
		posix_memalign((void **)&b,16,chunk_size * 4);
#endif			
		buffers.push_back (b);
	}
	
	while (to_do && !itt.cancel) {
		
		this_chunk = min (to_do, chunk_size);
		
		if (track.export_stuff (buffers, nchans, start, this_chunk)) {
			goto out;
		}

		uint32_t n = 0;
		for (vector<Source*>::iterator src=srcs.begin(); src != srcs.end(); ++src, ++n) {
			if ((*src)->write (buffers[n], this_chunk) != this_chunk) {
				goto out;
			}
		}
		
		start += this_chunk;
		to_do -= this_chunk;
		
		itt.progress = (float) (1.0 - ((double) to_do / len));

	}

	if (!itt.cancel) {
		
		time_t now;
		struct tm* xnow;
		time (&now);
		xnow = localtime (&now);
		
		for (vector<Source*>::iterator src=srcs.begin(); src != srcs.end(); ++src) {
			dynamic_cast<FileSource*>((*src))->update_header (position, *xnow, now);
		}
		
		/* build peakfile for new source */
		
		for (vector<Source*>::iterator src=srcs.begin(); src != srcs.end(); ++src) {
			dynamic_cast<FileSource*>(*src)->build_peaks ();
		}
		
		ret = 0;
	}
		
  out:
	if (ret) {
		for (vector<Source*>::iterator src=srcs.begin(); src != srcs.end(); ++src) {
			dynamic_cast<FileSource*>(*src)->mark_for_remove ();
			delete *src;
		}
	}

	for (vector<Sample*>::iterator i = buffers.begin(); i != buffers.end(); ++i) {
		free(*i);
	}

	atomic_set (&processing_prohibited, 0);

	itt.done = true;

	return ret;
}

vector<Sample*>&
Session::get_silent_buffers (uint32_t howmany)
{
	for (uint32_t i = 0; i < howmany; ++i) {
		memset (_silent_buffers[i], 0, sizeof (Sample) * current_block_size);
	}
	return _silent_buffers;
}

uint32_t 
Session::ntracks () const
{
	uint32_t n = 0;
	RWLockMonitor lm (route_lock, false, __LINE__, __FILE__);

	for (RouteList::const_iterator i = routes.begin(); i != routes.end(); ++i) {
		if (dynamic_cast<AudioTrack*> (*i)) {
			++n;
		}
	}

	return n;
}

uint32_t 
Session::nbusses () const
{
	uint32_t n = 0;
	RWLockMonitor lm (route_lock, false, __LINE__, __FILE__);

	for (RouteList::const_iterator i = routes.begin(); i != routes.end(); ++i) {
		if (dynamic_cast<AudioTrack*> (*i) == 0) {
			++n;
		}
	}

	return n;
}

void
Session::set_layer_model (LayerModel lm)
{
	if (lm != layer_model) {
		layer_model = lm;
		set_dirty ();
		ControlChanged (LayeringModel);
	}
}

void
Session::set_xfade_model (CrossfadeModel xm)
{
	if (xm != xfade_model) {
		xfade_model = xm;
		set_dirty ();
		ControlChanged (CrossfadingModel);
	}
}

