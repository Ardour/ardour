/*
    Copyright (C) 2000-2007 Paul Davis 

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

#include <sys/time.h>
#include <unistd.h>
#include <cstdlib>
#include <cmath>
#include <string>
#include <algorithm>

#include <boost/none.hpp>

#include <sigc++/bind.h>

#include <pbd/convert.h>
#include <pbd/error.h>
#include <pbd/enumwriter.h>
#include <pbd/memento_command.h>

#include <glibmm/miscutils.h>
#include <gtkmm/image.h>
#include <gdkmm/color.h>
#include <gdkmm/bitmap.h>

#include <gtkmm2ext/grouped_buttons.h>
#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/tearoff.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/window_title.h>
#include <gtkmm2ext/choice.h>

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
#include <ardour/profile.h>

#include <control_protocol/control_protocol.h>

#include "ardour_ui.h"
#include "editor.h"
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
#include "canvas_impl.h"
#include "actions.h"
#include "gui_thread.h"
#include "sfdb_ui.h"

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
	N_("Edit Point"),
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

static const gchar *_edit_point_strings[] = {
	N_("Playhead"),
	N_("Marker"),
	N_("Mouse"),
	0
};

static const gchar *_zoom_focus_strings[] = {
	N_("Left"),
	N_("Right"),
	N_("Center"),
	N_("Playhead"),
 	N_("Mouse"),
 	N_("Marker"),
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

Editor::Editor ()
	: 
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

	  edit_point_clock (X_("editpoint"), false, X_("EditPointClock"), true),
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
	selection->MarkersChanged.connect (mem_fun(*this, &Editor::marker_selection_changed));

	clicked_regionview = 0;
	clicked_trackview = 0;
	clicked_audio_trackview = 0;
	clicked_crossfadeview = 0;
	clicked_control_point = 0;
	latest_regionview = 0;
	last_update_frame = 0;
	drag_info.item = 0;
	current_mixer_strip = 0;
	current_bbt_points = 0;
	
	snap_type_strings =  I18N (_snap_type_strings);
	snap_mode_strings =  I18N (_snap_mode_strings);
	zoom_focus_strings = I18N (_zoom_focus_strings);
	edit_point_strings = I18N (_edit_point_strings);

	snap_type = SnapToFrame;
	set_snap_to (snap_type);

	snap_mode = SnapNormal;
	set_snap_mode (snap_mode);

	_edit_point = EditAtMouse;
	set_edit_point_preference (_edit_point);

	snap_threshold = 5.0;
	bbt_beat_subdivision = 4;
	canvas_width = 0;
	canvas_height = 0;
	autoscroll_active = false;
	autoscroll_timeout_tag = -1;
	interthread_progress_window = 0;
	logo_item = 0;

#ifdef FFT_ANALYSIS
	analysis_window = 0;
#endif

	current_interthread_info = 0;
	_show_measures = true;
	_show_waveforms = true;
	_show_waveforms_recording = true;
	first_action_message = 0;
	export_dialog = 0;
	export_range_markers_dialog = 0;
	show_gain_after_trim = false;
	ignore_route_list_reorder = false;
	no_route_list_redisplay = false;
	verbose_cursor_on = true;
	route_removal = false;
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
	entered_marker = 0;
	clear_entered_track = false;
	_new_regionviews_show_envelope = false;
	current_timestretch = 0;
	in_edit_group_row_change = false;
	last_canvas_frame = 0;
	playhead_cursor = 0;
	button_release_can_deselect = true;
	canvas_idle_queued = false;
	_dragging_playhead = false;
	_dragging_edit_point = false;
	_dragging_hscrollbar = false;

	scrubbing_direction = 0;

	sfbrowser = 0;
	ignore_route_order_sync = false;

	location_marker_color = ARDOUR_UI::config()->canvasvar_LocationMarker.get();
	location_range_color = ARDOUR_UI::config()->canvasvar_LocationRange.get();
	location_cd_marker_color = ARDOUR_UI::config()->canvasvar_LocationCDMarker.get();
	location_loop_color = ARDOUR_UI::config()->canvasvar_LocationLoop.get();
	location_punch_color = ARDOUR_UI::config()->canvasvar_LocationPunch.get();

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
	vertical_adjustment.signal_value_changed().connect (mem_fun(*this, &Editor::tie_vertical_scrolling), true);
	
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

 	edit_hscrollbar.signal_button_press_event().connect (mem_fun(*this, &Editor::hscrollbar_button_press), false);
 	edit_hscrollbar.signal_button_release_event().connect (mem_fun(*this, &Editor::hscrollbar_button_release), false);
 	edit_hscrollbar.signal_size_allocate().connect (mem_fun(*this, &Editor::hscrollbar_allocate));

	edit_hscrollbar.set_name ("EditorHScrollbar");

	build_cursors ();
	setup_toolbar ();

 	edit_point_clock.ValueChanged.connect (mem_fun(*this, &Editor::edit_point_clock_changed));
	
	time_canvas_vbox.pack_start (*_ruler_separator, false, false);
	time_canvas_vbox.pack_start (*minsec_ruler, false, false);
	time_canvas_vbox.pack_start (*smpte_ruler, false, false);
	time_canvas_vbox.pack_start (*frames_ruler, false, false);
	time_canvas_vbox.pack_start (*bbt_ruler, false, false);
	time_canvas_vbox.pack_start (time_canvas, true, true);
	time_canvas_vbox.set_size_request (-1, (int)(timebar_height * visible_timebars) + 2);

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

	time_button_frame.add(time_button_event_box);
	time_button_frame.property_shadow_type() = Gtk::SHADOW_OUT;

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

	edit_packer.attach (time_button_frame,       0, 2, 0, 1,    FILL,        FILL, 0, 0);
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
	route_display_model->signal_rows_reordered().connect (mem_fun (*this, &Editor::track_list_reorder));

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

	group_model->signal_row_changed().connect (mem_fun (*this, &Editor::edit_group_row_change));

	edit_group_display.set_name ("EditGroupList");
	edit_group_display.get_selection()->set_mode (SELECTION_SINGLE);
	edit_group_display.set_headers_visible (true);
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
	named_selection_display.set_name ("NamedSelectionDisplay");
	
	named_selection_display.get_selection()->set_mode (SELECTION_SINGLE);
	named_selection_display.set_size_request (100, -1);
	named_selection_display.signal_button_release_event().connect (mem_fun(*this, &Editor::named_selection_display_button_release), false);
	named_selection_display.signal_key_release_event().connect (mem_fun(*this, &Editor::named_selection_display_key_release), false);
	named_selection_display.get_selection()->signal_changed().connect (mem_fun (*this, &Editor::named_selection_display_selection_changed));

	/* SNAPSHOTS */

	snapshot_display_model = ListStore::create (snapshot_display_columns);
	snapshot_display.set_model (snapshot_display_model);
	snapshot_display.append_column (X_("snapshot"), snapshot_display_columns.visible_name);
	snapshot_display.set_name ("SnapshotDisplay");
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

	status_bar_hpacker.show ();

	vpacker.pack_end (status_bar_hpacker, false, false);
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

	WindowTitle title(Glib::get_application_name());
	title += _("Editor");
	set_title (title.get_string());
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
	Route::SyncOrderKeys.connect (mem_fun (*this, &Editor::sync_order_keys));

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

	playhead_cursor->set_y_axis (y1);
	if (logo_item) {
		logo_item->property_y() = y1;
	}

	controls_layout.get_vadjustment()->set_value (y1);

#ifdef GTKOSX
	/* the way idle updates and immediate window flushing work on GTK-Quartz
	   requires that we force an immediate redraw right here. The controls
	   layout will do the same all by itself, as does the canvas widget, but
	   most of the time, the canvas itself hasn't updated itself because its
	   idle handler hasn't run. consequently, the call that its layout makes
	   to gdk_window_process_updates() finds nothing to do. here, we force
	   the update to happen, then request a flush of the new window state.
	*/
	track_canvas.update_now ();
	gdk_window_process_updates (GTK_LAYOUT(track_canvas.gobj())->bin_window, true);
#endif
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
Editor::edit_point_clock_changed()
{
	if (_dragging_edit_point) {
		return;
	}

	if (selection->markers.empty()) {
		return;
	}

	bool ignored;
	Location* loc = find_location_from_marker (selection->markers.front(), ignored);

	if (!loc) {
		return;
	}

	loc->move_to (edit_point_clock.current_time());
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

	/*
		_control_scroll_target is an optional<T>
	
		it acts like a pointer to an nframes_t, with
		a operator conversion to boolean to check
		that it has a value could possibly use
		playhead_cursor->current_frame to store the
		value and a boolean in the class to know
		when it's out of date
	*/

	if (!_control_scroll_target) {
		_control_scroll_target = session->transport_frame();
		_dragging_playhead = true;
	}

	if ((fraction < 0.0f) && (*_control_scroll_target < (nframes_t) fabs(step))) {
		*_control_scroll_target = 0;
	} else if ((fraction > 0.0f) && (max_frames - *_control_scroll_target < step)) {
		*_control_scroll_target = max_frames - (current_page_frames()*2); // allow room for slop in where the PH is on the screen
	} else {
		*_control_scroll_target += (nframes_t) floor (step);
	}

	/* move visuals, we'll catch up with it later */

	playhead_cursor->set_position (*_control_scroll_target);
	UpdateAllTransportClocks (*_control_scroll_target);
	
	if (*_control_scroll_target > (current_page_frames() / 2)) {
		/* try to center PH in window */
		reset_x_origin (*_control_scroll_target - (current_page_frames()/2));
	} else {
		reset_x_origin (0);
	}

	/*
		Now we do a timeout to actually bring the session to the right place
		according to the playhead. This is to avoid reading disk buffers on every
		call to control_scroll, which is driven by ScrollTimeline and therefore
		probably by a control surface wheel which can generate lots of events.
	*/
	/* cancel the existing timeout */

	control_scroll_connection.disconnect ();

	/* add the next timeout */

	control_scroll_connection = Glib::signal_timeout().connect (bind (mem_fun (*this, &Editor::deferred_control_scroll), *_control_scroll_target), 250);
}

bool
Editor::deferred_control_scroll (nframes_t target)
{
	session->request_locate (*_control_scroll_target, session->transport_rolling());
	// reset for next stream
	_control_scroll_target = boost::none;
	_dragging_playhead = false;
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
		horizontal_adjustment.set_upper (last_canvas_frame / frames_per_unit);
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

		string session_name;

		if (session->snap_name() != session->name()) {
			session_name = session->snap_name();
		} else {
			session_name = session->name();
		}

		if (dirty) {
			session_name = "*" + session_name;
		}

		WindowTitle title(session_name);
		title += Glib::get_application_name();
		set_title (title.get_string());
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
	session_connections.push_back (session->AudioRegionAdded.connect (mem_fun(*this, &Editor::handle_new_audio_region)));
	session_connections.push_back (session->AudioRegionRemoved.connect (mem_fun(*this, &Editor::handle_audio_region_removed)));
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

	edit_point_clock.set_session (session);
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
	} else {
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
	} else {
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

	if (sfbrowser) {
		sfbrowser->set_session (session);
	}

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
			AudioTimeAxisView *atv;
			
			if ((atv = dynamic_cast<AudioTimeAxisView*>(tv)) != 0) {
				if (atv->route()->master()) {
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
			items.push_back (MenuElem (_("Deactivate"), bind (mem_fun (*this, &Editor::set_fade_in_active), false)));
		} else {
			items.push_back (MenuElem (_("Activate"), bind (mem_fun (*this, &Editor::set_fade_in_active), true)));
		}
		
		items.push_back (SeparatorElem());

		if (Profile->get_sae()) {
			items.push_back (MenuElem (_("Linear"), bind (mem_fun (*this, &Editor::set_fade_in_shape), AudioRegion::Linear)));
			items.push_back (MenuElem (_("Slowest"), bind (mem_fun (*this, &Editor::set_fade_in_shape), AudioRegion::Fast)));
		} else {
			items.push_back (MenuElem (_("Linear"), bind (mem_fun (*this, &Editor::set_fade_in_shape), AudioRegion::Linear)));
			items.push_back (MenuElem (_("Slowest"), bind (mem_fun (*this, &Editor::set_fade_in_shape), AudioRegion::Fast)));
			items.push_back (MenuElem (_("Slow"), bind (mem_fun (*this, &Editor::set_fade_in_shape), AudioRegion::LogB)));
			items.push_back (MenuElem (_("Fast"), bind (mem_fun (*this, &Editor::set_fade_in_shape), AudioRegion::LogA)));
			items.push_back (MenuElem (_("Fastest"), bind (mem_fun (*this, &Editor::set_fade_in_shape), AudioRegion::Slow)));
		}
		break;

	case FadeOutItem:
	case FadeOutHandleItem:
		if (arv->audio_region()->fade_out_active()) {
			items.push_back (MenuElem (_("Deactivate"), bind (mem_fun (*this, &Editor::set_fade_out_active), false)));
		} else {
			items.push_back (MenuElem (_("Activate"), bind (mem_fun (*this, &Editor::set_fade_out_active), true)));
		}
		
		items.push_back (SeparatorElem());

		if (Profile->get_sae()) {
			items.push_back (MenuElem (_("Linear"), bind (mem_fun (*this, &Editor::set_fade_out_shape), AudioRegion::Linear)));
			items.push_back (MenuElem (_("Slowest"), bind (mem_fun (*this, &Editor::set_fade_out_shape), AudioRegion::Slow)));
		} else {
			items.push_back (MenuElem (_("Linear"), bind (mem_fun (*this, &Editor::set_fade_out_shape), AudioRegion::Linear)));
			items.push_back (MenuElem (_("Slowest"), bind (mem_fun (*this, &Editor::set_fade_out_shape), AudioRegion::Slow)));
			items.push_back (MenuElem (_("Slow"), bind (mem_fun (*this, &Editor::set_fade_out_shape), AudioRegion::LogA)));
			items.push_back (MenuElem (_("Fast"), bind (mem_fun (*this, &Editor::set_fade_out_shape), AudioRegion::LogB)));
			items.push_back (MenuElem (_("Fastest"), bind (mem_fun (*this, &Editor::set_fade_out_shape), AudioRegion::Fast)));
		}
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
		if (clicked_audio_trackview->get_diskstream()) {
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
				if (clicked_regionview && clicked_regionview->region()->covers (get_preferred_edit_position())) {
					ActionManager::set_sensitive (ActionManager::edit_point_in_region_sensitive_actions, true);
				} else {
					ActionManager::set_sensitive (ActionManager::edit_point_in_region_sensitive_actions, false);
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

	if (item_type != SelectionItem && clicked_audio_trackview && clicked_audio_trackview->audio_track()) {

		/* Bounce to disk */
		
		using namespace Menu_Helpers;
		MenuList& edit_items  = menu->items();
		
		edit_items.push_back (SeparatorElem());

		switch (clicked_audio_trackview->audio_track()->freeze_state()) {
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

	AudioTimeAxisView* atv = dynamic_cast<AudioTimeAxisView*> (clicked_trackview);

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

	AudioTimeAxisView* atv = dynamic_cast<AudioTimeAxisView*> (clicked_trackview);

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
	// edit_items.push_back (SeparatorElem());
	// add_dstream_context_items (edit_items);

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

	region_menu->signal_map_event().connect (bind (mem_fun(*this, &Editor::set_selected_regionview_from_map_event), sv, boost::weak_ptr<Region>(region)));

	items.push_back (MenuElem (_("Popup region editor"), mem_fun(*this, &Editor::edit_region)));
	items.push_back (MenuElem (_("Raise to top layer"), mem_fun(*this, &Editor::raise_region_to_top)));
	items.push_back (MenuElem (_("Lower to bottom layer"), mem_fun  (*this, &Editor::lower_region_to_bottom)));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Define sync point"), mem_fun(*this, &Editor::set_region_sync_from_edit_point)));
	items.push_back (MenuElem (_("Remove sync point"), mem_fun(*this, &Editor::remove_region_sync)));
	items.push_back (SeparatorElem());

	items.push_back (MenuElem (_("Audition"), mem_fun(*this, &Editor::audition_selected_region)));
	items.push_back (MenuElem (_("Export"), mem_fun(*this, &Editor::export_region)));
	items.push_back (MenuElem (_("Bounce"), mem_fun(*this, &Editor::bounce_region_selection)));

#ifdef FFT_ANALYSIS
	items.push_back (MenuElem (_("Analyze region"), mem_fun(*this, &Editor::analyze_region_selection)));
#endif

	items.push_back (SeparatorElem());

	sigc::connection fooc;

	items.push_back (CheckMenuElem (_("Lock")));
	region_lock_item = static_cast<CheckMenuItem*>(&items.back());
	fooc = region_lock_item->signal_activate().connect (mem_fun(*this, &Editor::toggle_region_lock));
	if (region->locked()) {
		fooc.block (true);
		region_lock_item->set_active();
		fooc.block (false);
	}
	items.push_back (CheckMenuElem (_("Mute")));
	region_mute_item = static_cast<CheckMenuItem*>(&items.back());
	fooc = region_mute_item->signal_activate().connect (mem_fun(*this, &Editor::toggle_region_mute));
	if (region->muted()) {
		fooc.block (true);
		region_mute_item->set_active();
		fooc.block (false);
	}
	
	if (!Profile->get_sae()) {
		items.push_back (CheckMenuElem (_("Opaque")));
		region_opaque_item = static_cast<CheckMenuItem*>(&items.back());
		fooc = region_opaque_item->signal_activate().connect (mem_fun(*this, &Editor::toggle_region_opaque));
		if (region->opaque()) {
			fooc.block (true);
			region_opaque_item->set_active();
			fooc.block (false);
		}
	}

	items.push_back (CheckMenuElem (_("Original position"), mem_fun(*this, &Editor::naturalize)));
	if (region->at_natural_position()) {
		items.back().set_sensitive (false);
	}
	
	items.push_back (SeparatorElem());
	
	if (ar) {
		
		RegionView* rv = sv->find_view (ar);
		AudioRegionView* arv = dynamic_cast<AudioRegionView*>(rv);
		
		if (!Profile->get_sae()) {
			items.push_back (MenuElem (_("Reset Envelope"), mem_fun(*this, &Editor::reset_region_gain_envelopes)));

			items.push_back (CheckMenuElem (_("Envelope Visible")));
			region_envelope_visible_item = static_cast<CheckMenuItem*> (&items.back());
			fooc = region_envelope_visible_item->signal_activate().connect (mem_fun(*this, &Editor::toggle_gain_envelope_visibility));
			if (arv->envelope_visible()) {
				fooc.block (true);
				region_envelope_visible_item->set_active (true);
				fooc.block (false);
			}
		
			items.push_back (CheckMenuElem (_("Envelope Active")));
			region_envelope_active_item = static_cast<CheckMenuItem*> (&items.back());
			fooc = region_envelope_active_item->signal_activate().connect (mem_fun(*this, &Editor::toggle_gain_envelope_active));
			
			if (ar->envelope_active()) {
				fooc.block (true);
				region_envelope_active_item->set_active (true);
				fooc.block (false);
			}

			items.push_back (SeparatorElem());
		}

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
	
	trim_items.push_back (MenuElem (_("Start to edit point"), mem_fun(*this, &Editor::trim_region_from_edit_point)));
	trim_items.push_back (MenuElem (_("Edit point to end"), mem_fun(*this, &Editor::trim_region_to_edit_point)));
	trim_items.push_back (MenuElem (_("Trim To Loop"), mem_fun(*this, &Editor::trim_region_to_loop)));
	trim_items.push_back (MenuElem (_("Trim To Punch"), mem_fun(*this, &Editor::trim_region_to_punch)));
			     
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
Editor::add_selection_context_items (Menu_Helpers::MenuList& items)
{
	using namespace Menu_Helpers;

	items.push_back (MenuElem (_("Play range"), mem_fun(*this, &Editor::play_selection)));
	items.push_back (MenuElem (_("Loop range"), bind (mem_fun(*this, &Editor::set_loop_from_selection), true)));

#ifdef FFT_ANALYSIS
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Analyze range"), mem_fun(*this, &Editor::analyze_range_selection)));
#endif
	
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Extend Range to End of Region"), bind (mem_fun(*this, &Editor::extend_selection_to_end_of_region), false)));
	items.push_back (MenuElem (_("Extend Range to Start of Region"), bind (mem_fun(*this, &Editor::extend_selection_to_start_of_region), false)));

	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Convert to region in-place"), mem_fun(*this, &Editor::separate_region_from_selection)));
	items.push_back (MenuElem (_("Convert to region in region list"), mem_fun(*this, &Editor::new_region_from_selection)));
	
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Select all in range"), mem_fun(*this, &Editor::select_all_selectables_using_time_selection)));

	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Set loop from selection"), bind (mem_fun(*this, &Editor::set_loop_from_selection), false)));
	items.push_back (MenuElem (_("Set punch from selection"), mem_fun(*this, &Editor::set_punch_from_selection)));
	
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Add Range Markers"), mem_fun (*this, &Editor::add_location_from_selection)));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Crop region to range"), mem_fun(*this, &Editor::crop_region_to_selection)));
	items.push_back (MenuElem (_("Fill range with region"), mem_fun(*this, &Editor::region_fill_selection)));
	items.push_back (MenuElem (_("Duplicate range"), bind (mem_fun(*this, &Editor::duplicate_dialog), false)));
	items.push_back (MenuElem (_("Create chunk from range"), mem_fun(*this, &Editor::create_named_selection)));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Bounce range"), mem_fun(*this, &Editor::bounce_range_selection)));
	items.push_back (MenuElem (_("Export range"), mem_fun(*this, &Editor::export_selection)));
}

void
Editor::add_dstream_context_items (Menu_Helpers::MenuList& edit_items)
{
	using namespace Menu_Helpers;

	/* Playback */

	Menu *play_menu = manage (new Menu);
	MenuList& play_items = play_menu->items();
	play_menu->set_name ("ArdourContextMenu");
	
	play_items.push_back (MenuElem (_("Play from edit point"), mem_fun(*this, &Editor::play_from_edit_point)));
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
	select_items.push_back (MenuElem (_("Select All After Edit Point"), bind (mem_fun(*this, &Editor::select_all_selectables_using_edit), true)));
	select_items.push_back (MenuElem (_("Select All Before Edit Point"), bind (mem_fun(*this, &Editor::select_all_selectables_using_edit), false)));
	select_items.push_back (MenuElem (_("Select All After Playhead"), bind (mem_fun(*this, &Editor::select_all_selectables_using_cursor), playhead_cursor, true)));
	select_items.push_back (MenuElem (_("Select All Before Playhead"), bind (mem_fun(*this, &Editor::select_all_selectables_using_cursor), playhead_cursor, false)));
	select_items.push_back (MenuElem (_("Select All Between Playhead & Edit Point"), bind (mem_fun(*this, &Editor::select_all_selectables_between), false)));
	select_items.push_back (MenuElem (_("Select All Within Playhead & Edit Point"), bind (mem_fun(*this, &Editor::select_all_selectables_between), true)));
	select_items.push_back (MenuElem (_("Select Range Between Playhead & Edit Point"), mem_fun(*this, &Editor::select_range_between)));


	select_items.push_back (SeparatorElem());

	edit_items.push_back (MenuElem (_("Select"), *select_menu));

	/* Cut-n-Paste */

	Menu *cutnpaste_menu = manage (new Menu);
	MenuList& cutnpaste_items = cutnpaste_menu->items();
	cutnpaste_menu->set_name ("ArdourContextMenu");
	
	cutnpaste_items.push_back (MenuElem (_("Cut"), mem_fun(*this, &Editor::cut)));
	cutnpaste_items.push_back (MenuElem (_("Copy"), mem_fun(*this, &Editor::copy)));
	cutnpaste_items.push_back (MenuElem (_("Paste"), bind (mem_fun(*this, &Editor::paste), 1.0f)));

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
	nudge_items.push_back (MenuElem (_("Nudge track after edit point fwd"), (bind (mem_fun(*this, &Editor::nudge_track), true, true))));
	nudge_items.push_back (MenuElem (_("Nudge entire track bwd"), (bind (mem_fun(*this, &Editor::nudge_track), false, false))));
	nudge_items.push_back (MenuElem (_("Nudge track after edit point bwd"), (bind (mem_fun(*this, &Editor::nudge_track), true, false))));

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
	
	play_items.push_back (MenuElem (_("Play from edit point"), mem_fun(*this, &Editor::play_from_edit_point)));
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
	select_items.push_back (MenuElem (_("Select all after edit point"), bind (mem_fun(*this, &Editor::select_all_selectables_using_edit), true)));
	select_items.push_back (MenuElem (_("Select all before edit point"), bind (mem_fun(*this, &Editor::select_all_selectables_using_edit), false)));
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
	nudge_items.push_back (MenuElem (_("Nudge track after edit point fwd"), (bind (mem_fun(*this, &Editor::nudge_track), true, true))));
	nudge_items.push_back (MenuElem (_("Nudge entire track bwd"), (bind (mem_fun(*this, &Editor::nudge_track), false, false))));
	nudge_items.push_back (MenuElem (_("Nudge track after edit point bwd"), (bind (mem_fun(*this, &Editor::nudge_track), true, false))));

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
void
Editor::set_edit_point_preference (EditPoint ep)
{
	_edit_point = ep;
	string str = edit_point_strings[(int)ep];

	if (str != edit_point_selector.get_active_text ()) {
		edit_point_selector.set_active_text (str);
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

	if ((prop = node.property ("mixer-width"))) {
		editor_mixer_strip_width = Width (string_2_enum (prop->value(), editor_mixer_strip_width));
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

	if ((prop = node.property ("edit-point"))) {
		set_edit_point_preference ((EditPoint) string_2_enum (prop->value(), _edit_point));
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
		set_follow_playhead (yn);
		RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("toggle-follow-playhead"));
		if (act) {
			RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic(act);
			if (tact->get_active() != yn) {
				tact->set_active (yn);
			}
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

	maybe_add_mixer_strip_width (*node);
	
	snprintf (buf, sizeof(buf), "%d", (int) zoom_focus);
	node->add_property ("zoom-focus", buf);
	snprintf (buf, sizeof(buf), "%f", frames_per_unit);
	node->add_property ("zoom", buf);
	snprintf (buf, sizeof(buf), "%d", (int) snap_type);
	node->add_property ("snap-to", buf);
	snprintf (buf, sizeof(buf), "%d", (int) snap_mode);
	node->add_property ("snap-mode", buf);

	node->add_property ("edit-point", enum_2_string (_edit_point));

	snprintf (buf, sizeof (buf), "%" PRIu32, playhead_cursor->current_frame);
	node->add_property ("playhead", buf);

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
Editor::snap_to (nframes64_t& start, int32_t direction, bool for_mark)
{
	Location* before = 0;
	Location* after = 0;

	if (!session) {
		return;
	}

	const nframes64_t one_second = session->frame_rate();
	const nframes64_t one_minute = session->frame_rate() * 60;
	const nframes64_t one_smpte_second = (nframes64_t)(rint(session->smpte_frames_per_second()) * session->frames_per_smpte_frame());
	nframes64_t one_smpte_minute = (nframes64_t)(rint(session->smpte_frames_per_second()) * session->frames_per_smpte_frame() * 60);
	nframes64_t presnap = start;

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

	case SnapToEditPoint:
		start = get_preferred_edit_position ();
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
	Gtkmm2ext::set_size_request_to_display_given_text (zoom_focus_selector, "Playhead", FUDGE, 0);
	set_popdown_strings (zoom_focus_selector, zoom_focus_strings);
	zoom_focus_selector.signal_changed().connect (mem_fun(*this, &Editor::zoom_focus_selection_done));
	ARDOUR_UI::instance()->tooltips().set_tip (zoom_focus_selector, _("Zoom focus"));

	zoom_box.pack_start (zoom_focus_selector, true, true);
	zoom_box.pack_start (zoom_out_button, false, false);
	zoom_box.pack_start (zoom_in_button, false, false);
	zoom_box.pack_start (zoom_out_full_button, false, false);

	snap_box.set_spacing (1);
	snap_box.set_border_width (2);

	snap_type_selector.set_name ("SnapTypeSelector");
	Gtkmm2ext::set_size_request_to_display_given_text (snap_type_selector, "SMPTE Seconds", 2+FUDGE, 10);
	set_popdown_strings (snap_type_selector, snap_type_strings);
	snap_type_selector.signal_changed().connect (mem_fun(*this, &Editor::snap_type_selection_done));
	ARDOUR_UI::instance()->tooltips().set_tip (snap_type_selector, _("Snap/Grid Units"));

	snap_mode_selector.set_name ("SnapModeSelector");
	Gtkmm2ext::set_size_request_to_display_given_text (snap_mode_selector, "Magnetic Snap", 2+FUDGE, 10);
	set_popdown_strings (snap_mode_selector, snap_mode_strings);
	snap_mode_selector.signal_changed().connect (mem_fun(*this, &Editor::snap_mode_selection_done));
	ARDOUR_UI::instance()->tooltips().set_tip (snap_mode_selector, _("Snap/Grid Mode"));

	edit_point_selector.set_name ("SnapModeSelector");
	Gtkmm2ext::set_size_request_to_display_given_text (edit_point_selector, "Playhead", 2+FUDGE, 10);
	set_popdown_strings (edit_point_selector, edit_point_strings);
	edit_point_selector.signal_changed().connect (mem_fun(*this, &Editor::edit_point_selection_done));
	ARDOUR_UI::instance()->tooltips().set_tip (edit_point_selector, _("Edit point"));

	snap_box.pack_start (edit_point_clock, false, false);
	snap_box.pack_start (snap_mode_selector, false, false);
	snap_box.pack_start (snap_type_selector, false, false);
	snap_box.pack_start (edit_point_selector, false, false);

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

			
			ustring p = *i;
                        PBD::url_decode (p);

			// scan forward past three slashes
			
			ustring::size_type slashcnt = 0;
			ustring::size_type n = 0;
			ustring::iterator x = p.begin();

			while (slashcnt < 3 && x != p.end()) {
				if ((*x) == '/') {
					slashcnt++;
				} else if (slashcnt == 3) {
					break;
				}
				++n;
				++x;
			}

			if (slashcnt != 3 || x == p.end()) {
				error << _("malformed URL passed to drag-n-drop code") << endmsg;
				continue;
			}

			paths.push_back (p.substr (n - 1));
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

	update_loop_range_view (true);
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
                // before = &get_state();
		session->begin_reversible_command (name);
	}
}

void
Editor::commit_reversible_command ()
{
	if (session) {
		// session->commit_reversible_command (new MementoCommand<Editor>(*this, before, &get_state()));
		session->commit_reversible_command ();
	}
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
	if (selection->regions.empty() && (selection->time.length() == 0)) {
		return;
	}

	ArdourDialog win ("duplicate dialog");
	Label  label (_("Duplicate how many times?"));
	Adjustment adjustment (1.0, 1.0, 1000000.0, 1.0, 5.0);
	SpinButton spinner (adjustment);

	win.get_vbox()->set_spacing (12);
	win.get_vbox()->pack_start (label);

	/* dialogs have ::add_action_widget() but that puts the spinner in the wrong
	   place, visually. so do this by hand.
	*/

	win.get_vbox()->pack_start (spinner);
	spinner.signal_activate().connect (sigc::bind (mem_fun (win, &ArdourDialog::response), RESPONSE_ACCEPT));

	label.show ();
	spinner.show ();

	win.add_button (Stock::OK, RESPONSE_ACCEPT);
	win.add_button (Stock::CANCEL, RESPONSE_CANCEL);

	win.set_position (WIN_POS_MOUSE);

	spinner.grab_focus ();

	switch (win.run ()) {
	case RESPONSE_ACCEPT:
		break;
	default:
		return;
	}

	float times = adjustment.get_value();

	if (!selection->regions.empty()) {
		duplicate_some_regions (selection->regions, times);
	} else {
		duplicate_selection (times);
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
	} else if (choice == _("Edit Point")) {
		snaptype = SnapToEditPoint;
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
Editor::edit_point_selection_done ()
{
	string choice = edit_point_selector.get_active_text();
	EditPoint ep = EditAtSelectedMarker;

	if (choice == _("Marker")) {
		_edit_point = EditAtSelectedMarker;
	} else if (choice == _("Playhead")) {
		_edit_point = EditAtPlayhead;
	} else {
		_edit_point = EditAtMouse;
	}

	RefPtr<RadioAction> ract = edit_point_action (ep);

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
	} else if (choice == _("Mouse")) {
		focus_type = ZoomFocusMouse;
	} else if (choice == _("Edit Point")) {
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
		ARDOUR_UI::instance()->add_route (this);
	}
	return TRUE;
}

gint
Editor::mouse_select_button_release (GdkEventButton* ev)
{
	/* this handles just right-clicks */

	if (ev->button != 3) {
		return false;
	}

	return true;
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
	dialog.add_button (_("Keep playlist"), RESPONSE_REJECT);
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


/** A new snapshot has been selected.
 */
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
	if (ev->button == 3) {
		/* Right-click on the snapshot list. Work out which snapshot it
		   was over. */
		Gtk::TreeModel::Path path;
		Gtk::TreeViewColumn* col;
		int cx;
		int cy;
		snapshot_display.get_path_at_pos ((int) ev->x, (int) ev->y, path, col, cx, cy);
		Gtk::TreeModel::iterator iter = snapshot_display_model->get_iter (path);
		if (iter) {
			Gtk::TreeModel::Row row = *iter;
			popup_snapshot_context_menu (ev->button, ev->time, row[snapshot_display_columns.real_name]);
		}
		return true;
	}

	return false;
}


/** Pop up the snapshot display context menu.
 * @param button Button used to open the menu.
 * @param time Menu open time.
 * @snapshot_name Name of the snapshot that the menu click was over.
 */

void
Editor::popup_snapshot_context_menu (int button, int32_t time, Glib::ustring snapshot_name)
{
	using namespace Menu_Helpers;

	MenuList& items (snapshot_context_menu.items());
	items.clear ();

	const bool modification_allowed = (session->snap_name() != snapshot_name && session->name() != snapshot_name);

	items.push_back (MenuElem (_("Remove"), bind (mem_fun (*this, &Editor::remove_snapshot), snapshot_name)));
	if (!modification_allowed) {
		items.back().set_sensitive (false);
	}

	items.push_back (MenuElem (_("Rename"), bind (mem_fun (*this, &Editor::rename_snapshot), snapshot_name)));
	if (!modification_allowed) {
		items.back().set_sensitive (false);
	}

	snapshot_context_menu.popup (button, time);
}

void
Editor::rename_snapshot (Glib::ustring old_name)
{
	ArdourPrompter prompter(true);

	string new_name;

	prompter.set_name ("Prompter");
	prompter.add_button (Gtk::Stock::SAVE, Gtk::RESPONSE_ACCEPT);
	prompter.set_prompt (_("New name of snapshot"));
	prompter.set_initial_text (old_name);
	
	if (prompter.run() == RESPONSE_ACCEPT) {
		prompter.get_result (new_name);
		if (new_name.length()) {
			session->rename_state (old_name, new_name);
		        redisplay_snapshots ();
		}
	}
}


void
Editor::remove_snapshot (Glib::ustring name)
{
	vector<string> choices;

	std::string prompt  = string_compose (_("Do you really want to remove snapshot \"%1\" ?\n(cannot be undone)"), name);

	choices.push_back (_("No, do nothing."));
	choices.push_back (_("Yes, remove it."));

	Gtkmm2ext::Choice prompter (prompt, choices);

	if (prompter.run () == 1) {
		session->remove_state (name);
		redisplay_snapshots ();
	}
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
		
		/* this lingers on in case we ever want to change the visible
		   name of the snapshot.
		*/
		
		string display_name;
		display_name = statename;

		if (statename == session->snap_name()) {
			snapshot_display.get_selection()->select(row);
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

/**
 *  Make new playlists for a given track and also any others that belong
 *  to the same active edit group.
 *  @param v Track.
 */

void 
Editor::new_playlists (TimeAxisView* v)
{
	begin_reversible_command (_("new playlists"));
	mapover_audio_tracks (mem_fun (*this, &Editor::mapped_use_new_playlist), v);
	commit_reversible_command ();
}


/**
 *  Use a copy of the current playlist for a given track and also any others that belong
 *  to the same active edit group.
 *  @param v Track.
 */

void
Editor::copy_playlists (TimeAxisView* v)
{
	begin_reversible_command (_("copy playlists"));
	mapover_audio_tracks (mem_fun (*this, &Editor::mapped_use_copy_playlist), v);
	commit_reversible_command ();
}


/**
 *  Clear the current playlist for a given track and also any others that belong
 *  to the same active edit group.
 *  @param v Track.
 */

void 
Editor::clear_playlists (TimeAxisView* v)
{
	begin_reversible_command (_("clear playlists"));
	mapover_audio_tracks (mem_fun (*this, &Editor::mapped_clear_playlist), v);
	commit_reversible_command ();
}

void 
Editor::mapped_use_new_playlist (AudioTimeAxisView& atv, uint32_t sz)
{
	atv.use_new_playlist (sz > 1 ? false : true);
}

void
Editor::mapped_use_copy_playlist (AudioTimeAxisView& atv, uint32_t sz)
{
	atv.use_copy_playlist (sz > 1 ? false : true);
}

void 
Editor::mapped_clear_playlist (AudioTimeAxisView& atv, uint32_t sz)
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
	
	if (playhead_cursor) playhead_cursor->set_position (playhead_cursor->current_frame);

	instant_save ();
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
			redisplay_tempo (true);
		}
	}

	return 0; /* this is always a one-shot call */
}

struct EditorOrderTimeAxisSorter {
    bool operator() (const TimeAxisView* a, const TimeAxisView* b) const {
	    return a->order < b->order;
    }
};
	
void
Editor::sort_track_selection ()
{
	EditorOrderTimeAxisSorter cmp;
	selection->tracks.sort (cmp);
}

nframes64_t
Editor::get_preferred_edit_position()
{
	bool ignored;
	nframes64_t where = 0;

	switch (_edit_point) {
	case EditAtPlayhead:
		where = session->audible_frame();
		break;
		
	case EditAtSelectedMarker:
		if (!selection->markers.empty()) {
			bool whocares;
			Location* loc = find_location_from_marker (selection->markers.front(), whocares);
			if (loc) {
				where =  loc->start();
				break;
			}
		} 
		/* fallthru */
		
	default:
	case EditAtMouse:
		if (!mouse_frame (where, ignored)) {
			/* XXX not right but what can we do ? */
			return 0;
		}
		snap_to (where);
		break;
	}

	return where;
}

void
Editor::set_loop_range (nframes_t start, nframes_t end, string cmd)
{
	if (!session) return;

	begin_reversible_command (cmd);
	
	Location* tll;

	if ((tll = transport_loop_location()) == 0) {
		Location* loc = new Location (start, end, _("Loop"),  Location::IsAutoLoop);
                XMLNode &before = session->locations()->get_state();
		session->locations()->add (loc, true);
		session->set_auto_loop_location (loc);
                XMLNode &after = session->locations()->get_state();
		session->add_command (new MementoCommand<Locations>(*(session->locations()), &before, &after));
	}
	else {
                XMLNode &before = tll->get_state();
		tll->set_hidden (false, this);
		tll->set (start, end);
                XMLNode &after = tll->get_state();
                session->add_command (new MementoCommand<Location>(*tll, &before, &after));
	}
	
	commit_reversible_command ();
}

void
Editor::set_punch_range (nframes_t start, nframes_t end, string cmd)
{
	if (!session) return;

	begin_reversible_command (cmd);
	
	Location* tpl;

	if ((tpl = transport_punch_location()) == 0) {
		Location* loc = new Location (start, end, _("Loop"),  Location::IsAutoPunch);
                XMLNode &before = session->locations()->get_state();
		session->locations()->add (loc, true);
		session->set_auto_loop_location (loc);
                XMLNode &after = session->locations()->get_state();
		session->add_command (new MementoCommand<Locations>(*(session->locations()), &before, &after));
	}
	else {
                XMLNode &before = tpl->get_state();
		tpl->set_hidden (false, this);
		tpl->set (start, end);
                XMLNode &after = tpl->get_state();
                session->add_command (new MementoCommand<Location>(*tpl, &before, &after));
	}
	
	commit_reversible_command ();
}

RegionSelection
Editor::get_regions_at (nframes64_t where, const TrackSelection& ts) const
{
	RegionSelection rs;
	const TrackSelection* tracks;

	if (ts.empty()) {
		tracks = &track_views;
	} else {
		tracks = &ts;
	}

	for (TrackSelection::const_iterator t = tracks->begin(); t != tracks->end(); ++t) {
	
		AudioTimeAxisView* atv = dynamic_cast<AudioTimeAxisView*>(*t);

		if (atv) {
			boost::shared_ptr<Diskstream> ds;
			boost::shared_ptr<Playlist> pl;
			
			if ((ds = atv->get_diskstream()) && ((pl = ds->playlist()))) {

				Playlist::RegionList* regions = pl->regions_at ((nframes_t) floor ( (double)where * ds->speed()));

				for (Playlist::RegionList::iterator i = regions->begin(); i != regions->end(); ++i) {

					RegionView* rv = atv->audio_view()->find_view (*i);

					if (rv) {
						rs.push_back (rv);
					}
				}

				delete regions;
			}
		}
	}

	return rs;
}

RegionSelection&
Editor::get_regions_for_action ()
{
	if (!selection->regions.empty()) {
		return selection->regions;
	} 

	nframes64_t where = get_preferred_edit_position();
	tmp_regions = get_regions_at (where, selection->tracks);
	return tmp_regions;
}
