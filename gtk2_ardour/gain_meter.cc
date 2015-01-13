/*
  Copyright (C) 2002 Paul Davis

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

#include <limits.h>

#include "ardour/amp.h"
#include "ardour/route_group.h"
#include "ardour/session_route.h"
#include "ardour/dB.h"
#include "ardour/utils.h"

#include <pangomm.h>
#include <gtkmm/style.h>
#include <gdkmm/color.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/fastmeter.h>
#include <gtkmm2ext/barcontroller.h>
#include <gtkmm2ext/gtk_ui.h>
#include "pbd/fastlog.h"
#include "pbd/stacktrace.h"

#include "ardour_ui.h"
#include "gain_meter.h"
#include "global_signals.h"
#include "logmeter.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "public_editor.h"
#include "utils.h"
#include "meter_patterns.h"
#include "selection.h"

#include "ardour/session.h"
#include "ardour/route.h"
#include "ardour/meter.h"
#include "ardour/audio_track.h"
#include "ardour/midi_track.h"

#include "i18n.h"
#include "dbg_msg.h"

using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace std;
using Gtkmm2ext::Keyboard;
using namespace ArdourMeter;

GainMeter::GainMeter (Session* s, const std::string& layout_script_file)
	: Gtk::VBox ()
	, WavesUI (layout_script_file, *this)
    , gain_slider (get_fader ("gain_slider"))
	, gain_adjustment (get_adjustment ("gain_adjustment"))
	, gain_display_entry (get_entry ("gain_display_entry"))
	, gain_display_button (get_waves_button ("gain_display_button"))
	, peak_display_button (get_waves_button ("peak_display_button"))
	, level_meter_home (get_box ("level_meter_home"))
	, level_meter (_session)
    , _data_type (DataType::AUDIO)
	, _meter_width (xml_property (*xml_tree ()->root (), "meterwidth", 5))
	, _thin_meter_width (xml_property (*xml_tree ()->root (), "thinmeterwidth", 5))
    , _gain_slider_double_clicked (false)
{
	using namespace Menu_Helpers;
	//gain_adjustment (gain_to_slider_position_with_max (1.0, Config->get_max_gain()), 0.0, 1.0, 0.01, 0.1)
	gain_adjustment.set_value (gain_to_slider_position_with_max (1.0, Config->get_max_gain()));
	gain_adjustment.set_lower (0.0);
	gain_adjustment.set_upper (1.0);
	gain_adjustment.set_step_increment (0.01);
	gain_adjustment.set_page_increment (0.1);
	set_session (s);

	ignore_toggle = false;
	meter_menu = 0;
	next_release_selects = false;
    affected_by_selection = false;
    
	level_meter_home.pack_start(level_meter);
	level_meter.ButtonPress.connect_same_thread (_level_meter_connection, boost::bind (&GainMeter::level_meter_button_press, this, _1));

	gain_slider.signal_button_press_event().connect (sigc::mem_fun(*this, &GainMeter::gain_slider_button_press), false);
	gain_slider.signal_button_release_event().connect (sigc::mem_fun(*this, &GainMeter::gain_slider_button_release), true);

	gain_display_entry.signal_activate().connect (sigc::mem_fun (*this, &GainMeter::gain_activated));
	gain_display_entry.signal_focus_in_event().connect (sigc::mem_fun (*this, &GainMeter::gain_focus_in), false);
	gain_display_entry.signal_focus_out_event().connect (sigc::mem_fun (*this, &GainMeter::gain_focus_out), false);
	gain_display_entry.signal_key_press_event ().connect (sigc::mem_fun(*this, &GainMeter::gain_key_press), false);
    gain_display_entry.signal_button_press_event ().connect (sigc::mem_fun(*this, &GainMeter::gain_display_entry_press), false);
	
    gain_display_button.unset_flags (Gtk::CAN_FOCUS);
    gain_display_button.signal_button_press_event ().connect (sigc::mem_fun(*this, &GainMeter::gain_display_button_press), false);

	max_peak = minus_infinity();
	peak_display_button.set_text (_("-inf"));
	peak_display_button.unset_flags (Gtk::CAN_FOCUS);

	gain_astyle_menu.items().push_back (MenuElem (_("Trim")));
	gain_astyle_menu.items().push_back (MenuElem (_("Abs")));

	gain_astate_menu.set_name ("ArdourContextMenu");
	gain_astyle_menu.set_name ("ArdourContextMenu");

	gain_adjustment.signal_value_changed ().connect (sigc::mem_fun(*this, &GainMeter::gain_adjusted));    
	peak_display_button.signal_button_release_event ().connect (sigc::mem_fun(*this, &GainMeter::peak_button_release), false);

	ResetAllPeakDisplays.connect (sigc::mem_fun(*this, &GainMeter::reset_peak_display));
	ResetRoutePeakDisplays.connect (sigc::mem_fun(*this, &GainMeter::reset_route_peak_display));
	ResetGroupPeakDisplays.connect (sigc::mem_fun(*this, &GainMeter::reset_group_peak_display));
	RedrawMetrics.connect (sigc::mem_fun(*this, &GainMeter::redraw_metrics));

	UI::instance()->theme_changed.connect (sigc::mem_fun(*this, &GainMeter::on_theme_changed));
	ColorsChanged.connect (sigc::bind(sigc::mem_fun (*this, &GainMeter::color_handler), false));
	DPIReset.connect (sigc::bind(sigc::mem_fun (*this, &GainMeter::color_handler), true));
}

GainMeter::~GainMeter ()
{
	delete meter_menu;
}

void
GainMeter::set_controls (boost::shared_ptr<Route> r,
			     boost::shared_ptr<PeakMeter> pm,
			     boost::shared_ptr<Amp> amp)
{
 	connections.clear ();
	model_connections.drop_connections ();

	if (!pm && !amp) {
		level_meter.set_meter (0);
		gain_slider.set_controllable (boost::shared_ptr<PBD::Controllable>());
		_meter.reset ();
		_amp.reset ();
		_route.reset ();
		return;
	}

	_meter = pm;
	_amp = amp;
	_route = r;

 	level_meter.set_meter (pm.get());
	gain_slider.set_controllable (amp->gain_control());

	if (amp) {
		amp->ConfigurationChanged.connect (
			model_connections, invalidator (*this), boost::bind (&GainMeter::setup_gain_adjustment, this), gui_context ()
			);
	}

	setup_gain_adjustment ();

	if (!_route || !_route->is_auditioner()) {

		using namespace Menu_Helpers;

		gain_astate_menu.items().clear ();

		gain_astate_menu.items().push_back (MenuElem (S_("Automation|OFF"),
							      sigc::bind (sigc::mem_fun (*(amp.get()), &Automatable::set_parameter_automation_state),
									  Evoral::Parameter(GainAutomation), (AutoState) ARDOUR::Off)));
		gain_astate_menu.items().push_back (MenuElem (_("READ"),
							      sigc::bind (sigc::mem_fun (*(amp.get()), &Automatable::set_parameter_automation_state),
								    Evoral::Parameter(GainAutomation), (AutoState) Play)));
		gain_astate_menu.items().push_back (MenuElem (_("WRITE"),
							      sigc::bind (sigc::mem_fun (*(amp.get()), &Automatable::set_parameter_automation_state),
								    Evoral::Parameter(GainAutomation), (AutoState) Write)));
		gain_astate_menu.items().push_back (MenuElem (_("TOUCH"),
							      sigc::bind (sigc::mem_fun (*(amp.get()), &Automatable::set_parameter_automation_state),
								    Evoral::Parameter(GainAutomation), (AutoState) Touch)));


		boost::shared_ptr<AutomationControl> gc = amp->gain_control();
	}

	amp->gain_control()->Changed.connect (model_connections, invalidator (*this), boost::bind (&GainMeter::gain_changed, this), gui_context());

	gain_changed ();
	show_gain ();
	update_gain_sensitive ();
//

	if (_meter) {
		_meter->ConfigurationChanged.connect (
			model_connections, invalidator (*this), boost::bind (&GainMeter::meter_configuration_changed, this, _1), gui_context()
			);
		_meter->TypeChanged.connect (
			model_connections, invalidator (*this), boost::bind (&GainMeter::meter_type_changed, this, _1), gui_context()
			);

		meter_configuration_changed (_meter->input_streams ());
	}


	if (_route) {
		_route->active_changed.connect (model_connections, invalidator (*this), boost::bind (&GainMeter::route_active_changed, this), gui_context ());
	}

	/*
	   if we have a non-hidden route (ie. we're not the click or the auditioner),
	   pack some route-dependent stuff.
	*/


	setup_meters ();
}

void
GainMeter::setup_gain_adjustment ()
{
	if (!_amp) {
		return;
	}

	if (_previous_amp_output_streams == _amp->output_streams ()) {
		return;
	}

	ignore_toggle = true;

	if (_amp->output_streams().n_midi() <=  _amp->output_streams().n_audio()) {
		_data_type = DataType::AUDIO;
		gain_adjustment.set_lower (0.0);
		gain_adjustment.set_upper (1.0);
		gain_adjustment.set_step_increment (0.01);
		gain_adjustment.set_page_increment (0.1);
		gain_slider.set_default_value (gain_to_slider_position (1));
	} else {
		_data_type = DataType::MIDI;
		gain_adjustment.set_lower (0.0);
		gain_adjustment.set_upper (2.0);
		gain_adjustment.set_step_increment (1.0/128.0);
		gain_adjustment.set_page_increment (10.0/128.0);
		gain_slider.set_default_value (1.0);
	}

	ignore_toggle = false;

	effective_gain_display ();
	
	_previous_amp_output_streams = _amp->output_streams ();
}

void
GainMeter::hide_all_meters ()
{
	level_meter.hide_meters();
}

void
GainMeter::setup_meters ()
{
	level_meter.setup_meters (_meter_width, _thin_meter_width);
}

void
GainMeter::set_type (MeterType t)
{
	level_meter.set_type(t);
}

bool
GainMeter::gain_key_press (GdkEventKey* ev)
{
	/* illegal key for gain entry */
    switch (ev->keyval) {
        case GDK_Escape:
            show_gain();
            gain_display_entry.hide();
            gain_display_button.show();
            return true;
        default:
            break;
	}

	return !key_is_legal_for_numeric_entry (ev->keyval);
}

bool
GainMeter::gain_display_button_press (GdkEventButton* ev)
{
	switch (ev->type) {
        case GDK_BUTTON_PRESS:
            /* MOD1 == "Alt" */
            if (Keyboard::modifier_state_contains (ev->state, GDK_MOD1_MASK)) {
                _amp->set_gain (dB_to_coefficient(0.0), this);
            }
            break;
        case GDK_2BUTTON_PRESS:
            start_gain_level_editing();
            break;
        default:
            break;
    }
    
    return true;
}

bool
GainMeter::gain_display_entry_press (GdkEventButton* ev)
{
    return ev->button == 3;
}

bool
GainMeter::peak_button_release (GdkEventButton* ev)
{
	/* reset peak label */

	if (ev->button == 1 && Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier|Keyboard::TertiaryModifier)) {
		ResetAllPeakDisplays ();
	} else if (ev->button == 1 && Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
		if (_route) {
			ResetGroupPeakDisplays (_route->route_group());
		}
	} else {
		ResetRoutePeakDisplays (_route.get());
	}

	return false;
}

void
GainMeter::reset_peak_display ()
{
	_meter->reset_max();
	level_meter.clear_meters();
	max_peak = -INFINITY;
	peak_display_button.set_text (_("-inf"));
	peak_display_button.set_active_state(Gtkmm2ext::Off);
}

void
GainMeter::reset_route_peak_display (Route* route)
{
	if (_route && _route.get() == route) {
		reset_peak_display ();
	}
}

void
GainMeter::reset_group_peak_display (RouteGroup* group)
{
	if (_route && group == _route->route_group()) {
		reset_peak_display ();
	}
}

void
GainMeter::popup_meter_menu (GdkEventButton *ev)
{
	using namespace Menu_Helpers;

	if (meter_menu == 0) {
		meter_menu = new Gtk::Menu;
		MenuList& items = meter_menu->items();

		items.push_back (MenuElem ("-inf .. +0dBFS"));
		items.push_back (MenuElem ("-10dB .. +0dBFS"));
		items.push_back (MenuElem ("-4 .. +0dBFS"));
		items.push_back (SeparatorElem());
		items.push_back (MenuElem ("-inf .. -2dBFS"));
		items.push_back (MenuElem ("-10dB .. -2dBFS"));
		items.push_back (MenuElem ("-4 .. -2dBFS"));
	}

	meter_menu->popup (1, ev->time);
}

bool
GainMeter::gain_focus_in (GdkEventFocus* ev)
{
	if (ev->in) {
		gain_display_entry.select_region (0, -1);
	} else {
		gain_display_entry.select_region (0, 0);
	}
	return false;
}

bool
GainMeter::gain_focus_out (GdkEventFocus* ev)
{
    gain_activated ();
	return false;
}



void
GainMeter::start_gain_level_editing ()
{
	gain_display_button.hide();
	gain_display_entry.show();
	gain_display_entry.grab_focus();
}

void
GainMeter::gain_activated ()
{
	float f;

	{
		// Switch to user's preferred locale so that
		// if they use different LC_NUMERIC conventions,
		// we will honor them.

		PBD::LocaleGuard lg ("");
		if (sscanf (gain_display_entry.get_text().c_str(), "%f", &f) != 1) {
			return;
		}
	}

	/* clamp to displayable values */
	if (_data_type == DataType::AUDIO) {
		f = min (f, 6.0f);
        
        if (_route && _route->amp() == _amp) {
            
            Selection& selection = ARDOUR_UI::instance()->the_editor().get_selection();
            TimeAxisView* tv = ARDOUR_UI::instance()->the_editor().get_route_view_by_route_id (_route->id() );
            
            // if route is a part of selection and affected_by_selection is set
            if (affected_by_selection && selection.selected(tv) && selection.tracks.size() > 1 ) {
                
                RouteList routes;
                TrackViewList track_list = selection.tracks;
                TrackViewList::const_iterator iter = track_list.begin ();
                for (; iter != track_list.end(); ++iter) {
                    RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*>(*iter);
                    
                    if (rtv) {
                        rtv->route()->set_gain (dB_to_coefficient(f), this);;
                    }
                }
                
            } else {
                _route->set_gain (dB_to_coefficient(f), this);
            }
        } else {
            _amp->set_gain (dB_to_coefficient(f), this);
        }
	} else {
		f = min (fabs (f), 2.0f);
		_amp->set_gain (f, this);
	}

	if (gain_display_entry.has_focus()) {
		Gtk::Widget* w = gain_display_entry.get_toplevel();
		if (w) {
			Gtk::Window* win = dynamic_cast<Gtk::Window*> (w);

			/* sigh. gtkmm doesn't wrap get_default_widget() */

			if (win) {
				GtkWidget* f = gtk_window_get_default_widget (win->gobj());
				if (f) {
					gtk_widget_grab_focus (f);
				}
			}
		}
	}
	gain_display_entry.hide();
	gain_display_button.show();
}

void
GainMeter::show_gain ()
{
	char buf[32];

	float v = gain_adjustment.get_value();

	switch (_data_type) {
	case DataType::AUDIO:
		if (v == 0.0) {
			strcpy (buf, _("-inf"));
		} else {
			snprintf (buf, sizeof (buf), "%.1f", accurate_coefficient_to_dB (slider_position_to_gain_with_max (v, Config->get_max_gain())));
		}
		break;
	case DataType::MIDI:
		snprintf (buf, sizeof (buf), "%.1f", v);
		break;
	}

	gain_display_entry.set_text (buf);
	gain_display_button.set_text (buf);
}

void
GainMeter::gain_adjusted ()
{
	gain_t value;

	/* convert from adjustment range (0..1) to gain coefficient */

	if (_data_type == DataType::AUDIO) {
		value = slider_position_to_gain_with_max (gain_adjustment.get_value(), Config->get_max_gain());
	} else {
		value = gain_adjustment.get_value();
	}
	
	if (!ignore_toggle) {
		if (_route && _route->amp() == _amp) {
            
            Selection& selection = ARDOUR_UI::instance()->the_editor().get_selection();
            TimeAxisView* tv = ARDOUR_UI::instance()->the_editor().get_route_view_by_route_id (_route->id() );
            
            // if route is a part of selection and affected_by_selection is set
            if (affected_by_selection && selection.selected(tv) && selection.tracks.size() > 1 ) {

                RouteList routes;
                TrackViewList track_list = selection.tracks;
                TrackViewList::const_iterator iter = track_list.begin ();
                for (; iter != track_list.end(); ++iter) {
                    RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*>(*iter);
                    
                    if (rtv) {
                        routes.push_back(rtv->route() );
                    }
                }
                
                adjust_gain_relatively(value, routes, this);
                
            } else {
                _route->set_gain (value, this);
            }
            
		} else {
			_amp->set_gain (value, this);
		}
	}

	show_gain ();
}

gain_t
GainMeter::get_relative_gain_factor (gain_t val, RouteList& routes)
{
    gain_t usable_gain = _amp->gain();
    
    // avoid "devide by 0" situation
    if (usable_gain < 0.000001) {
        usable_gain = 0.000001;
    }
    
    gain_t delta = val;
    if (delta < 0.0f) {
        delta = 0.0f;
    }
    
    delta -= usable_gain;
    
    if (delta == 0.0f)
        return 0.0f;
    
    gain_t factor = delta / usable_gain;
    
    if (factor > 0.0f) { // get max factor for selected tracks
        
        // we are already on top
        if (abs(usable_gain - Amp::max_gain_coefficient) < Amp::min_gain_coefficient_gap) {
            return 0.0f;
        }
        
        for (RouteList::iterator i = routes.begin(); i != routes.end(); ++i) {
            gain_t g = (*i)->amp()->gain();
            
            // we are close anough to count this route's on max
            if (abs(g - Amp::max_gain_coefficient) < Amp::min_gain_coefficient_gap ) {
                continue;
            }
            
            // if the current factor woulnd't raise this route above maximum
            if ((g + g * factor) < Amp::max_gain_coefficient) {
                continue;
            }
            
            // factor is calculated so that it would raise current route to max
            factor = Amp::max_gain_coefficient / g - 1.0f;
        }
        
    } else { // get min factor for selected tracks
        
        for (RouteList::iterator i = routes.begin(); i != routes.end(); ++i) {
            gain_t g = (*i)->amp()->gain();
            
            // we are close anough to count this on min
            if (g <= Amp::min_gain_coefficient) {
                continue;
            }
            
            if ((g + g * factor) > Amp::min_gain_coefficient) {
                continue;
            }
            
            // factor is calculated so that it would lower current route to min
            factor = Amp::min_gain_coefficient / g - 1.0f;
        }
    }
    
    return factor;
}

void
GainMeter::adjust_gain_relatively(gain_t val, RouteList& routes, void* src)
{
    gain_t factor = get_relative_gain_factor(val, routes);
    
    for (RouteList::iterator i = routes.begin(); i != routes.end(); ++i) {
        (*i)->inc_gain(factor, src);
    }
}

void
GainMeter::effective_gain_display ()
{
	float value = 0.0;

	switch (_data_type) {
	case DataType::AUDIO:
		value = gain_to_slider_position_with_max (_amp->gain(), Config->get_max_gain());
		break;
	case DataType::MIDI:
		value = _amp->gain ();
		break;
	}

	if (gain_adjustment.get_value() != value) {
		ignore_toggle = true;
		gain_adjustment.set_value (value);
		ignore_toggle = false;
	}
}

void
GainMeter::gain_changed ()
{
	Gtkmm2ext::UI::instance()->call_slot (invalidator (*this), boost::bind (&GainMeter::effective_gain_display, this));
}

void
GainMeter::set_fader_name (const char * name)
{
//	gain_slider.set_name (name);
}

void
GainMeter::update_gain_sensitive ()
{
	bool x = !(_amp->gain_control()->alist()->automation_state() & Play);
	gain_slider.set_sensitive (x);
}

static MeterPoint
next_meter_point (MeterPoint mp)
{
	switch (mp) {
	case MeterInput:
		return MeterPreFader;
		break;

	case MeterPreFader:
		return MeterPostFader;
		break;

	case MeterPostFader:
		return MeterOutput;
		break;

	case MeterOutput:
		return MeterCustom;
		break;

	case MeterCustom:
		return MeterInput;
		break;
	}

	/*NOTREACHED*/
	return MeterInput;
}

gint
GainMeter::meter_press(GdkEventButton* ev)
{
	wait_for_release = false;

	if (!_route) {
		return FALSE;
	}

	if (!ignore_toggle) {

		if (Keyboard::is_context_menu_event (ev)) {

			// no menu at this time.

		} else {

			if (Keyboard::is_button2_event(ev)) {

				// Primary-button2 click is the midi binding click
				// button2-click is "momentary"

				if (!Keyboard::modifier_state_equals (ev->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier))) {
					wait_for_release = true;
					old_meter_point = _route->meter_point ();
				}
			}

			if (_route && (ev->button == 1 || Keyboard::is_button2_event (ev))) {

				if (Keyboard::modifier_state_equals (ev->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::TertiaryModifier))) {

					/* Primary+Tertiary-click applies change to all routes */

					_session->foreach_route (this, &GainMeter::set_meter_point, next_meter_point (_route->meter_point()));


				} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {

					/* Primary-click: solo mix group.
					   NOTE: Primary-button2 is MIDI learn.
					*/

					if (ev->button == 1) {
						set_route_group_meter_point (*_route, next_meter_point (_route->meter_point()));
					}

				} else {

					/* click: change just this route */

					// XXX no undo yet

					_route->set_meter_point (next_meter_point (_route->meter_point()));
				}
			}
		}
	}

	return true;

}

gint
GainMeter::meter_release(GdkEventButton*)
{
	if (!ignore_toggle) {
		if (wait_for_release) {
			wait_for_release = false;

			if (_route) {
				set_meter_point (*_route, old_meter_point);
			}
		}
	}

	return true;
}

void
GainMeter::set_meter_point (Route& route, MeterPoint mp)
{
	route.set_meter_point (mp);
}

void
GainMeter::set_route_group_meter_point (Route& route, MeterPoint mp)
{
	RouteGroup* route_group;

	if ((route_group = route.route_group ()) != 0) {
		route_group->foreach_route (boost::bind (&Route::set_meter_point, _1, mp, false));
	} else {
		route.set_meter_point (mp);
	}
}

void
GainMeter::meter_point_clicked ()
{
	if (_route) {
		/* WHAT? */
	}
}

bool
GainMeter::gain_slider_button_press (GdkEventButton* ev)
{
	switch (ev->type) {
        case GDK_BUTTON_PRESS:
            if (Keyboard::modifier_state_contains (ev->state, GDK_MOD1_MASK)) {
                if (_route && _route->amp() == _amp) {
                    Selection& selection = ARDOUR_UI::instance()->the_editor().get_selection();
                    TimeAxisView* tv = ARDOUR_UI::instance()->the_editor().get_route_view_by_route_id (_route->id() );
                    
                    // if route is a part of selection and affected_by_selection is set
                    if (affected_by_selection && selection.selected(tv) && selection.tracks.size() > 1 ) {
                        
                        RouteList routes;
                        TrackViewList track_list = selection.tracks;
                        TrackViewList::const_iterator iter = track_list.begin ();
                        for (; iter != track_list.end(); ++iter) {
                            RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*>(*iter);
                            
                            if (rtv) {
                                rtv->route()->set_gain (dB_to_coefficient(0.0), this);;
                            }
                        }
                        
                    } else {
                        _route->set_gain (dB_to_coefficient(0.0), this);
                    }
                } else {
                    _amp->set_gain (dB_to_coefficient(0.0), this);
                }
                return true;
            }
            _amp->gain_control()->start_touch (_amp->session().transport_frame());
            break;
        case GDK_2BUTTON_PRESS:
            if (ev->state == 0) {
                _gain_slider_double_clicked = true;
            }
            return false;
        default:
            return false;
	}

	return false;
}

bool
GainMeter::gain_slider_button_release (GdkEventButton* ev)
{
	_amp->gain_control()->stop_touch (false, _amp->session().transport_frame());
    if (_gain_slider_double_clicked) {
        start_gain_level_editing ();
        _gain_slider_double_clicked = false;
    }
	return false;
}

string
GainMeter::astate_string (AutoState state)
{
	return _astate_string (state, false);
}

string
GainMeter::short_astate_string (AutoState state)
{
	return _astate_string (state, true);
}

string
GainMeter::_astate_string (AutoState state, bool shrt)
{
	string sstr;

	switch (state) {
	case ARDOUR::Off:
		sstr = (shrt ? "OFF" : _("OFF"));
		break;
	case Play:
		sstr = (shrt ? "READ" : _("READ"));
		break;
	case Touch:
		sstr = (shrt ? "TOUCH" : _("TOUCH"));
		break;
	case Write:
		sstr = (shrt ? "WRITE" : _("WRITE"));
		break;
	}

	return sstr;
}

string
GainMeter::astyle_string (AutoStyle style)
{
	return _astyle_string (style, false);
}

string
GainMeter::short_astyle_string (AutoStyle style)
{
	return _astyle_string (style, true);
}

string
GainMeter::_astyle_string (AutoStyle style, bool shrt)
{
	if (style & Trim) {
		return _("Trim");
	} else {
	        /* XXX it might different in different languages */

		return (shrt ? _("Abs") : _("Abs"));
	}
}

void
GainMeter::update_meters()
{
	char buf[32];
	float mpeak = level_meter.update_meters();

	if (mpeak > max_peak) {
		max_peak = mpeak;
		if (mpeak <= -200.0f) {
			peak_display_button.set_text (_("-inf"));
		} else {
			snprintf (buf, sizeof(buf), "%.1f", mpeak);
			peak_display_button.set_text (buf);
		}
	}
	if (mpeak >= Config->get_meter_peak()) {
		//peak_display_button.set_name ("MixerStripPeakDisplayPeak");
		peak_display_button.set_active_state(Gtkmm2ext::ExplicitActive);
	}
}

void GainMeter::color_handler(bool /*dpi*/)
{
	setup_meters();
}

void
GainMeter::on_theme_changed()
{
}

void
GainMeter::redraw_metrics()
{
}

int
GainMeter::get_gm_width ()
{
	//rework it

	Gtk::Requisition sz;
	int min_w = 0;
	sz.width = 0;
	level_meter.size_request (sz);
	min_w += sz.width;

	gain_slider.size_request (sz);
	return sz.width + min_w + 6;
}

boost::shared_ptr<PBD::Controllable>
GainMeter::get_controllable()
{
	if (_amp) {
		return _amp->gain_control();
	} else {
		return boost::shared_ptr<PBD::Controllable>();
	}
}

bool
GainMeter::level_meter_button_press (GdkEventButton* ev)
{
	return LevelMeterButtonPress (ev); /* EMIT SIGNAL */
}

void
GainMeter::meter_configuration_changed (ChanCount c)
{
	int type = 0;
	_types.clear ();

	for (DataType::iterator i = DataType::begin(); i != DataType::end(); ++i) {
		if (c.get (*i) > 0) {
			_types.push_back (*i);
			type |= 1 << (*i);
		}
	}

	setup_meters();
	meter_clear_pattern_cache(4);
}

void
GainMeter::route_active_changed ()
{
	if (_meter) {
		meter_configuration_changed (_meter->input_streams ());
	}
}

void
GainMeter::meter_type_changed (MeterType t)
{
	_route->set_meter_type(t);
	RedrawMetrics();
}
