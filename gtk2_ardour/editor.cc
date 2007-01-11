/*
    Copyright (C) 2000-2006 Paul Davis 

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

#include <unistd.h>
#include <cstdlib>
#include <cmath>
#include <string>
#include <algorithm>

#include <sigc++/bind.h>

#include <pbd/convert.h>
#include <pbd/error.h>
#include <pbd/memento_command.h>

#include <gtkmm/image.h>
#include <gdkmm/color.h>
#include <gdkmm/bitmap.h>

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/tearoff.h>
#include <gtkmm2ext/utils.h>

#include <ardour/audio_track.h>
#include <ardour/audio_diskstream.h>
#include <ardour/plugin_manager.h>
#include <ardour/location.h>
#include <ardour/audioplaylist.h>
#include <ardour/audioregion.h>
#include <ardour/region.h>
#include <ardour/session_route.h>
#include <ardour/tempo.h>
#include <ardour/utils.h>

#include <control_protocol/control_protocol.h>

#include "ardour_ui.h"
#include "editor.h"
#include "grouped_buttons.h"
#include "keyboard.h"
#include "marker.h"
#include "playlist_selector.h"
#include "audio_region_view.h"
#include "rgb_macros.h"
#include "selection.h"
#include "audio_streamview.h"
#include "time_axis_view.h"
#include "audio_time_axis.h"
#include "utils.h"
#include "crossfade_view.h"
#include "editing.h"
#include "public_editor.h"
#include "crossfade_edit.h"
#include "audio_time_axis.h"
#include "canvas_impl.h"
#include "actions.h"
#include "gui_thread.h"

#ifdef FFT_ANALYSIS
#include "analysis_window.h"
#endif

#include "i18n.h"

/* <CMT Additions> */
#include "imageframe_socket_handler.h"
/* </CMT Additions> */

using namespace std;
using namespace sigc;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Glib;
using namespace Gtkmm2ext;
using namespace Editing;

using PBD::internationalize;
using PBD::atoi;

const double Editor::timebar_height = 15.0;

#include "editor_xpms"

static const gchar *_snap_type_strings[] = {
	N_("None"),
	N_("CD Frames"),
	N_("SMPTE Frames"),
	N_("SMPTE Seconds"),
	N_("SMPTE Minutes"),
	N_("Seconds"),
	N_("Minutes"),
	N_("Beats/32"),
	N_("Beats/16"),
	N_("Beats/8"),
	N_("Beats/4"),
	N_("Beats/3"),
	N_("Beats"),
	N_("Bars"),
	N_("Marks"),
	N_("Edit Cursor"),
	N_("Region starts"),
	N_("Region ends"),
	N_("Region syncs"),
	N_("Region bounds"),
	0
};

static const gchar *_snap_mode_strings[] = {
	N_("Normal"),
	N_("Magnetic"),
	0
};

static const gchar *_zoom_focus_strings[] = {
	N_("Left"),
	N_("Right"),
	N_("Center"),
	N_("Playhead"),
 	N_("Edit Cursor"),
	0
};

/* Soundfile  drag-n-drop */

Gdk::Cursor* Editor::cross_hair_cursor = 0;
Gdk::Cursor* Editor::selector_cursor = 0;
Gdk::Cursor* Editor::trimmer_cursor = 0;
Gdk::Cursor* Editor::grabber_cursor = 0;
Gdk::Cursor* Editor::zoom_cursor = 0;
Gdk::Cursor* Editor::time_fx_cursor = 0;
Gdk::Cursor* Editor::fader_cursor = 0;
Gdk::Cursor* Editor::speaker_cursor = 0;
Gdk::Cursor* Editor::wait_cursor = 0;
Gdk::Cursor* Editor::timebar_cursor = 0;

void
show_me_the_size (Requisition* r, const char* what)
{
	cerr << "size of " << what << " = " << r->width << " x " << r->height << endl;
}

void 
check_adjustment (Gtk::Adjustment* adj)
{
	cerr << "CHANGE adj  = " 
	     << adj->get_lower () <<  ' '
	     << adj->get_upper () <<  ' '
	     << adj->get_value () <<  ' '
	     << adj->get_step_increment () <<  ' '
	     << adj->get_page_increment () <<  ' '
	     << adj->get_page_size () <<  ' '
	     << endl;

}

Editor::Editor (AudioEngine& eng) 
	: engine (eng),

	  /* time display buttons */

	  minsec_label (_("Mins:Secs")),
	  bbt_label (_("Bars:Beats")),
	  smpte_label (_("Timecode")),
	  frame_label (_("Frames")),
	  tempo_label (_("Tempo")),
	  meter_label (_("Meter")),
	  mark_label (_("Location Markers")),
	  range_mark_label (_("Range Markers")),
	  transport_mark_label (_("Loop/Punch Ranges")),

	  edit_packer (3, 3, false),

	  /* the values here don't matter: layout widgets
	     reset them as needed.
	  */

	  vertical_adjustment (0.0, 0.0, 10.0, 400.0),
	  horizontal_adjustment (0.0, 0.0, 20.0, 1200.0),

	  /* tool bar related */

	  edit_cursor_clock (X_("editcursor"), false, X_("EditCursorClock"), true),
	  zoom_range_clock (X_("zoomrange"), false, X_("ZoomRangeClock"), true, true),
	  
	  toolbar_selection_clock_table (2,3),
	  
	  automation_mode_button (_("mode")),
	  global_automation_button (_("automation")),

	  /* <CMT Additions> */
	  image_socket_listener(0),
	  /* </CMT Additions> */

	  /* nudge */

	  nudge_clock (X_("nudge"), false, X_("NudgeClock"), true, true)

{
	constructed = false;

	/* we are a singleton */

	PublicEditor::_instance = this;

	session = 0;

	selection = new Selection;
	cut_buffer = new Selection;

	selection->TimeChanged.connect (mem_fun(*this, &Editor::time_selection_changed));
	selection->TracksChanged.connect (mem_fun(*this, &Editor::track_selection_changed));
	selection->RegionsChanged.connect (mem_fun(*this, &Editor::region_selection_changed));
	selection->PointsChanged.connect (mem_fun(*this, &Editor::point_selection_changed));

	clicked_regionview = 0;
	clicked_axisview = 0;
	clicked_routeview = 0;
	clicked_crossfadeview = 0;
	clicked_control_point = 0;
	latest_regionview = 0;
	last_update_frame = 0;
	drag_info.item = 0;
	current_mixer_strip = 0;
	current_bbt_points = 0;

	snap_type_strings = I18N (_snap_type_strings);
	snap_mode_strings = I18N (_snap_mode_strings);
	zoom_focus_strings = I18N(_zoom_focus_strings);

	snap_type = SnapToFrame;
	set_snap_to (snap_type);
	snap_mode = SnapNormal;
	set_snap_mode (snap_mode);
	snap_threshold = 5.0;
	bbt_beat_subdivision = 4;
	canvas_width = 0;
	canvas_height = 0;
	autoscroll_active = false;
	autoscroll_timeout_tag = -1;
	interthread_progress_window = 0;

#ifdef FFT_ANALYSIS
	analysis_window = 0;
#endif

	current_interthread_info = 0;
	_show_measures = true;
	_show_waveforms = true;
	_show_waveforms_recording = true;
	first_action_message = 0;
	export_dialog = 0;
	show_gain_after_trim = false;
	ignore_route_list_reorder = false;
	no_route_list_redisplay = false;
	verbose_cursor_on = true;
	route_removal = false;
	track_spacing = 0;
	show_automatic_regions_in_region_list = true;
	region_list_sort_type = (Editing::RegionListSortType) 0; 
	have_pending_keyboard_selection = false;
	_follow_playhead = true;
	_xfade_visibility = true;
	editor_ruler_menu = 0;
	no_ruler_shown_update = false;
	edit_group_list_menu = 0;
	route_list_menu = 0;
	region_list_menu = 0;
	marker_menu = 0;
	start_end_marker_menu = 0;
	range_marker_menu = 0;
	marker_menu_item = 0;
	tm_marker_menu = 0;
	transport_marker_menu = 0;
	new_transport_marker_menu = 0;
	editor_mixer_strip_width = Wide;
	show_editor_mixer_when_tracks_arrive = false;
	region_edit_menu_split_item = 0;
	temp_location = 0;
	region_edit_menu_split_multichannel_item = 0;
	leftmost_frame = 0;
	ignore_mouse_mode_toggle = false;
	current_stepping_trackview = 0;
	entered_track = 0;
	entered_regionview = 0;
	clear_entered_track = false;
	_new_regionviews_show_envelope = false;
	current_timestretch = 0;
	in_edit_group_row_change = false;
	last_canvas_frame = 0;
	edit_cursor = 0;
	playhead_cursor = 0;
	button_release_can_deselect = true;
	canvas_idle_queued = false;
	_dragging_playhead = false;

	location_marker_color = color_map[cLocationMarker];
	location_range_color = color_map[cLocationRange];
	location_cd_marker_color = color_map[cLocationCDMarker];
	location_loop_color = color_map[cLocationLoop];
	location_punch_color = color_map[cLocationPunch];

	range_marker_drag_rect = 0;
	marker_drag_line = 0;
	
	set_mouse_mode (MouseObject, true);

	frames_per_unit = 2048; /* too early to use reset_zoom () */
	reset_hscrollbar_stepping ();
	
	zoom_focus = ZoomFocusLeft;
	set_zoom_focus (ZoomFocusLeft);
 	zoom_range_clock.ValueChanged.connect (mem_fun(*this, &Editor::zoom_adjustment_changed));

	initialize_rulers ();
	initialize_canvas ();

	edit_controls_vbox.set_spacing (0);
	horizontal_adjustment.signal_value_changed().connect (mem_fun(*this, &Editor::canvas_horizontally_scrolled));
	vertical_adjustment.signal_value_changed().connect (mem_fun(*this, &Editor::tie_vertical_scrolling));
	
	track_canvas.set_hadjustment (horizontal_adjustment);
	track_canvas.set_vadjustment (vertical_adjustment);
	time_canvas.set_hadjustment (horizontal_adjustment);

	track_canvas.signal_map_event().connect (mem_fun (*this, &Editor::track_canvas_map_handler));
	time_canvas.signal_map_event().connect (mem_fun (*this, &Editor::time_canvas_map_handler));
	
	controls_layout.add (edit_controls_vbox);
	controls_layout.set_name ("EditControlsBase");
	controls_layout.add_events (Gdk::SCROLL_MASK);
	controls_layout.signal_scroll_event().connect (mem_fun(*this, &Editor::control_layout_scroll), false);
	
	controls_layout.add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK|Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK);
	controls_layout.signal_button_release_event().connect (mem_fun(*this, &Editor::edit_controls_button_release));
	controls_layout.signal_size_request().connect (mem_fun (*this, &Editor::controls_layout_size_request));

	edit_vscrollbar.set_adjustment (vertical_adjustment);
	edit_hscrollbar.set_adjustment (horizontal_adjustment);

 	edit_hscrollbar.signal_button_press_event().connect (mem_fun(*this, &Editor::hscrollbar_button_press));
 	edit_hscrollbar.signal_button_release_event().connect (mem_fun(*this, &Editor::hscrollbar_button_release));
 	edit_hscrollbar.signal_size_allocate().connect (mem_fun(*this, &Editor::hscrollbar_allocate));

	build_cursors ();
	setup_toolbar ();

 	edit_cursor_clock.ValueChanged.connect (mem_fun(*this, &Editor::edit_cursor_clock_changed));
	
	time_canvas_vbox.pack_start (*minsec_ruler, false, false);
	time_canvas_vbox.pack_start (*smpte_ruler, false, false);
	time_canvas_vbox.pack_start (*frames_ruler, false, false);
	time_canvas_vbox.pack_start (*bbt_ruler, false, false);
	time_canvas_vbox.pack_start (time_canvas, true, true);
	time_canvas_vbox.set_size_request (-1, (int)(timebar_height * visible_timebars));

	bbt_label.set_name ("EditorTimeButton");
	bbt_label.set_size_request (-1, (int)timebar_height);
	bbt_label.set_alignment (1.0, 0.5);
	bbt_label.set_padding (5,0);
	minsec_label.set_name ("EditorTimeButton");
	minsec_label.set_size_request (-1, (int)timebar_height);
	minsec_label.set_alignment (1.0, 0.5);
	minsec_label.set_padding (5,0);
	smpte_label.set_name ("EditorTimeButton");
	smpte_label.set_size_request (-1, (int)timebar_height);
	smpte_label.set_alignment (1.0, 0.5);
	smpte_label.set_padding (5,0);
	frame_label.set_name ("EditorTimeButton");
	frame_label.set_size_request (-1, (int)timebar_height);
	frame_label.set_alignment (1.0, 0.5);
	frame_label.set_padding (5,0);
	tempo_label.set_name ("EditorTimeButton");
	tempo_label.set_size_request (-1, (int)timebar_height);
	tempo_label.set_alignment (1.0, 0.5);
	tempo_label.set_padding (5,0);
	meter_label.set_name ("EditorTimeButton");
	meter_label.set_size_request (-1, (int)timebar_height);
	meter_label.set_alignment (1.0, 0.5);
	meter_label.set_padding (5,0);
	mark_label.set_name ("EditorTimeButton");
	mark_label.set_size_request (-1, (int)timebar_height);
	mark_label.set_alignment (1.0, 0.5);
	mark_label.set_padding (5,0);
	range_mark_label.set_name ("EditorTimeButton");
	range_mark_label.set_size_request (-1, (int)timebar_height);
	range_mark_label.set_alignment (1.0, 0.5);
	range_mark_label.set_padding (5,0);
	transport_mark_label.set_name ("EditorTimeButton");
	transport_mark_label.set_size_request (-1, (int)timebar_height);
	transport_mark_label.set_alignment (1.0, 0.5);
	transport_mark_label.set_padding (5,0);
	
	time_button_vbox.pack_start (minsec_label, false, false);
	time_button_vbox.pack_start (smpte_label, false, false);
	time_button_vbox.pack_start (frame_label, false, false);
	time_button_vbox.pack_start (bbt_label, false, false);
	time_button_vbox.pack_start (meter_label, false, false);
	time_button_vbox.pack_start (tempo_label, false, false);
	time_button_vbox.pack_start (mark_label, false, false);

	time_button_event_box.add (time_button_vbox);
	
	time_button_event_box.set_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);
	time_button_event_box.set_name ("TimebarLabelBase");
	time_button_event_box.signal_button_release_event().connect (mem_fun(*this, &Editor::ruler_label_button_release));

	/* these enable us to have a dedicated window (for cursor setting, etc.) 
	   for the canvas areas.
	*/

	track_canvas_event_box.add (track_canvas);

	time_canvas_event_box.add (time_canvas_vbox);
	time_canvas_event_box.set_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK|Gdk::POINTER_MOTION_MASK);
	
	edit_packer.set_col_spacings (0);
	edit_packer.set_row_spacings (0);
	edit_packer.set_homogeneous (false);
	edit_packer.set_border_width (0);
	edit_packer.set_name ("EditorWindow");
	
	edit_packer.attach (edit_vscrollbar,         0, 1, 1, 3,    FILL,        FILL|EXPAND, 0, 0);

	edit_packer.attach (time_button_event_box,   1, 2, 0, 1,    FILL,        FILL, 0, 0);
	edit_packer.attach (time_canvas_event_box,   2, 3, 0, 1,    FILL|EXPAND, FILL, 0, 0);

	edit_packer.attach (controls_layout,         1, 2, 1, 2,    FILL,        FILL|EXPAND, 0, 0);
	edit_packer.attach (track_canvas_event_box,  2, 3, 1, 2,    FILL|EXPAND, FILL|EXPAND, 0, 0);

	edit_packer.attach (zoom_box,                1, 2, 2, 3,    FILL,         FILL, 0, 0);
	edit_packer.attach (edit_hscrollbar,         2, 3, 2, 3,    FILL|EXPAND,  FILL, 0, 0);

	bottom_hbox.set_border_width (2);
	bottom_hbox.set_spacing (3);

	route_display_model = ListStore::create(route_display_columns);
	route_list_display.set_model (route_display_model);
	route_list_display.append_column (_("Show"), route_display_columns.visible);
	route_list_display.append_column (_("Name"), route_display_columns.text);
	route_list_display.get_column (0)->set_data (X_("colnum"), GUINT_TO_POINTER(0));
	route_list_display.get_column (1)->set_data (X_("colnum"), GUINT_TO_POINTER(1));
	route_list_display.set_headers_visible (true);
	route_list_display.set_name ("TrackListDisplay");
	route_list_display.get_selection()->set_mode (SELECTION_NONE);
	route_list_display.set_reorderable (true);
	route_list_display.set_size_request (100,-1);

	CellRendererToggle* route_list_visible_cell = dynamic_cast<CellRendererToggle*>(route_list_display.get_column_cell_renderer (0));
	route_list_visible_cell->property_activatable() = true;
	route_list_visible_cell->property_radio() = false;
	
	route_display_model->signal_row_deleted().connect (mem_fun (*this, &Editor::route_list_delete));
	route_display_model->signal_row_changed().connect (mem_fun (*this, &Editor::route_list_change));

	route_list_display.signal_button_press_event().connect (mem_fun (*this, &Editor::route_list_display_button_press), false);

	route_list_scroller.add (route_list_display);
	route_list_scroller.set_policy (POLICY_NEVER, POLICY_AUTOMATIC);

	group_model = ListStore::create(group_columns);
	edit_group_display.set_model (group_model);
	edit_group_display.append_column (_("Name"), group_columns.text);
	edit_group_display.append_column (_("Active"), group_columns.is_active);
	edit_group_display.append_column (_("Show"), group_columns.is_visible);
	edit_group_display.get_column (0)->set_data (X_("colnum"), GUINT_TO_POINTER(0));
	edit_group_display.get_column (1)->set_data (X_("colnum"), GUINT_TO_POINTER(1));
	edit_group_display.get_column (2)->set_data (X_("colnum"), GUINT_TO_POINTER(2));
	edit_group_display.get_column (0)->set_expand (true);
	edit_group_display.get_column (1)->set_expand (false);
	edit_group_display.get_column (2)->set_expand (false);
	edit_group_display.set_headers_visible (true);

	/* name is directly editable */

	CellRendererText* name_cell = dynamic_cast<CellRendererText*>(edit_group_display.get_column_cell_renderer (0));
	name_cell->property_editable() = true;
	name_cell->signal_edited().connect (mem_fun (*this, &Editor::edit_group_name_edit));

	/* use checkbox for the active + visible columns */

	CellRendererToggle* active_cell = dynamic_cast<CellRendererToggle*>(edit_group_display.get_column_cell_renderer (1));
	active_cell->property_activatable() = true;
	active_cell->property_radio() = false;

	active_cell = dynamic_cast<CellRendererToggle*>(edit_group_display.get_column_cell_renderer (1));
	active_cell->property_activatable() = true;
	active_cell->property_radio() = false;

	edit_group_display.set_name ("EditGroupList");

	group_model->signal_row_changed().connect (mem_fun (*this, &Editor::edit_group_row_change));

	edit_group_display.set_name ("EditGroupList");
	edit_group_display.get_selection()->set_mode (SELECTION_SINGLE);
	edit_group_display.set_reorderable (false);
	edit_group_display.set_rules_hint (true);
	edit_group_display.set_size_request (75, -1);

	edit_group_display_scroller.add (edit_group_display);
	edit_group_display_scroller.set_policy (POLICY_AUTOMATIC, POLICY_AUTOMATIC);

	edit_group_display.signal_button_press_event().connect (mem_fun(*this, &Editor::edit_group_list_button_press_event), false);

	VBox* edit_group_display_packer = manage (new VBox());
	HBox* edit_group_display_button_box = manage (new HBox());
	edit_group_display_button_box->set_homogeneous (true);

	Button* edit_group_add_button = manage (new Button ());
	Button* edit_group_remove_button = manage (new Button ());

	Widget* w;

	w = manage (new Image (Stock::ADD, ICON_SIZE_BUTTON));
	w->show();
	edit_group_add_button->add (*w);

	w = manage (new Image (Stock::REMOVE, ICON_SIZE_BUTTON));
	w->show();
	edit_group_remove_button->add (*w);

	edit_group_add_button->signal_clicked().connect (mem_fun (*this, &Editor::new_edit_group));
	edit_group_remove_button->signal_clicked().connect (mem_fun (*this, &Editor::remove_selected_edit_group));
	
	edit_group_display_button_box->pack_start (*edit_group_add_button);
	edit_group_display_button_box->pack_start (*edit_group_remove_button);

	edit_group_display_packer->pack_start (edit_group_display_scroller, true, true);
	edit_group_display_packer->pack_start (*edit_group_display_button_box, false, false);

	region_list_display.set_size_request (100, -1);
	region_list_display.set_name ("RegionListDisplay");

	region_list_model = TreeStore::create (region_list_columns);
	region_list_model->set_sort_func (0, mem_fun (*this, &Editor::region_list_sorter));
	region_list_model->set_sort_column (0, SORT_ASCENDING);

	region_list_display.set_model (region_list_model);
	region_list_display.append_column (_("Regions"), region_list_columns.name);
	region_list_display.set_headers_visible (false);

	region_list_display.get_selection()->set_select_function (mem_fun (*this, &Editor::region_list_selection_filter));
	
	TreeViewColumn* tv_col = region_list_display.get_column(0);
	CellRendererText* renderer = dynamic_cast<CellRendererText*>(region_list_display.get_column_cell_renderer (0));
	tv_col->add_attribute(renderer->property_text(), region_list_columns.name);
	tv_col->add_attribute(renderer->property_foreground_gdk(), region_list_columns.color_);
	
	region_list_display.get_selection()->set_mode (SELECTION_MULTIPLE);
	region_list_display.add_object_drag (region_list_columns.region.index(), "regions");

	/* setup DnD handling */
	
	list<TargetEntry> region_list_target_table;
	
	region_list_target_table.push_back (TargetEntry ("text/plain"));
	region_list_target_table.push_back (TargetEntry ("text/uri-list"));
	region_list_target_table.push_back (TargetEntry ("application/x-rootwin-drop"));
	
	region_list_display.add_drop_targets (region_list_target_table);
	region_list_display.signal_drag_data_received().connect (mem_fun(*this, &Editor::region_list_display_drag_data_received));

	region_list_scroller.add (region_list_display);
	region_list_scroller.set_policy (POLICY_NEVER, POLICY_AUTOMATIC);

	region_list_display.signal_key_press_event().connect (mem_fun(*this, &Editor::region_list_display_key_press));
	region_list_display.signal_key_release_event().connect (mem_fun(*this, &Editor::region_list_display_key_release));
	region_list_display.signal_button_press_event().connect (mem_fun(*this, &Editor::region_list_display_button_press), false);
	region_list_display.signal_button_release_event().connect (mem_fun(*this, &Editor::region_list_display_button_release));
	region_list_display.get_selection()->signal_changed().connect (mem_fun(*this, &Editor::region_list_selection_changed));
	// region_list_display.signal_popup_menu().connect (bind (mem_fun (*this, &Editor::show_region_list_display_context_menu), 1, 0));
	
	named_selection_scroller.add (named_selection_display);
	named_selection_scroller.set_policy (POLICY_NEVER, POLICY_AUTOMATIC);

	named_selection_model = TreeStore::create (named_selection_columns);
	named_selection_display.set_model (named_selection_model);
	named_selection_display.append_column (_("Chunks"), named_selection_columns.text);
	named_selection_display.set_headers_visible (false);
	named_selection_display.set_size_request (100, -1);
	named_selection_display.set_name ("RegionListDisplay");
	
	named_selection_display.get_selection()->set_mode (SELECTION_SINGLE);
	named_selection_display.set_size_request (100, -1);
	named_selection_display.signal_button_release_event().connect (mem_fun(*this, &Editor::named_selection_display_button_press), false);
	named_selection_display.get_selection()->signal_changed().connect (mem_fun (*this, &Editor::named_selection_display_selection_changed));

	/* SNAPSHOTS */

	snapshot_display_model = ListStore::create (snapshot_display_columns);
	snapshot_display.set_model (snapshot_display_model);
	snapshot_display.append_column (X_("snapshot"), snapshot_display_columns.visible_name);
	snapshot_display.set_name ("SnapshotDisplayList");
	snapshot_display.set_size_request (75, -1);
	snapshot_display.set_headers_visible (false);
	snapshot_display.set_reorderable (false);
	snapshot_display_scroller.add (snapshot_display);
	snapshot_display_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	snapshot_display.get_selection()->signal_changed().connect (mem_fun(*this, &Editor::snapshot_display_selection_changed));
	snapshot_display.signal_button_press_event().connect (mem_fun (*this, &Editor::snapshot_display_button_press), false);

	Gtk::Label* nlabel;

	nlabel = manage (new Label (_("Regions")));
	nlabel->set_angle (-90);
       	the_notebook.append_page (region_list_scroller, *nlabel);
	nlabel = manage (new Label (_("Tracks/Busses")));
	nlabel->set_angle (-90);
       	the_notebook.append_page (route_list_scroller, *nlabel);
	nlabel = manage (new Label (_("Snapshots")));
	nlabel->set_angle (-90);
	the_notebook.append_page (snapshot_display_scroller, *nlabel);
	nlabel = manage (new Label (_("Edit Groups")));
	nlabel->set_angle (-90);
	the_notebook.append_page (*edit_group_display_packer, *nlabel);
	nlabel = manage (new Label (_("Chunks")));
	nlabel->set_angle (-90);
	the_notebook.append_page (named_selection_scroller, *nlabel);

	the_notebook.set_show_tabs (true);
	the_notebook.set_scrollable (true);
	the_notebook.popup_enable ();
	the_notebook.set_tab_pos (Gtk::POS_RIGHT);

	post_maximal_editor_width = 0;
	post_maximal_pane_position = 0;
	edit_pane.pack1 (edit_packer, true, true);
	edit_pane.pack2 (the_notebook, false, true);
	
	edit_pane.signal_size_allocate().connect (bind (mem_fun(*this, &Editor::pane_allocation_handler), static_cast<Paned*> (&edit_pane)));

	top_hbox.pack_start (toolbar_frame, true, true);

	HBox *hbox = manage (new HBox);
	hbox->pack_start (edit_pane, true, true);

	global_vpacker.pack_start (top_hbox, false, false);
	global_vpacker.pack_start (*hbox, true, true);

	global_hpacker.pack_start (global_vpacker, true, true);

	set_name ("EditorWindow");
	add_accel_group (ActionManager::ui_manager->get_accel_group());

	vpacker.pack_end (global_hpacker, true, true);

	/* register actions now so that set_state() can find them and set toggles/checks etc */
	
	register_actions ();
	
	XMLNode* node = ARDOUR_UI::instance()->editor_settings();
	set_state (*node);

	_playlist_selector = new PlaylistSelector();
	_playlist_selector->signal_delete_event().connect (bind (sigc::ptr_fun (just_hide_it), static_cast<Window *> (_playlist_selector)));

	RegionView::RegionViewGoingAway.connect (mem_fun(*this, &Editor::catch_vanishing_regionview));

	/* nudge stuff */

	nudge_forward_button.add (*(manage (new Image (::get_icon("nudge_right")))));
	nudge_backward_button.add (*(manage (new Image (::get_icon("nudge_left")))));

	ARDOUR_UI::instance()->tooltips().set_tip (nudge_forward_button, _("Nudge Region/Selection Forwards"));
	ARDOUR_UI::instance()->tooltips().set_tip (nudge_backward_button, _("Nudge Region/Selection Backwards"));

	nudge_forward_button.set_name ("TransportButton");
	nudge_backward_button.set_name ("TransportButton");

	fade_context_menu.set_name ("ArdourContextMenu");

	/* icons, titles, WM stuff */

	list<Glib::RefPtr<Gdk::Pixbuf> > window_icons;
	Glib::RefPtr<Gdk::Pixbuf> icon;

	if ((icon = ::get_icon ("ardour_icon_16px")) != 0) {
		window_icons.push_back (icon);
	}
	if ((icon = ::get_icon ("ardour_icon_22px")) != 0) {
		window_icons.push_back (icon);
	}
	if ((icon = ::get_icon ("ardour_icon_32px")) != 0) {
		window_icons.push_back (icon);
	}
	if ((icon = ::get_icon ("ardour_icon_48px")) != 0) {
		window_icons.push_back (icon);
	}
	if (!window_icons.empty()) {
		set_icon_list (window_icons);
		set_default_icon_list (window_icons);
	}
	set_title (_("ardour: editor"));
	set_wmclass (X_("ardour_editor"), "Ardour");

	add (vpacker);
	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK);

	signal_configure_event().connect (mem_fun (*ARDOUR_UI::instance(), &ARDOUR_UI::configure_handler));
	signal_delete_event().connect (mem_fun (*ARDOUR_UI::instance(), &ARDOUR_UI::exit_on_main_window_close));

	/* allow external control surfaces/protocols to do various things */

	ControlProtocol::ZoomToSession.connect (mem_fun (*this, &Editor::temporal_zoom_session));
	ControlProtocol::ZoomIn.connect (bind (mem_fun (*this, &Editor::temporal_zoom_step), false));
	ControlProtocol::ZoomOut.connect (bind (mem_fun (*this, &Editor::temporal_zoom_step), true));
	ControlProtocol::ScrollTimeline.connect (mem_fun (*this, &Editor::control_scroll));

	Config->ParameterChanged.connect (mem_fun (*this, &Editor::parameter_changed));

	constructed = true;
	instant_save ();
}

Editor::~Editor()
{
	/* <CMT Additions> */
	if(image_socket_listener)
	{
		if(image_socket_listener->is_connected())
		{
			image_socket_listener->close_connection() ;
		}
		
		delete image_socket_listener ;
		image_socket_listener = 0 ;
	}
	/* </CMT Additions> */
}

void
Editor::add_toplevel_controls (Container& cont)
{
	vpacker.pack_start (cont, false, false);
	cont.show_all ();
}

void
Editor::catch_vanishing_regionview (RegionView *rv)
{
	/* note: the selection will take care of the vanishing
	   audioregionview by itself.
	*/

	if (clicked_regionview == rv) {
		clicked_regionview = 0;
	}

	if (entered_regionview == rv) {
		set_entered_regionview (0);
	}
}

void
Editor::set_entered_regionview (RegionView* rv)
{
	if (rv == entered_regionview) {
		return;
	}

	if (entered_regionview) {
		entered_regionview->exited ();
	}

	if ((entered_regionview = rv) != 0) {
		entered_regionview->entered ();
	}
}

void
Editor::set_entered_track (TimeAxisView* tav)
{
	if (entered_track) {
		entered_track->exited ();
	}

	if ((entered_track = tav) != 0) {
		entered_track->entered ();
	}
}

void
Editor::show_window ()
{
	show_all ();
	present ();

	/* now reset all audio_time_axis heights, because widgets might need
	   to be re-hidden
	*/
	
	TimeAxisView *tv;
	
	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		tv = (static_cast<TimeAxisView*>(*i));
		tv->reset_height ();
	}
}

void
Editor::tie_vertical_scrolling ()
{
	double y1 = vertical_adjustment.get_value();
	controls_layout.get_vadjustment()->set_value (y1);
	playhead_cursor->set_y_axis(y1);
	edit_cursor->set_y_axis(y1);
}

void
Editor::instant_save ()
{
	if (!constructed || !ARDOUR_UI::instance()->session_loaded) {
		return;
	}

	if (session) {
		session->add_instant_xml(get_state(), session->path());
	} else {
		Config->add_instant_xml(get_state(), get_user_ardour_path());
	}
}

void
Editor::edit_cursor_clock_changed()
{
	if (edit_cursor->current_frame != edit_cursor_clock.current_time()) {
		edit_cursor->set_position (edit_cursor_clock.current_time());
	}
}


void
Editor::zoom_adjustment_changed ()
{
	if (session == 0) {
		return;
	}

	double fpu = zoom_range_clock.current_duration() / canvas_width;

	if (fpu < 1.0) {
		fpu = 1.0;
		zoom_range_clock.set ((nframes_t) floor (fpu * canvas_width));
	} else if (fpu > session->current_end_frame() / canvas_width) {
		fpu = session->current_end_frame() / canvas_width;
		zoom_range_clock.set ((nframes_t) floor (fpu * canvas_width));
	}
	
	temporal_zoom (fpu);
}

void
Editor::control_scroll (float fraction)
{
	ENSURE_GUI_THREAD(bind (mem_fun (*this, &Editor::control_scroll), fraction));

	if (!session) {
		return;
	}

	double step = fraction * current_page_frames();
	nframes_t target;

	if ((fraction < 0.0f) && (session->transport_frame() < (nframes_t) fabs(step))) {
		target = 0;
	} else if ((fraction > 0.0f) && (max_frames - session->transport_frame() < step)) {
		target = (max_frames - (current_page_frames()*2)); // allow room for slop in where the PH is on the screen
	} else {
		target = (session->transport_frame() + (nframes_t) floor ((fraction * current_page_frames())));
	}

	/* move visuals, we'll catch up with it later */

	playhead_cursor->set_position (target);

	if (target > (current_page_frames() / 2)) {
		/* try to center PH in window */
		reset_x_origin (target - (current_page_frames()/2));
	} else {
		reset_x_origin (0);
	}

	/* cancel the existing */

	control_scroll_connection.disconnect ();

	/* add the next one */

	control_scroll_connection = Glib::signal_timeout().connect (bind (mem_fun (*this, &Editor::deferred_control_scroll), target), 50);
}

bool
Editor::deferred_control_scroll (nframes_t target)
{
	session->request_locate (target);
	return false;
}

void
Editor::on_realize ()
{
	Window::on_realize ();
	Realized ();
}

void
Editor::start_scrolling ()
{
	scroll_connection = ARDOUR_UI::instance()->SuperRapidScreenUpdate.connect 
		(mem_fun(*this, &Editor::update_current_screen));
}

void
Editor::stop_scrolling ()
{
	scroll_connection.disconnect ();
}

void
Editor::map_position_change (nframes_t frame)
{
	ENSURE_GUI_THREAD (bind (mem_fun(*this, &Editor::map_position_change), frame));

	if (session == 0 || !_follow_playhead) {
		return;
	}

	center_screen (frame);
	playhead_cursor->set_position (frame);
}	

void
Editor::center_screen (nframes_t frame)
{
	double page = canvas_width * frames_per_unit;

	/* if we're off the page, then scroll.
	 */
	
	if (frame < leftmost_frame || frame >= leftmost_frame + page) {
		center_screen_internal (frame, page);
	}
}

void
Editor::center_screen_internal (nframes_t frame, float page)
{
	page /= 2;
		
	if (frame > page) {
		frame -= (nframes_t) page;
	} else {
		frame = 0;
	}

	reset_x_origin (frame);
}

void
Editor::handle_new_duration ()
{
	ENSURE_GUI_THREAD (mem_fun (*this, &Editor::handle_new_duration));

	nframes_t new_end = session->get_maximum_extent() + (nframes_t) floorf (current_page_frames() * 0.10f);
				  
	if (new_end > last_canvas_frame) {
		last_canvas_frame = new_end;
		reset_scrolling_region ();
	}

	horizontal_adjustment.set_value (leftmost_frame/frames_per_unit);
}

void
Editor::update_title_s (const string & snap_name)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &Editor::update_title_s), snap_name));
	
	update_title ();
}

void
Editor::update_title ()
{
	ENSURE_GUI_THREAD (mem_fun(*this, &Editor::update_title));

	if (session) {
		bool dirty = session->dirty();

		string wintitle = _("ardour: editor: ");

		if (dirty) {
			wintitle += '[';
		}

		wintitle += session->name();

		if (session->snap_name() != session->name()) {
			wintitle += ':';
			wintitle += session->snap_name();
		}

		if (dirty) {
			wintitle += ']';
		}

		set_title (wintitle);
	}
}

void
Editor::connect_to_session (Session *t)
{
	session = t;

	XMLNode* node = ARDOUR_UI::instance()->editor_settings();
	set_state (*node);

	/* catch up with the playhead */

	session->request_locate (playhead_cursor->current_frame);

	if (first_action_message) {
	        first_action_message->hide();
	}

	update_title ();

	session->GoingAway.connect (mem_fun(*this, &Editor::session_going_away));
	session->history().Changed.connect (mem_fun (*this, &Editor::history_changed));

	/* These signals can all be emitted by a non-GUI thread. Therefore the
	   handlers for them must not attempt to directly interact with the GUI,
	   but use Gtkmm2ext::UI::instance()->call_slot();
	*/

	session_connections.push_back (session->TransportStateChange.connect (mem_fun(*this, &Editor::map_transport_state)));
	session_connections.push_back (session->PositionChanged.connect (mem_fun(*this, &Editor::map_position_change)));
	session_connections.push_back (session->RouteAdded.connect (mem_fun(*this, &Editor::handle_new_route)));
	session_connections.push_back (session->RegionAdded.connect (mem_fun(*this, &Editor::handle_new_region)));
	session_connections.push_back (session->RegionRemoved.connect (mem_fun(*this, &Editor::handle_region_removed)));
	session_connections.push_back (session->DurationChanged.connect (mem_fun(*this, &Editor::handle_new_duration)));
	session_connections.push_back (session->edit_group_added.connect (mem_fun(*this, &Editor::add_edit_group)));
	session_connections.push_back (session->edit_group_removed.connect (mem_fun(*this, &Editor::edit_groups_changed)));
	session_connections.push_back (session->NamedSelectionAdded.connect (mem_fun(*this, &Editor::handle_new_named_selection)));
	session_connections.push_back (session->NamedSelectionRemoved.connect (mem_fun(*this, &Editor::handle_new_named_selection)));
	session_connections.push_back (session->DirtyChanged.connect (mem_fun(*this, &Editor::update_title)));
	session_connections.push_back (session->StateSaved.connect (mem_fun(*this, &Editor::update_title_s)));
	session_connections.push_back (session->AskAboutPlaylistDeletion.connect (mem_fun(*this, &Editor::playlist_deletion_dialog)));
	session_connections.push_back (session->RegionHiddenChange.connect (mem_fun(*this, &Editor::region_hidden)));

	session_connections.push_back (session->SMPTEOffsetChanged.connect (mem_fun(*this, &Editor::update_just_smpte)));

	session_connections.push_back (session->tempo_map().StateChanged.connect (mem_fun(*this, &Editor::tempo_map_changed)));

	edit_groups_changed ();

	edit_cursor_clock.set_session (session);
	zoom_range_clock.set_session (session);
	_playlist_selector->set_session (session);
	nudge_clock.set_session (session);

#ifdef FFT_ANALYSIS
	if (analysis_window != 0)
		analysis_window->set_session (session);
#endif

	Location* loc = session->locations()->auto_loop_location();
	if (loc == 0) {
		loc = new Location (0, session->current_end_frame(), _("Loop"),(Location::Flags) (Location::IsAutoLoop | Location::IsHidden));
		if (loc->start() == loc->end()) {
			loc->set_end (loc->start() + 1);
		}
		session->locations()->add (loc, false);
		session->set_auto_loop_location (loc);
	}
	else {
		// force name
		loc->set_name (_("Loop"));
	}
	
	loc = session->locations()->auto_punch_location();
	if (loc == 0) {
		loc = new Location (0, session->current_end_frame(), _("Punch"), (Location::Flags) (Location::IsAutoPunch | Location::IsHidden));
		if (loc->start() == loc->end()) {
			loc->set_end (loc->start() + 1);
		}
		session->locations()->add (loc, false);
		session->set_auto_punch_location (loc);
	}
	else {
		// force name
		loc->set_name (_("Punch"));
	}

	Config->map_parameters (mem_fun (*this, &Editor::parameter_changed));
	
	session->StateSaved.connect (mem_fun(*this, &Editor::session_state_saved));
	
	refresh_location_display ();
	session->locations()->added.connect (mem_fun(*this, &Editor::add_new_location));
	session->locations()->removed.connect (mem_fun(*this, &Editor::location_gone));
	session->locations()->changed.connect (mem_fun(*this, &Editor::refresh_location_display));
	session->locations()->StateChanged.connect (mem_fun(*this, &Editor::refresh_location_display_s));
	session->locations()->end_location()->changed.connect (mem_fun(*this, &Editor::end_location_changed));

	handle_new_duration ();

	redisplay_regions ();
	redisplay_named_selections ();
	redisplay_snapshots ();

	initial_route_list_display ();

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		(static_cast<TimeAxisView*>(*i))->set_samples_per_unit (frames_per_unit);
	}

	restore_ruler_visibility ();
	//tempo_map_changed (Change (0));
	session->tempo_map().apply_with_metrics (*this, &Editor::draw_metric_marks);

	start_scrolling ();

	/* don't show master bus in a new session */

	if (ARDOUR_UI::instance()->session_is_new ()) {

	        TreeModel::Children rows = route_display_model->children();
		TreeModel::Children::iterator i;
	
		no_route_list_redisplay = true;
		
		for (i = rows.begin(); i != rows.end(); ++i) {
			TimeAxisView *tv =  (*i)[route_display_columns.tv];
			RouteTimeAxisView *rtv;
			
			if ((rtv = dynamic_cast<RouteTimeAxisView*>(tv)) != 0) {
				if (rtv->route()->master()) {
					route_list_display.get_selection()->unselect (i);
				}
			}
		}
		
		no_route_list_redisplay = false;
		redisplay_route_list ();
	}

        /* register for undo history */

        session->register_with_memento_command_factory(_id, this);
}

void
Editor::build_cursors ()
{
	using namespace Gdk;
	
	Gdk::Color mbg ("#000000" ); /* Black */
	Gdk::Color mfg ("#0000ff" ); /* Blue. */

	{
		RefPtr<Bitmap> source, mask;
		source = Bitmap::create (mag_bits, mag_width, mag_height);
		mask = Bitmap::create (magmask_bits, mag_width, mag_height);
		zoom_cursor = new Gdk::Cursor (source, mask, mfg, mbg, mag_x_hot, mag_y_hot);
	}

	Gdk::Color fbg ("#ffffff" );
	Gdk::Color ffg  ("#000000" );
	
	{
		RefPtr<Bitmap> source, mask;
		
		source = Bitmap::create (fader_cursor_bits, fader_cursor_width, fader_cursor_height);
		mask = Bitmap::create (fader_cursor_mask_bits, fader_cursor_width, fader_cursor_height);
		fader_cursor = new Gdk::Cursor (source, mask, ffg, fbg, fader_cursor_x_hot, fader_cursor_y_hot);
	}
	
	{ 
		RefPtr<Bitmap> source, mask;
		source = Bitmap::create (speaker_cursor_bits, speaker_cursor_width, speaker_cursor_height);
		mask = Bitmap::create (speaker_cursor_mask_bits, speaker_cursor_width, speaker_cursor_height);
		speaker_cursor = new Gdk::Cursor (source, mask, ffg, fbg, speaker_cursor_x_hot, speaker_cursor_y_hot);
	}

	grabber_cursor = new Gdk::Cursor (HAND2);
	cross_hair_cursor = new Gdk::Cursor (CROSSHAIR);
	trimmer_cursor =  new Gdk::Cursor (SB_H_DOUBLE_ARROW);
	selector_cursor = new Gdk::Cursor (XTERM);
	time_fx_cursor = new Gdk::Cursor (SIZING);
	wait_cursor = new Gdk::Cursor  (WATCH);
	timebar_cursor = new Gdk::Cursor(LEFT_PTR);
}

void
Editor::popup_fade_context_menu (int button, int32_t time, ArdourCanvas::Item* item, ItemType item_type)
{
	using namespace Menu_Helpers;
	AudioRegionView* arv = static_cast<AudioRegionView*> (item->get_data ("regionview"));

	if (arv == 0) {
		fatal << _("programming error: fade in canvas item has no regionview data pointer!") << endmsg;
		/*NOTREACHED*/
	}

	MenuList& items (fade_context_menu.items());

	items.clear ();

	switch (item_type) {
	case FadeInItem:
	case FadeInHandleItem:
		if (arv->audio_region()->fade_in_active()) {
			items.push_back (MenuElem (_("Deactivate"), bind (mem_fun (*arv, &AudioRegionView::set_fade_in_active), false)));
		} else {
			items.push_back (MenuElem (_("Activate"), bind (mem_fun (*arv, &AudioRegionView::set_fade_in_active), true)));
		}
		
		items.push_back (SeparatorElem());
		
		items.push_back (MenuElem (_("Linear"), bind (mem_fun (*arv, &AudioRegionView::set_fade_in_shape), AudioRegion::Linear)));
		items.push_back (MenuElem (_("Slowest"), bind (mem_fun (*arv, &AudioRegionView::set_fade_in_shape), AudioRegion::LogB)));
		items.push_back (MenuElem (_("Slow"), bind (mem_fun (*arv, &AudioRegionView::set_fade_in_shape), AudioRegion::Fast)));
		items.push_back (MenuElem (_("Fast"), bind (mem_fun (*arv, &AudioRegionView::set_fade_in_shape), AudioRegion::LogA)));
		items.push_back (MenuElem (_("Fastest"), bind (mem_fun (*arv, &AudioRegionView::set_fade_in_shape), AudioRegion::Slow)));
		break;

	case FadeOutItem:
	case FadeOutHandleItem:
		if (arv->audio_region()->fade_out_active()) {
			items.push_back (MenuElem (_("Deactivate"), bind (mem_fun (*arv, &AudioRegionView::set_fade_out_active), false)));
		} else {
			items.push_back (MenuElem (_("Activate"), bind (mem_fun (*arv, &AudioRegionView::set_fade_out_active), true)));
		}
		
		items.push_back (SeparatorElem());
		
		items.push_back (MenuElem (_("Linear"), bind (mem_fun (*arv, &AudioRegionView::set_fade_out_shape), AudioRegion::Linear)));
		items.push_back (MenuElem (_("Slowest"), bind (mem_fun (*arv, &AudioRegionView::set_fade_out_shape), AudioRegion::Fast)));
		items.push_back (MenuElem (_("Slow"), bind (mem_fun (*arv, &AudioRegionView::set_fade_out_shape), AudioRegion::LogB)));
		items.push_back (MenuElem (_("Fast"), bind (mem_fun (*arv, &AudioRegionView::set_fade_out_shape), AudioRegion::LogA)));
		items.push_back (MenuElem (_("Fastest"), bind (mem_fun (*arv, &AudioRegionView::set_fade_out_shape), AudioRegion::Slow)));

		break;

	default:
		fatal << _("programming error: ")
		      << X_("non-fade canvas item passed to popup_fade_context_menu()")
		      << endmsg;
		/*NOTREACHED*/
	}

	fade_context_menu.popup (button, time);
}

void
Editor::popup_track_context_menu (int button, int32_t time, ItemType item_type, bool with_selection, nframes_t frame)
{
	using namespace Menu_Helpers;
	Menu* (Editor::*build_menu_function)(nframes_t);
	Menu *menu;

	switch (item_type) {
	case RegionItem:
	case RegionViewName:
	case RegionViewNameHighlight:
		if (with_selection) {
			build_menu_function = &Editor::build_track_selection_context_menu;
		} else {
			build_menu_function = &Editor::build_track_region_context_menu;
		}
		break;

	case SelectionItem:
		if (with_selection) {
			build_menu_function = &Editor::build_track_selection_context_menu;
		} else {
			build_menu_function = &Editor::build_track_context_menu;
		}
		break;

	case CrossfadeViewItem:
		build_menu_function = &Editor::build_track_crossfade_context_menu;
		break;

	case StreamItem:
		if (clicked_routeview->is_track()) {
			build_menu_function = &Editor::build_track_context_menu;
		} else {
			build_menu_function = &Editor::build_track_bus_context_menu;
		}
		break;

	default:
		/* probably shouldn't happen but if it does, we don't care */
		return;
	}

	menu = (this->*build_menu_function)(frame);
	menu->set_name ("ArdourContextMenu");
	
	/* now handle specific situations */

	switch (item_type) {
	case RegionItem:
	case RegionViewName:
	case RegionViewNameHighlight:
		if (!with_selection) {
			if (region_edit_menu_split_item) {
				if (clicked_regionview && clicked_regionview->region()->covers (edit_cursor->current_frame)) {
					ActionManager::set_sensitive (ActionManager::edit_cursor_in_region_sensitive_actions, true);
				} else {
					ActionManager::set_sensitive (ActionManager::edit_cursor_in_region_sensitive_actions, false);
				}
			}
			/*
			if (region_edit_menu_split_multichannel_item) {
				if (clicked_regionview && clicked_regionview->region().n_channels() > 1) {
					// GTK2FIX find the action, change its sensitivity
					// region_edit_menu_split_multichannel_item->set_sensitive (true);
				} else {
					// GTK2FIX see above
					// region_edit_menu_split_multichannel_item->set_sensitive (false);
				}
			}*/
		}
		break;

	case SelectionItem:
		break;

	case CrossfadeViewItem:
		break;

	case StreamItem:
		break;

	default:
		/* probably shouldn't happen but if it does, we don't care */
		return;
	}

	if (clicked_routeview && clicked_routeview->audio_track()) {

		/* Bounce to disk */
		
		using namespace Menu_Helpers;
		MenuList& edit_items  = menu->items();
		
		edit_items.push_back (SeparatorElem());

		switch (clicked_routeview->audio_track()->freeze_state()) {
		case AudioTrack::NoFreeze:
			edit_items.push_back (MenuElem (_("Freeze"), mem_fun(*this, &Editor::freeze_route)));
			break;

		case AudioTrack::Frozen:
			edit_items.push_back (MenuElem (_("Unfreeze"), mem_fun(*this, &Editor::unfreeze_route)));
			break;
			
		case AudioTrack::UnFrozen:
			edit_items.push_back (MenuElem (_("Freeze"), mem_fun(*this, &Editor::freeze_route)));
			break;
		}

	}

	menu->popup (button, time);
}

Menu*
Editor::build_track_context_menu (nframes_t ignored)
{
	using namespace Menu_Helpers;

 	MenuList& edit_items = track_context_menu.items();
	edit_items.clear();

	add_dstream_context_items (edit_items);
	return &track_context_menu;
}

Menu*
Editor::build_track_bus_context_menu (nframes_t ignored)
{
	using namespace Menu_Helpers;

 	MenuList& edit_items = track_context_menu.items();
	edit_items.clear();

	add_bus_context_items (edit_items);
	return &track_context_menu;
}

Menu*
Editor::build_track_region_context_menu (nframes_t frame)
{
	using namespace Menu_Helpers;
	MenuList& edit_items  = track_region_context_menu.items();
	edit_items.clear();

	AudioTimeAxisView* atv = dynamic_cast<AudioTimeAxisView*> (clicked_axisview);

	if (atv) {
		boost::shared_ptr<Diskstream> ds;
		boost::shared_ptr<Playlist> pl;
		
		if ((ds = atv->get_diskstream()) && ((pl = ds->playlist()))) {
			Playlist::RegionList* regions = pl->regions_at ((nframes_t) floor ( (double)frame * ds->speed()));
			for (Playlist::RegionList::iterator i = regions->begin(); i != regions->end(); ++i) {
				add_region_context_items (atv->audio_view(), (*i), edit_items);
			}
			delete regions;
		}
	}

	add_dstream_context_items (edit_items);

	return &track_region_context_menu;
}

Menu*
Editor::build_track_crossfade_context_menu (nframes_t frame)
{
	using namespace Menu_Helpers;
	MenuList& edit_items  = track_crossfade_context_menu.items();
	edit_items.clear ();

	AudioTimeAxisView* atv = dynamic_cast<AudioTimeAxisView*> (clicked_axisview);

	if (atv) {
		boost::shared_ptr<Diskstream> ds;
		boost::shared_ptr<Playlist> pl;
		boost::shared_ptr<AudioPlaylist> apl;

		if ((ds = atv->get_diskstream()) && ((pl = ds->playlist()) != 0) && ((apl = boost::dynamic_pointer_cast<AudioPlaylist> (pl)) != 0)) {

			Playlist::RegionList* regions = pl->regions_at (frame);
			AudioPlaylist::Crossfades xfades;

			apl->crossfades_at (frame, xfades);

			bool many = xfades.size() > 1;

			for (AudioPlaylist::Crossfades::iterator i = xfades.begin(); i != xfades.end(); ++i) {
				add_crossfade_context_items (atv->audio_view(), (*i), edit_items, many);
			}

			for (Playlist::RegionList::iterator i = regions->begin(); i != regions->end(); ++i) {
				add_region_context_items (atv->audio_view(), (*i), edit_items);
			}

			delete regions;
		}
	}

	add_dstream_context_items (edit_items);

	return &track_crossfade_context_menu;
}

#ifdef FFT_ANALYSIS
void
Editor::analyze_region_selection()
{
	if (analysis_window == 0) {
		analysis_window = new AnalysisWindow();

		if (session != 0)
			analysis_window->set_session(session);

		analysis_window->show_all();
	}

	analysis_window->set_regionmode();
	analysis_window->analyze();
	
	analysis_window->present();
}

void
Editor::analyze_range_selection()
{
	if (analysis_window == 0) {
		analysis_window = new AnalysisWindow();

		if (session != 0)
			analysis_window->set_session(session);

		analysis_window->show_all();
	}

	analysis_window->set_rangemode();
	analysis_window->analyze();
	
	analysis_window->present();
}
#endif /* FFT_ANALYSIS */



Menu*
Editor::build_track_selection_context_menu (nframes_t ignored)
{
	using namespace Menu_Helpers;
	MenuList& edit_items  = track_selection_context_menu.items();
	edit_items.clear ();

	add_selection_context_items (edit_items);
	add_dstream_context_items (edit_items);

	return &track_selection_context_menu;
}

void
Editor::add_crossfade_context_items (AudioStreamView* view, boost::shared_ptr<Crossfade> xfade, Menu_Helpers::MenuList& edit_items, bool many)
{
	using namespace Menu_Helpers;
	Menu     *xfade_menu = manage (new Menu);
	MenuList& items       = xfade_menu->items();
	xfade_menu->set_name ("ArdourContextMenu");
	string str;

	if (xfade->active()) {
		str = _("Mute");
	} else { 
		str = _("Unmute");
	}

	items.push_back (MenuElem (str, bind (mem_fun(*this, &Editor::toggle_xfade_active), boost::weak_ptr<Crossfade> (xfade))));
	items.push_back (MenuElem (_("Edit"), bind (mem_fun(*this, &Editor::edit_xfade), boost::weak_ptr<Crossfade> (xfade))));

	if (xfade->can_follow_overlap()) {

		if (xfade->following_overlap()) {
			str = _("Convert to short");
		} else {
			str = _("Convert to full");
		}

		items.push_back (MenuElem (str, bind (mem_fun(*this, &Editor::toggle_xfade_length), xfade)));
	}

	if (many) {
		str = xfade->out()->name();
		str += "->";
		str += xfade->in()->name();
	} else {
		str = _("Crossfade");
	}

	edit_items.push_back (MenuElem (str, *xfade_menu));
	edit_items.push_back (SeparatorElem());
}

void
Editor::xfade_edit_left_region ()
{
	if (clicked_crossfadeview) {
		clicked_crossfadeview->left_view.show_region_editor ();
	}
}

void
Editor::xfade_edit_right_region ()
{
	if (clicked_crossfadeview) {
		clicked_crossfadeview->right_view.show_region_editor ();
	}
}

void
Editor::add_region_context_items (AudioStreamView* sv, boost::shared_ptr<Region> region, Menu_Helpers::MenuList& edit_items)
{
	using namespace Menu_Helpers;
	Menu     *region_menu = manage (new Menu);
	MenuList& items       = region_menu->items();
	region_menu->set_name ("ArdourContextMenu");
	
	boost::shared_ptr<AudioRegion> ar;

	if (region) {
		ar = boost::dynamic_pointer_cast<AudioRegion> (region);
	}

	/* when this particular menu pops up, make the relevant region 
	   become selected.
	*/

	// region_menu->signal_map_event().connect (bind (mem_fun(*this, &Editor::set_selected_regionview_from_map_event), sv, boost::weak_ptr<Region>(region)));

	items.push_back (MenuElem (_("Popup region editor"), mem_fun(*this, &Editor::edit_region)));
	items.push_back (MenuElem (_("Raise to top layer"), mem_fun(*this, &Editor::raise_region_to_top)));
	items.push_back (MenuElem (_("Lower to bottom layer"), mem_fun  (*this, &Editor::lower_region_to_bottom)));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Define sync point"), mem_fun(*this, &Editor::set_region_sync_from_edit_cursor)));
	items.push_back (MenuElem (_("Remove sync point"), mem_fun(*this, &Editor::remove_region_sync)));
	items.push_back (SeparatorElem());

	items.push_back (MenuElem (_("Audition"), mem_fun(*this, &Editor::audition_selected_region)));
	items.push_back (MenuElem (_("Export"), mem_fun(*this, &Editor::export_region)));
	items.push_back (MenuElem (_("Bounce"), mem_fun(*this, &Editor::bounce_region_selection)));

#ifdef FFT_ANALYSIS
	items.push_back (MenuElem (_("Analyze region"), mem_fun(*this, &Editor::analyze_region_selection)));
#endif

	items.push_back (SeparatorElem());

	items.push_back (CheckMenuElem (_("Lock"), mem_fun(*this, &Editor::toggle_region_lock)));
	region_lock_item = static_cast<CheckMenuItem*>(&items.back());
	if (region->locked()) {
		region_lock_item->set_active();
	}
	items.push_back (CheckMenuElem (_("Mute"), mem_fun(*this, &Editor::toggle_region_mute)));
	region_mute_item = static_cast<CheckMenuItem*>(&items.back());
	if (region->muted()) {
		region_mute_item->set_active();
	}
	items.push_back (CheckMenuElem (_("Opaque"), mem_fun(*this, &Editor::toggle_region_opaque)));
	region_opaque_item = static_cast<CheckMenuItem*>(&items.back());
	if (region->opaque()) {
		region_opaque_item->set_active();
	}

	items.push_back (CheckMenuElem (_("Original position"), mem_fun(*this, &Editor::naturalize)));
	if (region->at_natural_position()) {
		items.back().set_sensitive (false);
	}

	items.push_back (SeparatorElem());

	if (ar) {
		
		RegionView* rv = sv->find_view (ar);
		AudioRegionView* arv = dynamic_cast<AudioRegionView*>(rv);

		items.push_back (MenuElem (_("Reset Envelope"), mem_fun(*this, &Editor::reset_region_gain_envelopes)));
		
		items.push_back (CheckMenuElem (_("Envelope Visible"), mem_fun(*this, &Editor::toggle_gain_envelope_visibility)));
		region_envelope_visible_item = static_cast<CheckMenuItem*> (&items.back());

		if (arv->envelope_visible()) {
			region_envelope_visible_item->set_active (true);
		}

		items.push_back (CheckMenuElem (_("Envelope Active"), mem_fun(*this, &Editor::toggle_gain_envelope_active)));
		region_envelope_active_item = static_cast<CheckMenuItem*> (&items.back());

		if (ar->envelope_active()) {
			region_envelope_active_item->set_active (true);
		}

		items.push_back (SeparatorElem());

		if (ar->scale_amplitude() != 1.0f) {
			items.push_back (MenuElem (_("DeNormalize"), mem_fun(*this, &Editor::denormalize_region)));
		} else {
			items.push_back (MenuElem (_("Normalize"), mem_fun(*this, &Editor::normalize_region)));
		}
	}
	items.push_back (MenuElem (_("Reverse"), mem_fun(*this, &Editor::reverse_region)));
	items.push_back (SeparatorElem());


	/* range related stuff */

	items.push_back (MenuElem (_("Add Range Markers"), mem_fun (*this, &Editor::add_location_from_audio_region)));
	items.push_back (MenuElem (_("Set Range Selection"), mem_fun (*this, &Editor::set_selection_from_audio_region)));
	items.push_back (SeparatorElem());
			 
	/* Nudge region */

	Menu *nudge_menu = manage (new Menu());
	MenuList& nudge_items = nudge_menu->items();
	nudge_menu->set_name ("ArdourContextMenu");
	
	nudge_items.push_back (MenuElem (_("Nudge fwd"), (bind (mem_fun(*this, &Editor::nudge_forward), false))));
	nudge_items.push_back (MenuElem (_("Nudge bwd"), (bind (mem_fun(*this, &Editor::nudge_backward), false))));
	nudge_items.push_back (MenuElem (_("Nudge fwd by capture offset"), (mem_fun(*this, &Editor::nudge_forward_capture_offset))));
	nudge_items.push_back (MenuElem (_("Nudge bwd by capture offset"), (mem_fun(*this, &Editor::nudge_backward_capture_offset))));

	items.push_back (MenuElem (_("Nudge"), *nudge_menu));
	items.push_back (SeparatorElem());

	Menu *trim_menu = manage (new Menu);
	MenuList& trim_items = trim_menu->items();
	trim_menu->set_name ("ArdourContextMenu");
	
	trim_items.push_back (MenuElem (_("Start to edit cursor"), mem_fun(*this, &Editor::trim_region_from_edit_cursor)));
	trim_items.push_back (MenuElem (_("Edit cursor to end"), mem_fun(*this, &Editor::trim_region_to_edit_cursor)));
			     
	items.push_back (MenuElem (_("Trim"), *trim_menu));
	items.push_back (SeparatorElem());

	items.push_back (MenuElem (_("Split"), (mem_fun(*this, &Editor::split_region))));
	region_edit_menu_split_item = &items.back();

	items.push_back (MenuElem (_("Make mono regions"), (mem_fun(*this, &Editor::split_multichannel_region))));
	region_edit_menu_split_multichannel_item = &items.back();

	items.push_back (MenuElem (_("Duplicate"), (bind (mem_fun(*this, &Editor::duplicate_dialog), true))));
	items.push_back (MenuElem (_("Fill Track"), (mem_fun(*this, &Editor::region_fill_track))));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Remove"), mem_fun(*this, &Editor::remove_clicked_region)));

	/* OK, stick the region submenu at the top of the list, and then add
	   the standard items.
	*/

	/* we have to hack up the region name because "_" has a special
	   meaning for menu titles.
	*/

	string::size_type pos = 0;
	string menu_item_name = region->name();

	while ((pos = menu_item_name.find ("_", pos)) != string::npos) {
		menu_item_name.replace (pos, 1, "__");
		pos += 2;
	}
	
	edit_items.push_back (MenuElem (menu_item_name, *region_menu));
	edit_items.push_back (SeparatorElem());
}

void
Editor::add_selection_context_items (Menu_Helpers::MenuList& edit_items)
{
	using namespace Menu_Helpers;
	Menu     *selection_menu = manage (new Menu);
	MenuList& items       = selection_menu->items();
	selection_menu->set_name ("ArdourContextMenu");

	items.push_back (MenuElem (_("Play range"), mem_fun(*this, &Editor::play_selection)));
	items.push_back (MenuElem (_("Loop range"), mem_fun(*this, &Editor::set_route_loop_selection)));

#ifdef FFT_ANALYSIS
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Analyze range"), mem_fun(*this, &Editor::analyze_range_selection)));
#endif
	
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Separate range to track"), mem_fun(*this, &Editor::separate_region_from_selection)));
	items.push_back (MenuElem (_("Separate range to region list"), mem_fun(*this, &Editor::new_region_from_selection)));
	
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Select all in range"), mem_fun(*this, &Editor::select_all_selectables_using_time_selection)));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Add Range Markers"), mem_fun (*this, &Editor::add_location_from_selection)));
	items.push_back (MenuElem (_("Set range to loop range"), mem_fun(*this, &Editor::set_selection_from_loop)));
	items.push_back (MenuElem (_("Set range to punch range"), mem_fun(*this, &Editor::set_selection_from_punch)));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Crop region to range"), mem_fun(*this, &Editor::crop_region_to_selection)));
	items.push_back (MenuElem (_("Fill range with region"), mem_fun(*this, &Editor::region_fill_selection)));
	items.push_back (MenuElem (_("Duplicate range"), bind (mem_fun(*this, &Editor::duplicate_dialog), false)));
	items.push_back (MenuElem (_("Create chunk from range"), mem_fun(*this, &Editor::name_selection)));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Bounce range"), mem_fun(*this, &Editor::bounce_range_selection)));
	items.push_back (MenuElem (_("Export range"), mem_fun(*this, &Editor::export_selection)));

	edit_items.push_back (MenuElem (_("Range"), *selection_menu));
	edit_items.push_back (SeparatorElem());
}

void
Editor::add_dstream_context_items (Menu_Helpers::MenuList& edit_items)
{
	using namespace Menu_Helpers;

	/* Playback */

	Menu *play_menu = manage (new Menu);
	MenuList& play_items = play_menu->items();
	play_menu->set_name ("ArdourContextMenu");
	
	play_items.push_back (MenuElem (_("Play from edit cursor"), mem_fun(*this, &Editor::play_from_edit_cursor)));
	play_items.push_back (MenuElem (_("Play from start"), mem_fun(*this, &Editor::play_from_start)));
	play_items.push_back (MenuElem (_("Play region"), mem_fun(*this, &Editor::play_selected_region)));
	play_items.push_back (SeparatorElem());
	play_items.push_back (MenuElem (_("Loop Region"), mem_fun(*this, &Editor::loop_selected_region)));
	
	edit_items.push_back (MenuElem (_("Play"), *play_menu));

	/* Selection */

	Menu *select_menu = manage (new Menu);
	MenuList& select_items = select_menu->items();
	select_menu->set_name ("ArdourContextMenu");
	
	select_items.push_back (MenuElem (_("Select All in track"), bind (mem_fun(*this, &Editor::select_all_in_track), Selection::Set)));
	select_items.push_back (MenuElem (_("Select All"), bind (mem_fun(*this, &Editor::select_all), Selection::Set)));
	select_items.push_back (MenuElem (_("Invert selection in track"), mem_fun(*this, &Editor::invert_selection_in_track)));
	select_items.push_back (MenuElem (_("Invert selection"), mem_fun(*this, &Editor::invert_selection)));
	select_items.push_back (SeparatorElem());
	select_items.push_back (MenuElem (_("Set range to loop range"), mem_fun(*this, &Editor::set_selection_from_loop)));
	select_items.push_back (MenuElem (_("Set range to punch range"), mem_fun(*this, &Editor::set_selection_from_punch)));
	select_items.push_back (SeparatorElem());
	select_items.push_back (MenuElem (_("Select all after edit cursor"), bind (mem_fun(*this, &Editor::select_all_selectables_using_cursor), edit_cursor, true)));
	select_items.push_back (MenuElem (_("Select all before edit cursor"), bind (mem_fun(*this, &Editor::select_all_selectables_using_cursor), edit_cursor, false)));
	select_items.push_back (MenuElem (_("Select all after playhead"), bind (mem_fun(*this, &Editor::select_all_selectables_using_cursor), playhead_cursor, true)));
	select_items.push_back (MenuElem (_("Select all before playhead"), bind (mem_fun(*this, &Editor::select_all_selectables_using_cursor), playhead_cursor, false)));
	select_items.push_back (MenuElem (_("Select all between cursors"), bind (mem_fun(*this, &Editor::select_all_selectables_between_cursors), playhead_cursor, edit_cursor)));
	select_items.push_back (SeparatorElem());

	edit_items.push_back (MenuElem (_("Select"), *select_menu));

	/* Cut-n-Paste */

	Menu *cutnpaste_menu = manage (new Menu);
	MenuList& cutnpaste_items = cutnpaste_menu->items();
	cutnpaste_menu->set_name ("ArdourContextMenu");
	
	cutnpaste_items.push_back (MenuElem (_("Cut"), mem_fun(*this, &Editor::cut)));
	cutnpaste_items.push_back (MenuElem (_("Copy"), mem_fun(*this, &Editor::copy)));
	cutnpaste_items.push_back (MenuElem (_("Paste at edit cursor"), bind (mem_fun(*this, &Editor::paste), 1.0f)));
	cutnpaste_items.push_back (MenuElem (_("Paste at mouse"), mem_fun(*this, &Editor::mouse_paste)));

	cutnpaste_items.push_back (SeparatorElem());

	cutnpaste_items.push_back (MenuElem (_("Align"), bind (mem_fun(*this, &Editor::align), ARDOUR::SyncPoint)));
	cutnpaste_items.push_back (MenuElem (_("Align Relative"), bind (mem_fun(*this, &Editor::align_relative), ARDOUR::SyncPoint)));

	cutnpaste_items.push_back (SeparatorElem());

	cutnpaste_items.push_back (MenuElem (_("Insert chunk"), bind (mem_fun(*this, &Editor::paste_named_selection), 1.0f)));

	edit_items.push_back (MenuElem (_("Edit"), *cutnpaste_menu));

	/* Adding new material */
	
	edit_items.push_back (SeparatorElem());
	edit_items.push_back (MenuElem (_("Insert Selected Region"), bind (mem_fun(*this, &Editor::insert_region_list_selection), 1.0f)));
	edit_items.push_back (MenuElem (_("Insert Existing Audio"), bind (mem_fun(*this, &Editor::add_external_audio_action), ImportToTrack)));

	/* Nudge track */

	Menu *nudge_menu = manage (new Menu());
	MenuList& nudge_items = nudge_menu->items();
	nudge_menu->set_name ("ArdourContextMenu");
	
	edit_items.push_back (SeparatorElem());
	nudge_items.push_back (MenuElem (_("Nudge entire track fwd"), (bind (mem_fun(*this, &Editor::nudge_track), false, true))));
	nudge_items.push_back (MenuElem (_("Nudge track after edit cursor fwd"), (bind (mem_fun(*this, &Editor::nudge_track), true, true))));
	nudge_items.push_back (MenuElem (_("Nudge entire track bwd"), (bind (mem_fun(*this, &Editor::nudge_track), false, false))));
	nudge_items.push_back (MenuElem (_("Nudge track after edit cursor bwd"), (bind (mem_fun(*this, &Editor::nudge_track), true, false))));

	edit_items.push_back (MenuElem (_("Nudge"), *nudge_menu));
}

void
Editor::add_bus_context_items (Menu_Helpers::MenuList& edit_items)
{
	using namespace Menu_Helpers;

	/* Playback */

	Menu *play_menu = manage (new Menu);
	MenuList& play_items = play_menu->items();
	play_menu->set_name ("ArdourContextMenu");
	
	play_items.push_back (MenuElem (_("Play from edit cursor"), mem_fun(*this, &Editor::play_from_edit_cursor)));
	play_items.push_back (MenuElem (_("Play from start"), mem_fun(*this, &Editor::play_from_start)));
	edit_items.push_back (MenuElem (_("Play"), *play_menu));

	/* Selection */

	Menu *select_menu = manage (new Menu);
	MenuList& select_items = select_menu->items();
	select_menu->set_name ("ArdourContextMenu");
	
	select_items.push_back (MenuElem (_("Select All in track"), bind (mem_fun(*this, &Editor::select_all_in_track), Selection::Set)));
	select_items.push_back (MenuElem (_("Select All"), bind (mem_fun(*this, &Editor::select_all), Selection::Set)));
	select_items.push_back (MenuElem (_("Invert selection in track"), mem_fun(*this, &Editor::invert_selection_in_track)));
	select_items.push_back (MenuElem (_("Invert selection"), mem_fun(*this, &Editor::invert_selection)));
	select_items.push_back (SeparatorElem());
	select_items.push_back (MenuElem (_("Select all after edit cursor"), bind (mem_fun(*this, &Editor::select_all_selectables_using_cursor), edit_cursor, true)));
	select_items.push_back (MenuElem (_("Select all before edit cursor"), bind (mem_fun(*this, &Editor::select_all_selectables_using_cursor), edit_cursor, false)));
	select_items.push_back (MenuElem (_("Select all after playhead"), bind (mem_fun(*this, &Editor::select_all_selectables_using_cursor), playhead_cursor, true)));
	select_items.push_back (MenuElem (_("Select all before playhead"), bind (mem_fun(*this, &Editor::select_all_selectables_using_cursor), playhead_cursor, false)));

	edit_items.push_back (MenuElem (_("Select"), *select_menu));

	/* Cut-n-Paste */

	Menu *cutnpaste_menu = manage (new Menu);
	MenuList& cutnpaste_items = cutnpaste_menu->items();
	cutnpaste_menu->set_name ("ArdourContextMenu");
	
	cutnpaste_items.push_back (MenuElem (_("Cut"), mem_fun(*this, &Editor::cut)));
	cutnpaste_items.push_back (MenuElem (_("Copy"), mem_fun(*this, &Editor::copy)));
	cutnpaste_items.push_back (MenuElem (_("Paste"), bind (mem_fun(*this, &Editor::paste), 1.0f)));

	Menu *nudge_menu = manage (new Menu());
	MenuList& nudge_items = nudge_menu->items();
	nudge_menu->set_name ("ArdourContextMenu");
	
	edit_items.push_back (SeparatorElem());
	nudge_items.push_back (MenuElem (_("Nudge entire track fwd"), (bind (mem_fun(*this, &Editor::nudge_track), false, true))));
	nudge_items.push_back (MenuElem (_("Nudge track after edit cursor fwd"), (bind (mem_fun(*this, &Editor::nudge_track), true, true))));
	nudge_items.push_back (MenuElem (_("Nudge entire track bwd"), (bind (mem_fun(*this, &Editor::nudge_track), false, false))));
	nudge_items.push_back (MenuElem (_("Nudge track after edit cursor bwd"), (bind (mem_fun(*this, &Editor::nudge_track), true, false))));

	edit_items.push_back (MenuElem (_("Nudge"), *nudge_menu));
}

/* CURSOR SETTING AND MARKS AND STUFF */

void
Editor::set_snap_to (SnapType st)
{	
	snap_type = st;
	string str = snap_type_strings[(int) st];

	if (str != snap_type_selector.get_active_text()) {
		snap_type_selector.set_active_text (str);
	}

	instant_save ();

	switch (snap_type) {
	case SnapToAThirtysecondBeat:
	case SnapToASixteenthBeat:
	case SnapToAEighthBeat:
	case SnapToAQuarterBeat:
	case SnapToAThirdBeat:
                update_tempo_based_rulers ();
	default:
		/* relax */
		break;
    }
}

void
Editor::set_snap_mode (SnapMode mode)
{
	snap_mode = mode;
	string str = snap_mode_strings[(int)mode];

	if (str != snap_mode_selector.get_active_text ()) {
		snap_mode_selector.set_active_text (str);
	}

	instant_save ();
}

int
Editor::set_state (const XMLNode& node)
{
	const XMLProperty* prop;
	XMLNode* geometry;
	int x, y, xoff, yoff;
	Gdk::Geometry g;

	if ((prop = node.property ("id")) != 0) {
		_id = prop->value ();
	}

	if ((geometry = find_named_node (node, "geometry")) == 0) {

		g.base_width = default_width;
		g.base_height = default_height;
		x = 1;
		y = 1;
		xoff = 0;
		yoff = 21;

	} else {

		g.base_width = atoi(geometry->property("x_size")->value());
		g.base_height = atoi(geometry->property("y_size")->value());
		x = atoi(geometry->property("x_pos")->value());
		y = atoi(geometry->property("y_pos")->value());
		xoff = atoi(geometry->property("x_off")->value());
		yoff = atoi(geometry->property("y_off")->value());
	}

	set_default_size (g.base_width, g.base_height);
	move (x, y);

	if (session && (prop = node.property ("playhead"))) {
		nframes_t pos = atol (prop->value().c_str());
		playhead_cursor->set_position (pos);
	} else {
		playhead_cursor->set_position (0);

		/* reset_x_origin() doesn't work right here, since the old
		   position may be zero already, and it does nothing in such
		   circumstances.
		*/
		
		leftmost_frame = 0;
		horizontal_adjustment.set_value (0);
	}

	if (session && (prop = node.property ("edit-cursor"))) {
		nframes_t pos = atol (prop->value().c_str());
		edit_cursor->set_position (pos);
	} else {
		edit_cursor->set_position (0);
	}

	if ((prop = node.property ("zoom-focus"))) {
		set_zoom_focus ((ZoomFocus) atoi (prop->value()));
	}

	if ((prop = node.property ("zoom"))) {
		reset_zoom (PBD::atof (prop->value()));
	}

	if ((prop = node.property ("snap-to"))) {
		set_snap_to ((SnapType) atoi (prop->value()));
	}

	if ((prop = node.property ("snap-mode"))) {
		set_snap_mode ((SnapMode) atoi (prop->value()));
	}

	if ((prop = node.property ("mouse-mode"))) {
		MouseMode m = str2mousemode(prop->value());
		mouse_mode = MouseMode ((int) m + 1); /* lie, force mode switch */
		set_mouse_mode (m, true);
	} else {
		mouse_mode = MouseGain; /* lie, to force the mode switch */
		set_mouse_mode (MouseObject, true);
	}

	if ((prop = node.property ("show-waveforms"))) {
		bool yn = (prop->value() == "yes");
		_show_waveforms = !yn;
		RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("ToggleWaveformVisibility"));
		if (act) {
			RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic(act);
			/* do it twice to force the change */
			tact->set_active (!yn);
			tact->set_active (yn);
		}
	}

	if ((prop = node.property ("show-waveforms-recording"))) {
		bool yn = (prop->value() == "yes");
		_show_waveforms_recording = !yn;
		RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("ToggleWaveformsWhileRecording"));
		if (act) {
			RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic(act);
			/* do it twice to force the change */
			tact->set_active (!yn);
			tact->set_active (yn);
		}
	}
	
	if ((prop = node.property ("show-measures"))) {
		bool yn = (prop->value() == "yes");
		_show_measures = !yn;
		RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("ToggleMeasureVisibility"));
		if (act) {
			RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic(act);
			/* do it twice to force the change */
			tact->set_active (!yn);
			tact->set_active (yn);
		}
	}

	if ((prop = node.property ("follow-playhead"))) {
		bool yn = (prop->value() == "yes");
		RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("toggle-follow-playhead"));
		if (act) {
			RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic(act);
			/* do it twice to force the change */
			tact->set_active (!yn);
			tact->set_active (yn);
		}
	}


	if ((prop = node.property ("region-list-sort-type"))) {
		region_list_sort_type = (Editing::RegionListSortType) -1; // force change 
		reset_region_list_sort_type(str2regionlistsorttype(prop->value()));
	}

	if ((prop = node.property ("xfades-visible"))) {
		bool yn = (prop->value() == "yes");
		_xfade_visibility = !yn;
		// set_xfade_visibility (yn);
	}

	if ((prop = node.property ("show-editor-mixer"))) {

		Glib::RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("show-editor-mixer"));
		if (act) {

			Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
			bool yn = (prop->value() == X_("yes"));

			/* do it twice to force the change */
			
			tact->set_active (!yn);
			tact->set_active (yn);
		}
	}


	return 0;
}

XMLNode&
Editor::get_state ()
{
	XMLNode* node = new XMLNode ("Editor");
	char buf[32];

	_id.print (buf, sizeof (buf));
	node->add_property ("id", buf);
	
	if (is_realized()) {
		Glib::RefPtr<Gdk::Window> win = get_window();
		
		int x, y, xoff, yoff, width, height;
		win->get_root_origin(x, y);
		win->get_position(xoff, yoff);
		win->get_size(width, height);
		
		XMLNode* geometry = new XMLNode ("geometry");

		snprintf(buf, sizeof(buf), "%d", width);
		geometry->add_property("x_size", string(buf));
		snprintf(buf, sizeof(buf), "%d", height);
		geometry->add_property("y_size", string(buf));
		snprintf(buf, sizeof(buf), "%d", x);
		geometry->add_property("x_pos", string(buf));
		snprintf(buf, sizeof(buf), "%d", y);
		geometry->add_property("y_pos", string(buf));
		snprintf(buf, sizeof(buf), "%d", xoff);
		geometry->add_property("x_off", string(buf));
		snprintf(buf, sizeof(buf), "%d", yoff);
		geometry->add_property("y_off", string(buf));
		snprintf(buf,sizeof(buf), "%d",gtk_paned_get_position (static_cast<Paned*>(&edit_pane)->gobj()));
		geometry->add_property("edit_pane_pos", string(buf));

		node->add_child_nocopy (*geometry);
	}

	snprintf (buf, sizeof(buf), "%d", (int) zoom_focus);
	node->add_property ("zoom-focus", buf);
	snprintf (buf, sizeof(buf), "%f", frames_per_unit);
	node->add_property ("zoom", buf);
	snprintf (buf, sizeof(buf), "%d", (int) snap_type);
	node->add_property ("snap-to", buf);
	snprintf (buf, sizeof(buf), "%d", (int) snap_mode);
	node->add_property ("snap-mode", buf);

	snprintf (buf, sizeof (buf), "%" PRIu32, playhead_cursor->current_frame);
	node->add_property ("playhead", buf);
	snprintf (buf, sizeof (buf), "%" PRIu32, edit_cursor->current_frame);
	node->add_property ("edit-cursor", buf);

	node->add_property ("show-waveforms", _show_waveforms ? "yes" : "no");
	node->add_property ("show-waveforms-recording", _show_waveforms_recording ? "yes" : "no");
	node->add_property ("show-measures", _show_measures ? "yes" : "no");
	node->add_property ("follow-playhead", _follow_playhead ? "yes" : "no");
	node->add_property ("xfades-visible", _xfade_visibility ? "yes" : "no");
	node->add_property ("region-list-sort-type", enum2str(region_list_sort_type));
	node->add_property ("mouse-mode", enum2str(mouse_mode));
	
	Glib::RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("show-editor-mixer"));
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
		node->add_property (X_("show-editor-mixer"), tact->get_active() ? "yes" : "no");
	}

	return *node;
}



TimeAxisView *
Editor::trackview_by_y_position (double y)
{
	for (TrackViewList::iterator iter = track_views.begin(); iter != track_views.end(); ++iter) {

		TimeAxisView *tv;

		if ((tv = (*iter)->covers_y_position (y)) != 0) {
			return tv;
		}
	}

	return 0;
}

void
Editor::snap_to (nframes_t& start, int32_t direction, bool for_mark)
{
	Location* before = 0;
	Location* after = 0;

	if (!session) {
		return;
	}

	const nframes_t one_second = session->frame_rate();
	const nframes_t one_minute = session->frame_rate() * 60;
	const nframes_t one_smpte_second = (nframes_t)(rint(session->smpte_frames_per_second()) * session->frames_per_smpte_frame());
	nframes_t one_smpte_minute = (nframes_t)(rint(session->smpte_frames_per_second()) * session->frames_per_smpte_frame() * 60);
	nframes_t presnap = start;

	switch (snap_type) {
	case SnapToFrame:
		break;

	case SnapToCDFrame:
		if (direction) {
			start = (nframes_t) ceil ((double) start / (one_second / 75)) * (one_second / 75);
		} else {
			start = (nframes_t) floor ((double) start / (one_second / 75)) * (one_second / 75);
		}
		break;

	case SnapToSMPTEFrame:
	        if (fmod((double)start, (double)session->frames_per_smpte_frame()) > (session->frames_per_smpte_frame() / 2)) {
			start = (nframes_t) (ceil ((double) start / session->frames_per_smpte_frame()) * session->frames_per_smpte_frame());
		} else {
			start = (nframes_t) (floor ((double) start / session->frames_per_smpte_frame()) *  session->frames_per_smpte_frame());
		}
		break;

	case SnapToSMPTESeconds:
		if (session->smpte_offset_negative())
		{
			start += session->smpte_offset ();
		} else {
			start -= session->smpte_offset ();
		}    
		if (start % one_smpte_second > one_smpte_second / 2) {
			start = (nframes_t) ceil ((double) start / one_smpte_second) * one_smpte_second;
		} else {
			start = (nframes_t) floor ((double) start / one_smpte_second) * one_smpte_second;
		}
		
		if (session->smpte_offset_negative())
		{
			start -= session->smpte_offset ();
		} else {
			start += session->smpte_offset ();
		}
		break;
		
	case SnapToSMPTEMinutes:
		if (session->smpte_offset_negative())
		{
			start += session->smpte_offset ();
		} else {
			start -= session->smpte_offset ();
		}
		if (start % one_smpte_minute > one_smpte_minute / 2) {
			start = (nframes_t) ceil ((double) start / one_smpte_minute) * one_smpte_minute;
		} else {
			start = (nframes_t) floor ((double) start / one_smpte_minute) * one_smpte_minute;
		}
		if (session->smpte_offset_negative())
		{
			start -= session->smpte_offset ();
		} else {
			start += session->smpte_offset ();
		}
		break;
		
	case SnapToSeconds:
		if (start % one_second > one_second / 2) {
			start = (nframes_t) ceil ((double) start / one_second) * one_second;
		} else {
			start = (nframes_t) floor ((double) start / one_second) * one_second;
		}
		break;
		
	case SnapToMinutes:
		if (start % one_minute > one_minute / 2) {
			start = (nframes_t) ceil ((double) start / one_minute) * one_minute;
		} else {
			start = (nframes_t) floor ((double) start / one_minute) * one_minute;
		}
		break;

	case SnapToBar:
		start = session->tempo_map().round_to_bar (start, direction);
		break;

	case SnapToBeat:
		start = session->tempo_map().round_to_beat (start, direction);
		break;

	case SnapToAThirtysecondBeat:
                start = session->tempo_map().round_to_beat_subdivision (start, 32);
                break;

	case SnapToASixteenthBeat:
                start = session->tempo_map().round_to_beat_subdivision (start, 16);
                break;

	case SnapToAEighthBeat:
                start = session->tempo_map().round_to_beat_subdivision (start, 8);
                break;

	case SnapToAQuarterBeat:
                start = session->tempo_map().round_to_beat_subdivision (start, 4);
                break;

        case SnapToAThirdBeat:
                start = session->tempo_map().round_to_beat_subdivision (start, 3);
                break;

	case SnapToEditCursor:
		start = edit_cursor->current_frame;
		break;

	case SnapToMark:
		if (for_mark) {
			return;
		}

		before = session->locations()->first_location_before (start);
		after = session->locations()->first_location_after (start);

		if (direction < 0) {
			if (before) {
				start = before->start();
			} else {
				start = 0;
			}
		} else if (direction > 0) {
			if (after) {
				start = after->start();
			} else {
				start = session->current_end_frame();
			}
		} else {
			if (before) {
				if (after) {
					/* find nearest of the two */
					if ((start - before->start()) < (after->start() - start)) {
						start = before->start();
					} else {
						start = after->start();
					}
				} else {
					start = before->start();
				}
			} else if (after) {
				start = after->start();
			} else {
				/* relax */
			}
		}
		break;

	case SnapToRegionStart:
	case SnapToRegionEnd:
	case SnapToRegionSync:
	case SnapToRegionBoundary:
		if (!region_boundary_cache.empty()) {
			vector<nframes_t>::iterator i;

			if (direction > 0) {
				i = std::upper_bound (region_boundary_cache.begin(), region_boundary_cache.end(), start);
			} else {
				i = std::lower_bound (region_boundary_cache.begin(), region_boundary_cache.end(), start);
			}
			
			if (i != region_boundary_cache.end()) {
				start = *i;
			} else {
				start = region_boundary_cache.back();
			}
		}
		break;
	}

	switch (snap_mode) {
	case SnapNormal:
		return;			
		
	case SnapMagnetic:
		
		if (presnap > start) {
			if (presnap > (start + unit_to_frame(snap_threshold))) {
				start = presnap;
			}
			
		} else if (presnap < start) {
			if (presnap < (start - unit_to_frame(snap_threshold))) {
				start = presnap;
			}
		}
		
	default:
		return;
		
	}
}

void
Editor::setup_toolbar ()
{
	string pixmap_path;

	const guint32 FUDGE = 18; // Combo's are stupid - they steal space from the entry for the button


	/* Mode Buttons (tool selection) */

	vector<ToggleButton *> mouse_mode_buttons;

	mouse_move_button.add (*(manage (new Image (::get_icon("tool_object")))));
	mouse_move_button.set_relief(Gtk::RELIEF_NONE);
	mouse_mode_buttons.push_back (&mouse_move_button);
	mouse_select_button.add (*(manage (new Image (get_xpm("tool_range.xpm")))));
	mouse_select_button.set_relief(Gtk::RELIEF_NONE);
	mouse_mode_buttons.push_back (&mouse_select_button);
	mouse_gain_button.add (*(manage (new Image (::get_icon("tool_gain")))));
	mouse_gain_button.set_relief(Gtk::RELIEF_NONE);
	mouse_mode_buttons.push_back (&mouse_gain_button);
	mouse_zoom_button.add (*(manage (new Image (::get_icon("tool_zoom")))));
	mouse_zoom_button.set_relief(Gtk::RELIEF_NONE);
	mouse_mode_buttons.push_back (&mouse_zoom_button);
	mouse_timefx_button.add (*(manage (new Image (::get_icon("tool_stretch")))));
	mouse_timefx_button.set_relief(Gtk::RELIEF_NONE);
	mouse_mode_buttons.push_back (&mouse_timefx_button);
	mouse_audition_button.add (*(manage (new Image (::get_icon("tool_audition")))));
	mouse_audition_button.set_relief(Gtk::RELIEF_NONE);
	mouse_mode_buttons.push_back (&mouse_audition_button);
	
	mouse_mode_button_set = new GroupedButtons (mouse_mode_buttons);

	HBox* mode_box = manage(new HBox);
	mode_box->set_border_width (2);
	mode_box->set_spacing(4);
	mouse_mode_button_box.set_spacing(1);
	mouse_mode_button_box.pack_start(mouse_move_button, true, true);
	mouse_mode_button_box.pack_start(mouse_select_button, true, true);
	mouse_mode_button_box.pack_start(mouse_zoom_button, true, true);
	mouse_mode_button_box.pack_start(mouse_gain_button, true, true);
	mouse_mode_button_box.pack_start(mouse_timefx_button, true, true);
	mouse_mode_button_box.pack_start(mouse_audition_button, true, true);
	mouse_mode_button_box.set_homogeneous(true);

	vector<string> edit_mode_strings;
	edit_mode_strings.push_back (edit_mode_to_string (Splice));
	edit_mode_strings.push_back (edit_mode_to_string (Slide));

	edit_mode_selector.set_name ("EditModeSelector");
	Gtkmm2ext::set_size_request_to_display_given_text (edit_mode_selector, longest (edit_mode_strings).c_str(), 2+FUDGE, 10);
	set_popdown_strings (edit_mode_selector, edit_mode_strings);
	edit_mode_selector.signal_changed().connect (mem_fun(*this, &Editor::edit_mode_selection_done));

	mode_box->pack_start(edit_mode_selector);
	mode_box->pack_start(mouse_mode_button_box);
	
	mouse_mode_tearoff = manage (new TearOff (*mode_box));
	mouse_mode_tearoff->set_name ("MouseModeBase");

	mouse_mode_tearoff->Detach.connect (bind (mem_fun(*this, &Editor::detach_tearoff), static_cast<Box*>(&toolbar_hbox), 
						  &mouse_mode_tearoff->tearoff_window()));
	mouse_mode_tearoff->Attach.connect (bind (mem_fun(*this, &Editor::reattach_tearoff), static_cast<Box*> (&toolbar_hbox), 
						  &mouse_mode_tearoff->tearoff_window(), 1));
	mouse_mode_tearoff->Hidden.connect (bind (mem_fun(*this, &Editor::detach_tearoff), static_cast<Box*>(&toolbar_hbox), 
						  &mouse_mode_tearoff->tearoff_window()));
	mouse_mode_tearoff->Visible.connect (bind (mem_fun(*this, &Editor::reattach_tearoff), static_cast<Box*> (&toolbar_hbox), 
						   &mouse_mode_tearoff->tearoff_window(), 1));

	mouse_move_button.set_name ("MouseModeButton");
	mouse_select_button.set_name ("MouseModeButton");
	mouse_gain_button.set_name ("MouseModeButton");
	mouse_zoom_button.set_name ("MouseModeButton");
	mouse_timefx_button.set_name ("MouseModeButton");
	mouse_audition_button.set_name ("MouseModeButton");

	ARDOUR_UI::instance()->tooltips().set_tip (mouse_move_button, _("Select/Move Objects"));
	ARDOUR_UI::instance()->tooltips().set_tip (mouse_select_button, _("Select/Move Ranges"));
	ARDOUR_UI::instance()->tooltips().set_tip (mouse_gain_button, _("Draw Gain Automation"));
	ARDOUR_UI::instance()->tooltips().set_tip (mouse_zoom_button, _("Select Zoom Range"));
	ARDOUR_UI::instance()->tooltips().set_tip (mouse_timefx_button, _("Stretch/Shrink Regions"));
	ARDOUR_UI::instance()->tooltips().set_tip (mouse_audition_button, _("Listen to Specific Regions"));

	mouse_move_button.unset_flags (CAN_FOCUS);
	mouse_select_button.unset_flags (CAN_FOCUS);
	mouse_gain_button.unset_flags (CAN_FOCUS);
	mouse_zoom_button.unset_flags (CAN_FOCUS);
	mouse_timefx_button.unset_flags (CAN_FOCUS);
	mouse_audition_button.unset_flags (CAN_FOCUS);

	mouse_select_button.signal_toggled().connect (bind (mem_fun(*this, &Editor::mouse_mode_toggled), Editing::MouseRange));
	mouse_select_button.signal_button_release_event().connect (mem_fun(*this, &Editor::mouse_select_button_release));

	mouse_move_button.signal_toggled().connect (bind (mem_fun(*this, &Editor::mouse_mode_toggled), Editing::MouseObject));
	mouse_gain_button.signal_toggled().connect (bind (mem_fun(*this, &Editor::mouse_mode_toggled), Editing::MouseGain));
	mouse_zoom_button.signal_toggled().connect (bind (mem_fun(*this, &Editor::mouse_mode_toggled), Editing::MouseZoom));
	mouse_timefx_button.signal_toggled().connect (bind (mem_fun(*this, &Editor::mouse_mode_toggled), Editing::MouseTimeFX));
	mouse_audition_button.signal_toggled().connect (bind (mem_fun(*this, &Editor::mouse_mode_toggled), Editing::MouseAudition));

	// mouse_move_button.set_active (true);
	

	/* Zoom */
	
	zoom_box.set_spacing (1);
	zoom_box.set_border_width (2);

	zoom_in_button.set_name ("EditorTimeButton");
	zoom_in_button.set_size_request(-1,16);
	zoom_in_button.add (*(manage (new Image (::get_icon("zoom_in")))));
	zoom_in_button.signal_clicked().connect (bind (mem_fun(*this, &Editor::temporal_zoom_step), false));
	ARDOUR_UI::instance()->tooltips().set_tip (zoom_in_button, _("Zoom In"));
	
	zoom_out_button.set_name ("EditorTimeButton");
	zoom_out_button.set_size_request(-1,16);
	zoom_out_button.add (*(manage (new Image (::get_icon("zoom_out")))));
	zoom_out_button.signal_clicked().connect (bind (mem_fun(*this, &Editor::temporal_zoom_step), true));
	ARDOUR_UI::instance()->tooltips().set_tip (zoom_out_button, _("Zoom Out"));

	zoom_out_full_button.set_name ("EditorTimeButton");
	zoom_out_full_button.set_size_request(-1,16);
	zoom_out_full_button.add (*(manage (new Image (::get_icon("zoom_full")))));
	zoom_out_full_button.signal_clicked().connect (mem_fun(*this, &Editor::temporal_zoom_session));
	ARDOUR_UI::instance()->tooltips().set_tip (zoom_out_full_button, _("Zoom to Session"));
	
	zoom_focus_selector.set_name ("ZoomFocusSelector");
	Gtkmm2ext::set_size_request_to_display_given_text (zoom_focus_selector, "Edit Cursor", FUDGE, 0);
	set_popdown_strings (zoom_focus_selector, zoom_focus_strings);
	zoom_focus_selector.signal_changed().connect (mem_fun(*this, &Editor::zoom_focus_selection_done));
	ARDOUR_UI::instance()->tooltips().set_tip (zoom_focus_selector, _("Zoom focus"));

	zoom_box.pack_start (zoom_focus_selector, true, true);
	zoom_box.pack_start (zoom_out_button, false, false);
	zoom_box.pack_start (zoom_in_button, false, false);
	zoom_box.pack_start (zoom_out_full_button, false, false);

	/* Edit Cursor / Snap */

	snap_box.set_spacing (1);
	snap_box.set_border_width (2);

	snap_type_selector.set_name ("SnapTypeSelector");
	Gtkmm2ext::set_size_request_to_display_given_text (snap_type_selector, "SMPTE Seconds", 2+FUDGE, 10);
	set_popdown_strings (snap_type_selector, snap_type_strings);
	snap_type_selector.signal_changed().connect (mem_fun(*this, &Editor::snap_type_selection_done));
	ARDOUR_UI::instance()->tooltips().set_tip (snap_type_selector, _("Unit to snap cursors and ranges to"));

	snap_mode_selector.set_name ("SnapModeSelector");
	Gtkmm2ext::set_size_request_to_display_given_text (snap_mode_selector, "Magnetic Snap", 2+FUDGE, 10);
	set_popdown_strings (snap_mode_selector, snap_mode_strings);
	snap_mode_selector.signal_changed().connect (mem_fun(*this, &Editor::snap_mode_selection_done));

	snap_box.pack_start (edit_cursor_clock, false, false);
	snap_box.pack_start (snap_mode_selector, false, false);
	snap_box.pack_start (snap_type_selector, false, false);


	/* Nudge */

	HBox *nudge_box = manage (new HBox);
	nudge_box->set_spacing(1);
	nudge_box->set_border_width (2);

	nudge_forward_button.signal_clicked().connect (bind (mem_fun(*this, &Editor::nudge_forward), false));
	nudge_backward_button.signal_clicked().connect (bind (mem_fun(*this, &Editor::nudge_backward), false));

	nudge_box->pack_start (nudge_backward_button, false, false);
	nudge_box->pack_start (nudge_forward_button, false, false);
	nudge_box->pack_start (nudge_clock, false, false);


	/* Pack everything in... */

	HBox* hbox = new HBox;
	hbox->set_spacing(10);

	tools_tearoff = new TearOff (*hbox);
	tools_tearoff->set_name ("MouseModeBase");

	tools_tearoff->Detach.connect (bind (mem_fun(*this, &Editor::detach_tearoff), static_cast<Box*>(&toolbar_hbox), 
					     &tools_tearoff->tearoff_window()));
	tools_tearoff->Attach.connect (bind (mem_fun(*this, &Editor::reattach_tearoff), static_cast<Box*> (&toolbar_hbox), 
					     &tools_tearoff->tearoff_window(), 0));
	tools_tearoff->Hidden.connect (bind (mem_fun(*this, &Editor::detach_tearoff), static_cast<Box*>(&toolbar_hbox), 
					     &tools_tearoff->tearoff_window()));
	tools_tearoff->Visible.connect (bind (mem_fun(*this, &Editor::reattach_tearoff), static_cast<Box*> (&toolbar_hbox), 
					      &tools_tearoff->tearoff_window(), 0));

	toolbar_hbox.set_spacing (10);
	toolbar_hbox.set_border_width (1);

	toolbar_hbox.pack_start (*mouse_mode_tearoff, false, false);
	toolbar_hbox.pack_start (*tools_tearoff, false, false);

	
	hbox->pack_start (snap_box, false, false);
	// hbox->pack_start (zoom_box, false, false); 
	hbox->pack_start (*nudge_box, false, false);

	hbox->show_all ();
	
	toolbar_base.set_name ("ToolBarBase");
	toolbar_base.add (toolbar_hbox);

	toolbar_frame.set_shadow_type (SHADOW_OUT);
	toolbar_frame.set_name ("BaseFrame");
	toolbar_frame.add (toolbar_base);
}

int
Editor::convert_drop_to_paths (vector<ustring>& paths, 
			       const RefPtr<Gdk::DragContext>& context,
			       gint                x,
			       gint                y,
			       const SelectionData& data,
			       guint               info,
			       guint               time)			       

{	
	if (session == 0) {
		return -1;
	}

	vector<ustring> uris = data.get_uris();

	if (uris.empty()) {
		
		/* This is seriously fucked up. Nautilus doesn't say that its URI lists
		   are actually URI lists. So do it by hand.
		*/

		if (data.get_target() != "text/plain") {
			return -1;
		}
  
		/* Parse the "uri-list" format that Nautilus provides, 
		   where each pathname is delimited by \r\n
		*/
	
		const char* p = data.get_text().c_str();
		const char* q;

		while (p)
		{
			if (*p != '#')
			{
				while (g_ascii_isspace (*p))
					p++;
				
				q = p;
				while (*q && (*q != '\n') && (*q != '\r'))
					q++;
				
				if (q > p)
				{
					q--;
					while (q > p && g_ascii_isspace (*q))
						q--;
					
					if (q > p)
					{
						uris.push_back (ustring (p, q - p + 1));
					}
				}
			}
			p = strchr (p, '\n');
			if (p)
				p++;
		}

		if (uris.empty()) {
			return -1;
		}
	}
	
	for (vector<ustring>::iterator i = uris.begin(); i != uris.end(); ++i) {
		if ((*i).substr (0,7) == "file://") {
			string p = *i;
                        PBD::url_decode (p);
			paths.push_back (p.substr (7));
		}
	}

	return 0;
}

void
Editor::new_tempo_section ()

{
}

void
Editor::map_transport_state ()
{
	ENSURE_GUI_THREAD (mem_fun(*this, &Editor::map_transport_state));

	if (session->transport_stopped()) {
		have_pending_keyboard_selection = false;
	}
}

/* UNDO/REDO */

Editor::State::State ()
{
	selection = new Selection;
}

Editor::State::~State ()
{
	delete selection;
}

UndoAction
Editor::get_memento () const
{
	State *state = new State;

	store_state (*state);
	return bind (mem_fun (*(const_cast<Editor*>(this)), &Editor::restore_state), state);
}

void
Editor::store_state (State& state) const
{
	*state.selection = *selection;
}

void
Editor::restore_state (State *state)
{
	if (*selection == *state->selection) {
		return;
	}

	*selection = *state->selection;
	time_selection_changed ();
	region_selection_changed ();   

	/* XXX other selection change handlers? */
}

void
Editor::begin_reversible_command (string name)
{
	if (session) {
                before = &get_state();
		session->begin_reversible_command (name);
	}
}

void
Editor::commit_reversible_command ()
{
	if (session) {
		session->commit_reversible_command (new MementoCommand<Editor>(*this, before, &get_state()));
	}
}

struct TrackViewByPositionSorter
{
    bool operator() (const TimeAxisView* a, const TimeAxisView *b) {
	    return a->y_position < b->y_position;
    }
};

bool
Editor::extend_selection_to_track (TimeAxisView& view)
{
	if (selection->selected (&view)) {
		/* already selected, do nothing */
		return false;
	}

	if (selection->tracks.empty()) {

		if (!selection->selected (&view)) {
			selection->set (&view);
			return true;
		} else {
			return false;
		}
	} 

	/* something is already selected, so figure out which range of things to add */
	
	TrackViewList to_be_added;
	TrackViewList sorted = track_views;
	TrackViewByPositionSorter cmp;
	bool passed_clicked = false;
	bool forwards;

	sorted.sort (cmp);

	if (!selection->selected (&view)) {
		to_be_added.push_back (&view);
	}

	/* figure out if we should go forward or backwards */

	for (TrackViewList::iterator i = sorted.begin(); i != sorted.end(); ++i) {

		if ((*i) == &view) {
			passed_clicked = true;
		}

		if (selection->selected (*i)) {
			if (passed_clicked) {
				forwards = true;
			} else {
				forwards = false;
			}
			break;
		}
	}
			
	passed_clicked = false;

	if (forwards) {

		for (TrackViewList::iterator i = sorted.begin(); i != sorted.end(); ++i) {
					
			if ((*i) == &view) {
				passed_clicked = true;
				continue;
			}
					
			if (passed_clicked) {
				if ((*i)->hidden()) {
					continue;
				}
				if (selection->selected (*i)) {
					break;
				} else if (!(*i)->hidden()) {
					to_be_added.push_back (*i);
				}
			}
		}

	} else {

		for (TrackViewList::reverse_iterator r = sorted.rbegin(); r != sorted.rend(); ++r) {
					
			if ((*r) == &view) {
				passed_clicked = true;
				continue;
			}
					
			if (passed_clicked) {
						
				if ((*r)->hidden()) {
					continue;
				}
						
				if (selection->selected (*r)) {
					break;
				} else if (!(*r)->hidden()) {
					to_be_added.push_back (*r);
				}
			}
		}
	}
			
	if (!to_be_added.empty()) {
		selection->add (to_be_added);
		return true;
	}
	
	return false;
}


bool
Editor::set_selected_track (TimeAxisView& view, Selection::Operation op, bool no_remove)
{
	bool commit = false;

	switch (op) {
	case Selection::Toggle:
		if (selection->selected (&view)) {
			if (!no_remove) {
				selection->remove (&view);
				commit = true;
			}
		} else {
			selection->add (&view);
			commit = false;
		}
		break;

	case Selection::Add:
		if (!selection->selected (&view)) {
			selection->add (&view);
			commit = true;
		}
		break;

	case Selection::Set:
		if (selection->selected (&view) && selection->tracks.size() == 1) {
			/* no commit necessary */
		} else {
			selection->set (&view);
			commit = true;
		}
		break;
		
	case Selection::Extend:
		commit = extend_selection_to_track (view);
		break;
	}

	return commit;
}

bool
Editor::set_selected_track_from_click (Selection::Operation op, bool no_remove)
{
	if (!clicked_routeview) {
		return false;
	}
	
	return set_selected_track (*clicked_routeview, op, no_remove);
}

bool
Editor::set_selected_control_point_from_click (Selection::Operation op, bool no_remove)
{
	if (!clicked_control_point) {
		return false;
	}

	/* select this point and any others that it represents */

	double y1, y2;
	nframes_t x1, x2;

	x1 = pixel_to_frame (clicked_control_point->get_x() - 10);
	x2 = pixel_to_frame (clicked_control_point->get_x() + 10);
 	y1 = clicked_control_point->get_x() - 10;
	y2 = clicked_control_point->get_y() + 10;

	return select_all_within (x1, x2, y1, y2, op);
}

void
Editor::get_relevant_tracks (set<RouteTimeAxisView*>& relevant_tracks)
{
	/* step one: get all selected tracks and all tracks in the relevant edit groups */

	for (TrackSelection::iterator ti = selection->tracks.begin(); ti != selection->tracks.end(); ++ti) {

		RouteTimeAxisView* atv = dynamic_cast<RouteTimeAxisView*>(*ti);

		if (!atv) {
			continue;
		}

		RouteGroup* group = atv->route()->edit_group();

		if (group && group->is_active()) {
			
			/* active group for this track, loop over all tracks and get every member of the group */

			for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
				
				RouteTimeAxisView* tatv;
				
				if ((tatv = dynamic_cast<RouteTimeAxisView*> (*i)) != 0) {
					
					if (tatv->route()->edit_group() == group) {
						relevant_tracks.insert (tatv);
					}
				}
			}
		} else {
			relevant_tracks.insert (atv);
		}
	}
}

void
Editor::mapover_tracks (slot<void,RouteTimeAxisView&,uint32_t> sl)
{
	set<RouteTimeAxisView*> relevant_tracks;

	get_relevant_tracks (relevant_tracks);

	uint32_t sz = relevant_tracks.size();

	for (set<RouteTimeAxisView*>::iterator ati = relevant_tracks.begin(); ati != relevant_tracks.end(); ++ati) {
		sl (**ati, sz);
	}
}

void
Editor::mapped_set_selected_regionview_from_click (RouteTimeAxisView& tv, uint32_t ignored, 
						   RegionView* basis, vector<RegionView*>* all_equivs)
{
	boost::shared_ptr<Playlist> pl;
	vector<boost::shared_ptr<Region> > results;
	RegionView* marv;
	boost::shared_ptr<Diskstream> ds;

	if ((ds = tv.get_diskstream()) == 0) {
		/* bus */
		return;
	}

	if (&tv == &basis->get_time_axis_view()) {
		/* looking in same track as the original */
		return;
	}

	
	if ((pl = ds->playlist()) != 0) {
		pl->get_equivalent_regions (basis->region(), results);
	}
	
	for (vector<boost::shared_ptr<Region> >::iterator ir = results.begin(); ir != results.end(); ++ir) {
		if ((marv = tv.view()->find_view (*ir)) != 0) {
			all_equivs->push_back (marv);
		}
	}
}

bool
Editor::set_selected_regionview_from_click (bool press, Selection::Operation op, bool no_track_remove)
{
	vector<RegionView*> all_equivalent_regions;
	bool commit = false;

	if (!clicked_regionview || !clicked_routeview) {
		return false;
	}

	if (op == Selection::Toggle || op == Selection::Set) {
		
		mapover_tracks (bind (mem_fun (*this, &Editor::mapped_set_selected_regionview_from_click), 
					    clicked_regionview, &all_equivalent_regions));
		
		
		/* add clicked regionview since we skipped all other regions in the same track as the one it was in */
		
		all_equivalent_regions.push_back (clicked_regionview);
		
		switch (op) {
		case Selection::Toggle:
			
			if (clicked_regionview->get_selected()) {
				if (press) {

					/* whatever was clicked was selected already; do nothing here but allow
					   the button release to deselect it
					*/

					button_release_can_deselect = true;

				} else {

					if (button_release_can_deselect) {

						/* just remove this one region, but only on a permitted button release */

						selection->remove (clicked_regionview);
						commit = true;

						/* no more deselect action on button release till a new press
						   finds an already selected object.
						*/

						button_release_can_deselect = false;
					}
				} 

			} else {

				if (press) {
					/* add all the equivalent regions, but only on button press */
					
					if (!all_equivalent_regions.empty()) {
						commit = true;
					}
					
					for (vector<RegionView*>::iterator i = all_equivalent_regions.begin(); i != all_equivalent_regions.end(); ++i) {
						selection->add (*i);
					}
				} 
			}
			break;
			
		case Selection::Set:
			if (!clicked_regionview->get_selected()) {
				selection->set (all_equivalent_regions);
				commit = true;
			} else {
				/* no commit necessary: clicked on an already selected region */
				goto out;
			}
			break;

		default:
			/* silly compiler */
			break;
		}

	} else if (op == Selection::Extend) {

		list<Selectable*> results;
		nframes_t last_frame;
		nframes_t first_frame;

		/* 1. find the last selected regionview in the track that was clicked in */

		last_frame = 0;
		first_frame = max_frames;

		for (RegionSelection::iterator x = selection->regions.begin(); x != selection->regions.end(); ++x) {
			if (&(*x)->get_time_axis_view() == &clicked_regionview->get_time_axis_view()) {

				if ((*x)->region()->last_frame() > last_frame) {
					last_frame = (*x)->region()->last_frame();
				}

				if ((*x)->region()->first_frame() < first_frame) {
					first_frame = (*x)->region()->first_frame();
				}
			}
		}

		/* 2. figure out the boundaries for our search for new objects */

		switch (clicked_regionview->region()->coverage (first_frame, last_frame)) {
		case OverlapNone:
			if (last_frame < clicked_regionview->region()->first_frame()) {
				first_frame = last_frame;
				last_frame = clicked_regionview->region()->last_frame();
			} else {
				last_frame = first_frame;
				first_frame = clicked_regionview->region()->first_frame();
			}
			break;

		case OverlapExternal:
			if (last_frame < clicked_regionview->region()->first_frame()) {
				first_frame = last_frame;
				last_frame = clicked_regionview->region()->last_frame();
			} else {
				last_frame = first_frame;
				first_frame = clicked_regionview->region()->first_frame();
			}
			break;

		case OverlapInternal:
			if (last_frame < clicked_regionview->region()->first_frame()) {
				first_frame = last_frame;
				last_frame = clicked_regionview->region()->last_frame();
			} else {
				last_frame = first_frame;
				first_frame = clicked_regionview->region()->first_frame();
			}
			break;

		case OverlapStart:
		case OverlapEnd:
			/* nothing to do except add clicked region to selection, since it
			   overlaps with the existing selection in this track.
			*/
			break;
		}

		/* 2. find all selectable objects (regionviews in this case) between that one and the end of the
		      one that was clicked.
		*/

		set<RouteTimeAxisView*> relevant_tracks;
		
		get_relevant_tracks (relevant_tracks);
		
		for (set<RouteTimeAxisView*>::iterator t = relevant_tracks.begin(); t != relevant_tracks.end(); ++t) {
			(*t)->get_selectables (first_frame, last_frame, -1.0, -1.0, results);
		}
		
		/* 3. convert to a vector of audio regions */

		vector<RegionView*> regions;
		
		for (list<Selectable*>::iterator x = results.begin(); x != results.end(); ++x) {
			RegionView* arv;

			if ((arv = dynamic_cast<RegionView*>(*x)) != 0) {
				regions.push_back (arv);
			}
		}

		if (!regions.empty()) {
			selection->add (regions);
			commit = true;
		}
	}

  out:
	return commit;
}

void
Editor::set_selected_regionview_from_region_list (boost::shared_ptr<Region> region, Selection::Operation op)
{
	vector<RegionView*> all_equivalent_regions;

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		
		RouteTimeAxisView* tatv;
		
		if ((tatv = dynamic_cast<RouteTimeAxisView*> (*i)) != 0) {
			
			boost::shared_ptr<Playlist> pl;
			vector<boost::shared_ptr<Region> > results;
			RegionView* marv;
			boost::shared_ptr<Diskstream> ds;
			
			if ((ds = tatv->get_diskstream()) == 0) {
				/* bus */
				continue;
			}
			
			if ((pl = (ds->playlist())) != 0) {
				pl->get_region_list_equivalent_regions (region, results);
			}
			
			for (vector<boost::shared_ptr<Region> >::iterator ir = results.begin(); ir != results.end(); ++ir) {
				if ((marv = tatv->view()->find_view (*ir)) != 0) {
					all_equivalent_regions.push_back (marv);
				}
			}
			
		}
	}
	
	begin_reversible_command (_("set selected regions"));
	
	switch (op) {
	case Selection::Toggle:
		/* XXX this is not correct */
		selection->toggle (all_equivalent_regions);
		break;
	case Selection::Set:
		selection->set (all_equivalent_regions);
		break;
	case Selection::Extend:
		selection->add (all_equivalent_regions);
		break;
	case Selection::Add:
		selection->add (all_equivalent_regions);
		break;
	}

	commit_reversible_command () ;
}

bool
Editor::set_selected_regionview_from_map_event (GdkEventAny* ev, StreamView* sv, boost::weak_ptr<Region> weak_r)
{
	RegionView* rv;
	boost::shared_ptr<Region> r (weak_r.lock());

	if (!r) {
		return true;
	}

	boost::shared_ptr<AudioRegion> ar;

	if ((ar = boost::dynamic_pointer_cast<AudioRegion> (r)) == 0) {
		return true;
	}

	if ((rv = sv->find_view (ar)) == 0) {
		return true;
	}

	/* don't reset the selection if its something other than 
	   a single other region.
	*/

	if (selection->regions.size() > 1) {
		return true;
	}
	
	begin_reversible_command (_("set selected regions"));
	
	selection->set (rv);

	commit_reversible_command () ;

	return true;
}

void
Editor::set_edit_group_solo (Route& route, bool yn)
{
	RouteGroup *edit_group;

	if ((edit_group = route.edit_group()) != 0) {
		edit_group->apply (&Route::set_solo, yn, this);
	} else {
		route.set_solo (yn, this);
	}
}

void
Editor::set_edit_group_mute (Route& route, bool yn)
{
	RouteGroup *edit_group = 0;

	if ((edit_group == route.edit_group()) != 0) {
		edit_group->apply (&Route::set_mute, yn, this);
	} else {
		route.set_mute (yn, this);
	}
}
		
void
Editor::history_changed ()
{
	string label;

	if (undo_action && session) {
		if (session->undo_depth() == 0) {
			label = _("Undo");
		} else {
			label = string_compose(_("Undo (%1)"), session->next_undo());
		}
		undo_action->property_label() = label;
	}

	if (redo_action && session) {
		if (session->redo_depth() == 0) {
			label = _("Redo");
		} else {
			label = string_compose(_("Redo (%1)"), session->next_redo());
		}
		redo_action->property_label() = label;
	}
}

void
Editor::duplicate_dialog (bool dup_region)
{
	if (dup_region) {
		if (clicked_regionview == 0) {
			return;
		}
	} else {
		if (selection->time.length() == 0) {
			return;
		}
	}

	ArdourDialog win ("duplicate dialog");
	Entry  entry;
	Label  label (_("Duplicate how many times?"));

	win.get_vbox()->pack_start (label);
	win.add_action_widget (entry, RESPONSE_ACCEPT);
	win.add_button (Stock::OK, RESPONSE_ACCEPT);
	win.add_button (Stock::CANCEL, RESPONSE_CANCEL);

	win.set_position (WIN_POS_MOUSE);

	entry.set_text ("1");
	set_size_request_to_display_given_text (entry, X_("12345678"), 20, 15);
	entry.select_region (0, -1);
	entry.grab_focus ();


	switch (win.run ()) {
	case RESPONSE_ACCEPT:
		break;
	default:
		return;
	}

	string text = entry.get_text();
	float times;

	if (sscanf (text.c_str(), "%f", &times) == 1) {
		if (dup_region) {
			RegionSelection regions;
			regions.add (clicked_regionview);
			duplicate_some_regions (regions, times);
		} else {
			duplicate_selection (times);
		}
	}
}

void
Editor::show_verbose_canvas_cursor ()
{
        verbose_canvas_cursor->raise_to_top();
        verbose_canvas_cursor->show();
	verbose_cursor_visible = true;
}

void
Editor::hide_verbose_canvas_cursor ()
{
        verbose_canvas_cursor->hide();
	verbose_cursor_visible = false;
}

void
Editor::set_verbose_canvas_cursor (const string & txt, double x, double y)
{
	/* XXX get origin of canvas relative to root window,
	   add x and y and check compared to gdk_screen_{width,height}
	*/
	verbose_canvas_cursor->property_text() = txt.c_str();
	verbose_canvas_cursor->property_x() = x;
	verbose_canvas_cursor->property_y() = y;
}

void
Editor::set_verbose_canvas_cursor_text (const string & txt)
{
	verbose_canvas_cursor->property_text() = txt.c_str();
}

void
Editor::edit_mode_selection_done ()
{
	if (session == 0) {
		return;
	}

	string choice = edit_mode_selector.get_active_text();
	EditMode mode = Slide;

	if (choice == _("Splice Edit")) {
		mode = Splice;
	} else if (choice == _("Slide Edit")) {
		mode = Slide;
	}

	Config->set_edit_mode (mode);
}	

void
Editor::snap_type_selection_done ()
{
	string choice = snap_type_selector.get_active_text();
	SnapType snaptype = SnapToFrame;

	if (choice == _("Beats/3")) {
		snaptype = SnapToAThirdBeat;
	} else if (choice == _("Beats/4")) {
		snaptype = SnapToAQuarterBeat;
	} else if (choice == _("Beats/8")) {
		snaptype = SnapToAEighthBeat;
	} else if (choice == _("Beats/16")) {
		snaptype = SnapToASixteenthBeat;
	} else if (choice == _("Beats/32")) {
		snaptype = SnapToAThirtysecondBeat;
	} else if (choice == _("Beats")) {
		snaptype = SnapToBeat;
	} else if (choice == _("Bars")) {
		snaptype = SnapToBar;
	} else if (choice == _("Marks")) {
		snaptype = SnapToMark;
	} else if (choice == _("Edit Cursor")) {
		snaptype = SnapToEditCursor;
	} else if (choice == _("Region starts")) {
		snaptype = SnapToRegionStart;
	} else if (choice == _("Region ends")) {
		snaptype = SnapToRegionEnd;
	} else if (choice == _("Region bounds")) {
		snaptype = SnapToRegionBoundary;
	} else if (choice == _("Region syncs")) {
		snaptype = SnapToRegionSync;
	} else if (choice == _("CD Frames")) {
		snaptype = SnapToCDFrame;
	} else if (choice == _("SMPTE Frames")) {
		snaptype = SnapToSMPTEFrame;
	} else if (choice == _("SMPTE Seconds")) {
		snaptype = SnapToSMPTESeconds;
	} else if (choice == _("SMPTE Minutes")) {
		snaptype = SnapToSMPTEMinutes;
	} else if (choice == _("Seconds")) {
		snaptype = SnapToSeconds;
	} else if (choice == _("Minutes")) {
		snaptype = SnapToMinutes;
	} else if (choice == _("None")) {
		snaptype = SnapToFrame;
	}

	RefPtr<RadioAction> ract = snap_type_action (snaptype);
	if (ract) {
		ract->set_active ();
	}
}	

void
Editor::snap_mode_selection_done ()
{
	string choice = snap_mode_selector.get_active_text();
	SnapMode mode = SnapNormal;

	if (choice == _("Normal")) {
		mode = SnapNormal;
	} else if (choice == _("Magnetic")) {
		mode = SnapMagnetic;
	}

	RefPtr<RadioAction> ract = snap_mode_action (mode);

	if (ract) {
		ract->set_active (true);
	}
}

void
Editor::zoom_focus_selection_done ()
{
	string choice = zoom_focus_selector.get_active_text();
	ZoomFocus focus_type = ZoomFocusLeft;

	if (choice == _("Left")) {
		focus_type = ZoomFocusLeft;
	} else if (choice == _("Right")) {
		focus_type = ZoomFocusRight;
	} else if (choice == _("Center")) {
		focus_type = ZoomFocusCenter;
	} else if (choice == _("Playhead")) {
		focus_type = ZoomFocusPlayhead;
	} else if (choice == _("Edit Cursor")) {
		focus_type = ZoomFocusEdit;
	} 
	
	RefPtr<RadioAction> ract = zoom_focus_action (focus_type);

	if (ract) {
		ract->set_active ();
	}
}	

gint
Editor::edit_controls_button_release (GdkEventButton* ev)
{
	if (Keyboard::is_context_menu_event (ev)) {
		ARDOUR_UI::instance()->add_route ();
	}
	return TRUE;
}

void
Editor::track_selection_changed ()
{
	switch (selection->tracks.size()){
	case 0:
		break;
	default:
		set_selected_mixer_strip (*(selection->tracks.front()));
		break;
	}

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		(*i)->set_selected (false);
		if (mouse_mode == MouseRange) {
			(*i)->hide_selection ();
		}
	}

	for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
		(*i)->set_selected (true);
		if (mouse_mode == MouseRange) {
			(*i)->show_selection (selection->time);
		}
	}
}

void
Editor::time_selection_changed ()
{
	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		(*i)->hide_selection ();
	}

	if (selection->tracks.empty()) {
		for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
			(*i)->show_selection (selection->time);
		}
	} else {
		for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
			(*i)->show_selection (selection->time);
		}
	}

	if (selection->time.empty()) {
		ActionManager::set_sensitive (ActionManager::time_selection_sensitive_actions, false);
	} else {
		ActionManager::set_sensitive (ActionManager::time_selection_sensitive_actions, true);
	}
}

void
Editor::region_selection_changed ()
{
	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		(*i)->set_selected_regionviews (selection->regions);
	}
}

void
Editor::point_selection_changed ()
{
	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		(*i)->set_selected_points (selection->points);
	}
}

gint
Editor::mouse_select_button_release (GdkEventButton* ev)
{
	/* this handles just right-clicks */

	if (ev->button != 3) {
		return FALSE;
	}

	return TRUE;
}

Editor::TrackViewList *
Editor::get_valid_views (TimeAxisView* track, RouteGroup* group)
{
	TrackViewList *v;
	TrackViewList::iterator i;

	v = new TrackViewList;

	if (track == 0 && group == 0) {

		/* all views */

		for (i = track_views.begin(); i != track_views.end (); ++i) {
			v->push_back (*i);
		}

	} else if (track != 0 && group == 0 || (track != 0 && group != 0 && !group->is_active())) {
		
		/* just the view for this track
		 */

		v->push_back (track);

	} else {
		
		/* views for all tracks in the edit group */
		
		for (i  = track_views.begin(); i != track_views.end (); ++i) {

			if (group == 0 || (*i)->edit_group() == group) {
				v->push_back (*i);
			}
		}
	}
	
	return v;
}

void
Editor::set_zoom_focus (ZoomFocus f)
{
	string str = zoom_focus_strings[(int)f];

	if (str != zoom_focus_selector.get_active_text()) {
		zoom_focus_selector.set_active_text (str);
	}
	
	if (zoom_focus != f) {
		zoom_focus = f;

		ZoomFocusChanged (); /* EMIT_SIGNAL */

		instant_save ();
	}
}

void
Editor::ensure_float (Window& win)
{
	win.set_transient_for (*this);
}

void 
Editor::pane_allocation_handler (Allocation &alloc, Paned* which)
{
	/* recover or initialize pane positions. do this here rather than earlier because
	   we don't want the positions to change the child allocations, which they seem to do.
	 */

	int pos;
	XMLProperty* prop;
	char buf[32];
	XMLNode* node = ARDOUR_UI::instance()->editor_settings();
	int width, height;
	static int32_t done;
	XMLNode* geometry;

	if ((geometry = find_named_node (*node, "geometry")) == 0) {
		width = default_width;
		height = default_height;
	} else {
		width = atoi(geometry->property("x_size")->value());
		height = atoi(geometry->property("y_size")->value());
	}

	if (which == static_cast<Paned*> (&edit_pane)) {

		if (done) {
			return;
		}

		if (!geometry || (prop = geometry->property ("edit_pane_pos")) == 0) {
			/* initial allocation is 90% to canvas, 10% to notebook */
			pos = (int) floor (alloc.get_width() * 0.90f);
			snprintf (buf, sizeof(buf), "%d", pos);
		} else {
			pos = atoi (prop->value());
		}
		
		if ((done = GTK_WIDGET(edit_pane.gobj())->allocation.width > pos)) {
			edit_pane.set_position (pos);
			pre_maximal_pane_position = pos;
		}
	}
}

void
Editor::detach_tearoff (Box* b, Window* w)
{
	if (tools_tearoff->torn_off() && 
	    mouse_mode_tearoff->torn_off()) {
		top_hbox.remove (toolbar_frame);
	}
}

void
Editor::reattach_tearoff (Box* b, Window* w, int32_t n)
{
	if (toolbar_frame.get_parent() == 0) {
		top_hbox.pack_end (toolbar_frame);
	}
}

void
Editor::set_show_measures (bool yn)
{
	if (_show_measures != yn) {
		hide_measures ();

		if ((_show_measures = yn) == true) {
			draw_measures ();
		}
		DisplayControlChanged (ShowMeasures);
		instant_save ();
	}
}

void
Editor::toggle_follow_playhead ()
{
	RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("toggle-follow-playhead"));
	if (act) {
		RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic(act);
		set_follow_playhead (tact->get_active());
	}
}

void
Editor::set_follow_playhead (bool yn)
{
	if (_follow_playhead != yn) {
		if ((_follow_playhead = yn) == true) {
			/* catch up */
			update_current_screen ();
		}
		DisplayControlChanged (FollowPlayhead);
		instant_save ();
	}
}

void
Editor::toggle_xfade_active (boost::weak_ptr<Crossfade> wxfade)
{
	boost::shared_ptr<Crossfade> xfade (wxfade.lock());
	if (xfade) {
		xfade->set_active (!xfade->active());
	}
}

void
Editor::toggle_xfade_length (boost::weak_ptr<Crossfade> wxfade)
{
	boost::shared_ptr<Crossfade> xfade (wxfade.lock());
	if (xfade) {
		xfade->set_follow_overlap (!xfade->following_overlap());
	}
}

void
Editor::edit_xfade (boost::weak_ptr<Crossfade> wxfade)
{
	boost::shared_ptr<Crossfade> xfade (wxfade.lock());

	if (!xfade) {
		return;
	}

	CrossfadeEditor cew (*session, xfade, xfade->fade_in().get_min_y(), 1.0);
		
	ensure_float (cew);
	
	switch (cew.run ()) {
	case RESPONSE_ACCEPT:
		break;
	default:
		return;
	}
	
	cew.apply ();
	xfade->StateChanged (Change (~0));
}

PlaylistSelector&
Editor::playlist_selector () const
{
	return *_playlist_selector;
}

nframes_t
Editor::get_nudge_distance (nframes_t pos, nframes_t& next)
{
	nframes_t ret;

	ret = nudge_clock.current_duration (pos);
	next = ret + 1; /* XXXX fix me */

	return ret;
}

void
Editor::end_location_changed (Location* location)
{
	ENSURE_GUI_THREAD (bind (mem_fun(*this, &Editor::end_location_changed), location));
	reset_scrolling_region ();
}

int
Editor::playlist_deletion_dialog (boost::shared_ptr<Playlist> pl)
{
	ArdourDialog dialog ("playlist deletion dialog");
	Label  label (string_compose (_("Playlist %1 is currently unused.\n"
					"If left alone, no audio files used by it will be cleaned.\n"
					"If deleted, audio files used by it alone by will cleaned."),
				      pl->name()));
	
	dialog.set_position (WIN_POS_CENTER);
	dialog.get_vbox()->pack_start (label);

	label.show ();

	dialog.add_button (_("Delete playlist"), RESPONSE_ACCEPT);
	dialog.add_button (_("Keep playlist"), RESPONSE_CANCEL);
	dialog.add_button (_("Cancel"), RESPONSE_CANCEL);

	switch (dialog.run ()) {
	case RESPONSE_ACCEPT:
		/* delete the playlist */
		return 0;
		break;

	case RESPONSE_REJECT:
		/* keep the playlist */
		return 1;
		break;

	default:
		break;
	}

	return -1;
}

bool
Editor::audio_region_selection_covers (nframes_t where)
{
	for (RegionSelection::iterator a = selection->regions.begin(); a != selection->regions.end(); ++a) {
		if ((*a)->region()->covers (where)) {
			return true;
		}
	}

	return false;
}

void
Editor::prepare_for_cleanup ()
{
	cut_buffer->clear_regions ();
	cut_buffer->clear_playlists ();

	selection->clear_regions ();
	selection->clear_playlists ();
}

Location*
Editor::transport_loop_location()
{
	if (session) {
		return session->locations()->auto_loop_location();
	} else {
		return 0;
	}
}

Location*
Editor::transport_punch_location()
{
	if (session) {
		return session->locations()->auto_punch_location();
	} else {
		return 0;
	}
}

bool
Editor::control_layout_scroll (GdkEventScroll* ev)
{
	switch (ev->direction) {
	case GDK_SCROLL_UP:
		scroll_tracks_up_line ();
		return true;
		break;

	case GDK_SCROLL_DOWN:
		scroll_tracks_down_line ();
		return true;
		
	default:
		/* no left/right handling yet */
		break;
	}

	return false;
}

void
Editor::snapshot_display_selection_changed ()
{
	if (snapshot_display.get_selection()->count_selected_rows() > 0) {

		TreeModel::iterator i = snapshot_display.get_selection()->get_selected();
		
		Glib::ustring snap_name = (*i)[snapshot_display_columns.real_name];

		if (snap_name.length() == 0) {
			return;
		}
		
		if (session->snap_name() == snap_name) {
			return;
		}
		
		ARDOUR_UI::instance()->load_session(session->path(), string (snap_name));
	}
}

bool
Editor::snapshot_display_button_press (GdkEventButton* ev)
{
	 return false;
}

void
Editor::redisplay_snapshots ()
{
	if (session == 0) {
		return;
	}

	vector<string*>* states;

	if ((states = session->possible_states()) == 0) {
		return;
	}

	snapshot_display_model->clear ();

	for (vector<string*>::iterator i = states->begin(); i != states->end(); ++i) {
		string statename = *(*i);
		TreeModel::Row row = *(snapshot_display_model->append());
		
		// we don't have a way of changing the rendering in just one TreeView 
		// cell so just put an asterisk on each side of the name for now.
		string display_name;
		if (statename == session->snap_name()) {
			display_name = "*"+statename+"*";
			snapshot_display.get_selection()->select(row);
		} else {
			display_name = statename;
		}
		
		row[snapshot_display_columns.visible_name] = display_name;
		row[snapshot_display_columns.real_name] = statename;
	}

	delete states;
}

void
Editor::session_state_saved (string snap_name)
{
	ENSURE_GUI_THREAD (bind (mem_fun(*this, &Editor::session_state_saved), snap_name));
	redisplay_snapshots ();
}

void
Editor::maximise_editing_space ()
{
	initial_ruler_update_required = true;

	mouse_mode_tearoff->set_visible (false);
	tools_tearoff->set_visible (false);

	pre_maximal_pane_position = edit_pane.get_position();
	pre_maximal_editor_width = this->get_width();

	if(post_maximal_pane_position == 0) {
		post_maximal_pane_position = edit_pane.get_width();
	}


	fullscreen();
	if(post_maximal_editor_width) {
		edit_pane.set_position (post_maximal_pane_position - 
			abs(post_maximal_editor_width - pre_maximal_editor_width));
	} else {
		edit_pane.set_position (post_maximal_pane_position);
	}
}

void
Editor::restore_editing_space ()
{
	initial_ruler_update_required = true;

	// user changed width of pane during fullscreen
	if(post_maximal_pane_position != edit_pane.get_position()) {
		post_maximal_pane_position = edit_pane.get_position();
	}

	unfullscreen();

	mouse_mode_tearoff->set_visible (true);
	tools_tearoff->set_visible (true);
	post_maximal_editor_width = this->get_width();


	edit_pane.set_position (
		pre_maximal_pane_position + abs(this->get_width() - pre_maximal_editor_width)
	);
}

void 
Editor::new_playlists ()
{
	begin_reversible_command (_("new playlists"));
	mapover_tracks (mem_fun (*this, &Editor::mapped_use_new_playlist));
	commit_reversible_command ();
}

void
Editor::copy_playlists ()
{
	begin_reversible_command (_("copy playlists"));
	mapover_tracks (mem_fun (*this, &Editor::mapped_use_copy_playlist));
	commit_reversible_command ();
}

void 
Editor::clear_playlists ()
{
	begin_reversible_command (_("clear playlists"));
	mapover_tracks (mem_fun (*this, &Editor::mapped_clear_playlist));
	commit_reversible_command ();
}

void 
Editor::mapped_use_new_playlist (RouteTimeAxisView& atv, uint32_t sz)
{
	atv.use_new_playlist (sz > 1 ? false : true);
}

void
Editor::mapped_use_copy_playlist (RouteTimeAxisView& atv, uint32_t sz)
{
	atv.use_copy_playlist (sz > 1 ? false : true);
}

void 
Editor::mapped_clear_playlist (RouteTimeAxisView& atv, uint32_t sz)
{
	atv.clear_playlist ();
}

bool
Editor::on_key_press_event (GdkEventKey* ev)
{
	return key_press_focus_accelerator_handler (*this, ev);
}

void
Editor::reset_x_origin (nframes_t frame)
{
	queue_visual_change (frame);
}

void
Editor::reset_zoom (double fpu)
{
	queue_visual_change (fpu);
}

void
Editor::reposition_and_zoom (nframes_t frame, double fpu)
{
	reset_x_origin (frame);
	reset_zoom (fpu);
}

void
Editor::set_frames_per_unit (double fpu)
{
	nframes_t frames;

	/* this is the core function that controls the zoom level of the canvas. it is called
	   whenever one or more calls are made to reset_zoom(). it executes in an idle handler.
	*/

	if (fpu == frames_per_unit) {
		return;
	}

	if (fpu < 2.0) {
		fpu = 2.0;
	}

	// convert fpu to frame count

	frames = (nframes_t) floor (fpu * canvas_width);
	
	/* don't allow zooms that fit more than the maximum number
	   of frames into an 800 pixel wide space.
	*/

	if (max_frames / fpu < 800.0) {
		return;
	}

	if (fpu == frames_per_unit) {
		return;
	}

	frames_per_unit = fpu;

	if (frames != zoom_range_clock.current_duration()) {
		zoom_range_clock.set (frames);
	}

	if (mouse_mode == MouseRange && selection->time.start () != selection->time.end_frame ()) {
		if (!selection->tracks.empty()) {
			for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
				(*i)->reshow_selection (selection->time);
			}
		} else {
			for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
				(*i)->reshow_selection (selection->time);
			}
		}
	}

	ZoomChanged (); /* EMIT_SIGNAL */

	reset_hscrollbar_stepping ();
	reset_scrolling_region ();
	
	if (edit_cursor) edit_cursor->set_position (edit_cursor->current_frame);
	if (playhead_cursor) playhead_cursor->set_position (playhead_cursor->current_frame);

	instant_save ();
}

void 
Editor::canvas_horizontally_scrolled ()
{
	/* this is the core function that controls horizontal scrolling of the canvas. it is called
	   whenever the horizontal_adjustment emits its "value_changed" signal. it executes in an
	   idle handler.
	*/

  	leftmost_frame = (nframes_t) floor (horizontal_adjustment.get_value() * frames_per_unit);
	nframes_t rightmost_frame = leftmost_frame + current_page_frames ();
	
	if (rightmost_frame > last_canvas_frame) {
		last_canvas_frame = rightmost_frame;
		reset_scrolling_region ();
	}
	
	update_fixed_rulers ();
	tempo_map_changed (Change (0));
}

void
Editor::queue_visual_change (nframes_t where)
{
	pending_visual_change.pending = VisualChange::Type (pending_visual_change.pending | VisualChange::TimeOrigin);
	pending_visual_change.time_origin = where;

	if (pending_visual_change.idle_handler_id < 0) {
		pending_visual_change.idle_handler_id = g_idle_add (_idle_visual_changer, this);
	}
}

void
Editor::queue_visual_change (double fpu)
{
	pending_visual_change.pending = VisualChange::Type (pending_visual_change.pending | VisualChange::ZoomLevel);
	pending_visual_change.frames_per_unit = fpu;

	if (pending_visual_change.idle_handler_id < 0) {
		pending_visual_change.idle_handler_id = g_idle_add_full (G_PRIORITY_HIGH_IDLE, _idle_visual_changer, this, 0);
	}
}

int
Editor::_idle_visual_changer (void* arg)
{
	return static_cast<Editor*>(arg)->idle_visual_changer ();
}

int
Editor::idle_visual_changer ()
{
	VisualChange::Type p = pending_visual_change.pending;

	pending_visual_change.pending = (VisualChange::Type) 0;
	pending_visual_change.idle_handler_id = -1;
	
	if (p & VisualChange::ZoomLevel) {
		set_frames_per_unit (pending_visual_change.frames_per_unit);
	}

	if (p & VisualChange::TimeOrigin) {
		if (pending_visual_change.time_origin != leftmost_frame) {
			horizontal_adjustment.set_value (pending_visual_change.time_origin/frames_per_unit);
			/* the signal handler will do the rest */
		} else {
			update_fixed_rulers();
			tempo_map_changed (Change (0));
		}
	}

	return 0;
}
