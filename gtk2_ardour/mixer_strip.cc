/*
    Copyright (C) 2000-2002 Paul Davis

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

#include <pbd/convert.h>
#include <pbd/enumwriter.h>

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/choice.h>
#include <gtkmm2ext/stop_signal.h>
#include <gtkmm2ext/doi.h>
#include <gtkmm2ext/slider_controller.h>
#include <gtkmm2ext/bindable_button.h>

#include <ardour/ardour.h>
#include <ardour/session.h>
#include <ardour/audioengine.h>
#include <ardour/route.h>
#include <ardour/audio_track.h>
#include <ardour/audio_diskstream.h>
#include <ardour/panner.h>
#include <ardour/send.h>
#include <ardour/insert.h>
#include <ardour/profile.h>
#include <ardour/ladspa_plugin.h>
#include <ardour/connection.h>
#include <ardour/session_connection.h>

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

#include "i18n.h"

using namespace sigc;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace std;

int MixerStrip::scrollbar_height = 0;

#ifdef VARISPEED_IN_MIXER_STRIP
static void 
speed_printer (char buf[32], Gtk::Adjustment& adj, void* arg)
{
	float val = adj.get_value ();

	if (val == 1.0) {
		strcpy (buf, "1");
	} else {
		snprintf (buf, 32, "%.3f", val);
	}
}
#endif 

MixerStrip::MixerStrip (Mixer_UI& mx, Session& sess, bool in_mixer)
	: AxisView(sess),
	  RouteUI (sess, _("Mute"), _("Solo"), _("Record")),
	  _mixer(mx),
	  _mixer_owned (in_mixer),
	  pre_redirect_box (PreFader, sess, mx.plugin_selector(), mx.selection(), in_mixer),
	  post_redirect_box (PostFader, sess, mx.plugin_selector(), mx.selection(), in_mixer),
	  gpm (sess),
	  panners (sess),
	  button_table (3, 2),
	  middle_button_table (1, 2),
	  bottom_button_table (1, 2),
	  meter_point_label (_("pre")),
	  comment_button (_("Comments")),
	  speed_adjustment (1.0, 0.001, 4.0, 0.001, 0.1),
	  speed_spinner (&speed_adjustment, "MixerStripSpeedBase", true)

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
	: AxisView(sess),
	  RouteUI (sess, _("Mute"), _("Solo"), _("Record")),
	  _mixer(mx),
	  _mixer_owned (in_mixer),
	  pre_redirect_box (PreFader, sess, mx.plugin_selector(), mx.selection(), in_mixer),
	  post_redirect_box (PostFader, sess, mx.plugin_selector(), mx.selection(), in_mixer),
	  gpm (sess),
	  panners (sess),
	  button_table (3, 2),
	  middle_button_table (1, 2),
	  bottom_button_table (1, 2),
	  meter_point_label (_("pre")),
	  comment_button (_("Comments")),
	  speed_adjustment (1.0, 0.001, 4.0, 0.001, 0.1),
	  speed_spinner (&speed_adjustment, "MixerStripSpeedBase", true)

{
	init ();
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
	rename_menu_item = 0;
	ignore_comment_edit = false;
	ignore_toggle = false;
	ignore_speed_adjustment = false;
	comment_window = 0;
	comment_area = 0;
	_width_owner = 0;

	width_button.add (*(manage (new Gtk::Image (::get_icon("strip_width")))));
	hide_button.add (*(manage (new Gtk::Image (::get_icon("hide")))));

	input_label.set_text (_("Input"));
	ARDOUR_UI::instance()->set_tip (&input_button, _("Click to choose inputs"), "");
	input_button.add (input_label);
	input_button.set_name ("MixerIOButton");
	input_label.set_name ("MixerIOButtonLabel");

	output_label.set_text (_("Output"));
	ARDOUR_UI::instance()->set_tip (&output_button, _("Click to choose outputs"), "");
	output_button.add (output_label);
	output_button.set_name ("MixerIOButton");
	output_label.set_name ("MixerIOButtonLabel");

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
	/* XXX what is this meant to do? */
	//meter_point_button.signal_button_release_event().connect (mem_fun (gpm, &GainMeter::meter_release), false);

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
	Gtkmm2ext::set_size_request_to_display_given_text (group_button, "Group", 2, 2);

	group_label.set_name ("MixerGroupButtonLabel");

	comment_button.set_name ("MixerCommentButton");

	comment_button.signal_clicked().connect (mem_fun(*this, &MixerStrip::comment_button_clicked));
	
	global_vpacker.set_border_width (0);
	global_vpacker.set_spacing (0);

	VBox *whvbox = manage (new VBox);

	width_button.set_name ("MixerWidthButton");
	hide_button.set_name ("MixerHideButton");
	top_event_box.set_name ("MixerTopEventBox");

	width_button.signal_clicked().connect (mem_fun(*this, &MixerStrip::width_clicked));
	hide_button.signal_clicked().connect (mem_fun(*this, &MixerStrip::hide_clicked));

	width_hide_box.pack_start (width_button, false, true);
	width_hide_box.pack_start (top_event_box, true, true);
	width_hide_box.pack_end (hide_button, false, true);
	Gtk::Alignment *gain_meter_alignment = Gtk::manage(new Gtk::Alignment());
	gain_meter_alignment->set_padding(0, 4, 0, 0);
	gain_meter_alignment->add(gpm);

	whvbox->pack_start (width_hide_box, true, true);

	global_vpacker.pack_start (*whvbox, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (button_table,Gtk::PACK_SHRINK);
	global_vpacker.pack_start (pre_redirect_box, true, true);
	global_vpacker.pack_start (middle_button_table,Gtk::PACK_SHRINK);
	global_vpacker.pack_start (*gain_meter_alignment,Gtk::PACK_SHRINK);
	global_vpacker.pack_start (bottom_button_table,Gtk::PACK_SHRINK);
	global_vpacker.pack_start (post_redirect_box, true, true);
	global_vpacker.pack_start (panners, Gtk::PACK_SHRINK);
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

	name_button.signal_button_press_event().connect (mem_fun(*this, &MixerStrip::name_button_button_press), false);
	group_button.signal_button_press_event().connect (mem_fun(*this, &MixerStrip::select_mix_group), false);

	_width = (Width) -1;

	/* start off as a passthru strip. we'll correct this, if necessary,
	   in update_diskstream_display().
	*/

	set_name ("AudioTrackStripBase");

	add_events (Gdk::BUTTON_RELEASE_MASK);
}

MixerStrip::~MixerStrip ()
{
	GoingAway(); /* EMIT_SIGNAL */

	if (input_selector) {
		delete input_selector;
	}

	if (output_selector) {
		delete output_selector;
	}
	
	if(comment_window) {
		delete comment_window;
	}
}

void
MixerStrip::set_route (boost::shared_ptr<Route> rt)
{
	if (rec_enable_button->get_parent()) {
		button_table.remove (*rec_enable_button);
	}

#ifdef VARISPEED_IN_MIXER_STRIP
	if (speed_frame->get_parent()) {
		button_table.remove (*speed_frame);
	}
#endif

	RouteUI::set_route (rt);

	if (input_selector) {
		delete input_selector;
		input_selector = 0;
	}

	if (output_selector) {
		delete output_selector;
		output_selector = 0;
	}

	panners.set_io (rt);
	gpm.set_io (rt);
	pre_redirect_box.set_route (rt);
	post_redirect_box.set_route (rt);

	if (set_color_from_route()) {
		set_color (unique_random_color());
	}

	if (_mixer_owned && (route()->master() || route()->control())) {
		
		if (scrollbar_height == 0) {
			HScrollbar scrollbar;
			Gtk::Requisition requisition(scrollbar.size_request ());
			scrollbar_height = requisition.height;
		}

		EventBox* spacer = manage (new EventBox);
		spacer->set_size_request (-1, scrollbar_height);
		global_vpacker.pack_start (*spacer, false, false);
	}

	if (is_audio_track()) {

		boost::shared_ptr<AudioTrack> at = audio_track();

		connections.push_back (at->FreezeChange.connect (mem_fun(*this, &MixerStrip::map_frozen)));

#ifdef VARISPEED_IN_MIXER_STRIP
		speed_adjustment.signal_value_changed().connect (mem_fun(*this, &MixerStrip::speed_adjustment_changed));
		
		speed_frame.set_name ("BaseFrame");
		speed_frame.set_shadow_type (Gtk::SHADOW_IN);
		speed_frame.add (speed_spinner);
		
		speed_spinner.set_print_func (speed_printer, 0);

		ARDOUR_UI::instance()->tooltips().set_tip (speed_spinner, _("Varispeed"));

		button_table.attach (speed_frame, 0, 2, 5, 6);
#endif /* VARISPEED_IN_MIXER_STRIP */

		button_table.attach (*rec_enable_button, 0, 2, 2, 3);
		rec_enable_button->set_sensitive (_session.writable());
		rec_enable_button->show();
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

	connections.push_back (_route->meter_change.connect (mem_fun(*this, &MixerStrip::meter_changed)));
	connections.push_back (_route->input_changed.connect (mem_fun(*this, &MixerStrip::input_changed)));
	connections.push_back (_route->output_changed.connect (mem_fun(*this, &MixerStrip::output_changed)));
	connections.push_back (_route->mute_changed.connect (mem_fun(*this, &RouteUI::mute_changed)));
	connections.push_back (_route->solo_changed.connect (mem_fun(*this, &RouteUI::solo_changed)));
	connections.push_back (_route->solo_safe_changed.connect (mem_fun(*this, &RouteUI::solo_changed)));
	connections.push_back (_route->mix_group_changed.connect (mem_fun(*this, &MixerStrip::mix_group_changed)));
	connections.push_back (_route->panner().Changed.connect (mem_fun(*this, &MixerStrip::connect_to_pan)));

	if (is_audio_track()) {
		connections.push_back (audio_track()->DiskstreamChanged.connect (mem_fun(*this, &MixerStrip::diskstream_changed)));
		connections.push_back (get_diskstream()->SpeedChanged.connect (mem_fun(*this, &MixerStrip::speed_changed)));
	}

	connections.push_back (_route->name_changed.connect (mem_fun(*this, &RouteUI::name_changed)));
	connections.push_back (_route->comment_changed.connect (mem_fun(*this, &MixerStrip::comment_changed)));
	connections.push_back (_route->gui_changed.connect (mem_fun(*this, &MixerStrip::route_gui_changed)));

	set_stuff_from_route ();

	/* now force an update of all the various elements */

	pre_redirect_box.update();
	post_redirect_box.update();
	mute_changed (0);
	solo_changed (0);
	name_changed (0);
	comment_changed (0);
	mix_group_changed (0);

	connect_to_pan ();

	panners.setup_pan ();

	if (is_audio_track()) {
		speed_changed ();
	}

	update_diskstream_display ();
	update_input_display ();
	update_output_display ();
}

void
MixerStrip::set_stuff_from_route ()
{
	XMLProperty *prop;

	ensure_xml_node ();

	/* if width is not set, it will be set by the MixerUI or editor */

	if ((prop = xml_node->property ("strip_width")) != 0) {
		set_width (Width (string_2_enum (prop->value(), _width)), this);
	}

	if ((prop = xml_node->property ("shown_mixer")) != 0) {
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
MixerStrip::set_width (Width w, void* owner)
{
	/* always set the gpm width again, things may be hidden */

	gpm.set_width (w);
	panners.set_width (w);
	pre_redirect_box.set_width (w);
	post_redirect_box.set_width (w);

	_width_owner = owner;

	ensure_xml_node ();
	
	_width = w;

	if (_width_owner == this) {
		xml_node->add_property ("strip_width", enum_2_string (_width));
	}

	switch (w) {
	case Wide:

		if (rec_enable_button)  {
			((Gtk::Label*)rec_enable_button->get_child())->set_text (_("Record"));
		}
		((Gtk::Label*)mute_button->get_child())->set_text  (_("Mute"));
		((Gtk::Label*)solo_button->get_child())->set_text (_("Solo"));

		if (_route->comment() == "") {
		       comment_button.unset_bg (STATE_NORMAL);
		       ((Gtk::Label*)comment_button.get_child())->set_text (_("Comments"));
		} else {
		       comment_button.modify_bg (STATE_NORMAL, color());
		       ((Gtk::Label*)comment_button.get_child())->set_text (_("*Comments*"));
		}

		((Gtk::Label*)gpm.gain_automation_style_button.get_child())->set_text (gpm.astyle_string(_route->gain_automation_curve().automation_style()));
		((Gtk::Label*)gpm.gain_automation_state_button.get_child())->set_text (gpm.astate_string(_route->gain_automation_curve().automation_state()));
		((Gtk::Label*)panners.pan_automation_style_button.get_child())->set_text (panners.astyle_string(_route->panner().automation_style()));
		((Gtk::Label*)panners.pan_automation_state_button.get_child())->set_text (panners.astate_string(_route->panner().automation_state()));
		Gtkmm2ext::set_size_request_to_display_given_text (name_button, "long", 2, 2);
		set_size_request (-1, -1);
		break;

	case Narrow:
		if (rec_enable_button) {
			((Gtk::Label*)rec_enable_button->get_child())->set_text (_("Rec"));
		}
		((Gtk::Label*)mute_button->get_child())->set_text (_("M"));
		((Gtk::Label*)solo_button->get_child())->set_text (_("S"));

		if (_route->comment() == "") {
		       comment_button.unset_bg (STATE_NORMAL);
		       ((Gtk::Label*)comment_button.get_child())->set_text (_("Cmt"));
		} else {
		       comment_button.modify_bg (STATE_NORMAL, color());
		       ((Gtk::Label*)comment_button.get_child())->set_text (_("*Cmt*"));
		}

		((Gtk::Label*)gpm.gain_automation_style_button.get_child())->set_text (gpm.short_astyle_string(_route->gain_automation_curve().automation_style()));
		((Gtk::Label*)gpm.gain_automation_state_button.get_child())->set_text (gpm.short_astate_string(_route->gain_automation_curve().automation_state()));
		((Gtk::Label*)panners.pan_automation_style_button.get_child())->set_text (panners.short_astyle_string(_route->panner().automation_style()));
		((Gtk::Label*)panners.pan_automation_state_button.get_child())->set_text (panners.short_astate_string(_route->panner().automation_state()));
		Gtkmm2ext::set_size_request_to_display_given_text (name_button, "longest label", 2, 2);
		set_size_request (max (50, gpm.get_gm_width()), -1);
		break;
	}
	update_input_display ();
	update_output_display ();
	mix_group_changed (0);
	name_changed (0);
#ifdef GTKOSX
	WidthChanged();
#endif
}

void
MixerStrip::set_packed (bool yn)
{
	_packed = yn;

	ensure_xml_node ();

	if (_packed) {
		xml_node->add_property ("shown_mixer", "yes");
	} else {
		xml_node->add_property ("shown_mixer", "no");
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
		output_menu.set_name ("ArdourContextMenu");
		citems.clear();
		
		citems.push_back (MenuElem (_("Edit"), mem_fun(*this, &MixerStrip::edit_output_configuration)));
		citems.push_back (SeparatorElem());
		citems.push_back (MenuElem (_("Disconnect"), mem_fun (*(static_cast<RouteUI*>(this)), &RouteUI::disconnect_output)));
		citems.push_back (SeparatorElem());
		
		_session.foreach_connection (this, &MixerStrip::add_connection_to_output_menu);

		output_menu.popup (1, ev->time);
		break;
		
	default:
	        break;
	}
	return TRUE;
}

void
MixerStrip::edit_output_configuration ()
{
	if (output_selector == 0) {
		output_selector = new IOSelectorWindow (_session, _route, false);
	} 

	if (output_selector->is_visible()) {
		output_selector->get_toplevel()->get_window()->raise();
	} else {
		output_selector->show_all ();
	}
}

void
MixerStrip::edit_input_configuration ()
{
	if (input_selector == 0) {
		input_selector = new IOSelectorWindow (_session, _route, true);
	} 

	if (input_selector->is_visible()) {
		input_selector->get_toplevel()->get_window()->raise();
	} else {
		input_selector->show_all ();
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
		citems.push_back (MenuElem (_("Edit"), mem_fun(*this, &MixerStrip::edit_input_configuration)));
		citems.push_back (SeparatorElem());
		citems.push_back (MenuElem (_("Disconnect"), mem_fun (*(static_cast<RouteUI*>(this)), &RouteUI::disconnect_input)));
		citems.push_back (SeparatorElem());
		
		_session.foreach_connection (this, &MixerStrip::add_connection_to_input_menu);

		input_menu.popup (1, ev->time);
		break;
		
	default:
	        break;
	}
	return TRUE;
}

void
MixerStrip::connection_input_chosen (ARDOUR::Connection *c)
{
	if (!ignore_toggle) {

		try { 
			_route->use_input_connection (*c, this);
		}

		catch (AudioEngine::PortRegistrationFailure& err) {
			error << _("could not register new ports required for that connection")
			      << endmsg;
		}
	}
}

void
MixerStrip::connection_output_chosen (ARDOUR::Connection *c)
{
	if (!ignore_toggle) {

		try { 
			_route->use_output_connection (*c, this);
		}

		catch (AudioEngine::PortRegistrationFailure& err) {
			error << _("could not register new ports required for that connection")
			      << endmsg;
		}
	}
}

void
MixerStrip::add_connection_to_input_menu (ARDOUR::Connection* c)
{
	using namespace Menu_Helpers;

	if (dynamic_cast<InputConnection *> (c) == 0) {
		return;
	}

	MenuList& citems = input_menu.items();
	
	if (c->nports() == _route->n_inputs()) {

		citems.push_back (CheckMenuElem (c->name(), bind (mem_fun(*this, &MixerStrip::connection_input_chosen), c)));
		
		ARDOUR::Connection *current = _route->input_connection();
		
		if (current == c) {
			ignore_toggle = true;
			dynamic_cast<CheckMenuItem *> (&citems.back())->set_active (true);
			ignore_toggle = false;
		}
	}
}

void
MixerStrip::add_connection_to_output_menu (ARDOUR::Connection* c)
{
	using namespace Menu_Helpers;

	if (dynamic_cast<OutputConnection *> (c) == 0) {
		return;
	}

	if (c->nports() == _route->n_outputs()) {

		MenuList& citems = output_menu.items();
		citems.push_back (CheckMenuElem (c->name(), bind (mem_fun(*this, &MixerStrip::connection_output_chosen), c)));
		
		ARDOUR::Connection *current = _route->output_connection();
		
		if (current == c) {
			ignore_toggle = true;
			dynamic_cast<CheckMenuItem *> (&citems.back())->set_active (true);
			ignore_toggle = false;
		}
	}
}

void
MixerStrip::update_diskstream_display ()
{
	map_frozen ();
	update_input_display ();

	if (is_audio_track()) {

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

	if (!_route->panner().empty()) {
		StreamPanner* sp = _route->panner().front();

		panstate_connection = sp->automation().automation_state_changed.connect (mem_fun(panners, &PannerUI::pan_automation_state_changed));
		panstyle_connection = sp->automation().automation_style_changed.connect (mem_fun(panners, &PannerUI::pan_automation_style_changed));
	}

	panners.pan_changed (this);
}

/*
 * Output port labelling
 * =====================
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
 * Tooltips
 * ========
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
MixerStrip::update_io_button (boost::shared_ptr<ARDOUR::Route> route, Width width, bool for_input)
{
	uint32_t io_count;
	uint32_t io_index;
	Port *port;
	const char **connections;
	
	uint32_t connection_index = 0;
	uint32_t total_connection_count = 0;
	uint32_t io_connection_count = 0;
	uint32_t ardour_connection_count = 0;
	uint32_t system_connection_count = 0;
	uint32_t other_connection_count = 0;

	ostringstream label;
	string label_string;
	char * label_cstr;

	bool have_label = false;
	bool each_io_has_one_connection = true;

	string connection_name;
	string ardour_track_name;
	string other_connection_type;
	string system_ports;
	string system_port;
	
	ostringstream tooltip;
	char * tooltip_cstr;
	
	tooltip << route->name();
	
	if (for_input) {
		io_count = route->n_inputs();
	} else {
		io_count = route->n_outputs();
	}    
	
	for (io_index = 0; io_index < io_count; ++io_index) {
		if (for_input) {
			port = route->input(io_index);
		} else {
			port = route->output(io_index);
		}
		
		connections = port->get_connections();
		io_connection_count = 0;
		
		if (connections) {
			for (connection_index = 0; connections[connection_index]; ++connection_index) {
				connection_name = connections[connection_index];
				
				if (connection_index == 0) {
					tooltip << endl << port->name().substr(port->name().find("/") + 1) << " -> " << connection_name;
				} else {
					tooltip << ", " << connection_name;
				}
				
				if (connection_name.find("ardour:") == 0) {
					if (ardour_track_name.empty()) {
						// "ardour:Master/in 1" -> "ardour:Master/"
						string::size_type slash = connection_name.find("/");
						if (slash != string::npos) {
							ardour_track_name = connection_name.substr(0, slash + 1);
						}
					}
					
					if (connection_name.find(ardour_track_name) == 0) {
						++ardour_connection_count;
					}
				} else if (connection_name.find("system:") == 0) {
					if (for_input) {
						// "system:capture_123" -> "123"
						system_port = connection_name.substr(15);
					} else {
						// "system:playback_123" -> "123"
						system_port = connection_name.substr(16);
					}
					
					if (system_ports.empty()) {
						system_ports += system_port;
					} else {
						system_ports += "/" + system_port;
					}
					
					++system_connection_count;
				} else {
					if (other_connection_type.empty()) {
						// "jamin:in 1" -> "jamin:"
						other_connection_type = connection_name.substr(0, connection_name.find(":") + 1);
					}
					
					if (connection_name.find(other_connection_type) == 0) {
						++other_connection_count;
					}
				}
				
				++total_connection_count;
				++io_connection_count;
			}
		} 
		
		if (io_connection_count != 1) {
			each_io_has_one_connection = false;
		}
	}
	
	if (total_connection_count == 0) {
		tooltip << endl << _("Disconnected");
	}
	
	tooltip_cstr = new char[tooltip.str().size() + 1];
	strcpy(tooltip_cstr, tooltip.str().c_str());
	
	if (for_input) {
		ARDOUR_UI::instance()->set_tip (&input_button, tooltip_cstr, "");
	} else {
		ARDOUR_UI::instance()->set_tip (&output_button, tooltip_cstr, "");
	}  
	
	if (each_io_has_one_connection) {
		if ((total_connection_count == ardour_connection_count)) {
			// all connections are to the same track in ardour
			// "ardour:Master/" -> "Master"
			string::size_type slash = ardour_track_name.find("/");
			if (slash != string::npos) {
				label << ardour_track_name.substr(7, slash - 7);
				have_label = true;
			}
		}
		else if (total_connection_count == system_connection_count) {
			// all connections are to system ports
			label << system_ports;
			have_label = true;
		}
		else if (total_connection_count == other_connection_count) {
			// all connections are to the same external program eg jamin
			// "jamin:" -> "jamin"
			label << other_connection_type.substr(0, other_connection_type.size() - 1);
			have_label = true;
		}
	}
	
	if (!have_label) {
		if (total_connection_count == 0) {
			// Disconnected
			label << "-";
		} else {
			// Odd configuration
			label << "*" << total_connection_count << "*";
		}
	}
	
	switch (width) {
	case Wide:
		label_string = label.str().substr(0, 6);
		break;
	case Narrow:
		label_string = label.str().substr(0, 3);
		break;
	}
	
	label_cstr = new char[label_string.size() + 1];
	strcpy(label_cstr, label_string.c_str());
	
	if (for_input) {
		input_label.set_text (label_cstr);
	} else {
		output_label.set_text (label_cstr);
	}
}

void
MixerStrip::update_input_display ()
{
	update_io_button (_route, _width, true);
	panners.setup_pan ();
}

void
MixerStrip::update_output_display ()
{
    update_io_button (_route, _width, false);
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
	set_width(_width, this);
}

void
MixerStrip::output_changed (IOChange change, void *src)
{
	Gtkmm2ext::UI::instance()->call_slot (mem_fun(*this, &MixerStrip::update_output_display));
	set_width(_width, this);
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
MixerStrip::set_mix_group (RouteGroup *rg)
{
	_route->set_mix_group (rg, this);
}

void
MixerStrip::add_mix_group_to_menu (RouteGroup *rg, RadioMenuItem::Group* group)
{
	using namespace Menu_Helpers;

	MenuList& items = group_menu->items();

	items.push_back (RadioMenuElem (*group, rg->name(), bind (mem_fun(*this, &MixerStrip::set_mix_group), rg)));

	if (_route->mix_group() == rg) {
		static_cast<RadioMenuItem*>(&items.back())->set_active ();
	}
}

bool
MixerStrip::select_mix_group (GdkEventButton *ev)
{
	using namespace Menu_Helpers;

	if (group_menu == 0) {
	        group_menu = new Menu;
	} 
	group_menu->set_name ("ArdourContextMenu");
	MenuList& items = group_menu->items();
	RadioMenuItem::Group group;

	switch (ev->button) {
	case 1:

		items.clear ();
		items.push_back (RadioMenuElem (group, _("No group"), bind (mem_fun(*this, &MixerStrip::set_mix_group), (RouteGroup *) 0)));

		_session.foreach_mix_group (bind (mem_fun (*this, &MixerStrip::add_mix_group_to_menu), &group));

		group_menu->popup (1, ev->time);
		break;

	default:
		break;
	}
	
	return true;
}	

void
MixerStrip::mix_group_changed (void *ignored)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &MixerStrip::mix_group_changed), ignored));
	
	RouteGroup *rg = _route->mix_group();
	
	if (rg) {
		group_label.set_text (PBD::short_version (rg->name(), 5));
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
MixerStrip::speed_adjustment_changed ()
{
	/* since there is a usable speed adjustment, there has to be a diskstream */
	if (!ignore_speed_adjustment) {
		get_diskstream()->set_speed (speed_adjustment.get_value());
	}
}

void
MixerStrip::speed_changed ()
{
	Gtkmm2ext::UI::instance()->call_slot (mem_fun(*this, &MixerStrip::update_speed_display));
}

void
MixerStrip::update_speed_display ()
{
	float val;
	
	val = get_diskstream()->speed();

	if (val != 1.0) {
		speed_spinner.set_name ("MixerStripSpeedBaseNotOne");
	} else {
		speed_spinner.set_name ("MixerStripSpeedBase");
	}

	if (speed_adjustment.get_value() != val) {
		ignore_speed_adjustment = true;
		speed_adjustment.set_value (val);
		ignore_speed_adjustment = false;
	}
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
MixerStrip::name_changed (void *src)
{
	switch (_width) {
	case Wide:
		RouteUI::name_changed (src);
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
		set_width (Narrow, this);
		break;
	case Narrow:
		set_width (Wide, this);
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
			pre_redirect_box.set_sensitive (false);
			post_redirect_box.set_sensitive (false);
			speed_spinner.set_sensitive (false);
			hide_redirect_editors ();
			break;
		default:
			pre_redirect_box.set_sensitive (true);
			post_redirect_box.set_sensitive (true);
			speed_spinner.set_sensitive (true);
			// XXX need some way, maybe, to retoggle redirect editors
			break;
		}
	}
}

void
MixerStrip::hide_redirect_editors ()
{
	_route->foreach_redirect (this, &MixerStrip::hide_redirect_editor);
}

void
MixerStrip::hide_redirect_editor (boost::shared_ptr<Redirect> redirect)
{
	void* gui = redirect->get_gui ();
	
	if (gui) {
		static_cast<Gtk::Widget*>(gui)->hide ();
	}
}

void
MixerStrip::route_active_changed ()
{
	RouteUI::route_active_changed ();

	if (is_audio_track()) {
		if (_route->active()) {
			set_name ("AudioTrackStripBase");
			gpm.set_meter_strip_name ("AudioTrackMetrics");
		} else {
			set_name ("AudioTrackStripBaseInactive");
			gpm.set_meter_strip_name ("AudioTrackMetricsInactive");
		}
		gpm.set_fader_name ("AudioTrackFader");
	} else { // FIXME: assumed audio bus
		if (_route->active()) {
			set_name ("AudioBusStripBase");
			gpm.set_meter_strip_name ("AudioBusMetrics");
		} else {
			set_name ("AudioBusStripBaseInactive");
			gpm.set_meter_strip_name ("AudioBusMetricsInactive");
		}
		gpm.set_fader_name ("AudioBusFader");
	}
}

RouteGroup*
MixerStrip::mix_group() const
{
	return _route->mix_group();
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
	set_width (_width, this);
}

