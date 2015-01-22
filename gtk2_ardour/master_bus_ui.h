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

#ifndef __tracks_master_bus_ui_h__
#define __tracks_master_bus_ui_h__

#include <list>
#include <set>

#include <gtkmm/table.h>
#include <gtkmm/button.h>
#include <gtkmm/box.h>
#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/radiomenuitem.h>
#include <gtkmm/checkmenuitem.h>
#include <gtkmm/adjustment.h>

#include <gtkmm2ext/selector.h>
#include <gtkmm2ext/slider_controller.h>

#include "ardour/playlist.h"
#include "ardour/types.h"
#include "ardour/route_group.h"

#include "route_ui.h"
#include "enums.h"
#include "gain_meter.h"

class MasterBusUI : public Gtk::EventBox, public WavesUI
{
public:
 	MasterBusUI (ARDOUR::Session*, PublicEditor&);
    void init(ARDOUR::Session* session);
 	virtual ~MasterBusUI ();

    void fast_update ();
	void set_route (boost::shared_ptr<ARDOUR::Route>);
    void update_master_bus_selection();
	static PBD::Signal1<void,MasterBusUI*> CatchDeletion;
    
    void master_bus_set_visible (bool set_visible);

private:
	static int __meter_width;
	float _max_peak;
	float _peak_treshold;

 	void reset ();
	void meter_configuration_changed (ARDOUR::ChanCount);
	void reset_peak_display ();
	void reset_route_peak_display (ARDOUR::Route* route);
	void reset_group_peak_display (ARDOUR::RouteGroup* group);
	void on_peak_display_button (WavesButton*);
	void on_master_mute_button (WavesButton*);
    bool on_master_mute_button_enter (GdkEventCrossing*);
    bool on_master_mute_button_leave (GdkEventCrossing*);
    void on_clear_solo_button (WavesButton*);
	void on_global_rec_button (WavesButton*);
    void on_output_connection_mode_changed ();
    bool on_level_meter_button_press (GdkEventButton*);
    bool on_master_event_box_button_press (GdkEventButton*);
    
    PBD::ScopedConnection _route_meter_connection;
    
    // MASTER staff
    void connect_route_state_signals(ARDOUR::RouteList& tracks);
    void update_master();
    
    // GLOBAL RECORD staff
    PBD::ScopedConnectionList _route_state_connections;
    PBD::ScopedConnectionList _session_connections;
    
    void record_state_changed ();
    bool check_all_tracks_are_record_armed ();
    
    // MASTER MUTE staff
    void route_mute_state_changed (void* );
    bool check_all_tracks_are_muted();
    
    // CLEAR SOLO staff
    void solo_blink (bool onoff);
    bool exists_soloed_track();
        
	boost::shared_ptr<ARDOUR::Route> _route;
    PBD::ScopedConnection _mode_connection;
    PBD::ScopedConnection _output_mode_connection;

	Gtk::Box& _level_meter_home;
	LevelMeterHBox _level_meter;
	WavesButton& _peak_display_button;
	WavesButton& _master_mute_button;
	WavesButton& _clear_solo_button;
	WavesButton& _global_rec_button;
    Gtk::EventBox& _no_peak_display_box;
    Gtk::HBox& _master_bus_hbox;
    Gtk::HBox& _master_bus_empty_hbox;
    Gtk::Image& _master_bus_multi_out_mode_icon;
    Gtk::Container& _master_event_box;
    
    PublicEditor& _editor;
    
    bool _selected;
    bool _ignore_mute_update;
    bool _ignore_selection_click;
};

#endif /* __tracks_master_bus_ui_h__ */

