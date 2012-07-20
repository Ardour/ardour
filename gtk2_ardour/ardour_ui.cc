/*
    Copyright (C) 1999-2007 Paul Davis 

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

#include <algorithm>
#include <cmath>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <cerrno>
#include <fstream>
#include <stdlib.h>
#include <cstring>

#include <iostream>

#include <sys/resource.h>

#include <gtkmm/messagedialog.h>
#include <gtkmm/accelmap.h>
#include <gtkmm2ext/application.h>

#include <pbd/error.h>
#include <pbd/basename.h>
#include <pbd/compose.h>
#include <pbd/pathscanner.h>
#include <pbd/failed_constructor.h>
#include <pbd/enumwriter.h>
#include <pbd/stacktrace.h>
#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/click_box.h>
#include <gtkmm2ext/fastmeter.h>
#include <gtkmm2ext/stop_signal.h>
#include <gtkmm2ext/popup.h>
#include <gtkmm2ext/window_title.h>

#include <midi++/port.h>
#include <midi++/mmc.h>

#include <ardour/ardour.h>
#include <ardour/profile.h>
#include <ardour/session_route.h>
#include <ardour/port.h>
#include <ardour/audioengine.h>
#include <ardour/playlist.h>
#include <ardour/utils.h>
#include <ardour/plugin.h>
#include <ardour/audio_diskstream.h>
#include <ardour/audiofilesource.h>
#include <ardour/recent_sessions.h>
#include <ardour/port.h>
#include <ardour/audio_track.h>
#include <ardour/control_protocol_manager.h>

typedef uint64_t microseconds_t;

#include "actions.h"
#include "ardour_ui.h"
#include "public_editor.h"
#include "audio_clock.h"
#include "keyboard.h"
#include "mixer_ui.h"
#include "prompter.h"
#include "opts.h"
#include "add_route_dialog.h"
#include "new_session_dialog.h"
#include "about.h"
#include "splash.h"
#include "nag.h"
#include "utils.h"
#include "gui_thread.h"
#include "theme_manager.h"
#include "engine_dialog.h"
#include "gain_meter.h"
#include "route_time_axis.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace sigc;

ARDOUR_UI *ARDOUR_UI::theArdourUI = 0;
UIConfiguration *ARDOUR_UI::ui_config = 0;

sigc::signal<void,bool> ARDOUR_UI::Blink;
sigc::signal<void>      ARDOUR_UI::RapidScreenUpdate;
sigc::signal<void>      ARDOUR_UI::SuperRapidScreenUpdate;
sigc::signal<void,nframes_t, bool, nframes_t> ARDOUR_UI::Clock;

ARDOUR_UI::ARDOUR_UI (int *argcp, char **argvp[])

	: Gtkmm2ext::UI (PROGRAM_NAME, argcp, argvp),

	  primary_clock (X_("primary"), false, X_("TransportClockDisplay"), true, false, true),
	  secondary_clock (X_("secondary"), false, X_("SecondaryClockDisplay"), true, false, true),
	  preroll_clock (X_("preroll"), false, X_("PreRollClock"), true, true),
	  postroll_clock (X_("postroll"), false, X_("PostRollClock"), true, true),

	  /* preroll stuff */

	  preroll_button (_("pre\nroll")),
	  postroll_button (_("post\nroll")),

	  /* big clock */

	  big_clock (X_("bigclock"), false, "BigClockNonRecording", true, false, true),

	  /* transport */

	  roll_controllable ("transport roll", *this, TransportControllable::Roll),
	  stop_controllable ("transport stop", *this, TransportControllable::Stop),
	  goto_start_controllable ("transport goto start", *this, TransportControllable::GotoStart),
	  goto_end_controllable ("transport goto end", *this, TransportControllable::GotoEnd),
	  auto_loop_controllable ("transport auto loop", *this, TransportControllable::AutoLoop),
	  play_selection_controllable ("transport play selection", *this, TransportControllable::PlaySelection),
	  rec_controllable ("transport rec-enable", *this, TransportControllable::RecordEnable),
	  shuttle_controllable ("shuttle", *this, TransportControllable::ShuttleControl),
	  shuttle_controller_binding_proxy (shuttle_controllable),

	  roll_button (&roll_controllable),
	  stop_button (&stop_controllable),
	  goto_start_button (&goto_start_controllable),
	  goto_end_button (&goto_end_controllable),
	  auto_loop_button (&auto_loop_controllable),
	  play_selection_button (&play_selection_controllable),
	  rec_button (&rec_controllable),
	  
	  shuttle_units_button (_("% ")),

	  punch_in_button (_("Punch In")),
	  punch_out_button (_("Punch Out")),
	  auto_return_button (_("Auto Return")),
	  auto_play_button (_("Auto Play")),
	  auto_input_button (_("Auto Input")),
	  click_button (_("Click")),
	  time_master_button (_("time\nmaster")),

	  auditioning_alert_button (_("AUDITION")),
	  solo_alert_button (_("SOLO")),
	  shown_flag (false),
	  error_log_button (_("Errors"))
{
	using namespace Gtk::Menu_Helpers;

	Gtkmm2ext::init();

#ifdef TOP_MENUBAR
	_auto_display_errors = false;
#endif

	about = 0;
	splash = 0;

	if (ARDOUR_COMMAND_LINE::session_name.length()) {	
		/* only show this if we're not going to post the new session dialog */
		show_splash ();
	}

	if (theArdourUI == 0) {
		theArdourUI = this;
	}

	ui_config = new UIConfiguration();
	theme_manager = new ThemeManager();

	engine = 0;
	editor = 0;
	mixer = 0;
	session = 0;
	_session_is_new = false;
	big_clock_window = 0;
	session_selector_window = 0;
	new_session_dialog = 0;
	last_key_press_time = 0;
	connection_editor = 0;
	add_route_dialog = 0;
	route_params = 0;
	option_editor = 0;
	location_ui = 0;
	key_editor = 0;
	open_session_selector = 0;
	have_configure_timeout = false;
	have_disk_speed_dialog_displayed = false;
	_will_create_new_session_automatically = false;
	session_loaded = false;
	last_speed_displayed = -1.0f;
	ignore_dual_punch = false;
	_mixer_on_top = false;

	roll_button.unset_flags (Gtk::CAN_FOCUS);
	stop_button.unset_flags (Gtk::CAN_FOCUS);
	goto_start_button.unset_flags (Gtk::CAN_FOCUS);
	goto_end_button.unset_flags (Gtk::CAN_FOCUS);
	auto_loop_button.unset_flags (Gtk::CAN_FOCUS);
	play_selection_button.unset_flags (Gtk::CAN_FOCUS);
	rec_button.unset_flags (Gtk::CAN_FOCUS);

	last_configure_time= 0;

	shuttle_grabbed = false;
	shuttle_fract = 0.0;
	shuttle_max_speed = 8.0f;

	shuttle_style_menu = 0;
	shuttle_unit_menu = 0;

        // We do not have jack linked in yet so;
        
	last_shuttle_request = last_peak_grab = 0; //  get_microseconds();

	ARDOUR::Diskstream::DiskOverrun.connect (mem_fun(*this, &ARDOUR_UI::disk_overrun_handler));
	ARDOUR::Diskstream::DiskUnderrun.connect (mem_fun(*this, &ARDOUR_UI::disk_underrun_handler));

	ARDOUR::Plugin::PresetFileExists.connect (mem_fun(*this, &ARDOUR_UI::preset_file_exists_handler));

	/* handle dialog requests */

	ARDOUR::Session::Dialog.connect (mem_fun(*this, &ARDOUR_UI::session_dialog));

	/* handle pending state with a dialog */

	ARDOUR::Session::AskAboutPendingState.connect (mem_fun(*this, &ARDOUR_UI::pending_state_dialog));

	/* handle sr mismatch with a dialog */

	ARDOUR::Session::AskAboutSampleRateMismatch.connect (mem_fun(*this, &ARDOUR_UI::sr_mismatch_dialog));

	/* lets get this party started */

    	try {
		if (ARDOUR::init (ARDOUR_COMMAND_LINE::use_vst, ARDOUR_COMMAND_LINE::try_hw_optimization)) {
			throw failed_constructor ();
		}

		setup_gtk_ardour_enums ();
		Config->set_current_owner (ConfigVariableBase::Interface);
		setup_profile ();

		GainMeter::setup_slider_pix ();
		RouteTimeAxisView::setup_slider_pix ();

	} catch (failed_constructor& err) {
		error << string_compose (_("could not initialize %1."), PROGRAM_NAME) << endmsg;
		// pass it on up
		throw;
	} 

	/* we like keyboards */

	keyboard = new Keyboard;

	reset_dpi();

	starting.connect (mem_fun(*this, &ARDOUR_UI::startup));
	stopping.connect (mem_fun(*this, &ARDOUR_UI::shutdown));

	platform_setup ();
}

int
ARDOUR_UI::create_engine ()
{
	// this gets called every time by new_session()

	if (engine) {
		return 0;
	}

	loading_message (_("Starting audio engine"));

	try { 
		engine = new ARDOUR::AudioEngine (ARDOUR_COMMAND_LINE::jack_client_name);

	} catch (...) {

		return -1;
	}

	engine->Stopped.connect (mem_fun(*this, &ARDOUR_UI::engine_stopped));
	engine->Running.connect (mem_fun(*this, &ARDOUR_UI::engine_running));
	engine->Halted.connect (bind (mem_fun(*this, &ARDOUR_UI::engine_halted), false));
	engine->SampleRateChanged.connect (mem_fun(*this, &ARDOUR_UI::update_sample_rate));

	post_engine ();

	return 0;
}

void
ARDOUR_UI::post_engine ()
{
	/* Things to be done once we create the AudioEngine
	 */

	check_memory_locking();

       	ActionManager::init ();
	_tooltips.enable();

	if (setup_windows ()) {
		throw failed_constructor ();
	}

	/* this is the first point at which all the keybindings are available */

	if (ARDOUR_COMMAND_LINE::show_key_actions) {
		vector<string> names;
		vector<string> paths;
		vector<string> keys;
		vector<AccelKey> bindings;

		ActionManager::get_all_actions (names, paths, keys, bindings);

		vector<string>::iterator n;
		vector<string>::iterator k;
		for (n = names.begin(), k = keys.begin(); n != names.end(); ++n, ++k) {
			cerr << "Action: " << (*n) << " bound to " << (*k) << endl;
		}

		exit (0);
	}

	blink_timeout_tag = -1;

	/* the global configuration object is now valid */

	use_config ();

	/* this being a GUI and all, we want peakfiles */

	AudioFileSource::set_build_peakfiles (true);
	AudioFileSource::set_build_missing_peakfiles (true);

	/* set default clock modes */

	if (Profile->get_sae()) {
		primary_clock.set_mode (AudioClock::BBT);
		secondary_clock.set_mode (AudioClock::MinSec);
	}  else {
		primary_clock.set_mode (AudioClock::SMPTE);
		secondary_clock.set_mode (AudioClock::BBT);
	}

	/* start the time-of-day-clock */
	
#ifndef GTKOSX
	/* OS X provides an always visible wallclock, so don't be stupid */
	update_wall_clock ();
	Glib::signal_timeout().connect (mem_fun(*this, &ARDOUR_UI::update_wall_clock), 60000);
#endif

	update_disk_space ();
	update_cpu_load ();
	update_sample_rate (engine->frame_rate());

	platform_specific ();

	/* now start and maybe save state */

	if (do_engine_start () == 0) {
		if (session && _session_is_new) {
			/* we need to retain initial visual 
			   settings for a new session 
			*/
			session->save_state ("");
		}
	}
}

ARDOUR_UI::~ARDOUR_UI ()
{
	save_ardour_state ();

	if (keyboard) {
		delete keyboard;
	}

	if (editor) {
		delete editor;
	}

	if (mixer) {
		delete mixer;
	}

	if (add_route_dialog) {
		delete add_route_dialog;
	}

	if (new_session_dialog) {
		delete new_session_dialog;
	}
	
	if (splash) {
		delete splash;
	}
}

void
ARDOUR_UI::pop_back_splash ()
{
	if (Splash::instance()) {
		// Splash::instance()->pop_back();
		Splash::instance()->hide ();
	}
}

gint
ARDOUR_UI::configure_timeout ()
{
	if (last_configure_time == 0) {
		/* no configure events yet */
		return TRUE;
	}

	/* force a gap of 0.5 seconds since the last configure event
	 */

	if (get_microseconds() - last_configure_time < 500000) {
		return TRUE;
	} else {
		have_configure_timeout = false;
		save_ardour_state ();
		return FALSE;
	}
}

gboolean
ARDOUR_UI::configure_handler (GdkEventConfigure* conf)
{
	if (have_configure_timeout) {
		last_configure_time = get_microseconds();
	} else {
		Glib::signal_timeout().connect (mem_fun(*this, &ARDOUR_UI::configure_timeout), 100);
		have_configure_timeout = true;
	}
	
	return FALSE;
}

void
ARDOUR_UI::set_transport_controllable_state (const XMLNode& node)
{
	const XMLProperty* prop;

	if ((prop = node.property ("roll")) != 0) {
		roll_controllable.set_id (prop->value());
	}
	if ((prop = node.property ("stop")) != 0) {
		stop_controllable.set_id (prop->value());
	}
	if ((prop = node.property ("goto_start")) != 0) {
		goto_start_controllable.set_id (prop->value());
	}
	if ((prop = node.property ("goto_end")) != 0) {
		goto_end_controllable.set_id (prop->value());
	}
	if ((prop = node.property ("auto_loop")) != 0) {
		auto_loop_controllable.set_id (prop->value());
	}
	if ((prop = node.property ("play_selection")) != 0) {
		play_selection_controllable.set_id (prop->value());
	}
	if ((prop = node.property ("rec")) != 0) {
		rec_controllable.set_id (prop->value());
	}
	if ((prop = node.property ("shuttle")) != 0) {
		shuttle_controllable.set_id (prop->value());
	}
}

XMLNode&
ARDOUR_UI::get_transport_controllable_state ()
{
	XMLNode* node = new XMLNode(X_("TransportControllables"));
	char buf[64];

	roll_controllable.id().print (buf, sizeof (buf));
	node->add_property (X_("roll"), buf);
	stop_controllable.id().print (buf, sizeof (buf));
	node->add_property (X_("stop"), buf);
	goto_start_controllable.id().print (buf, sizeof (buf));
	node->add_property (X_("goto_start"), buf);
	goto_end_controllable.id().print (buf, sizeof (buf));
	node->add_property (X_("goto_end"), buf);
	auto_loop_controllable.id().print (buf, sizeof (buf));
	node->add_property (X_("auto_loop"), buf);
	play_selection_controllable.id().print (buf, sizeof (buf));
	node->add_property (X_("play_selection"), buf);
	rec_controllable.id().print (buf, sizeof (buf));
	node->add_property (X_("rec"), buf);
	shuttle_controllable.id().print (buf, sizeof (buf));
	node->add_property (X_("shuttle"), buf);

	return *node;
}

void
ARDOUR_UI::save_ardour_state ()
{
	if (!keyboard || !mixer || !editor) {
		return;
	}
	
	/* XXX this is all a bit dubious. add_extra_xml() uses
	   a different lifetime model from add_instant_xml().
	*/

	XMLNode* node = new XMLNode (keyboard->get_state());
	Config->add_extra_xml (*node);
	Config->add_extra_xml (get_transport_controllable_state());
	if (new_session_dialog) {
		if (new_session_dialog->engine_control.was_used()) {
			Config->add_extra_xml (new_session_dialog->engine_control.get_state());
		}
	}
	Config->save_state();
	ui_config->save_state ();

	XMLNode enode(static_cast<Stateful*>(editor)->get_state());
	XMLNode mnode(mixer->get_state());

	if (session) {
		session->add_instant_xml (enode, session->path());
		session->add_instant_xml (mnode, session->path());
	} else {
		Config->add_instant_xml (enode, get_user_ardour_path());
		Config->add_instant_xml (mnode, get_user_ardour_path());
	}

	Keyboard::save_keybindings ();
}

gint
ARDOUR_UI::autosave_session ()
{
	if (g_main_depth() > 1) {
		/* inside a recursive main loop,
		   give up because we may not be able to 
		   take a lock.
		*/
		return 1;
	}

        if (!Config->get_periodic_safety_backups())
                return 1;
        
        if (session) {
                session->maybe_write_autosave();
        }

        return 1;
}

void
ARDOUR_UI::update_autosave ()
{
        ENSURE_GUI_THREAD (mem_fun (*this, &ARDOUR_UI::update_autosave));
        
        if (session && session->dirty()) {
                if (_autosave_connection.connected()) {
                        _autosave_connection.disconnect();
                }

                _autosave_connection = Glib::signal_timeout().connect (mem_fun (*this, &ARDOUR_UI::autosave_session),
								    Config->get_periodic_safety_backup_interval() * 1000);

        } else {
                if (_autosave_connection.connected()) {
                        _autosave_connection.disconnect();
                }               
        }
}

void
ARDOUR_UI::backend_audio_error (bool we_set_params, Gtk::Window* toplevel)
{
	string title;
	if (we_set_params) {
		title = string_compose (_("%1 could not start JACK"), PROGRAM_NAME);
	} else {
		title = string_compose (_("%1 could not connect to JACK."), PROGRAM_NAME);
	}

	MessageDialog win (title,
			   false,
			   Gtk::MESSAGE_INFO,
			   Gtk::BUTTONS_NONE);
	
	if (we_set_params) {
		win.set_secondary_text(_("There are several possible reasons:\n\
\n\
1) You requested audio parameters that are not supported..\n\
2) JACK is running as another user.\n\
\n\
Please consider the possibilities, and perhaps try different parameters."));
	} else {
		win.set_secondary_text(_("There are several possible reasons:\n\
\n\
1) JACK is not running.\n\
2) JACK is running as another user, perhaps root.\n\
3) There is already another client called \"ardour\".\n\
\n\
Please consider the possibilities, and perhaps (re)start JACK."));
	}

	if (toplevel) {
		win.set_transient_for (*toplevel);
	}

	if (we_set_params) {
		win.add_button (Stock::OK, RESPONSE_CLOSE);
	} else {
		win.add_button (Stock::QUIT, RESPONSE_CLOSE);
	}

	win.set_default_response (RESPONSE_CLOSE);
	
	win.show_all ();
	win.set_position (Gtk::WIN_POS_CENTER);
	pop_back_splash ();

	/* we just don't care about the result, but we want to block */

	win.run ();
}

void
ARDOUR_UI::startup ()
{
	Application* app = Application::instance();

	app->ShouldQuit.connect (sigc::mem_fun (*this, &ARDOUR_UI::queue_finish));
	app->ShouldLoad.connect (sigc::mem_fun (*this, &ARDOUR_UI::idle_load));

	string name, path;
	
	new_session_dialog = new NewSessionDialog();

	bool backend_audio_is_running = EngineControl::engine_running();
	XMLNode* audio_setup = Config->extra_xml ("AudioSetup");
	
	if (audio_setup) {
		new_session_dialog->engine_control.set_state (*audio_setup);
	}

	app->ready ();

	if (!get_session_parameters (backend_audio_is_running, ARDOUR_COMMAND_LINE::new_session)) {
		return;
	}
	
	BootMessage (string_compose (_("%1 is ready for use"), PROGRAM_NAME));
	show ();
}

void
ARDOUR_UI::no_memory_warning ()
{
	XMLNode node (X_("no-memory-warning"));
	Config->add_instant_xml (node, get_user_ardour_path());
}

void
ARDOUR_UI::check_memory_locking ()
{
#ifdef __APPLE__
	/* OS X doesn't support mlockall(2), and so testing for memory locking capability there is pointless */
	return;
#else // !__APPLE__

	XMLNode* memory_warning_node = Config->instant_xml (X_("no-memory-warning"), get_user_ardour_path());

	if (engine->is_realtime() && memory_warning_node == 0) {

		struct rlimit limits;
		int64_t ram;
		long pages, page_size;

		if ((page_size = sysconf (_SC_PAGESIZE)) < 0 ||(pages = sysconf (_SC_PHYS_PAGES)) < 0) {
			ram = 0;
		} else {
			ram = (int64_t) pages * (int64_t) page_size;
		}

		if (getrlimit (RLIMIT_MEMLOCK, &limits)) {
			return;
		}
		
		if (limits.rlim_cur != RLIM_INFINITY) {

			if (ram == 0 || ((double) limits.rlim_cur / ram) < 0.75) {
			

				MessageDialog msg (string_compose (_("WARNING: Your system has a limit for maximum amount of locked memory. "
								     "This might cause %1 to run out of memory before your system "
								     "runs out of memory. \n\n"
								     "You can view the memory limit with 'ulimit -l', "
								     "and it is normally controlled by /etc/security/limits.conf"), PROGRAM_NAME));
						   
				VBox* vbox = msg.get_vbox();
				HBox hbox;
				CheckButton cb (_("Do not show this window again"));
				
				cb.signal_toggled().connect (mem_fun (*this, &ARDOUR_UI::no_memory_warning));
				
				hbox.pack_start (cb, true, false);
				vbox->pack_start (hbox);
				hbox.show_all ();

				pop_back_splash ();
				
				msg.run ();
			}
		}
	}
#endif // !__APPLE__
}

void
ARDOUR_UI::queue_finish ()
{
	Glib::signal_idle().connect (mem_fun (*this, &ARDOUR_UI::idle_finish));
}

bool
ARDOUR_UI::idle_finish ()
{
	finish ();
	return false; /* do not call again */
}

void
ARDOUR_UI::finish()
{
	if (session) {

		int tries = 0;

		while (session->transport_rolling() && (++tries < 8)) {
			/* stop but do not abort capture */
			session->request_stop (false, true);
			usleep (10000);
		}

		if (session->dirty()) {
			switch (ask_about_saving_session(_("quit"))) {
			case -1:
				return;
				break;
			case 1:
				/* use the default name */
				if (save_state_canfail ("")) {
					/* failed - don't quit */
					MessageDialog msg (*editor, 
							   _("\
Ardour was unable to save your session.\n\n\
If you still wish to quit, please use the\n\n\
\"Just quit\" option."));
					pop_back_splash();
					msg.run ();
					return;
				}
				break;
			case 0:
				break;
			}
		}
		
		session->set_clean();
		session->set_deletion_in_progress ();
		unload_session(true);
	}

	ArdourDialog::close_all_dialogs ();
	engine->stop (true);
	save_ardour_state ();
	quit ();
}

int
ARDOUR_UI::ask_about_saving_session (const string & what)
{
	ArdourDialog window (_("ardour: save session?"));
	Gtk::HBox dhbox;  // the hbox for the image and text
	Gtk::Label  prompt_label;
	Gtk::Image* dimage = manage (new Gtk::Image(Stock::DIALOG_WARNING,  Gtk::ICON_SIZE_DIALOG));

	string msg;

	msg = string_compose(_("Don't %1"), what);
	window.add_button (msg, RESPONSE_REJECT);
	msg = string_compose(_("Just %1"), what);
	window.add_button (msg, RESPONSE_APPLY);
	msg = string_compose(_("Save and %1"), what);
	window.add_button (msg, RESPONSE_ACCEPT);

	window.set_default_response (RESPONSE_ACCEPT);

	Gtk::Button noquit_button (msg);
	noquit_button.set_name ("EditorGTKButton");

	string prompt;
	string type;

	if (session->snap_name() == session->name()) {
		type = _("session");
	} else {
		type = _("snapshot");
	}
	prompt = string_compose(_("The %1\"%2\"\nhas not been saved.\n\nAny changes made this time\nwill be lost unless you save it.\n\nWhat do you want to do?"), 
			 type, session->snap_name());
	
	prompt_label.set_text (prompt);
	prompt_label.set_name (X_("PrompterLabel"));
	prompt_label.set_alignment(ALIGN_LEFT, ALIGN_TOP);

	dimage->set_alignment(ALIGN_CENTER, ALIGN_TOP)
;
	dhbox.set_homogeneous (false);
	dhbox.pack_start (*dimage, false, false, 5);
	dhbox.pack_start (prompt_label, true, false, 5);
	window.get_vbox()->pack_start (dhbox);

	window.set_name (_("Prompter"));
	window.set_position (Gtk::WIN_POS_MOUSE);
	window.set_modal (true);
	window.set_resizable (false);
	window.show_all ();

	window.set_keep_above (true);
	window.present ();

	ResponseType r = (ResponseType) window.run();

	window.hide ();

	switch (r) {
	case RESPONSE_ACCEPT: // save and get out of here
		return 1;
	case RESPONSE_APPLY:  // get out of here
		return 0;
	default:
		break;
	}

	return -1;
}
	
int
ARDOUR_UI::every_second ()
{
	update_cpu_load ();
	update_buffer_load ();
	update_disk_space ();
	return TRUE;
}

gint
ARDOUR_UI::every_point_one_seconds ()
{
	update_speed_display ();
	RapidScreenUpdate(); /* EMIT_SIGNAL */
	return TRUE;
}

gint
ARDOUR_UI::every_point_zero_one_seconds ()
{
	// august 2007: actual update frequency: 40Hz, not 100Hz

	SuperRapidScreenUpdate(); /* EMIT_SIGNAL */
	return TRUE;
}

void
ARDOUR_UI::update_sample_rate (nframes_t ignored)
{
	char buf[32];

	ENSURE_GUI_THREAD (bind (mem_fun(*this, &ARDOUR_UI::update_sample_rate), ignored));

	if (!engine->connected()) {

		snprintf (buf, sizeof (buf), _("disconnected"));

	} else {

		nframes_t rate = engine->frame_rate();
		
		if (fmod (rate, 1000.0) != 0.0) {
			snprintf (buf, sizeof (buf), _("%.1f kHz / %4.1f ms"), 
				  (float) rate/1000.0f,
				  (engine->frames_per_cycle() / (float) rate) * 1000.0f);
		} else {
			snprintf (buf, sizeof (buf), _("%u kHz / %4.1f ms"), 
				  rate/1000,
				  (engine->frames_per_cycle() / (float) rate) * 1000.0f);
		}
	}

	sample_rate_label.set_text (buf);
}

void
ARDOUR_UI::update_cpu_load ()
{
	char buf[32];
	snprintf (buf, sizeof (buf), _("DSP: %5.1f%%"), engine->get_cpu_load());
	cpu_load_label.set_text (buf);
}

void
ARDOUR_UI::update_buffer_load ()
{
	char buf[64];
	uint32_t c, p;

	if (session) {
		c = session->capture_load ();
		p = session->playback_load ();
		
		snprintf (buf, sizeof (buf), _("Buffers p:%" PRIu32 "%% c:%" PRIu32 "%%"), 
			  session->playback_load(), session->capture_load());
		buffer_load_label.set_text (buf);
	} else {
		buffer_load_label.set_text ("");
	}
}

void
ARDOUR_UI::count_recenabled_streams (Route& route)
{
	Track* track = dynamic_cast<Track*>(&route);
	if (track && track->diskstream()->record_enabled()) {
		rec_enabled_streams += track->n_inputs();
	}
}

void
ARDOUR_UI::update_disk_space()
{
	if (session == 0) {
		return;
	}

	nframes_t frames = session->available_capture_duration();
	char buf[64];

	if (frames == max_frames) {
		strcpy (buf, _("Disk: 24hrs+"));
	} else {
		int hrs;
		int mins;
		int secs;
		nframes_t fr = session->frame_rate();
		
		rec_enabled_streams = 0;
		session->foreach_route (this, &ARDOUR_UI::count_recenabled_streams);
		
		if (rec_enabled_streams) {
			frames /= rec_enabled_streams;
		}
		
		hrs  = frames / (fr * 3600);
		frames -= hrs * fr * 3600;
		mins = frames / (fr * 60);
		frames -= mins * fr * 60;
		secs = frames / fr;
		
		snprintf (buf, sizeof(buf), _("Disk: %02dh:%02dm:%02ds"), hrs, mins, secs);
	}

	disk_space_label.set_text (buf);
}		  

gint
ARDOUR_UI::update_wall_clock ()
{
	time_t now;
	struct tm *tm_now;
	char buf[16];

	time (&now);
	tm_now = localtime (&now);

	sprintf (buf, "%02d:%02d", tm_now->tm_hour, tm_now->tm_min);
	wall_clock_label.set_text (buf);

	return TRUE;
}

gint
ARDOUR_UI::session_menu (GdkEventButton *ev)
{
	session_popup_menu->popup (0, 0);
	return TRUE;
}

void
ARDOUR_UI::redisplay_recent_sessions ()
{
	vector<string *> *sessions;
	vector<string *>::iterator i;
	RecentSessionsSorter cmp;
	
	recent_session_display.set_model (Glib::RefPtr<TreeModel>(0));
	recent_session_model->clear ();

	RecentSessions rs;
	ARDOUR::read_recent_sessions (rs);

	if (rs.empty()) {
		recent_session_display.set_model (recent_session_model);
		return;
	}

	/* sort them alphabetically */
	sort (rs.begin(), rs.end(), cmp);
	sessions = new vector<string*>;

	for (RecentSessions::iterator i = rs.begin(); i != rs.end(); ++i) {
		sessions->push_back (new string ((*i).second));
	}

	for (i = sessions->begin(); i != sessions->end(); ++i) {

		vector<string*>* states;
		vector<const gchar*> item;
		string fullpath = *(*i);
		
		/* remove any trailing / */

		if (fullpath[fullpath.length()-1] == G_DIR_SEPARATOR) {
			fullpath = fullpath.substr (0, fullpath.length()-1);
		}

		/* check whether session still exists */
		if (!Glib::file_test(fullpath.c_str(), Glib::FILE_TEST_EXISTS)) {
			/* session doesn't exist */
			cerr << "skipping non-existent session " << fullpath << endl;
			continue;
		}		
		
		/* now get available states for this session */

		if ((states = Session::possible_states (fullpath)) == 0) {
			/* no state file? */
			continue;
		}

		TreeModel::Row row = *(recent_session_model->append());

		row[recent_session_columns.visible_name] = Glib::path_get_basename (fullpath);
		row[recent_session_columns.fullpath] = fullpath;

		if (states->size() > 1) {

			/* add the children */
			
			for (vector<string*>::iterator i2 = states->begin(); i2 != states->end(); ++i2) {
				
				TreeModel::Row child_row = *(recent_session_model->append (row.children()));

				child_row[recent_session_columns.visible_name] = **i2;
				child_row[recent_session_columns.fullpath] = fullpath;

				delete *i2;
			}
		}

		delete states;
	}

	recent_session_display.set_model (recent_session_model);
	delete sessions;
}

void
ARDOUR_UI::build_session_selector ()
{
	session_selector_window = new ArdourDialog ("session selector");
	
	Gtk::ScrolledWindow *scroller = manage (new Gtk::ScrolledWindow);
	
	session_selector_window->add_button (Stock::CANCEL, RESPONSE_CANCEL);
	session_selector_window->add_button (Stock::OPEN, RESPONSE_ACCEPT);
	session_selector_window->set_default_response (RESPONSE_ACCEPT);
	recent_session_model = TreeStore::create (recent_session_columns);
	recent_session_display.set_model (recent_session_model);
	recent_session_display.append_column (_("Recent Sessions"), recent_session_columns.visible_name);
	recent_session_display.set_headers_visible (false);
	recent_session_display.get_selection()->set_mode (SELECTION_BROWSE);
	recent_session_display.signal_row_activated().connect (mem_fun (*this, &ARDOUR_UI::recent_session_row_activated));

	scroller->add (recent_session_display);
	scroller->set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	session_selector_window->set_name ("SessionSelectorWindow");
	session_selector_window->set_size_request (200, 400);
	session_selector_window->get_vbox()->pack_start (*scroller);
	session_selector_window->show_all_children();
}

void
ARDOUR_UI::recent_session_row_activated (const TreePath& path, TreeViewColumn* col)
{
	session_selector_window->response (RESPONSE_ACCEPT);
}

void
ARDOUR_UI::open_recent_session ()
{
	bool can_return = (session != 0);

	if (session_selector_window == 0) {
		build_session_selector ();
	}
	
	redisplay_recent_sessions ();

	while (true) {
		
		session_selector_window->set_position (WIN_POS_MOUSE);

		ResponseType r = (ResponseType) session_selector_window->run ();
		
		switch (r) {
		case RESPONSE_ACCEPT:
			break;
		default:
			if (can_return) {
				session_selector_window->hide();
				return;
			} else {
				exit (1);
			}
		}

		if (recent_session_display.get_selection()->count_selected_rows() == 0) {
			continue;
		}
		
		session_selector_window->hide();

		Gtk::TreeModel::iterator i = recent_session_display.get_selection()->get_selected();
		
		if (i == recent_session_model->children().end()) {
			return;
		}
		
		std::string path = (*i)[recent_session_columns.fullpath];
		std::string state = (*i)[recent_session_columns.visible_name];
		
		_session_is_new = false;
		
		if (load_session (path, state) == 0) {
			break;
		}

		can_return = false;
	}
}

bool
ARDOUR_UI::check_audioengine ()
{
	if (engine) {
		if (!engine->connected()) {
			MessageDialog msg (string_compose (_("%1 is not connected to JACK\n"
							     "You cannot open or close sessions in this condition"), PROGRAM_NAME));
			pop_back_splash ();
			msg.set_position (WIN_POS_CENTER);
			msg.run ();
			return false;
		}
		return true;
	} else {
		return false;
	}
}

void
ARDOUR_UI::open_session ()
{
	if (!check_audioengine()) {
		return;
	}

	/* popup selector window */

	if (open_session_selector == 0) {

		/* ardour sessions are folders */

		open_session_selector = new Gtk::FileChooserDialog (_("Open Session"), FILE_CHOOSER_ACTION_OPEN);
		open_session_selector->add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
		open_session_selector->add_button (Gtk::Stock::OPEN, Gtk::RESPONSE_ACCEPT);
		open_session_selector->set_default_response(Gtk::RESPONSE_ACCEPT);

		FileFilter session_filter;
		session_filter.add_pattern ("*.ardour");
		session_filter.set_name (_("Ardour sessions"));
		open_session_selector->add_filter (session_filter);
		open_session_selector->set_filter (session_filter);
  	}

	int response = open_session_selector->run();
	open_session_selector->hide ();

	switch (response) {
	case RESPONSE_ACCEPT:
		break;
	default:
		open_session_selector->hide();
		return;
	}

	open_session_selector->hide();
	string session_path = open_session_selector->get_filename();
	string path, name;
	bool isnew;

	if (session_path.length() > 0) {
		if (Session::find_session (session_path, path, name, isnew) == 0) {
			_session_is_new = isnew;
			load_session (path, name);
		}
	}
}


void
ARDOUR_UI::session_add_midi_track ()
{
	cerr << _("Patience is a virtue.\n");
}

void
ARDOUR_UI::session_add_audio_route (bool track, int32_t input_channels, int32_t output_channels, ARDOUR::TrackMode mode, uint32_t how_many)
{
	list<boost::shared_ptr<AudioTrack> > tracks;
	Session::RouteList routes;

	if (session == 0) {
		warning << _("You cannot add a track or bus without a session already loaded.") << endmsg;
		return;
	}

	try { 
		if (track) {
			tracks = session->new_audio_track (input_channels, output_channels, mode, how_many);

			if (tracks.size() != how_many) {
				if (how_many == 1) {
					error << _("could not create a new audio track") << endmsg;
				} else {
					error << string_compose (_("could only create %1 of %2 new audio %3"), 
								 tracks.size(), how_many, (track ? _("tracks") : _("busses"))) << endmsg;
				}
			}

		} else {

			routes = session->new_audio_route (input_channels, output_channels, how_many);

			if (routes.size() != how_many) {
				if (how_many == 1) {
					error << _("could not create a new audio track") << endmsg;
				} else {
					error << string_compose (_("could not create %1 new audio tracks"), how_many) << endmsg;
				}
			}
		}
		
#if CONTROLOUTS
		if (need_control_room_outs) {
			pan_t pans[2];
			
			pans[0] = 0.5;
			pans[1] = 0.5;
			
			route->set_stereo_control_outs (control_lr_channels);
			route->control_outs()->set_stereo_pan (pans, this);
		}
#endif /* CONTROLOUTS */
	}

	catch (...) {
		MessageDialog msg (*editor, 
				   _("There are insufficient JACK ports available\n\
to create a new track or bus.\n\
You should save Ardour, exit and\n\
restart JACK with more ports."));
		pop_back_splash ();
		msg.run ();
	}
}

void
ARDOUR_UI::do_transport_locate (nframes_t new_position)
{
	nframes_t _preroll = 0;

	if (session) {
		// XXX CONFIG_CHANGE FIX - requires AnyTime handling
		// _preroll = session->convert_to_frames_at (new_position, Config->get_preroll());

		if (new_position > _preroll) {
			new_position -= _preroll;
		} else {
			new_position = 0;
		}

		session->request_locate (new_position);
	}
}

void
ARDOUR_UI::transport_goto_start ()  
{
	if (session) {
		session->goto_start();

		
		/* force displayed area in editor to start no matter
		   what "follow playhead" setting is.
		*/
		
		if (editor) {
			editor->reset_x_origin (session->current_start_frame());
		}
	}
}

void
ARDOUR_UI::transport_goto_zero ()
{
	if (session) {
		session->request_locate (0);

		
		/* force displayed area in editor to start no matter
		   what "follow playhead" setting is.
		*/
		
		if (editor) {
			editor->reset_x_origin (0);
		}
	}
}

void
ARDOUR_UI::transport_goto_wallclock ()
{
	if (session && editor) {

		time_t now;
		struct tm tmnow;
		nframes64_t frames;
		
		time (&now);
		localtime_r (&now, &tmnow);
	
		frames = tmnow.tm_hour * (60 * 60 * session->frame_rate());
		frames += tmnow.tm_min * (60 * session->frame_rate());
		frames += tmnow.tm_sec * session->frame_rate();

		session->request_locate (frames);

		/* force displayed area in editor to start no matter
		   what "follow playhead" setting is.
		*/
		
		if (editor) {
			editor->reset_x_origin (frames - (editor->current_page_frames()/2));
		}
	}
}

void
ARDOUR_UI::transport_goto_end ()
{
	if (session) {
		nframes_t frame = session->current_end_frame();
		session->request_locate (frame);

		/* force displayed area in editor to start no matter
		   what "follow playhead" setting is.
		*/
		
		if (editor) {
			editor->reset_x_origin (frame);
		}
	}
}

void
ARDOUR_UI::transport_stop ()
{
	if (!session) {
		return;
	}

	if (session->is_auditioning()) {
		session->cancel_audition ();
		return;
	}
	
	session->request_stop (false, true);
}

void
ARDOUR_UI::transport_stop_and_forget_capture ()
{
	if (session) {
		session->request_stop (true, true);
	}
}

void
ARDOUR_UI::remove_last_capture()
{
	if (editor) {
		editor->remove_last_capture();
	}
}

void
ARDOUR_UI::transport_record (bool roll)
{
	
	if (session) {
		switch (session->record_status()) {
		case Session::Disabled:
			if (session->ntracks() == 0) {
				MessageDialog msg (*editor, _("Please create 1 or more track\nbefore trying to record.\nCheck the Session menu."));
				msg.run ();
				return;
			}
			session->maybe_enable_record ();
			if (roll) {
				transport_roll ();
			}
			break;
		case Session::Recording:
			if (roll) {
				session->request_stop();
			} else {
				session->disable_record (false, true);
			}
			break;

		case Session::Enabled:
			session->disable_record (false, true);
		}
	}
	//cerr << "ARDOUR_UI::transport_record () called roll = " << roll << " session->record_status() = " << session->record_status() << endl;
}

void 
ARDOUR_UI::transport_roll ()
{
	if (!session) {
		return;
	}

	if (session->is_auditioning()) {
		return;
	}
	
	switch (Config->get_slave_source()) {
	case None:
	case JACK:
		break;
	default:
		/* transport controlled by the master */
		return;
	}

	bool rolling = session->transport_rolling();

	if (session->get_play_loop()) {
		/* XXX it is not possible to just leave seamless loop and keep
		   playing at present (nov 4th 2009)
		*/
		if (!Config->get_seamless_loop()) {
			/* off, and stop */
			session->request_play_loop (false, true);
		} else {
			return;
		}
	} else if (session->get_play_range ()) {
		session->request_play_range (0, true);
	} 

	if (!rolling) {
		session->request_transport_speed (1.0f);
	}

	map_transport_state ();
}

void
ARDOUR_UI::toggle_roll (bool with_abort, bool roll_out_of_bounded_mode)
{
	
	if (!session) {
		return;
	}

	if (session->is_auditioning()) {
		session->cancel_audition ();
		return;
	}
	
	switch (Config->get_slave_source()) {
	case None:
	case JACK:
		break;
	default:
		/* transport controlled by the master */
		return;
	}

	bool rolling = session->transport_rolling();
	bool affect_transport = true;

	if (rolling && roll_out_of_bounded_mode) {
		/* drop out of loop/range playback but leave transport rolling */
		if (session->get_play_loop()) {
			if (Config->get_seamless_loop()) {
				/* the disk buffers contain copies of the loop - we can't 
				   just keep playing, so stop the transport. the user
				   can restart as they wish.
				*/
				affect_transport = true;
			} else {
				/* disk buffers are normal, so we can keep playing */
				affect_transport = false;
			}
			session->request_play_loop (false, true);
		} else if (session->get_play_range ()) {
			affect_transport = false;
			session->request_play_range (0, true);
		} 
	} 

	if (affect_transport) {
		if (rolling) {
			session->request_stop (with_abort, true);
		} else {
			session->request_transport_speed (1.0f);
		}
	}

	map_transport_state ();
}

void
ARDOUR_UI::toggle_session_auto_loop ()
{
	if (session) {
		if (session->get_play_loop()) {
			if (session->transport_rolling()) {
				Location * looploc = session->locations()->auto_loop_location();
				if (looploc) {
					session->request_locate (looploc->start(), true);
				}
			} else {
				session->request_play_loop (false);
			}
		} else {
			Location * looploc = session->locations()->auto_loop_location();
			if (looploc) {
				session->request_play_loop (true);
			}
		}
	}

	Location *loop_loc = session->locations()->auto_loop_location();
	if (loop_loc)
		loop_loc->set_hidden (false, this);
}

void
ARDOUR_UI::transport_play_selection ()
{
	if (!session) {
		return;
	}

	editor->play_selection ();
}

void
ARDOUR_UI::transport_rewind (int option)
{
	float current_transport_speed;
 
       	if (session) {
		current_transport_speed = session->transport_speed();
		
		if (current_transport_speed >= 0.0f) {
			switch (option) {
			case 0:
				session->request_transport_speed (-1.0f);
				break;
			case 1:
				session->request_transport_speed (-4.0f);
				break;
			case -1:
				session->request_transport_speed (-0.5f);
				break;
			}
		} else {
			/* speed up */
			session->request_transport_speed (current_transport_speed * 1.5f);
		}
	}
}

void
ARDOUR_UI::transport_forward (int option)
{
	float current_transport_speed;
	
	if (session) {
		current_transport_speed = session->transport_speed();
		
		if (current_transport_speed <= 0.0f) {
			switch (option) {
			case 0:
				session->request_transport_speed (1.0f);
				break;
			case 1:
				session->request_transport_speed (4.0f);
				break;
			case -1:
				session->request_transport_speed (0.5f);
				break;
			}
		} else {
			/* speed up */
			session->request_transport_speed (current_transport_speed * 1.5f);
		}
	}
}

void
ARDOUR_UI::toggle_record_enable (uint32_t dstream)
{
	if (session == 0) {
		return;
	}

	boost::shared_ptr<Route> r;
	
	if ((r = session->route_by_remote_id (dstream)) != 0) {

		Track* t;

		if ((t = dynamic_cast<Track*>(r.get())) != 0) {
			t->diskstream()->set_record_enabled (!t->diskstream()->record_enabled());
		}
	} 
}

void
ARDOUR_UI::queue_transport_change ()
{
	Gtkmm2ext::UI::instance()->call_slot (mem_fun(*this, &ARDOUR_UI::map_transport_state));
}

void
ARDOUR_UI::map_transport_state ()
{
	if (!session) {
		auto_loop_button.set_visual_state (0);
		play_selection_button.set_visual_state (0);
		roll_button.set_visual_state (0);
		stop_button.set_visual_state (1);
		return;
	}

	float sp = session->transport_speed();

	if (sp == 1.0f) {
		shuttle_fract = SHUTTLE_FRACT_SPEED1;  /* speed = 1.0, believe it or not */
		shuttle_box.queue_draw ();
	} else if (sp == 0.0f) {
		shuttle_fract = 0;
		shuttle_box.queue_draw ();
		update_disk_space ();
	}

	if (sp != 0.0) {

		if (session->get_play_range()) {

			play_selection_button.set_visual_state (1);
			roll_button.set_visual_state (0);
			auto_loop_button.set_visual_state (0);
			
		} else if (session->get_play_loop ()) {
			
			auto_loop_button.set_visual_state (1);
			play_selection_button.set_visual_state (0);
			roll_button.set_visual_state (0);

		} else {
			
			roll_button.set_visual_state (1);
			play_selection_button.set_visual_state (0);
			auto_loop_button.set_visual_state (0);
		}

		stop_button.set_visual_state (0);

	} else {

		stop_button.set_visual_state (1);
		roll_button.set_visual_state (0);
		play_selection_button.set_visual_state (0);
		auto_loop_button.set_visual_state (0);
	}

}

void
ARDOUR_UI::GlobalClickBox::printer (char buf[32], Adjustment &adj, void *arg)
{
	snprintf (buf, sizeof(buf), "%s", ((GlobalClickBox *) arg)->strings[
		(int) adj.get_value()].c_str());
}

void
ARDOUR_UI::engine_stopped ()
{
	ENSURE_GUI_THREAD (mem_fun(*this, &ARDOUR_UI::engine_stopped));
	ActionManager::set_sensitive (ActionManager::jack_sensitive_actions, false);
	ActionManager::set_sensitive (ActionManager::jack_opposite_sensitive_actions, true);
}

void
ARDOUR_UI::engine_running ()
{
	ENSURE_GUI_THREAD (mem_fun(*this, &ARDOUR_UI::engine_running));
	ActionManager::set_sensitive (ActionManager::jack_sensitive_actions, true);
	ActionManager::set_sensitive (ActionManager::jack_opposite_sensitive_actions, false);

	Glib::RefPtr<Action> action;
	const char* action_name = 0;

	switch (engine->frames_per_cycle()) {
	case 32:
		action_name = X_("JACKLatency32");
		break;
	case 64:
		action_name = X_("JACKLatency64");
		break;
	case 128:
		action_name = X_("JACKLatency128");
		break;
	case 512:
		action_name = X_("JACKLatency512");
		break;
	case 1024:
		action_name = X_("JACKLatency1024");
		break;
	case 2048:
		action_name = X_("JACKLatency2048");
		break;
	case 4096:
		action_name = X_("JACKLatency4096");
		break;
	case 8192:
		action_name = X_("JACKLatency8192");
		break;
	default:
		/* XXX can we do anything useful ? */
		break;
	}

	if (action_name) {

		action = ActionManager::get_action (X_("JACK"), action_name);
		
		if (action) {
			Glib::RefPtr<RadioAction> ract = Glib::RefPtr<RadioAction>::cast_dynamic (action);
			ract->set_active ();
		}
	}
}

void
ARDOUR_UI::engine_halted (const char* reason, bool free_reason)
{
	if (!Gtkmm2ext::UI::instance()->caller_is_ui_thread()) {
		/* we can't rely on the original string continuing to exist when we are called
		   again in the GUI thread, so make a copy and note that we need to
		   free it later.
		*/
		char *copy = strdup (reason);
		Gtkmm2ext::UI::instance()->call_slot (bind (mem_fun (*this, &ARDOUR_UI::engine_halted), copy, true));
		return;
	} 

	ActionManager::set_sensitive (ActionManager::jack_sensitive_actions, false);
	ActionManager::set_sensitive (ActionManager::jack_opposite_sensitive_actions, true);

	update_sample_rate (0);

	string msgstr;

	/* if the reason is a non-empty string, it means that the backend was shutdown
	   rather than just Ardour.
	*/

	if (strlen (reason)) {
		msgstr = string_compose (_("The audio backend (JACK) was shutdown because:\n\n%1"), reason);
	} else {
		msgstr = _("\
JACK has either been shutdown or it\n\
disconnected Ardour because Ardour\n\
was not fast enough. Try to restart\n\
JACK, reconnect and save the session.");
	}

	MessageDialog msg (*editor, msgstr);
	pop_back_splash ();
	msg.run ();

	if (free_reason) {
		free ((char *) reason);
	}
}

int32_t
ARDOUR_UI::do_engine_start ()
{
	try { 
		engine->start();
	}

	catch (...) {
		engine->stop ();
		error << _("Unable to start the session running")
		      << endmsg;
		unload_session ();
		return -2;
	}
	
	return 0;
}

void
ARDOUR_UI::setup_theme ()
{
	theme_manager->setup_theme();
}

void
ARDOUR_UI::update_clocks ()
{
	if (!editor || !editor->dragging_playhead()) {
		Clock (session->audible_frame(), false, editor->get_preferred_edit_position()); /* EMIT_SIGNAL */
	}
}

void
ARDOUR_UI::start_clocking ()
{
	clock_signal_connection = RapidScreenUpdate.connect (mem_fun(*this, &ARDOUR_UI::update_clocks));
}

void
ARDOUR_UI::stop_clocking ()
{
	clock_signal_connection.disconnect ();
}
	
void
ARDOUR_UI::toggle_clocking ()
{
#if 0
	if (clock_button.get_active()) {
		start_clocking ();
	} else {
		stop_clocking ();
	}
#endif
}

gint
ARDOUR_UI::_blink (void *arg)

{
	((ARDOUR_UI *) arg)->blink ();
	return TRUE;
}

void
ARDOUR_UI::blink ()
{
	Blink (blink_on = !blink_on); /* EMIT_SIGNAL */
}

void
ARDOUR_UI::start_blinking ()
{
	/* Start the blink signal. Everybody with a blinking widget
	   uses Blink to drive the widget's state.
	*/

	if (blink_timeout_tag < 0) {
		blink_on = false;	
		blink_timeout_tag = g_timeout_add (240, _blink, this);
	}
}

void
ARDOUR_UI::stop_blinking ()
{
	if (blink_timeout_tag >= 0) {
		g_source_remove (blink_timeout_tag);
		blink_timeout_tag = -1;
	}
}

void
ARDOUR_UI::name_io_setup (AudioEngine& engine, 
			  string& buf,
			  IO& io,
			  bool in)
{
	if (in) {
		if (io.n_inputs() == 0) {
			buf = _("none");
			return;
		}
		
		/* XXX we're not handling multiple ports yet. */

		const char **connections = io.input(0)->get_connections();
		
		if (connections == 0 || connections[0] == '\0') {
			buf = _("off");
		} else {
			buf = connections[0];
		}

		free (connections);

	} else {

		if (io.n_outputs() == 0) {
			buf = _("none");
			return;
		}
		
		/* XXX we're not handling multiple ports yet. */

		const char **connections = io.output(0)->get_connections();
		
		if (connections == 0 || connections[0] == '\0') {
			buf = _("off");
		} else {
			buf = connections[0];
		}

		free (connections);
	}
}

/** Ask the user for the name of a new shapshot and then take it.
 */
void
ARDOUR_UI::snapshot_session (bool switch_to_it)
{
	ArdourPrompter prompter (true);
	string snapname;

	prompter.set_name ("Prompter");
	prompter.add_button (Gtk::Stock::SAVE, Gtk::RESPONSE_ACCEPT);
	prompter.set_title (_("Take Snapshot"));
	prompter.set_prompt (_("Name of New Snapshot"));

	if (!switch_to_it) {
		char timebuf[128];
		time_t n;
		struct tm local_time;
		
		time (&n);
		localtime_r (&n, &local_time);
		strftime (timebuf, sizeof(timebuf), "%FT%H.%M.%S", &local_time);
		
		prompter.set_initial_text (timebuf);
	}

  again:
	switch (prompter.run()) {
	case RESPONSE_ACCEPT:
		prompter.get_result (snapname);
		if (snapname.length()){
			if (snapname.find ('/') != string::npos) {
				MessageDialog msg (_("To ensure compatibility with various systems\n"
						     "snapshot names may not contain a '/' character"));
				msg.run ();
				goto again;
			}
			if (snapname.find ('\\') != string::npos) {
				MessageDialog msg (_("To ensure compatibility with various systems\n"
						     "snapshot names may not contain a '\\' character"));
				msg.run ();
				goto again;
			}
			if (snapname.find (':') != string::npos) {
				MessageDialog msg (_("To ensure compatibility with various systems\n"
						     "snapshot names may not contain a ':' character"));
				msg.run ();
				goto again;
			}
			save_state (snapname, switch_to_it);
		}
		break;

	default:
		break;
	}
}

void
ARDOUR_UI::save_state (const string & name, bool switch_to_it)
{
	(void) save_state_canfail (name, switch_to_it);
}
		
int
ARDOUR_UI::save_state_canfail (string name, bool switch_to_it)
{
	if (session) {
		int ret;

		if (name.length() == 0) {
			name = session->snap_name();
		}

		if ((ret = session->save_state (name, false, switch_to_it)) != 0) {
			return ret;
		}
	}
	save_ardour_state (); /* XXX cannot fail? yeah, right ... */
	return 0;
}

void
ARDOUR_UI::primary_clock_value_changed ()
{
	if (session) {
		session->request_locate (primary_clock.current_time ());
	}
}

void
ARDOUR_UI::big_clock_value_changed ()
{
	if (session) {
		session->request_locate (big_clock.current_time ());
	}
}

void
ARDOUR_UI::secondary_clock_value_changed ()
{
	if (session) {
		session->request_locate (secondary_clock.current_time ());
	}
}

void
ARDOUR_UI::transport_rec_enable_blink (bool onoff) 
{
	if (session == 0) {
		return;
	}
	
	switch (session->record_status()) {
	case Session::Enabled:
		if (onoff) {
			rec_button.set_visual_state (2);
		} else {
			rec_button.set_visual_state (0);
		}
		break;

	case Session::Recording:
		rec_button.set_visual_state (1);
		break;

	default:
		rec_button.set_visual_state (0);
		break;
	}
}

void
ARDOUR_UI::save_template ()
{
	ArdourPrompter prompter (true);
	string name;

	if (!check_audioengine()) {
		return;
	}

	prompter.set_name (X_("Prompter"));
	prompter.set_title (_("Save Mix Template"));
	prompter.set_prompt (_("Name for mix template:"));
	prompter.set_initial_text(session->name() + _("-template"));
	prompter.add_button (Gtk::Stock::SAVE, Gtk::RESPONSE_ACCEPT);

	switch (prompter.run()) {
	case RESPONSE_ACCEPT:
		prompter.get_result (name);
		
		if (name.length()) {
			session->save_template (name);
		}
		break;

	default:
		break;
	}
}

void
ARDOUR_UI::fontconfig_dialog ()
{
#if 0
  /* this issue seems to have gone away with changes to font handling in GTK/Quartz
   */
#ifdef GTKOSX
	/* X11 users will always have fontconfig info around, but new GTK-OSX users 
	   may not and it can take a while to build it. Warn them.
	*/
	
	std::string fontconfig = Glib::build_filename (Glib::get_home_dir(), ".fontconfig");
	
	if (!Glib::file_test (fontconfig, Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_DIR)) {
		MessageDialog msg (*new_session_dialog,
				   string_compose (_("Welcome to %1.\n\n"
						     "The program will take a bit longer to start up\n"
						     "while the system fonts are checked.\n\n"
						     "This will only be done once, and you will\n"
						     "not see this message again\n"), PROGRAM_NAME),
				   true,
				   Gtk::MESSAGE_INFO,
				   Gtk::BUTTONS_OK);
		pop_back_splash ();
		msg.show_all ();
		msg.present ();
		msg.run ();
	}
#endif
#endif
}

void
ARDOUR_UI::parse_cmdline_path (const std::string& cmdline_path, std::string& session_name, std::string& session_path, bool& existing_session)
{
	existing_session = false;

	if (Glib::file_test (cmdline_path, Glib::FILE_TEST_IS_DIR)) {
		session_path = cmdline_path;
		existing_session = true;
	} else if (Glib::file_test (cmdline_path, Glib::FILE_TEST_IS_REGULAR)) {
		session_path = Glib::path_get_dirname (string (cmdline_path));
		existing_session = true;
	} else {
		/* it doesn't exist, assume the best */
		session_path = Glib::path_get_dirname (string (cmdline_path));
	}
	
	session_name = basename_nosuffix (string (cmdline_path));
}

int
ARDOUR_UI::load_cmdline_session (const std::string& session_name, const std::string& session_path, bool& existing_session)
{
	/* when this is called, the backend audio system must be running */

	/* the main idea here is to deal with the fact that a cmdline argument for the session
	   can be interpreted in different ways - it could be a directory or a file, and before
	   we load, we need to know both the session directory and the snapshot (statefile) within it
	   that we are supposed to use.
	*/

	if (session_name.length() == 0 || session_path.length() == 0) {
		return false;
	}
	
	if (Glib::file_test (session_path, Glib::FILE_TEST_IS_DIR)) {

		std::string predicted_session_file;
                
                predicted_session_file = Glib::build_filename (session_path, session_name + Session::statefile_suffix());
		
		if (Glib::file_test (predicted_session_file, Glib::FILE_TEST_EXISTS)) {
			existing_session = true;
		}
		
	} else if (Glib::file_test (session_path, Glib::FILE_TEST_EXISTS)) {
		
		if (session_path.find (Session::statefile_suffix()) == session_path.length() - 7) {
			/* existing .ardour file */
			existing_session = true;
		}

	} else {
		existing_session = false;
	}
	
	/* lets just try to load it */
	
	if (create_engine ()) {
		backend_audio_error (false, new_session_dialog);
		return -1;
	}
	
	return load_session (session_path, session_name);
}

bool
ARDOUR_UI::ask_about_loading_existing_session (const std::string& session_path)
{
	std::string str = string_compose (_("This session\n%1\nalready exists. Do you want to open it?"), session_path);
	
	MessageDialog msg (str,
			   false,
			   Gtk::MESSAGE_WARNING,
			   Gtk::BUTTONS_YES_NO,
			   true);
	
	
        msg.set_name (X_("OpenExistingDialog"));
        msg.set_title (_("Open Existing Session"));
	msg.set_wmclass (X_("existing_session"), PROGRAM_NAME);
	msg.set_position (Gtk::WIN_POS_MOUSE);
	pop_back_splash ();

	switch (msg.run()) {
	case RESPONSE_YES:
		return true;
		break;
	}
	return false;
}

int
ARDOUR_UI::build_session_from_nsd (const std::string& session_path, const std::string& session_name)
{
	
	uint32_t cchns;
	uint32_t mchns;
	AutoConnectOption iconnect;
	AutoConnectOption oconnect;
	uint32_t nphysin;
	uint32_t nphysout;
	
	if (Profile->get_sae()) {
		
		cchns = 0;
		mchns = 2;
		iconnect = AutoConnectPhysical;
		oconnect = AutoConnectMaster;
		nphysin = 0; // use all available
		nphysout = 0; // use all available
		
	} else {
		
		/* get settings from advanced section of NSD */
		
		if (new_session_dialog->create_control_bus()) {
			cchns = (uint32_t) new_session_dialog->control_channel_count();
		} else {
			cchns = 0;
		}
		
		if (new_session_dialog->create_master_bus()) {
			mchns = (uint32_t) new_session_dialog->master_channel_count();
		} else {
			mchns = 0;
		}
		
		if (new_session_dialog->connect_inputs()) {
			iconnect = AutoConnectPhysical;
		} else {
			iconnect = AutoConnectOption (0);
		}
		
		/// @todo some minor tweaks.

		oconnect = AutoConnectOption (0);
		
		if (new_session_dialog->connect_outputs()) {
			if (new_session_dialog->connect_outs_to_master()) {
				oconnect = AutoConnectMaster;
			} else if (new_session_dialog->connect_outs_to_physical()) {
				oconnect = AutoConnectPhysical;
			}
		}
		
		nphysin = (uint32_t) new_session_dialog->input_limit_count();
		nphysout = (uint32_t) new_session_dialog->output_limit_count();
	}
	
	if (build_session (session_path,
			   session_name,
			   cchns,
			   mchns,
			   iconnect,
			   oconnect,
			   nphysin,
			   nphysout, 
			   engine->frame_rate() * 60 * 5)) {
		
		return -1;
	}

	return 0;
}

void
ARDOUR_UI::end_loading_messages ()
{
	// hide_splash ();
}

void
ARDOUR_UI::loading_message (const std::string& msg)
{
	show_splash ();
	splash->message (msg);
	flush_pending ();
}

void
ARDOUR_UI::idle_load (const std::string& path)
{
	if (session) {

		if (Glib::file_test (path, Glib::FILE_TEST_IS_DIR)) {
			/* /path/to/foo => /path/to/foo, foo */
			load_session (path, basename_nosuffix (path));
		} else {
			/* /path/to/foo/foo.ardour => /path/to/foo, foo */
			load_session (Glib::path_get_dirname (path), basename_nosuffix (path));
		}

	} else {

		ARDOUR_COMMAND_LINE::session_name = path;

		if (new_session_dialog) {

			/* make it break out of Dialog::run() and
			   start again.
			 */

			new_session_dialog->response (1);
		}
	}
}

bool
ARDOUR_UI::get_session_parameters (bool backend_audio_is_running, bool should_be_new)
{
	bool existing_session = false;
	std::string session_name;
	std::string session_path;
	std::string template_name;
	int response;
	
  begin:
	response = Gtk::RESPONSE_NONE;

	if (!ARDOUR_COMMAND_LINE::session_name.empty()) {

		parse_cmdline_path (ARDOUR_COMMAND_LINE::session_name, session_name, session_path, existing_session);

		/* don't ever reuse this */

		ARDOUR_COMMAND_LINE::session_name = string();

		if (existing_session && backend_audio_is_running) {

			/* just load the thing already */

			if (load_cmdline_session (session_name, session_path, existing_session) == 0) {
				return true;
			}
		}

		/* make the NSD use whatever information we have */

		new_session_dialog->set_session_name (session_name);
		new_session_dialog->set_session_folder (session_path);
	}

	/* loading failed, or we need the NSD for something */

	new_session_dialog->set_modal (false);
	new_session_dialog->set_position (WIN_POS_CENTER);
	new_session_dialog->set_current_page (0);
	new_session_dialog->set_existing_session (existing_session);
	new_session_dialog->reset_recent();

	do {
		new_session_dialog->set_have_engine (backend_audio_is_running);
		new_session_dialog->present ();
		response = new_session_dialog->run ();
		
                cerr << "NSD response " << response << " folder = " << new_session_dialog->session_folder() << endl;

		_session_is_new = false;
                
		/* handle possible negative responses */

		switch (response) {
		case 1:
			/* sent by idle_load, meaning restart the whole process again */
			new_session_dialog->hide();
			new_session_dialog->reset();
			goto begin;
			break;

		case Gtk::RESPONSE_CANCEL:
		case Gtk::RESPONSE_DELETE_EVENT:
			if (!session) {
				if (engine && engine->running()) {
					engine->stop (true);
				}
				quit();
			}
			new_session_dialog->hide ();
			return false;
			
		case Gtk::RESPONSE_NONE:
			/* "Clear" was pressed */
			goto try_again;
		}

		fontconfig_dialog();

		if (!backend_audio_is_running) {
			int ret = new_session_dialog->engine_control.setup_engine ();
			if (ret < 0) {
				return false;
			} else if (ret > 0) {
				response = Gtk::RESPONSE_REJECT;
				goto try_again;
			}
			
			/* hide the NSD while we start up the engine */

			new_session_dialog->hide ();
			flush_pending ();
		}

		if (create_engine ()) {

			backend_audio_error (!backend_audio_is_running, new_session_dialog);
			flush_pending ();

			new_session_dialog->set_existing_session (false);
			new_session_dialog->set_current_page (0); // new engine page
			new_session_dialog->engine_control.unset_interface_chosen ();

			response = Gtk::RESPONSE_NONE;
			goto try_again;
		}

		backend_audio_is_running = true;		
			
		if (response == Gtk::RESPONSE_OK) {

			session_name = new_session_dialog->session_name();

			if (session_name.empty()) {
				response = Gtk::RESPONSE_NONE;
				goto try_again;
			} 

			/* if the user mistakenly typed path information into the session filename entry,
			   convert what they typed into a path & a name
			*/
			
			if (Glib::path_is_absolute (session_name) || 
			    (session_name.length() > 2 && session_name[0] == '.' && session_name[1] == G_DIR_SEPARATOR) ||
			    (session_name.length() > 3 && session_name[0] == '.' && session_name[1] == '.' && session_name[2] == G_DIR_SEPARATOR)) {

				session_path = Glib::path_get_dirname (session_name);
				session_name = Glib::path_get_basename (session_name);
				
			} else {

				session_path = new_session_dialog->session_folder();

			}

			template_name = std::string();			
			switch (new_session_dialog->which_page()) {

			case NewSessionDialog::OpenPage: 
				goto loadit;
				break;

			case NewSessionDialog::EnginePage:
				if (new_session_dialog->engine_control.interface_chosen() && !session_path.empty()) {
					goto loadit;
				} else {
					goto try_again;
				}
				break;

			case NewSessionDialog::NewPage: /* nominally the "new" session creator, but could be in use for an old session */
				
				should_be_new = true;

				if (session_name.find ('/') != std::string::npos) {
					MessageDialog msg (*new_session_dialog, _("To ensure compatibility with various systems\n"
							 "session names may not contain a '/' character"));
					msg.run ();
					response = RESPONSE_NONE;
					goto try_again;
				}

				if (session_name.find ('\\') != std::string::npos) {
					MessageDialog msg (*new_session_dialog, _("To ensure compatibility with various systems\n"
							 "session names may not contain a '\\' character"));
					msg.run ();
					response = RESPONSE_NONE;
					goto try_again;
				}

				//XXX This is needed because session constructor wants a 
				//non-existant path. hopefully this will be fixed at some point.

				session_path = Glib::build_filename (session_path, session_name);

				if (Glib::file_test (session_path, Glib::FileTest (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))) {

					new_session_dialog->hide ();

					if (ask_about_loading_existing_session (session_path)) {
						goto loadit;
					} else {
						response = RESPONSE_NONE;
						goto try_again;
					} 
				}

			        _session_is_new = true;
						
				if (new_session_dialog->use_session_template()) {

					template_name = new_session_dialog->session_template_name();
					goto loadit;
			  
				} else {
					if (build_session_from_nsd (session_path, session_name)) {
						response = RESPONSE_NONE;
						goto try_again;
					}
					goto done;
				}
				break;
				
			default:
				break;
			}
			
		  loadit:
			new_session_dialog->hide ();

			if (load_session (session_path, session_name, template_name)) {
				/* force a retry */
				response = Gtk::RESPONSE_NONE;
			}

		  try_again:
			if (response == Gtk::RESPONSE_NONE) {
				new_session_dialog->set_existing_session (false);
				new_session_dialog->reset ();
			}
		}

	} while (response == Gtk::RESPONSE_NONE || response == Gtk::RESPONSE_REJECT);

  done:
	show();
	new_session_dialog->hide();
	new_session_dialog->reset();
	goto_editor_window ();
	return true;
}	

void
ARDOUR_UI::close_session ()
{
	if (!check_audioengine()) {
		return;
	}

	if (unload_session (true)) {
		return;
	}
	
	get_session_parameters (true, false);
}

int
ARDOUR_UI::load_session (const std::string& path, const std::string& snap_name, std::string mix_template)
{
	Session *new_session;
	int unload_status;
	int retval = -1;

	session_loaded = false;

	if (!check_audioengine()) {
		return -1;
	}

	unload_status = unload_session ();

	if (unload_status < 0) {
		goto out;
	} else if (unload_status > 0) {
		retval = 0;
		goto out;
	}

	loading_message (string_compose (_("Please wait while %1 loads your session"), PROGRAM_NAME));

	try {
		new_session = new Session (*engine, path, snap_name, mix_template);
	}

	/* this one is special */

	catch (AudioEngine::PortRegistrationFailure& err) {

		MessageDialog msg (err.what(),
				   true,
				   Gtk::MESSAGE_INFO,
				   Gtk::BUTTONS_CLOSE);
		
		msg.set_title (_("Port Registration Error"));
		msg.set_secondary_text (_("Click the Close button to try again."));
		msg.set_position (Gtk::WIN_POS_CENTER);
		pop_back_splash ();
		msg.present ();

		int response = msg.run ();

		msg.hide ();

		switch (response) {
		case RESPONSE_CANCEL:
			exit (1);
		default:
			break;
		}
		goto out;
	}

	/* this exception is also special */

	catch (Session::SRMismatchRejected& err) {
		goto out; /* just go back and reload something else, etc. */
	}

	catch (...) {

		MessageDialog msg (string_compose(_("Session \"%1 (snapshot %2)\" did not load successfully"), path, snap_name),
				   true,
				   Gtk::MESSAGE_INFO,
				   Gtk::BUTTONS_CLOSE);
		
		msg.set_title (_("Loading Error"));
		msg.set_secondary_text (_("Click the Close button to try again."));
		msg.set_position (Gtk::WIN_POS_CENTER);
		pop_back_splash ();
		msg.present ();

		int response = msg.run ();

		msg.hide ();

		switch (response) {
		case RESPONSE_CANCEL:
			exit (1);
		default:
			break;
		}
		goto out;
	}

	/* Now the session been created, add the transport controls */
	new_session->add_controllable(&roll_controllable);
	new_session->add_controllable(&stop_controllable);
	new_session->add_controllable(&goto_start_controllable);
	new_session->add_controllable(&goto_end_controllable);
	new_session->add_controllable(&auto_loop_controllable);
	new_session->add_controllable(&play_selection_controllable);
	new_session->add_controllable(&rec_controllable);

	/* Once the transport controlls have been added, the ControlProtocolManager
	   is okay to instantiate the various protocols. */
	BootMessage (_("Reset Control Protocols"));
	ControlProtocolManager::instance().set_session (*new_session);

	connect_to_session (new_session);

	Config->set_current_owner (ConfigVariableBase::Interface);

	session_loaded = true;
	
	goto_editor_window ();

	if (session) {
		session->set_clean ();
	}

	flush_pending ();
	retval = 0;

  out:
	return retval;
}

int
ARDOUR_UI::build_session (const std::string& path, const std::string& snap_name, 
			  uint32_t control_channels,
			  uint32_t master_channels, 
			  AutoConnectOption input_connect,
			  AutoConnectOption output_connect,
			  uint32_t nphysin,
			  uint32_t nphysout,
			  nframes_t initial_length)
{
	Session *new_session;
	int x;

	if (!check_audioengine()) {
		return -1;
	}

	session_loaded = false;

	x = unload_session ();

	if (x < 0) {
		return -1;
	} else if (x > 0) {
		return 0;
	}
	
	_session_is_new = true;

	try {
		new_session = new Session (*engine, path, snap_name, input_connect, output_connect,
					   control_channels, master_channels, nphysin, nphysout, initial_length);
	}

	catch (...) {

		MessageDialog msg (string_compose(_("Could not create session in \"%1\""), path));
		pop_back_splash ();
		msg.run ();
		return -1;
	}

	connect_to_session (new_session);

	session_loaded = true;

	new_session->save_state(new_session->name());

	return 0;
}

void
ARDOUR_UI::show ()
{
	if (editor) {
		editor->show_window ();
		
		if (!shown_flag) {
			editor->present ();
		}

		shown_flag = true;
	}
}

void
ARDOUR_UI::show_about ()
{
	if (about == 0) {
		about = new About;
		about->signal_response().connect(mem_fun (*this, &ARDOUR_UI::about_signal_response) );
	}

	about->set_transient_for(*editor);

	about->show_all ();
}

void
ARDOUR_UI::launch_chat ()
{
#ifdef __APPLE__
	NagScreen::open_uri("http://webchat.freenode.net/?channels=ardour-osx");
#else
	NagScreen::open_uri("http://webchat.freenode.net/?channels=ardour");
#endif
}

void
ARDOUR_UI::launch_manual ()
{
	NagScreen::open_uri("http://ardour.org/flossmanual");
}

void
ARDOUR_UI::launch_reference ()
{
	NagScreen::open_uri("http://ardour.org/refmanual");
}

void
ARDOUR_UI::hide_about ()
{
	if (about) {
	        about->get_window()->set_cursor ();
		about->hide ();
	}
}

void
ARDOUR_UI::about_signal_response(int response)
{
	hide_about();
}

void
ARDOUR_UI::show_splash ()
{
	if (splash == 0) {
		try {
			splash = new Splash;
		} catch (...) {
			return;
		}
	}

	splash->show ();
	splash->present ();
	splash->queue_draw ();
	splash->get_window()->process_updates (true);
	flush_pending ();
}

void
ARDOUR_UI::hide_splash ()
{
	if (splash) {
		splash->hide();
	}
}

void
ARDOUR_UI::display_cleanup_results (Session::cleanup_report& rep, const gchar* list_title, 
				    const string& plural_msg, const string& singular_msg)
{
	size_t removed;

	removed = rep.paths.size();

	if (removed == 0) {
		MessageDialog msgd (*editor,
				    _("No audio files were ready for cleanup"), 
				    true,
				    Gtk::MESSAGE_INFO,
				    (Gtk::ButtonsType)(Gtk::BUTTONS_OK)  );
		msgd.set_secondary_text (_("If this seems suprising, \n\
check for any existing snapshots.\n\
These may still include regions that\n\
require some unused files to continue to exist."));
	
		msgd.run ();
		return;
	} 

	ArdourDialog results (_("ardour: cleanup"), true, false);
	
	struct CleanupResultsModelColumns : public Gtk::TreeModel::ColumnRecord {
	    CleanupResultsModelColumns() { 
		    add (visible_name);
		    add (fullpath);
	    }
	    Gtk::TreeModelColumn<std::string> visible_name;
	    Gtk::TreeModelColumn<std::string> fullpath;
	};

	
	CleanupResultsModelColumns results_columns;
	Glib::RefPtr<Gtk::ListStore> results_model;
	Gtk::TreeView results_display;
	
	results_model = ListStore::create (results_columns);
	results_display.set_model (results_model);
	results_display.append_column (list_title, results_columns.visible_name);

	results_display.set_name ("CleanupResultsList");
	results_display.set_headers_visible (true);
	results_display.set_headers_clickable (false);
	results_display.set_reorderable (false);

	Gtk::ScrolledWindow list_scroller;
	Gtk::Label txt;
	Gtk::VBox dvbox;
	Gtk::HBox dhbox;  // the hbox for the image and text
	Gtk::HBox ddhbox; // the hbox we eventually pack into the dialog's vbox
	Gtk::Image* dimage = manage (new Gtk::Image(Stock::DIALOG_INFO,  Gtk::ICON_SIZE_DIALOG));

	dimage->set_alignment(ALIGN_LEFT, ALIGN_TOP);


	/* subst:
	   %1 - number of files removed
	   %2 - location of "dead_sounds"
	   %3 - size of files affected
	   %4 - prefix for "bytes" to produce sensible results (e.g. mega, kilo, giga)
	*/

	const char* bprefix;
	float space_adjusted;

	if (rep.space < 1000000.0f) {
		bprefix = X_("kilo");
		space_adjusted = truncf((float)rep.space / 1000.0f);
	} else if (rep.space < (1000000.0f * 1000)) {
		bprefix = X_("mega");
		space_adjusted = truncf((float)rep.space / (1000000.0f));
	} else {
		bprefix = X_("giga");
		space_adjusted = truncf((float)rep.space / (1000000.0f * 1000));
	}

	if (removed > 1) {
		txt.set_text (string_compose (plural_msg, removed, session->path() + "dead_sounds", space_adjusted, bprefix));
	} else {
		txt.set_text (string_compose (singular_msg, removed, session->path() + "dead_sounds", space_adjusted, bprefix));
	}

	dhbox.pack_start (*dimage, true, false, 5);
	dhbox.pack_start (txt, true, false, 5);

	for (vector<string>::iterator i = rep.paths.begin(); i != rep.paths.end(); ++i) {
		TreeModel::Row row = *(results_model->append());
		row[results_columns.visible_name] = *i;
		row[results_columns.fullpath] = *i;
	}
	
	list_scroller.add (results_display);
	list_scroller.set_size_request (-1, 150);
	list_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	dvbox.pack_start (dhbox, true, false, 5);
	dvbox.pack_start (list_scroller, true, false, 5);
	ddhbox.pack_start (dvbox, true, false, 5);

	results.get_vbox()->pack_start (ddhbox, true, false, 5);
	results.add_button (Stock::CLOSE, RESPONSE_CLOSE);
	results.set_default_response (RESPONSE_CLOSE);
	results.set_position (Gtk::WIN_POS_MOUSE);
	results.show_all_children ();
	results.set_resizable (false);

	results.run ();

}

void
ARDOUR_UI::cleanup ()
{
	if (session == 0) {
		/* shouldn't happen: menu item is insensitive */
		return;
	}


	MessageDialog  checker (_("Are you sure you want to cleanup?"),
				true,
				Gtk::MESSAGE_QUESTION,
				(Gtk::ButtonsType)(Gtk::BUTTONS_NONE));

	checker.set_secondary_text(_("Cleanup is a destructive operation.\n\
ALL undo/redo information will be lost if you cleanup.\n\
After cleanup, unused audio files will be moved to a \
\"dead sounds\" location."));
	
	checker.add_button (Stock::CANCEL, RESPONSE_CANCEL);
	checker.add_button (_("Clean Up"), RESPONSE_ACCEPT);
	checker.set_default_response (RESPONSE_CANCEL);

	checker.set_name (_("CleanupDialog"));
	checker.set_wmclass (X_("ardour_cleanup"), PROGRAM_NAME);
	checker.set_position (Gtk::WIN_POS_MOUSE);

	switch (checker.run()) {
	case RESPONSE_ACCEPT:
		break;
	default:
		return;
	}

	Session::cleanup_report rep;

	editor->prepare_for_cleanup ();

	/* do not allow flush until a session is reloaded */

	Glib::RefPtr<Action> act = ActionManager::get_action (X_("Main"), X_("FlushWastebasket"));
	if (act) {
		act->set_sensitive (false);
	}

	if (session->cleanup_sources (rep)) {
		editor->finish_cleanup ();
		return;
	}
	
	editor->finish_cleanup ();

	checker.hide();
	display_cleanup_results (rep, 
				 _("cleaned files"),
				 _("\
The following %1 files were not in use and \n\
have been moved to:\n\
%2. \n\n\
Flushing the wastebasket will \n\
release an additional\n\
%3 %4bytes of disk space.\n"),
				 _("\
The following file was not in use and \n	\
has been moved to:\n				\
%2. \n\n\
Flushing the wastebasket will \n\
release an additional\n\
%3 %4bytes of disk space.\n"
					 ));

}

void
ARDOUR_UI::flush_trash ()
{
	if (session == 0) {
		/* shouldn't happen: menu item is insensitive */
		return;
	}

	Session::cleanup_report rep;

	if (session->cleanup_trash_sources (rep)) {
		return;
	}

	display_cleanup_results (rep, 
				 _("deleted file"),
				 _("The following %1 files were deleted from\n\
%2,\n\
releasing %3 %4bytes of disk space"),
				 _("The following file was deleted from\n\
%2,\n\
releasing %3 %4bytes of disk space"));
}

void
ARDOUR_UI::add_route (Gtk::Window* float_window)
{
	int count;

	if (!session) {
		return;
	}

	if (add_route_dialog == 0) {
		add_route_dialog = new AddRouteDialog;
		if (float_window) {
			add_route_dialog->set_transient_for (*float_window);
		}
	}

	if (add_route_dialog->is_visible()) {
		/* we're already doing this */
		return;
	}

	ResponseType r = (ResponseType) add_route_dialog->run ();
	
	add_route_dialog->hide();

	switch (r) {
	case RESPONSE_ACCEPT:
		break;
	default:
		return;
		break;
	}

	if ((count = add_route_dialog->count()) <= 0) {
		return;
	}

	string template_path = add_route_dialog->track_template();

	if (!template_path.empty()) {
	  session->new_route_from_template (count, template_path);
	  return;
	}
	
	uint32_t input_chan = add_route_dialog->channels ();
	uint32_t output_chan;
	string name_template = add_route_dialog->name_template ();
	bool track = add_route_dialog->track ();

	AutoConnectOption oac = Config->get_output_auto_connect();

	if (oac & AutoConnectMaster) {
		output_chan = (session->master_out() ? session->master_out()->n_inputs() : input_chan);
	} else {
		output_chan = input_chan;
	}

	/* XXX do something with name template */

	if (track) {
		session_add_audio_track (input_chan, output_chan, add_route_dialog->mode(), count);
	} else {
		session_add_audio_bus (input_chan, output_chan, count);
	}
}

XMLNode*
ARDOUR_UI::mixer_settings () const
{
	XMLNode* node = 0;

	if (session) {
		node = session->instant_xml(X_("Mixer"), session->path());
	} else {
		node = Config->instant_xml(X_("Mixer"), get_user_ardour_path());
	}

	if (!node) {
		node = new XMLNode (X_("Mixer"));
	}

	return node;
}

XMLNode*
ARDOUR_UI::editor_settings () const
{
	XMLNode* node = 0;

	if (session) {
		node = session->instant_xml(X_("Editor"), session->path());
	} else {
		node = Config->instant_xml(X_("Editor"), get_user_ardour_path());
	}
	
	if (!node) {
		if (getenv("ARDOUR_INSTANT_XML_PATH")) {
			node = Config->instant_xml(X_("Editor"), getenv("ARDOUR_INSTANT_XML_PATH"));
		}
	}

	if (!node) {
		node = new XMLNode (X_("Editor"));
	}
	return node;
}

XMLNode*
ARDOUR_UI::keyboard_settings () const
{
	XMLNode* node = 0;

	node = Config->extra_xml(X_("Keyboard"));
	
	if (!node) {
		node = new XMLNode (X_("Keyboard"));
	}
	return node;
}

void
ARDOUR_UI::create_xrun_marker(nframes_t where)
{
	editor->mouse_add_new_marker (where, false, true);
}

void
ARDOUR_UI::halt_on_xrun_message ()
{
	MessageDialog msg (*editor,
			   _("Recording was stopped because your system could not keep up."));
	msg.run ();
}

void
ARDOUR_UI::xrun_handler(nframes_t where)
{
	if (!session) {
		return;
	}

	ENSURE_GUI_THREAD (bind(mem_fun(*this, &ARDOUR_UI::xrun_handler), where));

	if (session && Config->get_create_xrun_marker() && session->actively_recording()) {
		create_xrun_marker(where);
	}

	if (session && Config->get_stop_recording_on_xrun() && session->actively_recording()) {
		halt_on_xrun_message ();
	}
}

bool
ARDOUR_UI::preset_file_exists_handler ()
{
	/* if driven from another thread, say "do not overwrite" and show the user nothing.
	 */

	if (!Gtkmm2ext::UI::instance()->caller_is_ui_thread()) {	\
		return false;
	}
	
 	HBox* hbox = new HBox();
	Image* image = new Image (Stock::DIALOG_QUESTION, ICON_SIZE_DIALOG);
	Gtk::Dialog dialog (_("Preset Exists"), true, false);
	Label  message (_("\
A preset with this name already exists for this plugin.\n\
\n\
What you would like to do?\n"));
	image->set_alignment(ALIGN_CENTER, ALIGN_TOP);
	hbox->pack_start (*image, PACK_EXPAND_WIDGET, 12);
	hbox->pack_end (message, PACK_EXPAND_PADDING, 12);
	dialog.get_vbox()->pack_start(*hbox, PACK_EXPAND_PADDING, 6);
	dialog.add_button (_("Overwrite the existing preset"), RESPONSE_ACCEPT);
	dialog.add_button (_("Leave the existing preset alone"), RESPONSE_REJECT);
	dialog.set_default_response (RESPONSE_ACCEPT);
	dialog.set_position (WIN_POS_MOUSE);
	dialog.set_type_hint (Gdk::WINDOW_TYPE_HINT_UTILITY); // need to make it float above the preset name dialog

	message.show();
	image->show();
	hbox->show();

	switch (dialog.run ()) {
	case RESPONSE_ACCEPT:
		return true;
	default:
		return false;
	}
}

void
ARDOUR_UI::disk_overrun_handler ()
{

	ENSURE_GUI_THREAD (mem_fun(*this, &ARDOUR_UI::disk_overrun_handler));

	if (!have_disk_speed_dialog_displayed) {
		have_disk_speed_dialog_displayed = true;
		MessageDialog* msg = new MessageDialog (*editor, _("\
The disk system on your computer\n\
was not able to keep up with Ardour.\n\
\n\
Specifically, it failed to write data to disk\n\
quickly enough to keep up with recording.\n"));
		msg->signal_response().connect (bind (mem_fun (*this, &ARDOUR_UI::disk_speed_dialog_gone), msg));
		msg->show_all ();
	}
}

void
ARDOUR_UI::disk_underrun_handler ()
{

	ENSURE_GUI_THREAD (mem_fun(*this, &ARDOUR_UI::disk_underrun_handler));

	if (!have_disk_speed_dialog_displayed) {
		have_disk_speed_dialog_displayed = true;
		MessageDialog* msg = new MessageDialog (*editor,
				   _("The disk system on your computer\n\
was not able to keep up with Ardour.\n\
\n\
Specifically, it failed to read data from disk\n\
quickly enough to keep up with playback.\n"));
		msg->signal_response().connect (bind (mem_fun (*this, &ARDOUR_UI::disk_speed_dialog_gone), msg));
		msg->show_all ();
	} 
}

void
ARDOUR_UI::disk_speed_dialog_gone (int ignored_response, MessageDialog* msg)
{
	have_disk_speed_dialog_displayed = false;
	delete msg;
}

void
ARDOUR_UI::session_dialog (std::string msg)
{
	ENSURE_GUI_THREAD (bind (mem_fun(*this, &ARDOUR_UI::session_dialog), msg));
	
	MessageDialog* d;

	if (editor) {
		d = new MessageDialog (*editor, msg, false, MESSAGE_INFO, BUTTONS_OK, true);
	} else {
		d = new MessageDialog (msg, false, MESSAGE_INFO, BUTTONS_OK, true);
	}

	d->show_all ();
	d->run ();
	delete d;
}	

int
ARDOUR_UI::pending_state_dialog ()
{
 	HBox* hbox = new HBox();
	Image* image = new Image (Stock::DIALOG_QUESTION, ICON_SIZE_DIALOG);
	ArdourDialog dialog (_("Crash Recovery"), true);
	Label  message (string_compose (_("\
This session appears to have been in\n\
middle of recording when ardour or\n\
the computer was shutdown.\n\
\n\
%1 can recover any captured audio for\n\
you, or it can ignore it. Please decide\n\
what you would like to do.\n"), PROGRAM_NAME));
	image->set_alignment(ALIGN_CENTER, ALIGN_TOP);
	hbox->pack_start (*image, PACK_EXPAND_WIDGET, 12);
	hbox->pack_end (message, PACK_EXPAND_PADDING, 12);
	dialog.get_vbox()->pack_start(*hbox, PACK_EXPAND_PADDING, 6);
	dialog.add_button (_("Ignore crash data"), RESPONSE_REJECT);
	dialog.add_button (_("Recover from crash"), RESPONSE_ACCEPT);
	dialog.set_default_response (RESPONSE_ACCEPT);
	dialog.set_position (WIN_POS_CENTER);
	message.show();
	image->show();
	hbox->show();

	pop_back_splash ();

	switch (dialog.run ()) {
	case RESPONSE_ACCEPT:
		return 1;
	default:
		return 0;
	}
}

int
ARDOUR_UI::sr_mismatch_dialog (nframes_t desired, nframes_t actual)
{
 	HBox* hbox = new HBox();
	Image* image = new Image (Stock::DIALOG_QUESTION, ICON_SIZE_DIALOG);
	ArdourDialog dialog (_("Sample Rate Mismatch"), true);
	Label  message (string_compose (_("\
This session was created with a sample rate of %1 Hz\n\
\n\
The audioengine is currently running at %2 Hz\n"), desired, actual));

	image->set_alignment(ALIGN_CENTER, ALIGN_TOP);
	hbox->pack_start (*image, PACK_EXPAND_WIDGET, 12);
	hbox->pack_end (message, PACK_EXPAND_PADDING, 12);
	dialog.get_vbox()->pack_start(*hbox, PACK_EXPAND_PADDING, 6);
	dialog.add_button (_("Do not load session"), RESPONSE_REJECT);
	dialog.add_button (_("Load session anyway"), RESPONSE_ACCEPT);
	dialog.set_default_response (RESPONSE_ACCEPT);
	dialog.set_position (WIN_POS_CENTER);
	message.show();
	image->show();
	hbox->show();

	switch (dialog.run ()) {
	case RESPONSE_ACCEPT:
		return 0;
	default:
		return 1;
	}
}

	
void
ARDOUR_UI::disconnect_from_jack ()
{
	if (engine) {
		if( engine->disconnect_from_jack ()) {
			MessageDialog msg (*editor, _("Could not disconnect from JACK"));
			msg.run ();
		}

		update_sample_rate (0);
	}
}

void
ARDOUR_UI::reconnect_to_jack ()
{
	if (engine) {
		if (engine->reconnect_to_jack ()) {
			MessageDialog msg (*editor,  _("Could not reconnect to JACK"));
			msg.run ();
		}

		update_sample_rate (0);
	}
}

void
ARDOUR_UI::use_config ()
{
	Glib::RefPtr<Action> act;

	switch (Config->get_native_file_data_format ()) {
	case FormatFloat:
		act = ActionManager::get_action (X_("options"), X_("FileDataFormatFloat"));
		break;
	case FormatInt24:
		act = ActionManager::get_action (X_("options"), X_("FileDataFormat24bit"));
		break;
	case FormatInt16:
		act = ActionManager::get_action (X_("options"), X_("FileDataFormat16bit"));
		break;
	}

	if (act) {
		Glib::RefPtr<RadioAction> ract = Glib::RefPtr<RadioAction>::cast_dynamic(act);
		ract->set_active ();
	}	

	switch (Config->get_native_file_header_format ()) {
	case BWF:
		act = ActionManager::get_action (X_("options"), X_("FileHeaderFormatBWF"));
		break;
	case WAVE:
		act = ActionManager::get_action (X_("options"), X_("FileHeaderFormatWAVE"));
		break;
	case WAVE64:
		act = ActionManager::get_action (X_("options"), X_("FileHeaderFormatWAVE64"));
		break;
	case iXML:
		act = ActionManager::get_action (X_("options"), X_("FileHeaderFormatiXML"));
		break;
	case RF64:
		act = ActionManager::get_action (X_("options"), X_("FileHeaderFormatRF64"));
		break;
	case CAF:
		act = ActionManager::get_action (X_("options"), X_("FileHeaderFormatCAF"));
		break;
	case AIFF:
		act = ActionManager::get_action (X_("options"), X_("FileHeaderFormatAIFF"));
		break;
	}

	if (act) {
		Glib::RefPtr<RadioAction> ract = Glib::RefPtr<RadioAction>::cast_dynamic(act);
		ract->set_active ();
	}	

	XMLNode* node = Config->extra_xml (X_("TransportControllables"));
	if (node) {
		set_transport_controllable_state (*node);
	}
}

void
ARDOUR_UI::update_transport_clocks (nframes_t pos)
{
	if (Config->get_primary_clock_delta_edit_cursor()) {
		primary_clock.set (pos, false, editor->get_preferred_edit_position(), 1);
	} else {
		primary_clock.set (pos, 0, true);
	}

	if (Config->get_secondary_clock_delta_edit_cursor()) {
		secondary_clock.set (pos, false, editor->get_preferred_edit_position(), 2);
	} else {
		secondary_clock.set (pos);
	}

	if (big_clock_window) {
		big_clock.set (pos);
	}
}

void
ARDOUR_UI::record_state_changed ()
{
	ENSURE_GUI_THREAD (mem_fun (*this, &ARDOUR_UI::record_state_changed));

	if (!session || !big_clock_window) {
		/* why bother - the clock isn't visible */
		return;
	}

	switch (session->record_status()) {
	case Session::Recording:
		big_clock.set_widget_name ("BigClockRecording");
		break;
	default:
		big_clock.set_widget_name ("BigClockNonRecording");
		break;
	}
}

bool
ARDOUR_UI::first_idle ()
{
	if (session) {
		session->allow_auto_play (true);
	}

	if (editor) {
		editor->first_idle();
	}

	Keyboard::set_can_save_keybindings (true);
	return false;
}

void
ARDOUR_UI::store_clock_modes ()
{
	XMLNode* node = new XMLNode(X_("ClockModes"));

	for (vector<AudioClock*>::iterator x = AudioClock::clocks.begin(); x != AudioClock::clocks.end(); ++x) {
		node->add_property ((*x)->name().c_str(), enum_2_string ((*x)->mode()));
	}

	session->add_extra_xml (*node);
	session->set_dirty ();
}


		
ARDOUR_UI::TransportControllable::TransportControllable (std::string name, ARDOUR_UI& u, ToggleType tp)
	: Controllable (name), ui (u), type(tp)
{
	
}

void
ARDOUR_UI::TransportControllable::set_value (float val)
{
	if (type == ShuttleControl) {
		double fract;

		if (val == 0.5f) {
			fract = 0.0;
		} else {
			if (val < 0.5f) {
				fract = -((0.5f - val)/0.5f);
			} else {
				fract = ((val - 0.5f)/0.5f);
			}
		}
		
		ui.set_shuttle_fract (fract);
		return;
	}

	if (val < 0.5f) {
		/* do nothing: these are radio-style actions */
		return;
	}

	const char *action = 0;

	switch (type) {
	case Roll:
		action = X_("Roll");
		break;
	case Stop:
		action = X_("Stop");
		break;
	case GotoStart:
		action = X_("GotoStart");
		break;
	case GotoEnd:
		action = X_("GotoEnd");
		break;
	case AutoLoop:
		action = X_("Loop");
		break;
	case PlaySelection:
		action = X_("PlaySelection");
		break;
	case RecordEnable:
		action = X_("Record");
		break;
	default:
		break;
	}

	if (action == 0) {
		return;
	}

	Glib::RefPtr<Action> act = ActionManager::get_action ("Transport", action);

	if (act) {
		act->activate ();
	}
}

float
ARDOUR_UI::TransportControllable::get_value (void) const
{
	float val = 0.0f;
	
	switch (type) {
	case Roll:
		break;
	case Stop:
		break;
	case GotoStart:
		break;
	case GotoEnd:
		break;
	case AutoLoop:
		break;
	case PlaySelection:
		break;
	case RecordEnable:
		break;
	case ShuttleControl:
		break;
	default:
		break;
	}

	return val;
}

void
ARDOUR_UI::TransportControllable::set_id (const string& str)
{
	_id = str;
}

void
ARDOUR_UI::setup_profile ()
{
	if (gdk_screen_width() < 1200) {
		Profile->set_small_screen ();
	}

	if (getenv ("ARDOUR_SAE")) {
		Profile->set_sae ();
		Profile->set_single_package ();
	}
}


void
ARDOUR_UI::toggle_translations ()
{
	using namespace Glib;

	RefPtr<Action> act = ActionManager::get_action (X_("Main"), X_("EnableTranslation"));
	if (act) {
		RefPtr<ToggleAction> ract = RefPtr<ToggleAction>::cast_dynamic (act);
		if (ract) {

			string i18n_killer = ARDOUR::translation_kill_path();
			
			bool already_enabled = !ARDOUR::translations_are_disabled ();

			if (ract->get_active ()) {
				/* we don't care about errors */
				int fd = ::open (i18n_killer.c_str(), O_RDONLY|O_CREAT, 0644);
				close (fd);
			} else {
				/* we don't care about errors */
				unlink (i18n_killer.c_str());
			}

			if (already_enabled != ract->get_active()) {
				MessageDialog win (already_enabled ? _("Translations disabled") : _("Translations enabled"),
						   false,
						   Gtk::MESSAGE_WARNING,
						   Gtk::BUTTONS_OK);
				win.set_secondary_text (string_compose (_("You must restart %1 for this to take effect."), PROGRAM_NAME));
				win.set_position (Gtk::WIN_POS_CENTER);
				win.present ();
				win.run ();
			}
		}
	}
}
