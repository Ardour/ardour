/*
 * Copyright (C) 2021 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <gtkmm/alignment.h>
#include <gtkmm/filechooserdialog.h>
#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/stock.h>

#include "pbd/compose.h"
#include "pbd/convert.h"

#include "pbd/basename.h"
#include "pbd/file_utils.h"
#include "pbd/pathexpand.h"
#include "pbd/search_path.h"

#include "ardour/directory_names.h"
#include "ardour/filesystem_paths.h"
#include "ardour/region.h"
#include "ardour/triggerbox.h"

#include "slot_properties_box.h"

#include "ardour_ui.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "trigger_ui.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ArdourWidgets;
using namespace Gtkmm2ext;
using namespace PBD;
using namespace Temporal;

std::vector<std::string> TriggerUI::follow_strings;
std::string              TriggerUI::longest_follow;
std::vector<std::string> TriggerUI::quantize_strings;
std::string              TriggerUI::longest_quantize;
std::vector<std::string> TriggerUI::launch_strings;
std::string              TriggerUI::longest_launch;

Gtkmm2ext::Bindings*           TriggerUI::bindings = 0;
Glib::RefPtr<Gtk::ActionGroup> TriggerUI::trigger_actions;

TriggerUI::TriggerUI ()
	: _renaming (false)
{
	if (follow_strings.empty()) {
		follow_strings.push_back (follow_action_to_string (Trigger::None));
		follow_strings.push_back (follow_action_to_string (Trigger::Stop));
		follow_strings.push_back (follow_action_to_string (Trigger::Again));
		follow_strings.push_back (follow_action_to_string (Trigger::QueuedTrigger));
		follow_strings.push_back (follow_action_to_string (Trigger::NextTrigger));
		follow_strings.push_back (follow_action_to_string (Trigger::PrevTrigger));
		follow_strings.push_back (follow_action_to_string (Trigger::FirstTrigger));
		follow_strings.push_back (follow_action_to_string (Trigger::LastTrigger));
		follow_strings.push_back (follow_action_to_string (Trigger::AnyTrigger));
		follow_strings.push_back (follow_action_to_string (Trigger::OtherTrigger));

		for (std::vector<std::string>::const_iterator i = follow_strings.begin(); i != follow_strings.end(); ++i) {
			if (i->length() > longest_follow.length()) {
				longest_follow = *i;
			}
		}

		launch_strings.push_back (launch_style_to_string (Trigger::OneShot));
		launch_strings.push_back (launch_style_to_string (Trigger::Gate));
		launch_strings.push_back (launch_style_to_string (Trigger::Toggle));
		launch_strings.push_back (launch_style_to_string (Trigger::Repeat));

		for (std::vector<std::string>::const_iterator i = launch_strings.begin(); i != launch_strings.end(); ++i) {
			if (i->length() > longest_launch.length()) {
				longest_launch = *i;
			}
		}
	}
}

TriggerUI::~TriggerUI()
{
}

void
TriggerUI::setup_actions_and_bindings ()
{
	load_bindings ();
	register_actions ();
}

void
TriggerUI::load_bindings ()
{
	bindings = Bindings::get_bindings (X_("Triggers"));
}

void
TriggerUI::register_actions ()
{
	trigger_actions = ActionManager::create_action_group (bindings, X_("Triggers"));

	for (int32_t n = 0; n < TriggerBox::default_triggers_per_box; ++n) {
		const std::string action_name  = string_compose ("trigger-scene-%1", n);
		const std::string display_name = string_compose (_("Scene %1"), n);

		ActionManager::register_toggle_action (trigger_actions, action_name.c_str (), display_name.c_str (), sigc::bind (sigc::ptr_fun (TriggerUI::trigger_scene), n));
	}
}

void
TriggerUI::trigger_scene (int32_t n)
{
	TriggerBox::scene_bang (n);
}

void
TriggerUI::choose_color ()
{
	_color_dialog.get_colorsel()->set_has_opacity_control (false);
	_color_dialog.get_colorsel()->set_has_palette (true);
	_color_dialog.get_ok_button()->signal_clicked().connect (sigc::bind (sigc::mem_fun (_color_dialog, &Gtk::Dialog::response), Gtk::RESPONSE_ACCEPT));
	_color_dialog.get_cancel_button()->signal_clicked().connect (sigc::bind (sigc::mem_fun (_color_dialog, &Gtk::Dialog::response), Gtk::RESPONSE_CANCEL));

	Gdk::Color c = ARDOUR_UI_UTILS::gdk_color_from_rgba(trigger()->color());

	_color_dialog.get_colorsel()->set_previous_color (c);
	_color_dialog.get_colorsel()->set_current_color (c);

	switch (_color_dialog.run()) {
		case Gtk::RESPONSE_ACCEPT: {
			c = _color_dialog.get_colorsel()->get_current_color();
			color_t ct = ARDOUR_UI_UTILS::gdk_color_to_rgba(c);
			trigger()->set_color(ct);
		} break;
		default:
			break;
	}

	_color_dialog.hide ();
}

void
TriggerUI::choose_sample ()
{
	if (!_file_chooser) {
		_file_chooser = new Gtk::FileChooserDialog (_("Select sample"), Gtk::FILE_CHOOSER_ACTION_OPEN);
		_file_chooser->add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
		_file_chooser->add_button (Gtk::Stock::OK, Gtk::RESPONSE_OK);

		/* for newbies, start in the bundled media folder */
		Searchpath spath (ardour_data_search_path ());
		spath.add_subdirectory_to_paths (media_dir_name);
		for (auto const& f : spath) {
			if (Glib::file_test (f, Glib::FILE_TEST_IS_DIR | Glib::FILE_TEST_EXISTS)) {
				_file_chooser->set_current_folder (f);
			}
		}

		/* TODO: add various shortcut paths to user's media folders

		_file_chooser->add_shortcut_folder_uri(Glib::build_filename (user_config_directory (), media_dir_name);

		Searchpath cpath (Config->get_sample_lib_path ());
		for (auto const& f : cpath) {
			_file_chooser->add_shortcut_folder_uri (f);
		}
		*/

#ifdef __APPLE__
		try {
			/* add_shortcut_folder throws an exception if the folder being added already has a shortcut */
			_file_chooser->add_shortcut_folder_uri("file:///Library/GarageBand/Apple Loops");
			_file_chooser->add_shortcut_folder_uri("file:///Library/Audio/Apple Loops");
			_file_chooser->add_shortcut_folder_uri("file:///Library/Application Support/GarageBand/Instrument Library/Sampler/Sampler Files");
		}
		catch (Glib::Error & e) {
			std::cerr << "sfdb.add_shortcut_folder() threw Glib::Error " << e.what() << std::endl;
		}
#endif

	}

	_file_chooser_connection.disconnect ();
	_file_chooser_connection = _file_chooser->signal_response ().connect (sigc::mem_fun (*this, &SlotPropertyTable::sample_chosen));

	_file_chooser->present ();
}

void
TriggerUI::sample_chosen (int response)
{
	_file_chooser->hide ();

	switch (response) {
		case Gtk::RESPONSE_OK:
			break;
		default:
			return;
	}

	std::list<std::string> paths = _file_chooser->get_filenames ();

	for (std::list<std::string>::iterator s = paths.begin (); s != paths.end (); ++s) {
		TriggerBox *box = (TriggerBox *) &tref.trigger()->box();
		box->set_from_path (trigger()->index(), *s);
	}
}

/* ****************************************************************************/

bool
TriggerUI::namebox_button_press (GdkEventButton* ev)
{
	if (_renaming) {
		return false;
	}
	if ((ev->button == 1 && ev->type == GDK_2BUTTON_PRESS) || Keyboard::is_edit_event (ev)) {
		start_rename ();
		return true;
	}
	return false;
}

bool
TriggerUI::start_rename ()
{
	if (_renaming) {
		return false;
	}
	assert (_entry_connections.empty ());

	GtkRequisition r (_name_label.size_request ());
	_nameentry.set_size_request (r.width, -1);
	_nameentry.set_text (trigger()->name ());
	_namebox.remove ();
	_namebox.add (_nameentry);
	_nameentry.show ();
	_nameentry.grab_focus ();
	_nameentry.add_modal_grab ();
	_renaming = true;

	_entry_connections.push_back (_nameentry.signal_changed().connect (sigc::mem_fun (*this, &SlotPropertyTable::entry_changed)));
	_entry_connections.push_back (_nameentry.signal_activate().connect (sigc::mem_fun (*this, &SlotPropertyTable::entry_activated)));
	_entry_connections.push_back (_nameentry.signal_key_press_event().connect (sigc::mem_fun (*this, &SlotPropertyTable::entry_key_press), false));
	_entry_connections.push_back (_nameentry.signal_key_release_event().connect (sigc::mem_fun (*this, &SlotPropertyTable::entry_key_release), false));
	_entry_connections.push_back (_nameentry.signal_button_press_event ().connect (sigc::mem_fun (*this, &SlotPropertyTable::entry_button_press), false));
	_entry_connections.push_back (_nameentry.signal_focus_in_event ().connect (sigc::mem_fun (*this, &SlotPropertyTable::entry_focus_in)));
	_entry_connections.push_back (_nameentry.signal_focus_out_event ().connect (sigc::mem_fun (*this, &SlotPropertyTable::entry_focus_out)));
	return true;
}

void
TriggerUI::end_rename (bool ignore_change)
{
	if (!_renaming) {
		return;
	}
	std::string result = _nameentry.get_text ();
	disconnect_entry_signals ();
	_nameentry.remove_modal_grab ();
	_namebox.remove ();
	_namebox.add (_name_label);
	_name_label.show ();
	_renaming = false;

	if (ignore_change) {
		return;
	}

	trigger()->set_name (result);
}

void
TriggerUI::entry_changed ()
{
}

void
TriggerUI::entry_activated ()
{
	end_rename (false);
}

bool
TriggerUI::entry_focus_in (GdkEventFocus*)
{
	return false;
}

bool
TriggerUI::entry_focus_out (GdkEventFocus*)
{
	end_rename (false);
	return false;
}

bool
TriggerUI::entry_button_press (GdkEventButton* ev)
{
	if (Keyboard::is_context_menu_event (ev)) {
		return false;
	} else if (Gtkmm2ext::event_inside_widget_window (_namebox, (GdkEvent*) ev)) {
		return false;
	} else {
		end_rename (false);
		return false;
	}
}

bool
TriggerUI::entry_key_press (GdkEventKey* ev)
{
	switch (ev->keyval) {
		case GDK_Escape:
			/* fallthrough */
		case GDK_ISO_Left_Tab:
			/* fallthrough */
		case GDK_Tab:
			/* fallthrough */
			return true;
		default:
			break;
	}
	return false;
}

bool
TriggerUI::entry_key_release (GdkEventKey* ev)
{
	switch (ev->keyval) {
		case GDK_Escape:
			end_rename (true);
			return true;
		case GDK_ISO_Left_Tab:
			end_rename (false);
//			EditNextName (this, false); /* TODO */
			return true;
		case GDK_Tab:
			end_rename (false);
//			EditNextName (this, true); /* TODO */
			return true;
		default:
			break;
	}
	return false;
}

void
TriggerUI::disconnect_entry_signals ()
{
	for (std::list<sigc::connection>::iterator i = _entry_connections.begin(); i != _entry_connections.end(); ++i) {
		i->disconnect ();
	}
	_entry_connections.clear ();
}

/* ****************************************************************************/



std::string
TriggerUI::launch_style_to_string (Trigger::LaunchStyle ls)
{
	switch (ls) {
	case Trigger::OneShot:
		return _("One Shot");
	case Trigger::Gate:
		return _("Gate");
	case Trigger::Toggle:
		return _("Toggle");
	case Trigger::Repeat:
		return _("Repeat");
	}
	/*NOTREACHED*/
	return std::string();
}

std::string
TriggerUI::quantize_length_to_string (BBT_Offset const & ql)
{
	if (ql < Temporal::BBT_Offset (0, 0, 0)) {
		/* negative quantization == do not quantize */
		return _("None");
	}

	if (ql == BBT_Offset (1, 0, 0)) {
		return _("1 Bar");
	} else if (ql == BBT_Offset (0, 1, 0)) {
		return _("1/4");
	} else if (ql == BBT_Offset (0, 2, 0)) {
		return _("1/2");
	} else if (ql == BBT_Offset (0, 4, 0)) {
		return _("Whole");
	} else if (ql == BBT_Offset (0, 0,Temporal::ticks_per_beat/2)) {
		return _("1/8");
	} else if (ql == BBT_Offset (0, 0,Temporal::ticks_per_beat/4)) {
		return _("1/16");
	} else if (ql == BBT_Offset (0, 0,Temporal::ticks_per_beat/8)) {
		return _("1/32");
	} else if (ql == BBT_Offset (0, 0,Temporal::ticks_per_beat/16)) {
		return _("1/64");
	} else {
		return "???";
	}
}

std::string
TriggerUI::follow_action_to_string (Trigger::FollowAction fa)
{
	switch (fa) {
	case Trigger::None:
		return _("None");
	case Trigger::Stop:
		return _("Stop");
	case Trigger::Again:
		return _("Again");
	case Trigger::QueuedTrigger:
		return _("Queued");
	case Trigger::NextTrigger:
		return _("Next");
	case Trigger::PrevTrigger:
		return _("Prev");
	case Trigger::FirstTrigger:
		return _("First");
	case Trigger::LastTrigger:
		return _("Last");
	case Trigger::AnyTrigger:
		return _("Any");
	case Trigger::OtherTrigger:
		return _("Other");
	}
	/*NOTREACHED*/
	return std::string();
}

TriggerPtr
TriggerUI::trigger() const
{
	return tref.trigger();
}

void
TriggerUI::trigger_changed (PropertyChange what)
{
	on_trigger_changed(what);
}


void
TriggerUI::set_trigger (ARDOUR::TriggerReference tr)
{
	tref = tr;

	PropertyChange pc;

	pc.add (Properties::name);
	pc.add (Properties::color);
	pc.add (Properties::use_follow);
	pc.add (Properties::legato);
	pc.add (Properties::quantization);
	pc.add (Properties::launch_style);
	pc.add (Properties::follow_count);
	pc.add (Properties::follow_action0);
	pc.add (Properties::follow_action1);
	pc.add (Properties::velocity_effect);
	pc.add (Properties::follow_action_probability);

	trigger_changed (pc);

	trigger()->PropertyChanged.connect (trigger_connections, MISSING_INVALIDATOR, boost::bind (&TriggerUI::trigger_changed, this, _1), gui_context());
}
