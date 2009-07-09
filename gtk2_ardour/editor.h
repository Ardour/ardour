/*
    Copyright (C) 2000-2003 Paul Davis 

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

#ifndef __ardour_editor_h__
#define __ardour_editor_h__

#include <list>
#include <map>
#include <set>
#include <string>
#include <sys/time.h>
#include <bitset>

#include <glibmm/ustring.h>

#include <boost/optional.hpp>

#include <libgnomecanvasmm/canvas.h>
#include <libgnomecanvasmm/group.h>
#include <libgnomecanvasmm/line.h>
#include <libgnomecanvasmm/pixbuf.h>

#include <cmath>

#include <gtkmm/layout.h>
#include <gtkmm/comboboxtext.h>

#include <gtkmm2ext/selector.h>
#include <gtkmm2ext/click_box.h>
#include <gtkmm2ext/dndtreeview.h>

#include "pbd/stateful.h"
#include "ardour/session.h"
#include "ardour/tempo.h"
#include "ardour/stretch.h"
#include "ardour/location.h"
#include "ardour/audioregion.h"
#include "ardour/track.h"
#include "ardour/types.h"
#include "ardour/route_group.h"

#include "audio_clock.h"
#include "gtk-custom-ruler.h"
#include "ardour_dialog.h"
#include "public_editor.h"
#include "editing.h"
#include "enums.h"
#include "editor_items.h"
#include "region_selection.h"
#include "canvas.h"
#include "editor_summary.h"

namespace Gtkmm2ext {
	class TearOff;
}

namespace ARDOUR {
	class AudioDiskstream;
	class RouteGroup;
	class Playlist;
	class AudioPlaylist;
	class Region;
	class Location;
	class TempoSection;
	class NamedSelection;
	class Session;
	class Filter;
	class Crossfade;
	class ChanCount;
}

namespace LADSPA {
	class Plugin;
}

class AnalysisWindow;
class AudioRegionView;
class AudioStreamView;
class AudioTimeAxisView;
class AutomationLine;
class AutomationSelection;
class AutomationTimeAxisView;
class BundleManager;
class ControlPoint;
class CrossfadeView;
class Drag;
class GlobalPortMatrixWindow;
class GroupedButtons;
class Marker;
class MixerStrip;
class PlaylistSelector;
class PluginSelector;
class RhythmFerret;
class Selection;
class SoundFileOmega;
class StreamView;
class TempoLines;
class TimeAxisView;
class TimeFXDialog;
class TimeSelection;
class TrackSelection;
class EditorGroupTabs;
class EditorRoutes;
class EditorRouteGroups;
class EditorRegions;
class EditorSnapshots;

/* <CMT Additions> */
class ImageFrameView;
class ImageFrameTimeAxisView;
class ImageFrameTimeAxis;
class MarkerTimeAxis ;
class MarkerView ;
class ImageFrameSocketHandler ;
class TimeAxisViewItem ;
/* </CMT Additions> */

struct EditorCursor {
	Editor&               editor;
	ArdourCanvas::Points  points;
	ArdourCanvas::Line    canvas_item;
	nframes64_t           current_frame;
	double		  length;
	
	EditorCursor (Editor&, bool (Editor::*)(GdkEvent*,ArdourCanvas::Item*));
	~EditorCursor ();
	
	void set_position (nframes64_t);
	void set_length (double units);
	void set_y_axis (double position);

	sigc::signal<void, nframes64_t> PositionChanged;
};

class Editor : public PublicEditor
{
  public:
	Editor ();
	~Editor ();
	
	void             connect_to_session (ARDOUR::Session *);
	ARDOUR::Session* current_session() const { return session; }
	void             first_idle ();
	virtual bool     have_idled () const { return _have_idled; }

	nframes64_t leftmost_position() const { return leftmost_frame; }

	nframes64_t current_page_frames() const {
		return (nframes64_t) floor (_canvas_width * frames_per_unit);
	}
	
	double canvas_height () const {
		return _canvas_height;
	}

	void cycle_snap_mode ();
	void cycle_snap_choice ();
	void set_snap_to (Editing::SnapType);
	void set_snap_mode (Editing::SnapMode);
	void set_snap_threshold (double pixel_distance) {snap_threshold = pixel_distance;}

	void undo (uint32_t n = 1);
	void redo (uint32_t n = 1);

	XMLNode& get_state ();
	int set_state (const XMLNode& );

	void set_mouse_mode (Editing::MouseMode, bool force=true);
	void step_mouse_mode (bool next);
	Editing::MouseMode current_mouse_mode () const { return mouse_mode; }
	
	void set_midi_edit_cursor (Editing::MidiEditMode);
	void set_midi_edit_mode (Editing::MidiEditMode, bool force=true);
	Editing::MidiEditMode current_midi_edit_mode () const { return midi_edit_mode; }

#ifdef WITH_CMT
	void add_imageframe_time_axis(const std::string & track_name, void*) ;
	void add_imageframe_marker_time_axis(const std::string & track_name, TimeAxisViewPtr marked_track, void*) ;
	void connect_to_image_compositor() ;
	void scroll_timeaxis_to_imageframe_item(const TimeAxisViewItem* item) ;
#endif

	TimeAxisViewPtr get_named_time_axis(const std::string & name) ;
	void foreach_time_axis_view (sigc::slot<void,TimeAxisViewPtr>);
	void add_to_idle_resize (TimeAxisViewPtr, int32_t);

	RouteTimeAxisViewPtr get_route_view_by_id (PBD::ID& id);

	void consider_auditioning (boost::shared_ptr<ARDOUR::Region>);
	void hide_a_region (boost::shared_ptr<ARDOUR::Region>);
	void remove_a_region (boost::shared_ptr<ARDOUR::Region>);

#ifdef USE_RUBBERBAND
	std::vector<std::string> rb_opt_strings;
#endif

	/* option editor-access */

	void set_show_waveforms_recording (bool yn);
	bool show_waveforms_recording() const { return _show_waveforms_recording; }
	
	/* things that need to be public to be used in the main menubar */

	void new_region_from_selection ();
	void separate_regions_between (const TimeSelection&);
	void separate_region_from_selection ();
	void separate_region_from_punch ();
	void separate_region_from_loop ();
	void separate_regions_using_location (ARDOUR::Location&);
	void toggle_playback (bool with_abort);
	void transition_to_rolling (bool forward);

	/* undo related */

	nframes64_t unit_to_frame (double unit) const {
		return (nframes64_t) rint (unit * frames_per_unit);
	}
	
	double frame_to_unit (nframes64_t frame) const {
		return rint ((double) frame / (double) frames_per_unit);
	}

	double frame_to_unit (double frame) const {
		return rint (frame / frames_per_unit);
	}

	/* NOTE: these functions assume that the "pixel" coordinate is
	   the result of using the world->canvas affine transform on a
	   world coordinate. These coordinates already take into
	   account any scrolling carried out by adjusting the
	   xscroll_adjustment.  
	*/

	nframes64_t pixel_to_frame (double pixel) const {
		
		/* pixel can be less than zero when motion events
		   are processed. since we've already run the world->canvas
		   affine, that means that the location *really* is "off
		   to the right" and thus really is "before the start".
		*/

		if (pixel >= 0) {
			return (nframes64_t) rint (pixel * frames_per_unit * GNOME_CANVAS(track_canvas->gobj())->pixels_per_unit);
		} else {
			return 0;
		}
	}

	gulong frame_to_pixel (nframes64_t frame) const {
		return (gulong) rint ((frame / (frames_per_unit * GNOME_CANVAS(track_canvas->gobj())->pixels_per_unit)));
	}

	void flush_canvas ();

	/* selection */

	Selection& get_selection() const { return *selection; }
	Selection& get_cut_buffer() const { return *cut_buffer; }

	bool extend_selection_to_track (TimeAxisViewPtr);

	void play_selection ();
	void select_all_in_track (Selection::Operation op);
	void select_all (Selection::Operation op);
	void invert_selection_in_track ();
	void invert_selection ();
	void deselect_all ();
	
	void set_selected_regionview_from_region_list (boost::shared_ptr<ARDOUR::Region> region, Selection::Operation op = Selection::Set);

	/* tempo */

	void set_show_measures (bool yn);
	bool show_measures () const { return _show_measures; }

	/* analysis window */

	void analyze_region_selection();
	void analyze_range_selection();

	/* export */
	
	void export_audio ();
	void export_selection ();
	void export_range ();
	void export_region ();

	void add_toplevel_controls (Gtk::Container&);
	Gtk::HBox& get_status_bar_packer()  { return status_bar_hpacker; }

	void               set_zoom_focus (Editing::ZoomFocus);
	Editing::ZoomFocus get_zoom_focus () const { return zoom_focus; }
	double             get_current_zoom () const { return frames_per_unit; }

	void temporal_zoom_step (bool coarser);

	/* stuff that AudioTimeAxisView and related classes use */

	PlaylistSelector& playlist_selector() const;
	void clear_playlist (boost::shared_ptr<ARDOUR::Playlist>);

	void new_playlists (boost::weak_ptr<TimeAxisView>);
	void copy_playlists (boost::weak_ptr<TimeAxisView>);
	void clear_playlists (boost::weak_ptr<TimeAxisView>);

	TrackViewList* get_valid_views (TimeAxisViewPtr, ARDOUR::RouteGroup* grp = 0);
	void get_onscreen_tracks (TrackViewList&);

	Width editor_mixer_strip_width;
	void maybe_add_mixer_strip_width (XMLNode&);
	void show_editor_mixer (bool yn);
	void create_editor_mixer ();
	void show_editor_list (bool yn);
	void set_selected_mixer_strip (TimeAxisViewPtr);
	void hide_track_in_display (TimeAxisViewPtr tv, bool temporary = false);

	/* nudge is initiated by transport controls owned by ARDOUR_UI */

	void nudge_forward (bool next, bool force_playhead);
	void nudge_backward (bool next, bool force_playhead);

	/* nudge initiated from context menu */

	void nudge_forward_capture_offset ();
	void nudge_backward_capture_offset ();

	/* playhead/screen stuff */
	
	void set_follow_playhead (bool yn);
	void toggle_follow_playhead ();
	bool follow_playhead() const { return _follow_playhead; }
	bool dragging_playhead () const { return _dragging_playhead; }

	void toggle_zero_line_visibility ();
	void toggle_waveforms_while_recording ();
	void set_summary ();
	void set_group_tabs ();
	void toggle_measure_visibility ();
	void toggle_logo_visibility ();

	double get_physical_screen_width () const { return physical_screen_width; };
	double physical_screen_width;
	double physical_screen_height;

	/* fades/xfades */

 	void toggle_selected_region_fades (int dir);
 	void update_region_fade_visibility ();
	bool xfade_visibility() const { return _xfade_visibility; }
	void update_xfade_visibility ();

	/* redirect shared ops menu. caller must free returned menu */

	Gtk::Menu* redirect_menu ();

	/* floating windows/transient */

	void ensure_float (Gtk::Window&);

	void show_window ();

	void scroll_tracks_down_line ();
	void scroll_tracks_up_line ();

	bool new_regionviews_display_gain () { return _new_regionviews_show_envelope; }
	void prepare_for_cleanup ();
	void finish_cleanup ();

	void maximise_editing_space();
	void restore_editing_space();

	void reset_x_origin (nframes64_t);
	void reset_y_origin (double);
	void reset_zoom (double);
	void reposition_and_zoom (nframes64_t, double);

	nframes64_t get_preferred_edit_position (bool ignore_playhead = false);

	bool update_mouse_speed ();
	bool decelerate_mouse_speed ();

	void toggle_meter_updating();

	void show_rhythm_ferret();
	void show_bundle_manager ();
	void show_global_port_matrix (ARDOUR::DataType);

	void goto_visual_state (uint32_t);
	void save_visual_state (uint32_t);

	TrackViewList const & get_track_views () {
		return track_views;
	}

	int get_regionview_count_from_region_list (boost::shared_ptr<ARDOUR::Region>);

	void do_import (std::vector<Glib::ustring> paths, Editing::ImportDisposition, Editing::ImportMode mode, ARDOUR::SrcQuality, nframes64_t&);
	void do_embed (std::vector<Glib::ustring> paths, Editing::ImportDisposition, Editing::ImportMode mode,  nframes64_t&);

	void get_regions_corresponding_to (boost::shared_ptr<ARDOUR::Region> region, std::vector<RegionView*>& regions);
	
  protected:
	void map_transport_state ();
	void map_position_change (nframes64_t);

	void on_realize();

  private:

	void color_handler ();
	
	ARDOUR::Session     *session; ///< The session that we are editing, or 0
	bool                 constructed;

	// to keep track of the playhead position for control_scroll
	boost::optional<nframes64_t> _control_scroll_target;
 
	PlaylistSelector* _playlist_selector;

	typedef std::pair<TimeAxisViewPtr,XMLNode*> TAVState;

	struct VisualState {
	    double              y_position;
	    double              frames_per_unit;
	    nframes64_t         leftmost_frame;
	    Editing::ZoomFocus  zoom_focus;
	    bool                zoomed_to_region;
	    std::list<TAVState> track_states;
	};
	
	std::list<VisualState*> undo_visual_stack;
	std::list<VisualState*> redo_visual_stack;
	VisualState* current_visual_state (bool with_tracks = true);
	void undo_visual_state ();
	void redo_visual_state ();
	void use_visual_state (VisualState&);
	bool no_save_visual;
	void swap_visual_state ();
	
	std::vector<VisualState*> visual_states;
	sigc::connection visual_state_op_connection;
	void start_visual_state_op (uint32_t n);
	void cancel_visual_state_op (uint32_t n);
	bool end_visual_state_op (uint32_t n);

	nframes64_t leftmost_frame;
	double      frames_per_unit;
	Editing::ZoomFocus zoom_focus;

	void set_frames_per_unit (double);
	void post_zoom ();

	Editing::MouseMode mouse_mode;
	Editing::MidiEditMode midi_edit_mode;

	int  post_maximal_editor_width;
	int  post_maximal_pane_position;
	int  pre_maximal_pane_position;
	int  pre_maximal_editor_width;
	void pane_allocation_handler (Gtk::Allocation&, Gtk::Paned*);

	Gtk::Notebook the_notebook;
	Gtk::HPaned   edit_pane;

	Gtk::EventBox meter_base;
	Gtk::HBox     meter_box;
	Gtk::EventBox marker_base;
	Gtk::HBox     marker_box;
	Gtk::VBox     scrollers_rulers_markers_box;

	void location_changed (ARDOUR::Location *);
	void location_flags_changed (ARDOUR::Location *, void *);
	void refresh_location_display ();
	void refresh_location_display_s (ARDOUR::Change);
	void refresh_location_display_internal (ARDOUR::Locations::LocationList&);
	void add_new_location (ARDOUR::Location *);
	void location_gone (ARDOUR::Location *);
	void remove_marker (ArdourCanvas::Item&, GdkEvent*);
	gint really_remove_marker (ARDOUR::Location* loc);
	void goto_nth_marker (int nth);

	uint32_t location_marker_color;
	uint32_t location_range_color;
	uint32_t location_loop_color;
	uint32_t location_punch_color;
	uint32_t location_cd_marker_color;

	struct LocationMarkers {
	    Marker* start;
	    Marker* end;
	    bool    valid;

	    LocationMarkers () : start(0), end(0), valid (true) {}
	    
	    ~LocationMarkers ();

	    void hide();
	    void show ();
	    void set_name (const std::string&);
	    void set_position (nframes64_t start, nframes64_t end = 0);
	    void set_color_rgba (uint32_t);
	};

	LocationMarkers  *find_location_markers (ARDOUR::Location *) const;
	ARDOUR::Location* find_location_from_marker (Marker *, bool& is_start) const;
	Marker* entered_marker;

	typedef std::map<ARDOUR::Location*,LocationMarkers *> LocationMarkerMap;
	LocationMarkerMap location_markers;

	void hide_marker (ArdourCanvas::Item*, GdkEvent*);
	void clear_marker_display ();
	void mouse_add_new_marker (nframes64_t where, bool is_cd=false, bool is_xrun=false);
	bool choose_new_marker_name(std::string &name);
	void update_cd_marker_display ();
	void ensure_cd_marker_updated (LocationMarkers * lam, ARDOUR::Location * location);

	TimeAxisViewPtr      clicked_axisview;
	RouteTimeAxisViewPtr clicked_routeview;
	/** The last RegionView that was clicked on, or 0 if the last click was not
	 * on a RegionView.  This is set up by the canvas event handlers in
	 * editor_canvas_events.cc
	 */
	RegionView*        clicked_regionview;
	RegionSelection    latest_regionviews;
	uint32_t           clicked_selection;
	CrossfadeView*     clicked_crossfadeview;
	ControlPoint*      clicked_control_point;

	void sort_track_selection (TrackSelection* sel = 0);

	void get_relevant_tracks (std::set<RouteTimeAxisViewPtr>& relevant_tracks) const;
	void get_equivalent_tracks (RouteTimeAxisViewPtr, std::set<RouteTimeAxisViewPtr> &, ARDOUR::RouteGroup::Property) const;
	void get_equivalent_regions (RegionView* rv, std::vector<RegionView*> &, ARDOUR::RouteGroup::Property) const;
	RegionSelection get_equivalent_regions (RegionSelection &, ARDOUR::RouteGroup::Property) const;
	void mapover_tracks (sigc::slot<void,RouteTimeAxisViewPtr,uint32_t> sl, TimeAxisViewPtr, ARDOUR::RouteGroup::Property) const;

	/* functions to be passed to mapover_tracks(), possibly with sigc::bind()-supplied arguments */

	void mapped_get_equivalent_regions (RouteTimeAxisViewPtr, uint32_t, RegionView *, std::vector<RegionView*>*) const;
	void mapped_use_new_playlist (RouteTimeAxisViewPtr, uint32_t, std::vector<boost::shared_ptr<ARDOUR::Playlist> > const &);
	void mapped_use_copy_playlist (RouteTimeAxisViewPtr, uint32_t, std::vector<boost::shared_ptr<ARDOUR::Playlist> > const &);
	void mapped_clear_playlist (RouteTimeAxisViewPtr, uint32_t);

	/* end */

	void button_selection (ArdourCanvas::Item* item, GdkEvent* event, ItemType item_type);
	bool button_release_can_deselect;

	void catch_vanishing_regionview (RegionView *);

	void set_selected_track (TimeAxisViewPtr, Selection::Operation op = Selection::Set, bool no_remove=false);
	void select_all_tracks ();
	
	bool set_selected_control_point_from_click (Selection::Operation op = Selection::Set, bool no_remove=false);
	void set_selected_track_from_click (bool press, Selection::Operation op = Selection::Set, bool no_remove=false);
	void set_selected_track_as_side_effect (bool force = false);
	bool set_selected_regionview_from_click (bool press, Selection::Operation op = Selection::Set, bool no_track_remove=false);

	bool set_selected_regionview_from_map_event (GdkEventAny*, StreamView*, boost::weak_ptr<ARDOUR::Region>);
	void collect_new_region_view (RegionView *);
	void collect_and_select_new_region_view (RegionView *);
	
	Gtk::Menu track_context_menu;
	Gtk::Menu track_region_context_menu;
	Gtk::Menu track_selection_context_menu;
	Gtk::Menu track_crossfade_context_menu;

	Gtk::MenuItem* region_edit_menu_split_item;
	Gtk::MenuItem* region_edit_menu_split_multichannel_item;
	Gtk::Menu * track_region_edit_playlist_menu;
	Gtk::Menu * track_edit_playlist_submenu;
	Gtk::Menu * track_selection_edit_playlist_submenu;

	void popup_track_context_menu (int, int, ItemType, bool, nframes64_t);
	Gtk::Menu* build_track_context_menu (nframes64_t);
	Gtk::Menu* build_track_bus_context_menu (nframes64_t);
	Gtk::Menu* build_track_region_context_menu (nframes64_t frame);
	Gtk::Menu* build_track_crossfade_context_menu (nframes64_t);
	Gtk::Menu* build_track_selection_context_menu (nframes64_t);
	void add_dstream_context_items (Gtk::Menu_Helpers::MenuList&);
	void add_bus_context_items (Gtk::Menu_Helpers::MenuList&);
	void add_region_context_items (StreamView*, boost::shared_ptr<ARDOUR::Region>, Gtk::Menu_Helpers::MenuList&);
	void add_crossfade_context_items (AudioStreamView*, boost::shared_ptr<ARDOUR::Crossfade>, Gtk::Menu_Helpers::MenuList&, bool many);
	void add_selection_context_items (Gtk::Menu_Helpers::MenuList&);

	void handle_new_route (ARDOUR::RouteList&);
	void remove_route (TimeAxisViewPtr);
	bool route_removal;

	Gtk::HBox           global_hpacker;
	Gtk::VBox           global_vpacker;
	Gtk::VBox           vpacker;

	Gdk::Cursor*          current_canvas_cursor;
	void set_canvas_cursor ();
	Gdk::Cursor* which_grabber_cursor ();

	ArdourCanvas::Canvas* track_canvas;

	ArdourCanvas::Text* verbose_canvas_cursor;
	bool                 verbose_cursor_visible;

	void parameter_changed (std::string);
	
	bool track_canvas_motion (GdkEvent*);

	void set_verbose_canvas_cursor (const std::string &, double x, double y);
	void set_verbose_canvas_cursor_text (const std::string &);
	void show_verbose_canvas_cursor();
	void hide_verbose_canvas_cursor();

	bool verbose_cursor_on; // so far unused

	Gtk::EventBox             time_canvas_event_box;
	Gtk::EventBox             track_canvas_event_box;
	Gtk::EventBox             time_button_event_box;
	Gtk::EventBox             ruler_label_event_box;

	ArdourCanvas::Group      *minsec_group;
	ArdourCanvas::Pixbuf     *logo_item;
	ArdourCanvas::Group      *bbt_group;
	ArdourCanvas::Group      *smpte_group;
	ArdourCanvas::Group      *frame_group;
	ArdourCanvas::Group      *tempo_group;
	ArdourCanvas::Group      *meter_group;
	ArdourCanvas::Group      *marker_group;
	ArdourCanvas::Group      *range_marker_group;
	ArdourCanvas::Group      *transport_marker_group;
	ArdourCanvas::Group*      cd_marker_group;

	ArdourCanvas::Group*      timebar_group;

	/* These bars never need to be scrolled */
	ArdourCanvas::Group*      meter_bar_group;
	ArdourCanvas::Group*      tempo_bar_group;
	ArdourCanvas::Group*      marker_bar_group;
	ArdourCanvas::Group*      range_marker_bar_group;
	ArdourCanvas::Group*      transport_marker_bar_group;
	ArdourCanvas::Group*      cd_marker_bar_group;

	/** The group containing all items that require horizontal scrolling. */
	ArdourCanvas::Group* _background_group;
	/* 
	   The _master_group is the group containing all items
	   that require horizontal scrolling..
	   It is primarily used to separate canvas items 
	   that require horizontal scrolling from those that do not. 
	*/
	ArdourCanvas::Group* _master_group;

	/* The group containing all trackviews.  Only scrolled vertically. */
	ArdourCanvas::Group* _trackview_group;
	
	/* The group used for region motion.  Sits on top of _trackview_group */
	ArdourCanvas::Group* _region_motion_group;
	
	enum RulerType {
		ruler_metric_smpte = 0,
		ruler_metric_bbt = 1,
		ruler_metric_frames = 2,
		ruler_metric_minsec = 3,

		ruler_time_tempo = 4,
		ruler_time_meter = 5,
		ruler_time_marker = 6,
		ruler_time_range_marker = 7,
		ruler_time_transport_marker = 8,
		ruler_time_cd_marker = 9,
	};

	static GtkCustomMetric ruler_metrics[4];
	Glib::RefPtr<Gtk::ToggleAction> ruler_timecode_action;
	Glib::RefPtr<Gtk::ToggleAction> ruler_bbt_action;
	Glib::RefPtr<Gtk::ToggleAction> ruler_samples_action;
	Glib::RefPtr<Gtk::ToggleAction> ruler_minsec_action;
	Glib::RefPtr<Gtk::ToggleAction> ruler_tempo_action;
	Glib::RefPtr<Gtk::ToggleAction> ruler_meter_action;
	Glib::RefPtr<Gtk::ToggleAction> ruler_marker_action;
	Glib::RefPtr<Gtk::ToggleAction> ruler_range_action;
	Glib::RefPtr<Gtk::ToggleAction> ruler_loop_punch_action;
	Glib::RefPtr<Gtk::ToggleAction> ruler_cd_marker_action;
	bool                            no_ruler_shown_update;
	
	gint ruler_button_press (GdkEventButton*);
	gint ruler_button_release (GdkEventButton*);
	gint ruler_mouse_motion (GdkEventMotion*);
	bool ruler_scroll (GdkEventScroll* event);

	Gtk::Widget * ruler_grabbed_widget;
	
	void initialize_rulers ();
	void update_just_smpte ();
	void compute_fixed_ruler_scale (); //calculates the RulerScale of the fixed rulers
	void update_fixed_rulers ();
	void update_tempo_based_rulers (); 
	void popup_ruler_menu (nframes64_t where = 0, ItemType type = RegionItem);
	void update_ruler_visibility ();
	void set_ruler_visible (RulerType, bool);
	void toggle_ruler_visibility (RulerType rt);
	void ruler_toggled (int);
	gint ruler_label_button_release (GdkEventButton*);
	void store_ruler_visibility ();
	void restore_ruler_visibility ();
	
	static gint _metric_get_smpte (GtkCustomRulerMark **, gdouble, gdouble, gint);
	static gint _metric_get_bbt (GtkCustomRulerMark **, gdouble, gdouble, gint);
	static gint _metric_get_frames (GtkCustomRulerMark **, gdouble, gdouble, gint);
	static gint _metric_get_minsec (GtkCustomRulerMark **, gdouble, gdouble, gint);
	
	enum MinsecRulerScale {
		minsec_show_seconds,
		minsec_show_minutes,
		minsec_show_hours,
		minsec_show_frames
	};

	MinsecRulerScale minsec_ruler_scale;

	nframes_t minsec_mark_interval;
	gint minsec_mark_modulo;
	gint minsec_nmarks;
	void set_minsec_ruler_scale (gdouble lower, gdouble upper);

	enum SMPTERulerScale {
		smpte_show_bits,
		smpte_show_frames,
		smpte_show_seconds,
		smpte_show_minutes,
		smpte_show_hours
	};

	SMPTERulerScale smpte_ruler_scale;

	nframes_t smpte_mark_interval;
	gint smpte_mark_modulo;
	gint smpte_nmarks;
	void set_smpte_ruler_scale (gdouble lower, gdouble upper);

	enum BBTRulerScale {
		bbt_over,
		bbt_show_64,
		bbt_show_16,
		bbt_show_4,
		bbt_show_1,
		bbt_show_beats,
		bbt_show_ticks,
		bbt_show_ticks_detail,
		bbt_show_ticks_super_detail
	};

	BBTRulerScale bbt_ruler_scale;

        uint32_t bbt_bars;
	gint bbt_nmarks;
	uint32_t bbt_bar_helper_on;
	uint32_t bbt_accent_modulo;
	void compute_bbt_ruler_scale (nframes64_t lower, nframes64_t upper);

	gint metric_get_smpte (GtkCustomRulerMark **, gdouble, gdouble, gint);
	gint metric_get_bbt (GtkCustomRulerMark **, gdouble, gdouble, gint);
	gint metric_get_frames (GtkCustomRulerMark **, gdouble, gdouble, gint);
	gint metric_get_minsec (GtkCustomRulerMark **, gdouble, gdouble, gint);

	Gtk::Widget        *_ruler_separator;
	GtkWidget          *_smpte_ruler;
	GtkWidget          *_bbt_ruler;
	GtkWidget          *_frames_ruler;
	GtkWidget          *_minsec_ruler;
	Gtk::Widget        *smpte_ruler;
	Gtk::Widget        *bbt_ruler;
	Gtk::Widget        *frames_ruler;
	Gtk::Widget        *minsec_ruler;
	static Editor      *ruler_editor;

	static const double timebar_height;
	guint32 visible_timebars;
	gdouble canvas_timebars_vsize;
	gdouble get_canvas_timebars_vsize () const { return canvas_timebars_vsize; }
	Gtk::Menu          *editor_ruler_menu;
	
	ArdourCanvas::SimpleRect* tempo_bar;
	ArdourCanvas::SimpleRect* meter_bar;
	ArdourCanvas::SimpleRect* marker_bar;
	ArdourCanvas::SimpleRect* range_marker_bar;
	ArdourCanvas::SimpleRect* transport_marker_bar;
	ArdourCanvas::SimpleRect* cd_marker_bar;
	
	Gtk::Label  minsec_label;
	Gtk::Label  bbt_label;
	Gtk::Label  smpte_label;
	Gtk::Label  frame_label;
	Gtk::Label  tempo_label;
	Gtk::Label  meter_label;
	Gtk::Label  mark_label;
	Gtk::Label  range_mark_label;
	Gtk::Label  transport_mark_label;
	Gtk::Label  cd_mark_label;

	Gtk::VBox          time_button_vbox;
	Gtk::HBox          time_button_hbox;

	friend class EditorCursor;
	
	EditorCursor*        playhead_cursor;
	ArdourCanvas::Group* cursor_group;

	nframes64_t get_region_boundary (nframes64_t pos, int32_t dir, bool with_selection, bool only_onscreen);

	void    cursor_to_region_boundary (bool with_selection, int32_t dir);
	void    cursor_to_next_region_boundary (bool with_selection);
	void    cursor_to_previous_region_boundary (bool with_selection);
	void    cursor_to_next_region_point (EditorCursor*, ARDOUR::RegionPoint);
	void    cursor_to_previous_region_point (EditorCursor*, ARDOUR::RegionPoint);
	void    cursor_to_region_point (EditorCursor*, ARDOUR::RegionPoint, int32_t dir);
	void    cursor_to_selection_start (EditorCursor *);
	void    cursor_to_selection_end   (EditorCursor *);

	void    selected_marker_to_region_boundary (bool with_selection, int32_t dir);
	void    selected_marker_to_next_region_boundary (bool with_selection);
	void    selected_marker_to_previous_region_boundary (bool with_selection);
	void    selected_marker_to_next_region_point (ARDOUR::RegionPoint);
	void    selected_marker_to_previous_region_point (ARDOUR::RegionPoint);
	void    selected_marker_to_region_point (ARDOUR::RegionPoint, int32_t dir);
	void    selected_marker_to_selection_start ();
	void    selected_marker_to_selection_end   ();

	void    select_all_selectables_using_cursor (EditorCursor *, bool);
	void    select_all_selectables_using_edit (bool);
	void    select_all_selectables_between (bool within);
	void    select_range_between ();

	boost::shared_ptr<ARDOUR::Region> find_next_region (nframes64_t, ARDOUR::RegionPoint, int32_t dir, TrackViewList&, TimeAxisViewPtr * = 0);
	nframes64_t find_next_region_boundary (nframes64_t, int32_t dir, const TrackViewList&);

	std::vector<nframes64_t> region_boundary_cache;
	void build_region_boundary_cache ();

	Gtk::HBox           top_hbox;
	Gtk::HBox           bottom_hbox;
	
	Gtk::Table          edit_packer;
	Gtk::VScrollbar     edit_vscrollbar;

	Gtk::Adjustment     vertical_adjustment;
	Gtk::Adjustment     horizontal_adjustment;

	Gtk::Layout         controls_layout;
	bool control_layout_scroll (GdkEventScroll* ev);
	void controls_layout_size_request (Gtk::Requisition*);
	sigc::connection controls_layout_size_request_connection;

	Gtk::HScrollbar     edit_hscrollbar;
	bool                _dragging_hscrollbar;

	void reset_hscrollbar_stepping ();
	
	bool hscrollbar_button_press (GdkEventButton*);
	bool hscrollbar_button_release (GdkEventButton*);
	void hscrollbar_allocate (Gtk::Allocation &alloc);

	double _canvas_width;
	double _canvas_height;
	double full_canvas_height;

	bool track_canvas_map_handler (GdkEventAny*);

	gint edit_controls_button_release (GdkEventButton*);
	Gtk::Menu *edit_controls_left_menu;
	Gtk::Menu *edit_controls_right_menu;

	Gtk::VBox           ruler_label_vbox;
	Gtk::VBox           track_canvas_vbox;
	Gtk::VBox           time_canvas_vbox;
	Gtk::VBox           edit_controls_vbox;
	Gtk::HBox           edit_controls_hbox;

	void control_scroll (float);
	void access_action (std::string,std::string);
	bool deferred_control_scroll (nframes64_t);
	sigc::connection control_scroll_connection;

	gdouble get_trackview_group_vertical_offset () const { return vertical_adjustment.get_value () - canvas_timebars_vsize;}
	
	ArdourCanvas::Group* get_background_group () const { return _background_group; }
	ArdourCanvas::Group* get_trackview_group () const { return _trackview_group; }
	double last_trackview_group_vertical_offset;
	void tie_vertical_scrolling ();
	void scroll_canvas_horizontally ();
	void scroll_canvas_vertically ();

	struct VisualChange {
		enum Type { 
			TimeOrigin = 0x1,
			ZoomLevel = 0x2,
			YOrigin = 0x4
		};
		
		Type pending;
		nframes64_t time_origin;
		double frames_per_unit;
		double y_origin;
		
		int idle_handler_id;
		
		VisualChange() : pending ((VisualChange::Type) 0), time_origin (0), frames_per_unit (0), idle_handler_id (-1) {}
		void add (Type t) {
			pending = Type (pending | t);
		}
	};


	VisualChange pending_visual_change;

	static int _idle_visual_changer (void *arg);
	int idle_visual_changer ();

	void queue_visual_change (nframes64_t);
	void queue_visual_change (double);
	void queue_visual_change_y (double);
	void ensure_visual_change_idle_handler ();

	void end_location_changed (ARDOUR::Location*);

	/* named selections */

	struct NamedSelectionDisplayModelColumns : public Gtk::TreeModel::ColumnRecord {
	    NamedSelectionDisplayModelColumns() { 
		    add (text);
		    add (selection);
	    }
	    Gtk::TreeModelColumn<Glib::ustring>  text;
	    Gtk::TreeModelColumn<ARDOUR::NamedSelection*>    selection;
	};

	NamedSelectionDisplayModelColumns named_selection_columns;
	Glib::RefPtr<Gtk::TreeStore>     named_selection_model;

	Gtkmm2ext::DnDTreeView<ARDOUR::NamedSelection*> named_selection_display;
	Gtk::ScrolledWindow    named_selection_scroller;

	void create_named_selection ();
	void paste_named_selection (float times);
	void remove_selected_named_selections ();

	void handle_new_named_selection ();
	void add_named_selection_to_named_selection_display (ARDOUR::NamedSelection&);
	void redisplay_named_selections ();

	bool named_selection_display_button_release (GdkEventButton *ev);
	bool named_selection_display_key_release (GdkEventKey *ev);
	void named_selection_display_selection_changed ();

	/* track views */
	TrackViewList track_views;
	std::pair<TimeAxisViewPtr, ARDOUR::layer_t> trackview_by_y_position (double);
	TrackSelection axis_views_from_routes (std::list<ARDOUR::Route *>) const;

	static Gdk::Cursor* cross_hair_cursor;
	static Gdk::Cursor* trimmer_cursor;
	static Gdk::Cursor* selector_cursor;
	static Gdk::Cursor* grabber_cursor;
	static Gdk::Cursor* grabber_edit_point_cursor;
	static Gdk::Cursor* zoom_cursor;
	static Gdk::Cursor* time_fx_cursor;
	static Gdk::Cursor* fader_cursor;
	static Gdk::Cursor* speaker_cursor;
	static Gdk::Cursor* midi_pencil_cursor;
	static Gdk::Cursor* midi_select_cursor;
	static Gdk::Cursor* midi_resize_cursor;
	static Gdk::Cursor* midi_erase_cursor;
	static Gdk::Cursor* wait_cursor;
	static Gdk::Cursor* timebar_cursor;
	static Gdk::Cursor* transparent_cursor;

	static void build_cursors ();

	sigc::connection scroll_connection;
	nframes64_t last_update_frame;
	void center_screen (nframes64_t);
	void center_screen_internal (nframes64_t, float);
	
	void update_current_screen ();
	
	void session_going_away ();

	nframes64_t cut_buffer_start;
	nframes64_t cut_buffer_length;

	bool typed_event (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool button_press_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool button_release_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool motion_handler (ArdourCanvas::Item*, GdkEvent*, bool from_autoscroll = false);
	bool enter_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool leave_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);
	
	/* KEYMAP HANDLING */

	void register_actions ();

	int ensure_cursor (nframes64_t* pos);

	void cut_copy (Editing::CutCopyOp);
	bool can_cut_copy () const;
	void cut_copy_points (Editing::CutCopyOp);
	void cut_copy_regions (Editing::CutCopyOp, RegionSelection&);
	void cut_copy_ranges (Editing::CutCopyOp);

	void mouse_paste ();
	void paste_internal (nframes64_t position, float times);

	/* EDITING OPERATIONS */
	
	void reset_point_selection ();
	void toggle_region_mute ();
	void toggle_region_lock ();
	void toggle_region_opaque ();
	void toggle_record_enable ();
	void set_region_lock_style (ARDOUR::Region::PositionLockStyle);
	void raise_region ();
	void raise_region_to_top ();
	void lower_region ();
	void lower_region_to_bottom ();
	void split_region ();
	void split_regions_at (nframes64_t, RegionSelection&);
	void split_region_at_transients ();
	void split_region_at_points (boost::shared_ptr<ARDOUR::Region>, ARDOUR::AnalysisFeatureList&, bool can_ferret);
	void crop_region_to_selection ();
	void crop_region_to (nframes64_t start, nframes64_t end);
	void set_sync_point (nframes64_t, const RegionSelection&);
	void set_region_sync_from_edit_point ();
	void remove_region_sync();
	void align_selection (ARDOUR::RegionPoint, nframes64_t position, const RegionSelection&);
	void align_selection_relative (ARDOUR::RegionPoint point, nframes64_t position, const RegionSelection&);
	void align_region (boost::shared_ptr<ARDOUR::Region>, ARDOUR::RegionPoint point, nframes64_t position);
	void align_region_internal (boost::shared_ptr<ARDOUR::Region>, ARDOUR::RegionPoint point, nframes64_t position);
	void remove_selected_regions ();
	void remove_clicked_region ();
	void edit_region ();
	void rename_region ();
	void duplicate_some_regions (RegionSelection&, float times);
	void duplicate_selection (float times);
	void region_fill_selection ();

	void region_fill_track ();
	void audition_playlist_region_standalone (boost::shared_ptr<ARDOUR::Region>);
	void audition_playlist_region_via_route (boost::shared_ptr<ARDOUR::Region>, ARDOUR::Route&);
	void split_multichannel_region();
	void reverse_region ();
	void strip_region_silence ();
	void normalize_region ();
	double _last_normalization_value;
	void denormalize_region ();
	void adjust_region_scale_amplitude (bool up);
	void quantize_region ();

	void do_insert_time ();
	void insert_time (nframes64_t, nframes64_t, Editing::InsertTimeOption, bool, bool, bool);

	void tab_to_transient (bool forward);

	void use_region_as_bar ();
	void use_range_as_bar ();

	void define_one_bar (nframes64_t start, nframes64_t end);

	void audition_region_from_region_list ();
	void hide_region_from_region_list ();

	void align (ARDOUR::RegionPoint);
	void align_relative (ARDOUR::RegionPoint);
	void naturalize ();

	void reset_focus ();

	void split ();

	void cut ();
	void copy ();
	void paste (float times);

	int  get_prefix (float&, bool&);

	void keyboard_paste ();
	void keyboard_insert_region_list_selection ();

	void region_from_selection ();
	void create_region_from_selection (std::vector<boost::shared_ptr<ARDOUR::Region> >&);

	void play_from_start ();
	void play_from_edit_point ();
	void play_from_edit_point_and_return ();
	void play_selected_region ();
	void play_edit_range ();
	void loop_selected_region ();
	void play_location (ARDOUR::Location&);
	void loop_location (ARDOUR::Location&);

	void temporal_zoom_selection ();
	void temporal_zoom_region (bool both_axes);
	void toggle_zoom_region (bool both_axes);
	bool zoomed_to_region;
	void temporal_zoom_session ();
	void temporal_zoom (gdouble scale);
	void temporal_zoom_by_frame (nframes64_t start, nframes64_t end, const std::string & op);
	void temporal_zoom_to_frame (bool coarser, nframes64_t frame);

	void amplitude_zoom (gdouble scale);
	void amplitude_zoom_step (bool in);

	void insert_region_list_drag (boost::shared_ptr<ARDOUR::Region>, int x, int y);
	void insert_region_list_selection (float times);

	void insert_route_list_drag (boost::shared_ptr<ARDOUR::Route>, int x, int y);

	/* import & embed */
	
	void add_external_audio_action (Editing::ImportMode);
	void external_audio_dialog ();
	void session_import_dialog ();

	int  check_whether_and_how_to_import(std::string, bool all_or_nothing = true);
	bool check_multichannel_status (const std::vector<Glib::ustring>& paths);

	SoundFileOmega* sfbrowser;
	
	void bring_in_external_audio (Editing::ImportMode mode,  nframes64_t& pos);

	bool  idle_drop_paths  (std::vector<Glib::ustring> paths, nframes64_t frame, double ypos);
	void  drop_paths_part_two  (const std::vector<Glib::ustring>& paths, nframes64_t frame, double ypos);
	
	int  import_sndfiles (std::vector<Glib::ustring> paths, Editing::ImportMode mode,  ARDOUR::SrcQuality, nframes64_t& pos,
			      int target_regions, int target_tracks, boost::shared_ptr<ARDOUR::Track>, bool, uint32_t total);
	int  embed_sndfiles (std::vector<Glib::ustring> paths, bool multiple_files, bool& check_sample_rate, Editing::ImportMode mode, 
			     nframes64_t& pos, int target_regions, int target_tracks, boost::shared_ptr<ARDOUR::Track>&);

	int add_sources (std::vector<Glib::ustring> paths, ARDOUR::SourceList& sources, nframes64_t& pos, Editing::ImportMode,
			 int target_regions, int target_tracks, boost::shared_ptr<ARDOUR::Track>&, bool add_channel_suffix);
	int finish_bringing_in_material (boost::shared_ptr<ARDOUR::Region> region, uint32_t, uint32_t,  nframes64_t& pos, Editing::ImportMode mode,
				      boost::shared_ptr<ARDOUR::Track>& existing_track);

	boost::shared_ptr<ARDOUR::AudioTrack> get_nth_selected_audio_track (int nth) const;
	boost::shared_ptr<ARDOUR::MidiTrack> get_nth_selected_midi_track (int nth) const;

	/* generic interthread progress window */
	
	ArdourDialog* interthread_progress_window;
	Gtk::Label interthread_progress_label;
	Gtk::VBox interthread_progress_vbox;
	Gtk::ProgressBar interthread_progress_bar;
	Gtk::Button interthread_cancel_button;
	Gtk::Label interthread_cancel_label;
	sigc::connection  interthread_progress_connection;
	void interthread_cancel_clicked ();
	void build_interthread_progress_window ();
	ARDOUR::InterThreadInfo* current_interthread_info;

	AnalysisWindow* analysis_window;

	/* import specific info */

	struct EditorImportStatus : public ARDOUR::Session::ImportStatus {
	    Editing::ImportMode mode;
	    nframes64_t pos;
	    int target_tracks;
	    int target_regions;
	    boost::shared_ptr<ARDOUR::Track> track;
	    bool replace;
	};

	EditorImportStatus import_status;
	gint import_progress_timeout (void *);
	static void *_import_thread (void *);
	void* import_thread ();
	void finish_import ();

	/* to support this ... */

	void import_audio (bool as_tracks);
	void do_import (std::vector<Glib::ustring> paths, bool split, bool as_tracks);

	void move_to_start ();
	void move_to_end ();
	void goto_frame ();
	void center_playhead ();
	void center_edit_point ();
	void edit_cursor_backward ();
	void edit_cursor_forward ();
	void playhead_forward_to_grid ();
	void playhead_backward_to_grid ();
	void playhead_backward ();
	void playhead_forward ();
	void scroll_playhead (bool forward);
	void scroll_backward (float pages=0.8f);
	void scroll_forward (float pages=0.8f);
	void scroll_tracks_down ();
	void scroll_tracks_up ();
	void delete_sample_forward ();
	void delete_sample_backward ();
	void delete_screen ();
	void search_backwards ();
	void search_forwards ();
	void set_mark ();
	void clear_markers ();
	void clear_ranges ();
	void clear_locations ();
	void unhide_markers ();
	void unhide_ranges ();
	void jump_forward_to_mark ();
	void jump_backward_to_mark ();
	void cursor_align (bool playhead_to_edit);

	void remove_last_capture ();
	void select_all_selectables_using_time_selection ();
	void select_all_selectables_using_loop();
	void select_all_selectables_using_punch();
	void set_selection_from_range (ARDOUR::Location&);
	void set_selection_from_punch ();
	void set_selection_from_loop ();
	void set_selection_from_region ();

	void add_location_mark (nframes64_t where);
	void add_location_from_audio_region ();
	void add_locations_from_audio_region ();
	void add_location_from_selection ();
	void set_loop_from_selection (bool play);
	void set_punch_from_selection ();
	void set_punch_from_region ();

	void set_loop_from_edit_range (bool play);
	void set_loop_from_region (bool play);
	void set_punch_from_edit_range ();

	void set_loop_range (nframes64_t start, nframes64_t end, std::string cmd);
	void set_punch_range (nframes64_t start, nframes64_t end, std::string cmd);

	void add_location_from_playhead_cursor ();
	bool select_new_marker;

	void reverse_selection ();
	void edit_envelope ();

	void start_scrolling ();
	void stop_scrolling ();

	double last_scrub_x;
	int scrubbing_direction;
	int scrub_reversals;
	int scrub_reverse_distance;
	void scrub ();

	void keyboard_selection_begin ();
	void keyboard_selection_finish (bool add);
	bool have_pending_keyboard_selection;
	nframes64_t pending_keyboard_selection_start;

	boost::shared_ptr<ARDOUR::Region> select_region_for_operation (int dir, TimeAxisViewPtr *tv);
	void extend_selection_to_end_of_region (bool next);
	void extend_selection_to_start_of_region (bool previous);

	Editing::SnapType snap_type;
	Editing::SnapMode snap_mode;

	/// Snap threshold in pixels
	double snap_threshold;

	bool ignore_gui_changes;

	Drag* _drag;

	void break_drag ();

	Gtk::Menu fade_context_menu;
	void popup_fade_context_menu (int, int, ArdourCanvas::Item*, ItemType);

	void set_fade_in_shape (ARDOUR::AudioRegion::FadeShape);
	void set_fade_out_shape (ARDOUR::AudioRegion::FadeShape);
	
	void set_fade_length (bool in);
	void toggle_fade_active (bool in);
	void set_fade_in_active (bool);
	void set_fade_out_active (bool);
	
	std::set<boost::shared_ptr<ARDOUR::Playlist> > motion_frozen_playlists;
	RegionSelection pre_drag_region_selection;

	bool _dragging_playhead;
	bool _dragging_edit_point;

	void marker_drag_motion_callback (GdkEvent*);
	void marker_drag_finished_callback (GdkEvent*);

	gint mouse_rename_region (ArdourCanvas::Item*, GdkEvent*);

	void start_region_grab (ArdourCanvas::Item*, GdkEvent*, RegionView*);
	void start_create_region_grab (ArdourCanvas::Item*, GdkEvent*);
	void start_region_copy_grab (ArdourCanvas::Item*, GdkEvent*, RegionView*);
	void start_region_brush_grab (ArdourCanvas::Item*, GdkEvent*, RegionView*);
	void start_selection_grab (ArdourCanvas::Item*, GdkEvent*);

	void region_view_item_click (AudioRegionView&, GdkEventButton*);

	void remove_gain_control_point (ArdourCanvas::Item*, GdkEvent*);
	void remove_control_point (ArdourCanvas::Item*, GdkEvent*);

	void mouse_brush_insert_region (RegionView*, nframes64_t pos);
	void brush (nframes64_t);

	void show_verbose_time_cursor (nframes64_t frame, double offset = 0, double xpos=-1, double ypos=-1);
	void show_verbose_duration_cursor (nframes64_t start, nframes64_t end, double offset = 0, double xpos=-1, double ypos=-1);
	double clamp_verbose_cursor_x (double);
	double clamp_verbose_cursor_y (double);

	/* Canvas event handlers */

	bool canvas_control_point_event (GdkEvent* event,ArdourCanvas::Item*, ControlPoint*);
	bool canvas_line_event (GdkEvent* event,ArdourCanvas::Item*, AutomationLine*);
	bool canvas_selection_rect_event (GdkEvent* event,ArdourCanvas::Item*, SelectionRect*);
	bool canvas_selection_start_trim_event (GdkEvent* event,ArdourCanvas::Item*, SelectionRect*);
	bool canvas_selection_end_trim_event (GdkEvent* event,ArdourCanvas::Item*, SelectionRect*);
	bool canvas_crossfade_view_event (GdkEvent* event,ArdourCanvas::Item*, CrossfadeView*);
	bool canvas_fade_in_event (GdkEvent* event,ArdourCanvas::Item*, AudioRegionView*);
	bool canvas_fade_in_handle_event (GdkEvent* event,ArdourCanvas::Item*, AudioRegionView*);
	bool canvas_fade_out_event (GdkEvent* event,ArdourCanvas::Item*, AudioRegionView*);
	bool canvas_fade_out_handle_event (GdkEvent* event,ArdourCanvas::Item*, AudioRegionView*);
	

	// These variables are used to detect a feedback loop and break it to avoid a gui hang
private:
	ArdourCanvas::Item *last_item_entered;
	int last_item_entered_n;
public:

	bool canvas_region_view_event (GdkEvent* event,ArdourCanvas::Item*, RegionView*);
	bool canvas_region_view_name_highlight_event (GdkEvent* event,ArdourCanvas::Item*, RegionView*);
	bool canvas_region_view_name_event (GdkEvent* event,ArdourCanvas::Item*, RegionView*);
	bool canvas_stream_view_event (GdkEvent* event,ArdourCanvas::Item*, boost::weak_ptr<TimeAxisView>);
	bool canvas_marker_event (GdkEvent* event,ArdourCanvas::Item*, Marker*);
	bool canvas_zoom_rect_event (GdkEvent* event,ArdourCanvas::Item*);
	bool canvas_tempo_marker_event (GdkEvent* event,ArdourCanvas::Item*, TempoMarker*);
	bool canvas_meter_marker_event (GdkEvent* event,ArdourCanvas::Item*, MeterMarker*);
	bool canvas_automation_track_event(GdkEvent* event, ArdourCanvas::Item*, boost::weak_ptr<AutomationTimeAxisView>);

	bool canvas_tempo_bar_event (GdkEvent* event, ArdourCanvas::Item*);
	bool canvas_meter_bar_event (GdkEvent* event, ArdourCanvas::Item*);
	bool canvas_marker_bar_event (GdkEvent* event, ArdourCanvas::Item*);
	bool canvas_range_marker_bar_event (GdkEvent* event, ArdourCanvas::Item*);
	bool canvas_transport_marker_bar_event (GdkEvent* event, ArdourCanvas::Item*);
	bool canvas_cd_marker_bar_event (GdkEvent* event, ArdourCanvas::Item*);

	bool canvas_imageframe_item_view_event(GdkEvent* event, ArdourCanvas::Item*,ImageFrameView*);
	bool canvas_imageframe_view_event(GdkEvent* event, ArdourCanvas::Item*,ImageFrameTimeAxis*);
	bool canvas_imageframe_start_handle_event(GdkEvent* event, ArdourCanvas::Item*,ImageFrameView*);
	bool canvas_imageframe_end_handle_event(GdkEvent* event, ArdourCanvas::Item*,ImageFrameView*);
	bool canvas_marker_time_axis_view_event(GdkEvent* event, ArdourCanvas::Item*,MarkerTimeAxis*);
	bool canvas_markerview_item_view_event(GdkEvent* event, ArdourCanvas::Item*,MarkerView*);
	bool canvas_markerview_start_handle_event(GdkEvent* event, ArdourCanvas::Item*,MarkerView*);
	bool canvas_markerview_end_handle_event(GdkEvent* event, ArdourCanvas::Item*,MarkerView*);

	/* non-public event handlers */

	bool canvas_playhead_cursor_event (GdkEvent* event, ArdourCanvas::Item*);
	bool track_canvas_scroll (GdkEventScroll* event);

	bool track_canvas_scroll_event (GdkEventScroll* event);
	bool track_canvas_button_press_event (GdkEventButton* event);
	bool track_canvas_button_release_event (GdkEventButton* event);
	bool track_canvas_motion_notify_event (GdkEventMotion* event);

	Gtk::Allocation canvas_allocation;
	void track_canvas_allocate (Gtk::Allocation alloc);
	bool track_canvas_size_allocated ();
	bool track_canvas_drag_motion (Glib::RefPtr<Gdk::DragContext> const &, int, int, guint);

	void set_playhead_cursor ();

	void kbd_driver (sigc::slot<void,GdkEvent*>, bool use_track_canvas = true, bool use_time_canvas = true, bool can_select = true);
	void kbd_mute_unmute_region ();
	void kbd_brush ();

	void kbd_do_brush (GdkEvent*);
	void kbd_do_audition (GdkEvent*);

	void handle_new_duration ();
	void initialize_canvas ();

	/* display control */
	
	bool _show_measures;
	/// true if the editor should follow the playhead, otherwise false
	bool _follow_playhead;
	/// true if waveforms should be shown while recording audio tracks, otherwise false
	bool _show_waveforms_recording;
	
	ARDOUR::TempoMap::BBTPointList *current_bbt_points;
	
	TempoLines* tempo_lines;

	ArdourCanvas::Group* time_line_group;

	void hide_measures ();
	void draw_measures ();
	bool redraw_measures ();

	void new_tempo_section ();

	void mouse_add_new_tempo_event (nframes64_t where);
	void mouse_add_new_meter_event (nframes64_t where);

	void remove_tempo_marker (ArdourCanvas::Item*);
	void remove_meter_marker (ArdourCanvas::Item*);
	gint real_remove_tempo_marker (ARDOUR::TempoSection*);
	gint real_remove_meter_marker (ARDOUR::MeterSection*);
	
	void edit_tempo_section (ARDOUR::TempoSection*);
	void edit_meter_section (ARDOUR::MeterSection*);
	void edit_tempo_marker (ArdourCanvas::Item*);
	void edit_meter_marker (ArdourCanvas::Item*);
	void edit_control_point (ArdourCanvas::Item*);
	
	void marker_menu_edit ();
	void marker_menu_remove ();
	void marker_menu_rename ();
	void marker_menu_lock (bool yn);
	void marker_menu_hide ();
	void marker_menu_loop_range ();
	void marker_menu_select_all_selectables_using_range ();
	void marker_menu_select_using_range ();
	void marker_menu_separate_regions_using_location ();
	void marker_menu_play_from ();
	void marker_menu_play_range ();
	void marker_menu_set_playhead ();
	void marker_menu_set_from_playhead ();
	void marker_menu_set_from_selection ();
	void marker_menu_range_to_next ();
	void new_transport_marker_menu_set_loop ();
	void new_transport_marker_menu_set_punch ();
	void update_loop_range_view (bool visibility=false);
	void update_punch_range_view (bool visibility=false);
        void new_transport_marker_menu_popdown ();
	void marker_context_menu (GdkEventButton*, ArdourCanvas::Item*);
	void tm_marker_context_menu (GdkEventButton*, ArdourCanvas::Item*);
	void transport_marker_context_menu (GdkEventButton*, ArdourCanvas::Item*);
	void new_transport_marker_context_menu (GdkEventButton*, ArdourCanvas::Item*);
	void build_range_marker_menu (bool loop_or_punch);
	void build_marker_menu (bool start_or_end);
	void build_tm_marker_menu ();
	void build_new_transport_marker_menu ();

	Gtk::Menu* tm_marker_menu;
	Gtk::Menu* marker_menu;
	Gtk::Menu* start_end_marker_menu;
	Gtk::Menu* range_marker_menu;
	Gtk::Menu* transport_marker_menu;
	Gtk::Menu* new_transport_marker_menu;
	Gtk::Menu* cd_marker_menu;
	ArdourCanvas::Item* marker_menu_item;

	typedef std::list<Marker*> Marks;
	Marks metric_marks;

	void remove_metric_marks ();
	void draw_metric_marks (const ARDOUR::Metrics& metrics);

	void compute_current_bbt_points (nframes_t left, nframes_t right);
	void tempo_map_changed (ARDOUR::Change);
	void redisplay_tempo (bool immediate_redraw);
	
	void snap_to (nframes64_t& first, int32_t direction = 0, bool for_mark = false);
	void snap_to (nframes64_t& first, nframes64_t& last, int32_t direction = 0, bool for_mark = false);

	double snap_length_beats (nframes64_t start);

	uint32_t bbt_beat_subdivision;

	/* toolbar */

	Gtk::ToggleButton editor_mixer_button;
	Gtk::ToggleButton editor_list_button;
	void editor_mixer_button_toggled ();
	void editor_list_button_toggled ();

	AudioClock               edit_point_clock;
	AudioClock               zoom_range_clock;
	Gtk::Button              zoom_in_button;
	Gtk::Button              zoom_out_button;
	Gtk::Button              zoom_out_full_button;
	Gtk::Button              zoom_onetoone_button;

	Gtk::VBox                toolbar_clock_vbox;
	Gtk::VBox                toolbar_selection_clock_vbox; 
	Gtk::Table               toolbar_selection_clock_table;
	Gtk::Label               toolbar_selection_cursor_label;
	
	Gtk::HBox                mouse_mode_button_box;
	Gtkmm2ext::TearOff*      mouse_mode_tearoff;
	Gtk::ToggleButton        mouse_select_button;
	Gtk::ToggleButton        mouse_move_button;
	Gtk::ToggleButton        mouse_gain_button;
	Gtk::ToggleButton        mouse_zoom_button;
	Gtk::ToggleButton        mouse_timefx_button;
	Gtk::ToggleButton        mouse_audition_button;
	Gtk::ToggleButton        mouse_note_button;
	GroupedButtons          *mouse_mode_button_set;
	void                     mouse_mode_toggled (Editing::MouseMode m);
	bool                     ignore_mouse_mode_toggle;

	gint                     mouse_select_button_release (GdkEventButton*);

	Gtk::VBox                automation_box;
	Gtk::Button              automation_mode_button;
	Gtk::ToggleButton        global_automation_button;

	Gtk::ComboBoxText edit_mode_selector;
	Gtk::VBox         edit_mode_box;

	void set_edit_mode (ARDOUR::EditMode);
	void cycle_edit_mode ();
	void edit_mode_selection_done ();

	Gtk::ComboBoxText snap_type_selector;
	Gtk::ComboBoxText snap_mode_selector;
	Gtk::HBox         snap_box;

	std::vector<std::string> snap_type_strings;
	std::vector<std::string> snap_mode_strings;

	void snap_type_selection_done ();
	void snap_mode_selection_done ();
	void snap_mode_chosen (Editing::SnapMode);
	void snap_type_chosen (Editing::SnapType);

	Glib::RefPtr<Gtk::RadioAction> snap_type_action (Editing::SnapType);
	Glib::RefPtr<Gtk::RadioAction> snap_mode_action (Editing::SnapMode);

	Gtk::ComboBoxText zoom_focus_selector;
	Gtk::VBox         zoom_focus_box;

	std::vector<std::string> zoom_focus_strings;
	
	void zoom_focus_selection_done ();
	void zoom_focus_chosen (Editing::ZoomFocus);

	Glib::RefPtr<Gtk::RadioAction> zoom_focus_action (Editing::ZoomFocus);

	Gtk::HBox           zoom_box;
	Gtk::VBox           zoom_vbox;

	void                zoom_adjustment_changed();

	void                edit_point_clock_changed();
	
	void setup_toolbar ();

	Gtkmm2ext::TearOff*      tools_tearoff;
	Gtk::HBox                toolbar_hbox;
	Gtk::EventBox            toolbar_base;
	Gtk::Frame               toolbar_frame;

	/* midi toolbar */

	Gtk::HBox                midi_tool_button_box;
	Gtk::ToggleButton        midi_tool_pencil_button;
	Gtk::ToggleButton        midi_tool_select_button;
	Gtk::ToggleButton        midi_tool_resize_button;
	Gtk::ToggleButton        midi_tool_erase_button;
	Gtk::Button              midi_panic_button;
	Gtk::ToggleButton        midi_sound_notes;
	GroupedButtons          *midi_tool_button_set;
	void                     midi_edit_mode_toggled (Editing::MidiEditMode m);
	void                     midi_panic_button_pressed ();
	bool                     sound_notes () const { return midi_sound_notes.get_active(); }
	
	bool                     ignore_midi_edit_mode_toggle;

	Gtkmm2ext::TearOff* midi_tools_tearoff;
	Gtk::HBox           midi_toolbar_hbox;
	Gtk::EventBox       midi_toolbar_base;
	Gtk::Frame          midi_toolbar_frame;
	
	void setup_midi_toolbar ();

	/* selection process */

	Selection* selection;
	Selection* cut_buffer;

	void time_selection_changed ();
	void track_selection_changed ();
	void region_selection_changed ();
	sigc::connection editor_regions_selection_changed_connection;
	void sensitize_the_right_region_actions (bool have_selected_regions);
	void point_selection_changed ();
	void marker_selection_changed ();

	void cancel_selection ();

	void region_selection_op (void (ARDOUR::Region::*pmf)(void));
	void region_selection_op (void (ARDOUR::Region::*pmf)(void*), void*);
	void region_selection_op (void (ARDOUR::Region::*pmf)(bool), bool);

	bool audio_region_selection_covers (nframes64_t where);

	/* transport range select process */

	ArdourCanvas::SimpleRect*  cd_marker_bar_drag_rect;
	ArdourCanvas::SimpleRect*  range_bar_drag_rect;
	ArdourCanvas::SimpleRect*  transport_bar_drag_rect;

#ifdef GTKOSX
	ArdourCanvas::SimpleRect     *bogus_background_rect;
#endif
	ArdourCanvas::SimpleRect     *transport_bar_range_rect;
	ArdourCanvas::SimpleRect     *transport_bar_preroll_rect;
	ArdourCanvas::SimpleRect     *transport_bar_postroll_rect;
	ArdourCanvas::SimpleRect     *transport_loop_range_rect;
	ArdourCanvas::SimpleRect     *transport_punch_range_rect;
	ArdourCanvas::SimpleLine     *transport_punchin_line;
	ArdourCanvas::SimpleLine     *transport_punchout_line;
	ArdourCanvas::SimpleRect     *transport_preroll_rect;
	ArdourCanvas::SimpleRect     *transport_postroll_rect;

	ARDOUR::Location*  transport_loop_location();
	ARDOUR::Location*  transport_punch_location();

	ARDOUR::Location   *temp_location;
	
	/* object rubberband select process */
	
	bool select_all_within (nframes64_t start, nframes64_t end, gdouble topy, gdouble boty, const TrackViewList&, Selection::Operation op);
	
	ArdourCanvas::SimpleRect   *rubberband_rect;
	
	/* mouse zoom process */

	ArdourCanvas::SimpleRect   *zoom_rect;
	void reposition_zoom_rect (nframes64_t start, nframes64_t end);

	EditorRouteGroups* _route_groups;
	EditorRoutes* _routes;
	EditorRegions* _regions;
	EditorSnapshots* _snapshots;
	
	/* diskstream/route display management */
	Glib::RefPtr<Gdk::Pixbuf> rec_enabled_icon;
	Glib::RefPtr<Gdk::Pixbuf> rec_disabled_icon;

	Glib::RefPtr<Gtk::TreeSelection> route_display_selection;

	bool sync_track_view_list_and_routes ();

	Gtk::VBox           list_vpacker;

	/* autoscrolling */

	bool autoscroll_active;
	int autoscroll_timeout_tag;
	int autoscroll_x;
	int autoscroll_y;
	int last_autoscroll_x;
	int last_autoscroll_y;
	uint32_t autoscroll_cnt;
	nframes64_t autoscroll_x_distance;
	double autoscroll_y_distance;

	static gint _autoscroll_canvas (void *);
	bool autoscroll_canvas ();
	void start_canvas_autoscroll (int x, int y);
	void stop_canvas_autoscroll ();
	void maybe_autoscroll (GdkEventMotion*, bool);
	bool allow_vertical_scroll;

	/* trimming */
	void point_trim (GdkEvent*);
	void single_contents_trim (RegionView&, nframes64_t, bool, bool, bool);
	void single_start_trim (RegionView&, nframes64_t, bool, bool, bool);
	void single_end_trim (RegionView&, nframes64_t, bool, bool, bool);

	void thaw_region_after_trim (RegionView& rv);

	void trim_region_front();
	void trim_region_back();
	void trim_region (bool front);

	void trim_region_to_edit_point ();
	void trim_region_from_edit_point ();
	void trim_region_to_loop ();
	void trim_region_to_punch ();
	void trim_region_to_location (const ARDOUR::Location&, const char* cmd);

	void trim_to_region(bool forward);
	void trim_region_to_previous_region_end();
	void trim_region_to_next_region_start();

	bool show_gain_after_trim;

	/* Drag-n-Drop */

	int convert_drop_to_paths (
			std::vector<Glib::ustring>&           paths,
			const Glib::RefPtr<Gdk::DragContext>& context,
			gint                                  x,
			gint                                  y,
			const Gtk::SelectionData&             data,
			guint                                 info,
			guint                                 time);

	void track_canvas_drag_data_received (
			const Glib::RefPtr<Gdk::DragContext>& context,
			gint                                  x,
			gint                                  y,
			const Gtk::SelectionData&             data,
			guint                                 info,
			guint                                 time);

	void drop_paths (
			const Glib::RefPtr<Gdk::DragContext>& context,
			gint                                  x,
			gint                                  y,
			const Gtk::SelectionData&             data,
			guint                                 info,
			guint                                 time);

	void drop_regions (
			const Glib::RefPtr<Gdk::DragContext>& context,
			gint                                  x,
			gint                                  y,
			const Gtk::SelectionData&             data,
			guint                                 info,
			guint                                 time);

	void drop_routes (
			const Glib::RefPtr<Gdk::DragContext>& context,
			gint                x,
			gint                y,
			const Gtk::SelectionData& data,
			guint               info,
			guint               time);

	/* audio export */

	int  write_region_selection(RegionSelection&);
	bool write_region (std::string path, boost::shared_ptr<ARDOUR::AudioRegion>);
	void bounce_region_selection ();
	void bounce_range_selection (bool replace, bool enable_processing);
	void external_edit_region ();

	int write_audio_selection (TimeSelection&);
	bool write_audio_range (ARDOUR::AudioPlaylist&, const ARDOUR::ChanCount& channels, std::list<ARDOUR::AudioRange>&);

	void write_selection ();

	XMLNode *before; /* used in *_reversible_command */

	void begin_reversible_command (std::string cmd_name);
	void commit_reversible_command ();

	void update_title ();	
	void update_title_s (const std::string & snapshot_name);

	struct State {
	    Selection* selection;
	    double frames_per_unit;

	    State (PublicEditor const * e);
	    ~State ();
	};

	void store_state (State&) const;
	void restore_state (State *);

	void instant_save ();

	boost::shared_ptr<ARDOUR::AudioRegion> last_audition_region;
	
	/* freeze operations */

	ARDOUR::InterThreadInfo freeze_status;
	gint freeze_progress_timeout (void *);
	static void* _freeze_thread (void*);
	void* freeze_thread ();

	void freeze_route ();
	void unfreeze_route ();

	/* route-group solo + mute */

	void set_route_group_solo (ARDOUR::Route&, bool);
	void set_route_group_mute (ARDOUR::Route&, bool);

	/* duplication */

	void duplicate_dialog (bool with_dialog);
	
	nframes64_t event_frame (GdkEvent const *, double* px = 0, double* py = 0) const;

	/* returns false if mouse pointer is not in track or marker canvas
	 */
	bool mouse_frame (nframes64_t&, bool& in_track_canvas) const;

	/* "whats mine is yours" */

	TimeFXDialog* current_timefx;

	static void* timefx_thread (void *arg);
	void do_timefx (TimeFXDialog&);

	int time_stretch (RegionSelection&, float fraction);
	int pitch_shift (RegionSelection&, float cents);
	void pitch_shift_regions ();
	int time_fx (RegionSelection&, float val, bool pitching);

	/* editor-mixer strip */

	MixerStrip *current_mixer_strip;
	bool show_editor_mixer_when_tracks_arrive;
	Gtk::VBox current_mixer_strip_vbox;
	void cms_new (boost::shared_ptr<ARDOUR::Route>);
	void cms_deleted ();
	void current_mixer_strip_hidden ();
	void current_mixer_strip_removed ();

	void detach_tearoff (Gtk::Box* b, Gtk::Window* w);
	void reattach_tearoff (Gtk::Box* b, Gtk::Window* w, int32_t n);
#ifdef GTKOSX
	void ensure_all_elements_drawn ();
#endif
	/* nudging tracks */

	void nudge_track (bool use_edit_point, bool forwards);

	/* xfades */

	bool _xfade_visibility;
	
#ifdef WITH_CMT
	void handle_new_imageframe_time_axis_view(const std::string & track_name, void* src) ;
	void handle_new_imageframe_marker_time_axis_view(const std::string & track_name, TimeAxisViewPtr marked_track) ;

	void start_imageframe_grab(ArdourCanvas::Item*, GdkEvent*) ;
	void start_markerview_grab(ArdourCanvas::Item*, GdkEvent*) ;

	void imageframe_drag_motion_callback(ArdourCanvas::Item*, GdkEvent*) ;
	void markerview_drag_motion_callback(ArdourCanvas::Item*, GdkEvent*) ;
	void timeaxis_item_drag_finished_callback(ArdourCanvas::Item*, GdkEvent*) ;

	gint canvas_imageframe_item_view_event(ArdourCanvas::Item* item, GdkEvent* event, ImageFrameView* ifv);
	gint canvas_imageframe_view_event(ArdourCanvas::Item* item, GdkEvent* event, ImageFrameTimeAxis* ifta);
	gint canvas_imageframe_start_handle_event(ArdourCanvas::Item* item, GdkEvent* event, ImageFrameView* ifv);
	gint canvas_imageframe_end_handle_event(ArdourCanvas::Item* item, GdkEvent* event, ImageFrameView* ifv);

	gint canvas_marker_time_axis_view_event(ArdourCanvas::Item* item, GdkEvent* event, MarkerTimeAxis* mta);
	gint canvas_markerview_item_view_event(ArdourCanvas::Item* item, GdkEvent* event, MarkerView* mv);
	gint canvas_markerview_start_handle_event(ArdourCanvas::Item* item, GdkEvent* event, MarkerView* mv);
	gint canvas_markerview_end_handle_event(ArdourCanvas::Item* item, GdkEvent* event, MarkerView* mv);

	void imageframe_start_handle_op(ArdourCanvas::Item* item, GdkEvent* event) ;
	void imageframe_end_handle_op(ArdourCanvas::Item* item, GdkEvent* event) ;
	void imageframe_start_handle_trim_motion(ArdourCanvas::Item* item, GdkEvent* event) ;
	void imageframe_start_handle_end_trim(ArdourCanvas::Item* item, GdkEvent* event) ;
	void imageframe_end_handle_trim_motion(ArdourCanvas::Item* item, GdkEvent* event) ;
	void imageframe_end_handle_end_trim(ArdourCanvas::Item* item, GdkEvent* event) ;
	
	void markerview_item_start_handle_op(ArdourCanvas::Item* item, GdkEvent* event) ;
	void markerview_item_end_handle_op(ArdourCanvas::Item* item, GdkEvent* event) ;
	void markerview_start_handle_trim_motion(ArdourCanvas::Item* item, GdkEvent* event) ;
	void markerview_start_handle_end_trim(ArdourCanvas::Item* item, GdkEvent* event) ;
	void markerview_end_handle_trim_motion(ArdourCanvas::Item* item, GdkEvent* event) ;
	void markerview_end_handle_end_trim(ArdourCanvas::Item* item, GdkEvent* event) ;

	void popup_imageframe_edit_menu(int button, int32_t time, ArdourCanvas::Item* ifv, bool with_frame) ;
	void popup_marker_time_axis_edit_menu(int button, int32_t time, ArdourCanvas::Item* ifv, bool with_frame) ;

	ImageFrameSocketHandler* image_socket_listener ;
#endif

	void toggle_xfade_active (boost::weak_ptr<ARDOUR::Crossfade>);
	void toggle_xfade_length (boost::weak_ptr<ARDOUR::Crossfade>);
	void edit_xfade (boost::weak_ptr<ARDOUR::Crossfade>);
	void xfade_edit_left_region ();
	void xfade_edit_right_region ();

	static const int32_t default_width = 995;
	static const int32_t default_height = 765;

	/* nudge */

	Gtk::Button      nudge_forward_button;
	Gtk::Button      nudge_backward_button;
	Gtk::HBox        nudge_hbox;
	Gtk::VBox        nudge_vbox;
	AudioClock       nudge_clock;

	nframes64_t get_nudge_distance (nframes64_t pos, nframes64_t& next);

	bool nudge_forward_release (GdkEventButton*);
	bool nudge_backward_release (GdkEventButton*);
	
	/* audio filters */

	void apply_filter (ARDOUR::Filter&, std::string cmd);

	/* handling cleanup */

	int playlist_deletion_dialog (boost::shared_ptr<ARDOUR::Playlist>);

	std::vector<sigc::connection> session_connections;

	/* tracking step changes of track height */

	TimeAxisViewPtr current_stepping_trackview;
	ARDOUR::microseconds_t last_track_height_step_timestamp;
	gint track_height_step_timeout();
	sigc::connection step_timeout;

	TimeAxisViewPtr entered_track;
	RegionView*   entered_regionview;


	void ensure_entered_track_selected (bool op_acts_on_objects = false);
	bool clear_entered_track;
	bool left_track_canvas (GdkEventCrossing*);
	bool entered_track_canvas (GdkEventCrossing*);
	void set_entered_track (TimeAxisViewPtr);
	void set_entered_regionview (RegionView*);
	void ensure_track_visible (TimeAxisViewPtr);
	gint left_automation_track ();

	bool _new_regionviews_show_envelope;

	void reset_canvas_action_sensitivity (bool);
	void toggle_gain_envelope_visibility ();
	void toggle_gain_envelope_active ();
	void reset_region_gain_envelopes ();

	bool on_key_press_event (GdkEventKey*);
	bool on_key_release_event (GdkEventKey*);

	void session_state_saved (std::string);

	Glib::RefPtr<Gtk::Action>              undo_action;
	Glib::RefPtr<Gtk::Action>              redo_action;

	void history_changed ();

	Gtk::HBox      status_bar_hpacker;

	Editing::EditPoint _edit_point;

	Gtk::ComboBoxText edit_point_selector;

	void set_edit_point_preference (Editing::EditPoint ep, bool force = false);
	void cycle_edit_point (bool with_marker);
	void set_edit_point ();
	void edit_point_selection_done ();
	void edit_point_chosen (Editing::EditPoint);
	Glib::RefPtr<Gtk::RadioAction> edit_point_action (Editing::EditPoint);
	std::vector<std::string> edit_point_strings;

	void selected_marker_moved (ARDOUR::Location*);
	sigc::connection edit_point_clock_connection_a;
	sigc::connection edit_point_clock_connection_b;

	bool get_edit_op_range (nframes64_t& start, nframes64_t& end) const;

	void get_regions_at (RegionSelection&, nframes64_t where, const TrackSelection& ts) const;
	void get_regions_after (RegionSelection&, nframes64_t where, const TrackSelection& ts) const;
	
	void get_regions_for_action (RegionSelection&, bool allowed_entered_regionview = false);

	sigc::connection fast_screen_update_connection;
	gint start_updating ();
	gint stop_updating ();
	void fast_update_strips ();
	bool meters_running;

	void select_next_route ();
	void select_prev_route ();

	void snap_to_internal (nframes64_t& first, int32_t direction = 0, bool for_mark = false);

	RhythmFerret* rhythm_ferret;
	BundleManager* _bundle_manager;
	GlobalPortMatrixWindow* _global_port_matrix[ARDOUR::DataType::num_types];

	void fit_tracks (TrackSelection &);
	void fit_selected_tracks ();
	void set_track_height (uint32_t h);

	void remove_tracks ();
	void toggle_tracks_active ();

	bool _have_idled;
	int resize_idle_id;
	bool idle_resize();
	friend gboolean _idle_resize (gpointer);
	int32_t _pending_resize_amount;
	TimeAxisViewPtr _pending_resize_view;

	void visible_order_range (int*, int*) const;

	void located ();
	bool _pending_locate_request;

	EditorSummary* _summary;
	void region_view_added (RegionView *);

	void update_canvas_now ();
	void streamview_height_changed ();

	EditorGroupTabs* _group_tabs;
	void fit_route_group (ARDOUR::RouteGroup *);

	TimeAxisViewPtr find_time_axis (TimeAxisView *);

	friend class Drag;
	friend class RegionDrag;
	friend class RegionMoveDrag;
	friend class RegionSpliceDrag;
	friend class TrimDrag;
	friend class MeterMarkerDrag;
	friend class TempoMarkerDrag;
	friend class CursorDrag;
	friend class FadeInDrag;
	friend class FadeOutDrag;
	friend class MarkerDrag;
	friend class RegionGainDrag;
	friend class ControlPointDrag;
	friend class LineDrag;
	friend class RubberbandSelectDrag;
        friend class TimeFXDrag;
        friend class ScrubDrag;
	friend class SelectionDrag;
	friend class RangeMarkerBarDrag;
	friend class MouseZoomDrag;
	friend class RegionCreateDrag;
	friend class RegionMotionDrag;
	friend class RegionInsertDrag;

	friend class EditorSummary;
	friend class EditorGroupTabs;

	friend class EditorRoutes;
};

#endif /* __ardour_editor_h__ */
