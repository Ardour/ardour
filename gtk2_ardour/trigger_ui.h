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

#ifndef __ardour_gtk_trigger_ui_h__
#define __ardour_gtk_trigger_ui_h__

#include "gtkmm/colorselection.h"

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/bindings.h"

#include "ardour/triggerbox.h"
#include "widgets/ardour_button.h"
#include "widgets/slider_controller.h"
#include "widgets/frame.h"

namespace Gtk
{
	class FileChooserDialog;
	class Menu;
}

class TriggerUI
{
public:
	TriggerUI ();
	~TriggerUI ();

	void set_trigger (ARDOUR::TriggerReference);

	virtual void on_trigger_changed (PBD::PropertyChange const& ) = 0;

	static std::string follow_action_to_string (ARDOUR::Trigger::FollowAction);
	static ARDOUR::Trigger::FollowAction  string_to_follow_action (std::string const &);
	static std::string quantize_length_to_string (Temporal::BBT_Offset const &);
	static std::string launch_style_to_string (ARDOUR::Trigger::LaunchStyle);

	static std::vector<std::string> follow_strings;
	static std::string              longest_follow;
	static std::vector<std::string> quantize_strings;
	static std::string              longest_quantize;
	static std::vector<std::string> launch_strings;
	static std::string              longest_launch;

	static void                     setup_actions_and_bindings ();

	ARDOUR::TriggerReference trigger_reference() const { return tref; }
	ARDOUR::TriggerPtr       trigger() const;
	ARDOUR::TriggerBox&		 triggerbox() const { return trigger()->box(); }

	void choose_color ();
	void choose_sample ();
	void sample_chosen (int r);

private:
	void trigger_changed (PBD::PropertyChange const& );  //calls on_trigger_changed to subclasses

	/* Actions for Triggers: accessed via ardour_ui and shortcuts and lua */
	static Glib::RefPtr<Gtk::ActionGroup> trigger_actions;
	static void trigger_scene (int32_t);
	static Gtkmm2ext::Bindings* bindings;
	static void                 load_bindings ();
	static void                 register_actions ();

protected:
	/* all of this for name editing ...  */
	bool namebox_button_press (GdkEventButton*);
	bool start_rename ();
	void end_rename (bool);
	void entry_changed ();
	void entry_activated ();
	bool entry_focus_in (GdkEventFocus*);
	bool entry_focus_out (GdkEventFocus*);
	bool entry_key_press (GdkEventKey*);
	bool entry_key_release (GdkEventKey*);
	bool entry_button_press (GdkEventButton*);
	void disconnect_entry_signals ();
	std::list<sigc::connection> _entry_connections;
	bool                        _renaming;
	Gtk::Entry                  _nameentry;
	Gtk::Label                  _name_label;
	Gtk::EventBox               _namebox;
	ArdourWidgets::Frame        _name_frame;

	Gtk::ColorSelectionDialog   _color_dialog;

	sigc::connection            _file_chooser_connection;
	Gtk::FileChooserDialog*     _file_chooser;

	void                  trigger_swap (uint32_t);
	PBD::ScopedConnection trigger_swap_connection;

	ARDOUR::TriggerReference tref;
	PBD::ScopedConnection trigger_connections;
};



#endif /* __ardour_gtk_trigger_ui_h__ */
