/*
    Copyright (C) 2000-2006 Paul Davis

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

#include <cmath>
#include <list>
#include <algorithm>

#include <sigc++/bind.h>

#include "pbd/convert.h"
#include "pbd/enumwriter.h"
#include "pbd/replace_all.h"
#include "pbd/stacktrace.h"
#include "pbd/whitespace.h"

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/choice.h>
#include <gtkmm2ext/doi.h>
#include <gtkmm2ext/slider_controller.h>
#include <gtkmm2ext/bindable_button.h>
#include "gtkmm2ext/rgb_macros.h"

#include "ardour/audio_track.h"
#include "ardour/audioengine.h"
#include "ardour/internal_send.h"
#include "ardour/meter.h"
#include "ardour/midi_track.h"
#include "ardour/pannable.h"
#include "ardour/panner.h"
#include "ardour/panner_shell.h"
#include "ardour/panner_manager.h"
#include "ardour/port.h"
#include "ardour/profile.h"
#include "ardour/route.h"
#include "ardour/route_group.h"
#include "ardour/send.h"
#include "ardour/session.h"
#include "ardour/types.h"
#include "ardour/user_bundle.h"
#include "ardour/data_type.h"
#include "ardour/audio_backend.h"
#include "ardour/engine_state_controller.h"

#include "ardour_ui.h"
#include "ardour_window.h"
#include "mixer_strip.h"
#include "mixer_ui.h"
#include "keyboard.h"
#include "ardour_button.h"
#include "public_editor.h"
#include "send_ui.h"
#include "io_selector.h"
#include "utils.h"
#include "gui_thread.h"
#include "route_group_menu.h"
#include "meter_patterns.h"
#include "waves_message_dialog.h"

#include "i18n.h"
#include "dbg_msg.h"

using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace std;
using namespace ArdourMeter;

int MixerStrip::scrollbar_height = 0;
PBD::Signal1<void,MixerStrip*> MixerStrip::CatchDeletion;
PBD::Signal2<void,MixerStrip::TabToStrip,const MixerStrip*> MixerStrip::EndStripNameEdit;

MixerStrip::MixerStrip (Session* sess, const std::string& layout_script_file, size_t max_name_size)
	: AxisView(sess)
	, RouteUI (sess, layout_script_file)
    , _max_name_size (max_name_size)
	, _mixer_owned (xml_property(*xml_tree()->root(), "selfdestruct", true))
//	, processor_box (sess, boost::bind (&MixerStrip::plugin_selector, this), mx.selection(), this, xml_property(*xml_tree()->root(), "selfdestruct", true))
	, gpm (sess, xml_property(*xml_tree()->root(), "gainmeterscript", "default_gain_meter.xml"))
	, panners (sess)
	, _visibility (X_("mixer-strip-visibility"))

    ,_editor (ARDOUR_UI::instance()->the_editor())

	, gain_meter_home (get_box ("gain_meter_home"))
	, _comment_button (get_waves_button ("comment_button"))
	, midi_input_enable_button (get_waves_button ("midi_input_enable_button"))
    , _name_button_home (get_event_box("name_label_home"))
    , name_button (get_waves_button ("name_button"))
    , _name_entry (get_entry("name_entry"))
    , _name_entry_eventbox (get_event_box("name_entry_eventbox"))
	, group_button (get_waves_button ("group_button"))
	, panners_home (get_event_box ("panners_home"))
{
	init ();

	if (!_mixer_owned) {
		/* the editor mixer strip: don't destroy it every time
		   the underlying route goes away.
		*/

		self_destruct = false;
	}
}

namespace {
    bool is_vowel_letter(char letter)
    {
        if( letter=='a' || letter=='e' || letter=='i' || letter=='o' ||
            letter=='q' || letter=='u' || letter=='y')
            return true;
        return false;
    }
    
    string cut_string(string route_name, size_t max_name_size)
    {
        if ( max_name_size == 0 )
            return route_name;
        
        string cutted_route_name;  
        
        if ( route_name.size()<=max_name_size )
        {
            cutted_route_name = route_name;
        }
        else
        {
            // first step: delete vowel letters
            int i = route_name.size()-1; // iterator
            while( i >= 0 && route_name.size() > max_name_size )
            {
                if( is_vowel_letter( route_name[i] ) )
                    route_name.erase(i, 1);
                else
                    --i;
            }
            
            if ( route_name.size()<=max_name_size )
                cutted_route_name = route_name;
            else  // second step: leave only first letters
                cutted_route_name.assign(route_name, 0, max_name_size);
        }
        
        return cutted_route_name;
    }
}
    
MixerStrip::MixerStrip (Session* sess, boost::shared_ptr<Route> rt, const std::string& layout_script_file, size_t max_name_size)
	: AxisView(sess)
	, RouteUI (sess, layout_script_file)
    , _max_name_size(max_name_size)
	, _mixer_owned (xml_property(*xml_tree()->root(), "selfdestruct", true))
//	, processor_box (sess, boost::bind (&MixerStrip::plugin_selector, this), mx.selection(), this, xml_property(*xml_tree()->root(), "selfdestruct", true))
	, gpm (sess, xml_property(*xml_tree()->root(), "gainmeterscript", "default_gain_meter.xml"))
	, panners (sess)
	, _visibility (X_("mixer-strip-visibility"))

    , _editor (ARDOUR_UI::instance()->the_editor())

	, gain_meter_home (get_box ("gain_meter_home"))
	, _comment_button (get_waves_button ("comment_button"))
	, midi_input_enable_button (get_waves_button ("midi_input_enable_button"))
    , _name_button_home (get_event_box("name_label_home"))
    , name_button (get_waves_button ("name_button"))
    , _name_entry (get_entry ("name_entry"))
    , _name_entry_eventbox (get_event_box("name_entry_eventbox"))
	, group_button (get_waves_button ("group_button"))
	, panners_home (get_event_box ("panners_home"))
{
	init ();
	set_route (rt);
	name_button.set_text ( cut_string(_route->name(), _max_name_size) );
}

void
MixerStrip::init ()
{
	input_selector = 0;
	output_selector = 0;
	group_menu = 0;
	ignore_comment_edit = false;
	ignore_toggle = false;
	comment_window = 0;
	comment_area = 0;

	string t = _("Click to toggle the width of this mixer strip.");
	if (_mixer_owned) {
		t += string_compose (_("\n%1-%2-click to toggle the width of all strips."), Keyboard::primary_modifier_name(), Keyboard::tertiary_modifier_name ());
	}
	_comment_button.signal_clicked.connect (sigc::mem_fun (*this, &MixerStrip::toggle_comment_editor));

	panners_home.add (panners);

	/* force setting of visible selected status */

	_selected = true;
	set_selected (false);

	_packed = false;
	_embedded = false;

	_session->engine().Stopped.connect (*this, invalidator (*this), boost::bind (&MixerStrip::engine_stopped, this), gui_context());
	_session->engine().Running.connect (*this, invalidator (*this), boost::bind (&MixerStrip::engine_running, this), gui_context());
    _session->RecordStateChanged.connect (*this, invalidator (*this), boost::bind (&MixerStrip::on_record_state_changed, this), gui_context());

	/* ditto for this button and busses */

	name_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MixerStrip::name_button_button_press), false);
	group_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MixerStrip::select_route_group), false);

	/* start off as a passthru strip. we'll correct this, if necessary,
	   in update_diskstream_display().
	*/

	/* start off as a passthru strip. we'll correct this, if necessary,
	   in update_diskstream_display().
	*/

	add_events (Gdk::BUTTON_RELEASE_MASK|
		    Gdk::ENTER_NOTIFY_MASK|
		    Gdk::LEAVE_NOTIFY_MASK|
		    Gdk::KEY_PRESS_MASK|
		    Gdk::KEY_RELEASE_MASK);

	set_flags (get_flags() | Gtk::CAN_FOCUS);

	AudioEngine::instance()->PortConnectedOrDisconnected.connect (
		*this, invalidator (*this), boost::bind (&MixerStrip::port_connected_or_disconnected, this, _1, _3), gui_context ()
		);

	gain_meter_home.pack_start (gpm, false, false);

	/* Add the widgets under visibility control to the VisibilityGroup; the names used here
	   must be the same as those used in RCOptionEditor so that the configuration changes
	   are recognised when they occur.
	*/
	_visibility.add (&_comment_button, X_("Comments"), _("Comments"));
	_visibility.add (&group_button, X_("Group"), _("Group"));

	parameter_changed (X_("mixer-strip-visibility"));

	Config->ParameterChanged.connect (_config_connection, MISSING_INVALIDATOR, boost::bind (&MixerStrip::parameter_changed, this, _1), gui_context());
   
    _name_entry.signal_key_press_event().connect (sigc::mem_fun (*this, &MixerStrip::name_entry_key_press), false);
	_name_entry.signal_key_release_event().connect (sigc::mem_fun (*this, &MixerStrip::name_entry_key_release), false);
	_name_entry.signal_focus_out_event().connect (sigc::mem_fun (*this, &MixerStrip::name_entry_focus_out));
	
    if( _route )
        _name_entry.set_text ( _route->name() );
	
    _name_entry.signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &MixerStrip::end_name_edit), RESPONSE_OK));
    
    _name_button_home.add_events (Gdk::BUTTON_PRESS_MASK|
								   Gdk::BUTTON_RELEASE_MASK|
								   Gdk::POINTER_MOTION_MASK);
	_name_button_home.set_flags (CAN_FOCUS);
    
	/* note that this handler connects *before* the default handler */
	_name_button_home.signal_button_press_event().connect (sigc::mem_fun (*this, &MixerStrip::controls_ebox_button_press));
    _name_button_home.signal_button_release_event().connect (sigc::mem_fun (*this, &MixerStrip::controls_ebox_button_release));
    _name_entry.set_max_length(250);
    
    deletion_in_progress = false;
}

MixerStrip::~MixerStrip ()
{
    deletion_in_progress = true;
    
	CatchDeletion (this);

	delete input_selector;
	delete output_selector;
	delete comment_window;
}

bool
MixerStrip::controls_ebox_button_press (GdkEventButton* event)
{
	if ((event->button == 1 && event->type == GDK_2BUTTON_PRESS) || Keyboard::is_edit_event (event)) {
		/* see if it is inside the name label */
		if (name_button.is_ancestor (_name_button_home)) {
			int nlx;
			int nly;
			_name_button_home.translate_coordinates (name_button, event->x, event->y, nlx, nly);
			Gtk::Allocation a = name_button.get_allocation ();
			if (nlx > 0 && nlx < a.get_width() && nly > 0 && nly < a.get_height()) {
				_begin_name_edit ();
				return true;
			}
		}
        
	}
           
	return true;
}

bool
MixerStrip::controls_ebox_button_release (GdkEventButton* ev)
{       
	return false;
}

void
MixerStrip::selection_click (GdkEventButton* ev)
{
	Selection::Operation op = ArdourKeyboard::selection_type (ev->state);
}

bool
MixerStrip::name_entry_key_press (GdkEventKey* ev)
{
	/* steal escape, tabs from GTK */
    
	switch (ev->keyval) {
        case GDK_Escape:
        case GDK_ISO_Left_Tab:
        case GDK_Tab:
            return true;
	}
	return false;
}

bool
MixerStrip::name_entry_key_release (GdkEventKey* ev)
{
	TrackViewList::iterator i;
    
	switch (ev->keyval) {
        case GDK_Escape:
            end_name_edit (RESPONSE_CANCEL);
            return true;
            
            /* Shift+Tab Keys Pressed. Note that for Shift+Tab, GDK actually
             * generates a different ev->keyval, rather than setting
             * ev->state.
             */
        case GDK_ISO_Left_Tab:
            end_name_edit (RESPONSE_APPLY);
            return true;
            
        case GDK_Tab:
            end_name_edit (RESPONSE_ACCEPT);
            return true;
        default:
            break;
	}
    
	return false;
}


bool
MixerStrip::name_entry_focus_out (GdkEventFocus*)
{
	end_name_edit (RESPONSE_OK);
	return false;
}


void
MixerStrip::_begin_name_edit ()
{
    if (!_route)
        return;
    
    if( _route->is_master () )
        return;
    
    if ( (_session->record_status()==Session::Recording) && (_route->record_enabled()) )
        return;
    
    _name_entry.set_text ( _route->name() );
    name_button.hide();
    _name_entry_eventbox.show ();
    _name_entry.show ();
        
    _name_entry.select_region (0, -1);
    _name_entry.set_state (STATE_NORMAL);
    _name_entry.grab_focus ();
    _name_entry.start_editing (0);
}

void
MixerStrip::route_rec_enable_changed ()
{
    RouteUI::route_rec_enable_changed ();
    on_record_state_changed ();
}

void
MixerStrip::on_record_state_changed ()
{
    if ( !_session )
        return;
    
    if ( (_session->record_status()==Session::Recording) && (_route) && (_route->record_enabled()) )
        end_name_edit (RESPONSE_CANCEL);
}

void
MixerStrip::end_name_edit (int response)
{
    if (_name_entry.get_visible () == false) {
        return;
    }
    
    bool edit_next = false;
    bool edit_prev = false;
    
	switch (response) {
        case RESPONSE_CANCEL:
            break;
        case RESPONSE_OK:
            name_entry_changed ();
            break;
        case RESPONSE_ACCEPT:
            name_entry_changed ();
            edit_next = true;
        case RESPONSE_APPLY:
            name_entry_changed ();
            edit_prev = true;
	}
    
    // _name_entry's text and _route->name must be synchronized
    if ( _route )
        _name_entry.set_text ( _route->name () );
	
    name_button.show ();
	_name_entry.hide ();
    _name_entry_eventbox.hide ();
    if (edit_next) {
        EndStripNameEdit (TabToNext, this); //EMIT SIGNAL
    }
    else if (edit_prev) {
        EndStripNameEdit (TabToPrev, this); //EMIT SIGNAL
    }
}

void
MixerStrip::name_entry_changed ()
{
    if (!_route) {
        return;
    }
    
    string x = _name_entry.get_text ();
    
	if (x == _route->name() ) {
		return;
	}
    
	strip_whitespace_edges (x);
    
	if (x.length() == 0) {
		_name_entry.set_text (_route->name() );
		return;
	}
    
	if (_session->route_name_internal (x)) {
		ARDOUR_UI::instance()->popup_error (string_compose (_("You cannot create a track with that name as it is reserved for %1"),
                                                            PROGRAM_NAME));
		_name_entry.grab_focus ();
	} else if (RouteUI::verify_new_route_name (x)) {
		_route->set_name (x);
	} else {
		_name_entry.grab_focus ();
	}
}

void
MixerStrip::set_route (boost::shared_ptr<Route> rt)
{
    if ( deletion_in_progress )
        return;
    
    if ( !_session )
        return;
    
    end_name_edit (RESPONSE_OK);
    
	RouteUI::set_route (rt);

    _name_entry.set_text ( _route->name() );
	/* ProcessorBox needs access to _route so that it can read
	   GUI object state.
	*/
//	processor_box.set_route (rt);

	/* map the current state */

	mute_changed (0);
	update_solo_display ();

	delete input_selector;
	input_selector = 0;

	delete output_selector;
	output_selector = 0;

	revert_to_default_display ();

	/* unpack these from the parent and stuff them into our own
	   table
	*/
	
	gpm.set_type (rt->meter_type());
	
	master_mute_button.set_visible (route()->is_master());
	get_container ("track_buttons_home").set_visible (!route()->is_master());

	if (_mixer_owned && (route()->is_master() || route()->is_monitor())) {

		if (scrollbar_height == 0) {
			HScrollbar scrollbar;
			Gtk::Requisition requisition(scrollbar.size_request ());
			scrollbar_height = requisition.height;
		}
	}

	if (is_track()) {
		monitor_input_button.show ();
	} else {
		monitor_input_button.hide();
	}

	if (is_midi_track()) {
		midi_input_enable_button.show();
		/* get current state */
		midi_input_status_changed ();
		/* follow changes */
		midi_track()->InputActiveChanged.connect (route_connections, invalidator (*this), boost::bind (&MixerStrip::midi_input_status_changed, this), gui_context());
	} else {
		midi_input_enable_button.hide();
	}

	if (is_audio_track()) {
		boost::shared_ptr<AudioTrack> at = audio_track();
		at->FreezeChange.connect (route_connections, invalidator (*this), boost::bind (&MixerStrip::map_frozen, this), gui_context());
	}

	if (is_track ()) {
		rec_enable_button.set_sensitive (_session->writable());
		rec_enable_button.show();
        gpm.set_affected_by_selection(true);
	} else {

		/* non-master bus */

		if (!_route->is_master()) {
			show_sends_button.show();
        }
	}

	_route->meter_change.connect (route_connections, invalidator (*this), bind (&MixerStrip::meter_changed, this), gui_context());
	_route->input()->changed.connect (*this, invalidator (*this), boost::bind (&MixerStrip::update_output_display, this), gui_context());
	_route->output()->changed.connect (*this, invalidator (*this), boost::bind (&MixerStrip::update_output_display, this), gui_context());
	_route->route_group_changed.connect (route_connections, invalidator (*this), boost::bind (&MixerStrip::route_group_changed, this), gui_context());

	_route->io_changed.connect (route_connections, invalidator (*this), boost::bind (&MixerStrip::io_changed_proxy, this), gui_context ());

	if (_route->panner_shell()) {
		update_panner_choices();
		_route->panner_shell()->Changed.connect (route_connections, invalidator (*this), boost::bind (&MixerStrip::connect_to_pan, this), gui_context());
	}

	if (is_audio_track()) {
		audio_track()->DiskstreamChanged.connect (route_connections, invalidator (*this), boost::bind (&MixerStrip::diskstream_changed, this), gui_context());
	}

	_route->comment_changed.connect (route_connections, invalidator (*this), boost::bind (&MixerStrip::comment_changed, this, _1), gui_context());
	_route->PropertyChanged.connect (route_connections, invalidator (*this), boost::bind (&MixerStrip::property_changed, this, _1), gui_context());

	set_stuff_from_route ();

	/* now force an update of all the various elements */

	mute_changed (0);
	update_solo_display ();
	name_changed ();
	comment_changed (0);
	route_group_changed ();

	connect_to_pan ();
	panners.setup_pan ();

	if (has_audio_outputs ()) {
		panners.show_all ();
	} else {
		panners.hide_all ();
	}

	update_diskstream_display ();
	update_input_display ();
	update_output_display ();

	add_events (Gdk::BUTTON_RELEASE_MASK);

//	processor_box.show ();

	if (!route()->is_master() && !route()->is_monitor()) {
		/* we don't allow master or control routes to be hidden */
		//hide_button.show();
	}

	gpm.reset_peak_display ();
	gpm.show ();

	parameter_changed ("mixer-strip-visibility");

	show ();
}

void
MixerStrip::set_stuff_from_route ()
{
	/* if width is not set, it will be set by the MixerUI or editor */

	//string str = gui_property ("strip-width");
	//if (!str.empty()) {
	//	set_width_enum (Width (string_2_enum (str, _width)), this);
	//}
}

void
MixerStrip::set_packed (bool yn)
{
	_packed = yn;

	if (_packed) {
		set_gui_property ("visible", true);
	} else {
		set_gui_property ("visible", false);
	}
}


struct RouteCompareByName {
	bool operator() (boost::shared_ptr<Route> a, boost::shared_ptr<Route> b) {
		return a->name().compare (b->name()) < 0;
	}
};

gint
MixerStrip::output_press (GdkEventButton *ev)
{
	using namespace Menu_Helpers;
	if (!_session->engine().connected()) {
		WavesMessageDialog msg ("", _("Not connected to audio engine - no I/O changes are possible"));
		msg.run ();
		return true;
	}

	MenuList& citems = output_menu.items();
	switch (ev->button) {

	case 1:
		edit_output_configuration ();
		break;

	case 3:
	{
		output_menu.set_name ("ArdourContextMenu");
		citems.clear ();
		output_menu_bundles.clear ();

		citems.push_back (MenuElem (_("Disconnect"), sigc::mem_fun (*(static_cast<RouteUI*>(this)), &RouteUI::disconnect_output)));

		for (DataType::iterator i = DataType::begin(); i != DataType::end(); ++i) {
			citems.push_back (
				MenuElem (
					string_compose ("Add %1 port", (*i).to_i18n_string()),
					sigc::bind (sigc::mem_fun (*this, &MixerStrip::add_output_port), *i)
					)
				);
		}
		
		citems.push_back (SeparatorElem());
		uint32_t const n_with_separator = citems.size ();

		ARDOUR::BundleList current = _route->output()->bundles_connected ();

		boost::shared_ptr<ARDOUR::BundleList> b = _session->bundles ();

		/* give user bundles first chance at being in the menu */

		for (ARDOUR::BundleList::iterator i = b->begin(); i != b->end(); ++i) {
			if (boost::dynamic_pointer_cast<UserBundle> (*i)) {
				maybe_add_bundle_to_output_menu (*i, current);
			}
		}

		for (ARDOUR::BundleList::iterator i = b->begin(); i != b->end(); ++i) {
			if (boost::dynamic_pointer_cast<UserBundle> (*i) == 0) {
				maybe_add_bundle_to_output_menu (*i, current);
			}
		}

		boost::shared_ptr<ARDOUR::RouteList> routes = _session->get_routes ();
		RouteList copy = *routes;
		copy.sort (RouteCompareByName ());
		for (ARDOUR::RouteList::const_iterator i = copy.begin(); i != copy.end(); ++i) {
			maybe_add_bundle_to_output_menu ((*i)->input()->bundle(), current);
		}

		if (citems.size() == n_with_separator) {
			/* no routes added; remove the separator */
			citems.pop_back ();
		}

		output_menu.popup (1, ev->time);
		break;
	}

	default:
		break;
	}
	return TRUE;
}

void
MixerStrip::edit_output_configuration ()
{
	if (output_selector == 0) {

		boost::shared_ptr<Send> send;
		boost::shared_ptr<IO> output;

		if ((send = boost::dynamic_pointer_cast<Send>(_current_delivery)) != 0) {
			if (!boost::dynamic_pointer_cast<InternalSend>(send)) {
				output = send->output();
			} else {
				output = _route->output ();
			}
		} else {
			output = _route->output ();
		}

		output_selector = new IOSelectorWindow (_session, output);
	}

	if (output_selector->is_visible()) {
		output_selector->get_toplevel()->get_window()->raise();
	} else {
		output_selector->present ();
	}

	output_selector->set_keep_above (true);
}

void
MixerStrip::edit_input_configuration ()
{
	if (input_selector == 0) {
		input_selector = new IOSelectorWindow (_session, _route->input());
	}

	if (input_selector->is_visible()) {
		input_selector->get_toplevel()->get_window()->raise();
	} else {
		input_selector->present ();
	}

	input_selector->set_keep_above (true);
}

gint
MixerStrip::input_press (GdkEventButton *ev)
{
	using namespace Menu_Helpers;

	MenuList& citems = input_menu.items();
	input_menu.set_name ("ArdourContextMenu");
	citems.clear();

	if (!_session->engine().connected()) {
		WavesMessageDialog msg ("", _("Not connected to audio engine - no I/O changes are possible"));
		msg.run ();
		return true;
	}

	if (_session->actively_recording() && _route->record_enabled())
		return true;

	switch (ev->button) {

	case 1:
		edit_input_configuration ();
		break;

	case 3:
	{
		citems.push_back (MenuElem (_("Disconnect"), sigc::mem_fun (*(static_cast<RouteUI*>(this)), &RouteUI::disconnect_input)));

		for (DataType::iterator i = DataType::begin(); i != DataType::end(); ++i) {
			citems.push_back (
				MenuElem (
					string_compose ("Add %1 port", (*i).to_i18n_string()),
					sigc::bind (sigc::mem_fun (*this, &MixerStrip::add_input_port), *i)
					)
				);
		}

		citems.push_back (SeparatorElem());
		uint32_t const n_with_separator = citems.size ();
		
		input_menu_bundles.clear ();

		ARDOUR::BundleList current = _route->input()->bundles_connected ();

		boost::shared_ptr<ARDOUR::BundleList> b = _session->bundles ();

		/* give user bundles first chance at being in the menu */

		for (ARDOUR::BundleList::iterator i = b->begin(); i != b->end(); ++i) {
			if (boost::dynamic_pointer_cast<UserBundle> (*i)) {
				maybe_add_bundle_to_input_menu (*i, current);
			}
		}

		for (ARDOUR::BundleList::iterator i = b->begin(); i != b->end(); ++i) {
			if (boost::dynamic_pointer_cast<UserBundle> (*i) == 0) {
				maybe_add_bundle_to_input_menu (*i, current);
			}
		}

		boost::shared_ptr<ARDOUR::RouteList> routes = _session->get_routes ();
		RouteList copy = *routes;
		copy.sort (RouteCompareByName ());
		for (ARDOUR::RouteList::const_iterator i = copy.begin(); i != copy.end(); ++i) {
			maybe_add_bundle_to_input_menu ((*i)->output()->bundle(), current);
		}

		if (citems.size() == n_with_separator) {
			/* no routes added; remove the separator */
			citems.pop_back ();
		}

		input_menu.popup (1, ev->time);
		break;
	}
	default:
		break;
	}
	return TRUE;
}

void
MixerStrip::bundle_input_chosen (boost::shared_ptr<ARDOUR::Bundle> c)
{
	if (ignore_toggle) {
		return;
	}

	ARDOUR::BundleList current = _route->input()->bundles_connected ();

	if (std::find (current.begin(), current.end(), c) == current.end()) {
		_route->input()->connect_ports_to_bundle (c, true, this);
	} else {
		_route->input()->disconnect_ports_from_bundle (c, this);
	}
}

void
MixerStrip::bundle_output_chosen (boost::shared_ptr<ARDOUR::Bundle> c)
{
	if (ignore_toggle) {
		return;
	}

	ARDOUR::BundleList current = _route->output()->bundles_connected ();

	if (std::find (current.begin(), current.end(), c) == current.end()) {
		_route->output()->connect_ports_to_bundle (c, true, this);
	} else {
		_route->output()->disconnect_ports_from_bundle (c, this);
	}
}

void
MixerStrip::maybe_add_bundle_to_input_menu (boost::shared_ptr<Bundle> b, ARDOUR::BundleList const& /*current*/)
{
	using namespace Menu_Helpers;

 	if (b->ports_are_outputs() == false || b->nchannels() != _route->n_inputs() || *b == *_route->output()->bundle()) {
 		return;
 	}

	list<boost::shared_ptr<Bundle> >::iterator i = input_menu_bundles.begin ();
	while (i != input_menu_bundles.end() && b->has_same_ports (*i) == false) {
		++i;
	}

	if (i != input_menu_bundles.end()) {
		return;
	}

	input_menu_bundles.push_back (b);

	MenuList& citems = input_menu.items();

	std::string n = b->name ();
	replace_all (n, "_", " ");

	citems.push_back (MenuElem (n, sigc::bind (sigc::mem_fun(*this, &MixerStrip::bundle_input_chosen), b)));
}

void
MixerStrip::maybe_add_bundle_to_output_menu (boost::shared_ptr<Bundle> b, ARDOUR::BundleList const& /*current*/)
{
	using namespace Menu_Helpers;

 	if (b->ports_are_inputs() == false || b->nchannels() != _route->n_outputs() || *b == *_route->input()->bundle()) {
 		return;
 	}

	list<boost::shared_ptr<Bundle> >::iterator i = output_menu_bundles.begin ();
	while (i != output_menu_bundles.end() && b->has_same_ports (*i) == false) {
		++i;
	}

	if (i != output_menu_bundles.end()) {
		return;
	}

	output_menu_bundles.push_back (b);

	MenuList& citems = output_menu.items();

	std::string n = b->name ();
	replace_all (n, "_", " ");

	citems.push_back (MenuElem (n, sigc::bind (sigc::mem_fun(*this, &MixerStrip::bundle_output_chosen), b)));
}

void
MixerStrip::update_diskstream_display ()
{
    if (is_track() && input_selector) {
        input_selector->hide_all ();
    }
    
    route_color_changed ();
}

void
MixerStrip::connect_to_pan ()
{
	ENSURE_GUI_THREAD (*this, &MixerStrip::connect_to_pan)

	panstate_connection.disconnect ();
	panstyle_connection.disconnect ();

	if (!_route->panner()) {
		return;
	}

	boost::shared_ptr<Pannable> p = _route->pannable ();

	p->automation_state_changed.connect (panstate_connection, invalidator (*this), boost::bind (&PannerUI::pan_automation_state_changed, &panners), gui_context());
	p->automation_style_changed.connect (panstyle_connection, invalidator (*this), boost::bind (&PannerUI::pan_automation_style_changed, &panners), gui_context());

	/* This call reduncant, PannerUI::set_panner() connects to _panshell->Changed itself
	 * However, that only works a panner was previously set.
	 *
	 * PannerUI must remain subscribed to _panshell->Changed() in case
	 * we switch the panner eg. AUX-Send and back
	 * _route->panner_shell()->Changed() vs _panshell->Changed
	 */
	if (panners._panner == 0) {
		panners.panshell_changed ();
	}
	update_panner_choices();
}

void
MixerStrip::update_panner_choices ()
{
	ENSURE_GUI_THREAD (*this, &MixerStrip::update_panner_choices)
	if (!_route->panner_shell()) { return; }

	uint32_t in = _route->output()->n_ports().n_audio();
	uint32_t out = in;
	if (_route->panner()) {
		in = _route->panner()->in().n_audio();
	}

	panners.set_available_panners(PannerManager::instance().PannerManager::get_available_panners(in, out));
}

/*
 * Output port labelling
 * =====================
 *
 * Case 1: Each output has one connection, all connections are to system:playback_%i
 *   out 1 -> system:playback_1
 *   out 2 -> system:playback_2
 *   out 3 -> system:playback_3
 *   Display as: 1/2/3
 *
 * Case 2: Each output has one connection, all connections are to ardour:track_x/in 1
 *   out 1 -> ardour:track_x/in 1
 *   out 2 -> ardour:track_x/in 2
 *   Display as: track_x
 *
 * Case 3: Each output has one connection, all connections are to Jack client "program x"
 *   out 1 -> program x:foo
 *   out 2 -> program x:foo
 *   Display as: program x
 *
 * Case 4: No connections (Disconnected)
 *   Display as: -
 *
 * Default case (unusual routing):
 *   Display as: *number of connections*
 *
 * Tooltips
 * ========
 * .-----------------------------------------------.
 * | Mixdown                                       |
 * | out 1 -> ardour:master/in 1, jamin:input/in 1 |
 * | out 2 -> ardour:master/in 2, jamin:input/in 2 |
 * '-----------------------------------------------'
 * .-----------------------------------------------.
 * | Guitar SM58                                   |
 * | Disconnected                                  |
 * '-----------------------------------------------'
 */

void
//MixerStrip::update_io_button (boost::shared_ptr<ARDOUR::Route> route, Width width, bool for_input)
MixerStrip::update_io_button (boost::shared_ptr<ARDOUR::Route> route, bool for_input)
{
	uint32_t io_count;
	uint32_t io_index;
	boost::shared_ptr<Port> port;
	vector<string> port_connections;

	uint32_t total_connection_count = 0;
	uint32_t io_connection_count = 0;
	uint32_t ardour_connection_count = 0;
	uint32_t system_connection_count = 0;
	uint32_t other_connection_count = 0;

	ostringstream label;
	string label_string;

	bool have_label = false;
	bool each_io_has_one_connection = true;

	string connection_name;
	string ardour_track_name;
	string other_connection_type;
	string system_ports;
	string system_port;

	ostringstream tooltip;

	if (for_input) {
		io_count = route->n_inputs().n_total();
		tooltip << string_compose (_("<b>INPUT</b> to %1"), std::string (Glib::Markup::escape_text(route->name())));
	} else {
		io_count = route->n_outputs().n_total();
		tooltip << string_compose (_("<b>OUTPUT</b> from %1"), std::string (Glib::Markup::escape_text(route->name())));
	}


	for (io_index = 0; io_index < io_count; ++io_index) {
		if (for_input) {
			port = route->input()->nth (io_index);
		} else {
			port = route->output()->nth (io_index);
		}

		port_connections.clear ();
		port->get_connections(port_connections);
		io_connection_count = 0;

		if (!port_connections.empty()) {
			for (vector<string>::iterator i = port_connections.begin(); i != port_connections.end(); ++i) {
				string& connection_name (*i);

				if (io_connection_count == 0) {
					tooltip << endl << std::string (Glib::Markup::escape_text(port->name().substr(port->name().find("/") + 1))) << " -> " << std::string (Glib::Markup::escape_text(connection_name));
				} else {
					tooltip << ", " << std::string (Glib::Markup::escape_text(connection_name));
				}

				if (connection_name.find("ardour:") == 0) {
					if (ardour_track_name.empty()) {
						// "ardour:Master/in 1" -> "ardour:Master/"
						string::size_type slash = connection_name.find("/");
						if (slash != string::npos) {
							ardour_track_name = connection_name.substr(0, slash + 1);
						}
					}

					if (connection_name.find(ardour_track_name) == 0) {
						++ardour_connection_count;
					}
				} else if (connection_name.find("system:") == 0) {
					if (for_input) {
						// "system:capture_123" -> "123"
						system_port = connection_name.substr(15);
					} else {
						// "system:playback_123" -> "123"
						system_port = connection_name.substr(16);
					}

					if (system_ports.empty()) {
						system_ports += system_port;
					} else {
						system_ports += "/" + system_port;
					}

					++system_connection_count;
				} else {
					if (other_connection_type.empty()) {
						// "jamin:in 1" -> "jamin:"
						other_connection_type = connection_name.substr(0, connection_name.find(":") + 1);
					}

					if (connection_name.find(other_connection_type) == 0) {
						++other_connection_count;
					}
				}

				++total_connection_count;
				++io_connection_count;
			}
		}

		if (io_connection_count != 1) {
			each_io_has_one_connection = false;
		}
	}

	if (total_connection_count == 0) {
		tooltip << endl << _("Disconnected");
	}

	if (each_io_has_one_connection) {
		if (total_connection_count == ardour_connection_count) {
			// all connections are to the same track in ardour
			// "ardour:Master/" -> "Master"
			string::size_type slash = ardour_track_name.find("/");
			if (slash != string::npos) {
				label << ardour_track_name.substr(7, slash - 7);
				have_label = true;
			}
		}
		else if (total_connection_count == system_connection_count) {
			// all connections are to system ports
			label << system_ports;
			have_label = true;
		}
		else if (total_connection_count == other_connection_count) {
			// all connections are to the same external program eg jamin
			// "jamin:" -> "jamin"
			label << other_connection_type.substr(0, other_connection_type.size() - 1);
			have_label = true;
		}
	}

	if (!have_label) {
		if (total_connection_count == 0) {
			// Disconnected
			label << "-";
		} else {
			// Odd configuration
			label << "*" << total_connection_count << "*";
		}
	}

	label_string = label.str().substr(0, 7);

	//switch (width) {
	//case Wide:
	//	label_string = label.str().substr(0, 7);
	//	break;
	//case Narrow:
	//	label_string = label.str().substr(0, 3);
	//	break;
 // 	}
}

void
MixerStrip::update_input_display ()
{
//	update_io_button (_route, _width, true);
	update_io_button (_route, true);
  	panners.setup_pan ();

	if (has_audio_outputs ()) {
		panners.show_all ();
	} else {
		panners.hide_all ();
	}

}

void
MixerStrip::update_output_display ()
{
	update_io_button (_route, false);
  	gpm.setup_meters ();
  	panners.setup_pan ();

	if (has_audio_outputs ()) {
		panners.show_all ();
	} else {
		panners.hide_all ();
	}
}

void
MixerStrip::fast_update ()
{
	gpm.update_meters ();
}

void
MixerStrip::diskstream_changed ()
{
	Gtkmm2ext::UI::instance()->call_slot (invalidator (*this), boost::bind (&MixerStrip::update_diskstream_display, this));
}

void
MixerStrip::io_changed_proxy ()
{
	Glib::signal_idle().connect_once (sigc::mem_fun (*this, &MixerStrip::update_panner_choices));
}

void
MixerStrip::port_connected_or_disconnected (boost::weak_ptr<Port> wa, boost::weak_ptr<Port> wb)
{
    if (!_route) {
        return;
    }
    
	boost::shared_ptr<Port> a = wa.lock ();
	boost::shared_ptr<Port> b = wb.lock ();

	if ((a && _route->input()->has_port (a)) || (b && _route->input()->has_port (b))) {
		update_input_display ();
		//set_width_enum (_width, this);
	}

	if ((a && _route->output()->has_port (a)) || (b && _route->output()->has_port (b))) {
		update_output_display ();
		//set_width_enum (_width, this);
	}
}

void
MixerStrip::setup_comment_button ()
{
	if (_route->comment().empty ()) {
		_comment_button.unset_bg (STATE_NORMAL);
		_comment_button.set_text (_("Comments"));
	} else {
		_comment_button.modify_bg (STATE_NORMAL, color ());
		_comment_button.set_text (_("*Comments*"));
	}

	ARDOUR_UI::instance()->set_tip (
		_comment_button, _route->comment().empty() ? _("Click to Add/Edit Comments") : _route->comment()
		);
}

void
MixerStrip::comment_editor_done_editing ()
{
	string const str = comment_area->get_buffer()->get_text();
	if (str == _route->comment ()) {
		return;
	}

	_route->set_comment (str, this);
	setup_comment_button ();
}

void
MixerStrip::toggle_comment_editor (WavesButton*)
{
	if (ignore_toggle) {
		return;
	}

	if (comment_window && comment_window->is_visible ()) {
		comment_window->hide ();
	} else {
		open_comment_editor ();
	}
}

void
MixerStrip::open_comment_editor ()
{
	if (comment_window == 0) {
		setup_comment_editor ();
	}

	string title;
	title = _route->name();
	title += _(": comment editor");

	comment_window->set_title (title);
	comment_window->present();
}

void
MixerStrip::setup_comment_editor ()
{
	comment_window = new ArdourWindow (""); // title will be reset to show route
	comment_window->set_skip_taskbar_hint (true);
	comment_window->signal_hide().connect (sigc::mem_fun(*this, &MixerStrip::comment_editor_done_editing));
	comment_window->set_default_size (400, 200);

	comment_area = manage (new TextView());
	comment_area->set_name ("MixerTrackCommentArea");
	comment_area->set_wrap_mode (WRAP_WORD);
	comment_area->set_editable (true);
	comment_area->get_buffer()->set_text (_route->comment());
	comment_area->show ();

	comment_window->add (*comment_area);
}

void
MixerStrip::comment_changed (void *src)
{
	ENSURE_GUI_THREAD (*this, &MixerStrip::comment_changed, src)

	if (src != this) {
		ignore_comment_edit = true;
		if (comment_area) {
			comment_area->get_buffer()->set_text (_route->comment());
		}
		ignore_comment_edit = false;
	}
}

bool
MixerStrip::select_route_group (GdkEventButton *ev)
{
	using namespace Menu_Helpers;

	if (ev->button == 1) {

		if (group_menu == 0) {

			PropertyList* plist = new PropertyList();

			plist->add (Properties::gain, true);
			plist->add (Properties::mute, true);
			plist->add (Properties::solo, true);

			group_menu = new RouteGroupMenu (_session, plist);
		}

		WeakRouteList r;
		r.push_back (route ());
		group_menu->build (r);
		group_menu->menu()->popup (1, ev->time);
	}

	return true;
}

void
MixerStrip::route_group_changed ()
{
	ENSURE_GUI_THREAD (*this, &MixerStrip::route_group_changed)

	RouteGroup *rg = _route->route_group();

	if (rg) {
		group_button.set_text (PBD::short_version (rg->name(), 5));
	}
}

void
MixerStrip::route_color_changed ()
{
	Gdk::Color new_color = color();
	name_button.modify_bg (Gtk::STATE_NORMAL, new_color);
	name_button.modify_bg (Gtk::STATE_ACTIVE, new_color);
	name_button.modify_bg (Gtk::STATE_PRELIGHT, new_color);
	reset_strip_style ();
}

void
MixerStrip::show_passthru_color ()
{
	reset_strip_style ();
}

#if 0 /* not used in tracks */
void
MixerStrip::build_route_ops_menu ()
{
	using namespace Menu_Helpers;
	route_ops_menu = new Menu;
	route_ops_menu->set_name ("ArdourContextMenu");

	MenuList& items = route_ops_menu->items();

	items.push_back (MenuElem (_("Comments..."), sigc::mem_fun (*this, &MixerStrip::open_comment_editor)));
	if (!_route->is_master()) {
		items.push_back (MenuElem (_("Save As Template..."), sigc::mem_fun(*this, &RouteUI::save_as_template)));
	}
	items.push_back (MenuElem (_("Rename..."), sigc::mem_fun(*this, &RouteUI::route_rename)));
	rename_menu_item = &items.back();

	items.push_back (SeparatorElem());
	items.push_back (CheckMenuElem (_("Active")));
	Gtk::CheckMenuItem* i = dynamic_cast<Gtk::CheckMenuItem *> (&items.back());
	i->set_active (_route->active());
	i->set_sensitive(! _session->transport_rolling());
	i->signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &RouteUI::set_route_active), !_route->active(), false));

	items.push_back (SeparatorElem());

	items.push_back (MenuElem (_("Adjust Latency..."), sigc::mem_fun (*this, &RouteUI::adjust_latency)));

	items.push_back (SeparatorElem());
	items.push_back (CheckMenuElem (_("Protect Against Denormals"), sigc::mem_fun (*this, &RouteUI::toggle_denormal_protection)));
	denormal_menu_item = dynamic_cast<Gtk::CheckMenuItem *> (&items.back());
	denormal_menu_item->set_active (_route->denormal_protection());

	if (!Profile->get_sae()) {
		items.push_back (SeparatorElem());
		items.push_back (MenuElem (_("Remote Control ID..."), sigc::mem_fun (*this, &RouteUI::open_remote_control_id_dialog)));
	}

	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Remove"), sigc::bind (sigc::mem_fun(*this, &RouteUI::remove_this_route), false)));
}
#endif

gboolean
MixerStrip::name_button_button_press (GdkEventButton* ev)
{
    if (ev->button != 1)
        return true;

	return false;
}

void
MixerStrip::set_selected (bool yn)
{
	root().set_state (yn ? Gtk::STATE_ACTIVE : Gtk::STATE_NORMAL);
	AxisView::set_selected (yn);
}

void
MixerStrip::property_changed (const PropertyChange& what_changed)
{
	RouteUI::property_changed (what_changed);

	if (what_changed.contains (ARDOUR::Properties::name)) {
		name_changed ();
	}
}

void
MixerStrip::name_changed ()
{
    std::string name;
    if (_route->is_master ()) {
        name = "Master Bus";
    } else {
        name = _route->name ();
    }
	name_button.set_text ( cut_string( name, _max_name_size) );
    _name_entry.set_text ( name );
	ARDOUR_UI::instance()->set_tip (name_button, name);
}

void
MixerStrip::set_embedded (bool yn)
{
	_embedded = yn;
}

void
MixerStrip::map_frozen ()
{
	ENSURE_GUI_THREAD (*this, &MixerStrip::map_frozen)

	boost::shared_ptr<AudioTrack> at = audio_track();

	if (at) {
		switch (at->freeze_state()) {
		case AudioTrack::Frozen:
//			processor_box.set_sensitive (false);
			hide_redirect_editors ();
			break;
		default:
//			processor_box.set_sensitive (true);
			// XXX need some way, maybe, to retoggle redirect editors
			break;
		}
	}
}

void
MixerStrip::hide_redirect_editors ()
{
	_route->foreach_processor (sigc::mem_fun (*this, &MixerStrip::hide_processor_editor));
}

void
MixerStrip::hide_processor_editor (boost::weak_ptr<Processor> p)
{
	boost::shared_ptr<Processor> processor (p.lock ());
	if (!processor) {
		return;
	}

	//Gtk::Window* w = processor_box.get_processor_ui (processor);

	//if (w) {
	//	w->hide ();
	//}
}

void
MixerStrip::reset_strip_style ()
{
	return;
	if (_current_delivery && boost::dynamic_pointer_cast<Send>(_current_delivery)) {

		gpm.set_fader_name ("SendStripBase");

	} else {

		if (is_midi_track()) {
			if (_route->active()) {
				set_name ("MidiTrackStripBase");
			} else {
				set_name ("MidiTrackStripBaseInactive");
			}
			gpm.set_fader_name ("MidiTrackFader");
		} else if (is_audio_track()) {
			if (_route->active()) {
				set_name ("AudioTrackStripBase");
			} else {
				set_name ("AudioTrackStripBaseInactive");
			}
			gpm.set_fader_name ("AudioTrackFader");
		} else {
			if (_route->active()) {
				set_name ("AudioBusStripBase");
			} else {
				set_name ("AudioBusStripBaseInactive");
			}
			gpm.set_fader_name ("AudioBusFader");

			/* (no MIDI busses yet) */
		}
	}
}


void
MixerStrip::engine_stopped ()
{
}

void
MixerStrip::engine_running ()
{
}

string
MixerStrip::meter_point_string (MeterPoint mp)
{
	//switch (_width) {
	//case Wide:
	//	switch (mp) {
	//	case MeterInput:
	//		return _("in");
	//		break;
	//		
	//	case MeterPreFader:
	//		return _("pre");
	//		break;
	//		
	//	case MeterPostFader:
	//		return _("post");
	//		break;
	//		
	//	case MeterOutput:
	//		return _("out");
	//		break;
	//		
	//	case MeterCustom:
	//	default:
	//		return _("custom");
	//		break;
	//	}
	//	break;
	//case Narrow:
	//	switch (mp) {
	//	case MeterInput:
	//		return _("in");
	//		break;
	//		
	//	case MeterPreFader:
	//		return _("pr");
	//		break;
	//		
	//	case MeterPostFader:
	//		return _("po");
	//		break;
	//		
	//	case MeterOutput:
	//		return _("o");
	//		break;
	//		
	//	case MeterCustom:
	//	default:
	//		return _("c");
	//		break;
	//	}
	//	break;
	//}

	switch (mp) {
	case MeterInput:
		return _("in");
		break;
			
	case MeterPreFader:
		return _("pre");
		break;
			
	case MeterPostFader:
		return _("post");
		break;
			
	case MeterOutput:
		return _("out");
		break;
			
	case MeterCustom:
	default:
		return _("custom");
		break;
	}
	return string();
}

/** Called when the metering point has changed */
void
MixerStrip::meter_changed ()
{
	gpm.setup_meters ();
	// reset peak when meter point changes
	gpm.reset_peak_display();
}

/** The bus that we are displaying sends to has changed, or been turned off.
 *  @param send_to New bus that we are displaying sends to, or 0.
 */
void
MixerStrip::bus_send_display_changed (boost::shared_ptr<Route> send_to)
{
	RouteUI::bus_send_display_changed (send_to);

	if (send_to) {
		boost::shared_ptr<Send> send = _route->internal_send_for (send_to);

		if (send) {
			show_send (send);
		} else {
			revert_to_default_display ();
		}
	} else {
		revert_to_default_display ();
	}
}

void
MixerStrip::drop_send ()
{
	boost::shared_ptr<Send> current_send;

	if (_current_delivery && ((current_send = boost::dynamic_pointer_cast<Send>(_current_delivery)) != 0)) {
		current_send->set_metering (false);
	}

	send_gone_connection.disconnect ();
	group_button.set_sensitive (true);
	set_invert_sensitive (true);
	mute_button.set_sensitive (true);
	solo_button.set_sensitive (true);
	rec_enable_button.set_sensitive (true);
	monitor_input_button.set_sensitive (true);
	_comment_button.set_sensitive (true);
}

void
MixerStrip::set_current_delivery (boost::shared_ptr<Delivery> d)
{
	_current_delivery = d;
	DeliveryChanged (_current_delivery);
}

void
MixerStrip::show_send (boost::shared_ptr<Send> send)
{
	assert (send != 0);

	drop_send ();

	set_current_delivery (send);

	send->meter()->set_type(_route->shared_peak_meter()->get_type());
	send->set_metering (true);
	_current_delivery->DropReferences.connect (send_gone_connection, invalidator (*this), boost::bind (&MixerStrip::revert_to_default_display, this), gui_context());

	gain_meter().set_controls (_route, send->meter(), send->amp());
	gain_meter().setup_meters ();
    
	uint32_t const in = _current_delivery->pans_required();
	uint32_t const out = _current_delivery->pan_outs();

    panner_ui().set_panner (boost::shared_ptr<Route>(), _current_delivery->panner_shell(), _current_delivery->panner());
	panner_ui().set_available_panners(PannerManager::instance().PannerManager::get_available_panners(in, out));
	panner_ui().setup_pan ();
	panner_ui().show_all ();

	group_button.set_sensitive (false);
	set_invert_sensitive (false);
	mute_button.set_sensitive (false);
	solo_button.set_sensitive (false);
	rec_enable_button.set_sensitive (false);
	monitor_input_button.set_sensitive (false);
	_comment_button.set_sensitive (false);

	reset_strip_style ();
}

void
MixerStrip::revert_to_default_display ()
{
	drop_send ();

	set_current_delivery (_route->main_outs ());

	gain_meter().set_controls (_route, _route->shared_peak_meter(), _route->amp());
	gain_meter().setup_meters ();

	panner_ui().set_panner (_route, _route->main_outs()->panner_shell(), _route->main_outs()->panner());
	update_panner_choices();
	panner_ui().setup_pan ();

	if (has_audio_outputs ()) {
		panners.show_all ();
	} else {
		panners.hide_all ();
	}

	reset_strip_style ();
}

void
MixerStrip::set_button_names ()
{
	if (_route && _route->solo_safe()) {
		solo_button.set_visual_state (Gtkmm2ext::VisualState (solo_button.visual_state() | Gtkmm2ext::Insensitive));
	} else {
		solo_button.set_visual_state (Gtkmm2ext::VisualState (solo_button.visual_state() & ~Gtkmm2ext::Insensitive));
	}
}

void
MixerStrip::gain_slider_set_visible (bool visibility)
{
   gpm.get_gain_slider().set_visible (visibility);
   gpm.get_gain_slider().set_no_show_all (!visibility);
   gpm.get_gain_display_button().set_visible (visibility);
   gpm.get_gain_display_button().set_no_show_all (!visibility);
}

PluginSelector*
MixerStrip::plugin_selector()
{
	//return _mixer.plugin_selector();
	return 0;
}

void
MixerStrip::hide_things ()
{
//	processor_box.hide_things ();
}

bool
MixerStrip::input_active_button_press (GdkEventButton*)
{
	/* nothing happens on press */
	return true;
}

bool
MixerStrip::input_active_button_release (GdkEventButton* ev)
{
	boost::shared_ptr<MidiTrack> mt = midi_track ();

	if (!mt) {
		return true;
	}

	boost::shared_ptr<RouteList> rl (new RouteList);

	rl->push_back (route());

	_session->set_exclusive_input_active (rl, !mt->input_active(),
					      Keyboard::modifier_state_contains (ev->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::SecondaryModifier)));

	return true;
}

void
MixerStrip::midi_input_status_changed ()
{
	boost::shared_ptr<MidiTrack> mt = midi_track ();
	assert (mt);
	midi_input_enable_button.set_active (mt->input_active ());
}

string
MixerStrip::state_id () const
{
	return string_compose ("strip %1", _route->id().to_s());
}

void
MixerStrip::parameter_changed (string p)
{
	if (p == _visibility.get_state_name()) {
		/* The user has made changes to the mixer strip visibility, so get
		   our VisibilityGroup to reflect these changes in our widgets.
		*/
		_visibility.set_state (Config->get_mixer_strip_visibility ());
	}
}

/** Called to decide whether the solo isolate / solo lock button visibility should
 *  be overridden from that configured by the user.  We do this for the master bus.
 *
 *  @return optional value that is present if visibility state should be overridden.
 */
boost::optional<bool>
MixerStrip::override_solo_visibility () const
{
	if (_route && _route->is_master ()) {
		return boost::optional<bool> (false);
	}
	
	return boost::optional<bool> ();
}

void
MixerStrip::add_input_port (DataType t)
{
	_route->input()->add_port ("", this, t);
}

void
MixerStrip::add_output_port (DataType t)
{
	_route->output()->add_port ("", this, t);
}

void
MixerStrip::route_active_changed ()
{
	reset_strip_style ();
}

void
MixerStrip::copy_processors ()
{
//	processor_box.processor_operation (ProcessorBox::ProcessorsCopy);
}

void
MixerStrip::cut_processors ()
{
//	processor_box.processor_operation (ProcessorBox::ProcessorsCut);
}

void
MixerStrip::paste_processors ()
{
//	processor_box.processor_operation (ProcessorBox::ProcessorsPaste);
}

void
MixerStrip::select_all_processors ()
{
//	processor_box.processor_operation (ProcessorBox::ProcessorsSelectAll);
}

void
MixerStrip::deselect_all_processors ()
{
//	processor_box.processor_operation (ProcessorBox::ProcessorsSelectNone);
}

bool
MixerStrip::delete_processors ()
{
	return false;
//	return processor_box.processor_operation (ProcessorBox::ProcessorsDelete);
}

void
MixerStrip::toggle_processors ()
{
//	processor_box.processor_operation (ProcessorBox::ProcessorsToggleActive);
}

void
MixerStrip::ab_plugins ()
{
//	processor_box.processor_operation (ProcessorBox::ProcessorsAB);
}

void
MixerStrip::add_level_meter_item_point (Menu_Helpers::MenuList& items,
		RadioMenuItem::Group& group, string const & name, MeterPoint point)
{
	using namespace Menu_Helpers;
	
	items.push_back (RadioMenuElem (group, name, sigc::bind (sigc::mem_fun (*this, &MixerStrip::set_meter_point), point)));
	RadioMenuItem* i = dynamic_cast<RadioMenuItem *> (&items.back ());
	i->set_active (_route->meter_point() == point);
}

void
MixerStrip::set_meter_point (MeterPoint p)
{
	if (_suspend_menu_callbacks) return;
	_route->set_meter_point (p);
}

void
MixerStrip::add_level_meter_item_type (Menu_Helpers::MenuList& items,
		RadioMenuItem::Group& group, string const & name, MeterType type)
{
	using namespace Menu_Helpers;
	
	items.push_back (RadioMenuElem (group, name, sigc::bind (sigc::mem_fun (*this, &MixerStrip::set_meter_type), type)));
	RadioMenuItem* i = dynamic_cast<RadioMenuItem *> (&items.back ());
	i->set_active (_route->meter_type() == type);
}

void
MixerStrip::set_meter_type (MeterType t)
{
	if (_suspend_menu_callbacks) return;
	gpm.set_type (t);
}
