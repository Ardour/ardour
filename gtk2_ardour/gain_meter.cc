/*
 * Copyright (C) 2005-2006 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2015 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2016 Julien "_FrnchFrgg_" RIVAUD <frnchfrgg@free.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <limits.h>

#include <pangomm.h>

#include <gtkmm/alignment.h>
#include <gdkmm/color.h>
#include <gtkmm/style.h>

#include "ardour/amp.h"
#include "ardour/logmeter.h"
#include "ardour/route_group.h"
#include "ardour/session_route.h"
#include "ardour/dB.h"
#include "ardour/utils.h"

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/gtk_ui.h"

#include "widgets/tooltips.h"

#include "pbd/fastlog.h"

#include "gain_meter.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "public_editor.h"
#include "utils.h"
#include "meter_patterns.h"
#include "timers.h"
#include "ui_config.h"

#include "ardour/session.h"
#include "ardour/route.h"
#include "ardour/meter.h"
#include "ardour/audio_track.h"
#include "ardour/midi_track.h"
#include "ardour/dB.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ArdourWidgets;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace std;
using Gtkmm2ext::Keyboard;
using namespace ArdourMeter;

static void
reset_cursor_to_default (Gtk::Entry* widget)
{
	Glib::RefPtr<Gdk::Window> win = widget->get_text_window ();
	if (win) {
		/* C++ doesn't provide a pointer argument version of this
		   (i.e. you cannot set to NULL to get the default/parent
		   cursor)
		*/
		gdk_window_set_cursor (win->gobj(), 0);
	}
}

static void
reset_cursor_to_default_state (Gtk::StateType, Gtk::Entry* widget)
{
	reset_cursor_to_default (widget);
}

sigc::signal<void, ARDOUR::AutoState> GainMeterBase::ChangeGainAutomationState;

GainMeterBase::GainMeterBase (Session* s, bool horizontal, int fader_length, int fader_girth)
	: gain_adjustment (gain_to_slider_position_with_max (1.0, Config->get_max_gain()),  // value
	                   0.0,  // lower
	                   1.0,  // upper
	                   dB_coeff_step(Config->get_max_gain()) / 10.0,  // step increment
	                   dB_coeff_step(Config->get_max_gain()))  // page increment
	, gain_automation_state_button ("")
	, meter_point_button (_("pre"))
	, gain_astate_propagate (false)
	, _data_type (DataType::AUDIO)
	, _clear_meters (true)
	, _meter_peaked (false)
{
	using namespace Menu_Helpers;

	set_session (s);

	ignore_toggle = false;
	meter_menu = 0;
	next_release_selects = false;
	_width = Wide;

	fader_length = rint (fader_length * UIConfiguration::instance().get_ui_scale());
	fader_girth = rint (fader_girth * UIConfiguration::instance().get_ui_scale());

	if (horizontal) {
		gain_slider = manage (new HSliderController (&gain_adjustment, boost::shared_ptr<PBD::Controllable>(), fader_length, fader_girth));
		gain_slider->set_tweaks (ArdourFader::Tweaks(ArdourFader::NoButtonForward | ArdourFader::NoVerticalScroll));
	} else {
		gain_slider = manage (new VSliderController (&gain_adjustment, boost::shared_ptr<PBD::Controllable>(), fader_length, fader_girth));
		gain_slider->set_tweaks (ArdourFader::NoButtonForward);
	}

	level_meter = new LevelMeterHBox(_session);

	level_meter->ButtonPress.connect_same_thread (_level_meter_connection, boost::bind (&GainMeterBase::level_meter_button_press, this, _1));
	meter_metric_area.signal_button_press_event().connect (sigc::mem_fun (*this, &GainMeterBase::level_meter_button_press));
	meter_metric_area.add_events (Gdk::BUTTON_PRESS_MASK);

	gain_slider->StartGesture.connect (sigc::mem_fun (*this, &GainMeter::amp_start_touch));
	gain_slider->StopGesture.connect (sigc::mem_fun (*this, &GainMeter::amp_stop_touch));
	gain_slider->set_name ("GainFader");

	gain_display.set_name ("MixerStripGainDisplay");
	set_size_request_to_display_given_text (gain_display, "-80.g", 2, 6); /* note the descender */
	gain_display.signal_activate().connect (sigc::mem_fun (*this, &GainMeter::gain_activated));
	gain_display.signal_focus_in_event().connect (sigc::mem_fun (*this, &GainMeter::gain_focused), false);
	gain_display.signal_focus_out_event().connect (sigc::mem_fun (*this, &GainMeter::gain_focused), false);
	gain_display.set_alignment (0.5);

	peak_display.set_name ("MixerStripPeakDisplay");
	set_size_request_to_display_given_text (peak_display, "-80.g", 2, 6); /* note the descender */
	max_peak = minus_infinity();
	peak_display.set_text (_("-inf"));
	peak_display.set_alignment (0.5);

	/* stuff related to the fact that the peak display is not, in
	   fact, supposed to be a text entry.
	*/
	peak_display.set_events (peak_display.get_events() & ~(Gdk::EventMask (Gdk::LEAVE_NOTIFY_MASK|Gdk::ENTER_NOTIFY_MASK|Gdk::POINTER_MOTION_MASK)));
	peak_display.signal_map().connect (sigc::bind (sigc::ptr_fun (reset_cursor_to_default), &peak_display));
	peak_display.signal_state_changed().connect (sigc::bind (sigc::ptr_fun (reset_cursor_to_default_state), &peak_display));
	peak_display.unset_flags (Gtk::CAN_FOCUS);
	peak_display.set_editable (false);

	gain_automation_state_button.set_name ("mixer strip button");

	set_tooltip (gain_automation_state_button, _("Fader automation mode"));
	set_tooltip (peak_display, _("dBFS - Digital Peak Hold. Click to reset."));

	gain_automation_state_button.unset_flags (Gtk::CAN_FOCUS);

	gain_automation_state_button.set_size_request(15, 15);

	gain_astate_menu.set_name ("ArdourContextMenu");
	gain_astate_menu.set_reserve_toggle_size(false);

	meter_point_button.set_name ("mixer strip button");

	set_tooltip (&meter_point_button, _("Metering point"));

	meter_point_button.unset_flags (Gtk::CAN_FOCUS);

	meter_point_button.set_size_request(15, 15);

	meter_point_menu.set_name ("ArdourContextMenu");
	meter_point_menu.set_reserve_toggle_size(false);

	meter_point_menu.items().clear ();
	meter_point_menu.items().push_back (MenuElem(meterpt_string(MeterInput),
	 sigc::bind (sigc::mem_fun (*this,
	 &GainMeterBase::meter_point_clicked), (MeterPoint) MeterInput)));
	meter_point_menu.items().push_back (MenuElem(meterpt_string(MeterPreFader),
	 sigc::bind (sigc::mem_fun (*this,
	 &GainMeterBase::meter_point_clicked), (MeterPoint) MeterPreFader)));
	meter_point_menu.items().push_back (MenuElem(meterpt_string (MeterPostFader),
	 sigc::bind (sigc::mem_fun (*this,
	 &GainMeterBase::meter_point_clicked), (MeterPoint) MeterPostFader)));
	meter_point_menu.items().push_back (MenuElem(meterpt_string (MeterOutput),
	 sigc::bind (sigc::mem_fun (*this,
	 &GainMeterBase::meter_point_clicked), (MeterPoint) MeterOutput)));
	meter_point_menu.items().push_back (MenuElem(meterpt_string (MeterCustom),
	 sigc::bind (sigc::mem_fun (*this,
	 &GainMeterBase::meter_point_clicked), (MeterPoint) MeterCustom)));

	meter_point_button.signal_button_press_event().connect (sigc::mem_fun (*this, &GainMeter::meter_press), false);

	gain_adjustment.signal_value_changed().connect (sigc::mem_fun(*this, &GainMeterBase::fader_moved));
	peak_display.signal_button_press_event().connect (sigc::mem_fun(*this, &GainMeterBase::peak_button_press), false);
	peak_display.signal_button_release_event().connect (sigc::mem_fun(*this, &GainMeterBase::peak_button_release), false);
	gain_display.signal_key_press_event().connect (sigc::mem_fun(*this, &GainMeterBase::gain_key_press), false);

	ResetAllPeakDisplays.connect (sigc::mem_fun(*this, &GainMeterBase::reset_peak_display));
	ResetRoutePeakDisplays.connect (sigc::mem_fun(*this, &GainMeterBase::reset_route_peak_display));
	ResetGroupPeakDisplays.connect (sigc::mem_fun(*this, &GainMeterBase::reset_group_peak_display));
	RedrawMetrics.connect (sigc::mem_fun(*this, &GainMeterBase::redraw_metrics));

	UI::instance()->theme_changed.connect (sigc::mem_fun(*this, &GainMeterBase::on_theme_changed));
	UIConfiguration::instance().ColorsChanged.connect (sigc::bind(sigc::mem_fun (*this, &GainMeterBase::color_handler), false));
	UIConfiguration::instance().DPIReset.connect (sigc::bind(sigc::mem_fun (*this, &GainMeterBase::color_handler), true));
}

GainMeterBase::~GainMeterBase ()
{
	delete meter_menu;
	delete level_meter;
}

void
GainMeterBase::set_controls (boost::shared_ptr<Route> r,
			     boost::shared_ptr<PeakMeter> pm,
                             boost::shared_ptr<Amp> amp,
                             boost::shared_ptr<GainControl> control)
{
	connections.clear ();
	model_connections.drop_connections ();

	/* no meter and no control? nothing to do ... */

	if (!pm && !control) {
		level_meter->set_meter (0);
		gain_slider->set_controllable (boost::shared_ptr<PBD::Controllable>());
		_meter.reset ();
		_amp.reset ();
		_route.reset ();
		_control.reset ();
		return;
	}

	_meter = pm;
	_amp = amp;
	_route = r;
	_control = control;

	level_meter->set_meter (pm.get());
	gain_slider->set_controllable (_control);

	if (amp) {
		amp->ConfigurationChanged.connect (
			model_connections, invalidator (*this), boost::bind (&GainMeterBase::setup_gain_adjustment, this), gui_context ()
			);
	}

	setup_gain_adjustment ();

	if (!_route || !_route->is_auditioner()) {

		using namespace Menu_Helpers;

		gain_astate_menu.items().clear ();

		gain_astate_menu.items().push_back (MenuElem (astate_string (ARDOUR::Off),
							      sigc::bind (sigc::mem_fun (*this, &GainMeterBase::set_gain_astate), (AutoState) ARDOUR::Off)));
		gain_astate_menu.items().push_back (MenuElem (astate_string (ARDOUR::Play),
							      sigc::bind (sigc::mem_fun (*this, &GainMeterBase::set_gain_astate), (AutoState) ARDOUR::Play)));
		gain_astate_menu.items().push_back (MenuElem (astate_string (ARDOUR::Write),
							      sigc::bind (sigc::mem_fun (*this, &GainMeterBase::set_gain_astate), (AutoState) ARDOUR::Write)));
		gain_astate_menu.items().push_back (MenuElem (astate_string (ARDOUR::Touch),
							      sigc::bind (sigc::mem_fun (*this, &GainMeterBase::set_gain_astate), (AutoState) ARDOUR::Touch)));
		gain_astate_menu.items().push_back (MenuElem (astate_string (ARDOUR::Latch),
							      sigc::bind (sigc::mem_fun (*this, &GainMeterBase::set_gain_astate), (AutoState) ARDOUR::Latch)));

		connections.push_back (gain_automation_state_button.signal_button_press_event().connect (sigc::mem_fun(*this, &GainMeterBase::gain_automation_state_button_event), false));
		connections.push_back (ChangeGainAutomationState.connect (sigc::mem_fun(*this, &GainMeterBase::set_gain_astate)));

		_control->alist()->automation_state_changed.connect (model_connections, invalidator (*this), boost::bind (&GainMeter::gain_automation_state_changed, this), gui_context());

		gain_automation_state_changed ();
	}

	_control->Changed.connect (model_connections, invalidator (*this), boost::bind (&GainMeterBase::gain_changed, this), gui_context());

	gain_changed ();
	show_gain ();
	update_gain_sensitive ();

	if (!_meter) {
		peak_display.hide ();
	} else {
		peak_display.show ();
	}
}

void
GainMeterBase::set_gain_astate (AutoState as)
{
	if (gain_astate_propagate) {
		gain_astate_propagate = false;
		ChangeGainAutomationState (as);
		return;
	}
	if (_control) {
		_control->set_automation_state (as);
		_session->set_dirty ();
	}
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
		gain_adjustment.set_lower (GAIN_COEFF_ZERO);
		gain_adjustment.set_upper (GAIN_COEFF_UNITY);
		gain_adjustment.set_step_increment (dB_coeff_step(Config->get_max_gain()) / 10.0);
		gain_adjustment.set_page_increment (dB_coeff_step(Config->get_max_gain()));
		gain_slider->set_default_value (gain_to_slider_position_with_max (GAIN_COEFF_UNITY, Config->get_max_gain()));
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
	uint32_t meter_channels = 0;
	if (_meter) {
		meter_channels = _meter->input_streams().n_total();
	} else if (_route) {
		meter_channels = _route->shared_peak_meter()->input_streams().n_total();
	}

	switch (_width) {
		case Wide:
			//meter_ticks1_area.show();
			//meter_ticks2_area.show();
			meter_metric_area.show();
			if (meter_channels == 1) {
				meter_width = 10;
			}
			break;
		case Narrow:
			if (meter_channels > 1) {
				meter_width = 4;
			}
			//meter_ticks1_area.hide();
			//meter_ticks2_area.hide();
			meter_metric_area.hide();
			break;
	}
	level_meter->setup_meters(len, meter_width);
}

void
GainMeter::setup_meters (int len)
{
	switch (_width) {
		case Wide:
			{
				uint32_t meter_channels = 0;
				if (_meter) {
					meter_channels = _meter->input_streams().n_total();
				} else if (_route) {
					meter_channels = _route->shared_peak_meter()->input_streams().n_total();
				}
				hbox.set_homogeneous(meter_channels < 7 ? true : false);
			}
			break;
		case Narrow:
			hbox.set_homogeneous(false);
			break;
	}
	GainMeterBase::setup_meters (len);
}

bool
GainMeterBase::gain_key_press (GdkEventKey* ev)
{
	if (ARDOUR_UI_UTILS::key_is_legal_for_numeric_entry (ev->keyval)) {
		/* drop through to normal handling */
		return false;
	}
	/* illegal key for gain entry */
	return true;
}

bool
GainMeterBase::peak_button_press (GdkEventButton* ev)
{
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
	if (!_route) {
		// catch "reset all" for VCAs
		return;
	}
	_meter->reset_max();
	_clear_meters = true;
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

	// Use the user's preferred locale/LC_NUMERIC setting
	if (sscanf (gain_display.get_text().c_str(), "%f", &f) != 1) {
		return;
	}

	/* clamp to displayable values */
	if (_data_type == DataType::AUDIO) {
		f = min (f, 6.0f);
		_control->set_value (dB_to_coefficient(f), Controllable::UseGroup);
	} else {
		f = min (fabs (f), 2.0f);
		_control->set_value (f, Controllable::UseGroup);
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
GainMeterBase::fader_moved ()
{
	if (!ignore_toggle) {

		gain_t value;

		/* convert from adjustment range (0..1) to gain coefficient */

		if (_data_type == DataType::AUDIO) {
			value = slider_position_to_gain_with_max (gain_adjustment.get_value(), Config->get_max_gain());
		} else {
			value = gain_adjustment.get_value();
		}

		// XXX hack allow to override group
		// (this breaks group'ed  shift+click reset)
		if (Keyboard::the_keyboard().key_is_down (GDK_Shift_R)
				|| Keyboard::the_keyboard().key_is_down (GDK_Shift_L)) {
			_control->set_value (value, Controllable::InverseGroup);
		} else {
			_control->set_value (value, Controllable::UseGroup);
		}
	}

	show_gain ();
}

void
GainMeterBase::effective_gain_display ()
{
	gain_t fader_position = 0;

	switch (_data_type) {
	case DataType::AUDIO:
		/* the position of the fader should reflect any master gain,
		 * not just the control's own inherent value
		 */
		fader_position = gain_to_slider_position_with_max (_control->get_value(), Config->get_max_gain());
		break;
	case DataType::MIDI:
		fader_position = _control->get_value ();
		break;
	}

	if (gain_adjustment.get_value() != fader_position) {
		ignore_toggle = true;
		gain_adjustment.set_value (fader_position);
		ignore_toggle = false;
	}
}

void
GainMeterBase::gain_changed ()
{
	ENSURE_GUI_THREAD (*this, &GainMeterBase::gain_automation_state_changed);
	effective_gain_display ();
}

void
GainMeterBase::set_meter_strip_name (const char * name)
{
	char tmp[256];
	meter_metric_area.set_name (name);
	sprintf(tmp, "Mark%sLeft", name);
	meter_ticks1_area.set_name (tmp);
	sprintf(tmp, "Mark%sRight", name);
	meter_ticks2_area.set_name (tmp);
}

void
GainMeterBase::set_fader_name (const char * name)
{
	gain_slider->set_name (name);
}

void
GainMeterBase::update_gain_sensitive ()
{
	bool x = !(_control->alist()->automation_state() & Play);
	static_cast<ArdourWidgets::SliderController*>(gain_slider)->set_sensitive (x);
}

gint
GainMeterBase::meter_press(GdkEventButton* ev)
{
	if (!_route) {
		return false;
	}
	if (!ignore_toggle) {
		switch (ev->button) {
			case 1:
				if (Keyboard::modifier_state_equals (ev->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::TertiaryModifier))) {

					/* Primary+Tertiary-click applies change to all routes */

					meter_point_change_target = MeterPointChangeAll;

				} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {

					/* Primary-click: apply change to all routes in group */

					meter_point_change_target = MeterPointChangeGroup;

				} else {

					/* click: change just this route */

					meter_point_change_target = MeterPointChangeSingle;
				}
				Gtkmm2ext::anchored_menu_popup(&meter_point_menu,
				                               &meter_point_button,
				                               meterpt_string (_route->meter_point()),
				                               1, ev->time);
				break;
			default:
				break;
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
		route_group->foreach_route (boost::bind (&Route::set_meter_point, _1, mp));
	} else {
		route.set_meter_point (mp);
	}
}

void
GainMeterBase::meter_point_clicked (MeterPoint mp)
{
	if (_route) {
		switch (meter_point_change_target) {
			case MeterPointChangeAll:
				_session->foreach_route (this, &GainMeterBase::set_meter_point, mp);
				break;
			case MeterPointChangeGroup:
				set_route_group_meter_point (*_route, mp);
				break;
			case MeterPointChangeSingle:
				_route->set_meter_point (mp);
				break;
		}
	}
}

void
GainMeterBase::amp_start_touch ()
{
	_control->start_touch (timepos_t (_control->session().transport_sample()));
}

void
GainMeterBase::amp_stop_touch ()
{
	_control->stop_touch (timepos_t (_control->session().transport_sample()));
	effective_gain_display ();
}

gint
GainMeterBase::gain_automation_state_button_event (GdkEventButton *ev)
{
	if (ev->type == GDK_BUTTON_RELEASE) {
		return TRUE;
	}

	switch (ev->button) {
		case 1:
			gain_astate_propagate = Keyboard::modifier_state_contains (ev->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier | Keyboard::TertiaryModifier));
			Gtkmm2ext::anchored_menu_popup(&gain_astate_menu,
			                               &gain_automation_state_button,
			                               astate_string(_control->alist()->automation_state()),
			                               1, ev->time);
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
	switch (state) {
		case ARDOUR::Off:
			return shrt ? S_("Manual|M") : S_("Automation|Manual");
		case Play:
			return shrt ? S_("Play|P") : _("Play");
		case Touch:
			return shrt ? S_("Touch|T") : _("Touch");
		case Latch:
			return shrt ? S_("Latch|L") : _("Latch");
		case Write:
			return shrt ? S_("Write|W"): _("Write");
	}
	assert (0);
	return "???";
}

string
GainMeterBase::meterpt_string (MeterPoint mp)
{
	switch (mp) {
		case MeterInput:
			return _("Input");
		case MeterPreFader:
			return _("Pre Fader");
		case MeterPostFader:
			return _("Post Fader");
		case MeterOutput:
			return _("Output");
		case MeterCustom:
			return _("Custom");
	}
	assert (0);
	return "???"; // make gcc and _FrnchFrgg_ happy
}

void
GainMeterBase::gain_automation_state_changed ()
{
	ENSURE_GUI_THREAD (*this, &GainMeterBase::gain_automation_state_changed);
	gain_automation_state_button.set_text (short_astate_string(_control->alist()->automation_state()));

	const bool automation_watch_required = (_control->alist()->automation_state() != ARDOUR::Off);

	if (gain_automation_state_button.get_active() != automation_watch_required) {
		ignore_toggle = true;
		gain_automation_state_button.set_active (automation_watch_required);
		ignore_toggle = false;
	}

	update_gain_sensitive ();

	gain_watching.disconnect();
}

const ChanCount
GainMeterBase::meter_channels() const
{
		if (_meter) { return _meter->input_streams(); }
		else { return ChanCount(); }
}
void
GainMeterBase::update_meters()
{
	if (_clear_meters) {
		max_peak = minus_infinity ();
		level_meter->clear_meters ();
		peak_display.set_text (_("-inf"));
		peak_display.set_name ("MixerStripPeakDisplay");
		_meter_peaked = false;
		_clear_meters = false;
	}

	float mpeak = level_meter->update_meters();

	if (mpeak > max_peak) {
		max_peak = mpeak;
		if (mpeak <= -200.0f) {
			peak_display.set_text (_("-inf"));
		} else {
			char buf[32];
			snprintf (buf, sizeof(buf), "%.1f", mpeak);
			peak_display.set_text (buf);
		}
	}

	bool peaking = mpeak >= UIConfiguration::instance().get_meter_peak();

	if (!_meter_peaked && peaking) {
			peak_display.set_name ("MixerStripPeakDisplayPeak");
			_meter_peaked = true;
	}
}

void GainMeterBase::color_handler(bool /*dpi*/)
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

#define PX_SCALE(pxmin, dflt) rint(std::max((double)pxmin, (double)dflt * UIConfiguration::instance().get_ui_scale()))

GainMeter::GainMeter (Session* s, int fader_length)
	: GainMeterBase (s, false, fader_length, 24)
	, gain_display_box(true, 0)
	, hbox(true, 2)
{
	if (gain_display.get_parent()) {
		gain_display.get_parent()->remove (gain_display);
	}
	gain_display_box.pack_start (gain_display, true, true);

	if (peak_display.get_parent()) {
		peak_display.get_parent()->remove (gain_display);
	}
	gain_display_box.pack_start (peak_display, true, true);

	meter_metric_area.set_name ("AudioTrackMetrics");
	meter_metric_area.set_size_request(PX_SCALE(24, 24), -1);

	gain_automation_state_button.set_name ("mixer strip button");

	set_tooltip (gain_automation_state_button, _("Fader automation mode"));

	gain_automation_state_button.unset_flags (Gtk::CAN_FOCUS);

	gain_automation_state_button.set_size_request (PX_SCALE(12, 15), PX_SCALE(12, 15));

	fader_vbox.set_spacing (0);
	fader_vbox.pack_start (*gain_slider, true, true);

	fader_alignment.set (0.5, 0.5, 0.0, 1.0);
	fader_alignment.add (fader_vbox);

	hbox.pack_start (fader_alignment, true, true);

	set_spacing (PX_SCALE(2, 2));

	pack_start (gain_display_box, Gtk::PACK_SHRINK);
	pack_start (hbox, true, true);

	meter_alignment.set (0.5, 0.5, 0.0, 1.0);
	meter_alignment.add (*level_meter);

	meter_metric_area.signal_expose_event().connect (
		sigc::mem_fun(*this, &GainMeter::meter_metrics_expose));

	meter_ticks1_area.set_size_request (PX_SCALE(3, 3), -1);
	meter_ticks2_area.set_size_request (PX_SCALE(3, 3), -1);

	meter_ticks1_area.signal_expose_event().connect (
			sigc::mem_fun(*this, &GainMeter::meter_ticks1_expose));
	meter_ticks2_area.signal_expose_event().connect (
			sigc::mem_fun(*this, &GainMeter::meter_ticks2_expose));

	meter_hbox.pack_start (meter_ticks1_area, false, false);
	meter_hbox.pack_start (meter_alignment, false, false);
	meter_hbox.pack_start (meter_ticks2_area, false, false);
	meter_hbox.pack_start (meter_metric_area, false, false);

	meter_metric_area.set_no_show_all ();
}
#undef PX_SCALE

GainMeter::~GainMeter () { }

void
GainMeter::set_controls (boost::shared_ptr<Route> r,
			 boost::shared_ptr<PeakMeter> meter,
                         boost::shared_ptr<Amp> amp,
			 boost::shared_ptr<GainControl> control)
{
	if (meter_hbox.get_parent()) {
		hbox.remove (meter_hbox);
	}

//	if (gain_automation_state_button.get_parent()) {
//		fader_vbox->remove (gain_automation_state_button);
//	}

	GainMeterBase::set_controls (r, meter, amp, control);

	if (_meter) {
		_meter->ConfigurationChanged.connect (
			model_connections, invalidator (*this), boost::bind (&GainMeter::meter_configuration_changed, this, _1), gui_context()
			);
		_meter->MeterTypeChanged.connect (
			model_connections, invalidator (*this), boost::bind (&GainMeter::redraw_metrics, this), gui_context()
			);

		meter_configuration_changed (_meter->input_streams ());
	}


	if (_route) {
		_route->active_changed.connect (model_connections, invalidator (*this), boost::bind (&GainMeter::route_active_changed, this), gui_context ());
		hbox.pack_start (meter_hbox, true, true);
		meter_hbox.show ();
	}

//	if (r && !r->is_auditioner()) {
//		fader_vbox->pack_start (gain_automation_state_button, false, false, 0);
//	}

	gain_display_box.show ();
	gain_display.show ();
	gain_slider->show ();
	fader_vbox.show ();
	fader_alignment.show ();
	hbox.show ();
	setup_meters ();
}

int
GainMeter::get_gm_width ()
{
	Gtk::Requisition sz;
	int min_w = 0;
	sz.width = 0;
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
	if (!_route) {
		if (_types.empty()) { _types.push_back(DataType::AUDIO); }
		return meter_expose_metrics(ev, MeterPeak, _types, &meter_metric_area);
	}
	return meter_expose_metrics(ev, _route->meter_type(), _types, &meter_metric_area);
}

gint
GainMeter::meter_ticks1_expose (GdkEventExpose *ev)
{
	if (!_route) {
		if (_types.empty()) { _types.push_back(DataType::AUDIO); }
		return meter_expose_ticks(ev, MeterPeak, _types, &meter_ticks1_area);
	}
	return meter_expose_ticks(ev, _route->meter_type(), _types, &meter_ticks1_area);
}

gint
GainMeter::meter_ticks2_expose (GdkEventExpose *ev)
{
	if (!_route) {
		if (_types.empty()) { _types.push_back(DataType::AUDIO); }
		return meter_expose_ticks(ev, MeterPeak, _types, &meter_ticks2_area);
	}
	return meter_expose_ticks(ev, _route->meter_type(), _types, &meter_ticks2_area);
}

void
GainMeter::on_style_changed (const Glib::RefPtr<Gtk::Style>&)
{
	gain_display.queue_draw();
	peak_display.queue_draw();
}

boost::shared_ptr<PBD::Controllable>
GainMeterBase::get_controllable()
{
	if (_amp) {
		return _control;
	} else {
		return boost::shared_ptr<PBD::Controllable>();
	}
}

bool
GainMeterBase::level_meter_button_press (GdkEventButton* ev)
{
	return static_cast<bool>(LevelMeterButtonPress (ev)); /* EMIT SIGNAL */
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
	else if (
			   (type == (1 << DataType::MIDI))
			|| (_route && boost::dynamic_pointer_cast<MidiTrack>(_route))
			) {
		if (!_route || _route->active()) {
			set_meter_strip_name ("MidiTrackMetrics");
		} else {
			set_meter_strip_name ("MidiTrackMetricsInactive");
		}
	}
	else if (type == (1 << DataType::AUDIO)) {
		if (!_route || _route->active()) {
			set_meter_strip_name ("AudioTrackMetrics");
		} else {
			set_meter_strip_name ("AudioTrackMetricsInactive");
		}
	} else {
		if (!_route || _route->active()) {
			set_meter_strip_name ("AudioMidiTrackMetrics");
		} else {
			set_meter_strip_name ("AudioMidiTrackMetricsInactive");
		}
	}

	setup_meters();
	meter_clear_pattern_cache(4);
	on_style_changed(Glib::RefPtr<Gtk::Style>());
}

void
GainMeter::route_active_changed ()
{
	if (_meter) {
		meter_configuration_changed (_meter->input_streams ());
	}
}

void
GainMeter::redraw_metrics ()
{
	GainMeterBase::redraw_metrics ();
}
