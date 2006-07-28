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

    $Id$
*/

#ifndef __ardour_gui_h__
#define __ardour_gui_h__

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
#include <gtkmm/adjustment.h>
#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/click_box.h>
#include <gtkmm2ext/stateful_button.h>
#include <ardour/ardour.h>
#include <ardour/session.h>

#include "audio_clock.h"
#include "ardour_dialog.h"
#include "editing.h"

class AudioClock;
class PublicEditor;
class Keyboard;
class MeterBridge;
class OptionEditor;
class Mixer_UI;
class ConnectionEditor;
class RouteParams_UI;
class SoundFileBrowser;
class About;
class AddRouteDialog;
class NewSessionDialog;
class LocationUI;
class ColorManager;

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

class ARDOUR_UI : public Gtkmm2ext::UI
{
  public:
	ARDOUR_UI (int *argcp, char **argvp[], string rcfile);
	~ARDOUR_UI();

	void show ();
	bool shown() { return shown_flag; }
	
	void show_splash ();
	void hide_splash ();
	
	int load_session (const string & path, const string & snapshot, string* mix_template = 0);
	bool session_loaded;
	int build_session (const string & path, const string & snapshot, 
			   uint32_t ctl_chns, 
			   uint32_t master_chns,
			   ARDOUR::Session::AutoConnectOption input_connect,
			   ARDOUR::Session::AutoConnectOption output_connect,
			   uint32_t nphysin,
			   uint32_t nphysout,
			   jack_nframes_t initial_length);
	bool session_is_new() const { return _session_is_new; }

	ARDOUR::Session* the_session() { return session; }

	bool will_create_new_session_automatically() const {
		return _will_create_new_session_automatically;
	}

	void set_will_create_new_session_automatically (bool yn) {
		_will_create_new_session_automatically = yn;
	}

        void new_session(bool startup = false, std::string path = string());
	gint cmdline_new_session (string path);
	int  unload_session ();
	void close_session(); 

	int  save_state_canfail (string state_name = "");
	void save_state (const string & state_name = "");
	void restore_state (string state_name = "");

	static double gain_to_slider_position (ARDOUR::gain_t g);
        static ARDOUR::gain_t slider_position_to_gain (double pos);

	static ARDOUR_UI *instance () { return theArdourUI; }

	PublicEditor&	  the_editor(){return *editor;}
	Mixer_UI* the_mixer() { return mixer; }
	
	void toggle_location_window ();
	void toggle_color_manager ();
	void toggle_big_clock_window ();
	void toggle_connection_editor ();
	void toggle_route_params_window ();
	void toggle_tempo_window ();
	void toggle_editing_space();

	Gtk::Tooltips& tooltips() { return _tooltips; }

	static sigc::signal<void,bool> Blink;
	static sigc::signal<void>      RapidScreenUpdate;
	static sigc::signal<void>      SuperRapidScreenUpdate;
	static sigc::signal<void,jack_nframes_t> Clock;

	/* this is a helper function to centralize the (complex) logic for
	   blinking rec-enable buttons.
	*/

	void rec_enable_button_blink (bool onoff, ARDOUR::AudioDiskstream *, Gtk::Widget *w);

	void name_io_setup (ARDOUR::AudioEngine&, string&, ARDOUR::IO& io, bool in);
	void choose_io (ARDOUR::IO&, bool input);

	static gint hide_and_quit (GdkEventAny *ev, ArdourDialog *);

	XMLNode* editor_settings() const;
	XMLNode* mixer_settings () const;
	XMLNode* keyboard_settings () const;

	void save_ardour_state ();
	gboolean configure_handler (GdkEventConfigure* conf);

	void do_transport_locate (jack_nframes_t position);
	void halt_on_xrun_message ();

	AudioClock primary_clock;
	AudioClock secondary_clock;
	AudioClock preroll_clock;
	AudioClock postroll_clock;

	void add_route ();
	
	void session_add_audio_track (int input_channels, int32_t output_channels, ARDOUR::TrackMode mode) {
		session_add_audio_route (true, input_channels, output_channels, mode);
	}

	void session_add_audio_bus (int input_channels, int32_t output_channels) {
		session_add_audio_route (false, input_channels, output_channels, ARDOUR::Normal);
	}

	void session_add_midi_track () {
		session_add_midi_route (true);
	}

	void session_add_midi_bus () {
		session_add_midi_route (false);
	}

	void set_engine (ARDOUR::AudioEngine&);

	gint exit_on_main_window_close (GdkEventAny *);

	void maximise_editing_space ();
	void restore_editing_space ();

	void set_native_file_header_format (ARDOUR::HeaderFormat sf);
	void set_native_file_data_format (ARDOUR::SampleFormat sf);

  protected:
	friend class PublicEditor;

	void toggle_clocking ();
	void toggle_auto_play ();
	void toggle_auto_input ();
	void toggle_punch_in ();
	void toggle_punch_out ();
	void toggle_auto_return ();
	void toggle_click ();

	void toggle_session_auto_loop ();
	void toggle_session_punch_in ();
	
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

	void                     goto_editor_window ();
	void                     goto_mixer_window ();
	
	Gtk::Table               adjuster_table;
	Gtk::Frame               adjuster_frame;
	Gtk::Fixed               adjuster_base;

	GlobalClickBox     *online_control_button;
	vector<string>      online_control_strings;

	GlobalClickBox    *crossfade_time_button;
	vector<string>     crossfade_time_strings;

	GlobalClickBox    *mmc_id_button;
	vector<string>     mmc_id_strings;

	Gtk::ToggleButton   preroll_button;
	Gtk::ToggleButton   postroll_button;

	Gtk::Table          transport_table;
	Gtk::Table          option_table;

	int  setup_windows ();
	void setup_session_menu ();
	void setup_transport ();
	void setup_clock ();
	void setup_session_info ();
	void setup_adjustables ();

	Gtk::MenuBar* make_menubar ();
	
	static ARDOUR_UI *theArdourUI;

	void startup ();
	void shutdown ();

	void finish();
	int  ask_about_saving_session (const string & why);
	gint ask_about_save_deleted (GdkEventAny*);
	void save_session_choice_made (int);
	int  save_the_session;

	void queue_transport_change ();
	void map_transport_state ();
	int32_t do_engine_start ();
	gint start_engine ();
	
	void engine_halted ();
	void engine_stopped ();
	void engine_running ();

	void use_config ();

	void clear_meters ();

	static gint _blink  (void *);
	void blink ();
	gint blink_timeout_tag;
	bool blink_on;
	void start_blinking ();
	void stop_blinking ();

	void control_methods_adjusted ();
	void mmc_device_id_adjusted ();

  private:
	Gtk::VBox     top_packer;

	sigc::connection clock_signal_connection;
	void         update_clocks ();
	void         start_clocking ();
	void         stop_clocking ();

	void manage_window (Gtk::Window&);
	
	AudioClock   big_clock;
	Gtk::Frame   big_clock_frame;
	Gtk::Window* big_clock_window;

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

	Gtkmm2ext::StatefulButton roll_button;
	Gtkmm2ext::StatefulButton stop_button;
	Gtkmm2ext::StatefulButton rewind_button;
	Gtkmm2ext::StatefulButton forward_button;
	Gtkmm2ext::StatefulButton goto_start_button;
	Gtkmm2ext::StatefulButton goto_end_button;
	Gtkmm2ext::StatefulButton auto_loop_button;
	Gtkmm2ext::StatefulButton play_selection_button;

	Gtkmm2ext::StatefulButton rec_button;

	Gtk::ToggleButton time_master_button;
	Gtk::ComboBoxText sync_option_combo;

	void sync_option_changed ();
	void toggle_time_master ();

	enum ShuttleBehaviour {
		Sprung,
		Wheel
	};

	enum ShuttleUnits {
		Percentage,
		Semitones
	};

	Gtk::DrawingArea  shuttle_box;
	Gtk::EventBox     speed_display_box;
	Gtk::Label        speed_display_label;
	Gtk::Button       shuttle_units_button;
	Gtk::ComboBoxText shuttle_style_button;
	Gtk::Menu*        shuttle_unit_menu;
	Gtk::Menu*        shuttle_style_menu;
	ShuttleBehaviour  shuttle_behaviour;
	ShuttleUnits      shuttle_units;
	float             shuttle_max_speed;
	Gtk::Menu*        shuttle_context_menu;

	void build_shuttle_context_menu ();
	void show_shuttle_context_menu ();
	void shuttle_style_changed();
	void shuttle_unit_clicked ();
	void set_shuttle_behaviour (ShuttleBehaviour);
	void set_shuttle_units (ShuttleUnits);
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

	Gtk::ToggleButton punch_in_button;
	Gtk::ToggleButton punch_out_button;
	Gtk::ToggleButton auto_return_button;
	Gtk::ToggleButton auto_play_button;
	Gtk::ToggleButton auto_input_button;
	Gtk::ToggleButton click_button;
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

	void primary_clock_value_changed ();
	void secondary_clock_value_changed ();

	/* called by Blink signal */

	void transport_rec_enable_blink (bool onoff);

	/* These change where we accept control from:
	   MMC, X (local) or both.
	*/

	void allow_mmc_only ();
	void allow_mmc_and_local ();
	void allow_local_only ();

	static void rate_printer (char buf[32], Gtk::Adjustment &, void *);

	Gtk::Menu*        session_popup_menu;

	struct RecentSessionModelColumns : public Gtk::TreeModel::ColumnRecord {
	    RecentSessionModelColumns() { 
		    add (visible_name);
		    add (fullpath);
	    }
	    Gtk::TreeModelColumn<Glib::ustring> visible_name;
	    Gtk::TreeModelColumn<Glib::ustring> fullpath;
	};

	RecentSessionModelColumns    recent_session_columns;
	Gtk::TreeView                recent_session_display;
	Glib::RefPtr<Gtk::TreeStore> recent_session_model;

	ArdourDialog*     session_selector_window;
	Gtk::FileChooserDialog* open_session_selector;
	
	void build_session_selector();
	void recent_session_selection_changed ();
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

	void build_menu_bar ();
	void build_control_surface_menu ();
	void pack_toplevel_controls();

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
	void update_sample_rate (jack_nframes_t);

	gint every_second ();
	gint every_point_one_seconds ();
	gint every_point_zero_one_seconds ();

	sigc::connection second_connection;
	sigc::connection point_one_second_connection;
	sigc::connection point_zero_one_second_connection;

	void diskstream_added (ARDOUR::Diskstream*);

	gint session_menu (GdkEventButton *);

	bool _will_create_new_session_automatically;

	NewSessionDialog* m_new_session_dialog;
	
	void open_session ();
	void open_recent_session ();
	void open_ok_clicked ();

	void save_template ();

	void session_add_audio_route (bool disk, int32_t input_channels, int32_t output_channels, ARDOUR::TrackMode mode);
	void session_add_midi_route (bool disk);

	void set_transport_sensitivity (bool);

	void remove_last_capture ();

	void transport_goto_zero ();
	void transport_goto_start ();
	void transport_goto_end ();
	void transport_stop ();
	void transport_stop_and_forget_capture ();
	void transport_record ();
	void transport_roll ();
	void transport_play_selection(); 
	void transport_forward (int option);
	void transport_rewind (int option);
	void transport_loop ();

	void transport_locating ();
	void transport_rolling ();
	void transport_rewinding ();
	void transport_forwarding ();
	void transport_stopped ();

	void send_all_midi_feedback ();
	
	bool _session_is_new;
	void connect_to_session (ARDOUR::Session *);
	void connect_dependents_to_session (ARDOUR::Session *);
	void we_have_dependents ();
	void setup_keybindings ();
	void setup_session_options ();
	void setup_config_options ();
	
	guint32  last_key_press_time;

	void snapshot_session ();

	void map_record_state ();
	void queue_map_record_state ();

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

	ColorManager* color_manager;

	/* Options window */
	
	OptionEditor *option_editor;
	
	/* route dialog */

	AddRouteDialog *add_route_dialog;
	void add_route_dialog_done (int status);

	/* SoundFile Browser */
	SoundFileBrowser *sfdb;
	void toggle_sound_file_browser ();
	int create_sound_file_browser ();
	
	/* Keyboard Handling */
	
	Keyboard* keyboard;

	/* Keymap handling */

	Glib::RefPtr<Gtk::ActionGroup> get_common_actions();
	void install_actions ();
	void test_binding_action (const char *);
	void start_keyboard_prefix();

	void toggle_record_enable (uint32_t);

	uint32_t rec_enabled_diskstreams;
	void count_recenabled_diskstreams (ARDOUR::Route&);

	About* about;
	bool shown_flag;
	/* cleanup */

	Gtk::MenuItem *cleanup_item;

	void display_cleanup_results (ARDOUR::Session::cleanup_report& rep, const gchar* list_title, const string & msg);
	void cleanup ();
	void flush_trash ();

	bool have_configure_timeout;
	struct timeval last_configure_time;
	gint configure_timeout ();

	struct timeval last_peak_grab;
	struct timeval last_shuttle_request;

	void delete_sources_in_the_right_thread (list<ARDOUR::AudioFileSource*>*);

	void editor_display_control_changed (Editing::DisplayControl c);

	bool have_disk_overrun_displayed;
	bool have_disk_underrun_displayed;

	void disk_overrun_message_gone ();
	void disk_underrun_message_gone ();
	void disk_overrun_handler ();
	void disk_underrun_handler ();

	int pending_state_dialog ();
	
	void disconnect_from_jack ();
	void reconnect_to_jack ();
	void set_jack_buffer_size (jack_nframes_t);

	Gtk::MenuItem* jack_disconnect_item;
	Gtk::MenuItem* jack_reconnect_item;
	Gtk::Menu*     jack_bufsize_menu;

	int make_session_clean ();
	bool filter_ardour_session_dirs (const Gtk::FileFilter::Info&);

	Glib::RefPtr<Gtk::ActionGroup> common_actions;

	void editor_realized ();

	std::vector<std::string> positional_sync_strings;

	void toggle_config_state (const char* group, const char* action, void (ARDOUR::Configuration::*set)(bool));
	void toggle_session_state (const char* group, const char* action, void (ARDOUR::Session::*set)(bool), bool (ARDOUR::Session::*get)(void) const);
	void toggle_session_state (const char* group, const char* action, sigc::slot<void> theSlot);
	void toggle_send_midi_feedback ();
	void toggle_use_mmc ();
	void toggle_send_mmc ();
	void toggle_use_midi_control();
	void toggle_send_mtc ();

	void toggle_AutoConnectNewTrackInputsToHardware();
	void toggle_AutoConnectNewTrackOutputsToHardware();
	void toggle_AutoConnectNewTrackOutputsToMaster();
	void toggle_ManuallyConnectNewTrackOutputs();
	void toggle_UseHardwareMonitoring();
	void toggle_UseSoftwareMonitoring();
	void toggle_UseExternalMonitoring();
	void toggle_StopPluginsWithTransport();
	void toggle_DoNotRunPluginsWhileRecording();
	void toggle_VerifyRemoveLastCapture();
	void toggle_StopRecordingOnXrun();
	void toggle_StopTransportAtEndOfSession();
	void toggle_GainReduceFastTransport();
	void toggle_LatchedSolo();
	void toggle_SoloViaBus();
	void toggle_AutomaticallyCreateCrossfades();
	void toggle_UnmuteNewFullCrossfades();
	void toggle_LatchedRecordEnable ();

	void mtc_port_changed ();
	void map_some_session_state (const char* group, const char* action, bool (ARDOUR::Session::*get)() const);
	void queue_session_control_changed (ARDOUR::Session::ControlType t);
	void session_control_changed (ARDOUR::Session::ControlType t);

	void toggle_control_protocol (ARDOUR::ControlProtocolInfo*);
};

#endif /* __ardour_gui_h__ */

