/*
    Copyright (C) 2000-2006 Paul Davis

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

#include "ardour/port.h"

#include "gui_thread.h"
#include "ardour_ui.h"
#include "public_editor.h"
#include "route_inspector.h"

#include "i18n.h"
#include "dbg_msg.h"

using namespace ARDOUR;

RouteInspector::RouteInspector (Session* sess, const std::string& layout_script_file, size_t max_name_size)
	: AxisView (sess)
	, MixerStrip(sess, layout_script_file, max_name_size)
	, color_palette_button (get_waves_button ("color_palette_button"))
	, color_palette_home (get_container ("color_palette_home"))
	, color_palette_button_home (get_container ("color_palette_button_home"))
	, color_buttons_home (get_container ("color_buttons_home"))
    , info_panel_button (get_waves_button ("info_panel_button"))
	, info_panel_home (get_widget ("info_panel_home"))
	, input_info_label (get_label ("input_info_label"))
	, output_info_label (get_label ("output_info_label"))
{
	init ();
}
   
RouteInspector::RouteInspector (Session* sess, boost::shared_ptr<Route> rt, const std::string& layout_script_file, size_t max_name_size)
	: AxisView (sess)
	, MixerStrip(sess, rt, layout_script_file, max_name_size)
	, color_palette_button (get_waves_button ("color_palette_button"))
	, color_palette_home (get_container ("color_palette_home"))
	, color_palette_button_home (get_container ("color_palette_button_home"))
	, color_buttons_home (get_container ("color_buttons_home"))
    , info_panel_button (get_waves_button ("info_panel_button"))
	, info_panel_home (get_widget ("info_panel_home"))
	, input_info_label (get_label ("input_info_label"))
	, output_info_label (get_label ("output_info_label"))
{
	init ();
}

void
RouteInspector::init ()
{
	color_button[0] = &get_waves_button ("color_button_1");
	color_button[1] = &get_waves_button ("color_button_2");
	color_button[2] = &get_waves_button ("color_button_3");
	color_button[3] = &get_waves_button ("color_button_4");
	color_button[4] = &get_waves_button ("color_button_5");
	color_button[5] = &get_waves_button ("color_button_6");
	color_button[6] = &get_waves_button ("color_button_7");
	color_button[7] = &get_waves_button ("color_button_8");
	color_button[8] = &get_waves_button ("color_button_9");
	color_button[9] = &get_waves_button ("color_button_10");
	color_button[10] = &get_waves_button ("color_button_11");
	color_button[11] = &get_waves_button ("color_button_12");
	color_button[12] = &get_waves_button ("color_button_13");
	color_button[13] = &get_waves_button ("color_button_14");
	color_button[14] = &get_waves_button ("color_button_15");

	color_palette_button.signal_clicked.connect (sigc::mem_fun (*this, &RouteInspector::color_palette_button_clicked));
	info_panel_button.signal_clicked.connect (sigc::mem_fun (*this, &RouteInspector::info_panel_button_clicked));

	for (size_t i=0; i<(sizeof(color_button)/sizeof(color_button[0])); i++) {
		color_button[i]->signal_clicked.connect (sigc::mem_fun (*this, &RouteInspector::color_button_clicked));
	}

    _session->session_routes_reconnected.connect (_session_connections, invalidator (*this), boost::bind (&RouteInspector::update_inspector_info_panel, this), gui_context());
    EngineStateController::instance()->EngineRunning.connect(_input_output_channels_update, invalidator (*this), boost::bind (&RouteInspector::update_inspector_info_panel, this), gui_context());
}

RouteInspector::~RouteInspector ()
{
}


void
RouteInspector::update_inspector_info_panel ()
{
    if (!route ()) {
        return;
	}
    // Input label
    std::string input_text;
    
    AutoConnectOption auto_connection_options;
    auto_connection_options = Config->get_output_auto_connect();
    
	PortSet& in_ports (_route->input()->ports() );
    
    std::string track_name = _route->name();
    
    for (PortSet::iterator i = in_ports.begin(); i != in_ports.end(); ++i)
    {        
        std::vector<std::string> connections_string;
        i->get_connections(connections_string);

        for(unsigned int j = 0; j < connections_string.size(); ++j)
        {
            // delete "/audio_out 1"
            remove_pattern_from_string(connections_string[j], "/audio_out 1", connections_string[j]);
           
            // Do not show the same inputs. For example Input for Master Bus can be "Track 1/audio_out 1", "Track 1/audio_out 2". We leave only one "Track 1".
            int pos = connections_string[j].find("/audio_out 2");
            if( pos != std::string::npos )
                continue;
            
            remove_pattern_from_string(connections_string[j], "system:capture:", connections_string[j]);
            remove_pattern_from_string(connections_string[j], "ardour:", connections_string[j]);
                        
            input_text += "\n" + connections_string[j];
        }
    }
    
    input_text = "In" + input_text;
    input_info_label.set_text (input_text);
    input_info_label.set_tooltip_text (input_text);
    
    // Output label
    std::string output_text = "";
    PortSet& out_ports (_route->output()->ports() );
    
    if( !_route->is_master () )
    {
        // if multi out mode
        if( auto_connection_options == AutoConnectPhysical )
        {
            for (PortSet::iterator i = out_ports.begin(); i != out_ports.end(); ++i)
            {
                std::vector<std::string> connections_string;
                i->get_connections(connections_string);
                
                for(unsigned int j = 0; j < connections_string.size(); ++j)
                {
                    if( connections_string[j].find("system:playback:") != std::string::npos )
                        connections_string[j].erase(0, 16);
                    
                    output_text += "\n" + connections_string[j];
                }
            }
        } else { // stereo out mode
            output_text = "\nMaster Bus";
        }
        
    } else {
        
        for (PortSet::iterator i = out_ports.begin(); i != out_ports.end(); ++i)
        {
            std::vector<std::string> connections_string;
            i->get_connections(connections_string);
            
            for(unsigned int j = 0; j < connections_string.size(); ++j)
            {
                if( connections_string[j].find("system:playback:") != std::string::npos )
                    connections_string[j].erase(0, 16);
                
                output_text += "\n" + connections_string[j];
            }
        }

    }
    
    output_text = "Out" + output_text;
    output_info_label.set_text(output_text);
    output_info_label.set_tooltip_text(output_text);
}

void
RouteInspector::set_route (boost::shared_ptr<Route> rt)
{
	MixerStrip::set_route (rt);
	if (route ()) {
		color_palette_home.set_visible (!route()->is_master());
	}
	update_inspector_info_panel ();
}

void
RouteInspector::route_color_changed ()
{
	MixerStrip::route_color_changed ();
	Gdk::Color new_color = color();
	for (size_t i=0; i<(sizeof(color_button)/sizeof(color_button[0])); i++) {
		color_button[i]->set_active (new_color == Gdk::Color (XMLColor[i]));
	}
	
	color_palette_button_home.modify_bg (Gtk::STATE_NORMAL, new_color);
	color_palette_button_home.modify_bg (Gtk::STATE_ACTIVE, new_color);
	color_palette_button_home.queue_draw ();
}

void
RouteInspector::color_palette_button_clicked (WavesButton *button)
{
	color_buttons_home.set_visible (!color_buttons_home.is_visible ());
	color_palette_button.set_active (color_buttons_home.is_visible ());
}

void
RouteInspector::color_button_clicked (WavesButton *button)
{
	button->set_active (true);
	for (size_t i=0; i<(sizeof(color_button)/sizeof(color_button[0])); i++) {
		if (button != color_button[i]) {
			color_button[i]->set_active (false);
		} else {
			TrackSelection& track_selection =  ARDOUR_UI::instance()->the_editor().get_selection().tracks;
            track_selection.foreach_route_ui (boost::bind (&RouteUI::set_color, _1, Gdk::Color (XMLColor[i])));
		}
	}
}

void
RouteInspector::info_panel_button_clicked (WavesButton *button)
{
	info_panel_home.set_visible (!info_panel_home.is_visible ());
	info_panel_button.set_active (info_panel_home.is_visible ());
}
