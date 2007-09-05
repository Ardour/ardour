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

#include <glibmm/thread.h>
#include <glibmm/miscutils.h>

#include <pbd/error.h>
#include <glibmm/thread.h>
#include <pbd/pathscanner.h>
#include <pbd/stl_delete.h>
#include <pbd/basename.h>
#include <pbd/stacktrace.h>

#include <ardour/audioengine.h>
#include <ardour/configuration.h>
#include <ardour/session.h>
#include <ardour/audio_diskstream.h>
#include <ardour/utils.h>
#include <ardour/audioplaylist.h>
#include <ardour/audioregion.h>
#include <ardour/audiofilesource.h>
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
#include <ardour/data_type.h>
#include <ardour/source_factory.h>
#include <ardour/region_factory.h>

#ifdef HAVE_LIBLO
#include <ardour/osc.h>
#endif

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using boost::shared_ptr;

#ifdef __x86_64__
static const int CPU_CACHE_ALIGN = 64;
#else
static const int CPU_CACHE_ALIGN = 16; /* arguably 32 on most arches, but it matters less */
#endif

const char* Session::_template_suffix = X_(".template");
const char* Session::_statefile_suffix = X_(".ardour");
const char* Session::_pending_suffix = X_(".pending");
const char* Session::old_sound_dir_name = X_("sounds");
const char* Session::sound_dir_name = X_("audiofiles");
const char* Session::peak_dir_name = X_("peaks");
const char* Session::dead_sound_dir_name = X_("dead_sounds");
const char* Session::interchange_dir_name = X_("interchange");
const char* Session::export_dir_name = X_("export");

Session::compute_peak_t			Session::compute_peak 		= 0;
Session::find_peaks_t			Session::find_peaks 		= 0;
Session::apply_gain_to_buffer_t		Session::apply_gain_to_buffer 	= 0;
Session::mix_buffers_with_gain_t	Session::mix_buffers_with_gain 	= 0;
Session::mix_buffers_no_gain_t		Session::mix_buffers_no_gain 	= 0;

sigc::signal<int> Session::AskAboutPendingState;
sigc::signal<void> Session::SendFeedback;

sigc::signal<void> Session::SMPTEOffsetChanged;
sigc::signal<void> Session::StartTimeChanged;
sigc::signal<void> Session::EndTimeChanged;

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
	  diskstreams (new DiskstreamList),
	  routes (new RouteList),
	  auditioner ((Auditioner*) 0),
	  _click_io ((IO*) 0),
	  main_outs (0)
{
	bool new_session;

	if (!eng.connected()) {
		throw failed_constructor();
	}

	cerr << "Loading session " << fullpath << " using snapshot " << snapshot_name << " (1)" << endl;

	n_physical_outputs = _engine.n_physical_outputs();
	n_physical_inputs =  _engine.n_physical_inputs();

	first_stage_init (fullpath, snapshot_name);
	
	new_session = !g_file_test (_path.c_str(), GFileTest (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR));
	if (new_session) {
		if (create (new_session, mix_template, compute_initial_length())) {
			cerr << "create failed\n";
			destroy ();
			throw failed_constructor ();
		}
	}
	
	if (second_stage_init (new_session)) {
		destroy ();
		throw failed_constructor ();
	}
	
	store_recent_sessions(_name, _path);
	
	bool was_dirty = dirty();

	_state_of_the_state = StateOfTheState (_state_of_the_state & ~Dirty);

	Config->ParameterChanged.connect (mem_fun (*this, &Session::config_changed));

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
		  nframes_t initial_length)

	: _engine (eng),
	  _mmc_port (default_mmc_port),
	  _mtc_port (default_mtc_port),
	  _midi_port (default_midi_port),
	  pending_events (2048),
	  midi_requests (16),
	  diskstreams (new DiskstreamList),
	  routes (new RouteList),
	  main_outs (0)

{
	bool new_session;

	if (!eng.connected()) {
		throw failed_constructor();
	}

	cerr << "Loading session " << fullpath << " using snapshot " << snapshot_name << " (2)" << endl;

	n_physical_outputs = _engine.n_physical_outputs();
	n_physical_inputs = _engine.n_physical_inputs();

	if (n_physical_inputs) {
		n_physical_inputs = max (requested_physical_in, n_physical_inputs);
	}

	if (n_physical_outputs) {
		n_physical_outputs = max (requested_physical_out, n_physical_outputs);
	}

	first_stage_init (fullpath, snapshot_name);

	new_session = !g_file_test (_path.c_str(), GFileTest (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR));

	if (new_session) {
		if (create (new_session, 0, initial_length)) {
			destroy ();
			throw failed_constructor ();
		}
	}

	{
		/* set up Master Out and Control Out if necessary */
		
		RouteList rl;
		int control_id = 1;
		
		if (control_out_channels) {
			shared_ptr<Route> r (new Route (*this, _("monitor"), -1, control_out_channels, -1, control_out_channels, Route::ControlOut));
			r->set_remote_control_id (control_id++);
			
			rl.push_back (r);
		}
		
		if (master_out_channels) {
			shared_ptr<Route> r (new Route (*this, _("master"), -1, master_out_channels, -1, master_out_channels, Route::MasterOut));
			r->set_remote_control_id (control_id);
			 
			rl.push_back (r);
		} else {
			/* prohibit auto-connect to master, because there isn't one */
			output_ac = AutoConnectOption (output_ac & ~AutoConnectMaster);
		}
		
		if (!rl.empty()) {
			add_routes (rl);
		}
		
	}

	Config->set_input_auto_connect (input_ac);
	Config->set_output_auto_connect (output_ac);

	if (second_stage_init (new_session)) {
		destroy ();
		throw failed_constructor ();
	}
	
	store_recent_sessions(_name, _path);
	
	bool was_dirty = dirty ();

	_state_of_the_state = StateOfTheState (_state_of_the_state & ~Dirty);

	Config->ParameterChanged.connect (mem_fun (*this, &Session::config_changed));

	if (was_dirty) {
		DirtyChanged (); /* EMIT SIGNAL */
	}
}

Session::~Session ()
{
	destroy ();
}

void
Session::destroy ()
{
	/* if we got to here, leaving pending capture state around
	   is a mistake.
	*/

	remove_pending_capture_state ();

	_state_of_the_state = StateOfTheState (CannotSave|Deletion);
	_engine.remove_session ();

	GoingAway (); /* EMIT SIGNAL */
	
	/* do this */

	notify_callbacks ();

	/* clear history so that no references to objects are held any more */

	_history.clear ();

	/* clear state tree so that no references to objects are held any more */
	
	if (state_tree) {
		delete state_tree;
	}

	terminate_butler_thread ();
	terminate_midi_thread ();
	
	if (click_data && click_data != default_click) {
		delete [] click_data;
	}

	if (click_emphasis_data && click_emphasis_data != default_click_emphasis) {
		delete [] click_emphasis_data;
	}

	clear_clicks ();

	for (vector<Sample*>::iterator i = _passthru_buffers.begin(); i != _passthru_buffers.end(); ++i) {
		free(*i);
	}

	for (vector<Sample*>::iterator i = _silent_buffers.begin(); i != _silent_buffers.end(); ++i) {
		free(*i);
	}

 	for (vector<Sample*>::iterator i = _send_buffers.begin(); i != _send_buffers.end(); ++i) {
 		free(*i);
 	}

	AudioDiskstream::free_working_buffers();

	/* this should cause deletion of the auditioner */

	// auditioner.reset ();
	
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

		(*i)->drop_references ();
		
		i = tmp;
	}
	
	for (PlaylistList::iterator i = unused_playlists.begin(); i != unused_playlists.end(); ) {
		PlaylistList::iterator tmp;

		tmp = i;
		++tmp;

		(*i)->drop_references ();
		
		i = tmp;
	}
	
	playlists.clear ();
	unused_playlists.clear ();

#ifdef TRACK_DESTRUCTION
	cerr << "delete audio regions\n";
#endif /* TRACK_DESTRUCTION */
	
	for (AudioRegionList::iterator i = audio_regions.begin(); i != audio_regions.end(); ) {
		AudioRegionList::iterator tmp;

		tmp = i;
		++tmp;

		i->second->drop_references ();

		i = tmp;
	}

	audio_regions.clear ();
	
#ifdef TRACK_DESTRUCTION
	cerr << "delete routes\n";
#endif /* TRACK_DESTRUCTION */
        {
		RCUWriter<RouteList> writer (routes);
		boost::shared_ptr<RouteList> r = writer.get_copy ();
		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			(*i)->drop_references ();
		}
		r->clear ();
		/* writer goes out of scope and updates master */
	}

	routes.flush ();

#ifdef TRACK_DESTRUCTION
	cerr << "delete diskstreams\n";
#endif /* TRACK_DESTRUCTION */
       {
	       RCUWriter<DiskstreamList> dwriter (diskstreams);
	       boost::shared_ptr<DiskstreamList> dsl = dwriter.get_copy();
	       for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
		       (*i)->drop_references ();
	       }
	       dsl->clear ();
       }
       diskstreams.flush ();

#ifdef TRACK_DESTRUCTION
	cerr << "delete audio sources\n";
#endif /* TRACK_DESTRUCTION */
	for (AudioSourceList::iterator i = audio_sources.begin(); i != audio_sources.end(); ) {
		AudioSourceList::iterator tmp;

		tmp = i;
		++tmp;

		i->second->drop_references ();

		i = tmp;
	}

	audio_sources.clear ();

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
}

void
Session::set_worst_io_latencies ()
{
	_worst_output_latency = 0;
	_worst_input_latency = 0;

	if (!_engine.connected()) {
		return;
	}

	boost::shared_ptr<RouteList> r = routes.reader ();
	
	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		_worst_output_latency = max (_worst_output_latency, (*i)->output_latency());
		_worst_input_latency = max (_worst_input_latency, (*i)->input_latency());
	}
}

void
Session::when_engine_running ()
{
	string first_physical_output;

	/* we don't want to run execute this again */

	set_block_size (_engine.frames_per_cycle());
	set_frame_rate (_engine.frame_rate());

	Config->map_parameters (mem_fun (*this, &Session::config_changed));

	/* every time we reconnect, recompute worst case output latencies */

	_engine.Running.connect (mem_fun (*this, &Session::set_worst_io_latencies));

	if (synced_to_jack()) {
		_engine.transport_stop ();
	}

	if (Config->get_jack_time_master()) {
		_engine.transport_locate (_transport_frame);
	}

	_clicking = false;

	try {
		XMLNode* child = 0;
		
		_click_io.reset (new ClickIO (*this, "click", 0, 0, -1, -1));

		if (state_tree && (child = find_named_node (*state_tree->root(), "Click")) != 0) {

			/* existing state for Click */
			
			if (_click_io->set_state (*child->children().front()) == 0) {
				
				_clicking = Config->get_clicking ();

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
					_clicking = Config->get_clicking ();
				}
			}
		}
	}

	catch (failed_constructor& err) {
		error << _("cannot setup Click I/O") << endmsg;
	}

	set_worst_io_latencies ();

	if (_clicking) {
		// XXX HOW TO ALERT UI TO THIS ? DO WE NEED TO?
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
	
	for (list<PortInsert*>::iterator i = _port_inserts.begin(); i != _port_inserts.end(); ++i) {
		uint32_t id;

		if (sscanf ((*i)->name().c_str(), "%*s %u", &id) == 1) {
			if (id > insert_cnt) {
				insert_cnt = id;
			}
		}
	}

	send_cnt = 0;

	for (list<Send*>::iterator i = _sends.begin(); i != _sends.end(); ++i) {
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

#ifdef HAVE_LIBLO
	/* and to OSC */

	osc->set_session (*this);
#endif
    
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

	if (auditioner == 0) {
		
		/* we delay creating the auditioner till now because
		   it makes its own connections to ports.
		   the engine has to be running for this to work.
		*/
		
		try {
			auditioner.reset (new Auditioner (*this));
		}
		
		catch (failed_constructor& err) {
			warning << _("cannot create Auditioner: no auditioning of regions possible") << endmsg;
		}
	}

	/* Tell all IO objects to create their ports */

	IO::enable_ports ();

	if (_control_out) {
		uint32_t n;
		vector<string> cports;

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


		uint32_t ni = _control_out->n_inputs();

		for (n = 0; n < ni; ++n) {
			cports.push_back (_control_out->input(n)->name());
		}

		boost::shared_ptr<RouteList> r = routes.reader ();		

		for (RouteList::iterator x = r->begin(); x != r->end(); ++x) {
			(*x)->set_control_outs (cports);
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
Session::playlist_length_changed ()
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
Session::diskstream_playlist_changed (boost::weak_ptr<Diskstream> wptr)
{
	boost::shared_ptr<Diskstream> dstream = wptr.lock();
	
	if (!dstream) {
		return;

	}

	boost::shared_ptr<Playlist> playlist;

	if ((playlist = dstream->playlist()) != 0) {
		playlist->LengthChanged.connect (mem_fun (this, &Session::playlist_length_changed));
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

	if (Config->get_all_safe()) {
		return false;
	}
	return true;
}

void
Session::reset_input_monitor_state ()
{
	if (transport_rolling()) {

		boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();

		for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
			if ((*i)->record_enabled ()) {
				//cerr << "switching to input = " << !auto_input << __FILE__ << __LINE__ << endl << endl;
				(*i)->monitor_input (Config->get_monitoring_model() == HardwareMonitoring && !Config->get_auto_input());
			}
		}
	} else {
		boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();

		for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
			if ((*i)->record_enabled ()) {
				//cerr << "switching to input = " << !Config->get_auto_input() << __FILE__ << __LINE__ << endl << endl;
				(*i)->monitor_input (Config->get_monitoring_model() == HardwareMonitoring);
			}
		}
	}
}

void
Session::auto_punch_start_changed (Location* location)
{
	replace_event (Event::PunchIn, location->start());

	if (get_record_enabled() && Config->get_punch_in()) {
		/* capture start has been changed, so save new pending state */
		save_state ("", true);
	}
}	

void
Session::auto_punch_end_changed (Location* location)
{
	nframes_t when_to_stop = location->end();
	// when_to_stop += _worst_output_latency + _worst_input_latency;
	replace_event (Event::PunchOut, when_to_stop);
}	

void
Session::auto_punch_changed (Location* location)
{
	nframes_t when_to_stop = location->end();

	replace_event (Event::PunchIn, location->start());
	//when_to_stop += _worst_output_latency + _worst_input_latency;
	replace_event (Event::PunchOut, when_to_stop);
}	

void
Session::auto_loop_changed (Location* location)
{
	replace_event (Event::AutoLoop, location->end(), location->start());

	if (transport_rolling() && play_loop) {

		//if (_transport_frame < location->start() || _transport_frame > location->end()) {

		if (_transport_frame > location->end()) {
			// relocate to beginning of loop
			clear_events (Event::LocateRoll);
			
			request_locate (location->start(), true);

		}
		else if (Config->get_seamless_loop() && !loop_changing) {
			
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
	if (g_atomic_int_get (&_record_status) != Recording) {
		g_atomic_int_set (&_record_status, Recording);
		_last_record_location = _transport_frame;
		send_mmc_in_another_thread (MIDI::MachineControl::cmdRecordStrobe);

		if (Config->get_monitoring_model() == HardwareMonitoring && Config->get_auto_input()) {
			boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();
			for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
				if ((*i)->record_enabled ()) {
					(*i)->monitor_input (true);   
				}
			}
		}

		RecordStateChanged ();
	}
}

void
Session::disable_record (bool rt_context, bool force)
{
	RecordState rs;

	if ((rs = (RecordState) g_atomic_int_get (&_record_status)) != Disabled) {

		if ((!Config->get_latched_record_enable () && !play_loop) || force) {
			g_atomic_int_set (&_record_status, Disabled);
		} else {
			if (rs == Recording) {
				g_atomic_int_set (&_record_status, Enabled);
			}
		}

		send_mmc_in_another_thread (MIDI::MachineControl::cmdRecordExit);

		if (Config->get_monitoring_model() == HardwareMonitoring && Config->get_auto_input()) {
			boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();
			
			for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
				if ((*i)->record_enabled ()) {
					(*i)->monitor_input (false);   
				}
			}
		}
		
		RecordStateChanged (); /* emit signal */

		if (!rt_context) {
			remove_pending_capture_state ();
		}
	}
}

void
Session::step_back_from_record ()
{
	/* XXX really atomic compare+swap here */
	if (g_atomic_int_get (&_record_status) == Recording) {
		g_atomic_int_set (&_record_status, Enabled);

		if (Config->get_monitoring_model() == HardwareMonitoring) {
			boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();
			
			for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
				if (Config->get_auto_input() && (*i)->record_enabled ()) {
					//cerr << "switching from input" << __FILE__ << __LINE__ << endl << endl;
					(*i)->monitor_input (false);   
				}
			}
		}
	}
}

void
Session::maybe_enable_record ()
{
	g_atomic_int_set (&_record_status, Enabled);

	/* this function is currently called from somewhere other than an RT thread.
	   this save_state() call therefore doesn't impact anything.
	*/

	save_state ("", true);

	if (_transport_speed) {
		if (!Config->get_punch_in()) {
			enable_record ();
		} 
	} else {
		send_mmc_in_another_thread (MIDI::MachineControl::cmdRecordPause);
		RecordStateChanged (); /* EMIT SIGNAL */
	}

	set_dirty();
}

nframes_t
Session::audible_frame () const
{
	nframes_t ret;
	nframes_t offset;
	nframes_t tf;

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
Session::set_frame_rate (nframes_t frames_per_second)
{
	/** \fn void Session::set_frame_size(nframes_t)
		the AudioEngine object that calls this guarantees 
		that it will not be called while we are also in
		::process(). Its fine to do things that block
		here.
	*/

	_base_frame_rate = frames_per_second;

	sync_time_vars();

	IO::set_automation_interval ((jack_nframes_t) ceil ((double) frames_per_second * (0.001 * Config->get_automation_interval())));

	clear_clicks ();

	// XXX we need some equivalent to this, somehow
	// SndFileSource::setup_standard_crossfades (frames_per_second);

	set_dirty();

	/* XXX need to reset/reinstantiate all LADSPA plugins */
}

void
Session::set_block_size (nframes_t nframes)
{
	/* the AudioEngine guarantees 
	   that it will not be called while we are also in
	   ::process(). It is therefore fine to do things that block
	   here.
	*/

	{ 
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

		for (vector<Sample*>::iterator i = _send_buffers.begin(); i != _send_buffers.end(); ++i) {
			free(*i);

			Sample *buf;
#ifdef NO_POSIX_MEMALIGN
			buf = (Sample *) malloc(current_block_size * sizeof(Sample));
#else
			posix_memalign((void **)&buf,CPU_CACHE_ALIGN,current_block_size * sizeof(Sample));
#endif			
			*i = buf;

			memset (*i, 0, sizeof (Sample) * current_block_size);
		}

		
		if (_gain_automation_buffer) {
			delete [] _gain_automation_buffer;
		}
		_gain_automation_buffer = new gain_t[nframes];

		allocate_pan_automation_buffers (nframes, _npan_buffers, true);

		boost::shared_ptr<RouteList> r = routes.reader ();

		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			(*i)->set_block_size (nframes);
		}
		
		boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();
		for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
			(*i)->set_block_size (nframes);
		}

		set_worst_io_latencies ();
	}
}

void
Session::set_default_fade (float steepness, float fade_msecs)
{
#if 0
	nframes_t fade_frames;
	
	/* Don't allow fade of less 1 frame */
	
	if (fade_msecs < (1000.0 * (1.0/_current_frame_rate))) {

		fade_msecs = 0;
		fade_frames = 0;

	} else {
		
		fade_frames = (nframes_t) floor (fade_msecs * _current_frame_rate * 0.001);
		
	}

	default_fade_msecs = fade_msecs;
	default_fade_steepness = steepness;

	{
		// jlc, WTF is this!
		Glib::RWLock::ReaderLock lm (route_lock);
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
    bool operator() (boost::shared_ptr<Route> r1, boost::shared_ptr<Route> r2) {
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
trace_terminal (shared_ptr<Route> r1, shared_ptr<Route> rbase)
{
	shared_ptr<Route> r2;

	if ((r1->fed_by.find (rbase) != r1->fed_by.end()) && (rbase->fed_by.find (r1) != rbase->fed_by.end())) {
		info << string_compose(_("feedback loop setup between %1 and %2"), r1->name(), rbase->name()) << endmsg;
		return;
	} 

	/* make a copy of the existing list of routes that feed r1 */

	set<shared_ptr<Route> > existing = r1->fed_by;

	/* for each route that feeds r1, recurse, marking it as feeding
	   rbase as well.
	*/

	for (set<shared_ptr<Route> >::iterator i = existing.begin(); i != existing.end(); ++i) {
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
Session::resort_routes ()
{
	/* don't do anything here with signals emitted
	   by Routes while we are being destroyed.
	*/

	if (_state_of_the_state & Deletion) {
		return;
	}


	{

		RCUWriter<RouteList> writer (routes);
		shared_ptr<RouteList> r = writer.get_copy ();
		resort_routes_using (r);
		/* writer goes out of scope and forces update */
	}

}
void
Session::resort_routes_using (shared_ptr<RouteList> r)
{
	RouteList::iterator i, j;
	
	for (i = r->begin(); i != r->end(); ++i) {
		
		(*i)->fed_by.clear ();
		
		for (j = r->begin(); j != r->end(); ++j) {
			
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
	
	for (i = r->begin(); i != r->end(); ++i) {
		trace_terminal (*i, *i);
	}	

	RouteSorter cmp;
	r->sort (cmp);
	
	/* don't leave dangling references to routes in Route::fed_by */

	for (i = r->begin(); i != r->end(); ++i) {
		(*i)->fed_by.clear ();
	}

#if 0
	cerr << "finished route resort\n";
	
	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		cerr << " " << (*i)->name() << " signal order = " << (*i)->order_key ("signal") << endl;
	}
	cerr << endl;
#endif
	
}

list<boost::shared_ptr<AudioTrack> >
Session::new_audio_track (int input_channels, int output_channels, TrackMode mode, uint32_t how_many)
{
	char track_name[32];
	uint32_t track_id = 0;
	uint32_t n = 0;
	uint32_t channels_used = 0;
	string port;
	RouteList new_routes;
	list<boost::shared_ptr<AudioTrack> > ret;
	uint32_t control_id;

	/* count existing audio tracks */

	{
		shared_ptr<RouteList> r = routes.reader ();

		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			if (dynamic_cast<AudioTrack*>((*i).get()) != 0) {
				if (!(*i)->hidden()) {
					n++;
					channels_used += (*i)->n_inputs();
				}
			}
		}
	}

	vector<string> physinputs;
	vector<string> physoutputs;
	uint32_t nphysical_in;
	uint32_t nphysical_out;

	_engine.get_physical_outputs (physoutputs);
	_engine.get_physical_inputs (physinputs);
	control_id = ntracks() + nbusses() + 1;

	while (how_many) {

		/* check for duplicate route names, since we might have pre-existing
		   routes with this name (e.g. create Audio1, Audio2, delete Audio1,
		   save, close,restart,add new route - first named route is now
		   Audio2)
		*/
		

		do {
			++track_id;

			snprintf (track_name, sizeof(track_name), "Audio %" PRIu32, track_id);

			if (route_by_name (track_name) == 0) {
				break;
			}
			
		} while (track_id < (UINT_MAX-1));

		if (Config->get_input_auto_connect() & AutoConnectPhysical) {
			nphysical_in = min (n_physical_inputs, (uint32_t) physinputs.size());
		} else {
			nphysical_in = 0;
		}
		
		if (Config->get_output_auto_connect() & AutoConnectPhysical) {
			nphysical_out = min (n_physical_outputs, (uint32_t) physinputs.size());
		} else {
			nphysical_out = 0;
		}

		shared_ptr<AudioTrack> track;
		
		try {
			track = boost::shared_ptr<AudioTrack>((new AudioTrack (*this, track_name, Route::Flag (0), mode)));
			
			if (track->ensure_io (input_channels, output_channels, false, this)) {
				error << string_compose (_("cannot configure %1 in/%2 out configuration for new audio track"),
							 input_channels, output_channels)
				      << endmsg;
				goto failed;
			}

			if (nphysical_in) {
				for (uint32_t x = 0; x < track->n_inputs() && x < nphysical_in; ++x) {
					
					port = "";
					
					if (Config->get_input_auto_connect() & AutoConnectPhysical) {
						port = physinputs[(channels_used+x)%nphysical_in];
					} 
					
					if (port.length() && track->connect_input (track->input (x), port, this)) {
						break;
					}
				}
			}
			
			for (uint32_t x = 0; x < track->n_outputs(); ++x) {
				
				port = "";
				
				if (nphysical_out && (Config->get_output_auto_connect() & AutoConnectPhysical)) {
					port = physoutputs[(channels_used+x)%nphysical_out];
				} else if (Config->get_output_auto_connect() & AutoConnectMaster) {
					if (_master_out) {
						port = _master_out->input (x%_master_out->n_inputs())->name();
					}
				}
				
				if (port.length() && track->connect_output (track->output (x), port, this)) {
					break;
				}
			}
			
			channels_used += track->n_inputs ();

			track->audio_diskstream()->non_realtime_input_change();
			
			track->DiskstreamChanged.connect (mem_fun (this, &Session::resort_routes));
			track->set_remote_control_id (control_id);
			++control_id;

			new_routes.push_back (track);
			ret.push_back (track);
		}

		catch (failed_constructor &err) {
			error << _("Session: could not create new audio track.") << endmsg;

			if (track) {
				/* we need to get rid of this, since the track failed to be created */
				/* XXX arguably, AudioTrack::AudioTrack should not do the Session::add_diskstream() */

				{ 
					RCUWriter<DiskstreamList> writer (diskstreams);
					boost::shared_ptr<DiskstreamList> ds = writer.get_copy();
					ds->remove (track->audio_diskstream());
				}
			}

			goto failed;
		}

		catch (AudioEngine::PortRegistrationFailure& pfe) {

			error << _("No more JACK ports are available. You will need to stop Ardour and restart JACK with ports if you need this many tracks.") << endmsg;

			if (track) {
				/* we need to get rid of this, since the track failed to be created */
				/* XXX arguably, AudioTrack::AudioTrack should not do the Session::add_diskstream() */

				{ 
					RCUWriter<DiskstreamList> writer (diskstreams);
					boost::shared_ptr<DiskstreamList> ds = writer.get_copy();
					ds->remove (track->audio_diskstream());
				}
			}

			goto failed;
		}

		--how_many;
	}

  failed:
	if (!new_routes.empty()) {
		add_routes (new_routes, false);
		save_state (_current_snapshot_name);
	}

	return ret;
}

void
Session::set_remote_control_ids ()
{
	RemoteModel m = Config->get_remote_model();

	shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if ( MixerOrdered == m) {			
			long order = (*i)->order_key(N_("signal"));
			(*i)->set_remote_control_id( order+1 );
		} else if ( EditorOrdered == m) {
			long order = (*i)->order_key(N_("editor"));
			(*i)->set_remote_control_id( order+1 );
		} else if ( UserOrdered == m) {
			//do nothing ... only changes to remote id's are initiated by user 
		}
	}
}


Session::RouteList
Session::new_audio_route (int input_channels, int output_channels, uint32_t how_many)
{
	char bus_name[32];
	uint32_t bus_id = 1;
	uint32_t n = 0;
	string port;
	RouteList ret;
	uint32_t control_id;

	/* count existing audio busses */

	{
		shared_ptr<RouteList> r = routes.reader ();

		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			if (dynamic_cast<AudioTrack*>((*i).get()) == 0) {
				if (!(*i)->hidden() && (*i)->name() != _("master")) {
					bus_id++;
				}
			}
		}
	}

	vector<string> physinputs;
	vector<string> physoutputs;

	_engine.get_physical_outputs (physoutputs);
	_engine.get_physical_inputs (physinputs);
	control_id = ntracks() + nbusses() + 1;

	while (how_many) {

		do {
			snprintf (bus_name, sizeof(bus_name), "Bus %" PRIu32, bus_id);

			bus_id++;

			if (route_by_name (bus_name) == 0) {
				break;
			}

		} while (bus_id < (UINT_MAX-1));

		try {
			shared_ptr<Route> bus (new Route (*this, bus_name, -1, -1, -1, -1, Route::Flag(0), DataType::AUDIO));
			
			if (bus->ensure_io (input_channels, output_channels, false, this)) {
				error << string_compose (_("cannot configure %1 in/%2 out configuration for new audio track"),
							 input_channels, output_channels)
				      << endmsg;
				goto failure;
			}
			
			for (uint32_t x = 0; n_physical_inputs && x < bus->n_inputs(); ++x) {
				
				port = "";

				if (Config->get_input_auto_connect() & AutoConnectPhysical) {
						port = physinputs[((n+x)%n_physical_inputs)];
				} 
				
				if (port.length() && bus->connect_input (bus->input (x), port, this)) {
					break;
				}
			}
			
			for (uint32_t x = 0; n_physical_outputs && x < bus->n_outputs(); ++x) {
				
				port = "";
				
				if (Config->get_output_auto_connect() & AutoConnectPhysical) {
					port = physoutputs[((n+x)%n_physical_outputs)];
				} else if (Config->get_output_auto_connect() & AutoConnectMaster) {
					if (_master_out) {
						port = _master_out->input (x%_master_out->n_inputs())->name();
					}
				}
				
				if (port.length() && bus->connect_output (bus->output (x), port, this)) {
					break;
				}
			}
			
			bus->set_remote_control_id (control_id);
			++control_id;

			ret.push_back (bus);
		}
	

		catch (failed_constructor &err) {
			error << _("Session: could not create new audio route.") << endmsg;
			goto failure;
		}

		catch (AudioEngine::PortRegistrationFailure& pfe) {
			error << _("No more JACK ports are available. You will need to stop Ardour and restart JACK with ports if you need this many tracks.") << endmsg;
			goto failure;
		}


		--how_many;
	}

  failure:
	if (!ret.empty()) {
		add_routes (ret, false);
		save_state (_current_snapshot_name);
	}

	return ret;

}

void
Session::add_routes (RouteList& new_routes, bool save)
{
	{ 
		RCUWriter<RouteList> writer (routes);
		shared_ptr<RouteList> r = writer.get_copy ();
		r->insert (r->end(), new_routes.begin(), new_routes.end());
		resort_routes_using (r);
	}

	for (RouteList::iterator x = new_routes.begin(); x != new_routes.end(); ++x) {
		
		boost::weak_ptr<Route> wpr (*x);

		(*x)->solo_changed.connect (sigc::bind (mem_fun (*this, &Session::route_solo_changed), wpr));
		(*x)->mute_changed.connect (mem_fun (*this, &Session::route_mute_changed));
		(*x)->output_changed.connect (mem_fun (*this, &Session::set_worst_io_latencies_x));
		(*x)->redirects_changed.connect (mem_fun (*this, &Session::update_latency_compensation_proxy));
		
		if ((*x)->master()) {
			_master_out = (*x);
		}
		
		if ((*x)->control()) {
			_control_out = (*x);
		} 
	}

	if (_control_out && IO::connecting_legal) {

		vector<string> cports;
		uint32_t ni = _control_out->n_inputs();
		uint32_t n;

		for (n = 0; n < ni; ++n) {
			cports.push_back (_control_out->input(n)->name());
		}

		for (RouteList::iterator x = new_routes.begin(); x != new_routes.end(); ++x) {
			(*x)->set_control_outs (cports);
		}
	} 

	set_dirty();

	if (save) {
		save_state (_current_snapshot_name);
	}

	RouteAdded (new_routes); /* EMIT SIGNAL */
}

void
Session::add_diskstream (boost::shared_ptr<Diskstream> dstream)
{
	/* need to do this in case we're rolling at the time, to prevent false underruns */
	dstream->do_refill_with_alloc ();
	
	dstream->set_block_size (current_block_size);

	{
		RCUWriter<DiskstreamList> writer (diskstreams);
		boost::shared_ptr<DiskstreamList> ds = writer.get_copy();
		ds->push_back (dstream);
		/* writer goes out of scope, copies ds back to main */
	} 

	dstream->PlaylistChanged.connect (sigc::bind (mem_fun (*this, &Session::diskstream_playlist_changed), 
						      boost::weak_ptr<Diskstream> (dstream)));
	/* this will connect to future changes, and check the current length */
	diskstream_playlist_changed (dstream);

	dstream->prepare ();
}

void
Session::remove_route (shared_ptr<Route> route)
{
	{ 	
		RCUWriter<RouteList> writer (routes);
		shared_ptr<RouteList> rs = writer.get_copy ();
		
		rs->remove (route);

		/* deleting the master out seems like a dumb
		   idea, but its more of a UI policy issue
		   than our concern.
		*/

		if (route == _master_out) {
			_master_out = shared_ptr<Route> ();
		}

		if (route == _control_out) {
			_control_out = shared_ptr<Route> ();

			/* cancel control outs for all routes */

			vector<string> empty;

			for (RouteList::iterator r = rs->begin(); r != rs->end(); ++r) {
				(*r)->set_control_outs (empty);
			}
		}

		update_route_solo_state ();
		
		/* writer goes out of scope, forces route list update */
	}

	// FIXME: audio specific
	AudioTrack* at;
	boost::shared_ptr<AudioDiskstream> ds;
	
	if ((at = dynamic_cast<AudioTrack*>(route.get())) != 0) {
		ds = at->audio_diskstream();
	}
	
	if (ds) {

		{
			RCUWriter<DiskstreamList> dsl (diskstreams);
			boost::shared_ptr<DiskstreamList> d = dsl.get_copy();
			d->remove (ds);
		}

		diskstreams.flush ();
	}

	find_current_end ();
	
	update_latency_compensation (false, false);
	set_dirty();

	// We need to disconnect the routes inputs and outputs 

	route->disconnect_inputs (0);
	route->disconnect_outputs (0);
	
	/* get rid of it from the dead wood collection in the route list manager */

	/* XXX i think this is unsafe as it currently stands, but i am not sure. (pd, october 2nd, 2006) */

	routes.flush ();

	/* try to cause everyone to drop their references */

	route->drop_references ();

	/* save the new state of the world */

	if (save_state (_current_snapshot_name)) {
		save_history (_current_snapshot_name);
	}
}	

void
Session::route_mute_changed (void* src)
{
	set_dirty ();
}

void
Session::route_solo_changed (void* src, boost::weak_ptr<Route> wpr)
{      
	if (solo_update_disabled) {
		// We know already
		return;
	}
	
	bool is_track;
	boost::shared_ptr<Route> route = wpr.lock ();

	if (!route) {
		/* should not happen */
		error << string_compose (_("programming error: %1"), X_("invalid route weak ptr passed to route_solo_changed")) << endmsg;
		return;
	}

	is_track = (boost::dynamic_pointer_cast<AudioTrack>(route) != 0);
	
	shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		
		/* soloing a track mutes all other tracks, soloing a bus mutes all other busses */
		
		if (is_track) {
			
			/* don't mess with busses */
			
			if (dynamic_cast<AudioTrack*>((*i).get()) == 0) {
				continue;
			}
			
		} else {
			
			/* don't mess with tracks */
			
			if (dynamic_cast<AudioTrack*>((*i).get()) != 0) {
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
				
				if (Config->get_solo_latched()) {
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

        for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if ((*i)->soloed()) {
			something_soloed = true;
			if (dynamic_cast<AudioTrack*>((*i).get())) {
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
		SoloActive (currently_soloing); /* EMIT SIGNAL */
	}

	SoloChanged (); /* EMIT SIGNAL */

	set_dirty();
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
	
	shared_ptr<RouteList> r = routes.reader ();

        for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if ((*i)->soloed()) {
			mute = true;
			if (dynamic_cast<AudioTrack*>((*i).get())) {
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

		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
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
	shared_ptr<RouteList> r = routes.reader ();

        for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		
		if (is_track) {
			
			/* only alter track solo mute */
			
			if (dynamic_cast<AudioTrack*>((*i).get())) {
				if ((*i)->soloed()) {
					(*i)->set_solo_mute (!mute);
				} else {
					(*i)->set_solo_mute (mute);
				}
			}

		} else {

			/* only alter bus solo mute */

			if (!dynamic_cast<AudioTrack*>((*i).get())) {

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
	update_route_solo_state();
}	
		
shared_ptr<Route>
Session::route_by_name (string name)
{
	shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if ((*i)->name() == name) {
			return *i;
		}
	}

	return shared_ptr<Route> ((Route*) 0);
}

shared_ptr<Route>
Session::route_by_id (PBD::ID id)
{
	shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if ((*i)->id() == id) {
			return *i;
		}
	}

	return shared_ptr<Route> ((Route*) 0);
}

shared_ptr<Route>
Session::route_by_remote_id (uint32_t id)
{
	shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if ((*i)->remote_control_id() == id) {
			return *i;
		}
	}

	return shared_ptr<Route> ((Route*) 0);
}

void
Session::find_current_end ()
{
	if (_state_of_the_state & Loading) {
		return;
	}

	nframes_t max = get_maximum_extent ();

	if (max > end_location->end()) {
		end_location->set_end (max);
		set_dirty();
		DurationChanged(); /* EMIT SIGNAL */
	}
}

nframes_t
Session::get_maximum_extent () const
{
	nframes_t max = 0;
	nframes_t me; 

	boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();

	for (DiskstreamList::const_iterator i = dsl->begin(); i != dsl->end(); ++i) {
		boost::shared_ptr<Playlist> pl = (*i)->playlist();
		if ((me = pl->get_maximum_extent()) > max) {
			max = me;
		}
	}

	return max;
}

boost::shared_ptr<Diskstream>
Session::diskstream_by_name (string name)
{
	boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();

	for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
		if ((*i)->name() == name) {
			return *i;
		}
	}

	return boost::shared_ptr<Diskstream>((Diskstream*) 0);
}

boost::shared_ptr<Diskstream>
Session::diskstream_by_id (const PBD::ID& id)
{
	boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();

	for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
		if ((*i)->id() == id) {
			return *i;
		}
	}

	return boost::shared_ptr<Diskstream>((Diskstream*) 0);
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
			if (i->second->name() == sbuf) {
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
		
		Glib::Mutex::Lock lm (region_lock);

		snprintf (buf, sizeof (buf), "%d", (int)audio_regions.size() + 1);

		
		result = "region.";
		result += buf;

	} else {

		/* XXX this is going to be slow. optimize me later */
		
		if (newlevel) {
			subbase = base;
		} else {
			string::size_type pos;

			pos = base.find_last_of ('.');

			/* pos may be npos, but then we just use entire base */

			subbase = base.substr (0, pos);

		}

		bool name_taken = true;
		
		{
			Glib::Mutex::Lock lm (region_lock);
			
			for (int n = 1; n < 5000; ++n) {
				
				result = subbase;
				snprintf (buf, sizeof (buf), ".%d", n);
				result += buf;
				
				name_taken = false;
				
				for (AudioRegionList::const_iterator i = audio_regions.begin(); i != audio_regions.end(); ++i) {
					if (i->second->name() == result) {
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
Session::add_region (boost::shared_ptr<Region> region)
{
	boost::shared_ptr<AudioRegion> ar;
	boost::shared_ptr<AudioRegion> oar;
	bool added = false;

	{ 
		Glib::Mutex::Lock lm (region_lock);

		if ((ar = boost::dynamic_pointer_cast<AudioRegion> (region)) != 0) {

			AudioRegionList::iterator x;

			for (x = audio_regions.begin(); x != audio_regions.end(); ++x) {

				oar = boost::dynamic_pointer_cast<AudioRegion> (x->second);

				if (ar->region_list_equivalent (oar)) {
					break;
				}
			}

			if (x == audio_regions.end()) {

				pair<AudioRegionList::key_type,AudioRegionList::mapped_type> entry;

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
		region->GoingAway.connect (sigc::bind (mem_fun (*this, &Session::remove_region), boost::weak_ptr<Region>(region)));
		region->StateChanged.connect (sigc::bind (mem_fun (*this, &Session::region_changed), boost::weak_ptr<Region>(region)));
		AudioRegionAdded (ar); /* EMIT SIGNAL */
	}
}

void
Session::region_changed (Change what_changed, boost::weak_ptr<Region> weak_region)
{
	boost::shared_ptr<Region> region (weak_region.lock ());

	if (!region) {
		return;
	}

	if (what_changed & Region::HiddenChanged) {
		/* relay hidden changes */
		RegionHiddenChange (region);
	}
}

void
Session::remove_region (boost::weak_ptr<Region> weak_region)
{
	AudioRegionList::iterator i;
	boost::shared_ptr<Region> region (weak_region.lock ());

	if (!region) {
		return;
	}

	boost::shared_ptr<AudioRegion> ar;
	bool removed = false;

	{ 
		Glib::Mutex::Lock lm (region_lock);

 		if ((ar = boost::dynamic_pointer_cast<AudioRegion> (region)) != 0) {
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
		AudioRegionRemoved (ar); /* EMIT SIGNAL */
	}
}

boost::shared_ptr<AudioRegion>
Session::find_whole_file_parent (boost::shared_ptr<AudioRegion const> child)
{
	AudioRegionList::iterator i;
	boost::shared_ptr<AudioRegion> region;
	Glib::Mutex::Lock lm (region_lock);

	for (i = audio_regions.begin(); i != audio_regions.end(); ++i) {

		region = i->second;

		if (region->whole_file()) {

			if (child->source_equivalent (region)) {
				return region;
			}
		}
	} 

	return boost::shared_ptr<AudioRegion> ();
}	

void
Session::find_equivalent_playlist_regions (boost::shared_ptr<Region> region, vector<boost::shared_ptr<Region> >& result)
{
	for (PlaylistList::iterator i = playlists.begin(); i != playlists.end(); ++i)
		(*i)->get_region_list_equivalent_regions (region, result);
}

int
Session::destroy_region (boost::shared_ptr<Region> region)
{
	vector<boost::shared_ptr<Source> > srcs;
		
	{
		boost::shared_ptr<AudioRegion> aregion;
		
		if ((aregion = boost::dynamic_pointer_cast<AudioRegion> (region)) == 0) {
			return 0;
		}
		
		if (aregion->playlist()) {
			aregion->playlist()->destroy_region (region);
		}
		
		for (uint32_t n = 0; n < aregion->n_channels(); ++n) {
			srcs.push_back (aregion->source (n));
		}
	}

	region->drop_references ();

	for (vector<boost::shared_ptr<Source> >::iterator i = srcs.begin(); i != srcs.end(); ++i) {

		if (!(*i)->used()) {
			boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource>(*i);
			
			if (afs) {
				(afs)->mark_for_remove ();
			}
			
			(*i)->drop_references ();
			
			cerr << "source was not used by any playlist\n";
		}
	}

	return 0;
}

int
Session::destroy_regions (list<boost::shared_ptr<Region> > regions)
{
	for (list<boost::shared_ptr<Region> >::iterator i = regions.begin(); i != regions.end(); ++i) {
		destroy_region (*i);
	}
	return 0;
}

int
Session::remove_last_capture ()
{
	list<boost::shared_ptr<Region> > r;
	
	boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();
	
	for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
		list<boost::shared_ptr<Region> >& l = (*i)->last_capture_regions();
		
		if (!l.empty()) {
			r.insert (r.end(), l.begin(), l.end());
			l.clear ();
		}
	}

	destroy_regions (r);

	save_state (_current_snapshot_name);

	return 0;
}

int
Session::remove_region_from_region_list (boost::shared_ptr<Region> r)
{
	remove_region (r);
	return 0;
}

/* Source Management */

void
Session::add_source (boost::shared_ptr<Source> source)
{
	boost::shared_ptr<AudioFileSource> afs;

	if ((afs = boost::dynamic_pointer_cast<AudioFileSource>(source)) != 0) {

		pair<AudioSourceList::key_type, AudioSourceList::mapped_type> entry;
		pair<AudioSourceList::iterator,bool> result;

		entry.first = source->id();
		entry.second = afs;
		
		{
			Glib::Mutex::Lock lm (audio_source_lock);
			result = audio_sources.insert (entry);
		}

		if (result.second) {
			source->GoingAway.connect (sigc::bind (mem_fun (this, &Session::remove_source), boost::weak_ptr<Source> (source)));
			set_dirty();
		}

	} 
}

void
Session::remove_source (boost::weak_ptr<Source> src)
{
	AudioSourceList::iterator i;
	boost::shared_ptr<Source> source = src.lock();

	if (!source) {
		return;
	} 

	{ 
		Glib::Mutex::Lock lm (audio_source_lock);
		
		if ((i = audio_sources.find (source->id())) != audio_sources.end()) {
			audio_sources.erase (i);
		} 
	}
	
	if (!_state_of_the_state & InCleanup) {
		
		/* save state so we don't end up with a session file
		   referring to non-existent sources.
		*/
		
		save_state (_current_snapshot_name);
	}
}

boost::shared_ptr<Source>
Session::source_by_id (const PBD::ID& id)
{
	Glib::Mutex::Lock lm (audio_source_lock);
	AudioSourceList::iterator i;
	boost::shared_ptr<Source> source;

	if ((i = audio_sources.find (id)) != audio_sources.end()) {
		source = i->second;
	}

	/* XXX search MIDI or other searches here */
	
	return source;
}


boost::shared_ptr<Source>
Session::source_by_path_and_channel (const Glib::ustring& path, uint16_t chn)
{
	Glib::Mutex::Lock lm (audio_source_lock);

	for (AudioSourceList::iterator i = audio_sources.begin(); i != audio_sources.end(); ++i) {
		boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource>(i->second);

		if (afs && afs->path() == path && chn == afs->channel()) {
			return afs;
		} 
		       
	}
	return boost::shared_ptr<Source>();
}

string
Session::peak_path_from_audio_path (string audio_path) const
{
	string res;

	res = peak_dir ();
	res += PBD::basename_nosuffix (audio_path);
	res += ".peak";

	return res;
}

string
Session::change_audio_path_by_name (string path, string oldname, string newname, bool destructive)
{
	string look_for;
	string old_basename = PBD::basename_nosuffix (oldname);
	string new_legalized = legalize_for_path (newname);

	/* note: we know (or assume) the old path is already valid */

	if (destructive) {
		
		/* destructive file sources have a name of the form:

		    /path/to/Tnnnn-NAME(%[LR])?.wav
		  
		    the task here is to replace NAME with the new name.
		*/
		
		/* find last slash */

		string dir;
		string prefix;
		string::size_type slash;
		string::size_type dash;

		if ((slash = path.find_last_of ('/')) == string::npos) {
			return "";
		}

		dir = path.substr (0, slash+1);

		/* '-' is not a legal character for the NAME part of the path */

		if ((dash = path.find_last_of ('-')) == string::npos) {
			return "";
		}

		prefix = path.substr (slash+1, dash-(slash+1));

		path = dir;
		path += prefix;
		path += '-';
		path += new_legalized;
		path += ".wav";  /* XXX gag me with a spoon */
		
	} else {
		
		/* non-destructive file sources have a name of the form:

		    /path/to/NAME-nnnnn(%[LR])?.wav
		  
		    the task here is to replace NAME with the new name.
		*/
		
		string dir;
		string suffix;
		string::size_type slash;
		string::size_type dash;
		string::size_type postfix;

		/* find last slash */

		if ((slash = path.find_last_of ('/')) == string::npos) {
			return "";
		}

		dir = path.substr (0, slash+1);

		/* '-' is not a legal character for the NAME part of the path */

		if ((dash = path.find_last_of ('-')) == string::npos) {
			return "";
		}

		suffix = path.substr (dash+1);
		
		// Suffix is now everything after the dash. Now we need to eliminate
		// the nnnnn part, which is done by either finding a '%' or a '.'

		postfix = suffix.find_last_of ("%");
		if (postfix == string::npos) {
			postfix = suffix.find_last_of ('.');
		}

		if (postfix != string::npos) {
			suffix = suffix.substr (postfix);
		} else {
			error << "Logic error in Session::change_audio_path_by_name(), please report to the developers" << endl;
			return "";
		}

		const uint32_t limit = 10000;
		char buf[PATH_MAX+1];

		for (uint32_t cnt = 1; cnt <= limit; ++cnt) {

			snprintf (buf, sizeof(buf), "%s%s-%u%s", dir.c_str(), newname.c_str(), cnt, suffix.c_str());

			if (access (buf, F_OK) != 0) {
				path = buf;
				break;
			}
			path = "";
		}

		if (path == "") {
			error << "FATAL ERROR! Could not find a " << endl;
		}

	}

	return path;
}

string
Session::audio_path_from_name (string name, uint32_t nchan, uint32_t chan, bool destructive)
{
	string spath;
	uint32_t cnt;
	char buf[PATH_MAX+1];
	const uint32_t limit = 10000;
	string legalized;

	buf[0] = '\0';
	legalized = legalize_for_path (name);

	/* find a "version" of the file name that doesn't exist in
	   any of the possible directories.
	*/

	for (cnt = (destructive ? ++destructive_index : 1); cnt <= limit; ++cnt) {

		vector<space_and_path>::iterator i;
		uint32_t existing = 0;

		for (i = session_dirs.begin(); i != session_dirs.end(); ++i) {

			spath = (*i).path;

			spath += sound_dir (false);

			if (destructive) {
				if (nchan < 2) {
					snprintf (buf, sizeof(buf), "%s/T%04d-%s.wav", spath.c_str(), cnt, legalized.c_str());
				} else if (nchan == 2) {
					if (chan == 0) {
						snprintf (buf, sizeof(buf), "%s/T%04d-%s%%L.wav", spath.c_str(), cnt, legalized.c_str());
					} else {
						snprintf (buf, sizeof(buf), "%s/T%04d-%s%%R.wav", spath.c_str(), cnt, legalized.c_str());
					}
				} else if (nchan < 26) {
					snprintf (buf, sizeof(buf), "%s/T%04d-%s%%%c.wav", spath.c_str(), cnt, legalized.c_str(), 'a' + chan);
				} else {
					snprintf (buf, sizeof(buf), "%s/T%04d-%s.wav", spath.c_str(), cnt, legalized.c_str());
				}

			} else {

				spath += '/';
				spath += legalized;

				if (nchan < 2) {
					snprintf (buf, sizeof(buf), "%s-%u.wav", spath.c_str(), cnt);
				} else if (nchan == 2) {
					if (chan == 0) {
						snprintf (buf, sizeof(buf), "%s-%u%%L.wav", spath.c_str(), cnt);
					} else {
						snprintf (buf, sizeof(buf), "%s-%u%%R.wav", spath.c_str(), cnt);
					}
				} else if (nchan < 26) {
					snprintf (buf, sizeof(buf), "%s-%u%%%c.wav", spath.c_str(), cnt, 'a' + chan);
				} else {
					snprintf (buf, sizeof(buf), "%s-%u.wav", spath.c_str(), cnt);
				}
			}

			if (g_file_test (buf, G_FILE_TEST_EXISTS)) {
				existing++;
			} 

		}

		if (existing == 0) {
			break;
		}

		if (cnt > limit) {
			error << string_compose(_("There are already %1 recordings for %2, which I consider too many."), limit, name) << endmsg;
			destroy ();
			throw failed_constructor();
		}
	}

	/* we now have a unique name for the file, but figure out where to
	   actually put it.
	*/

	string foo = buf;

	spath = discover_best_sound_dir ();
	spath += '/';

	string::size_type pos = foo.find_last_of ('/');
	
	if (pos == string::npos) {
		spath += foo;
	} else {
		spath += foo.substr (pos + 1);
	}

	return spath;
}

boost::shared_ptr<AudioFileSource>
Session::create_audio_source_for_session (AudioDiskstream& ds, uint32_t chan, bool destructive)
{
	string spath = audio_path_from_name (ds.name(), ds.n_channels(), chan, destructive);
	return boost::dynamic_pointer_cast<AudioFileSource> (SourceFactory::createWritable (*this, spath, destructive, frame_rate()));
}

/* Playlist management */

boost::shared_ptr<Playlist>
Session::playlist_by_name (string name)
{
	Glib::Mutex::Lock lm (playlist_lock);
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

	return boost::shared_ptr<Playlist>();
}

void
Session::add_playlist (boost::shared_ptr<Playlist> playlist)
{
	if (playlist->hidden()) {
		return;
	}

	{ 
		Glib::Mutex::Lock lm (playlist_lock);
		if (find (playlists.begin(), playlists.end(), playlist) == playlists.end()) {
			playlists.insert (playlists.begin(), playlist);
			playlist->InUse.connect (sigc::bind (mem_fun (*this, &Session::track_playlist), boost::weak_ptr<Playlist>(playlist)));
			playlist->GoingAway.connect (sigc::bind (mem_fun (*this, &Session::remove_playlist), boost::weak_ptr<Playlist>(playlist)));
		}
	}

	set_dirty();

	PlaylistAdded (playlist); /* EMIT SIGNAL */
}

void
Session::get_playlists (vector<boost::shared_ptr<Playlist> >& s)
{
	{ 
		Glib::Mutex::Lock lm (playlist_lock);
		for (PlaylistList::iterator i = playlists.begin(); i != playlists.end(); ++i) {
			s.push_back (*i);
		}
		for (PlaylistList::iterator i = unused_playlists.begin(); i != unused_playlists.end(); ++i) {
			s.push_back (*i);
		}
	}
}

void
Session::track_playlist (bool inuse, boost::weak_ptr<Playlist> wpl)
{
	boost::shared_ptr<Playlist> pl(wpl.lock());

	if (!pl) {
		return;
	}

	PlaylistList::iterator x;

	if (pl->hidden()) {
		/* its not supposed to be visible */
		return;
	}

	{ 
		Glib::Mutex::Lock lm (playlist_lock);

		if (!inuse) {

			unused_playlists.insert (pl);
			
			if ((x = playlists.find (pl)) != playlists.end()) {
				playlists.erase (x);
			}

			
		} else {

			playlists.insert (pl);
			
			if ((x = unused_playlists.find (pl)) != unused_playlists.end()) {
				unused_playlists.erase (x);
			}
		}
	}
}

void
Session::remove_playlist (boost::weak_ptr<Playlist> weak_playlist)
{
	if (_state_of_the_state & Deletion) {
		return;
	}

	boost::shared_ptr<Playlist> playlist (weak_playlist.lock());

	if (!playlist) {
		return;
	}

	{ 
		Glib::Mutex::Lock lm (playlist_lock);

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
Session::set_audition (boost::shared_ptr<Region> r)
{
	pending_audition_region = r;
	post_transport_work = PostTransportWork (post_transport_work | PostTransportAudition);
	schedule_butler_transport_work ();
}

void
Session::audition_playlist ()
{
	Event* ev = new Event (Event::Audition, Event::Add, Event::Immediate, 0, 0.0);
	ev->region.reset ();
	queue_event (ev);
}

void
Session::non_realtime_set_audition ()
{
	if (!pending_audition_region) {
		auditioner->audition_current_playlist ();
	} else {
		auditioner->audition_region (pending_audition_region);
		pending_audition_region.reset ();
	}
	AuditionActive (true); /* EMIT SIGNAL */
}

void
Session::audition_region (boost::shared_ptr<Region> r)
{
	Event* ev = new Event (Event::Audition, Event::Add, Event::Immediate, 0, 0.0);
	ev->region = r;
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
Session::RoutePublicOrderSorter::operator() (boost::shared_ptr<Route> a, boost::shared_ptr<Route> b)
{
	return a->order_key(N_("signal")) < b->order_key(N_("signal"));
}

void
Session::remove_empty_sounds ()
{
	PathScanner scanner;

	vector<string *>* possible_audiofiles = scanner (sound_dir(), "\\.(wav|aiff|caf|w64|L|R)$", false, true);
	
	Glib::Mutex::Lock lm (audio_source_lock);
	
	regex_t compiled_tape_track_pattern;
	int err;

	if ((err = regcomp (&compiled_tape_track_pattern, "/T[0-9][0-9][0-9][0-9]-", REG_EXTENDED|REG_NOSUB))) {

		char msg[256];
		
		regerror (err, &compiled_tape_track_pattern, msg, sizeof (msg));
		
		error << string_compose (_("Cannot compile tape track regexp for use (%1)"), msg) << endmsg;
		return;
	}

	for (vector<string *>::iterator i = possible_audiofiles->begin(); i != possible_audiofiles->end(); ++i) {
		
		/* never remove files that appear to be a tape track */

		if (regexec (&compiled_tape_track_pattern, (*i)->c_str(), 0, 0, 0) == 0) {
			delete *i;
			continue;
		}
			
		if (AudioFileSource::is_empty (*this, *(*i))) {

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

void
Session::set_all_solo (bool yn)
{
	shared_ptr<RouteList> r = routes.reader ();
	
	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if (!(*i)->hidden()) {
			(*i)->set_solo (yn, this);
		}
	}

	set_dirty();
}
		
void
Session::set_all_mute (bool yn)
{
	shared_ptr<RouteList> r = routes.reader ();
	
	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if (!(*i)->hidden()) {
			(*i)->set_mute (yn, this);
		}
	}

	set_dirty();
}
		
uint32_t
Session::n_diskstreams () const
{
	uint32_t n = 0;

	boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();

	for (DiskstreamList::const_iterator i = dsl->begin(); i != dsl->end(); ++i) {
		if (!(*i)->hidden()) {
			n++;
		}
	}
	return n;
}

void
Session::graph_reordered ()
{
	/* don't do this stuff if we are setting up connections
	   from a set_state() call or creating new tracks.
	*/

	if (_state_of_the_state & InitialConnecting) {
		return;
	}

	/* every track/bus asked for this to be handled but it was deferred because
	   we were connecting. do it now.
	*/

	request_input_change_handling ();

	resort_routes ();

	/* force all diskstreams to update their capture offset values to 
	   reflect any changes in latencies within the graph.
	*/
	
	boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();

	for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
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
	shared_ptr<RouteList> r = routes.reader ();
	
	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		AudioTrack* at;

		if ((at = dynamic_cast<AudioTrack*>((*i).get())) != 0) {
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

	redirect->GoingAway.connect (sigc::bind (mem_fun (*this, &Session::remove_redirect), redirect));

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
			list<PortInsert*>::iterator x = find (_port_inserts.begin(), _port_inserts.end(), port_insert);
			if (x != _port_inserts.end()) {
				insert_bitset[port_insert->bit_slot()] = false;
				_port_inserts.erase (x);
			}
		} else if ((plugin_insert = dynamic_cast<PluginInsert *> (insert)) != 0) {
			_plugin_inserts.remove (plugin_insert);
		} else {
			fatal << string_compose (_("programming error: %1"),
						 X_("unknown type of Insert deleted!")) 
			      << endmsg;
			/*NOTREACHED*/
		}
	} else if ((send = dynamic_cast<Send *> (redirect)) != 0) {
		list<Send*>::iterator x = find (_sends.begin(), _sends.end(), send);
		if (x != _sends.end()) {
			send_bitset[send->bit_slot()] = false;
			_sends.erase (x);
		}
	} else {
		fatal << _("programming error: unknown type of Redirect deleted!") << endmsg;
		/*NOTREACHED*/
	}

	set_dirty();
}

nframes_t
Session::available_capture_duration ()
{
	float sample_bytes_on_disk = 4.0; // keep gcc happy

	switch (Config->get_native_file_data_format()) {
	case FormatFloat:
		sample_bytes_on_disk = 4.0;
		break;

	case FormatInt24:
		sample_bytes_on_disk = 3.0;
		break;

	case FormatInt16:
		sample_bytes_on_disk = 2.0;
		break;

	default: 
		/* impossible, but keep some gcc versions happy */
		fatal << string_compose (_("programming error: %1"),
					 X_("illegal native file data format"))
		      << endmsg;
		/*NOTREACHED*/
	}

	double scale = 4096.0 / sample_bytes_on_disk;

	if (_total_free_4k_blocks * scale > (double) max_frames) {
		return max_frames;
	}
	
	return (nframes_t) floor (_total_free_4k_blocks * scale);
}

void
Session::add_connection (ARDOUR::Connection* connection)
{
	{
		Glib::Mutex::Lock guard (connection_lock);
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
		Glib::Mutex::Lock guard (connection_lock);
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
	Glib::Mutex::Lock lm (connection_lock);

	for (ConnectionList::const_iterator i = _connections.begin(); i != _connections.end(); ++i) {
		if ((*i)->name() == name) {
			return* i;
		}
	}

	return 0;
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
		posix_memalign((void **)&p,CPU_CACHE_ALIGN,current_block_size * sizeof(Sample));
#endif			
		_passthru_buffers.push_back (p);

		*p = 0;
		
#ifdef NO_POSIX_MEMALIGN
		p =  (Sample *) malloc(current_block_size * sizeof(Sample));
#else
		posix_memalign((void **)&p,CPU_CACHE_ALIGN,current_block_size * 4);
#endif			
		memset (p, 0, sizeof (Sample) * current_block_size);
		_silent_buffers.push_back (p);

		*p = 0;
		
#ifdef NO_POSIX_MEMALIGN
		p =  (Sample *) malloc(current_block_size * sizeof(Sample));
#else
		posix_memalign((void **)&p,CPU_CACHE_ALIGN,current_block_size * sizeof(Sample));
#endif			
		memset (p, 0, sizeof (Sample) * current_block_size);
		_send_buffers.push_back (p);
		
	}
	allocate_pan_automation_buffers (current_block_size, howmany, false);
}

uint32_t
Session::next_insert_id ()
{
	/* this doesn't really loop forever. just think about it */

	while (true) {
		for (boost::dynamic_bitset<uint32_t>::size_type n = 0; n < insert_bitset.size(); ++n) {
			if (!insert_bitset[n]) {
				insert_bitset[n] = true;
				return n;
				
			}
		}
		
		/* none available, so resize and try again */

		insert_bitset.resize (insert_bitset.size() + 16, false);
	}
}

uint32_t
Session::next_send_id ()
{
	/* this doesn't really loop forever. just think about it */

	while (true) {
		for (boost::dynamic_bitset<uint32_t>::size_type n = 0; n < send_bitset.size(); ++n) {
			if (!send_bitset[n]) {
				send_bitset[n] = true;
				return n;
				
			}
		}
		
		/* none available, so resize and try again */

		send_bitset.resize (send_bitset.size() + 16, false);
	}
}

void
Session::mark_send_id (uint32_t id)
{
	if (id >= send_bitset.size()) {
		send_bitset.resize (id+16, false);
	}
	if (send_bitset[id]) {
		warning << string_compose (_("send ID %1 appears to be in use already"), id) << endmsg;
	}
	send_bitset[id] = true;
}

void
Session::mark_insert_id (uint32_t id)
{
	if (id >= insert_bitset.size()) {
		insert_bitset.resize (id+16, false);
	}
	if (insert_bitset[id]) {
		warning << string_compose (_("insert ID %1 appears to be in use already"), id) << endmsg;
	}
	insert_bitset[id] = true;
}

/* Named Selection management */

NamedSelection *
Session::named_selection_by_name (string name)
{
	Glib::Mutex::Lock lm (named_selection_lock);
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
		Glib::Mutex::Lock lm (named_selection_lock);
		named_selections.insert (named_selections.begin(), named_selection);
	}

	for (list<boost::shared_ptr<Playlist> >::iterator i = named_selection->playlists.begin(); i != named_selection->playlists.end(); ++i) {
		add_playlist (*i);
	}

	set_dirty();

	NamedSelectionAdded (); /* EMIT SIGNAL */
}

void
Session::remove_named_selection (NamedSelection* named_selection)
{
	bool removed = false;

	{ 
		Glib::Mutex::Lock lm (named_selection_lock);

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
	boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();

	for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
		(*i)->reset_write_sources (false);
	}
}

bool
Session::route_name_unique (string n) const
{
	shared_ptr<RouteList> r = routes.reader ();
	
	for (RouteList::const_iterator i = r->begin(); i != r->end(); ++i) {
		if ((*i)->name() == n) {
			return false;
		}
	}
	
	return true;
}

uint32_t
Session::n_playlists () const
{
	Glib::Mutex::Lock lm (playlist_lock);
	return playlists.size();
}

void
Session::allocate_pan_automation_buffers (nframes_t nframes, uint32_t howmany, bool force)
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

int
Session::freeze (InterThreadInfo& itt)
{
	shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {

		AudioTrack *at;

		if ((at = dynamic_cast<AudioTrack*>((*i).get())) != 0) {
			/* XXX this is wrong because itt.progress will keep returning to zero at the start
			   of every track.
			*/
			at->freeze (itt);
		}
	}

	return 0;
}

int
Session::write_one_audio_track (AudioTrack& track, nframes_t start, nframes_t len, 	
			       bool overwrite, vector<boost::shared_ptr<AudioSource> >& srcs, InterThreadInfo& itt)
{
	int ret = -1;
	boost::shared_ptr<Playlist> playlist;
	boost::shared_ptr<AudioFileSource> fsource;
	uint32_t x;
	char buf[PATH_MAX+1];
	string dir;
	uint32_t nchans;
	nframes_t position;
	nframes_t this_chunk;
	nframes_t to_do;
	vector<Sample*> buffers;

	// any bigger than this seems to cause stack overflows in called functions
	const nframes_t chunk_size = (128 * 1024)/4;

	g_atomic_int_set (&processing_prohibited, 1);
	
	/* call tree *MUST* hold route_lock */
	
	if ((playlist = track.diskstream()->playlist()) == 0) {
		goto out;
	}

	/* external redirects will be a problem */

	if (track.has_external_redirects()) {
		goto out;
	}

	nchans = track.audio_diskstream()->n_channels();
	
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
			fsource = boost::dynamic_pointer_cast<AudioFileSource> (SourceFactory::createWritable (*this, buf, false, frame_rate()));
		}
		
		catch (failed_constructor& err) {
			error << string_compose (_("cannot create new audio file \"%1\" for %2"), buf, track.name()) << endmsg;
			goto out;
		}

		srcs.push_back (fsource);
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
		posix_memalign((void **)&b,4096,chunk_size * sizeof(Sample));
#endif			
		buffers.push_back (b);
	}

	for (vector<boost::shared_ptr<AudioSource> >::iterator src=srcs.begin(); src != srcs.end(); ++src) {
		(*src)->prepare_for_peakfile_writes ();
	}
			
	while (to_do && !itt.cancel) {
		
		this_chunk = min (to_do, chunk_size);
		
		if (track.export_stuff (buffers, nchans, start, this_chunk)) {
			goto out;
		}

		uint32_t n = 0;
		for (vector<boost::shared_ptr<AudioSource> >::iterator src=srcs.begin(); src != srcs.end(); ++src, ++n) {
			boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource>(*src);
			
			if (afs) {
				if (afs->write (buffers[n], this_chunk) != this_chunk) {
					goto out;
				}
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
		
		for (vector<boost::shared_ptr<AudioSource> >::iterator src=srcs.begin(); src != srcs.end(); ++src) {
			boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource>(*src);

			if (afs) {
				afs->update_header (position, *xnow, now);
				afs->flush_header ();
			}
		}
		
		/* construct a region to represent the bounced material */

		boost::shared_ptr<Region> aregion = RegionFactory::create (srcs, 0, srcs.front()->length(), 
									   region_name_from_path (srcs.front()->name(), true));

		ret = 0;
	}
		
  out:
	if (ret) {
		for (vector<boost::shared_ptr<AudioSource> >::iterator src = srcs.begin(); src != srcs.end(); ++src) {
			boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource>(*src);

			if (afs) {
				afs->mark_for_remove ();
			}

			(*src)->drop_references ();
		}

	} else {
		for (vector<boost::shared_ptr<AudioSource> >::iterator src = srcs.begin(); src != srcs.end(); ++src) {
			(*src)->done_with_peakfile_writes ();
		}
	}

	for (vector<Sample*>::iterator i = buffers.begin(); i != buffers.end(); ++i) {
		free(*i);
	}

	g_atomic_int_set (&processing_prohibited, 0);

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
	shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::const_iterator i = r->begin(); i != r->end(); ++i) {
		if (dynamic_cast<AudioTrack*> ((*i).get())) {
			++n;
		}
	}

	return n;
}

uint32_t 
Session::nbusses () const
{
	uint32_t n = 0;
	shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::const_iterator i = r->begin(); i != r->end(); ++i) {
		if (dynamic_cast<AudioTrack*> ((*i).get()) == 0) {
			++n;
		}
	}

	return n;
}

void
Session::add_automation_list(AutomationList *al)
{
	automation_lists[al->id()] = al;
}

nframes_t
Session::compute_initial_length ()
{
	return _engine.frame_rate() * 60 * 5;
}

void
Session::sync_order_keys ()
{
	if (!Config->get_sync_all_route_ordering()) {
		/* leave order keys as they are */
		return;
	}

	boost::shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		(*i)->sync_order_keys ();
	}

	Route::SyncOrderKeys (); // EMIT SIGNAL
}
