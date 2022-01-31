/*
 * Copyright (C) 2022 Ben Loftis <ben@harrisonconsoles.com>
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

#include "trigger_ui.h"
#include "ardour_dialog.h"

#include "pbd/properties.h"

#include <gtkmm/table.h>

namespace ArdourWidgets {
	class ArdourButton;
}

class TriggerJumpDialog : public ArdourDialog, public TriggerUI
{
public:
	TriggerJumpDialog (bool right_fa);

	void done (int);

	void on_trigger_set ();
	void button_clicked (int b);
	void on_trigger_changed (PBD::PropertyChange const& what);

private:
	Gtk::Table _table;

	bool _right_fa;

	typedef std::list <ArdourWidgets::ArdourButton*> ButtonList;
	ButtonList _buttonlist;
};
