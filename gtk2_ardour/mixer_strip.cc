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

#include "pbd/convert.h"
#include "pbd/enumwriter.h"
#include "pbd/replace_all.h"
#include "pbd/stacktrace.h"

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/choice.h>
#include <gtkmm2ext/doi.h>
#include <gtkmm2ext/slider_controller.h>
#include <gtkmm2ext/bindable_button.h>

#include "ardour/audio_track.h"
#include "ardour/audioengine.h"
#include "ardour/internal_send.h"
#include "ardour/midi_track.h"
#include "ardour/pannable.h"
#include "ardour/panner.h"
#include "ardour/panner_shell.h"
#include "ardour/port.h"
#include "ardour/profile.h"
#include "ardour/route.h"
#include "ardour/route_group.h"
#include "ardour/send.h"
#include "ardour/session.h"
#include "ardour/types.h"
#include "ardour/user_bundle.h"

#include "ardour_ui.h"
#include "ardour_window.h"
#include "mixer_strip.h"
#include "mixer_ui.h"
#include "keyboard.h"
#include "ardour_button.h"
#include "public_editor.h"
#include "send_ui.h"
#include "io_selector.h"
#include "utils.h"
#include "gui_thread.h"
#include "route_group_menu.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace std;

int MixerStrip::scrollbar_height = 0;
PBD::Signal1<void,MixerStrip*> MixerStrip::CatchDeletion;

MixerStrip::MixerStrip (Mixer_UI& mx, Session* sess, bool in_mixer)
	: AxisView(sess)
	, RouteUI (sess)
	, _mixer(mx)
	, _mixer_owned (in_mixer)
	, processor_box (sess, boost::bind (&MixerStrip::plugin_selector, this), mx.selection(), this, in_mixer)
	, gpm (sess, 250)
	, panners (sess)
	, button_size_group (Gtk::SizeGroup::create (Gtk::SIZE_GROUP_HORIZONTAL))
	, button_table (3, 1)
	, rec_solo_table (2, 2)
	, top_button_table (1, 2)
	, middle_button_table (1, 2)
	, bottom_button_table (1, 2)
	, meter_point_button (_("pre"))
	, midi_input_enable_button (0)
	, _comment_button (_("Comments"))
	, _visibility (X_("mixer-strip-visibility"))
{
	init ();

	if (!_mixer_owned) {
		/* the editor mixer strip: don't destroy it every time
		   the underlying route goes away.
		*/

		self_destruct = false;
	}
}

MixerStrip::MixerStrip (Mixer_UI& mx, Session* sess, boost::shared_ptr<Route> rt, bool in_mixer)
	: AxisView(sess)
	, RouteUI (sess)
	, _mixer(mx)
	, _mixer_owned (in_mixer)
	, processor_box (sess, boost::bind (&MixerStrip::plugin_selector, this), mx.selection(), this, in_mixer)
	, gpm (sess, 250)
	, panners (sess)
	, button_size_group (Gtk::SizeGroup::create (Gtk::SIZE_GROUP_HORIZONTAL))
	, button_table (3, 1)
	, middle_button_table (1, 2)
	, bottom_button_table (1, 2)
	, meter_point_button (_("pre"))
	, midi_input_enable_button (0)
	, _comment_button (_("Comments"))
	, _visibility (X_("mixer-strip-visibility"))
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
	route_ops_menu = 0;
	ignore_comment_edit = false;
	ignore_toggle = false;
	comment_window = 0;
	comment_area = 0;
	_width_owner = 0;
	spacer = 0;

	/* the length of this string determines the width of the mixer strip when it is set to `wide' */
	longest_label = "longest label";

	string t = _("Click to toggle the width of this mixer strip.");
	if (_mixer_owned) {
		t += string_compose (_("\n%1-%2-click to toggle the width of all strips."), Keyboard::primary_modifier_name(), Keyboard::tertiary_modifier_name ());
	}
	
	width_button.set_image (::get_icon("strip_width"));
	ARDOUR_UI::instance()->set_tip (width_button, t);

	hide_button.set_image(::get_icon("hide"));
	ARDOUR_UI::instance()->set_tip (&hide_button, _("Hide this mixer strip"));

	input_button.set_text (_("Input"));
	input_button.set_name ("mixer strip button");
	input_button.set_size_request (-1, 20);
	input_button_box.pack_start (input_button, true, true);

	output_button.set_text (_("Output"));
	output_button.set_name ("mixer strip button");
	Gtkmm2ext::set_size_request_to_display_given_text (output_button, longest_label.c_str(), 4, 4);

	ARDOUR_UI::instance()->set_tip (&meter_point_button, _("Click to select metering point"), "");
	meter_point_button.set_name ("mixer strip button");

	/* TRANSLATORS: this string should be longest of the strings
	   used to describe meter points. In english, it's "input".
	*/
	set_size_request_to_display_given_text (meter_point_button, _("tupni"), 5, 5);

	bottom_button_table.attach (meter_point_button, 1, 2, 0, 1);

	meter_point_button.signal_button_press_event().connect (sigc::mem_fun (gpm, &GainMeter::meter_press), false);
	meter_point_button.signal_button_release_event().connect (sigc::mem_fun (gpm, &GainMeter::meter_release), false);

	hide_button.set_events (hide_button.get_events() & ~(Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK));

	monitor_input_button->set_diameter (3);
	monitor_disk_button->set_diameter (3);

        solo_isolated_led = manage (new ArdourButton (ArdourButton::led_default_elements));
        solo_isolated_led->show ();
        solo_isolated_led->set_diameter (3);
        solo_isolated_led->set_no_show_all (true);
        solo_isolated_led->set_name (X_("solo isolate"));
        solo_isolated_led->add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);
        solo_isolated_led->signal_button_release_event().connect (sigc::mem_fun (*this, &RouteUI::solo_isolate_button_release));
	UI::instance()->set_tip (solo_isolated_led, _("Isolate Solo"), "");

        solo_safe_led = manage (new ArdourButton (ArdourButton::led_default_elements));
        solo_safe_led->show ();
        solo_safe_led->set_diameter (3);
        solo_safe_led->set_no_show_all (true);
        solo_safe_led->set_name (X_("solo safe"));
        solo_safe_led->add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);
        solo_safe_led->signal_button_release_event().connect (sigc::mem_fun (*this, &RouteUI::solo_safe_button_release));
	UI::instance()->set_tip (solo_safe_led, _("Lock Solo Status"), "");

	solo_safe_led->set_text (_("lock"));
	solo_isolated_led->set_text (_("iso"));

	top_button_table.set_homogeneous (true);
	top_button_table.set_spacings (2);
	top_button_table.attach (*monitor_input_button, 0, 1, 0, 1);
        top_button_table.attach (*monitor_disk_button, 1, 2, 0, 1);
	top_button_table.show ();

	rec_solo_table.set_homogeneous (true);
	rec_solo_table.set_row_spacings (2);
	rec_solo_table.set_col_spacings (2);
        rec_solo_table.attach (*solo_isolated_led, 1, 2, 0, 1);
        rec_solo_table.attach (*solo_safe_led, 1, 2, 1, 2);
        rec_solo_table.show ();

	button_table.set_homogeneous (false);
	button_table.set_spacings (2);

	if (solo_isolated_led) {
		button_size_group->add_widget (*solo_isolated_led);
	}
	if (solo_safe_led) {
		button_size_group->add_widget (*solo_safe_led);
	}
	if (rec_enable_button) {
		button_size_group->add_widget (*rec_enable_button);
	}
	if (monitor_disk_button) {
		button_size_group->add_widget (*monitor_disk_button);
	}
	if (monitor_input_button) {
		button_size_group->add_widget (*monitor_input_button);
	}

	button_table.attach (name_button, 0, 1, 0, 1);
	button_table.attach (input_button_box, 0, 1, 1, 2);
	button_table.attach (_invert_button_box, 0, 1, 2, 3);

	middle_button_table.set_homogeneous (true);
	middle_button_table.set_spacings (2);

	bottom_button_table.set_spacings (2);
	bottom_button_table.set_homogeneous (true);
//	bottom_button_table.attach (group_button, 0, 1, 0, 1);
	bottom_button_table.attach (gpm.gain_automation_state_button, 0, 1, 0, 1);

	name_button.set_name ("mixer strip button");
	name_button.set_text (" "); /* non empty text, forces creation of the layout */
	name_button.set_text (""); /* back to empty */
	name_button.layout()->set_ellipsize (Pango::ELLIPSIZE_END);
	name_button.signal_size_allocate().connect (sigc::mem_fun (*this, &MixerStrip::name_button_resized));
	Gtkmm2ext::set_size_request_to_display_given_text (name_button, longest_label.c_str(), 2, 2);
	name_button.set_size_request (-1, 20);

	ARDOUR_UI::instance()->set_tip (&group_button, _("Mix group"), "");
	group_button.set_name ("mixer strip button");
	Gtkmm2ext::set_size_request_to_display_given_text (group_button, "Group", 2, 2);

	_comment_button.set_name (X_("mixer strip button"));
	_comment_button.signal_clicked.connect (sigc::mem_fun (*this, &MixerStrip::toggle_comment_editor));

	global_vpacker.set_border_width (0);
	global_vpacker.set_spacing (0);

	width_button.set_name ("mixer strip button");
	hide_button.set_name ("mixer strip button");
	top_event_box.set_name ("mixer strip button");

	width_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MixerStrip::width_button_pressed), false);
	hide_button.signal_clicked.connect (sigc::mem_fun(*this, &MixerStrip::hide_clicked));

	width_hide_box.pack_start (width_button, false, true);
	width_hide_box.pack_start (top_event_box, true, true);
	width_hide_box.pack_end (hide_button, false, true);

	whvbox.pack_start (width_hide_box, true, true);

	global_vpacker.set_spacing (2);
	global_vpacker.pack_start (whvbox, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (button_table, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (processor_box, true, true);
	global_vpacker.pack_start (panners, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (top_button_table, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (rec_solo_table, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (middle_button_table, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (gpm, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (bottom_button_table, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (output_button, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (_comment_button, Gtk::PACK_SHRINK);

	global_frame.add (global_vpacker);
	global_frame.set_shadow_type (Gtk::SHADOW_IN);
	global_frame.set_name ("BaseFrame");

	add (global_frame);

	/* force setting of visible selected status */

	_selected = true;
	set_selected (false);

	_packed = false;
	_embedded = false;

	_session->engine().Stopped.connect (*this, invalidator (*this), boost::bind (&MixerStrip::engine_stopped, this), gui_context());
	_session->engine().Running.connect (*this, invalidator (*this), boost::bind (&MixerStrip::engine_running, this), gui_context());

	input_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MixerStrip::input_press), false);
	output_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MixerStrip::output_press), false);

	/* ditto for this button and busses */

	name_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MixerStrip::name_button_button_press), false);
	group_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MixerStrip::select_route_group), false);

	_width = (Width) -1;

	/* start off as a passthru strip. we'll correct this, if necessary,
	   in update_diskstream_display().
	*/

	/* start off as a passthru strip. we'll correct this, if necessary,
	   in update_diskstream_display().
	*/

	if (is_midi_track()) {
		set_name ("MidiTrackStripBase");
	} else {
		set_name ("AudioTrackStripBase");
	}

	add_events (Gdk::BUTTON_RELEASE_MASK|
		    Gdk::ENTER_NOTIFY_MASK|
		    Gdk::LEAVE_NOTIFY_MASK|
		    Gdk::KEY_PRESS_MASK|
		    Gdk::KEY_RELEASE_MASK);

	set_flags (get_flags() | Gtk::CAN_FOCUS);

	AudioEngine::instance()->PortConnectedOrDisconnected.connect (
		*this, invalidator (*this), boost::bind (&MixerStrip::port_connected_or_disconnected, this, _1, _3), gui_context ()
		);

	/* Add the widgets under visibility control to the VisibilityGroup; the names used here
	   must be the same as those used in RCOptionEditor so that the configuration changes
	   are recognised when they occur.
	*/
	_visibility.add (&_invert_button_box, X_("PhaseInvert"), _("Phase Invert"));
	_visibility.add (solo_safe_led, X_("SoloSafe"), _("Solo Safe"), true, boost::bind (&MixerStrip::override_solo_visibility, this));
	_visibility.add (solo_isolated_led, X_("SoloIsolated"), _("Solo Isolated"), true, boost::bind (&MixerStrip::override_solo_visibility, this));
	_visibility.add (&_comment_button, X_("Comments"), _("Comments"));
	_visibility.add (&group_button, X_("Group"), _("Group"));
	_visibility.add (&meter_point_button, X_("MeterPoint"), _("Meter Point"));

	parameter_changed (X_("mixer-strip-visibility"));

	Config->ParameterChanged.connect (_config_connection, MISSING_INVALIDATOR, boost::bind (&MixerStrip::parameter_changed, this, _1), gui_context());

	gpm.LevelMeterButtonPress.connect_same_thread (_level_meter_connection, boost::bind (&MixerStrip::level_meter_button_press, this, _1));
}

MixerStrip::~MixerStrip ()
{
	CatchDeletion (this);

	delete input_selector;
	delete output_selector;
	delete comment_window;
}

void
MixerStrip::set_route (boost::shared_ptr<Route> rt)
{
	if (rec_enable_button->get_parent()) {
		rec_solo_table.remove (*rec_enable_button);
	}

	if (show_sends_button->get_parent()) {
		rec_solo_table.remove (*show_sends_button);
	}

	RouteUI::set_route (rt);

	/* ProcessorBox needs access to _route so that it can read
	   GUI object state.
	*/
	processor_box.set_route (rt);

	/* map the current state */

	mute_changed (0);
	update_solo_display ();

	delete input_selector;
	input_selector = 0;

	delete output_selector;
	output_selector = 0;

	revert_to_default_display ();

	/* unpack these from the parent and stuff them into our own
	   table
	*/
	
	if (gpm.peak_display.get_parent()) {
		gpm.peak_display.get_parent()->remove (gpm.peak_display);
	}
	if (gpm.gain_display.get_parent()) {
		gpm.gain_display.get_parent()->remove (gpm.gain_display);
	}

	gpm.set_type (rt->meter_type());
	
	middle_button_table.attach (gpm.gain_display,0,1,1,2);
	middle_button_table.attach (gpm.peak_display,1,2,1,2);

	if (solo_button->get_parent()) {
		middle_button_table.remove (*solo_button);
	}

	if (mute_button->get_parent()) {
		middle_button_table.remove (*mute_button);
	}

	if (route()->is_master()) {
		middle_button_table.attach (*mute_button, 0, 2, 0, 1);
		solo_button->hide ();
		mute_button->show ();
		rec_solo_table.hide ();
	} else {
		middle_button_table.attach (*mute_button, 0, 1, 0, 1);
		middle_button_table.attach (*solo_button, 1, 2, 0, 1);
		mute_button->show ();
		solo_button->show ();
		rec_solo_table.show ();
	}

	if (_mixer_owned && (route()->is_master() || route()->is_monitor())) {

		if (scrollbar_height == 0) {
			HScrollbar scrollbar;
			Gtk::Requisition requisition(scrollbar.size_request ());
			scrollbar_height = requisition.height;
		}

		spacer = manage (new EventBox);
		spacer->set_size_request (-1, scrollbar_height);
		global_vpacker.pack_start (*spacer, false, false);
	}

	if (is_track()) {
		monitor_input_button->show ();
		monitor_disk_button->show ();
	} else {
		monitor_input_button->hide();
		monitor_disk_button->hide ();
	}

	if (is_midi_track()) {
		if (midi_input_enable_button == 0) {
			midi_input_enable_button = manage (new ArdourButton);
			midi_input_enable_button->set_name ("midi input button");
			midi_input_enable_button->set_image (::get_icon (X_("midi_socket_small")));
			midi_input_enable_button->signal_button_press_event().connect (sigc::mem_fun (*this, &MixerStrip::input_active_button_press), false);
			midi_input_enable_button->signal_button_release_event().connect (sigc::mem_fun (*this, &MixerStrip::input_active_button_release), false);
			ARDOUR_UI::instance()->set_tip (midi_input_enable_button, _("Enable/Disable MIDI input"));
		} else {
			input_button_box.remove (*midi_input_enable_button);
		}
		/* get current state */
		midi_input_status_changed ();
		input_button_box.pack_start (*midi_input_enable_button, false, false);
		/* follow changes */
		midi_track()->InputActiveChanged.connect (route_connections, invalidator (*this), boost::bind (&MixerStrip::midi_input_status_changed, this), gui_context());
	} else {
		if (midi_input_enable_button) {
			/* removal from the container will delete it */
			input_button_box.remove (*midi_input_enable_button);
			midi_input_enable_button = 0;
		}
	}

	if (is_audio_track()) {
		boost::shared_ptr<AudioTrack> at = audio_track();
		at->FreezeChange.connect (route_connections, invalidator (*this), boost::bind (&MixerStrip::map_frozen, this), gui_context());
	}

	if (is_track ()) {

		rec_solo_table.attach (*rec_enable_button, 0, 1, 0, 2);
		rec_enable_button->set_sensitive (_session->writable());
		rec_enable_button->show();

	} else {

		/* non-master bus */

		if (!_route->is_master()) {
			rec_solo_table.attach (*show_sends_button, 0, 1, 0, 2);
			show_sends_button->show();
		}
	}

	meter_point_button.set_text (meter_point_string (_route->meter_point()));

	delete route_ops_menu;
	route_ops_menu = 0;

	_route->meter_change.connect (route_connections, invalidator (*this), bind (&MixerStrip::meter_changed, this), gui_context());
	_route->route_group_changed.connect (route_connections, invalidator (*this), boost::bind (&MixerStrip::route_group_changed, this), gui_context());

	if (_route->panner_shell()) {
		_route->panner_shell()->Changed.connect (route_connections, invalidator (*this), boost::bind (&MixerStrip::connect_to_pan, this), gui_context());
	}

	if (is_audio_track()) {
		audio_track()->DiskstreamChanged.connect (route_connections, invalidator (*this), boost::bind (&MixerStrip::diskstream_changed, this), gui_context());
	}

	_route->comment_changed.connect (route_connections, invalidator (*this), boost::bind (&MixerStrip::comment_changed, this, _1), gui_context());
	_route->PropertyChanged.connect (route_connections, invalidator (*this), boost::bind (&MixerStrip::property_changed, this, _1), gui_context());

	set_stuff_from_route ();

	/* now force an update of all the various elements */

	mute_changed (0);
	update_solo_display ();
	name_changed ();
	comment_changed (0);
	route_group_changed ();

	connect_to_pan ();
	panners.setup_pan ();

	if (has_audio_outputs ()) {
		panners.show_all ();
	} else {
		panners.hide_all ();
	}

	update_diskstream_display ();
	update_input_display ();
	update_output_display ();

	add_events (Gdk::BUTTON_RELEASE_MASK);

	processor_box.show ();

	if (!route()->is_master() && !route()->is_monitor()) {
		/* we don't allow master or control routes to be hidden */
		hide_button.show();
	}

	gpm.reset_peak_display ();
	gpm.gain_display.show ();
	gpm.peak_display.show ();

	width_button.show();
	width_hide_box.show();
	whvbox.show ();
	global_frame.show();
	global_vpacker.show();
	button_table.show();
	middle_button_table.show();
	bottom_button_table.show();
	gpm.show_all ();
	meter_point_button.show();
	input_button_box.show_all();
	output_button.show();
	name_button.show();
	_comment_button.show();
	group_button.show();
	gpm.gain_automation_state_button.show();

	parameter_changed ("mixer-strip-visibility");

	show ();
}

void
MixerStrip::set_stuff_from_route ()
{
	/* if width is not set, it will be set by the MixerUI or editor */

	string str = gui_property ("strip-width");
	if (!str.empty()) {
		set_width_enum (Width (string_2_enum (str, _width)), this);
	}
}

void
MixerStrip::set_width_enum (Width w, void* owner)
{
	/* always set the gpm width again, things may be hidden */

	gpm.set_width (w);
	panners.set_width (w);

	boost::shared_ptr<AutomationList> gain_automation = _route->gain_control()->alist();

	_width_owner = owner;

	_width = w;

	if (_width_owner == this) {
		set_gui_property ("strip-width", enum_2_string (_width));
	}

	set_button_names ();

	switch (w) {
	case Wide:

		if (show_sends_button)  {
			show_sends_button->set_text (_("Aux\nSends"));
			show_sends_button->layout()->set_alignment (Pango::ALIGN_CENTER);
		}

		gpm.gain_automation_style_button.set_text (
				gpm.astyle_string(gain_automation->automation_style()));
		gpm.gain_automation_state_button.set_text (
				gpm.astate_string(gain_automation->automation_state()));

		if (_route->panner()) {
			((Gtk::Label*)panners.pan_automation_style_button.get_child())->set_text (
					panners.astyle_string(_route->panner()->automation_style()));
			((Gtk::Label*)panners.pan_automation_state_button.get_child())->set_text (
					panners.astate_string(_route->panner()->automation_state()));
		}


		Gtkmm2ext::set_size_request_to_display_given_text (name_button, longest_label.c_str(), 2, 2);
		set_size_request (-1, -1);
		break;

	case Narrow:

		if (show_sends_button) {
			show_sends_button->set_text (_("Snd"));
		}

		gpm.gain_automation_style_button.set_text (
				gpm.short_astyle_string(gain_automation->automation_style()));
		gpm.gain_automation_state_button.set_text (
				gpm.short_astate_string(gain_automation->automation_state()));

		if (_route->panner()) {
			((Gtk::Label*)panners.pan_automation_style_button.get_child())->set_text (
			panners.short_astyle_string(_route->panner()->automation_style()));
			((Gtk::Label*)panners.pan_automation_state_button.get_child())->set_text (
			panners.short_astate_string(_route->panner()->automation_state()));
		}

		Gtkmm2ext::set_size_request_to_display_given_text (name_button, "long", 2, 2);
		set_size_request (max (50, gpm.get_gm_width()), -1);
		break;
	}

	processor_box.set_width (w);

	update_input_display ();
	update_output_display ();
	setup_comment_button ();
	route_group_changed ();
	name_changed ();
	WidthChanged ();
}

void
MixerStrip::set_packed (bool yn)
{
	_packed = yn;

	if (_packed) {
		set_gui_property ("visible", true);
	} else {
		set_gui_property ("visible", false);
	}
}


struct RouteCompareByName {
	bool operator() (boost::shared_ptr<Route> a, boost::shared_ptr<Route> b) {
		return a->name().compare (b->name()) < 0;
	}
};

gint
MixerStrip::output_press (GdkEventButton *ev)
{
	using namespace Menu_Helpers;
	if (!_session->engine().connected()) {
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
		citems.clear ();
		output_menu_bundles.clear ();

		citems.push_back (MenuElem (_("Disconnect"), sigc::mem_fun (*(static_cast<RouteUI*>(this)), &RouteUI::disconnect_output)));

		for (DataType::iterator i = DataType::begin(); i != DataType::end(); ++i) {
			citems.push_back (
				MenuElem (
					string_compose ("Add %1 port", (*i).to_i18n_string()),
					sigc::bind (sigc::mem_fun (*this, &MixerStrip::add_output_port), *i)
					)
				);
		}
		
		citems.push_back (SeparatorElem());
		uint32_t const n_with_separator = citems.size ();

		ARDOUR::BundleList current = _route->output()->bundles_connected ();

		boost::shared_ptr<ARDOUR::BundleList> b = _session->bundles ();

		/* give user bundles first chance at being in the menu */

		for (ARDOUR::BundleList::iterator i = b->begin(); i != b->end(); ++i) {
			if (boost::dynamic_pointer_cast<UserBundle> (*i)) {
				maybe_add_bundle_to_output_menu (*i, current);
			}
		}

		for (ARDOUR::BundleList::iterator i = b->begin(); i != b->end(); ++i) {
			if (boost::dynamic_pointer_cast<UserBundle> (*i) == 0) {
				maybe_add_bundle_to_output_menu (*i, current);
			}
		}

		boost::shared_ptr<ARDOUR::RouteList> routes = _session->get_routes ();
		RouteList copy = *routes;
		copy.sort (RouteCompareByName ());
		for (ARDOUR::RouteList::const_iterator i = copy.begin(); i != copy.end(); ++i) {
			maybe_add_bundle_to_output_menu ((*i)->input()->bundle(), current);
		}

		if (citems.size() == n_with_separator) {
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

		boost::shared_ptr<Send> send;
		boost::shared_ptr<IO> output;

		if ((send = boost::dynamic_pointer_cast<Send>(_current_delivery)) != 0) {
			if (!boost::dynamic_pointer_cast<InternalSend>(send)) {
				output = send->output();
			} else {
				output = _route->output ();
			}
		} else {
			output = _route->output ();
		}

		output_selector = new IOSelectorWindow (_session, output);
	}

	if (output_selector->is_visible()) {
		output_selector->get_toplevel()->get_window()->raise();
	} else {
		output_selector->present ();
	}

	output_selector->set_keep_above (true);
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

	input_selector->set_keep_above (true);
}

gint
MixerStrip::input_press (GdkEventButton *ev)
{
	using namespace Menu_Helpers;

	MenuList& citems = input_menu.items();
	input_menu.set_name ("ArdourContextMenu");
	citems.clear();

	if (!_session->engine().connected()) {
		MessageDialog msg (_("Not connected to JACK - no I/O changes are possible"));
		msg.run ();
		return true;
	}

	if (_session->actively_recording() && _route->record_enabled())
		return true;

	switch (ev->button) {

	case 1:
		edit_input_configuration ();
		break;

	case 3:
	{
		citems.push_back (MenuElem (_("Disconnect"), sigc::mem_fun (*(static_cast<RouteUI*>(this)), &RouteUI::disconnect_input)));

		for (DataType::iterator i = DataType::begin(); i != DataType::end(); ++i) {
			citems.push_back (
				MenuElem (
					string_compose ("Add %1 port", (*i).to_i18n_string()),
					sigc::bind (sigc::mem_fun (*this, &MixerStrip::add_input_port), *i)
					)
				);
		}

		citems.push_back (SeparatorElem());
		uint32_t const n_with_separator = citems.size ();
		
		input_menu_bundles.clear ();

		ARDOUR::BundleList current = _route->input()->bundles_connected ();

		boost::shared_ptr<ARDOUR::BundleList> b = _session->bundles ();

		/* give user bundles first chance at being in the menu */

		for (ARDOUR::BundleList::iterator i = b->begin(); i != b->end(); ++i) {
			if (boost::dynamic_pointer_cast<UserBundle> (*i)) {
				maybe_add_bundle_to_input_menu (*i, current);
			}
		}

		for (ARDOUR::BundleList::iterator i = b->begin(); i != b->end(); ++i) {
			if (boost::dynamic_pointer_cast<UserBundle> (*i) == 0) {
				maybe_add_bundle_to_input_menu (*i, current);
			}
		}

		boost::shared_ptr<ARDOUR::RouteList> routes = _session->get_routes ();
		RouteList copy = *routes;
		copy.sort (RouteCompareByName ());
		for (ARDOUR::RouteList::const_iterator i = copy.begin(); i != copy.end(); ++i) {
			maybe_add_bundle_to_input_menu ((*i)->output()->bundle(), current);
		}

		if (citems.size() == n_with_separator) {
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
MixerStrip::bundle_input_chosen (boost::shared_ptr<ARDOUR::Bundle> c)
{
	if (ignore_toggle) {
		return;
	}

	ARDOUR::BundleList current = _route->input()->bundles_connected ();

	if (std::find (current.begin(), current.end(), c) == current.end()) {
		_route->input()->connect_ports_to_bundle (c, true, this);
	} else {
		_route->input()->disconnect_ports_from_bundle (c, this);
	}
}

void
MixerStrip::bundle_output_chosen (boost::shared_ptr<ARDOUR::Bundle> c)
{
	if (ignore_toggle) {
		return;
	}

	ARDOUR::BundleList current = _route->output()->bundles_connected ();

	if (std::find (current.begin(), current.end(), c) == current.end()) {
		_route->output()->connect_ports_to_bundle (c, true, this);
	} else {
		_route->output()->disconnect_ports_from_bundle (c, this);
	}
}

void
MixerStrip::maybe_add_bundle_to_input_menu (boost::shared_ptr<Bundle> b, ARDOUR::BundleList const& /*current*/)
{
	using namespace Menu_Helpers;

 	if (b->ports_are_outputs() == false || b->nchannels() != _route->n_inputs() || *b == *_route->output()->bundle()) {
 		return;
 	}

	list<boost::shared_ptr<Bundle> >::iterator i = input_menu_bundles.begin ();
	while (i != input_menu_bundles.end() && b->has_same_ports (*i) == false) {
		++i;
	}

	if (i != input_menu_bundles.end()) {
		return;
	}

	input_menu_bundles.push_back (b);

	MenuList& citems = input_menu.items();

	std::string n = b->name ();
	replace_all (n, "_", " ");

	citems.push_back (MenuElem (n, sigc::bind (sigc::mem_fun(*this, &MixerStrip::bundle_input_chosen), b)));
}

void
MixerStrip::maybe_add_bundle_to_output_menu (boost::shared_ptr<Bundle> b, ARDOUR::BundleList const& /*current*/)
{
	using namespace Menu_Helpers;

 	if (b->ports_are_inputs() == false || b->nchannels() != _route->n_outputs() || *b == *_route->input()->bundle()) {
 		return;
 	}

	list<boost::shared_ptr<Bundle> >::iterator i = output_menu_bundles.begin ();
	while (i != output_menu_bundles.end() && b->has_same_ports (*i) == false) {
		++i;
	}

	if (i != output_menu_bundles.end()) {
		return;
	}

	output_menu_bundles.push_back (b);

	MenuList& citems = output_menu.items();

	std::string n = b->name ();
	replace_all (n, "_", " ");

	citems.push_back (MenuElem (n, sigc::bind (sigc::mem_fun(*this, &MixerStrip::bundle_output_chosen), b)));
}

void
MixerStrip::update_diskstream_display ()
{
	if (is_track()) {

		if (input_selector) {
			input_selector->hide_all ();
		}

		route_color_changed ();

	} else {

		show_passthru_color ();
	}
}

void
MixerStrip::connect_to_pan ()
{
	ENSURE_GUI_THREAD (*this, &MixerStrip::connect_to_pan)

	panstate_connection.disconnect ();
	panstyle_connection.disconnect ();

	if (!_route->panner()) {
		return;
	}

	boost::shared_ptr<Pannable> p = _route->pannable ();

	p->automation_state_changed.connect (panstate_connection, invalidator (*this), boost::bind (&PannerUI::pan_automation_state_changed, &panners), gui_context());
	p->automation_style_changed.connect (panstyle_connection, invalidator (*this), boost::bind (&PannerUI::pan_automation_style_changed, &panners), gui_context());

	panners.panshell_changed ();
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
	boost::shared_ptr<Port> port;
	vector<string> port_connections;

	uint32_t total_connection_count = 0;
	uint32_t io_connection_count = 0;
	uint32_t ardour_connection_count = 0;
	uint32_t system_connection_count = 0;
	uint32_t other_connection_count = 0;

	ostringstream label;
	string label_string;

	bool have_label = false;
	bool each_io_has_one_connection = true;

	string connection_name;
	string ardour_track_name;
	string other_connection_type;
	string system_ports;
	string system_port;

	ostringstream tooltip;
	char * tooltip_cstr;

	if (for_input) {
		io_count = route->n_inputs().n_total();
		tooltip << string_compose (_("<b>INPUT</b> to %1"), Glib::Markup::escape_text(route->name()));
	} else {
		io_count = route->n_outputs().n_total();
		tooltip << string_compose (_("<b>OUTPUT</b> from %1"), Glib::Markup::escape_text(route->name()));
	}


	for (io_index = 0; io_index < io_count; ++io_index) {
		if (for_input) {
			port = route->input()->nth (io_index);
		} else {
			port = route->output()->nth (io_index);
		}

		port_connections.clear ();
		port->get_connections(port_connections);
		io_connection_count = 0;

		if (!port_connections.empty()) {
			for (vector<string>::iterator i = port_connections.begin(); i != port_connections.end(); ++i) {
				string& connection_name (*i);

				if (io_connection_count == 0) {
					tooltip << endl << Glib::Markup::escape_text(port->name().substr(port->name().find("/") + 1)) << " -> " << Glib::Markup::escape_text(connection_name);
				} else {
					tooltip << ", " << Glib::Markup::escape_text(connection_name);
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
		if (total_connection_count == ardour_connection_count) {
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
		label_string = label.str().substr(0, 7);
		break;
	case Narrow:
		label_string = label.str().substr(0, 3);
		break;
  	}

	if (for_input) {
		input_button.set_text (label_string);
	} else {
		output_button.set_text (label_string);
	}
}

void
MixerStrip::update_input_display ()
{
	update_io_button (_route, _width, true);
  	panners.setup_pan ();

	if (has_audio_outputs ()) {
		panners.show_all ();
	} else {
		panners.hide_all ();
	}

}

void
MixerStrip::update_output_display ()
{
	update_io_button (_route, _width, false);
  	gpm.setup_meters ();
  	panners.setup_pan ();

	if (has_audio_outputs ()) {
		panners.show_all ();
	} else {
		panners.hide_all ();
	}
}

void
MixerStrip::fast_update ()
{
	gpm.update_meters ();
}

void
MixerStrip::diskstream_changed ()
{
	Gtkmm2ext::UI::instance()->call_slot (invalidator (*this), boost::bind (&MixerStrip::update_diskstream_display, this));
}

void
MixerStrip::port_connected_or_disconnected (boost::weak_ptr<Port> wa, boost::weak_ptr<Port> wb)
{
	boost::shared_ptr<Port> a = wa.lock ();
	boost::shared_ptr<Port> b = wb.lock ();

	if ((a && _route->input()->has_port (a)) || (b && _route->input()->has_port (b))) {
		update_input_display ();
		set_width_enum (_width, this);
	}

	if ((a && _route->output()->has_port (a)) || (b && _route->output()->has_port (b))) {
		update_output_display ();
		set_width_enum (_width, this);
	}
}

void
MixerStrip::setup_comment_button ()
{
	switch (_width) {

	case Wide:
		if (_route->comment().empty ()) {
			_comment_button.unset_bg (STATE_NORMAL);
			_comment_button.set_text (_("Comments"));
		} else {
			_comment_button.modify_bg (STATE_NORMAL, color ());
			_comment_button.set_text (_("*Comments*"));
		}
		break;

	case Narrow:
		if (_route->comment().empty ()) {
			_comment_button.unset_bg (STATE_NORMAL);
			_comment_button.set_text (_("Cmt"));
		} else {
			_comment_button.modify_bg (STATE_NORMAL, color ());
			_comment_button.set_text (_("*Cmt*"));
		}
		break;
	}

	ARDOUR_UI::instance()->set_tip (
		_comment_button, _route->comment().empty() ? _("Click to Add/Edit Comments") : _route->comment()
		);
}

void
MixerStrip::comment_editor_done_editing ()
{
	string const str = comment_area->get_buffer()->get_text();
	if (str == _route->comment ()) {
		return;
	}

	_route->set_comment (str, this);
	setup_comment_button ();
}

void
MixerStrip::toggle_comment_editor ()
{
	if (ignore_toggle) {
		return;
	}

	if (comment_window && comment_window->is_visible ()) {
		comment_window->hide ();
	} else {
		open_comment_editor ();
	}
}

void
MixerStrip::open_comment_editor ()
{
	if (comment_window == 0) {
		setup_comment_editor ();
	}

	string title;
	title = _route->name();
	title += _(": comment editor");

	comment_window->set_title (title);
	comment_window->present();
}

void
MixerStrip::setup_comment_editor ()
{
	comment_window = new ArdourWindow (""); // title will be reset to show route
	comment_window->set_skip_taskbar_hint (true);
	comment_window->signal_hide().connect (sigc::mem_fun(*this, &MixerStrip::comment_editor_done_editing));
	comment_window->set_default_size (400, 200);

	comment_area = manage (new TextView());
	comment_area->set_name ("MixerTrackCommentArea");
	comment_area->set_wrap_mode (WRAP_WORD);
	comment_area->set_editable (true);
	comment_area->get_buffer()->set_text (_route->comment());
	comment_area->show ();

	comment_window->add (*comment_area);
}

void
MixerStrip::comment_changed (void *src)
{
	ENSURE_GUI_THREAD (*this, &MixerStrip::comment_changed, src)

	if (src != this) {
		ignore_comment_edit = true;
		if (comment_area) {
			comment_area->get_buffer()->set_text (_route->comment());
		}
		ignore_comment_edit = false;
	}
}

bool
MixerStrip::select_route_group (GdkEventButton *ev)
{
	using namespace Menu_Helpers;

	if (ev->button == 1) {

		if (group_menu == 0) {

			PropertyList* plist = new PropertyList();

			plist->add (Properties::gain, true);
			plist->add (Properties::mute, true);
			plist->add (Properties::solo, true);

			group_menu = new RouteGroupMenu (_session, plist);
		}

		WeakRouteList r;
		r.push_back (route ());
		group_menu->build (r);
		group_menu->menu()->popup (1, ev->time);
	}

	return true;
}

void
MixerStrip::route_group_changed ()
{
	ENSURE_GUI_THREAD (*this, &MixerStrip::route_group_changed)

	RouteGroup *rg = _route->route_group();

	if (rg) {
		group_button.set_text (PBD::short_version (rg->name(), 5));
	} else {
		switch (_width) {
		case Wide:
			group_button.set_text (_("Grp"));
			break;
		case Narrow:
			group_button.set_text (_("~G"));
			break;
		}
	}
}

void
MixerStrip::route_color_changed ()
{
	name_button.modify_bg (STATE_NORMAL, color());
	top_event_box.modify_bg (STATE_NORMAL, color());
	reset_strip_style ();
}

void
MixerStrip::show_passthru_color ()
{
	reset_strip_style ();
}

void
MixerStrip::build_route_ops_menu ()
{
	using namespace Menu_Helpers;
	route_ops_menu = new Menu;
	route_ops_menu->set_name ("ArdourContextMenu");

	MenuList& items = route_ops_menu->items();

	items.push_back (MenuElem (_("Comments..."), sigc::mem_fun (*this, &MixerStrip::open_comment_editor)));
	if (!_route->is_master()) {
		items.push_back (MenuElem (_("Save As Template..."), sigc::mem_fun(*this, &RouteUI::save_as_template)));
	}
	items.push_back (MenuElem (_("Rename..."), sigc::mem_fun(*this, &RouteUI::route_rename)));
	rename_menu_item = &items.back();

	items.push_back (SeparatorElem());
	items.push_back (CheckMenuElem (_("Active")));
	CheckMenuItem* i = dynamic_cast<CheckMenuItem *> (&items.back());
	i->set_active (_route->active());
	i->signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &RouteUI::set_route_active), !_route->active(), false));

	items.push_back (SeparatorElem());

	items.push_back (MenuElem (_("Adjust Latency..."), sigc::mem_fun (*this, &RouteUI::adjust_latency)));

	items.push_back (SeparatorElem());
	items.push_back (CheckMenuElem (_("Protect Against Denormals"), sigc::mem_fun (*this, &RouteUI::toggle_denormal_protection)));
	denormal_menu_item = dynamic_cast<CheckMenuItem *> (&items.back());
	denormal_menu_item->set_active (_route->denormal_protection());

	if (!Profile->get_sae()) {
		items.push_back (SeparatorElem());
		items.push_back (MenuElem (_("Remote Control ID..."), sigc::mem_fun (*this, &RouteUI::open_remote_control_id_dialog)));
	}

	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Remove"), sigc::bind (sigc::mem_fun(*this, &RouteUI::remove_this_route), false)));
}

gboolean
MixerStrip::name_button_button_press (GdkEventButton* ev)
{
	/* show menu for either button 1 or 3, so as not to confuse people
	   and also not hide stuff from them.
	*/

	if (ev->button == 3 || ev->button == 1) {
		list_route_operations ();

		/* do not allow rename if the track is record-enabled */
		rename_menu_item->set_sensitive (!_route->record_enabled());
		route_ops_menu->popup (1, ev->time);
	}

	return false;
}

void
MixerStrip::list_route_operations ()
{
	delete route_ops_menu;
	build_route_ops_menu ();
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
MixerStrip::property_changed (const PropertyChange& what_changed)
{
	RouteUI::property_changed (what_changed);

	if (what_changed.contains (ARDOUR::Properties::name)) {
		name_changed ();
	}
}

void
MixerStrip::name_changed ()
{
	switch (_width) {
	case Wide:
		name_button.set_text (_route->name());
		break;
	case Narrow:
		name_button.set_text (PBD::short_version (_route->name(), 5));
		break;
	}

	ARDOUR_UI::instance()->set_tip (name_button, _route->name());
}

void
MixerStrip::name_button_resized (Gtk::Allocation& alloc)
{
	name_button.layout()->set_width (alloc.get_width() * PANGO_SCALE);
}

bool
MixerStrip::width_button_pressed (GdkEventButton* ev)
{
	if (ev->button != 1) {
		return false;
	}
	
	if (Keyboard::modifier_state_contains (ev->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier | Keyboard::TertiaryModifier)) && _mixer_owned) {
		switch (_width) {
		case Wide:
			_mixer.set_strip_width (Narrow);
			break;

		case Narrow:
			_mixer.set_strip_width (Wide);
			break;
		}
	} else {
		switch (_width) {
		case Wide:
			set_width_enum (Narrow, this);
			break;
		case Narrow:
			set_width_enum (Wide, this);
			break;
		}
	}

	return true;
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
	ENSURE_GUI_THREAD (*this, &MixerStrip::map_frozen)

	boost::shared_ptr<AudioTrack> at = audio_track();

	if (at) {
		switch (at->freeze_state()) {
		case AudioTrack::Frozen:
			processor_box.set_sensitive (false);
			hide_redirect_editors ();
			break;
		default:
			processor_box.set_sensitive (true);
			// XXX need some way, maybe, to retoggle redirect editors
			break;
		}
	}
}

void
MixerStrip::hide_redirect_editors ()
{
	_route->foreach_processor (sigc::mem_fun (*this, &MixerStrip::hide_processor_editor));
}

void
MixerStrip::hide_processor_editor (boost::weak_ptr<Processor> p)
{
	boost::shared_ptr<Processor> processor (p.lock ());
	if (!processor) {
		return;
	}

	Gtk::Window* w = processor_box.get_processor_ui (processor);

	if (w) {
		w->hide ();
	}
}

void
MixerStrip::reset_strip_style ()
{
	if (_current_delivery && boost::dynamic_pointer_cast<Send>(_current_delivery)) {

		gpm.set_fader_name ("SendStripBase");

	} else {

		if (is_midi_track()) {
			if (_route->active()) {
				set_name ("MidiTrackStripBase");
			} else {
				set_name ("MidiTrackStripBaseInactive");
			}
			gpm.set_fader_name ("MidiTrackFader");
		} else if (is_audio_track()) {
			if (_route->active()) {
				set_name ("AudioTrackStripBase");
			} else {
				set_name ("AudioTrackStripBaseInactive");
			}
			gpm.set_fader_name ("AudioTrackFader");
		} else {
			if (_route->active()) {
				set_name ("AudioBusStripBase");
			} else {
				set_name ("AudioBusStripBaseInactive");
			}
			gpm.set_fader_name ("AudioBusFader");

			/* (no MIDI busses yet) */
		}
	}
}


void
MixerStrip::engine_stopped ()
{
}

void
MixerStrip::engine_running ()
{
}

string
MixerStrip::meter_point_string (MeterPoint mp)
{
	switch (_width) {
	case Wide:
		switch (mp) {
		case MeterInput:
			return _("in");
			break;
			
		case MeterPreFader:
			return _("pre");
			break;
			
		case MeterPostFader:
			return _("post");
			break;
			
		case MeterOutput:
			return _("out");
			break;
			
		case MeterCustom:
		default:
			return _("custom");
			break;
		}
		break;
	case Narrow:
		switch (mp) {
		case MeterInput:
			return _("in");
			break;
			
		case MeterPreFader:
			return _("pr");
			break;
			
		case MeterPostFader:
			return _("po");
			break;
			
		case MeterOutput:
			return _("o");
			break;
			
		case MeterCustom:
		default:
			return _("c");
			break;
		}
		break;
	}

	return string();
}

/** Called when the metering point has changed */
void
MixerStrip::meter_changed ()
{
	meter_point_button.set_text (meter_point_string (_route->meter_point()));
	gpm.setup_meters ();
	// reset peak when meter point changes
	gpm.reset_peak_display();
}

/** The bus that we are displaying sends to has changed, or been turned off.
 *  @param send_to New bus that we are displaying sends to, or 0.
 */
void
MixerStrip::bus_send_display_changed (boost::shared_ptr<Route> send_to)
{
	RouteUI::bus_send_display_changed (send_to);

	if (send_to) {
		boost::shared_ptr<Send> send = _route->internal_send_for (send_to);

		if (send) {
			show_send (send);
		} else {
			revert_to_default_display ();
		}
	} else {
		revert_to_default_display ();
	}
}

void
MixerStrip::drop_send ()
{
	boost::shared_ptr<Send> current_send;

	if (_current_delivery && ((current_send = boost::dynamic_pointer_cast<Send>(_current_delivery)) != 0)) {
		current_send->set_metering (false);
	}

	send_gone_connection.disconnect ();
	input_button.set_sensitive (true);
	output_button.set_sensitive (true);
	group_button.set_sensitive (true);
	set_invert_sensitive (true);
	meter_point_button.set_sensitive (true);
	mute_button->set_sensitive (true);
	solo_button->set_sensitive (true);
	rec_enable_button->set_sensitive (true);
	solo_isolated_led->set_sensitive (true);
	solo_safe_led->set_sensitive (true);
	monitor_input_button->set_sensitive (true);
	monitor_disk_button->set_sensitive (true);
	_comment_button.set_sensitive (true);
}

void
MixerStrip::set_current_delivery (boost::shared_ptr<Delivery> d)
{
	_current_delivery = d;
	DeliveryChanged (_current_delivery);
}

void
MixerStrip::show_send (boost::shared_ptr<Send> send)
{
	assert (send != 0);

	drop_send ();

	set_current_delivery (send);

	send->set_metering (true);
	_current_delivery->DropReferences.connect (send_gone_connection, invalidator (*this), boost::bind (&MixerStrip::revert_to_default_display, this), gui_context());

	gain_meter().set_controls (_route, send->meter(), send->amp());
	gain_meter().setup_meters ();

	panner_ui().set_panner (_current_delivery->panner_shell(), _current_delivery->panner());
	panner_ui().setup_pan ();

	/* make sure the send has audio output */

	if (_current_delivery->output() && _current_delivery->output()->n_ports().n_audio() > 0) {
		panners.show_all ();
	} else {
		panners.hide_all ();
	}

	input_button.set_sensitive (false);
	group_button.set_sensitive (false);
	set_invert_sensitive (false);
	meter_point_button.set_sensitive (false);
	mute_button->set_sensitive (false);
	solo_button->set_sensitive (false);
	rec_enable_button->set_sensitive (false);
	solo_isolated_led->set_sensitive (false);
	solo_safe_led->set_sensitive (false);
	monitor_input_button->set_sensitive (false);
	monitor_disk_button->set_sensitive (false);
	_comment_button.set_sensitive (false);

	if (boost::dynamic_pointer_cast<InternalSend>(send)) {
		output_button.set_sensitive (false);
	}

	reset_strip_style ();
}

void
MixerStrip::revert_to_default_display ()
{
	drop_send ();

	set_current_delivery (_route->main_outs ());

	gain_meter().set_controls (_route, _route->shared_peak_meter(), _route->amp());
	gain_meter().setup_meters ();

	panner_ui().set_panner (_route->main_outs()->panner_shell(), _route->main_outs()->panner());
	panner_ui().setup_pan ();

	if (has_audio_outputs ()) {
		panners.show_all ();
	} else {
		panners.hide_all ();
	}

	reset_strip_style ();
}

void
MixerStrip::set_button_names ()
{
	switch (_width) {
	case Wide:
		rec_enable_button->set_text (_("Rec"));
		mute_button->set_text (_("Mute"));
		monitor_input_button->set_text (_("In"));
		monitor_disk_button->set_text (_("Disk"));

		if (_route && _route->solo_safe()) {
			if (solo_safe_pixbuf == 0) {
				solo_safe_pixbuf = ::get_icon("solo-safe-icon");
			}
			solo_button->set_image (solo_safe_pixbuf);
			solo_button->set_text (string());
		} else {
			solo_button->set_image (Glib::RefPtr<Gdk::Pixbuf>());
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
		solo_isolated_led->set_text (_("iso"));
		solo_safe_led->set_text (_("lock"));
		break;

	default:
		rec_enable_button->set_text (_("R"));
		mute_button->set_text (_("M"));
		monitor_input_button->set_text (_("I"));
		monitor_disk_button->set_text (_("D"));
		if (_route && _route->solo_safe()) {
			solo_button->remove ();
			if (solo_safe_pixbuf == 0) {
				solo_safe_pixbuf =::get_icon("solo-safe-icon");
			}
			solo_button->set_image (solo_safe_pixbuf);
			solo_button->set_text (string());
		} else {
			solo_button->set_image (Glib::RefPtr<Gdk::Pixbuf>());
			if (!Config->get_solo_control_is_listen_control()) {
				solo_button->set_text (_("S"));
			} else {
				switch (Config->get_listen_position()) {
				case AfterFaderListen:
					solo_button->set_text (_("A"));
					break;
				case PreFaderListen:
					solo_button->set_text (_("P"));
					break;
				}
			}
		}
		solo_isolated_led->set_text (_("i"));
		solo_safe_led->set_text (_("L"));
		break;
	}

	if (_route) {
		meter_point_button.set_text (meter_point_string (_route->meter_point()));
	} else {
		meter_point_button.set_text ("");
	}
}

PluginSelector*
MixerStrip::plugin_selector()
{
	return _mixer.plugin_selector();
}

void
MixerStrip::hide_things ()
{
	processor_box.hide_things ();
}

bool
MixerStrip::input_active_button_press (GdkEventButton*)
{
	/* nothing happens on press */
	return true;
}

bool
MixerStrip::input_active_button_release (GdkEventButton* ev)
{
	boost::shared_ptr<MidiTrack> mt = midi_track ();

	if (!mt) {
		return true;
	}

	boost::shared_ptr<RouteList> rl (new RouteList);

	rl->push_back (route());

	_session->set_exclusive_input_active (rl, !mt->input_active(),
					      Keyboard::modifier_state_contains (ev->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::SecondaryModifier)));

	return true;
}

void
MixerStrip::midi_input_status_changed ()
{
	if (midi_input_enable_button) {
		boost::shared_ptr<MidiTrack> mt = midi_track ();
		assert (mt);
		midi_input_enable_button->set_active (mt->input_active ());
	}
}

string
MixerStrip::state_id () const
{
	return string_compose ("strip %1", _route->id().to_s());
}

void
MixerStrip::parameter_changed (string p)
{
	if (p == _visibility.get_state_name()) {
		/* The user has made changes to the mixer strip visibility, so get
		   our VisibilityGroup to reflect these changes in our widgets.
		*/
		_visibility.set_state (Config->get_mixer_strip_visibility ());
	}
}

/** Called to decide whether the solo isolate / solo lock button visibility should
 *  be overridden from that configured by the user.  We do this for the master bus.
 *
 *  @return optional value that is present if visibility state should be overridden.
 */
boost::optional<bool>
MixerStrip::override_solo_visibility () const
{
	if (_route && _route->is_master ()) {
		return boost::optional<bool> (false);
	}
	
	return boost::optional<bool> ();
}

void
MixerStrip::add_input_port (DataType t)
{
	_route->input()->add_port ("", this, t);
}

void
MixerStrip::add_output_port (DataType t)
{
	_route->output()->add_port ("", this, t);
}

void
MixerStrip::route_active_changed ()
{
	reset_strip_style ();
}

void
MixerStrip::copy_processors ()
{
	processor_box.processor_operation (ProcessorBox::ProcessorsCopy);
}

void
MixerStrip::cut_processors ()
{
	processor_box.processor_operation (ProcessorBox::ProcessorsCut);
}

void
MixerStrip::paste_processors ()
{
	processor_box.processor_operation (ProcessorBox::ProcessorsPaste);
}

void
MixerStrip::select_all_processors ()
{
	processor_box.processor_operation (ProcessorBox::ProcessorsSelectAll);
}

void
MixerStrip::delete_processors ()
{
	processor_box.processor_operation (ProcessorBox::ProcessorsDelete);
}

void
MixerStrip::toggle_processors ()
{
	processor_box.processor_operation (ProcessorBox::ProcessorsToggleActive);
}

void
MixerStrip::ab_plugins ()
{
	processor_box.processor_operation (ProcessorBox::ProcessorsAB);
}

bool
MixerStrip::level_meter_button_press (GdkEventButton* ev)
{
	if (ev->button == 3) {
		popup_level_meter_menu (ev);
		return true;
	}

	return false;
}

void
MixerStrip::popup_level_meter_menu (GdkEventButton* ev)
{
	using namespace Gtk::Menu_Helpers;

	Gtk::Menu* m = manage (new Menu);
	MenuList& items = m->items ();

	RadioMenuItem::Group group;

	_suspend_menu_callbacks = true;
	add_level_meter_item_point (items, group, _("Input"), MeterInput);
	add_level_meter_item_point (items, group, _("Pre-fader"), MeterPreFader);
	add_level_meter_item_point (items, group, _("Post-fader"), MeterPostFader);
	add_level_meter_item_point (items, group, _("Output"), MeterOutput);
	add_level_meter_item_point (items, group, _("Custom"), MeterCustom);

	RadioMenuItem::Group tgroup;
	items.push_back (SeparatorElem());

	add_level_meter_item_type (items, tgroup, _("Peak"), MeterPeak);
	add_level_meter_item_type (items, tgroup, _("RMS + Peak"), MeterKrms);

	m->popup (ev->button, ev->time);
	_suspend_menu_callbacks = false;
}

void
MixerStrip::add_level_meter_item_point (Menu_Helpers::MenuList& items,
		RadioMenuItem::Group& group, string const & name, MeterPoint point)
{
	using namespace Menu_Helpers;
	
	items.push_back (RadioMenuElem (group, name, sigc::bind (sigc::mem_fun (*this, &MixerStrip::set_meter_point), point)));
	RadioMenuItem* i = dynamic_cast<RadioMenuItem *> (&items.back ());
	i->set_active (_route->meter_point() == point);
}

void
MixerStrip::set_meter_point (MeterPoint p)
{
	if (_suspend_menu_callbacks) return;
	_route->set_meter_point (p);
}

void
MixerStrip::add_level_meter_item_type (Menu_Helpers::MenuList& items,
		RadioMenuItem::Group& group, string const & name, MeterType type)
{
	using namespace Menu_Helpers;
	
	items.push_back (RadioMenuElem (group, name, sigc::bind (sigc::mem_fun (*this, &MixerStrip::set_meter_type), type)));
	RadioMenuItem* i = dynamic_cast<RadioMenuItem *> (&items.back ());
	i->set_active (_route->meter_type() == type);
}

void
MixerStrip::set_meter_type (MeterType t)
{
	if (_suspend_menu_callbacks) return;
	gpm.set_type (t);
}
