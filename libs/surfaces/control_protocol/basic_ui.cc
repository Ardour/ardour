/*
    Copyright (C) 2006 Paul Davis 

    This program is free software; you can redistribute it
    and/or modify it under the terms of the GNU Lesser
    General Public License as published by the Free Software
    Foundation; either version 2 of the License, or (at your
    option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "pbd/pthread_utils.h"
#include "pbd/memento_command.h"

#include "ardour/session.h"
#include "ardour/location.h"

#include "control_protocol/basic_ui.h"

#include "i18n.h"

using namespace ARDOUR;

PBD::Signal2<void,std::string,std::string> BasicUI::AccessAction;

BasicUI::BasicUI (Session& s)
	: session (&s)
{
}

BasicUI::BasicUI ()
	: session (0)
{
}

BasicUI::~BasicUI ()
{
	
}

void
BasicUI::register_thread (std::string name)
{
	std::string pool_name = name;
	pool_name += " events";

	SessionEvent::create_per_thread_pool (pool_name, 64);
}

void
BasicUI::access_action ( std::string action_path ) 
{
	int split_at = action_path.find( "/" );
	std::string group = action_path.substr( 0, split_at );
	std::string item = action_path.substr( split_at + 1 );

	AccessAction( group, item );
}

void
BasicUI::loop_toggle () 
{
	if (session->get_play_loop()) {
		session->request_play_loop (false);
	} else {
		session->request_play_loop (true);
		if (!session->transport_rolling()) {
			session->request_transport_speed (1.0);
		}
	}
}

void
BasicUI::goto_start ()
{
	session->goto_start ();
}

void
BasicUI::goto_end ()
{
	session->goto_end ();
}

void       
BasicUI::add_marker (const std::string& markername)
{
	framepos_t where = session->audible_frame();
	Location *location = new Location (*session, where, where, markername, Location::IsMark);
	session->begin_reversible_command (_("add marker"));
	XMLNode &before = session->locations()->get_state();
	session->locations()->add (location, true);
	XMLNode &after = session->locations()->get_state();
	session->add_command (new MementoCommand<Locations>(*(session->locations()), &before, &after));
	session->commit_reversible_command ();
}

void
BasicUI::rewind ()
{
	std::cerr << "request transport speed of " << session->transport_speed() - 1.5 << std::endl;
	session->request_transport_speed (session->transport_speed() - 1.5);
}

void
BasicUI::ffwd ()
{
	session->request_transport_speed (session->transport_speed() + 1.5);
}

void
BasicUI::transport_stop ()
{
	session->request_transport_speed (0.0);
}

void
BasicUI::transport_play (bool from_last_start)
{
	bool rolling = session->transport_rolling ();

	if (session->get_play_loop()) {
		session->request_play_loop (false);
	} 

	if (session->get_play_range ()) {
		session->request_play_range (0);
	}
	
	if (from_last_start && rolling) {
		session->request_locate (session->last_transport_start(), true);

	}

	session->request_transport_speed (1.0f);
}

void
BasicUI::rec_enable_toggle ()
{
	switch (session->record_status()) {
	case Session::Disabled:
		if (session->ntracks() == 0) {
			// string txt = _("Please create 1 or more track\nbefore trying to record.\nCheck the Session menu.");
			// MessageDialog msg (*editor, txt);
			// msg.run ();
			return;
		}
		session->maybe_enable_record ();
		break;
	case Session::Recording:
	case Session::Enabled:
		session->disable_record (true);
	}
}

void
BasicUI::save_state ()
{
	session->save_state ("");
}

void
BasicUI::prev_marker ()
{
	Location *location = session->locations()->first_location_before (session->transport_frame());
	
	if (location) {
		session->request_locate (location->start(), session->transport_rolling());
	} else {
		session->goto_start ();
	}
}

void
BasicUI::next_marker ()
{
	Location *location = session->locations()->first_location_after (session->transport_frame());

	if (location) {
		session->request_locate (location->start(), session->transport_rolling());
	} else {
		session->request_locate (session->current_end_frame());
	}
}

void
BasicUI::set_transport_speed (double speed)
{
	session->request_transport_speed (speed);
}

double
BasicUI::get_transport_speed ()
{
	return session->transport_speed ();
}

void
BasicUI::undo ()
{
	session->undo (1);
}

void
BasicUI::redo ()
{
	session->redo (1);
}

void
BasicUI::toggle_all_rec_enables ()
{
	if (session->get_record_enabled()) {
		// session->record_disenable_all ();
	} else {
		// session->record_enable_all ();
	}
}

void
BasicUI::toggle_punch_in ()
{
	session->config.set_punch_in (!session->config.get_punch_in());
}

void
BasicUI::toggle_punch_out ()
{
	session->config.set_punch_out (!session->config.get_punch_out());
}

bool
BasicUI::get_record_enabled ()
{
	return session->get_record_enabled();
}

void
BasicUI::set_record_enable (bool yn)
{
	if (yn) {
		session->maybe_enable_record ();
	} else {
		session->disable_record (false, true);
	}
}

framepos_t
BasicUI::transport_frame ()
{
	return session->transport_frame();
}

void
BasicUI::locate (framepos_t where, bool roll_after_locate)
{
	session->request_locate (where, roll_after_locate);
}

bool
BasicUI::locating ()
{
	return session->locate_pending();
}

bool
BasicUI::locked ()
{
	return session->transport_locked ();
}

ARDOUR::framecnt_t
BasicUI::timecode_frames_per_hour ()
{
	return session->timecode_frames_per_hour ();
}

void
BasicUI::timecode_time (framepos_t where, Timecode::Time& timecode)
{
	session->timecode_time (where, *((Timecode::Time *) &timecode));
}

void 
BasicUI::timecode_to_sample (Timecode::Time& timecode, framepos_t & sample, bool use_offset, bool use_subframes) const
{
	session->timecode_to_sample (*((Timecode::Time*)&timecode), sample, use_offset, use_subframes);
}

void 
BasicUI::sample_to_timecode (framepos_t sample, Timecode::Time& timecode, bool use_offset, bool use_subframes) const
{
	session->sample_to_timecode (sample, *((Timecode::Time*)&timecode), use_offset, use_subframes);
}

#if 0
this stuff is waiting to go in so that all UIs can offer complex solo/mute functionality

void
BasicUI::solo_release (boost::shared_ptr<Route> r)
{
}

void
BasicUI::solo_press (boost::shared_ptr<Route> r, bool momentary, bool global, bool exclusive, bool isolate, bool solo_group)
{
	if (momentary) {
		_solo_release = new SoloMuteRelease (_route->soloed());
	}
	
	if (global) {
		
		if (_solo_release) {
			_solo_release->routes = _session->get_routes ();
		}
		
		if (Config->get_solo_control_is_listen_control()) {
			_session->set_listen (_session->get_routes(), !_route->listening(),  Session::rt_cleanup, true);
		} else {
			_session->set_solo (_session->get_routes(), !_route->soloed(),  Session::rt_cleanup, true);
		}
		
	} else if (exclusive) {
		
		if (_solo_release) {
			_solo_release->exclusive = true;
			
			boost::shared_ptr<RouteList> routes = _session->get_routes();
			
			for (RouteList::iterator i = routes->begin(); i != routes->end(); ++i) {
				if ((*i)->soloed ()) {
					_solo_release->routes_on->push_back (*i);
				} else {
					_solo_release->routes_off->push_back (*i);
				}
			}
		}
		
		if (Config->get_solo_control_is_listen_control()) {
			/* ??? we need a just_one_listen() method */
		} else {
			_session->set_just_one_solo (_route, true);
		}
		
	} else if (isolate) {
		
		// shift-click: toggle solo isolated status
		
		_route->set_solo_isolated (!_route->solo_isolated(), this);
		delete _solo_release;
		_solo_release = 0;
		
	} else if (solo_group) {
		
		/* Primary-button1: solo mix group.
		   NOTE: Primary-button2 is MIDI learn.
		*/
		
		if (_route->route_group()) {
			
			if (_solo_release) {
				_solo_release->routes = _route->route_group()->route_list();
			}
			
			if (Config->get_solo_control_is_listen_control()) {
				_session->set_listen (_route->route_group()->route_list(), !_route->listening(),  Session::rt_cleanup, true);
			} else {
				_session->set_solo (_route->route_group()->route_list(), !_route->soloed(),  Session::rt_cleanup, true);
			}
		}
		
	} else {
		
		/* click: solo this route */
		
		boost::shared_ptr<RouteList> rl (new RouteList);
		rl->push_back (route());
		
		if (_solo_release) {
			_solo_release->routes = rl;
		}
		
		if (Config->get_solo_control_is_listen_control()) {
			_session->set_listen (rl, !_route->listening());
		} else {
			_session->set_solo (rl, !_route->soloed());
		}
	}
}
#endif
