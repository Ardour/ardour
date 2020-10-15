/*
 * Copyright (C) 2005-2006 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2005-2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2009-2010 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013-2016 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2014-2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2016-2017 Julien "_FrnchFrgg_" RIVAUD <frnchfrgg@free.fr>
 * Copyright (C) 2018 Len Ovens <len@ovenwerks.net>
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

#include <cmath>
#include <list>
#include <algorithm>

#include <sigc++/bind.h>

#include <gtkmm/messagedialog.h>

#include "pbd/convert.h"
#include "pbd/enumwriter.h"
#include "pbd/replace_all.h"
#include "pbd/unwind.h"

#include "ardour/amp.h"
#include "ardour/async_midi_port.h"
#include "ardour/audio_track.h"
#include "ardour/audioengine.h"
#include "ardour/internal_send.h"
#include "ardour/io.h"
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
#include "ardour/vca.h"
#include "ardour/vca_manager.h"

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/menu_elems.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/doi.h"

#include "widgets/tooltips.h"

#include "ardour_window.h"
#include "automation_controller.h"
#include "context_menu_helper.h"
#include "enums_convert.h"
#include "mixer_strip.h"
#include "mixer_ui.h"
#include "keyboard.h"
#include "public_editor.h"
#include "send_ui.h"
#include "io_selector.h"
#include "utils.h"
#include "gui_thread.h"
#include "route_group_menu.h"
#include "meter_patterns.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ArdourWidgets;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace std;
using namespace ArdourMeter;

MixerStrip* MixerStrip::_entered_mixer_strip;
PBD::Signal1<void,MixerStrip*> MixerStrip::CatchDeletion;

#define PX_SCALE(px) std::max((float)px, rintf((float)px * UIConfiguration::instance().get_ui_scale()))

MixerStrip::MixerStrip (Mixer_UI& mx, Session* sess, bool in_mixer)
	: SessionHandlePtr (sess)
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
	, master_volume_table (2, 2)
	, bottom_button_table (1, 3)
	, input_button (true)
	, output_button (false)
	, monitor_section_button (0)
	, midi_input_enable_button (0)
	, _plugin_insert_cnt (0)
	, _comment_button (_("Comments"))
	, trim_control (ArdourKnob::default_elements, ArdourKnob::Flags (ArdourKnob::Detent | ArdourKnob::ArcToZero))
	, _master_volume_menu (0)
	, _loudess_analysis_button (0)
	, _visibility (X_("mixer-element-visibility"))
	, _suspend_menu_callbacks (false)
	, control_slave_ui (sess)
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
	: SessionHandlePtr (sess)
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
	, master_volume_table (1, 2)
	, bottom_button_table (1, 3)
	, input_button (true)
	, output_button (false)
	, monitor_section_button (0)
	, midi_input_enable_button (0)
	, _plugin_insert_cnt (0)
	, _comment_button (_("Comments"))
	, trim_control (ArdourKnob::default_elements, ArdourKnob::Flags (ArdourKnob::Detent | ArdourKnob::ArcToZero))
	, _master_volume_menu (0)
	, _loudess_analysis_button (0)
	, _visibility (X_("mixer-element-visibility"))
	, _suspend_menu_callbacks (false)
	, control_slave_ui (sess)
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
	_width_owner = 0;

	/* the length of this string determines the width of the mixer strip when it is set to `wide' */
	longest_label = "longest label";

	string t = _("Click to toggle the width of this mixer strip.");
	if (_mixer_owned) {
		t += string_compose (_("\n%1-%2-click to toggle the width of all strips."), Keyboard::primary_modifier_name(), Keyboard::tertiary_modifier_name ());
	}

	width_button.set_icon (ArdourIcon::StripWidth);
	hide_button.set_tweaks (ArdourButton::Square);
	set_tooltip (width_button, t);

	hide_button.set_icon (ArdourIcon::HideEye);
	hide_button.set_tweaks (ArdourButton::Square);
	set_tooltip (&hide_button, _("Hide this mixer strip"));

	input_button_box.set_spacing(2);
	input_button_box.pack_start (input_button, true, true);

	bottom_button_table.attach (gpm.meter_point_button, 2, 3, 0, 1);

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

	solo_safe_led->set_text (S_("SoloLock|Lock"));
	solo_isolated_led->set_text (_("Iso"));

	solo_iso_table.set_homogeneous (true);
	solo_iso_table.set_spacings (2);
	solo_iso_table.attach (*solo_isolated_led, 0, 1, 0, 1);
	solo_iso_table.attach (*solo_safe_led, 1, 2, 0, 1);
	solo_iso_table.show ();

	rec_mon_table.set_homogeneous (true);
	rec_mon_table.set_row_spacings (2);
	rec_mon_table.set_col_spacings (2);
	if (ARDOUR::Profile->get_mixbus()) {
		rec_mon_table.resize (1, 3);
		rec_mon_table.attach (*monitor_input_button, 1, 2, 0, 1);
		rec_mon_table.attach (*monitor_disk_button, 2, 3, 0, 1);
	}
	rec_mon_table.show ();

	master_volume_table.set_homogeneous (true);
	master_volume_table.set_row_spacings (2);
	master_volume_table.set_col_spacings (2);

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

	set_tooltip (&group_button, _("Mix group"));
	group_button.set_name ("mixer strip button");

	Gtk::Requisition mpb_size = gpm.meter_point_button.size_request();
	group_button.set_size_request (mpb_size.width, mpb_size.height);

	_comment_button.set_name (X_("mixer strip button"));
	_comment_button.set_text_ellipsize (Pango::ELLIPSIZE_END);
	_comment_button.signal_clicked.connect (sigc::mem_fun (*this, &RouteUI::toggle_comment_editor));
	_comment_button.signal_size_allocate().connect (sigc::mem_fun (*this, &MixerStrip::comment_button_resized));

	trim_control.set_size_request (PX_SCALE(19), PX_SCALE(19));
	trim_control.set_tooltip_prefix (_("Trim: "));
	trim_control.set_name ("trim knob");
	trim_control.set_no_show_all (true);
	trim_control.StartGesture.connect(sigc::mem_fun(*this, &MixerStrip::trim_start_touch));
	trim_control.StopGesture.connect(sigc::mem_fun(*this, &MixerStrip::trim_end_touch));
	input_button_box.pack_start (trim_control, false, false);

	global_vpacker.set_no_show_all ();
	global_vpacker.set_border_width (1);
	global_vpacker.set_spacing (0);

	width_button.set_name ("mixer strip button");
	hide_button.set_name ("mixer strip button");

	width_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MixerStrip::width_button_pressed), false);
	hide_button.signal_clicked.connect (sigc::mem_fun(*this, &MixerStrip::hide_clicked));

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
	number_label.set_tweaks (ArdourButton::OccasionalText);

	global_vpacker.set_spacing (2);
	global_vpacker.pack_start (width_hide_box, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (name_button, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (input_button_box, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (invert_button_box, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (processor_box, true, true);
	global_vpacker.pack_start (panners, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (rec_mon_table, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (master_volume_table, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (solo_iso_table, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (mute_solo_table, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (gpm, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (control_slave_ui, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (bottom_button_table, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (output_button, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (_comment_button, Gtk::PACK_SHRINK);

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
		scrollbar_height += 3; // track_display_frame border/shadow
	}
	spacer.set_size_request (-1, scrollbar_height);
	global_vpacker.pack_end (spacer, false, false);
#endif

	global_frame.add (global_vpacker);
	global_frame.set_shadow_type (Gtk::SHADOW_IN);
	global_frame.set_name ("BaseFrame");

	add (global_frame);

	/* force setting of visible selected status */

	_selected = true;
	set_selected (false);

	_packed = false;
	_embedded = false;

	number_label.signal_button_press_event().connect (sigc::mem_fun(*this, &MixerStrip::number_button_button_press), false);

	name_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MixerStrip::name_button_button_press), false);

	group_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MixerStrip::select_route_group), false);

	_width = (Width) -1;

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

	/* Add the widgets under visibility control to the VisibilityGroup; the names used here
	   must be the same as those used in RCOptionEditor so that the configuration changes
	   are recognised when they occur.
	*/
	_visibility.add (&input_button_box, X_("Input"), _("Input"), false);
	_visibility.add (&invert_button_box, X_("PhaseInvert"), _("Phase Invert"), false);
	_visibility.add (&rec_mon_table, X_("RecMon"), _("Record & Monitor"), false, boost::bind (&MixerStrip::override_rec_mon_visibility, this));
	_visibility.add (&solo_iso_table, X_("SoloIsoLock"), _("Solo Iso / Lock"), false);
	_visibility.add (&output_button, X_("Output"), _("Output"), false);
	_visibility.add (&_comment_button, X_("Comments"), _("Comments"), false);
	_visibility.add (&control_slave_ui, X_("VCA"), _("VCA Assigns"), false);

	parameter_changed (X_("mixer-element-visibility"));
	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &MixerStrip::parameter_changed));
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

	if (this ==_entered_mixer_strip) {
		_entered_mixer_strip = NULL;
	}

	delete _master_volume_menu;
}

void
MixerStrip::vca_assign (boost::shared_ptr<ARDOUR::VCA> vca)
{
	boost::shared_ptr<Slavable> sl = boost::dynamic_pointer_cast<Slavable> ( route() );
	if (sl)
		sl->assign(vca);
}

void
MixerStrip::vca_unassign (boost::shared_ptr<ARDOUR::VCA> vca)
{
	boost::shared_ptr<Slavable> sl = boost::dynamic_pointer_cast<Slavable> ( route() );
	if (sl)
		sl->unassign(vca);
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

string
MixerStrip::name() const
{
	if (_route) {
		return _route->name();
	}
	return string();
}

void
MixerStrip::update_trim_control ()
{
	if (route()->trim() && route()->trim()->active() &&
	    route()->n_inputs().n_audio() > 0) {
		trim_control.show ();
		trim_control.set_controllable (route()->trim()->gain_control());
	} else {
		trim_control.hide ();
		boost::shared_ptr<Controllable> none;
		trim_control.set_controllable (none);
	}
}

void
MixerStrip::trim_start_touch ()
{
	assert (_route && _session);
	if (route()->trim() && route()->trim()->active() && route()->n_inputs().n_audio() > 0) {
		route()->trim()->gain_control ()->start_touch (timepos_t (_session->transport_sample()));
	}
}

void
MixerStrip::trim_end_touch ()
{
	assert (_route && _session);
	if (route()->trim() && route()->trim()->active() && route()->n_inputs().n_audio() > 0) {
		route()->trim()->gain_control ()->stop_touch (timepos_t (_session->transport_sample()));
	}
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

	control_slave_ui.set_stripable (boost::dynamic_pointer_cast<Stripable> (rt));

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

	mute_solo_table.attach (gpm.gain_display,0,1,1,2, EXPAND|FILL, EXPAND);
	mute_solo_table.attach (gpm.peak_display,1,2,1,2, EXPAND|FILL, EXPAND);

	if (solo_button->get_parent()) {
		mute_solo_table.remove (*solo_button);
	}

	if (mute_button->get_parent()) {
		mute_solo_table.remove (*mute_button);
	}

	if (route()->is_master()) {
		solo_button->hide ();
		mute_button->show ();
		mute_solo_table.attach (*mute_button, 0, 2, 0, 1);
		if (Config->get_use_master_volume ()) {
			master_volume_table.show ();
		}

		if (monitor_section_button == 0 && _mixer_owned) {
			Glib::RefPtr<Action> act = ActionManager::get_action ("Mixer", "ToggleMonitorSection");
			_session->MonitorChanged.connect (route_connections, invalidator (*this), boost::bind (&MixerStrip::monitor_changed, this), gui_context());
			_session->MonitorBusAddedOrRemoved.connect (route_connections, invalidator (*this), boost::bind (&MixerStrip::monitor_section_added_or_removed, this), gui_context());

			monitor_section_button = manage (new ArdourButton);
			monitor_changed ();
			monitor_section_button->set_related_action (act);
			set_tooltip (monitor_section_button, _("Show/Hide Monitoring Section"));
			monitor_section_button->unset_flags (Gtk::CAN_FOCUS);
			monitor_section_added_or_removed ();
		}
	} else {
		bottom_button_table.attach (group_button, 1, 2, 0, 1);
		mute_solo_table.attach (*mute_button, 0, 1, 0, 1);
		mute_solo_table.attach (*solo_button, 1, 2, 0, 1);
		mute_button->show ();
		solo_button->show ();
		master_volume_table.hide ();
	}

	if (route()->is_master() && _volume_controller == 0) {
		assert (_loudess_analysis_button == 0);
		assert (route()->volume_control());
		boost::shared_ptr<AutomationControl> ac = route()->volume_control ();

		_volume_controller = AutomationController::create (ac->parameter (), ParameterDescriptor (ac->parameter ()), ac, false);
		_volume_controller->set_name (X_("ProcessorControlSlider"));
		_volume_controller->set_size_request (PX_SCALE(19), -1);
		_volume_controller->disable_vertical_scroll ();

		_loudess_analysis_button = manage (new ArdourButton (S_("Loudness|LAN")));
		_loudess_analysis_button->signal_clicked.connect (mem_fun (*this, &MixerStrip::loudess_analysis_button_clicked));
		_volume_controller->signal_button_press_event().connect (mem_fun (*this, &MixerStrip::volume_controller_button_pressed), false);

		set_tooltip (*_volume_controller, _("Master output volume"));
		set_tooltip (_loudess_analysis_button, _("Measure loudness of the session, normalize master output volume"));

		master_volume_table.attach (*_loudess_analysis_button, 0, 2, 0, 1);
		master_volume_table.attach (*_volume_controller, 0, 2, 1, 2);

		_loudess_analysis_button->show ();
		_volume_controller->show ();
#ifdef MIXBUS
	} else if (!route()->is_master()) {
		master_volume_table.hide ();
#endif
	}

	hide_master_spacer (false);

	if (is_track()) {
		monitor_input_button->show ();
		monitor_disk_button->show ();
	} else {
		monitor_input_button->hide();
		monitor_disk_button->hide ();
	}

	update_trim_control();

	if (is_midi_track()) {
		if (midi_input_enable_button == 0) {
			midi_input_enable_button = manage (new ArdourButton);
			midi_input_enable_button->set_name ("midi input button");
			midi_input_enable_button->set_elements ((ArdourButton::Element)(ArdourButton::Edge|ArdourButton::Body|ArdourButton::VectorIcon));
			midi_input_enable_button->set_icon (ArdourIcon::DinMidi);
			midi_input_enable_button->signal_button_press_event().connect (sigc::mem_fun (*this, &MixerStrip::input_active_button_press), false);
			midi_input_enable_button->signal_button_release_event().connect (sigc::mem_fun (*this, &MixerStrip::input_active_button_release), false);
			set_tooltip (midi_input_enable_button, _("Enable/Disable MIDI input"));
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
		rec_enable_button->show();

		if (ARDOUR::Profile->get_mixbus()) {
			rec_mon_table.attach (*monitor_input_button, 1, 2, 0, 1);
			rec_mon_table.attach (*monitor_disk_button, 2, 3, 0, 1);
		} else {
			rec_mon_table.attach (*monitor_input_button, 1, 2, 0, 1);
			rec_mon_table.attach (*monitor_disk_button, 1, 2, 1, 2);
		}

	} else {

		/* non-master bus */

		if (!_route->is_master()) {
			if (ARDOUR::Profile->get_mixbus()) {
				rec_mon_table.attach (*show_sends_button, 0, 3, 0, 2);
			} else {
				rec_mon_table.attach (*show_sends_button, 0, 2, 0, 2);
			}

			if (_mixer_owned) {
				show_sends_button->show();
			} else {
				show_sends_button->hide();
			}
		}
	}

	input_button.set_route (route (), this);
	output_button.set_route (route (), this);

	gpm.meter_point_button.set_text (meter_point_string (_route->meter_point()));

	delete route_ops_menu;
	route_ops_menu = 0;

	_route->meter_change.connect (route_connections, invalidator (*this), bind (&MixerStrip::meter_changed, this), gui_context());
	_route->input()->changed.connect (*this, invalidator (*this), boost::bind (&MixerStrip::update_input_display, this), gui_context());
	_route->output()->changed.connect (*this, invalidator (*this), boost::bind (&MixerStrip::update_output_display, this), gui_context());
	_route->route_group_changed.connect (route_connections, invalidator (*this), boost::bind (&MixerStrip::route_group_changed, this), gui_context());

	_route->io_changed.connect (route_connections, invalidator (*this), boost::bind (&MixerStrip::io_changed_proxy, this), gui_context ());

	if (_route->panner_shell()) {
		update_panner_choices();
		_route->panner_shell()->Changed.connect (route_connections, invalidator (*this), boost::bind (&MixerStrip::connect_to_pan, this), gui_context());
	}

	_route->comment_changed.connect (route_connections, invalidator (*this), boost::bind (&MixerStrip::setup_comment_button, this), gui_context());

	set_stuff_from_route ();

	/* now force an update of all the various elements */

	name_changed ();
	comment_changed ();
	route_group_changed ();
	update_track_number_visibility ();

	connect_to_pan ();
	panners.setup_pan ();

	if (has_audio_outputs ()) {
		panners.show_all ();
	} else {
		panners.hide_all ();
	}

	route_color_changed ();
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
	gpm.meter_point_button.show();
	input_button_box.show_all();
	output_button.show();
	name_button.show();
	_comment_button.show();
	group_button.show();
	gpm.gain_automation_state_button.show();

	parameter_changed ("mixer-element-visibility");
	map_frozen();

	show ();
	update_sensitivity ();
}

void
MixerStrip::set_stuff_from_route ()
{
	/* if width is not set, it will be set by the MixerUI or editor */

	Width width;
	if (get_gui_property ("strip-width", width)) {
		set_width_enum (width, this);
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
		set_gui_property ("strip-width", _width);
	}

	set_button_names ();

	const float scale = std::max(1.f, UIConfiguration::instance().get_ui_scale());

	gpm.gain_automation_state_button.set_text (GainMeterBase::short_astate_string (gain_automation->automation_state()));

	if (_route->panner()) {
		((Gtk::Label*)panners.pan_automation_state_button.get_child())->set_text (GainMeterBase::short_astate_string (_route->pannable()->automation_state()));
	}

	switch (w) {
	case Wide:

		if (show_sends_button)  {
			show_sends_button->set_text (_("Show Sends"));
		}

		{
			// panners expect an even number of horiz. pixels
			int width = rintf (max (110.f * scale, gpm.get_gm_width() + 10.f * scale)) + 1;
			width &= ~1;
			set_size_request (width, -1);
		}
		break;

	case Narrow:

		if (show_sends_button) {
			show_sends_button->set_text (_("Show"));
		}

		gain_meter().setup_meters (); // recalc meter width

		{
			// panners expect an even number of horiz. pixels
			int width = rintf (max (60.f * scale, gpm.get_gm_width() + 10.f * scale)) + 1;
			width &= ~1;
			set_size_request (width, -1);
		}
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
	set_gui_property ("visible", _packed);
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


void
MixerStrip::update_input_display ()
{
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
MixerStrip::io_changed_proxy ()
{
	Glib::signal_idle().connect_once (sigc::mem_fun (*this, &MixerStrip::update_panner_choices));
	Glib::signal_idle().connect_once (sigc::mem_fun (*this, &MixerStrip::update_trim_control));
}

void
MixerStrip::setup_comment_button ()
{
	std::string comment = _route->comment();

	set_tooltip (_comment_button, comment.empty() ? _("Click to add/edit comments") : _route->comment());

	if (comment.empty ()) {
		_comment_button.set_name ("generic button");
		_comment_button.set_text (_width  == Wide ? _("Comments") : _("Cmt"));
		return;
	}

	_comment_button.set_name ("comment button");

	string::size_type pos = comment.find_first_of (" \t\n");
	if (pos != string::npos) {
		comment = comment.substr (0, pos);
	}
	if (comment.empty()) {
		_comment_button.set_text (_width  == Wide ? _("Comments") : _("Cmt"));
	} else {
		_comment_button.set_text (comment);
	}
}

bool
MixerStrip::select_route_group (GdkEventButton *ev)
{
	using namespace Menu_Helpers;

	if (ev->button == 1) {

		if (group_menu == 0) {

			PropertyList* plist = new PropertyList();

			plist->add (Properties::group_gain, true);
			plist->add (Properties::group_mute, true);
			plist->add (Properties::group_solo, true);

			group_menu = new RouteGroupMenu (_session, plist);
		}

		WeakRouteList r;
		r.push_back (route ());
		group_menu->build (r);

		RouteGroup *rg = _route->route_group();

		Gtkmm2ext::anchored_menu_popup(group_menu->menu(), &group_button,
		                               rg ? rg->name() : _("No Group"),
		                               1, ev->time);
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
	using namespace ARDOUR_UI_UTILS;
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
MixerStrip::help_count_plugins (boost::weak_ptr<Processor> p)
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
void
MixerStrip::build_route_ops_menu ()
{
	using namespace Menu_Helpers;
	route_ops_menu = new Menu;
	route_ops_menu->set_name ("ArdourContextMenu");

	bool active = _route->active () || ARDOUR::Profile->get_mixbus();

	MenuList& items = route_ops_menu->items();

	if (active) {

		items.push_back (MenuElem (_("Color..."), sigc::mem_fun (*this, &RouteUI::choose_color)));

		items.push_back (MenuElem (_("Comments..."), sigc::mem_fun (*this, &RouteUI::open_comment_editor)));

		items.push_back (MenuElem (_("Inputs..."), sigc::mem_fun (*this, &RouteUI::edit_input_configuration)));

		items.push_back (MenuElem (_("Outputs..."), sigc::mem_fun (*this, &RouteUI::edit_output_configuration)));

		if (!Profile->get_mixbus()) {
			items.push_back (SeparatorElem());
		}

		if (!_route->is_master()
#ifdef MIXBUS
				&& !_route->mixbus()
#endif
		   ) {
			if (Profile->get_mixbus()) {
				items.push_back (SeparatorElem());
			}
			items.push_back (MenuElem (_("Save As Template..."), sigc::mem_fun(*this, &RouteUI::save_as_template)));
		}

		if (!Profile->get_mixbus()) {
			items.push_back (MenuElem (_("Rename..."), sigc::mem_fun(*this, &RouteUI::route_rename)));
			/* do not allow rename if the track is record-enabled */
			items.back().set_sensitive (!is_track() || !track()->rec_enable_control()->get_value());
		}

		items.push_back (SeparatorElem());
	}

	if ((!_route->is_master() || !active)
#ifdef MIXBUS
			&& !_route->mixbus()
#endif
	   )
	{
		items.push_back (CheckMenuElem (_("Active")));
		Gtk::CheckMenuItem* i = dynamic_cast<Gtk::CheckMenuItem *> (&items.back());
		i->set_active (active);
		i->set_sensitive (!_session->transport_rolling());
		i->signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &RouteUI::set_route_active), !_route->active(), false));
		items.push_back (SeparatorElem());
	}

	if (active && !Profile->get_mixbus ()) {
		items.push_back (CheckMenuElem (_("Strict I/O")));
		Gtk::CheckMenuItem* i = dynamic_cast<Gtk::CheckMenuItem *> (&items.back());
		i->set_active (_route->strict_io());
		i->signal_activate().connect (sigc::hide_return (sigc::bind (sigc::mem_fun (*_route, &Route::set_strict_io), !_route->strict_io())));
		items.push_back (SeparatorElem());
	}

	if (active && is_track()) {
		Gtk::Menu* dio_menu = new Menu;
		MenuList& dio_items = dio_menu->items();
		dio_items.push_back (MenuElem (_("Record Pre-Fader"), sigc::bind (sigc::mem_fun (*this, &RouteUI::set_disk_io_point), DiskIOPreFader)));
		dio_items.push_back (MenuElem (_("Record Post-Fader"), sigc::bind (sigc::mem_fun (*this, &RouteUI::set_disk_io_point), DiskIOPostFader)));
		dio_items.push_back (MenuElem (_("Custom Record+Playback Positions"), sigc::bind (sigc::mem_fun (*this, &RouteUI::set_disk_io_point), DiskIOCustom)));

		items.push_back (MenuElem (_("Disk I/O..."), *dio_menu));
		items.push_back (SeparatorElem());
	}

	_plugin_insert_cnt = 0;
	_route->foreach_processor (sigc::mem_fun (*this, &MixerStrip::help_count_plugins));
	if (active && _plugin_insert_cnt > 0) {
		items.push_back (MenuElem (_("Pin Connections..."), sigc::mem_fun (*this, &RouteUI::manage_pins)));
	}

	if (active && (boost::dynamic_pointer_cast<MidiTrack>(_route) || _route->the_instrument ())) {
		items.push_back (MenuElem (_("Patch Selector..."),
					sigc::mem_fun(*this, &RouteUI::select_midi_patch)));
	}

	if (active && _route->the_instrument () && _route->the_instrument ()->output_streams().n_audio() > 2) {
		// TODO ..->n_audio() > 1 && separate_output_groups) hard to check here every time.
		items.push_back (MenuElem (_("Fan out to Busses"), sigc::bind (sigc::mem_fun (*this, &RouteUI::fan_out), true, true)));
		items.push_back (MenuElem (_("Fan out to Tracks"), sigc::bind (sigc::mem_fun (*this, &RouteUI::fan_out), false, true)));
		items.push_back (SeparatorElem());
	}

	items.push_back (CheckMenuElem (_("Protect Against Denormals"), sigc::mem_fun (*this, &RouteUI::toggle_denormal_protection)));
	denormal_menu_item = dynamic_cast<Gtk::CheckMenuItem *> (&items.back());
	denormal_menu_item->set_active (_route->denormal_protection());

	/* note that this relies on selection being shared across editor and
	 * mixer (or global to the backend, in the future), which is the only
	 * sane thing for users anyway.
	 */
	StripableTimeAxisView* stav = PublicEditor::instance().get_stripable_time_axis_by_id (_route->id());
	if (active && stav) {
		Selection& selection (PublicEditor::instance().get_selection());
		if (!selection.selected (stav)) {
			selection.set (stav);
		}

		if (!_route->is_master()) {
			items.push_back (SeparatorElem());
			items.push_back (MenuElem (_("Duplicate..."), sigc::mem_fun (*this, &RouteUI::duplicate_selected_routes)));
			items.push_back (SeparatorElem());
			items.push_back (MenuElem (_("Remove"), sigc::mem_fun(PublicEditor::instance(), &PublicEditor::remove_tracks)));
		}
	}
}

gboolean
MixerStrip::name_button_button_press (GdkEventButton* ev)
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
MixerStrip::number_button_button_press (GdkEventButton* ev)
{
	if (  ev->button == 3 ) {
		list_route_operations ();

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

	if (selected()) {
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
MixerStrip::route_property_changed (const PropertyChange& what_changed)
{
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

	set_tooltip (name_button, Gtkmm2ext::markup_escape_text(_route->name()));

	if (_session->config.get_track_name_number()) {
		const int64_t track_number = _route->track_number ();
		if (track_number == 0) {
			number_label.set_text ("-");
		} else {
			number_label.set_text (PBD::to_string (abs(_route->track_number ())));
		}
	} else {
		number_label.set_text ("");
	}
}

void
MixerStrip::name_button_resized (Gtk::Allocation& alloc)
{
	name_button.set_layout_ellipsize_width (alloc.get_width() * PANGO_SCALE);
}

void
MixerStrip::comment_button_resized (Gtk::Allocation& alloc)
{
	_comment_button.set_layout_ellipsize_width (alloc.get_width() * PANGO_SCALE);
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
MixerStrip::loudess_analysis_button_clicked ()
{
	PublicEditor::instance().loudness_assistant (false);
}

bool
MixerStrip::volume_controller_button_pressed (GdkEventButton* ev)
{
	using namespace Menu_Helpers;
	if (Keyboard::is_context_menu_event (ev)) {
		delete _master_volume_menu;
		_master_volume_menu = new Menu;
		_master_volume_menu->set_name ("ArdourContextMenu");
		MenuList& items = _master_volume_menu->items();
		items.clear ();
		items.push_back (CheckMenuElem (_("Custom LAN Amp Position")));
		Gtk::CheckMenuItem* cmi = static_cast<Gtk::CheckMenuItem*> (&items.back());
		cmi->set_active (!_route->volume_applies_to_output ());
		cmi->signal_toggled().connect (sigc::bind (sigc::mem_fun (_route.get(), &Route::set_volume_applies_to_output), !_route->volume_applies_to_output ()));

		_master_volume_menu->popup (ev->button, ev->time);
		return true;
	}
	return false;
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

	bool en   = _route->active () || ARDOUR::Profile->get_mixbus();

	if (at) {
		switch (at->freeze_state()) {
		case AudioTrack::Frozen:
			processor_box.set_sensitive (false);
			hide_redirect_editors ();
			break;
		default:
			processor_box.set_sensitive (en);
			break;
		}
	} else {
		processor_box.set_sensitive (en);
	}
	RouteUI::map_frozen ();
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
			return S_("Meter|In");
			break;

		case MeterPreFader:
			return S_("Meter|Pr");
			break;

		case MeterPostFader:
			return S_("Meter|Po");
			break;

		case MeterOutput:
			return S_("Meter|O");
			break;

		case MeterCustom:
		default:
			return S_("Meter|C");
			break;
		}
		break;
	}

	return string();
}

/** Called when the monitor-section state */
void
MixerStrip::monitor_changed ()
{
	assert (monitor_section_button);
	if (_session->monitor_active()) {
		monitor_section_button->set_name ("master monitor section button active");
	} else {
		monitor_section_button->set_name ("master monitor section button normal");
	}
}

void
MixerStrip::monitor_section_added_or_removed ()
{
	assert (monitor_section_button);
	if (mute_button->get_parent()) {
		mute_button->get_parent()->remove(*mute_button);
	}
	if (monitor_section_button->get_parent()) {
		monitor_section_button->get_parent()->remove(*monitor_section_button);
	}
	if (_session && _session->monitor_out ()) {
		mute_solo_table.attach (*mute_button, 0, 1, 0, 1);
		mute_solo_table.attach (*monitor_section_button, 1, 2, 0, 1);
		mute_button->show();
		monitor_section_button->show();
	} else {
		mute_solo_table.attach (*mute_button, 0, 2, 0, 1);
		mute_button->show();
	}
}

/** Called when the metering point has changed */
void
MixerStrip::meter_changed ()
{
	gpm.meter_point_button.set_text (meter_point_string (_route->meter_point()));
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
	RouteUI::check_rec_enable_sensitivity ();
}

void
MixerStrip::set_current_delivery (boost::shared_ptr<Delivery> d)
{
	_current_delivery = d;
	DeliveryChanged (_current_delivery);
	update_sensitivity ();
}

void
MixerStrip::show_send (boost::shared_ptr<Send> send)
{
	assert (send != 0);

	drop_send ();

	set_current_delivery (send);

	send->meter()->set_meter_type (_route->meter_type ());
	send->set_metering (true);
	_current_delivery->DropReferences.connect (send_gone_connection, invalidator (*this), boost::bind (&MixerStrip::revert_to_default_display, this), gui_context());

	gain_meter().set_controls (_route, send->meter(), send->amp(), send->gain_control());
	gain_meter().setup_meters ();

	uint32_t const in = _current_delivery->pans_required();
	uint32_t const out = _current_delivery->pan_outs();

	panner_ui().set_panner (_current_delivery->panner_shell(), _current_delivery->panner());
	panner_ui().set_available_panners(PannerManager::instance().PannerManager::get_available_panners(in, out));
	panner_ui().setup_pan ();
	panner_ui().set_send_drawing_mode (true);
	panner_ui().show_all ();

	reset_strip_style ();
}

void
MixerStrip::revert_to_default_display ()
{
	drop_send ();

	set_current_delivery (_route->main_outs ());

	gain_meter().set_controls (_route, _route->shared_peak_meter(), _route->amp(), _route->gain_control());
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
		if (monitor_section_button) {
			monitor_section_button->set_text (_("Mon"));
		}

		if ((_route && _route->solo_safe_control()->solo_safe()) || !solo_button->get_sensitive()) {
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
		solo_safe_led->set_text (S_("SoloLock|Lock"));
		break;

	default:
		if (is_master ()) {
			/* master bus has no solo button, "Mute" fits in narrow mode */
			mute_button->set_text (_("Mute"));
		} else {
			mute_button->set_text (S_("Mute|M"));
		}
		monitor_input_button->set_text (S_("MonitorInput|I"));
		monitor_disk_button->set_text (S_("MonitorDisk|D"));
		if (monitor_section_button) {
			monitor_section_button->set_text (_("Mon"));
		}

		if ((_route && _route->solo_safe_control()->solo_safe()) || !solo_button->get_sensitive()) {
			solo_button->set_visual_state (Gtkmm2ext::VisualState (solo_button->visual_state() | Gtkmm2ext::Insensitive));
		} else {
			solo_button->set_visual_state (Gtkmm2ext::VisualState (solo_button->visual_state() & ~Gtkmm2ext::Insensitive));
		}
		if (!Config->get_solo_control_is_listen_control()) {
			solo_button->set_text (S_("Solo|S"));
		} else {
			switch (Config->get_listen_position()) {
			case AfterFaderListen:
				solo_button->set_text (S_("AfterFader|A"));
				break;
			case PreFaderListen:
				solo_button->set_text (S_("Prefader|P"));
				break;
			}
		}

		solo_isolated_led->set_text (S_("SoloIso|I"));
		solo_safe_led->set_text (S_("SoloLock|L"));
		break;
	}

	if (_route) {
		gpm.meter_point_button.set_text (meter_point_string (_route->meter_point()));
	} else {
		gpm.meter_point_button.set_text ("");
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
		_visibility.set_state (UIConfiguration::instance().get_mixer_strip_visibility ());
	} else if (p == "track-name-number") {
		name_changed ();
		update_track_number_visibility();
	} else if (p == "use-master-volume") {
		if (route () && route()->is_master()) {
			if (Config->get_use_master_volume ()) {
				master_volume_table.show ();
			} else {
				master_volume_table.hide ();
			}
		}
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
	if (is_master ()) {
		return boost::optional<bool> (false);
	}

	return boost::optional<bool> ();
}

boost::optional<bool>
MixerStrip::override_rec_mon_visibility () const
{
	if (is_master ()) {
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
	RouteUI::route_active_changed ();
	reset_strip_style ();
	update_sensitivity ();
}

void
MixerStrip::update_sensitivity ()
{
	bool en   = _route->active () || ARDOUR::Profile->get_mixbus();
	bool send = _current_delivery && boost::dynamic_pointer_cast<Send>(_current_delivery) != 0;
	bool aux  = _current_delivery && boost::dynamic_pointer_cast<InternalSend>(_current_delivery) != 0;

	if (route()->is_master()) {
		solo_iso_table.set_sensitive (false);
		control_slave_ui.set_sensitive (false);
	} else {
		solo_iso_table.set_sensitive (en && !send);
		control_slave_ui.set_sensitive (en && !send);
	}

	input_button.set_sensitive (en && !send);
	group_button.set_sensitive (en && !send);
	set_invert_sensitive (en && !send);
	gpm.meter_point_button.set_sensitive (en && !send);
	mute_button->set_sensitive (en && !send);
	solo_button->set_sensitive (en && !send);
	solo_isolated_led->set_sensitive (en && !send);
	solo_safe_led->set_sensitive (en && !send);
	monitor_input_button->set_sensitive (en && !send);
	monitor_disk_button->set_sensitive (en && !send);
	_comment_button.set_sensitive (en && !send);
	trim_control.set_sensitive (en && !send);
	control_slave_ui.set_sensitive (en && !send);

	if (midi_input_enable_button) {
		midi_input_enable_button->set_sensitive (en && !send);
	}

	output_button.set_sensitive (en && !aux);

	map_frozen ();
	set_button_names (); // update solo button visual state
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

	Gtk::Menu* m = ARDOUR_UI_UTILS::shared_popup_menu ();
	MenuList& items = m->items ();

	RadioMenuItem::Group group;

	PBD::Unwinder<bool> uw (_suspend_menu_callbacks, true);
	add_level_meter_item_point (items, group, _("Input"), MeterInput);
	add_level_meter_item_point (items, group, _("Pre Fader"), MeterPreFader);
	add_level_meter_item_point (items, group, _("Post Fader"), MeterPostFader);
	add_level_meter_item_point (items, group, _("Output"), MeterOutput);
	add_level_meter_item_point (items, group, _("Custom"), MeterCustom);

	if (gpm.meter_channels().n_audio() == 0) {
		m->popup (ev->button, ev->time);
		return;
	}

	RadioMenuItem::Group tgroup;
	items.push_back (SeparatorElem());

	add_level_meter_item_type (items, tgroup, ArdourMeter::meter_type_string(MeterPeak), MeterPeak);
	add_level_meter_item_type (items, tgroup, ArdourMeter::meter_type_string(MeterPeak0dB), MeterPeak0dB);
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
	_route->set_meter_type (t);
}

void
MixerStrip::update_track_number_visibility ()
{
	DisplaySuspender ds;
	bool show_label = _session->config.get_track_name_number();

	if (_route && _route->is_master()) {
		show_label = false;
	}

	if (show_label) {
		number_label.show ();
		// see ArdourButton::on_size_request(), we should probably use a global size-group here instead.
		// except the width of the number label is subtracted from the name-hbox, so we
		// need to explictly calculate it anyway until the name-label & entry become ArdourWidgets.
		int tnw = (2 + std::max(2u, _session->track_number_decimals())) * number_label.char_pixel_width();
		if (tnw & 1) --tnw;
		number_label.set_size_request(tnw, -1);
		number_label.show ();
	} else {
		number_label.hide ();
	}
}

Gdk::Color
MixerStrip::color () const
{
	return route_color ();
}

bool
MixerStrip::marked_for_display () const
{
	return !_route->presentation_info().hidden();
}

bool
MixerStrip::set_marked_for_display (bool yn)
{
	return RouteUI::mark_hidden (!yn);
}

void
MixerStrip::hide_master_spacer (bool yn)
{
	if (_mixer_owned && route()->is_master() && !yn) {
		spacer.show();
	} else {
		spacer.hide();
	}
}
