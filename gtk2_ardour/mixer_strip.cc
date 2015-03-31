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
#include "ardour/meter.h"
#include "ardour/midi_track.h"
#include "ardour/pannable.h"
#include "ardour/panner.h"
#include "ardour/panner_shell.h"
#include "ardour/panner_manager.h"
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
#include "meter_patterns.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace std;
using namespace ArdourMeter;

MixerStrip* MixerStrip::_entered_mixer_strip;

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
	, rec_mon_table (2, 2)
	, solo_iso_table (1, 2)
	, mute_solo_table (1, 2)
	, bottom_button_table (1, 3)
	, meter_point_button (_("pre"))
	, midi_input_enable_button (0)
	, _comment_button (_("Comments"))
	, _visibility (X_("mixer-element-visibility"))
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
	, rec_mon_table (2, 2)
	, solo_iso_table (1, 2)
	, mute_solo_table (1, 2)
	, bottom_button_table (1, 3)
	, meter_point_button (_("pre"))
	, midi_input_enable_button (0)
	, _comment_button (_("Comments"))
	, _visibility (X_("mixer-element-visibility"))
{
	init ();
	set_route (rt);
}

void
MixerStrip::init ()
{
	_entered_mixer_strip= 0;
	group_menu = 0;
	route_ops_menu = 0;
	ignore_comment_edit = false;
	ignore_toggle = false;
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

	input_button_box.set_spacing(2);
	
	input_button.set_text (_("Input"));
	input_button.set_name ("mixer strip button");
	input_button_box.pack_start (input_button, true, true);

	output_button.set_text (_("Output"));
	output_button.set_name ("mixer strip button");

	ARDOUR_UI::instance()->set_tip (&meter_point_button, _("Click to select metering point"), "");
	meter_point_button.set_name ("mixer strip button");

	bottom_button_table.attach (meter_point_button, 2, 3, 0, 1);

	meter_point_button.signal_button_press_event().connect (sigc::mem_fun (gpm, &GainMeter::meter_press), false);
	meter_point_button.signal_button_release_event().connect (sigc::mem_fun (gpm, &GainMeter::meter_release), false);
	
	hide_button.set_events (hide_button.get_events() & ~(Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK));

	solo_isolated_led = manage (new ArdourButton (ArdourButton::led_default_elements));
	solo_isolated_led->show ();
	solo_isolated_led->set_no_show_all (true);
	solo_isolated_led->set_name (X_("solo isolate"));
	solo_isolated_led->add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);
	solo_isolated_led->signal_button_release_event().connect (sigc::mem_fun (*this, &RouteUI::solo_isolate_button_release), false);
	UI::instance()->set_tip (solo_isolated_led, _("Isolate Solo"), "");

	solo_safe_led = manage (new ArdourButton (ArdourButton::led_default_elements));
	solo_safe_led->show ();
	solo_safe_led->set_no_show_all (true);
	solo_safe_led->set_name (X_("solo safe"));
	solo_safe_led->add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);
	solo_safe_led->signal_button_release_event().connect (sigc::mem_fun (*this, &RouteUI::solo_safe_button_release), false);
	UI::instance()->set_tip (solo_safe_led, _("Lock Solo Status"), "");

	solo_safe_led->set_text (_("Lock"));
	solo_isolated_led->set_text (_("Iso"));

	solo_iso_table.set_homogeneous (true);
	solo_iso_table.set_spacings (2);
	if (!ARDOUR::Profile->get_trx()) {
		solo_iso_table.attach (*solo_isolated_led, 0, 1, 0, 1);
		solo_iso_table.attach (*solo_safe_led, 1, 2, 0, 1);
	}
	solo_iso_table.show ();

	rec_mon_table.set_homogeneous (true);
	rec_mon_table.set_row_spacings (2);
	rec_mon_table.set_col_spacings (2);
	if (ARDOUR::Profile->get_mixbus()) {
		rec_mon_table.resize (1, 3);
		rec_mon_table.attach (*monitor_input_button, 1, 2, 0, 1);
		rec_mon_table.attach (*monitor_disk_button, 2, 3, 0, 1);
	} else if (!ARDOUR::Profile->get_trx()) {
		rec_mon_table.attach (*monitor_input_button, 1, 2, 0, 1);
		rec_mon_table.attach (*monitor_disk_button, 1, 2, 1, 2);
	}
	rec_mon_table.show ();

	if (solo_isolated_led) {
		button_size_group->add_widget (*solo_isolated_led);
	}
	if (solo_safe_led) {
		button_size_group->add_widget (*solo_safe_led);
	}

	if (!ARDOUR::Profile->get_mixbus()) {
		if (rec_enable_button) {
			button_size_group->add_widget (*rec_enable_button);
		}
		if (monitor_disk_button) {
			button_size_group->add_widget (*monitor_disk_button);
		}
		if (monitor_input_button) {
			button_size_group->add_widget (*monitor_input_button);
		}
	}
	
	mute_solo_table.set_homogeneous (true);
	mute_solo_table.set_spacings (2);

	bottom_button_table.set_spacings (2);
	bottom_button_table.set_homogeneous (true);
	bottom_button_table.attach (group_button, 1, 2, 0, 1);
	bottom_button_table.attach (gpm.gain_automation_state_button, 0, 1, 0, 1);

	name_button.set_name ("mixer strip button");
	name_button.set_text_ellipsize (Pango::ELLIPSIZE_END);
	name_button.signal_size_allocate().connect (sigc::mem_fun (*this, &MixerStrip::name_button_resized));

	ARDOUR_UI::instance()->set_tip (&group_button, _("Mix group"), "");
	group_button.set_name ("mixer strip button");

	_comment_button.set_name (X_("mixer strip button"));
	_comment_button.signal_clicked.connect (sigc::mem_fun (*this, &RouteUI::toggle_comment_editor));

	global_vpacker.set_border_width (1);
	global_vpacker.set_spacing (0);

	width_button.set_name ("mixer strip button");
	hide_button.set_name ("mixer strip button");

	width_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MixerStrip::width_button_pressed), false);
	hide_button.signal_clicked.connect (sigc::mem_fun(*this, &MixerStrip::hide_clicked));

//	width_hide_box.set_border_width (1);
	width_hide_box.set_spacing (2);
	width_hide_box.pack_start (width_button, false, true);
	width_hide_box.pack_start (number_label, true, true);
	width_hide_box.pack_end (hide_button, false, true);

	number_label.set_text ("-");
	number_label.set_elements((ArdourButton::Element)(ArdourButton::Edge|ArdourButton::Body|ArdourButton::Text|ArdourButton::Inactive));
	number_label.set_no_show_all ();
	number_label.set_name ("tracknumber label");
	number_label.set_fixed_colors (0x80808080, 0x80808080);
	number_label.set_alignment (.5, .5);
	number_label.set_fallthrough_to_parent (true);

	global_vpacker.set_spacing (2);
	if (!ARDOUR::Profile->get_trx()) {
		global_vpacker.pack_start (width_hide_box, Gtk::PACK_SHRINK);
		global_vpacker.pack_start (name_button, Gtk::PACK_SHRINK);
		global_vpacker.pack_start (input_button_box, Gtk::PACK_SHRINK);
		global_vpacker.pack_start (_invert_button_box, Gtk::PACK_SHRINK);
		global_vpacker.pack_start (processor_box, true, true);
	}
	global_vpacker.pack_start (panners, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (rec_mon_table, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (solo_iso_table, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (mute_solo_table, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (gpm, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (bottom_button_table, Gtk::PACK_SHRINK);
	if (!ARDOUR::Profile->get_trx()) {
		global_vpacker.pack_start (output_button, Gtk::PACK_SHRINK);
		global_vpacker.pack_start (_comment_button, Gtk::PACK_SHRINK);
	} else {
		global_vpacker.pack_start (name_button, Gtk::PACK_SHRINK);
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

	_session->engine().Stopped.connect (*this, invalidator (*this), boost::bind (&MixerStrip::engine_stopped, this), gui_context());
	_session->engine().Running.connect (*this, invalidator (*this), boost::bind (&MixerStrip::engine_running, this), gui_context());

	input_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MixerStrip::input_press), false);
	input_button.signal_button_release_event().connect (sigc::mem_fun(*this, &MixerStrip::input_release), false);
	input_button.signal_size_allocate().connect (sigc::mem_fun (*this, &MixerStrip::input_button_resized));

	input_button.set_text_ellipsize (Pango::ELLIPSIZE_MIDDLE);
	output_button.set_text_ellipsize (Pango::ELLIPSIZE_MIDDLE);

	output_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MixerStrip::output_press), false);
	output_button.signal_button_release_event().connect (sigc::mem_fun(*this, &MixerStrip::output_release), false);
	output_button.signal_size_allocate().connect (sigc::mem_fun (*this, &MixerStrip::output_button_resized));

	number_label.signal_button_press_event().connect (sigc::mem_fun(*this, &MixerStrip::number_button_button_press), false);

	name_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MixerStrip::name_button_button_press), false);
	name_button.signal_button_release_event().connect (sigc::mem_fun(*this, &MixerStrip::name_button_button_release), false);

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
	_visibility.add (&input_button_box, X_("Input"), _("Input"), false);
	_visibility.add (&_invert_button_box, X_("PhaseInvert"), _("Phase Invert"), false);
	_visibility.add (&rec_mon_table, X_("RecMon"), _("Record & Monitor"), false);
	_visibility.add (&solo_iso_table, X_("SoloIsoLock"), _("Solo Iso / Lock"), false);
	_visibility.add (&output_button, X_("Output"), _("Output"), false);
	_visibility.add (&_comment_button, X_("Comments"), _("Comments"), false);

	parameter_changed (X_("mixer-element-visibility"));
	ARDOUR_UI::config()->ParameterChanged.connect (sigc::mem_fun (*this, &MixerStrip::parameter_changed));
	Config->ParameterChanged.connect (_config_connection, MISSING_INVALIDATOR, boost::bind (&MixerStrip::parameter_changed, this, _1), gui_context());
	_session->config.ParameterChanged.connect (_config_connection, MISSING_INVALIDATOR, boost::bind (&MixerStrip::parameter_changed, this, _1), gui_context());

	//watch for mouse enter/exit so we can do some stuff
	signal_enter_notify_event().connect (sigc::mem_fun(*this, &MixerStrip::mixer_strip_enter_event ));
	signal_leave_notify_event().connect (sigc::mem_fun(*this, &MixerStrip::mixer_strip_leave_event ));

	gpm.LevelMeterButtonPress.connect_same_thread (_level_meter_connection, boost::bind (&MixerStrip::level_meter_button_press, this, _1));
}

MixerStrip::~MixerStrip ()
{
	CatchDeletion (this);

	if (this ==_entered_mixer_strip)
		_entered_mixer_strip = NULL;
}

bool
MixerStrip::mixer_strip_enter_event (GdkEventCrossing* /*ev*/)
{
	_entered_mixer_strip = this;
	
	//although we are triggering on the "enter", to the user it will appear that it is happenin on the "leave"
	//because the mixerstrip control is a parent that encompasses the strip
	deselect_all_processors();

	return false;
}

bool
MixerStrip::mixer_strip_leave_event (GdkEventCrossing *ev)
{
	//if we have moved outside our strip, but not into a child view, then deselect ourselves
	if ( !(ev->detail == GDK_NOTIFY_INFERIOR) ) {
		_entered_mixer_strip= 0;

		//clear keyboard focus in the gain display.  this is cheesy but fixes a longstanding "bug" where the user starts typing in the gain entry, and leaves it active, thereby prohibiting other keybindings from working
		gpm.gain_display.set_sensitive(false);
		gpm.show_gain();
		gpm.gain_display.set_sensitive(true);

		//if we leave this mixer strip we need to clear out any selections
		//processor_box.processor_display.select_none();  //but this doesn't work, because it gets triggered when (for example) you open the menu or start a drag
	}
	
	return false;
}

void
MixerStrip::set_route (boost::shared_ptr<Route> rt)
{
	//the rec/monitor stuff only shows up for tracks.
	//the show_sends only shows up for buses.
	//remove them all here, and we may add them back later
	if (show_sends_button->get_parent()) {
		rec_mon_table.remove (*show_sends_button);
	}
	if (rec_enable_button->get_parent()) {
		rec_mon_table.remove (*rec_enable_button);
	}
	if (monitor_input_button->get_parent()) {
		rec_mon_table.remove (*monitor_input_button);
	}
	if (monitor_disk_button->get_parent()) {
		rec_mon_table.remove (*monitor_disk_button);
	}
	if (group_button.get_parent()) {
		bottom_button_table.remove (group_button);
	}
	
	RouteUI::set_route (rt);

	/* ProcessorBox needs access to _route so that it can read
	   GUI object state.
	*/
	processor_box.set_route (rt);

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
	
	mute_solo_table.attach (gpm.gain_display,0,1,1,2, EXPAND|FILL, EXPAND);
	mute_solo_table.attach (gpm.peak_display,1,2,1,2, EXPAND|FILL, EXPAND);

	if (solo_button->get_parent()) {
		mute_solo_table.remove (*solo_button);
	}

	if (mute_button->get_parent()) {
		mute_solo_table.remove (*mute_button);
	}

	if (route()->is_master()) {
		mute_solo_table.attach (*mute_button, 0, 2, 0, 1);
		solo_button->hide ();
		mute_button->show ();
		rec_mon_table.hide ();
		if (solo_iso_table.get_parent()) {
			solo_iso_table.get_parent()->remove(solo_iso_table);
		}
	} else {
		bottom_button_table.attach (group_button, 1, 2, 0, 1);
		mute_solo_table.attach (*mute_button, 0, 1, 0, 1);
		mute_solo_table.attach (*solo_button, 1, 2, 0, 1);
		mute_button->show ();
		solo_button->show ();
		rec_mon_table.show ();
	}

	if (_mixer_owned && route()->is_master() ) {

		HScrollbar scrollbar;
		Gtk::Requisition requisition(scrollbar.size_request ());
		int scrollbar_height = requisition.height;

		spacer = manage (new EventBox);
		spacer->set_size_request (-1, scrollbar_height+2);
		global_vpacker.pack_start (*spacer, false, false);
		spacer->show();
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

		rec_mon_table.attach (*rec_enable_button, 0, 1, 0, ARDOUR::Profile->get_mixbus() ? 1 : 2);
		rec_enable_button->set_sensitive (_session->writable());
		rec_enable_button->show();

		if (ARDOUR::Profile->get_mixbus()) {
			rec_mon_table.attach (*monitor_input_button, 1, 2, 0, 1);
			rec_mon_table.attach (*monitor_disk_button, 2, 3, 0, 1);
		} else if (ARDOUR::Profile->get_trx()) {
			rec_mon_table.attach (*monitor_input_button, 1, 2, 0, 2);
		} else {
			rec_mon_table.attach (*monitor_input_button, 1, 2, 0, 1);
			rec_mon_table.attach (*monitor_disk_button, 1, 2, 1, 2);
		}

	} else {

		/* non-master bus */

		if (!_route->is_master()) {
			rec_mon_table.attach (*show_sends_button, 0, 1, 0, 2);
			show_sends_button->show();
		}
	}

	meter_point_button.set_text (meter_point_string (_route->meter_point()));

	delete route_ops_menu;
	route_ops_menu = 0;

	_route->meter_change.connect (route_connections, invalidator (*this), bind (&MixerStrip::meter_changed, this), gui_context());
	_route->input()->changed.connect (*this, invalidator (*this), boost::bind (&MixerStrip::update_output_display, this), gui_context());
	_route->output()->changed.connect (*this, invalidator (*this), boost::bind (&MixerStrip::update_output_display, this), gui_context());
	_route->route_group_changed.connect (route_connections, invalidator (*this), boost::bind (&MixerStrip::route_group_changed, this), gui_context());

	_route->io_changed.connect (route_connections, invalidator (*this), boost::bind (&MixerStrip::io_changed_proxy, this), gui_context ());

	if (_route->panner_shell()) {
		update_panner_choices();
		_route->panner_shell()->Changed.connect (route_connections, invalidator (*this), boost::bind (&MixerStrip::connect_to_pan, this), gui_context());
	}

	if (is_audio_track()) {
		audio_track()->DiskstreamChanged.connect (route_connections, invalidator (*this), boost::bind (&MixerStrip::diskstream_changed, this), gui_context());
	}

	_route->comment_changed.connect (route_connections, invalidator (*this), boost::bind (&MixerStrip::setup_comment_button, this), gui_context());
	_route->PropertyChanged.connect (route_connections, invalidator (*this), boost::bind (&MixerStrip::property_changed, this, _1), gui_context());

	set_stuff_from_route ();

	/* now force an update of all the various elements */

	update_mute_display ();
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
		number_label.show();
	}

	gpm.reset_peak_display ();
	gpm.gain_display.show ();
	gpm.peak_display.show ();

	width_button.show();
	width_hide_box.show();
	global_frame.show();
	global_vpacker.show();
	mute_solo_table.show();
	bottom_button_table.show();
	gpm.show_all ();
	meter_point_button.show();
	input_button_box.show_all();
	output_button.show();
	name_button.show();
	_comment_button.show();
	group_button.show();
	gpm.gain_automation_state_button.show();

	parameter_changed ("mixer-element-visibility");

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
			show_sends_button->set_text (_("Aux"));
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


		set_size_request (max (110, gpm.get_gm_width()+5), -1);
		break;

	case Narrow:

		if (show_sends_button) {
			show_sends_button->set_text (_("Snd"));
		}

		gpm.gain_automation_style_button.set_text (
				gpm.short_astyle_string(gain_automation->automation_style()));
		gpm.gain_automation_state_button.set_text (
				gpm.short_astate_string(gain_automation->automation_state()));
		gain_meter().setup_meters (); // recalc meter width

		if (_route->panner()) {
			((Gtk::Label*)panners.pan_automation_style_button.get_child())->set_text (
			panners.short_astyle_string(_route->panner()->automation_style()));
			((Gtk::Label*)panners.pan_automation_state_button.get_child())->set_text (
			panners.short_astate_string(_route->panner()->automation_state()));
		}

		set_size_request (max (60, gpm.get_gm_width() + 10), -1);
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
MixerStrip::output_release (GdkEventButton *ev)
{
	switch (ev->button) {
	case 3:
		edit_output_configuration ();
		break;
	}
	
	return false;
}

gint
MixerStrip::output_press (GdkEventButton *ev)
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
			maybe_add_bundle_to_output_menu ((*i)->output()->bundle(), current);
		}

		if (citems.size() == n_with_separator) {
			/* no routes added; remove the separator */
			citems.pop_back ();
		}

		citems.push_back (SeparatorElem());

		for (DataType::iterator i = DataType::begin(); i != DataType::end(); ++i) {
			citems.push_back (
				MenuElem (
					string_compose (_("Add %1 port"), (*i).to_i18n_string()),
					sigc::bind (sigc::mem_fun (*this, &MixerStrip::add_output_port), *i)
					)
				);
		}
		
		citems.push_back (SeparatorElem());
		citems.push_back (MenuElem (_("Routing Grid"), sigc::mem_fun (*(static_cast<RouteUI*>(this)), &RouteUI::edit_output_configuration)));

		output_menu.popup (1, ev->time);
		break;
	}

	default:
		break;
	}
	return TRUE;
}

gint
MixerStrip::input_release (GdkEventButton *ev)
{
	switch (ev->button) {

	case 3:
		edit_input_configuration ();
		break;
	default:
		break;

	}

	return false;
}


gint
MixerStrip::input_press (GdkEventButton *ev)
{
	using namespace Menu_Helpers;

	MenuList& citems = input_menu.items();
	input_menu.set_name ("ArdourContextMenu");
	citems.clear();

	if (!_session->engine().connected()) {
		MessageDialog msg (_("Not connected to audio engine - no I/O changes are possible"));
		msg.run ();
		return true;
	}

	if (_session->actively_recording() && _route->record_enabled())
		return true;

	switch (ev->button) {

	case 3:
		return false;  //don't handle the mouse-down here.  wait for mouse-up to pop the menu

	case 1:
	{
		citems.push_back (MenuElem (_("Disconnect"), sigc::mem_fun (*(static_cast<RouteUI*>(this)), &RouteUI::disconnect_input)));

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

		citems.push_back (SeparatorElem());
		for (DataType::iterator i = DataType::begin(); i != DataType::end(); ++i) {
			citems.push_back (
				MenuElem (
					string_compose (_("Add %1 port"), (*i).to_i18n_string()),
					sigc::bind (sigc::mem_fun (*this, &MixerStrip::add_input_port), *i)
					)
				);
		}

		citems.push_back (SeparatorElem());
		citems.push_back (MenuElem (_("Routing Grid"), sigc::mem_fun (*(static_cast<RouteUI*>(this)), &RouteUI::edit_input_configuration)));

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
        if (is_track() && input_selector) {
                        input_selector->hide_all ();
        }

        route_color_changed ();
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

	/* This call reduncant, PannerUI::set_panner() connects to _panshell->Changed itself
	 * However, that only works a panner was previously set.
	 *
	 * PannerUI must remain subscribed to _panshell->Changed() in case
	 * we switch the panner eg. AUX-Send and back
	 * _route->panner_shell()->Changed() vs _panshell->Changed
	 */
	if (panners._panner == 0) {
		panners.panshell_changed ();
	}
	update_panner_choices();
}

void
MixerStrip::update_panner_choices ()
{
	ENSURE_GUI_THREAD (*this, &MixerStrip::update_panner_choices)
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

	bool have_label = false;
	bool each_io_has_one_connection = true;

	string connection_name;
	string ardour_track_name;
	string other_connection_type;
	string system_ports;
	string system_port;

	ostringstream tooltip;
	char * tooltip_cstr;
	
	//to avoid confusion, the button caption should only show connections that match the datatype of the track
	DataType dt = DataType::AUDIO;
	if ( boost::dynamic_pointer_cast<MidiTrack>(route) != 0 )
		dt = DataType::MIDI;

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
		
		//ignore any port connections that don't match our DataType
		if (port->type() != dt)
			continue;  

		port_connections.clear ();
		port->get_connections(port_connections);
		io_connection_count = 0;

		if (!port_connections.empty()) {
			for (vector<string>::iterator i = port_connections.begin(); i != port_connections.end(); ++i) {
				string pn = "";
				string& connection_name (*i);

				if (connection_name.find("system:") == 0) {
					pn = AudioEngine::instance()->get_pretty_name_by_name (connection_name);
				}

				if (io_connection_count == 0) {
					tooltip << endl << Glib::Markup::escape_text(port->name().substr(port->name().find("/") + 1))
						<< " -> "
						<< Glib::Markup::escape_text( pn.empty() ? connection_name : pn );
				} else {
					tooltip << ", "
						<< Glib::Markup::escape_text( pn.empty() ? connection_name : pn );
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
				} else if (!pn.empty()) {
					if (system_ports.empty()) {
						system_ports += pn;
					} else {
						system_ports += "/" + pn;
					}
					if (connection_name.find("system:") == 0) {
						++system_connection_count;
					}
				} else if (connection_name.find("system:midi_") == 0) {
					if (for_input) {
						// "system:midi_capture_123" -> "123"
						system_port = "M " + connection_name.substr(20);
					} else {
						// "system:midi_playback_123" -> "123"
						system_port = "M " + connection_name.substr(21);
					}

					if (system_ports.empty()) {
						system_ports += system_port;
					} else {
						system_ports += "/" + system_port;
					}

					++system_connection_count;

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

	if (for_input) {
		input_button.set_text (label.str());
	} else {
		output_button.set_text (label.str());
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
MixerStrip::io_changed_proxy ()
{
	Glib::signal_idle().connect_once (sigc::mem_fun (*this, &MixerStrip::update_panner_choices));
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
	number_label.set_fixed_colors (gdk_color_to_rgba (color()), gdk_color_to_rgba (color()));
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

	items.push_back (MenuElem (_("Color..."), sigc::mem_fun (*this, &RouteUI::choose_color)));

	items.push_back (MenuElem (_("Comments..."), sigc::mem_fun (*this, &RouteUI::open_comment_editor)));

	items.push_back (MenuElem (_("Inputs..."), sigc::mem_fun (*this, &RouteUI::edit_input_configuration)));

	items.push_back (MenuElem (_("Outputs..."), sigc::mem_fun (*this, &RouteUI::edit_output_configuration)));

	items.push_back (SeparatorElem());

	if (!_route->is_master()) {
		items.push_back (MenuElem (_("Save As Template..."), sigc::mem_fun(*this, &RouteUI::save_as_template)));
	}
	items.push_back (MenuElem (_("Rename..."), sigc::mem_fun(*this, &RouteUI::route_rename)));
	rename_menu_item = &items.back();

	items.push_back (SeparatorElem());
	items.push_back (CheckMenuElem (_("Active")));
	Gtk::CheckMenuItem* i = dynamic_cast<Gtk::CheckMenuItem *> (&items.back());
	i->set_active (_route->active());
	i->set_sensitive(! _session->transport_rolling());
	i->signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &RouteUI::set_route_active), !_route->active(), false));

	items.push_back (SeparatorElem());

	items.push_back (MenuElem (_("Adjust Latency..."), sigc::mem_fun (*this, &RouteUI::adjust_latency)));

	items.push_back (SeparatorElem());
	items.push_back (CheckMenuElem (_("Protect Against Denormals"), sigc::mem_fun (*this, &RouteUI::toggle_denormal_protection)));
	denormal_menu_item = dynamic_cast<Gtk::CheckMenuItem *> (&items.back());
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
	if (ev->button == 3) {
		list_route_operations ();

		/* do not allow rename if the track is record-enabled */
		rename_menu_item->set_sensitive (!_route->record_enabled());
		route_ops_menu->popup (1, ev->time);

		return true;
	}

	return false;
}

gboolean
MixerStrip::name_button_button_release (GdkEventButton* ev)
{
	if (ev->button == 1) {
		list_route_operations ();

		/* do not allow rename if the track is record-enabled */
		rename_menu_item->set_sensitive (!_route->record_enabled());
		route_ops_menu->popup (1, ev->time);
	}

	return false;
}

gboolean
MixerStrip::number_button_button_press (GdkEventButton* ev)
{
	if (  ev->button == 3 ) {
		list_route_operations ();

		/* do not allow rename if the track is record-enabled */
		rename_menu_item->set_sensitive (!_route->record_enabled());
		route_ops_menu->popup (1, ev->time);
		
		return true;
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
	
//	if (!yn)
//		processor_box.deselect_all_processors();
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

	if (_session->config.get_track_name_number()) {
		const int64_t track_number = _route->track_number ();
		if (track_number == 0) {
			number_label.set_text ("-");
		} else {
			number_label.set_text (PBD::to_string (abs(_route->track_number ()), std::dec));
		}
	} else {
		number_label.set_text ("");
	}
}

void
MixerStrip::input_button_resized (Gtk::Allocation& alloc)
{
	input_button.set_layout_ellipsize_width (alloc.get_width() * PANGO_SCALE);
}

void
MixerStrip::output_button_resized (Gtk::Allocation& alloc)
{
	output_button.set_layout_ellipsize_width (alloc.get_width() * PANGO_SCALE);
}

void
MixerStrip::name_button_resized (Gtk::Allocation& alloc)
{
	name_button.set_layout_ellipsize_width (alloc.get_width() * PANGO_SCALE);
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
			_mixer.set_strip_width (Narrow, true);
			break;

		case Narrow:
			_mixer.set_strip_width (Wide, true);
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
			return _("In");
			break;
			
		case MeterPreFader:
			return _("Pre");
			break;
			
		case MeterPostFader:
			return _("Post");
			break;
			
		case MeterOutput:
			return _("Out");
			break;
			
		case MeterCustom:
		default:
			return _("Custom");
			break;
		}
		break;
	case Narrow:
		switch (mp) {
		case MeterInput:
			return _("In");
			break;
			
		case MeterPreFader:
			return _("Pr");
			break;
			
		case MeterPostFader:
			return _("Po");
			break;
			
		case MeterOutput:
			return _("O");
			break;
			
		case MeterCustom:
		default:
			return _("C");
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

	send->meter()->set_type(_route->shared_peak_meter()->get_type());
	send->set_metering (true);
	_current_delivery->DropReferences.connect (send_gone_connection, invalidator (*this), boost::bind (&MixerStrip::revert_to_default_display, this), gui_context());

	gain_meter().set_controls (_route, send->meter(), send->amp());
	gain_meter().setup_meters ();

	uint32_t const in = _current_delivery->pans_required();
	uint32_t const out = _current_delivery->pan_outs();

	panner_ui().set_panner (_current_delivery->panner_shell(), _current_delivery->panner());
	panner_ui().set_available_panners(PannerManager::instance().PannerManager::get_available_panners(in, out));
	panner_ui().setup_pan ();
	panner_ui().set_send_drawing_mode (true);
	panner_ui().show_all ();

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
MixerStrip::set_button_names ()
{
	switch (_width) {
	case Wide:
		mute_button->set_text (_("Mute"));
		monitor_input_button->set_text (_("In"));
		monitor_disk_button->set_text (_("Disk"));

		if (_route && _route->solo_safe()) {
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
		solo_isolated_led->set_text (_("Iso"));
		solo_safe_led->set_text (_("Lock"));
		break;

	default:
		mute_button->set_text (_("M"));
		monitor_input_button->set_text (_("I"));
		monitor_disk_button->set_text (_("D"));

		if (_route && _route->solo_safe()) {
			solo_button->set_visual_state (Gtkmm2ext::VisualState (solo_button->visual_state() | Gtkmm2ext::Insensitive));
		} else {
			solo_button->set_visual_state (Gtkmm2ext::VisualState (solo_button->visual_state() & ~Gtkmm2ext::Insensitive));
		}
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

		solo_isolated_led->set_text (_("I"));
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
		_visibility.set_state (ARDOUR_UI::config()->get_mixer_strip_visibility ());
	}
	else if (p == "track-name-number") {
		name_changed ();
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
MixerStrip::deselect_all_processors ()
{
	processor_box.processor_operation (ProcessorBox::ProcessorsSelectNone);
}

bool
MixerStrip::delete_processors ()
{
	return processor_box.processor_operation (ProcessorBox::ProcessorsDelete);
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
	if (_current_delivery && boost::dynamic_pointer_cast<Send>(_current_delivery)) {
		return false;
	}
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
	add_level_meter_item_point (items, group, _("Pre Fader"), MeterPreFader);
	add_level_meter_item_point (items, group, _("Post Fader"), MeterPostFader);
	add_level_meter_item_point (items, group, _("Output"), MeterOutput);
	add_level_meter_item_point (items, group, _("Custom"), MeterCustom);

	if (gpm.meter_channels().n_audio() == 0) {
		m->popup (ev->button, ev->time);
		_suspend_menu_callbacks = false;
		return;
	}

	RadioMenuItem::Group tgroup;
	items.push_back (SeparatorElem());

	add_level_meter_item_type (items, tgroup, ArdourMeter::meter_type_string(MeterPeak), MeterPeak);
	add_level_meter_item_type (items, tgroup, ArdourMeter::meter_type_string(MeterKrms),  MeterKrms);
	add_level_meter_item_type (items, tgroup, ArdourMeter::meter_type_string(MeterIEC1DIN), MeterIEC1DIN);
	add_level_meter_item_type (items, tgroup, ArdourMeter::meter_type_string(MeterIEC1NOR), MeterIEC1NOR);
	add_level_meter_item_type (items, tgroup, ArdourMeter::meter_type_string(MeterIEC2BBC), MeterIEC2BBC);
	add_level_meter_item_type (items, tgroup, ArdourMeter::meter_type_string(MeterIEC2EBU), MeterIEC2EBU);
	add_level_meter_item_type (items, tgroup, ArdourMeter::meter_type_string(MeterK20), MeterK20);
	add_level_meter_item_type (items, tgroup, ArdourMeter::meter_type_string(MeterK14), MeterK14);
	add_level_meter_item_type (items, tgroup, ArdourMeter::meter_type_string(MeterK12), MeterK12);
	add_level_meter_item_type (items, tgroup, ArdourMeter::meter_type_string(MeterVU),  MeterVU);

	int _strip_type;
	if (_route->is_master()) {
		_strip_type = 4;
	}
	else if (boost::dynamic_pointer_cast<AudioTrack>(_route) == 0
			&& boost::dynamic_pointer_cast<MidiTrack>(_route) == 0) {
		/* non-master bus */
		_strip_type = 3;
	}
	else if (boost::dynamic_pointer_cast<MidiTrack>(_route)) {
		_strip_type = 2;
	}
	else {
		_strip_type = 1;
	}

	MeterType cmt = _route->meter_type();
	const std::string cmn = ArdourMeter::meter_type_string(cmt);

	items.push_back (SeparatorElem());
	items.push_back (MenuElem (string_compose(_("Change all in Group to %1"), cmn),
				sigc::bind (SetMeterTypeMulti, -1, _route->route_group(), cmt)));
	items.push_back (MenuElem (string_compose(_("Change all to %1"), cmn),
				sigc::bind (SetMeterTypeMulti, 0, _route->route_group(), cmt)));
	items.push_back (MenuElem (string_compose(_("Change same track-type to %1"), cmn),
				sigc::bind (SetMeterTypeMulti, _strip_type, _route->route_group(), cmt)));

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
