/*
 * Copyright (C) 2005-2024 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2024 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2024 Ben Loftis <ben@harrisonconsoles.com>
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
#include "gtk2ardour-version.h"
#endif

#include <glib.h>
#include "pbd/gstdio_compat.h"

#include <ytkmm/accelmap.h>
#include <ytkmm/messagedialog.h>
#include <ytkmm/stock.h>
#include <ytkmm/uimanager.h>

#include "pbd/error.h"
#include "pbd/compose.h"
#include "pbd/convert.h"
#include "pbd/failed_constructor.h"
#include "pbd/memento_command.h"
#include "pbd/openuri.h"
#include "pbd/types_convert.h"
#include "pbd/file_utils.h"
#include "pbd/pthread_utils.h"
#include "pbd/replace_all.h"
#include "pbd/scoped_file_descriptor.h"
#include "pbd/xml++.h"

#include "gtkmm2ext/application.h"
#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/menu_elems.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/window_title.h"

#include "widgets/tooltips.h"

#include "ardour/ardour.h"
#include "ardour/audioengine.h"
#include "ardour/profile.h"
#include "ardour/revision.h"
#include "ardour/transport_master.h"
#include "ardour/transport_master_manager.h"
#include "ardour/triggerbox.h"
#include "ardour/track.h"
#include "ardour/vca_manager.h"
#include "ardour/utils.h"

#include "control_protocol/basic_ui.h"

#include "actions.h"
#include "application_bar.h"
#include "ardour_ui.h"
#include "debug.h"
#include "gui_object.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "keyeditor.h"
#include "luainstance.h"
#include "main_clock.h"
#include "meter_patterns.h"
#include "mixer_ui.h"
#include "public_editor.h"
#include "rc_option_editor.h"
#include "recorder_ui.h"
#include "session_dialog.h"
#include "session_option_editor.h"
#include "splash.h"
#include "time_info_box.h"
#include "timers.h"
#include "trigger_page.h"
#include "triggerbox_ui.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace ArdourWidgets;
using namespace Gtk;
using namespace std;
using namespace Gtk::Menu_Helpers;

static const gchar *_record_mode_strings_[] = {
	N_("Layered"),
	N_("Non-Layered"),
	N_("Snd on Snd"),
	0
};

#define PX_SCALE(px) std::max((float)px, rintf((float)px * UIConfiguration::instance().get_ui_scale()))

ApplicationBar::ApplicationBar ()
	: _have_layout (false)
	, _basic_ui (0)
	, _latency_disable_button (ArdourButton::led_default_elements)
	, _auto_return_button (ArdourButton::led_default_elements)
	, _follow_edits_button (ArdourButton::led_default_elements)
	, _primary_clock  (X_("primary"), X_("transport"), MainClock::PrimaryClock)
	, _secondary_clock (X_("secondary"), X_("secondary"), MainClock::SecondaryClock)
	, _secondary_clock_spacer (0)
	, _auditioning_alert_button (_("Audition"))
	, _solo_alert_button (_("Solo"))
	, _feedback_alert_button (_("Feedback"))
	, _cue_rec_enable (_("Rec Cues"), ArdourButton::led_default_elements)
	, _cue_play_enable (_("Play Cues"), ArdourButton::led_default_elements)
	, _time_info_box (0)
	, _editor_meter_peak_display()
	, _editor_meter(0)
	, _feedback_exists (false)
	, _ambiguous_latency (false)
	, _clear_editor_meter (true)
	, _editor_meter_peaked (false)
{
	_record_mode_strings = I18N (_record_mode_strings_);

	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &ApplicationBar::parameter_changed));
	ARDOUR_UI::instance()->ActionsReady.connect (_forever_connections, MISSING_INVALIDATOR, std::bind (&ApplicationBar::ui_actions_ready, this), gui_context ());
}

ApplicationBar::~ApplicationBar ()
{
	delete _time_info_box;
	_time_info_box = 0;
}

void
ApplicationBar::on_parent_changed (Gtk::Widget*)
{
	assert (!_have_layout);
	_have_layout = true;

	_transport_ctrl.setup (ARDOUR_UI::instance ());
	_transport_ctrl.map_actions ();

	/* sync_button */
	_sync_button.signal_button_press_event().connect (sigc::mem_fun (*this, &ApplicationBar::sync_button_clicked), false);
	_sync_button.set_sizing_text (S_("LogestSync|M-Clk"));

	/* sub-layout for Sync | Shuttle (grow) */
	HBox* ssbox = manage (new HBox);
	ssbox->set_spacing (PX_SCALE(2));
	ssbox->pack_start (_sync_button, false, false, 0);
	ssbox->pack_start (_shuttle_box, true, true, 0);
	ssbox->pack_start (*_shuttle_box.vari_button(), false, false, 0);
	ssbox->pack_start (*_shuttle_box.info_button(), false, false, 0);

	_punch_label.set_text (_("Punch:"));
	_layered_label.set_text (_("Rec:"));

	_punch_in_button.set_text (S_("Punch|In"));
	_punch_out_button.set_text (S_("Punch|Out"));

	_record_mode_selector.AddMenuElem (MenuElem (_record_mode_strings[(int)RecLayered], sigc::bind (sigc::mem_fun (*this, &ApplicationBar::set_record_mode), RecLayered)));
	_record_mode_selector.AddMenuElem (MenuElem (_record_mode_strings[(int)RecNonLayered], sigc::bind (sigc::mem_fun (*this, &ApplicationBar::set_record_mode), RecNonLayered)));
	_record_mode_selector.AddMenuElem (MenuElem (_record_mode_strings[(int)RecSoundOnSound], sigc::bind (sigc::mem_fun (*this, &ApplicationBar::set_record_mode), RecSoundOnSound)));
	_record_mode_selector.set_sizing_texts (_record_mode_strings);

	_latency_disable_button.set_text (_("Disable PDC"));

	_auto_return_button.set_text(_("Auto Return"));
	_follow_edits_button.set_text(_("Follow Range"));

	/* alert box sub-group */
	VBox* alert_box = manage (new VBox);
	alert_box->set_homogeneous (true);
	alert_box->set_spacing (1);
	alert_box->set_border_width (0);
	alert_box->pack_start (_solo_alert_button, true, true);
	alert_box->pack_start (_auditioning_alert_button, true, true);
	alert_box->pack_start (_feedback_alert_button, true, true);

	/* monitor section sub-group */
	VBox* monitor_box = manage (new VBox);
	monitor_box->set_homogeneous (true);
	monitor_box->set_spacing (1);
	monitor_box->set_border_width (0);
	monitor_box->pack_start (_monitor_mono_button, true, true);
	monitor_box->pack_start (_monitor_dim_button, true, true);
	monitor_box->pack_start (_monitor_mute_button, true, true);

	_monitor_dim_button.set_text (_("Dim All"));
	_monitor_mono_button.set_text (_("Mono"));
	_monitor_mute_button.set_text (_("Mute All"));

	_cue_rec_enable.signal_clicked.connect(sigc::mem_fun(*this, &ApplicationBar::cue_rec_state_clicked));
	_cue_play_enable.signal_clicked.connect(sigc::mem_fun(*this, &ApplicationBar::cue_ffwd_state_clicked));
	_auditioning_alert_button.signal_clicked.connect (sigc::mem_fun(*this,&ApplicationBar::audition_alert_clicked));

	_time_info_box = new TimeInfoBox ("ToolbarTimeInfo", false);

	int vpadding = 1;
	int hpadding = 2;
	int col = 0;
#define TCOL col, col + 1

	_table.attach (_transport_ctrl, TCOL, 0, 1 , SHRINK, SHRINK, 0, 0);
	_table.attach (*ssbox,         TCOL, 1, 2 , FILL,   SHRINK, 0, 0);
	++col;

	_table.attach (*(manage (new ArdourVSpacer ())), TCOL, 0, 2 , SHRINK, EXPAND|FILL, 3, 0);
	++col;

	_table.attach (_punch_label, TCOL, 0, 1 , FILL, SHRINK, 3, 0);
	_table.attach (_layered_label, TCOL, 1, 2 , FILL, SHRINK, 3, 0);
	++col;

	_table.attach (_punch_in_button,      col,      col + 1, 0, 1 , FILL, SHRINK, hpadding, vpadding);
	_table.attach (_punch_space,          col + 1,  col + 2, 0, 1 , FILL, SHRINK, 0, vpadding);
	_table.attach (_punch_out_button,     col + 2,  col + 3, 0, 1 , FILL, SHRINK, hpadding, vpadding);
	_table.attach (_record_mode_selector, col,      col + 3, 1, 2 , FILL, SHRINK, hpadding, vpadding);
	col += 3;

	_table.attach (_recpunch_spacer, TCOL, 0, 2 , SHRINK, EXPAND|FILL, 3, 0);
	++col;

	_table.attach (_latency_disable_button, TCOL, 0, 1 , FILL, SHRINK, hpadding, vpadding);
	_table.attach (_route_latency_value, TCOL, 1, 2 , SHRINK, EXPAND|FILL, hpadding, 0);
	++col;

	_route_latency_value.set_alignment (Gtk::ALIGN_END, Gtk::ALIGN_CENTER);

	_table.attach (_latency_spacer, TCOL, 0, 2 , SHRINK, EXPAND|FILL, 3, 0);
	++col;

	_table.attach (_follow_edits_button, TCOL, 0, 1 , FILL, SHRINK, hpadding, vpadding);
	_table.attach (_auto_return_button,  TCOL, 1, 2 , FILL, SHRINK, hpadding, vpadding);
	++col;

	_table.attach (*(manage (new ArdourVSpacer ())), TCOL, 0, 2 , SHRINK, EXPAND|FILL, 3, 0);
	++col;

	_table.attach (_primary_clock,                col,     col + 2, 0, 1 , FILL, SHRINK, hpadding, 0);
	_table.attach (*(_primary_clock.left_btn()),  col,     col + 1, 1, 2 , FILL, SHRINK, hpadding, 0);
	_table.attach (*(_primary_clock.right_btn()), col + 1, col + 2, 1, 2 , FILL, SHRINK, hpadding, 0);
	col += 2;

	_table.attach (*(manage (new ArdourVSpacer ())), TCOL, 0, 2 , SHRINK, EXPAND|FILL, 3, 0);
	++col;

	if (!ARDOUR::Profile->get_small_screen()) {
		_table.attach (_secondary_clock,                col,     col + 2, 0, 1 , FILL, SHRINK, hpadding, 0);
		_table.attach (*(_secondary_clock.left_btn()),  col,     col + 1, 1, 2 , FILL, SHRINK, hpadding, 0);
		_table.attach (*(_secondary_clock.right_btn()), col + 1, col + 2, 1, 2 , FILL, SHRINK, hpadding, 0);
		(ARDOUR_UI::instance()->secondary_clock)->set_no_show_all (true);
		(ARDOUR_UI::instance()->secondary_clock)->left_btn()->set_no_show_all (true);
		(ARDOUR_UI::instance()->secondary_clock)->right_btn()->set_no_show_all (true);
		col += 2;

		_secondary_clock_spacer = manage (new ArdourVSpacer ());
		_table.attach (*_secondary_clock_spacer, TCOL, 0, 2 , SHRINK, EXPAND|FILL, 3, 0);
		++col;
	}

	_table.attach (*alert_box, TCOL, 0, 2, SHRINK, EXPAND|FILL, hpadding, 0);
	++col;

	_table.attach (_monitor_spacer, TCOL, 0, 2 , SHRINK, EXPAND|FILL, 3, 0);
	++col;

	_table.attach (*monitor_box, TCOL, 0, 2 , SHRINK, EXPAND|FILL, 3, 0);
	++col;

	_table.attach (_cuectrl_spacer, TCOL, 0, 2 , SHRINK, EXPAND|FILL, 3, 0);
	++col;

	_table.attach (_cue_rec_enable, TCOL, 0, 1 , FILL, FILL, 3, 0);
	_table.attach (_cue_play_enable, TCOL, 1, 2 , FILL, FILL, 3, 0);
	++col;

	/* editor-meter, mini-timeline and selection clock are options in the transport_hbox */
	_transport_hbox.set_spacing (3);
	_table.attach (_transport_hbox, TCOL, 0, 2, EXPAND|FILL, EXPAND|FILL, hpadding, 0);
	++col;

	/* lua script action buttons */
	for (int i = 0; i < MAX_LUA_ACTION_BUTTONS; ++i) {
		const int r = i % 2;
		const int c = col + i / 2;
		_table.attach (_action_script_call_btn[i], c, c + 1, r, r + 1, FILL, SHRINK, 1, vpadding);
	}
	col += MAX_LUA_ACTION_BUTTONS / 2;

	_table.attach (_scripts_spacer, TCOL, 0, 2 , SHRINK, EXPAND|FILL, 3, 0);
	++col;

	_table.set_spacings (0);
	_table.set_row_spacings (4);
	_table.set_border_width (1);

	_table.show_all (); // TODO: update visibility somewhere else
	pack_start (_table, true, true);

	/*sizing */
	Glib::RefPtr<SizeGroup> button_height_size_group = ARDOUR_UI::instance()->button_height_size_group;
	button_height_size_group->add_widget (_transport_ctrl.size_button ());
	button_height_size_group->add_widget (_sync_button);
	button_height_size_group->add_widget (_punch_in_button);
	button_height_size_group->add_widget (_punch_out_button);
	button_height_size_group->add_widget (_record_mode_selector);
	button_height_size_group->add_widget (_latency_disable_button);
	button_height_size_group->add_widget (_follow_edits_button);
	button_height_size_group->add_widget (_auto_return_button);

	for (int i = 0; i < MAX_LUA_ACTION_BUTTONS; ++i) {
		button_height_size_group->add_widget (_action_script_call_btn[i]);
	}

	/* clock button size groups */
	button_height_size_group->add_widget (*_primary_clock.left_btn());
	button_height_size_group->add_widget (*_primary_clock.right_btn());
	button_height_size_group->add_widget (*_secondary_clock.left_btn());
	button_height_size_group->add_widget (*_secondary_clock.right_btn());

	Glib::RefPtr<SizeGroup> punch_button_size_group = SizeGroup::create (Gtk::SIZE_GROUP_HORIZONTAL);
	punch_button_size_group->add_widget (_punch_in_button);
	punch_button_size_group->add_widget (_punch_out_button);

	Glib::RefPtr<SizeGroup> clock1_size_group = SizeGroup::create (SIZE_GROUP_HORIZONTAL);
	clock1_size_group->add_widget (*_primary_clock.left_btn());
	clock1_size_group->add_widget (*_primary_clock.right_btn());

	Glib::RefPtr<SizeGroup> clock2_size_group = SizeGroup::create (SIZE_GROUP_HORIZONTAL);
	clock2_size_group->add_widget (*_secondary_clock.left_btn());
	clock2_size_group->add_widget (*_secondary_clock.right_btn());

	Glib::RefPtr<SizeGroup> monitor_button_size_group = SizeGroup::create (Gtk::SIZE_GROUP_HORIZONTAL);
	monitor_button_size_group->add_widget (_monitor_dim_button);
	monitor_button_size_group->add_widget (_monitor_mono_button);
	monitor_button_size_group->add_widget (_monitor_mute_button);

	/* tooltips */
	Gtkmm2ext::UI::instance()->set_tip (_punch_in_button, _("Start recording at auto-punch start"));
	Gtkmm2ext::UI::instance()->set_tip (_punch_out_button, _("Stop recording at auto-punch end"));
	Gtkmm2ext::UI::instance()->set_tip (_record_mode_selector, _("<b>Layered</b>, new recordings will be added as regions on a layer atop existing regions.\n<b>SoundOnSound</b>, behaves like <i>Layered</i>, except underlying regions will be audible.\n<b>Non Layered</b>, the underlying region will be spliced and replaced with the newly recorded region."));
	Gtkmm2ext::UI::instance()->set_tip (_latency_disable_button, _("Disable all Plugin Delay Compensation. This results in the shortest delay from live input to output, but any paths with delay-causing plugins will sound later than those without."));
	Gtkmm2ext::UI::instance()->set_tip (_auto_return_button, _("Return to last playback start when stopped"));
	Gtkmm2ext::UI::instance()->set_tip (_follow_edits_button, _("Playhead follows Range tool clicks, and Range selections"));
	Gtkmm2ext::UI::instance()->set_tip (_primary_clock, _("<b>Primary Clock</b> right-click to set display mode. Click to edit, click+drag a digit or mouse-over+scroll wheel to modify.\nText edits: right-to-left overwrite <tt>Esc</tt>: cancel; <tt>Enter</tt>: confirm; postfix the edit with '+' or '-' to enter delta times.\n"));
	Gtkmm2ext::UI::instance()->set_tip (_secondary_clock, _("<b>Secondary Clock</b> right-click to set display mode. Click to edit, click+drag a digit or mouse-over+scroll wheel to modify.\nText edits: right-to-left overwrite <tt>Esc</tt>: cancel; <tt>Enter</tt>: confirm; postfix the edit with '+' or '-' to enter delta times.\n"));
	Gtkmm2ext::UI::instance()->set_tip (_solo_alert_button, _("When active, something is soloed.\nClick to de-solo everything"));
	Gtkmm2ext::UI::instance()->set_tip (_auditioning_alert_button, _("When active, auditioning is taking place.\nClick to stop the audition"));
	Gtkmm2ext::UI::instance()->set_tip (_feedback_alert_button, _("When lit, there is a ports connection issue, leading to feedback loop or ambiguous alignment.\nThis is caused by connecting an output back to some input (feedback), or by multiple connections from a source to the same output via different paths (ambiguous latency, record alignment)."));
	Gtkmm2ext::UI::instance()->set_tip (_monitor_dim_button, _("Monitor section dim output"));
	Gtkmm2ext::UI::instance()->set_tip (_monitor_mono_button, _("Monitor section mono output"));
	Gtkmm2ext::UI::instance()->set_tip (_monitor_mute_button, _("Monitor section mute output"));
	Gtkmm2ext::UI::instance()->set_tip (_cue_rec_enable, _("<b>When enabled</b>, triggering Cues will result in Cue Markers added to the timeline"));
	Gtkmm2ext::UI::instance()->set_tip (_cue_play_enable, _("<b>When enabled</b>, Cue Markers will trigger the associated Cue when passed on the timeline"));
	Gtkmm2ext::UI::instance()->set_tip (_editor_meter_peak_display, _("Reset All Peak Meters"));

	/* theming */
	_sync_button.set_name ("transport active option button");
	_punch_in_button.set_name ("punch button");
	_punch_out_button.set_name ("punch button");
	_record_mode_selector.set_name ("record mode button");
	_latency_disable_button.set_name ("latency button");
	_auto_return_button.set_name ("transport option button");
	_follow_edits_button.set_name ("transport option button");
	_solo_alert_button.set_name ("rude solo");
	_auditioning_alert_button.set_name ("rude audition");
	_feedback_alert_button.set_name ("feedback alert");
	_monitor_dim_button.set_name ("monitor section dim");
	_monitor_mono_button.set_name ("monitor section mono");
	_monitor_mute_button.set_name ("mute button");

	_monitor_dim_button.set_layout_font (UIConfiguration::instance().get_SmallerFont());
	_monitor_mono_button.set_layout_font (UIConfiguration::instance().get_SmallerFont());
	_monitor_mute_button.set_layout_font (UIConfiguration::instance().get_SmallerFont());

	_monitor_dim_button.set_elements (ArdourButton::Element(ArdourButton::Body|ArdourButton::Text));
	_monitor_mono_button.set_elements (ArdourButton::Element(ArdourButton::Body|ArdourButton::Text));
	_monitor_mute_button.set_elements (ArdourButton::Element(ArdourButton::Body|ArdourButton::Text));

	_solo_alert_button.set_elements (ArdourButton::Element(ArdourButton::Body|ArdourButton::Text));
	_auditioning_alert_button.set_elements (ArdourButton::Element(ArdourButton::Body|ArdourButton::Text));
	_feedback_alert_button.set_elements (ArdourButton::Element(ArdourButton::Body|ArdourButton::Text));

	_solo_alert_button.set_layout_font (UIConfiguration::instance().get_SmallerFont());
	_auditioning_alert_button.set_layout_font (UIConfiguration::instance().get_SmallerFont());
	_feedback_alert_button.set_layout_font (UIConfiguration::instance().get_SmallerFont());

	_feedback_alert_button.set_sizing_text (_("Feedgeek")); //< longest of "Feedback" and "No Align", include descender

	_cue_rec_enable.set_name ("record enable button");
	_cue_play_enable.set_name ("transport option button");

	/* indicate global latency compensation en/disable */
	ARDOUR::Latent::DisableSwitchChanged.connect (_forever_connections, MISSING_INVALIDATOR, std::bind (&ApplicationBar::latency_switch_changed, this), gui_context ());
	ARDOUR::Session::FeedbackDetected.connect (_forever_connections, MISSING_INVALIDATOR, std::bind (&ApplicationBar::feedback_detected, this), gui_context ());
	ARDOUR::Session::SuccessfulGraphSort.connect (_forever_connections, MISSING_INVALIDATOR, std::bind (&ApplicationBar::successful_graph_sort, this), gui_context ());

	TriggerBox::CueRecordingChanged.connect (_forever_connections, MISSING_INVALIDATOR, std::bind (&ApplicationBar::cue_rec_state_changed, this), gui_context ());
	cue_rec_state_changed();

	/* initialize */
	update_clock_visibility ();
	set_transport_sensitivity (false);
	latency_switch_changed ();
	session_latency_updated (true);

	/* desensitize */
	_feedback_alert_button.set_sensitive (false);
	_feedback_alert_button.set_visual_state (Gtkmm2ext::NoVisualState);
	_auditioning_alert_button.set_sensitive (false);
	_auditioning_alert_button.set_visual_state (Gtkmm2ext::NoVisualState);

	if (_session) {
		repack_transport_hbox ();
	}
}
#undef PX_SCALE
#undef TCOL

void
ApplicationBar::ui_actions_ready ()
{
	_blink_connection = Timers::blink_connect (sigc::mem_fun(*this, &ApplicationBar::blink_handler));

	_point_zero_something_second_connection = Timers::super_rapid_connect (sigc::mem_fun(*this, &ApplicationBar::every_point_zero_something_seconds));

	LuaInstance::instance()->ActionChanged.connect (sigc::mem_fun (*this, &ApplicationBar::action_script_changed));

	Glib::RefPtr<Action> act;

	act = ActionManager::get_action (X_("Transport"), X_("ToggleExternalSync"));
	_sync_button.set_related_action (act);

	act = ActionManager::get_action ("Transport", "TogglePunchIn");
	_punch_in_button.set_related_action (act);
	act = ActionManager::get_action ("Transport", "TogglePunchOut");
	_punch_out_button.set_related_action (act);

	act = ActionManager::get_action ("Main", "ToggleLatencyCompensation");
	_latency_disable_button.set_related_action (act);

	act = ActionManager::get_action ("Transport", "ToggleAutoReturn");
	_auto_return_button.set_related_action (act);
	act = ActionManager::get_action (X_("Transport"), X_("ToggleFollowEdits"));
	_follow_edits_button.set_related_action (act);

	_auto_return_button.set_text(_("Auto Return"));
	_follow_edits_button.set_text(_("Follow Range"));

	/* CANNOT sigc::bind these to clicked or toggled, must use pressed or released */
	act = ActionManager::get_action (X_("Main"), X_("cancel-solo"));
	_solo_alert_button.set_related_action (act);

	act = ActionManager::get_action (X_("Monitor Section"), X_("monitor-dim-all"));
	_monitor_dim_button.set_related_action (act);
	act = ActionManager::get_action (X_("Monitor Section"), X_("monitor-mono"));
	_monitor_mono_button.set_related_action (act);
	act = ActionManager::get_action (X_("Monitor Section"), X_("monitor-cut-all"));
	_monitor_mute_button.set_related_action (act);

	for (int i = 0; i < MAX_LUA_ACTION_BUTTONS; ++i) {
		std::string const a = string_compose (X_("script-%1"), i + 1);
		Glib::RefPtr<Action> act = ActionManager::get_action(X_("LuaAction"), a.c_str());
		assert (act);
		_action_script_call_btn[i].set_name ("lua action button");
		_action_script_call_btn[i].set_text (string_compose ("%1%2", std::hex, i+1));
		_action_script_call_btn[i].set_related_action (act);
		_action_script_call_btn[i].signal_button_press_event().connect (sigc::bind (sigc::mem_fun(*this, &ApplicationBar::bind_lua_action_script), i), false);
		if (act->get_sensitive ()) {
			_action_script_call_btn[i].set_visual_state (Gtkmm2ext::VisualState (_action_script_call_btn[i].visual_state() & ~Gtkmm2ext::Insensitive));
		} else {
			_action_script_call_btn[i].set_visual_state (Gtkmm2ext::VisualState (_action_script_call_btn[i].visual_state() | Gtkmm2ext::Insensitive));
		}
		_action_script_call_btn[i].set_sizing_text ("88");
		_action_script_call_btn[i].set_no_show_all ();
	}

	if (_session && _have_layout) {
		repack_transport_hbox();
	}
}

void
ApplicationBar::repack_transport_hbox ()
{
	if (!_have_layout) {
		return;
	}

	if (_time_info_box) {
		if (_time_info_box->get_parent()) {
			_transport_hbox.remove (*_time_info_box);
		}
		if (UIConfiguration::instance().get_show_toolbar_selclock ()) {
			_transport_hbox.pack_start (*_time_info_box, false, false);
			_time_info_box->show();
		}
	}

	if (_mini_timeline.get_parent()) {
		_transport_hbox.remove (_mini_timeline);
	}
	if (UIConfiguration::instance().get_show_mini_timeline ()) {
		_transport_hbox.pack_start (_mini_timeline, true, true);
		_mini_timeline.show();
	}

	if (_editor_meter) {
		if (_editor_meter_table.get_parent()) {
			_transport_hbox.remove (_editor_meter_table);
		}
		if (_meterbox_spacer.get_parent()) {
			_transport_hbox.remove (_meterbox_spacer);
			_transport_hbox.remove (_meterbox_spacer2);
		}

		if (UIConfiguration::instance().get_show_editor_meter()) {
			_transport_hbox.pack_end (_meterbox_spacer, false, false, 3);
			_transport_hbox.pack_end (_editor_meter_table, false, false);
			_transport_hbox.pack_end (_meterbox_spacer2, false, false, 1);
			_meterbox_spacer2.set_size_request (1, -1);
			_editor_meter_table.show();
			_meterbox_spacer.show();
			_meterbox_spacer2.show();
		}
	}

	bool show_rec = UIConfiguration::instance().get_show_toolbar_recpunch ();
	if (show_rec) {
		_punch_label.show ();
		_layered_label.show ();
		_punch_in_button.show ();
		_punch_out_button.show ();
		_record_mode_selector.show ();
		_recpunch_spacer.show ();
	} else {
		_punch_label.hide ();
		_layered_label.hide ();
		_punch_in_button.hide ();
		_punch_out_button.hide ();
		_record_mode_selector.hide ();
		_recpunch_spacer.hide ();
	}

	bool show_pdc = UIConfiguration::instance().get_show_toolbar_latency ();
	if (show_pdc) {
		_latency_disable_button.show ();
		_route_latency_value.show ();
		_latency_spacer.show ();
	} else {
		_latency_disable_button.hide ();
		_route_latency_value.hide ();
		_latency_spacer.hide ();
	}

	bool show_cue = UIConfiguration::instance().get_show_toolbar_cuectrl ();
	if (show_cue) {
		_cue_rec_enable.show ();
		_cue_play_enable.show ();
		_cuectrl_spacer.show ();
	} else {
		_cue_rec_enable.hide ();
		_cue_play_enable.hide ();
		_cuectrl_spacer.hide ();
	}

	bool show_mnfo = UIConfiguration::instance().get_show_toolbar_monitor_info ();
	if (show_mnfo) {
		_monitor_dim_button.show ();
		_monitor_mono_button.show ();
		_monitor_mute_button.show ();
		_monitor_spacer.show ();
	} else {
		_monitor_dim_button.hide ();
		_monitor_mono_button.hide ();
		_monitor_mute_button.hide ();
		_monitor_spacer.hide ();
	}
}

void
ApplicationBar::feedback_detected ()
{
	_feedback_exists = true;
}

void
ApplicationBar::successful_graph_sort ()
{
	_feedback_exists = false;
}

void
ApplicationBar::soloing_changed (bool onoff)
{
	if (_solo_alert_button.get_active() != onoff) {
		_solo_alert_button.set_active (onoff);
	}
}

void
ApplicationBar::_auditioning_changed (bool onoff)
{
	_auditioning_alert_button.set_active (onoff);
	_auditioning_alert_button.set_sensitive (onoff);
	if (!onoff) {
		_auditioning_alert_button.set_visual_state (Gtkmm2ext::NoVisualState);
	}
	set_transport_sensitivity (!onoff);
}

void
ApplicationBar::auditioning_changed (bool onoff)
{
	UI::instance()->call_slot (MISSING_INVALIDATOR, std::bind (&ApplicationBar::_auditioning_changed, this, onoff));
}

void
ApplicationBar::audition_alert_clicked ()
{
	if (_session) {
		_session->cancel_audition();
	}
}

void
ApplicationBar::solo_blink (bool onoff)
{
	if (_session == 0) {
		return;
	}

	if (_session->soloing() || _session->listening()) {
		if (onoff) {
			_solo_alert_button.set_active (true);
		} else {
			_solo_alert_button.set_active (false);
		}
	} else {
		_solo_alert_button.set_active (false);
	}
}

void
ApplicationBar::audition_blink (bool onoff)
{
	if (_session == 0) {
		return;
	}

	if (_session->is_auditioning()) {
		if (onoff) {
			_auditioning_alert_button.set_active (true);
		} else {
			_auditioning_alert_button.set_active (false);
		}
	} else {
		_auditioning_alert_button.set_active (false);
	}
}

void
ApplicationBar::feedback_blink (bool onoff)
{
	if (_feedback_exists) {
		_feedback_alert_button.set_active (true);
		_feedback_alert_button.set_text (_("Feedback"));
		if (onoff) {
			_feedback_alert_button.reset_fixed_colors ();
		} else {
			_feedback_alert_button.set_active_color (UIConfigurationBase::instance().color ("feedback alert: alt active", NULL));
		}
	} else if (_ambiguous_latency && !UIConfiguration::instance().get_show_toolbar_latency ()) {
		_feedback_alert_button.set_text (_("No Align"));
		_feedback_alert_button.set_active (true);
		if (onoff) {
			_feedback_alert_button.reset_fixed_colors ();
		} else {
			_feedback_alert_button.set_active_color (UIConfigurationBase::instance().color ("feedback alert: alt active", NULL));
		}
	} else {
		_feedback_alert_button.set_text (_("Feedback"));
		_feedback_alert_button.reset_fixed_colors ();
		_feedback_alert_button.set_active (false);
	}
}

bool
ApplicationBar::bind_lua_action_script (GdkEventButton*ev, int i)
{
	if (!_session) {
		return false;
	}
	LuaInstance *li = LuaInstance::instance();
	std::string name;
	if (ev->button != 3 && !(ev->button == 1 && !li->lua_action_name (i, name))) {
		return false;
	}
	if (Gtkmm2ext::Keyboard::modifier_state_equals (ev->state, Gtkmm2ext::Keyboard::TertiaryModifier)) {
		li->remove_lua_action (i);
	} else {
		Gtk::Window *win = dynamic_cast<Gtk::Window*> (get_toplevel());
		li->interactive_add (*win, LuaScriptInfo::EditorAction, i);
	}
	return true;
}

void
ApplicationBar::action_script_changed (int i, const std::string& n)
{
	if (i < 0 || i >= MAX_LUA_ACTION_SCRIPTS) {
		return;
	}

	if (i < MAX_LUA_ACTION_BUTTONS) {
		if (LuaInstance::instance()->lua_action_has_icon (i)) {
			uintptr_t ii = i;
			_action_script_call_btn[i].set_icon (&LuaInstance::render_action_icon, (void*)ii);
		} else {
			_action_script_call_btn[i].set_icon (0, 0);
		}
		if (n.empty ()) {
			_action_script_call_btn[i].set_text (string_compose ("%1%2", std::hex, i+1));
		} else {
			_action_script_call_btn[i].set_text (n.substr(0,1));
		}
	}

	std::string const a = string_compose (X_("script-%1"), i + 1);
	Glib::RefPtr<Action> act = ActionManager::get_action(X_("LuaAction"), a.c_str());
	assert (act);
	if (n.empty ()) {
		act->set_label (string_compose (_("Unset #%1"), i + 1));
		act->set_tooltip (_("No action bound\nRight-click to assign"));
		act->set_sensitive (false);
	} else {
		act->set_label (n);
		act->set_tooltip (string_compose (_("%1\n\nClick to run\nRight-click to re-assign\nShift+right-click to unassign"), n));
		act->set_sensitive (true);
	}
	KeyEditor::UpdateBindings ();
}

void
ApplicationBar::set_session (Session *s)
{
	SessionHandlePtr::set_session (s);

	if (s) {
		_transport_ctrl.set_session (s);
		_shuttle_box.set_session (s);
		_primary_clock.set_session (s);
		_secondary_clock.set_session (s);
		_mini_timeline.set_session (s);
		_time_info_box->set_session (s);
	}

	if (_basic_ui) {
		delete _basic_ui;
		_basic_ui = 0;
	}

	map_transport_state ();

	if (!_session) {
		_point_zero_something_second_connection.disconnect();
		_blink_connection.disconnect ();

		if (_editor_meter) {
			_editor_meter_table.remove(*_editor_meter);
			delete _editor_meter;
			_editor_meter = 0;
			_editor_meter_peak_display.hide();
		}

		return;
	}

	_basic_ui = new BasicUI (*s);

	_session->AuditionActive.connect (_session_connections, MISSING_INVALIDATOR, std::bind (&ApplicationBar::auditioning_changed, this, _1), gui_context());
	_session->TransportStateChange.connect (_session_connections, MISSING_INVALIDATOR, std::bind (&ApplicationBar::map_transport_state, this), gui_context());
	_session->config.ParameterChanged.connect (_session_connections, MISSING_INVALIDATOR, std::bind (&ApplicationBar::parameter_changed, this, _1), gui_context());
	_session->LatencyUpdated.connect (_session_connections, MISSING_INVALIDATOR, std::bind (&ApplicationBar::session_latency_updated, this, _1), gui_context());
	_session->SoloActive.connect (_session_connections, MISSING_INVALIDATOR, std::bind (&ApplicationBar::soloing_changed, this, _1), gui_context());
	_session->AuditionActive.connect (_session_connections, MISSING_INVALIDATOR, std::bind (&ApplicationBar::auditioning_changed, this, _1), gui_context());

	//initialize all session and global config settings
	std::function<void (std::string)> pc (std::bind (&ApplicationBar::parameter_changed, this, _1));
	_session->config.map_parameters (pc);
	UIConfiguration::instance().map_parameters (pc);

	/* initialize */
	session_latency_updated (true);

	_solo_alert_button.set_active (_session->soloing());

	if (_editor_meter_table.get_parent()) {
		_transport_hbox.remove (_editor_meter_table);
	}
	if (_editor_meter) {
		_editor_meter_table.remove(*_editor_meter);
		delete _editor_meter;
		_editor_meter = 0;
	}
	if (_editor_meter_table.get_parent()) {
		_transport_hbox.remove (_editor_meter_table);
	}
	if (_editor_meter_peak_display.get_parent ()) {
		_editor_meter_table.remove (_editor_meter_peak_display);
	}
	if (_session &&
	    _session->master_out() &&
	    _session->master_out()->n_outputs().n(DataType::AUDIO) > 0) {

		_editor_meter = new LevelMeterHBox(_session);
		_editor_meter->set_meter (_session->master_out()->shared_peak_meter().get());
		_editor_meter->clear_meters();
		_editor_meter->setup_meters (30, 10, 6);
		_editor_meter->show();

		_editor_meter_table.set_spacings(3);
		_editor_meter_table.attach(*_editor_meter,             0,1, 0,1, FILL, EXPAND|FILL, 0, 1);
		_editor_meter_table.attach(_editor_meter_peak_display, 0,1, 1,2, FILL, SHRINK, 0, 0);

		_editor_meter->show();
		_editor_meter_peak_display.show();

		ArdourMeter::ResetAllPeakDisplays.connect (sigc::mem_fun(*this, &ApplicationBar::reset_peak_display));
		ArdourMeter::ResetRoutePeakDisplays.connect (sigc::mem_fun(*this, &ApplicationBar::reset_route_peak_display));
		ArdourMeter::ResetGroupPeakDisplays.connect (sigc::mem_fun(*this, &ApplicationBar::reset_group_peak_display));

		_editor_meter_peak_display.set_name ("meterbridge peakindicator");
		_editor_meter_peak_display.set_can_focus (false);
		_editor_meter_peak_display.set_size_request (-1, std::max (5.f, std::min (12.f, rintf (8.f * UIConfiguration::instance().get_ui_scale()))) );
		_editor_meter_peak_display.set_corner_radius (1.0);

		_clear_editor_meter = true;
		_editor_meter_peak_display.signal_button_release_event().connect (sigc::mem_fun(*this, &ApplicationBar::editor_meter_peak_button_release), false);
	}

	if (_have_layout) {
		repack_transport_hbox();
	}
}

void
ApplicationBar::set_transport_sensitivity (bool yn)
{
	ActionManager::set_sensitive (ActionManager::transport_sensitive_actions, yn);
	_shuttle_box.set_sensitive (yn);
}

void
ApplicationBar::latency_switch_changed ()
{
	bool pdc_off = ARDOUR::Latent::zero_latency ();
	if (_latency_disable_button.get_active() != pdc_off) {
		_latency_disable_button.set_active (pdc_off);
	}
}

void
ApplicationBar::focus_on_clock ()
{
	_primary_clock.focus ();
}

void
ApplicationBar::update_clock_visibility ()
{
	if (ARDOUR::Profile->get_small_screen()) {
		return;
	}
	if (UIConfiguration::instance().get_show_secondary_clock ()) {
		_secondary_clock.show();
		_secondary_clock.left_btn()->show();
		_secondary_clock.right_btn()->show();
	} else {
		_secondary_clock.hide();
		_secondary_clock.left_btn()->hide();
		_secondary_clock.right_btn()->hide();
	}
}

void
ApplicationBar::session_latency_updated (bool for_playback)
{
	if (!for_playback) {
		/* latency updates happen in pairs, in the following order:
		 *  - for capture
		 *  - for playback
		 */
		return;
	}

	if (!_session) {
		_route_latency_value.set_text ("--");
	} else {
		samplecnt_t wrl = _session->worst_route_latency ();
		float rate      = _session->nominal_sample_rate ();
		_route_latency_value.set_text (samples_as_time_string (wrl, rate));
	}
}

void
ApplicationBar::parameter_changed (std::string p)
{
	if (p == "external-sync") {
		if (!_session->config.get_external_sync()) {
			_sync_button.set_text (S_("SyncSource|Int."));
		} else {
			_sync_button.set_text (TransportMasterManager::instance().current()->display_name());
		}
	} else if (p == "sync-source") {
		if (_session) {
			if (!_session->config.get_external_sync()) {
				_sync_button.set_text (S_("SyncSource|Int."));
			} else {
				_sync_button.set_text (TransportMasterManager::instance().current()->display_name());
			}
		} else {
			/* changing sync source without a session is unlikely/impossible , except during startup */
			_sync_button.set_text (TransportMasterManager::instance().current()->display_name());
		}
		if (_session->config.get_video_pullup() == 0.0f || TransportMasterManager::instance().current()->type() != Engine) {
			UI::instance()->set_tip (_sync_button, _("Enable/Disable external positional sync"));
		} else {
			UI::instance()->set_tip (_sync_button, _("External sync is not possible: video pull up/down is set"));
		}
	} else if (p == "show-mini-timeline") {
		repack_transport_hbox ();
	} else if (p == "show-dsp-load-info") {
		repack_transport_hbox ();
	} else if (p == "show-disk-space-info") {
		repack_transport_hbox ();
	} else if (p == "show-toolbar-recpunch") {
		repack_transport_hbox ();
	} else if (p == "show-toolbar-monitoring") {
		repack_transport_hbox ();
	} else if (p == "show-toolbar-selclock") {
		repack_transport_hbox ();
	} else if (p == "show-toolbar-latency") {
		repack_transport_hbox ();
	} else if (p == "show-toolbar-cuectrl") {
		repack_transport_hbox ();
	} else if (p == "show-toolbar-monitor-info") {
		repack_transport_hbox ();
	} else if (p == "show-editor-meter") {
		repack_transport_hbox ();
	} else if (p == "show-secondary-clock") {
		update_clock_visibility ();
	} else if (p == "action-table-columns") {
		const uint32_t cols = UIConfiguration::instance().get_action_table_columns ();
		for (int i = 0; i < MAX_LUA_ACTION_BUTTONS; ++i) {
			const int col = i / 2;
			if (cols & (1<<col)) {
				_action_script_call_btn[i].show();
			} else {
				_action_script_call_btn[i].hide();
			}
		}
		if (cols == 0) {
			_scripts_spacer.hide ();
		} else {
			_scripts_spacer.show ();
		}
	} else if (p == "cue-behavior") {
		CueBehavior cb (_session->config.get_cue_behavior());
		_cue_play_enable.set_active (cb & ARDOUR::FollowCues);
	} else if (p == "record-mode") {
		size_t m = _session->config.get_record_mode ();
		assert (m < _record_mode_strings.size ());
		_record_mode_selector.set_active (_record_mode_strings[m]);
	}
}

bool
ApplicationBar::sync_button_clicked (GdkEventButton* ev)
{
	if (ev->button != 3) {
		/* this handler is just for button-3 clicks */
		return false;
	}

	Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action ("Window", "toggle-transport-masters");
	tact->set_active();
	return true;
}

void
ApplicationBar::cue_ffwd_state_clicked ()
{
	PublicEditor::instance().toggle_cue_behavior ();
}

void
ApplicationBar::cue_rec_state_clicked ()
{
	TriggerBox::set_cue_recording(!TriggerBox::cue_recording());
}

void
ApplicationBar::cue_rec_state_changed ()
{
	_cue_rec_enable.set_active_state( TriggerBox::cue_recording() ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	//Config->get_cue_behavior()
}

void
ApplicationBar::set_record_mode (RecordMode m)
{
	if (_session) {
		_session->config.set_record_mode (m);
	}
}

bool
ApplicationBar::editor_meter_peak_button_release (GdkEventButton* ev)
{
	if (ev->button == 1) {
		ArdourMeter::ResetAllPeakDisplays ();
	}
	return false;
}

void
ApplicationBar::sync_blink (bool onoff)
{
	if (_session == 0 || !_session->config.get_external_sync()) {
		/* internal sync */
		_sync_button.set_active (false);
		return;
	}

	if (!_session->transport_locked()) {
		/* not locked, so blink on and off according to the onoff argument */

		if (onoff) {
			_sync_button.set_active (true);
		} else {
			_sync_button.set_active (false);
		}
	} else {
		/* locked */
		_sync_button.set_active (true);
	}
}

void
ApplicationBar::every_point_zero_something_seconds ()
{
	// august 2007: actual update frequency: 25Hz (40ms), not 100Hz

	if (_editor_meter && UIConfiguration::instance().get_show_editor_meter() && _editor_meter_peak_display.get_mapped ()) {

		if (_clear_editor_meter) {
			_editor_meter->clear_meters();
			_editor_meter_peak_display.set_active_state (Gtkmm2ext::Off);
			_clear_editor_meter = false;
			_editor_meter_peaked = false;
		}

		if (!UIConfiguration::instance().get_no_strobe()) {
			const float mpeak = _editor_meter->update_meters();
			const bool peaking = mpeak > UIConfiguration::instance().get_meter_peak();

			if (!_editor_meter_peaked && peaking) {
				_editor_meter_peak_display.set_active_state (Gtkmm2ext::ExplicitActive);
				_editor_meter_peaked = true;
			}
		}
	}
}

void
ApplicationBar::blink_handler (bool blink_on)
{
	sync_blink (blink_on);

	if (UIConfiguration::instance().get_no_strobe() || !UIConfiguration::instance().get_blink_alert_indicators()) {
		blink_on = true;
	}
	solo_blink (blink_on);
	audition_blink (blink_on);
	feedback_blink (blink_on);
}

void
ApplicationBar::map_transport_state ()
{
	_shuttle_box.map_transport_state ();

	if (!_session) {
		_record_mode_selector.set_sensitive (false);
		return;
	}

	float sp = _session->transport_speed();

	if (sp != 0.0f) {
		_record_mode_selector.set_sensitive (!_session->actively_recording ());
	} else {
		_record_mode_selector.set_sensitive (true);
	}
}

void
ApplicationBar::reset_peak_display ()
{
	if (!_session || !_session->master_out() || !_editor_meter) return;
	_clear_editor_meter = true;
}

void
ApplicationBar::reset_group_peak_display (RouteGroup* group)
{
	if (!_session || !_session->master_out()) return;
	if (group == _session->master_out()->route_group()) {
		reset_peak_display ();
	}
}

void
ApplicationBar::reset_route_peak_display (Route* route)
{
	if (!_session || !_session->master_out()) return;
	if (_session->master_out().get() == route) {
		reset_peak_display ();
	}
}
