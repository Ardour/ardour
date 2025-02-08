/*
 * Copyright (C) 2023 Paul Davis <paul@linuxaudiosystems.com>
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

#include "pbd/stateful_diff_command.h"

#include "ardour/midi_region.h"
#include "ardour/midi_track.h"
#include "ardour/smf_source.h"

#include "canvas/box.h"
#include "canvas/canvas.h"
#include "canvas/container.h"
#include "canvas/debug.h"
#include "canvas/scroll_group.h"
#include "canvas/rectangle.h"
#include "canvas/widget.h"

#include "gtkmm2ext/actions.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_dropdown.h"
#include "widgets/metabutton.h"
#include "widgets/tooltips.h"

#include "ardour_ui.h"
#include "editor_cursors.h"
#include "editor_drag.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "midi_util.h"
#include "pianoroll_background.h"
#include "pianoroll.h"
#include "pianoroll_midi_view.h"
#include "note_base.h"
#include "prh.h"
#include "timers.h"
#include "ui_config.h"
#include "verbose_cursor.h"

#include "pbd/i18n.h"

using namespace PBD;
using namespace ARDOUR;
using namespace ArdourCanvas;
using namespace ArdourWidgets;
using namespace Gtkmm2ext;
using namespace Temporal;

Pianoroll::Pianoroll (std::string const & name)
	: CueEditor (name)
	, timebar_height (15.)
	, n_timebars (3)
	, prh (nullptr)
	, bg (nullptr)
	, view (nullptr)
	, bbt_metric (*this)
	, _note_mode (Sustained)
	, zoom_in_allocate (false)
	, bar_adjustment (4, 1, 32, 1, 4)
	, bar_spinner (bar_adjustment)
	, length_label (X_("Record (Bars):"))
{
	mouse_mode = Editing::MouseContent;
	autoscroll_vertical_allowed = false;

	build_grid_type_menu ();
	build_draw_midi_menus();

	build_upper_toolbar ();
	build_canvas ();
	build_lower_toolbar ();

	load_bindings ();
	register_actions ();

	/* this is weird because it will use an action that uses current_editing_context */
	EditingContext* ec = current_editing_context ();
	switch_editing_context (this);
	set_mouse_mode (Editing::MouseContent, true);
	switch_editing_context (ec);
}

Pianoroll::~Pianoroll ()
{
	delete bindings;
	ActionManager::drop_action_group (editor_actions);
	ActionManager::drop_action_group (snap_actions);
}

bool
Pianoroll::enter (GdkEventCrossing*)
{
	switch_editing_context (this);
	return false;
}

void
Pianoroll::load_bindings ()
{
	bindings = Bindings::get_bindings (editor_name());
	load_shared_bindings ();
}

void
Pianoroll::register_actions ()
{
	editor_actions = ActionManager::create_action_group (bindings, editor_name());

	bind_mouse_mode_buttons ();
	register_grid_actions ();
}

ArdourCanvas::GtkCanvasViewport*
Pianoroll::get_canvas_viewport() const
{
	return _canvas_viewport;
}

ArdourCanvas::GtkCanvas*
Pianoroll::get_canvas() const
{
	return _canvas;
}

bool
Pianoroll::canvas_pre_event (GdkEvent* ev)
{
	switch (ev->type) {
	case GDK_ENTER_NOTIFY:
	case GDK_LEAVE_NOTIFY:
		if (canvas_enter_leave (&ev->crossing)) {
			return true;
		}
		break;
	default:
		break;
	}

	return false;
}

void
Pianoroll::rebuild_parameter_button_map()
{
	parameter_button_map.clear ();
	parameter_button_map.insert (std::make_pair (velocity_button, Evoral::Parameter (ARDOUR::MidiVelocityAutomation, _visible_channel)));
	parameter_button_map.insert (std::make_pair (bender_button, Evoral::Parameter (ARDOUR::MidiPitchBenderAutomation, _visible_channel)));
	parameter_button_map.insert (std::make_pair (pressure_button, Evoral::Parameter (ARDOUR::MidiChannelPressureAutomation, _visible_channel)));
	parameter_button_map.insert (std::make_pair (expression_button, Evoral::Parameter (ARDOUR::MidiCCAutomation, _visible_channel, MIDI_CTL_MSB_EXPRESSION)));
	parameter_button_map.insert (std::make_pair (modulation_button, Evoral::Parameter (ARDOUR::MidiCCAutomation, _visible_channel, MIDI_CTL_MSB_MODWHEEL)));

	parameter_button_map.insert (std::make_pair (cc_dropdown1, Evoral::Parameter (ARDOUR::MidiCCAutomation, _visible_channel, MIDI_CTL_MSB_GENERAL_PURPOSE1)));
	parameter_button_map.insert (std::make_pair (cc_dropdown2, Evoral::Parameter (ARDOUR::MidiCCAutomation, _visible_channel, MIDI_CTL_MSB_GENERAL_PURPOSE2)));
	parameter_button_map.insert (std::make_pair (cc_dropdown3, Evoral::Parameter (ARDOUR::MidiCCAutomation, _visible_channel, MIDI_CTL_MSB_GENERAL_PURPOSE3)));
}

void
Pianoroll::reset_user_cc_choice (std::string name, Evoral::Parameter param, MetaButton* metabutton)
{
	ParameterButtonMap::iterator iter;

	for (iter = parameter_button_map.begin(); iter != parameter_button_map.end(); ++iter) {
		if (iter->first == metabutton) {
			parameter_button_map.erase (iter);
			break;
		}
	}

	parameter_button_map.insert (std::make_pair (metabutton, param));

	metabutton->set_by_menutext (name);
}

void
Pianoroll::add_single_controller_item (Gtk::Menu_Helpers::MenuList& ctl_items,
                                       int                     ctl,
                                       const std::string&      name,
                                       ArdourWidgets::MetaButton* mb)
{
	using namespace Gtk::Menu_Helpers;

	const uint16_t selected_channels = 0xffff;
	for (uint8_t chn = 0; chn < 16; chn++) {

		if (selected_channels & (0x0001 << chn)) {

			Evoral::Parameter fully_qualified_param (MidiCCAutomation, chn, ctl);
			std::string menu_text (string_compose ("<b>%1</b>: %2 [%3]", ctl, name, int (chn + 1)));

			mb->add_item (name, menu_text, sigc::bind (sigc::mem_fun (*this, &Pianoroll::reset_user_cc_choice), name, fully_qualified_param, mb));

			/* one channel only */
			break;
		}
	}
}

static void relax() {}

void
Pianoroll::add_multi_controller_item (Gtk::Menu_Helpers::MenuList& ctl_items,
                                      const uint16_t          channels,
                                      int                     ctl,
                                      const std::string&      name,
                                      MetaButton* mb)
{
	using namespace Gtk;
	using namespace Gtk::Menu_Helpers;

	Menu* chn_menu = manage (new Menu);
	MenuList& chn_items (chn_menu->items());
	std::string menu_text (string_compose ("%1: %2", ctl, name));

	/* Build the channel sub-menu */

	Evoral::Parameter param_without_channel (MidiCCAutomation, 0, ctl);

	for (uint8_t chn = 0; chn < 16; chn++) {
		if (channels & (0x0001 << chn)) {

			/* for each selected channel, add a menu item for this controller */

			Evoral::Parameter fully_qualified_param (MidiCCAutomation, chn, ctl);

			chn_items.push_back (CheckMenuElem (string_compose (_("Channel %1"), chn+1),
			                                    sigc::bind (sigc::mem_fun (*this, &Pianoroll::reset_user_cc_choice),  menu_text, fully_qualified_param, mb)));

			ParameterButtonMap::iterator i = parameter_button_map.find (mb);

			if (i != parameter_button_map.end()) {
				if (fully_qualified_param == i->second) {
					Gtk::CheckMenuItem* cmi = static_cast<Gtk::CheckMenuItem*>(&ctl_items.back());
					cmi->set_active();
				}
			}
		}
	}

	/* add an item to metabutton's menu that will connect to the
	 * per-channel submenu we built above.
	 */

	mb->add_item (name, menu_text, *chn_menu, sigc::ptr_fun (relax));
}


void
Pianoroll::build_lower_toolbar ()
{
	ArdourButton::Element elements = ArdourButton::Element (ArdourButton::Text|ArdourButton::Indicator|ArdourButton::Edge|ArdourButton::Body);

	velocity_button = new ArdourButton (_("Velocity"), elements);
	bender_button = new ArdourButton (_("Bender"), elements);
	pressure_button = new ArdourButton (_("Pressure"), elements);
	expression_button = new ArdourButton (_("Expression"), elements);
	modulation_button = new ArdourButton (_("Modulation"), elements);
	cc_dropdown1 = new MetaButton ();
	cc_dropdown2 = new MetaButton ();
	cc_dropdown3 = new MetaButton ();

	cc_dropdown1->add_elements (ArdourButton::Indicator);
	cc_dropdown2->add_elements (ArdourButton::Indicator);
	cc_dropdown3->add_elements (ArdourButton::Indicator);

	cc_dropdown1->signal_led_clicked.connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::user_led_click), cc_dropdown1));
	cc_dropdown2->signal_led_clicked.connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::user_led_click), cc_dropdown2));
	cc_dropdown3->signal_led_clicked.connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::user_led_click), cc_dropdown3));

	rebuild_parameter_button_map ();

	/* Only need to do this once because i->first is the actual button,
	 * which does not change even when the parameter_button_map is rebuilt.
	 */

	for (ParameterButtonMap::iterator i = parameter_button_map.begin(); i != parameter_button_map.end(); ++i) {
		i->first->set_active_color (0xff0000ff);
		i->first->set_distinct_led_click (true);
		i->first->set_led_left (true);
		i->first->set_act_on_release (false);
		i->first->set_fallthrough_to_parent (true);
	}

	// button_bar.set_homogeneous (true);
	button_bar.set_spacing (6);
	button_bar.set_border_width (6);
	button_bar.pack_start (*velocity_button, false, false);
	button_bar.pack_start (*bender_button, false, false);
	button_bar.pack_start (*pressure_button, false, false);
	button_bar.pack_start (*modulation_button, false, false);
	button_bar.pack_start (*cc_dropdown1, false, false);
	button_bar.pack_start (*cc_dropdown2, false, false);
	button_bar.pack_start (*cc_dropdown3, false, false);

	velocity_button->signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::automation_button_event), ARDOUR::MidiVelocityAutomation, 0));
	pressure_button->signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::automation_button_event), ARDOUR::MidiChannelPressureAutomation, 0));
	bender_button->signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::automation_button_event), ARDOUR::MidiPitchBenderAutomation, 0));
	modulation_button->signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::automation_button_event), ARDOUR::MidiCCAutomation, MIDI_CTL_MSB_MODWHEEL));
	expression_button->signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::automation_button_event), ARDOUR::MidiCCAutomation, MIDI_CTL_MSB_EXPRESSION));

	cc_dropdown1->signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::user_automation_button_event), cc_dropdown1), false);
	cc_dropdown2->signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::user_automation_button_event), cc_dropdown2), false);
	cc_dropdown3->signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::user_automation_button_event), cc_dropdown3), false);

	velocity_button->signal_led_clicked.connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::automation_led_click), ARDOUR::MidiVelocityAutomation, 0));
	pressure_button->signal_led_clicked.connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::automation_led_click), ARDOUR::MidiChannelPressureAutomation, 0));
	bender_button->signal_led_clicked.connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::automation_led_click), ARDOUR::MidiPitchBenderAutomation, 0));
	modulation_button->signal_led_clicked.connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::automation_led_click), ARDOUR::MidiCCAutomation, MIDI_CTL_MSB_MODWHEEL));
	expression_button->signal_led_clicked.connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::automation_led_click), ARDOUR::MidiCCAutomation, MIDI_CTL_MSB_EXPRESSION));

	_toolbox.pack_start (button_bar, false, false);
}

void
Pianoroll::build_upper_toolbar ()
{
	using namespace Gtk::Menu_Helpers;

	Gtk::HBox* mode_box = manage(new Gtk::HBox);
	mode_box->set_border_width (2);
	mode_box->set_spacing(2);

	Gtk::HBox* mouse_mode_box = manage (new Gtk::HBox);
	Gtk::HBox* mouse_mode_hbox = manage (new Gtk::HBox);
	Gtk::VBox* mouse_mode_vbox = manage (new Gtk::VBox);
	Gtk::Alignment* mouse_mode_align = manage (new Gtk::Alignment);

	Glib::RefPtr<Gtk::SizeGroup> mouse_mode_size_group = Gtk::SizeGroup::create (Gtk::SIZE_GROUP_VERTICAL);
	mouse_mode_size_group->add_widget (mouse_draw_button);
	mouse_mode_size_group->add_widget (mouse_content_button);

	mouse_mode_size_group->add_widget (grid_type_selector);
	mouse_mode_size_group->add_widget (draw_length_selector);
	mouse_mode_size_group->add_widget (draw_velocity_selector);
	mouse_mode_size_group->add_widget (draw_channel_selector);
	mouse_mode_size_group->add_widget (snap_mode_button);

	mouse_mode_hbox->set_spacing (2);
	mouse_mode_hbox->pack_start (mouse_draw_button, false, false);
	mouse_mode_hbox->pack_start (mouse_content_button, false, false);

	mouse_mode_vbox->pack_start (*mouse_mode_hbox);

	mouse_mode_align->add (*mouse_mode_vbox);
	mouse_mode_align->set (0.5, 1.0, 0.0, 0.0);

	mouse_mode_box->pack_start (*mouse_mode_align, false, false);

	pack_snap_box ();
	pack_draw_box ();

	Gtk::HBox* _toolbar_inner = manage (new Gtk::HBox);
	Gtk::HBox* _toolbar_outer = manage (new Gtk::HBox);
	Gtk::HBox* _toolbar_left = manage (new Gtk::HBox);

	_toolbar_inner->pack_start (*mouse_mode_box, false, false);
	_toolbar_inner->pack_start (snap_box, false, false);
	_toolbar_inner->pack_start (grid_box, false, false);
	_toolbar_inner->pack_start (draw_box, false, false);

	set_tooltip (full_zoom_button, _("Zoom to full clip"));
	set_tooltip (note_mode_button, _("Toggle between drum and regular note drawing"));
	note_mode_button.set_icon (ArdourIcon::Drum);

	play_note_selection_button.set_icon (ArdourIcon::ToolAudition);

#define PX_SCALE(px) std::max((float)px, rintf((float)px * UIConfiguration::instance().get_ui_scale()))
	note_mode_button.set_size_request (PX_SCALE(50), -1);
	note_mode_button.set_active_color (UIConfiguration::instance().color ("alert:yellow"));

	rec_enable_button.set_icon (ArdourIcon::RecButton);
	rec_enable_button.set_sensitive (false);
	rec_enable_button.signal_button_release_event().connect (sigc::mem_fun (*this, &Pianoroll::rec_button_press), false);

	rec_box.set_spacing (12);
	rec_box.pack_start (rec_enable_button, false, false);
	rec_box.pack_start (length_label, false, false);
	rec_box.pack_start (bar_spinner, false, false);
	rec_enable_button.show();
	length_label.show ();
	bar_spinner.show ();
	rec_box.set_no_show_all (true);
	/* rec box not shown */

	_toolbar_outer->set_border_width (6);
	_toolbar_outer->set_spacing (12);
	_toolbar_outer->pack_start (rec_box, false, false);
	_toolbar_outer->pack_start (visible_channel_label, false, false);
	_toolbar_outer->pack_start (visible_channel_selector, false, false);
	_toolbar_outer->pack_start (play_note_selection_button, false, false);
	_toolbar_outer->pack_start (note_mode_button, false, false);
	_toolbar_outer->pack_start (follow_playhead_button, false, false);
	_toolbar_outer->pack_start (*_toolbar_inner, true, false);

	build_zoom_focus_menu ();
	zoom_focus_selector.set_text (zoom_focus_strings[(int)_zoom_focus]);

	_toolbar_left->pack_start (zoom_in_button, false, false);
	_toolbar_left->pack_start (zoom_out_button, false, false);
	_toolbar_left->pack_start (full_zoom_button, false, false);
	_toolbar_left->pack_start (zoom_focus_selector, false, false);

	_toolbar_outer->pack_start (*_toolbar_left, true, false);
	_toolbox.pack_start (*_toolbar_outer, false, false);

	Bindings* pr_bindings = Bindings::get_bindings (X_("Pianoroll"));
	_toolbox.set_data (X_("ardour-bindings"), pr_bindings);

	_contents.add (_toolbox);
	_contents.signal_enter_notify_event().connect (sigc::mem_fun (*this, &Pianoroll::enter), false);
}

void
Pianoroll::set_visible_channel (int n)
{
	_visible_channel = n;
	visible_channel_selector.set_active (string_compose ("%1", _visible_channel + 1));

	rebuild_parameter_button_map ();

	if (view) {
		view->swap_automation_channel (n);
	}
}

void
Pianoroll::build_canvas ()
{
	_canvas_viewport = new ArdourCanvas::GtkCanvasViewport (horizontal_adjustment, vertical_adjustment);

	_canvas = _canvas_viewport->canvas ();
	_canvas->set_background_color (UIConfiguration::instance().color ("arrange base"));
	_canvas->signal_event().connect (sigc::mem_fun (*this, &Pianoroll::canvas_pre_event), false);
	dynamic_cast<ArdourCanvas::GtkCanvas*>(_canvas)->use_nsglview (UIConfiguration::instance().get_nsgl_view_mode () == NSGLHiRes);

	_canvas->PreRender.connect (sigc::mem_fun(*this, &EditingContext::pre_render));

	/* scroll group for items that should not automatically scroll
	 *  (e.g verbose cursor). It shares the canvas coordinate space.
	*/
	no_scroll_group = new ArdourCanvas::Container (_canvas->root());

	h_scroll_group = new ArdourCanvas::ScrollGroup (_canvas->root(), ArdourCanvas::ScrollGroup::ScrollsHorizontally);
	CANVAS_DEBUG_NAME (h_scroll_group, "canvas h scroll");
	_canvas->add_scroller (*h_scroll_group);


	v_scroll_group = new ArdourCanvas::ScrollGroup (_canvas->root(), ArdourCanvas::ScrollGroup::ScrollsVertically);
	CANVAS_DEBUG_NAME (v_scroll_group, "canvas v scroll");
	_canvas->add_scroller (*v_scroll_group);

	hv_scroll_group = new ArdourCanvas::ScrollGroup (_canvas->root(),
	                                                 ArdourCanvas::ScrollGroup::ScrollSensitivity (ArdourCanvas::ScrollGroup::ScrollsVertically|
		                ArdourCanvas::ScrollGroup::ScrollsHorizontally));
	CANVAS_DEBUG_NAME (hv_scroll_group, "cue canvas hv scroll");
	_canvas->add_scroller (*hv_scroll_group);

	cursor_scroll_group = new ArdourCanvas::ScrollGroup (_canvas->root(), ArdourCanvas::ScrollGroup::ScrollsHorizontally);
	CANVAS_DEBUG_NAME (cursor_scroll_group, "cue canvas cursor scroll");
	_canvas->add_scroller (*cursor_scroll_group);

	/*a group to hold global rects like punch/loop indicators */
	global_rect_group = new ArdourCanvas::Container (hv_scroll_group);
	CANVAS_DEBUG_NAME (global_rect_group, "cue global rect group");

        transport_loop_range_rect = new ArdourCanvas::Rectangle (global_rect_group, ArdourCanvas::Rect (0.0, 0.0, 0.0, ArdourCanvas::COORD_MAX));
	CANVAS_DEBUG_NAME (transport_loop_range_rect, "cue loop rect");
	transport_loop_range_rect->hide();

	/*a group to hold time (measure) lines */
	time_line_group = new ArdourCanvas::Container (h_scroll_group);
	CANVAS_DEBUG_NAME (time_line_group, "cue  time line group");

	meter_bar = new ArdourCanvas::Rectangle (time_line_group, ArdourCanvas::Rect (0., 0, ArdourCanvas::COORD_MAX, timebar_height));
	CANVAS_DEBUG_NAME (meter_bar, "Meter Bar");
	meter_bar->set_fill(true);
	meter_bar->set_outline(true);
	meter_bar->set_outline_what(ArdourCanvas::Rectangle::BOTTOM);
	meter_bar->set_fill_color (UIConfiguration::instance().color_mod ("meter bar", "marker bar"));
	meter_bar->set_outline_color (UIConfiguration::instance().color ("marker bar separator"));
	meter_bar->set_outline_what (ArdourCanvas::Rectangle::BOTTOM);

	tempo_bar = new ArdourCanvas::Rectangle (time_line_group, ArdourCanvas::Rect (0.0, timebar_height, ArdourCanvas::COORD_MAX, timebar_height * 2));
	CANVAS_DEBUG_NAME (tempo_bar, "Tempo Bar");
	tempo_bar->set_fill(true);
	tempo_bar->set_outline(true);
	tempo_bar->set_outline_what(ArdourCanvas::Rectangle::BOTTOM);
	tempo_bar->set_fill_color (UIConfiguration::instance().color_mod ("tempo bar", "marker bar"));
	tempo_bar->set_outline_color (UIConfiguration::instance().color ("marker bar separator"));
	meter_bar->set_outline_what (ArdourCanvas::Rectangle::BOTTOM);

	Pango::FontDescription font (UIConfiguration::instance().get_SmallerFont());
	Pango::FontDescription larger_font (UIConfiguration::instance().get_SmallBoldFont());
	bbt_ruler = new ArdourCanvas::Ruler (time_line_group, &bbt_metric, ArdourCanvas::Rect (0, timebar_height * 2, ArdourCanvas::COORD_MAX, timebar_height * 3));
	bbt_ruler->set_font_description (font);
	Gtkmm2ext::Color base = UIConfiguration::instance().color ("ruler base");
	Gtkmm2ext::Color text = UIConfiguration::instance().color ("ruler text");
	bbt_ruler->set_fill_color (base);
	bbt_ruler->set_outline_color (text);
	CANVAS_DEBUG_NAME (bbt_ruler, "cue bbt ruler");


	data_group = new ArdourCanvas::Container (hv_scroll_group);
	CANVAS_DEBUG_NAME (data_group, "cue data group");

	bg = new CueMidiBackground (data_group);
	_canvas_viewport->signal_size_allocate().connect (sigc::mem_fun(*this, &Pianoroll::canvas_allocate), false);

	// used as rubberband rect
	rubberband_rect = new ArdourCanvas::Rectangle (data_group, ArdourCanvas::Rect (0.0, 0.0, 0.0, 0.0));
	rubberband_rect->hide();
	rubberband_rect->set_outline_color (UIConfiguration::instance().color ("rubber band rect"));
	rubberband_rect->set_fill_color (UIConfiguration::instance().color_mod ("rubber band rect", "selection rect"));
	CANVAS_DEBUG_NAME (rubberband_rect, X_("cue rubberband rect"));

	prh = new ArdourCanvas::PianoRollHeader (v_scroll_group, *bg);

	view = new PianorollMidiView (nullptr, *data_group, *no_scroll_group, *this, *bg, 0xff0000ff);
	view->AutomationStateChange.connect (sigc::mem_fun (*this, &Pianoroll::automation_state_changed));

	bg->set_view (view);
	prh->set_view (view);

	/* This must be called after prh and bg have had their view set */

	double w, h;
	prh->size_request (w, h);

	_timeline_origin = w;

	prh->set_position (Duple (0., n_timebars * timebar_height));
	data_group->set_position (ArdourCanvas::Duple (_timeline_origin, timebar_height * n_timebars));
	no_scroll_group->set_position (ArdourCanvas::Duple (_timeline_origin, timebar_height * n_timebars));
	cursor_scroll_group->set_position (ArdourCanvas::Duple (_timeline_origin, timebar_height * n_timebars));
	h_scroll_group->set_position (Duple (_timeline_origin, 0.));

	_verbose_cursor = new VerboseCursor (*this);

	// _playhead_cursor = new EditorCursor (*this, &Editor::canvas_playhead_cursor_event, X_("playhead"));
	_playhead_cursor = new EditorCursor (*this, X_("playhead"));
	_playhead_cursor->set_sensitive (UIConfiguration::instance().get_sensitize_playhead());
	_playhead_cursor->set_color (UIConfiguration::instance().color ("play head"));
	_playhead_cursor->canvas_item().raise_to_top();
	h_scroll_group->raise_to_top ();

	_canvas->set_name ("MidiCueCanvas");
	_canvas->add_events (Gdk::POINTER_MOTION_HINT_MASK | Gdk::SCROLL_MASK | Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);
	_canvas->set_can_focus ();

	_toolbox.pack_start (*_canvas_viewport, true, true);

	bindings_changed ();
}

void
Pianoroll::bindings_changed ()
{
	Bindings* midi_bindings = Bindings::get_bindings (X_("MIDI"));
	Bindings* shared_bindings = Bindings::get_bindings (X_("Editing"));

	_canvas_viewport->set_data (X_("ardour-bindings"), shared_bindings);
	_canvas->set_data (X_("ardour-bindings"), midi_bindings);
}

void
Pianoroll::maybe_update ()
{
	if (!_track) {
		return;
	}

	ARDOUR::TriggerPtr trigger = _track->triggerbox()->currently_playing ();

	if (!trigger) {
		_playhead_cursor->set_position (0);
	} else {
		if (trigger->active ()) {
			_playhead_cursor->set_position (trigger->current_pos().samples() + trigger->the_region()->start().samples());
		} else {
			_playhead_cursor->set_position (0);
		}
	}
}

bool
Pianoroll::canvas_enter_leave (GdkEventCrossing* ev)
{
	switch (ev->type) {
	case GDK_ENTER_NOTIFY:
		if (ev->detail != GDK_NOTIFY_INFERIOR) {
			_canvas_viewport->canvas()->grab_focus ();
			ActionManager::set_sensitive (_midi_actions, true);
			within_track_canvas = true;
			EditingContext::switch_editing_context (this);
		}
		break;
	case GDK_LEAVE_NOTIFY:
		if (ev->detail != GDK_NOTIFY_INFERIOR) {
			ActionManager::set_sensitive (_midi_actions, false);
			within_track_canvas = false;
			ARDOUR_UI::instance()->reset_focus (_canvas_viewport);
			gdk_window_set_cursor (_canvas_viewport->get_window()->gobj(), nullptr);
		}
	default:
		break;
	}
	return false;
}

void
Pianoroll::canvas_allocate (Gtk::Allocation alloc)
{
	_visible_canvas_width = alloc.get_width();
	_visible_canvas_height = alloc.get_height();

	double timebars = n_timebars * timebar_height;
	bg->set_size (alloc.get_width(), alloc.get_height() - timebars);
	view->set_height (alloc.get_height() - timebars);
	prh->set (ArdourCanvas::Rect (0, 0, prh->x1(), view->midi_context().height()));

	_track_canvas_width = _visible_canvas_width - prh->x1();

	if (zoom_in_allocate) {
		zoom_to_show (timecnt_t (timepos_t (max_extents_scale() * max_zoom_extent ().second.samples())));
		zoom_in_allocate = false;
	}

	update_grid ();
}

timepos_t
Pianoroll::snap_to_grid (timepos_t const & presnap, Temporal::RoundMode direction, SnapPref gpref) const
{
	/* BBT time only */
	return snap_to_bbt (presnap, direction, gpref);
}

void
Pianoroll::snap_to_internal (timepos_t& start, Temporal::RoundMode direction, SnapPref pref, bool ensure_snap) const
{
	UIConfiguration const& uic (UIConfiguration::instance ());
	const timepos_t presnap = start;


	timepos_t dist = timepos_t::max (start.time_domain()); // this records the distance of the best snap result we've found so far
	timepos_t best = timepos_t::max (start.time_domain()); // this records the best snap-result we've found so far

	timepos_t pre (presnap);
	timepos_t post (snap_to_grid (pre, direction, pref));

	check_best_snap (presnap, post, dist, best);

	if (timepos_t::max (start.time_domain()) == best) {
		return;
	}

	/* now check "magnetic" state: is the grid within reasonable on-screen distance to trigger a snap?
	 * this also helps to avoid snapping to somewhere the user can't see.  (i.e.: I clicked on a region and it disappeared!!)
	 * ToDo: Perhaps this should only occur if EditPointMouse?
	 */
	samplecnt_t snap_threshold_s = pixel_to_sample (uic.get_snap_threshold ());

	if (!ensure_snap && ::llabs (best.distance (presnap).samples()) > snap_threshold_s) {
		return;
	}

	start = best;
}

void
Pianoroll::set_samples_per_pixel (samplecnt_t spp)
{
	CueEditor::set_samples_per_pixel (spp);

	if (view) {
		view->set_samples_per_pixel (spp);
	}

	bbt_ruler->set_range (0, current_page_samples());
	compute_bbt_ruler_scale (0, current_page_samples());
	bbt_metric.units_per_pixel = spp;
}

samplecnt_t
Pianoroll::current_page_samples() const
{
	return (samplecnt_t) _track_canvas_width * samples_per_pixel;
}

bool
Pianoroll::canvas_bg_event (GdkEvent* event, ArdourCanvas::Item* item)
{
	return typed_event (item, event, RegionItem);
}

bool
Pianoroll::canvas_control_point_event (GdkEvent* event, ArdourCanvas::Item* item, ControlPoint* cp)
{
	return typed_event (item, event, ControlPointItem);
}

bool
Pianoroll::canvas_note_event (GdkEvent* event, ArdourCanvas::Item* item)
{
	return typed_event (item, event, NoteItem);
}

bool
Pianoroll::canvas_velocity_base_event (GdkEvent* event, ArdourCanvas::Item* item)
{
	return typed_event (item, event, VelocityBaseItem);
}

bool
Pianoroll::canvas_velocity_event (GdkEvent* event, ArdourCanvas::Item* item)
{
	return typed_event (item, event, VelocityItem);
}

bool
Pianoroll::canvas_cue_start_event (GdkEvent* event, ArdourCanvas::Item* item)
{
	return typed_event (item, event, ClipStartItem);
}

bool
Pianoroll::canvas_cue_end_event (GdkEvent* event, ArdourCanvas::Item* item)
{
	return typed_event (item, event, ClipEndItem);
}

void
Pianoroll::set_trigger_start (Temporal::timepos_t const & p)
{
	if (ref.trigger()) {
		ref.trigger()->the_region()->trim_front (p);
	} else {
		begin_reversible_command (_("trim region front"));
		view->midi_region()->clear_changes ();
		view->midi_region()->trim_front (view->midi_region()->position() + p);
		add_command (new StatefulDiffCommand (view->midi_region()));
		commit_reversible_command ();
	}
}

void
Pianoroll::set_trigger_end (Temporal::timepos_t const & p)
{
	if (ref.trigger()) {
		ref.trigger()->the_region()->trim_end (p);
	} else {
		begin_reversible_command (_("trim region end"));
		view->midi_region()->clear_changes ();
		view->midi_region()->trim_end (view->midi_region()->position() + p);
		add_command (new StatefulDiffCommand (view->midi_region()));
		commit_reversible_command ();
	}
}

Gtk::Widget&
Pianoroll::viewport()
{
	return *_canvas_viewport;
}

Gtk::Widget&
Pianoroll::contents ()
{
	return _contents;
}

void
Pianoroll::data_captured (samplecnt_t total_duration)
{
	data_capture_duration = total_duration;

	if (!idle_update_queued.exchange (1)) {
		Glib::signal_idle().connect (sigc::mem_fun (*this, &Pianoroll::idle_data_captured));
	}

	_playhead_cursor->set_position (data_capture_duration);
}

bool
Pianoroll::idle_data_captured ()
{
	double where = sample_to_pixel_unrounded (data_capture_duration);

	if (where > _visible_canvas_width * 0.80) {
		set_samples_per_pixel (samples_per_pixel * 1.5);
	}

	if (view) {
		view->clip_data_recorded (data_capture_duration);
	}
	idle_update_queued.store (0);
	return false;
}

void
Pianoroll::box_rec_enable_change (ARDOUR::TriggerBox const & b)
{
}

void
Pianoroll::trigger_rec_enable_change (ARDOUR::Trigger const & t)
{
	if (!t.armed()) {
		view->end_write ();
	}
}

bool
Pianoroll::button_press_handler (ArdourCanvas::Item* item, GdkEvent* event, ItemType item_type)
{
	switch (event->button.button) {
	case 1:
		return button_press_handler_1 (item, event, item_type);
		break;

	case 2:
		return button_press_handler_2 (item, event, item_type);
		break;

	case 3:
		break;

	default:
		return button_press_dispatch (&event->button);
		break;

	}

	return false;
}

bool
Pianoroll::button_press_handler_1 (ArdourCanvas::Item* item, GdkEvent* event, ItemType item_type)
{
	NoteBase* note = nullptr;

	switch (item_type) {
	case NoteItem:
		if (mouse_mode == Editing::MouseContent) {
			/* Existing note: allow trimming/motion */
			if ((note = reinterpret_cast<NoteBase*> (item->get_data ("notebase")))) {
				if (note->big_enough_to_trim() && note->mouse_near_ends()) {
					_drags->set (new NoteResizeDrag (*this, item), event, get_canvas_cursor());
				} else {
					NoteDrag* nd = new NoteDrag (*this, item);
					nd->set_bounding_item (data_group);
					_drags->set (nd, event);
				}
			}
		}
		return true;

	case ControlPointItem:
		if (mouse_mode == Editing::MouseContent) {
			_drags->set (new ControlPointDrag (*this, item), event);
		}
		return true;
		break;

	case VelocityItem:
		/* mouse mode independent - always allow drags */
		_drags->set (new LollipopDrag (*this, item), event);
		return true;
		break;

	case VelocityBaseItem:
		switch (mouse_mode) {
		case Editing::MouseContent:
			/* rubberband drag to select notes */
			_drags->set (new RubberbandSelectDrag (*this, item, [&](GdkEvent* ev, timepos_t const & pos) { return view->velocity_rb_click (ev, pos); }), event);
			break;
		case Editing::MouseDraw:
			_drags->set (new VelocityLineDrag (*this, *static_cast<ArdourCanvas::Rectangle*>(item), false, Temporal::BeatTime), event);
			break;
		default:
			break;
		}
		return true;
		break;

	case AutomationTrackItem:
		switch (mouse_mode) {
		case Editing::MouseContent:
			/* rubberband drag to select automation points */
			_drags->set (new RubberbandSelectDrag (*this, item, [&](GdkEvent* ev, timepos_t const & pos) { return view->automation_rb_click (ev, pos); }), event);
			break;
		case Editing::MouseDraw:
			_drags->set (new AutomationDrawDrag (*this, nullptr, *static_cast<ArdourCanvas::Rectangle*>(item), false, Temporal::BeatTime), event);
			break;
		default:
			break;
		}
		return true;
		break;

	case EditorAutomationLineItem: {
		ARDOUR::SelectionOperation op = ArdourKeyboard::selection_type (event->button.state);
		select_automation_line (&event->button, item, op);
		switch (mouse_mode) {
		case Editing::MouseContent:
			_drags->set (new LineDrag (*this, item, [&](GdkEvent* ev,timepos_t const & pos, double) { view->line_drag_click (ev, pos); }), event);
			break;
		default:
			break;
		}
		return true;
	}

	case ClipStartItem: {
		ArdourCanvas::Rectangle* r = dynamic_cast<ArdourCanvas::Rectangle*> (item);
		if (r) {
			_drags->set (new ClipStartDrag (*this, *r, *this), event);
		}
		return true;
		break;
	}

	case ClipEndItem: {
		ArdourCanvas::Rectangle* r = dynamic_cast<ArdourCanvas::Rectangle*> (item);
		if (r) {
			_drags->set (new ClipEndDrag (*this, *r, *this), event);
		}
		return true;
		break;
	}

	default:
		break;
	}

	return false;
}

bool
Pianoroll::button_press_handler_2 (ArdourCanvas::Item*, GdkEvent*, ItemType)
{
	return true;
}

bool
Pianoroll::button_release_handler (ArdourCanvas::Item* item, GdkEvent* event, ItemType item_type)
{
	if (!Keyboard::is_context_menu_event (&event->button)) {

		/* see if we're finishing a drag */

		if (_drags->active ()) {
			bool const r = _drags->end_grab (event);
			if (r) {
				/* grab dragged, so do nothing else */
				return true;
			}
		}
	}

	if (Keyboard::is_context_menu_event (&event->button)) {
		switch (item_type) {
		case NoteItem:
			if (internal_editing()) {
				popup_note_context_menu (item, event);
				return true;
			}
			break;
		default:
			break;
		}
	}

	return false;
}

bool
Pianoroll::button_press_dispatch (GdkEventButton* ev)
{
	/* this function is intended only for buttons 4 and above. */

	Gtkmm2ext::MouseButton b (ev->state, ev->button);
	return button_bindings->activate (b, Gtkmm2ext::Bindings::Press);
}

bool
Pianoroll::button_release_dispatch (GdkEventButton* ev)
{
	/* this function is intended only for buttons 4 and above. */

	Gtkmm2ext::MouseButton b (ev->state, ev->button);
	return button_bindings->activate (b, Gtkmm2ext::Bindings::Release);
}

bool
Pianoroll::motion_handler (ArdourCanvas::Item*, GdkEvent* event, bool from_autoscroll)
{
	if (_drags->active ()) {
		//drags change the snapped_cursor location, because we are snapping the thing being dragged, not the actual mouse cursor
		return _drags->motion_handler (event, from_autoscroll);
	}

	return true;
}

bool
Pianoroll::key_press_handler (ArdourCanvas::Item*, GdkEvent* ev, ItemType)
{

	switch (ev->key.keyval) {
	case GDK_d:
		set_mouse_mode (Editing::MouseDraw);
		break;
	case GDK_e:
		set_mouse_mode (Editing::MouseContent);
		break;
	}

	return true;
}

bool
Pianoroll::key_release_handler (ArdourCanvas::Item*, GdkEvent*, ItemType)
{
	return true;
}

void
Pianoroll::set_mouse_mode (Editing::MouseMode m, bool force)
{
	if (m != Editing::MouseDraw && m != Editing::MouseContent) {
		return;
	}

	EditingContext::set_mouse_mode (m, force);
}

void
Pianoroll::step_mouse_mode (bool next)
{
}

Editing::MouseMode
Pianoroll::current_mouse_mode () const
{
	return mouse_mode;
}

bool
Pianoroll::internal_editing() const
{
	return true;
}

RegionSelection
Pianoroll::region_selection()
{
	RegionSelection rs;
	return rs;
}

static void
edit_last_mark_label (std::vector<ArdourCanvas::Ruler::Mark>& marks, const std::string& newlabel)
{
	ArdourCanvas::Ruler::Mark copy = marks.back();
	copy.label = newlabel;
	marks.pop_back ();
	marks.push_back (copy);
}

void
Pianoroll::metric_get_bbt (std::vector<ArdourCanvas::Ruler::Mark>& marks, samplepos_t leftmost, samplepos_t rightmost, gint /*maxchars*/)
{
	if (!_session) {
		return;
	}

	bool provided = false;
	std::shared_ptr<Temporal::TempoMap> tmap;

	if (view && view->midi_region()) {
		std::shared_ptr<SMFSource> smf (std::dynamic_pointer_cast<SMFSource> (view->midi_region()->midi_source()));

		if (smf) {
			tmap = smf->tempo_map (provided);
		}
	}

	if (!provided) {
		tmap.reset (new Temporal::TempoMap (Temporal::Tempo (120, 4), Temporal::Meter (4, 4)));
	}

	EditingContext::TempoMapScope tms (*this, tmap);
	Temporal::TempoMapPoints::const_iterator i;

	char buf[64];
	Temporal::BBT_Time next_beat;
	double bbt_position_of_helper;
	bool helper_active = false;
	ArdourCanvas::Ruler::Mark mark;
	const samplecnt_t sr (_session->sample_rate());

	Temporal::TempoMapPoints grid;
	grid.reserve (4096);


	/* prevent negative values of leftmost from creeping into tempomap
	 */

	const Beats left = tmap->quarters_at_sample (leftmost).round_down_to_beat();
	const Beats lower_beat = (left < Beats() ? Beats() : left);

	using std::max;

	switch (bbt_ruler_scale) {

	case bbt_show_quarters:
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 0, 1);
		break;
	case bbt_show_eighths:
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 0, 2);
		break;
	case bbt_show_sixteenths:
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 0, 4);
		break;
	case bbt_show_thirtyseconds:
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 0, 8);
		break;
	case bbt_show_sixtyfourths:
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 0, 16);
		break;
	case bbt_show_onetwentyeighths:
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 0, 32);
		break;

	case bbt_show_1:
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 1);
		break;

	case bbt_show_4:
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 4);
		break;

	case bbt_show_16:
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 16);
		break;

	case bbt_show_64:
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 64);
		break;

	default:
		/* bbt_show_many */
		tmap->get_grid (grid, max (tmap->superclock_at (lower_beat), (superclock_t) 0), samples_to_superclock (rightmost, sr), 128);
		break;
	}

#if 0 // DEBUG GRID
	for (auto const& g : grid) {
		std::cout << "Grid " << g.time() <<  " Beats: " << g.beats() << " BBT: " << g.bbt() << " sample: " << g.sample(_session->nominal_sample_rate ()) << "\n";
	}
#endif

	if (distance (grid.begin(), grid.end()) == 0) {
		return;
	}

	/* we can accent certain lines depending on the user's Grid choice */
	/* for example, even in a 4/4 meter we can draw a grid with triplet-feel */
	/* and in this case you will want the accents on '3s' not '2s' */
	uint32_t bbt_divisor = 2;

	using namespace Editing;

	switch (_grid_type) {
	case GridTypeBeatDiv3:
		bbt_divisor = 3;
		break;
	case GridTypeBeatDiv5:
		bbt_divisor = 5;
		break;
	case GridTypeBeatDiv6:
		bbt_divisor = 3;
		break;
	case GridTypeBeatDiv7:
		bbt_divisor = 7;
		break;
	case GridTypeBeatDiv10:
		bbt_divisor = 5;
		break;
	case GridTypeBeatDiv12:
		bbt_divisor = 3;
		break;
	case GridTypeBeatDiv14:
		bbt_divisor = 7;
		break;
	case GridTypeBeatDiv16:
		break;
	case GridTypeBeatDiv20:
		bbt_divisor = 5;
		break;
	case GridTypeBeatDiv24:
		bbt_divisor = 6;
		break;
	case GridTypeBeatDiv28:
		bbt_divisor = 7;
		break;
	case GridTypeBeatDiv32:
		break;
	default:
		bbt_divisor = 2;
		break;
	}

	uint32_t bbt_beat_subdivision = 1;
	switch (bbt_ruler_scale) {
	case bbt_show_quarters:
		bbt_beat_subdivision = 1;
		break;
	case bbt_show_eighths:
		bbt_beat_subdivision = 1;
		break;
	case bbt_show_sixteenths:
		bbt_beat_subdivision = 2;
		break;
	case bbt_show_thirtyseconds:
		bbt_beat_subdivision = 4;
		break;
	case bbt_show_sixtyfourths:
		bbt_beat_subdivision = 8;
		break;
	case bbt_show_onetwentyeighths:
		bbt_beat_subdivision = 16;
		break;
	default:
		bbt_beat_subdivision = 1;
		break;
	}

	bbt_beat_subdivision *= bbt_divisor;

	switch (bbt_ruler_scale) {

	case bbt_show_many:
		snprintf (buf, sizeof(buf), "cannot handle %" PRIu32 " bars", bbt_bars);
		mark.style = ArdourCanvas::Ruler::Mark::Major;
		mark.label = buf;
		mark.position = leftmost;
		marks.push_back (mark);
		break;

	case bbt_show_64:
			for (i = grid.begin(); i != grid.end(); i++) {
				BBT_Time bbt ((*i).bbt());
				if (bbt.is_bar()) {
					if (bbt.bars % 64 == 1) {
						if (bbt.bars % 256 == 1) {
							snprintf (buf, sizeof(buf), "%" PRIu32, bbt.bars);
							mark.style = ArdourCanvas::Ruler::Mark::Major;
						} else {
							buf[0] = '\0';
							if (bbt.bars % 256 == 129)  {
								mark.style = ArdourCanvas::Ruler::Mark::Minor;
							} else {
								mark.style = ArdourCanvas::Ruler::Mark::Micro;
							}
						}
						mark.label = buf;
						mark.position = (*i).sample (sr);
						marks.push_back (mark);
					}
				}
			}
			break;

	case bbt_show_16:
		for (i = grid.begin(); i != grid.end(); i++) {
			BBT_Time bbt ((*i).bbt());
			if (bbt.is_bar()) {
			  if (bbt.bars % 16 == 1) {
				if (bbt.bars % 64 == 1) {
					snprintf (buf, sizeof(buf), "%" PRIu32, bbt.bars);
					mark.style = ArdourCanvas::Ruler::Mark::Major;
				} else {
					buf[0] = '\0';
					if (bbt.bars % 64 == 33)  {
						mark.style = ArdourCanvas::Ruler::Mark::Minor;
					} else {
						mark.style = ArdourCanvas::Ruler::Mark::Micro;
					}
				}
				mark.label = buf;
				mark.position = (*i).sample(sr);
				marks.push_back (mark);
			  }
			}
		}
	  break;

	case bbt_show_4:
		for (i = grid.begin(); i != grid.end(); ++i) {
			BBT_Time bbt ((*i).bbt());
			if (bbt.is_bar()) {
				if (bbt.bars % 4 == 1) {
					if (bbt.bars % 16 == 1) {
						snprintf (buf, sizeof(buf), "%" PRIu32, bbt.bars);
						mark.style = ArdourCanvas::Ruler::Mark::Major;
				} else {
						buf[0] = '\0';
						mark.style = ArdourCanvas::Ruler::Mark::Minor;
					}
					mark.label = buf;
					mark.position = (*i).sample (sr);
					marks.push_back (mark);
				}
			}
		}
	  break;

	case bbt_show_1:
		for (i = grid.begin(); i != grid.end(); ++i) {
			BBT_Time bbt ((*i).bbt());
			if (bbt.is_bar()) {
				snprintf (buf, sizeof(buf), "%" PRIu32, bbt.bars);
				mark.style = ArdourCanvas::Ruler::Mark::Major;
				mark.label = buf;
				mark.position = (*i).sample (sr);
				marks.push_back (mark);
			}
		}
	break;

	case bbt_show_quarters:

		mark.label = "";
		mark.position = leftmost;
		mark.style = ArdourCanvas::Ruler::Mark::Micro;
		marks.push_back (mark);

		for (i = grid.begin(); i != grid.end(); ++i) {

			BBT_Time bbt ((*i).bbt());

			if ((*i).sample (sr) < leftmost && (bbt_bar_helper_on)) {
				snprintf (buf, sizeof(buf), "<%" PRIu32 "|%" PRIu32, bbt.bars, bbt.beats);
				edit_last_mark_label (marks, buf);
			} else {

				if (bbt.is_bar()) {
					mark.style = ArdourCanvas::Ruler::Mark::Major;
					snprintf (buf, sizeof(buf), "%" PRIu32, bbt.bars);
				} else if ((bbt.beats % 2) == 1) {
					mark.style = ArdourCanvas::Ruler::Mark::Minor;
					buf[0] = '\0';
				} else {
					mark.style = ArdourCanvas::Ruler::Mark::Micro;
					buf[0] = '\0';
				}
				mark.label = buf;
				mark.position = (*i).sample (sr);
				marks.push_back (mark);
			}
		}
		break;

	case bbt_show_eighths:
	case bbt_show_sixteenths:
	case bbt_show_thirtyseconds:
	case bbt_show_sixtyfourths:
	case bbt_show_onetwentyeighths:

		bbt_position_of_helper = leftmost + (3 * get_current_zoom ());

		mark.label = "";
		mark.position = leftmost;
		mark.style = ArdourCanvas::Ruler::Mark::Micro;
		marks.push_back (mark);

		for (i = grid.begin(); i != grid.end(); ++i) {

			BBT_Time bbt ((*i).bbt());

			if ((*i).sample (sr) < leftmost && (bbt_bar_helper_on)) {
				snprintf (buf, sizeof(buf), "<%" PRIu32 "|%" PRIu32, bbt.bars, bbt.beats);
				edit_last_mark_label (marks, buf);
				helper_active = true;
			} else {

				if (bbt.is_bar()) {
					mark.style = ArdourCanvas::Ruler::Mark::Major;
					snprintf (buf, sizeof(buf), "%" PRIu32, bbt.bars);
				} else if (bbt.ticks == 0) {
					mark.style = ArdourCanvas::Ruler::Mark::Minor;
					snprintf (buf, sizeof(buf), "%" PRIu32, bbt.beats);
				} else {
					mark.style = ArdourCanvas::Ruler::Mark::Micro;
					buf[0] = '\0';
				}

				if (((*i).sample(sr) < bbt_position_of_helper) && helper_active) {
					buf[0] = '\0';
				}
				mark.label =  buf;
				mark.position = (*i).sample (sr);
				marks.push_back (mark);
			}
		}

		break;
	}
}


void
Pianoroll::mouse_mode_toggled (Editing::MouseMode m)
{
	Glib::RefPtr<Gtk::Action>       act  = get_mouse_mode_action (m);
	Glib::RefPtr<Gtk::ToggleAction> tact = Glib::RefPtr<Gtk::ToggleAction>::cast_dynamic (act);

	if (!tact->get_active()) {
		/* this was just the notification that the old mode has been
		 * left. we'll get called again with the new mode active in a
		 * jiffy.
		 */
		return;
	}

	mouse_mode = m;

	/* this should generate a new enter event which will
	   trigger the appropriate cursor.
	*/

	if (_canvas) {
		_canvas->re_enter ();
	}
}

int
Pianoroll::set_state (XMLNode const & node, int version)
{
	set_common_editing_state (node);
	return 0;
}

XMLNode&
Pianoroll::get_state () const
{
	XMLNode* node (new XMLNode (editor_name()));
	get_common_editing_state (*node);
	return *node;
}

/** @param allow_horiz true to allow horizontal autoscroll, otherwise false.
 *
 *  @param allow_vert true to allow vertical autoscroll, otherwise false.
 *
 */
void
Pianoroll::maybe_autoscroll (bool allow_horiz, bool allow_vert, bool from_headers)
{
	if (!UIConfiguration::instance().get_autoscroll_editor () || autoscroll_active ()) {
		return;
	}

	/* define a rectangular boundary for scrolling. If the mouse moves
	 * outside of this area and/or continue to be outside of this area,
	 * then we will continuously auto-scroll the canvas in the appropriate
	 * direction(s)
	 *
	 * the boundary is defined in coordinates relative to canvas' own
	 * window since that is what we're going to call ::get_pointer() on
	 * during autoscrolling to determine if we're still outside the
	 * boundary or not.
	 */

	ArdourCanvas::Rect scrolling_boundary;
	Gtk::Allocation alloc;

	alloc = get_canvas()->get_allocation ();

	alloc.set_x (0);
	alloc.set_y (0);

	if (allow_vert) {
		/* reduce height by the height of the timebars, which happens
		   to correspond to the position of the data_group.
		*/

		alloc.set_height (alloc.get_height() - data_group->position().y);
		alloc.set_y (alloc.get_y() + data_group->position().y);

		/* now reduce it again so that we start autoscrolling before we
		 * move off the top or bottom of the canvas
		 */

		alloc.set_height (alloc.get_height() - 20);
		alloc.set_y (alloc.get_y() + 10);
	}

	if (allow_horiz && (alloc.get_width() > 20)) {

		if (prh) {
			double w, h;
			prh->size_request (w, h);

			alloc.set_width (alloc.get_width() - w);
			alloc.set_x (alloc.get_x() + w);
		}

		/* the effective width of the autoscroll boundary so
		   that we start scrolling before we hit the edge.

		   this helps when the window is slammed up against the
		   right edge of the screen, making it hard to scroll
		   effectively.
		*/

		alloc.set_width (alloc.get_width() - 20);
		alloc.set_x (alloc.get_x() + 10);
	}

	scrolling_boundary = ArdourCanvas::Rect (alloc.get_x(), alloc.get_y(), alloc.get_x() + alloc.get_width(), alloc.get_y() + alloc.get_height());

	int x, y;
	Gdk::ModifierType mask;

	get_canvas()->get_window()->get_pointer (x, y, mask);

	if ((allow_horiz && ((x < scrolling_boundary.x0 && _leftmost_sample > 0) || x >= scrolling_boundary.x1)) ||
	    (allow_vert && ((y < scrolling_boundary.y0 && vertical_adjustment.get_value() > 0)|| y >= scrolling_boundary.y1))) {
		start_canvas_autoscroll (allow_horiz, allow_vert, scrolling_boundary);
	}
}

bool
Pianoroll::autoscroll_active () const
{
	return autoscroll_connection.connected ();
}

bool
Pianoroll::autoscroll_canvas ()
{
	using std::max;
	using std::min;
	int x, y;
	Gdk::ModifierType mask;
	sampleoffset_t dx = 0;
	bool no_stop = false;
	Gtk::Window* toplevel = dynamic_cast<Gtk::Window*>(_canvas_viewport->get_toplevel());

	if (!toplevel) {
		return false;
	}

	get_canvas()->get_window()->get_pointer (x, y, mask);

	VisualChange vc;
	bool vertical_motion = false;

	if (autoscroll_horizontal_allowed) {

		samplepos_t new_sample = _leftmost_sample;

		/* horizontal */

		if (x > autoscroll_boundary.x1) {

			/* bring it back into view */
			dx = x - autoscroll_boundary.x1;
			dx += 10 + (2 * (autoscroll_cnt/2));

			dx = pixel_to_sample (dx);

			dx *= UIConfiguration::instance().get_draggable_playhead_speed();

			if (_leftmost_sample < max_samplepos - dx) {
				new_sample = _leftmost_sample + dx;
			} else {
				new_sample = max_samplepos;
			}

			no_stop = true;

		} else if (x < autoscroll_boundary.x0) {

			dx = autoscroll_boundary.x0 - x;
			dx += 10 + (2 * (autoscroll_cnt/2));

			dx = pixel_to_sample (dx);

			dx *= UIConfiguration::instance().get_draggable_playhead_speed();

			if (_leftmost_sample >= dx) {
				new_sample = _leftmost_sample - dx;
			} else {
				new_sample = 0;
			}

			no_stop = true;
		}

		if (new_sample != _leftmost_sample) {
			vc.time_origin = new_sample;
			vc.add (VisualChange::TimeOrigin);
		}
	}

	if (autoscroll_vertical_allowed) {

		// const double vertical_pos = vertical_adjustment.get_value();
		const int speed_factor = 10;

		/* vertical */

		if (y < autoscroll_boundary.y0) {

			/* scroll to make higher tracks visible */

			if (autoscroll_cnt && (autoscroll_cnt % speed_factor == 0)) {
				// XXX SCROLL UP
				vertical_motion = true;
			}
			no_stop = true;

		} else if (y > autoscroll_boundary.y1) {

			if (autoscroll_cnt && (autoscroll_cnt % speed_factor == 0)) {
				// XXX SCROLL DOWN
				vertical_motion = true;
			}
			no_stop = true;
		}

	}

	if (vc.pending || vertical_motion) {

		/* change horizontal first */

		if (vc.pending) {
			visual_changer (vc);
		}

		/* now send a motion event to notify anyone who cares
		   that we have moved to a new location (because we scrolled)
		*/

		GdkEventMotion ev;

		ev.type = GDK_MOTION_NOTIFY;
		ev.state = Gdk::BUTTON1_MASK;

		/* the motion handler expects events in canvas coordinate space */

		/* we asked for the mouse position above (::get_pointer()) via
		 * our own top level window (we being the Editor). Convert into
		 * coordinates within the canvas window.
		 */

		int cx;
		int cy;

		//toplevel->translate_coordinates (*get_canvas(), x, y, cx,
		//cy);
		cx = x;
		cy = y;

		/* clamp x and y to remain within the autoscroll boundary,
		 * which is defined in window coordinates
		 */

		x = min (max ((ArdourCanvas::Coord) cx, autoscroll_boundary.x0), autoscroll_boundary.x1);
		y = min (max ((ArdourCanvas::Coord) cy, autoscroll_boundary.y0), autoscroll_boundary.y1);

		/* now convert from Editor window coordinates to canvas
		 * window coordinates
		 */

		ArdourCanvas::Duple d = get_canvas()->window_to_canvas (ArdourCanvas::Duple (cx, cy));
		ev.x = d.x;
		ev.y = d.y;
		ev.state = mask;

		motion_handler (0, (GdkEvent*) &ev, true);

	} else if (no_stop) {

		/* not changing visual state but pointer is outside the scrolling boundary
		 * so we still need to deliver a fake motion event
		 */

		GdkEventMotion ev;

		ev.type = GDK_MOTION_NOTIFY;
		ev.state = Gdk::BUTTON1_MASK;

		/* the motion handler expects events in canvas coordinate space */

		/* first convert from Editor window coordinates to canvas
		 * window coordinates
		 */

		int cx;
		int cy;

		/* clamp x and y to remain within the visible area. except
		 * .. if horizontal scrolling is allowed, always allow us to
		 * move back to zero
		 */

		if (autoscroll_horizontal_allowed) {
			x = min (max ((ArdourCanvas::Coord) x, 0.0), autoscroll_boundary.x1);
		} else {
			x = min (max ((ArdourCanvas::Coord) x, autoscroll_boundary.x0), autoscroll_boundary.x1);
		}
		y = min (max ((ArdourCanvas::Coord) y, autoscroll_boundary.y0), autoscroll_boundary.y1);

		// toplevel->translate_coordinates (*get_canvas_viewport(), x,
		// y, cx, cy);
		cx = x;
		cy = y;

		ArdourCanvas::Duple d = get_canvas()->window_to_canvas (ArdourCanvas::Duple (cx, cy));
		ev.x = d.x;
		ev.y = d.y;
		ev.state = mask;

		motion_handler (0, (GdkEvent*) &ev, true);

	} else {
		stop_canvas_autoscroll ();
		return false;
	}

	autoscroll_cnt++;

	return true; /* call me again */
}

void
Pianoroll::start_canvas_autoscroll (bool allow_horiz, bool allow_vert, const ArdourCanvas::Rect& boundary)
{
	if (!_session) {
		return;
	}

	stop_canvas_autoscroll ();

	autoscroll_horizontal_allowed = allow_horiz;
	autoscroll_vertical_allowed = allow_vert;
	autoscroll_boundary = boundary;

	/* do the first scroll right now
	*/

	autoscroll_canvas ();

	/* scroll again at very very roughly 30FPS */

	autoscroll_connection = Glib::signal_timeout().connect (sigc::mem_fun (*this, &Pianoroll::autoscroll_canvas), 30);
}

void
Pianoroll::stop_canvas_autoscroll ()
{
	autoscroll_connection.disconnect ();
	autoscroll_cnt = 0;
}

void
Pianoroll::visual_changer (const VisualChange& vc)
{
	/**
	 * Changed first so the correct horizontal canvas position is calculated in
	 * EditingContext::set_horizontal_position
	 */
	if (vc.pending & VisualChange::ZoomLevel) {
		set_samples_per_pixel (vc.samples_per_pixel);
	}

	if (vc.pending & VisualChange::TimeOrigin) {
		double new_time_origin = sample_to_pixel_unrounded (vc.time_origin);
		set_horizontal_position (new_time_origin);
	}

	if (vc.pending & VisualChange::YOrigin) {
		vertical_adjustment.set_value (vc.y_origin);
	}

	/**
	 * Now the canvas is in the final state before render the canvas items that
	 * support the Item::prepare_for_render interface can calculate the correct
	 * item to visible canvas intersection.
	 */
	if (vc.pending & VisualChange::ZoomLevel) {
		on_samples_per_pixel_changed ();

		// update_tempo_based_rulers ();
	}

	if (!(vc.pending & VisualChange::ZoomLevel)) {
		/* If the canvas is not being zoomed then the canvas items will not change
		 * and cause Item::prepare_for_render to be called so do it here manually.
		 * Not ideal, but I can't think of a better solution atm.
		 */
		get_canvas()->prepare_for_render();
	}

	/* If we are only scrolling vertically there is no need to update these */
	if (vc.pending != VisualChange::YOrigin) {
		// XXX update_fixed_rulers ();
		redisplay_grid (true);
	}
}

void
Pianoroll::on_samples_per_pixel_changed ()
{
	if (view) {
		view->set_samples_per_pixel (samples_per_pixel);
	}
}

void
Pianoroll::midi_action (void (MidiView::*method)())
{
	if (!view) {
		return;
	}

	(view->*method) ();
}

void
Pianoroll::escape ()
{
	if (!view) {
		return;
	}

	view->clear_selection ();
}

Gdk::Cursor*
Pianoroll::which_track_cursor () const
{
	return _cursors->grabber;
}

Gdk::Cursor*
Pianoroll::which_mode_cursor () const
{
	Gdk::Cursor* mode_cursor = MouseCursors::invalid_cursor ();

	switch (mouse_mode) {
	case Editing::MouseContent:
		mode_cursor = _cursors->grabber;
		break;

	case Editing::MouseDraw:
		mode_cursor = _cursors->midi_pencil;
		break;

	default:
		break;
	}

	return mode_cursor;
}

Gdk::Cursor*
Pianoroll::which_trim_cursor (bool left_side) const
{
	abort ();
	/*NOTREACHED*/
	return nullptr;
}


Gdk::Cursor*
Pianoroll::which_canvas_cursor (ItemType type) const
{
	Gdk::Cursor* cursor = which_mode_cursor ();

	if (mouse_mode == Editing::MouseContent) {

		/* find correct cursor to use in object/smart mode */
		switch (type) {
		case AutomationTrackItem:
			cursor = which_track_cursor ();
			break;
		case PlayheadCursorItem:
			cursor = _cursors->grabber;
			break;
		case SelectionItem:
			cursor = _cursors->selector;
			break;
		case ControlPointItem:
			cursor = _cursors->fader;
			break;
		case GainLineItem:
			cursor = _cursors->cross_hair;
			break;
		case EditorAutomationLineItem:
			cursor = _cursors->cross_hair;
			break;
		case StartSelectionTrimItem:
			cursor = _cursors->left_side_trim;
			break;
		case EndSelectionTrimItem:
			cursor = _cursors->right_side_trim;
			break;
		case NoteItem:
			cursor = _cursors->grabber_note;
			break;
		case RegionItem:
			cursor = nullptr; /* default cursor */
			break;
		case VelocityItem:
			cursor = _cursors->up_down;
			break;

		case ClipEndItem:
		case ClipStartItem:
			cursor = _cursors->expand_left_right;
			break;

		default:
			break;
		}

	} else if (mouse_mode == Editing::MouseDraw) {

		/* ControlPointItem is not really specific to region gain mode
		   but it is the same cursor so don't worry about this for now.
		   The result is that we'll see the fader cursor if we enter
		   non-region-gain-line control points while in MouseDraw
		   mode, even though we can't edit them in this mode.
		*/

		switch (type) {
		case ControlPointItem:
			cursor = _cursors->fader;
			break;
		case NoteItem:
			cursor = _cursors->grabber_note;
			break;
		case ClipEndItem:
		case ClipStartItem:
			cursor = _cursors->expand_left_right;
			break;
		case RegionItem:
			cursor = _cursors->midi_pencil;
			break;
		case VelocityItem:
			cursor = _cursors->up_down;
			break;
		default:
			break;
		}
	}

	return cursor;
}

bool
Pianoroll::enter_handler (ArdourCanvas::Item* item, GdkEvent* ev, ItemType item_type)
{
	choose_canvas_cursor_on_entry (item_type);

	switch (item_type) {
	case AutomationTrackItem:
		/* item is the base rectangle */
		if (view) {
			view->automation_entry ();
		}
		break;

	case EditorAutomationLineItem:
		{
			ArdourCanvas::Line *line = dynamic_cast<ArdourCanvas::Line *> (item);

			if (line) {
				line->set_outline_color (UIConfiguration::instance().color ("entered automation line"));
			}
		}
		break;
	default:
		break;
	}

	return true;
}

bool
Pianoroll::leave_handler (ArdourCanvas::Item* item, GdkEvent* ev, ItemType item_type)
{
	EditorAutomationLine* al;

	set_canvas_cursor (which_mode_cursor());

	switch (item_type) {
	case ControlPointItem:
		_verbose_cursor->hide ();
		break;

	case EditorAutomationLineItem:
		al = reinterpret_cast<EditorAutomationLine*> (item->get_data ("line"));
		{
			ArdourCanvas::Line *line = dynamic_cast<ArdourCanvas::Line *> (item);
			if (line) {
				line->set_outline_color (al->get_line_color());
			}
		}
		if (ev->crossing.detail != GDK_NOTIFY_INFERIOR) {
			view->automation_leave ();
		}
		break;

	default:
		break;
	}


	return true;
}

std::list<SelectableOwner*>
Pianoroll::selectable_owners()
{
	if (view) {
		return view->selectable_owners();
	}

	return std::list<SelectableOwner*> ();
}

void
Pianoroll::trigger_prop_change (PBD::PropertyChange const & what_changed)
{
	if (what_changed.contains (Properties::region)) {
		std::shared_ptr<MidiRegion> mr = std::dynamic_pointer_cast<MidiRegion> (ref.trigger()->the_region());
		if (mr) {
			set_region (mr);
		}
	}
}

void
Pianoroll::blink_rec_enable (bool onoff)
{
	if (onoff) {
		rec_enable_button.set_active_state (Gtkmm2ext::ExplicitActive);
	} else {
		rec_enable_button.set_active_state (Gtkmm2ext::Off);
	}
}

void
Pianoroll::rec_enable_change ()
{
	if (!ref.box()) {
		return;
	}

	rec_blink_connection.disconnect ();

	switch (ref.box()->record_enabled()) {
	case Recording:
		rec_enable_button.set_active_state (Gtkmm2ext::ExplicitActive);
		break;
	case Enabled:
		rec_enable_button.set_active_state (Gtkmm2ext::ExplicitActive);
		if (!UIConfiguration::instance().get_no_strobe() && ref.trigger()->armed()) {
			rec_blink_connection = Timers::blink_connect (sigc::mem_fun (*this, &Pianoroll::blink_rec_enable));
		}
		break;
	case Disabled:
		rec_enable_button.set_active_state (Gtkmm2ext::Off);
		break;
	}
}


bool
Pianoroll::rec_button_press (GdkEventButton* ev)
{
	std::cerr << "RBP!\n";

	if (ev->button != 1) {
		return false;
	}

	if (!ref.box()) {
		return true;
	}

	TriggerPtr trigger (ref.trigger());
	bool trigger_armed = trigger->armed();

	if (!trigger_armed) {

		Stripable* st = dynamic_cast<Stripable*> (ref.box()->owner());
		assert (st);
		std::shared_ptr<Track> track = std::dynamic_pointer_cast<MidiTrack> (st->shared_from_this());
		assert (track);

		std::shared_ptr<RouteList> rl;

		rl.reset (new RouteList);
		rl->push_back (track);

		_session->set_controls (route_list_to_control_list (rl, &Stripable::rec_enable_control), true, Controllable::NoGroup);
	}

	/* Step two: the trigger */

	if (trigger_armed) {
		trigger->disarm ();
	} else {
		trigger->arm ();
	}

	return true;
}

void
Pianoroll::set (TriggerReference & tref)
{
	_update_connection.disconnect ();
	object_connections.drop_connections ();

	ref = tref;

	rec_box.show ();
	rec_enable_button.set_sensitive (true);
	ref.box()->RecEnableChanged.connect (object_connections, invalidator (*this), std::bind (&Pianoroll::rec_enable_change, this), gui_context());

	idle_update_queued.store (0);

	ref.box()->Captured.connect (object_connections, invalidator (*this), std::bind (&Pianoroll::data_captured, this, _1), gui_context());
	/* Don't bind a shared_ptr<TriggerBox> within the lambda */
	TriggerBox* tb (ref.box().get());
	tb->RecEnableChanged.connect (object_connections, invalidator (*this), [&, tb]() { box_rec_enable_change (*tb); }, gui_context());

	Stripable* st = dynamic_cast<Stripable*> (ref.box()->owner());
	assert (st);
	_track = std::dynamic_pointer_cast<MidiTrack> (st->shared_from_this());
	assert (_track);

	set_track (_track);

	_update_connection = Timers::rapid_connect (sigc::mem_fun (*this, &Pianoroll::maybe_update));
	_track->DropReferences.connect (object_connections, invalidator (*this), std::bind (&Pianoroll::unset, this), gui_context());
	ref.trigger()->PropertyChanged.connect (object_connections, invalidator (*this), std::bind (&Pianoroll::trigger_prop_change, this, _1), gui_context());

	if (ref.trigger()->the_region()) {

		std::shared_ptr<MidiRegion> mr = std::dynamic_pointer_cast<MidiRegion> (ref.trigger()->the_region());

		if (mr) {
			set_region (mr);
		}
	}
}

void
Pianoroll::unset ()
{
	_update_connection.disconnect();
	object_connections.drop_connections ();
	_track.reset ();
	view->set_region (nullptr);
	ref = TriggerReference ();
}

void
Pianoroll::set_track (std::shared_ptr<ARDOUR::MidiTrack> track)
{
	if (view) {
		view->set_track (track);
	}

	cc_dropdown1->menu().items().clear ();
	cc_dropdown2->menu().items().clear ();
	cc_dropdown3->menu().items().clear ();

	build_controller_menu (cc_dropdown1->menu(), track->instrument_info(), 0xffff,
	                       sigc::bind (sigc::mem_fun (*this, &Pianoroll::add_single_controller_item), cc_dropdown1),
	                       sigc::bind (sigc::mem_fun (*this, &Pianoroll::add_multi_controller_item), cc_dropdown1));
	build_controller_menu (cc_dropdown2->menu(), track->instrument_info(), 0xffff,
	                       sigc::bind (sigc::mem_fun (*this, &Pianoroll::add_single_controller_item), cc_dropdown2),
	                       sigc::bind (sigc::mem_fun (*this, &Pianoroll::add_multi_controller_item), cc_dropdown2));
	build_controller_menu (cc_dropdown3->menu(), track->instrument_info(), 0xffff,
	                       sigc::bind (sigc::mem_fun (*this, &Pianoroll::add_single_controller_item), cc_dropdown3),
	                       sigc::bind (sigc::mem_fun (*this, &Pianoroll::add_multi_controller_item), cc_dropdown3));

	// reset_user_cc_choice (Evoral::Parameter (ARDOUR::MidiCCAutomation, _visible_channel, MIDI_CTL_MSB_GENERAL_PURPOSE1), cc_dropdown1);
	// reset_user_cc_choice (Evoral::Parameter (ARDOUR::MidiCCAutomation, _visible_channel, MIDI_CTL_MSB_GENERAL_PURPOSE2), cc_dropdown2);
	// reset_user_cc_choice (Evoral::Parameter (ARDOUR::MidiCCAutomation, _visible_channel, MIDI_CTL_MSB_GENERAL_PURPOSE3), cc_dropdown3);
}

void
Pianoroll::set_region (std::shared_ptr<ARDOUR::MidiRegion> r)
{
	if (!r) {
		view->set_region (nullptr);
		return;
	}

	view->set_region (r);
	view->show_start (true);
	view->show_end (true);

	bool provided = false;
	std::shared_ptr<Temporal::TempoMap> map;
	std::shared_ptr<SMFSource> smf (std::dynamic_pointer_cast<SMFSource> (r->midi_source()));

	if (smf) {
		map = smf->tempo_map (provided);
	}

	if (!provided) {
		/* COPY MAIN SESSION TEMPO MAP? */
		map.reset (new Temporal::TempoMap (Temporal::Tempo (120, 4), Temporal::Meter (4, 4)));
	}

	{
		EditingContext::TempoMapScope tms (*this, map);
		/* Compute zoom level to show entire source plus some margin if possible */
		zoom_to_show (timecnt_t (timepos_t (max_extents_scale() * max_zoom_extent ().second.samples())));

	}

	set_visible_channel (0);
}

void
Pianoroll::zoom_to_show (Temporal::timecnt_t const & duration)
{
	if (!_visible_canvas_width) {
		zoom_in_allocate = true;
		return;
	}

	/* make it 20% wider than we need */
	samplecnt_t samples = duration.samples();
	samplecnt_t spp = floor (samples / _track_canvas_width);

	reset_zoom (spp);
}

bool
Pianoroll::user_automation_button_event (GdkEventButton* ev, MetaButton* mb)
{
	if (mb->is_menu_popup_event (ev)) {
		return false;
	}

	if (mb->is_led_click (ev)) {
		user_led_click (ev, mb);
		return true;
	}

	ParameterButtonMap::iterator i = parameter_button_map.find (mb);
	if (i == parameter_button_map.end()) {
		return false;
	}

	return automation_button_event (ev, i->second.type(), i->second.id());
}

bool
Pianoroll::automation_button_event (GdkEventButton* ev, Evoral::ParameterType type, int id)
{
	if (ev->button != 1) {
		return false;
	}

	SelectionOperation op = ArdourKeyboard::selection_type (ev->state);

	switch (ev->type) {
	case GDK_BUTTON_RELEASE:
		if (view)  {
			Evoral::Parameter param (type, _visible_channel, id);

			if (view->is_visible_automation (param) && (op == SelectionSet)) {
				op = SelectionToggle;
			}

			view->update_automation_display (param, op);
		}
		return true;
	default:
		break;
	}

	return false;
}

void
Pianoroll::user_led_click (GdkEventButton* ev, MetaButton* metabutton)
{
	if (ev->button != 1) {
		return;
	}

	/* Find the parameter */

	ParameterButtonMap::iterator i;
	for (i = parameter_button_map.begin(); i != parameter_button_map.end(); ++i) {
		if (i->first == metabutton) {
			break;
		}
	}

	if (i == parameter_button_map.end()) {
		return;
	}

	if (view) {
		view->set_active_automation (i->second);
	}
}

void
Pianoroll::automation_led_click (GdkEventButton* ev, Evoral::ParameterType type, int id)
{
	if (ev->button != 1) {
		return;
	}

	if (view)  {
		view->set_active_automation (Evoral::Parameter (type, _visible_channel, id));
	}
}

void
Pianoroll::automation_state_changed ()
{
	assert (view);

	for (ParameterButtonMap::iterator i = parameter_button_map.begin(); i != parameter_button_map.end(); ++i) {
		std::string str (ARDOUR::EventTypeMap::instance().to_symbol (i->second));

		/* Indicate visible automation state with selected/not-selected visual state */

		if (view->is_visible_automation (i->second)) {
			i->first->set_visual_state (Gtkmm2ext::Selected);
		} else {
			i->first->set_visual_state (Gtkmm2ext::NoVisualState);
		}

		/* Indicate active automation state with explicit widget active state (LED) */

		if (view->is_active_automation (i->second)) {
			i->first->set_active_state (Gtkmm2ext::ExplicitActive);
		} else {
			i->first->set_active_state (Gtkmm2ext::Off);
		}
	}
}

void
Pianoroll::note_mode_clicked ()
{
	assert (bg);

	if (bg->note_mode() == Sustained) {
		set_note_mode (Percussive);
	} else {
		set_note_mode (Sustained);
	}
}

void
Pianoroll::set_note_mode (NoteMode nm)
{
	assert (bg);

	if (nm != bg->note_mode()) {
		bg->set_note_mode (nm);
		if (bg->note_mode() == Percussive) {
			note_mode_button.set_active (true);
		} else {
			note_mode_button.set_active (false);
		}
	}
}

void
Pianoroll::build_zoom_focus_menu ()
{
	using namespace Gtk::Menu_Helpers;
	using namespace Editing;

	zoom_focus_selector.AddMenuElem (MenuElem (zoom_focus_strings[(int)ZoomFocusLeft], sigc::bind (sigc::mem_fun(*this, &EditingContext::zoom_focus_selection_done), (ZoomFocus) ZoomFocusLeft)));
	zoom_focus_selector.AddMenuElem (MenuElem (zoom_focus_strings[(int)ZoomFocusRight], sigc::bind (sigc::mem_fun(*this, &EditingContext::zoom_focus_selection_done), (ZoomFocus) ZoomFocusRight)));
	zoom_focus_selector.AddMenuElem (MenuElem (zoom_focus_strings[(int)ZoomFocusCenter], sigc::bind (sigc::mem_fun(*this, &EditingContext::zoom_focus_selection_done), (ZoomFocus) ZoomFocusCenter)));
	zoom_focus_selector.AddMenuElem (MenuElem (zoom_focus_strings[(int)ZoomFocusMouse], sigc::bind (sigc::mem_fun(*this, &EditingContext::zoom_focus_selection_done), (ZoomFocus) ZoomFocusMouse)));
	zoom_focus_selector.set_sizing_texts (zoom_focus_strings);
}


std::pair<Temporal::timepos_t,Temporal::timepos_t>
Pianoroll::max_zoom_extent() const
{
	if (view && view->midi_region()) {
		/* XXX make this dependent on view _show_source setting */
		return std::make_pair (Temporal::timepos_t (Temporal::Beats()), Temporal::timepos_t (view->midi_region()->midi_source()->length().beats()));
	}

	return std::make_pair (Temporal::timepos_t (Temporal::Beats()), Temporal::timepos_t (Temporal::Beats (32, 0)));
}

void
Pianoroll::full_zoom_clicked()
{
	/* XXXX NEED LOCAL TEMPO MAP */

	std::pair<Temporal::timepos_t,Temporal::timepos_t> dur (max_zoom_extent());
	samplecnt_t s = dur.second.samples() - dur.first.samples();
	reposition_and_zoom (0,  (s / (double) _visible_canvas_width));
}

void
Pianoroll::point_selection_changed ()
{
	if (view) {
		view->point_selection_changed ();
	}
}

void
Pianoroll::delete_ ()
{
	/* Editor has a lot to do here, potentially. But we don't */
	cut_copy (Editing::Delete);
}

void
Pianoroll::paste (float times, bool from_context_menu)
{
	if (view) {
		// view->paste (Editing::Cut);
	}
}

void
Pianoroll::keyboard_paste ()
{
}

/** Cut, copy or clear selected regions, automation points or a time range.
 * @param op Operation (Delete, Cut, Copy or Clear)
 */

void
Pianoroll::cut_copy (Editing::CutCopyOp op)
{
	using namespace Editing;

	/* only cancel selection if cut/copy is successful.*/

	std::string opname;

	switch (op) {
	case Delete:
		opname = _("delete");
		break;
	case Cut:
		opname = _("cut");
		break;
	case Copy:
		opname = _("copy");
		break;
	case Clear:
		opname = _("clear");
		break;
	}

	/* if we're deleting something, and the mouse is still pressed,
	   the thing we started a drag for will be gone when we release
	   the mouse button(s). avoid this. see part 2 at the end of
	   this function.
	*/

	if (op == Delete || op == Cut || op == Clear) {
		if (_drags->active ()) {
			_drags->abort ();
		}
	}

	if (op != Delete) { //"Delete" doesn't change copy/paste buf
		cut_buffer->clear ();
	}

	switch (mouse_mode) {
	case MouseDraw:
	case MouseContent:
		if (view) {
			begin_reversible_command (opname + ' ' + X_("MIDI"));
			view->cut_copy_clear (op);
			commit_reversible_command ();
		}
		return;
	default:
		break;
	}


	if (op == Delete || op == Cut || op == Clear) {
		_drags->abort ();
	}
}

void
Pianoroll::select_all_within (Temporal::timepos_t const & start, Temporal::timepos_t const & end, double y0, double y1, std::list<SelectableOwner*> const & ignored, ARDOUR::SelectionOperation op, bool preserve_if_selected)
{
	std::list<Selectable*> found;

	if (!view) {
		return;
	}

	AutomationLine* al = view->active_automation_line();

	if (!al) {
		return;
	}

	double topfrac;
	double botfrac;


	/* translate y0 and y1 to use the top of the automation area as the * origin */

	double automation_origin = view->automation_group_position().y;

	y0 -= automation_origin;
	y1 -= automation_origin;

	if (y0 < 0. && al->height() <= y1) {

		/* _y_position is below top, mybot is above bot, so we're fully
		   covered vertically.
		*/

		topfrac = 1.0;
		botfrac = 0.0;

	} else {

		/* top and bot are within _y_position .. mybot */

		topfrac = 1.0 - (y0 / al->height());
		botfrac = 1.0 - (y1 / al->height());

	}

	al->get_selectables (start, end, botfrac, topfrac, found);

	if (found.empty()) {
		view->clear_selection ();
		return;
	}

	if (preserve_if_selected && op != SelectionToggle) {
		auto i = found.begin();
		while (i != found.end() && (*i)->selected()) {
			++i;
		}

		if (i == found.end()) {
			return;
		}
	}

	switch (op) {
	case SelectionAdd:
		begin_reversible_selection_op (X_("add select all within"));
		selection->add (found);
		break;
	case SelectionToggle:
		begin_reversible_selection_op (X_("toggle select all within"));
		selection->toggle (found);
		break;
	case SelectionSet:
		begin_reversible_selection_op (X_("select all within"));
		selection->set (found);
		break;
	default:
		return;
	}

	commit_reversible_selection_op ();
}
