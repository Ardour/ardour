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

#include "ytkmm/scrollbar.h"

#include "pbd/stateful_diff_command.h"
#include "pbd/unwind.h"

#include "ardour/midi_region.h"
#include "ardour/midi_track.h"
#include "ardour/smf_source.h"
#include "ardour/region_factory.h"

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
#include "editing_convert.h"
#include "editor_cursors.h"
#include "editor_drag.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "midi_util.h"
#include "paste_context.h"
#include "pianoroll_background.h"
#include "pianoroll.h"
#include "pianoroll_midi_view.h"
#include "note_base.h"
#include "prh.h"
#include "timers.h"
#include "ui_config.h"
#include "verbose_cursor.h"

#include "pbd/i18n.h"

#undef PIANOROLL_USER_BUTTONS

using namespace PBD;
using namespace ARDOUR;
using namespace ArdourCanvas;
using namespace ArdourWidgets;
using namespace Gtkmm2ext;
using namespace Temporal;

Pianoroll::Pianoroll (std::string const & name, bool with_transport)
	: CueEditor (name, with_transport)
	, prh (nullptr)
	, layered_automation (true)
	, bg (nullptr)
	, view (nullptr)
	, bbt_metric (*this)
	, ignore_channel_changes (false)
{
	autoscroll_vertical_allowed = false;

	load_bindings ();
	register_actions ();

	build_upper_toolbar ();
	build_canvas ();

	build_grid_type_menu ();
	build_draw_midi_menus();

	build_lower_toolbar ();

	set_action_defaults ();
	set_mouse_mode (Editing::MouseContent, true);

	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &Pianoroll::parameter_changed));
}

Pianoroll::~Pianoroll ()
{
	drop_grid (); // unparent gridlines before deleting _canvas_viewport

	delete view;
	delete bg;
}

void
Pianoroll::set_show_source (bool yn)
{
	EC_LOCAL_TEMPO_SCOPE;

	CueEditor::set_show_source (yn);
	if (view) {
		view->set_show_source (yn);
	}
}

void
Pianoroll::rebuild_parameter_button_map()
{
	EC_LOCAL_TEMPO_SCOPE;

	parameter_button_map.clear ();
	parameter_button_map.insert (std::make_pair (velocity_button, Evoral::Parameter (ARDOUR::MidiVelocityAutomation, _visible_channel)));
	parameter_button_map.insert (std::make_pair (bender_button, Evoral::Parameter (ARDOUR::MidiPitchBenderAutomation, _visible_channel)));
	parameter_button_map.insert (std::make_pair (pressure_button, Evoral::Parameter (ARDOUR::MidiChannelPressureAutomation, _visible_channel)));
	parameter_button_map.insert (std::make_pair (expression_button, Evoral::Parameter (ARDOUR::MidiCCAutomation, _visible_channel, MIDI_CTL_MSB_EXPRESSION)));
	parameter_button_map.insert (std::make_pair (modulation_button, Evoral::Parameter (ARDOUR::MidiCCAutomation, _visible_channel, MIDI_CTL_MSB_MODWHEEL)));

#ifdef PIANOROLL_USER_BUTTONS
	parameter_button_map.insert (std::make_pair (cc_dropdown1, Evoral::Parameter (ARDOUR::MidiCCAutomation, _visible_channel, MIDI_CTL_MSB_GENERAL_PURPOSE1)));
	parameter_button_map.insert (std::make_pair (cc_dropdown2, Evoral::Parameter (ARDOUR::MidiCCAutomation, _visible_channel, MIDI_CTL_MSB_GENERAL_PURPOSE2)));
	parameter_button_map.insert (std::make_pair (cc_dropdown3, Evoral::Parameter (ARDOUR::MidiCCAutomation, _visible_channel, MIDI_CTL_MSB_GENERAL_PURPOSE3)));
#endif
}

void
Pianoroll::reset_user_cc_choice (std::string name, Evoral::Parameter param, MetaButton* metabutton)
{
#ifdef PIANOROLL_USER_BUTTONS

	EC_LOCAL_TEMPO_SCOPE;

	ParameterButtonMap::iterator iter;

	for (iter = parameter_button_map.begin(); iter != parameter_button_map.end(); ++iter) {
		if (iter->first == metabutton) {
			parameter_button_map.erase (iter);
			break;
		}
	}

	parameter_button_map.insert (std::make_pair (metabutton, param));

	metabutton->set_by_menutext (name);
#endif
}

void
Pianoroll::add_single_controller_item (Gtk::Menu_Helpers::MenuList& ctl_items,
                                       int                     ctl,
                                       const std::string&      name,
                                       ArdourWidgets::MetaButton* mb)
{
	EC_LOCAL_TEMPO_SCOPE;

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

void
Pianoroll::add_multi_controller_item (Gtk::Menu_Helpers::MenuList&,
                                      const uint16_t          channels,
                                      int                     ctl,
                                      const std::string&      name,
                                      MetaButton*             mb)
{
#ifdef PIANOROLL_USER_BUTTONS

	EC_LOCAL_TEMPO_SCOPE;

	using namespace Gtk;
	using namespace Gtk::Menu_Helpers;

	Menu* chn_menu = manage (new Menu);
	MenuList& chn_items (chn_menu->items());
	std::string menu_text (string_compose ("%1: %2", ctl, name));

	/* Build the channel sub-menu */

	Evoral::Parameter param_without_channel (MidiCCAutomation, 0, ctl);

	/* look up the parameter represented by this MetaButton */
	ParameterButtonMap::iterator pbmi = parameter_button_map.find (mb);

	for (uint8_t chn = 0; chn < 16; chn++) {
		if (channels & (0x0001 << chn)) {

			/* for each selected channel, add a menu item for this controller */

			Evoral::Parameter fully_qualified_param (MidiCCAutomation, chn, ctl);

			chn_items.push_back (CheckMenuElem (string_compose (_("Channel %1"), chn+1),
			                                    sigc::bind (sigc::mem_fun (*this, &Pianoroll::reset_user_cc_choice),  menu_text, fully_qualified_param, mb)));


			if (pbmi != parameter_button_map.end()) {

				/* if this parameter is the one represented by
				   the button, mark it active in the menu
				*/

				if (fully_qualified_param == pbmi->second) {
					Gtk::CheckMenuItem* cmi = static_cast<Gtk::CheckMenuItem*>(&chn_items.back());
					// cmi->set_active();
				}
			}
		}
	}

	/* add an item to metabutton's menu that will connect to the
	 * per-channel submenu we built above.
	 */

	mb->add_item (name, menu_text, *chn_menu, [](){});
#endif
}

void
Pianoroll::layered_automation_button_clicked ()
{
	set_layered_automation (!layered_automation);
}

void
Pianoroll::set_layered_automation (bool yn)
{
	if ((layered_automation = yn)) {
		layered_automation_button->set_active_state (Gtkmm2ext::ExplicitActive);
		if (view && view->n_visible_automation() > 1) {
			view->hide_all_automation ();
		}
	} else {
		layered_automation_button->set_active_state (Gtkmm2ext::Off);
		if (view && view->n_visible_automation() > 1) {
			view->hide_all_automation ();
		}
	}
}

void
Pianoroll::build_lower_toolbar ()
{
	EC_LOCAL_TEMPO_SCOPE;
	Gtk::RadioButtonGroup edit_group;

	horizontal_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &Pianoroll::scrolled));

	layered_automation_button = new ArdourButton (ArdourButton::Element (ArdourButton::VectorIcon|ArdourButton::Edge|ArdourButton::Body));
	layered_automation_button->set_icon (ArdourIcon::PsetBrowse);
	layered_automation_button->signal_clicked.connect (sigc::mem_fun (*this, &Pianoroll::layered_automation_button_clicked));

	Gtk::HBox* stupid (manage (new Gtk::HBox));
	Gtk::Label* layer_label (manage (new Gtk::Label (_("Layered"))));
	stupid->pack_start (*layered_automation_button, false, false);
	stupid->pack_start (*layer_label, false, false, 6);

	velocity_button = new ControllerControls (-1, _("Velocity"), edit_group);
	bender_button = new ControllerControls (MIDI_CMD_BENDER, _("Bender"), edit_group);
	pressure_button = new ControllerControls (MIDI_CMD_CHANNEL_PRESSURE, _("Pressure"), edit_group);
	expression_button = new ControllerControls (MIDI_CTL_MSB_EXPRESSION, _("Expression"), edit_group);
	modulation_button = new ControllerControls (MIDI_CTL_MSB_MODWHEEL, _("Modulation"), edit_group);

#ifdef PIANOROLL_USER_BUTTONS
	cc_dropdown1 = new MetaButton ();
	cc_dropdown2 = new MetaButton ();
	cc_dropdown3 = new MetaButton ();

	cc_dropdown1->disable_scrolling ();
	cc_dropdown2->disable_scrolling ();
	cc_dropdown3->disable_scrolling ();

	cc_dropdown1->add_elements (ArdourButton::Indicator);
	cc_dropdown2->add_elements (ArdourButton::Indicator);
	cc_dropdown3->add_elements (ArdourButton::Indicator);
#endif
	rebuild_parameter_button_map ();

	// button_bar.set_homogeneous (true);
	button_bar.set_spacing (6);
	button_bar.set_border_width (6);

	button_bar.pack_start (*stupid, false, false);

	button_bar.pack_start (*velocity_button, false, false);
	button_bar.pack_start (*bender_button, false, false);
	button_bar.pack_start (*pressure_button, false, false);
	button_bar.pack_start (*modulation_button, false, false);

#ifdef PIANOROLL_USER_BUTTONS
	button_bar.pack_start (*cc_dropdown1, false, false);
	button_bar.pack_start (*cc_dropdown2, false, false);
	button_bar.pack_start (*cc_dropdown3, false, false);
#endif

	velocity_button->show_clicked.connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::automation_show_button_click), ARDOUR::MidiVelocityAutomation, 0));
	pressure_button->show_clicked.connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::automation_show_button_click), ARDOUR::MidiChannelPressureAutomation, 0));
	bender_button->show_clicked.connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::automation_show_button_click), ARDOUR::MidiPitchBenderAutomation, 0));
	modulation_button->show_clicked.connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::automation_show_button_click), ARDOUR::MidiCCAutomation, MIDI_CTL_MSB_MODWHEEL));
	expression_button->show_clicked.connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::automation_show_button_click), ARDOUR::MidiCCAutomation, MIDI_CTL_MSB_EXPRESSION));

	velocity_button->edit_clicked.connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::automation_active_button_click), ARDOUR::MidiVelocityAutomation, 0));
	pressure_button->edit_clicked.connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::automation_active_button_click), ARDOUR::MidiChannelPressureAutomation, 0));
	bender_button->edit_clicked.connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::automation_active_button_click), ARDOUR::MidiPitchBenderAutomation, 0));
	modulation_button->edit_clicked.connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::automation_active_button_click), ARDOUR::MidiCCAutomation, MIDI_CTL_MSB_MODWHEEL));
	expression_button->edit_clicked.connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::automation_active_button_click), ARDOUR::MidiCCAutomation, MIDI_CTL_MSB_EXPRESSION));

#ifdef PIANOROLL_USER_BUTTONS
	cc_dropdown1->show_clicked.connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::user_automation_active_button_click), cc_dropdown1), false);
	cc_dropdown2->show_clicked.connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::user_automation_active_button_click), cc_dropdown2), false);
	cc_dropdown3->show_clicked.connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::user_automation_active_button_click), cc_dropdown3), false);

	cc_dropdown1->edit_clicked.connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::user_led_click), cc_dropdown1));
	cc_dropdown2->edit_clicked.connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::user_led_click), cc_dropdown2));
	cc_dropdown3->edit_clicked.connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::user_led_click), cc_dropdown3));

	cc_dropdown1->signal_map().connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::build_cc_menu), cc_dropdown1));
	cc_dropdown2->signal_map().connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::build_cc_menu), cc_dropdown2));
	cc_dropdown3->signal_map().connect (sigc::bind (sigc::mem_fun (*this, &Pianoroll::build_cc_menu), cc_dropdown3));
#endif

	_toolbox.pack_start (*_canvas_hscrollbar, false, false);
	_toolbox.pack_start (button_bar, false, false);
}

void
Pianoroll::pack_inner (Gtk::Box& box)
{
	EC_LOCAL_TEMPO_SCOPE;

	box.pack_start (snap_box, false, false);
	box.pack_start (*(manage (new ArdourVSpacer ())), false, false, 3);
	box.pack_start (draw_box, false, false);
	draw_box.show ();
}

void
Pianoroll::pack_outer (Gtk::Box& box)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (with_transport_controls) {
		box.pack_start (play_box, false, false, 12);
	}

	box.pack_start (rec_box, false, false);
	box.pack_start (visible_channel_label, false, false);
	box.pack_start (visible_channel_selector, false, false);
	box.pack_start (note_mode_button, false, false);
}

void
Pianoroll::set_visible_channel (int n)
{
	EC_LOCAL_TEMPO_SCOPE;

	PBD::Unwinder<bool> uw (ignore_channel_changes, true);

	_visible_channel = n;
	visible_channel_selector.set_active (string_compose ("%1", _visible_channel + 1));

	rebuild_parameter_button_map ();

	if (view) {
		view->set_visible_channel (n);
		view->swap_automation_channel (n);
	}

	prh->instrument_info_change ();
}

void
Pianoroll::build_canvas ()
{
	EC_LOCAL_TEMPO_SCOPE;

	_canvas.set_background_color (UIConfiguration::instance().color ("arrange base"));
	_canvas.signal_event().connect (sigc::mem_fun (*this, &Pianoroll::canvas_pre_event), false);
	dynamic_cast<ArdourCanvas::GtkCanvas*>(&_canvas)->use_nsglview (UIConfiguration::instance().get_nsgl_view_mode () == NSGLHiRes);

	_canvas.PreRender.connect (sigc::mem_fun(*this, &EditingContext::pre_render));

	/* scroll group for items that should not automatically scroll
	 *  (e.g verbose cursor). It shares the canvas coordinate space.
	*/
	no_scroll_group = new ArdourCanvas::Container (_canvas.root());

	h_scroll_group = new ArdourCanvas::ScrollGroup (_canvas.root(), ArdourCanvas::ScrollGroup::ScrollsHorizontally);
	CANVAS_DEBUG_NAME (h_scroll_group, "pianoroll h scroll");
	_canvas.add_scroller (*h_scroll_group);


	v_scroll_group = new ArdourCanvas::ScrollGroup (_canvas.root(), ArdourCanvas::ScrollGroup::ScrollsVertically);
	CANVAS_DEBUG_NAME (v_scroll_group, "pianoroll v scroll");
	_canvas.add_scroller (*v_scroll_group);

	hv_scroll_group = new ArdourCanvas::ScrollGroup (_canvas.root(),
	                                                 ArdourCanvas::ScrollGroup::ScrollSensitivity (ArdourCanvas::ScrollGroup::ScrollsVertically|
		                ArdourCanvas::ScrollGroup::ScrollsHorizontally));
	CANVAS_DEBUG_NAME (hv_scroll_group, "pianoroll hv scroll");
	_canvas.add_scroller (*hv_scroll_group);

	cursor_scroll_group = new ArdourCanvas::ScrollGroup (_canvas.root(), ArdourCanvas::ScrollGroup::ScrollsHorizontally);
	CANVAS_DEBUG_NAME (cursor_scroll_group, "pianoroll cursor scroll");
	_canvas.add_scroller (*cursor_scroll_group);

	/*a group to hold global rects like punch/loop indicators */
	global_rect_group = new ArdourCanvas::Container (hv_scroll_group);
	CANVAS_DEBUG_NAME (global_rect_group, "pianoroll global rect group");

        transport_loop_range_rect = new ArdourCanvas::Rectangle (global_rect_group, ArdourCanvas::Rect (0.0, 0.0, 0.0, ArdourCanvas::COORD_MAX));
	CANVAS_DEBUG_NAME (transport_loop_range_rect, "pianoroll loop rect");
	transport_loop_range_rect->hide();

	/*a group to hold time (measure) lines */
	time_line_group = new ArdourCanvas::Container (h_scroll_group);
	CANVAS_DEBUG_NAME (time_line_group, "pianoroll time line group");

	n_timebars = 0;

#if 0 /* these can't be used for anything useful, so don't display them until they can */
	meter_bar = new ArdourCanvas::Rectangle (time_line_group, ArdourCanvas::Rect (0., 0, ArdourCanvas::COORD_MAX, timebar_height * (n_timebars+1)));
	CANVAS_DEBUG_NAME (meter_bar, "Meter Bar");
	meter_bar->set_fill(true);
	meter_bar->set_outline(true);
	meter_bar->set_outline_what(ArdourCanvas::Rectangle::BOTTOM);
	meter_bar->set_fill_color (UIConfiguration::instance().color_mod ("meter bar", "marker bar"));
	meter_bar->set_outline_color (UIConfiguration::instance().color ("marker bar separator"));
	meter_bar->set_outline_what (ArdourCanvas::Rectangle::BOTTOM);

	n_timebars++;

	tempo_bar = new ArdourCanvas::Rectangle (time_line_group, ArdourCanvas::Rect (0.0, timebar_height * n_timebars, ArdourCanvas::COORD_MAX, timebar_height * (n_timebars+1)));
	CANVAS_DEBUG_NAME (tempo_bar, "Tempo Bar");
	tempo_bar->set_fill(true);
	tempo_bar->set_outline(true);
	tempo_bar->set_outline_what(ArdourCanvas::Rectangle::BOTTOM);
	tempo_bar->set_fill_color (UIConfiguration::instance().color_mod ("tempo bar", "marker bar"));
	tempo_bar->set_outline_color (UIConfiguration::instance().color ("marker bar separator"));
	meter_bar->set_outline_what (ArdourCanvas::Rectangle::BOTTOM);

	n_timebars++;
#endif

	bbt_ruler = new ArdourCanvas::Ruler (time_line_group, &bbt_metric, ArdourCanvas::Rect (0, timebar_height * n_timebars, ArdourCanvas::COORD_MAX, timebar_height * (n_timebars+1)));
	bbt_ruler->set_font_description (UIConfiguration::instance().get_NormalBoldFont());
	bbt_ruler->set_minor_font_description (UIConfiguration::instance().get_SmallFont());
	Gtkmm2ext::Color base = UIConfiguration::instance().color ("ruler base");
	Gtkmm2ext::Color text = UIConfiguration::instance().color ("ruler text");
	bbt_ruler->set_fill_color (base);
	bbt_ruler->set_outline_color (text);
	CANVAS_DEBUG_NAME (bbt_ruler, "cue bbt ruler");

	n_timebars++;

	bbt_ruler->Event.connect (sigc::mem_fun (*this, &CueEditor::ruler_event));

	data_group = new ArdourCanvas::Container (hv_scroll_group);
	CANVAS_DEBUG_NAME (data_group, "cue data group");

	bg = new PianorollMidiBackground (data_group, *this);
	_canvas_viewport.signal_size_allocate().connect (sigc::mem_fun(*this, &Pianoroll::canvas_allocate), false);

	// used as rubberband rect
	rubberband_rect = new ArdourCanvas::Rectangle (data_group, ArdourCanvas::Rect (0.0, 0.0, 0.0, 0.0));
	rubberband_rect->hide();
	rubberband_rect->set_outline_color (UIConfiguration::instance().color ("rubber band rect"));
	rubberband_rect->set_fill_color (UIConfiguration::instance().color_mod ("rubber band rect", "selection rect"));
	CANVAS_DEBUG_NAME (rubberband_rect, X_("cue rubberband rect"));

	prh = new ArdourCanvas::PianoRollHeader (v_scroll_group, *bg);
	prh->SetNoteSelection.connect (sigc::mem_fun (*this, &Pianoroll::set_note_selection));
	prh->AddNoteSelection.connect (sigc::mem_fun (*this, &Pianoroll::add_note_selection));
	prh->ExtendNoteSelection.connect (sigc::mem_fun (*this, &Pianoroll::extend_note_selection));
	prh->ToggleNoteSelection.connect (sigc::mem_fun (*this, &Pianoroll::toggle_note_selection));

	view = new PianorollMidiView (nullptr, *data_group, *no_scroll_group, *this, *bg, 0xff0000ff);
	view->AutomationStateChange.connect (sigc::mem_fun (*this, &Pianoroll::automation_state_changed));
	view->VisibleChannelChanged.connect (view_connections, invalidator (*this), std::bind (&Pianoroll::visible_channel_changed, this), gui_context());
	view->set_show_source (show_source);

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

	_verbose_cursor.reset (new VerboseCursor (*this));

	// _playhead_cursor = new EditorCursor (*this, &Editor::canvas_playhead_cursor_event, X_("playhead"));
	_playhead_cursor = new EditorCursor (*this, X_("playhead"));
	_playhead_cursor->set_sensitive (UIConfiguration::instance().get_sensitize_playhead());
	_playhead_cursor->set_color (UIConfiguration::instance().color ("play head"));
	_playhead_cursor->canvas_item().raise_to_top();
	h_scroll_group->raise_to_top ();

	_canvas.set_name ("MidiCueCanvas");
	_canvas.add_events (Gdk::POINTER_MOTION_HINT_MASK | Gdk::SCROLL_MASK | Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);
	_canvas.set_can_focus ();
	_canvas.signal_show().connect (sigc::mem_fun (*this, &CueEditor::catch_pending_show_region));

	_toolbox.pack_start (_canvas_viewport, true, true);
}

void
Pianoroll::visible_channel_changed ()
{
	EC_LOCAL_TEMPO_SCOPE;

	if (ignore_channel_changes) {
		/* We're changing it */
		return;
	}

	/* Something else changed it */

	if (!view) {
		return; /* Ought to be impossible */
	}

	_visible_channel = view->visible_channel();
	visible_channel_selector.set_active (string_compose ("%1", view->visible_channel() + 1));
}

void
Pianoroll::bindings_changed ()
{
	EC_LOCAL_TEMPO_SCOPE;

	bindings.clear ();
	load_shared_bindings ();
}

void
Pianoroll::maybe_update ()
{
	EC_LOCAL_TEMPO_SCOPE;

	ARDOUR::TriggerPtr playing_trigger;

	if (ref.trigger()) {

		/* Trigger editor */

		playing_trigger = ref.box()->currently_playing ();

		if (!playing_trigger) {

			if (_drags->active() || !view || !_track || !_track->triggerbox()) {
				return;
			}

			if (_track->triggerbox()->record_enabled() == Recording) {
				_playhead_cursor->set_position (data_capture_duration);
			}

		} else {
			if (playing_trigger->active ()) {
				if (playing_trigger->the_region()) {
					_playhead_cursor->set_position (playing_trigger->current_pos().samples() + playing_trigger->the_region()->start().samples());
				}
			} else {
				_playhead_cursor->set_position (0);
			}
		}

	} else if (view->midi_region()) {

		Temporal::TempoMap::SharedPtr global_tempo_map (Temporal::TempoMap::global_fetch());
		Temporal::TempoMap::SharedPtr local_tempo_map (Temporal::TempoMap::use());

		/* Timeline region editor */

		if (!_session) {
			return;
		}

		samplepos_t pos = _session->audible_sample();

		/* find out the beat time represented by pos in the global map,
		 * convert back to sample position with the local map
		 */

		pos = local_tempo_map->sample_at (global_tempo_map->quarters_at (timepos_t (pos)));

		/* Do the same for the source position */

		samplepos_t spos = local_tempo_map->sample_at (global_tempo_map->quarters_at (view->midi_region()->source_position()));

		if (pos < spos) {
			_playhead_cursor->set_position (0);
		} else {
			_playhead_cursor->set_position (pos - spos);
		}

	} else {
		_playhead_cursor->set_position (0);
	}

	assert (_session);
	if (_session && _session->transport_rolling() && follow_playhead() && !_scroll_drag) {
		reset_x_origin_to_follow_playhead ();
	}
}

bool
Pianoroll::canvas_enter_leave (GdkEventCrossing* ev)
{
	EC_LOCAL_TEMPO_SCOPE;

	switch (ev->type) {
	case GDK_ENTER_NOTIFY:
		if (ev->detail != GDK_NOTIFY_INFERIOR) {
			_canvas.grab_focus ();
			ActionManager::set_sensitive (_midi_actions, true);
			within_track_canvas = true;
		}
		break;
	case GDK_LEAVE_NOTIFY:
		if (ev->detail != GDK_NOTIFY_INFERIOR) {
			ActionManager::set_sensitive (_midi_actions, false);
			within_track_canvas = false;
			ARDOUR_UI::instance()->reset_focus (&_canvas_viewport);
			gdk_window_set_cursor (_canvas_viewport.get_window()->gobj(), nullptr);
		}
	default:
		break;
	}
	return false;
}

void
Pianoroll::canvas_allocate (Gtk::Allocation alloc)
{
	EC_LOCAL_TEMPO_SCOPE;

	_visible_canvas_width = alloc.get_width();
	_visible_canvas_height = alloc.get_height();

	double timebars = n_timebars * timebar_height;
	bg->set_size (alloc.get_width(), alloc.get_height() - timebars);
	view->set_height (alloc.get_height() - timebars);
	prh->set (ArdourCanvas::Rect (0, 0, prh->x1(), view->midi_context().height()));

	_track_canvas_width = _visible_canvas_width - prh->x1();
	_timeline_origin = prh->x1();

	data_group->set_position (ArdourCanvas::Duple (_timeline_origin, timebar_height * n_timebars));
	no_scroll_group->set_position (ArdourCanvas::Duple (_timeline_origin, timebar_height * n_timebars));
	cursor_scroll_group->set_position (ArdourCanvas::Duple (_timeline_origin, timebar_height * n_timebars));
	h_scroll_group->set_position (Duple (_timeline_origin, 0.));

	if (zoom_in_allocate) {

		zoom_to_show (max_zoom_extent());
		if (_region) {
			bg->display_region (*view);
		}
		zoom_in_allocate = false;
	}

	update_grid ();

	instant_save ();
}

timepos_t
Pianoroll::snap_to_grid (timepos_t const & presnap, Temporal::RoundMode direction, SnapPref gpref) const
{
	EC_LOCAL_TEMPO_SCOPE;

	/* BBT time only */
	return snap_to_bbt (presnap, direction, gpref);
}

void
Pianoroll::snap_to_internal (timepos_t& start, Temporal::RoundMode direction, SnapPref pref, bool ensure_snap) const
{
	EC_LOCAL_TEMPO_SCOPE;

	UIConfiguration const& uic (UIConfiguration::instance ());

	timepos_t post (snap_to_grid (start, direction, pref));

	/* now check "magnetic" state: is the grid within reasonable on-screen distance to trigger a snap?
	 * this also helps to avoid snapping to somewhere the user can't see.  (i.e.: I clicked on a region and it disappeared!!)
	 * ToDo: Perhaps this should only occur if EditPointMouse?
	 */
	samplecnt_t snap_threshold_s = pixel_to_sample (uic.get_snap_threshold ());

	if (!ensure_snap && ::llabs (post.distance (start).samples()) > snap_threshold_s) {
		return;
	}

	start = post;
}

void
Pianoroll::set_samples_per_pixel (samplecnt_t spp)
{
	EC_LOCAL_TEMPO_SCOPE;

	assert (spp > 0);
#ifndef NDEBUG
	if (spp < 1) {
		spp = 1;
	}
#endif

	CueEditor::set_samples_per_pixel (spp);

	if (view) {
		view->set_samples_per_pixel (spp);
	}

	update_tempo_based_rulers ();

	horizontal_adjustment.set_upper (max_zoom_extent().second.samples() / samples_per_pixel);
	horizontal_adjustment.set_page_size (current_page_samples()/ samples_per_pixel / 10);
	horizontal_adjustment.set_page_increment (current_page_samples()/ samples_per_pixel / 20);
	horizontal_adjustment.set_step_increment (current_page_samples() / samples_per_pixel / 100);
}

samplecnt_t
Pianoroll::current_page_samples() const
{
	EC_LOCAL_TEMPO_SCOPE;

	return (samplecnt_t) _track_canvas_width * samples_per_pixel;
}

bool
Pianoroll::canvas_bg_event (GdkEvent* event, ArdourCanvas::Item* item)
{
	EC_LOCAL_TEMPO_SCOPE;

	return typed_event (item, event, RegionItem);
}

bool
Pianoroll::canvas_control_point_event (GdkEvent* event, ArdourCanvas::Item* item, ControlPoint* cp)
{
	EC_LOCAL_TEMPO_SCOPE;

	return typed_event (item, event, ControlPointItem);
}

bool
Pianoroll::canvas_note_event (GdkEvent* event, ArdourCanvas::Item* item)
{
	EC_LOCAL_TEMPO_SCOPE;

	return typed_event (item, event, NoteItem);
}

bool
Pianoroll::canvas_velocity_base_event (GdkEvent* event, ArdourCanvas::Item* item)
{
	EC_LOCAL_TEMPO_SCOPE;

	return typed_event (item, event, VelocityBaseItem);
}

bool
Pianoroll::canvas_velocity_event (GdkEvent* event, ArdourCanvas::Item* item)
{
	EC_LOCAL_TEMPO_SCOPE;

	return typed_event (item, event, VelocityItem);
}

bool
Pianoroll::canvas_cue_start_event (GdkEvent* event, ArdourCanvas::Item* item)
{
	EC_LOCAL_TEMPO_SCOPE;

	return typed_event (item, event, ClipStartItem);
}

bool
Pianoroll::canvas_cue_end_event (GdkEvent* event, ArdourCanvas::Item* item)
{
	EC_LOCAL_TEMPO_SCOPE;

	return typed_event (item, event, ClipEndItem);
}

Gtk::Widget&
Pianoroll::contents ()
{
	EC_LOCAL_TEMPO_SCOPE;

	return _contents;
}

bool
Pianoroll::idle_data_captured ()
{
	EC_LOCAL_TEMPO_SCOPE;

	if (!ref.box()) {
		return false;
	}

	CueEditor::idle_data_captured ();

	if (view) {
		view->clip_data_recorded (data_capture_duration);
	}

	return false;
}

bool
Pianoroll::button_press_handler (ArdourCanvas::Item* item, GdkEvent* event, ItemType item_type)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (event->type != GDK_BUTTON_PRESS) {
		return false;
	}

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
	EC_LOCAL_TEMPO_SCOPE;

	NoteBase* note = nullptr;
	Editing::MouseMode mouse_mode = current_mouse_mode();
	switch (item_type) {
	case NoteItem:
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
			_drags->set (new AutomationDrawDrag (*this, nullptr, *static_cast<ArdourCanvas::Rectangle*>(item), false, Temporal::BeatTime,
			                                     [&](GdkEvent* ev, timepos_t const & pos) { return view->automation_rb_click (ev, pos); }), event);
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
			_drags->set (new ClipStartDrag (*this, *r), event);
		}
		return true;
		break;
	}

	case ClipEndItem: {
		ArdourCanvas::Rectangle* r = dynamic_cast<ArdourCanvas::Rectangle*> (item);
		if (r) {
			_drags->set (new ClipEndDrag (*this, *r), event);
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
	EC_LOCAL_TEMPO_SCOPE;

	return true;
}

bool
Pianoroll::button_release_handler (ArdourCanvas::Item* item, GdkEvent* event, ItemType item_type)
{
	NoteBase* e;

	EC_LOCAL_TEMPO_SCOPE;

	if (!Keyboard::is_context_menu_event (&event->button)) {

		/* see if we're finishing a drag */

		if (_drags->active ()) {
			bool const r = _drags->end_grab (event);
			if (r) {
				/* grab dragged, so do nothing else */
				return true;
			}
		}

		if (event->button.button == 2) {
			switch (current_mouse_mode()) {
			case Editing::MouseContent:
			case Editing::MouseDraw:
				switch (item_type) {
				case NoteItem:
					e = reinterpret_cast<NoteBase*> (item->get_data ("notebase"));
					assert (e);
					if (midi_view()) {
						midi_view()->delete_note (e->note());
					}
					return true;
				default:
					break;
				}
				break;
			default:
				break;
			}
			return true;
		}

	} else {

		switch (item_type) {
		case NoteItem:
			if (internal_editing()) {
				popup_note_context_menu (item, event);
				return true;
			}
			break;
		case RegionItem:
			if (internal_editing()) {
				popup_region_context_menu (item, event);
				return true;
			}
			break;
		default:
			break;
		}

		popup_note_context_menu (item, event);
		return true;
	}

	return false;
}

void
Pianoroll::popup_region_context_menu (ArdourCanvas::Item* item, GdkEvent* event)
{
	EC_LOCAL_TEMPO_SCOPE;

	using namespace Gtk::Menu_Helpers;

	if (!view) {
		return;
	}

	const uint32_t sel_size = view->selection_size ();
	MidiViews mvs ({view});

	MenuList& items = _region_context_menu.items();
	items.clear();

	if (sel_size > 0) {
		items.push_back (MenuElem(_("Delete"), sigc::mem_fun (*view, &MidiView::delete_selection)));
	}

	items.push_back(MenuElem(_("Edit..."), sigc::bind(sigc::mem_fun(*this, &EditingContext::edit_notes), view)));
	items.push_back(MenuElem(_("Transpose..."),  sigc::bind(sigc::mem_fun(*this, &EditingContext::transpose_regions), mvs)));
	items.push_back(MenuElem(_("Legatize"), sigc::bind(sigc::mem_fun(*this, &EditingContext::legatize_regions), mvs, false)));
	if (sel_size < 2) {
		items.back().set_sensitive (false);
	}
	items.push_back(MenuElem(_("Quantize..."), sigc::bind(sigc::mem_fun(*this, &EditingContext::quantize_regions), mvs)));
	items.push_back(MenuElem(_("Remove Overlap"), sigc::bind(sigc::mem_fun(*this, &EditingContext::legatize_regions), mvs, true)));
	if (sel_size < 2) {
		items.back().set_sensitive (false);
	}
	items.push_back(MenuElem(_("Transform..."), sigc::bind(sigc::mem_fun(*this, &EditingContext::transform_regions), mvs)));

	_region_context_menu.popup (event->button.button, event->button.time);
}

bool
Pianoroll::button_press_dispatch (GdkEventButton* ev)
{
	EC_LOCAL_TEMPO_SCOPE;

	/* this function is intended only for buttons 4 and above. */

	Gtkmm2ext::MouseButton b (ev->state, ev->button);
	return button_bindings->activate (b, Gtkmm2ext::Bindings::Press);
}

bool
Pianoroll::button_release_dispatch (GdkEventButton* ev)
{
	EC_LOCAL_TEMPO_SCOPE;

	/* this function is intended only for buttons 4 and above. */

	Gtkmm2ext::MouseButton b (ev->state, ev->button);
	return button_bindings->activate (b, Gtkmm2ext::Bindings::Release);
}

bool
Pianoroll::motion_handler (ArdourCanvas::Item*, GdkEvent* event, bool from_autoscroll)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (_drags->active ()) {
		//drags change the snapped_cursor location, because we are snapping the thing being dragged, not the actual mouse cursor
		return _drags->motion_handler (event, from_autoscroll);
	}

	return true;
}

bool
Pianoroll::key_press_handler (ArdourCanvas::Item*, GdkEvent* ev, ItemType)
{
	EC_LOCAL_TEMPO_SCOPE;


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
	EC_LOCAL_TEMPO_SCOPE;

	return true;
}

void
Pianoroll::set_mouse_mode (Editing::MouseMode m, bool force)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (m != Editing::MouseDraw && m != Editing::MouseContent) {
		return;
	}

	EditingContext::set_mouse_mode (m, force);
}

void
Pianoroll::midi_action (void (MidiView::*method)())
{
	EC_LOCAL_TEMPO_SCOPE;

	if (!view) {
		return;
	}

	(view->*method) ();
}

void
Pianoroll::escape ()
{
	EC_LOCAL_TEMPO_SCOPE;

	if (!view) {
		return;
	}

	view->clear_selection ();
}

Gdk::Cursor*
Pianoroll::which_track_cursor () const
{
	EC_LOCAL_TEMPO_SCOPE;

	return _cursors->grabber;
}

Gdk::Cursor*
Pianoroll::which_mode_cursor () const
{
	EC_LOCAL_TEMPO_SCOPE;

	Gdk::Cursor* mode_cursor = MouseCursors::invalid_cursor ();

	switch (current_mouse_mode()) {
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
	EC_LOCAL_TEMPO_SCOPE;

	abort ();
	/*NOTREACHED*/
	return nullptr;
}


Gdk::Cursor*
Pianoroll::which_canvas_cursor (ItemType type) const
{
	EC_LOCAL_TEMPO_SCOPE;

	Gdk::Cursor* cursor = which_mode_cursor ();
	Editing::MouseMode mouse_mode = current_mouse_mode ();

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
	EC_LOCAL_TEMPO_SCOPE;

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
	EC_LOCAL_TEMPO_SCOPE;

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
	EC_LOCAL_TEMPO_SCOPE;

	if (view) {
		return view->selectable_owners();
	}

	return std::list<SelectableOwner*> ();
}

void
Pianoroll::trigger_prop_change (PBD::PropertyChange const & what_changed)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (what_changed.contains (Properties::region)) {
		std::shared_ptr<MidiRegion> mr = std::dynamic_pointer_cast<MidiRegion> (ref.trigger()->the_region());
		set_region (mr);
	}
}

void
Pianoroll::make_a_region ()
{
	EC_LOCAL_TEMPO_SCOPE;

	std::shared_ptr<MidiSource> new_source = _session->create_midi_source_for_session (_track->name());
	SourceList sources;
	sources.push_back (new_source);

	PropertyList plist;
	plist.add (ARDOUR::Properties::start, timepos_t (Temporal::Beats ()));
	plist.add (ARDOUR::Properties::length, timepos_t (Temporal::Beats::beats (32)));
	plist.add (ARDOUR::Properties::name, new_source->name());
	plist.add (ARDOUR::Properties::whole_file, true);

	std::shared_ptr<MidiRegion> mr = std::dynamic_pointer_cast<MidiRegion> (RegionFactory::create (sources, plist, true));

	plist.remove (ARDOUR::Properties::whole_file);
	mr = std::dynamic_pointer_cast<MidiRegion> (RegionFactory::create (mr, timecnt_t::zero (Temporal::BeatTime), plist, true));

	if (ref.trigger()) {
		ref.trigger()->set_region (mr);
	}

	set_region (mr);
}

void
Pianoroll::unset_region ()
{
	CueEditor::unset_region ();
	view->set_region (nullptr);
}

void
Pianoroll::unset_trigger ()
{
	CueEditor::unset_trigger ();
}

void
Pianoroll::build_cc_menu (ArdourWidgets::MetaButton* ccbtn)
{
	if (!ccbtn->menu().items().empty () || !_track) {
		return;
	}

	/* note this can take a long time, and also is not entirely correct.
	 * ::add_multi_controller_item() add items directly to the top-level
	 * while keeping empty sub-menus for grouped controls 1-31, 32-64, etc.
	 */
	build_controller_menu (ccbtn->menu(), _track->instrument_info(), 0xffff,
	                       sigc::bind (sigc::mem_fun (*this, &Pianoroll::add_single_controller_item), ccbtn),
	                       sigc::bind (sigc::mem_fun (*this, &Pianoroll::add_multi_controller_item), ccbtn),  12);
	// reset_user_cc_choice (Evoral::Parameter (ARDOUR::MidiCCAutomation, _visible_channel, MIDI_CTL_MSB_GENERAL_PURPOSE1), ccbtn);
}

void
Pianoroll::set_track (std::shared_ptr<ARDOUR::Track> track)
{
	EC_LOCAL_TEMPO_SCOPE;

	CueEditor::set_track (track);

	if (view) {
		view->set_track (std::dynamic_pointer_cast<MidiTrack> (track));
	}

	if (_track == track) {
		return;
	}

#ifdef PIANOROLL_USER_BUTTONS
	cc_dropdown1->menu().items().clear ();
	cc_dropdown2->menu().items().clear ();
	cc_dropdown3->menu().items().clear ();

	if (cc_dropdown1->get_mapped ()) {
		build_cc_menu (cc_dropdown1);
	}
	if (cc_dropdown2->get_mapped ()) {
		build_cc_menu (cc_dropdown2);
	}
	if (cc_dropdown3->get_mapped ()) {
		build_cc_menu (cc_dropdown3);
	}
#endif
}

void
Pianoroll::set_region (std::shared_ptr<ARDOUR::Region> region)
{
	CueEditor::set_region (region);

	if (_visible_pending_region) {
		return;
	}

	std::shared_ptr<MidiRegion> r (std::dynamic_pointer_cast<ARDOUR::MidiRegion> (region));

	if (!r) {
		view->set_region (nullptr);
		_update_connection.disconnect ();
		return;
	}

	view->set_region (r);
	view->show_start (true);
	view->show_end (true);

	layered_automation_button->set_active_state (Gtkmm2ext::Off);
	layered_automation = false;

	set_visible_channel (view->pick_visible_channel());

	/* Compute zoom level to show entire source plus some margin if possible */

	zoom_to_show (max_zoom_extent());

	bg->display_region (*view);

	maybe_set_from_rsu ();

	if (r->source()->empty()) {
		std::shared_ptr<MidiTrack> mt (std::dynamic_pointer_cast<ARDOUR::MidiTrack> (_track));
		if (mt) {
			note_mode_actions[mt->note_mode()]->set_active (true);
		}
	}
}

bool
Pianoroll::user_automation_active_button_click (GdkEventButton* ev, MetaButton* mb)
{
#ifdef PIANOROLL_USER_BUTTONS
	EC_LOCAL_TEMPO_SCOPE;

	if (mb->is_menu_popup_event (ev)) {
		return false;
	}

	if (mb->is_led_click (ev)) {
		return false;
	}

	ParameterButtonMap::iterator i = parameter_button_map.find (mb);

	if (i == parameter_button_map.end()) {
		return false;
	}

	if (view) {
		view->set_active_automation (i->second);
	}

#endif
	return true;
}

void
Pianoroll::user_automation_show_button_click (GdkEventButton* ev, MetaButton* metabutton)
{
#ifdef PIANOROLL_USER_BUTTONS
	EC_LOCAL_TEMPO_SCOPE;

	if (ev->button != 1) {
		return;
	}

	ParameterButtonMap::iterator i = parameter_button_map.find (metabutton);

	if (i == parameter_button_map.end()) {
		return;
	}

	automation_active_button_click (ev, i->second.type(), i->second.id());
#endif
}

void
Pianoroll::automation_active_button_click (Evoral::ParameterType type, int id)
{
	if (!view)  {
		return;
	}

	EC_LOCAL_TEMPO_SCOPE;

	Evoral::Parameter p (type, _visible_channel, id);

	if (view->is_active_automation (p)) {
		view->unset_active_automation ();
		return;
	}

	if (!layered_automation && !view->is_visible_automation (p)) {
		view->hide_all_automation ();
	}

	view->set_active_automation (p);
}

void
Pianoroll::automation_show_button_click (Evoral::ParameterType type, int id)
{
	if (!view)  {
		return;
	}

	EC_LOCAL_TEMPO_SCOPE;

	Evoral::Parameter param (type, _visible_channel, id);
	if (!layered_automation && !view->is_visible_automation (param)) {
		/* Param is about to become visible, hide everything else */
		view->hide_all_automation ();
	}
	view->toggle_visibility (param);
}

void
Pianoroll::automation_state_changed ()
{
	EC_LOCAL_TEMPO_SCOPE;

	assert (view);

	for (ParameterButtonMap::iterator i = parameter_button_map.begin(); i != parameter_button_map.end(); ++i) {
		std::string str (ARDOUR::EventTypeMap::instance().to_symbol (i->second));

		/* Indicate active automation state with selected/not-selected visual state */

		if (view->is_active_automation (i->second)) {
			i->first->set_editing (true);
		} else {
			i->first->set_editing (false);
		}

		/* Indicate visible automation state with explicit widget active state (LED) */

		if (view->is_visible_automation (i->second)) {
			i->first->set_showing (true);
		} else {
			i->first->set_showing (false);
		}
	}
}

ARDOUR::NoteMode
Pianoroll::note_mode () const
{
	return bg->note_mode();
}

void
Pianoroll::note_mode_chosen (ARDOUR::NoteMode mode)
{
	EC_LOCAL_TEMPO_SCOPE;

	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

	Glib::RefPtr<Gtk::RadioAction> ract = note_mode_actions[mode];

	if (!ract->get_active()) {
		return;
	}

	if (mode != bg->note_mode()) {
		bg->set_note_mode (mode);
		if (bg->note_mode() == Percussive) {
			note_mode_button.set_active_state (Gtkmm2ext::ExplicitActive);
		} else {
			note_mode_button.set_active_state (Gtkmm2ext::Off);
		}
	}

	instant_save ();
}

void
Pianoroll::note_mode_clicked ()
{
	EC_LOCAL_TEMPO_SCOPE;

	assert (bg);


	if (bg->note_mode() == Sustained) {
		note_mode_actions[Percussive]->set_active (true);
	} else {
		note_mode_actions[Sustained]->set_active (true);
	}
}

void
Pianoroll::point_selection_changed ()
{
	EC_LOCAL_TEMPO_SCOPE;

	if (view) {
		view->point_selection_changed ();
	}
}

void
Pianoroll::delete_ ()
{
	EC_LOCAL_TEMPO_SCOPE;

	/* Editor has a lot to do here, potentially. But we don't */
	cut_copy (Editing::Delete);
}

void
Pianoroll::paste (float times, bool from_context_menu)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (view) {
		// view->paste (Editing::Cut);
	}
}

void
Pianoroll::keyboard_paste ()
{
	if (!view || !_region) {
		return;
	}

	EC_LOCAL_TEMPO_SCOPE;

	timepos_t where (get_preferred_edit_position (Editing::EDIT_IGNORE_NONE, false, false));
	timepos_t absolute_where = _region->region_beats_to_absolute_time (where.beats());

	PasteContext pc (0, 1, ItemCounts(), true);
	begin_reversible_command (string_compose (_("paste %1"), X_("MIDI")));
	view->paste (absolute_where, get_cut_buffer(), pc);
	commit_reversible_command ();
}

/** Cut, copy or clear selected regions, automation points or a time range.
 * @param op Operation (Delete, Cut, Copy or Clear)
 */

void
Pianoroll::cut_copy (Editing::CutCopyOp op)
{
	EC_LOCAL_TEMPO_SCOPE;

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

	switch (current_mouse_mode()) {
	case MouseDraw:
	case MouseContent:
		if (view) {
			begin_reversible_command (opname + ' ' + X_("MIDI"));
			view->cut_copy_clear (*selection, op);
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
	EC_LOCAL_TEMPO_SCOPE;

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

void
Pianoroll::set_session (ARDOUR::Session* s)
{
	EC_LOCAL_TEMPO_SCOPE;

	CueEditor::set_session (s);

	if (with_transport_controls) {
		if (_session) {
			_session->TransportStateChange.connect (_session_connections, MISSING_INVALIDATOR, std::bind (&Pianoroll::map_transport_state, this), gui_context());
		} else {
			_session_connections.drop_connections();
		}

		map_transport_state ();
	}

	if (_session) {
		zoom_to_show (max_zoom_extent());
	}
}

void
Pianoroll::session_going_away ()
{
	_update_connection.disconnect ();
	CueEditor::session_going_away ();
}

void
Pianoroll::map_transport_state ()
{
	EC_LOCAL_TEMPO_SCOPE;

	if (!_session) {
		loop_button.unset_active_state ();
		play_button.unset_active_state ();
		return;
	}

	if (_session->transport_rolling()) {

		/* we're rolling */

		if (_session->get_play_loop ()) {

			loop_button.set_active (true);

			if (Config->get_loop_is_mode()) {
				play_button.set_active (true);
			} else {
				play_button.set_active (false);
			}
		} else {
			play_button.set_active (true);
			loop_button.set_active (false);
		}
	} else {
		play_button.set_active (false);

		if (Config->get_loop_is_mode()) {
			loop_button.set_active (true);
		} else {
			loop_button.set_active (false);
		}

		hide_count_in ();
	}
}

bool
Pianoroll::allow_trim_cursors () const
{
	EC_LOCAL_TEMPO_SCOPE;

	auto mouse_mode = current_mouse_mode ();
	return mouse_mode == Editing::MouseContent || mouse_mode == Editing::MouseTimeFX;
}

void
Pianoroll::shift_contents (timepos_t const & t, bool model)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (!view) {
		return;
	}

	view->shift_midi (t, model);
}

InstrumentInfo*
Pianoroll::instrument_info () const
{
	EC_LOCAL_TEMPO_SCOPE;

	if (!view || !view->midi_track()) {
		return nullptr;
	}

	return &view->midi_track()->instrument_info ();
}

void
Pianoroll::update_tempo_based_rulers ()
{
	EC_LOCAL_TEMPO_SCOPE;

	if (!_session) {
		return;
	}

	bbt_metric.units_per_pixel = samples_per_pixel;
	compute_bbt_ruler_scale (_leftmost_sample, _leftmost_sample + current_page_samples());
	bbt_ruler->set_range (_leftmost_sample, _leftmost_sample+current_page_samples());
}

void
Pianoroll::set_note_selection (uint8_t note)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (!view) {
		return;
	}

	uint16_t chn_mask = view->midi_track()->get_playback_channel_mask();

	begin_reversible_selection_op (X_("Set Note Selection"));
	view->select_matching_notes (note, chn_mask, false, false);
	commit_reversible_selection_op();
}

void
Pianoroll::add_note_selection (uint8_t note)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (!view) {
		return;
	}

	const uint16_t chn_mask = view->midi_track()->get_playback_channel_mask();

	begin_reversible_selection_op (X_("Add Note Selection"));
	view->select_matching_notes (note, chn_mask, true, false);
	commit_reversible_selection_op();
}

void
Pianoroll::extend_note_selection (uint8_t note)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (!view) {
		return;
	}

	const uint16_t chn_mask = view->midi_track()->get_playback_channel_mask();

	begin_reversible_selection_op (X_("Extend Note Selection"));
	view->select_matching_notes (note, chn_mask, true, true);
	commit_reversible_selection_op();
}

void
Pianoroll::toggle_note_selection (uint8_t note)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (!view) {
		return;
	}

	const uint16_t chn_mask = view->midi_track()->get_playback_channel_mask();

	begin_reversible_selection_op (X_("Toggle Note Selection"));
	view->toggle_matching_notes (note, chn_mask);
	commit_reversible_selection_op();
}

void
Pianoroll::set_note_highlight (uint8_t note)
{
	if (prh) {
		prh->set_note_highlight (note);
	}
}

void
Pianoroll::begin_write ()
{
	EC_LOCAL_TEMPO_SCOPE;

	if (view) {
		view->begin_write ();
	}
}

void
Pianoroll::end_write ()
{
	EC_LOCAL_TEMPO_SCOPE;

	if (view) {
		view->end_write ();
	}
}

void
Pianoroll::manage_possible_header (Gtk::Allocation& alloc)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (prh) {
		double w, h;
		prh->size_request (w, h);
		alloc.set_width (alloc.get_width() - w);
		alloc.set_x (alloc.get_x() + w);
	}
}

void
Pianoroll::show_count_in (std::string const & str)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (view) {
		view->set_overlay_text (str);
	}
}

void
Pianoroll::hide_count_in ()
{
	EC_LOCAL_TEMPO_SCOPE;

	if (view) {
		view->hide_overlay_text ();
	}
}

void
Pianoroll::set_from_rsu (RegionUISettings& region_ui_settings)
{
	note_mode_actions[region_ui_settings.note_mode]->set_active (true);
	CueEditor::set_from_rsu (region_ui_settings);
}

void
Pianoroll::instant_save ()
{
	EC_LOCAL_TEMPO_SCOPE;

	region_ui_settings.draw_length = draw_length();
	region_ui_settings.draw_velocity = draw_velocity();
	region_ui_settings.channel = draw_channel();
	region_ui_settings.note_min = bg->lowest_note ();
	region_ui_settings.note_max = bg->highest_note();
	region_ui_settings.note_mode = note_mode ();

	CueEditor::instant_save ();
}

void
Pianoroll::parameter_changed (std::string param)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (param == X_("note-name-display")) {
		if (prh) {
			prh->instrument_info_change ();
		}
	}
}

timepos_t
Pianoroll::source_to_timeline (timepos_t const & source_pos) const
{
	assert (midi_view());

	if (midi_view()->show_source()) {
		return midi_view()->source_beats_to_timeline (source_pos.beats());
	}

	return source_pos;
}

Gtk::Menu*
Pianoroll::get_single_region_context_menu ()
{
	using namespace Gtk;
	using namespace Menu_Helpers;

	Menu* m = new Menu;
	MenuList& items (m->items());

	items.push_back (MenuElem (_("Quantize..."), sigc::mem_fun (*this, &EditingContext::quantize_region)));
	items.push_back (MenuElem (_("Legatize"), sigc::bind(sigc::mem_fun (*this, &EditingContext::legatize_region), false)));
	items.push_back (MenuElem (_("Transform..."), sigc::mem_fun (*this, &EditingContext::transform_region)));
	items.push_back (MenuElem (_("Remove Overlap"), sigc::bind(sigc::mem_fun (*this, &EditingContext::legatize_region), true)));
	// items.push_back (MenuElem (_("Insert Patch Change..."), sigc::bind (sigc::mem_fun (*this, &EditingContext::insert_patch_change), false)));
	// items.push_back (MenuElem (_("Insert Patch Change..."), sigc::bind (sigc::mem_fun (*this, &EditingContext::insert_patch_change), true)));

	return m;
}

EditingContext::MidiViews
Pianoroll::midiviews_from_region_selection (RegionSelection const &) const
{
	/* there is no region selection */

	MidiViews mv;

	if (midi_view()) {
		mv.push_back (midi_view());
	}

	return mv;
}

/*----*/

using namespace ArdourWidgets;

ControllerControls::ControllerControls (int num, std::string const & str, Gtk::RadioButtonGroup& group)
	: number (num)
{
	using namespace Gtk;

	ArdourButton::Element elements = ArdourButton::Element (ArdourButton::VectorIcon|ArdourButton::Edge|ArdourButton::Body);

	show_hide_button = new ArdourButton (elements);
	edit_button = new ArdourButton (elements);
	name.set_text (str);

	show_hide_button->set_icon (ArdourIcon::HideEye);
	edit_button->set_icon (ArdourIcon::ToolDraw);

	show_hide_button->signal_clicked.connect (sigc::mem_fun (show_clicked, &sigc::signal<void>::emit));
	edit_button->signal_clicked.connect (sigc::mem_fun (edit_clicked, &sigc::signal<void>::emit));

	Gtkmm2ext::Color c = UIConfiguration::instance().color (X_("alert:yellow"));
	show_hide_button->set_active_color (c);
	show_hide_button->set_act_on_release (false);
	show_hide_button->set_fallthrough_to_parent (false);

	edit_button->set_active_color (c);
	edit_button->set_act_on_release (false);
	edit_button->set_fallthrough_to_parent (false);

	set_spacing (6);
	pack_start (*show_hide_button, false, false);
	pack_start (*edit_button, false, false);
	pack_start (name, false, false, 6);
	show_all ();
}

ControllerControls::~ControllerControls ()
{
}

bool
ControllerControls::showing() const
{
	return show_hide_button->active_state() != Gtkmm2ext::Off;
}

bool
ControllerControls::editing() const
{
	return edit_button->get_active();
}

void
ControllerControls::set_showing (bool yn)
{
	show_hide_button->set_active_state (yn ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	if (!yn) {
		show_hide_button->set_icon (ArdourIcon::EditorShowAutoOnTouch);
	} else {
		show_hide_button->set_icon (ArdourIcon::HideEye);
	}
}

void
ControllerControls::set_editing (bool yn)
{
	edit_button->set_active_state (yn ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}
