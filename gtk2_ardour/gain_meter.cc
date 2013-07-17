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
#include "midi++/manager.h"
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

#include "ardour/session.h"
#include "ardour/route.h"
#include "ardour/meter.h"
#include "ardour/audio_track.h"
#include "ardour/midi_track.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace std;
using Gtkmm2ext::Keyboard;
using namespace ArdourMeter;

GainMeterBase::GainMeterBase (Session* s, bool horizontal, int fader_length, int fader_girth)
	: gain_adjustment (gain_to_slider_position_with_max (1.0, Config->get_max_gain()), 0.0, 1.0, 0.01, 0.1)
	, gain_automation_style_button ("")
	, gain_automation_state_button ("")
	, _data_type (DataType::AUDIO)

{
	using namespace Menu_Helpers;

	set_session (s);

	ignore_toggle = false;
	meter_menu = 0;
	next_release_selects = false;
	_width = Wide;

	if (horizontal) {
		gain_slider = manage (new HSliderController (&gain_adjustment, fader_length, fader_girth, false));
	} else {
		gain_slider = manage (new VSliderController (&gain_adjustment, fader_length, fader_girth, false));
	}

	level_meter = new LevelMeter(_session);

	level_meter->ButtonPress.connect_same_thread (_level_meter_connection, boost::bind (&GainMeterBase::level_meter_button_press, this, _1));
	meter_metric_area.signal_button_press_event().connect (sigc::mem_fun (*this, &GainMeterBase::level_meter_button_press));
	meter_metric_area.add_events (Gdk::BUTTON_PRESS_MASK);

	gain_slider->signal_button_press_event().connect (sigc::mem_fun(*this, &GainMeter::gain_slider_button_press), false);
	gain_slider->signal_button_release_event().connect (sigc::mem_fun(*this, &GainMeter::gain_slider_button_release), false);
	gain_slider->set_name ("GainFader");

	gain_display.set_name ("MixerStripGainDisplay");
	set_size_request_to_display_given_text (gain_display, "-80.g", 2, 6); /* note the descender */
	gain_display.signal_activate().connect (sigc::mem_fun (*this, &GainMeter::gain_activated));
	gain_display.signal_focus_in_event().connect (sigc::mem_fun (*this, &GainMeter::gain_focused), false);
	gain_display.signal_focus_out_event().connect (sigc::mem_fun (*this, &GainMeter::gain_focused), false);

	peak_display.set_name ("MixerStripPeakDisplay");
	set_size_request_to_display_given_text (peak_display, "-80.g", 2, 6); /* note the descender */
	max_peak = minus_infinity();
	peak_display.set_label (_("-inf"));
	peak_display.unset_flags (Gtk::CAN_FOCUS);

	gain_automation_style_button.set_name ("mixer strip button");
	gain_automation_state_button.set_name ("mixer strip button");

	ARDOUR_UI::instance()->set_tip (gain_automation_state_button, _("Fader automation mode"));
	ARDOUR_UI::instance()->set_tip (gain_automation_style_button, _("Fader automation type"));

	gain_automation_style_button.unset_flags (Gtk::CAN_FOCUS);
	gain_automation_state_button.unset_flags (Gtk::CAN_FOCUS);

	gain_automation_state_button.set_size_request(15, 15);
	gain_automation_style_button.set_size_request(15, 15);

	gain_astyle_menu.items().push_back (MenuElem (_("Trim")));
	gain_astyle_menu.items().push_back (MenuElem (_("Abs")));

	gain_astate_menu.set_name ("ArdourContextMenu");
	gain_astyle_menu.set_name ("ArdourContextMenu");

	gain_adjustment.signal_value_changed().connect (sigc::mem_fun(*this, &GainMeterBase::gain_adjusted));
	peak_display.signal_button_release_event().connect (sigc::mem_fun(*this, &GainMeterBase::peak_button_release), false);
	gain_display.signal_key_press_event().connect (sigc::mem_fun(*this, &GainMeterBase::gain_key_press), false);

	ResetAllPeakDisplays.connect (sigc::mem_fun(*this, &GainMeterBase::reset_peak_display));
	ResetRoutePeakDisplays.connect (sigc::mem_fun(*this, &GainMeterBase::reset_route_peak_display));
	ResetGroupPeakDisplays.connect (sigc::mem_fun(*this, &GainMeterBase::reset_group_peak_display));
	RedrawMetrics.connect (sigc::mem_fun(*this, &GainMeterBase::redraw_metrics));

	UI::instance()->theme_changed.connect (sigc::mem_fun(*this, &GainMeterBase::on_theme_changed));
	ColorsChanged.connect (sigc::bind(sigc::mem_fun (*this, &GainMeterBase::color_handler), false));
	DPIReset.connect (sigc::bind(sigc::mem_fun (*this, &GainMeterBase::color_handler), true));
	
//	PBD::ScopedConnection _config_connection;
//	Config->ParameterChanged.connect ( _config_connection, MISSING_INVALIDATOR, boost::bind(&GainMeterBase::set_flat_buttons, this, _1), gui_context() );
}

void
GainMeterBase::set_flat_buttons ()
{
printf("set_flat_butt\n");
//	gain_slider->set_flat_buttons( ARDOUR_UI::config()->get_flat_buttons() );
}

GainMeterBase::~GainMeterBase ()
{
	delete meter_menu;
	delete level_meter;
}

void
GainMeterBase::set_controls (boost::shared_ptr<Route> r,
			     boost::shared_ptr<PeakMeter> pm,
			     boost::shared_ptr<Amp> amp)
{
 	connections.clear ();
	model_connections.drop_connections ();

	if (!pm && !amp) {
		level_meter->set_meter (0);
		gain_slider->set_controllable (boost::shared_ptr<PBD::Controllable>());
		_meter.reset ();
		_amp.reset ();
		_route.reset ();
		return;
	}

	_meter = pm;
	_amp = amp;
	_route = r;

 	level_meter->set_meter (pm.get());
	gain_slider->set_controllable (amp->gain_control());

	if (amp) {
		amp->ConfigurationChanged.connect (
			model_connections, invalidator (*this), boost::bind (&GainMeterBase::setup_gain_adjustment, this), gui_context ()
			);
	}

	setup_gain_adjustment ();

	if (!_route || !_route->is_auditioner()) {

		using namespace Menu_Helpers;

		gain_astate_menu.items().clear ();

		gain_astate_menu.items().push_back (MenuElem (S_("Automation|Manual"),
							      sigc::bind (sigc::mem_fun (*(amp.get()), &Automatable::set_parameter_automation_state),
									  Evoral::Parameter(GainAutomation), (AutoState) ARDOUR::Off)));
		gain_astate_menu.items().push_back (MenuElem (_("Play"),
							      sigc::bind (sigc::mem_fun (*(amp.get()), &Automatable::set_parameter_automation_state),
								    Evoral::Parameter(GainAutomation), (AutoState) Play)));
		gain_astate_menu.items().push_back (MenuElem (_("Write"),
							      sigc::bind (sigc::mem_fun (*(amp.get()), &Automatable::set_parameter_automation_state),
								    Evoral::Parameter(GainAutomation), (AutoState) Write)));
		gain_astate_menu.items().push_back (MenuElem (_("Touch"),
							      sigc::bind (sigc::mem_fun (*(amp.get()), &Automatable::set_parameter_automation_state),
								    Evoral::Parameter(GainAutomation), (AutoState) Touch)));

		connections.push_back (gain_automation_style_button.signal_button_press_event().connect (sigc::mem_fun(*this, &GainMeterBase::gain_automation_style_button_event), false));
		connections.push_back (gain_automation_state_button.signal_button_press_event().connect (sigc::mem_fun(*this, &GainMeterBase::gain_automation_state_button_event), false));

		boost::shared_ptr<AutomationControl> gc = amp->gain_control();

		gc->alist()->automation_state_changed.connect (model_connections, invalidator (*this), boost::bind (&GainMeter::gain_automation_state_changed, this), gui_context());
		gc->alist()->automation_style_changed.connect (model_connections, invalidator (*this), boost::bind (&GainMeter::gain_automation_style_changed, this), gui_context());

		gain_automation_state_changed ();
	}

	amp->gain_control()->Changed.connect (model_connections, invalidator (*this), boost::bind (&GainMeterBase::gain_changed, this), gui_context());

	gain_changed ();
	show_gain ();
	update_gain_sensitive ();
}

void
GainMeterBase::setup_gain_adjustment ()
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
		gain_slider->set_default_value (gain_to_slider_position (1));
	} else {
		_data_type = DataType::MIDI;
		gain_adjustment.set_lower (0.0);
		gain_adjustment.set_upper (2.0);
		gain_adjustment.set_step_increment (1.0/128.0);
		gain_adjustment.set_page_increment (10.0/128.0);
		gain_slider->set_default_value (1.0);
	}

	ignore_toggle = false;

	effective_gain_display ();
	
	_previous_amp_output_streams = _amp->output_streams ();
}

void
GainMeterBase::hide_all_meters ()
{
	level_meter->hide_meters();
}

void
GainMeter::hide_all_meters ()
{
	GainMeterBase::hide_all_meters ();
}

void
GainMeterBase::setup_meters (int len)
{
	int meter_width = 5;

	switch (_width) {
		case Wide:
			meter_ticks1_area.show();
			meter_ticks2_area.show();
			if (_route && _route->shared_peak_meter()->input_streams().n_total() == 1) {
				meter_width = 10;
			}
			break;
		case Narrow:
			meter_width = 2;
			meter_ticks1_area.hide();
			meter_ticks2_area.hide();
			break;
	}
	level_meter->setup_meters(len, meter_width);
}

void
GainMeterBase::set_type (MeterType t)
{
	level_meter->set_type(t);
}

void
GainMeter::setup_meters (int len)
{
	switch (_width) {
		case Wide:
			hbox.set_homogeneous(true);
			break;
		case Narrow:
			hbox.set_homogeneous(false);
			break;
	}
	GainMeterBase::setup_meters (len);
}

void
GainMeter::set_type (MeterType t)
{
	GainMeterBase::set_type (t);
}

bool
GainMeterBase::gain_key_press (GdkEventKey* ev)
{
	if (key_is_legal_for_numeric_entry (ev->keyval)) {
		/* drop through to normal handling */
		return false;
	}
	/* illegal key for gain entry */
	return true;
}

bool
GainMeterBase::peak_button_release (GdkEventButton* ev)
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

	return true;
}

void
GainMeterBase::reset_peak_display ()
{
	_meter->reset_max();
	level_meter->clear_meters();
	max_peak = -INFINITY;
	peak_display.set_label (_("-inf"));
	peak_display.set_name ("MixerStripPeakDisplay");
}

void
GainMeterBase::reset_route_peak_display (Route* route)
{
	if (_route && _route.get() == route) {
		reset_peak_display ();
	}
}

void
GainMeterBase::reset_group_peak_display (RouteGroup* group)
{
	if (_route && group == _route->route_group()) {
		reset_peak_display ();
	}
}

void
GainMeterBase::popup_meter_menu (GdkEventButton *ev)
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
GainMeterBase::gain_focused (GdkEventFocus* ev)
{
	if (ev->in) {
		gain_display.select_region (0, -1);
	} else {
		gain_display.select_region (0, 0);
	}
	return false;
}

void
GainMeterBase::gain_activated ()
{
	float f;

	{
		// Switch to user's preferred locale so that
		// if they use different LC_NUMERIC conventions,
		// we will honor them.

		PBD::LocaleGuard lg ("");
		if (sscanf (gain_display.get_text().c_str(), "%f", &f) != 1) {
			return;
		}
	}

	/* clamp to displayable values */
	if (_data_type == DataType::AUDIO) {
		f = min (f, 6.0f);
		_amp->set_gain (dB_to_coefficient(f), this);
	} else {
		f = min (fabs (f), 2.0f);
		_amp->set_gain (f, this);
	}

	if (gain_display.has_focus()) {
		Gtk::Widget* w = gain_display.get_toplevel();
		if (w) {
			Gtk::Window* win = dynamic_cast<Gtk::Window*> (w);

			/* sigh. gtkmm doesn't wrap get_default_widget() */

			if (win) {
				GtkWidget* f = gtk_window_get_default_widget (win->gobj());
				if (f) {
					gtk_widget_grab_focus (f);
					return;
				}
			}
		}
	}
}

void
GainMeterBase::show_gain ()
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

	gain_display.set_text (buf);
}

void
GainMeterBase::gain_adjusted ()
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
			_route->set_gain (value, this);
		} else {
			_amp->set_gain (value, this);
		}
	}

	show_gain ();
}

void
GainMeterBase::effective_gain_display ()
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
GainMeterBase::gain_changed ()
{
	Gtkmm2ext::UI::instance()->call_slot (invalidator (*this), boost::bind (&GainMeterBase::effective_gain_display, this));
}

void
GainMeterBase::set_meter_strip_name (const char * name)
{
	meter_metric_area.set_name (name);
	meter_ticks1_area.set_name (name);
	meter_ticks2_area.set_name (name);
}

void
GainMeterBase::set_fader_name (const char * name)
{
	gain_slider->set_name (name);
}

void
GainMeterBase::update_gain_sensitive ()
{
	bool x = !(_amp->gain_control()->alist()->automation_state() & Play);
	static_cast<Gtkmm2ext::SliderController*>(gain_slider)->set_sensitive (x);
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
GainMeterBase::meter_press(GdkEventButton* ev)
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

					_session->foreach_route (this, &GainMeterBase::set_meter_point, next_meter_point (_route->meter_point()));


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
GainMeterBase::meter_release(GdkEventButton*)
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
GainMeterBase::set_meter_point (Route& route, MeterPoint mp)
{
	route.set_meter_point (mp);
}

void
GainMeterBase::set_route_group_meter_point (Route& route, MeterPoint mp)
{
	RouteGroup* route_group;

	if ((route_group = route.route_group ()) != 0) {
		route_group->foreach_route (boost::bind (&Route::set_meter_point, _1, mp, false));
	} else {
		route.set_meter_point (mp);
	}
}

void
GainMeterBase::meter_point_clicked ()
{
	if (_route) {
		/* WHAT? */
	}
}

bool
GainMeterBase::gain_slider_button_press (GdkEventButton* ev)
{
	switch (ev->type) {
	case GDK_BUTTON_PRESS:
		_amp->gain_control()->start_touch (_amp->session().transport_frame());
		break;
	default:
		return false;
	}

	return false;
}

bool
GainMeterBase::gain_slider_button_release (GdkEventButton*)
{
	_amp->gain_control()->stop_touch (false, _amp->session().transport_frame());
	return false;
}

gint
GainMeterBase::gain_automation_state_button_event (GdkEventButton *ev)
{
	if (ev->type == GDK_BUTTON_RELEASE) {
		return TRUE;
	}

	switch (ev->button) {
		case 1:
			gain_astate_menu.popup (1, ev->time);
			break;
		default:
			break;
	}

	return TRUE;
}

gint
GainMeterBase::gain_automation_style_button_event (GdkEventButton *ev)
{
	if (ev->type == GDK_BUTTON_RELEASE) {
		return TRUE;
	}

	switch (ev->button) {
	case 1:
		gain_astyle_menu.popup (1, ev->time);
		break;
	default:
		break;
	}
	return TRUE;
}

string
GainMeterBase::astate_string (AutoState state)
{
	return _astate_string (state, false);
}

string
GainMeterBase::short_astate_string (AutoState state)
{
	return _astate_string (state, true);
}

string
GainMeterBase::_astate_string (AutoState state, bool shrt)
{
	string sstr;

	switch (state) {
	case ARDOUR::Off:
		sstr = (shrt ? "M" : _("M"));
		break;
	case Play:
		sstr = (shrt ? "P" : _("P"));
		break;
	case Touch:
		sstr = (shrt ? "T" : _("T"));
		break;
	case Write:
		sstr = (shrt ? "W" : _("W"));
		break;
	}

	return sstr;
}

string
GainMeterBase::astyle_string (AutoStyle style)
{
	return _astyle_string (style, false);
}

string
GainMeterBase::short_astyle_string (AutoStyle style)
{
	return _astyle_string (style, true);
}

string
GainMeterBase::_astyle_string (AutoStyle style, bool shrt)
{
	if (style & Trim) {
		return _("Trim");
	} else {
	        /* XXX it might different in different languages */

		return (shrt ? _("Abs") : _("Abs"));
	}
}

void
GainMeterBase::gain_automation_style_changed ()
{
	switch (_width) {
	case Wide:
		gain_automation_style_button.set_text (astyle_string(_amp->gain_control()->alist()->automation_style()));
		break;
	case Narrow:
		gain_automation_style_button.set_text  (short_astyle_string(_amp->gain_control()->alist()->automation_style()));
		break;
	}
}

void
GainMeterBase::gain_automation_state_changed ()
{
	ENSURE_GUI_THREAD (*this, &GainMeterBase::gain_automation_state_changed)

	bool x;

	switch (_width) {
	case Wide:
		gain_automation_state_button.set_text (astate_string(_amp->gain_control()->alist()->automation_state()));
		break;
	case Narrow:
		gain_automation_state_button.set_text (short_astate_string(_amp->gain_control()->alist()->automation_state()));
		break;
	}

	x = (_amp->gain_control()->alist()->automation_state() != ARDOUR::Off);

	if (gain_automation_state_button.get_active() != x) {
		ignore_toggle = true;
		gain_automation_state_button.set_active (x);
		ignore_toggle = false;
	}

	update_gain_sensitive ();

	/* start watching automation so that things move */

	gain_watching.disconnect();

	if (x) {
		gain_watching = ARDOUR_UI::RapidScreenUpdate.connect (sigc::mem_fun (*this, &GainMeterBase::effective_gain_display));
	}
}

void
GainMeterBase::update_meters()
{
	char buf[32];
	float mpeak = level_meter->update_meters();

	if (mpeak > max_peak) {
		max_peak = mpeak;
		if (mpeak <= -200.0f) {
			peak_display.set_label (_("-inf"));
		} else {
			snprintf (buf, sizeof(buf), "%.1f", mpeak);
			peak_display.set_label (buf);
		}
	}
	if (mpeak >= Config->get_meter_peak()) {
		peak_display.set_name ("MixerStripPeakDisplayPeak");
	}
}

void GainMeterBase::color_handler(bool dpi)
{
	setup_meters();
}

void
GainMeterBase::set_width (Width w, int len)
{
	_width = w;
	int meter_width = 5;
	if (_width == Wide && _route && _route->shared_peak_meter()->input_streams().n_total() == 1) {
		meter_width = 10;
	}
	level_meter->setup_meters(len, meter_width);
}


void
GainMeterBase::on_theme_changed()
{
}

void
GainMeterBase::redraw_metrics()
{
	meter_metric_area.queue_draw ();
	meter_ticks1_area.queue_draw ();
	meter_ticks2_area.queue_draw ();
}

GainMeter::GainMeter (Session* s, int fader_length)
	: GainMeterBase (s, false, fader_length, 24)
	, gain_display_box(true, 0)
	, hbox(true, 2)
{
	if (gain_display.get_parent()) {
		gain_display.get_parent()->remove (gain_display);
	}
	gain_display_box.pack_start (gain_display, true, true);

	meter_metric_area.set_name ("AudioTrackMetrics");
	meter_metric_area.set_size_request(24, -1);

	gain_automation_style_button.set_name ("mixer strip button");
	gain_automation_state_button.set_name ("mixer strip button");

	ARDOUR_UI::instance()->set_tip (gain_automation_state_button, _("Fader automation mode"));
	ARDOUR_UI::instance()->set_tip (gain_automation_style_button, _("Fader automation type"));

	gain_automation_style_button.unset_flags (Gtk::CAN_FOCUS);
	gain_automation_state_button.unset_flags (Gtk::CAN_FOCUS);

	gain_automation_state_button.set_size_request(15, 15);
	gain_automation_style_button.set_size_request(15, 15);

	fader_vbox = manage (new Gtk::VBox());
	fader_vbox->set_spacing (0);
	fader_vbox->pack_start (*gain_slider, true, true);

	fader_alignment.set (0.5, 0.5, 0.0, 1.0);
	fader_alignment.add (*fader_vbox);

	hbox.pack_start (fader_alignment, true, true);

	set_spacing (2);

	pack_start (gain_display_box, Gtk::PACK_SHRINK);
	pack_start (hbox, Gtk::PACK_SHRINK);

	meter_alignment.set (0.5, 0.5, 0.0, 1.0);
	meter_alignment.add (*level_meter);

	meter_metric_area.signal_expose_event().connect (
		sigc::mem_fun(*this, &GainMeter::meter_metrics_expose));

	meter_ticks1_area.set_size_request(3,-1);
	meter_ticks2_area.set_size_request(3,-1);

	meter_ticks1_area.signal_expose_event().connect (
			sigc::mem_fun(*this, &GainMeter::meter_ticks1_expose));
	meter_ticks2_area.signal_expose_event().connect (
			sigc::mem_fun(*this, &GainMeter::meter_ticks2_expose));

	meter_hbox.pack_start (meter_metric_area, false, false);
	meter_hbox.pack_start (meter_ticks1_area, false, false);
	meter_hbox.pack_start (meter_alignment, false, false);
	meter_hbox.pack_start (meter_ticks2_area, false, false);
}

void
GainMeter::set_controls (boost::shared_ptr<Route> r,
			 boost::shared_ptr<PeakMeter> meter,
			 boost::shared_ptr<Amp> amp)
{
	if (meter_hbox.get_parent()) {
		hbox.remove (meter_hbox);
	}

//	if (gain_automation_state_button.get_parent()) {
//		fader_vbox->remove (gain_automation_state_button);
//	}

	GainMeterBase::set_controls (r, meter, amp);

	if (_meter) {
		_meter->ConfigurationChanged.connect (
			model_connections, invalidator (*this), boost::bind (&GainMeter::meter_configuration_changed, this, _1), gui_context()
			);
		_meter->TypeChanged.connect (
			model_connections, invalidator (*this), boost::bind (&GainMeter::meter_type_changed, this, _1), gui_context()
			);

		meter_configuration_changed (_meter->input_streams ());
	}


	/*
	   if we have a non-hidden route (ie. we're not the click or the auditioner),
	   pack some route-dependent stuff.
	*/

	hbox.pack_start (meter_hbox, true, true);

//	if (r && !r->is_auditioner()) {
//		fader_vbox->pack_start (gain_automation_state_button, false, false, 0);
//	}

	hbox.show_all ();
	setup_meters ();
}

int
GainMeter::get_gm_width ()
{
	Gtk::Requisition sz;
	int min_w = 0;
	meter_metric_area.size_request (sz);
	min_w += sz.width;
	level_meter->size_request (sz);
	min_w += sz.width;

	fader_alignment.size_request (sz);
	if (_width == Wide)
		return max(sz.width * 2, min_w * 2) + 6;
	else
		return sz.width + min_w + 6;

}

gint
GainMeter::meter_metrics_expose (GdkEventExpose *ev)
{
	return meter_expose_metrics(ev, _types, &meter_metric_area);
}

gint
GainMeter::meter_ticks1_expose (GdkEventExpose *ev)
{
	return meter_expose_ticks(ev, _types, &meter_ticks1_area);
}

gint
GainMeter::meter_ticks2_expose (GdkEventExpose *ev)
{
	return meter_expose_ticks(ev, _types, &meter_ticks2_area);
}

boost::shared_ptr<PBD::Controllable>
GainMeterBase::get_controllable()
{
	if (_amp) {
		return _amp->gain_control();
	} else {
		return boost::shared_ptr<PBD::Controllable>();
	}
}

bool
GainMeterBase::level_meter_button_press (GdkEventButton* ev)
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

	if (_route
			&& boost::dynamic_pointer_cast<AudioTrack>(_route) == 0
			&& boost::dynamic_pointer_cast<MidiTrack>(_route) == 0
			) {
		if (_route->active()) {
			set_meter_strip_name ("AudioBusMetrics");
		} else {
			set_meter_strip_name ("AudioBusMetricsInactive");
		}
	}
	else if (type == (1 << DataType::AUDIO)) {
		if (!_route || _route->active()) {
			set_meter_strip_name ("AudioTrackMetrics");
		} else {
			set_meter_strip_name ("AudioTrackMetricsInactive");
		}
	}
	else if (type == (1 << DataType::MIDI)) {
		if (!_route || _route->active()) {
			set_meter_strip_name ("MidiTrackMetrics");
		} else {
			set_meter_strip_name ("MidiTrackMetricsInactive");
		}
	} else {
		if (!_route || _route->active()) {
			set_meter_strip_name ("AudioMidiTrackMetrics");
		} else {
			set_meter_strip_name ("AudioMidiTrackMetricsInactive");
		}
	}
	meter_clear_pattern_cache(4);
}

void
GainMeter::meter_type_changed (MeterType t)
{
	_route->set_meter_type(t);
}
