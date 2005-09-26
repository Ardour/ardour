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

#include <unistd.h>
#include <cstdlib>
#include <cmath>
#include <string>
#include <algorithm>

#include <sigc++/bind.h>

#include <libgnomecanvas/libgnomecanvas.h>
#include <pbd/error.h>

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/tearoff.h>
#include <gtkmm2ext/utils.h>

#include <ardour/audio_track.h>
#include <ardour/diskstream.h>
#include <ardour/plugin_manager.h>
#include <ardour/location.h>
#include <ardour/audioplaylist.h>
#include <ardour/audioregion.h>
#include <ardour/session.h>
#include <ardour/session_route.h>
#include <ardour/tempo.h>
#include <ardour/utils.h>

#include "ardour_ui.h"
#include "canvas-ruler.h"
#include "canvas-simpleline.h"
#include "canvas-simplerect.h"
#include "canvas-waveview.h"
#include "check_mark.h"
#include "editor.h"
#include "grouped_buttons.h"
#include "keyboard.h"
#include "library_ui.h"
#include "marker.h"
#include "playlist_selector.h"
#include "regionview.h"
#include "rgb_macros.h"
#include "selection.h"
#include "streamview.h"
#include "time_axis_view.h"
#include "utils.h"
#include "crossfade_view.h"
#include "editing.h"
#include "public_editor.h"
#include "crossfade_edit.h"
#include "extra_bind.h"
#include "audio_time_axis.h"
#include "gui_thread.h"

#include "i18n.h"

/* <CMT Additions> */
#include "imageframe_socket_handler.h"
/* </CMT Additions> */

using namespace std;
using namespace sigc;
using namespace ARDOUR;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Editing;

/* XXX this is a hack. it ought to be the maximum value of an jack_nframes_t */

const double max_canvas_coordinate = 100000000.0;
const double Editor::timebar_height = 15.0;

#include "editor_xpms"

static const gchar *route_list_titles[] = {
	N_("Tracks"),
	0
};

static const gchar *edit_group_list_titles[] = {
	"foo", "bar", 0
};

static const gchar *region_list_display_titles[] = {
	N_("Regions/name"), 
	0
};

static const gchar *named_selection_display_titles[] = {
	N_("Chunks"), 
	0
};

static const int32_t slide_index = 0;
static const int32_t splice_index = 1;

static const gchar *edit_mode_strings[] = {
	N_("Slide"),
	N_("Splice"),
	0
};

static const gchar *snap_type_strings[] = {
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

static const gchar *snap_mode_strings[] = {
	N_("Normal"),
	N_("Magnetic"),
	0
};

static const gchar *zoom_focus_strings[] = {
	N_("Left"),
	N_("Right"),
	N_("Center"),
	N_("Playhead"),
	N_("Edit Cursor"),
	0
};

/* Soundfile  drag-n-drop */

enum {
  TARGET_STRING,
  TARGET_ROOTWIN,
  TARGET_URL
};

static GtkTargetEntry target_table[] = {
  { "STRING",     0, TARGET_STRING },
  { "text/plain", 0, TARGET_STRING },
  { "text/uri-list", 0, TARGET_URL },
  { "application/x-rootwin-drop", 0, TARGET_ROOTWIN }
};

static guint n_targets = sizeof(target_table) / sizeof(target_table[0]);

GdkCursor* Editor::cross_hair_cursor = 0;
GdkCursor* Editor::selector_cursor = 0;
GdkCursor* Editor::trimmer_cursor = 0;
GdkCursor* Editor::grabber_cursor = 0;
GdkCursor* Editor::zoom_cursor = 0;
GdkCursor* Editor::time_fx_cursor = 0;
GdkCursor* Editor::fader_cursor = 0;
GdkCursor* Editor::speaker_cursor = 0;
GdkCursor* Editor::null_cursor = 0;
GdkCursor* Editor::wait_cursor = 0;
GdkCursor* Editor::timebar_cursor = 0;

GdkPixmap *Editor::check_pixmap = 0;
GdkBitmap *Editor::check_mask = 0;
GdkPixmap *Editor::empty_pixmap = 0;
GdkBitmap *Editor::empty_mask = 0;

extern gint route_list_compare_func (GtkCList*,gconstpointer,gconstpointer);

Editor::Editor (AudioEngine& eng) 
	: engine (eng),

	  /* time display buttons */

	  minsec_label (_("Mins:Secs")),
	  bbt_label (_("Bars:Beats")),
	  smpte_label (_("SMPTE")),
	  frame_label (_("Frames")),
	  tempo_label (_("Tempo")),
	  meter_label (_("Meter")),
	  mark_label (_("Location Markers")),
	  range_mark_label (_("Range Markers")),
	  transport_mark_label (_("Loop/Punch Ranges")),

	  edit_packer (3, 3, false),
	  edit_hscroll_left_arrow (Gtk::ARROW_LEFT, Gtk::SHADOW_OUT),
	  edit_hscroll_right_arrow (Gtk::ARROW_RIGHT, Gtk::SHADOW_OUT),

	  region_list_display (internationalize (region_list_display_titles)),

	  named_selection_display (internationalize (named_selection_display_titles)),
	  
	  /* tool bar related */

 	  editor_mixer_button (_("editor\nmixer")),

	  selection_start_clock (X_("SelectionStartClock"), true),
	  selection_end_clock (X_("SelectionEndClock"), true),
	  edit_cursor_clock (X_("EditCursorClock"), true),
	  zoom_range_clock (X_("ZoomRangeClock"), true, true),
	  
	  toolbar_selection_clock_table (2,3),
	  
	  mouse_mode_button_table (2, 3),

	  mouse_select_button (_("range")),
	  mouse_move_button (_("object")),
	  mouse_gain_button (_("gain")),
	  mouse_zoom_button (_("zoom")),
	  mouse_timefx_button (_("timefx")),
	  mouse_audition_button (_("listen")),

	  automation_mode_button (_("mode")),
	  global_automation_button (_("automation")),

	  edit_mode_label (_("Edit Mode")),
	  snap_type_label (_("Snap To")),
	  snap_mode_label(_("Snap Mode")),
	  zoom_focus_label (_("Zoom Focus")),

	  route_list (internationalize (route_list_titles)),
	  edit_group_list (internationalize (edit_group_list_titles)),
	  
	  /* <CMT Additions> */
	  image_socket_listener(0),
	  /* </CMT Additions> */

	  /* nudge */

	  nudge_label (_("Nudge")),
	  nudge_clock (X_("NudgeClock"), true, true)

{
	constructed = false;

	/* we are a singleton */

	PublicEditor::_instance = this;

	init_colormap ();

	check_pixmap = gdk_pixmap_colormap_create_from_xpm_d (NULL, 
							      gtk_widget_get_colormap (GTK_WIDGET(edit_group_list.gobj())),
							      &check_mask, NULL, (gchar **) check_xpm);
	empty_pixmap = gdk_pixmap_colormap_create_from_xpm_d (NULL, 
							      gtk_widget_get_colormap (GTK_WIDGET(edit_group_list.gobj())),
							      &empty_mask, NULL, (gchar **) empty_xpm);

	session = 0;

	selection = new Selection;
	cut_buffer = new Selection;

	selection->TimeChanged.connect (mem_fun(*this, &Editor::time_selection_changed));
	selection->TracksChanged.connect (mem_fun(*this, &Editor::track_selection_changed));
	selection->RegionsChanged.connect (mem_fun(*this, &Editor::region_selection_changed));
	selection->PointsChanged.connect (mem_fun(*this, &Editor::point_selection_changed));

	clicked_regionview = 0;
	clicked_trackview = 0;
	clicked_audio_trackview = 0;
	clicked_crossfadeview = 0;
	clicked_control_point = 0;
	latest_regionview = 0;
	region_list_display_drag_region = 0;
	last_update_frame = 0;
	drag_info.item = 0;
	last_audition_region = 0;
	region_list_button_region = 0;
	current_mixer_strip = 0;
	current_bbt_points = 0;

	snap_type = SnapToFrame;
	set_snap_to (snap_type);
	snap_mode = SnapNormal;
	set_snap_mode (snap_mode);
	snap_threshold = 5.0;
	bbt_beat_subdivision = 4;
	canvas_width = 0;
	canvas_height = 0;
	autoscroll_timeout_tag = -1;
	interthread_progress_window = 0;
	current_interthread_info = 0;
	_show_measures = true;
	_show_waveforms = true;
	_show_waveforms_recording = true;
	first_action_message = 0;
	export_dialog = 0;
	show_gain_after_trim = false;
	no_zoom_repos_update = false;
	need_wave_cursor = 0;
	ignore_route_list_reorder = false;
	verbose_cursor_on = true;
	route_removal = false;
	track_spacing = 0;
	show_automatic_regions_in_region_list = true;
	have_pending_keyboard_selection = false;
	_follow_playhead = true;
	_xfade_visibility = true;
	editor_ruler_menu = 0;
	no_ruler_shown_update = false;
	edit_group_list_menu = 0;
	route_list_menu = 0;
	region_list_menu = 0;
	marker_menu = 0;
	marker_menu_item = 0;
	tm_marker_menu = 0;
	transport_marker_menu = 0;
	new_transport_marker_menu = 0;
	editor_mixer_strip_width = Wide;
	repos_zoom_queued = false;
	import_audio_item = 0;
	embed_audio_item = 0;
	region_edit_menu_split_item = 0;
	temp_location = 0;
	region_edit_menu_split_multichannel_item = 0;
	edit_hscroll_dragging = false;
	leftmost_frame = 0;
	ignore_mouse_mode_toggle = false;
	current_stepping_trackview = 0;
	entered_track = 0;
	entered_regionview = 0;
	clear_entered_track = false;
	_new_regionviews_show_envelope = false;
	current_timestretch = 0;

	edit_cursor = 0;
	playhead_cursor = 0;

	location_marker_color = color_map[cLocationMarker];
	location_range_color = color_map[cLocationRange];
	location_cd_marker_color = color_map[cLocationCDMarker];
	location_loop_color = color_map[cLocationLoop];
	location_punch_color = color_map[cLocationPunch];

	range_marker_drag_rect = 0;
	marker_drag_line = 0;
	
	mouse_mode = MouseZoom; /* force change in next call */
	set_mouse_mode (MouseObject, true);

	frames_per_unit = 2048; /* too early to use set_frames_per_unit */
	zoom_focus = ZoomFocusLeft;
 	zoom_range_clock.ValueChanged.connect (mem_fun(*this, &Editor::zoom_adjustment_changed));

	initialize_rulers ();
	initialize_canvas ();

	track_canvas_scroller.add (*track_canvas);
	track_canvas_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_NEVER);
	track_canvas_scroller.set_name ("TrackCanvasScroller");

	track_canvas_scroller.get_vadjustment()->value_changed.connect (mem_fun(*this, &Editor::tie_vertical_scrolling));
	track_canvas_scroller.get_vadjustment()->set_step_increment (10.0);

	track_canvas_scroller.get_hadjustment()->set_lower (0.0);
	track_canvas_scroller.get_hadjustment()->set_upper (1200.0);
	track_canvas_scroller.get_hadjustment()->set_step_increment (20.0);
	track_canvas_scroller.get_hadjustment()->value_changed.connect (mem_fun(*this, &Editor::canvas_horizontally_scrolled));
	
	edit_vscrollbar.set_adjustment(track_canvas_scroller.get_vadjustment());
	edit_hscrollbar.set_adjustment(track_canvas_scroller.get_hadjustment());

 	edit_hscrollbar.button_press_event.connect (mem_fun(*this, &Editor::hscroll_slider_button_press));
 	edit_hscrollbar.button_release_event.connect (mem_fun(*this, &Editor::hscroll_slider_button_release));
 	edit_hscrollbar.size_allocate.connect (mem_fun(*this, &Editor::hscroll_slider_allocate));
	
	time_canvas_scroller.add (*time_canvas);
	time_canvas_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_NEVER);
	time_canvas_scroller.set_hadjustment (track_canvas_scroller.get_hadjustment());
	time_canvas_scroller.set_name ("TimeCanvasScroller");

	edit_controls_vbox.set_spacing (track_spacing);
	edit_controls_hbox.pack_start (edit_controls_vbox, true, true);
	edit_controls_scroller.add_with_viewport (edit_controls_hbox);
	edit_controls_scroller.set_name ("EditControlsBase");
	edit_controls_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_NEVER);

	Viewport* viewport = static_cast<Viewport*> (edit_controls_scroller.get_child());

	viewport->set_shadow_type (GTK_SHADOW_NONE);
	viewport->set_name ("EditControlsBase");
	viewport->add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK|GDK_ENTER_NOTIFY_MASK|GDK_LEAVE_NOTIFY_MASK);
	viewport->button_release_event.connect (mem_fun(*this, &Editor::edit_controls_button_release));

	build_cursors ();
	setup_toolbar ();

	XMLNode* node = ARDOUR_UI::instance()->editor_settings();
	set_state (*node);

 	edit_cursor_clock.ValueChanged.connect (mem_fun(*this, &Editor::edit_cursor_clock_changed));
	
	time_canvas_vbox.pack_start (*minsec_ruler, false, false);
	time_canvas_vbox.pack_start (*smpte_ruler, false, false);
	time_canvas_vbox.pack_start (*frames_ruler, false, false);
	time_canvas_vbox.pack_start (*bbt_ruler, false, false);
	time_canvas_vbox.pack_start (time_canvas_scroller, true, true);
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
	time_button_event_box.button_release_event.connect (mem_fun(*this, &Editor::ruler_label_button_release));

	/* these enable us to have a dedicated window (for cursor setting, etc.) 
	   for the canvas areas.
	*/

	track_canvas_event_box.add (track_canvas_scroller);

	time_canvas_event_box.add (time_canvas_vbox);
	time_canvas_event_box.set_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK|Gdk::POINTER_MOTION_MASK);

	
	edit_packer.set_col_spacings (0);
	edit_packer.set_row_spacings (0);
	edit_packer.set_homogeneous (false);
	edit_packer.set_name ("EditorWindow");

// 	edit_packer.attach (edit_hscroll_left_arrow_event,  0, 1, 0, 1,    Gtk::FILL,  0, 0, 0);
// 	edit_packer.attach (edit_hscroll_slider,            1, 2, 0, 1,    Gtk::FILL|Gtk::EXPAND,  0, 0, 0);
// 	edit_packer.attach (edit_hscroll_right_arrow_event, 2, 3, 0, 1,    Gtk::FILL,  0, 0, 0);
 	edit_packer.attach (edit_hscrollbar,            1, 2, 0, 1,    Gtk::FILL|Gtk::EXPAND,  0, 0, 0);

	edit_packer.attach (time_button_event_box,               0, 1, 1, 2,    Gtk::FILL,            0, 0, 0);
	edit_packer.attach (time_canvas_event_box,          1, 2, 1, 2,    Gtk::FILL|Gtk::EXPAND, 0, 0, 0);

	edit_packer.attach (edit_controls_scroller,         0, 1, 2, 3,    Gtk::FILL,                   Gtk::FILL|Gtk::EXPAND, 0, 0);
	edit_packer.attach (track_canvas_event_box,         1, 2, 2, 3,    Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND, 0, 0);
	edit_packer.attach (edit_vscrollbar,                2, 3, 2, 3,    0,                   Gtk::FILL|Gtk::EXPAND, 0, 0);

	edit_frame.set_name ("BaseFrame");
	edit_frame.set_shadow_type (Gtk::SHADOW_IN);
	edit_frame.add (edit_packer);

	zoom_in_button.set_name ("EditorTimeButton");
	zoom_out_button.set_name ("EditorTimeButton");
	ARDOUR_UI::instance()->tooltips().set_tip (zoom_in_button, _("Zoom in"));
	ARDOUR_UI::instance()->tooltips().set_tip (zoom_out_button, _("Zoom out"));

//	zoom_onetoone_button.set_name ("EditorTimeButton");
	zoom_out_full_button.set_name ("EditorTimeButton");
//	ARDOUR_UI::instance()->tooltips().set_tip (zoom_onetoone_button, _("Zoom in 1:1"));
	ARDOUR_UI::instance()->tooltips().set_tip (zoom_out_full_button, _("Zoom to session"));

	zoom_in_button.add (*(manage (new Gtk::Image (zoom_in_button_xpm))));
	zoom_out_button.add (*(manage (new Gtk::Image (zoom_out_button_xpm))));
	zoom_out_full_button.add (*(manage (new Gtk::Image (zoom_out_full_button_xpm))));
//	zoom_onetoone_button.add (*(manage (new Gtk::Image (zoom_onetoone_button_xpm))));

	
	zoom_in_button.signal_clicked().connect (bind (mem_fun(*this, &Editor::temporal_zoom_step), false));
	zoom_out_button.signal_clicked().connect (bind (mem_fun(*this, &Editor::temporal_zoom_step), true));
	zoom_out_full_button.signal_clicked().connect (mem_fun(*this, &Editor::temporal_zoom_session));
//	zoom_onetoone_button.signal_clicked().connect (bind (mem_fun(*this, &Editor::temporal_zoom), 1.0));
	
	zoom_indicator_box.pack_start (zoom_out_button, false, false);
	zoom_indicator_box.pack_start (zoom_in_button, false, false);
 	zoom_indicator_box.pack_start (zoom_range_clock, false, false);	
//	zoom_indicator_box.pack_start (zoom_onetoone_button, false, false);
	zoom_indicator_box.pack_start (zoom_out_full_button, false, false);
	
	zoom_indicator_label.set_text (_("Zoom Span"));
	zoom_indicator_label.set_name ("ToolBarLabel");


	zoom_indicator_vbox.set_spacing (3);
	zoom_indicator_vbox.set_border_width (3);
	zoom_indicator_vbox.pack_start (zoom_indicator_label, false, false);
	zoom_indicator_vbox.pack_start (zoom_indicator_box, false, false);


	bottom_hbox.set_border_width (3);
	bottom_hbox.set_spacing (3);

	route_list.set_name ("TrackListDisplay");
	route_list.set_size_request (75,-1);
	route_list.column_titles_active();
	route_list.set_compare_func (route_list_compare_func);
	route_list.set_shadow_type (Gtk::SHADOW_IN);
	route_list.set_selection_mode (GTK_SELECTION_MULTIPLE);
	route_list.set_reorderable (true);
	edit_group_list.set_size_request (75, -1);

	route_list_scroller.add (route_list);
	route_list_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	route_list.select_row.connect (mem_fun(*this, &Editor::route_list_selected));
	route_list.unselect_row.connect (mem_fun(*this, &Editor::route_list_unselected));
	route_list.row_move.connect (mem_fun(*this, &Editor::queue_route_list_reordered));
	route_list.click_column.connect (mem_fun(*this, &Editor::route_list_column_click));

	edit_group_list_button_label.set_text (_("Edit Groups"));
	edit_group_list_button_label.set_name ("EditGroupTitleButton");
	edit_group_list_button.add (edit_group_list_button_label);
	edit_group_list_button.set_name ("EditGroupTitleButton");

	edit_group_list.column_titles_hide();
	edit_group_list.set_name ("MixerGroupList");
	edit_group_list.set_shadow_type (Gtk::SHADOW_IN);
	edit_group_list.set_selection_mode (GTK_SELECTION_MULTIPLE);
	edit_group_list.set_reorderable (false);
	edit_group_list.set_size_request (75, -1);
	edit_group_list.set_column_auto_resize (0, true);
	edit_group_list.columns_autosize ();

	edit_group_list_scroller.add (edit_group_list);
	edit_group_list_scroller.set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

	edit_group_list_button.signal_clicked().connect (mem_fun(*this, &Editor::edit_group_list_button_clicked));
	edit_group_list.button_press_event.connect (mem_fun(*this, &Editor::edit_group_list_button_press_event));
	edit_group_list.select_row.connect (mem_fun(*this, &Editor::edit_group_selected));
	edit_group_list.unselect_row.connect (mem_fun(*this, &Editor::edit_group_unselected));

	list<string> stupid_list;

	stupid_list.push_back ("*");
	stupid_list.push_back (_("-all-"));

	edit_group_list.rows().push_back (stupid_list);
	edit_group_list.rows().back().set_data (0);
	edit_group_list.rows().back().select();
	
	edit_group_vbox.pack_start (edit_group_list_button, false, false);
	edit_group_vbox.pack_start (edit_group_list_scroller, true, true);
	
	route_list_frame.set_name ("BaseFrame");
	route_list_frame.set_shadow_type (Gtk::SHADOW_IN);
	route_list_frame.add (route_list_scroller);

	edit_group_list_frame.set_name ("BaseFrame");
	edit_group_list_frame.set_shadow_type (Gtk::SHADOW_IN);
	edit_group_list_frame.add (edit_group_vbox);

	route_group_vpane.add1 (route_list_frame);
	route_group_vpane.add2 (edit_group_list_frame);

	list_vpacker.pack_start (route_group_vpane, true, true);

	region_list_hidden_node = region_list_display.rows().end();

	region_list_display.add_events (GDK_ENTER_NOTIFY_MASK|GDK_LEAVE_NOTIFY_MASK|Gdk::POINTER_MOTION_MASK);

	region_list_display.drag_dest_set (GTK_DEST_DEFAULT_ALL,
					   target_table, n_targets - 1,
					   GdkDragAction (Gdk::ACTION_COPY|Gdk::ACTION_MOVE));
	region_list_display.drag_data_received.connect (mem_fun(*this, &Editor::region_list_display_drag_data_received));

	region_list_scroller.add (region_list_display);
	region_list_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	region_list_display.set_name ("RegionListDisplay");
	region_list_display.set_size_request (100, -1);
	region_list_display.column_titles_active ();
	region_list_display.set_selection_mode (GTK_SELECTION_SINGLE);

	region_list_display.set_data ("editor", this);
	region_list_display.set_compare_func (_region_list_sorter);
	region_list_sort_type = ByName;
	reset_region_list_sort_type (region_list_sort_type);

	region_list_display.set_flags (Gtk::CAN_FOCUS);

	region_list_display.key_press_event.connect (mem_fun(*this, &Editor::region_list_display_key_press));
	region_list_display.key_release_event.connect (mem_fun(*this, &Editor::region_list_display_key_release));
	region_list_display.button_press_event.connect (mem_fun(*this, &Editor::region_list_display_button_press));
	region_list_display.button_release_event.connect (mem_fun(*this, &Editor::region_list_display_button_release));
	region_list_display.motion_notify_event.connect (mem_fun(*this, &Editor::region_list_display_motion));
	region_list_display.enter_notify_event.connect (mem_fun(*this, &Editor::region_list_display_enter_notify));
	region_list_display.leave_notify_event.connect (mem_fun(*this, &Editor::region_list_display_leave_notify));
	region_list_display.select_row.connect (mem_fun(*this, &Editor::region_list_display_selected));
	region_list_display.unselect_row.connect (mem_fun(*this, &Editor::region_list_display_unselected));
	region_list_display.click_column.connect (mem_fun(*this, &Editor::region_list_column_click));
	
	named_selection_scroller.add (named_selection_display);
	named_selection_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	named_selection_display.set_name ("RegionListDisplay");
	named_selection_display.set_size_request (100, -1);
	named_selection_display.column_titles_active ();
	named_selection_display.set_selection_mode (GTK_SELECTION_SINGLE);

	named_selection_display.button_press_event.connect (mem_fun(*this, &Editor::named_selection_display_button_press));
	named_selection_display.select_row.connect (mem_fun(*this, &Editor::named_selection_display_selected));
	named_selection_display.unselect_row.connect (mem_fun(*this, &Editor::named_selection_display_unselected));

	region_selection_vpane.pack1 (region_list_scroller, true, true);
	region_selection_vpane.pack2 (named_selection_scroller, true, true);

	canvas_region_list_pane.pack1 (edit_frame, true, true);
	canvas_region_list_pane.pack2 (region_selection_vpane, true, true);

	track_list_canvas_pane.size_allocate.connect_after (bind (mem_fun(*this, &Editor::pane_allocation_handler),
								  static_cast<Gtk::Paned*> (&track_list_canvas_pane)));
	canvas_region_list_pane.size_allocate.connect_after (bind (mem_fun(*this, &Editor::pane_allocation_handler),
								   static_cast<Gtk::Paned*> (&canvas_region_list_pane)));
	route_group_vpane.size_allocate.connect_after (bind (mem_fun(*this, &Editor::pane_allocation_handler),
							     static_cast<Gtk::Paned*> (&route_group_vpane)));
	region_selection_vpane.size_allocate.connect_after (bind (mem_fun(*this, &Editor::pane_allocation_handler),
								  static_cast<Gtk::Paned*> (&region_selection_vpane)));
	
	track_list_canvas_pane.pack1 (list_vpacker, true, true);
	track_list_canvas_pane.pack2 (canvas_region_list_pane, true, true);

	/* provide special pane-handle event handling for easy "hide" action */

	/* 0: collapse to show left/upper child
	   1: collapse to show right/lower child
	*/

	route_group_vpane.set_data ("collapse-direction", (gpointer) 0);
	region_selection_vpane.set_data ("collapse-direction", (gpointer) 0);
	canvas_region_list_pane.set_data ("collapse-direction", (gpointer) 0);
	track_list_canvas_pane.set_data ("collapse-direction", (gpointer) 1);

	route_group_vpane.button_release_event.connect (bind (ptr_fun (pane_handler), static_cast<Paned*> (&route_group_vpane)));
	region_selection_vpane.button_release_event.connect (bind (ptr_fun (pane_handler), static_cast<Paned*> (&region_selection_vpane)));
	canvas_region_list_pane.button_release_event.connect (bind (ptr_fun (pane_handler), static_cast<Paned*> (&canvas_region_list_pane)));
	track_list_canvas_pane.button_release_event.connect (bind (ptr_fun (pane_handler), static_cast<Paned*> (&track_list_canvas_pane)));

	top_hbox.pack_start (toolbar_frame, true, true);

	HBox *hbox = manage (new HBox);
	hbox->pack_start (track_list_canvas_pane, true, true);

	global_vpacker.pack_start (top_hbox, false, false);
	global_vpacker.pack_start (*hbox, true, true);

	global_hpacker.pack_start (global_vpacker, true, true);

	set_name ("EditorWindow");

	vpacker.pack_end (global_hpacker, true, true);
	
	_playlist_selector = new PlaylistSelector();
	_playlist_selector->delete_event.connect (bind (ptr_fun (just_hide_it), static_cast<Window *> (_playlist_selector)));

	AudioRegionView::AudioRegionViewGoingAway.connect (mem_fun(*this, &Editor::catch_vanishing_audio_regionview));

	/* nudge stuff */

	nudge_forward_button.add (*(manage (new Gtk::Image (right_arrow_xpm))));
	nudge_backward_button.add (*(manage (new Gtk::Image (left_arrow_xpm))));

	ARDOUR_UI::instance()->tooltips().set_tip (nudge_forward_button, _("Nudge region/selection forwards"));
	ARDOUR_UI::instance()->tooltips().set_tip (nudge_backward_button, _("Nudge region/selection backwards"));

	nudge_forward_button.set_name ("TransportButton");
	nudge_backward_button.set_name ("TransportButton");

	fade_context_menu.set_name ("ArdourContextMenu");

	install_keybindings ();

	set_title (_("ardour: editor"));
	set_wmclass (_("ardour_editor"), "Ardour");

	add (vpacker);
	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK);

	configure_event.connect (mem_fun (*ARDOUR_UI::instance(), &ARDOUR_UI::configure_handler));
	delete_event.connect (mem_fun (*ARDOUR_UI::instance(), &ARDOUR_UI::exit_on_main_window_close));

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
Editor::catch_vanishing_audio_regionview (AudioRegionView *rv)
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
Editor::set_entered_regionview (AudioRegionView* rv)
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

gint
Editor::left_track_canvas (GdkEventCrossing *ev)
{
	set_entered_track (0);
	set_entered_regionview (0);
	return FALSE;
}


void
Editor::initialize_canvas ()
{
	gnome_canvas_init ();

	track_gnome_canvas = gnome_canvas_new_aa ();

	/* adjust sensitivity for "picking" items */

	// GNOME_CANVAS(track_gnome_canvas)->close_enough = 2;

	gtk_signal_connect (GTK_OBJECT(gnome_canvas_root (GNOME_CANVAS(track_gnome_canvas))), "event",
			    (GtkSignalFunc) Editor::_track_canvas_event, this);
	track_canvas = wrap (track_gnome_canvas);
	track_canvas->set_name ("EditorMainCanvas");

	track_canvas->add_events (Gdk::POINTER_MOTION_HINT_MASK);

	track_canvas->leave_notify_event.connect (mem_fun(*this, &Editor::left_track_canvas));
	
	/* set up drag-n-drop */

	track_canvas->drag_dest_set (GTK_DEST_DEFAULT_ALL,
				     target_table, n_targets - 1,
				     GdkDragAction (Gdk::ACTION_COPY|Gdk::ACTION_MOVE));
	track_canvas->drag_data_received.connect (mem_fun(*this, &Editor::track_canvas_drag_data_received));

	/* stuff for the verbose canvas cursor */

	string fontname = get_font_for_style (N_("VerboseCanvasCursor"));

	verbose_canvas_cursor = gnome_canvas_item_new (gnome_canvas_root(GNOME_CANVAS(track_gnome_canvas)),
						     gnome_canvas_text_get_type(),
						     "font", fontname.c_str(),
						     "anchor", GTK_ANCHOR_NW,
						     "fill_color_rgba", color_map[cVerboseCanvasCursor],
						     NULL);
	verbose_cursor_visible = false;

	/* a group to hold time (measure) lines */

	time_line_group = gnome_canvas_item_new (gnome_canvas_root(GNOME_CANVAS(track_gnome_canvas)),
						  gnome_canvas_group_get_type(),
						  "x", 0.0,
						  "y", 0.0,
						  NULL);

	cursor_group = gnome_canvas_item_new (gnome_canvas_root(GNOME_CANVAS(track_gnome_canvas)),
						  gnome_canvas_group_get_type(),
						  "x", 0.0,
						  "y", 0.0,
						  NULL);
	
	time_gnome_canvas = gnome_canvas_new_aa ();
	time_canvas = wrap (time_gnome_canvas);
	time_canvas->set_name ("EditorTimeCanvas");

	time_canvas->add_events (Gdk::POINTER_MOTION_HINT_MASK);

	meter_group = gnome_canvas_item_new (gnome_canvas_root(GNOME_CANVAS(time_gnome_canvas)),
					    gnome_canvas_group_get_type(),
					    "x", 0.0,
					    "y", 0.0,
					    NULL);
	tempo_group = gnome_canvas_item_new (gnome_canvas_root(GNOME_CANVAS(time_gnome_canvas)),
					    gnome_canvas_group_get_type(),
					    "x", 0.0,
					    "y", timebar_height,
					    NULL);
	marker_group = gnome_canvas_item_new (gnome_canvas_root(GNOME_CANVAS(time_gnome_canvas)),
					    gnome_canvas_group_get_type(),
					    "x", 0.0,
					    "y", timebar_height * 2.0,
					    NULL);
	range_marker_group = gnome_canvas_item_new (gnome_canvas_root(GNOME_CANVAS(time_gnome_canvas)),
					    gnome_canvas_group_get_type(),
					    "x", 0.0,
					    "y", timebar_height * 3.0,
					    NULL);
	transport_marker_group = gnome_canvas_item_new (gnome_canvas_root(GNOME_CANVAS(time_gnome_canvas)),
					    gnome_canvas_group_get_type(),
					    "x", 0.0,
					    "y", timebar_height * 4.0,
					    NULL);
	
	tempo_bar = gnome_canvas_item_new (GNOME_CANVAS_GROUP(tempo_group),
					 gnome_canvas_simplerect_get_type(),
					 "x1", 0.0,
					 "y1", 0.0,
					 "x2", max_canvas_coordinate,
					 "y2", timebar_height,
					 "fill_color_rgba", color_map[cTempoBar],
					 "outline_pixels", 0,
					  NULL);
	meter_bar = gnome_canvas_item_new (GNOME_CANVAS_GROUP(meter_group),
					 gnome_canvas_simplerect_get_type(),
					 "x1", 0.0,
					 "y1", 0.0,
					 "x2", max_canvas_coordinate,
					 "y2", timebar_height,
					 "fill_color_rgba", color_map[cMeterBar],
					 "outline_pixels", 0,
					 NULL);
	marker_bar = gnome_canvas_item_new (GNOME_CANVAS_GROUP(marker_group),
					  gnome_canvas_simplerect_get_type(),
					  "x1", 0.0,
					  "y1", 0.0,
					  "x2", max_canvas_coordinate,
					  "y2", timebar_height,
					  "fill_color_rgba", color_map[cMarkerBar],
					  "outline_pixels", 0,
					  NULL);
	range_marker_bar = gnome_canvas_item_new (GNOME_CANVAS_GROUP(range_marker_group),
						gnome_canvas_simplerect_get_type(),
						"x1", 0.0,
						"y1", 0.0,
						"x2", max_canvas_coordinate,
						"y2", timebar_height,
						"fill_color_rgba", color_map[cRangeMarkerBar],
						"outline_pixels", 0,
						NULL);
	transport_marker_bar = gnome_canvas_item_new (GNOME_CANVAS_GROUP(transport_marker_group),
						    gnome_canvas_simplerect_get_type(),
						    "x1", 0.0,
						    "y1", 0.0,
						    "x2", max_canvas_coordinate,
						    "y2", timebar_height,
						    "fill_color_rgba", color_map[cTransportMarkerBar],
						    "outline_pixels", 0,
						    NULL);
	
	range_bar_drag_rect = gnome_canvas_item_new (GNOME_CANVAS_GROUP(range_marker_group),
						   gnome_canvas_simplerect_get_type(),
						   "x1", 0.0,
						   "y1", 0.0,
						   "x2", 0.0,
						   "y2", timebar_height,
						   "fill_color_rgba", color_map[cRangeDragBarRectFill],
						   "outline_color_rgba", color_map[cRangeDragBarRect],
						   NULL);
	gnome_canvas_item_hide (range_bar_drag_rect);

	transport_bar_drag_rect = gnome_canvas_item_new (GNOME_CANVAS_GROUP(transport_marker_group),
						       gnome_canvas_simplerect_get_type(),
						       "x1", 0.0,
						       "y1", 0.0,
						       "x2", 0.0,
						       "y2", timebar_height,
						       "fill_color_rgba", color_map[cTransportDragRectFill],
						       "outline_color_rgba", color_map[cTransportDragRect],
						       NULL);
	gnome_canvas_item_hide (transport_bar_drag_rect);

	
	marker_drag_line_points = gnome_canvas_points_new (2);
	marker_drag_line_points->coords[0] = 0.0;
	marker_drag_line_points->coords[1] = 0.0;
	marker_drag_line_points->coords[2] = 0.0;
	marker_drag_line_points->coords[3] = 0.0;
	
	
	//cerr << "set mdl points, nc = " << marker_drag_line_points->num_points << endl;
	marker_drag_line = gnome_canvas_item_new (gnome_canvas_root(GNOME_CANVAS(track_gnome_canvas)),
						gnome_canvas_line_get_type(),
						"width_pixels", 1,
						"fill_color_rgba", color_map[cMarkerDragLine],
						"points", marker_drag_line_points,
						NULL);
	gnome_canvas_item_hide (marker_drag_line);

	range_marker_drag_rect = gnome_canvas_item_new (gnome_canvas_root(GNOME_CANVAS(track_gnome_canvas)),
					       gnome_canvas_simplerect_get_type(),
					       "x1", 0.0,
					       "y1", 0.0,
					       "x2", 0.0,
					       "y2", 0.0,
					       "fill_color_rgba", color_map[cRangeDragRectFill],
					       "outline_color_rgba", color_map[cRangeDragRect],
					       NULL);
	gnome_canvas_item_hide (range_marker_drag_rect);
	

	transport_loop_range_rect = gnome_canvas_item_new ((GNOME_CANVAS_GROUP(time_line_group)),
					       gnome_canvas_simplerect_get_type(),
					       "x1", 0.0,
					       "y1", 0.0,
					       "x2", 0.0,
					       "y2", 0.0,
					       "fill_color_rgba", color_map[cTransportLoopRectFill],
					       "outline_color_rgba", color_map[cTransportLoopRect],
					       "outline_pixels", 1,
					       NULL);
	gnome_canvas_item_hide (transport_loop_range_rect);

	transport_punch_range_rect = gnome_canvas_item_new ((GNOME_CANVAS_GROUP(time_line_group)),
					       gnome_canvas_simplerect_get_type(),
					       "x1", 0.0,
					       "y1", 0.0,
					       "x2", 0.0,
					       "y2", 0.0,
					       "fill_color_rgba", color_map[cTransportPunchRectFill],
					       "outline_color_rgba", color_map[cTransportPunchRect],
					       "outline_pixels", 0,
					       NULL);
	gnome_canvas_item_lower_to_bottom (transport_punch_range_rect);
	gnome_canvas_item_lower_to_bottom (transport_loop_range_rect); // loop on the bottom
	gnome_canvas_item_hide (transport_punch_range_rect);

	transport_punchin_line = gnome_canvas_item_new ((GNOME_CANVAS_GROUP(time_line_group)),
					       gnome_canvas_simplerect_get_type(),
					       "x1", 0.0,
					       "y1", 0.0,
					       "x2", 0.0,
					       "y2", 0.0,
					       "outline_color_rgba", color_map[cPunchInLine],
					       "outline_pixels", 1,
					       NULL);
	gnome_canvas_item_hide (transport_punchin_line);

	transport_punchout_line  = gnome_canvas_item_new ((GNOME_CANVAS_GROUP(time_line_group)),
					       gnome_canvas_simplerect_get_type(),
					       "x1", 0.0,
					       "y1", 0.0,
					       "x2", 0.0,
					       "y2", 0.0,
					       "outline_color_rgba", color_map[cPunchOutLine],
					       "outline_pixels", 1,
					       NULL);
	gnome_canvas_item_hide (transport_punchout_line);



	
	// used to show zoom mode active zooming
	zoom_rect = gnome_canvas_item_new (gnome_canvas_root(GNOME_CANVAS(track_gnome_canvas)),
					 gnome_canvas_simplerect_get_type(),
					 "x1", 0.0,
					 "y1", 0.0,
					 "x2", 0.0,
					 "y2", 0.0,
					 "fill_color_rgba", color_map[cZoomRectFill],
					 "outline_color_rgba", color_map[cZoomRect],
					 "outline_pixels", 1,
					 NULL);
	gnome_canvas_item_hide (zoom_rect);
	gtk_signal_connect (GTK_OBJECT(zoom_rect), "event",
			    (GtkSignalFunc) PublicEditor::canvas_zoom_rect_event,
			    this);

	// used as rubberband rect
	rubberband_rect = gnome_canvas_item_new (gnome_canvas_root(GNOME_CANVAS(track_gnome_canvas)),
					       gnome_canvas_simplerect_get_type(),
					       "x1", 0.0,
					       "y1", 0.0,
					       "x2", 0.0,
					       "y2", 0.0,
					       "outline_color_rgba", color_map[cRubberBandRect],
					       "fill_color_rgba", (guint32) color_map[cRubberBandRectFill],
					       "outline_pixels", 1,
					       NULL);
	gnome_canvas_item_hide (rubberband_rect);

	

	gtk_signal_connect (GTK_OBJECT(tempo_bar), "event",
			    (GtkSignalFunc) PublicEditor::canvas_tempo_bar_event,
			    this);

	gtk_signal_connect (GTK_OBJECT(meter_bar), "event",
			    (GtkSignalFunc) PublicEditor::canvas_meter_bar_event,
			    this);

	gtk_signal_connect (GTK_OBJECT(marker_bar), "event",
			    (GtkSignalFunc) PublicEditor::canvas_marker_bar_event,
			    this);

	gtk_signal_connect (GTK_OBJECT(range_marker_bar), "event",
			    (GtkSignalFunc) PublicEditor::canvas_range_marker_bar_event,
			    this);

	gtk_signal_connect (GTK_OBJECT(transport_marker_bar), "event",
			    (GtkSignalFunc) PublicEditor::canvas_transport_marker_bar_event,
			    this);
	
	/* separator lines */

	tempo_line_points = gnome_canvas_points_new (2);
	tempo_line_points->coords[0] = 0;
	tempo_line_points->coords[1] = timebar_height;
	tempo_line_points->coords[2] = max_canvas_coordinate;
	tempo_line_points->coords[3] = timebar_height;
	//cerr << "set tl points, nc = " << tempo_line_points->num_points << endl;
	tempo_line = gnome_canvas_item_new (GNOME_CANVAS_GROUP(tempo_group),
					  gnome_canvas_line_get_type(),
					  "width_pixels", 0,
					  "fill_color", "black",
					  "points", tempo_line_points,
					  NULL);

	// cerr << "tempo line @ " << tempo_line << endl;

	meter_line_points = gnome_canvas_points_new (2);
	meter_line_points->coords[0] = 0;
	meter_line_points->coords[1] = timebar_height;
	meter_line_points->coords[2] = max_canvas_coordinate;
	meter_line_points->coords[3] = timebar_height;
	// cerr << "set ml points, nc = " << tempo_line_points->num_points << endl;
	meter_line = gnome_canvas_item_new (GNOME_CANVAS_GROUP(meter_group),
					  gnome_canvas_line_get_type(),
					  "width_pixels", 0,
					  "fill_color", "black",
					  "points", meter_line_points,
					  NULL);

	// cerr << "meter line @ " << tempo_line << endl;
	
	marker_line_points = gnome_canvas_points_new (2);
	marker_line_points->coords[0] = 0;
	marker_line_points->coords[1] = timebar_height;
	marker_line_points->coords[2] = max_canvas_coordinate;
	marker_line_points->coords[3] = timebar_height;
	// cerr << "set ml2 points, nc = " << marker_line_points->num_points << endl;
	marker_line = gnome_canvas_item_new (GNOME_CANVAS_GROUP(marker_group),
					   gnome_canvas_line_get_type(),
					   "width_pixels", 0,
					   "fill_color", "black",
					   "points", marker_line_points,
					   NULL);
	// cerr << "set rml points, nc = " << marker_line_points->num_points << endl;
	range_marker_line = gnome_canvas_item_new (GNOME_CANVAS_GROUP(range_marker_group),
					   gnome_canvas_line_get_type(),
					   "width_pixels", 0,
					   "fill_color", "black",
					   "points", marker_line_points,
					   NULL);
	// cerr << "set tml2 points, nc = " << marker_line_points->num_points << endl;
	transport_marker_line = gnome_canvas_item_new (GNOME_CANVAS_GROUP(transport_marker_group),
					   gnome_canvas_line_get_type(),
					   "width_pixels", 0,
					   "fill_color", "black",
					   "points", marker_line_points,
					   NULL);

	// cerr << "marker line @ " << marker_line << endl;

	ZoomChanged.connect (bind (mem_fun(*this, &Editor::update_loop_range_view), false));
	ZoomChanged.connect (bind (mem_fun(*this, &Editor::update_punch_range_view), false));
	
	double time_height = timebar_height * 5;
	double time_width = FLT_MAX/frames_per_unit;
	gnome_canvas_set_scroll_region (GNOME_CANVAS(time_gnome_canvas), 0.0, 0.0, time_width, time_height);

	edit_cursor = new Cursor (*this, "blue", (GtkSignalFunc) _canvas_edit_cursor_event);
	playhead_cursor = new Cursor (*this, "red", (GtkSignalFunc) _canvas_playhead_cursor_event);

	track_canvas->size_allocate.connect (mem_fun(*this, &Editor::track_canvas_allocate));
}

void
Editor::show_window ()
{
	show_all ();
	
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
	edit_controls_scroller.get_vadjustment()->set_value (track_canvas_scroller.get_vadjustment()->get_value());

	float y1 = track_canvas_scroller.get_vadjustment()->get_value();
	playhead_cursor->set_y_axis(y1);
	edit_cursor->set_y_axis(y1);
}

void
Editor::set_frames_per_unit (double fpu)
{
	jack_nframes_t frames;

	if (fpu == frames_per_unit) {
		return;
	}

	if (fpu < 1.0) {
		fpu = 1.0;
	}

	// convert fpu to frame count

	frames = (jack_nframes_t) (fpu * canvas_width);
	
	/* don't allow zooms that fit more than the maximum number
	   of frames into an 800 pixel wide space.
	*/

	if (max_frames / fpu < 800.0) {
		return;
	}

	frames_per_unit = fpu;

	if (frames != zoom_range_clock.current_duration()) {
		zoom_range_clock.set (frames);
	}

	/* only update these if we not about to call reposition_x_origin,
	   which will do the same updates.
	*/
	
	if (session) {
		track_canvas_scroller.get_hadjustment()->set_upper (session->current_end_frame() / frames_per_unit);
	}
	
	if (!no_zoom_repos_update) {
		track_canvas_scroller.get_hadjustment()->set_value (leftmost_frame/frames_per_unit);
		update_hscroller ();
		update_fixed_rulers ();
		tempo_map_changed (Change (0));
	}

	if (mouse_mode == MouseRange && selection->time.start () != selection->time.end_frame ()) {
		for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
			(*i)->reshow_selection (selection->time);
		}
	}

	ZoomChanged (); /* EMIT_SIGNAL */

	if (edit_cursor) edit_cursor->set_position (edit_cursor->current_frame);
	if (playhead_cursor) playhead_cursor->set_position (playhead_cursor->current_frame);

	instant_save ();

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
		Config->add_instant_xml(get_state(), Config->get_user_ardour_path());
	}
}

void
Editor::reposition_x_origin (jack_nframes_t frame)
{
	if (frame != leftmost_frame) {
		leftmost_frame = frame;
		double pixel = frame_to_pixel (frame);
		if (pixel >= track_canvas_scroller.get_hadjustment()->get_upper()) {
			track_canvas_scroller.get_hadjustment()->set_upper (frame_to_pixel (frame + (current_page_frames())));
		}
		track_canvas_scroller.get_hadjustment()->set_value (frame/frames_per_unit);
		XOriginChanged (); /* EMIT_SIGNAL */
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
	if (session == 0 || no_zoom_repos_update) {
		return;
	}

	double fpu = (double) zoom_range_clock.current_duration() / (double) canvas_width;

	if (fpu < 1.0) {
		fpu = 1.0;
		zoom_range_clock.set ((jack_nframes_t) (fpu * canvas_width));
	}
	else if (fpu > session->current_end_frame() / (double) canvas_width) {
		fpu = session->current_end_frame() / (double) canvas_width;
		zoom_range_clock.set ((jack_nframes_t) (fpu * canvas_width));
	}
	
	temporal_zoom (fpu);
}

void 
Editor::canvas_horizontally_scrolled ()
{
	/* XXX note the potential loss of accuracy here caused by
	   adjustments being 32bit floats with only a 24 bit mantissa,
	   whereas jack_nframes_t is at least a 32 bit uint32_teger.
	*/
	
	leftmost_frame = (jack_nframes_t) floor (track_canvas_scroller.get_hadjustment()->get_value() * frames_per_unit);
	
	update_hscroller ();
	update_fixed_rulers ();
	
	if (!edit_hscroll_dragging) {
		tempo_map_changed (Change (0));
	} else {
	        update_tempo_based_rulers();
	}
}

void
Editor::reposition_and_zoom (jack_nframes_t frame, double nfpu)
{
	if (!repos_zoom_queued) {
		Main::idle.connect (bind (mem_fun(*this, &Editor::deferred_reposition_and_zoom), frame, nfpu));
		repos_zoom_queued = true;
	}
}

gint
Editor::deferred_reposition_and_zoom (jack_nframes_t frame, double nfpu)
{
 	/* if we need to force an update to the hscroller stuff,
	   don't set no_zoom_repos_update.
	*/

	no_zoom_repos_update = (frame != leftmost_frame);
	
	set_frames_per_unit (nfpu);
	if (no_zoom_repos_update) {
		reposition_x_origin  (frame);
	}
	no_zoom_repos_update = false;
	repos_zoom_queued = false;
	
	return FALSE;
}

void
Editor::on_realize ()
{
	/* Even though we're not using acceleration, we want the
	   labels to show up.
	*/

	track_context_menu.accelerate (*this->get_toplevel());
	track_region_context_menu.accelerate (*this->get_toplevel());
	
	Window::on_realize ();

	GdkPixmap* empty_pixmap = gdk_pixmap_new (get_window(), 1, 1, 1);
	GdkPixmap* empty_bitmap = gdk_pixmap_new (get_window(), 1, 1, 1);
	GdkColor white = { 0, 0, 0 };

	null_cursor = gdk_cursor_new_from_pixmap (empty_pixmap, empty_bitmap, &white, &white, 0, 0);
}

void
Editor::on_map ()
{
	Window::on_map ();

	track_canvas_scroller.get_window().set_cursor (current_canvas_cursor);
	time_canvas_scroller.get_window().set_cursor (timebar_cursor);
}

void
Editor::track_canvas_allocate (GtkAllocation *alloc)
{
	canvas_width = alloc->width;
	canvas_height = alloc->height;

	if (session == 0 && !ARDOUR_UI::instance()->will_create_new_session_automatically()) {

		string fontname = get_font_for_style (N_("FirstActionMessage"));

		const char *txt1 = _("Start a new session\n");
		const char *txt2 = _("via Session menu");

		/* this mess of code is here to find out how wide this text is and
		   position the message in the center of the editor window. there
		   are two lines, so we use the longer of the the lines to
		   compute width, and multiply the height by 2.
		*/
			
		gint width;
		gint lbearing;
		gint rbearing;
		gint ascent;
		gint descent;
		
		/* this is a dummy widget that exists so that we can get the
		   style from the RC file. 
		*/
		
		Label foo (_(txt2));
		foo.set_name ("NoSessionMessage");
		foo.ensure_style ();
		
		gdk_string_extents (foo.get_style()->get_font(),
				    _(txt2),
				    &lbearing,
				    &rbearing,
				    &width,
				    &ascent,
				    &descent);
			
		if (first_action_message == 0) {
			
			char txt[strlen(txt1)+strlen(txt2)+1];
			
			/* merge both lines */
			
			strcpy (txt, _(txt1));
			strcat (txt, _(txt2));
			
			first_action_message = gnome_canvas_item_new (gnome_canvas_root(GNOME_CANVAS(track_gnome_canvas)),
								    gnome_canvas_text_get_type(),
								    "font", fontname.c_str(),
								    "fill_color_rgba", color_map[cFirstActionMessage],
								    "x", (gdouble) (canvas_width - width) / 2.0,
								    "y", (gdouble) (canvas_height/2.0) - (2.0 * (ascent+descent)),
								    "anchor", GTK_ANCHOR_NORTH_WEST,
								    "text", txt,
								    NULL);
			
		} else {

			/* center it */

			gnome_canvas_item_set (first_action_message,
					     "x", (gdouble) (canvas_width - width) / 2.0,
					     "y", (gdouble) (canvas_height/2.0) - (2.0 * (ascent+descent)),
					     NULL);
		}
	}

	zoom_range_clock.set ((jack_nframes_t) (canvas_width * frames_per_unit));
	edit_cursor->set_position (edit_cursor->current_frame);
	playhead_cursor->set_position (playhead_cursor->current_frame);
	reset_scrolling_region (alloc);
	
	Resized (); /* EMIT_SIGNAL */
}

void
Editor::reset_scrolling_region (GtkAllocation *alloc)
{
	guint32 last_canvas_unit;
	double height;
	guint32 canvas_alloc_height, canvas_alloc_width;
	TrackViewList::iterator i;
	static bool first_time = true;

	/* We need to make sure that the canvas always has its
	   scrolling region set to larger of:

	   - the size allocated for it (within the container its packed in)
	   - the size required to see the entire session

	   If we don't ensure at least the first of these, the canvas
	   does some wierd and in my view unnecessary stuff to center
	   itself within the allocated area, which causes bad, bad
	   results.
	   
	   XXX GnomeCanvas has fixed this, and has an option to
	   control the centering behaviour.
	*/

	last_canvas_unit = (guint32) ceil ((float) max_frames / frames_per_unit);

	height = 0;

	if (session) {
		for (i = track_views.begin(); i != track_views.end(); ++i) {
			if ((*i)->control_parent) {
				height += (*i)->effective_height;
				height += track_spacing;
			}
		}
		
		if (height) {
			height -= track_spacing;
		}
	}

	canvas_height = (guint32) height;
	
	if (alloc) {
		canvas_alloc_height = alloc->height;
		canvas_alloc_width = alloc->width;
	} else {
		canvas_alloc_height = track_gnome_canvas->allocation.height;
		canvas_alloc_width = track_gnome_canvas->allocation.width;
	}

	canvas_height = max (canvas_height, canvas_alloc_height);

	gnome_canvas_set_scroll_region (GNOME_CANVAS(track_gnome_canvas), 0.0, 0.0, 
				      max (last_canvas_unit, canvas_alloc_width),
				      canvas_height);

	if (edit_cursor) edit_cursor->set_length (canvas_alloc_height);
	if (playhead_cursor) playhead_cursor->set_length (canvas_alloc_height);

	if (marker_drag_line) {
		marker_drag_line_points->coords[3] = canvas_height;
		// cerr << "set mlA points, nc = " << marker_drag_line_points->num_points << endl;
		gnome_canvas_item_set (marker_drag_line, "points", marker_drag_line_points, NULL);
	}
	if (range_marker_drag_rect) {
		gnome_canvas_item_set (range_marker_drag_rect, "y1", 0.0, "y2", (double) canvas_height, NULL);
	}
	if (transport_loop_range_rect) {
		gnome_canvas_item_set (transport_loop_range_rect, "y1", 0.0, "y2", (double) canvas_height, NULL);
	}
	if (transport_punch_range_rect) {
		gnome_canvas_item_set (transport_punch_range_rect, "y1", 0.0, "y2", (double) canvas_height, NULL);
	}
	if (transport_punchin_line) {
		gnome_canvas_item_set (transport_punchin_line, "y1", 0.0, "y2", (double) canvas_height, NULL);
	}
	if (transport_punchout_line) {
		gnome_canvas_item_set (transport_punchout_line, "y1", 0.0, "y2", (double) canvas_height, NULL);
	}
	
	
	update_fixed_rulers ();

	if (is_visible() && first_time) {
		tempo_map_changed (Change (0));
		first_time = false;
	} else {
		redisplay_tempo ();
	}
}

void
Editor::queue_session_control_changed (Session::ControlType t)
{
	Gtkmm2ext::UI::instance()->call_slot (bind (mem_fun(*this, &Editor::session_control_changed), t));
}

void
Editor::session_control_changed (Session::ControlType t)
{
	// right now we're only tracking the loop and punch state

	switch (t) {
	case Session::AutoLoop:
		update_loop_range_view (true);
		break;
	case Session::PunchIn:
	case Session::PunchOut:
		update_punch_range_view (true);
		break;

	default:
		break;
	}
}

void
Editor::fake_add_edit_group (RouteGroup *group)
{
	Gtkmm2ext::UI::instance()->call_slot (bind (mem_fun(*this, &Editor::add_edit_group), group));
}

void
Editor::fake_handle_new_audio_region (AudioRegion *region)
{
	Gtkmm2ext::UI::instance()->call_slot (bind (mem_fun(*this, &Editor::handle_new_audio_region), region));
}

void
Editor::fake_handle_audio_region_removed (AudioRegion *region)
{
	Gtkmm2ext::UI::instance()->call_slot (bind (mem_fun(*this, &Editor::handle_audio_region_removed), region));
}

void
Editor::fake_handle_new_duration ()
{
	Gtkmm2ext::UI::instance()->call_slot (mem_fun(*this, &Editor::handle_new_duration));
}

void
Editor::start_scrolling ()
{
	scroll_connection = ARDOUR_UI::instance()->SuperRapidScreenUpdate.connect 
		(mem_fun(*this, &Editor::update_current_screen));

	slower_update_connection = ARDOUR_UI::instance()->RapidScreenUpdate.connect 
		(mem_fun(*this, &Editor::update_slower));
}

void
Editor::stop_scrolling ()
{
	scroll_connection.disconnect ();
	slower_update_connection.disconnect ();
}

void
Editor::map_position_change (jack_nframes_t frame)
{
	ENSURE_GUI_THREAD (bind (mem_fun(*this, &Editor::map_position_change), frame));

	if (session == 0 || !_follow_playhead) {
		return;
	}

	center_screen (frame);
	playhead_cursor->set_position (frame);
}	

void
Editor::center_screen (jack_nframes_t frame)
{
	float page = canvas_width * frames_per_unit;

	/* if we're off the page, then scroll.
	 */
	
	if (frame < leftmost_frame || frame >= leftmost_frame + page) {
		center_screen_internal (frame,page);
	}
}

void
Editor::center_screen_internal (jack_nframes_t frame, float page)
{
	page /= 2;
		
	if (frame > page) {
		frame -= (jack_nframes_t) page;
	} else {
		frame = 0;
	}

    reposition_x_origin (frame);
}

void
Editor::handle_new_duration ()
{
	reset_scrolling_region ();

	if (session) {
		track_canvas_scroller.get_hadjustment()->set_upper (session->current_end_frame() / frames_per_unit);
		track_canvas_scroller.get_hadjustment()->set_value (leftmost_frame/frames_per_unit);
	}
	
	update_hscroller ();
}

void
Editor::update_title_s (string snap_name)
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

	if (first_action_message) {
		gnome_canvas_item_hide (first_action_message);
	}

	flush_track_canvas();

	update_title ();

	session->going_away.connect (mem_fun(*this, &Editor::session_going_away));

	/* These signals can all be emitted by a non-GUI thread. Therefore the
	   handlers for them must not attempt to directly interact with the GUI,
	   but use Gtkmm2ext::UI::instance()->call_slot();
	*/

	session_connections.push_back (session->TransportStateChange.connect (mem_fun(*this, &Editor::map_transport_state)));
	session_connections.push_back (session->PositionChanged.connect (mem_fun(*this, &Editor::map_position_change)));
	session_connections.push_back (session->RouteAdded.connect (mem_fun(*this, &Editor::handle_new_route_p)));
	session_connections.push_back (session->AudioRegionAdded.connect (mem_fun(*this, &Editor::fake_handle_new_audio_region)));
	session_connections.push_back (session->AudioRegionRemoved.connect (mem_fun(*this, &Editor::fake_handle_audio_region_removed)));
	session_connections.push_back (session->DurationChanged.connect (mem_fun(*this, &Editor::fake_handle_new_duration)));
	session_connections.push_back (session->edit_group_added.connect (mem_fun(*this, &Editor::fake_add_edit_group)));
	session_connections.push_back (session->NamedSelectionAdded.connect (mem_fun(*this, &Editor::handle_new_named_selection)));
	session_connections.push_back (session->NamedSelectionRemoved.connect (mem_fun(*this, &Editor::handle_new_named_selection)));
	session_connections.push_back (session->DirtyChanged.connect (mem_fun(*this, &Editor::update_title)));
	session_connections.push_back (session->StateSaved.connect (mem_fun(*this, &Editor::update_title_s)));
	session_connections.push_back (session->AskAboutPlaylistDeletion.connect (mem_fun(*this, &Editor::playlist_deletion_dialog)));
	session_connections.push_back (session->RegionHiddenChange.connect (mem_fun(*this, &Editor::region_hidden)));

	session_connections.push_back (session->SMPTEOffsetChanged.connect (mem_fun(*this, &Editor::update_just_smpte)));
	session_connections.push_back (session->SMPTETypeChanged.connect (mem_fun(*this, &Editor::update_just_smpte)));

	session_connections.push_back (session->tempo_map().StateChanged.connect (mem_fun(*this, &Editor::tempo_map_changed)));

	session->foreach_edit_group(this, &Editor::add_edit_group);

	editor_mixer_button.toggled.connect (mem_fun(*this, &Editor::editor_mixer_button_toggled));
	editor_mixer_button.set_name (X_("EditorMixerButton"));

	edit_cursor_clock.set_session (session);
	selection_start_clock.set_session (session);
	selection_end_clock.set_session (session);
	zoom_range_clock.set_session (session);
	_playlist_selector->set_session (session);
	nudge_clock.set_session (session);

	switch (session->get_edit_mode()) {
	case Splice:
		edit_mode_selector.get_entry()->set_text (edit_mode_strings[splice_index]);
		break;

	case Slide:
		edit_mode_selector.get_entry()->set_text (edit_mode_strings[slide_index]);
		break;
	}

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

	update_loop_range_view (true);
	update_punch_range_view (true);
	
	session->ControlChanged.connect (mem_fun(*this, &Editor::queue_session_control_changed));

	
	refresh_location_display ();
	session->locations()->added.connect (mem_fun(*this, &Editor::add_new_location));
	session->locations()->removed.connect (mem_fun(*this, &Editor::location_gone));
	session->locations()->changed.connect (mem_fun(*this, &Editor::refresh_location_display));
	session->locations()->StateChanged.connect (mem_fun(*this, &Editor::refresh_location_display_s));
	session->locations()->end_location()->changed.connect (mem_fun(*this, &Editor::end_location_changed));

	reset_scrolling_region ();

	redisplay_regions ();
	redisplay_named_selections ();

	route_list.freeze ();
	route_list.clear ();
	session->foreach_route (this, &Editor::handle_new_route);
	// route_list.select_all ();
	route_list.sort ();
	route_list_reordered ();
	route_list.thaw ();

	if (embed_audio_item) {
		embed_audio_item->set_sensitive (true);
	} 
	if (import_audio_item) {
		import_audio_item->set_sensitive (true);
	}

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		(static_cast<TimeAxisView*>(*i))->set_samples_per_unit (frames_per_unit);
	}

	/* ::reposition_x_origin() doesn't work right here, since the old
	   position may be zero already, and it does nothing in such
	   circumstances.
	*/

	leftmost_frame = 0;
	
	track_canvas_scroller.get_hadjustment()->set_upper (session->current_end_frame() / frames_per_unit);
	track_canvas_scroller.get_hadjustment()->set_value (0);

	update_hscroller ();
	restore_ruler_visibility ();
	tempo_map_changed (Change (0));

	edit_cursor->set_position (0);
	playhead_cursor->set_position (0);

	start_scrolling ();

	XMLNode* node = ARDOUR_UI::instance()->editor_settings();
	set_state (*node);

	/* don't show master bus in a new session */

	if (ARDOUR_UI::instance()->session_is_new ()) {

		Gtk::CList_Helpers::RowList::iterator i;
		Gtk::CList_Helpers::RowList& rowlist = route_list.rows();

		route_list.freeze ();
		
		for (i = rowlist.begin(); i != rowlist.end(); ++i) {
			TimeAxisView *tv = (TimeAxisView *) i->get_data ();
			AudioTimeAxisView *atv;

			if ((atv = dynamic_cast<AudioTimeAxisView*>(tv)) != 0) {
				if (atv->route().master()) {
					(*i)->unselect ();
				}
			}
		}

		route_list.thaw ();
	}
}

void
Editor::build_cursors ()
{
	GdkPixmap *source, *mask;
	GdkColor fg = { 0, 65535, 0, 0 }; /* Red. */
	GdkColor bg = { 0, 0, 0, 65535 }; /* Blue. */
	
	source = gdk_bitmap_create_from_data (NULL, hand_bits,
					      hand_width, hand_height);
	mask = gdk_bitmap_create_from_data (NULL, handmask_bits,
					    handmask_width, handmask_height);
	grabber_cursor = gdk_cursor_new_from_pixmap (source, mask, &fg, &bg, hand_x_hot, hand_y_hot);
	gdk_pixmap_unref (source);
	gdk_pixmap_unref (mask);


	GdkColor mbg = { 0, 0, 0, 0 }; /* Black */
	GdkColor mfg = { 0, 0, 0, 65535 }; /* Blue. */
	
	source = gdk_bitmap_create_from_data (NULL, mag_bits,
					      mag_width, mag_height);
	mask = gdk_bitmap_create_from_data (NULL, magmask_bits,
					    mag_width, mag_height);
	zoom_cursor = gdk_cursor_new_from_pixmap (source, mask, &mfg, &mbg, mag_x_hot, mag_y_hot);
	gdk_pixmap_unref (source);
	gdk_pixmap_unref (mask);

	GdkColor fbg = { 0, 65535, 65535, 65535 };
	GdkColor ffg = { 0, 0, 0, 0 };
	
	source = gdk_bitmap_create_from_data (NULL, fader_cursor_bits,
					      fader_cursor_width, fader_cursor_height);
	mask = gdk_bitmap_create_from_data (NULL, fader_cursor_mask_bits,
					    fader_cursor_width, fader_cursor_height);
	fader_cursor = gdk_cursor_new_from_pixmap (source, mask, &ffg, &fbg, fader_cursor_x_hot, fader_cursor_y_hot);
	gdk_pixmap_unref (source);
	gdk_pixmap_unref (mask);

	source = gdk_bitmap_create_from_data (NULL, speaker_cursor_bits,
					      speaker_cursor_width, speaker_cursor_height);
	mask = gdk_bitmap_create_from_data (NULL, speaker_cursor_mask_bits,
					    speaker_cursor_width, speaker_cursor_height);
	speaker_cursor = gdk_cursor_new_from_pixmap (source, mask, &ffg, &fbg, speaker_cursor_x_hot, speaker_cursor_y_hot);
	gdk_pixmap_unref (source);
	gdk_pixmap_unref (mask);

	cross_hair_cursor = gdk_cursor_new (GDK_CROSSHAIR);
	trimmer_cursor = gdk_cursor_new (GDK_SB_H_DOUBLE_ARROW);
	selector_cursor = gdk_cursor_new (GDK_XTERM);
	time_fx_cursor = gdk_cursor_new (GDK_SIZING);
	wait_cursor = gdk_cursor_new (GDK_WATCH);
	timebar_cursor = gdk_cursor_new (GDK_LEFT_PTR);
}

void
Editor::popup_fade_context_menu (int button, int32_t time, GnomeCanvasItem* item, ItemType item_type)
{
	using namespace Menu_Helpers;
	AudioRegionView* arv = static_cast<AudioRegionView*> (gtk_object_get_data (GTK_OBJECT(item), "regionview"));

	if (arv == 0) {
		fatal << _("programming error: fade in canvas item has no regionview data pointer!") << endmsg;
		/*NOTREACHED*/
	}

	MenuList& items (fade_context_menu.items());

	items.clear ();

	switch (item_type) {
	case FadeInItem:
	case FadeInHandleItem:
		if (arv->region.fade_in_active()) {
			items.push_back (MenuElem (_("Deactivate"), bind (slot (*arv, &AudioRegionView::set_fade_in_active), false)));
		} else {
			items.push_back (MenuElem (_("Activate"), bind (slot (*arv, &AudioRegionView::set_fade_in_active), true)));
		}
		
		items.push_back (SeparatorElem());
		
		items.push_back (MenuElem (_("Linear"), bind (slot (arv->region, &AudioRegion::set_fade_in_shape), AudioRegion::Linear)));
		items.push_back (MenuElem (_("Slowest"), bind (slot (arv->region, &AudioRegion::set_fade_in_shape), AudioRegion::LogB)));
		items.push_back (MenuElem (_("Slow"), bind (slot (arv->region, &AudioRegion::set_fade_in_shape), AudioRegion::Fast)));
		items.push_back (MenuElem (_("Fast"), bind (slot (arv->region, &AudioRegion::set_fade_in_shape), AudioRegion::LogA)));
		items.push_back (MenuElem (_("Fastest"), bind (slot (arv->region, &AudioRegion::set_fade_in_shape), AudioRegion::Slow)));
		break;

	case FadeOutItem:
	case FadeOutHandleItem:
		if (arv->region.fade_out_active()) {
			items.push_back (MenuElem (_("Deactivate"), bind (slot (*arv, &AudioRegionView::set_fade_out_active), false)));
		} else {
			items.push_back (MenuElem (_("Activate"), bind (slot (*arv, &AudioRegionView::set_fade_out_active), true)));
		}
		
		items.push_back (SeparatorElem());
		
		items.push_back (MenuElem (_("Linear"), bind (slot (arv->region, &AudioRegion::set_fade_out_shape), AudioRegion::Linear)));
		items.push_back (MenuElem (_("Slowest"), bind (slot (arv->region, &AudioRegion::set_fade_out_shape), AudioRegion::Fast)));
		items.push_back (MenuElem (_("Slow"), bind (slot (arv->region, &AudioRegion::set_fade_out_shape), AudioRegion::LogB)));
		items.push_back (MenuElem (_("Fast"), bind (slot (arv->region, &AudioRegion::set_fade_out_shape), AudioRegion::LogA)));
		items.push_back (MenuElem (_("Fastest"), bind (slot (arv->region, &AudioRegion::set_fade_out_shape), AudioRegion::Slow)));

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
Editor::popup_track_context_menu (int button, int32_t time, ItemType item_type, bool with_selection, jack_nframes_t frame)
{
	using namespace Menu_Helpers;
	Menu* (Editor::*build_menu_function)(jack_nframes_t);
	Menu *menu;

	switch (item_type) {
	case RegionItem:
	case AudioRegionViewName:
	case AudioRegionViewNameHighlight:
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
	case AudioRegionViewName:
	case AudioRegionViewNameHighlight:
		if (!with_selection) {
			if (region_edit_menu_split_item) {
				if (clicked_regionview && clicked_regionview->region.covers (edit_cursor->current_frame)) {
					region_edit_menu_split_item->set_sensitive (true);
				} else {
					region_edit_menu_split_item->set_sensitive (false);
				}
			}
			if (region_edit_menu_split_multichannel_item) {
				if (clicked_regionview && clicked_regionview->region.n_channels() > 1) {
					region_edit_menu_split_multichannel_item->set_sensitive (true);
				} else {
					region_edit_menu_split_multichannel_item->set_sensitive (false);
				}
			}
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

	if (clicked_audio_trackview && clicked_audio_trackview->audio_track()) {

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
Editor::build_track_context_menu (jack_nframes_t ignored)
{
	using namespace Menu_Helpers;

 	MenuList& edit_items = track_context_menu.items();
	edit_items.clear();

	add_dstream_context_items (edit_items);
	return &track_context_menu;
}

Menu*
Editor::build_track_bus_context_menu (jack_nframes_t ignored)
{
	using namespace Menu_Helpers;

 	MenuList& edit_items = track_context_menu.items();
	edit_items.clear();

	add_bus_context_items (edit_items);
	return &track_context_menu;
}

Menu*
Editor::build_track_region_context_menu (jack_nframes_t frame)
{
	using namespace Menu_Helpers;
	MenuList& edit_items  = track_region_context_menu.items();
	edit_items.clear();

	AudioTimeAxisView* atv = dynamic_cast<AudioTimeAxisView*> (clicked_trackview);

	if (atv) {
		DiskStream* ds;
		Playlist* pl;
		
		if ((ds = atv->get_diskstream()) && ((pl = ds->playlist()))) {
			Playlist::RegionList* regions = pl->regions_at ((jack_nframes_t) floor ( (double)frame * ds->speed()));
			for (Playlist::RegionList::iterator i = regions->begin(); i != regions->end(); ++i) {
				add_region_context_items (atv->view, (*i), edit_items);
			}
			delete regions;
		}
	}

	add_dstream_context_items (edit_items);

	return &track_region_context_menu;
}

Menu*
Editor::build_track_crossfade_context_menu (jack_nframes_t frame)
{
	using namespace Menu_Helpers;
	MenuList& edit_items  = track_crossfade_context_menu.items();
	edit_items.clear ();

	AudioTimeAxisView* atv = dynamic_cast<AudioTimeAxisView*> (clicked_trackview);

	if (atv) {
		DiskStream* ds;
		Playlist* pl;
		AudioPlaylist* apl;

		if ((ds = atv->get_diskstream()) && ((pl = ds->playlist()) != 0) && ((apl = dynamic_cast<AudioPlaylist*> (pl)) != 0)) {

			Playlist::RegionList* regions = pl->regions_at (frame);
			AudioPlaylist::Crossfades xfades;

			apl->crossfades_at (frame, xfades);

			bool many = xfades.size() > 1;

			for (AudioPlaylist::Crossfades::iterator i = xfades.begin(); i != xfades.end(); ++i) {
				add_crossfade_context_items (atv->view, (*i), edit_items, many);
			}

			for (Playlist::RegionList::iterator i = regions->begin(); i != regions->end(); ++i) {
				add_region_context_items (atv->view, (*i), edit_items);
			}

			delete regions;
		}
	}

	add_dstream_context_items (edit_items);

	return &track_crossfade_context_menu;
}

Menu*
Editor::build_track_selection_context_menu (jack_nframes_t ignored)
{
	using namespace Menu_Helpers;
	MenuList& edit_items  = track_selection_context_menu.items();
	edit_items.clear ();

	add_selection_context_items (edit_items);
	add_dstream_context_items (edit_items);

	return &track_selection_context_menu;
}

void
Editor::add_crossfade_context_items (StreamView* view, Crossfade* xfade, Menu_Helpers::MenuList& edit_items, bool many)
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

	items.push_back (MenuElem (str, bind (mem_fun(*this, &Editor::toggle_xfade_active), xfade)));
	items.push_back (MenuElem (_("Edit"), bind (mem_fun(*this, &Editor::edit_xfade), xfade)));

	if (xfade->can_follow_overlap()) {

		if (xfade->following_overlap()) {
			str = _("Convert to short");
		} else {
			str = _("Convert to full");
		}

		items.push_back (MenuElem (str, bind (mem_fun(*this, &Editor::toggle_xfade_length), xfade)));
	}

	if (many) {
		str = xfade->out().name();
		str += "->";
		str += xfade->in().name();
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
Editor::add_region_context_items (StreamView* sv, Region* region, Menu_Helpers::MenuList& edit_items)
{
	using namespace Menu_Helpers;
	Menu     *region_menu = manage (new Menu);
	MenuList& items       = region_menu->items();
	region_menu->set_name ("ArdourContextMenu");
	
	AudioRegion* ar = 0;

	if (region) {
		ar = dynamic_cast<AudioRegion*> (region);
	}

	/* when this particular menu pops up, make the relevant region 
	   become selected.
	*/

	region_menu->map_event.connect (bind (mem_fun(*this, &Editor::set_selected_regionview_from_map_event), sv, region));

	items.push_back (MenuElem (_("Popup region editor"), mem_fun(*this, &Editor::edit_region)));
	items.push_back (MenuElem (_("Raise to top layer"), mem_fun(*this, &Editor::raise_region_to_top)));
	items.push_back (MenuElem (_("Lower to bottom layer"), slot  (*this, &Editor::lower_region_to_bottom)));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Define sync point"), mem_fun(*this, &Editor::set_region_sync_from_edit_cursor)));
	items.push_back (MenuElem (_("Remove sync point"), mem_fun(*this, &Editor::remove_region_sync)));
	items.push_back (SeparatorElem());

	items.push_back (MenuElem (_("Audition"), mem_fun(*this, &Editor::audition_selected_region)));
	items.push_back (MenuElem (_("Export"), mem_fun(*this, &Editor::export_region)));
	items.push_back (MenuElem (_("Bounce"), mem_fun(*this, &Editor::bounce_region_selection)));
	items.push_back (SeparatorElem());

	/* XXX hopefully this nonsense will go away with SigC++ 2.X, where the compiler
	   might be able to figure out which overloaded member function to use in
	   a bind() call ...
	*/

	void (Editor::*type_A_pmf)(void (Region::*pmf)(bool), bool) = &Editor::region_selection_op;

	items.push_back (MenuElem (_("Lock"), bind (mem_fun(*this, type_A_pmf), &Region::set_locked, true)));
	items.push_back (MenuElem (_("Unlock"), bind (mem_fun(*this, type_A_pmf), &Region::set_locked, false)));
	items.push_back (SeparatorElem());

	if (region->muted()) {
		items.push_back (MenuElem (_("Unmute"), bind (mem_fun(*this, type_A_pmf), &Region::set_muted, false)));
	} else {
		items.push_back (MenuElem (_("Mute"), bind (mem_fun(*this, type_A_pmf), &Region::set_muted, true)));
	}
	items.push_back (SeparatorElem());

	items.push_back (MenuElem (_("Original position"), mem_fun(*this, &Editor::naturalize)));
	items.push_back (SeparatorElem());


	if (ar) {

		items.push_back (MenuElem (_("Toggle envelope visibility"), mem_fun(*this, &Editor::toggle_gain_envelope_visibility)));
		items.push_back (MenuElem (_("Toggle envelope active"), mem_fun(*this, &Editor::toggle_gain_envelope_active)));
		items.push_back (SeparatorElem());

		if (ar->scale_amplitude() != 1.0f) {
			items.push_back (MenuElem (_("DeNormalize"), mem_fun(*this, &Editor::denormalize_region)));
		} else {
			items.push_back (MenuElem (_("Normalize"), mem_fun(*this, &Editor::normalize_region)));
		}
	}
	items.push_back (MenuElem (_("Reverse"), mem_fun(*this, &Editor::reverse_region)));
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
	region_edit_menu_split_item = items.back();

	items.push_back (MenuElem (_("Make mono regions"), (mem_fun(*this, &Editor::split_multichannel_region))));
	region_edit_menu_split_multichannel_item = items.back();

	items.push_back (MenuElem (_("Duplicate"), (bind (mem_fun(*this, &Editor::duplicate_dialog), true))));
	items.push_back (MenuElem (_("Fill Track"), (mem_fun(*this, &Editor::region_fill_track))));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Remove"), mem_fun(*this, &Editor::remove_clicked_region)));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Destroy"), mem_fun(*this, &Editor::destroy_clicked_region)));

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
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Create chunk from range"), mem_fun(*this, &Editor::name_selection)));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Create Region"), mem_fun(*this, &Editor::new_region_from_selection)));
	items.push_back (MenuElem (_("Separate Region"), mem_fun(*this, &Editor::separate_region_from_selection)));
	items.push_back (MenuElem (_("Crop Region to range"), mem_fun(*this, &Editor::crop_region_to_selection)));
	items.push_back (MenuElem (_("Bounce range"), mem_fun(*this, &Editor::bounce_range_selection)));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Duplicate"), bind (mem_fun(*this, &Editor::duplicate_dialog), false)));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Export"), mem_fun(*this, &Editor::export_selection)));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Fill range w/Region"), mem_fun(*this, &Editor::region_fill_selection)));
	
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
	
	play_items.push_back (MenuElem (_("Play from edit cursor")));
	play_items.push_back (MenuElem (_("Play from start"), mem_fun(*this, &Editor::play_from_start)));
	play_items.push_back (MenuElem (_("Play region"), mem_fun(*this, &Editor::play_selected_region)));
	play_items.push_back (SeparatorElem());
	play_items.push_back (MenuElem (_("Loop Region"), mem_fun(*this, &Editor::loop_selected_region)));
	
	edit_items.push_back (MenuElem (_("Play"), *play_menu));

	/* Selection */

	Menu *select_menu = manage (new Menu);
	MenuList& select_items = select_menu->items();
	select_menu->set_name ("ArdourContextMenu");
	
	select_items.push_back (MenuElem (_("Select All in track"), bind (mem_fun(*this, &Editor::select_all_in_track), false)));
	select_items.push_back (MenuElem (_("Select All"), bind (mem_fun(*this, &Editor::select_all), false)));
	select_items.push_back (MenuElem (_("Invert in track"), mem_fun(*this, &Editor::invert_selection_in_track)));
	select_items.push_back (MenuElem (_("Invert"), mem_fun(*this, &Editor::invert_selection)));
	select_items.push_back (SeparatorElem());
	select_items.push_back (MenuElem (_("Select loop range"), mem_fun(*this, &Editor::set_selection_from_loop)));
	select_items.push_back (MenuElem (_("Select punch range"), mem_fun(*this, &Editor::set_selection_from_punch)));
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

	cutnpaste_items.push_back (SeparatorElem());

	cutnpaste_items.push_back (MenuElem (_("New Region from range"), mem_fun(*this, &Editor::new_region_from_selection)));
	cutnpaste_items.push_back (MenuElem (_("Separate Range"), mem_fun(*this, &Editor::separate_region_from_selection)));

	edit_items.push_back (MenuElem (_("Edit"), *cutnpaste_menu));

	/* Adding new material */
	
	Menu *import_menu = manage (new Menu());
	MenuList& import_items = import_menu->items();
	import_menu->set_name ("ArdourContextMenu");
	
	import_items.push_back (MenuElem (_("Insert Region"), bind (mem_fun(*this, &Editor::insert_region_list_selection), 1.0f)));
	import_items.push_back (MenuElem (_("Insert external sndfile"), bind (mem_fun(*this, &Editor::insert_sndfile), false)));

	edit_items.push_back (MenuElem (_("Import"), *import_menu));

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
	
	play_items.push_back (MenuElem (_("Play from edit cursor")));
	play_items.push_back (MenuElem (_("Play from start"), mem_fun(*this, &Editor::play_from_start)));
	edit_items.push_back (MenuElem (_("Play"), *play_menu));

	/* Selection */

	Menu *select_menu = manage (new Menu);
	MenuList& select_items = select_menu->items();
	select_menu->set_name ("ArdourContextMenu");
	
	select_items.push_back (MenuElem (_("Select All in track"), bind (mem_fun(*this, &Editor::select_all_in_track), false)));
	select_items.push_back (MenuElem (_("Select All"), bind (mem_fun(*this, &Editor::select_all), false)));
	select_items.push_back (MenuElem (_("Invert in track"), mem_fun(*this, &Editor::invert_selection_in_track)));
	select_items.push_back (MenuElem (_("Invert"), mem_fun(*this, &Editor::invert_selection)));
	select_items.push_back (SeparatorElem());
	select_items.push_back (MenuElem (_("Select loop range"), mem_fun(*this, &Editor::set_selection_from_loop)));
	select_items.push_back (MenuElem (_("Select punch range"), mem_fun(*this, &Editor::set_selection_from_punch)));
	select_items.push_back (SeparatorElem());

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
	vector<string> txt = internationalize (snap_type_strings);
	snap_type_selector.get_entry()->set_text (txt[(int)st]);

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
	vector<string> txt = internationalize (snap_mode_strings);
	snap_mode_selector.get_entry()->set_text (txt[(int)mode]);

	instant_save ();
}

void
Editor::add_location_from_selection ()
{
	if (selection->time.empty()) {
		return;
	}

	if (session == 0 || clicked_trackview == 0) {
		return;
	}

	jack_nframes_t start = selection->time[clicked_selection].start;
	jack_nframes_t end = selection->time[clicked_selection].end;

	Location *location = new Location (start, end, "selection");

	session->begin_reversible_command (_("add marker"));
	session->add_undo (session->locations()->get_memento());
	session->locations()->add (location, true);
	session->add_redo_no_execute (session->locations()->get_memento());
	session->commit_reversible_command ();
}

void
Editor::add_location_from_playhead_cursor ()
{
	jack_nframes_t where = session->audible_frame();
	
	Location *location = new Location (where, where, "mark", Location::IsMark);
	session->begin_reversible_command (_("add marker"));
	session->add_undo (session->locations()->get_memento());
	session->locations()->add (location, true);
	session->add_redo_no_execute (session->locations()->get_memento());
	session->commit_reversible_command ();
}


int
Editor::set_state (const XMLNode& node)
{
	const XMLProperty* prop;
	XMLNode* geometry;
	int x, y, width, height, xoff, yoff;

	if ((geometry = find_named_node (node, "geometry")) == 0) {

		width = default_width;
		height = default_height;
		x = 1;
		y = 1;
		xoff = 0;
		yoff = 21;

	} else {

		width = atoi(geometry->property("x_size")->value());
		height = atoi(geometry->property("y_size")->value());
		x = atoi(geometry->property("x_pos")->value());
		y = atoi(geometry->property("y_pos")->value());
		xoff = atoi(geometry->property("x_off")->value());
		yoff = atoi(geometry->property("y_off")->value());
	}

	set_default_size(width, height);
	set_uposition(x, y-yoff);

	if ((prop = node.property ("zoom-focus"))) {
		set_zoom_focus ((ZoomFocus) atoi (prop->value()));
	}

	if ((prop = node.property ("zoom"))) {
		set_frames_per_unit (atof (prop->value()));
	}

	if ((prop = node.property ("snap-to"))) {
		set_snap_to ((SnapType) atoi (prop->value()));
	}

	if ((prop = node.property ("snap-mode"))) {
		set_snap_mode ((SnapMode) atoi (prop->value()));
	}

	if ((prop = node.property ("show-waveforms"))) {
		bool yn = (prop->value() == "yes");
		_show_waveforms = !yn;
		set_show_waveforms (yn);
	}

	if ((prop = node.property ("show-waveforms-recording"))) {
		bool yn = (prop->value() == "yes");
		_show_waveforms_recording = !yn;
		set_show_waveforms_recording (yn);
	}
	
	if ((prop = node.property ("show-measures"))) {
		bool yn = (prop->value() == "yes");
		_show_measures = !yn;
		set_show_measures (yn);
	}

	if ((prop = node.property ("follow-playhead"))) {
		bool yn = (prop->value() == "yes");
		_follow_playhead = !yn;
		set_follow_playhead (yn);
	}

	if ((prop = node.property ("xfades-visible"))) {
		bool yn = (prop->value() == "yes");
		_xfade_visibility = !yn;
		set_xfade_visibility (yn);
	}

	if ((prop = node.property ("region-list-sort-type"))) {
		region_list_sort_type = (Editing::RegionListSortType) -1; /* force change */
		reset_region_list_sort_type(str2regionlistsorttype(prop->value()));
	}

	if ((prop = node.property ("mouse-mode"))) {
		MouseMode m = str2mousemode(prop->value());
		mouse_mode = MouseMode ((int) m + 1); /* lie, force mode switch */
		set_mouse_mode (m, true);
	} else {
		mouse_mode = MouseGain; /* lie, to force the mode switch */
		set_mouse_mode (MouseObject, true);
	}

	if ((prop = node.property ("editor-mixer-button"))) {
		editor_mixer_button.set_active(prop->value() == "yes");
	}

	return 0;
}

XMLNode&
Editor::get_state ()
{
	XMLNode* node = new XMLNode ("Editor");
	char buf[32];

	if (is_realized()) {
		Gdk_Window win = get_window();
		
		int x, y, xoff, yoff, width, height;
		win.get_root_origin(x, y);
		win.get_position(xoff, yoff);
		win.get_size(width, height);
		
		XMLNode* geometry = new XMLNode ("geometry");
		char buf[32];
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
		snprintf(buf,sizeof(buf), "%d",gtk_paned_get_position (static_cast<Paned*>(&canvas_region_list_pane)->gobj()));
		geometry->add_property("canvas_region_list_pane_pos", string(buf));
		snprintf(buf,sizeof(buf), "%d", gtk_paned_get_position (static_cast<Paned*>(&track_list_canvas_pane)->gobj()));
		geometry->add_property("track_list_canvas_pane_pos", string(buf));
		snprintf(buf,sizeof(buf), "%d", gtk_paned_get_position (static_cast<Paned*>(&region_selection_vpane)->gobj()));
		geometry->add_property("region_selection_pane_pos", string(buf));
		snprintf(buf,sizeof(buf), "%d", gtk_paned_get_position (static_cast<Paned*>(&route_group_vpane)->gobj()));
		geometry->add_property("route_group_pane_pos", string(buf));

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

	node->add_property ("show-waveforms", _show_waveforms ? "yes" : "no");
	node->add_property ("show-waveforms-recording", _show_waveforms_recording ? "yes" : "no");
	node->add_property ("show-measures", _show_measures ? "yes" : "no");
	node->add_property ("follow-playhead", _follow_playhead ? "yes" : "no");
	node->add_property ("xfades-visible", _xfade_visibility ? "yes" : "no");
	node->add_property ("region-list-sort-type", enum2str(region_list_sort_type));
	node->add_property ("mouse-mode", enum2str(mouse_mode));
	node->add_property ("editor-mixer-button", editor_mixer_button.get_active() ? "yes" : "no");
	
	return *node;
}



TimeAxisView *
Editor::trackview_by_y_position (double y)
{
	TrackViewList::iterator iter;
	TimeAxisView *tv;

	for (iter = track_views.begin(); iter != track_views.end(); ++iter) {

		tv = *iter;

		if (tv->hidden()) {
			continue;
		}

		if (tv->y_position <= y && y < ((tv->y_position + tv->height + track_spacing))) {
			return tv;
		}
	}

	return 0;
}

void
Editor::snap_to (jack_nframes_t& start, int32_t direction, bool for_mark)
{
	Location* before = 0;
	Location* after = 0;

	if (!session) {
		return;
	}

	const jack_nframes_t one_second = session->frame_rate();
	const jack_nframes_t one_minute = session->frame_rate() * 60;

	jack_nframes_t presnap = start;

	switch (snap_type) {
	case SnapToFrame:
		break;

	case SnapToCDFrame:
		if (direction) {
			start = (jack_nframes_t) ceil ((double) start / (one_second / 75)) * (one_second / 75);
		} else {
			start = (jack_nframes_t) floor ((double) start / (one_second / 75)) * (one_second / 75);
		}
		break;
	case SnapToSMPTEFrame:
		if (direction) {
			start = (jack_nframes_t) (ceil ((double) start / session->frames_per_smpte_frame()) * session->frames_per_smpte_frame());
		} else {
			start = (jack_nframes_t) (floor ((double) start / session->frames_per_smpte_frame()) *  session->frames_per_smpte_frame());
		}
		break;

	case SnapToSMPTESeconds:
		if (session->smpte_offset_negative())
		{
			start += session->smpte_offset ();
		} else {
			start -= session->smpte_offset ();
		}    
		if (direction > 0) {
			start = (jack_nframes_t) ceil ((double) start / one_second) * one_second;
		} else {
			start = (jack_nframes_t) floor ((double) start / one_second) * one_second;
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
		if (direction) {
			start = (jack_nframes_t) ceil ((double) start / one_minute) * one_minute;
		} else {
			start = (jack_nframes_t) floor ((double) start / one_minute) * one_minute;
		}
		if (session->smpte_offset_negative())
		{
			start -= session->smpte_offset ();
		} else {
			start += session->smpte_offset ();
		}
		break;
		
	case SnapToSeconds:
		if (direction) {
			start = (jack_nframes_t) ceil ((double) start / one_second) * one_second;
		} else {
			start = (jack_nframes_t) floor ((double) start / one_second) * one_second;
		}
		break;
		
	case SnapToMinutes:
		if (direction) {
			start = (jack_nframes_t) ceil ((double) start / one_minute) * one_minute;
		} else {
			start = (jack_nframes_t) floor ((double) start / one_minute) * one_minute;
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
			vector<jack_nframes_t>::iterator i;

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
	nstring pixmap_path;
	vector<ToggleButton *> mouse_mode_buttons;

	mouse_mode_buttons.push_back (&mouse_move_button);
	mouse_mode_buttons.push_back (&mouse_select_button);
	mouse_mode_buttons.push_back (&mouse_gain_button);
	mouse_mode_buttons.push_back (&mouse_zoom_button);
	mouse_mode_buttons.push_back (&mouse_timefx_button);
	mouse_mode_buttons.push_back (&mouse_audition_button);
	mouse_mode_button_set = new GroupedButtons (mouse_mode_buttons);

	mouse_mode_button_table.set_homogeneous (true);
	mouse_mode_button_table.set_col_spacings (2);
	mouse_mode_button_table.set_row_spacings (2);
	mouse_mode_button_table.set_border_width (5);

	mouse_mode_button_table.attach (mouse_move_button, 0, 1, 0, 1);
	mouse_mode_button_table.attach (mouse_select_button, 1, 2, 0, 1);
	mouse_mode_button_table.attach (mouse_zoom_button, 2, 3, 0, 1);
 
	mouse_mode_button_table.attach (mouse_gain_button, 0, 1, 1, 2);
	mouse_mode_button_table.attach (mouse_timefx_button, 1, 2, 1, 2);
	mouse_mode_button_table.attach (mouse_audition_button, 2, 3, 1, 2);

	mouse_mode_tearoff = manage (new TearOff (mouse_mode_button_table));
	mouse_mode_tearoff->set_name ("MouseModeBase");

	mouse_mode_tearoff->Detach.connect (bind (mem_fun(*this, &Editor::detach_tearoff), static_cast<Gtk::Box*>(&toolbar_hbox), 
						  static_cast<Gtk::Widget*>(&mouse_mode_button_table)));
	mouse_mode_tearoff->Attach.connect (bind (mem_fun(*this, &Editor::reattach_tearoff), static_cast<Gtk::Box*> (&toolbar_hbox), 
						  static_cast<Gtk::Widget*> (&mouse_mode_button_table), 1));

	mouse_move_button.set_name ("MouseModeButton");
	mouse_select_button.set_name ("MouseModeButton");
	mouse_gain_button.set_name ("MouseModeButton");
	mouse_zoom_button.set_name ("MouseModeButton");
	mouse_timefx_button.set_name ("MouseModeButton");
	mouse_audition_button.set_name ("MouseModeButton");

	ARDOUR_UI::instance()->tooltips().set_tip (mouse_move_button, _("select/move objects"));
	ARDOUR_UI::instance()->tooltips().set_tip (mouse_select_button, _("select/move ranges"));
	ARDOUR_UI::instance()->tooltips().set_tip (mouse_gain_button, _("draw gain automation"));
	ARDOUR_UI::instance()->tooltips().set_tip (mouse_zoom_button, _("select zoom range"));
	ARDOUR_UI::instance()->tooltips().set_tip (mouse_timefx_button, _("stretch/shrink regions"));
	ARDOUR_UI::instance()->tooltips().set_tip (mouse_audition_button, _("listen to specific regions"));

	mouse_move_button.unset_flags (Gtk::CAN_FOCUS);
	mouse_select_button.unset_flags (Gtk::CAN_FOCUS);
	mouse_gain_button.unset_flags (Gtk::CAN_FOCUS);
	mouse_zoom_button.unset_flags (Gtk::CAN_FOCUS);
	mouse_timefx_button.unset_flags (Gtk::CAN_FOCUS);
	mouse_audition_button.unset_flags (Gtk::CAN_FOCUS);

	mouse_select_button.toggled.connect (bind (mem_fun(*this, &Editor::mouse_mode_toggled), Editing::MouseRange));
	mouse_select_button.button_release_event.connect (mem_fun(*this, &Editor::mouse_select_button_release));

	mouse_move_button.toggled.connect (bind (mem_fun(*this, &Editor::mouse_mode_toggled), Editing::MouseObject));
	mouse_gain_button.toggled.connect (bind (mem_fun(*this, &Editor::mouse_mode_toggled), Editing::MouseGain));
	mouse_zoom_button.toggled.connect (bind (mem_fun(*this, &Editor::mouse_mode_toggled), Editing::MouseZoom));
	mouse_timefx_button.toggled.connect (bind (mem_fun(*this, &Editor::mouse_mode_toggled), Editing::MouseTimeFX));
	mouse_audition_button.toggled.connect (bind (mem_fun(*this, &Editor::mouse_mode_toggled), Editing::MouseAudition));

	// mouse_move_button.set_active (true);

	/* automation control */

	global_automation_button.set_name ("MouseModeButton");
	automation_mode_button.set_name ("MouseModeButton");

	automation_box.set_spacing (2);
	automation_box.set_border_width (2);
	automation_box.pack_start (global_automation_button, false, false);
	automation_box.pack_start (automation_mode_button, false, false);

	/* Edit mode */

	edit_mode_label.set_name ("ToolBarLabel");

	edit_mode_selector.set_name ("EditModeSelector");
	edit_mode_selector.get_entry()->set_name ("EditModeSelector");
	edit_mode_selector.get_popwin()->set_name ("EditModeSelector");

	edit_mode_box.set_spacing (3);
	edit_mode_box.set_border_width (3);

	/* XXX another disgusting hack because of the way combo boxes size themselves */

	Gtkmm2ext::set_size_request_to_display_given_text (*edit_mode_selector.get_entry(), "EdgtMode", 2, 10);
	edit_mode_selector.set_popdown_strings (internationalize (edit_mode_strings));
	edit_mode_selector.set_value_in_list (true, false);

	edit_mode_box.pack_start (edit_mode_label, false, false);
	edit_mode_box.pack_start (edit_mode_selector, false, false);

	edit_mode_selector.get_popwin()->unmap_event.connect (mem_fun(*this, &Editor::edit_mode_selection_done));

	/* Snap Type */

	snap_type_label.set_name ("ToolBarLabel");

	snap_type_selector.set_name ("SnapTypeSelector");
	snap_type_selector.get_entry()->set_name ("SnapTypeSelector");
	snap_type_selector.get_popwin()->set_name ("SnapTypeSelector");

	snap_type_box.set_spacing (3);
	snap_type_box.set_border_width (3);

	/* XXX another disgusting hack because of the way combo boxes size themselves */

	const guint32 FUDGE = 10; // Combo's are stupid - they steal space from the entry for the button
	Gtkmm2ext::set_size_request_to_display_given_text (*snap_type_selector.get_entry(), "Region bounds", 2+FUDGE, 10);
	snap_type_selector.set_popdown_strings (internationalize (snap_type_strings));
	snap_type_selector.set_value_in_list (true, false);

	snap_type_box.pack_start (snap_type_label, false, false);
	snap_type_box.pack_start (snap_type_selector, false, false);

	snap_type_selector.get_popwin()->unmap_event.connect (mem_fun(*this, &Editor::snap_type_selection_done));

	/* Snap mode, not snap type */

	snap_mode_label.set_name ("ToolBarLabel");

	snap_mode_selector.set_name ("SnapModeSelector");
	snap_mode_selector.get_entry()->set_name ("SnapModeSelector");
	snap_mode_selector.get_popwin()->set_name ("SnapModeSelector");
	
	snap_mode_box.set_spacing (3);
	snap_mode_box.set_border_width (3);

	Gtkmm2ext::set_size_request_to_display_given_text (*snap_mode_selector.get_entry(), "SngpMode", 2, 10);
	snap_mode_selector.set_popdown_strings (internationalize (snap_mode_strings));
	snap_mode_selector.set_value_in_list (true, false);

	snap_mode_box.pack_start (snap_mode_label, false, false);
	snap_mode_box.pack_start (snap_mode_selector, false, false);

	snap_mode_selector.get_popwin()->unmap_event.connect (mem_fun(*this, &Editor::snap_mode_selection_done));

	/* Zoom focus mode */

	zoom_focus_label.set_name ("ToolBarLabel");

	zoom_focus_selector.set_name ("ZoomFocusSelector");
	zoom_focus_selector.get_entry()->set_name ("ZoomFocusSelector");
	zoom_focus_selector.get_popwin()->set_name ("ZoomFocusSelector");

	zoom_focus_box.set_spacing (3);
	zoom_focus_box.set_border_width (3);

	/* XXX another disgusting hack because of the way combo boxes size themselves */

	Gtkmm2ext::set_size_request_to_display_given_text (*zoom_focus_selector.get_entry(), "Edgt Cursor", 2, 10);
	zoom_focus_selector.set_popdown_strings (internationalize (zoom_focus_strings));
	zoom_focus_selector.set_value_in_list (true, false);

	zoom_focus_box.pack_start (zoom_focus_label, false, false);
	zoom_focus_box.pack_start (zoom_focus_selector, false, false);

	zoom_focus_selector.get_popwin()->unmap_event.connect (mem_fun(*this, &Editor::zoom_focus_selection_done));

	/* selection/cursor clocks */

	toolbar_selection_cursor_label.set_name ("ToolBarLabel");
	selection_start_clock_label.set_name ("ToolBarLabel");
	selection_end_clock_label.set_name ("ToolBarLabel");
	edit_cursor_clock_label.set_name ("ToolBarLabel");

	selection_start_clock_label.set_text (_("Start:"));
	selection_end_clock_label.set_text (_("End:"));
	edit_cursor_clock_label.set_text (_("Edit:"));

	toolbar_selection_clock_table.set_border_width (5);
	toolbar_selection_clock_table.set_col_spacings (2);
	toolbar_selection_clock_table.set_homogeneous (false);

//	toolbar_selection_clock_table.attach (selection_start_clock_label, 0, 1, 0, 1, 0, 0, 0, 0);
//	toolbar_selection_clock_table.attach (selection_end_clock_label, 1, 2, 0, 1, 0, 0, 0, 0);
	toolbar_selection_clock_table.attach (edit_cursor_clock_label, 2, 3, 0, 1, 0, 0, 0, 0);

//	toolbar_selection_clock_table.attach (selection_start_clock, 0, 1, 1, 2, 0, 0);
//	toolbar_selection_clock_table.attach (selection_end_clock, 1, 2, 1, 2, 0, 0);
	toolbar_selection_clock_table.attach (edit_cursor_clock, 2, 3, 1, 2, 0, 0);


//	toolbar_clock_vbox.set_spacing (2);
//	toolbar_clock_vbox.set_border_width (10);
	/* the editor/mixer button will be enabled at session connect */

	editor_mixer_button.set_active(false);
	editor_mixer_button.set_sensitive(false);

	HBox* hbox = new HBox;

	hbox->pack_start (editor_mixer_button, false, false);
	hbox->pack_start (toolbar_selection_clock_table, false, false);
	hbox->pack_start (zoom_indicator_vbox, false, false); 
	hbox->pack_start (zoom_focus_box, false, false);
	hbox->pack_start (snap_type_box, false, false);
	hbox->pack_start (snap_mode_box, false, false);
	hbox->pack_start (edit_mode_box, false, false);

	VBox *vbox = manage (new VBox);

	vbox->set_spacing (3);
	vbox->set_border_width (3);

	HBox *nbox = manage (new HBox);
	
	nudge_forward_button.signal_clicked().connect (bind (mem_fun(*this, &Editor::nudge_forward), false));
	nudge_backward_button.signal_clicked().connect (bind (mem_fun(*this, &Editor::nudge_backward), false));

	nbox->pack_start (nudge_backward_button, false, false);
	nbox->pack_start (nudge_forward_button, false, false);
	nbox->pack_start (nudge_clock, false, false, 5);

	nudge_label.set_name ("ToolBarLabel");

	vbox->pack_start (nudge_label, false, false);
	vbox->pack_start (*nbox, false, false);

	hbox->pack_start (*vbox, false, false);

	hbox->show_all ();

	tools_tearoff = new TearOff (*hbox);
	tools_tearoff->set_name ("MouseModeBase");

	tools_tearoff->Detach.connect (bind (mem_fun(*this, &Editor::detach_tearoff), static_cast<Gtk::Box*>(&toolbar_hbox), 
					     static_cast<Gtk::Widget*>(hbox)));
	tools_tearoff->Attach.connect (bind (mem_fun(*this, &Editor::reattach_tearoff), static_cast<Gtk::Box*> (&toolbar_hbox), 
					     static_cast<Gtk::Widget*> (hbox), 0));

	toolbar_hbox.set_spacing (8);
	toolbar_hbox.set_border_width (2);

	toolbar_hbox.pack_start (*tools_tearoff, false, false);
	toolbar_hbox.pack_start (*mouse_mode_tearoff, false, false);
	
	toolbar_base.set_name ("ToolBarBase");
	toolbar_base.add (toolbar_hbox);

	toolbar_frame.set_shadow_type (Gtk::SHADOW_OUT);
	toolbar_frame.set_name ("BaseFrame");
	toolbar_frame.add (toolbar_base);
}

gint
Editor::_autoscroll_canvas (void *arg)
{
	return ((Editor *) arg)->autoscroll_canvas ();
}

gint
Editor::autoscroll_canvas ()
{
	jack_nframes_t new_frame;
	bool keep_calling = true;

	if (autoscroll_direction < 0) {
		if (leftmost_frame < autoscroll_distance) {
			new_frame = 0;
		} else {
			new_frame = leftmost_frame - autoscroll_distance;
		}
 	} else {
		if (leftmost_frame > max_frames - autoscroll_distance) {
			new_frame = max_frames;
		} else {
			new_frame = leftmost_frame + autoscroll_distance;
		}
	}

	if (new_frame != leftmost_frame) {
		reposition_x_origin (new_frame);
	}

	if (new_frame == 0 || new_frame == max_frames) {
		/* we are done */
		return FALSE;
	}

	autoscroll_cnt++;

	if (autoscroll_cnt == 1) {

		/* connect the timeout so that we get called repeatedly */
		
		autoscroll_timeout_tag = gtk_timeout_add (100, _autoscroll_canvas, this);
		keep_calling = false;

	} else if (autoscroll_cnt > 10 && autoscroll_cnt < 20) {
		
		/* after about a while, speed up a bit by changing the timeout interval */

		autoscroll_timeout_tag = gtk_timeout_add (50, _autoscroll_canvas, this);
		keep_calling = false;
		
	} else if (autoscroll_cnt >= 20 && autoscroll_cnt < 30) {

		/* after about another while, speed up some more */

		autoscroll_timeout_tag = gtk_timeout_add (25, _autoscroll_canvas, this);
		keep_calling = false;

	} else if (autoscroll_cnt >= 30) {

		/* we've been scrolling for a while ... crank it up */

		autoscroll_distance = 10 * (jack_nframes_t) floor (canvas_width * frames_per_unit);
	}

	return keep_calling;
}

void
Editor::start_canvas_autoscroll (int dir)
{
	if (!session) {
		return;
	}

	stop_canvas_autoscroll ();

	autoscroll_direction = dir;
	autoscroll_distance = (jack_nframes_t) floor ((canvas_width * frames_per_unit)/10.0);
	autoscroll_cnt = 0;
	
	/* do it right now, which will start the repeated callbacks */
	
	autoscroll_canvas ();
}

void
Editor::stop_canvas_autoscroll ()
{
	if (autoscroll_timeout_tag >= 0) {
		gtk_timeout_remove (autoscroll_timeout_tag);
		autoscroll_timeout_tag = -1;
	}
}

int
Editor::convert_drop_to_paths (vector<string>& paths, 
			       GdkDragContext     *context,
			       gint                x,
			       gint                y,
			       GtkSelectionData   *data,
			       guint               info,
			       guint               time)			       

{	
	string spath;
	char *path;
	int state;
	gchar *tname = gdk_atom_name (data->type);

	if (session == 0 || strcmp (tname, "text/plain") != 0) {
		return -1;
	}

	/* Parse the "uri-list" format that Nautilus provides, 
	   where each pathname is delimited by \r\n
	*/

	path = (char *) data->data;
	state = 0;

	for (int n = 0; n < data->length; ++n) {

		switch (state) {
		case 0:
			if (path[n] == '\r') {
				state = 1;
			} else {
				spath += path[n];
			}
			break;
		case 1:
			if (path[n] == '\n') {
				paths.push_back (spath);
				spath = "";
				state = 0;
			} else {
				warning << _("incorrectly formatted URI list, ignored")
					<< endmsg;
				return -1;
			}
			break;
		}
	}

	/* nautilus and presumably some other file managers prefix even text/plain with file:// */
		
	for (vector<string>::iterator p = paths.begin(); p != paths.end(); ++p) {

		// cerr << "dropped text was " << *p << endl;

		url_decode (*p);

		// cerr << "decoded was " << *p << endl;

		if ((*p).substr (0,7) == "file://") {
			(*p) = (*p).substr (7);
		}
	}
	
	return 0;
}

void  
Editor::track_canvas_drag_data_received  (GdkDragContext     *context,
					  gint                x,
					  gint                y,
					  GtkSelectionData   *data,
					  guint               info,
					  guint               time)
{
	TimeAxisView* tvp;
	AudioTimeAxisView* tv;
	double cy;
	vector<string> paths;
	string spath;
	GdkEvent ev;
	jack_nframes_t frame;

	if (convert_drop_to_paths (paths, context, x, y, data, info, time)) {
		goto out;
	}

	/* D-n-D coordinates are window-relative, so convert to "world" coordinates
	 */

	double wx;
	double wy;

	gnome_canvas_window_to_world (GNOME_CANVAS(track_gnome_canvas), (double) x, (double) y, &wx, &wy);

	ev.type = GDK_BUTTON_RELEASE;
	ev.button.x = wx;
	ev.button.y = wy;

	frame = event_frame (&ev, 0, &cy);

	snap_to (frame);

	if ((tvp = trackview_by_y_position (cy)) == 0) {

		/* drop onto canvas background: create a new track */

		insert_paths_as_new_tracks (paths, false);

		
	} else if ((tv = dynamic_cast<AudioTimeAxisView*>(tvp)) != 0) {

		/* check that its an audio track, not a bus */

		if (tv->get_diskstream()) {

			for (vector<string>::iterator p = paths.begin(); p != paths.end(); ++p) {
				insert_sndfile_into (*p, true, tv, frame);
			}
		}

	}

  out:
	gtk_drag_finish (context, TRUE, FALSE, time);
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
	return bind (slot (*(const_cast<Editor*>(this)), &Editor::restore_state), state);
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
		UndoAction ua = get_memento();
		session->begin_reversible_command (name, &ua);
	}
}

void
Editor::commit_reversible_command ()
{
	if (session) {
		UndoAction ua = get_memento();
		session->commit_reversible_command (&ua);
	}
}

void
Editor::flush_track_canvas ()
{
	/* I don't think this is necessary, and only causes more problems.
	   I'm commenting it out
	   and if the imageframe folks don't have any issues, we can take
	   out this method entirely
	*/
	
	//gnome_canvas_update_now (GNOME_CANVAS(track_gnome_canvas));
	//gtk_main_iteration ();
}

void
Editor::set_selected_track_from_click (bool add, bool with_undo, bool no_remove)
{
	if (!clicked_trackview) {
		return;
	}

	if (with_undo) {
		begin_reversible_command (_("set selected trackview"));
	}

	if (add) {
		
		if (selection->selected (clicked_trackview)) {
			if (!no_remove) {
				selection->remove (clicked_trackview);
			}
		} else {
			selection->add (clicked_trackview);
		}
		
	} else {

		if (selection->selected (clicked_trackview) && selection->tracks.size() == 1) {
			/* no commit necessary */
			return;
		} 

		selection->set (clicked_trackview);
	}
	
	if (with_undo) {
		commit_reversible_command ();
	}
}

void
Editor::set_selected_control_point_from_click (bool add, bool with_undo, bool no_remove)
{
	if (!clicked_control_point) {
		return;
	}

	if (with_undo) {
		begin_reversible_command (_("set selected control point"));
	}

	if (add) {
		
	} else {

	}
	
	if (with_undo) {
		commit_reversible_command ();
	}
}

void
Editor::set_selected_regionview_from_click (bool add, bool no_track_remove)
{
	if (!clicked_regionview) {
		return;
	}

	AudioTimeAxisView* atv = dynamic_cast<AudioTimeAxisView*>(&clicked_regionview->get_time_axis_view());

	if (!atv) {
		return;
	}

	RouteGroup* group = atv->route().edit_group();
	vector<AudioRegionView*> all_equivalent_regions;
	
	if (group && group->is_active()) {

		for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {

			AudioTimeAxisView* tatv;

			if ((tatv = dynamic_cast<AudioTimeAxisView*> (*i)) != 0) {

				if (tatv->route().edit_group() != group) {
					continue;
				}
			
				AudioPlaylist* pl;
				vector<AudioRegion*> results;
				AudioRegionView* marv;
				DiskStream* ds;
				
				if ((ds = tatv->get_diskstream()) == 0) {
					/* bus */
					continue;
				}
				
				if ((pl = ds->playlist()) != 0) {
					pl->get_equivalent_regions (clicked_regionview->region, 
								    results);
				}
				
				for (vector<AudioRegion*>::iterator ir = results.begin(); ir != results.end(); ++ir) {
					if ((marv = tatv->view->find_view (**ir)) != 0) {
						all_equivalent_regions.push_back (marv);
					}
				}
				
			}
		}

	} else {

		all_equivalent_regions.push_back (clicked_regionview);

	}
	
	begin_reversible_command (_("set selected regionview"));
	
	if (add) {

		if (clicked_regionview->get_selected()) {
			if (group && group->is_active() && selection->audio_regions.size() > 1) {
				/* reduce selection down to just the one clicked */
				selection->set (clicked_regionview);
			} else {
				selection->remove (clicked_regionview);
			}
		} else {
			selection->add (all_equivalent_regions);
		}

		set_selected_track_from_click (add, false, no_track_remove);
		
	} else {

		// karsten wiese suggested these two lines to make
		// a selected region rise to the top. but this
		// leads to a mismatch between actual layering
		// and visual layering. resolution required ....
		//
		// gnome_canvas_item_raise_to_top (clicked_regionview->get_canvas_group());
		// gnome_canvas_item_raise_to_top (clicked_regionview->get_time_axis_view().canvas_display);

		if (clicked_regionview->get_selected()) {
			/* no commit necessary: we are the one selected. */
			return;

		} else {
			
			selection->set (all_equivalent_regions);
			set_selected_track_from_click (add, false, false);
		}
	}

	commit_reversible_command () ;
}

void
Editor::set_selected_regionview_from_region_list (Region& r, bool add)
{
	vector<AudioRegionView*> all_equivalent_regions;
	AudioRegion* region;

	if ((region = dynamic_cast<AudioRegion*>(&r)) == 0) {
		return;
	}

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		
		AudioTimeAxisView* tatv;
		
		if ((tatv = dynamic_cast<AudioTimeAxisView*> (*i)) != 0) {
			
			AudioPlaylist* pl;
			vector<AudioRegion*> results;
			AudioRegionView* marv;
			DiskStream* ds;
			
			if ((ds = tatv->get_diskstream()) == 0) {
				/* bus */
				continue;
			}

			if ((pl = ds->playlist()) != 0) {
				pl->get_region_list_equivalent_regions (*region, results);
			}
			
			for (vector<AudioRegion*>::iterator ir = results.begin(); ir != results.end(); ++ir) {
				if ((marv = tatv->view->find_view (**ir)) != 0) {
					all_equivalent_regions.push_back (marv);
				}
			}
			
		}
	}
	
	begin_reversible_command (_("set selected regions"));
	
	if (add) {

		selection->add (all_equivalent_regions);
		
	} else {

		selection->set (all_equivalent_regions);
	}

	commit_reversible_command () ;
}

gint
Editor::set_selected_regionview_from_map_event (GdkEventAny* ev, StreamView* sv, Region* r)
{
	AudioRegionView* rv;
	AudioRegion* ar;

	if ((ar = dynamic_cast<AudioRegion*> (r)) == 0) {
		return TRUE;
	}

	if ((rv = sv->find_view (*ar)) == 0) {
		return TRUE;
	}

	/* don't reset the selection if its something other than 
	   a single other region.
	*/

	if (selection->audio_regions.size() > 1) {
		return TRUE;
	}
	
	begin_reversible_command (_("set selected regions"));
	
	selection->set (rv);

	commit_reversible_command () ;

	return TRUE;
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
Editor::set_edit_menu (Menu& menu)
{
	edit_menu = &menu;
	edit_menu->map_.connect (mem_fun(*this, &Editor::edit_menu_map_handler));
}

void
Editor::edit_menu_map_handler ()
{
	using namespace Menu_Helpers;
	MenuList& edit_items = edit_menu->items();
	string label;

	/* Nuke all the old items */
		
	edit_items.clear ();

	if (session == 0) {
		return;
	}

	if (session->undo_depth() == 0) {
		label = _("Undo");
	} else {
		label = compose(_("Undo (%1)"), session->next_undo());
	}
	
	edit_items.push_back (MenuElem (label, bind (mem_fun(*this, &Editor::undo), 1U)));
	
	if (session->undo_depth() == 0) {
		edit_items.back()->set_sensitive (false);
	}
	
	if (session->redo_depth() == 0) {
		label = _("Redo");
	} else {
		label = compose(_("Redo (%1)"), session->next_redo());
	}
	
	edit_items.push_back (MenuElem (label, bind (mem_fun(*this, &Editor::redo), 1U)));
	if (session->redo_depth() == 0) {
		edit_items.back()->set_sensitive (false);
	}

	vector<MenuItem*> mitems;

	edit_items.push_back (SeparatorElem());
	edit_items.push_back (MenuElem (_("Cut"), mem_fun(*this, &Editor::cut)));
	mitems.push_back (edit_items.back());
	edit_items.push_back (MenuElem (_("Copy"), mem_fun(*this, &Editor::copy)));
	mitems.push_back (edit_items.back());
	edit_items.push_back (MenuElem (_("Paste"), bind (mem_fun(*this, &Editor::paste), 1.0f)));
	mitems.push_back (edit_items.back());
	edit_items.push_back (SeparatorElem());
	edit_items.push_back (MenuElem (_("Align"), bind (mem_fun(*this, &Editor::align), ARDOUR::SyncPoint)));
	mitems.push_back (edit_items.back());
	edit_items.push_back (MenuElem (_("Align Relative"), bind (mem_fun(*this, &Editor::align_relative), ARDOUR::SyncPoint)));
	mitems.push_back (edit_items.back());
	edit_items.push_back (SeparatorElem());

	if (selection->empty()) {
		for (vector<MenuItem*>::iterator i = mitems.begin(); i != mitems.end(); ++i) {
			(*i)->set_sensitive (false);
		}
	}

	Menu* import_menu = manage (new Menu());
	import_menu->set_name ("ArdourContextMenu");
	MenuList& import_items = import_menu->items();
	
	import_items.push_back (MenuElem (_("... as new track"), bind (mem_fun(*this, &Editor::import_audio), true)));
	import_items.push_back (MenuElem (_("... as new region"), bind (mem_fun(*this, &Editor::import_audio), false)));

	Menu* embed_menu = manage (new Menu());
	embed_menu->set_name ("ArdourContextMenu");
	MenuList& embed_items = embed_menu->items();

	embed_items.push_back (MenuElem (_("... as new track"), bind (mem_fun(*this, &Editor::insert_sndfile), true)));
	embed_items.push_back (MenuElem (_("... as new region"), mem_fun(*this, &Editor::embed_audio)));

	edit_items.push_back (MenuElem (_("Import audio (copy)"), *import_menu));
	edit_items.push_back (MenuElem (_("Embed audio (link)"), *embed_menu));
	edit_items.push_back (SeparatorElem());

	edit_items.push_back (MenuElem (_("Remove last capture"), mem_fun(*this, &Editor::remove_last_capture)));
	if (!session->have_captured()) {
		edit_items.back()->set_sensitive (false);
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
	HBox   hbox;
	HBox   button_box;
	Button ok_button (_("OK"));
	Button cancel_button (_("Cancel"));
	VBox   vbox;

	button_box.set_spacing (7);
	set_size_request_to_display_given_text (ok_button, _("Cancel"), 20, 15); // this is cancel on purpose
	set_size_request_to_display_given_text (cancel_button, _("Cancel"), 20, 15);
	button_box.pack_end (ok_button, false, false);
	button_box.pack_end (cancel_button, false, false);
	
	hbox.set_spacing (5);
	hbox.pack_start (label);
	hbox.pack_start (entry, true, true);
	
	vbox.set_spacing (5);
	vbox.set_border_width (5);
	vbox.pack_start (hbox);
	vbox.pack_start (button_box);

	win.add (vbox);
	win.set_position (Gtk::WIN_POS_MOUSE);
	win.show_all ();

	ok_button.signal_clicked().connect (bind (slot (win, &ArdourDialog::stop), 0));
	entry.activate.connect (bind (slot (win, &ArdourDialog::stop), 0));
	cancel_button.signal_clicked().connect (bind (slot (win, &ArdourDialog::stop), 1));

	entry.signal_focus_in_event().connect (slot (ARDOUR_UI::generic_focus_in_event));
	entry.signal_focus_out_event().connect (slot (ARDOUR_UI::generic_focus_out_event));
	
	entry.set_text ("1");
	set_size_request_to_display_given_text (entry, X_("12345678"), 20, 15);
	entry.select_region (0, entry.get_text_length());

	win.set_position (Gtk::WIN_POS_MOUSE);
	win.realize ();
	win.get_window().set_decorations (GdkWMDecoration (GDK_DECOR_BORDER|GDK_DECOR_RESIZEH));

	entry.grab_focus ();

	win.run ();

	if (win.run_status() != 0) {
		return;
	}

	string text = entry.get_text();
	float times;

	if (sscanf (text.c_str(), "%f", &times) == 1) {
		if (dup_region) {
			AudioRegionSelection regions;
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
	gnome_canvas_item_raise_to_top (verbose_canvas_cursor);
	gnome_canvas_item_show (verbose_canvas_cursor);
	verbose_cursor_visible = true;
}

void
Editor::hide_verbose_canvas_cursor ()
{
	gnome_canvas_item_hide (verbose_canvas_cursor);
	verbose_cursor_visible = false;
}

void
Editor::set_verbose_canvas_cursor (string txt, double x, double y)
{
	/* XXX get origin of canvas relative to root window,
	   add x and y and check compared to gdk_screen_{width,height}
	*/
	gnome_canvas_item_set (verbose_canvas_cursor, "text", txt.c_str(), "x", x, "y", y, NULL);	
}

void
Editor::set_verbose_canvas_cursor_text (string txt)
{
	gnome_canvas_item_set (verbose_canvas_cursor, "text", txt.c_str(), NULL);
}

gint
Editor::edit_mode_selection_done (GdkEventAny *ev)
{
	if (session == 0) {
		return FALSE;
	}

	string choice = edit_mode_selector.get_entry()->get_text();
	EditMode mode = Slide;

	if (choice == _("Splice")) {
		mode = Splice;
	} else if (choice == _("Slide")) {
		mode = Slide;
	}

	session->set_edit_mode (mode);

	return FALSE;
}	

gint
Editor::snap_type_selection_done (GdkEventAny *ev)
{
	if (session == 0) {
		return FALSE;
	}

	string choice = snap_type_selector.get_entry()->get_text();
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
	
	set_snap_to (snaptype);

	return FALSE;
}	

gint
Editor::snap_mode_selection_done (GdkEventAny *ev)
{
	if(session == 0) return FALSE;

	string choice = snap_mode_selector.get_entry()->get_text();
	SnapMode mode = SnapNormal;

	if (choice == _("Normal")) {
		mode = SnapNormal;
	} else if (choice == _("Magnetic")) {
		mode = SnapMagnetic;
	}

	set_snap_mode (mode);

	return FALSE;
}

gint
Editor::zoom_focus_selection_done (GdkEventAny *ev)
{
	if (session == 0) {
		return FALSE;
	}

	string choice = zoom_focus_selector.get_entry()->get_text();
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

	set_zoom_focus (focus_type);

	return FALSE;
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
}

void
Editor::region_selection_changed ()
{
	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		(*i)->set_selected_regionviews (selection->audio_regions);
	}
}

void
Editor::point_selection_changed ()
{
	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		(*i)->set_selected_points (selection->points);
	}
}

void
Editor::run_sub_event_loop ()
{
	Keyboard::the_keyboard().allow_focus (true);
	sub_event_loop_status = 0;
	Main::run ();
}

void
Editor::finish_sub_event_loop (int status)
{
	Main::quit ();
	Keyboard::the_keyboard().allow_focus (false);
	sub_event_loop_status = status;
}

gint
Editor::finish_sub_event_loop_on_delete (GdkEventAny *ignored, int32_t status)
{
	finish_sub_event_loop (status);
	return TRUE;
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
	if (zoom_focus != f) {
		zoom_focus = f;
		vector<string> txt = internationalize (zoom_focus_strings);
		zoom_focus_selector.get_entry()->set_text (txt[(int)f]);
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
Editor::pane_allocation_handler (GtkAllocation *alloc, Gtk::Paned* which)
{
	/* recover or initialize pane positions. do this here rather than earlier because
	   we don't want the positions to change the child allocations, which they seem to do.
	 */

	int pos;
	XMLProperty* prop;
	char buf[32];
	XMLNode* node = ARDOUR_UI::instance()->editor_settings();
	int width, height;
	static int32_t done[4] = { 0, 0, 0, 0 };
	XMLNode* geometry;

	if ((geometry = find_named_node (*node, "geometry")) == 0) {
		width = default_width;
		height = default_height;
	} else {
		width = atoi(geometry->property("x_size")->value());
		height = atoi(geometry->property("y_size")->value());
	}

	if (which == static_cast<Gtk::Paned*> (&track_list_canvas_pane)) {

		if (done[0]) {
			return;
		}

		if (!geometry || (prop = geometry->property("track_list_canvas_pane_pos")) == 0) {
			pos = 75;
			snprintf (buf, sizeof(buf), "%d", pos);
		} else {
			pos = atoi (prop->value());
		}

		if ((done[0] = GTK_WIDGET(track_list_canvas_pane.gobj())->allocation.width > pos)) {
			track_list_canvas_pane.set_position (pos);
		}

	} else if (which == static_cast<Gtk::Paned*> (&canvas_region_list_pane)) {

		if (done[1]) {
			return;
		}

		if (!geometry || (prop = geometry->property("canvas_region_list_pane_pos")) == 0) {
			pos = width - (95 * 2);
			snprintf (buf, sizeof(buf), "%d", pos);
		} else {
			pos = atoi (prop->value());
		}

		if ((done[1] = GTK_WIDGET(canvas_region_list_pane.gobj())->allocation.width > pos)) {
			canvas_region_list_pane.set_position (pos);
		}

	} else if (which == static_cast<Gtk::Paned*> (&route_group_vpane)) {

		if (done[2]) {
			return;
		}

		if (!geometry || (prop = geometry->property("route_group_pane_pos")) == 0) {
			pos = width - (95 * 2);
			snprintf (buf, sizeof(buf), "%d", pos);
		} else {
			pos = atoi (prop->value());
		}

		if ((done[2] = GTK_WIDGET(route_group_vpane.gobj())->allocation.height > pos)) {
			route_group_vpane.set_position (pos);
		}

	} else if (which == static_cast<Gtk::Paned*> (&region_selection_vpane)) {

		if (done[3]) {
			return;
		}

		if (!geometry || (prop = geometry->property("region_selection_pane_pos")) == 0) {
			pos = width - (95 * 2);
			snprintf (buf, sizeof(buf), "%d", pos);
		} else {
			pos = atoi (prop->value());
		}

		if ((done[3] = GTK_WIDGET(region_selection_vpane.gobj())->allocation.height > pos)) { 
			region_selection_vpane.set_position (pos);
		}
	}
}

void
Editor::detach_tearoff (Gtk::Box* b, Gtk::Widget* w)
{
	if (tools_tearoff->torn_off() && 
	    mouse_mode_tearoff->torn_off()) {
		top_hbox.remove (toolbar_frame);
	}
	
	ensure_float (*w->get_toplevel());
}

void
Editor::reattach_tearoff (Gtk::Box* b, Gtk::Widget* w, int32_t n)
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
Editor::toggle_xfade_active (Crossfade* xfade)
{
	xfade->set_active (!xfade->active());
}

void
Editor::toggle_xfade_length (Crossfade* xfade)
{
	xfade->set_follow_overlap (!xfade->following_overlap());
}

void
Editor::edit_xfade (Crossfade* xfade)
{
	CrossfadeEditor cew (*session, *xfade, xfade->fade_in().get_min_y(), 1.0);
		
	ensure_float (cew);
	
	cew.ok_button.signal_clicked().connect (bind (slot (cew, &ArdourDialog::stop), 1));
	cew.cancel_button.signal_clicked().connect (bind (slot (cew, &ArdourDialog::stop), 0));
	cew.delete_event.connect (slot (cew, &ArdourDialog::wm_doi_event_stop));

	cew.run ();
	
	if (cew.run_status() == 1) {
		cew.apply ();
		xfade->StateChanged (Change (~0));
	}
}

PlaylistSelector&
Editor::playlist_selector () const
{
	return *_playlist_selector;
}

jack_nframes_t
Editor::get_nudge_distance (jack_nframes_t pos, jack_nframes_t& next)
{
	jack_nframes_t ret;

	ret = nudge_clock.current_duration (pos);
	next = ret + 1; /* XXXX fix me */

	return ret;
}

void
Editor::end_location_changed (Location* location)
{
	ENSURE_GUI_THREAD (bind (mem_fun(*this, &Editor::end_location_changed), location));
	track_canvas_scroller.get_hadjustment()->set_upper (location->end() / frames_per_unit);
}

int
Editor::playlist_deletion_dialog (Playlist* pl)
{
	ArdourDialog dialog ("playlist deletion dialog");
	Label  label (compose (_("Playlist %1 is currently unused.\n"
				 "If left alone, no audio files used by it will be cleaned.\n"
				 "If deleted, audio files used by it alone by will cleaned."),
			       pl->name()));
	HBox   button_box;
	Button del_button (_("Delete playlist"));
	Button keep_button (_("Keep playlist"));
	Button abort_button (_("Cancel cleanup"));
	VBox   vbox;

	button_box.set_spacing (7);
	button_box.set_homogeneous (true);
	button_box.pack_end (del_button, false, false);
	button_box.pack_end (keep_button, false, false);
	button_box.pack_end (abort_button, false, false);
	
	vbox.set_spacing (5);
	vbox.set_border_width (5);
	vbox.pack_start (label);
	vbox.pack_start (button_box);

	dialog.add (vbox);
	dialog.set_position (GTK_WIN_POS_CENTER);
	dialog.show_all ();

	del_button.signal_clicked().connect (bind (slot (dialog, &ArdourDialog::stop), 0));
	keep_button.signal_clicked().connect (bind (slot (dialog, &ArdourDialog::stop), 1));
	abort_button.signal_clicked().connect (bind (slot (dialog, &ArdourDialog::stop), 2));

	dialog.realize ();
	dialog.get_window().set_decorations (GdkWMDecoration (GDK_DECOR_BORDER|GDK_DECOR_RESIZEH));

	dialog.run ();

	switch (dialog.run_status()) {
	case 1:
		/* keep the playlist */
		return 1;
		break;
	case 0:
		/* delete the playlist */
		return 0;
		break;
	case 2:
		/* abort cleanup */
		return -1;
		break;
	default:
		/* keep the playlist */
		return 1;
	}
}

bool
Editor::audio_region_selection_covers (jack_nframes_t where)
{
	for (AudioRegionSelection::iterator a = selection->audio_regions.begin(); a != selection->audio_regions.end(); ++a) {
		if ((*a)->region.covers (where)) {
			return true;
		}
	}

	return false;
}

void
Editor::prepare_for_cleanup ()
{
	cut_buffer->clear_audio_regions ();
	cut_buffer->clear_playlists ();

	selection->clear_audio_regions ();
	selection->clear_playlists ();
}

void
Editor::init_colormap ()
{
	for (size_t x = 0; x < sizeof (color_id_strs) / sizeof (color_id_strs[0]); ++x) {
		pair<ColorID,int> newpair;
		
		newpair.first = (ColorID) x;
		newpair.second = rgba_from_style (enum2str (newpair.first), 0, 0, 0, 255);
		color_map.insert (newpair);
	}
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
