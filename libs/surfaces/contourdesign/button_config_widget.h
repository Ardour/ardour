/*
 * Copyright (C) 2019 Johannes Mueller <github@johannes-mueller.org>
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

#ifndef ardour_contourdesign_button_config_widget_h
#define ardour_contourdesign_button_config_widget_h

#include <gtkmm/box.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/combobox.h>
#include <gtkmm/treestore.h>

#include "contourdesign.h"
#include "jump_distance_widget.h"

namespace ActionManager
{
class ActionModel;
}

namespace ArdourSurface
{
class ButtonConfigWidget : public Gtk::HBox
{
public:
	ButtonConfigWidget ();
	~ButtonConfigWidget () {};

	void set_current_config (boost::shared_ptr<ButtonBase> btn_cnf);
	boost::shared_ptr<ButtonBase> get_current_config (ContourDesignControlProtocol& ccp) const;

	sigc::signal<void> Changed;

private:
	void set_current_action (std::string action_string);
	void set_jump_distance (JumpDistance dist);

	Gtk::RadioButton _choice_jump;
	Gtk::RadioButton _choice_action;

	void update_choice ();
	void update_config ();

	bool find_action_in_model (const Gtk::TreeModel::iterator& iter, std::string const& action_path, Gtk::TreeModel::iterator* found);

	JumpDistanceWidget _jump_distance;
	Gtk::ComboBox _action_cb;

	const ActionManager::ActionModel& _action_model;
};
}

#endif /* ardour_contourdesign_button_config_widget_h */
