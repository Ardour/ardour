/*
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2005-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2005-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2006-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2006 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2006 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2018 Ben Loftis <ben@harrisonconsoles.com>
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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <cerrno>
#include <iostream>
#include <cmath>

#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>

#include <sigc++/bind.h>
#include <gtkmm/settings.h>

#include "canvas/canvas.h"

#include "pbd/error.h"
#include "pbd/basename.h"
#include "pbd/fastlog.h"

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/window_title.h"

#include "ardour/audioengine.h"
#include "ardour/profile.h"
#include "ardour/session.h"
#include "ardour/types.h"

#include "ardour_ui.h"
#include "keyboard.h"
#include "public_editor.h"
#include "audio_clock.h"
#include "actions.h"
#include "main_clock.h"
#include "mixer_ui.h"
#include "recorder_ui.h"
#include "application_bar.h" //TODO: move to each window
#include "trigger_page.h"
#include "utils.h"
#include "time_info_box.h"
#include "midi_tracer.h"
#include "global_port_matrix.h"
#include "location_ui.h"
#include "rc_option_editor.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace ArdourWidgets;
using namespace Gtk;
using namespace Glib;
using namespace ARDOUR_UI_UTILS;
using namespace Menu_Helpers;

void
ARDOUR_UI::setup_tooltips ()
{
	ArdourCanvas::Canvas::set_tooltip_timeout (Gtk::Settings::get_default()->property_gtk_tooltip_timeout ());

	parameter_changed("click-gain");
	set_tip (solo_alert_button, _("When active, something is soloed.\nClick to de-solo everything"));
	set_tip (auditioning_alert_button, _("When active, auditioning is taking place.\nClick to stop the audition"));
	set_tip (feedback_alert_button, _("When lit, there is a ports connection issue, leading to feedback loop or ambiguous alignment.\nThis is caused by connecting an output back to some input (feedback), or by multiple connections from a source to the same output via different paths (ambiguous latency, record alignment)."));
	set_tip (primary_clock, _("<b>Primary Clock</b> right-click to set display mode. Click to edit, click+drag a digit or mouse-over+scroll wheel to modify.\nText edits: right-to-left overwrite <tt>Esc</tt>: cancel; <tt>Enter</tt>: confirm; postfix the edit with '+' or '-' to enter delta times.\n"));
	set_tip (secondary_clock, _("<b>Secondary Clock</b> right-click to set display mode. Click to edit, click+drag a digit or mouse-over+scroll wheel to modify.\nText edits: right-to-left overwrite <tt>Esc</tt>: cancel; <tt>Enter</tt>: confirm; postfix the edit with '+' or '-' to enter delta times.\n"));
	set_tip (editor_meter_peak_display, _("Reset All Peak Meters"));
	set_tip (error_alert_button, _("Show Error Log and acknowledge warnings"));
	set_tip (_cue_rec_enable, _("<b>When enabled</b>, triggering Cues will result in Cue Markers added to the timeline"));
	set_tip (_cue_play_enable, _("<b>When enabled</b>, Cue Markers will trigger the associated Cue when passed on the timeline"));

	synchronize_sync_source_and_video_pullup ();

	editor->setup_tooltips ();
}

bool
ARDOUR_UI::status_bar_button_press (GdkEventButton* ev)
{
	bool handled = false;

	switch (ev->button) {
	case 1:
		status_bar_label.set_text ("");
		handled = true;
		break;
	default:
		break;
	}

	return handled;
}

void
ARDOUR_UI::display_message (const char* prefix, gint prefix_len, RefPtr<TextBuffer::Tag> ptag, RefPtr<TextBuffer::Tag> mtag, const char* msg)
{
	UI::display_message (prefix, prefix_len, ptag, mtag, msg);

	ArdourLogLevel ll = LogLevelNone;

	if (strcmp (prefix, _("[ERROR]: ")) == 0) {
		ll = LogLevelError;
	} else if (strcmp (prefix, _("[WARNING]: ")) == 0) {
		ll = LogLevelWarning;
	} else if (strcmp (prefix, _("[INFO]: ")) == 0) {
		ll = LogLevelInfo;
	}

	_log_not_acknowledged = std::max(_log_not_acknowledged, ll);
}

XMLNode*
ARDOUR_UI::tearoff_settings (const char* name) const
{
	XMLNode* ui_node = Config->extra_xml(X_("UI"));

	if (ui_node) {
		XMLNode* tearoff_node = ui_node->child (X_("Tearoffs"));
		if (tearoff_node) {
			XMLNode* mnode = tearoff_node->child (name);
			return mnode;
		}
	}

	return 0;
}

#define PX_SCALE(px) std::max((float)px, rintf((float)px * UIConfiguration::instance().get_ui_scale()))

static
bool drag_failed (const Glib::RefPtr<Gdk::DragContext>& context, DragResult result, Tabbable* tab)
{
	if (result == Gtk::DRAG_RESULT_NO_TARGET) {
		tab->detach ();
		return true;
	}
	return false;
}

void
ARDOUR_UI::cue_rec_state_clicked ()
{
	TriggerBox::set_cue_recording(!TriggerBox::cue_recording());
}

void
ARDOUR_UI::cue_ffwd_state_clicked ()
{
	if (editor) {
		editor->toggle_cue_behavior ();
	}
}

void
ARDOUR_UI::cue_rec_state_changed ()
{
	_cue_rec_enable.set_active_state( TriggerBox::cue_recording() ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	//Config->get_cue_behavior()
}

void
ARDOUR_UI::update_clock_visibility ()
{
	if (ARDOUR::Profile->get_small_screen()) {
		return;
	}
	if (UIConfiguration::instance().get_show_secondary_clock ()) {
		secondary_clock->show();
		secondary_clock->left_btn()->show();
		secondary_clock->right_btn()->show();
	} else {
		secondary_clock->hide();
		secondary_clock->left_btn()->hide();
		secondary_clock->right_btn()->hide();
	}
}

void
ARDOUR_UI::setup_transport ()
{
	RefPtr<Action> act;
	/* setup actions */

	/* CANNOT sigc::bind these to clicked or toggled, must use pressed or released */
	act = ActionManager::get_action (X_("Main"), X_("cancel-solo"));
	solo_alert_button.set_related_action (act);
	auditioning_alert_button.signal_clicked.connect (sigc::mem_fun(*this,&ARDOUR_UI::audition_alert_clicked));
	error_alert_button.signal_button_release_event().connect (sigc::mem_fun(*this,&ARDOUR_UI::error_alert_press), false);
	act = ActionManager::get_action (X_("Editor"), X_("toggle-log-window"));
	error_alert_button.set_related_action(act);
	error_alert_button.set_fallthrough_to_parent(true);

	editor_visibility_button.set_related_action (ActionManager::get_action (X_("Common"), X_("change-editor-visibility")));
	mixer_visibility_button.set_related_action (ActionManager::get_action (X_("Common"), X_("change-mixer-visibility")));
	prefs_visibility_button.set_related_action (ActionManager::get_action (X_("Common"), X_("change-preferences-visibility")));
	recorder_visibility_button.set_related_action (ActionManager::get_action (X_("Common"), X_("change-recorder-visibility")));
	trigger_page_visibility_button.set_related_action (ActionManager::get_action (X_("Common"), X_("change-trigger-visibility")));

	act = ActionManager::get_action (X_("Monitor Section"), X_("monitor-dim-all"));
	monitor_dim_button.set_related_action (act);
	act = ActionManager::get_action (X_("Monitor Section"), X_("monitor-mono"));
	monitor_mono_button.set_related_action (act);
	act = ActionManager::get_action (X_("Monitor Section"), X_("monitor-cut-all"));
	monitor_mute_button.set_related_action (act);

	/* connect signals */
	ARDOUR_UI::Clock.connect (sigc::bind (sigc::mem_fun (primary_clock, &MainClock::set), false));
	ARDOUR_UI::Clock.connect (sigc::bind (sigc::mem_fun (secondary_clock, &MainClock::set), false));

	primary_clock->ValueChanged.connect (sigc::mem_fun(*this, &ARDOUR_UI::primary_clock_value_changed));
	primary_clock->change_display_delta_mode_signal.connect (sigc::mem_fun(UIConfiguration::instance(), &UIConfiguration::set_primary_clock_delta_mode));
	secondary_clock->ValueChanged.connect (sigc::mem_fun(*this, &ARDOUR_UI::secondary_clock_value_changed));
	secondary_clock->change_display_delta_mode_signal.connect (sigc::mem_fun(UIConfiguration::instance(), &UIConfiguration::set_secondary_clock_delta_mode));
	big_clock->ValueChanged.connect (sigc::mem_fun(*this, &ARDOUR_UI::big_clock_value_changed));

	editor_visibility_button.signal_drag_failed().connect (sigc::bind (sigc::ptr_fun (drag_failed), editor));
	mixer_visibility_button.signal_drag_failed().connect (sigc::bind (sigc::ptr_fun (drag_failed), mixer));
	prefs_visibility_button.signal_drag_failed().connect (sigc::bind (sigc::ptr_fun (drag_failed), rc_option_editor));
	recorder_visibility_button.signal_drag_failed().connect (sigc::bind (sigc::ptr_fun (drag_failed), recorder));
	trigger_page_visibility_button.signal_drag_failed().connect (sigc::bind (sigc::ptr_fun (drag_failed), trigger_page));

	_cue_rec_enable.set_name ("record enable button");
	_cue_rec_enable.signal_clicked.connect(sigc::mem_fun(*this, &ARDOUR_UI::cue_rec_state_clicked));

	_cue_play_enable.set_name ("transport option button");
	_cue_play_enable.signal_clicked.connect(sigc::mem_fun(*this, &ARDOUR_UI::cue_ffwd_state_clicked));

	/* catch context clicks so that we can show a menu on these buttons */

	editor_visibility_button.signal_button_press_event().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::tabbable_visibility_button_press), X_("editor")), false);
	mixer_visibility_button.signal_button_press_event().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::tabbable_visibility_button_press), X_("mixer")), false);
	prefs_visibility_button.signal_button_press_event().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::tabbable_visibility_button_press), X_("preferences")), false);
	recorder_visibility_button.signal_button_press_event().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::tabbable_visibility_button_press), X_("recorder")), false);
	trigger_page_visibility_button.signal_button_press_event().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::tabbable_visibility_button_press), X_("trigger")), false);

	/* setup widget style/name */

	solo_alert_button.set_name ("rude solo");
	auditioning_alert_button.set_name ("rude audition");
	feedback_alert_button.set_name ("feedback alert");
	error_alert_button.set_name ("error alert");

	solo_alert_button.set_elements (ArdourButton::Element(ArdourButton::Body|ArdourButton::Text));
	auditioning_alert_button.set_elements (ArdourButton::Element(ArdourButton::Body|ArdourButton::Text));
	feedback_alert_button.set_elements (ArdourButton::Element(ArdourButton::Body|ArdourButton::Text));

	solo_alert_button.set_layout_font (UIConfiguration::instance().get_SmallerFont());
	auditioning_alert_button.set_layout_font (UIConfiguration::instance().get_SmallerFont());
	feedback_alert_button.set_layout_font (UIConfiguration::instance().get_SmallerFont());

	feedback_alert_button.set_sizing_text (_("Facdbeek")); //< longest of "Feedback" and "No Align"

	editor_visibility_button.set_name (X_("page switch button"));
	mixer_visibility_button.set_name (X_("page switch button"));
	prefs_visibility_button.set_name (X_("page switch button"));
	recorder_visibility_button.set_name (X_("page switch button"));
	trigger_page_visibility_button.set_name (X_("page switch button"));

	monitor_dim_button.set_name ("monitor section dim");
	monitor_mono_button.set_name ("monitor section mono");
	monitor_mute_button.set_name ("mute button");

	monitor_dim_button.set_layout_font (UIConfiguration::instance().get_SmallerFont());
	monitor_mono_button.set_layout_font (UIConfiguration::instance().get_SmallerFont());
	monitor_mute_button.set_layout_font (UIConfiguration::instance().get_SmallerFont());

	monitor_dim_button.set_elements (ArdourButton::Element(ArdourButton::Body|ArdourButton::Text));
	monitor_mono_button.set_elements (ArdourButton::Element(ArdourButton::Body|ArdourButton::Text));
	monitor_mute_button.set_elements (ArdourButton::Element(ArdourButton::Body|ArdourButton::Text));

	/* and widget text */

	monitor_dim_button.set_text (_("Dim All"));
	monitor_mono_button.set_text (_("Mono"));
	monitor_mute_button.set_text (_("Mute All"));

	/* and tooltips */

	Gtkmm2ext::UI::instance()->set_tip (editor_visibility_button,
	                                    string_compose (_("Left-Click to show the %1 window\n"
	                                                      "Right-click to show more options"), editor->name()));

	Gtkmm2ext::UI::instance()->set_tip (mixer_visibility_button,
	                                    string_compose (_("Left-Click to show the %1 window\n"
	                                                      "Right-click to show more options"), mixer->name()));

	Gtkmm2ext::UI::instance()->set_tip (prefs_visibility_button,
	                                    string_compose (_("Left-Click to show the %1 window\n"
	                                                      "Right-click to show more options"), rc_option_editor->name()));

	Gtkmm2ext::UI::instance()->set_tip (recorder_visibility_button,
	                                    string_compose (_("Left-Click to show the %1 window\n"
	                                                      "Right-click to show more options"), recorder->name()));

	Gtkmm2ext::UI::instance()->set_tip (trigger_page_visibility_button,
	                                    string_compose (_("Left-Click to show the %1 window\n"
	                                                      "Right-click to show more options"), trigger_page->name()));


	/* monitor section */
	Gtkmm2ext::UI::instance()->set_tip (monitor_dim_button, _("Monitor section dim output"));
	Gtkmm2ext::UI::instance()->set_tip (monitor_mono_button, _("Monitor section mono output"));
	Gtkmm2ext::UI::instance()->set_tip (monitor_mute_button, _("Monitor section mute output"));

	/* transport control size-group */

	Glib::RefPtr<SizeGroup> monitor_button_size_group = SizeGroup::create (Gtk::SIZE_GROUP_HORIZONTAL);
	monitor_button_size_group->add_widget (monitor_dim_button);
	monitor_button_size_group->add_widget (monitor_mono_button);
	monitor_button_size_group->add_widget (monitor_mute_button);

	/* and now the layout... */

	/* top level packing */
	transport_table.set_spacings (0);
	transport_table.set_row_spacings (4);
	transport_table.set_border_width (1);

	transport_frame.set_name ("TransportFrame");
	transport_frame.set_shadow_type (Gtk::SHADOW_NONE);

	/* An event box to hold the table. We use this because we want specific
	   control over the background color, and without this event box,
	   nothing inside the transport_frame actually draws a background. We
	   would therefore end up seeing the background of the parent widget,
	   which is probably some default color. Adding the EventBox adds a
	   widget that will draw the background, using a style based on
	   the parent, "TransportFrame".
	*/
	Gtk::EventBox* ebox = manage (new Gtk::EventBox);
	transport_frame.add (*ebox);
	ebox->add (transport_table);

	/* alert box sub-group */
	VBox* alert_box = manage (new VBox);
	alert_box->set_homogeneous (true);
	alert_box->set_spacing (1);
	alert_box->set_border_width (0);
	alert_box->pack_start (solo_alert_button, true, true);
	alert_box->pack_start (auditioning_alert_button, true, true);
	alert_box->pack_start (feedback_alert_button, true, true);

	/* monitor section sub-group */
	VBox* monitor_box = manage (new VBox);
	monitor_box->set_homogeneous (true);
	monitor_box->set_spacing (1);
	monitor_box->set_border_width (0);
	monitor_box->pack_start (monitor_mono_button, true, true);
	monitor_box->pack_start (monitor_dim_button, true, true);
	monitor_box->pack_start (monitor_mute_button, true, true);

	/* clock button size groups */
	button_height_size_group->add_widget (*primary_clock->left_btn());
	button_height_size_group->add_widget (*primary_clock->right_btn());
	button_height_size_group->add_widget (*secondary_clock->left_btn());
	button_height_size_group->add_widget (*secondary_clock->right_btn());

	//tab selections
	button_height_size_group->add_widget (trigger_page_visibility_button);
	button_height_size_group->add_widget (recorder_visibility_button);
	button_height_size_group->add_widget (editor_visibility_button);
	button_height_size_group->add_widget (mixer_visibility_button);
	button_height_size_group->add_widget (prefs_visibility_button);

	for (int i = 0; i < MAX_LUA_ACTION_BUTTONS; ++i) {
		button_height_size_group->add_widget (action_script_call_btn[i]);
	}

	Glib::RefPtr<SizeGroup> clock1_size_group = SizeGroup::create (SIZE_GROUP_HORIZONTAL);
	clock1_size_group->add_widget (*primary_clock->left_btn());
	clock1_size_group->add_widget (*primary_clock->right_btn());

	Glib::RefPtr<SizeGroup> clock2_size_group = SizeGroup::create (SIZE_GROUP_HORIZONTAL);
	clock2_size_group->add_widget (*secondary_clock->left_btn());
	clock2_size_group->add_widget (*secondary_clock->right_btn());

	/* and the main table layout */
	int vpadding = 1;
	int hpadding = 2;
	int col = 0;
#define TCOL col, col + 1

	transport_table.attach (*application_bar, TCOL, 0, 2 , EXPAND|FILL, EXPAND|FILL, 3, 0);
	++col;

	transport_table.attach (*alert_box, TCOL, 0, 2, SHRINK, EXPAND|FILL, hpadding, 0);
	++col;

	transport_table.attach (monitor_spacer, TCOL, 0, 2 , SHRINK, EXPAND|FILL, 3, 0);
	++col;

	transport_table.attach (*monitor_box, TCOL, 0, 2 , SHRINK, EXPAND|FILL, 3, 0);
	++col;

	transport_table.attach (cuectrl_spacer, TCOL, 0, 2 , SHRINK, EXPAND|FILL, 3, 0);
	++col;

	transport_table.attach (_cue_rec_enable, TCOL, 0, 1 , FILL, FILL, 3, 0);
	transport_table.attach (_cue_play_enable, TCOL, 1, 2 , FILL, FILL, 3, 0);
	++col;

	/* editor-meter, mini-timeline and selection clock are options in the transport_hbox */
	transport_hbox.set_spacing (3);
	transport_table.attach (transport_hbox, TCOL, 0, 2, EXPAND|FILL, EXPAND|FILL, hpadding, 0);
	++col;

	/* lua script action buttons */
	for (int i = 0; i < MAX_LUA_ACTION_BUTTONS; ++i) {
		const int r = i % 2;
		const int c = col + i / 2;
		transport_table.attach (action_script_call_btn[i], c, c + 1, r, r + 1, FILL, SHRINK, 1, vpadding);
	}
	col += MAX_LUA_ACTION_BUTTONS / 2;

	transport_table.attach (scripts_spacer, TCOL, 0, 2 , SHRINK, EXPAND|FILL, 3, 0);
	++col;

	transport_table.attach (recorder_visibility_button,     TCOL, 0, 1 , FILL, SHRINK, hpadding, vpadding);
	transport_table.attach (trigger_page_visibility_button, TCOL, 1, 2 , FILL, SHRINK, hpadding, vpadding);
	++col;
	transport_table.attach (editor_visibility_button,       TCOL, 0, 1 , FILL, SHRINK, hpadding, vpadding);
	transport_table.attach (mixer_visibility_button,        TCOL, 1, 2 , FILL, SHRINK, hpadding, vpadding);
	++col;

	/* initialize */
	update_clock_visibility ();
	/* desensitize */

	feedback_alert_button.set_sensitive (false);
	feedback_alert_button.set_visual_state (Gtkmm2ext::NoVisualState);
	auditioning_alert_button.set_sensitive (false);
	auditioning_alert_button.set_visual_state (Gtkmm2ext::NoVisualState);
}
#undef PX_SCALE
#undef TCOL

void
ARDOUR_UI::soloing_changed (bool onoff)
{
	if (solo_alert_button.get_active() != onoff) {
		solo_alert_button.set_active (onoff);
	}
}

void
ARDOUR_UI::_auditioning_changed (bool onoff)
{
	auditioning_alert_button.set_active (onoff);
	auditioning_alert_button.set_sensitive (onoff);
	if (!onoff) {
		auditioning_alert_button.set_visual_state (Gtkmm2ext::NoVisualState);
	}
}

void
ARDOUR_UI::auditioning_changed (bool onoff)
{
	UI::instance()->call_slot (MISSING_INVALIDATOR, std::bind (&ARDOUR_UI::_auditioning_changed, this, onoff));
}

void
ARDOUR_UI::audition_alert_clicked ()
{
	if (_session) {
		_session->cancel_audition();
	}
}

bool
ARDOUR_UI::error_alert_press (GdkEventButton* ev)
{
	bool do_toggle = true;
	if (ev->button == 1) {
		if (_log_not_acknowledged == LogLevelError) {
			// just acknowledge the error, don't hide the log if it's already visible
			RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Editor"), X_("toggle-log-window"));
			if (tact->get_active()) {
				do_toggle = false;
			}
		}
		_log_not_acknowledged = LogLevelNone;
		error_blink (false); // immediate acknowledge
	}
	// maybe fall through to to button toggle
	return !do_toggle;
}


void
ARDOUR_UI::solo_blink (bool onoff)
{
	if (_session == 0) {
		return;
	}

	if (_session->soloing() || _session->listening()) {
		if (onoff) {
			solo_alert_button.set_active (true);
		} else {
			solo_alert_button.set_active (false);
		}
	} else {
		solo_alert_button.set_active (false);
	}
}

void
ARDOUR_UI::audition_blink (bool onoff)
{
	if (_session == 0) {
		return;
	}

	if (_session->is_auditioning()) {
		if (onoff) {
			auditioning_alert_button.set_active (true);
		} else {
			auditioning_alert_button.set_active (false);
		}
	} else {
		auditioning_alert_button.set_active (false);
	}
}

void
ARDOUR_UI::feedback_blink (bool onoff)
{
	if (_feedback_exists) {
		feedback_alert_button.set_active (true);
		feedback_alert_button.set_text (_("Feedback"));
		if (onoff) {
			feedback_alert_button.reset_fixed_colors ();
		} else {
			feedback_alert_button.set_active_color (UIConfigurationBase::instance().color ("feedback alert: alt active", NULL));
		}
	} else if (_ambiguous_latency && !UIConfiguration::instance().get_show_toolbar_latency ()) {
		feedback_alert_button.set_text (_("No Align"));
		feedback_alert_button.set_active (true);
		if (onoff) {
			feedback_alert_button.reset_fixed_colors ();
		} else {
			feedback_alert_button.set_active_color (UIConfigurationBase::instance().color ("feedback alert: alt active", NULL));
		}
	} else {
		feedback_alert_button.set_text (_("Feedback"));
		feedback_alert_button.reset_fixed_colors ();
		feedback_alert_button.set_active (false);
	}
}

void
ARDOUR_UI::error_blink (bool onoff)
{
	switch (_log_not_acknowledged) {
		case LogLevelError:
			// blink
			if (onoff) {
				error_alert_button.set_custom_led_color(0xff0000ff); // bright red
			} else {
				error_alert_button.set_custom_led_color(0x880000ff); // dark red
			}
			break;
		case LogLevelWarning:
			error_alert_button.set_custom_led_color(0xccaa00ff); // yellow
			break;
		case LogLevelInfo:
			error_alert_button.set_custom_led_color(0x88cc00ff); // lime green
			break;
		default:
			error_alert_button.set_custom_led_color(0x333333ff); // gray
			break;
	}
}

void
ARDOUR_UI::set_punch_sensitivity ()
{
	bool can_punch = _session && _session->punch_is_possible() && _session->locations()->auto_punch_location ();
	ActionManager::get_action ("Transport", "TogglePunchIn")->set_sensitive (can_punch);
	ActionManager::get_action ("Transport", "TogglePunchOut")->set_sensitive (can_punch);
}

void
ARDOUR_UI::editor_realized ()
{
	std::function<void (string)> pc (std::bind (&ARDOUR_UI::parameter_changed, this, _1));
	Config->map_parameters (pc);

	UIConfiguration::instance().reset_dpi ();
}

void
ARDOUR_UI::maximise_editing_space ()
{
	if (editor) {
		editor->maximise_editing_space ();
	}
}

void
ARDOUR_UI::restore_editing_space ()
{
	if (editor) {
		editor->restore_editing_space ();
	}
}

void
ARDOUR_UI::show_ui_prefs ()
{
	if (rc_option_editor) {
		show_tabbable (rc_option_editor);
		rc_option_editor->set_current_page (_("Appearance"));
	}
}

void
ARDOUR_UI::show_mixer_prefs ()
{
	if (rc_option_editor) {
		show_tabbable (rc_option_editor);
		rc_option_editor->set_current_page (_("Signal Flow"));
	}
}

void
ARDOUR_UI::show_plugin_prefs ()
{
	if (rc_option_editor) {
		show_tabbable (rc_option_editor);
		rc_option_editor->set_current_page (_("Plugins"));
	}
}

bool
ARDOUR_UI::click_button_clicked (GdkEventButton* ev)
{
	if (ev->button != 3) {
		/* this handler is just for button-3 clicks */
		return false;
	}

	show_tabbable (rc_option_editor);
	rc_option_editor->set_current_page (_("Metronome"));
	return true;
}

void
ARDOUR_UI::toggle_follow_edits ()
{
	RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Transport"), X_("ToggleFollowEdits"));
	UIConfiguration::instance().set_follow_edits (tact->get_active ());
}

void
ARDOUR_UI::update_title ()
{
	stringstream snap_label;
	snap_label << X_("<span weight=\"ultralight\">")
	           << _("Name")
	           << X_("</span>: ");

	if (_session) {
		bool dirty = _session->dirty();

		string session_name;

		if (_session->snap_name() != _session->name()) {
			session_name = _session->snap_name();
		} else {
			session_name = _session->name();
		}

		if (dirty) {
			session_name = "*" + session_name;
		}

		WindowTitle title (session_name);
		title += Glib::get_application_name();
		_main_window.set_title (title.get_string());

		snap_label << Gtkmm2ext::markup_escape_text (session_name);
	} else {
		WindowTitle title (Glib::get_application_name());
		_main_window.set_title (title.get_string());
		snap_label << "-";
	}
	snapshot_name_label.set_markup (snap_label.str());
}

