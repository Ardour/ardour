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

#include <cmath>
#include <list>
#include <algorithm>

#include <sigc++/bind.h>

#include <gtkmm/messagedialog.h>

#include "pbd/convert.h"
#include "pbd/enumwriter.h"
#include "pbd/replace_all.h"
#include "pbd/stacktrace.h"

#include "ardour/amp.h"
#include "ardour/audioengine.h"
#include "ardour/internal_send.h"
#include "ardour/io.h"
#include "ardour/pannable.h"
#include "ardour/panner.h"
#include "ardour/panner_shell.h"
#include "ardour/panner_manager.h"
#include "ardour/port.h"
#include "ardour/profile.h"
#include "ardour/route.h"
#include "ardour/send.h"
#include "ardour/session.h"
#include "ardour/types.h"
#include "ardour/user_bundle.h"

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

FoldbackStrip* FoldbackStrip::_entered_foldback_strip;
PBD::Signal1<void,FoldbackStrip*> FoldbackStrip::CatchDeletion;

FoldbackStrip::FoldbackStrip (Mixer_UI& mx, Session* sess, boost::shared_ptr<Route> rt)
	: SessionHandlePtr (sess)
	, RouteUI (sess)
	, _mixer(mx)
	, _mixer_owned (true)
	, panners (sess)
	, button_size_group (Gtk::SizeGroup::create (Gtk::SIZE_GROUP_HORIZONTAL))
	, mute_solo_table (1, 2)
	, bottom_button_table (1, 1)
	, _comment_button (_("Comments"))
	, fb_level_control (0)
{
	init ();
	set_route (rt);
}

void
FoldbackStrip::init ()
{
	_entered_foldback_strip= 0;
	route_ops_menu = 0;
	route_select_menu = 0;
	ignore_comment_edit = false;
	ignore_toggle = false;
	comment_area = 0;
	_width_owner = 0;

	/* the length of this string determines the width of the mixer strip when it is set to `wide' */
	longest_label = "longest label";

	//input_button_box.set_spacing(2);


	output_button.set_text (_("Output"));
	output_button.set_name ("foldback strip button");
//	send_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
//	send_scroller.add (send_display);

	send_display.set_flags (CAN_FOCUS);
	send_display.set_name ("ProcessorList");
	send_display.set_data ("sendbox", this);
	send_display.set_size_request (48, -1);
	send_display.set_spacing (0);


//	send_scroller.show ();
	send_display.show ();

	// TODO implement ArdourKnob::on_size_request properly
	fb_level_control = new ArdourKnob (ArdourKnob::default_elements, ArdourKnob::Detent);

#define PX_SCALE(px) std::max((float)px, rintf((float)px * UIConfiguration::instance().get_ui_scale()))
	fb_level_control->set_size_request (PX_SCALE(65), PX_SCALE(65));
#undef PX_SCALE
	fb_level_control->set_tooltip_prefix (_("Level: "));
	fb_level_control->set_name ("monitor section knob");
	fb_level_control->set_no_show_all (true);

	bottom_button_table.attach (*fb_level_control, 0, 1, 0, 1,FILL,FILL,20,20); //EXPAND


	mute_solo_table.set_homogeneous (true);
	mute_solo_table.set_spacings (2);

	show_sends_button->set_text (_("Show Sends"));

	show_sends_box.pack_start (*show_sends_button, true, true);
	show_sends_button->show();

	bottom_button_table.set_spacings (20);
	bottom_button_table.set_row_spacings (20);
	bottom_button_table.set_homogeneous (true);

	name_button.set_name ("monitor section button");
	name_button.set_text_ellipsize (Pango::ELLIPSIZE_END);
	name_button.signal_size_allocate().connect (sigc::mem_fun (*this, &FoldbackStrip::name_button_resized));

	_select_button.set_name ("monitor section button");
	_select_button.set_text (_("Select Foldback Bus"));


	_comment_button.set_name (X_("foldback strip button"));
	_comment_button.set_text_ellipsize (Pango::ELLIPSIZE_END);
	_comment_button.signal_clicked.connect (sigc::mem_fun (*this, &RouteUI::toggle_comment_editor));
	_comment_button.signal_size_allocate().connect (sigc::mem_fun (*this, &FoldbackStrip::comment_button_resized));

	global_vpacker.set_border_width (1);

	global_vpacker.set_spacing (4);
	global_vpacker.pack_start (_select_button, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (name_button, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (_invert_button_box, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (show_sends_box, Gtk::PACK_SHRINK);

#ifndef MIXBUS
	//add a spacer underneath the master bus;
	//this fills the area that is taken up by the scrollbar on the tracks;
	//and therefore keeps the faders "even" across the bottom
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
	global_vpacker.pack_end (mute_solo_table, Gtk::PACK_SHRINK);
	global_vpacker.pack_end (panners, Gtk::PACK_SHRINK);
	global_vpacker.pack_end (bottom_button_table, Gtk::PACK_SHRINK);

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

	output_button.set_text_ellipsize (Pango::ELLIPSIZE_MIDDLE);

	output_button.signal_button_press_event().connect (sigc::mem_fun(*this, &FoldbackStrip::output_press), false);
	output_button.signal_button_release_event().connect (sigc::mem_fun(*this, &FoldbackStrip::output_release), false);
	output_button.signal_size_allocate().connect (sigc::mem_fun (*this, &FoldbackStrip::output_button_resized));


	name_button.signal_button_press_event().connect (sigc::mem_fun(*this, &FoldbackStrip::name_button_button_press), false);
	_select_button.signal_button_press_event().connect (sigc::mem_fun (*this, &FoldbackStrip::select_button_button_press), false);

	_width = Wide;


	/* start off as a passthru strip. we'll correct this, if necessary,
	   in update_diskstream_display().
	*/



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

	if (this ==_entered_foldback_strip)
		_entered_foldback_strip = NULL;
}

bool
FoldbackStrip::mixer_strip_enter_event (GdkEventCrossing* /*ev*/)
{
	_entered_foldback_strip = this;

	//although we are triggering on the "enter", to the user it will appear that it is happenin on the "leave"
	//because the FoldbackStrip control is a parent that encompasses the strip
	//deselect_all_processors();

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
		RouteUI::self_delete ();
/*	_route.reset (); // drop reference to route, so that it can be cleaned up
	route_connections.drop_connections ();

	delete solo_menu;
	delete mute_menu;
	delete sends_menu;
	delete record_menu;
	delete comment_window;
	delete input_selector;
	delete output_selector;
	delete monitor_input_button;
	delete monitor_disk_button;
	delete _invert_menu;

	send_blink_connection.disconnect ();
	rec_blink_connection.disconnect ();*/
		return;
	}
	//the rec/monitor stuff only shows up for tracks.
	//the show_sends only shows up for buses.
	//remove them all here, and we may add them back later
	if (show_sends_button->get_parent()) {
		show_sends_box.remove (*show_sends_button);
	}

	RouteUI::set_route (rt);

	revert_to_default_display ();

	if (solo_button->get_parent()) {
		mute_solo_table.remove (*solo_button);
	}

	if (mute_button->get_parent()) {
		mute_solo_table.remove (*mute_button);
	}

	mute_solo_table.attach (*mute_button, 0, 1, 0, 1);
	mute_solo_table.attach (*solo_button, 1, 2, 0, 1);
	mute_button->show ();
	solo_button->show ();
	show_sends_box.show ();

	spacer.show();

	update_fb_level_control();

	show_sends_box.pack_start (*show_sends_button, true, true);
	show_sends_button->show();


	delete route_ops_menu;
	route_ops_menu = 0;

	delete route_select_menu;
	route_select_menu = 0;

	_route->output()->changed.connect (*this, invalidator (*this), boost::bind (&FoldbackStrip::update_output_display, this), gui_context());

	_route->io_changed.connect (route_connections, invalidator (*this), boost::bind (&FoldbackStrip::io_changed_proxy, this), gui_context ());

	if (_route->panner_shell()) {
		update_panner_choices();
		_route->panner_shell()->Changed.connect (route_connections, invalidator (*this), boost::bind (&FoldbackStrip::connect_to_pan, this), gui_context());
	}

	_route->comment_changed.connect (route_connections, invalidator (*this), boost::bind (&FoldbackStrip::setup_comment_button, this), gui_context());

	set_stuff_from_route ();

	/* now force an update of all the various elements */

	update_mute_display ();
	update_solo_display ();
	name_changed ();
	comment_changed ();

	connect_to_pan ();
	panners.setup_pan ();
	panners.show_all ();

	update_output_display ();

	add_events (Gdk::BUTTON_RELEASE_MASK);

	Route::FedBy fed_by = _route->fed_by();
	for (Route::FedBy::iterator i = fed_by.begin(); i != fed_by.end(); ++i) {
		if (i->sends_only) {
			boost::shared_ptr<Route> s_rt (i->r.lock());
			boost::shared_ptr<Send> snd = s_rt->internal_send_for (_route);
			//s_rt->DropReferences.connect (*this, MISSING_INVALIDATOR, boost::bind (&FoldbackStrip::fill_sendbox, this), this);
			boost::shared_ptr<Processor> processor (snd);
			//boost::shared_ptr<PluginInsert> plugin_insert = boost::dynamic_pointer_cast<PluginInsert> (processor);
// Ok we have the info... now we need widgets to put it in... I suspect I need a class or three
// this doesn't work... and I need to clear it out and rebuild each time.
			ArdourWidgets::ArdourButton _button;
			_button.set_distinct_led_click (true);
			_button.set_fallthrough_to_parent(true);
			_button.set_led_left (true);
//			_button.signal_led_clicked.connect (sigc::mem_fun (*this, &FoldbackStrip::led_clicked));
			_button.set_text (s_rt->name ());

			//Gtk::VBox	snd_entry = NULL;
			Gtk::VBox	snd_entry;
			snd_entry.set_border_width (1);
			snd_entry.set_spacing (4);
			snd_entry.pack_start (_button, true, true);
			send_display.pack_start (snd_entry, true, true);
			_button.show ();
			snd_entry.show ();


		}
	}



	global_frame.show();
	global_vpacker.show();
	mute_solo_table.show();
	bottom_button_table.show();
	show_sends_box.show_all();
	//send_scroller.show ();
	send_display.show ();
	output_button.show();
	name_button.show();
	_select_button.show();
	_comment_button.show();

	map_frozen();

	show ();
}

void
FoldbackStrip::set_stuff_from_route ()
{
	/* if width is not set, it will be set by the MixerUI or editor */

	Width width;
	if (get_gui_property ("strip-width", width)) {
	}
}

void
FoldbackStrip::set_packed (bool yn)
{
	_packed = yn;
	set_gui_property ("visible", _packed);
}


struct RouteCompareByName {
	bool operator() (boost::shared_ptr<Route> a, boost::shared_ptr<Route> b) {
		return a->name().compare (b->name()) < 0;
	}
};

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
	if (!_session->engine().connected()) {
		MessageDialog msg (_("Not connected to audio engine - no I/O changes are possible"));
		msg.run ();
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

		/* guess the user-intended main type of the route output */
		DataType intended_type = guess_main_type(false);

		/* try adding the master bus first */
		boost::shared_ptr<Route> master = _session->master_out();
		if (master) {
			maybe_add_bundle_to_output_menu (master->input()->bundle(), current, intended_type);
		}

		/* then other routes inputs */
		RouteList copy = _session->get_routelist ();
		copy.sort (RouteCompareByName ());
		for (ARDOUR::RouteList::const_iterator i = copy.begin(); i != copy.end(); ++i) {
			maybe_add_bundle_to_output_menu ((*i)->input()->bundle(), current, intended_type);
		}

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

		if (!ARDOUR::Profile->get_mixbus()) {
			citems.push_back (SeparatorElem());

			for (DataType::iterator i = DataType::begin(); i != DataType::end(); ++i) {
				citems.push_back (
						MenuElem (
							string_compose (_("Add %1 port"), (*i).to_i18n_string()),
							sigc::bind (sigc::mem_fun (*this, &FoldbackStrip::add_output_port), *i)
							)
						);
			}
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

	/* Don't add the monitor input unless we are Master */
	boost::shared_ptr<Route> monitor = _session->monitor_out();
	if ((!_route->is_master()) && monitor && b->has_same_ports (monitor->input()->bundle()))
		return;

	/* It should either match exactly our outputs (if |type| is DataType::NIL)
	 * or have the same number of |type| channels than our outputs. */
	if (type == DataType::NIL) {
		if(b->nchannels() != _route->n_outputs())
			return;
	} else {
		if (b->nchannels().n(type) != _route->n_outputs().n(type))
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

	//p->automation_state_changed.connect (panstate_connection, invalidator (*this), boost::bind (&PannerUI::pan_automation_state_changed, &panners), gui_context());

	/* This call reduncant, PannerUI::set_panner() connects to _panshell->Changed itself
	 * However, that only works a panner was previously set.
	 *
	 * PannerUI must remain subscribed to _panshell->Changed() in case
	 * we switch the panner eg. AUX-Send and back
	 * _route->panner_shell()->Changed() vs _panshell->Changed
	 */
/*	if (panners._panner == 0) {
		panners.panshell_changed ();
	}
*/	update_panner_choices();
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

DataType
FoldbackStrip::guess_main_type(bool for_input, bool favor_connected) const
{
	/* The heuristic follows these principles:
	 *  A) If all ports that the user connected are of the same type, then he
	 *     very probably intends to use the IO with that type. A common subcase
	 *     is when the IO has only ports of the same type (connected or not).
	 *  B) If several types of ports are connected, then we should guess based
	 *     on the likeliness of the user wanting to use a given type.
	 *     We assume that the DataTypes are ordered from the most likely to the
	 *     least likely when iterating or comparing them with "<".
	 *  C) If no port is connected, the same logic can be applied with all ports
	 *     instead of connected ones. TODO: Try other ideas, for instance look at
	 *     the last plugin output when |for_input| is false (note: when StrictIO
	 *     the outs of the last plugin should be the same as the outs of the route
	 *     modulo the panner which forwards non-audio anyway).
	 * All of these constraints are respected by the following algorithm that
	 * just returns the most likely datatype found in connected ports if any, or
	 * available ports if any (since if all ports are of the same type, the most
	 * likely found will be that one obviously). */

	boost::shared_ptr<IO> io = for_input ? _route->input() : _route->output();

	/* Find most likely type among connected ports */
	if (favor_connected) {
		DataType type = DataType::NIL; /* NIL is always last so least likely */
		for (PortSet::iterator p = io->ports().begin(); p != io->ports().end(); ++p) {
			if (p->connected() && p->type() < type)
				type = p->type();
		}
		if (type != DataType::NIL) {
			/* There has been a connected port (necessarily non-NIL) */
			return type;
		}
	}

	/* Find most likely type among available ports.
	 * The iterator stops before NIL. */
	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		if (io->n_ports().n(*t) > 0)
			return *t;
	}

	/* No port at all, return the most likely datatype by default */
	return DataType::front();
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

	DataType dt = guess_main_type(false);
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
FoldbackStrip::show_passthru_color ()
{
	//reset_strip_style ();
}

void
FoldbackStrip::build_route_ops_menu ()
{
	using namespace Menu_Helpers;
	route_ops_menu = new Menu;
	route_ops_menu->set_name ("ArdourContextMenu");

	MenuList& items = route_ops_menu->items();

	items.push_back (MenuElem (_("Comments..."), sigc::mem_fun (*this, &RouteUI::open_comment_editor)));

	items.push_back (MenuElem (_("Outputs..."), sigc::mem_fun (*this, &RouteUI::edit_output_configuration)));

	items.push_back (SeparatorElem());

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
}

void
FoldbackStrip::build_route_select_menu ()
{
	using namespace Menu_Helpers;
	route_select_menu = new Menu;
	route_select_menu->set_name ("ArdourContextMenu");

	MenuList& items = route_select_menu->items();
	StripableList fb_list;
	_session->get_stripables (fb_list, PresentationInfo::FoldbackBus);
	for (StripableList::iterator s = fb_list.begin(); s != fb_list.end(); ++s) {

		boost::shared_ptr<Route> route = boost::dynamic_pointer_cast<Route> ((*s));
		if (route == _route) {
			continue;
		}
		items.push_back (MenuElem (route->name (), sigc::bind (sigc::mem_fun (*this, &FoldbackStrip::set_route), route)));
	}

}


gboolean
FoldbackStrip::name_button_button_press (GdkEventButton* ev)
{
	if (ev->button == 1 || ev->button == 3) {
		list_route_operations ();

		if (ev->button == 1) {
			Gtkmm2ext::anchored_menu_popup(route_ops_menu, &name_button, "",
			                               1, ev->time);
		} else {
			route_ops_menu->popup (3, ev->time);
		}

		return true;
	}

	return false;
}

gboolean
FoldbackStrip::select_button_button_press (GdkEventButton* ev)
{
	if (ev->button == 1 || ev->button == 3) {
		list_fb_routes ();

		if (ev->button == 1) {
			Gtkmm2ext::anchored_menu_popup(route_select_menu, &_select_button, "",
			                               1, ev->time);
		} else {
			route_select_menu->popup (3, ev->time);
		}

		return true;
	}

	return false;
}

void
FoldbackStrip::list_route_operations ()
{
	delete route_ops_menu;
	build_route_ops_menu ();
}

void
FoldbackStrip::list_fb_routes ()
{
	delete route_select_menu;
	build_route_select_menu ();
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

	name_button.set_text_ellipsize (Pango::ELLIPSIZE_END);
	name_button.set_text (_route->name());

	set_tooltip (name_button, Gtkmm2ext::markup_escape_text(_route->name()));

}

void
FoldbackStrip::output_button_resized (Gtk::Allocation& alloc)
{
	output_button.set_layout_ellipsize_width (alloc.get_width() * PANGO_SCALE);
}

void
FoldbackStrip::name_button_resized (Gtk::Allocation& alloc)
{
	name_button.set_layout_ellipsize_width (20);
}

void
FoldbackStrip::comment_button_resized (Gtk::Allocation& alloc)
{
	_comment_button.set_layout_ellipsize_width (alloc.get_width() * PANGO_SCALE);
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
}

void
FoldbackStrip::reset_strip_style ()
{
	set_name ("AudioBusStripBase");
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
	mute_button->set_sensitive (true);
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

		mute_button->set_text (_("Mute"));


		if (_route && _route->solo_safe_control()->solo_safe()) {
			solo_button->set_visual_state (Gtkmm2ext::VisualState (solo_button->visual_state() | Gtkmm2ext::Insensitive));
		} else {
			solo_button->set_visual_state (Gtkmm2ext::VisualState (solo_button->visual_state() & ~Gtkmm2ext::Insensitive));
		}
		if (!Config->get_solo_control_is_listen_control()) {
			solo_button->set_text (_("Solo"));
		} else {
			switch (Config->get_listen_position()) {
			case AfterFaderListen:
				solo_button->set_text (_("AFL"));
				break;
			case PreFaderListen:
				solo_button->set_text (_("PFL"));
				break;
			}
		}


}

string
FoldbackStrip::state_id () const
{
	return string_compose ("strip %1", _route->id().to_s());
}

void
FoldbackStrip::add_output_port (DataType t)
{
	_route->output()->add_port ("", this, t);
}

void
FoldbackStrip::route_active_changed ()
{
	reset_strip_style ();
}

void
FoldbackStrip::build_sends_menu ()
{
	using namespace Menu_Helpers;

	sends_menu = new Menu;
	sends_menu->set_name ("ArdourContextMenu");
	MenuList& items = sends_menu->items();

	items.push_back (
		MenuElem(_("Assign all tracks"), sigc::bind (sigc::mem_fun (*this, &RouteUI::create_sends), PreFader, false))
		);

	items.push_back (
		MenuElem(_("Assign all tracks and buses (prefader)"), sigc::bind (sigc::mem_fun (*this, &RouteUI::create_sends), PreFader, true))
		);

	items.push_back (
		MenuElem(_("Assign selected tracks (prefader)"), sigc::bind (sigc::mem_fun (*this, &RouteUI::create_selected_sends), PreFader, false))
		);

	items.push_back (
		MenuElem(_("Assign selected tracks and buses (prefader)"), sigc::bind (sigc::mem_fun (*this, &RouteUI::create_selected_sends), PreFader, true)));

	items.push_back (MenuElem(_("Copy track/bus gains to sends"), sigc::mem_fun (*this, &RouteUI::set_sends_gain_from_track)));
	items.push_back (MenuElem(_("Set sends gain to -inf"), sigc::mem_fun (*this, &RouteUI::set_sends_gain_to_zero)));
	items.push_back (MenuElem(_("Set sends gain to 0dB"), sigc::mem_fun (*this, &RouteUI::set_sends_gain_to_unity)));

}

Gdk::Color
FoldbackStrip::color () const
{
	return route_color ();
}
/*
bool
FoldbackStrip::marked_for_display () const
{
	return !_route->presentation_info().hidden();
}
*/
void
FoldbackStrip::remove_current_fb ()
{
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
	}
	_session->remove_route (old_route);


}
