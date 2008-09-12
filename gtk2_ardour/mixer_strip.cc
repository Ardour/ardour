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
#include <ardour/processor.h>
#include <ardour/profile.h>
#include <ardour/ladspa_plugin.h>
#include <ardour/auto_bundle.h>
#include <ardour/user_bundle.h>

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

MixerStrip::MixerStrip (Mixer_UI& mx, Session& sess, boost::shared_ptr<Route> rt, bool in_mixer)
	: AxisView(sess),
	  RouteUI (rt, sess, _("Mute"), _("Solo"), _("Record")),
	  _mixer(mx),
	  _mixer_owned (in_mixer),
	  pre_processor_box (PreFader, sess, rt, mx.plugin_selector(), mx.selection(), in_mixer),
	  post_processor_box (PostFader, sess, rt, mx.plugin_selector(), mx.selection(), in_mixer),
	  gpm (_route, sess),
	  panners (_route, sess),
	  button_table (3, 2),
	  middle_button_table (1, 2),
	  bottom_button_table (1, 2),
	  meter_point_label (_("pre")),
	  comment_button (_("Comments")),
	  speed_adjustment (1.0, 0.001, 4.0, 0.001, 0.1),
	  speed_spinner (&speed_adjustment, "MixerStripSpeedBase", true)

{
	if (set_color_from_route()) {
		set_color (unique_random_color());
	}

	input_selector = 0;
	output_selector = 0;
	group_menu = 0;
	if (!_route->is_hidden()) {
		_marked_for_display = true;
	}
	route_ops_menu = 0;
	ignore_comment_edit = false;
	ignore_toggle = false;
	ignore_speed_adjustment = false;
	comment_window = 0;
	comment_area = 0;
	_width_owner = 0;

	Gtk::Image *width_icon = manage (new Gtk::Image (::get_icon("strip_width")));
	Gtk::Image *hide_icon = manage (new Gtk::Image (::get_icon("hide")));
	width_button.add (*width_icon);
	hide_button.add (*hide_icon);

	input_label.set_text (_("Input"));
	input_button.add (input_label);
	input_button.set_name ("MixerIOButton");
	input_label.set_name ("MixerIOButtonLabel");

	output_label.set_text (_("Output"));
	output_button.add (output_label);
	output_button.set_name ("MixerIOButton");
	output_label.set_name ("MixerIOButtonLabel");

	_route->meter_change.connect (mem_fun(*this, &MixerStrip::meter_changed));
	meter_point_button.add (meter_point_label);
	meter_point_button.set_name ("MixerStripMeterPreButton");
	meter_point_label.set_name ("MixerStripMeterPreButton");
	
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
	
	if (is_audio_track()) {
		boost::shared_ptr<AudioTrack> at = audio_track();

		at->FreezeChange.connect (mem_fun(*this, &MixerStrip::map_frozen));

#ifdef VARISPEED_IN_MIXER_STRIP
		speed_adjustment.signal_value_changed().connect (mem_fun(*this, &MixerStrip::speed_adjustment_changed));
		
		speed_frame.set_name ("BaseFrame");
		speed_frame.set_shadow_type (Gtk::SHADOW_IN);
		speed_frame.add (speed_spinner);
		
		speed_spinner.set_print_func (speed_printer, 0);

		ARDOUR_UI::instance()->tooltips().set_tip (speed_spinner, _("Varispeed"));

		button_table.attach (speed_frame, 0, 2, 5, 6);
#endif /* VARISPEED_IN_MIXER_STRIP */

	}
	
	if(rec_enable_button) {
		rec_enable_button->signal_button_press_event().connect (mem_fun(*this, &RouteUI::rec_enable_press), false);
		rec_enable_button->signal_button_release_event().connect (mem_fun(*this, &RouteUI::rec_enable_release));
	
		rec_enable_button->set_name ("MixerRecordEnableButton");
		button_table.attach (*rec_enable_button, 0, 2, 2, 3);
	}
	
	name_button.add (name_label);
	name_button.set_name ("MixerNameButton");
	Gtkmm2ext::set_size_request_to_display_given_text (name_button, "longest label", 2, 2);

	name_label.set_name ("MixerNameButtonLabel");
	if (_route->phase_invert()) {
	        name_label.set_text (X_("Ø ") + name_label.get_text());
	} else {
	        name_label.set_text (_route->name());
	}

	group_button.add (group_label);
	group_button.set_name ("MixerGroupButton");
	group_label.set_name ("MixerGroupButtonLabel");

	comment_button.set_name ("MixerCommentButton");

	ARDOUR_UI::instance()->tooltips().set_tip (comment_button, _route->comment()==""	?
							_("Click to Add/Edit Comments"):
							_route->comment());

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
	global_vpacker.pack_start (pre_processor_box, true, true);
	global_vpacker.pack_start (middle_button_table,Gtk::PACK_SHRINK);
	global_vpacker.pack_start (*gain_meter_alignment,Gtk::PACK_SHRINK);
	global_vpacker.pack_start (bottom_button_table,Gtk::PACK_SHRINK);
	global_vpacker.pack_start (post_processor_box, true, true);
	if (!is_midi_track()) {
		global_vpacker.pack_start (panners, Gtk::PACK_SHRINK);
	}
	global_vpacker.pack_start (output_button, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (comment_button, Gtk::PACK_SHRINK);

	if (route()->is_master() || route()->is_control()) {
		
		if (scrollbar_height == 0) {
			HScrollbar scrollbar;
			Gtk::Requisition requisition(scrollbar.size_request ());
			scrollbar_height = requisition.height;
		}

		EventBox* spacer = manage (new EventBox);
		spacer->set_size_request (-1, scrollbar_height);
		global_vpacker.pack_start (*spacer, false, false);
	}

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
	_route->input_changed.connect (mem_fun(*this, &MixerStrip::input_changed));
	_route->output_changed.connect (mem_fun(*this, &MixerStrip::output_changed));
	_route->mute_changed.connect (mem_fun(*this, &RouteUI::mute_changed));
	_route->solo_changed.connect (mem_fun(*this, &RouteUI::solo_changed));
	_route->solo_safe_changed.connect (mem_fun(*this, &RouteUI::solo_changed));
	_route->mix_group_changed.connect (mem_fun(*this, &MixerStrip::mix_group_changed));
	_route->panner().Changed.connect (mem_fun(*this, &MixerStrip::connect_to_pan));

	if (is_audio_track()) {
		audio_track()->DiskstreamChanged.connect (mem_fun(*this, &MixerStrip::diskstream_changed));
		get_diskstream()->SpeedChanged.connect (mem_fun(*this, &MixerStrip::speed_changed));
	}

	_route->NameChanged.connect (mem_fun(*this, &RouteUI::name_changed));
	_route->comment_changed.connect (mem_fun(*this, &MixerStrip::comment_changed));
	_route->gui_changed.connect (mem_fun(*this, &MixerStrip::route_gui_changed));

	input_button.signal_button_press_event().connect (mem_fun(*this, &MixerStrip::input_press), false);
	output_button.signal_button_press_event().connect (mem_fun(*this, &MixerStrip::output_press), false);

	solo_button->signal_button_press_event().connect (mem_fun(*this, &RouteUI::solo_press), false);
	solo_button->signal_button_release_event().connect (mem_fun(*this, &RouteUI::solo_release), false);
	mute_button->signal_button_press_event().connect (mem_fun(*this, &RouteUI::mute_press), false);
	mute_button->signal_button_release_event().connect (mem_fun(*this, &RouteUI::mute_release), false);

	name_button.signal_button_press_event().connect (mem_fun(*this, &MixerStrip::name_button_button_press), false);
	group_button.signal_button_press_event().connect (mem_fun(*this, &MixerStrip::select_mix_group), false);

	_width = (Width) -1;
	set_stuff_from_route ();

	/* start off as a passthru strip. we'll correct this, if necessary,
	   in update_diskstream_display().
	*/

	if (is_midi_track())
		set_name ("MidiTrackStripBase");
	else
		set_name ("AudioTrackStripBase");

	/* now force an update of all the various elements */

	pre_processor_box.update();
	post_processor_box.update();
	mute_changed (0);
	solo_changed (0);
	name_changed ();
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

	add_events (Gdk::BUTTON_RELEASE_MASK);

	whvbox->show();
	hide_icon->show();
	width_icon->show();
	gain_meter_alignment->show_all();

	pre_processor_box.show();

	if (!route()->is_master() && !route()->is_control()) {
		/* we don't allow master or control routes to be hidden */
		hide_button.show();
	}
	width_button.show();
	width_hide_box.show();
	global_frame.show();
	global_vpacker.show();
	button_table.show();
	middle_button_table.show();
	bottom_button_table.show();
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
	speed_spinner.show();
	speed_label.show();
	speed_frame.show();

	show();
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
	pre_processor_box.set_width (w);
	post_processor_box.set_width (w);

	boost::shared_ptr<AutomationList> gain_automation = _route->gain_control()->list();

	_width_owner = owner;

	ensure_xml_node ();
	
	_width = w;

	if (_width_owner == this) {
		xml_node->add_property ("strip_width", enum_2_string (_width));
	}

	switch (w) {
	case Wide:
		set_size_request (-1, -1);
		
		if (rec_enable_button)  {
			((Gtk::Label*)rec_enable_button->get_child())->set_text (_("record"));
		}
		((Gtk::Label*)mute_button->get_child())->set_text  (_("Mute"));
		((Gtk::Label*)solo_button->get_child())->set_text (_("Solo"));

		if (_route->comment() == "") {
		       comment_button.unset_bg (STATE_NORMAL);
		       ((Gtk::Label*)comment_button.get_child())->set_text (_("comments"));
		} else {
		       comment_button.modify_bg (STATE_NORMAL, color());
		       ((Gtk::Label*)comment_button.get_child())->set_text (_("*comments*"));
		}

		((Gtk::Label*)gpm.gain_automation_style_button.get_child())->set_text (gpm.astyle_string(gain_automation->automation_style()));
		((Gtk::Label*)gpm.gain_automation_state_button.get_child())->set_text (gpm.astate_string(gain_automation->automation_state()));
		((Gtk::Label*)panners.pan_automation_style_button.get_child())->set_text (panners.astyle_string(_route->panner().automation_style()));
		((Gtk::Label*)panners.pan_automation_state_button.get_child())->set_text (panners.astate_string(_route->panner().automation_state()));
		Gtkmm2ext::set_size_request_to_display_given_text (name_button, "long", 2, 2);
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

		((Gtk::Label*)gpm.gain_automation_style_button.get_child())->set_text (gpm.short_astyle_string(gain_automation->automation_style()));
		((Gtk::Label*)gpm.gain_automation_state_button.get_child())->set_text (gpm.short_astate_string(gain_automation->automation_state()));
		((Gtk::Label*)panners.pan_automation_style_button.get_child())->set_text (panners.short_astyle_string(_route->panner().automation_style()));
		((Gtk::Label*)panners.pan_automation_state_button.get_child())->set_text (panners.short_astate_string(_route->panner().automation_state()));
		Gtkmm2ext::set_size_request_to_display_given_text (name_button, "longest label", 2, 2);
		set_size_request (max (50, gpm.get_gm_width()), -1);
		break;
	}

	update_input_display ();
	update_output_display ();
	mix_group_changed (0);
	name_changed ();

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
	{
		output_menu.set_name ("ArdourContextMenu");
		citems.clear();
		
		citems.push_back (MenuElem (_("Edit"), mem_fun(*this, &MixerStrip::edit_output_configuration)));
		citems.push_back (SeparatorElem());
		citems.push_back (MenuElem (_("Disconnect"), mem_fun (*(static_cast<RouteUI*>(this)), &RouteUI::disconnect_output)));
		citems.push_back (SeparatorElem());

		std::vector<boost::shared_ptr<Bundle> > current = _route->bundles_connected_to_outputs ();

 		_session.foreach_bundle (
			bind (mem_fun (*this, &MixerStrip::add_bundle_to_output_menu), current)
 			);

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
		output_selector = new IOSelectorWindow (_session, _route, false);
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
		input_selector = new IOSelectorWindow (_session, _route, true);
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
	{
		citems.push_back (MenuElem (_("Edit"), mem_fun(*this, &MixerStrip::edit_input_configuration)));
		citems.push_back (SeparatorElem());
		citems.push_back (MenuElem (_("Disconnect"), mem_fun (*(static_cast<RouteUI*>(this)), &RouteUI::disconnect_input)));
		citems.push_back (SeparatorElem());

		std::vector<boost::shared_ptr<Bundle> > current = _route->bundles_connected_to_inputs ();

		_session.foreach_bundle (
			bind (mem_fun (*this, &MixerStrip::add_bundle_to_input_menu), current)
			);

		input_menu.popup (1, ev->time);
		break;
	}
	default:
	        break;
	}
	return TRUE;
}

void
MixerStrip::bundle_input_chosen (boost::shared_ptr<ARDOUR::Bundle> c)
{
	if (!ignore_toggle) {

		try { 
			_route->connect_input_ports_to_bundle (c, this);
		}

		catch (AudioEngine::PortRegistrationFailure& err) {
			error << _("could not register new ports required for that bundle")
			      << endmsg;
		}
	}
}

void
MixerStrip::bundle_output_chosen (boost::shared_ptr<ARDOUR::Bundle> c)
{
	if (!ignore_toggle) {

		try { 
			_route->connect_output_ports_to_bundle (c, this);
		}

		catch (AudioEngine::PortRegistrationFailure& err) {
			error << _("could not register new ports required for that bundle")
			      << endmsg;
		}
	}
}

void
MixerStrip::add_bundle_to_input_menu (boost::shared_ptr<Bundle> b, std::vector<boost::shared_ptr<Bundle> > const & current)
{
	using namespace Menu_Helpers;

	/* the input menu needs to contain only output bundles (that we
	   can connect inputs to */
 	if (b->ports_are_outputs() == false) {
 		return;
 	}

	MenuList& citems = input_menu.items();
	
	if (b->nchannels() == _route->n_inputs()) {

		citems.push_back (CheckMenuElem (b->name(), bind (mem_fun(*this, &MixerStrip::bundle_input_chosen), b)));

		if (std::find (current.begin(), current.end(), b) != current.end()) {
			ignore_toggle = true;
			dynamic_cast<CheckMenuItem *> (&citems.back())->set_active (true);
			ignore_toggle = false;
		}
	}
}

void
MixerStrip::add_bundle_to_output_menu (boost::shared_ptr<Bundle> b, std::vector<boost::shared_ptr<Bundle> > const & current)
{
	using namespace Menu_Helpers;

	/* the output menu needs to contain only input bundles (that we
	   can connect outputs to */
 	if (b->ports_are_inputs() == false) {
 		return;
 	}

	if (b->nchannels() == _route->n_outputs()) {

		MenuList& citems = output_menu.items();
		citems.push_back (CheckMenuElem (b->name(), bind (mem_fun(*this, &MixerStrip::bundle_output_chosen), b)));
		
		if (std::find (current.begin(), current.end(), b) != current.end()) {
			ignore_toggle = true;
			dynamic_cast<CheckMenuItem *> (&citems.back())->set_active (true);
			ignore_toggle = false;
		}
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

	if (!_route->panner().empty()) {
		StreamPanner* sp = _route->panner().front();

		panstate_connection = sp->pan_control()->list()->automation_state_changed.connect (mem_fun(panners, &PannerUI::pan_automation_state_changed));
		panstyle_connection = sp->pan_control()->list()->automation_style_changed.connect (mem_fun(panners, &PannerUI::pan_automation_style_changed));
	}

	panners.pan_changed (this);
}

void
MixerStrip::update_input_display ()
{
	std::vector<boost::shared_ptr<ARDOUR::Bundle> > c = _route->bundles_connected_to_inputs ();

	/* XXX: how do we represent >1 connected bundle? */
	if (c.empty() == false) {
		input_label.set_text (c[0]->name());
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
	std::vector<boost::shared_ptr<ARDOUR::Bundle> > c = _route->bundles_connected_to_outputs ();

	/* XXX: how do we represent >1 connected bundle? */
	if (c.empty() == false) {
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
		group_label.set_text (rg->name());
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
	route_ops_menu = manage (new Menu);
	route_ops_menu->set_name ("ArdourContextMenu");

	MenuList& items = route_ops_menu->items();

	items.push_back (MenuElem (_("Rename"), mem_fun(*this, &RouteUI::route_rename)));
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

	build_remote_control_menu ();
	
	items.push_back (SeparatorElem());
	if (!Profile->get_sae()) {
              items.push_back (MenuElem (_("Remote Control ID"), *remote_control_menu));
        }

	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Remove"), mem_fun(*this, &RouteUI::remove_this_route)));
}

gint
MixerStrip::name_button_button_press (GdkEventButton* ev)
{
	if (ev->button == 1) {
		list_route_operations ();

		Menu_Helpers::MenuList& items = route_ops_menu->items();
		/* do not allow rename if the track is record-enabled */
		static_cast<MenuItem*> (&items.front())->set_sensitive (!_route->record_enabled());

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
			pre_processor_box.set_sensitive (false);
			post_processor_box.set_sensitive (false);
			speed_spinner.set_sensitive (false);
			break;
		default:
			pre_processor_box.set_sensitive (true);
			post_processor_box.set_sensitive (true);
			speed_spinner.set_sensitive (true);
			// XXX need some way, maybe, to retoggle redirect editors
			break;
		}
	}
	
	hide_redirect_editors ();
}

void
MixerStrip::hide_redirect_editors ()
{
	_route->foreach_processor (this, &MixerStrip::hide_processor_editor);
}

void
MixerStrip::hide_processor_editor (boost::shared_ptr<Processor> processor)
{
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
		set_width(_width, this);
}

