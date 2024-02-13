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

#include <iostream>

#include "pbd/error.h"
#include "pbd/stacktrace.h"

#include "ardour/legatize.h"
#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/rc_configuration.h"
#include "ardour/transpose.h"
#include "ardour/quantize.h"

#include "gtkmm2ext/bindings.h"

#include "widgets/tooltips.h"

#include "actions.h"
#include "ardour_ui.h"
#include "edit_note_dialog.h"
#include "editing_context.h"
#include "editing_convert.h"
#include "editor_drag.h"
#include "keyboard.h"
#include "midi_region_view.h"
#include "note_base.h"
#include "quantize_dialog.h"
#include "selection.h"
#include "selection_memento.h"
#include "transform_dialog.h"
#include "transpose_dialog.h"
#include "verbose_cursor.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace Editing;
using namespace Glib;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace PBD;
using namespace Temporal;
using std::string;

sigc::signal<void> EditingContext::DropDownKeys;
Gtkmm2ext::Bindings* EditingContext::button_bindings = nullptr;
Glib::RefPtr<Gtk::ActionGroup> EditingContext::_midi_actions;
std::queue<EditingContext*> EditingContext::ec_stack;
std::vector<std::string> EditingContext::grid_type_strings;
MouseCursors* EditingContext::_cursors = nullptr;

static const gchar *_grid_type_strings[] = {
	N_("No Grid"),
	N_("Bar"),
	N_("1/4 Note"),
	N_("1/8 Note"),
	N_("1/16 Note"),
	N_("1/32 Note"),
	N_("1/64 Note"),
	N_("1/128 Note"),
	N_("1/3 (8th triplet)"), // or "1/12" ?
	N_("1/6 (16th triplet)"),
	N_("1/12 (32nd triplet)"),
	N_("1/24 (64th triplet)"),
	N_("1/5 (8th quintuplet)"),
	N_("1/10 (16th quintuplet)"),
	N_("1/20 (32nd quintuplet)"),
	N_("1/7 (8th septuplet)"),
	N_("1/14 (16th septuplet)"),
	N_("1/28 (32nd septuplet)"),
	N_("Timecode"),
	N_("MinSec"),
	N_("CD Frames"),
	0
};

EditingContext::EditingContext (std::string const & name)
	: _name (name)
	, rubberband_rect (0)
	, pre_internal_grid_type (GridTypeBeat)
	, pre_internal_snap_mode (SnapOff)
	, internal_grid_type (GridTypeBeat)
	, internal_snap_mode (SnapOff)
	, _grid_type (GridTypeBeat)
	, _snap_mode (SnapOff)
	, _draw_length (GridTypeNone)
	, _draw_velocity (DRAW_VEL_AUTO)
	, _draw_channel (DRAW_CHAN_AUTO)
	, _drags (new DragManager (this))
	, _leftmost_sample (0)
	, _playhead_cursor (nullptr)
	, _snapped_cursor (nullptr)
	, _follow_playhead (false)
	, selection (new Selection (this, true))
	, cut_buffer (new Selection (this, false))
	, _selection_memento (new SelectionMemento())
	, _verbose_cursor (nullptr)
	, samples_per_pixel (2048)
	, zoom_focus (ZoomFocusPlayhead)
	, bbt_ruler_scale (bbt_show_many)
	, bbt_bars (0)
	, bbt_bar_helper_on (0)
	, _visible_canvas_width (0)
	, _visible_canvas_height (0)
	, quantize_dialog (nullptr)
	, vertical_adjustment (0.0, 0.0, 10.0, 400.0)
	, horizontal_adjustment (0.0, 0.0, 1e16)
	, mouse_mode (MouseObject)
{
	if (!button_bindings) {
		button_bindings = new Bindings ("editor-mouse");

		XMLNode* node = button_settings();
		if (node) {
			for (XMLNodeList::const_iterator i = node->children().begin(); i != node->children().end(); ++i) {
				button_bindings->load_operation (**i);
			}
		}
	}

	if (grid_type_strings.empty()) {
		grid_type_strings =  I18N (_grid_type_strings);
	}

	snap_mode_button.set_text (_("Snap"));
	snap_mode_button.set_name ("mouse mode button");

	if (!_cursors) {
		_cursors = new MouseCursors;
		_cursors->set_cursor_set (UIConfiguration::instance().get_icon_set());
		std::cerr << "Set cursor set to " << UIConfiguration::instance().get_icon_set() << std::endl;
	}
}

EditingContext::~EditingContext()
{
}

void
EditingContext::set_session (ARDOUR::Session* s)
{
	SessionHandlePtr::set_session (s);
}

void
EditingContext::set_selected_midi_region_view (MidiRegionView& mrv)
{
	/* clear note selection in all currently selected MidiRegionViews */

	if (get_selection().regions.contains (&mrv) && get_selection().regions.size() == 1) {
		/* Nothing to do */
		return;
	}

	midi_action (&MidiRegionView::clear_note_selection);
	get_selection().set (&mrv);
}

void
EditingContext::register_midi_actions (Bindings* midi_bindings)
{
	if (_midi_actions) {
		return;
	}

	_midi_actions = ActionManager::create_action_group (midi_bindings, X_("Notes"));

	/* two versions to allow same action for Delete and Backspace */

	ActionManager::register_action (_midi_actions, X_("clear-selection"), _("Clear Note Selection"), []() { current_editing_context()->midi_action (&MidiRegionView::clear_note_selection); });
	ActionManager::register_action (_midi_actions, X_("invert-selection"), _("Invert Note Selection"), []() { current_editing_context()->midi_action (&MidiRegionView::invert_selection); });
	ActionManager::register_action (_midi_actions, X_("extend-selection"), _("Extend Note Selection"), []() { current_editing_context()->midi_action (&MidiRegionView::extend_selection); });
	ActionManager::register_action (_midi_actions, X_("duplicate-selection"), _("Duplicate Note Selection"), []() { current_editing_context()->midi_action (&MidiRegionView::duplicate_selection); });

	/* Lengthen */

	ActionManager::register_action (_midi_actions, X_("move-starts-earlier-fine"), _("Move Note Start Earlier (fine)"), []() { current_editing_context()->midi_action (&MidiRegionView::move_note_starts_earlier_fine); });
	ActionManager::register_action (_midi_actions, X_("move-starts-earlier"), _("Move Note Start Earlier"), []() { current_editing_context()->midi_action (&MidiRegionView::move_note_starts_earlier); });
	ActionManager::register_action (_midi_actions, X_("move-ends-later-fine"), _("Move Note Ends Later (fine)"), []() { current_editing_context()->midi_action (&MidiRegionView::move_note_ends_later_fine); });
	ActionManager::register_action (_midi_actions, X_("move-ends-later"), _("Move Note Ends Later"), []() { current_editing_context()->midi_action (&MidiRegionView::move_note_ends_later); });

	/* Shorten */

	ActionManager::register_action (_midi_actions, X_("move-starts-later-fine"), _("Move Note Start Later (fine)"), []() { current_editing_context()->midi_action (&MidiRegionView::move_note_starts_later_fine); });
	ActionManager::register_action (_midi_actions, X_("move-starts-later"), _("Move Note Start Later"), []() { current_editing_context()->midi_action (&MidiRegionView::move_note_starts_later); });
	ActionManager::register_action (_midi_actions, X_("move-ends-earlier-fine"), _("Move Note Ends Earlier (fine)"), []() { current_editing_context()->midi_action (&MidiRegionView::move_note_ends_earlier_fine); });
	ActionManager::register_action (_midi_actions, X_("move-ends-earlier"), _("Move Note Ends Earlier"), []() { current_editing_context()->midi_action (&MidiRegionView::move_note_ends_earlier); });


	/* Alt versions allow bindings for both Tab and ISO_Left_Tab, if desired */

	ActionManager::register_action (_midi_actions, X_("select-next"), _("Select Next"), []() { current_editing_context()->midi_action (&MidiRegionView::select_next_note); });
	ActionManager::register_action (_midi_actions, X_("alt-select-next"), _("Select Next (alternate)"), []() { current_editing_context()->midi_action (&MidiRegionView::select_next_note); });
	ActionManager::register_action (_midi_actions, X_("select-previous"), _("Select Previous"), []() { current_editing_context()->midi_action (&MidiRegionView::select_previous_note); });
	ActionManager::register_action (_midi_actions, X_("alt-select-previous"), _("Select Previous (alternate)"), []() { current_editing_context()->midi_action (&MidiRegionView::select_previous_note); });
	ActionManager::register_action (_midi_actions, X_("add-select-next"), _("Add Next to Selection"), []() { current_editing_context()->midi_action (&MidiRegionView::add_select_next_note); });
	ActionManager::register_action (_midi_actions, X_("alt-add-select-next"), _("Add Next to Selection (alternate)"), []() { current_editing_context()->midi_action (&MidiRegionView::add_select_next_note); });
	ActionManager::register_action (_midi_actions, X_("add-select-previous"), _("Add Previous to Selection"), []() { current_editing_context()->midi_action (&MidiRegionView::add_select_previous_note); });
	ActionManager::register_action (_midi_actions, X_("alt-add-select-previous"), _("Add Previous to Selection (alternate)"), []() { current_editing_context()->midi_action (&MidiRegionView::add_select_previous_note); });

	ActionManager::register_action (_midi_actions, X_("increase-velocity"), _("Increase Velocity"), []() { current_editing_context()->midi_action (&MidiRegionView::increase_note_velocity); });
	ActionManager::register_action (_midi_actions, X_("increase-velocity-fine"), _("Increase Velocity (fine)"), []() { current_editing_context()->midi_action (&MidiRegionView::increase_note_velocity_fine); });
	ActionManager::register_action (_midi_actions, X_("increase-velocity-smush"), _("Increase Velocity (allow mush)"), []() { current_editing_context()->midi_action (&MidiRegionView::increase_note_velocity_smush); });
	ActionManager::register_action (_midi_actions, X_("increase-velocity-together"), _("Increase Velocity (non-relative)"), []() { current_editing_context()->midi_action (&MidiRegionView::increase_note_velocity_together); });
	ActionManager::register_action (_midi_actions, X_("increase-velocity-fine-smush"), _("Increase Velocity (fine, allow mush)"), []() { current_editing_context()->midi_action (&MidiRegionView::increase_note_velocity_fine_smush); });
	ActionManager::register_action (_midi_actions, X_("increase-velocity-fine-together"), _("Increase Velocity (fine, non-relative)"), []() { current_editing_context()->midi_action (&MidiRegionView::increase_note_velocity_fine_together); });
	ActionManager::register_action (_midi_actions, X_("increase-velocity-smush-together"), _("Increase Velocity (maintain ratios, allow mush)"), []() { current_editing_context()->midi_action (&MidiRegionView::increase_note_velocity_smush_together); });
	ActionManager::register_action (_midi_actions, X_("increase-velocity-fine-smush-together"), _("Increase Velocity (fine, allow mush, non-relative)"), []() { current_editing_context()->midi_action (&MidiRegionView::increase_note_velocity_fine_smush_together); });

	ActionManager::register_action (_midi_actions, X_("decrease-velocity"), _("Decrease Velocity"), []() { current_editing_context()->midi_action (&MidiRegionView::decrease_note_velocity); });
	ActionManager::register_action (_midi_actions, X_("decrease-velocity-fine"), _("Decrease Velocity (fine)"), []() { current_editing_context()->midi_action (&MidiRegionView::decrease_note_velocity_fine); });
	ActionManager::register_action (_midi_actions, X_("decrease-velocity-smush"), _("Decrease Velocity (allow mush)"), []() { current_editing_context()->midi_action (&MidiRegionView::decrease_note_velocity_smush); });
	ActionManager::register_action (_midi_actions, X_("decrease-velocity-together"), _("Decrease Velocity (non-relative)"), []() { current_editing_context()->midi_action (&MidiRegionView::decrease_note_velocity_together); });
	ActionManager::register_action (_midi_actions, X_("decrease-velocity-fine-smush"), _("Decrease Velocity (fine, allow mush)"), []() { current_editing_context()->midi_action (&MidiRegionView::decrease_note_velocity_fine_smush); });
	ActionManager::register_action (_midi_actions, X_("decrease-velocity-fine-together"), _("Decrease Velocity (fine, non-relative)"), []() { current_editing_context()->midi_action (&MidiRegionView::decrease_note_velocity_fine_together); });
	ActionManager::register_action (_midi_actions, X_("decrease-velocity-smush-together"), _("Decrease Velocity (maintain ratios, allow mush)"), []() { current_editing_context()->midi_action (&MidiRegionView::decrease_note_velocity_smush_together); });
	ActionManager::register_action (_midi_actions, X_("decrease-velocity-fine-smush-together"), _("Decrease Velocity (fine, allow mush, non-relative)"), []() { current_editing_context()->midi_action (&MidiRegionView::decrease_note_velocity_fine_smush_together); });

	ActionManager::register_action (_midi_actions, X_("transpose-up-octave"), _("Transpose Up (octave)"), []() { current_editing_context()->midi_action (&MidiRegionView::transpose_up_octave); });
	ActionManager::register_action (_midi_actions, X_("transpose-up-octave-smush"), _("Transpose Up (octave, allow mush)"), []() { current_editing_context()->midi_action (&MidiRegionView::transpose_up_octave_smush); });
	ActionManager::register_action (_midi_actions, X_("transpose-up-semitone"), _("Transpose Up (semitone)"), []() { current_editing_context()->midi_action (&MidiRegionView::transpose_up_tone); });
	ActionManager::register_action (_midi_actions, X_("transpose-up-semitone-smush"), _("Transpose Up (semitone, allow mush)"), []() { current_editing_context()->midi_action (&MidiRegionView::transpose_up_octave_smush); });

	ActionManager::register_action (_midi_actions, X_("transpose-down-octave"), _("Transpose Down (octave)"), []() { current_editing_context()->midi_action (&MidiRegionView::transpose_down_octave); });
	ActionManager::register_action (_midi_actions, X_("transpose-down-octave-smush"), _("Transpose Down (octave, allow mush)"), []() { current_editing_context()->midi_action (&MidiRegionView::transpose_down_octave_smush); });
	ActionManager::register_action (_midi_actions, X_("transpose-down-semitone"), _("Transpose Down (semitone)"), []() { current_editing_context()->midi_action (&MidiRegionView::transpose_down_tone); });
	ActionManager::register_action (_midi_actions, X_("transpose-down-semitone-smush"), _("Transpose Down (semitone, allow mush)"), []() { current_editing_context()->midi_action (&MidiRegionView::transpose_down_octave_smush); });

	ActionManager::register_action (_midi_actions, X_("nudge-later"), _("Nudge Notes Later (grid)"), []() { current_editing_context()->midi_action (&MidiRegionView::nudge_notes_later); });
	ActionManager::register_action (_midi_actions, X_("nudge-later-fine"), _("Nudge Notes Later (1/4 grid)"), []() { current_editing_context()->midi_action (&MidiRegionView::nudge_notes_later_fine); });
	ActionManager::register_action (_midi_actions, X_("nudge-earlier"), _("Nudge Notes Earlier (grid)"), []() { current_editing_context()->midi_action (&MidiRegionView::nudge_notes_earlier); });
	ActionManager::register_action (_midi_actions, X_("nudge-earlier-fine"), _("Nudge Notes Earlier (1/4 grid)"), []() { current_editing_context()->midi_action (&MidiRegionView::nudge_notes_earlier_fine); });

	ActionManager::register_action (_midi_actions, X_("split-notes-grid"), _("Split Selected Notes on grid boundaries"), []() { current_editing_context()->midi_action (&MidiRegionView::split_notes_grid); });
	ActionManager::register_action (_midi_actions, X_("split-notes-more"), _("Split Selected Notes into more pieces"), []() { current_editing_context()->midi_action (&MidiRegionView::split_notes_more); });
	ActionManager::register_action (_midi_actions, X_("split-notes-less"), _("Split Selected Notes into less pieces"), []() { current_editing_context()->midi_action (&MidiRegionView::split_notes_less); });
	ActionManager::register_action (_midi_actions, X_("join-notes"), _("Join Selected Notes"), []() { current_editing_context()->midi_action (&MidiRegionView::join_notes); });

	ActionManager::register_action (_midi_actions, X_("edit-channels"), _("Edit Note Channels"), []() { current_editing_context()->midi_action (&MidiRegionView::channel_edit); });
	ActionManager::register_action (_midi_actions, X_("edit-velocities"), _("Edit Note Velocities"), []() { current_editing_context()->midi_action (&MidiRegionView::velocity_edit); });

	ActionManager::register_action (_midi_actions, X_("quantize-selected-notes"), _("Quantize Selected Notes"), []() { current_editing_context()->midi_action ( &MidiRegionView::quantize_selected_notes); });

	Glib::RefPtr<ActionGroup> length_actions = ActionManager::create_action_group (midi_bindings, X_("DrawLength"));
	RadioAction::Group draw_length_group;

	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-thirtyseconds"),  grid_type_strings[(int)GridTypeBeatDiv32].c_str(), (sigc::bind (sigc::ptr_fun (&EditingContext::_draw_length_chosen), Editing::GridTypeBeatDiv32)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-twentyeighths"),  grid_type_strings[(int)GridTypeBeatDiv28].c_str(), (sigc::bind (sigc::ptr_fun (&EditingContext::_draw_length_chosen), Editing::GridTypeBeatDiv28)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-twentyfourths"),  grid_type_strings[(int)GridTypeBeatDiv24].c_str(), (sigc::bind (sigc::ptr_fun (&EditingContext::_draw_length_chosen), Editing::GridTypeBeatDiv24)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-twentieths"),     grid_type_strings[(int)GridTypeBeatDiv20].c_str(), (sigc::bind (sigc::ptr_fun (&EditingContext::_draw_length_chosen), Editing::GridTypeBeatDiv20)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-asixteenthbeat"), grid_type_strings[(int)GridTypeBeatDiv16].c_str(), (sigc::bind (sigc::ptr_fun (&EditingContext::_draw_length_chosen), Editing::GridTypeBeatDiv16)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-fourteenths"),    grid_type_strings[(int)GridTypeBeatDiv14].c_str(), (sigc::bind (sigc::ptr_fun (&EditingContext::_draw_length_chosen), Editing::GridTypeBeatDiv14)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-twelfths"),       grid_type_strings[(int)GridTypeBeatDiv12].c_str(), (sigc::bind (sigc::ptr_fun (&EditingContext::_draw_length_chosen), Editing::GridTypeBeatDiv12)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-tenths"),         grid_type_strings[(int)GridTypeBeatDiv10].c_str(), (sigc::bind (sigc::ptr_fun (&EditingContext::_draw_length_chosen), Editing::GridTypeBeatDiv10)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-eighths"),        grid_type_strings[(int)GridTypeBeatDiv8].c_str(),  (sigc::bind (sigc::ptr_fun (&EditingContext::_draw_length_chosen), Editing::GridTypeBeatDiv8)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-sevenths"),       grid_type_strings[(int)GridTypeBeatDiv7].c_str(),  (sigc::bind (sigc::ptr_fun (&EditingContext::_draw_length_chosen), Editing::GridTypeBeatDiv7)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-sixths"),         grid_type_strings[(int)GridTypeBeatDiv6].c_str(),  (sigc::bind (sigc::ptr_fun (&EditingContext::_draw_length_chosen), Editing::GridTypeBeatDiv6)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-fifths"),         grid_type_strings[(int)GridTypeBeatDiv5].c_str(),  (sigc::bind (sigc::ptr_fun (&EditingContext::_draw_length_chosen), Editing::GridTypeBeatDiv5)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-quarters"),       grid_type_strings[(int)GridTypeBeatDiv4].c_str(),  (sigc::bind (sigc::ptr_fun (&EditingContext::_draw_length_chosen), Editing::GridTypeBeatDiv4)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-thirds"),         grid_type_strings[(int)GridTypeBeatDiv3].c_str(),  (sigc::bind (sigc::ptr_fun (&EditingContext::_draw_length_chosen), Editing::GridTypeBeatDiv3)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-halves"),         grid_type_strings[(int)GridTypeBeatDiv2].c_str(),  (sigc::bind (sigc::ptr_fun (&EditingContext::_draw_length_chosen), Editing::GridTypeBeatDiv2)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-beat"),           grid_type_strings[(int)GridTypeBeat].c_str(),      (sigc::bind (sigc::ptr_fun (&EditingContext::_draw_length_chosen), Editing::GridTypeBeat)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-bar"),            grid_type_strings[(int)GridTypeBar].c_str(),       (sigc::bind (sigc::ptr_fun (&EditingContext::_draw_length_chosen), Editing::GridTypeBar)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-auto"),           _("Auto"),                                         (sigc::bind (sigc::ptr_fun (&EditingContext::_draw_length_chosen), DRAW_LEN_AUTO)));

	Glib::RefPtr<ActionGroup> velocity_actions = ActionManager::create_action_group (midi_bindings, _("Draw Velocity"));
	RadioAction::Group draw_velocity_group;
	ActionManager::register_radio_action (velocity_actions, draw_velocity_group, X_("draw-velocity-auto"),  _("Auto"), (sigc::bind (sigc::ptr_fun(&EditingContext::_draw_velocity_chosen), DRAW_VEL_AUTO)));
	for (int i = 1; i <= 127; i++) {
		char buf[64];
		sprintf(buf, X_("draw-velocity-%d"), i);
		char vel[64];
		sprintf(vel, _("Velocity %d"), i);
		ActionManager::register_radio_action (velocity_actions, draw_velocity_group, buf, vel, (sigc::bind (sigc::ptr_fun(&EditingContext::_draw_velocity_chosen), i)));
	}

	Glib::RefPtr<ActionGroup> channel_actions = ActionManager::create_action_group (midi_bindings, _("Draw Channel"));
	RadioAction::Group draw_channel_group;
	ActionManager::register_radio_action (channel_actions, draw_channel_group, X_("draw-channel-auto"),  _("Auto"), (sigc::bind (sigc::ptr_fun(&EditingContext::_draw_channel_chosen), DRAW_CHAN_AUTO)));
	for (int i = 0; i <= 15; i++) {
		char buf[64];
		sprintf(buf, X_("draw-channel-%d"), i+1);
		char ch[64];
		sprintf(ch, X_("Channel %d"), i+1);
		ActionManager::register_radio_action (channel_actions, draw_channel_group, buf, ch, (sigc::bind (sigc::ptr_fun(&EditingContext::_draw_channel_chosen), i)));
	}

	ActionManager::set_sensitive (_midi_actions, false);
}

void
EditingContext::midi_action (void (MidiView::*method)())
{
	MidiRegionSelection ms = get_selection().midi_regions();

	if (ms.empty()) {
		return;
	}

	if (ms.size() > 1) {

		auto views = filter_to_unique_midi_region_views (ms);

		for (auto & mrv : views) {
			(mrv->*method) ();
		}

	} else {

		MidiRegionView* mrv = dynamic_cast<MidiRegionView*>(ms.front());

		if (mrv) {
			(mrv->*method)();
		}
	}
}

void
EditingContext::grid_type_selection_done (GridType gridtype)
{
	RefPtr<RadioAction> ract = grid_type_action (gridtype);
	if (ract && ract->get_active()) {  /*radio-action is already set*/
		set_grid_to(gridtype);         /*so we must set internal state here*/
	} else {
		ract->set_active ();
	}
}

void
EditingContext::draw_length_selection_done (GridType gridtype)
{
	RefPtr<RadioAction> ract = draw_length_action (gridtype);
	if (ract && ract->get_active()) {  /*radio-action is already set*/
		set_draw_length_to(gridtype);  /*so we must set internal state here*/
	} else {
		ract->set_active ();
	}
}

void
EditingContext::draw_velocity_selection_done (int v)
{
	RefPtr<RadioAction> ract = draw_velocity_action (v);
	if (ract && ract->get_active()) {  /*radio-action is already set*/
		set_draw_velocity_to(v);       /*so we must set internal state here*/
	} else {
		ract->set_active ();
	}
}

void
EditingContext::draw_channel_selection_done (int c)
{
	RefPtr<RadioAction> ract = draw_channel_action (c);
	if (ract && ract->get_active()) {  /*radio-action is already set*/
		set_draw_channel_to(c);        /*so we must set internal state here*/
	} else {
		ract->set_active ();
	}
}

void
EditingContext::snap_mode_selection_done (SnapMode mode)
{
	RefPtr<RadioAction> ract = snap_mode_action (mode);

	if (ract) {
		ract->set_active (true);
	}
}

RefPtr<RadioAction>
EditingContext::grid_type_action (GridType type)
{
	const char* action = 0;
	RefPtr<Action> act;

	switch (type) {
	case Editing::GridTypeBeatDiv32:
		action = "grid-type-thirtyseconds";
		break;
	case Editing::GridTypeBeatDiv28:
		action = "grid-type-twentyeighths";
		break;
	case Editing::GridTypeBeatDiv24:
		action = "grid-type-twentyfourths";
		break;
	case Editing::GridTypeBeatDiv20:
		action = "grid-type-twentieths";
		break;
	case Editing::GridTypeBeatDiv16:
		action = "grid-type-asixteenthbeat";
		break;
	case Editing::GridTypeBeatDiv14:
		action = "grid-type-fourteenths";
		break;
	case Editing::GridTypeBeatDiv12:
		action = "grid-type-twelfths";
		break;
	case Editing::GridTypeBeatDiv10:
		action = "grid-type-tenths";
		break;
	case Editing::GridTypeBeatDiv8:
		action = "grid-type-eighths";
		break;
	case Editing::GridTypeBeatDiv7:
		action = "grid-type-sevenths";
		break;
	case Editing::GridTypeBeatDiv6:
		action = "grid-type-sixths";
		break;
	case Editing::GridTypeBeatDiv5:
		action = "grid-type-fifths";
		break;
	case Editing::GridTypeBeatDiv4:
		action = "grid-type-quarters";
		break;
	case Editing::GridTypeBeatDiv3:
		action = "grid-type-thirds";
		break;
	case Editing::GridTypeBeatDiv2:
		action = "grid-type-halves";
		break;
	case Editing::GridTypeBeat:
		action = "grid-type-beat";
		break;
	case Editing::GridTypeBar:
		action = "grid-type-bar";
		break;
	case Editing::GridTypeNone:
		action = "grid-type-none";
		break;
	case Editing::GridTypeTimecode:
		action = "grid-type-timecode";
		break;
	case Editing::GridTypeCDFrame:
		action = "grid-type-cdframe";
		break;
	case Editing::GridTypeMinSec:
		action = "grid-type-minsec";
		break;
	default:
		fatal << string_compose (_("programming error: %1: %2"), "Editor: impossible snap-to type", (int) type) << endmsg;
		abort(); /*NOTREACHED*/
	}

	act = ActionManager::get_action (X_("Snap"), action);

	if (act) {
		RefPtr<RadioAction> ract = RefPtr<RadioAction>::cast_dynamic(act);
		return ract;

	} else  {
		error << string_compose (_("programming error: %1"), "EditingContext::grid_type_chosen could not find action to match type.") << endmsg;
		return RefPtr<RadioAction>();
	}
}

void
EditingContext::next_grid_choice ()
{
	switch (_grid_type) {
	case Editing::GridTypeBeatDiv32:
		set_grid_to (Editing::GridTypeNone);
		break;
	case Editing::GridTypeBeatDiv16:
		set_grid_to (Editing::GridTypeBeatDiv32);
		break;
	case Editing::GridTypeBeatDiv8:
		set_grid_to (Editing::GridTypeBeatDiv16);
		break;
	case Editing::GridTypeBeatDiv4:
		set_grid_to (Editing::GridTypeBeatDiv8);
		break;
	case Editing::GridTypeBeatDiv2:
		set_grid_to (Editing::GridTypeBeatDiv4);
		break;
	case Editing::GridTypeBeat:
		set_grid_to (Editing::GridTypeBeatDiv2);
		break;
	case Editing::GridTypeBar:
		set_grid_to (Editing::GridTypeBeat);
		break;
	case Editing::GridTypeNone:
		set_grid_to (Editing::GridTypeBar);
		break;
	case Editing::GridTypeBeatDiv3:
	case Editing::GridTypeBeatDiv6:
	case Editing::GridTypeBeatDiv12:
	case Editing::GridTypeBeatDiv24:
	case Editing::GridTypeBeatDiv5:
	case Editing::GridTypeBeatDiv10:
	case Editing::GridTypeBeatDiv20:
	case Editing::GridTypeBeatDiv7:
	case Editing::GridTypeBeatDiv14:
	case Editing::GridTypeBeatDiv28:
	case Editing::GridTypeTimecode:
	case Editing::GridTypeMinSec:
	case Editing::GridTypeCDFrame:
		break;  //do nothing
	}
}

void
EditingContext::prev_grid_choice ()
{
	switch (_grid_type) {
	case Editing::GridTypeBeatDiv32:
		set_grid_to (Editing::GridTypeBeatDiv16);
		break;
	case Editing::GridTypeBeatDiv16:
		set_grid_to (Editing::GridTypeBeatDiv8);
		break;
	case Editing::GridTypeBeatDiv8:
		set_grid_to (Editing::GridTypeBeatDiv4);
		break;
	case Editing::GridTypeBeatDiv4:
		set_grid_to (Editing::GridTypeBeatDiv2);
		break;
	case Editing::GridTypeBeatDiv2:
		set_grid_to (Editing::GridTypeBeat);
		break;
	case Editing::GridTypeBeat:
		set_grid_to (Editing::GridTypeBar);
		break;
	case Editing::GridTypeBar:
		set_grid_to (Editing::GridTypeNone);
		break;
	case Editing::GridTypeNone:
		set_grid_to (Editing::GridTypeBeatDiv32);
		break;
	case Editing::GridTypeBeatDiv3:
	case Editing::GridTypeBeatDiv6:
	case Editing::GridTypeBeatDiv12:
	case Editing::GridTypeBeatDiv24:
	case Editing::GridTypeBeatDiv5:
	case Editing::GridTypeBeatDiv10:
	case Editing::GridTypeBeatDiv20:
	case Editing::GridTypeBeatDiv7:
	case Editing::GridTypeBeatDiv14:
	case Editing::GridTypeBeatDiv28:
	case Editing::GridTypeTimecode:
	case Editing::GridTypeMinSec:
	case Editing::GridTypeCDFrame:
		break;  //do nothing
	}
}

void
EditingContext::grid_type_chosen (GridType type)
{
	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

	RefPtr<RadioAction> ract = grid_type_action (type);

	if (ract && ract->get_active()) {
		set_grid_to (type);
	}
}

void
EditingContext::_draw_length_chosen (GridType type)
{
	if (current_editing_context()) {
		current_editing_context()->draw_length_chosen (type);
	}
}

void
EditingContext::draw_length_chosen (GridType type)
{
	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

	RefPtr<RadioAction> ract = draw_length_action (type);

	if (ract && ract->get_active()) {
		set_draw_length_to (type);
	}
}

void
EditingContext::_draw_velocity_chosen (int v)
{
	if (current_editing_context()) {
		current_editing_context()->draw_velocity_action (v);
	}
}

void
EditingContext::draw_velocity_chosen (int v)
{
	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

	RefPtr<RadioAction> ract = draw_velocity_action (v);

	if (ract && ract->get_active()) {
		set_draw_velocity_to (v);
	}
}

void
EditingContext::_draw_channel_chosen (int c)
{
	if (current_editing_context()) {
		current_editing_context()->draw_channel_chosen (c);
	}
}

void
EditingContext::draw_channel_chosen (int c)
{
	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

	RefPtr<RadioAction> ract = draw_channel_action (c);

	if (ract && ract->get_active()) {
		set_draw_channel_to (c);
	}
}

RefPtr<RadioAction>
EditingContext::snap_mode_action (SnapMode mode)
{
	const char* action = 0;
	RefPtr<Action> act;

	switch (mode) {
	case Editing::SnapOff:
		action = X_("snap-off");
		break;
	case Editing::SnapNormal:
		action = X_("snap-normal");
		break;
	case Editing::SnapMagnetic:
		action = X_("snap-magnetic");
		break;
	default:
		fatal << string_compose (_("programming error: %1: %2"), "Editor: impossible snap mode type", (int) mode) << endmsg;
		abort(); /*NOTREACHED*/
	}

	act = ActionManager::get_action (X_("Editor"), action);

	if (act) {
		RefPtr<RadioAction> ract = RefPtr<RadioAction>::cast_dynamic(act);
		return ract;

	} else  {
		error << string_compose (_("programming error: %1: %2"), "EditingContext::snap_mode_chosen could not find action to match mode.", action) << endmsg;
		return RefPtr<RadioAction> ();
	}
}

void
EditingContext::cycle_snap_mode ()
{
	switch (_snap_mode) {
	case SnapOff:
	case SnapNormal:
		set_snap_mode (SnapMagnetic);
		break;
	case SnapMagnetic:
		set_snap_mode (SnapOff);
		break;
	}
}

void
EditingContext::snap_mode_chosen (SnapMode mode)
{
	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

	if (mode == SnapNormal) {
		mode = SnapMagnetic;
	}

	RefPtr<RadioAction> ract = snap_mode_action (mode);

	if (ract && ract->get_active()) {
		set_snap_mode (mode);
	}
}

GridType
EditingContext::grid_type() const
{
	return _grid_type;
}

GridType
EditingContext::draw_length() const
{
	return _draw_length;
}

int
EditingContext::draw_velocity() const
{
	return _draw_velocity;
}

int
EditingContext::draw_channel() const
{
	return _draw_channel;
}

bool
EditingContext::grid_musical() const
{
	return grid_type_is_musical (_grid_type);
}

bool
EditingContext::grid_type_is_musical(GridType gt) const
{
	switch (gt) {
	case GridTypeBeatDiv32:
	case GridTypeBeatDiv28:
	case GridTypeBeatDiv24:
	case GridTypeBeatDiv20:
	case GridTypeBeatDiv16:
	case GridTypeBeatDiv14:
	case GridTypeBeatDiv12:
	case GridTypeBeatDiv10:
	case GridTypeBeatDiv8:
	case GridTypeBeatDiv7:
	case GridTypeBeatDiv6:
	case GridTypeBeatDiv5:
	case GridTypeBeatDiv4:
	case GridTypeBeatDiv3:
	case GridTypeBeatDiv2:
	case GridTypeBeat:
	case GridTypeBar:
		return true;
	case GridTypeNone:
	case GridTypeTimecode:
	case GridTypeMinSec:
	case GridTypeCDFrame:
		return false;
	}
	return false;
}

SnapMode
EditingContext::snap_mode() const
{
	return _snap_mode;
}

void
EditingContext::set_draw_length_to (GridType gt)
{
	if ( !grid_type_is_musical(gt) ) {  //range-check
		gt = DRAW_LEN_AUTO;
	}

	_draw_length = gt;

	if (DRAW_LEN_AUTO==gt) {
		draw_length_selector.set_text (_("Auto"));
		return;
	}

	unsigned int grid_index = (unsigned int)gt;
	std::string str = grid_type_strings[grid_index];
	if (str != draw_length_selector.get_text()) {
		draw_length_selector.set_text (str);
	}

	instant_save ();
}

void
EditingContext::set_draw_velocity_to (int v)
{
	if ( v<0 || v>127 ) {  //range-check midi channel
		v = DRAW_VEL_AUTO;
	}

	_draw_velocity = v;

	if (DRAW_VEL_AUTO==v) {
		draw_velocity_selector.set_text (_("Auto"));
		return;
	}

	char buf[64];
	sprintf(buf, "%d", v );
	draw_velocity_selector.set_text (buf);

	instant_save ();
}

void
EditingContext::set_draw_channel_to (int c)
{
	if ( c<0 || c>15 ) {  //range-check midi channel
		c = DRAW_CHAN_AUTO;
	}

	_draw_channel = c;

	if (DRAW_CHAN_AUTO==c) {
		draw_channel_selector.set_text (_("Auto"));
		return;
	}

	char buf[64];
	sprintf(buf, "%d", c+1 );
	draw_channel_selector.set_text (buf);

	instant_save ();
}

void
EditingContext::set_grid_to (GridType gt)
{
	unsigned int grid_ind = (unsigned int)gt;

	if (internal_editing() && UIConfiguration::instance().get_grid_follows_internal()) {
		internal_grid_type = gt;
	} else {
		pre_internal_grid_type = gt;
	}

	bool grid_type_changed = true;
	if ( grid_type_is_musical(_grid_type) && grid_type_is_musical(gt))
		grid_type_changed = false;

	_grid_type = gt;

	if (grid_ind > grid_type_strings.size() - 1) {
		grid_ind = 0;
		_grid_type = (GridType)grid_ind;
	}

	std::string str = grid_type_strings[grid_ind];

	if (str != grid_type_selector.get_text()) {
		grid_type_selector.set_text (str);
	}

	if (grid_type_changed && UIConfiguration::instance().get_show_grids_ruler()) {
		show_rulers_for_grid ();
	}

	instant_save ();

	const bool grid_is_musical = grid_musical ();

	if (grid_is_musical) {
		compute_bbt_ruler_scale (_leftmost_sample, _leftmost_sample + current_page_samples());
		update_tempo_based_rulers ();
	} else if (current_mouse_mode () == Editing::MouseGrid) {
		Glib::RefPtr<RadioAction> ract = Glib::RefPtr<RadioAction>::cast_dynamic (get_mouse_mode_action (Editing::MouseObject));
		ract->set_active (true);
	}

	get_mouse_mode_action (Editing::MouseGrid)->set_sensitive (grid_is_musical);

	mark_region_boundary_cache_dirty ();

	redisplay_grid (false);

	SnapChanged (); /* EMIT SIGNAL */
}

void
EditingContext::set_snap_mode (SnapMode mode)
{
	if (internal_editing()) {
		internal_snap_mode = mode;
	} else {
		pre_internal_snap_mode = mode;
	}

	_snap_mode = mode;

	if (_snap_mode == SnapOff) {
		snap_mode_button.set_active_state (Gtkmm2ext::Off);
	} else {
		snap_mode_button.set_active_state (Gtkmm2ext::ExplicitActive);
	}

	instant_save ();
}


RefPtr<RadioAction>
EditingContext::draw_velocity_action (int v)
{
	char buf[64];
	const char* action = 0;
	RefPtr<Action> act;

	if (v==DRAW_VEL_AUTO) {
		action = "draw-velocity-auto";
	} else if (v>=1 && v<=127) {
		sprintf(buf, X_("draw-velocity-%d"), v);  //we don't allow drawing a velocity 0;  some synths use that as note-off
		action = buf;
	}

	act = ActionManager::get_action (_("Draw Velocity"), action);
	if (act) {
		RefPtr<RadioAction> ract = RefPtr<RadioAction>::cast_dynamic(act);
		return ract;
	} else  {
		error << string_compose (_("programming error: %1"), "EditingContext::draw_velocity_action could not find action to match velocity.") << endmsg;
		return RefPtr<RadioAction>();
	}
}

RefPtr<RadioAction>
EditingContext::draw_channel_action (int c)
{
	char buf[64];
	const char* action = 0;
	RefPtr<Action> act;

	if (c==DRAW_CHAN_AUTO) {
		action = "draw-channel-auto";
	} else if (c>=0 && c<=15) {
		sprintf(buf, X_("draw-channel-%d"), c+1);
		action = buf;
	}

	act = ActionManager::get_action (_("Draw Channel"), action);
	if (act) {
		RefPtr<RadioAction> ract = RefPtr<RadioAction>::cast_dynamic(act);
		return ract;
	} else  {
		error << string_compose (_("programming error: %1"), "EditingContext::draw_channel_action could not find action to match channel.") << endmsg;
		return RefPtr<RadioAction>();
	}
}

RefPtr<RadioAction>
EditingContext::draw_length_action (GridType type)
{
	const char* action = 0;
	RefPtr<Action> act;

	switch (type) {
	case Editing::GridTypeBeatDiv32:
		action = "draw-length-thirtyseconds";
		break;
	case Editing::GridTypeBeatDiv28:
		action = "draw-length-twentyeighths";
		break;
	case Editing::GridTypeBeatDiv24:
		action = "draw-length-twentyfourths";
		break;
	case Editing::GridTypeBeatDiv20:
		action = "draw-length-twentieths";
		break;
	case Editing::GridTypeBeatDiv16:
		action = "draw-length-asixteenthbeat";
		break;
	case Editing::GridTypeBeatDiv14:
		action = "draw-length-fourteenths";
		break;
	case Editing::GridTypeBeatDiv12:
		action = "draw-length-twelfths";
		break;
	case Editing::GridTypeBeatDiv10:
		action = "draw-length-tenths";
		break;
	case Editing::GridTypeBeatDiv8:
		action = "draw-length-eighths";
		break;
	case Editing::GridTypeBeatDiv7:
		action = "draw-length-sevenths";
		break;
	case Editing::GridTypeBeatDiv6:
		action = "draw-length-sixths";
		break;
	case Editing::GridTypeBeatDiv5:
		action = "draw-length-fifths";
		break;
	case Editing::GridTypeBeatDiv4:
		action = "draw-length-quarters";
		break;
	case Editing::GridTypeBeatDiv3:
		action = "draw-length-thirds";
		break;
	case Editing::GridTypeBeatDiv2:
		action = "draw-length-halves";
		break;
	case Editing::GridTypeBeat:
		action = "draw-length-beat";
		break;
	case Editing::GridTypeBar:
		action = "draw-length-bar";
		break;
	case Editing::GridTypeNone:
		action = "draw-length-auto";
		break;
	case Editing::GridTypeTimecode:
	case Editing::GridTypeCDFrame:
	case Editing::GridTypeMinSec:
	default:
		fatal << string_compose (_("programming error: %1: %2"), "Editor: impossible grid length type", (int) type) << endmsg;
		abort(); /*NOTREACHED*/
	}

	act = ActionManager::get_action (X_("DrawLength"), action);

	if (act) {
		RefPtr<RadioAction> ract = RefPtr<RadioAction>::cast_dynamic(act);
		return ract;

	} else  {
		error << string_compose (_("programming error: %1"), "EditingContext::draw_length_chosen could not find action to match type.") << endmsg;
		return RefPtr<RadioAction>();
	}
}

void
EditingContext::build_grid_type_menu ()
{
	using namespace Menu_Helpers;

	/* there's no Grid, but if Snap is engaged, the Snap preferences will be applied */
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeNone],      sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeNone)));
	grid_type_selector.AddMenuElem(SeparatorElem());

	/* musical grid: bars, quarter-notes, etc */
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBar],       sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBar)));
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeat],      sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeat)));
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeatDiv2],  sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeatDiv2)));
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeatDiv4],  sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeatDiv4)));
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeatDiv8],  sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeatDiv8)));
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeatDiv16], sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeatDiv16)));
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeatDiv32], sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeatDiv32)));

	/* triplet grid */
	grid_type_selector.AddMenuElem(SeparatorElem());
	Gtk::Menu *_triplet_menu = manage (new Menu);
	MenuList& triplet_items (_triplet_menu->items());
	{
		triplet_items.push_back (MenuElem (grid_type_strings[(int)GridTypeBeatDiv3],  sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeatDiv3)));
		triplet_items.push_back (MenuElem (grid_type_strings[(int)GridTypeBeatDiv6],  sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeatDiv6)));
		triplet_items.push_back (MenuElem (grid_type_strings[(int)GridTypeBeatDiv12], sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeatDiv12)));
		triplet_items.push_back (MenuElem (grid_type_strings[(int)GridTypeBeatDiv24], sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeatDiv24)));
	}
	grid_type_selector.AddMenuElem (Menu_Helpers::MenuElem (_("Triplets"), *_triplet_menu));

	/* quintuplet grid */
	Gtk::Menu *_quintuplet_menu = manage (new Menu);
	MenuList& quintuplet_items (_quintuplet_menu->items());
	{
		quintuplet_items.push_back (MenuElem (grid_type_strings[(int)GridTypeBeatDiv5],  sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeatDiv5)));
		quintuplet_items.push_back (MenuElem (grid_type_strings[(int)GridTypeBeatDiv10], sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeatDiv10)));
		quintuplet_items.push_back (MenuElem (grid_type_strings[(int)GridTypeBeatDiv20], sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeatDiv20)));
	}
	grid_type_selector.AddMenuElem (Menu_Helpers::MenuElem (_("Quintuplets"), *_quintuplet_menu));

	/* septuplet grid */
	Gtk::Menu *_septuplet_menu = manage (new Menu);
	MenuList& septuplet_items (_septuplet_menu->items());
	{
		septuplet_items.push_back (MenuElem (grid_type_strings[(int)GridTypeBeatDiv7],  sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeatDiv7)));
		septuplet_items.push_back (MenuElem (grid_type_strings[(int)GridTypeBeatDiv14], sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeatDiv14)));
		septuplet_items.push_back (MenuElem (grid_type_strings[(int)GridTypeBeatDiv28], sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeatDiv28)));
	}
	grid_type_selector.AddMenuElem (Menu_Helpers::MenuElem (_("Septuplets"), *_septuplet_menu));

	grid_type_selector.AddMenuElem(SeparatorElem());
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeTimecode], sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeTimecode)));
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeMinSec], sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeMinSec)));
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeCDFrame], sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeCDFrame)));

	grid_type_selector.set_sizing_texts (grid_type_strings);
}

void
EditingContext::build_draw_midi_menus ()
{
	using namespace Menu_Helpers;

	/* Note-Length when drawing */
	draw_length_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeat],      sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_length_selection_done), (GridType) GridTypeBeat)));
	draw_length_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeatDiv2],  sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_length_selection_done), (GridType) GridTypeBeatDiv2)));
	draw_length_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeatDiv4],  sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_length_selection_done), (GridType) GridTypeBeatDiv4)));
	draw_length_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeatDiv8],  sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_length_selection_done), (GridType) GridTypeBeatDiv8)));
	draw_length_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeatDiv16], sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_length_selection_done), (GridType) GridTypeBeatDiv16)));
	draw_length_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeatDiv32], sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_length_selection_done), (GridType) GridTypeBeatDiv32)));
	draw_length_selector.AddMenuElem (MenuElem (_("Auto"), sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_length_selection_done), (GridType) DRAW_LEN_AUTO)));

	{
		std::vector<std::string> draw_grid_type_strings = {grid_type_strings.begin() + GridTypeBeat, grid_type_strings.begin() + GridTypeBeatDiv32 + 1};
		draw_grid_type_strings.push_back (_("Auto"));
		grid_type_selector.set_sizing_texts (draw_grid_type_strings);
	}

	/* Note-Velocity when drawing */
	draw_velocity_selector.AddMenuElem (MenuElem ("8",    sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_velocity_selection_done), 8)));
	draw_velocity_selector.AddMenuElem (MenuElem ("32",   sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_velocity_selection_done), 32)));
	draw_velocity_selector.AddMenuElem (MenuElem ("64",   sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_velocity_selection_done), 64)));
	draw_velocity_selector.AddMenuElem (MenuElem ("82",   sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_velocity_selection_done), 82)));
	draw_velocity_selector.AddMenuElem (MenuElem ("100",  sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_velocity_selection_done), 100)));
	draw_velocity_selector.AddMenuElem (MenuElem ("127",  sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_velocity_selection_done), 127)));
	draw_velocity_selector.AddMenuElem (MenuElem (_("Auto"), sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_velocity_selection_done), DRAW_VEL_AUTO)));

	/* Note-Channel when drawing */
	for (int i = 0; i<= 15; i++) {
		char buf[64];
		sprintf(buf, "%d", i+1);
		draw_channel_selector.AddMenuElem (MenuElem (buf, sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_channel_selection_done), i)));
	}
	draw_channel_selector.AddMenuElem (MenuElem (_("Auto"), sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_channel_selection_done), DRAW_CHAN_AUTO)));
}

bool
EditingContext::drag_active () const
{
	return _drags->active();
}

bool
EditingContext::preview_video_drag_active () const
{
	return _drags->preview_video ();
}

Temporal::TimeDomain
EditingContext::time_domain () const
{
	if (_session) {
		return _session->config.get_default_time_domain();
	}

	/* Probably never reached */

	if (_snap_mode == SnapOff) {
		return Temporal::AudioTime;
	}

	switch (_grid_type) {
		case GridTypeNone:
			/* fallthrough */
		case GridTypeMinSec:
			/* fallthrough */
		case GridTypeCDFrame:
			/* fallthrough */
		case GridTypeTimecode:
			/* fallthrough */
			return Temporal::AudioTime;
		default:
			break;
	}

	return Temporal::BeatTime;
}

void
EditingContext::toggle_follow_playhead ()
{
	RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Editor"), X_("toggle-follow-playhead"));
	set_follow_playhead (tact->get_active());
}

/** @param yn true to follow playhead, otherwise false.
 *  @param catch_up true to reset the editor view to show the playhead (if yn == true), otherwise false.
 */
void
EditingContext::set_follow_playhead (bool yn, bool catch_up)
{
	if (_follow_playhead != yn) {
		if ((_follow_playhead = yn) == true && catch_up) {
			/* catch up */
			reset_x_origin_to_follow_playhead ();
		}
		instant_save ();
	}
}

void
EditingContext::begin_reversible_command (string name)
{
	if (_session) {
		before.push_back (&_selection_memento->get_state ());
		_session->begin_reversible_command (name);
	}
}

void
EditingContext::begin_reversible_command (GQuark q)
{
	if (_session) {
		before.push_back (&_selection_memento->get_state ());
		_session->begin_reversible_command (q);
	}
}

void
EditingContext::abort_reversible_command ()
{
	if (_session) {
		while(!before.empty()) {
			delete before.front();
			before.pop_front();
		}
		_session->abort_reversible_command ();
	}
}

void
EditingContext::commit_reversible_command ()
{
	if (_session) {
		if (before.size() == 1) {
			_session->add_command (new MementoCommand<SelectionMemento>(*(_selection_memento), before.front(), &_selection_memento->get_state ()));
			begin_selection_op_history ();
		}

		if (before.empty()) {
			PBD::stacktrace (std::cerr, 30);
			std::cerr << "Please call begin_reversible_command() before commit_reversible_command()." << std::endl;
		} else {
			before.pop_back();
		}

		_session->commit_reversible_command ();
	}
}

double
EditingContext::time_to_pixel (timepos_t const & pos) const
{
	return sample_to_pixel (pos.samples());
}

double
EditingContext::time_to_pixel_unrounded (timepos_t const & pos) const
{
	return sample_to_pixel_unrounded (pos.samples());
}

double
EditingContext::duration_to_pixels (timecnt_t const & dur) const
{
	return sample_to_pixel (dur.samples());
}

double
EditingContext::duration_to_pixels_unrounded (timecnt_t const & dur) const
{
	return sample_to_pixel_unrounded (dur.samples());
}

/** Snap a position to the grid, if appropriate, taking into account current
 *  grid settings and also the state of any snap modifier keys that may be pressed.
 *  @param start Position to snap.
 *  @param event Event to get current key modifier information from, or 0.
 */
void
EditingContext::snap_to_with_modifier (timepos_t& start, GdkEvent const * event, Temporal::RoundMode direction, SnapPref pref, bool ensure_snap) const
{
	if (!_session || !event) {
		return;
	}

	if (ArdourKeyboard::indicates_snap (event->button.state)) {
		if (_snap_mode == SnapOff) {
			snap_to_internal (start, direction, pref, ensure_snap);
		}

	} else {
		if (_snap_mode != SnapOff) {
			snap_to_internal (start, direction, pref);
		} else if (ArdourKeyboard::indicates_snap_delta (event->button.state)) {
			/* SnapOff, but we pressed the snap_delta modifier */
			snap_to_internal (start, direction, pref, ensure_snap);
		}
	}
}

void
EditingContext::snap_to (timepos_t& start, Temporal::RoundMode direction, SnapPref pref, bool ensure_snap) const
{
	if (!_session || (_snap_mode == SnapOff && !ensure_snap)) {
		return;
	}

	snap_to_internal (start, direction, pref, ensure_snap);
}

timepos_t
EditingContext::snap_to_bbt (timepos_t const & presnap, Temporal::RoundMode direction, SnapPref gpref) const
{
	return _snap_to_bbt (presnap, direction, gpref, _grid_type);
}

timepos_t
EditingContext::_snap_to_bbt (timepos_t const & presnap, Temporal::RoundMode direction, SnapPref gpref, GridType grid_type) const
{
	timepos_t ret(presnap);
	TempoMap::SharedPtr tmap (TempoMap::use());

	/* Snap to bar always uses bars, and ignores visual grid, so it may
	 * sometimes snap to bars that are not visually distinguishable.
	 *
	 * XXX this should probably work totally different: we should get the
	 * nearby grid and walk towards the next bar point.
	 */

	if (grid_type == GridTypeBar) {
		TempoMetric m (tmap->metric_at (presnap));
		BBT_Argument bbt (m.bbt_at (presnap));
		switch (direction) {
		case RoundDownAlways:
			bbt = BBT_Argument (bbt.reference(), bbt.round_down_to_bar ());
			break;
		case RoundUpAlways:
			bbt = BBT_Argument (bbt.reference(), bbt.round_up_to_bar ());
			break;
		case RoundNearest:
			bbt = BBT_Argument (bbt.reference(), m.round_to_bar (bbt));
			break;
		default:
			break;
		}
		return timepos_t (tmap->quarters_at (bbt));
	}

	if (gpref != SnapToGrid_Unscaled) { // use the visual grid lines which are limited by the zoom scale that the user selected

		/* Determine the most obvious divisor of a beat to use
		 * for the snap, based on the grid setting.
		 */

		int divisor;
		switch (_grid_type) {
			case GridTypeBeatDiv3:
			case GridTypeBeatDiv6:
			case GridTypeBeatDiv12:
			case GridTypeBeatDiv24:
				divisor = 3;
				break;
			case GridTypeBeatDiv5:
			case GridTypeBeatDiv10:
			case GridTypeBeatDiv20:
				divisor = 5;
				break;
			case GridTypeBeatDiv7:
			case GridTypeBeatDiv14:
			case GridTypeBeatDiv28:
				divisor = 7;
				break;
			case GridTypeBeat:
				divisor = 1;
				break;
			case GridTypeNone:
				return ret;
			default:
				divisor = 2;
				break;
		};

		/* bbt_ruler_scale reflects the level of detail we will show
		 * for the visual grid. Adjust the "natural" divisor to reflect
		 * this level of detail, and snap to that.
		 *
		 * So, for example, if the grid is Div3, we use 3 divisions per
		 * beat, but if the visual grid is using bbt_show_sixteenths (a
		 * fairly high level of detail), we will snap to (2 * 3)
		 * divisions per beat. Etc.
		 */

		BBTRulerScale scale = bbt_ruler_scale;
		switch (scale) {
			case bbt_show_many:
			case bbt_show_64:
			case bbt_show_16:
			case bbt_show_4:
			case bbt_show_1:
				/* Round to Bar */
				ret = timepos_t (tmap->quarters_at (presnap).round_to_subdivision (-1, direction));
				break;
			case bbt_show_quarters:
				/* Round to Beat */
				ret = timepos_t (tmap->quarters_at (presnap).round_to_subdivision (1, direction));
				break;
			case bbt_show_eighths:
				ret = timepos_t (tmap->quarters_at (presnap).round_to_subdivision (1 * divisor, direction));
				break;
			case bbt_show_sixteenths:
				ret = timepos_t (tmap->quarters_at (presnap).round_to_subdivision (2 * divisor, direction));
				break;
			case bbt_show_thirtyseconds:
				ret = timepos_t (tmap->quarters_at (presnap).round_to_subdivision (4 * divisor, direction));
				break;
			case bbt_show_sixtyfourths:
				ret = timepos_t (tmap->quarters_at (presnap).round_to_subdivision (8 * divisor, direction));
				break;
			case bbt_show_onetwentyeighths:
				ret = timepos_t (tmap->quarters_at (presnap).round_to_subdivision (16 * divisor, direction));
				break;
		}
	} else {
		/* Just use the grid as specified, without paying attention to
		 * zoom level
		 */

		ret = timepos_t (tmap->quarters_at (presnap).round_to_subdivision (get_grid_beat_divisions(_grid_type), direction));
	}

	return ret;
}

void
EditingContext::check_best_snap (timepos_t const & presnap, timepos_t &test, timepos_t &dist, timepos_t &best) const
{
	timepos_t diff = timepos_t (presnap.distance (test).abs ());
	if (diff < dist) {
		dist = diff;
		best = test;
	}

	test = timepos_t::max (test.time_domain()); // reset this so it doesn't get accidentally reused
}

timepos_t
EditingContext::canvas_event_time (GdkEvent const * event, double* pcx, double* pcy) const
{
	timepos_t pos (canvas_event_sample (event, pcx, pcy));

	if (time_domain() == Temporal::AudioTime) {
		return pos;
	}

	return timepos_t (pos.beats());
}

samplepos_t
EditingContext::canvas_event_sample (GdkEvent const * event, double* pcx, double* pcy) const
{
	double x;
	double y;

	/* event coordinates are already in canvas units */

	if (!gdk_event_get_coords (event, &x, &y)) {
		std::cerr << "!NO c COORDS for event type " << event->type << std::endl;
		return 0;
	}

	if (pcx) {
		*pcx = x;
	}

	if (pcy) {
		*pcy = y;
	}

	/* note that pixel_to_sample_from_event() never returns less than zero, so even if the pixel
	   position is negative (as can be the case with motion events in particular),
	   the sample location is always positive.
	*/

	return pixel_to_sample_from_event (x);
}

uint32_t
EditingContext::count_bars (Beats const & start, Beats const & end) const
{
	TempoMapPoints bar_grid;
	TempoMap::SharedPtr tmap (TempoMap::use());
	bar_grid.reserve (4096);
	superclock_t s (tmap->superclock_at (start));
	superclock_t e (tmap->superclock_at (end));
	tmap->get_grid (bar_grid, s, e, 1);
	return bar_grid.size();
}

void
EditingContext::compute_bbt_ruler_scale (samplepos_t lower, samplepos_t upper)
{
	if (_session == 0) {
		return;
	}

	Temporal::BBT_Time lower_beat, upper_beat; // the beats at each end of the ruler
	Temporal::TempoMap::SharedPtr tmap (Temporal::TempoMap::use());
	Beats floor_lower_beat = std::max (Beats(), tmap->quarters_at_sample (lower)).round_down_to_beat ();

	if (floor_lower_beat < Temporal::Beats()) {
		floor_lower_beat = Temporal::Beats();
	}

	const samplepos_t beat_before_lower_pos = tmap->sample_at (floor_lower_beat);
	const samplepos_t beat_after_upper_pos = tmap->sample_at ((std::max (Beats(), tmap->quarters_at_sample  (upper)).round_down_to_beat()) + Beats (1, 0));

	lower_beat = Temporal::TempoMap::use()->bbt_at (timepos_t (beat_before_lower_pos));
	upper_beat = Temporal::TempoMap::use()->bbt_at (timepos_t (beat_after_upper_pos));
	uint32_t beats = 0;

	bbt_bar_helper_on = false;
	bbt_bars = 0;

	bbt_ruler_scale =  bbt_show_many;

	const Beats ceil_upper_beat = std::max (Beats(), tmap->quarters_at_sample (upper)).round_up_to_beat() + Beats (1, 0);

	if (ceil_upper_beat == floor_lower_beat) {
		return;
	}

	bbt_bars = count_bars (floor_lower_beat, ceil_upper_beat);

	double ruler_line_granularity = UIConfiguration::instance().get_ruler_granularity ();  //in pixels
	ruler_line_granularity = visible_canvas_width() / (ruler_line_granularity*5);  //fudge factor '5' probably related to (4+1 beats)/measure, I think

	beats = (ceil_upper_beat - floor_lower_beat).get_beats();
	double beat_density = ((beats + 1) * ((double) (upper - lower) / (double) (1 + beat_after_upper_pos - beat_before_lower_pos))) / (float)ruler_line_granularity;

	/* Only show the bar helper if there aren't many bars on the screen */
	if ((bbt_bars < 2) || (beats < 5)) {
		bbt_bar_helper_on = true;
	}

	if (beat_density > 2048) {
		bbt_ruler_scale = bbt_show_many;
	} else if (beat_density > 1024) {
		bbt_ruler_scale = bbt_show_64;
	} else if (beat_density > 256) {
		bbt_ruler_scale = bbt_show_16;
	} else if (beat_density > 64) {
		bbt_ruler_scale = bbt_show_4;
	} else if (beat_density > 16) {
		bbt_ruler_scale = bbt_show_1;
	} else if (beat_density > 4) {
		bbt_ruler_scale =  bbt_show_quarters;
	} else  if (beat_density > 2) {
		bbt_ruler_scale =  bbt_show_eighths;
	} else  if (beat_density > 1) {
		bbt_ruler_scale =  bbt_show_sixteenths;
	} else  if (beat_density > 0.5) {
		bbt_ruler_scale =  bbt_show_thirtyseconds;
	} else  if (beat_density > 0.25) {
		bbt_ruler_scale =  bbt_show_sixtyfourths;
	} else {
		bbt_ruler_scale =  bbt_show_onetwentyeighths;
	}

	/* Now that we know how fine a grid (Ruler) is allowable on this screen, limit it to the coarseness selected by the user */
	/* note: GridType and RulerScale are not the same enums, so it's not a simple mathematical operation */
	int suggested_scale = (int) bbt_ruler_scale;
	int divs = get_grid_music_divisions(_grid_type, 0);
	if (_grid_type == GridTypeBar) {
		suggested_scale = std::min(suggested_scale, (int) bbt_show_1);
	} else if (_grid_type == GridTypeBeat) {
		suggested_scale = std::min(suggested_scale, (int) bbt_show_quarters);
	}  else if ( divs < 4 ) {
		suggested_scale = std::min(suggested_scale, (int) bbt_show_eighths);
	}  else if ( divs < 8 ) {
		suggested_scale = std::min(suggested_scale, (int) bbt_show_sixteenths);
	} else if ( divs < 16 ) {
		suggested_scale = std::min(suggested_scale, (int) bbt_show_thirtyseconds);
	} else if ( divs < 32 ) {
		suggested_scale = std::min(suggested_scale, (int) bbt_show_sixtyfourths);
	} else {
		suggested_scale = std::min(suggested_scale, (int) bbt_show_onetwentyeighths);
	}

	bbt_ruler_scale = (EditingContext::BBTRulerScale) suggested_scale;
}

Quantize*
EditingContext::get_quantize_op ()
{
	if (!quantize_dialog) {
		quantize_dialog = new QuantizeDialog (*this);
	}

	quantize_dialog->present ();
	int r = quantize_dialog->run ();
	quantize_dialog->hide ();


	if (r != Gtk::RESPONSE_OK) {
		return nullptr;
	}

	return new Quantize (quantize_dialog->snap_start(),
	                     quantize_dialog->snap_end(),
	                     quantize_dialog->start_grid_size(),
	                     quantize_dialog->end_grid_size(),
	                     quantize_dialog->strength(),
	                     quantize_dialog->swing(),
	                     quantize_dialog->threshold());
}

timecnt_t
EditingContext::relative_distance (timepos_t const & origin, timecnt_t const & duration, Temporal::TimeDomain domain)
{
	return Temporal::TempoMap::use()->convert_duration (duration, origin, domain);
}

/** Snap a time offset within our region using the current snap settings.
 *  @param x Time offset from this region's position.
 *  @param ensure_snap whether to ignore snap_mode (in the case of SnapOff) and magnetic snap.
 *  Used when inverting snap mode logic with key modifiers, or snap distance calculation.
 *  @return Snapped time offset from this region's position.
 */
timecnt_t
EditingContext::snap_relative_time_to_relative_time (timepos_t const & origin, timecnt_t const & x, bool ensure_snap) const
{
	/* x is relative to origin, convert it to global absolute time */
	timepos_t const session_pos = origin + x;

	/* try a snap in either direction */
	timepos_t snapped = session_pos;
	snap_to (snapped, Temporal::RoundNearest, SnapToAny_Visual, ensure_snap);

	/* if we went off the beginning of the region, snap forwards */
	if (snapped < origin) {
		snapped = session_pos;
		snap_to (snapped, Temporal::RoundUpAlways, SnapToAny_Visual, ensure_snap);
	}

	/* back to relative */
	return origin.distance (snapped);
}

std::shared_ptr<Temporal::TempoMap const>
EditingContext::start_local_tempo_map (std::shared_ptr<Temporal::TempoMap>)
{
	/* default is a no-op */
	return Temporal::TempoMap::use ();
}

bool
EditingContext::typed_event (ArdourCanvas::Item* item, GdkEvent *event, ItemType type)
{
	if (!session () || session()->loading () || session()->deletion_in_progress ()) {
		return false;
	}

	gint ret = FALSE;

	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		ret = button_press_handler (item, event, type);
		break;
	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, type);
		break;
	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event);
		break;

	case GDK_ENTER_NOTIFY:
		ret = enter_handler (item, event, type);
		break;

	case GDK_LEAVE_NOTIFY:
		ret = leave_handler (item, event, type);
		break;

	case GDK_KEY_PRESS:
		ret = key_press_handler (item, event, type);
		break;

	case GDK_KEY_RELEASE:
		ret = key_release_handler (item, event, type);
		break;

	default:
		break;
	}
	return ret;
}

void
EditingContext::popup_note_context_menu (ArdourCanvas::Item* item, GdkEvent* event)
{
	using namespace Menu_Helpers;

	NoteBase* note = reinterpret_cast<NoteBase*>(item->get_data("notebase"));
	if (!note) {
		return;
	}

	/* We need to get the selection here and pass it to the operations, since
	   popping up the menu will cause a region leave event which clears
	   entered_regionview. */

	MidiView&       mrv = note->region_view();
	const RegionSelection rs  = region_selection ();
	const uint32_t sel_size = mrv.selection_size ();

	MenuList& items = _note_context_menu.items();
	items.clear();

	if (sel_size > 0) {
		items.push_back (MenuElem(_("Delete"), sigc::mem_fun(mrv, &MidiView::delete_selection)));
	}

	items.push_back(MenuElem(_("Edit..."), sigc::bind(sigc::mem_fun(*this, &EditingContext::edit_notes), &mrv)));
	items.push_back(MenuElem(_("Transpose..."),  sigc::bind(sigc::mem_fun(*this, &EditingContext::transpose_regions), rs)));
	items.push_back(MenuElem(_("Legatize"), sigc::bind(sigc::mem_fun(*this, &EditingContext::legatize_regions), rs, false)));
	if (sel_size < 2) {
		items.back().set_sensitive (false);
	}
	items.push_back(MenuElem(_("Quantize..."), sigc::bind(sigc::mem_fun(*this, &EditingContext::quantize_regions), rs)));
	items.push_back(MenuElem(_("Remove Overlap"), sigc::bind(sigc::mem_fun(*this, &EditingContext::legatize_regions), rs, true)));
	if (sel_size < 2) {
		items.back().set_sensitive (false);
	}
	items.push_back(MenuElem(_("Transform..."), sigc::bind(sigc::mem_fun(*this, &EditingContext::transform_regions), rs)));

	_note_context_menu.popup (event->button.button, event->button.time);
}

XMLNode*
EditingContext::button_settings () const
{
	XMLNode* settings = ARDOUR_UI::instance()->editor_settings();
	XMLNode* node = find_named_node (*settings, X_("Buttons"));

	if (!node) {
		node = new XMLNode (X_("Buttons"));
	}

	return node;
}

std::vector<MidiView*>
EditingContext::filter_to_unique_midi_region_views (RegionSelection const & rs) const
{
	typedef std::pair<std::shared_ptr<MidiSource>,timepos_t> MapEntry;
	std::set<MapEntry> single_region_set;

	std::vector<MidiView*> views;

	/* build a list of regions that are unique with respect to their source
	 * and start position. Note: this is non-exhaustive... if someone has a
	 * non-forked copy of a MIDI region and then suitably modifies it, this
	 * will still put both regions into the list of things to be acted
	 * upon.
	 *
	 * Solution: user should not select both regions, or should fork one of them.
	 */

	for (auto const & rv : rs) {

		MidiView* mrv = dynamic_cast<MidiView*> (rv);

		if (!mrv) {
			continue;
		}

		MapEntry entry = make_pair (mrv->midi_region()->midi_source(), mrv->midi_region()->start());

		if (single_region_set.insert (entry).second) {
			views.push_back (mrv);
		}
	}

	return views;
}

void
EditingContext::quantize_region ()
{
	if (_session) {
		quantize_regions(region_selection ());
	}
}

void
EditingContext::quantize_regions (const RegionSelection& rs)
{
	if (rs.n_midi_regions() == 0) {
		return;
	}

	Quantize* quant = get_quantize_op ();

	if (!quant) {
		return;
	}

	if (!quant->empty()) {
		apply_midi_note_edit_op (*quant, rs);
	}

	delete quant;
}

void
EditingContext::legatize_region (bool shrink_only)
{
	if (_session) {
		legatize_regions(region_selection (), shrink_only);
	}
}

void
EditingContext::legatize_regions (const RegionSelection& rs, bool shrink_only)
{
	if (rs.n_midi_regions() == 0) {
		return;
	}

	Legatize legatize(shrink_only);
	apply_midi_note_edit_op (legatize, rs);
}

void
EditingContext::transform_region ()
{
	if (_session) {
		transform_regions(region_selection ());
	}
}

void
EditingContext::transform_regions (const RegionSelection& rs)
{
	if (rs.n_midi_regions() == 0) {
		return;
	}

	TransformDialog td;

	td.present();
	const int r = td.run();
	td.hide();

	if (r == Gtk::RESPONSE_OK) {
		Transform transform(td.get());
		apply_midi_note_edit_op(transform, rs);
	}
}

void
EditingContext::transpose_region ()
{
	if (_session) {
		transpose_regions(region_selection ());
	}
}

void
EditingContext::transpose_regions (const RegionSelection& rs)
{
	if (rs.n_midi_regions() == 0) {
		return;
	}

	TransposeDialog d;
	int const r = d.run ();

	if (r == RESPONSE_ACCEPT) {
		Transpose transpose(d.semitones ());
		apply_midi_note_edit_op (transpose, rs);
	}
}

void
EditingContext::edit_notes (MidiView* mrv)
{
	MidiView::Selection const & s = mrv->selection();

	if (s.empty ()) {
		return;
	}

	EditNoteDialog* d = new EditNoteDialog (mrv, s);
	d->show_all ();

	d->signal_response().connect (sigc::bind (sigc::mem_fun (*this, &EditingContext::note_edit_done), d));
}

void
EditingContext::note_edit_done (int r, EditNoteDialog* d)
{
	d->done (r);
	delete d;
}

PBD::Command*
EditingContext::apply_midi_note_edit_op_to_region (MidiOperator& op, MidiView& mrv)
{
	Evoral::Sequence<Temporal::Beats>::Notes selected;
	mrv.selection_as_notelist (selected, true);

	if (selected.empty()) {
		return 0;
	}

	std::cerr << "Apply op to " << selected.size() << std::endl;

	std::vector<Evoral::Sequence<Temporal::Beats>::Notes> v;
	v.push_back (selected);

	timepos_t pos = mrv.midi_region()->source_position();

	return op (mrv.midi_region()->model(), pos.beats(), v);
}

void
EditingContext::apply_midi_note_edit_op (MidiOperator& op, const RegionSelection& rs)
{
	if (rs.empty()) {
		return;
	}

	bool in_command = false;

	std::vector<MidiView*> views = filter_to_unique_midi_region_views (rs);

	for (auto & mv : views) {

		Command* cmd = apply_midi_note_edit_op_to_region (op, *mv);
		if (cmd) {
			if (!in_command) {
				begin_reversible_command (op.name ());
				in_command = true;
			}
			(*cmd)();
			_session->add_command (cmd);
			}
	}

	if (in_command) {
		commit_reversible_command ();
		_session->set_dirty ();
	}
}

EditingContext*
EditingContext::current_editing_context()
{
	if (!ec_stack.empty()) {
		return ec_stack.front ();
	}

	return nullptr;
}

void
EditingContext::push_editing_context (EditingContext* ec)
{
	ec_stack.push (ec);
}

void
EditingContext::pop_editing_context ()
{
	ec_stack.pop ();
}

double
EditingContext::horizontal_position () const
{
	return sample_to_pixel (_leftmost_sample);
}

void
EditingContext::set_horizontal_position (double p)
{
	p = std::max (0., p);

	horizontal_adjustment.set_value (p);

	_leftmost_sample = (samplepos_t) floor (p * samples_per_pixel);
}

Gdk::Cursor*
EditingContext::get_canvas_cursor () const
{
	/* The top of the cursor stack is always the currently visible cursor. */
	return _cursor_stack.back();
}

void
EditingContext::set_canvas_cursor (Gdk::Cursor* cursor)
{
	Glib::RefPtr<Gdk::Window> win = get_canvas_viewport()->get_window();

	if (win && !_cursors->is_invalid (cursor)) {
		/* glibmm 2.4 doesn't allow null cursor pointer because it uses
		   a Gdk::Cursor& as the argument to Gdk::Window::set_cursor().
		   But a null pointer just means "use parent window cursor",
		   and so should be allowed. Gtkmm 3.x has fixed this API.

		   For now, drop down and use C API
		*/
		gdk_window_set_cursor (win->gobj(), cursor ? cursor->gobj() : 0);
	}
}

size_t
EditingContext::push_canvas_cursor (Gdk::Cursor* cursor)
{
	if (!_cursors->is_invalid (cursor)) {
		_cursor_stack.push_back (cursor);
		set_canvas_cursor (cursor);
	}
	return _cursor_stack.size() - 1;
}

void
EditingContext::pop_canvas_cursor ()
{
	while (true) {
		if (_cursor_stack.size() <= 1) {
			PBD::error << "attempt to pop default cursor" << endmsg;
			return;
		}

		_cursor_stack.pop_back();
		if (_cursor_stack.back()) {
			/* Popped to an existing cursor, we're done.  Otherwise, the
			   context that created this cursor has been destroyed, so we need
			   to skip to the next down the stack. */
			set_canvas_cursor (_cursor_stack.back());
			return;
		}
	}
}

void
EditingContext::pack_draw_box ()
{
	/* Draw  - these MIDI tools are only visible when in Draw mode */
	draw_box.set_spacing (2);
	draw_box.set_border_width (2);
	draw_box.pack_start (*manage (new Label (_("Len:"))), false, false);
	draw_box.pack_start (draw_length_selector, false, false, 4);
	draw_box.pack_start (*manage (new Label (_("Ch:"))), false, false);
	draw_box.pack_start (draw_channel_selector, false, false, 4);
	draw_box.pack_start (*manage (new Label (_("Vel:"))), false, false);
	draw_box.pack_start (draw_velocity_selector, false, false, 4);

	draw_length_selector.set_name ("mouse mode button");
	draw_velocity_selector.set_name ("mouse mode button");
	draw_channel_selector.set_name ("mouse mode button");

	draw_velocity_selector.set_sizing_text (_("Auto"));
	draw_channel_selector.set_sizing_text (_("Auto"));

	draw_velocity_selector.disable_scrolling ();
	draw_velocity_selector.signal_scroll_event().connect (sigc::mem_fun(*this, &EditingContext::on_velocity_scroll_event), false);
}

void
EditingContext::pack_snap_box ()
{
	snap_box.pack_start (snap_mode_button, false, false);
	snap_box.pack_start (grid_type_selector, false, false);
}

Glib::RefPtr<Action>
EditingContext::get_mouse_mode_action (MouseMode m) const
{
	char const * group_name = _name.c_str(); /* use char* to force correct ::get_action variant */

	switch (m) {
	case MouseRange:
		return ActionManager::get_action (group_name, X_("set-mouse-mode-range"));
	case MouseObject:
		return ActionManager::get_action (group_name, X_("set-mouse-mode-object"));
	case MouseCut:
		return ActionManager::get_action (group_name, X_("set-mouse-mode-cut"));
	case MouseDraw:
		return ActionManager::get_action (group_name, X_("set-mouse-mode-draw"));
	case MouseTimeFX:
		return ActionManager::get_action (group_name, X_("set-mouse-mode-timefx"));
	case MouseGrid:
		return ActionManager::get_action (group_name, X_("set-mouse-mode-grid"));
	case MouseContent:
		return ActionManager::get_action (group_name, X_("set-mouse-mode-content"));
	}
	return Glib::RefPtr<Action>();
}

void
EditingContext::register_mouse_mode_actions ()
{
	RefPtr<Action> act;
	std::string group_name = _name;
	Glib::RefPtr<ActionGroup> mouse_mode_actions = ActionManager::create_action_group (bindings, group_name);
	RadioAction::Group mouse_mode_group;

	act = ActionManager::register_radio_action (mouse_mode_actions, mouse_mode_group, "set-mouse-mode-object", _("Grab (Object Tool)"), boost::bind (&EditingContext::mouse_mode_toggled, this, Editing::MouseObject));
	act = ActionManager::register_radio_action (mouse_mode_actions, mouse_mode_group, "set-mouse-mode-range", _("Range Tool"), boost::bind (&EditingContext::mouse_mode_toggled, this, Editing::MouseRange));
	act = ActionManager::register_radio_action (mouse_mode_actions, mouse_mode_group, "set-mouse-mode-draw", _("Note Drawing Tool"), boost::bind (&EditingContext::mouse_mode_toggled, this, Editing::MouseDraw));
	act = ActionManager::register_radio_action (mouse_mode_actions, mouse_mode_group, "set-mouse-mode-timefx", _("Time FX Tool"), boost::bind (&EditingContext::mouse_mode_toggled, this, Editing::MouseTimeFX));
	act = ActionManager::register_radio_action (mouse_mode_actions, mouse_mode_group, "set-mouse-mode-grid", _("Grid Tool"), boost::bind (&EditingContext::mouse_mode_toggled, this, Editing::MouseGrid));
	act = ActionManager::register_radio_action (mouse_mode_actions, mouse_mode_group, "set-mouse-mode-content", _("Internal Edit (Content Tool)"), boost::bind (&EditingContext::mouse_mode_toggled, this, Editing::MouseContent));
	act = ActionManager::register_radio_action (mouse_mode_actions, mouse_mode_group, "set-mouse-mode-cut", _("Cut Tool"), boost::bind (&EditingContext::mouse_mode_toggled, this, Editing::MouseCut));

	add_mouse_mode_actions (mouse_mode_actions);
}

void
EditingContext::bind_mouse_mode_buttons ()
{
	mouse_move_button.set_related_action (get_mouse_mode_action (Editing::MouseObject));
	mouse_move_button.set_icon (ArdourWidgets::ArdourIcon::ToolGrab);
	mouse_move_button.set_name ("mouse mode button");

	mouse_select_button.set_related_action (get_mouse_mode_action (Editing::MouseRange));
	mouse_select_button.set_icon (ArdourWidgets::ArdourIcon::ToolRange);
	mouse_select_button.set_name ("mouse mode button");

	mouse_draw_button.set_related_action (get_mouse_mode_action (Editing::MouseDraw));
	mouse_draw_button.set_icon (ArdourWidgets::ArdourIcon::ToolDraw);
	mouse_draw_button.set_name ("mouse mode button");

	mouse_timefx_button.set_related_action (get_mouse_mode_action (Editing::MouseTimeFX));
	mouse_timefx_button.set_icon (ArdourWidgets::ArdourIcon::ToolStretch);
	mouse_timefx_button.set_name ("mouse mode button");

	mouse_grid_button.set_related_action (get_mouse_mode_action (Editing::MouseGrid));
	mouse_grid_button.set_icon (ArdourWidgets::ArdourIcon::ToolGrid);
	mouse_grid_button.set_name ("mouse mode button");

	mouse_content_button.set_related_action (get_mouse_mode_action (Editing::MouseContent));
	mouse_content_button.set_icon (ArdourWidgets::ArdourIcon::ToolContent);
	mouse_content_button.set_name ("mouse mode button");

	mouse_cut_button.set_related_action (get_mouse_mode_action (Editing::MouseCut));
	mouse_cut_button.set_icon (ArdourWidgets::ArdourIcon::ToolCut);
	mouse_cut_button.set_name ("mouse mode button");

	set_tooltip (mouse_move_button, _("Grab Mode (select/move objects)"));
	set_tooltip (mouse_cut_button, _("Cut Mode (split regions)"));
	set_tooltip (mouse_select_button, _("Range Mode (select time ranges)"));
	set_tooltip (mouse_grid_button, _("Grid Mode (edit tempo-map, drag/drop music-time grid)"));
	set_tooltip (mouse_draw_button, _("Draw Mode (draw and edit gain/notes/automation)"));
	set_tooltip (mouse_timefx_button, _("Stretch Mode (time-stretch audio and midi regions, preserving pitch)"));
	set_tooltip (mouse_content_button, _("Internal Edit Mode (edit notes and automation points)"));
}

void
EditingContext::set_mouse_mode (MouseMode m, bool force)
{
	if (_drags->active ()) {
		return;
	}

	if (!force && m == mouse_mode) {
		return;
	}

	Glib::RefPtr<Action>       act  = get_mouse_mode_action(m);
	Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);

	/* go there and back to ensure that the toggled handler is called to set up mouse_mode */
	tact->set_active (false);
	tact->set_active (true);

	/* NOTE: this will result in a call to mouse_mode_toggled which does the heavy lifting */
}


bool
EditingContext::on_velocity_scroll_event (GdkEventScroll* ev)
{
	int v = PBD::atoi (draw_velocity_selector.get_text ());
	switch (ev->direction) {
		case GDK_SCROLL_DOWN:
			v = std::min (127, v + 1);
			break;
		case GDK_SCROLL_UP:
			v = std::max (1, v - 1);
			break;
		default:
			return false;
	}
	set_draw_velocity_to(v);
	return true;
}

void
EditingContext::set_common_editing_state (XMLNode const & node)
{
	double z;
	if (node.get_property ("zoom", z)) {
		/* older versions of ardour used floating point samples_per_pixel */
		reset_zoom (llrintf (z));
	} else {
		reset_zoom (samples_per_pixel);
	}

	GridType grid_type;
	if (!node.get_property ("grid-type", grid_type)) {
		grid_type = _grid_type;
	}
	grid_type_selection_done (grid_type);

	GridType draw_length;
	if (!node.get_property ("draw-length", draw_length)) {
		draw_length = _draw_length;
	}
	draw_length_selection_done (draw_length);

	int draw_vel;
	if (!node.get_property ("draw-velocity", draw_vel)) {
		draw_vel = _draw_velocity;
	}
	draw_velocity_selection_done (draw_vel);

	int draw_chan;
	if (!node.get_property ("draw-channel", draw_chan)) {
		draw_chan = DRAW_CHAN_AUTO;
	}
	draw_channel_selection_done (draw_chan);

	SnapMode sm;
	if (node.get_property ("snap-mode", sm)) {
		snap_mode_selection_done(sm);
		/* set text of Dropdown. in case _snap_mode == SnapOff (default)
		 * snap_mode_selection_done() will only mark an already active item as active
		 * which does not trigger set_text().
		 */
		set_snap_mode (sm);
	} else {
		set_snap_mode (_snap_mode);
	}

	node.get_property ("internal-grid-type", internal_grid_type);
	node.get_property ("internal-snap-mode", internal_snap_mode);
	node.get_property ("pre-internal-grid-type", pre_internal_grid_type);
	node.get_property ("pre-internal-snap-mode", pre_internal_snap_mode);

	std::string mm_str;
	if (node.get_property ("mouse-mode", mm_str)) {
		MouseMode m = str2mousemode(mm_str);
		set_mouse_mode (m, true);
	} else {
		set_mouse_mode (MouseObject, true);
	}

	samplepos_t lf_pos;
	if (node.get_property ("left-frame", lf_pos)) {
		if (lf_pos < 0) {
			lf_pos = 0;
		}
		reset_x_origin (lf_pos);
	}
}

void
EditingContext::get_common_editing_state (XMLNode& node) const
{
	node.set_property ("zoom", samples_per_pixel);
	node.set_property ("grid-type", _grid_type);
	node.set_property ("snap-mode", _snap_mode);
	node.set_property ("internal-grid-type", internal_grid_type);
	node.set_property ("internal-snap-mode", internal_snap_mode);
	node.set_property ("pre-internal-grid-type", pre_internal_grid_type);
	node.set_property ("pre-internal-snap-mode", pre_internal_snap_mode);
	node.set_property ("draw-length", _draw_length);
	node.set_property ("draw-velocity", _draw_velocity);
	node.set_property ("draw-channel", _draw_channel);
	node.set_property ("left-frame", _leftmost_sample);
}
