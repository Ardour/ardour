#ifndef __gtk_ardour_public_editor_h__
#define __gtk_ardour_public_editor_h__

#include <map>

#include <string>
#include <glib.h>
#include <gdk/gdktypes.h>
#include <libgnomecanvas/libgnomecanvas.h>
#include <gtkmm/window.h>
#include <jack/types.h>
#include <sigc++/signal.h>

#include "editing.h"
#include "keyboard_target.h"

namespace ARDOUR {
	class Session;
	class AudioExportSpecification;
	class Region;
	class Playlist;
	class RouteGroup;
}

namespace Gtk {
	class Container;
	class Menu;
}

class Editor;
class TimeAxisViewItem;
class TimeAxisView;
class PluginSelector;
class PlaylistSelector;
class XMLNode;
class Selection;

class PublicEditor : public Gtk::Window, public Stateful, public KeyboardTarget {
  public:
	PublicEditor();
	virtual ~PublicEditor();

	typedef list<TimeAxisView *> TrackViewList;

	static PublicEditor* instance() { return _instance; }

	virtual void             connect_to_session (ARDOUR::Session*) = 0;
	virtual ARDOUR::Session* current_session() const = 0;
	virtual void set_snap_to (Editing::SnapType) = 0;
	virtual void set_snap_mode (Editing::SnapMode) = 0;
	virtual void set_snap_threshold (double) = 0;
	virtual void undo (uint32_t n = 1) = 0;
	virtual void redo (uint32_t n = 1) = 0;
	virtual void set_mouse_mode (Editing::MouseMode, bool force = false) = 0;
	virtual void step_mouse_mode (bool next) = 0;
	virtual Editing::MouseMode current_mouse_mode () = 0;
	virtual void add_imageframe_time_axis(std::string track_name, void*)  = 0;
	virtual void add_imageframe_marker_time_axis(std::string track_name, TimeAxisView* marked_track, void*)  = 0;
	virtual void connect_to_image_compositor()  = 0;
	virtual void scroll_timeaxis_to_imageframe_item(const TimeAxisViewItem* item)  = 0;
	virtual TimeAxisView* get_named_time_axis(std::string name)  = 0;
	virtual void consider_auditioning (ARDOUR::Region&) = 0;
	virtual void set_show_waveforms (bool yn) = 0;
	virtual bool show_waveforms() const = 0;
	virtual void set_show_waveforms_recording (bool yn) = 0;
	virtual bool show_waveforms_recording() const = 0;
	virtual void new_region_from_selection () = 0;
	virtual void separate_region_from_selection () = 0;
	virtual void toggle_playback (bool with_abort) = 0;
	virtual void set_edit_menu (Gtk::Menu&) = 0;
	virtual jack_nframes_t unit_to_frame (double unit) = 0;
	virtual double frame_to_unit (jack_nframes_t frame) = 0;
	virtual double frame_to_unit (double frame) = 0;
	virtual jack_nframes_t pixel_to_frame (double pixel) = 0;
	virtual gulong frame_to_pixel (jack_nframes_t frame) = 0;
	virtual Selection& get_selection() const = 0;
	virtual Selection& get_cut_buffer() const = 0;
	virtual void play_selection () = 0;
	virtual void set_show_measures (bool yn) = 0;
	virtual bool show_measures () const = 0;
	virtual void export_session() = 0;
	virtual void export_selection() = 0;
	virtual void add_toplevel_controls (Gtk::Container&) = 0;
	virtual void      set_zoom_focus (Editing::ZoomFocus) = 0;
	virtual Editing::ZoomFocus get_zoom_focus () const = 0;
	virtual gdouble   get_current_zoom () = 0;
	virtual PlaylistSelector& playlist_selector() const = 0;
	virtual void route_name_changed (TimeAxisView *) = 0;
	virtual void clear_playlist (ARDOUR::Playlist&) = 0;
	virtual void set_selected_mixer_strip (TimeAxisView&) = 0;
	virtual void unselect_strip_in_display (TimeAxisView& tv) = 0;
	virtual void set_follow_playhead (bool yn) = 0;
	virtual void toggle_follow_playhead () = 0;
	virtual bool follow_playhead() const = 0;
	virtual void toggle_xfade_visibility () = 0;
	virtual void set_xfade_visibility (bool yn) = 0;
	virtual bool xfade_visibility() const = 0;
	virtual void ensure_float (Gtk::Window&) = 0;
	virtual void show_window () = 0;
	virtual TrackViewList* get_valid_views (TimeAxisView*, ARDOUR::RouteGroup* grp = 0) = 0;
	virtual jack_nframes_t leftmost_position() const = 0;
	virtual jack_nframes_t current_page_frames() const = 0;
	virtual void temporal_zoom_step (bool coarser) = 0;
	virtual void scroll_tracks_down_line () = 0;
	virtual void scroll_tracks_up_line () = 0;
	virtual bool new_regionviews_display_gain () = 0;
	virtual void prepare_for_cleanup () = 0;
	virtual void reposition_x_origin (jack_nframes_t frame) = 0;
	virtual void remove_last_capture () = 0;

	sigc::signal<void,Editing::DisplayControl> DisplayControlChanged;
	sigc::signal<void> ZoomFocusChanged;
	sigc::signal<void> ZoomChanged;
	sigc::signal<void> XOriginChanged;
	sigc::signal<void> Resized;

	static gint canvas_crossfade_view_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data);
	static gint canvas_fade_in_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data);
	static gint canvas_fade_in_handle_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data);
	static gint canvas_fade_out_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data);
	static gint canvas_fade_out_handle_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data);
	static gint canvas_region_view_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data);
	static gint canvas_region_view_name_highlight_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data);
	static gint canvas_region_view_name_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data);
	static gint canvas_stream_view_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data);
	static gint canvas_marker_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data);
	static gint canvas_zoom_rect_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data);
	static gint canvas_selection_rect_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data);
	static gint canvas_selection_start_trim_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data);
	static gint canvas_selection_end_trim_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data);
	static gint canvas_control_point_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data);
	static gint canvas_line_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data);
	static gint canvas_tempo_marker_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data);
	static gint canvas_meter_marker_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data);
	static gint canvas_tempo_bar_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data);
	static gint canvas_meter_bar_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data);
	static gint canvas_marker_bar_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data);
	static gint canvas_range_marker_bar_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data);
	static gint canvas_transport_marker_bar_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data);
	static gint canvas_imageframe_item_view_event(GnomeCanvasItem *item, GdkEvent* event, gpointer data) ;
	static gint canvas_imageframe_view_event(GnomeCanvasItem *item, GdkEvent* event, gpointer data) ;
	static gint canvas_imageframe_start_handle_event(GnomeCanvasItem* item, GdkEvent* event, gpointer data) ;
	static gint canvas_imageframe_end_handle_event(GnomeCanvasItem* item, GdkEvent* event, gpointer data) ;
	static gint canvas_marker_time_axis_view_event(GnomeCanvasItem* item, GdkEvent* event, gpointer data) ;
	static gint canvas_markerview_item_view_event(GnomeCanvasItem* item, GdkEvent* event, gpointer data) ;
	static gint canvas_markerview_start_handle_event(GnomeCanvasItem* item, GdkEvent* event, gpointer data) ;
	static gint canvas_markerview_end_handle_event(GnomeCanvasItem* item, GdkEvent* event, gpointer data) ;
	static gint canvas_automation_track_event(GnomeCanvasItem* item, GdkEvent* event, gpointer data) ;

  protected:
	virtual gint _canvas_fade_in_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) = 0;
	virtual gint _canvas_fade_in_handle_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) = 0;
	virtual gint _canvas_fade_out_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) = 0;
	virtual gint _canvas_fade_out_handle_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) = 0;
	virtual gint _canvas_crossfade_view_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) = 0;
	virtual gint _canvas_region_view_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) = 0;
	virtual gint _canvas_region_view_name_highlight_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) = 0;
	virtual gint _canvas_region_view_name_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) = 0;
	virtual gint _canvas_stream_view_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) = 0;
	virtual gint _canvas_marker_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) = 0;
	virtual gint _canvas_zoom_rect_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) = 0;
	virtual gint _canvas_selection_rect_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) = 0;
	virtual gint _canvas_selection_start_trim_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) = 0;
	virtual gint _canvas_selection_end_trim_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) = 0;
	virtual gint _canvas_control_point_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) = 0;
	virtual gint _canvas_line_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) = 0;
	virtual gint _canvas_tempo_marker_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) = 0;
	virtual gint _canvas_meter_marker_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) = 0;
	virtual gint _canvas_tempo_bar_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) = 0;
	virtual gint _canvas_meter_bar_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) = 0;
	virtual gint _canvas_marker_bar_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) = 0;
	virtual gint _canvas_range_marker_bar_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) = 0;
	virtual gint _canvas_transport_marker_bar_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) = 0;
	virtual gint _canvas_imageframe_item_view_event(GnomeCanvasItem *item, GdkEvent* event, gpointer data) = 0;
	virtual gint _canvas_imageframe_view_event(GnomeCanvasItem *item, GdkEvent* event, gpointer data) = 0;
	virtual gint _canvas_imageframe_start_handle_event(GnomeCanvasItem* item, GdkEvent* event, gpointer data) = 0;
	virtual gint _canvas_imageframe_end_handle_event(GnomeCanvasItem* item, GdkEvent* event, gpointer data) = 0;
	virtual gint _canvas_marker_time_axis_view_event(GnomeCanvasItem* item, GdkEvent* event, gpointer data) = 0;
	virtual gint _canvas_markerview_item_view_event(GnomeCanvasItem* item, GdkEvent* event, gpointer data) = 0;
	virtual gint _canvas_markerview_start_handle_event(GnomeCanvasItem* item, GdkEvent* event, gpointer data) = 0;
	virtual gint _canvas_markerview_end_handle_event(GnomeCanvasItem* item, GdkEvent* event, gpointer data) = 0;
	virtual gint _canvas_automation_track_event(GnomeCanvasItem* item, GdkEvent* event, gpointer data) = 0;

	static PublicEditor* _instance;
};

#endif // __gtk_ardour_public_editor_h__ 
