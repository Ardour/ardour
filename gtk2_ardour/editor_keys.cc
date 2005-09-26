/*
    Copyright (C) 2000 Paul Davis 

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

    $Id$
*/

#include <cstdlib>
#include <cmath>
#include <string>

#include <pbd/error.h>

#include <ardour/session.h>
#include <ardour/region.h>

#include "ardour_ui.h"
#include "editor.h"
#include "time_axis_view.h"
#include "regionview.h"
#include "selection.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace sigc;

void
Editor::install_keybindings ()
{
	/* add named actions for the editor */

	add_action ("toggle-xfades-active", mem_fun(*this, &Editor::toggle_xfades_active));

	add_action ("playhead-to-next-region-start", bind (mem_fun(*this, &Editor::cursor_to_next_region_point), playhead_cursor, RegionPoint (Start)));
	add_action ("playhead-to-next-region-end", bind (mem_fun(*this, &Editor::cursor_to_next_region_point), playhead_cursor, RegionPoint (End)));
	add_action ("playhead-to-next-region-sync", bind (mem_fun(*this, &Editor::cursor_to_next_region_point), playhead_cursor, RegionPoint (SyncPoint)));

	add_action ("playhead-to-previous-region-start", bind (mem_fun(*this, &Editor::cursor_to_previous_region_point), playhead_cursor, RegionPoint (Start)));
	add_action ("playhead-to-previous-region-end", bind (mem_fun(*this, &Editor::cursor_to_previous_region_point), playhead_cursor, RegionPoint (End)));
	add_action ("playhead-to-previous-region-sync", bind (mem_fun(*this, &Editor::cursor_to_previous_region_point), playhead_cursor, RegionPoint (SyncPoint)));

	add_action ("edit-cursor-to-next-region-start", bind (mem_fun(*this, &Editor::cursor_to_next_region_point), edit_cursor, RegionPoint (Start)));
	add_action ("edit-cursor-to-next-region-end", bind (mem_fun(*this, &Editor::cursor_to_next_region_point), edit_cursor, RegionPoint (End)));
	add_action ("edit-cursor-to-next-region-sync", bind (mem_fun(*this, &Editor::cursor_to_next_region_point), edit_cursor, RegionPoint (SyncPoint)));

	add_action ("edit-cursor-to-previous-region-start", bind (mem_fun(*this, &Editor::cursor_to_previous_region_point), edit_cursor, RegionPoint (Start)));
	add_action ("edit-cursor-to-previous-region-end", bind (mem_fun(*this, &Editor::cursor_to_previous_region_point), edit_cursor, RegionPoint (End)));
	add_action ("edit-cursor-to-previous-region-sync", bind (mem_fun(*this, &Editor::cursor_to_previous_region_point), edit_cursor, RegionPoint (SyncPoint)));

	add_action ("playhead-to-range-start", bind (mem_fun(*this, &Editor::cursor_to_selection_start), playhead_cursor));
	add_action ("playhead-to-range-end", bind (mem_fun(*this, &Editor::cursor_to_selection_end), playhead_cursor));

	add_action ("edit-cursor-to-range-start", bind (mem_fun(*this, &Editor::cursor_to_selection_start), edit_cursor));
	add_action ("edit-cursor-to-range-end", bind (mem_fun(*this, &Editor::cursor_to_selection_end), edit_cursor));

	add_action ("jump-forward-to-mark", mem_fun(*this, &Editor::jump_forward_to_mark));
	add_action ("jump-backward-to-mark", mem_fun(*this, &Editor::jump_backward_to_mark));
	add_action ("add-location-from-playhead", mem_fun(*this, &Editor::add_location_from_playhead_cursor));

	add_action ("nudge-forward", bind (mem_fun(*this, &Editor::nudge_forward), false));
	add_action ("nudge-next-forward", bind (mem_fun(*this, &Editor::nudge_forward), true));
	add_action ("nudge-backward", bind (mem_fun(*this, &Editor::nudge_backward), false));
	add_action ("nudge-next-backward", bind (mem_fun(*this, &Editor::nudge_backward), true));

	add_action ("toggle-playback", bind (mem_fun(*this, &Editor::toggle_playback), false));
	add_action ("toggle-playback-forget-capture", bind (mem_fun(*this, &Editor::toggle_playback), true));

	add_action ("toggle-loop-playback", mem_fun(*this, &Editor::toggle_loop_playback));
	
	add_action ("temporal-zoom-out", bind (mem_fun(*this, &Editor::temporal_zoom_step), true));
	add_action ("temporal-zoom-in", bind (mem_fun(*this, &Editor::temporal_zoom_step), false));
	add_action ("zoom-to-session", mem_fun(*this, &Editor::temporal_zoom_session));

	add_action ("scroll-tracks-up", mem_fun(*this, &Editor::scroll_tracks_up));
	add_action ("scroll-tracks-down", mem_fun(*this, &Editor::scroll_tracks_down));
	add_action ("step-tracks-up", mem_fun(*this, &Editor::scroll_tracks_up_line));
	add_action ("step-tracks-down", mem_fun(*this, &Editor::scroll_tracks_down_line));

	add_action ("scroll-backward", bind (mem_fun(*this, &Editor::scroll_backward), 0.8f));
	add_action ("scroll-forward", bind (mem_fun(*this, &Editor::scroll_forward), 0.8f));
	add_action ("goto", mem_fun(*this, &Editor::goto_frame));
	add_action ("center-playhead", mem_fun(*this, &Editor::center_playhead));
	add_action ("center-edit_cursor", mem_fun(*this, &Editor::center_edit_cursor));
	add_action ("playhead-forward", mem_fun(*this, &Editor::playhead_forward));
	add_action ("playhead-backward", mem_fun(*this, &Editor::playhead_backward));
	add_action ("playhead-to-edit", bind (mem_fun(*this, &Editor::cursor_align), true));
	add_action ("edit-to-playhead", bind (mem_fun(*this, &Editor::cursor_align), false));

	add_action ("align-regions-start", bind (mem_fun(*this, &Editor::align), ARDOUR::Start));
	add_action ("align-regions-start-relative", bind (mem_fun(*this, &Editor::align_relative), ARDOUR::Start));
	add_action ("align-regions-end", bind (mem_fun(*this, &Editor::align), ARDOUR::End));
	add_action ("align-regions-end-relative", bind (mem_fun(*this, &Editor::align_relative), ARDOUR::End));
	add_action ("align-regions-sync", bind (mem_fun(*this, &Editor::align), ARDOUR::SyncPoint));
	add_action ("align-regions-sync-relative", bind (mem_fun(*this, &Editor::align_relative), ARDOUR::SyncPoint));
	
	add_action ("set-playhead", mem_fun(*this, &Editor::kbd_set_playhead_cursor));
	add_action ("set-edit-cursor", mem_fun(*this, &Editor::kbd_set_edit_cursor));

	add_action ("set-mouse-mode-object", bind (mem_fun(*this, &Editor::set_mouse_mode), Editing::MouseObject, false));
	add_action ("set-mouse-mode-range", bind (mem_fun(*this, &Editor::set_mouse_mode), Editing::MouseRange, false));
	add_action ("set-mouse-mode-gain", bind (mem_fun(*this, &Editor::set_mouse_mode), Editing::MouseGain, false));
	add_action ("set-mouse-mode-zoom", bind (mem_fun(*this, &Editor::set_mouse_mode), Editing::MouseZoom, false));
	add_action ("set-mouse-mode-timefx", bind (mem_fun(*this, &Editor::set_mouse_mode), Editing::MouseTimeFX, false));

	add_action ("set-undo", bind (mem_fun(*this, &Editor::undo), 1U));
	add_action ("set-redo", bind (mem_fun(*this, &Editor::redo), 1U));

	add_action ("export-session", mem_fun(*this, &Editor::export_session));
	add_action ("export-range", mem_fun(*this, &Editor::export_selection));

	add_action ("editor-cut", mem_fun(*this, &Editor::cut));
	add_action ("editor-copy", mem_fun(*this, &Editor::copy));
	add_action ("editor-paste", mem_fun(*this, &Editor::keyboard_paste));
	add_action ("duplicate-region", mem_fun(*this, &Editor::keyboard_duplicate_region));
	add_action ("duplicate-range", mem_fun(*this, &Editor::keyboard_duplicate_selection));
	add_action ("insert-region", mem_fun(*this, &Editor::keyboard_insert_region_list_selection));
	add_action ("reverse-region", mem_fun(*this, &Editor::reverse_region));
	add_action ("normalize-region", mem_fun(*this, &Editor::normalize_region));
	add_action ("editor-crop", mem_fun(*this, &Editor::crop_region_to_selection));
	add_action ("insert-chunk", bind (mem_fun(*this, &Editor::paste_named_selection), 1.0f));

	add_action ("split-at-edit-cursor", mem_fun(*this, &Editor::split_region));
	add_action ("split-at-mouse", mem_fun(*this, &Editor::kbd_split));

	add_action ("brush-at-mouse", mem_fun(*this, &Editor::kbd_brush));
	add_action ("audition-at-mouse", mem_fun(*this, &Editor::kbd_audition));

	add_action ("start-range", mem_fun(*this, &Editor::keyboard_selection_begin));
	add_action ("finish-range", bind (mem_fun(*this, &Editor::keyboard_selection_finish), false));
	add_action ("finish-add-range", bind (mem_fun(*this, &Editor::keyboard_selection_finish), true));

	add_action ("extend-range-to-end-of-region", bind (mem_fun(*this, &Editor::extend_selection_to_end_of_region), false));
	add_action ("extend-range-to-start-of-region", bind (mem_fun(*this, &Editor::extend_selection_to_start_of_region), false));

	add_action ("zoom-focus-left", bind (mem_fun(*this, &Editor::set_zoom_focus), Editing::ZoomFocusLeft));
	add_action ("zoom-focus-right", bind (mem_fun(*this, &Editor::set_zoom_focus), Editing::ZoomFocusRight));
	add_action ("zoom-focus-center", bind (mem_fun(*this, &Editor::set_zoom_focus), Editing::ZoomFocusCenter));
	add_action ("zoom-focus-playhead", bind (mem_fun(*this, &Editor::set_zoom_focus), Editing::ZoomFocusPlayhead));
	add_action ("zoom-focus-edit", bind (mem_fun(*this, &Editor::set_zoom_focus), Editing::ZoomFocusEdit));

	add_action ("toggle-follow-playhead", (mem_fun(*this, &Editor::toggle_follow_playhead)));
	add_action ("remove-last-capture", (mem_fun(*this, &Editor::remove_last_capture)));

	add_action ("snap-to-frame", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToFrame)));
	add_action ("snap-to-cd-frame", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToCDFrame)));
	add_action ("snap-to-smpte-frame", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToSMPTEFrame)));
	add_action ("snap-to-smpte-seconds", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToSMPTESeconds)));
	add_action ("snap-to-smpte-minutes", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToSMPTEMinutes)));
	add_action ("snap-to-seconds", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToSeconds)));
	add_action ("snap-to-minutes", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToMinutes)));
	add_action ("snap-to-thirtyseconds", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToAThirtysecondBeat)));
	add_action ("snap-to-asixteenthbeat", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToASixteenthBeat)));
	add_action ("snap-to-eighths", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToAEighthBeat)));
	add_action ("snap-to-quarters", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToAQuarterBeat)));
	add_action ("snap-to-thirds", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToAThirdBeat)));
	add_action ("snap-to-beat", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToBeat)));
	add_action ("snap-to-bar", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToBar)));
	add_action ("snap-to-mark", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToMark)));
	add_action ("snap-to-edit-cursor", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToEditCursor)));
	add_action ("snap-to-region-start", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToRegionStart)));
	add_action ("snap-to-region-end", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToRegionEnd)));
	add_action ("snap-to-region-sync", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToRegionSync)));
	add_action ("snap-to-region-boundary", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToRegionBoundary)));
}

void
Editor::keyboard_selection_finish (bool add)
{
	if (session && have_pending_keyboard_selection) {
		begin_reversible_command (_("keyboard selection"));
		if (!add) {
			selection->set (0, pending_keyboard_selection_start, session->audible_frame());
		} else {
			selection->add (pending_keyboard_selection_start, session->audible_frame());
		}
		commit_reversible_command ();
		have_pending_keyboard_selection = false;
	}
}

void
Editor::keyboard_selection_begin ()
{
	if (session) {
		pending_keyboard_selection_start = session->audible_frame();
		have_pending_keyboard_selection = true;
	}
}

void
Editor::keyboard_duplicate_region ()
{
	if (selection->audio_regions.empty()) {
		return;
	}

	float prefix;
	bool was_floating;

	if (get_prefix (prefix, was_floating) == 0) {
		duplicate_some_regions (selection->audio_regions, prefix);
	} else {
		duplicate_some_regions (selection->audio_regions, 1);
	}
}

void
Editor::keyboard_duplicate_selection ()
{
	float prefix;
	bool was_floating;

	if (get_prefix (prefix, was_floating) == 0) {
		duplicate_selection (prefix);
	} else {
		duplicate_selection (1);
	}
}

void
Editor::keyboard_paste ()
{
	float prefix;
	bool was_floating;

	if (get_prefix (prefix, was_floating) == 0) {
		paste (prefix);
	} else {
		paste (1);
	}
}

void
Editor::keyboard_insert_region_list_selection ()
{
	float prefix;
	bool was_floating;

	if (get_prefix (prefix, was_floating) == 0) {
		insert_region_list_selection (prefix);
	} else {
		insert_region_list_selection (1);
	}
}

int
Editor::get_prefix (float& val, bool& was_floating)
{
	return Keyboard::the_keyboard().get_prefix (val, was_floating);
}

