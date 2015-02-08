/*
    Copyright (C) 20002-2004 Paul Davis

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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

/* This file contains any ARDOUR_UI methods that require knowledge of
   the editor, and exists so that no compilation dependency exists
   between the main ARDOUR_UI modules and the PublicEditor class. This
   is to cut down on the nasty compile times for both these classes.
*/

#include <cmath>

#include <glibmm/miscutils.h>
#include <gtk/gtk.h>

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/window_title.h"
#include "gtkmm2ext/tearoff.h"
#include "gtkmm2ext/cairo_packer.h"

#include "pbd/file_utils.h"
#include "pbd/fpu.h"
#include "pbd/convert.h"
#include "pbd/unwind.h"

#include "ardour_ui.h"
#include "public_editor.h"
#include "audio_clock.h"
#include "keyboard.h"
#include "monitor_section.h"
#include "editor.h"
#include "actions.h"
#include "mixer_ui.h"
#include "window_manager.h"
#include "global_port_matrix.h"
#include "location_ui.h"
#include "main_clock.h"
#include "time_info_box.h"
#include "utils.h"
#include "gui_thread.h"

#include <gtkmm2ext/application.h>

#include "ardour/session.h"
#include "ardour/profile.h"
#include "ardour/engine_state_controller.h"

#include "control_protocol/control_protocol.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace Glib;

 //recent session menuitem id
static const std::string recent_session_menuitem_id="recent-session-";
int
ARDOUR_UI::create_editor ()
{
	try {
		editor = new Editor ();
		_dsp_load_adjustment = &editor->get_adjustment ("dsp_load_adjustment");
                _hd_load_adjustment = &editor->get_adjustment("hd_load_adjustment");
                
                _dsp_load_label = &editor->get_label("dsp_load_label");
                _hd_load_label = &editor->get_label("hd_load_label");
                _hd_remained_time_label = &editor->get_label("hd_remained_time");
                
                _bit_depth_button = &editor->get_waves_button("bit_depth_button");
                _frame_rate_button = &editor->get_waves_button("frame_rate_button");        
                _sample_rate_dropdown = &editor->get_waves_dropdown("sample_rate_dropdown");
                _display_format_dropdown = &editor->get_waves_dropdown("display_format_dropdown");
                _timecode_source_dropdown = &editor->get_waves_dropdown("timecode_selector_dropdown");
                _mtc_idle_icon = &editor->get_image("mtc_idle_icon");
                _mtc_sync_icon = &editor->get_image("mtc_sync_icon");
        
                _tracks_button = &editor->get_waves_button("tracks_button");
	}

	catch (failed_constructor& err) {
		return -1;
	}
    
        _bit_depth_button->signal_clicked.connect(sigc::mem_fun (*this, &ARDOUR_UI::on_bit_depth_button));
        _frame_rate_button->signal_clicked.connect(sigc::mem_fun (*this, &ARDOUR_UI::on_frame_rate_button));
        _tracks_button->signal_clicked.connect(sigc::mem_fun (*this, &ARDOUR_UI::on_tracks_button));
        
        _sample_rate_dropdown->selected_item_changed.connect (mem_fun(*this, &ARDOUR_UI::on_sample_rate_dropdown_item_clicked ));
        _display_format_dropdown->selected_item_changed.connect (mem_fun(*this, &ARDOUR_UI::on_display_format_dropdown_item_clicked ));
        _timecode_source_dropdown->selected_item_changed.connect (mem_fun(*this, &ARDOUR_UI::on_timecode_source_dropdown_item_clicked ));
        
	editor->Realized.connect (sigc::mem_fun (*this, &ARDOUR_UI::editor_realized));
	editor->signal_window_state_event().connect (sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::main_window_state_event_handler), true));
    
        return 0;
}

void
ARDOUR_UI::populate_display_format_dropdown ()
{
    static std::vector<string> display_formats;
    _display_format_dropdown->clear_items ();
    display_formats.clear ();
    
    display_formats.push_back("Timecode");
    display_formats.push_back("Time");
    display_formats.push_back("Samples");
    
    for(int i = 0; i < display_formats.size(); ++i)
    {
        _display_format_dropdown->add_menu_item (display_formats[i], &display_formats[i]);
    }
    
    if( !_session )
        return;
    
    AudioClock::Mode mode = primary_clock->mode();
 
    string format;
    if (AudioClock::Timecode == mode)
        format = display_formats[0];
    else if (AudioClock::MinSec == mode)
        format = display_formats[1];
    else format = display_formats[2];
        
    _display_format_dropdown->set_text( format );
}

void
ARDOUR_UI::populate_timecode_source_dropdown ()
{
    static std::vector<string> timecode_source;
    _timecode_source_dropdown->clear_items ();
    
    timecode_source.clear();
    timecode_source.push_back("Internal");
    timecode_source.push_back("MTC");
    
    // GZ: Is Not available in current Waves TracksLive version
    //timecode_source.push_back("LTC");
        
    for(int i = 0; i < timecode_source.size(); ++i)
    {
        _timecode_source_dropdown->add_menu_item (timecode_source[i], &timecode_source[i]);
    }
    
    if( !_session )
        return;
    
    bool use_external_timecode_source = _session->config.get_external_sync ();
    
    if (use_external_timecode_source)
    {
        if ( Config->get_sync_source() == MTC )
            _timecode_source_dropdown->set_text (timecode_source[1]);//"MTC"
        
        // GZ: LTC is not available in current version of Waves TracksLive
        /*
        else
            _timecode_source_dropdown->set_text (timecode_source[2]);//"LTC"
         */
        
    } else {
        _timecode_source_dropdown->set_text (timecode_source[0]);//Internal
    }
    
    update_timecode_source_dropdown_items();
}

void
ARDOUR_UI::update_timecode_source_dropdown_items()
{
    if (!_session) {
        return;
    }
    
    Gtk::MenuItem* mtc_item = _timecode_source_dropdown->get_item ("MTC");
    
    if (mtc_item) {
        if (_session->mtc_input_port()->connected() ) {
            mtc_item->set_sensitive(true);
        } else {
            mtc_item->set_sensitive(false);
        }
    }
}

void
ARDOUR_UI::set_mtc_indicator_active (bool set_active)
{
    _mtc_sync_icon->set_visible (set_active);
    _mtc_idle_icon->set_visible (!set_active);
}

void
ARDOUR_UI::hide_mtc_indicator ()
{
    _mtc_sync_icon->set_visible (false);
    _mtc_idle_icon->set_visible (false);
}

void
ARDOUR_UI::on_time_info_box_mode_changed ()
{
    sync_displays_format ( time_info_box->mode() );
}

void
ARDOUR_UI::on_primary_clock_mode_changed ()
{
    sync_displays_format ( primary_clock->mode() );
}

void
ARDOUR_UI::sync_displays_format (AudioClock::Mode mode)
{
    if (_ignore_changes) {
		return;
	}
    
    PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
    
    if( time_info_box )
        time_info_box->set_mode (mode);
    if ( big_clock )
        primary_clock->set_mode (mode);
    
    switch (mode) {
        case AudioClock::Timecode:
            _display_format_dropdown->set_text ("Timecode");
            break;
        case AudioClock::MinSec:
            _display_format_dropdown->set_text ("Time");
            break;
        case AudioClock::Frames:
            _display_format_dropdown->set_text ("Samples");
            break;
        default:
            break;
    }
}

void
ARDOUR_UI::on_display_format_dropdown_item_clicked (WavesDropdown* dropdown, int el_number)
{
    void* data = dropdown->get_item_data_pv(el_number);
    assert(data);
    
    string format = *(string*)data;
    
    AudioClock::Mode mode;
    
    if( format == "Timecode" )
        mode = AudioClock::Timecode;
    else if ( format == "Time" )
        mode = AudioClock::MinSec;
    else if ( format == "Samples" )
        mode = AudioClock::Frames;
    
    sync_displays_format (mode);
}

void
ARDOUR_UI::on_timecode_source_dropdown_item_clicked (WavesDropdown* dropdown, int el_number)
{
    void* data = dropdown->get_item_data_pv(el_number);
    assert(data);
    
    string timecode_source = *(string*)data;
    
    if ( timecode_source == "Internal" )
    {
        _session->config.set_external_sync (false);
    } else if ( timecode_source == "MTC" )
    {
        Config->set_sync_source (MTC);
        _session->config.set_external_sync (true);
    }
    // GZ: LTC is not available in current version of Waves TracksLive
    /* else if ( timecode_source == "LTC" )
    {
        Config->set_sync_source (LTC);
        _session->config.set_external_sync (true);        
    } */
}

void
ARDOUR_UI::on_tracks_button (WavesButton*)
{
    about->show ();
}

void
ARDOUR_UI::on_bit_depth_button (WavesButton*)
{
    tracks_control_panel->show_and_open_tab (TracksControlPanel::SessionSettingsTab);
}

void
ARDOUR_UI::on_frame_rate_button (WavesButton*)
{
    tracks_control_panel->show_and_open_tab (TracksControlPanel::SessionSettingsTab);
}

void
ARDOUR_UI::populate_sample_rate_dropdown ()
{
    static std::vector<float> sample_rates;
    
    _sample_rate_dropdown->clear_items();
    
    sample_rates.clear ();
    EngineStateController::instance()->available_sample_rates_for_current_device( sample_rates );
	
	std::vector<std::string> s;
	for (std::vector<float>::const_iterator x = sample_rates.begin(); x != sample_rates.end(); ++x) {
		s.push_back (ARDOUR_UI_UTILS::rate_as_string (*x));
	}
    
	{
		// set _ignore_changes flag to ignore changes in combo-box callbacks
		PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
        
        for(int i = 0; i < sample_rates.size(); ++i)
        {
            _sample_rate_dropdown->add_menu_item (s[i], &sample_rates[i]);
        }
        
        update_sample_rate_dropdown ();
	}
}

framecnt_t
ARDOUR_UI::get_sample_rate () const
{
    const string& sample_rate = _sample_rate_dropdown->get_text ();
    
    if ( "44.1 kHz" == sample_rate )
    {
        return 44100;
    } else if ( "48 kHz" == sample_rate )
    {
        return 48000;
    } else if ( "88.2 kHz" == sample_rate )
    {
        return 88200;
    } else if ( "96 kHz" == sample_rate )
    {
        return 96000;
    } else if ( "176.4 kHz" == sample_rate )
    {
        return 176400;
    } else if ( "192 kHz" == sample_rate )
    {
        return 192000;
    }
    
    float r = atof (sample_rate);
    
    /* the string may have been translated with an abbreviation for
     * thousands, so use a crude heuristic to fix this.
     */
	if (r < 1000.0) {
		r *= 1000.0;
	}
	return r;
}

void
ARDOUR_UI::sample_rate_changed()
{
	if (_ignore_changes) {
		return;
	}
    
    framecnt_t new_sample_rate = get_sample_rate ();
    if ( EngineStateController::instance()->set_new_sample_rate_in_controller(new_sample_rate) )
    {
        return;
    } 
    // ELSE restore previous buffer size value in combo box
    update_sample_rate_dropdown ();
}

void
ARDOUR_UI::on_sample_rate_dropdown_item_clicked (WavesDropdown* dropdown, int el_number)
{
    void* data = dropdown->get_item_data_pv(el_number);
    assert(data);
    
    framecnt_t sample_rate = *(float*)data;
    
    if (EngineStateController::instance()->set_new_sample_rate_in_controller( sample_rate ) )
    {
        EngineStateController::instance()->push_current_state_to_backend (false);
        return;
    }
    // ELSE restore previous buffer size value in combo box
    update_sample_rate_dropdown ();
}

void
ARDOUR_UI::update_bit_depth_button ()
{
    if( _session && _bit_depth_button )
    {
        string file_data_format;
        switch (_session->config.get_native_file_data_format ()) {
        case FormatFloat:
            file_data_format = "32 bit";
            break;
        case FormatInt24:
            file_data_format = "24 bit";
            break;
        case FormatInt16:
            file_data_format = "16 bit";
            break;
        }
        _bit_depth_button->set_text (file_data_format);
    }
}

void
ARDOUR_UI::update_sample_rate_dropdown ()
{
    if( !_sample_rate_dropdown )
        return;
    
    {
		// set _ignore_changes flag to ignore changes in combo-box callbacks
		PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
        std::string active_sr = ARDOUR_UI_UTILS::rate_as_string(EngineStateController::instance()->get_current_sample_rate());
        _sample_rate_dropdown->set_text (active_sr);
    }
}

void
ARDOUR_UI::update_frame_rate_button ()
{
    if( !_frame_rate_button )
        return;
         
    string timecode_format_string;
    switch( _timecode_format ) {
        case Timecode::timecode_24:
            timecode_format_string = "24 fps";
            break;
        case Timecode::timecode_25:
            timecode_format_string = "25 fps";
            break;
        case Timecode::timecode_30:
            timecode_format_string = "30 fps";
            break;
        case Timecode::timecode_23976:
            timecode_format_string = "23.976 fps";
            break;
        case Timecode::timecode_2997:
            timecode_format_string = "29.97 fps";
            break;
        default:
            break;
    }
    
    _frame_rate_button->set_text (timecode_format_string);
}

void
ARDOUR_UI::update_recent_session_menuitems ()
{
    std::vector<std::string> recent_session_names;
    recent_session_full_paths.clear ();
    get_recent_session_names_and_paths (recent_session_names,recent_session_full_paths);
    
    // gtk changes label of menu item automatically
    // '_' to '' on mac
    // '_' to underscore on win
    // so that to save '_' in the session name
    // we should transform '_' to '__'
    // it's not obvious behaviour of gtk
    for (int i=0; i < recent_session_names.size(); ++i){
        std::string& session_name = recent_session_names[i];
        for (int j=0; j < session_name.size(); ++j)
            if ( session_name[j] == '_' ){
                session_name.insert (j,"_");  // we should doubled
                ++j;                         // and skip next symbol because it's '_'
            }
    }
    
	for (int i=0; i < MAX_RECENT_SESSION_COUNT; ++i){
		Glib::RefPtr<Action> act;

		std::string label=string_compose( ("%1%2"), recent_session_menuitem_id.c_str(),i ) ;
		act = ActionManager::get_action_from_name (label.c_str());

		// set label for existing recent session menuitem and hiding
		// the items there are no recent sessions for them.
		act->set_label (i < recent_session_names.size () ? recent_session_names[i].c_str () : "");
		act->set_sensitive (i < recent_session_names.size ());
	}
}

void
ARDOUR_UI::set_topbar_buttons_sensitive (bool yn)
{
    _bit_depth_button->set_sensitive (yn);
    _frame_rate_button->set_sensitive (yn);
    
    _sample_rate_dropdown->set_sensitive (yn);
    _display_format_dropdown->set_sensitive (yn);
    _timecode_source_dropdown->set_sensitive (yn);
}

void
ARDOUR_UI::install_actions ()
{
	Glib::RefPtr<ActionGroup> main_actions = ActionGroup::create (X_("Main"));
	Glib::RefPtr<ActionGroup> main_menu_actions = ActionGroup::create (X_("Main_menu"));
	Glib::RefPtr<Action> act;

	/* menus + submenus that need action items */

	act=ActionManager::register_action (main_menu_actions, X_("Session"), _("File"));
    ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (main_actions, X_("Cleanup"), _("CleanUp"));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::register_action (main_menu_actions, X_("Sync"), _("Sync"));
	ActionManager::register_action (main_menu_actions, X_("TransportOptions"), _("Options"));
	act=ActionManager::register_action (main_menu_actions, X_("WindowMenu"), _("Window"));
    ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::register_action (main_menu_actions, X_("Help"), _("Help"));
 	ActionManager::register_action (main_menu_actions, X_("KeyMouseActions"), _("Misc. Shortcuts"));
	ActionManager::register_action (main_menu_actions, X_("AudioFileFormat"), _("Audio File Format"));
	ActionManager::register_action (main_menu_actions, X_("AudioFileFormatHeader"), _("File Type"));
	ActionManager::register_action (main_menu_actions, X_("AudioFileFormatData"), _("Sample Format"));
	ActionManager::register_action (main_menu_actions, X_("ControlSurfaces"), _("Control Surfaces"));
	ActionManager::register_action (main_menu_actions, X_("Plugins"), _("Plugins"));
	ActionManager::register_action (main_menu_actions, X_("Metering"), _("Metering"));
	ActionManager::register_action (main_menu_actions, X_("MeteringFallOffRate"), _("Fall Off Rate"));
	ActionManager::register_action (main_menu_actions, X_("MeteringHoldTime"), _("Hold Time"));
	ActionManager::register_action (main_menu_actions, X_("Denormals"), _("Denormal Handling"));

	/* the real actions */


	act = ActionManager::register_action (main_actions, X_("New"), _("New"),  hide_return (sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::get_session_parameters), false, true, "")));

	ActionManager::register_action (main_actions, X_("Open"), _("Open"),  sigc::mem_fun(*this, &ARDOUR_UI::open_session));
	ActionManager::register_action (main_actions, X_("Recent"), _("Recent"),  sigc::mem_fun(*this, &ARDOUR_UI::open_recent_session));
    /* register act for recent_session_menuitems */
    for(int i=0;i<MAX_RECENT_SESSION_COUNT;++i){
        std::string label=string_compose( ("%1%2"), recent_session_menuitem_id.c_str(),i ) ;
         ActionManager::register_action (main_actions, X_(label.c_str()), _(""),  sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::open_recent_session_from_menuitem), i));
    }
    
    
	act = ActionManager::register_action (main_actions, X_("Close"), _("Close"),  sigc::mem_fun(*this, &ARDOUR_UI::close_session));
	ActionManager::session_sensitive_actions.push_back (act);
    ActionManager::record_restricted_actions.push_back (act);
    
	act = ActionManager::register_action (main_actions, X_("AddTrackBus"), _("Add Track"),
					      sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::add_route), (Gtk::Window*) 0));
    ActionManager::record_restricted_actions.push_back (act);
	//ActionManager::session_sensitive_actions.push_back (act);
	//ActionManager::write_sensitive_actions.push_back (act);

    
	act = ActionManager::register_action (main_actions, X_("OpenVideo"), _("Open Video"),
					      sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::add_video), (Gtk::Window*) 0));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (main_actions, X_("CloseVideo"), _("Remove Video"),
					      sigc::mem_fun (*this, &ARDOUR_UI::remove_video));
	act->set_sensitive (false);
	act = ActionManager::register_action (main_actions, X_("ExportVideo"), _("Export To Video File"),
			hide_return (sigc::bind (sigc::mem_fun(*editor, &PublicEditor::export_video), false)));
	ActionManager::session_sensitive_actions.push_back (act);

#ifdef __APPLE__
	act = ActionManager::register_action (main_actions, X_("MainWindow"), _("Main Window"),  sigc::mem_fun(*this, &ARDOUR_UI::goto_editor_window));
    //ActionManager::session_sensitive_actions.push_back (act);
#endif
    act = ActionManager::register_action (main_actions, X_("Minimize"), _("Minimize"),  sigc::mem_fun(*this, &ARDOUR_UI::minimize_window));
    //ActionManager::session_sensitive_actions.push_back (act);
    act = ActionManager::register_action (main_actions, X_("Zoom"), _("Zoom"),  sigc::mem_fun(*this, &ARDOUR_UI::maximize_window));
    //ActionManager::session_sensitive_actions.push_back (act);
    
	ActionManager::register_action (main_actions, X_("LockSession"), _("Lock this session"),  sigc::mem_fun(*this, &ARDOUR_UI::on_lock_button_pressed));
	ActionManager::register_action (main_actions, X_("ToggleMultiOutMode"), "Multi Out", sigc::mem_fun(*this, &ARDOUR_UI::toggle_multi_out_mode));
	ActionManager::register_action (main_actions, X_("ToggleStereoOutMode"), "Stereo Out", sigc::mem_fun(*this, &ARDOUR_UI::toggle_stereo_out_mode));

	act = ActionManager::register_action (main_actions, X_("SnapshotStay"), _("Snapshot (& keep working on current version) ..."), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::snapshot_session), false));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("SnapshotSwitch"), _("Snapshot (& switch to new version) ..."), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::snapshot_session), true));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("SaveAs"), _("Save As..."), sigc::mem_fun(*this, &ARDOUR_UI::save_session_as));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("Rename"), _("Rename..."), sigc::mem_fun(*this, &ARDOUR_UI::rename_session));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("SaveTemplate"), _("Save Template..."),  sigc::mem_fun(*this, &ARDOUR_UI::save_template));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("Metadata"), _("Metadata"));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("EditMetadata"), _("Edit Metadata..."),  sigc::mem_fun(*this, &ARDOUR_UI::edit_metadata));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("ImportMetadata"), _("Import Metadata..."),  sigc::mem_fun(*this, &ARDOUR_UI::import_metadata));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("ExportAudio"), _("Mixdown"),  sigc::mem_fun (*editor, &PublicEditor::export_audio));
	//ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("StemExport"), _("Stem Export"),  sigc::mem_fun (*editor, &PublicEditor::stem_export));
	//ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("Export"), _("Export"));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (main_actions, X_("CleanupUnused"), _("Delete Unused Sources"),  sigc::mem_fun (*(ARDOUR_UI::instance()), &ARDOUR_UI::cleanup));
    ActionManager::record_restricted_actions.push_back (act);
	//ActionManager::session_sensitive_actions.push_back (act);
    act = ActionManager::register_action (main_actions, X_("ShowUnused"), _("Show Unused"),  sigc::mem_fun (*(ARDOUR_UI::instance()), &ARDOUR_UI::open_dead_folder));
    //ActionManager::session_sensitive_actions.push_back (act);
    
    ActionManager::register_action (main_actions, X_("recent-sessions"), _("Recent Sessions"));
	act = ActionManager::register_action (main_actions, X_("FlushWastebasket"), _("Flush Wastebasket"),  sigc::mem_fun (*(ARDOUR_UI::instance()), &ARDOUR_UI::flush_trash));
	ActionManager::write_sensitive_actions.push_back (act);
	ActionManager::session_sensitive_actions.push_back (act);

	/* these actions are intended to be shared across all windows */

	common_actions = ActionGroup::create (X_("Common"));
    
    act = ActionManager::register_action (main_actions, X_("AddTrackBus"), _("Add Track"),
                                          sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::add_route), (Gtk::Window*) 0));
    
	act = ActionManager::register_action (common_actions, X_("Quit"), _("Quit"), (hide_return (sigc::mem_fun(*this, &ARDOUR_UI::finish))));
    act = ActionManager::register_action (common_actions, X_("Hide"), _("Hide"), sigc::mem_fun (*this, &ARDOUR_UI::hide_application));

    
	/* windows visibility actions */

	ActionManager::register_toggle_action (common_actions, X_("ToggleMaximalEditor"), _("Maximise Editor Space"), sigc::mem_fun (*this, &ARDOUR_UI::toggle_editing_space));
	act = ActionManager::register_toggle_action (common_actions, X_("KeepTearoffs"), _("Show Toolbars"), mem_fun (*this, &ARDOUR_UI::toggle_keep_tearoffs));
	ActionManager::session_sensitive_actions.push_back (act);

	ActionManager::register_action (common_actions, X_("show-ui-prefs"), _("Show more UI preferences"), sigc::mem_fun (*this, &ARDOUR_UI::show_ui_prefs));

	ActionManager::register_toggle_action (common_actions, X_("toggle-mixer"), S_("Mixer"),  sigc::mem_fun(*this, &ARDOUR_UI::toggle_mixer_bridge_view));
	ActionManager::register_toggle_action (common_actions, X_("toggle-meterbridge"), S_("Meter Bridge"),  sigc::mem_fun(*this, &ARDOUR_UI::toggle_meterbridge));

	act = ActionManager::register_action (common_actions, X_("NewMIDITracer"), _("MIDI Tracer"), sigc::mem_fun(*this, &ARDOUR_UI::new_midi_tracer_window));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::register_action (common_actions, X_("Chat"), _("Chat"),  sigc::mem_fun(*this, &ARDOUR_UI::launch_chat));
	/** TRANSLATORS: This is `Manual' in the sense of an instruction book that tells a user how to use Ardour */
	ActionManager::register_action (common_actions, X_("Manual"), S_("Help|Manual"),  mem_fun(*this, &ARDOUR_UI::launch_manual));
	ActionManager::register_action (common_actions, X_("Reference"), _("Reference"),  mem_fun(*this, &ARDOUR_UI::launch_reference));

	ActionManager::register_action (common_actions, X_("OpenMediaFolder"), _("OpenMediaFolder"),  mem_fun(*this, &ARDOUR_UI::open_media_folder));

	act = ActionManager::register_action (common_actions, X_("Save"), _("Save"),  sigc::hide_return (sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::save_state), string(""), false)));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);

	Glib::RefPtr<ActionGroup> transport_actions = ActionGroup::create (X_("Transport"));

	/* do-nothing action for the "transport" menu bar item */

	act=ActionManager::register_action (transport_actions, X_("Transport"), _("Transport"));
    ActionManager::session_sensitive_actions.push_back (act);

	/* these two are not used by key bindings, instead use ToggleRoll for that. these two do show up in
	   menus and via button proxies.
	*/

	act = ActionManager::register_action (transport_actions, X_("Stop"), _("Stop"), sigc::mem_fun(*this, &ARDOUR_UI::transport_stop));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("Roll"), _("Roll"), sigc::mem_fun(*this, &ARDOUR_UI::transport_roll));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	act = ActionManager::register_action (transport_actions, X_("ToggleRoll"), _("Playback Start/Stop"), sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::toggle_roll), false, false));
	//ActionManager::session_sensitive_actions.push_back (act);
	//ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("alternate-ToggleRoll"), _("Start/Stop"), sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::toggle_roll), false, false));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("ToggleRollMaybe"), _("Start/Continue/Stop"), sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::toggle_roll), false, true));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	//Tracks Live doesn't use it
//    act = ActionManager::register_action (transport_actions, X_("ToggleRollForgetCapture"), _("Stop and Forget Capture"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::toggle_roll), true, false));
//	ActionManager::session_sensitive_actions.push_back (act);
//	ActionManager::transport_sensitive_actions.push_back (act);

	/* these two behave as follows:

	   - if transport speed != 1.0 or != -1.0, change speed to 1.0 or -1.0 (respectively)
	   - otherwise do nothing
	*/

	act = ActionManager::register_action (transport_actions, X_("TransitionToRoll"), _("Transition To Roll"), sigc::bind (sigc::mem_fun (*editor, &PublicEditor::transition_to_rolling), true));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	act = ActionManager::register_action (transport_actions, X_("TransitionToReverse"), _("Transition To Reverse"), sigc::bind (sigc::mem_fun (*editor, &PublicEditor::transition_to_rolling), false));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	act = ActionManager::register_action (transport_actions, X_("Loop"), _("Play Loop Range"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_session_auto_loop));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("PlaySelection"), _("Play Selected Range"), sigc::mem_fun(*this, &ARDOUR_UI::transport_play_selection));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("PlayPreroll"), _("Play Selection w/Preroll"), sigc::mem_fun(*this, &ARDOUR_UI::transport_play_preroll));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	act = ActionManager::register_action (transport_actions, X_("Record"), _("Record"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::transport_record), false));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("record-roll"), _("Start Recording"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::transport_record), true));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("alternate-record-roll"), _("Start Recording"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::transport_record), true));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::write_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("Rewind"), _("Rewind"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::transport_rewind), 0));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("RewindSlow"), _("Rewind (Slow)"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::transport_rewind), -1));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("RewindFast"), _("Rewind (Fast)"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::transport_rewind), 1));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("Forward"), _("Forward"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::transport_forward), 0));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("ForwardSlow"), _("Forward (Slow)"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::transport_forward), -1));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("ForwardFast"), _("Forward (Fast)"), sigc::bind (sigc::mem_fun(*this, &ARDOUR_UI::transport_forward), 1));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("GotoZero"), _("Goto Zero"), sigc::mem_fun(*this, &ARDOUR_UI::transport_goto_zero));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("GotoStart"), _("Jump to Session Start"), sigc::mem_fun(*this, &ARDOUR_UI::transport_goto_start));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("alternate-GotoStart"), _("Goto Start"), sigc::mem_fun(*this, &ARDOUR_UI::transport_goto_start));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("GotoEnd"), _("Jump to Session End"), sigc::mem_fun(*this, &ARDOUR_UI::transport_goto_end));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("GotoWallClock"), _("Goto Wall Clock"), sigc::mem_fun(*this, &ARDOUR_UI::transport_goto_wallclock));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	//these actions handle the numpad events, ProTools style
	act = ActionManager::register_action (transport_actions, X_("numpad-decimal"), _("Numpad Decimal"), mem_fun(*this, &ARDOUR_UI::transport_numpad_decimal));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("numpad-0"), _("Numpad 0"), bind (mem_fun(*this, &ARDOUR_UI::transport_numpad_event), 0));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("numpad-1"), _("Numpad 1"), bind (mem_fun(*this, &ARDOUR_UI::transport_numpad_event), 1));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("numpad-2"), _("Numpad 2"), bind (mem_fun(*this, &ARDOUR_UI::transport_numpad_event), 2));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("numpad-3"), _("Numpad 3"), bind (mem_fun(*this, &ARDOUR_UI::transport_numpad_event), 3));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("numpad-4"), _("Numpad 4"), bind (mem_fun(*this, &ARDOUR_UI::transport_numpad_event), 4));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("numpad-5"), _("Numpad 5"), bind (mem_fun(*this, &ARDOUR_UI::transport_numpad_event), 5));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("numpad-6"), _("Numpad 6"), bind (mem_fun(*this, &ARDOUR_UI::transport_numpad_event), 6));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("numpad-7"), _("Numpad 7"), bind (mem_fun(*this, &ARDOUR_UI::transport_numpad_event), 7));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("numpad-8"), _("Numpad 8"), bind (mem_fun(*this, &ARDOUR_UI::transport_numpad_event), 8));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("numpad-9"), _("Numpad 9"), bind (mem_fun(*this, &ARDOUR_UI::transport_numpad_event), 9));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	act = ActionManager::register_action (transport_actions, X_("focus-on-clock"), _("Focus On Clock"), sigc::mem_fun(*this, &ARDOUR_UI::focus_on_clock));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);

	act = ActionManager::register_action (transport_actions, X_("primary-clock-timecode"), _("Time code"), sigc::bind (sigc::mem_fun(primary_clock, &AudioClock::set_mode), AudioClock::Timecode));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("primary-clock-bbt"), _("Bars & Beats"), sigc::bind (sigc::mem_fun(primary_clock, &AudioClock::set_mode), AudioClock::BBT));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("primary-clock-minsec"), _("Time"), sigc::bind (sigc::mem_fun(primary_clock, &AudioClock::set_mode), AudioClock::MinSec));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("primary-clock-samples"), _("Samples"), sigc::bind (sigc::mem_fun(primary_clock, &AudioClock::set_mode), AudioClock::Frames));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (transport_actions, X_("secondary-clock-timecode"), _("Timecode"), sigc::bind (sigc::mem_fun(secondary_clock, &AudioClock::set_mode), AudioClock::Timecode));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("secondary-clock-bbt"), _("Bars & Beats"), sigc::bind (sigc::mem_fun(secondary_clock, &AudioClock::set_mode), AudioClock::BBT));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("secondary-clock-minsec"), _("Minutes & Seconds"), sigc::bind (sigc::mem_fun(secondary_clock, &AudioClock::set_mode), AudioClock::MinSec));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (transport_actions, X_("secondary-clock-samples"), _("Samples"), sigc::bind (sigc::mem_fun(secondary_clock, &AudioClock::set_mode), AudioClock::Frames));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_toggle_action (transport_actions, X_("TogglePunchIn"), _("Punch In"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_punch_in));
	act->set_short_label (_("In"));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (transport_actions, X_("TogglePunchOut"), _("Punch Out"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_punch_out));
	act->set_short_label (_("Out"));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (transport_actions, X_("TogglePunch"), _("Punch In/Out"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_punch));
	act->set_short_label (_("In/Out"));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (transport_actions, X_("ToggleClick"), _("Click"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_click));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (transport_actions, X_("ToggleAutoInput"), _("Auto Input"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_auto_input));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (transport_actions, X_("ToggleAutoPlay"), _("Auto Play"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_auto_play));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (transport_actions, X_("ToggleAutoReturn"), _("Auto Return"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_all_auto_return));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (transport_actions, X_("ToggleFollowEdits"), _("Follow Edits"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_follow_edits));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::transport_sensitive_actions.push_back (act);


	act = ActionManager::register_toggle_action (transport_actions, X_("ToggleVideoSync"), _("Sync Startup to Video"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_video_sync));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (transport_actions, X_("ToggleTimeMaster"), _("Time Master"), sigc::mem_fun(*this, &ARDOUR_UI::toggle_time_master));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (transport_actions, X_("ToggleExternalSync"), "", sigc::mem_fun(*this, &ARDOUR_UI::toggle_external_sync));
	ActionManager::session_sensitive_actions.push_back (act);

	for (int i = 1; i <= 32; ++i) {
		string const a = string_compose (X_("ToggleRecordEnableTrack%1"), i);
		string const n = string_compose (_("Toggle Record Enable Track %1"), i);
		act = ActionManager::register_action (common_actions, a.c_str(), n.c_str(), sigc::bind (sigc::mem_fun (*this, &ARDOUR_UI::toggle_record_enable), i - 1));
		ActionManager::session_sensitive_actions.push_back (act);
	}

	Glib::RefPtr<ActionGroup> shuttle_actions = ActionGroup::create ("ShuttleActions");

	shuttle_actions->add (Action::create (X_("SetShuttleUnitsPercentage"), _("Percentage")), hide_return (sigc::bind (sigc::mem_fun (*Config, &RCConfiguration::set_shuttle_units), Percentage)));
	shuttle_actions->add (Action::create (X_("SetShuttleUnitsSemitones"), _("Semitones")), hide_return (sigc::bind (sigc::mem_fun (*Config, &RCConfiguration::set_shuttle_units), Semitones)));

	Glib::RefPtr<ActionGroup> option_actions = ActionGroup::create ("options");

	act = ActionManager::register_toggle_action (option_actions, X_("SendMTC"), _("Send MTC"), sigc::mem_fun (*this, &ARDOUR_UI::toggle_send_mtc));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (option_actions, X_("SendMMC"), _("Send MMC"), sigc::mem_fun (*this, &ARDOUR_UI::toggle_send_mmc));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (option_actions, X_("UseMMC"), _("Use MMC"), sigc::mem_fun (*this, &ARDOUR_UI::toggle_use_mmc));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (option_actions, X_("SendMidiClock"), _("Send MIDI Clock"), sigc::mem_fun (*this, &ARDOUR_UI::toggle_send_midi_clock));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (option_actions, X_("SendMIDIfeedback"), _("Send MIDI Feedback"), sigc::mem_fun (*this, &ARDOUR_UI::toggle_send_midi_feedback));
	ActionManager::session_sensitive_actions.push_back (act);

	/* MIDI */

	Glib::RefPtr<ActionGroup> midi_actions = ActionGroup::create (X_("MIDI"));
	ActionManager::register_action (midi_actions, X_("panic"), _("Panic"), sigc::mem_fun(*this, &ARDOUR_UI::midi_panic));

	ActionManager::add_action_group (shuttle_actions);
	ActionManager::add_action_group (option_actions);
	ActionManager::add_action_group (transport_actions);
	ActionManager::add_action_group (main_actions);
	ActionManager::add_action_group (main_menu_actions);
	ActionManager::add_action_group (common_actions);
	ActionManager::add_action_group (midi_actions);
}

void
ARDOUR_UI::build_menu_bar ()
{
	menu_bar = dynamic_cast<MenuBar*> (ActionManager::get_widget (X_("/Main")));

	/*
	 * This is needed because this property does not get installed
	 * until the Menu GObject class is registered, which happens
	 * when the first menu instance is created.
	 */
	// XXX bug in gtkmm causes this to popup an error message
	// Gtk::Settings::get_default()->property_gtk_can_change_accels() = true;
	// so use this instead ...
	gtk_settings_set_long_property (gtk_settings_get_default(), "gtk-can-change-accels", 1, "Ardour:designers");

#ifndef TOP_MENUBAR
	menu_bar->set_name ("MainMenuBar");
	editor->get_h_box ("menu_bar_home").pack_start (*menu_bar, false, false);
#else
	use_menubar_as_top_menubar ();
#endif
	return;
}

void
ARDOUR_UI::use_menubar_as_top_menubar ()
{
	Gtk::Widget* widget;
	Application* app = Application::instance ();

        /* the addresses ("/ui/Main...") used below are based on the menu definitions in the menus file
         */

	/* Quit will be taken care of separately */

	if ((widget = ActionManager::get_widget ("/ui/Main/Session/Quit"))) {
		widget->hide ();
	}

	/* Put items for About and Preferences into App menu (the
	 * ardour.menus.in file does not list them for OS X)
	 */

	GtkApplicationMenuGroup* group = app->add_app_menu_group ();

	if ((widget = ActionManager::get_widget ("/ui/Main/Session/toggle-about"))) {
		app->add_app_menu_item (group, dynamic_cast<MenuItem*>(widget));
        }

	if ((widget = ActionManager::get_widget ("/ui/Main/Session/toggle-rc-options-editor"))) {
		app->add_app_menu_item (group, dynamic_cast<MenuItem*>(widget));
        }

	app->set_menu_bar (*menu_bar);
}

void
ARDOUR_UI::save_application_state ()
{
	if (!keyboard || !editor) {
		return;
	}

	/* XXX this is all a bit dubious. add_extra_xml() uses
	   a different lifetime model from add_instant_xml().
	*/

	XMLNode* node = new XMLNode (keyboard->get_state());
	Config->add_extra_xml (*node);
	Config->add_extra_xml (get_transport_controllable_state());

	XMLNode* window_node = new XMLNode (X_("UI"));
	
	/* Windows */

	WM::Manager::instance().add_state (*window_node);

	/* tearoffs */

	XMLNode* tearoff_node = new XMLNode (X_("Tearoffs"));

	if (editor && editor->mouse_mode_tearoff()) {
		XMLNode* t = new XMLNode (X_("mouse-mode"));
		editor->mouse_mode_tearoff ()->add_state (*t);
		tearoff_node->add_child_nocopy (*t);
	}

	window_node->add_child_nocopy (*tearoff_node);

	Config->add_extra_xml (*window_node);

	Config->save_state();

	if (ui_config->dirty()) {
		ui_config->save_state ();
	}

	XMLNode& enode (static_cast<Stateful*>(editor)->get_state());
    
    if (!_session) {
		Config->add_instant_xml (enode);
		if (location_ui) {
			Config->add_instant_xml (location_ui->ui().get_state ());
		}
	}
	delete &enode;

	Keyboard::save_keybindings ();
}

void
ARDOUR_UI::focus_on_clock ()
{
	if (editor && primary_clock) {
		editor->present ();
		primary_clock->focus ();
	}
}

void
ARDOUR_UI::update_marker_inspector (MarkerSelection* markers)
{
	if (_session && _session->deletion_in_progress()) {
		return;
	}
	
	if(markers && !markers->empty() && markers->back ()->location () && markers->back ()->location ()->is_mark ()) {
		marker_inspector_dialog->set_marker (markers->back ());
	} else {
		marker_inspector_dialog->set_marker (0);
	}
}

void
ARDOUR_UI::update_track_color_dialog (boost::shared_ptr<ARDOUR::Route> route)
{
	track_color_dialog->set_route (route);
}

void
ARDOUR_UI::show_marker_inspector ()
{
	marker_inspector_dialog->set_position (Gtk::WIN_POS_MOUSE);
	track_color_dialog->hide ();
    marker_inspector_dialog->show ();
    marker_inspector_dialog->present ();
}

void
ARDOUR_UI::show_track_color_dialog ()
{
	track_color_dialog->set_position (Gtk::WIN_POS_MOUSE);
	marker_inspector_dialog->hide ();
    track_color_dialog->show();
}
