/*
    Copyright (C) 1999 Paul Davis

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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <cerrno>
#include <iostream>
#include <cmath>

#include <sigc++/bind.h>
#include "canvas/canvas.h"

#include "pbd/error.h"
#include "pbd/basename.h"
#include "pbd/fastlog.h"

#include "gtkmm2ext/cairocell.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/click_box.h"
#include "gtkmm2ext/window_title.h"

#include "ardour/profile.h"
#include "ardour/session.h"
#include "ardour/types.h"

#include "ardour_spacer.h"
#include "ardour_ui.h"
#include "keyboard.h"
#include "public_editor.h"
#include "audio_clock.h"
#include "actions.h"
#include "main_clock.h"
#include "mixer_ui.h"
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
using namespace Gtk;
using namespace Glib;
using namespace ARDOUR_UI_UTILS;

void
ARDOUR_UI::setup_tooltips ()
{
	ArdourCanvas::Canvas::set_tooltip_timeout (Gtk::Settings::get_default()->property_gtk_tooltip_timeout ());

	set_tip (roll_button, _("Play from playhead"));
	set_tip (stop_button, _("Stop playback"));
	set_tip (rec_button, _("Toggle record"));
	set_tip (play_selection_button, _("Play range/selection"));
	set_tip (goto_start_button, _("Go to start of session"));
	set_tip (goto_end_button, _("Go to end of session"));
	set_tip (auto_loop_button, _("Play loop range"));
	set_tip (midi_panic_button, _("MIDI Panic\nSend note off and reset controller messages on all MIDI channels"));
	set_tip (auto_return_button, _("Return to last playback start when stopped"));
	set_tip (follow_edits_button, _("Playhead follows Range tool clicks, and Range selections"));
	set_tip (auto_input_button, _("Be sensible about input monitoring"));
	set_tip (click_button, _("Enable/Disable audio click"));
	set_tip (solo_alert_button, _("When active, something is soloed.\nClick to de-solo everything"));
	set_tip (auditioning_alert_button, _("When active, auditioning is taking place.\nClick to stop the audition"));
	set_tip (feedback_alert_button, _("When active, there is a feedback loop."));
	set_tip (primary_clock, _("<b>Primary Clock</b> right-click to set display mode. Click to edit, click+drag a digit or mouse-over+scroll wheel to modify.\nText edits: right-to-left overwrite <tt>Esc</tt>: cancel; <tt>Enter</tt>: confirm; postfix the edit with '+' or '-' to enter delta times.\n"));
	set_tip (secondary_clock, _("<b>Secondary Clock</b> right-click to set display mode. Click to edit, click+drag a digit or mouse-over+scroll wheel to modify.\nText edits: right-to-left overwrite <tt>Esc</tt>: cancel; <tt>Enter</tt>: confirm; postfix the edit with '+' or '-' to enter delta times.\n"));
	set_tip (editor_meter_peak_display, _("Reset All Peak Indicators"));
	set_tip (error_alert_button, _("Show Error Log and acknowledge warnings"));

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
ARDOUR_UI::display_message (const char *prefix, gint prefix_len, RefPtr<TextBuffer::Tag> ptag, RefPtr<TextBuffer::Tag> mtag, const char *msg)
{
	string text;

	UI::display_message (prefix, prefix_len, ptag, mtag, msg);

	ArdourLogLevel ll = LogLevelNone;

	if (strcmp (prefix, _("[ERROR]: ")) == 0) {
		text = "<span color=\"red\" weight=\"bold\">";
		ll = LogLevelError;
	} else if (strcmp (prefix, _("[WARNING]: ")) == 0) {
		text = "<span color=\"yellow\" weight=\"bold\">";
		ll = LogLevelWarning;
	} else if (strcmp (prefix, _("[INFO]: ")) == 0) {
		text = "<span color=\"green\" weight=\"bold\">";
		ll = LogLevelInfo;
	} else {
		text = "<span color=\"white\" weight=\"bold\">???";
	}

	_log_not_acknowledged = std::max(_log_not_acknowledged, ll);

#ifdef TOP_MENUBAR
	text += prefix;
	text += "</span>";
	text += msg;

	status_bar_label.set_markup (text);
#endif
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
ARDOUR_UI::repack_transport_hbox ()
{
	if (time_info_box) {
		if (time_info_box->get_parent()) {
			transport_hbox.remove (*time_info_box);
		}
		if (UIConfiguration::instance().get_show_toolbar_selclock ()) {
			transport_hbox.pack_start (*time_info_box, false, false);
			time_info_box->show();
		}
	}

	if (mini_timeline.get_parent()) {
		transport_hbox.remove (mini_timeline);
	}
	if (UIConfiguration::instance().get_show_mini_timeline ()) {
		transport_hbox.pack_start (mini_timeline, true, true);
		mini_timeline.show();
	}

	if (editor_meter) {
		if (meter_box.get_parent()) {
			transport_hbox.remove (meter_box);
			transport_hbox.remove (editor_meter_peak_display);
		}

		if (UIConfiguration::instance().get_show_editor_meter()) {
			transport_hbox.pack_end (editor_meter_peak_display, false, false);
			transport_hbox.pack_end (meter_box, false, false);
			meter_box.show();
			editor_meter_peak_display.show();
		}
	}
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
		if (secondary_clock_spacer) {
			secondary_clock_spacer->show();
		}
	} else {
		secondary_clock->hide();
		secondary_clock->left_btn()->hide();
		secondary_clock->right_btn()->hide();
		if (secondary_clock_spacer) {
			secondary_clock_spacer->hide();
		}
	}
}

bool
ARDOUR_UI::transport_expose (GdkEventExpose* ev)
{
return false;
	int x0, y0;
	Gtk::Widget* window_parent;
	Glib::RefPtr<Gdk::Window> win = Gtkmm2ext::window_to_draw_on (transport_table, &window_parent);
	Glib::RefPtr<Gtk::Style> style = transport_table.get_style();
	if (!win || !style) {
		return false;
	}

	Cairo::RefPtr<Cairo::Context> cr = transport_table.get_window()->create_cairo_context ();

	cr->rectangle (ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cr->clip ();

	transport_table.translate_coordinates (*window_parent, 0, 0, x0, y0);

	cr->rectangle (x0, y0, transport_table.get_width(), transport_table.get_height());
	Gdk::Color bg (style->get_bg (transport_table.get_state()));
	cr->set_source_rgb (bg.get_red_p(), bg.get_green_p(), bg.get_blue_p());
	cr->fill ();

	static const int xmargin = 2;
	static const int ymargin = 1;

	/* draw box around record-options */
	int xx, ww, hh, uu;

	punch_label.translate_coordinates (transport_table, -xmargin, 0, xx, uu); // left
	punch_out_button.translate_coordinates (transport_table, xmargin, 0, ww, uu); // right
	ww += punch_out_button.get_width () - xx; // width
	hh = transport_table.get_height() - 1;

	Gtkmm2ext::rounded_rectangle (cr->cobj(), x0 + xx - 0.5, y0 + 0.5, ww + 1, hh, 6);
	cr->set_source_rgb (0, 0, 0);
	cr->set_line_width (1.0);
	cr->stroke ();

	/* line to rec-enable */
	int rx;
	rec_button.translate_coordinates (transport_table, -xmargin, 0, rx, uu); // top
	int dx = rx + rec_button.get_width() - xx;

	cr->move_to (x0 + xx, 1.5 + y0 + ymargin + round (punch_in_button.get_height () * .5));
	cr->rel_line_to (dx, 0);
	cr->set_line_width (2.0);
	cr->stroke ();

	return false;
}

void
ARDOUR_UI::setup_transport ()
{
	RefPtr<Action> act;
	/* setup actions */

	act = ActionManager::get_action ("Transport", "ToggleClick");
	click_button.set_related_action (act);
	click_button.signal_button_press_event().connect (sigc::mem_fun (*this, &ARDOUR_UI::click_button_clicked), false);

	act = ActionManager::get_action (X_("Transport"), X_("Stop"));
	stop_button.set_related_action (act);
	act = ActionManager::get_action (X_("Transport"), X_("Roll"));
	roll_button.set_related_action (act);
	act = ActionManager::get_action (X_("Transport"), X_("Record"));
	rec_button.set_related_action (act);
	act = ActionManager::get_action (X_("Transport"), X_("GotoStart"));
	goto_start_button.set_related_action (act);
	act = ActionManager::get_action (X_("Transport"), X_("GotoEnd"));
	goto_end_button.set_related_action (act);
	act = ActionManager::get_action (X_("Transport"), X_("Loop"));
	auto_loop_button.set_related_action (act);
	act = ActionManager::get_action (X_("Transport"), X_("PlaySelection"));
	play_selection_button.set_related_action (act);
	act = ActionManager::get_action (X_("MIDI"), X_("panic"));
	midi_panic_button.set_related_action (act);
	act = ActionManager::get_action (X_("Transport"), X_("ToggleExternalSync"));

	sync_button.set_related_action (act);
	sync_button.signal_button_press_event().connect (sigc::mem_fun (*this, &ARDOUR_UI::sync_button_clicked), false);

	sync_button.set_sizing_text (S_("LogestSync|M-Clk"));

	/* CANNOT sigc::bind these to clicked or toggled, must use pressed or released */
	act = ActionManager::get_action (X_("Main"), X_("cancel-solo"));
	solo_alert_button.set_related_action (act);
	auditioning_alert_button.signal_clicked.connect (sigc::mem_fun(*this,&ARDOUR_UI::audition_alert_clicked));
	error_alert_button.signal_button_release_event().connect (sigc::mem_fun(*this,&ARDOUR_UI::error_alert_press), false);
	act = ActionManager::get_action (X_("Editor"), X_("toggle-log-window"));
	error_alert_button.set_related_action(act);
	error_alert_button.set_fallthrough_to_parent(true);

	layered_button.signal_clicked.connect (sigc::mem_fun(*this,&ARDOUR_UI::layered_button_clicked));

	editor_visibility_button.set_related_action (ActionManager::get_action (X_("Common"), X_("change-editor-visibility")));
	mixer_visibility_button.set_related_action (ActionManager::get_action (X_("Common"), X_("change-mixer-visibility")));
	prefs_visibility_button.set_related_action (ActionManager::get_action (X_("Common"), X_("change-preferences-visibility")));

	act = ActionManager::get_action ("Transport", "ToggleAutoReturn");
	auto_return_button.set_related_action (act);
	act = ActionManager::get_action (X_("Transport"), X_("ToggleFollowEdits"));
	follow_edits_button.set_related_action (act);
	act = ActionManager::get_action ("Transport", "ToggleAutoInput");
	auto_input_button.set_related_action (act);

	act = ActionManager::get_action ("Transport", "TogglePunchIn");
	punch_in_button.set_related_action (act);
	act = ActionManager::get_action ("Transport", "TogglePunchOut");
	punch_out_button.set_related_action (act);

	/* connect signals */
	ARDOUR_UI::Clock.connect (sigc::mem_fun (primary_clock, &AudioClock::set));
	ARDOUR_UI::Clock.connect (sigc::mem_fun (secondary_clock, &AudioClock::set));

	primary_clock->ValueChanged.connect (sigc::mem_fun(*this, &ARDOUR_UI::primary_clock_value_changed));
	secondary_clock->ValueChanged.connect (sigc::mem_fun(*this, &ARDOUR_UI::secondary_clock_value_changed));
	big_clock->ValueChanged.connect (sigc::mem_fun(*this, &ARDOUR_UI::big_clock_value_changed));

	editor_visibility_button.signal_drag_failed().connect (sigc::bind (sigc::ptr_fun (drag_failed), editor));
	mixer_visibility_button.signal_drag_failed().connect (sigc::bind (sigc::ptr_fun (drag_failed), mixer));
	prefs_visibility_button.signal_drag_failed().connect (sigc::bind (sigc::ptr_fun (drag_failed), rc_option_editor));

	/* catch context clicks so that we can show a menu on these buttons */

	editor_visibility_button.signal_button_press_event().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::tabbable_visibility_button_press), X_("editor")), false);
	mixer_visibility_button.signal_button_press_event().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::tabbable_visibility_button_press), X_("mixer")), false);
	prefs_visibility_button.signal_button_press_event().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::tabbable_visibility_button_press), X_("preferences")), false);

	/* setup widget style/name */

	auto_return_button.set_name ("transport option button");
	follow_edits_button.set_name ("transport option button");
	auto_input_button.set_name ("transport option button");

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

	editor_visibility_button.set_name (X_("page switch button"));
	mixer_visibility_button.set_name (X_("page switch button"));
	prefs_visibility_button.set_name (X_("page switch button"));

	punch_in_button.set_name ("punch button");
	punch_out_button.set_name ("punch button");
	layered_button.set_name (("layered button"));

	click_button.set_name ("transport button");
	sync_button.set_name ("transport active option button");

	/* and widget text */
	auto_return_button.set_text(_("Auto Return"));
	follow_edits_button.set_text(_("Follow Range"));
	//auto_input_button.set_text (_("Auto Input"));
	punch_in_button.set_text (_("In"));
	punch_out_button.set_text (_("Out"));
	layered_button.set_text (_("Non-Layered"));

	punch_label.set_text (_("Punch:"));
	layered_label.set_text (_("Rec:"));

	/* and tooltips */

	Gtkmm2ext::UI::instance()->set_tip (editor_visibility_button,
	                                    string_compose (_("Drag this tab to the desktop to show %1 in its own window\n\n"
	                                                      "To put the window back, use the Window > %1 > Attach menu action"), editor->name()));

	Gtkmm2ext::UI::instance()->set_tip (mixer_visibility_button,
	                                    string_compose (_("Drag this tab to the desktop to show %1 in its own window\n\n"
	                                                      "To put the window back, use the Window > %1 > Attach menu action"), mixer->name()));

	Gtkmm2ext::UI::instance()->set_tip (prefs_visibility_button,
	                                    string_compose (_("Drag this tab to the desktop to show %1 in its own window\n\n"
	                                                      "To put the window back, use the Window > %1 > Attach menu action"), rc_option_editor->name()));

	Gtkmm2ext::UI::instance()->set_tip (punch_in_button, _("Start recording at auto-punch start"));
	Gtkmm2ext::UI::instance()->set_tip (punch_out_button, _("Stop recording at auto-punch end"));

	/* setup icons */

	click_button.set_icon (ArdourIcon::TransportMetronom);
	goto_start_button.set_icon (ArdourIcon::TransportStart);
	goto_end_button.set_icon (ArdourIcon::TransportEnd);
	roll_button.set_icon (ArdourIcon::TransportPlay);
	stop_button.set_icon (ArdourIcon::TransportStop);
	play_selection_button.set_icon (ArdourIcon::TransportRange);
	auto_loop_button.set_icon (ArdourIcon::TransportLoop);
	rec_button.set_icon (ArdourIcon::RecButton);
	midi_panic_button.set_icon (ArdourIcon::TransportPanic);

	/* transport control size-group */

	Glib::RefPtr<SizeGroup> transport_button_size_group = SizeGroup::create (SIZE_GROUP_BOTH);
	transport_button_size_group->add_widget (goto_start_button);
	transport_button_size_group->add_widget (goto_end_button);
	transport_button_size_group->add_widget (auto_loop_button);
	transport_button_size_group->add_widget (rec_button);
	transport_button_size_group->add_widget (play_selection_button);
	transport_button_size_group->add_widget (roll_button);
	transport_button_size_group->add_widget (stop_button);

	Glib::RefPtr<SizeGroup> punch_button_size_group = SizeGroup::create (Gtk::SIZE_GROUP_HORIZONTAL);
	punch_button_size_group->add_widget (punch_in_button);
	punch_button_size_group->add_widget (punch_out_button);

	/* and now the layout... */

	/* top level packing */
	transport_table.set_spacings (0);
	transport_table.set_row_spacings (4);
	transport_table.set_border_width (2);
	transport_frame.add (transport_table);
	transport_frame.set_name ("TransportFrame");
	transport_frame.set_shadow_type (Gtk::SHADOW_NONE);

	transport_table.signal_expose_event().connect (sigc::mem_fun (*this, &ARDOUR_UI::transport_expose), false);

	/* transport controls sub-group */
	click_button.set_size_request (PX_SCALE(20), PX_SCALE(20));

	HBox* tbox = manage (new HBox);
	tbox->set_spacing (PX_SCALE(2));

	tbox->pack_start (midi_panic_button, true, true, 0);
	tbox->pack_start (click_button, true, true, 0);
	tbox->pack_start (goto_start_button, true, true);
	tbox->pack_start (goto_end_button, true, true);
	tbox->pack_start (auto_loop_button, true, true);
	tbox->pack_start (play_selection_button, true, true);

	tbox->pack_start (roll_button, true, true);
	tbox->pack_start (stop_button, true, true);
	tbox->pack_start (rec_button, true, true, 3);

	/* alert box sub-group */
	VBox* alert_box = manage (new VBox);
	alert_box->set_homogeneous (true);
	alert_box->set_spacing (1);
	alert_box->set_border_width (0);
	alert_box->pack_start (solo_alert_button, true, false, 0);
	alert_box->pack_start (auditioning_alert_button, true, false, 0);
	alert_box->pack_start (feedback_alert_button, true, false, 0);

	/* clock button size groups */
	Glib::RefPtr<SizeGroup> button_height_size_group = SizeGroup::create (Gtk::SIZE_GROUP_VERTICAL);
	button_height_size_group->add_widget (follow_edits_button);
	button_height_size_group->add_widget (*primary_clock->left_btn());
	button_height_size_group->add_widget (*primary_clock->right_btn());
	button_height_size_group->add_widget (*secondary_clock->left_btn());
	button_height_size_group->add_widget (*secondary_clock->right_btn());

	button_height_size_group->add_widget (stop_button);
//	button_height_size_group->add_widget (sync_button);
//	button_height_size_group->add_widget (layered_button);
	button_height_size_group->add_widget (auto_return_button);
	button_height_size_group->add_widget (editor_visibility_button);
	button_height_size_group->add_widget (mixer_visibility_button);

	Glib::RefPtr<SizeGroup> clock1_size_group = SizeGroup::create (SIZE_GROUP_BOTH);
	clock1_size_group->add_widget (*primary_clock->left_btn());
	clock1_size_group->add_widget (*primary_clock->right_btn());

	Glib::RefPtr<SizeGroup> clock2_size_group = SizeGroup::create (SIZE_GROUP_BOTH);
	clock2_size_group->add_widget (*secondary_clock->left_btn());
	clock2_size_group->add_widget (*secondary_clock->right_btn());

	/* sub-layout for Sync | Shuttle (grow) */
	HBox* ssbox = manage (new HBox);
	ssbox->set_spacing (PX_SCALE(2));
	ssbox->pack_start (sync_button, false, false, 0);
	ssbox->pack_start (shuttle_box, true, true, 0);
	ssbox->pack_start (*shuttle_box.info_button(), false, false, 0);


	/* and the main table layout */

	int col = 0;
#define TCOL col, col + 1

	transport_table.attach (*tbox, TCOL, 0, 1 , SHRINK, SHRINK, 0, 0);
	transport_table.attach (*ssbox, TCOL, 1, 2 , FILL, SHRINK, 0, 0);
	++col;

	transport_table.attach (*(manage (new ArdourVSpacer ())), TCOL, 0, 2 , SHRINK, EXPAND|FILL, 3, 0);
	++col;

	transport_table.attach (punch_label, TCOL, 0, 1 , FILL, SHRINK, 3, 0);
	transport_table.attach (layered_label, TCOL, 1, 2 , FILL, SHRINK, 3, 0);
	++col;

	transport_table.attach (punch_in_button,  col,     col + 1, 0, 1 , FILL, SHRINK, 0, 2);
	transport_table.attach (*(manage (new Label (""))), col + 1, col + 2, 0, 1 , FILL, SHRINK, 2, 2);
	transport_table.attach (punch_out_button, col + 2, col + 3, 0, 1 , FILL, SHRINK, 0, 2);
	transport_table.attach (layered_button,   col,     col + 3, 1, 2 , FILL, SHRINK, 0, 2);
	col += 3;

	transport_table.attach (*(manage (new ArdourVSpacer ())), TCOL, 0, 2 , SHRINK, EXPAND|FILL, 3, 0);
	++col;

	transport_table.attach (follow_edits_button, TCOL, 0, 1 , FILL, SHRINK, 2, 0);
	transport_table.attach (auto_return_button,  TCOL, 1, 2 , FILL, SHRINK, 2, 0);
	++col;

	transport_table.attach (*(manage (new ArdourVSpacer ())), TCOL, 0, 2 , SHRINK, EXPAND|FILL, 3, 0);
	++col;

	transport_table.attach (*primary_clock,              col,     col + 2, 0, 1 , FILL, SHRINK, 2, 0);
	transport_table.attach (*primary_clock->left_btn(),  col,     col + 1, 1, 2 , FILL, SHRINK, 2, 0);
	transport_table.attach (*primary_clock->right_btn(), col + 1, col + 2, 1, 2 , FILL, SHRINK, 2, 0);
	col += 2;

	transport_table.attach (*(manage (new ArdourVSpacer ())), TCOL, 0, 2 , SHRINK, EXPAND|FILL, 3, 0);
	++col;

	if (!ARDOUR::Profile->get_small_screen()) {
		transport_table.attach (*secondary_clock,              col,     col + 2, 0, 1 , FILL, SHRINK, 2, 0);
		transport_table.attach (*secondary_clock->left_btn(),  col,     col + 1, 1, 2 , FILL, SHRINK, 2, 0);
		transport_table.attach (*secondary_clock->right_btn(), col + 1, col + 2, 1, 2 , FILL, SHRINK, 2, 0);
		secondary_clock->set_no_show_all (true);
		secondary_clock->left_btn()->set_no_show_all (true);
		secondary_clock->right_btn()->set_no_show_all (true);
		col += 2;

		secondary_clock_spacer = manage (new ArdourVSpacer ());
		transport_table.attach (*secondary_clock_spacer, TCOL, 0, 2 , SHRINK, EXPAND|FILL, 3, 0);
		++col;
	}

	transport_table.attach (*alert_box, TCOL, 0, 2, SHRINK, EXPAND|FILL, 2, 0);
	++col;

	transport_table.attach (*(manage (new ArdourVSpacer ())), TCOL, 0, 2 , SHRINK, EXPAND|FILL, 3, 0);
	++col;

	/* editor-meter, mini-timeline and selection clock are options in the transport_hbox */
	transport_hbox.set_spacing (3);
	transport_table.attach (transport_hbox, TCOL, 0, 2, EXPAND|FILL, EXPAND|FILL, 2, 0);
	++col;

	/* lua script action buttons */
	transport_table.attach (action_script_table, TCOL, 0, 2, SHRINK, EXPAND|FILL, 1, 0);
	++col;

	transport_table.attach (editor_visibility_button, TCOL, 0, 1 , FILL, SHRINK, 2, 0);
	transport_table.attach (mixer_visibility_button,  TCOL, 1, 2 , FILL, SHRINK, 2, 0);
	++col;

	repack_transport_hbox ();
	update_clock_visibility ();
	/* desensitize */

	feedback_alert_button.set_sensitive (false);
	feedback_alert_button.set_visual_state (Gtkmm2ext::NoVisualState);
	auditioning_alert_button.set_sensitive (false);
	auditioning_alert_button.set_visual_state (Gtkmm2ext::NoVisualState);

	stop_button.set_active (true);
	set_transport_sensitivity (false);
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
	set_transport_sensitivity (!onoff);
}

void
ARDOUR_UI::auditioning_changed (bool onoff)
{
	UI::instance()->call_slot (MISSING_INVALIDATOR, boost::bind (&ARDOUR_UI::_auditioning_changed, this, onoff));
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
			RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("toggle-log-window"));
			Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
			if (tact && tact->get_active()) {
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
ARDOUR_UI::layered_button_clicked ()
{
	if (_session) {
		_session->config.set_layered_record_mode (!_session->config.get_layered_record_mode ());
	}
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
ARDOUR_UI::sync_blink (bool onoff)
{
	if (_session == 0 || !_session->config.get_external_sync()) {
		/* internal sync */
		sync_button.set_active (false);
		return;
	}

	if (!_session->transport_locked()) {
		/* not locked, so blink on and off according to the onoff argument */

		if (onoff) {
			sync_button.set_active (true);
		} else {
			sync_button.set_active (false);
		}
	} else {
		/* locked */
		sync_button.set_active (true);
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
		if (onoff) {
			feedback_alert_button.set_active (true);
		} else {
			feedback_alert_button.set_active (false);
		}
	} else {
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
ARDOUR_UI::set_transport_sensitivity (bool yn)
{
	ActionManager::set_sensitive (ActionManager::transport_sensitive_actions, yn);
	shuttle_box.set_sensitive (yn);
}

void
ARDOUR_UI::editor_realized ()
{
	boost::function<void (string)> pc (boost::bind (&ARDOUR_UI::parameter_changed, this, _1));
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

bool
ARDOUR_UI::sync_button_clicked (GdkEventButton* ev)
{
	if (ev->button != 3) {
		/* this handler is just for button-3 clicks */
		return false;
	}

	show_tabbable (rc_option_editor);
	rc_option_editor->set_current_page (_("Sync"));
	return true;
}

void
ARDOUR_UI::toggle_follow_edits ()
{
	RefPtr<Action> act = ActionManager::get_action (X_("Transport"), X_("ToggleFollowEdits"));
	assert (act);

	RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic (act);
	assert (tact);

	UIConfiguration::instance().set_follow_edits (tact->get_active ());
}

void
ARDOUR_UI::update_title ()
{
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
	} else {
		WindowTitle title (Glib::get_application_name());
		_main_window.set_title (title.get_string());
	}

}
