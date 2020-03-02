/*
    Copyright (C) 2018-2019 Len Ovens

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

#include "ardour/audioengine.h"
#include "ardour/pannable.h"
#include "ardour/panner_shell.h"
#include "ardour/panner_manager.h"
#include "ardour/profile.h"
#include "ardour/route.h"
#include "ardour/send.h"
#include "ardour/session.h"
#include "ardour/user_bundle.h"
#include "ardour/value_as_string.h"

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/menu_elems.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/doi.h"

#include "widgets/tooltips.h"

#include "ardour_window.h"
#include "enums_convert.h"
#include "foldback_strip.h"
#include "mixer_ui.h"
#include "keyboard.h"
#include "public_editor.h"
#include "send_ui.h"
#include "timers.h"
#include "io_selector.h"
#include "utils.h"
#include "gui_thread.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ArdourWidgets;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace std;

#define PX_SCALE(px) std::max((float)px, rintf((float)px * UIConfiguration::instance().get_ui_scale()))

FoldbackSend::FoldbackSend (boost::shared_ptr<Send> snd, \
	boost::shared_ptr<ARDOUR::Route> sr,  boost::shared_ptr<ARDOUR::Route> fr, uint32_t wd)
	: _button (ArdourButton::led_default_elements)
	, _send (snd)
	, _send_route (sr)
	, _foldback_route (fr)
	, _send_proc (snd)
	, _send_del (snd)
	, _width (wd)
	, pan_control (ArdourKnob::default_elements, ArdourKnob::Flags (ArdourKnob::Detent | ArdourKnob::ArcToZero))
	, _adjustment (gain_to_slider_position_with_max (1.0, Config->get_max_gain()), 0, 1, 0.01, 0.1)
	, _slider (&_adjustment, boost::shared_ptr<PBD::Controllable>(), 0, max(13.f, rintf(13.f * UIConfiguration::instance().get_ui_scale())))
	, _ignore_ui_adjustment (true)
	, _slider_persistant_tooltip (&_slider)

{

	HBox * snd_but_pan = new HBox ();

	_button.set_distinct_led_click (true);
	_button.set_fallthrough_to_parent(true);
	_button.set_led_left (true);
	_button.signal_led_clicked.connect (sigc::mem_fun (*this, &FoldbackSend::led_clicked));
	_button.set_name ("processor prefader");
	_button.set_layout_ellipsize_width (PX_SCALE(_width) * PANGO_SCALE);
	_button.set_text_ellipsize (Pango::ELLIPSIZE_END);
	name_changed ();
	snd_but_pan->pack_start (_button, true, true);
	_button.set_active (_send_proc->enabled ());
	_button.show ();

	if (_foldback_route->input()->n_ports().n_audio() == 2) {
		_button.set_layout_ellipsize_width (PX_SCALE(_width - 19) * PANGO_SCALE);
		boost::shared_ptr<Pannable> pannable = _send_del->panner()->pannable();
		boost::shared_ptr<AutomationControl> ac;
		ac = pannable->pan_azimuth_control;
		pan_control.set_size_request (PX_SCALE(19), PX_SCALE(19));
		pan_control.set_tooltip_prefix (_("Pan: "));
		pan_control.set_name ("trim knob");
		pan_control.set_no_show_all (true);
		snd_but_pan->pack_start (pan_control, false, false);
		pan_control.show ();
		pan_control.set_controllable (ac);
	}
	boost::shared_ptr<AutomationControl> lc;
	lc = _send->gain_control();
	_slider.set_controllable (lc);
	_slider.set_name ("ProcessorControlSlider");
	_slider.set_text (_("Level"));

	pack_start (*snd_but_pan, Gtk::PACK_SHRINK);
	snd_but_pan->show();
	pack_start (_slider, true, true);
	_slider.show ();
	level_changed ();

	_adjustment.signal_value_changed().connect (sigc::mem_fun (*this,  &FoldbackSend::level_adjusted));
	lc->Changed.connect (_connections, invalidator (*this), boost::bind (&FoldbackSend::level_changed, this), gui_context ());
	_send_proc->ActiveChanged.connect (_connections, invalidator (*this), boost::bind (&FoldbackSend::send_state_changed, this), gui_context ());
	_button.signal_button_press_event().connect (sigc::mem_fun (*this, &FoldbackSend::button_press));
	_send_route->PropertyChanged.connect (_connections, invalidator (*this), boost::bind (&FoldbackSend::route_property_changed, this, _1), gui_context());

	show ();


}

FoldbackSend::~FoldbackSend ()
{
	_connections.drop_connections();
	_slider.set_controllable (boost::shared_ptr<AutomationControl> ());
	pan_control.set_controllable (boost::shared_ptr<AutomationControl> ());
	_send = boost::shared_ptr<Send> ();
	_send_route = boost::shared_ptr<Route> ();
	_foldback_route = boost::shared_ptr<Route> ();
	_send_proc = boost::shared_ptr<Processor> ();
	_send_del = boost::shared_ptr<Delivery> ();

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

	ArdourWidgets::set_tooltip (_button, Gtkmm2ext::markup_escape_text(_send_route->name()));
}

void
FoldbackSend::led_clicked(GdkEventButton *ev)
{
	if (_send_proc) {
		if (_button.get_active ()) {
			_send_proc->enable (false);

		} else {
			_send_proc->enable (true);
		}
	}
}

gboolean
FoldbackSend::button_press (GdkEventButton* ev)
{
	if (ev->button == 1) {
		Menu* menu = build_send_menu ();

		Gtkmm2ext::anchored_menu_popup(menu, &_button, "", 1, ev->time);
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
	boost::shared_ptr<AutomationControl> lc = _send->gain_control();

	if (!lc) {
		return;
	}

	lc->set_value ( lc->interface_to_internal(_adjustment.get_value ()) , Controllable::NoGroup);
	set_tooltip ();
}

void
FoldbackSend::level_changed ()
{
	boost::shared_ptr<AutomationControl> lc = _send->gain_control();
	if (!lc) {
		return;
	}

	_ignore_ui_adjustment = true;

	const double nval = lc->internal_to_interface (lc->get_value ());
	if (_adjustment.get_value() != nval) {
		_adjustment.set_value (nval);
		set_tooltip ();
	}

	_ignore_ui_adjustment = false;
}

void
FoldbackSend::set_tooltip ()
{
	boost::shared_ptr<AutomationControl> lc = _send->gain_control();

	if (!lc) {
		return;
	}
	std::string tt = ARDOUR::value_as_string (lc->desc(), lc->get_value ());
	string sm = Gtkmm2ext::markup_escape_text (tt);
	_slider_persistant_tooltip.set_tip (sm);
}

Menu*
FoldbackSend::build_send_menu ()
{
	using namespace Menu_Helpers;

	if (!_send) {
		return NULL;
	}

	Menu* menu = manage (new Menu);
	MenuList& items = menu->items ();
	menu->set_name ("ArdourContextMenu");

	items.push_back (
		MenuElem(_("Copy track/bus gain to send"), sigc::bind (sigc::mem_fun (*this, &FoldbackSend::set_gain), -0.1))
		);
	items.push_back (
		MenuElem(_("Set send gain to -inf"), sigc::bind (sigc::mem_fun (*this, &FoldbackSend::set_gain), 0.0))
		);
	items.push_back (
		MenuElem(_("Set send gain to 0dB"), sigc::bind (sigc::mem_fun (*this, &FoldbackSend::set_gain), 1.0))
		);
	items.push_back (MenuElem(_("Remove This Send"), sigc::mem_fun (*this, &FoldbackSend::remove_me)));

	return menu;

}

void
FoldbackSend::set_gain (float new_gain)
{
	if (new_gain < 0) {
		// get level from sending route
		new_gain = _send_route->gain_control ()->get_value ();
	}
	boost::shared_ptr<AutomationControl> lc = _send->gain_control();

	if (!lc) {
		return;
	}
	lc->set_value (new_gain, Controllable::NoGroup);

}

void
FoldbackSend::remove_me ()
{
	boost::shared_ptr<Processor> send_proc = boost::dynamic_pointer_cast<Processor> (_send);
	_connections.drop_connections();
	_send_route->remove_processor (send_proc);

}


FoldbackStrip* FoldbackStrip::_entered_foldback_strip;
PBD::Signal1<void,FoldbackStrip*> FoldbackStrip::CatchDeletion;

FoldbackStrip::FoldbackStrip (Mixer_UI& mx, Session* sess, boost::shared_ptr<Route> rt)
	: SessionHandlePtr (sess)
	, RouteUI (sess)
	, _mixer(mx)
	, _mixer_owned (true)
	, _width (80)
	, _pr_selection ()
	, panners (sess)
	, mute_solo_table (1, 2)
	, _plugin_insert_cnt (0)
	, _comment_button (_("Comments"))
	, fb_level_control (0)
{
	_session = sess;
	init ();
	set_route (rt);
}

void
FoldbackStrip::init ()
{
	_entered_foldback_strip= 0;
	ignore_comment_edit = false;
	ignore_toggle = false;
	comment_area = 0;

	_previous_button.set_name ("mixer strip button");
	_previous_button.set_icon (ArdourIcon::ScrollLeft);
	_previous_button.set_tweaks (ArdourButton::Square);
	UI::instance()->set_tip (&_previous_button, _("Previous foldback bus"), "");
	_previous_button.set_sensitive (false);

	_next_button.set_name ("mixer strip button");
	_next_button.set_icon (ArdourIcon::ScrollRight);
	_next_button.set_tweaks (ArdourButton::Square);
	UI::instance()->set_tip (&_next_button, _("Next foldback bus"), "");
	_next_button.set_sensitive (false);

	_hide_button.set_name ("mixer strip button");
	_hide_button.set_icon (ArdourIcon::HideEye);
	_hide_button.set_tweaks (ArdourButton::Square);
	set_tooltip (&_hide_button, _("Hide Foldback strip"));

	prev_next_box.pack_start (_previous_button, false, true);
	prev_next_box.pack_start (_next_button, false, true);
	prev_next_box.pack_end (_hide_button, false, true);

	name_button.set_name ("mixer strip button");
	name_button.set_text_ellipsize (Pango::ELLIPSIZE_END);
	name_button.set_layout_ellipsize_width (PX_SCALE(_width) * PANGO_SCALE);

	// invertbuttons and box in route_ui

	_show_sends_button.set_name ("send alert button");
	_show_sends_button.set_text (_("Show Sends"));
	UI::instance()->set_tip (&_show_sends_button, _("make mixer strips show sends to this bus"), "");

	send_display.set_flags (CAN_FOCUS);
	send_display.set_spacing (4);

	send_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
	send_scroller.add (send_display);
	send_scroller.get_child()->set_name ("FoldbackBusStripBase");

	// panners from route_ui
	panners.set_width (Wide);

	insert_box = new ProcessorBox (0, boost::bind (&FoldbackStrip::plugin_selector, this), _pr_selection, 0);
	insert_box->set_no_show_all ();
	insert_box->show ();
	insert_box->set_session (_session);
	insert_box->set_width (Wide);
	insert_box->set_size_request (PX_SCALE(_width + 34), PX_SCALE(100));

	mute_solo_table.set_homogeneous (true);
	mute_solo_table.set_spacings (2);
	solo_button->set_text (_("Listen"));
	mute_solo_table.attach (*solo_button, 0, 2, 0, 1);
	mute_solo_table.set_size_request (PX_SCALE(_width + 34), PX_SCALE(20));

	fb_level_control = new ArdourKnob (ArdourKnob::default_elements, ArdourKnob::Detent);
	fb_level_control->set_size_request (PX_SCALE(50), PX_SCALE(50));
	fb_level_control->set_tooltip_prefix (_("Level: "));
	fb_level_control->set_name ("foldback knob");
	fb_level_control->set_no_show_all (true);

	VBox* level_box = manage (new VBox);
	level_box->pack_start (*fb_level_control, true, false);
	master_box.pack_start (*level_box, true, false);
	master_box.set_size_request (PX_SCALE(_width + 34), PX_SCALE(80));
	master_box.set_name ("FoldbackBusStripBase");
	level_box->show ();

	output_button.set_text (_("Output"));
	output_button.set_name ("mixer strip button");
	output_button.set_text_ellipsize (Pango::ELLIPSIZE_MIDDLE);
	output_button.set_layout_ellipsize_width (PX_SCALE(_width) * PANGO_SCALE);

	_comment_button.set_name (X_("mixer strip button"));
	_comment_button.set_text_ellipsize (Pango::ELLIPSIZE_END);
	_comment_button.set_layout_ellipsize_width (PX_SCALE(_width) * PANGO_SCALE);

	global_vpacker.set_border_width (1);
	global_vpacker.set_spacing (2);

	// Packing is from top down to the send box. Thje send box
	// needs the most room and takes all left over space
	// Everything below the send box is packed from the bottom up
	// the panner is the last thing to pack as it doesn't always show
	// and packing it below the sendbox means nothing moves when it shows
	// or hides.
	global_vpacker.pack_start (prev_next_box, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (name_button, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (_invert_button_box, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (_show_sends_button, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (send_scroller, true, true);
#ifndef MIXBUS
	//add a spacer underneath the foldback bus;
	//this fills the area that is taken up by the scrollbar on the tracks;
	//and therefore keeps the strip boxes "even" across the bottom
	int scrollbar_height = 0;
	{
		Gtk::Window window (WINDOW_TOPLEVEL);
		HScrollbar scrollbar;
		window.add (scrollbar);
		scrollbar.set_name ("MixerWindow");
		scrollbar.ensure_style();
		Gtk::Requisition requisition(scrollbar.size_request ());
		scrollbar_height = requisition.height;
	}
	spacer.set_size_request (-1, scrollbar_height);
	global_vpacker.pack_end (spacer, false, false);
#endif
	global_vpacker.pack_end (_comment_button, Gtk::PACK_SHRINK);
	global_vpacker.pack_end (output_button, Gtk::PACK_SHRINK);
	global_vpacker.pack_end (master_box, Gtk::PACK_SHRINK);
	global_vpacker.pack_end (mute_solo_table, Gtk::PACK_SHRINK);
	global_vpacker.pack_end (*insert_box, Gtk::PACK_SHRINK);
	global_vpacker.pack_end (panners, Gtk::PACK_SHRINK);

	global_frame.add (global_vpacker);
	global_frame.set_shadow_type (Gtk::SHADOW_IN);
	global_frame.set_name ("BaseFrame");

	add (global_frame);

	/* force setting of visible selected status */

	_selected = true;
	set_selected (false);
	_packed = false;
	_embedded = false;

	_session->engine().Stopped.connect (*this, invalidator (*this), boost::bind (&FoldbackStrip::engine_stopped, this), gui_context());
	_session->engine().Running.connect (*this, invalidator (*this), boost::bind (&FoldbackStrip::engine_running, this), gui_context());

	output_button.signal_button_press_event().connect (sigc::mem_fun(*this, &FoldbackStrip::output_press), false);
	output_button.signal_button_release_event().connect (sigc::mem_fun(*this, &FoldbackStrip::output_release), false);

	name_button.signal_button_press_event().connect (sigc::mem_fun(*this, &FoldbackStrip::name_button_button_press), false);
	_previous_button.signal_clicked.connect (sigc::mem_fun (*this, &FoldbackStrip::previous_button_clicked));
	_next_button.signal_clicked.connect (sigc::mem_fun (*this, &FoldbackStrip::next_button_clicked));
	_hide_button.signal_clicked.connect (sigc::mem_fun(*this, &FoldbackStrip::hide_clicked));
	_show_sends_button.signal_clicked.connect (sigc::mem_fun(*this, &FoldbackStrip::show_sends_clicked));
	send_scroller.signal_button_press_event().connect (sigc::mem_fun (*this, &FoldbackStrip::send_button_press_event));
	_comment_button.signal_clicked.connect (sigc::mem_fun (*this, &RouteUI::toggle_comment_editor));

	add_events (Gdk::BUTTON_RELEASE_MASK|
		    Gdk::ENTER_NOTIFY_MASK|
		    Gdk::LEAVE_NOTIFY_MASK|
		    Gdk::KEY_PRESS_MASK|
		    Gdk::KEY_RELEASE_MASK);

	set_flags (get_flags() | Gtk::CAN_FOCUS);

	AudioEngine::instance()->PortConnectedOrDisconnected.connect (
		*this, invalidator (*this), boost::bind (&FoldbackStrip::port_connected_or_disconnected, this, _1, _3), gui_context ()
		);

	//watch for mouse enter/exit so we can do some stuff
	signal_enter_notify_event().connect (sigc::mem_fun(*this, &FoldbackStrip::mixer_strip_enter_event ));
	signal_leave_notify_event().connect (sigc::mem_fun(*this, &FoldbackStrip::mixer_strip_leave_event ));

}

FoldbackStrip::~FoldbackStrip ()
{
	CatchDeletion (this);
	delete fb_level_control;
	fb_level_control = 0;
	_connections.drop_connections();
	clear_send_box ();
	send_blink_connection.disconnect ();

	if (this ==_entered_foldback_strip)
		_entered_foldback_strip = NULL;
}

bool
FoldbackStrip::mixer_strip_enter_event (GdkEventCrossing* /*ev*/)
{
	_entered_foldback_strip = this;

	//although we are triggering on the "enter", to the user it will appear that it is happenin on the "leave"
	//because the FoldbackStrip control is a parent that encompasses the strip
	deselect_all_processors();

	return false;
}

bool
FoldbackStrip::mixer_strip_leave_event (GdkEventCrossing *ev)
{
	//if we have moved outside our strip, but not into a child view, then deselect ourselves
	if ( !(ev->detail == GDK_NOTIFY_INFERIOR) ) {
		_entered_foldback_strip= 0;

	}

	return false;
}

string
FoldbackStrip::name() const
{
	if (_route) {
		return _route->name();
	}
	return string();
}

void
FoldbackStrip::update_fb_level_control ()
{
	fb_level_control->show ();
	fb_level_control->set_controllable (_route->gain_control());
}

void
FoldbackStrip::set_route (boost::shared_ptr<Route> rt)
{
	/// FIX NO route
	if (!rt) {
		clear_send_box ();
		RouteUI::self_delete ();

		return;
	}
	if (_route) {
		_route->solo_control()->set_value (0.0, Controllable::NoGroup);
	}

	RouteUI::set_route (rt);

	insert_box->set_route (_route);
	revert_to_default_display ();
	update_fb_level_control();

	BusSendDisplayChanged (boost::shared_ptr<Route> ());
	_showing_sends = false;
	_show_sends_button.set_active (false);
	send_blink_connection.disconnect ();

	if (_route->panner_shell()) {
		update_panner_choices();
		_route->panner_shell()->Changed.connect (route_connections, invalidator (*this), boost::bind (&FoldbackStrip::connect_to_pan, this), gui_context());
	}

	_route->output()->changed.connect (*this, invalidator (*this), boost::bind (&FoldbackStrip::update_output_display, this), gui_context());
	_route->io_changed.connect (route_connections, invalidator (*this), boost::bind (&FoldbackStrip::io_changed_proxy, this), gui_context ());

	_route->comment_changed.connect (route_connections, invalidator (*this), boost::bind (&FoldbackStrip::setup_comment_button, this), gui_context());

	/* now force an update of all the various elements */

	name_changed ();
	update_send_box ();
	_session->FBSendsChanged.connect (route_connections, invalidator (*this), boost::bind (&FoldbackStrip::update_send_box, this), gui_context());
	comment_changed ();
	connect_to_pan ();
	panners.setup_pan ();
	panners.show_all ();
	update_output_display ();

	add_events (Gdk::BUTTON_RELEASE_MASK);
	prev_next_changed ();
	_previous_button.show();
	_next_button.show();
	_hide_button.show();
	prev_next_box.show ();
	name_button.show();
	send_display.show ();
	send_scroller.show ();
	_show_sends_button.show();
	insert_box->show ();
	solo_button->show ();
	mute_solo_table.show();
	master_box.show();
	output_button.show();
	_comment_button.show();
	spacer.show();
	global_frame.show();
	global_vpacker.show();

	map_frozen();

	show ();
	set_button_names ();
}

// predicate for sort call in get_sorted_stripables
struct StripableByPresentationOrder
{
	bool operator () (const boost::shared_ptr<Stripable> & a, const boost::shared_ptr<Stripable> & b) const
	{
		return a->presentation_info().order() < b->presentation_info().order();
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

	Route::FedBy fed_by = _route->fed_by();
	for (Route::FedBy::iterator i = fed_by.begin(); i != fed_by.end(); ++i) {
		if (i->sends_only) {
			boost::shared_ptr<Route> rt (i->r.lock());
			boost::shared_ptr<Stripable> s = boost::dynamic_pointer_cast<Stripable> (rt);
			stripables.push_back (s);
		}
	}
	stripables.sort (StripableByPresentationOrder());
	for (StripableList::iterator it = stripables.begin(); it != stripables.end(); ++it) {

		boost::shared_ptr<Stripable> s_sp = *it;
		boost::shared_ptr<Route> s_rt = boost::dynamic_pointer_cast<Route> (s_sp);
		boost::shared_ptr<Send> snd = s_rt->internal_send_for (_route);
		if (snd) {
			FoldbackSend * fb_s = new FoldbackSend (snd, s_rt, _route, _width);
			send_display.pack_start (*fb_s, Gtk::PACK_SHRINK);
			fb_s->show ();
			s_rt->processors_changed.connect (_connections, invalidator (*this), boost::bind (&FoldbackStrip::processors_changed, this, _1), gui_context ());
		}
	}
}

void
FoldbackStrip::clear_send_box ()
{
	std::vector< Widget* > snd_list = send_display.get_children ();
	_connections.drop_connections ();
	for (uint32_t i = 0; i < snd_list.size(); i++) {
		send_display.remove (*(snd_list[i]));
		delete snd_list[i];
	}
	snd_list.clear();
}

void
FoldbackStrip::processors_changed (RouteProcessorChange)
{
	update_send_box ();
}

void
FoldbackStrip::set_packed (bool yn)
{
	_packed = yn;
}

gint
FoldbackStrip::output_release (GdkEventButton *ev)
{
	switch (ev->button) {
	case 3:
		edit_output_configuration ();
		break;
	}

	return false;
}

gint
FoldbackStrip::output_press (GdkEventButton *ev)
{
	using namespace Menu_Helpers;
	if (!ARDOUR_UI_UTILS::engine_is_running ()) {
		return true;
	}

	MenuList& citems = output_menu.items();
	switch (ev->button) {

	case 3:
		return false;  //wait for the mouse-up to pop the dialog

	case 1:
	{
		output_menu.set_name ("ArdourContextMenu");
		citems.clear ();
		output_menu_bundles.clear ();

		citems.push_back (MenuElem (_("Disconnect"), sigc::mem_fun (*(static_cast<RouteUI*>(this)), &RouteUI::disconnect_output)));

		citems.push_back (SeparatorElem());
		uint32_t const n_with_separator = citems.size ();

		ARDOUR::BundleList current = _route->output()->bundles_connected ();

		boost::shared_ptr<ARDOUR::BundleList> b = _session->bundles ();

		DataType intended_type = DataType::AUDIO;

		/* then try adding user bundles, often labeled/grouped physical inputs */
		for (ARDOUR::BundleList::iterator i = b->begin(); i != b->end(); ++i) {
			if (boost::dynamic_pointer_cast<UserBundle> (*i)) {
				maybe_add_bundle_to_output_menu (*i, current, intended_type);
			}
		}

		/* then all other bundles, including physical outs or other sofware */
		for (ARDOUR::BundleList::iterator i = b->begin(); i != b->end(); ++i) {
			if (boost::dynamic_pointer_cast<UserBundle> (*i) == 0) {
				maybe_add_bundle_to_output_menu (*i, current, intended_type);
			}
		}

		if (citems.size() == n_with_separator) {
			/* no routes added; remove the separator */
			citems.pop_back ();
		}

		citems.push_back (SeparatorElem());
		citems.push_back (MenuElem (_("Routing Grid"), sigc::mem_fun (*(static_cast<RouteUI*>(this)), &RouteUI::edit_output_configuration)));

		Gtkmm2ext::anchored_menu_popup(&output_menu, &output_button, "",
		                               1, ev->time);

		break;
	}

	default:
		break;
	}
	return TRUE;
}

void
FoldbackStrip::bundle_output_chosen (boost::shared_ptr<ARDOUR::Bundle> c)
{
	if (ignore_toggle) {
		return;
	}

	_route->output()->connect_ports_to_bundle (c, true, true, this);
}

void
FoldbackStrip::maybe_add_bundle_to_output_menu (boost::shared_ptr<Bundle> b, ARDOUR::BundleList const& /*current*/,
                                             DataType type)
{
	using namespace Menu_Helpers;

	/* The bundle should be an input one, but not ours */
	if (b->ports_are_inputs() == false || *b == *_route->input()->bundle()) {
		return;
	}

	/* Don't add the monitor input */
	boost::shared_ptr<Route> monitor = _session->monitor_out();
	if (monitor && b->has_same_ports (monitor->input()->bundle()))
		return;

	/* It should have the same number of |type| channels as our outputs. */
	if (b->nchannels().n(type) != _route->n_outputs().n(type)) {
		return;
	}

	/* Avoid adding duplicates */
	list<boost::shared_ptr<Bundle> >::iterator i = output_menu_bundles.begin ();
	while (i != output_menu_bundles.end() && b->has_same_ports (*i) == false) {
		++i;
	}
	if (i != output_menu_bundles.end()) {
		return;
	}

	/* Now add the bundle to the menu */
	output_menu_bundles.push_back (b);

	MenuList& citems = output_menu.items();
	citems.push_back (MenuElemNoMnemonic (b->name (), sigc::bind (sigc::mem_fun(*this, &FoldbackStrip::bundle_output_chosen), b)));
}

void
FoldbackStrip::connect_to_pan ()
{
	ENSURE_GUI_THREAD (*this, &FoldbackStrip::connect_to_pan)

	panstate_connection.disconnect ();
	panstyle_connection.disconnect ();

	if (!_route->panner()) {
		return;
	}

	boost::shared_ptr<Pannable> p = _route->pannable ();

	update_panner_choices();
}

void
FoldbackStrip::update_panner_choices ()
{
	ENSURE_GUI_THREAD (*this, &FoldbackStrip::update_panner_choices)
	if (!_route->panner_shell()) { return; }

	uint32_t in = _route->output()->n_ports().n_audio();
	uint32_t out = in;
	if (_route->panner()) {
		in = _route->panner()->in().n_audio();
	}

	panners.set_available_panners(PannerManager::instance().PannerManager::get_available_panners(in, out));
}

/*
 * Output port labelling
 *
 * Case 1: Each output has one connection, all connections are to system:playback_%i
 *   out 1 -> system:playback_1
 *   out 2 -> system:playback_2
 *   out 3 -> system:playback_3
 *   Display as: 1/2/3
 *
 * Case 2: Each output has one connection, all connections are to ardour:track_x/in 1
 *   out 1 -> ardour:track_x/in 1
 *   out 2 -> ardour:track_x/in 2
 *   Display as: track_x
 *
 * Case 3: Each output has one connection, all connections are to Jack client "program x"
 *   out 1 -> program x:foo
 *   out 2 -> program x:foo
 *   Display as: program x
 *
 * Case 4: No connections (Disconnected)
 *   Display as: -
 *
 * Default case (unusual routing):
 *   Display as: *number of connections*
 *
 *
 * Tooltips
 *
 * .-----------------------------------------------.
 * | Mixdown                                       |
 * | out 1 -> ardour:master/in 1, jamin:input/in 1 |
 * | out 2 -> ardour:master/in 2, jamin:input/in 2 |
 * '-----------------------------------------------'
 * .-----------------------------------------------.
 * | Guitar SM58                                   |
 * | Disconnected                                  |
 * '-----------------------------------------------'
 */

void
FoldbackStrip::update_io_button ()
{
	ostringstream tooltip;
	ostringstream label;
	bool have_label = false;

	uint32_t total_connection_count = 0;
	uint32_t typed_connection_count = 0;
	bool each_typed_port_has_one_connection = true;

	DataType dt = DataType::AUDIO;
	boost::shared_ptr<IO> io = _route->output();

	/* Fill in the tooltip. Also count:
	 *  - The total number of connections.
	 *  - The number of main-typed connections.
	 *  - Whether each main-typed port has exactly one connection. */

	tooltip << string_compose (_("<b>OUTPUT</b> from %1"),
			Gtkmm2ext::markup_escape_text (_route->name()));

	string arrow = Gtkmm2ext::markup_escape_text(" -> ");
	vector<string> port_connections;
	for (PortSet::iterator port = io->ports().begin();
	                       port != io->ports().end();
	                       ++port) {
		port_connections.clear();
		port->get_connections(port_connections);

		uint32_t port_connection_count = 0;

		for (vector<string>::iterator i = port_connections.begin();
		                              i != port_connections.end();
		                              ++i) {
			++port_connection_count;

			if (port_connection_count == 1) {
				tooltip << endl << Gtkmm2ext::markup_escape_text (
						port->name().substr(port->name().find("/") + 1));
				tooltip << arrow;
			} else {
				tooltip << ", ";
			}

			tooltip << Gtkmm2ext::markup_escape_text(*i);
		}

		total_connection_count += port_connection_count;
		if (port->type() == dt) {
			typed_connection_count += port_connection_count;
			each_typed_port_has_one_connection &= (port_connection_count == 1);
		}

	}

	if (total_connection_count == 0) {
		tooltip << endl << _("Disconnected");
	}

	if (typed_connection_count == 0) {
		label << "-";
		have_label = true;
	}

	/* Are all main-typed channels connected to the same route ? */
	if (!have_label) {
		boost::shared_ptr<ARDOUR::RouteList> routes = _session->get_routes ();
		for (ARDOUR::RouteList::const_iterator route = routes->begin();
		                                       route != routes->end();
		                                       ++route) {
			boost::shared_ptr<IO> dest_io = (*route)->output();
			if (io->bundle()->connected_to(dest_io->bundle(),
			                               _session->engine(),
			                               dt, true)) {
				label << Gtkmm2ext::markup_escape_text ((*route)->name());
				have_label = true;
				break;
			}
		}
	}

	/* Are all main-typed channels connected to the same (user) bundle ? */
	if (!have_label) {
		boost::shared_ptr<ARDOUR::BundleList> bundles = _session->bundles ();
		for (ARDOUR::BundleList::iterator bundle = bundles->begin();
		                                  bundle != bundles->end();
		                                  ++bundle) {
			if (boost::dynamic_pointer_cast<UserBundle> (*bundle) == 0)
				continue;
			if (io->bundle()->connected_to(*bundle, _session->engine(),
			                               dt, true)) {
				label << Gtkmm2ext::markup_escape_text ((*bundle)->name());
				have_label = true;
				break;
			}
		}
	}

	/* Is each main-typed channel only connected to a physical output ? */
	if (!have_label && each_typed_port_has_one_connection) {
		ostringstream temp_label;
		vector<string> phys;
		string playorcapture;

		_session->engine().get_physical_outputs(dt, phys);
		playorcapture = "playback_";
		for (PortSet::iterator port = io->ports().begin(dt);
		                       port != io->ports().end(dt);
		                       ++port) {
			string pn = "";
			for (vector<string>::iterator s = phys.begin();
			                              s != phys.end();
			                              ++s) {
				if (!port->connected_to(*s))
					continue;
				pn = AudioEngine::instance()->get_pretty_name_by_name(*s);
				if (pn.empty()) {
					string::size_type start = (*s).find(playorcapture);
					if (start != string::npos) {
						pn = (*s).substr(start + playorcapture.size());
					}
				}
				break;
			}
			if (pn.empty()) {
				temp_label.str(""); /* erase the failed attempt */
				break;
			}
			if (port != io->ports().begin(dt))
				temp_label << "/";
			temp_label << pn;
		}

		if (!temp_label.str().empty()) {
			label << temp_label.str();
			have_label = true;
		}
	}

	/* Is each main-typed channel connected to a single and different port with
	 * the same client name (e.g. another JACK client) ? */
	if (!have_label && each_typed_port_has_one_connection) {
		string maybe_client = "";
		vector<string> connections;
		for (PortSet::iterator port = io->ports().begin(dt);
		                       port != io->ports().end(dt);
		                       ++port) {
			port_connections.clear();
			port->get_connections(port_connections);
			string connection = port_connections.front();

			vector<string>::iterator i = connections.begin();
			while (i != connections.end() && *i != connection) {
				++i;
			}
			if (i != connections.end())
				break; /* duplicate connection */
			connections.push_back(connection);

			connection = connection.substr(0, connection.find(":"));
			if (maybe_client.empty())
				maybe_client = connection;
			if (maybe_client != connection)
				break;
		}
		if (connections.size() == io->n_ports().n(dt)) {
			label << maybe_client;
			have_label = true;
		}
	}

	/* Odd configuration */
	if (!have_label) {
		label << "*" << total_connection_count << "*";
	}

	if (total_connection_count > typed_connection_count) {
		label << "\u2295"; /* circled plus */
	}

	/* Actually set the properties of the button */
	char * cstr = new char[tooltip.str().size() + 1];
	strcpy(cstr, tooltip.str().c_str());

	output_button.set_text (label.str());
	set_tooltip (&output_button, cstr);

	delete [] cstr;
}

void
FoldbackStrip::update_output_display ()
{
	update_io_button ();
	panners.setup_pan ();

	if (has_audio_outputs ()) {
		panners.show_all ();
	} else {
		panners.hide_all ();
	}
}

void
FoldbackStrip::io_changed_proxy ()
{
	Glib::signal_idle().connect_once (sigc::mem_fun (*this, &FoldbackStrip::update_panner_choices));
}

void
FoldbackStrip::port_connected_or_disconnected (boost::weak_ptr<Port> wa, boost::weak_ptr<Port> wb)
{
	boost::shared_ptr<Port> a = wa.lock ();
	boost::shared_ptr<Port> b = wb.lock ();

	if ((a && _route->output()->has_port (a)) || (b && _route->output()->has_port (b))) {
		update_output_display ();
	}
}

void
FoldbackStrip::setup_comment_button ()
{
	std::string comment = _route->comment();

	set_tooltip (_comment_button, comment.empty() ? _("Click to add/edit comments") : _route->comment());

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
	if (comment.empty()) {
		_comment_button.set_text (_("Comments"));
	} else {
		_comment_button.set_text (comment);
	}
}

void
FoldbackStrip::help_count_plugins (boost::weak_ptr<Processor> p)
{
	boost::shared_ptr<Processor> processor (p.lock ());
	if (!processor || !processor->display_to_user()) {
		return;
	}
	boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (processor);
#ifdef MIXBUS
	if (pi && pi->is_channelstrip ()) {
		return;
	}
#endif
	if (pi) {
		++_plugin_insert_cnt;
	}
}

Gtk::Menu*
FoldbackStrip::build_route_ops_menu ()
{
	using namespace Menu_Helpers;

	Menu* menu = manage (new Menu);
	MenuList& items = menu->items ();
	menu->set_name ("ArdourContextMenu");

	items.push_back (MenuElem (_("Comments..."), sigc::mem_fun (*this, &RouteUI::open_comment_editor)));

	items.push_back (MenuElem (_("Outputs..."), sigc::mem_fun (*this, &RouteUI::edit_output_configuration)));

	items.push_back (SeparatorElem());

	items.push_back (MenuElem (_("Save As Template..."), sigc::mem_fun(*this, &RouteUI::save_as_template)));

	items.push_back (MenuElem (_("Rename..."), sigc::mem_fun(*this, &RouteUI::route_rename)));

	items.push_back (SeparatorElem());
	items.push_back (CheckMenuElem (_("Active")));
	Gtk::CheckMenuItem* i = dynamic_cast<Gtk::CheckMenuItem *> (&items.back());
	i->set_active (_route->active());
	i->set_sensitive(! _session->transport_rolling());
	i->signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &RouteUI::set_route_active), !_route->active(), false));

	items.push_back (SeparatorElem());
	items.push_back (CheckMenuElem (_("Protect Against Denormals"), sigc::mem_fun (*this, &RouteUI::toggle_denormal_protection)));
	denormal_menu_item = dynamic_cast<Gtk::CheckMenuItem *> (&items.back());
	denormal_menu_item->set_active (_route->denormal_protection());

	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Remove"), sigc::mem_fun(*this, &FoldbackStrip::remove_current_fb)));
	return menu;
}

Gtk::Menu*
FoldbackStrip::build_route_select_menu ()
{
	using namespace Menu_Helpers;

	Menu* menu = manage (new Menu);
	MenuList& items = menu->items ();
	menu->set_name ("ArdourContextMenu");

	StripableList fb_list;
	_session->get_stripables (fb_list, PresentationInfo::FoldbackBus);
	for (StripableList::iterator s = fb_list.begin(); s != fb_list.end(); ++s) {

		boost::shared_ptr<Route> route = boost::dynamic_pointer_cast<Route> ((*s));
		if (route == _route) {
			continue;
		}
		items.push_back (MenuElem (route->name (), sigc::bind (sigc::mem_fun (*this, &FoldbackStrip::set_route), route)));
	}
	return menu;
}


gboolean
FoldbackStrip::name_button_button_press (GdkEventButton* ev)
{
	if (ev->button == 1) {
		Menu* menu = build_route_select_menu ();

		Gtkmm2ext::anchored_menu_popup(menu, &name_button, "",
			                               1, ev->time);
		return true;
	} else if (ev->button == 3) {
		Menu* r_menu = build_route_ops_menu ();
		r_menu->popup (3, ev->time);
		return true;
	}
	return false;
}

void
FoldbackStrip::previous_button_clicked ()
{
	bool past_current = false;
	StripableList slist;
	boost::shared_ptr<Route> previous = boost::shared_ptr<Route> ();
	_session->get_stripables (slist, PresentationInfo::FoldbackBus);
	if (slist.size () > 1) {
		for (StripableList::iterator s = slist.begin(); s != slist.end(); ++s) {
			if ((*s) == _route) {
				past_current = true;
			}
			if (!past_current) {
				previous = boost::dynamic_pointer_cast<Route> (*s);
			}
		}
	} else {
		// only one route do nothing
		return;
	}
	//use previous to set route
	if (previous) {
		set_route (previous);
	}
}

void
FoldbackStrip::next_button_clicked ()
{
	bool past_current = false;
	StripableList slist;
	boost::shared_ptr<Route> next = boost::shared_ptr<Route> ();
	_session->get_stripables (slist, PresentationInfo::FoldbackBus);
	if (slist.size () > 1) {
		for (StripableList::iterator s = slist.begin(); s != slist.end(); ++s) {
			if (past_current) {
				next = boost::dynamic_pointer_cast<Route> (*s);
				break;
			}
			if ((*s) == _route) {
				past_current = true;
			}
		}
	} else {
		// only one route do nothing
		return;
	}
	//use next to set route
	if (next) {
		set_route (next);
	}
}

void
FoldbackStrip::prev_next_changed ()
{
	StripableList slist;
	_session->get_stripables (slist, PresentationInfo::FoldbackBus);
	if ((slist.size() < 2) || (boost::dynamic_pointer_cast<Stripable> (_route) == *(slist.begin()))) {
		_previous_button.set_sensitive (false);
	} else {
		_previous_button.set_sensitive (true);
	}
	if ((slist.size () < 2) || boost::dynamic_pointer_cast<Stripable> (_route) == *(--slist.end())) {
		_next_button.set_sensitive (false);
	} else {
		_next_button.set_sensitive (true);
	}
}

void
FoldbackStrip::hide_clicked()
{
	_hide_button.set_sensitive(false);
	ActionManager::get_toggle_action (X_("Mixer"), X_("ToggleFoldbackStrip"))->set_active (false);
	_hide_button.set_sensitive(true);
}

void
FoldbackStrip::show_sends_clicked ()
{
	if (_showing_sends) {
		BusSendDisplayChanged (boost::shared_ptr<Route> ()); /* EMIT SIGNAL */
		_showing_sends = false;
		_show_sends_button.set_active (false);
		send_blink_connection.disconnect ();
	} else {
		BusSendDisplayChanged (_route); /* EMIT SIGNAL */
		_showing_sends = true;
		_show_sends_button.set_active (true);
		send_blink_connection = Timers::blink_connect (sigc::mem_fun (*this, &FoldbackStrip::send_blink));
	}
}

void
FoldbackStrip::send_blink (bool onoff)
{
	if (!(&_show_sends_button)) {
		return;
	}

	if (onoff) {
		_show_sends_button.set_active_state (Gtkmm2ext::ExplicitActive);
	} else {
		_show_sends_button.unset_active_state ();
	}
}

void
FoldbackStrip::set_selected (bool yn)
{

	global_frame.set_shadow_type (Gtk::SHADOW_IN);
	global_frame.set_name ("MixerStripFrame");

	global_frame.queue_draw ();

}

void
FoldbackStrip::route_property_changed (const PropertyChange& what_changed)
{
	if (what_changed.contains (ARDOUR::Properties::name)) {
		name_changed ();
	}
}

void
FoldbackStrip::name_changed ()
{
	name_button.set_text (_route->name());

	set_tooltip (name_button, Gtkmm2ext::markup_escape_text(_route->name()));
}

void
FoldbackStrip::set_embedded (bool yn)
{
	_embedded = yn;
}

void
FoldbackStrip::map_frozen ()
{
	ENSURE_GUI_THREAD (*this, &FoldbackStrip::map_frozen)


	RouteUI::map_frozen ();
}

void
FoldbackStrip::hide_redirect_editors ()
{
	_route->foreach_processor (sigc::mem_fun (*this, &FoldbackStrip::hide_processor_editor));
}

void
FoldbackStrip::hide_processor_editor (boost::weak_ptr<Processor> p)
{
	boost::shared_ptr<Processor> processor (p.lock ());
	if (!processor) {
		return;
	}

	Gtk::Window* w = insert_box->get_processor_ui (processor);

	if (w) {
		w->hide ();
	}
}

void
FoldbackStrip::reset_strip_style ()
{
			if (_route->active()) {
				set_name ("FoldbackBusStripBase");
			} else {
				set_name ("AudioBusStripBaseInactive");
			}

}

void
FoldbackStrip::engine_stopped ()
{
}

void
FoldbackStrip::engine_running ()
{
}

void
FoldbackStrip::drop_send ()
{
	boost::shared_ptr<Send> current_send;

	if (_current_delivery && ((current_send = boost::dynamic_pointer_cast<Send>(_current_delivery)) != 0)) {
		current_send->set_metering (false);
	}

	send_gone_connection.disconnect ();
	output_button.set_sensitive (true);
	set_invert_sensitive (true);
	solo_button->set_sensitive (true);
	_comment_button.set_sensitive (true);
	fb_level_control->set_sensitive (true);
	set_button_names (); // update solo button visual state
}

void
FoldbackStrip::set_current_delivery (boost::shared_ptr<Delivery> d)
{
	_current_delivery = d;
	DeliveryChanged (_current_delivery);
}

void
FoldbackStrip::revert_to_default_display ()
{
	drop_send ();

	set_current_delivery (_route->main_outs ());

	panner_ui().set_panner (_route->main_outs()->panner_shell(), _route->main_outs()->panner());
	update_panner_choices();
	panner_ui().setup_pan ();
	panner_ui().set_send_drawing_mode (false);

	if (has_audio_outputs ()) {
		panners.show_all ();
	} else {
		panners.hide_all ();
	}

	reset_strip_style ();
}

void
FoldbackStrip::set_button_names ()
{

	if (!Config->get_solo_control_is_listen_control()) {
		solo_button->hide ();
	} else {
		solo_button->set_sensitive (true);
		solo_button->show ();
		UI::instance()->set_tip (solo_button, _("Listen on monitor"), "");
		switch (Config->get_listen_position()) {
		case AfterFaderListen:
			solo_button->set_text (_("Listen"));
			break;
		case PreFaderListen:
			solo_button->set_text (_("Listen"));
			break;
		}
	}
}

PluginSelector*
FoldbackStrip::plugin_selector()
{
	return _mixer.plugin_selector();
}

void
FoldbackStrip::route_active_changed ()
{
	reset_strip_style ();
}

void
FoldbackStrip::copy_processors ()
{
	insert_box->processor_operation (ProcessorBox::ProcessorsCopy);
}

void
FoldbackStrip::cut_processors ()
{
	insert_box->processor_operation (ProcessorBox::ProcessorsCut);
}

void
FoldbackStrip::paste_processors ()
{
	insert_box->processor_operation (ProcessorBox::ProcessorsPaste);
}

void
FoldbackStrip::select_all_processors ()
{
	insert_box->processor_operation (ProcessorBox::ProcessorsSelectAll);
}

void
FoldbackStrip::deselect_all_processors ()
{
	insert_box->processor_operation (ProcessorBox::ProcessorsSelectNone);
}

bool
FoldbackStrip::delete_processors ()
{
	return insert_box->processor_operation (ProcessorBox::ProcessorsDelete);
}

void
FoldbackStrip::toggle_processors ()
{
	insert_box->processor_operation (ProcessorBox::ProcessorsToggleActive);
}

void
FoldbackStrip::ab_plugins ()
{
	insert_box->processor_operation (ProcessorBox::ProcessorsAB);
}

void
FoldbackStrip::create_selected_sends (bool include_buses)
{
	boost::shared_ptr<StripableList> slist (new StripableList);
	PresentationInfo::Flag fl = PresentationInfo::AudioTrack;
	if (include_buses) {
		fl = PresentationInfo::MixerRoutes;
	}
	_session->get_stripables (*slist, fl);

	for (StripableList::iterator i = (*slist).begin(); i != (*slist).end(); ++i) {
		if ((*i)->is_selected() && !(*i)->is_master() && !(*i)->is_monitor()) {
			boost::shared_ptr<Route> rt = boost::dynamic_pointer_cast<Route>(*i);
			if (rt) {
				rt->add_foldback_send (_route);
			}
		}
	}

}

bool
FoldbackStrip::send_button_press_event (GdkEventButton *ev)
{
	if (ev->button == 3) {
		Menu* menu = build_sends_menu ();
		menu->popup (3, ev->time);
		return true;
	}
	return false;
}

Gtk::Menu*
FoldbackStrip::build_sends_menu ()
{
	using namespace Menu_Helpers;

	Menu* menu = manage (new Menu);
	MenuList& items = menu->items ();
	menu->set_name ("ArdourContextMenu");

	items.push_back (
		MenuElem(_("Assign selected tracks (prefader)"), sigc::bind (sigc::mem_fun (*this, &FoldbackStrip::create_selected_sends), false))
		);

	items.push_back (
		MenuElem(_("Assign selected tracks and buses (prefader)"), sigc::bind (sigc::mem_fun (*this, &FoldbackStrip::create_selected_sends), true)));

	items.push_back (MenuElem(_("Copy track/bus gains to sends"), sigc::mem_fun (*this, &RouteUI::set_sends_gain_from_track)));
	items.push_back (MenuElem(_("Set sends gain to -inf"), sigc::mem_fun (*this, &RouteUI::set_sends_gain_to_zero)));
	items.push_back (MenuElem(_("Set sends gain to 0dB"), sigc::mem_fun (*this, &RouteUI::set_sends_gain_to_unity)));

	return menu;
}

void
FoldbackStrip::remove_current_fb ()
{
	clear_send_box ();
	StripableList slist;
	boost::shared_ptr<Route> next = boost::shared_ptr<Route> ();
	boost::shared_ptr<Route> old_route = _route;
	_session->get_stripables (slist, PresentationInfo::FoldbackBus);
	if (slist.size ()) {
		for (StripableList::iterator s = slist.begin(); s != slist.end(); ++s) {
			if ((*s) != _route) {
				next = boost::dynamic_pointer_cast<Route> (*s);
				break;
			}
		}
	}
	if (next) {
		set_route (next);
		_session->remove_route (old_route);
		prev_next_changed ();
	} else {
		clear_send_box ();
		RouteUI::self_delete ();
		_session->remove_route (old_route);
	}
}
