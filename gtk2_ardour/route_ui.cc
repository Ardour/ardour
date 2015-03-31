/*
    Copyright (C) 2002-2006 Paul Davis

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

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/choice.h>
#include <gtkmm2ext/doi.h>
#include <gtkmm2ext/bindable_button.h>
#include <gtkmm2ext/barcontroller.h>
#include <gtkmm2ext/gtk_ui.h>

#include "ardour/profile.h"
#include "ardour/route_group.h"
#include "ardour/dB.h"
#include "pbd/memento_command.h"
#include "pbd/stacktrace.h"
#include "pbd/controllable.h"
#include "pbd/enumwriter.h"

#include "ardour_ui.h"
#include "editor.h"
#include "route_ui.h"
#include "ardour_button.h"
#include "keyboard.h"
#include "utils.h"
#include "prompter.h"
#include "gui_thread.h"
#include "ardour_dialog.h"
#include "latency_gui.h"
#include "mixer_strip.h"
#include "automation_time_axis.h"
#include "route_time_axis.h"
#include "group_tabs.h"
#include "waves_message_dialog.h"

#include "ardour/audio_track.h"
#include "ardour/audioengine.h"
#include "ardour/filename_extensions.h"
#include "ardour/midi_track.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/template_utils.h"

#include "i18n.h"

#include "dbg_msg.h"

using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;

const char* RouteUI::XMLColor[15] = { "#9a2f16", "#9a4618", "#9a5f1a", "#f48e16", "#f4ad16",
									  "#4b7c0d", "#1c9a4a", "#1c9b7f", "#1d9b9b", "#197f9a",
									  "#4937a3", "#6f2ba1", "#7e1a99", "#9a177e", "#242424"};

Gtk::TargetEntry RouteUI::header_target("HEADER", Gtk::TARGET_SAME_APP | Gtk::TARGET_OTHER_WIDGET,ROUTE_HEADER);

uint32_t RouteUI::_max_invert_buttons = 3;
PBD::Signal1<void, boost::shared_ptr<Route> > RouteUI::BusSendDisplayChanged;
boost::weak_ptr<Route> RouteUI::_showing_sends_to;

RouteUI::RouteUI (ARDOUR::Session* sess, const std::string& layout_script_file)
	: AxisView (sess)
	, Gtk::EventBox ()
	, WavesUI (layout_script_file, *this)
	, mute_menu (0)
	, solo_menu (0)
	, sends_menu (0)
	, record_menu (0)
	, _invert_menu (0)
	, master_mute_button (get_waves_button ("master_mute_button"))
	, mute_button (get_waves_button ("mute_button"))
	, solo_button (get_waves_button ("solo_button"))
	, rec_enable_button (get_waves_button ("rec_enable_button"))
	, show_sends_button (get_waves_button ("show_sends_button"))
	, monitor_input_button (get_waves_button ("monitor_input_button"))
    , rec_enable_indicator_on (get_widget("rec_enable_indicator_on"))
    , rec_enable_indicator_off (get_widget("rec_enable_indicator_off"))
	, _dnd_operation_in_progress (false)
    , _dnd_operation_enabled (false)
{
	set_attributes (*this, *xml_tree ()->root (), XMLNodeMap ());
	if (sess) init ();
}

RouteUI::~RouteUI()
{
    // remove RouteUI property node from the GUI state
    // must be handled before reseting _route
    gui_object_state().remove_node(route_state_id() );
    
	_route.reset (); /* drop reference to route, so that it can be cleaned up */
	route_connections.drop_connections ();
    
	delete solo_menu;
	delete mute_menu;
	delete sends_menu;
    delete record_menu;
	delete _invert_menu;
}

void
RouteUI::init ()
{
	self_destruct = true;
	mute_menu = 0;
	solo_menu = 0;
	sends_menu = 0;
    record_menu = 0;
	_invert_menu = 0;
	pre_fader_mute_check = 0;
	post_fader_mute_check = 0;
	listen_mute_check = 0;
	main_mute_check = 0;
    solo_safe_check = 0;
    solo_isolated_check = 0;
    _momentary_solo = false;
	denormal_menu_item = 0;
    step_edit_item = 0;
	_i_am_the_modifier = 0;
    _momentary_solo = 0;
    
    // Not used in TracksLive
	//setup_invert_buttons ();

	_session->SoloChanged.connect (_session_connections, invalidator (*this), boost::bind (&RouteUI::solo_changed_so_update_mute, this), gui_context());
	_session->TransportStateChange.connect (_session_connections, invalidator (*this), boost::bind (&RouteUI::check_rec_enable_sensitivity, this), gui_context());
	_session->RecordStateChanged.connect (_session_connections, invalidator (*this), boost::bind (&RouteUI::session_rec_enable_changed, this), gui_context());

	_session->config.ParameterChanged.connect (*this, invalidator (*this), boost::bind (&RouteUI::parameter_changed, this, _1), gui_context());
	Config->ParameterChanged.connect (*this, invalidator (*this), boost::bind (&RouteUI::parameter_changed, this, _1), gui_context());

	rec_enable_button.signal_button_press_event().connect (sigc::mem_fun(*this, &RouteUI::rec_enable_press), false);
	rec_enable_button.signal_button_release_event().connect (sigc::mem_fun(*this, &RouteUI::rec_enable_release), false);

	show_sends_button.signal_button_press_event().connect (sigc::mem_fun(*this, &RouteUI::show_sends_press), false);
	show_sends_button.signal_button_release_event().connect (sigc::mem_fun(*this, &RouteUI::show_sends_release));

	solo_button.signal_button_press_event().connect (sigc::mem_fun(*this, &RouteUI::solo_press), false);
	solo_button.signal_button_release_event().connect (sigc::mem_fun(*this, &RouteUI::solo_release), false);
	mute_button.signal_button_press_event().connect (sigc::mem_fun(*this, &RouteUI::mute_press), false);
	master_mute_button.signal_button_press_event().connect (sigc::mem_fun(*this, &RouteUI::mute_press), false);
	mute_button.signal_button_release_event().connect (sigc::mem_fun(*this, &RouteUI::mute_release), false);
	master_mute_button.signal_button_release_event().connect (sigc::mem_fun(*this, &RouteUI::mute_release), false);
	
//	monitor_input_button.set_distinct_led_click (false);

	monitor_input_button.signal_button_press_event().connect (sigc::mem_fun(*this, &RouteUI::monitor_input_press), false);
	monitor_input_button.signal_button_release_event().connect (sigc::mem_fun(*this, &RouteUI::monitor_input_release), false);

	BusSendDisplayChanged.connect_same_thread (*this, boost::bind(&RouteUI::bus_send_display_changed, this, _1));
    
    // DnD callbacks
    // source side
    signal_drag_begin().connect (sigc::mem_fun(*this, &RouteUI::on_route_drag_begin));
    signal_drag_data_get().connect (sigc::mem_fun(*this, &RouteUI::on_route_drag_data_get));
    signal_drag_end().connect (sigc::mem_fun(*this, &RouteUI::on_route_drag_end));
    
    // destination callbacks
    signal_drag_motion().connect (sigc::mem_fun(*this, &RouteUI::on_route_drag_motion));
    signal_drag_leave().connect (sigc::mem_fun(*this, &RouteUI::on_route_drag_leave));
    signal_drag_drop().connect (sigc::mem_fun(*this, &RouteUI::on_route_drag_drop));
    signal_drag_data_received().connect (sigc::mem_fun(*this, &RouteUI::on_route_drag_data_received));
}

void
RouteUI::reset ()
{
	route_connections.drop_connections ();

	delete solo_menu;
	solo_menu = 0;

	delete mute_menu;
	mute_menu = 0;

	denormal_menu_item = 0;
}

void
RouteUI::self_delete ()
{
	delete this;
}

namespace {
    size_t default_palette_color = 9;
    size_t master_color = 3;
}

void
RouteUI::set_route (boost::shared_ptr<Route> rp)
{
    if ( !_session )
        return;
    
	reset ();

	_route = rp;

	if (set_color_from_route()) {
        Gdk::Color color;
        Editor* editor = dynamic_cast<Editor*>( &(ARDOUR_UI::instance()->the_editor()) );
        
		if ( _route->is_master() )
            color = (Gdk::Color)(XMLColor[master_color]);
		else
		    color = (Gdk::Color)(XMLColor[default_palette_color]);

        set_color (color);
	}

	rp->DropReferences.connect (route_connections, 
								invalidator (*this),
								boost::bind (self_destruct ? &RouteUI::self_delete : &RouteUI::reset,
											 this), gui_context());

	mute_button.set_controllable (_route->mute_control());
	master_mute_button.set_controllable (_route->mute_control());
	solo_button.set_controllable (_route->solo_control());

	_route->active_changed.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::route_active_changed, this), gui_context());
	_route->mute_changed.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::mute_changed, this, _1), gui_context());

	_route->solo_changed.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::update_solo_display, this), gui_context());
	_route->solo_safe_changed.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::update_solo_display, this), gui_context());
	_route->listen_changed.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::update_solo_display, this), gui_context());
	_route->solo_isolated_changed.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::update_solo_display, this), gui_context());
    
	_route->PropertyChanged.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::property_changed, this, _1), gui_context());

    // Not used in TracksLive
    //_route->phase_invert_changed.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::polarity_changed, this), gui_context());
	//_route->io_changed.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::setup_invert_buttons, this), gui_context ());
    
	_route->gui_changed.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::route_gui_changed, this, _1), gui_context ());

	if (_session && _session->writable() && is_track()) {
		boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track>(_route);

		t->RecordEnableChanged.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::route_rec_enable_changed, this), gui_context());
		
		rec_enable_button.show();
 		rec_enable_button.set_controllable (t->rec_enable_control());

                if (is_midi_track()) {
                        midi_track()->StepEditStatusChange.connect (route_connections, invalidator (*this),
                                                                    boost::bind (&RouteUI::step_edit_changed, this, _1), gui_context());
                }

	} 

	/* this will work for busses and tracks, and needs to be called to
	   set up the name entry/name label display.
	*/

	update_rec_display ();

	if (is_track()) {
		boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track>(_route);
		t->MonitoringChanged.connect (route_connections, invalidator (*this), boost::bind (&RouteUI::monitoring_changed, this), gui_context());

		update_monitoring_display ();
	}

	mute_button.unset_flags (Gtk::CAN_FOCUS);
	master_mute_button.unset_flags (Gtk::CAN_FOCUS);
	solo_button.unset_flags (Gtk::CAN_FOCUS);

    mute_button.set_visible ( !_route->is_master() );
    solo_button.set_visible ( !_route->is_master() );
    rec_enable_button.set_visible ( !_route->is_master() );
    monitor_input_button.set_visible ( !_route->is_master() );

	map_frozen ();

    // Not used in TracksLive
	//setup_invert_buttons ();
	//set_invert_button_state ();

	boost::shared_ptr<Route> s = _showing_sends_to.lock ();
	bus_send_display_changed (s);

	update_mute_display ();
	update_solo_display ();
}

void RouteUI::enable_header_dnd ()
{
    std::vector<Gtk::TargetEntry> targets;
    targets.push_back(header_target);
    drag_source_set(targets, Gdk::BUTTON1_MASK);
    drag_dest_set(targets, DEST_DEFAULT_HIGHLIGHT);
    _dnd_operation_enabled = true;
}

bool RouteUI::disable_header_dnd ()
{
    // disable DnD operations
    drag_source_unset ();
    drag_dest_unset ();
    _dnd_operation_enabled = false;
}

void
RouteUI::on_route_drag_begin(const Glib::RefPtr<Gdk::DragContext>& context)
{    
	_dnd_operation_in_progress = true;
    handle_route_drag_begin(context);
}

void
RouteUI::on_route_drag_end(const Glib::RefPtr<Gdk::DragContext>& context)
{
	_dnd_operation_in_progress = false;
	handle_route_drag_end(context);
}

void
RouteUI::on_route_drag_data_get(const Glib::RefPtr<Gdk::DragContext>& context, Gtk::SelectionData& selection_data, guint info, guint time)
{
    switch (info)
    {
        case RouteUI::ROUTE_HEADER:
        {
            // Put route id, if we have a route
            if (_route) {
                std::string route_id_string =_route->id().to_s();
                selection_data.set(8, (const guint8*)route_id_string.c_str(), route_id_string.length() + 1  );
                break;
            }
        }
            
        default:
            break;
    }
}

bool
RouteUI::on_route_drag_motion(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, guint time)
{
    return handle_route_drag_motion(context, x, y, time);
}

void
RouteUI::on_route_drag_leave(const Glib::RefPtr<Gdk::DragContext>& context, guint time)
{
    handle_route_drag_leave(context, time);
}

bool
RouteUI::on_route_drag_drop(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, guint time)
{
    if (RouteUI::header_target.get_target () == drag_dest_find_target (context) )
    {
        // request the data from the source:
        drag_get_data (context, RouteUI::header_target.get_target (), time );
        return true;
    }
    
    return false;
}

void
RouteUI::on_route_drag_data_received(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, const Gtk::SelectionData& selection_data, guint info, guint time)
{
    handle_route_drag_data_received(context, x, y, selection_data, info, time);
}

void
RouteUI::polarity_changed ()
{
    if (!_route) {
            return;
    }

	set_invert_button_state ();
}

bool
RouteUI::is_selected ()
{
    TimeAxisView* tv = ARDOUR_UI::instance()->the_editor().get_route_view_by_route_id (_route->id() );
    return tv->is_selected ();
}

boost::shared_ptr<ARDOUR::RouteList>
RouteUI::get_selected_route_list ()
{
    boost::shared_ptr<ARDOUR::RouteList> selected_route_list(new RouteList);
    
    TrackSelection& tracks_selection = ARDOUR_UI::instance()->the_editor().get_selection().tracks;
    
    for(std::list<TimeAxisView*>::iterator it = tracks_selection.begin(); it != tracks_selection.end(); it++) {
        
        RouteUI* t = dynamic_cast<RouteUI*> (*it);
        if (t) {
            selected_route_list->push_back (t->route());
        }
    }
    
    return selected_route_list;
}

bool
RouteUI::mute_press (GdkEventButton* ev)
{    
	if (ev->button != 1) {
		return true;
	}

#if 0
//    Groups will be able to use in the future, but Tracks Live doesn't support it now
    if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
        
        
        /* Primary-button1 applies change to the mix group even if it is not active
         NOTE: Primary-button2 is MIDI learn.
         */
        
        boost::shared_ptr<RouteList> rl;
        
        if (ev->button == 1) {
            
            if (_route->route_group()) {
                
                rl = _route->route_group()->route_list();
                
                if (_mute_release) {
                    _mute_release->routes = rl;
                }
            } else {
                rl.reset (new RouteList);
                rl->push_back (_route);
            }
            
            _session->set_mute (rl, !_route->muted(), Session::rt_cleanup, true);
        }
        
    } else {
#endif
    
    if ( is_selected () ) {
        // change mute state of the all selected tracks
        
        boost::shared_ptr<ARDOUR::RouteList> selected_route_list = get_selected_route_list ();
        _session->set_mute (selected_route_list, !_route->muted());
    } else {
        // change mute state of the single cliced track
        
        boost::shared_ptr<RouteList> rl (new RouteList);
        rl->push_back (_route);
        _session->set_mute (rl, !_route->muted());
    }
    
	return true;
}
        
bool
RouteUI::mute_release (GdkEventButton*)
{
	return true;
}

bool
RouteUI::solo_press(GdkEventButton* ev)
{
	/* ignore double/triple clicks */

	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS || ev->button == 2 ) {
		return true;
	}

    /* Goal here is to use Ctrl as the modifier on all platforms.
       This is questionable design, but here's how we do it.
    */
	int control_modifier;
    int alt_modifier;
#ifdef __APPLE__
    control_modifier = Keyboard::SecondaryModifier; /* Control */
    alt_modifier = Keyboard::Level4Modifier; /* Alt */
#else
    /* Anything except OS X */
    control_modifier = Keyboard::PrimaryModifier;  /* Control */
    alt_modifier = Keyboard::SecondaryModifier; /* Alt */
#endif
    int momentary_mask = alt_modifier | Keyboard::TertiaryModifier; /* Alt+Shift */

    if (Keyboard::is_context_menu_event (ev)) {
        if (!ARDOUR::Profile->get_trx ()) {
            if (solo_menu == 0) {
                build_solo_menu ();
            }
            solo_menu->popup (1, ev->time);
        }
    } else {
		if ( ev->button == 1 )
        {
            if (Keyboard::modifier_state_equals (ev->state, control_modifier)) {
                
                // control-click: toggle solo isolated status
                if ( is_selected () ) {
                    boost::shared_ptr<ARDOUR::RouteList> selected_route_list = get_selected_route_list ();
                    _session->set_solo_isolated_force (selected_route_list, !_route->solo_isolated(), Session::rt_cleanup, true);
                } else {
                    _route->set_solo_isolated_force (!_route->solo_isolated(), _session);
                }
			} else
            if (Keyboard::modifier_state_equals (ev->state, alt_modifier)) {
                
                // alt-click: toogle Solo X-Or
                
				if (Config->get_solo_control_is_listen_control()) {
					_session->set_listen (_session->get_routes(), false,  Session::rt_cleanup, true);
				} else {
					_session->set_solo (_session->get_routes(), false,  Session::rt_cleanup, true);
				}
                
                boost::shared_ptr<RouteList> rl (new RouteList);
                
                if ( is_selected () ) {
                    rl = get_selected_route_list ();
                } else {
                    rl->push_back (_route);
                }
                
				if (Config->get_solo_control_is_listen_control()) {
					_session->set_listen (rl, true);
				} else {
					_session->set_solo (rl, true);
				}
            } else {
                if (Keyboard::modifier_state_equals (ev->state, momentary_mask)) {
                    
                    // alt+shift-click: toogle momentary solo
                    
                    _momentary_solo = new SoloMuteRelease(true);
                    // is used for muting tracks back after momentary solo release
                    _momentary_solo->routes_on = boost::shared_ptr<ARDOUR::RouteList>(new ARDOUR::RouteList);
                    // is used for unsoloing tracks after momentary solo release
                    _momentary_solo->routes_off = boost::shared_ptr<ARDOUR::RouteList>(new ARDOUR::RouteList);
                    
                    if ( is_selected () ) {
                    
                        // is used for unsoloing tracks after momentary solo release
                        _momentary_solo->routes_off = get_selected_route_list ();
                        
                        for (RouteList::iterator it = _momentary_solo->routes_off->begin(); it != _momentary_solo->routes_off->end(); ++it) {
                            if ( (*it)->muted() )
                                _momentary_solo->routes_on->push_back ( *it );
                        }
                    } else {
                        // is used for unsoloing tracks after momentary solo release
                        _momentary_solo->routes_off->push_back (_route);
                        if (_route->muted ())
                            _momentary_solo->routes_on->push_back (_route);
                    }
                }
                
				/* click: solo this route */

                if ( is_selected () ) {
                    
                    boost::shared_ptr<ARDOUR::RouteList> selected_route_list = get_selected_route_list ();

                    if (Config->get_solo_control_is_listen_control()) {
                        _session->set_listen (selected_route_list, !_route->listening_via_monitor(), Session::rt_cleanup, true);
                    } else {

                        _session->set_solo (selected_route_list, !_route->self_soloed(), Session::rt_cleanup, true);
                    }
                } else {
                    boost::shared_ptr<RouteList> rl (new RouteList);
                    rl->push_back (_route);
                    
                    if (Config->get_solo_control_is_listen_control()) {
                        _session->set_listen (rl, !_route->listening_via_monitor(), Session::rt_cleanup, true);
                    } else {
                        _session->set_solo (rl, !_route->self_soloed(), Session::rt_cleanup, true);
                    }
                }
			}
		} // if ( ev->button == 1 )
	}

	return true;
}

bool
RouteUI::solo_release (GdkEventButton*)
{
    if ( _momentary_solo )
    {
        // unsolo tracks after momentary solo release
        _session->set_solo (_momentary_solo->routes_off, false);
        
        // mute tracks back after momentary solo release
        _session->set_mute (_momentary_solo->routes_on, true);
        
        delete _momentary_solo;
        _momentary_solo = 0;
    }

	return true;
}

bool
RouteUI::rec_enable_press(GdkEventButton* ev)
{
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	if (!_session->engine().connected()) {
        WavesMessageDialog msg ("", _("Not connected to AudioEngine - cannot engage record"));
		msg.run ();
		return true;
	}

//    Tracks Live doesn't use it
#if 0
    if (is_midi_track())
    {
        /* rec-enable button exits from step editing */

        if (midi_track()->step_editing())
        {
            midi_track()->set_step_editing (false);
            return true;
        }
    }
#endif
    
	if (is_track()) {

		if (Keyboard::is_button2_event (ev)) {

			// do nothing on midi sigc::bind event
			return rec_enable_button.on_button_press_event (ev);

		}
#if 0
//    Groups will be able to use in the future, but Tracks Live doesn't support it now
            else if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {

			/* Primary-button1 applies change to the route group (even if it is not active)
			   NOTE: Primary-button2 is MIDI learn.
			*/

			if (ev->button == 1) {

				boost::shared_ptr<RouteList> rl;
				
				if (_route->route_group()) {
					
					rl = _route->route_group()->route_list();
					
				} else {
					rl.reset (new RouteList);
					rl->push_back (_route);
				}

				_session->set_record_enabled (rl, !rec_enable_button.active_state(), Session::rt_cleanup, true);
			}
#endif
		else if (Keyboard::is_context_menu_event (ev)) {

			/* do this on release */

		} else {
            boost::shared_ptr<RouteList> rl (new RouteList);
            
            if ( is_selected () ) {
                rl = get_selected_route_list ();
            } else {
                rl->push_back (_route);
            }
            /* Record enable uses EditorRoutes::redisplay N time. Where N is the number of changed tracks. 
             DisplaySuspender is used in order to improve GUI performance */
            DisplaySuspender ds;
            _session->set_record_enabled (rl, !rec_enable_button.active_state(), Session::rt_cleanup, true);
		}
	}

	return true;
}

bool
RouteUI::rec_enable_release (GdkEventButton* ev)
{
#if 0
    if (Keyboard::is_context_menu_event (ev)) {
        build_record_menu ();
        if (record_menu) {
            record_menu->popup (1, ev->time);
        }
        return true;
    }
#endif
    
	return true;
}

void
RouteUI::monitoring_changed ()
{
	update_monitoring_display ();
}

void
RouteUI::update_monitoring_display ()
{
	if (!_route) {
		return;
	}

	boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track>(_route);

	if (!t) {
		return;
	}

	if (t->monitoring_choice() & MonitorInput) {
		monitor_input_button.set_active_state (Gtkmm2ext::ExplicitActive);
	} else {
        monitor_input_button.unset_active_state ();
	}
}

bool
RouteUI::monitor_input_press(GdkEventButton* ev)
{
    return monitor_press (ev, MonitorInput);
;
}

bool
RouteUI::monitor_input_release(GdkEventButton*)
{
	return true;
}

bool
RouteUI::monitor_disk_press (GdkEventButton* ev)
{
	return monitor_press (ev, MonitorDisk);
}

bool
RouteUI::monitor_disk_release (GdkEventButton*)
{
	return true;
}

bool
RouteUI::monitor_press (GdkEventButton* ev, MonitorChoice monitor_choice)
{	
	if (ev->button != 1) {
		return false;
	}

	boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track>(_route);

	if (!t) {
		return true;
	}

	MonitorChoice mc;
		
	/* XXX for now, monitoring choices are orthogonal. cue monitoring 
	   will follow in 3.X but requires mixing the input and playback (disk)
	   signal together, which requires yet more buffers.
	*/

	if (t->monitoring_choice() & monitor_choice) {
		mc = MonitorChoice (t->monitoring_choice() & ~monitor_choice);
	} else {
		/* this line will change when the options are non-orthogonal */
		// mc = MonitorChoice (t->monitoring_choice() | monitor_choice);
		mc = monitor_choice;
	}

#if 0
    if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
		if (_route->route_group() && _route->route_group()->is_monitoring()) {
			rl = _route->route_group()->route_list();
		} else {
			rl.reset (new RouteList);
			rl->push_back (_route);
		}
	}
#endif
    
    boost::shared_ptr<RouteList> rl(new RouteList);

    if ( is_selected () ) {
        rl = get_selected_route_list ();
    } else {
        rl->push_back (_route);
    }
    _session->set_monitoring (rl, mc, Session::rt_cleanup, true);

	return true;
}

void
RouteUI::build_record_menu ()
{
        if (record_menu) {
                return;
        }

        /* no rec-button context menu for non-MIDI tracks
         */

        if (is_midi_track()) {
                record_menu = new Menu;
                record_menu->set_name ("ArdourContextMenu");

                using namespace Menu_Helpers;
                MenuList& items = record_menu->items();

                items.push_back (CheckMenuElem (_("Step Entry"), sigc::mem_fun (*this, &RouteUI::toggle_step_edit)));
                step_edit_item = dynamic_cast<Gtk::CheckMenuItem*> (&items.back());

                if (_route->record_enabled()) {
                        step_edit_item->set_sensitive (false);
                }

                step_edit_item->set_active (midi_track()->step_editing());
        }
}

void
RouteUI::toggle_step_edit ()
{
        if (!is_midi_track() || _route->record_enabled()) {
                return;
        }

        midi_track()->set_step_editing (step_edit_item->get_active());
}

void
RouteUI::step_edit_changed (bool yn)
{
    if (yn) {
        rec_enable_button.set_active_state (Gtkmm2ext::ExplicitActive);
        start_step_editing ();
        if (step_edit_item) {
            step_edit_item->set_active (true);
        }
    } else {
        rec_enable_button.unset_active_state ();
        stop_step_editing ();
        if (step_edit_item) {
            step_edit_item->set_active (false);
        }
    }
}

void
RouteUI::build_sends_menu ()
{
	using namespace Menu_Helpers;

	sends_menu = new Menu;
	sends_menu->set_name ("ArdourContextMenu");
	MenuList& items = sends_menu->items();

	items.push_back (
		MenuElem(_("Assign all tracks (prefader)"), sigc::bind (sigc::mem_fun (*this, &RouteUI::create_sends), PreFader, false))
		);

	items.push_back (
		MenuElem(_("Assign all tracks and buses (prefader)"), sigc::bind (sigc::mem_fun (*this, &RouteUI::create_sends), PreFader, true))
		);

	items.push_back (
		MenuElem(_("Assign all tracks (postfader)"), sigc::bind (sigc::mem_fun (*this, &RouteUI::create_sends), PostFader, false))
		);

	items.push_back (
		MenuElem(_("Assign all tracks and buses (postfader)"), sigc::bind (sigc::mem_fun (*this, &RouteUI::create_sends), PostFader, true))
		);

	items.push_back (
		MenuElem(_("Assign selected tracks (prefader)"), sigc::bind (sigc::mem_fun (*this, &RouteUI::create_selected_sends), PreFader, false))
		);

	items.push_back (
		MenuElem(_("Assign selected tracks and buses (prefader)"), sigc::bind (sigc::mem_fun (*this, &RouteUI::create_selected_sends), PreFader, true)));

	items.push_back (
		MenuElem(_("Assign selected tracks (postfader)"), sigc::bind (sigc::mem_fun (*this, &RouteUI::create_selected_sends), PostFader, false))
		);

	items.push_back (
		MenuElem(_("Assign selected tracks and buses (postfader)"), sigc::bind (sigc::mem_fun (*this, &RouteUI::create_selected_sends), PostFader, true))
		);

	items.push_back (MenuElem(_("Copy track/bus gains to sends"), sigc::mem_fun (*this, &RouteUI::set_sends_gain_from_track)));
	items.push_back (MenuElem(_("Set sends gain to -inf"), sigc::mem_fun (*this, &RouteUI::set_sends_gain_to_zero)));
	items.push_back (MenuElem(_("Set sends gain to 0dB"), sigc::mem_fun (*this, &RouteUI::set_sends_gain_to_unity)));

}

void
RouteUI::create_sends (Placement p, bool include_buses)
{
    if ( _session )
        _session->globally_add_internal_sends (_route, p, include_buses);
}

void
RouteUI::create_selected_sends (Placement p, bool include_buses)
{
	boost::shared_ptr<RouteList> rlist (new RouteList);
	TrackSelection& selected_tracks (ARDOUR_UI::instance()->the_editor().get_selection().tracks);

	for (TrackSelection::iterator i = selected_tracks.begin(); i != selected_tracks.end(); ++i) {
		RouteTimeAxisView* rtv;
		RouteUI* rui;
		if ((rtv = dynamic_cast<RouteTimeAxisView*>(*i)) != 0) {
			if ((rui = dynamic_cast<RouteUI*>(rtv)) != 0) {
				if (include_buses || boost::dynamic_pointer_cast<AudioTrack>(rui->route())) {
					rlist->push_back (rui->route());
				}
			}
		}
	}
    
    if ( _session )
        _session->add_internal_sends (_route, p, rlist);
}

void
RouteUI::set_sends_gain_from_track ()
{
    if ( _session )
        _session->globally_set_send_gains_from_track (_route);
}

void
RouteUI::set_sends_gain_to_zero ()
{
    if ( _session )
        _session->globally_set_send_gains_to_zero (_route);
}

void
RouteUI::set_sends_gain_to_unity ()
{
	if ( _session )
        _session->globally_set_send_gains_to_unity (_route);
}

bool
RouteUI::show_sends_press(GdkEventButton* ev)
{
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS ) {
		return true;
	}

	if (!is_track()) {

		if (Keyboard::is_button2_event (ev) && Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {

			// do nothing on midi sigc::bind event
			return false;

		} else if (Keyboard::is_context_menu_event (ev)) {

			if (sends_menu == 0) {
				build_sends_menu ();
			}

			sends_menu->popup (0, ev->time);

		} else {

			boost::shared_ptr<Route> s = _showing_sends_to.lock ();

			if (s == _route) {
				set_showing_sends_to (boost::shared_ptr<Route> ());
			} else {
				set_showing_sends_to (_route);
			}
		}
	}

	return true;
}

bool
RouteUI::show_sends_release (GdkEventButton*)
{
	return true;
}

void
RouteUI::send_blink (bool onoff)
{
	if (onoff) {
		show_sends_button.set_active_state (Gtkmm2ext::ExplicitActive);
	} else {
		show_sends_button.unset_active_state ();
	}
}

Gtkmm2ext::ActiveState
RouteUI::solo_active_state (boost::shared_ptr<Route> r)
{
	if (r->is_master() || r->is_monitor()) {
		return Gtkmm2ext::Off;
	}

	if (Config->get_solo_control_is_listen_control()) {

		if (r->listening_via_monitor()) {
			return Gtkmm2ext::ExplicitActive;
		} else {
			return Gtkmm2ext::Off;
		}
	}

	if (r->soloed()) {
        if (!r->self_soloed()) {
            return Gtkmm2ext::ImplicitActive;
        } else {
            return Gtkmm2ext::ExplicitActive;
        }
	} else {
		return r->solo_isolated() ? Gtkmm2ext::ImplicitActive : Gtkmm2ext::Off;
	}
}

Gtkmm2ext::ActiveState
RouteUI::solo_isolate_active_state (boost::shared_ptr<Route> r)
{
	if (r->is_master() || r->is_monitor()) {
		return Gtkmm2ext::Off;
	}

	if (r->solo_isolated()) {
		return Gtkmm2ext::ExplicitActive;
	} else {
		return Gtkmm2ext::Off;
	}
}

Gtkmm2ext::ActiveState
RouteUI::solo_safe_active_state (boost::shared_ptr<Route> r)
{
	if (r->is_master() || r->is_monitor()) {
		return Gtkmm2ext::Off;
	}

	if (r->solo_safe()) {
		return Gtkmm2ext::ExplicitActive;
	} else {
		return Gtkmm2ext::Off;
	}
}

void
RouteUI::update_solo_display ()
{
	bool yn = _route->solo_safe ();

	if (solo_safe_check && solo_safe_check->get_active() != yn) {
		solo_safe_check->set_active (yn);
	}

	yn = _route->solo_isolated ();

	if (solo_isolated_check && solo_isolated_check->get_active() != yn) {
		solo_isolated_check->set_active (yn);
	}

    set_button_names ();

/*	if (solo_isolated_led) {
		if (_route->solo_isolated()) {
			solo_isolated_led->set_active_state (Gtkmm2ext::ExplicitActive);
		} else {
			solo_isolated_led->unset_active_state ();
		}
    }*/

/*	if (solo_safe_led) {
		if (_route->solo_safe()) {
			solo_safe_led.set_active_state (Gtkmm2ext::ExplicitActive);
		} else {
			solo_safe_led.unset_active_state ();
		}
    }*/

	solo_button.set_active_state (solo_active_state (_route));

    /* some changes to solo status can affect mute display, so catch up
     */
    update_mute_display ();
}

void
RouteUI::solo_changed_so_update_mute ()
{
	update_mute_display ();
}

void
RouteUI::mute_changed(void* /*src*/)
{
	update_mute_display ();
}

ActiveState
RouteUI::mute_active_state (Session* s, boost::shared_ptr<Route> r)
{
	if (r->is_monitor()) {
		return ActiveState(0);
	}


	if (Config->get_show_solo_mutes() && !Config->get_solo_control_is_listen_control ()) {

		if (r->muted ()) {
			/* full mute */
			return Gtkmm2ext::ExplicitActive;
		} else if (!r->is_master() && s->soloing() && !r->soloed() && !r->solo_isolated()) {
			/* master is NEVER muted by others */
			return Gtkmm2ext::ImplicitActive;
		} else {
			/* no mute at all */
			return Gtkmm2ext::Off;
		}

	} else {

		if (r->muted()) {
			/* full mute */
			return Gtkmm2ext::ExplicitActive;
		} else {
			/* no mute at all */
			return Gtkmm2ext::Off;
		}
	}

	return ActiveState(0);
}

void
RouteUI::update_mute_display ()
{
	if (!_route) {
		return;
	}

	mute_button.set_active_state (mute_active_state (_session, _route));
	master_mute_button.set_active_state (mute_active_state (_session, _route));
}

void
RouteUI::route_rec_enable_changed ()
{
        update_rec_display ();
	update_monitoring_display ();
}

void
RouteUI::session_rec_enable_changed ()
{
        update_rec_display ();
	update_monitoring_display ();
}

void
RouteUI::update_rec_display ()
{
//	if (!rec_enable_button || !_route) {
	if (!_route) {
		return;
	}

	if (_route->record_enabled()) {
        switch (_session->record_status ()) {
        case Session::Recording:
                rec_enable_button.set_active_state (Gtkmm2ext::ExplicitActive);
                break;

        case Session::Disabled:
        case Session::Enabled:
                rec_enable_button.set_active_state (Gtkmm2ext::ImplicitActive);
                break;

        }

        if (step_edit_item) {
                step_edit_item->set_sensitive (false);
        }
	} else {
		rec_enable_button.unset_active_state ();

                if (step_edit_item) {
                        step_edit_item->set_sensitive (true);
                }
	}


	check_rec_enable_sensitivity ();
 
    update_rec_enable_indicator();
   
}

void
RouteUI::update_rec_enable_indicator()
{
    if (_route && _route->record_enabled ())
    {
        //switch-on rec_enable_indicator
        rec_enable_indicator_on.show();
        rec_enable_indicator_off.hide();
    }
    else
    {
        //switch-off rec_enable_indicator
        rec_enable_indicator_on.hide();
        rec_enable_indicator_off.show();
    }

     //rec_enable_indicator.set_state ((_route && _route->record_enabled ()) ? Gtk::STATE_ACTIVE : Gtk::STATE_NORMAL);
}

void
RouteUI::build_solo_menu (void)
{
	using namespace Menu_Helpers;

	solo_menu = new Menu;
	solo_menu->set_name ("ArdourContextMenu");
	MenuList& items = solo_menu->items();
	Gtk::CheckMenuItem* check;
	check = new Gtk::CheckMenuItem(_("Solo Safe"));
	check->set_active (_route->solo_isolated());
	check->signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &RouteUI::toggle_solo_isolated), check));
	items.push_back (CheckMenuElem(*check));
    solo_isolated_check = dynamic_cast<Gtk::CheckMenuItem*>(&items.back());
	check->show_all();
	/*
	check = new Gtk::CheckMenuItem(_("Solo Safe"));
	check->set_active (_route->solo_safe());
	check->signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &RouteUI::toggle_solo_safe), check));
	items.push_back (CheckMenuElem(*check));
    solo_safe_check = dynamic_cast<Gtk::CheckMenuItem*>(&items.back());
	check->show_all();
	*/
}

void
RouteUI::init_mute_menu(MuteMaster::MutePoint mp, Gtk::CheckMenuItem* check)
{
	check->set_active (_route->mute_points() & mp);
}

void
RouteUI::toggle_mute_menu(MuteMaster::MutePoint mp, Gtk::CheckMenuItem* check)
{
	if (check->get_active()) {
		_route->set_mute_points (MuteMaster::MutePoint (_route->mute_points() | mp));
	} else {
		_route->set_mute_points (MuteMaster::MutePoint (_route->mute_points() & ~mp));
	}
}

void
RouteUI::muting_change ()
{
	ENSURE_GUI_THREAD (*this, &RouteUI::muting_change)

	bool yn;
	MuteMaster::MutePoint current = _route->mute_points ();

	yn = (current & MuteMaster::PreFader);

	if (pre_fader_mute_check->get_active() != yn) {
		pre_fader_mute_check->set_active (yn);
	}

	yn = (current & MuteMaster::PostFader);

	if (post_fader_mute_check->get_active() != yn) {
		post_fader_mute_check->set_active (yn);
	}

	yn = (current & MuteMaster::Listen);

	if (listen_mute_check->get_active() != yn) {
		listen_mute_check->set_active (yn);
	}

	yn = (current & MuteMaster::Main);

	if (main_mute_check->get_active() != yn) {
		main_mute_check->set_active (yn);
	}
}

#if 0 /* not used in Tracks */
bool
RouteUI::solo_isolate_button_release (GdkEventButton* ev)
{
	if (ev->type == GDK_2BUTTON_PRESS || ev->type == GDK_3BUTTON_PRESS) {
		return true;
	}

	bool view = solo_isolated_led->active_state();
	bool model = _route->solo_isolated();

	/* called BEFORE the view has changed */

	if (ev->button == 1) {
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::TertiaryModifier))) {

			if (model) {
				/* disable isolate for all routes */
				DisplaySuspender ds;
				_session->set_solo_isolated (_session->get_routes(), false, Session::rt_cleanup, true);
			}

		} else {
			if (model == view) {

				/* flip just this route */

				boost::shared_ptr<RouteList> rl (new RouteList);
				rl->push_back (_route);
				DisplaySuspender ds;
				_session->set_solo_isolated (rl, !view, Session::rt_cleanup, true);
			}
		}
	}

	return true;
}
#endif

#if 0 /* not used in Tracks */
bool
RouteUI::solo_safe_button_release (GdkEventButton* ev)
{
	if (ev->button == 1) {
		_route->set_solo_safe (!solo_safe_led.active_state(), this);
		return true;
	}
	return false;
}
#endif

void
RouteUI::toggle_solo_isolated (Gtk::CheckMenuItem* check)
{
    bool view = check->get_active();
    bool model = _route->solo_isolated();

    /* called AFTER the view has changed */

    if (model != view) {
        _route->set_solo_isolated (view, this);
    }
}

void
RouteUI::toggle_solo_safe (Gtk::CheckMenuItem* check)
{
	_route->set_solo_safe (check->get_active(), this);
}

/** Ask the user to choose a colour, and then set all selected tracks
 *  to that colour.
 */
void
RouteUI::choose_color ()
{
	bool picked;
	Gdk::Color const color = Gtkmm2ext::UI::instance()->get_color (_("Color Selection"), picked, &_color);

	if (picked) {
		ARDOUR_UI::instance()->the_editor().get_selection().tracks.foreach_route_ui (
			boost::bind (&RouteUI::set_color, _1, color)
			);
	}
}

/** Set the route's own color.  This may not be used for display if
 *  the route is in a group which shares its color with its routes.
 */
void
RouteUI::set_color (const Gdk::Color & c)
{
	/* leave _color alone in the group case so that tracks can retain their
	 * own pre-group colors.
	 */

	char buf[64];
	_color = c;
	snprintf (buf, sizeof (buf), "%d:%d:%d", c.get_red(), c.get_green(), c.get_blue());
	
	/* note: we use the route state ID here so that color is the same for both
	   the time axis view and the mixer strip
	*/
	
	gui_object_state().set_property<string> (route_state_id(), X_("color"), buf);
	_route->gui_changed ("color", (void *) 0); /* EMIT_SIGNAL */
}

/** @return GUI state ID for things that are common to the route in all its representations */
string
RouteUI::route_state_id () const
{
	return string_compose (X_("route %1"), _route->id().to_s());
}

int
RouteUI::set_color_from_route ()
{
	const string str = gui_object_state().get_string (route_state_id(), X_("color"));

	if (str.empty()) {
		return 1;
	}

	int r, g, b;

	sscanf (str.c_str(), "%d:%d:%d", &r, &g, &b);

	_color.set_red (r);
	_color.set_green (g);
	_color.set_blue (b);

	return 0;
}

void
RouteUI::remove_this_route (bool apply_to_selection)
{
    if (!_session) {
        return;
    }
    
    boost::shared_ptr<RouteList> routes_to_remove(new RouteList);
    if (apply_to_selection) {      
        TrackSelection& track_selection =  ARDOUR_UI::instance()->the_editor().get_selection().tracks;
        
        for (list<TimeAxisView*>::iterator i = track_selection.begin(); i != track_selection.end(); ++i) {
            RouteUI* t = dynamic_cast<RouteUI*> (*i);
            if (t) {
                if ( t->route()->is_master() || t->route()->is_monitor() )
                    continue;
                
                routes_to_remove->push_back(t->route() );
            }
        }
	} else {
		if ( route()->is_master() || route()->is_monitor() )
			return;
        
        routes_to_remove->push_back(this->route() );
	}
    
    Session* session = ARDOUR_UI::instance()->the_session();
    
    if (session) {
        Session::StateProtector sp (session);
        session->remove_routes (routes_to_remove);
        routes_to_remove->clear ();
    }
}

gint
RouteUI::idle_remove_this_route (RouteUI *rui)
{
	rui->_session->remove_route (rui->route());
	return false;
}


/** @return true if this name should be used for the route, otherwise false */
bool
RouteUI::verify_new_route_name (const std::string& name)
{
	if (name.find (':') == string::npos) {
		return true;
	}
	
	WavesMessageDialog colon_msg ("waves_route_rename_dialog.xml", "", _("The use of colons (':') is discouraged in track and bus names.\nDo you want to use this new name?"),
                                  WavesMessageDialog::BUTTON_ACCEPT | WavesMessageDialog::BUTTON_CANCEL);
	
	return (colon_msg.run () == Gtk::RESPONSE_ACCEPT);
}

void
RouteUI::route_rename ()
{
	ArdourPrompter name_prompter (true);
	string result;
	bool done = false;

	if (is_track()) {
		name_prompter.set_title (_("Rename Track"));
	} else {
		name_prompter.set_title (_("Rename Bus"));
	}
	name_prompter.set_prompt (_("New name:"));
	name_prompter.set_initial_text (_route->name());
	name_prompter.add_button (_("Rename"), Gtk::RESPONSE_ACCEPT);
	name_prompter.set_response_sensitive (Gtk::RESPONSE_ACCEPT, false);
	name_prompter.show_all ();

	while (!done) {
		switch (name_prompter.run ()) {
		case Gtk::RESPONSE_ACCEPT:
			name_prompter.get_result (result);
			name_prompter.hide ();
			if (result.length()) {
				if (verify_new_route_name (result)) {
					_route->set_name (result);
					done = true;
				} else {
					/* back to name prompter */
				}

			} else {
				/* nothing entered, just get out of here */
				done = true;
			}
			break;
		default:
			done = true;
			break;
		}
	}

	return;

}

void
RouteUI::property_changed (const PropertyChange& what_changed)
{
	if (what_changed.contains (ARDOUR::Properties::name)) {
		name_label.set_text (_route->name());
	}
}

void
RouteUI::set_route_active (bool a, bool apply_to_selection)
{
	if (apply_to_selection) {
		ARDOUR_UI::instance()->the_editor().get_selection().tracks.foreach_route_ui (boost::bind (&RouteTimeAxisView::set_route_active, _1, a, false));
	} else {
		_route->set_active (a, this);
	}
}

void
RouteUI::toggle_denormal_protection ()
{
	if (denormal_menu_item) {

		bool x;

		ENSURE_GUI_THREAD (*this, &RouteUI::toggle_denormal_protection)

		if ((x = denormal_menu_item->get_active()) != _route->denormal_protection()) {
			_route->set_denormal_protection (x);
		}
	}
}

void
RouteUI::denormal_protection_changed ()
{
	if (denormal_menu_item) {
		denormal_menu_item->set_active (_route->denormal_protection());
	}
}

void
RouteUI::disconnect_input ()
{
	_route->input()->disconnect (this);
}

void
RouteUI::disconnect_output ()
{
	_route->output()->disconnect (this);
}

bool
RouteUI::is_track () const
{
	return boost::dynamic_pointer_cast<Track>(_route) != 0;
}

boost::shared_ptr<Track>
RouteUI::track() const
{
	return boost::dynamic_pointer_cast<Track>(_route);
}

bool
RouteUI::is_audio_track () const
{
	return boost::dynamic_pointer_cast<AudioTrack>(_route) != 0;
}

boost::shared_ptr<AudioTrack>
RouteUI::audio_track() const
{
	return boost::dynamic_pointer_cast<AudioTrack>(_route);
}

bool
RouteUI::is_midi_track () const
{
	return boost::dynamic_pointer_cast<MidiTrack>(_route) != 0;
}

boost::shared_ptr<MidiTrack>
RouteUI::midi_track() const
{
	return boost::dynamic_pointer_cast<MidiTrack>(_route);
}

bool
RouteUI::has_audio_outputs () const
{
	return (_route->n_outputs().n_audio() > 0);
}

string
RouteUI::name() const
{
	return _route->name();
}

void
RouteUI::map_frozen ()
{
	ENSURE_GUI_THREAD (*this, &RouteUI::map_frozen)

 	AudioTrack* at = dynamic_cast<AudioTrack*>(_route.get());

	if (at) {
		switch (at->freeze_state()) {
		case AudioTrack::Frozen:
			rec_enable_button.set_sensitive (false);
			break;
		default:
			rec_enable_button.set_sensitive (true);
			break;
		}
	}
}

void
RouteUI::adjust_latency ()
{
    if ( !_session )
        return;
    
	LatencyDialog dialog (_route->name() + _(" latency"), *(_route->output()), _session->frame_rate(), AudioEngine::instance()->samples_per_cycle());
}

void
RouteUI::save_as_template ()
{
	std::string path;
	std::string safe_name;
	string name;

	path = ARDOUR::user_route_template_directory ();

	if (g_mkdir_with_parents (path.c_str(), 0755)) {
		error << string_compose (_("Cannot create route template directory %1"), path) << endmsg;
		return;
	}

	Prompter p (true); // modal

	p.set_title (_("Save As Template"));
	p.set_prompt (_("Template name:"));
	p.add_button ("SAVE", Gtk::RESPONSE_ACCEPT);
	switch (p.run()) {
	case RESPONSE_ACCEPT:
		break;
	default:
		return;
	}

	p.hide ();
	p.get_result (name, true);

	safe_name = legalize_for_path (name);
	safe_name += template_suffix;

	path = Glib::build_filename (path, safe_name);

	_route->save_as_template (path, name);
}

void
RouteUI::check_rec_enable_sensitivity ()
{
	if (_session && _session->transport_rolling() && rec_enable_button.active_state() && Config->get_disable_disarm_during_roll()) {
		rec_enable_button.set_sensitive (false);
	} else {
		rec_enable_button.set_sensitive (true);
	}

	update_monitoring_display ();
}

void
RouteUI::parameter_changed (string const & p)
{
	/* this handles RC and per-session parameter changes */

	if (p == "disable-disarm-during-roll") {
		check_rec_enable_sensitivity ();
	} else if (p == "use-monitor-bus" || p == "solo-control-is-listen-control" || p == "listen-position") {
		set_button_names ();
	} else if (p == "auto-input") {
		update_monitoring_display ();
	}
}

void
RouteUI::step_gain_up ()
{
	_route->set_gain (dB_to_coefficient (accurate_coefficient_to_dB (_route->gain_control()->get_value()) + 0.1), this);
}

void
RouteUI::page_gain_up ()
{
	_route->set_gain (dB_to_coefficient (accurate_coefficient_to_dB (_route->gain_control()->get_value()) + 0.5), this);
}

void
RouteUI::step_gain_down ()
{
	_route->set_gain (dB_to_coefficient (accurate_coefficient_to_dB (_route->gain_control()->get_value()) - 0.1), this);
}

void
RouteUI::page_gain_down ()
{
	_route->set_gain (dB_to_coefficient (accurate_coefficient_to_dB (_route->gain_control()->get_value()) - 0.5), this);
}

void
RouteUI::open_remote_control_id_dialog ()
{
    if ( !_session )
        return;
    
	ArdourDialog dialog (_("Remote Control ID"));
	SpinButton* spin = 0;

	dialog.get_vbox()->set_border_width (18);

	if (Config->get_remote_model() == UserOrdered) {
		uint32_t const limit = _session->ntracks() + _session->nbusses () + 4;
		
		HBox* hbox = manage (new HBox);
		hbox->set_spacing (6);
		hbox->pack_start (*manage (new Label (_("Remote control ID:"))));
		spin = manage (new SpinButton);
		spin->set_digits (0);
		spin->set_increments (1, 10);
		spin->set_range (0, limit);
		spin->set_value (_route->remote_control_id());
		hbox->pack_start (*spin);
		dialog.get_vbox()->pack_start (*hbox);
		
		dialog.add_button ("CANCEL", RESPONSE_CANCEL);
		dialog.add_button ("APPLY", RESPONSE_ACCEPT);
	} else {
		Label* l = manage (new Label());
		if (_route->is_master() || _route->is_monitor()) {
			l->set_markup (string_compose (_("The remote control ID of %1 is: %2\n\n\n"
							 "The remote control ID of %3 cannot be changed."),
						       Glib::Markup::escape_text (_route->name()),
						       _route->remote_control_id(),
						       (_route->is_master() ? _("the master bus") : _("the monitor bus"))));
		} else {
			l->set_markup (string_compose (_("The remote control ID of %5 is: %2\n\n\n"
							 "Remote Control IDs are currently determined by track/bus ordering in %6.\n\n"
							 "%3Use the User Interaction tab of the Preferences window if you want to change this%4"),
						       (is_track() ? _("track") : _("bus")),
						       _route->remote_control_id(),
						       "<span size=\"small\" style=\"italic\">",
						       "</span>",
						       Glib::Markup::escape_text (_route->name()),
						       PROGRAM_NAME));
		}
		dialog.get_vbox()->pack_start (*l);
		dialog.add_button ("OK", RESPONSE_CANCEL);
	}

	dialog.show_all ();
	int const r = dialog.run ();

	if (r == RESPONSE_ACCEPT && spin) {
		_route->set_remote_control_id (spin->get_value_as_int ());
	}
}

void
RouteUI::setup_invert_buttons ()
{
// not used in TracksLive
#if 0
	/* remove old invert buttons */
	for (vector<ArdourButton*>::iterator i = _invert_buttons.begin(); i != _invert_buttons.end(); ++i) {
		//_invert_button_box.remove (**i);
	}

	_invert_buttons.clear ();

	if (!_route || !_route->input()) {
		return;
	}

	uint32_t const N = _route->input()->n_ports().n_audio ();

	uint32_t const to_add = (N <= _max_invert_buttons) ? N : 1;

	for (uint32_t i = 0; i < to_add; ++i) {
		ArdourButton* b = manage (new ArdourButton);
		b->set_size_request(20,20);
		b->signal_button_press_event().connect (sigc::mem_fun (*this, &RouteUI::invert_press));
		b->signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &RouteUI::invert_release), i));

		b->set_name (X_("invert button"));
		if (to_add == 1) {
			if (N > 1) {
				b->set_text (string_compose (X_("Ø (%1)"), N));
			} else {
				b->set_text (X_("Ø"));
			}
		} else {
			b->set_text (string_compose (X_("Ø%1"), i + 1));
		}

		if (N <= _max_invert_buttons) {
			UI::instance()->set_tip (*b, string_compose (_("Left-click to invert (phase reverse) channel %1 of this track.  Right-click to show menu."), i + 1));
		} else {
			UI::instance()->set_tip (*b, _("Click to show a menu of channels for inversion (phase reverse)"));
		}

		_invert_buttons.push_back (b);
		//_invert_button_box.pack_start (*b);
	}

    
	//_invert_button_box.set_spacing (1);
//	_invert_button_box.show_all ();
#endif
}

void
RouteUI::set_invert_button_state ()
{
#if 0 // Not used in TracksLive
	uint32_t const N = _route->input()->n_ports().n_audio();
	if (N > _max_invert_buttons) {

		/* One button for many channels; explicit active if all channels are inverted,
		   implicit active if some are, off if none are.
		*/

		ArdourButton* b = _invert_buttons.front ();
		
		if (_route->phase_invert().count() == _route->phase_invert().size()) {
			b->set_active_state (Gtkmm2ext::ExplicitActive);
		} else if (_route->phase_invert().any()) {
			b->set_active_state (Gtkmm2ext::ImplicitActive);
		} else {
			b->set_active_state (Gtkmm2ext::Off);
		}

	} else {

		/* One button per channel; just set active */

		int j = 0;
		for (vector<ArdourButton*>::iterator i = _invert_buttons.begin(); i != _invert_buttons.end(); ++i, ++j) {
			(*i)->set_active (_route->phase_invert (j));
		}
		
	}
#endif
}

bool
RouteUI::invert_release (GdkEventButton* ev, uint32_t i)
{
#if 0 // Not used in TracksLive
	if (ev->button == 1 && i < _invert_buttons.size()) {
		uint32_t const N = _route->input()->n_ports().n_audio ();
		if (N <= _max_invert_buttons) {
			/* left-click inverts phase so long as we have a button per channel */
			_route->set_phase_invert (i, !_invert_buttons[i]->get_active());
			return true;
		}
	}
#endif
	return false;
}


bool
RouteUI::invert_press (GdkEventButton* ev)
{
#if 0 // Not used in TracksLive
	using namespace Menu_Helpers;

	uint32_t const N = _route->input()->n_ports().n_audio();
	if (N <= _max_invert_buttons && ev->button != 3) {
		/* If we have an invert button per channel, we only pop
		   up a menu on right-click; left click is handled
		   on release.
		*/
		return true;
	}
	
	delete _invert_menu;
	_invert_menu = new Menu;
	_invert_menu->set_name ("ArdourContextMenu");
	MenuList& items = _invert_menu->items ();

	for (uint32_t i = 0; i < N; ++i) {
		items.push_back (CheckMenuElem (string_compose (X_("Ø%1"), i + 1), sigc::bind (sigc::mem_fun (*this, &RouteUI::invert_menu_toggled), i)));
		Gtk::CheckMenuItem* e = dynamic_cast<Gtk::CheckMenuItem*> (&items.back ());
		++_i_am_the_modifier;
		e->set_active (_route->phase_invert (i));
		--_i_am_the_modifier;
	}

	_invert_menu->popup (0, ev->time);
#endif
	return false;
}

void
RouteUI::invert_menu_toggled (uint32_t c)
{
	if (_i_am_the_modifier) {
		return;
	}

	_route->set_phase_invert (c, !_route->phase_invert (c));
}

void
RouteUI::set_invert_sensitive (bool yn)
{
#if 0 // Not used in TracksLive
        for (vector<ArdourButton*>::iterator b = _invert_buttons.begin(); b != _invert_buttons.end(); ++b) {
                (*b)->set_sensitive (yn);
        }
#endif
}

void
RouteUI::request_redraw ()
{
	if (_route) {
		_route->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
	}
}

/** The Route's gui_changed signal has been emitted */
void
RouteUI::route_gui_changed (string what_changed)
{
	if (what_changed == "color") {
		if (set_color_from_route () == 0) {
			route_color_changed ();
		}
	}
}

/** @return the color that this route should use; it maybe its own,
    or it maybe that of its route group.
*/
Gdk::Color
RouteUI::color () const
{
	RouteGroup* g = _route->route_group ();
	
	if (g && g->is_color()) {
		Gdk::Color c;
		set_color_from_rgba (c, GroupTabs::group_color (g));
		return c;
	}

	return _color;
}

void
RouteUI::set_showing_sends_to (boost::shared_ptr<Route> send_to)
{
	_showing_sends_to = send_to;
	BusSendDisplayChanged (send_to); /* EMIT SIGNAL */
}

void
RouteUI::bus_send_display_changed (boost::shared_ptr<Route> send_to)
{
	if (_route == send_to) {
		show_sends_button.set_active (true);
		send_blink_connection = ARDOUR_UI::instance()->Blink.connect (sigc::mem_fun (*this, &RouteUI::send_blink));
	} else {
		show_sends_button.set_active (false);
		send_blink_connection.disconnect ();
	}
}

RouteGroup*
RouteUI::route_group() const
{
	return _route->route_group();
}
