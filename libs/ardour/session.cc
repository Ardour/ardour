/*
    Copyright (C) 1999-2010 Paul Davis

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

#define __STDC_LIMIT_MACROS
#include <stdint.h>

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

#include <glibmm/thread.h>
#include <glibmm/miscutils.h>
#include <glibmm/fileutils.h>

#include <boost/algorithm/string/erase.hpp>

#include "pbd/error.h"
#include "pbd/boost_debug.h"
#include "pbd/pathscanner.h"
#include "pbd/stl_delete.h"
#include "pbd/basename.h"
#include "pbd/stacktrace.h"
#include "pbd/file_utils.h"
#include "pbd/convert.h"

#include "ardour/amp.h"
#include "ardour/analyser.h"
#include "ardour/audio_buffer.h"
#include "ardour/audio_diskstream.h"
#include "ardour/audio_port.h"
#include "ardour/audio_track.h"
#include "ardour/audioengine.h"
#include "ardour/audiofilesource.h"
#include "ardour/audioplaylist.h"
#include "ardour/audioregion.h"
#include "ardour/auditioner.h"
#include "ardour/buffer_manager.h"
#include "ardour/buffer_set.h"
#include "ardour/bundle.h"
#include "ardour/butler.h"
#include "ardour/click.h"
#include "ardour/configuration.h"
#include "ardour/crossfade.h"
#include "ardour/cycle_timer.h"
#include "ardour/data_type.h"
#include "ardour/debug.h"
#include "ardour/filename_extensions.h"
#include "ardour/internal_send.h"
#include "ardour/io_processor.h"
#include "ardour/midi_diskstream.h"
#include "ardour/midi_playlist.h"
#include "ardour/midi_region.h"
#include "ardour/midi_track.h"
#include "ardour/midi_ui.h"
#include "ardour/named_selection.h"
#include "ardour/process_thread.h"
#include "ardour/playlist.h"
#include "ardour/plugin_insert.h"
#include "ardour/port_insert.h"
#include "ardour/processor.h"
#include "ardour/rc_configuration.h"
#include "ardour/recent_sessions.h"
#include "ardour/region_factory.h"
#include "ardour/return.h"
#include "ardour/route_group.h"
#include "ardour/send.h"
#include "ardour/session.h"
#include "ardour/session_directory.h"
#include "ardour/session_directory.h"
#include "ardour/session_metadata.h"
#include "ardour/session_playlists.h"
#include "ardour/slave.h"
#include "ardour/smf_source.h"
#include "ardour/source_factory.h"
#include "ardour/tape_file_matcher.h"
#include "ardour/tempo.h"
#include "ardour/utils.h"
#include "ardour/graph.h"

#include "midi++/port.h"
#include "midi++/mmc.h"
#include "midi++/manager.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using boost::shared_ptr;
using boost::weak_ptr;

bool Session::_disable_all_loaded_plugins = false;

PBD::Signal1<void,std::string> Session::Dialog;
PBD::Signal0<int> Session::AskAboutPendingState;
PBD::Signal2<int,nframes_t,nframes_t> Session::AskAboutSampleRateMismatch;
PBD::Signal0<void> Session::SendFeedback;

PBD::Signal0<void> Session::TimecodeOffsetChanged;
PBD::Signal0<void> Session::StartTimeChanged;
PBD::Signal0<void> Session::EndTimeChanged;
PBD::Signal0<void> Session::AutoBindingOn;
PBD::Signal0<void> Session::AutoBindingOff;
PBD::Signal2<void,std::string, std::string> Session::Exported;
PBD::Signal1<int,boost::shared_ptr<Playlist> > Session::AskAboutPlaylistDeletion;

static void clean_up_session_event (SessionEvent* ev) { delete ev; }
const SessionEvent::RTeventCallback Session::rt_cleanup (clean_up_session_event);

Session::Session (AudioEngine &eng,
		  const string& fullpath,
		  const string& snapshot_name,
                  BusProfile* bus_profile,
		  string mix_template)

	: _engine (eng)
        , _target_transport_speed (0.0)
        , _requested_return_frame (-1)
        , _session_dir (new SessionDirectory(fullpath))
        , state_tree (0)
        , _butler (new Butler (*this))
        , _post_transport_work (0)
        , _send_timecode_update (false)
        , _all_route_group (new RouteGroup (*this, "all"))
        , route_graph (new Graph(*this))
        , routes (new RouteList)
        , _total_free_4k_blocks (0)
        , _bundles (new BundleList)
        , _bundle_xml_node (0)
        , _click_io ((IO*) 0)
        , click_data (0)
        , click_emphasis_data (0)
        , main_outs (0)
        , _metadata (new SessionMetadata())
        , _have_rec_enabled_track (false)
        , _suspend_timecode_transmission (0)
{
	_locations = new Locations (*this);
		
	playlists.reset (new SessionPlaylists);

        _all_route_group->set_active (true, this);

	interpolation.add_channel_to (0, 0);

	if (!eng.connected()) {
		throw failed_constructor();
	}

	n_physical_outputs = _engine.n_physical_outputs ();
	n_physical_inputs =  _engine.n_physical_inputs ();

	first_stage_init (fullpath, snapshot_name);

        _is_new = !Glib::file_test (_path, Glib::FileTest (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR));

	if (_is_new) {
		if (create (mix_template, bus_profile)) {
			destroy ();
			throw failed_constructor ();
		}
        }

	if (second_stage_init ()) {
		destroy ();
		throw failed_constructor ();
	}

	store_recent_sessions(_name, _path);

	bool was_dirty = dirty();

	_state_of_the_state = StateOfTheState (_state_of_the_state & ~Dirty);

	Config->ParameterChanged.connect_same_thread (*this, boost::bind (&Session::config_changed, this, _1, false));
	config.ParameterChanged.connect_same_thread (*this, boost::bind (&Session::config_changed, this, _1, true));

	if (was_dirty) {
		DirtyChanged (); /* EMIT SIGNAL */
	}

        _is_new = false;
}

Session::~Session ()
{
	destroy ();
}

void
Session::destroy ()
{
	vector<void*> debug_pointers;

	/* if we got to here, leaving pending capture state around
	   is a mistake.
	*/

	remove_pending_capture_state ();

	_state_of_the_state = StateOfTheState (CannotSave|Deletion);

	_engine.remove_session ();

	/* clear history so that no references to objects are held any more */

	_history.clear ();

	/* clear state tree so that no references to objects are held any more */

	delete state_tree;

        /* remove all stubfiles that might still be lurking */

        cleanup_stubfiles ();

	/* reset dynamic state version back to default */

	Stateful::loading_state_version = 0;

	_butler->drop_references ();
	delete _butler;
	delete midi_control_ui;
        delete _all_route_group;

	if (click_data != default_click) {
		delete [] click_data;
	}

	if (click_emphasis_data != default_click_emphasis) {
		delete [] click_emphasis_data;
	}

	clear_clicks ();

	/* clear out any pending dead wood from RCU managed objects */

	routes.flush ();
	_bundles.flush ();
	
	AudioDiskstream::free_working_buffers();

	/* tell everyone who is still standing that we're about to die */
	drop_references ();

	/* tell everyone to drop references and delete objects as we go */

	DEBUG_TRACE (DEBUG::Destruction, "delete named selections\n");
	named_selections.clear ();

	DEBUG_TRACE (DEBUG::Destruction, "delete regions\n");
	RegionFactory::delete_all_regions ();

	DEBUG_TRACE (DEBUG::Destruction, "delete routes\n");

	/* reset these three references to special routes before we do the usual route delete thing */

	auditioner.reset ();
	_master_out.reset ();
	_monitor_out.reset ();

	{
		RCUWriter<RouteList> writer (routes);
		boost::shared_ptr<RouteList> r = writer.get_copy ();

		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			DEBUG_TRACE(DEBUG::Destruction, string_compose ("Dropping for route %1 ; pre-ref = %2\n", (*i)->name(), (*i).use_count()));
			(*i)->drop_references ();
		}

		r->clear ();
		/* writer goes out of scope and updates master */
	}
	routes.flush ();

	boost::shared_ptr<RouteList> r = routes.reader ();

	DEBUG_TRACE (DEBUG::Destruction, "delete sources\n");
	for (SourceMap::iterator i = sources.begin(); i != sources.end(); ++i) {
		DEBUG_TRACE(DEBUG::Destruction, string_compose ("Dropping for source %1 ; pre-ref = %2\n", i->second->path(), i->second.use_count()));
		i->second->drop_references ();
	}

	sources.clear ();

	DEBUG_TRACE (DEBUG::Destruction, "delete route groups\n");
	for (list<RouteGroup *>::iterator i = _route_groups.begin(); i != _route_groups.end(); ++i) {
		
		delete *i;
	}

	Crossfade::set_buffer_size (0);

	/* not strictly necessary, but doing it here allows the shared_ptr debugging to work */
	playlists.reset ();

	boost_debug_list_ptrs ();

	delete _locations;

	DEBUG_TRACE (DEBUG::Destruction, "Session::destroy() done\n");
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
		_worst_output_latency = max (_worst_output_latency, (*i)->output()->latency());
		_worst_input_latency = max (_worst_input_latency, (*i)->input()->latency());
	}
}

void
Session::when_engine_running ()
{
	string first_physical_output;

	BootMessage (_("Set block size and sample rate"));

	set_block_size (_engine.frames_per_cycle());
	set_frame_rate (_engine.frame_rate());

	BootMessage (_("Using configuration"));

	boost::function<void (std::string)> ff (boost::bind (&Session::config_changed, this, _1, false));
	boost::function<void (std::string)> ft (boost::bind (&Session::config_changed, this, _1, true));

	Config->map_parameters (ff);
	config.map_parameters (ft);

	/* every time we reconnect, recompute worst case output latencies */

	_engine.Running.connect_same_thread (*this, boost::bind (&Session::set_worst_io_latencies, this));

	if (synced_to_jack()) {
		_engine.transport_stop ();
	}

	if (config.get_jack_time_master()) {
		_engine.transport_locate (_transport_frame);
	}

	_clicking = false;

	try {
		XMLNode* child = 0;

		_click_io.reset (new ClickIO (*this, "click"));

		if (state_tree && (child = find_named_node (*state_tree->root(), "Click")) != 0) {

			/* existing state for Click */
			int c;

			if (Stateful::loading_state_version < 3000) {
				c = _click_io->set_state_2X (*child->children().front(), Stateful::loading_state_version, false);
			} else {
				c = _click_io->set_state (*child->children().front(), Stateful::loading_state_version);
			}
					

			if (c == 0) {
				_clicking = Config->get_clicking ();
				
			} else {
				
				error << _("could not setup Click I/O") << endmsg;
				_clicking = false;
			}


		} else {

			/* default state for Click: dual-mono to first 2 physical outputs */

			vector<string> outs;
			_engine.get_physical_outputs (DataType::AUDIO, outs);

			for (uint32_t physport = 0; physport < 2; ++physport) {
				if (outs.size() > physport) {
					if (_click_io->add_port (outs[physport], this)) {
						// relax, even though its an error
					}
				}
			}
			
			if (_click_io->n_ports () > ChanCount::ZERO) {
				_clicking = Config->get_clicking ();
			}
		}
	}
	
	catch (failed_constructor& err) {
		error << _("cannot setup Click I/O") << endmsg;
	}

	BootMessage (_("Compute I/O Latencies"));

	set_worst_io_latencies ();

	if (_clicking) {
		// XXX HOW TO ALERT UI TO THIS ? DO WE NEED TO?
	}

	BootMessage (_("Set up standard connections"));

	vector<string> inputs[DataType::num_types];
	vector<string> outputs[DataType::num_types];
	for (uint32_t i = 0; i < DataType::num_types; ++i) {
		_engine.get_physical_inputs (DataType (DataType::Symbol (i)), inputs[i]);
		_engine.get_physical_outputs (DataType (DataType::Symbol (i)), outputs[i]);
	}

	/* Create a set of Bundle objects that map
	   to the physical I/O currently available.  We create both
	   mono and stereo bundles, so that the common cases of mono
	   and stereo tracks get bundles to put in their mixer strip
	   in / out menus.  There may be a nicer way of achieving that;
	   it doesn't really scale that well to higher channel counts
	*/

	/* mono output bundles */

	for (uint32_t np = 0; np < outputs[DataType::AUDIO].size(); ++np) {
		char buf[32];
		snprintf (buf, sizeof (buf), _("out %" PRIu32), np+1);

		shared_ptr<Bundle> c (new Bundle (buf, true));
		c->add_channel (_("mono"), DataType::AUDIO);
		c->set_port (0, outputs[DataType::AUDIO][np]);

		add_bundle (c);
	}

	/* stereo output bundles */

	for (uint32_t np = 0; np < outputs[DataType::AUDIO].size(); np += 2) {
		if (np + 1 < outputs[DataType::AUDIO].size()) {
			char buf[32];
			snprintf (buf, sizeof(buf), _("out %" PRIu32 "+%" PRIu32), np + 1, np + 2);
			shared_ptr<Bundle> c (new Bundle (buf, true));
			c->add_channel (_("L"), DataType::AUDIO);
			c->set_port (0, outputs[DataType::AUDIO][np]);
			c->add_channel (_("R"), DataType::AUDIO);
			c->set_port (1, outputs[DataType::AUDIO][np + 1]);

			add_bundle (c);
		}
	}

	/* mono input bundles */

	for (uint32_t np = 0; np < inputs[DataType::AUDIO].size(); ++np) {
		char buf[32];
		snprintf (buf, sizeof (buf), _("in %" PRIu32), np+1);

		shared_ptr<Bundle> c (new Bundle (buf, false));
		c->add_channel (_("mono"), DataType::AUDIO);
		c->set_port (0, inputs[DataType::AUDIO][np]);

		add_bundle (c);
	}

	/* stereo input bundles */

	for (uint32_t np = 0; np < inputs[DataType::AUDIO].size(); np += 2) {
		if (np + 1 < inputs[DataType::AUDIO].size()) {
			char buf[32];
			snprintf (buf, sizeof(buf), _("in %" PRIu32 "+%" PRIu32), np + 1, np + 2);

			shared_ptr<Bundle> c (new Bundle (buf, false));
			c->add_channel (_("L"), DataType::AUDIO);
			c->set_port (0, inputs[DataType::AUDIO][np]);
			c->add_channel (_("R"), DataType::AUDIO);
			c->set_port (1, inputs[DataType::AUDIO][np + 1]);

			add_bundle (c);
		}
	}

	/* MIDI input bundles */

	for (uint32_t np = 0; np < inputs[DataType::MIDI].size(); ++np) {
		string n = inputs[DataType::MIDI][np];
		boost::erase_first (n, X_("alsa_pcm:"));
		
		shared_ptr<Bundle> c (new Bundle (n, false));
		c->add_channel ("", DataType::MIDI);
		c->set_port (0, inputs[DataType::MIDI][np]);
		add_bundle (c);
	}
		
	/* MIDI output bundles */

	for (uint32_t np = 0; np < outputs[DataType::MIDI].size(); ++np) {
		string n = outputs[DataType::MIDI][np];
		boost::erase_first (n, X_("alsa_pcm:"));

		shared_ptr<Bundle> c (new Bundle (n, true));
		c->add_channel ("", DataType::MIDI);
		c->set_port (0, outputs[DataType::MIDI][np]);
		add_bundle (c);
	}

	BootMessage (_("Setup signal flow and plugins"));

	hookup_io ();

	if (_is_new && !no_auto_connect()) {

                /* don't connect the master bus outputs if there is a monitor bus */

		if (_master_out && Config->get_auto_connect_standard_busses() && !_monitor_out) {

			/* if requested auto-connect the outputs to the first N physical ports.
			 */

			uint32_t limit = _master_out->n_outputs().n_total();

			for (uint32_t n = 0; n < limit; ++n) {
				Port* p = _master_out->output()->nth (n);
				string connect_to;
				if (outputs[p->type()].size() > n) {
					connect_to = outputs[p->type()][n];
				}

				if (!connect_to.empty() && p->connected_to (connect_to) == false) {
					if (_master_out->output()->connect (p, connect_to, this)) {
						error << string_compose (_("cannot connect master output %1 to %2"), n, connect_to)
						      << endmsg;
						break;
					}
				}
			}
		}

		if (_monitor_out) {

			/* AUDIO ONLY as of june 29th 2009, because listen semantics for anything else
			   are undefined, at best.
			 */

			/* control out listens to master bus (but ignores it
			   under some conditions)
			*/

			uint32_t limit = _monitor_out->n_inputs().n_audio();

			if (_master_out) {
				for (uint32_t n = 0; n < limit; ++n) {
					AudioPort* p = _monitor_out->input()->ports().nth_audio_port (n);
					AudioPort* o = _master_out->output()->ports().nth_audio_port (n);

					if (o) {
						string connect_to = o->name();
						if (_monitor_out->input()->connect (p, connect_to, this)) {
							error << string_compose (_("cannot connect control input %1 to %2"), n, connect_to)
							      << endmsg;
							break;
						}
					}
				}
			}

			/* if control out is not connected, connect control out to physical outs
			*/

			if (!_monitor_out->output()->connected ()) {

				if (!Config->get_monitor_bus_preferred_bundle().empty()) {

					boost::shared_ptr<Bundle> b = bundle_by_name (Config->get_monitor_bus_preferred_bundle());

					if (b) {
						_monitor_out->output()->connect_ports_to_bundle (b, this);
					} else {
						warning << string_compose (_("The preferred I/O for the monitor bus (%1) cannot be found"),
									   Config->get_monitor_bus_preferred_bundle())
							<< endmsg;
					}

				} else {
                                        
					for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
						uint32_t mod = n_physical_outputs.get (*t);
						uint32_t limit = _monitor_out->n_outputs().get(*t);

						for (uint32_t n = 0; n < limit; ++n) {

							Port* p = _monitor_out->output()->ports().port(*t, n);
							string connect_to;
							if (outputs[*t].size() > (n % mod)) {
								connect_to = outputs[*t][n % mod];
							}

							if (!connect_to.empty()) {
								if (_monitor_out->output()->connect (p, connect_to, this)) {
									error << string_compose (
											_("cannot connect control output %1 to %2"),
											n, connect_to)
										<< endmsg;
									break;
								}
							}
						}
					}
				}
			}
		}
	}

	/* catch up on send+insert cnts */

	_state_of_the_state = StateOfTheState (_state_of_the_state & ~(CannotSave|Dirty));

	/* hook us up to the engine */

	BootMessage (_("Connect to engine"));

	_engine.set_session (this);
}

void
Session::hookup_io ()
{
	/* stop graph reordering notifications from
	   causing resorts, etc.
	*/

	_state_of_the_state = StateOfTheState (_state_of_the_state | InitialConnecting);


	if (!auditioner) {

		/* we delay creating the auditioner till now because
		   it makes its own connections to ports.
		*/

		try {
                        Auditioner* a = new Auditioner (*this);
                        if (a->init()) {
                                delete a;
                                throw failed_constructor();
                        }
                        a->use_new_diskstream ();
			auditioner.reset (a);
		}

		catch (failed_constructor& err) {
			warning << _("cannot create Auditioner: no auditioning of regions possible") << endmsg;
		}
	}

	/* load bundles, which we may have postponed earlier on */
	if (_bundle_xml_node) {
		load_bundles (*_bundle_xml_node);
		delete _bundle_xml_node;
	}

	/* Tell all IO objects to connect themselves together */

	IO::enable_connecting ();
	MIDI::Port::MakeConnections ();

	/* Now reset all panners */

	Delivery::reset_panners ();

	/* Connect tracks to monitor/listen bus if there is one.
           Note that in an existing session, the internal sends will
           already exist, but we want the routes to notice that
           they connect to the control out specifically.
         */

        if (_monitor_out) {
		boost::shared_ptr<RouteList> r = routes.reader ();
                for (RouteList::iterator x = r->begin(); x != r->end(); ++x) {
                        
                        if ((*x)->is_monitor()) {
                                
                                /* relax */
                                
                        } else if ((*x)->is_master()) {
                                
                                /* relax */
                                
                        } else {
                                
                                (*x)->listen_via (_monitor_out,
                                                  (Config->get_listen_position() == AfterFaderListen ? PostFader : PreFader),
                                                  false, false);
                        }
                }
        }

	/* Anyone who cares about input state, wake up and do something */

	IOConnectionsComplete (); /* EMIT SIGNAL */

	_state_of_the_state = StateOfTheState (_state_of_the_state & ~InitialConnecting);

	/* now handle the whole enchilada as if it was one
	   graph reorder event.
	*/

	graph_reordered ();

	/* update the full solo state, which can't be
	   correctly determined on a per-route basis, but
	   needs the global overview that only the session
	   has.
	*/

	update_route_solo_state ();
}

void
Session::playlist_length_changed ()
{
	update_session_range_location_marker ();
}

void
Session::track_playlist_changed (boost::weak_ptr<Track> wp)
{
	boost::shared_ptr<Track> track = wp.lock ();
	if (!track) {
		return;
	}
	
	boost::shared_ptr<Playlist> playlist;

	if ((playlist = track->playlist()) != 0) {
		playlist->LengthChanged.connect_same_thread (*this, boost::bind (&Session::playlist_length_changed, this));
	}

	update_session_range_location_marker ();
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

		boost::shared_ptr<RouteList> rl = routes.reader ();
		for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
			boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
			if (tr && tr->record_enabled ()) {
				//cerr << "switching to input = " << !auto_input << __FILE__ << __LINE__ << endl << endl;
				tr->monitor_input (Config->get_monitoring_model() == HardwareMonitoring && !config.get_auto_input());
			}
		}
		
	} else {
		
		boost::shared_ptr<RouteList> rl = routes.reader ();
		for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
			boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
			if (tr && tr->record_enabled ()) {
				//cerr << "switching to input = " << !Config->get_auto_input() << __FILE__ << __LINE__ << endl << endl;
				tr->monitor_input (Config->get_monitoring_model() == HardwareMonitoring);
			}
		}
	}
}

void
Session::auto_punch_start_changed (Location* location)
{
	replace_event (SessionEvent::PunchIn, location->start());

	if (get_record_enabled() && config.get_punch_in()) {
		/* capture start has been changed, so save new pending state */
		save_state ("", true);
	}
}

void
Session::auto_punch_end_changed (Location* location)
{
	nframes_t when_to_stop = location->end();
	// when_to_stop += _worst_output_latency + _worst_input_latency;
	replace_event (SessionEvent::PunchOut, when_to_stop);
}

void
Session::auto_punch_changed (Location* location)
{
	nframes_t when_to_stop = location->end();

	replace_event (SessionEvent::PunchIn, location->start());
	//when_to_stop += _worst_output_latency + _worst_input_latency;
	replace_event (SessionEvent::PunchOut, when_to_stop);
}

void
Session::auto_loop_changed (Location* location)
{
	replace_event (SessionEvent::AutoLoop, location->end(), location->start());

	if (transport_rolling() && play_loop) {


		// if (_transport_frame > location->end()) {

		if (_transport_frame < location->start() || _transport_frame > location->end()) {
			// relocate to beginning of loop
			clear_events (SessionEvent::LocateRoll);

			request_locate (location->start(), true);

		}
		else if (Config->get_seamless_loop() && !loop_changing) {

			// schedule a locate-roll to refill the diskstreams at the
			// previous loop end
			loop_changing = true;

			if (location->end() > last_loopend) {
				clear_events (SessionEvent::LocateRoll);
				SessionEvent *ev = new SessionEvent (SessionEvent::LocateRoll, SessionEvent::Add, last_loopend, last_loopend, 0, true);
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

	if ((existing = _locations->auto_punch_location()) != 0 && existing != location) {
		punch_connections.drop_connections();
		existing->set_auto_punch (false, this);
		remove_event (existing->start(), SessionEvent::PunchIn);
		clear_events (SessionEvent::PunchOut);
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

	punch_connections.drop_connections ();

	location->start_changed.connect_same_thread (punch_connections, boost::bind (&Session::auto_punch_start_changed, this, _1));
	location->end_changed.connect_same_thread (punch_connections, boost::bind (&Session::auto_punch_end_changed, this, _1));
	location->changed.connect_same_thread (punch_connections, boost::bind (&Session::auto_punch_changed, this, _1));

	location->set_auto_punch (true, this);

	auto_punch_changed (location);

	auto_punch_location_changed (location);
}

void
Session::set_auto_loop_location (Location* location)
{
	Location* existing;

	if ((existing = _locations->auto_loop_location()) != 0 && existing != location) {
		loop_connections.drop_connections ();
		existing->set_auto_loop (false, this);
		remove_event (existing->end(), SessionEvent::AutoLoop);
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

	loop_connections.drop_connections ();

	location->start_changed.connect_same_thread (loop_connections, boost::bind (&Session::auto_loop_changed, this, _1));
	location->end_changed.connect_same_thread (loop_connections, boost::bind (&Session::auto_loop_changed, this, _1));
	location->changed.connect_same_thread (loop_connections, boost::bind (&Session::auto_loop_changed, this, _1));

	location->set_auto_loop (true, this);

	/* take care of our stuff first */

	auto_loop_changed (location);

	/* now tell everyone else */

	auto_loop_location_changed (location);
}

void
Session::locations_added (Location *)
{
	set_dirty ();
}

void
Session::locations_changed ()
{
	_locations->apply (*this, &Session::handle_locations_changed);
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

		if (location->is_session_range()) {
			_session_range_location = location;
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
        while (1) {
                RecordState rs = (RecordState) g_atomic_int_get (&_record_status);
                
                if (rs == Recording) {
                        break;
                }

                if (g_atomic_int_compare_and_exchange (&_record_status, rs, Recording)) {
                        
                        _last_record_location = _transport_frame;
			MIDI::Manager::instance()->mmc()->send (MIDI::MachineControlCommand (MIDI::MachineControl::cmdRecordStrobe));
                        
                        if (Config->get_monitoring_model() == HardwareMonitoring && config.get_auto_input()) {
                                
                                boost::shared_ptr<RouteList> rl = routes.reader ();
                                for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
                                        boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
                                        if (tr && tr->record_enabled ()) {
                                                tr->monitor_input (true);
                                        }
                                }
                        }
                        
                        RecordStateChanged ();
                        break;
                }
        }
}

void
Session::disable_record (bool rt_context, bool force)
{
	RecordState rs;

	if ((rs = (RecordState) g_atomic_int_get (&_record_status)) != Disabled) {

		if ((!Config->get_latched_record_enable () && !play_loop) || force) {
			g_atomic_int_set (&_record_status, Disabled);
			MIDI::Manager::instance()->mmc()->send (MIDI::MachineControlCommand (MIDI::MachineControl::cmdRecordExit));
		} else {
			if (rs == Recording) {
				g_atomic_int_set (&_record_status, Enabled);
			}
		}

		if (Config->get_monitoring_model() == HardwareMonitoring && config.get_auto_input()) {

			boost::shared_ptr<RouteList> rl = routes.reader ();
			for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
				boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
				if (tr && tr->record_enabled ()) {
					tr->monitor_input (false);
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
	if (g_atomic_int_compare_and_exchange (&_record_status, Recording, Enabled)) {

		if (Config->get_monitoring_model() == HardwareMonitoring && config.get_auto_input()) {
			boost::shared_ptr<RouteList> rl = routes.reader ();
			for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
				boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
				if (tr && tr->record_enabled ()) {
					//cerr << "switching from input" << __FILE__ << __LINE__ << endl << endl;
					tr->monitor_input (false);
				}
			}
		}
	}
}

void
Session::maybe_enable_record ()
{
        if (_step_editors > 0) {
                return;
        }

	g_atomic_int_set (&_record_status, Enabled);

	/* this function is currently called from somewhere other than an RT thread.
	   this save_state() call therefore doesn't impact anything.
	*/

	save_state ("", true);

	if (_transport_speed) {
		if (!config.get_punch_in()) {
			enable_record ();
		}
	} else {
		MIDI::Manager::instance()->mmc()->send (MIDI::MachineControlCommand (MIDI::MachineControl::cmdRecordPause));
		RecordStateChanged (); /* EMIT SIGNAL */
	}

	set_dirty();
}

nframes64_t
Session::audible_frame () const
{
	nframes64_t ret;
	nframes64_t tf;
	nframes_t offset;

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

	ret = tf;

	if (!non_realtime_work_pending()) {

		/* MOVING */

		/* Check to see if we have passed the first guaranteed
		   audible frame past our last start position. if not,
		   return that last start point because in terms
		   of audible frames, we have not moved yet.

		   `Start position' in this context means the time we last
		   either started or changed transport direction.
		*/

		if (_transport_speed > 0.0f) {

			if (!play_loop || !have_looped) {
				if (tf < _last_roll_or_reversal_location + offset) {
					return _last_roll_or_reversal_location;
				}
			}


			/* forwards */
			ret -= offset;

		} else if (_transport_speed < 0.0f) {

			/* XXX wot? no backward looping? */

			if (tf > _last_roll_or_reversal_location - offset) {
				return _last_roll_or_reversal_location;
			} else {
				/* backwards */
				ret += offset;
			}
		}
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

	Automatable::set_automation_interval ((jack_nframes_t) ceil ((double) frames_per_second * (0.001 * Config->get_automation_interval())));

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
		current_block_size = nframes;

		ensure_buffers ();

		boost::shared_ptr<RouteList> r = routes.reader ();

		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			(*i)->set_block_size (nframes);
		}

		boost::shared_ptr<RouteList> rl = routes.reader ();
		for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
			boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
			if (tr) {
				tr->set_block_size (nframes);
			}
		}

		set_worst_io_latencies ();
	}
}

void
Session::set_default_fade (float /*steepness*/, float /*fade_msecs*/)
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
	    if (r2->feeds (r1)) {
		    return false;
	    } else if (r1->feeds (r2)) {
		    return true;
	    } else {
		    if (r1->not_fed ()) {
			    if (r2->not_fed ()) {
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

	if (r1->feeds (rbase) && rbase->feeds (r1)) {
		info << string_compose(_("feedback loop setup between %1 and %2"), r1->name(), rbase->name()) << endmsg;
		return;
	}

	/* make a copy of the existing list of routes that feed r1 */

        Route::FedBy existing (r1->fed_by());
                        
	/* for each route that feeds r1, recurse, marking it as feeding
	   rbase as well.
	*/

	for (Route::FedBy::iterator i = existing.begin(); i != existing.end(); ++i) {
		if (!(r2 = i->r.lock ())) {
			/* (*i) went away, ignore it */
			continue;
		}
		
		/* r2 is a route that feeds r1 which somehow feeds base. mark
		   base as being fed by r2
		*/

		rbase->add_fed_by (r2, i->sends_only);

		if (r2 != rbase) {

			/* 2nd level feedback loop detection. if r1 feeds or is fed by r2,
			   stop here.
			*/

			if (r1->feeds (r2) && r2->feeds (r1)) {
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

	//route_graph->dump(1);

#ifndef NDEBUG
        boost::shared_ptr<RouteList> rl = routes.reader ();
        for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
                DEBUG_TRACE (DEBUG::Graph, string_compose ("%1 fed by ...\n", (*i)->name()));
                
                const Route::FedBy& fb ((*i)->fed_by());

                for (Route::FedBy::const_iterator f = fb.begin(); f != fb.end(); ++f) {
                        boost::shared_ptr<Route> sf = f->r.lock();
                        if (sf) {
                                DEBUG_TRACE (DEBUG::Graph, string_compose ("\t%1 (sends only ? %2)\n", sf->name(), f->sends_only));
                        }
                }
        }
#endif

}
void
Session::resort_routes_using (shared_ptr<RouteList> r)
{
	RouteList::iterator i, j;

	for (i = r->begin(); i != r->end(); ++i) {

		(*i)->clear_fed_by ();

		for (j = r->begin(); j != r->end(); ++j) {

			/* although routes can feed themselves, it will
			   cause an endless recursive descent if we
			   detect it. so don't bother checking for
			   self-feeding.
			*/

			if (*j == *i) {
				continue;
			}

                        bool via_sends_only;

			if ((*j)->direct_feeds (*i, &via_sends_only)) {
				(*i)->add_fed_by (*j, via_sends_only);
			}
		}
	}

	for (i = r->begin(); i != r->end(); ++i) {
		trace_terminal (*i, *i);
	}

	RouteSorter cmp;
	r->sort (cmp);

	route_graph->rechain (r);

#ifndef NDEBUG
        DEBUG_TRACE (DEBUG::Graph, "Routes resorted, order follows:\n");
	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		DEBUG_TRACE (DEBUG::Graph, string_compose ("\t%1 signal order %2\n", 
                                                           (*i)->name(), (*i)->order_key ("signal")));
	}
#endif

}

/** Find the route name starting with \a base with the lowest \a id.
 *
 * Names are constructed like e.g. "Audio 3" for base="Audio" and id=3.
 * The available route name with the lowest ID will be used, and \a id
 * will be set to the ID.
 *
 * \return false if a route name could not be found, and \a track_name
 * and \a id do not reflect a free route name.
 */
bool
Session::find_route_name (const char* base, uint32_t& id, char* name, size_t name_len)
{
	do {
		snprintf (name, name_len, "%s %" PRIu32, base, id);

		if (route_by_name (name) == 0) {
			return true;
		}

		++id;

	} while (id < (UINT_MAX-1));

	return false;
}

void
Session::count_existing_route_channels (ChanCount& in, ChanCount& out)
{
	in  = ChanCount::ZERO;
	out = ChanCount::ZERO;
	shared_ptr<RouteList> r = routes.reader ();
	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if (!(*i)->is_hidden()) {
			in += (*i)->n_inputs();
			out	+= (*i)->n_outputs();
		}
	}
}

list<boost::shared_ptr<MidiTrack> >
Session::new_midi_track (TrackMode mode, RouteGroup* route_group, uint32_t how_many)
{
	char track_name[32];
	uint32_t track_id = 0;
	ChanCount existing_inputs;
	ChanCount existing_outputs;
	string port;
	RouteList new_routes;
	list<boost::shared_ptr<MidiTrack> > ret;
	uint32_t control_id;

	count_existing_route_channels (existing_inputs, existing_outputs);

	control_id = ntracks() + nbusses();

	while (how_many) {
		if (!find_route_name ("Midi", ++track_id, track_name, sizeof(track_name))) {
			error << "cannot find name for new midi track" << endmsg;
			goto failed;
		}

		shared_ptr<MidiTrack> track;
                
		try {
			MidiTrack* mt = new MidiTrack (*this, track_name, Route::Flag (0), mode);

                        if (mt->init ()) {
                                delete mt;
                                goto failed;
                        }

                        mt->use_new_diskstream();

			boost_debug_shared_ptr_mark_interesting (mt, "Track");
			track = boost::shared_ptr<MidiTrack>(mt);

			if (track->input()->ensure_io (ChanCount(DataType::MIDI, 1), false, this)) {
				error << "cannot configure 1 in/1 out configuration for new midi track" << endmsg;
				goto failed;
			}


			if (track->output()->ensure_io (ChanCount(DataType::MIDI, 1), false, this)) {
				error << "cannot configure 1 in/1 out configuration for new midi track" << endmsg;
				goto failed;
			}

			auto_connect_route (track, existing_inputs, existing_outputs);

			track->non_realtime_input_change();

			if (route_group) {
				route_group->add (track);
			}

			track->DiskstreamChanged.connect_same_thread (*this, boost::bind (&Session::resort_routes, this));
			track->set_remote_control_id (control_id);

			new_routes.push_back (track);
			ret.push_back (track);
		}

		catch (failed_constructor &err) {
			error << _("Session: could not create new midi track.") << endmsg;
			goto failed;
		}

		catch (AudioEngine::PortRegistrationFailure& pfe) {

			error << string_compose (_("No more JACK ports are available. You will need to stop %1 and restart JACK with ports if you need this many tracks."), PROGRAM_NAME) << endmsg;
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

/** @param connect_inputs true to connect inputs as well as outputs, false to connect just outputs */
void
Session::auto_connect_route (boost::shared_ptr<Route> route, ChanCount& existing_inputs, ChanCount& existing_outputs, bool connect_inputs)
{
	/* If both inputs and outputs are auto-connected to physical ports,
	   use the max of input and output offsets to ensure auto-connected
	   port numbers always match up (e.g. the first audio input and the
	   first audio output of the route will have the same physical
	   port number).  Otherwise just use the lowest input or output
	   offset possible.
	*/

	const bool in_out_physical =
		   (Config->get_input_auto_connect() & AutoConnectPhysical)
		&& (Config->get_output_auto_connect() & AutoConnectPhysical)
		&& connect_inputs;

	const ChanCount in_offset = in_out_physical
		? ChanCount::max(existing_inputs, existing_outputs)
		: existing_inputs;

	const ChanCount out_offset = in_out_physical
		? ChanCount::max(existing_inputs, existing_outputs)
		: existing_outputs;

	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		vector<string> physinputs;
		vector<string> physoutputs;

		_engine.get_physical_outputs (*t, physoutputs);
		_engine.get_physical_inputs (*t, physinputs);

		if (!physinputs.empty() && connect_inputs) {
			uint32_t nphysical_in = physinputs.size();
			for (uint32_t i = 0; i < route->n_inputs().get(*t) && i < nphysical_in; ++i) {
				string port;

				if (Config->get_input_auto_connect() & AutoConnectPhysical) {
					port = physinputs[(in_offset.get(*t) + i) % nphysical_in];
				}

				if (!port.empty() && route->input()->connect (
						route->input()->ports().port(*t, i), port, this)) {
					break;
				}
			}
		}

		if (!physoutputs.empty()) {
			uint32_t nphysical_out = physoutputs.size();
			for (uint32_t i = 0; i < route->n_outputs().get(*t); ++i) {
				string port;

				if (Config->get_output_auto_connect() & AutoConnectPhysical) {
					port = physoutputs[(out_offset.get(*t) + i) % nphysical_out];
				} else if (Config->get_output_auto_connect() & AutoConnectMaster) {
					if (_master_out && _master_out->n_inputs().get(*t) > 0) {
						port = _master_out->input()->ports().port(*t,
								i % _master_out->input()->n_ports().get(*t))->name();
					}
				}

				if (!port.empty() && route->output()->connect (
						route->output()->ports().port(*t, i), port, this)) {
					break;
				}
			}
		}
	}

	existing_inputs += route->n_inputs();
	existing_outputs += route->n_outputs();
}

list< boost::shared_ptr<AudioTrack> >
Session::new_audio_track (int input_channels, int output_channels, TrackMode mode, RouteGroup* route_group, uint32_t how_many)
{
	char track_name[32];
	uint32_t track_id = 0;
	ChanCount existing_inputs;
	ChanCount existing_outputs;
	string port;
	RouteList new_routes;
	list<boost::shared_ptr<AudioTrack> > ret;
	uint32_t control_id;

	count_existing_route_channels (existing_inputs, existing_outputs);

	control_id = ntracks() + nbusses() + 1;

	while (how_many) {
		if (!find_route_name ("Audio", ++track_id, track_name, sizeof(track_name))) {
			error << "cannot find name for new audio track" << endmsg;
			goto failed;
		}

		shared_ptr<AudioTrack> track;

		try {
			AudioTrack* at = new AudioTrack (*this, track_name, Route::Flag (0), mode);

                        if (at->init ()) {
                                delete at;
                                goto failed;
                        }

                        at->use_new_diskstream();

			boost_debug_shared_ptr_mark_interesting (at, "Track");
			track = boost::shared_ptr<AudioTrack>(at);

			if (track->input()->ensure_io (ChanCount(DataType::AUDIO, input_channels), false, this)) {
				error << string_compose (
							_("cannot configure %1 in/%2 out configuration for new audio track"),
							 input_channels, output_channels)
				      << endmsg;
				goto failed;
			}

			if (track->output()->ensure_io (ChanCount(DataType::AUDIO, output_channels), false, this)) {
				error << string_compose (
							_("cannot configure %1 in/%2 out configuration for new audio track"),
							 input_channels, output_channels)
				      << endmsg;
				goto failed;
			}

			auto_connect_route (track, existing_inputs, existing_outputs);

			if (route_group) {
				route_group->add (track);
			}

			track->non_realtime_input_change();

			track->DiskstreamChanged.connect_same_thread (*this, boost::bind (&Session::resort_routes, this));
			track->set_remote_control_id (control_id);
			++control_id;

			new_routes.push_back (track);
			ret.push_back (track);
		}

		catch (failed_constructor &err) {
			error << _("Session: could not create new audio track.") << endmsg;
			goto failed;
		}

		catch (AudioEngine::PortRegistrationFailure& pfe) {

			error << pfe.what() << endmsg;
			goto failed;
		}

		--how_many;
	}

  failed:
	if (!new_routes.empty()) {
		add_routes (new_routes, true);
	}

	return ret;
}

void
Session::set_remote_control_ids ()
{
	RemoteModel m = Config->get_remote_model();
	bool emit_signal = false;

	shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if (MixerOrdered == m) {
			int32_t order = (*i)->order_key(N_("signal"));
			(*i)->set_remote_control_id (order+1, false);
			emit_signal = true;
		} else if (EditorOrdered == m) {
			int32_t order = (*i)->order_key(N_("editor"));
			(*i)->set_remote_control_id (order+1, false);
			emit_signal = true;
		} else if (UserOrdered == m) {
			//do nothing ... only changes to remote id's are initiated by user
		}
	}

	if (emit_signal) {
		Route::RemoteControlIDChange();
	}
}


RouteList
Session::new_audio_route (bool aux, int input_channels, int output_channels, RouteGroup* route_group, uint32_t how_many)
{
	char bus_name[32];
	uint32_t bus_id = 0;
	ChanCount existing_inputs;
	ChanCount existing_outputs;
	string port;
	RouteList ret;
	uint32_t control_id;

	count_existing_route_channels (existing_inputs, existing_outputs);

	control_id = ntracks() + nbusses() + 1;

	while (how_many) {
		if (!find_route_name ("Bus", ++bus_id, bus_name, sizeof(bus_name))) {
			error << "cannot find name for new audio bus" << endmsg;
			goto failure;
		}

		try {
			Route* rt = new Route (*this, bus_name, Route::Flag(0), DataType::AUDIO);

                        if (rt->init ()) {
                                delete rt;
                                goto failure;
                        }

			boost_debug_shared_ptr_mark_interesting (rt, "Route");
			shared_ptr<Route> bus (rt);

			if (bus->input()->ensure_io (ChanCount(DataType::AUDIO, input_channels), false, this)) {
				error << string_compose (_("cannot configure %1 in/%2 out configuration for new audio track"),
							 input_channels, output_channels)
				      << endmsg;
				goto failure;
			}


			if (bus->output()->ensure_io (ChanCount(DataType::AUDIO, output_channels), false, this)) {
				error << string_compose (_("cannot configure %1 in/%2 out configuration for new audio track"),
							 input_channels, output_channels)
				      << endmsg;
				goto failure;
			}

			auto_connect_route (bus, existing_inputs, existing_outputs, false);

			if (route_group) {
				route_group->add (bus);
			}
			bus->set_remote_control_id (control_id);
			++control_id;

			if (aux) {
				bus->add_internal_return ();
			}

			ret.push_back (bus);
		}


		catch (failed_constructor &err) {
			error << _("Session: could not create new audio route.") << endmsg;
			goto failure;
		}

		catch (AudioEngine::PortRegistrationFailure& pfe) {
			error << pfe.what() << endmsg;
			goto failure;
		}


		--how_many;
	}

  failure:
	if (!ret.empty()) {
		add_routes (ret, true);
	}

	return ret;

}

RouteList
Session::new_route_from_template (uint32_t how_many, const std::string& template_path)
{
	char name[32];
	RouteList ret;
	uint32_t control_id;
	XMLTree tree;
	uint32_t number = 0;

	if (!tree.read (template_path.c_str())) {
		return ret;
	}

	XMLNode* node = tree.root();

	control_id = ntracks() + nbusses() + 1;

	while (how_many) {

		XMLNode node_copy (*node); // make a copy so we can change the name if we need to

		std::string node_name = IO::name_from_state (*node_copy.children().front());

		/* generate a new name by adding a number to the end of the template name */
		if (!find_route_name (node_name.c_str(), ++number, name, sizeof(name))) {
			fatal << _("Session: UINT_MAX routes? impossible!") << endmsg;
			/*NOTREACHED*/
		}

		/* set IO children to use the new name */
		XMLNodeList const & children = node_copy.children ();
		for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
			if ((*i)->name() == IO::state_node_name) {
				IO::set_name_in_state (**i, name);
			}
		}

		Track::zero_diskstream_id_in_xml (node_copy);

		try {
			shared_ptr<Route> route (XMLRouteFactory (node_copy, 3000));

			if (route == 0) {
				error << _("Session: cannot create track/bus from template description") << endmsg;
				goto out;
			}

			if (boost::dynamic_pointer_cast<Track>(route)) {
				/* force input/output change signals so that the new diskstream
				   picks up the configuration of the route. During session
				   loading this normally happens in a different way.
				*/
				route->input()->changed (IOChange (ConfigurationChanged|ConnectionsChanged), this);
				route->output()->changed (IOChange (ConfigurationChanged|ConnectionsChanged), this);
			}

			route->set_remote_control_id (control_id);
			++control_id;

			ret.push_back (route);
		}

		catch (failed_constructor &err) {
			error << _("Session: could not create new route from template") << endmsg;
			goto out;
		}

		catch (AudioEngine::PortRegistrationFailure& pfe) {
			error << pfe.what() << endmsg;
			goto out;
		}

		--how_many;
	}

  out:
	if (!ret.empty()) {
		add_routes (ret, true);
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


		/* if there is no control out and we're not in the middle of loading,
		   resort the graph here. if there is a control out, we will resort
		   toward the end of this method. if we are in the middle of loading,
		   we will resort when done.
		*/

		if (!_monitor_out && IO::connecting_legal) {
			resort_routes_using (r);
		}
	}

	for (RouteList::iterator x = new_routes.begin(); x != new_routes.end(); ++x) {

		boost::weak_ptr<Route> wpr (*x);
		boost::shared_ptr<Route> r (*x);

		r->listen_changed.connect_same_thread (*this, boost::bind (&Session::route_listen_changed, this, _1, wpr));
		r->solo_changed.connect_same_thread (*this, boost::bind (&Session::route_solo_changed, this, _1, _2, wpr));
		r->solo_isolated_changed.connect_same_thread (*this, boost::bind (&Session::route_solo_isolated_changed, this, _1, wpr));
		r->mute_changed.connect_same_thread (*this, boost::bind (&Session::route_mute_changed, this, _1));
		r->output()->changed.connect_same_thread (*this, boost::bind (&Session::set_worst_io_latencies_x, this, _1, _2));
		r->processors_changed.connect_same_thread (*this, boost::bind (&Session::route_processors_changed, this, _1));
		r->order_key_changed.connect_same_thread (*this, boost::bind (&Session::route_order_key_changed, this));

		if (r->is_master()) {
			_master_out = r;
		}

		if (r->is_monitor()) {
			_monitor_out = r;
		}

		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (r);
		if (tr) {
			tr->PlaylistChanged.connect_same_thread (*this, boost::bind (&Session::track_playlist_changed, this, boost::weak_ptr<Track> (tr)));
			track_playlist_changed (boost::weak_ptr<Track> (tr));
			tr->RecordEnableChanged.connect_same_thread (*this, boost::bind (&Session::update_have_rec_enabled_track, this));

                        boost::shared_ptr<MidiTrack> mt = boost::dynamic_pointer_cast<MidiTrack> (tr);
                        if (mt) {
                                mt->StepEditStatusChange.connect_same_thread (*this, boost::bind (&Session::step_edit_status_change, this, _1));
                        }
		}
	}

	if (_monitor_out && IO::connecting_legal) {

		for (RouteList::iterator x = new_routes.begin(); x != new_routes.end(); ++x) {
			if ((*x)->is_monitor()) {
                                /* relax */
                        } else if ((*x)->is_master()) {
                                /* relax */
			} else {
                                (*x)->listen_via (_monitor_out,
                                                  (Config->get_listen_position() == AfterFaderListen ? PostFader : PreFader),
                                                  false, false);
                        }
		}

		resort_routes ();
	}

	set_dirty();

	if (save) {
		save_state (_current_snapshot_name);
	}

	RouteAdded (new_routes); /* EMIT SIGNAL */
	Route::RemoteControlIDChange (); /* EMIT SIGNAL */
}

void
Session::globally_set_send_gains_to_zero (boost::shared_ptr<Route> dest)
{
	boost::shared_ptr<RouteList> r = routes.reader ();
	boost::shared_ptr<Send> s;

	/* only tracks */

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if (boost::dynamic_pointer_cast<Track>(*i)) {
			if ((s = (*i)->internal_send_for (dest)) != 0) {
				s->amp()->gain_control()->set_value (0.0);
			}
		}
	}
}

void
Session::globally_set_send_gains_to_unity (boost::shared_ptr<Route> dest)
{
	boost::shared_ptr<RouteList> r = routes.reader ();
	boost::shared_ptr<Send> s;

	/* only tracks */

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if (boost::dynamic_pointer_cast<Track>(*i)) {
			if ((s = (*i)->internal_send_for (dest)) != 0) {
				s->amp()->gain_control()->set_value (1.0);
			}
		}
	}
}

void
Session::globally_set_send_gains_from_track(boost::shared_ptr<Route> dest)
{
	boost::shared_ptr<RouteList> r = routes.reader ();
	boost::shared_ptr<Send> s;

	/* only tracks */

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if (boost::dynamic_pointer_cast<Track>(*i)) {
			if ((s = (*i)->internal_send_for (dest)) != 0) {
				s->amp()->gain_control()->set_value ((*i)->gain_control()->get_value());
			}
		}
	}
}

void
Session::globally_add_internal_sends (boost::shared_ptr<Route> dest, Placement p)
{
	boost::shared_ptr<RouteList> r = routes.reader ();
	boost::shared_ptr<RouteList> t (new RouteList);

	/* only send tracks */

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if (boost::dynamic_pointer_cast<Track>(*i)) {
			t->push_back (*i);
		}
	}

	add_internal_sends (dest, p, t);
}

void
Session::add_internal_sends (boost::shared_ptr<Route> dest, Placement p, boost::shared_ptr<RouteList> senders)
{
	if (dest->is_monitor() || dest->is_master()) {
		return;
	}

	if (!dest->internal_return()) {
		dest->add_internal_return();
	}

	for (RouteList::iterator i = senders->begin(); i != senders->end(); ++i) {

		if ((*i)->is_monitor() || (*i)->is_master() || (*i) == dest) {
			continue;
		}

		(*i)->listen_via (dest, p, true, true);
	}

	graph_reordered ();
}

void
Session::remove_route (shared_ptr<Route> route)
{
        if (((route == _master_out) || (route == _monitor_out)) && !Config->get_allow_special_bus_removal()) {
                return;
        }

        route->set_solo (false, this);

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

		if (route == _monitor_out) {

			/* cancel control outs for all routes */

			for (RouteList::iterator r = rs->begin(); r != rs->end(); ++r) {
				(*r)->drop_listen (_monitor_out);
			}

			_monitor_out.reset ();
		}

		/* writer goes out of scope, forces route list update */
	}

        update_route_solo_state ();
	update_session_range_location_marker ();

	// We need to disconnect the route's inputs and outputs

	route->input()->disconnect (0);
	route->output()->disconnect (0);

	/* if the route had internal sends sending to it, remove them */
	if (route->internal_return()) {

		boost::shared_ptr<RouteList> r = routes.reader ();
		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			boost::shared_ptr<Send> s = (*i)->internal_send_for (route);
			if (s) {
				(*i)->remove_processor (s);
			}
		}
	}	

        boost::shared_ptr<MidiTrack> mt = boost::dynamic_pointer_cast<MidiTrack> (route);
        if (mt && mt->step_editing()) {
                if (_step_editors > 0) {
                        _step_editors--;
                }
        }

	update_latency_compensation (false, false);
	set_dirty();

        /* Re-sort routes to remove the graph's current references to the one that is
	 * going away, then flush old references out of the graph.
         */

	resort_routes ();
        route_graph->clear_other_chain ();

	/* get rid of it from the dead wood collection in the route list manager */

	/* XXX i think this is unsafe as it currently stands, but i am not sure. (pd, october 2nd, 2006) */

	routes.flush ();

	/* try to cause everyone to drop their references */

	route->drop_references ();

	sync_order_keys (N_("session"));

	Route::RemoteControlIDChange(); /* EMIT SIGNAL */

	/* save the new state of the world */

	if (save_state (_current_snapshot_name)) {
		save_history (_current_snapshot_name);
	}
}

void
Session::route_mute_changed (void* /*src*/)
{
	set_dirty ();
}

void
Session::route_listen_changed (void* /*src*/, boost::weak_ptr<Route> wpr)
{
	boost::shared_ptr<Route> route = wpr.lock();
	if (!route) {
		error << string_compose (_("programming error: %1"), X_("invalid route weak ptr passed to route_solo_changed")) << endmsg;
		return;
	}

	if (route->listening()) {

                if (Config->get_exclusive_solo()) {
                        /* new listen: disable all other listen */
                        shared_ptr<RouteList> r = routes.reader ();
                        for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
                                if ((*i) == route || (*i)->solo_isolated() || (*i)->is_master() || (*i)->is_monitor() || (*i)->is_hidden()) {
                                        continue;
                                } 
                                (*i)->set_listen (false, this);
                        }
                }

		_listen_cnt++;

	} else if (_listen_cnt > 0) {

		_listen_cnt--;
	}
}
void
Session::route_solo_isolated_changed (void* /*src*/, boost::weak_ptr<Route> wpr)
{
	boost::shared_ptr<Route> route = wpr.lock ();

	if (!route) {
		/* should not happen */
		error << string_compose (_("programming error: %1"), X_("invalid route weak ptr passed to route_solo_changed")) << endmsg;
		return;
	}
        
        bool send_changed = false;

        if (route->solo_isolated()) {
                if (_solo_isolated_cnt == 0) {
                        send_changed = true;
                }
                _solo_isolated_cnt++;
        } else if (_solo_isolated_cnt > 0) {
                _solo_isolated_cnt--;
                if (_solo_isolated_cnt == 0) {
                        send_changed = true;
                }
        }

        if (send_changed) {
                IsolatedChanged (); /* EMIT SIGNAL */
        }
}
            
void
Session::route_solo_changed (bool self_solo_change, void* /*src*/, boost::weak_ptr<Route> wpr)
{
        if (!self_solo_change) {
                // session doesn't care about changes to soloed-by-others
                return;
        }

	if (solo_update_disabled) {
		// We know already
		return;
	}

	boost::shared_ptr<Route> route = wpr.lock ();

	if (!route) {
		/* should not happen */
		error << string_compose (_("programming error: %1"), X_("invalid route weak ptr passed to route_solo_changed")) << endmsg;
		return;
	}
        
	shared_ptr<RouteList> r = routes.reader ();
	int32_t delta;

	if (route->self_soloed()) {
		delta = 1;
	} else {
		delta = -1;
	}
 
        if (delta == 1 && Config->get_exclusive_solo()) {
                /* new solo: disable all other solos */
                for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
                        if ((*i) == route || (*i)->solo_isolated() || (*i)->is_master() || (*i)->is_monitor() || (*i)->is_hidden()) {
                                continue;
                        } 
                        (*i)->set_solo (false, this);
                }
        }

	solo_update_disabled = true;
        
        RouteList uninvolved;
        
	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		bool via_sends_only;
                bool in_signal_flow;

		if ((*i) == route || (*i)->solo_isolated() || (*i)->is_master() || (*i)->is_monitor() || (*i)->is_hidden()) {
			continue;
		} 

                in_signal_flow = false;

                if ((*i)->feeds (route, &via_sends_only)) {
			if (!via_sends_only) {
                                if (!route->soloed_by_others_upstream()) {
                                        (*i)->mod_solo_by_others_downstream (delta);
                                }
                                in_signal_flow = true;
			}
		} 
                
                if (route->feeds (*i, &via_sends_only)) {
                        (*i)->mod_solo_by_others_upstream (delta);
                        in_signal_flow = true;
                }

                if (!in_signal_flow) {
                        uninvolved.push_back (*i);
                }
	}

	solo_update_disabled = false;
	update_route_solo_state (r);

        /* now notify that the mute state of the routes not involved in the signal
           pathway of the just-solo-changed route may have altered.
        */

        for (RouteList::iterator i = uninvolved.begin(); i != uninvolved.end(); ++i) {
                (*i)->mute_changed (this);
        }

	SoloChanged (); /* EMIT SIGNAL */
	set_dirty();
}

void
Session::update_route_solo_state (boost::shared_ptr<RouteList> r)
{
	/* now figure out if anything that matters is soloed (or is "listening")*/

	bool something_soloed = false;
        uint32_t listeners = 0;
        uint32_t isolated = 0;

	if (!r) {
		r = routes.reader();
	}

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if (!(*i)->is_master() && !(*i)->is_monitor() && !(*i)->is_hidden() && (*i)->self_soloed()) {
			something_soloed = true;
		}

                if (!(*i)->is_hidden() && (*i)->listening()) {
                        if (Config->get_solo_control_is_listen_control()) {
                                listeners++;
                        } else {
                                (*i)->set_listen (false, this);
                        }
                }

                if ((*i)->solo_isolated()) {
                        isolated++;
                }
	}

        if (something_soloed != _non_soloed_outs_muted) {
                _non_soloed_outs_muted = something_soloed;
                SoloActive (_non_soloed_outs_muted); /* EMIT SIGNAL */
        }

        _listen_cnt = listeners;

        if (isolated != _solo_isolated_cnt) {
                _solo_isolated_cnt = isolated;
                IsolatedChanged (); /* EMIT SIGNAL */
        }
}

boost::shared_ptr<RouteList> 
Session::get_routes_with_internal_returns() const
{
	shared_ptr<RouteList> r = routes.reader ();
	boost::shared_ptr<RouteList> rl (new RouteList);

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		if ((*i)->internal_return ()) {
			rl->push_back (*i);
		}
	}
	return rl;
}

bool
Session::io_name_is_legal (const std::string& name)
{
        shared_ptr<RouteList> r = routes.reader ();
        
        for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
                if ((*i)->name() == name) {
                        return false;
                }
                
                if ((*i)->has_io_processor_named (name)) {
                        return false;
                }
        }
        
        return true;
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

/** If either end of the session range location marker lies inside the current
 *  session extent, move it to the corresponding session extent.
 */
void
Session::update_session_range_location_marker ()
{
	if (_state_of_the_state & Loading) {
		return;
	}

	pair<nframes_t, nframes_t> const ext = get_extent ();

	if (_session_range_location == 0) {
		/* we don't have a session range yet; use this one (provided it is valid) */
		if (ext.first != max_frames) {
			add_session_range_location (ext.first, ext.second);
		}
	} else {
		/* update the existing session range */
		if (ext.first < _session_range_location->start()) {
			_session_range_location->set_start (ext.first);
			set_dirty ();
		}
		
		if (ext.second > _session_range_location->end()) {
			_session_range_location->set_end (ext.second);
			set_dirty ();
		}
		
	}
}

/** @return Extent of the session's contents; if the session is empty, the first value of
 *  the pair will equal max_frames.
 */
pair<nframes_t, nframes_t>
Session::get_extent () const
{
	pair<nframes_t, nframes_t> ext (max_frames, 0);
	
	boost::shared_ptr<RouteList> rl = routes.reader ();
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
		if (!tr || tr->destructive()) {
			// ignore tape tracks when getting extents
			continue;
		}

		pair<nframes_t, nframes_t> e = tr->playlist()->get_extent ();
		if (e.first < ext.first) {
			ext.first = e.first;
		}
		if (e.second > ext.second) {
			ext.second = e.second;
		}
	}

	return ext;
}

/* Region management */

boost::shared_ptr<Region>
Session::find_whole_file_parent (boost::shared_ptr<Region const> child) const
{
        const RegionFactory::RegionMap& regions (RegionFactory::regions());
	RegionFactory::RegionMap::const_iterator i;
	boost::shared_ptr<Region> region;

	Glib::Mutex::Lock lm (region_lock);

	for (i = regions.begin(); i != regions.end(); ++i) {

		region = i->second;

		if (region->whole_file()) {

			if (child->source_equivalent (region)) {
				return region;
			}
		}
	}

	return boost::shared_ptr<Region> ();
}

int
Session::destroy_sources (list<boost::shared_ptr<Source> > srcs)
{
        set<boost::shared_ptr<Region> > relevant_regions;

	for (list<boost::shared_ptr<Source> >::iterator s = srcs.begin(); s != srcs.end(); ++s) {
                RegionFactory::get_regions_using_source (*s, relevant_regions);
	}

        cerr << "There are " << relevant_regions.size() << " using " << srcs.size() << " sources" << endl;

        for (set<boost::shared_ptr<Region> >::iterator r = relevant_regions.begin(); r != relevant_regions.end(); ) {
                set<boost::shared_ptr<Region> >::iterator tmp;

                tmp = r;
                ++tmp;

                cerr << "Cleanup " << (*r)->name() << " UC = " << (*r).use_count() << endl;

                playlists->destroy_region (*r);
                RegionFactory::map_remove (*r);

                (*r)->drop_sources ();
                (*r)->drop_references ();

                cerr << "\tdone UC = " << (*r).use_count() << endl;

                relevant_regions.erase (r);

                r = tmp;
        }

	for (list<boost::shared_ptr<Source> >::iterator s = srcs.begin(); s != srcs.end(); ) {
                
                {
                        Glib::Mutex::Lock ls (source_lock);
                        /* remove from the main source list */
                        sources.erase ((*s)->id());
                }

                (*s)->mark_for_remove ();
                (*s)->drop_references ();

                s = srcs.erase (s);
        }

	return 0;
}

int
Session::remove_last_capture ()
{
	list<boost::shared_ptr<Source> > srcs;

	boost::shared_ptr<RouteList> rl = routes.reader ();
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
		if (!tr) {
			continue;
		}
		
		list<boost::shared_ptr<Source> >& l = tr->last_capture_sources();

		if (!l.empty()) {
			srcs.insert (srcs.end(), l.begin(), l.end());
			l.clear ();
		}
	}

	destroy_sources (srcs);

	save_state (_current_snapshot_name);

	return 0;
}

/* Source Management */

void
Session::add_source (boost::shared_ptr<Source> source)
{
	pair<SourceMap::key_type, SourceMap::mapped_type> entry;
	pair<SourceMap::iterator,bool> result;

	entry.first = source->id();
	entry.second = source;

	{
		Glib::Mutex::Lock lm (source_lock);
		result = sources.insert (entry);
	}

	if (result.second) {

                /* yay, new source */

		set_dirty();

                boost::shared_ptr<AudioFileSource> afs;
                
                if ((afs = boost::dynamic_pointer_cast<AudioFileSource>(source)) != 0) {
                        if (Config->get_auto_analyse_audio()) {
                                Analyser::queue_source_for_analysis (source, false);
                        }
                }
        }
}

void
Session::remove_source (boost::weak_ptr<Source> src)
{
	SourceMap::iterator i;
	boost::shared_ptr<Source> source = src.lock();

	if (!source) {
		return;
	}

	{
		Glib::Mutex::Lock lm (source_lock);

		if ((i = sources.find (source->id())) != sources.end()) {
                        cerr << "Removing source " << source->name() << endl;
			sources.erase (i);
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
	Glib::Mutex::Lock lm (source_lock);
	SourceMap::iterator i;
	boost::shared_ptr<Source> source;

	if ((i = sources.find (id)) != sources.end()) {
		source = i->second;
	}

	return source;
}

boost::shared_ptr<Source>
Session::source_by_path_and_channel (const Glib::ustring& path, uint16_t chn)
{
	Glib::Mutex::Lock lm (source_lock);

	for (SourceMap::iterator i = sources.begin(); i != sources.end(); ++i) {
		boost::shared_ptr<AudioFileSource> afs
			= boost::dynamic_pointer_cast<AudioFileSource>(i->second);

		if (afs && afs->path() == path && chn == afs->channel()) {
			return afs;
		}
	}
	return boost::shared_ptr<Source>();
}


string
Session::change_source_path_by_name (string path, string oldname, string newname, bool destructive)
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

		string dir;
		string prefix;
		string::size_type dash;

		dir = Glib::path_get_dirname (path);
                path = Glib::path_get_basename (path);

		/* '-' is not a legal character for the NAME part of the path */

		if ((dash = path.find_last_of ('-')) == string::npos) {
			return "";
		}

		prefix = path.substr (0, dash);

		path += prefix;
		path += '-';
		path += new_legalized;
		path += native_header_format_extension (config.get_native_file_header_format(), DataType::AUDIO);

                path = Glib::build_filename (dir, path);

	} else {

		/* non-destructive file sources have a name of the form:

		    /path/to/NAME-nnnnn(%[LR])?.ext

		    the task here is to replace NAME with the new name.
		*/

		string dir;
		string suffix;
		string::size_type dash;
		string::size_type postfix;

		dir = Glib::path_get_dirname (path);
                path = Glib::path_get_basename (path);

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
			error << "Logic error in Session::change_source_path_by_name(), please report" << endl;
			return "";
		}

		const uint32_t limit = 10000;
		char buf[PATH_MAX+1];

		for (uint32_t cnt = 1; cnt <= limit; ++cnt) {

			snprintf (buf, sizeof(buf), "%s-%u%s", newname.c_str(), cnt, suffix.c_str());

                        if (!matching_unsuffixed_filename_exists_in (dir, buf)) {
                                path = Glib::build_filename (dir, buf);
				break;
			}

			path = "";
		}

		if (path.empty()) {
			fatal << string_compose (_("FATAL ERROR! Could not find a suitable version of %1 for a rename"),
                                                 newname) << endl;
                        /*NOTREACHED*/
		}
	}

	return path;
}

/** Return the full path (in some session directory) for a new within-session source.
 * \a name must be a session-unique name that does not contain slashes
 *         (e.g. as returned by new_*_source_name)
 */
string
Session::new_source_path_from_name (DataType type, const string& name, bool as_stub)
{
	assert(name.find("/") == string::npos);

	SessionDirectory sdir(get_best_session_directory_for_new_source());

	sys::path p;
	if (type == DataType::AUDIO) {
		p = (as_stub ? sdir.sound_stub_path() : sdir.sound_path());
	} else if (type == DataType::MIDI) {
		p = (as_stub ? sdir.midi_stub_path() : sdir.midi_path());
	} else {
		error << "Unknown source type, unable to create file path" << endmsg;
		return "";
	}

	p /= name;
	return p.to_string();
}

Glib::ustring
Session::peak_path (Glib::ustring base) const
{
	sys::path peakfile_path(_session_dir->peak_path());
	peakfile_path /= basename_nosuffix (base) + peakfile_suffix;
	return peakfile_path.to_string();
}

/** Return a unique name based on \a base for a new internal audio source */
string
Session::new_audio_source_name (const string& base, uint32_t nchan, uint32_t chan, bool destructive)
{
	uint32_t cnt;
	char buf[PATH_MAX+1];
	const uint32_t limit = 10000;
	string legalized;
        string ext = native_header_format_extension (config.get_native_file_header_format(), DataType::AUDIO);

	buf[0] = '\0';
	legalized = legalize_for_path (base);

	// Find a "version" of the base name that doesn't exist in any of the possible directories.
	for (cnt = (destructive ? ++destructive_index : 1); cnt <= limit; ++cnt) {

		vector<space_and_path>::iterator i;
		uint32_t existing = 0;

		for (i = session_dirs.begin(); i != session_dirs.end(); ++i) {

			if (destructive) {

				if (nchan < 2) {
					snprintf (buf, sizeof(buf), "T%04d-%s%s",
                                                  cnt, legalized.c_str(), ext.c_str());
				} else if (nchan == 2) {
					if (chan == 0) {
						snprintf (buf, sizeof(buf), "T%04d-%s%%L%s",
                                                          cnt, legalized.c_str(), ext.c_str());
					} else {
						snprintf (buf, sizeof(buf), "T%04d-%s%%R%s",
                                                          cnt, legalized.c_str(), ext.c_str());
					}
				} else if (nchan < 26) {
					snprintf (buf, sizeof(buf), "T%04d-%s%%%c%s",
                                                  cnt, legalized.c_str(), 'a' + chan, ext.c_str());
				} else {
					snprintf (buf, sizeof(buf), "T%04d-%s%s",
                                                  cnt, legalized.c_str(), ext.c_str());
				}

			} else {

				if (nchan < 2) {
					snprintf (buf, sizeof(buf), "%s-%u%s", legalized.c_str(), cnt, ext.c_str());
				} else if (nchan == 2) {
					if (chan == 0) {
						snprintf (buf, sizeof(buf), "%s-%u%%L%s", legalized.c_str(), cnt, ext.c_str());
					} else {
						snprintf (buf, sizeof(buf), "%s-%u%%R%s", legalized.c_str(), cnt, ext.c_str());
					}
				} else if (nchan < 26) {
					snprintf (buf, sizeof(buf), "%s-%u%%%c%s", legalized.c_str(), cnt, 'a' + chan, ext.c_str());
				} else {
					snprintf (buf, sizeof(buf), "%s-%u%s", legalized.c_str(), cnt, ext.c_str());
				}
			}

			SessionDirectory sdir((*i).path);

			string spath = sdir.sound_path().to_string();
			string spath_stubs = sdir.sound_stub_path().to_string();

                        /* note that we search *without* the extension so that
                           we don't end up both "Audio 1-1.wav" and "Audio 1-1.caf" 
                           in the event that this new name is required for
                           a file format change.
                        */

                        if (matching_unsuffixed_filename_exists_in (spath, buf) ||
                            matching_unsuffixed_filename_exists_in (spath_stubs, buf)) {
                                existing++;
                                break;
                        }
		}

		if (existing == 0) {
			break;
		}

		if (cnt > limit) {
			error << string_compose(
					_("There are already %1 recordings for %2, which I consider too many."),
					limit, base) << endmsg;
			destroy ();
			throw failed_constructor();
		}
	}
        
	return Glib::path_get_basename (buf);
}

/** Create a new within-session audio source */
boost::shared_ptr<AudioFileSource>
Session::create_audio_source_for_session (size_t n_chans, string const & n, uint32_t chan, bool destructive, bool as_stub)
{
	const string name    = new_audio_source_name (n, n_chans, chan, destructive);
	const string path    = new_source_path_from_name(DataType::AUDIO, name, as_stub);

	return boost::dynamic_pointer_cast<AudioFileSource> (
		SourceFactory::createWritable (DataType::AUDIO, *this, path, destructive, frame_rate()));
}

/** Return a unique name based on \a base for a new internal MIDI source */
string
Session::new_midi_source_name (const string& base)
{
	uint32_t cnt;
	char buf[PATH_MAX+1];
	const uint32_t limit = 10000;
	string legalized;

	buf[0] = '\0';
	legalized = legalize_for_path (base);

	// Find a "version" of the file name that doesn't exist in any of the possible directories.
	for (cnt = 1; cnt <= limit; ++cnt) {

		vector<space_and_path>::iterator i;
		uint32_t existing = 0;

		for (i = session_dirs.begin(); i != session_dirs.end(); ++i) {

			SessionDirectory sdir((*i).path);

			sys::path p = sdir.midi_path();
			p /= legalized;

			snprintf (buf, sizeof(buf), "%s-%u.mid", p.to_string().c_str(), cnt);

			if (sys::exists (buf)) {
				existing++;
			}
		}

		if (existing == 0) {
			break;
		}

		if (cnt > limit) {
			error << string_compose(
					_("There are already %1 recordings for %2, which I consider too many."),
					limit, base) << endmsg;
			destroy ();
			throw failed_constructor();
		}
	}

	return Glib::path_get_basename(buf);
}


/** Create a new within-session MIDI source */
boost::shared_ptr<MidiSource>
Session::create_midi_source_for_session (Track* track, string const & n, bool as_stub)
{
        /* try to use the existing write source for the track, to keep numbering sane 
         */

        if (track) {
                /*MidiTrack* mt = dynamic_cast<Track*> (track);
                assert (mt);
                */

                list<boost::shared_ptr<Source> > l = track->steal_write_sources ();
                
                if (!l.empty()) {
                        assert (boost::dynamic_pointer_cast<MidiSource> (l.front()));
                        return boost::dynamic_pointer_cast<MidiSource> (l.front());
                }
        }

	const string name = new_midi_source_name (n);
	const string path = new_source_path_from_name (DataType::MIDI, name, as_stub);

	return boost::dynamic_pointer_cast<SMFSource> (
			SourceFactory::createWritable (
					DataType::MIDI, *this, path, false, frame_rate()));
}


void
Session::add_playlist (boost::shared_ptr<Playlist> playlist, bool unused)
{
	if (playlist->hidden()) {
		return;
	}

	playlists->add (playlist);

	if (unused) {
		playlist->release();
	}

	set_dirty();
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

	playlists->remove (playlist);

	set_dirty();
}

void
Session::set_audition (boost::shared_ptr<Region> r)
{
	pending_audition_region = r;
	add_post_transport_work (PostTransportAudition);
	_butler->schedule_transport_work ();
}

void
Session::audition_playlist ()
{
	SessionEvent* ev = new SessionEvent (SessionEvent::Audition, SessionEvent::Add, SessionEvent::Immediate, 0, 0.0);
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
	SessionEvent* ev = new SessionEvent (SessionEvent::Audition, SessionEvent::Add, SessionEvent::Immediate, 0, 0.0);
	ev->region = r;
	queue_event (ev);
}

void
Session::cancel_audition ()
{
	if (auditioner->auditioning()) {
		auditioner->cancel_audition ();
		AuditionActive (false); /* EMIT SIGNAL */
	}
}

bool
Session::RoutePublicOrderSorter::operator() (boost::shared_ptr<Route> a, boost::shared_ptr<Route> b)
{
        if (a->is_monitor()) { 
                return true;
        }
        if (b->is_monitor()) {
                return false;
        }
	return a->order_key(N_("signal")) < b->order_key(N_("signal"));
}

bool
Session::is_auditioning () const
{
	/* can be called before we have an auditioner object */
	if (auditioner) {
		return auditioner->auditioning();
	} else {
		return false;
	}
}

void
Session::graph_reordered ()
{
	/* don't do this stuff if we are setting up connections
	   from a set_state() call or creating new tracks. Ditto for deletion.
	*/

	if (_state_of_the_state & (InitialConnecting|Deletion)) {
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

	boost::shared_ptr<RouteList> rl = routes.reader ();
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
		if (tr) {
			tr->set_capture_offset ();
		}
	}
}

nframes_t
Session::available_capture_duration ()
{
	float sample_bytes_on_disk = 4.0; // keep gcc happy

	switch (config.get_native_file_data_format()) {
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
Session::add_bundle (shared_ptr<Bundle> bundle)
{
	{
		RCUWriter<BundleList> writer (_bundles);
		boost::shared_ptr<BundleList> b = writer.get_copy ();
		b->push_back (bundle);
	}

	BundleAdded (bundle); /* EMIT SIGNAL */

	set_dirty();
}

void
Session::remove_bundle (shared_ptr<Bundle> bundle)
{
	bool removed = false;

	{
		RCUWriter<BundleList> writer (_bundles);
		boost::shared_ptr<BundleList> b = writer.get_copy ();
		BundleList::iterator i = find (b->begin(), b->end(), bundle);

		if (i != b->end()) {
			b->erase (i);
			removed = true;
		}
	}

	if (removed) {
		 BundleRemoved (bundle); /* EMIT SIGNAL */
	}

	set_dirty();
}

shared_ptr<Bundle>
Session::bundle_by_name (string name) const
{
	boost::shared_ptr<BundleList> b = _bundles.reader ();

	for (BundleList::const_iterator i = b->begin(); i != b->end(); ++i) {
		if ((*i)->name() == name) {
			return* i;
		}
	}

	return boost::shared_ptr<Bundle> ();
}

void
Session::tempo_map_changed (const PropertyChange&)
{
	clear_clicks ();

	playlists->update_after_tempo_map_change ();

	_locations->apply (*this, &Session::update_locations_after_tempo_map_change);
	
	set_dirty ();
}

void
Session::update_locations_after_tempo_map_change (Locations::LocationList& loc)
{
	for (Locations::LocationList::iterator i = loc.begin(); i != loc.end(); ++i) {
		(*i)->recompute_frames_from_bbt ();
	}
}

/** Ensures that all buffers (scratch, send, silent, etc) are allocated for
 * the given count with the current block size.
 */
void
Session::ensure_buffers (ChanCount howmany)
{
        BufferManager::ensure_buffers (howmany);
}

void
Session::ensure_buffer_set(BufferSet& buffers, const ChanCount& count)
{
	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		buffers.ensure_buffers(*t, count.get(*t), _engine.raw_buffer_size(*t));
	}
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

uint32_t
Session::next_return_id ()
{
	/* this doesn't really loop forever. just think about it */

	while (true) {
		for (boost::dynamic_bitset<uint32_t>::size_type n = 0; n < return_bitset.size(); ++n) {
			if (!return_bitset[n]) {
				return_bitset[n] = true;
				return n;

			}
		}

		/* none available, so resize and try again */

		return_bitset.resize (return_bitset.size() + 16, false);
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
Session::mark_return_id (uint32_t id)
{
	if (id >= return_bitset.size()) {
		return_bitset.resize (id+16, false);
	}
	if (return_bitset[id]) {
		warning << string_compose (_("return ID %1 appears to be in use already"), id) << endmsg;
	}
	return_bitset[id] = true;
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

void
Session::unmark_send_id (uint32_t id)
{
	if (id < send_bitset.size()) {
                send_bitset[id] = false;
        }
}

void
Session::unmark_return_id (uint32_t id)
{
	if (id < return_bitset.size()) {
                return_bitset[id] = false;
        }
}

void
Session::unmark_insert_id (uint32_t id)
{
	if (id < insert_bitset.size()) {
                insert_bitset[id] = false;
        }
}


/* Named Selection management */

boost::shared_ptr<NamedSelection>
Session::named_selection_by_name (string name)
{
	Glib::Mutex::Lock lm (named_selection_lock);
	for (NamedSelectionList::iterator i = named_selections.begin(); i != named_selections.end(); ++i) {
		if ((*i)->name == name) {
			return *i;
		}
	}
	return boost::shared_ptr<NamedSelection>();
}

void
Session::add_named_selection (boost::shared_ptr<NamedSelection> named_selection)
{
	{
		Glib::Mutex::Lock lm (named_selection_lock);
		named_selections.insert (named_selections.begin(), named_selection);
	}

	set_dirty();

	NamedSelectionAdded (); /* EMIT SIGNAL */
}

void
Session::remove_named_selection (boost::shared_ptr<NamedSelection> named_selection)
{
	bool removed = false;

	{
		Glib::Mutex::Lock lm (named_selection_lock);

		NamedSelectionList::iterator i = find (named_selections.begin(), named_selections.end(), named_selection);

		if (i != named_selections.end()) {
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
	boost::shared_ptr<RouteList> rl = routes.reader ();
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
		if (tr) {
                        /* don't save state as we do this, there's no point
                         */

                        _state_of_the_state = StateOfTheState (_state_of_the_state|InCleanup);
			tr->reset_write_sources (false);
                        _state_of_the_state = StateOfTheState (_state_of_the_state & ~InCleanup);
		}
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

bool
Session::route_name_internal (string n) const
{
	if (auditioner && auditioner->name() == n) {
		return true;
	}

	if (_click_io && _click_io->name() == n) {
		return true;
	}

	return false;
}

int
Session::freeze_all (InterThreadInfo& itt)
{
	shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {

		boost::shared_ptr<Track> t;

		if ((t = boost::dynamic_pointer_cast<Track>(*i)) != 0) {
			/* XXX this is wrong because itt.progress will keep returning to zero at the start
			   of every track.
			*/
			t->freeze_me (itt);
		}
	}

	return 0;
}

boost::shared_ptr<Region>
Session::write_one_track (AudioTrack& track, framepos_t start, framepos_t end,
			  bool /*overwrite*/, vector<boost::shared_ptr<Source> >& srcs,
			  InterThreadInfo& itt, bool enable_processing)
{
	boost::shared_ptr<Region> result;
	boost::shared_ptr<Playlist> playlist;
	boost::shared_ptr<AudioFileSource> fsource;
	uint32_t x;
	char buf[PATH_MAX+1];
	ChanCount nchans(track.n_channels());
	framepos_t position;
	framecnt_t this_chunk;
	framepos_t to_do;
	BufferSet buffers;
	SessionDirectory sdir(get_best_session_directory_for_new_source ());
	const string sound_dir = sdir.sound_path().to_string();
	framepos_t len = end - start;
        bool need_block_size_reset = false;
        string ext;

	if (end <= start) {
		error << string_compose (_("Cannot write a range where end <= start (e.g. %1 <= %2)"),
					 end, start) << endmsg;
		return result;
	}

	const framecnt_t chunk_size = (256 * 1024)/4;

	// block all process callback handling

	block_processing ();

	/* call tree *MUST* hold route_lock */

	if ((playlist = track.playlist()) == 0) {
		goto out;
	}

	/* external redirects will be a problem */

	if (track.has_external_redirects()) {
		goto out;
	}

        ext = native_header_format_extension (config.get_native_file_header_format(), DataType::AUDIO);

	for (uint32_t chan_n=0; chan_n < nchans.n_audio(); ++chan_n) {

		for (x = 0; x < 99999; ++x) {
			snprintf (buf, sizeof(buf), "%s/%s-%d-bounce-%" PRIu32 "%s", sound_dir.c_str(), playlist->name().c_str(), chan_n, x+1, ext.c_str());
			if (access (buf, F_OK) != 0) {
				break;
			}
		}

		if (x == 99999) {
			error << string_compose (_("too many bounced versions of playlist \"%1\""), playlist->name()) << endmsg;
			goto out;
		}

		try {
			fsource = boost::dynamic_pointer_cast<AudioFileSource> (
				SourceFactory::createWritable (DataType::AUDIO, *this, buf, false, frame_rate()));
		}

		catch (failed_constructor& err) {
			error << string_compose (_("cannot create new audio file \"%1\" for %2"), buf, track.name()) << endmsg;
			goto out;
		}

		srcs.push_back (fsource);
	}

        /* tell redirects that care that we are about to use a much larger blocksize */
        
        need_block_size_reset = true;
        track.set_block_size (chunk_size);

	/* XXX need to flush all redirects */

	position = start;
	to_do = len;

	/* create a set of reasonably-sized buffers */
	buffers.ensure_buffers(DataType::AUDIO, nchans.n_audio(), chunk_size);
	buffers.set_count(nchans);

	for (vector<boost::shared_ptr<Source> >::iterator src=srcs.begin(); src != srcs.end(); ++src) {
		boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource>(*src);
		if (afs)
			afs->prepare_for_peakfile_writes ();
	}

	while (to_do && !itt.cancel) {

		this_chunk = min (to_do, chunk_size);

		if (track.export_stuff (buffers, start, this_chunk, enable_processing)) {
			goto out;
		}

		uint32_t n = 0;
		for (vector<boost::shared_ptr<Source> >::iterator src=srcs.begin(); src != srcs.end(); ++src, ++n) {
			boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource>(*src);

			if (afs) {
				if (afs->write (buffers.get_audio(n).data(), this_chunk) != this_chunk) {
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

		for (vector<boost::shared_ptr<Source> >::iterator src=srcs.begin(); src != srcs.end(); ++src) {
			boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource>(*src);

			if (afs) {
				afs->update_header (position, *xnow, now);
				afs->flush_header ();
			}
		}

		/* construct a region to represent the bounced material */

		PropertyList plist;
		
		plist.add (Properties::start, 0);
		plist.add (Properties::length, srcs.front()->length(srcs.front()->timeline_position()));
		plist.add (Properties::name, region_name_from_path (srcs.front()->name(), true));
		
		result = RegionFactory::create (srcs, plist);
			   
	}

  out:
	if (!result) {
		for (vector<boost::shared_ptr<Source> >::iterator src = srcs.begin(); src != srcs.end(); ++src) {
			boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource>(*src);

			if (afs) {
				afs->mark_for_remove ();
			}

			(*src)->drop_references ();
		}

	} else {
		for (vector<boost::shared_ptr<Source> >::iterator src = srcs.begin(); src != srcs.end(); ++src) {
			boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource>(*src);

			if (afs)
				afs->done_with_peakfile_writes ();
		}
	}


        if (need_block_size_reset) {
                track.set_block_size (get_block_size());
        }
        
	unblock_processing ();

	return result;
}

gain_t*
Session::gain_automation_buffer() const
{
        return ProcessThread::gain_automation_buffer ();
}

pan_t**
Session::pan_automation_buffer() const
{
        return ProcessThread::pan_automation_buffer ();
}

BufferSet&
Session::get_silent_buffers (ChanCount count)
{
        return ProcessThread::get_silent_buffers (count);
#if 0
	assert(_silent_buffers->available() >= count);
	_silent_buffers->set_count(count);

	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		for (size_t i= 0; i < count.get(*t); ++i) {
			_silent_buffers->get(*t, i).clear();
		}
	}

	return *_silent_buffers;
#endif
}

BufferSet&
Session::get_scratch_buffers (ChanCount count)
{
        return ProcessThread::get_scratch_buffers (count);
#if 0
	if (count != ChanCount::ZERO) {
		assert(_scratch_buffers->available() >= count);
		_scratch_buffers->set_count(count);
	} else {
		_scratch_buffers->set_count (_scratch_buffers->available());
	}

	return *_scratch_buffers;
#endif
}

BufferSet&
Session::get_mix_buffers (ChanCount count)
{
        return ProcessThread::get_mix_buffers (count);
#if 0
	assert(_mix_buffers->available() >= count);
	_mix_buffers->set_count(count);
	return *_mix_buffers;
#endif
}

uint32_t
Session::ntracks () const
{
	uint32_t n = 0;
	shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::const_iterator i = r->begin(); i != r->end(); ++i) {
		if (boost::dynamic_pointer_cast<Track> (*i)) {
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
		if (boost::dynamic_pointer_cast<Track>(*i) == 0) {
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

void
Session::sync_order_keys (std::string const & base)
{
	if (deletion_in_progress()) {
		return;
	}

	if (!Config->get_sync_all_route_ordering()) {
		/* leave order keys as they are */
		return;
	}

	boost::shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		(*i)->sync_order_keys (base);
	}

	Route::SyncOrderKeys (base); // EMIT SIGNAL

	/* this might not do anything */

	set_remote_control_ids ();
}

/** @return true if there is at least one record-enabled track, otherwise false */
bool
Session::have_rec_enabled_track () const
{
	return g_atomic_int_get (&_have_rec_enabled_track) == 1;
}

/** Update the state of our rec-enabled tracks flag */
void
Session::update_have_rec_enabled_track ()
{
	boost::shared_ptr<RouteList> rl = routes.reader ();
	RouteList::iterator i = rl->begin();
	while (i != rl->end ()) {

		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
		if (tr && tr->record_enabled ()) {
			break;
		}
		
		++i;
	}

	int const old = g_atomic_int_get (&_have_rec_enabled_track);

	g_atomic_int_set (&_have_rec_enabled_track, i != rl->end () ? 1 : 0);

	if (g_atomic_int_get (&_have_rec_enabled_track) != old) {
		RecordStateChanged (); /* EMIT SIGNAL */
	}
}

void
Session::listen_position_changed ()
{
	Placement p;

	switch (Config->get_listen_position()) {
	case AfterFaderListen:
		p = PostFader;
		break;

	case PreFaderListen:
		p = PreFader;
		break;
	}

	boost::shared_ptr<RouteList> r = routes.reader ();

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		(*i)->put_monitor_send_at (p);
	}
}

void
Session::solo_control_mode_changed ()
{
	/* cancel all solo or all listen when solo control mode changes */

        if (soloing()) {
                set_solo (get_routes(), false);
        } else if (listening()) {
                set_listen (get_routes(), false);
        }
}

/** Called when anything about any of our route groups changes (membership, state etc.) */
void
Session::route_group_changed ()
{
	RouteGroupChanged (); /* EMIT SIGNAL */
}

vector<SyncSource>
Session::get_available_sync_options () const
{
	vector<SyncSource> ret;
	
	ret.push_back (JACK);
	ret.push_back (MTC);
	ret.push_back (MIDIClock);

	return ret;
}

boost::shared_ptr<RouteList>
Session::get_routes_with_regions_at (nframes64_t const p) const
{
	shared_ptr<RouteList> r = routes.reader ();
	shared_ptr<RouteList> rl (new RouteList);

	for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);
		if (!tr) {
			continue;
		}
		
		boost::shared_ptr<Playlist> pl = tr->playlist ();
		if (!pl) {
			continue;
		}
		
		if (pl->has_region_at (p)) {
			rl->push_back (*i);
		}
	}

	return rl;
}

void
Session::goto_end ()
{
	if (_session_range_location) {
		request_locate (_session_range_location->end(), false);
	} else {
		request_locate (0, false);
	}
}

void
Session::goto_start ()
{
	if (_session_range_location) {
		request_locate (_session_range_location->start(), false);
	} else {
		request_locate (0, false);
	}
}

nframes_t
Session::current_start_frame () const
{
	return _session_range_location ? _session_range_location->start() : 0;
}

nframes_t
Session::current_end_frame () const
{
	return _session_range_location ? _session_range_location->end() : 0;
}

void
Session::add_session_range_location (nframes_t start, nframes_t end)
{
	_session_range_location = new Location (*this, start, end, _("session"), Location::IsSessionRange);
	_locations->add (_session_range_location);
}

/** Called when one of our routes' order keys has changed */
void
Session::route_order_key_changed ()
{
	RouteOrderKeyChanged (); /* EMIT SIGNAL */
}

void
Session::step_edit_status_change (bool yn)
{
        bool send = false;

        bool val = false;
        if (yn) {
                send = (_step_editors == 0);
                val = true;

                _step_editors++;
        } else {
                send = (_step_editors == 1);
                val = false;

                if (_step_editors > 0) {
                        _step_editors--;
                }
        }

        if (send) {
                StepEditStatusChange (val);
        }
}

