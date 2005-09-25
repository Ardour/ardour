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

    $Id$
*/

#include <cmath>
#include <glib.h>

#include <sigc++/bind.h>

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/choice.h>
#include <gtkmm2ext/slider_controller.h>
#include <gtkmm2ext/stop_signal.h>
#include <gtkmm2ext/bindable_button.h>
#include <gtkmm2ext/doi.h>

#include <ardour/ardour.h>
#include <ardour/session.h>
#include <ardour/audioengine.h>
#include <ardour/route.h>
#include <ardour/audio_track.h>
#include <ardour/diskstream.h>
#include <ardour/panner.h>
#include <ardour/send.h>
#include <ardour/insert.h>
#include <ardour/ladspa_plugin.h>
#include <ardour/connection.h>
#include <ardour/session_connection.h>

#include "ardour_ui.h"
#include "ardour_dialog.h"
#include "ardour_message.h"
#include "mixer_strip.h"
#include "mixer_ui.h"
#include "keyboard.h"
#include "plugin_selector.h"
#include "public_editor.h"

#include "plugin_ui.h"
#include "send_ui.h"
#include "io_selector.h"
#include "utils.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace sigc;
using namespace ARDOUR;
using namespace Gtk;
using namespace Gtkmm2ext;

/* XPM */
static const gchar * small_x_xpm[] = {
"11 11 2 1",
" 	c None",
".	c #cccc99",
"           ",
"           ",
"  .     .  ",
"   .   .   ",
"    . .    ",
"     .     ",
"    . .    ",
"   .   .   ",
"  .     .  ",
"           ",
"           "};

/* XPM */
static const gchar * lr_xpm[] = {
"11 11 2 1",
" 	c None",
".	c #cccc99",
"           ",
"           ",
"   .   .   ",
"  .     .  ",
" .       . ",
"...........",
" .       . ",
"  .     .  ",
"   .   .   ",
"           ",
"           "};

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

MixerStrip::MixerStrip (Mixer_UI& mx, Session& sess, Route& rt, bool in_mixer)
	: AxisView(sess),
	  RouteUI (rt, sess, _("mute"), _("solo"), _("RECORD")),
	  _mixer(mx),
	  pre_redirect_box (PreFader, sess, rt, mx.plugin_selector(), mx.selection(), in_mixer),
	  post_redirect_box (PostFader, sess, rt, mx.plugin_selector(), mx.selection(), in_mixer),
	  gpm (_route, sess),
	  panners (_route, sess),
	  button_table (8, 2),
	  gain_automation_style_button (""),
	  gain_automation_state_button (""),
	  pan_automation_style_button (""),
	  pan_automation_state_button (""),
	  polarity_button (_("polarity")),
	  comment_button (_("comments")),
	  speed_adjustment (1.0, 0.001, 4.0, 0.001, 0.1),
	  speed_spinner (&speed_adjustment, "MixerStripSpeedBase", true)

{
	if (set_color_from_route()) {
		set_color (unique_random_color());
	}

	input_selector = 0;
	output_selector = 0;
	group_menu = 0;
	_marked_for_display = false;
	route_ops_menu = 0;
	ignore_comment_edit = false;
	ignore_toggle = false;
	ignore_speed_adjustment = false;
	comment_window = 0;

	width_button.add (*(manage (new Gtk::Image (lr_xpm))));
	hide_button.add (*(manage (new Gtk::Image (small_x_xpm))));


	input_label.set_text (_("INPUT"));
	input_button.add (input_label);
	input_button.set_name ("MixerIOButton");
	input_label.set_name ("MixerIOButtonLabel");

	output_label.set_text (_("OUTPUT"));
	output_button.add (output_label);
	output_button.set_name ("MixerIOButton");
	output_label.set_name ("MixerIOButtonLabel");

	rec_enable_button->set_name ("MixerRecordEnableButton");
	rec_enable_button->unset_flags (Gtk::CAN_FOCUS);

	solo_button->set_name ("MixerSoloButton");
	mute_button->set_name ("MixerMuteButton");
	gain_automation_style_button.set_name ("MixerAutomationModeButton");
	gain_automation_state_button.set_name ("MixerAutomationPlaybackButton");
	pan_automation_style_button.set_name ("MixerAutomationModeButton");
	pan_automation_state_button.set_name ("MixerAutomationPlaybackButton");
	polarity_button.set_name ("MixerPhaseInvertButton");

	ARDOUR_UI::instance()->tooltips().set_tip (pan_automation_state_button, _("Pan automation mode"));
	ARDOUR_UI::instance()->tooltips().set_tip (gain_automation_state_button, _("Gain automation mode"));

	ARDOUR_UI::instance()->tooltips().set_tip (pan_automation_style_button, _("Pan automation type"));
	ARDOUR_UI::instance()->tooltips().set_tip (gain_automation_style_button, _("Gain automation type"));

	hide_button.set_events (hide_button.get_events() & ~(GDK_ENTER_NOTIFY_MASK|GDK_LEAVE_NOTIFY_MASK));

	width_button.unset_flags (Gtk::CAN_FOCUS);
	hide_button.unset_flags (Gtk::CAN_FOCUS);
	input_button.unset_flags (Gtk::CAN_FOCUS);
	output_button.unset_flags (Gtk::CAN_FOCUS);
	solo_button->unset_flags (Gtk::CAN_FOCUS);
	mute_button->unset_flags (Gtk::CAN_FOCUS);
	gain_automation_style_button.unset_flags (Gtk::CAN_FOCUS);
	gain_automation_state_button.unset_flags (Gtk::CAN_FOCUS);
	pan_automation_style_button.unset_flags (Gtk::CAN_FOCUS);
	pan_automation_state_button.unset_flags (Gtk::CAN_FOCUS);
	polarity_button.unset_flags (Gtk::CAN_FOCUS);

	button_table.set_homogeneous (true);

	button_table.attach (name_button, 0, 2, 0, 1);
	button_table.attach (group_button, 0, 2, 1, 2);
	button_table.attach (input_button, 0, 2, 2, 3);

	button_table.attach (polarity_button, 0, 2, 3, 4);

	button_table.attach (*solo_button, 0, 1, 4, 5);
	button_table.attach (*mute_button, 1, 2, 4, 5);

	// button_table.attach (gain_automation_style_button, 0, 1, 5, 6);
	button_table.attach (gain_automation_state_button, 0, 1, 5, 6);
	// button_table.attach (pan_automation_style_button, 0, 1, 6, 7);
	button_table.attach (pan_automation_state_button, 1, 2, 5, 6);

	using namespace Menu_Helpers;
	
	gain_astate_menu.items().push_back (MenuElem (_("off"), 
						      bind (mem_fun (_route, &IO::set_gain_automation_state), (AutoState) Off)));
	gain_astate_menu.items().push_back (MenuElem (_("play"),
						      bind (mem_fun (_route, &IO::set_gain_automation_state), (AutoState) Play)));
	gain_astate_menu.items().push_back (MenuElem (_("write"),
						      bind (mem_fun (_route, &IO::set_gain_automation_state), (AutoState) Write)));
	gain_astate_menu.items().push_back (MenuElem (_("touch"),
						      bind (mem_fun (_route, &IO::set_gain_automation_state), (AutoState) Touch)));
	
	gain_astyle_menu.items().push_back (MenuElem (_("trim")));
	gain_astyle_menu.items().push_back (MenuElem (_("abs")));

	pan_astate_menu.items().push_back (MenuElem (_("off"), 
						     bind (mem_fun (_route.panner(), &Panner::set_automation_state), (AutoState) Off)));
	pan_astate_menu.items().push_back (MenuElem (_("play"),
						     bind (mem_fun (_route.panner(), &Panner::set_automation_state), (AutoState) Play)));
	pan_astate_menu.items().push_back (MenuElem (_("write"),
						     bind (mem_fun (_route.panner(), &Panner::set_automation_state), (AutoState) Write)));
	pan_astate_menu.items().push_back (MenuElem (_("touch"),
						     bind (mem_fun (_route.panner(), &Panner::set_automation_state), (AutoState) Touch)));

	pan_astyle_menu.items().push_back (MenuElem (_("trim")));
	pan_astyle_menu.items().push_back (MenuElem (_("abs")));
	
	gain_astate_menu.set_name ("ArdourContextMenu");
	gain_astyle_menu.set_name ("ArdourContextMenu");
	pan_astate_menu.set_name ("ArdourContextMenu");
	pan_astyle_menu.set_name ("ArdourContextMenu");

	ARDOUR_UI::instance()->tooltips().set_tip (gain_automation_style_button, _("gain automation mode"));
	ARDOUR_UI::instance()->tooltips().set_tip (pan_automation_style_button, _("pan automation mode"));
	ARDOUR_UI::instance()->tooltips().set_tip (gain_automation_state_button, _("gain automation state"));
	ARDOUR_UI::instance()->tooltips().set_tip (pan_automation_state_button, _("pan automation state"));

	if (is_audio_track()) {
		
		AudioTrack* at = dynamic_cast<AudioTrack*>(&_route);

		at->FreezeChange.connect (mem_fun(*this, &MixerStrip::map_frozen));

		speed_adjustment.value_changed.connect (mem_fun(*this, &MixerStrip::speed_adjustment_changed));
		
		speed_frame.set_name ("BaseFrame");
		speed_frame.set_shadow_type (Gtk::SHADOW_IN);
		speed_frame.add (speed_spinner);
		
		speed_spinner.set_print_func (speed_printer, 0);

		ARDOUR_UI::instance()->tooltips().set_tip (speed_spinner, _("varispeed"));

		speed_spinner.show ();
		speed_frame.show  ();

		button_table.attach (speed_frame, 0, 2, 6, 7);
		button_table.attach (*rec_enable_button, 0, 2, 7, 8);
	}
	
	name_button.add (name_label);
	name_button.set_name ("MixerNameButton");
	Gtkmm2ext::set_size_request_to_display_given_text (name_button, "longest label", 2, 2);

	name_label.set_name ("MixerNameButtonLabel");
	name_label.set_text (_route.name());

	group_button.add (group_label);
	group_button.set_name ("MixerGroupButton");
	group_label.set_name ("MixerGroupButtonLabel");

	comment_button.set_name ("MixerCommentButton");
	ARDOUR_UI::instance()->tooltips().set_tip (comment_button, _route.comment()==""	?
							_("click to add/edit comments"):
							_route.comment());
	comment_button.signal_clicked().connect (mem_fun(*this, &MixerStrip::comment_button_clicked));
	
	global_vpacker.set_border_width (4);
	global_vpacker.set_spacing (4);

	Gtk::VBox *whvbox = manage (new Gtk::VBox);

	width_button.set_name ("MixerWidthButton");
	hide_button.set_name ("MixerHideButton");

	width_button.signal_clicked().connect (mem_fun(*this, &MixerStrip::width_clicked));
	hide_button.signal_clicked().connect (mem_fun(*this, &MixerStrip::hide_clicked));

	width_hide_box.pack_start (width_button, false, true);
	width_hide_box.pack_end (hide_button, false, true);

	whvbox->pack_start (width_hide_box, true, true);

	global_vpacker.pack_start (*whvbox, false, false);
	global_vpacker.pack_start (button_table, false, false);
	global_vpacker.pack_start (pre_redirect_box, true, true);
	global_vpacker.pack_start (gpm, false, false);
	global_vpacker.pack_start (post_redirect_box, true, true);
	global_vpacker.pack_start (panners, false, false);
	global_vpacker.pack_start (output_button, false, false);
	global_vpacker.pack_start (comment_button, false, false);

	global_frame.add (global_vpacker);
	global_frame.set_shadow_type (Gtk::SHADOW_IN);
	global_frame.set_name ("BaseFrame");

	add (global_frame);

	/* force setting of visible selected status */

	_selected = true;
	set_selected (false);

	whvbox->show_all ();
	name_label.show ();
	group_label.show();
	input_label.show ();
	output_label.show ();
	pre_redirect_box.show_all ();
	post_redirect_box.show_all ();
	button_table.show ();
	comment_button.show ();
	name_button.show ();
	input_button.show ();
	group_button.show ();
	output_button.show ();
	rec_enable_button->show ();
	solo_button->show ();
	mute_button->show ();
	gain_automation_style_button.show ();
	gain_automation_state_button.show ();
	pan_automation_style_button.show ();
	pan_automation_state_button.show ();
	polarity_button.show ();
	global_vpacker.show ();
	global_frame.show ();

	_packed = false;
	_embedded = false;

	_route.input_changed.connect (mem_fun(*this, &MixerStrip::input_changed));
	_route.output_changed.connect (mem_fun(*this, &MixerStrip::output_changed));
	_route.mute_changed.connect (mem_fun(*this, &RouteUI::mute_changed));
	_route.solo_changed.connect (mem_fun(*this, &RouteUI::solo_changed));
	_route.solo_safe_changed.connect (mem_fun(*this, &RouteUI::solo_changed));
	_route.mix_group_changed.connect (mem_fun(*this, &MixerStrip::mix_group_changed));
	_route.gain_automation_curve().automation_state_changed.connect (mem_fun(*this, &MixerStrip::gain_automation_state_changed));
	_route.gain_automation_curve().automation_style_changed.connect (mem_fun(*this, &MixerStrip::gain_automation_style_changed));
	_route.panner().Changed.connect (mem_fun(*this, &MixerStrip::connect_to_pan));

	if (is_audio_track()) {
		audio_track()->diskstream_changed.connect (mem_fun(*this, &MixerStrip::diskstream_changed));
		get_diskstream()->speed_changed.connect (mem_fun(*this, &MixerStrip::speed_changed));
	}

	_route.name_changed.connect (mem_fun(*this, &RouteUI::name_changed));
	_route.comment_changed.connect (mem_fun(*this, &MixerStrip::comment_changed));
	_route.gui_changed.connect (mem_fun(*this, &MixerStrip::route_gui_changed));

	input_button.button_release_event.connect (mem_fun(*this, &MixerStrip::input_press));
	output_button.button_release_event.connect (mem_fun(*this, &MixerStrip::output_press));

	rec_enable_button->button_press_event.connect (mem_fun(*this, &RouteUI::rec_enable_press));
	solo_button->button_press_event.connect (mem_fun(*this, &RouteUI::solo_press));
	solo_button->button_release_event.connect (mem_fun(*this, &RouteUI::solo_release));
	mute_button->button_press_event.connect (mem_fun(*this, &RouteUI::mute_press));
	mute_button->button_release_event.connect (mem_fun(*this, &RouteUI::mute_release));

	gain_automation_style_button.button_press_event.connect_after (ptr_fun (do_not_propagate));
	pan_automation_style_button.button_press_event.connect_after (ptr_fun (do_not_propagate));
	gain_automation_state_button.button_press_event.connect_after (ptr_fun (do_not_propagate));
	pan_automation_state_button.button_press_event.connect_after (ptr_fun (do_not_propagate));

	gain_automation_style_button.button_press_event.connect (mem_fun(*this, &MixerStrip::gain_automation_style_button_event));
	gain_automation_style_button.button_release_event.connect (mem_fun(*this, &MixerStrip::gain_automation_style_button_event));
	pan_automation_style_button.button_press_event.connect (mem_fun(*this, &MixerStrip::pan_automation_style_button_event));
	pan_automation_style_button.button_release_event.connect (mem_fun(*this, &MixerStrip::pan_automation_style_button_event));

	gain_automation_state_button.button_press_event.connect (mem_fun(*this, &MixerStrip::gain_automation_state_button_event));
	gain_automation_state_button.button_release_event.connect (mem_fun(*this, &MixerStrip::gain_automation_state_button_event));
	pan_automation_state_button.button_press_event.connect (mem_fun(*this, &MixerStrip::pan_automation_state_button_event));
	pan_automation_state_button.button_release_event.connect (mem_fun(*this, &MixerStrip::pan_automation_state_button_event));

	polarity_button.toggled.connect (mem_fun(*this, &MixerStrip::polarity_toggled));

	name_button.button_release_event.connect (mem_fun(*this, &MixerStrip::name_button_button_release));

	group_button.button_press_event.connect (mem_fun(*this, &MixerStrip::select_mix_group));

	_width = (Width) -1;
	set_stuff_from_route ();

	/* start off as a passthru strip. we'll correct this, if necessary,
	   in update_diskstream_display().
	*/

	set_name ("AudioTrackStripBase");

	/* now force an update of all the various elements */

	pre_redirect_box.update();
	post_redirect_box.update();
	mute_changed (0);
	solo_changed (0);
	name_changed (0);
	comment_changed (0);
	mix_group_changed (0);
	gain_automation_state_changed ();
	pan_automation_state_changed ();
	connect_to_pan ();

	panners.setup_pan ();

	if (is_audio_track()) {
		speed_changed ();
	}

	/* XXX hack: no phase invert changed signal */

	polarity_button.set_active (_route.phase_invert());

	update_diskstream_display ();
	update_input_display ();
	update_output_display ();

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
}

void
MixerStrip::set_stuff_from_route ()
{
	XMLProperty *prop;
	
	ensure_xml_node ();

	if ((prop = xml_node->property ("strip_width")) != 0) {
		if (prop->value() == "wide") {
			set_width (Wide);
		} else if (prop->value() == "narrow") {
			set_width (Narrow);
		}
		else {
			error << compose(_("unknown strip width \"%1\" in XML GUI information"), prop->value()) << endmsg;
			set_width (Wide);
		}
	}
	else {
		set_width (Wide);
	}

	if ((prop = xml_node->property ("shown_mixer")) != 0) {
		if (prop->value() == "no") {
			_marked_for_display = false;
		} else {
			_marked_for_display = true;
		}
	}
	else {
		/* backwards compatibility */
		_marked_for_display = true;
	}
}

void
MixerStrip::set_width (Width w)
{
	/* always set the gpm width again, things may be hidden */
	gpm.set_width (w);
	panners.set_width (w);
	pre_redirect_box.set_width (w);
	post_redirect_box.set_width (w);
	
	if (_width == w) {
		return;
	}

	ensure_xml_node ();
	
	_width = w;

	switch (w) {
	case Wide:
		set_size_request (-1, -1);
		xml_node->add_property ("strip_width", "wide");

		static_cast<Gtk::Label*> (rec_enable_button->get_child())->set_text (_("RECORD"));
		static_cast<Gtk::Label*> (mute_button->get_child())->set_text (_("mute"));
		static_cast<Gtk::Label*> (solo_button->get_child())->set_text (_("solo"));
		static_cast<Gtk::Label*> (comment_button.get_child())->set_text (_("comments"));
		static_cast<Gtk::Label*> (gain_automation_style_button.get_child())->set_text (astyle_string(_route.gain_automation_curve().automation_style()));
		static_cast<Gtk::Label*> (gain_automation_state_button.get_child())->set_text (astate_string(_route.gain_automation_curve().automation_state()));
		static_cast<Gtk::Label*> (pan_automation_style_button.get_child())->set_text (astyle_string(_route.panner().automation_style()));
		static_cast<Gtk::Label*> (pan_automation_state_button.get_child())->set_text (astate_string(_route.panner().automation_state()));
		static_cast<Gtk::Label*> (polarity_button.get_child())->set_text (_("polarity"));
		Gtkmm2ext::set_size_request_to_display_given_text (name_button, "long", 2, 2);
		break;

	case Narrow:
		set_size_request (50, -1);
		xml_node->add_property ("strip_width", "narrow");

		static_cast<Gtk::Label*> (rec_enable_button->get_child())->set_text (_("REC"));
		static_cast<Gtk::Label*> (mute_button->get_child())->set_text (_("m"));
		static_cast<Gtk::Label*> (solo_button->get_child())->set_text (_("s"));
		static_cast<Gtk::Label*> (comment_button.get_child())->set_text (_("cmt"));
		static_cast<Gtk::Label*> (gain_automation_style_button.get_child())->set_text (short_astyle_string(_route.gain_automation_curve().automation_style()));
		static_cast<Gtk::Label*> (gain_automation_state_button.get_child())->set_text (short_astate_string(_route.gain_automation_curve().automation_state()));
		static_cast<Gtk::Label*> (pan_automation_style_button.get_child())->set_text (short_astyle_string(_route.panner().automation_style()));
		static_cast<Gtk::Label*> (pan_automation_state_button.get_child())->set_text (short_astate_string(_route.panner().automation_state()));
		static_cast<Gtk::Label*> (polarity_button.get_child())->set_text (_("pol"));
		Gtkmm2ext::set_size_request_to_display_given_text (name_button, "longest label", 2, 2);
		break;
	}

	update_input_display ();
	update_output_display ();
	mix_group_changed (0);
	name_changed (0);
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
		ArdourMessage msg (NULL, "nojackdialog", _("Not connected to JACK - no I/O changes are possible"));
		return TRUE;
	}

	MenuList& citems = output_menu.items();
	output_menu.set_name ("ArdourContextMenu");
	citems.clear();

	citems.push_back (MenuElem (_("Edit"), mem_fun(*this, &MixerStrip::edit_output_configuration)));
	citems.push_back (SeparatorElem());
	citems.push_back (MenuElem (_("Disconnect"), mem_fun (*(static_cast<RouteUI*>(this)), &RouteUI::disconnect_output)));
	citems.push_back (SeparatorElem());

	_session.foreach_connection (this, &MixerStrip::add_connection_to_output_menu);

	output_menu.popup (1, ev->time);

	return TRUE;
}

void
MixerStrip::edit_output_configuration ()
{
	if (output_selector == 0) {
		output_selector = new IOSelectorWindow (_session, _route, false);
	} 

	if (output_selector->is_visible()) {
		output_selector->get_toplevel()->get_window().raise();
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
		input_selector->get_toplevel()->get_window().raise();
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
		ArdourMessage msg (NULL, "nojackdialog", _("Not connected to JACK - no I/O changes are possible"));
		return TRUE;
	}

#if ADVANCED_ROUTE_DISKSTREAM_CONNECTIVITY
	if (is_audio_track()) {
		citems.push_back (MenuElem (_("Track"), mem_fun(*this, &MixerStrip::select_stream_input)));
	}
#endif
	citems.push_back (MenuElem (_("Edit"), mem_fun(*this, &MixerStrip::edit_input_configuration)));
	citems.push_back (SeparatorElem());
	citems.push_back (MenuElem (_("Disconnect"), mem_fun (*(static_cast<RouteUI*>(this)), &RouteUI::disconnect_input)));
	citems.push_back (SeparatorElem());

	_session.foreach_connection (this, &MixerStrip::add_connection_to_input_menu);

	input_menu.popup (1, ev->time);

	return TRUE;
}

void
MixerStrip::connection_input_chosen (ARDOUR::Connection *c)
{
	if (!ignore_toggle) {

		try { 
			_route.use_input_connection (*c, this);
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
			_route.use_output_connection (*c, this);
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
	
	if (c->nports() == _route.n_inputs()) {

		citems.push_back (CheckMenuElem (c->name(), bind (mem_fun(*this, &MixerStrip::connection_input_chosen), c)));
		
		ARDOUR::Connection *current = _route.input_connection();
		
		if (current == c) {
			ignore_toggle = true;
			dynamic_cast<CheckMenuItem *> (citems.back())->set_active (true);
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

	if (c->nports() == _route.n_outputs()) {

		MenuList& citems = output_menu.items();
		citems.push_back (CheckMenuElem (c->name(), bind (mem_fun(*this, &MixerStrip::connection_output_chosen), c)));
		
		ARDOUR::Connection *current = _route.output_connection();
		
		if (current == c) {
			ignore_toggle = true;
			dynamic_cast<CheckMenuItem *> (citems.back())->set_active (true);
			ignore_toggle = false;
		}
	}
}

void
MixerStrip::select_stream_input ()
{
	using namespace Menu_Helpers;

	Menu *stream_menu = manage (new Menu);
	MenuList& items = stream_menu->items();
	stream_menu->set_name ("ArdourContextMenu");
	
	Session::DiskStreamList streams = _session.disk_streams();

	for (Session::DiskStreamList::iterator i = streams.begin(); i != streams.end(); ++i) {

		if (!(*i)->hidden()) {

			items.push_back (CheckMenuElem ((*i)->name(), bind (mem_fun(*this, &MixerStrip::stream_input_chosen), *i)));
			
			if (get_diskstream() == *i) {
				ignore_toggle = true;
				static_cast<CheckMenuItem *> (items.back())->set_active (true);
				ignore_toggle = false;
			} 
		}
	}
	
	stream_menu->popup (1, 0);
}

void
MixerStrip::stream_input_chosen (DiskStream *stream)
{
	if (is_audio_track()) {
		audio_track()->set_diskstream (*stream, this);
	}
}

void
MixerStrip::update_diskstream_display ()
{
	if (is_audio_track()) {

		map_frozen ();

		update_input_display ();

		if (input_selector) {
			input_selector->hide_all ();
		}

		show_route_color ();

	} else {

		map_frozen ();

		update_input_display ();
		show_passthru_color ();
	}
}

void
MixerStrip::connect_to_pan ()
{
	ENSURE_GUI_THREAD(mem_fun(*this, &MixerStrip::connect_to_pan));
	
	panstate_connection.disconnect ();
	panstyle_connection.disconnect ();

	if (!_route.panner().empty()) {
		StreamPanner* sp = _route.panner().front();

		panstate_connection = sp->automation().automation_state_changed.connect (mem_fun(*this, &MixerStrip::pan_automation_state_changed));
		panstyle_connection = sp->automation().automation_style_changed.connect (mem_fun(*this, &MixerStrip::pan_automation_style_changed));
	}

	panners.pan_changed (this);
}

void
MixerStrip::update_input_display ()
{
	ARDOUR::Connection *c;

	if ((c = _route.input_connection()) != 0) {
		input_label.set_text (c->name());
	} else {
		switch (_width) {
		case Wide:
			input_label.set_text (_("INPUT"));
			break;
		case Narrow:
			input_label.set_text (_("IN"));
			break;
		}
	}

	panners.setup_pan ();
}

void
MixerStrip::update_output_display ()
{
	ARDOUR::Connection *c;

	if ((c = _route.output_connection()) != 0) {
		output_label.set_text (c->name());
	} else {
		switch (_width) {
		case Wide:
			output_label.set_text (_("OUTPUT"));
			break;
		case Narrow:
			output_label.set_text (_("OUT"));
			break;
		}
	}

	gpm.setup_meters ();
	panners.setup_pan ();
}

void
MixerStrip::update ()
{
	gpm.update_meters ();
}

void
MixerStrip::fast_update ()
{
	if (_session.meter_falloff() > 0.0f) {
		gpm.update_meters_falloff ();
	}
}

gint
MixerStrip::gain_automation_state_button_event (GdkEventButton *ev)
{
	if (ev->type == GDK_BUTTON_PRESS || ev->type == GDK_2BUTTON_PRESS) {
		return TRUE;
	}
	
	switch (ev->button) {
	case 1:
		switch (ev->button) {
		case 1:
			gain_astate_menu.popup (1, ev->time);
			break;
		default:
			break;
		}
	}

	return TRUE;
}

gint
MixerStrip::gain_automation_style_button_event (GdkEventButton *ev)
{
	if (ev->type == GDK_BUTTON_PRESS || ev->type == GDK_2BUTTON_PRESS) {
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

gint
MixerStrip::pan_automation_state_button_event (GdkEventButton *ev)
{
	using namespace Menu_Helpers;

	if (ev->type == GDK_BUTTON_PRESS || ev->type == GDK_2BUTTON_PRESS) {
		return TRUE;
	}

	switch (ev->button) {
	case 1:
		pan_astate_menu.popup (1, ev->time);
		break;
	default:
		break;
	}

	return TRUE;
}

gint
MixerStrip::pan_automation_style_button_event (GdkEventButton *ev)
{
	switch (ev->button) {
	case 1:
		pan_astyle_menu.popup (1, ev->time);
		break;
	default:
		break;
	}
	return TRUE;
}

string
MixerStrip::astate_string (AutoState state)
{
	return _astate_string (state, false);
}

string
MixerStrip::short_astate_string (AutoState state)
{
	return _astate_string (state, true);
}

string
MixerStrip::_astate_string (AutoState state, bool shrt)
{
	string sstr;

	switch (state) {
	case Off:
		sstr = (shrt ? "--" : _("off"));
		break;
	case Play:
		sstr = (shrt ? "P" : _("aplay"));
		break;
	case Touch:
		sstr = (shrt ? "T" : _("touch"));
		break;
	case Write:
		sstr = (shrt ? "W" : _("awrite"));
		break;
	}

	return sstr;
}

string
MixerStrip::astyle_string (AutoStyle style)
{
	return _astyle_string (style, false);
}

string
MixerStrip::short_astyle_string (AutoStyle style)
{
	return _astyle_string (style, true);
}

string
MixerStrip::_astyle_string (AutoStyle style, bool shrt)
{
	if (style & Trim) {
		return _("trim");
	} else {
		/* XXX it might different in different languages */

		return (shrt ? _("abs") : _("abs"));
	}
}

void
MixerStrip::diskstream_changed (void *src)
{
	Gtkmm2ext::UI::instance()->call_slot (mem_fun(*this, &MixerStrip::update_diskstream_display));
}	

void
MixerStrip::gain_automation_style_changed ()
{
	switch (_width) {
	case Wide:
		static_cast<Gtk::Label*> (gain_automation_style_button.get_child())->set_text (astyle_string(_route.gain_automation_curve().automation_style()));
		break;
	case Narrow:
		static_cast<Gtk::Label*> (gain_automation_style_button.get_child())->set_text (short_astyle_string(_route.gain_automation_curve().automation_style()));
		break;
	}
}

void
MixerStrip::gain_automation_state_changed ()
{
	ENSURE_GUI_THREAD(mem_fun(*this, &MixerStrip::gain_automation_state_changed));
	
	bool x;

	switch (_width) {
	case Wide:
		static_cast<Gtk::Label*> (gain_automation_state_button.get_child())->set_text (astate_string(_route.gain_automation_curve().automation_state()));
		break;
	case Narrow:
		static_cast<Gtk::Label*> (gain_automation_state_button.get_child())->set_text (short_astate_string(_route.gain_automation_curve().automation_state()));
		break;
	}

	x = (_route.gain_automation_state() != Off);
	
	if (gain_automation_state_button.get_active() != x) {
		ignore_toggle = true;
		gain_automation_state_button.set_active (x);
		ignore_toggle = false;
	}

	gpm.update_gain_sensitive ();
	
	/* start watching automation so that things move */
	
	gain_watching.disconnect();

	if (x) {
		gain_watching = ARDOUR_UI::RapidScreenUpdate.connect (mem_fun (gpm, &GainMeter::effective_gain_display));
	}
}

void
MixerStrip::pan_automation_style_changed ()
{
	ENSURE_GUI_THREAD(mem_fun(*this, &MixerStrip::pan_automation_style_changed));
	
	switch (_width) {
	case Wide:
		static_cast<Gtk::Label*> (pan_automation_style_button.get_child())->set_text (astyle_string(_route.panner().automation_style()));
		break;
	case Narrow:
		static_cast<Gtk::Label*> (pan_automation_style_button.get_child())->set_text (short_astyle_string(_route.panner().automation_style()));
		break;
	}
}

void
MixerStrip::pan_automation_state_changed ()
{
	ENSURE_GUI_THREAD(mem_fun(*this, &MixerStrip::pan_automation_state_changed));
	
	bool x;

	switch (_width) {
	case Wide:
		static_cast<Gtk::Label*> (pan_automation_state_button.get_child())->set_text (astate_string(_route.panner().automation_state()));
		break;
	case Narrow:
		static_cast<Gtk::Label*> (pan_automation_state_button.get_child())->set_text (short_astate_string(_route.panner().automation_state()));
		break;
	}

	/* when creating a new session, we get to create busses (and
	   sometimes tracks) with no outputs by the time they get
	   here.
	*/

	if (_route.panner().empty()) {
		return;
	}

	x = (_route.panner().front()->automation().automation_state() != Off);

	if (pan_automation_state_button.get_active() != x) {
		ignore_toggle = true;
		pan_automation_state_button.set_active (x);
		ignore_toggle = false;
	}

	panners.update_pan_sensitive ();
	
	/* start watching automation so that things move */
	
	pan_watching.disconnect();

	if (x) {
		pan_watching = ARDOUR_UI::RapidScreenUpdate.connect (mem_fun (panners, &PannerUI::effective_pan_display));
	}
}

void
MixerStrip::input_changed (IOChange change, void *src)
{
	Gtkmm2ext::UI::instance()->call_slot (mem_fun(*this, &MixerStrip::update_input_display));
}

void
MixerStrip::output_changed (IOChange change, void *src)
{
	Gtkmm2ext::UI::instance()->call_slot (mem_fun(*this, &MixerStrip::update_output_display));
}

void
MixerStrip::comment_button_clicked ()
{
	if (comment_window == 0) {
		setup_comment_editor ();
	}

	if (comment_window->is_visible()) {
		comment_window->hide ();
	} else {
		comment_window->set_position (Gtk::WIN_POS_MOUSE);
		comment_window->show_all ();
	}
}

void
MixerStrip::setup_comment_editor ()
{
	comment_window = new Window (GTK_WINDOW_TOPLEVEL);

	string str;
	str = _route.name();
	str += _(": comment editor");
	comment_window->set_title (str);

	comment_area.set_name ("MixerTrackCommentArea");
	comment_area.set_editable (true);
	comment_area.signal_focus_in_event().connect (ptr_fun (ARDOUR_UI::generic_focus_in_event));
	comment_area.signal_focus_out_event().connect (ptr_fun (ARDOUR_UI::generic_focus_out_event));
	comment_area.changed.connect (mem_fun(*this, &MixerStrip::comment_edited));
	comment_area.button_release_event.connect_after (ptr_fun (do_not_propagate));
	comment_area.show ();

	comment_window->add (comment_area);
	comment_window->delete_event.connect (bind (ptr_fun (just_hide_it), comment_window));
}

void
MixerStrip::comment_changed (void *src)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &MixerStrip::comment_changed), src));
	
	if (src != this) {
		ignore_comment_edit = true;
		comment_area.freeze ();
		comment_area.delete_text (0, -1);
		comment_area.set_point (0);
		comment_area.insert (_route.comment());
		comment_area.thaw ();
		ignore_comment_edit = false;
	}
}

void
MixerStrip::comment_edited ()
{
	ENSURE_GUI_THREAD(mem_fun(*this, &MixerStrip::comment_edited));
	
	if (!ignore_comment_edit) {
		string str =  comment_area.get_chars(0,-1);
		_route.set_comment (str, this);
		ARDOUR_UI::instance()->tooltips().set_tip (comment_button, 
							   str.empty() ? _("click to add/edit comments") : str);
	}
}

void
MixerStrip::set_mix_group (RouteGroup *rg)

{
	_route.set_mix_group (rg, this);
	delete group_menu;
	group_menu = 0;
}

void
MixerStrip::add_mix_group_to_menu (RouteGroup *rg)
{
	using namespace Menu_Helpers;

	MenuList& items = group_menu->items();
	items.push_back (MenuElem (rg->name(), bind (mem_fun(*this, &MixerStrip::set_mix_group), rg)));
}

gint
MixerStrip::select_mix_group (GdkEventButton *ev)
{
	using namespace Menu_Helpers;

	group_menu = new Menu;
	group_menu->set_name ("ArdourContextMenu");
	MenuList& items = group_menu->items();

	items.push_back (MenuElem (_("no group"), bind (mem_fun(*this, &MixerStrip::set_mix_group), (RouteGroup *) 0)));
	_session.foreach_mix_group (this, &MixerStrip::add_mix_group_to_menu);

	group_menu->popup (ev->button, 0);
	return stop_signal (group_button, "button_press_event");
}	

void
MixerStrip::mix_group_changed (void *ignored)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &MixerStrip::mix_group_changed), ignored));
	
	RouteGroup *rg = _route.mix_group();
	
	if (rg) {
		group_label.set_text (rg->name());
	} else {
		switch (_width) {
		case Wide:
			group_label.set_text (_("no group"));
			break;
		case Narrow:
			group_label.set_text (_("~G"));
			break;
		}
	}
}

void
MixerStrip::polarity_toggled ()
{
	bool x;

	if ((x = polarity_button.get_active()) != _route.phase_invert()) {
		_route.set_phase_invert (x, this);
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
	Gtk::Style *style;

	name_button.ensure_style ();
	style = name_button.get_style()->copy();
	style->set_bg (Gtk::STATE_NORMAL, color());
	name_button.set_style (*style);
	style->unref();

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
	
	items.push_back (MenuElem (_("Rename"), mem_fun(*this, &RouteUI::route_rename)));
	items.push_back (SeparatorElem());
	items.push_back (CheckMenuElem (_("Active"), mem_fun(*this, &RouteUI::toggle_route_active)));
	route_active_menu_item = dynamic_cast<CheckMenuItem *> (items.back());
	route_active_menu_item->set_active (_route.active());

	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Remove"), mem_fun(*this, &RouteUI::remove_this_route)));
}

gint
MixerStrip::name_button_button_release (GdkEventButton* ev)
{
	if (ev->button == 3) {
		list_route_operations ();
	}
	return FALSE;
}

void
MixerStrip::list_route_operations ()
{
	if (route_ops_menu == 0) {
		build_route_ops_menu ();
	}

	route_ops_menu->popup (1, 0);
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
		global_frame.set_shadow_type (GTK_SHADOW_ETCHED_OUT);
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
		name_label.set_text (short_version (_route.name(), 5));
		break;
	}
}

void
MixerStrip::width_clicked ()
{
	switch (_width) {
	case Wide:
		set_width (Narrow);
		break;
	case Narrow:
		set_width (Wide);
		break;
	}
}

void
MixerStrip::hide_clicked ()
{
	if (_embedded) {
		 Hiding(); /* EMIT_SIGNAL */
	} else {
		_mixer.unselect_strip_in_display (this);
	}
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

	AudioTrack* at = dynamic_cast<AudioTrack*>(&_route);

	if (at) {
		switch (at->freeze_state()) {
		case AudioTrack::Frozen:
			pre_redirect_box.set_sensitive (false);
			post_redirect_box.set_sensitive (false);
			speed_spinner.set_sensitive (false);
			break;
		default:
			pre_redirect_box.set_sensitive (true);
			post_redirect_box.set_sensitive (true);
			speed_spinner.set_sensitive (true);
			break;
		}
	}
	_route.foreach_redirect (this, &MixerStrip::hide_redirect_editor);
}

void
MixerStrip::hide_redirect_editor (Redirect* redirect)
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
		if (_route.active()) {
			set_name ("AudioTrackStripBase");
			gpm.set_meter_strip_name ("AudioTrackStripBase");
		} else {
			set_name ("AudioTrackStripBaseInactive");
			gpm.set_meter_strip_name ("AudioTrackStripBaseInactive");
		}
		gpm.set_fader_name ("AudioTrackFader");
	} else {
		if (_route.active()) {
			set_name ("AudioBusStripBase");
			gpm.set_meter_strip_name ("AudioBusStripBase");
		} else {
			set_name ("AudioBusStripBaseInactive");
			gpm.set_meter_strip_name ("AudioBusStripBaseInactive");
		}
		gpm.set_fader_name ("AudioBusFader");
	}
}
