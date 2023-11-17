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

#include "pbd/error.h"

#include "ardour/rc_configuration.h"

#include "gtkmm2ext/bindings.h"

#include "actions.h"
#include "editing_context.h"
#include "editor_drag.h"
#include "midi_region_view.h"

#include "pbd/i18n.h"

using namespace Editing;
using namespace Glib;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace PBD;

sigc::signal<void> EditingContext::DropDownKeys;

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

EditingContext::EditingContext ()
	: rubberband_rect (0)
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
{
	grid_type_strings =  I18N (_grid_type_strings);
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
	_midi_actions = ActionManager::create_action_group (midi_bindings, X_("Notes"));

	/* two versions to allow same action for Delete and Backspace */

	ActionManager::register_action (_midi_actions, X_("clear-selection"), _("Clear Note Selection"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::clear_note_selection));
	ActionManager::register_action (_midi_actions, X_("invert-selection"), _("Invert Note Selection"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::invert_selection));
	ActionManager::register_action (_midi_actions, X_("extend-selection"), _("Extend Note Selection"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::extend_selection));
	ActionManager::register_action (_midi_actions, X_("duplicate-selection"), _("Duplicate Note Selection"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::duplicate_selection));

	/* Lengthen */

	ActionManager::register_action (_midi_actions, X_("move-starts-earlier-fine"), _("Move Note Start Earlier (fine)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::move_note_starts_earlier_fine));
	ActionManager::register_action (_midi_actions, X_("move-starts-earlier"), _("Move Note Start Earlier"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::move_note_starts_earlier));
	ActionManager::register_action (_midi_actions, X_("move-ends-later-fine"), _("Move Note Ends Later (fine)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::move_note_ends_later_fine));
	ActionManager::register_action (_midi_actions, X_("move-ends-later"), _("Move Note Ends Later"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::move_note_ends_later));

	/* Shorten */

	ActionManager::register_action (_midi_actions, X_("move-starts-later-fine"), _("Move Note Start Later (fine)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::move_note_starts_later_fine));
	ActionManager::register_action (_midi_actions, X_("move-starts-later"), _("Move Note Start Later"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::move_note_starts_later));
	ActionManager::register_action (_midi_actions, X_("move-ends-earlier-fine"), _("Move Note Ends Earlier (fine)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::move_note_ends_earlier_fine));
	ActionManager::register_action (_midi_actions, X_("move-ends-earlier"), _("Move Note Ends Earlier"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::move_note_ends_earlier));


	/* Alt versions allow bindings for both Tab and ISO_Left_Tab, if desired */

	ActionManager::register_action (_midi_actions, X_("select-next"), _("Select Next"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::select_next_note));
	ActionManager::register_action (_midi_actions, X_("alt-select-next"), _("Select Next (alternate)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::select_next_note));
	ActionManager::register_action (_midi_actions, X_("select-previous"), _("Select Previous"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::select_previous_note));
	ActionManager::register_action (_midi_actions, X_("alt-select-previous"), _("Select Previous (alternate)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::select_previous_note));
	ActionManager::register_action (_midi_actions, X_("add-select-next"), _("Add Next to Selection"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::add_select_next_note));
	ActionManager::register_action (_midi_actions, X_("alt-add-select-next"), _("Add Next to Selection (alternate)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::add_select_next_note));
	ActionManager::register_action (_midi_actions, X_("add-select-previous"), _("Add Previous to Selection"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::add_select_previous_note));
	ActionManager::register_action (_midi_actions, X_("alt-add-select-previous"), _("Add Previous to Selection (alternate)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::add_select_previous_note));

	ActionManager::register_action (_midi_actions, X_("increase-velocity"), _("Increase Velocity"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::increase_note_velocity));
	ActionManager::register_action (_midi_actions, X_("increase-velocity-fine"), _("Increase Velocity (fine)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::increase_note_velocity_fine));
	ActionManager::register_action (_midi_actions, X_("increase-velocity-smush"), _("Increase Velocity (allow mush)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::increase_note_velocity_smush));
	ActionManager::register_action (_midi_actions, X_("increase-velocity-together"), _("Increase Velocity (non-relative)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::increase_note_velocity_together));
	ActionManager::register_action (_midi_actions, X_("increase-velocity-fine-smush"), _("Increase Velocity (fine, allow mush)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::increase_note_velocity_fine_smush));
	ActionManager::register_action (_midi_actions, X_("increase-velocity-fine-together"), _("Increase Velocity (fine, non-relative)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::increase_note_velocity_fine_together));
	ActionManager::register_action (_midi_actions, X_("increase-velocity-smush-together"), _("Increase Velocity (maintain ratios, allow mush)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::increase_note_velocity_smush_together));
	ActionManager::register_action (_midi_actions, X_("increase-velocity-fine-smush-together"), _("Increase Velocity (fine, allow mush, non-relative)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::increase_note_velocity_fine_smush_together));

	ActionManager::register_action (_midi_actions, X_("decrease-velocity"), _("Decrease Velocity"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::decrease_note_velocity));
	ActionManager::register_action (_midi_actions, X_("decrease-velocity-fine"), _("Decrease Velocity (fine)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::decrease_note_velocity_fine));
	ActionManager::register_action (_midi_actions, X_("decrease-velocity-smush"), _("Decrease Velocity (allow mush)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::decrease_note_velocity_smush));
	ActionManager::register_action (_midi_actions, X_("decrease-velocity-together"), _("Decrease Velocity (non-relative)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::decrease_note_velocity_together));
	ActionManager::register_action (_midi_actions, X_("decrease-velocity-fine-smush"), _("Decrease Velocity (fine, allow mush)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::decrease_note_velocity_fine_smush));
	ActionManager::register_action (_midi_actions, X_("decrease-velocity-fine-together"), _("Decrease Velocity (fine, non-relative)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::decrease_note_velocity_fine_together));
	ActionManager::register_action (_midi_actions, X_("decrease-velocity-smush-together"), _("Decrease Velocity (maintain ratios, allow mush)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::decrease_note_velocity_smush_together));
	ActionManager::register_action (_midi_actions, X_("decrease-velocity-fine-smush-together"), _("Decrease Velocity (fine, allow mush, non-relative)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::decrease_note_velocity_fine_smush_together));

	ActionManager::register_action (_midi_actions, X_("transpose-up-octave"), _("Transpose Up (octave)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::transpose_up_octave));
	ActionManager::register_action (_midi_actions, X_("transpose-up-octave-smush"), _("Transpose Up (octave, allow mush)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::transpose_up_octave_smush));
	ActionManager::register_action (_midi_actions, X_("transpose-up-semitone"), _("Transpose Up (semitone)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::transpose_up_tone));
	ActionManager::register_action (_midi_actions, X_("transpose-up-semitone-smush"), _("Transpose Up (semitone, allow mush)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::transpose_up_octave_smush));

	ActionManager::register_action (_midi_actions, X_("transpose-down-octave"), _("Transpose Down (octave)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::transpose_down_octave));
	ActionManager::register_action (_midi_actions, X_("transpose-down-octave-smush"), _("Transpose Down (octave, allow mush)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::transpose_down_octave_smush));
	ActionManager::register_action (_midi_actions, X_("transpose-down-semitone"), _("Transpose Down (semitone)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::transpose_down_tone));
	ActionManager::register_action (_midi_actions, X_("transpose-down-semitone-smush"), _("Transpose Down (semitone, allow mush)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::transpose_down_octave_smush));

	ActionManager::register_action (_midi_actions, X_("nudge-later"), _("Nudge Notes Later (grid)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::nudge_notes_later));
	ActionManager::register_action (_midi_actions, X_("nudge-later-fine"), _("Nudge Notes Later (1/4 grid)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::nudge_notes_later_fine));
	ActionManager::register_action (_midi_actions, X_("nudge-earlier"), _("Nudge Notes Earlier (grid)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::nudge_notes_earlier));
	ActionManager::register_action (_midi_actions, X_("nudge-earlier-fine"), _("Nudge Notes Earlier (1/4 grid)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::nudge_notes_earlier_fine));

	ActionManager::register_action (_midi_actions, X_("edit-channels"), _("Edit Note Channels"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::channel_edit));
	ActionManager::register_action (_midi_actions, X_("edit-velocities"), _("Edit Note Velocities"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::velocity_edit));

	ActionManager::register_action (_midi_actions, X_("quantize-selected-notes"), _("Quantize Selected Notes"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action ), &MidiRegionView::quantize_selected_notes));

	Glib::RefPtr<ActionGroup> length_actions = ActionManager::create_action_group (midi_bindings, X_("DrawLength"));
	RadioAction::Group draw_length_group;

	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-thirtyseconds"),  grid_type_strings[(int)GridTypeBeatDiv32].c_str(), (sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeatDiv32)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-twentyeighths"),  grid_type_strings[(int)GridTypeBeatDiv28].c_str(), (sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeatDiv28)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-twentyfourths"),  grid_type_strings[(int)GridTypeBeatDiv24].c_str(), (sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeatDiv24)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-twentieths"),     grid_type_strings[(int)GridTypeBeatDiv20].c_str(), (sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeatDiv20)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-asixteenthbeat"), grid_type_strings[(int)GridTypeBeatDiv16].c_str(), (sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeatDiv16)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-fourteenths"),    grid_type_strings[(int)GridTypeBeatDiv14].c_str(), (sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeatDiv14)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-twelfths"),       grid_type_strings[(int)GridTypeBeatDiv12].c_str(), (sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeatDiv12)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-tenths"),         grid_type_strings[(int)GridTypeBeatDiv10].c_str(), (sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeatDiv10)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-eighths"),        grid_type_strings[(int)GridTypeBeatDiv8].c_str(),  (sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeatDiv8)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-sevenths"),       grid_type_strings[(int)GridTypeBeatDiv7].c_str(),  (sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeatDiv7)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-sixths"),         grid_type_strings[(int)GridTypeBeatDiv6].c_str(),  (sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeatDiv6)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-fifths"),         grid_type_strings[(int)GridTypeBeatDiv5].c_str(),  (sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeatDiv5)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-quarters"),       grid_type_strings[(int)GridTypeBeatDiv4].c_str(),  (sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeatDiv4)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-thirds"),         grid_type_strings[(int)GridTypeBeatDiv3].c_str(),  (sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeatDiv3)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-halves"),         grid_type_strings[(int)GridTypeBeatDiv2].c_str(),  (sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeatDiv2)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-beat"),           grid_type_strings[(int)GridTypeBeat].c_str(),      (sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeat)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-bar"),            grid_type_strings[(int)GridTypeBar].c_str(),       (sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_length_chosen), Editing::GridTypeBar)));
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-auto"),           _("Auto"),                                         (sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_length_chosen), DRAW_LEN_AUTO)));

	Glib::RefPtr<ActionGroup> velocity_actions = ActionManager::create_action_group (midi_bindings, _("Draw Velocity"));
	RadioAction::Group draw_velocity_group;
	ActionManager::register_radio_action (velocity_actions, draw_velocity_group, X_("draw-velocity-auto"),  _("Auto"), (sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_velocity_chosen), DRAW_VEL_AUTO)));
	for (int i = 1; i <= 127; i++) {
		char buf[64];
		sprintf(buf, X_("draw-velocity-%d"), i);
		char vel[64];
		sprintf(vel, _("Velocity %d"), i);
		ActionManager::register_radio_action (velocity_actions, draw_velocity_group, buf, vel, (sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_velocity_chosen), i)));
	}

	Glib::RefPtr<ActionGroup> channel_actions = ActionManager::create_action_group (midi_bindings, _("Draw Channel"));
	RadioAction::Group draw_channel_group;
	ActionManager::register_radio_action (channel_actions, draw_channel_group, X_("draw-channel-auto"),  _("Auto"), (sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_channel_chosen), DRAW_CHAN_AUTO)));
	for (int i = 0; i <= 15; i++) {
		char buf[64];
		sprintf(buf, X_("draw-channel-%d"), i+1);
		char ch[64];
		sprintf(ch, X_("Channel %d"), i+1);
		ActionManager::register_radio_action (channel_actions, draw_channel_group, buf, ch, (sigc::bind (sigc::mem_fun(*this, &EditingContext::draw_channel_chosen), i)));
	}

	ActionManager::set_sensitive (_midi_actions, false);
}

void
EditingContext::midi_action (void (MidiRegionView::*method)())
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
		Glib::RefPtr<RadioAction> ract = ActionManager::get_radio_action (X_("MouseMode"), X_("set-mouse-mode-object"));
		ract->set_active (true);
	}

	ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-grid"))->set_sensitive (grid_is_musical);

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
