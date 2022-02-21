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

class TriggerJumpDialog;

class TriggerUI
{
public:
	TriggerUI ();
	virtual ~TriggerUI ();

	void set_trigger (ARDOUR::TriggerReference);

	virtual void on_trigger_set () {}
	virtual void on_trigger_changed (PBD::PropertyChange const& ) = 0;

	static std::string follow_action_to_string (ARDOUR::FollowAction const &, bool with_targets=false);
	static std::string quantize_length_to_string (Temporal::BBT_Offset const &);
	static std::string launch_style_to_string (ARDOUR::Trigger::LaunchStyle);
	static std::string stretch_mode_to_string (ARDOUR::Trigger::StretchMode);

	static std::vector<std::string> follow_strings;
	static std::string              longest_follow;
	static std::vector<std::string> quantize_strings;
	static std::string              longest_quantize;
	static std::vector<std::string> launch_strings;
	static std::string              longest_launch;
	static std::vector<std::string> stretch_mode_strings;
	static std::string              longest_stretch_mode;

	static void                     setup_actions_and_bindings ();

	ARDOUR::TriggerReference trigger_reference() const { return tref; }
	ARDOUR::TriggerPtr       trigger() const;
	ARDOUR::TriggerBox&		 triggerbox() const { return trigger()->box(); }

	void choose_color ();
	void choose_sample (bool allow_multiple_select);
	void sample_chosen (int r);

	void launch_context_menu ();
	void follow_context_menu ();
	void context_menu ();

	void edit_jump_done (int r, TriggerJumpDialog* d);
	void edit_jump(bool right_fa);

	void set_follow_action (ARDOUR::FollowAction const &);
	void set_launch_style (ARDOUR::Trigger::LaunchStyle);
	void set_quantization (Temporal::BBT_Offset const&);
	void set_from_selection ();

	void toggle_trigger_isolated ();
	void clear_trigger ();
	void edit_trigger ();

private:
	void trigger_changed (PBD::PropertyChange const& );  //calls on_trigger_changed to subclasses

	/* Actions for Triggers: accessed via ardour_ui and shortcuts and lua */
	static Glib::RefPtr<Gtk::ActionGroup> trigger_actions;
	static void trigger_cue (int32_t);
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

	Gtk::FileChooserDialog* _file_chooser;
	sigc::connection        _file_chooser_connection;

	Gtk::Menu*              _launch_context_menu;
	Gtk::Menu*              _follow_context_menu;
	Gtk::Menu*              _context_menu;
	bool                    _ignore_menu_action;

	Gtk::ColorSelectionDialog   _color_dialog;

	void                  trigger_swap (uint32_t);
	PBD::ScopedConnection trigger_swap_connection;

	ARDOUR::TriggerReference tref;
	PBD::ScopedConnectionList trigger_connections;
};



#endif /* __ardour_gtk_trigger_ui_h__ */
