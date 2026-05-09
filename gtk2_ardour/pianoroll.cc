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
#include "ardour/quantize.h"
#include "ardour/region_factory.h"
#include "ardour/smf_source.h"

#include "canvas/box.h"
#include "canvas/button.h"
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
#include "canvas_icon.h"
#include "chord_box.h"
#include "control_point.h"
#include "cross_cursor.h"
#include "editing_convert.h"
#include "editor_cursors.h"
#include "editor_drag.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "midi_inspector.h"
#include "midi_util.h"
#include "paste_context.h"
#include "pianoroll_background.h"
#include "pianoroll.h"
#include "pianoroll_midi_view.h"
#include "pitch_color_dialog.h"
#include "public_editor.h"
#include "quantize_dialog.h"
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

std::map<std::string,std::string> Pianoroll::controller_name_map;


Pianoroll::Pianoroll (std::string const & name, bool with_transport, bool expandabl)
	: CueEditor (name, with_transport)
	, prh (nullptr)
	, _editing_policy (ActiveView)
	, _color_mode (UIConfiguration::instance().get_default_midi_note_color_mode())
	, size_button (ArdourButton::default_elements, true)
	, automation_button (_("A"))
	, expandable (expandabl)
	, no_toggle (false)
	, bg (nullptr)
	, _active_view (nullptr)
	, bbt_metric (*this)
	, ignore_channel_changes (false)
	, xcursor (nullptr)
	, midi_inspector (nullptr)
	, inspector_scroller (nullptr)
	, inspector_button (_("<"))
{
	if (controller_name_map.empty()) {
		build_midi_controller_name_map ();
	}

	autoscroll_vertical_allowed = false;

	load_bindings ();
	register_actions ();

	size_button.set_icon (ArdourIcon::ZoomFull);
	size_button.signal_clicked.connect ([&]() { toggle_size (); });

	automation_button.signal_clicked.connect ([&]() { automation_button_clicked(); });

	using namespace Gtk::Menu_Helpers;

	policy_dropdown.add_menu_elem (MenuElem (_("All Regions"), sigc::bind (sigc::mem_fun (*this, &Pianoroll::set_editing_policy), AllViews)));
	policy_dropdown.add_menu_elem (MenuElem (_("Active Region"), sigc::bind (sigc::mem_fun (*this, &Pianoroll::set_editing_policy), ActiveView)));
	set_editing_policy (ActiveView);

	/* Ordering must match enum declaration order */
	colors_dropdown.add_menu_elem (MenuElem (_("Velocity"), sigc::bind (sigc::mem_fun (*this, &Pianoroll::set_color_mode), ARDOUR::MeterColors)));
	colors_dropdown.add_menu_elem (MenuElem (_("Channel"), sigc::bind (sigc::mem_fun (*this, &Pianoroll::set_color_mode), ARDOUR::ChannelColors)));
	colors_dropdown.add_menu_elem (MenuElem (_("Track"), sigc::bind (sigc::mem_fun (*this, &Pianoroll::set_color_mode), ARDOUR::TrackColor)));
	colors_dropdown.add_menu_elem (MenuElem (_("Pitch"), sigc::bind (sigc::mem_fun (*this, &Pianoroll::set_color_mode), ARDOUR::PitchColors)));
	colors_dropdown.add_menu_elem (MenuElem (_("Setup"), sigc::mem_fun (*this, &Pianoroll::setup_colors)));
	colors_dropdown.set_active ((int) _color_mode);
	ArdourWidgets::set_tooltip (colors_dropdown, _("Color Scheme for MIDI events"));

	/* We always need the MIDI inspector to exist, but we don't pack it by default */

	midi_inspector = manage (new MidiInspector (*this));
	midi_inspector->chord_box->ReplaceChord.connect ([this](std::vector<int> intervals) { replace_chord (intervals); });
	midi_inspector->chord_box->InvertChord.connect ([this](bool up) { invert_selected_chord (up); });
	midi_inspector->chord_box->DropChord.connect ([this](std::vector<int> which_notes) { drop_selected_chord (which_notes); });

	inspector_button.signal_clicked.connect (sigc::mem_fun (*this, &Pianoroll::inspector_button_clicked));
	ArdourWidgets::set_tooltip (inspector_button, _("Expand/Collapse MIDI inspector"));

	build_upper_toolbar ();
	build_grid_type_menu ();
	build_draw_midi_menus();
	build_lower_toolbar ();
	build_canvas ();

	set_action_defaults ();
	set_mouse_mode (Editing::MouseContent, true);

	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &Pianoroll::parameter_changed));
}

Pianoroll::~Pianoroll ()
{
	for (auto & [region,view] : region_view_map) {
		delete view;
	}

	for (auto & [param,lane] : automation_lanes) {
		delete lane;
	}

	view_connections.drop_connections ();
	_update_connection.disconnect ();
	selection_connection.disconnect ();

	drop_grid (); // unparent gridlines before deleting _canvas_viewport

	delete bg;
	delete xcursor;
}

void
Pianoroll::inspector_button_clicked ()
{
	if (!inspector_scroller) {

		inspector_scroller = manage (new Gtk::ScrolledWindow);
		inspector_scroller->add (*midi_inspector);
		inspector_scroller->set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

		UIConfiguration::instance().DPIReset.connect ([this]() { inspector_scroller->queue_resize(); });

		_hpacker.pack_start (*inspector_scroller, false, false);
		_hpacker.reorder_child (*inspector_scroller, 0);

		midi_inspector->show_all ();
	}

	if (inspector_scroller->is_visible()) {
		inspector_scroller->hide();
	} else {
		inspector_scroller->show();
	}

}

void
Pianoroll::toggle_size()
{
	PublicEditor::instance().toggle_main ();
}

void
Pianoroll::setup_colors ()
{
	PitchColorDialog pcd;

	pcd.ColorsChanged.connect ([this]() { update_pitch_colors(); });

	pcd.present ();
	pcd.run();
}

void
Pianoroll::update_pitch_colors ()
{
	for (auto & [region,view] : region_view_map) {
		view->color_handler ();
	}

	NoteBase::save_colors ();
}

void
Pianoroll::set_editing_policy (EditingPolicy ep)
{
	_editing_policy = ep;
	std::string txt;
	switch (_editing_policy) {
	case AllViews:
		txt = _("All Regions");
		break;
	case ActiveView:
		txt = _("Active Region");
		break;
	}

	policy_dropdown.set_text (txt);

	if (_editing_policy == ActiveView) {
		for (auto & [region,view] : region_view_map) {
			view->set_sensitive ((view == _active_view));
		}
	} else {
		for (auto & [region,view] : region_view_map) {
			view->set_sensitive (true);
		}
	}
}

void
Pianoroll::set_show_source (bool yn)
{
	EC_LOCAL_TEMPO_SCOPE;

	CueEditor::set_show_source (yn);
	for (auto & [region,view] : region_view_map) {
		view->set_show_source (yn);
	}
}

void
Pianoroll::toggle_automation (Evoral::Parameter param)
{
	if (no_toggle) {
		return;
	}

	if (automation_lanes.find (param) == automation_lanes.end()){
		add_automation_lane (param);
	} else {
		remove_automation_lane (param);
	}
}

void
Pianoroll::add_single_controller_item (Gtk::Menu_Helpers::MenuList& ctl_items,
                                       int                     ctl,
                                       const std::string&      name)
{
	EC_LOCAL_TEMPO_SCOPE;

	using namespace Gtk::Menu_Helpers;

	const uint16_t selected_channels = 0xffff;
	for (uint8_t chn = 0; chn < 16; chn++) {

		if (selected_channels & (0x0001 << chn)) {

			Evoral::Parameter fully_qualified_param (MidiCCAutomation, chn, ctl);
			std::string menu_text (string_compose ("%1: %2", ctl, parameter_name (fully_qualified_param)));

			ctl_items.push_back (CheckMenuElem (menu_text, [this,fully_qualified_param]() { toggle_automation (fully_qualified_param); }));

			if (automation_lanes.find (fully_qualified_param) != automation_lanes.end()) {
				Gtk::CheckMenuItem* cmi = static_cast<Gtk::CheckMenuItem*> (&ctl_items.back());
				PBD::Unwinder<bool> uw (no_toggle, true);
				cmi->set_active();
			}

			/* one channel only */
			break;
		}
	}
}

void
Pianoroll::add_multi_controller_item (Gtk::Menu_Helpers::MenuList& menulist,
                                      const uint16_t          channels,
                                      int                     ctl,
                                      const std::string&      name)
{
	EC_LOCAL_TEMPO_SCOPE;

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
			                                    [this,fully_qualified_param]() { toggle_automation (fully_qualified_param); }));


			if (automation_lanes.find (fully_qualified_param) != automation_lanes.end()) {
				Gtk::CheckMenuItem* cmi = static_cast<Gtk::CheckMenuItem*>(&chn_items.back());
				PBD::Unwinder<bool> uw (no_toggle, true);
				cmi->set_active();
			}
		}
	}

	/* add an item to metabutton's menu that will connect to the
	 * per-channel submenu we built above.
	 */

	menulist.push_back (MenuElem (menu_text, *chn_menu));
}

void
Pianoroll::build_lower_toolbar ()
{
	EC_LOCAL_TEMPO_SCOPE;
	Gtk::RadioButtonGroup edit_group;

	horizontal_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &Pianoroll::scrolled));

	_toolbox.pack_start (*_canvas_hscrollbar, false, false);
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

	box.pack_start (inspector_button, false, false);

	if (with_transport_controls) {
		box.pack_start (play_box, false, false, 12);
	}

	box.pack_start (rec_box, false, false);
	box.pack_start (visible_channel_label, false, false);
	box.pack_start (visible_channel_selector, false, false);
	box.pack_start (note_mode_button, false, false);

	if (expandable) {
		box.pack_end (size_button, false, false);
	}

	ArdourWidgets::set_tooltip (automation_button, _("Select visible MIDI automation"));

	box.pack_end (automation_button, false, false);
	box.pack_end (colors_dropdown, false, false);
	box.pack_end (region_dropdown, false, false);
	box.pack_end (policy_dropdown, false, false);
	region_dropdown.show ();
	policy_dropdown.show ();
}

void
Pianoroll::automation_button_clicked ()
{
	Gtk::Menu* am = build_automation_menu ();
	if (!am) {
		return;
	}

	am->popup (1, 0);
}

void
Pianoroll::set_color_mode (ARDOUR::ColorMode cm)
{
	if (_color_mode == cm) {
		return;
	}

	_color_mode = cm;
	colors_dropdown.set_active ((int) cm);

	if (bg) {
		bg->set_color_mode (cm);
	}

	for (auto & [region,view] : region_view_map) {
		view->color_handler ();
	}
}

void
Pianoroll::set_visible_channel (int n)
{
	EC_LOCAL_TEMPO_SCOPE;

	PBD::Unwinder<bool> uw (ignore_channel_changes, true);

	_visible_channel = n;
	visible_channel_selector.set_active (string_compose ("%1", _visible_channel + 1));

	for (auto & [region,view] : region_view_map) {
		view->set_visible_channel (n);
		view->swap_automation_channel (n);
	}

	prh->instrument_info_change ();
}

void
Pianoroll::build_canvas ()
{
	EC_LOCAL_TEMPO_SCOPE;

	_canvas.MouseMotion.connect ([this](ArdourCanvas::Duple const & pos) { motion_track (pos); });
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

	// add a background color to match the main editor pianoroll look
	ArdourCanvas::Rectangle* bg_rect = new ArdourCanvas::Rectangle (data_group, ArdourCanvas::Rect (0., 0., ArdourCanvas::COORD_MAX,  ArdourCanvas::COORD_MAX));
	bg_rect->set_fill_color(UIConfiguration::instance().color_mod ("midi track base", "midi track base"));
	bg_rect->set_outline(false);

	bg = new PianorollMidiBackground (data_group, *this);
	bg->set_color_mode (_color_mode);
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
	_toolbox.reorder_child (_canvas_viewport, 1);
}

void
Pianoroll::replace_chord (std::vector<int> intervals)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (_active_view) {
		_active_view->replace_chord (intervals);
	}
}

void
Pianoroll::invert_selected_chord (bool up)
{
	if (_editing_policy == ActiveView) {

		if (!_active_view) {
			return;
		}

		_active_view->invert_selected_chord (up);

	} else if (_editing_policy == AllViews) {

		for (auto & [region,view] : region_view_map) {
			view->invert_selected_chord (up);
		}
	}
}

void
Pianoroll::drop_selected_chord (std::vector<int> which_notes)
{
	if (_editing_policy == ActiveView) {

		if (!_active_view) {
			return;
		}

		_active_view->drop_selected_chord (which_notes);

	} else if (_editing_policy == AllViews) {

		for (auto & [region,view] : region_view_map) {
			view->drop_selected_chord (which_notes);
		}
	}
}

Quantize*
Pianoroll::get_quantize_op ()
{
	EC_LOCAL_TEMPO_SCOPE;

	QuantizeWidget* qw (midi_inspector->quantize_widget);

	return new Quantize (qw->snap_start(),
	                     qw->snap_end(),
	                     qw->start_grid_size(),
	                     qw->end_grid_size(),
	                     qw->strength(),
	                     qw->swing(),
	                     qw->threshold());
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

	if (!_active_view) {
		return; /* Ought to be impossible */
	}

	_visible_channel = _active_view->visible_channel();
	visible_channel_selector.set_active (string_compose ("%1", _active_view->visible_channel() + 1));
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

			if (_drags->active() || !_active_view || !_active_view->midi_track()->triggerbox()) {
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

	} else if (_active_view->midi_region()) {

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

		samplepos_t spos = local_tempo_map->sample_at (global_tempo_map->quarters_at (_active_view->midi_region()->source_position()));

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
			if (xcursor) {
				xcursor->hide();
			}
		}
	default:
		break;
	}
	return false;
}

void
Pianoroll::partition_height ()
{
	double timebars = n_timebars * timebar_height;
	double data_height = _visible_canvas_height - timebars;
	double note_area_height = automation_lanes.empty() ? data_height : floor (2 * data_height / 3.);
	double automation_height = floor (data_height - note_area_height);

	bg->set_size (_visible_canvas_width, note_area_height);
	prh->set (ArdourCanvas::Rect (0, 0, prh->x1(), note_area_height));

	if (automation_lanes.empty()) {
		for (auto & [region,view] : region_view_map) {
			view->set_height (data_height);
		}
		return;
	}

	double ay = note_area_height;
	double per_lane = floor (automation_height / automation_lanes.size()) - 2;

	for (auto & [param, lane] : automation_lanes) {
		lane->group->set_position (ArdourCanvas::Duple (0., ay));
		lane->group->set (ArdourCanvas::Rect (0., 0., ArdourCanvas::COORD_MAX, per_lane));
		lane->close_x->set_position (ArdourCanvas::Duple (4, ay + 30));
		lane->label->set_position (ArdourCanvas::Duple (20, ay + 30));
		if (lane->clear_button) {
			lane->clear_button->set_position (ArdourCanvas::Duple (prh->get().width() - (lane->clear_button->size().x + 4), ay + 25));
		}
		ay += per_lane + 2;
	}

	for (auto & [region,view] : region_view_map) {
		view->partition_height ();
		view->set_height (data_height);
	}
}

Evoral::Parameter
Pianoroll::automation_by_y (double y)
{
	ArdourCanvas::Duple d (0., y);
	double timebars = n_timebars * timebar_height;

	for (auto & [param,lane] : automation_lanes) {
		ArdourCanvas::Rect r (lane->group->get().translate (lane->group->position()).translate (ArdourCanvas::Duple (0, timebars)));
		if (r.contains (d)) {
			return param;
		}
	}

	return Evoral::Parameter (NullAutomation);
}

void
Pianoroll::canvas_allocate (Gtk::Allocation alloc)
{
	EC_LOCAL_TEMPO_SCOPE;

	_visible_canvas_width = alloc.get_width();
	_visible_canvas_height = alloc.get_height();

	double timebars = n_timebars * timebar_height;

	_track_canvas_width = _visible_canvas_width - prh->x1();
	_timeline_origin = prh->x1();

	partition_height ();

	data_group->set_position (ArdourCanvas::Duple (_timeline_origin, timebars));
	no_scroll_group->set_position (ArdourCanvas::Duple (_timeline_origin, timebars));
	cursor_scroll_group->set_position (ArdourCanvas::Duple (_timeline_origin, timebars));
	h_scroll_group->set_position (Duple (_timeline_origin, 0.));

	if (!xcursor) {
		xcursor = new CrossCursor (_canvas.root());
		xcursor->set_line_width (5);
		xcursor->set_outline_color (UIConfiguration::instance().color_mod ("verbose canvas cursor", "verbose canvas cursor"));
		xcursor->hide (); /* for now, it will become visible on first motion */
	}

	xcursor->set_extents (_visible_canvas_width, _visible_canvas_height);

	if (zoom_in_allocate) {

		if (!_active_view || !maybe_set_from_rsu (_active_view->midi_region()->id())) {
			zoom_to_show (max_zoom_extent());
		}
		if (_region) {
			/* XXXX */
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

	for (auto & [region,view] : region_view_map) {
		view->set_samples_per_pixel (spp);
	}

	update_tempo_based_rulers ();

	horizontal_adjustment.set_upper (max_zoom_extent().second.samples() / samples_per_pixel);
	horizontal_adjustment.set_page_size (current_page_samples()/ samples_per_pixel / 10);
	horizontal_adjustment.set_page_increment (current_page_samples()/ samples_per_pixel / 20);
	horizontal_adjustment.set_step_increment (current_page_samples() / samples_per_pixel / 100);

	instant_save ();
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

	if (_active_view) {
		_active_view->clip_data_recorded (data_capture_duration);
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
	Evoral::Parameter param (NullAutomation);

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
			ControlPointDrag* cpd = new ControlPointDrag (*this, item);

			ControlPoint* cp = reinterpret_cast<ControlPoint*> (item->get_data ("control_point"));
			if (cp) {
				AutomationLine& line (cp->line());
				Evoral::Parameter line_param (line.the_list()->parameter());
				for (auto & [param,lane] : automation_lanes) {
					if (param == line_param) {
						cpd->set_bounding_item (lane->group);
						break;
					}
				}
			}
			_drags->set (cpd, event);
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
			_drags->set (new RubberbandSelectDrag (*this, item, [&](GdkEvent* ev, timepos_t const & pos) { return _active_view->velocity_rb_click (ev, pos); }), event);
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
			param = automation_by_y (event->button.y);
			if (param.type() != NullAutomation) {
				_drags->set (new RubberbandSelectDrag (*this, item, [this,param](GdkEvent* ev, timepos_t const & pos) { return _active_view->automation_rb_click (ev, pos, param); }), event);
			}
			break;
		case Editing::MouseDraw:
			param = automation_by_y (event->button.y);
			if (param.type() != NullAutomation) {
				_drags->set (new AutomationDrawDrag (*this, nullptr, *static_cast<ArdourCanvas::Rectangle*>(item), false, Temporal::BeatTime,
				                                     [this,param](GdkEvent* ev, timepos_t const & pos) { return _active_view->automation_rb_click (ev, pos, param); }), event);
			}
			break;
		default:
			break;
		}
		return true;
		break;

	case EditorAutomationLineItem: {
		ARDOUR::SelectionOperation op = ArdourKeyboard::selection_type (event->button.state);
		select_automation_line (&event->button, item, op);
		if (mouse_mode == Editing::MouseContent) {
			LineDrag* ld = new LineDrag (*this, item, [&](GdkEvent* ev,timepos_t const & pos, double) { _active_view->line_drag_click (ev, pos); });
			AutomationLine* line = reinterpret_cast<AutomationLine*> (item->get_data ("line"));
			if (line) {
				Evoral::Parameter line_param (line->the_list()->parameter());
				for (auto & [param,lane] : automation_lanes) {
					if (param == line_param) {
						ld->set_bounding_item (lane->group);
						break;
					}
				}
			}
			_drags->set (ld, event);
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

	if (!_active_view) {
		return;
	}

	const uint32_t sel_size = _active_view->selection_size ();
	MidiViews mvs ({_active_view});

	MenuList& items = _region_context_menu.items();
	items.clear();

	if (sel_size > 0) {
		items.push_back (MenuElem(_("Delete"), sigc::mem_fun (*_active_view, &MidiView::delete_selection)));
	}

	items.push_back(MenuElem(_("Edit..."), sigc::bind(sigc::mem_fun(*this, &EditingContext::edit_notes), _active_view)));
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

void
Pianoroll::note_entered ()
{
	assert (xcursor);
	xcursor->hide ();
}

void
Pianoroll::note_left ()
{
}

void
Pianoroll::motion_track (ArdourCanvas::Duple const & pos)
{
	assert (xcursor);

	if (!_drags->active()) {
		xcursor->hide ();
		return;
	}

	if (_drags->dragging_lollipop()) {
		xcursor->hide ();
		return;
	}

	auto res = automation_lanes.find (MidiVelocityAutomation);
	if (res != automation_lanes.end()) {
		double y0 = res->second->group->position().y;
		double y1 = y0 + res->second->group->get().height();
		Duple cp0 (res->second->group->parent()->item_to_canvas (Duple (0, y0)));
		Duple cp1 (res->second->group->parent()->item_to_canvas (Duple (0, y1)));

		if (pos.y >= cp0.y && pos.y < cp1.y) {
			xcursor->hide ();
			return;
		}
	}

	xcursor->show ();

	/* when events arrive in the canvas, they are adjusted to canvas
	   coordinates by using the "best" scrollgroup, which will always be
	   the HV scroll group. Reverse this transformation to get back to
	   window coordinates. Canvas::canvas_to_window() doesn't do
	   specifically this transformation, for various reasons.
	*/

	ArdourCanvas::Duple xc (ArdourCanvas::Duple (pos.x, pos.y).translate (-hv_scroll_group->scroll_offset()));

	ArdourCanvas::Item const * bounds = _drags->drags().front()->bounding_item();

	if (bounds) {
		ArdourCanvas::Rect rect (bounds->item_to_window (bounds->bounding_box()));
		xc.x = std::max (xc.x, rect.x0);
		xc.x = std::min (xc.x, rect.x1);
		xc.y = std::max (xc.y, rect.y0);
		xc.y = std::min (xc.y, rect.y1);
	}

	xcursor->set_position (xc);
}

bool
Pianoroll::motion_handler (ArdourCanvas::Item* item, GdkEvent* event, bool from_autoscroll)
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

	if (_editing_policy == ActiveView) {

		if (!_active_view) {
			return;
		}

		(_active_view->*method) ();

	} else if (_editing_policy == AllViews) {

		for (auto & [region,view] : region_view_map) {
			(view->*method) ();
		}
	}
}

void
Pianoroll::escape ()
{
	EC_LOCAL_TEMPO_SCOPE;

	if (!_active_view) {
		return;
	}

	_active_view->clear_selection ();
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

	if (_active_view) {
		return _active_view->selectable_owners();
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
	if (region_view_map.empty()) {
		CueEditor::unset_region ();
		// _active_view->set_region (nullptr);
	}
}

void
Pianoroll::unset_trigger ()
{
	CueEditor::unset_trigger ();
}

void
Pianoroll::replace_region (std::shared_ptr<ARDOUR::Region> region, std::shared_ptr<ARDOUR::MidiTrack> track)
{
	view_connections.drop_connections ();
	for (auto & [region,view] : region_view_map) {
		delete view;
	}
	region_view_map.clear ();

	add_region (region, track);
	set_region (region);
}

void
Pianoroll::add_region (std::shared_ptr<ARDOUR::Region> region, std::shared_ptr<ARDOUR::MidiTrack> track)
{
	PianorollMidiView* new_view = new PianorollMidiView (track, *data_group, *no_scroll_group, *this, *bg);

	std::shared_ptr<ARDOUR::MidiRegion> mr (std::dynamic_pointer_cast<ARDOUR::MidiRegion> (region));
	assert (mr);

	new_view->set_region (mr);
	new_view->set_show_source (show_source);
	new_view->show_start (true);
	new_view->show_end (true);

	auto res = region_view_map.insert (std::make_pair (region, new_view));
	if (res.second) {
		rebuild_region_dropdown ();
	}

	region->DropReferences.connect (view_connections, invalidator (*this), sigc::bind (sigc::mem_fun (*this, &Pianoroll::region_going_away), std::weak_ptr<ARDOUR::Region> (region)), gui_context());
}

void
Pianoroll::remove_regions ()
{
	std::vector<MidiView*> mvs;
	for (auto & [region,view] : region_view_map) {
		mvs.push_back (view);
	}

	region_view_map.clear ();
	set_region (nullptr);

	for (auto & mv : mvs) {
		delete mv;
	}
}

void
Pianoroll::remove_region (std::shared_ptr<ARDOUR::Region> region)
{
	auto rvm = region_view_map.find (region);
	if (rvm == region_view_map.end()) {
		return;
	}

	MidiView* mv (rvm->second);
	region_view_map.erase (rvm);

	if (_active_view == mv) {
		set_region (nullptr);
	}

	delete mv;
}

void
Pianoroll::rebuild_region_dropdown ()
{
	region_dropdown.clear_items ();
	for (auto & [region,view] : region_view_map) {
		std::weak_ptr<ARDOUR::Region> wr (region);
		region_dropdown.add_menu_elem (Gtk::Menu_Helpers::MenuElem (region->name(), [this,wr]() { std::shared_ptr<ARDOUR::Region> r (wr.lock()); if (r) set_region (r); }));
	}
}

void
Pianoroll::region_going_away (std::weak_ptr<ARDOUR::Region> wr)
{
	std::shared_ptr<ARDOUR::Region> region (wr.lock());
	if (!region) {
		return;
	}

	auto rvm = region_view_map.find (region);
	if (rvm == region_view_map.end()) {
		return;
	}

	bool switch_views = (_active_view == rvm->second);

	/* Clean up the view */
	delete rvm->second;
	region_view_map.erase (rvm);
	rebuild_region_dropdown ();

	if (switch_views) {
		if (region_view_map.empty()) {
			set_region (nullptr);
		} else {
			set_region (region_view_map.begin()->first);
		}
	}
}

void
Pianoroll::set_region (std::shared_ptr<ARDOUR::Region> region)
{
	CueEditor::set_region (region);

	if (_visible_pending_region) {
		return;
	}

	/* unset everything */

	for (auto & [param,lane] : automation_lanes) {
		(void) lane->group->set_data ("linemerger", nullptr);
	}

	_active_view = nullptr;
	view_connections.drop_connections ();
	_update_connection.disconnect ();
	selection_connection.disconnect ();
	midi_inspector->set_region (_session, nullptr);

	if (!region) {
		return;
	}

	std::shared_ptr<MidiRegion> r (std::dynamic_pointer_cast<ARDOUR::MidiRegion> (region));

	auto rvm = region_view_map.find (region);

	if (rvm == region_view_map.end()) {
		error << _("Attempt to set pianoroll region that was not added!") << endmsg;
		return;
	}

	/* OK, time to switch the "active" view */

	_active_view = rvm->second;
	CueEditor::set_track (_active_view->midi_track());

	if (_editing_policy == ActiveView) {
		for (auto & [region,view] : region_view_map) {
			view->set_sensitive ((view == _active_view));
		}
	} else {
		for (auto & [region,view] : region_view_map) {
			view->set_sensitive (true);
		}
	}

	_active_view->VisibleChannelChanged.connect (view_connections, invalidator (*this), std::bind (&Pianoroll::visible_channel_changed, this), gui_context());
	selection_connection = _active_view->SelectionChanged.connect ([this]() { midi_view_selection_changed (); });

	set_visible_channel (_active_view->pick_visible_channel());

	for (auto & [param,lane] : automation_lanes) {
		lane->group->set_data ("linemerger", _active_view);
	}

	/* Visible note range should always span all regions on display */

	uint8_t lowest_note = 127;
	uint8_t highest_note = 0;

	for (auto & [region,view] : region_view_map) {
		std::shared_ptr<ARDOUR::SMFSource> smf (std::dynamic_pointer_cast<ARDOUR::SMFSource> (region->source()));
		assert (smf);
		lowest_note = std::min (lowest_note, smf->model()->lowest_note());
		highest_note = std::max (highest_note, smf->model()->highest_note());
	}

	(void) bg->update_data_note_range (lowest_note, highest_note);
	bg->apply_note_range (lowest_note, highest_note, true);

	if (!_active_view || !maybe_set_from_rsu (_active_view->midi_region()->id())) {
		/* Compute zoom level to show entire source plus some margin if possible */
		zoom_to_show (max_zoom_extent());
	}

	if (region_view_map.size() > 1) {
		show_automation_for_all ();
	}

	if (r->source()->empty()) {
		std::shared_ptr<MidiTrack> mt (std::dynamic_pointer_cast<ARDOUR::MidiTrack> (_track));
		if (mt) {
			note_mode_actions[mt->note_mode()]->set_active (true);
		}
	}

	region_dropdown.set_active (region->name());
	midi_inspector->set_region (_session, _active_view->midi_region());
}

void
Pianoroll::apply_note_range (uint8_t lowest, uint8_t highest)
{
	for (auto & [region,view] : region_view_map) {
		view->apply_note_range (lowest, highest);
	}
}

Pianoroll::AutomationLane::AutomationLane (Evoral::Parameter const & param, Pianoroll const & pr, ArdourCanvas::Item* parent, uint32_t nth)
	: group (new ArdourCanvas::Rectangle (parent))
	, label (new ArdourCanvas::Text (parent->canvas()->root()))
	, close_x (new ArdourCanvas::Icon (parent->canvas()->root(), ArdourWidgets::ArdourIcon::CloseCross))
	, clear_button ((param.type() == MidiVelocityAutomation) ? nullptr : new ArdourCanvas::Button (parent->canvas()->root(), _("Clear"), UIConfiguration::instance().get_SmallFont()))
{
	group->set_outline (false);
	CANVAS_DEBUG_NAME (group, std::string ("pr auto group for ") + pr.parameter_name (param));

	label->set (pr.parameter_name (param));
	label->set_color (UIConfiguration::instance().color (X_("gtk_foreground")));
	label->set_font_description (UIConfiguration::instance().get_SmallFont());

	close_x->set (ArdourCanvas::Rect (0, 0, 12, 12));
	close_x->set_outline_color (UIConfiguration::instance().color (X_("gtk_foreground")));

	if (clear_button) {
		clear_button->text()->set_color (UIConfiguration::instance().color (X_("gtk_foreground")));
		clear_button->set_highlight (true);
		clear_button->set_size (clear_button->text()->width() + 8, clear_button->text()->height() + 8);
	}
}

Pianoroll::AutomationLane::~AutomationLane ()
{
	delete group;
	delete label;
	delete close_x;
	delete clear_button;
}

void
Pianoroll::AutomationLane::deduce_color (uint32_t nth)
{
	if (nth % 2 != 0) {
		group->set_fill_color (UIConfiguration::instance().color ("midi automation track fill"));
	} else {
		Gtkmm2ext::HSV hsv (UIConfiguration::instance().color ("midi automation track fill"));
		hsv = hsv.lighter (0.05);
		group->set_fill_color (hsv.color());
	}
}


std::string
Pianoroll::parameter_name (Evoral::Parameter const & param) const
{
	std::string str = midi_track()->get_parameter_name (param);
	auto res = controller_name_map.find (str);

	/* Try to find a short name */

	if (res != controller_name_map.end()) {
		str = res->second;
	}

	return str;
}

bool
Pianoroll::automation_clear_event (GdkEvent* ev, Evoral::Parameter param)
{
	switch (ev->type) {
	case GDK_BUTTON_PRESS:
		return true;
	case GDK_BUTTON_RELEASE:
		clear_automation_lane (param);
		return true;
	default:
		break;
	}
	return true;
}

bool
Pianoroll::automation_close_event (GdkEvent* ev, Evoral::Parameter param)
{
	switch (ev->type) {
	case GDK_BUTTON_PRESS:
		return true;
	case GDK_BUTTON_RELEASE:
		remove_automation_lane (param);
		return true;
	default:
		break;
	}
	return false;
}

void
Pianoroll::add_automation_lane (Evoral::Parameter const & param)
{
	if (automation_lanes.find (param) != automation_lanes.end()) {
		return;
	}

	AutomationLane* lane = new AutomationLane (param, *this, data_group, automation_lanes.size());;
	lane->group->Event.connect ([this,param](GdkEvent* event) { return automation_group_event (event, param); });
	lane->close_x->Event.connect ([this,param](GdkEvent* event) { return automation_close_event (event, param); });
	if (lane->clear_button) {
		lane->clear_button->Event.connect ([this,param](GdkEvent* event) { return automation_clear_event (event, param); });
	}

	if (_active_view) {
		lane->group->set_data ("linemerger", _active_view);
	}

	automation_lanes.insert (std::make_pair (param, lane));

	partition_height ();

	for (auto & [region,view] : region_view_map) {
		view->add_automation_lane (param, *lane);
	}

	/* recolor lane backgrounds, since ordering may have changed */

	uint32_t n = 0;
	for (auto & [param,lane] : automation_lanes) {
		lane->deduce_color (n++);
	}

	instant_save ();
}

void
Pianoroll::remove_automation_lane (Evoral::Parameter const & param)
{
	auto existing = automation_lanes.find (param);

	if (existing == automation_lanes.end()) {
		return;
	}

	AutomationLane* lane = existing->second;
	automation_lanes.erase (existing);

	partition_height ();

	for (auto & [region,view] : region_view_map) {
		view->remove_automation_lane (param, *lane);
	}

	delete lane;

	/* recolor lane backgrounds, since ordering has changed */

	uint32_t n = 0;
	for (auto & [param,lane] : automation_lanes) {
		lane->deduce_color (n++);
	}

	instant_save ();
}

void
Pianoroll::clear_automation_lane (Evoral::Parameter const & param)
{
	auto res = automation_lanes.find (param);
	if (res == automation_lanes.end()) {
		return;
	}

	if (_editing_policy == ActiveView) {

		_active_view->clear_automation_lane (param);

	} else if (_editing_policy == AllViews) {

		for (auto & [region,view] : region_view_map) {
			view->clear_automation_lane (param);
		}
	}
}

bool
Pianoroll::automation_group_event (GdkEvent* event, Evoral::Parameter param)
{
	switch (event->type) {
	case GDK_ENTER_NOTIFY:
		for (auto & [region,view] : region_view_map) {
			view->set_active_automation (param);
		}
		break;
	case GDK_LEAVE_NOTIFY:
		if (event->crossing.detail != GDK_NOTIFY_INFERIOR) {
			for (auto & [region,view] : region_view_map) {
				view->set_active_automation (NullAutomation);
			}
		}
		break;
	default:
		break;
	}
	return false;
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

	if (_active_view) {
		_active_view->point_selection_changed ();
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

	if (_active_view) {
		// _active_view->paste (Editing::Cut);
	}
}

void
Pianoroll::keyboard_paste ()
{
	if (!_active_view || !_region) {
		return;
	}

	EC_LOCAL_TEMPO_SCOPE;

	timepos_t where (get_preferred_edit_position (Editing::EDIT_IGNORE_NONE, false, false));
	timepos_t absolute_where = _region->region_beats_to_absolute_time (where.beats());

	PasteContext pc (0, 1, ItemCounts(), true);
	begin_reversible_command (string_compose (_("paste %1"), X_("MIDI")));
	_active_view->paste (absolute_where, get_cut_buffer(), pc);
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
		if (_active_view) {
			begin_reversible_command (opname + ' ' + X_("MIDI"));
			_active_view->cut_copy_clear (*selection, op);
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

	if (_editing_policy == ActiveView && !_active_view) {
		return;
	}

	std::list<Selectable*> found;

	AutomationLane* lane (nullptr);
	Evoral::Parameter param (NullAutomation);
	ArdourCanvas::Duple top (0., y0);
	ArdourCanvas::Duple bottom (0., y1);

	for (auto & [p,l] : automation_lanes) {
		ArdourCanvas::Rect r (l->group->get().translate (l->group->position()));
		if (r.contains (top)) {
			lane = l;
			param = p;
			break;
		}
		if (r.contains (bottom)) {
			lane = l;
			param = p;
			break;
		}
	}

	if (param.type() == NullAutomation) {
		return;
	}

	double topfrac;
	double botfrac;

	/* translate y0 and y1 to use the top of the automation area as the * origin */

	double automation_origin = lane->group->position().y;

	y0 -= automation_origin;
	y1 -= automation_origin;

	if (y0 < 0. && lane->height() <= y1) {

		/* _y_position is below top, mybot is above bot, so we're fully
		   covered vertically.
		*/

		topfrac = 1.0;
		botfrac = 0.0;

	} else {

		/* top and bot are within _y_position .. mybot */

		topfrac = 1.0 - (y0 / lane->height());
		botfrac = 1.0 - (y1 / lane->height());

	}

	if (_editing_policy == ActiveView) {

		_active_view->get_selectables (param, start, end, botfrac, topfrac, found);

	} else if (_editing_policy == AllViews) {

		for (auto & [region,view] : region_view_map) {
			view->get_selectables (param, start, end, botfrac, topfrac, found);
		}
	}

	if (found.empty()) {
		if (_editing_policy == ActiveView) {
			_active_view->clear_selection ();
		}

	} else if (_editing_policy == AllViews) {

		for (auto & [region,view] : region_view_map) {
			view->clear_selection ();
		}
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
		commit_reversible_selection_op ();
		break;
	case SelectionToggle:
		begin_reversible_selection_op (X_("toggle select all within"));
		selection->toggle (found);
		commit_reversible_selection_op ();
		break;
	case SelectionSet:
		begin_reversible_selection_op (X_("select all within"));
		selection->set (found);
		commit_reversible_selection_op ();
		break;
	default:
		return;
	}

}

void
Pianoroll::set_session (ARDOUR::Session* s)
{
	EC_LOCAL_TEMPO_SCOPE;

	CueEditor::set_session (s);

	if (with_transport_controls) {
		if (_session) {
			_session->TransportStateChange.connect (_session_connections, invalidator (*this), std::bind (&Pianoroll::map_transport_state, this), gui_context());
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
	selection_connection.disconnect ();

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

	if (!_active_view) {
		return;
	}

	_active_view->shift_midi (t, model);
}

InstrumentInfo*
Pianoroll::instrument_info () const
{
	EC_LOCAL_TEMPO_SCOPE;

	if (!_active_view || !_active_view->midi_track()) {
		return nullptr;
	}

	return &_active_view->midi_track()->instrument_info ();
}

std::shared_ptr<ARDOUR::MidiTrack>
Pianoroll::midi_track() const
{
	return std::dynamic_pointer_cast<ARDOUR::MidiTrack> (_track);
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

	if (!_active_view) {
		return;
	}

	uint16_t chn_mask = _active_view->midi_track()->get_playback_channel_mask();

	begin_reversible_selection_op (X_("Set Note Selection"));
	_active_view->select_matching_notes (note, chn_mask, false, false);
	commit_reversible_selection_op();
}

void
Pianoroll::add_note_selection (uint8_t note)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (!_active_view) {
		return;
	}

	const uint16_t chn_mask = _active_view->midi_track()->get_playback_channel_mask();

	begin_reversible_selection_op (X_("Add Note Selection"));
	_active_view->select_matching_notes (note, chn_mask, true, false);
	commit_reversible_selection_op();
}

void
Pianoroll::extend_note_selection (uint8_t note)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (!_active_view) {
		return;
	}

	const uint16_t chn_mask = _active_view->midi_track()->get_playback_channel_mask();

	begin_reversible_selection_op (X_("Extend Note Selection"));
	_active_view->select_matching_notes (note, chn_mask, true, true);
	commit_reversible_selection_op();
}

void
Pianoroll::toggle_note_selection (uint8_t note)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (!_active_view) {
		return;
	}

	const uint16_t chn_mask = _active_view->midi_track()->get_playback_channel_mask();

	begin_reversible_selection_op (X_("Toggle Note Selection"));
	_active_view->toggle_matching_notes (note, chn_mask);
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

	if (_active_view) {
		_active_view->begin_write ();
	}
}

void
Pianoroll::end_write ()
{
	EC_LOCAL_TEMPO_SCOPE;

	if (_active_view) {
		_active_view->end_write ();
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

	if (_active_view) {
		_active_view->set_overlay_text (str);
	}
}

void
Pianoroll::hide_count_in ()
{
	EC_LOCAL_TEMPO_SCOPE;

	if (_active_view) {
		_active_view->hide_overlay_text ();
	}
}

void
Pianoroll::set_from_rsu (RegionUISettings& region_ui_settings)
{
	assert (_active_view);

	note_mode_actions[region_ui_settings.note_mode]->set_active (true);
	CueEditor::set_from_rsu (region_ui_settings);

	if (region_view_map.size() > 1) {
		return;
	}

	/* there's only 1 region, show it's automation */
	show_automation_for_all ();
}

void
Pianoroll::show_automation_for_all ()
{
	for (auto & [region,view] : region_view_map) {
		view->remove_all_automation ();
	}

	for (auto & [param,lane] : automation_lanes) {
		delete lane;
	}

	automation_lanes.clear ();

	std::set<Evoral::Parameter> params_for_automation;

	for (auto & [region,view] : region_view_map) {
		RegionUISettingsManager::iterator rsu = ARDOUR_UI::instance()->region_ui_settings_manager.find (region->id());
		if (rsu != ARDOUR_UI::instance()->region_ui_settings_manager.end()) {

			if (!rsu->second.automation) {
				continue;
			}

			/* We can't add the automation lanes as we iterate over the automation
			 * node children, because adding automation lanes will modify that node
			 * in place. So get the parameters out of the XMLNodes, and then add
			 * them.
			 */


			for (auto const & n : rsu->second.automation->children()) {
				std::string val;
				if (n->get_property (X_("param"), val)) {
					params_for_automation.insert (ARDOUR::EventTypeMap::instance().from_symbol (val));
				}
			}
		}
	}

	for (auto & param : params_for_automation) {
		add_automation_lane (param);
	}
}


void
Pianoroll::instant_save ()
{
	EC_LOCAL_TEMPO_SCOPE;

	for (auto & [region,view] : region_view_map) {
		RegionUISettings rus;
		initialize_region_ui_settings (rus);

		rus.draw_length = draw_length();
		rus.draw_velocity = draw_velocity();
		rus.channel = draw_channel();
		rus.note_min = bg->lowest_note ();
		rus.note_max = bg->highest_note();
		rus.note_mode = note_mode ();
		rus.color_mode = color_mode ();

		XMLNode* as (view->automation_state());
		if (as) {
			rus.automation.reset (as);
		}

		add_region_ui_settings (region->id(), rus);
	}
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
Pianoroll::build_automation_menu ()
{
	using namespace Gtk;
	using namespace Menu_Helpers;

	if (!_track) {
		return nullptr;
	}

	Menu* automation_menu = new Menu;
	int mask = (1 << _visible_channel);
	std::vector<Evoral::Parameter> params {
		MidiVelocityAutomation,
		MidiPitchBenderAutomation,
		MidiChannelPressureAutomation,
		MidiNotePressureAutomation };

	for (auto p : params) {
		automation_menu->items().push_back (CheckMenuElem (parameter_name(p), [this,p]() { toggle_automation (p); }));
		if (automation_lanes.find (p) != automation_lanes.end()) {
			Gtk::CheckMenuItem* cmi = static_cast<Gtk::CheckMenuItem*> (&automation_menu->items().back());
			PBD::Unwinder<bool> uw (no_toggle, true);
			cmi->set_active();
		}
	}

	build_controller_menu (*automation_menu, _track->instrument_info(), mask,
	                       sigc::mem_fun (*this, &Pianoroll::add_single_controller_item),
	                       sigc::mem_fun (*this, &Pianoroll::add_multi_controller_item),
	                       20);

	return automation_menu;
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

	Gtk::Menu* am = build_automation_menu ();
	if (am) {
		items.push_back (MenuElem (_("Automation"), *am));
	}

	return m;
}

EditingContext::MidiViews
Pianoroll::midiviews_from_region_selection (RegionSelection const &) const
{
	MidiViews mv;

	if (_editing_policy == ActiveView) {

		if (_active_view) {
			mv.push_back (_active_view);
		}

	} else if (_editing_policy == AllViews) {

		for (auto & [region,view] : region_view_map) {
			mv.push_back (view);
		}
	}

	return mv;
}

void
Pianoroll::midi_view_selection_changed ()
{
	if (!_active_view) {
		midi_inspector->chord_box->show_chord ("");
		return;
	}

	MidiView::Selection const & sel (_active_view->selection());
	if (sel.size() < 2) {
		midi_inspector->chord_box->show_chord ("");
		return;
	}

	std::vector<int> pitches;

	for (auto const & s : sel) {
		pitches.push_back (s->note()->note());
	}

	std::sort (pitches.begin(), pitches.end());
	std::string name = midi_inspector->chord_box->identify_chord (pitches);
	midi_inspector->chord_box->show_chord (name);
}

bool
Pianoroll::get_midi_chord (int root_pitch, std::vector<int>& pitches) const
{
	if (!_active_view) {
		return false;
	}

	return midi_inspector->chord_box->get_midi_chord (root_pitch, pitches);
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

void
Pianoroll::build_midi_controller_name_map ()
{
	/* Maps names from MIDNAM/MIDI standard names for controllers to shorter
	   versions. Anything missing here means "use the given name as
	   is". The map keys come from somewhere that is non-translatable, but
	   the map values are translatable.
	*/
	using namespace std;

	controller_name_map.insert (make_pair<string,string> (X_("Modulation Wheel or Lever"), _("Modulation")));
	controller_name_map.insert (make_pair<string,string> (X_("Breath Controller"), _("Breath Ctrlr")));
	controller_name_map.insert (make_pair<string,string> (X_("Foot Controller"), _("Foot Ctrlr")));
	controller_name_map.insert (make_pair<string,string> (X_("Portamento Time"), _("Portamento Time")));
	controller_name_map.insert (make_pair<string,string> (X_("Data Entry MSB"), _("Data Entry MSB")));
	controller_name_map.insert (make_pair<string,string> (X_("Channel Volume"), _("Channel Volume")));
	controller_name_map.insert (make_pair<string,string> (X_("Expression Controller"), _("Expression Ctrlr")));
	controller_name_map.insert (make_pair<string,string> (X_("Effect Control 1"), _("Effect Control 1")));
	controller_name_map.insert (make_pair<string,string> (X_("Effect Control 2"), _("Effect Control 2")));
	controller_name_map.insert (make_pair<string,string> (X_("General Purpose Controller 1"), _("Gen. Ctrlr 1")));
	controller_name_map.insert (make_pair<string,string> (X_("General Purpose Controller 2"), _("Gen. Ctrlr 2")));
	controller_name_map.insert (make_pair<string,string> (X_("General Purpose Controller 3"), _("Gen. Ctrlr 3")));
	controller_name_map.insert (make_pair<string,string> (X_("General Purpose Controller 4"), _("Gen. Ctrlr 4")));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 0 (Bank Select) (Fine)"), _("CC 0 LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 1 (Modulation Wheel or Lever) (Fine)"), _("Modulation LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 2 (Breath Controller) (Fine)"), _("Breath LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 3 (Undefined) (Fine)"), _("CC 3 LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 4 (Foot Controller) (Fine)"), _("Foot LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 5 (Portamento Time) (Fine)"), _("Portamento LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 6 (Data Entry) (Fine)"), _("Data LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 7 (Channel Volume) (Fine)"), _("Chn. Vol LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 8 (Balance) (Fine)"), _("Balance LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 9 (Undefined) (Fine)"), _("CC 9 LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 10 (Pan) (Fine)"), _("CC 10 LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 11 (Expression Controller) (Fine)"), _("CC 11 LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 12 (Effect control 1) (Fine)"), _("CC 12 LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 13 (Effect control 2) (Fine)"), _("CC 13 LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 14 (Undefined) (Fine)"), _("CC 14 LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 15 (Undefined) (Fine)"), _("CC 15 LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 16 (General Purpose Controller 1) (Fine)"), _("CC 16 LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 17 (General Purpose Controller 2) (Fine)"), _("CC 17 LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 18 (General Purpose Controller 3) (Fine)"), _("CC 18 LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 19 (General Purpose Controller 4) (Fine)"), _("CC 19 LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 20 (Undefined) (Fine)"), _("CC 20 LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 21 (Undefined) (Fine)"), _("CC 21 LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 22 (Undefined) (Fine)"), _("CC 22 LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 23 (Undefined) (Fine)"), _("CC 23 LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 24 (Undefined) (Fine)"), _("CC 24 LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 25 (Undefined) (Fine)"), _("CC 25 LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 26 (Undefined) (Fine)"), _("CC 26 LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 27 (Undefined) (Fine)"), _("CC 27 LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 28 (Undefined) (Fine)"), _("CC 28 LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 29 (Undefined) (Fine)"), _("CC 29 LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 30 (Undefined) (Fine)"), _(_("CC 30 LSB"))));
	controller_name_map.insert (make_pair<string,string> (X_("LSB for Control 31 (Undefined) (Fine)"), _("CC 31 LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("Damper Pedal on/off (Sustain) ≤63 off, ≥64 on"), _("Sustain on/off")));
	controller_name_map.insert (make_pair<string,string> (X_("Portamento On/Off ≤63 off, ≥64 on"), _("Portamento on/off")));
	controller_name_map.insert (make_pair<string,string> (X_("Sostenuto On/Off ≤63 off, ≥64 on"), _("Sostenuto on/off")));
	controller_name_map.insert (make_pair<string,string> (X_("Soft Pedal On/Off ≤63 off, ≥64 on"), _("Soft Pedal on/off")));
	controller_name_map.insert (make_pair<string,string> (X_("Legato Footswitch ≤63 Normal, ≥64 Legato"), _("Legato Footswitch")));
	controller_name_map.insert (make_pair<string,string> (X_("Hold 2 ≤63 off, ≥64 on"), _("Hold 2 ≤63 off, ≥64 on")));
	controller_name_map.insert (make_pair<string,string> (X_("Sound Controller 1 (default: Sound Variation) (Fine)"), _("Sound Ctrlr 1")));
	controller_name_map.insert (make_pair<string,string> (X_("Sound Controller 2 (default: Timbre/Harmonic Intens.) (Fine)"), _("Sound Ctrlr 2")));
	controller_name_map.insert (make_pair<string,string> (X_("Sound Controller 3 (default: Release Time) (Fine)"), _("Sound Ctrlr 3")));
	controller_name_map.insert (make_pair<string,string> (X_("Sound Controller 4 (default: Attack Time) (Fine)"), _("Sound Ctrlr 4")));
	controller_name_map.insert (make_pair<string,string> (X_("Sound Controller 5 (default: Brightness) (Fine)"), _("Sound Ctrlr 5")));
	controller_name_map.insert (make_pair<string,string> (X_("Sound Controller 6 (default: Decay Time) (Fine)"), _("Sound Ctrlr 6")));
	controller_name_map.insert (make_pair<string,string> (X_("Sound Controller 7 (default: Vibrato Rate) (Fine)"), _("Sound Ctrlr 7")));
	controller_name_map.insert (make_pair<string,string> (X_("Sound Controller 8 (default: Vibrato Depth) (Fine)"), _("Sound Ctrlr 8")));
	controller_name_map.insert (make_pair<string,string> (X_("Sound Controller 9 (default: Vibrato Delay) (Fine)"), _("Sound Ctrlr 9")));
	controller_name_map.insert (make_pair<string,string> (X_("Sound Controller 10 (default undefined) (Fine)"), _("Sound Ctrlr 10")));
	controller_name_map.insert (make_pair<string,string> (X_("General Purpose Controller 5 (Fine)"), _("Gen. Ctrlr 5 LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("General Purpose Controller 6 (Fine)"), _("Gen. Ctrlr 6 LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("General Purpose Controller 7 (Fine)"), _("Gen. Ctrlr 7 LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("General Purpose Controller 8 (Fine)"), _("Gen. Ctrlr 8 LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("Portamento Control (Fine)"), _("Portamento (Fine)")));
	controller_name_map.insert (make_pair<string,string> (X_("High Resolution Velocity Prefix (Velocity LSB)"), _("Hi-Res Velo. Prefix")));
	controller_name_map.insert (make_pair<string,string> (X_("Effects 1 Depth (default: Reverb Send Level)"), _("Effects 1 Depth")));
	controller_name_map.insert (make_pair<string,string> (X_("Data Increment (Data Entry +1)"), _("Data Increment")));
	controller_name_map.insert (make_pair<string,string> (X_("Data Decrement (Data Entry -1)"), _("Data Decrement")));
	controller_name_map.insert (make_pair<string,string> (X_("Non-Registered Parameter Number LSB"), _("NRPN LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("Non-Registered Parameter Number MSB"), _("NPRN MSB")));
	controller_name_map.insert (make_pair<string,string> (X_("Registered Parameter Number LSB"), _("RPN LSB")));
	controller_name_map.insert (make_pair<string,string> (X_("Registered Parameter Number MSB"), _("RPN MSB")));
}
