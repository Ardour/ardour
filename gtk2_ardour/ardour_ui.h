/*
    Copyright (C) 1999-2002 Paul Davis

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

#ifndef __ardour_gui_h__
#define __ardour_gui_h__

#include <time.h>

/* need _BSD_SOURCE to get timersub macros */

#ifdef _BSD_SOURCE
#include <sys/time.h>
#else
#define _BSD_SOURCE
#include <sys/time.h>
#undef _BSD_SOURCE
#endif

#include <list>
#include <cmath>


#include "pbd/xml++.h"
#include "pbd/controllable.h"
#include <gtkmm/box.h>
#include <gtkmm/frame.h>
#include <gtkmm/label.h>
#include <gtkmm/table.h>
#include <gtkmm/fixed.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/button.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/treeview.h>
#include <gtkmm/menubar.h>
#include <gtkmm/textbuffer.h>
#include <gtkmm/adjustment.h>
#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/click_box.h>
#include <gtkmm2ext/stateful_button.h>
#include <gtkmm2ext/bindable_button.h>

#include "ardour/ardour.h"
#include "ardour/types.h"
#include "ardour/utils.h"
#include "ardour/plugin.h"
#include "ardour/session_handle.h"
#include "ardour/system_exec.h"

#include "video_timeline.h"

#include "about_dialog.h"
#include "ardour_button.h"
#include "ardour_dialog.h"
#include "ardour_dropdown.h"
#include "ardour_window.h"
#include "editing.h"
#include "session_lock_dialog.h"
#include "tracks_control_panel.h"
#include "marker_inspector_dialog.h"
#include "waves_message_dialog.h"
#include "waves_track_color_dialog.h"
#include "ui_config.h"
#include "enums.h"
#include "visibility_group.h"
#include "window_manager.h"

#include "add_tracks_dialog.h"
#include "add_video_dialog.h"
#include "big_clock_window.h"
#include "bundle_manager.h"
#include "global_port_matrix.h"
#include "keyeditor.h"
#include "location_ui.h"
#include "progress_dialog.h"
#include "rc_option_editor.h"
#include "route_params_ui.h"
#include "session_option_editor.h"
#include "speaker_dialog.h"
#include "theme_manager.h"

#include "waves_ui.h"

#include "timecode/time.h"
#include "time.h"

struct MarkerSelection;
class TrackSelection;
class VideoTimeLine;
class ArdourKeyboard;
class AudioClock;
class ButtonJoiner;
class ConnectionEditor;
class MainClock;
class PublicEditor;
class RCOptionEditor;
class RouteParams_UI;
class SessionDialog;
class SessionOptionEditor;
class ShuttleControl;
class Splash;
class TimeInfoBox;
class MidiTracer;
class NSM_Client;
class LevelMeterHBox;
class GUIObjectState;

namespace Gtkmm2ext {
	class TearOff;
}

namespace ARDOUR {
	class ControlProtocolInfo;
	class IO;
	class Port;
	class Route;
	class RouteGroup;
	class Location;
	class ProcessThread;
}

namespace Gtkmm2ext {
	class TearOff;
}

namespace Gtk {
	class ProgressBar;
}

class ARDOUR_UI : public Gtkmm2ext::UI, public ARDOUR::SessionHandlePtr
{
  public:
        ARDOUR_UI (int *argcp, char **argvp[], const char* localedir);
	~ARDOUR_UI();

	bool run_startup (bool should_be_new, std::string load_template);

	void show_splash ();
	void hide_splash ();

	void launch_chat ();
	void launch_manual ();
	void launch_reference ();
	void show_about ();
	void hide_about ();
	void open_media_folder ();

	void idle_load (const std::string& path);
	void finish();

	int load_session (const std::string& path, const std::string& snapshot, std::string mix_template = std::string());
	bool session_loaded;
	int build_session (const std::string& path, const std::string& snapshot, ARDOUR::BusProfile&);
	bool session_is_new() const { return _session_is_new; }
	std::string format_session_time (framepos_t frame);

	ARDOUR::Session* the_session() { return _session; }

	bool get_smart_mode () const;
	
	int get_session_parameters (bool quit_on_cancel, bool should_be_new = false, std::string load_template = "");
        int  build_session_from_dialog (SessionDialog&, const std::string& session_name, const std::string& session_path);
	bool ask_about_loading_existing_session (const std::string& session_path);

	/// @return true if session was successfully unloaded.
	int unload_session (bool hide_stuff = false);
	void close_session();
    void lock_session ();
    
    int save_session_state (const std::string & name = "", bool pending = false, bool switch_to_it = false);
    void save_session_gui_state ();
	int save_state (const std::string & state_name = "", bool switch_to_it = false);
    void set_session_dirty ();

	static ARDOUR_UI *instance () { return theArdourUI; }
	static UIConfiguration *config () { return ui_config; }

	PublicEditor&	  the_editor(){return *editor;}

	void new_midi_tracer_window ();
	void toggle_editing_space();
	void toggle_keep_tearoffs();

	Gtk::Tooltips& tooltips() { return _tooltips; }

	Gtk::HBox& editor_transport_box() { return _editor_transport_box; }

	static PublicEditor* _instance;
	static sigc::signal<void,bool> Blink;

	/** point_zero_one_seconds -- 10Hz ^= 100ms */
	static sigc::signal<void>      RapidScreenUpdate;

	/** point_zero_something_seconds -- currently 25Hz ^= 40ms */
	static sigc::signal<void>      SuperRapidScreenUpdate;

	/** every_fps -- see set_fps_timeout_connection() 25Hz < x < 120Hz */
	static sigc::signal<void>      FPSUpdate;

	/** Emitted frequently with the audible frame, false, and the edit point as
	 *  parameters respectively.
	 *
	 *  (either RapidScreenUpdate || SuperRapidScreenUpdate - user-config)
	 */
	static sigc::signal<void, framepos_t, bool, framepos_t> Clock;

	static void close_all_dialogs () { CloseAllDialogs(); }
    static sigc::signal<void> CloseAllDialogs;

	XMLNode* editor_settings() const;
	XMLNode* mixer_settings () const;
	XMLNode* keyboard_settings () const;
	XMLNode* tearoff_settings (const char*) const;

	void save_application_state ();
	gboolean configure_handler (GdkEventConfigure* conf);

	void halt_on_xrun_message ();
	void xrun_handler (framepos_t);
	void create_xrun_marker (framepos_t);

    ProgressDialog _progress_dialog;
    
	GUIObjectState* gui_object_state;

	MainClock* primary_clock;
	MainClock* secondary_clock;
	void focus_on_clock ();
	AudioClock*   big_clock;

    WavesButton* _tracks_button;
    void on_tracks_button (WavesButton*);
    WavesButton* _bit_depth_button;
    WavesButton* _frame_rate_button;
    void on_bit_depth_button (WavesButton*);
    void on_frame_rate_button (WavesButton*);
    
    uint32_t _ignore_changes;
    
    WavesDropdown* _sample_rate_dropdown;
    void on_sample_rate_dropdown_item_clicked (WavesDropdown*, int);
    void sample_rate_changed();
    ARDOUR::framecnt_t get_sample_rate () const;
    void populate_sample_rate_dropdown ();
    
    WavesDropdown* _display_format_dropdown;
    void on_display_format_dropdown_item_clicked (WavesDropdown*, int);
    void populate_display_format_dropdown ();
    
    WavesDropdown* _timecode_source_dropdown;
    void on_timecode_source_dropdown_item_clicked (WavesDropdown*, int);
    void populate_timecode_source_dropdown ();
    void update_timecode_source_dropdown_items ();
    void set_mtc_indicator_active (bool set_active);
    void hide_mtc_indicator ();
    
    void update_bit_depth_button ();
    void update_sample_rate_dropdown ();
    void update_frame_rate_button ();
    void update_recent_session_menuitems();

    PBD::ScopedConnectionList update_connections_to_toolbar_buttons;
    
	TimeInfoBox* time_info_box;

	VideoTimeLine *video_timeline;

	void store_clock_modes ();
	void restore_clock_modes ();
	void reset_main_clocks ();

	void synchronize_sync_source_and_video_pullup ();

	void add_route (Gtk::Window* float_window);
    void delete_selected_tracks();
        void add_routes_part_two ();
        void add_routes_thread ();

    void add_audio_track_instantly ();
    
	void add_video (Gtk::Window* float_window);
	void remove_video ();
	void start_video_server_menu (Gtk::Window* float_window);
	bool start_video_server (Gtk::Window* float_window, bool popup_msg);
	void stop_video_server (bool ask_confirm=false);
	void flush_videotimeline_cache (bool localcacheonly=false);

	void session_add_audio_track (
		int input_channels,
		int32_t output_channels,
		ARDOUR::TrackMode mode,
		ARDOUR::RouteGroup* route_group,
		uint32_t how_many,
		std::string const & name_template
		) {

		session_add_audio_route (true, input_channels, output_channels, mode, route_group, how_many, name_template);
	}

	void session_add_audio_bus (int input_channels, int32_t output_channels, ARDOUR::RouteGroup* route_group,
				    uint32_t how_many, std::string const & name_template) {
		session_add_audio_route (false, input_channels, output_channels, ARDOUR::Normal, route_group, how_many, name_template);
	}

	void session_add_midi_track (ARDOUR::RouteGroup* route_group, uint32_t how_many, std::string const & name_template,
				     ARDOUR::PluginInfoPtr instrument) {
		session_add_midi_route (true, route_group, how_many, name_template, instrument);
	}

    void session_add_mixed_track (const ARDOUR::ChanCount& input, const ARDOUR::ChanCount& output, ARDOUR::RouteGroup* route_group, uint32_t how_many, std::string const & name_template,
				    ARDOUR::PluginInfoPtr instrument);

	/*  void session_add_midi_bus () {
		session_add_midi_route (false);
	}*/

    void attach_to_engine ();
	void post_engine ();

    void update_output_operation_mode_buttons();
    
	gint exit_on_main_window_close (GdkEventAny *);

	void maximise_editing_space ();
	void restore_editing_space ();

	void show_ui_prefs ();

	void update_tearoff_visibility ();

	void setup_profile ();
	void setup_tooltips ();

	void set_shuttle_fract (double);

	void get_process_buffers ();
	void drop_process_buffers ();

	void reset_peak_display ();
	void reset_route_peak_display (ARDOUR::Route*);
	void reset_group_peak_display (ARDOUR::RouteGroup*);

    const std::string& announce_string() const { return _announce_string; }

    int disconnect_from_engine ();
    int reconnect_to_engine ();

    void set_sample_format(ARDOUR::SampleFormat sf) {_sample_format = sf;}
    void set_header_format(ARDOUR::HeaderFormat hf) {_header_format = hf;}
    void set_timecode_format(Timecode::TimecodeFormat tc) {_timecode_format = tc;}
    
    bool screen_lock_is_allowed() const;
    void on_lock_button_pressed ();
    void on_lock_session ();
    void on_unlock_session ();
    PBD::Signal0<void> lock_button_was_pressed;
    
    bool session_auto_save_is_allowed() const;
    void update_marker_inspector (MarkerSelection*);
    void update_track_color_dialog (boost::shared_ptr<ARDOUR::Route> route);
    void show_marker_inspector();
    void show_track_color_dialog();

  protected:
	friend class PublicEditor;

	void toggle_auto_play ();
	void toggle_auto_input ();
	void toggle_punch ();
	void unset_dual_punch ();
	bool ignore_dual_punch;
	void toggle_punch_in ();
	void toggle_punch_out ();
	void show_loop_punch_ruler_and_disallow_hide ();
	void reenable_hide_loop_punch_ruler_if_appropriate ();
	void toggle_click ();
        void toggle_audio_midi_setup ();
	void toggle_session_auto_loop ();
	void toggle_multi_out_mode ();
	void toggle_stereo_out_mode ();

	void toggle_rc_options_window ();
	void toggle_session_options_window ();

  private:
	Gtk::Tooltips       _tooltips;
	NSM_Client          *nsm;
	bool                _was_dirty;
    bool                _mixer_on_top;
    bool first_time_engine_run;

    void minimize_window();
    void maximize_window();
	void goto_editor_window ();
	void toggle_mixer_bridge_view ();
	void toggle_meterbridge ();
    void show_meterbridge_view ();

	int  setup_windows ();
	void setup_transport ();
	void setup_clock ();
    
    bool delete_session_files(std::string session_path);

	static ARDOUR_UI *theArdourUI;

    int starting ();

	int  ask_about_saving_session (const std::vector<std::string>& actions);

	/* periodic safety backup, to be precise */
	gint autosave_session();
	void update_autosave();
	sigc::connection _autosave_connection;

	void map_transport_state ();
	int32_t do_engine_start ();

	void engine_halted (const char* reason, bool free_reason);
	void engine_stopped ();
	void engine_running ();

	void use_config ();

	static gint _blink  (void *);
	void blink ();
	gint blink_timeout_tag;
	bool blink_on;
	void start_blinking ();
	void stop_blinking ();

	void about_signal_response(int response);

	Gtk::VBox     top_packer;
	Gtk::VBox     tracks_tools_packer;

	sigc::connection clock_signal_connection;
	void         update_clocks ();
	void         start_clocking ();
	void         stop_clocking ();

	bool main_window_state_event_handler (GdkEventWindowState*, bool window_was_editor);

	void update_transport_clocks (framepos_t pos);
	void record_state_changed ();

	std::list<MidiTracer*> _midi_tracer_windows;

	/* Transport Control */

	Gtk::Frame               transport_frame;
	Gtk::HBox               _editor_transport_box;
	Gtk::HBox                transport_hbox;
	Gtk::Fixed               transport_base;
	Gtk::Fixed               transport_button_base;
	Gtk::Frame               transport_button_frame;
	Gtk::HBox                transport_button_hbox;
	Gtk::VBox                transport_button_vbox;
	Gtk::HBox                transport_option_button_hbox;
	Gtk::VBox                transport_option_button_vbox;

	struct TransportControllable : public PBD::Controllable {
	    enum ToggleType {
		    Roll = 0,
		    Stop,
		    RecordEnable,
		    GotoStart,
		    GotoEnd,
		    AutoLoop,
		    PlaySelection,
	    };

	    TransportControllable (std::string name, ARDOUR_UI&, ToggleType);
	    void set_value (double);
	    double get_value (void) const;

	    ARDOUR_UI& ui;
	    ToggleType type;
	};

	boost::shared_ptr<TransportControllable> roll_controllable;
	boost::shared_ptr<TransportControllable> stop_controllable;
	boost::shared_ptr<TransportControllable> goto_start_controllable;
	boost::shared_ptr<TransportControllable> goto_end_controllable;
	boost::shared_ptr<TransportControllable> auto_loop_controllable;
	boost::shared_ptr<TransportControllable> play_selection_controllable;
	boost::shared_ptr<TransportControllable> rec_controllable;

	void toggle_follow_edits ();

	void set_transport_controllable_state (const XMLNode&);
	XMLNode& get_transport_controllable_state ();

	ArdourButton roll_button;
	ArdourButton stop_button;
	ArdourButton goto_start_button;
	ArdourButton goto_end_button;
	ArdourButton auto_loop_button;
	ArdourButton play_selection_button;
	ArdourButton rec_button;

	void toggle_external_sync ();
	void toggle_time_master ();
	void toggle_video_sync ();

	ShuttleControl* shuttle_box;

	ArdourDropdown auto_return_dropdown;
	Gtk::CheckMenuItem *auto_return_last_locate;
	Gtk::CheckMenuItem *auto_return_range_selection;
	Gtk::CheckMenuItem *auto_return_region_selection;
	Gtk::CheckMenuItem *auto_return_loop;
	Gtk::MenuItem *auto_return_toggle;

	void toggle_auto_return_state (ARDOUR::AutoReturnTarget);
	void toggle_all_auto_return ();
	
	ArdourButton follow_edits_button;
	ArdourButton auto_input_button;
	ArdourButton click_button;
	// NO NEED TO HAVE IT: ArdourButton sync_button;
 
	ArdourButton auditioning_alert_button;
	ArdourButton solo_alert_button;
	ArdourButton feedback_alert_button;

	Gtk::VBox alert_box;
	Gtk::VBox meter_box;
	LevelMeterHBox * editor_meter;
	float            editor_meter_max_peak;
	ArdourButton     editor_meter_peak_display;
	bool             editor_meter_peak_button_release (GdkEventButton*);

	void solo_blink (bool);
	void sync_blink (bool);
	void audition_blink (bool);
	void feedback_blink (bool);

	void soloing_changed (bool);
	void auditioning_changed (bool);
	void _auditioning_changed (bool);
	
	bool solo_alert_press (GdkEventButton* ev);
	bool audition_alert_press (GdkEventButton* ev);
	bool feedback_alert_press (GdkEventButton *);

	void big_clock_value_changed ();
	void primary_clock_value_changed ();
	void secondary_clock_value_changed ();

	/* called by Blink signal */

	void transport_rec_enable_blink (bool onoff);

	Gtk::Menu*        session_popup_menu;

	struct RecentSessionModelColumns : public Gtk::TreeModel::ColumnRecord {
	    RecentSessionModelColumns() {
		    add (visible_name);
		    add (tip);
		    add (fullpath);
	    }
	    Gtk::TreeModelColumn<std::string> visible_name;
	    Gtk::TreeModelColumn<std::string> tip;
	    Gtk::TreeModelColumn<std::string> fullpath;
	};

	RecentSessionModelColumns    recent_session_columns;
	Gtk::TreeView                recent_session_display;
	Glib::RefPtr<Gtk::TreeStore> recent_session_model;

	ArdourDialog*     session_selector_window;
	Gtk::FileChooserDialog* open_session_selector;

	void build_session_selector();
	void redisplay_recent_sessions();
	void recent_session_row_activated (const Gtk::TreePath& path, Gtk::TreeViewColumn* col);

	struct RecentSessionsSorter {
		bool operator() (std::pair<std::string,std::string> a, std::pair<std::string,std::string> b) const {
		    return ARDOUR::cmp_nocase(a.first, b.first) == -1;
	    }
	};
    
    /* opening recent sessions from menuitems */
    void open_recent_session_from_menuitem (unsigned int);

	/* menu bar and associated stuff */

	Gtk::MenuBar* menu_bar;

	void use_menubar_as_top_menubar ();
	void build_menu_bar ();

	void update_disk_space ();
    void update_disk_usage ();

	void update_cpu_load ();
	
	gint every_second ();
	gint every_point_one_seconds ();
	gint every_point_zero_something_seconds ();
	gint every_fps ();

	sigc::connection second_connection;
	sigc::connection point_one_second_connection;
	sigc::connection point_zero_something_second_connection;
	sigc::connection fps_connection;

	void set_fps_timeout_connection ();

	void open_session ();
	void open_recent_session ();
	void save_template ();

	void edit_metadata ();
	void import_metadata ();

	void session_add_audio_route (bool, int32_t, int32_t, ARDOUR::TrackMode, ARDOUR::RouteGroup *, uint32_t, std::string const &);
	void session_add_midi_route (bool, ARDOUR::RouteGroup *, uint32_t, std::string const &, ARDOUR::PluginInfoPtr);

	void set_transport_sensitivity (bool);

	//stuff for ProTools-style numpad
	void transport_numpad_event (int num);
	void transport_numpad_decimal ();
	bool _numpad_locate_happening;
	int _pending_locate_num;
	gint transport_numpad_timeout ();
	sigc::connection _numpad_timeout_connection;

	void transport_goto_nth_marker (int nth);
	void transport_goto_zero ();
	void transport_goto_start ();
	void transport_goto_end ();
	void transport_goto_wallclock ();
	void transport_stop ();
	void transport_record (bool roll);
	void transport_roll ();
	void transport_play_selection();
	void transport_play_preroll(); 
	void transport_forward (int option);
	void transport_rewind (int option);
	void transport_loop ();
	void toggle_roll (bool with_abort, bool roll_out_of_bounded_mode);
	bool trx_record_enable_all_tracks ();

	bool _session_is_new;
	void set_session (ARDOUR::Session *);
	void connect_dependents_to_session (ARDOUR::Session *);
	void we_have_dependents ();

	void setup_session_options ();

	guint32  last_key_press_time;

	void snapshot_session (bool switch_to_it);
	bool save_as_progress_update (float fraction, int64_t cnt, int64_t total, Gtk::Label* label, Gtk::ProgressBar* bar);
	void save_session_as ();
	void rename_session ();
	void setup_order_hint ();

	PublicEditor     *editor;
	int         create_editor ();

    /* Dialogs that can be created via new<T> */

    WM::Proxy<SpeakerDialog> speaker_config_window;
    WM::Proxy<ThemeManager> theme_manager;
    WM::Proxy<KeyEditor> key_editor;
    WM::Proxy<RCOptionEditor> rc_option_editor;
    boost::shared_ptr<AddTracksDialog> _add_tracks_dialog;
    WM::Proxy<About> about;
    WM::Proxy<LocationUIWindow> location_ui;
    WM::Proxy<RouteParams_UI> route_params;
    WM::Proxy<TracksControlPanel> tracks_control_panel;
    WM::Proxy<SessionLockDialog> session_lock_dialog;
    WM::Proxy<MarkerInspectorDialog> marker_inspector_dialog;
    WM::Proxy<WavesTrackColorDialog> track_color_dialog;
    
    /* Windows/Dialogs that require a creator method */

    WM::ProxyWithConstructor<SessionOptionEditor> session_option_editor;
    WM::ProxyWithConstructor<AddVideoDialog> add_video_dialog;
    WM::ProxyWithConstructor<BundleManager> bundle_manager;
    WM::ProxyWithConstructor<BigClockWindow> big_clock_window;
//    WM::ProxyWithConstructor<GlobalPortMatrixWindow> audio_port_matrix;
//    WM::ProxyWithConstructor<GlobalPortMatrixWindow> midi_port_matrix;

    /* creator methods */

    SessionOptionEditor*    create_session_option_editor ();
    BundleManager*          create_bundle_manager ();
    AddVideoDialog*         create_add_video_dialog ();
    BigClockWindow*         create_big_clock_window(); 
    GlobalPortMatrixWindow* create_global_port_matrix (ARDOUR::DataType);

	static UIConfiguration *ui_config;

	ARDOUR::SystemExec *video_server_process;

	void handle_locations_change (ARDOUR::Location*);

	/* Keyboard Handling */

	ArdourKeyboard* keyboard;

	/* Keymap handling */

	void install_actions ();

	void toggle_record_enable (uint32_t);

	uint32_t rec_enabled_streams;
	void count_recenabled_streams (ARDOUR::Route&);

	Splash* splash;

	void pop_back_splash (Gtk::Window&);

	/* cleanup */

	Gtk::MenuItem *cleanup_item;

	void display_cleanup_results (ARDOUR::CleanupReport& rep, const gchar* list_title, const bool msg_delete);
	void cleanup ();
    void open_dead_folder ();
	void flush_trash ();

	bool have_configure_timeout;
	ARDOUR::microseconds_t last_configure_time;
	gint configure_timeout ();

	ARDOUR::microseconds_t last_peak_grab;
	ARDOUR::microseconds_t last_shuttle_request;

	bool have_disk_speed_dialog_displayed;
	void disk_speed_dialog_gone (int ignored_response, WavesMessageDialog*);
	void disk_overrun_handler ();
	void disk_underrun_handler ();
	void gui_idle_handler ();

	void cancel_plugin_scan ();
	void cancel_plugin_timeout ();
	void plugin_scan_dialog (std::string type, std::string plugin, bool);
	void plugin_scan_timeout (int);

    void session_format_mismatch (std::string, std::string);

	void session_dialog (std::string);
	int pending_state_dialog ();
	int sr_mismatch_dialog (ARDOUR::framecnt_t, ARDOUR::framecnt_t);

	Gtk::MenuItem* jack_disconnect_item;
	Gtk::MenuItem* jack_reconnect_item;
	Gtk::Menu*     jack_bufsize_menu;

	Glib::RefPtr<Gtk::ActionGroup> common_actions;

	void editor_realized ();

	std::vector<std::string> positional_sync_strings;

	void toggle_send_midi_feedback ();
	void toggle_use_mmc ();
	void toggle_send_mmc ();
	void toggle_send_mtc ();
	void toggle_send_midi_clock ();

	void toggle_use_osc ();

	void parameter_changed (std::string);
	void session_parameter_changed (const std::string& param);

	bool first_idle ();

	void check_memory_locking ();

	bool check_audioengine();
	void audioengine_setup ();

	void loading_message (const std::string& msg);

	PBD::ScopedConnectionList forever_connections;
    PBD::ScopedConnection halt_connection; 

    void step_edit_status_change (bool);

	void platform_specific ();
	void platform_setup ();

	/* these are used only in response to a platform-specific "ShouldQuit" signal
	 */
	bool idle_finish ();
	void queue_finish ();
	void fontconfig_dialog ();

    int missing_file (ARDOUR::Session*s, std::string str, ARDOUR::DataType type);
    int ambiguous_file (std::string file, std::vector<std::string> hits);

    bool transport_button_press (GdkEventButton*, Glib::RefPtr<Gtk::Action>&);
   
	bool click_button_clicked (GdkEventButton *);

	Gtk::Adjustment* _dsp_load_adjustment;
    Gtk::Adjustment* _hd_load_adjustment;
    Gtk::Label* _dsp_load_label;
    Gtk::Label* _hd_load_label;
    Gtk::Label* _hd_remained_time_label;
        
    Gtk::Image* _mtc_idle_icon;
    Gtk::Image* _mtc_sync_icon;

	/** A ProcessThread so that we have some thread-local buffers for use by
	 *  PluginEqGui::impulse_analysis ().
	 */
	ARDOUR::ProcessThread* _process_thread;

	void feedback_detected ();

	ArdourButton             midi_panic_button;
	void                     midi_panic ();

	void successful_graph_sort ();
	bool _feedback_exists;

    std::string _announce_string;
    void check_announcements ();

	int do_engine_setup(ARDOUR::framecnt_t desired_sample_rate);
    
    ARDOUR::SampleFormat _sample_format;
    ARDOUR::HeaderFormat _header_format;
    Timecode::TimecodeFormat _timecode_format;
    
    void sync_displays_format (AudioClock::Mode mode);
    void on_time_info_box_mode_changed ();
    void on_primary_clock_mode_changed ();
    
    PBD::ScopedConnection connection_with_session_config;

    //get names and paths of recent sessions
    void get_recent_session_names_and_paths(std::vector<std::string>& session_names,std::vector<std::string>& session_paths);
  
    //full path to recent sessions
    std::vector<std::string> recent_session_full_paths;
    
};

#endif /* __ardour_gui_h__ */

