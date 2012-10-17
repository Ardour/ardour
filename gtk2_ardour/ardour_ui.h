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

#include <libgnomecanvasmm/canvas.h>

#include <pbd/xml++.h>
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
#include <ardour/ardour.h>
#include <ardour/session.h>
#include <ardour/types.h>

#include "audio_clock.h"
#include "ardour_dialog.h"
#include "editing.h"
#include "ui_config.h"

class AudioClock;
class PublicEditor;
class Keyboard;
class OptionEditor;
class KeyEditor;
class Mixer_UI;
class ConnectionEditor;
class RouteParams_UI;
class About;
class Splash;
class AddRouteDialog;
class LocationUI;
class ThemeManager;
class NewSessionDialog;

namespace Gtkmm2ext {
	class TearOff;
}

namespace ARDOUR {
	class AudioEngine;
	class Route;
	class Port;
	class IO;
	class ControlProtocolInfo;
}

namespace ALSA {
	class MultiChannelDevice;
}

#define FRAME_NAME "BaseFrame"

extern sigc::signal<void>  ColorsChanged;
extern sigc::signal<void>  DPIReset;

class ARDOUR_UI : public Gtkmm2ext::UI
{
  public:
	ARDOUR_UI (int *argcp, char **argvp[]);
	~ARDOUR_UI();

	void show ();
	bool shown() { return shown_flag; }
	
	void show_splash ();
	void hide_splash ();

	void launch_chat ();
	void launch_manual ();
	void launch_reference ();

	void show_about ();
	void hide_about ();
	
	void idle_load (const std::string& path);
	void finish();

	int load_session (const std::string& path, const std::string& snapshot, std::string mix_template = std::string());
	bool session_loaded;
	int build_session (const std::string& path, const std::string& snapshot, 
			   uint32_t ctl_chns, 
			   uint32_t master_chns,
			   ARDOUR::AutoConnectOption input_connect,
			   ARDOUR::AutoConnectOption output_connect,
			   uint32_t nphysin,
			   uint32_t nphysout,
			   nframes_t initial_length);
	bool session_is_new() const { return _session_is_new; }

	ARDOUR::Session* the_session() { return session; }

	bool will_create_new_session_automatically() const {
		return _will_create_new_session_automatically;
	}

	void set_will_create_new_session_automatically (bool yn) {
		_will_create_new_session_automatically = yn;
	}

	bool get_session_parameters (bool have_engine = false, bool should_be_new = false);
	void parse_cmdline_path (const std::string& cmdline_path, std::string& session_name, std::string& session_path, bool& existing_session);
	int  load_cmdline_session (const std::string& session_name, const std::string& session_path, bool& existing_session);
	int  build_session_from_nsd (const std::string& session_name, const std::string& session_path);
	bool ask_about_loading_existing_session (const std::string& session_path);
	int  unload_session (bool hide_stuff = false);
	void close_session(); 

	int  save_state_canfail (string state_name = "", bool switch_to_it = false);
	void save_state (const string & state_name = "", bool switch_to_it = false);

	static double gain_to_slider_position (ARDOUR::gain_t g);
        static ARDOUR::gain_t slider_position_to_gain (double pos);

	static ARDOUR_UI *instance () { return theArdourUI; }
	static UIConfiguration *config () { return ui_config; }

	PublicEditor&	  the_editor(){return *editor;}
	Mixer_UI* the_mixer() { return mixer; }

	ARDOUR::AudioEngine& the_engine() const { return *engine; }

	void toggle_key_editor ();
	void toggle_location_window ();
	void toggle_theme_manager ();
	void toggle_big_clock_window ();
	void toggle_connection_editor ();
	void toggle_route_params_window ();
	void toggle_editing_space();

	Gtk::Tooltips& tooltips() { return _tooltips; }

	static sigc::signal<void,bool> Blink;
	static sigc::signal<void>      RapidScreenUpdate;
	static sigc::signal<void>      SuperRapidScreenUpdate;
	static sigc::signal<void,nframes_t, bool, nframes_t> Clock;

	void name_io_setup (ARDOUR::AudioEngine&, string&, ARDOUR::IO& io, bool in);

	XMLNode* editor_settings() const;
	XMLNode* mixer_settings () const;
	XMLNode* keyboard_settings () const;

	void save_ardour_state ();
	gboolean configure_handler (GdkEventConfigure* conf);

	void do_transport_locate (nframes_t position);
	void halt_on_xrun_message ();
	void xrun_handler (nframes_t);
	void create_xrun_marker (nframes_t);

	AudioClock primary_clock;
	AudioClock secondary_clock;
	AudioClock preroll_clock;
	AudioClock postroll_clock;

	void store_clock_modes ();
	void restore_clock_modes ();

	void add_route (Gtk::Window* float_window);
	
	void session_add_audio_track (int input_channels, int32_t output_channels, ARDOUR::TrackMode mode, uint32_t how_many) {
		session_add_audio_route (true, input_channels, output_channels, mode, how_many);
	}

	void session_add_audio_bus (int input_channels, int32_t output_channels, uint32_t how_many) {
		session_add_audio_route (false, input_channels, output_channels, ARDOUR::Normal, how_many);
	}

	void session_add_midi_track ();

	int  create_engine ();
	void post_engine ();

	gint exit_on_main_window_close (GdkEventAny *);

	void maximise_editing_space ();
	void restore_editing_space ();

	void set_native_file_header_format (ARDOUR::HeaderFormat sf);
	void set_native_file_data_format (ARDOUR::SampleFormat sf);

	void setup_profile ();
	void setup_theme ();

	void set_shuttle_fract (double);

  protected:
	friend class PublicEditor;

	void toggle_clocking ();
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

	void toggle_session_auto_loop ();
	
	void toggle_options_window ();

  private:
	struct GlobalClickBox : public Gtk::VBox {
	    Gtkmm2ext::ClickBox  *box;
	    Gtk::Frame      frame;
	    Gtk::Label      label;
	    vector<string> &strings;
	    Gtk::Adjustment adjustment;

	    static void printer (char buf[32], Gtk::Adjustment &adj, void *arg);

	    GlobalClickBox (const string &str, vector<string> &vs)
		    : strings (vs),
		      adjustment (0, 0, vs.size() - 1, 1, 1, 0) {
		    box = new Gtkmm2ext::ClickBox (&adjustment, "ClickButton");
		    label.set_text (str);
		    label.set_name ("GlobalButtonLabel");
		    frame.add (*box);
		    frame.set_shadow_type (Gtk::SHADOW_IN);
		    pack_start (label);
		    pack_start (frame);
		    box->set_print_func (printer, this);
		    box->set_wrap (true);
	    };
	};

	ARDOUR::AudioEngine                 *engine;
	ARDOUR::Session                     *session;

	Gtk::Tooltips          _tooltips;

	void                goto_editor_window ();
	void                goto_mixer_window ();
	void                toggle_editor_mixer_on_top ();
	bool                _mixer_on_top;

	GlobalClickBox     *online_control_button;
	vector<string>      online_control_strings;

	GlobalClickBox    *crossfade_time_button;
	vector<string>     crossfade_time_strings;

	Gtk::ToggleButton   preroll_button;
	Gtk::ToggleButton   postroll_button;

	int  setup_windows ();
	void setup_transport ();
	void setup_clock ();

	static ARDOUR_UI *theArdourUI;

	void backend_audio_error (bool we_set_params, Gtk::Window* toplevel = 0);
	void startup ();
	void shutdown ();

	int  ask_about_saving_session (const string & why);

	/* periodic safety backup, to be precise */
	gint autosave_session();
	void update_autosave();
	sigc::connection _autosave_connection;

	void queue_transport_change ();
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


  private:
	Gtk::VBox     top_packer;

	sigc::connection clock_signal_connection;
	void         update_clocks ();
	void         start_clocking ();
	void         stop_clocking ();

	void manage_window (Gtk::Window&);
	
	AudioClock   big_clock;
	Gtk::Window* big_clock_window;

	void float_big_clock (Gtk::Window* parent);
	bool main_window_state_event_handler (GdkEventWindowState*, bool window_was_editor);

	void update_transport_clocks (nframes_t pos);
	void record_state_changed ();

	/* Transport Control */

	void detach_tearoff (Gtk::Box* parent, Gtk::Widget* contents);
	void reattach_tearoff (Gtk::Box* parent, Gtk::Widget* contents, int32_t order);

	Gtkmm2ext::TearOff*      transport_tearoff;
	Gtk::Frame               transport_frame;
	Gtk::HBox                transport_tearoff_hbox;
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
		    ShuttleControl
		    
	    };
	    
	    TransportControllable (std::string name, ARDOUR_UI&, ToggleType);
	    void set_value (float);
	    float get_value (void) const;
	    
	    void set_id (const std::string&);
	    
	    ARDOUR_UI& ui;
	    ToggleType type;
	};

	TransportControllable roll_controllable;
	TransportControllable stop_controllable;
	TransportControllable goto_start_controllable;
	TransportControllable goto_end_controllable;
	TransportControllable auto_loop_controllable;
	TransportControllable play_selection_controllable;
	TransportControllable rec_controllable;
	TransportControllable shuttle_controllable;
	BindingProxy shuttle_controller_binding_proxy;

	void set_transport_controllable_state (const XMLNode&);
	XMLNode& get_transport_controllable_state ();

	BindableButton roll_button;
	BindableButton stop_button;
	BindableButton goto_start_button;
	BindableButton goto_end_button;
	BindableButton auto_loop_button;
	BindableButton play_selection_button;
	BindableButton rec_button;

	Gtk::ComboBoxText sync_option_combo;

	void sync_option_changed ();
	void toggle_time_master ();
	void toggle_video_sync ();

	Gtk::DrawingArea  shuttle_box;
	Gtk::EventBox     speed_display_box;
	Gtk::Label        speed_display_label;
	Gtk::Button       shuttle_units_button;
	Gtk::ComboBoxText shuttle_style_button;
	Gtk::Menu*        shuttle_unit_menu;
	Gtk::Menu*        shuttle_style_menu;
	float             shuttle_max_speed;
	Gtk::Menu*        shuttle_context_menu;

	void build_shuttle_context_menu ();
	void show_shuttle_context_menu ();
	void shuttle_style_changed();
	void shuttle_unit_clicked ();
	void set_shuttle_max_speed (float);
	void update_speed_display ();
	float last_speed_displayed;

	gint shuttle_box_button_press (GdkEventButton*);
	gint shuttle_box_button_release (GdkEventButton*);
	gint shuttle_box_scroll (GdkEventScroll*);
	gint shuttle_box_motion (GdkEventMotion*);
	gint shuttle_box_expose (GdkEventExpose*);
	gint mouse_shuttle (double x, bool force);
	void use_shuttle_fract (bool force);

	bool   shuttle_grabbed;
	double shuttle_fract;

	Gtkmm2ext::StatefulToggleButton punch_in_button;
	Gtkmm2ext::StatefulToggleButton punch_out_button;
	Gtkmm2ext::StatefulToggleButton auto_return_button;
	Gtkmm2ext::StatefulToggleButton auto_play_button;
	Gtkmm2ext::StatefulToggleButton auto_input_button;
	Gtkmm2ext::StatefulToggleButton click_button;
	Gtkmm2ext::StatefulToggleButton time_master_button;

	Gtk::ToggleButton auditioning_alert_button;
	Gtk::ToggleButton solo_alert_button;

	Gtk::VBox alert_box;
	
	void solo_blink (bool);
	void audition_blink (bool);

	void soloing_changed (bool);
	void auditioning_changed (bool);
	void _auditioning_changed (bool);

	void solo_alert_toggle ();
	void audition_alert_toggle ();

	void big_clock_value_changed ();
	void primary_clock_value_changed ();
	void secondary_clock_value_changed ();

	/* called by Blink signal */

	void transport_rec_enable_blink (bool onoff);

	Gtk::Menu*        session_popup_menu;

	struct RecentSessionModelColumns : public Gtk::TreeModel::ColumnRecord {
	    RecentSessionModelColumns() { 
		    add (visible_name);
		    add (fullpath);
	    }
	    Gtk::TreeModelColumn<std::string> visible_name;
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
	    bool operator() (std::pair<string,string> a, std::pair<string,string> b) const {
		    return cmp_nocase(a.first, b.first) == -1;
	    }
	};

	/* menu bar and associated stuff */

	Gtk::MenuBar* menu_bar;
	Gtk::EventBox menu_bar_base;
	Gtk::HBox     menu_hbox;

	void use_menubar_as_top_menubar ();

	void build_menu_bar ();
	void build_control_surface_menu ();

	Gtk::Label   wall_clock_label;
	Gtk::EventBox wall_clock_box;
	gint update_wall_clock ();

	Gtk::Label   disk_space_label;
	Gtk::EventBox disk_space_box;
	void update_disk_space ();

	Gtk::Label   cpu_load_label;
	Gtk::EventBox cpu_load_box;
	void update_cpu_load ();

	Gtk::Label   buffer_load_label;
	Gtk::EventBox buffer_load_box;
	void update_buffer_load ();

	Gtk::Label   sample_rate_label;
	Gtk::EventBox sample_rate_box;
	void update_sample_rate (nframes_t);

	gint every_second ();
	gint every_point_one_seconds ();
	gint every_point_zero_one_seconds ();

	sigc::connection second_connection;
	sigc::connection point_one_second_connection;
	sigc::connection point_oh_five_second_connection;
	sigc::connection point_zero_one_second_connection;

	gint session_menu (GdkEventButton *);

	bool _will_create_new_session_automatically;

	NewSessionDialog* new_session_dialog;
	
	void open_session ();
	void open_recent_session ();
	void save_template ();

	void session_add_audio_route (bool disk, int32_t input_channels, int32_t output_channels, ARDOUR::TrackMode mode, uint32_t how_many);

	void set_transport_sensitivity (bool);

	void remove_last_capture ();

	void transport_goto_zero ();
	void transport_goto_start ();
	void transport_goto_end ();
	void transport_goto_wallclock ();
	void transport_stop ();
	void transport_stop_and_forget_capture ();
	void transport_record (bool roll);
	void transport_roll ();
	void transport_play_selection(); 
	void transport_forward (int option);
	void transport_rewind (int option);
	void transport_loop ();
	void toggle_roll (bool with_abort, bool roll_out_of_bounded_mode);

	bool _session_is_new;
	void connect_to_session (ARDOUR::Session *);
	void connect_dependents_to_session (ARDOUR::Session *);
	void we_have_dependents ();
	
	void setup_session_options ();
	
	guint32  last_key_press_time;

	void snapshot_session (bool switch_to_it);

	Mixer_UI   *mixer;
	int         create_mixer ();
	
	PublicEditor     *editor;
	int         create_editor ();

	RouteParams_UI *route_params;
	int             create_route_params ();

	ConnectionEditor *connection_editor;
	int               create_connection_editor ();

	LocationUI *location_ui;
	int         create_location_ui ();
	void        handle_locations_change (ARDOUR::Location*);

	static UIConfiguration *ui_config;
	ThemeManager *theme_manager;

	/* Key bindings editor */

	KeyEditor *key_editor;

	/* Options window */
	
	OptionEditor *option_editor;
	
	/* route dialog */

	AddRouteDialog *add_route_dialog;
	
	/* Keyboard Handling */
	
	Keyboard* keyboard;

	/* Keymap handling */

	void install_actions ();

	void toggle_record_enable (uint32_t);

	uint32_t rec_enabled_streams;
	void count_recenabled_streams (ARDOUR::Route&);

	About* about;
	Splash* splash;
	void pop_back_splash ();
	bool shown_flag;

	/* cleanup */

	Gtk::MenuItem *cleanup_item;

	void display_cleanup_results (ARDOUR::Session::cleanup_report& rep, const gchar* list_title, 
				      const string& plural_msg, const string& singular_msg);
	void cleanup ();
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
	
	bool preset_file_exists_handler ();

	void session_dialog (std::string);
	int pending_state_dialog ();
	int sr_mismatch_dialog (nframes_t, nframes_t);
	
	void disconnect_from_jack ();
	void reconnect_to_jack ();
	void set_jack_buffer_size (nframes_t);

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

	void toggle_use_osc ();

	void toggle_denormal_protection ();

	void set_input_auto_connect (ARDOUR::AutoConnectOption);
	void set_output_auto_connect (ARDOUR::AutoConnectOption);
	void set_solo_model (ARDOUR::SoloModel);
	void set_monitor_model (ARDOUR::MonitorModel);
	void set_remote_model (ARDOUR::RemoteModel);
	void set_denormal_model (ARDOUR::DenormalModel);

	void toggle_seamless_loop ();
	void toggle_sync_order_keys ();
	void toggle_new_plugins_active();
	void toggle_StopPluginsWithTransport();
	void toggle_DoNotRunPluginsWhileRecording();
	void toggle_VerifyRemoveLastCapture();
	void toggle_PeriodicSafetyBackups();
	void toggle_StopRecordingOnXrun();
	void toggle_CreateXrunMarker();
	void toggle_StopTransportAtEndOfSession();
	void toggle_GainReduceFastTransport();
	void toggle_LatchedSolo();
	void toggle_ShowSoloMutes();
	void toggle_SoloMuteOverride();
	void toggle_LatchedRecordEnable ();
	void toggle_RegionEquivalentsOverlap ();
	void toggle_PrimaryClockDeltaEditCursor ();
	void toggle_SecondaryClockDeltaEditCursor ();
	void toggle_only_copy_imported_files ();
	void toggle_ShowTrackMeters ();
	void toggle_use_narrow_ms();
	void toggle_NameNewMarkers ();
	void toggle_rubberbanding_snaps_to_grid ();
	void toggle_auto_analyse_audio ();
	void toggle_TapeMachineMode();

	void mtc_port_changed ();
	void map_solo_model ();
	void map_monitor_model ();
	void map_denormal_model ();
	void map_denormal_protection ();
	void map_remote_model ();
	void map_file_header_format ();
	void map_file_data_format ();
	void map_input_auto_connect ();
	void map_output_auto_connect ();
	void map_only_copy_imported_files ();
	void parameter_changed (const char*);

	void set_meter_hold (ARDOUR::MeterHold);
	void set_meter_falloff (ARDOUR::MeterFalloff);
	void map_meter_hold ();
	void map_meter_falloff ();

	void toggle_control_protocol (ARDOUR::ControlProtocolInfo*);
	void control_protocol_status_change (ARDOUR::ControlProtocolInfo*);
	void toggle_control_protocol_feedback (ARDOUR::ControlProtocolInfo*, const char* group_name, std::string action_name);

	bool first_idle ();

	void no_memory_warning ();
	void check_memory_locking ();

	bool check_audioengine();
	void audioengine_setup ();

	void display_message (const char *prefix, gint prefix_len, 
			      Glib::RefPtr<Gtk::TextBuffer::Tag> ptag, Glib::RefPtr<Gtk::TextBuffer::Tag> mtag, const char *msg);
	Gtk::Label status_bar_label;
	bool status_bar_button_press (GdkEventButton*);
	Gtk::ToggleButton error_log_button;
	
	void loading_message (const std::string& msg);
	void end_loading_messages ();

	void platform_specific ();
	void platform_setup ();
	void fontconfig_dialog ();
	void toggle_translations ();

	/* these are used only in response to a platform-specific "ShouldQuit" signal
	 */
	bool idle_finish ();
	void queue_finish ();
};

#endif /* __ardour_gui_h__ */

