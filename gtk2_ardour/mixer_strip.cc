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
#include <algorithm>

#include <sigc++/bind.h>

#include "pbd/convert.h"
#include "pbd/enumwriter.h"
#include "pbd/replace_all.h"

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/choice.h>
#include <gtkmm2ext/stop_signal.h>
#include <gtkmm2ext/doi.h>
#include <gtkmm2ext/slider_controller.h>
#include <gtkmm2ext/bindable_button.h>

#include "ardour/ardour.h"
#include "ardour/amp.h"
#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "ardour/route.h"
#include "ardour/route_group.h"
#include "ardour/audio_track.h"
#include "ardour/audio_diskstream.h"
#include "ardour/panner.h"
#include "ardour/send.h"
#include "ardour/processor.h"
#include "ardour/profile.h"
#include "ardour/ladspa_plugin.h"
#include "ardour/user_bundle.h"

#include "ardour_ui.h"
#include "ardour_dialog.h"
#include "mixer_strip.h"
#include "mixer_ui.h"
#include "keyboard.h"
#include "public_editor.h"
#include "send_ui.h"
#include "io_selector.h"
#include "utils.h"
#include "gui_thread.h"
#include "route_group_menu.h"

#include "i18n.h"

using namespace sigc;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace std;

sigc::signal<void,boost::shared_ptr<Route> > MixerStrip::SwitchIO;

int MixerStrip::scrollbar_height = 0;

MixerStrip::MixerStrip (Mixer_UI& mx, Session& sess, bool in_mixer)
 	: AxisView(sess)
	, RouteUI (sess)
	,_mixer(mx)
	, _mixer_owned (in_mixer)
 	, processor_box (sess, mx.plugin_selector(), mx.selection(), this, in_mixer)
	, gpm (sess)
	, panners (sess)
	, button_table (3, 2)
	, middle_button_table (1, 2)
 	, bottom_button_table (1, 2)
	, meter_point_label (_("pre"))
 	, comment_button (_("Comments"))
			 
{
	init ();
	
	if (!_mixer_owned) {
		/* the editor mixer strip: don't destroy it every time
		   the underlying route goes away.
		*/
		
		self_destruct = false;
	}
}

MixerStrip::MixerStrip (Mixer_UI& mx, Session& sess, boost::shared_ptr<Route> rt, bool in_mixer)
 	: AxisView(sess)
	, RouteUI (sess)
	,_mixer(mx)
	, _mixer_owned (in_mixer)
 	, processor_box (sess, mx.plugin_selector(), mx.selection(), this, in_mixer)
	, gpm (sess)
	, panners (sess)
	, button_table (3, 2)
	, middle_button_table (1, 2)
 	, bottom_button_table (1, 2)
	, meter_point_label (_("pre"))
 	, comment_button (_("Comments"))
			 
{
	init ();
	set_button_names ();
	set_route (rt);
}

void
MixerStrip::init ()
{
	input_selector = 0;
	output_selector = 0;
	group_menu = 0;
	_marked_for_display = false;
	route_ops_menu = 0;
	ignore_comment_edit = false;
	ignore_toggle = false;
	comment_window = 0;
	comment_area = 0;
	_width_owner = 0;
	spacer = 0;

	Gtk::Image* img;

	img = manage (new Gtk::Image (::get_icon("strip_width")));
	img->show ();

	width_button.add (*img);

	img = manage (new Gtk::Image (::get_icon("hide")));
	img->show ();

	hide_button.add (*img);

	input_label.set_text (_("Input"));
	ARDOUR_UI::instance()->set_tip (&input_button, _("Button 1 to choose inputs from a port matrix, button 3 to select inputs from a menu"), "");
	input_button.add (input_label);
	input_button.set_name ("MixerIOButton");
	input_label.set_name ("MixerIOButtonLabel");
	Gtkmm2ext::set_size_request_to_display_given_text (input_button, "longest label", 4, 4);

	output_label.set_text (_("Output"));
	ARDOUR_UI::instance()->set_tip (&output_button, _("Button 1 to choose outputs from a port matrix, button 3 to select inputs from a menu"), "");
	output_button.add (output_label);
	output_button.set_name ("MixerIOButton");
	output_label.set_name ("MixerIOButtonLabel");
	Gtkmm2ext::set_size_request_to_display_given_text (output_button, "longest label", 4, 4);

	ARDOUR_UI::instance()->set_tip (&meter_point_button, _("Select metering point"), "");
	meter_point_button.add (meter_point_label);
	meter_point_button.set_name ("MixerStripMeterPreButton");
	meter_point_label.set_name ("MixerStripMeterPreButton");
	
	/* TRANSLATORS: this string should be longest of the strings
	   used to describe meter points. In english, it's "input".
	*/
	set_size_request_to_display_given_text (meter_point_button, _("tupni"), 5, 5);
    
	bottom_button_table.attach (meter_point_button, 1, 2, 0, 1);
    
	meter_point_button.signal_button_press_event().connect (mem_fun (gpm, &GainMeter::meter_press), false);
	meter_point_button.signal_button_release_event().connect (mem_fun (gpm, &GainMeter::meter_release), false);

	hide_button.set_events (hide_button.get_events() & ~(Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK));

	mute_button->set_name ("MixerMuteButton");
	solo_button->set_name ("MixerSoloButton");

	button_table.set_homogeneous (true);
	button_table.set_spacings (0);

	button_table.attach (name_button, 0, 2, 0, 1);
	button_table.attach (input_button, 0, 2, 1, 2);

	middle_button_table.set_homogeneous (true);
	middle_button_table.set_spacings (0);
	middle_button_table.attach (*mute_button, 0, 1, 0, 1);
	middle_button_table.attach (*solo_button, 1, 2, 0, 1);

	bottom_button_table.set_col_spacings (0);
	bottom_button_table.set_homogeneous (true);
	bottom_button_table.attach (group_button, 0, 1, 0, 1);
	
	name_button.add (name_label);
	name_button.set_name ("MixerNameButton");
	Gtkmm2ext::set_size_request_to_display_given_text (name_button, "longest label", 2, 2);

	name_label.set_name ("MixerNameButtonLabel");
	ARDOUR_UI::instance()->set_tip (&group_button, _("Mix group"), "");
	group_button.add (group_label);
	group_button.set_name ("MixerGroupButton");
	group_label.set_name ("MixerGroupButtonLabel");

	comment_button.set_name ("MixerCommentButton");

	comment_button.signal_clicked().connect (mem_fun(*this, &MixerStrip::comment_button_clicked));
	
	global_vpacker.set_border_width (0);
	global_vpacker.set_spacing (0);

	width_button.set_name ("MixerWidthButton");
	hide_button.set_name ("MixerHideButton");
	top_event_box.set_name ("MixerTopEventBox");

	width_button.signal_clicked().connect (mem_fun(*this, &MixerStrip::width_clicked));
	hide_button.signal_clicked().connect (mem_fun(*this, &MixerStrip::hide_clicked));

	width_hide_box.pack_start (width_button, false, true);
	width_hide_box.pack_start (top_event_box, true, true);
	width_hide_box.pack_end (hide_button, false, true);
	gain_meter_alignment.set_padding(0, 4, 0, 0);
	gain_meter_alignment.add(gpm);

	whvbox.pack_start (width_hide_box, true, true);

	global_vpacker.pack_start (whvbox, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (button_table,Gtk::PACK_SHRINK);
	global_vpacker.pack_start (processor_box, true, true);
	global_vpacker.pack_start (middle_button_table,Gtk::PACK_SHRINK);
	global_vpacker.pack_start (gain_meter_alignment,Gtk::PACK_SHRINK);
	global_vpacker.pack_start (bottom_button_table,Gtk::PACK_SHRINK);
	if (!is_midi_track()) {
		global_vpacker.pack_start (panners, Gtk::PACK_SHRINK);
	}
	global_vpacker.pack_start (output_button, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (comment_button, Gtk::PACK_SHRINK);

	global_frame.add (global_vpacker);
	global_frame.set_shadow_type (Gtk::SHADOW_IN);
	global_frame.set_name ("BaseFrame");

	add (global_frame);

	/* force setting of visible selected status */

	_selected = true;
	set_selected (false);

	_packed = false;
	_embedded = false;

	_session.engine().Stopped.connect (mem_fun(*this, &MixerStrip::engine_stopped));
	_session.engine().Running.connect (mem_fun(*this, &MixerStrip::engine_running));

	input_button.signal_button_press_event().connect (mem_fun(*this, &MixerStrip::input_press), false);
	output_button.signal_button_press_event().connect (mem_fun(*this, &MixerStrip::output_press), false);

	solo_button->signal_button_press_event().connect (mem_fun(*this, &RouteUI::solo_press), false);
	solo_button->signal_button_release_event().connect (mem_fun(*this, &RouteUI::solo_release), false);
	mute_button->signal_button_press_event().connect (mem_fun(*this, &RouteUI::mute_press), false);
	mute_button->signal_button_release_event().connect (mem_fun(*this, &RouteUI::mute_release), false);

	/* we don't need this if its not an audio track, but we don't know that yet and it doesn't
	   hurt (much).
	*/

	rec_enable_button->set_name ("MixerRecordEnableButton");
	rec_enable_button->signal_button_press_event().connect (mem_fun(*this, &RouteUI::rec_enable_press), false);
	rec_enable_button->signal_button_release_event().connect (mem_fun(*this, &RouteUI::rec_enable_release));

	/* ditto for this button and busses */

	show_sends_button->signal_button_press_event().connect (mem_fun(*this, &RouteUI::show_sends_press), false);
	show_sends_button->signal_button_release_event().connect (mem_fun(*this, &RouteUI::show_sends_release));

	name_button.signal_button_press_event().connect (mem_fun(*this, &MixerStrip::name_button_button_press), false);
	group_button.signal_button_press_event().connect (mem_fun(*this, &MixerStrip::select_route_group), false);

	_width = (Width) -1;

	/* start off as a passthru strip. we'll correct this, if necessary,
	   in update_diskstream_display().
	*/

	/* start off as a passthru strip. we'll correct this, if necessary,
	   in update_diskstream_display().
	*/

	if (is_midi_track())
		set_name ("MidiTrackStripBase");
	else
		set_name ("AudioTrackStripBase");

	add_events (Gdk::BUTTON_RELEASE_MASK|
		    Gdk::ENTER_NOTIFY_MASK|
		    Gdk::LEAVE_NOTIFY_MASK|
		    Gdk::KEY_PRESS_MASK|
		    Gdk::KEY_RELEASE_MASK);

	set_flags (get_flags() | Gtk::CAN_FOCUS);
	
	SwitchIO.connect (mem_fun (*this, &MixerStrip::switch_io));
	
}

MixerStrip::~MixerStrip ()
{
	GoingAway(); /* EMIT_SIGNAL */

	delete input_selector;
	delete output_selector;
}

void
MixerStrip::set_route (boost::shared_ptr<Route> rt)
{
	if (rec_enable_button->get_parent()) {
		button_table.remove (*rec_enable_button);
	}

	if (show_sends_button->get_parent()) {
		button_table.remove (*show_sends_button);
	}

	RouteUI::set_route (rt);

	delete input_selector;
	input_selector = 0;

	delete output_selector;
	output_selector = 0;

	boost::shared_ptr<Send> send;

	if (_current_delivery && (send = boost::dynamic_pointer_cast<Send>(_current_delivery))) {
		send->set_metering (false);
	}

	_current_delivery = _route->main_outs ();

	panners.set_panner (rt->main_outs()->panner());
	gpm.set_controls (rt, rt->shared_peak_meter(), rt->gain_control(), rt->amp());
	processor_box.set_route (rt);

	if (set_color_from_route()) {
		set_color (unique_random_color());
	}

	if (_mixer_owned && (route()->is_master() || route()->is_control())) {
		
		if (scrollbar_height == 0) {
			HScrollbar scrollbar;
			Gtk::Requisition requisition(scrollbar.size_request ());
			scrollbar_height = requisition.height;
		}

		spacer = manage (new EventBox);
		spacer->set_size_request (-1, scrollbar_height);
		global_vpacker.pack_start (*spacer, false, false);
	}

	if (is_audio_track()) {

		boost::shared_ptr<AudioTrack> at = audio_track();

		connections.push_back (at->FreezeChange.connect (mem_fun(*this, &MixerStrip::map_frozen)));

		button_table.attach (*rec_enable_button, 0, 2, 2, 3);
		rec_enable_button->show();

	} else if (!is_track()) {
		/* non-master bus */

		if (!_route->is_master()) {
			button_table.attach (*show_sends_button, 0, 2, 2, 3);
			show_sends_button->show();
		}
	}

	if (_route->phase_invert()) {
	        name_label.set_text (X_("Ø ") + name_label.get_text());
	} else {
	        name_label.set_text (_route->name());
	}

	switch (_route->meter_point()) {
	case MeterInput:
		meter_point_label.set_text (_("input"));
		break;
		
	case MeterPreFader:
		meter_point_label.set_text (_("pre"));
		break;
		
	case MeterPostFader:
		meter_point_label.set_text (_("post"));
		break;
	}

	delete route_ops_menu;
	route_ops_menu = 0;
	
	ARDOUR_UI::instance()->tooltips().set_tip (comment_button, _route->comment().empty() ?
						   _("Click to Add/Edit Comments"):
						   _route->comment());

	connections.push_back (_route->meter_change.connect (
			mem_fun(*this, &MixerStrip::meter_changed)));
	connections.push_back (_route->input()->changed.connect (
			mem_fun(*this, &MixerStrip::input_changed)));
	connections.push_back (_route->output()->changed.connect (
			mem_fun(*this, &MixerStrip::output_changed)));
	connections.push_back (_route->route_group_changed.connect (
			mem_fun(*this, &MixerStrip::route_group_changed)));

	if (_route->panner()) {
		connections.push_back (_route->panner()->Changed.connect (
			mem_fun(*this, &MixerStrip::connect_to_pan)));
	}

	if (is_audio_track()) {
		connections.push_back (audio_track()->DiskstreamChanged.connect (
			mem_fun(*this, &MixerStrip::diskstream_changed)));
	}

	connections.push_back (_route->NameChanged.connect (
			mem_fun(*this, &RouteUI::name_changed)));
	connections.push_back (_route->comment_changed.connect (
			mem_fun(*this, &MixerStrip::comment_changed)));
	connections.push_back (_route->gui_changed.connect (
			mem_fun(*this, &MixerStrip::route_gui_changed)));

	set_stuff_from_route ();

	/* now force an update of all the various elements */

	processor_box.update();
	mute_changed (0);
	solo_changed (0);
	name_changed ();
	comment_changed (0);
	route_group_changed (0);

	connect_to_pan ();

	panners.setup_pan ();

	update_diskstream_display ();
	update_input_display ();
	update_output_display ();

	add_events (Gdk::BUTTON_RELEASE_MASK);

	processor_box.show();

	if (!route()->is_master() && !route()->is_control()) {
		/* we don't allow master or control routes to be hidden */
		hide_button.show();
	}

	width_button.show();
	width_hide_box.show();
	whvbox.show ();
	global_frame.show();
	global_vpacker.show();
	button_table.show();
	middle_button_table.show();
	bottom_button_table.show();
	processor_box.show_all ();
	gpm.show_all ();
	panners.show_all ();
	gain_meter_alignment.show ();
	gain_unit_button.show();
	gain_unit_label.show();
	meter_point_button.show();
	meter_point_label.show();
	diskstream_button.show();
	diskstream_label.show();
	input_button.show();
	input_label.show();
	output_button.show();
	output_label.show();
	name_label.show();
	name_button.show();
	comment_button.show();
	group_button.show();
	group_label.show();

	show ();
}

void
MixerStrip::set_stuff_from_route ()
{
	XMLProperty *prop;

	ensure_xml_node ();

	/* if width is not set, it will be set by the MixerUI or editor */

	if ((prop = xml_node->property ("strip-width")) != 0) {
		set_width_enum (Width (string_2_enum (prop->value(), _width)), this);
	}

	if ((prop = xml_node->property ("shown-mixer")) != 0) {
		if (prop->value() == "no") {
			_marked_for_display = false;
		} else {
			_marked_for_display = true;
		}
	} else {
		/* backwards compatibility */
		_marked_for_display = true;
	}
}

void
MixerStrip::set_width_enum (Width w, void* owner)
{
	/* always set the gpm width again, things may be hidden */

	gpm.set_width (w);
	panners.set_width (w);
	processor_box.set_width (w);

	boost::shared_ptr<AutomationList> gain_automation = _route->gain_control()->alist();

	_width_owner = owner;

	ensure_xml_node ();
	
	_width = w;

	if (_width_owner == this) {
		xml_node->add_property ("strip-width", enum_2_string (_width));
	}

	set_button_names ();

	switch (w) {
	case Wide:
		if (show_sends_button)  {
			((Gtk::Label*)show_sends_button->get_child())->set_text (_("Sends"));
		}

		if (_route->comment() == "") {
			comment_button.unset_bg (STATE_NORMAL);
			((Gtk::Label*)comment_button.get_child())->set_text (_("Comments"));
		} else {
			comment_button.modify_bg (STATE_NORMAL, color());
			((Gtk::Label*)comment_button.get_child())->set_text (_("*Comments*"));
		}

		((Gtk::Label*)gpm.gain_automation_style_button.get_child())->set_text (
				gpm.astyle_string(gain_automation->automation_style()));
		((Gtk::Label*)gpm.gain_automation_state_button.get_child())->set_text (
				gpm.astate_string(gain_automation->automation_state()));

		if (_route->panner()) {
			((Gtk::Label*)panners.pan_automation_style_button.get_child())->set_text (
					panners.astyle_string(_route->panner()->automation_style()));
			((Gtk::Label*)panners.pan_automation_state_button.get_child())->set_text (
					panners.astate_string(_route->panner()->automation_state()));
		}

		Gtkmm2ext::set_size_request_to_display_given_text (name_button, "long", 2, 2);
		set_size_request (-1, -1);
		break;

	case Narrow:
		if (show_sends_button) {
			((Gtk::Label*)show_sends_button->get_child())->set_text (_("Snd"));
		}

		if (_route->comment() == "") {
		       comment_button.unset_bg (STATE_NORMAL);
		       ((Gtk::Label*)comment_button.get_child())->set_text (_("Cmt"));
		} else {
		       comment_button.modify_bg (STATE_NORMAL, color());
		       ((Gtk::Label*)comment_button.get_child())->set_text (_("*Cmt*"));
		}

		((Gtk::Label*)gpm.gain_automation_style_button.get_child())->set_text (
				gpm.short_astyle_string(gain_automation->automation_style()));
		((Gtk::Label*)gpm.gain_automation_state_button.get_child())->set_text (
				gpm.short_astate_string(gain_automation->automation_state()));
		
		if (_route->panner()) {
			((Gtk::Label*)panners.pan_automation_style_button.get_child())->set_text (
			panners.short_astyle_string(_route->panner()->automation_style()));
			((Gtk::Label*)panners.pan_automation_state_button.get_child())->set_text (
			panners.short_astate_string(_route->panner()->automation_state()));
		}

		Gtkmm2ext::set_size_request_to_display_given_text (name_button, "longest label", 2, 2);
		set_size_request (max (50, gpm.get_gm_width()), -1);
		break;
	}
	update_input_display ();
	update_output_display ();
	route_group_changed (0);
	name_changed ();
	WidthChanged ();
}

void
MixerStrip::set_packed (bool yn)
{
	_packed = yn;

	ensure_xml_node ();

	if (_packed) {
		xml_node->add_property ("shown-mixer", "yes");
	} else {
		xml_node->add_property ("shown-mixer", "no");
	}
}


gint
MixerStrip::output_press (GdkEventButton *ev)
{
        using namespace Menu_Helpers;
	if (!_session.engine().connected()) {
	        MessageDialog msg (_("Not connected to JACK - no I/O changes are possible"));
		msg.run ();
		return true;
	}

	MenuList& citems = output_menu.items();
	switch (ev->button) {

	case 1:
		edit_output_configuration ();
		break;
		
	case 3:
	{
		output_menu.set_name ("ArdourContextMenu");
		citems.clear();
		
		citems.push_back (MenuElem (_("Disconnect"), mem_fun (*(static_cast<RouteUI*>(this)), &RouteUI::disconnect_output)));
		citems.push_back (SeparatorElem());

		ARDOUR::BundleList current = _route->output()->bundles_connected ();

		boost::shared_ptr<ARDOUR::BundleList> b = _session.bundles ();
		for (ARDOUR::BundleList::iterator i = b->begin(); i != b->end(); ++i) {
			maybe_add_bundle_to_output_menu (*i, current);
		}

		boost::shared_ptr<ARDOUR::RouteList> routes = _session.get_routes ();
		for (ARDOUR::RouteList::const_iterator i = routes->begin(); i != routes->end(); ++i) {
			maybe_add_bundle_to_output_menu ((*i)->input()->bundle(), current);
		}

		if (citems.size() == 2) {
			/* no routes added; remove the separator */
			citems.pop_back ();
		}

		output_menu.popup (1, ev->time);
		break;
	}

	default:
	        break;
	}
	return TRUE;
}

void
MixerStrip::edit_output_configuration ()
{
	if (output_selector == 0) {
		output_selector = new IOSelectorWindow (_session, _route->output());
	} 

	if (output_selector->is_visible()) {
		output_selector->get_toplevel()->get_window()->raise();
	} else {
		output_selector->present ();
	}
}

void
MixerStrip::edit_input_configuration ()
{
	if (input_selector == 0) {
		input_selector = new IOSelectorWindow (_session, _route->input());
	} 

	if (input_selector->is_visible()) {
		input_selector->get_toplevel()->get_window()->raise();
	} else {
		input_selector->present ();
	}
}

gint
MixerStrip::input_press (GdkEventButton *ev)
{
	using namespace Menu_Helpers;

	MenuList& citems = input_menu.items();
	input_menu.set_name ("ArdourContextMenu");
	citems.clear();
	
	if (!_session.engine().connected()) {
	        MessageDialog msg (_("Not connected to JACK - no I/O changes are possible"));
		msg.run ();
		return true;
	}

	switch (ev->button) {

	case 1:
		edit_input_configuration ();
		break;

	case 3:
	{
		citems.push_back (MenuElem (_("Disconnect"), mem_fun (*(static_cast<RouteUI*>(this)), &RouteUI::disconnect_input)));
		citems.push_back (SeparatorElem());

		ARDOUR::BundleList current = _route->input()->bundles_connected ();

		boost::shared_ptr<ARDOUR::BundleList> b = _session.bundles ();
		for (ARDOUR::BundleList::iterator i = b->begin(); i != b->end(); ++i) {
			maybe_add_bundle_to_input_menu (*i, current);
		}

		boost::shared_ptr<ARDOUR::RouteList> routes = _session.get_routes ();
		for (ARDOUR::RouteList::const_iterator i = routes->begin(); i != routes->end(); ++i) {
			maybe_add_bundle_to_input_menu ((*i)->output()->bundle(), current);
		}

		if (citems.size() == 2) {
			/* no routes added; remove the separator */
			citems.pop_back ();
		}

		input_menu.popup (1, ev->time);
		break;
	}
	default:
	        break;
	}
	return TRUE;
}

void
MixerStrip::bundle_input_toggled (boost::shared_ptr<ARDOUR::Bundle> c)
{
	if (ignore_toggle) {
		return;
	}

	ARDOUR::BundleList current = _route->input()->bundles_connected ();

	if (std::find (current.begin(), current.end(), c) == current.end()) {
		_route->input()->connect_ports_to_bundle (c, this);
	} else {
		_route->input()->disconnect_ports_from_bundle (c, this);
	}
}

void
MixerStrip::bundle_output_toggled (boost::shared_ptr<ARDOUR::Bundle> c)
{
	if (ignore_toggle) {
		return;
	}

	ARDOUR::BundleList current = _route->output()->bundles_connected ();

	if (std::find (current.begin(), current.end(), c) == current.end()) {
		_route->output()->connect_ports_to_bundle (c, this);
	} else {
		_route->output()->disconnect_ports_from_bundle (c, this);
	}
}

void
MixerStrip::maybe_add_bundle_to_input_menu (boost::shared_ptr<Bundle> b, ARDOUR::BundleList const & current)
{
	using namespace Menu_Helpers;

 	if (b->ports_are_outputs() == false ||
	    route()->input()->default_type() != b->type() ||
	    b->nchannels() != _route->n_inputs().get (b->type ())) {
		
 		return;
 	}

	MenuList& citems = input_menu.items();
	
	std::string n = b->name ();
	replace_all (n, "_", " ");
	
	citems.push_back (CheckMenuElem (n, bind (mem_fun(*this, &MixerStrip::bundle_input_toggled), b)));
	
	if (std::find (current.begin(), current.end(), b) != current.end()) {
		ignore_toggle = true;
		dynamic_cast<CheckMenuItem *> (&citems.back())->set_active (true);
		ignore_toggle = false;
	}
}

void
MixerStrip::maybe_add_bundle_to_output_menu (boost::shared_ptr<Bundle> b, ARDOUR::BundleList const & current)
{
	using namespace Menu_Helpers;

 	if (b->ports_are_inputs() == false ||
	    route()->output()->default_type() != b->type() ||
	    b->nchannels() != _route->n_outputs().get (b->type ())) {
		
 		return;
 	}

	MenuList& citems = output_menu.items();
	
	std::string n = b->name ();
	replace_all (n, "_", " ");
	
	citems.push_back (CheckMenuElem (n, bind (mem_fun(*this, &MixerStrip::bundle_output_toggled), b)));
	
	if (std::find (current.begin(), current.end(), b) != current.end()) {
		ignore_toggle = true;
		dynamic_cast<CheckMenuItem *> (&citems.back())->set_active (true);
		ignore_toggle = false;
	}
}

void
MixerStrip::update_diskstream_display ()
{
	if (is_track()) {

		if (input_selector) {
			input_selector->hide_all ();
		}

		show_route_color ();

	} else {

		show_passthru_color ();
	}
}

void
MixerStrip::connect_to_pan ()
{
	ENSURE_GUI_THREAD(mem_fun(*this, &MixerStrip::connect_to_pan));

	panstate_connection.disconnect ();
	panstyle_connection.disconnect ();

	if (!_route->panner()) {
		return;
	}

	boost::shared_ptr<ARDOUR::AutomationControl> pan_control
		= boost::dynamic_pointer_cast<ARDOUR::AutomationControl>(
				_route->panner()->data().control(Evoral::Parameter(PanAutomation)));

	if (pan_control) {
		panstate_connection = pan_control->alist()->automation_state_changed.connect (mem_fun(panners, &PannerUI::pan_automation_state_changed));
		panstyle_connection = pan_control->alist()->automation_style_changed.connect (mem_fun(panners, &PannerUI::pan_automation_style_changed));
	}

	panners.pan_changed (this);
}

void
MixerStrip::update_input_display ()
{
	ARDOUR::BundleList const c = _route->input()->bundles_connected();

	if (c.size() > 1) {
		input_label.set_text (_("Inputs"));
	} else if (c.size() == 1) {
		input_label.set_text (c[0]->name ());
	} else {
		switch (_width) {
		case Wide:
			input_label.set_text (_(" Input"));
			break;
		case Narrow:
			input_label.set_text (_("I"));
			break;
		}
	}
	panners.setup_pan ();
}

void
MixerStrip::update_output_display ()
{
	ARDOUR::BundleList const c = _route->output()->bundles_connected ();

	/* XXX: how do we represent >1 connected bundle? */
	if (c.size() > 1) {
		output_label.set_text (_("Outputs"));
	} else if (c.size() == 1) {
		output_label.set_text (c[0]->name());
	} else {
		switch (_width) {
		case Wide:
			output_label.set_text (_("Output"));
			break;
		case Narrow:
			output_label.set_text (_("O"));
			break;
		}
	}

	gpm.setup_meters ();
	panners.setup_pan ();
}

void
MixerStrip::fast_update ()
{
	gpm.update_meters ();
}

void
MixerStrip::diskstream_changed ()
{
	Gtkmm2ext::UI::instance()->call_slot (mem_fun(*this, &MixerStrip::update_diskstream_display));
}	

void
MixerStrip::input_changed (IOChange change, void *src)
{
	Gtkmm2ext::UI::instance()->call_slot (mem_fun(*this, &MixerStrip::update_input_display));
	set_width_enum (_width, this);
}

void
MixerStrip::output_changed (IOChange change, void *src)
{
	Gtkmm2ext::UI::instance()->call_slot (mem_fun(*this, &MixerStrip::update_output_display));
	set_width_enum (_width, this);
}


void 
MixerStrip::comment_editor_done_editing() 
{
	string str =  comment_area->get_buffer()->get_text();
	if (_route->comment() != str) {
		_route->set_comment (str, this);

		switch (_width) {
		   
		case Wide:
			if (! str.empty()) {
			        comment_button.modify_bg (STATE_NORMAL, color());
				((Gtk::Label*)comment_button.get_child())->set_text (_("*Comments*"));
			} else {
			        comment_button.unset_bg (STATE_NORMAL);
				((Gtk::Label*)comment_button.get_child())->set_text (_("Comments"));
			}
			break;
		   
		case Narrow:
			if (! str.empty()) {
			        comment_button.modify_bg (STATE_NORMAL, color());
				((Gtk::Label*)comment_button.get_child())->set_text (_("*Cmt*"));
			} else {
			        comment_button.unset_bg (STATE_NORMAL);
				((Gtk::Label*)comment_button.get_child())->set_text (_("Cmt"));
			} 
			break;
		}
		 
		ARDOUR_UI::instance()->tooltips().set_tip (comment_button, 
				str.empty() ? _("Click to Add/Edit Comments") : str);
	}

}

void
MixerStrip::comment_button_clicked ()
{
	if (comment_window == 0) {
		setup_comment_editor ();
	}

    int x, y, cw_width, cw_height;

	if (comment_window->is_visible()) {
		comment_window->hide ();
		return;
	}

	comment_window->get_size (cw_width, cw_height);
	comment_window->get_position(x, y);
	comment_window->move(x, y - (cw_height / 2) - 45);
	/* 
	   half the dialog height minus the comments button height 
	   with some window decoration fudge thrown in.
	*/

	comment_window->show();
	comment_window->present();
}

void
MixerStrip::setup_comment_editor ()
{
	string title;
	title = _route->name();
	title += _(": comment editor");

	comment_window = new ArdourDialog (title, false);
	comment_window->set_position (Gtk::WIN_POS_MOUSE);
	comment_window->set_skip_taskbar_hint (true);
	comment_window->signal_hide().connect (mem_fun(*this, &MixerStrip::comment_editor_done_editing));

	comment_area = manage (new TextView());
	comment_area->set_name ("MixerTrackCommentArea");
	comment_area->set_size_request (110, 178);
	comment_area->set_wrap_mode (WRAP_WORD);
	comment_area->set_editable (true);
	comment_area->get_buffer()->set_text (_route->comment());
	comment_area->show ();

	comment_window->get_vbox()->pack_start (*comment_area);
	comment_window->get_action_area()->hide();
}

void
MixerStrip::comment_changed (void *src)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &MixerStrip::comment_changed), src));
	
	if (src != this) {
		ignore_comment_edit = true;
		if (comment_area) {
			comment_area->get_buffer()->set_text (_route->comment());
		}
		ignore_comment_edit = false;
	}
}

void
MixerStrip::set_route_group (RouteGroup *rg)
{
	_route->set_route_group (rg, this);
}

bool
MixerStrip::select_route_group (GdkEventButton *ev)
{
	using namespace Menu_Helpers;

	if (ev->button == 1) {

		if (group_menu == 0) {
			
			group_menu = new RouteGroupMenu (
				_session,
				(RouteGroup::Property) (RouteGroup::Gain | RouteGroup::Mute | RouteGroup::Solo)
				);
			
			group_menu->GroupSelected.connect (mem_fun (*this, &MixerStrip::set_route_group));
		}

		group_menu->popup (1, ev->time);
	}
	
	return true;
}	

void
MixerStrip::route_group_changed (void *ignored)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &MixerStrip::route_group_changed), ignored));
	
	RouteGroup *rg = _route->route_group();

	if (rg) {
		/* XXX: this needs a better algorithm */
		string truncated = rg->name ();
		if (truncated.length () > 5) {
			truncated = truncated.substr (0, 5);
		}
		group_label.set_text (truncated);
	} else {
		switch (_width) {
		case Wide:
			group_label.set_text (_("Grp"));
			break;
		case Narrow:
			group_label.set_text (_("~G"));
			break;
		}
	}
}


void 
MixerStrip::route_gui_changed (string what_changed, void* ignored)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &MixerStrip::route_gui_changed), what_changed, ignored));
	
	if (what_changed == "color") {
		if (set_color_from_route () == 0) {
			show_route_color ();
		}
	}
}

void
MixerStrip::show_route_color ()
{
	name_button.modify_bg (STATE_NORMAL, color());
	top_event_box.modify_bg (STATE_NORMAL, color());
	route_active_changed ();
}

void
MixerStrip::show_passthru_color ()
{
	route_active_changed ();
}

void
MixerStrip::build_route_ops_menu ()
{
	using namespace Menu_Helpers;
	route_ops_menu = new Menu;
	route_ops_menu->set_name ("ArdourContextMenu");

	MenuList& items = route_ops_menu->items();

	items.push_back (MenuElem (_("Save As Template"), mem_fun(*this, &RouteUI::save_as_template)));
	items.push_back (MenuElem (_("Rename"), mem_fun(*this, &RouteUI::route_rename)));
	rename_menu_item = &items.back();
	items.push_back (SeparatorElem());
	items.push_back (CheckMenuElem (_("Active"), mem_fun (*this, &RouteUI::toggle_route_active)));
	route_active_menu_item = dynamic_cast<CheckMenuItem *> (&items.back());
	route_active_menu_item->set_active (_route->active());

	items.push_back (SeparatorElem());

	items.push_back (MenuElem (_("Adjust latency"), mem_fun (*this, &RouteUI::adjust_latency)));

	items.push_back (SeparatorElem());
	items.push_back (CheckMenuElem (_("Invert Polarity"), mem_fun (*this, &RouteUI::toggle_polarity)));
	polarity_menu_item = dynamic_cast<CheckMenuItem *> (&items.back());
	polarity_menu_item->set_active (_route->phase_invert());
	items.push_back (CheckMenuElem (_("Protect against denormals"), mem_fun (*this, &RouteUI::toggle_denormal_protection)));
	denormal_menu_item = dynamic_cast<CheckMenuItem *> (&items.back());
	denormal_menu_item->set_active (_route->denormal_protection());

	if (!Profile->get_sae()) {
		build_remote_control_menu ();
		items.push_back (SeparatorElem());
		items.push_back (MenuElem (_("Remote Control ID"), *remote_control_menu));
        }

	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Remove"), mem_fun(*this, &RouteUI::remove_this_route)));
}

gint
MixerStrip::name_button_button_press (GdkEventButton* ev)
{
	if (ev->button == 1 || ev->button == 3) {
		list_route_operations ();

		/* do not allow rename if the track is record-enabled */
		rename_menu_item->set_sensitive (!_route->record_enabled());
		route_ops_menu->popup (1, ev->time);
	}
	return FALSE;
}

void
MixerStrip::list_route_operations ()
{
	if (route_ops_menu == 0) {
		build_route_ops_menu ();
	}
	
	refresh_remote_control_menu();
}

void
MixerStrip::set_selected (bool yn)
{
	AxisView::set_selected (yn);
	if (_selected) {
		global_frame.set_shadow_type (Gtk::SHADOW_ETCHED_OUT);
		global_frame.set_name ("MixerStripSelectedFrame");
	} else {
		global_frame.set_shadow_type (Gtk::SHADOW_IN);
		global_frame.set_name ("MixerStripFrame");
	}
	global_frame.queue_draw ();
}

void
MixerStrip::name_changed ()
{
	switch (_width) {
	case Wide:
		RouteUI::name_changed ();
		break;
	case Narrow:
	        name_label.set_text (PBD::short_version (_route->name(), 5));
		break;
	}
	if (_route->phase_invert()) {
	        name_label.set_text (X_("Ø ") + name_label.get_text());
	}
}

void
MixerStrip::width_clicked ()
{
	switch (_width) {
	case Wide:
		set_width_enum (Narrow, this);
		break;
	case Narrow:
		set_width_enum (Wide, this);
		break;
	}
}

void
MixerStrip::hide_clicked ()
{
	// LAME fix to reset the button status for when it is redisplayed (part 1)
	hide_button.set_sensitive(false);
	
	if (_embedded) {
		Hiding(); /* EMIT_SIGNAL */
	} else {
		_mixer.hide_strip (this);
	}
	
	// (part 2)
	hide_button.set_sensitive(true);
}

void
MixerStrip::set_embedded (bool yn)
{
	_embedded = yn;
}

void
MixerStrip::map_frozen ()
{
	ENSURE_GUI_THREAD (mem_fun(*this, &MixerStrip::map_frozen));

	boost::shared_ptr<AudioTrack> at = audio_track();

	if (at) {
		switch (at->freeze_state()) {
		case AudioTrack::Frozen:
			processor_box.set_sensitive (false);
			break;
		default:
			processor_box.set_sensitive (true);
			// XXX need some way, maybe, to retoggle redirect editors
			break;
		}
	}
	
	hide_redirect_editors ();
}

void
MixerStrip::hide_redirect_editors ()
{
	_route->foreach_processor (mem_fun (*this, &MixerStrip::hide_processor_editor));
}

void
MixerStrip::hide_processor_editor (boost::weak_ptr<Processor> p)
{
	boost::shared_ptr<Processor> processor (p.lock ());
	if (!processor) {
		return;
	}
	
	void* gui = processor->get_gui ();
	
	if (gui) {
		static_cast<Gtk::Widget*>(gui)->hide ();
	}
}

void
MixerStrip::route_active_changed ()
{
	RouteUI::route_active_changed ();

	if (is_midi_track()) {
		if (_route->active()) {
			set_name ("MidiTrackStripBase");
			gpm.set_meter_strip_name ("MidiTrackStripBase");
		} else {
			set_name ("MidiTrackStripBaseInactive");
			gpm.set_meter_strip_name ("MidiTrackStripBaseInactive");
		}
		gpm.set_fader_name ("MidiTrackFader");
	} else if (is_audio_track()) {
		if (_route->active()) {
			set_name ("AudioTrackStripBase");
			gpm.set_meter_strip_name ("AudioTrackMetrics");
		} else {
			set_name ("AudioTrackStripBaseInactive");
			gpm.set_meter_strip_name ("AudioTrackMetricsInactive");
		}
		gpm.set_fader_name ("AudioTrackFader");
	} else {
		if (_route->active()) {
			set_name ("AudioBusStripBase");
			gpm.set_meter_strip_name ("AudioBusMetrics");
		} else {
			set_name ("AudioBusStripBaseInactive");
			gpm.set_meter_strip_name ("AudioBusMetricsInactive");
		}
		gpm.set_fader_name ("AudioBusFader");
		
		/* (no MIDI busses yet) */
	}
}

RouteGroup*
MixerStrip::route_group() const
{
	return _route->route_group();
}

void
MixerStrip::engine_stopped ()
{
}

void
MixerStrip::engine_running ()
{
}

void
MixerStrip::meter_changed (void *src)
{
	ENSURE_GUI_THREAD (bind (mem_fun(*this, &MixerStrip::meter_changed), src));

	switch (_route->meter_point()) {
	case MeterInput:
		meter_point_label.set_text (_("input"));
		break;

	case MeterPreFader:
		meter_point_label.set_text (_("pre"));
		break;
		
	case MeterPostFader:
		meter_point_label.set_text (_("post"));
		break;
	}

	gpm.setup_meters ();
	// reset peak when meter point changes
	gpm.reset_peak_display();
	set_width_enum (_width, this);
}

void
MixerStrip::switch_io (boost::shared_ptr<Route> target)
{
	if (_route == target || _route->is_master()) {
		/* don't change the display for the target or the master bus */
		return;
	} else if (!is_track() && show_sends_button) {
		/* make sure our show sends button is inactive, and we no longer blink,
		   since we're not the target.
		*/
		send_blink_connection.disconnect ();
		show_sends_button->set_active (false);
		show_sends_button->set_state (STATE_NORMAL);
	}

	if (!target) {
		/* switch back to default */
		revert_to_default_display ();
		return;
	}
	
	boost::shared_ptr<Send> send;

	if (_current_delivery && (send = boost::dynamic_pointer_cast<Send>(_current_delivery))) {
		send->set_metering (false);
	}
	
	_current_delivery = _route->internal_send_for (target);

	cerr << "internal send from " << _route->name() << " to " << target->name() << " = " 
	     << _current_delivery << endl;

	if (_current_delivery) {
		send = boost::dynamic_pointer_cast<Send>(_current_delivery);
		send->set_metering (true);
		_current_delivery->GoingAway.connect (mem_fun (*this, &MixerStrip::revert_to_default_display));
		gain_meter().set_controls (_route, send->meter(), send->amp()->gain_control(), send->amp());
		panner_ui().set_panner (_current_delivery->panner());

	} else {
		_current_delivery = _route->main_outs ();
		gain_meter().set_controls (_route, _route->shared_peak_meter(), _route->gain_control(), _route->amp());
		panner_ui().set_panner (_route->main_outs()->panner());
	}
	
	gain_meter().setup_meters ();
	panner_ui().setup_pan ();
}


void
MixerStrip::revert_to_default_display ()
{
	show_sends_button->set_active (false);
	
	boost::shared_ptr<Send> send;

	if (_current_delivery && (send = boost::dynamic_pointer_cast<Send>(_current_delivery))) {
		send->set_metering (false);
	}
	
	_current_delivery = _route->main_outs();

	gain_meter().set_controls (_route, _route->shared_peak_meter(), _route->gain_control(), _route->amp());
	gain_meter().setup_meters ();
	panner_ui().set_panner (_route->main_outs()->panner());
	panner_ui().setup_pan ();
}

void
MixerStrip::set_button_names ()
{
	switch (_width) {
	case Wide:
		rec_enable_button_label.set_text (_("Rec"));
		mute_button_label.set_text (_("Mute"));
		if (!Config->get_solo_control_is_listen_control()) {
			solo_button_label.set_text (_("Solo"));
		} else {
			switch (Config->get_listen_position()) {
			case AfterFaderListen:
				solo_button_label.set_text (_("AFL"));
				break;
			case PreFaderListen:
				solo_button_label.set_text (_("PFL"));
				break;
			}
		}
		break;

	default:
		rec_enable_button_label.set_text (_("R"));
		mute_button_label.set_text (_("M"));
		if (!Config->get_solo_control_is_listen_control()) {
			solo_button_label.set_text (_("S"));
		} else {
			switch (Config->get_listen_position()) {
			case AfterFaderListen:
				solo_button_label.set_text (_("A"));
				break;
			case PreFaderListen:
				solo_button_label.set_text (_("P"));
				break;
			}
		}
		break;
		
	}
}

bool
MixerStrip::on_key_press_event (GdkEventKey* ev)
{
	GdkEventButton fake;
	fake.type = GDK_BUTTON_PRESS;
	fake.button = 1;
	fake.state = ev->state;

	switch (ev->keyval) {
	case GDK_m:
		mute_press (&fake);
		return true;
		break;
		
	case GDK_s:
		solo_press (&fake);
		return true;
		break;
		
	case GDK_r:
		rec_enable_press (&fake);
		return true;
		break;
		
	case GDK_e:
		show_sends_press (&fake);
		return true;
		break;			
		
	case GDK_g:
		if (ev->state & Keyboard::PrimaryModifier) {
			step_gain_down ();
		} else {
			step_gain_up ();
		}
		return true;
		break;

	case GDK_0:
		if (_route) {
			_route->set_gain (1.0, this);
		}
		return true;
		
	default:
		break;
	}

	return false;
}


bool
MixerStrip::on_key_release_event (GdkEventKey* ev)
{
	GdkEventButton fake;
	fake.type = GDK_BUTTON_RELEASE;
	fake.button = 1;
	fake.state = ev->state;

	switch (ev->keyval) {
	case GDK_m:
		mute_release (&fake);
		return true;
		break;
		
	case GDK_s:
		solo_release (&fake);
		return true;
		break;
		
	case GDK_r:
		rec_enable_release (&fake);
		return true;
		break;
		
	case GDK_e:
		show_sends_release (&fake);
		return true;
		break;			
		
	case GDK_g:
		return true;
		break;
		
	default:
		break;
	}

	return false;
}

bool
MixerStrip::on_enter_notify_event (GdkEventCrossing* ev)
{
	Keyboard::magic_widget_grab_focus ();
	grab_focus ();
	return false;
}

bool
MixerStrip::on_leave_notify_event (GdkEventCrossing* ev)
{
	switch (ev->detail) {
	case GDK_NOTIFY_INFERIOR:
		break;
	default:
		Keyboard::magic_widget_drop_focus ();
	}

	return false;
}
