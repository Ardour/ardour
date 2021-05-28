/*
 * Copyright (C) 2018-2020 Len Ovens
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

#include "ardour/audioengine.h"
#include "ardour/logmeter.h"
#include "ardour/meter.h"
#include "ardour/pannable.h"
#include "ardour/panner_manager.h"
#include "ardour/panner_shell.h"
#include "ardour/profile.h"
#include "ardour/route.h"
#include "ardour/send.h"
#include "ardour/session.h"
#include "ardour/user_bundle.h"
#include "ardour/value_as_string.h"

#include "gtkmm2ext/doi.h"
#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/menu_elems.h"
#include "gtkmm2ext/utils.h"
#include "pbd/fastlog.h"

#include "widgets/tooltips.h"

#include "ardour_window.h"
#include "enums_convert.h"
#include "foldback_strip.h"
#include "gui_thread.h"
#include "io_selector.h"
#include "keyboard.h"
#include "mixer_ui.h"
#include "public_editor.h"
#include "send_ui.h"
#include "timers.h"
#include "ui_config.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ArdourWidgets;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace std;

#define PX_SCALE(px) std::max ((float)px, rintf ((float)px* UIConfiguration::instance ().get_ui_scale ()))

FoldbackSend::FoldbackSend (boost::shared_ptr<Send> snd, boost::shared_ptr<ARDOUR::Route> sr, boost::shared_ptr<ARDOUR::Route> fr, uint32_t wd)
	: _button (ArdourButton::led_default_elements)
	, _send (snd)
	, _send_route (sr)
	, _foldback_route (fr)
	, _send_proc (snd)
	, _send_del (snd)
	, _width (wd)
	, _pan_control (ArdourKnob::default_elements, ArdourKnob::Flags (ArdourKnob::Detent | ArdourKnob::ArcToZero))
	, _adjustment (gain_to_slider_position_with_max (1.0, Config->get_max_gain ()), 0, 1, 0.01, 0.1)
	, _slider (&_adjustment, boost::shared_ptr<PBD::Controllable> (), 0, max (13.f, rintf (13.f * UIConfiguration::instance ().get_ui_scale ())))
	, _ignore_ui_adjustment (true)
	, _slider_persistant_tooltip (&_slider)
{
	HBox* snd_but_pan = new HBox ();

	_button.set_distinct_led_click (true);
	_button.set_fallthrough_to_parent (true);
	_button.set_led_left (true);
	_button.signal_led_clicked.connect (sigc::mem_fun (*this, &FoldbackSend::led_clicked));
	if (_send_proc->get_pre_fader ()) {
		_button.set_name ("processor prefader");
	} else {
		_button.set_name ("processor postfader");
	}
	_button.set_layout_ellipsize_width (PX_SCALE (_width) * PANGO_SCALE);
	_button.set_text_ellipsize (Pango::ELLIPSIZE_END);
	name_changed ();
	snd_but_pan->pack_start (_button, true, true);
	_button.set_active (_send_proc->enabled ());
	_button.show ();

	if (_foldback_route->input ()->n_ports ().n_audio () == 2) {
		_button.set_layout_ellipsize_width (PX_SCALE (_width - 19) * PANGO_SCALE);
		boost::shared_ptr<Pannable>          pannable = _send_del->panner ()->pannable ();
		boost::shared_ptr<AutomationControl> ac;
		ac = pannable->pan_azimuth_control;
		_pan_control.set_size_request (PX_SCALE (19), PX_SCALE (19));
		_pan_control.set_tooltip_prefix (_("Pan: "));
		_pan_control.set_name ("trim knob");
		_pan_control.set_no_show_all (true);
		snd_but_pan->pack_start (_pan_control, false, false);
		_pan_control.show ();
		_pan_control.set_controllable (ac);
	}
	boost::shared_ptr<AutomationControl> lc;
	lc = _send->gain_control ();
	_slider.set_controllable (lc);
	_slider.set_name ("ProcessorControlSlider");
	_slider.set_text (_("Level"));

	pack_start (*snd_but_pan, Gtk::PACK_SHRINK);
	snd_but_pan->show ();
	pack_start (_slider, true, true);
	_slider.show ();
	level_changed ();

	_adjustment.signal_value_changed ().connect (sigc::mem_fun (*this, &FoldbackSend::level_adjusted));
	lc->Changed.connect (_connections, invalidator (*this), boost::bind (&FoldbackSend::level_changed, this), gui_context ());
	_send_proc->ActiveChanged.connect (_connections, invalidator (*this), boost::bind (&FoldbackSend::send_state_changed, this), gui_context ());
	_button.signal_button_press_event ().connect (sigc::mem_fun (*this, &FoldbackSend::button_press));
	_button.signal_button_release_event ().connect (sigc::mem_fun (*this, &FoldbackSend::button_release));
	_send_route->PropertyChanged.connect (_connections, invalidator (*this), boost::bind (&FoldbackSend::route_property_changed, this, _1), gui_context ());

	show ();
}

FoldbackSend::~FoldbackSend ()
{
	_connections.drop_connections ();
	_slider.set_controllable (boost::shared_ptr<AutomationControl> ());
	_pan_control.set_controllable (boost::shared_ptr<AutomationControl> ());
	_send           = boost::shared_ptr<Send> ();
	_send_route     = boost::shared_ptr<Route> ();
	_foldback_route = boost::shared_ptr<Route> ();
	_send_proc      = boost::shared_ptr<Processor> ();
	_send_del       = boost::shared_ptr<Delivery> ();
}

void
FoldbackSend::route_property_changed (const PropertyChange& what_changed)
{
	if (what_changed.contains (ARDOUR::Properties::name)) {
		name_changed ();
	}
}

void
FoldbackSend::name_changed ()
{
	_button.set_text (_send_route->name ());

	ArdourWidgets::set_tooltip (_button, Gtkmm2ext::markup_escape_text (_send_route->name ()));
}

void
FoldbackSend::led_clicked (GdkEventButton* ev)
{
	if (!_send_proc) {
		return;
	}
	if (_button.get_active ()) {
		_send_proc->enable (false);
	} else {
		_send_proc->enable (true);
	}
}

bool
FoldbackSend::button_press (GdkEventButton* ev)
{
	if (Keyboard::is_context_menu_event (ev)) {
		Menu* menu = build_send_menu ();
		Gtkmm2ext::anchored_menu_popup (menu, &_button, "", ev->button, ev->time);
		return true;
	}
	return false;
}

bool
FoldbackSend::button_release (GdkEventButton* ev)
{
	if (!_send_proc) {
		return false;
	}

	if (Keyboard::is_delete_event (ev)) {
		remove_me ();
		return true;
	} else if (Keyboard::is_button2_event (ev)
#ifndef __APPLE__
	           && (Keyboard::no_modifier_keys_pressed (ev) && ((ev->state & Gdk::BUTTON2_MASK) == Gdk::BUTTON2_MASK))
#endif
	) {
		_send_proc->enable (!_send_proc->enabled ());
		return true;
	}
	return false;
}

void
FoldbackSend::send_state_changed ()
{
	_button.set_active (_send_proc->enabled ());
}

void
FoldbackSend::level_adjusted ()
{
	if (_ignore_ui_adjustment) {
		return;
	}
	boost::shared_ptr<AutomationControl> lc = _send->gain_control ();

	if (!lc) {
		return;
	}

	lc->set_value (lc->interface_to_internal (_adjustment.get_value ()), Controllable::NoGroup);
	set_tooltip ();
}

void
FoldbackSend::level_changed ()
{
	boost::shared_ptr<AutomationControl> lc = _send->gain_control ();
	if (!lc) {
		return;
	}

	_ignore_ui_adjustment = true;

	const double nval = lc->internal_to_interface (lc->get_value ());
	if (_adjustment.get_value () != nval) {
		_adjustment.set_value (nval);
		set_tooltip ();
	}

	_ignore_ui_adjustment = false;
}

void
FoldbackSend::set_tooltip ()
{
	boost::shared_ptr<AutomationControl> lc = _send->gain_control ();

	if (!lc) {
		return;
	}
	std::string tt = ARDOUR::value_as_string (lc->desc (), lc->get_value ());
	string      sm = Gtkmm2ext::markup_escape_text (tt);
	_slider_persistant_tooltip.set_tip (sm);
}

Menu*
FoldbackSend::build_send_menu ()
{
	using namespace Menu_Helpers;

	if (!_send) {
		return NULL;
	}

	Menu*     menu  = manage (new Menu);
	MenuList& items = menu->items ();
	menu->set_name ("ArdourContextMenu");

	items.push_back (
	    MenuElem (_("Copy track/bus gain to send"), sigc::bind (sigc::mem_fun (*this, &FoldbackSend::set_gain), -0.1)));
	items.push_back (
	    MenuElem (_("Set send gain to -inf"), sigc::bind (sigc::mem_fun (*this, &FoldbackSend::set_gain), 0.0)));
	items.push_back (
	    MenuElem (_("Set send gain to 0dB"), sigc::bind (sigc::mem_fun (*this, &FoldbackSend::set_gain), 1.0)));

	items.push_back (SeparatorElem());

	if (_send_proc->get_pre_fader ()) {
		items.push_back (
		    MenuElem (_("Set send post fader"), sigc::bind (sigc::mem_fun (*this, &FoldbackSend::set_send_position), true)));
	} else {
		items.push_back (
		    MenuElem (_("Set send pre fader"), sigc::bind (sigc::mem_fun (*this, &FoldbackSend::set_send_position), false)));
	}

	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Remove This Send"), sigc::mem_fun (*this, &FoldbackSend::remove_me)));

	return menu;
}

void
FoldbackSend::set_gain (float new_gain)
{
	if (new_gain < 0) {
		/* get level from sending route */
		new_gain = _send_route->gain_control ()->get_value ();
	}
	boost::shared_ptr<AutomationControl> lc = _send->gain_control ();

	if (!lc) {
		return;
	}
	lc->set_value (new_gain, Controllable::NoGroup);
}

void
FoldbackSend::set_send_position (bool post)
{
	boost::shared_ptr<Route> new_snd_rt = _send_route;
	boost::shared_ptr<Route> new_fb_rt  = _foldback_route;
	float                    new_level  = _send->gain_control ()->get_value ();
	bool                     new_enable = _send_proc->enabled ();
	bool                     is_pan     = false;
	float                    new_pan    = 0.0;
	if (_foldback_route->input ()->n_ports ().n_audio () == 2) {
		is_pan                                   = true;
		boost::shared_ptr<Pannable>     pannable = _send_del->panner ()->pannable ();
		boost::shared_ptr<Controllable> ac;
		ac      = pannable->pan_azimuth_control;
		new_pan = ac->get_value ();
	}

	remove_me ();
	new_snd_rt->add_foldback_send (new_fb_rt, post);

	boost::shared_ptr<Send> snd = new_snd_rt->internal_send_for (new_fb_rt);
	if (snd) {
		snd->gain_control ()->set_value (new_level, Controllable::NoGroup);
		boost::shared_ptr<Processor> snd_proc = boost::dynamic_pointer_cast<Processor> (snd);
		snd_proc->enable (new_enable);
		if (is_pan) {
			boost::shared_ptr<Delivery>     new_del  = boost::dynamic_pointer_cast<Delivery> (snd);
			boost::shared_ptr<Pannable>     pannable = new_del->panner ()->pannable ();
			boost::shared_ptr<Controllable> ac;
			ac = pannable->pan_azimuth_control;
			ac->set_value (new_pan, Controllable::NoGroup);
		}
	}
}

void
FoldbackSend::remove_me ()
{
	boost::shared_ptr<Processor> send_proc = boost::dynamic_pointer_cast<Processor> (_send);
	_connections.drop_connections ();
	_send_route->remove_processor (send_proc);
}

/* ****************************************************************************/

PBD::Signal1<void, FoldbackStrip*> FoldbackStrip::CatchDeletion;

FoldbackStrip::FoldbackStrip (Mixer_UI& mx, Session* sess, boost::shared_ptr<Route> rt)
	: SessionHandlePtr (sess)
	, RouteUI (sess)
	, _mixer (mx)
	, _showing_sends (false)
	, _width (80)
	, _panners (sess)
	, _output_button (false)
	, _comment_button (_("Comments"))
	, _level_control (ArdourKnob::default_elements, ArdourKnob::Detent)
	, _meter (0)
{
	init ();
	set_route (rt);
}

void
FoldbackStrip::init ()
{
	_previous_button.set_name ("mixer strip button");
	_previous_button.set_icon (ArdourIcon::ScrollLeft);
	_previous_button.set_tweaks (ArdourButton::Square);
	UI::instance ()->set_tip (&_previous_button, _("Previous foldback bus"), "");
	_previous_button.set_sensitive (false);

	_next_button.set_name ("mixer strip button");
	_next_button.set_icon (ArdourIcon::ScrollRight);
	_next_button.set_tweaks (ArdourButton::Square);
	UI::instance ()->set_tip (&_next_button, _("Next foldback bus"), "");
	_next_button.set_sensitive (false);

	_hide_button.set_name ("mixer strip button");
	_hide_button.set_icon (ArdourIcon::HideEye);
	_hide_button.set_tweaks (ArdourButton::Square);
	set_tooltip (&_hide_button, _("Hide Foldback strip"));

	_number_label.set_elements((ArdourButton::Element)(ArdourButton::Edge|ArdourButton::Body|ArdourButton::Text|ArdourButton::Inactive));
	_number_label.set_no_show_all ();
	_number_label.set_name ("generic button");
	_number_label.set_alignment (.5, .5);
	_number_label.set_fallthrough_to_parent (true);

	_prev_next_box.set_spacing (2);
	_prev_next_box.pack_start (_previous_button, false, true);
	_prev_next_box.pack_start (_next_button, false, true);
	_prev_next_box.pack_start (_number_label, true, true);
	_prev_next_box.pack_end (_hide_button, false, true);

	_name_button.set_name ("mixer strip button");
	_name_button.set_text_ellipsize (Pango::ELLIPSIZE_END);
	_name_button.set_layout_ellipsize_width (PX_SCALE (_width) * PANGO_SCALE);

	_send_display.set_flags (CAN_FOCUS);
	_send_display.set_spacing (4);

	_send_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
	_send_scroller.add (_send_display);
	_send_scroller.get_child ()->set_name ("FoldbackBusStripBase");

	_panners.set_width (Wide);

	_insert_box = new ProcessorBox (0, boost::bind (&FoldbackStrip::plugin_selector, this), _pr_selection, 0);
	_insert_box->set_no_show_all ();
	_insert_box->show ();
	_insert_box->set_session (_session);
	_insert_box->set_width (Wide);
	_insert_box->set_size_request (PX_SCALE (_width + 34), PX_SCALE (160));

	_meter = new FastMeter ((uint32_t)floor (UIConfiguration::instance ().get_meter_hold ()),
	                        8, FastMeter::Horizontal, PX_SCALE (100),
	                        UIConfiguration::instance ().color ("meter color0"),
	                        UIConfiguration::instance ().color ("meter color1"),
	                        UIConfiguration::instance ().color ("meter color2"),
	                        UIConfiguration::instance ().color ("meter color3"),
	                        UIConfiguration::instance ().color ("meter color4"),
	                        UIConfiguration::instance ().color ("meter color5"),
	                        UIConfiguration::instance ().color ("meter color6"),
	                        UIConfiguration::instance ().color ("meter color7"),
	                        UIConfiguration::instance ().color ("meter color8"),
	                        UIConfiguration::instance ().color ("meter color9"),
	                        UIConfiguration::instance ().color ("meter background bottom"),
	                        UIConfiguration::instance ().color ("meter background top"),
	                        0x991122ff, 0x551111ff,
	                        (115.0 * log_meter0dB (-15)),
	                        89.125,
	                        106.375,
	                        115.0,
	                        (UIConfiguration::instance ().get_meter_style_led () ? 3 : 1));

	_level_control.set_size_request (PX_SCALE (50), PX_SCALE (50));
	_level_control.set_tooltip_prefix (_("Level: "));
	_level_control.set_name ("foldback knob");

	VBox* lcenter_box = manage (new VBox);
	lcenter_box->pack_start (_level_control, true, false);
	_level_box.pack_start (*lcenter_box, true, false);
	_level_box.set_size_request (PX_SCALE (_width + 34), PX_SCALE (80));
	_level_box.set_name ("FoldbackBusStripBase");
	lcenter_box->show ();

	_output_button.set_text (_("Output"));
	_output_button.set_name ("mixer strip button");
	_output_button.set_text_ellipsize (Pango::ELLIPSIZE_MIDDLE);
	_output_button.set_layout_ellipsize_width (PX_SCALE (_width) * PANGO_SCALE);

	_comment_button.set_name (X_("mixer strip button"));
	_comment_button.set_text_ellipsize (Pango::ELLIPSIZE_END);
	_comment_button.set_layout_ellipsize_width (PX_SCALE (_width) * PANGO_SCALE);

	_global_vpacker.set_border_width (1);
	_global_vpacker.set_spacing (2);

	/* Packing is from top down to the send box. The send box
	 * needs the most room and takes all left over space
	 * Everything below the send box is packed from the bottom up
	 * the panner is the last thing to pack as it doesn't always show
	 * and packing it below the sendbox means nothing moves when it shows
	 * or hides.
	 */
	_global_vpacker.pack_start (_prev_next_box, Gtk::PACK_SHRINK);
	_global_vpacker.pack_start (_name_button, Gtk::PACK_SHRINK);
	_global_vpacker.pack_start (*show_sends_button, Gtk::PACK_SHRINK);
	_global_vpacker.pack_start (invert_button_box, Gtk::PACK_SHRINK);
	_global_vpacker.pack_start (_send_scroller, true, true);
#ifndef MIXBUS
	/* Add a spacer underneath the foldback bus;
	 * this fills the area that is taken up by the scrollbar on the tracks;
	 * and therefore keeps the strip boxes "even" across the bottom
	 */
	int scrollbar_height = 0;
	{
		Gtk::Window window (WINDOW_TOPLEVEL);
		HScrollbar  scrollbar;
		window.add (scrollbar);
		scrollbar.set_name ("MixerWindow");
		scrollbar.ensure_style ();
		Gtk::Requisition requisition (scrollbar.size_request ());
		scrollbar_height = requisition.height;
		scrollbar_height += 3; // track_display_frame border/shadow
	}
	_spacer.set_size_request (-1, scrollbar_height);
	_global_vpacker.pack_end (_spacer, false, false);
	_spacer.show ();
#endif
	_global_vpacker.pack_end (_comment_button, Gtk::PACK_SHRINK);
	_global_vpacker.pack_end (_output_button, Gtk::PACK_SHRINK);
	_global_vpacker.pack_end (_level_box, Gtk::PACK_SHRINK);
	_global_vpacker.pack_end (*_meter, false, false);
	_global_vpacker.pack_end (*solo_button, false, false);
	_global_vpacker.pack_end (_panners, Gtk::PACK_SHRINK);
	_global_vpacker.pack_end (*_insert_box, Gtk::PACK_SHRINK);

	_global_frame.add (_global_vpacker);
	_global_frame.set_shadow_type (Gtk::SHADOW_IN);
	_global_frame.set_name ("MixerStripFrame");
	add (_global_frame);

	_number_label.signal_button_release_event().connect (sigc::mem_fun (*this, &FoldbackStrip::number_button_press), false);
	_name_button.signal_button_press_event ().connect (sigc::mem_fun (*this, &FoldbackStrip::name_button_button_press), false);
	_previous_button.signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &FoldbackStrip::cycle_foldbacks), false));
	_next_button.signal_clicked.connect (sigc::bind (sigc::mem_fun (*this, &FoldbackStrip::cycle_foldbacks), true));
	_hide_button.signal_clicked.connect (sigc::mem_fun (*this, &FoldbackStrip::hide_clicked));
	_send_scroller.signal_button_press_event ().connect (sigc::mem_fun (*this, &FoldbackStrip::send_scroller_press));
	_comment_button.signal_clicked.connect (sigc::mem_fun (*this, &RouteUI::toggle_comment_editor));

	add_events (Gdk::BUTTON_RELEASE_MASK |
	            Gdk::ENTER_NOTIFY_MASK |
	            Gdk::KEY_PRESS_MASK |
	            Gdk::KEY_RELEASE_MASK);

	set_flags (get_flags () | Gtk::CAN_FOCUS);

	signal_enter_notify_event ().connect (sigc::mem_fun (*this, &FoldbackStrip::fb_strip_enter_event));

	Mixer_UI::instance ()->show_spill_change.connect (sigc::mem_fun (*this, &FoldbackStrip::spill_change));
	PresentationInfo::Change.connect (*this, invalidator (*this), boost::bind (&FoldbackStrip::presentation_info_changed, this, _1), gui_context ());
}

FoldbackStrip::~FoldbackStrip ()
{
	CatchDeletion (this);
	clear_send_box ();
}

bool
FoldbackStrip::fb_strip_enter_event (GdkEventCrossing* /*ev*/)
{
	deselect_all_processors ();
	return false;
}

string
FoldbackStrip::name () const
{
	if (_route) {
		return _route->name ();
	}
	return string ();
}

void
FoldbackStrip::set_route (boost::shared_ptr<Route> rt)
{
	if (!rt) {
		clear_send_box ();
		return;
	}

	RouteUI::set_route (rt);

	_output_button.set_route (_route, this);

	int number = 0;
	{
		RouteList rl (_session->get_routelist (true, PresentationInfo::FoldbackBus));
		RouteList::iterator i = find (rl.begin (), rl.end (), _route);
		assert (i != rl.end ());
		number = 1 + std::distance (rl.begin (), i);
	}

	_insert_box->set_route (_route);
	_level_control.set_controllable (_route->gain_control ());
	_level_control.show ();

	_number_label.set_inactive_color (_route->presentation_info().color ());
	_number_label.set_text (PBD::to_string (number));
	_number_label.show ();

	/* setup panners */
	panner_ui ().set_panner (_route->main_outs ()->panner_shell (), _route->main_outs ()->panner ());
	panner_ui ().setup_pan ();
	panner_ui ().set_send_drawing_mode (false);

	if (has_audio_outputs ()) {
		_panners.show_all ();
	} else {
		_panners.hide_all ();
	}

	if (_route->panner_shell ()) {
		update_panner_choices ();
		_route->panner_shell ()->Changed.connect (route_connections, invalidator (*this), boost::bind (&FoldbackStrip::connect_to_pan, this), gui_context ());
	}

	/* set up metering */
	_route->set_meter_point (MeterPostFader);
	_route->set_meter_type (MeterPeak0dB);

	_route->output ()->changed.connect (route_connections, invalidator (*this), boost::bind (&FoldbackStrip::update_output_display, this), gui_context ());
	_route->io_changed.connect (route_connections, invalidator (*this), boost::bind (&FoldbackStrip::io_changed_proxy, this), gui_context ());
	_route->comment_changed.connect (route_connections, invalidator (*this), boost::bind (&FoldbackStrip::setup_comment_button, this), gui_context ());

	_session->FBSendsChanged.connect (route_connections, invalidator (*this), boost::bind (&FoldbackStrip::update_send_box, this), gui_context ());

	/* now force an update of all the various elements */
	name_changed ();
	update_send_box ();
	comment_changed ();
	connect_to_pan ();
	_panners.setup_pan ();
	_panners.show_all ();
	update_output_display ();
	reset_strip_style ();
	setup_comment_button ();

	add_events (Gdk::BUTTON_RELEASE_MASK);
	update_sensitivity ();
	_previous_button.show ();
	_next_button.show ();
	_hide_button.show ();
	_prev_next_box.show ();
	_name_button.show ();
	_send_display.show ();
	_send_scroller.show ();
	show_sends_button->show ();
	_insert_box->show ();
	_meter->show ();
	_level_box.show ();
	_output_button.show ();
	_comment_button.show ();
	_global_frame.show ();
	_global_vpacker.show ();

	show ();
}

struct StripableByPresentationOrder {
	bool operator() (const boost::shared_ptr<Stripable>& a, const boost::shared_ptr<Stripable>& b) const
	{
		return a->presentation_info ().order () < b->presentation_info ().order ();
	}
};

void
FoldbackStrip::update_send_box ()
{
	clear_send_box ();
	if (!_route) {
		return;
	}
	StripableList stripables;
	stripables.clear ();

	Route::FedBy fed_by = _route->fed_by ();
	for (Route::FedBy::iterator i = fed_by.begin (); i != fed_by.end (); ++i) {
		if (i->sends_only) {
			boost::shared_ptr<Route>     rt (i->r.lock ());
			boost::shared_ptr<Stripable> s = boost::dynamic_pointer_cast<Stripable> (rt);
			stripables.push_back (s);
		}
	}
	stripables.sort (StripableByPresentationOrder ());
	for (StripableList::iterator it = stripables.begin (); it != stripables.end (); ++it) {
		boost::shared_ptr<Stripable> s_sp = *it;
		boost::shared_ptr<Route>     s_rt = boost::dynamic_pointer_cast<Route> (s_sp);
		boost::shared_ptr<Send>      snd  = s_rt->internal_send_for (_route);
		if (snd) {
			FoldbackSend* fb_s = new FoldbackSend (snd, s_rt, _route, _width);
			_send_display.pack_start (*fb_s, Gtk::PACK_SHRINK);
			fb_s->show ();
			s_rt->processors_changed.connect (_send_connections, invalidator (*this), boost::bind (&FoldbackStrip::update_send_box, this), gui_context ());
		}
	}
	update_sensitivity ();
}

void
FoldbackStrip::clear_send_box ()
{
	std::vector<Widget*> snd_list = _send_display.get_children ();
	_send_connections.drop_connections ();
	for (uint32_t i = 0; i < snd_list.size (); i++) {
		_send_display.remove (*(snd_list[i]));
		delete snd_list[i];
	}
	snd_list.clear ();
}

void
FoldbackStrip::connect_to_pan ()
{
	ENSURE_GUI_THREAD (*this, &FoldbackStrip::connect_to_pan)

	if (!_route->panner ()) {
		return;
	}

	update_panner_choices ();
}

void
FoldbackStrip::update_panner_choices ()
{
	ENSURE_GUI_THREAD (*this, &FoldbackStrip::update_panner_choices)
	if (!_route->panner_shell ()) {
		return;
	}

	uint32_t in  = _route->output ()->n_ports ().n_audio ();
	uint32_t out = in;
	if (_route->panner ()) {
		in = _route->panner ()->in ().n_audio ();
	}

	_panners.set_available_panners (PannerManager::instance ().PannerManager::get_available_panners (in, out));
}

void
FoldbackStrip::update_output_display ()
{
	_panners.setup_pan ();
	if (has_audio_outputs ()) {
		_panners.show_all ();
	} else {
		_panners.hide_all ();
	}
}

void
FoldbackStrip::io_changed_proxy ()
{
	Glib::signal_idle ().connect_once (sigc::mem_fun (*this, &FoldbackStrip::update_panner_choices));
}

void
FoldbackStrip::setup_comment_button ()
{
	std::string comment = _route->comment ();

	set_tooltip (_comment_button, comment.empty () ? _("Click to add/edit comments") : _route->comment ());

	if (comment.empty ()) {
		_comment_button.set_name ("generic button");
		_comment_button.set_text (_("Comments"));
		return;
	}

	_comment_button.set_name ("comment button");

	string::size_type pos = comment.find_first_of (" \t\n");
	if (pos != string::npos) {
		comment = comment.substr (0, pos);
	}
	if (comment.empty ()) {
		_comment_button.set_text (_("Comments"));
	} else {
		_comment_button.set_text (comment);
	}
}

Gtk::Menu*
FoldbackStrip::build_route_ops_menu ()
{
	using namespace Menu_Helpers;

	Menu*     menu  = manage (new Menu);
	MenuList& items = menu->items ();
	menu->set_name ("ArdourContextMenu");

	bool active = _route->active ();

	if (active) {
		items.push_back (MenuElem (_("Color..."), sigc::mem_fun (*this, &RouteUI::choose_color)));
		items.push_back (MenuElem (_("Comments..."), sigc::mem_fun (*this, &RouteUI::open_comment_editor)));

		items.push_back (MenuElem (_("Outputs..."), sigc::mem_fun (*this, &RouteUI::edit_output_configuration)));

		items.push_back (SeparatorElem ());

		items.push_back (MenuElem (_("Save As Template..."), sigc::mem_fun (*this, &RouteUI::save_as_template)));

		items.push_back (MenuElem (_("Rename..."), sigc::mem_fun (*this, &RouteUI::route_rename)));

		items.push_back (SeparatorElem ());
	}

	items.push_back (CheckMenuElem (_("Active")));
	Gtk::CheckMenuItem* i = dynamic_cast<Gtk::CheckMenuItem*> (&items.back ());
	i->set_active (_route->active ());
	i->set_sensitive (!_session->transport_rolling ());
	i->signal_activate ().connect (sigc::bind (sigc::mem_fun (*this, &RouteUI::set_route_active), !_route->active (), false));

	if (active && !Profile->get_mixbus ()) {
		items.push_back (SeparatorElem ());
		items.push_back (CheckMenuElem (_("Protect Against Denormals"), sigc::mem_fun (*this, &RouteUI::toggle_denormal_protection)));
		denormal_menu_item = dynamic_cast<Gtk::CheckMenuItem*> (&items.back ());
		denormal_menu_item->set_active (_route->denormal_protection ());
	}

	if (active) {
		items.push_back (SeparatorElem ());

		items.push_back (MenuElem (_("Duplicate Foldback Bus"), sigc::mem_fun (*this, &FoldbackStrip::duplicate_current_fb)));

	}

	items.push_back (SeparatorElem ());

	items.push_back (MenuElem (_("Remove"), sigc::mem_fun (*this, &FoldbackStrip::remove_current_fb)));
	return menu;
}

Gtk::Menu*
FoldbackStrip::build_route_select_menu ()
{
	using namespace Menu_Helpers;

	Menu*     menu  = manage (new Menu);
	MenuList& items = menu->items ();
	menu->set_name ("ArdourContextMenu");

	RouteList fb_list;
	fb_list = _session->get_routelist (true, PresentationInfo::FoldbackBus);

	for (RouteList::iterator s = fb_list.begin (); s != fb_list.end (); ++s) {
		boost::shared_ptr<Route> route = (*s);
		if (route == _route) {
			continue;
		}
		items.push_back (MenuElem (route->name (), sigc::bind (sigc::mem_fun (*this, &FoldbackStrip::set_route), route)));
	}
	return menu;
}

bool
FoldbackStrip::name_button_button_press (GdkEventButton* ev)
{
	if (ev->button == 1 && ev->type == GDK_BUTTON_PRESS) {
		StripableList            slist;
		boost::shared_ptr<Route> previous = boost::shared_ptr<Route> ();
		_session->get_stripables (slist, PresentationInfo::FoldbackBus);
		if (slist.size () > 1) {
			Menu* menu = build_route_select_menu ();
			Gtkmm2ext::anchored_menu_popup (menu, &_name_button, "", ev->button, ev->time);
		}
		return true;
	} else if (Keyboard::is_context_menu_event (ev)) {
		Menu* r_menu = build_route_ops_menu ();
		r_menu->popup (ev->button, ev->time);
		return true;
	}
	return false;
}

bool
FoldbackStrip::number_button_press (GdkEventButton* ev)
{
	if (Keyboard::is_context_menu_event (ev)) {
		return name_button_button_press (ev);
	}
	return false;
}

bool
FoldbackStrip::send_scroller_press (GdkEventButton* ev)
{
	if (Keyboard::is_context_menu_event (ev)) {
		/* Show context menu, identical to send button right-click */
		return RouteUI::show_sends_press (ev);
	}
	return true;
}

void
FoldbackStrip::cycle_foldbacks (bool next)
{
	RouteList rl (_session->get_routelist (true, PresentationInfo::FoldbackBus));
	if (rl.size () < 2) {
		return;
	}
	RouteList::iterator i = find (rl.begin (), rl.end (), _route);
	assert (i != rl.end ());

	if (next) {
		if (++i == rl.end ()) {
			i = rl.begin ();
		}
	} else {
		if (i == rl.begin ()) {
			i = rl.end ();
		}
		--i;
	}
	set_route (*i);

	if (_showing_sends) {
		set_showing_sends_to (_route);
		Mixer_UI::instance ()->show_spill (_route);
	}
}

void
FoldbackStrip::update_sensitivity ()
{
	RouteList fb_list (_session->get_routelist (true, PresentationInfo::FoldbackBus));

	if ((fb_list.size () < 2) || (_route == *(fb_list.begin ()))) {
		_previous_button.set_sensitive (false);
	} else {
		_previous_button.set_sensitive (true);
	}
	if ((fb_list.size () < 2) || _route == *(--fb_list.end ())) {
		_next_button.set_sensitive (false);
	} else {
		_next_button.set_sensitive (true);
	}

	bool active = _route && _route->active ();
	show_sends_button->set_sensitive (active && _send_display.get_children ().size () > 0);
	solo_button->set_sensitive (active && Config->get_solo_control_is_listen_control ());
}

void
FoldbackStrip::hide_clicked ()
{
	ActionManager::get_toggle_action (X_("Mixer"), X_("ToggleFoldbackStrip"))->set_active (false);
}

void
FoldbackStrip::presentation_info_changed (PropertyChange const& what_changed)
{
	if (what_changed.contains (Properties::order)) {
		update_send_box ();
	}
}

void
FoldbackStrip::spill_change (boost::shared_ptr<Stripable> s)
{
	if (s == _route) {
		_showing_sends = true;
	} else {
		_showing_sends = false;
	}
}

void
FoldbackStrip::fast_update ()
{
	/* As this is the output level to a DAC, peak level is what is important
	 * So, much like the mackie control, we just want the highest peak from
	 * all channels in the route.
	 */
	boost::shared_ptr<PeakMeter> peak_meter  = _route->shared_peak_meter ();
	const float                  meter_level = peak_meter->meter_level (0, MeterMCP);
	_meter->set (log_meter0dB (meter_level));
}

void
FoldbackStrip::route_property_changed (const PropertyChange& what_changed)
{
	if (what_changed.contains (ARDOUR::Properties::name)) {
		name_changed ();
	}
}

void
FoldbackStrip::route_color_changed ()
{
	_number_label.set_inactive_color (_route->presentation_info().color ());
}

void
FoldbackStrip::name_changed ()
{
	_name_button.set_text (_route->name ());
	set_tooltip (_name_button, Gtkmm2ext::markup_escape_text (_route->name ()));
}

void
FoldbackStrip::reset_strip_style ()
{
	bool active = _route->active ();
	if (active) {
		set_name ("FoldbackBusStripBase");
	} else {
		set_name ("AudioBusStripBaseInactive");
	}

	set_invert_sensitive (active);
	update_sensitivity ();

	_comment_button.set_sensitive (active);
	_output_button.set_sensitive (active);
	_level_control.set_sensitive (active);
	_insert_box->set_sensitive (active);
	solo_button->set_sensitive (active && Config->get_solo_control_is_listen_control ());
}

void
FoldbackStrip::set_button_names ()
{
	show_sends_button->set_text (_("Show Sends"));

	switch (Config->get_listen_position ()) {
		case AfterFaderListen:
			solo_button->set_text (_("AFL"));
			update_sensitivity ();
			break;
		case PreFaderListen:
			solo_button->set_text (_("PFL"));
			update_sensitivity ();
			break;
	}
}

PluginSelector*
FoldbackStrip::plugin_selector ()
{
	return _mixer.plugin_selector ();
}

void
FoldbackStrip::route_active_changed ()
{
	reset_strip_style ();
}

void
FoldbackStrip::deselect_all_processors ()
{
	_insert_box->processor_operation (ProcessorBox::ProcessorsSelectNone);
}

void
FoldbackStrip::create_selected_sends (ARDOUR::Placement p, bool)
{
	boost::shared_ptr<StripableList> slist (new StripableList);
	PresentationInfo::Flag           fl = PresentationInfo::MixerRoutes;
	_session->get_stripables (*slist, fl);

	for (StripableList::iterator i = (*slist).begin (); i != (*slist).end (); ++i) {
		if ((*i)->is_selected () && !(*i)->is_master () && !(*i)->is_monitor ()) {
			boost::shared_ptr<Route> rt = boost::dynamic_pointer_cast<Route> (*i);
			if (rt) {
				rt->add_foldback_send (_route, p == PostFader);
			}
		}
	}
	update_sensitivity ();
}

void
FoldbackStrip::duplicate_current_fb ()
{
	RouteList                new_rt_lst;
	boost::shared_ptr<Route> new_fb;
	boost::shared_ptr<Route> old_fb      = _route;
	string                   new_name_tp = "Foldback";

	/* get number of io so long as it is 1 or 2 */
	uint32_t io = 1;
	if (old_fb->n_outputs ().n_audio () && (old_fb->n_outputs ().n_audio () > 1)) {
		io = 2;
	}

	new_rt_lst = _session->new_audio_route (io, io, 0, 1, new_name_tp, PresentationInfo::FoldbackBus, (uint32_t)-1);
	new_fb     = *(new_rt_lst.begin ());

	if (new_fb) {
		double oldgain = old_fb->gain_control ()->get_value ();
		new_fb->gain_control ()->set_value (oldgain * 0.25, PBD::Controllable::NoGroup);

		Route::FedBy fed_by = old_fb->fed_by ();
		for (Route::FedBy::iterator i = fed_by.begin (); i != fed_by.end (); ++i) {
			if (i->sends_only) {
				boost::shared_ptr<Route>     rt (i->r.lock ());
				boost::shared_ptr<Send>      old_snd  = rt->internal_send_for (old_fb);
				boost::shared_ptr<Processor> old_proc = old_snd;
				bool                         old_pre  = old_proc->get_pre_fader ();
				rt->add_foldback_send (new_fb, !old_pre);
				if (old_snd) {
					float                   old_gain = old_snd->gain_control ()->get_value ();
					boost::shared_ptr<Send> new_snd  = rt->internal_send_for (new_fb);
					new_snd->gain_control ()->set_value (old_gain, PBD::Controllable::NoGroup);
				}
			}
		}
		set_route (new_fb);
		route_rename ();
	} else {
		PBD::error << "Unable to create new FoldbackBus." << endmsg;
	}
}

void
FoldbackStrip::remove_current_fb ()
{
	clear_send_box ();
	StripableList            slist;
	boost::shared_ptr<Route> next      = boost::shared_ptr<Route> ();
	boost::shared_ptr<Route> old_route = _route;
	_session->get_stripables (slist, PresentationInfo::FoldbackBus);
	if (slist.size ()) {
		for (StripableList::iterator s = slist.begin (); s != slist.end (); ++s) {
			if ((*s) != _route) {
				next = boost::dynamic_pointer_cast<Route> (*s);
				break;
			}
		}
	}

	set_route (next);
	_session->remove_route (old_route);
}
