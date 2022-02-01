/*
 * Copyright (C) 2005-2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2005-2009 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2005-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2006-2009 Sampo Savolainen <v2@iki.fi>
 * Copyright (C) 2006-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2017 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013-2015 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2014-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2014-2019 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015 André Nusser <andre.nusser@googlemail.com>
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

/* Note: public Editor methods are documented in public_editor.h */

#include <stdint.h>
#include <unistd.h>
#include <cstdlib>
#include <cmath>
#include <string>
#include <algorithm>
#include <map>

#include "ardour_ui.h"
/*
 * ardour_ui.h include was moved to the top of the list
 * due to a conflicting definition of 'Style' between
 * Apple's MacTypes.h and BarController.
 */

#include <boost/none.hpp>

#include <sigc++/bind.h>

#include "pbd/convert.h"
#include "pbd/error.h"
#include "pbd/enumwriter.h"
#include "pbd/memento_command.h"
#include "pbd/unknown_type.h"
#include "pbd/unwind.h"
#include "pbd/timersub.h"

#include <glibmm/datetime.h> /*for playlist group_id */
#include <glibmm/miscutils.h>
#include <glibmm/uriutils.h>
#include <gtkmm/image.h>
#include <gdkmm/color.h>
#include <gdkmm/bitmap.h>

#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>

#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/window_title.h"
#include "gtkmm2ext/cell_renderer_pixbuf_toggle.h"

#include "ardour/analysis_graph.h"
#include "ardour/audio_track.h"
#include "ardour/audioengine.h"
#include "ardour/audioregion.h"
#include "ardour/lmath.h"
#include "ardour/location.h"
#include "ardour/profile.h"
#include "ardour/route.h"
#include "ardour/route_group.h"
#include "ardour/session_playlists.h"
#include "ardour/tempo.h"
#include "ardour/utils.h"
#include "ardour/vca_manager.h"
#include "ardour/vca.h"

#include "canvas/debug.h"
#include "canvas/note.h"
#include "canvas/text.h"

#include "widgets/ardour_spacer.h"
#include "widgets/eventboxext.h"
#include "widgets/tooltips.h"
#include "widgets/prompter.h"

#include "control_protocol/control_protocol.h"

#include "actions.h"
#include "analysis_window.h"
#include "ardour_message.h"
#include "audio_clock.h"
#include "audio_region_view.h"
#include "audio_streamview.h"
#include "audio_time_axis.h"
#include "automation_time_axis.h"
#include "bundle_manager.h"
#include "crossfade_edit.h"
#include "debug.h"
#include "editing.h"
#include "editing_convert.h"
#include "editor.h"
#include "editor_cursors.h"
#include "editor_drag.h"
#include "editor_group_tabs.h"
#include "editor_locations.h"
#include "editor_regions.h"
#include "editor_route_groups.h"
#include "editor_routes.h"
#include "editor_snapshots.h"
#include "editor_sources.h"
#include "editor_summary.h"
#include "enums_convert.h"
#include "export_report.h"
#include "global_port_matrix.h"
#include "gui_object.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "luainstance.h"
#include "marker.h"
#include "midi_region_view.h"
#include "midi_time_axis.h"
#include "mixer_strip.h"
#include "mixer_ui.h"
#include "mouse_cursors.h"
#include "note_base.h"
#include "plugin_setup_dialog.h"
#include "public_editor.h"
#include "quantize_dialog.h"
#include "region_peak_cursor.h"
#include "region_layering_order_editor.h"
#include "rgb_macros.h"
#include "rhythm_ferret.h"
#include "route_sorter.h"
#include "selection.h"
#include "selection_properties_box.h"
#include "simple_progress_dialog.h"
#include "sfdb_ui.h"
#include "grid_lines.h"
#include "time_axis_view.h"
#include "timers.h"
#include "ui_config.h"
#include "utils.h"
#include "vca_time_axis.h"
#include "verbose_cursor.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace ArdourWidgets;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Gtk;
using namespace Glib;
using namespace Gtkmm2ext;
using namespace Editing;
using namespace Temporal;

using PBD::internationalize;
using PBD::atoi;
using Gtkmm2ext::Keyboard;

double Editor::timebar_height = 15.0;

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

static const gchar *_edit_point_strings[] = {
	N_("Playhead"),
	N_("Marker"),
	N_("Mouse"),
	0
};

static const gchar *_edit_mode_strings[] = {
	N_("Slide"),
	N_("Ripple"),
	N_("Ripple All"),
	N_("Lock"),
	0
};

static const gchar *_zoom_focus_strings[] = {
	N_("Left"),
	N_("Right"),
	N_("Center"),
	N_("Playhead"),
	N_("Mouse"),
	N_("Edit point"),
	0
};

#ifdef USE_RUBBERBAND
static const gchar *_rb_opt_strings[] = {
	N_("Mushy"),
	N_("Smooth"),
	N_("Balanced multitimbral mixture"),
	N_("Unpitched percussion with stable notes"),
	N_("Crisp monophonic instrumental"),
	N_("Unpitched solo percussion"),
	N_("Resample without preserving pitch"),
#ifdef HAVE_SOUNDTOUCH
	N_("Vocal"),
#endif
	0
};
#endif

/* Robin says: this should be odd to accommodate cairo drawing offset (width/2 rounds up to pixel boundary) */
#ifdef __APPLE__
#define COMBO_TRIANGLE_WIDTH 19 // ArdourButton _diameter (11) + 2 * arrow-padding (2*2) + 2 * text-padding (2*5)
#else
#define COMBO_TRIANGLE_WIDTH 11 // as-measured for win/linux.
#endif

Editor::Editor ()
	: PublicEditor (global_hpacker)
	, editor_mixer_strip_width (Wide)
	, constructed (false)
	, _properties_box (0)
	, no_save_visual (false)
	, _leftmost_sample (0)
	, samples_per_pixel (2048)
	, zoom_focus (ZoomFocusPlayhead)
	, mouse_mode (MouseObject)
	, pre_internal_grid_type (GridTypeBeat)
	, pre_internal_snap_mode (SnapOff)
	, internal_grid_type (GridTypeBeat)
	, internal_snap_mode (SnapOff)
	, _join_object_range_state (JOIN_OBJECT_RANGE_NONE)
	, _notebook_shrunk (false)
	, location_marker_color (0)
	, location_range_color (0)
	, location_loop_color (0)
	, location_punch_color (0)
	, location_cd_marker_color (0)
	, entered_marker (0)
	, _show_marker_lines (false)
	, clicked_axisview (0)
	, clicked_routeview (0)
	, clicked_regionview (0)
	, clicked_selection (0)
	, clicked_control_point (0)
	, button_release_can_deselect (true)
	, _mouse_changed_selection (false)
	, _popup_region_menu_item (0)
	, _track_canvas (0)
	, _track_canvas_viewport (0)
	, within_track_canvas (false)
	, _verbose_cursor (0)
	, _region_peak_cursor (0)
	, tempo_group (0)
	, meter_group (0)
	, marker_group (0)
	, range_marker_group (0)
	, transport_marker_group (0)
	, cd_marker_group (0)
	, _time_markers_group (0)
	, hv_scroll_group (0)
	, h_scroll_group (0)
	, cursor_scroll_group (0)
	, no_scroll_group (0)
	, _trackview_group (0)
	, _drag_motion_group (0)
	, _canvas_drop_zone (0)
	, no_ruler_shown_update (false)
	,  ruler_grabbed_widget (0)
	, ruler_dialog (0)
	, minsec_mark_interval (0)
	, minsec_mark_modulo (0)
	, minsec_nmarks (0)
	, timecode_ruler_scale (timecode_show_many_hours)
	, timecode_mark_modulo (0)
	, timecode_nmarks (0)
	, _samples_ruler_interval (0)
	, bbt_ruler_scale (bbt_show_many)
	, bbt_bars (0)
	, bbt_nmarks (0)
	, bbt_bar_helper_on (0)
	, timecode_ruler (0)
	, bbt_ruler (0)
	, samples_ruler (0)
	, minsec_ruler (0)
	, visible_timebars (0)
	, editor_ruler_menu (0)
	, tempo_bar (0)
	, meter_bar (0)
	, marker_bar (0)
	, range_marker_bar (0)
	, transport_marker_bar (0)
	, cd_marker_bar (0)
	, minsec_label (_("Mins:Secs"))
	, bbt_label (_("Bars:Beats"))
	, timecode_label (_("Timecode"))
	, samples_label (_("Samples"))
	, tempo_label (_("Tempo"))
	, meter_label (_("Meter"))
	, mark_label (_("Location Markers"))
	, range_mark_label (_("Range Markers"))
	, transport_mark_label (_("Loop/Punch Ranges"))
	, cd_mark_label (_("CD Markers"))
	, cue_mark_label (_("Cue Markers"))
	, videotl_label (_("Video Timeline"))
	, videotl_group (0)
	, _region_boundary_cache_dirty (true)
	, edit_packer (4, 4, true)
	, vertical_adjustment (0.0, 0.0, 10.0, 400.0)
	, horizontal_adjustment (0.0, 0.0, 1e16)
	, unused_adjustment (0.0, 0.0, 10.0, 400.0)
	, controls_layout (unused_adjustment, vertical_adjustment)
	, _scroll_callbacks (0)
	, _visible_canvas_width (0)
	, _visible_canvas_height (0)
	, _full_canvas_height (0)
	, edit_controls_left_menu (0)
	, edit_controls_right_menu (0)
	, visual_change_queued(false)
	, _tvl_no_redisplay(false)
	, _tvl_redisplay_on_resume(false)
	, _last_update_time (0)
	, _err_screen_engine (0)
	, cut_buffer_start (0)
	, cut_buffer_length (0)
	, button_bindings (0)
	, last_paste_pos (timepos_t::max (Temporal::AudioTime)) /* XXX NUTEMPO how to choose time domain */
	, paste_count (0)
	, sfbrowser (0)
	, current_interthread_info (0)
	, analysis_window (0)
	, select_new_marker (false)
	, last_scrub_x (0)
	, scrubbing_direction (0)
	, scrub_reversals (0)
	, scrub_reverse_distance (0)
	, have_pending_keyboard_selection (false)
	, pending_keyboard_selection_start (0)
	, _grid_type (GridTypeBeat)
	, _snap_mode (SnapOff)
	, _draw_length (GridTypeNone)
	, _draw_velocity (DRAW_VEL_AUTO)
	, _draw_channel (DRAW_CHAN_AUTO)
	, ignore_gui_changes (false)
	, _drags (new DragManager (this))
	, lock_dialog (0)
	  /* , last_event_time { 0, 0 } */ /* this initialization style requires C++11 */
	, _dragging_playhead (false)
	, _follow_playhead (true)
	, _stationary_playhead (false)
	, _maximised (false)
	, grid_lines (0)
	, global_rect_group (0)
	, time_line_group (0)
	, tempo_marker_menu (0)
	, meter_marker_menu (0)
	, marker_menu (0)
	, range_marker_menu (0)
	, new_transport_marker_menu (0)
	, marker_menu_item (0)
	, _visible_track_count (-1)
	,  toolbar_selection_clock_table (2,3)
	,  automation_mode_button (_("mode"))
	, selection (new Selection (this, true))
	, cut_buffer (new Selection (this, false))
	, _selection_memento (new SelectionMemento())
	, _all_region_actions_sensitized (false)
	, _ignore_region_action (false)
	, _last_region_menu_was_main (false)
	, _track_selection_change_without_scroll (false)
	, _editor_track_selection_change_without_scroll (false)
	, _playhead_cursor (0)
	, _snapped_cursor (0)
	, cd_marker_bar_drag_rect (0)
	, cue_marker_bar_drag_rect (0)
	, range_bar_drag_rect (0)
	, transport_bar_drag_rect (0)
	, transport_bar_range_rect (0)
	, transport_bar_preroll_rect (0)
	, transport_bar_postroll_rect (0)
	, transport_loop_range_rect (0)
	, transport_punch_range_rect (0)
	, transport_punchin_line (0)
	, transport_punchout_line (0)
	, transport_preroll_rect (0)
	, transport_postroll_rect (0)
	, temp_location (0)
	, rubberband_rect (0)
	, _route_groups (0)
	, _routes (0)
	, _regions (0)
	, _snapshots (0)
	, _locations (0)
	, autoscroll_horizontal_allowed (false)
	, autoscroll_vertical_allowed (false)
	, autoscroll_cnt (0)
	, autoscroll_widget (0)
	, show_gain_after_trim (false)
	, selection_op_cmd_depth (0)
	, selection_op_history_it (0)
	, no_save_instant (false)
	, current_timefx (0)
	, current_mixer_strip (0)
	, show_editor_mixer_when_tracks_arrive (false)
	,  nudge_clock (new AudioClock (X_("nudge"), false, X_("nudge"), true, false, true))
	, current_stepping_trackview (0)
	, last_track_height_step_timestamp (0)
	, entered_track (0)
	, entered_regionview (0)
	, clear_entered_track (false)
	, _edit_point (EditAtMouse)
	, meters_running (false)
	, rhythm_ferret (0)
	, _have_idled (false)
	, resize_idle_id (-1)
	, _pending_resize_amount (0)
	, _pending_resize_view (0)
	, _pending_locate_request (false)
	, _pending_initial_locate (false)
	, _summary (0)
	, _group_tabs (0)
	, _last_motion_y (0)
	, layering_order_editor (0)
	, _last_cut_copy_source_track (0)
	, _region_selection_change_updates_region_list (true)
	, _cursors (0)
	, _following_mixer_selection (false)
	, _show_touched_automation (false)
	, _control_point_toggled_on_press (false)
	, _stepping_axis_view (0)
	, quantize_dialog (0)
	, _main_menu_disabler (0)
{
	/* we are a singleton */

	PublicEditor::_instance = this;

	_have_idled = false;

	last_event_time.tv_sec = 0;
	last_event_time.tv_usec = 0;

	selection_op_history.clear();
	before.clear();

	grid_type_strings =  I18N (_grid_type_strings);
	zoom_focus_strings = I18N (_zoom_focus_strings);
	edit_mode_strings = I18N (_edit_mode_strings);
	edit_point_strings = I18N (_edit_point_strings);
#ifdef USE_RUBBERBAND
	rb_opt_strings = I18N (_rb_opt_strings);
	rb_current_opt = 4;
#endif

	build_edit_mode_menu();
	build_zoom_focus_menu();
	build_track_count_menu();
	build_grid_type_menu();
	build_edit_point_menu();

	location_marker_color = UIConfiguration::instance().color ("location marker");
	location_range_color = UIConfiguration::instance().color ("location range");
	location_cd_marker_color = UIConfiguration::instance().color ("location cd marker");
	location_loop_color = UIConfiguration::instance().color ("location loop");
	location_punch_color = UIConfiguration::instance().color ("location punch");

	timebar_height = std::max (12., ceil (15. * UIConfiguration::instance().get_ui_scale()));

	TimeAxisView::setup_sizes ();
	ArdourMarker::setup_sizes (timebar_height);
	TempoCurve::setup_sizes (timebar_height);

	bbt_label.set_name ("EditorRulerLabel");
	bbt_label.set_size_request (-1, (int)timebar_height);
	bbt_label.set_alignment (1.0, 0.5);
	bbt_label.set_padding (5,0);
	bbt_label.hide ();
	bbt_label.set_no_show_all();
	minsec_label.set_name ("EditorRulerLabel");
	minsec_label.set_size_request (-1, (int)timebar_height);
	minsec_label.set_alignment (1.0, 0.5);
	minsec_label.set_padding (5,0);
	minsec_label.hide ();
	minsec_label.set_no_show_all();
	timecode_label.set_name ("EditorRulerLabel");
	timecode_label.set_size_request (-1, (int)timebar_height);
	timecode_label.set_alignment (1.0, 0.5);
	timecode_label.set_padding (5,0);
	timecode_label.hide ();
	timecode_label.set_no_show_all();
	samples_label.set_name ("EditorRulerLabel");
	samples_label.set_size_request (-1, (int)timebar_height);
	samples_label.set_alignment (1.0, 0.5);
	samples_label.set_padding (5,0);
	samples_label.hide ();
	samples_label.set_no_show_all();

	tempo_label.set_name ("EditorRulerLabel");
	tempo_label.set_size_request (-1, (int)timebar_height);
	tempo_label.set_alignment (1.0, 0.5);
	tempo_label.set_padding (5,0);
	tempo_label.hide();
	tempo_label.set_no_show_all();

	meter_label.set_name ("EditorRulerLabel");
	meter_label.set_size_request (-1, (int)timebar_height);
	meter_label.set_alignment (1.0, 0.5);
	meter_label.set_padding (5,0);
	meter_label.hide();
	meter_label.set_no_show_all();

	mark_label.set_name ("EditorRulerLabel");
	mark_label.set_size_request (-1, (int)timebar_height);
	mark_label.set_alignment (1.0, 0.5);
	mark_label.set_padding (5,0);
	mark_label.hide();
	mark_label.set_no_show_all();

	cd_mark_label.set_name ("EditorRulerLabel");
	cd_mark_label.set_size_request (-1, (int)timebar_height);
	cd_mark_label.set_alignment (1.0, 0.5);
	cd_mark_label.set_padding (5,0);
	cd_mark_label.hide();
	cd_mark_label.set_no_show_all();

	cue_mark_label.set_name ("EditorRulerLabel");
	cue_mark_label.set_size_request (-1, (int)timebar_height);
	cue_mark_label.set_alignment (1.0, 0.5);
	cue_mark_label.set_padding (5,0);
	cue_mark_label.hide();
	cue_mark_label.set_no_show_all();

	videotl_bar_height = 4;
	videotl_label.set_name ("EditorRulerLabel");
	videotl_label.set_size_request (-1, (int)timebar_height * videotl_bar_height);
	videotl_label.set_alignment (1.0, 0.5);
	videotl_label.set_padding (5,0);
	videotl_label.hide();
	videotl_label.set_no_show_all();

	range_mark_label.set_name ("EditorRulerLabel");
	range_mark_label.set_size_request (-1, (int)timebar_height);
	range_mark_label.set_alignment (1.0, 0.5);
	range_mark_label.set_padding (5,0);
	range_mark_label.hide();
	range_mark_label.set_no_show_all();

	transport_mark_label.set_name ("EditorRulerLabel");
	transport_mark_label.set_size_request (-1, (int)timebar_height);
	transport_mark_label.set_alignment (1.0, 0.5);
	transport_mark_label.set_padding (5,0);
	transport_mark_label.hide();
	transport_mark_label.set_no_show_all();

	initialize_canvas ();

	CairoWidget::set_focus_handler (sigc::mem_fun (ARDOUR_UI::instance(), &ARDOUR_UI::reset_focus));

	_summary = new EditorSummary (this);

	TempoMap::MapChanged.connect (tempo_map_connection, invalidator (*this), boost::bind (&Editor::tempo_map_changed, this), gui_context());

	selection->TimeChanged.connect (sigc::mem_fun(*this, &Editor::time_selection_changed));
	selection->TracksChanged.connect (sigc::mem_fun(*this, &Editor::track_selection_changed));

	editor_regions_selection_changed_connection = selection->RegionsChanged.connect (sigc::mem_fun(*this, &Editor::region_selection_changed));

	selection->PointsChanged.connect (sigc::mem_fun(*this, &Editor::point_selection_changed));
	selection->MarkersChanged.connect (sigc::mem_fun(*this, &Editor::marker_selection_changed));

	edit_controls_vbox.set_spacing (0);
	vertical_adjustment.signal_value_changed().connect (sigc::mem_fun(*this, &Editor::tie_vertical_scrolling), true);
	_track_canvas->signal_map_event().connect (sigc::mem_fun (*this, &Editor::track_canvas_map_handler));

	HBox* h = manage (new HBox);
	_group_tabs = new EditorGroupTabs (this);
	h->pack_start (*_group_tabs, PACK_SHRINK);
	h->pack_start (edit_controls_vbox);
	controls_layout.add (*h);

	controls_layout.set_name ("EditControlsBase");
	controls_layout.add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK|Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK|Gdk::SCROLL_MASK);
	controls_layout.signal_button_press_event().connect (sigc::mem_fun(*this, &Editor::edit_controls_button_event));
	controls_layout.signal_button_release_event().connect (sigc::mem_fun(*this, &Editor::edit_controls_button_event));
	controls_layout.signal_scroll_event().connect (sigc::mem_fun(*this, &Editor::control_layout_scroll), false);

	_cursors = new MouseCursors;
	_cursors->set_cursor_set (UIConfiguration::instance().get_icon_set());
	cerr << "Set cursor set to " << UIConfiguration::instance().get_icon_set() << endl;

	/* Push default cursor to ever-present bottom of cursor stack. */
	push_canvas_cursor(_cursors->grabber);

	ArdourCanvas::GtkCanvas* time_pad = manage (new ArdourCanvas::GtkCanvas ());

	ArdourCanvas::Line* pad_line_1 = new ArdourCanvas::Line (time_pad->root());
	pad_line_1->set (ArdourCanvas::Duple (0.0, 1.0), ArdourCanvas::Duple (100.0, 1.0));
	pad_line_1->set_outline_color (0xFF0000FF);
	pad_line_1->show();

	/* CAIROCANVAS */
	time_pad->show();

	edit_packer.set_col_spacings (0);
	edit_packer.set_row_spacings (0);
	edit_packer.set_homogeneous (false);
	edit_packer.set_border_width (0);
	edit_packer.set_name ("EditorWindow");

	time_bars_event_box.add (time_bars_vbox);
	time_bars_event_box.set_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);
	time_bars_event_box.signal_button_release_event().connect (sigc::mem_fun(*this, &Editor::ruler_label_button_release));

	ArdourWidgets::ArdourDropShadow *axis_view_shadow = manage (new (ArdourWidgets::ArdourDropShadow));
	axis_view_shadow->set_size_request (4, -1);
	axis_view_shadow->set_name("EditorWindow");
	axis_view_shadow->show();

	edit_packer.attach (*axis_view_shadow,     0, 1, 0, 2,    FILL,        FILL|EXPAND, 0, 0);

	/* labels for the time bars */
	edit_packer.attach (time_bars_event_box,     1, 2, 0, 1,    FILL,        SHRINK, 0, 0);
	/* track controls */
	edit_packer.attach (controls_layout,         1, 2, 1, 2,    FILL,        FILL|EXPAND, 0, 0);
	/* canvas */
	edit_packer.attach (*_track_canvas_viewport,  2, 3, 0, 2,    FILL|EXPAND, FILL|EXPAND, 0, 0);

	bottom_hbox.set_border_width (2);
	bottom_hbox.set_spacing (3);

	PresentationInfo::Change.connect (*this, MISSING_INVALIDATOR, boost::bind (&Editor::presentation_info_changed, this, _1), gui_context());

	_route_groups = new EditorRouteGroups (this);
	_routes = new EditorRoutes ();
	_regions = new EditorRegions (this);
	_sources = new EditorSources (this);
	_snapshots = new EditorSnapshots (this);
	_locations = new EditorLocations (this);
	_properties_box = new SelectionPropertiesBox ();

	/* these are static location signals */

	Location::start_changed.connect (*this, invalidator (*this), boost::bind (&Editor::location_changed, this, _1), gui_context());
	Location::end_changed.connect (*this, invalidator (*this), boost::bind (&Editor::location_changed, this, _1), gui_context());
	Location::changed.connect (*this, invalidator (*this), boost::bind (&Editor::location_changed, this, _1), gui_context());

#if SELECTION_PROPERTIES_BOX_TODO
	add_notebook_page (_("Selection"), *_properties_box);
#warning Fix Properties Sidebar Layout to fit < 720px height
#endif
	add_notebook_page (_("Tracks & Busses"), _routes->widget ());
	add_notebook_page (_("Sources"), _sources->widget ());
	add_notebook_page (_("Regions"), _regions->widget ());
	add_notebook_page (_("Snapshots"), _snapshots->widget ());
	add_notebook_page (_("Track & Bus Groups"), _route_groups->widget ());
	add_notebook_page (_("Ranges & Marks"), _locations->widget ());

	_the_notebook.set_show_tabs (true);
	_the_notebook.set_scrollable (true);
	_the_notebook.popup_disable ();
	_the_notebook.set_tab_pos (Gtk::POS_RIGHT);
	_the_notebook.show_all ();

	_notebook_shrunk = false;


	/* Pick up some settings we need to cache, early */

	XMLNode* settings = ARDOUR_UI::instance()->editor_settings();

	if (settings) {
		settings->get_property ("notebook-shrunk", _notebook_shrunk);
	}

	editor_summary_pane.set_check_divider_position (true);
	editor_summary_pane.add (edit_packer);

	Button* summary_arrow_left = manage (new Button);
	summary_arrow_left->add (*manage (new Arrow (ARROW_LEFT, SHADOW_NONE)));
	summary_arrow_left->signal_pressed().connect (sigc::hide_return (sigc::bind (sigc::mem_fun (*this, &Editor::scroll_press), LEFT)));
	summary_arrow_left->signal_released().connect (sigc::mem_fun (*this, &Editor::scroll_release));

	Button* summary_arrow_right = manage (new Button);
	summary_arrow_right->add (*manage (new Arrow (ARROW_RIGHT, SHADOW_NONE)));
	summary_arrow_right->signal_pressed().connect (sigc::hide_return (sigc::bind (sigc::mem_fun (*this, &Editor::scroll_press), RIGHT)));
	summary_arrow_right->signal_released().connect (sigc::mem_fun (*this, &Editor::scroll_release));

	VBox* summary_arrows_left = manage (new VBox);
	summary_arrows_left->pack_start (*summary_arrow_left);

	VBox* summary_arrows_right = manage (new VBox);
	summary_arrows_right->pack_start (*summary_arrow_right);

	Gtk::Frame* summary_frame = manage (new Gtk::Frame);
	summary_frame->set_shadow_type (Gtk::SHADOW_ETCHED_IN);

	summary_frame->add (*_summary);
	summary_frame->show ();

	_summary_hbox.pack_start (*summary_arrows_left, false, false);
	_summary_hbox.pack_start (*summary_frame, true, true);
	_summary_hbox.pack_start (*summary_arrows_right, false, false);

	editor_summary_pane.add (_summary_hbox);
	edit_pane.set_check_divider_position (true);
	edit_pane.add (editor_summary_pane);
	_editor_list_vbox.pack_start (*_properties_box, false, false, 0);
	_editor_list_vbox.pack_start (_the_notebook);
	edit_pane.add (_editor_list_vbox);
	edit_pane.set_child_minsize (_editor_list_vbox, 30); /* rough guess at width of notebook tabs */

	edit_pane.set_drag_cursor (*_cursors->expand_left_right);
	editor_summary_pane.set_drag_cursor (*_cursors->expand_up_down);

	float fract;
	if (!settings || !settings->get_property ("edit-horizontal-pane-pos", fract) || fract > 1.0) {
		/* initial allocation is 90% to canvas, 10% to notebook */
		fract = 0.90;
	}
	edit_pane.set_divider (0, fract);

	if (!settings || !settings->get_property ("edit-vertical-pane-pos", fract) || fract > 1.0) {
		/* initial allocation is 90% to canvas, 10% to summary */
		fract = 0.90;
	}
	editor_summary_pane.set_divider (0, fract);

	global_vpacker.set_spacing (0);
	global_vpacker.set_border_width (0);

	/* the next three EventBoxes provide the ability for their child widgets to have a background color.  That is all. */

	Gtk::EventBox* ebox = manage (new Gtk::EventBox); // a themeable box
	ebox->set_name("EditorWindow");
	ebox->add (ebox_hpacker);

	Gtk::EventBox* epane_box = manage (new EventBoxExt); // a themeable box
	epane_box->set_name("EditorWindow");
	epane_box->add (edit_pane);

	Gtk::EventBox* epane_box2 = manage (new EventBoxExt); // a themeable box
	epane_box2->set_name("EditorWindow");
	epane_box2->add (global_vpacker);

	ArdourWidgets::ArdourDropShadow *toolbar_shadow = manage (new (ArdourWidgets::ArdourDropShadow));
	toolbar_shadow->set_size_request (-1, 4);
	toolbar_shadow->set_mode(ArdourWidgets::ArdourDropShadow::DropShadowBoth);
	toolbar_shadow->set_name("EditorWindow");
	toolbar_shadow->show();

	global_vpacker.pack_start (*toolbar_shadow, false, false);
	global_vpacker.pack_start (*ebox, false, false);
	global_vpacker.pack_start (*epane_box, true, true);
	global_hpacker.pack_start (*epane_box2, true, true);

	/* need to show the "contents" widget so that notebook will show if tab is switched to
	 */

	global_hpacker.show ();
	ebox_hpacker.show();
	ebox->show();

	/* register actions now so that set_state() can find them and set toggles/checks etc */

	load_bindings ();
	register_actions ();

	setup_toolbar ();

	RegionView::RegionViewGoingAway.connect (*this, invalidator (*this),  boost::bind (&Editor::catch_vanishing_regionview, this, _1), gui_context());

	/* nudge stuff */

	nudge_forward_button.set_name ("nudge button");
	nudge_forward_button.set_icon(ArdourIcon::NudgeRight);

	nudge_backward_button.set_name ("nudge button");
	nudge_backward_button.set_icon(ArdourIcon::NudgeLeft);

	fade_context_menu.set_name ("ArdourContextMenu");

	Gtkmm2ext::Keyboard::the_keyboard().ZoomVerticalModifierReleased.connect (sigc::mem_fun (*this, &Editor::zoom_vertical_modifier_released));

	/* allow external control surfaces/protocols to do various things */

	ControlProtocol::ZoomToSession.connect (*this, invalidator (*this), boost::bind (&Editor::temporal_zoom_session, this), gui_context());
	ControlProtocol::ZoomIn.connect (*this, invalidator (*this), boost::bind (&Editor::temporal_zoom_step, this, false), gui_context());
	ControlProtocol::ZoomOut.connect (*this, invalidator (*this), boost::bind (&Editor::temporal_zoom_step, this, true), gui_context());
	ControlProtocol::Undo.connect (*this, invalidator (*this), boost::bind (&Editor::undo, this, true), gui_context());
	ControlProtocol::Redo.connect (*this, invalidator (*this), boost::bind (&Editor::redo, this, true), gui_context());
	ControlProtocol::ScrollTimeline.connect (*this, invalidator (*this), boost::bind (&Editor::control_scroll, this, _1), gui_context());
	ControlProtocol::StepTracksUp.connect (*this, invalidator (*this), boost::bind (&Editor::control_step_tracks_up, this), gui_context());
	ControlProtocol::StepTracksDown.connect (*this, invalidator (*this), boost::bind (&Editor::control_step_tracks_down, this), gui_context());
	ControlProtocol::GotoView.connect (*this, invalidator (*this), boost::bind (&Editor::control_view, this, _1), gui_context());
	ControlProtocol::CloseDialog.connect (*this, invalidator (*this), Keyboard::close_current_dialog, gui_context());
	ControlProtocol::VerticalZoomInAll.connect (*this, invalidator (*this), boost::bind (&Editor::control_vertical_zoom_in_all, this), gui_context());
	ControlProtocol::VerticalZoomOutAll.connect (*this, invalidator (*this), boost::bind (&Editor::control_vertical_zoom_out_all, this), gui_context());
	ControlProtocol::VerticalZoomInSelected.connect (*this, invalidator (*this), boost::bind (&Editor::control_vertical_zoom_in_selected, this), gui_context());
	ControlProtocol::VerticalZoomOutSelected.connect (*this, invalidator (*this), boost::bind (&Editor::control_vertical_zoom_out_selected, this), gui_context());

	BasicUI::AccessAction.connect (*this, invalidator (*this), boost::bind (&Editor::access_action, this, _1, _2), gui_context());

	/* handle escape */

	ARDOUR_UI::instance()->Escape.connect (*this, invalidator (*this), boost::bind (&Editor::escape, this), gui_context());

	/* problematic: has to return a value and thus cannot be x-thread */

	Session::AskAboutPlaylistDeletion.connect_same_thread (*this, boost::bind (&Editor::playlist_deletion_dialog, this, _1));
	Route::PluginSetup.connect_same_thread (*this, boost::bind (&Editor::plugin_setup, this, _1, _2, _3));

	Config->ParameterChanged.connect (*this, invalidator (*this), boost::bind (&Editor::parameter_changed, this, _1), gui_context());
	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &Editor::ui_parameter_changed));

	TimeAxisView::CatchDeletion.connect (*this, invalidator (*this), boost::bind (&Editor::timeaxisview_deleted, this, _1), gui_context());

	_ignore_region_action = false;
	_last_region_menu_was_main = false;

	_show_marker_lines = false;

	/* Button bindings */

	button_bindings = new Bindings ("editor-mouse");

	XMLNode* node = button_settings();
	if (node) {
		for (XMLNodeList::const_iterator i = node->children().begin(); i != node->children().end(); ++i) {
			button_bindings->load_operation (**i);
		}
	}

	constructed = true;

	/* grab current parameter state */
	boost::function<void (string)> pc (boost::bind (&Editor::ui_parameter_changed, this, _1));
	UIConfiguration::instance().map_parameters (pc);

	setup_fade_images ();
}

Editor::~Editor()
{
	delete tempo_marker_menu;
	delete meter_marker_menu;
	delete marker_menu;
	delete range_marker_menu;
	delete new_transport_marker_menu;
	delete editor_ruler_menu;
	delete _popup_region_menu_item;

	delete button_bindings;
	delete _routes;
	delete _route_groups;
	delete _track_canvas_viewport;
	delete _drags;
	delete nudge_clock;
	delete _verbose_cursor;
	delete _region_peak_cursor;
	delete quantize_dialog;
	delete _summary;
	delete _group_tabs;
	delete _regions;
	delete _snapshots;
	delete _locations;
	delete _properties_box;
	delete selection;
	delete cut_buffer;
	delete _cursors;

	LuaInstance::destroy_instance ();

	for (list<XMLNode *>::iterator i = selection_op_history.begin(); i != selection_op_history.end(); ++i) {
		delete *i;
	}
	for (std::map<ARDOUR::FadeShape, Gtk::Image*>::const_iterator i = _xfade_in_images.begin(); i != _xfade_in_images.end (); ++i) {
		delete i->second;
	}
	for (std::map<ARDOUR::FadeShape, Gtk::Image*>::const_iterator i = _xfade_out_images.begin(); i != _xfade_out_images.end (); ++i) {
		delete i->second;
	}
}

XMLNode*
Editor::button_settings () const
{
	XMLNode* settings = ARDOUR_UI::instance()->editor_settings();
	XMLNode* node = find_named_node (*settings, X_("Buttons"));

	if (!node) {
		node = new XMLNode (X_("Buttons"));
	}

	return node;
}

bool
Editor::get_smart_mode () const
{
	return ((current_mouse_mode() == MouseObject) && smart_mode_action->get_active());
}

void
Editor::catch_vanishing_regionview (RegionView *rv)
{
	/* note: the selection will take care of the vanishing
	   audioregionview by itself.
	*/

	if (_drags->active() && _drags->have_item (rv->get_canvas_group()) && !_drags->ending()) {
		_drags->abort ();
	}

	if (clicked_regionview == rv) {
		clicked_regionview = 0;
	}

	if (entered_regionview == rv) {
		set_entered_regionview (0);
	}

	if (!_all_region_actions_sensitized) {
		sensitize_all_region_actions (true);
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

	entered_regionview = rv;

	if (entered_regionview  != 0) {
		entered_regionview->entered ();
	}

	if (!_all_region_actions_sensitized && _last_region_menu_was_main) {
		/* This RegionView entry might have changed what region actions
		   are allowed, so sensitize them all in case a key is pressed.
		*/
		sensitize_all_region_actions (true);
	}
}

void
Editor::set_entered_track (TimeAxisView* tav)
{
	if (entered_track) {
		entered_track->exited ();
	}

	entered_track = tav;

	if (entered_track) {
		entered_track->entered ();
	}
}

void
Editor::instant_save ()
{
	if (!constructed || !_session || no_save_instant) {
		return;
	}

	_session->add_instant_xml(get_state());
}

void
Editor::control_vertical_zoom_in_all ()
{
	tav_zoom_smooth (false, true);
}

void
Editor::control_vertical_zoom_out_all ()
{
	tav_zoom_smooth (true, true);
}

void
Editor::control_vertical_zoom_in_selected ()
{
	tav_zoom_smooth (false, false);
}

void
Editor::control_vertical_zoom_out_selected ()
{
	tav_zoom_smooth (true, false);
}

void
Editor::control_view (uint32_t view)
{
	goto_visual_state (view);
}

void
Editor::control_step_tracks_up ()
{
	scroll_tracks_up_line ();
}

void
Editor::control_step_tracks_down ()
{
	scroll_tracks_down_line ();
}

void
Editor::control_scroll (float fraction)
{
	ENSURE_GUI_THREAD (*this, &Editor::control_scroll, fraction)

	if (!_session) {
		return;
	}

	double step = fraction * current_page_samples();

	/*
		_control_scroll_target is an optional<T>

		it acts like a pointer to an samplepos_t, with
		a operator conversion to boolean to check
		that it has a value could possibly use
		_playhead_cursor->current_sample to store the
		value and a boolean in the class to know
		when it's out of date
	*/

	if (!_control_scroll_target) {
		_control_scroll_target = _session->transport_sample();
		_dragging_playhead = true;
	}

	if ((fraction < 0.0f) && (*_control_scroll_target <= (samplepos_t) fabs(step))) {
		*_control_scroll_target = 0;
	} else if ((fraction > 0.0f) && (max_samplepos - *_control_scroll_target < step)) {
		*_control_scroll_target = max_samplepos - (current_page_samples()*2); // allow room for slop in where the PH is on the screen
	} else {
		*_control_scroll_target += (samplepos_t) trunc (step);
	}

	/* move visuals, we'll catch up with it later */

	_playhead_cursor->set_position (*_control_scroll_target);
	UpdateAllTransportClocks (*_control_scroll_target);

	if (*_control_scroll_target > (current_page_samples() / 2)) {
		/* try to center PH in window */
		reset_x_origin (*_control_scroll_target - (current_page_samples()/2));
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

	control_scroll_connection = Glib::signal_timeout().connect (sigc::bind (sigc::mem_fun (*this, &Editor::deferred_control_scroll), *_control_scroll_target), 250);
}

bool
Editor::deferred_control_scroll (samplepos_t /*target*/)
{
	_session->request_locate (*_control_scroll_target);
	/* reset for next stream */
	_control_scroll_target = boost::none;
	_dragging_playhead = false;
	return false;
}

void
Editor::access_action (const std::string& action_group, const std::string& action_item)
{
	if (!_session) {
		return;
	}

	ENSURE_GUI_THREAD (*this, &Editor::access_action, action_group, action_item)

	RefPtr<Action> act;
	try {
		act = ActionManager::get_action (action_group.c_str(), action_item.c_str());
		if (act) {
			cerr << "firing up " << action_item << endl;
			act->activate();
		}
	} catch ( ActionManager::MissingActionException const& e) {
		cerr << "MissingActionException:" << e.what () << endl;
	}
}

void
Editor::set_toggleaction (const std::string& action_group, const std::string& action_item, bool s)
{
	ActionManager::set_toggleaction_state (action_group.c_str(), action_item.c_str(), s);
}

void
Editor::on_realize ()
{
	Realized ();

	if (UIConfiguration::instance().get_lock_gui_after_seconds()) {
		start_lock_event_timing ();
	}
}

void
Editor::start_lock_event_timing ()
{
	/* check if we should lock the GUI every 30 seconds */

	Glib::signal_timeout().connect (sigc::mem_fun (*this, &Editor::lock_timeout_callback), 30 * 1000);
}

bool
Editor::generic_event_handler (GdkEvent* ev)
{
	switch (ev->type) {
	case GDK_BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
	case GDK_MOTION_NOTIFY:
	case GDK_KEY_PRESS:
	case GDK_KEY_RELEASE:
		if (contents().is_mapped()) {
			gettimeofday (&last_event_time, 0);
		}
		break;

	case GDK_LEAVE_NOTIFY:
		switch (ev->crossing.detail) {
		case GDK_NOTIFY_UNKNOWN:
		case GDK_NOTIFY_INFERIOR:
		case GDK_NOTIFY_ANCESTOR:
			break;
		case GDK_NOTIFY_VIRTUAL:
		case GDK_NOTIFY_NONLINEAR:
		case GDK_NOTIFY_NONLINEAR_VIRTUAL:
			/* leaving window, so reset focus, thus ending any and
			   all text entry operations.
			*/
			ARDOUR_UI::instance()->reset_focus (&contents());
			break;
		}
		break;

	default:
		break;
	}

	return false;
}

bool
Editor::lock_timeout_callback ()
{
	struct timeval now, delta;

	gettimeofday (&now, 0);

	timersub (&now, &last_event_time, &delta);

	if (delta.tv_sec > (time_t) UIConfiguration::instance().get_lock_gui_after_seconds()) {
		lock ();
		/* don't call again. Returning false will effectively
		   disconnect us from the timer callback.

		   unlock() will call start_lock_event_timing() to get things
		   started again.
		*/
		return false;
	}

	return true;
}

void
Editor::map_position_change (samplepos_t sample)
{
	ENSURE_GUI_THREAD (*this, &Editor::map_position_change, sample)

	if (_session == 0) {
		return;
	}

	if (_follow_playhead) {
		center_screen (sample);
	}

	if (!_session->locate_initiated()) {
		_playhead_cursor->set_position (sample);
	}
}

void
Editor::center_screen (samplepos_t sample)
{
	samplecnt_t const page = _visible_canvas_width * samples_per_pixel;

	/* if we're off the page, then scroll.
	 */

	if (sample < _leftmost_sample || sample >= _leftmost_sample + page) {
		center_screen_internal (sample, page);
	}
}

void
Editor::center_screen_internal (samplepos_t sample, float page)
{
	page /= 2;

	if (sample > page) {
		sample -= (samplepos_t) page;
	} else {
		sample = 0;
	}

	reset_x_origin (sample);
}


void
Editor::update_title ()
{
	ENSURE_GUI_THREAD (*this, &Editor::update_title);

	if (!own_window()) {
		return;
	}

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

		WindowTitle title(session_name);
		title += S_("Window|Editor");
		title += Glib::get_application_name();
		own_window()->set_title (title.get_string());
	} else {
		/* ::session_going_away() will have taken care of it */
	}
}

void
Editor::set_session (Session *t)
{
	SessionHandlePtr::set_session (t);

	if (!_session) {
		return;
	}

	/* initialize _leftmost_sample to the extents of the session
	 * this prevents a bogus setting of leftmost = "0" if the summary view asks for the leftmost sample
	 * before the visible state has been loaded from instant.xml */
	_leftmost_sample = session_gui_extents().first.samples();

	nudge_clock->set_session (_session);
	_summary->set_session (_session);
	_group_tabs->set_session (_session);
	_route_groups->set_session (_session);
	_regions->set_session (_session);
	_sources->set_session (_session);
	_snapshots->set_session (_session);
	_routes->set_session (_session);
	_locations->set_session (_session);
	_properties_box->set_session (_session);

	if (rhythm_ferret) {
		rhythm_ferret->set_session (_session);
	}

	if (analysis_window) {
		analysis_window->set_session (_session);
	}

	if (sfbrowser) {
		sfbrowser->set_session (_session);
	}

	initial_display ();
	compute_fixed_ruler_scale ();

	/* Make sure we have auto loop and auto punch ranges */

	Location* loc = _session->locations()->auto_loop_location();
	if (loc != 0) {
		loc->set_name (_("Loop"));
	}

	loc = _session->locations()->auto_punch_location();
	if (loc != 0) {
		/* force name */
		loc->set_name (_("Punch"));
	}

	refresh_location_display ();

	/* This must happen after refresh_location_display(), as (amongst other things) we restore
	 * the selected Marker; this needs the LocationMarker list to be available.
	 */
	XMLNode* node = ARDOUR_UI::instance()->editor_settings();
	set_state (*node, Stateful::loading_state_version);

	/* catch up on selection state, etc. */

	PropertyChange sc;
	sc.add (Properties::selected);
	presentation_info_changed (sc);

	/* catch up with the playhead */

	_session->request_locate (_playhead_cursor->current_sample (), MustStop);
	_pending_initial_locate = true;

	update_title ();

	/* These signals can all be emitted by a non-GUI thread. Therefore the
	   handlers for them must not attempt to directly interact with the GUI,
	   but use PBD::Signal<T>::connect() which accepts an event loop
	   ("context") where the handler will be asked to run.
	*/

	_session->StepEditStatusChange.connect (_session_connections, invalidator (*this), boost::bind (&Editor::step_edit_status_change, this, _1), gui_context());
	_session->TransportStateChange.connect (_session_connections, invalidator (*this), boost::bind (&Editor::map_transport_state, this), gui_context());
	_session->TransportLooped.connect (_session_connections, invalidator (*this), boost::bind (&Editor::transport_looped, this), gui_context());
	_session->PositionChanged.connect (_session_connections, invalidator (*this), boost::bind (&Editor::map_position_change, this, _1), gui_context());
	_session->vca_manager().VCAAdded.connect (_session_connections, invalidator (*this), boost::bind (&Editor::add_vcas, this, _1), gui_context());
	_session->RouteAdded.connect (_session_connections, invalidator (*this), boost::bind (&Editor::add_routes, this, _1), gui_context());
	_session->DirtyChanged.connect (_session_connections, invalidator (*this), boost::bind (&Editor::update_title, this), gui_context());
	_session->Located.connect (_session_connections, invalidator (*this), boost::bind (&Editor::located, this), gui_context());
	_session->config.ParameterChanged.connect (_session_connections, invalidator (*this), boost::bind (&Editor::parameter_changed, this, _1), gui_context());
	_session->StateSaved.connect (_session_connections, invalidator (*this), boost::bind (&Editor::session_state_saved, this, _1), gui_context());
	_session->locations()->added.connect (_session_connections, invalidator (*this), boost::bind (&Editor::add_new_location, this, _1), gui_context());
	_session->locations()->removed.connect (_session_connections, invalidator (*this), boost::bind (&Editor::location_gone, this, _1), gui_context());
	_session->locations()->changed.connect (_session_connections, invalidator (*this), boost::bind (&Editor::refresh_location_display, this), gui_context());
	_session->history().Changed.connect (_session_connections, invalidator (*this), boost::bind (&Editor::history_changed, this), gui_context());

	_playhead_cursor->track_canvas_item().reparent ((ArdourCanvas::Item*) get_cursor_scroll_group());
	_playhead_cursor->show ();

	_snapped_cursor->track_canvas_item().reparent ((ArdourCanvas::Item*) get_cursor_scroll_group());
	_snapped_cursor->set_color (UIConfiguration::instance().color ("edit point"));
	_snapped_cursor->show ();

	boost::function<void (string)> pc (boost::bind (&Editor::parameter_changed, this, _1));
	Config->map_parameters (pc);
	_session->config.map_parameters (pc);

	restore_ruler_visibility ();
	//tempo_map_changed (PropertyChange (0));
	TempoMap::Metrics metrics;
	TempoMap::use()->get_metrics (metrics);
	draw_metric_marks (metrics);

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		(static_cast<TimeAxisView*>(*i))->set_samples_per_pixel (samples_per_pixel);
	}

	super_rapid_screen_update_connection = Timers::super_rapid_connect (
		sigc::mem_fun (*this, &Editor::super_rapid_screen_update)
		);

	/* register for undo history */
	_session->register_with_memento_command_factory(id(), this);
	_session->register_with_memento_command_factory(_selection_memento->id(), _selection_memento);

	LuaInstance::instance()->set_session(_session);

	start_updating_meters ();
}

void
Editor::fill_xfade_menu (Menu_Helpers::MenuList& items, bool start)
{
	using namespace Menu_Helpers;

	void (Editor::*emf)(FadeShape);
	std::map<ARDOUR::FadeShape,Gtk::Image*>* images;

	if (start) {
		images = &_xfade_in_images;
		emf = &Editor::set_fade_in_shape;
	} else {
		images = &_xfade_out_images;
		emf = &Editor::set_fade_out_shape;
	}

	items.push_back (
		ImageMenuElem (
			_("Linear (for highly correlated material)"),
			*(*images)[FadeLinear],
			sigc::bind (sigc::mem_fun (*this, emf), FadeLinear)
			)
		);

	dynamic_cast<ImageMenuItem*>(&items.back())->set_always_show_image ();

	items.push_back (
		ImageMenuElem (
			_("Constant power"),
			*(*images)[FadeConstantPower],
			sigc::bind (sigc::mem_fun (*this, emf), FadeConstantPower)
			));

	dynamic_cast<ImageMenuItem*>(&items.back())->set_always_show_image ();

	items.push_back (
		ImageMenuElem (
			_("Symmetric"),
			*(*images)[FadeSymmetric],
			sigc::bind (sigc::mem_fun (*this, emf), FadeSymmetric)
			)
		);

	dynamic_cast<ImageMenuItem*>(&items.back())->set_always_show_image ();

	items.push_back (
		ImageMenuElem (
			_("Slow"),
			*(*images)[FadeSlow],
			sigc::bind (sigc::mem_fun (*this, emf), FadeSlow)
			));

	dynamic_cast<ImageMenuItem*>(&items.back())->set_always_show_image ();

	items.push_back (
		ImageMenuElem (
			_("Fast"),
			*(*images)[FadeFast],
			sigc::bind (sigc::mem_fun (*this, emf), FadeFast)
			));

	dynamic_cast<ImageMenuItem*>(&items.back())->set_always_show_image ();
}

/** Pop up a context menu for when the user clicks on a start crossfade */
void
Editor::popup_xfade_in_context_menu (int button, int32_t time, ArdourCanvas::Item* item, ItemType /*item_type*/)
{
	using namespace Menu_Helpers;
	AudioRegionView* arv = dynamic_cast<AudioRegionView*> ((RegionView*)item->get_data ("regionview"));
	if (!arv) {
		return;
	}

	MenuList& items (xfade_in_context_menu.items());
	items.clear ();

	if (arv->audio_region()->fade_in_active()) {
		items.push_back (MenuElem (_("Deactivate"), sigc::bind (sigc::mem_fun (*this, &Editor::set_fade_in_active), false)));
	} else {
		items.push_back (MenuElem (_("Activate"), sigc::bind (sigc::mem_fun (*this, &Editor::set_fade_in_active), true)));
	}

	items.push_back (SeparatorElem());
	fill_xfade_menu (items, true);

	xfade_in_context_menu.popup (button, time);
}

/** Pop up a context menu for when the user clicks on an end crossfade */
void
Editor::popup_xfade_out_context_menu (int button, int32_t time, ArdourCanvas::Item* item, ItemType /*item_type*/)
{
	using namespace Menu_Helpers;
	AudioRegionView* arv = dynamic_cast<AudioRegionView*> ((RegionView*)item->get_data ("regionview"));
	if (!arv) {
		return;
	}

	MenuList& items (xfade_out_context_menu.items());
	items.clear ();

	if (arv->audio_region()->fade_out_active()) {
		items.push_back (MenuElem (_("Deactivate"), sigc::bind (sigc::mem_fun (*this, &Editor::set_fade_out_active), false)));
	} else {
		items.push_back (MenuElem (_("Activate"), sigc::bind (sigc::mem_fun (*this, &Editor::set_fade_out_active), true)));
	}

	items.push_back (SeparatorElem());
	fill_xfade_menu (items, false);

	xfade_out_context_menu.popup (button, time);
}

void
Editor::popup_track_context_menu (int button, int32_t time, ItemType item_type, bool with_selection)
{
	using namespace Menu_Helpers;
	Menu* (Editor::*build_menu_function)();
	Menu *menu;

	switch (item_type) {
	case RegionItem:
	case RegionViewName:
	case RegionViewNameHighlight:
	case LeftFrameHandle:
	case RightFrameHandle:
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

	case StreamItem:
		if (clicked_routeview != 0 && clicked_routeview->track()) {
			build_menu_function = &Editor::build_track_context_menu;
		} else {
			build_menu_function = &Editor::build_track_bus_context_menu;
		}
		break;

	default:
		/* probably shouldn't happen but if it does, we don't care */
		return;
	}

	menu = (this->*build_menu_function)();
	menu->set_name ("ArdourContextMenu");

	/* now handle specific situations */

	switch (item_type) {
	case RegionItem:
	case RegionViewName:
	case RegionViewNameHighlight:
	case LeftFrameHandle:
	case RightFrameHandle:
		break;

	case SelectionItem:
		break;

	case StreamItem:
		break;

	default:
		/* probably shouldn't happen but if it does, we don't care */
		return;
	}

	if (item_type != SelectionItem && clicked_routeview && clicked_routeview->audio_track()) {

		/* Bounce to disk */

		using namespace Menu_Helpers;
		MenuList& edit_items  = menu->items();

		edit_items.push_back (SeparatorElem());

		switch (clicked_routeview->audio_track()->freeze_state()) {
		case AudioTrack::NoFreeze:
			edit_items.push_back (MenuElem (_("Freeze"), sigc::mem_fun(*this, &Editor::freeze_route)));
			break;

		case AudioTrack::Frozen:
			edit_items.push_back (MenuElem (_("Unfreeze"), sigc::mem_fun(*this, &Editor::unfreeze_route)));
			break;

		case AudioTrack::UnFrozen:
			edit_items.push_back (MenuElem (_("Freeze"), sigc::mem_fun(*this, &Editor::freeze_route)));
			break;
		}

	}

	if (item_type == StreamItem && clicked_routeview) {
		clicked_routeview->build_underlay_menu(menu);
	}

	/* When the region menu is opened, we setup the actions so that they look right
	   in the menu.
	*/
	sensitize_the_right_region_actions (false);
	_last_region_menu_was_main = false;

	menu->signal_hide().connect (sigc::bind (sigc::mem_fun (*this, &Editor::sensitize_all_region_actions), true));
	menu->popup (button, time);
}

Menu*
Editor::build_track_context_menu ()
{
	using namespace Menu_Helpers;

	MenuList& edit_items = track_context_menu.items();
	edit_items.clear();

	add_dstream_context_items (edit_items);
	return &track_context_menu;
}

Menu*
Editor::build_track_bus_context_menu ()
{
	using namespace Menu_Helpers;

	MenuList& edit_items = track_context_menu.items();
	edit_items.clear();

	add_bus_context_items (edit_items);
	return &track_context_menu;
}

Menu*
Editor::build_track_region_context_menu ()
{
	using namespace Menu_Helpers;
	MenuList& edit_items  = track_region_context_menu.items();
	edit_items.clear();

	/* we've just cleared the track region context menu, so the menu that these
	   two items were on will have disappeared; stop them dangling.
	*/
	RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (clicked_axisview);

	if (rtv) {
		boost::shared_ptr<Track> tr;
		boost::shared_ptr<Playlist> pl;

		if ((tr = rtv->track())) {
			add_region_context_items (edit_items, tr);
		}
	}

	add_dstream_context_items (edit_items);

	return &track_region_context_menu;
}

void
Editor::loudness_analyze_region_selection ()
{
	if (!_session) {
		return;
	}
	Selection& s (PublicEditor::instance ().get_selection ());
	RegionSelection ars = s.regions;
	ARDOUR::AnalysisGraph ag (_session);
	samplecnt_t total_work = 0;

	for (RegionSelection::iterator j = ars.begin (); j != ars.end (); ++j) {
		AudioRegionView* arv = dynamic_cast<AudioRegionView*> (*j);
		if (!arv) {
			continue;
		}
		if (!boost::dynamic_pointer_cast<AudioRegion> (arv->region ())) {
			continue;
		}
		assert (dynamic_cast<RouteTimeAxisView *> (&arv->get_time_axis_view ()));
		total_work += arv->region ()->length_samples ();
	}

	SimpleProgressDialog spd (_("Region Loudness Analysis"), sigc::mem_fun (ag, &AnalysisGraph::cancel));
	ScopedConnection c;
	ag.set_total_samples (total_work);
	ag.Progress.connect_same_thread (c, boost::bind (&SimpleProgressDialog::update_progress, &spd, _1, _2));
	spd.show();

	for (RegionSelection::iterator j = ars.begin (); j != ars.end (); ++j) {
		AudioRegionView* arv = dynamic_cast<AudioRegionView*> (*j);
		if (!arv) {
			continue;
		}
		boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion> (arv->region ());
		if (!ar) {
			continue;
		}
		ag.analyze_region (ar);
	}
	spd.hide();
	if (!ag.canceled ()) {
		ExportReport er (_("Audio Report/Analysis"), ag.results ());
		er.run();
	}
}

void
Editor::loudness_analyze_range_selection ()
{
	if (!_session) {
		return;
	}
	Selection& s (PublicEditor::instance ().get_selection ());
	TimeSelection ts = s.time;
	ARDOUR::AnalysisGraph ag (_session);
	samplecnt_t total_work = 0;

	for (TrackSelection::iterator i = s.tracks.begin (); i != s.tracks.end (); ++i) {
		boost::shared_ptr<AudioPlaylist> pl = boost::dynamic_pointer_cast<AudioPlaylist> ((*i)->playlist ());
		if (!pl) {
			continue;
		}
		RouteUI *rui = dynamic_cast<RouteUI *> (*i);
		if (!pl || !rui) {
			continue;
		}
		for (std::list<TimelineRange>::iterator j = ts.begin (); j != ts.end (); ++j) {
			total_work += j->length_samples ();
		}
	}

	SimpleProgressDialog spd (_("Range Loudness Analysis"), sigc::mem_fun (ag, &AnalysisGraph::cancel));
	ScopedConnection c;
	ag.set_total_samples (total_work);
	ag.Progress.connect_same_thread (c, boost::bind (&SimpleProgressDialog::update_progress, &spd, _1, _2));
	spd.show();

	for (TrackSelection::iterator i = s.tracks.begin (); i != s.tracks.end (); ++i) {
		boost::shared_ptr<AudioPlaylist> pl = boost::dynamic_pointer_cast<AudioPlaylist> ((*i)->playlist ());
		if (!pl) {
			continue;
		}
		RouteUI *rui = dynamic_cast<RouteUI *> (*i);
		if (!pl || !rui) {
			continue;
		}
		ag.analyze_range (rui->route (), pl, ts);
	}
	spd.hide();
	if (!ag.canceled ()) {
		ExportReport er (_("Audio Report/Analysis"), ag.results ());
		er.run();
	}
}

void
Editor::spectral_analyze_region_selection ()
{
	if (analysis_window == 0) {
		analysis_window = new AnalysisWindow();

		if (_session != 0)
			analysis_window->set_session(_session);

		analysis_window->show_all();
	}

	analysis_window->set_regionmode();
	analysis_window->analyze();

	analysis_window->present();
}

void
Editor::spectral_analyze_range_selection()
{
	if (analysis_window == 0) {
		analysis_window = new AnalysisWindow();

		if (_session != 0)
			analysis_window->set_session(_session);

		analysis_window->show_all();
	}

	analysis_window->set_rangemode();
	analysis_window->analyze();

	analysis_window->present();
}

Menu*
Editor::build_track_selection_context_menu ()
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
Editor::add_region_context_items (Menu_Helpers::MenuList& edit_items, boost::shared_ptr<Track> track)
{
	using namespace Menu_Helpers;

	/* OK, stick the region submenu at the top of the list, and then add
	   the standard items.
	*/

	RegionSelection rs = get_regions_from_selection_and_entered ();

	string menu_item_name = (rs.size() == 1) ? rs.front()->region()->name() : _("Selected Regions");

	if (_popup_region_menu_item == 0) {
		_popup_region_menu_item = new MenuItem (menu_item_name, false);
		_popup_region_menu_item->set_submenu (*dynamic_cast<Menu*> (ActionManager::get_widget (X_("/PopupRegionMenu"))));
		_popup_region_menu_item->show ();
	} else {
		_popup_region_menu_item->set_label (menu_item_name);
	}

	/* No layering allowed in later is higher layering model */
	RefPtr<Action> act = ActionManager::get_action (X_("EditorMenu"), X_("RegionMenuLayering"));
	if (act && Config->get_layer_model() == LaterHigher) {
		act->set_sensitive (false);
	} else if (act) {
		act->set_sensitive (true);
	}

	const timepos_t position = get_preferred_edit_position (EDIT_IGNORE_NONE, true);

	edit_items.push_back (*_popup_region_menu_item);
	if (Config->get_layer_model() == Manual && track->playlist()->count_regions_at (position) > 1 && (layering_order_editor == 0 || !layering_order_editor->is_visible ())) {
		edit_items.push_back (*manage (_region_actions->get_action ("choose-top-region-context-menu")->create_menu_item ()));
	}
	edit_items.push_back (SeparatorElem());
}

/** Add context menu items relevant to selection ranges.
 * @param edit_items List to add the items to.
 */
void
Editor::add_selection_context_items (Menu_Helpers::MenuList& edit_items)
{
	using namespace Menu_Helpers;

	edit_items.push_back (MenuElem (_("Play Range"), sigc::mem_fun(*this, &Editor::play_selection)));
	edit_items.push_back (MenuElem (_("Loop Range"), sigc::bind (sigc::mem_fun(*this, &Editor::set_loop_from_selection), true)));

	edit_items.push_back (SeparatorElem());
	edit_items.push_back (MenuElem (_("Zoom to Range"), sigc::bind (sigc::mem_fun(*this, &Editor::temporal_zoom_selection), Horizontal)));

	edit_items.push_back (SeparatorElem());
	edit_items.push_back (MenuElem (_("Loudness Analysis"), sigc::mem_fun(*this, &Editor::loudness_analyze_range_selection)));
	edit_items.push_back (MenuElem (_("Spectral Analysis"), sigc::mem_fun(*this, &Editor::spectral_analyze_range_selection)));
	edit_items.push_back (SeparatorElem());
	edit_items.push_back (MenuElem (_("Loudness Assistant..."), sigc::bind (sigc::mem_fun (*this, &Editor::loudness_assistant), true)));
	edit_items.push_back (SeparatorElem());

	edit_items.push_back (
		MenuElem (
			_("Move Range Start to Previous Region Boundary"),
			sigc::bind (sigc::mem_fun (*this, &Editor::move_range_selection_start_or_end_to_region_boundary), false, false)
			)
		);

	edit_items.push_back (
		MenuElem (
			_("Move Range Start to Next Region Boundary"),
			sigc::bind (sigc::mem_fun (*this, &Editor::move_range_selection_start_or_end_to_region_boundary), false, true)
			)
		);

	edit_items.push_back (
		MenuElem (
			_("Move Range End to Previous Region Boundary"),
			sigc::bind (sigc::mem_fun (*this, &Editor::move_range_selection_start_or_end_to_region_boundary), true, false)
			)
		);

	edit_items.push_back (
		MenuElem (
			_("Move Range End to Next Region Boundary"),
			sigc::bind (sigc::mem_fun (*this, &Editor::move_range_selection_start_or_end_to_region_boundary), true, true)
			)
		);

	edit_items.push_back (SeparatorElem());
	edit_items.push_back (MenuElem (_("Separate"), mem_fun(*this, &Editor::separate_region_from_selection)));
//	edit_items.push_back (MenuElem (_("Convert to Region in Region List"), sigc::mem_fun(*this, &Editor::new_region_from_selection)));

	edit_items.push_back (SeparatorElem());
	edit_items.push_back (MenuElem (_("Select All in Range"), sigc::mem_fun(*this, &Editor::select_all_selectables_using_time_selection)));

	edit_items.push_back (SeparatorElem());
	edit_items.push_back (MenuElem (_("Set Loop from Selection"), sigc::bind (sigc::mem_fun(*this, &Editor::set_loop_from_selection), false)));
	edit_items.push_back (MenuElem (_("Set Punch from Selection"), sigc::mem_fun(*this, &Editor::set_punch_from_selection)));
	edit_items.push_back (MenuElem (_("Set Session Start/End from Selection"), sigc::mem_fun(*this, &Editor::set_session_extents_from_selection)));

	edit_items.push_back (SeparatorElem());
	edit_items.push_back (MenuElem (_("Add Range Markers"), sigc::mem_fun (*this, &Editor::add_location_from_selection)));

	edit_items.push_back (SeparatorElem());
	edit_items.push_back (MenuElem (_("Crop Region to Range"), sigc::mem_fun(*this, &Editor::crop_region_to_selection)));
	edit_items.push_back (MenuElem (_("Duplicate Range"), sigc::bind (sigc::mem_fun(*this, &Editor::duplicate_range), false)));

	edit_items.push_back (SeparatorElem());
	edit_items.push_back (MenuElem (_("Consolidate Range"), sigc::bind (sigc::mem_fun(*this, &Editor::bounce_range_selection), ReplaceRange, false)));
	edit_items.push_back (MenuElem (_("Consolidate Range with Processing"), sigc::bind (sigc::mem_fun(*this, &Editor::bounce_range_selection), ReplaceRange, true)));
	edit_items.push_back (MenuElem (_("Bounce Range to Source List"), sigc::bind (sigc::mem_fun(*this, &Editor::bounce_range_selection), NewSource, false)));
	edit_items.push_back (MenuElem (_("Bounce Range to Source List with Processing"), sigc::bind (sigc::mem_fun(*this, &Editor::bounce_range_selection), NewSource, true)));
	edit_items.push_back (MenuElem (_("Bounce Range to Trigger Clip"), sigc::bind (sigc::mem_fun(*this, &Editor::bounce_range_selection), NewTrigger, false)));
	edit_items.push_back (MenuElem (_("Export Range..."), sigc::mem_fun(*this, &Editor::export_selection)));
	if (ARDOUR_UI::instance()->video_timeline->get_duration() > 0) {
		edit_items.push_back (MenuElem (_("Export Video Range..."), sigc::bind (sigc::mem_fun(*(ARDOUR_UI::instance()), &ARDOUR_UI::export_video), true)));
	}
}


void
Editor::add_dstream_context_items (Menu_Helpers::MenuList& edit_items)
{
	using namespace Menu_Helpers;

	/* Playback */

	Menu *play_menu = manage (new Menu);
	MenuList& play_items = play_menu->items();
	play_menu->set_name ("ArdourContextMenu");

	play_items.push_back (MenuElem (_("Play from Edit Point"), sigc::mem_fun(*this, &Editor::play_from_edit_point)));
	play_items.push_back (MenuElem (_("Play from Start"), sigc::mem_fun(*this, &Editor::play_from_start)));
	play_items.push_back (MenuElem (_("Play Region"), sigc::mem_fun(*this, &Editor::play_selected_region)));
	play_items.push_back (SeparatorElem());
	play_items.push_back (MenuElem (_("Loop Region"), sigc::bind (sigc::mem_fun (*this, &Editor::set_loop_from_region), true)));

	edit_items.push_back (MenuElem (_("Play"), *play_menu));

	/* Selection */

	Menu *select_menu = manage (new Menu);
	MenuList& select_items = select_menu->items();
	select_menu->set_name ("ArdourContextMenu");

	select_items.push_back (MenuElem (_("Select All in Track"), sigc::bind (sigc::mem_fun(*this, &Editor::select_all_in_track), Selection::Set)));
	select_items.push_back (MenuElem (_("Select All Objects"), sigc::bind (sigc::mem_fun(*this, &Editor::select_all_objects), Selection::Set)));
	select_items.push_back (MenuElem (_("Invert Selection in Track"), sigc::mem_fun(*this, &Editor::invert_selection_in_track)));
	select_items.push_back (MenuElem (_("Invert Selection"), sigc::mem_fun(*this, &Editor::invert_selection)));
	select_items.push_back (SeparatorElem());
	select_items.push_back (MenuElem (_("Set Range to Loop Range"), sigc::mem_fun(*this, &Editor::set_selection_from_loop)));
	select_items.push_back (MenuElem (_("Set Range to Punch Range"), sigc::mem_fun(*this, &Editor::set_selection_from_punch)));
	select_items.push_back (MenuElem (_("Set Range to Selected Regions"), sigc::mem_fun(*this, &Editor::set_selection_from_region)));
	select_items.push_back (SeparatorElem());
	select_items.push_back (MenuElem (_("Select All After Edit Point"), sigc::bind (sigc::mem_fun(*this, &Editor::select_all_selectables_using_edit), true, true)));
	select_items.push_back (MenuElem (_("Select All Before Edit Point"), sigc::bind (sigc::mem_fun(*this, &Editor::select_all_selectables_using_edit), false, true)));
	select_items.push_back (MenuElem (_("Select All After Playhead"), sigc::bind (sigc::mem_fun(*this, &Editor::select_all_selectables_using_cursor), _playhead_cursor, true)));
	select_items.push_back (MenuElem (_("Select All Before Playhead"), sigc::bind (sigc::mem_fun(*this, &Editor::select_all_selectables_using_cursor), _playhead_cursor, false)));
	select_items.push_back (MenuElem (_("Select All Between Playhead and Edit Point"), sigc::bind (sigc::mem_fun(*this, &Editor::select_all_selectables_between), false)));
	select_items.push_back (MenuElem (_("Select All Within Playhead and Edit Point"), sigc::bind (sigc::mem_fun(*this, &Editor::select_all_selectables_between), true)));
	select_items.push_back (MenuElem (_("Select Range Between Playhead and Edit Point"), sigc::mem_fun(*this, &Editor::select_range_between)));

	edit_items.push_back (MenuElem (_("Select"), *select_menu));

	/* Cut-n-Paste */

	Menu *cutnpaste_menu = manage (new Menu);
	MenuList& cutnpaste_items = cutnpaste_menu->items();
	cutnpaste_menu->set_name ("ArdourContextMenu");

	cutnpaste_items.push_back (MenuElem (_("Cut"), sigc::mem_fun(*this, &Editor::cut)));
	cutnpaste_items.push_back (MenuElem (_("Copy"), sigc::mem_fun(*this, &Editor::copy)));
	cutnpaste_items.push_back (MenuElem (_("Paste"), sigc::bind (sigc::mem_fun(*this, &Editor::paste), 1.0f, true)));

	cutnpaste_items.push_back (SeparatorElem());

	cutnpaste_items.push_back (MenuElem (_("Align"), sigc::bind (sigc::mem_fun (*this, &Editor::align_regions), ARDOUR::SyncPoint)));
	cutnpaste_items.push_back (MenuElem (_("Align Relative"), sigc::bind (sigc::mem_fun (*this, &Editor::align_regions_relative), ARDOUR::SyncPoint)));

	edit_items.push_back (MenuElem (_("Edit"), *cutnpaste_menu));

	/* Adding new material */

	edit_items.push_back (SeparatorElem());
	edit_items.push_back (MenuElem (_("Insert Selected Region"), sigc::bind (sigc::mem_fun(*this, &Editor::insert_source_list_selection), 1.0f)));
	edit_items.push_back (MenuElem (_("Insert Existing Media"), sigc::bind (sigc::mem_fun(*this, &Editor::add_external_audio_action), ImportToTrack)));

	/* Nudge track */

	Menu *nudge_menu = manage (new Menu());
	MenuList& nudge_items = nudge_menu->items();
	nudge_menu->set_name ("ArdourContextMenu");

	edit_items.push_back (SeparatorElem());
	nudge_items.push_back (MenuElem (_("Nudge Entire Track Later"), (sigc::bind (sigc::mem_fun(*this, &Editor::nudge_track), false, true))));
	nudge_items.push_back (MenuElem (_("Nudge Track After Edit Point Later"), (sigc::bind (sigc::mem_fun(*this, &Editor::nudge_track), true, true))));
	nudge_items.push_back (MenuElem (_("Nudge Entire Track Earlier"), (sigc::bind (sigc::mem_fun(*this, &Editor::nudge_track), false, false))));
	nudge_items.push_back (MenuElem (_("Nudge Track After Edit Point Earlier"), (sigc::bind (sigc::mem_fun(*this, &Editor::nudge_track), true, false))));

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

	play_items.push_back (MenuElem (_("Play from Edit Point"), sigc::mem_fun(*this, &Editor::play_from_edit_point)));
	play_items.push_back (MenuElem (_("Play from Start"), sigc::mem_fun(*this, &Editor::play_from_start)));
	edit_items.push_back (MenuElem (_("Play"), *play_menu));

	/* Selection */

	Menu *select_menu = manage (new Menu);
	MenuList& select_items = select_menu->items();
	select_menu->set_name ("ArdourContextMenu");

	select_items.push_back (MenuElem (_("Select All in Track"), sigc::bind (sigc::mem_fun(*this, &Editor::select_all_in_track), Selection::Set)));
	select_items.push_back (MenuElem (_("Select All Objects"), sigc::bind (sigc::mem_fun(*this, &Editor::select_all_objects), Selection::Set)));
	select_items.push_back (MenuElem (_("Invert Selection in Track"), sigc::mem_fun(*this, &Editor::invert_selection_in_track)));
	select_items.push_back (MenuElem (_("Invert Selection"), sigc::mem_fun(*this, &Editor::invert_selection)));
	select_items.push_back (SeparatorElem());
	select_items.push_back (MenuElem (_("Select All After Edit Point"), sigc::bind (sigc::mem_fun(*this, &Editor::select_all_selectables_using_edit), true, true)));
	select_items.push_back (MenuElem (_("Select All Before Edit Point"), sigc::bind (sigc::mem_fun(*this, &Editor::select_all_selectables_using_edit), false, true)));
	select_items.push_back (MenuElem (_("Select All After Playhead"), sigc::bind (sigc::mem_fun(*this, &Editor::select_all_selectables_using_cursor), _playhead_cursor, true)));
	select_items.push_back (MenuElem (_("Select All Before Playhead"), sigc::bind (sigc::mem_fun(*this, &Editor::select_all_selectables_using_cursor), _playhead_cursor, false)));

	edit_items.push_back (MenuElem (_("Select"), *select_menu));

	/* Cut-n-Paste */
#if 0 // unused, why?
	Menu *cutnpaste_menu = manage (new Menu);
	MenuList& cutnpaste_items = cutnpaste_menu->items();
	cutnpaste_menu->set_name ("ArdourContextMenu");

	cutnpaste_items.push_back (MenuElem (_("Cut"), sigc::mem_fun(*this, &Editor::cut)));
	cutnpaste_items.push_back (MenuElem (_("Copy"), sigc::mem_fun(*this, &Editor::copy)));
	cutnpaste_items.push_back (MenuElem (_("Paste"), sigc::bind (sigc::mem_fun(*this, &Editor::paste), 1.0f, true)));
#endif

	Menu *nudge_menu = manage (new Menu());
	MenuList& nudge_items = nudge_menu->items();
	nudge_menu->set_name ("ArdourContextMenu");

	edit_items.push_back (SeparatorElem());
	nudge_items.push_back (MenuElem (_("Nudge Entire Track Later"), (sigc::bind (sigc::mem_fun(*this, &Editor::nudge_track), false, true))));
	nudge_items.push_back (MenuElem (_("Nudge Track After Edit Point Later"), (sigc::bind (sigc::mem_fun(*this, &Editor::nudge_track), true, true))));
	nudge_items.push_back (MenuElem (_("Nudge Entire Track Earlier"), (sigc::bind (sigc::mem_fun(*this, &Editor::nudge_track), false, false))));
	nudge_items.push_back (MenuElem (_("Nudge Track After Edit Point Earlier"), (sigc::bind (sigc::mem_fun(*this, &Editor::nudge_track), true, false))));

	edit_items.push_back (MenuElem (_("Nudge"), *nudge_menu));
}

GridType
Editor::grid_type() const
{
	return _grid_type;
}

GridType
Editor::draw_length() const
{
	return _draw_length;
}

int
Editor::draw_velocity() const
{
	return _draw_velocity;
}

int
Editor::draw_channel() const
{
	return _draw_channel;
}

bool
Editor::grid_musical() const
{
	return grid_type_is_musical (_grid_type);
}

bool
Editor::grid_type_is_musical(GridType gt) const
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
Editor::snap_mode() const
{
	return _snap_mode;
}

void
Editor::show_rulers_for_grid ()
{
	/* show appropriate rulers for this grid setting. */
	if (grid_musical()) {
		ruler_tempo_action->set_active(true);
		ruler_meter_action->set_active(true);
		ruler_bbt_action->set_active(true);

		if (UIConfiguration::instance().get_rulers_follow_grid()) {
			ruler_timecode_action->set_active(false);
			ruler_minsec_action->set_active(false);
			ruler_samples_action->set_active(false);
		}
	} else if (_grid_type == GridTypeTimecode) {
		ruler_timecode_action->set_active(true);

		if (UIConfiguration::instance().get_rulers_follow_grid()) {
			ruler_tempo_action->set_active(false);
			ruler_meter_action->set_active(false);
			ruler_bbt_action->set_active(false);
			ruler_minsec_action->set_active(false);
			ruler_samples_action->set_active(false);
		}
	} else if (_grid_type == GridTypeMinSec) {
		ruler_minsec_action->set_active(true);

		if (UIConfiguration::instance().get_rulers_follow_grid()) {
			ruler_tempo_action->set_active(false);
			ruler_meter_action->set_active(false);
			ruler_bbt_action->set_active(false);
			ruler_timecode_action->set_active(false);
			ruler_samples_action->set_active(false);
		}
	} else if (_grid_type == GridTypeCDFrame) {
		ruler_cd_marker_action->set_active(true);
		ruler_minsec_action->set_active(true);

		if (UIConfiguration::instance().get_rulers_follow_grid()) {
			ruler_tempo_action->set_active(false);
			ruler_meter_action->set_active(false);
			ruler_bbt_action->set_active(false);
			ruler_timecode_action->set_active(false);
			ruler_samples_action->set_active(false);
		}
	}
}

void
Editor::set_draw_length_to (GridType gt)
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
	string str = grid_type_strings[grid_index];
	if (str != draw_length_selector.get_text()) {
		draw_length_selector.set_text (str);
	}

	instant_save ();
}

void
Editor::set_draw_velocity_to (int v)
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
Editor::set_draw_channel_to (int c)
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
Editor::set_grid_to (GridType gt)
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

	string str = grid_type_strings[grid_ind];

	if (str != grid_type_selector.get_text()) {
		grid_type_selector.set_text (str);
	}

	if (grid_type_changed && UIConfiguration::instance().get_show_grids_ruler()) {
		show_rulers_for_grid ();
	}

	instant_save ();

	if (grid_musical()) {
		compute_bbt_ruler_scale (_leftmost_sample, _leftmost_sample + current_page_samples());
		update_tempo_based_rulers ();
	}

	mark_region_boundary_cache_dirty ();

	redisplay_grid (false);

	SnapChanged (); /* EMIT SIGNAL */
}

void
Editor::set_snap_mode (SnapMode mode)
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

void
Editor::set_edit_point_preference (EditPoint ep, bool force)
{
	if (Profile->get_mixbus()) {
		if (ep == EditAtSelectedMarker) {
			ep = EditAtPlayhead;
		}
	}

	bool changed = (_edit_point != ep);

	_edit_point = ep;

	string str = edit_point_strings[(int)ep];
	if (str != edit_point_selector.get_text ()) {
		edit_point_selector.set_text (str);
	}

	update_all_enter_cursors();

	if (!force && !changed) {
		return;
	}

	const char* action=NULL;

	switch (_edit_point) {
	case EditAtPlayhead:
		action = "edit-at-playhead";
		break;
	case EditAtSelectedMarker:
		action = "edit-at-selected-marker";
		break;
	case EditAtMouse:
		action = "edit-at-mouse";
		break;
	}

	Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action ("Editor", action);
	tact->set_active (true);

	samplepos_t foo;
	bool in_track_canvas;

	if (!mouse_sample (foo, in_track_canvas)) {
		in_track_canvas = false;
	}

	reset_canvas_action_sensitivity (in_track_canvas);
	sensitize_the_right_region_actions (false);

	instant_save ();
}

int
Editor::set_state (const XMLNode& node, int version)
{
	set_id (node);
	PBD::Unwinder<bool> nsi (no_save_instant, true);
	bool yn;

	Tabbable::set_state (node, version);

	samplepos_t ph_pos;
	if (_session && node.get_property ("playhead", ph_pos)) {
		if (ph_pos >= 0) {
			_playhead_cursor->set_position (ph_pos);
		} else {
			warning << _("Playhead position stored with a negative value - ignored (use zero instead)") << endmsg;
			_playhead_cursor->set_position (0);
		}
	} else {
		_playhead_cursor->set_position (0);
	}

	node.get_property ("mixer-width", editor_mixer_strip_width);

	node.get_property ("zoom-focus", zoom_focus);
	zoom_focus_selection_done (zoom_focus);

	double z;
	if (node.get_property ("zoom", z)) {
		/* older versions of ardour used floating point samples_per_pixel */
		reset_zoom (llrintf (z));
	} else {
		reset_zoom (samples_per_pixel);
	}

	int32_t cnt;
	if (node.get_property ("visible-track-count", cnt)) {
		set_visible_track_count (cnt);
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

	double y_origin;
	if (node.get_property ("y-origin", y_origin)) {
		reset_y_origin (y_origin);
	}

	yn = false;
	node.get_property ("join-object-range", yn);
	{
		RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("MouseMode"), X_("set-mouse-mode-object-range"));
		/* do it twice to force the change */
		tact->set_active (!yn);
		tact->set_active (yn);
		set_mouse_mode(mouse_mode, true);
	}

	EditPoint ep;
	if (node.get_property ("edit-point", ep)) {
		set_edit_point_preference (ep, true);
	} else {
		set_edit_point_preference (_edit_point);
	}

	if (node.get_property ("follow-playhead", yn)) {
		set_follow_playhead (yn);
	}

	if (node.get_property ("stationary-playhead", yn)) {
		set_stationary_playhead (yn);
	}

	if (node.get_property ("show-editor-mixer", yn)) {

		Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Editor"), X_("show-editor-mixer"));
		/* do it twice to force the change */
		tact->set_active (!yn);
		tact->set_active (yn);
	}

	yn = false;
	node.get_property ("show-editor-list", yn);
	{
		Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Editor"), X_("show-editor-list"));
		/* do it twice to force the change */
		tact->set_active (!yn);
		tact->set_active (yn);
	}

	int32_t el_page;
	if (node.get_property (X_("editor-list-page"), el_page)) {
		_the_notebook.set_current_page (el_page);
	}

	yn = false;
	node.get_property (X_("show-marker-lines"), yn);
	{
		Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Editor"), X_("show-marker-lines"));
		/* do it twice to force the change */
		tact->set_active (!yn);
		tact->set_active (yn);
	}

	yn = false;
	node.get_property (X_("show-touched-automation"), yn);
	{
		Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Editor"), X_("show-touched-automation"));
		/* do it twice to force the change */
		tact->set_active (!yn);
		tact->set_active (yn);
	}

	XMLNodeList children = node.children ();
	for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
		selection->set_state (**i, Stateful::current_state_version);
		_locations->set_state (**i);
	}

	if (node.get_property ("maximised", yn)) {
		Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Common"), X_("ToggleMaximalEditor"));
		bool fs = tact->get_active();
		if (yn ^ fs) {
			ActionManager::do_action ("Common", "ToggleMaximalEditor");
		}
	}

	timepos_t nudge_clock_value;
	if (node.get_property ("nudge-clock-value", nudge_clock_value)) {
		nudge_clock->set (nudge_clock_value);
	} else {
		nudge_clock->set_mode (AudioClock::Timecode);
		nudge_clock->set (timepos_t (_session->sample_rate() * 5), true);
	}

	{
		/* apply state
		 * Not all properties may have been in XML, but
		 * those that are linked to a private variable may need changing
		 */
		RefPtr<ToggleAction> tact;

		tact = ActionManager::get_toggle_action (X_("Editor"), X_("toggle-follow-playhead"));
		yn = _follow_playhead;
		if (tact->get_active() != yn) {
			tact->set_active (yn);
		}

		tact = ActionManager::get_toggle_action (X_("Editor"), X_("toggle-stationary-playhead"));
		yn = _stationary_playhead;
		if (tact->get_active() != yn) {
			tact->set_active (yn);
		}
	}

	return 0;
}

XMLNode&
Editor::get_state ()
{
	XMLNode* node = new XMLNode (X_("Editor"));

	node->set_property ("id", id().to_s ());

	node->add_child_nocopy (Tabbable::get_state());

	node->set_property("edit-horizontal-pane-pos", edit_pane.get_divider ());
	node->set_property("notebook-shrunk", _notebook_shrunk);
	node->set_property("edit-vertical-pane-pos", editor_summary_pane.get_divider());

	maybe_add_mixer_strip_width (*node);

	node->set_property ("zoom-focus", zoom_focus);

	node->set_property ("zoom", samples_per_pixel);
	node->set_property ("grid-type", _grid_type);
	node->set_property ("snap-mode", _snap_mode);
	node->set_property ("internal-grid-type", internal_grid_type);
	node->set_property ("internal-snap-mode", internal_snap_mode);
	node->set_property ("pre-internal-grid-type", pre_internal_grid_type);
	node->set_property ("pre-internal-snap-mode", pre_internal_snap_mode);
	node->set_property ("edit-point", _edit_point);
	node->set_property ("visible-track-count", _visible_track_count);

	node->set_property ("draw-length", _draw_length);
	node->set_property ("draw-velocity", _draw_velocity);
	node->set_property ("draw-channel", _draw_channel);

	node->set_property ("playhead", _playhead_cursor->current_sample ());
	node->set_property ("left-frame", _leftmost_sample);
	node->set_property ("y-origin", vertical_adjustment.get_value ());

	node->set_property ("maximised", _maximised);
	node->set_property ("follow-playhead", _follow_playhead);
	node->set_property ("stationary-playhead", _stationary_playhead);
	node->set_property ("mouse-mode", mouse_mode);
	node->set_property ("join-object-range", smart_mode_action->get_active ());

	Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Editor"), X_("show-editor-mixer"));
	node->set_property (X_("show-editor-mixer"), tact->get_active());

	tact = ActionManager::get_toggle_action (X_("Editor"), X_("show-editor-list"));
	node->set_property (X_("show-editor-list"), tact->get_active());

	node->set_property (X_("editor-list-page"), _the_notebook.get_current_page ());

	if (button_bindings) {
		XMLNode* bb = new XMLNode (X_("Buttons"));
		button_bindings->save (*bb);
		node->add_child_nocopy (*bb);
	}

	node->set_property (X_("show-marker-lines"), _show_marker_lines);
	node->set_property (X_("show-touched-automation"), _show_touched_automation);

	node->add_child_nocopy (selection->get_state ());

	node->set_property ("nudge-clock-value", nudge_clock->current_duration());

	node->add_child_nocopy (_locations->get_state ());

	return *node;
}

/** if @param trackview_relative_offset is true, @param y y is an offset into the trackview area, in pixel units
 *  if @param trackview_relative_offset is false, @param y y is a global canvas *  coordinate, in pixel units
 *
 *  @return pair: TimeAxisView that y is over, layer index.
 *
 *  TimeAxisView may be 0.  Layer index is the layer number if the TimeAxisView is valid and is
 *  in stacked or expanded region display mode, otherwise 0.
 */
std::pair<TimeAxisView *, double>
Editor::trackview_by_y_position (double y, bool trackview_relative_offset) const
{
	if (!trackview_relative_offset) {
		y -= _trackview_group->canvas_origin().y;
	}

	if (y < 0) {
		return std::make_pair ((TimeAxisView *) 0, 0);
	}

	for (TrackViewList::const_iterator iter = track_views.begin(); iter != track_views.end(); ++iter) {

		std::pair<TimeAxisView*, double> const r = (*iter)->covers_y_position (y);

		if (r.first) {
			return r;
		}
	}

	return std::make_pair ((TimeAxisView *) 0, 0);
}

void
Editor::set_snapped_cursor_position (timepos_t const & pos)
{
	if (_edit_point == EditAtMouse) {
		_snapped_cursor->set_position (pos.samples());
	}
}


/** Snap a position to the grid, if appropriate, taking into account current
 *  grid settings and also the state of any snap modifier keys that may be pressed.
 *  @param start Position to snap.
 *  @param event Event to get current key modifier information from, or 0.
 */
void
Editor::snap_to_with_modifier (timepos_t& start, GdkEvent const * event, Temporal::RoundMode direction, SnapPref pref)
{
	if (!_session || !event) {
		return;
	}

	if (ArdourKeyboard::indicates_snap (event->button.state)) {
		if (_snap_mode == SnapOff) {
			snap_to_internal (start, direction, pref);
		}

	} else {
		if (_snap_mode != SnapOff) {
			snap_to_internal (start, direction, pref);
		} else if (ArdourKeyboard::indicates_snap_delta (event->button.state)) {
			/* SnapOff, but we pressed the snap_delta modifier */
			snap_to_internal (start, direction, pref);
		}
	}
}

void
Editor::snap_to (timepos_t& start, Temporal::RoundMode direction, SnapPref pref, bool ensure_snap)
{
	if (!_session || (_snap_mode == SnapOff && !ensure_snap)) {
		return;
	}

	snap_to_internal (start, direction, pref, ensure_snap);
}

static void
check_best_snap (timepos_t const & presnap, timepos_t &test, timepos_t &dist, timepos_t &best)
{
	timepos_t diff = timepos_t (presnap.distance (test).abs ());
	if (diff < dist) {
		dist = diff;
		best = test;
	}

	test = timepos_t::max (test.time_domain()); // reset this so it doesn't get accidentally reused
}

timepos_t
Editor::snap_to_timecode (timepos_t const & presnap, Temporal::RoundMode direction, SnapPref gpref)
{
	timepos_t start = presnap;
	samplepos_t start_sample = presnap.samples();
	const samplepos_t one_timecode_second = (samplepos_t)(rint(_session->timecode_frames_per_second()) * _session->samples_per_timecode_frame());
	samplepos_t one_timecode_minute = (samplepos_t)(rint(_session->timecode_frames_per_second()) * _session->samples_per_timecode_frame() * 60);

	TimecodeRulerScale scale = (gpref != SnapToGrid_Unscaled) ? timecode_ruler_scale : timecode_show_samples;

	switch (scale) {
	case timecode_show_bits:
	case timecode_show_samples:
		if ((direction == Temporal::RoundUpMaybe || direction == Temporal::RoundDownMaybe) &&
		    fmod((double)start_sample, (double)_session->samples_per_timecode_frame()) == 0) {
			/* start is already on a whole timecode frame, do nothing */
		} else if (((direction == 0) && (fmod((double)start_sample, (double)_session->samples_per_timecode_frame()) > (_session->samples_per_timecode_frame() / 2))) || (direction > 0)) {
			start_sample = (samplepos_t) (ceil ((double) start_sample / _session->samples_per_timecode_frame()) * _session->samples_per_timecode_frame());
		} else {
			start_sample = (samplepos_t) (floor ((double) start_sample / _session->samples_per_timecode_frame()) *  _session->samples_per_timecode_frame());
		}
		start = timepos_t (start_sample);
		break;

	case timecode_show_seconds:
		if (_session->config.get_timecode_offset_negative()) {
			start_sample += _session->config.get_timecode_offset ();
		} else {
			start_sample -= _session->config.get_timecode_offset ();
		}
		if ((direction == Temporal::RoundUpMaybe || direction == Temporal::RoundDownMaybe) &&
		    (start_sample % one_timecode_second == 0)) {
			/* start is already on a whole second, do nothing */
		} else if (((direction == 0) && (start_sample % one_timecode_second > one_timecode_second / 2)) || direction > 0) {
			start_sample = (samplepos_t) ceil ((double) start_sample / one_timecode_second) * one_timecode_second;
		} else {
			start_sample = (samplepos_t) floor ((double) start_sample / one_timecode_second) * one_timecode_second;
		}

		if (_session->config.get_timecode_offset_negative()) {
			start_sample -= _session->config.get_timecode_offset ();
		} else {
			start_sample += _session->config.get_timecode_offset ();
		}
		start = timepos_t (start_sample);
		break;

	case timecode_show_minutes:
	case timecode_show_hours:
	case timecode_show_many_hours:
		if (_session->config.get_timecode_offset_negative()) {
			start_sample += _session->config.get_timecode_offset ();
		} else {
			start_sample -= _session->config.get_timecode_offset ();
		}
		if ((direction == Temporal::RoundUpMaybe || direction == Temporal::RoundDownMaybe) &&
		    (start_sample % one_timecode_minute == 0)) {
			/* start is already on a whole minute, do nothing */
		} else if (((direction == 0) && (start_sample % one_timecode_minute > one_timecode_minute / 2)) || direction > 0) {
			start_sample = (samplepos_t) ceil ((double) start_sample / one_timecode_minute) * one_timecode_minute;
		} else {
			start_sample = (samplepos_t) floor ((double) start_sample / one_timecode_minute) * one_timecode_minute;
		}
		if (_session->config.get_timecode_offset_negative()) {
			start_sample -= _session->config.get_timecode_offset ();
		} else {
			start_sample += _session->config.get_timecode_offset ();
		}
		start = timepos_t (start_sample);
		break;
	default:
		fatal << "Editor::smpte_snap_to_internal() called with non-timecode snap type!" << endmsg;
	}

	return start;
}

timepos_t
Editor::snap_to_minsec (timepos_t const & presnap, Temporal::RoundMode direction, SnapPref gpref)
{
	samplepos_t presnap_sample = presnap.samples ();

	const samplepos_t one_second = _session->sample_rate();
	const samplepos_t one_minute = one_second * 60;
	const samplepos_t one_hour = one_minute * 60;

	MinsecRulerScale scale = (gpref != SnapToGrid_Unscaled) ? minsec_ruler_scale : minsec_show_seconds;

	switch (scale) {
		case minsec_show_msecs:
		case minsec_show_seconds: {
			if ((direction == Temporal::RoundUpMaybe || direction == Temporal::RoundDownMaybe) &&
				presnap_sample % one_second == 0) {
				/* start is already on a whole second, do nothing */
			} else if (((direction == 0) && (presnap_sample % one_second > one_second / 2)) || (direction > 0)) {
				presnap_sample = (samplepos_t) ceil ((double) presnap_sample / one_second) * one_second;
			} else {
				presnap_sample = (samplepos_t) floor ((double) presnap_sample / one_second) * one_second;
			}
		} break;

		case minsec_show_minutes: {
			if ((direction == Temporal::RoundUpMaybe || direction == Temporal::RoundDownMaybe) &&
				presnap_sample % one_minute == 0) {
				/* start is already on a whole minute, do nothing */
			} else if (((direction == 0) && (presnap_sample % one_minute > one_minute / 2)) || (direction > 0)) {
				presnap_sample = (samplepos_t) ceil ((double) presnap_sample / one_minute) * one_minute;
			} else {
				presnap_sample = (samplepos_t) floor ((double) presnap_sample / one_minute) * one_minute;
			}
		} break;

		default: {
			if ((direction == Temporal::RoundUpMaybe || direction == Temporal::RoundDownMaybe) &&
				presnap_sample % one_hour == 0) {
				/* start is already on a whole hour, do nothing */
			} else if (((direction == 0) && (presnap_sample % one_hour > one_hour / 2)) || (direction > 0)) {
				presnap_sample = (samplepos_t) ceil ((double) presnap_sample / one_hour) * one_hour;
			} else {
				presnap_sample = (samplepos_t) floor ((double) presnap_sample / one_hour) * one_hour;
			}
		} break;
	}

	return timepos_t (presnap_sample);
}

timepos_t
Editor::snap_to_cd_frames (timepos_t const & presnap, Temporal::RoundMode direction, SnapPref gpref)
{
	if ((gpref != SnapToGrid_Unscaled) && (minsec_ruler_scale != minsec_show_msecs)) {
		return snap_to_minsec (presnap, direction, gpref);
	}

	const samplepos_t one_second = _session->sample_rate();

	samplepos_t presnap_sample = presnap.samples();

	if ((direction == Temporal::RoundUpMaybe || direction == Temporal::RoundDownMaybe) &&
		presnap_sample % (one_second/75) == 0) {
		/* start is already on a whole CD sample, do nothing */
	} else if (((direction == 0) && (presnap_sample % (one_second/75) > (one_second/75) / 2)) || (direction > 0)) {
		presnap_sample = (samplepos_t) ceil ((double) presnap_sample / (one_second / 75)) * (one_second / 75);
	} else {
		presnap_sample = (samplepos_t) floor ((double) presnap_sample / (one_second / 75)) * (one_second / 75);
	}

	return timepos_t (presnap_sample);
}

timepos_t
Editor::snap_to_bbt (timepos_t const & presnap, Temporal::RoundMode direction, SnapPref gpref)
{
	timepos_t ret(presnap);
	TempoMap::SharedPtr tmap (TempoMap::use());

	if (gpref != SnapToGrid_Unscaled) { // use the visual grid lines which are limited by the zoom scale that the user selected

		int divisor = 2;
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
		default:
			divisor = 2;
		};

		BBTRulerScale scale = bbt_ruler_scale;
		switch (scale) {
			case bbt_show_many:
			case bbt_show_64:
			case bbt_show_16:
			case bbt_show_4:
			case bbt_show_1:
				ret = timepos_t (tmap->quarters_at (tmap->round_to_bar (tmap->bbt_at (presnap))));
				break;
			case bbt_show_quarters:
				ret = timepos_t (tmap->quarters_at (presnap).round_to_beat ());
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
		ret = timepos_t (tmap->quarters_at (presnap).round_to_subdivision (get_grid_beat_divisions(_grid_type), direction));
	}

	return ret;
}

timepos_t
Editor::snap_to_grid (timepos_t const & presnap, Temporal::RoundMode direction, SnapPref gpref)
{
	timepos_t ret(presnap);

	if (grid_musical()) {
		ret = snap_to_bbt (presnap, direction, gpref);
	}

	switch (_grid_type) {
		case GridTypeTimecode:
			ret = snap_to_timecode(presnap, direction, gpref);
			break;
		case GridTypeMinSec:
			ret = snap_to_minsec(presnap, direction, gpref);
			break;
		case GridTypeCDFrame:
			ret = snap_to_cd_frames(presnap, direction, gpref);
			break;
		default:
			{}
	};

	return ret;
}

timepos_t
Editor::snap_to_marker (timepos_t const & presnap, Temporal::RoundMode direction)
{
	timepos_t before;
	timepos_t after;
	timepos_t test;

	if (_session->locations()->list().empty()) {
		/* No marks to snap to, so just don't snap */
		return timepos_t();
	}

	_session->locations()->marks_either_side (presnap, before, after);

	if (before == timepos_t::max (before.time_domain())) {
		test = after;
	} else if (after == timepos_t::max (after.time_domain())) {
		test = before;
	} else  {
		if ((direction == Temporal::RoundUpMaybe || direction == Temporal::RoundUpAlways)) {
			test = after;
		} else if ((direction == Temporal::RoundDownMaybe || direction == Temporal::RoundDownAlways)) {
			test = before;
		} else if (direction ==  0) {
			if (before.distance (presnap) < presnap.distance (after)) {
				test = before;
			} else {
				test = after;
			}
		}
	}

	return test;
}

void
Editor::snap_to_internal (timepos_t& start, Temporal::RoundMode direction, SnapPref pref, bool ensure_snap)
{
	UIConfiguration const& uic (UIConfiguration::instance ());
	const timepos_t presnap = start;

	timepos_t test = timepos_t::max (start.time_domain()); // for each snap, we'll use this value
	timepos_t dist = timepos_t::max (start.time_domain()); // this records the distance of the best snap result we've found so far
	timepos_t best = timepos_t::max (start.time_domain()); // this records the best snap-result we've found so far

	/* check snap-to-marker */
	if ((pref == SnapToAny_Visual) && uic.get_snap_to_marks ()) {
		test = snap_to_marker (presnap, direction);
		check_best_snap (presnap, test, dist, best);
	}

	/* check snap-to-region-{start/end/sync} */
	if ((pref == SnapToAny_Visual) && (uic.get_snap_to_region_start () || uic.get_snap_to_region_end () || uic.get_snap_to_region_sync ())) {

		if (!region_boundary_cache.empty ()) {

			vector<timepos_t>::iterator prev = region_boundary_cache.begin ();
			vector<timepos_t>::iterator next = std::upper_bound (region_boundary_cache.begin (), region_boundary_cache.end (), presnap);
			if (next != region_boundary_cache.begin ()) {
				prev = next;
				prev--;
			}
			if (next == region_boundary_cache.end ()) {
				next--;
			}

			if ((direction == Temporal::RoundUpMaybe || direction == Temporal::RoundUpAlways)) {
				test = *next;
			} else if ((direction == Temporal::RoundDownMaybe || direction == Temporal::RoundDownAlways)) {
				test = *prev;
			} else if (direction ==  0) {
				if ((*prev).distance (presnap) < presnap.distance (*next)) {
					test = *prev;
				} else {
					test = *next;
				}
			}

		}

		check_best_snap (presnap, test, dist, best);
	}

	/* check Grid */
	if (uic.get_snap_to_grid () && (_grid_type != GridTypeNone)) {
		timepos_t pre (presnap);
		timepos_t post (snap_to_grid (pre, direction, pref));
		check_best_snap (presnap, post, dist, best);
	}

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
Editor::setup_toolbar ()
{
	HBox* mode_box = manage(new HBox);
	mode_box->set_border_width (2);
	mode_box->set_spacing(2);

	HBox* mouse_mode_box = manage (new HBox);
	HBox* mouse_mode_hbox = manage (new HBox);
	VBox* mouse_mode_vbox = manage (new VBox);
	Alignment* mouse_mode_align = manage (new Alignment);

	Glib::RefPtr<SizeGroup> mouse_mode_size_group = SizeGroup::create (SIZE_GROUP_VERTICAL);
	mouse_mode_size_group->add_widget (smart_mode_button);
	mouse_mode_size_group->add_widget (mouse_move_button);
	mouse_mode_size_group->add_widget (mouse_cut_button);
	mouse_mode_size_group->add_widget (mouse_select_button);
	mouse_mode_size_group->add_widget (mouse_timefx_button);
	if (!Profile->get_mixbus()) {
		mouse_mode_size_group->add_widget (mouse_audition_button);
	}
	mouse_mode_size_group->add_widget (mouse_draw_button);
	mouse_mode_size_group->add_widget (mouse_content_button);

	if (!Profile->get_mixbus()) {
		mouse_mode_size_group->add_widget (zoom_in_button);
		mouse_mode_size_group->add_widget (zoom_out_button);
		mouse_mode_size_group->add_widget (zoom_out_full_button);
		mouse_mode_size_group->add_widget (zoom_focus_selector);
		mouse_mode_size_group->add_widget (tav_shrink_button);
		mouse_mode_size_group->add_widget (tav_expand_button);
	} else {
		mouse_mode_size_group->add_widget (zoom_preset_selector);
		mouse_mode_size_group->add_widget (visible_tracks_selector);
	}

	mouse_mode_size_group->add_widget (grid_type_selector);
	mouse_mode_size_group->add_widget (draw_length_selector);
	mouse_mode_size_group->add_widget (draw_velocity_selector);
	mouse_mode_size_group->add_widget (draw_channel_selector);
	mouse_mode_size_group->add_widget (snap_mode_button);

	mouse_mode_size_group->add_widget (edit_point_selector);
	mouse_mode_size_group->add_widget (edit_mode_selector);

	mouse_mode_size_group->add_widget (*nudge_clock);
	mouse_mode_size_group->add_widget (nudge_forward_button);
	mouse_mode_size_group->add_widget (nudge_backward_button);

	mouse_mode_hbox->set_spacing (2);
	mouse_mode_hbox->pack_start (smart_mode_button, false, false);

	mouse_mode_hbox->pack_start (mouse_move_button, false, false);
	mouse_mode_hbox->pack_start (mouse_select_button, false, false);

	mouse_mode_hbox->pack_start (mouse_cut_button, false, false);

	if (!ARDOUR::Profile->get_mixbus()) {
		mouse_mode_hbox->pack_start (mouse_audition_button, false, false);
	}

	mouse_mode_hbox->pack_start (mouse_timefx_button, false, false);
	mouse_mode_hbox->pack_start (mouse_draw_button, false, false);
	mouse_mode_hbox->pack_start (mouse_content_button, false, false);

	mouse_mode_vbox->pack_start (*mouse_mode_hbox);

	mouse_mode_align->add (*mouse_mode_vbox);
	mouse_mode_align->set (0.5, 1.0, 0.0, 0.0);

	mouse_mode_box->pack_start (*mouse_mode_align, false, false);

	edit_mode_selector.set_name ("mouse mode button");

	mode_box->pack_start (edit_mode_selector, false, false);
	mode_box->pack_start (*(manage (new ArdourVSpacer ())), false, false, 3);
	mode_box->pack_start (edit_point_selector, false, false);
	mode_box->pack_start (*(manage (new ArdourVSpacer ())), false, false, 3);

	mode_box->pack_start (*mouse_mode_box, false, false);

	/* Zoom */

	_zoom_box.set_spacing (2);
	_zoom_box.set_border_width (2);

	RefPtr<Action> act;

	zoom_preset_selector.set_name ("zoom button");
	zoom_preset_selector.set_icon (ArdourIcon::ZoomExpand);

	zoom_in_button.set_name ("zoom button");
	zoom_in_button.set_icon (ArdourIcon::ZoomIn);
	act = ActionManager::get_action (X_("Editor"), X_("temporal-zoom-in"));
	zoom_in_button.set_related_action (act);

	zoom_out_button.set_name ("zoom button");
	zoom_out_button.set_icon (ArdourIcon::ZoomOut);
	act = ActionManager::get_action (X_("Editor"), X_("temporal-zoom-out"));
	zoom_out_button.set_related_action (act);

	zoom_out_full_button.set_name ("zoom button");
	zoom_out_full_button.set_icon (ArdourIcon::ZoomFull);
	act = ActionManager::get_action (X_("Editor"), X_("zoom-to-session"));
	zoom_out_full_button.set_related_action (act);

	zoom_focus_selector.set_name ("zoom button");

	if (ARDOUR::Profile->get_mixbus()) {
		_zoom_box.pack_start (zoom_preset_selector, false, false);
	} else {
		_zoom_box.pack_start (zoom_out_button, false, false);
		_zoom_box.pack_start (zoom_in_button, false, false);
		_zoom_box.pack_start (zoom_out_full_button, false, false);
		_zoom_box.pack_start (zoom_focus_selector, false, false);
	}

	/* Track zoom buttons */
	_track_box.set_spacing (2);
	_track_box.set_border_width (2);

	visible_tracks_selector.set_name ("zoom button");
	if (Profile->get_mixbus()) {
		visible_tracks_selector.set_icon (ArdourIcon::TimeAxisExpand);
	} else {
		set_size_request_to_display_given_text (visible_tracks_selector, _("All"), 30, 2);
	}

	tav_expand_button.set_name ("zoom button");
	tav_expand_button.set_icon (ArdourIcon::TimeAxisExpand);
	act = ActionManager::get_action (X_("Editor"), X_("expand-tracks"));
	tav_expand_button.set_related_action (act);

	tav_shrink_button.set_name ("zoom button");
	tav_shrink_button.set_icon (ArdourIcon::TimeAxisShrink);
	act = ActionManager::get_action (X_("Editor"), X_("shrink-tracks"));
	tav_shrink_button.set_related_action (act);

	if (ARDOUR::Profile->get_mixbus()) {
		_track_box.pack_start (visible_tracks_selector);
	} else {
		_track_box.pack_start (visible_tracks_selector);
		_track_box.pack_start (tav_shrink_button);
		_track_box.pack_start (tav_expand_button);
	}

	snap_box.set_spacing (2);
	snap_box.set_border_width (2);

	grid_type_selector.set_name ("mouse mode button");
	draw_length_selector.set_name ("mouse mode button");
	draw_velocity_selector.set_name ("mouse mode button");
	draw_channel_selector.set_name ("mouse mode button");

	grid_type_selector.set_sizing_text (grid_type_strings[(int)GridTypeBeatDiv32]);
	draw_length_selector.set_sizing_text (grid_type_strings[(int)GridTypeBeatDiv32]);
	draw_velocity_selector.set_sizing_text (_("Auto"));
	draw_channel_selector.set_sizing_text (_("Auto"));

	draw_velocity_selector.disable_scrolling ();
	draw_velocity_selector.signal_scroll_event().connect (sigc::mem_fun(*this, &Editor::on_velocity_scroll_event), false);

	snap_mode_button.set_name ("mouse mode button");

	edit_point_selector.set_name ("mouse mode button");

	snap_box.pack_start (snap_mode_button, false, false);
	snap_box.pack_start (grid_type_selector, false, false);

	/* Nudge */

	HBox *nudge_box = manage (new HBox);
	nudge_box->set_spacing (2);
	nudge_box->set_border_width (2);

	nudge_forward_button.signal_button_release_event().connect (sigc::mem_fun(*this, &Editor::nudge_forward_release), false);
	nudge_backward_button.signal_button_release_event().connect (sigc::mem_fun(*this, &Editor::nudge_backward_release), false);

	nudge_box->pack_start (nudge_backward_button, false, false);
	nudge_box->pack_start (nudge_forward_button, false, false);
	nudge_box->pack_start (*nudge_clock, false, false);

	/* Draw  - these MIDI tools are only visible when in Draw mode */
	draw_box.set_spacing (2);
	draw_box.set_border_width (2);
	draw_box.pack_start (*manage (new Label (_("Len:"))), false, false);
	draw_box.pack_start (draw_length_selector, false, false, 4);
	draw_box.pack_start (*manage (new Label (_("Ch:"))), false, false);
	draw_box.pack_start (draw_channel_selector, false, false, 4);
	draw_box.pack_start (*manage (new Label (_("Vel:"))), false, false);
	draw_box.pack_start (draw_velocity_selector, false, false, 4);

	/* Pack everything in... */

	toolbar_hbox.set_spacing (2);
	toolbar_hbox.set_border_width (2);

	ArdourWidgets::ArdourDropShadow *tool_shadow = manage (new (ArdourWidgets::ArdourDropShadow));
	tool_shadow->set_size_request (4, -1);
	tool_shadow->show();

	ebox_hpacker.pack_start (*tool_shadow, false, false);
	ebox_hpacker.pack_start(ebox_vpacker, true, true);

	Gtk::EventBox* spacer = manage (new Gtk::EventBox); // extra space under the mouse toolbar, for aesthetics
	spacer->set_name("EditorWindow");
	spacer->set_size_request(-1,4);
	spacer->show();

	ebox_vpacker.pack_start(toolbar_hbox, false, false);
	ebox_vpacker.pack_start(*spacer, false, false);
	ebox_vpacker.show();

	toolbar_hbox.pack_start (*mode_box, false, false);
	toolbar_hbox.pack_start (*(manage (new ArdourVSpacer ())), false, false, 3);
	toolbar_hbox.pack_start (snap_box, false, false);
	toolbar_hbox.pack_start (*(manage (new ArdourVSpacer ())), false, false, 3);
	toolbar_hbox.pack_start (*nudge_box, false, false);
	toolbar_hbox.pack_start (*(manage (new ArdourVSpacer ())), false, false, 3);
	toolbar_hbox.pack_start (draw_box, false, false);
	toolbar_hbox.pack_end (_zoom_box, false, false, 2);
	toolbar_hbox.pack_end (*(manage (new ArdourVSpacer ())), false, false, 3);
	toolbar_hbox.pack_end (_track_box, false, false);

	toolbar_hbox.show_all ();
}

bool
Editor::on_velocity_scroll_event (GdkEventScroll* ev)
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
Editor::build_edit_point_menu ()
{
	using namespace Menu_Helpers;

	edit_point_selector.AddMenuElem (MenuElem (edit_point_strings[(int)EditAtPlayhead], sigc::bind (sigc::mem_fun(*this, &Editor::edit_point_selection_done), (EditPoint) EditAtPlayhead)));
	if(!Profile->get_mixbus())
		edit_point_selector.AddMenuElem (MenuElem (edit_point_strings[(int)EditAtSelectedMarker], sigc::bind (sigc::mem_fun(*this, &Editor::edit_point_selection_done), (EditPoint) EditAtSelectedMarker)));
	edit_point_selector.AddMenuElem (MenuElem (edit_point_strings[(int)EditAtMouse], sigc::bind (sigc::mem_fun(*this, &Editor::edit_point_selection_done), (EditPoint) EditAtMouse)));

	set_size_request_to_display_given_text (edit_point_selector, edit_point_strings, COMBO_TRIANGLE_WIDTH, 2);
}

void
Editor::build_edit_mode_menu ()
{
	using namespace Menu_Helpers;

	edit_mode_selector.AddMenuElem (MenuElem (edit_mode_strings[(int)Slide], sigc::bind (sigc::mem_fun(*this, &Editor::edit_mode_selection_done), (EditMode) Slide)));
	edit_mode_selector.AddMenuElem (MenuElem (edit_mode_strings[(int)Ripple], sigc::bind (sigc::mem_fun(*this, &Editor::edit_mode_selection_done), (EditMode) Ripple)));
	edit_mode_selector.AddMenuElem (MenuElem (edit_mode_strings[(int)RippleAll], sigc::bind (sigc::mem_fun(*this, &Editor::edit_mode_selection_done), (EditMode) RippleAll)));
	edit_mode_selector.AddMenuElem (MenuElem (edit_mode_strings[(int)Lock], sigc::bind (sigc::mem_fun(*this, &Editor::edit_mode_selection_done), (EditMode)  Lock)));
	/* Note: Splice was removed */

	set_size_request_to_display_given_text (edit_mode_selector, edit_mode_strings, COMBO_TRIANGLE_WIDTH, 2);
}

void
Editor::build_grid_type_menu ()
{
	using namespace Menu_Helpers;

	/* main grid: bars, quarter-notes, etc */
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeNone],      sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_selection_done), (GridType) GridTypeNone)));
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBar],       sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_selection_done), (GridType) GridTypeBar)));
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeat],      sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_selection_done), (GridType) GridTypeBeat)));
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeatDiv2],  sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_selection_done), (GridType) GridTypeBeatDiv2)));
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeatDiv4],  sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_selection_done), (GridType) GridTypeBeatDiv4)));
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeatDiv8],  sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_selection_done), (GridType) GridTypeBeatDiv8)));
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeatDiv16], sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_selection_done), (GridType) GridTypeBeatDiv16)));
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeatDiv32], sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_selection_done), (GridType) GridTypeBeatDiv32)));

	/* triplet grid */
	grid_type_selector.AddMenuElem(SeparatorElem());
	Gtk::Menu *_triplet_menu = manage (new Menu);
	MenuList& triplet_items (_triplet_menu->items());
	{
		triplet_items.push_back (MenuElem (grid_type_strings[(int)GridTypeBeatDiv3],  sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_selection_done), (GridType) GridTypeBeatDiv3)));
		triplet_items.push_back (MenuElem (grid_type_strings[(int)GridTypeBeatDiv6],  sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_selection_done), (GridType) GridTypeBeatDiv6)));
		triplet_items.push_back (MenuElem (grid_type_strings[(int)GridTypeBeatDiv12], sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_selection_done), (GridType) GridTypeBeatDiv12)));
		triplet_items.push_back (MenuElem (grid_type_strings[(int)GridTypeBeatDiv24], sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_selection_done), (GridType) GridTypeBeatDiv24)));
	}
	grid_type_selector.AddMenuElem (Menu_Helpers::MenuElem (_("Triplets"), *_triplet_menu));

	/* quintuplet grid */
	Gtk::Menu *_quintuplet_menu = manage (new Menu);
	MenuList& quintuplet_items (_quintuplet_menu->items());
	{
		quintuplet_items.push_back (MenuElem (grid_type_strings[(int)GridTypeBeatDiv5],  sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_selection_done), (GridType) GridTypeBeatDiv5)));
		quintuplet_items.push_back (MenuElem (grid_type_strings[(int)GridTypeBeatDiv10], sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_selection_done), (GridType) GridTypeBeatDiv10)));
		quintuplet_items.push_back (MenuElem (grid_type_strings[(int)GridTypeBeatDiv20], sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_selection_done), (GridType) GridTypeBeatDiv20)));
	}
	grid_type_selector.AddMenuElem (Menu_Helpers::MenuElem (_("Quintuplets"), *_quintuplet_menu));

	/* septuplet grid */
	Gtk::Menu *_septuplet_menu = manage (new Menu);
	MenuList& septuplet_items (_septuplet_menu->items());
	{
		septuplet_items.push_back (MenuElem (grid_type_strings[(int)GridTypeBeatDiv7],  sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_selection_done), (GridType) GridTypeBeatDiv7)));
		septuplet_items.push_back (MenuElem (grid_type_strings[(int)GridTypeBeatDiv14], sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_selection_done), (GridType) GridTypeBeatDiv14)));
		septuplet_items.push_back (MenuElem (grid_type_strings[(int)GridTypeBeatDiv28], sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_selection_done), (GridType) GridTypeBeatDiv28)));
	}
	grid_type_selector.AddMenuElem (Menu_Helpers::MenuElem (_("Septuplets"), *_septuplet_menu));

	grid_type_selector.AddMenuElem(SeparatorElem());
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeTimecode], sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_selection_done), (GridType) GridTypeTimecode)));
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeMinSec], sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_selection_done), (GridType) GridTypeMinSec)));
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeCDFrame], sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_selection_done), (GridType) GridTypeCDFrame)));


	/* Note-Length when drawing */
	draw_length_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeat],      sigc::bind (sigc::mem_fun(*this, &Editor::draw_length_selection_done), (GridType) GridTypeBeat)));
	draw_length_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeatDiv2],  sigc::bind (sigc::mem_fun(*this, &Editor::draw_length_selection_done), (GridType) GridTypeBeatDiv2)));
	draw_length_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeatDiv4],  sigc::bind (sigc::mem_fun(*this, &Editor::draw_length_selection_done), (GridType) GridTypeBeatDiv4)));
	draw_length_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeatDiv8],  sigc::bind (sigc::mem_fun(*this, &Editor::draw_length_selection_done), (GridType) GridTypeBeatDiv8)));
	draw_length_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeatDiv16], sigc::bind (sigc::mem_fun(*this, &Editor::draw_length_selection_done), (GridType) GridTypeBeatDiv16)));
	draw_length_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeatDiv32], sigc::bind (sigc::mem_fun(*this, &Editor::draw_length_selection_done), (GridType) GridTypeBeatDiv32)));
	draw_length_selector.AddMenuElem (MenuElem (_("Auto"), sigc::bind (sigc::mem_fun(*this, &Editor::draw_length_selection_done), (GridType) DRAW_LEN_AUTO)));

	/* Note-Velocity when drawing */
	{
		draw_velocity_selector.AddMenuElem (MenuElem ("8",    sigc::bind (sigc::mem_fun(*this, &Editor::draw_velocity_selection_done), 8)));
		draw_velocity_selector.AddMenuElem (MenuElem ("32",   sigc::bind (sigc::mem_fun(*this, &Editor::draw_velocity_selection_done), 32)));
		draw_velocity_selector.AddMenuElem (MenuElem ("64",   sigc::bind (sigc::mem_fun(*this, &Editor::draw_velocity_selection_done), 64)));
		draw_velocity_selector.AddMenuElem (MenuElem ("82",   sigc::bind (sigc::mem_fun(*this, &Editor::draw_velocity_selection_done), 82)));
		draw_velocity_selector.AddMenuElem (MenuElem ("100",  sigc::bind (sigc::mem_fun(*this, &Editor::draw_velocity_selection_done), 100)));
		draw_velocity_selector.AddMenuElem (MenuElem ("127",  sigc::bind (sigc::mem_fun(*this, &Editor::draw_velocity_selection_done), 127)));
	}
	draw_velocity_selector.AddMenuElem (MenuElem (_("Auto"), sigc::bind (sigc::mem_fun(*this, &Editor::draw_velocity_selection_done), DRAW_VEL_AUTO)));

	/* Note-Channel when drawing */
	for (int i = 0; i<= 15; i++) {
		char buf[64];
		sprintf(buf, "%d", i+1);
		draw_channel_selector.AddMenuElem (MenuElem (buf, sigc::bind (sigc::mem_fun(*this, &Editor::draw_channel_selection_done), i)));
	}
	draw_channel_selector.AddMenuElem (MenuElem (_("Auto"), sigc::bind (sigc::mem_fun(*this, &Editor::draw_channel_selection_done), DRAW_CHAN_AUTO)));
}

void
Editor::setup_tooltips ()
{
	set_tooltip (smart_mode_button, _("Smart Mode (add range functions to Grab Mode)"));
	set_tooltip (mouse_move_button, _("Grab Mode (select/move objects)"));
	set_tooltip (mouse_cut_button, _("Cut Mode (split regions)"));
	set_tooltip (mouse_select_button, _("Range Mode (select time ranges)"));
	set_tooltip (mouse_draw_button, _("Draw Mode (draw and edit gain/notes/automation)"));
	set_tooltip (mouse_timefx_button, _("Stretch Mode (time-stretch audio and midi regions, preserving pitch)"));
	set_tooltip (mouse_audition_button, _("Audition Mode (listen to regions)"));
	set_tooltip (mouse_content_button, _("Internal Edit Mode (edit notes and automation points)"));
	set_tooltip (*_group_tabs, _("Groups: click to (de)activate\nContext-click for other operations"));
	set_tooltip (nudge_forward_button, _("Nudge Region/Selection Later"));
	set_tooltip (nudge_backward_button, _("Nudge Region/Selection Earlier"));
	set_tooltip (zoom_in_button, _("Zoom In"));
	set_tooltip (zoom_out_button, _("Zoom Out"));
	set_tooltip (zoom_preset_selector, _("Zoom to Time Scale"));
	set_tooltip (zoom_out_full_button, _("Zoom to Session"));
	set_tooltip (zoom_focus_selector, _("Zoom Focus"));
	set_tooltip (tav_expand_button, _("Expand Tracks"));
	set_tooltip (tav_shrink_button, _("Shrink Tracks"));
	set_tooltip (visible_tracks_selector, _("Number of visible tracks"));
	set_tooltip (draw_length_selector, _("Note Length to Draw (AUTO uses the current Grid setting)"));
	set_tooltip (draw_velocity_selector, _("Note Velocity to Draw (AUTO uses the nearest note's velocity)"));
	set_tooltip (draw_channel_selector, _("Note Channel to Draw (AUTO uses the nearest note's channel)"));
	set_tooltip (grid_type_selector, _("Grid Mode"));
	set_tooltip (snap_mode_button, _("Snap Mode\n\nRight-click to visit Snap preferences."));
	set_tooltip (edit_point_selector, _("Edit Point"));
	set_tooltip (edit_mode_selector, _("Edit Mode"));
	set_tooltip (nudge_clock, _("Nudge Clock\n(controls distance used to nudge regions and selections)"));
}

void
Editor::new_tempo_section ()
{
}

void
Editor::map_transport_state ()
{
	ENSURE_GUI_THREAD (*this, &Editor::map_transport_state);

	if (_session && _session->transport_stopped()) {
		have_pending_keyboard_selection = false;
	}

	update_loop_range_view ();
}

void
Editor::transport_looped ()
{
	/* reset Playhead position interpolation.
	 * see Editor::super_rapid_screen_update
	 */
	_last_update_time = 0;
}

/* UNDO/REDO */

void
Editor::begin_selection_op_history ()
{
	selection_op_cmd_depth = 0;
	selection_op_history_it = 0;

	while(!selection_op_history.empty()) {
		delete selection_op_history.front();
		selection_op_history.pop_front();
	}

	selection_undo_action->set_sensitive (false);
	selection_redo_action->set_sensitive (false);
	selection_op_history.push_front (&_selection_memento->get_state ());
}

void
Editor::begin_reversible_selection_op (string name)
{
	if (_session) {
		//cerr << name << endl;
		/* begin/commit pairs can be nested */
		selection_op_cmd_depth++;
	}
}

void
Editor::commit_reversible_selection_op ()
{
	if (_session) {
		if (selection_op_cmd_depth == 1) {

			if (selection_op_history_it > 0 && selection_op_history_it < selection_op_history.size()) {
				/* The user has undone some selection ops and then made a new one,
				 * making anything earlier in the list invalid.
				 */

				list<XMLNode *>::iterator it = selection_op_history.begin();
				list<XMLNode *>::iterator e_it = it;
				advance (e_it, selection_op_history_it);

				for (; it != e_it; ++it) {
					delete *it;
				}
				selection_op_history.erase (selection_op_history.begin(), e_it);
			}

			selection_op_history.push_front (&_selection_memento->get_state ());
			selection_op_history_it = 0;

			selection_undo_action->set_sensitive (true);
			selection_redo_action->set_sensitive (false);
		}

		if (selection_op_cmd_depth > 0) {
			selection_op_cmd_depth--;
		}
	}
}

void
Editor::undo_selection_op ()
{
	if (_session) {
		selection_op_history_it++;
		uint32_t n = 0;
		for (std::list<XMLNode *>::iterator i = selection_op_history.begin(); i != selection_op_history.end(); ++i) {
			if (n == selection_op_history_it) {
				_selection_memento->set_state (*(*i), Stateful::current_state_version);
				selection_redo_action->set_sensitive (true);
			}
			++n;
		}
		/* is there an earlier entry? */
		if ((selection_op_history_it + 1) >= selection_op_history.size()) {
			selection_undo_action->set_sensitive (false);
		}
	}
}

void
Editor::redo_selection_op ()
{
	if (_session) {
		if (selection_op_history_it > 0) {
			selection_op_history_it--;
		}
		uint32_t n = 0;
		for (std::list<XMLNode *>::iterator i = selection_op_history.begin(); i != selection_op_history.end(); ++i) {
			if (n == selection_op_history_it) {
				_selection_memento->set_state (*(*i), Stateful::current_state_version);
				selection_undo_action->set_sensitive (true);
			}
			++n;
		}

		if (selection_op_history_it == 0) {
			selection_redo_action->set_sensitive (false);
		}
	}
}

void
Editor::begin_reversible_command (string name)
{
	if (_session) {
		before.push_back (&_selection_memento->get_state ());
		_session->begin_reversible_command (name);
	}
}

void
Editor::begin_reversible_command (GQuark q)
{
	if (_session) {
		before.push_back (&_selection_memento->get_state ());
		_session->begin_reversible_command (q);
	}
}

void
Editor::abort_reversible_command ()
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
Editor::commit_reversible_command ()
{
	if (_session) {
		if (before.size() == 1) {
			_session->add_command (new MementoCommand<SelectionMemento>(*(_selection_memento), before.front(), &_selection_memento->get_state ()));
			redo_action->set_sensitive(false);
			undo_action->set_sensitive(true);
			begin_selection_op_history ();
		}

		if (before.empty()) {
			cerr << "Please call begin_reversible_command() before commit_reversible_command()." << endl;
		} else {
			before.pop_back();
		}

		_session->commit_reversible_command ();
	}
}

void
Editor::history_changed ()
{
	string label;

	if (undo_action && _session) {
		if (_session->undo_depth() == 0) {
			label = S_("Command|Undo");
		} else {
			label = string_compose(S_("Command|Undo (%1)"), _session->next_undo());
		}
		undo_action->property_label() = label;
	}

	if (redo_action && _session) {
		if (_session->redo_depth() == 0) {
			label = _("Redo");
			redo_action->set_sensitive (false);
		} else {
			label = string_compose(_("Redo (%1)"), _session->next_redo());
			redo_action->set_sensitive (true);
		}
		redo_action->property_label() = label;
	}
}

void
Editor::duplicate_range (bool with_dialog)
{
	float times = 1.0f;

	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (selection->time.length() == 0 && rs.empty()) {
		return;
	}

	if (with_dialog) {

		ArdourDialog win (_("Duplicate"));
		Label label (_("Number of duplications:"));
		Adjustment adjustment (1.0, 1.0, 1000000.0, 1.0, 5.0);
		SpinButton spinner (adjustment, 0.0, 1);
		HBox hbox;

		win.get_vbox()->set_spacing (12);
		win.get_vbox()->pack_start (hbox);
		hbox.set_border_width (6);
		hbox.pack_start (label, PACK_EXPAND_PADDING, 12);

		/* dialogs have ::add_action_widget() but that puts the spinner in the wrong
		   place, visually. so do this by hand.
		*/

		hbox.pack_start (spinner, PACK_EXPAND_PADDING, 12);
		spinner.signal_activate().connect (sigc::bind (sigc::mem_fun (win, &ArdourDialog::response), RESPONSE_ACCEPT));
		spinner.grab_focus();

		hbox.show ();
		label.show ();
		spinner.show ();

		win.add_button (Stock::CANCEL, RESPONSE_CANCEL);
		win.add_button (_("Duplicate"), RESPONSE_ACCEPT);
		win.set_default_response (RESPONSE_ACCEPT);

		spinner.grab_focus ();

		switch (win.run ()) {
		case RESPONSE_ACCEPT:
			break;
		default:
			return;
		}

		times = adjustment.get_value();
	}

	if ((current_mouse_mode() == MouseRange)) {
		if (!selection->time.length().is_zero()) {
			duplicate_selection (times);
		}
	} else if (get_smart_mode()) {
		if (!selection->time.length().is_zero()) {
			duplicate_selection (times);
		} else
			duplicate_some_regions (rs, times);
	} else {
		duplicate_some_regions (rs, times);
	}
}

void
Editor::set_edit_mode (EditMode m)
{
	Config->set_edit_mode (m);
}

void
Editor::cycle_edit_mode ()
{
	switch (Config->get_edit_mode()) {
	case Slide:
		Config->set_edit_mode (Ripple);
		break;
	case Ripple:
		Config->set_edit_mode (RippleAll);
		break;
	case RippleAll:
		Config->set_edit_mode (Lock);
		break;
	case Lock:
		Config->set_edit_mode (Slide);
		break;
	}
}

void
Editor::edit_mode_selection_done (EditMode m)
{
	Config->set_edit_mode (m);
}

void
Editor::grid_type_selection_done (GridType gridtype)
{
	RefPtr<RadioAction> ract = grid_type_action (gridtype);
	if (ract && ract->get_active()) {  /*radio-action is already set*/
		set_grid_to(gridtype);         /*so we must set internal state here*/
	} else {
		ract->set_active ();
	}
}

void
Editor::draw_length_selection_done (GridType gridtype)
{
	RefPtr<RadioAction> ract = draw_length_action (gridtype);
	if (ract && ract->get_active()) {  /*radio-action is already set*/
		set_draw_length_to(gridtype);  /*so we must set internal state here*/
	} else {
		ract->set_active ();
	}
}

void
Editor::draw_velocity_selection_done (int v)
{
	RefPtr<RadioAction> ract = draw_velocity_action (v);
	if (ract && ract->get_active()) {  /*radio-action is already set*/
		set_draw_velocity_to(v);       /*so we must set internal state here*/
	} else {
		ract->set_active ();
	}
}

void
Editor::draw_channel_selection_done (int c)
{
	RefPtr<RadioAction> ract = draw_channel_action (c);
	if (ract && ract->get_active()) {  /*radio-action is already set*/
		set_draw_channel_to(c);        /*so we must set internal state here*/
	} else {
		ract->set_active ();
	}
}

void
Editor::snap_mode_selection_done (SnapMode mode)
{
	RefPtr<RadioAction> ract = snap_mode_action (mode);

	if (ract) {
		ract->set_active (true);
	}
}

void
Editor::cycle_edit_point (bool with_marker)
{
	if(Profile->get_mixbus())
		with_marker = false;

	switch (_edit_point) {
	case EditAtMouse:
		set_edit_point_preference (EditAtPlayhead);
		break;
	case EditAtPlayhead:
		if (with_marker) {
			set_edit_point_preference (EditAtSelectedMarker);
		} else {
			set_edit_point_preference (EditAtMouse);
		}
		break;
	case EditAtSelectedMarker:
		set_edit_point_preference (EditAtMouse);
		break;
	}
}

void
Editor::edit_point_selection_done (EditPoint ep)
{
	set_edit_point_preference (ep);
}

void
Editor::build_zoom_focus_menu ()
{
	using namespace Menu_Helpers;

	zoom_focus_selector.AddMenuElem (MenuElem (zoom_focus_strings[(int)ZoomFocusLeft], sigc::bind (sigc::mem_fun(*this, &Editor::zoom_focus_selection_done), (ZoomFocus) ZoomFocusLeft)));
	zoom_focus_selector.AddMenuElem (MenuElem (zoom_focus_strings[(int)ZoomFocusRight], sigc::bind (sigc::mem_fun(*this, &Editor::zoom_focus_selection_done), (ZoomFocus) ZoomFocusRight)));
	zoom_focus_selector.AddMenuElem (MenuElem (zoom_focus_strings[(int)ZoomFocusCenter], sigc::bind (sigc::mem_fun(*this, &Editor::zoom_focus_selection_done), (ZoomFocus) ZoomFocusCenter)));
	zoom_focus_selector.AddMenuElem (MenuElem (zoom_focus_strings[(int)ZoomFocusPlayhead], sigc::bind (sigc::mem_fun(*this, &Editor::zoom_focus_selection_done), (ZoomFocus) ZoomFocusPlayhead)));
	zoom_focus_selector.AddMenuElem (MenuElem (zoom_focus_strings[(int)ZoomFocusMouse], sigc::bind (sigc::mem_fun(*this, &Editor::zoom_focus_selection_done), (ZoomFocus) ZoomFocusMouse)));
	zoom_focus_selector.AddMenuElem (MenuElem (zoom_focus_strings[(int)ZoomFocusEdit], sigc::bind (sigc::mem_fun(*this, &Editor::zoom_focus_selection_done), (ZoomFocus) ZoomFocusEdit)));

	set_size_request_to_display_given_text (zoom_focus_selector, zoom_focus_strings, COMBO_TRIANGLE_WIDTH, 2);
}

void
Editor::zoom_focus_selection_done (ZoomFocus f)
{
	RefPtr<RadioAction> ract = zoom_focus_action (f);
	if (ract) {
		ract->set_active ();
	}
}

void
Editor::build_track_count_menu ()
{
	using namespace Menu_Helpers;

	if (!Profile->get_mixbus()) {
		visible_tracks_selector.AddMenuElem (MenuElem (X_("1"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 1)));
		visible_tracks_selector.AddMenuElem (MenuElem (X_("2"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 2)));
		visible_tracks_selector.AddMenuElem (MenuElem (X_("3"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 3)));
		visible_tracks_selector.AddMenuElem (MenuElem (X_("4"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 4)));
		visible_tracks_selector.AddMenuElem (MenuElem (X_("8"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 8)));
		visible_tracks_selector.AddMenuElem (MenuElem (X_("12"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 12)));
		visible_tracks_selector.AddMenuElem (MenuElem (X_("16"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 16)));
		visible_tracks_selector.AddMenuElem (MenuElem (X_("20"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 20)));
		visible_tracks_selector.AddMenuElem (MenuElem (X_("24"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 24)));
		visible_tracks_selector.AddMenuElem (MenuElem (X_("32"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 32)));
		visible_tracks_selector.AddMenuElem (MenuElem (X_("64"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 64)));
		visible_tracks_selector.AddMenuElem (MenuElem (_("Selection"), sigc::mem_fun(*this, &Editor::fit_selection)));
		visible_tracks_selector.AddMenuElem (MenuElem (_("All"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 0)));
	} else {
		visible_tracks_selector.AddMenuElem (MenuElem (_("Fit 1 track"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 1)));
		visible_tracks_selector.AddMenuElem (MenuElem (_("Fit 2 tracks"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 2)));
		visible_tracks_selector.AddMenuElem (MenuElem (_("Fit 4 tracks"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 4)));
		visible_tracks_selector.AddMenuElem (MenuElem (_("Fit 8 tracks"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 8)));
		visible_tracks_selector.AddMenuElem (MenuElem (_("Fit 16 tracks"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 16)));
		visible_tracks_selector.AddMenuElem (MenuElem (_("Fit 24 tracks"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 24)));
		visible_tracks_selector.AddMenuElem (MenuElem (_("Fit 32 tracks"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 32)));
		visible_tracks_selector.AddMenuElem (MenuElem (_("Fit 48 tracks"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 48)));
		visible_tracks_selector.AddMenuElem (MenuElem (_("Fit All tracks"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 0)));
		visible_tracks_selector.AddMenuElem (MenuElem (_("Fit Selection"), sigc::mem_fun(*this, &Editor::fit_selection)));

		zoom_preset_selector.AddMenuElem (MenuElem (_("Zoom to 10 ms"), sigc::bind (sigc::mem_fun(*this, &Editor::set_zoom_preset), 10)));
		zoom_preset_selector.AddMenuElem (MenuElem (_("Zoom to 100 ms"), sigc::bind (sigc::mem_fun(*this, &Editor::set_zoom_preset), 100)));
		zoom_preset_selector.AddMenuElem (MenuElem (_("Zoom to 1 sec"), sigc::bind (sigc::mem_fun(*this, &Editor::set_zoom_preset), 1 * 1000)));
		zoom_preset_selector.AddMenuElem (MenuElem (_("Zoom to 10 sec"), sigc::bind (sigc::mem_fun(*this, &Editor::set_zoom_preset), 10 * 1000)));
		zoom_preset_selector.AddMenuElem (MenuElem (_("Zoom to 1 min"), sigc::bind (sigc::mem_fun(*this, &Editor::set_zoom_preset), 60 * 1000)));
		zoom_preset_selector.AddMenuElem (MenuElem (_("Zoom to 10 min"), sigc::bind (sigc::mem_fun(*this, &Editor::set_zoom_preset), 10 * 60 * 1000)));
		zoom_preset_selector.AddMenuElem (MenuElem (_("Zoom to 1 hour"), sigc::bind (sigc::mem_fun(*this, &Editor::set_zoom_preset), 60 * 60 * 1000)));
		zoom_preset_selector.AddMenuElem (MenuElem (_("Zoom to 8 hours"), sigc::bind (sigc::mem_fun(*this, &Editor::set_zoom_preset), 8 * 60 * 60 * 1000)));
		zoom_preset_selector.AddMenuElem (MenuElem (_("Zoom to 24 hours"), sigc::bind (sigc::mem_fun(*this, &Editor::set_zoom_preset), 24 * 60 * 60 * 1000)));
		zoom_preset_selector.AddMenuElem (MenuElem (_("Zoom to Session"), sigc::mem_fun(*this, &Editor::temporal_zoom_session)));
		zoom_preset_selector.AddMenuElem (MenuElem (_("Zoom to Extents"), sigc::mem_fun(*this, &Editor::temporal_zoom_extents)));
		zoom_preset_selector.AddMenuElem (MenuElem (_("Zoom to Range/Region Selection"), sigc::bind (sigc::mem_fun(*this, &Editor::temporal_zoom_selection), Horizontal)));
	}
}

void
Editor::set_zoom_preset (int64_t ms)
{
	if (ms <= 0) {
		temporal_zoom_session();
		return;
	}

	ARDOUR::samplecnt_t const sample_rate = ARDOUR::AudioEngine::instance()->sample_rate();
	temporal_zoom ((sample_rate * ms / 1000) / _visible_canvas_width);
}

void
Editor::set_visible_track_count (int32_t n)
{
	_visible_track_count = n;

	/* if the canvas hasn't really been allocated any size yet, just
	   record the desired number of visible tracks and return. when canvas
	   allocation happens, we will get called again and then we can do the
	   real work.
	*/

	if (_visible_canvas_height <= 1) {
		return;
	}

	int h;
	string str;
	DisplaySuspender ds;

	if (_visible_track_count > 0) {
		h = trackviews_height() / _visible_track_count;
		std::ostringstream s;
		s << _visible_track_count;
		str = s.str();
	} else if (_visible_track_count == 0) {
		uint32_t n = 0;
		for (TrackViewList::const_iterator i = track_views.begin(); i != track_views.end(); ++i) {
			if ((*i)->marked_for_display()) {
				++n;
				TimeAxisView::Children cl ((*i)->get_child_list ());
				for (TimeAxisView::Children::const_iterator j = cl.begin(); j != cl.end(); ++j) {
					if ((*j)->marked_for_display()) {
						++n;
					}
				}
			}
		}
		if (n == 0) {
			visible_tracks_selector.set_text (X_("*"));
			return;
		}
		h = trackviews_height() / n;
		str = _("All");
	} else {
		/* negative value means that the visible track count has
		   been overridden by explicit track height changes.
		*/
		visible_tracks_selector.set_text (X_("*"));
		return;
	}

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		(*i)->set_height (h, TimeAxisView::HeightPerLane);
	}

	if (str != visible_tracks_selector.get_text()) {
		visible_tracks_selector.set_text (str);
	}
}

void
Editor::override_visible_track_count ()
{
	_visible_track_count = -1;
	visible_tracks_selector.set_text (_("*"));
}

bool
Editor::edit_controls_button_event (GdkEventButton* ev)
{
	if ((ev->type == GDK_2BUTTON_PRESS && ev->button == 1) || (ev->type == GDK_BUTTON_RELEASE && Keyboard::is_context_menu_event (ev))) {
		ARDOUR_UI::instance()->add_route ();
	} else if (ev->button == 1 && ev->type == GDK_BUTTON_PRESS) {
		selection->clear_tracks ();
	}
	return true;
}

bool
Editor::mouse_select_button_release (GdkEventButton* ev)
{
	/* this handles just right-clicks */

	if (ev->button != 3) {
		return false;
	}

	return true;
}

void
Editor::set_zoom_focus (ZoomFocus f)
{
	string str = zoom_focus_strings[(int)f];

	if (str != zoom_focus_selector.get_text()) {
		zoom_focus_selector.set_text (str);
	}

	if (zoom_focus != f) {
		zoom_focus = f;
		instant_save ();
	}
}

void
Editor::cycle_zoom_focus ()
{
	switch (zoom_focus) {
	case ZoomFocusLeft:
		set_zoom_focus (ZoomFocusRight);
		break;
	case ZoomFocusRight:
		set_zoom_focus (ZoomFocusCenter);
		break;
	case ZoomFocusCenter:
		set_zoom_focus (ZoomFocusPlayhead);
		break;
	case ZoomFocusPlayhead:
		set_zoom_focus (ZoomFocusMouse);
		break;
	case ZoomFocusMouse:
		set_zoom_focus (ZoomFocusEdit);
		break;
	case ZoomFocusEdit:
		set_zoom_focus (ZoomFocusLeft);
		break;
	}
}

void
Editor::update_grid ()
{
	if (!_session) {
		return;
	}

	if (_grid_type == GridTypeNone) {
		hide_grid_lines ();
	} else if (grid_musical()) {
		Temporal::TempoMapPoints grid;
		if (bbt_ruler_scale != bbt_show_many) {
			compute_current_bbt_points (grid, _leftmost_sample, _leftmost_sample + current_page_samples());
		}
		maybe_draw_grid_lines ();
	} else {
		maybe_draw_grid_lines ();
	}
}

void
Editor::toggle_follow_playhead ()
{
	RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Editor"), X_("toggle-follow-playhead"));
	set_follow_playhead (tact->get_active());
}

/** @param yn true to follow playhead, otherwise false.
 *  @param catch_up true to reset the editor view to show the playhead (if yn == true), otherwise false.
 */
void
Editor::set_follow_playhead (bool yn, bool catch_up)
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
Editor::toggle_stationary_playhead ()
{
	RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Editor"), X_("toggle-stationary-playhead"));
	set_stationary_playhead (tact->get_active());
}

void
Editor::set_stationary_playhead (bool yn)
{
	if (_stationary_playhead != yn) {
		if ((_stationary_playhead = yn) == true) {
			/* catch up -- FIXME need a 3.0 equivalent of this 2.X call */
			// update_current_screen ();
		}
		instant_save ();
	}
}

bool
Editor::show_touched_automation () const
{
	if (!contents().is_mapped()) {
		return false;
	}
	return _show_touched_automation;
}

void
Editor::toggle_show_touched_automation ()
{
	RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Editor"), X_("show-touched-automation"));
	set_show_touched_automation (tact->get_active());
}

void
Editor::set_show_touched_automation (bool yn)
{
	if (_show_touched_automation == yn) {
		return;
	}
	_show_touched_automation = yn;
	if (!yn) {
		RouteTimeAxisView::signal_ctrl_touched (true);
	}
	instant_save ();
}

Temporal::timecnt_t
Editor::get_paste_offset (Temporal::timepos_t const & pos, unsigned paste_count, Temporal::timecnt_t const & duration)
{
	if (paste_count == 0) {
		/* don't bother calculating an offset that will be zero anyway */
		return timecnt_t (0, timepos_t());
	}

	/* calculate basic unsnapped multi-paste offset */
	Temporal::timecnt_t offset = duration * paste_count;

	/* snap offset so pos + offset is aligned to the grid */
	Temporal::timepos_t snap_pos (pos + offset);
	snap_to (snap_pos, Temporal::RoundUpMaybe);

	return pos.distance (snap_pos);
}

unsigned
Editor::get_grid_beat_divisions (GridType gt)
{
	switch (gt) {
	case GridTypeBeatDiv32:  return 32;
	case GridTypeBeatDiv28:  return 28;
	case GridTypeBeatDiv24:  return 24;
	case GridTypeBeatDiv20:  return 20;
	case GridTypeBeatDiv16:  return 16;
	case GridTypeBeatDiv14:  return 14;
	case GridTypeBeatDiv12:  return 12;
	case GridTypeBeatDiv10:  return 10;
	case GridTypeBeatDiv8:   return 8;
	case GridTypeBeatDiv7:   return 7;
	case GridTypeBeatDiv6:   return 6;
	case GridTypeBeatDiv5:   return 5;
	case GridTypeBeatDiv4:   return 4;
	case GridTypeBeatDiv3:   return 3;
	case GridTypeBeatDiv2:   return 2;
	case GridTypeBeat:       return 1;
	case GridTypeBar:        return 1;

	case GridTypeNone:       return 0;
	case GridTypeTimecode:   return 0;
	case GridTypeMinSec:     return 0;
	case GridTypeCDFrame:    return 0;
	default:                 return 0;
	}
	return 0;
}

/** returns the current musical grid divisiions using the supplied modifier mask from a GtkEvent.
    if the grid is non-musical, returns 0.
    if the grid is snapped to bars, returns -1.
    @param event_state the current keyboard modifier mask.
*/
int32_t
Editor::get_grid_music_divisions (Editing::GridType gt, uint32_t event_state)
{
	if (snap_mode() == SnapOff && !ArdourKeyboard::indicates_snap (event_state)) {
		return 0;
	}

	if (snap_mode() != SnapOff && ArdourKeyboard::indicates_snap (event_state)) {
		return 0;
	}

	switch (gt) {
	case GridTypeBeatDiv32:  return 32;
	case GridTypeBeatDiv28:  return 28;
	case GridTypeBeatDiv24:  return 24;
	case GridTypeBeatDiv20:  return 20;
	case GridTypeBeatDiv16:  return 16;
	case GridTypeBeatDiv14:  return 14;
	case GridTypeBeatDiv12:  return 12;
	case GridTypeBeatDiv10:  return 10;
	case GridTypeBeatDiv8:   return 8;
	case GridTypeBeatDiv7:   return 7;
	case GridTypeBeatDiv6:   return 6;
	case GridTypeBeatDiv5:   return 5;
	case GridTypeBeatDiv4:   return 4;
	case GridTypeBeatDiv3:   return 3;
	case GridTypeBeatDiv2:   return 2;
	case GridTypeBeat:       return 1;
	case GridTypeBar :       return -1;

	case GridTypeNone:       return 0;
	case GridTypeTimecode:   return 0;
	case GridTypeMinSec:     return 0;
	case GridTypeCDFrame:    return 0;
	}
	return 0;
}

Temporal::Beats
Editor::get_grid_type_as_beats (bool& success, timepos_t const & position)
{
	success = true;

	const unsigned divisions = get_grid_beat_divisions (_grid_type);
	if (divisions) {
		return Temporal::Beats::from_double (1.0 / (double) divisions);
	}

	TempoMap::SharedPtr tmap (TempoMap::use());

	switch (_grid_type) {
	case GridTypeBeat:
		return Temporal::Beats::from_double (4.0 / tmap->meter_at (position).note_value());
	case GridTypeBar:
		if (_session) {
			const Meter& m = tmap->meter_at (position);
			return Temporal::Beats::from_double ((4.0 * m.divisions_per_bar()) / m.note_value());
		}
		break;
	default:
#warning NUTEMPO need to implement all other subdivs
		success = false;
		break;
	}

	return Temporal::Beats();
}

Temporal::Beats
Editor::get_draw_length_as_beats (bool& success, timepos_t const & position)
{
	success = true;

	GridType grid_to_use = draw_length() == DRAW_LEN_AUTO ? grid_type() : draw_length();
	const unsigned divisions = get_grid_beat_divisions (grid_to_use);
	if (divisions) {
		return Temporal::Beats::from_double (1.0 / (double) divisions);
	}

	success = false;
	return Temporal::Beats();
}

timecnt_t
Editor::get_nudge_distance (timepos_t const & pos, timecnt_t& next)
{
	timecnt_t ret;

	ret = nudge_clock->current_duration (pos);
	next = ret + timepos_t::smallest_step (pos.time_domain()); /* FIX ME ... not sure this is how to compute "next" */

	return ret;
}

int
Editor::playlist_deletion_dialog (boost::shared_ptr<Playlist> pl)
{
	ArdourDialog dialog (_("Playlist Deletion"));
	Label  label (string_compose (_("Playlist %1 is currently unused.\n"
					"If it is kept, its audio files will not be cleaned.\n"
					"If it is deleted, audio files used by it alone will be cleaned."),
				      pl->name()));

	dialog.set_position (WIN_POS_CENTER);
	dialog.get_vbox()->pack_start (label);

	label.show ();

	dialog.add_button (_("Delete All Unused"), RESPONSE_YES); // needs clarification. this and all remaining ones
	dialog.add_button (_("Delete Playlist"), RESPONSE_ACCEPT);
	Button* keep = dialog.add_button (_("Keep Playlist"), RESPONSE_REJECT);
	dialog.add_button (_("Keep Remaining"), RESPONSE_NO); // ditto
	dialog.add_button (_("Cancel"), RESPONSE_CANCEL);

	/* by default gtk uses the left most button */
	keep->grab_focus ();

	switch (dialog.run ()) {
	case RESPONSE_NO:
		/* keep this and all remaining ones */
		return -2;
		break;

	case RESPONSE_YES:
		/* delete this and all others */
		return 2;
		break;

	case RESPONSE_ACCEPT:
		/* delete the playlist */
		return 1;
		break;

	case RESPONSE_REJECT:
		/* keep the playlist */
		return 0;
		break;

	default:
		break;
	}

	return -1;
}

int
Editor::plugin_setup (boost::shared_ptr<Route> r, boost::shared_ptr<PluginInsert> pi, ARDOUR::Route::PluginSetupOptions flags)
{
	PluginSetupDialog psd (r, pi, flags);
	int rv = psd.run ();
	return rv + (psd.fan_out() ? 4 : 0);
}

bool
Editor::audio_region_selection_covers (samplepos_t where)
{
	for (RegionSelection::iterator a = selection->regions.begin(); a != selection->regions.end(); ++a) {
		if ((*a)->region()->covers (where)) {
			return true;
		}
	}

	return false;
}

void
Editor::cleanup_regions ()
{
	_regions->remove_unused_regions();
}


void
Editor::prepare_for_cleanup ()
{
	cut_buffer->clear_regions ();
	cut_buffer->clear_playlists ();

	selection->clear_regions ();
	selection->clear_playlists ();

	_regions->suspend_redisplay ();
}

void
Editor::finish_cleanup ()
{
	_regions->resume_redisplay ();
}

Location*
Editor::transport_loop_location()
{
	if (_session) {
		return _session->locations()->auto_loop_location();
	} else {
		return 0;
	}
}

Location*
Editor::transport_punch_location()
{
	if (_session) {
		return _session->locations()->auto_punch_location();
	} else {
		return 0;
	}
}

bool
Editor::control_layout_scroll (GdkEventScroll* ev)
{
	/* Just forward to the normal canvas scroll method. The coordinate
	   systems are different but since the canvas is always larger than the
	   track headers, and aligned with the trackview area, this will work.

	   In the not too distant future this layout is going away anyway and
	   headers will be on the canvas.
	*/
	return canvas_scroll_event (ev, false);
}

void
Editor::session_state_saved (string)
{
	update_title ();
	_snapshots->redisplay ();
}

void
Editor::maximise_editing_space ()
{
	if (_maximised) {
		return;
	}

	Gtk::Window* toplevel = current_toplevel();

	if (toplevel) {
		toplevel->fullscreen ();
		_maximised = true;
	}
}

void
Editor::restore_editing_space ()
{
	if (!_maximised) {
		return;
	}

	Gtk::Window* toplevel = current_toplevel();

	if (toplevel) {
		toplevel->unfullscreen();
		_maximised = false;
	}
}

bool
Editor::stamp_new_playlist (string title, string &name, string &pgroup, bool copy)
{
	pgroup = Playlist::generate_pgroup_id ();

	if (name.length()==0) {
		name = _("Take.1");
		if (_session->playlists()->by_name (name)) {
			name = Playlist::bump_name (name, *_session);
		}
	}

	Prompter prompter (true);
	prompter.set_title (title);
	prompter.set_prompt (_("Name for new playlist:"));
	prompter.set_initial_text (name);
	prompter.add_button (Gtk::Stock::NEW, Gtk::RESPONSE_ACCEPT);
	prompter.set_response_sensitive (Gtk::RESPONSE_ACCEPT, true);
	prompter.show_all ();

	while (true) {
		if (prompter.run () != Gtk::RESPONSE_ACCEPT) {
			return false;
		}
		prompter.get_result (name);
		if (name.length()) {
			if (_session->playlists()->by_name (name)) {
				prompter.set_prompt (_("That name is already in use.  Use this instead?"));
				prompter.set_initial_text (Playlist::bump_name (name, *_session));
			} else {
				break;
			}
		}
	}

	return true;
}

void
Editor::mapped_clear_playlist (RouteUI& rui)
{
	rui.clear_playlist ();
}

/** Clear the current playlist for a given track and also any others that belong
 *  to the same active route group with the `select' property.
 *  @param v Track.
 */

void
Editor::clear_grouped_playlists (RouteUI* rui)
{
	begin_reversible_command (_("clear playlists"));
	vector<boost::shared_ptr<ARDOUR::Playlist> > playlists;
	_session->playlists()->get (playlists);
	mapover_grouped_routes (sigc::mem_fun (*this, &Editor::mapped_clear_playlist), rui, ARDOUR::Properties::group_select.property_id);
	commit_reversible_command ();
}

void
Editor::mapped_select_playlist_matching (RouteUI& rui, boost::weak_ptr<ARDOUR::Playlist> pl)
{
	rui.select_playlist_matching (pl);
}

void
Editor::mapped_use_new_playlist (RouteUI& rui, std::string name, string gid, bool copy, vector<boost::shared_ptr<ARDOUR::Playlist> > const & playlists)
{
	rui.use_new_playlist (name, gid, playlists, copy);
}

void
Editor::new_playlists_for_all_tracks (bool copy)
{
	string name, gid;
	if (stamp_new_playlist(  copy ?  _("Copy Playlist for ALL Tracks") : _("New Playlist for ALL Tracks"), name,gid,copy)) {
		vector<boost::shared_ptr<ARDOUR::Playlist> > playlists;
		_session->playlists()->get (playlists);
		mapover_all_routes (sigc::bind (sigc::mem_fun (*this, &Editor::mapped_use_new_playlist), name, gid, copy, playlists));
	}
}

void
Editor::new_playlists_for_grouped_tracks (RouteUI* rui, bool copy)
{
	string name, gid;
	if (stamp_new_playlist(  copy ?  _("Copy Playlist for this track/group") : _("New Playlist for this track/group"), name,gid,copy)) {
		vector<boost::shared_ptr<ARDOUR::Playlist> > playlists;
		_session->playlists()->get (playlists);
		mapover_grouped_routes (sigc::bind (sigc::mem_fun (*this, &Editor::mapped_use_new_playlist), name, gid, copy, playlists), rui, ARDOUR::Properties::group_select.property_id);
	}
}

void
Editor::new_playlists_for_selected_tracks (bool copy)
{
	string name, gid;
	if (stamp_new_playlist(  copy ?  _("Copy Playlist for Selected Tracks") : _("New Playlist for Selected Tracks"), name,gid,copy)) {
		vector<boost::shared_ptr<ARDOUR::Playlist> > playlists;
		_session->playlists()->get (playlists);
		mapover_selected_routes (sigc::bind (sigc::mem_fun (*this, &Editor::mapped_use_new_playlist), name, gid, copy, playlists));
	}
}

void
Editor::new_playlists_for_armed_tracks (bool copy)
{
	string name, gid;
	if (stamp_new_playlist( copy ?  _("Copy Playlist for Armed Tracks") : _("New Playlist for Armed Tracks"), name,gid,copy)) {
		vector<boost::shared_ptr<ARDOUR::Playlist> > playlists;
		_session->playlists()->get (playlists);
		mapover_armed_routes (sigc::bind (sigc::mem_fun (*this, &Editor::mapped_use_new_playlist), name, gid, copy, playlists));
	}
}

double
Editor::get_y_origin () const
{
	return vertical_adjustment.get_value ();
}

/** Queue up a change to the viewport x origin.
 *  @param sample New x origin.
 */
void
Editor::reset_x_origin (samplepos_t sample)
{
	pending_visual_change.add (VisualChange::TimeOrigin);
	pending_visual_change.time_origin = sample;
	ensure_visual_change_idle_handler ();
}

void
Editor::reset_y_origin (double y)
{
	pending_visual_change.add (VisualChange::YOrigin);
	pending_visual_change.y_origin = y;
	ensure_visual_change_idle_handler ();
}

void
Editor::reset_zoom (samplecnt_t spp)
{
	if (spp == samples_per_pixel) {
		return;
	}

	pending_visual_change.add (VisualChange::ZoomLevel);
	pending_visual_change.samples_per_pixel = spp;
	ensure_visual_change_idle_handler ();
}

void
Editor::reposition_and_zoom (samplepos_t sample, double fpu)
{
	reset_x_origin (sample);
	reset_zoom (fpu);

	if (!no_save_visual) {
		undo_visual_stack.push_back (current_visual_state(false));
	}
}

Editor::VisualState::VisualState (bool with_tracks)
	: gui_state (with_tracks ? new GUIObjectState : 0)
{
}

Editor::VisualState::~VisualState ()
{
	delete gui_state;
}

Editor::VisualState*
Editor::current_visual_state (bool with_tracks)
{
	VisualState* vs = new VisualState (with_tracks);
	vs->y_position = vertical_adjustment.get_value();
	vs->samples_per_pixel = samples_per_pixel;
	vs->_leftmost_sample = _leftmost_sample;
	vs->zoom_focus = zoom_focus;

	if (with_tracks) {
		vs->gui_state->set_state (ARDOUR_UI::instance()->gui_object_state->get_state());
	}

	return vs;
}

void
Editor::undo_visual_state ()
{
	if (undo_visual_stack.empty()) {
		return;
	}

	VisualState* vs = undo_visual_stack.back();
	undo_visual_stack.pop_back();


	redo_visual_stack.push_back (current_visual_state (vs ? vs->gui_state != 0 : false));

	if (vs) {
		use_visual_state (*vs);
	}
}

void
Editor::redo_visual_state ()
{
	if (redo_visual_stack.empty()) {
		return;
	}

	VisualState* vs = redo_visual_stack.back();
	redo_visual_stack.pop_back();

	/* XXX: can 'vs' really be 0? Is there a place that puts NULL pointers onto the stack? */
	undo_visual_stack.push_back (current_visual_state (vs ? (vs->gui_state != 0) : false));

	if (vs) {
		use_visual_state (*vs);
	}
}

void
Editor::swap_visual_state ()
{
	if (undo_visual_stack.empty()) {
		redo_visual_state ();
	} else {
		undo_visual_state ();
	}
}

void
Editor::use_visual_state (VisualState& vs)
{
	PBD::Unwinder<bool> nsv (no_save_visual, true);
	DisplaySuspender ds;

	vertical_adjustment.set_value (vs.y_position);

	set_zoom_focus (vs.zoom_focus);
	reposition_and_zoom (vs._leftmost_sample, vs.samples_per_pixel);

	if (vs.gui_state) {
		ARDOUR_UI::instance()->gui_object_state->set_state (vs.gui_state->get_state());

		for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
			(*i)->clear_property_cache();
			(*i)->reset_visual_state ();
		}
	}

	// TODO push state to PresentationInfo, force update ?
}

/** This is the core function that controls the zoom level of the canvas. It is called
 *  whenever one or more calls are made to reset_zoom().  It executes in an idle handler.
 *  @param spp new number of samples per pixel
 */
void
Editor::set_samples_per_pixel (samplecnt_t spp)
{
	if (spp < 1) {
		return;
	}

	const samplecnt_t three_days = 3 * 24 * 60 * 60 * (_session ? _session->sample_rate() : 48000);
	const samplecnt_t lots_of_pixels = 4000;

	/* if the zoom level is greater than what you'd get trying to display 3
	 * days of audio on a really big screen, then it's too big.
	 */

	if (spp * lots_of_pixels > three_days) {
		return;
	}

	samples_per_pixel = spp;
}

void
Editor::on_samples_per_pixel_changed ()
{
	bool const showing_time_selection = selection->time.length() > 0;

	if (showing_time_selection && selection->time.start_sample () != selection->time.end_sample ()) {
		for (TrackViewList::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
			(*i)->reshow_selection (selection->time);
		}
	}

	ZoomChanged (); /* EMIT_SIGNAL */

	ArdourCanvas::GtkCanvasViewport* c;

	c = get_track_canvas();
	if (c) {
		c->canvas()->zoomed ();
	}

	if (_playhead_cursor) {
		_playhead_cursor->set_position (_playhead_cursor->current_sample ());
	}

	refresh_location_display();
	_summary->set_overlays_dirty ();

	update_marker_labels ();

	instant_save ();
}

samplepos_t
Editor::playhead_cursor_sample () const
{
	return _playhead_cursor->current_sample();
}

void
Editor::queue_visual_videotimeline_update ()
{
	pending_visual_change.add (VisualChange::VideoTimeline);
	ensure_visual_change_idle_handler ();
}

void
Editor::ensure_visual_change_idle_handler ()
{
	if (pending_visual_change.idle_handler_id < 0) {
		/* see comment in add_to_idle_resize above. */
		pending_visual_change.idle_handler_id = g_idle_add_full (G_PRIORITY_HIGH_IDLE + 10, _idle_visual_changer, this, NULL);
		pending_visual_change.being_handled = false;
	}
}

int
Editor::_idle_visual_changer (void* arg)
{
	return static_cast<Editor*>(arg)->idle_visual_changer ();
}

void
Editor::pre_render ()
{
	visual_change_queued = false;

	if (pending_visual_change.pending != 0) {
		ensure_visual_change_idle_handler();
	}
}

int
Editor::idle_visual_changer ()
{
	pending_visual_change.idle_handler_id = -1;

	if (pending_visual_change.pending == 0) {
		return 0;
	}

	/* set_horizontal_position() below (and maybe other calls) call
	   gtk_main_iteration(), so it's possible that a signal will be handled
	   half-way through this method.  If this signal wants an
	   idle_visual_changer we must schedule another one after this one, so
	   mark the idle_handler_id as -1 here to allow that.  Also make a note
	   that we are doing the visual change, so that changes in response to
	   super-rapid-screen-update can be dropped if we are still processing
	   the last one.
	*/

	if (visual_change_queued) {
		return 0;
	}

	pending_visual_change.being_handled = true;

	VisualChange vc = pending_visual_change;

	pending_visual_change.pending = (VisualChange::Type) 0;

	visual_changer (vc);

	pending_visual_change.being_handled = false;

	visual_change_queued = true;

	return 0; /* this is always a one-shot call */
}

void
Editor::visual_changer (const VisualChange& vc)
{
	/**
	 * Changed first so the correct horizontal canvas position is calculated in
	 * Editor::set_horizontal_position
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

		compute_fixed_ruler_scale ();

		compute_bbt_ruler_scale (vc.time_origin, pending_visual_change.time_origin + current_page_samples());
		update_tempo_based_rulers ();
	}

	if (!(vc.pending & VisualChange::ZoomLevel)) {
		/* If the canvas is not being zoomed then the canvas items will not change
		 * and cause Item::prepare_for_render to be called so do it here manually.
		 * Not ideal, but I can't think of a better solution atm.
		 */
		_track_canvas->prepare_for_render();
	}

	/* If we are only scrolling vertically there is no need to update these */
	if (vc.pending != VisualChange::YOrigin) {
		update_fixed_rulers ();
		redisplay_grid (true);

		/* video frames & position need to be updated for zoom, horiz-scroll
		 * and (explicitly) VisualChange::VideoTimeline.
		 */
		update_video_timeline();
	}

	_region_peak_cursor->hide ();
	_summary->set_overlays_dirty ();
}

struct EditorOrderTimeAxisSorter {
    bool operator() (const TimeAxisView* a, const TimeAxisView* b) const {
	    return a->order () < b->order ();
    }
};

void
Editor::sort_track_selection (TrackViewList& sel)
{
	EditorOrderTimeAxisSorter cmp;
	sel.sort (cmp);
}

timepos_t
Editor::get_preferred_edit_position (EditIgnoreOption ignore, bool from_context_menu, bool from_outside_canvas)
{
	bool ignored;
	timepos_t where;
	EditPoint ep = _edit_point;

	if (Profile->get_mixbus()) {
		if (ep == EditAtSelectedMarker) {
			ep = EditAtPlayhead;
		}
	}

	if (from_outside_canvas && (ep == EditAtMouse)) {
		ep = EditAtPlayhead;
	} else if (from_context_menu && (ep == EditAtMouse)) {
		return timepos_t (canvas_event_sample (&context_click_event, 0, 0));
	}

	if (entered_marker) {
		DEBUG_TRACE (DEBUG::CutNPaste, string_compose ("GPEP: use entered marker @ %1\n", entered_marker->position()));
		return entered_marker->position();
	}

	if ((ignore == EDIT_IGNORE_PHEAD) && ep == EditAtPlayhead) {
		ep = EditAtSelectedMarker;
	}

	if ((ignore == EDIT_IGNORE_MOUSE) && ep == EditAtMouse) {
		ep = EditAtPlayhead;
	}

	samplepos_t ms;

	switch (ep) {
	case EditAtPlayhead:
		if (_dragging_playhead) {
			/* NOTE: since the user is dragging with the mouse, this operation will implicitly be Snapped */
			where = timepos_t (_playhead_cursor->current_sample());
		} else {
			where = timepos_t (_session->audible_sample());
		}
		DEBUG_TRACE (DEBUG::CutNPaste, string_compose ("GPEP: use playhead @ %1\n", where));
		break;

	case EditAtSelectedMarker:
		if (!selection->markers.empty()) {
			bool is_start;
			Location* loc = find_location_from_marker (selection->markers.front(), is_start);
			if (loc) {
				if (is_start) {
					where =  loc->start();
				} else {
					where = loc->end();
				}
				DEBUG_TRACE (DEBUG::CutNPaste, string_compose ("GPEP: use selected marker @ %1\n", where));
				break;
			}
		}
		/* fallthrough */

	default:
	case EditAtMouse:
		if (!mouse_sample (ms, ignored)) {
			/* XXX not right but what can we do ? */
			return timepos_t ();
		}
		where = timepos_t (ms);
		snap_to (where);
		DEBUG_TRACE (DEBUG::CutNPaste, string_compose ("GPEP: use mouse @ %1\n", where));
		break;
	}

	return where;
}

void
Editor::set_loop_range (timepos_t const & start, timepos_t const & end, string cmd)
{
	if (!_session) return;

	begin_reversible_command (cmd);

	Location* tll;

	if ((tll = transport_loop_location()) == 0) {
		Location* loc = new Location (*_session, start, end, _("Loop"),  Location::IsAutoLoop);
		XMLNode &before = _session->locations()->get_state();
		_session->locations()->add (loc, true);
		_session->set_auto_loop_location (loc);
		XMLNode &after = _session->locations()->get_state();
		_session->add_command (new MementoCommand<Locations>(*(_session->locations()), &before, &after));
	} else {
		XMLNode &before = tll->get_state();
		tll->set_hidden (false, this);
		tll->set (start, end);
		XMLNode &after = tll->get_state();
		_session->add_command (new MementoCommand<Location>(*tll, &before, &after));
	}

	commit_reversible_command ();
}

void
Editor::set_punch_range (timepos_t const & start, timepos_t const & end, string cmd)
{
	if (!_session) return;

	begin_reversible_command (cmd);

	Location* tpl;

	if ((tpl = transport_punch_location()) == 0) {
		Location* loc = new Location (*_session, start, end, _("Punch"),  Location::IsAutoPunch);
		XMLNode &before = _session->locations()->get_state();
		_session->locations()->add (loc, true);
		_session->set_auto_punch_location (loc);
		XMLNode &after = _session->locations()->get_state();
		_session->add_command (new MementoCommand<Locations>(*(_session->locations()), &before, &after));
	} else {
		XMLNode &before = tpl->get_state();
		tpl->set_hidden (false, this);
		tpl->set (start, end);
		XMLNode &after = tpl->get_state();
		_session->add_command (new MementoCommand<Location>(*tpl, &before, &after));
	}

	commit_reversible_command ();
}

/** Find regions which exist at a given time, and optionally on a given list of tracks.
 *  @param rs List to which found regions are added.
 *  @param where Time to look at.
 *  @param ts Tracks to look on; if this is empty, all tracks are examined.
 */
void
Editor::get_regions_at (RegionSelection& rs, timepos_t const & where, const TrackViewList& ts) const
{
	const TrackViewList* tracks;

	if (ts.empty()) {
		tracks = &track_views;
	} else {
		tracks = &ts;
	}

	for (TrackViewList::const_iterator t = tracks->begin(); t != tracks->end(); ++t) {

		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*>(*t);

		if (rtv) {
			boost::shared_ptr<Track> tr;
			boost::shared_ptr<Playlist> pl;

			if ((tr = rtv->track()) && ((pl = tr->playlist()))) {

				boost::shared_ptr<RegionList> regions = pl->regions_at (where);

				for (RegionList::iterator i = regions->begin(); i != regions->end(); ++i) {
					RegionView* rv = rtv->view()->find_view (*i);
					if (rv) {
						rs.add (rv);
					}
				}
			}
		}
	}
}

void
Editor::get_regions_after (RegionSelection& rs, timepos_t const & where, const TrackViewList& ts) const
{
	const TrackViewList* tracks;

	if (ts.empty()) {
		tracks = &track_views;
	} else {
		tracks = &ts;
	}

	for (TrackViewList::const_iterator t = tracks->begin(); t != tracks->end(); ++t) {
		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*>(*t);
		if (rtv) {
			boost::shared_ptr<Track> tr;
			boost::shared_ptr<Playlist> pl;

			if ((tr = rtv->track()) && ((pl = tr->playlist()))) {

				boost::shared_ptr<RegionList> regions = pl->regions_touched (where, timepos_t::max (where.time_domain()));

				for (RegionList::iterator i = regions->begin(); i != regions->end(); ++i) {

					RegionView* rv = rtv->view()->find_view (*i);

					if (rv) {
						rs.add (rv);
					}
				}
			}
		}
	}
}

/** Get regions using the following method:
 *
 *  Make a region list using:
 *   (a) any selected regions
 *   (b) the intersection of any selected tracks and the edit point(*)
 *   (c) if neither exists, and edit_point == mouse, then whatever region is under the mouse
 *
 *  (*) NOTE: in this case, if 'No Selection = All Tracks' is active, search all tracks
 *
 *  Note that we have forced the rule that selected regions and selected tracks are mutually exclusive
 */

RegionSelection
Editor::get_regions_from_selection_and_edit_point (EditIgnoreOption ignore, bool from_context_menu, bool from_outside_canvas)
{
	RegionSelection regions;

	if (_edit_point == EditAtMouse && entered_regionview && selection->tracks.empty() && selection->regions.empty()) {
		regions.add (entered_regionview);
	} else {
		regions = selection->regions;
	}

	if (regions.empty()) {
		TrackViewList tracks = selection->tracks;

		if (!tracks.empty()) {
			/* no region selected or entered, but some selected tracks:
			 * act on all regions on the selected tracks at the edit point
			 */
			timepos_t const where = get_preferred_edit_position (ignore, from_context_menu, from_outside_canvas);
			get_regions_at(regions, where, tracks);
		}
	}

	return regions;
}

/** Get regions using the following method:
 *
 *  Make a region list using:
 *   (a) any selected regions
 *   (b) the intersection of any selected tracks and the edit point(*)
 *   (c) if neither exists, then whatever region is under the mouse
 *
 *  (*) NOTE: in this case, if 'No Selection = All Tracks' is active, search all tracks
 *
 *  Note that we have forced the rule that selected regions and selected tracks are mutually exclusive
 */
RegionSelection
Editor::get_regions_from_selection_and_mouse (timepos_t const & pos)
{
	RegionSelection regions;

	if (entered_regionview && selection->tracks.empty() && selection->regions.empty()) {
		regions.add (entered_regionview);
	} else {
		regions = selection->regions;
	}

	if (regions.empty()) {
		TrackViewList tracks = selection->tracks;

		if (!tracks.empty()) {
			/* no region selected or entered, but some selected tracks:
			 * act on all regions on the selected tracks at the edit point
			 */
			get_regions_at (regions, pos, tracks);
		}
	}

	return regions;
}

/** Start with the selected Region(s) or TriggerSlot
 *  if neither is found, try using the entered_regionview (region under the mouse).
 */

RegionSelection
Editor::get_regions_from_selection_and_entered () const
{
	RegionSelection regions = selection->regions;

	if (regions.empty() && !selection->triggers.empty()) {
		regions = selection->trigger_regionview_proxy();
	}

	if (regions.empty() && entered_regionview) {
		regions.add (entered_regionview);
	}

	return regions;
}

void
Editor::get_regionviews_by_id (PBD::ID const id, RegionSelection & regions) const
{
	for (TrackViewList::const_iterator i = track_views.begin(); i != track_views.end(); ++i) {
		RouteTimeAxisView* rtav;

		if ((rtav = dynamic_cast<RouteTimeAxisView*> (*i)) != 0) {
			boost::shared_ptr<Playlist> pl;
			std::vector<boost::shared_ptr<Region> > results;
			boost::shared_ptr<Track> tr;

			if ((tr = rtav->track()) == 0) {
				/* bus */
				continue;
			}

			if ((pl = (tr->playlist())) != 0) {
				boost::shared_ptr<Region> r = pl->region_by_id (id);
				if (r) {
					RegionView* rv = rtav->view()->find_view (r);
					if (rv) {
						regions.push_back (rv);
					}
				}
			}
		}
	}
}

void
Editor::get_per_region_note_selection (list<pair<PBD::ID, set<boost::shared_ptr<Evoral::Note<Temporal::Beats> > > > > &selection) const
{

	for (TrackViewList::const_iterator i = track_views.begin(); i != track_views.end(); ++i) {
		MidiTimeAxisView* mtav;

		if ((mtav = dynamic_cast<MidiTimeAxisView*> (*i)) != 0) {

			mtav->get_per_region_note_selection (selection);
		}
	}

}

void
Editor::get_regionview_corresponding_to (boost::shared_ptr<Region> region, vector<RegionView*>& regions)
{
	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {

		RouteTimeAxisView* tatv;

		if ((tatv = dynamic_cast<RouteTimeAxisView*> (*i)) != 0) {

			boost::shared_ptr<Playlist> pl;
			RegionView* marv;
			boost::shared_ptr<Track> tr;

			if ((tr = tatv->track()) == 0) {
				/* bus */
				continue;
			}

			if ((marv = tatv->view()->find_view (region)) != 0) {
				regions.push_back (marv);
			}
		}
	}
}

RegionView*
Editor::regionview_from_region (boost::shared_ptr<Region> region) const
{
	for (TrackViewList::const_iterator i = track_views.begin(); i != track_views.end(); ++i) {
		RouteTimeAxisView* tatv;
		if ((tatv = dynamic_cast<RouteTimeAxisView*> (*i)) != 0) {
			if (!tatv->track()) {
				continue;
			}
			RegionView* marv = tatv->view()->find_view (region);
			if (marv) {
				return marv;
			}
		}
	}
	return NULL;
}

RouteTimeAxisView*
Editor::rtav_from_route (boost::shared_ptr<Route> route) const
{
	for (TrackViewList::const_iterator i = track_views.begin(); i != track_views.end(); ++i) {
		RouteTimeAxisView* rtav;
		if ((rtav = dynamic_cast<RouteTimeAxisView*> (*i)) != 0) {
			if (rtav->route() == route) {
				return rtav;
			}
		}
	}
	return NULL;
}

void
Editor::show_rhythm_ferret ()
{
	if (rhythm_ferret == 0) {
		rhythm_ferret = new RhythmFerret(*this);
	}

	rhythm_ferret->set_session (_session);
	rhythm_ferret->show ();
	rhythm_ferret->present ();
}

void
Editor::first_idle ()
{
	ArdourMessageDialog* dialog = 0;

	if (track_views.size() > 1) {
		Timers::TimerSuspender t;
		dialog = new ArdourMessageDialog (
			string_compose (_("Please wait while %1 loads visual data."), PROGRAM_NAME),
			true
			);
		dialog->present ();
		ARDOUR_UI::instance()->flush_pending (60);
	}

	for (TrackViewList::iterator t = track_views.begin(); t != track_views.end(); ++t) {
		(*t)->first_idle();
	}

	/* now that all regionviews should exist, setup region selection */

	RegionSelection rs;

	for (list<PBD::ID>::iterator pr = selection->regions.pending.begin (); pr != selection->regions.pending.end (); ++pr) {
		/* this is cumulative: rs is NOT cleared each time */
		get_regionviews_by_id (*pr, rs);
	}

	selection->set (rs);

	/* first idle adds route children (automation tracks), so we need to redisplay here */
	redisplay_track_views ();

	delete dialog;

	if (_session->undo_depth() == 0) {
		undo_action->set_sensitive(false);
	}
	redo_action->set_sensitive(false);
	begin_selection_op_history ();

	_have_idled = true;
}

gboolean
Editor::_idle_resize (gpointer arg)
{
	return ((Editor*)arg)->idle_resize ();
}

void
Editor::add_to_idle_resize (TimeAxisView* view, int32_t h)
{
	if (resize_idle_id < 0) {
		/* https://developer.gnome.org/glib/stable/glib-The-Main-Event-Loop.html#G-PRIORITY-HIGH-IDLE:CAPS
		 * GTK+ uses G_PRIORITY_HIGH_IDLE + 10 for resizing operations, and G_PRIORITY_HIGH_IDLE + 20 for redrawing operations.
		 * (This is done to ensure that any pending resizes are processed before any pending redraws, so that widgets are not redrawn twice unnecessarily.)
		 */
		resize_idle_id = g_idle_add_full (G_PRIORITY_HIGH_IDLE + 10, _idle_resize, this, NULL);
		_pending_resize_amount = 0;
	}

	/* make a note of the smallest resulting height, so that we can clamp the
	   lower limit at TimeAxisView::hSmall */

	int32_t min_resulting = INT32_MAX;

	_pending_resize_amount += h;
	_pending_resize_view = view;

	min_resulting = min (min_resulting, int32_t (_pending_resize_view->current_height()) + _pending_resize_amount);

	if (selection->tracks.contains (_pending_resize_view)) {
		for (TrackViewList::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
			min_resulting = min (min_resulting, int32_t ((*i)->current_height()) + _pending_resize_amount);
		}
	}

	if (min_resulting < 0) {
		min_resulting = 0;
	}

	/* clamp */
	if (uint32_t (min_resulting) < TimeAxisView::preset_height (HeightSmall)) {
		_pending_resize_amount += TimeAxisView::preset_height (HeightSmall) - min_resulting;
	}
}

/** Handle pending resizing of tracks */
bool
Editor::idle_resize ()
{
	_pending_resize_view->idle_resize (_pending_resize_view->current_height() + _pending_resize_amount);

	if (dynamic_cast<AutomationTimeAxisView*> (_pending_resize_view) == 0 &&
	    selection->tracks.contains (_pending_resize_view)) {

		for (TrackViewList::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
			if (*i != _pending_resize_view) {
				(*i)->idle_resize ((*i)->current_height() + _pending_resize_amount);
			}
		}
	}

	_pending_resize_amount = 0;
	_group_tabs->set_dirty ();
	resize_idle_id = -1;

	return false;
}

void
Editor::located ()
{
	ENSURE_GUI_THREAD (*this, &Editor::located);

	if (_session) {
		_playhead_cursor->set_position (_session->audible_sample ());
		if (_follow_playhead && !_pending_initial_locate) {
			reset_x_origin_to_follow_playhead ();
		}
	}

	_pending_locate_request = false;
	_pending_initial_locate = false;
	_last_update_time = 0;
}

void
Editor::region_view_added (RegionView * rv)
{
	MidiRegionView* mrv = dynamic_cast<MidiRegionView*> (rv);
	if (mrv) {
		list<pair<PBD::ID const, list<Evoral::event_id_t> > >::iterator rnote;
		for (rnote = selection->pending_midi_note_selection.begin(); rnote != selection->pending_midi_note_selection.end(); ++rnote) {
			if (rv->region()->id () == (*rnote).first) {
				list<Evoral::event_id_t> notes ((*rnote).second);
				selection->pending_midi_note_selection.erase(rnote);
				mrv->select_notes (notes, false); // NB. this may change the selection
				break;
			}
		}
	}

	_summary->set_background_dirty ();

	mark_region_boundary_cache_dirty ();
}

void
Editor::region_view_removed ()
{
	_summary->set_background_dirty ();

	mark_region_boundary_cache_dirty ();
}

AxisView*
Editor::axis_view_by_stripable (boost::shared_ptr<Stripable> s) const
{
	for (TrackViewList::const_iterator j = track_views.begin (); j != track_views.end(); ++j) {
		if ((*j)->stripable() == s) {
			return *j;
		}
	}

	return 0;
}

AxisView*
Editor::axis_view_by_control (boost::shared_ptr<AutomationControl> c) const
{
	for (TrackViewList::const_iterator j = track_views.begin (); j != track_views.end(); ++j) {
		if ((*j)->control() == c) {
			return *j;
		}

		TimeAxisView::Children kids = (*j)->get_child_list ();

		for (TimeAxisView::Children::iterator k = kids.begin(); k != kids.end(); ++k) {
			if ((*k)->control() == c) {
				return (*k).get();
			}
		}
	}

	return 0;
}

TrackViewList
Editor::axis_views_from_routes (boost::shared_ptr<RouteList> r) const
{
	TrackViewList t;

	for (RouteList::const_iterator i = r->begin(); i != r->end(); ++i) {
		TimeAxisView* tv = time_axis_view_from_stripable (*i);
		if (tv) {
			t.push_back (tv);
		}
	}

	return t;
}

void
Editor::suspend_route_redisplay ()
{
	_tvl_no_redisplay = true;
}

void
Editor::queue_redisplay_track_views ()
{
	if (!_tvl_redisplay_connection.connected ()) {
		_tvl_redisplay_connection = Glib::signal_idle().connect (sigc::mem_fun (*this, &Editor::redisplay_track_views));
	}
}

void
Editor::process_redisplay_track_views ()
{
	if (_tvl_redisplay_connection.connected ()) {
		_tvl_redisplay_connection.disconnect ();
		redisplay_track_views ();
	}
}

void
Editor::redisplay_track_views_now ()
{
	_tvl_redisplay_connection.disconnect ();
	redisplay_track_views ();
}

void
Editor::resume_route_redisplay ()
{
	_tvl_no_redisplay = false;
	if (_tvl_redisplay_on_resume) {
		queue_redisplay_track_views ();
	}
}

void
Editor::initial_display ()
{
	DisplaySuspender ds;
	StripableList s;
	_session->get_stripables (s);
	add_stripables (s);
}

void
Editor::add_vcas (VCAList& vlist)
{
	StripableList sl;

	for (VCAList::iterator v = vlist.begin(); v != vlist.end(); ++v) {
		sl.push_back (boost::dynamic_pointer_cast<Stripable> (*v));
	}

	add_stripables (sl);
}

void
Editor::add_routes (RouteList& rlist)
{
	StripableList sl;

	for (RouteList::iterator r = rlist.begin(); r != rlist.end(); ++r) {
		sl.push_back (*r);
	}

	add_stripables (sl);
}

void
Editor::add_stripables (StripableList& sl)
{
	boost::shared_ptr<VCA> v;
	boost::shared_ptr<Route> r;
	TrackViewList new_selection;
	bool changed = false;
	bool from_scratch = (track_views.size() == 0);

	sl.sort (Stripable::Sorter());

	DisplaySuspender ds;

	for (StripableList::iterator s = sl.begin(); s != sl.end(); ++s) {

		if ((*s)->is_foldbackbus()) {
			continue;
		}

		if ((v = boost::dynamic_pointer_cast<VCA> (*s)) != 0) {

			VCATimeAxisView* vtv = new VCATimeAxisView (*this, _session, *_track_canvas);
			vtv->set_vca (v);
			track_views.push_back (vtv);

			(*s)->gui_changed.connect (*this, invalidator (*this), boost::bind (&Editor::handle_gui_changes, this, _1, _2), gui_context());
			changed = true;

		} else if ((r = boost::dynamic_pointer_cast<Route> (*s)) != 0) {

			if (r->is_auditioner() || r->is_monitor()) {
				continue;
			}

			RouteTimeAxisView* rtv;
			DataType dt = r->input()->default_type();

			if (dt == ARDOUR::DataType::AUDIO) {
				rtv = new AudioTimeAxisView (*this, _session, *_track_canvas);
				rtv->set_route (r);
			} else if (dt == ARDOUR::DataType::MIDI) {
				rtv = new MidiTimeAxisView (*this, _session, *_track_canvas);
				rtv->set_route (r);
			} else {
				throw unknown_type();
			}

			track_views.push_back (rtv);
			new_selection.push_back (rtv);

			rtv->effective_gain_display ();

			rtv->view()->RegionViewAdded.connect (sigc::mem_fun (*this, &Editor::region_view_added));
			rtv->view()->RegionViewRemoved.connect (sigc::mem_fun (*this, &Editor::region_view_removed));
			(*s)->gui_changed.connect (*this, invalidator (*this), boost::bind (&Editor::handle_gui_changes, this, _1, _2), gui_context());
			changed = true;
		}
	}

	if (changed) {
		queue_redisplay_track_views ();
	}

	/* note: !new_selection.empty() means that we got some routes rather
	 * than just VCAs
	 */

	if (!from_scratch && !new_selection.empty()) {
		selection->set (new_selection);
		begin_selection_op_history();
	}

	if (show_editor_mixer_when_tracks_arrive && !new_selection.empty()) {
		show_editor_mixer (true);
	}

	editor_list_button.set_sensitive (true);
}

void
Editor::timeaxisview_deleted (TimeAxisView *tv)
{
	if (tv == entered_track) {
		entered_track = 0;
	}

	if (_session && _session->deletion_in_progress()) {
		/* the situation is under control */
		return;
	}

	DisplaySuspender ds;

	ENSURE_GUI_THREAD (*this, &Editor::timeaxisview_deleted, tv);

	if (dynamic_cast<AutomationTimeAxisView*> (tv)) {
		selection->remove (tv);
		return;
	}

	RouteTimeAxisView* rtav = dynamic_cast<RouteTimeAxisView*> (tv);

	TimeAxisView::Children c = tv->get_child_list ();
	for (TimeAxisView::Children::const_iterator i = c.begin(); i != c.end(); ++i) {
		if (entered_track == i->get()) {
			entered_track = 0;
		}
	}

	/* remove it from the list of track views */

	TrackViewList::iterator i;

	if ((i = find (track_views.begin(), track_views.end(), tv)) != track_views.end()) {
		i = track_views.erase (i);
	}

	/* Update the route that is shown in the editor-mixer. */
	if (!rtav) {
		return;
	}

	boost::shared_ptr<Route> route = rtav->route ();
	if (current_mixer_strip && current_mixer_strip->route() == route) {

		TimeAxisView* next_tv;

		if (track_views.empty()) {
			next_tv = 0;
		} else if (i == track_views.end()) {
			next_tv = track_views.front();
		} else {
			next_tv = (*i);
		}

		// skip VCAs (cannot be selected, n/a in editor-mixer)
		if (dynamic_cast<VCATimeAxisView*> (next_tv)) {
			/* VCAs are sorted last in line -- route_sorter.h, jump to top */
			next_tv = track_views.front();
		}
		if (dynamic_cast<VCATimeAxisView*> (next_tv)) {
			/* just in case: no master, only a VCA remains */
			next_tv = 0;
		}


		if (next_tv) {
			set_selected_mixer_strip (*next_tv);
		} else {
			/* make the editor mixer strip go away setting the
			 * button to inactive (which also unticks the menu option)
			 */

			ActionManager::uncheck_toggleaction ("Editor/show-editor-mixer");
		}
	}
}

void
Editor::hide_track_in_display (TimeAxisView* tv, bool apply_to_selection)
{
	if (!tv) {
		return;
	}

	DisplaySuspender ds;
	PresentationInfo::ChangeSuspender cs;

	if (apply_to_selection) {
		for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end();) {

			TrackSelection::iterator j = i;
			++j;

			hide_track_in_display (*i, false);

			i = j;
		}
	} else {
		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (tv);

		if (rtv && current_mixer_strip && (rtv->route() == current_mixer_strip->route())) {
			/* this will hide the mixer strip */
			set_selected_mixer_strip (*tv);
		}
		if (rtv) {
			rtv->route()->presentation_info().set_hidden (true);
			/* TODO also handle Routegroups IFF (rg->is_hidden() && !rg->is_selection())
			 * selection currently unconditionally hides due to above if() clause :(
			 */
		}
	}
}

void
Editor::show_track_in_display (TimeAxisView* tv, bool move_into_view)
{
	if (!tv) {
		return;
	}
	RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (tv);
	if (rtv) {
		rtv->route()->presentation_info().set_hidden (false);
#if 0 // TODO see above
		RouteGroup* rg = rtv->route ()->route_group ();
		if (rg && rg->is_active () && rg->is_hidden () && !rg->is_select ()) {
			boost::shared_ptr<RouteList> rl (rg->route_list ());
			for (RouteList::const_iterator i = rl->begin(); i != rl->end(); ++i) {
				(*i)->presentation_info().set_hidden (false);
			}
	}
#endif
	}
	if (move_into_view) {
		ensure_time_axis_view_is_visible (*tv, false);
	}
}

struct TrackViewStripableSorter
{
  bool operator() (const TimeAxisView* tav_a, const TimeAxisView *tav_b)
  {
    StripableTimeAxisView const* stav_a = dynamic_cast<StripableTimeAxisView const*>(tav_a);
    StripableTimeAxisView const* stav_b = dynamic_cast<StripableTimeAxisView const*>(tav_b);
    assert (stav_a && stav_b);

    boost::shared_ptr<ARDOUR::Stripable> const& a = stav_a->stripable ();
    boost::shared_ptr<ARDOUR::Stripable> const& b = stav_b->stripable ();
    return ARDOUR::Stripable::Sorter () (a, b);
  }
};

bool
Editor::redisplay_track_views ()
{
	if (!_session || _session->deletion_in_progress()) {
		return false;
	}

	if (_tvl_no_redisplay) {
		_tvl_redisplay_on_resume = true;
		return false;
	}

	_tvl_redisplay_on_resume = false;

	track_views.sort (TrackViewStripableSorter ());

	uint32_t position;
	TrackViewList::const_iterator i;

	/* n will be the count of tracks plus children (updated by TimeAxisView::show_at),
	 * so we will use that to know where to put things.
	 */
	int n;
	for (n = 0, position = 0, i = track_views.begin(); i != track_views.end(); ++i) {
		TimeAxisView *tv = (*i);

		if (tv->marked_for_display ()) {
			position += tv->show_at (position, n, &edit_controls_vbox);
		} else {
			tv->hide ();
		}
		n++;
	}

	reset_controls_layout_height (position);
	reset_controls_layout_width ();
	_full_canvas_height = position;

	if ((vertical_adjustment.get_value() + _visible_canvas_height) > vertical_adjustment.get_upper()) {
		/*
		 * We're increasing the size of the canvas while the bottom is visible.
		 * We scroll down to keep in step with the controls layout.
		 */
		vertical_adjustment.set_value (_full_canvas_height - _visible_canvas_height);
	}

	_summary->set_background_dirty();
	_group_tabs->set_dirty ();

	return false;
}

void
Editor::handle_gui_changes (string const & what, void*)
{
	if (what == "track_height" || what == "visible_tracks") {
		queue_redisplay_track_views ();
	}
}

void
Editor::foreach_time_axis_view (sigc::slot<void,TimeAxisView&> theslot)
{
	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		theslot (**i);
	}
}

/** Find a StripableTimeAxisView by the ID of its stripable */
StripableTimeAxisView*
Editor::get_stripable_time_axis_by_id (const PBD::ID& id) const
{
	StripableTimeAxisView* v;

	for (TrackViewList::const_iterator i = track_views.begin(); i != track_views.end(); ++i) {
		if((v = dynamic_cast<StripableTimeAxisView*>(*i)) != 0) {
			if(v->stripable()->id() == id) {
				return v;
			}
		}
	}

	return 0;
}

void
Editor::fit_route_group (RouteGroup *g)
{
	TrackViewList ts = axis_views_from_routes (g->route_list ());
	fit_tracks (ts);
}

void
Editor::consider_auditioning (boost::shared_ptr<Region> region)
{
	boost::shared_ptr<AudioRegion> r = boost::dynamic_pointer_cast<AudioRegion> (region);

	if (r == 0) {
		_session->cancel_audition ();
		return;
	}

	if (_session->is_auditioning()) {
		_session->cancel_audition ();
		if (r == last_audition_region) {
			return;
		}
	}

	_session->audition_region (r);
	last_audition_region = r;
}

void
Editor::hide_a_region (boost::shared_ptr<Region> r)
{
	r->set_hidden (true);
}

void
Editor::show_a_region (boost::shared_ptr<Region> r)
{
	r->set_hidden (false);
}

void
Editor::audition_region_from_region_list ()
{
	_regions->selection_mapover (sigc::mem_fun (*this, &Editor::consider_auditioning));
}

void
Editor::step_edit_status_change (bool yn)
{
	if (yn) {
		start_step_editing ();
	} else {
		stop_step_editing ();
	}
}

void
Editor::start_step_editing ()
{
	step_edit_connection = Glib::signal_timeout().connect (sigc::mem_fun (*this, &Editor::check_step_edit), 20);
}

void
Editor::stop_step_editing ()
{
	step_edit_connection.disconnect ();
}

bool
Editor::check_step_edit ()
{
	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		MidiTimeAxisView* mtv = dynamic_cast<MidiTimeAxisView*> (*i);
		if (mtv) {
			mtv->check_step_edit ();
		}
	}

	return true; // do it again, till we stop
}

bool
Editor::scroll_press (Direction dir)
{
	++_scroll_callbacks;

	if (_scroll_connection.connected() && _scroll_callbacks < 5) {
		/* delay the first auto-repeat */
		return true;
	}

	switch (dir) {
	case LEFT:
		scroll_backward (1);
		break;

	case RIGHT:
		scroll_forward (1);
		break;

	case UP:
		scroll_up_one_track ();
		break;

	case DOWN:
		scroll_down_one_track ();
		break;
	}

	/* do hacky auto-repeat */
	if (!_scroll_connection.connected ()) {

		_scroll_connection = Glib::signal_timeout().connect (
			sigc::bind (sigc::mem_fun (*this, &Editor::scroll_press), dir), 100
			);

		_scroll_callbacks = 0;
	}

	return true;
}

void
Editor::scroll_release ()
{
	_scroll_connection.disconnect ();
}

/** Queue a change for the Editor viewport x origin to follow the playhead */
void
Editor::reset_x_origin_to_follow_playhead ()
{
	samplepos_t const sample = _playhead_cursor->current_sample ();

	if (sample < _leftmost_sample || sample > _leftmost_sample + current_page_samples()) {

		if (_session->transport_speed() < 0) {

			if (sample > (current_page_samples() / 2)) {
				center_screen (sample-(current_page_samples()/2));
			} else {
				center_screen (current_page_samples()/2);
			}

		} else {

			samplepos_t l = 0;

			if (sample < _leftmost_sample) {
				/* moving left */
				if (_session->transport_rolling()) {
					/* rolling; end up with the playhead at the right of the page */
					l = sample - current_page_samples ();
				} else {
					/* not rolling: end up with the playhead 1/4 of the way along the page */
					l = sample - current_page_samples() / 4;
				}
			} else {
				/* moving right */
				if (_session->transport_rolling()) {
					/* rolling: end up with the playhead on the left of the page */
					l = sample;
				} else {
					/* not rolling: end up with the playhead 3/4 of the way along the page */
					l = sample - 3 * current_page_samples() / 4;
				}
			}

			if (l < 0) {
				l = 0;
			}

			center_screen_internal (l + (current_page_samples() / 2), current_page_samples ());
		}
	}
}

void
Editor::super_rapid_screen_update ()
{
	if (!_session || !_session->engine().running()) {
		return;
	}

	/* METERING / MIXER STRIPS */

	/* update track meters, if required */
	if (contents().is_mapped() && meters_running) {
		RouteTimeAxisView* rtv;
		for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
			if ((rtv = dynamic_cast<RouteTimeAxisView*>(*i)) != 0) {
				rtv->fast_update ();
			}
		}
	}

	/* and any current mixer strip */
	if (current_mixer_strip) {
		current_mixer_strip->fast_update ();
	}

	bool latent_locate = false;
	samplepos_t sample = _session->audible_sample (&latent_locate);
	const int64_t now = g_get_monotonic_time ();
	double err = 0;

	if (_session->exporting ()) {
		/* freewheel/export may be faster or slower than transport_speed() / SR.
		 * Also exporting multiple ranges locates/jumps without a _pending_locate_request.
		 */
		_last_update_time = 0;
	}

	if (!_session->transport_rolling () || _session->is_auditioning ()) {
		/* Do not interpolate the playhead position; just set it */
		_last_update_time = 0;
	}

	if (_last_update_time > 0) {
		/* interpolate and smoothen playhead position */
		const double ds =  (now - _last_update_time) * _session->transport_speed() * _session->nominal_sample_rate () * 1e-6;
		samplepos_t guess = _playhead_cursor->current_sample () + rint (ds);
		err = sample - guess;

		guess += err * .12 + _err_screen_engine; // time-constant based on 25fps (super_rapid_screen_update)
		_err_screen_engine += .0144 * (err - _err_screen_engine); // tc^2

#if 0 // DEBUG
		printf ("eng: %ld  gui:%ld (%+6.1f)  diff: %6.1f (err: %7.2f)\n",
				sample, guess, ds,
				err, _err_screen_engine);
#endif

		sample = guess;
	} else {
		_err_screen_engine = 0;
	}

	if (err > 8192 || latent_locate) {
		// in case of xruns or freewheeling
		_last_update_time = 0;
		sample = _session->audible_sample ();
	} else {
		_last_update_time = now;
	}

	/* snapped cursor stuff (the snapped_cursor shows where an operation is going to occur) */
	bool ignored;
	MusicSample where (sample, 0);
	if (!UIConfiguration::instance().get_show_snapped_cursor()) {
		_snapped_cursor->hide ();
	} else if (_edit_point == EditAtPlayhead && !_dragging_playhead) {
		/* EditAtPlayhead does not snap */
	} else if (_edit_point == EditAtSelectedMarker) {
		/* NOTE: I don't think EditAtSelectedMarker should snap. They are what they are.
		 * however, the current editing code -does- snap so I'll draw it that way for now.
		 */
		if (!selection->markers.empty()) {
			timepos_t ms (selection->markers.front()->position());
			snap_to (ms); // should use snap_to_with_modifier?
			_snapped_cursor->set_position (ms.samples());
			_snapped_cursor->show ();
		}
	} else if (_edit_point == EditAtMouse && mouse_sample (where.sample, ignored)) {
		/* cursor is in the editing canvas. show it. */
		_snapped_cursor->show ();
	} else {
		/* mouse is out of the editing canvas, or edit-point isn't mouse. Hide the snapped_cursor */
		_snapped_cursor->hide ();
	}

	/* There are a few reasons why we might not update the playhead / viewport stuff:
	 *
	 * 1.  we don't update things when there's a pending locate request, otherwise
	 *     when the editor requests a locate there is a chance that this method
	 *     will move the playhead before the locate request is processed, causing
	 *     a visual glitch.
	 * 2.  if we're not rolling, there's nothing to do here (locates are handled elsewhere).
	 * 3.  if we're still at the same frame that we were last time, there's nothing to do.
	 */
	if (_pending_locate_request) {
		_last_update_time = 0;
		return;
	}

	if (_dragging_playhead) {
		_last_update_time = 0;
		return;
	}

	if (_playhead_cursor->current_sample () == sample) {
		return;
	}

	if (!_pending_locate_request && !_session->locate_initiated()) {
		_playhead_cursor->set_position (sample);
	}

	if (_session->requested_return_sample() >= 0) {
		_last_update_time = 0;
		return;
	}

	if (!_follow_playhead || pending_visual_change.being_handled) {
		/* We only do this if we aren't already
		 * handling a visual change (ie if
		 * pending_visual_change.being_handled is
		 * false) so that these requests don't stack
		 * up there are too many of them to handle in
		 * time.
		 */
		return;
	}

	if (!_stationary_playhead) {
		reset_x_origin_to_follow_playhead ();
	} else {
		samplepos_t const sample = _playhead_cursor->current_sample ();
		double target = ((double)sample - (double)current_page_samples() / 2.0);
		if (target <= 0.0) {
			target = 0.0;
		}
		/* compare to EditorCursor::set_position() */
		double const old_pos = sample_to_pixel_unrounded (_leftmost_sample);
		double const new_pos = sample_to_pixel_unrounded (target);
		if (rint (new_pos) != rint (old_pos)) {
			reset_x_origin (pixel_to_sample (new_pos));
		}
	}
}


void
Editor::session_going_away ()
{
	_have_idled = false;

	_session_connections.drop_connections ();

	super_rapid_screen_update_connection.disconnect ();

	selection->clear ();
	cut_buffer->clear ();

	clicked_regionview = 0;
	clicked_axisview = 0;
	clicked_routeview = 0;
	entered_regionview = 0;
	entered_track = 0;
	_last_update_time = 0;
	_drags->abort ();

	_playhead_cursor->hide ();

	/* rip everything out of the list displays */

	_routes->clear ();
	_route_groups->clear ();

	/* do this first so that deleting a track doesn't reset cms to null
	   and thus cause a leak.
	*/

	if (current_mixer_strip) {
		if (current_mixer_strip->get_parent() != 0) {
			global_hpacker.remove (*current_mixer_strip);
		}
		delete current_mixer_strip;
		current_mixer_strip = 0;
	}

	/* delete all trackviews */

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		delete *i;
	}
	track_views.clear ();

	nudge_clock->set_session (0);

	editor_list_button.set_active(false);
	editor_list_button.set_sensitive(false);

	/* clear tempo/meter rulers */
	remove_metric_marks ();
	clear_marker_display ();

	hide_grid_lines ();
	delete grid_lines;
	grid_lines = 0;

	stop_step_editing ();

	if (own_window()) {

		/* get rid of any existing editor mixer strip */

		WindowTitle title(Glib::get_application_name());
		title += _("Editor");

		own_window()->set_title (title.get_string());
	}

	SessionHandlePtr::session_going_away ();
}

void
Editor::trigger_script (int i)
{
	LuaInstance::instance()-> call_action (i);
}

void
Editor::show_editor_list (bool yn)
{
	if (yn) {
		_editor_list_vbox.show ();
	} else {
		_editor_list_vbox.hide ();
	}
}

void
Editor::change_region_layering_order (bool from_context_menu)
{
	const timepos_t position = get_preferred_edit_position (EDIT_IGNORE_NONE, from_context_menu);

	if (!clicked_routeview) {
		if (layering_order_editor) {
			layering_order_editor->hide ();
		}
		return;
	}

	boost::shared_ptr<Track> track = boost::dynamic_pointer_cast<Track> (clicked_routeview->route());

	if (!track) {
		return;
	}

	boost::shared_ptr<Playlist> pl = track->playlist();

	if (!pl) {
		return;
	}

	if (layering_order_editor == 0) {
		layering_order_editor = new RegionLayeringOrderEditor (*this);
	}

	layering_order_editor->set_context (clicked_routeview->name(), _session, clicked_routeview, pl, position);
	layering_order_editor->maybe_present ();
}

void
Editor::update_region_layering_order_editor ()
{
	if (layering_order_editor && layering_order_editor->is_visible ()) {
		change_region_layering_order (true);
	}
}

void
Editor::setup_fade_images ()
{
	_xfade_in_images[FadeLinear] = new Gtk::Image (get_icon_path (X_("fadein-linear")));
	_xfade_in_images[FadeSymmetric] = new Gtk::Image (get_icon_path (X_("fadein-symmetric")));
	_xfade_in_images[FadeFast] = new Gtk::Image (get_icon_path (X_("fadein-fast-cut")));
	_xfade_in_images[FadeSlow] = new Gtk::Image (get_icon_path (X_("fadein-slow-cut")));
	_xfade_in_images[FadeConstantPower] = new Gtk::Image (get_icon_path (X_("fadein-constant-power")));

	_xfade_out_images[FadeLinear] = new Gtk::Image (get_icon_path (X_("fadeout-linear")));
	_xfade_out_images[FadeSymmetric] = new Gtk::Image (get_icon_path (X_("fadeout-symmetric")));
	_xfade_out_images[FadeFast] = new Gtk::Image (get_icon_path (X_("fadeout-fast-cut")));
	_xfade_out_images[FadeSlow] = new Gtk::Image (get_icon_path (X_("fadeout-slow-cut")));
	_xfade_out_images[FadeConstantPower] = new Gtk::Image (get_icon_path (X_("fadeout-constant-power")));

}

/** @return Gtk::manage()d menu item for a given action from `editor_actions' */
Gtk::MenuItem&
Editor::action_menu_item (std::string const & name)
{
	Glib::RefPtr<Action> a = editor_actions->get_action (name);
	assert (a);

	return *manage (a->create_menu_item ());
}

void
Editor::add_notebook_page (string const & name, Gtk::Widget& widget)
{
	EventBox* b = manage (new EventBox);
	b->signal_button_press_event().connect (sigc::bind (sigc::mem_fun (*this, &Editor::notebook_tab_clicked), &widget));
	Label* l = manage (new Label (name));
	l->set_angle (-90);
	b->add (*l);
	b->show_all ();
	_the_notebook.append_page (widget, *b);
}

bool
Editor::notebook_tab_clicked (GdkEventButton* ev, Gtk::Widget* page)
{
	if (ev->type == GDK_BUTTON_PRESS || ev->type == GDK_2BUTTON_PRESS) {
		_the_notebook.set_current_page (_the_notebook.page_num (*page));
	}

	if (ev->type == GDK_2BUTTON_PRESS) {

		/* double-click on a notebook tab shrinks or expands the notebook */

		if (_notebook_shrunk) {
			if (pre_notebook_shrink_pane_width) {
				edit_pane.set_divider (0, *pre_notebook_shrink_pane_width);
			}
			_notebook_shrunk = false;
		} else {
			pre_notebook_shrink_pane_width = edit_pane.get_divider();

			/* this expands the LHS of the edit pane to cover the notebook
			   PAGE but leaves the tabs visible.
			 */
			edit_pane.set_divider (0, edit_pane.get_divider() + page->get_width());
			_notebook_shrunk = true;
		}
	}

	return true;
}

void
Editor::popup_control_point_context_menu (ArdourCanvas::Item* item, GdkEvent* event)
{
	using namespace Menu_Helpers;

	MenuList& items = _control_point_context_menu.items ();
	items.clear ();

	items.push_back (MenuElem (_("Edit..."), sigc::bind (sigc::mem_fun (*this, &Editor::edit_control_point), item)));
	items.push_back (MenuElem (_("Delete"), sigc::bind (sigc::mem_fun (*this, &Editor::remove_control_point), item)));
	if (!can_remove_control_point (item)) {
		items.back().set_sensitive (false);
	}

	_control_point_context_menu.popup (event->button.button, event->button.time);
}

void
Editor::popup_note_context_menu (ArdourCanvas::Item* item, GdkEvent* event)
{
	using namespace Menu_Helpers;

	NoteBase* note = reinterpret_cast<NoteBase*>(item->get_data("notebase"));
	if (!note) {
		return;
	}

	/* We need to get the selection here and pass it to the operations, since
	   popping up the menu will cause a region leave event which clears
	   entered_regionview. */

	MidiRegionView&       mrv = note->region_view();
	const RegionSelection rs  = get_regions_from_selection_and_entered ();
	const uint32_t sel_size = mrv.selection_size ();

	MenuList& items = _note_context_menu.items();
	items.clear();

	if (sel_size > 0) {
		items.push_back(MenuElem(_("Delete"),
					 sigc::mem_fun(mrv, &MidiRegionView::delete_selection)));
	}

	items.push_back(MenuElem(_("Edit..."),
				 sigc::bind(sigc::mem_fun(*this, &Editor::edit_notes), &mrv)));
	if (sel_size != 1) {
		items.back().set_sensitive (false);
	}

	items.push_back(MenuElem(_("Transpose..."),
	                         sigc::bind(sigc::mem_fun(*this, &Editor::transpose_regions), rs)));


	items.push_back(MenuElem(_("Legatize"),
				 sigc::bind(sigc::mem_fun(*this, &Editor::legatize_regions), rs, false)));
	if (sel_size < 2) {
		items.back().set_sensitive (false);
	}

	items.push_back(MenuElem(_("Quantize..."),
	                         sigc::bind(sigc::mem_fun(*this, &Editor::quantize_regions), rs)));

	items.push_back(MenuElem(_("Remove Overlap"),
				 sigc::bind(sigc::mem_fun(*this, &Editor::legatize_regions), rs, true)));
	if (sel_size < 2) {
		items.back().set_sensitive (false);
	}

	items.push_back(MenuElem(_("Transform..."),
	                         sigc::bind(sigc::mem_fun(*this, &Editor::transform_regions), rs)));

	_note_context_menu.popup (event->button.button, event->button.time);
}

void
Editor::zoom_vertical_modifier_released()
{
	_stepping_axis_view = 0;
}

void
Editor::ui_parameter_changed (string parameter)
{
	if (parameter == "icon-set") {
		while (!_cursor_stack.empty()) {
			_cursor_stack.pop_back();
		}
		_cursors->set_cursor_set (UIConfiguration::instance().get_icon_set());
		_cursor_stack.push_back(_cursors->grabber);
		edit_pane.set_drag_cursor (*_cursors->expand_left_right);
		editor_summary_pane.set_drag_cursor (*_cursors->expand_up_down);

	} else if (parameter == "draggable-playhead") {
		if (_verbose_cursor) {
			_playhead_cursor->set_sensitive (UIConfiguration::instance().get_draggable_playhead());
		}
	} else if (parameter == "use-note-bars-for-velocity") {
		ArdourCanvas::Note::set_show_velocity_bars (UIConfiguration::instance().get_use_note_bars_for_velocity());
		_track_canvas->request_redraw (_track_canvas->visible_area());
	} else if (parameter == "use-note-color-for-velocity") {
		/* handled individually by each MidiRegionView */
	}
}

Gtk::Window*
Editor::use_own_window (bool and_fill_it)
{
	bool new_window = !own_window();

	Gtk::Window* win = Tabbable::use_own_window (and_fill_it);

	if (win && new_window) {
		win->set_name ("EditorWindow");

		ARDOUR_UI::instance()->setup_toplevel_window (*win, _("Editor"), this);

		// win->signal_realize().connect (*this, &Editor::on_realize);
		win->signal_event().connect (sigc::bind (sigc::ptr_fun (&Keyboard::catch_user_event_for_pre_dialog_focus), win));
		win->signal_event().connect (sigc::mem_fun (*this, &Editor::generic_event_handler));
		win->set_data ("ardour-bindings", bindings);

		update_title ();
	}

	DisplaySuspender ds;
	contents().show_all ();

	/* XXX: this is a bit unfortunate; it would probably
	   be nicer if we could just call show () above rather
	   than needing the show_all ()
	*/

	/* re-hide stuff if necessary */
	editor_list_button_toggled ();
	parameter_changed ("show-summary");
	parameter_changed ("show-group-tabs");
	parameter_changed ("show-zoom-tools");

	/* now reset all audio_time_axis heights, because widgets might need
	   to be re-hidden
	*/

	TimeAxisView *tv;

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		tv = (static_cast<TimeAxisView*>(*i));
		tv->reset_height ();
	}

	if (current_mixer_strip) {
		current_mixer_strip->hide_things ();
		current_mixer_strip->parameter_changed ("mixer-element-visibility");
	}

	return win;
}

double
Editor::time_to_pixel (timepos_t const & pos) const
{
	return sample_to_pixel (pos.samples());
}

double
Editor::time_to_pixel_unrounded (timepos_t const & pos) const
{
	return sample_to_pixel_unrounded (pos.samples());
}

double
Editor::duration_to_pixels (timecnt_t const & dur) const
{
	return sample_to_pixel (dur.samples());
}

double
Editor::duration_to_pixels_unrounded (timecnt_t const & dur) const
{
	return sample_to_pixel_unrounded (dur.samples());
}

Temporal::TimeDomain
Editor::default_time_domain () const
{
	if (_grid_type == GridTypeNone || _snap_mode == SnapOff) {
		return AudioTime;
	}

	return BeatTime;
}
