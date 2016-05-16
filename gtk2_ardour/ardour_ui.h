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

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/click_box.h"
#include "gtkmm2ext/stateful_button.h"
#include "gtkmm2ext/bindable_button.h"
#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/visibility_tracker.h"

#include "ardour/ardour.h"
#include "ardour/types.h"
#include "ardour/utils.h"
#include "ardour/plugin.h"
#include "ardour/session_handle.h"
#include "ardour/system_exec.h"

#include "video_timeline.h"

#include "add_route_dialog.h"
#include "ardour_button.h"
#include "ardour_dialog.h"
#include "ardour_window.h"
#include "editing.h"
#include "enums.h"
#include "visibility_group.h"
#include "window_manager.h"

#ifdef COMPILER_MSVC
#include "about.h"
#include "add_video_dialog.h"
#include "big_clock_window.h"
#include "bundle_manager.h"
#include "engine_dialog.h"
#include "export_video_dialog.h"
#include "global_port_matrix.h"
#include "keyeditor.h"
#include "location_ui.h"
#include "lua_script_manager.h"
#include "rc_option_editor.h"
#include "route_params_ui.h"
#include "session_option_editor.h"
#include "speaker_dialog.h"
#else
class About;
class AddRouteDialog;
class AddVideoDialog;
class BigClockWindow;
class BundleManager;
class EngineControl;
class ExportVideoDialog;
class KeyEditor;
class LocationUIWindow;
class LuaScriptManager;
class RCOptionEditor;
class RouteParams_UI;
class SessionOptionEditor;
class SpeakerDialog;
class GlobalPortMatrixWindow;
#endif

class VideoTimeLine;
class ArdourKeyboard;
class AudioClock;
class ButtonJoiner;
class ConnectionEditor;
class DuplicateRouteDialog;
class MainClock;
class Mixer_UI;
class ArdourPrompter;
class PublicEditor;
class SaveAsDialog;
class SessionDialog;
class SessionOptionEditorWindow;
class ShuttleControl;
class Splash;
class TimeInfoBox;
class Meterbridge;
class LuaWindow;
class MidiTracer;
class NSM_Client;
class LevelMeterHBox;
class GUIObjectState;

namespace ARDOUR {
	class ControlProtocolInfo;
	class IO;
	class Port;
	class Route;
	class RouteGroup;
	class Location;
	class ProcessThread;
}

namespace Gtk {
	class ProgressBar;
}

namespace Gtkmm2ext {
	class Tabbable;
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
	void launch_tracker ();
	void launch_subscribe ();
	void launch_cheat_sheet ();
	void launch_website ();
	void launch_website_dev ();
	void launch_forums ();
	void launch_howto_report ();
	void show_about ();
	void hide_about ();

	void load_from_application_api (const std::string& path);
	void finish();

	int load_session (const std::string& path, const std::string& snapshot, std::string mix_template = std::string());
	bool session_loaded;
	int build_session (const std::string& path, const std::string& snapshot, ARDOUR::BusProfile&);
	bool session_is_new() const { return _session_is_new; }

	ARDOUR::Session* the_session() { return _session; }

	bool get_smart_mode () const;

	int get_session_parameters (bool quit_on_cancel, bool should_be_new = false, std::string load_template = "");
	int  build_session_from_dialog (SessionDialog&, const std::string& session_name, const std::string& session_path);
	bool ask_about_loading_existing_session (const std::string& session_path);

	/// @return true if session was successfully unloaded.
	int unload_session (bool hide_stuff = false);
	void close_session();

	int  save_state_canfail (std::string state_name = "", bool switch_to_it = false);
	void save_state (const std::string & state_name = "", bool switch_to_it = false);

	static ARDOUR_UI *instance () { return theArdourUI; }

	PublicEditor&	  the_editor() { return *editor;}
	Mixer_UI* the_mixer() { return mixer; }

	void new_midi_tracer_window ();
	void toggle_editing_space();
	void toggle_mixer_space();
	void toggle_mixer_list();
	void toggle_monitor_section_visibility ();
	void toggle_keep_tearoffs();

	static PublicEditor* _instance;

	/** Emitted frequently with the audible frame, false, and the edit point as
	 *  parameters respectively.
	 *
	 *  (either RapidScreenUpdate || SuperRapidScreenUpdate - user-config)
	 */
	static sigc::signal<void, framepos_t, bool, framepos_t> Clock;

	static void close_all_dialogs () { CloseAllDialogs(); }
	static sigc::signal<void> CloseAllDialogs;

	XMLNode* main_window_settings() const;
	XMLNode* editor_settings() const;
	XMLNode* mixer_settings () const;
	XMLNode* keyboard_settings () const;
	XMLNode* tearoff_settings (const char*) const;

	void save_ardour_state ();
	gboolean configure_handler (GdkEventConfigure* conf);

	void halt_on_xrun_message ();
	void xrun_handler (framepos_t);
	void create_xrun_marker (framepos_t);

	GUIObjectState* gui_object_state;

	MainClock* primary_clock;
	MainClock* secondary_clock;
	void focus_on_clock ();
	AudioClock*   big_clock;

	TimeInfoBox* time_info_box;

	VideoTimeLine *video_timeline;

	void store_clock_modes ();
	void restore_clock_modes ();
	void reset_main_clocks ();

	void synchronize_sync_source_and_video_pullup ();

	void add_route ();

	void add_routes_part_two ();
	void add_routes_thread ();

	void start_duplicate_routes ();

	void add_lua_script ();
	void remove_lua_script ();

	void add_video (Gtk::Window* float_window);
	void remove_video ();
	void start_video_server_menu (Gtk::Window* float_window);
	bool start_video_server (Gtk::Window* float_window, bool popup_msg);
	void stop_video_server (bool ask_confirm=false);
	void flush_videotimeline_cache (bool localcacheonly=false);
	void export_video (bool range = false);

	void session_add_vca (std::string const &, uint32_t);

	void session_add_audio_track (
		int input_channels,
		int32_t output_channels,
		ARDOUR::TrackMode mode,
		ARDOUR::RouteGroup* route_group,
		uint32_t how_many,
		std::string const & name_template,
		bool strict_io,
		ARDOUR::PresentationInfo::order_t order
		) {
		session_add_audio_route (true, input_channels, output_channels, mode, route_group, how_many, name_template, strict_io, order);
	}

	void session_add_audio_bus (
			int input_channels,
			int32_t output_channels,
			ARDOUR::RouteGroup* route_group,
			uint32_t how_many,
			std::string const & name_template,
			bool strict_io,
			ARDOUR::PresentationInfo::order_t order
			) {
		session_add_audio_route (false, input_channels, output_channels, ARDOUR::Normal, route_group, how_many, name_template, strict_io, order);
	}

	void session_add_midi_track (
			ARDOUR::RouteGroup* route_group,
			uint32_t how_many,
			std::string const & name_template,
			bool strict_io,
			ARDOUR::PresentationInfo::order_t order,
			ARDOUR::PluginInfoPtr instrument,
			ARDOUR::Plugin::PresetRecord* preset = NULL) {
		session_add_midi_route (true, route_group, how_many, name_template, strict_io, order, instrument, preset);
	}

	void session_add_mixed_track (const ARDOUR::ChanCount&, const ARDOUR::ChanCount&, ARDOUR::RouteGroup*, uint32_t, std::string const &, bool, ARDOUR::PluginInfoPtr,
	                              ARDOUR::PresentationInfo::order_t order);
	void session_add_midi_bus (ARDOUR::RouteGroup*, uint32_t, std::string const &, bool, ARDOUR::PluginInfoPtr,
	                           ARDOUR::PresentationInfo::order_t order);
	void session_add_audio_route (bool, int32_t, int32_t, ARDOUR::TrackMode, ARDOUR::RouteGroup *, uint32_t, std::string const &, bool,
	                              ARDOUR::PresentationInfo::order_t order);
	void session_add_midi_route (bool, ARDOUR::RouteGroup *, uint32_t, std::string const &, bool, ARDOUR::PresentationInfo::order_t order,
	                             ARDOUR::PluginInfoPtr, ARDOUR::Plugin::PresetRecord*);

	void display_insufficient_ports_message ();

	void attach_to_engine ();
	void post_engine ();

	gint exit_on_main_window_close (GdkEventAny *);

	void maximise_editing_space ();
	void restore_editing_space ();

	void show_ui_prefs ();

	bool check_audioengine(Gtk::Window&);

	void setup_profile ();
	void setup_tooltips ();

	void set_shuttle_fract (double);

	void get_process_buffers ();
	void drop_process_buffers ();

	void reset_peak_display ();
	void reset_route_peak_display (ARDOUR::Route*);
	void reset_group_peak_display (ARDOUR::RouteGroup*);

	const std::string& announce_string() const { return _announce_string; }

	void hide_application ();

	Gtk::Notebook& tabs();
	Gtk::Window& main_window () { return _main_window; }

	void setup_toplevel_window (Gtk::Window&, const std::string& name, void* owner);

	/* called from a static C function */

	GtkNotebook* tab_window_root_drop (GtkNotebook* src,
	                                   GtkWidget* w,
	                                   gint x,
	                                   gint y,
	                                   gpointer user_data);

	bool tabbed_window_state_event_handler (GdkEventWindowState*, void* object);
	bool key_event_handler (GdkEventKey*, Gtk::Window* window);

	Gtkmm2ext::ActionMap global_actions;

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
	void toggle_auto_return ();
	void toggle_click ();
	void toggle_audio_midi_setup ();
	void toggle_session_auto_loop ();
	void toggle_rc_options_window ();
	void toggle_session_options_window ();

private:
	Gtk::Window   _main_window;
	Gtkmm2ext::VisibilityTracker* main_window_visibility;
	Gtk::VBox      main_vpacker;
	Gtk::HBox      status_bar_hpacker;
	Gtk::Notebook _tabs;
	PublicEditor*  editor;
	Mixer_UI*      mixer;
	Gtk::Tooltips _tooltips;
	NSM_Client*    nsm;
	bool          _was_dirty;
	bool          _mixer_on_top;
	bool          _initial_verbose_plugin_scan;
	bool           first_time_engine_run;

	void show_tabbable (Gtkmm2ext::Tabbable*);
	void hide_tabbable (Gtkmm2ext::Tabbable*);
	void detach_tabbable (Gtkmm2ext::Tabbable*);
	void attach_tabbable (Gtkmm2ext::Tabbable*);
	void button_change_tabbable_visibility (Gtkmm2ext::Tabbable*);
	void key_change_tabbable_visibility (Gtkmm2ext::Tabbable*);
	void toggle_editor_and_mixer ();

	void tabbable_state_change (Gtkmm2ext::Tabbable&);

	void toggle_meterbridge ();
	void toggle_luawindow ();

	int  setup_windows ();
	void setup_transport ();
	void setup_clock ();

	static ARDOUR_UI *theArdourUI;
	SessionDialog *_session_dialog;

	int starting ();

	int  ask_about_saving_session (const std::vector<std::string>& actions);

	void save_session_at_its_request (std::string);
	/* periodic safety backup, to be precise */
	gint autosave_session();
	void update_autosave();
	sigc::connection _autosave_connection;

	void session_dirty_changed ();
	void update_title ();

	void map_transport_state ();
	int32_t do_engine_start ();

	void engine_halted (const char* reason, bool free_reason);
	void engine_stopped ();
	void engine_running ();

	void use_config ();

	void about_signal_response(int response);

	Gtk::VBox     top_packer;

	sigc::connection clock_signal_connection;
	void         update_clocks ();
	void         start_clocking ();
	void         stop_clocking ();

	void update_transport_clocks (framepos_t pos);
	void record_state_changed ();

	std::list<MidiTracer*> _midi_tracer_windows;

	/* Transport Control */

	Gtk::Frame               transport_frame;
	Gtk::HBox                transport_hbox;
	Gtk::Fixed               transport_base;
	Gtk::Fixed               transport_button_base;
	Gtk::Frame               transport_button_frame;
	Gtk::HBox                transport_button_hbox;
	Gtk::VBox                transport_button_vbox;
	Gtk::HBox                transport_option_button_hbox;
	Gtk::VBox                transport_option_button_vbox;
	Gtk::HBox                transport_clock_hbox;
	Gtk::VBox                transport_clock_vbox;
	Gtk::HBox                primary_clock_hbox;
	Gtk::HBox                secondary_clock_hbox;

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
	    void set_value (double, PBD::Controllable::GroupControlDisposition group_override);
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

	ArdourButton auto_return_button;
	ArdourButton follow_edits_button;
	ArdourButton auto_input_button;
	ArdourButton click_button;
	ArdourButton sync_button;

	ArdourButton auditioning_alert_button;
	ArdourButton solo_alert_button;
	ArdourButton feedback_alert_button;
	ArdourButton error_alert_button;

	ArdourButton action_script_call_btn[10];
	Gtk::Table action_script_table;

	Gtk::VBox alert_box;
	Gtk::VBox meter_box;
	LevelMeterHBox * editor_meter;
	float            editor_meter_max_peak;
	ArdourButton     editor_meter_peak_display;
	bool             editor_meter_peak_button_release (GdkEventButton*);

	void blink_handler (bool);
	sigc::connection blink_connection;

	void cancel_solo ();
	void solo_blink (bool);
	void sync_blink (bool);
	void audition_blink (bool);
	void feedback_blink (bool);
	void error_blink (bool);

	void set_flat_buttons();

	void soloing_changed (bool);
	void auditioning_changed (bool);
	void _auditioning_changed (bool);

	bool solo_alert_press (GdkEventButton* ev);
	bool audition_alert_press (GdkEventButton* ev);
	bool feedback_alert_press (GdkEventButton *);
	bool error_alert_press (GdkEventButton *);

	void big_clock_value_changed ();
	void primary_clock_value_changed ();
	void secondary_clock_value_changed ();

	/* called by Blink signal */

	void transport_rec_enable_blink (bool onoff);

	Gtk::Menu*        session_popup_menu;

	/* menu bar and associated stuff */

	Gtk::MenuBar* menu_bar;
	Gtk::EventBox menu_bar_base;
	Gtk::HBox     menu_hbox;

	void use_menubar_as_top_menubar ();
	void build_menu_bar ();

	Gtk::Label   wall_clock_label;
	gint update_wall_clock ();

	Gtk::Label   disk_space_label;
	void update_disk_space ();

	Gtk::Label   timecode_format_label;
	void update_timecode_format ();

	Gtk::Label   cpu_load_label;
	void update_cpu_load ();

	Gtk::Label   xrun_label;
	void update_xrun_count ();

	Gtk::Label   peak_thread_work_label;
	void update_peak_thread_work ();

	Gtk::Label   buffer_load_label;
	void update_buffer_load ();

	Gtk::Label   sample_rate_label;
	void update_sample_rate (ARDOUR::framecnt_t);

	Gtk::Label    format_label;
	void update_format ();

	void every_second ();
	void every_point_one_seconds ();
	void every_point_zero_something_seconds ();

	sigc::connection second_connection;
	sigc::connection point_one_second_connection;
	sigc::connection point_zero_something_second_connection;
	sigc::connection fps_connection;

	void set_fps_timeout_connection ();

	void open_session ();
	void open_recent_session ();
	bool process_save_template_prompter (ArdourPrompter& prompter);
	void save_template ();

	void edit_metadata ();
	void import_metadata ();

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

	bool process_snapshot_session_prompter (ArdourPrompter& prompter, bool switch_to_it);
	void snapshot_session (bool switch_to_it);

	void quick_snapshot_session (bool switch_to_it);  //does not promtp for name, just makes a timestamped file

	SaveAsDialog* save_as_dialog;

	bool save_as_progress_update (float fraction, int64_t cnt, int64_t total, Gtk::Label* label, Gtk::ProgressBar* bar);
	void save_session_as ();
	void rename_session ();
	ARDOUR::PresentationInfo::order_t translate_order (AddRouteDialog::InsertAt);

	int         create_mixer ();
	int         create_editor ();
	int         create_meterbridge ();
	int         create_luawindow ();
	int         create_masters ();

	Meterbridge  *meterbridge;
	LuaWindow    *luawindow;

	/* Dialogs that can be created via new<T> */

	RCOptionEditor* rc_option_editor;
	Gtk::HBox rc_option_editor_placeholder;

	WM::Proxy<SpeakerDialog> speaker_config_window;
	WM::Proxy<AddRouteDialog> add_route_dialog;
	WM::Proxy<About> about;
	WM::Proxy<LocationUIWindow> location_ui;
	WM::Proxy<RouteParams_UI> route_params;
	WM::Proxy<EngineControl> audio_midi_setup;
	WM::Proxy<ExportVideoDialog> export_video_dialog;
	WM::Proxy<LuaScriptManager> lua_script_window;

	/* Windows/Dialogs that require a creator method */

	WM::ProxyWithConstructor<SessionOptionEditor> session_option_editor;
	WM::ProxyWithConstructor<AddVideoDialog> add_video_dialog;
	WM::ProxyWithConstructor<BundleManager> bundle_manager;
	WM::ProxyWithConstructor<BigClockWindow> big_clock_window;
	WM::ProxyWithConstructor<GlobalPortMatrixWindow> audio_port_matrix;
	WM::ProxyWithConstructor<GlobalPortMatrixWindow> midi_port_matrix;
	WM::ProxyWithConstructor<KeyEditor> key_editor;

	/* creator methods */

	SessionOptionEditor*    create_session_option_editor ();
	BundleManager*          create_bundle_manager ();
	AddVideoDialog*         create_add_video_dialog ();
	BigClockWindow*         create_big_clock_window();
	GlobalPortMatrixWindow* create_global_port_matrix (ARDOUR::DataType);
	KeyEditor*              create_key_editor ();

	ARDOUR::SystemExec *video_server_process;

	void handle_locations_change (ARDOUR::Location*);

	/* Keyboard Handling */

	ArdourKeyboard* keyboard;

	/* Keymap handling */

	void install_actions ();

	void toggle_record_enable (uint16_t);

	uint32_t rec_enabled_streams;
	void count_recenabled_streams (ARDOUR::Route&);

	Splash* splash;

	void pop_back_splash (Gtk::Window&);

	/* cleanup */

	Gtk::MenuItem *cleanup_item;

	void display_cleanup_results (ARDOUR::CleanupReport& rep, const gchar* list_title, const bool msg_delete);
	void cleanup ();
	void cleanup_peakfiles ();
	void flush_trash ();

	bool have_configure_timeout;
	ARDOUR::microseconds_t last_configure_time;
	gint configure_timeout ();

	ARDOUR::microseconds_t last_peak_grab;
	ARDOUR::microseconds_t last_shuttle_request;

	bool have_disk_speed_dialog_displayed;
	void disk_speed_dialog_gone (int ignored_response, Gtk::MessageDialog*);
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
	void sr_mismatch_message (ARDOUR::framecnt_t, ARDOUR::framecnt_t);

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
	void session_parameter_changed (std::string);

	bool first_idle ();

	void check_memory_locking ();

	void audioengine_setup ();

	void display_message (const char *prefix, gint prefix_len,
			Glib::RefPtr<Gtk::TextBuffer::Tag> ptag, Glib::RefPtr<Gtk::TextBuffer::Tag> mtag,
			const char *msg);
	Gtk::Label status_bar_label;
	bool status_bar_button_press (GdkEventButton*);

	void loading_message (const std::string& msg);

	PBD::ScopedConnectionList forever_connections;
	PBD::ScopedConnection halt_connection;

	void step_edit_status_change (bool);

	/* these are used only in response to a platform-specific "ShouldQuit" signal */
	bool idle_finish ();
	void queue_finish ();
	void fontconfig_dialog ();

	int missing_file (ARDOUR::Session*s, std::string str, ARDOUR::DataType type);
	int ambiguous_file (std::string file, std::vector<std::string> hits);

	bool click_button_clicked (GdkEventButton *);

	VisibilityGroup _status_bar_visibility;

	/** A ProcessThread so that we have some thread-local buffers for use by
	 *  PluginEqGui::impulse_analysis ().
	 */
	ARDOUR::ProcessThread* _process_thread;

	void feedback_detected ();

	ArdourButton             midi_panic_button;
	void                     midi_panic ();

	void successful_graph_sort ();
	bool _feedback_exists;

	enum ArdourLogLevel {
		LogLevelNone = 0,
		LogLevelInfo,
		LogLevelWarning,
		LogLevelError
	};

	ArdourLogLevel _log_not_acknowledged;

	void resize_text_widgets ();

	bool xrun_button_release (GdkEventButton* ev);

	std::string _announce_string;
	void check_announcements ();

	int do_audio_midi_setup (uint32_t);
	void audioengine_became_silent ();

	DuplicateRouteDialog* duplicate_routes_dialog;

	void grab_focus_after_dialog ();

	void tabs_switch (GtkNotebookPage*, guint page_number);
	void tabs_page_added (Gtk::Widget*, guint);
	void tabs_page_removed (Gtk::Widget*, guint);
	ArdourButton editor_visibility_button;
	ArdourButton mixer_visibility_button;
	ArdourButton prefs_visibility_button;

	bool key_press_focus_accelerator_handler (Gtk::Window& window, GdkEventKey* ev, Gtkmm2ext::Bindings*);
	bool try_gtk_accel_binding (GtkWindow* win, GdkEventKey* ev, bool translate, GdkModifierType modifier);

	bool main_window_delete_event (GdkEventAny*);
	bool idle_ask_about_quit ();

	void load_bindings ();
	bool tabbable_visibility_button_press (GdkEventButton* ev, std::string const& tabbable_name);

	void step_up_through_tabs ();
	void step_down_through_tabs ();
};

#endif /* __ardour_gui_h__ */
