/*
    Copyright (C) 2002-2006 Paul Davis 

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

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/stop_signal.h>
#include <gtkmm2ext/choice.h>
#include <gtkmm2ext/doi.h>
#include <gtkmm2ext/bindable_button.h>

#include <ardour/route_group.h>
#include <pbd/memento_command.h>
#include <pbd/stacktrace.h>
#include <pbd/shiva.h>

#include "route_ui.h"
#include "keyboard.h"
#include "utils.h"
#include "prompter.h"
#include "gui_thread.h"

#include <ardour/route.h>
#include <ardour/session.h>
#include <ardour/audioengine.h>
#include <ardour/audio_track.h>
#include <ardour/audio_diskstream.h>

#include "i18n.h"
using namespace sigc;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR;
using namespace PBD;

RouteUI::RouteUI (ARDOUR::Session& sess, const char* mute_name, const char* solo_name, const char* rec_name)
	: AxisView(sess)
{
	init ();
	set_button_names (mute_name, solo_name, rec_name);
}

RouteUI::RouteUI (boost::shared_ptr<ARDOUR::Route> rt, 
		  ARDOUR::Session& sess, const char* mute_name, const char* solo_name, const char* rec_name)
	: AxisView(sess)
{
	init ();
	set_button_names (mute_name, solo_name, rec_name);
	set_route (rt);
}

void
RouteUI::init ()
{
	xml_node = 0;
	mute_menu = 0;
	solo_menu = 0;
	remote_control_menu = 0;
	ignore_toggle = false;
	wait_for_release = false;
	route_active_menu_item = 0;
	was_solo_safe = false;
	polarity_menu_item = 0;
	denormal_menu_item = 0;

	mute_button = manage (new BindableToggleButton (0, ""));
	mute_button->set_self_managed (true);
	mute_button->set_name ("MuteButton");

	solo_button = manage (new BindableToggleButton (0, ""));
	solo_button->set_self_managed (true);
	solo_button->set_name ("SoloButton");

	rec_enable_button = manage (new BindableToggleButton (0, ""));
	rec_enable_button->set_name ("RecordEnableButton");
	rec_enable_button->set_self_managed (true);

	_session.SoloChanged.connect (mem_fun(*this, &RouteUI::solo_changed_so_update_mute));
}

void
RouteUI::reset ()
{
	connections.clear ();

	if (solo_menu) {
		delete solo_menu;
		solo_menu = 0;
	}

	if (mute_menu) {
		delete mute_menu;
		mute_menu = 0;
	}
	
	if (remote_control_menu) {
		delete remote_control_menu;
		remote_control_menu = 0;
	}

	if (xml_node) {
		/* do not delete the node - its owned by the route */
		xml_node = 0;
	}

	route_active_menu_item = 0;
	polarity_menu_item = 0;
	denormal_menu_item = 0;
}

void
RouteUI::set_button_names (const char* mute, const char* solo, const char* rec)
{
	m_name = mute;
	s_name = solo;
	r_name = rec;
}

void
RouteUI::set_route (boost::shared_ptr<Route> rp)
{
	reset ();

	_route = rp;

	if (set_color_from_route()) {
		set_color (unique_random_color());
	}

	/* no, there is no memory leak here. This object cleans itself (and other stuff)
	   up when the route is destroyed.
	*/

	new PairedShiva<Route,RouteUI> (*_route, *this);

	mute_button->set_controllable (&_route->mute_control());
	mute_button->set_label (m_name);
	
	solo_button->set_controllable (&_route->solo_control());
	solo_button->set_label (s_name);

	connections.push_back (_route->active_changed.connect (mem_fun (*this, &RouteUI::route_active_changed)));
	connections.push_back (_route->mute_changed.connect (mem_fun(*this, &RouteUI::mute_changed)));
	connections.push_back (_route->solo_changed.connect (mem_fun(*this, &RouteUI::solo_changed)));
	connections.push_back (_route->solo_safe_changed.connect (mem_fun(*this, &RouteUI::solo_changed)));

	/* when solo changes, update mute state too, in case the user wants us to display it */

	update_solo_display ();
	update_mute_display ();

	if (is_track()) {
		boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track>(_route);

		connections.push_back (t->diskstream()->RecordEnableChanged.connect (mem_fun (*this, &RouteUI::route_rec_enable_changed)));

		connections.push_back (_session.RecordStateChanged.connect (mem_fun (*this, &RouteUI::session_rec_enable_changed)));

		rec_enable_button->set_controllable (&t->rec_enable_control());
		rec_enable_button->set_label (r_name);

		update_rec_display ();
	} 
	
	connections.push_back (_route->RemoteControlIDChanged.connect (mem_fun(*this, &RouteUI::refresh_remote_control_menu)));

	/* map the current state */

	map_frozen ();
}

RouteUI::~RouteUI()
{
	GoingAway (); /* EMIT SIGNAL */
	if (solo_menu) {
		delete solo_menu;
	}

	if (mute_menu) {
		delete mute_menu;
	}

	if (remote_control_menu) {
		delete remote_control_menu;
	}
}

bool
RouteUI::mute_press(GdkEventButton* ev)
{
	if (ev->type == GDK_2BUTTON_PRESS) {
		return true;
	}

	if (!ignore_toggle) {

		if (Keyboard::is_context_menu_event (ev)) {

			if (mute_menu == 0){
				build_mute_menu();
			}

			mute_menu->popup(0,ev->time);

		} else {

			if (ev->button == 2) {
				// Primary-button2 click is the midi binding click
				// button2-click is "momentary"
				
				if (!Keyboard::modifier_state_equals (ev->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier))) {
					wait_for_release = true;
				} else {
					return false;
				}
			}

			if (ev->button == 1 || ev->button == 2) {

				if (Keyboard::modifier_state_equals (ev->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::TertiaryModifier))) {

					/* Primary-Tertiary-click applies change to all routes */

					_session.begin_reversible_command (_("mute change"));
                                        Session::GlobalMuteStateCommand *cmd = new Session::GlobalMuteStateCommand(_session, this);
					_session.set_all_mute (!_route->muted());
                                        cmd->mark();
					_session.add_command(cmd);
					_session.commit_reversible_command ();

				} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {

					/* Primary-button1 applies change to the mix group.
					   NOTE: Primary-button2 is MIDI learn.
					*/

					if (ev->button == 1) {
						set_mix_group_mute (_route, !_route->muted());
					}
					
				} else {

					/* plain click applies change to this route */

					reversibly_apply_route_boolean ("mute change", &Route::set_mute, !_route->muted(), this);
				}
			}
		}

	}

	return true;
}

bool
RouteUI::mute_release(GdkEventButton* ev)
{
	if (!ignore_toggle) {
		if (wait_for_release){
			wait_for_release = false;
			// undo the last op
			// because the press was the last undoable thing we did
			_session.undo (1U);
		}
	}
	return true;
}

bool
RouteUI::solo_press(GdkEventButton* ev)
{
	/* ignore double clicks */

	if (ev->type == GDK_2BUTTON_PRESS) {
		return true;
	}

	if (!ignore_toggle) {

		if (Keyboard::is_context_menu_event (ev)) {
			
			if (solo_menu == 0) {
				build_solo_menu ();
			}

			solo_menu->popup (1, ev->time);

		} else {

			if (ev->button == 2) {

				// Primary-button2 click is the midi binding click
				// button2-click is "momentary"
				
				if (!Keyboard::modifier_state_equals (ev->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier))) {
					wait_for_release = true;
				} else {
					return false;
				}
			}

			if (ev->button == 1 || ev->button == 2) {

				if (Keyboard::modifier_state_equals (ev->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::TertiaryModifier))) {

					/* Primary-Tertiary-click applies change to all routes */

					_session.begin_reversible_command (_("solo change"));
                                        Session::GlobalSoloStateCommand *cmd = new Session::GlobalSoloStateCommand(_session, this);
					_session.set_all_solo (!_route->soloed());
                                        cmd->mark();
					_session.add_command (cmd);
					_session.commit_reversible_command ();
					
				} else if (Keyboard::modifier_state_contains (ev->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::SecondaryModifier))) {

					// Primary-Secondary-click: exclusively solo this track, not a toggle */

					_session.begin_reversible_command (_("solo change"));
                                        Session::GlobalSoloStateCommand *cmd = new Session::GlobalSoloStateCommand (_session, this);
					_session.set_all_solo (false);
					_route->set_solo (true, this);
                                        cmd->mark();
					_session.add_command(cmd);
					_session.commit_reversible_command ();

				} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)) {

					// shift-click: set this route to solo safe

					_route->set_solo_safe (!_route->solo_safe(), this);
					wait_for_release = false;

				} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {

					/* Primary-button1: solo mix group.
					   NOTE: Primary-button2 is MIDI learn.
					*/

					if (ev->button == 1) {
						set_mix_group_solo (_route, !_route->soloed());
					}

				} else {

					/* click: solo this route */
					reversibly_apply_route_boolean ("solo change", &Route::set_solo, !_route->soloed(), this);
				}
			}
		}
	}

	return true;
}

bool
RouteUI::solo_release(GdkEventButton* ev)
{
	if (!ignore_toggle) {
		if (wait_for_release) {
			wait_for_release = false;
			// undo the last op
			// because the press was the last undoable thing we did

			_session.undo (1U);
		}
	}

	return true;
}

bool
RouteUI::rec_enable_press(GdkEventButton* ev)
{
	if (ev->type == GDK_2BUTTON_PRESS) {
		return true;
	}

	if (!_session.engine().connected()) {
	        MessageDialog msg (_("Not connected to JACK - cannot engage record"));
		msg.run ();
		return true;
	}

	if (!ignore_toggle && is_track() && rec_enable_button) {

		if (ev->button == 2 && Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {

			// do nothing on midi bind event
			return false;

		} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::TertiaryModifier))) {

			_session.begin_reversible_command (_("rec-enable change"));
                        Session::GlobalRecordEnableStateCommand *cmd = new Session::GlobalRecordEnableStateCommand(_session, this);

			if (rec_enable_button->get_active()) {
				_session.record_disenable_all ();
			} else {
				_session.record_enable_all ();
			}

                        cmd->mark();
			_session.add_command(cmd);
			_session.commit_reversible_command ();

		} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {

			/* Primary-button1 applies change to the mix group.
			   NOTE: Primary-button2 is MIDI learn.
			*/

			set_mix_group_rec_enable (_route, !_route->record_enabled());

		} else {

			reversibly_apply_audio_track_boolean ("rec-enable change", &AudioTrack::set_record_enable, !audio_track()->record_enabled(), this);
		}
	}

	return true;
}

bool
RouteUI::rec_enable_release (GdkEventButton* ev)
{
	return true;
}

void
RouteUI::solo_changed(void* src)
{
	Gtkmm2ext::UI::instance()->call_slot (mem_fun (*this, &RouteUI::update_solo_display));
}

void
RouteUI::update_solo_display ()
{
	bool x;
	vector<Gdk::Color> fg_colors;
	Gdk::Color c;
	
	if (solo_button->get_active() != (x = _route->soloed())){
		ignore_toggle = true;
		solo_button->set_active(x);
		ignore_toggle = false;
	} 
	
	if (_route->solo_safe()) {
		solo_button->set_visual_state (2);
	} else if (_route->soloed()) {
		solo_button->set_visual_state (1);
	} else {
		solo_button->set_visual_state (0);
	}
}

void
RouteUI::solo_changed_so_update_mute ()
{
	Gtkmm2ext::UI::instance()->call_slot (mem_fun (*this, &RouteUI::update_mute_display));
}

void
RouteUI::mute_changed(void* src)
{
	Gtkmm2ext::UI::instance()->call_slot (mem_fun (*this, &RouteUI::update_mute_display));
}

void
RouteUI::update_mute_display ()
{
	bool model = _route->muted();
	bool view = mute_button->get_active();

	/* first make sure the button's "depressed" visual
	   is correct.
	*/

	if (model != view) {
		ignore_toggle = true;
		mute_button->set_active (model);
		ignore_toggle = false;
	}

	/* now attend to visual state */
	
	if (Config->get_show_solo_mutes()) {
		if (_route->muted()) {
			mute_button->set_visual_state (2);
		} else if (!_route->soloed() && _route->solo_muted()) {
			
			mute_button->set_visual_state (1);
		} else {
			mute_button->set_visual_state (0);
		}
	} else {
		if (_route->muted()) {
			mute_button->set_visual_state (2);
		} else {
			mute_button->set_visual_state (0);
		}
	}

}

void
RouteUI::route_rec_enable_changed ()
{
	Gtkmm2ext::UI::instance()->call_slot (mem_fun (*this, &RouteUI::update_rec_display));
}

void
RouteUI::session_rec_enable_changed ()
{
	Gtkmm2ext::UI::instance()->call_slot (mem_fun (*this, &RouteUI::update_rec_display));
}

void
RouteUI::update_rec_display ()
{
	bool model = _route->record_enabled();
	bool view = rec_enable_button->get_active();

	/* first make sure the button's "depressed" visual
	   is correct.
	*/

	if (model != view) {
		ignore_toggle = true;
		rec_enable_button->set_active (model);
		ignore_toggle = false;
	}

	/* now make sure its color state is correct */

	if (model) {

		switch (_session.record_status ()) {
		case Session::Recording:
			rec_enable_button->set_visual_state (1);
			break;

		case Session::Disabled:
		case Session::Enabled:
			rec_enable_button->set_visual_state (2);
			break;

		}

	} else {
		rec_enable_button->set_visual_state (0);
	}
}

void
RouteUI::build_remote_control_menu ()
{
	remote_control_menu = new Menu;
	refresh_remote_control_menu ();
}

void
RouteUI::refresh_remote_control_menu ()
{
	ENSURE_GUI_THREAD (mem_fun (*this, &RouteUI::refresh_remote_control_menu));

	// only refresh the menu if it has been instantiated

	if (remote_control_menu == 0) {
		return;
	}

	using namespace Menu_Helpers;

	RadioMenuItem::Group rc_group;
	CheckMenuItem* rc_active;
	uint32_t limit = _session.ntracks() + _session.nbusses();
	char buf[32];

	MenuList& rc_items = remote_control_menu->items();
	rc_items.clear ();

	/* note that this menu list starts at zero, not 1, because zero
	   is a valid, if useless, ID.
	*/

	limit += 4; /* leave some breathing room */
	
	rc_items.push_back (RadioMenuElem (rc_group, _("None")));
	if (_route->remote_control_id() == 0) {
		rc_active = dynamic_cast<CheckMenuItem*> (&rc_items.back());
		rc_active->set_active ();
	}
		
	for (uint32_t i = 1; i < limit; ++i) {
		snprintf (buf, sizeof (buf), "%u", i);
		rc_items.push_back (RadioMenuElem (rc_group, buf));
		rc_active = dynamic_cast<RadioMenuItem*>(&rc_items.back());
		if (_route->remote_control_id() == i) {
			rc_active = dynamic_cast<CheckMenuItem*> (&rc_items.back());
			rc_active->set_active ();
		}
		rc_active->signal_activate().connect (bind (mem_fun (*this, &RouteUI::set_remote_control_id), i, rc_active));
	}
}

void
RouteUI::set_remote_control_id (uint32_t id, CheckMenuItem* item)
{
	/* this is called when the radio menu item is toggled, and so 
	   is actually invoked twice per menu selection. we only
	   care about the invocation for the item that was being
	   marked active.
	*/

	if (item->get_active()) {
		_route->set_remote_control_id (id);
	}
}

void
RouteUI::build_solo_menu (void)
{
	using namespace Menu_Helpers;
	
	solo_menu = new Menu;
	solo_menu->set_name ("ArdourContextMenu");
	MenuList& items = solo_menu->items();
	CheckMenuItem* check;

	check = new CheckMenuItem(_("Solo-safe"));
	check->set_active (_route->solo_safe());
	check->signal_toggled().connect (bind (mem_fun (*this, &RouteUI::toggle_solo_safe), check));
	_route->solo_safe_changed.connect(bind (mem_fun (*this, &RouteUI::solo_safe_toggle), check));
	items.push_back (CheckMenuElem(*check));
	check->show_all();

	//items.push_back (SeparatorElem());
	// items.push_back (MenuElem (_("MIDI Bind"), mem_fun (*mute_button, &BindableToggleButton::midi_learn)));
	
}

void
RouteUI::build_mute_menu(void)
{
	using namespace Menu_Helpers;
	
	mute_menu = new Menu;
	mute_menu->set_name ("ArdourContextMenu");
	MenuList& items = mute_menu->items();
	CheckMenuItem* check;
	
	check = new CheckMenuItem(_("Pre Fader"));
	init_mute_menu(PRE_FADER, check);
	check->signal_toggled().connect(bind (mem_fun (*this, &RouteUI::toggle_mute_menu), PRE_FADER, check));
	_route->pre_fader_changed.connect(bind (mem_fun (*this, &RouteUI::pre_fader_toggle), check));
	items.push_back (CheckMenuElem(*check));
	check->show_all();

	check = new CheckMenuItem(_("Post Fader"));
	init_mute_menu(POST_FADER, check);
	check->signal_toggled().connect(bind (mem_fun (*this, &RouteUI::toggle_mute_menu), POST_FADER, check));
	_route->post_fader_changed.connect(bind (mem_fun (*this, &RouteUI::post_fader_toggle), check));
	items.push_back (CheckMenuElem(*check));
	check->show_all();
	
	check = new CheckMenuItem(_("Control Outs"));
	init_mute_menu(CONTROL_OUTS, check);
	check->signal_toggled().connect(bind (mem_fun (*this, &RouteUI::toggle_mute_menu), CONTROL_OUTS, check));
	_route->control_outs_changed.connect(bind (mem_fun (*this, &RouteUI::control_outs_toggle), check));
	items.push_back (CheckMenuElem(*check));
	check->show_all();

	check = new CheckMenuItem(_("Main Outs"));
	init_mute_menu(MAIN_OUTS, check);
	check->signal_toggled().connect(bind (mem_fun (*this, &RouteUI::toggle_mute_menu), MAIN_OUTS, check));
	_route->main_outs_changed.connect(bind (mem_fun (*this, &RouteUI::main_outs_toggle), check));
	items.push_back (CheckMenuElem(*check));
	check->show_all();

	//items.push_back (SeparatorElem());
	// items.push_back (MenuElem (_("MIDI Bind"), mem_fun (*mute_button, &BindableToggleButton::midi_learn)));
}

void
RouteUI::init_mute_menu(mute_type type, CheckMenuItem* check)
{
	if (_route->get_mute_config (type)) {
		check->set_active (true);
	}
}

void
RouteUI::toggle_mute_menu(mute_type type, Gtk::CheckMenuItem* check)
{
	_route->set_mute_config(type, check->get_active(), this);
}

void
RouteUI::toggle_solo_safe (Gtk::CheckMenuItem* check)
{
	_route->set_solo_safe (check->get_active(), this);
}

void
RouteUI::set_mix_group_solo(boost::shared_ptr<Route> route, bool yn)
{
	RouteGroup* mix_group;

	if((mix_group = route->mix_group()) != 0){
		_session.begin_reversible_command (_("mix group solo  change"));
                Session::GlobalSoloStateCommand *cmd = new Session::GlobalSoloStateCommand(_session, this);
		mix_group->apply(&Route::set_solo, yn, this);
                cmd->mark();
		_session.add_command (cmd);
		_session.commit_reversible_command ();
	} else {
		reversibly_apply_route_boolean ("solo change", &Route::set_solo, !route->soloed(), this);
	}
}

void
RouteUI::reversibly_apply_route_boolean (string name, void (Route::*func)(bool, void *), bool yn, void *arg)
{
	_session.begin_reversible_command (name);
        XMLNode &before = _route->get_state();
        bind(mem_fun(*_route, func), yn, arg)();
        XMLNode &after = _route->get_state();
        _session.add_command (new MementoCommand<Route>(*_route, &before, &after));
	_session.commit_reversible_command ();
}

void
RouteUI::reversibly_apply_audio_track_boolean (string name, void (AudioTrack::*func)(bool, void *), bool yn, void *arg)
{
	_session.begin_reversible_command (name);
        XMLNode &before = audio_track()->get_state();
	bind (mem_fun (*audio_track(), func), yn, arg)();
        XMLNode &after = audio_track()->get_state();
	_session.add_command (new MementoCommand<AudioTrack>(*audio_track(), &before, &after));
	_session.commit_reversible_command ();
}

void
RouteUI::set_mix_group_mute(boost::shared_ptr<Route> route, bool yn)
{
	RouteGroup* mix_group;

	if((mix_group = route->mix_group()) != 0){
		_session.begin_reversible_command (_("mix group mute change"));
                Session::GlobalMuteStateCommand *cmd = new Session::GlobalMuteStateCommand (_session, this);
		mix_group->apply(&Route::set_mute, yn, this);
                cmd->mark();
		_session.add_command(cmd);
		_session.commit_reversible_command ();
	} else {
		reversibly_apply_route_boolean ("mute change", &Route::set_mute, !route->muted(), this);
	}
}

void
RouteUI::set_mix_group_rec_enable(boost::shared_ptr<Route> route, bool yn)
{
	RouteGroup* mix_group;

	if((mix_group = route->mix_group()) != 0){
		_session.begin_reversible_command (_("mix group rec-enable change"));
                Session::GlobalRecordEnableStateCommand *cmd = new Session::GlobalRecordEnableStateCommand(_session, this);
		mix_group->apply (&Route::set_record_enable, yn, this);
                cmd->mark();
		_session.add_command(cmd);
		_session.commit_reversible_command ();
	} else {
		reversibly_apply_route_boolean ("rec-enable change", &Route::set_record_enable, !_route->record_enabled(), this);
	}
}


bool
RouteUI::choose_color()
{
	bool picked;
	Gdk::Color color;

	color = Gtkmm2ext::UI::instance()->get_color (_("ardour: color selection"), picked, &_color);

	if (picked) {
		set_color (color);
	}

	return picked;
}

void
RouteUI::set_color (const Gdk::Color & c)
{
	char buf[64];
	
	_color = c;
	
	ensure_xml_node ();
	snprintf (buf, sizeof (buf), "%d:%d:%d", c.get_red(), c.get_green(), c.get_blue());
	xml_node->add_property ("color", buf);

	_route->gui_changed ("color", (void *) 0); /* EMIT_SIGNAL */
}


void
RouteUI::ensure_xml_node ()
{
	if (xml_node == 0) {
		if ((xml_node = _route->extra_xml ("GUI")) == 0) {
			xml_node = new XMLNode ("GUI");
			_route->add_extra_xml (*xml_node);
		}
	}
}

XMLNode*
RouteUI::get_child_xml_node (const string & childname)
{
	XMLNode* child;

	ensure_xml_node ();
	
	
	if ((child = find_named_node (*xml_node, childname)) == 0) {
		child = new XMLNode (childname);
		xml_node->add_child_nocopy (*child);
	}

	return child;
}

int
RouteUI::set_color_from_route ()
{
	XMLProperty *prop;
	
	RouteUI::ensure_xml_node ();

	if ((prop = xml_node->property ("color")) != 0) {
		int r, g, b;
		sscanf (prop->value().c_str(), "%d:%d:%d", &r, &g, &b);
		_color.set_red(r);
		_color.set_green(g);
		_color.set_blue(b);
		return 0;
	} 
	return 1;
}

void
RouteUI::remove_this_route ()
{
	vector<string> choices;
	string prompt;

	if (is_track()) {
		prompt  = string_compose (_("Do you really want to remove track \"%1\" ?\n\nYou may also lose the playlist used by this track.\n(cannot be undone)"), _route->name());
	} else {
		prompt  = string_compose (_("Do you really want to remove bus \"%1\" ?\n(cannot be undone)"), _route->name());
	}

	choices.push_back (_("No, do nothing."));
	choices.push_back (_("Yes, remove it."));

	Choice prompter (prompt, choices);

	if (prompter.run () == 1) {
		Glib::signal_idle().connect (bind (sigc::ptr_fun (&RouteUI::idle_remove_this_route), this));
	}
}

gint
RouteUI::idle_remove_this_route (RouteUI *rui)
{
	rui->_session.remove_route (rui->_route);
	return FALSE;
}

void
RouteUI::route_rename ()
{
	ArdourPrompter name_prompter (true);
	string result;
	name_prompter.set_prompt (_("New Name: "));
	name_prompter.set_initial_text (_route->name());
	name_prompter.add_button (_("Rename"), Gtk::RESPONSE_ACCEPT);
	name_prompter.set_response_sensitive (Gtk::RESPONSE_ACCEPT, false);
	name_prompter.show_all ();

	switch (name_prompter.run ()) {

	case Gtk::RESPONSE_ACCEPT:
        name_prompter.get_result (result);
        if (result.length()) {
			_route->set_name (result, this);
		}	
		break;
	}

	return;
  
}

void
RouteUI::name_changed (void *src)
{
	ENSURE_GUI_THREAD(bind (mem_fun (*this, &RouteUI::name_changed), src));

	name_label.set_text (_route->name());
}

void
RouteUI::toggle_route_active ()
{
	bool yn;

	if (route_active_menu_item) {
		if (route_active_menu_item->get_active() != (yn = _route->active())) {
			_route->set_active (!yn);
		}
	}
}

void
RouteUI::route_active_changed ()
{
	if (route_active_menu_item) {
		Gtkmm2ext::UI::instance()->call_slot (bind (mem_fun (*route_active_menu_item, &CheckMenuItem::set_active), _route->active()));
	}
}

void
RouteUI::toggle_polarity ()
{
	if (polarity_menu_item) {

		bool x;

		ENSURE_GUI_THREAD(mem_fun (*this, &RouteUI::toggle_polarity));
		
		if ((x = polarity_menu_item->get_active()) != _route->phase_invert()) {
			_route->set_phase_invert (x, this);
			if (x) {
			        name_label.set_text (X_("Ø ") + name_label.get_text());
			} else {
			        name_label.set_text (_route->name());
			}
		}
	}
}

void
RouteUI::polarity_changed ()
{
	/* no signal for this yet */
}

void
RouteUI::toggle_denormal_protection ()
{
	if (denormal_menu_item) {

		bool x;

		ENSURE_GUI_THREAD(mem_fun (*this, &RouteUI::toggle_denormal_protection));
		
		if ((x = denormal_menu_item->get_active()) != _route->denormal_protection()) {
			_route->set_denormal_protection (x, this);
		}
	}
}

void
RouteUI::denormal_protection_changed ()
{
	/* no signal for this yet */
}


void
RouteUI::solo_safe_toggle(void* src, Gtk::CheckMenuItem* check)
{
	bool yn = _route->solo_safe ();

	if (check->get_active() != yn) {
		check->set_active (yn);
	}
}
void
RouteUI::pre_fader_toggle(void* src, Gtk::CheckMenuItem* check)
{
	ENSURE_GUI_THREAD(bind (mem_fun (*this, &RouteUI::pre_fader_toggle), src, check));
	
	bool yn = _route->get_mute_config(PRE_FADER);
	if (check->get_active() != yn) {
		check->set_active (yn);
	}
}

void
RouteUI::post_fader_toggle(void* src, Gtk::CheckMenuItem* check)
{
	ENSURE_GUI_THREAD(bind (mem_fun (*this, &RouteUI::post_fader_toggle), src, check));
	
	bool yn = _route->get_mute_config(POST_FADER);
	if (check->get_active() != yn) {
		check->set_active (yn);
	}
}

void
RouteUI::control_outs_toggle(void* src, Gtk::CheckMenuItem* check)
{
	ENSURE_GUI_THREAD(bind (mem_fun (*this, &RouteUI::control_outs_toggle), src, check));
	
	bool yn = _route->get_mute_config(CONTROL_OUTS);
	if (check->get_active() != yn) {
		check->set_active (yn);
	}
}

void
RouteUI::main_outs_toggle(void* src, Gtk::CheckMenuItem* check)
{
	ENSURE_GUI_THREAD(bind (mem_fun (*this, &RouteUI::main_outs_toggle), src, check));
	
	bool yn = _route->get_mute_config(MAIN_OUTS);
	if (check->get_active() != yn) {
		check->set_active (yn);
	}
}

void
RouteUI::disconnect_input ()
{
	_route->disconnect_inputs (this);
}

void
RouteUI::disconnect_output ()
{
	_route->disconnect_outputs (this);
}

bool
RouteUI::is_track () const
{
	return boost::dynamic_pointer_cast<Track>(_route) != 0;
}

boost::shared_ptr<Track>
RouteUI::track() const
{
	return boost::dynamic_pointer_cast<Track>(_route);
}

bool
RouteUI::is_audio_track () const
{
	return boost::dynamic_pointer_cast<AudioTrack>(_route) != 0;
}

boost::shared_ptr<AudioTrack>
RouteUI::audio_track() const
{
	return boost::dynamic_pointer_cast<AudioTrack>(_route);
}

boost::shared_ptr<Diskstream>
RouteUI::get_diskstream () const
{
	boost::shared_ptr<Track> t;

	if ((t = boost::dynamic_pointer_cast<Track>(_route)) != 0) {
		return t->diskstream();
	} else {
		return boost::shared_ptr<Diskstream> ((Diskstream*) 0);
	}
}

string
RouteUI::name() const
{
	return _route->name();
}

void
RouteUI::map_frozen ()
{
	ENSURE_GUI_THREAD (mem_fun (*this, &RouteUI::map_frozen));

 	AudioTrack* at = dynamic_cast<AudioTrack*>(_route.get());

	if (at) {
		switch (at->freeze_state()) {
		case AudioTrack::Frozen:
			rec_enable_button->set_sensitive (false);
			break;
		default:
			rec_enable_button->set_sensitive (true);
			break;
		}
	}
}

