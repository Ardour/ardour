/*
	Copyright (C) 2014 Waves Audio Ltd.

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

#include <cstdlib>
#include <cmath>
#include <cassert>

#include <algorithm>
#include <string>
#include <vector>
#include <map>
#include <utility>

#include <sigc++/bind.h>

#include "pbd/error.h"
#include "pbd/stl_delete.h"
#include "pbd/whitespace.h"
#include "pbd/memento_command.h"
#include "pbd/enumwriter.h"
#include "pbd/unwind.h"
#include "pbd/stateful_diff_command.h"

#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>
#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/selector.h>
#include <gtkmm2ext/bindable_button.h>
#include <gtkmm2ext/utils.h>

#include "meter_patterns.h"
#include "ardour/amp.h"
#include "ardour/meter.h"
#include "ardour/event_type_map.h"
#include "ardour/processor.h"
#include "ardour/profile.h"
#include "ardour/route_group.h"
#include "ardour/session.h"
#include "ardour/session_playlists.h"
#include "ardour/audio_track.h"
#include "ardour/engine_state_controller.h"

#include "evoral/Parameter.hpp"

#include "canvas/debug.h"

#include "ardour_ui.h"
#include "ardour_button.h"
#include "debug.h"
#include "global_signals.h"
#include "master_bus_ui.h"
#include "enums.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "playlist_selector.h"
#include "point_selection.h"
#include "prompter.h"
#include "public_editor.h"
#include "region_view.h"
#include "rgb_macros.h"
#include "selection.h"
#include "streamview.h"
#include "utils.h"
#include "route_group_menu.h"

#include "ardour/track.h"

#include "i18n.h"
#include "dbg_msg.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace Editing;
using namespace std;
using std::list;
using namespace ArdourMeter;

int MasterBusUI::__meter_width = 3;
PBD::Signal1<void,MasterBusUI*> MasterBusUI::CatchDeletion;

MasterBusUI::MasterBusUI (Session* sess, PublicEditor& ed)
	: WavesUI ("master_ui.xml", *this)
	, _max_peak (minus_infinity())
	, _peak_treshold (xml_property(*xml_tree()->root(), "peaktreshold", -144.4)) // Think about having it in config
	, _level_meter_home (get_box ("level_meter_home"))
	, _level_meter (sess)
	, _peak_display_button (get_waves_button ("peak_display_button"))
	, _master_mute_button (get_waves_button ("master_mute_button"))
	, _clear_solo_button (get_waves_button ("clear_solo_button"))
	, _global_rec_button (get_waves_button ("global_rec_button"))
    , _no_peak_display_box (get_event_box("no_peak_display_box") )
    , _master_bus_hbox (get_h_box("master_bus_hbox") )
    , _master_bus_empty_hbox (get_h_box("master_bus_empty_hbox"))
    , _master_bus_multi_out_mode_icon (get_image("master_bus_multi_out_mode_icon"))
    , _master_event_box (WavesUI::root () )
    , _selected(false)
    , _ignore_mute_update(false)
    , _ignore_selection_click(false)
    , _editor(ed)
{
	set_attributes (*this, *xml_tree ()->root (), XMLNodeMap ());
	_level_meter_home.pack_start (_level_meter, true, true);
	_peak_display_button.unset_flags (Gtk::CAN_FOCUS);
	_master_mute_button.unset_flags (Gtk::CAN_FOCUS);
	_clear_solo_button.unset_flags (Gtk::CAN_FOCUS);
	_global_rec_button.unset_flags (Gtk::CAN_FOCUS);
    _master_event_box.set_flags (CAN_FOCUS);
    
    _master_event_box.add_events (Gdk::BUTTON_RELEASE_MASK);
	   
	ResetAllPeakDisplays.connect (sigc::mem_fun(*this, &MasterBusUI::reset_peak_display));
	ResetRoutePeakDisplays.connect (sigc::mem_fun(*this, &MasterBusUI::reset_route_peak_display));
	ResetGroupPeakDisplays.connect (sigc::mem_fun(*this, &MasterBusUI::reset_group_peak_display));

	_peak_display_button.signal_clicked.connect (sigc::mem_fun (*this, &MasterBusUI::on_peak_display_button));
	_master_mute_button.signal_clicked.connect (sigc::mem_fun (*this, &MasterBusUI::on_master_mute_button));
    _master_mute_button.signal_enter_notify_event().connect (sigc::mem_fun (*this, &MasterBusUI::on_master_mute_button_enter));
    _master_mute_button.signal_leave_notify_event().connect (sigc::mem_fun (*this, &MasterBusUI::on_master_mute_button_leave));
	_clear_solo_button.signal_clicked.connect (sigc::mem_fun (*this, &MasterBusUI::on_clear_solo_button));
	_global_rec_button.signal_clicked.connect (sigc::mem_fun (*this, &MasterBusUI::on_global_rec_button));
    _master_event_box.signal_button_press_event().connect (sigc::mem_fun (*this, &MasterBusUI::on_master_event_box_button_press));
    
    _editor.get_selection().TracksChanged.connect (sigc::mem_fun(*this, &MasterBusUI::update_master_bus_selection));
    
    EngineStateController::instance()->OutputConnectionModeChanged.connect (_mode_connection,
                                                                            MISSING_INVALIDATOR,
                                                                            boost::bind (&MasterBusUI::update_master_bus_selection, this),
                                                                            gui_context ());
    EngineStateController::instance()->OutputConfigChanged.connect (_output_mode_connection,
                                                                    MISSING_INVALIDATOR,
                                                                    boost::bind (&MasterBusUI::
                                                                        on_output_connection_mode_changed, this),
                                                                    gui_context());
    
    init(sess);
}

void MasterBusUI::init(ARDOUR::Session *session)
{    
    // if new tracks is added, they must effect on Global Record button and Master Mute button
    session->RouteAdded.connect (_session_connections, invalidator (*this), boost::bind (&MasterBusUI::connect_route_state_signals, this, _1), gui_context());
    session->RouteRemovedFromRouteGroup.connect (_session_connections, invalidator (*this), boost::bind (&MasterBusUI::update_master, this), gui_context());
    
    set_route (session->master_out ());
    
    // connect existing tracks to MASTER
    connect_route_state_signals( *(session->get_tracks().get()) );
    
    if (!_level_meter.get_parent () ) {
        _level_meter_home.pack_start (_level_meter);
    }

    on_output_connection_mode_changed();
    update_master();
    
    //ARDOUR_UI::Blink.connect (sigc::mem_fun (*this, &MasterBusUI::solo_blink));
    
    _level_meter.set_session(session);
}

void MasterBusUI::on_output_connection_mode_changed()
{
    /*
    if (Config->get_output_auto_connect() & AutoConnectPhysical) {
        if (_peak_display_button.get_parent ()) {
            get_box ("peak_display_button_home").remove (_peak_display_button);
        }
        
        if (_level_meter.get_parent ()) {
            _level_meter_home.remove (_level_meter);
        }
        
        if( !_no_peak_display_box.get_parent ()) {
            get_box ("peak_display_button_home").pack_start (_no_peak_display_box);
        }
        
        if (!_master_bus_multi_out_mode_icon.get_parent ()) {
            get_box ("the_icon_home").pack_start (_master_bus_multi_out_mode_icon);
        }
    } else if (Config->get_output_auto_connect() & AutoConnectMaster) {
        if (_no_peak_display_box.get_parent ()) {
            get_box ("peak_display_button_home").remove (_no_peak_display_box);
        }
        
        if (_master_bus_multi_out_mode_icon.get_parent ()) {
            get_box ("the_icon_home").remove (_master_bus_multi_out_mode_icon);
        }
        
        if (!_peak_display_button.get_parent ()) {
            get_box ("peak_display_button_home").pack_start (_peak_display_button);
        }
        
        if (!_level_meter.get_parent ()) {
            _level_meter_home.pack_start (_level_meter);
        }
    } */

    // update MASTER MUTE
    route_mute_state_changed(NULL);
}

void
MasterBusUI::master_bus_set_visible (bool set_visible)
{
    if ( set_visible )
    {
        _master_bus_hbox.show ();
        _master_bus_empty_hbox.hide ();
    } else
    {
        _master_bus_hbox.hide ();
        _master_bus_empty_hbox.show ();
    }
}

void
MasterBusUI::update_master_bus_selection ()
{
    TimeAxisView* tv = _editor.axis_view_from_route (_route );
    
    if (tv && _editor.get_selection().selected(tv) ) {
        _selected = true;
    } else {
        _selected = false;
    }
    
    
    if (_selected) {
        _master_event_box.set_state (Gtk::STATE_ACTIVE);
    } else {
        _master_event_box.set_state (Gtk::STATE_NORMAL);
    }
}

bool
MasterBusUI::on_master_event_box_button_press (GdkEventButton *ev)
{
    if (_ignore_selection_click) {
        return true;
    }
    
    if (ev->button == 1) {
        
        if (Keyboard::modifier_state_equals (ev->state, (Keyboard::TertiaryModifier|Keyboard::PrimaryModifier))) {
            
            TimeAxisView* tv = _editor.axis_view_from_route (_route );
            if (tv) {
                /* special case: select/deselect all tracks along with master bus*/
                if (_editor.get_selection().selected (tv)) {
                    _editor.get_selection().clear_tracks ();
                } else {
                    _editor.select_all_tracks ();
                }
            }
            return true;
        }
        
        switch (ArdourKeyboard::selection_type (ev->state)) {
            case Selection::Toggle:
            {
                TimeAxisView* tv = _editor.axis_view_from_route (_route );
                if (tv) {
                    _editor.get_selection().toggle (tv);
                }
                _selected = false;
            }
            break;
                
            case Selection::Set:
            {
                TimeAxisView* tv = _editor.axis_view_from_route (_route );
                if (tv) {
                    _editor.set_selected_track(*tv);
                }
                _selected = true;
            }
            break;
                
            case Selection::Extend:
            {
                TimeAxisView* tv = _editor.axis_view_from_route (_route );
                if (tv) {
                     _editor.extend_selection_to_track (*tv);
                }
                _selected = true;
            }
            break;
                
            case Selection::Add:
            {
                    TimeAxisView* tv = _editor.axis_view_from_route (_route );
                    if (tv) {
                        _editor.get_selection().add (tv);
                    }
                    _selected = true;
            }
            break;
        }
    }
    
    update_master_bus_selection();
    return true;
}

MasterBusUI::~MasterBusUI ()
{
	CatchDeletion (this);
}

void
MasterBusUI::set_route (boost::shared_ptr<Route> rt)
{
	reset ();
	_route = rt;
	_level_meter.set_meter (_route->shared_peak_meter().get());
	_level_meter.clear_meters();
	_level_meter.set_type (_route->meter_type());
	_level_meter.setup_meters (__meter_width, __meter_width);
	_route->shared_peak_meter()->ConfigurationChanged.connect (_route_meter_connection,
		                                                       invalidator (*this),
															   boost::bind (&MasterBusUI::meter_configuration_changed, 
															                this,
																			_1), 
															   gui_context());
	_route->DropReferences.connect (_route_meter_connection,
									invalidator (*this),
									boost::bind (&MasterBusUI::reset,
												 this),
									gui_context());
}

void
MasterBusUI::reset ()
{
	_route_meter_connection.disconnect ();
	_route = boost::shared_ptr<ARDOUR::Route>(); // It's to have it "false"
}

void
MasterBusUI::fast_update ()
{
	if (_route) {
		Gtk::Requisition sz;
		size_request (sz);
		if (sz.height == 0) {
			return;
		}
		float mpeak = _level_meter.update_meters();
		if (mpeak > _max_peak) {
			_max_peak = mpeak;
			if (mpeak >= Config->get_meter_peak()) {
				_peak_display_button.set_active_state (Gtkmm2ext::ExplicitActive);
			}
			char buf[32];
			if (mpeak <= _peak_treshold) {
				_peak_display_button.set_text ("- inf");
			} else {
				snprintf (buf, sizeof(buf), "%.1f", mpeak);
				_peak_display_button.set_text (buf);
			}
		}
 	}
}

void
MasterBusUI::meter_configuration_changed (ChanCount c)
{
	_level_meter.setup_meters (__meter_width, __meter_width);
}

void
MasterBusUI::reset_peak_display ()
{
	_level_meter.clear_meters();
	_max_peak = -INFINITY;
	_peak_display_button.set_text (_("- inf"));
	_peak_display_button.set_active_state(Gtkmm2ext::Off);
}

void
MasterBusUI::reset_route_peak_display (Route* route)
{
	if (_route && _route.get() == route) {
		reset_peak_display ();
	}
}

void
MasterBusUI::reset_group_peak_display (RouteGroup* group)
{
	if (_route && group == _route->route_group()) {
		reset_peak_display ();
	}
}

void 
MasterBusUI::on_peak_display_button (WavesButton*)
{
	if (_route) {
		ResetRoutePeakDisplays (_route.get());
	}
}

// MASTER staff
void MasterBusUI::connect_route_state_signals(RouteList& tracks)
{
    for (RouteList::iterator i = tracks.begin(); i != tracks.end(); ++i)
    {
        boost::shared_ptr<Track> t;
        
        if ((t = boost::dynamic_pointer_cast<Track>(*i)) != 0) {
            t->RecordEnableChanged.connect (_route_state_connections,
                                            invalidator (*this),
                                            boost::bind (&MasterBusUI::record_state_changed, this),
                                            gui_context() );
		}
        
        (*i)->mute_changed.connect (_route_state_connections,
                                    invalidator (*this),
                                    boost::bind (&MasterBusUI::route_mute_state_changed, this, _1),
                                    gui_context() );
        
        (*i)->DropReferences.connect(_route_state_connections,
                                     invalidator (*this),
                                     boost::bind (&MasterBusUI::update_master, this),
                                     gui_context() );
    }
    
    Route* master = ARDOUR_UI::instance()->the_session()->master_out().get();
    master->mute_changed.connect (_route_state_connections,
                                  invalidator (*this),
                                  boost::bind (&MasterBusUI::route_mute_state_changed, this, _1),
                                  gui_context() );
    
    update_master();
}

void MasterBusUI::update_master()
{
    record_state_changed();
    route_mute_state_changed(0);
}

// Master Mute Staff mute_changed
bool MasterBusUI::check_all_tracks_are_muted()
{
    Session* session = ARDOUR_UI::instance()->the_session();
    
    if( !session )
        return false;
    
    boost::shared_ptr<RouteList> tracks = session->get_tracks();
    
    if(tracks->size() == 0)
        return false;
    
    bool all_tracks_are_muted = true;
    for (RouteList::iterator i = tracks->begin(); i != tracks->end(); ++i)
    {
        if ( !(*i)->muted () )
        {
            all_tracks_are_muted = false;
            break;
        }
    }
    
    return all_tracks_are_muted;
}

void MasterBusUI::on_master_mute_button (WavesButton*)
{
    Session* session = ARDOUR_UI::instance()->the_session();
    
    if( !session )
        return;
    
    PBD::Unwinder<bool> uw (_ignore_mute_update, true);
    
    if (Config->get_output_auto_connect() & AutoConnectPhysical) // Multi out
    {
        boost::shared_ptr<RouteList> tracks = session->get_tracks();
        bool all_tracks_are_muted = this->check_all_tracks_are_muted();
        session->set_mute(tracks, !all_tracks_are_muted);
        _master_mute_button.set_active( !all_tracks_are_muted );
    } else if (Config->get_output_auto_connect() & AutoConnectMaster) // Stereo out
    {
        boost::shared_ptr<Route> master = session->master_out();
        master->set_mute(!master->muted(), session);
        _master_mute_button.set_active(master->muted());
    }
}

bool
MasterBusUI::on_master_mute_button_enter (GdkEventCrossing*)
{
    _ignore_selection_click = true;
    return true;
}

bool
MasterBusUI::on_master_mute_button_leave (GdkEventCrossing*)
{
    _ignore_selection_click = false;
    return true;
}

void MasterBusUI::route_mute_state_changed (void* )
{
    Session* session = ARDOUR_UI::instance()->the_session();
    
    if( !session )
        return;
    
    if( _ignore_mute_update )
        return;

    if (Config->get_output_auto_connect() & AutoConnectPhysical) // Multi out
    {
        _master_mute_button.set_active( check_all_tracks_are_muted() );
    } else if (Config->get_output_auto_connect() & AutoConnectMaster) // Stereo out
{
        boost::shared_ptr<Route> master = session->master_out();
        _master_mute_button.set_active(master->muted());
    }
}

bool MasterBusUI::exists_soloed_track()
{
    bool exists_soled_track = false;
    
    Session* session = ARDOUR_UI::instance()->the_session();
    if( !session )
        return false;
    
    boost::shared_ptr<RouteList> tracks = session->get_tracks ();
    
    if(tracks->size() == 0)
        return false;
    
    for (RouteList::iterator i = tracks->begin(); i != tracks->end(); ++i)
    {
        if ( (*i)->soloed() )
        {
            exists_soled_track = true;
            break;
        }
    }
    
    return exists_soled_track;
}

void MasterBusUI::solo_blink (bool onoff)
{
    if ( exists_soloed_track() )
       _clear_solo_button.set_active( onoff );
    else
        _clear_solo_button.set_active( false );
}

void MasterBusUI::on_clear_solo_button (WavesButton*)
{
    Session* session = ARDOUR_UI::instance()->the_session();
    
    if( !session )
        return;
    
    boost::shared_ptr<RouteList> rl = session->get_tracks ();
    session->set_solo(rl, false); // set all tracks not soled
}

// Global record staff
bool MasterBusUI::check_all_tracks_are_record_armed ()
{
    Session* session = ARDOUR_UI::instance()->the_session();
    
    if( !session )
        return false;
    
    boost::shared_ptr<RouteList> tracks = session->get_tracks ();
    
    if(tracks->size() == 0)
        return false;
    
    bool all_tracks_are_record_armed = true;
    for (RouteList::iterator i = tracks->begin(); i != tracks->end(); ++i)
    {
        if ( !(*i)->record_enabled() )
        {
            all_tracks_are_record_armed = false;
            break;
        }
    }
    
    return all_tracks_are_record_armed;
}

void MasterBusUI::record_state_changed ()
{
    _global_rec_button.set_active (check_all_tracks_are_record_armed());
}

void MasterBusUI::on_global_rec_button (WavesButton*)
{
    Session* session = ARDOUR_UI::instance()->the_session();
    
    if( !session )
        return;
    
    boost::shared_ptr<RouteList> rl = session->get_tracks ();
    
    bool all_tracks_are_record_armed = this->check_all_tracks_are_record_armed();    
    session->set_record_enabled (rl, !all_tracks_are_record_armed);
    _global_rec_button.set_active(!all_tracks_are_record_armed);
}
