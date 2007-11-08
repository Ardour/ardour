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

#include <pbd/stateful.h>
#include <ardour/session.h>
#include <ardour/stretch.h>
#include <ardour/tempo.h>
#include <ardour/location.h>
#include <ardour/audioregion.h>

#include "audio_clock.h"
#include "gtk-custom-ruler.h"
#include "ardour_dialog.h"
#include "public_editor.h"
#include "editing.h"
#include "enums.h"
#include "editor_items.h"
#include "region_selection.h"
#include "canvas.h"
#include "draginfo.h"

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
	class AudioFilter;
	class Crossfade;
}

namespace LADSPA {
	class Plugin;
}

class TimeAxisView;
class RouteTimeAxisView;
class AudioTimeAxisView;
class AutomationTimeAxisView;
class AudioRegionView;
class CrossfadeView;
class PluginSelector;
class PlaylistSelector;
class Marker;
class GroupedButtons;
class AutomationLine;
class UIExportSpecification;
class ExportDialog;
class Selection;
class TimeSelection;
class TrackSelection;
class AutomationSelection;
class MixerStrip;
class StreamView;
class AudioStreamView;
class ControlPoint;
class SoundFileOmega;
#ifdef FFT_ANALYSIS
class AnalysisWindow;
#endif

/* <CMT Additions> */
class ImageFrameView;
class ImageFrameTimeAxisView;
class ImageFrameTimeAxis;
class MarkerTimeAxis ;
class MarkerView ;
class ImageFrameSocketHandler ;
class TimeAxisViewItem ;
/* </CMT Additions> */


class Editor : public PublicEditor
{
  public:
	Editor ();
	~Editor ();

	void             connect_to_session (ARDOUR::Session *);
	ARDOUR::Session* current_session() const { return session; }

	nframes_t leftmost_position() const { return leftmost_frame; }
	nframes_t current_page_frames() const {
		return (nframes_t) floor (canvas_width * frames_per_unit);
	}

	void set_snap_to (Editing::SnapType);
	void set_snap_mode (Editing::SnapMode);
	void set_snap_threshold (double pixel_distance) {snap_threshold = pixel_distance;}

	void undo (uint32_t n = 1);
	void redo (uint32_t n = 1);

	XMLNode& get_state ();
	int set_state (const XMLNode& );

	void set_mouse_mode (Editing::MouseMode, bool force=true);
	void step_mouse_mode (bool next);
	Editing::MouseMode current_mouse_mode () { return mouse_mode; }

	void add_imageframe_time_axis(const std::string & track_name, void*) ;
	void add_imageframe_marker_time_axis(const std::string & track_name, TimeAxisView* marked_track, void*) ;
	void connect_to_image_compositor() ;
	void scroll_timeaxis_to_imageframe_item(const TimeAxisViewItem* item) ;
	TimeAxisView* get_named_time_axis(const std::string & name) ;

	void consider_auditioning (boost::shared_ptr<ARDOUR::Region>);
	void hide_a_region (boost::shared_ptr<ARDOUR::Region>);
	void remove_a_region (boost::shared_ptr<ARDOUR::Region>);

	/* option editor-access */

	void set_show_waveforms (bool yn);
	bool show_waveforms() const { return _show_waveforms; }

	void set_show_waveforms_recording (bool yn);
	bool show_waveforms_recording() const { return _show_waveforms_recording; }
	
	/* things that need to be public to be used in the main menubar */

	void new_region_from_selection ();
	void separate_region_from_selection ();
	void separate_regions_using_location (ARDOUR::Location&);
	void toggle_playback (bool with_abort);
	void transition_to_rolling (bool forward);

	/* undo related */

	nframes_t unit_to_frame (double unit) const {
		return (nframes_t) rint (unit * frames_per_unit);
	}
	
	double frame_to_unit (nframes_t frame) const {
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
			return (nframes_t) rint (pixel * frames_per_unit * GNOME_CANVAS(track_canvas.gobj())->pixels_per_unit);
		} else {
			return 0;
		}
	}

	gulong frame_to_pixel (nframes64_t frame) const {
		return (gulong) rint ((frame / (frames_per_unit *  GNOME_CANVAS(track_canvas.gobj())->pixels_per_unit)));
	}

	/* selection */

	Selection& get_selection() const { return *selection; }
	Selection& get_cut_buffer() const { return *cut_buffer; }

	bool extend_selection_to_track (TimeAxisView&);

	void play_selection ();
	void select_all_in_track (Selection::Operation op);
	void select_all (Selection::Operation op);
	void invert_selection_in_track ();
	void invert_selection ();

	/* tempo */

	void set_show_measures (bool yn);
	bool show_measures () const { return _show_measures; }
	bool initial_ruler_update_required;

#ifdef FFT_ANALYSIS
	/* analysis window */
	void analyze_region_selection();
	void analyze_range_selection();
#endif

	/* export */

	/* these initiate export ... */
	
	void export_session();
	void export_selection();

	void add_toplevel_controls (Gtk::Container&);
	Gtk::HBox& get_status_bar_packer()  { return status_bar_hpacker; }

	void      set_zoom_focus (Editing::ZoomFocus);
	Editing::ZoomFocus get_zoom_focus () const { return zoom_focus; }
	gdouble   get_current_zoom () { return frames_per_unit; }

	void temporal_zoom_step (bool coarser);

	/* stuff that AudioTimeAxisView and related classes use */

	PlaylistSelector& playlist_selector() const;
	void route_name_changed (TimeAxisView *);
	gdouble        frames_per_unit;
	nframes_t leftmost_frame;
	void clear_playlist (boost::shared_ptr<ARDOUR::Playlist>);

	void new_playlists (TimeAxisView*);
	void copy_playlists (TimeAxisView*);
	void clear_playlists (TimeAxisView*);

	TrackViewList* get_valid_views (TimeAxisView*, ARDOUR::RouteGroup* grp = 0);

	Width editor_mixer_strip_width;
	void maybe_add_mixer_strip_width (XMLNode&);
	void show_editor_mixer (bool yn);
	void set_selected_mixer_strip (TimeAxisView&);
	void hide_track_in_display (TimeAxisView& tv);
	void show_track_in_display (TimeAxisView& tv);

	/* nudge is initiated by transport controls owned by ARDOUR_UI */

	void nudge_forward (bool next);
	void nudge_backward (bool next);

	/* nudge initiated from context menu */

	void nudge_forward_capture_offset ();
	void nudge_backward_capture_offset ();

	/* playhead/screen stuff */
	
	void set_follow_playhead (bool yn);
	void toggle_follow_playhead ();
	bool follow_playhead() const { return _follow_playhead; }
	bool dragging_playhead () const { return _dragging_playhead; }

	void toggle_waveform_visibility ();
	void toggle_waveforms_while_recording ();
	void toggle_measure_visibility ();
	void toggle_logo_visibility ();

	/* SMPTE timecode & video sync */

	void smpte_fps_chosen (ARDOUR::SmpteFormat format);
	void video_pullup_chosen (ARDOUR::Session::PullupFormat pullup);
	void subframes_per_frame_chosen (uint32_t);

	void update_smpte_mode();
	void update_video_pullup();
	void update_subframes_per_frame ();
	/* xfades */

	void toggle_auto_xfade ();
	void toggle_xfades_active ();
	void toggle_xfade_visibility ();
	bool xfade_visibility() const { return _xfade_visibility; }
	void update_xfade_visibility ();
	void update_crossfade_model ();
	void set_crossfade_model (ARDOUR::CrossfadeModel);

	/* layers */
	void set_layer_model (ARDOUR::LayerModel);
	void update_layering_model ();

	/* redirect shared ops menu. caller must free returned menu */

	Gtk::Menu* redirect_menu ();

	/* floating windows/transient */

	void ensure_float (Gtk::Window&);

	void show_window ();

	void scroll_tracks_down_line ();
	void scroll_tracks_up_line ();

	bool new_regionviews_display_gain () { return _new_regionviews_show_envelope; }
	void prepare_for_cleanup ();

	void maximise_editing_space();
	void restore_editing_space();

	void reset_x_origin (nframes_t);
	void reset_zoom (double);
	void reposition_and_zoom (nframes_t, double);

	nframes_t edit_cursor_position(bool);
	nframes64_t get_preferred_edit_position () const;

	bool update_mouse_speed ();
	bool decelerate_mouse_speed ();

  protected:
	void map_transport_state ();
	void map_position_change (nframes_t);

	void on_realize();

  private:
	
	ARDOUR::Session     *session;
	bool                 constructed;
  
	// to keep track of the playhead position for control_scroll
	boost::optional<nframes_t> _control_scroll_target;

	PlaylistSelector* _playlist_selector;

	void          set_frames_per_unit (double);

	Editing::MouseMode mouse_mode;

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

	uint32_t location_marker_color;
	uint32_t location_range_color;
	uint32_t location_loop_color;
	uint32_t location_punch_color;
	uint32_t location_cd_marker_color;

	struct LocationMarkers {
	    Marker* start;
	    Marker* end;

	    ~LocationMarkers ();

	    void hide();
	    void show ();
	    void set_name (const string&);
	    void set_position (nframes_t start, nframes_t end = 0);
	    void set_color_rgba (uint32_t);
	};

	LocationMarkers  *find_location_markers (ARDOUR::Location *) const;
	ARDOUR::Location* find_location_from_marker (Marker *, bool& is_start) const;

	typedef std::map<ARDOUR::Location*,LocationMarkers *> LocationMarkerMap;
	LocationMarkerMap location_markers;

	void hide_marker (ArdourCanvas::Item*, GdkEvent*);
	void clear_marker_display ();
	void mouse_add_new_marker (nframes_t where);

	TimeAxisView*      clicked_trackview;
	AudioTimeAxisView* clicked_audio_trackview;
	RegionView*        clicked_regionview;
	RegionView*        latest_regionview;
	uint32_t           clicked_selection;
	CrossfadeView*     clicked_crossfadeview;
	ControlPoint*      clicked_control_point;

	void sort_track_selection ();

	void get_relevant_audio_tracks (std::set<AudioTimeAxisView*>& relevant_tracks);
	void get_equivalent_regions (RegionView* rv, std::vector<RegionView*>&);
	void mapover_audio_tracks (sigc::slot<void,AudioTimeAxisView&,uint32_t> sl, TimeAxisView*);

	/* functions to be passed to mapover_audio_tracks(), possibly with sigc::bind()-supplied arguments */

	void mapped_get_equivalent_regions (RouteTimeAxisView&, uint32_t, RegionView*, vector<RegionView*>*);
	void mapped_use_new_playlist (AudioTimeAxisView&, uint32_t);
	void mapped_use_copy_playlist (AudioTimeAxisView&, uint32_t);
	void mapped_clear_playlist (AudioTimeAxisView&, uint32_t);

	/* end */

	void button_selection (ArdourCanvas::Item* item, GdkEvent* event, ItemType item_type);
	bool button_release_can_deselect;

	void catch_vanishing_regionview (RegionView *);

	bool set_selected_track (TimeAxisView&, Selection::Operation op = Selection::Set, bool no_remove=false);
	void select_all_tracks ();

	bool set_selected_control_point_from_click (Selection::Operation op = Selection::Set, bool no_remove=false);
	bool set_selected_track_from_click (bool press, Selection::Operation op = Selection::Set, bool no_remove=false);
	bool set_selected_regionview_from_click (bool press, Selection::Operation op = Selection::Set, bool no_track_remove=false);

	void set_selected_regionview_from_region_list (boost::shared_ptr<ARDOUR::Region> region, Selection::Operation op = Selection::Set);
	bool set_selected_regionview_from_map_event (GdkEventAny*, StreamView*, boost::weak_ptr<ARDOUR::Region>);
	void collect_new_region_view (RegionView *);

	Gtk::Menu track_context_menu;
	Gtk::Menu track_region_context_menu;
	Gtk::Menu track_selection_context_menu;
	Gtk::Menu track_crossfade_context_menu;

	Gtk::MenuItem* region_edit_menu_split_item;
	Gtk::MenuItem* region_edit_menu_split_multichannel_item;
	Gtk::Menu * track_region_edit_playlist_menu;
	Gtk::Menu * track_edit_playlist_submenu;
	Gtk::Menu * track_selection_edit_playlist_submenu;
	
	void popup_track_context_menu (int, int, ItemType, bool, nframes_t);
	Gtk::Menu* build_track_context_menu (nframes_t);
	Gtk::Menu* build_track_bus_context_menu (nframes_t);
	Gtk::Menu* build_track_region_context_menu (nframes_t frame);
	Gtk::Menu* build_track_crossfade_context_menu (nframes_t);
	Gtk::Menu* build_track_selection_context_menu (nframes_t);
	void add_dstream_context_items (Gtk::Menu_Helpers::MenuList&);
	void add_bus_context_items (Gtk::Menu_Helpers::MenuList&);
	void add_region_context_items (AudioStreamView*, boost::shared_ptr<ARDOUR::Region>, Gtk::Menu_Helpers::MenuList&);
	void add_crossfade_context_items (AudioStreamView*, boost::shared_ptr<ARDOUR::Crossfade>, Gtk::Menu_Helpers::MenuList&, bool many);
	void add_selection_context_items (Gtk::Menu_Helpers::MenuList&);

	void handle_new_route (ARDOUR::Session::RouteList&);
	void remove_route (TimeAxisView *);
	bool route_removal;

	Gtk::HBox           global_hpacker;
	Gtk::VBox           global_vpacker;
	Gtk::VBox           vpacker;

	Gdk::Cursor*          current_canvas_cursor;

	ArdourCanvas::CanvasAA track_canvas;
	ArdourCanvas::CanvasAA time_canvas;

	ArdourCanvas::Text* first_action_message;
	ArdourCanvas::Text* verbose_canvas_cursor;
	bool                 verbose_cursor_visible;

	void parameter_changed (const char *);
	
	bool track_canvas_motion (GdkEvent*);

	void set_verbose_canvas_cursor (const string &, double x, double y);
	void set_verbose_canvas_cursor_text (const string &);
	void show_verbose_canvas_cursor();
	void hide_verbose_canvas_cursor();

	bool verbose_cursor_on; // so far unused

	Gtk::EventBox      time_canvas_event_box;
	Gtk::EventBox      track_canvas_event_box;
	Gtk::EventBox      time_button_event_box;
	Gtk::Frame         time_button_frame;

	ArdourCanvas::Pixbuf*     logo_item;
	ArdourCanvas::Group*      minsec_group;
	ArdourCanvas::Group*      bbt_group;
	ArdourCanvas::Group*      smpte_group;
	ArdourCanvas::Group*      frame_group;
	ArdourCanvas::Group*      tempo_group;
	ArdourCanvas::Group*      meter_group;
	ArdourCanvas::Group*      marker_group;
	ArdourCanvas::Group*      range_marker_group;
	ArdourCanvas::Group*      transport_marker_group;
	
	enum {
		ruler_metric_smpte = 0,
		ruler_metric_bbt = 1,
		ruler_metric_frames = 2,
		ruler_metric_minsec = 3,

		ruler_time_tempo = 4,
		ruler_time_meter = 5,
		ruler_time_marker = 6,
		ruler_time_range_marker = 7,
		ruler_time_transport_marker = 8,
	};

	static GtkCustomMetric ruler_metrics[4];
	bool                   ruler_shown[9];
	bool                   no_ruler_shown_update;
	
	gint ruler_button_press (GdkEventButton*);
	gint ruler_button_release (GdkEventButton*);
	gint ruler_mouse_motion (GdkEventMotion*);

	gint ruler_pressed_button;
	Gtk::Widget * ruler_grabbed_widget;
	
	void initialize_rulers ();
	void update_just_smpte ();
	void update_fixed_rulers ();
	void update_tempo_based_rulers (); 
	void popup_ruler_menu (nframes_t where = 0, ItemType type = RegionItem);
	void update_ruler_visibility ();
	void ruler_toggled (int);
	gint ruler_label_button_release (GdkEventButton*);
	void store_ruler_visibility ();
	void restore_ruler_visibility ();
	
	static gint _metric_get_smpte (GtkCustomRulerMark **, gdouble, gdouble, gint);
	static gint _metric_get_bbt (GtkCustomRulerMark **, gdouble, gdouble, gint);
	static gint _metric_get_frames (GtkCustomRulerMark **, gdouble, gdouble, gint);
	static gint _metric_get_minsec (GtkCustomRulerMark **, gdouble, gdouble, gint);
	
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
	Gtk::Menu          *editor_ruler_menu;
	
	ArdourCanvas::SimpleRect* tempo_bar;
	ArdourCanvas::SimpleRect* meter_bar;
	ArdourCanvas::SimpleRect* marker_bar;
	ArdourCanvas::SimpleRect* range_marker_bar;
	ArdourCanvas::SimpleRect* transport_marker_bar;

	
	ArdourCanvas::SimpleLine* tempo_line;
	ArdourCanvas::SimpleLine* meter_line;
	ArdourCanvas::SimpleLine* marker_line;
	ArdourCanvas::SimpleLine* range_marker_line;
	ArdourCanvas::SimpleLine* transport_marker_line;

	Gtk::Label  minsec_label;
	Gtk::Label  bbt_label;
	Gtk::Label  smpte_label;
	Gtk::Label  frame_label;
	Gtk::Label  tempo_label;
	Gtk::Label  meter_label;
	Gtk::Label  mark_label;
	Gtk::Label  range_mark_label;
	Gtk::Label  transport_mark_label;
	

	Gtk::VBox          time_button_vbox;
	Gtk::HBox          time_button_hbox;

	struct Cursor {
	    Editor&               editor;
	    ArdourCanvas::Points  points;
	    ArdourCanvas::Line    canvas_item;
	    nframes_t        current_frame;
	    double		  length;

	    Cursor (Editor&, bool (Editor::*)(GdkEvent*,ArdourCanvas::Item*));
	    ~Cursor ();

	    void set_position (nframes_t);
	    void set_length (double units);
	    void set_y_axis (double position);
	};

	friend struct Cursor; /* it needs access to several private
				 fields. XXX fix me.
			      */

	Cursor* playhead_cursor;
	Cursor* edit_cursor;
	ArdourCanvas::Group* cursor_group;

	void    cursor_to_next_region_point (Cursor*, ARDOUR::RegionPoint);
	void    cursor_to_previous_region_point (Cursor*, ARDOUR::RegionPoint);
	void    cursor_to_region_point (Cursor*, ARDOUR::RegionPoint, int32_t dir);
	void    cursor_to_selection_start (Cursor *);
	void    cursor_to_selection_end   (Cursor *);
	void    select_all_selectables_using_cursor (Cursor *, bool);
	void    select_all_selectables_between_cursors (Cursor *, Cursor *);

	boost::shared_ptr<ARDOUR::Region> find_next_region (nframes_t, ARDOUR::RegionPoint, int32_t dir, TrackViewList&, TimeAxisView ** = 0);

	vector<nframes_t> region_boundary_cache;
	void build_region_boundary_cache ();

	Gtk::VBox           trackview_vpacker;

	Gtk::HBox           top_hbox;
	Gtk::HBox           bottom_hbox;
	
	Gtk::Table          edit_packer;
	Gtk::VScrollbar     edit_vscrollbar;

	Gtk::Adjustment     vertical_adjustment;
	Gtk::Adjustment     horizontal_adjustment;

	Gtk::Layout         controls_layout;
	bool control_layout_scroll (GdkEventScroll* ev);
	void controls_layout_size_request (Gtk::Requisition*);

	Gtk::HScrollbar     edit_hscrollbar;
	bool                _dragging_hscrollbar;

	void reset_hscrollbar_stepping ();
	
	bool hscrollbar_button_press (GdkEventButton*);
	bool hscrollbar_button_release (GdkEventButton*);
	void hscrollbar_allocate (Gtk::Allocation &alloc);

	double canvas_width;
	double canvas_height;
	double full_canvas_height;
	nframes_t last_canvas_frame;

	bool track_canvas_map_handler (GdkEventAny*);
	bool time_canvas_map_handler (GdkEventAny*);

	gint edit_controls_button_release (GdkEventButton*);
	Gtk::Menu *edit_controls_left_menu;
	Gtk::Menu *edit_controls_right_menu;

	Gtk::VBox           track_canvas_vbox;
	Gtk::VBox           time_canvas_vbox;
	Gtk::VBox           edit_controls_vbox;
	Gtk::HBox           edit_controls_hbox;

	void control_scroll (float);
	bool deferred_control_scroll (nframes_t);
	sigc::connection control_scroll_connection;

	void tie_vertical_scrolling ();
	void canvas_horizontally_scrolled ();

	struct VisualChange {
	    enum Type { 
		    TimeOrigin = 0x1,
		    ZoomLevel = 0x2
	    };

	    Type pending;
	    nframes_t time_origin;
	    double frames_per_unit;

	    int idle_handler_id;

	    VisualChange() : pending ((VisualChange::Type) 0), time_origin (0), frames_per_unit (0), idle_handler_id (-1) {}
	};


	VisualChange pending_visual_change;

	static int _idle_visual_changer (void *arg);
	int idle_visual_changer ();

	void queue_visual_change (nframes_t);
	void queue_visual_change (double);

	void end_location_changed (ARDOUR::Location*);

	struct RegionListDisplayModelColumns : public Gtk::TreeModel::ColumnRecord {
	    RegionListDisplayModelColumns() {
		    add (name);
		    add (region);
		    add (color_);
	    }
	    Gtk::TreeModelColumn<Glib::ustring> name;
	    Gtk::TreeModelColumn<boost::shared_ptr<ARDOUR::Region> > region;
	    Gtk::TreeModelColumn<Gdk::Color> color_;
	};
	    
	RegionListDisplayModelColumns          region_list_columns;
	Gtkmm2ext::DnDTreeView<boost::shared_ptr<ARDOUR::Region> > region_list_display;

	Glib::RefPtr<Gtk::TreeStore>           region_list_model;
	Glib::RefPtr<Gtk::ToggleAction>        toggle_full_region_list_action;
	Glib::RefPtr<Gtk::ToggleAction>        toggle_show_auto_regions_action;

	void region_list_selection_changed ();
	bool region_list_selection_filter (const Glib::RefPtr<Gtk::TreeModel>& model, const Gtk::TreeModel::Path& path, bool yn);

	Gtk::Menu          *region_list_menu;
	Gtk::ScrolledWindow region_list_scroller;
	Gtk::Frame          region_list_frame;

	bool region_list_display_key_press (GdkEventKey *);
	bool region_list_display_key_release (GdkEventKey *);
	bool region_list_display_button_press (GdkEventButton *);
	bool region_list_display_button_release (GdkEventButton *);
	void region_list_clear ();
	void region_list_selection_mapover (sigc::slot<void,boost::shared_ptr<ARDOUR::Region> >);
	void build_region_list_menu ();
	void show_region_list_display_context_menu (int button, int time);

	bool show_automatic_regions_in_region_list;
	Editing::RegionListSortType region_list_sort_type;

	void reset_region_list_sort_direction (bool);
	void reset_region_list_sort_type (Editing::RegionListSortType);

	void toggle_full_region_list ();
	void toggle_show_auto_regions ();

	int region_list_sorter (Gtk::TreeModel::iterator, Gtk::TreeModel::iterator);

	/* snapshots */

	Gtk::ScrolledWindow      snapshot_display_scroller;
	struct SnapshotDisplayModelColumns : public Gtk::TreeModel::ColumnRecord {
	    SnapshotDisplayModelColumns() { 
		    add (visible_name);
		    add (real_name);
	    }
	    Gtk::TreeModelColumn<Glib::ustring> visible_name;
	    Gtk::TreeModelColumn<Glib::ustring> real_name;
	};

	SnapshotDisplayModelColumns snapshot_display_columns;
	Glib::RefPtr<Gtk::ListStore> snapshot_display_model;
	Gtk::TreeView snapshot_display;
	Gtk::Menu snapshot_context_menu;

	bool snapshot_display_button_press (GdkEventButton*);
	void snapshot_display_selection_changed ();
	void redisplay_snapshots();
	void popup_snapshot_context_menu (int, int32_t, Glib::ustring);

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
	void remove_snapshot (Glib::ustring);
	void rename_snapshot (Glib::ustring);

	void handle_new_named_selection ();
	void add_named_selection_to_named_selection_display (ARDOUR::NamedSelection&);
	void redisplay_named_selections ();

	bool named_selection_display_button_release (GdkEventButton *ev);
	bool named_selection_display_key_release (GdkEventKey *ev);
	void named_selection_display_selection_changed ();

	/* track views */
	TrackViewList  track_views;
	TimeAxisView     *trackview_by_y_position (double ypos);

	static Gdk::Cursor* cross_hair_cursor;
	static Gdk::Cursor* trimmer_cursor;
	static Gdk::Cursor* selector_cursor;
	static Gdk::Cursor* grabber_cursor;
	static Gdk::Cursor* zoom_cursor;
	static Gdk::Cursor* time_fx_cursor;
	static Gdk::Cursor* fader_cursor;
	static Gdk::Cursor* speaker_cursor;
	static Gdk::Cursor* wait_cursor;
	static Gdk::Cursor* timebar_cursor;

	static void build_cursors ();

	sigc::connection scroll_connection;
	nframes_t last_update_frame;
	void center_screen (nframes_t);
	void center_screen_internal (nframes_t, float);
	
	void update_current_screen ();
	
	void session_going_away ();

	nframes_t cut_buffer_start;
	nframes_t cut_buffer_length;

	bool typed_event (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool button_press_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool button_release_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool motion_handler (ArdourCanvas::Item*, GdkEvent*, ItemType, bool from_autoscroll = false);
	bool enter_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);
	bool leave_handler (ArdourCanvas::Item*, GdkEvent*, ItemType);
	
	/* KEYMAP HANDLING */

	void register_actions ();

	int ensure_cursor (nframes_t* pos);

	void handle_new_audio_region (boost::weak_ptr<ARDOUR::AudioRegion>);
	void handle_audio_region_removed (boost::weak_ptr<ARDOUR::AudioRegion>);
	void add_audio_region_to_region_display (boost::shared_ptr<ARDOUR::AudioRegion>);
	void region_hidden (boost::shared_ptr<ARDOUR::Region>);
	void redisplay_regions ();
	void insert_into_tmp_audio_regionlist(boost::shared_ptr<ARDOUR::AudioRegion>);

	list<boost::shared_ptr<ARDOUR::AudioRegion> > tmp_audio_region_list;

	void cut_copy (Editing::CutCopyOp);
	void cut_copy_points (Editing::CutCopyOp);
	void cut_copy_regions (Editing::CutCopyOp);
	void cut_copy_ranges (Editing::CutCopyOp);

	void mouse_paste ();
	void paste_internal (nframes_t position, float times);

	/* EDITING OPERATIONS */
	
	void reset_point_selection ();
	void toggle_region_mute ();
	void toggle_region_lock ();
	void toggle_region_opaque ();
	void raise_region ();
	void raise_region_to_top ();
	void lower_region ();
	void lower_region_to_bottom ();
	void split_region ();
	void split_region_at (nframes_t);
	void split_regions_at (nframes_t, RegionSelection&);
	void crop_region_to_selection ();
	void set_a_regions_sync_position (boost::shared_ptr<ARDOUR::Region>, nframes_t);
	void set_region_sync_from_edit_cursor ();
	void remove_region_sync();
	void align_selection (ARDOUR::RegionPoint, nframes_t position);
	void align_selection_relative (ARDOUR::RegionPoint point, nframes_t position);
	void align_region (boost::shared_ptr<ARDOUR::Region>, ARDOUR::RegionPoint point, nframes_t position);
	void align_region_internal (boost::shared_ptr<ARDOUR::Region>, ARDOUR::RegionPoint point, nframes_t position);
	void remove_clicked_region ();
	void destroy_clicked_region ();
	void edit_region ();
	void duplicate_some_regions (RegionSelection&, float times);
	void duplicate_selection (float times);
	void region_fill_selection ();

	void region_fill_track ();
	void audition_playlist_region_standalone (boost::shared_ptr<ARDOUR::Region>);
	void audition_playlist_region_via_route (boost::shared_ptr<ARDOUR::Region>, ARDOUR::Route&);
	void split_multichannel_region();
	void reverse_region ();
	void normalize_region ();
	void denormalize_region ();

	void audition_region_from_region_list ();
	void hide_region_from_region_list ();
	void remove_region_from_region_list ();

	void align (ARDOUR::RegionPoint);
	void align_relative (ARDOUR::RegionPoint);
	void naturalize ();

	void reset_focus ();

	void cut ();
	void copy ();
	void paste (float times);

	int  get_prefix (float&, bool&);

	void keyboard_paste ();
	void keyboard_duplicate_region ();
	void keyboard_duplicate_selection ();
	void keyboard_insert_region_list_selection ();

	void region_from_selection ();
	void create_region_from_selection (std::vector<boost::shared_ptr<ARDOUR::AudioRegion> >&);

	bool region_renamed;
	void rename_region ();
	void rename_region_finished (bool);

	void play_from_start ();
	void play_from_edit_cursor ();
	void play_selected_region ();
	void audition_selected_region ();
	void loop_selected_region ();
	void play_location (ARDOUR::Location&);
	void loop_location (ARDOUR::Location&);

	Editing::ZoomFocus zoom_focus;

	void temporal_zoom_selection ();
	void temporal_zoom_session ();
	void temporal_zoom (gdouble scale);
	void temporal_zoom_by_frame (nframes_t start, nframes_t end, const string & op);
	void temporal_zoom_to_frame (bool coarser, nframes_t frame);

	void amplitude_zoom (gdouble scale);
	void amplitude_zoom_step (bool in);

	void insert_region_list_drag (boost::shared_ptr<ARDOUR::AudioRegion>, int x, int y);
	void insert_region_list_selection (float times);

	void add_external_audio_action (Editing::ImportMode);
	void external_audio_dialog ();
	bool check_multichannel_status (const std::vector<Glib::ustring>& paths);

	SoundFileOmega* sfbrowser;
	
	void bring_in_external_audio (Editing::ImportMode mode,  nframes64_t& pos);
	void do_import (vector<Glib::ustring> paths, Editing::ImportDisposition, Editing::ImportMode mode, ARDOUR::SrcQuality, nframes64_t&);

	void _do_embed (vector<Glib::ustring> paths, Editing::ImportDisposition, Editing::ImportMode mode,  nframes64_t&);
	void do_embed (vector<Glib::ustring> paths, Editing::ImportDisposition, Editing::ImportMode mode,  nframes64_t&);
	bool idle_do_embed (vector<Glib::ustring> paths, Editing::ImportDisposition, Editing::ImportMode mode,  nframes64_t&);

	int  import_sndfiles (vector<Glib::ustring> paths, Editing::ImportMode mode,  ARDOUR::SrcQuality, nframes64_t& pos,
			      int target_regions, int target_tracks, boost::shared_ptr<ARDOUR::AudioTrack>&);
	int  embed_sndfiles (vector<Glib::ustring> paths, bool multiple_files, bool& check_sample_rate, Editing::ImportMode mode, 
			     nframes64_t& pos, int target_regions, int target_tracks, boost::shared_ptr<ARDOUR::AudioTrack>&);

	int add_sources (vector<Glib::ustring> paths, ARDOUR::SourceList& sources, nframes64_t& pos, Editing::ImportMode,
			 int target_regions, int target_tracks, boost::shared_ptr<ARDOUR::AudioTrack>&, bool add_channel_suffix);
	int finish_bringing_in_audio (boost::shared_ptr<ARDOUR::AudioRegion> region, uint32_t, uint32_t,  nframes64_t& pos, Editing::ImportMode mode,
				      boost::shared_ptr<ARDOUR::AudioTrack>& existing_track);

	boost::shared_ptr<ARDOUR::AudioTrack> get_nth_selected_audio_track (int nth) const;

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

#ifdef FFT_ANALYSIS
	AnalysisWindow* analysis_window;
#endif

	/* import specific info */

	ARDOUR::Session::import_status import_status;
	gint import_progress_timeout (void *);
	static void *_import_thread (void *);
	void* import_thread ();

	/* to support this ... */

	void import_audio (bool as_tracks);
	void do_import (vector<Glib::ustring> paths, bool split, bool as_tracks);

	void move_to_start ();
	void move_to_end ();
	void goto_frame ();
	void center_playhead ();
	void center_edit_cursor ();
	void edit_cursor_backward ();
	void edit_cursor_forward ();
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
	void set_selection_from_audio_region ();

	void add_location_from_audio_region ();
	void add_location_from_selection ();
	void set_loop_from_selection (bool play);
	void set_punch_from_selection ();

	void set_loop_range (nframes_t start, nframes_t end, std::string cmd);
	void set_punch_range (nframes_t start, nframes_t end, std::string cmd);

	void add_location_from_playhead_cursor ();

	void reverse_selection ();
	void edit_envelope ();

	void start_scrolling ();
	void stop_scrolling ();

	bool _scrubbing;
	double last_scrub_x;
	int scrubbing_direction;

	void keyboard_selection_begin ();
	void keyboard_selection_finish (bool add);
	bool have_pending_keyboard_selection;
	nframes_t pending_keyboard_selection_start;

	boost::shared_ptr<ARDOUR::Region> select_region_for_operation (int dir, TimeAxisView **tv);
	void extend_selection_to_end_of_region (bool next);
	void extend_selection_to_start_of_region (bool previous);

	Editing::SnapType snap_type;
	Editing::SnapMode snap_mode;
	double snap_threshold;

	void handle_gui_changes (const string &, void *);

	void    hide_all_tracks (bool with_select);

	DragInfo drag_info;
	LineDragInfo current_line_drag_info;

	void start_grab (GdkEvent*, Gdk::Cursor* cursor = 0);
	bool end_grab (ArdourCanvas::Item*, GdkEvent*);
	void swap_grab (ArdourCanvas::Item*, Gdk::Cursor* cursor, uint32_t time);

	Gtk::Menu fade_context_menu;
	void popup_fade_context_menu (int, int, ArdourCanvas::Item*, ItemType);

	void start_fade_in_grab (ArdourCanvas::Item*, GdkEvent*);
	void start_fade_out_grab (ArdourCanvas::Item*, GdkEvent*);
	void fade_in_drag_motion_callback (ArdourCanvas::Item*, GdkEvent*);
	void fade_out_drag_motion_callback (ArdourCanvas::Item*, GdkEvent*);
	void fade_in_drag_finished_callback (ArdourCanvas::Item*, GdkEvent*);
	void fade_out_drag_finished_callback (ArdourCanvas::Item*, GdkEvent*);

	void set_fade_in_shape (ARDOUR::AudioRegion::FadeShape);
	void set_fade_out_shape (ARDOUR::AudioRegion::FadeShape);

	void set_fade_in_active (bool);
	void set_fade_out_active (bool);
	
	std::set<boost::shared_ptr<ARDOUR::Playlist> > motion_frozen_playlists;
	void region_drag_motion_callback (ArdourCanvas::Item*, GdkEvent*);
	void region_drag_finished_callback (ArdourCanvas::Item*, GdkEvent*);

	bool _dragging_playhead;

	void cursor_drag_motion_callback (ArdourCanvas::Item*, GdkEvent*);
	void cursor_drag_finished_callback (ArdourCanvas::Item*, GdkEvent*);
	void marker_drag_motion_callback (ArdourCanvas::Item*, GdkEvent*);
	void marker_drag_finished_callback (ArdourCanvas::Item*, GdkEvent*);
	void control_point_drag_motion_callback (ArdourCanvas::Item*, GdkEvent*);
	void control_point_drag_finished_callback (ArdourCanvas::Item*, GdkEvent*);
	void line_drag_motion_callback (ArdourCanvas::Item*, GdkEvent*);
	void line_drag_finished_callback (ArdourCanvas::Item*, GdkEvent*);

	void tempo_marker_drag_motion_callback (ArdourCanvas::Item*, GdkEvent*);
	void tempo_marker_drag_finished_callback (ArdourCanvas::Item*, GdkEvent*);
	void meter_marker_drag_motion_callback (ArdourCanvas::Item*, GdkEvent*);
	void meter_marker_drag_finished_callback (ArdourCanvas::Item*, GdkEvent*);

	gint mouse_rename_region (ArdourCanvas::Item*, GdkEvent*);

	void start_region_grab (ArdourCanvas::Item*, GdkEvent*);
	void start_region_copy_grab (ArdourCanvas::Item*, GdkEvent*);
	void start_region_brush_grab (ArdourCanvas::Item*, GdkEvent*);
	void start_selection_grab (ArdourCanvas::Item*, GdkEvent*);
	void start_cursor_grab (ArdourCanvas::Item*, GdkEvent*);
	void start_marker_grab (ArdourCanvas::Item*, GdkEvent*);
	void start_control_point_grab (ArdourCanvas::Item*, GdkEvent*);
	void start_line_grab_from_regionview (ArdourCanvas::Item*, GdkEvent*);
	void start_line_grab_from_line (ArdourCanvas::Item*, GdkEvent*);
	void start_line_grab (AutomationLine *, GdkEvent*);
	void start_tempo_marker_grab (ArdourCanvas::Item*, GdkEvent*);
	void start_tempo_marker_copy_grab (ArdourCanvas::Item*, GdkEvent*);
	void start_meter_marker_grab (ArdourCanvas::Item*, GdkEvent*);
	void start_meter_marker_copy_grab (ArdourCanvas::Item*, GdkEvent*);

	void region_view_item_click (AudioRegionView&, GdkEventButton*);

	void remove_gain_control_point (ArdourCanvas::Item*, GdkEvent*);
	void remove_control_point (ArdourCanvas::Item*, GdkEvent*);

	void mouse_brush_insert_region (RegionView*, nframes_t pos);
	void brush (nframes_t);

	void show_verbose_time_cursor (nframes_t frame, double offset = 0, double xpos=-1, double ypos=-1);
	void show_verbose_duration_cursor (nframes_t start, nframes_t end, double offset = 0, double xpos=-1, double ypos=-1);

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
	bool canvas_region_view_event (GdkEvent* event,ArdourCanvas::Item*, RegionView*);
	bool canvas_region_view_name_highlight_event (GdkEvent* event,ArdourCanvas::Item*, RegionView*);
	bool canvas_region_view_name_event (GdkEvent* event,ArdourCanvas::Item*, RegionView*);
	bool canvas_stream_view_event (GdkEvent* event,ArdourCanvas::Item*, RouteTimeAxisView*);
	bool canvas_marker_event (GdkEvent* event,ArdourCanvas::Item*, Marker*);
	bool canvas_zoom_rect_event (GdkEvent* event,ArdourCanvas::Item*);
	bool canvas_tempo_marker_event (GdkEvent* event,ArdourCanvas::Item*, TempoMarker*);
	bool canvas_meter_marker_event (GdkEvent* event,ArdourCanvas::Item*, MeterMarker*);
	bool canvas_automation_track_event(GdkEvent* event, ArdourCanvas::Item*, AutomationTimeAxisView*) ;

	bool canvas_tempo_bar_event (GdkEvent* event, ArdourCanvas::Item*);
	bool canvas_meter_bar_event (GdkEvent* event, ArdourCanvas::Item*);
	bool canvas_marker_bar_event (GdkEvent* event, ArdourCanvas::Item*);
	bool canvas_range_marker_bar_event (GdkEvent* event, ArdourCanvas::Item*);
	bool canvas_transport_marker_bar_event (GdkEvent* event, ArdourCanvas::Item*);

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
	bool canvas_edit_cursor_event (GdkEvent* event, ArdourCanvas::Item*);
	bool track_canvas_scroll (GdkEventScroll* event);

	bool track_canvas_scroll_event (GdkEventScroll* event);
	bool track_canvas_button_press_event (GdkEventButton* event);
	bool track_canvas_button_release_event (GdkEventButton* event);
	bool track_canvas_motion_notify_event (GdkEventMotion* event);

	Gtk::Allocation canvas_allocation;
	bool canvas_idle_queued;
	void track_canvas_allocate (Gtk::Allocation alloc);
	bool track_canvas_size_allocated ();

	void set_edit_cursor (GdkEvent* event);
	void set_playhead_cursor (GdkEvent* event);

	void kbd_driver (sigc::slot<void,GdkEvent*>, bool use_track_canvas = true, bool use_time_canvas = true, bool can_select = true);
	void kbd_set_playhead_cursor ();
	void kbd_set_edit_cursor ();
	void kbd_mute_unmute_region ();
	void kbd_split ();
	void kbd_set_sync_position ();
	void kbd_align (ARDOUR::RegionPoint);
	void kbd_align_relative (ARDOUR::RegionPoint);
	void kbd_brush ();
	void kbd_audition ();

	void kbd_do_split (GdkEvent*);
	void kbd_do_set_sync_position (GdkEvent* ev);
	void kbd_do_align (GdkEvent*, ARDOUR::RegionPoint);
	void kbd_do_align_relative (GdkEvent*, ARDOUR::RegionPoint);
	void kbd_do_brush (GdkEvent*);
	void kbd_do_audition (GdkEvent*);

	void handle_new_duration ();
	void initialize_canvas ();
	void reset_scrolling_region (Gtk::Allocation* alloc = 0);

	/* display control */
	
	bool _show_measures;
	bool _show_waveforms;
	bool _follow_playhead;
	bool _show_waveforms_recording;
	
	ARDOUR::TempoMap::BBTPointList *current_bbt_points;
	
	typedef vector<ArdourCanvas::SimpleLine*> TimeLineList;
	TimeLineList free_measure_lines;
	TimeLineList used_measure_lines;

	ArdourCanvas::Group* time_line_group;
	ArdourCanvas::SimpleLine* get_time_line ();
	void hide_measures ();
	void draw_measures ();
	bool lazy_hide_and_draw_measures ();

	void new_tempo_section ();

	void mouse_add_new_tempo_event (nframes_t where);
	void mouse_add_new_meter_event (nframes_t where);

	void remove_tempo_marker (ArdourCanvas::Item*);
	void remove_meter_marker (ArdourCanvas::Item*);
	gint real_remove_tempo_marker (ARDOUR::TempoSection*);
	gint real_remove_meter_marker (ARDOUR::MeterSection*);
	
	void edit_tempo_section (ARDOUR::TempoSection*);
	void edit_meter_section (ARDOUR::MeterSection*);
	void edit_tempo_marker (ArdourCanvas::Item*);
	void edit_meter_marker (ArdourCanvas::Item*);
	
	void marker_menu_edit ();
	void marker_menu_remove ();
	void marker_menu_rename ();
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
	void new_transport_marker_menu_set_loop ();
	void new_transport_marker_menu_set_punch ();
	void update_loop_range_view (bool visibility=false);
	void update_punch_range_view (bool visibility=false);
        gint new_transport_marker_menu_popdown (GdkEventAny*);
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
	ArdourCanvas::Item* marker_menu_item;

	typedef list<Marker*> Marks;
	Marks metric_marks;

	void remove_metric_marks ();
	void draw_metric_marks (const ARDOUR::Metrics& metrics);

	void tempo_map_changed (ARDOUR::Change);
	void redisplay_tempo (bool immediate_redraw);
	
	void snap_to (nframes64_t& first, int32_t direction = 0, bool for_mark = false);
	void snap_to (nframes_t& first, int32_t direction = 0, bool for_mark = false) {
		/* XXX remove this function when everything moves to 64 bit frame counts */
		nframes64_t first64 = first;
		snap_to (first64, direction, for_mark);
		first = (nframes_t) first64;
	}

	uint32_t bbt_beat_subdivision;

	/* toolbar */
	
	Gtk::ToggleButton        editor_mixer_button;

	void editor_mixer_button_toggled ();

	AudioClock               edit_cursor_clock;
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
	GroupedButtons          *mouse_mode_button_set;
	void                     mouse_mode_toggled (Editing::MouseMode m);
	bool                     ignore_mouse_mode_toggle;

	gint                     mouse_select_button_release (GdkEventButton*);

	Gtk::VBox                automation_box;
	Gtk::Button              automation_mode_button;
	Gtk::ToggleButton        global_automation_button;

	Gtk::ComboBoxText edit_mode_selector;
	Gtk::VBox         edit_mode_box;

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

	void                zoom_adjustment_changed();

	void                edit_cursor_clock_changed();
	
	void setup_toolbar ();

	Gtkmm2ext::TearOff*       tools_tearoff;
	Gtk::HBox                toolbar_hbox;
	Gtk::EventBox            toolbar_base;
	Gtk::Frame               toolbar_frame;

	/* selection process */

	Selection* selection;
	Selection* cut_buffer;

	void time_selection_changed ();
	void track_selection_changed ();
	void region_selection_changed ();
	void point_selection_changed ();
	void marker_selection_changed ();

	enum SelectionOp {
		CreateSelection,
		SelectionStartTrim,
		SelectionEndTrim,
		SelectionMove
	} selection_op;

	void start_selection_op (ArdourCanvas::Item* item, GdkEvent* event, SelectionOp);
	void drag_selection (ArdourCanvas::Item* item, GdkEvent* event);
	void end_selection_op (ArdourCanvas::Item* item, GdkEvent* event);
	void cancel_selection ();

	void region_selection_op (void (ARDOUR::Region::*pmf)(void));
	void region_selection_op (void (ARDOUR::Region::*pmf)(void*), void*);
	void region_selection_op (void (ARDOUR::Region::*pmf)(bool), bool);

	bool audio_region_selection_covers (nframes_t where);

	/* transport range select process */
	enum RangeMarkerOp {
		CreateRangeMarker,
		CreateTransportMarker
	} range_marker_op;

	void start_range_markerbar_op (ArdourCanvas::Item* item, GdkEvent* event, RangeMarkerOp);
	void drag_range_markerbar_op (ArdourCanvas::Item* item, GdkEvent* event);
	void end_range_markerbar_op (ArdourCanvas::Item* item, GdkEvent* event);

	ArdourCanvas::SimpleRect*  range_bar_drag_rect;
	ArdourCanvas::SimpleRect*  transport_bar_drag_rect;
	ArdourCanvas::Line*        marker_drag_line;
 	ArdourCanvas::Points       marker_drag_line_points;
	ArdourCanvas::SimpleRect*  range_marker_drag_rect;

	void update_marker_drag_item (ARDOUR::Location *);
	
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
	
	void start_rubberband_select (ArdourCanvas::Item* item, GdkEvent* event);
	void drag_rubberband_select (ArdourCanvas::Item* item, GdkEvent* event);
	void end_rubberband_select (ArdourCanvas::Item* item, GdkEvent* event);

	bool select_all_within (nframes_t start, nframes_t end, gdouble topy, gdouble boty, const TrackViewList&, Selection::Operation op);
	
	ArdourCanvas::SimpleRect   *rubberband_rect;
	
	/* mouse zoom process */

	void start_mouse_zoom (ArdourCanvas::Item* item, GdkEvent* event);
	void drag_mouse_zoom (ArdourCanvas::Item* item, GdkEvent* event);
	void end_mouse_zoom (ArdourCanvas::Item* item, GdkEvent* event);

	ArdourCanvas::SimpleRect   *zoom_rect;
	void reposition_zoom_rect (nframes_t start, nframes_t end);
	
	/* diskstream/route display management */

	struct RouteDisplayModelColumns : public Gtk::TreeModel::ColumnRecord {
	    RouteDisplayModelColumns() { 
		    add (text);
		    add (visible);
		    add (tv);
		    add (route);
	    }
	    Gtk::TreeModelColumn<Glib::ustring>  text;
	    Gtk::TreeModelColumn<bool>           visible;
	    Gtk::TreeModelColumn<TimeAxisView*>  tv;
	    Gtk::TreeModelColumn<boost::shared_ptr<ARDOUR::Route> >  route;
	};

	RouteDisplayModelColumns         route_display_columns;
	Glib::RefPtr<Gtk::ListStore>     route_display_model;
	Glib::RefPtr<Gtk::TreeSelection> route_display_selection;

	Gtkmm2ext::DnDTreeView<boost::shared_ptr<ARDOUR::Route> > route_list_display; 
	Gtk::ScrolledWindow                   route_list_scroller;
	Gtk::Menu*                            route_list_menu;

	void sync_order_keys ();
	bool ignore_route_order_sync;

	bool route_list_display_button_press (GdkEventButton*);
	bool route_list_selection_filter (const Glib::RefPtr<Gtk::TreeModel>& model, const Gtk::TreeModel::Path& path, bool yn);

	void route_list_change (const Gtk::TreeModel::Path&,const Gtk::TreeModel::iterator&);
	void route_list_delete (const Gtk::TreeModel::Path&);
	void track_list_reorder (const Gtk::TreeModel::Path& path, const Gtk::TreeModel::iterator& iter, int* new_order);

	void initial_route_list_display ();
	void redisplay_route_list();
	bool ignore_route_list_reorder;
	bool no_route_list_redisplay;

	void build_route_list_menu ();
	void show_route_list_menu ();

	void show_all_routes ();
	void hide_all_routes ();
	void show_all_audiotracks ();
	void hide_all_audiotracks ();
	void show_all_audiobus ();
	void hide_all_audiobus ();

	void set_all_tracks_visibility (bool yn);
	void set_all_audio_visibility (int type, bool yn);

	/* edit group management */

        struct GroupListModelColumns : public Gtk::TreeModel::ColumnRecord {
                GroupListModelColumns () {
		       add (is_active);
		       add (is_visible);
                       add (text);
		       add (routegroup);
                }
	        Gtk::TreeModelColumn<bool> is_active;
	        Gtk::TreeModelColumn<bool> is_visible;
	        Gtk::TreeModelColumn<std::string> text;
	        Gtk::TreeModelColumn<ARDOUR::RouteGroup*>   routegroup;
	};

	GroupListModelColumns group_columns;
	Glib::RefPtr<Gtk::ListStore> group_model;
	Glib::RefPtr<Gtk::TreeSelection> group_selection;

	Gtk::TreeView          edit_group_display;
	Gtk::ScrolledWindow    edit_group_display_scroller;
	Gtk::Menu*             edit_group_list_menu;

	void build_edit_group_list_menu ();
	void activate_all_edit_groups ();
	void disable_all_edit_groups ();

	bool in_edit_group_row_change;
	void edit_group_row_change (const Gtk::TreeModel::Path&,const Gtk::TreeModel::iterator&);
	void edit_group_name_edit (const Glib::ustring&, const Glib::ustring&);
	void new_edit_group ();
	void edit_group_list_button_clicked ();
	gint edit_group_list_button_press_event (GdkEventButton* ev);
	void add_edit_group (ARDOUR::RouteGroup* group);
	void remove_selected_edit_group ();
	void edit_groups_changed ();
	void group_flags_changed (void*, ARDOUR::RouteGroup*);

	Gtk::VBox           list_vpacker;

	/* autoscrolling */

	bool autoscroll_active;
	int autoscroll_timeout_tag;
	int autoscroll_direction;
	uint32_t autoscroll_cnt;
	nframes_t autoscroll_distance;
     
	static gint _autoscroll_canvas (void *);
	bool autoscroll_canvas ();
	void start_canvas_autoscroll (int direction);
	void stop_canvas_autoscroll ();
	void maybe_autoscroll (GdkEvent*);

	/* trimming */
	enum TrimOp {
		StartTrim,
		EndTrim,
		ContentsTrim,
	} trim_op;

	void start_trim (ArdourCanvas::Item*, GdkEvent*);
	void point_trim (GdkEvent*);
	void trim_motion_callback (ArdourCanvas::Item*, GdkEvent*);
	void single_contents_trim (RegionView&, nframes_t, bool, bool, bool);
	void single_start_trim (RegionView&, nframes_t, bool, bool);
	void single_end_trim (RegionView&, nframes_t, bool, bool);

	void trim_finished_callback (ArdourCanvas::Item*, GdkEvent*);
	void thaw_region_after_trim (RegionView& rv);
	
	void trim_region_to_edit_cursor ();
	void trim_region_from_edit_cursor ();

	bool show_gain_after_trim;

	/* Drag-n-Drop */

	int convert_drop_to_paths (std::vector<Glib::ustring>& paths,
				   const Glib::RefPtr<Gdk::DragContext>& context,
				   gint                x,
				   gint                y,
				   const Gtk::SelectionData& data,
				   guint               info,
				   guint               time);

	void  track_canvas_drag_data_received  (const Glib::RefPtr<Gdk::DragContext>& context,
						gint                x,
						gint                y,
						const Gtk::SelectionData& data,
						guint               info,
						guint               time);
	
	void  region_list_display_drag_data_received  (const Glib::RefPtr<Gdk::DragContext>& context,
						       gint                x,
						       gint                y,
						       const Gtk::SelectionData& data,
						       guint               info,
						       guint               time);


	void  drop_paths  (const Glib::RefPtr<Gdk::DragContext>& context,
			   gint                x,
			   gint                y,
			   const Gtk::SelectionData& data,
			   guint               info,
			   guint               time);

	void  drop_regions  (const Glib::RefPtr<Gdk::DragContext>& context,
			     gint                x,
			     gint                y,
			     const Gtk::SelectionData& data,
			     guint               info,
			     guint               time);

	/* audio export */

	ExportDialog *export_dialog;
	ExportDialog *export_range_markers_dialog;
	
	void export_range (nframes_t start, nframes_t end);
	void export_range_markers ();

	int  write_region_selection(RegionSelection&);
	bool write_region (string path, boost::shared_ptr<ARDOUR::AudioRegion>);
	void export_region ();
	void bounce_region_selection ();
	void bounce_range_selection ();
	void external_edit_region ();

	int write_audio_selection (TimeSelection&);
	bool write_audio_range (ARDOUR::AudioPlaylist&, uint32_t channels, list<ARDOUR::AudioRange>&);

	void write_selection ();

	/* history */

	UndoAction get_memento() const;

        XMLNode *before; /* used in *_reversible_command */
	void begin_reversible_command (string cmd_name);
	void commit_reversible_command ();

	/* visual history */

	UndoHistory visual_history;
	UndoTransaction current_visual_command;


	void update_title ();	
	void update_title_s (const string & snapshot_name);

	struct State {
	    Selection* selection;
	    double     frames_per_unit;

	    State();
	    ~State();
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

	/* edit-group solo + mute */

	void set_edit_group_solo (ARDOUR::Route&, bool);
	void set_edit_group_mute (ARDOUR::Route&, bool);

	/* duplication */

	void duplicate_dialog (bool for_region);
	
	nframes64_t event_frame (GdkEvent*, double* px = 0, double* py = 0) const;

	/* returns false if mouse pointer is not in track or marker canvas
	 */
	bool mouse_frame (nframes64_t&, bool& in_track_canvas) const;

	void time_fx_motion (ArdourCanvas::Item*, GdkEvent*);
	void start_time_fx (ArdourCanvas::Item*, GdkEvent*);
	void end_time_fx (ArdourCanvas::Item*, GdkEvent*);

	struct TimeStretchDialog : public ArdourDialog {
	    ARDOUR::TimeStretchRequest request;
	    Editor&               editor;
	    RegionSelection       regions;
	    Gtk::ProgressBar      progress_bar;
	    Gtk::ToggleButton     quick_button;
	    Gtk::ToggleButton     antialias_button;
	    Gtk::Button*          cancel_button;
	    Gtk::Button*          action_button;
	    Gtk::HBox             upper_button_box;
	    Gtk::VBox             packer;
	    int                   status;

	    TimeStretchDialog (Editor& e);

	    gint update_progress ();
	    sigc::connection first_cancel;
	    sigc::connection first_delete;
	    void cancel_timestretch_in_progress ();
	    gint delete_timestretch_in_progress (GdkEventAny*);
	};

	/* "whats mine is yours" */

	friend class TimeStretchDialog;

	TimeStretchDialog* current_timestretch;

	static void* timestretch_thread (void *arg);
	int run_timestretch (RegionSelection&, float fraction);
	void do_timestretch (TimeStretchDialog&);

	/* editor-mixer strip */

	MixerStrip *current_mixer_strip;
	bool show_editor_mixer_when_tracks_arrive;
	Gtk::VBox current_mixer_strip_vbox;
	void cms_deleted ();
	void current_mixer_strip_hidden ();
	void current_mixer_strip_removed ();

	void detach_tearoff (Gtk::Box* b, Gtk::Window* w);
	void reattach_tearoff (Gtk::Box* b, Gtk::Window* w, int32_t n);

	/* nudging tracks */

	void nudge_track (bool use_edit_cursor, bool forwards);

	/* xfades */

	bool _xfade_visibility;
	
	/* <CMT Additions> */
	void handle_new_imageframe_time_axis_view(const string & track_name, void* src) ;
	void handle_new_imageframe_marker_time_axis_view(const string & track_name, TimeAxisView* marked_track) ;

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
	/* </CMT Additions> */

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

	nframes_t get_nudge_distance (nframes_t pos, nframes_t& next);
	
	/* audio filters */

	void apply_filter (ARDOUR::AudioFilter&, string cmd);

	/* handling cleanup */

	int playlist_deletion_dialog (boost::shared_ptr<ARDOUR::Playlist>);

	vector<sigc::connection> session_connections;

	/* tracking step changes of track height */

	TimeAxisView* current_stepping_trackview;
	struct timeval last_track_height_step_timestamp;
	gint track_height_step_timeout();
	sigc::connection step_timeout;

	TimeAxisView* entered_track;
	RegionView*   entered_regionview;
	bool clear_entered_track;
	gint left_track_canvas (GdkEventCrossing*);
	void set_entered_track (TimeAxisView*);
	void set_entered_regionview (RegionView*);
	gint left_automation_track ();

	bool _new_regionviews_show_envelope;

	void toggle_gain_envelope_visibility ();
	void toggle_gain_envelope_active ();
	void reset_region_gain_envelopes ();

	Gtk::CheckMenuItem* region_envelope_visible_item;
	Gtk::CheckMenuItem* region_envelope_active_item;
	Gtk::CheckMenuItem* region_mute_item;
	Gtk::CheckMenuItem* region_lock_item;
	Gtk::CheckMenuItem* region_opaque_item;
	
	bool on_key_press_event (GdkEventKey*);

	void session_state_saved (string);

	Glib::RefPtr<Gtk::Action>              undo_action;
	Glib::RefPtr<Gtk::Action>              redo_action;

	void history_changed ();
	void color_handler ();
	
	Gtk::HBox      status_bar_hpacker;

	Editing::EditPoint _edit_point;

	Gtk::ComboBoxText edit_point_selector;

	void set_edit_point (Editing::EditPoint ep);
	void edit_point_selection_done ();
	void edit_point_chosen (Editing::EditPoint);
	Glib::RefPtr<Gtk::RadioAction> edit_point_action (Editing::EditPoint);
	std::vector<std::string> edit_point_strings;
};

#endif /* __ardour_editor_h__ */
